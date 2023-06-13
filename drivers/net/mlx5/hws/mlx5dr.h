/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5DR_H_
#define MLX5DR_H_

#include <rte_flow.h>

struct mlx5dr_context;
struct mlx5dr_table;
struct mlx5dr_matcher;
struct mlx5dr_rule;
struct ibv_context;

enum mlx5dr_table_type {
	MLX5DR_TABLE_TYPE_NIC_RX,
	MLX5DR_TABLE_TYPE_NIC_TX,
	MLX5DR_TABLE_TYPE_FDB,
	MLX5DR_TABLE_TYPE_MAX,
};

enum mlx5dr_matcher_resource_mode {
	/* Allocate resources based on number of rules with minimal failure probability */
	MLX5DR_MATCHER_RESOURCE_MODE_RULE,
	/* Allocate fixed size hash table based on given column and rows */
	MLX5DR_MATCHER_RESOURCE_MODE_HTABLE,
};

enum mlx5dr_action_type {
	MLX5DR_ACTION_TYP_LAST,
	MLX5DR_ACTION_TYP_TNL_L2_TO_L2,
	MLX5DR_ACTION_TYP_L2_TO_TNL_L2,
	MLX5DR_ACTION_TYP_TNL_L3_TO_L2,
	MLX5DR_ACTION_TYP_L2_TO_TNL_L3,
	MLX5DR_ACTION_TYP_DROP,
	MLX5DR_ACTION_TYP_TIR,
	MLX5DR_ACTION_TYP_FT,
	MLX5DR_ACTION_TYP_CTR,
	MLX5DR_ACTION_TYP_TAG,
	MLX5DR_ACTION_TYP_MODIFY_HDR,
	MLX5DR_ACTION_TYP_VPORT,
	MLX5DR_ACTION_TYP_MISS,
	MLX5DR_ACTION_TYP_POP_VLAN,
	MLX5DR_ACTION_TYP_PUSH_VLAN,
	MLX5DR_ACTION_TYP_ASO_METER,
	MLX5DR_ACTION_TYP_ASO_CT,
	MLX5DR_ACTION_TYP_DEST_ROOT,
	MLX5DR_ACTION_TYP_MAX,
};

enum mlx5dr_action_flags {
	MLX5DR_ACTION_FLAG_ROOT_RX = 1 << 0,
	MLX5DR_ACTION_FLAG_ROOT_TX = 1 << 1,
	MLX5DR_ACTION_FLAG_ROOT_FDB = 1 << 2,
	MLX5DR_ACTION_FLAG_HWS_RX = 1 << 3,
	MLX5DR_ACTION_FLAG_HWS_TX = 1 << 4,
	MLX5DR_ACTION_FLAG_HWS_FDB = 1 << 5,
	/* Shared action can be used over a few threads, since data is written
	 * only once at the creation of the action.
	 */
	MLX5DR_ACTION_FLAG_SHARED = 1 << 6,
};

enum mlx5dr_action_reformat_type {
	MLX5DR_ACTION_REFORMAT_TYPE_TNL_L2_TO_L2,
	MLX5DR_ACTION_REFORMAT_TYPE_L2_TO_TNL_L2,
	MLX5DR_ACTION_REFORMAT_TYPE_TNL_L3_TO_L2,
	MLX5DR_ACTION_REFORMAT_TYPE_L2_TO_TNL_L3,
};

enum mlx5dr_action_aso_meter_color {
	MLX5DR_ACTION_ASO_METER_COLOR_RED = 0x0,
	MLX5DR_ACTION_ASO_METER_COLOR_YELLOW = 0x1,
	MLX5DR_ACTION_ASO_METER_COLOR_GREEN = 0x2,
	MLX5DR_ACTION_ASO_METER_COLOR_UNDEFINED = 0x3,
};

enum mlx5dr_action_aso_ct_flags {
	MLX5DR_ACTION_ASO_CT_DIRECTION_INITIATOR = 0 << 0,
	MLX5DR_ACTION_ASO_CT_DIRECTION_RESPONDER = 1 << 0,
};

enum mlx5dr_match_template_flags {
	/* Allow relaxed matching by skipping derived dependent match fields. */
	MLX5DR_MATCH_TEMPLATE_FLAG_RELAXED_MATCH = 1,
};

enum mlx5dr_send_queue_actions {
	/* Start executing all pending queued rules */
	MLX5DR_SEND_QUEUE_ACTION_DRAIN_ASYNC = 1 << 0,
	/* Start executing all pending queued rules wait till completion */
	MLX5DR_SEND_QUEUE_ACTION_DRAIN_SYNC = 1 << 1,
};

struct mlx5dr_context_attr {
	uint16_t queues;
	uint16_t queue_size;
	size_t initial_log_ste_memory; /* Currently not in use */
	/* Optional PD used for allocating res ources */
	struct ibv_pd *pd;
	/* Optional other ctx for resources allocation, all objects will be created on it */
	struct ibv_context *shared_ibv_ctx;
};

struct mlx5dr_table_attr {
	enum mlx5dr_table_type type;
	uint32_t level;
};

enum mlx5dr_matcher_flow_src {
	MLX5DR_MATCHER_FLOW_SRC_ANY = 0x0,
	MLX5DR_MATCHER_FLOW_SRC_WIRE = 0x1,
	MLX5DR_MATCHER_FLOW_SRC_VPORT = 0x2,
};

enum mlx5dr_matcher_insert_mode {
	MLX5DR_MATCHER_INSERT_BY_HASH = 0x0,
	MLX5DR_MATCHER_INSERT_BY_INDEX = 0x1,
};

enum mlx5dr_matcher_distribute_mode {
	MLX5DR_MATCHER_DISTRIBUTE_BY_HASH = 0x0,
	MLX5DR_MATCHER_DISTRIBUTE_BY_LINEAR = 0x1,
};

struct mlx5dr_matcher_attr {
	/* Processing priority inside table */
	uint32_t priority;
	/* Provide all rules with unique rule_idx in num_log range to reduce locking */
	bool optimize_using_rule_idx;
	/* Resource mode and corresponding size */
	enum mlx5dr_matcher_resource_mode mode;
	/* Optimize insertion in case packet origin is the same for all rules */
	enum mlx5dr_matcher_flow_src optimize_flow_src;
	/* Define the insertion and distribution modes for this matcher */
	enum mlx5dr_matcher_insert_mode insert_mode;
	enum mlx5dr_matcher_distribute_mode distribute_mode;
	union {
		struct {
			uint8_t sz_row_log;
			uint8_t sz_col_log;
		} table;

		struct {
			uint8_t num_log;
		} rule;
	};
};

struct mlx5dr_rule_attr {
	uint16_t queue_id;
	void *user_data;
	/* Valid if matcher optimize_using_rule_idx is set or
	 * if matcher is configured to insert rules by index.
	 */
	uint32_t rule_idx;
	uint32_t burst:1;
};

struct mlx5dr_devx_obj {
	struct mlx5dv_devx_obj *obj;
	uint32_t id;
};

/* In actions that take offset, the offset is unique, and the user should not
 * reuse the same index because data changing is not atomic.
 */
struct mlx5dr_rule_action {
	struct mlx5dr_action *action;
	union {
		struct {
			uint32_t value;
		} tag;

		struct {
			uint32_t offset;
		} counter;

		struct {
			uint32_t offset;
			uint8_t *data;
		} modify_header;

		struct {
			uint32_t offset;
			uint8_t *data;
		} reformat;

		struct {
			rte_be32_t vlan_hdr;
		} push_vlan;

		struct {
			uint32_t offset;
			enum mlx5dr_action_aso_meter_color init_color;
		} aso_meter;

		struct {
			uint32_t offset;
			enum mlx5dr_action_aso_ct_flags direction;
		} aso_ct;
	};
};

/* Open a context used for direct rule insertion using hardware steering.
 * Each context can contain multiple tables of different types.
 *
 * @param[in] ibv_ctx
 *	The ibv context to used for HWS.
 * @param[in] attr
 *	Attributes used for context open.
 * @return pointer to mlx5dr_context on success NULL otherwise.
 */
struct mlx5dr_context *
mlx5dr_context_open(struct ibv_context *ibv_ctx,
		    struct mlx5dr_context_attr *attr);

/* Close a context used for direct hardware steering.
 *
 * @param[in] ctx
 *	mlx5dr context to close.
 * @return zero on success non zero otherwise.
 */
int mlx5dr_context_close(struct mlx5dr_context *ctx);

/* Create a new direct rule table. Each table can contain multiple matchers.
 *
 * @param[in] ctx
 *	The context in which the new table will be opened.
 * @param[in] attr
 *	Attributes used for table creation.
 * @return pointer to mlx5dr_table on success NULL otherwise.
 */
struct mlx5dr_table *
mlx5dr_table_create(struct mlx5dr_context *ctx,
		    struct mlx5dr_table_attr *attr);

/* Destroy direct rule table.
 *
 * @param[in] tbl
 *	mlx5dr table to destroy.
 * @return zero on success non zero otherwise.
 */
int mlx5dr_table_destroy(struct mlx5dr_table *tbl);

/* Create new match template based on items mask, the match template
 * will be used for matcher creation.
 *
 * @param[in] items
 *	Describe the mask for template creation
 * @param[in] flags
 *	Template creation flags
 * @return pointer to mlx5dr_match_template on success NULL otherwise
 */
struct mlx5dr_match_template *
mlx5dr_match_template_create(const struct rte_flow_item items[],
			     enum mlx5dr_match_template_flags flags);

/* Destroy match template.
 *
 * @param[in] mt
 *	Match template to destroy.
 * @return zero on success non zero otherwise.
 */
int mlx5dr_match_template_destroy(struct mlx5dr_match_template *mt);

/* Create new action template based on action_type array, the action template
 * will be used for matcher creation.
 *
 * @param[in] action_type
 *	An array of actions based on the order of actions which will be provided
 *	with rule_actions to mlx5dr_rule_create. The last action is marked
 *	using MLX5DR_ACTION_TYP_LAST.
 * @return pointer to mlx5dr_action_template on success NULL otherwise
 */
struct mlx5dr_action_template *
mlx5dr_action_template_create(const enum mlx5dr_action_type action_type[]);

/* Destroy action template.
 *
 * @param[in] at
 *	Action template to destroy.
 * @return zero on success non zero otherwise.
 */
int mlx5dr_action_template_destroy(struct mlx5dr_action_template *at);

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
 * @param[in] at
 *	Array of action templates to be used on matcher.
 * @param[in] num_of_at
 *	Number of action templates in mt array.
 * @param[in] attr
 *	Attributes used for matcher creation.
 * @return pointer to mlx5dr_matcher on success NULL otherwise.
 */
struct mlx5dr_matcher *
mlx5dr_matcher_create(struct mlx5dr_table *table,
		      struct mlx5dr_match_template *mt[],
		      uint8_t num_of_mt,
		      struct mlx5dr_action_template *at[],
		      uint8_t num_of_at,
		      struct mlx5dr_matcher_attr *attr);

/* Destroy direct rule matcher.
 *
 * @param[in] matcher
 *	Matcher to destroy.
 * @return zero on success non zero otherwise.
 */
int mlx5dr_matcher_destroy(struct mlx5dr_matcher *matcher);

/* Get the size of the rule handle (mlx5dr_rule) to be used on rule creation.
 *
 * @return size in bytes of rule handle struct.
 */
size_t mlx5dr_rule_get_handle_size(void);

/* Enqueue create rule operation.
 *
 * @param[in] matcher
 *	The matcher in which the new rule will be created.
 * @param[in] mt_idx
 *	Match template index to create the match with.
 * @param[in] items
 *	The items used for the value matching.
 * @param[in] rule_actions
 *	Rule action to be executed on match.
 * @param[in] at_idx
 *	Action template index to apply the actions with.
 * @param[in] num_of_actions
 *	Number of rule actions.
 * @param[in] attr
 *	Rule creation attributes.
 * @param[in, out] rule_handle
 *	A valid rule handle. The handle doesn't require any initialization.
 * @return zero on successful enqueue non zero otherwise.
 */
int mlx5dr_rule_create(struct mlx5dr_matcher *matcher,
		       uint8_t mt_idx,
		       const struct rte_flow_item items[],
		       uint8_t at_idx,
		       struct mlx5dr_rule_action rule_actions[],
		       struct mlx5dr_rule_attr *attr,
		       struct mlx5dr_rule *rule_handle);

/* Enqueue destroy rule operation.
 *
 * @param[in] rule
 *	The rule destruction to enqueue.
 * @param[in] attr
 *	Rule destruction attributes.
 * @return zero on successful enqueue non zero otherwise.
 */
int mlx5dr_rule_destroy(struct mlx5dr_rule *rule,
			struct mlx5dr_rule_attr *attr);

/* Enqueue update actions on an existing rule.
 *
 * @param[in, out] rule_handle
 *	A valid rule handle to update.
 * @param[in] at_idx
 *	Action template index to update the actions with.
 *  @param[in] rule_actions
 *	Rule action to be executed on match.
 * @param[in] attr
 *	Rule update attributes.
 * @return zero on successful enqueue non zero otherwise.
 */
int mlx5dr_rule_action_update(struct mlx5dr_rule *rule_handle,
			      uint8_t at_idx,
			      struct mlx5dr_rule_action rule_actions[],
			      struct mlx5dr_rule_attr *attr);

/* Create direct rule drop action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_dest_drop(struct mlx5dr_context *ctx,
			       uint32_t flags);

/* Create direct rule default miss action.
 * Defaults are RX: Drop TX: Wire.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_default_miss(struct mlx5dr_context *ctx,
				  uint32_t flags);

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
struct mlx5dr_action *
mlx5dr_action_create_dest_table(struct mlx5dr_context *ctx,
				struct mlx5dr_table *tbl,
				uint32_t flags);

/* Create direct rule goto vport action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] ib_port_num
 *	Destination ib_port number.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_dest_vport(struct mlx5dr_context *ctx,
				uint32_t ib_port_num,
				uint32_t flags);

/*  Create direct rule goto TIR action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] obj
 *	Direct rule TIR devx object.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @param[in] is_local
 *	indicates where the tir object was created, local gvmi or other gvmi
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_dest_tir(struct mlx5dr_context *ctx,
			      struct mlx5dr_devx_obj *obj,
			      uint32_t flags,
			      bool is_local);

/* Create direct rule TAG action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_tag(struct mlx5dr_context *ctx,
			 uint32_t flags);

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
struct mlx5dr_action *
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
struct mlx5dr_action *
mlx5dr_action_create_reformat(struct mlx5dr_context *ctx,
			      enum mlx5dr_action_reformat_type reformat_type,
			      size_t data_sz,
			      void *inline_data,
			      uint32_t log_bulk_size,
			      uint32_t flags);

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
struct mlx5dr_action *
mlx5dr_action_create_modify_header(struct mlx5dr_context *ctx,
				   size_t pattern_sz,
				   __be64 pattern[],
				   uint32_t log_bulk_size,
				   uint32_t flags);

/* Create direct rule ASO flow meter action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] devx_obj
 *	The DEVX ASO object.
 * @param[in] return_reg_c
 *	Copy the ASO object value into this reg_c, after a packet hits a rule with this ASO object.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_aso_meter(struct mlx5dr_context *ctx,
			       struct mlx5dr_devx_obj *devx_obj,
			       uint8_t return_reg_c,
			       uint32_t flags);

/* Create direct rule ASO CT action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] devx_obj
 *	The DEVX ASO object.
 * @param[in] return_reg_id
 *	Copy the ASO object value into this reg_id, after a packet hits a rule with this ASO object.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_aso_ct(struct mlx5dr_context *ctx,
			    struct mlx5dr_devx_obj *devx_obj,
			    uint8_t return_reg_id,
			    uint32_t flags);

/* Create direct rule pop vlan action.
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_pop_vlan(struct mlx5dr_context *ctx, uint32_t flags);

/* Create direct rule push vlan action.
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_push_vlan(struct mlx5dr_context *ctx, uint32_t flags);

/* Create dest root table, this action will jump to root table according
 * the given priority.
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] priority
 *	The priority of matcher in the root table to jump to.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags).
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_dest_root(struct mlx5dr_context *ctx,
				uint16_t priority,
				uint32_t flags);

/* Destroy direct rule action.
 *
 * @param[in] action
 *	The action to destroy.
 * @return zero on success non zero otherwise.
 */
int mlx5dr_action_destroy(struct mlx5dr_action *action);

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
int mlx5dr_send_queue_poll(struct mlx5dr_context *ctx,
			   uint16_t queue_id,
			   struct rte_flow_op_result res[],
			   uint32_t res_nb);

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
int mlx5dr_send_queue_action(struct mlx5dr_context *ctx,
			     uint16_t queue_id,
			     uint32_t actions);

/* Dump HWS info
 *
 * @param[in] ctx
 *	The context which to dump the info from.
 * @param[in] f
 *	The file to write the dump to.
 * @return zero on success non zero otherwise.
 */
int mlx5dr_debug_dump(struct mlx5dr_context *ctx, FILE *f);

#endif
