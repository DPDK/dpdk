/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include "mlx5dr_internal.h"

static void mlx5dr_rule_skip(struct mlx5dr_matcher *matcher,
			     struct mlx5dr_match_template *mt,
			     const struct rte_flow_item *items,
			     bool *skip_rx, bool *skip_tx)
{
	const struct flow_hw_port_info *vport;
	const struct rte_flow_item_ethdev *v;

	/* Flow_src is the 1st priority */
	if (matcher->attr.optimize_flow_src) {
		*skip_tx = matcher->attr.optimize_flow_src == MLX5DR_MATCHER_FLOW_SRC_WIRE;
		*skip_rx = matcher->attr.optimize_flow_src == MLX5DR_MATCHER_FLOW_SRC_VPORT;
		return;
	}

	/* By default FDB rules are added to both RX and TX */
	*skip_rx = false;
	*skip_tx = false;

	if (mt->item_flags & MLX5_FLOW_ITEM_REPRESENTED_PORT) {
		v = items[mt->vport_item_id].spec;
		vport = flow_hw_conv_port_id(v->port_id);
		if (unlikely(!vport)) {
			DR_LOG(ERR, "Fail to map port ID %d, ignoring", v->port_id);
			return;
		}

		if (!vport->is_wire)
			/* Match vport ID is not WIRE -> Skip RX */
			*skip_rx = true;
		else
			/* Match vport ID is WIRE -> Skip TX */
			*skip_tx = true;
	}
}

static void
mlx5dr_rule_update_copy_tag(struct mlx5dr_rule *rule,
			    struct mlx5dr_wqe_gta_data_seg_ste *wqe_data,
			    bool is_jumbo)
{
	if (is_jumbo)
		memcpy(wqe_data->jumbo, rule->tag.jumbo, MLX5DR_JUMBO_TAG_SZ);
	else
		memcpy(wqe_data->tag, rule->tag.match, MLX5DR_MATCH_TAG_SZ);
}

static void mlx5dr_rule_init_dep_wqe(struct mlx5dr_send_ring_dep_wqe *dep_wqe,
				     struct mlx5dr_rule *rule,
				     const struct rte_flow_item *items,
				     struct mlx5dr_match_template *mt,
				     void *user_data)
{
	struct mlx5dr_matcher *matcher = rule->matcher;
	struct mlx5dr_table *tbl = matcher->tbl;
	bool skip_rx, skip_tx;

	dep_wqe->rule = rule;
	dep_wqe->user_data = user_data;

	if (!items) { /* rule update */
		dep_wqe->rtc_0 = rule->rtc_0;
		dep_wqe->rtc_1 = rule->rtc_1;
		dep_wqe->retry_rtc_1 = 0;
		dep_wqe->retry_rtc_0 = 0;
		return;
	}

	switch (tbl->type) {
	case MLX5DR_TABLE_TYPE_NIC_RX:
	case MLX5DR_TABLE_TYPE_NIC_TX:
		dep_wqe->rtc_0 = matcher->match_ste.rtc_0->id;
		dep_wqe->retry_rtc_0 = matcher->col_matcher ?
				       matcher->col_matcher->match_ste.rtc_0->id : 0;
		dep_wqe->rtc_1 = 0;
		dep_wqe->retry_rtc_1 = 0;
		break;

	case MLX5DR_TABLE_TYPE_FDB:
		mlx5dr_rule_skip(matcher, mt, items, &skip_rx, &skip_tx);

		if (!skip_rx) {
			dep_wqe->rtc_0 = matcher->match_ste.rtc_0->id;
			dep_wqe->retry_rtc_0 = matcher->col_matcher ?
					       matcher->col_matcher->match_ste.rtc_0->id : 0;
		} else {
			dep_wqe->rtc_0 = 0;
			dep_wqe->retry_rtc_0 = 0;
		}

		if (!skip_tx) {
			dep_wqe->rtc_1 = matcher->match_ste.rtc_1->id;
			dep_wqe->retry_rtc_1 = matcher->col_matcher ?
					       matcher->col_matcher->match_ste.rtc_1->id : 0;
		} else {
			dep_wqe->rtc_1 = 0;
			dep_wqe->retry_rtc_1 = 0;
		}

		break;

	default:
		assert(false);
		break;
	}
}

static void mlx5dr_rule_gen_comp(struct mlx5dr_send_engine *queue,
				 struct mlx5dr_rule *rule,
				 bool err,
				 void *user_data,
				 enum mlx5dr_rule_status rule_status_on_succ)
{
	enum rte_flow_op_status comp_status;

	if (!err) {
		comp_status = RTE_FLOW_OP_SUCCESS;
		rule->status = rule_status_on_succ;
	} else {
		comp_status = RTE_FLOW_OP_ERROR;
		rule->status = MLX5DR_RULE_STATUS_FAILED;
	}

	mlx5dr_send_engine_inc_rule(queue);
	mlx5dr_send_engine_gen_comp(queue, user_data, comp_status);
}

static void
mlx5dr_rule_save_delete_info(struct mlx5dr_rule *rule,
			     struct mlx5dr_send_ste_attr *ste_attr)
{
	struct mlx5dr_match_template *mt = rule->matcher->mt;
	bool is_jumbo = mlx5dr_matcher_mt_is_jumbo(mt);

	if (unlikely(mlx5dr_matcher_req_fw_wqe(rule->matcher))) {
		uint8_t *src_tag;

		/* Save match definer id and tag for delete */
		rule->tag_ptr = simple_calloc(2, sizeof(*rule->tag_ptr));
		assert(rule->tag_ptr);

		src_tag = (uint8_t *)ste_attr->wqe_data->tag;
		memcpy(rule->tag_ptr[0].match, src_tag, MLX5DR_MATCH_TAG_SZ);
		rule->tag_ptr[1].reserved[0] = ste_attr->send_attr.match_definer_id;

		/* Save range definer id and tag for delete */
		if (ste_attr->range_wqe_data) {
			src_tag = (uint8_t *)ste_attr->range_wqe_data->tag;
			memcpy(rule->tag_ptr[1].match, src_tag, MLX5DR_MATCH_TAG_SZ);
			rule->tag_ptr[1].reserved[1] = ste_attr->send_attr.range_definer_id;
		}
		return;
	}

	if (is_jumbo)
		memcpy(rule->tag.jumbo, ste_attr->wqe_data->jumbo, MLX5DR_JUMBO_TAG_SZ);
	else
		memcpy(rule->tag.match, ste_attr->wqe_data->tag, MLX5DR_MATCH_TAG_SZ);
}

static void
mlx5dr_rule_clear_delete_info(struct mlx5dr_rule *rule)
{
	if (unlikely(mlx5dr_matcher_req_fw_wqe(rule->matcher)))
		simple_free(rule->tag_ptr);
}

static void
mlx5dr_rule_load_delete_info(struct mlx5dr_rule *rule,
			     struct mlx5dr_send_ste_attr *ste_attr)
{
	if (unlikely(mlx5dr_matcher_req_fw_wqe(rule->matcher))) {
		/* Load match definer id and tag for delete */
		ste_attr->wqe_tag = &rule->tag_ptr[0];
		ste_attr->send_attr.match_definer_id = rule->tag_ptr[1].reserved[0];

		/* Load range definer id and tag for delete */
		if (rule->matcher->flags & MLX5DR_MATCHER_FLAGS_RANGE_DEFINER) {
			ste_attr->range_wqe_tag = &rule->tag_ptr[1];
			ste_attr->send_attr.range_definer_id = rule->tag_ptr[1].reserved[1];
		}
	} else {
		ste_attr->wqe_tag = &rule->tag;
	}
}

static int mlx5dr_rule_alloc_action_ste(struct mlx5dr_rule *rule,
					struct mlx5dr_rule_attr *attr)
{
	struct mlx5dr_matcher *matcher = rule->matcher;
	int ret;

	/* Use rule_idx for locking optimzation, otherwise allocate from pool */
	if (matcher->attr.optimize_using_rule_idx ||
	    mlx5dr_matcher_is_insert_by_idx(matcher)) {
		rule->action_ste_idx = attr->rule_idx * matcher->action_ste.max_stes;
	} else {
		struct mlx5dr_pool_chunk ste = {0};

		ste.order = rte_log2_u32(matcher->action_ste.max_stes);
		ret = mlx5dr_pool_chunk_alloc(matcher->action_ste.pool, &ste);
		if (ret) {
			DR_LOG(ERR, "Failed to allocate STE for rule actions");
			return ret;
		}
		rule->action_ste_idx = ste.offset;
	}
	return 0;
}

void mlx5dr_rule_free_action_ste_idx(struct mlx5dr_rule *rule)
{
	struct mlx5dr_matcher *matcher = rule->matcher;

	if (rule->action_ste_idx > -1 &&
	    !matcher->attr.optimize_using_rule_idx &&
	    !mlx5dr_matcher_is_insert_by_idx(matcher)) {
		struct mlx5dr_pool_chunk ste = {0};

		/* This release is safe only when the rule match part was deleted */
		ste.order = rte_log2_u32(matcher->action_ste.max_stes);
		ste.offset = rule->action_ste_idx;
		mlx5dr_pool_chunk_free(matcher->action_ste.pool, &ste);
	}
}

static void mlx5dr_rule_create_init(struct mlx5dr_rule *rule,
				    struct mlx5dr_send_ste_attr *ste_attr,
				    struct mlx5dr_actions_apply_data *apply,
				    bool is_update)
{
	struct mlx5dr_matcher *matcher = rule->matcher;
	struct mlx5dr_table *tbl = matcher->tbl;
	struct mlx5dr_context *ctx = tbl->ctx;

	/* Init rule before reuse */
	if (!is_update) {
		/* In update we use these rtc's */
		rule->rtc_0 = 0;
		rule->rtc_1 = 0;
	}

	rule->pending_wqes = 0;
	rule->action_ste_idx = -1;
	rule->status = MLX5DR_RULE_STATUS_CREATING;

	/* Init default send STE attributes */
	ste_attr->gta_opcode = MLX5DR_WQE_GTA_OP_ACTIVATE;
	ste_attr->send_attr.opmod = MLX5DR_WQE_GTA_OPMOD_STE;
	ste_attr->send_attr.opcode = MLX5DR_WQE_OPCODE_TBL_ACCESS;
	ste_attr->send_attr.len = MLX5DR_WQE_SZ_GTA_CTRL + MLX5DR_WQE_SZ_GTA_DATA;

	/* Init default action apply */
	apply->tbl_type = tbl->type;
	apply->common_res = &ctx->common_res[tbl->type];
	apply->jump_to_action_stc = matcher->action_ste.stc.offset;
	apply->require_dep = 0;
}

static int mlx5dr_rule_create_hws_fw_wqe(struct mlx5dr_rule *rule,
					 struct mlx5dr_rule_attr *attr,
					 uint8_t mt_idx,
					 const struct rte_flow_item items[],
					 uint8_t at_idx,
					 struct mlx5dr_rule_action rule_actions[])
{
	struct mlx5dr_action_template *at = &rule->matcher->at[at_idx];
	struct mlx5dr_match_template *mt = &rule->matcher->mt[mt_idx];
	struct mlx5dr_send_ring_dep_wqe range_wqe = {{0}};
	struct mlx5dr_send_ring_dep_wqe match_wqe = {{0}};
	bool is_range = mlx5dr_matcher_mt_is_range(mt);
	bool is_jumbo = mlx5dr_matcher_mt_is_jumbo(mt);
	struct mlx5dr_matcher *matcher = rule->matcher;
	struct mlx5dr_context *ctx = matcher->tbl->ctx;
	struct mlx5dr_send_ste_attr ste_attr = {0};
	struct mlx5dr_actions_apply_data apply;
	struct mlx5dr_send_engine *queue;

	queue = &ctx->send_queue[attr->queue_id];
	if (unlikely(mlx5dr_send_engine_err(queue))) {
		rte_errno = EIO;
		return rte_errno;
	}

	mlx5dr_rule_create_init(rule, &ste_attr, &apply, false);
	mlx5dr_rule_init_dep_wqe(&match_wqe, rule, items, mt, attr->user_data);
	mlx5dr_rule_init_dep_wqe(&range_wqe, rule, items, mt, attr->user_data);

	ste_attr.direct_index = 0;
	ste_attr.rtc_0 = match_wqe.rtc_0;
	ste_attr.rtc_1 = match_wqe.rtc_1;
	ste_attr.used_id_rtc_0 = &rule->rtc_0;
	ste_attr.used_id_rtc_1 = &rule->rtc_1;
	ste_attr.retry_rtc_0 = match_wqe.retry_rtc_0;
	ste_attr.retry_rtc_1 = match_wqe.retry_rtc_1;
	ste_attr.send_attr.rule = match_wqe.rule;
	ste_attr.send_attr.user_data = match_wqe.user_data;

	ste_attr.send_attr.fence = 1;
	ste_attr.send_attr.notify_hw = 1;
	ste_attr.wqe_tag_is_jumbo = is_jumbo;

	/* Prepare match STE TAG */
	ste_attr.wqe_ctrl = &match_wqe.wqe_ctrl;
	ste_attr.wqe_data = &match_wqe.wqe_data;
	ste_attr.send_attr.match_definer_id = mlx5dr_definer_get_id(mt->definer);

	mlx5dr_definer_create_tag(items,
				  mt->fc,
				  mt->fc_sz,
				  (uint8_t *)match_wqe.wqe_data.action);

	/* Prepare range STE TAG */
	if (is_range) {
		ste_attr.range_wqe_data = &range_wqe.wqe_data;
		ste_attr.send_attr.len += MLX5DR_WQE_SZ_GTA_DATA;
		ste_attr.send_attr.range_definer_id = mlx5dr_definer_get_id(mt->range_definer);

		mlx5dr_definer_create_tag_range(items,
						mt->fcr,
						mt->fcr_sz,
						(uint8_t *)range_wqe.wqe_data.action);
	}

	/* Apply the actions on the last STE */
	apply.queue = queue;
	apply.next_direct_idx = 0;
	apply.rule_action = rule_actions;
	apply.wqe_ctrl = &match_wqe.wqe_ctrl;
	apply.wqe_data = (uint32_t *)(is_range ?
				      &range_wqe.wqe_data :
				      &match_wqe.wqe_data);

	/* Skip setters[0] used for jumbo STE since not support with FW WQE */
	mlx5dr_action_apply_setter(&apply, &at->setters[1], 0);

	/* Send WQEs to FW */
	mlx5dr_send_stes_fw(queue, &ste_attr);

	/* Backup TAG on the rule for deletion */
	mlx5dr_rule_save_delete_info(rule, &ste_attr);
	mlx5dr_send_engine_inc_rule(queue);

	/* Send dependent WQEs */
	if (!attr->burst)
		mlx5dr_send_all_dep_wqe(queue);

	return 0;
}

static int mlx5dr_rule_create_hws(struct mlx5dr_rule *rule,
				  struct mlx5dr_rule_attr *attr,
				  uint8_t mt_idx,
				  const struct rte_flow_item items[],
				  uint8_t at_idx,
				  struct mlx5dr_rule_action rule_actions[])
{
	struct mlx5dr_action_template *at = &rule->matcher->at[at_idx];
	struct mlx5dr_match_template *mt = &rule->matcher->mt[mt_idx];
	bool is_jumbo = mlx5dr_matcher_mt_is_jumbo(mt);
	struct mlx5dr_matcher *matcher = rule->matcher;
	struct mlx5dr_context *ctx = matcher->tbl->ctx;
	struct mlx5dr_send_ste_attr ste_attr = {0};
	struct mlx5dr_send_ring_dep_wqe *dep_wqe;
	struct mlx5dr_actions_wqe_setter *setter;
	struct mlx5dr_actions_apply_data apply;
	struct mlx5dr_send_engine *queue;
	uint8_t total_stes, action_stes;
	bool is_update;
	int i, ret;

	is_update = (items == NULL);

	/* Insert rule using FW WQE if cannot use GTA WQE */
	if (unlikely(mlx5dr_matcher_req_fw_wqe(matcher) && !is_update))
		return mlx5dr_rule_create_hws_fw_wqe(rule, attr, mt_idx, items,
						     at_idx, rule_actions);

	queue = &ctx->send_queue[attr->queue_id];
	if (unlikely(mlx5dr_send_engine_err(queue))) {
		rte_errno = EIO;
		return rte_errno;
	}

	mlx5dr_rule_create_init(rule, &ste_attr, &apply, is_update);

	/* Allocate dependent match WQE since rule might have dependent writes.
	 * The queued dependent WQE can be later aborted or kept as a dependency.
	 * dep_wqe buffers (ctrl, data) are also reused for all STE writes.
	 */
	dep_wqe = mlx5dr_send_add_new_dep_wqe(queue);
	mlx5dr_rule_init_dep_wqe(dep_wqe, rule, items, mt, attr->user_data);

	ste_attr.wqe_ctrl = &dep_wqe->wqe_ctrl;
	ste_attr.wqe_data = &dep_wqe->wqe_data;
	apply.wqe_ctrl = &dep_wqe->wqe_ctrl;
	apply.wqe_data = (uint32_t *)&dep_wqe->wqe_data;
	apply.rule_action = rule_actions;
	apply.queue = queue;

	setter = &at->setters[at->num_of_action_stes];
	total_stes = at->num_of_action_stes + (is_jumbo && !at->only_term);
	action_stes = total_stes - 1;

	if (action_stes) {
		/* Allocate action STEs for complex rules */
		ret = mlx5dr_rule_alloc_action_ste(rule, attr);
		if (ret) {
			DR_LOG(ERR, "Failed to allocate action memory %d", ret);
			mlx5dr_send_abort_new_dep_wqe(queue);
			return ret;
		}
		/* Skip RX/TX based on the dep_wqe init */
		ste_attr.rtc_0 = dep_wqe->rtc_0 ? matcher->action_ste.rtc_0->id : 0;
		ste_attr.rtc_1 = dep_wqe->rtc_1 ? matcher->action_ste.rtc_1->id : 0;
		/* Action STEs are written to a specific index last to first */
		ste_attr.direct_index = rule->action_ste_idx + action_stes;
		apply.next_direct_idx = ste_attr.direct_index;
	} else {
		apply.next_direct_idx = 0;
	}

	for (i = total_stes; i-- > 0;) {
		mlx5dr_action_apply_setter(&apply, setter--, !i && is_jumbo);

		if (i == 0) {
			/* Handle last match STE.
			 * For hash split / linear lookup RTCs, packets reaching any STE
			 * will always match and perform the specified actions, which
			 * makes the tag irrelevant.
			 */
			if (likely(!mlx5dr_matcher_is_insert_by_idx(matcher) && !is_update))
				mlx5dr_definer_create_tag(items, mt->fc, mt->fc_sz,
							  (uint8_t *)dep_wqe->wqe_data.action);
			else if (unlikely(is_update))
				mlx5dr_rule_update_copy_tag(rule, &dep_wqe->wqe_data, is_jumbo);

			/* Rule has dependent WQEs, match dep_wqe is queued */
			if (action_stes || apply.require_dep)
				break;

			/* Rule has no dependencies, abort dep_wqe and send WQE now */
			mlx5dr_send_abort_new_dep_wqe(queue);
			ste_attr.wqe_tag_is_jumbo = is_jumbo;
			ste_attr.send_attr.notify_hw = !attr->burst;
			ste_attr.send_attr.user_data = dep_wqe->user_data;
			ste_attr.send_attr.rule = dep_wqe->rule;
			ste_attr.rtc_0 = dep_wqe->rtc_0;
			ste_attr.rtc_1 = dep_wqe->rtc_1;
			ste_attr.used_id_rtc_0 = &rule->rtc_0;
			ste_attr.used_id_rtc_1 = &rule->rtc_1;
			ste_attr.retry_rtc_0 = dep_wqe->retry_rtc_0;
			ste_attr.retry_rtc_1 = dep_wqe->retry_rtc_1;
			ste_attr.direct_index = mlx5dr_matcher_is_insert_by_idx(matcher) ?
						attr->rule_idx : 0;
		} else {
			apply.next_direct_idx = --ste_attr.direct_index;
		}

		mlx5dr_send_ste(queue, &ste_attr);
	}

	/* Backup TAG on the rule for deletion, only after insertion */
	if (!is_update)
		mlx5dr_rule_save_delete_info(rule, &ste_attr);

	mlx5dr_send_engine_inc_rule(queue);

	/* Send dependent WQEs */
	if (!attr->burst)
		mlx5dr_send_all_dep_wqe(queue);

	return 0;
}

static void mlx5dr_rule_destroy_failed_hws(struct mlx5dr_rule *rule,
					   struct mlx5dr_rule_attr *attr)
{
	struct mlx5dr_context *ctx = rule->matcher->tbl->ctx;
	struct mlx5dr_send_engine *queue;

	queue = &ctx->send_queue[attr->queue_id];

	mlx5dr_rule_gen_comp(queue, rule, false,
			     attr->user_data, MLX5DR_RULE_STATUS_DELETED);

	/* Rule failed now we can safely release action STEs */
	mlx5dr_rule_free_action_ste_idx(rule);

	/* Clear complex tag */
	mlx5dr_rule_clear_delete_info(rule);

	/* If a rule that was indicated as burst (need to trigger HW) has failed
	 * insertion we won't ring the HW as nothing is being written to the WQ.
	 * In such case update the last WQE and ring the HW with that work
	 */
	if (attr->burst)
		return;

	mlx5dr_send_all_dep_wqe(queue);
	mlx5dr_send_engine_flush_queue(queue);
}

static int mlx5dr_rule_destroy_hws(struct mlx5dr_rule *rule,
				   struct mlx5dr_rule_attr *attr)
{
	struct mlx5dr_context *ctx = rule->matcher->tbl->ctx;
	struct mlx5dr_matcher *matcher = rule->matcher;
	bool fw_wqe = mlx5dr_matcher_req_fw_wqe(matcher);
	bool is_range = mlx5dr_matcher_mt_is_range(matcher->mt);
	bool is_jumbo = mlx5dr_matcher_mt_is_jumbo(matcher->mt);
	struct mlx5dr_wqe_gta_ctrl_seg wqe_ctrl = {0};
	struct mlx5dr_send_ste_attr ste_attr = {0};
	struct mlx5dr_send_engine *queue;

	queue = &ctx->send_queue[attr->queue_id];

	if (unlikely(mlx5dr_send_engine_err(queue))) {
		mlx5dr_rule_destroy_failed_hws(rule, attr);
		return 0;
	}

	/* Rule is not completed yet */
	if (rule->status == MLX5DR_RULE_STATUS_CREATING) {
		rte_errno = EBUSY;
		return rte_errno;
	}

	/* Rule failed and doesn't require cleanup */
	if (rule->status == MLX5DR_RULE_STATUS_FAILED) {
		mlx5dr_rule_destroy_failed_hws(rule, attr);
		return 0;
	}

	mlx5dr_send_engine_inc_rule(queue);

	/* Send dependent WQE */
	if (!attr->burst)
		mlx5dr_send_all_dep_wqe(queue);

	rule->status = MLX5DR_RULE_STATUS_DELETING;

	ste_attr.send_attr.opmod = MLX5DR_WQE_GTA_OPMOD_STE;
	ste_attr.send_attr.opcode = MLX5DR_WQE_OPCODE_TBL_ACCESS;
	ste_attr.send_attr.len = MLX5DR_WQE_SZ_GTA_CTRL + MLX5DR_WQE_SZ_GTA_DATA;
	if (unlikely(is_range))
		ste_attr.send_attr.len += MLX5DR_WQE_SZ_GTA_DATA;

	ste_attr.send_attr.rule = rule;
	ste_attr.send_attr.notify_hw = !attr->burst;
	ste_attr.send_attr.user_data = attr->user_data;

	ste_attr.rtc_0 = rule->rtc_0;
	ste_attr.rtc_1 = rule->rtc_1;
	ste_attr.used_id_rtc_0 = &rule->rtc_0;
	ste_attr.used_id_rtc_1 = &rule->rtc_1;
	ste_attr.wqe_ctrl = &wqe_ctrl;
	ste_attr.wqe_tag_is_jumbo = is_jumbo;
	ste_attr.gta_opcode = MLX5DR_WQE_GTA_OP_DEACTIVATE;
	if (unlikely(mlx5dr_matcher_is_insert_by_idx(matcher)))
		ste_attr.direct_index = attr->rule_idx;

	mlx5dr_rule_load_delete_info(rule, &ste_attr);

	if (unlikely(fw_wqe)) {
		mlx5dr_send_stes_fw(queue, &ste_attr);
		mlx5dr_rule_clear_delete_info(rule);
	} else {
		mlx5dr_send_ste(queue, &ste_attr);
	}

	return 0;
}

static int mlx5dr_rule_create_root(struct mlx5dr_rule *rule,
				   struct mlx5dr_rule_attr *rule_attr,
				   const struct rte_flow_item items[],
				   uint8_t at_idx,
				   struct mlx5dr_rule_action rule_actions[])
{
	struct mlx5dv_flow_matcher *dv_matcher = rule->matcher->dv_matcher;
	uint8_t num_actions = rule->matcher->at[at_idx].num_actions;
	struct mlx5dr_context *ctx = rule->matcher->tbl->ctx;
	struct mlx5dv_flow_match_parameters *value;
	struct mlx5_flow_attr flow_attr = {0};
	struct mlx5dv_flow_action_attr *attr;
	struct rte_flow_error error;
	uint8_t match_criteria;
	int ret;

	attr = simple_calloc(num_actions, sizeof(*attr));
	if (!attr) {
		rte_errno = ENOMEM;
		return rte_errno;
	}

	value = simple_calloc(1, MLX5_ST_SZ_BYTES(fte_match_param) +
			      offsetof(struct mlx5dv_flow_match_parameters, match_buf));
	if (!value) {
		rte_errno = ENOMEM;
		goto free_attr;
	}

	flow_attr.tbl_type = rule->matcher->tbl->type;

	ret = flow_dv_translate_items_hws(items, &flow_attr, value->match_buf,
					  MLX5_SET_MATCHER_HS_V, NULL,
					  &match_criteria,
					  &error);
	if (ret) {
		DR_LOG(ERR, "Failed to convert items to PRM [%s]", error.message);
		goto free_value;
	}

	/* Convert actions to verb action attr */
	ret = mlx5dr_action_root_build_attr(rule_actions, num_actions, attr);
	if (ret)
		goto free_value;

	/* Create verb flow */
	value->match_sz = MLX5_ST_SZ_BYTES(fte_match_param);
	rule->flow = mlx5_glue->dv_create_flow_root(dv_matcher,
						    value,
						    num_actions,
						    attr);

	mlx5dr_rule_gen_comp(&ctx->send_queue[rule_attr->queue_id], rule, !rule->flow,
			     rule_attr->user_data, MLX5DR_RULE_STATUS_CREATED);

	simple_free(value);
	simple_free(attr);

	return 0;

free_value:
	simple_free(value);
free_attr:
	simple_free(attr);

	return -rte_errno;
}

static int mlx5dr_rule_destroy_root(struct mlx5dr_rule *rule,
				    struct mlx5dr_rule_attr *attr)
{
	struct mlx5dr_context *ctx = rule->matcher->tbl->ctx;
	int err = 0;

	if (rule->flow)
		err = ibv_destroy_flow(rule->flow);

	mlx5dr_rule_gen_comp(&ctx->send_queue[attr->queue_id], rule, err,
			     attr->user_data, MLX5DR_RULE_STATUS_DELETED);

	return 0;
}

static int mlx5dr_rule_enqueue_precheck(struct mlx5dr_context *ctx,
					struct mlx5dr_rule_attr *attr)
{
	if (unlikely(!attr->user_data)) {
		rte_errno = EINVAL;
		return rte_errno;
	}

	/* Check if there is room in queue */
	if (unlikely(mlx5dr_send_engine_full(&ctx->send_queue[attr->queue_id]))) {
		rte_errno = EBUSY;
		return rte_errno;
	}

	return 0;
}

int mlx5dr_rule_create(struct mlx5dr_matcher *matcher,
		       uint8_t mt_idx,
		       const struct rte_flow_item items[],
		       uint8_t at_idx,
		       struct mlx5dr_rule_action rule_actions[],
		       struct mlx5dr_rule_attr *attr,
		       struct mlx5dr_rule *rule_handle)
{
	struct mlx5dr_context *ctx;
	int ret;

	rule_handle->matcher = matcher;
	ctx = matcher->tbl->ctx;

	if (mlx5dr_rule_enqueue_precheck(ctx, attr))
		return -rte_errno;

	assert(matcher->num_of_mt >= mt_idx);
	assert(matcher->num_of_at >= at_idx);
	assert(items);

	if (unlikely(mlx5dr_table_is_root(matcher->tbl)))
		ret = mlx5dr_rule_create_root(rule_handle,
					      attr,
					      items,
					      at_idx,
					      rule_actions);
	else
		ret = mlx5dr_rule_create_hws(rule_handle,
					     attr,
					     mt_idx,
					     items,
					     at_idx,
					     rule_actions);
	return -ret;
}

int mlx5dr_rule_destroy(struct mlx5dr_rule *rule,
			struct mlx5dr_rule_attr *attr)
{
	int ret;

	if (mlx5dr_rule_enqueue_precheck(rule->matcher->tbl->ctx, attr))
		return -rte_errno;

	if (unlikely(mlx5dr_table_is_root(rule->matcher->tbl)))
		ret = mlx5dr_rule_destroy_root(rule, attr);
	else
		ret = mlx5dr_rule_destroy_hws(rule, attr);

	return -ret;
}

int mlx5dr_rule_action_update(struct mlx5dr_rule *rule_handle,
			      uint8_t at_idx,
			      struct mlx5dr_rule_action rule_actions[],
			      struct mlx5dr_rule_attr *attr)
{
	struct mlx5dr_matcher *matcher = rule_handle->matcher;
	int ret;

	if (unlikely(mlx5dr_table_is_root(matcher->tbl) ||
	    unlikely(mlx5dr_matcher_req_fw_wqe(matcher)))) {
		DR_LOG(ERR, "Rule update not supported on current matcher");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	if (!matcher->attr.optimize_using_rule_idx &&
	    !mlx5dr_matcher_is_insert_by_idx(matcher)) {
		DR_LOG(ERR, "Rule update requires optimize by idx matcher");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	if (mlx5dr_rule_enqueue_precheck(matcher->tbl->ctx, attr))
		return -rte_errno;

	ret = mlx5dr_rule_create_hws(rule_handle,
				     attr,
				     0,
				     NULL,
				     at_idx,
				     rule_actions);

	return -ret;
}

size_t mlx5dr_rule_get_handle_size(void)
{
	return sizeof(struct mlx5dr_rule);
}

int mlx5dr_rule_hash_calculate(struct mlx5dr_matcher *matcher,
			       const struct rte_flow_item items[],
			       uint8_t mt_idx,
			       enum mlx5dr_rule_hash_calc_mode mode,
			       uint32_t *ret_hash)
{
	uint8_t tag[MLX5DR_STE_SZ] = {0};
	struct mlx5dr_match_template *mt;

	if (!matcher || !matcher->mt) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	mt = &matcher->mt[mt_idx];

	if (mlx5dr_matcher_req_fw_wqe(matcher) ||
	    mlx5dr_table_is_root(matcher->tbl) ||
	    matcher->tbl->ctx->caps->access_index_mode == MLX5DR_MATCHER_INSERT_BY_HASH ||
	    matcher->tbl->ctx->caps->flow_table_hash_type != MLX5_FLOW_TABLE_HASH_TYPE_CRC32) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	mlx5dr_definer_create_tag(items, mt->fc, mt->fc_sz, tag);
	if (mlx5dr_matcher_mt_is_jumbo(mt))
		*ret_hash = mlx5dr_crc32_calc(tag, MLX5DR_JUMBO_TAG_SZ);
	else
		*ret_hash = mlx5dr_crc32_calc(tag + MLX5DR_ACTIONS_SZ,
					      MLX5DR_MATCH_TAG_SZ);

	if (mode == MLX5DR_RULE_HASH_CALC_MODE_IDX)
		*ret_hash = *ret_hash & (BIT(matcher->attr.rule.num_log) - 1);

	return 0;
}
