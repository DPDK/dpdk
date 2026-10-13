/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#include "i40e_ethdev.h"
#include "i40e_flow.h"
#include "i40e_hash.h"

#include "../common/flow_engine.h"
#include "../common/flow_check.h"
#include "../common/flow_util.h"

struct i40e_hash_priv {
	struct i40e_rss_state *state;
};

struct i40e_hash_ctx {
	struct ci_flow_engine_ctx base;
	struct i40e_rss_conf rss_conf;
	uint32_t pctype;
	bool customized_ptype;
	bool outer_ipv6;
};

struct i40e_flow_engine_hash_flow {
	struct rte_flow base;
	struct i40e_rss_conf rss_conf;
	struct i40e_rss_flow_node *node;
};

#define I40E_HASH_L4_TYPES		(RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
					RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
					RTE_ETH_RSS_NONFRAG_IPV4_SCTP | \
					RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
					RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
					RTE_ETH_RSS_NONFRAG_IPV6_SCTP)

#define I40E_HASH_L2_RSS_MASK		(RTE_ETH_RSS_VLAN | RTE_ETH_RSS_ETH | \
					RTE_ETH_RSS_L2_SRC_ONLY | \
					RTE_ETH_RSS_L2_DST_ONLY)

#define I40E_HASH_L23_RSS_MASK		(I40E_HASH_L2_RSS_MASK | \
					RTE_ETH_RSS_L3_SRC_ONLY | \
					RTE_ETH_RSS_L3_DST_ONLY)

#define I40E_HASH_IPV4_L23_RSS_MASK	(RTE_ETH_RSS_IPV4 | I40E_HASH_L23_RSS_MASK)
#define I40E_HASH_IPV6_L23_RSS_MASK	(RTE_ETH_RSS_IPV6 | I40E_HASH_L23_RSS_MASK)

#define I40E_HASH_L234_RSS_MASK		(I40E_HASH_L23_RSS_MASK | \
					RTE_ETH_RSS_PORT | RTE_ETH_RSS_L4_SRC_ONLY | \
					RTE_ETH_RSS_L4_DST_ONLY)

#define I40E_HASH_IPV4_L234_RSS_MASK	(I40E_HASH_L234_RSS_MASK | RTE_ETH_RSS_IPV4)
#define I40E_HASH_IPV6_L234_RSS_MASK	(I40E_HASH_L234_RSS_MASK | RTE_ETH_RSS_IPV6)

/* Structure of mapping RSS type to input set */
struct i40e_hash_map_rss_inset {
	uint64_t rss_type;
	uint64_t inset;
};

static const struct i40e_hash_map_rss_inset i40e_hash_rss_inset[] = {
	/* IPv4 */
	{ RTE_ETH_RSS_IPV4, I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST },
	{ RTE_ETH_RSS_FRAG_IPV4, I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST },

	{ RTE_ETH_RSS_NONFRAG_IPV4_OTHER,
	  I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST },

	{ RTE_ETH_RSS_NONFRAG_IPV4_TCP, I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
	  I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT },

	{ RTE_ETH_RSS_NONFRAG_IPV4_UDP, I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
	  I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT },

	{ RTE_ETH_RSS_NONFRAG_IPV4_SCTP, I40E_INSET_IPV4_SRC | I40E_INSET_IPV4_DST |
	  I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT | I40E_INSET_SCTP_VT },

	/* IPv6 */
	{ RTE_ETH_RSS_IPV6, I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST },
	{ RTE_ETH_RSS_FRAG_IPV6, I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST },

	{ RTE_ETH_RSS_NONFRAG_IPV6_OTHER,
	  I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST },

	{ RTE_ETH_RSS_NONFRAG_IPV6_TCP, I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
	  I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT },

	{ RTE_ETH_RSS_NONFRAG_IPV6_UDP, I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
	  I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT },

	{ RTE_ETH_RSS_NONFRAG_IPV6_SCTP, I40E_INSET_IPV6_SRC | I40E_INSET_IPV6_DST |
	  I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT | I40E_INSET_SCTP_VT },

	/* Port */
	{ RTE_ETH_RSS_PORT, I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT },

	/* Ether */
	{ RTE_ETH_RSS_L2_PAYLOAD, I40E_INSET_LAST_ETHER_TYPE },
	{ RTE_ETH_RSS_ETH, I40E_INSET_DMAC | I40E_INSET_SMAC },

	/* VLAN */
	{ RTE_ETH_RSS_S_VLAN, I40E_INSET_VLAN_OUTER },
	{ RTE_ETH_RSS_C_VLAN, I40E_INSET_VLAN_INNER },
};

static uint64_t
i40e_hash_get_inset(uint64_t rss_types, bool symmetric_enable)
{
	uint64_t mask, inset = 0;
	int i;

	for (i = 0; i < (int)RTE_DIM(i40e_hash_rss_inset); i++) {
		if (rss_types & i40e_hash_rss_inset[i].rss_type)
			inset |= i40e_hash_rss_inset[i].inset;
	}

	if (!inset)
		return 0;

	/* If SRC_ONLY and DST_ONLY of the same level are used simultaneously,
	 * it is the same case as none of them are added.
	 */
	mask = rss_types & (RTE_ETH_RSS_L2_SRC_ONLY | RTE_ETH_RSS_L2_DST_ONLY);
	if (mask == RTE_ETH_RSS_L2_SRC_ONLY)
		inset &= ~I40E_INSET_DMAC;
	else if (mask == RTE_ETH_RSS_L2_DST_ONLY)
		inset &= ~I40E_INSET_SMAC;

	mask = rss_types & (RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY);
	if (mask == RTE_ETH_RSS_L3_SRC_ONLY)
		inset &= ~(I40E_INSET_IPV4_DST | I40E_INSET_IPV6_DST);
	else if (mask == RTE_ETH_RSS_L3_DST_ONLY)
		inset &= ~(I40E_INSET_IPV4_SRC | I40E_INSET_IPV6_SRC);

	mask = rss_types & (RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY);
	if (mask == RTE_ETH_RSS_L4_SRC_ONLY)
		inset &= ~I40E_INSET_DST_PORT;
	else if (mask == RTE_ETH_RSS_L4_DST_ONLY)
		inset &= ~I40E_INSET_SRC_PORT;

	if (rss_types & I40E_HASH_L4_TYPES) {
		uint64_t l3_mask = rss_types &
				   (RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY);
		uint64_t l4_mask = rss_types &
				   (RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY);

		if (l3_mask && !l4_mask)
			inset &= ~(I40E_INSET_SRC_PORT | I40E_INSET_DST_PORT);
		else if (!l3_mask && l4_mask)
			inset &= ~(I40E_INSET_IPV4_DST | I40E_INSET_IPV6_DST |
				 I40E_INSET_IPV4_SRC | I40E_INSET_IPV6_SRC);
	}

	/* SCTP Verification Tag is not required in hash computation for SYMMETRIC_TOEPLITZ */
	if (symmetric_enable) {
		mask = rss_types & RTE_ETH_RSS_NONFRAG_IPV4_SCTP;
		if (mask == RTE_ETH_RSS_NONFRAG_IPV4_SCTP)
			inset &= ~I40E_INSET_SCTP_VT;

		mask = rss_types & RTE_ETH_RSS_NONFRAG_IPV6_SCTP;
		if (mask == RTE_ETH_RSS_NONFRAG_IPV6_SCTP)
			inset &= ~I40E_INSET_SCTP_VT;
	}

	return inset;
}

/*
 * Hash pattern graph implementation
 * Pattern: START -> ETH -> [VLAN] -> [VLAN] -> (IPv4|IPv6) -> (TCP|UDP|SCTP|ESP|L2TPV3OIP|AH)
 *          START -> ETH -> [VLAN] -> [VLAN] -> (IPv4|IPv6) frag
 *          START -> ETH -> [VLAN] -> [VLAN] -> (IPv4|IPv6) -> UDP -> (GTPC|ESP|GTPU)
 *          START -> ETH -> [VLAN] -> [VLAN] -> (IPv4|IPv6) -> UDP -> GTPU -> (IPv4|IPv6)
 */
enum i40e_hash_pattern_node_id {
	I40E_HASH_PATTERN_NODE_START = FLOW_GRAPH_NODE_FIRST,
	I40E_HASH_PATTERN_NODE_ETH,
	I40E_HASH_PATTERN_NODE_OUTER_VLAN,
	I40E_HASH_PATTERN_NODE_INNER_VLAN,
	I40E_HASH_PATTERN_NODE_IPV4,
	I40E_HASH_PATTERN_NODE_IPV6,
	I40E_HASH_PATTERN_NODE_IPV6_FRAG,
	I40E_HASH_PATTERN_NODE_TCP,
	I40E_HASH_PATTERN_NODE_UDP,
	I40E_HASH_PATTERN_NODE_SCTP,
	I40E_HASH_PATTERN_NODE_ESP,
	I40E_HASH_PATTERN_NODE_GTPU,
	I40E_HASH_PATTERN_NODE_GTPC,
	I40E_HASH_PATTERN_NODE_L2TPV3OIP,
	I40E_HASH_PATTERN_NODE_AH,
	I40E_HASH_PATTERN_NODE_INNER_IPV4,
	I40E_HASH_PATTERN_NODE_INNER_IPV6,
	I40E_HASH_PATTERN_NODE_END,
	I40E_HASH_PATTERN_NODE_MAX,
};

static int
i40e_hash_pattern_node_eth_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;
	hash_ctx->pctype = I40E_FILTER_PCTYPE_L2_PAYLOAD;
	return 0;
}

static int
i40e_hash_pattern_node_ipv4_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;
	/* frag vs non-frag is resolved against RSS types at finalize */
	hash_ctx->pctype = I40E_FILTER_PCTYPE_NONF_IPV4_OTHER;
	return 0;
}

static int
i40e_hash_pattern_node_ipv6_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;
	hash_ctx->pctype = I40E_FILTER_PCTYPE_NONF_IPV6_OTHER;
	hash_ctx->outer_ipv6 = true;
	return 0;
}

static int
i40e_hash_pattern_node_ipv6_frag_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;
	hash_ctx->pctype = I40E_FILTER_PCTYPE_FRAG_IPV6;
	return 0;
}

static int
i40e_hash_pattern_node_tcp_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;
	hash_ctx->pctype = (hash_ctx->pctype == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER) ?
			  I40E_FILTER_PCTYPE_NONF_IPV4_TCP :
			  I40E_FILTER_PCTYPE_NONF_IPV6_TCP;
	return 0;
}

static int
i40e_hash_pattern_node_udp_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;
	hash_ctx->pctype = (hash_ctx->pctype == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER) ?
			  I40E_FILTER_PCTYPE_NONF_IPV4_UDP :
			  I40E_FILTER_PCTYPE_NONF_IPV6_UDP;
	return 0;
}

static int
i40e_hash_pattern_node_sctp_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;
	hash_ctx->pctype = (hash_ctx->pctype == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER) ?
			  I40E_FILTER_PCTYPE_NONF_IPV4_SCTP :
			  I40E_FILTER_PCTYPE_NONF_IPV6_SCTP;
	return 0;
}

static int
i40e_hash_pattern_node_esp_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;
	bool ipv4 = false;
	bool udp = false;

	/* ESP can be over IP or over UDP */
	ipv4 = (hash_ctx->pctype == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER ||
		hash_ctx->pctype == I40E_FILTER_PCTYPE_NONF_IPV4_UDP);
	udp = (hash_ctx->pctype == I40E_FILTER_PCTYPE_NONF_IPV4_UDP ||
		hash_ctx->pctype == I40E_FILTER_PCTYPE_NONF_IPV6_UDP);
	if (udp)
		hash_ctx->pctype = ipv4 ? I40E_CUSTOMIZED_ESP_IPV4_UDP : I40E_CUSTOMIZED_ESP_IPV6_UDP;
	else
		hash_ctx->pctype = ipv4 ? I40E_CUSTOMIZED_ESP_IPV4 : I40E_CUSTOMIZED_ESP_IPV6;

	hash_ctx->customized_ptype = true;
	return 0;
}

static int
i40e_hash_pattern_node_gtpu_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;

	/* GTPU pctype does not differentiate between IPv4 and IPv6 */
	hash_ctx->pctype = I40E_CUSTOMIZED_GTPU;
	hash_ctx->customized_ptype = true;
	return 0;
}

static int
i40e_hash_pattern_node_gtpc_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;

	/* GTPC pctype does not differentiate between IPv4 and IPv6 */
	hash_ctx->pctype = I40E_CUSTOMIZED_GTPC;
	hash_ctx->customized_ptype = true;
	return 0;
}

static int
i40e_hash_pattern_node_l2tpv3oip_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;
	hash_ctx->pctype = (hash_ctx->pctype == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER) ?
			  I40E_CUSTOMIZED_IPV4_L2TPV3 :
			  I40E_CUSTOMIZED_IPV6_L2TPV3;
	hash_ctx->customized_ptype = true;
	return 0;
}

static int
i40e_hash_pattern_node_ah_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;

	hash_ctx->pctype = (hash_ctx->pctype == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER) ?
			  I40E_CUSTOMIZED_AH_IPV4 :
			  I40E_CUSTOMIZED_AH_IPV6;
	hash_ctx->customized_ptype = true;
	return 0;
}

static int
i40e_hash_pattern_node_inner_ipv4_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;

	/* inner IP patterns are always over GTP-U */
	hash_ctx->pctype = I40E_CUSTOMIZED_GTPU_IPV4;
	hash_ctx->customized_ptype = true;
	return 0;
}

static int
i40e_hash_pattern_node_inner_ipv6_process(void *ctx,
		const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;

	/* inner IP patterns are always over GTP-U */
	hash_ctx->pctype = I40E_CUSTOMIZED_GTPU_IPV6;
	hash_ctx->customized_ptype = true;
	return 0;
}

static const struct flow_graph i40e_hash_pattern_graph = {
	.nodes = (struct flow_graph_node[]) {
		[I40E_HASH_PATTERN_NODE_START] = {
			.name = "START",
		},
		[I40E_HASH_PATTERN_NODE_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_eth_process,
		},
		[I40E_HASH_PATTERN_NODE_OUTER_VLAN] = {
			.name = "OUTER_VLAN",
			.type = RTE_FLOW_ITEM_TYPE_VLAN,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[I40E_HASH_PATTERN_NODE_INNER_VLAN] = {
			.name = "INNER_VLAN",
			.type = RTE_FLOW_ITEM_TYPE_VLAN,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[I40E_HASH_PATTERN_NODE_IPV4] = {
			.name = "IPv4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_ipv4_process,
		},
		[I40E_HASH_PATTERN_NODE_IPV6] = {
			.name = "IPv6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_ipv6_process,
		},
		[I40E_HASH_PATTERN_NODE_IPV6_FRAG] = {
			.name = "IPv6_FRAG",
			.type = RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_ipv6_frag_process,
		},
		[I40E_HASH_PATTERN_NODE_TCP] = {
			.name = "TCP",
			.type = RTE_FLOW_ITEM_TYPE_TCP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_tcp_process,
		},
		[I40E_HASH_PATTERN_NODE_UDP] = {
			.name = "UDP",
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_udp_process,
		},
		[I40E_HASH_PATTERN_NODE_SCTP] = {
			.name = "SCTP",
			.type = RTE_FLOW_ITEM_TYPE_SCTP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_sctp_process,
		},
		[I40E_HASH_PATTERN_NODE_ESP] = {
			.name = "ESP",
			.type = RTE_FLOW_ITEM_TYPE_ESP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_esp_process,
		},
		[I40E_HASH_PATTERN_NODE_GTPU] = {
			.name = "GTPU",
			.type = RTE_FLOW_ITEM_TYPE_GTPU,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_gtpu_process,
		},
		[I40E_HASH_PATTERN_NODE_GTPC] = {
			.name = "GTPC",
			.type = RTE_FLOW_ITEM_TYPE_GTPC,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_gtpc_process,
		},
		[I40E_HASH_PATTERN_NODE_L2TPV3OIP] = {
			.name = "L2TPV3OIP",
			.type = RTE_FLOW_ITEM_TYPE_L2TPV3OIP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_l2tpv3oip_process,
		},
		[I40E_HASH_PATTERN_NODE_AH] = {
			.name = "AH",
			.type = RTE_FLOW_ITEM_TYPE_AH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_ah_process,
		},
		[I40E_HASH_PATTERN_NODE_INNER_IPV4] = {
			.name = "INNER_IPV4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_inner_ipv4_process,
		},
		[I40E_HASH_PATTERN_NODE_INNER_IPV6] = {
			.name = "INNER_IPV6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_hash_pattern_node_inner_ipv6_process,
		},
		[I40E_HASH_PATTERN_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[I40E_HASH_PATTERN_NODE_START] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_ETH] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_OUTER_VLAN,
				I40E_HASH_PATTERN_NODE_IPV4,
				I40E_HASH_PATTERN_NODE_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_OUTER_VLAN] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_INNER_VLAN,
				I40E_HASH_PATTERN_NODE_IPV4,
				I40E_HASH_PATTERN_NODE_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_INNER_VLAN] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_IPV4,
				I40E_HASH_PATTERN_NODE_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_IPV4] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_TCP,
				I40E_HASH_PATTERN_NODE_UDP,
				I40E_HASH_PATTERN_NODE_SCTP,
				I40E_HASH_PATTERN_NODE_ESP,
				I40E_HASH_PATTERN_NODE_L2TPV3OIP,
				I40E_HASH_PATTERN_NODE_AH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_IPV6] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_IPV6_FRAG,
				I40E_HASH_PATTERN_NODE_TCP,
				I40E_HASH_PATTERN_NODE_UDP,
				I40E_HASH_PATTERN_NODE_SCTP,
				I40E_HASH_PATTERN_NODE_ESP,
				I40E_HASH_PATTERN_NODE_L2TPV3OIP,
				I40E_HASH_PATTERN_NODE_AH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_IPV6_FRAG] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_TCP] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_UDP] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_GTPU,
				I40E_HASH_PATTERN_NODE_GTPC,
				I40E_HASH_PATTERN_NODE_ESP,
				I40E_HASH_PATTERN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_SCTP] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_ESP] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_GTPU] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_INNER_IPV4,
				I40E_HASH_PATTERN_NODE_INNER_IPV6,
				I40E_HASH_PATTERN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_GTPC] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_L2TPV3OIP] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_AH] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_INNER_IPV4] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_PATTERN_NODE_INNER_IPV6] = {
			.next = (size_t[]) {
				I40E_HASH_PATTERN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

/*
 * Hash VLAN graph implementation
 * Pattern: START -> VLAN -> END
 */
enum i40e_hash_vlan_node_id {
	I40E_HASH_VLAN_NODE_START = FLOW_GRAPH_NODE_FIRST,
	I40E_HASH_VLAN_NODE_VLAN,
	I40E_HASH_VLAN_NODE_END,
	I40E_HASH_VLAN_NODE_MAX,
};

static int
i40e_hash_node_vlan_validate(const void *ctx __rte_unused, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_vlan *vlan_mask = ci_flow_item_mask(item);

	/* only the VLAN priority bits may be matched */
	if (rte_be_to_cpu_16(vlan_mask->hdr.vlan_tci) != RTE_VLAN_PRI_MASK ||
	    vlan_mask->hdr.eth_proto != 0 ||
	    vlan_mask->has_more_vlan) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid VLAN mask");
	}
	return 0;
}

static int
i40e_hash_node_vlan_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_hash_ctx *hash_ctx = ctx;
	const struct rte_flow_item_vlan *vlan_spec = item->spec;

	hash_ctx->rss_conf.region_priority = rte_be_to_cpu_16(vlan_spec->hdr.vlan_tci) >> 13;

	return 0;
}

static const struct flow_graph i40e_hash_vlan_graph = {
	.nodes = (struct flow_graph_node[]) {
		[I40E_HASH_VLAN_NODE_START] = {
			.name = "START",
		},
		[I40E_HASH_VLAN_NODE_VLAN] = {
			.name = "VLAN",
			.type = RTE_FLOW_ITEM_TYPE_VLAN,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_hash_node_vlan_validate,
			.process = i40e_hash_node_vlan_process,
		},
		[I40E_HASH_VLAN_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[I40E_HASH_VLAN_NODE_START] = {
			.next = (size_t[]) {
				I40E_HASH_VLAN_NODE_VLAN,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_HASH_VLAN_NODE_VLAN] = {
			.next = (size_t[]) {
				I40E_HASH_VLAN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

static bool
i40e_hash_validate_rss_types(uint64_t rss_types)
{
	uint64_t type, mask;

	/* Validate L2 */
	type = RTE_ETH_RSS_ETH & rss_types;
	mask = (RTE_ETH_RSS_L2_SRC_ONLY | RTE_ETH_RSS_L2_DST_ONLY) & rss_types;
	if (!type && mask)
		return false;

	/* Validate L3 */
	type = (I40E_HASH_L4_TYPES | RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 |
	       RTE_ETH_RSS_NONFRAG_IPV4_OTHER | RTE_ETH_RSS_IPV6 |
	       RTE_ETH_RSS_FRAG_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_OTHER) & rss_types;
	mask = (RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY) & rss_types;
	if (!type && mask)
		return false;

	/* Validate L4 */
	type = (I40E_HASH_L4_TYPES | RTE_ETH_RSS_PORT) & rss_types;
	mask = (RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY) & rss_types;
	if (!type && mask)
		return false;

	return true;
}

static int
i40e_hash_validate_rss_common(const struct rte_flow_action_rss *rss_act,
		struct rte_flow_error *error)
{
	/* RSS level is not supported */
	if (rss_act->level != 0) {
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"RSS level is not supported");
	}

	/* symmetric toeplitz is only supported when a specific pattern is provided */
	if (rss_act->func == RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"Symmetric hash function not supported without specific patterns");
	}

	/*
	 * When RSS types is not specified in testpmd, it will set up a default
	 * RSS types value for the flow. Even though no hash engine part calling
	 * this particular function will use RSS types parameter for anything,
	 * we cannot reject having it because it is extra effort for testpmd
	 * user to avoid specifying it.
	 *
	 * So, instead, accept types value even though we are not using it for
	 * anything, but produce a warning for the user.
	 */
	if (rss_act->types != 0)
		PMD_DRV_LOG(WARNING, "RSS types specified but will not be used");

	/* check RSS key length if it is specified */
	if (rss_act->key_len != 0 && rss_act->key_len != I40E_RSS_KEY_LEN) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"RSS key length must be 52 bytes");
	}

	return 0;
}

static int
i40e_hash_pattern_rss_check(const struct ci_flow_actions *actions,
		const struct ci_flow_actions_check_param *param __rte_unused,
		struct rte_flow_error *error)
{
	const struct rte_flow_action_rss *rss_act = actions->actions[0]->conf;

	/* queue list is not supported */
	if (rss_act->queue_num != 0) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"RSS queues not supported when pattern specified");
	}

	/* disallow unsupported hash functions */
	switch (rss_act->func) {
	case RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ:
	case RTE_ETH_HASH_FUNCTION_DEFAULT:
	case RTE_ETH_HASH_FUNCTION_TOEPLITZ:
	case RTE_ETH_HASH_FUNCTION_SIMPLE_XOR:
		break;
	default:
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"RSS hash function not supported when pattern specified");
	}

	if (!i40e_hash_validate_rss_types(rss_act->types))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				rss_act, "RSS types are invalid");

	/* check RSS key length if it is specified */
	if (rss_act->key_len != 0 && rss_act->key_len != I40E_RSS_KEY_LEN) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"RSS key length must be 52 bytes");
	}

	return 0;
}

static int
i40e_hash_pattern_ctx_init(const struct rte_flow_action actions[],
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct i40e_hash_ctx *hash_ctx = (struct i40e_hash_ctx *)ctx;
	struct ci_flow_actions parsed_actions = {0};
	struct ci_flow_actions_check_param param = {
		.allowed_types = (enum rte_flow_action_type[]) {
			RTE_FLOW_ACTION_TYPE_RSS,
			RTE_FLOW_ACTION_TYPE_END
		},
		.max_actions = 1,
		.check = i40e_hash_pattern_rss_check,
	};
	const struct rte_flow_action_rss *rss_act;
	int ret;

	ret = ci_flow_check_attr(attr, NULL, error);
	if (ret != 0)
		return ret;

	ret = ci_flow_check_actions(actions, &param, &parsed_actions, error);
	if (ret != 0)
		return ret;

	rss_act = parsed_actions.actions[0]->conf;

	hash_ctx->rss_conf.symmetric_enable =
			rss_act->func == RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ;
	hash_ctx->rss_conf.func = rss_act->func;
	hash_ctx->rss_conf.types = rss_act->types;

	if (rss_act->key_len != 0) {
		/* key length already checked */
		memcpy(hash_ctx->rss_conf.key, rss_act->key, I40E_RSS_KEY_LEN);
		hash_ctx->rss_conf.key_len = I40E_RSS_KEY_LEN;
	}

	hash_ctx->rss_conf.inset = i40e_hash_get_inset(rss_act->types,
			hash_ctx->rss_conf.symmetric_enable);

	return 0;
}

static uint64_t
i40e_hash_get_x722_ext_pctypes(uint8_t match_pctype)
{
	uint64_t pctypes = 0;

	switch (match_pctype) {
	case I40E_FILTER_PCTYPE_NONF_IPV4_TCP:
		pctypes = BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK);
		break;

	case I40E_FILTER_PCTYPE_NONF_IPV4_UDP:
		pctypes = BIT_ULL(I40E_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP) |
			  BIT_ULL(I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP);
		break;

	case I40E_FILTER_PCTYPE_NONF_IPV6_TCP:
		pctypes = BIT_ULL(I40E_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK);
		break;

	case I40E_FILTER_PCTYPE_NONF_IPV6_UDP:
		pctypes = BIT_ULL(I40E_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP) |
			  BIT_ULL(I40E_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP);
		break;
	}

	return pctypes;
}

static int
i40e_hash_translate_gtp_inset(struct i40e_rss_conf *rss_conf,
			      struct rte_flow_error *error)
{
	if (rss_conf->inset &
	    (I40E_INSET_IPV4_SRC | I40E_INSET_IPV6_SRC |
	    I40E_INSET_DST_PORT | I40E_INSET_SRC_PORT))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL,
					  "Only support external destination IP");

	if (rss_conf->inset & I40E_INSET_IPV4_DST)
		rss_conf->inset = (rss_conf->inset & ~I40E_INSET_IPV4_DST) |
				  I40E_INSET_TUNNEL_IPV4_DST;

	if (rss_conf->inset & I40E_INSET_IPV6_DST)
		rss_conf->inset = (rss_conf->inset & ~I40E_INSET_IPV6_DST) |
				  I40E_INSET_TUNNEL_IPV6_DST;

	return 0;
}

/* plain IP patterns also cover fragmented packets of the same IP version */
static bool
i40e_hash_pctype_is_frag(uint32_t pctype, uint32_t pattern_pctype)
{
	return (pattern_pctype == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER &&
		pctype == I40E_FILTER_PCTYPE_FRAG_IPV4) ||
	       (pattern_pctype == I40E_FILTER_PCTYPE_NONF_IPV6_OTHER &&
		pctype == I40E_FILTER_PCTYPE_FRAG_IPV6);
}

static bool
i40e_hash_pattern_covers_pctype(const struct i40e_hash_ctx *hash_ctx, uint32_t pctype)
{
	return pctype == hash_ctx->pctype || i40e_hash_pctype_is_frag(pctype, hash_ctx->pctype);
}

/* GTP pctypes need outer IP match */
static uint64_t
i40e_hash_pctype_rss_types(const struct i40e_hash_ctx *hash_ctx, uint32_t pctype,
		uint64_t rss_types)
{
	if (hash_ctx->customized_ptype &&
	    (pctype == I40E_CUSTOMIZED_GTPC || pctype == I40E_CUSTOMIZED_GTPU))
		rss_types |= hash_ctx->outer_ipv6 ? RTE_ETH_RSS_IPV6 : RTE_ETH_RSS_IPV4;

	return rss_types;
}

static bool
i40e_hash_rss_types_supported(uint64_t requested, uint64_t supported)
{
	return (requested & ~supported) == 0;
}

static int
i40e_hash_pattern_ctx_finalize(struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(ctx->dev_data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(ctx->dev_data->dev_private);
	struct i40e_hash_ctx *hash_ctx = (struct i40e_hash_ctx *)ctx;
	const struct hash_type_to_pattern {
		uint32_t pctype;
		uint64_t valid_rss_flags;
	} valid_pctype_to_pattern[] = {
		/* Ether */
		{I40E_FILTER_PCTYPE_L2_PAYLOAD, I40E_HASH_L2_RSS_MASK | RTE_ETH_RSS_L2_PAYLOAD},
		/* IP */
		{I40E_FILTER_PCTYPE_NONF_IPV4_OTHER, RTE_ETH_RSS_NONFRAG_IPV4_OTHER | I40E_HASH_IPV4_L23_RSS_MASK},
		{I40E_FILTER_PCTYPE_NONF_IPV6_OTHER, RTE_ETH_RSS_NONFRAG_IPV6_OTHER | I40E_HASH_IPV6_L23_RSS_MASK},
		/* IP fragmented */
		{I40E_FILTER_PCTYPE_FRAG_IPV4, RTE_ETH_RSS_FRAG_IPV4 | I40E_HASH_IPV4_L23_RSS_MASK},
		{I40E_FILTER_PCTYPE_FRAG_IPV6, RTE_ETH_RSS_FRAG_IPV6 | I40E_HASH_IPV6_L23_RSS_MASK},
		/* TCP */
		{I40E_FILTER_PCTYPE_NONF_IPV4_TCP, RTE_ETH_RSS_NONFRAG_IPV4_TCP | I40E_HASH_IPV4_L234_RSS_MASK},
		{I40E_FILTER_PCTYPE_NONF_IPV6_TCP, RTE_ETH_RSS_NONFRAG_IPV6_TCP | I40E_HASH_IPV6_L234_RSS_MASK},
		/* UDP */
		{I40E_FILTER_PCTYPE_NONF_IPV4_UDP, RTE_ETH_RSS_NONFRAG_IPV4_UDP | I40E_HASH_IPV4_L234_RSS_MASK},
		{I40E_FILTER_PCTYPE_NONF_IPV6_UDP, RTE_ETH_RSS_NONFRAG_IPV6_UDP | I40E_HASH_IPV6_L234_RSS_MASK},
		/* SCTP */
		{I40E_FILTER_PCTYPE_NONF_IPV4_SCTP, RTE_ETH_RSS_NONFRAG_IPV4_SCTP | I40E_HASH_IPV4_L234_RSS_MASK},
		{I40E_FILTER_PCTYPE_NONF_IPV6_SCTP, RTE_ETH_RSS_NONFRAG_IPV6_SCTP | I40E_HASH_IPV6_L234_RSS_MASK},
		/* AH */
		{I40E_CUSTOMIZED_AH_IPV4, RTE_ETH_RSS_AH},
		{I40E_CUSTOMIZED_AH_IPV6, RTE_ETH_RSS_AH},
		/* L2TPV3 */
		{I40E_CUSTOMIZED_IPV4_L2TPV3, RTE_ETH_RSS_L2TPV3},
		{I40E_CUSTOMIZED_IPV6_L2TPV3, RTE_ETH_RSS_L2TPV3},
		/* ESP */
		{I40E_CUSTOMIZED_ESP_IPV4, RTE_ETH_RSS_ESP},
		{I40E_CUSTOMIZED_ESP_IPV6, RTE_ETH_RSS_ESP},
		{I40E_CUSTOMIZED_ESP_IPV4_UDP, RTE_ETH_RSS_ESP},
		{I40E_CUSTOMIZED_ESP_IPV6_UDP, RTE_ETH_RSS_ESP},
		/* GTPC/GTPU: IP version bit is added from the outer header */
		{I40E_CUSTOMIZED_GTPC, I40E_HASH_L234_RSS_MASK},
		{I40E_CUSTOMIZED_GTPU, I40E_HASH_L234_RSS_MASK},
		/* IP over GTPU */
		{I40E_CUSTOMIZED_GTPU_IPV4, RTE_ETH_RSS_GTPU},
		{I40E_CUSTOMIZED_GTPU_IPV6, RTE_ETH_RSS_GTPU},
	};
	bool matched = false, accepted = false;
	size_t i;

	for (i = 0; i < RTE_DIM(valid_pctype_to_pattern); i++) {
		uint32_t pctype = valid_pctype_to_pattern[i].pctype;
		uint64_t supported;

		if (!i40e_hash_pattern_covers_pctype(hash_ctx, pctype))
			continue;
		matched = true;

		supported = i40e_hash_pctype_rss_types(hash_ctx, pctype,
				valid_pctype_to_pattern[i].valid_rss_flags);
		if (!i40e_hash_rss_types_supported(hash_ctx->rss_conf.types, supported))
			continue;
		accepted = true;

		/* for customized pctypes, find if it's supported */
		if (hash_ctx->customized_ptype) {
			struct i40e_customized_pctype *ct;

			ct = i40e_find_customized_pctype(pf, pctype);

			if (ct == NULL || !ct->valid) {
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
						"Specified pattern is not supported by the device");
			}
			hash_ctx->rss_conf.config_pctypes |= BIT_ULL(ct->pctype);

			/* GTPC/GTPU endpoints require special handling */
			return i40e_hash_translate_gtp_inset(&hash_ctx->rss_conf, error);
		} else {
			hash_ctx->rss_conf.config_pctypes |= BIT_ULL(pctype);

			/* X722 needs special handling */
			if (hw->mac.type == I40E_MAC_X722) {
				uint64_t types = i40e_hash_get_x722_ext_pctypes(pctype);
				hash_ctx->rss_conf.config_pctypes |= types;
			}
		}
	}

	if (matched && !accepted) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
				"Some RSS types are not supported for the specified pattern");
	}

	return 0;
}

static int
i40e_hash_queue_region_check(const struct ci_flow_actions *actions,
		const struct ci_flow_actions_check_param *param,
		struct rte_flow_error *error)
{
	const struct rte_flow_action_rss *rss_act = actions->actions[0]->conf;
	struct rte_eth_dev_data *dev_data = param->driver_ctx;
	const struct i40e_pf *pf;
	uint64_t hash_queues;
	int ret;

	ret = i40e_hash_validate_rss_common(rss_act, error);
	if (ret)
		return ret;

	RTE_BUILD_BUG_ON(sizeof(hash_queues) != sizeof(pf->hash_enabled_queues));

	/* having RSS key is not supported */
	if (rss_act->key != NULL) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"RSS key not supported");
	}

	/* queue region must be specified */
	if (rss_act->queue_num == 0) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"RSS queues missing");
	}

	/* queue region must be power of two */
	if (!rte_is_power_of_2(rss_act->queue_num)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"RSS queue number must be power of two");
	}

	/* generic checks already filtered out discontiguous/non-unique RSS queues */

	/* queues must not exceed maximum queues per traffic class */
	if (rss_act->queue[rss_act->queue_num - 1] >= I40E_MAX_Q_PER_TC) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"Invalid RSS queue index");
	}

	/* queues must be in LUT */
	pf = I40E_DEV_PRIVATE_TO_PF(dev_data->dev_private);
	hash_queues = (BIT_ULL(rss_act->queue[0] + rss_act->queue_num) - 1) &
			~(BIT_ULL(rss_act->queue[0]) - 1);

	if (hash_queues & ~pf->hash_enabled_queues) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				rss_act, "Some queues are not in LUT");
	}

	return 0;
}

static int
i40e_hash_vlan_ctx_init(const struct rte_flow_action actions[],
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct i40e_hash_ctx *hash_ctx = (struct i40e_hash_ctx *)ctx;
	struct ci_flow_actions parsed_actions = {0};
	struct ci_flow_actions_check_param param = {
		.allowed_types = (enum rte_flow_action_type[]) {
			RTE_FLOW_ACTION_TYPE_RSS,
			RTE_FLOW_ACTION_TYPE_END
		},
		.max_actions = 1,
		.driver_ctx = ctx->dev_data,
		.check = i40e_hash_queue_region_check,
		.rss_queues_contig = true,
	};
	const struct rte_flow_action_rss *rss_act;
	int ret;

	ret = ci_flow_check_attr(attr, NULL, error);
	if (ret != 0)
		return ret;

	ret = ci_flow_check_actions(actions, &param, &parsed_actions, error);
	if (ret != 0)
		return ret;

	rss_act = parsed_actions.actions[0]->conf;
	hash_ctx->rss_conf.func = rss_act->func;
	hash_ctx->rss_conf.region_queue_num = rss_act->queue_num;
	hash_ctx->rss_conf.region_queue_start = rss_act->queue[0];

	return 0;
}

static int
i40e_hash_queue_list_check(const struct ci_flow_actions *actions,
		const struct ci_flow_actions_check_param *param,
		struct rte_flow_error *error)
{
	const struct rte_flow_action_rss *rss_act = actions->actions[0]->conf;
	struct rte_eth_dev_data *dev_data = param->driver_ctx;
	struct i40e_pf *pf;
	struct i40e_hw *hw;
	uint16_t max_queue;
	bool has_queue, has_key;
	int ret;

	ret = i40e_hash_validate_rss_common(rss_act, error);
	if (ret)
		return ret;

	has_queue = rss_act->queue != NULL;
	has_key = rss_act->key != NULL;

	/* if we have queues, we must not have key */
	if (has_queue && has_key) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"RSS key for queue region is not supported");
	}

	/* if there are no queues, no further checks needed */
	if (!has_queue)
		return 0;

	/* check queue number limits */
	hw = I40E_DEV_PRIVATE_TO_HW(dev_data->dev_private);
	if (rss_act->queue_num > hw->func_caps.rss_table_size) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				rss_act, "Too many RSS queues");
	}

	pf = I40E_DEV_PRIVATE_TO_PF(dev_data->dev_private);
	if (pf->dev_data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_VMDQ_FLAG)
		max_queue = i40e_pf_calc_configured_queues_num(pf);
	else
		max_queue = pf->dev_data->nb_rx_queues;

	max_queue = RTE_MIN(max_queue, I40E_MAX_Q_PER_TC);

	/* we know RSS queues are monotonic so we only need to check last queue */
	if (rss_act->queue[rss_act->queue_num - 1] >= max_queue) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"Invalid RSS queue");
	}

	return 0;
}

static int
i40e_hash_empty_ctx_init(const struct rte_flow_action actions[],
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct i40e_hash_ctx *hash_ctx = (struct i40e_hash_ctx *)ctx;
	struct ci_flow_actions parsed_actions = {0};
	struct ci_flow_actions_check_param param = {
		.allowed_types = (enum rte_flow_action_type[]) {
			RTE_FLOW_ACTION_TYPE_RSS,
			RTE_FLOW_ACTION_TYPE_END
		},
		.max_actions = 1,
		.driver_ctx = ctx->dev_data,
		.check = i40e_hash_queue_list_check,
	};
	const struct rte_flow_action_rss *rss_act;
	int ret;

	ret = ci_flow_check_attr(attr, NULL, error);
	if (ret != 0)
		return ret;

	ret = ci_flow_check_actions(actions, &param, &parsed_actions, error);
	if (ret != 0)
		return ret;

	rss_act = parsed_actions.actions[0]->conf;
	hash_ctx->rss_conf.func = rss_act->func;

	/* if we have queues, copy them */
	if (rss_act->queue_num > 0) {
		memcpy(hash_ctx->rss_conf.queue,
				rss_act->queue,
				rss_act->queue_num * sizeof(hash_ctx->rss_conf.queue[0]));
		hash_ctx->rss_conf.queue_num = rss_act->queue_num;
	/* if we have key, copy it */
	} else if (rss_act->key_len > 0) {
		/* key length already checked */
		memcpy(hash_ctx->rss_conf.key, rss_act->key, I40E_RSS_KEY_LEN);
		hash_ctx->rss_conf.key_len = I40E_RSS_KEY_LEN;
	}

	return 0;
}

static int
i40e_hash_ctx_to_flow(const struct ci_flow_engine_ctx *ctx,
		struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	const struct i40e_hash_ctx *hash_ctx = (const struct i40e_hash_ctx *)ctx;
	struct i40e_flow_engine_hash_flow *hash_flow = (struct i40e_flow_engine_hash_flow *)flow;

	memcpy(&hash_flow->rss_conf, &hash_ctx->rss_conf, sizeof(hash_flow->rss_conf));

	return 0;
}

/*
 * Strip from an earlier (prev) filter's owned reset flags anything that a
 * later (ref) filter now also owns, since the later one takes over ownership
 * of overlapping HW state.
 */
static void
i40e_hash_invalidate_prev(const struct i40e_rss_conf *ref_conf,
		const struct i40e_rss_filter_data *ref_data,
		const struct i40e_rss_conf *prev_conf,
		struct i40e_rss_filter_data *prev_data)
{
	uint32_t reset_flags = prev_data->misc_reset_flags;

	prev_data->misc_reset_flags &= ~ref_data->misc_reset_flags;

	if ((reset_flags & I40E_HASH_FLOW_RESET_FLAG_REGION) &&
	    (ref_data->misc_reset_flags & I40E_HASH_FLOW_RESET_FLAG_REGION) &&
	    (prev_conf->region_queue_start != ref_conf->region_queue_start ||
	     prev_conf->region_queue_num != ref_conf->region_queue_num))
		prev_data->misc_reset_flags |= I40E_HASH_FLOW_RESET_FLAG_REGION;

	prev_data->reset_config_pctypes &= ~ref_data->reset_config_pctypes;
	prev_data->reset_symmetric_pctypes &= ~ref_data->reset_symmetric_pctypes;
}

/*
 * Recompute the owned-reset-flags metadata for every currently registered
 * RSS flow. For example, flow A sets up RSS key, flow B sets up RSS hash
 * function, and flow C sets up a different RSS key.
 *
 * When flow C registers, we need to make sure that flow A's reset flags do
 * not include RSS key, because that is now owned by flow C. This is to make
 * sure that if flow A is removed, RSS key configuration is not affected.
 */
static void
i40e_hash_reconstruct_metadata(struct i40e_rss_state *state)
{
	struct i40e_rss_flow_node *node, *prev;

	TAILQ_FOREACH(node, &state->list, next) {
		struct i40e_flow_engine_hash_flow *hash_flow =
				(struct i40e_flow_engine_hash_flow *)node->flow;

		i40e_hash_compute_reset_flags(&hash_flow->rss_conf, &node->filter_data);
	}

	TAILQ_FOREACH(node, &state->list, next) {
		struct i40e_flow_engine_hash_flow *hash_flow =
				(struct i40e_flow_engine_hash_flow *)node->flow;

		TAILQ_FOREACH(prev, &state->list, next) {
			struct i40e_flow_engine_hash_flow *prev_hash_flow;

			if (prev == node)
				break;

			prev_hash_flow = (struct i40e_flow_engine_hash_flow *)prev->flow;
			i40e_hash_invalidate_prev(&hash_flow->rss_conf, &node->filter_data,
					&prev_hash_flow->rss_conf, &prev->filter_data);
		}
	}
}

static int
i40e_hash_flow_register(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_flow_engine_hash_flow *hash_flow = (struct i40e_flow_engine_hash_flow *)flow;
	struct i40e_hash_priv *priv = flow->engine_priv;
	struct i40e_rss_state *state = priv->state;
	struct i40e_rss_flow_node *node;

	node = rte_zmalloc("i40e_rss_flow_node", sizeof(*node), 0);
	if (node == NULL) {
		return rte_flow_error_set(error, ENOMEM,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Failed to allocate RSS flow tracking node");
	}

	node->flow = flow;
	hash_flow->node = node;
	TAILQ_INSERT_TAIL(&state->list, node, next);

	i40e_hash_reconstruct_metadata(state);

	return 0;
}

static int
i40e_hash_flow_unregister(struct ci_flow *flow, struct rte_flow_error *error __rte_unused)
{
	struct i40e_flow_engine_hash_flow *hash_flow = (struct i40e_flow_engine_hash_flow *)flow;
	struct i40e_hash_priv *priv = flow->engine_priv;
	struct i40e_rss_state *state = priv->state;

	TAILQ_REMOVE(&state->list, hash_flow->node, next);
	rte_free(hash_flow->node);
	hash_flow->node = NULL;

	i40e_hash_reconstruct_metadata(state);

	return 0;
}

static int
i40e_hash_flow_install(struct ci_flow *flow,
		struct rte_flow_error *error)
{
	struct i40e_flow_engine_hash_flow *hash_flow = (struct i40e_flow_engine_hash_flow *)flow;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(flow->dev_data->dev_private);
	int ret;

	ret = i40e_hash_program_hw(pf, &hash_flow->rss_conf);
	if (ret) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, flow,
				"Failed to program hash filter");
	}
	return 0;
}

static int
i40e_hash_flow_uninstall(struct ci_flow *flow,
		struct rte_flow_error *error)
{
	struct i40e_flow_engine_hash_flow *hash_flow = (struct i40e_flow_engine_hash_flow *)flow;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(flow->dev_data->dev_private);
	int ret;

	ret = i40e_hash_deprogram_hw(pf, &hash_flow->node->filter_data);
	if (ret) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, flow,
				"Failed to reset hash filter");
	}
	return 0;
}

static int
i40e_hash_flow_query(struct ci_flow *flow,
		const struct rte_flow_action *action,
		void *data,
		struct rte_flow_error *error)
{
	struct i40e_flow_engine_hash_flow *hash_flow = (struct i40e_flow_engine_hash_flow *)flow;
	const struct i40e_rss_conf *rss_conf = &hash_flow->rss_conf;
	struct rte_flow_action_rss *rss_act = data;

	if (action->type != RTE_FLOW_ACTION_TYPE_RSS) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"Unsupported action for query");
	}

	*rss_act = (struct rte_flow_action_rss){
		.func = rss_conf->func,
		.types = rss_conf->types,
		.key_len = rss_conf->key_len,
		.key = rss_conf->key_len != 0 ? rss_conf->key : NULL,
		.queue_num = rss_conf->queue_num,
		.queue = rss_conf->queue_num != 0 ? rss_conf->queue : NULL,
	};
	return 0;
}

static int
i40e_hash_flow_engine_init(const struct ci_flow_engine *engine __rte_unused,
		struct rte_eth_dev_data *dev_data,
		void *priv)
{
	struct i40e_hash_priv *hash_priv = priv;

	hash_priv->state = i40e_rss_state_attach(dev_data);
	return 0;
}

static void
i40e_hash_flow_engine_uninit(const struct ci_flow_engine *engine __rte_unused,
		void *priv)
{
	struct i40e_hash_priv *hash_priv = priv;

	i40e_rss_state_detach(hash_priv->state);
}

static const struct ci_flow_engine_ops i40e_flow_engine_hash_pattern_ops = {
	.engine_init = i40e_hash_flow_engine_init,
	.engine_uninit = i40e_hash_flow_engine_uninit,
	.ctx_init = i40e_hash_pattern_ctx_init,
	.ctx_finalize = i40e_hash_pattern_ctx_finalize,
	.ctx_to_flow = i40e_hash_ctx_to_flow,
	.flow_register = i40e_hash_flow_register,
	.flow_unregister = i40e_hash_flow_unregister,
	.flow_install = i40e_hash_flow_install,
	.flow_uninstall = i40e_hash_flow_uninstall,
	.flow_query = i40e_hash_flow_query,
};

static const struct ci_flow_engine_ops i40e_flow_engine_hash_vlan_ops = {
	.engine_init = i40e_hash_flow_engine_init,
	.engine_uninit = i40e_hash_flow_engine_uninit,
	.ctx_init = i40e_hash_vlan_ctx_init,
	.ctx_to_flow = i40e_hash_ctx_to_flow,
	.flow_register = i40e_hash_flow_register,
	.flow_unregister = i40e_hash_flow_unregister,
	.flow_install = i40e_hash_flow_install,
	.flow_uninstall = i40e_hash_flow_uninstall,
	.flow_query = i40e_hash_flow_query,
};

static const struct ci_flow_engine_ops i40e_flow_engine_hash_empty_ops = {
	.engine_init = i40e_hash_flow_engine_init,
	.engine_uninit = i40e_hash_flow_engine_uninit,
	.ctx_init = i40e_hash_empty_ctx_init,
	.ctx_to_flow = i40e_hash_ctx_to_flow,
	.flow_register = i40e_hash_flow_register,
	.flow_unregister = i40e_hash_flow_unregister,
	.flow_install = i40e_hash_flow_install,
	.flow_uninstall = i40e_hash_flow_uninstall,
	.flow_query = i40e_hash_flow_query,
};

const struct ci_flow_engine i40e_flow_engine_hash_pattern = {
	.name = "i40e_hash_pattern",
	.ops = &i40e_flow_engine_hash_pattern_ops,
	.graph = &i40e_hash_pattern_graph,
	.ctx_size = sizeof(struct i40e_hash_ctx),
	.flow_size = sizeof(struct i40e_flow_engine_hash_flow),
	.priv_size = sizeof(struct i40e_hash_priv),
};

const struct ci_flow_engine i40e_flow_engine_hash_vlan = {
	.name = "i40e_hash_vlan",
	.ops = &i40e_flow_engine_hash_vlan_ops,
	.graph = &i40e_hash_vlan_graph,
	.ctx_size = sizeof(struct i40e_hash_ctx),
	.flow_size = sizeof(struct i40e_flow_engine_hash_flow),
	.priv_size = sizeof(struct i40e_hash_priv),
};

const struct ci_flow_engine i40e_flow_engine_hash_empty = {
	.name = "i40e_hash_empty",
	.ops = &i40e_flow_engine_hash_empty_ops,
	.ctx_size = sizeof(struct i40e_hash_ctx),
	.flow_size = sizeof(struct i40e_flow_engine_hash_flow),
	.priv_size = sizeof(struct i40e_hash_priv),
};
