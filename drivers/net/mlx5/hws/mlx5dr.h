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
	MLX5DR_TABLE_TYPE_FDB_RX,
	MLX5DR_TABLE_TYPE_FDB_TX,
	MLX5DR_TABLE_TYPE_FDB_UNIFIED,
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
	MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2,
	MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2,
	MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2,
	MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3,
	MLX5DR_ACTION_TYP_DROP,
	MLX5DR_ACTION_TYP_TIR,
	MLX5DR_ACTION_TYP_TBL,
	MLX5DR_ACTION_TYP_CTR,
	MLX5DR_ACTION_TYP_TAG,
	MLX5DR_ACTION_TYP_MODIFY_HDR,
	MLX5DR_ACTION_TYP_VPORT,
	MLX5DR_ACTION_TYP_MISS,
	MLX5DR_ACTION_TYP_POP_VLAN,
	MLX5DR_ACTION_TYP_PUSH_VLAN,
	MLX5DR_ACTION_TYP_ASO_METER,
	MLX5DR_ACTION_TYP_ASO_CT,
	MLX5DR_ACTION_TYP_INSERT_HEADER,
	MLX5DR_ACTION_TYP_REMOVE_HEADER,
	MLX5DR_ACTION_TYP_DEST_ROOT,
	MLX5DR_ACTION_TYP_DEST_ARRAY,
	MLX5DR_ACTION_TYP_POP_IPV6_ROUTE_EXT,
	MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT,
	MLX5DR_ACTION_TYP_NAT64,
	MLX5DR_ACTION_TYP_JUMP_TO_MATCHER,
	MLX5DR_ACTION_TYP_MAX,
};

enum mlx5dr_action_flags {
	MLX5DR_ACTION_FLAG_ROOT_RX = 1 << 0,
	MLX5DR_ACTION_FLAG_ROOT_TX = 1 << 1,
	MLX5DR_ACTION_FLAG_ROOT_FDB = 1 << 2,
	MLX5DR_ACTION_FLAG_HWS_RX = 1 << 3,
	MLX5DR_ACTION_FLAG_HWS_TX = 1 << 4,
	MLX5DR_ACTION_FLAG_HWS_FDB = 1 << 5,
	MLX5DR_ACTION_FLAG_HWS_FDB_RX = 1 << 6,
	MLX5DR_ACTION_FLAG_HWS_FDB_TX = 1 << 7,
	MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED = 1 << 8,
	/* Shared action can be used over a few threads, since data is written
	 * only once at the creation of the action.
	 */
	MLX5DR_ACTION_FLAG_SHARED = 1 << 9,
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
	MLX5DR_MATCH_TEMPLATE_FLAG_NONE = 0,
	/* Allow relaxed matching by skipping derived dependent match fields. */
	MLX5DR_MATCH_TEMPLATE_FLAG_RELAXED_MATCH = 1,
};

enum mlx5dr_action_template_flags {
	/* Allow relaxed actions order. */
	MLX5DR_ACTION_TEMPLATE_FLAG_RELAXED_ORDER = 1 << 0,
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
	/* Optional PD used for allocating resources */
	struct ibv_pd *pd;
	/* Optional the STC array size for that context */
	size_t initial_log_stc_memory;
	/* Optional other ctx for resources allocation, all objects will be created on it */
	struct ibv_context *shared_ibv_ctx;
	bool bwc; /* add support for backward compatible API*/
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

/* Match mode describes the behavior of the matcher STE's when a packet arrives */
enum mlx5dr_matcher_match_mode {
	/* Packet arriving at this matcher STE's will match according it's tag and match definer */
	MLX5DR_MATCHER_MATCH_MODE_DEFAULT = 0x0,
	/* Packet arriving at this matcher STE's will always hit and perform the actions */
	MLX5DR_MATCHER_MATCH_MODE_ALWAYS_HIT = 0x1,
};

enum mlx5dr_rule_hash_calc_mode {
	MLX5DR_RULE_HASH_CALC_MODE_RAW,
	MLX5DR_RULE_HASH_CALC_MODE_IDX,
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
	/* Define the insertion, distribution and match modes for this matcher */
	enum mlx5dr_matcher_insert_mode insert_mode;
	enum mlx5dr_matcher_distribute_mode distribute_mode;
	enum mlx5dr_matcher_match_mode match_mode;
	/* Define whether the created matcher supports resizing into a bigger matcher */
	bool resizable;
	/* This will imply that this matcher is not part of the matchers chain of parent table */
	bool isolated;
	union {
		struct {
			uint8_t sz_row_log;
			uint8_t sz_col_log;
		} table;

		struct {
			uint8_t num_log;
		} rule;
	};
	/* Optional AT attach configuration - Max number of additional AT */
	uint8_t max_num_of_at_attach;
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

struct mlx5dr_action_reformat_header {
	size_t sz;
	void *data;
};

struct mlx5dr_action_insert_header {
	struct mlx5dr_action_reformat_header hdr;
	/* PRM start anchor to which header will be inserted */
	uint8_t anchor;
	/* Header insertion offset in bytes, from the start
	 * anchor to the location where new header will be inserted.
	 */
	uint8_t offset;
	/* Indicates this header insertion adds encapsulation header to the packet,
	 * requiring device to update offloaded fields (for example IPv4 total length).
	 */
	bool encap;
	/* It must be set when adding ESP header.
	 * It's also sets the next_protocol value in the ipsec trailer.
	 */
	bool push_esp;
};

enum mlx5dr_action_remove_header_type {
	MLX5DR_ACTION_REMOVE_HEADER_TYPE_BY_OFFSET,
	MLX5DR_ACTION_REMOVE_HEADER_TYPE_BY_HEADER,
};

struct mlx5dr_action_remove_header_attr {
	enum mlx5dr_action_remove_header_type type;
	union {
		struct {
			/* PRM start anchor from which header will be removed */
			uint8_t start_anchor;
			/* PRM end anchor till which header will be removed */
			uint8_t end_anchor;
			bool decap;
		} by_anchor;
		struct {
			/* PRM start anchor from which header will be removed */
			uint8_t start_anchor;
			uint8_t size;
		} by_offset;
	};
};

struct mlx5dr_action_mh_pattern {
	/* Byte size of modify actions provided by "data" */
	size_t sz;
	/* PRM format modify actions pattern */
	__be64 *data;
};

/* In actions that take offset, the offset is unique, pointing to a single
 * resource and the user should not reuse the same index because data changing
 * is not atomic.
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
			uint8_t pattern_idx;
			uint8_t *data;
		} modify_header;

		struct {
			uint32_t offset;
			uint8_t hdr_idx;
			uint8_t *data;
		} reformat;

		struct {
			uint32_t offset;
			uint8_t *header;
		} ipv6_ext;

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

		struct {
			uint32_t offset;
		} jump_to_matcher;
	};
};

struct mlx5dr_action_dest_attr {
	/* Required action combination */
	enum mlx5dr_action_type *action_type;

	/* Required destination action to forward the packet */
	struct mlx5dr_action *dest;

	/* Optional reformat data */
	struct {
		size_t reformat_data_sz;
		void *reformat_data;
	} reformat;
};

enum mlx5dr_action_jump_to_matcher_type {
	MLX5DR_ACTION_JUMP_TO_MATCHER_BY_INDEX,
};

struct mlx5dr_action_jump_to_matcher_attr {
	enum mlx5dr_action_jump_to_matcher_type type;
	struct mlx5dr_matcher *matcher;
};

union mlx5dr_crc_encap_entropy_hash_ip_field {
	uint8_t  ipv6_addr[16];
	struct {
		uint8_t  reserved[12];
		rte_be32_t  ipv4_addr;
	};
};

struct __rte_packed_begin mlx5dr_crc_encap_entropy_hash_fields {
	union mlx5dr_crc_encap_entropy_hash_ip_field dst;
	union mlx5dr_crc_encap_entropy_hash_ip_field src;
	uint8_t next_protocol;
	rte_be16_t dst_port;
	rte_be16_t src_port;
} __rte_packed_end;

enum mlx5dr_crc_encap_entropy_hash_size {
	MLX5DR_CRC_ENCAP_ENTROPY_HASH_SIZE_8,
	MLX5DR_CRC_ENCAP_ENTROPY_HASH_SIZE_16,
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

/* Set default miss table for mlx5dr_table by using another mlx5dr_table
 * Traffic which all table matchers miss will be forwarded to miss table.
 *
 * @param[in] tbl
 *	source mlx5dr table
 * @param[in] miss_tbl
 *	target (miss) mlx5dr table, or NULL to remove current miss table
 * @return zero on success non zero otherwise.
 */
int mlx5dr_table_set_default_miss(struct mlx5dr_table *tbl,
				  struct mlx5dr_table *miss_tbl);

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
 * @param[in] flags
 *	Template creation flags
 * @return pointer to mlx5dr_action_template on success NULL otherwise
 */
struct mlx5dr_action_template *
mlx5dr_action_template_create(const enum mlx5dr_action_type action_type[],
			      uint32_t flags);

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

/* Attach new action template to direct rule matcher.
 *
 * @param[in] matcher
 *	Matcher to attach at to.
 * @param[in] at
 *	Action template to be attached to the matcher.
 * @return zero on success non zero otherwise.
 */
int mlx5dr_matcher_attach_at(struct mlx5dr_matcher *matcher,
			     struct mlx5dr_action_template *at);

/* Link two matchers and enable moving rules from src matcher to dst matcher.
 * Both matchers must be in the same table type, must be created with 'resizable'
 * property, and should have the same characteristics (e.g. same mt, same at).
 *
 * It is the user's responsibility to make sure that the dst matcher
 * was allocated with the appropriate size.
 *
 * Once the function is completed, the user is:
 *  - allowed to move rules from src into dst matcher
 *  - no longer allowed to insert rules to the src matcher
 *
 * The user is always allowed to insert rules to the dst matcher and
 * to delete rules from any matcher.
 *
 * @param[in] src_matcher
 *	source matcher for moving rules from
 * @param[in] dst_matcher
 *	destination matcher for moving rules to
 * @return zero on successful move, non zero otherwise.
 */
int mlx5dr_matcher_resize_set_target(struct mlx5dr_matcher *src_matcher,
				     struct mlx5dr_matcher *dst_matcher);

/* Enqueue moving rule operation: moving rule from src matcher to a dst matcher
 *
 * @param[in] src_matcher
 *	matcher that the rule belongs to
 * @param[in] rule
 *	the rule to move
 * @param[in] attr
 *	rule attributes
 * @return zero on success, non zero otherwise.
 */
int mlx5dr_matcher_resize_rule_move(struct mlx5dr_matcher *src_matcher,
				    struct mlx5dr_rule *rule,
				    struct mlx5dr_rule_attr *attr);

/* Check matcher ability to update existing rules
 *
 * @param[in] matcher
 *	that the rule belongs to.
 * @return true when the matcher is updatable false otherwise.
 */
bool mlx5dr_matcher_is_updatable(struct mlx5dr_matcher *matcher);

/* Check matcher if might contain rules that need complex structure
 *
 * @param[in] matcher
 *	that the rule belongs to.
 * @return true when the matcher is contains such rules, false otherwise.
 */
bool mlx5dr_matcher_is_dependent(struct mlx5dr_matcher *matcher);

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

/* Calculate hash for a given set of items, which indicates rule location in
 * the hash table.
 *
 * @param[in] matcher
 *	The matcher of the created rule.
 * @param[in] items
 *	Matching pattern item definition.
 * @param[in] mt_idx
 *	Match template index that the match was created with.
 * @param[in] mode
 *	Hash calculation mode
 * @param[in, out] ret_hash
 *	Returned calculated hash result
 * @return zero on success non zero otherwise.
 */
int mlx5dr_rule_hash_calculate(struct mlx5dr_matcher *matcher,
			       const struct rte_flow_item items[],
			       uint8_t mt_idx,
			       enum mlx5dr_rule_hash_calc_mode mode,
			       uint32_t *ret_hash);

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

/* Create direct rule LAST action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_last(struct mlx5dr_context *ctx,
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

/* Check if counter action on root table is supported.
 *
 * @return true if counter action on root table is supported.
 */
static inline bool
mlx5dr_action_counter_root_is_supported(void)
{
#ifdef HAVE_MLX5DV_FLOW_ACTION_COUNTERS_DEVX_WITH_OFFSET
	return true;
#else
	return false;
#endif
}

/* Create direct rule reformat action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] reformat_type
 *	Type of reformat prefixed with MLX5DR_ACTION_TYP_REFORMAT.
 * @param[in] num_of_hdrs
 *	Number of provided headers in "hdrs" array.
 * @param[in] hdrs
 *	Headers array containing header information.
 * @param[in] log_bulk_size
 *	Number of unique values used with this reformat.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_reformat(struct mlx5dr_context *ctx,
			      enum mlx5dr_action_type reformat_type,
			      uint8_t num_of_hdrs,
			      struct mlx5dr_action_reformat_header *hdrs,
			      uint32_t log_bulk_size,
			      uint32_t flags);

/* Create direct rule modify header action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] num_of_patterns
 *	Number of provided patterns in "patterns" array.
 * @param[in] patterns
 *	Patterns array containing pattern information.
 * @param[in] log_bulk_size
 *	Number of unique values used with this pattern.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_modify_header(struct mlx5dr_context *ctx,
				   uint8_t num_of_patterns,
				   struct mlx5dr_action_mh_pattern *patterns,
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

/* Create a dest array action, this action can duplicate packets and forward to
 * multiple destinations in the destination list.
 * @param[in] ctx
 *     The context in which the new action will be created.
 * @param[in] num_dest
 *     The number of dests attributes.
 * @param[in] dests
 *     The destination array. Each contains a destination action and can have
 *     additional actions.
 * @param[in] flags
 *     Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_dest_array(struct mlx5dr_context *ctx,
				size_t num_dest,
				struct mlx5dr_action_dest_attr *dests,
				uint32_t flags);

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

/* Create insert header action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] num_of_hdrs
 *	Number of provided headers in "hdrs" array.
 * @param[in] hdrs
 *	Headers array containing header information.
 * @param[in] log_bulk_size
 *	Number of unique values used with this insert header.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_insert_header(struct mlx5dr_context *ctx,
				   uint8_t num_of_hdrs,
				   struct mlx5dr_action_insert_header *hdrs,
				   uint32_t log_bulk_size,
				   uint32_t flags);

/* Create remove header action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] attr
 *	attributes: specifies the remove header type, PRM start anchor and
 *	the PRM end anchor or the PRM start anchor and remove size in bytes.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_remove_header(struct mlx5dr_context *ctx,
				   struct mlx5dr_action_remove_header_attr *attr,
				   uint32_t flags);

/* Create action to push or remove IPv6 extension header.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] type
 *	Type of direct rule action: MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT or
 *	MLX5DR_ACTION_TYP_POP_IPV6_ROUTE_EXT.
 * @param[in] hdr
 *	Header for packet reformat.
 * @param[in] log_bulk_size
 *	Number of unique values used with this pattern.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_reformat_ipv6_ext(struct mlx5dr_context *ctx,
				       enum mlx5dr_action_type type,
				       struct mlx5dr_action_reformat_header *hdr,
				       uint32_t log_bulk_size,
				       uint32_t flags);

enum mlx5dr_action_nat64_flags {
	MLX5DR_ACTION_NAT64_V4_TO_V6 = 1 << 0,
	MLX5DR_ACTION_NAT64_V6_TO_V4 = 1 << 1,
	/* Indicates if to backup ipv4 addresses in last two registers */
	MLX5DR_ACTION_NAT64_BACKUP_ADDR = 1 << 2,
};

struct mlx5dr_action_nat64_attr {
	uint8_t num_of_registers;
	uint8_t *registers;
	enum mlx5dr_action_nat64_flags flags;
};

/* Create direct rule nat64 action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] attr
 *	The relevant attribute of the NAT action.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_nat64(struct mlx5dr_context *ctx,
			   struct mlx5dr_action_nat64_attr *attr,
			   uint32_t flags);

/* Create direct rule jump to matcher action.
 *
 * @param[in] ctx
 *	The context in which the new action will be created.
 * @param[in] attr
 *	The relevant attribute of the action.
 * @param[in] flags
 *	Action creation flags. (enum mlx5dr_action_flags)
 * @return pointer to mlx5dr_action on success NULL otherwise.
 */
struct mlx5dr_action *
mlx5dr_action_create_jump_to_matcher(struct mlx5dr_context *ctx,
				     struct mlx5dr_action_jump_to_matcher_attr *attr,
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

/* Calculate encap entropy hash value
 *
 * @param[in] ctx
 *	The context to get from it's capabilities the entropy hash type.
 * @param[in] data
 *	The fields for the hash calculation.
 * @param[in] entropy_res
 *	An array to store the hash value to it.
 * @param[in] res_size
 *	The result size.
 * @return zero on success non zero otherwise.
 */
int mlx5dr_crc_encap_entropy_hash_calc(struct mlx5dr_context *ctx,
				       struct mlx5dr_crc_encap_entropy_hash_fields *data,
				       uint8_t entropy_res[],
				       enum mlx5dr_crc_encap_entropy_hash_size res_size);

struct mlx5dr_bwc_matcher;
struct mlx5dr_bwc_rule;

/* Create a new BWC direct rule matcher.
 * This function does the following:
 *   - creates match template based on flow items
 *   - creates an empty action template
 *   - creates a usual mlx5dr_matcher with these mt and at, setting
 *     its size to minimal
 * Notes:
 *   - table->ctx must have BWC support
 *   - complex rules are not supported
 *
 * @param[in] table
 *	The table in which the new matcher will be opened
 * @param[in] priority
 *	Priority for this BWC matcher
 * @param[in] flow_items
 *	Array of flow items that serve as basis for match and action templates
 * @return pointer to mlx5dr_bwc_matcher on success or NULL otherwise.
 */
struct mlx5dr_bwc_matcher *
mlx5dr_bwc_matcher_create(struct mlx5dr_table *table,
			  uint32_t priority,
			  const struct rte_flow_item flow_items[]);

/* Destroy BWC direct rule matcher.
 *
 * @param[in] bwc_matcher
 *	Matcher to destroy
 * @return zero on success, non zero otherwise
 */
int mlx5dr_bwc_matcher_destroy(struct mlx5dr_bwc_matcher *bwc_matcher);

/* Create a new BWC rule.
 * Unlike the usual rule creation function, this one is blocking: when the
 * function returns, the rule is written to its place (no need to poll).
 * This function does the following:
 *   - finds matching action template based on the provided rule_actions, or
 *     creates new action template if matching action template doesn't exist
 *   - updates corresponding BWC matcher stats
 *   - if needed, the function performs rehash:
 *       - creates a new matcher based on mt, at, new_sz
 *       - moves all the existing matcher rules to the new matcher
 *       - removes the old matcher
 *   - inserts new rule
 *   - polls till completion is received
 * Notes:
 *   - matcher->tbl->ctx must have BWC support
 *   - separate BWC ctx queues are used
 *
 * @param[in] bwc_matcher
 *	The BWC matcher in which the new rule will be created.
 * @param[in] flow_items
 *	Flow items to be used for the value matching
 * @param[in] rule_actions
 *	Rule action to be executed on match
 * @param[in, out] rule_handle
 *	A valid rule handle. The handle doesn't require any initialization
 * @return valid BWC rule handle on success, NULL otherwise
 */
struct mlx5dr_bwc_rule *
mlx5dr_bwc_rule_create(struct mlx5dr_bwc_matcher *bwc_matcher,
		       const struct rte_flow_item flow_items[],
		       struct mlx5dr_rule_action rule_actions[]);

/* Destroy BWC direct rule.
 *
 * @param[in] bwc_rule
 *	Rule to destroy
 * @return zero on success, non zero otherwise
 */
int mlx5dr_bwc_rule_destroy(struct mlx5dr_bwc_rule *bwc_rule);

#endif
