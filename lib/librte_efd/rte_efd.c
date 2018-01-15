/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/queue.h>

#include <rte_log.h>
#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_prefetch.h>
#include <rte_branch_prediction.h>
#include <rte_memcpy.h>
#include <rte_ring.h>
#include <rte_jhash.h>
#include <rte_hash_crc.h>

#include "rte_efd.h"
#if defined(RTE_ARCH_X86)
#include "rte_efd_x86.h"
#elif defined(RTE_ARCH_ARM64)
#include "rte_efd_arm64.h"
#endif

#define EFD_KEY(key_idx, table) (table->keys + ((key_idx) * table->key_len))
/** Hash function used to determine chunk_id and bin_id for a group */
#define EFD_HASH(key, table) \
	(uint32_t)(rte_jhash(key, table->key_len, 0xbc9f1d34))
/** Hash function used as constant component of perfect hash search */
#define EFD_HASHFUNCA(key, table) \
	(uint32_t)(rte_hash_crc(key, table->key_len, 0xbc9f1d35))
/** Hash function used as multiplicative component of perfect hash search */
#define EFD_HASHFUNCB(key, table) \
	(uint32_t)(rte_hash_crc(key, table->key_len, 0xbc9f1d36))

/*************************************************************************
 * Fixed constants
 *************************************************************************/

/* These parameters are fixed by the efd_bin_to_group balancing table */
#define EFD_CHUNK_NUM_GROUPS (64)
#define EFD_CHUNK_NUM_BINS   (256)
#define EFD_CHUNK_NUM_BIN_TO_GROUP_SETS \
	(EFD_CHUNK_NUM_BINS / EFD_CHUNK_NUM_GROUPS)

/*
 * Target number of rules that each chunk is created to handle.
 * Used when initially allocating the table
 */
#define EFD_TARGET_CHUNK_NUM_RULES  \
	(EFD_CHUNK_NUM_GROUPS * EFD_TARGET_GROUP_NUM_RULES)
/*
 * Max number of rules that each chunk is created to handle.
 * Used when initially allocating the table
 */
#define EFD_TARGET_CHUNK_MAX_NUM_RULES  \
	(EFD_CHUNK_NUM_GROUPS * EFD_MAX_GROUP_NUM_RULES)

/** This is fixed based on the bin_to_group permutation array */
#define EFD_MAX_GROUP_NUM_BINS (16)

/**
 * The end of the chunks array needs some extra padding to ensure
 * that vectorization over-reads on the last online chunk stay within
allocated memory
 */
#define EFD_NUM_CHUNK_PADDING_BYTES (256)

/* All different internal lookup functions */
enum efd_lookup_internal_function {
	EFD_LOOKUP_SCALAR = 0,
	EFD_LOOKUP_AVX2,
	EFD_LOOKUP_NEON,
	EFD_LOOKUP_NUM
};

TAILQ_HEAD(rte_efd_list, rte_tailq_entry);

static struct rte_tailq_elem rte_efd_tailq = {
	.name = "RTE_EFD",
};
EAL_REGISTER_TAILQ(rte_efd_tailq);

/** Internal permutation array used to shuffle bins into pseudorandom groups */
const uint32_t efd_bin_to_group[EFD_CHUNK_NUM_BIN_TO_GROUP_SETS][EFD_CHUNK_NUM_BINS] = {
	{
		0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
		4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7,
		8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11,
		12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15,
		16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18, 19, 19, 19, 19,
		20, 20, 20, 20, 21, 21, 21, 21, 22, 22, 22, 22, 23, 23, 23, 23,
		24, 24, 24, 24, 25, 25, 25, 25, 26, 26, 26, 26, 27, 27, 27, 27,
		28, 28, 28, 28, 29, 29, 29, 29, 30, 30, 30, 30, 31, 31, 31, 31,
		32, 32, 32, 32, 33, 33, 33, 33, 34, 34, 34, 34, 35, 35, 35, 35,
		36, 36, 36, 36, 37, 37, 37, 37, 38, 38, 38, 38, 39, 39, 39, 39,
		40, 40, 40, 40, 41, 41, 41, 41, 42, 42, 42, 42, 43, 43, 43, 43,
		44, 44, 44, 44, 45, 45, 45, 45, 46, 46, 46, 46, 47, 47, 47, 47,
		48, 48, 48, 48, 49, 49, 49, 49, 50, 50, 50, 50, 51, 51, 51, 51,
		52, 52, 52, 52, 53, 53, 53, 53, 54, 54, 54, 54, 55, 55, 55, 55,
		56, 56, 56, 56, 57, 57, 57, 57, 58, 58, 58, 58, 59, 59, 59, 59,
		60, 60, 60, 60, 61, 61, 61, 61, 62, 62, 62, 62, 63, 63, 63, 63
	},
	{
		34, 33, 48, 59, 0, 21, 36, 18, 9, 49, 54, 38, 51, 23, 31, 5,
		44, 23, 37, 52, 11, 4, 58, 20, 38, 40, 38, 22, 26, 28, 42, 6,
		46, 16, 31, 28, 46, 14, 60, 0, 35, 53, 16, 58, 16, 29, 39, 7,
		1, 54, 15, 11, 48, 3, 62, 9, 58, 5, 30, 43, 17, 7, 36, 34,
		6, 36, 2, 14, 10, 1, 47, 47, 20, 45, 62, 56, 34, 25, 39, 18,
		51, 41, 61, 25, 56, 40, 41, 37, 52, 35, 30, 57, 11, 42, 37, 27,
		54, 19, 26, 13, 48, 31, 46, 15, 12, 10, 16, 20, 43, 17, 12, 55,
		45, 18, 8, 41, 7, 31, 42, 63, 12, 14, 21, 57, 24, 40, 5, 41,
		13, 44, 23, 59, 25, 57, 52, 50, 62, 1, 2, 49, 32, 57, 26, 43,
		56, 60, 55, 5, 49, 6, 3, 50, 46, 39, 27, 33, 17, 4, 53, 13,
		2, 19, 36, 51, 63, 0, 22, 33, 59, 28, 29, 23, 45, 33, 53, 27,
		22, 21, 40, 56, 4, 18, 44, 47, 28, 17, 4, 50, 21, 62, 8, 39,
		0, 8, 15, 24, 29, 24, 9, 11, 48, 61, 35, 55, 43, 1, 54, 42,
		53, 60, 22, 3, 32, 52, 25, 8, 15, 60, 7, 55, 27, 63, 19, 10,
		63, 24, 61, 19, 12, 38, 6, 29, 13, 37, 10, 3, 45, 32, 32, 30,
		49, 61, 44, 14, 20, 58, 35, 30, 2, 26, 34, 51, 9, 59, 47, 50
	},
	{
		32, 35, 32, 34, 55, 5, 6, 23, 49, 11, 6, 23, 52, 37, 29, 54,
		55, 40, 63, 50, 29, 52, 61, 25, 12, 56, 39, 38, 29, 11, 46, 1,
		40, 11, 19, 56, 7, 28, 51, 16, 15, 48, 21, 51, 60, 31, 14, 22,
		41, 47, 59, 56, 53, 28, 58, 26, 43, 27, 41, 33, 24, 52, 44, 38,
		13, 59, 48, 51, 60, 15, 3, 30, 15, 0, 10, 62, 44, 14, 28, 51,
		38, 2, 41, 26, 25, 49, 10, 12, 55, 57, 27, 35, 19, 33, 0, 30,
		5, 36, 47, 53, 5, 53, 20, 43, 34, 37, 52, 41, 21, 63, 59, 9,
		24, 1, 45, 24, 39, 44, 45, 16, 9, 17, 7, 50, 57, 22, 18, 28,
		25, 45, 2, 40, 58, 15, 17, 3, 1, 27, 61, 39, 19, 0, 19, 21,
		57, 62, 54, 60, 54, 40, 48, 33, 36, 37, 4, 42, 1, 43, 58, 8,
		13, 42, 10, 56, 35, 22, 48, 61, 63, 10, 49, 9, 24, 9, 25, 57,
		33, 18, 13, 31, 42, 36, 36, 55, 30, 37, 53, 34, 59, 4, 4, 23,
		8, 16, 58, 14, 30, 11, 12, 63, 49, 62, 2, 39, 47, 22, 2, 60,
		18, 8, 46, 31, 6, 20, 32, 29, 46, 42, 20, 31, 32, 61, 34, 4,
		47, 26, 20, 43, 26, 21, 7, 3, 16, 35, 18, 44, 27, 62, 13, 23,
		6, 50, 12, 8, 45, 17, 3, 46, 50, 7, 14, 5, 17, 54, 38, 0
	},
	{
		29, 56, 5, 7, 54, 48, 23, 37, 35, 44, 52, 40, 33, 49, 60, 0,
		59, 51, 28, 12, 41, 26, 2, 23, 34, 5, 59, 40, 3, 19, 6, 26,
		35, 53, 45, 49, 29, 57, 28, 62, 58, 59, 19, 53, 59, 62, 6, 54,
		13, 15, 48, 50, 45, 21, 41, 12, 34, 40, 24, 56, 19, 21, 35, 18,
		55, 45, 9, 61, 47, 61, 19, 15, 16, 39, 17, 31, 3, 51, 21, 50,
		17, 25, 25, 11, 44, 16, 18, 28, 14, 2, 37, 61, 58, 27, 62, 4,
		14, 17, 1, 9, 46, 28, 37, 0, 53, 43, 57, 7, 57, 46, 21, 41,
		39, 14, 52, 60, 44, 53, 49, 60, 49, 63, 13, 11, 29, 1, 55, 47,
		55, 12, 60, 43, 54, 37, 13, 6, 42, 10, 36, 13, 9, 8, 34, 51,
		31, 32, 12, 7, 57, 2, 26, 14, 3, 30, 63, 3, 32, 1, 5, 11,
		27, 24, 26, 44, 31, 23, 56, 38, 62, 0, 40, 30, 6, 23, 38, 2,
		47, 5, 15, 27, 16, 10, 31, 25, 22, 63, 30, 25, 20, 33, 32, 50,
		29, 43, 55, 10, 50, 45, 56, 20, 4, 7, 27, 46, 11, 16, 22, 52,
		35, 20, 41, 54, 46, 33, 42, 18, 63, 8, 22, 58, 36, 4, 51, 42,
		38, 32, 38, 22, 17, 0, 47, 8, 48, 8, 48, 1, 61, 36, 33, 20,
		24, 39, 39, 18, 30, 36, 9, 43, 42, 24, 10, 58, 4, 15, 34, 52
	},
};

/*************************************************************************
 * Offline region structures
 *************************************************************************/

/** Online group containing number of rules, values, keys and their bins
 * for EFD_MAX_GROUP_NUM_RULES rules.
 */
struct efd_offline_group_rules {
	uint32_t num_rules;
	/**< Sum of the number of rules in all bins assigned to this group. */

	uint32_t key_idx[EFD_MAX_GROUP_NUM_RULES];
	/**< Array with all keys of the group. */
	efd_value_t value[EFD_MAX_GROUP_NUM_RULES];
	/**< Array with all values of the keys of the group. */

	uint8_t bin_id[EFD_MAX_GROUP_NUM_RULES];
	/**< Stores the bin for each correspending key to
	 * avoid having to recompute it
	 */
};

/** Offline chunk record, containing EFD_TARGET_CHUNK_NUM_RULES rules.
 * Those rules are split into EFD_CHUNK_NUM_GROUPS groups per chunk.
 */
struct efd_offline_chunk_rules {
	uint16_t num_rules;
	/**< Number of rules in the entire chunk;
	 * used to detect unbalanced groups
	 */

	struct efd_offline_group_rules group_rules[EFD_CHUNK_NUM_GROUPS];
	/**< Array of all groups in the chunk. */
};

/*************************************************************************
 * Online region structures
 *************************************************************************/

/** Online group containing values for EFD_MAX_GROUP_NUM_RULES rules. */
struct efd_online_group_entry {
	efd_hashfunc_t hash_idx[RTE_EFD_VALUE_NUM_BITS];
	efd_lookuptbl_t lookup_table[RTE_EFD_VALUE_NUM_BITS];
} __attribute__((__packed__));

/**
 * A single chunk record, containing EFD_TARGET_CHUNK_NUM_RULES rules.
 * Those rules are split into EFD_CHUNK_NUM_GROUPS groups per chunk.
 */
struct efd_online_chunk {
	uint8_t bin_choice_list[(EFD_CHUNK_NUM_BINS * 2 + 7) / 8];
	/**< This is a packed indirection index into the 'groups' array.
	 * Each byte contains four two-bit values which index into
	 * the efd_bin_to_group array.
	 * The efd_bin_to_group array returns the index into the groups array
	 */

	struct efd_online_group_entry groups[EFD_CHUNK_NUM_GROUPS];
	/**< Array of all the groups in the chunk. */
} __attribute__((__packed__));

/**
 * EFD table structure
 */
struct rte_efd_table {
	char name[RTE_EFD_NAMESIZE]; /**< Name of the efd table. */

	uint32_t key_len; /**< Length of the key stored offline */

	uint32_t max_num_rules;
	/**< Static maximum number of entries the table was constructed to hold. */

	uint32_t num_rules;
	/**< Number of entries currently in the table . */

	uint32_t num_chunks;
	/**< Number of chunks in the table needed to support num_rules. */

	uint32_t num_chunks_shift;
	/**< Bits to shift to get chunk id, instead of dividing by num_chunk. */

	enum efd_lookup_internal_function lookup_fn;
	/**< Indicates which lookup function to use. */

	struct efd_online_chunk *chunks[RTE_MAX_NUMA_NODES];
	/**< Dynamic array of size num_chunks of chunk records. */

	struct efd_offline_chunk_rules *offline_chunks;
	/**< Dynamic array of size num_chunks of key-value pairs. */

	struct rte_ring *free_slots;
	/**< Ring that stores all indexes of the free slots in the key table */

	uint8_t *keys; /**< Dynamic array of size max_num_rules of keys */
};

/**
 * Computes the chunk ID for a given key hash
 *
 * @param table
 *   EFD table to reference
 * @param hashed_key
 *   32-bit key hash returned by EFD_HASH
 *
 * @return
 *   chunk ID containing this key hash
 */
static inline uint32_t
efd_get_chunk_id(const struct rte_efd_table * const table,
		const uint32_t hashed_key)
{
	return hashed_key & (table->num_chunks - 1);
}

/**
 * Computes the bin ID for a given key hash
 *
 * @param table
 *   EFD table to reference
 * @param hashed_key
 *   32-bit key hash returned by EFD_HASH
 *
 * @return bin ID containing this key hash
 */
static inline uint32_t
efd_get_bin_id(const struct rte_efd_table * const table,
		const uint32_t hashed_key)
{
	return (hashed_key >> table->num_chunks_shift) & (EFD_CHUNK_NUM_BINS - 1);
}

/**
 * Looks up the current permutation choice for a particular bin in the online table
 *
 * @param table
 *  EFD table to reference
 * @param socket_id
 *   Socket ID to use to look up existing values (ideally caller's socket id)
 * @param chunk_id
 *   Chunk ID of bin to look up
 * @param bin_id
 *   Bin ID to look up
 *
 * @return
 *   Currently active permutation choice in the online table
 */
static inline uint8_t
efd_get_choice(const struct rte_efd_table * const table,
		const unsigned int socket_id, const uint32_t chunk_id,
		const uint32_t bin_id)
{
	struct efd_online_chunk *chunk = &table->chunks[socket_id][chunk_id];

	/*
	 * Grab the chunk (byte) that contains the choices
	 * for four neighboring bins.
	 */
	uint8_t choice_chunk =
			chunk->bin_choice_list[bin_id / EFD_CHUNK_NUM_BIN_TO_GROUP_SETS];

	/*
	 * Compute the offset into the chunk that contains
	 * the group_id lookup position
	 */
	int offset = (bin_id & 0x3) * 2;

	/* Extract from the byte just the desired lookup position */
	return (uint8_t) ((choice_chunk >> offset) & 0x3);
}

/**
 * Compute the chunk_id and bin_id for a given key
 *
 * @param table
 *   EFD table to reference
 * @param key
 *   Key to hash and find location of
 * @param chunk_id
 *   Computed chunk ID
 * @param bin_id
 *   Computed bin ID
 *
 */
static inline void
efd_compute_ids(const struct rte_efd_table * const table,
		const void *key, uint32_t * const chunk_id, uint32_t * const bin_id)
{
	/* Compute the position of the entry in the hash table */
	uint32_t h = EFD_HASH(key, table);

	/* Compute the chunk_id where that entry can be found */
	*chunk_id = efd_get_chunk_id(table, h);

	/*
	 * Compute the bin within that chunk where the entry
	 * can be found (0 - 255)
	 */
	*bin_id = efd_get_bin_id(table, h);
}

/**
 * Search for a hash function for a group that satisfies all group results
 */
static inline int
efd_search_hash(struct rte_efd_table * const table,
		const struct efd_offline_group_rules * const off_group,
		struct efd_online_group_entry * const on_group)
{
	efd_hashfunc_t hash_idx;
	efd_hashfunc_t start_hash_idx[RTE_EFD_VALUE_NUM_BITS];
	efd_lookuptbl_t start_lookup_table[RTE_EFD_VALUE_NUM_BITS];

	uint32_t i, j, rule_id;
	uint32_t hash_val_a[EFD_MAX_GROUP_NUM_RULES];
	uint32_t hash_val_b[EFD_MAX_GROUP_NUM_RULES];
	uint32_t hash_val[EFD_MAX_GROUP_NUM_RULES];


	rte_prefetch0(off_group->value);

	/*
	 * Prepopulate the hash_val tables by running the two hash functions
	 * for each provided rule
	 */
	for (i = 0; i < off_group->num_rules; i++) {
		void *key_stored = EFD_KEY(off_group->key_idx[i], table);
		hash_val_b[i] = EFD_HASHFUNCB(key_stored, table);
		hash_val_a[i] = EFD_HASHFUNCA(key_stored, table);
	}

	for (i = 0; i < RTE_EFD_VALUE_NUM_BITS; i++) {
		hash_idx = on_group->hash_idx[i];
		start_hash_idx[i] = hash_idx;
		start_lookup_table[i] = on_group->lookup_table[i];

		do {
			efd_lookuptbl_t lookup_table = 0;
			efd_lookuptbl_t lookup_table_complement = 0;

			for (rule_id = 0; rule_id < off_group->num_rules; rule_id++)
				hash_val[rule_id] = hash_val_a[rule_id] + (hash_idx *
					hash_val_b[rule_id]);

			/*
			 * The goal here is to find a hash function for this
			 * particular bit entry that meets the following criteria:
			 * The most significant bits of the hash result define a
			 * shift into the lookup table where the bit will be stored
			 */

			/* Iterate over each provided rule */
			for (rule_id = 0; rule_id < off_group->num_rules;
					rule_id++) {
				/*
				 * Use the few most significant bits (number based on
				 * EFD_LOOKUPTBL_SIZE) to see what position the
				 * expected bit should be set in the lookup_table
				 */
				uint32_t bucket_idx = hash_val[rule_id] >>
						EFD_LOOKUPTBL_SHIFT;

				/*
				 * Get the current bit of interest.
				 * This only find an appropriate hash function
				 * for one bit at a time of the rule
				 */
				efd_lookuptbl_t expected =
						(off_group->value[rule_id] >> i) & 0x1;

				/*
				 * Add the expected bit (if set) to a map
				 * (lookup_table). Also set its complement
				 * in lookup_table_complement
				 */
				lookup_table |= expected << bucket_idx;
				lookup_table_complement |= (1 - expected)
						<< bucket_idx;

				/*
				 * If ever the hash function of two different
				 * elements result in different values at the
				 * same location in the lookup_table,
				 * the current hash_idx is not valid.
				 */
				if (lookup_table & lookup_table_complement)
					break;
			}

			/*
			 * Check if the previous loop completed without
			 * breaking early
			 */
			if (rule_id == off_group->num_rules) {
				/*
				 * Current hash function worked, store it
				 * for the current group
				 */
				on_group->hash_idx[i] = hash_idx;
				on_group->lookup_table[i] = lookup_table;

				/*
				 * Make sure that the hash function has changed
				 * from the starting value
				 */
				hash_idx = start_hash_idx[i] + 1;
				break;
			}
			hash_idx++;

		} while (hash_idx != start_hash_idx[i]);

		/* Failed to find perfect hash for this group */
		if (hash_idx == start_hash_idx[i]) {
			/*
			 * Restore previous hash_idx and lookup_table
			 * for all value bits
			 */
			for (j = 0; j < i; j++) {
				on_group->hash_idx[j] = start_hash_idx[j];
				on_group->lookup_table[j] = start_lookup_table[j];
			}
			return 1;
		}
	}

	return 0;
}

struct rte_efd_table *
rte_efd_create(const char *name, uint32_t max_num_rules, uint32_t key_len,
		uint8_t online_cpu_socket_bitmask, uint8_t offline_cpu_socket)
{
	struct rte_efd_table *table = NULL;
	uint8_t *key_array = NULL;
	uint32_t num_chunks, num_chunks_shift;
	uint8_t socket_id;
	struct rte_efd_list *efd_list = NULL;
	struct rte_tailq_entry *te;
	uint64_t offline_table_size;
	char ring_name[RTE_RING_NAMESIZE];
	struct rte_ring *r = NULL;
	unsigned int i;

	efd_list = RTE_TAILQ_CAST(rte_efd_tailq.head, rte_efd_list);

	if (online_cpu_socket_bitmask == 0) {
		RTE_LOG(ERR, EFD, "At least one CPU socket must be enabled "
				"in the bitmask\n");
		return NULL;
	}

	if (max_num_rules == 0) {
		RTE_LOG(ERR, EFD, "Max num rules must be higher than 0\n");
		return NULL;
	}

	/*
	 * Compute the minimum number of chunks (smallest power of 2)
	 * that can hold all of the rules
	 */
	if (max_num_rules % EFD_TARGET_CHUNK_NUM_RULES == 0)
		num_chunks = rte_align32pow2(max_num_rules /
			EFD_TARGET_CHUNK_NUM_RULES);
	else
		num_chunks = rte_align32pow2((max_num_rules /
			EFD_TARGET_CHUNK_NUM_RULES) + 1);

	num_chunks_shift = rte_bsf32(num_chunks);

	rte_rwlock_write_lock(RTE_EAL_TAILQ_RWLOCK);

	/*
	 * Guarantee there's no existing: this is normally already checked
	 * by ring creation above
	 */
	TAILQ_FOREACH(te, efd_list, next)
	{
		table = (struct rte_efd_table *) te->data;
		if (strncmp(name, table->name, RTE_EFD_NAMESIZE) == 0)
			break;
	}

	table = NULL;
	if (te != NULL) {
		rte_errno = EEXIST;
		te = NULL;
		goto error_unlock_exit;
	}

	te = rte_zmalloc("EFD_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		RTE_LOG(ERR, EFD, "tailq entry allocation failed\n");
		goto error_unlock_exit;
	}

	/* Create a new EFD table management structure */
	table = rte_zmalloc_socket(NULL,
			sizeof(struct rte_efd_table),
			RTE_CACHE_LINE_SIZE,
			offline_cpu_socket);
	if (table == NULL) {
		RTE_LOG(ERR, EFD, "Allocating EFD table management structure"
				" on socket %u failed\n",
				offline_cpu_socket);
		goto error_unlock_exit;
	}


	RTE_LOG(DEBUG, EFD, "Allocated EFD table management structure "
			"on socket %u\n", offline_cpu_socket);

	table->max_num_rules = num_chunks * EFD_TARGET_CHUNK_MAX_NUM_RULES;
	table->num_rules = 0;
	table->num_chunks = num_chunks;
	table->num_chunks_shift = num_chunks_shift;
	table->key_len = key_len;

	/* key_array */
	key_array = rte_zmalloc_socket(NULL,
			table->max_num_rules * table->key_len,
			RTE_CACHE_LINE_SIZE,
			offline_cpu_socket);
	if (key_array == NULL) {
		RTE_LOG(ERR, EFD, "Allocating key array"
				" on socket %u failed\n",
				offline_cpu_socket);
		goto error_unlock_exit;
	}
	table->keys = key_array;
	snprintf(table->name, sizeof(table->name), "%s", name);

	RTE_LOG(DEBUG, EFD, "Creating an EFD table with %u chunks,"
			" which potentially supports %u entries\n",
			num_chunks, table->max_num_rules);

	/* Make sure all the allocatable table pointers are NULL initially */
	for (socket_id = 0; socket_id < RTE_MAX_NUMA_NODES; socket_id++)
		table->chunks[socket_id] = NULL;
	table->offline_chunks = NULL;

	/*
	 * Allocate one online table per socket specified
	 * in the user-supplied bitmask
	 */
	uint64_t online_table_size = num_chunks * sizeof(struct efd_online_chunk) +
			EFD_NUM_CHUNK_PADDING_BYTES;

	for (socket_id = 0; socket_id < RTE_MAX_NUMA_NODES; socket_id++) {
		if ((online_cpu_socket_bitmask >> socket_id) & 0x01) {
			/*
			 * Allocate all of the EFD table chunks (the online portion)
			 * as a continuous block
			 */
			table->chunks[socket_id] =
				rte_zmalloc_socket(
				NULL,
				online_table_size,
				RTE_CACHE_LINE_SIZE,
				socket_id);
			if (table->chunks[socket_id] == NULL) {
				RTE_LOG(ERR, EFD,
						"Allocating EFD online table on "
						"socket %u failed\n",
						socket_id);
				goto error_unlock_exit;
			}
			RTE_LOG(DEBUG, EFD,
					"Allocated EFD online table of size "
					"%"PRIu64" bytes (%.2f MB) on socket %u\n",
					online_table_size,
					(float) online_table_size /
						(1024.0F * 1024.0F),
					socket_id);
		}
	}

#if defined(RTE_ARCH_X86)
	/*
	 * For less than 4 bits, scalar function performs better
	 * than vectorised version
	 */
	if (RTE_EFD_VALUE_NUM_BITS > 3 && rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2))
		table->lookup_fn = EFD_LOOKUP_AVX2;
	else
#endif
#if defined(RTE_ARCH_ARM64)
	/*
	 * For less than or equal to 16 bits, scalar function performs better
	 * than vectorised version
	 */
	if (RTE_EFD_VALUE_NUM_BITS > 16 &&
	    rte_cpu_get_flag_enabled(RTE_CPUFLAG_NEON))
		table->lookup_fn = EFD_LOOKUP_NEON;
	else
#endif
		table->lookup_fn = EFD_LOOKUP_SCALAR;

	/*
	 * Allocate the EFD table offline portion (with the actual rules
	 * mapping keys to values) as a continuous block.
	 * This could be several gigabytes of memory.
	 */
	offline_table_size = num_chunks * sizeof(struct efd_offline_chunk_rules);
	table->offline_chunks =
			rte_zmalloc_socket(NULL,
			offline_table_size,
			RTE_CACHE_LINE_SIZE,
			offline_cpu_socket);
	if (table->offline_chunks == NULL) {
		RTE_LOG(ERR, EFD, "Allocating EFD offline table on socket %u "
				"failed\n", offline_cpu_socket);
		goto error_unlock_exit;
	}

	RTE_LOG(DEBUG, EFD,
			"Allocated EFD offline table of size %"PRIu64" bytes "
			" (%.2f MB) on socket %u\n", offline_table_size,
			(float) offline_table_size / (1024.0F * 1024.0F),
			offline_cpu_socket);

	te->data = (void *) table;
	TAILQ_INSERT_TAIL(efd_list, te, next);
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);

	snprintf(ring_name, sizeof(ring_name), "HT_%s", table->name);
	/* Create ring (Dummy slot index is not enqueued) */
	r = rte_ring_create(ring_name, rte_align32pow2(table->max_num_rules),
			offline_cpu_socket, 0);
	if (r == NULL) {
		RTE_LOG(ERR, EFD, "memory allocation failed\n");
		goto error_unlock_exit;
	}

	/* Populate free slots ring. Entry zero is reserved for key misses. */
	for (i = 0; i < table->max_num_rules; i++)
		rte_ring_sp_enqueue(r, (void *) ((uintptr_t) i));

	table->free_slots = r;
	return table;

error_unlock_exit:
	rte_rwlock_write_unlock(RTE_EAL_TAILQ_RWLOCK);
	rte_efd_free(table);

	return NULL;
}

struct rte_efd_table *
rte_efd_find_existing(const char *name)
{
	struct rte_efd_table *table = NULL;
	struct rte_tailq_entry *te;
	struct rte_efd_list *efd_list;

	efd_list = RTE_TAILQ_CAST(rte_efd_tailq.head, rte_efd_list);

	rte_rwlock_read_lock(RTE_EAL_TAILQ_RWLOCK);

	TAILQ_FOREACH(te, efd_list, next)
	{
		table = (struct rte_efd_table *) te->data;
		if (strncmp(name, table->name, RTE_EFD_NAMESIZE) == 0)
			break;
	}
	rte_rwlock_read_unlock(RTE_EAL_TAILQ_RWLOCK);

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}
	return table;
}

void
rte_efd_free(struct rte_efd_table *table)
{
	uint8_t socket_id;

	if (table == NULL)
		return;

	for (socket_id = 0; socket_id < RTE_MAX_NUMA_NODES; socket_id++)
		rte_free(table->chunks[socket_id]);

	rte_ring_free(table->free_slots);
	rte_free(table->offline_chunks);
	rte_free(table->keys);
	rte_free(table);
}

/**
 * Applies a previously computed table entry to the specified table for all
 * socket-local copies of the online table.
 * Intended to apply an update for only a single change
 * to a key/value pair at a time
 *
 * @param table
 *   EFD table to reference
 * @param socket_id
 *   Socket ID to use to lookup existing values (ideally caller's socket id)
 * @param chunk_id
 *   Chunk index to update
 * @param group_id
 *   Group index to update
 * @param bin_id
 *   Bin within the group that this update affects
 * @param new_bin_choice
 *   Newly chosen permutation which this bin should use - only lower 2 bits
 * @param new_group_entry
 *   Previously computed updated chunk/group entry
 */
static inline void
efd_apply_update(struct rte_efd_table * const table, const unsigned int socket_id,
		const uint32_t chunk_id, const uint32_t group_id,
		const uint32_t bin_id, const uint8_t new_bin_choice,
		const struct efd_online_group_entry * const new_group_entry)
{
	int i;
	struct efd_online_chunk *chunk = &table->chunks[socket_id][chunk_id];
	uint8_t bin_index = bin_id / EFD_CHUNK_NUM_BIN_TO_GROUP_SETS;

	/*
	 * Grab the current byte that contains the choices
	 * for four neighboring bins
	 */
	uint8_t choice_chunk =
			chunk->bin_choice_list[bin_index];


	/* Compute the offset into the chunk that needs to be updated */
	int offset = (bin_id & 0x3) * 2;

	/* Zero the two bits of interest and set them to new_bin_choice */
	choice_chunk = (choice_chunk & (~(0x03 << offset)))
			| ((new_bin_choice & 0x03) << offset);

	/* Update the online table with the new data across all sockets */
	for (i = 0; i < RTE_MAX_NUMA_NODES; i++) {
		if (table->chunks[i] != NULL) {
			memcpy(&(table->chunks[i][chunk_id].groups[group_id]),
					new_group_entry,
					sizeof(struct efd_online_group_entry));
			table->chunks[i][chunk_id].bin_choice_list[bin_index] =
					choice_chunk;
		}
	}
}

/*
 * Move the bin from prev group to the new group
 */
static inline void
move_groups(uint32_t bin_id, uint8_t bin_size,
		struct efd_offline_group_rules *new_group,
		struct efd_offline_group_rules * const current_group)
{

	uint8_t empty_idx = 0;
	unsigned int i;

	if (new_group == current_group)
		return;

	for (i = 0; i < current_group->num_rules; i++) {
		/*
		 * Move keys that belong to the same bin
		 * to the new group
		 */
		if (current_group->bin_id[i] == bin_id) {
			new_group->key_idx[new_group->num_rules] =
					current_group->key_idx[i];
			new_group->value[new_group->num_rules] =
					current_group->value[i];
			new_group->bin_id[new_group->num_rules] =
					current_group->bin_id[i];
			new_group->num_rules++;
		} else {
			if (i != empty_idx) {
				/*
				 * Need to move this key towards
				 * the top of the array
				 */
				current_group->key_idx[empty_idx] =
						current_group->key_idx[i];
				current_group->value[empty_idx] =
						current_group->value[i];
				current_group->bin_id[empty_idx] =
						current_group->bin_id[i];
			}
			empty_idx++;
		}

	}
	current_group->num_rules -= bin_size;
}

/*
 * Revert group/s to their previous state before
 * trying to insert/add a new key
 */
static inline void
revert_groups(struct efd_offline_group_rules *previous_group,
		struct efd_offline_group_rules *current_group, uint8_t bin_size)
{
	unsigned int i;

	if (current_group == previous_group)
		return;

	/* Move keys back to previous group */
	for (i = current_group->num_rules - bin_size;
			i < current_group->num_rules; i++) {
		previous_group->key_idx[previous_group->num_rules] =
				current_group->key_idx[i];
		previous_group->value[previous_group->num_rules] =
				current_group->value[i];
		previous_group->bin_id[previous_group->num_rules] =
				current_group->bin_id[i];
		previous_group->num_rules++;
	}

	/*
	 * Decrease number of rules after the move
	 * in the new group
	 */
	current_group->num_rules -= bin_size;
}

/**
 * Computes an updated table entry where the supplied key points to a new host.
 * If no entry exists, one is inserted.
 *
 * This function does NOT modify the online table(s)
 * This function DOES modify the offline table
 *
 * @param table
 *   EFD table to reference
 * @param socket_id
 *   Socket ID to use to lookup existing values (ideally caller's socket id)
 * @param key
 *   Key to insert
 * @param value
 *   Value to associate with key
 * @param chunk_id
 *   Chunk ID of the chunk that was modified
 * @param group_id
 *   Group ID of the group that was modified
 * @param bin_id
 *   Bin ID that was modified
 * @param new_bin_choice
 *   Newly chosen permutation which this bin will use
 * @param entry
 *   Newly computed online entry to apply later with efd_apply_update
 *
 * @return
 *   RTE_EFD_UPDATE_WARN_GROUP_FULL
 *     Operation is insert, and the last available space in the
 *     key's group was just used. Future inserts may fail as groups fill up.
 *     This operation was still successful, and entry contains a valid update
 *   RTE_EFD_UPDATE_FAILED
 *     Either the EFD failed to find a suitable perfect hash or the group was full
 *     This is a fatal error, and the table is now in an indeterminate state
 *   RTE_EFD_UPDATE_NO_CHANGE
 *     Operation resulted in no change to the table (same value already exists)
 *   0
 *     Insert or update was successful, and the new efd_online_group_entry
 *     is stored in *entry
 *
 * @warning
 *   Note that entry will be UNCHANGED if the update has no effect, and thus any
 *   subsequent use of the entry content will likely be invalid
 */
static inline int
efd_compute_update(struct rte_efd_table * const table,
		const unsigned int socket_id, const void *key,
		const efd_value_t value, uint32_t * const chunk_id,
		uint32_t * const group_id, uint32_t * const bin_id,
		uint8_t * const new_bin_choice,
		struct efd_online_group_entry * const entry)
{
	unsigned int i;
	int ret;
	uint32_t new_idx;
	void *new_k, *slot_id = NULL;
	int status = EXIT_SUCCESS;
	unsigned int found = 0;

	efd_compute_ids(table, key, chunk_id, bin_id);

	struct efd_offline_chunk_rules * const chunk =
			&table->offline_chunks[*chunk_id];
	struct efd_offline_group_rules *new_group;

	uint8_t current_choice = efd_get_choice(table, socket_id,
			*chunk_id, *bin_id);
	uint32_t current_group_id = efd_bin_to_group[current_choice][*bin_id];
	struct efd_offline_group_rules * const current_group =
			&chunk->group_rules[current_group_id];
	uint8_t bin_size = 0;
	uint8_t key_changed_index = 0;
	efd_value_t key_changed_previous_value = 0;
	uint32_t key_idx_previous = 0;

	/* Scan the current group and see if the key is already present */
	for (i = 0; i < current_group->num_rules; i++) {
		if (current_group->bin_id[i] == *bin_id)
			bin_size++;
		else
			continue;

		void *key_stored = EFD_KEY(current_group->key_idx[i], table);
		if (found == 0 && unlikely(memcmp(key_stored, key,
				table->key_len) == 0)) {
			/* Key is already present */

			/*
			 * If previous value is same as new value,
			 * no additional work is required
			 */
			if (current_group->value[i] == value)
				return RTE_EFD_UPDATE_NO_CHANGE;

			key_idx_previous = current_group->key_idx[i];
			key_changed_previous_value = current_group->value[i];
			key_changed_index = i;
			current_group->value[i] = value;
			found = 1;
		}
	}

	if (found == 0) {
		/* Key does not exist. Insert the rule into the bin/group */
		if (unlikely(current_group->num_rules >= EFD_MAX_GROUP_NUM_RULES)) {
			RTE_LOG(ERR, EFD,
					"Fatal: No room remaining for insert into "
					"chunk %u group %u bin %u\n",
					*chunk_id,
					current_group_id, *bin_id);
			return RTE_EFD_UPDATE_FAILED;
		}

		if (unlikely(current_group->num_rules ==
				(EFD_MAX_GROUP_NUM_RULES - 1))) {
			RTE_LOG(INFO, EFD, "Warn: Insert into last "
					"available slot in chunk %u "
					"group %u bin %u\n", *chunk_id,
					current_group_id, *bin_id);
			status = RTE_EFD_UPDATE_WARN_GROUP_FULL;
		}

		if (rte_ring_sc_dequeue(table->free_slots, &slot_id) != 0)
			return RTE_EFD_UPDATE_FAILED;

		new_k = RTE_PTR_ADD(table->keys, (uintptr_t) slot_id *
					table->key_len);
		rte_prefetch0(new_k);
		new_idx = (uint32_t) ((uintptr_t) slot_id);

		rte_memcpy(EFD_KEY(new_idx, table), key, table->key_len);
		current_group->key_idx[current_group->num_rules] = new_idx;
		current_group->value[current_group->num_rules] = value;
		current_group->bin_id[current_group->num_rules] = *bin_id;
		current_group->num_rules++;
		table->num_rules++;
		bin_size++;
	} else {
		uint32_t last = current_group->num_rules - 1;
		/* Swap the key with the last key inserted*/
		current_group->key_idx[key_changed_index] =
				current_group->key_idx[last];
		current_group->value[key_changed_index] =
				current_group->value[last];
		current_group->bin_id[key_changed_index] =
				current_group->bin_id[last];

		/*
		 * Key to be updated will always be available
		 * at the end of the group
		 */
		current_group->key_idx[last] = key_idx_previous;
		current_group->value[last] = value;
		current_group->bin_id[last] = *bin_id;
	}

	*new_bin_choice = current_choice;
	*group_id = current_group_id;
	new_group = current_group;

	/* Group need to be rebalanced when it starts to get loaded */
	if (current_group->num_rules > EFD_MIN_BALANCED_NUM_RULES) {

		/*
		 * Subtract the number of entries in the bin from
		 * the original group
		 */
		current_group->num_rules -= bin_size;

		/*
		 * Figure out which of the available groups that this bin
		 * can map to is the smallest (using the current group
		 * as baseline)
		 */
		uint8_t smallest_choice = current_choice;
		uint8_t smallest_size = current_group->num_rules;
		uint32_t smallest_group_id = current_group_id;
		unsigned char choice;

		for (choice = 0; choice < EFD_CHUNK_NUM_BIN_TO_GROUP_SETS;
				choice++) {
			uint32_t test_group_id =
					efd_bin_to_group[choice][*bin_id];
			uint32_t num_rules =
					chunk->group_rules[test_group_id].num_rules;
			if (num_rules < smallest_size) {
				smallest_choice = choice;
				smallest_size = num_rules;
				smallest_group_id = test_group_id;
			}
		}

		*new_bin_choice = smallest_choice;
		*group_id = smallest_group_id;
		new_group = &chunk->group_rules[smallest_group_id];
		current_group->num_rules += bin_size;

	}

	uint8_t choice = 0;
	for (;;) {
		if (current_group != new_group &&
				new_group->num_rules + bin_size >
					EFD_MAX_GROUP_NUM_RULES) {
			RTE_LOG(DEBUG, EFD,
					"Unable to move_groups to dest group "
					"containing %u entries."
					"bin_size:%u choice:%02x\n",
					new_group->num_rules, bin_size,
					choice - 1);
			goto next_choice;
		}
		move_groups(*bin_id, bin_size, new_group, current_group);
		/*
		 * Recompute the hash function for the modified group,
		 * and return it to the caller
		 */
		ret = efd_search_hash(table, new_group, entry);

		if (!ret)
			return status;

		RTE_LOG(DEBUG, EFD,
				"Failed to find perfect hash for group "
				"containing %u entries. bin_size:%u choice:%02x\n",
				new_group->num_rules, bin_size, choice - 1);
		/* Restore groups modified to their previous state */
		revert_groups(current_group, new_group, bin_size);

next_choice:
		if (choice == EFD_CHUNK_NUM_BIN_TO_GROUP_SETS)
			break;
		*new_bin_choice = choice;
		*group_id = efd_bin_to_group[choice][*bin_id];
		new_group = &chunk->group_rules[*group_id];
		choice++;
	}

	if (!found) {
		current_group->num_rules--;
		table->num_rules--;
	} else
		current_group->value[current_group->num_rules - 1] =
			key_changed_previous_value;
	return RTE_EFD_UPDATE_FAILED;
}

int
rte_efd_update(struct rte_efd_table * const table, const unsigned int socket_id,
		const void *key, const efd_value_t value)
{
	uint32_t chunk_id = 0, group_id = 0, bin_id = 0;
	uint8_t new_bin_choice = 0;
	struct efd_online_group_entry entry;

	int status = efd_compute_update(table, socket_id, key, value,
			&chunk_id, &group_id, &bin_id,
			&new_bin_choice, &entry);

	if (status == RTE_EFD_UPDATE_NO_CHANGE)
		return EXIT_SUCCESS;

	if (status == RTE_EFD_UPDATE_FAILED)
		return status;

	efd_apply_update(table, socket_id, chunk_id, group_id, bin_id,
			new_bin_choice, &entry);
	return status;
}

int
rte_efd_delete(struct rte_efd_table * const table, const unsigned int socket_id,
		const void *key, efd_value_t * const prev_value)
{
	unsigned int i;
	uint32_t chunk_id, bin_id;
	uint8_t not_found = 1;

	efd_compute_ids(table, key, &chunk_id, &bin_id);

	struct efd_offline_chunk_rules * const chunk =
			&table->offline_chunks[chunk_id];

	uint8_t current_choice = efd_get_choice(table, socket_id,
			chunk_id, bin_id);
	uint32_t current_group_id = efd_bin_to_group[current_choice][bin_id];
	struct efd_offline_group_rules * const current_group =
			&chunk->group_rules[current_group_id];

	/*
	 * Search the current group for the specified key.
	 * If it exists, remove it and re-pack the other values
	 */
	for (i = 0; i < current_group->num_rules; i++) {
		if (not_found) {
			/* Found key that needs to be removed */
			if (memcmp(EFD_KEY(current_group->key_idx[i], table),
					key, table->key_len) == 0) {
				/* Store previous value if requested by caller */
				if (prev_value != NULL)
					*prev_value = current_group->value[i];

				not_found = 0;
				rte_ring_sp_enqueue(table->free_slots,
					(void *)((uintptr_t)current_group->key_idx[i]));
			}
		} else {
			/*
			 * If the desired key has been found,
			 * need to shift other values up one
			 */

			/* Need to shift this entry back up one index */
			current_group->key_idx[i - 1] = current_group->key_idx[i];
			current_group->value[i - 1] = current_group->value[i];
			current_group->bin_id[i - 1] = current_group->bin_id[i];
		}
	}

	if (not_found == 0) {
		table->num_rules--;
		current_group->num_rules--;
	}

	return not_found;
}

static inline efd_value_t
efd_lookup_internal_scalar(const efd_hashfunc_t *group_hash_idx,
		const efd_lookuptbl_t *group_lookup_table,
		const uint32_t hash_val_a, const uint32_t hash_val_b)
{
	efd_value_t value = 0;
	uint32_t i;

	for (i = 0; i < RTE_EFD_VALUE_NUM_BITS; i++) {
		value <<= 1;
		uint32_t h = hash_val_a + (hash_val_b *
			group_hash_idx[RTE_EFD_VALUE_NUM_BITS - i - 1]);
		uint16_t bucket_idx = h >> EFD_LOOKUPTBL_SHIFT;
		value |= (group_lookup_table[
				RTE_EFD_VALUE_NUM_BITS - i - 1] >>
				bucket_idx) & 0x1;
	}

	return value;
}


static inline efd_value_t
efd_lookup_internal(const struct efd_online_group_entry * const group,
		const uint32_t hash_val_a, const uint32_t hash_val_b,
		enum efd_lookup_internal_function lookup_fn)
{
	efd_value_t value = 0;

	switch (lookup_fn) {

#if defined(RTE_ARCH_X86) && defined(CC_SUPPORT_AVX2)
	case EFD_LOOKUP_AVX2:
		return efd_lookup_internal_avx2(group->hash_idx,
					group->lookup_table,
					hash_val_a,
					hash_val_b);
		break;
#endif
#if defined(RTE_ARCH_ARM64)
	case EFD_LOOKUP_NEON:
		return efd_lookup_internal_neon(group->hash_idx,
					group->lookup_table,
					hash_val_a,
					hash_val_b);
		break;
#endif
	case EFD_LOOKUP_SCALAR:
	/* Fall-through */
	default:
		return efd_lookup_internal_scalar(group->hash_idx,
					group->lookup_table,
					hash_val_a,
					hash_val_b);
	}

	return value;
}

efd_value_t
rte_efd_lookup(const struct rte_efd_table * const table,
		const unsigned int socket_id, const void *key)
{
	uint32_t chunk_id, group_id, bin_id;
	uint8_t bin_choice;
	const struct efd_online_group_entry *group;
	const struct efd_online_chunk * const chunks = table->chunks[socket_id];

	/* Determine the chunk and group location for the given key */
	efd_compute_ids(table, key, &chunk_id, &bin_id);
	bin_choice = efd_get_choice(table, socket_id, chunk_id, bin_id);
	group_id = efd_bin_to_group[bin_choice][bin_id];
	group = &chunks[chunk_id].groups[group_id];

	return efd_lookup_internal(group,
			EFD_HASHFUNCA(key, table),
			EFD_HASHFUNCB(key, table),
			table->lookup_fn);
}

void rte_efd_lookup_bulk(const struct rte_efd_table * const table,
		const unsigned int socket_id, const int num_keys,
		const void **key_list, efd_value_t * const value_list)
{
	int i;
	uint32_t chunk_id_list[RTE_EFD_BURST_MAX];
	uint32_t bin_id_list[RTE_EFD_BURST_MAX];
	uint8_t bin_choice_list[RTE_EFD_BURST_MAX];
	uint32_t group_id_list[RTE_EFD_BURST_MAX];
	struct efd_online_group_entry *group;

	struct efd_online_chunk *chunks = table->chunks[socket_id];

	for (i = 0; i < num_keys; i++) {
		efd_compute_ids(table, key_list[i], &chunk_id_list[i],
				&bin_id_list[i]);
		rte_prefetch0(&chunks[chunk_id_list[i]].bin_choice_list);
	}

	for (i = 0; i < num_keys; i++) {
		bin_choice_list[i] = efd_get_choice(table, socket_id,
				chunk_id_list[i], bin_id_list[i]);
		group_id_list[i] =
				efd_bin_to_group[bin_choice_list[i]][bin_id_list[i]];
		group = &chunks[chunk_id_list[i]].groups[group_id_list[i]];
		rte_prefetch0(group);
	}

	for (i = 0; i < num_keys; i++) {
		group = &chunks[chunk_id_list[i]].groups[group_id_list[i]];
		value_list[i] = efd_lookup_internal(group,
				EFD_HASHFUNCA(key_list[i], table),
				EFD_HASHFUNCB(key_list[i], table),
				table->lookup_fn);
	}
}
