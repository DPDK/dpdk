/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_CTL_H__
#define __INCLUDE_RTE_SWX_CTL_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Pipeline Control
 */

#include <stdint.h>
#include <stdio.h>

#include <rte_compat.h>
#include <rte_meter.h>

#include "rte_swx_port.h"
#include "rte_swx_table.h"

struct rte_swx_pipeline;

/** Name size. */
#ifndef RTE_SWX_CTL_NAME_SIZE
#define RTE_SWX_CTL_NAME_SIZE 64
#endif

/*
 * Pipeline Query API.
 */

/** Pipeline info. */
struct rte_swx_ctl_pipeline_info {
	/** Number of input ports. */
	uint32_t n_ports_in;

	/** Number of input ports. */
	uint32_t n_ports_out;

	/** Number of actions. */
	uint32_t n_actions;

	/** Number of tables. */
	uint32_t n_tables;

	/** Number of selector tables. */
	uint32_t n_selectors;

	/** Number of learner tables. */
	uint32_t n_learners;

	/** Number of register arrays. */
	uint32_t n_regarrays;

	/** Number of meter arrays. */
	uint32_t n_metarrays;
};

/**
 * Pipeline info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[out] pipeline
 *   Pipeline info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_info_get(struct rte_swx_pipeline *p,
			      struct rte_swx_ctl_pipeline_info *pipeline);

/**
 * Pipeline NUMA node get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[out] numa_node
 *   Pipeline NUMA node.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_numa_node_get(struct rte_swx_pipeline *p,
				   int *numa_node);

/*
 * Ports Query API.
 */

/**
 * Input port statistics counters read
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] port_id
 *   Port ID (0 .. *n_ports_in* - 1).
 * @param[out] stats
 *   Input port stats.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_port_in_stats_read(struct rte_swx_pipeline *p,
					uint32_t port_id,
					struct rte_swx_port_in_stats *stats);

/**
 * Output port statistics counters read
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] port_id
 *   Port ID (0 .. *n_ports_out* - 1).
 * @param[out] stats
 *   Output port stats.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_port_out_stats_read(struct rte_swx_pipeline *p,
					 uint32_t port_id,
					 struct rte_swx_port_out_stats *stats);

/*
 * Action Query API.
 */

/** Action info. */
struct rte_swx_ctl_action_info {
	/** Action name. */
	char name[RTE_SWX_CTL_NAME_SIZE];

	/** Number of action arguments. */
	uint32_t n_args;
};

/**
 * Action info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] action_id
 *   Action ID (0 .. *n_actions* - 1).
 * @param[out] action
 *   Action info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_action_info_get(struct rte_swx_pipeline *p,
			    uint32_t action_id,
			    struct rte_swx_ctl_action_info *action);

/** Action argument info. */
struct rte_swx_ctl_action_arg_info {
	/** Action argument name. */
	char name[RTE_SWX_CTL_NAME_SIZE];

	/** Action argument size (in bits). */
	uint32_t n_bits;

	/** Non-zero (true) when this action argument must be stored in the
	 * table in network byte order (NBO), zero when it must be stored in
	 * host byte order (HBO).
	 */
	int is_network_byte_order;
};

/**
 * Action argument info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] action_id
 *   Action ID (0 .. *n_actions* - 1).
 * @param[in] action_arg_id
 *   Action ID (0 .. *n_args* - 1).
 * @param[out] action_arg
 *   Action argument info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_action_arg_info_get(struct rte_swx_pipeline *p,
				uint32_t action_id,
				uint32_t action_arg_id,
				struct rte_swx_ctl_action_arg_info *action_arg);

/*
 * Table Query API.
 */

/** Table info. */
struct rte_swx_ctl_table_info {
	/** Table name. */
	char name[RTE_SWX_CTL_NAME_SIZE];

	/** Table creation arguments. */
	char args[RTE_SWX_CTL_NAME_SIZE];

	/** Number of match fields. */
	uint32_t n_match_fields;

	/** Number of actions. */
	uint32_t n_actions;

	/** Non-zero (true) when the default action is constant, therefore it
	 * cannot be changed; zero (false) when the default action not constant,
	 * therefore it can be changed.
	 */
	int default_action_is_const;

	/** Table size parameter. */
	uint32_t size;
};

/**
 * Table info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] table_id
 *   Table ID (0 .. *n_tables* - 1).
 * @param[out] table
 *   Table info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_table_info_get(struct rte_swx_pipeline *p,
			   uint32_t table_id,
			   struct rte_swx_ctl_table_info *table);

/** Table match field info.
 *
 * If (n_bits, offset) are known for all the match fields of the table, then the
 * table (key_offset, key_size, key_mask0) can be computed.
 */
struct rte_swx_ctl_table_match_field_info {
	/** Match type of the current match field. */
	enum rte_swx_table_match_type match_type;

	/** Non-zero (true) when the current match field is part of a registered
	 * header, zero (false) when it is part of the registered meta-data.
	 */
	int is_header;

	/** Match field size (in bits). */
	uint32_t n_bits;

	/** Match field offset within its parent struct (one of the headers or
	 * the meta-data).
	 */
	uint32_t offset;
};

/**
 * Table match field info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] table_id
 *   Table ID (0 .. *n_tables*).
 * @param[in] match_field_id
 *   Match field ID (0 .. *n_match_fields* - 1).
 * @param[out] match_field
 *   Table match field info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_table_match_field_info_get(struct rte_swx_pipeline *p,
	uint32_t table_id,
	uint32_t match_field_id,
	struct rte_swx_ctl_table_match_field_info *match_field);

/** Table action info. */
struct rte_swx_ctl_table_action_info {
	/** Action ID. */
	uint32_t action_id;

	/**  When non-zero (true), the action can be assigned to regular table entries. */
	int action_is_for_table_entries;

	/**  When non-zero (true), the action can be assigned to the table default entry. */
	int action_is_for_default_entry;
};

/**
 * Table action info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] table_id
 *   Table ID (0 .. *n_tables*).
 * @param[in] table_action_id
 *   Action index within the set of table actions (0 .. table n_actions - 1).
 *   Not to be confused with the action ID, which works at the pipeline level
 *   (0 .. pipeline n_actions - 1), which is precisely what this function
 *   returns as part of *table_action*.
 * @param[out] table_action
 *   Table action info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_table_action_info_get(struct rte_swx_pipeline *p,
	uint32_t table_id,
	uint32_t table_action_id,
	struct rte_swx_ctl_table_action_info *table_action);

/**
 * Table operations get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] table_id
 *   Table ID (0 .. *n_tables*).
 * @param[out] table_ops
 *   Table operations. Only valid when function returns success and *is_stub* is
 *   zero (false).
 * @param[out] is_stub
 *   A stub table is a table with no match fields. No "regular" table entries
 *   (i.e. entries other than the default entry) can be added to such a table,
 *   therefore the lookup operation always results in lookup miss. Non-zero
 *   (true) when the current table is a stub table, zero (false) otherwise.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_table_ops_get(struct rte_swx_pipeline *p,
			  uint32_t table_id,
			  struct rte_swx_table_ops *table_ops,
			  int *is_stub);

/** Table statistics. */
struct rte_swx_table_stats {
	/** Number of packets with lookup hit. */
	uint64_t n_pkts_hit;

	/** Number of packets with lookup miss. */
	uint64_t n_pkts_miss;

	/** Number of packets (with either lookup hit or miss) per pipeline
	 * action. Array of pipeline *n_actions* elements indexed by the
	 * pipeline-level *action_id*, therefore this array has the same size
	 * for all the tables within the same pipeline.
	 */
	uint64_t *n_pkts_action;
};

/**
 * Table statistics counters read
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] table_name
 *   Table name.
 * @param[out] stats
 *   Table stats. Must point to a pre-allocated structure. The *n_pkts_action*
 *   field also needs to be pre-allocated as array of pipeline *n_actions*
 *   elements. The pipeline actions that are not valid for the current table
 *   have their associated *n_pkts_action* element always set to zero.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_table_stats_read(struct rte_swx_pipeline *p,
				      const char *table_name,
				      struct rte_swx_table_stats *stats);

/*
 * Selector Table Query API.
 */

/** Selector info. */
struct rte_swx_ctl_selector_info {
	/** Selector table name. */
	char name[RTE_SWX_CTL_NAME_SIZE];

	/** Number of selector fields. */
	uint32_t n_selector_fields;

	/** Maximum number of groups. */
	uint32_t n_groups_max;

	/** Maximum number of members per group. */
	uint32_t n_members_per_group_max;
};

/**
 * Selector table info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] selector_id
 *   Selector table ID (0 .. *n_selectors* - 1).
 * @param[out] selector
 *   Selector table info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_selector_info_get(struct rte_swx_pipeline *p,
			      uint32_t selector_id,
			      struct rte_swx_ctl_selector_info *selector);

/**
 * Selector table "group ID" field info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] selector_id
 *   Selector table ID (0 .. *n_selectors*).
 * @param[out] field
 *   Selector table "group ID" field info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_selector_group_id_field_info_get(struct rte_swx_pipeline *p,
					     uint32_t selector_id,
					     struct rte_swx_ctl_table_match_field_info *field);

/**
 * Sselector table selector field info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] selector_id
 *   Selector table ID (0 .. *n_selectors*).
 * @param[in] selector_field_id
 *   Selector table selector field ID (0 .. *n_selector_fields* - 1).
 * @param[out] field
 *   Selector table selector field info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_selector_field_info_get(struct rte_swx_pipeline *p,
				    uint32_t selector_id,
				    uint32_t selector_field_id,
				    struct rte_swx_ctl_table_match_field_info *field);

/**
 * Selector table "member ID" field info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] selector_id
 *   Selector table ID (0 .. *n_selectors*).
 * @param[out] field
 *   Selector table "member ID" field info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_selector_member_id_field_info_get(struct rte_swx_pipeline *p,
					      uint32_t selector_id,
					      struct rte_swx_ctl_table_match_field_info *field);

/** Selector table statistics. */
struct rte_swx_pipeline_selector_stats {
	/** Number of packets. */
	uint64_t n_pkts;
};

/**
 * Selector table statistics counters read
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] selector_name
 *   Selector table name.
 * @param[out] stats
 *   Selector table stats. Must point to a pre-allocated structure.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_selector_stats_read(struct rte_swx_pipeline *p,
					 const char *selector_name,
					 struct rte_swx_pipeline_selector_stats *stats);

/*
 * Learner Table Query API.
 */

/** Learner table info. */
struct rte_swx_ctl_learner_info {
	/** Learner table name. */
	char name[RTE_SWX_CTL_NAME_SIZE];

	/** Number of match fields. */
	uint32_t n_match_fields;

	/** Number of actions. */
	uint32_t n_actions;

	/** Non-zero (true) when the default action is constant, therefore it
	 * cannot be changed; zero (false) when the default action not constant,
	 * therefore it can be changed.
	 */
	int default_action_is_const;

	/** Learner table size parameter. */
	uint32_t size;
};

/**
 * Learner table info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] learner_id
 *   Learner table ID (0 .. *n_learners* - 1).
 * @param[out] learner
 *   Learner table info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_learner_info_get(struct rte_swx_pipeline *p,
			     uint32_t learner_id,
			     struct rte_swx_ctl_learner_info *learner);

/**
 * Learner table match field info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] learner_id
 *   Learner table ID (0 .. *n_learners* - 1).
 * @param[in] match_field_id
 *   Match field ID (0 .. *n_match_fields* - 1).
 * @param[out] match_field
 *   Learner table match field info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_learner_match_field_info_get(struct rte_swx_pipeline *p,
					 uint32_t learner_id,
					 uint32_t match_field_id,
					 struct rte_swx_ctl_table_match_field_info *match_field);

/**
 * Learner table action info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] learner_id
 *   Learner table ID (0 .. *n_learners* - 1).
 * @param[in] learner_action_id
 *   Action index within the set of learner table actions (0 .. learner table n_actions - 1). Not
 *   to be confused with the pipeline-leve action ID (0 .. pipeline n_actions - 1), which is
 *   precisely what this function returns as part of the *learner_action*.
 * @param[out] learner_action
 *   Learner action info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_learner_action_info_get(struct rte_swx_pipeline *p,
				    uint32_t learner_id,
				    uint32_t learner_action_id,
				    struct rte_swx_ctl_table_action_info *learner_action);

/** Learner table statistics. */
struct rte_swx_learner_stats {
	/** Number of packets with lookup hit. */
	uint64_t n_pkts_hit;

	/** Number of packets with lookup miss. */
	uint64_t n_pkts_miss;

	/** Number of packets with successful learning. */
	uint64_t n_pkts_learn_ok;

	/** Number of packets with learning error. */
	uint64_t n_pkts_learn_err;

	/** Number of packets with forget event. */
	uint64_t n_pkts_forget;

	/** Number of packets (with either lookup hit or miss) per pipeline action. Array of
	 * pipeline *n_actions* elements indexed by the pipeline-level *action_id*, therefore this
	 * array has the same size for all the tables within the same pipeline.
	 */
	uint64_t *n_pkts_action;
};

/**
 * Learner table statistics counters read
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] learner_name
 *   Learner table name.
 * @param[out] stats
 *   Learner table stats. Must point to a pre-allocated structure. The *n_pkts_action* field also
 *   needs to be pre-allocated as array of pipeline *n_actions* elements. The pipeline actions that
 *   are not valid for the current learner table have their associated *n_pkts_action* element
 *   always set to zero.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_learner_stats_read(struct rte_swx_pipeline *p,
				      const char *learner_name,
				      struct rte_swx_learner_stats *stats);

/*
 * Table Update API.
 */

/** Table state. */
struct rte_swx_table_state {
	/** Table object. */
	void *obj;

	/** Action ID of the table default action. */
	uint64_t default_action_id;

	/** Action data of the table default action. Ignored when the action
	 * data size is zero; otherwise, action data size bytes are meaningful.
	 */
	uint8_t *default_action_data;
};

/**
 * Pipeline table state get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[out] table_state
 *   After successful execution, the *table_state* contains the pointer to the
 *   current pipeline table state, which is an array of *n_tables* elements,
 *   with array element i containing the state of the i-th pipeline table. The
 *   pipeline continues to own all the data structures directly or indirectly
 *   referenced by the *table_state* until the subsequent successful invocation
 *   of function *rte_swx_pipeline_table_state_set*.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_pipeline_table_state_get(struct rte_swx_pipeline *p,
				 struct rte_swx_table_state **table_state);

/**
 * Pipeline table state set
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[out] table_state
 *   After successful execution, the pipeline table state is updated to this
 *   *table_state*. The ownership of all the data structures directly or
 *   indirectly referenced by this *table_state* is passed from the caller to
 *   the pipeline.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_pipeline_table_state_set(struct rte_swx_pipeline *p,
				 struct rte_swx_table_state *table_state);

/*
 * High Level Reference Table Update API.
 */

/** Pipeline control opaque data structure. */
struct rte_swx_ctl_pipeline;

/**
 * Pipeline control create
 *
 * @param[in] p
 *   Pipeline handle.
 * @return
 *   Pipeline control handle, on success, or NULL, on error.
 */
__rte_experimental
struct rte_swx_ctl_pipeline *
rte_swx_ctl_pipeline_create(struct rte_swx_pipeline *p);

/**
 * Pipeline table entry add
 *
 * Schedule entry for addition to table or update as part of the next commit
 * operation.
 *
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] table_name
 *   Table name.
 * @param[in] entry
 *   Entry to be added to the table.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_table_entry_add(struct rte_swx_ctl_pipeline *ctl,
				     const char *table_name,
				     struct rte_swx_table_entry *entry);

/**
 * Pipeline table default entry add
 *
 * Schedule table default entry update as part of the next commit operation.
 *
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] table_name
 *   Table name.
 * @param[in] entry
 *   The new table default entry. The *key* and *key_mask* entry fields are
 *   ignored.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_table_default_entry_add(struct rte_swx_ctl_pipeline *ctl,
					     const char *table_name,
					     struct rte_swx_table_entry *entry);

/**
 * Pipeline table entry delete
 *
 * Schedule entry for deletion from table as part of the next commit operation.
 * Request is silently discarded if no such entry exists.
 *
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] table_name
 *   Table name.
 * @param[in] entry
 *   Entry to be deleted from the table. The *action_id* and *action_data* entry
 *   fields are ignored.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_table_entry_delete(struct rte_swx_ctl_pipeline *ctl,
					const char *table_name,
					struct rte_swx_table_entry *entry);

/**
 * Pipeline selector table group add
 *
 * Add a new group to a selector table. This operation is executed before this
 * function returns and its result is independent of the result of the next
 * commit operation.
 *
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] selector_name
 *   Selector table name.
 * @param[out] group_id
 *   The ID of the new group. Only valid when the function call is successful.
 *   This group is initially empty, i.e. it does not contain any members.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOSPC: All groups are currently in use, no group available.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_selector_group_add(struct rte_swx_ctl_pipeline *ctl,
					const char *selector_name,
					uint32_t *group_id);

/**
 * Pipeline selector table group delete
 *
 * Schedule a group for deletion as part of the next commit operation. The group
 * to be deleted can be empty or non-empty.
 *
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] selector_name
 *   Selector table name.
 * @param[in] group_id
 *   Group to be deleted from the selector table.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough memory.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_selector_group_delete(struct rte_swx_ctl_pipeline *ctl,
					   const char *selector_name,
					   uint32_t group_id);

/**
 * Pipeline selector table member add to group
 *
 * Schedule the operation to add a new member to an existing group as part of
 * the next commit operation. If this member is already in this group, the
 * member weight is updated to the new value. A weight of zero means this member
 * is to be deleted from the group.
 *
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] selector_name
 *   Selector table name.
 * @param[in] group_id
 *   The group ID.
 * @param[in] member_id
 *   The member to be added to the group.
 * @param[in] member_weight
 *   Member weight.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough memory;
 *   -ENOSPC: The group is full.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_selector_group_member_add(struct rte_swx_ctl_pipeline *ctl,
					       const char *selector_name,
					       uint32_t group_id,
					       uint32_t member_id,
					       uint32_t member_weight);

/**
 * Pipeline selector table member delete from group
 *
 * Schedule the operation to delete a member from an existing group as part of
 * the next commit operation.
 *
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] selector_name
 *   Selector table name.
 * @param[in] group_id
 *   The group ID. Must be valid.
 * @param[in] member_id
 *   The member to be added to the group. Must be valid.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_selector_group_member_delete(struct rte_swx_ctl_pipeline *ctl,
						  const char *selector_name,
						  uint32_t group_id,
						  uint32_t member_id);

/**
 * Pipeline learner table default entry add
 *
 * Schedule learner table default entry update as part of the next commit operation.
 *
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] learner_name
 *   Learner table name.
 * @param[in] entry
 *   The new table default entry. The *key* and *key_mask* entry fields are ignored.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_learner_default_entry_add(struct rte_swx_ctl_pipeline *ctl,
					       const char *learner_name,
					       struct rte_swx_table_entry *entry);

/**
 * Pipeline commit
 *
 * Perform all the scheduled table work.
 *
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] abort_on_fail
 *   When non-zero (false), all the scheduled work is discarded after a failed
 *   commit. Otherwise, the scheduled work is still kept pending for the next
 *   commit.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_commit(struct rte_swx_ctl_pipeline *ctl,
			    int abort_on_fail);

/**
 * Pipeline abort
 *
 * Discard all the scheduled table work.
 *
 * @param[in] ctl
 *   Pipeline control handle.
 */
__rte_experimental
void
rte_swx_ctl_pipeline_abort(struct rte_swx_ctl_pipeline *ctl);

/**
 * Pipeline table entry read
 *
 * Read table entry from string.
 *
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] table_name
 *   Table name.
 * @param[in] string
 *   String containing the table entry.
 * @param[out] is_blank_or_comment
 *   On error, this argument provides an indication of whether *string* contains
 *   an invalid table entry (set to zero) or a blank or comment line that should
 *   typically be ignored (set to a non-zero value).
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
struct rte_swx_table_entry *
rte_swx_ctl_pipeline_table_entry_read(struct rte_swx_ctl_pipeline *ctl,
				      const char *table_name,
				      const char *string,
				      int *is_blank_or_comment);

/**
 * Pipeline learner table default entry read
 *
 * Read learner table default entry from string.
 *
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] learner_name
 *   Learner table name.
 * @param[in] string
 *   String containing the learner table default entry.
 * @param[out] is_blank_or_comment
 *   On error, this argument provides an indication of whether *string* contains
 *   an invalid table entry (set to zero) or a blank or comment line that should
 *   typically be ignored (set to a non-zero value).
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
struct rte_swx_table_entry *
rte_swx_ctl_pipeline_learner_default_entry_read(struct rte_swx_ctl_pipeline *ctl,
						const char *learner_name,
						const char *string,
						int *is_blank_or_comment);

/**
 * Pipeline table print to file
 *
 * Print all the table entries to file.
 *
 * @param[in] f
 *   Output file.
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] table_name
 *   Table name.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_table_fprintf(FILE *f,
				   struct rte_swx_ctl_pipeline *ctl,
				   const char *table_name);

/**
 * Pipeline selector print to file
 *
 * Print all the selector entries to file.
 *
 * @param[in] f
 *   Output file.
 * @param[in] ctl
 *   Pipeline control handle.
 * @param[in] selector_name
 *   Selector table name.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_selector_fprintf(FILE *f,
				      struct rte_swx_ctl_pipeline *ctl,
				      const char *selector_name);

/*
 * Register Array Query API.
 */

/** Register array info. */
struct rte_swx_ctl_regarray_info {
	/** Register array name. */
	char name[RTE_SWX_CTL_NAME_SIZE];

	/** Register array size. */
	uint32_t size;
};

/**
 * Register array info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] regarray_id
 *   Register array ID (0 .. *n_regarrays* - 1).
 * @param[out] regarray
 *   Register array info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_regarray_info_get(struct rte_swx_pipeline *p,
			      uint32_t regarray_id,
			      struct rte_swx_ctl_regarray_info *regarray);

/**
 * Register read
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] regarray_name
 *   Register array name.
 * @param[in] regarray_index
 *   Register index within the array (0 .. *size* - 1).
 * @param[out] value
 *   Current register value.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_regarray_read(struct rte_swx_pipeline *p,
				   const char *regarray_name,
				   uint32_t regarray_index,
				   uint64_t *value);

/**
 * Register write
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] regarray_name
 *   Register array name.
 * @param[in] regarray_index
 *   Register index within the array (0 .. *size* - 1).
 * @param[in] value
 *   Value to be written to the register.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_pipeline_regarray_write(struct rte_swx_pipeline *p,
				   const char *regarray_name,
				   uint32_t regarray_index,
				   uint64_t value);

/*
 * Meter Array Query and Configuration API.
 */

/** Meter array info. */
struct rte_swx_ctl_metarray_info {
	/** Meter array name. */
	char name[RTE_SWX_CTL_NAME_SIZE];

	/** Meter array size. */
	uint32_t size;
};

/**
 * Meter array info get
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] metarray_id
 *   Meter array ID (0 .. *n_metarrays* - 1).
 * @param[out] metarray
 *   Meter array info.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_metarray_info_get(struct rte_swx_pipeline *p,
			      uint32_t metarray_id,
			      struct rte_swx_ctl_metarray_info *metarray);

/**
 * Meter profile add
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Meter profile name.
 * @param[in] params
 *   Meter profile parameters.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -ENOMEM: Not enough space/cannot allocate memory;
 *   -EEXIST: Meter profile with this name already exists.
 */
__rte_experimental
int
rte_swx_ctl_meter_profile_add(struct rte_swx_pipeline *p,
			      const char *name,
			      struct rte_meter_trtcm_params *params);

/**
 * Meter profile delete
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] name
 *   Meter profile name.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument;
 *   -EBUSY: Meter profile is currently in use.
 */
__rte_experimental
int
rte_swx_ctl_meter_profile_delete(struct rte_swx_pipeline *p,
				 const char *name);

/**
 * Meter reset
 *
 * Reset a meter within a given meter array to use the default profile that
 * causes all the input packets to be colored as green. It is the responsibility
 * of the control plane to make sure this meter is not used by the data plane
 * pipeline before calling this function.
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] metarray_name
 *   Meter array name.
 * @param[in] metarray_index
 *   Meter index within the meter array.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_meter_reset(struct rte_swx_pipeline *p,
			const char *metarray_name,
			uint32_t metarray_index);

/**
 * Meter set
 *
 * Set a meter within a given meter array to use a specific profile. It is the
 * responsibility of the control plane to make sure this meter is not used by
 * the data plane pipeline before calling this function.
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] metarray_name
 *   Meter array name.
 * @param[in] metarray_index
 *   Meter index within the meter array.
 * @param[in] profile_name
 *   Existing meter profile name.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_meter_set(struct rte_swx_pipeline *p,
		      const char *metarray_name,
		      uint32_t metarray_index,
		      const char *profile_name);

/** Meter statistics counters. */
struct rte_swx_ctl_meter_stats {
	/** Number of packets tagged by the meter for each color. */
	uint64_t n_pkts[RTE_COLORS];

	/** Number of bytes tagged by the meter for each color. */
	uint64_t n_bytes[RTE_COLORS];
};

/**
 * Meter statistics counters read
 *
 * @param[in] p
 *   Pipeline handle.
 * @param[in] metarray_name
 *   Meter array name.
 * @param[in] metarray_index
 *   Meter index within the meter array.
 * @param[out] stats
 *   Meter statistics counters.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument.
 */
__rte_experimental
int
rte_swx_ctl_meter_stats_read(struct rte_swx_pipeline *p,
			     const char *metarray_name,
			     uint32_t metarray_index,
			     struct rte_swx_ctl_meter_stats *stats);

/**
 * Pipeline control free
 *
 * @param[in] ctl
 *   Pipeline control handle.
 */
__rte_experimental
void
rte_swx_ctl_pipeline_free(struct rte_swx_ctl_pipeline *ctl);

#ifdef __cplusplus
}
#endif

#endif
