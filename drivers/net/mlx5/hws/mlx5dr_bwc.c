/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#include "mlx5dr_internal.h"

static uint16_t mlx5dr_bwc_queues(struct mlx5dr_context *ctx)
{
	return (ctx->queues - 1) / 2;
}

static uint16_t
mlx5dr_bwc_gen_queue_idx(struct mlx5dr_context *ctx)
{
	/* assign random queue */
	return rand() % mlx5dr_bwc_queues(ctx);
}

static uint16_t
mlx5dr_bwc_get_queue_id(struct mlx5dr_context *ctx, uint16_t idx)
{
	return idx + mlx5dr_bwc_queues(ctx);
}

static uint16_t
mlx5dr_bwc_get_burst_th(struct mlx5dr_context *ctx, uint16_t queue_id)
{
	return RTE_MIN(ctx->send_queue[queue_id].num_entries / 2,
		       MLX5DR_BWC_MATCHER_REHASH_BURST_TH);
}

static rte_spinlock_t *
mlx5dr_bwc_get_queue_lock(struct mlx5dr_context *ctx, uint16_t idx)
{
	return &ctx->bwc_send_queue_locks[idx];
}

static void mlx5dr_bwc_lock_all_queues(struct mlx5dr_context *ctx)
{
	uint16_t bwc_queues = mlx5dr_bwc_queues(ctx);
	rte_spinlock_t *queue_lock;
	int i;

	for (i = 0; i < bwc_queues; i++) {
		queue_lock = mlx5dr_bwc_get_queue_lock(ctx, i);
		rte_spinlock_lock(queue_lock);
	}
}

static void mlx5dr_bwc_unlock_all_queues(struct mlx5dr_context *ctx)
{
	uint16_t bwc_queues = mlx5dr_bwc_queues(ctx);
	rte_spinlock_t *queue_lock;
	int i;

	for (i = 0; i < bwc_queues; i++) {
		queue_lock = mlx5dr_bwc_get_queue_lock(ctx, i);
		rte_spinlock_unlock(queue_lock);
	}
}

static void mlx5dr_bwc_matcher_init_attr(struct mlx5dr_matcher_attr *attr,
					 uint32_t priority,
					 uint8_t size_log,
					 bool is_root)
{
	memset(attr, 0, sizeof(*attr));

	attr->priority = priority;
	attr->optimize_using_rule_idx = 0;
	attr->mode = MLX5DR_MATCHER_RESOURCE_MODE_RULE;
	attr->optimize_flow_src = MLX5DR_MATCHER_FLOW_SRC_ANY;
	attr->insert_mode = MLX5DR_MATCHER_INSERT_BY_HASH;
	attr->distribute_mode = MLX5DR_MATCHER_DISTRIBUTE_BY_HASH;
	attr->rule.num_log = size_log;

	if (!is_root) {
		attr->resizable = true;
		attr->max_num_of_at_attach = MLX5DR_BWC_MATCHER_ATTACH_AT_NUM;
	}
}

struct mlx5dr_bwc_matcher *
mlx5dr_bwc_matcher_create(struct mlx5dr_table *table,
			  uint32_t priority,
			  const struct rte_flow_item flow_items[])
{
	enum mlx5dr_action_type init_action_types[1] = { MLX5DR_ACTION_TYP_LAST };
	uint16_t bwc_queues = mlx5dr_bwc_queues(table->ctx);
	struct mlx5dr_bwc_matcher *bwc_matcher;
	struct mlx5dr_matcher_attr attr = {0};
	int i;

	if (!mlx5dr_context_bwc_supported(table->ctx)) {
		rte_errno = EINVAL;
		DR_LOG(ERR, "BWC rule: Context created w/o BWC API compatibility");
		return NULL;
	}

	bwc_matcher = simple_calloc(1, sizeof(*bwc_matcher));
	if (!bwc_matcher) {
		rte_errno = ENOMEM;
		return NULL;
	}

	bwc_matcher->rules = simple_calloc(bwc_queues, sizeof(*bwc_matcher->rules));
	if (!bwc_matcher->rules) {
		rte_errno = ENOMEM;
		goto free_bwc_matcher;
	}

	for (i = 0; i < bwc_queues; i++)
		LIST_INIT(&bwc_matcher->rules[i]);

	mlx5dr_bwc_matcher_init_attr(&attr,
				     priority,
				     MLX5DR_BWC_MATCHER_INIT_SIZE_LOG,
				     mlx5dr_table_is_root(table));

	bwc_matcher->mt = mlx5dr_match_template_create(flow_items,
						       MLX5DR_MATCH_TEMPLATE_FLAG_NONE);
	if (!bwc_matcher->mt) {
		rte_errno = EINVAL;
		goto free_bwc_matcher_rules;
	}

	bwc_matcher->priority = priority;
	bwc_matcher->size_log = MLX5DR_BWC_MATCHER_INIT_SIZE_LOG;

	/* create dummy action template */
	bwc_matcher->at[0] = mlx5dr_action_template_create(init_action_types, 0);
	bwc_matcher->num_of_at = 1;

	bwc_matcher->matcher = mlx5dr_matcher_create(table,
						     &bwc_matcher->mt, 1,
						     &bwc_matcher->at[0],
						     bwc_matcher->num_of_at,
						     &attr);
	if (!bwc_matcher->matcher) {
		/* rte_errno must be set */
		goto free_at;
	}

	return bwc_matcher;

free_at:
	mlx5dr_action_template_destroy(bwc_matcher->at[0]);
	mlx5dr_match_template_destroy(bwc_matcher->mt);
free_bwc_matcher_rules:
	simple_free(bwc_matcher->rules);
free_bwc_matcher:
	simple_free(bwc_matcher);

	return NULL;
}

int mlx5dr_bwc_matcher_destroy(struct mlx5dr_bwc_matcher *bwc_matcher)
{
	int i;

	if (bwc_matcher->num_of_rules)
		DR_LOG(ERR, "BWC matcher destroy: matcher still has %d rules",
		       bwc_matcher->num_of_rules);

	mlx5dr_matcher_destroy(bwc_matcher->matcher);
	bwc_matcher->matcher = NULL;

	for (i = 0; i < bwc_matcher->num_of_at; i++)
		mlx5dr_action_template_destroy(bwc_matcher->at[i]);

	mlx5dr_match_template_destroy(bwc_matcher->mt);
	simple_free(bwc_matcher->rules);
	simple_free(bwc_matcher);

	return 0;
}

static int
mlx5dr_bwc_queue_poll(struct mlx5dr_context *ctx,
		      uint16_t queue_id,
		      uint32_t *pending_rules,
		      bool drain)
{
	struct rte_flow_op_result comp[MLX5DR_BWC_MATCHER_REHASH_BURST_TH];
	uint16_t burst_th = mlx5dr_bwc_get_burst_th(ctx, queue_id);
	bool got_comp = *pending_rules >= burst_th;
	bool queue_full;
	int err = 0;
	int ret;
	int i;

	/* Check if there are any completions at all */
	if (!got_comp && !drain)
		return 0;

	/* The FULL state of a SQ is always a subcondition of the original 'got_comp'. */
	queue_full = mlx5dr_send_engine_full(&ctx->send_queue[queue_id]);
	while (queue_full || ((got_comp || drain) && *pending_rules)) {
		ret = mlx5dr_send_queue_poll(ctx, queue_id, comp, burst_th);
		if (unlikely(ret < 0)) {
			DR_LOG(ERR, "Rehash error: polling queue %d returned %d\n",
			       queue_id, ret);
			return -EINVAL;
		}

		if (ret) {
			(*pending_rules) -= ret;
			for (i = 0; i < ret; i++) {
				if (unlikely(comp[i].status != RTE_FLOW_OP_SUCCESS)) {
					DR_LOG(ERR,
					       "Rehash error: polling queue %d returned completion with error\n",
					       queue_id);
					err = -EINVAL;
				}
			}
			queue_full = false;
		}

		got_comp = !!ret;
	}

	return err;
}

static void
mlx5dr_bwc_rule_fill_attr(struct mlx5dr_bwc_matcher *bwc_matcher,
			  uint16_t bwc_queue_idx,
			  struct mlx5dr_rule_attr *rule_attr)
{
	struct mlx5dr_context *ctx = bwc_matcher->matcher->tbl->ctx;

	/* no use of INSERT_BY_INDEX in bwc rule */
	rule_attr->rule_idx = 0;

	/* notify HW at each rule insertion/deletion */
	rule_attr->burst = 0;

	/* We don't need user data, but the API requires it to exist */
	rule_attr->user_data = (void *)0xFACADE;

	rule_attr->queue_id = mlx5dr_bwc_get_queue_id(ctx, bwc_queue_idx);
}

static struct mlx5dr_bwc_rule *
mlx5dr_bwc_rule_alloc(void)
{
	struct mlx5dr_bwc_rule *bwc_rule;

	bwc_rule = simple_calloc(1, sizeof(*bwc_rule));
	if (unlikely(!bwc_rule))
		goto out_err;

	bwc_rule->rule = simple_calloc(1, sizeof(*bwc_rule->rule));
	if (unlikely(!bwc_rule->rule))
		goto free_rule;

	return bwc_rule;

free_rule:
	simple_free(bwc_rule);
out_err:
	rte_errno = ENOMEM;
	return NULL;
}

static void
mlx5dr_bwc_rule_free(struct mlx5dr_bwc_rule *bwc_rule)
{
	if (likely(bwc_rule->rule))
		simple_free(bwc_rule->rule);
	simple_free(bwc_rule);
}

static void
mlx5dr_bwc_rule_list_add(struct mlx5dr_bwc_rule *bwc_rule, uint16_t idx)
{
	struct mlx5dr_bwc_matcher *bwc_matcher = bwc_rule->bwc_matcher;

	rte_atomic_fetch_add_explicit(&bwc_matcher->num_of_rules, 1, rte_memory_order_relaxed);
	bwc_rule->bwc_queue_idx = idx;
	LIST_INSERT_HEAD(&bwc_matcher->rules[idx], bwc_rule, next);
}

static void mlx5dr_bwc_rule_list_remove(struct mlx5dr_bwc_rule *bwc_rule)
{
	struct mlx5dr_bwc_matcher *bwc_matcher = bwc_rule->bwc_matcher;

	rte_atomic_fetch_sub_explicit(&bwc_matcher->num_of_rules, 1, rte_memory_order_relaxed);
	LIST_REMOVE(bwc_rule, next);
}

static int
mlx5dr_bwc_rule_destroy_hws_async(struct mlx5dr_bwc_rule *bwc_rule,
				  struct mlx5dr_rule_attr *attr)
{
	return mlx5dr_rule_destroy(bwc_rule->rule, attr);
}

static int
mlx5dr_bwc_rule_destroy_hws_sync(struct mlx5dr_bwc_rule *bwc_rule,
				 struct mlx5dr_rule_attr *rule_attr)
{
	struct mlx5dr_context *ctx = bwc_rule->bwc_matcher->matcher->tbl->ctx;
	struct rte_flow_op_result completion;
	int ret;

	ret = mlx5dr_bwc_rule_destroy_hws_async(bwc_rule, rule_attr);
	if (unlikely(ret))
		return ret;

	do {
		ret = mlx5dr_send_queue_poll(ctx, rule_attr->queue_id, &completion, 1);
	} while (ret != 1);

	if (unlikely(completion.status != RTE_FLOW_OP_SUCCESS ||
		     (bwc_rule->rule->status != MLX5DR_RULE_STATUS_DELETED &&
		      bwc_rule->rule->status != MLX5DR_RULE_STATUS_DELETING))) {
		DR_LOG(ERR, "Failed destroying BWC rule: completion %d, rule status %d",
		       completion.status, bwc_rule->rule->status);
		rte_errno = EINVAL;
		return rte_errno;
	}

	return 0;
}

static int mlx5dr_bwc_rule_destroy_hws(struct mlx5dr_bwc_rule *bwc_rule)
{
	struct mlx5dr_bwc_matcher *bwc_matcher = bwc_rule->bwc_matcher;
	struct mlx5dr_context *ctx = bwc_matcher->matcher->tbl->ctx;
	uint16_t idx = bwc_rule->bwc_queue_idx;
	struct mlx5dr_rule_attr attr;
	rte_spinlock_t *queue_lock;
	int ret;

	mlx5dr_bwc_rule_fill_attr(bwc_matcher, idx, &attr);

	queue_lock = mlx5dr_bwc_get_queue_lock(ctx, idx);

	rte_spinlock_lock(queue_lock);

	ret = mlx5dr_bwc_rule_destroy_hws_sync(bwc_rule, &attr);
	mlx5dr_bwc_rule_list_remove(bwc_rule);

	rte_spinlock_unlock(queue_lock);

	mlx5dr_bwc_rule_free(bwc_rule);

	return ret;
}

static int mlx5dr_bwc_rule_destroy_root(struct mlx5dr_bwc_rule *bwc_rule)
{
	int ret;

	ret = mlx5dr_rule_destroy_root_no_comp(bwc_rule->rule);

	mlx5dr_bwc_rule_free(bwc_rule);

	return ret;
}

int mlx5dr_bwc_rule_destroy(struct mlx5dr_bwc_rule *bwc_rule)
{
	if (unlikely(mlx5dr_table_is_root(bwc_rule->bwc_matcher->matcher->tbl)))
		return mlx5dr_bwc_rule_destroy_root(bwc_rule);

	return mlx5dr_bwc_rule_destroy_hws(bwc_rule);
}

static struct mlx5dr_bwc_rule *
mlx5dr_bwc_rule_create_hws_async(struct mlx5dr_bwc_matcher *bwc_matcher,
				 const struct rte_flow_item flow_items[],
				 uint8_t at_idx,
				 struct mlx5dr_rule_action rule_actions[],
				 struct mlx5dr_rule_attr *rule_attr)
{
	struct mlx5dr_bwc_rule *bwc_rule;
	int ret;

	bwc_rule = mlx5dr_bwc_rule_alloc();
	if (unlikely(!bwc_rule))
		return NULL;

	bwc_rule->bwc_matcher = bwc_matcher;

	ret = mlx5dr_rule_create(bwc_matcher->matcher,
				 0, /* only one match template supported */
				 flow_items,
				 at_idx,
				 rule_actions,
				 rule_attr,
				 bwc_rule->rule);

	if (unlikely(ret)) {
		mlx5dr_bwc_rule_free(bwc_rule);
		rte_errno = EINVAL;
		return NULL;
	}

	return bwc_rule;
}

static struct mlx5dr_bwc_rule *
mlx5dr_bwc_rule_create_root_sync(struct mlx5dr_bwc_matcher *bwc_matcher,
				 const struct rte_flow_item flow_items[],
				 uint8_t num_actions,
				 struct mlx5dr_rule_action rule_actions[])
{
	struct mlx5dr_bwc_rule *bwc_rule;
	int ret;

	bwc_rule = mlx5dr_bwc_rule_alloc();
	if (unlikely(!bwc_rule)) {
		rte_errno = ENOMEM;
		return NULL;
	}

	bwc_rule->bwc_matcher = bwc_matcher;
	bwc_rule->rule->matcher = bwc_matcher->matcher;

	ret = mlx5dr_rule_create_root_no_comp(bwc_rule->rule,
					      flow_items,
					      num_actions,
					      rule_actions);
	if (unlikely(ret)) {
		mlx5dr_bwc_rule_free(bwc_rule);
		rte_errno = EINVAL;
		return NULL;
	}

	return bwc_rule;
}

static struct mlx5dr_bwc_rule *
mlx5dr_bwc_rule_create_hws_sync(struct mlx5dr_bwc_matcher *bwc_matcher,
				const struct rte_flow_item flow_items[],
				uint8_t at_idx,
				struct mlx5dr_rule_action rule_actions[],
				struct mlx5dr_rule_attr *rule_attr)

{
	struct mlx5dr_context *ctx = bwc_matcher->matcher->tbl->ctx;
	struct rte_flow_op_result completion;
	struct mlx5dr_bwc_rule *bwc_rule;
	int ret;

	bwc_rule = mlx5dr_bwc_rule_create_hws_async(bwc_matcher, flow_items,
						    at_idx, rule_actions,
						    rule_attr);
	if (unlikely(!bwc_rule))
		return NULL;

	do {
		ret = mlx5dr_send_queue_poll(ctx, rule_attr->queue_id, &completion, 1);
	} while (ret != 1);

	if (unlikely(completion.status != RTE_FLOW_OP_SUCCESS ||
		     (bwc_rule->rule->status != MLX5DR_RULE_STATUS_CREATED &&
		      bwc_rule->rule->status != MLX5DR_RULE_STATUS_CREATING))) {
		DR_LOG(ERR, "Failed creating BWC rule: completion %d, rule status %d",
		       completion.status, bwc_rule->rule->status);
		mlx5dr_bwc_rule_free(bwc_rule);
		return NULL;
	}

	return bwc_rule;
}

static bool
mlx5dr_bwc_matcher_size_maxed_out(struct mlx5dr_bwc_matcher *bwc_matcher)
{
	struct mlx5dr_cmd_query_caps *caps = bwc_matcher->matcher->tbl->ctx->caps;

	return bwc_matcher->size_log + MLX5DR_MATCHER_ASSURED_MAIN_TBL_DEPTH >=
	       caps->ste_alloc_log_max - 1;
}

static bool
mlx5dr_bwc_matcher_rehash_size_needed(struct mlx5dr_bwc_matcher *bwc_matcher,
				      uint32_t num_of_rules)
{
	/* size-based rehash for root table is kernel's responsibility */
	if (unlikely(mlx5dr_table_is_root(bwc_matcher->matcher->tbl)))
		return false;

	if (unlikely(mlx5dr_bwc_matcher_size_maxed_out(bwc_matcher)))
		return false;

	if (unlikely((num_of_rules * 100 / MLX5DR_BWC_MATCHER_REHASH_PERCENT_TH) >=
		     (1UL << bwc_matcher->size_log)))
		return true;

	return false;
}

static void
mlx5dr_bwc_rule_actions_to_action_types(struct mlx5dr_rule_action rule_actions[],
					enum mlx5dr_action_type action_types[])
{
	int i = 0;

	for (i = 0;
	     rule_actions[i].action && (rule_actions[i].action->type != MLX5DR_ACTION_TYP_LAST);
	     i++) {
		action_types[i] = (enum mlx5dr_action_type)rule_actions[i].action->type;
	}

	action_types[i] = MLX5DR_ACTION_TYP_LAST;
}

static int
mlx5dr_bwc_rule_actions_num(struct mlx5dr_rule_action rule_actions[])
{
	int i = 0;

	while (rule_actions[i].action &&
	       (rule_actions[i].action->type != MLX5DR_ACTION_TYP_LAST))
		i++;

	return i;
}

static int
mlx5dr_bwc_matcher_extend_at(struct mlx5dr_bwc_matcher *bwc_matcher,
			     struct mlx5dr_rule_action rule_actions[])
{
	enum mlx5dr_action_type action_types[MLX5_HW_MAX_ACTS];

	mlx5dr_bwc_rule_actions_to_action_types(rule_actions, action_types);

	bwc_matcher->at[bwc_matcher->num_of_at] =
		mlx5dr_action_template_create(action_types, 0);

	if (unlikely(!bwc_matcher->at[bwc_matcher->num_of_at])) {
		rte_errno = ENOMEM;
		return rte_errno;
	}

	bwc_matcher->num_of_at++;
	return 0;
}

static int
mlx5dr_bwc_matcher_extend_size(struct mlx5dr_bwc_matcher *bwc_matcher)
{
	struct mlx5dr_cmd_query_caps *caps = bwc_matcher->matcher->tbl->ctx->caps;

	if (unlikely(mlx5dr_bwc_matcher_size_maxed_out(bwc_matcher))) {
		DR_LOG(ERR, "Can't resize matcher: depth exceeds limit %d",
		       caps->rtc_log_depth_max);
		return -ENOMEM;
	}

	bwc_matcher->size_log =
		RTE_MIN(bwc_matcher->size_log + MLX5DR_BWC_MATCHER_SIZE_LOG_STEP,
			caps->ste_alloc_log_max - MLX5DR_MATCHER_ASSURED_MAIN_TBL_DEPTH);

	return 0;
}

static int
mlx5dr_bwc_matcher_find_at(struct mlx5dr_bwc_matcher *bwc_matcher,
			   struct mlx5dr_rule_action rule_actions[])
{
	enum mlx5dr_action_type *action_type_arr;
	int i, j;

	/* start from index 1 - first action template is a dummy */
	for (i = 1; i < bwc_matcher->num_of_at; i++) {
		j = 0;
		action_type_arr = bwc_matcher->at[i]->action_type_arr;

		while (rule_actions[j].action &&
		       rule_actions[j].action->type != MLX5DR_ACTION_TYP_LAST) {
			if (action_type_arr[j] != rule_actions[j].action->type)
				break;
			j++;
		}

		if (action_type_arr[j] == MLX5DR_ACTION_TYP_LAST &&
		    (!rule_actions[j].action ||
		     rule_actions[j].action->type == MLX5DR_ACTION_TYP_LAST))
			return i;
	}

	return -1;
}

static int
mlx5dr_bwc_matcher_move_all(struct mlx5dr_bwc_matcher *bwc_matcher)
{
	struct mlx5dr_context *ctx = bwc_matcher->matcher->tbl->ctx;
	uint16_t bwc_queues = mlx5dr_bwc_queues(ctx);
	struct mlx5dr_bwc_rule **bwc_rules;
	struct mlx5dr_rule_attr rule_attr;
	uint32_t *pending_rules;
	uint16_t burst_th;
	bool all_done;
	int i, j, ret;

	if (mlx5dr_table_is_root(bwc_matcher->matcher->tbl)) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	mlx5dr_bwc_rule_fill_attr(bwc_matcher, 0, &rule_attr);

	pending_rules = simple_calloc(bwc_queues, sizeof(*pending_rules));
	if (!pending_rules) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	bwc_rules = simple_calloc(bwc_queues, sizeof(*bwc_rules));
	if (!bwc_rules) {
		rte_errno = ENOMEM;
		goto free_pending_rules;
	}

	for (i = 0; i < bwc_queues; i++) {
		if (LIST_EMPTY(&bwc_matcher->rules[i]))
			bwc_rules[i] = NULL;
		else
			bwc_rules[i] = LIST_FIRST(&bwc_matcher->rules[i]);
	}

	do {
		all_done = true;

		for (i = 0; i < bwc_queues; i++) {
			rule_attr.queue_id = mlx5dr_bwc_get_queue_id(ctx, i);
			burst_th = mlx5dr_bwc_get_burst_th(ctx, rule_attr.queue_id);

			for (j = 0; j < burst_th && bwc_rules[i]; j++) {
				rule_attr.burst = !!((j + 1) % burst_th);
				ret = mlx5dr_matcher_resize_rule_move(bwc_matcher->matcher,
								      bwc_rules[i]->rule,
								      &rule_attr);
				if (unlikely(ret)) {
					DR_LOG(ERR, "Moving BWC rule failed during rehash - %d",
					       ret);
					rte_errno = ENOMEM;
					goto free_bwc_rules;
				}

				all_done = false;
				pending_rules[i]++;
				bwc_rules[i] = LIST_NEXT(bwc_rules[i], next);

				ret = mlx5dr_bwc_queue_poll(ctx, rule_attr.queue_id,
							    &pending_rules[i], false);
				if (unlikely(ret)) {
					rte_errno = EINVAL;
					goto free_bwc_rules;
				}
			}
		}
	} while (!all_done);

	/* drain all the bwc queues */
	for (i = 0; i < bwc_queues; i++) {
		if (pending_rules[i]) {
			uint16_t queue_id = mlx5dr_bwc_get_queue_id(ctx, i);
			mlx5dr_send_engine_flush_queue(&ctx->send_queue[queue_id]);
			ret = mlx5dr_bwc_queue_poll(ctx, queue_id,
						    &pending_rules[i], true);
			if (unlikely(ret)) {
				rte_errno = EINVAL;
				goto free_bwc_rules;
			}
		}
	}

	rte_errno = 0;

free_bwc_rules:
	simple_free(bwc_rules);
free_pending_rules:
	simple_free(pending_rules);

	return -rte_errno;
}

static int
mlx5dr_bwc_matcher_move(struct mlx5dr_bwc_matcher *bwc_matcher)
{
	struct mlx5dr_matcher_attr matcher_attr = {0};
	struct mlx5dr_matcher *old_matcher;
	struct mlx5dr_matcher *new_matcher;
	int ret;

	mlx5dr_bwc_matcher_init_attr(&matcher_attr,
				     bwc_matcher->priority,
				     bwc_matcher->size_log,
				     mlx5dr_table_is_root(bwc_matcher->matcher->tbl));

	old_matcher = bwc_matcher->matcher;
	new_matcher = mlx5dr_matcher_create(old_matcher->tbl,
					    &bwc_matcher->mt, 1,
					    bwc_matcher->at,
					    bwc_matcher->num_of_at,
					    &matcher_attr);
	if (!new_matcher) {
		DR_LOG(ERR, "Rehash error: matcher creation failed");
		return -ENOMEM;
	}

	ret = mlx5dr_matcher_resize_set_target(old_matcher, new_matcher);
	if (ret) {
		DR_LOG(ERR, "Rehash error: failed setting resize target");
		return ret;
	}

	ret = mlx5dr_bwc_matcher_move_all(bwc_matcher);
	if (ret) {
		DR_LOG(ERR, "Rehash error: moving rules failed");
		return -ENOMEM;
	}

	bwc_matcher->matcher = new_matcher;
	mlx5dr_matcher_destroy(old_matcher);

	return 0;
}

static int
mlx5dr_bwc_matcher_rehash_size(struct mlx5dr_bwc_matcher *bwc_matcher)
{
	uint32_t num_of_rules;
	int ret;

	/* If the current matcher size is already at its max size, we can't
	 * do the rehash. Skip it and try adding the rule again - perhaps
	 * there was some change.
	 */
	if (mlx5dr_bwc_matcher_size_maxed_out(bwc_matcher))
		return 0;

	/* It is possible that other rule has already performed rehash.
	 * Need to check again if we really need rehash.
	 * If the reason for rehash was size, but not any more - skip rehash.
	 */
	num_of_rules = rte_atomic_load_explicit(&bwc_matcher->num_of_rules,
						rte_memory_order_relaxed);
	if (!mlx5dr_bwc_matcher_rehash_size_needed(bwc_matcher, num_of_rules))
		return 0;

	/* Now we're done all the checking - do the rehash:
	 *  - extend match RTC size
	 *  - create new matcher
	 *  - move all the rules to the new matcher
	 *  - destroy the old matcher
	 */

	ret = mlx5dr_bwc_matcher_extend_size(bwc_matcher);
	if (ret)
		return ret;

	return mlx5dr_bwc_matcher_move(bwc_matcher);
}

static int
mlx5dr_bwc_matcher_rehash_at(struct mlx5dr_bwc_matcher *bwc_matcher)
{
	/* Rehash by action template doesn't require any additional checking.
	 * The bwc_matcher already contains the new action template.
	 * Just do the usual rehash:
	 *  - create new matcher
	 *  - move all the rules to the new matcher
	 *  - destroy the old matcher
	 */
	return mlx5dr_bwc_matcher_move(bwc_matcher);
}

static struct mlx5dr_bwc_rule *
mlx5dr_bwc_rule_create_root(struct mlx5dr_bwc_matcher *bwc_matcher,
			    const struct rte_flow_item flow_items[],
			    struct mlx5dr_rule_action rule_actions[])
{
	struct mlx5dr_bwc_rule *bwc_rule;

	bwc_rule = mlx5dr_bwc_rule_create_root_sync(bwc_matcher,
						    flow_items,
						    mlx5dr_bwc_rule_actions_num(rule_actions),
						    rule_actions);

	if (unlikely(!bwc_rule))
		DR_LOG(ERR, "BWC rule: failed creating rule on root tbl");

	return bwc_rule;
}

static struct mlx5dr_bwc_rule *
mlx5dr_bwc_rule_create_hws(struct mlx5dr_bwc_matcher *bwc_matcher,
			   const struct rte_flow_item flow_items[],
			   struct mlx5dr_rule_action rule_actions[])
{
	struct mlx5dr_context *ctx = bwc_matcher->matcher->tbl->ctx;
	struct mlx5dr_bwc_rule *bwc_rule = NULL;
	struct mlx5dr_rule_attr rule_attr;
	rte_spinlock_t *queue_lock;
	uint32_t num_of_rules;
	uint16_t idx;
	int at_idx;
	int ret;

	idx = mlx5dr_bwc_gen_queue_idx(ctx);

	mlx5dr_bwc_rule_fill_attr(bwc_matcher, idx, &rule_attr);

	queue_lock = mlx5dr_bwc_get_queue_lock(ctx, idx);

	rte_spinlock_lock(queue_lock);

	/* check if rehash needed due to missing action template */
	at_idx = mlx5dr_bwc_matcher_find_at(bwc_matcher, rule_actions);
	if (unlikely(at_idx < 0)) {
		/* we need to extend BWC matcher action templates array */
		rte_spinlock_unlock(queue_lock);
		mlx5dr_bwc_lock_all_queues(ctx);

		ret = mlx5dr_bwc_matcher_extend_at(bwc_matcher, rule_actions);
		if (unlikely(ret)) {
			mlx5dr_bwc_unlock_all_queues(ctx);
			rte_errno = EINVAL;
			DR_LOG(ERR, "BWC rule: failed extending action template - %d", ret);
			return NULL;
		}

		/* action templates array was extended, we need the last idx */
		at_idx = bwc_matcher->num_of_at - 1;

		ret = mlx5dr_matcher_attach_at(bwc_matcher->matcher,
					       bwc_matcher->at[at_idx]);
		if (unlikely(ret)) {
			/* Action template attach failed, possibly due to
			 * requiring more action STEs.
			 * Need to attempt creating new matcher with all
			 * the action templates, including the new one.
			 */
			ret = mlx5dr_bwc_matcher_rehash_at(bwc_matcher);
			if (unlikely(ret)) {
				mlx5dr_action_template_destroy(bwc_matcher->at[at_idx]);
				bwc_matcher->at[at_idx] = NULL;
				bwc_matcher->num_of_at--;

				mlx5dr_bwc_unlock_all_queues(ctx);

				DR_LOG(ERR, "BWC rule insertion: rehash AT failed - %d", ret);
				return NULL;
			}
		}

		mlx5dr_bwc_unlock_all_queues(ctx);
		rte_spinlock_lock(queue_lock);
	}

	/* check if number of rules require rehash */
	num_of_rules = rte_atomic_load_explicit(&bwc_matcher->num_of_rules,
						rte_memory_order_relaxed);
	if (unlikely(mlx5dr_bwc_matcher_rehash_size_needed(bwc_matcher, num_of_rules))) {
		rte_spinlock_unlock(queue_lock);

		mlx5dr_bwc_lock_all_queues(ctx);
		ret = mlx5dr_bwc_matcher_rehash_size(bwc_matcher);
		mlx5dr_bwc_unlock_all_queues(ctx);

		if (ret) {
			DR_LOG(ERR, "BWC rule insertion: rehash size [%d -> %d] failed - %d",
			       bwc_matcher->size_log - MLX5DR_BWC_MATCHER_SIZE_LOG_STEP,
			       bwc_matcher->size_log,
			       ret);
			return NULL;
		}

		rte_spinlock_lock(queue_lock);
	}

	bwc_rule = mlx5dr_bwc_rule_create_hws_sync(bwc_matcher,
						   flow_items,
						   at_idx,
						   rule_actions,
						   &rule_attr);

	if (likely(bwc_rule)) {
		mlx5dr_bwc_rule_list_add(bwc_rule, idx);
		rte_spinlock_unlock(queue_lock);
		return bwc_rule; /* rule inserted successfully */
	}

	/* At this point the rule wasn't added.
	 * It could be because there was collision, or some other problem.
	 * If we don't dive deeper than API, the only thing we know is that
	 * the status of completion is RTE_FLOW_OP_ERROR.
	 * Try rehash by size and insert rule again - last chance.
	 */

	rte_spinlock_unlock(queue_lock);

	mlx5dr_bwc_lock_all_queues(ctx);
	ret = mlx5dr_bwc_matcher_rehash_size(bwc_matcher);
	mlx5dr_bwc_unlock_all_queues(ctx);

	if (ret) {
		DR_LOG(ERR, "BWC rule insertion: rehash failed - %d", ret);
		return NULL;
	}

	/* Rehash done, but we still have that pesky rule to add */
	rte_spinlock_lock(queue_lock);

	bwc_rule = mlx5dr_bwc_rule_create_hws_sync(bwc_matcher,
						   flow_items,
						   at_idx,
						   rule_actions,
						   &rule_attr);

	if (unlikely(!bwc_rule)) {
		rte_spinlock_unlock(queue_lock);
		DR_LOG(ERR, "BWC rule insertion failed");
		return NULL;
	}

	mlx5dr_bwc_rule_list_add(bwc_rule, idx);
	rte_spinlock_unlock(queue_lock);

	return bwc_rule;
}

struct mlx5dr_bwc_rule *
mlx5dr_bwc_rule_create(struct mlx5dr_bwc_matcher *bwc_matcher,
		       const struct rte_flow_item flow_items[],
		       struct mlx5dr_rule_action rule_actions[])
{
	struct mlx5dr_context *ctx = bwc_matcher->matcher->tbl->ctx;

	if (unlikely(!mlx5dr_context_bwc_supported(ctx))) {
		rte_errno = EINVAL;
		DR_LOG(ERR, "BWC rule: Context created w/o BWC API compatibility");
		return NULL;
	}

	if (unlikely(mlx5dr_table_is_root(bwc_matcher->matcher->tbl)))
		return mlx5dr_bwc_rule_create_root(bwc_matcher,
						   flow_items,
						   rule_actions);

	return mlx5dr_bwc_rule_create_hws(bwc_matcher,
					  flow_items,
					  rule_actions);
}
