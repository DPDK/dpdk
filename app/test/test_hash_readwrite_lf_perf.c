/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Arm Limited
 */

#include <inttypes.h>
#include <locale.h>

#include <rte_cycles.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_jhash.h>
#include <rte_launch.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_spinlock.h>

#include "test.h"

#ifndef RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF
#define RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF 0
#endif

#define BULK_LOOKUP_SIZE 32

#define RUN_WITH_HTM_DISABLED 0

#if (RUN_WITH_HTM_DISABLED)

#define TOTAL_ENTRY (5*1024)
#define TOTAL_INSERT (5*1024)

#else

#define TOTAL_ENTRY (4*1024*1024)
#define TOTAL_INSERT (4*1024*1024)

#endif

#define READ_FAIL 1
#define READ_PASS_NO_KEY_SHIFTS 2
#define READ_PASS_SHIFT_PATH 4
#define READ_PASS_NON_SHIFT_PATH 8
#define BULK_LOOKUP 16
#define READ_PASS_KEY_SHIFTS_EXTBKT 32

#define WRITE_NO_KEY_SHIFT 0
#define WRITE_KEY_SHIFT 1
#define WRITE_EXT_BKT 2

#define NUM_TEST 3

#define QSBR_REPORTING_INTERVAL 1024

static unsigned int rwc_core_cnt[NUM_TEST] = {1, 2, 4};

struct rwc_perf {
	uint32_t w_no_ks_r_hit[2][NUM_TEST];
	uint32_t w_no_ks_r_miss[2][NUM_TEST];
	uint32_t w_ks_r_hit_nsp[2][NUM_TEST];
	uint32_t w_ks_r_hit_sp[2][NUM_TEST];
	uint32_t w_ks_r_miss[2][NUM_TEST];
	uint32_t multi_rw[NUM_TEST - 1][2][NUM_TEST];
	uint32_t w_ks_r_hit_extbkt[2][NUM_TEST];
	uint32_t writer_add_del[NUM_TEST];
};

static struct rwc_perf rwc_lf_results, rwc_non_lf_results;

static struct {
	uint32_t *keys;
	uint32_t *keys_no_ks;
	uint32_t *keys_ks;
	uint32_t *keys_absent;
	uint32_t *keys_shift_path;
	uint32_t *keys_non_shift_path;
	uint32_t *keys_ext_bkt;
	uint32_t *keys_ks_extbkt;
	uint32_t count_keys_no_ks;
	uint32_t count_keys_ks;
	uint32_t count_keys_absent;
	uint32_t count_keys_shift_path;
	uint32_t count_keys_non_shift_path;
	uint32_t count_keys_extbkt;
	uint32_t count_keys_ks_extbkt;
	uint32_t single_insert;
	struct rte_hash *h;
} tbl_rwc_test_param;

static uint64_t gread_cycles;
static uint64_t greads;
static uint64_t gwrite_cycles;
static uint64_t gwrites;

static volatile uint8_t writer_done;

static uint16_t enabled_core_ids[RTE_MAX_LCORE];

static uint8_t *scanned_bkts;

static inline uint16_t
get_short_sig(const hash_sig_t hash)
{
	return hash >> 16;
}

static inline uint32_t
get_prim_bucket_index(__rte_unused const struct rte_hash *h,
		      const hash_sig_t hash)
{
	uint32_t num_buckets;
	uint32_t bucket_bitmask;
	num_buckets  = rte_align32pow2(TOTAL_ENTRY) / 8;
	bucket_bitmask = num_buckets - 1;
	return hash & bucket_bitmask;
}

static inline uint32_t
get_alt_bucket_index(__rte_unused const struct rte_hash *h,
			uint32_t cur_bkt_idx, uint16_t sig)
{
	uint32_t num_buckets;
	uint32_t bucket_bitmask;
	num_buckets  = rte_align32pow2(TOTAL_ENTRY) / 8;
	bucket_bitmask = num_buckets - 1;
	return (cur_bkt_idx ^ sig) & bucket_bitmask;
}


static inline int
get_enabled_cores_list(void)
{
	uint32_t i = 0;
	uint16_t core_id;
	uint32_t max_cores = rte_lcore_count();
	RTE_LCORE_FOREACH(core_id) {
		enabled_core_ids[i] = core_id;
		i++;
	}

	if (i != max_cores) {
		printf("Number of enabled cores in list is different from "
			"number given by rte_lcore_count()\n");
		return -1;
	}
	return 0;
}

static int
init_params(int rwc_lf, int use_jhash, int htm, int ext_bkt)
{
	struct rte_hash *handle;

	struct rte_hash_parameters hash_params = {
		.entries = TOTAL_ENTRY,
		.key_len = sizeof(uint32_t),
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};

	if (use_jhash)
		hash_params.hash_func = rte_jhash;
	else
		hash_params.hash_func = rte_hash_crc;

	if (rwc_lf)
		hash_params.extra_flag =
			RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF |
			RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD;
	else if (htm)
		hash_params.extra_flag =
			RTE_HASH_EXTRA_FLAGS_TRANS_MEM_SUPPORT |
			RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY |
			RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD;
	else
		hash_params.extra_flag =
			RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY |
			RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD;

	if (ext_bkt)
		hash_params.extra_flag |= RTE_HASH_EXTRA_FLAGS_EXT_TABLE;

	hash_params.name = "tests";

	handle = rte_hash_create(&hash_params);
	if (handle == NULL) {
		printf("hash creation failed");
		return -1;
	}

	tbl_rwc_test_param.h = handle;
	return 0;
}

static inline int
check_bucket(uint32_t bkt_idx, uint32_t key)
{
	uint32_t iter;
	uint32_t prev_iter;
	uint32_t diff;
	uint32_t count = 0;
	const void *next_key;
	void *next_data;

	/* Temporary bucket to hold the keys */
	uint32_t keys_in_bkt[8];

	iter = bkt_idx * 8;
	prev_iter = iter;
	while (rte_hash_iterate(tbl_rwc_test_param.h,
			&next_key, &next_data, &iter) >= 0) {

		/* Check for duplicate entries */
		if (*(const uint32_t *)next_key == key)
			return 1;

		/* Identify if there is any free entry in the bucket */
		diff = iter - prev_iter;
		if (diff > 1)
			break;

		prev_iter = iter;
		keys_in_bkt[count] = *(const uint32_t *)next_key;
		count++;

		/* All entries in the bucket are occupied */
		if (count == 8) {

			/*
			 * Check if bucket was not scanned before, to avoid
			 * duplicate keys.
			 */
			if (scanned_bkts[bkt_idx] == 0) {
				/*
				 * Since this bucket (pointed to by bkt_idx) is
				 * full, it is likely that key(s) in this
				 * bucket will be on the shift path, when
				 * collision occurs. Thus, add it to
				 * keys_shift_path.
				 */
				memcpy(tbl_rwc_test_param.keys_shift_path +
					tbl_rwc_test_param.count_keys_shift_path
					, keys_in_bkt, 32);
				tbl_rwc_test_param.count_keys_shift_path += 8;
				scanned_bkts[bkt_idx] = 1;
			}
			return -1;
		}
	}
	return 0;
}

static int
generate_keys(void)
{
	uint32_t *keys = NULL;
	uint32_t *keys_no_ks = NULL;
	uint32_t *keys_ks = NULL;
	uint32_t *keys_absent = NULL;
	uint32_t *keys_non_shift_path = NULL;
	uint32_t *keys_ext_bkt = NULL;
	uint32_t *keys_ks_extbkt = NULL;
	uint32_t *found = NULL;
	uint32_t count_keys_no_ks = 0;
	uint32_t count_keys_ks = 0;
	uint32_t count_keys_extbkt = 0;
	uint32_t i;

	if (init_params(0, 0, 0, 0) != 0)
		return -1;

	/*
	 * keys will consist of a) keys whose addition to the hash table
	 * will result in shifting of the existing keys to their alternate
	 * locations b) keys whose addition to the hash table will not result
	 * in shifting of the existing keys.
	 */
	keys = rte_malloc(NULL, sizeof(uint32_t) * TOTAL_INSERT, 0);
	if (keys == NULL) {
		printf("RTE_MALLOC failed\n");
		goto err;
	}

	/*
	 * keys_no_ks (no key-shifts): Subset of 'keys' - consists of keys  that
	 * will NOT result in shifting of the existing keys to their alternate
	 * locations. Roughly around 900K keys.
	 */
	keys_no_ks = rte_malloc(NULL, sizeof(uint32_t) * TOTAL_INSERT, 0);
	if (keys_no_ks == NULL) {
		printf("RTE_MALLOC failed\n");
		goto err;
	}

	/*
	 * keys_ks (key-shifts): Subset of 'keys' - consists of keys that will
	 * result in shifting of the existing keys to their alternate locations.
	 * Roughly around 146K keys. There might be repeating keys. More code is
	 * required to filter out these keys which will complicate the test case
	 */
	keys_ks = rte_malloc(NULL, sizeof(uint32_t) * TOTAL_INSERT, 0);
	if (keys_ks == NULL) {
		printf("RTE_MALLOC failed\n");
		goto err;
	}

	/* Used to identify keys not inserted in the hash table */
	found = rte_zmalloc(NULL, sizeof(uint32_t) * TOTAL_INSERT, 0);
	if (found == NULL) {
		printf("RTE_MALLOC failed\n");
		goto err;
	}

	/*
	 * This consist of keys not inserted to the hash table.
	 * Used to test perf of lookup on keys that do not exist in the table.
	 */
	keys_absent = rte_malloc(NULL, sizeof(uint32_t) * TOTAL_INSERT, 0);
	if (keys_absent == NULL) {
		printf("RTE_MALLOC failed\n");
		goto err;
	}

	/*
	 * This consist of keys which are likely to be on the shift
	 * path (i.e. being moved to alternate location), when collision occurs
	 * on addition of a key to an already full primary bucket.
	 * Used to test perf of lookup on keys that are on the shift path.
	 */
	tbl_rwc_test_param.keys_shift_path = rte_malloc(NULL, sizeof(uint32_t) *
							TOTAL_INSERT, 0);
	if (tbl_rwc_test_param.keys_shift_path == NULL) {
		printf("RTE_MALLOC failed\n");
		goto err;
	}

	/*
	 * This consist of keys which are never on the shift
	 * path (i.e. being moved to alternate location), when collision occurs
	 * on addition of a key to an already full primary bucket.
	 * Used to test perf of lookup on keys that are not on the shift path.
	 */
	keys_non_shift_path = rte_malloc(NULL, sizeof(uint32_t) * TOTAL_INSERT,
					 0);
	if (keys_non_shift_path == NULL) {
		printf("RTE_MALLOC failed\n");
		goto err;
	}

	/*
	 * This consist of keys which will be stored in extended buckets
	 */
	keys_ext_bkt = rte_malloc(NULL, sizeof(uint32_t) * TOTAL_INSERT, 0);
	if (keys_ext_bkt == NULL) {
		printf("RTE_MALLOC failed\n");
		goto err;
	}

	/*
	 * This consist of keys which when deleted causes shifting of keys
	 * in extended buckets to respective secondary buckets
	 */
	keys_ks_extbkt = rte_malloc(NULL, sizeof(uint32_t) * TOTAL_INSERT, 0);
	if (keys_ks_extbkt == NULL) {
		printf("RTE_MALLOC failed\n");
		goto err;
	}

	hash_sig_t sig;
	uint32_t prim_bucket_idx;
	uint32_t sec_bucket_idx;
	uint16_t short_sig;
	uint32_t num_buckets;
	num_buckets  = rte_align32pow2(TOTAL_ENTRY) / 8;
	int ret;

	/*
	 * Used to mark bkts in which at least one key was shifted to its
	 * alternate location
	 */
	scanned_bkts = rte_malloc(NULL, sizeof(uint8_t) * num_buckets, 0);
	if (scanned_bkts == NULL) {
		printf("RTE_MALLOC failed\n");
		goto err;
	}

	tbl_rwc_test_param.keys = keys;
	tbl_rwc_test_param.keys_no_ks = keys_no_ks;
	tbl_rwc_test_param.keys_ks = keys_ks;
	tbl_rwc_test_param.keys_absent = keys_absent;
	tbl_rwc_test_param.keys_non_shift_path = keys_non_shift_path;
	tbl_rwc_test_param.keys_ext_bkt = keys_ext_bkt;
	tbl_rwc_test_param.keys_ks_extbkt = keys_ks_extbkt;
	/* Generate keys by adding previous two keys, neglect overflow */
	printf("Generating keys...\n");
	keys[0] = 0;
	keys[1] = 1;
	for (i = 2; i < TOTAL_INSERT; i++)
		keys[i] = keys[i-1] + keys[i-2];

	/* Segregate keys into keys_no_ks and keys_ks */
	for (i = 0; i < TOTAL_INSERT; i++) {
		/* Check if primary bucket has space.*/
		sig = rte_hash_hash(tbl_rwc_test_param.h,
					tbl_rwc_test_param.keys+i);
		prim_bucket_idx = get_prim_bucket_index(tbl_rwc_test_param.h,
							sig);
		ret = check_bucket(prim_bucket_idx, keys[i]);
		if (ret < 0) {
			/*
			 * Primary bucket is full, this key will result in
			 * shifting of the keys to their alternate locations.
			 */
			keys_ks[count_keys_ks] = keys[i];
			count_keys_ks++;
		} else if (ret == 0) {
			/*
			 * Primary bucket has space, this key will not result in
			 * shifting of the keys. Hence, add key to the table.
			 */
			ret = rte_hash_add_key_data(tbl_rwc_test_param.h,
							keys+i,
							(void *)((uintptr_t)i));
			if (ret < 0) {
				printf("writer failed %"PRIu32"\n", i);
				break;
			}
			keys_no_ks[count_keys_no_ks] = keys[i];
			count_keys_no_ks++;
		}
	}

	for (i = 0; i < count_keys_no_ks; i++) {
		/*
		 * Identify keys in keys_no_ks with value less than
		 * 4M (HTM enabled) OR 5K (HTM disabled)
		 */
		if (keys_no_ks[i] < TOTAL_INSERT)
			found[keys_no_ks[i]]++;
	}

	for (i = 0; i < count_keys_ks; i++) {
		/*
		 * Identify keys in keys_ks with value less than
		 * 4M (HTM enabled) OR 5K (HTM disabled)
		 */
		if (keys_ks[i] < TOTAL_INSERT)
			found[keys_ks[i]]++;
	}

	uint32_t count_keys_absent = 0;
	for (i = 0; i < TOTAL_INSERT; i++) {
		/*
		 * Identify missing keys between 0 and
		 * 4M (HTM enabled) OR 5K (HTM disabled)
		 */
		if (found[i] == 0)
			keys_absent[count_keys_absent++] = i;
	}

	/* Find keys that will not be on the shift path */
	uint32_t iter;
	const void *next_key;
	void *next_data;
	uint32_t count = 0;
	for (i = 0; i < num_buckets; i++) {
		/* Check bucket for no keys shifted to alternate locations */
		if (scanned_bkts[i] == 0) {
			iter = i * 8;
			while (rte_hash_iterate(tbl_rwc_test_param.h,
				&next_key, &next_data, &iter) >= 0) {

				/* Check if key belongs to the current bucket */
				if (i >= (iter-1)/8)
					keys_non_shift_path[count++]
						= *(const uint32_t *)next_key;
				else
					break;
			}
		}
	}

	tbl_rwc_test_param.count_keys_no_ks = count_keys_no_ks;
	tbl_rwc_test_param.count_keys_ks = count_keys_ks;
	tbl_rwc_test_param.count_keys_absent = count_keys_absent;
	tbl_rwc_test_param.count_keys_non_shift_path = count;

	memset(scanned_bkts, 0, num_buckets);
	count = 0;
	/* Find keys that will be in extended buckets */
	for (i = 0; i < count_keys_ks; i++) {
		ret = rte_hash_add_key(tbl_rwc_test_param.h, keys_ks + i);
		if (ret < 0) {
			/* Key will be added to ext bkt */
			keys_ext_bkt[count_keys_extbkt++] = keys_ks[i];
			/* Sec bkt to be added to keys_ks_extbkt */
			sig = rte_hash_hash(tbl_rwc_test_param.h,
					tbl_rwc_test_param.keys_ks + i);
			prim_bucket_idx = get_prim_bucket_index(
						tbl_rwc_test_param.h, sig);
			short_sig = get_short_sig(sig);
			sec_bucket_idx = get_alt_bucket_index(
						tbl_rwc_test_param.h,
						prim_bucket_idx, short_sig);
			if (scanned_bkts[sec_bucket_idx] == 0)
				scanned_bkts[sec_bucket_idx] = 1;
		}
	}

	/* Find keys that will shift keys in ext bucket*/
	for (i = 0; i < num_buckets; i++) {
		if (scanned_bkts[i] == 1) {
			iter = i * 8;
			while (rte_hash_iterate(tbl_rwc_test_param.h,
				&next_key, &next_data, &iter) >= 0) {
				/* Check if key belongs to the current bucket */
				if (i >= (iter-1)/8)
					keys_ks_extbkt[count++]
						= *(const uint32_t *)next_key;
				else
					break;
			}
		}
	}

	tbl_rwc_test_param.count_keys_ks_extbkt = count;
	tbl_rwc_test_param.count_keys_extbkt = count_keys_extbkt;

	printf("\nCount of keys NOT causing shifting of existing keys to "
	"alternate location: %d\n", tbl_rwc_test_param.count_keys_no_ks);
	printf("\nCount of keys causing shifting of existing keys to alternate "
		"locations: %d\n\n", tbl_rwc_test_param.count_keys_ks);
	printf("Count of absent keys that will never be added to the hash "
		"table: %d\n\n", tbl_rwc_test_param.count_keys_absent);
	printf("Count of keys likely to be on the shift path: %d\n\n",
	       tbl_rwc_test_param.count_keys_shift_path);
	printf("Count of keys not likely to be on the shift path: %d\n\n",
	       tbl_rwc_test_param.count_keys_non_shift_path);
	printf("Count of keys in extended buckets: %d\n\n",
	       tbl_rwc_test_param.count_keys_extbkt);
	printf("Count of keys shifting keys in ext buckets: %d\n\n",
	       tbl_rwc_test_param.count_keys_ks_extbkt);

	rte_free(found);
	rte_free(scanned_bkts);
	rte_hash_free(tbl_rwc_test_param.h);
	return 0;

err:
	rte_free(keys);
	rte_free(keys_no_ks);
	rte_free(keys_ks);
	rte_free(keys_absent);
	rte_free(found);
	rte_free(tbl_rwc_test_param.keys_shift_path);
	rte_free(keys_non_shift_path);
	rte_free(keys_ext_bkt);
	rte_free(keys_ks_extbkt);
	rte_free(scanned_bkts);
	rte_hash_free(tbl_rwc_test_param.h);
	return -1;
}

static int
test_rwc_reader(__rte_unused void *arg)
{
	uint32_t i, j;
	int ret;
	uint64_t begin, cycles;
	uint32_t loop_cnt = 0;
	uint8_t read_type = (uint8_t)((uintptr_t)arg);
	uint32_t read_cnt;
	uint32_t *keys;
	uint32_t extra_keys;
	int32_t pos[BULK_LOOKUP_SIZE];
	void *temp_a[BULK_LOOKUP_SIZE];

	if (read_type & READ_FAIL) {
		keys = tbl_rwc_test_param.keys_absent;
		read_cnt = tbl_rwc_test_param.count_keys_absent;
	} else if (read_type & READ_PASS_NO_KEY_SHIFTS) {
		keys = tbl_rwc_test_param.keys_no_ks;
		read_cnt = tbl_rwc_test_param.count_keys_no_ks;
	} else if (read_type & READ_PASS_SHIFT_PATH) {
		keys = tbl_rwc_test_param.keys_shift_path;
		read_cnt = tbl_rwc_test_param.count_keys_shift_path;
	} else if (read_type & READ_PASS_KEY_SHIFTS_EXTBKT) {
		keys = tbl_rwc_test_param.keys_ext_bkt;
		read_cnt = tbl_rwc_test_param.count_keys_extbkt;
	} else {
		keys = tbl_rwc_test_param.keys_non_shift_path;
		read_cnt = tbl_rwc_test_param.count_keys_non_shift_path;
	}

	extra_keys = read_cnt & (BULK_LOOKUP_SIZE - 1);

	begin = rte_rdtsc_precise();
	do {
		if (read_type & BULK_LOOKUP) {
			for (i = 0; i < (read_cnt - extra_keys);
			     i += BULK_LOOKUP_SIZE) {
				/* Array of  pointer to the list of keys */
				for (j = 0; j < BULK_LOOKUP_SIZE; j++)
					temp_a[j] = keys + i + j;
				rte_hash_lookup_bulk(tbl_rwc_test_param.h,
						   (const void **)
						   ((uintptr_t)temp_a),
						   BULK_LOOKUP_SIZE, pos);
				/* Validate lookup result */
				for (j = 0; j < BULK_LOOKUP_SIZE; j++)
					if ((read_type & READ_FAIL &&
					     pos[j] != -ENOENT) ||
					    (!(read_type & READ_FAIL) &&
					     pos[j] == -ENOENT)) {
						printf("lookup failed!"
						       "%"PRIu32"\n",
						       keys[i + j]);
						return -1;
					}
			}
			for (j = 0; j < extra_keys; j++)
				temp_a[j] = keys + i + j;

			rte_hash_lookup_bulk(tbl_rwc_test_param.h,
					   (const void **)
					   ((uintptr_t)temp_a),
					   extra_keys, pos);
			for (j = 0; j < extra_keys; j++)
				if ((read_type & READ_FAIL &&
				     pos[j] != -ENOENT) ||
				    (!(read_type & READ_FAIL) &&
				     pos[j] == -ENOENT)) {
					printf("lookup failed! %"PRIu32"\n",
					       keys[i + j]);
					return -1;
				}
		} else {
			for (i = 0; i < read_cnt; i++) {
				ret = rte_hash_lookup
					(tbl_rwc_test_param.h, keys + i);
				if (((read_type & READ_FAIL) &&
				     (ret != -ENOENT)) ||
				    (!(read_type & READ_FAIL) &&
					ret == -ENOENT)) {
					printf("lookup failed! %"PRIu32"\n",
					       keys[i]);
					return -1;
				}
			}
		}
		loop_cnt++;
	} while (!writer_done);

	cycles = rte_rdtsc_precise() - begin;
	__atomic_fetch_add(&gread_cycles, cycles, __ATOMIC_RELAXED);
	__atomic_fetch_add(&greads, read_cnt*loop_cnt, __ATOMIC_RELAXED);
	return 0;
}

static int
write_keys(uint8_t write_type)
{
	uint32_t i;
	int ret;
	uint32_t key_cnt = 0;
	uint32_t *keys;
	if (write_type == WRITE_KEY_SHIFT) {
		key_cnt = tbl_rwc_test_param.count_keys_ks;
		keys = tbl_rwc_test_param.keys_ks;
	} else if (write_type == WRITE_NO_KEY_SHIFT) {
		key_cnt = tbl_rwc_test_param.count_keys_no_ks;
		keys = tbl_rwc_test_param.keys_no_ks;
	} else if (write_type == WRITE_EXT_BKT) {
		key_cnt = tbl_rwc_test_param.count_keys_extbkt;
		keys = tbl_rwc_test_param.keys_ext_bkt;
	}
	for (i = 0; i < key_cnt; i++) {
		ret = rte_hash_add_key(tbl_rwc_test_param.h, keys + i);
		if ((write_type == WRITE_NO_KEY_SHIFT) && ret < 0) {
			printf("writer failed %"PRIu32"\n", i);
			return -1;
		}
	}
	return 0;
}

static int
test_rwc_multi_writer(__rte_unused void *arg)
{
	uint32_t i, offset;
	uint32_t pos_core = (uint32_t)((uintptr_t)arg);
	offset = pos_core * tbl_rwc_test_param.single_insert;
	for (i = offset; i < offset + tbl_rwc_test_param.single_insert; i++)
		rte_hash_add_key(tbl_rwc_test_param.h,
				 tbl_rwc_test_param.keys_ks + i);
	return 0;
}

/*
 * Test lookup perf:
 * Reader(s) lookup keys present in the table.
 */
static int
test_hash_add_no_ks_lookup_hit(struct rwc_perf *rwc_perf_results, int rwc_lf,
				int htm, int ext_bkt)
{
	unsigned int n, m;
	uint64_t i;
	int use_jhash = 0;
	uint8_t write_type = WRITE_NO_KEY_SHIFT;
	uint8_t read_type = READ_PASS_NO_KEY_SHIFTS;

	if (init_params(rwc_lf, use_jhash, htm, ext_bkt) != 0)
		goto err;
	printf("\nTest: Hash add - no key-shifts, read - hit\n");
	for (m = 0; m < 2; m++) {
		if (m == 1) {
			printf("\n** With bulk-lookup **\n");
			read_type |= BULK_LOOKUP;
		}
		for (n = 0; n < NUM_TEST; n++) {
			unsigned int tot_lcore = rte_lcore_count();
			if (tot_lcore < rwc_core_cnt[n] + 1)
				goto finish;

			printf("\nNumber of readers: %u\n", rwc_core_cnt[n]);

			__atomic_store_n(&greads, 0, __ATOMIC_RELAXED);
			__atomic_store_n(&gread_cycles, 0, __ATOMIC_RELAXED);

			rte_hash_reset(tbl_rwc_test_param.h);
			writer_done = 0;
			if (write_keys(write_type) < 0)
				goto err;
			writer_done = 1;
			for (i = 1; i <= rwc_core_cnt[n]; i++)
				rte_eal_remote_launch(test_rwc_reader,
						(void *)(uintptr_t)read_type,
							enabled_core_ids[i]);

			for (i = 1; i <= rwc_core_cnt[n]; i++)
				if (rte_eal_wait_lcore(enabled_core_ids[i]) < 0)
					goto err;

			unsigned long long cycles_per_lookup =
				__atomic_load_n(&gread_cycles, __ATOMIC_RELAXED)
				/ __atomic_load_n(&greads, __ATOMIC_RELAXED);
			rwc_perf_results->w_no_ks_r_hit[m][n]
						= cycles_per_lookup;
			printf("Cycles per lookup: %llu\n", cycles_per_lookup);
		}
	}

finish:
	rte_hash_free(tbl_rwc_test_param.h);
	return 0;

err:
	rte_eal_mp_wait_lcore();
	rte_hash_free(tbl_rwc_test_param.h);
	return -1;
}

/*
 * Test lookup perf:
 * Reader(s) lookup keys absent in the table while
 * 'Main' thread adds with no key-shifts.
 */
static int
test_hash_add_no_ks_lookup_miss(struct rwc_perf *rwc_perf_results, int rwc_lf,
				int htm, int ext_bkt)
{
	unsigned int n, m;
	uint64_t i;
	int use_jhash = 0;
	uint8_t write_type = WRITE_NO_KEY_SHIFT;
	uint8_t read_type = READ_FAIL;
	int ret;

	if (init_params(rwc_lf, use_jhash, htm, ext_bkt) != 0)
		goto err;
	printf("\nTest: Hash add - no key-shifts, Hash lookup - miss\n");
	for (m = 0; m < 2; m++) {
		if (m == 1) {
			printf("\n** With bulk-lookup **\n");
			read_type |= BULK_LOOKUP;
		}
		for (n = 0; n < NUM_TEST; n++) {
			unsigned int tot_lcore = rte_lcore_count();
			if (tot_lcore < rwc_core_cnt[n] + 1)
				goto finish;

			printf("\nNumber of readers: %u\n", rwc_core_cnt[n]);

			__atomic_store_n(&greads, 0, __ATOMIC_RELAXED);
			__atomic_store_n(&gread_cycles, 0, __ATOMIC_RELAXED);

			rte_hash_reset(tbl_rwc_test_param.h);
			writer_done = 0;

			for (i = 1; i <= rwc_core_cnt[n]; i++)
				rte_eal_remote_launch(test_rwc_reader,
						(void *)(uintptr_t)read_type,
							enabled_core_ids[i]);
			ret = write_keys(write_type);
			writer_done = 1;

			if (ret < 0)
				goto err;
			for (i = 1; i <= rwc_core_cnt[n]; i++)
				if (rte_eal_wait_lcore(enabled_core_ids[i]) < 0)
					goto err;

			unsigned long long cycles_per_lookup =
				__atomic_load_n(&gread_cycles, __ATOMIC_RELAXED)
				/ __atomic_load_n(&greads, __ATOMIC_RELAXED);
			rwc_perf_results->w_no_ks_r_miss[m][n]
						= cycles_per_lookup;
			printf("Cycles per lookup: %llu\n", cycles_per_lookup);
		}
	}

finish:
	rte_hash_free(tbl_rwc_test_param.h);
	return 0;

err:
	rte_eal_mp_wait_lcore();
	rte_hash_free(tbl_rwc_test_param.h);
	return -1;
}

/*
 * Test lookup perf:
 * Reader(s) lookup keys present in the table and not likely to be on the
 * shift path  while 'Main' thread adds keys causing key-shifts.
 */
static int
test_hash_add_ks_lookup_hit_non_sp(struct rwc_perf *rwc_perf_results,
				    int rwc_lf, int htm, int ext_bkt)
{
	unsigned int n, m;
	uint64_t i;
	int use_jhash = 0;
	int ret;
	uint8_t write_type;
	uint8_t read_type = READ_PASS_NON_SHIFT_PATH;

	if (init_params(rwc_lf, use_jhash, htm, ext_bkt) != 0)
		goto err;
	printf("\nTest: Hash add - key shift, Hash lookup - hit"
	       " (non-shift-path)\n");
	for (m = 0; m < 2; m++) {
		if (m == 1) {
			printf("\n** With bulk-lookup **\n");
			read_type |= BULK_LOOKUP;
		}
		for (n = 0; n < NUM_TEST; n++) {
			unsigned int tot_lcore = rte_lcore_count();
			if (tot_lcore < rwc_core_cnt[n] + 1)
				goto finish;

			printf("\nNumber of readers: %u\n", rwc_core_cnt[n]);

			__atomic_store_n(&greads, 0, __ATOMIC_RELAXED);
			__atomic_store_n(&gread_cycles, 0, __ATOMIC_RELAXED);

			rte_hash_reset(tbl_rwc_test_param.h);
			writer_done = 0;
			write_type = WRITE_NO_KEY_SHIFT;
			if (write_keys(write_type) < 0)
				goto err;
			for (i = 1; i <= rwc_core_cnt[n]; i++)
				rte_eal_remote_launch(test_rwc_reader,
						(void *)(uintptr_t)read_type,
							enabled_core_ids[i]);
			write_type = WRITE_KEY_SHIFT;
			ret = write_keys(write_type);
			writer_done = 1;

			if (ret < 0)
				goto err;
			for (i = 1; i <= rwc_core_cnt[n]; i++)
				if (rte_eal_wait_lcore(enabled_core_ids[i]) < 0)
					goto err;

			unsigned long long cycles_per_lookup =
				__atomic_load_n(&gread_cycles, __ATOMIC_RELAXED)
				/ __atomic_load_n(&greads, __ATOMIC_RELAXED);
			rwc_perf_results->w_ks_r_hit_nsp[m][n]
						= cycles_per_lookup;
			printf("Cycles per lookup: %llu\n", cycles_per_lookup);
		}
	}

finish:
	rte_hash_free(tbl_rwc_test_param.h);
	return 0;

err:
	rte_eal_mp_wait_lcore();
	rte_hash_free(tbl_rwc_test_param.h);
	return -1;
}

/*
 * Test lookup perf:
 * Reader(s) lookup keys present in the table and likely on the shift-path while
 * 'Main' thread adds keys causing key-shifts.
 */
static int
test_hash_add_ks_lookup_hit_sp(struct rwc_perf *rwc_perf_results, int rwc_lf,
				int htm, int ext_bkt)
{
	unsigned int n, m;
	uint64_t i;
	int use_jhash = 0;
	int ret;
	uint8_t write_type;
	uint8_t read_type = READ_PASS_SHIFT_PATH;

	if (init_params(rwc_lf, use_jhash, htm, ext_bkt) != 0)
		goto err;
	printf("\nTest: Hash add - key shift, Hash lookup - hit (shift-path)"
	       "\n");

	for (m = 0; m < 2; m++) {
		if (m == 1) {
			printf("\n** With bulk-lookup **\n");
			read_type |= BULK_LOOKUP;
		}
		for (n = 0; n < NUM_TEST; n++) {
			unsigned int tot_lcore = rte_lcore_count();
			if (tot_lcore < rwc_core_cnt[n] + 1)
				goto finish;

			printf("\nNumber of readers: %u\n", rwc_core_cnt[n]);

			__atomic_store_n(&greads, 0, __ATOMIC_RELAXED);
			__atomic_store_n(&gread_cycles, 0, __ATOMIC_RELAXED);

			rte_hash_reset(tbl_rwc_test_param.h);
			writer_done = 0;
			write_type = WRITE_NO_KEY_SHIFT;
			if (write_keys(write_type) < 0)
				goto err;
			for (i = 1; i <= rwc_core_cnt[n]; i++)
				rte_eal_remote_launch(test_rwc_reader,
						(void *)(uintptr_t)read_type,
						enabled_core_ids[i]);
			write_type = WRITE_KEY_SHIFT;
			ret = write_keys(write_type);
			writer_done = 1;

			if (ret < 0)
				goto err;
			for (i = 1; i <= rwc_core_cnt[n]; i++)
				if (rte_eal_wait_lcore(enabled_core_ids[i]) < 0)
					goto err;

			unsigned long long cycles_per_lookup =
				__atomic_load_n(&gread_cycles, __ATOMIC_RELAXED)
				/ __atomic_load_n(&greads, __ATOMIC_RELAXED);
			rwc_perf_results->w_ks_r_hit_sp[m][n]
						= cycles_per_lookup;
			printf("Cycles per lookup: %llu\n", cycles_per_lookup);
		}
	}

finish:
	rte_hash_free(tbl_rwc_test_param.h);
	return 0;

err:
	rte_eal_mp_wait_lcore();
	rte_hash_free(tbl_rwc_test_param.h);
	return -1;
}

/*
 * Test lookup perf:
 * Reader(s) lookup keys absent in the table while
 * 'Main' thread adds keys causing key-shifts.
 */
static int
test_hash_add_ks_lookup_miss(struct rwc_perf *rwc_perf_results, int rwc_lf, int
			     htm, int ext_bkt)
{
	unsigned int n, m;
	uint64_t i;
	int use_jhash = 0;
	int ret;
	uint8_t write_type;
	uint8_t read_type = READ_FAIL;

	if (init_params(rwc_lf, use_jhash, htm, ext_bkt) != 0)
		goto err;
	printf("\nTest: Hash add - key shift, Hash lookup - miss\n");
	for (m = 0; m < 2; m++) {
		if (m == 1) {
			printf("\n** With bulk-lookup **\n");
			read_type |= BULK_LOOKUP;
		}
		for (n = 0; n < NUM_TEST; n++) {
			unsigned int tot_lcore = rte_lcore_count();
			if (tot_lcore < rwc_core_cnt[n] + 1)
				goto finish;

			printf("\nNumber of readers: %u\n", rwc_core_cnt[n]);

			__atomic_store_n(&greads, 0, __ATOMIC_RELAXED);
			__atomic_store_n(&gread_cycles, 0, __ATOMIC_RELAXED);

			rte_hash_reset(tbl_rwc_test_param.h);
			writer_done = 0;
			write_type = WRITE_NO_KEY_SHIFT;
			if (write_keys(write_type) < 0)
				goto err;
			for (i = 1; i <= rwc_core_cnt[n]; i++)
				rte_eal_remote_launch(test_rwc_reader,
						(void *)(uintptr_t)read_type,
							enabled_core_ids[i]);
			write_type = WRITE_KEY_SHIFT;
			ret = write_keys(write_type);
			writer_done = 1;

			if (ret < 0)
				goto err;
			for (i = 1; i <= rwc_core_cnt[n]; i++)
				if (rte_eal_wait_lcore(enabled_core_ids[i]) < 0)
					goto err;

			unsigned long long cycles_per_lookup =
				__atomic_load_n(&gread_cycles, __ATOMIC_RELAXED)
				/ __atomic_load_n(&greads, __ATOMIC_RELAXED);
			rwc_perf_results->w_ks_r_miss[m][n] = cycles_per_lookup;
			printf("Cycles per lookup: %llu\n", cycles_per_lookup);
		}
	}

finish:
	rte_hash_free(tbl_rwc_test_param.h);
	return 0;

err:
	rte_eal_mp_wait_lcore();
	rte_hash_free(tbl_rwc_test_param.h);
	return -1;
}

/*
 * Test lookup perf for multi-writer:
 * Reader(s) lookup keys present in the table and likely on the shift-path while
 * Writers add keys causing key-shiftsi.
 * Writers are running in parallel, on different data plane cores.
 */
static int
test_hash_multi_add_lookup(struct rwc_perf *rwc_perf_results, int rwc_lf,
			   int htm, int ext_bkt)
{
	unsigned int n, m, k;
	uint64_t i;
	int use_jhash = 0;
	uint8_t write_type;
	uint8_t read_type = READ_PASS_SHIFT_PATH;

	if (init_params(rwc_lf, use_jhash, htm, ext_bkt) != 0)
		goto err;
	printf("\nTest: Multi-add-lookup\n");
	uint8_t pos_core;
	for (m = 1; m < NUM_TEST; m++) {
		/* Calculate keys added by each writer */
		tbl_rwc_test_param.single_insert =
			tbl_rwc_test_param.count_keys_ks / rwc_core_cnt[m];
		for (k = 0; k < 2; k++) {
			if (k == 1) {
				printf("\n** With bulk-lookup **\n");
				read_type |= BULK_LOOKUP;
			}
			for (n = 0; n < NUM_TEST; n++) {
				unsigned int tot_lcore	= rte_lcore_count();
				if (tot_lcore < (rwc_core_cnt[n] +
				     rwc_core_cnt[m] + 1))
					goto finish;

				printf("\nNumber of writers: %u",
				       rwc_core_cnt[m]);
				printf("\nNumber of readers: %u\n",
				       rwc_core_cnt[n]);

				__atomic_store_n(&greads, 0, __ATOMIC_RELAXED);
				__atomic_store_n(&gread_cycles, 0,
						 __ATOMIC_RELAXED);

				rte_hash_reset(tbl_rwc_test_param.h);
				writer_done = 0;
				write_type = WRITE_NO_KEY_SHIFT;
				if (write_keys(write_type) < 0)
					goto err;

				/* Launch reader(s) */
				for (i = 1; i <= rwc_core_cnt[n]; i++)
					rte_eal_remote_launch(test_rwc_reader,
						(void *)(uintptr_t)read_type,
						enabled_core_ids[i]);
				write_type = WRITE_KEY_SHIFT;
				pos_core = 0;

				/* Launch writers */
				for (; i <= rwc_core_cnt[m]
				     + rwc_core_cnt[n];	i++) {
					rte_eal_remote_launch
						(test_rwc_multi_writer,
						(void *)(uintptr_t)pos_core,
						enabled_core_ids[i]);
					pos_core++;
				}

				/* Wait for writers to complete */
				for (i = rwc_core_cnt[n] + 1;
				     i <= rwc_core_cnt[m] + rwc_core_cnt[n];
				     i++)
					rte_eal_wait_lcore(enabled_core_ids[i]);

				writer_done = 1;

				for (i = 1; i <= rwc_core_cnt[n]; i++)
					if (rte_eal_wait_lcore(enabled_core_ids[i]) < 0)
						goto err;

				unsigned long long cycles_per_lookup =
					__atomic_load_n(&gread_cycles,
							__ATOMIC_RELAXED) /
					__atomic_load_n(&greads,
							  __ATOMIC_RELAXED);
				rwc_perf_results->multi_rw[m][k][n]
					= cycles_per_lookup;
				printf("Cycles per lookup: %llu\n",
				       cycles_per_lookup);
			}
		}
	}

finish:
	rte_hash_free(tbl_rwc_test_param.h);
	return 0;

err:
	rte_eal_mp_wait_lcore();
	rte_hash_free(tbl_rwc_test_param.h);
	return -1;
}

/*
 * Test lookup perf:
 * Reader(s) lookup keys present in the extendable bkt.
 */
static int
test_hash_add_ks_lookup_hit_extbkt(struct rwc_perf *rwc_perf_results,
				int rwc_lf, int htm, int ext_bkt)
{
	unsigned int n, m;
	uint64_t i;
	int use_jhash = 0;
	uint8_t write_type;
	uint8_t read_type = READ_PASS_KEY_SHIFTS_EXTBKT;

	if (init_params(rwc_lf, use_jhash, htm, ext_bkt) != 0)
		goto err;
	printf("\nTest: Hash add - key-shifts, read - hit (ext_bkt)\n");
	for (m = 0; m < 2; m++) {
		if (m == 1) {
			printf("\n** With bulk-lookup **\n");
			read_type |= BULK_LOOKUP;
		}
		for (n = 0; n < NUM_TEST; n++) {
			unsigned int tot_lcore = rte_lcore_count();
			if (tot_lcore < rwc_core_cnt[n] + 1)
				goto finish;

			printf("\nNumber of readers: %u\n", rwc_core_cnt[n]);

			__atomic_store_n(&greads, 0, __ATOMIC_RELAXED);
			__atomic_store_n(&gread_cycles, 0, __ATOMIC_RELAXED);

			rte_hash_reset(tbl_rwc_test_param.h);
			write_type = WRITE_NO_KEY_SHIFT;
			if (write_keys(write_type) < 0)
				goto err;
			write_type = WRITE_KEY_SHIFT;
			if (write_keys(write_type) < 0)
				goto err;
			writer_done = 0;
			for (i = 1; i <= rwc_core_cnt[n]; i++)
				rte_eal_remote_launch(test_rwc_reader,
						(void *)(uintptr_t)read_type,
							enabled_core_ids[i]);
			for (i = 0; i < tbl_rwc_test_param.count_keys_ks_extbkt;
			     i++) {
				if (rte_hash_del_key(tbl_rwc_test_param.h,
					tbl_rwc_test_param.keys_ks_extbkt + i)
							< 0) {
					printf("Delete Failed: %u\n",
					tbl_rwc_test_param.keys_ks_extbkt[i]);
					goto err;
				}
			}
			writer_done = 1;

			for (i = 1; i <= rwc_core_cnt[n]; i++)
				if (rte_eal_wait_lcore(enabled_core_ids[i]) < 0)
					goto err;

			unsigned long long cycles_per_lookup =
				__atomic_load_n(&gread_cycles, __ATOMIC_RELAXED)
				/ __atomic_load_n(&greads, __ATOMIC_RELAXED);
			rwc_perf_results->w_ks_r_hit_extbkt[m][n]
						= cycles_per_lookup;
			printf("Cycles per lookup: %llu\n", cycles_per_lookup);
		}
	}

finish:
	rte_hash_free(tbl_rwc_test_param.h);
	return 0;

err:
	rte_eal_mp_wait_lcore();
	rte_hash_free(tbl_rwc_test_param.h);
	return -1;
}

static struct rte_rcu_qsbr *rv;

/*
 * Reader thread using rte_hash data structure with RCU
 */
static int
test_hash_rcu_qsbr_reader(void *arg)
{
	unsigned int i, j;
	uint32_t num_keys = tbl_rwc_test_param.count_keys_no_ks
				- QSBR_REPORTING_INTERVAL;
	uint32_t *keys = tbl_rwc_test_param.keys_no_ks;
	uint32_t lcore_id = rte_lcore_id();
	RTE_SET_USED(arg);

	(void)rte_rcu_qsbr_thread_register(rv, lcore_id);
	rte_rcu_qsbr_thread_online(rv, lcore_id);
	do {
		for (i = 0; i < num_keys; i += j) {
			for (j = 0; j < QSBR_REPORTING_INTERVAL; j++)
				rte_hash_lookup(tbl_rwc_test_param.h,
						keys + i + j);
			/* Update quiescent state counter */
			rte_rcu_qsbr_quiescent(rv, lcore_id);
		}
	} while (!writer_done);
	rte_rcu_qsbr_thread_offline(rv, lcore_id);
	(void)rte_rcu_qsbr_thread_unregister(rv, lcore_id);

	return 0;
}

/*
 * Writer thread using rte_hash data structure with RCU
 */
static int
test_hash_rcu_qsbr_writer(void *arg)
{
	uint32_t i, offset;
	uint64_t begin, cycles;
	uint8_t pos_core = (uint32_t)((uintptr_t)arg);
	offset = pos_core * tbl_rwc_test_param.single_insert;

	begin = rte_rdtsc_precise();
	for (i = offset; i < offset + tbl_rwc_test_param.single_insert; i++) {
		/* Delete element from the shared data structure */
		rte_hash_del_key(tbl_rwc_test_param.h,
					tbl_rwc_test_param.keys_no_ks + i);
		rte_hash_add_key(tbl_rwc_test_param.h,
				tbl_rwc_test_param.keys_no_ks + i);
	}
	cycles = rte_rdtsc_precise() - begin;
	__atomic_fetch_add(&gwrite_cycles, cycles, __ATOMIC_RELAXED);
	__atomic_fetch_add(&gwrites, tbl_rwc_test_param.single_insert,
			   __ATOMIC_RELAXED);
	return 0;
}

/*
 * Writer perf test with RCU QSBR in DQ mode:
 * Writer(s) delete and add keys in the table.
 * Readers lookup keys in the hash table
 */
static int
test_hash_rcu_qsbr_writer_perf(struct rwc_perf *rwc_perf_results, int rwc_lf,
				int htm, int ext_bkt)
{
	unsigned int n;
	uint64_t i;
	uint8_t write_type;
	int use_jhash = 0;
	struct rte_hash_rcu_config rcu_config = {0};
	uint32_t sz;
	uint8_t pos_core;

	printf("\nTest: Writer perf with integrated RCU\n");

	if (init_params(rwc_lf, use_jhash, htm, ext_bkt) != 0)
		goto err;

	sz = rte_rcu_qsbr_get_memsize(RTE_MAX_LCORE);
	rv = (struct rte_rcu_qsbr *)rte_zmalloc(NULL, sz, RTE_CACHE_LINE_SIZE);
	rcu_config.v = rv;

	if (rte_hash_rcu_qsbr_add(tbl_rwc_test_param.h, &rcu_config) < 0) {
		printf("RCU init in hash failed\n");
		goto err;
	}

	for (n = 0; n < NUM_TEST; n++) {
		unsigned int tot_lcore = rte_lcore_count();
		if (tot_lcore < rwc_core_cnt[n] + 3)
			goto finish;

		/* Calculate keys added by each writer */
		tbl_rwc_test_param.single_insert =
			tbl_rwc_test_param.count_keys_no_ks /
				rwc_core_cnt[n];
		printf("\nNumber of writers: %u\n", rwc_core_cnt[n]);

		__atomic_store_n(&gwrites, 0, __ATOMIC_RELAXED);
		__atomic_store_n(&gwrite_cycles, 0, __ATOMIC_RELAXED);

		rte_hash_reset(tbl_rwc_test_param.h);
		rte_rcu_qsbr_init(rv, RTE_MAX_LCORE);

		write_type = WRITE_NO_KEY_SHIFT;
		if (write_keys(write_type) < 0)
			goto err;
		write_type = WRITE_KEY_SHIFT;
		if (write_keys(write_type) < 0)
			goto err;

		/* Launch 2 readers */
		for (i = 1; i <= 2; i++)
			rte_eal_remote_launch(test_hash_rcu_qsbr_reader, NULL,
					      enabled_core_ids[i]);
		pos_core = 0;
		/* Launch writer(s) */
		for (; i <= rwc_core_cnt[n] + 2; i++) {
			rte_eal_remote_launch(test_hash_rcu_qsbr_writer,
				(void *)(uintptr_t)pos_core,
				enabled_core_ids[i]);
			pos_core++;
		}

		/* Wait for writers to complete */
		for (i = 3; i <= rwc_core_cnt[n] + 2; i++)
			rte_eal_wait_lcore(enabled_core_ids[i]);

		writer_done = 1;

		/* Wait for readers to complete */
		rte_eal_mp_wait_lcore();

		unsigned long long cycles_per_write_operation =
			__atomic_load_n(&gwrite_cycles, __ATOMIC_RELAXED) /
			__atomic_load_n(&gwrites, __ATOMIC_RELAXED);
		rwc_perf_results->writer_add_del[n]
					= cycles_per_write_operation;
		printf("Cycles per write operation: %llu\n",
				cycles_per_write_operation);
	}

finish:
	rte_hash_free(tbl_rwc_test_param.h);
	rte_free(rv);
	return 0;

err:
	writer_done = 1;
	rte_eal_mp_wait_lcore();
	rte_hash_free(tbl_rwc_test_param.h);
	rte_free(rv);
	return -1;
}

static int
test_hash_readwrite_lf_perf_main(void)
{
	/*
	 * Variables used to choose different tests.
	 * rwc_lf indicates if read-write concurrency lock-free support is
	 * enabled.
	 * htm indicates if Hardware transactional memory support is enabled.
	 */
	int rwc_lf = 0;
	int htm;
	int ext_bkt = 0;

	if (rte_lcore_count() < 2) {
		printf("Not enough cores for hash_readwrite_lf_perf_autotest, expecting at least 2\n");
		return TEST_SKIPPED;
	}

	setlocale(LC_NUMERIC, "");

	/* Reset tbl_rwc_test_param to discard values from previous run */
	memset(&tbl_rwc_test_param, 0, sizeof(tbl_rwc_test_param));

	if (rte_tm_supported())
		htm = 1;
	else
		htm = 0;

	if (generate_keys() != 0)
		return -1;
	if (get_enabled_cores_list() != 0)
		return -1;

	if (RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF) {
		rwc_lf = 1;
		ext_bkt = 1;
		printf("Test lookup with read-write concurrency lock free support"
		       " enabled\n");
		if (test_hash_add_no_ks_lookup_hit(&rwc_lf_results, rwc_lf,
							htm, ext_bkt) < 0)
			return -1;
		if (test_hash_add_no_ks_lookup_miss(&rwc_lf_results, rwc_lf,
							htm, ext_bkt) < 0)
			return -1;
		if (test_hash_add_ks_lookup_hit_non_sp(&rwc_lf_results, rwc_lf,
							htm, ext_bkt) < 0)
			return -1;
		if (test_hash_add_ks_lookup_hit_sp(&rwc_lf_results, rwc_lf,
							htm, ext_bkt) < 0)
			return -1;
		if (test_hash_add_ks_lookup_miss(&rwc_lf_results, rwc_lf, htm,
						 ext_bkt) < 0)
			return -1;
		if (test_hash_multi_add_lookup(&rwc_lf_results, rwc_lf, htm,
					       ext_bkt) < 0)
			return -1;
		if (test_hash_add_ks_lookup_hit_extbkt(&rwc_lf_results, rwc_lf,
							htm, ext_bkt) < 0)
			return -1;
		if (test_hash_rcu_qsbr_writer_perf(&rwc_lf_results, rwc_lf,
						   htm, ext_bkt) < 0)
			return -1;
	}
	printf("\nTest lookup with read-write concurrency lock free support"
	       " disabled\n");
	rwc_lf = 0;
	if (!htm) {
		printf("With HTM Disabled\n");
		if (!RUN_WITH_HTM_DISABLED) {
			printf("Enable RUN_WITH_HTM_DISABLED to test with"
			       " lock-free disabled");
			goto results;
		}
	} else
		printf("With HTM Enabled\n");
	if (test_hash_add_no_ks_lookup_hit(&rwc_non_lf_results, rwc_lf, htm,
					   ext_bkt) < 0)
		return -1;
	if (test_hash_add_no_ks_lookup_miss(&rwc_non_lf_results, rwc_lf, htm,
						ext_bkt) < 0)
		return -1;
	if (test_hash_add_ks_lookup_hit_non_sp(&rwc_non_lf_results, rwc_lf,
						htm, ext_bkt) < 0)
		return -1;
	if (test_hash_add_ks_lookup_hit_sp(&rwc_non_lf_results, rwc_lf, htm,
						ext_bkt) < 0)
		return -1;
	if (test_hash_add_ks_lookup_miss(&rwc_non_lf_results, rwc_lf, htm,
					 ext_bkt) < 0)
		return -1;
	if (test_hash_multi_add_lookup(&rwc_non_lf_results, rwc_lf, htm,
							ext_bkt) < 0)
		return -1;
	if (test_hash_add_ks_lookup_hit_extbkt(&rwc_non_lf_results, rwc_lf,
						htm, ext_bkt) < 0)
		return -1;
results:
	printf("\n\t\t\t\t\t\t********** Results summary **********\n\n");
	int i, j, k;
	for (j = 0; j < 2; j++) {
		if (j == 1)
			printf("\n\t\t\t\t\t#######********** Bulk Lookup "
			       "**********#######\n\n");
		printf("_______\t\t_______\t\t_________\t___\t\t_________\t\t"
			"\t\t\t\t_________________\n");
		printf("Writers\t\tReaders\t\tLock-free\tHTM\t\tTest-case\t\t\t"
		       "\t\t\tCycles per lookup\n");
		printf("_______\t\t_______\t\t_________\t___\t\t_________\t\t\t"
		       "\t\t\t_________________\n");
		for (i = 0; i < NUM_TEST; i++) {
			printf("%u\t\t%u\t\t", 1, rwc_core_cnt[i]);
			printf("Enabled\t\t");
			printf("N/A\t\t");
			printf("Hash add - no key-shifts, lookup - hit\t\t\t\t"
				"%u\n\t\t\t\t\t\t\t\t",
				rwc_lf_results.w_no_ks_r_hit[j][i]);
			printf("Hash add - no key-shifts, lookup - miss\t\t\t\t"
				"%u\n\t\t\t\t\t\t\t\t",
				rwc_lf_results.w_no_ks_r_miss[j][i]);
			printf("Hash add - key-shifts, lookup - hit"
			       "(non-shift-path)\t\t%u\n\t\t\t\t\t\t\t\t",
			       rwc_lf_results.w_ks_r_hit_nsp[j][i]);
			printf("Hash add - key-shifts, lookup - hit "
			       "(shift-path)\t\t%u\n\t\t\t\t\t\t\t\t",
			       rwc_lf_results.w_ks_r_hit_sp[j][i]);
			printf("Hash add - key-shifts, Hash lookup miss\t\t\t\t"
				"%u\n\t\t\t\t\t\t\t\t",
				rwc_lf_results.w_ks_r_miss[j][i]);
			printf("Hash add - key-shifts, Hash lookup hit (ext_bkt)\t\t"
				"%u\n\n\t\t\t\t",
				rwc_lf_results.w_ks_r_hit_extbkt[j][i]);

			printf("Disabled\t");
			if (htm)
				printf("Enabled\t\t");
			else
				printf("Disabled\t");
			printf("Hash add - no key-shifts, lookup - hit\t\t\t\t"
				"%u\n\t\t\t\t\t\t\t\t",
				rwc_non_lf_results.w_no_ks_r_hit[j][i]);
			printf("Hash add - no key-shifts, lookup - miss\t\t\t\t"
				"%u\n\t\t\t\t\t\t\t\t",
				rwc_non_lf_results.w_no_ks_r_miss[j][i]);
			printf("Hash add - key-shifts, lookup - hit "
			       "(non-shift-path)\t\t%u\n\t\t\t\t\t\t\t\t",
			       rwc_non_lf_results.w_ks_r_hit_nsp[j][i]);
			printf("Hash add - key-shifts, lookup - hit "
			       "(shift-path)\t\t%u\n\t\t\t\t\t\t\t\t",
			       rwc_non_lf_results.w_ks_r_hit_sp[j][i]);
			printf("Hash add - key-shifts, Hash lookup miss\t\t\t\t"
			       "%u\n\t\t\t\t\t\t\t\t",
			       rwc_non_lf_results.w_ks_r_miss[j][i]);
			printf("Hash add - key-shifts, Hash lookup hit (ext_bkt)\t\t"
				"%u\n",
				rwc_non_lf_results.w_ks_r_hit_extbkt[j][i]);

			printf("_______\t\t_______\t\t_________\t___\t\t"
			       "_________\t\t\t\t\t\t_________________\n");
		}

		for (i = 1; i < NUM_TEST; i++) {
			for (k = 0; k < NUM_TEST; k++) {
				printf("%u", rwc_core_cnt[i]);
				printf("\t\t%u\t\t", rwc_core_cnt[k]);
				printf("Enabled\t\t");
				printf("N/A\t\t");
				printf("Multi-add-lookup\t\t\t\t\t\t%u\n\n\t\t"
				       "\t\t",
				       rwc_lf_results.multi_rw[i][j][k]);
				printf("Disabled\t");
				if (htm)
					printf("Enabled\t\t");
				else
					printf("Disabled\t");
				printf("Multi-add-lookup\t\t\t\t\t\t%u\n",
				       rwc_non_lf_results.multi_rw[i][j][k]);

				printf("_______\t\t_______\t\t_________\t___"
				       "\t\t_________\t\t\t\t\t\t"
				       "_________________\n");
			}
		}
	}
	rte_free(tbl_rwc_test_param.keys);
	rte_free(tbl_rwc_test_param.keys_no_ks);
	rte_free(tbl_rwc_test_param.keys_ks);
	rte_free(tbl_rwc_test_param.keys_absent);
	rte_free(tbl_rwc_test_param.keys_shift_path);
	rte_free(tbl_rwc_test_param.keys_non_shift_path);
	rte_free(tbl_rwc_test_param.keys_ext_bkt);
	rte_free(tbl_rwc_test_param.keys_ks_extbkt);
	return 0;
}

REGISTER_TEST_COMMAND(hash_readwrite_lf_perf_autotest,
	test_hash_readwrite_lf_perf_main);
