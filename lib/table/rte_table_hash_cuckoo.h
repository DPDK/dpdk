/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef __INCLUDE_RTE_TABLE_HASH_CUCKOO_H__
#define __INCLUDE_RTE_TABLE_HASH_CUCKOO_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Table Hash Cuckoo
 *
 ***/
#include <stdint.h>

#include <rte_hash.h>

#include "rte_table.h"

/** Hash table parameters */
struct rte_table_hash_cuckoo_params {
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
	rte_hash_function f_hash;

	/** Seed value for the hash function */
	uint32_t seed;
};

/** Cuckoo hash table operations */
extern struct rte_table_ops rte_table_hash_cuckoo_ops;

#ifdef __cplusplus
}
#endif

#endif
