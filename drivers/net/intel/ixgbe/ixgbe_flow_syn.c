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

struct ixgbe_syn_flow {
	struct rte_flow flow;
	struct rte_eth_syn_filter syn;
};

struct ixgbe_syn_ctx {
	struct ci_flow_engine_ctx base;
	struct rte_eth_syn_filter syn;
};

struct ixgbe_syn_priv {
	bool installed; /* hardware supports a single SYN filter */
};

/**
 * SYN filter graph implementation
 * Pattern: START -> [ETH -> (IPV4|IPV6)] -> TCP -> END
 */

enum ixgbe_syn_node_id {
	IXGBE_SYN_NODE_START = FLOW_GRAPH_NODE_FIRST,
	IXGBE_SYN_NODE_ETH,
	IXGBE_SYN_NODE_IPV4,
	IXGBE_SYN_NODE_IPV6,
	IXGBE_SYN_NODE_TCP,
	IXGBE_SYN_NODE_END,
	IXGBE_SYN_NODE_MAX,
};

static int
ixgbe_validate_syn_tcp(const void *ctx __rte_unused,
		       const struct rte_flow_item *item,
		       struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *tcp_spec;
	const struct rte_flow_item_tcp *tcp_mask;

	tcp_spec = item->spec;
	tcp_mask = ci_flow_item_mask(item);

	/* SYN flag must be set in spec */
	if (!(tcp_spec->hdr.tcp_flags & RTE_TCP_SYN_FLAG)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"TCP SYN flag must be set");
	}

	/* Mask must match only SYN flag */
	if (tcp_mask->hdr.tcp_flags != RTE_TCP_SYN_FLAG) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"TCP flags mask must match SYN only");
	}

	/* All other TCP fields must have zero mask */
	if (tcp_mask->hdr.src_port ||
	    tcp_mask->hdr.dst_port ||
	    tcp_mask->hdr.sent_seq ||
	    tcp_mask->hdr.recv_ack ||
	    tcp_mask->hdr.data_off ||
	    tcp_mask->hdr.rx_win ||
	    tcp_mask->hdr.cksum ||
	    tcp_mask->hdr.tcp_urp) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Only TCP flags filtering supported");
	}

	return 0;
}

static const struct flow_graph ixgbe_syn_graph = {
	.nodes = (struct flow_graph_node[]) {
		[IXGBE_SYN_NODE_START] = {
			.name = "START",
		},
		[IXGBE_SYN_NODE_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[IXGBE_SYN_NODE_IPV4] = {
			.name = "IPV4",
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[IXGBE_SYN_NODE_IPV6] = {
			.name = "IPV6",
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.constraints = FLOW_GRAPH_NODE_EXPECT_EMPTY,
		},
		[IXGBE_SYN_NODE_TCP] = {
			.name = "TCP",
			.type = RTE_FLOW_ITEM_TYPE_TCP,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = ixgbe_validate_syn_tcp,
		},
		[IXGBE_SYN_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[IXGBE_SYN_NODE_START] = {
			.next = (size_t[]) {
				IXGBE_SYN_NODE_ETH,
				IXGBE_SYN_NODE_IPV4,
				IXGBE_SYN_NODE_IPV6,
				IXGBE_SYN_NODE_TCP,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_SYN_NODE_ETH] = {
			.next = (size_t[]) {
				IXGBE_SYN_NODE_IPV4,
				IXGBE_SYN_NODE_IPV6,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_SYN_NODE_IPV4] = {
			.next = (size_t[]) {
				IXGBE_SYN_NODE_TCP,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_SYN_NODE_IPV6] = {
			.next = (size_t[]) {
				IXGBE_SYN_NODE_TCP,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_SYN_NODE_TCP] = {
			.next = (size_t[]) {
				IXGBE_SYN_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

static int
ixgbe_flow_syn_ctx_init(const struct rte_flow_action actions[],
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct ixgbe_syn_ctx *syn_ctx = (struct ixgbe_syn_ctx *)ctx;
	struct ci_flow_actions parsed_actions;
	struct ci_flow_actions_check_param ap_param = {
		.allowed_types = (const enum rte_flow_action_type[]){
			/* only queue is allowed here */
			RTE_FLOW_ACTION_TYPE_QUEUE,
			RTE_FLOW_ACTION_TYPE_END
		},
		.driver_ctx = ctx->dev_data,
		.check = ixgbe_flow_actions_check,
		.max_actions = 1,
	};
	struct ci_flow_attr_check_param attr_param = {
		.allow_priority = true,
	};
	const struct rte_flow_action_queue *q_act;
	int ret;

	/* validate attributes */
	ret = ci_flow_check_attr(attr, &attr_param, error);
	if (ret)
		return ret;

	/* check priority */
	if (attr->priority != 0 && attr->priority != (uint32_t)~0U) {
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
			attr, "Priority can be 0 or 0xFFFFFFFF");
	}

	/* parse requested actions */
	ret = ci_flow_check_actions(actions, &ap_param, &parsed_actions, error);
	if (ret)
		return ret;

	q_act = parsed_actions.actions[0]->conf;

	syn_ctx->syn.queue = q_act->index;

	/* Support 2 priorities. rte_flow priority 0 is highest */
	syn_ctx->syn.hig_pri = attr->priority == 0;

	return 0;
}

static int
ixgbe_flow_syn_ctx_to_flow(const struct ci_flow_engine_ctx *ctx,
		struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	const struct ixgbe_syn_ctx *syn_ctx = (const struct ixgbe_syn_ctx *)ctx;
	struct ixgbe_syn_flow *syn_flow = (struct ixgbe_syn_flow *)flow;

	syn_flow->syn = syn_ctx->syn;

	return 0;
}

static int
ixgbe_flow_syn_flow_register(struct ci_flow *flow,
		struct rte_flow_error *error)
{
	struct ixgbe_syn_priv *priv = flow->engine_priv;

	/* hardware supports a single SYN filter */
	if (priv->installed) {
		return rte_flow_error_set(error, EEXIST,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"SYN filter already exists");
	}

	priv->installed = true;

	return 0;
}

static int
ixgbe_flow_syn_flow_unregister(struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_syn_priv *priv = flow->engine_priv;

	priv->installed = false;

	return 0;
}

static int
ixgbe_flow_syn_flow_install(struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_syn_flow *syn_flow = (struct ixgbe_syn_flow *)flow;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(flow->dev_data->dev_private);
	uint32_t synqf;

	synqf = ((syn_flow->syn.queue << IXGBE_SYN_FILTER_QUEUE_SHIFT) &
			IXGBE_SYN_FILTER_QUEUE) | IXGBE_SYN_FILTER_ENABLE;
	if (syn_flow->syn.hig_pri)
		synqf |= IXGBE_SYN_FILTER_SYNQFP;

	ixgbe_syn_filter_program(hw, synqf);

	return 0;
}

static int
ixgbe_flow_syn_flow_uninstall(struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(flow->dev_data->dev_private);

	ixgbe_syn_filter_program(hw, 0);

	return 0;
}

static int
ixgbe_flow_syn_engine_init(const struct ci_flow_engine *engine __rte_unused,
		struct rte_eth_dev_data *dev_data,
		void *priv __rte_unused)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev_data->dev_private);

	if (hw->mac.type == ixgbe_mac_82599EB ||
			hw->mac.type == ixgbe_mac_X540 ||
			hw->mac.type == ixgbe_mac_X550 ||
			hw->mac.type == ixgbe_mac_X550EM_x ||
			hw->mac.type == ixgbe_mac_X550EM_a ||
			hw->mac.type == ixgbe_mac_E610)
		return 0;

	return -ENOTSUP;
}

static const struct ci_flow_engine_ops ixgbe_syn_ops = {
	.engine_init = ixgbe_flow_syn_engine_init,
	.ctx_init = ixgbe_flow_syn_ctx_init,
	.ctx_to_flow = ixgbe_flow_syn_ctx_to_flow,
	.flow_register = ixgbe_flow_syn_flow_register,
	.flow_unregister = ixgbe_flow_syn_flow_unregister,
	.flow_install = ixgbe_flow_syn_flow_install,
	.flow_uninstall = ixgbe_flow_syn_flow_uninstall,
};

const struct ci_flow_engine ixgbe_syn_flow_engine = {
	.name = "ixgbe_syn",
	.ctx_size = sizeof(struct ixgbe_syn_ctx),
	.flow_size = sizeof(struct ixgbe_syn_flow),
	.priv_size = sizeof(struct ixgbe_syn_priv),
	.ops = &ixgbe_syn_ops,
	.graph = &ixgbe_syn_graph,
};
