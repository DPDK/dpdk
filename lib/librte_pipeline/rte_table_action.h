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

#include "rte_pipeline.h"

/** Table actions. */
enum rte_table_action_type {
	/** Forward to next pipeline table, output port or drop. */
	RTE_TABLE_ACTION_FWD = 0,
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

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_TABLE_ACTION_H__ */
