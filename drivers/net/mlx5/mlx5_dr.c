/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA CORPORATION. All rights reserved.
 */
#include <rte_flow.h>

#include "mlx5_defs.h"
#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
#include "mlx5_dr.h"

/*
 * The following null stubs are prepared in order not to break the linkage
 * before the HW steering low-level implementation is added.
 */

/* Open a context used for direct rule insertion using hardware steering.
 * Each context can contain multiple tables of different types.
 *
 * @param[in] ibv_ctx
 *	The ibv context to used for HWS.
 * @param[in] attr
 *	Attributes used for context open.
 * @return pointer to mlx5dr_context on success NULL otherwise.
 */
__rte_weak struct mlx5dr_context *
mlx5dr_context_open(void *ibv_ctx,
		    struct mlx5dr_context_attr *attr)
{
	(void)ibv_ctx;
	(void)attr;
	return NULL;
}

/* Close a context used for direct hardware steering.
 *
 * @param[in] ctx
 *	mlx5dr context to close.
 * @return zero on success non zero otherwise.
 */
__rte_weak int
mlx5dr_context_close(struct mlx5dr_context *ctx)
{
	(void)ctx;
	return 0;
}

/* Create a new direct rule table. Each table can contain multiple matchers.
 *
 * @param[in] ctx
 *	The context in which the new table will be opened.
 * @param[in] attr
 *	Attributes used for table creation.
 * @return pointer to mlx5dr_table on success NULL otherwise.
 */
__rte_weak struct mlx5dr_table *
mlx5dr_table_create(struct mlx5dr_context *ctx,
		    struct mlx5dr_table_attr *attr)
{
	(void)ctx;
	(void)attr;
	return NULL;
}

/* Destroy direct rule table.
 *
 * @param[in] tbl
 *	mlx5dr table to destroy.
 * @return zero on success non zero otherwise.
 */
__rte_weak int mlx5dr_table_destroy(struct mlx5dr_table *tbl)
{
	(void)tbl;
	return 0;
}

/* Create new match template based on items mask, the match template
 * will be used for matcher creation.
 *
 * @param[in] items
 *	Describe the mask for template creation
 * @param[in] flags
 *	Template creation flags
 * @return pointer to mlx5dr_match_template on success NULL otherwise
 */
__rte_weak struct mlx5dr_match_template *
mlx5dr_match_template_create(const struct rte_flow_item items[],
			     enum mlx5dr_match_template_flags flags)
{
	(void)items;
	(void)flags;
	return NULL;
}

/* Destroy match template.
 *
 * @param[in] mt
 *	Match template to destroy.
 * @return zero on success non zero otherwise.
 */
__rte_weak int
mlx5dr_match_template_destroy(struct mlx5dr_match_template *mt)
{
	(void)mt;
	return 0;
}

/* Create a new direct rule matcher. Each matcher can contain multiple rules.
 * Matchers on the table will be processed by priority. Matching fields and
 * mask are described by the match template. In some cases multiple match
 * templates can be used on the same matcher.
 *
 * @param[in] table
 *	The table in which the new matcher will be opened.
 * @param[in] mt
 *	Array of match templates to be used on matcher.
 * @param[in] num_of_mt
 *	Number of match templates in mt array.
 * @param[in] attr
 *	Attributes used for matcher creation.
 * @return pointer to mlx5dr_matcher on success NULL otherwise.
 */
__rte_weak struct mlx5dr_matcher *
mlx5dr_matcher_create(struct mlx5dr_table *table __rte_unused,
		      struct mlx5dr_match_template *mt[] __rte_unused,
		      uint8_t num_of_mt __rte_unused,
		      struct mlx5dr_matcher_attr *attr __rte_unused)
{
	return NULL;
}

/* Destroy direct rule matcher.
 *
 * @param[in] matcher
 *	Matcher to destroy.
 * @return zero on success non zero otherwise.
 */
__rte_weak int
mlx5dr_matcher_destroy(struct mlx5dr_matcher *matcher __rte_unused)
{
	return 0;
}

/* Enqueue create rule operation.
 *
 * @param[in] matcher
 *	The matcher in which the new rule will be created.
 * @param[in] mt_idx
 *	Match template index to create the rule with.
 * @param[in] items
 *	The items used for the value matching.
 * @param[in] rule_actions
 *	Rule action to be executed on match.
 * @param[in] num_of_actions
 *	Number of rule actions.
 * @param[in] attr
 *	Rule creation attributes.
 * @param[in, out] rule_handle
 *	A valid rule handle. The handle doesn't require any initialization.
 * @return zero on successful enqueue non zero otherwise.
 */
__rte_weak int
mlx5dr_rule_create(struct mlx5dr_matcher *matcher __rte_unused,
		   uint8_t mt_idx __rte_unused,
		   const struct rte_flow_item items[] __rte_unused,
		   struct mlx5dr_rule_action rule_actions[] __rte_unused,
		   uint8_t num_of_actions __rte_unused,
		   struct mlx5dr_rule_attr *attr __rte_unused,
		   struct mlx5dr_rule *rule_handle __rte_unused)
{
	return 0;
}

/* Enqueue destroy rule operation.
 *
 * @param[in] rule
 *	The rule destruction to enqueue.
 * @param[in] attr
 *	Rule destruction attributes.
 * @return zero on successful enqueue non zero otherwise.
 */
__rte_weak int
mlx5dr_rule_destroy(struct mlx5dr_rule *rule __rte_unused,
		    struct mlx5dr_rule_attr *attr __rte_unused)
{
	return 0;
}

/* Create direct rule drop action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
__rte_weak struct mlx5dr_action *
mlx5dr_action_create_dest_drop(struct mlx5dr_context *ctx __rte_unused,
			       uint32_t flags __rte_unused)
{
	return NULL;
}

/* Create direct rule default miss action.
 * Defaults are RX: Drop TX: Wire.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
__rte_weak struct mlx5dr_action *
mlx5dr_action_create_default_miss(struct mlx5dr_context *ctx __rte_unused,
				  uint32_t flags __rte_unused)
{
	return NULL;
}

/* Create direct rule goto table action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] tbl
 *	Destination table.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
__rte_weak struct mlx5dr_action *
mlx5dr_action_create_dest_table(struct mlx5dr_context *ctx __rte_unused,
				struct mlx5dr_table *tbl __rte_unused,
				uint32_t flags __rte_unused)
{
	return NULL;
}

/*  Create direct rule goto TIR action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] obj
 *	Direct rule TIR devx object.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
__rte_weak struct mlx5dr_action *
mlx5dr_action_create_dest_tir(struct mlx5dr_context *ctx __rte_unused,
			      struct mlx5dr_devx_obj *obj __rte_unused,
			      uint32_t flags __rte_unused)
{
	return NULL;
}

/* Create direct rule TAG action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
__rte_weak struct mlx5dr_action *
mlx5dr_action_create_tag(struct mlx5dr_context *ctx __rte_unused,
			 uint32_t flags __rte_unused)
{
	return NULL;
}

/* Create direct rule counter action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] obj
 *	Direct rule counter devx object.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
__rte_weak struct mlx5dr_action *
mlx5dr_action_create_counter(struct mlx5dr_context *ctx,
			     struct mlx5dr_devx_obj *obj,
			     uint32_t flags);

/* Create direct rule reformat action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] reformat_type
 *	Type of reformat.
 * @param[in] data_sz
 *	Size in bytes of data.
 * @param[in] inline_data
 *	Header data array in case of inline action.
 * @param[in] log_bulk_size
 *	Number of unique values used with this pattern.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
__rte_weak struct mlx5dr_action *
mlx5dr_action_create_reformat(struct mlx5dr_context *ctx __rte_unused,
	      enum mlx5dr_action_reformat_type reformat_type __rte_unused,
			      size_t data_sz __rte_unused,
			      void *inline_data __rte_unused,
			      uint32_t log_bulk_size __rte_unused,
			      uint32_t flags __rte_unused)
{
	return NULL;
}

/* Create direct rule modify header action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] pattern_sz
 *	Byte size of the pattern array.
 * @param[in] pattern
 *	PRM format modify pattern action array.
 * @param[in] log_bulk_size
 *	Number of unique values used with this pattern.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
__rte_weak struct mlx5dr_action *
mlx5dr_action_create_modify_header(struct mlx5dr_context *ctx,
				   size_t pattern_sz,
				   rte_be64_t pattern[],
				   uint32_t log_bulk_size,
				   uint32_t flags);

/* Destroy direct rule action.
 *
 * @param[in] action
 *	The action to destroy.
 * @return zero on success non zero otherwise.
 */
__rte_weak int
mlx5dr_action_destroy(struct mlx5dr_action *action __rte_unused)
{
	return 0;
}

/* Poll queue for rule creation and deletions completions.
 *
 * @param[in] ctx
 *	The context to which the queue belong to.
 * @param[in] queue_id
 *	The id of the queue to poll.
 * @param[in, out] res
 *	Completion array.
 * @param[in] res_nb
 *	Maximum number of results to return.
 * @return negative number on failure, the number of completions otherwise.
 */
__rte_weak int
mlx5dr_send_queue_poll(struct mlx5dr_context *ctx __rte_unused,
		       uint16_t queue_id __rte_unused,
		       struct rte_flow_op_result res[] __rte_unused,
		       uint32_t res_nb __rte_unused)
{
	return 0;
}

/* Perform an action on the queue
 *
 * @param[in] ctx
 *	The context to which the queue belong to.
 * @param[in] queue_id
 *	The id of the queue to perform the action on.
 * @param[in] actions
 *	Actions to perform on the queue. (enum mlx5dr_send_queue_actions)
 * @return zero on success non zero otherwise.
 */
__rte_weak int
mlx5dr_send_queue_action(struct mlx5dr_context *ctx __rte_unused,
			 uint16_t queue_id __rte_unused,
			 uint32_t actions __rte_unused)
{
	return 0;
}

#endif
