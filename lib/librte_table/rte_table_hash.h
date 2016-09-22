/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
 * Use-cases: Flow classification table, Address Resolution Protocol (ARP) table
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
 *        specific data structures per each bucket.
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
 *        per table and per each bucket.
 * 2. Key signature computation:
 *     a. Pre-computed key signature: The key lookup operation is split between
 *        two CPU cores. The first CPU core (typically the CPU core performing
 *        packet RX) extracts the key from the input packet, computes the key
 *        signature and saves both the key and the key signature in the packet
 *        buffer as packet meta-data. The second CPU core reads both the key and
 *        the key signature from the packet meta-data and performs the bucket
 *        search step of the key lookup operation.
 *     b. Key signature computed on lookup (do-sig): The same CPU core reads
 *        the key from the packet meta-data, uses it to compute the key
 *        signature and also performs the bucket search step of the key lookup
 *        operation.
 * 3. Key size:
 *     a. Configurable key size
 *     b. Single key size (8-byte, 16-byte or 32-byte key size)
 *
 ***/
#include <stdint.h>

#include "rte_table.h"

/** Hash function */
typedef uint64_t (*rte_table_hash_op_hash)(
	void *key,
	uint32_t key_size,
	uint64_t seed);

/**
 * Hash tables with configurable key size
 *
 */
/** Extendible bucket hash table parameters */
struct rte_table_hash_ext_params {
	/** Key size (number of bytes) */
	uint32_t key_size;

	/** Maximum number of keys */
	uint32_t n_keys;

	/** Number of hash table buckets. Each bucket stores up to 4 keys. */
	uint32_t n_buckets;

	/** Number of hash table bucket extensions. Each bucket extension has
	space for 4 keys and each bucket can have 0, 1 or more extensions. */
	uint32_t n_buckets_ext;

	/** Hash function */
	rte_table_hash_op_hash f_hash;

	/** Seed value for the hash function */
	uint64_t seed;

	/** Byte offset within packet meta-data where the 4-byte key signature
	is located. Valid for pre-computed key signature tables, ignored for
	do-sig tables. */
	uint32_t signature_offset;

	/** Byte offset within packet meta-data where the key is located */
	uint32_t key_offset;
};

/** Extendible bucket hash table operations for pre-computed key signature */
extern struct rte_table_ops rte_table_hash_ext_ops;

/** Extendible bucket hash table operations for key signature computed on
	lookup ("do-sig") */
extern struct rte_table_ops rte_table_hash_ext_dosig_ops;

/** LRU hash table parameters */
struct rte_table_hash_lru_params {
	/** Key size (number of bytes) */
	uint32_t key_size;

	/** Maximum number of keys */
	uint32_t n_keys;

	/** Number of hash table buckets. Each bucket stores up to 4 keys. */
	uint32_t n_buckets;

	/** Hash function */
	rte_table_hash_op_hash f_hash;

	/** Seed value for the hash function */
	uint64_t seed;

	/** Byte offset within packet meta-data where the 4-byte key signature
	is located. Valid for pre-computed key signature tables, ignored for
	do-sig tables. */
	uint32_t signature_offset;

	/** Byte offset within packet meta-data where the key is located */
	uint32_t key_offset;
};

/** LRU hash table operations for pre-computed key signature */
extern struct rte_table_ops rte_table_hash_lru_ops;

/** LRU hash table operations for key signature computed on lookup ("do-sig") */
extern struct rte_table_ops rte_table_hash_lru_dosig_ops;

/**
 * 8-byte key hash tables
 *
 */
/** LRU hash table parameters */
struct rte_table_hash_key8_lru_params {
	/** Maximum number of entries (and keys) in the table */
	uint32_t n_entries;

	/** Hash function */
	rte_table_hash_op_hash f_hash;

	/** Seed for the hash function */
	uint64_t seed;

	/** Byte offset within packet meta-data where the 4-byte key signature
	is located. Valid for pre-computed key signature tables, ignored for
	do-sig tables. */
	uint32_t signature_offset;

	/** Byte offset within packet meta-data where the key is located */
	uint32_t key_offset;

	/** Bit-mask to be AND-ed to the key on lookup */
	uint8_t *key_mask;
};

/** LRU hash table operations for pre-computed key signature */
extern struct rte_table_ops rte_table_hash_key8_lru_ops;

/** LRU hash table operations for key signature computed on lookup ("do-sig") */
extern struct rte_table_ops rte_table_hash_key8_lru_dosig_ops;

/** Extendible bucket hash table parameters */
struct rte_table_hash_key8_ext_params {
	/** Maximum number of entries (and keys) in the table */
	uint32_t n_entries;

	/** Number of entries (and keys) for hash table bucket extensions. Each
		bucket is extended in increments of 4 keys. */
	uint32_t n_entries_ext;

	/** Hash function */
	rte_table_hash_op_hash f_hash;

	/** Seed for the hash function */
	uint64_t seed;

	/** Byte offset within packet meta-data where the 4-byte key signature
	is located. Valid for pre-computed key signature tables, ignored for
	do-sig tables. */
	uint32_t signature_offset;

	/** Byte offset within packet meta-data where the key is located */
	uint32_t key_offset;

	/** Bit-mask to be AND-ed to the key on lookup */
	uint8_t *key_mask;
};

/** Extendible bucket hash table operations for pre-computed key signature */
extern struct rte_table_ops rte_table_hash_key8_ext_ops;

/** Extendible bucket hash table operations for key signature computed on
    lookup ("do-sig") */
extern struct rte_table_ops rte_table_hash_key8_ext_dosig_ops;

/**
 * 16-byte key hash tables
 *
 */
/** LRU hash table parameters */
struct rte_table_hash_key16_lru_params {
	/** Maximum number of entries (and keys) in the table */
	uint32_t n_entries;

	/** Hash function */
	rte_table_hash_op_hash f_hash;

	/** Seed for the hash function */
	uint64_t seed;

	/** Byte offset within packet meta-data where the 4-byte key signature
	is located. Valid for pre-computed key signature tables, ignored for
	do-sig tables. */
	uint32_t signature_offset;

	/** Byte offset within packet meta-data where the key is located */
	uint32_t key_offset;

	/** Bit-mask to be AND-ed to the key on lookup */
	uint8_t *key_mask;
};

/** LRU hash table operations for pre-computed key signature */
extern struct rte_table_ops rte_table_hash_key16_lru_ops;

/** LRU hash table operations for key signature computed on lookup
    ("do-sig") */
extern struct rte_table_ops rte_table_hash_key16_lru_dosig_ops;

/** Extendible bucket hash table parameters */
struct rte_table_hash_key16_ext_params {
	/** Maximum number of entries (and keys) in the table */
	uint32_t n_entries;

	/** Number of entries (and keys) for hash table bucket extensions. Each
	bucket is extended in increments of 4 keys. */
	uint32_t n_entries_ext;

	/** Hash function */
	rte_table_hash_op_hash f_hash;

	/** Seed for the hash function */
	uint64_t seed;

	/** Byte offset within packet meta-data where the 4-byte key signature
	is located. Valid for pre-computed key signature tables, ignored for
	do-sig tables. */
	uint32_t signature_offset;

	/** Byte offset within packet meta-data where the key is located */
	uint32_t key_offset;

	/** Bit-mask to be AND-ed to the key on lookup */
	uint8_t *key_mask;
};

/** Extendible bucket operations for pre-computed key signature */
extern struct rte_table_ops rte_table_hash_key16_ext_ops;

/** Extendible bucket hash table operations for key signature computed on
    lookup ("do-sig") */
extern struct rte_table_ops rte_table_hash_key16_ext_dosig_ops;

/**
 * 32-byte key hash tables
 *
 */
/** LRU hash table parameters */
struct rte_table_hash_key32_lru_params {
	/** Maximum number of entries (and keys) in the table */
	uint32_t n_entries;

	/** Hash function */
	rte_table_hash_op_hash f_hash;

	/** Seed for the hash function */
	uint64_t seed;

	/** Byte offset within packet meta-data where the 4-byte key signature
	is located. Valid for pre-computed key signature tables, ignored for
	do-sig tables. */
	uint32_t signature_offset;

	/** Byte offset within packet meta-data where the key is located */
	uint32_t key_offset;
};

/** LRU hash table operations for pre-computed key signature */
extern struct rte_table_ops rte_table_hash_key32_lru_ops;

/** Extendible bucket hash table parameters */
struct rte_table_hash_key32_ext_params {
	/** Maximum number of entries (and keys) in the table */
	uint32_t n_entries;

	/** Number of entries (and keys) for hash table bucket extensions. Each
		bucket is extended in increments of 4 keys. */
	uint32_t n_entries_ext;

	/** Hash function */
	rte_table_hash_op_hash f_hash;

	/** Seed for the hash function */
	uint64_t seed;

	/** Byte offset within packet meta-data where the 4-byte key signature
	is located. Valid for pre-computed key signature tables, ignored for
	do-sig tables. */
	uint32_t signature_offset;

	/** Byte offset within packet meta-data where the key is located */
	uint32_t key_offset;
};

/** Extendible bucket hash table operations */
extern struct rte_table_ops rte_table_hash_key32_ext_ops;

/** Cuckoo hash table parameters */
struct rte_table_hash_cuckoo_params {
    /** Key size (number of bytes */
		uint32_t key_size;

	/** Maximum number of hash table entries */
	uint32_t n_keys;

	/** Hash function used to calculate hash */
	rte_table_hash_op_hash f_hash;

	/** Seed value or Init value used by f_hash */
	uint32_t seed;

	/** Byte offset within packet meta-data where the 4-byte key signature
	is located. Valid for pre-computed key signature tables, ignored for
	do-sig tables. */
	uint32_t signature_offset;

	/** Byte offset within packet meta-data where the key is located */
	uint32_t key_offset;

	/** Hash table name */
	const char *name;
};

/** Cuckoo hash table operations */
extern struct rte_table_ops rte_table_hash_cuckoo_dosig_ops;

#ifdef __cplusplus
}
#endif

#endif
