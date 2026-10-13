/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#include "i40e_ethdev.h"
#include "i40e_flow.h"

#include "../common/flow_engine.h"
#include "../common/flow_check.h"
#include "../common/flow_util.h"

struct i40e_tunnel_priv {
	struct i40e_tunnel_state *state;
};

struct i40e_tunnel_ctx {
	struct ci_flow_engine_ctx base;
	struct i40e_tunnel_filter_conf filter;
};

struct i40e_tunnel_flow {
	struct rte_flow base;
	struct i40e_tunnel_filter_conf filter;
	struct i40e_tunnel_filter_match_key match_key;
};

/*
 * For some tunnel modes, we replace NIC default filters with custom ones. This
 * has a few side effects we need to be mindful of:
 *
 * - If there is a filter that uses the "replaced" filter ID, it will not
 *   function as expected
 * - Replacing a filter consumes its default ID until device reset
 * - All replacement-backed tunnel flows must use the same recipe until reset
 *
 * The following table of declarations defines replacement filters for various
 * tunnel flows we can install.
 */

#define I40E_REPLACE_VALID(value) \
	((value) | I40E_AQC_REPLACE_CLOUD_CMD_INPUT_VALIDATED)

#define I40E_REPLACE_TR_WORD0 0x48
#define I40E_REPLACE_PORT_TR_WORD0 0x49
#define I40E_REPLACE_DIRECTION_WORD0 0x41
#define I40E_REPLACE_INGRESS_WORD0 0x80

#define I40E_TR_VXLAN_GRE_KEY_MASK	0x4
#define I40E_TR_GENEVE_KEY_MASK		0x8
#define I40E_TR_GENERIC_UDP_TUNNEL_MASK	0x40
#define I40E_TR_GRE_KEY_MASK		0x400
#define I40E_TR_GRE_KEY_WITH_XSUM_MASK	0x800
#define I40E_TR_GRE_NO_KEY_MASK		0x8000

#define I40E_TR_L4_TYPE_TCP  0x2
#define I40E_TR_L4_TYPE_UDP  0x4
#define I40E_TR_L4_TYPE_SCTP 0x8

struct i40e_tunnel_filter_recipe {
	enum i40e_tunnel_replacement_type type;
	const struct i40e_tunnel_filter_replacement *steps;
	uint8_t nb_steps;
};

static const struct i40e_tunnel_filter_replacement i40e_tunnel_mpls_replacements[] = {
	{
		.old_filter_type = I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_IMAC,
		.new_filter_type = I40E_AQC_ADD_L1_FILTER_0X11,
		.data = {
			[0] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_TEID_WORD0),
			[2] = 0xff,
			[3] = 0xff,
			[4] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_TEID_WORD1),
			[7] = 0xf0,
			[8] = I40E_REPLACE_VALID(I40E_REPLACE_TR_WORD0),
			[10] = I40E_TR_VXLAN_GRE_KEY_MASK |
				I40E_TR_GENEVE_KEY_MASK | I40E_TR_GENERIC_UDP_TUNNEL_MASK,
			[11] = (I40E_TR_GRE_KEY_MASK | I40E_TR_GRE_KEY_WITH_XSUM_MASK |
				I40E_TR_GRE_NO_KEY_MASK) >> 8,
		},
	},
	{
		.valid_flags = I40E_AQC_REPLACE_CLOUD_FILTER |
			I40E_AQC_MIRROR_CLOUD_FILTER,
		.old_filter_type = I40E_AQC_ADD_CLOUD_FILTER_IIP,
		.new_filter_type = I40E_AQC_ADD_CLOUD_FILTER_0X11,
		.data = {
			[0] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG),
			[4] = I40E_REPLACE_VALID(I40E_AQC_ADD_L1_FILTER_0X11),
		},
	},
	{
		.valid_flags = I40E_AQC_REPLACE_CLOUD_FILTER |
			I40E_AQC_MIRROR_CLOUD_FILTER,
		.old_filter_type = I40E_AQC_ADD_CLOUD_FILTER_IMAC,
		.new_filter_type = I40E_AQC_ADD_CLOUD_FILTER_0X12,
		.data = {
			[0] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG),
			[4] = I40E_REPLACE_VALID(I40E_AQC_ADD_L1_FILTER_0X11),
		},
	},
};

static const struct i40e_tunnel_filter_replacement i40e_tunnel_gtp_replacements[] = {
	{
		.old_filter_type = I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_IMAC,
		.new_filter_type = I40E_AQC_ADD_L1_FILTER_0X12,
		.tr_bit = I40E_REPLACE_VALID(I40E_AQC_NEW_TR_22),
		.data = {
			[0] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_TEID_WORD0),
			[2] = 0xff,
			[3] = 0xff,
			[4] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_TEID_WORD1),
			[6] = 0xff,
			[7] = 0xff,
		},
	},
	{
		.old_filter_type = I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_TUNNLE_KEY,
		.new_filter_type = I40E_AQC_ADD_L1_FILTER_0X13,
		.tr_bit = I40E_REPLACE_VALID(I40E_AQC_NEW_TR_21),
		.data = {
			[0] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_TEID_WORD0),
			[2] = 0xff,
			[3] = 0xff,
			[4] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_TEID_WORD1),
			[6] = 0xff,
			[7] = 0xff,
		},
	},
	{
		.valid_flags = I40E_AQC_REPLACE_CLOUD_FILTER,
		.old_filter_type = I40E_AQC_ADD_CLOUD_FILTER_IMAC_IVLAN,
		.new_filter_type = I40E_AQC_ADD_CLOUD_FILTER_0X11,
		.data = {
			[0] = I40E_REPLACE_VALID(I40E_AQC_ADD_L1_FILTER_0X12),
			[4] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG),
		},
	},
	{
		.valid_flags = I40E_AQC_REPLACE_CLOUD_FILTER,
		.old_filter_type = I40E_AQC_ADD_CLOUD_FILTER_IMAC_IVLAN_TEN_ID,
		.new_filter_type = I40E_AQC_ADD_CLOUD_FILTER_0X12,
		.data = {
			[0] = I40E_REPLACE_VALID(I40E_AQC_ADD_L1_FILTER_0X13),
			[4] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG),
		},
	},
};

static const struct i40e_tunnel_filter_replacement i40e_tunnel_qinq_replacements[] = {
	{
		.old_filter_type = I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG_IVLAN,
		.new_filter_type = I40E_AQC_ADD_L1_FILTER_0X10,
		.data = {
			[0] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_VLAN),
			[2] = 0xff,
			[3] = 0x0f,
			[4] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_INNER_VLAN),
			[6] = 0xff,
			[7] = 0x0f,
		},
	},
	{
		.valid_flags = I40E_AQC_REPLACE_CLOUD_FILTER,
		.old_filter_type = I40E_AQC_ADD_CLOUD_FILTER_OIP,
		.new_filter_type = I40E_AQC_ADD_CLOUD_FILTER_0X10,
		.data = {
			[0] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG),
			[4] = I40E_REPLACE_VALID(I40E_AQC_ADD_L1_FILTER_0X10),
		},
	},
};

static const struct i40e_tunnel_filter_replacement
i40e_tunnel_src_port_replacements[] = {
	{
		.old_filter_type = I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_TUNNLE_KEY,
		.new_filter_type = I40E_AQC_ADD_L1_FILTER_0X11,
		.data = {
			[0] = I40E_REPLACE_VALID(I40E_REPLACE_DIRECTION_WORD0),
			[3] = I40E_REPLACE_INGRESS_WORD0,
			[4] = I40E_REPLACE_VALID(I40E_REPLACE_PORT_TR_WORD0),
			[6] = I40E_TR_L4_TYPE_UDP | I40E_TR_L4_TYPE_TCP |
				I40E_TR_L4_TYPE_SCTP,
			[8] = I40E_REPLACE_VALID(
				I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_SRC_PORT),
			[10] = 0xff,
			[11] = 0xff,
		},
	},
	{
		.valid_flags = I40E_AQC_REPLACE_CLOUD_FILTER,
		.old_filter_type = I40E_AQC_ADD_CLOUD_FILTER_IIP,
		.new_filter_type = I40E_AQC_ADD_CLOUD_FILTER_0X11,
		.data = {
			[0] = I40E_REPLACE_VALID(
				I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG),
			[4] = I40E_REPLACE_VALID(I40E_AQC_ADD_CLOUD_FILTER_0X11),
		},
	},
};

static const struct i40e_tunnel_filter_replacement
i40e_tunnel_dst_port_replacements[] = {
	{
		.old_filter_type = I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG_IVLAN,
		.new_filter_type = I40E_AQC_ADD_L1_FILTER_0X10,
		.data = {
			[0] = I40E_REPLACE_VALID(I40E_REPLACE_DIRECTION_WORD0),
			[3] = I40E_REPLACE_INGRESS_WORD0,
			[4] = I40E_REPLACE_VALID(I40E_REPLACE_PORT_TR_WORD0),
			[6] = I40E_TR_L4_TYPE_UDP | I40E_TR_L4_TYPE_TCP |
				I40E_TR_L4_TYPE_SCTP,
			[8] = I40E_REPLACE_VALID(
				I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_DST_PORT),
			[10] = 0xff,
			[11] = 0xff,
		},
	},
	{
		.valid_flags = I40E_AQC_REPLACE_CLOUD_FILTER,
		.old_filter_type = I40E_AQC_ADD_CLOUD_FILTER_OIP,
		.new_filter_type = I40E_AQC_ADD_CLOUD_FILTER_0X10,
		.data = {
			[0] = I40E_REPLACE_VALID(I40E_AQC_REPLACE_CLOUD_CMD_INPUT_FV_STAG),
			[4] = I40E_REPLACE_VALID(I40E_AQC_ADD_CLOUD_FILTER_0X10),
		},
	},
};

static const struct i40e_tunnel_filter_recipe i40e_tunnel_mpls_recipe = {
	.type = I40E_TUNNEL_REPLACEMENT_MPLS,
	.steps = i40e_tunnel_mpls_replacements,
	.nb_steps = RTE_DIM(i40e_tunnel_mpls_replacements),
};

static const struct i40e_tunnel_filter_recipe i40e_tunnel_gtp_recipe = {
	.type = I40E_TUNNEL_REPLACEMENT_GTP,
	.steps = i40e_tunnel_gtp_replacements,
	.nb_steps = RTE_DIM(i40e_tunnel_gtp_replacements),
};

static const struct i40e_tunnel_filter_recipe i40e_tunnel_qinq_recipe = {
	.type = I40E_TUNNEL_REPLACEMENT_QINQ,
	.steps = i40e_tunnel_qinq_replacements,
	.nb_steps = RTE_DIM(i40e_tunnel_qinq_replacements),
};

static const struct i40e_tunnel_filter_recipe i40e_tunnel_src_port_recipe = {
	.type = I40E_TUNNEL_REPLACEMENT_SRC_PORT,
	.steps = i40e_tunnel_src_port_replacements,
	.nb_steps = RTE_DIM(i40e_tunnel_src_port_replacements),
};

static const struct i40e_tunnel_filter_recipe i40e_tunnel_dst_port_recipe = {
	.type = I40E_TUNNEL_REPLACEMENT_DST_PORT,
	.steps = i40e_tunnel_dst_port_replacements,
	.nb_steps = RTE_DIM(i40e_tunnel_dst_port_replacements),
};

static const struct i40e_tunnel_filter_recipe *
i40e_tunnel_filter_recipe_get(const struct i40e_tunnel_filter_conf *filter)
{
	switch (filter->tunnel_type) {
	case I40E_TUNNEL_TYPE_MPLSoUDP:
	case I40E_TUNNEL_TYPE_MPLSoGRE:
		return &i40e_tunnel_mpls_recipe;
	case I40E_TUNNEL_TYPE_GTPC:
	case I40E_TUNNEL_TYPE_GTPU:
		return &i40e_tunnel_gtp_recipe;
	case I40E_TUNNEL_TYPE_QINQ:
		return &i40e_tunnel_qinq_recipe;
	case I40E_CLOUD_TYPE_UDP:
	case I40E_CLOUD_TYPE_TCP:
	case I40E_CLOUD_TYPE_SCTP:
		if (filter->l4_port_type == I40E_L4_PORT_TYPE_SRC)
			return &i40e_tunnel_src_port_recipe;
		return &i40e_tunnel_dst_port_recipe;
	default:
		return NULL;
	}
}

static int
i40e_tunnel_filter_prepare_hw(struct i40e_pf *pf,
		const struct i40e_tunnel_filter_conf *filter)
{
	const struct i40e_tunnel_filter_recipe *recipe;
	struct i40e_tunnel_state *state = &pf->tunnel_state;
	uint8_t i;
	int ret;

	/* this filter may not require configuration */
	recipe = i40e_tunnel_filter_recipe_get(filter);
	if (recipe == NULL)
		return 0;

	/* filter may have already been configured */
	if (state->replacement_state == I40E_TUNNEL_FILTER_REPLACED)
		return 0;

	for (i = 0; i < recipe->nb_steps; i++) {
		const struct i40e_tunnel_filter_replacement *replacement =
			&recipe->steps[i];

		ret = i40e_tunnel_filter_replace(pf, replacement);
		if (ret != I40E_SUCCESS)
			return ret;
	}

	state->replacement_state = I40E_TUNNEL_FILTER_REPLACED;

	return 0;
}

static int
i40e_check_tunnel_filter_type(uint8_t filter_type)
{
	const uint16_t i40e_supported_tunnel_filter_types[] = {
		RTE_ETH_TUNNEL_FILTER_IMAC | RTE_ETH_TUNNEL_FILTER_TENID |
		RTE_ETH_TUNNEL_FILTER_IVLAN,
		RTE_ETH_TUNNEL_FILTER_IMAC | RTE_ETH_TUNNEL_FILTER_IVLAN,
		RTE_ETH_TUNNEL_FILTER_IMAC | RTE_ETH_TUNNEL_FILTER_TENID,
		RTE_ETH_TUNNEL_FILTER_OMAC | RTE_ETH_TUNNEL_FILTER_TENID |
		RTE_ETH_TUNNEL_FILTER_IMAC,
		RTE_ETH_TUNNEL_FILTER_IMAC,
	};
	uint8_t i;

	for (i = 0; i < RTE_DIM(i40e_supported_tunnel_filter_types); i++) {
		if (filter_type == i40e_supported_tunnel_filter_types[i])
			return 0;
	}
	return -1;
}

/**
 * QinQ tunnel filter graph implementation
 * Pattern: START -> ETH -> OUTER_VLAN -> INNER_VLAN -> END
 */
enum i40e_tunnel_qinq_node_id {
	I40E_TUNNEL_QINQ_NODE_START = FLOW_GRAPH_NODE_FIRST,
	I40E_TUNNEL_QINQ_NODE_ETH,
	I40E_TUNNEL_QINQ_NODE_OUTER_VLAN,
	I40E_TUNNEL_QINQ_NODE_INNER_VLAN,
	I40E_TUNNEL_QINQ_NODE_END,
	I40E_TUNNEL_QINQ_NODE_MAX,
};

static int
i40e_tunnel_node_vlan_validate(const void *ctx __rte_unused, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_vlan *vlan_mask = ci_flow_item_mask(item);

	/* matching eth proto not supported */
	if (vlan_mask->hdr.eth_proto != 0) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid VLAN mask");
	}

	/* VLAN TCI is optional, but if specified, must be fully masked */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&vlan_mask->hdr.vlan_tci)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid VLAN mask");
	}

	return 0;
}

static int
i40e_tunnel_node_qinq_vlan_validate(const void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_vlan *vlan_mask = ci_flow_item_mask(item);
	int ret;

	ret = i40e_tunnel_node_vlan_validate(ctx, item, error);
	if (ret != 0)
		return ret;

	/* QinQ filter always matches both VLAN IDs, so TCI is mandatory */
	if (CI_FIELD_IS_ZERO(&vlan_mask->hdr.vlan_tci)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid VLAN mask");
	}

	return 0;
}

/* common VLAN processing for both outer and inner VLAN nodes */
static int
i40e_tunnel_node_vlan_process(struct i40e_tunnel_ctx *tunnel_ctx,
		const struct rte_flow_item *item, bool is_inner)
{
	const struct rte_flow_item_vlan *vlan_spec = item->spec;
	const struct rte_flow_item_vlan *vlan_mask = ci_flow_item_mask(item);
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	/* VLAN TCI not requested */
	if (CI_FIELD_IS_ZERO(&vlan_mask->hdr.vlan_tci))
		return 0;

	/* Store the VLAN ID and set filter flag */
	if (is_inner) {
		tunnel_filter->inner_vlan = rte_be_to_cpu_16(vlan_spec->hdr.vlan_tci);
		tunnel_filter->filter_type |= RTE_ETH_TUNNEL_FILTER_IVLAN;
	} else {
		tunnel_filter->outer_vlan = rte_be_to_cpu_16(vlan_spec->hdr.vlan_tci);
		/* no special flag for outer VLAN matching */
	}

	return 0;
}

static int
i40e_tunnel_node_outer_vlan_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;

	return i40e_tunnel_node_vlan_process(tunnel_ctx, item, false);
}

static int
i40e_tunnel_node_inner_vlan_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;

	return i40e_tunnel_node_vlan_process(tunnel_ctx, item, true);
}

static int
i40e_tunnel_qinq_node_end_process(void *ctx, const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	tunnel_filter->tunnel_type = I40E_TUNNEL_TYPE_QINQ;

	/* QinQ filter is not meant to set this flag */
	tunnel_filter->filter_type &= ~RTE_ETH_TUNNEL_FILTER_IVLAN;

	return 0;
}

static const struct flow_graph i40e_tunnel_qinq_graph = {
	.nodes = (struct flow_graph_node[]) {
		[I40E_TUNNEL_QINQ_NODE_START] = {
			.name = "START",
		},
		[I40E_TUNNEL_QINQ_NODE_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[I40E_TUNNEL_QINQ_NODE_OUTER_VLAN] = {
			.name = "OUTER_VLAN",
			.type = RTE_FLOW_ITEM_TYPE_VLAN,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_qinq_vlan_validate,
			.process = i40e_tunnel_node_outer_vlan_process,
		},
		[I40E_TUNNEL_QINQ_NODE_INNER_VLAN] = {
			.name = "INNER_VLAN",
			.type = RTE_FLOW_ITEM_TYPE_VLAN,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_qinq_vlan_validate,
			.process = i40e_tunnel_node_inner_vlan_process,
		},
		[I40E_TUNNEL_QINQ_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
			.process = i40e_tunnel_qinq_node_end_process,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[I40E_TUNNEL_QINQ_NODE_START] = {
			.next = (size_t[]) {
				I40E_TUNNEL_QINQ_NODE_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_QINQ_NODE_ETH] = {
			.next = (size_t[]) {
				I40E_TUNNEL_QINQ_NODE_OUTER_VLAN,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_QINQ_NODE_OUTER_VLAN] = {
			.next = (size_t[]) {
				I40E_TUNNEL_QINQ_NODE_INNER_VLAN,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_QINQ_NODE_INNER_VLAN] = {
			.next = (size_t[]) {
				I40E_TUNNEL_QINQ_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

/**
 * VXLAN tunnel filter graph implementation
 * Pattern: START -> ETH -> (IPv4 | IPv6) -> UDP -> VXLAN -> ETH -> [VLAN] -> END
 */
enum i40e_tunnel_vxlan_node_id {
	I40E_TUNNEL_VXLAN_NODE_START  = FLOW_GRAPH_NODE_FIRST,
	I40E_TUNNEL_VXLAN_NODE_OUTER_ETH,
	I40E_TUNNEL_VXLAN_NODE_IPV4,
	I40E_TUNNEL_VXLAN_NODE_IPV6,
	I40E_TUNNEL_VXLAN_NODE_UDP,
	I40E_TUNNEL_VXLAN_NODE_VXLAN,
	I40E_TUNNEL_VXLAN_NODE_INNER_ETH,
	I40E_TUNNEL_VXLAN_NODE_INNER_VLAN,
	I40E_TUNNEL_VXLAN_NODE_END,
	I40E_TUNNEL_VXLAN_NODE_MAX,
};

static int
i40e_tunnel_node_eth_validate(const void *ctx __rte_unused, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *eth_mask = ci_flow_item_mask(item);

	/* spec/mask is optional */
	if (eth_mask == NULL)
		return 0;

	/* matching eth type not supported */
	if (eth_mask->hdr.ether_type) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid ETH mask");
	}

	/* source MAC must be fully unmasked */
	if (!CI_FIELD_IS_ZERO(&eth_mask->hdr.src_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid ETH mask");
	}
	/* destination MAC must be fully masked */
	if (!CI_FIELD_IS_MASKED(&eth_mask->hdr.dst_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid ETH mask");
	}

	return 0;
}

static int
i40e_tunnel_eth_process(struct i40e_tunnel_ctx *tunnel_ctx,
		const struct rte_flow_item *item, bool is_inner)
{
	const struct rte_flow_item_eth *eth_spec = item->spec;
	const struct rte_flow_item_eth *eth_mask = ci_flow_item_mask(item);
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	/* eth spec/mask is optional */
	if (eth_mask == NULL)
		return 0;

	/* Store the MAC addresses and set filter flags */
	if (is_inner) {
		memcpy(&tunnel_filter->inner_mac, &eth_spec->hdr.dst_addr,
				sizeof(tunnel_filter->inner_mac));
		tunnel_filter->filter_type |= RTE_ETH_TUNNEL_FILTER_IMAC;
	} else {
		memcpy(&tunnel_filter->outer_mac, &eth_spec->hdr.dst_addr,
				sizeof(tunnel_filter->outer_mac));
		tunnel_filter->filter_type |= RTE_ETH_TUNNEL_FILTER_OMAC;
	}
	return 0;
}

static int
i40e_tunnel_node_outer_eth_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;

	return i40e_tunnel_eth_process(tunnel_ctx, item, false);
}

static int
i40e_tunnel_node_inner_eth_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;

	return i40e_tunnel_eth_process(tunnel_ctx, item, true);
}

static int
i40e_tunnel_node_ipv4_process(void *ctx, const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	tunnel_filter->ip_type = I40E_TUNNEL_IPTYPE_IPV4;

	return 0;
}

static int
i40e_tunnel_node_ipv6_process(void *ctx, const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	tunnel_filter->ip_type = I40E_TUNNEL_IPTYPE_IPV6;

	return 0;
}

static int
i40e_tunnel_node_vxlan_validate(const void *ctx __rte_unused,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_vxlan *vxlan_mask = ci_flow_item_mask(item);

	/* spec/mask are optional */
	if (vxlan_mask == NULL)
		return 0;

	/* VNI must be fully masked */
	if (!CI_FIELD_IS_MASKED(&vxlan_mask->hdr.vni)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid VXLAN mask");
	}
	return 0;
}

static int
i40e_tunnel_node_vxlan_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	const struct rte_flow_item_vxlan *vxlan_spec = item->spec;
	const struct rte_flow_item_vxlan *vxlan_mask = ci_flow_item_mask(item);
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	/* spec/mask are optional */
	if (vxlan_mask == NULL)
		return 0;

	/* Store the VNI and set filter flag */
	tunnel_filter->tenant_id = ci_be24_to_cpu(vxlan_spec->hdr.vni);
	tunnel_filter->filter_type |= RTE_ETH_TUNNEL_FILTER_TENID;

	return 0;
}

static int
i40e_tunnel_node_end_validate(const void *ctx,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	const struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	/* this shouldn't happen but check this just in case */
	if (i40e_check_tunnel_filter_type(tunnel_filter->filter_type) != 0) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid tunnel filter configuration");
	}
	return 0;
}

static int
i40e_tunnel_vxlan_node_end_process(void *ctx, const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	tunnel_filter->tunnel_type = I40E_TUNNEL_TYPE_VXLAN;

	return 0;
}

static const struct flow_graph i40e_tunnel_vxlan_graph = {
	.nodes = (struct flow_graph_node[]) {
		[I40E_TUNNEL_VXLAN_NODE_START] = {
			.name = "START",
		},
		[I40E_TUNNEL_VXLAN_NODE_OUTER_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
			               FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_eth_validate,
			.process = i40e_tunnel_node_outer_eth_process,
		},
		[I40E_TUNNEL_VXLAN_NODE_IPV4] = {
			.name = "IPv4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_tunnel_node_ipv4_process,
		},
		[I40E_TUNNEL_VXLAN_NODE_IPV6] = {
			.name = "IPv6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_tunnel_node_ipv6_process,
		},
		[I40E_TUNNEL_VXLAN_NODE_UDP] = {
			.name = "UDP",
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[I40E_TUNNEL_VXLAN_NODE_VXLAN] = {
			.name = "VXLAN",
			.type = RTE_FLOW_ITEM_TYPE_VXLAN,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
			               FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_vxlan_validate,
			.process = i40e_tunnel_node_vxlan_process,
		},
		[I40E_TUNNEL_VXLAN_NODE_INNER_ETH] = {
			.name = "INNER_ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
			               FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_eth_validate,
			.process = i40e_tunnel_node_inner_eth_process,
		},
		[I40E_TUNNEL_VXLAN_NODE_INNER_VLAN] = {
			.name = "INNER_VLAN",
			.type = RTE_FLOW_ITEM_TYPE_VLAN,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_vlan_validate,
			.process = i40e_tunnel_node_inner_vlan_process,
		},
		[I40E_TUNNEL_VXLAN_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
			.validate = i40e_tunnel_node_end_validate,
			.process = i40e_tunnel_vxlan_node_end_process
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[I40E_TUNNEL_VXLAN_NODE_START] = {
			.next = (size_t[]) {
				I40E_TUNNEL_VXLAN_NODE_OUTER_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_VXLAN_NODE_OUTER_ETH] = {
			.next = (size_t[]) {
				I40E_TUNNEL_VXLAN_NODE_IPV4,
				I40E_TUNNEL_VXLAN_NODE_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_VXLAN_NODE_IPV4] = {
			.next = (size_t[]) {
				I40E_TUNNEL_VXLAN_NODE_UDP,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_VXLAN_NODE_IPV6] = {
			.next = (size_t[]) {
				I40E_TUNNEL_VXLAN_NODE_UDP,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_VXLAN_NODE_UDP] = {
			.next = (size_t[]) {
				I40E_TUNNEL_VXLAN_NODE_VXLAN,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_VXLAN_NODE_VXLAN] = {
			.next = (size_t[]) {
				I40E_TUNNEL_VXLAN_NODE_INNER_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_VXLAN_NODE_INNER_ETH] = {
			.next = (size_t[]) {
				I40E_TUNNEL_VXLAN_NODE_INNER_VLAN,
				I40E_TUNNEL_VXLAN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_VXLAN_NODE_INNER_VLAN] = {
			.next = (size_t[]) {
				I40E_TUNNEL_VXLAN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

/**
 * NVGRE tunnel filter graph implementation
 * Pattern: START -> ETH -> (IPv4 | IPv6) -> NVGRE -> ETH -> [VLAN] -> END
 */
enum i40e_tunnel_nvgre_node_id {
	I40E_TUNNEL_NVGRE_NODE_START  = FLOW_GRAPH_NODE_FIRST,
	I40E_TUNNEL_NVGRE_NODE_OUTER_ETH,
	I40E_TUNNEL_NVGRE_NODE_IPV4,
	I40E_TUNNEL_NVGRE_NODE_IPV6,
	I40E_TUNNEL_NVGRE_NODE_NVGRE,
	I40E_TUNNEL_NVGRE_NODE_INNER_ETH,
	I40E_TUNNEL_NVGRE_NODE_INNER_VLAN,
	I40E_TUNNEL_NVGRE_NODE_END,
	I40E_TUNNEL_NVGRE_NODE_MAX,
};

static int
i40e_tunnel_node_nvgre_validate(const void *ctx __rte_unused,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_nvgre *nvgre_spec = item->spec;
	const struct rte_flow_item_nvgre *nvgre_mask = ci_flow_item_mask(item);

	/* spec/mask are optional */
	if (nvgre_mask == NULL)
		return 0;

	/* TNI must be fully masked */
	if (!CI_FIELD_IS_MASKED(&nvgre_mask->tni)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
				"Invalid NVGRE mask");
	}
	/* protocol must either be unmasked or fully masked */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&nvgre_mask->protocol)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
				"Invalid NVGRE mask");
	}
	/* reserved/version field must either be unmasked or fully masked */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&nvgre_mask->c_k_s_rsvd0_ver)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
				"Invalid NVGRE mask");
	}
	/* if reserved/version field is masked, it must be set to 0x2000 */
	if (nvgre_mask->c_k_s_rsvd0_ver &&
			nvgre_spec->c_k_s_rsvd0_ver != rte_cpu_to_be_16(0x2000)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
				"Invalid NVGRE spec");
	}
	/* if protocol field is masked, it must be set to 0x6558 */
	if (nvgre_mask->protocol &&
			nvgre_spec->protocol != rte_cpu_to_be_16(0x6558)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
				"Invalid NVGRE spec");
	}
	return 0;
}

static int
i40e_tunnel_node_nvgre_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	const struct rte_flow_item_nvgre *nvgre_spec = item->spec;
	const struct rte_flow_item_nvgre *nvgre_mask = ci_flow_item_mask(item);
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	/* spec/mask are optional */
	if (nvgre_mask == NULL)
		return 0;

	/* Store the VNI and set filter flag */
	tunnel_filter->tenant_id = ci_be24_to_cpu(nvgre_spec->tni);
	tunnel_filter->filter_type |= RTE_ETH_TUNNEL_FILTER_TENID;

	return 0;
}

static int
i40e_tunnel_node_nvgre_end_process(void *ctx, const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	tunnel_filter->tunnel_type = I40E_TUNNEL_TYPE_NVGRE;

	return 0;
}

static const struct flow_graph i40e_tunnel_nvgre_graph = {
	.nodes = (struct flow_graph_node[]) {
		[I40E_TUNNEL_NVGRE_NODE_START] = {
			.name = "START",
		},
		[I40E_TUNNEL_NVGRE_NODE_OUTER_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
			               FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_eth_validate,
			.process = i40e_tunnel_node_outer_eth_process,
		},
		[I40E_TUNNEL_NVGRE_NODE_IPV4] = {
			.name = "IPv4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_tunnel_node_ipv4_process,
		},
		[I40E_TUNNEL_NVGRE_NODE_IPV6] = {
			.name = "IPv6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_tunnel_node_ipv6_process,
		},
		[I40E_TUNNEL_NVGRE_NODE_NVGRE] = {
			.name = "NVGRE",
			.type = RTE_FLOW_ITEM_TYPE_NVGRE,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
			               FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_nvgre_validate,
			.process = i40e_tunnel_node_nvgre_process,
		},
		[I40E_TUNNEL_NVGRE_NODE_INNER_ETH] = {
			.name = "INNER_ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY |
			               FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_eth_validate,
			.process = i40e_tunnel_node_inner_eth_process,
		},
		[I40E_TUNNEL_NVGRE_NODE_INNER_VLAN] = {
			.name = "INNER_VLAN",
			.type = RTE_FLOW_ITEM_TYPE_VLAN,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_vlan_validate,
			.process = i40e_tunnel_node_inner_vlan_process,
		},
		[I40E_TUNNEL_NVGRE_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
			.validate = i40e_tunnel_node_end_validate,
			.process = i40e_tunnel_node_nvgre_end_process
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[I40E_TUNNEL_NVGRE_NODE_START] = {
			.next = (size_t[]) {
				I40E_TUNNEL_NVGRE_NODE_OUTER_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_NVGRE_NODE_OUTER_ETH] = {
			.next = (size_t[]) {
				I40E_TUNNEL_NVGRE_NODE_IPV4,
				I40E_TUNNEL_NVGRE_NODE_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_NVGRE_NODE_IPV4] = {
			.next = (size_t[]) {
				I40E_TUNNEL_NVGRE_NODE_NVGRE,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_NVGRE_NODE_IPV6] = {
			.next = (size_t[]) {
				I40E_TUNNEL_NVGRE_NODE_NVGRE,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_NVGRE_NODE_NVGRE] = {
			.next = (size_t[]) {
				I40E_TUNNEL_NVGRE_NODE_INNER_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_NVGRE_NODE_INNER_ETH] = {
			.next = (size_t[]) {
				I40E_TUNNEL_NVGRE_NODE_INNER_VLAN,
				I40E_TUNNEL_NVGRE_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_NVGRE_NODE_INNER_VLAN] = {
			.next = (size_t[]) {
				I40E_TUNNEL_NVGRE_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

/**
 * MPLS tunnel filter graph implementation
 * Pattern: START -> ETH -> (IPv4 | IPv6) -> (UDP | GRE) -> MPLS -> END
 */
enum i40e_tunnel_mpls_node_id {
	I40E_TUNNEL_MPLS_NODE_START  = FLOW_GRAPH_NODE_FIRST,
	I40E_TUNNEL_MPLS_NODE_ETH,
	I40E_TUNNEL_MPLS_NODE_IPV4,
	I40E_TUNNEL_MPLS_NODE_IPV6,
	I40E_TUNNEL_MPLS_NODE_UDP,
	I40E_TUNNEL_MPLS_NODE_GRE,
	I40E_TUNNEL_MPLS_NODE_MPLS,
	I40E_TUNNEL_MPLS_NODE_END,
	I40E_TUNNEL_MPLS_NODE_MAX,
};

static int
i40e_tunnel_mpls_node_udp_process(void *ctx, const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	tunnel_filter->tunnel_type = I40E_TUNNEL_TYPE_MPLSoUDP;

	return 0;
}

static int
i40e_tunnel_mpls_node_gre_process(void *ctx, const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	tunnel_filter->tunnel_type = I40E_TUNNEL_TYPE_MPLSoGRE;

	return 0;
}

static int
i40e_tunnel_node_mpls_validate(const void *ctx __rte_unused,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_mpls *mpls_mask = ci_flow_item_mask(item);
	const uint8_t label_mask[3] = {0xFF, 0xFF, 0xF0};

	/* MPLS label and TC must be fully masked */
	if (memcmp(mpls_mask->label_tc_s, label_mask, 3)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid MPLS mask");
	}
	return 0;
}

static int
i40e_tunnel_node_mpls_process(void *ctx, const struct rte_flow_item *item __rte_unused,
		struct rte_flow_error *error __rte_unused)
{
	const struct rte_flow_item_mpls *mpls_spec = item->spec;
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	tunnel_filter->tenant_id = ci_be24_to_cpu(mpls_spec->label_tc_s) >> 4;

	return 0;
}

static const struct flow_graph i40e_tunnel_mpls_graph = {
	.nodes = (struct flow_graph_node[]) {
		[I40E_TUNNEL_MPLS_NODE_START] = {
			.name = "START",
		},
		[I40E_TUNNEL_MPLS_NODE_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[I40E_TUNNEL_MPLS_NODE_IPV4] = {
			.name = "IPv4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_tunnel_node_ipv4_process,
		},
		[I40E_TUNNEL_MPLS_NODE_IPV6] = {
			.name = "IPv6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_tunnel_node_ipv6_process,
		},
		[I40E_TUNNEL_MPLS_NODE_UDP] = {
			.name = "UDP",
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_tunnel_mpls_node_udp_process,
		},
		[I40E_TUNNEL_MPLS_NODE_GRE] = {
			.name = "GRE",
			.type = RTE_FLOW_ITEM_TYPE_GRE,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_tunnel_mpls_node_gre_process,
		},
		[I40E_TUNNEL_MPLS_NODE_MPLS] = {
			.name = "MPLS",
			.type = RTE_FLOW_ITEM_TYPE_MPLS,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_mpls_validate,
			.process = i40e_tunnel_node_mpls_process,
		},
		[I40E_TUNNEL_MPLS_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[I40E_TUNNEL_MPLS_NODE_START] = {
			.next = (size_t[]) {
				I40E_TUNNEL_MPLS_NODE_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_MPLS_NODE_ETH] = {
			.next = (size_t[]) {
				I40E_TUNNEL_MPLS_NODE_IPV4,
				I40E_TUNNEL_MPLS_NODE_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_MPLS_NODE_IPV4] = {
			.next = (size_t[]) {
				I40E_TUNNEL_MPLS_NODE_UDP,
				I40E_TUNNEL_MPLS_NODE_GRE,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_MPLS_NODE_IPV6] = {
			.next = (size_t[]) {
				I40E_TUNNEL_MPLS_NODE_UDP,
				I40E_TUNNEL_MPLS_NODE_GRE,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_MPLS_NODE_UDP] = {
			.next = (size_t[]) {
				I40E_TUNNEL_MPLS_NODE_MPLS,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_MPLS_NODE_GRE] = {
			.next = (size_t[]) {
				I40E_TUNNEL_MPLS_NODE_MPLS,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_MPLS_NODE_MPLS] = {
			.next = (size_t[]) {
				I40E_TUNNEL_MPLS_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

/**
 * GTP tunnel filter graph implementation
 * Pattern: START -> ETH -> (IPv4 | IPv6) -> UDP -> (GTPC | GTPU) -> END
 */
enum i40e_tunnel_gtp_node_id {
	I40E_TUNNEL_GTP_NODE_START  = FLOW_GRAPH_NODE_FIRST,
	I40E_TUNNEL_GTP_NODE_ETH,
	I40E_TUNNEL_GTP_NODE_IPV4,
	I40E_TUNNEL_GTP_NODE_IPV6,
	I40E_TUNNEL_GTP_NODE_UDP,
	I40E_TUNNEL_GTP_NODE_GTPC,
	I40E_TUNNEL_GTP_NODE_GTPU,
	I40E_TUNNEL_GTP_NODE_END,
	I40E_TUNNEL_GTP_NODE_MAX,
};

static int
i40e_tunnel_node_gtp_validate(const void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_gtp *gtp_mask = ci_flow_item_mask(item);
	const struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	const struct rte_eth_dev_data *dev_data = tunnel_ctx->base.dev_data;
	const struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev_data->dev_private);

	/* does HW support GTP? */
	if (!pf->gtp_support) {
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"GTP not supported");
	}

	/* reject unsupported fields */
	if (gtp_mask->hdr.gtp_hdr_info ||
	    gtp_mask->hdr.msg_type ||
	    gtp_mask->hdr.plen) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid GTP mask");
	}

	/* teid must be fully masked */
	if (!CI_FIELD_IS_MASKED(&gtp_mask->hdr.teid)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid GTP mask");
	}
	return 0;
}

static int
i40e_tunnel_node_gtp_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_gtp *gtp_spec = item->spec;
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;

	if (item->type == RTE_FLOW_ITEM_TYPE_GTPC)
		tunnel_filter->tunnel_type = I40E_TUNNEL_TYPE_GTPC;
	else if (item->type == RTE_FLOW_ITEM_TYPE_GTPU)
		tunnel_filter->tunnel_type = I40E_TUNNEL_TYPE_GTPU;
	else
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid GTP item type");

	tunnel_filter->tenant_id = rte_be_to_cpu_32(gtp_spec->hdr.teid);

	return 0;
}

static const struct flow_graph i40e_tunnel_gtp_graph = {
	.nodes = (struct flow_graph_node[]) {
		[I40E_TUNNEL_GTP_NODE_START] = {
			.name = "START",
		},
		[I40E_TUNNEL_GTP_NODE_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[I40E_TUNNEL_GTP_NODE_IPV4] = {
			.name = "IPv4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_tunnel_node_ipv4_process,
		},
		[I40E_TUNNEL_GTP_NODE_IPV6] = {
			.name = "IPv6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_tunnel_node_ipv6_process,
		},
		[I40E_TUNNEL_GTP_NODE_UDP] = {
			.name = "UDP",
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[I40E_TUNNEL_GTP_NODE_GTPC] = {
			.name = "GTPC",
			.type = RTE_FLOW_ITEM_TYPE_GTPC,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_gtp_validate,
			.process = i40e_tunnel_node_gtp_process,
		},
		[I40E_TUNNEL_GTP_NODE_GTPU] = {
			.name = "GTPU",
			.type = RTE_FLOW_ITEM_TYPE_GTPU,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_gtp_validate,
			.process = i40e_tunnel_node_gtp_process,
		},
		[I40E_TUNNEL_GTP_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[I40E_TUNNEL_GTP_NODE_START] = {
			.next = (size_t[]) {
				I40E_TUNNEL_GTP_NODE_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_GTP_NODE_ETH] = {
			.next = (size_t[]) {
				I40E_TUNNEL_GTP_NODE_IPV4,
				I40E_TUNNEL_GTP_NODE_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_GTP_NODE_IPV4] = {
			.next = (size_t[]) {
				I40E_TUNNEL_GTP_NODE_UDP,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_GTP_NODE_IPV6] = {
			.next = (size_t[]) {
				I40E_TUNNEL_GTP_NODE_UDP,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_GTP_NODE_UDP] = {
			.next = (size_t[]) {
				I40E_TUNNEL_GTP_NODE_GTPC,
				I40E_TUNNEL_GTP_NODE_GTPU,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_GTP_NODE_GTPC] = {
			.next = (size_t[]) {
				I40E_TUNNEL_GTP_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_GTP_NODE_GTPU] = {
			.next = (size_t[]) {
				I40E_TUNNEL_GTP_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

/**
 * L4 tunnel filter graph implementation
 * Pattern: START -> ETH -> (IPv4 | IPv6) -> (TCP | UDP | SCTP) -> END
 */
enum i40e_tunnel_l4_node_id {
	I40E_TUNNEL_L4_NODE_START  = FLOW_GRAPH_NODE_FIRST,
	I40E_TUNNEL_L4_NODE_ETH,
	I40E_TUNNEL_L4_NODE_IPV4,
	I40E_TUNNEL_L4_NODE_IPV6,
	I40E_TUNNEL_L4_NODE_TCP,
	I40E_TUNNEL_L4_NODE_UDP,
	I40E_TUNNEL_L4_NODE_SCTP,
	I40E_TUNNEL_L4_NODE_END,
	I40E_TUNNEL_L4_NODE_MAX,
};

static int
i40e_tunnel_node_tcp_validate(const void *ctx __rte_unused,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *tcp_mask = ci_flow_item_mask(item);

	/* only source/destination ports are supported */
	if (tcp_mask->hdr.sent_seq ||
	    tcp_mask->hdr.recv_ack ||
	    tcp_mask->hdr.data_off ||
	    tcp_mask->hdr.tcp_flags ||
	    tcp_mask->hdr.rx_win ||
	    tcp_mask->hdr.cksum ||
	    tcp_mask->hdr.tcp_urp) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid TCP mask");
	}

	/* src/dst ports have to be fully masked or fully unmasked */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&tcp_mask->hdr.src_port) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&tcp_mask->hdr.dst_port)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid TCP mask");
	}
	/* there can be only one! */
	if (tcp_mask->hdr.src_port && tcp_mask->hdr.dst_port) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid TCP mask");
	}
	return 0;
}

static int
i40e_tunnel_node_tcp_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;
	const struct rte_flow_item_tcp *tcp_spec = item->spec;
	const struct rte_flow_item_tcp *tcp_mask = ci_flow_item_mask(item);

	if (tcp_mask->hdr.src_port) {
		tunnel_filter->l4_port_type = I40E_L4_PORT_TYPE_SRC;
		tunnel_filter->tenant_id = rte_be_to_cpu_32(tcp_spec->hdr.src_port);
	} else if (tcp_mask->hdr.dst_port) {
		tunnel_filter->l4_port_type = I40E_L4_PORT_TYPE_DST;
		tunnel_filter->tenant_id = rte_be_to_cpu_32(tcp_spec->hdr.dst_port);
	}
	tunnel_filter->tunnel_type = I40E_CLOUD_TYPE_TCP;

	return 0;
}

static int
i40e_tunnel_node_udp_validate(const void *ctx __rte_unused,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *udp_mask = ci_flow_item_mask(item);

	/* only source/destination ports are supported */
	if (udp_mask->hdr.dgram_len ||
	    udp_mask->hdr.dgram_cksum) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid UDP mask");
	}

	/* src/dst ports have to be fully masked or fully unmasked */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&udp_mask->hdr.src_port) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&udp_mask->hdr.dst_port)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid UDP mask");
	}
	/* there can be only one! */
	if (udp_mask->hdr.src_port && udp_mask->hdr.dst_port) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid UDP mask");
	}
	return 0;
}

static int
i40e_tunnel_node_udp_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;
	const struct rte_flow_item_udp *udp_spec = item->spec;
	const struct rte_flow_item_udp *udp_mask = ci_flow_item_mask(item);

	if (udp_mask->hdr.src_port) {
		tunnel_filter->l4_port_type = I40E_L4_PORT_TYPE_SRC;
		tunnel_filter->tenant_id = rte_be_to_cpu_32(udp_spec->hdr.src_port);
	} else if (udp_mask->hdr.dst_port) {
		tunnel_filter->l4_port_type = I40E_L4_PORT_TYPE_DST;
		tunnel_filter->tenant_id = rte_be_to_cpu_32(udp_spec->hdr.dst_port);
	}
	tunnel_filter->tunnel_type = I40E_CLOUD_TYPE_UDP;

	return 0;
}

static int
i40e_tunnel_node_sctp_validate(const void *ctx __rte_unused,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_sctp *sctp_mask = ci_flow_item_mask(item);

	/* only source/destination ports are supported */
	if (sctp_mask->hdr.cksum || sctp_mask->hdr.tag) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid SCTP mask");
	}

	/* src/dst ports have to be fully masked or fully unmasked */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&sctp_mask->hdr.src_port) ||
	    !CI_FIELD_IS_ZERO_OR_MASKED(&sctp_mask->hdr.dst_port)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid SCTP mask");
	}
	/* there can be only one! */
	if (sctp_mask->hdr.src_port && sctp_mask->hdr.dst_port) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Invalid SCTP mask");
	}
	return 0;
}

static int
i40e_tunnel_node_sctp_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error __rte_unused)
{
	struct i40e_tunnel_ctx *tunnel_ctx = ctx;
	struct i40e_tunnel_filter_conf *tunnel_filter = &tunnel_ctx->filter;
	const struct rte_flow_item_sctp *sctp_spec = item->spec;
	const struct rte_flow_item_sctp *sctp_mask = ci_flow_item_mask(item);

	if (sctp_mask->hdr.src_port) {
		tunnel_filter->l4_port_type = I40E_L4_PORT_TYPE_SRC;
		tunnel_filter->tenant_id = rte_be_to_cpu_32(sctp_spec->hdr.src_port);
	} else if (sctp_mask->hdr.dst_port) {
		tunnel_filter->l4_port_type = I40E_L4_PORT_TYPE_DST;
		tunnel_filter->tenant_id = rte_be_to_cpu_32(sctp_spec->hdr.dst_port);
	}
	tunnel_filter->tunnel_type = I40E_CLOUD_TYPE_SCTP;

	return 0;
}

static const struct flow_graph i40e_tunnel_l4_graph = {
	.nodes = (struct flow_graph_node[]) {
		[I40E_TUNNEL_L4_NODE_START] = {
			.name = "START",
		},
		[I40E_TUNNEL_L4_NODE_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[I40E_TUNNEL_L4_NODE_IPV4] = {
			.name = "IPv4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_tunnel_node_ipv4_process,
		},
		[I40E_TUNNEL_L4_NODE_IPV6] = {
			.name = "IPv6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
			.process = i40e_tunnel_node_ipv6_process,
		},
		[I40E_TUNNEL_L4_NODE_TCP] = {
			.name = "TCP",
			.type = RTE_FLOW_ITEM_TYPE_TCP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_tcp_validate,
			.process = i40e_tunnel_node_tcp_process,
		},
		[I40E_TUNNEL_L4_NODE_UDP] = {
			.name = "UDP",
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_udp_validate,
			.process = i40e_tunnel_node_udp_process,
		},
		[I40E_TUNNEL_L4_NODE_SCTP] = {
			.name = "SCTP",
			.type = RTE_FLOW_ITEM_TYPE_SCTP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_tunnel_node_sctp_validate,
			.process = i40e_tunnel_node_sctp_process,
		},
		[I40E_TUNNEL_L4_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[I40E_TUNNEL_L4_NODE_START] = {
			.next = (size_t[]) {
				I40E_TUNNEL_L4_NODE_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_L4_NODE_ETH] = {
			.next = (size_t[]) {
				I40E_TUNNEL_L4_NODE_IPV4,
				I40E_TUNNEL_L4_NODE_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_L4_NODE_IPV4] = {
			.next = (size_t[]) {
				I40E_TUNNEL_L4_NODE_TCP,
				I40E_TUNNEL_L4_NODE_UDP,
				I40E_TUNNEL_L4_NODE_SCTP,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_L4_NODE_IPV6] = {
			.next = (size_t[]) {
				I40E_TUNNEL_L4_NODE_TCP,
				I40E_TUNNEL_L4_NODE_UDP,
				I40E_TUNNEL_L4_NODE_SCTP,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_L4_NODE_TCP] = {
			.next = (size_t[]) {
				I40E_TUNNEL_L4_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_L4_NODE_UDP] = {
			.next = (size_t[]) {
				I40E_TUNNEL_L4_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_TUNNEL_L4_NODE_SCTP] = {
			.next = (size_t[]) {
				I40E_TUNNEL_L4_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

static int
i40e_tunnel_action_check(const struct ci_flow_actions *actions,
		const struct ci_flow_actions_check_param *param,
		struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(param->driver_ctx);
	const struct rte_flow_action *first, *second;
	const struct rte_flow_action_queue *act_q;
	bool is_to_vf = false;

	first = actions->actions[0];
	/* can be NULL */
	second = actions->actions[1];

	/* first action must be PF or VF */
	if (first->type == RTE_FLOW_ACTION_TYPE_VF) {
		const struct rte_flow_action_vf *vf = first->conf;
		if (vf->id >= pf->vf_num) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, first,
					"Invalid VF ID for tunnel filter");
		}
		is_to_vf = true;
	} else if (first->type != RTE_FLOW_ACTION_TYPE_PF) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, first,
				"Unsupported action");
	}

	/* check if second action is QUEUE */
	if (second == NULL)
		return 0;

	if (second->type != RTE_FLOW_ACTION_TYPE_QUEUE) {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, second,
					  "Unsupported action");
	}

	act_q = second->conf;
	/* check queue ID for PF flow */
	if (!is_to_vf && act_q->index >= pf->dev_data->nb_rx_queues) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, act_q,
				"Invalid queue ID for tunnel filter");
	}
	/* check queue ID for VF flow */
	if (is_to_vf && act_q->index >= pf->vf_nb_qps) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, act_q,
				"Invalid queue ID for tunnel filter");
	}

	return 0;
}

static int
i40e_tunnel_ctx_init(const struct rte_flow_action actions[],
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct i40e_tunnel_ctx *tunnel_ctx = (struct i40e_tunnel_ctx *)ctx;
	struct ci_flow_actions parsed_actions = {0};
	struct ci_flow_actions_check_param ac_param = {
		.allowed_types = (enum rte_flow_action_type[]) {
			RTE_FLOW_ACTION_TYPE_QUEUE,
			RTE_FLOW_ACTION_TYPE_PF,
			RTE_FLOW_ACTION_TYPE_VF,
			RTE_FLOW_ACTION_TYPE_END
		},
		.max_actions = 2,
		.check = i40e_tunnel_action_check,
		.driver_ctx = ctx->dev_data->dev_private,
	};
	const struct rte_flow_action *first, *second;
	const struct rte_flow_action_queue *act_q;
	int ret;

	ret = ci_flow_check_attr(attr, NULL, error);
	if (ret)
		return ret;

	ret = ci_flow_check_actions(actions, &ac_param, &parsed_actions, error);
	if (ret)
		return ret;

	first = parsed_actions.actions[0];
	/* can be NULL */
	second = parsed_actions.actions[1];

	if (first->type == RTE_FLOW_ACTION_TYPE_VF) {
		const struct rte_flow_action_vf *vf = first->conf;
		tunnel_ctx->filter.vf_id = vf->id;
		tunnel_ctx->filter.is_to_vf = 1;
	} else if (first->type == RTE_FLOW_ACTION_TYPE_PF) {
		tunnel_ctx->filter.is_to_vf = 0;
	}

	/* check if second action is QUEUE */
	if (second == NULL)
		return 0;

	act_q = second->conf;
	tunnel_ctx->filter.queue_id = act_q->index;

	return 0;
}

static int
i40e_tunnel_ctx_to_flow(const struct ci_flow_engine_ctx *ctx,
		struct ci_flow *flow,
		struct rte_flow_error *error)
{
	const struct i40e_tunnel_ctx *tunnel_ctx = (const struct i40e_tunnel_ctx *)ctx;
	struct i40e_tunnel_flow *tunnel_flow = (struct i40e_tunnel_flow *)flow;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(flow->dev_data->dev_private);
	int ret;

	/* copy filter configuration from context to flow */
	tunnel_flow->filter = tunnel_ctx->filter;

	/* compute hash input */
	ret = i40e_tunnel_filter_match_key_get(pf, &tunnel_flow->filter,
			&tunnel_flow->match_key);
	if (ret != 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to build tunnel filter input");
	}

	return 0;
}

static int
i40e_tunnel_flow_register(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_tunnel_flow *tunnel_flow = (struct i40e_tunnel_flow *)flow;
	struct i40e_tunnel_priv *priv = flow->engine_priv;
	struct i40e_tunnel_state *state = priv->state;
	const struct i40e_tunnel_filter_recipe *recipe;
	int ret;

	if (state->replacement_state == I40E_TUNNEL_FILTER_RESET_NEEDED) {
		return rte_flow_error_set(error, EIO,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Tunnel filter configuration requires device reset");
	}

	if (rte_hash_lookup(state->hash_table, &tunnel_flow->match_key) >= 0) {
		return rte_flow_error_set(error, EEXIST,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Conflict with existing tunnel rule");
	}

	recipe = i40e_tunnel_filter_recipe_get(&tunnel_flow->filter);
	if (recipe != NULL) {
		/*
		 * we're about to install a filter that requires tunnel filter
		 * replacement, which is a destructive operation and cannot be
		 * undone until reset.
		 *
		 * if the current configuration is default, we can only do the
		 * replacement if there are no other flows, as otherwise we're
		 * risking replacing an actively used filter.
		 *
		 * similarly, if another configuration is already in effect, we
		 * cannot proceed either.
		 */
		if (state->configured_recipe == I40E_TUNNEL_REPLACEMENT_DEFAULT) {
			if (state->flow_count != 0)
				return rte_flow_error_set(error, EBUSY,
						RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
						"Standard tunnel filters are in use");
		} else if (state->configured_recipe != recipe->type) {
			return rte_flow_error_set(error, EBUSY,
					RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					"Tunnel filter configuration is already selected");
		}
	} else if (state->configured_recipe != I40E_TUNNEL_REPLACEMENT_DEFAULT) {
		return rte_flow_error_set(error, EBUSY,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Tunnel filter configuration was replaced");
	}

	ret = rte_hash_add_key(state->hash_table, &tunnel_flow->match_key);
	if (ret < 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Tunnel filter table is full");
	}

	if (recipe != NULL &&
			state->configured_recipe == I40E_TUNNEL_REPLACEMENT_DEFAULT)
		state->configured_recipe = recipe->type;

	ci_usage_count_claim(&state->flow_count);

	return 0;
}

static int
i40e_tunnel_flow_unregister(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_tunnel_flow *tunnel_flow = (struct i40e_tunnel_flow *)flow;
	struct i40e_tunnel_priv *priv = flow->engine_priv;
	struct i40e_tunnel_state *state = priv->state;

	if (rte_hash_del_key(state->hash_table, &tunnel_flow->match_key) < 0) {
		return rte_flow_error_set(error, ENOENT,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Tunnel filter is missing from the filter table");
	}

	ci_usage_count_release(&state->flow_count);

	return 0;
}

static int
i40e_tunnel_flow_install(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(flow->dev_data->dev_private);
	struct i40e_tunnel_flow *tunnel_flow = (struct i40e_tunnel_flow *)flow;
	int ret;

	ret = i40e_tunnel_filter_prepare_hw(pf, &tunnel_flow->filter);
	if (ret != 0) {
		pf->tunnel_state.replacement_state =
			I40E_TUNNEL_FILTER_RESET_NEEDED;
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Failed to configure tunnel filters");
	}

	ret = i40e_tunnel_filter_program(pf, &tunnel_flow->filter, 1);
	if (ret) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Failed to install tunnel filter");
	}
	return 0;
}

static int
i40e_tunnel_flow_uninstall(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(flow->dev_data->dev_private);
	struct i40e_tunnel_flow *tunnel_flow = (struct i40e_tunnel_flow *)flow;
	int ret;

	ret = i40e_tunnel_filter_program(pf, &tunnel_flow->filter, 0);
	if (ret) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Failed to uninstall tunnel filter");
	}
	return 0;
}

static int
i40e_tunnel_flow_engine_init(const struct ci_flow_engine *engine __rte_unused,
		struct rte_eth_dev_data *dev_data,
		void *priv)
{
	struct i40e_tunnel_priv *tunnel_priv = priv;

	tunnel_priv->state = i40e_tunnel_state_attach(dev_data);
	return tunnel_priv->state == NULL ? -ENOMEM : 0;
}

static void
i40e_tunnel_flow_engine_uninit(const struct ci_flow_engine *engine __rte_unused,
		void *priv)
{
	struct i40e_tunnel_priv *tunnel_priv = priv;

	i40e_tunnel_state_detach(tunnel_priv->state);
}

static const struct ci_flow_engine_ops i40e_flow_engine_tunnel_ops = {
	.engine_init = i40e_tunnel_flow_engine_init,
	.engine_uninit = i40e_tunnel_flow_engine_uninit,
	.ctx_init = i40e_tunnel_ctx_init,
	.ctx_to_flow = i40e_tunnel_ctx_to_flow,
	.flow_register = i40e_tunnel_flow_register,
	.flow_unregister = i40e_tunnel_flow_unregister,
	.flow_install = i40e_tunnel_flow_install,
	.flow_uninstall = i40e_tunnel_flow_uninstall,
};

const struct ci_flow_engine i40e_flow_engine_tunnel_nvgre = {
	.name = "i40e_tunnel_nvgre",
	.ops = &i40e_flow_engine_tunnel_ops,
	.ctx_size = sizeof(struct i40e_tunnel_ctx),
	.flow_size = sizeof(struct i40e_tunnel_flow),
	.priv_size = sizeof(struct i40e_tunnel_priv),
	.graph = &i40e_tunnel_nvgre_graph,
};

const struct ci_flow_engine i40e_flow_engine_tunnel_vxlan = {
	.name = "i40e_tunnel_vxlan",
	.ops = &i40e_flow_engine_tunnel_ops,
	.ctx_size = sizeof(struct i40e_tunnel_ctx),
	.flow_size = sizeof(struct i40e_tunnel_flow),
	.priv_size = sizeof(struct i40e_tunnel_priv),
	.graph = &i40e_tunnel_vxlan_graph,
};

const struct ci_flow_engine i40e_flow_engine_tunnel_mpls = {
	.name = "i40e_tunnel_mpls",
	.ops = &i40e_flow_engine_tunnel_ops,
	.ctx_size = sizeof(struct i40e_tunnel_ctx),
	.flow_size = sizeof(struct i40e_tunnel_flow),
	.priv_size = sizeof(struct i40e_tunnel_priv),
	.graph = &i40e_tunnel_mpls_graph,
};

const struct ci_flow_engine i40e_flow_engine_tunnel_gtp = {
	.name = "i40e_tunnel_gtp",
	.ops = &i40e_flow_engine_tunnel_ops,
	.ctx_size = sizeof(struct i40e_tunnel_ctx),
	.flow_size = sizeof(struct i40e_tunnel_flow),
	.priv_size = sizeof(struct i40e_tunnel_priv),
	.graph = &i40e_tunnel_gtp_graph,
};

const struct ci_flow_engine i40e_flow_engine_tunnel_l4 = {
	.name = "i40e_tunnel_l4",
	.ops = &i40e_flow_engine_tunnel_ops,
	.ctx_size = sizeof(struct i40e_tunnel_ctx),
	.flow_size = sizeof(struct i40e_tunnel_flow),
	.priv_size = sizeof(struct i40e_tunnel_priv),
	.graph = &i40e_tunnel_l4_graph,
};

const struct ci_flow_engine i40e_flow_engine_tunnel_qinq = {
	.name = "i40e_tunnel_qinq",
	.ops = &i40e_flow_engine_tunnel_ops,
	.ctx_size = sizeof(struct i40e_tunnel_ctx),
	.flow_size = sizeof(struct i40e_tunnel_flow),
	.priv_size = sizeof(struct i40e_tunnel_priv),
	.graph = &i40e_tunnel_qinq_graph,
};
