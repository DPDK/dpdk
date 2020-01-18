/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 * Copyright(c) 2018 Arm Limited
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

#if defined(RTE_LIBRTE_HASH_DEBUG)
#define ERR_IF_TRUE(cond, fmt, args...) do { \
	if (cond) { \
		RTE_LOG(ERR, HASH, fmt, ##args); \
		return; \
	} \
} while (0)
#else
#define ERR_IF_TRUE(cond, fmt, args...)
#endif

#include <rte_hash_crc.h>
#include <rte_jhash.h>

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


/** Number of items per bucket. */
#define RTE_HASH_BUCKET_ENTRIES		8

#if !RTE_IS_POWER_OF_2(RTE_HASH_BUCKET_ENTRIES)
#error RTE_HASH_BUCKET_ENTRIES must be a power of 2
#endif

#define NULL_SIGNATURE			0

#define EMPTY_SLOT			0

#define KEY_ALIGNMENT			16

#define LCORE_CACHE_SIZE		64

#define RTE_HASH_BFS_QUEUE_MAX_LEN       1000

#define RTE_XABORT_CUCKOO_PATH_INVALIDED 0x4

#define RTE_HASH_TSX_MAX_RETRY  10

struct lcore_cache {
	unsigned len; /**< Cache len */
	uint32_t objs[LCORE_CACHE_SIZE]; /**< Cache objects */
} __rte_cache_aligned;

/* Structure that stores key-value pair */
struct rte_hash_key {
	union {
		uintptr_t idata;
		void *pdata;
	};
	/* Variable key size */
	char key[0];
};

/* All different signature compare functions */
enum rte_hash_sig_compare_function {
	RTE_HASH_COMPARE_SCALAR = 0,
	RTE_HASH_COMPARE_SSE,
	RTE_HASH_COMPARE_NEON,
	RTE_HASH_COMPARE_NUM
};

/** Bucket structure */
struct rte_hash_bucket {
	uint16_t sig_current[RTE_HASH_BUCKET_ENTRIES];

	uint32_t key_idx[RTE_HASH_BUCKET_ENTRIES];

	uint8_t flag[RTE_HASH_BUCKET_ENTRIES];

	void *next;
} __rte_cache_aligned;

/** A hash table structure. */
struct rte_hash {
	char name[RTE_HASH_NAMESIZE];   /**< Name of the hash. */
	uint32_t entries;               /**< Total table entries. */
	uint32_t num_buckets;           /**< Number of buckets in table. */

	struct rte_ring *free_slots;
	/**< Ring that stores all indexes of the free slots in the key table */

	struct lcore_cache *local_free_slots;
	/**< Local cache per lcore, storing some indexes of the free slots */

	/* Fields used in lookup */

	uint32_t key_len __rte_cache_aligned;
	/**< Length of hash key. */
	uint8_t hw_trans_mem_support;
	/**< If hardware transactional memory is used. */
	uint8_t use_local_cache;
	/**< If multi-writer support is enabled, use local cache
	 * to allocate key-store slots.
	 */
	uint8_t readwrite_concur_support;
	/**< If read-write concurrency support is enabled */
	uint8_t ext_table_support;     /**< Enable extendable bucket table */
	uint8_t no_free_on_del;
	/**< If key index should be freed on calling rte_hash_del_xxx APIs.
	 * If this is set, rte_hash_free_key_with_position must be called to
	 * free the key index associated with the deleted entry.
	 * This flag is enabled by default.
	 */
	uint8_t readwrite_concur_lf_support;
	/**< If read-write concurrency lock free support is enabled */
	uint8_t writer_takes_lock;
	/**< Indicates if the writer threads need to take lock */
	rte_hash_function hash_func;    /**< Function used to calculate hash. */
	uint32_t hash_func_init_val;    /**< Init value used by hash_func. */
	rte_hash_cmp_eq_t rte_hash_custom_cmp_eq;
	/**< Custom function used to compare keys. */
	enum cmp_jump_table_case cmp_jump_table_idx;
	/**< Indicates which compare function to use. */
	enum rte_hash_sig_compare_function sig_cmp_fn;
	/**< Indicates which signature compare function to use. */
	uint32_t bucket_bitmask;
	/**< Bitmask for getting bucket index from hash signature. */
	uint32_t key_entry_size;         /**< Size of each key entry. */

	void *key_store;                /**< Table storing all keys and data */
	struct rte_hash_bucket *buckets;
	/**< Table with buckets storing all the	hash values and key indexes
	 * to the key table.
	 */
	rte_rwlock_t *readwrite_lock; /**< Read-write lock thread-safety. */
	struct rte_hash_bucket *buckets_ext; /**< Extra buckets array */
	struct rte_ring *free_ext_bkts; /**< Ring of indexes of free buckets */
	/* Stores index of an empty ext bkt to be recycled on calling
	 * rte_hash_del_xxx APIs. When lock free read-write concurrency is
	 * enabled, an empty ext bkt cannot be put into free list immediately
	 * (as readers might be using it still). Hence freeing of the ext bkt
	 * is piggy-backed to freeing of the key index.
	 */
	uint32_t *ext_bkt_to_free;
	uint32_t *tbl_chng_cnt;
	/**< Indicates if the hash table changed from last read. */
} __rte_cache_aligned;

struct queue_node {
	struct rte_hash_bucket *bkt; /* Current bucket on the bfs search */
	uint32_t cur_bkt_idx;

	struct queue_node *prev;     /* Parent(bucket) in search path */
	int prev_slot;               /* Parent(slot) in search path */
};

#endif
