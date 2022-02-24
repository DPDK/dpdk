/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <rte_flow.h>

#include <mlx5_malloc.h>
#include "mlx5_defs.h"
#include "mlx5_flow.h"

#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)

const struct mlx5_flow_driver_ops mlx5_flow_hw_drv_ops;

/* DR action flags with different table. */
static uint32_t mlx5_hw_act_flag[MLX5_HW_ACTION_FLAG_MAX]
				[MLX5DR_TABLE_TYPE_MAX] = {
	{
		MLX5DR_ACTION_FLAG_ROOT_RX,
		MLX5DR_ACTION_FLAG_ROOT_TX,
		MLX5DR_ACTION_FLAG_ROOT_FDB,
	},
	{
		MLX5DR_ACTION_FLAG_HWS_RX,
		MLX5DR_ACTION_FLAG_HWS_TX,
		MLX5DR_ACTION_FLAG_HWS_FDB,
	},
};

/**
 * Destroy DR actions created by action template.
 *
 * For DR actions created during table creation's action translate.
 * Need to destroy the DR action when destroying the table.
 *
 * @param[in] acts
 *   Pointer to the template HW steering DR actions.
 */
static void
__flow_hw_action_template_destroy(struct mlx5_hw_actions *acts __rte_unused)
{
}

/**
 * Translate rte_flow actions to DR action.
 *
 * As the action template has already indicated the actions. Translate
 * the rte_flow actions to DR action if possbile. So in flow create
 * stage we will save cycles from handing the actions' organizing.
 * For the actions with limited information, need to add these to a
 * list.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] table_attr
 *   Pointer to the table attributes.
 * @param[in] item_templates
 *   Item template array to be binded to the table.
 * @param[in/out] acts
 *   Pointer to the template HW steering DR actions.
 * @param[in] at
 *   Action template.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    Table on success, NULL otherwise and rte_errno is set.
 */
static int
flow_hw_actions_translate(struct rte_eth_dev *dev,
			  const struct rte_flow_template_table_attr *table_attr,
			  struct mlx5_hw_actions *acts,
			  struct rte_flow_actions_template *at,
			  struct rte_flow_error *error __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_attr *attr = &table_attr->flow_attr;
	struct rte_flow_action *actions = at->actions;
	struct rte_flow_action *masks = at->masks;
	bool actions_end = false;
	uint32_t type;

	if (attr->transfer)
		type = MLX5DR_TABLE_TYPE_FDB;
	else if (attr->egress)
		type = MLX5DR_TABLE_TYPE_NIC_TX;
	else
		type = MLX5DR_TABLE_TYPE_NIC_RX;
	for (; !actions_end; actions++, masks++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			break;
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			acts->drop = priv->hw_drop[!!attr->group][type];
			break;
		case RTE_FLOW_ACTION_TYPE_END:
			actions_end = true;
			break;
		default:
			break;
		}
	}
	return 0;
}

/**
 * Create flow table.
 *
 * The input item and action templates will be binded to the table.
 * Flow memory will also be allocated. Matcher will be created based
 * on the item template. Action will be translated to the dedicated
 * DR action if possible.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] attr
 *   Pointer to the table attributes.
 * @param[in] item_templates
 *   Item template array to be binded to the table.
 * @param[in] nb_item_templates
 *   Number of item template.
 * @param[in] action_templates
 *   Action template array to be binded to the table.
 * @param[in] nb_action_templates
 *   Number of action template.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    Table on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow_template_table *
flow_hw_table_create(struct rte_eth_dev *dev,
		     const struct rte_flow_template_table_attr *attr,
		     struct rte_flow_pattern_template *item_templates[],
		     uint8_t nb_item_templates,
		     struct rte_flow_actions_template *action_templates[],
		     uint8_t nb_action_templates,
		     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5dr_matcher_attr matcher_attr = {0};
	struct rte_flow_template_table *tbl = NULL;
	struct mlx5_flow_group *grp;
	struct mlx5dr_match_template *mt[MLX5_HW_TBL_MAX_ITEM_TEMPLATE];
	struct rte_flow_attr flow_attr = attr->flow_attr;
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = error,
		.data = &flow_attr,
	};
	struct mlx5_indexed_pool_config cfg = {
		.size = sizeof(struct rte_flow),
		.trunk_size = 1 << 12,
		.per_core_cache = 1 << 13,
		.need_lock = 1,
		.release_mem_en = !!priv->sh->config.reclaim_mode,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_hw_table_flow",
	};
	struct mlx5_list_entry *ge;
	uint32_t i, max_tpl = MLX5_HW_TBL_MAX_ITEM_TEMPLATE;
	uint32_t nb_flows = rte_align32pow2(attr->nb_flows);
	int err;

	/* HWS layer accepts only 1 item template with root table. */
	if (!attr->flow_attr.group)
		max_tpl = 1;
	cfg.max_idx = nb_flows;
	/* For table has very limited flows, disable cache. */
	if (nb_flows < cfg.trunk_size) {
		cfg.per_core_cache = 0;
		cfg.trunk_size = nb_flows;
	}
	/* Check if we requires too many templates. */
	if (nb_item_templates > max_tpl ||
	    nb_action_templates > MLX5_HW_TBL_MAX_ACTION_TEMPLATE) {
		rte_errno = EINVAL;
		goto error;
	}
	/* Allocate the table memory. */
	tbl = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*tbl), 0, rte_socket_id());
	if (!tbl)
		goto error;
	/* Allocate flow indexed pool. */
	tbl->flow = mlx5_ipool_create(&cfg);
	if (!tbl->flow)
		goto error;
	/* Register the flow group. */
	ge = mlx5_hlist_register(priv->sh->groups, attr->flow_attr.group, &ctx);
	if (!ge)
		goto error;
	grp = container_of(ge, struct mlx5_flow_group, entry);
	tbl->grp = grp;
	/* Prepare matcher information. */
	matcher_attr.priority = attr->flow_attr.priority;
	matcher_attr.mode = MLX5DR_MATCHER_RESOURCE_MODE_RULE;
	matcher_attr.rule.num_log = rte_log2_u32(nb_flows);
	/* Build the item template. */
	for (i = 0; i < nb_item_templates; i++) {
		uint32_t ret;

		ret = __atomic_add_fetch(&item_templates[i]->refcnt, 1,
					 __ATOMIC_RELAXED);
		if (ret <= 1) {
			rte_errno = EINVAL;
			goto it_error;
		}
		mt[i] = item_templates[i]->mt;
		tbl->its[i] = item_templates[i];
	}
	tbl->matcher = mlx5dr_matcher_create
		(tbl->grp->tbl, mt, nb_item_templates, &matcher_attr);
	if (!tbl->matcher)
		goto it_error;
	tbl->nb_item_templates = nb_item_templates;
	/* Build the action template. */
	for (i = 0; i < nb_action_templates; i++) {
		uint32_t ret;

		ret = __atomic_add_fetch(&action_templates[i]->refcnt, 1,
					 __ATOMIC_RELAXED);
		if (ret <= 1) {
			rte_errno = EINVAL;
			goto at_error;
		}
		err = flow_hw_actions_translate(dev, attr,
						&tbl->ats[i].acts,
						action_templates[i], error);
		if (err) {
			i++;
			goto at_error;
		}
		tbl->ats[i].action_template = action_templates[i];
	}
	tbl->nb_action_templates = nb_action_templates;
	tbl->type = attr->flow_attr.transfer ? MLX5DR_TABLE_TYPE_FDB :
		    (attr->flow_attr.egress ? MLX5DR_TABLE_TYPE_NIC_TX :
		    MLX5DR_TABLE_TYPE_NIC_RX);
	LIST_INSERT_HEAD(&priv->flow_hw_tbl, tbl, next);
	return tbl;
at_error:
	while (i--) {
		__flow_hw_action_template_destroy(&tbl->ats[i].acts);
		__atomic_sub_fetch(&action_templates[i]->refcnt,
				   1, __ATOMIC_RELAXED);
	}
	i = nb_item_templates;
it_error:
	while (i--)
		__atomic_sub_fetch(&item_templates[i]->refcnt,
				   1, __ATOMIC_RELAXED);
	mlx5dr_matcher_destroy(tbl->matcher);
error:
	err = rte_errno;
	if (tbl) {
		if (tbl->grp)
			mlx5_hlist_unregister(priv->sh->groups,
					      &tbl->grp->entry);
		if (tbl->flow)
			mlx5_ipool_destroy(tbl->flow);
		mlx5_free(tbl);
	}
	rte_flow_error_set(error, err,
			  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			  "fail to create rte table");
	return NULL;
}

/**
 * Destroy flow table.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] table
 *   Pointer to the table to be destroyed.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_table_destroy(struct rte_eth_dev *dev,
		      struct rte_flow_template_table *table,
		      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int i;

	if (table->refcnt) {
		DRV_LOG(WARNING, "Table %p is still in using.", (void *)table);
		return rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "table in using");
	}
	LIST_REMOVE(table, next);
	for (i = 0; i < table->nb_item_templates; i++)
		__atomic_sub_fetch(&table->its[i]->refcnt,
				   1, __ATOMIC_RELAXED);
	for (i = 0; i < table->nb_action_templates; i++) {
		__flow_hw_action_template_destroy(&table->ats[i].acts);
		__atomic_sub_fetch(&table->ats[i].action_template->refcnt,
				   1, __ATOMIC_RELAXED);
	}
	mlx5dr_matcher_destroy(table->matcher);
	mlx5_hlist_unregister(priv->sh->groups, &table->grp->entry);
	mlx5_ipool_destroy(table->flow);
	mlx5_free(table);
	return 0;
}

/**
 * Create flow action template.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] attr
 *   Pointer to the action template attributes.
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[in] masks
 *   List of actions that marks which of the action's member is constant.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   Action template pointer on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow_actions_template *
flow_hw_actions_template_create(struct rte_eth_dev *dev,
			const struct rte_flow_actions_template_attr *attr,
			const struct rte_flow_action actions[],
			const struct rte_flow_action masks[],
			struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int len, act_len, mask_len, i;
	struct rte_flow_actions_template *at;

	act_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS,
				NULL, 0, actions, error);
	if (act_len <= 0)
		return NULL;
	len = RTE_ALIGN(act_len, 16);
	mask_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS,
				 NULL, 0, masks, error);
	if (mask_len <= 0)
		return NULL;
	len += RTE_ALIGN(mask_len, 16);
	at = mlx5_malloc(MLX5_MEM_ZERO, len + sizeof(*at), 64, rte_socket_id());
	if (!at) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate action template");
		return NULL;
	}
	at->attr = *attr;
	at->actions = (struct rte_flow_action *)(at + 1);
	act_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, at->actions, len,
				actions, error);
	if (act_len <= 0)
		goto error;
	at->masks = (struct rte_flow_action *)
		    (((uint8_t *)at->actions) + act_len);
	mask_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, at->masks,
				 len - act_len, masks, error);
	if (mask_len <= 0)
		goto error;
	/*
	 * mlx5 PMD hacks indirect action index directly to the action conf.
	 * The rte_flow_conv() function copies the content from conf pointer.
	 * Need to restore the indirect action index from action conf here.
	 */
	for (i = 0; actions->type != RTE_FLOW_ACTION_TYPE_END;
	     actions++, masks++, i++) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_INDIRECT) {
			at->actions[i].conf = actions->conf;
			at->masks[i].conf = masks->conf;
		}
	}
	__atomic_fetch_add(&at->refcnt, 1, __ATOMIC_RELAXED);
	LIST_INSERT_HEAD(&priv->flow_hw_at, at, next);
	return at;
error:
	mlx5_free(at);
	return NULL;
}

/**
 * Destroy flow action template.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] template
 *   Pointer to the action template to be destroyed.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_actions_template_destroy(struct rte_eth_dev *dev __rte_unused,
				 struct rte_flow_actions_template *template,
				 struct rte_flow_error *error __rte_unused)
{
	if (__atomic_load_n(&template->refcnt, __ATOMIC_RELAXED) > 1) {
		DRV_LOG(WARNING, "Action template %p is still in use.",
			(void *)template);
		return rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "action template in using");
	}
	LIST_REMOVE(template, next);
	mlx5_free(template);
	return 0;
}

/**
 * Create flow item template.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] attr
 *   Pointer to the item template attributes.
 * @param[in] items
 *   The template item pattern.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *  Item template pointer on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow_pattern_template *
flow_hw_pattern_template_create(struct rte_eth_dev *dev,
			     const struct rte_flow_pattern_template_attr *attr,
			     const struct rte_flow_item items[],
			     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_pattern_template *it;

	it = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*it), 0, rte_socket_id());
	if (!it) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate item template");
		return NULL;
	}
	it->attr = *attr;
	it->mt = mlx5dr_match_template_create(items, attr->relaxed_matching);
	if (!it->mt) {
		mlx5_free(it);
		rte_flow_error_set(error, rte_errno,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot create match template");
		return NULL;
	}
	__atomic_fetch_add(&it->refcnt, 1, __ATOMIC_RELAXED);
	LIST_INSERT_HEAD(&priv->flow_hw_itt, it, next);
	return it;
}

/**
 * Destroy flow item template.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] template
 *   Pointer to the item template to be destroyed.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_pattern_template_destroy(struct rte_eth_dev *dev __rte_unused,
			      struct rte_flow_pattern_template *template,
			      struct rte_flow_error *error __rte_unused)
{
	if (__atomic_load_n(&template->refcnt, __ATOMIC_RELAXED) > 1) {
		DRV_LOG(WARNING, "Item template %p is still in use.",
			(void *)template);
		return rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "item template in using");
	}
	LIST_REMOVE(template, next);
	claim_zero(mlx5dr_match_template_destroy(template->mt));
	mlx5_free(template);
	return 0;
}

/*
 * Get information about HWS pre-configurable resources.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[out] port_info
 *   Pointer to port information.
 * @param[out] queue_info
 *   Pointer to queue information.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_info_get(struct rte_eth_dev *dev __rte_unused,
		 struct rte_flow_port_info *port_info __rte_unused,
		 struct rte_flow_queue_info *queue_info __rte_unused,
		 struct rte_flow_error *error __rte_unused)
{
	/* Nothing to be updated currently. */
	memset(port_info, 0, sizeof(*port_info));
	/* Queue size is unlimited from low-level. */
	queue_info->max_size = UINT32_MAX;
	return 0;
}

/**
 * Create group callback.
 *
 * @param[in] tool_ctx
 *   Pointer to the hash list related context.
 * @param[in] cb_ctx
 *   Pointer to the group creation context.
 *
 * @return
 *   Group entry on success, NULL otherwise and rte_errno is set.
 */
struct mlx5_list_entry *
flow_hw_grp_create_cb(void *tool_ctx, void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct rte_eth_dev *dev = ctx->dev;
	struct rte_flow_attr *attr = (struct rte_flow_attr *)ctx->data;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5dr_table_attr dr_tbl_attr = {0};
	struct rte_flow_error *error = ctx->error;
	struct mlx5_flow_group *grp_data;
	struct mlx5dr_table *tbl = NULL;
	struct mlx5dr_action *jump;
	uint32_t idx = 0;

	grp_data = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_HW_GRP], &idx);
	if (!grp_data) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate flow table data entry");
		return NULL;
	}
	dr_tbl_attr.level = attr->group;
	if (attr->transfer)
		dr_tbl_attr.type = MLX5DR_TABLE_TYPE_FDB;
	else if (attr->egress)
		dr_tbl_attr.type = MLX5DR_TABLE_TYPE_NIC_TX;
	else
		dr_tbl_attr.type = MLX5DR_TABLE_TYPE_NIC_RX;
	tbl = mlx5dr_table_create(priv->dr_ctx, &dr_tbl_attr);
	if (!tbl)
		goto error;
	grp_data->tbl = tbl;
	if (attr->group) {
		/* Jump action be used by non-root table. */
		jump = mlx5dr_action_create_dest_table
			(priv->dr_ctx, tbl,
			 mlx5_hw_act_flag[!!attr->group][dr_tbl_attr.type]);
		if (!jump)
			goto error;
		grp_data->jump.hws_action = jump;
		/* Jump action be used by root table.  */
		jump = mlx5dr_action_create_dest_table
			(priv->dr_ctx, tbl,
			 mlx5_hw_act_flag[MLX5_HW_ACTION_FLAG_ROOT]
					 [dr_tbl_attr.type]);
		if (!jump)
			goto error;
		grp_data->jump.root_action = jump;
	}
	grp_data->idx = idx;
	grp_data->group_id = attr->group;
	grp_data->type = dr_tbl_attr.type;
	return &grp_data->entry;
error:
	if (grp_data->jump.root_action)
		mlx5dr_action_destroy(grp_data->jump.root_action);
	if (grp_data->jump.hws_action)
		mlx5dr_action_destroy(grp_data->jump.hws_action);
	if (tbl)
		mlx5dr_table_destroy(tbl);
	if (idx)
		mlx5_ipool_free(sh->ipool[MLX5_IPOOL_HW_GRP], idx);
	rte_flow_error_set(error, ENOMEM,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL,
			   "cannot allocate flow dr table");
	return NULL;
}

/**
 * Remove group callback.
 *
 * @param[in] tool_ctx
 *   Pointer to the hash list related context.
 * @param[in] entry
 *   Pointer to the entry to be removed.
 */
void
flow_hw_grp_remove_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_group *grp_data =
		    container_of(entry, struct mlx5_flow_group, entry);

	MLX5_ASSERT(entry && sh);
	/* To use the wrapper glue functions instead. */
	if (grp_data->jump.hws_action)
		mlx5dr_action_destroy(grp_data->jump.hws_action);
	if (grp_data->jump.root_action)
		mlx5dr_action_destroy(grp_data->jump.root_action);
	mlx5dr_table_destroy(grp_data->tbl);
	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_HW_GRP], grp_data->idx);
}

/**
 * Match group callback.
 *
 * @param[in] tool_ctx
 *   Pointer to the hash list related context.
 * @param[in] entry
 *   Pointer to the group to be matched.
 * @param[in] cb_ctx
 *   Pointer to the group matching context.
 *
 * @return
 *   0 on matched, 1 on miss matched.
 */
int
flow_hw_grp_match_cb(void *tool_ctx __rte_unused, struct mlx5_list_entry *entry,
		     void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_group *grp_data =
		container_of(entry, struct mlx5_flow_group, entry);
	struct rte_flow_attr *attr =
			(struct rte_flow_attr *)ctx->data;

	return (grp_data->group_id != attr->group) ||
		((grp_data->type != MLX5DR_TABLE_TYPE_FDB) &&
		attr->transfer) ||
		((grp_data->type != MLX5DR_TABLE_TYPE_NIC_TX) &&
		attr->egress) ||
		((grp_data->type != MLX5DR_TABLE_TYPE_NIC_RX) &&
		attr->ingress);
}

/**
 * Clone group entry callback.
 *
 * @param[in] tool_ctx
 *   Pointer to the hash list related context.
 * @param[in] entry
 *   Pointer to the group to be matched.
 * @param[in] cb_ctx
 *   Pointer to the group matching context.
 *
 * @return
 *   0 on matched, 1 on miss matched.
 */
struct mlx5_list_entry *
flow_hw_grp_clone_cb(void *tool_ctx, struct mlx5_list_entry *oentry,
		     void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_group *grp_data;
	struct rte_flow_error *error = ctx->error;
	uint32_t idx = 0;

	grp_data = mlx5_ipool_malloc(sh->ipool[MLX5_IPOOL_HW_GRP], &idx);
	if (!grp_data) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate flow table data entry");
		return NULL;
	}
	memcpy(grp_data, oentry, sizeof(*grp_data));
	grp_data->idx = idx;
	return &grp_data->entry;
}

/**
 * Free cloned group entry callback.
 *
 * @param[in] tool_ctx
 *   Pointer to the hash list related context.
 * @param[in] entry
 *   Pointer to the group to be freed.
 */
void
flow_hw_grp_clone_free_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_group *grp_data =
		    container_of(entry, struct mlx5_flow_group, entry);

	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_HW_GRP], grp_data->idx);
}

/**
 * Configure port HWS resources.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] port_attr
 *   Port configuration attributes.
 * @param[in] nb_queue
 *   Number of queue.
 * @param[in] queue_attr
 *   Array that holds attributes for each flow queue.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */

static int
flow_hw_configure(struct rte_eth_dev *dev,
		  const struct rte_flow_port_attr *port_attr,
		  uint16_t nb_queue,
		  const struct rte_flow_queue_attr *queue_attr[],
		  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5dr_context *dr_ctx = NULL;
	struct mlx5dr_context_attr dr_ctx_attr = {0};
	struct mlx5_hw_q *hw_q;
	struct mlx5_hw_q_job *job = NULL;
	uint32_t mem_size, i, j;

	if (!port_attr || !nb_queue || !queue_attr) {
		rte_errno = EINVAL;
		goto err;
	}
	/* In case re-configuring, release existing context at first. */
	if (priv->dr_ctx) {
		/* */
		for (i = 0; i < nb_queue; i++) {
			hw_q = &priv->hw_q[i];
			/* Make sure all queues are empty. */
			if (hw_q->size != hw_q->job_idx) {
				rte_errno = EBUSY;
				goto err;
			}
		}
		flow_hw_resource_release(dev);
	}
	/* Allocate the queue job descriptor LIFO. */
	mem_size = sizeof(priv->hw_q[0]) * nb_queue;
	for (i = 0; i < nb_queue; i++) {
		/*
		 * Check if the queues' size are all the same as the
		 * limitation from HWS layer.
		 */
		if (queue_attr[i]->size != queue_attr[0]->size) {
			rte_errno = EINVAL;
			goto err;
		}
		mem_size += (sizeof(struct mlx5_hw_q_job *) +
			    sizeof(struct mlx5_hw_q_job)) *
			    queue_attr[0]->size;
	}
	priv->hw_q = mlx5_malloc(MLX5_MEM_ZERO, mem_size,
				 64, SOCKET_ID_ANY);
	if (!priv->hw_q) {
		rte_errno = ENOMEM;
		goto err;
	}
	for (i = 0; i < nb_queue; i++) {
		priv->hw_q[i].job_idx = queue_attr[i]->size;
		priv->hw_q[i].size = queue_attr[i]->size;
		if (i == 0)
			priv->hw_q[i].job = (struct mlx5_hw_q_job **)
					    &priv->hw_q[nb_queue];
		else
			priv->hw_q[i].job = (struct mlx5_hw_q_job **)
					    &job[queue_attr[i - 1]->size];
		job = (struct mlx5_hw_q_job *)
		      &priv->hw_q[i].job[queue_attr[i]->size];
		for (j = 0; j < queue_attr[i]->size; j++)
			priv->hw_q[i].job[j] = &job[j];
	}
	dr_ctx_attr.pd = priv->sh->cdev->pd;
	dr_ctx_attr.queues = nb_queue;
	/* Queue size should all be the same. Take the first one. */
	dr_ctx_attr.queue_size = queue_attr[0]->size;
	dr_ctx = mlx5dr_context_open(priv->sh->cdev->ctx, &dr_ctx_attr);
	/* rte_errno has been updated by HWS layer. */
	if (!dr_ctx)
		goto err;
	priv->dr_ctx = dr_ctx;
	priv->nb_queue = nb_queue;
	/* Add global actions. */
	for (i = 0; i < MLX5_HW_ACTION_FLAG_MAX; i++) {
		for (j = 0; j < MLX5DR_TABLE_TYPE_MAX; j++) {
			priv->hw_drop[i][j] = mlx5dr_action_create_dest_drop
				(priv->dr_ctx, mlx5_hw_act_flag[i][j]);
			if (!priv->hw_drop[i][j])
				goto err;
		}
	}
	return 0;
err:
	for (i = 0; i < MLX5_HW_ACTION_FLAG_MAX; i++) {
		for (j = 0; j < MLX5DR_TABLE_TYPE_MAX; j++) {
			if (!priv->hw_drop[i][j])
				continue;
			mlx5dr_action_destroy(priv->hw_drop[i][j]);
		}
	}
	if (dr_ctx)
		claim_zero(mlx5dr_context_close(dr_ctx));
	mlx5_free(priv->hw_q);
	priv->hw_q = NULL;
	return rte_flow_error_set(error, rte_errno,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "fail to configure port");
}

/**
 * Release HWS resources.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 */
void
flow_hw_resource_release(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_template_table *tbl;
	struct rte_flow_pattern_template *it;
	struct rte_flow_actions_template *at;
	int i, j;

	if (!priv->dr_ctx)
		return;
	while (!LIST_EMPTY(&priv->flow_hw_tbl)) {
		tbl = LIST_FIRST(&priv->flow_hw_tbl);
		flow_hw_table_destroy(dev, tbl, NULL);
	}
	while (!LIST_EMPTY(&priv->flow_hw_itt)) {
		it = LIST_FIRST(&priv->flow_hw_itt);
		flow_hw_pattern_template_destroy(dev, it, NULL);
	}
	while (!LIST_EMPTY(&priv->flow_hw_at)) {
		at = LIST_FIRST(&priv->flow_hw_at);
		flow_hw_actions_template_destroy(dev, at, NULL);
	}
	for (i = 0; i < MLX5_HW_ACTION_FLAG_MAX; i++) {
		for (j = 0; j < MLX5DR_TABLE_TYPE_MAX; j++) {
			if (!priv->hw_drop[i][j])
				continue;
			mlx5dr_action_destroy(priv->hw_drop[i][j]);
		}
	}
	mlx5_free(priv->hw_q);
	priv->hw_q = NULL;
	claim_zero(mlx5dr_context_close(priv->dr_ctx));
	priv->dr_ctx = NULL;
	priv->nb_queue = 0;
}

const struct mlx5_flow_driver_ops mlx5_flow_hw_drv_ops = {
	.info_get = flow_hw_info_get,
	.configure = flow_hw_configure,
	.pattern_template_create = flow_hw_pattern_template_create,
	.pattern_template_destroy = flow_hw_pattern_template_destroy,
	.actions_template_create = flow_hw_actions_template_create,
	.actions_template_destroy = flow_hw_actions_template_destroy,
	.template_table_create = flow_hw_table_create,
	.template_table_destroy = flow_hw_table_destroy,
};

#endif
