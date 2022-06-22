/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef __INCLUDE_RTE_PORT_IN_ACTION_H__
#define __INCLUDE_RTE_PORT_IN_ACTION_H__

/**
 * @file
 * RTE Pipeline Input Port Actions
 *
 * This API provides a common set of actions for pipeline input ports to speed
 * up application development.
 *
 * Each pipeline input port can be assigned an action handler to be executed
 * on every input packet during the pipeline execution. The pipeline library
 * allows the user to define his own input port actions by providing customized
 * input port action handler. While the user can still follow this process, this
 * API is intended to provide a quicker development alternative for a set of
 * predefined actions.
 *
 * The typical steps to use this API are:
 *  - Define an input port action profile. This is a configuration template that
 *    can potentially be shared by multiple input ports from the same or
 *    different pipelines, with different input ports from the same pipeline
 *    able to use different action profiles. For every input port using a given
 *    action profile, the profile defines the set of actions and the action
 *    configuration to be executed by the input port. API functions:
 *    rte_port_in_action_profile_create(),
 *    rte_port_in_action_profile_action_register(),
 *    rte_port_in_action_profile_freeze().
 *
 *  - Instantiate the input port action profile to create input port action
 *    objects. Each pipeline input port has its own action object.
 *    API functions: rte_port_in_action_create().
 *
 *  - Use the input port action object to generate the input port action handler
 *    invoked by the pipeline. API functions:
 *    rte_port_in_action_params_get().
 *
 *  - Use the input port action object to generate the internal data structures
 *    used by the input port action handler based on given action parameters.
 *    API functions: rte_port_in_action_apply().
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_compat.h>
#include <rte_table_hash.h>

#include "rte_pipeline.h"

/** Input port actions. */
enum rte_port_in_action_type {
	/** Filter selected input packets. */
	RTE_PORT_IN_ACTION_FLTR = 0,

	/**  Load balance. */
	RTE_PORT_IN_ACTION_LB,
};

/**
 * RTE_PORT_IN_ACTION_FLTR
 */
/** Filter key size (number of bytes) */
#define RTE_PORT_IN_ACTION_FLTR_KEY_SIZE                   16

/** Filter action configuration (per action profile). */
struct rte_port_in_action_fltr_config {
	/** Key offset within the input packet buffer. Offset 0 points to the
	 * first byte of the MBUF structure.
	 */
	uint32_t key_offset;

	/** Key mask. */
	uint8_t key_mask[RTE_PORT_IN_ACTION_FLTR_KEY_SIZE];

	/** Key value. */
	uint8_t key[RTE_PORT_IN_ACTION_FLTR_KEY_SIZE];

	/** When non-zero, all the input packets that match the *key* (with the
	 * *key_mask* applied) are sent to the pipeline output port *port_id*.
	 * When zero, all the input packets that do NOT match the *key* (with
	 * *key_mask* applied) are sent to the pipeline output port *port_id*.
	 */
	int filter_on_match;

	/** Pipeline output port ID to send the filtered input packets to.
	 * Can be updated later.
	 *
	 * @see struct rte_port_in_action_fltr_params
	 */
	uint32_t port_id;
};

/** Filter action parameters (per action). */
struct rte_port_in_action_fltr_params {
	/** Pipeline output port ID to send the filtered input packets to. */
	uint32_t port_id;
};

/**
 * RTE_PORT_IN_ACTION_LB
 */
/** Load balance key size min (number of bytes). */
#define RTE_PORT_IN_ACTION_LB_KEY_SIZE_MIN                    8

/** Load balance key size max (number of bytes). */
#define RTE_PORT_IN_ACTION_LB_KEY_SIZE_MAX                    64

/** Load balance table size. */
#define RTE_PORT_IN_ACTION_LB_TABLE_SIZE                   16

/** Load balance action configuration (per action profile). */
struct rte_port_in_action_lb_config {
	/** Key size (number of bytes). */
	uint32_t key_size;

	/** Key offset within the input packet buffer. Offset 0 points to the
	 * first byte of the MBUF structure.
	 */
	uint32_t key_offset;

	/** Key mask(*key_size* bytes are valid). */
	uint8_t key_mask[RTE_PORT_IN_ACTION_LB_KEY_SIZE_MAX];

	/** Hash function. */
	rte_table_hash_op_hash f_hash;

	/** Seed value for *f_hash*. */
	uint64_t seed;

	/** Table defining the weight of each pipeline output port. The weights
	 * are set in 1/RTE_PORT_IN_ACTION_LB_TABLE_SIZE increments. To assign a
	 * weight of N/RTE_PORT_IN_ACTION_LB_TABLE_SIZE to a given output port
	 * (0 <= N <= RTE_PORT_IN_ACTION_LB_TABLE_SIZE), the output port needs
	 * to show up exactly N times in this table. Can be updated later.
	 *
	 * @see struct rte_port_in_action_lb_params
	 */
	uint32_t port_id[RTE_PORT_IN_ACTION_LB_TABLE_SIZE];
};

/** Load balance action parameters (per action). */
struct rte_port_in_action_lb_params {
	/** Table defining the weight of each pipeline output port. The weights
	 * are set in 1/RTE_PORT_IN_ACTION_LB_TABLE_SIZE increments. To assign a
	 * weight of N/RTE_PORT_IN_ACTION_LB_TABLE_SIZE to a given output port
	 * (0 <= N <= RTE_PORT_IN_ACTION_LB_TABLE_SIZE), the output port needs
	 * to show up exactly N times in this table.
	 */
	uint32_t port_id[RTE_PORT_IN_ACTION_LB_TABLE_SIZE];
};

/**
 * Input port action profile.
 */
struct rte_port_in_action_profile;

/**
 * Input port action profile create.
 *
 * @param[in] socket_id
 *   CPU socket ID for the internal data structures memory allocation.
 * @return
 *   Input port action profile handle on success, NULL otherwise.
 */
__rte_experimental
struct rte_port_in_action_profile *
rte_port_in_action_profile_create(uint32_t socket_id);

/**
 * Input port action profile free.
 *
 * @param[in] profile
 *   Input port action profile handle (needs to be valid).
 *   If profile is NULL, no operation is performed.
 * @return
 *   Always zero.
 */
__rte_experimental
int
rte_port_in_action_profile_free(struct rte_port_in_action_profile *profile);

/**
 * Input port action profile action register.
 *
 * @param[in] profile
 *   Input port action profile handle (needs to be valid and not in frozen
 *   state).
 * @param[in] type
 *   Specific input port action to be registered for *profile*.
 * @param[in] action_config
 *   Configuration for the *type* action.
 *   If struct rte_port_in_action_*type*_config is defined, it needs to point to
 *   a valid instance of this structure, otherwise it needs to be set to NULL.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_port_in_action_profile_action_register(
	struct rte_port_in_action_profile *profile,
	enum rte_port_in_action_type type,
	void *action_config);

/**
 * Input port action profile freeze.
 *
 * Once this function is called successfully, the given profile enters the
 * frozen state with the following immediate effects: no more actions can be
 * registered for this profile, so the profile can be instantiated to create
 * input port action objects.
 *
 * @param[in] profile
 *   Input port profile action handle (needs to be valid and not in frozen
 *   state).
 * @return
 *   Zero on success, non-zero error code otherwise.
 *
 * @see rte_port_in_action_create()
 */
__rte_experimental
int
rte_port_in_action_profile_freeze(struct rte_port_in_action_profile *profile);

/**
 * Input port action.
 */
struct rte_port_in_action;

/**
 * Input port action create.
 *
 * Instantiates the given input port action profile to create an input port
 * action object.
 *
 * @param[in] profile
 *   Input port profile action handle (needs to be valid and in frozen state).
 * @param[in] socket_id
 *   CPU socket ID where the internal data structures required by the new input
 *   port action object should be allocated.
 * @return
 *   Handle to input port action object on success, NULL on error.
 */
__rte_experimental
struct rte_port_in_action *
rte_port_in_action_create(struct rte_port_in_action_profile *profile,
	uint32_t socket_id);

/**
 * Input port action free.
 *
 * @param[in] action
 *   Handle to input port action object (needs to be valid).
 *   If action is NULL, no operation is performed.
 * @return
 *   Always zero.
 */
__rte_experimental
int
rte_port_in_action_free(struct rte_port_in_action *action);

/**
 * Input port params get.
 *
 * @param[in] action
 *   Handle to input port action object (needs to be valid).
 * @param[inout] params
 *   Pipeline input port parameters (needs to be pre-allocated).
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_port_in_action_params_get(struct rte_port_in_action *action,
	struct rte_pipeline_port_in_params *params);

/**
 * Input port action apply.
 *
 * @param[in] action
 *   Handle to input port action object (needs to be valid).
 * @param[in] type
 *   Specific input port action previously registered for the input port action
 *   profile of the *action* object.
 * @param[in] action_params
 *   Parameters for the *type* action.
 *   If struct rte_port_in_action_*type*_params is defined, it needs to point to
 *   a valid instance of this structure, otherwise it needs to be set to NULL.
 * @return
 *   Zero on success, non-zero error code otherwise.
 */
__rte_experimental
int
rte_port_in_action_apply(struct rte_port_in_action *action,
	enum rte_port_in_action_type type,
	void *action_params);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_PORT_IN_ACTION_H__ */
