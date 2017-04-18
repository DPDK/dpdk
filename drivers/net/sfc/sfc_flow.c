/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2017 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_tailq.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_eth_ctrl.h>
#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_rx.h"
#include "sfc_filter.h"
#include "sfc_flow.h"
#include "sfc_log.h"

/*
 * At now flow API is implemented in such a manner that each
 * flow rule is converted to a hardware filter.
 * All elements of flow rule (attributes, pattern items, actions)
 * correspond to one or more fields in the efx_filter_spec_s structure
 * that is responsible for the hardware filter.
 */

enum sfc_flow_item_layers {
	SFC_FLOW_ITEM_ANY_LAYER,
	SFC_FLOW_ITEM_START_LAYER,
	SFC_FLOW_ITEM_L2,
	SFC_FLOW_ITEM_L3,
	SFC_FLOW_ITEM_L4,
};

typedef int (sfc_flow_item_parse)(const struct rte_flow_item *item,
				  efx_filter_spec_t *spec,
				  struct rte_flow_error *error);

struct sfc_flow_item {
	enum rte_flow_item_type type;		/* Type of item */
	enum sfc_flow_item_layers layer;	/* Layer of item */
	enum sfc_flow_item_layers prev_layer;	/* Previous layer of item */
	sfc_flow_item_parse *parse;		/* Parsing function */
};

static sfc_flow_item_parse sfc_flow_parse_void;
static sfc_flow_item_parse sfc_flow_parse_eth;
static sfc_flow_item_parse sfc_flow_parse_vlan;
static sfc_flow_item_parse sfc_flow_parse_ipv4;
static sfc_flow_item_parse sfc_flow_parse_ipv6;
static sfc_flow_item_parse sfc_flow_parse_tcp;
static sfc_flow_item_parse sfc_flow_parse_udp;

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
static int
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
	uint8_t match;
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

		mask = (const uint8_t *)def_mask;
	} else {
		mask = (const uint8_t *)item->mask;
	}

	spec = (const uint8_t *)item->spec;
	last = (const uint8_t *)item->last;

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

	/* Check that mask and spec not asks for more match than supp_mask */
	for (i = 0; i < size; i++) {
		match = spec[i] | mask[i];
		supp = ((const uint8_t *)supp_mask)[i];

		if ((match | supp) != supp) {
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
		    __rte_unused efx_filter_spec_t *efx_spec,
		    __rte_unused struct rte_flow_error *error)
{
	return 0;
}

/**
 * Convert Ethernet item to EFX filter specification.
 *
 * @param item[in]
 *   Item specification. Only source and destination addresses and
 *   Ethernet type fields are supported. In addition to full and
 *   empty masks of destination address, individual/group mask is
 *   also supported. If the mask is NULL, default mask will be used.
 *   Ranging is not supported.
 * @param efx_spec[in, out]
 *   EFX filter specification to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 */
static int
sfc_flow_parse_eth(const struct rte_flow_item *item,
		   efx_filter_spec_t *efx_spec,
		   struct rte_flow_error *error)
{
	int rc;
	const struct rte_flow_item_eth *spec = NULL;
	const struct rte_flow_item_eth *mask = NULL;
	const struct rte_flow_item_eth supp_mask = {
		.dst.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
		.src.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
		.type = 0xffff,
	};
	const uint8_t ig_mask[EFX_MAC_ADDR_LEN] = {
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	rc = sfc_flow_parse_init(item,
				 (const void **)&spec,
				 (const void **)&mask,
				 &supp_mask,
				 &rte_flow_item_eth_mask,
				 sizeof(struct rte_flow_item_eth),
				 error);
	if (rc != 0)
		return rc;

	/* If "spec" is not set, could be any Ethernet */
	if (spec == NULL)
		return 0;

	if (is_same_ether_addr(&mask->dst, &supp_mask.dst)) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_LOC_MAC;
		rte_memcpy(efx_spec->efs_loc_mac, spec->dst.addr_bytes,
			   EFX_MAC_ADDR_LEN);
	} else if (memcmp(mask->dst.addr_bytes, ig_mask,
			  EFX_MAC_ADDR_LEN) == 0) {
		if (is_unicast_ether_addr(&spec->dst))
			efx_spec->efs_match_flags |=
				EFX_FILTER_MATCH_UNKNOWN_UCAST_DST;
		else
			efx_spec->efs_match_flags |=
				EFX_FILTER_MATCH_UNKNOWN_MCAST_DST;
	} else if (!is_zero_ether_addr(&mask->dst)) {
		goto fail_bad_mask;
	}

	if (is_same_ether_addr(&mask->src, &supp_mask.src)) {
		efx_spec->efs_match_flags |= EFX_FILTER_MATCH_REM_MAC;
		rte_memcpy(efx_spec->efs_rem_mac, spec->src.addr_bytes,
			   EFX_MAC_ADDR_LEN);
	} else if (!is_zero_ether_addr(&mask->src)) {
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
		    efx_filter_spec_t *efx_spec,
		    struct rte_flow_error *error)
{
	int rc;
	uint16_t vid;
	const struct rte_flow_item_vlan *spec = NULL;
	const struct rte_flow_item_vlan *mask = NULL;
	const struct rte_flow_item_vlan supp_mask = {
		.tci = rte_cpu_to_be_16(ETH_VLAN_ID_MAX),
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
		vid = rte_bswap16(spec->tci);

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
		    efx_filter_spec_t *efx_spec,
		    struct rte_flow_error *error)
{
	int rc;
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
		    efx_filter_spec_t *efx_spec,
		    struct rte_flow_error *error)
{
	int rc;
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
		   efx_filter_spec_t *efx_spec,
		   struct rte_flow_error *error)
{
	int rc;
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
		   efx_filter_spec_t *efx_spec,
		   struct rte_flow_error *error)
{
	int rc;
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

static const struct sfc_flow_item sfc_flow_items[] = {
	{
		.type = RTE_FLOW_ITEM_TYPE_VOID,
		.prev_layer = SFC_FLOW_ITEM_ANY_LAYER,
		.layer = SFC_FLOW_ITEM_ANY_LAYER,
		.parse = sfc_flow_parse_void,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.prev_layer = SFC_FLOW_ITEM_START_LAYER,
		.layer = SFC_FLOW_ITEM_L2,
		.parse = sfc_flow_parse_eth,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_VLAN,
		.prev_layer = SFC_FLOW_ITEM_L2,
		.layer = SFC_FLOW_ITEM_L2,
		.parse = sfc_flow_parse_vlan,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_IPV4,
		.prev_layer = SFC_FLOW_ITEM_L2,
		.layer = SFC_FLOW_ITEM_L3,
		.parse = sfc_flow_parse_ipv4,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_IPV6,
		.prev_layer = SFC_FLOW_ITEM_L2,
		.layer = SFC_FLOW_ITEM_L3,
		.parse = sfc_flow_parse_ipv6,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_TCP,
		.prev_layer = SFC_FLOW_ITEM_L3,
		.layer = SFC_FLOW_ITEM_L4,
		.parse = sfc_flow_parse_tcp,
	},
	{
		.type = RTE_FLOW_ITEM_TYPE_UDP,
		.prev_layer = SFC_FLOW_ITEM_L3,
		.layer = SFC_FLOW_ITEM_L4,
		.parse = sfc_flow_parse_udp,
	},
};

/*
 * Protocol-independent flow API support
 */
static int
sfc_flow_parse_attr(const struct rte_flow_attr *attr,
		    struct rte_flow *flow,
		    struct rte_flow_error *error)
{
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
	if (attr->priority != 0) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY, attr,
				   "Priorities are not supported");
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
				   "Only ingress is supported");
		return -rte_errno;
	}

	flow->spec.efs_flags |= EFX_FILTER_FLAG_RX;
	flow->spec.efs_rss_context = EFX_FILTER_SPEC_RSS_CONTEXT_DEFAULT;

	return 0;
}

/* Get item from array sfc_flow_items */
static const struct sfc_flow_item *
sfc_flow_get_item(enum rte_flow_item_type type)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(sfc_flow_items); i++)
		if (sfc_flow_items[i].type == type)
			return &sfc_flow_items[i];

	return NULL;
}

static int
sfc_flow_parse_pattern(const struct rte_flow_item pattern[],
		       struct rte_flow *flow,
		       struct rte_flow_error *error)
{
	int rc;
	unsigned int prev_layer = SFC_FLOW_ITEM_ANY_LAYER;
	const struct sfc_flow_item *item;

	if (pattern == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_NUM, NULL,
				   "NULL pattern");
		return -rte_errno;
	}

	for (; pattern->type != RTE_FLOW_ITEM_TYPE_END; pattern++) {
		item = sfc_flow_get_item(pattern->type);
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

		rc = item->parse(pattern, &flow->spec, error);
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
	struct sfc_rxq *rxq;

	if (queue->index >= sa->rxq_count)
		return -EINVAL;

	rxq = sa->rxq_info[queue->index].rxq;
	flow->spec.efs_dmaq_id = (uint16_t)rxq->hw_index;

	return 0;
}

static int
sfc_flow_parse_actions(struct sfc_adapter *sa,
		       const struct rte_flow_action actions[],
		       struct rte_flow *flow,
		       struct rte_flow_error *error)
{
	int rc;
	boolean_t is_specified = B_FALSE;

	if (actions == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "NULL actions");
		return -rte_errno;
	}

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;

		case RTE_FLOW_ACTION_TYPE_QUEUE:
			rc = sfc_flow_parse_queue(sa, actions->conf, flow);
			if (rc != 0) {
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, actions,
					"Bad QUEUE action");
				return -rte_errno;
			}

			is_specified = B_TRUE;
			break;

		default:
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ACTION, actions,
					   "Action is not supported");
			return -rte_errno;
		}
	}

	if (!is_specified) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM, actions,
				   "Action is unspecified");
		return -rte_errno;
	}

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
	struct sfc_adapter *sa = dev->data->dev_private;
	int rc;

	memset(&flow->spec, 0, sizeof(flow->spec));

	rc = sfc_flow_parse_attr(attr, flow, error);
	if (rc != 0)
		goto fail_bad_value;

	rc = sfc_flow_parse_pattern(pattern, flow, error);
	if (rc != 0)
		goto fail_bad_value;

	rc = sfc_flow_parse_actions(sa, actions, flow, error);
	if (rc != 0)
		goto fail_bad_value;

	if (!sfc_filter_is_match_supported(sa, flow->spec.efs_match_flags)) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Flow rule pattern is not supported");
		return -rte_errno;
	}

fail_bad_value:
	return rc;
}

static int
sfc_flow_validate(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	struct rte_flow flow;

	return sfc_flow_parse(dev, attr, pattern, actions, &flow, error);
}

static struct rte_flow *
sfc_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct rte_flow *flow = NULL;
	int rc;

	flow = rte_zmalloc("sfc_rte_flow", sizeof(*flow), 0);
	if (flow == NULL) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to allocate memory");
		goto fail_no_mem;
	}

	rc = sfc_flow_parse(dev, attr, pattern, actions, flow, error);
	if (rc != 0)
		goto fail_bad_value;

	TAILQ_INSERT_TAIL(&sa->filter.flow_list, flow, entries);

	sfc_adapter_lock(sa);

	if (sa->state == SFC_ADAPTER_STARTED) {
		rc = efx_filter_insert(sa->nic, &flow->spec);
		if (rc != 0) {
			rte_flow_error_set(error, rc,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Failed to insert filter");
			goto fail_filter_insert;
		}
	}

	sfc_adapter_unlock(sa);

	return flow;

fail_filter_insert:
	TAILQ_REMOVE(&sa->filter.flow_list, flow, entries);

fail_bad_value:
	rte_free(flow);
	sfc_adapter_unlock(sa);

fail_no_mem:
	return NULL;
}

static int
sfc_flow_remove(struct sfc_adapter *sa,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	int rc = 0;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	if (sa->state == SFC_ADAPTER_STARTED) {
		rc = efx_filter_remove(sa->nic, &flow->spec);
		if (rc != 0)
			rte_flow_error_set(error, rc,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Failed to destroy flow rule");
	}

	TAILQ_REMOVE(&sa->filter.flow_list, flow, entries);
	rte_free(flow);

	return rc;
}

static int
sfc_flow_destroy(struct rte_eth_dev *dev,
		 struct rte_flow *flow,
		 struct rte_flow_error *error)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct rte_flow *flow_ptr;
	int rc = EINVAL;

	sfc_adapter_lock(sa);

	TAILQ_FOREACH(flow_ptr, &sa->filter.flow_list, entries) {
		if (flow_ptr == flow)
			rc = 0;
	}
	if (rc != 0) {
		rte_flow_error_set(error, rc,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to find flow rule to destroy");
		goto fail_bad_value;
	}

	rc = sfc_flow_remove(sa, flow, error);

fail_bad_value:
	sfc_adapter_unlock(sa);

	return -rc;
}

static int
sfc_flow_flush(struct rte_eth_dev *dev,
	       struct rte_flow_error *error)
{
	struct sfc_adapter *sa = dev->data->dev_private;
	struct rte_flow *flow;
	int rc = 0;
	int ret = 0;

	sfc_adapter_lock(sa);

	while ((flow = TAILQ_FIRST(&sa->filter.flow_list)) != NULL) {
		rc = sfc_flow_remove(sa, flow, error);
		if (rc != 0)
			ret = rc;
	}

	sfc_adapter_unlock(sa);

	return -ret;
}

const struct rte_flow_ops sfc_flow_ops = {
	.validate = sfc_flow_validate,
	.create = sfc_flow_create,
	.destroy = sfc_flow_destroy,
	.flush = sfc_flow_flush,
	.query = NULL,
};

void
sfc_flow_init(struct sfc_adapter *sa)
{
	SFC_ASSERT(sfc_adapter_is_locked(sa));

	TAILQ_INIT(&sa->filter.flow_list);
}

void
sfc_flow_fini(struct sfc_adapter *sa)
{
	struct rte_flow *flow;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	while ((flow = TAILQ_FIRST(&sa->filter.flow_list)) != NULL) {
		TAILQ_REMOVE(&sa->filter.flow_list, flow, entries);
		rte_free(flow);
	}
}

void
sfc_flow_stop(struct sfc_adapter *sa)
{
	struct rte_flow *flow;

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	TAILQ_FOREACH(flow, &sa->filter.flow_list, entries)
		efx_filter_remove(sa->nic, &flow->spec);
}

int
sfc_flow_start(struct sfc_adapter *sa)
{
	struct rte_flow *flow;
	int rc = 0;

	sfc_log_init(sa, "entry");

	SFC_ASSERT(sfc_adapter_is_locked(sa));

	TAILQ_FOREACH(flow, &sa->filter.flow_list, entries) {
		rc = efx_filter_insert(sa->nic, &flow->spec);
		if (rc != 0)
			goto fail_bad_flow;
	}

	sfc_log_init(sa, "done");

fail_bad_flow:
	return rc;
}
