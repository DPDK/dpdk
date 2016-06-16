/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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

/* rte_cuckoo_hash.h
 * This file hold Cuckoo Hash private data structures to allows include from
 * platform specific files like rte_cuckoo_hash_x86.h
 */

#ifndef _RTE_CUCKOO_HASH_H_
#define _RTE_CUCKOO_HASH_H_

#if defined(RTE_ARCH_X86)
#include "rte_cmp_x86.h"
#endif

#if defined(RTE_ARCH_ARM64)
#include "rte_cmp_arm64.h"
#endif

/* Macro to enable/disable run-time checking of function parameters */
#if defined(RTE_LIBRTE_HASH_DEBUG)
#define RETURN_IF_TRUE(cond, retval) do { \
	if (cond) \
		return retval; \
} while (0)
#else
#define RETURN_IF_TRUE(cond, retval)
#endif

/* Hash function used if none is specified */
#if defined(RTE_MACHINE_CPUFLAG_SSE4_2) || defined(RTE_MACHINE_CPUFLAG_CRC32)
#include <rte_hash_crc.h>
#define DEFAULT_HASH_FUNC       rte_hash_crc
#else
#include <rte_jhash.h>
#define DEFAULT_HASH_FUNC       rte_jhash
#endif

#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM64)
/*
 * All different options to select a key compare function,
 * based on the key size and custom function.
 */
enum cmp_jump_table_case {
	KEY_CUSTOM = 0,
	KEY_16_BYTES,
	KEY_32_BYTES,
	KEY_48_BYTES,
	KEY_64_BYTES,
	KEY_80_BYTES,
	KEY_96_BYTES,
	KEY_112_BYTES,
	KEY_128_BYTES,
	KEY_OTHER_BYTES,
	NUM_KEY_CMP_CASES,
};

/*
 * Table storing all different key compare functions
 * (multi-process supported)
 */
const rte_hash_cmp_eq_t cmp_jump_table[NUM_KEY_CMP_CASES] = {
	NULL,
	rte_hash_k16_cmp_eq,
	rte_hash_k32_cmp_eq,
	rte_hash_k48_cmp_eq,
	rte_hash_k64_cmp_eq,
	rte_hash_k80_cmp_eq,
	rte_hash_k96_cmp_eq,
	rte_hash_k112_cmp_eq,
	rte_hash_k128_cmp_eq,
	memcmp
};
#else
/*
 * All different options to select a key compare function,
 * based on the key size and custom function.
 */
enum cmp_jump_table_case {
	KEY_CUSTOM = 0,
	KEY_OTHER_BYTES,
	NUM_KEY_CMP_CASES,
};

/*
 * Table storing all different key compare functions
 * (multi-process supported)
 */
const rte_hash_cmp_eq_t cmp_jump_table[NUM_KEY_CMP_CASES] = {
	NULL,
	memcmp
};

#endif

enum add_key_case {
	ADD_KEY_SINGLEWRITER = 0,
	ADD_KEY_MULTIWRITER,
	ADD_KEY_MULTIWRITER_TM,
};

/** Number of items per bucket. */
#define RTE_HASH_BUCKET_ENTRIES		4

#define NULL_SIGNATURE			0

#define KEY_ALIGNMENT			16

#define LCORE_CACHE_SIZE		64

#define RTE_HASH_BFS_QUEUE_MAX_LEN       1000

#define RTE_XABORT_CUCKOO_PATH_INVALIDED 0x4

#define RTE_HASH_TSX_MAX_RETRY  10

struct lcore_cache {
	unsigned len; /**< Cache len */
	void *objs[LCORE_CACHE_SIZE]; /**< Cache objects */
} __rte_cache_aligned;

/* Structure storing both primary and secondary hashes */
struct rte_hash_signatures {
	union {
		struct {
			hash_sig_t current;
			hash_sig_t alt;
		};
		uint64_t sig;
	};
};

/* Structure that stores key-value pair */
struct rte_hash_key {
	union {
		uintptr_t idata;
		void *pdata;
	};
	/* Variable key size */
	char key[0];
} __attribute__((aligned(KEY_ALIGNMENT)));

/** Bucket structure */
struct rte_hash_bucket {
	struct rte_hash_signatures signatures[RTE_HASH_BUCKET_ENTRIES];
	/* Includes dummy key index that always contains index 0 */
	uint32_t key_idx[RTE_HASH_BUCKET_ENTRIES + 1];
	uint8_t flag[RTE_HASH_BUCKET_ENTRIES];
} __rte_cache_aligned;

/** A hash table structure. */
struct rte_hash {
	char name[RTE_HASH_NAMESIZE];   /**< Name of the hash. */
	uint32_t entries;               /**< Total table entries. */
	uint32_t num_buckets;           /**< Number of buckets in table. */
	uint32_t key_len;               /**< Length of hash key. */
	rte_hash_function hash_func;    /**< Function used to calculate hash. */
	uint32_t hash_func_init_val;    /**< Init value used by hash_func. */
	rte_hash_cmp_eq_t rte_hash_custom_cmp_eq;
	/**< Custom function used to compare keys. */
	enum cmp_jump_table_case cmp_jump_table_idx;
	/**< Indicates which compare function to use. */
	uint32_t bucket_bitmask;        /**< Bitmask for getting bucket index
						from hash signature. */
	uint32_t key_entry_size;         /**< Size of each key entry. */

	struct rte_ring *free_slots;    /**< Ring that stores all indexes
						of the free slots in the key table */
	void *key_store;                /**< Table storing all keys and data */
	struct rte_hash_bucket *buckets;	/**< Table with buckets storing all the
							hash values and key indexes
							to the key table*/
	uint8_t hw_trans_mem_support;	/**< Hardware transactional
							memory support */
	struct lcore_cache *local_free_slots;
	/**< Local cache per lcore, storing some indexes of the free slots */
	enum add_key_case add_key; /**< Multi-writer hash add behavior */

	rte_spinlock_t *multiwriter_lock; /**< Multi-writer spinlock for w/o TM */
} __rte_cache_aligned;

struct queue_node {
	struct rte_hash_bucket *bkt; /* Current bucket on the bfs search */

	struct queue_node *prev;     /* Parent(bucket) in search path */
	int prev_slot;               /* Parent(slot) in search path */
};

#endif
