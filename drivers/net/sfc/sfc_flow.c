/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2017-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <rte_byteorder.h>
#include <rte_tailq.h>
#include <rte_common.h>
#include <rte_ethdev_driver.h>
#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_debug.h"
#include "sfc_rx.h"
#include "sfc_filter.h"
#include "sfc_flow.h"
#include "sfc_log.h"
#include "sfc_dp_rx.h"

struct sfc_flow_ops_by_spec {
	sfc_flow_parse_cb_t	*parse;
	sfc_flow_verify_cb_t	*verify;
	sfc_flow_cleanup_cb_t	*cleanup;
	sfc_flow_insert_cb_t	*insert;
	sfc_flow_remove_cb_t	*remove;
};

static sfc_flow_parse_cb_t sfc_flow_parse_rte_to_filter;
static sfc_flow_parse_cb_t sfc_flow_parse_rte_to_mae;
static sfc_flow_insert_cb_t sfc_flow_filter_insert;
static sfc_flow_remove_cb_t sfc_flow_filter_remove;

static const struct sfc_flow_ops_by_spec sfc_flow_ops_filter = {
	.parse = sfc_flow_parse_rte_to_filter,
	.verify = NULL,
	.cleanup = NULL,
	.insert = sfc_flow_filter_insert,
	.remove = sfc_flow_filter_remove,
};

static const struct sfc_flow_ops_by_spec sfc_flow_ops_mae = {
	.parse = sfc_flow_parse_rte_to_mae,
	.verify = sfc_mae_flow_verify,
	.cleanup = sfc_mae_flow_cleanup,
	.insert = sfc_mae_flow_insert,
	.remove = sfc_mae_flow_remove,
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
		.dst.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
		.src.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
		.type = 0xffff,
	};
	const struct rte_flow_item_eth ifrm_supp_mask = {
		.dst.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
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

	if (rte_is_same_ether_addr(&mask->dst, &supp_mask.dst)) {
		efx_spec->efs_match_flags |= is_ifrm ?
			EFX_FILTER_MATCH_IFRM_LOC_MAC :
			EFX_FILTER_MATCH_LOC_MAC;
		rte_memcpy(loc_mac, spec->dst.addr_bytes,
			   EFX_MAC_ADDR_LEN);
	} else if (memcmp(mask->dst.addr_bytes, ig_mask,
			  EFX_MAC_ADDR_LEN) == 0) {
		if (rte_is_unicast_ether_addr(&spec->dst))
			efx_spec->efs_match_flags |= is_ifrm ?
				EFX_FILTER_MATCH_IFRM_UNKNOWN_UCAST_DST :
				EFX_FILTER_MATCH_UNKNOWN_UCAST_DST;
		else
			efx_spec->efs_match_flags |= is_ifrm ?
				EFX_FILTER_MATCH_IFRM_UNKNOWN_MCAST_DST :
				EFX_FILTER_MATCH_UNKNOWN_MCAST_DST;
	} else if (!rte_is_zero_ether_addr(&mask->dst)) {
		goto fail_bad_mask;
	}

	/*
	 * ifrm_supp_mask ensures that the source address and
	 * ethertype masks are equal to zero in inner frame,
	 * so these fields are filled in only for the outer frame
	 */
	if (rte_is_same_ether_addr(&mask->src, &supp_mask.src)) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_REM_MAC;
		rte_memcpy(efx_spec->efs_rem_mac, spec->src.addr_bytes,
			   EFX_MAC_ADDR_LEN);
	} else if (!rte_is_zero_ether_addr(&mask->src)) {
		goto fail_bad_mask;
	}

	/*
	 * Ether type is in big-endian byte order in item and
	 * in little-endian in efx_spec, so byte swap is used
	 */
	if (mask->type == supp_mask.type) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_ETHER_TYPE;
		efx_spec->efs_ether_type = rte_bswap16(spec->type);
	} else if (mask->type != 0) {
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
		.tci = rte_cpu_to_be_16(ETH_VLAN_ID_MAX),
		.inner_type = RTE_BE16(0xffff),
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
	if (mask->tci == supp_mask.tci) {
		/* Apply mask to keep VID only */
		vid = rte_bswap16(spec->tci & mask->tci);

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
	if (mask->inner_type == supp_mask.inner_type) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_ETHER_TYPE;
		efx_spec->efs_ether_type = rte_bswap16(spec->inner_type);
	} else if (mask->inner_type) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "Bad mask for VLAN inner_type");
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
		.vni = { 0xff, 0xff, 0xff }
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

	rc = sfc_flow_set_efx_spec_vni_or_vsid(efx_spec, spec->vni,
					       mask->vni, item, error);

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

static const struct sfc_flow_item sfc_flow_items[] = {
	{
		.type = RTE_FLOW_ITEM_TYPE_VOID,
		.prev_layer = SFC_FLOW_ITEM_ANY_LAYER,
		.layer = SFC_FLOW_ITEM_ANY_LAYER,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_void,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.prev_layer = SFC_FLOW_ITEM_START_LAYER,
		.layer = SFC_FLOW_ITEM_L2,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_eth,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_VLAN,
		.prev_layer = SFC_FLOW_ITEM_L2,
		.layer = SFC_FLOW_ITEM_L2,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_vlan,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_IPV4,
		.prev_layer = SFC_FLOW_ITEM_L2,
		.layer = SFC_FLOW_ITEM_L3,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_ipv4,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_IPV6,
		.prev_layer = SFC_FLOW_ITEM_L2,
		.layer = SFC_FLOW_ITEM_L3,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_ipv6,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_TCP,
		.prev_layer = SFC_FLOW_ITEM_L3,
		.layer = SFC_FLOW_ITEM_L4,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_tcp,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_UDP,
		.prev_layer = SFC_FLOW_ITEM_L3,
		.layer = SFC_FLOW_ITEM_L4,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_udp,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_VXLAN,
		.prev_layer = SFC_FLOW_ITEM_L4,
		.layer = SFC_FLOW_ITEM_START_LAYER,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_vxlan,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_GENEVE,
		.prev_layer = SFC_FLOW_ITEM_L4,
		.layer = SFC_FLOW_ITEM_START_LAYER,
		.ctx_type = SFC_FLOW_PARSE_CTX_FILTER,
		.parse = sfc_flow_parse_geneve,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_NVGRE,
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
	if (attr->egress != 0) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, attr,
				   "Egress is not supported");
		return -rte_errno;
	}
	if (attr->ingress == 0) {
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
		if (mae->status != SFC_MAE_STATUS_SUPPORTED) {
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
sfc_flow_parse_pattern(const struct sfc_flow_item *flow_items,
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
		if (rc != 0)
			return rc;

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

	if (queue->index >= sfc_sa2shared(sa)->rxq_count)
		return -EINVAL;

	rxq = &sa->rxq_ctrl[queue->index];
	spec_filter->template.efs_dmaq_id = (uint16_t)rxq->hw_index;

	rxq_info = &sfc_sa2shared(sa)->rxq_info[queue->index];
	spec_filter->rss_hash_required = !!(rxq_info->rxq_flags &
					    SFC_RXQ_FLAG_RSS_HASH);

	return 0;
}

static int
sfc_flow_parse_rss(struct sfc_adapter *sa,
		   const struct rte_flow_action_rss *action_rss,
		   struct rte_flow *flow)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_rss *rss = &sas->rss;
	unsigned int rxq_sw_index;
	struct sfc_rxq *rxq;
	unsigned int rxq_hw_index_min;
	unsigned int rxq_hw_index_max;
	efx_rx_hash_type_t efx_hash_types;
	const uint8_t *rss_key;
	struct sfc_flow_spec *spec = &flow->spec;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	struct sfc_flow_rss *sfc_rss_conf = &spec_filter->rss_conf;
	unsigned int i;

	if (action_rss->queue_num == 0)
		return -EINVAL;

	rxq_sw_index = sfc_sa2shared(sa)->rxq_count - 1;
	rxq = &sa->rxq_ctrl[rxq_sw_index];
	rxq_hw_index_min = rxq->hw_index;
	rxq_hw_index_max = 0;

	for (i = 0; i < action_rss->queue_num; ++i) {
		rxq_sw_index = action_rss->queue[i];

		if (rxq_sw_index >= sfc_sa2shared(sa)->rxq_count)
			return -EINVAL;

		rxq = &sa->rxq_ctrl[rxq_sw_index];

		if (rxq->hw_index < rxq_hw_index_min)
			rxq_hw_index_min = rxq->hw_index;

		if (rxq->hw_index > rxq_hw_index_max)
			rxq_hw_index_max = rxq->hw_index;
	}

	switch (action_rss->func) {
	case RTE_ETH_HASH_FUNCTION_DEFAULT:
	case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
		break;
	default:
		return -EINVAL;
	}

	if (action_rss->level)
		return -EINVAL;

	/*
	 * Dummy RSS action with only one queue and no specific settings
	 * for hash types and key does not require dedicated RSS context
	 * and may be simplified to single queue action.
	 */
	if (action_rss->queue_num == 1 && action_rss->types == 0 &&
	    action_rss->key_len == 0) {
		spec_filter->template.efs_dmaq_id = rxq_hw_index_min;
		return 0;
	}

	if (action_rss->types) {
		int rc;

		rc = sfc_rx_hf_rte_to_efx(sa, action_rss->types,
					  &efx_hash_types);
		if (rc != 0)
			return -rc;
	} else {
		unsigned int i;

		efx_hash_types = 0;
		for (i = 0; i < rss->hf_map_nb_entries; ++i)
			efx_hash_types |= rss->hf_map[i].efx;
	}

	if (action_rss->key_len) {
		if (action_rss->key_len != sizeof(rss->key))
			return -EINVAL;

		rss_key = action_rss->key;
	} else {
		rss_key = rss->key;
	}

	spec_filter->rss = B_TRUE;

	sfc_rss_conf->rxq_hw_index_min = rxq_hw_index_min;
	sfc_rss_conf->rxq_hw_index_max = rxq_hw_index_max;
	sfc_rss_conf->rss_hash_types = efx_hash_types;
	rte_memcpy(sfc_rss_conf->rss_key, rss_key, sizeof(rss->key));

	for (i = 0; i < RTE_DIM(sfc_rss_conf->rss_tbl); ++i) {
		unsigned int nb_queues = action_rss->queue_num;
		unsigned int rxq_sw_index = action_rss->queue[i % nb_queues];
		struct sfc_rxq *rxq = &sa->rxq_ctrl[rxq_sw_index];

		sfc_rss_conf->rss_tbl[i] = rxq->hw_index - rxq_hw_index_min;
	}

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
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_rss *rss = &sas->rss;
	struct sfc_flow_spec_filter *spec_filter = &flow->spec.filter;
	struct sfc_flow_rss *flow_rss = &spec_filter->rss_conf;
	uint32_t efs_rss_context = EFX_RSS_CONTEXT_DEFAULT;
	boolean_t create_context;
	unsigned int i;
	int rc = 0;

	create_context = spec_filter->rss || (spec_filter->rss_hash_required &&
			rss->dummy_rss_context == EFX_RSS_CONTEXT_DEFAULT);

	if (create_context) {
		unsigned int rss_spread;
		unsigned int rss_hash_types;
		uint8_t *rss_key;

		if (spec_filter->rss) {
			rss_spread = MIN(flow_rss->rxq_hw_index_max -
					flow_rss->rxq_hw_index_min + 1,
					EFX_MAXRSS);
			rss_hash_types = flow_rss->rss_hash_types;
			rss_key = flow_rss->rss_key;
		} else {
			/*
			 * Initialize dummy RSS context parameters to have
			 * valid RSS hash. Use default RSS hash function and
			 * key.
			 */
			rss_spread = 1;
			rss_hash_types = rss->hash_types;
			rss_key = rss->key;
		}

		rc = efx_rx_scale_context_alloc(sa->nic,
						EFX_RX_SCALE_EXCLUSIVE,
						rss_spread,
						&efs_rss_context);
		if (rc != 0)
			goto fail_scale_context_alloc;

		rc = efx_rx_scale_mode_set(sa->nic, efs_rss_context,
					   rss->hash_alg,
					   rss_hash_types, B_TRUE);
		if (rc != 0)
			goto fail_scale_mode_set;

		rc = efx_rx_scale_key_set(sa->nic, efs_rss_context,
					  rss_key, sizeof(rss->key));
		if (rc != 0)
			goto fail_scale_key_set;
	} else {
		efs_rss_context = rss->dummy_rss_context;
	}

	if (spec_filter->rss || spec_filter->rss_hash_required) {
		/*
		 * At this point, fully elaborated filter specifications
		 * have been produced from the template. To make sure that
		 * RSS behaviour is consistent between them, set the same
		 * RSS context value everywhere.
		 */
		for (i = 0; i < spec_filter->count; i++) {
			efx_filter_spec_t *spec = &spec_filter->filters[i];

			spec->efs_rss_context = efs_rss_context;
			spec->efs_flags |= EFX_FILTER_FLAG_RX_RSS;
			if (spec_filter->rss)
				spec->efs_dmaq_id = flow_rss->rxq_hw_index_min;
		}
	}

	rc = sfc_flow_spec_insert(sa, &flow->spec);
	if (rc != 0)
		goto fail_filter_insert;

	if (create_context) {
		unsigned int dummy_tbl[RTE_DIM(flow_rss->rss_tbl)] = {0};
		unsigned int *tbl;

		tbl = spec_filter->rss ? flow_rss->rss_tbl : dummy_tbl;

		/*
		 * Scale table is set after filter insertion because
		 * the table entries are relative to the base RxQ ID
		 * and the latter is submitted to the HW by means of
		 * inserting a filter, so by the time of the request
		 * the HW knows all the information needed to verify
		 * the table entries, and the operation will succeed
		 */
		rc = efx_rx_scale_tbl_set(sa->nic, efs_rss_context,
					  tbl, RTE_DIM(flow_rss->rss_tbl));
		if (rc != 0)
			goto fail_scale_tbl_set;

		/* Remember created dummy RSS context */
		if (!spec_filter->rss)
			rss->dummy_rss_context = efs_rss_context;
	}

	return 0;

fail_scale_tbl_set:
	sfc_flow_spec_remove(sa, &flow->spec);

fail_filter_insert:
fail_scale_key_set:
fail_scale_mode_set:
	if (create_context)
		efx_rx_scale_context_free(sa->nic, efs_rss_context);

fail_scale_context_alloc:
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

	if (spec_filter->rss) {
		/*
		 * All specifications for a given flow rule have the same RSS
		 * context, so that RSS context value is taken from the first
		 * filter specification
		 */
		efx_filter_spec_t *spec = &spec_filter->filters[0];

		rc = efx_rx_scale_context_free(sa->nic, spec->efs_rss_context);
	}

	return rc;
}

static int
sfc_flow_parse_mark(struct sfc_adapter *sa,
		    const struct rte_flow_action_mark *mark,
		    struct rte_flow *flow)
{
	struct sfc_flow_spec *spec = &flow->spec;
	struct sfc_flow_spec_filter *spec_filter = &spec->filter;
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);

	if (mark == NULL || mark->id > encp->enc_filter_action_mark_max)
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

	rc = sfc_flow_parse_pattern(sfc_flow_items, RTE_DIM(sfc_flow_items),
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

	rc = sfc_mae_rule_parse_pattern(sa, pattern, spec_mae, error);
	if (rc != 0)
		return rc;

	rc = sfc_mae_rule_parse_actions(sa, actions, &spec_mae->action_set,
					error);
	if (rc != 0)
		return rc;

	return 0;
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

	if (sa->state == SFC_ADAPTER_STARTED) {
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

	if (sa->state == SFC_ADAPTER_STARTED)
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
		if (sa->state == SFC_ADAPTER_STARTED) {
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
sfc_flow_isolate(struct rte_eth_dev *dev, int enable,
		 struct rte_flow_error *error)
{
	struct sfc_adapter *sa = sfc_adapter_by_eth_dev(dev);
	int ret = 0;

	sfc_adapter_lock(sa);
	if (sa->state != SFC_ADAPTER_INITIALIZED) {
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

const struct rte_flow_ops sfc_flow_ops = {
	.validate = sfc_flow_validate,
	.create = sfc_flow_create,
	.destroy = sfc_flow_destroy,
	.flush = sfc_flow_flush,
	.query = NULL,
	.isolate = sfc_flow_isolate,
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
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_rss *rss = &sas->rss;
	struct rte_flow *flow;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	TAILQ_FOREACH(flow, &sa->flow_list, entries)
		sfc_flow_remove(sa, flow, NULL);

	if (rss->dummy_rss_context != EFX_RSS_CONTEXT_DEFAULT) {
		efx_rx_scale_context_free(sa->nic, rss->dummy_rss_context);
		rss->dummy_rss_context = EFX_RSS_CONTEXT_DEFAULT;
	}
}

int
sfc_flow_start(struct sfc_adapter *sa)
{
	struct rte_flow *flow;
	int rc = 0;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	TAILQ_FOREACH(flow, &sa->flow_list, entries) {
		rc = sfc_flow_insert(sa, flow, NULL);
		if (rc != 0)
			goto fail_bad_flow;
	}

	sfc_log_init(sa, "done");

fail_bad_flow:
	return rc;
}
