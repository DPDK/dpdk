/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Intel Corporation
 */

#include <rte_hash.h>
#include <rte_hash_crc.h>

#include "i40e_ethdev.h"
#include "i40e_flow.h"

#include "../common/flow_engine.h"
#include "../common/flow_check.h"
#include "../common/flow_util.h"

struct i40e_ethertype_ctx {
	struct ci_flow_engine_ctx base;
	struct rte_eth_ethertype_filter ethertype;
};

struct i40e_ethertype_flow {
	struct rte_flow base;
	struct rte_eth_ethertype_filter ethertype;
};

/* leading fields of struct rte_eth_ethertype_filter, used as the dedup hash key */
struct i40e_ethertype_key {
	struct rte_ether_addr mac_addr;
	uint16_t ether_type;
};

struct i40e_ethertype_priv {
	struct rte_hash *hash_table;
};

/**
 * Ethertype filter graph implementation
 * Pattern: START -> ETH -> END
 */

enum i40e_ethertype_node_id {
	I40E_ETHERTYPE_NODE_START = FLOW_GRAPH_NODE_FIRST,
	I40E_ETHERTYPE_NODE_ETH,
	I40E_ETHERTYPE_NODE_END,
	I40E_ETHERTYPE_NODE_MAX,
};

static int
i40e_ethertype_node_eth_validate(const void *ctx __rte_unused,
		const struct rte_flow_item *item, struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *eth_spec = item->spec;
	const struct rte_flow_item_eth *eth_mask = ci_flow_item_mask(item);
	uint16_t ether_type;

	/* Source MAC mask must be all zeros */
	if (!CI_FIELD_IS_ZERO(&eth_mask->hdr.src_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Source MAC filtering not supported");
	}

	/* Dest MAC mask must be all zeros or all ones */
	if (!CI_FIELD_IS_ZERO_OR_MASKED(&eth_mask->hdr.dst_addr)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Dest MAC filtering not supported");
	}

	/* Ethertype mask must be exact match */
	if (!CI_FIELD_IS_MASKED(&eth_mask->hdr.ether_type)) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Ethertype must be exactly matched");
	}

	/* Check for valid ethertype (not IPv4/IPv6/LLDP/VLAN) */
	ether_type = rte_be_to_cpu_16(eth_spec->hdr.ether_type);
	if (ether_type == RTE_ETHER_TYPE_IPV4 ||
	    ether_type == RTE_ETHER_TYPE_IPV6 ||
	    ether_type == RTE_ETHER_TYPE_LLDP ||
	    ether_type == RTE_ETHER_TYPE_VLAN) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"IPv4/IPv6/LLDP/VLAN not supported by ethertype filter");
	}

	return 0;
}

static int
i40e_ethertype_node_eth_process(void *ctx, const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	struct i40e_ethertype_ctx *ethertype_ctx = ctx;
	struct rte_eth_ethertype_filter *filter = &ethertype_ctx->ethertype;
	const struct rte_flow_item_eth *eth_spec = item->spec;
	const struct rte_flow_item_eth *eth_mask = ci_flow_item_mask(item);
	uint16_t ether_type, tpid;
	/* pf cannot be const so it's here rather than in validate() */
	struct rte_eth_dev_data *dev_data = ethertype_ctx->base.dev_data;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev_data->dev_private);

	ether_type = rte_be_to_cpu_16(eth_spec->hdr.ether_type);

	if (CI_FIELD_IS_MASKED(&eth_mask->hdr.dst_addr)) {
		filter->mac_addr = eth_spec->hdr.dst_addr;
		filter->flags |= RTE_ETHTYPE_FLAGS_MAC;
	}

	/* Cannot match currently installed VLAN ethertype */
	if (i40e_get_outer_vlan(pf, &tpid) != 0) {
		return rte_flow_error_set(error, EIO,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Can not get the Ethertype identifying the L2 tag");
	}
	if (ether_type == tpid) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Unsupported ether_type in control packet filter.");
	}

	filter->ether_type = ether_type;

	return 0;
}

static const struct flow_graph i40e_ethertype_graph = {
	.nodes = (struct flow_graph_node[]) {
		[I40E_ETHERTYPE_NODE_START] = {
			.name = "START",
		},
		[I40E_ETHERTYPE_NODE_ETH] = {
			.name = "ETH",
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.constraints = FLOW_GRAPH_NODE_EXPECT_SPEC,
			.validate = i40e_ethertype_node_eth_validate,
			.process = i40e_ethertype_node_eth_process,
		},
		[I40E_ETHERTYPE_NODE_END] = {
			.name = "END",
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	},
	.edges = (struct flow_graph_edge[]) {
		[I40E_ETHERTYPE_NODE_START] = {
			.next = (size_t[]) {
				I40E_ETHERTYPE_NODE_ETH,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
		[I40E_ETHERTYPE_NODE_ETH] = {
			.next = (size_t[]) {
				I40E_ETHERTYPE_NODE_END,
				FLOW_GRAPH_NODE_EDGE_END
			}
		},
	},
};

static int
i40e_flow_ethertype_ctx_init(const struct rte_flow_action *actions,
		const struct rte_flow_attr *attr,
		struct ci_flow_engine_ctx *ctx,
		struct rte_flow_error *error)
{
	struct i40e_ethertype_ctx *ethertype_ctx = (struct i40e_ethertype_ctx *)ctx;
	struct rte_eth_dev_data *dev_data = ethertype_ctx->base.dev_data;
	struct ci_flow_actions parsed_actions = {0};
	struct ci_flow_actions_check_param ac_param = {
		.allowed_types = (enum rte_flow_action_type[]) {
			RTE_FLOW_ACTION_TYPE_QUEUE,
			RTE_FLOW_ACTION_TYPE_DROP,
			RTE_FLOW_ACTION_TYPE_END,
		},
		.max_actions = 1,
	};
	const struct rte_flow_action *action;
	int ret;

	ret = ci_flow_check_actions(actions, &ac_param, &parsed_actions, error);
	if (ret)
		return ret;

	ret = ci_flow_check_attr(attr, NULL, error);
	if (ret)
		return ret;

	action = parsed_actions.actions[0];

	if (action->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
		const struct rte_flow_action_queue *act_q = action->conf;
		/* check queue index */
		if (act_q->index >= dev_data->nb_rx_queues) {
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Invalid queue index");
		}
		ethertype_ctx->ethertype.queue = act_q->index;
	} else if (action->type == RTE_FLOW_ACTION_TYPE_DROP) {
		ethertype_ctx->ethertype.flags |= RTE_ETHTYPE_FLAGS_DROP;
	}
	return 0;
}

static int
i40e_flow_ethertype_ctx_to_flow(const struct ci_flow_engine_ctx *ctx,
		struct ci_flow *flow,
		struct rte_flow_error *error __rte_unused)
{
	const struct i40e_ethertype_ctx *ethertype_ctx = (const struct i40e_ethertype_ctx *)ctx;
	struct i40e_ethertype_flow *ethertype_flow = (struct i40e_ethertype_flow *)flow;

	/* copy ethertype filter configuration to flow */
	ethertype_flow->ethertype = ethertype_ctx->ethertype;

	return 0;
}

static int
i40e_flow_ethertype_register(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_ethertype_flow *ethertype_flow = (struct i40e_ethertype_flow *)flow;
	struct i40e_ethertype_priv *priv = flow->engine_priv;
	struct i40e_ethertype_key key;
	int ret;

	memcpy(&key, &ethertype_flow->ethertype, sizeof(key));

	if (rte_hash_lookup(priv->hash_table, &key) >= 0) {
		return rte_flow_error_set(error, EEXIST,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Conflict with existing ethertype filter");
	}

	ret = rte_hash_add_key(priv->hash_table, &key);
	if (ret < 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to register ethertype filter");
	}

	return 0;
}

static int
i40e_flow_ethertype_unregister(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_ethertype_flow *ethertype_flow = (struct i40e_ethertype_flow *)flow;
	struct i40e_ethertype_priv *priv = flow->engine_priv;
	struct i40e_ethertype_key key;
	int ret;

	memcpy(&key, &ethertype_flow->ethertype, sizeof(key));

	ret = rte_hash_del_key(priv->hash_table, &key);
	if (ret < 0) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Ethertype filter not found on unregister");
	}

	return 0;
}

static int
i40e_flow_ethertype_install(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_ethertype_flow *ethertype_flow = (struct i40e_ethertype_flow *)flow;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(flow->dev_data->dev_private);
	int ret;

	ret = i40e_ethertype_filter_program(pf, &ethertype_flow->ethertype, true);
	if (ret) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Failed to install ethertype filter");
	}
	return 0;
}

static int
i40e_flow_ethertype_uninstall(struct ci_flow *flow, struct rte_flow_error *error)
{
	struct i40e_ethertype_flow *ethertype_flow = (struct i40e_ethertype_flow *)flow;
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(flow->dev_data->dev_private);
	int ret;

	ret = i40e_ethertype_filter_program(pf, &ethertype_flow->ethertype, false);
	if (ret) {
		return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, flow,
				"Failed to delete ethertype filter");
	}
	return 0;
}

static int
i40e_flow_ethertype_engine_init(const struct ci_flow_engine *engine __rte_unused,
		struct rte_eth_dev_data *dev_data, void *priv)
{
	char ethertype_hash_name[RTE_HASH_NAMESIZE];
	struct i40e_ethertype_priv *etype_priv = priv;
	struct rte_hash_parameters params = {
		.name = ethertype_hash_name,
		.entries = I40E_MAX_ETHERTYPE_FILTER_NUM,
		.key_len = sizeof(struct i40e_ethertype_key),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};

	snprintf(ethertype_hash_name, RTE_HASH_NAMESIZE, "etype_%s", dev_data->name);

	etype_priv->hash_table = rte_hash_create(&params);
	if (etype_priv->hash_table == NULL) {
		PMD_INIT_LOG(ERR, "Failed to create ethertype hash table: %s",
				rte_strerror(rte_errno));
		return -rte_errno;
	}

	return 0;
}

static void
i40e_flow_ethertype_engine_uninit(const struct ci_flow_engine *engine __rte_unused,
		void *priv)
{
	struct i40e_ethertype_priv *etype_priv = priv;

	rte_hash_free(etype_priv->hash_table);
}

static const struct ci_flow_engine_ops i40e_flow_engine_ethertype_ops = {
	.engine_init = i40e_flow_ethertype_engine_init,
	.engine_uninit = i40e_flow_ethertype_engine_uninit,
	.ctx_init = i40e_flow_ethertype_ctx_init,
	.ctx_to_flow = i40e_flow_ethertype_ctx_to_flow,
	.flow_register = i40e_flow_ethertype_register,
	.flow_unregister = i40e_flow_ethertype_unregister,
	.flow_install = i40e_flow_ethertype_install,
	.flow_uninstall = i40e_flow_ethertype_uninstall,
};

const struct ci_flow_engine i40e_flow_engine_ethertype = {
	.name = "i40e_ethertype",
	.ctx_size = sizeof(struct i40e_ethertype_ctx),
	.flow_size = sizeof(struct i40e_ethertype_flow),
	.priv_size = sizeof(struct i40e_ethertype_priv),
	.ops = &i40e_flow_engine_ethertype_ops,
	.graph = &i40e_ethertype_graph,
};
