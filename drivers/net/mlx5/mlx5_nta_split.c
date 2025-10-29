/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 NVIDIA Corporation & Affiliates
 */

#include <rte_common.h>
#include <rte_flow.h>

#include "mlx5_malloc.h"
#include "mlx5.h"
#include "mlx5_defs.h"
#include "mlx5_flow.h"
#include "mlx5_rx.h"

#ifdef HAVE_MLX5_HWS_SUPPORT

#define BITS_PER_BYTE	8

/*
 * Generate new actions lists for prefix and suffix flows.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] prefix_act
 *   Pointer to actions for prefix flow rule.
 * @param[in] suffix_act
 *   Pointer to actions for suffix flow rule.
 * @param[in] actions
 *   Pointer to the original actions list.
 * @param[in] qrss
 *   Pointer to the action of QUEUE / RSS.
 * @param[in] actions_n
 *   Number of the actions in the original list.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   Positive prefix flow ID on success, zero on failure.
 */
static uint32_t
mlx5_flow_nta_split_qrss_actions_prep(struct rte_eth_dev *dev,
				      struct rte_flow_action *prefix_act,
				      struct rte_flow_action *suffix_act,
				      const struct rte_flow_action *actions,
				      const struct rte_flow_action *qrss,
				      int actions_n,
				      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_action_modify_field *set_tag;
	struct rte_flow_action_jump *jump;
	const int qrss_idx = qrss - actions;
	uint32_t flow_id = 0;

	/* Allocate the new subflow ID and used to be matched later. */
	mlx5_ipool_malloc(priv->sh->ipool[MLX5_IPOOL_RSS_EXPANTION_FLOW_ID], &flow_id);
	if (!flow_id) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				   "can't allocate id for split Q/RSS subflow");
		return 0;
	}
	/*
	 * Given actions will be split
	 * - Replace QUEUE/RSS action with SET_TAG to set flow ID.
	 * - Add jump to mreg CP_TBL.
	 * As a result, there will be one more action.
	 */
	memcpy(prefix_act, actions, sizeof(struct rte_flow_action) * actions_n);
	/* Count MLX5_RTE_FLOW_ACTION_TYPE_TAG. */
	actions_n++;
	set_tag = (void *)(prefix_act + actions_n);
	/* Internal SET_TAG action to set flow ID. */
	set_tag->operation = RTE_FLOW_MODIFY_SET;
	set_tag->width = sizeof(flow_id) * BITS_PER_BYTE;
	set_tag->src.field = RTE_FLOW_FIELD_VALUE;
	memcpy(&set_tag->src.value, &flow_id, sizeof(flow_id));
	set_tag->dst.field = RTE_FLOW_FIELD_TAG;
	set_tag->dst.tag_index = RTE_PMD_MLX5_LINEAR_HASH_TAG_INDEX;
	/* Construct new actions array and replace QUEUE/RSS action. */
	prefix_act[qrss_idx] = (struct rte_flow_action) {
		.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
		.conf = set_tag,
	};
	/* JUMP action to jump to mreg copy table (CP_TBL). */
	jump = (void *)(set_tag + 1);
	*jump = (struct rte_flow_action_jump) {
		.group = MLX5_FLOW_MREG_CP_TABLE_GROUP,
	};
	prefix_act[actions_n - 2] = (struct rte_flow_action) {
		.type = RTE_FLOW_ACTION_TYPE_JUMP,
		.conf = jump,
	};
	prefix_act[actions_n - 1] = (struct rte_flow_action) {
		.type = RTE_FLOW_ACTION_TYPE_END,
	};
	/* Copy the suffix Q/RSS action, can also be indirect RSS. */
	suffix_act[0] = (struct rte_flow_action) {
		.type = qrss->type,
		.conf = qrss->conf,
	};
	suffix_act[1] = (struct rte_flow_action) {
		.type = RTE_FLOW_ACTION_TYPE_END,
	};
	return flow_id;
}

/*
 * Generate new attribute and items for suffix flows.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] split_attr
 *   Pointer to attribute for prefix flow rule.
 * @param[in] split_items
 *   Pointer to actions for suffix flow rule.
 * @param[in] qrss_id
 *   Prefix flow ID to match.
 */
static void
mlx5_flow_nta_split_qrss_items_prep(struct rte_eth_dev *dev,
				    struct rte_flow_attr *split_attr,
				    struct rte_flow_item *split_items,
				    uint32_t qrss_id)
{
	struct mlx5_rte_flow_item_tag *q_tag_spec;

	/* MLX5_FLOW_MREG_CP_TABLE_GROUP -> MLX5_FLOW_MREG_ACT_TABLE_GROUP(Q/RSS base) */
	split_attr->ingress = 1;
	split_attr->group = MLX5_FLOW_MREG_ACT_TABLE_GROUP;
	/* Only internal tag will be used, together with the item flags for RSS. */
	q_tag_spec = (void *)((char *)split_items + 2 * sizeof(struct rte_flow_item));
	split_items[0].type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_TAG;
	split_items[0].spec = q_tag_spec;
	split_items[1].type = RTE_FLOW_ITEM_TYPE_END;
	q_tag_spec->data = qrss_id;
	q_tag_spec->id = (enum modify_reg)
			 flow_hw_get_reg_id_by_domain(dev, RTE_FLOW_ITEM_TYPE_TAG,
						      MLX5DR_TABLE_TYPE_NIC_RX,
						      RTE_PMD_MLX5_LINEAR_HASH_TAG_INDEX);
	MLX5_ASSERT(q_tag_spec->id != REG_NON);
}

/*
 * Checking the split information and split the actions, items, attributes into
 * prefix and suffix to connect the flows after passing the copy tables.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in] actions
 *   Pointer to the original actions list.
 * @param[in] qrss
 *   Pointer to the action of QUEUE / RSS.
 * @param[in] action_flags
 *   Holds the actions detected.
 * @param[in] actions_n
 *   Number of original actions.
 * @param[in] external
 *   This flow rule is created by request external to PMD.
 * @param[out] res
 *   Pointer to the resource to store the split result.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   - Positive 1 on succeed.
 *   - 0 on no split.
 *   - negative errno value on error.
 */
int
mlx5_flow_nta_split_metadata(struct rte_eth_dev *dev,
			     const struct rte_flow_attr *attr,
			     const struct rte_flow_action actions[],
			     const struct rte_flow_action *qrss,
			     uint64_t action_flags,
			     int actions_n,
			     bool external,
			     struct mlx5_flow_hw_split_resource *res,
			     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_sh_config *config = &priv->sh->config;
	const struct rte_flow_action_queue *queue;
	const struct rte_flow_action_rss *rss;
	struct rte_flow_action *prfx_actions;
	struct rte_flow_action *sfx_actions;
	struct rte_flow_attr *sfx_attr;
	struct rte_flow_item *sfx_items;
	size_t pefx_act_size, sfx_act_size;
	size_t attr_size, item_size;
	size_t total_size;
	uint32_t qrss_id;

	/*
	 * The metadata copy flow should be created:
	 *   1. only on NIC Rx domain with Q / RSS
	 *   2. only when extended metadata mode is enabled
	 *   3. only on HWS, should always be "config->dv_flow_en == 2", this
	 *      checking can be skipped
	 * Note:
	 *   1. Even if metadata is not enabled in the data-path, it can still
	 *      be used to match on the Rx side.
	 *   2. The HWS Tx default copy rule or SQ rules already have the metadata
	 *      copy on the root table. The user's rule will always be inserted
	 *      and executed after the root table steering.
	 */
	if (config->dv_xmeta_en == MLX5_XMETA_MODE_LEGACY || attr->transfer ||
	    attr->egress || !external || !qrss)
		return 0;
	if (action_flags & MLX5_FLOW_ACTION_QUEUE) {
		queue = (const struct rte_flow_action_queue *)actions->conf;
		if (mlx5_rxq_is_hairpin(dev, queue->index))
			return 0;
	} else if (action_flags & MLX5_FLOW_ACTION_RSS) {
		rss = (const struct rte_flow_action_rss *)actions->conf;
		if (mlx5_rxq_is_hairpin(dev, rss->queue_num))
			return 0;
	}
	/* The prefix and suffix flows' actions. */
	pefx_act_size = sizeof(struct rte_flow_action) * (actions_n + 1) +
			sizeof(struct rte_flow_action_modify_field) +
			sizeof(struct rte_flow_action_jump);
	sfx_act_size = sizeof(struct rte_flow_action) * 2;
	/* The suffix attribute. */
	attr_size = sizeof(struct rte_flow_attr);
	/* The suffix items - mlx5_tag + end. */
	item_size = sizeof(struct rte_flow_item) * 2 +
		    sizeof(struct mlx5_rte_flow_item_tag);
	total_size = pefx_act_size + sfx_act_size + attr_size + item_size;
	prfx_actions = mlx5_malloc(MLX5_MEM_ZERO, total_size, 0, SOCKET_ID_ANY);
	if (!prfx_actions)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "no memory to split "
					  "metadata flow");
	sfx_actions = (void *)((char *)prfx_actions + pefx_act_size);
	qrss_id = mlx5_flow_nta_split_qrss_actions_prep(dev, prfx_actions,
							sfx_actions, actions,
							qrss, actions_n, error);
	if (!qrss_id) {
		mlx5_free(prfx_actions);
		return -rte_errno;
	}
	sfx_attr = (void *)((char *)sfx_actions + sfx_act_size);
	sfx_items = (void *)((char *)sfx_attr + attr_size);
	mlx5_flow_nta_split_qrss_items_prep(dev, sfx_attr, sfx_items, qrss_id);
	res->prefix.actions = prfx_actions;
	res->suffix.actions = sfx_actions;
	res->suffix.items = sfx_items;
	res->suffix.attr = sfx_attr;
	res->buf_start = prfx_actions;
	res->flow_idx = qrss_id;
	return 1;
}

/*
 * Release the buffer and flow ID.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] res
 *   Pointer to the resource to release.
 */
void
mlx5_flow_nta_split_resource_free(struct rte_eth_dev *dev,
				  struct mlx5_flow_hw_split_resource *res)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_RSS_EXPANTION_FLOW_ID], res->flow_idx);
	mlx5_free(res->buf_start);
}

/*
 * Callback functions for the metadata copy and mark / flag set flow.
 * The create and remove cannot reuse the DV since the flow opaque and structure
 * are different, and the action used to copy the metadata is also different.
 */
struct mlx5_list_entry *
flow_nta_mreg_create_cb(void *tool_ctx, void *cb_ctx)
{
	struct rte_eth_dev *dev = tool_ctx;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_mreg_copy_resource *mcp_res;
	struct rte_flow_error *error = ctx->error;
	uint32_t idx = 0;
	uint32_t mark_id = *(uint32_t *)(ctx->data);
	struct rte_flow_attr attr = {
		.group = MLX5_FLOW_MREG_CP_TABLE_GROUP,
		.ingress = 1,
	};
	struct mlx5_rte_flow_item_tag tag_spec = {
		.id = REG_C_0,
		.data = mark_id,
	};
	struct mlx5_rte_flow_item_tag tag_mask = {
		.data = priv->sh->dv_mark_mask,
	};
	struct rte_flow_action_mark ftag = {
		.id = mark_id,
	};
	struct rte_flow_action_modify_field rx_meta = {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.tag_index = REG_B,
		},
		.src = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.tag_index = REG_C_1,
		},
		.width = 32,
	};
	struct rte_flow_action_jump jump = {
		.group = MLX5_FLOW_MREG_ACT_TABLE_GROUP,
	};
	struct rte_flow_item items[2];
	struct rte_flow_action actions[4];

	/* Provide the full width of FLAG specific value. */
	if (mark_id == (priv->sh->dv_regc0_mask & MLX5_FLOW_MARK_DEFAULT))
		tag_spec.data = MLX5_FLOW_MARK_DEFAULT;
	/* Build a new flow. */
	if (mark_id != MLX5_DEFAULT_COPY_ID) {
		items[0] = (struct rte_flow_item) {
			.type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_TAG,
			.spec = &tag_spec,
			.mask = &tag_mask,
		};
		actions[0] = (struct rte_flow_action) {
			.type = RTE_FLOW_ACTION_TYPE_MARK,
			.conf = &ftag,
		};
	} else {
		/* Default rule, wildcard match with lowest priority. */
		attr.priority = MLX5_FLOW_LOWEST_PRIO_INDICATOR;
		items[0] = (struct rte_flow_item) {
			.type = RTE_FLOW_ITEM_TYPE_ETH,
		};
		actions[0] = (struct rte_flow_action) {
			.type = RTE_FLOW_ACTION_TYPE_VOID,
		};
	}
	/* (match REG 'tag') or all. */
	items[1].type = RTE_FLOW_ITEM_TYPE_END;
	/* (Mark) or void + copy to Rx meta + jump to the MREG_ACT_TABLE_GROUP. */
	actions[1].type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD;
	actions[1].conf = &rx_meta;
	actions[2].type = RTE_FLOW_ACTION_TYPE_JUMP;
	actions[2].conf = &jump;
	actions[3].type = RTE_FLOW_ACTION_TYPE_END;
	/* Build a new entry. */
	mcp_res = mlx5_ipool_zmalloc(priv->sh->ipool[MLX5_IPOOL_MCP], &idx);
	if (!mcp_res) {
		rte_errno = ENOMEM;
		return NULL;
	}
	mcp_res->idx = idx;
	mcp_res->mark_id = mark_id;
	/*
	 * The copy flows are not included in any list. There
	 * ones are referenced from other flows and cannot
	 * be applied, removed, deleted in arbitrary order
	 * by list traversing.
	 */
	mcp_res->hw_flow = mlx5_flow_list_create(dev, MLX5_FLOW_TYPE_MCP, &attr,
						 items, actions, false, error);
	if (!mcp_res->hw_flow) {
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MCP], idx);
		return NULL;
	}
	return &mcp_res->hlist_ent;
}

void
flow_nta_mreg_remove_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_flow_mreg_copy_resource *mcp_res =
			       container_of(entry, typeof(*mcp_res), hlist_ent);
	struct rte_eth_dev *dev = tool_ctx;
	struct mlx5_priv *priv = dev->data->dev_private;

	MLX5_ASSERT(mcp_res->hw_flow);
	mlx5_flow_list_destroy(dev, MLX5_FLOW_TYPE_MCP, mcp_res->hw_flow);
	mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MCP], mcp_res->idx);
}

/*
 * Add a flow of copying flow metadata registers in RX_CP_TBL.
 * @see flow_mreg_add_copy_action
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mark_id
 *   ID of MARK action, zero means default flow for META.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   Associated resource on success, NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_mreg_copy_resource *
mlx5_flow_nta_add_copy_action(struct rte_eth_dev *dev,
			      uint32_t mark_id,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_list_entry *entry;
	uint32_t specialize = 0;
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = error,
		.data = &mark_id,
		.data2 = &specialize,
	};

	/* Check if already registered. */
	MLX5_ASSERT(priv->sh->mreg_cp_tbl);
	entry = mlx5_hlist_register(priv->sh->mreg_cp_tbl, mark_id, &ctx);
	if (!entry)
		return NULL;
	return container_of(entry, struct mlx5_flow_mreg_copy_resource, hlist_ent);
}

/*
 * Release flow in RX_CP_TBL.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] idx
 *   Index in the pool to store the copy flow.
 */
void
mlx5_flow_nta_del_copy_action(struct rte_eth_dev *dev, uint32_t idx)
{
	struct mlx5_flow_mreg_copy_resource *mcp_res;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!idx)
		return;
	mcp_res = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_MCP], idx);
	if (!mcp_res || !priv->sh->mreg_cp_tbl)
		return;
	MLX5_ASSERT(mcp_res->hw_flow);
	mlx5_hlist_unregister(priv->sh->mreg_cp_tbl, &mcp_res->hlist_ent);
}

/*
 * Remove the default copy action from RX_CP_TBL.
 * @see flow_mreg_del_default_copy_action
 *
 * This functions is called in the mlx5_dev_start(). No thread safe
 * is guaranteed.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 */
void
mlx5_flow_nta_del_default_copy_action(struct rte_eth_dev *dev)
{
	struct mlx5_list_entry *entry;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_cb_ctx ctx;
	uint32_t mark_id;

	/* Check if default flow is registered. */
	if (!priv->sh->mreg_cp_tbl)
		return;
	mark_id = MLX5_DEFAULT_COPY_ID;
	ctx.data = &mark_id;
	entry = mlx5_hlist_lookup(priv->sh->mreg_cp_tbl, mark_id, &ctx);
	if (!entry)
		return;
	mlx5_hlist_unregister(priv->sh->mreg_cp_tbl, entry);
}

/*
 * Add the default copy action in RX_CP_TBL.
 *
 * This functions is called in the mlx5_dev_start(). No thread safe
 * is guaranteed.
 * @see flow_mreg_add_default_copy_action
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 for success, negative value otherwise and rte_errno is set.
 */
int
mlx5_flow_nta_add_default_copy_action(struct rte_eth_dev *dev,
				      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_sh_config *config = &priv->sh->config;
	struct mlx5_flow_mreg_copy_resource *mcp_res;
	struct mlx5_flow_cb_ctx ctx;
	uint32_t mark_id;

	if (config->dv_xmeta_en == MLX5_XMETA_MODE_LEGACY ||
	    !priv->sh->dv_regc0_mask)
		return 0;
	/*
	 * Add default mreg copy flow may be called multiple time, but
	 * only be called once in stop. Avoid register it twice.
	 */
	mark_id = MLX5_DEFAULT_COPY_ID;
	ctx.data = &mark_id;
	if (mlx5_hlist_lookup(priv->sh->mreg_cp_tbl, mark_id, &ctx))
		return 0;
	mcp_res = mlx5_flow_nta_add_copy_action(dev, mark_id, error);
	if (!mcp_res)
		return -rte_errno;
	return 0;
}

/*
 * Add a flow of copying flow metadata registers in RX_CP_TBL.
 * @see flow_mreg_update_copy_table
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] idx
 *   Pointer to store the index of flow in the pool.
 * @param[in] mark
 *   Pointer to mark or flag action.
 * @param[in] action_flags
 *   Holds the actions detected.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, negative value otherwise and rte_errno is set.
 */
int
mlx5_flow_nta_update_copy_table(struct rte_eth_dev *dev,
				uint32_t *idx,
				const struct rte_flow_action *mark,
				uint64_t action_flags,
				struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_sh_config *config = &priv->sh->config;
	struct mlx5_flow_mreg_copy_resource *mcp_res;
	const struct rte_flow_action_mark *mark_conf;
	uint32_t mark_id;

	if (config->dv_xmeta_en == MLX5_XMETA_MODE_LEGACY ||
	    !priv->sh->dv_regc0_mask)
		return 0;
	/* Find MARK action. */
	if (action_flags & (MLX5_FLOW_ACTION_FLAG | MLX5_FLOW_ACTION_MARK)) {
		if (mark) {
			mark_conf = (const struct rte_flow_action_mark *)mark->conf;
			mark_id = mark_conf->id;
		} else {
			mark_id = MLX5_FLOW_MARK_DEFAULT;
		}
		mcp_res = mlx5_flow_nta_add_copy_action(dev, mark_id, error);
		if (!mcp_res)
			return -rte_errno;
		*idx = mcp_res->idx;
	}
	return 0;
}

#endif
