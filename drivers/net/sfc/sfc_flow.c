/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2017-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <rte_byteorder.h>
#include <rte_tailq.h>
#include <rte_common.h>
#include <ethdev_driver.h>
#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_rx.h"
#include "sfc_filter.h"
#include "sfc_flow.h"
#include "sfc_flow_rss.h"
#include "sfc_flow_tunnel.h"
#include "sfc_log.h"
#include "sfc_dp_rx.h"
#include "sfc_mae_counter.h"
#include "sfc_switch.h"

struct sfc_flow_ops_by_spec {
	sfc_flow_parse_cb_t	*parse;
	sfc_flow_verify_cb_t	*verify;
	sfc_flow_cleanup_cb_t	*cleanup;
	sfc_flow_insert_cb_t	*insert;
	sfc_flow_remove_cb_t	*remove;
	sfc_flow_query_cb_t	*query;
};

static sfc_flow_parse_cb_t sfc_flow_parse_rte_to_filter;
static sfc_flow_parse_cb_t sfc_flow_parse_rte_to_mae;
static sfc_flow_insert_cb_t sfc_flow_filter_insert;
static sfc_flow_remove_cb_t sfc_flow_filter_remove;
static sfc_flow_cleanup_cb_t sfc_flow_cleanup;

static const struct sfc_flow_ops_by_spec sfc_flow_ops_filter = {
	.parse = sfc_flow_parse_rte_to_filter,
	.verify = NULL,
	.cleanup = sfc_flow_cleanup,
	.insert = sfc_flow_filter_insert,
	.remove = sfc_flow_filter_remove,
	.query = NULL,
};

static const struct sfc_flow_ops_by_spec sfc_flow_ops_mae = {
	.parse = sfc_flow_parse_rte_to_mae,
	.verify = sfc_mae_flow_verify,
	.cleanup = sfc_mae_flow_cleanup,
	.insert = sfc_mae_flow_insert,
	.remove = sfc_mae_flow_remove,
	.query = sfc_mae_flow_query,
};

static const struct sfc_flow_ops_by_spec *
sfc_flow_get_ops_by_spec(struct rte_flow *flow)
{
	struct sfc_flow_spec *spec = &flow->spec;
	const struct sfc_flow_ops_by_spec *ops = NULL;

	switch (spec->type) {
	case SFC_FLOW_SPEC_FILTER:
		ops = &sfc_flow_ops_filter;
		break;
	case SFC_FLOW_SPEC_MAE:
		ops = &sfc_flow_ops_mae;
		break;
	default:
		SFC_ASSERT(false);
		break;
	}

	return ops;
}

/*
 * Currently, filter-based (VNIC) flow API is implemented in such a manner
 * that each flow rule is converted to one or more hardware filters.
 * All elements of flow rule (attributes, pattern items, actions)
 * correspond to one or more fields in the efx_filter_spec_s structure
 * that is responsible for the hardware filter.
 * If some required field is unset in the flow rule, then a handful
 * of filter copies will be created to cover all possible values
 * of such a field.
 */

static sfc_flow_item_parse sfc_flow_parse_void;
static sfc_flow_item_parse sfc_flow_parse_eth;
static sfc_flow_item_parse sfc_flow_parse_vlan;
static sfc_flow_item_parse sfc_flow_parse_ipv4;
static sfc_flow_item_parse sfc_flow_parse_ipv6;
static sfc_flow_item_parse sfc_flow_parse_tcp;
static sfc_flow_item_parse sfc_flow_parse_udp;
static sfc_flow_item_parse sfc_flow_parse_vxlan;
static sfc_flow_item_parse sfc_flow_parse_geneve;
static sfc_flow_item_parse sfc_flow_parse_nvgre;
static sfc_flow_item_parse sfc_flow_parse_pppoex;

typedef int (sfc_flow_spec_set_vals)(struct sfc_flow_spec *spec,
				     unsigned int filters_count_for_one_val,
				     struct rte_flow_error *error);

typedef boolean_t (sfc_flow_spec_check)(efx_filter_match_flags_t match,
					efx_filter_spec_t *spec,
					struct sfc_filter *filter);

struct sfc_flow_copy_flag {
	/* EFX filter specification match flag */
	efx_filter_match_flags_t flag;
	/* Number of values of corresponding field */
	unsigned int vals_count;
	/* Function to set values in specifications */
	sfc_flow_spec_set_vals *set_vals;
	/*
	 * Function to check that the specification is suitable
	 * for adding this match flag
	 */
	sfc_flow_spec_check *spec_check;
};

static sfc_flow_spec_set_vals sfc_flow_set_unknown_dst_flags;
static sfc_flow_spec_check sfc_flow_check_unknown_dst_flags;
static sfc_flow_spec_set_vals sfc_flow_set_ethertypes;
static sfc_flow_spec_set_vals sfc_flow_set_ifrm_unknown_dst_flags;
static sfc_flow_spec_check sfc_flow_check_ifrm_unknown_dst_flags;
static sfc_flow_spec_set_vals sfc_flow_set_outer_vid_flag;
static sfc_flow_spec_check sfc_flow_check_outer_vid_flag;

static boolean_t
sfc_flow_is_zero(const uint8_t *buf, unsigned int size)
{
	uint8_t sum = 0;
	unsigned int i;

	for (i = 0; i < size; i++)
		sum |= buf[i];

	return (sum == 0) ? B_TRUE : B_FALSE;
}

/*
 * Validate item and prepare structures spec and mask for parsing
 */
int
sfc_flow_parse_init(const struct rte_flow_item *item,
		    const void **spec_ptr,
		    const void **mask_ptr,
		    const void *supp_mask,
		    const void *def_mask,
		    unsigned int size,
		    struct rte_flow_error *error)
{
	const uint8_t *spec;
	const uint8_t *mask;
	const uint8_t *last;
	uint8_t supp;
	unsigned int i;

	if (item == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				   "NULL item");
		return -rte_errno;
	}

	if ((item->last != NULL || item->mask != NULL) && item->spec == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "Mask or last is set without spec");
		return -rte_errno;
	}

	/*
	 * If "mask" is not set, default mask is used,
	 * but if default mask is NULL, "mask" should be set
	 */
	if (item->mask == NULL) {
		if (def_mask == NULL) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				"Mask should be specified");
			return -rte_errno;
		}

		mask = def_mask;
	} else {
		mask = item->mask;
	}

	spec = item->spec;
	last = item->last;

	if (spec == NULL)
		goto exit;

	/*
	 * If field values in "last" are either 0 or equal to the corresponding
	 * values in "spec" then they are ignored
	 */
	if (last != NULL &&
	    !sfc_flow_is_zero(last, size) &&
	    memcmp(last, spec, size) != 0) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "Ranging is not supported");
		return -rte_errno;
	}

	if (supp_mask == NULL) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Supported mask for item should be specified");
		return -rte_errno;
	}

	/* Check that mask does not ask for more match than supp_mask */
	for (i = 0; i < size; i++) {
		supp = ((const uint8_t *)supp_mask)[i];

		if (~supp & mask[i]) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ITEM, item,
					   "Item's field is not supported");
			return -rte_errno;
		}
	}

exit:
	*spec_ptr = spec;
	*mask_ptr = mask;
	return 0;
}

/*
 * Protocol parsers.
 * Masking is not supported, so masks in items should be either
 * full or empty (zeroed) and set only for supported fields which
 * are specified in the supp_mask.
 */

static int
sfc_flow_parse_void(__rte_unused const struct rte_flow_item *item,
		    __rte_unused struct sfc_flow_parse_ctx *parse_ctx,
		    __rte_unused struct rte_flow_error *error)
{
	return 0;
}

/**
 * Convert Ethernet item to EFX filter specification.
 *
 * @param item[in]
 *   Item specification. Outer frame specification may only comprise
 *   source/destination addresses and Ethertype field.
 *   Inner frame specification may contain destination address only.
 *   There is support for individual/group mask as well as for empty and full.
 *   If the mask is NULL, default mask will be used. Ranging is not supported.
 * @param efx_spec[in, out]
 *   EFX filter specification to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_parse_eth(const struct rte_flow_item *item,
		   struct sfc_flow_parse_ctx *parse_ctx,
		   struct rte_flow_error *error)
{
	int rc;
	efx_filter_spec_t *efx_spec = parse_ctx->filter;
	const struct rte_flow_item_eth *spec = NULL;
	const struct rte_flow_item_eth *mask = NULL;
	const struct rte_flow_item_eth supp_mask = {
		.hdr.dst_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
		.hdr.src_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
		.hdr.ether_type = 0xffff,
	};
	const struct rte_flow_item_eth ifrm_supp_mask = {
		.hdr.dst_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	};
	const uint8_t ig_mask[EFX_MAC_ADDR_LEN] = {
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	const struct rte_flow_item_eth *supp_mask_p;
	const struct rte_flow_item_eth *def_mask_p;
	uint8_t *loc_mac = NULL;
	boolean_t is_ifrm = (efx_spec->efs_encap_type !=
		EFX_TUNNEL_PROTOCOL_NONE);

	if (is_ifrm) {
		supp_mask_p = &ifrm_supp_mask;
		def_mask_p = &ifrm_supp_mask;
		loc_mac = efx_spec->efs_ifrm_loc_mac;
	} else {
		supp_mask_p = &supp_mask;
		def_mask_p = &rte_flow_item_eth_mask;
		loc_mac = efx_spec->efs_loc_mac;
	}

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec,
				 (const void **)&mask,
				 supp_mask_p, def_mask_p,
				 sizeof(struct rte_flow_item_eth),
				 error);
	if (rc != 0)
		return rc;

	/* If "spec" is not set, could be any Ethernet */
	if (spec == NULL)
		return 0;

	if (rte_is_same_ether_addr(&mask->hdr.dst_addr, &supp_mask.hdr.dst_addr)) {
		efx_spec->efs_match_flags |= is_ifrm ?
			EFX_FILTER_MATCH_IFRM_LOC_MAC :
			EFX_FILTER_MATCH_LOC_MAC;
		rte_memcpy(loc_mac, spec->hdr.dst_addr.addr_bytes,
			   EFX_MAC_ADDR_LEN);
	} else if (memcmp(mask->hdr.dst_addr.addr_bytes, ig_mask,
			  EFX_MAC_ADDR_LEN) == 0) {
		if (rte_is_unicast_ether_addr(&spec->hdr.dst_addr))
			efx_spec->efs_match_flags |= is_ifrm ?
				EFX_FILTER_MATCH_IFRM_UNKNOWN_UCAST_DST :
				EFX_FILTER_MATCH_UNKNOWN_UCAST_DST;
		else
			efx_spec->efs_match_flags |= is_ifrm ?
				EFX_FILTER_MATCH_IFRM_UNKNOWN_MCAST_DST :
				EFX_FILTER_MATCH_UNKNOWN_MCAST_DST;
	} else if (!rte_is_zero_ether_addr(&mask->hdr.dst_addr)) {
		goto fail_bad_mask;
	}

	/*
	 * ifrm_supp_mask ensures that the source address and
	 * ethertype masks are equal to zero in inner frame,
	 * so these fields are filled in only for the outer frame
	 */
	if (rte_is_same_ether_addr(&mask->hdr.src_addr, &supp_mask.hdr.src_addr)) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_REM_MAC;
		rte_memcpy(efx_spec->efs_rem_mac, spec->hdr.src_addr.addr_bytes,
			   EFX_MAC_ADDR_LEN);
	} else if (!rte_is_zero_ether_addr(&mask->hdr.src_addr)) {
		goto fail_bad_mask;
	}

	/*
	 * Ether type is in big-endian byte order in item and
	 * in little-endian in efx_spec, so byte swap is used
	 */
	if (mask->hdr.ether_type == supp_mask.hdr.ether_type) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_ETHER_TYPE;
		efx_spec->efs_ether_type = rte_bswap16(spec->hdr.ether_type);
	} else if (mask->hdr.ether_type != 0) {
		goto fail_bad_mask;
	}

	return 0;

fail_bad_mask:
	rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ITEM, item,
			   "Bad mask in the ETH pattern item");
	return -rte_errno;
}

/**
 * Convert VLAN item to EFX filter specification.
 *
 * @param item[in]
 *   Item specification. Only VID field is supported.
 *   The mask can not be NULL. Ranging is not supported.
 * @param efx_spec[in, out]
 *   EFX filter specification to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_parse_vlan(const struct rte_flow_item *item,
		    struct sfc_flow_parse_ctx *parse_ctx,
		    struct rte_flow_error *error)
{
	int rc;
	uint16_t vid;
	efx_filter_spec_t *efx_spec = parse_ctx->filter;
	const struct rte_flow_item_vlan *spec = NULL;
	const struct rte_flow_item_vlan *mask = NULL;
	const struct rte_flow_item_vlan supp_mask = {
		.hdr.vlan_tci = rte_cpu_to_be_16(RTE_ETH_VLAN_ID_MAX),
		.hdr.eth_proto = RTE_BE16(0xffff),
	};

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec,
				 (const void **)&mask,
				 &supp_mask,
				 NULL,
				 sizeof(struct rte_flow_item_vlan),
				 error);
	if (rc != 0)
		return rc;

	/*
	 * VID is in big-endian byte order in item and
	 * in little-endian in efx_spec, so byte swap is used.
	 * If two VLAN items are included, the first matches
	 * the outer tag and the next matches the inner tag.
	 */
	if (mask->hdr.vlan_tci == supp_mask.hdr.vlan_tci) {
		/* Apply mask to keep VID only */
		vid = rte_bswap16(spec->hdr.vlan_tci & mask->hdr.vlan_tci);

		if (!(efx_spec->efs_match_flags &
		      EFX_FILTER_MATCH_OUTER_VID)) {
			efx_spec->efs_match_flags |= EFX_FILTER_MATCH_OUTER_VID;
			efx_spec->efs_outer_vid = vid;
		} else if (!(efx_spec->efs_match_flags &
			     EFX_FILTER_MATCH_INNER_VID)) {
			efx_spec->efs_match_flags |= EFX_FILTER_MATCH_INNER_VID;
			efx_spec->efs_inner_vid = vid;
		} else {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, item,
					   "More than two VLAN items");
			return -rte_errno;
		}
	} else {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "VLAN ID in TCI match is required");
		return -rte_errno;
	}

	if (efx_spec->efs_match_flags & EFX_FILTER_MATCH_ETHER_TYPE) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "VLAN TPID matching is not supported");
		return -rte_errno;
	}
	if (mask->hdr.eth_proto == supp_mask.hdr.eth_proto) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_ETHER_TYPE;
		efx_spec->efs_ether_type = rte_bswap16(spec->hdr.eth_proto);
	} else if (mask->hdr.eth_proto) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "Bad mask for VLAN inner type");
		return -rte_errno;
	}

	return 0;
}

/**
 * Convert IPv4 item to EFX filter specification.
 *
 * @param item[in]
 *   Item specification. Only source and destination addresses and
 *   protocol fields are supported. If the mask is NULL, default
 *   mask will be used. Ranging is not supported.
 * @param efx_spec[in, out]
 *   EFX filter specification to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_parse_ipv4(const struct rte_flow_item *item,
		    struct sfc_flow_parse_ctx *parse_ctx,
		    struct rte_flow_error *error)
{
	int rc;
	efx_filter_spec_t *efx_spec = parse_ctx->filter;
	const struct rte_flow_item_ipv4 *spec = NULL;
	const struct rte_flow_item_ipv4 *mask = NULL;
	const uint16_t ether_type_ipv4 = rte_cpu_to_le_16(EFX_ETHER_TYPE_IPV4);
	const struct rte_flow_item_ipv4 supp_mask = {
		.hdr = {
			.src_addr = 0xffffffff,
			.dst_addr = 0xffffffff,
			.next_proto_id = 0xff,
		}
	};

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec,
				 (const void **)&mask,
				 &supp_mask,
				 &rte_flow_item_ipv4_mask,
				 sizeof(struct rte_flow_item_ipv4),
				 error);
	if (rc != 0)
		return rc;

	/*
	 * Filtering by IPv4 source and destination addresses requires
	 * the appropriate ETHER_TYPE in hardware filters
	 */
	if (!(efx_spec->efs_match_flags & EFX_FILTER_MATCH_ETHER_TYPE)) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_ETHER_TYPE;
		efx_spec->efs_ether_type = ether_type_ipv4;
	} else if (efx_spec->efs_ether_type != ether_type_ipv4) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Ethertype in pattern with IPV4 item should be appropriate");
		return -rte_errno;
	}

	if (spec == NULL)
		return 0;

	/*
	 * IPv4 addresses are in big-endian byte order in item and in
	 * efx_spec
	 */
	if (mask->hdr.src_addr == supp_mask.hdr.src_addr) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_REM_HOST;
		efx_spec->efs_rem_host.eo_u32[0] = spec->hdr.src_addr;
	} else if (mask->hdr.src_addr != 0) {
		goto fail_bad_mask;
	}

	if (mask->hdr.dst_addr == supp_mask.hdr.dst_addr) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_LOC_HOST;
		efx_spec->efs_loc_host.eo_u32[0] = spec->hdr.dst_addr;
	} else if (mask->hdr.dst_addr != 0) {
		goto fail_bad_mask;
	}

	if (mask->hdr.next_proto_id == supp_mask.hdr.next_proto_id) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_IP_PROTO;
		efx_spec->efs_ip_proto = spec->hdr.next_proto_id;
	} else if (mask->hdr.next_proto_id != 0) {
		goto fail_bad_mask;
	}

	return 0;

fail_bad_mask:
	rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ITEM, item,
			   "Bad mask in the IPV4 pattern item");
	return -rte_errno;
}

/**
 * Convert IPv6 item to EFX filter specification.
 *
 * @param item[in]
 *   Item specification. Only source and destination addresses and
 *   next header fields are supported. If the mask is NULL, default
 *   mask will be used. Ranging is not supported.
 * @param efx_spec[in, out]
 *   EFX filter specification to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_parse_ipv6(const struct rte_flow_item *item,
		    struct sfc_flow_parse_ctx *parse_ctx,
		    struct rte_flow_error *error)
{
	int rc;
	efx_filter_spec_t *efx_spec = parse_ctx->filter;
	const struct rte_flow_item_ipv6 *spec = NULL;
	const struct rte_flow_item_ipv6 *mask = NULL;
	const uint16_t ether_type_ipv6 = rte_cpu_to_le_16(EFX_ETHER_TYPE_IPV6);
	const struct rte_flow_item_ipv6 supp_mask = {
		.hdr = {
			.src_addr = { 0xff, 0xff, 0xff, 0xff,
				      0xff, 0xff, 0xff, 0xff,
				      0xff, 0xff, 0xff, 0xff,
				      0xff, 0xff, 0xff, 0xff },
			.dst_addr = { 0xff, 0xff, 0xff, 0xff,
				      0xff, 0xff, 0xff, 0xff,
				      0xff, 0xff, 0xff, 0xff,
				      0xff, 0xff, 0xff, 0xff },
			.proto = 0xff,
		}
	};

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec,
				 (const void **)&mask,
				 &supp_mask,
				 &rte_flow_item_ipv6_mask,
				 sizeof(struct rte_flow_item_ipv6),
				 error);
	if (rc != 0)
		return rc;

	/*
	 * Filtering by IPv6 source and destination addresses requires
	 * the appropriate ETHER_TYPE in hardware filters
	 */
	if (!(efx_spec->efs_match_flags & EFX_FILTER_MATCH_ETHER_TYPE)) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_ETHER_TYPE;
		efx_spec->efs_ether_type = ether_type_ipv6;
	} else if (efx_spec->efs_ether_type != ether_type_ipv6) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Ethertype in pattern with IPV6 item should be appropriate");
		return -rte_errno;
	}

	if (spec == NULL)
		return 0;

	/*
	 * IPv6 addresses are in big-endian byte order in item and in
	 * efx_spec
	 */
	if (memcmp(mask->hdr.src_addr, supp_mask.hdr.src_addr,
		   sizeof(mask->hdr.src_addr)) == 0) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_REM_HOST;

		RTE_BUILD_BUG_ON(sizeof(efx_spec->efs_rem_host) !=
				 sizeof(spec->hdr.src_addr));
		rte_memcpy(&efx_spec->efs_rem_host, spec->hdr.src_addr,
			   sizeof(efx_spec->efs_rem_host));
	} else if (!sfc_flow_is_zero(mask->hdr.src_addr,
				     sizeof(mask->hdr.src_addr))) {
		goto fail_bad_mask;
	}

	if (memcmp(mask->hdr.dst_addr, supp_mask.hdr.dst_addr,
		   sizeof(mask->hdr.dst_addr)) == 0) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_LOC_HOST;

		RTE_BUILD_BUG_ON(sizeof(efx_spec->efs_loc_host) !=
				 sizeof(spec->hdr.dst_addr));
		rte_memcpy(&efx_spec->efs_loc_host, spec->hdr.dst_addr,
			   sizeof(efx_spec->efs_loc_host));
	} else if (!sfc_flow_is_zero(mask->hdr.dst_addr,
				     sizeof(mask->hdr.dst_addr))) {
		goto fail_bad_mask;
	}

	if (mask->hdr.proto == supp_mask.hdr.proto) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_IP_PROTO;
		efx_spec->efs_ip_proto = spec->hdr.proto;
	} else if (mask->hdr.proto != 0) {
		goto fail_bad_mask;
	}

	return 0;

fail_bad_mask:
	rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ITEM, item,
			   "Bad mask in the IPV6 pattern item");
	return -rte_errno;
}

/**
 * Convert TCP item to EFX filter specification.
 *
 * @param item[in]
 *   Item specification. Only source and destination ports fields
 *   are supported. If the mask is NULL, default mask will be used.
 *   Ranging is not supported.
 * @param efx_spec[in, out]
 *   EFX filter specification to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_parse_tcp(const struct rte_flow_item *item,
		   struct sfc_flow_parse_ctx *parse_ctx,
		   struct rte_flow_error *error)
{
	int rc;
	efx_filter_spec_t *efx_spec = parse_ctx->filter;
	const struct rte_flow_item_tcp *spec = NULL;
	const struct rte_flow_item_tcp *mask = NULL;
	const struct rte_flow_item_tcp supp_mask = {
		.hdr = {
			.src_port = 0xffff,
			.dst_port = 0xffff,
		}
	};

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec,
				 (const void **)&mask,
				 &supp_mask,
				 &rte_flow_item_tcp_mask,
				 sizeof(struct rte_flow_item_tcp),
				 error);
	if (rc != 0)
		return rc;

	/*
	 * Filtering by TCP source and destination ports requires
	 * the appropriate IP_PROTO in hardware filters
	 */
	if (!(efx_spec->efs_match_flags & EFX_FILTER_MATCH_IP_PROTO)) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_IP_PROTO;
		efx_spec->efs_ip_proto = EFX_IPPROTO_TCP;
	} else if (efx_spec->efs_ip_proto != EFX_IPPROTO_TCP) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, item,
			"IP proto in pattern with TCP item should be appropriate");
		return -rte_errno;
	}

	if (spec == NULL)
		return 0;

	/*
	 * Source and destination ports are in big-endian byte order in item and
	 * in little-endian in efx_spec, so byte swap is used
	 */
	if (mask->hdr.src_port == supp_mask.hdr.src_port) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_REM_PORT;
		efx_spec->efs_rem_port = rte_bswap16(spec->hdr.src_port);
	} else if (mask->hdr.src_port != 0) {
		goto fail_bad_mask;
	}

	if (mask->hdr.dst_port == supp_mask.hdr.dst_port) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_LOC_PORT;
		efx_spec->efs_loc_port = rte_bswap16(spec->hdr.dst_port);
	} else if (mask->hdr.dst_port != 0) {
		goto fail_bad_mask;
	}

	return 0;

fail_bad_mask:
	rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ITEM, item,
			   "Bad mask in the TCP pattern item");
	return -rte_errno;
}

/**
 * Convert UDP item to EFX filter specification.
 *
 * @param item[in]
 *   Item specification. Only source and destination ports fields
 *   are supported. If the mask is NULL, default mask will be used.
 *   Ranging is not supported.
 * @param efx_spec[in, out]
 *   EFX filter specification to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_parse_udp(const struct rte_flow_item *item,
		   struct sfc_flow_parse_ctx *parse_ctx,
		   struct rte_flow_error *error)
{
	int rc;
	efx_filter_spec_t *efx_spec = parse_ctx->filter;
	const struct rte_flow_item_udp *spec = NULL;
	const struct rte_flow_item_udp *mask = NULL;
	const struct rte_flow_item_udp supp_mask = {
		.hdr = {
			.src_port = 0xffff,
			.dst_port = 0xffff,
		}
	};

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec,
				 (const void **)&mask,
				 &supp_mask,
				 &rte_flow_item_udp_mask,
				 sizeof(struct rte_flow_item_udp),
				 error);
	if (rc != 0)
		return rc;

	/*
	 * Filtering by UDP source and destination ports requires
	 * the appropriate IP_PROTO in hardware filters
	 */
	if (!(efx_spec->efs_match_flags & EFX_FILTER_MATCH_IP_PROTO)) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_IP_PROTO;
		efx_spec->efs_ip_proto = EFX_IPPROTO_UDP;
	} else if (efx_spec->efs_ip_proto != EFX_IPPROTO_UDP) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, item,
			"IP proto in pattern with UDP item should be appropriate");
		return -rte_errno;
	}

	if (spec == NULL)
		return 0;

	/*
	 * Source and destination ports are in big-endian byte order in item and
	 * in little-endian in efx_spec, so byte swap is used
	 */
	if (mask->hdr.src_port == supp_mask.hdr.src_port) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_REM_PORT;
		efx_spec->efs_rem_port = rte_bswap16(spec->hdr.src_port);
	} else if (mask->hdr.src_port != 0) {
		goto fail_bad_mask;
	}

	if (mask->hdr.dst_port == supp_mask.hdr.dst_port) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_LOC_PORT;
		efx_spec->efs_loc_port = rte_bswap16(spec->hdr.dst_port);
	} else if (mask->hdr.dst_port != 0) {
		goto fail_bad_mask;
	}

	return 0;

fail_bad_mask:
	rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ITEM, item,
			   "Bad mask in the UDP pattern item");
	return -rte_errno;
}

/*
 * Filters for encapsulated packets match based on the EtherType and IP
 * protocol in the outer frame.
 */
static int
sfc_flow_set_match_flags_for_encap_pkts(const struct rte_flow_item *item,
					efx_filter_spec_t *efx_spec,
					uint8_t ip_proto,
					struct rte_flow_error *error)
{
	if (!(efx_spec->efs_match_flags & EFX_FILTER_MATCH_IP_PROTO)) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_IP_PROTO;
		efx_spec->efs_ip_proto = ip_proto;
	} else if (efx_spec->efs_ip_proto != ip_proto) {
		switch (ip_proto) {
		case EFX_IPPROTO_UDP:
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Outer IP header protocol must be UDP "
				"in VxLAN/GENEVE pattern");
			return -rte_errno;

		case EFX_IPPROTO_GRE:
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Outer IP header protocol must be GRE "
				"in NVGRE pattern");
			return -rte_errno;

		default:
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Only VxLAN/GENEVE/NVGRE tunneling patterns "
				"are supported");
			return -rte_errno;
		}
	}

	if (efx_spec->efs_match_flags & EFX_FILTER_MATCH_ETHER_TYPE &&
	    efx_spec->efs_ether_type != EFX_ETHER_TYPE_IPV4 &&
	    efx_spec->efs_ether_type != EFX_ETHER_TYPE_IPV6) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Outer frame EtherType in pattern with tunneling "
			"must be IPv4 or IPv6");
		return -rte_errno;
	}

	return 0;
}

static int
sfc_flow_set_efx_spec_vni_or_vsid(efx_filter_spec_t *efx_spec,
				  const uint8_t *vni_or_vsid_val,
				  const uint8_t *vni_or_vsid_mask,
				  const struct rte_flow_item *item,
				  struct rte_flow_error *error)
{
	const uint8_t vni_or_vsid_full_mask[EFX_VNI_OR_VSID_LEN] = {
		0xff, 0xff, 0xff
	};

	if (memcmp(vni_or_vsid_mask, vni_or_vsid_full_mask,
		   EFX_VNI_OR_VSID_LEN) == 0) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_VNI_OR_VSID;
		rte_memcpy(efx_spec->efs_vni_or_vsid, vni_or_vsid_val,
			   EFX_VNI_OR_VSID_LEN);
	} else if (!sfc_flow_is_zero(vni_or_vsid_mask, EFX_VNI_OR_VSID_LEN)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "Unsupported VNI/VSID mask");
		return -rte_errno;
	}

	return 0;
}

/**
 * Convert VXLAN item to EFX filter specification.
 *
 * @param item[in]
 *   Item specification. Only VXLAN network identifier field is supported.
 *   If the mask is NULL, default mask will be used.
 *   Ranging is not supported.
 * @param efx_spec[in, out]
 *   EFX filter specification to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_parse_vxlan(const struct rte_flow_item *item,
		     struct sfc_flow_parse_ctx *parse_ctx,
		     struct rte_flow_error *error)
{
	int rc;
	efx_filter_spec_t *efx_spec = parse_ctx->filter;
	const struct rte_flow_item_vxlan *spec = NULL;
	const struct rte_flow_item_vxlan *mask = NULL;
	const struct rte_flow_item_vxlan supp_mask = {
		.hdr.vni = { 0xff, 0xff, 0xff }
	};

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec,
				 (const void **)&mask,
				 &supp_mask,
				 &rte_flow_item_vxlan_mask,
				 sizeof(struct rte_flow_item_vxlan),
				 error);
	if (rc != 0)
		return rc;

	rc = sfc_flow_set_match_flags_for_encap_pkts(item, efx_spec,
						     EFX_IPPROTO_UDP, error);
	if (rc != 0)
		return rc;

	efx_spec->efs_encap_type = EFX_TUNNEL_PROTOCOL_VXLAN;
	efx_spec->efs_match_flags |= EFX_FILTER_MATCH_ENCAP_TYPE;

	if (spec == NULL)
		return 0;

	rc = sfc_flow_set_efx_spec_vni_or_vsid(efx_spec, spec->hdr.vni,
					       mask->hdr.vni, item, error);

	return rc;
}

/**
 * Convert GENEVE item to EFX filter specification.
 *
 * @param item[in]
 *   Item specification. Only Virtual Network Identifier and protocol type
 *   fields are supported. But protocol type can be only Ethernet (0x6558).
 *   If the mask is NULL, default mask will be used.
 *   Ranging is not supported.
 * @param efx_spec[in, out]
 *   EFX filter specification to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_parse_geneve(const struct rte_flow_item *item,
		      struct sfc_flow_parse_ctx *parse_ctx,
		      struct rte_flow_error *error)
{
	int rc;
	efx_filter_spec_t *efx_spec = parse_ctx->filter;
	const struct rte_flow_item_geneve *spec = NULL;
	const struct rte_flow_item_geneve *mask = NULL;
	const struct rte_flow_item_geneve supp_mask = {
		.protocol = RTE_BE16(0xffff),
		.vni = { 0xff, 0xff, 0xff }
	};

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec,
				 (const void **)&mask,
				 &supp_mask,
				 &rte_flow_item_geneve_mask,
				 sizeof(struct rte_flow_item_geneve),
				 error);
	if (rc != 0)
		return rc;

	rc = sfc_flow_set_match_flags_for_encap_pkts(item, efx_spec,
						     EFX_IPPROTO_UDP, error);
	if (rc != 0)
		return rc;

	efx_spec->efs_encap_type = EFX_TUNNEL_PROTOCOL_GENEVE;
	efx_spec->efs_match_flags |= EFX_FILTER_MATCH_ENCAP_TYPE;

	if (spec == NULL)
		return 0;

	if (mask->protocol == supp_mask.protocol) {
		if (spec->protocol != rte_cpu_to_be_16(RTE_ETHER_TYPE_TEB)) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"GENEVE encap. protocol must be Ethernet "
				"(0x6558) in the GENEVE pattern item");
			return -rte_errno;
		}
	} else if (mask->protocol != 0) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Unsupported mask for GENEVE encap. protocol");
		return -rte_errno;
	}

	rc = sfc_flow_set_efx_spec_vni_or_vsid(efx_spec, spec->vni,
					       mask->vni, item, error);

	return rc;
}

/**
 * Convert NVGRE item to EFX filter specification.
 *
 * @param item[in]
 *   Item specification. Only virtual subnet ID field is supported.
 *   If the mask is NULL, default mask will be used.
 *   Ranging is not supported.
 * @param efx_spec[in, out]
 *   EFX filter specification to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_parse_nvgre(const struct rte_flow_item *item,
		     struct sfc_flow_parse_ctx *parse_ctx,
		     struct rte_flow_error *error)
{
	int rc;
	efx_filter_spec_t *efx_spec = parse_ctx->filter;
	const struct rte_flow_item_nvgre *spec = NULL;
	const struct rte_flow_item_nvgre *mask = NULL;
	const struct rte_flow_item_nvgre supp_mask = {
		.tni = { 0xff, 0xff, 0xff }
	};

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec,
				 (const void **)&mask,
				 &supp_mask,
				 &rte_flow_item_nvgre_mask,
				 sizeof(struct rte_flow_item_nvgre),
				 error);
	if (rc != 0)
		return rc;

	rc = sfc_flow_set_match_flags_for_encap_pkts(item, efx_spec,
						     EFX_IPPROTO_GRE, error);
	if (rc != 0)
		return rc;

	efx_spec->efs_encap_type = EFX_TUNNEL_PROTOCOL_NVGRE;
	efx_spec->efs_match_flags |= EFX_FILTER_MATCH_ENCAP_TYPE;

	if (spec == NULL)
		return 0;

	rc = sfc_flow_set_efx_spec_vni_or_vsid(efx_spec, spec->tni,
					       mask->tni, item, error);

	return rc;
}

/**
 * Convert PPPoEx item to EFX filter specification.
 *
 * @param item[in]
 *   Item specification.
 *   Matching on PPPoEx fields is not supported.
 *   This item can only be used to set or validate the EtherType filter.
 *   Only zero masks are allowed.
 *   Ranging is not supported.
 * @param efx_spec[in, out]
 *   EFX filter specification to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_parse_pppoex(const struct rte_flow_item *item,
		      struct sfc_flow_parse_ctx *parse_ctx,
		      struct rte_flow_error *error)
{
	efx_filter_spec_t *efx_spec = parse_ctx->filter;
	const struct rte_flow_item_pppoe *spec = NULL;
	const struct rte_flow_item_pppoe *mask = NULL;
	const struct rte_flow_item_pppoe supp_mask = {};
	const struct rte_flow_item_pppoe def_mask = {};
	uint16_t ether_type;
	int rc;

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec,
				 (const void **)&mask,
				 &supp_mask,
				 &def_mask,
				 sizeof(struct rte_flow_item_pppoe),
				 error);
	if (rc != 0)
		return rc;

	if (item->type == RTE_FLOW_ITEM_TYPE_PPPOED)
		ether_type = RTE_ETHER_TYPE_PPPOE_DISCOVERY;
	else
		ether_type = RTE_ETHER_TYPE_PPPOE_SESSION;

	if ((efx_spec->efs_match_flags & EFX_FILTER_MATCH_ETHER_TYPE) != 0) {
		if (efx_spec->efs_ether_type != ether_type) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, item,
					   "Invalid EtherType for a PPPoE flow item");
			return -rte_errno;
		}
	} else {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_ETHER_TYPE;
		efx_spec->efs_ether_type = ether_type;
	}

	return 0;
}

static const struct sfc_flow_item sfc_flow_items[] = {
	{
		.type = RTE_FLOW_ITEM_TYPE_VOID,
		.name = "VOID",
		.prev_layer = SFC_FLOW_ITEM_ANY_LAYER,
		.layer = SFC_FLOW_ITEM_ANY_LAYER,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_void,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.name = "ETH",
		.prev_layer = SFC_FLOW_ITEM_START_LAYER,
		.layer = SFC_FLOW_ITEM_L2,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_eth,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_VLAN,
		.name = "VLAN",
		.prev_layer = SFC_FLOW_ITEM_L2,
		.layer = SFC_FLOW_ITEM_L2,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_vlan,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_PPPOED,
		.name = "PPPOED",
		.prev_layer = SFC_FLOW_ITEM_L2,
		.layer = SFC_FLOW_ITEM_L2,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_pppoex,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_PPPOES,
		.name = "PPPOES",
		.prev_layer = SFC_FLOW_ITEM_L2,
		.layer = SFC_FLOW_ITEM_L2,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_pppoex,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_IPV4,
		.name = "IPV4",
		.prev_layer = SFC_FLOW_ITEM_L2,
		.layer = SFC_FLOW_ITEM_L3,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_ipv4,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_IPV6,
		.name = "IPV6",
		.prev_layer = SFC_FLOW_ITEM_L2,
		.layer = SFC_FLOW_ITEM_L3,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_ipv6,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_TCP,
		.name = "TCP",
		.prev_layer = SFC_FLOW_ITEM_L3,
		.layer = SFC_FLOW_ITEM_L4,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_tcp,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_UDP,
		.name = "UDP",
		.prev_layer = SFC_FLOW_ITEM_L3,
		.layer = SFC_FLOW_ITEM_L4,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_udp,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_VXLAN,
		.name = "VXLAN",
		.prev_layer = SFC_FLOW_ITEM_L4,
		.layer = SFC_FLOW_ITEM_START_LAYER,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_vxlan,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_GENEVE,
		.name = "GENEVE",
		.prev_layer = SFC_FLOW_ITEM_L4,
		.layer = SFC_FLOW_ITEM_START_LAYER,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_geneve,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_NVGRE,
		.name = "NVGRE",
		.prev_layer = SFC_FLOW_ITEM_L3,
		.layer = SFC_FLOW_ITEM_START_LAYER,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_nvgre,
	},
};

/*
 * Protocol-independent flow API support
 */
static int
sfc_flow_parse_attr(struct sfc_adapter *sa,
		    const struct rte_flow_attr *attr,
		    struct rte_flow *flow,
		    struct rte_flow_error *error)
{
	struct sfc_flow_spec *spec = &flow->spec;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	struct sfc_flow_spec_mae *spec_mae = &spec->mae;
	struct sfc_mae *mae = &sa->mae;

	if (attr == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				   "NULL attribute");
		return -rte_errno;
	}
	if (attr->group != 0) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_GROUP, attr,
				   "Groups are not supported");
		return -rte_errno;
	}
	if (attr->egress != 0 && attr->transfer == 0) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, attr,
				   "Egress is not supported");
		return -rte_errno;
	}
	if (attr->ingress == 0 && attr->transfer == 0) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS, attr,
				   "Ingress is compulsory");
		return -rte_errno;
	}
	if (attr->transfer == 0) {
		if (attr->priority != 0) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					   attr, "Priorities are unsupported");
			return -rte_errno;
		}
		spec->type = SFC_FLOW_SPEC_FILTER;
		spec_filter->template.efs_flags |= EFX_FILTER_FLAG_RX;
		spec_filter->template.efs_rss_context = EFX_RSS_CONTEXT_DEFAULT;
		spec_filter->template.efs_priority = EFX_FILTER_PRI_MANUAL;
	} else {
		if (mae->status != SFC_MAE_STATUS_ADMIN) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
					   attr, "Transfer is not supported");
			return -rte_errno;
		}
		if (attr->priority > mae->nb_action_rule_prios_max) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					   attr, "Unsupported priority level");
			return -rte_errno;
		}
		spec->type = SFC_FLOW_SPEC_MAE;
		spec_mae->priority = attr->priority;
		spec_mae->match_spec = NULL;
		spec_mae->action_set = NULL;
		spec_mae->rule_id.id = EFX_MAE_RSRC_ID_INVALID;
	}

	return 0;
}

/* Get item from array sfc_flow_items */
static const struct sfc_flow_item *
sfc_flow_get_item(const struct sfc_flow_item *items,
		  unsigned int nb_items,
		  enum rte_flow_item_type type)
{
	unsigned int i;

	for (i = 0; i < nb_items; i++)
		if (items[i].type == type)
			return &items[i];

	return NULL;
}

int
sfc_flow_parse_pattern(struct sfc_adapter *sa,
		       const struct sfc_flow_item *flow_items,
		       unsigned int nb_flow_items,
		       const struct rte_flow_item pattern[],
		       struct sfc_flow_parse_ctx *parse_ctx,
		       struct rte_flow_error *error)
{
	int rc;
	unsigned int prev_layer = SFC_FLOW_ITEM_ANY_LAYER;
	boolean_t is_ifrm = B_FALSE;
	const struct sfc_flow_item *item;

	if (pattern == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_NUM, NULL,
				   "NULL pattern");
		return -rte_errno;
	}

	for (; pattern->type != RTE_FLOW_ITEM_TYPE_END; pattern++) {
		item = sfc_flow_get_item(flow_items, nb_flow_items,
					 pattern->type);
		if (item == NULL) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ITEM, pattern,
					   "Unsupported pattern item");
			return -rte_errno;
		}

		/*
		 * Omitting one or several protocol layers at the beginning
		 * of pattern is supported
		 */
		if (item->prev_layer != SFC_FLOW_ITEM_ANY_LAYER &&
		    prev_layer != SFC_FLOW_ITEM_ANY_LAYER &&
		    item->prev_layer != prev_layer) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ITEM, pattern,
					   "Unexpected sequence of pattern items");
			return -rte_errno;
		}

		/*
		 * Allow only VOID and ETH pattern items in the inner frame.
		 * Also check that there is only one tunneling protocol.
		 */
		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
		case RTE_FLOW_ITEM_TYPE_ETH:
			break;

		case RTE_FLOW_ITEM_TYPE_VXLAN:
		case RTE_FLOW_ITEM_TYPE_GENEVE:
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			if (is_ifrm) {
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					pattern,
					"More than one tunneling protocol");
				return -rte_errno;
			}
			is_ifrm = B_TRUE;
			break;

		default:
			if (parse_ctx->type == SFC_FLOW_PARSE_CTX_FILTER &&
			    is_ifrm) {
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					pattern,
					"There is an unsupported pattern item "
					"in the inner frame");
				return -rte_errno;
			}
			break;
		}

		if (parse_ctx->type != item->ctx_type) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, pattern,
					"Parse context type mismatch");
			return -rte_errno;
		}

		rc = item->parse(pattern, parse_ctx, error);
		if (rc != 0) {
			sfc_err(sa, "failed to parse item %s: %s",
				item->name, strerror(-rc));
			return rc;
		}

		if (item->layer != SFC_FLOW_ITEM_ANY_LAYER)
			prev_layer = item->layer;
	}

	return 0;
}

static int
sfc_flow_parse_queue(struct sfc_adapter *sa,
		     const struct rte_flow_action_queue *queue,
		     struct rte_flow *flow)
{
	struct sfc_flow_spec *spec = &flow->spec;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	struct sfc_rxq *rxq;
	struct sfc_rxq_info *rxq_info;

	if (queue->index >= sfc_sa2shared(sa)->ethdev_rxq_count)
		return -EINVAL;

	rxq = sfc_rxq_ctrl_by_ethdev_qid(sa, queue->index);
	spec_filter->template.efs_dmaq_id = (uint16_t)rxq->hw_index;

	rxq_info = &sfc_sa2shared(sa)->rxq_info[queue->index];

	if ((rxq_info->rxq_flags & SFC_RXQ_FLAG_RSS_HASH) != 0) {
		struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
		struct sfc_rss *ethdev_rss = &sas->rss;

		spec_filter->template.efs_flags |= EFX_FILTER_FLAG_RX_RSS;
		spec_filter->rss_ctx = &ethdev_rss->dummy_ctx;
	}

	return 0;
}

static int
sfc_flow_parse_rss(struct sfc_adapter *sa,
		   const struct rte_flow_action_rss *action_rss,
		   struct rte_flow *flow)
{
	struct sfc_flow_spec_filter *spec_filter = &flow->spec.filter;
	struct sfc_flow_rss_conf conf;
	uint16_t sw_qid_min;
	struct sfc_rxq *rxq;
	int rc;

	spec_filter->template.efs_flags |= EFX_FILTER_FLAG_RX_RSS;

	rc = sfc_flow_rss_parse_conf(sa, action_rss, &conf, &sw_qid_min);
	if (rc != 0)
		return -rc;

	rxq = sfc_rxq_ctrl_by_ethdev_qid(sa, sw_qid_min);
	spec_filter->template.efs_dmaq_id = rxq->hw_index;

	spec_filter->rss_ctx = sfc_flow_rss_ctx_reuse(sa, &conf, sw_qid_min,
						      action_rss->queue);
	if (spec_filter->rss_ctx != NULL)
		return 0;

	rc = sfc_flow_rss_ctx_add(sa, &conf, sw_qid_min, action_rss->queue,
				  &spec_filter->rss_ctx);
	if (rc != 0)
		return -rc;

	return 0;
}

static int
sfc_flow_spec_flush(struct sfc_adapter *sa, struct sfc_flow_spec *spec,
		    unsigned int filters_count)
{
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	unsigned int i;
	int ret = 0;

	for (i = 0; i < filters_count; i++) {
		int rc;

		rc = efx_filter_remove(sa->nic, &spec_filter->filters[i]);
		if (ret == 0 && rc != 0) {
			sfc_err(sa, "failed to remove filter specification "
				"(rc = %d)", rc);
			ret = rc;
		}
	}

	return ret;
}

static int
sfc_flow_spec_insert(struct sfc_adapter *sa, struct sfc_flow_spec *spec)
{
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	unsigned int i;
	int rc = 0;

	for (i = 0; i < spec_filter->count; i++) {
		rc = efx_filter_insert(sa->nic, &spec_filter->filters[i]);
		if (rc != 0) {
			sfc_flow_spec_flush(sa, spec, i);
			break;
		}
	}

	return rc;
}

static int
sfc_flow_spec_remove(struct sfc_adapter *sa, struct sfc_flow_spec *spec)
{
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;

	return sfc_flow_spec_flush(sa, spec, spec_filter->count);
}

static int
sfc_flow_filter_insert(struct sfc_adapter *sa,
		       struct rte_flow *flow)
{
	struct sfc_flow_spec_filter *spec_filter = &flow->spec.filter;
	struct sfc_flow_rss_ctx *rss_ctx = spec_filter->rss_ctx;
	int rc = 0;

	rc = sfc_flow_rss_ctx_program(sa, rss_ctx);
	if (rc != 0)
		goto fail_rss_ctx_program;

	if (rss_ctx != NULL) {
		unsigned int i;

		/*
		 * At this point, fully elaborated filter specifications
		 * have been produced from the template. To make sure that
		 * RSS behaviour is consistent between them, set the same
		 * RSS context value everywhere.
		 */
		for (i = 0; i < spec_filter->count; i++) {
			efx_filter_spec_t *spec = &spec_filter->filters[i];

			spec->efs_rss_context = rss_ctx->nic_handle;
		}
	}

	rc = sfc_flow_spec_insert(sa, &flow->spec);
	if (rc != 0)
		goto fail_filter_insert;

	return 0;

fail_filter_insert:
	sfc_flow_rss_ctx_terminate(sa, rss_ctx);

fail_rss_ctx_program:
	return rc;
}

static int
sfc_flow_filter_remove(struct sfc_adapter *sa,
		       struct rte_flow *flow)
{
	struct sfc_flow_spec_filter *spec_filter = &flow->spec.filter;
	int rc = 0;

	rc = sfc_flow_spec_remove(sa, &flow->spec);
	if (rc != 0)
		return rc;

	sfc_flow_rss_ctx_terminate(sa, spec_filter->rss_ctx);

	return 0;
}

static int
sfc_flow_parse_mark(struct sfc_adapter *sa,
		    const struct rte_flow_action_mark *mark,
		    struct rte_flow *flow)
{
	struct sfc_flow_spec *spec = &flow->spec;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	uint32_t mark_max;

	mark_max = encp->enc_filter_action_mark_max;
	if (sfc_ft_is_active(sa))
		mark_max = RTE_MIN(mark_max, SFC_FT_USER_MARK_MASK);

	if (mark == NULL || mark->id > mark_max)
		return EINVAL;

	spec_filter->template.efs_flags |= EFX_FILTER_FLAG_ACTION_MARK;
	spec_filter->template.efs_mark = mark->id;

	return 0;
}

static int
sfc_flow_parse_actions(struct sfc_adapter *sa,
		       const struct rte_flow_action actions[],
		       struct rte_flow *flow,
		       struct rte_flow_error *error)
{
	int rc;
	struct sfc_flow_spec *spec = &flow->spec;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	const unsigned int dp_rx_features = sa->priv.dp_rx->features;
	const uint64_t rx_metadata = sa->negotiated_rx_metadata;
	uint32_t actions_set = 0;
	const uint32_t fate_actions_mask = (1UL << RTE_FLOW_ACTION_TYPE_QUEUE) |
					   (1UL << RTE_FLOW_ACTION_TYPE_RSS) |
					   (1UL << RTE_FLOW_ACTION_TYPE_DROP);
	const uint32_t mark_actions_mask = (1UL << RTE_FLOW_ACTION_TYPE_MARK) |
					   (1UL << RTE_FLOW_ACTION_TYPE_FLAG);

	if (actions == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "NULL actions");
		return -rte_errno;
	}

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			SFC_BUILD_SET_OVERFLOW(RTE_FLOW_ACTION_TYPE_VOID,
					       actions_set);
			break;

		case RTE_FLOW_ACTION_TYPE_QUEUE:
			SFC_BUILD_SET_OVERFLOW(RTE_FLOW_ACTION_TYPE_QUEUE,
					       actions_set);
			if ((actions_set & fate_actions_mask) != 0)
				goto fail_fate_actions;

			rc = sfc_flow_parse_queue(sa, actions->conf, flow);
			if (rc != 0) {
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, actions,
					"Bad QUEUE action");
				return -rte_errno;
			}
			break;

		case RTE_FLOW_ACTION_TYPE_RSS:
			SFC_BUILD_SET_OVERFLOW(RTE_FLOW_ACTION_TYPE_RSS,
					       actions_set);
			if ((actions_set & fate_actions_mask) != 0)
				goto fail_fate_actions;

			rc = sfc_flow_parse_rss(sa, actions->conf, flow);
			if (rc != 0) {
				rte_flow_error_set(error, -rc,
					RTE_FLOW_ERROR_TYPE_ACTION, actions,
					"Bad RSS action");
				return -rte_errno;
			}
			break;

		case RTE_FLOW_ACTION_TYPE_DROP:
			SFC_BUILD_SET_OVERFLOW(RTE_FLOW_ACTION_TYPE_DROP,
					       actions_set);
			if ((actions_set & fate_actions_mask) != 0)
				goto fail_fate_actions;

			spec_filter->template.efs_dmaq_id =
				EFX_FILTER_SPEC_RX_DMAQ_ID_DROP;
			break;

		case RTE_FLOW_ACTION_TYPE_FLAG:
			SFC_BUILD_SET_OVERFLOW(RTE_FLOW_ACTION_TYPE_FLAG,
					       actions_set);
			if ((actions_set & mark_actions_mask) != 0)
				goto fail_actions_overlap;

			if ((dp_rx_features & SFC_DP_RX_FEAT_FLOW_FLAG) == 0) {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"FLAG action is not supported on the current Rx datapath");
				return -rte_errno;
			} else if ((rx_metadata &
				    RTE_ETH_RX_METADATA_USER_FLAG) == 0) {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"flag delivery has not been negotiated");
				return -rte_errno;
			}

			spec_filter->template.efs_flags |=
				EFX_FILTER_FLAG_ACTION_FLAG;
			break;

		case RTE_FLOW_ACTION_TYPE_MARK:
			SFC_BUILD_SET_OVERFLOW(RTE_FLOW_ACTION_TYPE_MARK,
					       actions_set);
			if ((actions_set & mark_actions_mask) != 0)
				goto fail_actions_overlap;

			if ((dp_rx_features & SFC_DP_RX_FEAT_FLOW_MARK) == 0) {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"MARK action is not supported on the current Rx datapath");
				return -rte_errno;
			} else if ((rx_metadata &
				    RTE_ETH_RX_METADATA_USER_MARK) == 0) {
				rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"mark delivery has not been negotiated");
				return -rte_errno;
			}

			rc = sfc_flow_parse_mark(sa, actions->conf, flow);
			if (rc != 0) {
				rte_flow_error_set(error, rc,
					RTE_FLOW_ERROR_TYPE_ACTION, actions,
					"Bad MARK action");
				return -rte_errno;
			}
			break;

		default:
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ACTION, actions,
					   "Action is not supported");
			return -rte_errno;
		}

		actions_set |= (1UL << actions->type);
	}

	/* When fate is unknown, drop traffic. */
	if ((actions_set & fate_actions_mask) == 0) {
		spec_filter->template.efs_dmaq_id =
			EFX_FILTER_SPEC_RX_DMAQ_ID_DROP;
	}

	return 0;

fail_fate_actions:
	rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Cannot combine several fate-deciding actions, "
			   "choose between QUEUE, RSS or DROP");
	return -rte_errno;

fail_actions_overlap:
	rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Overlapping actions are not supported");
	return -rte_errno;
}

/**
 * Set the EFX_FILTER_MATCH_UNKNOWN_UCAST_DST
 * and EFX_FILTER_MATCH_UNKNOWN_MCAST_DST match flags in the same
 * specifications after copying.
 *
 * @param spec[in, out]
 *   SFC flow specification to update.
 * @param filters_count_for_one_val[in]
 *   How many specifications should have the same match flag, what is the
 *   number of specifications before copying.
 * @param error[out]
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_set_unknown_dst_flags(struct sfc_flow_spec *spec,
			       unsigned int filters_count_for_one_val,
			       struct rte_flow_error *error)
{
	unsigned int i;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	static const efx_filter_match_flags_t vals[] = {
		EFX_FILTER_MATCH_UNKNOWN_UCAST_DST,
		EFX_FILTER_MATCH_UNKNOWN_MCAST_DST
	};

	if (filters_count_for_one_val * RTE_DIM(vals) != spec_filter->count) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Number of specifications is incorrect while copying "
			"by unknown destination flags");
		return -rte_errno;
	}

	for (i = 0; i < spec_filter->count; i++) {
		/* The check above ensures that divisor can't be zero here */
		spec_filter->filters[i].efs_match_flags |=
			vals[i / filters_count_for_one_val];
	}

	return 0;
}

/**
 * Check that the following conditions are met:
 * - the list of supported filters has a filter
 *   with EFX_FILTER_MATCH_UNKNOWN_MCAST_DST flag instead of
 *   EFX_FILTER_MATCH_UNKNOWN_UCAST_DST, since this filter will also
 *   be inserted.
 *
 * @param match[in]
 *   The match flags of filter.
 * @param spec[in]
 *   Specification to be supplemented.
 * @param filter[in]
 *   SFC filter with list of supported filters.
 */
static boolean_t
sfc_flow_check_unknown_dst_flags(efx_filter_match_flags_t match,
				 __rte_unused efx_filter_spec_t *spec,
				 struct sfc_filter *filter)
{
	unsigned int i;
	efx_filter_match_flags_t match_mcast_dst;

	match_mcast_dst =
		(match & ~EFX_FILTER_MATCH_UNKNOWN_UCAST_DST) |
		EFX_FILTER_MATCH_UNKNOWN_MCAST_DST;
	for (i = 0; i < filter->supported_match_num; i++) {
		if (match_mcast_dst == filter->supported_match[i])
			return B_TRUE;
	}

	return B_FALSE;
}

/**
 * Set the EFX_FILTER_MATCH_ETHER_TYPE match flag and EFX_ETHER_TYPE_IPV4 and
 * EFX_ETHER_TYPE_IPV6 values of the corresponding field in the same
 * specifications after copying.
 *
 * @param spec[in, out]
 *   SFC flow specification to update.
 * @param filters_count_for_one_val[in]
 *   How many specifications should have the same EtherType value, what is the
 *   number of specifications before copying.
 * @param error[out]
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_set_ethertypes(struct sfc_flow_spec *spec,
			unsigned int filters_count_for_one_val,
			struct rte_flow_error *error)
{
	unsigned int i;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	static const uint16_t vals[] = {
		EFX_ETHER_TYPE_IPV4, EFX_ETHER_TYPE_IPV6
	};

	if (filters_count_for_one_val * RTE_DIM(vals) != spec_filter->count) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Number of specifications is incorrect "
			"while copying by Ethertype");
		return -rte_errno;
	}

	for (i = 0; i < spec_filter->count; i++) {
		spec_filter->filters[i].efs_match_flags |=
			EFX_FILTER_MATCH_ETHER_TYPE;

		/*
		 * The check above ensures that
		 * filters_count_for_one_val is not 0
		 */
		spec_filter->filters[i].efs_ether_type =
			vals[i / filters_count_for_one_val];
	}

	return 0;
}

/**
 * Set the EFX_FILTER_MATCH_OUTER_VID match flag with value 0
 * in the same specifications after copying.
 *
 * @param spec[in, out]
 *   SFC flow specification to update.
 * @param filters_count_for_one_val[in]
 *   How many specifications should have the same match flag, what is the
 *   number of specifications before copying.
 * @param error[out]
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_set_outer_vid_flag(struct sfc_flow_spec *spec,
			    unsigned int filters_count_for_one_val,
			    struct rte_flow_error *error)
{
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	unsigned int i;

	if (filters_count_for_one_val != spec_filter->count) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Number of specifications is incorrect "
			"while copying by outer VLAN ID");
		return -rte_errno;
	}

	for (i = 0; i < spec_filter->count; i++) {
		spec_filter->filters[i].efs_match_flags |=
			EFX_FILTER_MATCH_OUTER_VID;

		spec_filter->filters[i].efs_outer_vid = 0;
	}

	return 0;
}

/**
 * Set the EFX_FILTER_MATCH_IFRM_UNKNOWN_UCAST_DST and
 * EFX_FILTER_MATCH_IFRM_UNKNOWN_MCAST_DST match flags in the same
 * specifications after copying.
 *
 * @param spec[in, out]
 *   SFC flow specification to update.
 * @param filters_count_for_one_val[in]
 *   How many specifications should have the same match flag, what is the
 *   number of specifications before copying.
 * @param error[out]
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_set_ifrm_unknown_dst_flags(struct sfc_flow_spec *spec,
				    unsigned int filters_count_for_one_val,
				    struct rte_flow_error *error)
{
	unsigned int i;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	static const efx_filter_match_flags_t vals[] = {
		EFX_FILTER_MATCH_IFRM_UNKNOWN_UCAST_DST,
		EFX_FILTER_MATCH_IFRM_UNKNOWN_MCAST_DST
	};

	if (filters_count_for_one_val * RTE_DIM(vals) != spec_filter->count) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Number of specifications is incorrect while copying "
			"by inner frame unknown destination flags");
		return -rte_errno;
	}

	for (i = 0; i < spec_filter->count; i++) {
		/* The check above ensures that divisor can't be zero here */
		spec_filter->filters[i].efs_match_flags |=
			vals[i / filters_count_for_one_val];
	}

	return 0;
}

/**
 * Check that the following conditions are met:
 * - the specification corresponds to a filter for encapsulated traffic
 * - the list of supported filters has a filter
 *   with EFX_FILTER_MATCH_IFRM_UNKNOWN_MCAST_DST flag instead of
 *   EFX_FILTER_MATCH_IFRM_UNKNOWN_UCAST_DST, since this filter will also
 *   be inserted.
 *
 * @param match[in]
 *   The match flags of filter.
 * @param spec[in]
 *   Specification to be supplemented.
 * @param filter[in]
 *   SFC filter with list of supported filters.
 */
static boolean_t
sfc_flow_check_ifrm_unknown_dst_flags(efx_filter_match_flags_t match,
				      efx_filter_spec_t *spec,
				      struct sfc_filter *filter)
{
	unsigned int i;
	efx_tunnel_protocol_t encap_type = spec->efs_encap_type;
	efx_filter_match_flags_t match_mcast_dst;

	if (encap_type == EFX_TUNNEL_PROTOCOL_NONE)
		return B_FALSE;

	match_mcast_dst =
		(match & ~EFX_FILTER_MATCH_IFRM_UNKNOWN_UCAST_DST) |
		EFX_FILTER_MATCH_IFRM_UNKNOWN_MCAST_DST;
	for (i = 0; i < filter->supported_match_num; i++) {
		if (match_mcast_dst == filter->supported_match[i])
			return B_TRUE;
	}

	return B_FALSE;
}

/**
 * Check that the list of supported filters has a filter that differs
 * from @p match in that it has no flag EFX_FILTER_MATCH_OUTER_VID
 * in this case that filter will be used and the flag
 * EFX_FILTER_MATCH_OUTER_VID is not needed.
 *
 * @param match[in]
 *   The match flags of filter.
 * @param spec[in]
 *   Specification to be supplemented.
 * @param filter[in]
 *   SFC filter with list of supported filters.
 */
static boolean_t
sfc_flow_check_outer_vid_flag(efx_filter_match_flags_t match,
			      __rte_unused efx_filter_spec_t *spec,
			      struct sfc_filter *filter)
{
	unsigned int i;
	efx_filter_match_flags_t match_without_vid =
		match & ~EFX_FILTER_MATCH_OUTER_VID;

	for (i = 0; i < filter->supported_match_num; i++) {
		if (match_without_vid == filter->supported_match[i])
			return B_FALSE;
	}

	return B_TRUE;
}

/*
 * Match flags that can be automatically added to filters.
 * Selecting the last minimum when searching for the copy flag ensures that the
 * EFX_FILTER_MATCH_UNKNOWN_UCAST_DST flag has a higher priority than
 * EFX_FILTER_MATCH_ETHER_TYPE. This is because the filter
 * EFX_FILTER_MATCH_UNKNOWN_UCAST_DST is at the end of the list of supported
 * filters.
 */
static const struct sfc_flow_copy_flag sfc_flow_copy_flags[] = {
	{
		.flag = EFX_FILTER_MATCH_UNKNOWN_UCAST_DST,
		.vals_count = 2,
		.set_vals = sfc_flow_set_unknown_dst_flags,
		.spec_check = sfc_flow_check_unknown_dst_flags,
	},
	{
		.flag = EFX_FILTER_MATCH_ETHER_TYPE,
		.vals_count = 2,
		.set_vals = sfc_flow_set_ethertypes,
		.spec_check = NULL,
	},
	{
		.flag = EFX_FILTER_MATCH_IFRM_UNKNOWN_UCAST_DST,
		.vals_count = 2,
		.set_vals = sfc_flow_set_ifrm_unknown_dst_flags,
		.spec_check = sfc_flow_check_ifrm_unknown_dst_flags,
	},
	{
		.flag = EFX_FILTER_MATCH_OUTER_VID,
		.vals_count = 1,
		.set_vals = sfc_flow_set_outer_vid_flag,
		.spec_check = sfc_flow_check_outer_vid_flag,
	},
};

/* Get item from array sfc_flow_copy_flags */
static const struct sfc_flow_copy_flag *
sfc_flow_get_copy_flag(efx_filter_match_flags_t flag)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(sfc_flow_copy_flags); i++) {
		if (sfc_flow_copy_flags[i].flag == flag)
			return &sfc_flow_copy_flags[i];
	}

	return NULL;
}

/**
 * Make copies of the specifications, set match flag and values
 * of the field that corresponds to it.
 *
 * @param spec[in, out]
 *   SFC flow specification to update.
 * @param flag[in]
 *   The match flag to add.
 * @param error[out]
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_spec_add_match_flag(struct sfc_flow_spec *spec,
			     efx_filter_match_flags_t flag,
			     struct rte_flow_error *error)
{
	unsigned int i;
	unsigned int new_filters_count;
	unsigned int filters_count_for_one_val;
	const struct sfc_flow_copy_flag *copy_flag;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	int rc;

	copy_flag = sfc_flow_get_copy_flag(flag);
	if (copy_flag == NULL) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Unsupported spec field for copying");
		return -rte_errno;
	}

	new_filters_count = spec_filter->count * copy_flag->vals_count;
	if (new_filters_count > SF_FLOW_SPEC_NB_FILTERS_MAX) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Too much EFX specifications in the flow rule");
		return -rte_errno;
	}

	/* Copy filters specifications */
	for (i = spec_filter->count; i < new_filters_count; i++) {
		spec_filter->filters[i] =
			spec_filter->filters[i - spec_filter->count];
	}

	filters_count_for_one_val = spec_filter->count;
	spec_filter->count = new_filters_count;

	rc = copy_flag->set_vals(spec, filters_count_for_one_val, error);
	if (rc != 0)
		return rc;

	return 0;
}

/**
 * Check that the given set of match flags missing in the original filter spec
 * could be covered by adding spec copies which specify the corresponding
 * flags and packet field values to match.
 *
 * @param miss_flags[in]
 *   Flags that are missing until the supported filter.
 * @param spec[in]
 *   Specification to be supplemented.
 * @param filter[in]
 *   SFC filter.
 *
 * @return
 *   Number of specifications after copy or 0, if the flags can not be added.
 */
static unsigned int
sfc_flow_check_missing_flags(efx_filter_match_flags_t miss_flags,
			     efx_filter_spec_t *spec,
			     struct sfc_filter *filter)
{
	unsigned int i;
	efx_filter_match_flags_t copy_flags = 0;
	efx_filter_match_flags_t flag;
	efx_filter_match_flags_t match = spec->efs_match_flags | miss_flags;
	sfc_flow_spec_check *check;
	unsigned int multiplier = 1;

	for (i = 0; i < RTE_DIM(sfc_flow_copy_flags); i++) {
		flag = sfc_flow_copy_flags[i].flag;
		check = sfc_flow_copy_flags[i].spec_check;
		if ((flag & miss_flags) == flag) {
			if (check != NULL && (!check(match, spec, filter)))
				continue;

			copy_flags |= flag;
			multiplier *= sfc_flow_copy_flags[i].vals_count;
		}
	}

	if (copy_flags == miss_flags)
		return multiplier;

	return 0;
}

/**
 * Attempt to supplement the specification template to the minimally
 * supported set of match flags. To do this, it is necessary to copy
 * the specifications, filling them with the values of fields that
 * correspond to the missing flags.
 * The necessary and sufficient filter is built from the fewest number
 * of copies which could be made to cover the minimally required set
 * of flags.
 *
 * @param sa[in]
 *   SFC adapter.
 * @param spec[in, out]
 *   SFC flow specification to update.
 * @param error[out]
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_spec_filters_complete(struct sfc_adapter *sa,
			       struct sfc_flow_spec *spec,
			       struct rte_flow_error *error)
{
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	struct sfc_filter *filter = &sa->filter;
	efx_filter_match_flags_t miss_flags;
	efx_filter_match_flags_t min_miss_flags = 0;
	efx_filter_match_flags_t match;
	unsigned int min_multiplier = UINT_MAX;
	unsigned int multiplier;
	unsigned int i;
	int rc;

	match = spec_filter->template.efs_match_flags;
	for (i = 0; i < filter->supported_match_num; i++) {
		if ((match & filter->supported_match[i]) == match) {
			miss_flags = filter->supported_match[i] & (~match);
			multiplier = sfc_flow_check_missing_flags(miss_flags,
				&spec_filter->template, filter);
			if (multiplier > 0) {
				if (multiplier <= min_multiplier) {
					min_multiplier = multiplier;
					min_miss_flags = miss_flags;
				}
			}
		}
	}

	if (min_multiplier == UINT_MAX) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "The flow rule pattern is unsupported");
		return -rte_errno;
	}

	for (i = 0; i < RTE_DIM(sfc_flow_copy_flags); i++) {
		efx_filter_match_flags_t flag = sfc_flow_copy_flags[i].flag;

		if ((flag & min_miss_flags) == flag) {
			rc = sfc_flow_spec_add_match_flag(spec, flag, error);
			if (rc != 0)
				return rc;
		}
	}

	return 0;
}

/**
 * Check that set of match flags is referred to by a filter. Filter is
 * described by match flags with the ability to add OUTER_VID and INNER_VID
 * flags.
 *
 * @param match_flags[in]
 *   Set of match flags.
 * @param flags_pattern[in]
 *   Pattern of filter match flags.
 */
static boolean_t
sfc_flow_is_match_with_vids(efx_filter_match_flags_t match_flags,
			    efx_filter_match_flags_t flags_pattern)
{
	if ((match_flags & flags_pattern) != flags_pattern)
		return B_FALSE;

	switch (match_flags & ~flags_pattern) {
	case 0:
	case EFX_FILTER_MATCH_OUTER_VID:
	case EFX_FILTER_MATCH_OUTER_VID | EFX_FILTER_MATCH_INNER_VID:
		return B_TRUE;
	default:
		return B_FALSE;
	}
}

/**
 * Check whether the spec maps to a hardware filter which is known to be
 * ineffective despite being valid.
 *
 * @param filter[in]
 *   SFC filter with list of supported filters.
 * @param spec[in]
 *   SFC flow specification.
 */
static boolean_t
sfc_flow_is_match_flags_exception(struct sfc_filter *filter,
				  struct sfc_flow_spec *spec)
{
	unsigned int i;
	uint16_t ether_type;
	uint8_t ip_proto;
	efx_filter_match_flags_t match_flags;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;

	for (i = 0; i < spec_filter->count; i++) {
		match_flags = spec_filter->filters[i].efs_match_flags;

		if (sfc_flow_is_match_with_vids(match_flags,
						EFX_FILTER_MATCH_ETHER_TYPE) ||
		    sfc_flow_is_match_with_vids(match_flags,
						EFX_FILTER_MATCH_ETHER_TYPE |
						EFX_FILTER_MATCH_LOC_MAC)) {
			ether_type = spec_filter->filters[i].efs_ether_type;
			if (filter->supports_ip_proto_or_addr_filter &&
			    (ether_type == EFX_ETHER_TYPE_IPV4 ||
			     ether_type == EFX_ETHER_TYPE_IPV6))
				return B_TRUE;
		} else if (sfc_flow_is_match_with_vids(match_flags,
				EFX_FILTER_MATCH_ETHER_TYPE |
				EFX_FILTER_MATCH_IP_PROTO) ||
			   sfc_flow_is_match_with_vids(match_flags,
				EFX_FILTER_MATCH_ETHER_TYPE |
				EFX_FILTER_MATCH_IP_PROTO |
				EFX_FILTER_MATCH_LOC_MAC)) {
			ip_proto = spec_filter->filters[i].efs_ip_proto;
			if (filter->supports_rem_or_local_port_filter &&
			    (ip_proto == EFX_IPPROTO_TCP ||
			     ip_proto == EFX_IPPROTO_UDP))
				return B_TRUE;
		}
	}

	return B_FALSE;
}

static int
sfc_flow_validate_match_flags(struct sfc_adapter *sa,
			      struct rte_flow *flow,
			      struct rte_flow_error *error)
{
	struct sfc_flow_spec *spec = &flow->spec;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	efx_filter_spec_t *spec_tmpl = &spec_filter->template;
	efx_filter_match_flags_t match_flags = spec_tmpl->efs_match_flags;
	int rc;

	/* Initialize the first filter spec with template */
	spec_filter->filters[0] = *spec_tmpl;
	spec_filter->count = 1;

	if (!sfc_filter_is_match_supported(sa, match_flags)) {
		rc = sfc_flow_spec_filters_complete(sa, &flow->spec, error);
		if (rc != 0)
			return rc;
	}

	if (sfc_flow_is_match_flags_exception(&sa->filter, &flow->spec)) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"The flow rule pattern is unsupported");
		return -rte_errno;
	}

	return 0;
}

static int
sfc_flow_parse_rte_to_filter(struct rte_eth_dev *dev,
			     const struct rte_flow_item pattern[],
			     const struct rte_flow_action actions[],
			     struct rte_flow *flow,
			     struct rte_flow_error *error)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_flow_spec *spec = &flow->spec;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	struct sfc_flow_parse_ctx ctx;
	int rc;

	ctx.type = SFC_FLOW_PARSE_CTX_FILTER;
	ctx.filter = &spec_filter->template;

	rc = sfc_flow_parse_pattern(sa, sfc_flow_items, RTE_DIM(sfc_flow_items),
				    pattern, &ctx, error);
	if (rc != 0)
		goto fail_bad_value;

	rc = sfc_flow_parse_actions(sa, actions, flow, error);
	if (rc != 0)
		goto fail_bad_value;

	rc = sfc_flow_validate_match_flags(sa, flow, error);
	if (rc != 0)
		goto fail_bad_value;

	return 0;

fail_bad_value:
	return rc;
}

static int
sfc_flow_parse_rte_to_mae(struct rte_eth_dev *dev,
			  const struct rte_flow_item pattern[],
			  const struct rte_flow_action actions[],
			  struct rte_flow *flow,
			  struct rte_flow_error *error)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct sfc_flow_spec *spec = &flow->spec;
	struct sfc_flow_spec_mae *spec_mae = &spec->mae;
	int rc;

	/*
	 * If the flow is meant to be a TUNNEL rule in a FT context,
	 * preparse its actions and save its properties in spec_mae.
	 */
	rc = sfc_ft_tunnel_rule_detect(sa, actions, spec_mae, error);
	if (rc != 0)
		goto fail;

	rc = sfc_mae_rule_parse_pattern(sa, pattern, spec_mae, error);
	if (rc != 0)
		goto fail;

	if (spec_mae->ft_rule_type == SFC_FT_RULE_TUNNEL) {
		/*
		 * By design, this flow should be represented solely by the
		 * outer rule. But the HW/FW hasn't got support for setting
		 * Rx mark from RECIRC_ID on outer rule lookup yet. Neither
		 * does it support outer rule counters. As a workaround, an
		 * action rule of lower priority is used to do the job.
		 *
		 * So don't skip sfc_mae_rule_parse_actions() below.
		 */
	}

	rc = sfc_mae_rule_parse_actions(sa, actions, spec_mae, error);
	if (rc != 0)
		goto fail;

	if (spec_mae->ft_ctx != NULL) {
		if (spec_mae->ft_rule_type == SFC_FT_RULE_TUNNEL)
			spec_mae->ft_ctx->tunnel_rule_is_set = B_TRUE;

		++(spec_mae->ft_ctx->refcnt);
	}

	return 0;

fail:
	/* Reset these values to avoid confusing sfc_mae_flow_cleanup(). */
	spec_mae->ft_rule_type = SFC_FT_RULE_NONE;
	spec_mae->ft_ctx = NULL;

	return rc;
}

static int
sfc_flow_parse(struct rte_eth_dev *dev,
	       const struct rte_flow_attr *attr,
	       const struct rte_flow_item pattern[],
	       const struct rte_flow_action actions[],
	       struct rte_flow *flow,
	       struct rte_flow_error *error)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	const struct sfc_flow_ops_by_spec *ops;
	int rc;

	rc = sfc_flow_parse_attr(sa, attr, flow, error);
	if (rc != 0)
		return rc;

	ops = sfc_flow_get_ops_by_spec(flow);
	if (ops == NULL || ops->parse == NULL) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "No backend to handle this flow");
		return -rte_errno;
	}

	return ops->parse(dev, pattern, actions, flow, error);
}

static struct rte_flow *
sfc_flow_zmalloc(struct rte_flow_error *error)
{
	struct rte_flow *flow;

	flow = rte_zmalloc("sfc_rte_flow", sizeof(*flow), 0);
	if (flow == NULL) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to allocate memory");
	}

	return flow;
}

static void
sfc_flow_free(struct sfc_adapter *sa, struct rte_flow *flow)
{
	const struct sfc_flow_ops_by_spec *ops;

	ops = sfc_flow_get_ops_by_spec(flow);
	if (ops != NULL && ops->cleanup != NULL)
		ops->cleanup(sa, flow);

	rte_free(flow);
}

static int
sfc_flow_insert(struct sfc_adapter *sa, struct rte_flow *flow,
		struct rte_flow_error *error)
{
	const struct sfc_flow_ops_by_spec *ops;
	int rc;

	ops = sfc_flow_get_ops_by_spec(flow);
	if (ops == NULL || ops->insert == NULL) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "No backend to handle this flow");
		return rte_errno;
	}

	rc = ops->insert(sa, flow);
	if (rc != 0) {
		rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "Failed to insert the flow rule");
	}

	return rc;
}

static int
sfc_flow_remove(struct sfc_adapter *sa, struct rte_flow *flow,
		struct rte_flow_error *error)
{
	const struct sfc_flow_ops_by_spec *ops;
	int rc;

	ops = sfc_flow_get_ops_by_spec(flow);
	if (ops == NULL || ops->remove == NULL) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "No backend to handle this flow");
		return rte_errno;
	}

	rc = ops->remove(sa, flow);
	if (rc != 0) {
		rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "Failed to remove the flow rule");
	}

	return rc;
}

static int
sfc_flow_verify(struct sfc_adapter *sa, struct rte_flow *flow,
		struct rte_flow_error *error)
{
	const struct sfc_flow_ops_by_spec *ops;
	int rc = 0;

	ops = sfc_flow_get_ops_by_spec(flow);
	if (ops == NULL) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "No backend to handle this flow");
		return -rte_errno;
	}

	if (ops->verify != NULL) {
		SFC_ASSERT(sfc_adapter_is_locked(sa));
		rc = ops->verify(sa, flow);
	}

	if (rc != 0) {
		rte_flow_error_set(error, rc,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Failed to verify flow validity with FW");
		return -rte_errno;
	}

	return 0;
}

static int
sfc_flow_validate(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct rte_flow *flow;
	int rc;

	flow = sfc_flow_zmalloc(error);
	if (flow == NULL)
		return -rte_errno;

	sfc_adapter_lock(sa);

	rc = sfc_flow_parse(dev, attr, pattern, actions, flow, error);
	if (rc == 0)
		rc = sfc_flow_verify(sa, flow, error);

	sfc_flow_free(sa, flow);

	sfc_adapter_unlock(sa);

	return rc;
}

static struct rte_flow *
sfc_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct rte_flow *flow = NULL;
	int rc;

	flow = sfc_flow_zmalloc(error);
	if (flow == NULL)
		goto fail_no_mem;

	sfc_adapter_lock(sa);

	rc = sfc_flow_parse(dev, attr, pattern, actions, flow, error);
	if (rc != 0)
		goto fail_bad_value;

	TAILQ_INSERT_TAIL(&sa->flow_list, flow, entries);

	if (sa->state == SFC_ETHDEV_STARTED) {
		rc = sfc_flow_insert(sa, flow, error);
		if (rc != 0)
			goto fail_flow_insert;
	}

	sfc_adapter_unlock(sa);

	return flow;

fail_flow_insert:
	TAILQ_REMOVE(&sa->flow_list, flow, entries);

fail_bad_value:
	sfc_flow_free(sa, flow);
	sfc_adapter_unlock(sa);

fail_no_mem:
	return NULL;
}

static int
sfc_flow_destroy(struct rte_eth_dev *dev,
		 struct rte_flow *flow,
		 struct rte_flow_error *error)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct rte_flow *flow_ptr;
	int rc = EINVAL;

	sfc_adapter_lock(sa);

	TAILQ_FOREACH(flow_ptr, &sa->flow_list, entries) {
		if (flow_ptr == flow)
			rc = 0;
	}
	if (rc != 0) {
		rte_flow_error_set(error, rc,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to find flow rule to destroy");
		goto fail_bad_value;
	}

	if (sa->state == SFC_ETHDEV_STARTED)
		rc = sfc_flow_remove(sa, flow, error);

	TAILQ_REMOVE(&sa->flow_list, flow, entries);
	sfc_flow_free(sa, flow);

fail_bad_value:
	sfc_adapter_unlock(sa);

	return -rc;
}

static int
sfc_flow_flush(struct rte_eth_dev *dev,
	       struct rte_flow_error *error)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	struct rte_flow *flow;
	int ret = 0;

	sfc_adapter_lock(sa);

	while ((flow = TAILQ_FIRST(&sa->flow_list)) != NULL) {
		if (sa->state == SFC_ETHDEV_STARTED) {
			int rc;

			rc = sfc_flow_remove(sa, flow, error);
			if (rc != 0)
				ret = rc;
		}

		TAILQ_REMOVE(&sa->flow_list, flow, entries);
		sfc_flow_free(sa, flow);
	}

	sfc_adapter_unlock(sa);

	return -ret;
}

static int
sfc_flow_query(struct rte_eth_dev *dev,
	       struct rte_flow *flow,
	       const struct rte_flow_action *action,
	       void *data,
	       struct rte_flow_error *error)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	const struct sfc_flow_ops_by_spec *ops;
	int ret;

	sfc_adapter_lock(sa);

	ops = sfc_flow_get_ops_by_spec(flow);
	if (ops == NULL || ops->query == NULL) {
		ret = rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"No backend to handle this flow");
		goto fail_no_backend;
	}

	if (sa->state != SFC_ETHDEV_STARTED) {
		ret = rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"Can't query the flow: the adapter is not started");
		goto fail_not_started;
	}

	ret = ops->query(dev, flow, action, data, error);
	if (ret != 0)
		goto fail_query;

	sfc_adapter_unlock(sa);

	return 0;

fail_query:
fail_not_started:
fail_no_backend:
	sfc_adapter_unlock(sa);
	return ret;
}

static int
sfc_flow_isolate(struct rte_eth_dev *dev, int enable,
		 struct rte_flow_error *error)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	int ret = 0;

	sfc_adapter_lock(sa);
	if (sa->state != SFC_ETHDEV_INITIALIZED) {
		rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "please close the port first");
		ret = -rte_errno;
	} else {
		sfc_sa2shared(sa)->isolated = (enable) ? B_TRUE : B_FALSE;
	}
	sfc_adapter_unlock(sa);

	return ret;
}

static int
sfc_flow_pick_transfer_proxy(struct rte_eth_dev *dev,
			     uint16_t *transfer_proxy_port,
			     struct rte_flow_error *error)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	int ret;

	ret = sfc_mae_get_switch_domain_admin(sa->mae.switch_domain_id,
					      transfer_proxy_port);
	if (ret != 0) {
		return rte_flow_error_set(error, ret,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, NULL);
	}

	return 0;
}

const struct rte_flow_ops sfc_flow_ops = {
	.validate = sfc_flow_validate,
	.create = sfc_flow_create,
	.destroy = sfc_flow_destroy,
	.flush = sfc_flow_flush,
	.query = sfc_flow_query,
	.isolate = sfc_flow_isolate,
	.tunnel_decap_set = sfc_ft_decap_set,
	.tunnel_match = sfc_ft_match,
	.tunnel_action_decap_release = sfc_ft_action_decap_release,
	.tunnel_item_release = sfc_ft_item_release,
	.get_restore_info = sfc_ft_get_restore_info,
	.pick_transfer_proxy = sfc_flow_pick_transfer_proxy,
};

void
sfc_flow_init(struct sfc_adapter *sa)
{
	SFC_ASSERT(sfc_adapter_is_locked(sa));

	TAILQ_INIT(&sa->flow_list);
}

void
sfc_flow_fini(struct sfc_adapter *sa)
{
	struct rte_flow *flow;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	while ((flow = TAILQ_FIRST(&sa->flow_list)) != NULL) {
		TAILQ_REMOVE(&sa->flow_list, flow, entries);
		sfc_flow_free(sa, flow);
	}
}

void
sfc_flow_stop(struct sfc_adapter *sa)
{
	struct rte_flow *flow;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	TAILQ_FOREACH(flow, &sa->flow_list, entries)
		sfc_flow_remove(sa, flow, NULL);

	/*
	 * MAE counter service is not stopped on flow rule remove to avoid
	 * extra work. Make sure that it is stopped here.
	 */
	sfc_mae_counter_stop(sa);
}

int
sfc_flow_start(struct sfc_adapter *sa)
{
	struct rte_flow *flow;
	int rc = 0;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	sfc_ft_counters_reset(sa);

	TAILQ_FOREACH(flow, &sa->flow_list, entries) {
		rc = sfc_flow_insert(sa, flow, NULL);
		if (rc != 0)
			goto fail_bad_flow;
	}

	sfc_log_init(sa, "done");

fail_bad_flow:
	return rc;
}

static void
sfc_flow_cleanup(struct sfc_adapter *sa, struct rte_flow *flow)
{
	if (flow == NULL)
		return;

	sfc_flow_rss_ctx_del(sa, flow->spec.filter.rss_ctx);
}
