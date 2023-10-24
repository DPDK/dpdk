/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

/**
 * @file
 *
 * RTE Membership Library
 *
 * The Membership Library is an extension and generalization of a traditional
 * filter (for example Bloom Filter and cuckoo filter) structure that has
 * multiple usages in a variety of workloads and applications. The library is
 * used to test if a key belongs to certain sets. Two types of such
 * "set-summary" structures are implemented: hash-table based (HT) and vector
 * bloom filter (vBF). For HT setsummary, two subtypes or modes are available,
 * cache and non-cache modes. The table below summarize some properties of
 * the different implementations.
 */

/**
 * <!--
 * +==========+=====================+================+=========================+
 * |   type   |      vbf            |     HT-cache   |     HT-non-cache        |
 * +==========+=====================+==========================================+
 * |structure |  bloom-filter array |  hash-table like without storing key     |
 * +----------+---------------------+------------------------------------------+
 * |set id    | limited by bf count |           [1, 0x7fff]                    |
 * |          | up to 32.           |                                          |
 * +----------+---------------------+------------------------------------------+
 * |usages &  | small set range,    | can delete,    | cache most recent keys, |
 * |properties| user-specified      | big set range, | have both false-positive|
 * |          | false-positive rate,| small false    | and false-negative      |
 * |          | no deletion support.| positive depend| depend on table size,   |
 * |          |                     | on table size, | automatic overwritten.  |
 * |          |                     | new key does   |                         |
 * |          |                     | not overwrite  |                         |
 * |          |                     | existing key.  |                         |
 * +----------+---------------------+----------------+-------------------------+
 * +==========+=============================+
 * |   type   |      sketch                 |
 * +==========+=============================+
 * |structure | counting bloom filter array |
 * +----------+-----------------------------+
 * |set id    | 1: heavy set, 0: light set  |
 * |          |                             |
 * +----------+-----------------------------+
 * |usages &  | count size of a flow,       |
 * |properties| used for heavy hitter       |
 * |          | detection.                  |
 * +----------+-----------------------------+
 * -->
 */

#ifndef _RTE_MEMBER_H_
#define _RTE_MEMBER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include <rte_common.h>

/** The set ID type that stored internally in hash table based set summary. */
typedef uint16_t member_set_t;
/** Invalid set ID used to mean no match found. */
#define RTE_MEMBER_NO_MATCH 0
/** Maximum size of hash table that can be created. */
#define RTE_MEMBER_ENTRIES_MAX (1 << 30)
/** Maximum number of keys that can be searched as a bulk */
#define RTE_MEMBER_LOOKUP_BULK_MAX 64
/** Entry count per bucket in hash table based mode. */
#define RTE_MEMBER_BUCKET_ENTRIES 16
/** Maximum number of characters in setsum name. */
#define RTE_MEMBER_NAMESIZE 32
/** Max value of the random number */
#define RTE_RAND_MAX      ~0LLU
/**
 * As packets skipped in the sampling-based algorithm, the accounting
 * results accuracy is not guaranteed in the start stage. There should
 * be a "convergence time" to achieve the accuracy after receiving enough
 * packets.
 * For sketch, use the flag if prefer always bounded mode, which only
 * starts sampling after receiving enough packets to keep the results
 * accuracy always bounded.
 */
#define RTE_MEMBER_SKETCH_ALWAYS_BOUNDED 0x01
/** For sketch, use the flag if to count packet size instead of packet count */
#define RTE_MEMBER_SKETCH_COUNT_BYTE 0x02

/** @internal Hash function used by membership library. */
#if defined(RTE_ARCH_X86) || defined(__ARM_FEATURE_CRC32)
#include <rte_hash_crc.h>
#define MEMBER_HASH_FUNC       rte_hash_crc
#else
#include <rte_jhash.h>
#define MEMBER_HASH_FUNC       rte_jhash
#endif

extern int librte_member_logtype;

#define RTE_MEMBER_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, \
		librte_member_logtype, \
		RTE_FMT("%s(): " RTE_FMT_HEAD(__VA_ARGS__,), \
			__func__, \
			RTE_FMT_TAIL(__VA_ARGS__,)))

/** @internal setsummary structure. */
struct rte_member_setsum;

/**
 * Parameter struct used to create set summary
 */
struct rte_member_parameters;

/**
 * Define different set summary types
 */
enum rte_member_setsum_type {
	RTE_MEMBER_TYPE_HT = 0,  /**< Hash table based set summary. */
	RTE_MEMBER_TYPE_VBF,     /**< Vector of bloom filters. */
	RTE_MEMBER_TYPE_SKETCH,
	RTE_MEMBER_NUM_TYPE
};

/** @internal compare function for different arch. */
enum rte_member_sig_compare_function {
	RTE_MEMBER_COMPARE_SCALAR = 0,
	RTE_MEMBER_COMPARE_AVX2,
	RTE_MEMBER_COMPARE_NUM
};

/* sketch update function with different implementations. */
typedef void (*sketch_update_fn_t)(const struct rte_member_setsum *ss,
				   const void *key,
				   uint32_t count);

/* sketch lookup function with different implementations. */
typedef uint64_t (*sketch_lookup_fn_t)(const struct rte_member_setsum *ss,
				       const void *key);

/* sketch delete function with different implementations. */
typedef void (*sketch_delete_fn_t)(const struct rte_member_setsum *ss,
				   const void *key);

/** @internal setsummary structure. */
struct rte_member_setsum {
	enum rte_member_setsum_type type; /* Type of the set summary. */
	uint32_t key_len;		/* Length of key. */
	uint32_t prim_hash_seed;	/* Primary hash function seed. */
	uint32_t sec_hash_seed;		/* Secondary hash function seed. */

	/* Hash table based. */
	uint32_t bucket_cnt;		/* Number of buckets. */
	uint32_t bucket_mask;		/* Bit mask to get bucket index. */
	/* For runtime selecting AVX, scalar, etc for signature comparison. */
	enum rte_member_sig_compare_function sig_cmp_fn;
	uint8_t cache;			/* If it is cache mode for ht based. */

	/* Vector bloom filter. */
	uint32_t num_set;		/* Number of set (bf) in vbf. */
	uint32_t bits;			/* Number of bits in each bf. */
	uint32_t bit_mask;	/* Bit mask to get bit location in bf. */
	uint32_t num_hashes;	/* Number of hash values to index bf. */

	/* Parameters for sketch */
	float error_rate;
	float sample_rate;
	uint32_t num_col;
	uint32_t num_row;
	int always_bounded;
	double converge_thresh;
	uint32_t topk;
	uint32_t count_byte;
	uint64_t *hash_seeds;
	sketch_update_fn_t sketch_update; /* Pointer to the sketch update function */
	sketch_lookup_fn_t sketch_lookup; /* Pointer to the sketch lookup function */
	sketch_delete_fn_t sketch_delete; /* Pointer to the sketch delete function */

	void *runtime_var;
	uint32_t mul_shift;  /* vbf internal variable used during bit test. */
	uint32_t div_shift;  /* vbf internal variable used during bit test. */

	void *table;	/* This is the handler of hash table or vBF array. */


	/* Second cache line should start here. */
	uint32_t socket_id;          /* NUMA Socket ID for memory. */
	char name[RTE_MEMBER_NAMESIZE]; /* Name of this set summary. */
#ifdef RTE_ARCH_X86
	bool use_avx512;
#endif
} __rte_cache_aligned;

/**
 * Parameters used when create the set summary table. Currently user can
 * specify two types of setsummary: HT based and vBF. For HT based, user can
 * specify cache or non-cache mode. Here is a table to describe some differences
 */
struct rte_member_parameters {
	const char *name;			/**< Name of the hash. */

	/**
	 * User to specify the type of the setsummary from one of
	 * rte_member_setsum_type.
	 *
	 * HT based setsummary is implemented like a hash table. User should use
	 * this type when there are many sets.
	 *
	 * vBF setsummary is a vector of bloom filters. It is used when number
	 * of sets is not big (less than 32 for current implementation).
	 */
	enum rte_member_setsum_type type;

	/**
	 * is_cache is only used for HT based setsummary.
	 *
	 * If it is HT based setsummary, user to specify the subtype or mode
	 * of the setsummary. It could be cache, or non-cache mode.
	 * Set is_cache to be 1 if to use as cache mode.
	 *
	 * For cache mode, keys can be evicted out of the HT setsummary. Keys
	 * with the same signature and map to the same bucket
	 * will overwrite each other in the setsummary table.
	 * This mode is useful for the case that the set-summary only
	 * needs to keep record of the recently inserted keys. Both
	 * false-negative and false-positive could happen.
	 *
	 * For non-cache mode, keys cannot be evicted out of the cache. So for
	 * this mode the setsummary will become full eventually. Keys with the
	 * same signature but map to the same bucket will still occupy multiple
	 * entries. This mode does not give false-negative result.
	 */
	uint8_t is_cache;

	/**
	 * For HT setsummary, num_keys equals to the number of entries of the
	 * table. When the number of keys inserted in the HT setsummary
	 * approaches this number, eviction could happen. For cache mode,
	 * keys could be evicted out of the table. For non-cache mode, keys will
	 * be evicted to other buckets like cuckoo hash. The table will also
	 * likely to become full before the number of inserted keys equal to the
	 * total number of entries.
	 *
	 * For vBF, num_keys equal to the expected number of keys that will
	 * be inserted into the vBF. The implementation assumes the keys are
	 * evenly distributed to each BF in vBF. This is used to calculate the
	 * number of bits we need for each BF. User does not specify the size of
	 * each BF directly because the optimal size depends on the num_keys
	 * and false positive rate.
	 */
	uint32_t num_keys;

	/**
	 * The length of key is used for hash calculation. Since key is not
	 * stored in set-summary, large key does not require more memory space.
	 */
	uint32_t key_len;

	/**
	 * num_set is only used for vBF, but not used for HT setsummary.
	 *
	 * num_set is equal to the number of BFs in vBF. For current
	 * implementation, it only supports 1,2,4,8,16,32 BFs in one vBF set
	 * summary. If other number of sets are needed, for example 5, the user
	 * should allocate the minimum available value that larger than 5,
	 * which is 8.
	 */
	uint32_t num_set;

	/**
	 * false_positive_rate is only used for vBF, but not used for HT
	 * setsummary.
	 *
	 * For vBF, false_positive_rate is the user-defined false positive rate
	 * given expected number of inserted keys (num_keys). It is used to
	 * calculate the total number of bits for each BF, and the number of
	 * hash values used during lookup and insertion. For details please
	 * refer to vBF implementation and membership library documentation.
	 *
	 * For HT, This parameter is not directly set by users.
	 * HT setsummary's false positive rate is in the order of:
	 * false_pos = (1/bucket_count)*(1/2^16), since we use 16-bit signature.
	 * This is because two keys needs to map to same bucket and same
	 * signature to have a collision (false positive). bucket_count is equal
	 * to number of entries (num_keys) divided by entry count per bucket
	 * (RTE_MEMBER_BUCKET_ENTRIES). Thus, the false_positive_rate is not
	 * directly set by users for HT mode.
	 */
	float false_positive_rate;

	/**
	 * We use two seeds to calculate two independent hashes for each key.
	 *
	 * For HT type, one hash is used as signature, and the other is used
	 * for bucket location.
	 * For vBF type, these two hashes and their combinations are used as
	 * hash locations to index the bit array.
	 * For Sketch type, these seeds are not used.
	 */
	uint32_t prim_hash_seed;

	/**
	 * The secondary seed should be a different value from the primary seed.
	 */
	uint32_t sec_hash_seed;

	/**
	 * For count(min) sketch data structure, error rate defines the accuracy
	 * required by the user. Higher accuracy leads to more memory usage, but
	 * the flow size is estimated more accurately.
	 */
	float error_rate;

	/**
	 * Sampling rate means the internal sample rate of the rows of the count
	 * min sketches. Lower sampling rate can reduce CPU overhead, but the
	 * data structure will require more time to converge statistically.
	 */
	float sample_rate;

	/**
	 * How many top heavy hitter to be reported. The library will internally
	 * keep the keys of heavy hitters for final report.
	 */
	uint32_t top_k;

	/**
	 * Extra flags that may passed in by user
	 */
	uint32_t extra_flag;

	int socket_id;			/**< NUMA Socket ID for memory. */
} __rte_cache_aligned;

/**
 * Find an existing set-summary and return a pointer to it.
 *
 * @param name
 *   Name of the set-summary.
 * @return
 *   Pointer to the set-summary or NULL if object not found
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - ENOENT - value not available for return
 */
struct rte_member_setsum *
rte_member_find_existing(const char *name);

/**
 * Create set-summary (SS).
 *
 * @param params
 *   Parameters to initialize the setsummary.
 * @return
 *   Return the pointer to the setsummary.
 *   Return value is NULL if the creation failed.
 */
struct rte_member_setsum *
rte_member_create(const struct rte_member_parameters *params);

/**
 * Lookup key in set-summary (SS).
 * Single key lookup and return as soon as the first match found
 *
 * @param setsum
 *   Pointer of a setsummary.
 * @param key
 *   Pointer of the key to be looked up.
 * @param set_id
 *   Output the set id matches the key.
 * @return
 *   Return 1 for found a match and 0 for not found a match.
 */
int
rte_member_lookup(const struct rte_member_setsum *setsum, const void *key,
			member_set_t *set_id);

/**
 * Lookup bulk of keys in set-summary (SS).
 * Each key lookup returns as soon as the first match found
 *
 * @param setsum
 *   Pointer of a setsummary.
 * @param keys
 *   Pointer of the bulk of keys to be looked up.
 * @param num_keys
 *   Number of keys that will be lookup.
 * @param set_ids
 *   Output set ids for all the keys to this array.
 *   User should preallocate array that can contain all results, which size is
 *   the num_keys.
 * @return
 *   The number of keys that found a match.
 */
int
rte_member_lookup_bulk(const struct rte_member_setsum *setsum,
			const void **keys, uint32_t num_keys,
			member_set_t *set_ids);

/**
 * Lookup a key in set-summary (SS) for multiple matches.
 * The key lookup will find all matched entries (multiple match).
 * Note that for cache mode of HT, each key can have at most one match. This is
 * because keys with same signature that maps to same bucket will overwrite
 * each other. So multi-match lookup should be used for vBF and non-cache HT.
 *
 * @param setsum
 *   Pointer of a set-summary.
 * @param key
 *   Pointer of the key that to be looked up.
 * @param max_match_per_key
 *   User specified maximum number of matches for each key. The function returns
 *   as soon as this number of matches found for the key.
 * @param set_id
 *   Output set ids for all the matches of the key. User needs to preallocate
 *   the array that can contain max_match_per_key number of results.
 * @return
 *   The number of matches that found for the key.
 *   For cache mode HT set-summary, the number should be at most 1.
 */
int
rte_member_lookup_multi(const struct rte_member_setsum *setsum,
		const void *key, uint32_t max_match_per_key,
		member_set_t *set_id);

/**
 * Lookup a bulk of keys in set-summary (SS) for multiple matches each key.
 * Each key lookup will find all matched entries (multiple match).
 * Note that for cache mode HT, each key can have at most one match. So
 * multi-match function is mainly used for vBF and non-cache mode HT.
 *
 * @param setsum
 *   Pointer of a setsummary.
 * @param keys
 *   Pointer of the keys to be looked up.
 * @param num_keys
 *   The number of keys that will be lookup.
 * @param max_match_per_key
 *   The possible maximum number of matches for each key.
 * @param match_count
 *   Output the number of matches for each key in an array.
 * @param set_ids
 *   Return set ids for all the matches of all keys. Users pass in a
 *   preallocated 2D array with first dimension as key index and second
 *   dimension as match index. For example set_ids[bulk_size][max_match_per_key]
 * @return
 *   The number of keys that found one or more matches in the set-summary.
 */
int
rte_member_lookup_multi_bulk(const struct rte_member_setsum *setsum,
		const void **keys, uint32_t num_keys,
		uint32_t max_match_per_key,
		uint32_t *match_count,
		member_set_t *set_ids);

/**
 * Insert key into set-summary (SS).
 *
 * @param setsum
 *   Pointer of a set-summary.
 * @param key
 *   Pointer of the key to be added.
 * @param set_id
 *   The set id associated with the key that needs to be added. Different mode
 *   supports different set_id ranges. 0 cannot be used as set_id since
 *   RTE_MEMBER_NO_MATCH by default is set as 0.
 *   For HT mode, the set_id has range as [1, 0x7FFF], MSB is reserved.
 *   For vBF mode the set id is limited by the num_set parameter when create
 *   the set-summary. For sketch mode, this id is ignored.
 * @return
 *   HT (cache mode) and vBF should never fail unless the set_id is not in the
 *   valid range. In such case -EINVAL is returned.
 *   For HT (non-cache mode) it could fail with -ENOSPC error code when table is
 *   full.
 *   For success it returns different values for different modes to provide
 *   extra information for users.
 *   Return 0 for HT (cache mode) if the add does not cause
 *   eviction, return 1 otherwise. Return 0 for non-cache mode if success,
 *   -ENOSPC for full, and 1 if cuckoo eviction happens.
 *   Always returns 0 for vBF mode and sketch.
 */
int
rte_member_add(const struct rte_member_setsum *setsum, const void *key,
			member_set_t set_id);

/**
 * Add the packet byte size into the sketch.
 *
 * @param setsum
 *   Pointer of a set-summary.
 * @param key
 *   Pointer of the key to be added.
 * @param byte_count
 *   Add the byte count of the packet into the sketch.
 * @return
 * Return -EINVAL for invalid parameters, otherwise return 0.
 */
int
rte_member_add_byte_count(const struct rte_member_setsum *setsum,
			  const void *key, uint32_t byte_count);

/**
 * Query packet count for a certain flow-key.
 *
 * @param setsum
 *   Pointer of a set-summary.
 * @param key
 *   Pointer of the key to be added.
 * @param count
 *   The output packet count or byte count.
 * @return
 *   Return -EINVAL for invalid parameters.
 */
int
rte_member_query_count(const struct rte_member_setsum *setsum,
		       const void *key, uint64_t *count);


/**
 * Report heavyhitter flow-keys into set-summary (SS).
 *
 * @param setsum
 *   Pointer of a set-summary.
 * @param keys
 *   Pointer of the output top-k key array.
 * @param counts
 *   Pointer of the output packet count or byte count array of the top-k keys.
 * @return
 *   Return -EINVAL for invalid parameters. Return a positive integer indicate
 *   how many heavy hitters are reported.
 */
int
rte_member_report_heavyhitter(const struct rte_member_setsum *setsum,
			      void **keys, uint64_t *counts);


/**
 * De-allocate memory used by set-summary.
 *
 * @param setsum
 *   Pointer to the set summary.
 *   If setsum is NULL, no operation is performed.
 */
void
rte_member_free(struct rte_member_setsum *setsum);

/**
 * Reset the set-summary tables. E.g. reset bits to be 0 in BF,
 * reset set_id in each entry to be RTE_MEMBER_NO_MATCH in HT based SS.
 *
 * @param setsum
 *   Pointer to the set-summary.
 */
void
rte_member_reset(const struct rte_member_setsum *setsum);

/**
 * Delete items from the set-summary. Note that vBF does not support deletion
 * in current implementation. For vBF, error code of -EINVAL will be returned.
 *
 * @param setsum
 *   Pointer to the set-summary.
 * @param key
 *   Pointer of the key to be deleted.
 * @param set_id
 *   For HT mode, we need both key and its corresponding set_id to
 *   properly delete the key. Without set_id, we may delete other keys with the
 *   same signature.
 * @return
 *   If no entry found to delete, an error code of -ENOENT could be returned.
 */
int
rte_member_delete(const struct rte_member_setsum *setsum, const void *key,
			member_set_t set_id);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MEMBER_H_ */
