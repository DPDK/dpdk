/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_TABLE_H__
#define __INCLUDE_RTE_SWX_TABLE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Table
 *
 * Table interface.
 */

#include <stdint.h>

#include <rte_os.h>

/** Match type. */
enum rte_swx_table_match_type {
	/** Wildcard Match (WM). */
	RTE_SWX_TABLE_MATCH_WILDCARD,

	/** Longest Prefix Match (LPM). */
	RTE_SWX_TABLE_MATCH_LPM,

	/** Exact Match (EM). */
	RTE_SWX_TABLE_MATCH_EXACT,
};

/** Table creation parameters. */
struct rte_swx_table_params {
	/** Table match type. */
	enum rte_swx_table_match_type match_type;

	/** Key size in bytes. */
	uint32_t key_size;

	/** Offset of the first byte of the key within the key buffer. */
	uint32_t key_offset;

	/** Mask of *key_size* bytes logically laid over the bytes at positions
	 * *key_offset* .. (*key_offset* + *key_size* - 1) of the key buffer in
	 * order to specify which bits from the key buffer are part of the key
	 * and which ones are not. A bit value of 1 in the *key_mask0* means the
	 * respective bit in the key buffer is part of the key, while a bit
	 * value of 0 means the opposite. A NULL value means that all the bits
	 * are part of the key, i.e. the *key_mask0* is an all-ones mask.
	 */
	uint8_t *key_mask0;

	/** Maximum size (in bytes) of the action data. The data stored in the
	 * table for each entry is equal to *action_data_size* plus 8 bytes,
	 * which are used to store the action ID.
	 */
	uint32_t action_data_size;

	/** Maximum number of keys to be stored in the table together with their
	 * associated data.
	 */
	uint32_t n_keys_max;
};

/** Table entry. */
struct rte_swx_table_entry {
	/** Used to facilitate the membership of this table entry to a
	 * linked list.
	 */
	RTE_TAILQ_ENTRY(rte_swx_table_entry) node;

	/** Key value for the current entry. Array of *key_size* bytes or NULL
	 * if the *key_size* for the current table is 0.
	 */
	uint8_t *key;

	/** Key mask for the current entry. Array of *key_size* bytes that is
	 * logically and'ed with *key_mask0* of the current table. A NULL value
	 * means that all the key bits already enabled by *key_mask0* are part
	 * of the key of the current entry.
	 */
	uint8_t *key_mask;

	/** Placeholder for a possible compressed version of the *key* and
	 * *key_mask* of the current entry. Typically a hash signature, its main
	 * purpose is to the linked list search operation. Should be ignored by
	 * the API functions below.
	 */
	uint64_t key_signature;

	/** Key priority for the current entry. Useful for wildcard match (as
	 * match rules are commonly overlapping with other rules), ignored for
	 * exact match (as match rules never overlap, hence all rules have the
	 * same match priority) and for LPM (match priority is driven by the
	 * prefix length, with non-overlapping prefixes essentially having the
	 * same match priority). Value 0 indicates the highest match priority.
	 */
	uint32_t key_priority;

	/** Action ID for the current entry. */
	uint64_t action_id;

	/** Action data for the current entry. Considering S as the action data
	 * size of the *action_id* action, which must be less than or equal to
	 * the table *action_data_size*, the *action_data* field must point to
	 * an array of S bytes when S is non-zero. The *action_data* field is
	 * ignored when S is zero.
	 */
	uint8_t *action_data;
};

/** List of table entries. */
RTE_TAILQ_HEAD(rte_swx_table_entry_list, rte_swx_table_entry);

/**
 * Table memory footprint get
 *
 * @param[in] params
 *   Table create parameters.
 * @param[in] entries
 *   Table entries.
 * @param[in] args
 *   Any additional table create arguments. It may be NULL.
 * @return
 *   Table memory footprint in bytes, if successful, or zero, on error.
 */
typedef uint64_t
(*rte_swx_table_footprint_get_t)(struct rte_swx_table_params *params,
				 struct rte_swx_table_entry_list *entries,
				 const char *args);

/**
 * Table mailbox size get
 *
 * The mailbox is used to store the context of a lookup operation that is in
 * progress and it is passed as a parameter to the lookup operation. This allows
 * for multiple concurrent lookup operations into the same table.
 *
 * @return
 *   Table memory footprint in bytes, on success, or zero, on error.
 */
typedef uint64_t
(*rte_swx_table_mailbox_size_get_t)(void);

/**
 * Table create
 *
 * @param[in] params
 *   Table creation parameters.
 * @param[in] entries
 *   Entries to be added to the table at creation time.
 * @param[in] args
 *   Any additional table create arguments. It may be NULL.
 * @param[in] numa_node
 *   Non-Uniform Memory Access (NUMA) node.
 * @return
 *   Table handle, on success, or NULL, on error.
 */
typedef void *
(*rte_swx_table_create_t)(struct rte_swx_table_params *params,
			  struct rte_swx_table_entry_list *entries,
			  const char *args,
			  int numa_node);

/**
 * Table entry add
 *
 * @param[in] table
 *   Table handle.
 * @param[in] entry
 *   Entry to be added to the table.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid table handle, entry or entry field;
 *   -ENOSPC: Table full.
 */
typedef int
(*rte_swx_table_add_t)(void *table,
		       struct rte_swx_table_entry *entry);

/**
 * Table entry delete
 *
 * @param[in] table
 *   Table handle.
 * @param[in] entry
 *   Entry to be deleted from the table. The entry *action_id* and *action_data*
 *   fields are ignored.
 * @return
 *   0 on success or the following error codes otherwise:
 *   -EINVAL: Invalid table handle, entry or entry field;
 *   -ENOSPC: Table full.
 */
typedef int
(*rte_swx_table_delete_t)(void *table,
			  struct rte_swx_table_entry *entry);

/**
 * Table lookup
 *
 * The table lookup operation searches a given key in the table and upon its
 * completion it returns an indication of whether the key is found in the table
 * (lookup hit) or not (lookup miss). In case of lookup hit, the action_id and
 * the action_data associated with the key are also returned.
 *
 * Multiple invocations of this function may be required in order to complete a
 * single table lookup operation for a given table and a given lookup key. The
 * completion of the table lookup operation is flagged by a return value of 1;
 * in case of a return value of 0, the function must be invoked again with
 * exactly the same arguments.
 *
 * The mailbox argument is used to store the context of an on-going table lookup
 * operation. The mailbox mechanism allows for multiple concurrent table lookup
 * operations into the same table.
 *
 * The typical reason an implementation may choose to split the table lookup
 * operation into multiple steps is to hide the latency of the inherent memory
 * read operations: before a read operation with the source data likely not in
 * the CPU cache, the source data prefetch is issued and the table lookup
 * operation is postponed in favor of some other unrelated work, which the CPU
 * executes in parallel with the source data being fetched into the CPU cache;
 * later on, the table lookup operation is resumed, this time with the source
 * data likely to be read from the CPU cache with no CPU pipeline stall, which
 * significantly improves the table lookup performance.
 *
 * @param[in] table
 *   Table handle.
 * @param[in] mailbox
 *   Mailbox for the current table lookup operation.
 * @param[in] key
 *   Lookup key. Its size mult be equal to the table *key_size*. If the latter
 *   is zero, then the lookup key must be NULL.
 * @param[out] action_id
 *   ID of the action associated with the *key*. Must point to a valid 64-bit
 *   variable. Only valid when the function returns 1 and *hit* is set to true.
 * @param[out] action_data
 *   Action data for the *action_id* action. Must point to a valid array of
 *   table *action_data_size* bytes. Only valid when the function returns 1 and
 *   *hit* is set to true.
 * @param[out] hit
 *   Only valid when the function returns 1. Set to non-zero (true) on table
 *   lookup hit and to zero (false) on table lookup miss.
 * @return
 *   0 when the table lookup operation is not yet completed, and 1 when the
 *   table lookup operation is completed. No other return values are allowed.
 */
typedef int
(*rte_swx_table_lookup_t)(void *table,
			  void *mailbox,
			  uint8_t **key,
			  uint64_t *action_id,
			  uint8_t **action_data,
			  int *hit);

/**
 * Table free
 *
 * @param[in] table
 *   Table handle.
 */
typedef void
(*rte_swx_table_free_t)(void *table);

/** Table operations.  */
struct rte_swx_table_ops {
	/** Table memory footprint get. Set to NULL when not supported. */
	rte_swx_table_footprint_get_t footprint_get;

	/** Table mailbox size get. When NULL, the mailbox size is 0. */
	rte_swx_table_mailbox_size_get_t mailbox_size_get;

	/** Table create. Must be non-NULL. */
	rte_swx_table_create_t create;

	/** Incremental table entry add. Set to NULL when not supported, in
	 * which case the existing table has to be destroyed and a new table
	 * built from scratch with the new entry included.
	 */
	rte_swx_table_add_t add;

	/** Incremental table entry delete. Set to NULL when not supported, in
	 * which case the existing table has to be destroyed and a new table
	 * built from scratch with the entry excluded.
	 */
	rte_swx_table_delete_t del;

	/** Table lookup. Must be non-NULL. */
	rte_swx_table_lookup_t lkp;

	/** Table free. Must be non-NULL. */
	rte_swx_table_free_t free;
};

#ifdef __cplusplus
}
#endif

#endif
