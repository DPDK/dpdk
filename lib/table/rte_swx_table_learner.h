/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_TABLE_LEARNER_H__
#define __INCLUDE_RTE_SWX_TABLE_LEARNER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Learner Table
 *
 * The learner table API.
 *
 * This table type is typically used for learning or connection tracking, where it allows for the
 * implementation of the "add on miss" scenario: whenever the lookup key is not found in the table
 * (lookup miss), the data plane can decide to add this key to the table with a given action with no
 * control plane intervention. Likewise, the table keys expire based on a configurable timeout and
 * are thus automatically removed from the table with no control plane intervention.
 *
 * The keys are not automatically rearmed on lookup hit. To delay the key expiration, the key timer
 * has to be explicitly reinitialized on lookup hit. The key will be kept in the table as long as it
 * is frequently hit and explicitly rearmed on every hit.
 *
 * Operation overview:
 * 1) Lookup miss:
 *      a) add: Add the current input key (the key that missed the lookup) to the table with given
 *              action, action parameters and expiration timeout. This is the way to populate the
 *              table (which is empty initially). Data plane operation.
 *      b) Do nothing: Keep the current input key out of the table.
 * 2) Lookup hit:
 *      a) add: Update the action, action parameters and/or the expiration timeout for the current
 *              input key, which is already in the table. The expiration timer of the key is
 *              automatically rearmed. Data plane operation.
 *      b) rearm: Rearm the expiration timer for the current input key, which is already in the
 *              table. The timeout value used for the expiration timer is either the same as the one
 *              currently associated with the key or a new one can be provided as input. Data plane
 *              operation.
 *      c) delete: Delete the current input key from the table. The purpose of this operation is to
 *              force the deletion of the key from the table before the key expires on timeout due
 *              to inactivity. Data plane operation.
 *      d) Do nothing: Keep the expiration timer of the current input key running down. This key
 *              will thus expire naturally, unless it is hit again as part of a subsequent lookup
 *              operation, when the key timer can be rearmed or re-added to prolong its life.
 */

#include <stdint.h>
#include <sys/types.h>

#include <rte_compat.h>

#include "rte_swx_hash_func.h"

/** Maximum number of key timeout values per learner table. */
#ifndef RTE_SWX_TABLE_LEARNER_N_KEY_TIMEOUTS_MAX
#define RTE_SWX_TABLE_LEARNER_N_KEY_TIMEOUTS_MAX 16
#endif

/** Learner table creation parameters. */
struct rte_swx_table_learner_params {
	/** Key size in bytes. Must be non-zero. */
	uint32_t key_size;

	/** Offset of the first byte of the key within the key buffer. */
	uint32_t key_offset;

	/** Mask of *key_size* bytes logically laid over the bytes at positions
	 * *key_offset* .. (*key_offset* + *key_size* - 1) of the key buffer in order to specify
	 * which bits from the key buffer are part of the key and which ones are not. A bit value of
	 * 1 in the *key_mask0* means the respective bit in the key buffer is part of the key, while
	 * a bit value of 0 means the opposite. A NULL value means that all the bits are part of the
	 * key, i.e. the *key_mask0* is an all-ones mask.
	 */
	uint8_t *key_mask0;

	/** Maximum size (in bytes) of the action data. The data stored in the table for each entry
	 * is equal to *action_data_size* plus 8 bytes, which are used to store the action ID.
	 */
	uint32_t action_data_size;

	/** Hash function. When NULL, the default hash function will be used.
	 */
	rte_swx_hash_func_t hash_func;

	/** Maximum number of keys to be stored in the table together with their associated data. */
	uint32_t n_keys_max;

	/** The set of all possible key timeout values measured in seconds. Each value must be
	 * non-zero. Each table key expires and is automatically deleted from the table after
	 * this many seconds.
	 */
	uint32_t *key_timeout;

	/** Number of possible key timeout values present in the *key_timeout* set. It must be less
	 * than or equal to *RTE_SWX_TABLE_LEARNER_N_KEY_TIMEOUTS_MAX*.
	 */
	uint32_t n_key_timeouts;
};

/**
 * Learner table memory footprint get
 *
 * @param[in] params
 *   Table create parameters.
 * @return
 *   Table memory footprint in bytes.
 */
__rte_experimental
uint64_t
rte_swx_table_learner_footprint_get(struct rte_swx_table_learner_params *params);

/**
 * Learner table mailbox size get
 *
 * The mailbox is used to store the context of a lookup operation that is in
 * progress and it is passed as a parameter to the lookup operation. This allows
 * for multiple concurrent lookup operations into the same table.
 *
 * @return
 *   Table mailbox footprint in bytes.
 */
__rte_experimental
uint64_t
rte_swx_table_learner_mailbox_size_get(void);

/**
 * Learner table create
 *
 * @param[in] params
 *   Table creation parameters.
 * @param[in] numa_node
 *   Non-Uniform Memory Access (NUMA) node.
 * @return
 *   Table handle, on success, or NULL, on error.
 */
__rte_experimental
void *
rte_swx_table_learner_create(struct rte_swx_table_learner_params *params, int numa_node);

/**
 * Learner table key timeout update
 *
 * @param[in] table
 *   Table handle.
 * @param[in] key_timeout_id
 *   Key timeout ID. Must be less than the configured *n_key_timeouts* value.
 * @param[in] key_timeout
 *   Key timeout value measured in seconds.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid argument(s).
 */
__rte_experimental
int
rte_swx_table_learner_timeout_update(void *table,
				     uint32_t key_timeout_id,
				     uint32_t key_timeout);

/**
 * Learner table key lookup
 *
 * The table lookup operation searches a given key in the table and upon its completion it returns
 * an indication of whether the key is found in the table (lookup hit) or not (lookup miss). In case
 * of lookup hit, the action_id and the action_data associated with the key are also returned.
 *
 * Multiple invocations of this function may be required in order to complete a single table lookup
 * operation for a given table and a given lookup key. The completion of the table lookup operation
 * is flagged by a return value of 1; in case of a return value of 0, the function must be invoked
 * again with exactly the same arguments.
 *
 * The mailbox argument is used to store the context of an on-going table key lookup operation, and
 * possibly an associated key add operation. The mailbox mechanism allows for multiple concurrent
 * table key lookup and add operations into the same table.
 *
 * The table entry consists of the action ID and the action data. Each table entry is unique, even
 * though different table entries can have identical content, i.e. same values for the action ID and
 * the action data. The table entry ID is also returned by the table lookup operation. It can be
 * used to index into an external array of resources such as counters, registers or meters to
 * identify the resource directly associated with the current table entry with no need to store the
 * corresponding index into the table entry. The index of the external resource is thus
 * auto-generated instead of being stored in the table entry.
 *
 * @param[in] table
 *   Table handle.
 * @param[in] mailbox
 *   Mailbox for the current table lookup operation.
 * @param[in] time
 *   Current time measured in CPU clock cycles.
 * @param[in] key
 *   Lookup key. Its size must be equal to the table *key_size*.
 * @param[out] action_id
 *   ID of the action associated with the *key*. Must point to a valid 64-bit variable. Only valid
 *   when the function returns 1 and *hit* is set to true.
 * @param[out] action_data
 *   Action data for the *action_id* action. Must point to a valid array of table *action_data_size*
 *   bytes. Only valid when the function returns 1 and *hit* is set to true.
 * @param[out] entry_id
 *   Table entry unique ID. Must point to a valid 32-bit variable. Only valid when the function
 *   returns 1 and *hit* is set to true.
 * @param[out] hit
 *   Only valid when the function returns 1. Set to non-zero (true) on table lookup hit and to zero
 *   (false) on table lookup miss.
 * @return
 *   0 when the table lookup operation is not yet completed, and 1 when the table lookup operation
 *   is completed. No other return values are allowed.
 */
__rte_experimental
int
rte_swx_table_learner_lookup(void *table,
			     void *mailbox,
			     uint64_t time,
			     uint8_t **key,
			     uint64_t *action_id,
			     uint8_t **action_data,
			     size_t *entry_id,
			     int *hit);

/**
 * Learner table key add
 *
 * This operation takes the latest key that was looked up in the table and adds it to the table with
 * the given action ID and action data. Typically, this operation is only invoked when the latest
 * lookup operation in the current table resulted in lookup miss.
 *
 * @param[in] table
 *   Table handle.
 * @param[in] mailbox
 *   Mailbox for the current operation.
 * @param[in] time
 *   Current time measured in CPU clock cycles.
 * @param[out] action_id
 *   ID of the action associated with the key.
 * @param[out] action_data
 *   Action data for the *action_id* action.
 * @param[in] key_timeout_id
 *   Key timeout ID.
 * @return
 *   0 on success, 1 or error (table full).
 */
__rte_experimental
uint32_t
rte_swx_table_learner_add(void *table,
			  void *mailbox,
			  uint64_t time,
			  uint64_t action_id,
			  uint8_t *action_data,
			  uint32_t key_timeout_id);

/**
 * Learner table key rearm with same timeout value
 *
 * This operation takes the latest key that was looked up in the table and, in case of lookup hit,
 * it rearms its expiration timer using the same timeout value currently associated with the key.
 *
 * @param[in] table
 *   Table handle.
 * @param[in] mailbox
 *   Mailbox for the current operation.
 * @param[in] time
 *   Current time measured in CPU clock cycles.
 */
__rte_experimental
void
rte_swx_table_learner_rearm(void *table,
			    void *mailbox,
			    uint64_t time);

/**
 * Learner table key rearm with given timeout value
 *
 * This operation takes the latest key that was looked up in the table and, in case of lookup hit,
 * it rearms its expiration timer using the given timeout value.
 *
 * @param[in] table
 *   Table handle.
 * @param[in] mailbox
 *   Mailbox for the current operation.
 * @param[in] time
 *   Current time measured in CPU clock cycles.
 * @param[in] key_timeout_id
 *   Key timeout ID.
 */
__rte_experimental
void
rte_swx_table_learner_rearm_new(void *table,
				void *mailbox,
				uint64_t time,
				uint32_t key_timeout_id);

/**
 * Learner table key delete
 *
 * This operation takes the latest key that was looked up in the table and deletes it from the
 * table. Typically, this operation is only invoked to force the deletion of the key before the key
 * expires on timeout due to inactivity.
 *
 * @param[in] table
 *   Table handle.
 * @param[in] mailbox
 *   Mailbox for the current operation.
 */
__rte_experimental
void
rte_swx_table_learner_delete(void *table,
			     void *mailbox);

/**
 * Learner table free
 *
 * @param[in] table
 *   Table handle.
 */
__rte_experimental
void
rte_swx_table_learner_free(void *table);

#ifdef __cplusplus
}
#endif

#endif
