/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#include "i40e_ethdev.h"
#include "i40e_flow.h"

#include <rte_bitmap.h>
#include <rte_malloc.h>
#include <rte_hash_crc.h>

#include "../common/flow_engine.h"
#include "../common/flow_check.h"
#include "../common/flow_util.h"

struct i40e_fdir_ctx {
	struct ci_flow_engine_ctx base;
	struct i40e_fdir_filter fdir_filter;
	enum rte_flow_item_type custom_pctype;
	struct flex_item {
		size_t size;
		size_t offset;
	} flex_data[I40E_MAX_FLXPLD_FIED];
};

struct i40e_flow_engine_fdir_flow {
	struct rte_flow base;
	struct i40e_fdir_filter fdir_filter;
	bool installed;
};

struct i40e_fdir_flow_pool_entry {
	struct i40e_flow_engine_fdir_flow flow;
	uint32_t idx;
};

struct i40e_fdir_engine_priv {
	struct i40e_fdir_flow_pool_entry *pool;
	struct rte_bitmap *bitmap;
	struct rte_hash *hash_table;
};

#define I40E_FDIR_FLOW_ENTRY(flow_ptr) \
	container_of((flow_ptr), struct i40e_fdir_flow_pool_entry, flow)

/**
 * FDIR graph implementation (non-tunnel)
 * Pattern: START -> ETH -> [VLAN] -> (IPv4 | IPv6) -> [TCP | UDP | SCTP | ESP | L2TPv3 | GTP] -> END
 * With RAW flexible payload support:
 *   - L2: ETH/VLAN -> RAW -> RAW -> RAW -> END
 *   - L3: IPv4/IPv6 -> RAW -> RAW -> RAW -> END
 *   - L4: TCP/UDP/SCTP -> RAW -> RAW -> RAW -> END
 * GTP tunnel support:
 *   - IPv4/IPv6 -> UDP -> GTP -> END (GTP-C, GTP-U outer)
 *   - IPv4/IPv6 -> UDP -> GTP -> IPv4/IPv6 -> END (GTP-U with inner IP)
 */

enum i40e_fdir_node_id {
	I40E_FDIR_NODE_START = FLOW_GRAPH_NODE_FIRST,
	I40E_FDIR_NODE_ETH,
	I40E_FDIR_NODE_VLAN,
	I40E_FDIR_NODE_IPV4,
	I40E_FDIR_NODE_IPV6,
	I40E_FDIR_NODE_TCP,
	I40E_FDIR_NODE_UDP,
	I40E_FDIR_NODE_SCTP,
	I40E_FDIR_NODE_ESP,
	I40E_FDIR_NODE_L2TPV3OIP,
	I40E_FDIR_NODE_GTPC,
	I40E_FDIR_NODE_GTPU,
	I40E_FDIR_NODE_INNER_IPV4,
	I40E_FDIR_NODE_INNER_IPV6,
	I40E_FDIR_NODE_RAW,
	I40E_FDIR_NODE_END,
	I40E_FDIR_NODE_MAX,
};

static int
i40e_fdir_node_eth_validate(const void *ctx __rte_unused, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *eth_spec = item->spec;
	const struct rte_flow_item_eth *eth_mask = ci_flow_item_mask(item);
	bool no_src_mac, no_dst_mac, src_mac, dst_mac, no_etype;

	/* may be empty */
	if (eth_mask == NULL)
		return 0;

	/* source and destination masks may be all zero or all one */
	no_src_mac = CI_FIELD_IS_ZERO(&eth_mask->hdr.src_addr);
	no_dst_mac = CI_FIELD_IS_ZERO(&eth_mask->hdr.dst_addr);
	no_etype = CI_FIELD_IS_ZERO(&eth_mask->hdr.ether_type);
	src_mac = CI_FIELD_IS_MASKED(&eth_mask->hdr.src_addr);
	dst_mac = CI_FIELD_IS_MASKED(&eth_mask->hdr.dst_addr);

	/* can't be all zero */
	if (no_src_mac && no_dst_mac && no_etype) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item, "Invalid eth mask");
	}
	/* can't be neither zero nor ones */
	if ((!no_src_mac && !src_mac) ||
	    (!no_dst_mac && !dst_mac)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item, "Invalid eth mask");
	}

	/* ethertype can either be unmasked or fully masked */
	if (no_etype)
		return 0;

	if (!CI_FIELD_IS_MASKED(&eth_mask->hdr.ether_type)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item, "Invalid ethertype mask");
	}

	/* Check for valid ethertype (not IPv4/IPv6) */
	uint16_t ether_type = rte_be_to_cpu_16(eth_spec->hdr.ether_type);
	if (ether_type == RTE_ETHER_TYPE_IPV4 ||
	    ether_type == RTE_ETHER_TYPE_IPV6) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"IPv4/IPv6 not supported by ethertype filter");
	}

	return 0;
}

static int
i40e_fdir_node_eth_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	const struct rte_flow_item_eth *eth_spec = item->spec;
	const struct rte_flow_item_eth *eth_mask = ci_flow_item_mask(item);
	uint16_t tpid, ether_type;
	uint64_t input_set = 0;
	int ret;

	/* Set layer index for L2 flexible payload (after ETH/VLAN) */
	fdir_filter->input.flow_ext.layer_idx = I40E_FLXPLD_L2_IDX;

	/* set packet type */
	fdir_filter->input.pctype = I40E_FILTER_PCTYPE_L2_PAYLOAD;

	/* do we need to set up MAC addresses? */
	if (eth_mask == NULL)
		return 0;

	/* do we care for source address? */
	if (CI_FIELD_IS_MASKED(&eth_mask->hdr.src_addr)) {
		fdir_filter->input.flow.l2_flow.src = eth_spec->hdr.src_addr;
		input_set |= I40E_INSET_SMAC;
	}
	/* do we care for destination address? */
	if (CI_FIELD_IS_MASKED(&eth_mask->hdr.dst_addr)) {
		fdir_filter->input.flow.l2_flow.dst = eth_spec->hdr.dst_addr;
		input_set |= I40E_INSET_DMAC;
	}

	/* do we care for ethertype? */
	if (eth_mask->hdr.ether_type) {
		struct i40e_pf *pf =
				I40E_DEV_PRIVATE_TO_PF(fdir_ctx->base.dev_data->dev_private);

		ether_type = rte_be_to_cpu_16(eth_spec->hdr.ether_type);
		ret = i40e_get_outer_vlan(pf, &tpid);
		if (ret != 0) {
			return rte_flow_error_set(error, EIO,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Can not get the Ethertype identifying the L2 tag");
		}
		if (ether_type == tpid) {
			return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM, item,
						"Unsupported ether_type in control packet filter.");
		}
		fdir_filter->input.flow.l2_flow.ether_type = eth_spec->hdr.ether_type;
		input_set |= I40E_INSET_LAST_ETHER_TYPE;
	}

	fdir_filter->input.flow_ext.input_set = input_set;

	return 0;
}

static int
i40e_fdir_node_vlan_validate(const void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_vlan *vlan_spec = item->spec;
	const struct rte_flow_item_vlan *vlan_mask = ci_flow_item_mask(item);
	const struct i40e_fdir_ctx *fdir_ctx = ctx;
	const struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	uint16_t ether_type;

	if (vlan_mask == NULL)
		return 0;

	/* TCI mask can be either fully disabled or fully enabled. */
	if (vlan_mask->hdr.vlan_tci != 0 &&
	    vlan_mask->hdr.vlan_tci != rte_cpu_to_be_16(I40E_VLAN_TCI_MASK)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Unsupported TCI mask");
	}
	if (CI_FIELD_IS_ZERO(&vlan_mask->hdr.eth_proto))
		return 0;

	if (!CI_FIELD_IS_MASKED(&vlan_mask->hdr.eth_proto)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid VLAN header mask");
	}

	/* can't match on eth_proto as we're already matching on ethertype */
	if (fdir_filter->input.flow_ext.input_set & I40E_INSET_LAST_ETHER_TYPE) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Cannot set two ethertype filters");
	}

	ether_type = rte_be_to_cpu_16(vlan_spec->hdr.eth_proto);
	if (ether_type == RTE_ETHER_TYPE_IPV4 ||
	    ether_type == RTE_ETHER_TYPE_IPV6) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"IPv4/IPv6 not supported by VLAN protocol filter");
	}

	return 0;
}

static int
i40e_fdir_node_vlan_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_vlan *vlan_spec = item->spec;
	const struct rte_flow_item_vlan *vlan_mask = ci_flow_item_mask(item);
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;

	/* Set layer index for L2 flexible payload (after ETH/VLAN) */
	fdir_filter->input.flow_ext.layer_idx = I40E_FLXPLD_L2_IDX;

	/* set packet type */
	fdir_filter->input.pctype = I40E_FILTER_PCTYPE_L2_PAYLOAD;

	if (vlan_mask == NULL)
		return 0;

	/* Store TCI value if requested */
	if (vlan_mask->hdr.vlan_tci) {
		fdir_filter->input.flow_ext.vlan_tci = vlan_spec->hdr.vlan_tci;
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_VLAN_INNER;
	}

	/* if ethertype specified, store it */
	if (vlan_mask->hdr.eth_proto) {
		struct i40e_pf *pf =
				I40E_DEV_PRIVATE_TO_PF(fdir_ctx->base.dev_data->dev_private);
		uint16_t tpid, ether_type;
		int ret;

		ether_type = rte_be_to_cpu_16(vlan_spec->hdr.eth_proto);

		ret = i40e_get_outer_vlan(pf, &tpid);
		if (ret != 0) {
			return rte_flow_error_set(error, EIO,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Can not get the Ethertype identifying the L2 tag");
		}
		if (ether_type == tpid) {
			return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM, item,
						"Unsupported ether_type in control packet filter.");
		}
		fdir_filter->input.flow.l2_flow.ether_type = vlan_spec->hdr.eth_proto;
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_LAST_ETHER_TYPE;
	}

	return 0;
}

static int
i40e_fdir_node_ipv4_validate(const void *ctx __rte_unused, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *ipv4_spec = item->spec;
	const struct rte_flow_item_ipv4 *ipv4_mask = ci_flow_item_mask(item);
	const struct rte_flow_item_ipv4 *ipv4_last = item->last;

	if (ipv4_mask == NULL)
		return 0;

	/* Validate mask fields */
	if (ipv4_mask->hdr.version_ihl ||
			ipv4_mask->hdr.total_length ||
			ipv4_mask->hdr.packet_id ||
			ipv4_mask->hdr.hdr_checksum) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid IPv4 header mask");
	}
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&ipv4_mask->hdr.src_addr) ||
			!CI_FIELD_IS_ZERO_OR_MASKED(&ipv4_mask->hdr.dst_addr) ||
			!CI_FIELD_IS_ZERO_OR_MASKED(&ipv4_mask->hdr.type_of_service) ||
			!CI_FIELD_IS_ZERO_OR_MASKED(&ipv4_mask->hdr.time_to_live) ||
			!CI_FIELD_IS_ZERO_OR_MASKED(&ipv4_mask->hdr.next_proto_id)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid IPv4 header mask");
	}

	if (ipv4_last == NULL)
		return 0;

	/* Only fragment_offset supports range */
	if (ipv4_last->hdr.version_ihl ||
	    ipv4_last->hdr.type_of_service ||
	    ipv4_last->hdr.total_length ||
	    ipv4_last->hdr.packet_id ||
	    ipv4_last->hdr.time_to_live ||
	    ipv4_last->hdr.next_proto_id ||
	    ipv4_last->hdr.hdr_checksum ||
	    ipv4_last->hdr.src_addr ||
	    ipv4_last->hdr.dst_addr) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"IPv4 range only supported for fragment_offset");
	}

	/* Validate fragment_offset range values */
	uint16_t frag_mask = rte_be_to_cpu_16(ipv4_mask->hdr.fragment_offset);
	uint16_t frag_spec = rte_be_to_cpu_16(ipv4_spec->hdr.fragment_offset);
	uint16_t frag_last = rte_be_to_cpu_16(ipv4_last->hdr.fragment_offset);

	/* Mask must be 0x3fff (fragment offset + MF flag) */
	if (frag_mask != (RTE_IPV4_HDR_OFFSET_MASK | RTE_IPV4_HDR_MF_FLAG)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid IPv4 fragment_offset mask");
	}

	/* Only allow: frag rule (spec=0x8, last=0x2000) or non-frag (spec=0, last=0) */
	if (frag_spec == (1 << RTE_IPV4_HDR_FO_SHIFT) &&
	    frag_last == RTE_IPV4_HDR_MF_FLAG)
		return 0; /* Fragment rule */

	if (frag_spec == 0 && frag_last == 0)
		return 0; /* Non-fragment rule */

	return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Invalid IPv4 fragment_offset rule");
}

static int
i40e_fdir_node_ipv4_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	const struct rte_flow_item_ipv4 *ipv4_spec = item->spec;
	const struct rte_flow_item_ipv4 *ipv4_mask = ci_flow_item_mask(item);
	const struct rte_flow_item_ipv4 *ipv4_last = item->last;
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	uint16_t frag_spec, frag_last;

	/* Set layer index for L2 flexible payload (after ETH/VLAN) */
	fdir_filter->input.flow_ext.layer_idx = I40E_FLXPLD_L3_IDX;

	/* set packet type */
	fdir_filter->input.pctype = I40E_FILTER_PCTYPE_NONF_IPV4_OTHER;

	/* set up flow type */
	fdir_filter->input.flow_ext.inner_ip = false;
	fdir_filter->input.flow_ext.oip_type = I40E_FDIR_IPTYPE_IPV4;

	if (ipv4_mask == NULL)
		return 0;

	/* Mark that IPv4 fields are used */
	if (!CI_FIELD_IS_ZERO(&ipv4_mask->hdr.next_proto_id)) {
		fdir_filter->input.flow.ip4_flow.proto = ipv4_spec->hdr.next_proto_id;
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_IPV4_PROTO;
	}
	if (!CI_FIELD_IS_ZERO(&ipv4_mask->hdr.type_of_service)) {
		fdir_filter->input.flow.ip4_flow.tos = ipv4_spec->hdr.type_of_service;
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_IPV4_TOS;
	}
	if (!CI_FIELD_IS_ZERO(&ipv4_mask->hdr.time_to_live)) {
		fdir_filter->input.flow.ip4_flow.ttl = ipv4_spec->hdr.time_to_live;
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_IPV4_TTL;
	}
	if (!CI_FIELD_IS_ZERO(&ipv4_mask->hdr.src_addr)) {
		fdir_filter->input.flow.ip4_flow.src_ip = ipv4_spec->hdr.src_addr;
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_IPV4_SRC;
	}
	if (!CI_FIELD_IS_ZERO(&ipv4_mask->hdr.dst_addr)) {
		fdir_filter->input.flow.ip4_flow.dst_ip = ipv4_spec->hdr.dst_addr;
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_IPV4_DST;
	}

	/* do we have range? */
	if (ipv4_last == NULL)
		return 0;

	/* frag mask is already known to be non-zero */
	frag_spec = rte_be_to_cpu_16(ipv4_spec->hdr.fragment_offset);
	frag_last = rte_be_to_cpu_16(ipv4_last->hdr.fragment_offset);
	/* frag spec and last are already known to be either 0 or valid */

	/* is range specified for fragment_offset? */
	if (frag_spec != 0 && frag_last != 0)
		fdir_filter->input.pctype = I40E_FILTER_PCTYPE_FRAG_IPV4;

	return 0;
}

static int
i40e_fdir_node_ipv6_validate(const void *ctx __rte_unused, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv6 *ipv6_mask = ci_flow_item_mask(item);

	if (ipv6_mask == NULL)
		return 0;

	/* payload len isn't supported */
	if (ipv6_mask->hdr.payload_len) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid IPv6 header mask");
	}
	/* source and destination mask can either be all zeroes or all ones */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&ipv6_mask->hdr.src_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid IPv6 source address mask");
	}
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&ipv6_mask->hdr.dst_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid IPv6 destination address mask");
	}

	/* check other supported fields */
	if (!ci_is_zero_or_masked(ipv6_mask->hdr.vtc_flow, rte_cpu_to_be_32(I40E_IPV6_TC_MASK)) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&ipv6_mask->hdr.proto) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&ipv6_mask->hdr.hop_limits)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid IPv6 header mask");
	}

	return 0;
}

static int
i40e_fdir_node_ipv6_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	const struct rte_flow_item_ipv6 *ipv6_spec = item->spec;
	const struct rte_flow_item_ipv6 *ipv6_mask = ci_flow_item_mask(item);
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;

	/* Set layer index for L2 flexible payload (after ETH/VLAN) */
	fdir_filter->input.flow_ext.layer_idx = I40E_FLXPLD_L3_IDX;

	/* set packet type */
	fdir_filter->input.pctype = I40E_FILTER_PCTYPE_NONF_IPV6_OTHER;

	/* set up flow type */
	fdir_filter->input.flow_ext.inner_ip = false;
	fdir_filter->input.flow_ext.oip_type = I40E_FDIR_IPTYPE_IPV6;

	if (ipv6_mask == NULL)
		return 0;

	if (CI_FIELD_IS_MASKED(&ipv6_mask->hdr.src_addr)) {
		memcpy(&fdir_filter->input.flow.ipv6_flow.src_ip, &ipv6_spec->hdr.src_addr,
				sizeof(ipv6_spec->hdr.src_addr));
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_IPV6_SRC;
	}
	if (CI_FIELD_IS_MASKED(&ipv6_mask->hdr.dst_addr)) {
		memcpy(&fdir_filter->input.flow.ipv6_flow.dst_ip, &ipv6_spec->hdr.dst_addr,
				sizeof(ipv6_spec->hdr.dst_addr));
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_IPV6_DST;
	}

	if (!CI_FIELD_IS_ZERO(&ipv6_mask->hdr.vtc_flow)) {
		rte_be32_t vtc_flow = rte_be_to_cpu_32(ipv6_spec->hdr.vtc_flow);
		uint8_t tc = (uint8_t)((vtc_flow & I40E_IPV6_TC_MASK) >> I40E_FDIR_IPv6_TC_OFFSET);
		fdir_filter->input.flow.ipv6_flow.tc = tc;
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_IPV6_TC;
	}
	if (!CI_FIELD_IS_ZERO(&ipv6_mask->hdr.proto)) {
		fdir_filter->input.flow.ipv6_flow.proto = ipv6_spec->hdr.proto;
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_IPV6_NEXT_HDR;
	}
	if (!CI_FIELD_IS_ZERO(&ipv6_mask->hdr.hop_limits)) {
		fdir_filter->input.flow.ipv6_flow.hop_limits = ipv6_spec->hdr.hop_limits;
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_IPV6_HOP_LIMIT;
	}
	/* mark as fragment traffic if necessary */
	if (ipv6_spec->hdr.proto == I40E_IPV6_FRAG_HEADER)
		fdir_filter->input.pctype = I40E_FILTER_PCTYPE_FRAG_IPV6;


	return 0;
}

static int
i40e_fdir_node_tcp_validate(const void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct i40e_fdir_ctx *fdir_ctx = ctx;
	const struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	const struct rte_flow_item_tcp *tcp_mask = ci_flow_item_mask(item);

	/* cannot match both fragmented and TCP */
	if (fdir_filter->input.pctype == I40E_FILTER_PCTYPE_FRAG_IPV4 ||
	    fdir_filter->input.pctype == I40E_FILTER_PCTYPE_FRAG_IPV6) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Cannot combine fragmented IP and TCP match");
	}

	if (tcp_mask == NULL)
		return 0;

	if (tcp_mask->hdr.sent_seq ||
	    tcp_mask->hdr.recv_ack ||
	    tcp_mask->hdr.data_off ||
	    tcp_mask->hdr.tcp_flags ||
	    tcp_mask->hdr.rx_win ||
	    tcp_mask->hdr.cksum ||
	    tcp_mask->hdr.tcp_urp) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid TCP header mask");
	}
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&tcp_mask->hdr.src_port) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&tcp_mask->hdr.dst_port)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid TCP header mask");
	}
	return 0;
}

static int
i40e_fdir_node_tcp_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	const struct rte_flow_item_tcp *tcp_spec = item->spec;
	const struct rte_flow_item_tcp *tcp_mask = ci_flow_item_mask(item);
	rte_be16_t src_spec, dst_spec, src_mask, dst_mask;
	bool is_ipv4;

	/* Set layer index for L4 flexible payload */
	fdir_filter->input.flow_ext.layer_idx = I40E_FLXPLD_L4_IDX;

	/* set packet type depending on L3 type */
	if (fdir_filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV4)
		fdir_filter->input.pctype = I40E_FILTER_PCTYPE_NONF_IPV4_TCP;
	else if (fdir_filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV6)
		fdir_filter->input.pctype = I40E_FILTER_PCTYPE_NONF_IPV6_TCP;

	if (tcp_mask == NULL)
		return 0;

	src_spec = tcp_spec->hdr.src_port;
	dst_spec = tcp_spec->hdr.dst_port;
	src_mask = tcp_mask->hdr.src_port;
	dst_mask = tcp_mask->hdr.dst_port;
	is_ipv4 = fdir_filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV4;

	if (is_ipv4) {
		if (src_mask != 0) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_SRC_PORT;
			fdir_filter->input.flow.tcp4_flow.src_port = src_spec;
		}
		if (dst_mask) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_DST_PORT;
			fdir_filter->input.flow.tcp4_flow.dst_port = dst_spec;
		}
	} else {
		if (src_mask != 0) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_SRC_PORT;
			fdir_filter->input.flow.tcp6_flow.src_port = src_spec;
		}
		if (dst_mask) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_DST_PORT;
			fdir_filter->input.flow.tcp6_flow.dst_port = dst_spec;
		}
	}

	return 0;
}

static int
i40e_fdir_node_udp_validate(const void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct i40e_fdir_ctx *fdir_ctx = ctx;
	const struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	const struct rte_flow_item_udp *udp_mask = ci_flow_item_mask(item);

	/* cannot match both fragmented and TCP */
	if (fdir_filter->input.pctype == I40E_FILTER_PCTYPE_FRAG_IPV4 ||
	    fdir_filter->input.pctype == I40E_FILTER_PCTYPE_FRAG_IPV6) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Cannot combine fragmented IP and UDP match");
	}

	if (udp_mask == NULL)
		return 0;

	if (udp_mask->hdr.dgram_len || udp_mask->hdr.dgram_cksum) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid UDP header mask");
	}
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&udp_mask->hdr.src_port) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&udp_mask->hdr.dst_port)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid UDP header mask");
	}
	return 0;
}

static int
i40e_fdir_node_udp_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	const struct rte_flow_item_udp *udp_spec = item->spec;
	const struct rte_flow_item_udp *udp_mask = ci_flow_item_mask(item);
	rte_be16_t src_spec, dst_spec, src_mask, dst_mask;
	bool is_ipv4;

	/* Set layer index for L4 flexible payload */
	fdir_filter->input.flow_ext.layer_idx = I40E_FLXPLD_L4_IDX;

	/* set packet type depending on L3 type */
	if (fdir_filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV4)
		fdir_filter->input.pctype = I40E_FILTER_PCTYPE_NONF_IPV4_UDP;
	else if (fdir_filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV6)
		fdir_filter->input.pctype = I40E_FILTER_PCTYPE_NONF_IPV6_UDP;

	/* set UDP */
	fdir_filter->input.flow_ext.is_udp = true;

	if (udp_mask == NULL)
		return 0;

	src_spec = udp_spec->hdr.src_port;
	dst_spec = udp_spec->hdr.dst_port;
	src_mask = udp_mask->hdr.src_port;
	dst_mask = udp_mask->hdr.dst_port;
	is_ipv4 = fdir_filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV4;

	if (is_ipv4) {
		if (src_mask) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_SRC_PORT;
			fdir_filter->input.flow.udp4_flow.src_port = src_spec;
		}
		if (dst_mask) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_DST_PORT;
			fdir_filter->input.flow.udp4_flow.dst_port = dst_spec;
		}
	} else {
		if (src_mask) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_SRC_PORT;
			fdir_filter->input.flow.udp6_flow.src_port = src_spec;
		}
		if (dst_mask) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_DST_PORT;
			fdir_filter->input.flow.udp6_flow.dst_port = dst_spec;
		}
	}

	return 0;
}

static int
i40e_fdir_node_sctp_validate(const void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct i40e_fdir_ctx *fdir_ctx = ctx;
	const struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	const struct rte_flow_item_sctp *sctp_mask = ci_flow_item_mask(item);

	/* cannot match both fragmented and TCP */
	if (fdir_filter->input.pctype == I40E_FILTER_PCTYPE_FRAG_IPV4 ||
	    fdir_filter->input.pctype == I40E_FILTER_PCTYPE_FRAG_IPV6) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Cannot combine fragmented IP and SCTP match");
	}

	if (sctp_mask == NULL)
		return 0;

	if (sctp_mask->hdr.cksum) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid SCTP header mask");
	}
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&sctp_mask->hdr.src_port) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&sctp_mask->hdr.dst_port) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&sctp_mask->hdr.tag)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid SCTP header mask");
	}
	return 0;
}

static int
i40e_fdir_node_sctp_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	const struct rte_flow_item_sctp *sctp_spec = item->spec;
	const struct rte_flow_item_sctp *sctp_mask = ci_flow_item_mask(item);
	rte_be16_t src_spec, dst_spec, src_mask, dst_mask, tag_spec, tag_mask;
	bool is_ipv4;

	/* Set layer index for L4 flexible payload */
	fdir_filter->input.flow_ext.layer_idx = I40E_FLXPLD_L4_IDX;

	/* set packet type depending on L3 type */
	if (fdir_filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV4)
		fdir_filter->input.pctype = I40E_FILTER_PCTYPE_NONF_IPV4_SCTP;
	else if (fdir_filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV6)
		fdir_filter->input.pctype = I40E_FILTER_PCTYPE_NONF_IPV6_SCTP;

	if (sctp_mask == NULL)
		return 0;

	if (!CI_FIELD_IS_ZERO(&sctp_mask->hdr.tag))
		fdir_filter->input.flow_ext.input_set |= I40E_INSET_SCTP_VT;

	src_spec = sctp_spec->hdr.src_port;
	dst_spec = sctp_spec->hdr.dst_port;
	src_mask = sctp_mask->hdr.src_port;
	dst_mask = sctp_mask->hdr.dst_port;
	tag_spec = sctp_spec->hdr.tag;
	tag_mask = sctp_mask->hdr.tag;
	is_ipv4 = fdir_filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV4;

	if (is_ipv4) {
		if (src_mask) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_SRC_PORT;
			fdir_filter->input.flow.sctp4_flow.src_port = src_spec;
		}
		if (dst_mask) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_DST_PORT;
			fdir_filter->input.flow.sctp4_flow.dst_port = dst_spec;
		}
		if (tag_mask) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_SCTP_VT;
			fdir_filter->input.flow.sctp4_flow.verify_tag = tag_spec;
		}
	} else {
		if (src_mask) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_SRC_PORT;
			fdir_filter->input.flow.sctp6_flow.src_port = src_spec;
		}
		if (dst_mask) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_DST_PORT;
			fdir_filter->input.flow.sctp6_flow.dst_port = dst_spec;
		}
		if (tag_mask) {
			fdir_filter->input.flow_ext.input_set |= I40E_INSET_SCTP_VT;
			fdir_filter->input.flow.sctp6_flow.verify_tag = tag_spec;
		}
	}

	return 0;
}

static int
i40e_fdir_node_raw_validate(const void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct i40e_fdir_ctx *fdir_ctx = ctx;
	const struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	const struct rte_flow_item_raw *raw_spec = item->spec;
	const struct rte_flow_item_raw *raw_mask = item->mask;
	uint8_t raw_id = fdir_filter->input.flow_ext.raw_id;
	size_t spec_size, spec_offset;
	size_t total_size, i;
	size_t new_src_offset;
	bool wants_offset, wants_search, wants_limit, wants_absolute;

	if (raw_mask == NULL) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW item requires an explicit mask");
	}

	/* Check max RAW items limit */
	RTE_BUILD_BUG_ON(I40E_MAX_FLXPLD_LAYER != I40E_MAX_FLXPLD_FIED);
	if (raw_id >= I40E_MAX_FLXPLD_LAYER) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Maximum 3 RAW items allowed per layer");
	}

	if (raw_spec->pattern == NULL) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW spec pattern must not be NULL");
	}

	if (raw_mask->pattern == NULL) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW mask pattern must not be NULL");
	}

	if (raw_mask->length != raw_spec->length &&
	    raw_mask->length != 0xffff) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW mask length must match spec length or be 0xffff");
	}

	/* control field masks must be all zero or all one */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&raw_mask->offset) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&raw_mask->limit)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW control field masks must be all zero or all one");
	}
	wants_offset = raw_mask->offset != 0;
	wants_search = raw_mask->search != 0 && raw_spec->search != 0;
	wants_limit = raw_mask->limit != 0 && raw_spec->limit != 0;
	wants_absolute = raw_mask->relative != 0 && raw_spec->relative == 0;

	/* Offset must be 16-bit aligned */
	if (wants_offset && (raw_spec->offset % sizeof(uint16_t) != 0)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW offset must be even");
	}

	/* Offset must be non-negative */
	if (wants_offset && raw_spec->offset < 0) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW offset must be non-negative");
	}

	/* Search and limit not supported */
	if (wants_search || wants_limit) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW search/limit not supported");
	}

	/* only relative offsets are supported */
	if (wants_absolute) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW absolute offset not supported");
	}

	if (raw_spec->reserved != 0) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW reserved field must be zero");
	}

	/* flex size/offset for current item (in bytes) */
	spec_size = raw_spec->length;
	spec_offset = wants_offset ? raw_spec->offset : 0;

	/*
	 * RAW node can be triggered multiple times, each time we will be
	 * copying more data to the flexbyte buffer. we need to validate total
	 * size/offset against max allowed because we cannot overflow our
	 * flexbyte buffer.
	 */

	/* accumulate previous raw items' size/offset */
	total_size = 0;
	new_src_offset = 0;
	for (i = 0; i < raw_id; i++) {
		const struct flex_item *fi = &fdir_ctx->flex_data[i];
		total_size += fi->size;
		/* offset is relative to end of previous item */
		new_src_offset += fi->offset + fi->size;
	}
	/* add current item to totals */
	total_size += spec_size;
	new_src_offset += spec_offset;

	/* validate against max offset/size */
	if (spec_size + new_src_offset >= I40E_MAX_FLX_SOURCE_OFF) {
		return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM, item,
						"RAW total offset exceeds maximum");
	}
	if (total_size > I40E_FDIR_MAX_FLEXLEN) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"RAW total size exceeds maximum");
	}

	return 0;
}

static int
i40e_fdir_node_raw_process(void *ctx,
			    const struct rte_flow_item *item,
			    struct rte_flow_error *error __rte_unused)
{
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	const struct rte_flow_item_raw *raw_spec = item->spec;
	const struct rte_flow_item_raw *raw_mask = item->mask;
	uint8_t raw_id = fdir_filter->input.flow_ext.raw_id;
	enum i40e_flxpld_layer_idx layer_idx = fdir_filter->input.flow_ext.layer_idx;
	struct i40e_fdir_flex_pit *flex_pit;
	size_t spec_size, spec_offset, i;
	size_t total_size, new_src_offset;

	/* flex size for current item */
	spec_size = raw_spec->length;
	spec_offset = raw_mask->offset != 0 ? raw_spec->offset : 0;

	/* accumulate previous raw items' size/offset */
	total_size = 0;
	new_src_offset = 0;
	for (i = 0; i < raw_id; i++) {
		const struct flex_item *fi = &fdir_ctx->flex_data[i];
		total_size += fi->size;
		/* offset is relative to end of previous item */
		new_src_offset += fi->offset + fi->size;
	}
	/* src offset must also include current offset */
	new_src_offset += spec_offset;

	/* store the current data */
	fdir_ctx->flex_data[raw_id].size = spec_size;
	fdir_ctx->flex_data[raw_id].offset = spec_offset;

	/* copy bytes from current spec into the flex pit buffer */
	for (i = 0; i < spec_size; i++) {
		const size_t j = total_size + i;
		fdir_filter->input.flow_ext.flexbytes[j] = raw_spec->pattern[i];
		fdir_filter->input.flow_ext.flex_mask[j] = raw_mask->pattern[i];
	}

	/*
	 * all metadata in the flex pit is stored in units of 2 bytes (words),
	 * but all the limits are in bytes, so we need to convert sizes/offsets
	 * accordingly.
	 */

	/* pick our flex pit */
	flex_pit = &fdir_filter->input.flow_ext.flex_pit[layer_idx][raw_id];
	/* convert to words (2-byte units) */
	flex_pit->src_offset = (uint16_t)new_src_offset / sizeof(uint16_t);
	flex_pit->dst_offset = (uint16_t)total_size / sizeof(uint16_t);
	flex_pit->size = (uint16_t)spec_size / sizeof(uint16_t);

	/* increment raw item index */
	fdir_filter->input.flow_ext.raw_id++;

	/* mark as flex flow */
	fdir_filter->input.flow_ext.is_flex_flow = true;

	return 0;
}

static int
i40e_fdir_node_esp_validate(const void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct i40e_fdir_ctx *fdir_ctx = ctx;
	const struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(fdir_ctx->base.dev_data->dev_private);
	const struct rte_flow_item_esp *esp_mask = ci_flow_item_mask(item);

	if (!pf->esp_support) {
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Protocol not supported");
	}

	/* SPI must be fully masked */
	if (!CI_FIELD_IS_MASKED(&esp_mask->hdr.spi)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid ESP header mask");
	}
	return 0;
}

static int
i40e_fdir_node_esp_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	const struct rte_flow_item_esp *esp_spec = item->spec;
	bool is_ipv4 = fdir_filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV4;
	bool is_udp = fdir_filter->input.flow_ext.is_udp;

	/* ESP uses customized pctype */
	fdir_filter->input.flow_ext.customized_pctype = true;
	fdir_ctx->custom_pctype = item->type;

	if (is_ipv4) {
		if (is_udp)
			fdir_filter->input.flow.esp_ipv4_udp_flow.spi = esp_spec->hdr.spi;
		else
			fdir_filter->input.flow.esp_ipv4_flow.spi = esp_spec->hdr.spi;
	} else {
		if (is_udp)
			fdir_filter->input.flow.esp_ipv6_udp_flow.spi = esp_spec->hdr.spi;
		else
			fdir_filter->input.flow.esp_ipv6_flow.spi = esp_spec->hdr.spi;
	}

	return 0;
}

static int
i40e_fdir_node_l2tpv3oip_validate(const void *ctx __rte_unused,
				   const struct rte_flow_item *item,
				   struct rte_flow_error *error)
{
	const struct rte_flow_item_l2tpv3oip *l2tp_mask = ci_flow_item_mask(item);

	if (!CI_FIELD_IS_MASKED(&l2tp_mask->session_id)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid L2TPv3oIP header mask");
	}
	return 0;
}

static int
i40e_fdir_node_l2tpv3oip_process(void *ctx,
				  const struct rte_flow_item *item,
				  struct rte_flow_error *error __rte_unused)
{
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	const struct rte_flow_item_l2tpv3oip *l2tp_spec = item->spec;

	/* L2TPv3 uses customized pctype */
	fdir_filter->input.flow_ext.customized_pctype = true;
	fdir_ctx->custom_pctype = item->type;

	/* Store session_id in appropriate flow union member based on IP version */
	if (fdir_filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV4)
		fdir_filter->input.flow.ip4_l2tpv3oip_flow.session_id = l2tp_spec->session_id;
	else if (fdir_filter->input.flow_ext.oip_type == I40E_FDIR_IPTYPE_IPV6)
		fdir_filter->input.flow.ip6_l2tpv3oip_flow.session_id = l2tp_spec->session_id;

	return 0;
}

static int
i40e_fdir_node_gtp_validate(const void *ctx __rte_unused, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct i40e_fdir_ctx *fdir_ctx = ctx;
	const struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(fdir_ctx->base.dev_data->dev_private);
	const struct rte_flow_item_gtp *gtp_mask = ci_flow_item_mask(item);

	/* DDP may not support this packet type */
	if (!pf->gtp_support) {
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Protocol not supported");
	}

	if (gtp_mask->hdr.gtp_hdr_info ||
	    gtp_mask->hdr.msg_type ||
	    gtp_mask->hdr.plen) {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Invalid GTP header mask");
	}
	/* if GTP is specified, TEID must be masked */
	if (!CI_FIELD_IS_MASKED(&gtp_mask->hdr.teid)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid GTP header mask");
	}
	return 0;
}

static int
i40e_fdir_node_gtp_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	const struct rte_flow_item_gtp *gtp_spec = item->spec;

	/* Mark as GTP tunnel with customized pctype */
	fdir_filter->input.flow_ext.customized_pctype = true;
	fdir_ctx->custom_pctype = item->type;

	fdir_filter->input.flow.gtp_flow.teid = gtp_spec->teid;

	return 0;
}

static int
i40e_fdir_node_inner_ipv4_process(void *ctx, const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;

	/* Mark as inner IP */
	fdir_filter->input.flow_ext.inner_ip = true;
	fdir_filter->input.flow_ext.iip_type = I40E_FDIR_IPTYPE_IPV4;

	return 0;
}

static int
i40e_fdir_node_inner_ipv6_process(void *ctx, const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;

	/* Mark as inner IP */
	fdir_filter->input.flow_ext.inner_ip = true;
	fdir_filter->input.flow_ext.iip_type = I40E_FDIR_IPTYPE_IPV6;

	return 0;
}

/* END node validation for FDIR - performs pctype determination and input_set validation */
static int
i40e_fdir_node_end_validate(const void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct i40e_fdir_ctx *fdir_ctx = ctx;
	const struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	uint64_t input_set = fdir_filter->input.flow_ext.input_set;
	enum i40e_filter_pctype pctype = fdir_filter->input.pctype;

	/*
	 * Before sending the configuration down to hardware, we need to make
	 * sure that the configuration makes sense - more specifically, that the
	 * input set is a valid one that is actually supported by the hardware.
	 * This is validated for built-in ptypes, however for customized ptypes,
	 * the validation is skipped, and we have no way of validating the input
	 * set because we do not have that information at our disposal - the
	 * input set for customized packet type is not available through DDP
	 * queries.
	 *
	 * However, we do know that some things are unsupported by the hardware
	 * no matter the configuration. We can check for them here.
	 */
	const uint64_t i40e_l2_input_set = I40E_INSET_DMAC | I40E_INSET_SMAC;
	const uint64_t i40e_l3_input_set = (I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
					    I40E_INSET_IPV4_TOS | I40E_INSET_IPV4_TTL |
					    I40E_INSET_IPV4_PROTO);
	const uint64_t i40e_l4_input_set = (I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT);
	const bool l2_in_set = (input_set & i40e_l2_input_set) != 0;
	const bool l3_in_set = (input_set & i40e_l3_input_set) != 0;
	const bool l4_in_set = (input_set & i40e_l4_input_set) != 0;

	/* if we're matching ethertype, we may be matching L2 only, and cannot have RAW patterns */
	if ((input_set & I40E_INSET_LAST_ETHER_TYPE) != 0 &&
	     (pctype != I40E_FILTER_PCTYPE_L2_PAYLOAD ||
	      fdir_filter->input.flow_ext.is_flex_flow)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Cannot match ethertype with L3/L4 or RAW patterns");
	}

	/* L2 and L3 input sets are exclusive */
	if (l2_in_set && l3_in_set) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Matching both L2 and L3 is not supported");
	}
	/* L2 and L4 input sets are exclusive */
	if (l2_in_set && l4_in_set) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Matching both L2 and L4 is not supported");
	}

	/* if we are using one of the builtin packet types, validate it */
	if (!fdir_filter->input.flow_ext.customized_pctype) {
		/* validate the input set for the built-in pctype */
		if (i40e_validate_input_set(pctype, RTE_ETH_FILTER_FDIR, input_set) != 0) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Invalid input set");
		}
	}

	return 0;
}

static int
i40e_fdir_node_end_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	struct i40e_fdir_ctx *fdir_ctx = ctx;
	struct i40e_fdir_filter *fdir_filter = &fdir_ctx->fdir_filter;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(fdir_ctx->base.dev_data->dev_private);

	/* Get customized pctype value */
	if (fdir_filter->input.flow_ext.customized_pctype) {
		enum i40e_filter_pctype pctype = i40e_flow_fdir_get_pctype_value(pf,
				fdir_ctx->custom_pctype, fdir_filter);
		if (pctype == I40E_FILTER_PCTYPE_INVALID) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Unsupported packet type");
		}
		/* update FDIR packet type */
		fdir_filter->input.pctype = pctype;
	}

	return 0;
}

static const struct flow_graph i40e_fdir_graph = {
	.nodes = (struct flow_graph_node[]) {
		[I40E_FDIR_NODE_START] = { .name = "START" },
		[I40E_FDIR_NODE_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_fdir_node_eth_validate,
			.process = i40e_fdir_node_eth_process,
		},
		[I40E_FDIR_NODE_VLAN] = {
			.name = "VLAN",
			.type = RTE_FLOW_ITEM_TYPE_VLAN,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_fdir_node_vlan_validate,
			.process = i40e_fdir_node_vlan_process,
		},
		[I40E_FDIR_NODE_IPV4] = {
			.name = "IPv4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC |
				       FLOW_GRAPH_NODE_EXPECT_RANGE,
			.validate = i40e_fdir_node_ipv4_validate,
			.process = i40e_fdir_node_ipv4_process,
		},
		[I40E_FDIR_NODE_IPV6] = {
			.name = "IPv6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_fdir_node_ipv6_validate,
			.process = i40e_fdir_node_ipv6_process,
		},
		[I40E_FDIR_NODE_TCP] = {
			.name = "TCP",
			.type = RTE_FLOW_ITEM_TYPE_TCP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_fdir_node_tcp_validate,
			.process = i40e_fdir_node_tcp_process,
		},
		[I40E_FDIR_NODE_UDP] = {
			.name = "UDP",
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_fdir_node_udp_validate,
			.process = i40e_fdir_node_udp_process,
		},
		[I40E_FDIR_NODE_SCTP] = {
			.name = "SCTP",
			.type = RTE_FLOW_ITEM_TYPE_SCTP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
				       FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_fdir_node_sctp_validate,
			.process = i40e_fdir_node_sctp_process,
		},
		[I40E_FDIR_NODE_ESP] = {
			.name = "ESP",
			.type = RTE_FLOW_ITEM_TYPE_ESP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_fdir_node_esp_validate,
			.process = i40e_fdir_node_esp_process,
		},
		[I40E_FDIR_NODE_L2TPV3OIP] = {
			.name = "L2TPV3OIP",
			.type = RTE_FLOW_ITEM_TYPE_L2TPV3OIP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_fdir_node_l2tpv3oip_validate,
			.process = i40e_fdir_node_l2tpv3oip_process,
		},
		[I40E_FDIR_NODE_GTPC] = {
			.name = "GTPC",
			.type = RTE_FLOW_ITEM_TYPE_GTPC,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_fdir_node_gtp_validate,
			.process = i40e_fdir_node_gtp_process,
		},
		[I40E_FDIR_NODE_GTPU] = {
			.name = "GTPU",
			.type = RTE_FLOW_ITEM_TYPE_GTPU,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_fdir_node_gtp_validate,
			.process = i40e_fdir_node_gtp_process,
		},
		[I40E_FDIR_NODE_INNER_IPV4] = {
			.name = "INNER_IPv4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.validate = i40e_fdir_node_ipv4_validate,
			.process = i40e_fdir_node_inner_ipv4_process,
		},
		[I40E_FDIR_NODE_INNER_IPV6] = {
			.name = "INNER_IPv6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.validate = i40e_fdir_node_ipv6_validate,
			.process = i40e_fdir_node_inner_ipv6_process,
		},
		[I40E_FDIR_NODE_RAW] = {
			.name = "RAW",
			.type = RTE_FLOW_ITEM_TYPE_RAW,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_fdir_node_raw_validate,
			.process = i40e_fdir_node_raw_process,
		},
		[I40E_FDIR_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
			.validate = i40e_fdir_node_end_validate,
			.process = i40e_fdir_node_end_process
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[I40E_FDIR_NODE_START] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_ETH] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_VLAN,
				I40E_FDIR_NODE_IPV4,
				I40E_FDIR_NODE_IPV6,
				I40E_FDIR_NODE_RAW,
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_VLAN] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_IPV4,
				I40E_FDIR_NODE_IPV6,
				I40E_FDIR_NODE_RAW,
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_IPV4] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_TCP,
				I40E_FDIR_NODE_UDP,
				I40E_FDIR_NODE_SCTP,
				I40E_FDIR_NODE_ESP,
				I40E_FDIR_NODE_L2TPV3OIP,
				I40E_FDIR_NODE_RAW,
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_IPV6] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_TCP,
				I40E_FDIR_NODE_UDP,
				I40E_FDIR_NODE_SCTP,
				I40E_FDIR_NODE_ESP,
				I40E_FDIR_NODE_L2TPV3OIP,
				I40E_FDIR_NODE_RAW,
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_TCP] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_RAW,
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_UDP] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_GTPC,
				I40E_FDIR_NODE_GTPU,
				I40E_FDIR_NODE_ESP,
				I40E_FDIR_NODE_RAW,
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_SCTP] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_RAW,
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_ESP] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_L2TPV3OIP] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_GTPC] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_GTPU] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_INNER_IPV4,
				I40E_FDIR_NODE_INNER_IPV6,
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_INNER_IPV4] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_INNER_IPV6] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_FDIR_NODE_RAW] = {
			.next = (size_t[]) {
				I40E_FDIR_NODE_RAW,
				I40E_FDIR_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

static int
i40e_fdir_action_check(const struct ci_flow_actions *actions,
		const struct ci_flow_actions_check_param *param,
		struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(param->driver_ctx);
	const struct rte_flow_action *first, *second;

	first = actions->actions[0];
	/* can be NULL */
	second = actions->actions[1];

	switch (first->type) {
	case RTE_FLOW_ACTION_TYPE_QUEUE:
	{
		const struct rte_flow_action_queue *act_q = first->conf;
		/* check against PF constraints */
		if (act_q->index >= pf->dev_data->nb_rx_queues) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, first,
					"Invalid queue ID for FDIR");
		}
		break;
	}
	case RTE_FLOW_ACTION_TYPE_DROP:
	case RTE_FLOW_ACTION_TYPE_PASSTHRU:
	case RTE_FLOW_ACTION_TYPE_MARK:
		break;
	default:
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, first,
				"Invalid first action for FDIR");
	}

	/* do we have another? */
	if (second == NULL)
		return 0;

	switch (second->type) {
	case RTE_FLOW_ACTION_TYPE_MARK:
	{
		/* only one mark action can be specified */
		if (first->type == RTE_FLOW_ACTION_TYPE_MARK) {
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION, second,
						  "Invalid second action for FDIR");
		}
		break;
	}
	case RTE_FLOW_ACTION_TYPE_FLAG:
	{
		/* mark + flag is unsupported */
		if (first->type == RTE_FLOW_ACTION_TYPE_MARK) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, second,
					"Invalid second action for FDIR");
		}
		break;
	}
	case RTE_FLOW_ACTION_TYPE_RSS:
		/* RSS filter only can be after passthru or mark */
		if (first->type != RTE_FLOW_ACTION_TYPE_PASSTHRU &&
				first->type != RTE_FLOW_ACTION_TYPE_MARK) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, second,
					"Invalid second action for FDIR");
		}
		break;
	default:
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, second,
					  "Invalid second action for FDIR");
	}

	return 0;
}

static int
i40e_fdir_ctx_init(const struct rte_flow_action *actions,
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct i40e_adapter *adapter = I40E_DEV_PRIVATE_TO_ADAPTER(ctx->dev_data->dev_private);
	struct i40e_fdir_ctx *fdir_ctx = (struct i40e_fdir_ctx *)ctx;
	struct ci_flow_actions parsed_actions = {0};
	struct ci_flow_actions_check_param ac_param = {
		.allowed_types = (enum rte_flow_action_type[]) {
			RTE_FLOW_ACTION_TYPE_QUEUE,
			RTE_FLOW_ACTION_TYPE_DROP,
			RTE_FLOW_ACTION_TYPE_PASSTHRU,
			RTE_FLOW_ACTION_TYPE_MARK,
			RTE_FLOW_ACTION_TYPE_FLAG,
			RTE_FLOW_ACTION_TYPE_RSS,
			RTE_FLOW_ACTION_TYPE_END
		},
		.max_actions = 2,
		.driver_ctx = adapter,
		.check = i40e_fdir_action_check,
	};
	int ret;
	const struct rte_flow_action *first, *second;

	ret = ci_flow_check_attr(attr, NULL, error);
	if (ret)
		return ret;

	ret = ci_flow_check_actions(actions, &ac_param, &parsed_actions, error);
	if (ret)
		return ret;

	first = parsed_actions.actions[0];
	/* can be NULL */
	second = parsed_actions.actions[1];

	if (first->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
		const struct rte_flow_action_queue *act_q = first->conf;
		fdir_ctx->fdir_filter.action.rx_queue = act_q->index;
		fdir_ctx->fdir_filter.action.behavior = I40E_FDIR_ACCEPT;
	} else if (first->type == RTE_FLOW_ACTION_TYPE_DROP) {
		fdir_ctx->fdir_filter.action.behavior = I40E_FDIR_REJECT;
	} else if (first->type == RTE_FLOW_ACTION_TYPE_PASSTHRU) {
		fdir_ctx->fdir_filter.action.behavior = I40E_FDIR_PASSTHRU;
	} else if (first->type == RTE_FLOW_ACTION_TYPE_MARK) {
		const struct rte_flow_action_mark *act_m = first->conf;
		fdir_ctx->fdir_filter.action.behavior = I40E_FDIR_PASSTHRU;
		fdir_ctx->fdir_filter.action.report_status = I40E_FDIR_REPORT_ID;
		fdir_ctx->fdir_filter.soft_id = act_m->id;
	}

	if (second != NULL) {
		if (second->type == RTE_FLOW_ACTION_TYPE_MARK) {
			const struct rte_flow_action_mark *act_m = second->conf;
			fdir_ctx->fdir_filter.action.report_status = I40E_FDIR_REPORT_ID;
			fdir_ctx->fdir_filter.soft_id = act_m->id;
		} else if (second->type == RTE_FLOW_ACTION_TYPE_FLAG) {
			fdir_ctx->fdir_filter.action.report_status = I40E_FDIR_NO_REPORT_STATUS;
		}
		/* RSS action does nothing */
	}
	return 0;
}

static int
i40e_fdir_ctx_to_flow(const struct ci_flow_engine_ctx *ctx,
		struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	const struct i40e_fdir_ctx *fdir_ctx = (const struct i40e_fdir_ctx *)ctx;
	struct i40e_flow_engine_fdir_flow *fdir_flow = (struct i40e_flow_engine_fdir_flow *)flow;

	fdir_flow->fdir_filter = fdir_ctx->fdir_filter;

	return 0;
}

/* Add a flow director filter into the SW hash table */
static int
i40e_fdir_filter_hash_add(struct i40e_fdir_engine_priv *priv,
		const struct i40e_fdir_input *filter_key)
{
	int ret;

	/* try to find the filter in the hash table first */
	ret = rte_hash_lookup(priv->hash_table, filter_key);
	if (ret >= 0) {
		PMD_DRV_LOG(ERR, "Filter already exists!");
		return -EEXIST;
	}

	ret = rte_hash_add_key(priv->hash_table, filter_key);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to insert fdir filter: %s", rte_strerror(-ret));
		return ret;
	}

	return 0;
}

/* Delete a flow director filter from the SW hash table */
static int
i40e_fdir_filter_hash_del(struct i40e_fdir_engine_priv *priv,
		const struct i40e_fdir_input *filter_key)
{
	int ret;

	ret = rte_hash_del_key(priv->hash_table, filter_key);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to delete fdir filter: %s", rte_strerror(-ret));
		return ret;
	}

	return 0;
}

/**
 * Records the filter and everything derived from it.
 */
static int
i40e_fdir_flow_register(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_fdir_engine_priv *priv = flow->engine_priv;
	struct i40e_flow_engine_fdir_flow *fdir_flow = (struct i40e_flow_engine_fdir_flow *)flow;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(flow->dev_data->dev_private);
	const struct i40e_fdir_filter *filter = &fdir_flow->fdir_filter;
	enum i40e_filter_pctype pctype = i40e_fdir_filter_pctype(filter);
	uint64_t input_set = filter->input.flow_ext.input_set;
	struct i40e_fdir_state *fdir = &pf->fdir.fdir_state;
	struct i40e_fdir_pctype_state *pctype_state = &fdir->pctype[pctype];
	struct i40e_fdir_flex_mask new_mask = {0};
	bool common_pctype, flex_flow;
	int ret;

	/* fdir_space_size is a hardware capacity limit. */
	if (pf->fdir.fdir_rule_count >= pf->fdir.fdir_space_size) {
		return rte_flow_error_set(error, ENOSPC,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"FDIR filter space is full");
	}

	/* check input set if the packet type is common */
	common_pctype = !filter->input.flow_ext.customized_pctype;
	flex_flow = filter->input.flow_ext.is_flex_flow;

	if (common_pctype) {
		ret = i40e_fdir_inset_check(pf, pctype, input_set);
		if (ret < 0) {
			return rte_flow_error_set(error, -ret,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Invalid input set");
		}
	}

	/* validate and prepare flex flow configuration */
	if (flex_flow) {
		/* flex payload needs global registers, which multi-driver mode forbids */
		if (pf->support_multi_driver) {
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Flexible payload not supported in multi-driver mode");
		}
		ret = i40e_fdir_flex_prepare(pf, filter, pctype, &new_mask);
		if (ret < 0) {
			return rte_flow_error_set(error, -ret,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Invalid flex flow configuration");
		}
	}

	/* register the flow with the hash table */
	ret = i40e_fdir_filter_hash_add(priv, &filter->input);
	if (ret < 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to register fdir filter");
	}

	/* store input set for common pctypes (otherwise assume installed) */
	if (common_pctype)
		pctype_state->input_set = input_set;
	else
		pctype_state->installed = true;

	if (flex_flow) {
		struct i40e_fdir_flex_layer_state *fls;
		struct i40e_fdir_flex_mask_state *fms;

		/* write flex configuration into flow store */
		i40e_fdir_flex_store(fdir, filter, &new_mask);

		fls = &fdir->flex_layer[filter->input.flow_ext.layer_idx];
		fms = &pctype_state->flex;

		ci_usage_count_claim(&fls->use_count);
		ci_usage_count_claim(&fms->use_count);
	}

	/* claim pctype configuration */
	if (common_pctype)
		ci_usage_count_claim(&pctype_state->use_count);
	if (ci_usage_count_claim(&pf->fdir.fdir_rule_count))
		i40e_fdir_rx_proc_sync(pf->dev_data);

	return 0;
}

/**
 * i40e_fdir_flow_unregister - drop software ownership of a filter.
 */
static int
i40e_fdir_flow_unregister(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_fdir_engine_priv *priv = flow->engine_priv;
	struct i40e_flow_engine_fdir_flow *fdir_flow = (struct i40e_flow_engine_fdir_flow *)flow;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(flow->dev_data->dev_private);
	const struct i40e_fdir_filter *filter = &fdir_flow->fdir_filter;
	enum i40e_filter_pctype pctype = i40e_fdir_filter_pctype(filter);
	bool common_pctype = !filter->input.flow_ext.customized_pctype;
	bool flex_flow = filter->input.flow_ext.is_flex_flow;
	struct i40e_fdir_state *fdir = &pf->fdir.fdir_state;
	struct i40e_fdir_pctype_state *pctype_state = &fdir->pctype[pctype];
	int ret;

	ret = i40e_fdir_filter_hash_del(priv, &filter->input);
	if (ret < 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to unregister fdir filter");
	}
	if (flex_flow) {
		enum i40e_flxpld_layer_idx layer_idx = filter->input.flow_ext.layer_idx;
		struct i40e_fdir_flex_layer_state *fls = &fdir->flex_layer[layer_idx];

		/* release flex layer */
		if (ci_usage_count_release(&fls->use_count))
			i40e_fdir_flex_pit_set_default(fls->flex_set);
		/* release pctype flex mask */
		if (ci_usage_count_release(&pctype_state->flex.use_count))
			i40e_fdir_flex_mask_set_default(&pctype_state->flex.mask);
	}

	/* release pctype configuration */
	if (common_pctype && ci_usage_count_release(&pctype_state->use_count))
		*pctype_state = (struct i40e_fdir_pctype_state){0};

	/* release fdir processing queue flags */
	if (ci_usage_count_release(&pf->fdir.fdir_rule_count))
		i40e_fdir_rx_proc_sync(pf->dev_data);

	return 0;
}

static int
i40e_fdir_flow_install(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_flow_engine_fdir_flow *fdir_flow = (struct i40e_flow_engine_fdir_flow *)flow;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(flow->dev_data->dev_private);
	const struct i40e_fdir_filter *filter = &fdir_flow->fdir_filter;
	enum i40e_filter_pctype pctype = i40e_fdir_filter_pctype(filter);
	struct i40e_fdir_state *fdir = &pf->fdir.fdir_state;
	struct i40e_fdir_pctype_state *pctype_state = &fdir->pctype[pctype];
	bool common_pctype = !filter->input.flow_ext.customized_pctype;
	bool flex_flow = filter->input.flow_ext.is_flex_flow;
	uint32_t filter_count = pf->fdir.fdir_rule_count;
	bool needs_wait;
	int ret;

	/*
	 * when installing a flow, it may so happen that the flow is being
	 * allocated from shared fdir pool, so we need to wait for adminq to
	 * return status to know if the flow was actually installed.
	 *
	 * however, there are cases where the wait is not needed. one case is
	 * when we are re-installing the flow (HW will update existing filter so
	 * the flow is already there). another case is if we know that we are
	 * not using the shared pool.
	 */
	needs_wait = fdir_flow->installed == false &&
		i40e_fdir_filter_needs_status_wait(pf, filter_count - 1);

	if (!pf->fdir.hw_configured) {
		ret = i40e_fdir_engine_init(pf);
		if (ret < 0) {
			return rte_flow_error_set(error, -ret,
					RTE_FLOW_ERROR_TYPE_HANDLE, flow,
					"Failed to initialize flow director engine.");
		}
		pf->fdir.hw_configured = true;
	}
	if (!pctype_state->installed) {
		ret = i40e_fdir_inset_program(pf, pctype, pctype_state->input_set);
		if (ret < 0) {
			ret = rte_flow_error_set(error, -ret,
					RTE_FLOW_ERROR_TYPE_HANDLE, flow,
					"Failed to program fdir input set.");
			goto rollback_engine;
		}
		pctype_state->installed = true;
	}

	/* if this is a flex flow, we need to install flex layers and masks */
	if (flex_flow) {
		enum i40e_flxpld_layer_idx layer_idx = filter->input.flow_ext.layer_idx;
		struct i40e_fdir_flex_layer_state *fls = &fdir->flex_layer[layer_idx];

		if (!fls->installed) {
			i40e_fdir_flex_pit_program(pf, layer_idx, fls);
			i40e_fdir_flex_ort_program(pf, layer_idx, fls);
			fls->installed = true;
		}
		if (!pctype_state->flex.installed) {
			i40e_fdir_flex_mask_program(pf, pctype, &pctype_state->flex.mask);
			pctype_state->flex.installed = true;
		}
	}

	ret = i40e_fdir_filter_program(pf, filter, true, needs_wait);
	if (ret != 0) {
		ret = rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Failed to program fdir filter.");
		goto rollback_config;
	}

	/* flow is installed, don't wait next time */
	fdir_flow->installed = true;

	return 0;

rollback_config:
	if (flex_flow) {
		enum i40e_flxpld_layer_idx layer_idx = filter->input.flow_ext.layer_idx;
		struct i40e_fdir_flex_layer_state *fls = &fdir->flex_layer[layer_idx];

		if (ci_usage_count_is_last(&pctype_state->flex.use_count)) {
			struct i40e_fdir_flex_mask default_mask = {0};

			i40e_fdir_flex_mask_set_default(&default_mask);
			i40e_fdir_flex_mask_program(pf, pctype, &default_mask);
			pctype_state->flex.installed = false;
		}
		if (ci_usage_count_is_last(&fls->use_count)) {
			struct i40e_fdir_flex_layer_state default_layer = {0};

			i40e_fdir_flex_pit_set_default(default_layer.flex_set);
			i40e_fdir_flex_pit_program(pf, layer_idx, &default_layer);
			fls->installed = false;
		}
	}
	if (common_pctype && ci_usage_count_is_last(&pctype_state->use_count) &&
			pctype_state->installed) {
		if (i40e_fdir_inset_program(pf, pctype, 0) == 0)
			pctype_state->installed = false;
		else
			PMD_DRV_LOG(ERR, "Failed to roll back fdir input set");
	}
rollback_engine:
	if (ci_usage_count_is_last(&pf->fdir.fdir_rule_count) && pf->fdir.hw_configured) {
		i40e_fdir_teardown(pf);
		pf->fdir.hw_configured = false;
	}

	return ret;
}

static int
i40e_fdir_flow_uninstall(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_flow_engine_fdir_flow *fdir_flow = (struct i40e_flow_engine_fdir_flow *)flow;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(flow->dev_data->dev_private);
	const struct i40e_fdir_filter *filter = &fdir_flow->fdir_filter;
	enum i40e_filter_pctype pctype = i40e_fdir_filter_pctype(filter);
	struct i40e_fdir_state *fdir = &pf->fdir.fdir_state;
	struct i40e_fdir_pctype_state *pctype_state = &fdir->pctype[pctype];
	bool common_pctype = !filter->input.flow_ext.customized_pctype;
	bool flex_flow = filter->input.flow_ext.is_flex_flow;
	int ret;

	/* removal does not need wait */
	ret = i40e_fdir_filter_program(pf, &fdir_flow->fdir_filter, false, false);
	if (ret != 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				flow, "Failed to deprogram fdir filter.");
	}

	fdir_flow->installed = false;

	/* if this is a common pctype and it's last flow, deprogram pctype */
	if (common_pctype && ci_usage_count_is_last(&pctype_state->use_count) &&
			pctype_state->installed) {
		ret = i40e_fdir_inset_program(pf, pctype, 0);
		if (ret < 0) {
			return rte_flow_error_set(error, -ret,
					RTE_FLOW_ERROR_TYPE_HANDLE, flow,
					"Failed to clear fdir input set.");
		}
		pctype_state->installed = false;
	}

	/* if this is flex flow, check if we can deprogram flex configuration */
	if (flex_flow) {
		enum i40e_flxpld_layer_idx layer_idx = filter->input.flow_ext.layer_idx;
		struct i40e_fdir_flex_layer_state *fls = &fdir->flex_layer[layer_idx];

		if (ci_usage_count_is_last(&fls->use_count)) {
			struct i40e_fdir_flex_layer_state default_layer = {0};

			i40e_fdir_flex_pit_set_default(default_layer.flex_set);
			i40e_fdir_flex_pit_program(pf, layer_idx, &default_layer);
			fls->installed = false;
		}
		if (ci_usage_count_is_last(&pctype_state->flex.use_count)) {
			struct i40e_fdir_flex_mask default_mask = {0};

			i40e_fdir_flex_mask_set_default(&default_mask);
			i40e_fdir_flex_mask_program(pf, pctype, &default_mask);
			pctype_state->flex.installed = false;
		}
	}

	/* is this last flow? */
	if (!ci_usage_count_is_last(&pf->fdir.fdir_rule_count))
		return 0;

	i40e_fdir_teardown(pf);
	pf->fdir.hw_configured = false;

	return 0;
}

static int
i40e_fdir_flow_engine_init(const struct ci_flow_engine *engine,
		struct rte_eth_dev_data *dev_data,
		void *priv_data)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev_data->dev_private);
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	struct i40e_fdir_engine_priv *priv = priv_data;
	char fdir_hash_name[RTE_HASH_NAMESIZE];
	struct i40e_fdir_flow_pool_entry *pool;
	struct rte_hash *hash_table;
	struct rte_bitmap *bmp;
	uint32_t bmp_size;
	void *bmp_mem;
	uint32_t i;
	int ret;

	snprintf(fdir_hash_name, sizeof(fdir_hash_name), "fdir_%s", dev_data->name);

	struct rte_hash_parameters fdir_hash_params = {
		.name = fdir_hash_name,
		.entries = I40E_MAX_FDIR_FILTER_NUM,
		.key_len = sizeof(struct i40e_fdir_input),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};

	hash_table = rte_hash_create(&fdir_hash_params);
	if (hash_table == NULL) {
		PMD_INIT_LOG(ERR, "Failed to create fdir hash table: %s", rte_strerror(rte_errno));
		ret = -rte_errno;
		goto err_hash;
	}

	pool = rte_zmalloc(engine->name,
			fdir_info->fdir_space_size * sizeof(*pool), 0);
	if (pool == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate fdir pool");
		ret = -ENOMEM;
		goto err_pool;
	}

	bmp_size = rte_bitmap_get_memory_footprint(fdir_info->fdir_space_size);
	bmp_mem = rte_zmalloc("fdir_bmap", bmp_size, RTE_CACHE_LINE_SIZE);
	if (bmp_mem == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate fdir bitmap");
		ret = -ENOMEM;
		goto err_bitmap_mem;
	}

	bmp = rte_bitmap_init(fdir_info->fdir_space_size, bmp_mem, bmp_size);
	if (bmp == NULL) {
		PMD_INIT_LOG(ERR, "Failed to initialize fdir bitmap: %s", rte_strerror(rte_errno));
		ret = -rte_errno;
		goto err_bitmap;
	}

	for (i = 0; i < fdir_info->fdir_space_size; i++) {
		pool[i].idx = i;
		rte_bitmap_set(bmp, i);
	}

	priv->pool = pool;
	priv->bitmap = bmp;
	priv->hash_table = hash_table;

	return 0;
err_bitmap:
	rte_free(bmp_mem);
err_bitmap_mem:
	rte_free(pool);
err_pool:
	rte_hash_free(hash_table);
err_hash:
	return ret;
}

static void
i40e_fdir_flow_engine_uninit(const struct ci_flow_engine *engine __rte_unused,
		void *priv_data)
{
	struct i40e_fdir_engine_priv *priv = priv_data;

	rte_free(priv->bitmap);
	rte_free(priv->pool);
	rte_hash_free(priv->hash_table);
}

static struct ci_flow *
i40e_fdir_flow_alloc(const struct ci_flow_engine *engine __rte_unused,
		struct rte_eth_dev_data *dev_data,
		void *priv_data)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev_data->dev_private);
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	struct i40e_fdir_engine_priv *priv = priv_data;
	struct i40e_flow_engine_fdir_flow *flow;
	uint64_t slab = 0;
	uint32_t pos = 0;
	uint32_t bit;
	size_t mem_sz;
	int ret;

	if (fdir_info->fdir_rule_count >= fdir_info->fdir_space_size)
		return NULL;

	ret = rte_bitmap_scan(priv->bitmap, &pos, &slab);
	if (ret == 0)
		return NULL;

	bit = rte_bsf64(slab);
	pos += bit;
	rte_bitmap_clear(priv->bitmap, pos);

	flow = &priv->pool[pos].flow;
	/* do not touch ci_flow members, they are initialized by the caller */
	mem_sz = sizeof(*flow) - sizeof(struct ci_flow);

	memset(RTE_PTR_ADD(flow, sizeof(struct ci_flow)), 0, mem_sz);
	return (struct ci_flow *)flow;
}

static void
i40e_fdir_flow_free(struct ci_flow *flow,
		struct rte_eth_dev_data *dev_data __rte_unused,
		void *priv_data)
{
	struct i40e_fdir_engine_priv *priv = priv_data;
	struct i40e_fdir_flow_pool_entry *entry;

	entry = I40E_FDIR_FLOW_ENTRY((struct i40e_flow_engine_fdir_flow *)flow);
	/* idx is not set at alloc, it's set at init */
	rte_bitmap_set(priv->bitmap, entry->idx);
}

static const struct ci_flow_engine_ops i40e_flow_engine_fdir_ops = {
	.engine_init = i40e_fdir_flow_engine_init,
	.engine_uninit = i40e_fdir_flow_engine_uninit,
	.flow_alloc = i40e_fdir_flow_alloc,
	.flow_free = i40e_fdir_flow_free,
	.ctx_init = i40e_fdir_ctx_init,
	.ctx_to_flow = i40e_fdir_ctx_to_flow,
	.flow_register = i40e_fdir_flow_register,
	.flow_unregister = i40e_fdir_flow_unregister,
	.flow_install = i40e_fdir_flow_install,
	.flow_uninstall = i40e_fdir_flow_uninstall,
};

const struct ci_flow_engine i40e_flow_engine_fdir = {
	.name = "i40e_fdir",
	.ops = &i40e_flow_engine_fdir_ops,
	.ctx_size = sizeof(struct i40e_fdir_ctx),
	.flow_size = sizeof(struct i40e_flow_engine_fdir_flow),
	.priv_size = sizeof(struct i40e_fdir_engine_priv),
	.graph = &i40e_fdir_graph,
};
