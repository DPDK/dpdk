/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef __INCLUDE_RTE_TABLE_ACTION_H__
#define __INCLUDE_RTE_TABLE_ACTION_H__

/**
 * @file
 * RTE Pipeline Table Actions
 *
 * This API provides a common set of actions for pipeline tables to speed up
 * application development.
 *
 * Each match-action rule added to a pipeline table has associated data that
 * stores the action context. This data is input to the table action handler
 * called for every input packet that hits the rule as part of the table lookup
 * during the pipeline execution. The pipeline library allows the user to define
 * his own table actions by providing customized table action handlers (table
 * lookup) and complete freedom of setting the rules and their data (table rule
 * add/delete). While the user can still follow this process, this API is
 * intended to provide a quicker development alternative for a set of predefined
 * actions.
 *
 * The typical steps to use this API are:
 *  - Define a table action profile. This is a configuration template that can
 *    potentially be shared by multiple tables from the same or different
 *    pipelines, with different tables from the same pipeline likely to use
 *    different action profiles. For every table using a given action profile,
 *    the profile defines the set of actions and the action configuration to be
 *    implemented for all the table rules. API functions:
 *    rte_table_action_profile_create(),
 *    rte_table_action_profile_action_register(),
 *    rte_table_action_profile_freeze().
 *
 *  - Instantiate the table action profile to create table action objects. Each
 *    pipeline table has its own table action object. API functions:
 *    rte_table_action_create().
 *
 *  - Use the table action object to generate the pipeline table action handlers
 *    (invoked by the pipeline table lookup operation). API functions:
 *    rte_table_action_table_params_get().
 *
 *  - Use the table action object to generate the rule data (for the pipeline
 *    table rule add operation) based on given action parameters. API functions:
 *    rte_table_action_apply().
 *
 *  - Use the table action object to read action data (e.g. stats counters) for
 *    any given rule. API functions: rte_table_action_XYZ_read().
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_compat.h>
#include <rte_meter.h>

#include "rte_pipeline.h"

/** Table actions. */
enum rte_table_action_type {
	/** Forward to next pipeline table, output port or drop. */
	RTE_TABLE_ACTION_FWD = 0,

	/**  Traffic Metering and Policing. */
	RTE_TABLE_ACTION_MTR,
};

/** Common action configuration (per table action profile). */
struct rte_table_action_common_config {
	/** Input packet Internet Protocol (IP) version. Non-zero for IPv4, zero
	 * for IPv6.
	 */
	int ip_version;

	/** IP header offset within the input packet buffer. Offset 0 points to
	 * the first byte of the MBUF structure.
	 */
	uint32_t ip_offset;
};

/**
 * RTE_TABLE_ACTION_FWD
 */
/** Forward action parameters (per table rule). */
struct rte_table_action_fwd_params {
	/** Forward action. */
	enum rte_pipeline_action action;

	/** Pipeline table ID or output port ID. */
	uint32_t id;
};

/**
 * RTE_TABLE_ACTION_MTR
 */
/** Max number of traffic classes (TCs). */
#define RTE_TABLE_ACTION_TC_MAX                                  4

/** Max number of queues per traffic class. */
#define RTE_TABLE_ACTION_TC_QUEUE_MAX                            4

/** Differentiated Services Code Point (DSCP) translation table entry. */
struct rte_table_action_dscp_table_entry {
	/** Traffic class. Used by the meter or the traffic management actions.
	 * Has to be strictly smaller than *RTE_TABLE_ACTION_TC_MAX*. Traffic
	 * class 0 is the highest priority.
	 */
	uint32_t tc_id;

	/** Traffic class queue. Used by the traffic management action. Has to
	 * be strictly smaller than *RTE_TABLE_ACTION_TC_QUEUE_MAX*.
	 */
	uint32_t tc_queue_id;

	/** Packet color. Used by the meter action as the packet input color
	 * for the color aware mode of the traffic metering algorithm.
	 */
	enum rte_meter_color color;
};

/** DSCP translation table. */
struct rte_table_action_dscp_table {
	/** Array of DSCP table entries */
	struct rte_table_action_dscp_table_entry entry[64];
};

/** Supported traffic metering algorithms. */
enum rte_table_action_meter_algorithm {
	/** Single Rate Three Color Marker (srTCM) - IETF RFC 2697. */
	RTE_TABLE_ACTION_METER_SRTCM,

	/** Two Rate Three Color Marker (trTCM) - IETF RFC 2698. */
	RTE_TABLE_ACTION_METER_TRTCM,
};

/** Traffic metering profile (configuration template). */
struct rte_table_action_meter_profile {
	/** Traffic metering algorithm. */
	enum rte_table_action_meter_algorithm alg;

	RTE_STD_C11
	union {
		/** Only valid when *alg* is set to srTCM - IETF RFC 2697. */
		struct rte_meter_srtcm_params srtcm;

		/** Only valid when *alg* is set to trTCM - IETF RFC 2698. */
		struct rte_meter_trtcm_params trtcm;
	};
};

/** Policer actions. */
enum rte_table_action_policer {
	/** Recolor the packet as green. */
	RTE_TABLE_ACTION_POLICER_COLOR_GREEN = 0,

	/** Recolor the packet as yellow. */
	RTE_TABLE_ACTION_POLICER_COLOR_YELLOW,

	/** Recolor the packet as red. */
	RTE_TABLE_ACTION_POLICER_COLOR_RED,

	/** Drop the packet. */
	RTE_TABLE_ACTION_POLICER_DROP,

	/** Number of policer actions. */
	RTE_TABLE_ACTION_POLICER_MAX
};

/** Meter action configuration per traffic class. */
struct rte_table_action_mtr_tc_params {
	/** Meter profile ID. */
	uint32_t meter_profile_id;

	/** Policer actions. */
	enum rte_table_action_policer policer[e_RTE_METER_COLORS];
};

/** Meter action statistics counters per traffic class. */
struct rte_table_action_mtr_counters_tc {
	/** Number of packets per color at the output of the traffic metering
	 * and before the policer actions are executed. Only valid when
	 * *n_packets_valid* is non-zero.
	 */
	uint64_t n_packets[e_RTE_METER_COLORS];

	/** Number of packet bytes per color at the output of the traffic
	 * metering and before the policer actions are executed. Only valid when
	 * *n_bytes_valid* is non-zero.
	 */
	uint64_t n_bytes[e_RTE_METER_COLORS];

	/** When non-zero, the *n_packets* field is valid. */
	int n_packets_valid;

	/** When non-zero, the *n_bytes* field is valid. */
	int n_bytes_valid;
};

/** Meter action configuration (per table action profile). */
struct rte_table_action_mtr_config {
	/** Meter algorithm. */
	enum rte_table_action_meter_algorithm alg;

	/** Number of traffic classes. Each traffic class has its own traffic
	 * meter and policer instances. Needs to be equal to either 1 or to
	 * *RTE_TABLE_ACTION_TC_MAX*.
	 */
	uint32_t n_tc;

	/** When non-zero, the *n_packets* meter stats counter is enabled,
	 * otherwise it is disabled.
	 *
	 * @see struct rte_table_action_mtr_counters_tc
	 */
	int n_packets_enabled;

	/** When non-zero, the *n_bytes* meter stats counter is enabled,
	 * otherwise it is disabled.
	 *
	 * @see struct rte_table_action_mtr_counters_tc
	 */
	int n_bytes_enabled;
};

/** Meter action parameters (per table rule). */
struct rte_table_action_mtr_params {
	/** Traffic meter and policer parameters for each of the *tc_mask*
	 * traffic classes.
	 */
	struct rte_table_action_mtr_tc_params mtr[RTE_TABLE_ACTION_TC_MAX];

	/** Bit mask defining which traffic class parameters are valid in *mtr*.
	 * If bit N is set in *tc_mask*, then parameters for traffic class N are
	 * valid in *mtr*.
	 */
	uint32_t tc_mask;
};

/** Meter action statistics counters (per table rule). */
struct rte_table_action_mtr_counters {
	/** Stats counters for each of the *tc_mask* traffic classes. */
	struct rte_table_action_mtr_counters_tc stats[RTE_TABLE_ACTION_TC_MAX];

	/** Bit mask defining which traffic class parameters are valid in *mtr*.
	 * If bit N is set in *tc_mask*, then parameters for traffic class N are
	 * valid in *mtr*.
	 */
	uint32_t tc_mask;
};

/**
 * Table action profile.
 */
struct rte_table_action_profile;

/**
 * Table action profile create.
 *
 * @param[in] common
 *   Common action configuration.
 * @return
 *   Table action profile handle on success, NULL otherwise.
 */
struct rte_table_action_profile * __rte_experimental
rte_table_action_profile_create(struct rte_table_action_common_config *common);

/**
 * Table action profile free.
 *
 * @param[in] profile
 *   Table profile action handle (needs to be valid).
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_table_action_profile_free(struct rte_table_action_profile *profile);

/**
 * Table action profile action register.
 *
 * @param[in] profile
 *   Table profile action handle (needs to be valid and not in frozen state).
 * @param[in] type
 *   Specific table action to be registered for *profile*.
 * @param[in] action_config
 *   Configuration for the *type* action.
 *   If struct rte_table_action_*type*_config is defined by the Table Action
 *   API, it needs to point to a valid instance of this structure, otherwise it
 *   needs to be set to NULL.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_table_action_profile_action_register(struct rte_table_action_profile *profile,
	enum rte_table_action_type type,
	void *action_config);

/**
 * Table action profile freeze.
 *
 * Once this function is called successfully, the given profile enters the
 * frozen state with the following immediate effects: no more actions can be
 * registered for this profile, so the profile can be instantiated to create
 * table action objects.
 *
 * @param[in] profile
 *   Table profile action handle (needs to be valid and not in frozen state).
 * @return
 *   Zero on success, non-zero error code otherwise.
 *
 * @see rte_table_action_create()
 */
int __rte_experimental
rte_table_action_profile_freeze(struct rte_table_action_profile *profile);

/**
 * Table action.
 */
struct rte_table_action;

/**
 * Table action create.
 *
 * Instantiates the given table action profile to create a table action object.
 *
 * @param[in] profile
 *   Table profile action handle (needs to be valid and in frozen state).
 * @param[in] socket_id
 *   CPU socket ID where the internal data structures required by the new table
 *   action object should be allocated.
 * @return
 *   Handle to table action object on success, NULL on error.
 *
 * @see rte_table_action_create()
 */
struct rte_table_action * __rte_experimental
rte_table_action_create(struct rte_table_action_profile *profile,
	uint32_t socket_id);

/**
 * Table action free.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_table_action_free(struct rte_table_action *action);

/**
 * Table action table params get.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[inout] params
 *   Pipeline table parameters (needs to be pre-allocated).
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_table_action_table_params_get(struct rte_table_action *action,
	struct rte_pipeline_table_params *params);

/**
 * Table action apply.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] data
 *   Data byte array (typically table rule data) to apply action *type* on.
 * @param[in] type
 *   Specific table action previously registered for the table action profile of
 *   the *action* object.
 * @param[in] action_params
 *   Parameters for the *type* action.
 *   If struct rte_table_action_*type*_params is defined by the Table Action
 *   API, it needs to point to a valid instance of this structure, otherwise it
 *   needs to be set to NULL.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_table_action_apply(struct rte_table_action *action,
	void *data,
	enum rte_table_action_type type,
	void *action_params);

/**
 * Table action DSCP table update.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] dscp_mask
 *   64-bit mask defining the DSCP table entries to be updated. If bit N is set
 *   in this bit mask, then DSCP table entry N is to be updated, otherwise not.
 * @param[in] table
 *   DSCP table.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_table_action_dscp_table_update(struct rte_table_action *action,
	uint64_t dscp_mask,
	struct rte_table_action_dscp_table *table);

/**
 * Table action meter profile add.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] meter_profile_id
 *   Meter profile ID to be used for the *profile* once it is successfully added
 *   to the *action* object (needs to be unused by the set of meter profiles
 *   currently registered for the *action* object).
 * @param[in] profile
 *   Meter profile to be added.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_table_action_meter_profile_add(struct rte_table_action *action,
	uint32_t meter_profile_id,
	struct rte_table_action_meter_profile *profile);

/**
 * Table action meter profile delete.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] meter_profile_id
 *   Meter profile ID of the meter profile to be deleted from the *action*
 *   object (needs to be valid for the *action* object).
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_table_action_meter_profile_delete(struct rte_table_action *action,
	uint32_t meter_profile_id);

/**
 * Table action meter read.
 *
 * @param[in] action
 *   Handle to table action object (needs to be valid).
 * @param[in] data
 *   Data byte array (typically table rule data) with meter action previously
 *   applied on it.
 * @param[in] tc_mask
 *   Bit mask defining which traffic classes should have the meter stats
 *   counters read from *data* and stored into *stats*. If bit N is set in this
 *   bit mask, then traffic class N is part of this operation, otherwise it is
 *   not. If bit N is set in this bit mask, then traffic class N must be one of
 *   the traffic classes that are enabled for the meter action in the table
 *   action profile used by the *action* object.
 * @param[inout] stats
 *   When non-NULL, it points to the area where the meter stats counters read
 *   from *data* are saved. Only the meter stats counters for the *tc_mask*
 *   traffic classes are read and stored to *stats*.
 * @param[in] clear
 *   When non-zero, the meter stats counters are cleared (i.e. set to zero),
 *   otherwise the counters are not modified. When the read operation is enabled
 *   (*stats* is non-NULL), the clear operation is performed after the read
 *   operation is completed.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
int __rte_experimental
rte_table_action_meter_read(struct rte_table_action *action,
	void *data,
	uint32_t tc_mask,
	struct rte_table_action_mtr_counters *stats,
	int clear);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_TABLE_ACTION_H__ */
