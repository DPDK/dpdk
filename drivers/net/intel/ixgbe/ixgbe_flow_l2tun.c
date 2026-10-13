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

struct ixgbe_l2_tunnel_flow {
	struct rte_flow flow;
	struct ixgbe_l2_tunnel_conf l2_tunnel;
};

struct ixgbe_l2_tunnel_ctx {
	struct ci_flow_engine_ctx base;
	struct ixgbe_l2_tunnel_conf l2_tunnel;
};

/* per-device dedup set of installed tunnel ids (hardware matches by id) */
struct ixgbe_l2_tunnel_priv {
	uint32_t count;
	uint32_t tunnel_id[IXGBE_MAX_L2_TN_FILTER_NUM];
};

/**
 * L2 tunnel filter graph implementation (E-TAG)
 * Pattern: START -> E_TAG -> END
 */

enum ixgbe_l2_tunnel_node_id {
	IXGBE_L2_TUNNEL_NODE_START = FLOW_GRAPH_NODE_FIRST,
	IXGBE_L2_TUNNEL_NODE_E_TAG,
	IXGBE_L2_TUNNEL_NODE_END,
	IXGBE_L2_TUNNEL_NODE_MAX,
};

static int
ixgbe_validate_l2_tunnel_e_tag(const void *ctx __rte_unused,
				const struct rte_flow_item *item,
				struct rte_flow_error *error)
{
	const struct rte_flow_item_e_tag *e_tag_mask;

	e_tag_mask = ci_flow_item_mask(item);

	/* Only GRP and E-CID base supported (rsvd_grp_ecid_b field) */
	if (e_tag_mask->epcp_edei_in_ecid_b ||
	    e_tag_mask->in_ecid_e ||
	    e_tag_mask->ecid_e ||
	    rte_be_to_cpu_16(e_tag_mask->rsvd_grp_ecid_b) != 0x3FFF) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Only GRP and E-CID base (14 bits) supported");
	}

	return 0;
}

static int
ixgbe_process_l2_tunnel_e_tag(void *ctx,
			       const struct rte_flow_item *item,
			       struct rte_flow_error *error __rte_unused)
{
	struct ixgbe_l2_tunnel_ctx *l2tun_ctx = ctx;
	const struct rte_flow_item_e_tag *e_tag_spec = item->spec;

	l2tun_ctx->l2_tunnel.l2_tunnel_type = RTE_ETH_L2_TUNNEL_TYPE_E_TAG;
	l2tun_ctx->l2_tunnel.tunnel_id = rte_be_to_cpu_16(e_tag_spec->rsvd_grp_ecid_b);

	return 0;
}

static const struct flow_graph ixgbe_l2_tunnel_graph = {
	.nodes = (struct flow_graph_node[]) {
		[IXGBE_L2_TUNNEL_NODE_START] = {
			.name = "START",
		},
		[IXGBE_L2_TUNNEL_NODE_E_TAG] = {
			.name = "E_TAG",
			.type = RTE_FLOW_ITEM_TYPE_E_TAG,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = ixgbe_validate_l2_tunnel_e_tag,
			.process = ixgbe_process_l2_tunnel_e_tag,
		},
		[IXGBE_L2_TUNNEL_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[IXGBE_L2_TUNNEL_NODE_START] = {
			.next = (size_t[]) {
				IXGBE_L2_TUNNEL_NODE_E_TAG,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[IXGBE_L2_TUNNEL_NODE_E_TAG] = {
			.next = (size_t[]) {
				IXGBE_L2_TUNNEL_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

static int
ixgbe_flow_l2_tunnel_ctx_init(const struct rte_flow_action *actions,
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct ixgbe_l2_tunnel_ctx *l2tun_ctx = (struct ixgbe_l2_tunnel_ctx *)ctx;
	struct ixgbe_adapter *adapter = IXGBE_DEV_PRIVATE_TO_ADAPTER(ctx->dev_data->dev_private);
	struct ci_flow_actions parsed_actions;
	struct ci_flow_actions_check_param ap_param = {
		.allowed_types = (const enum rte_flow_action_type[]){
			/* only vf/pf is allowed here */
			RTE_FLOW_ACTION_TYPE_VF,
			RTE_FLOW_ACTION_TYPE_PF,
			RTE_FLOW_ACTION_TYPE_END
		},
		.driver_ctx = ctx->dev_data,
		.check = ixgbe_flow_actions_check,
		.max_actions = 1,
	};
	const struct rte_flow_action *action;
	int ret;

	/* validate attributes */
	ret = ci_flow_check_attr(attr, NULL, error);
	if (ret)
		return ret;

	/* parse requested actions */
	ret = ci_flow_check_actions(actions, &ap_param, &parsed_actions, error);
	if (ret)
		return ret;

	action = parsed_actions.actions[0];

	if (action->type == RTE_FLOW_ACTION_TYPE_VF) {
		const struct rte_flow_action_vf *vf = action->conf;
		l2tun_ctx->l2_tunnel.pool = vf->id;
	} else {
		l2tun_ctx->l2_tunnel.pool = adapter->max_vfs;
	}

	return ret;
}

static int
ixgbe_flow_l2_tunnel_ctx_to_flow(const struct ci_flow_engine_ctx *ctx,
		struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	const struct ixgbe_l2_tunnel_ctx *l2tun_ctx = (const struct ixgbe_l2_tunnel_ctx *)ctx;
	struct ixgbe_l2_tunnel_flow *l2tun_flow = (struct ixgbe_l2_tunnel_flow *)flow;

	l2tun_flow->l2_tunnel = l2tun_ctx->l2_tunnel;

	return 0;
}

static int
ixgbe_l2_tunnel_id_find(const struct ixgbe_l2_tunnel_priv *priv, uint32_t tunnel_id)
{
	uint32_t i;

	for (i = 0; i < priv->count; i++) {
		if (priv->tunnel_id[i] == tunnel_id)
			return (int)i;
	}

	return -ENOENT;
}

static int
ixgbe_l2_tunnel_id_add(struct ixgbe_l2_tunnel_priv *priv, uint32_t tunnel_id)
{
	if (priv->count >= IXGBE_MAX_L2_TN_FILTER_NUM)
		return -ENOSPC;
	if (ixgbe_l2_tunnel_id_find(priv, tunnel_id) >= 0)
		return -EEXIST;

	priv->tunnel_id[priv->count++] = tunnel_id;

	return 0;
}

static int
ixgbe_l2_tunnel_id_del(struct ixgbe_l2_tunnel_priv *priv, uint32_t tunnel_id)
{
	int idx = ixgbe_l2_tunnel_id_find(priv, tunnel_id);

	if (idx < 0)
		return -ENOENT;

	/* keep a dense set, order doesn't matter */
	priv->tunnel_id[idx] = priv->tunnel_id[--priv->count];

	return 0;
}

static int
ixgbe_flow_l2_tunnel_flow_register(struct ci_flow *flow,
		struct rte_flow_error *error)
{
	struct ixgbe_l2_tunnel_flow *l2tun_flow = (struct ixgbe_l2_tunnel_flow *)flow;
	struct ixgbe_l2_tunnel_priv *priv = flow->engine_priv;
	int ret;

	ret = ixgbe_l2_tunnel_id_add(priv, l2tun_flow->l2_tunnel.tunnel_id);
	if (ret == -ENOSPC) {
		return rte_flow_error_set(error, ENOSPC,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"L2 tunnel filter table is full");
	}
	if (ret == -EEXIST) {
		return rte_flow_error_set(error, EEXIST,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"L2 tunnel filter already exists");
	}
	if (ret < 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to register L2 tunnel filter");
	}

	return 0;
}

static int
ixgbe_flow_l2_tunnel_flow_unregister(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct ixgbe_l2_tunnel_flow *l2tun_flow = (struct ixgbe_l2_tunnel_flow *)flow;
	struct ixgbe_l2_tunnel_priv *priv = flow->engine_priv;

	if (ixgbe_l2_tunnel_id_del(priv, l2tun_flow->l2_tunnel.tunnel_id) != 0) {
		return rte_flow_error_set(error, ENOENT,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"L2 tunnel filter not found on unregister");
	}

	return 0;
}

static int
ixgbe_flow_l2_tunnel_flow_install(struct ci_flow *flow,
		struct rte_flow_error *error)
{
	struct ixgbe_l2_tunnel_flow *l2tun_flow = (struct ixgbe_l2_tunnel_flow *)flow;
	struct ixgbe_adapter *adapter = IXGBE_DEV_PRIVATE_TO_ADAPTER(flow->dev_data->dev_private);
	int ret;

	ret = ixgbe_e_tag_filter_add(adapter, &l2tun_flow->l2_tunnel);
	if (ret) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Failed to add L2 tunnel filter");
	}

	return 0;
}

static int
ixgbe_flow_l2_tunnel_flow_uninstall(struct ci_flow *flow,
		struct rte_flow_error *error)
{
	struct ixgbe_l2_tunnel_flow *l2tun_flow = (struct ixgbe_l2_tunnel_flow *)flow;
	struct ixgbe_adapter *adapter = IXGBE_DEV_PRIVATE_TO_ADAPTER(flow->dev_data->dev_private);
	int ret;

	ret = ixgbe_e_tag_filter_del(adapter, &l2tun_flow->l2_tunnel);
	if (ret) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Failed to remove L2 tunnel filter");
	}

	return 0;
}

static int
ixgbe_flow_l2_tunnel_engine_init(const struct ci_flow_engine *engine __rte_unused,
		struct rte_eth_dev_data *dev_data,
		void *priv __rte_unused)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev_data->dev_private);

	if (hw->mac.type == ixgbe_mac_X550 ||
			hw->mac.type == ixgbe_mac_X550EM_x ||
			hw->mac.type == ixgbe_mac_X550EM_a ||
			hw->mac.type == ixgbe_mac_E610)
		return 0;

	return -ENOTSUP;
}

static const struct ci_flow_engine_ops ixgbe_l2_tunnel_ops = {
	.engine_init = ixgbe_flow_l2_tunnel_engine_init,
	.ctx_init = ixgbe_flow_l2_tunnel_ctx_init,
	.ctx_to_flow = ixgbe_flow_l2_tunnel_ctx_to_flow,
	.flow_register = ixgbe_flow_l2_tunnel_flow_register,
	.flow_unregister = ixgbe_flow_l2_tunnel_flow_unregister,
	.flow_install = ixgbe_flow_l2_tunnel_flow_install,
	.flow_uninstall = ixgbe_flow_l2_tunnel_flow_uninstall,
};

const struct ci_flow_engine ixgbe_l2_tunnel_flow_engine = {
	.name = "ixgbe_l2_tunnel",
	.ctx_size = sizeof(struct ixgbe_l2_tunnel_ctx),
	.flow_size = sizeof(struct ixgbe_l2_tunnel_flow),
	.priv_size = sizeof(struct ixgbe_l2_tunnel_priv),
	.ops = &ixgbe_l2_tunnel_ops,
	.graph = &ixgbe_l2_tunnel_graph,
};
