/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#include <rte_flow.h>
#include <flow_graph.h>
#include <rte_ether.h>

#include "ixgbe_ethdev.h"
#include "ixgbe_flow.h"
#include "../common/flow_check.h"
#include "../common/flow_util.h"
#include "../common/flow_engine.h"

struct ixgbe_ethertype_flow {
	struct rte_flow flow;
	uint8_t idx;
	uint16_t ether_type;
	uint16_t queue;
	uint32_t etqf;
	uint32_t etqs;
};

struct ixgbe_ethertype_ctx {
	struct ci_flow_engine_ctx base;
	struct rte_eth_ethertype_filter filter;
};

/**
 * Ethertype filter graph implementation
 * Pattern: START -> ETH -> END
 */

enum ixgbe_ethertype_node_id {
	IXGBE_ETHERTYPE_NODE_START = FLOW_GRAPH_NODE_FIRST,
	IXGBE_ETHERTYPE_NODE_ETH,
	IXGBE_ETHERTYPE_NODE_END,
	IXGBE_ETHERTYPE_NODE_MAX,
};

static int
ixgbe_ethertype_node_eth_validate(const void *ctx __rte_unused,
				  const struct rte_flow_item *item,
				  struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *eth_spec;
	const struct rte_flow_item_eth *eth_mask;

	eth_spec = item->spec;
	eth_mask = ci_flow_item_mask(item);

	/* Source MAC mask must be all zeros */
	if (!CI_FIELD_IS_ZERO(&eth_mask->hdr.src_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Source MAC filtering not supported");
	}

	/* Dest MAC mask must be all zeros */
	if (!CI_FIELD_IS_ZERO(&eth_mask->hdr.dst_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Destination MAC filtering not supported");
	}

	/* Ethertype mask must be exact match */
	if (!CI_FIELD_IS_MASKED(&eth_mask->hdr.ether_type)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Ethertype must be exactly matched");
	}

	/* IPv4/IPv6 ethertypes not supported by hardware */
	uint16_t ether_type = rte_be_to_cpu_16(eth_spec->hdr.ether_type);
	if (ether_type == RTE_ETHER_TYPE_IPV4 || ether_type == RTE_ETHER_TYPE_IPV6) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"IPv4/IPv6 not supported by ethertype filter");
	}

	return 0;
}

static int
ixgbe_ethertype_node_eth_process(void *ctx,
				 const struct rte_flow_item *item,
				 struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_ethertype_ctx *graph_ctx = ctx;
	const struct rte_flow_item_eth *eth_spec = item->spec;

	graph_ctx->filter.ether_type = rte_be_to_cpu_16(eth_spec->hdr.ether_type);

	return 0;
}

static const struct flow_graph ixgbe_ethertype_graph = {
	.nodes = (struct flow_graph_node[]) {
		[IXGBE_ETHERTYPE_NODE_START] = {
			.name = "START",
		},
		[IXGBE_ETHERTYPE_NODE_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = ixgbe_ethertype_node_eth_validate,
			.process = ixgbe_ethertype_node_eth_process,
		},
		[IXGBE_ETHERTYPE_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[IXGBE_ETHERTYPE_NODE_START] = {
			.next = (size_t[]) {
				IXGBE_ETHERTYPE_NODE_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_ETHERTYPE_NODE_ETH] = {
			.next = (size_t[]) {
				IXGBE_ETHERTYPE_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

static int
ixgbe_flow_ethertype_ctx_init(const struct rte_flow_action *actions,
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct ci_flow_actions parsed_actions;
	struct ci_flow_actions_check_param ap_param = {
		.allowed_types = (const enum rte_flow_action_type[]){
			/* only queue is allowed here */
			RTE_FLOW_ACTION_TYPE_QUEUE,
			RTE_FLOW_ACTION_TYPE_END
		},
		.max_actions = 1,
		.driver_ctx = ctx->dev_data,
		.check = ixgbe_flow_actions_check
	};
	struct ixgbe_ethertype_ctx *ethertype_ctx = (struct ixgbe_ethertype_ctx *)ctx;
	const struct rte_flow_action_queue *q_act;
	int ret;

	/* validate attributes */
	ret = ci_flow_check_attr(attr, NULL, error);
	if (ret)
		return ret;

	/* parse requested actions */
	ret = ci_flow_check_actions(actions, &ap_param, &parsed_actions, error);
	if (ret)
		return ret;

	q_act = (const struct rte_flow_action_queue *)parsed_actions.actions[0]->conf;

	/* set up filter action */
	ethertype_ctx->filter.queue = q_act->index;

	return 0;
}

static int
ixgbe_flow_ethertype_ctx_to_flow(const struct ci_flow_engine_ctx *ctx,
		struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	const struct ixgbe_ethertype_ctx *ethertype_ctx = (const struct ixgbe_ethertype_ctx *)ctx;
	struct ixgbe_ethertype_flow *ethertype_flow = (struct ixgbe_ethertype_flow *)flow;

	/* copy filter configuration */
	ethertype_flow->ether_type = ethertype_ctx->filter.ether_type;
	ethertype_flow->queue = ethertype_ctx->filter.queue;

	/* build the ETQF/ETQS register values from the parsed filter */
	ethertype_flow->etqf = IXGBE_ETQF_FILTER_EN | (uint32_t)ethertype_flow->ether_type;
	ethertype_flow->etqs = ((uint32_t)ethertype_flow->queue <<
			IXGBE_ETQS_RX_QUEUE_SHIFT) & IXGBE_ETQS_RX_QUEUE;
	ethertype_flow->etqs |= IXGBE_ETQS_QUEUE_EN;

	return 0;
}

static int
ixgbe_flow_ethertype_register(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct ixgbe_ethertype_flow *ethertype_flow = (struct ixgbe_ethertype_flow *)flow;
	struct ixgbe_filter_info *filter_info =
		IXGBE_DEV_PRIVATE_TO_FILTER_INFO(flow->dev_data->dev_private);
	int idx;

	idx = ixgbe_ethertype_table_add(&filter_info->ethertype_table,
			ethertype_flow->ether_type, ethertype_flow->etqf,
			ethertype_flow->etqs);
	if (idx == -EEXIST) {
		return rte_flow_error_set(error, EEXIST,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Ethertype filter already exists");
	}
	if (idx == -ENOSPC) {
		return rte_flow_error_set(error, ENOSPC,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Ethertype filters are full");
	}
	if (idx < 0) {
		return rte_flow_error_set(error, -idx,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to register Ethertype filter");
	}
	ethertype_flow->idx = idx;
	return 0;
}

static int
ixgbe_flow_ethertype_unregister(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct ixgbe_ethertype_flow *ethertype_flow = (struct ixgbe_ethertype_flow *)flow;
	struct ixgbe_filter_info *filter_info =
		IXGBE_DEV_PRIVATE_TO_FILTER_INFO(flow->dev_data->dev_private);
	int ret;

	ret = ixgbe_ethertype_table_del(&filter_info->ethertype_table,
			ethertype_flow->idx);
	if (ret != 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Ethertype filter slot not found on unregister");
	}

	return 0;
}

static int
ixgbe_flow_ethertype_install(struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_ethertype_flow *ethertype_flow = (struct ixgbe_ethertype_flow *)flow;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(flow->dev_data->dev_private);

	ixgbe_ethertype_filter_program(hw, ethertype_flow->idx,
			ethertype_flow->etqf, ethertype_flow->etqs);

	return 0;
}

static int
ixgbe_flow_ethertype_uninstall(struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_ethertype_flow *ethertype_flow = (struct ixgbe_ethertype_flow *)flow;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(flow->dev_data->dev_private);

	ixgbe_ethertype_filter_program(hw, ethertype_flow->idx, 0, 0);

	return 0;
}

static int
ixgbe_flow_ethertype_engine_init(const struct ci_flow_engine *engine __rte_unused,
		struct rte_eth_dev_data *dev_data,
		void *priv __rte_unused)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev_data->dev_private);

	/* Ethertype filtering (ETQF) is only available on these MACs. */
	if (hw->mac.type == ixgbe_mac_82599EB ||
			hw->mac.type == ixgbe_mac_X540 ||
			hw->mac.type == ixgbe_mac_X550 ||
			hw->mac.type == ixgbe_mac_X550EM_x ||
			hw->mac.type == ixgbe_mac_X550EM_a ||
			hw->mac.type == ixgbe_mac_E610)
		return 0;

	return -ENOTSUP;
}

static const struct ci_flow_engine_ops ixgbe_ethertype_ops = {
	.engine_init = ixgbe_flow_ethertype_engine_init,
	.ctx_init = ixgbe_flow_ethertype_ctx_init,
	.ctx_to_flow = ixgbe_flow_ethertype_ctx_to_flow,
	.flow_register = ixgbe_flow_ethertype_register,
	.flow_unregister = ixgbe_flow_ethertype_unregister,
	.flow_install = ixgbe_flow_ethertype_install,
	.flow_uninstall = ixgbe_flow_ethertype_uninstall,
};

const struct ci_flow_engine ixgbe_ethertype_flow_engine = {
	.name = "ixgbe_ethertype",
	.ctx_size = sizeof(struct ixgbe_ethertype_ctx),
	.flow_size = sizeof(struct ixgbe_ethertype_flow),
	.graph = &ixgbe_ethertype_graph,
	.ops = &ixgbe_ethertype_ops,
};
