/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef __INCLUDE_RTE_TABLE_HASH_H__
#define __INCLUDE_RTE_TABLE_HASH_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Table Hash
 *
 * These tables use the exact match criterion to uniquely associate data to
 * lookup keys.
 *
 * Hash table types:
 * 1. Entry add strategy on bucket full:
 *     a. Least Recently Used (LRU): One of the existing keys in the bucket is
 *        deleted and the new key is added in its place. The number of keys in
 *        each bucket never grows bigger than 4. The logic to pick the key to
 *        be dropped from the bucket is LRU. The hash table lookup operation
 *        maintains the order in which the keys in the same bucket are hit, so
 *        every time a key is hit, it becomes the new Most Recently Used (MRU)
 *        key, i.e. the most unlikely candidate for drop. When a key is added
 *        to the bucket, it also becomes the new MRU key. When a key needs to
 *        be picked and dropped, the most likely candidate for drop, i.e. the
 *        current LRU key, is always picked. The LRU logic requires maintaining
 *        specific data structures per each bucket. Use-cases: flow cache, etc.
 *     b. Extendible bucket (ext): The bucket is extended with space for 4 more
 *        keys. This is done by allocating additional memory at table init time,
 *        which is used to create a pool of free keys (the size of this pool is
 *        configurable and always a multiple of 4). On key add operation, the
 *        allocation of a group of 4 keys only happens successfully within the
 *        limit of free keys, otherwise the key add operation fails. On key
 *        delete operation, a group of 4 keys is freed back to the pool of free
 *        keys when the key to be deleted is the only key that was used within
 *        its group of 4 keys at that time. On key lookup operation, if the
 *        current bucket is in extended state and a match is not found in the
 *        first group of 4 keys, the search continues beyond the first group of
 *        4 keys, potentially until all keys in this bucket are examined. The
 *        extendible bucket logic requires maintaining specific data structures
 *        per table and per each bucket. Use-cases: flow table, etc.
 * 2. Key size:
 *     a. Configurable key size
 *     b. Single key size (8-byte, 16-byte or 32-byte key size)
 *
 ***/
#include <stdint.h>

#include "rte_table.h"

/** Hash function */
typedef uint64_t (*rte_table_hash_op_hash)(
	void *key,
	void *key_mask,
	uint32_t key_size,
	uint64_t seed);

/** Hash table parameters */
struct rte_table_hash_params {
	/** Name */
	const char *name;

	/** Key size (number of bytes) */
	uint32_t key_size;

	/** Byte offset within packet meta-data where the key is located */
	uint32_t key_offset;

	/** Key mask */
	uint8_t *key_mask;

	/** Number of keys */
	uint32_t n_keys;

	/** Number of buckets */
	uint32_t n_buckets;

	/** Hash function */
	rte_table_hash_op_hash f_hash;

	/** Seed value for the hash function */
	uint64_t seed;
};

/** Extendible bucket hash table operations */
extern struct rte_table_ops rte_table_hash_ext_ops;
extern struct rte_table_ops rte_table_hash_key8_ext_ops;
extern struct rte_table_ops rte_table_hash_key16_ext_ops;
extern struct rte_table_ops rte_table_hash_key32_ext_ops;

/** LRU hash table operations */
extern struct rte_table_ops rte_table_hash_lru_ops;

extern struct rte_table_ops rte_table_hash_key8_lru_ops;
extern struct rte_table_ops rte_table_hash_key16_lru_ops;
extern struct rte_table_ops rte_table_hash_key32_lru_ops;

#ifdef __cplusplus
}
#endif

#endif
