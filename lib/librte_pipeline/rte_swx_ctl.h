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

#include <stddef.h>
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
	 * action. Array of pipeline *n_actions* elements indedex by the
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
