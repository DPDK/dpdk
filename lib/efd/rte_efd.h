/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef _RTE_EFD_H_
#define _RTE_EFD_H_

/**
 * @file
 *
 * RTE EFD Table
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************
 * User selectable constants
 *************************************************************************/

/*
 * If possible, best lookup performance will be achieved by ensuring that
 * the entire table fits in the L3 cache.
 *
 * Some formulas for calculating various sizes are listed below:
 *
 * # of chunks =
 *   2 ^ (ceiling(log2((requested # of rules) /
 *            (EFD_CHUNK_NUM_GROUPS * EFD_TARGET_GROUP_NUM_RULES))))
 *
 * Target # of rules = (# of chunks) * EFD_CHUNK_NUM_GROUPS *
 *            EFD_TARGET_GROUP_NUM_RULES
 *
 * Group Size (in bytes) = 4 (per value bit)
 *
 * Table size (in bytes) = RTE_EFD_VALUE_NUM_BITS * (# of chunks) *
 *            EFD_CHUNK_NUM_GROUPS * (group size)
 */

/**
 * !!! This parameter should be adjusted for your application !!!
 *
 * This parameter adjusts the number of bits of value that can be
 * stored in the table.
 * For example, setting the number of bits to 3 will allow storing 8 values
 * in the table (between 0 and 7).
 *
 * This number directly affects the performance of both lookups and insertion.
 * In general, performance decreases as more bits are stored in the table.
 *
 * This number is directly proportional to the size of the online region
 * used for lookups.
 *
 * Note that due to the way the CPU operates on memory, best lookup performance
 * will be achieved when RTE_EFD_VALUE_NUM_BITS is a multiple of 8.
 * These values align the hash indexes on 16-byte boundaries.
 * The greatest performance drop is moving from 8->9 bits, 16->17 bits, etc.
 *
 * This value must be between 1 and 32
 */
#ifndef RTE_EFD_VALUE_NUM_BITS
#define RTE_EFD_VALUE_NUM_BITS (8)
#endif

/*
 * EFD_TARGET_GROUP_NUM_RULES:
 *   Adjusts how many groups/chunks are allocated at table creation time
 *   to support the requested number of rules. Higher values pack entries
 *   more tightly in memory, resulting in a smaller memory footprint
 *   for the online table.
 *   This comes at the cost of lower insert/update performance.
 *
 * EFD_MAX_GROUP_NUM_RULES:
 *   This adjusts the amount of offline memory allocated to store key/value
 *   pairs for the table. The recommended numbers are upper-bounds for
 *   this parameter
 *   - any higher and it becomes very unlikely that a perfect hash function
 *   can be found for that group size. This value should be at
 *   least 40% larger than EFD_TARGET_GROUP_NUM_RULES
 *
 * Recommended values for various lookuptable and hashfunc sizes are:
 *
 *   HASH_FUNC_SIZE = 16, LOOKUPTBL_SIZE = 16:
 *     EFD_TARGET_GROUP_NUM_RULES = 22
 *     EFD_MAX_GROUP_NUM_RULES = 28
 */
#define EFD_TARGET_GROUP_NUM_RULES (22)
#define EFD_MAX_GROUP_NUM_RULES (28LU)

#define EFD_MIN_BALANCED_NUM_RULES      5

/**
 * Maximum number of keys that can be looked up in one call to efd_lookup_bulk
 */
#ifndef RTE_EFD_BURST_MAX
#define RTE_EFD_BURST_MAX (32)
#endif

/** Maximum number of characters in efd name.*/
#define RTE_EFD_NAMESIZE			32

#if (RTE_EFD_VALUE_NUM_BITS > 0 && RTE_EFD_VALUE_NUM_BITS <= 8)
typedef uint8_t efd_value_t;
#elif (RTE_EFD_VALUE_NUM_BITS > 8 && RTE_EFD_VALUE_NUM_BITS <= 16)
typedef uint16_t efd_value_t;
#elif (RTE_EFD_VALUE_NUM_BITS > 16 && RTE_EFD_VALUE_NUM_BITS <= 32)
typedef uint32_t efd_value_t;
#else
#error("RTE_EFD_VALUE_NUM_BITS must be in the range [1:32]")
#endif

#define EFD_LOOKUPTBL_SHIFT (32 - 4)
typedef uint16_t efd_lookuptbl_t;
typedef uint16_t efd_hashfunc_t;

/**
 * Creates an EFD table with a single offline region and multiple per-socket
 * internally-managed copies of the online table used for lookups
 *
 * @param name
 *   EFD table name
 * @param max_num_rules
 *   Minimum number of rules the table should be sized to hold.
 *   Will be rounded up to the next smallest valid table size
 * @param key_len
 *   Length of the key
 * @param online_cpu_socket_bitmask
 *   Bitmask specifying which sockets should get a copy of the online table.
 *   LSB = socket 0, etc.
 * @param offline_cpu_socket
 *   Identifies the socket where the offline table will be allocated
 *   (and most efficiently accessed in the case of updates/insertions)
 *
 * @return
 *   EFD table, or NULL if table allocation failed or the bitmask is invalid
 */
struct rte_efd_table *
rte_efd_create(const char *name, uint32_t max_num_rules, uint32_t key_len,
	uint64_t online_cpu_socket_bitmask, uint8_t offline_cpu_socket);

/**
 * Releases the resources from an EFD table
 *
 * @param table
 *   Pointer to table allocated with rte_efd_create().
 *   If table is NULL, no operation is performed.
 */
void
rte_efd_free(struct rte_efd_table *table);

/**
 * Find an existing EFD table object and return a pointer to it.
 *
 * @param name
 *   Name of the EFD table as passed to rte_efd_create()
 * @return
 *   Pointer to EFD table or NULL if object not found
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - ENOENT - value not available for return
 */
struct rte_efd_table*
rte_efd_find_existing(const char *name);

#define RTE_EFD_UPDATE_WARN_GROUP_FULL   (1)
#define RTE_EFD_UPDATE_NO_CHANGE         (2)
#define RTE_EFD_UPDATE_FAILED            (3)

/**
 * Computes an updated table entry for the supplied key/value pair.
 * The update is then immediately applied to the provided table and
 * all socket-local copies of the chunks are updated.
 * This operation is not multi-thread safe
 * and should only be called one from thread.
 *
 * @param table
 *   EFD table to reference
 * @param socket_id
 *   Socket ID to use to lookup existing value (ideally caller's socket id)
 * @param key
 *   EFD table key to modify
 * @param value
 *   Value to associate with the key
 *
 * @return
 *  RTE_EFD_UPDATE_WARN_GROUP_FULL
 *     Operation is insert, and the last available space in the
 *     key's group was just used
 *     Future inserts may fail as groups fill up
 *     This operation was still successful, and entry contains a valid update
 *  RTE_EFD_UPDATE_FAILED
 *     Either the EFD failed to find a suitable perfect hash or the group was full
 *     This is a fatal error, and the table is now in an indeterminate state
 *  RTE_EFD_UPDATE_NO_CHANGE
 *     Operation resulted in no change to the table (same value already exists)
 *  0 - success
 */
int
rte_efd_update(struct rte_efd_table *table, unsigned int socket_id,
	const void *key, efd_value_t value);

/**
 * Removes any value currently associated with the specified key from the table
 * This operation is not multi-thread safe
 * and should only be called from one thread.
 *
 * @param table
 *   EFD table to reference
 * @param socket_id
 *   Socket ID to use to lookup existing value (ideally caller's socket id)
 * @param key
 *   EFD table key to delete
 * @param prev_value
 *   If not NULL, will store the previous value here before deleting it
 *
 * @return
 *   0 - successfully found and deleted the key
 *   nonzero otherwise
 */
int
rte_efd_delete(struct rte_efd_table *table, unsigned int socket_id,
	const void *key, efd_value_t *prev_value);

/**
 * Looks up the value associated with a key
 * This operation is multi-thread safe.
 *
 * NOTE: Lookups will *always* succeed - this is a property of
 * using a perfect hash table.
 * If the specified key was never inserted, a pseudorandom answer will be returned.
 * There is no way to know based on the lookup if the key was ever inserted
 * originally, so this must be tracked elsewhere.
 *
 * @param table
 *   EFD table to reference
 * @param socket_id
 *   Socket ID to use to lookup existing value (ideally caller's socket id)
 * @param key
 *   EFD table key to look up
 *
 * @return
 *   Value associated with the key, or random junk if they key was never inserted
 */
efd_value_t
rte_efd_lookup(const struct rte_efd_table *table, unsigned int socket_id,
		const void *key);

/**
 * Looks up the value associated with several keys.
 * This operation is multi-thread safe.
 *
 * NOTE: Lookups will *always* succeed - this is a property of
 * using a perfect hash table.
 * If the specified key was never inserted, a pseudorandom answer will be returned.
 * There is no way to know based on the lookup if the key was ever inserted
 * originally, so this must be tracked elsewhere.
 *
 * @param table
 *   EFD table to reference
 * @param socket_id
 *   Socket ID to use to lookup existing value (ideally caller's socket id)
 * @param num_keys
 *   Number of keys in the key_list array, must be less than RTE_EFD_BURST_MAX
 * @param key_list
 *   Array of num_keys pointers which point to keys to look up
 * @param value_list
 *   Array of size num_keys where lookup values will be stored
 */
void
rte_efd_lookup_bulk(const struct rte_efd_table *table, unsigned int socket_id,
		int num_keys, const void **key_list,
		efd_value_t *value_list);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_EFD_H_ */
