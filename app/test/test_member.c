/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

/* This test is for membership library's simple feature test */

#include "test.h"

#include <rte_memcpy.h>
#include <rte_malloc.h>

#ifdef RTE_EXEC_ENV_WINDOWS
static int
test_member(void)
{
	printf("member not supported on Windows, skipping test\n");
	return TEST_SKIPPED;
}

#else

#include <rte_member.h>
#include <rte_byteorder.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ip.h>

struct rte_member_setsum *setsum_ht;
struct rte_member_setsum *setsum_cache;
struct rte_member_setsum *setsum_vbf;

/* 5-tuple key type */
struct flow_key {
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
	uint8_t proto;
} __rte_packed;

/* Set ID Macros for multimatch test usage */
#define M_MATCH_S 1	/* Not start with 0 since by default 0 means no match */
#define M_MATCH_E 15
#define M_MATCH_STEP 2
#define M_MATCH_CNT \
	(1 + (M_MATCH_E - M_MATCH_S) / M_MATCH_STEP)


#define NUM_SAMPLES 5
#define MAX_MATCH 32

/* Keys used by unit test functions */
static struct flow_key keys[NUM_SAMPLES] = {
	{
		.ip_src = RTE_IPV4(0x03, 0x02, 0x01, 0x00),
		.ip_dst = RTE_IPV4(0x07, 0x06, 0x05, 0x04),
		.port_src = 0x0908,
		.port_dst = 0x0b0a,
		.proto = 0x0c,
	},
	{
		.ip_src = RTE_IPV4(0x13, 0x12, 0x11, 0x10),
		.ip_dst = RTE_IPV4(0x17, 0x16, 0x15, 0x14),
		.port_src = 0x1918,
		.port_dst = 0x1b1a,
		.proto = 0x1c,
	},
	{
		.ip_src = RTE_IPV4(0x23, 0x22, 0x21, 0x20),
		.ip_dst = RTE_IPV4(0x27, 0x26, 0x25, 0x24),
		.port_src = 0x2928,
		.port_dst = 0x2b2a,
		.proto = 0x2c,
	},
	{
		.ip_src = RTE_IPV4(0x33, 0x32, 0x31, 0x30),
		.ip_dst = RTE_IPV4(0x37, 0x36, 0x35, 0x34),
		.port_src = 0x3938,
		.port_dst = 0x3b3a,
		.proto = 0x3c,
	},
	{
		.ip_src = RTE_IPV4(0x43, 0x42, 0x41, 0x40),
		.ip_dst = RTE_IPV4(0x47, 0x46, 0x45, 0x44),
		.port_src = 0x4948,
		.port_dst = 0x4b4a,
		.proto = 0x4c,
	}
};

uint32_t test_set[NUM_SAMPLES] = {1, 2, 3, 4, 5};

#define ITERATIONS  3
#define KEY_SIZE  4

#define MAX_ENTRIES (1 << 16)
uint8_t generated_keys[MAX_ENTRIES][KEY_SIZE];

static struct rte_member_parameters params = {
		.num_keys = MAX_ENTRIES,	/* Total hash table entries. */
		.key_len = KEY_SIZE,		/* Length of hash key. */

		/* num_set and false_positive_rate only relevant to vBF */
		.num_set = 16,
		.false_positive_rate = 0.03,
		.prim_hash_seed = 1,
		.sec_hash_seed = 11,
		.socket_id = 0			/* NUMA Socket ID for memory. */
};

/*
 * Sequence of operations for find existing setsummary
 *
 *  - create setsum
 *  - find existing setsum: hit
 *  - find non-existing setsum: miss
 *
 */
static int
test_member_find_existing(void)
{
	struct rte_member_setsum *tmp_setsum = NULL, *result = NULL;
	struct rte_member_parameters tmp_params = {
		.name = "member_find_existing",
		.num_keys = MAX_ENTRIES,	/* Total hash table entries. */
		.key_len = KEY_SIZE,		/* Length of hash key. */
		.type = RTE_MEMBER_TYPE_HT,
		.num_set = 32,
		.false_positive_rate = 0.03,
		.prim_hash_seed = 1,
		.sec_hash_seed = 11,
		.socket_id = 0			/* NUMA Socket ID for memory. */
	};

	/* Create */
	tmp_setsum = rte_member_create(&tmp_params);
	TEST_ASSERT(tmp_setsum != NULL, "setsum creation failed");

	/* Try to find existing hash table */
	result = rte_member_find_existing("member_find_existing");
	TEST_ASSERT(result == tmp_setsum, "could not find existing setsum");

	/* Try to find non-existing hash table */
	result = rte_member_find_existing("member_find_non_existing");
	TEST_ASSERT(result == NULL, "found setsum that shouldn't exist");

	/* Cleanup. */
	rte_member_free(tmp_setsum);

	return 0;
}

/*
 * Test for bad creating parameters
 */
static int
test_member_create_bad_param(void)
{
	struct rte_member_setsum *bad_setsum = NULL;
	struct rte_member_parameters bad_params = {
		.num_keys = MAX_ENTRIES,	/* Total hash table entries. */
		.key_len = KEY_SIZE,		/* Length of hash key. */
		.type = RTE_MEMBER_TYPE_HT,
		.num_set = 32,
		.false_positive_rate = 0.03,
		.prim_hash_seed = 1,
		.sec_hash_seed = 11,
		.socket_id = 0			/* NUMA Socket ID for memory. */
	};

	printf("Expected error section begin...\n");
	bad_params.name = "bad_param1";
	bad_params.num_set = 0;
	bad_params.type = RTE_MEMBER_TYPE_VBF;
	/* Test with 0 set for vBF should fail */
	bad_setsum = rte_member_create(&bad_params);
	if (bad_setsum != NULL) {
		rte_member_free(bad_setsum);
		printf("Impossible creating setsum successfully with invalid "
			"number of set for vBF\n");
		return -1;
	}

	bad_params.name = "bad_param2";
	bad_params.false_positive_rate = 0;
	bad_params.num_set = 32;
	/* Test with 0 false positive for vBF should fail */
	bad_setsum = rte_member_create(&bad_params);
	if (bad_setsum != NULL) {
		rte_member_free(bad_setsum);
		printf("Impossible creating setsum successfully with invalid "
			"false positive rate for vBF\n");
		return -1;
	}

	bad_params.name = "bad_param3";
	bad_params.false_positive_rate = 0.03;
	bad_params.num_keys = 0;
	/* Test with 0 key per BF for vBF should fail */
	bad_setsum = rte_member_create(&bad_params);
	if (bad_setsum != NULL) {
		rte_member_free(bad_setsum);
		printf("Impossible creating setsum successfully with invalid "
			"num_keys for vBF\n");
		return -1;
	}

	bad_params.name = "bad_param4";
	bad_params.type = RTE_MEMBER_TYPE_HT;
	bad_params.num_keys = RTE_MEMBER_BUCKET_ENTRIES / 2;
	/* Test with less than 1 bucket for HTSS should fail */
	bad_setsum = rte_member_create(&bad_params);
	if (bad_setsum != NULL) {
		rte_member_free(bad_setsum);
		printf("Impossible creating setsum successfully with too few "
			"number of keys(entries) for HT\n");
		return -1;
	}

	bad_params.name = "bad_param5";
	bad_params.num_keys = RTE_MEMBER_ENTRIES_MAX + 1;
	/* Test with more than maximum entries for HTSS should fail */
	bad_setsum = rte_member_create(&bad_params);
	if (bad_setsum != NULL) {
		rte_member_free(bad_setsum);
		printf("Impossible creating setsum successfully with to many "
			"number of keys(entries) for HT\n");
		return -1;
	}

	bad_params.name = "bad_param5";
	/* Test with same name should fail */
	bad_setsum = rte_member_create(&bad_params);
	if (bad_setsum != NULL) {
		rte_member_free(bad_setsum);
		printf("Impossible creating setsum successfully with existed "
			"name\n");
		return -1;
	}
	printf("Expected error section end...\n");
	rte_member_free(bad_setsum);
	return 0;
}

/* Create test setsummaries. */
static int test_member_create(void)
{
	params.key_len = sizeof(struct flow_key);

	params.name = "test_member_ht";
	params.is_cache = 0;
	params.type = RTE_MEMBER_TYPE_HT;
	setsum_ht = rte_member_create(&params);

	params.name = "test_member_cache";
	params.is_cache = 1;
	setsum_cache = rte_member_create(&params);

	params.name = "test_member_vbf";
	params.type = RTE_MEMBER_TYPE_VBF;
	setsum_vbf = rte_member_create(&params);

	if (setsum_ht == NULL || setsum_cache == NULL || setsum_vbf == NULL) {
		printf("Creation of setsums fail\n");
		return -1;
	}
	printf("Creation of setsums success\n");
	return 0;
}

static int test_member_insert(void)
{
	int ret_ht, ret_cache, ret_vbf, i;

	for (i = 0; i < NUM_SAMPLES; i++) {
		ret_ht = rte_member_add(setsum_ht, &keys[i], test_set[i]);
		ret_cache = rte_member_add(setsum_cache, &keys[i],
						test_set[i]);
		ret_vbf = rte_member_add(setsum_vbf, &keys[i], test_set[i]);
		TEST_ASSERT(ret_ht >= 0 && ret_cache >= 0 && ret_vbf >= 0,
				"insert error");
	}
	printf("insert key success\n");
	return 0;
}

static int test_member_lookup(void)
{
	int ret_ht, ret_cache, ret_vbf, i;
	uint16_t set_ht, set_cache, set_vbf;
	member_set_t set_ids_ht[NUM_SAMPLES] = {0};
	member_set_t set_ids_cache[NUM_SAMPLES] = {0};
	member_set_t set_ids_vbf[NUM_SAMPLES] = {0};

	uint32_t num_key_ht = NUM_SAMPLES;
	uint32_t num_key_cache = NUM_SAMPLES;
	uint32_t num_key_vbf = NUM_SAMPLES;

	const void *key_array[NUM_SAMPLES];

	/* Single lookup test */
	for (i = 0; i < NUM_SAMPLES; i++) {
		ret_ht = rte_member_lookup(setsum_ht, &keys[i], &set_ht);
		ret_cache = rte_member_lookup(setsum_cache, &keys[i],
							&set_cache);
		ret_vbf = rte_member_lookup(setsum_vbf, &keys[i], &set_vbf);
		TEST_ASSERT(ret_ht >= 0 && ret_cache >= 0 && ret_vbf >= 0,
				"single lookup function error");

		TEST_ASSERT(set_ht == test_set[i] &&
				set_cache == test_set[i] &&
				set_vbf == test_set[i],
				"single lookup set value error");
	}
	printf("lookup single key success\n");

	/* Bulk lookup test */
	for (i = 0; i < NUM_SAMPLES; i++)
		key_array[i] = &keys[i];

	ret_ht = rte_member_lookup_bulk(setsum_ht, key_array,
			num_key_ht, set_ids_ht);

	ret_cache = rte_member_lookup_bulk(setsum_cache, key_array,
			num_key_cache, set_ids_cache);

	ret_vbf = rte_member_lookup_bulk(setsum_vbf, key_array,
			num_key_vbf, set_ids_vbf);

	TEST_ASSERT(ret_ht >= 0 && ret_cache >= 0 && ret_vbf >= 0,
			"bulk lookup function error");

	for (i = 0; i < NUM_SAMPLES; i++) {
		TEST_ASSERT((set_ids_ht[i] == test_set[i]) &&
				(set_ids_cache[i] == test_set[i]) &&
				(set_ids_vbf[i] == test_set[i]),
				"bulk lookup result error");
	}

	return 0;
}

static int test_member_delete(void)
{
	int ret_ht, ret_cache, ret_vbf, i;
	uint16_t set_ht, set_cache, set_vbf;
	const void *key_array[NUM_SAMPLES];
	member_set_t set_ids_ht[NUM_SAMPLES] = {0};
	member_set_t set_ids_cache[NUM_SAMPLES] = {0};
	member_set_t set_ids_vbf[NUM_SAMPLES] = {0};
	uint32_t num_key_ht = NUM_SAMPLES;
	uint32_t num_key_cache = NUM_SAMPLES;
	uint32_t num_key_vbf = NUM_SAMPLES;

	/* Delete part of all inserted keys */
	for (i = 0; i < NUM_SAMPLES / 2; i++) {
		ret_ht = rte_member_delete(setsum_ht, &keys[i], test_set[i]);
		ret_cache = rte_member_delete(setsum_cache, &keys[i],
						test_set[i]);
		ret_vbf = rte_member_delete(setsum_vbf, &keys[i], test_set[i]);
		/* VBF does not support delete yet, so return error code */
		TEST_ASSERT(ret_ht >= 0 && ret_cache >= 0,
				"key deletion function error");
		TEST_ASSERT(ret_vbf < 0,
				"vbf does not support deletion, error");
	}

	for (i = 0; i < NUM_SAMPLES; i++)
		key_array[i] = &keys[i];

	ret_ht = rte_member_lookup_bulk(setsum_ht, key_array,
			num_key_ht, set_ids_ht);

	ret_cache = rte_member_lookup_bulk(setsum_cache, key_array,
			num_key_cache, set_ids_cache);

	ret_vbf = rte_member_lookup_bulk(setsum_vbf, key_array,
			num_key_vbf, set_ids_vbf);

	TEST_ASSERT(ret_ht >= 0 && ret_cache >= 0 && ret_vbf >= 0,
			"bulk lookup function error");

	for (i = 0; i < NUM_SAMPLES / 2; i++) {
		TEST_ASSERT((set_ids_ht[i] == RTE_MEMBER_NO_MATCH) &&
				(set_ids_cache[i] == RTE_MEMBER_NO_MATCH),
				"bulk lookup result error");
	}

	for (i = NUM_SAMPLES / 2; i < NUM_SAMPLES; i++) {
		TEST_ASSERT((set_ids_ht[i] == test_set[i]) &&
				(set_ids_cache[i] == test_set[i]) &&
				(set_ids_vbf[i] == test_set[i]),
				"bulk lookup result error");
	}

	/* Delete the left of inserted keys */
	for (i = NUM_SAMPLES / 2; i < NUM_SAMPLES; i++) {
		ret_ht = rte_member_delete(setsum_ht, &keys[i], test_set[i]);
		ret_cache = rte_member_delete(setsum_cache, &keys[i],
						test_set[i]);
		ret_vbf = rte_member_delete(setsum_vbf, &keys[i], test_set[i]);
		/* VBF does not support delete yet, so return error code */
		TEST_ASSERT(ret_ht >= 0 && ret_cache >= 0,
				"key deletion function error");
		TEST_ASSERT(ret_vbf < 0,
				"vbf does not support deletion, error");
	}

	for (i = 0; i < NUM_SAMPLES; i++) {
		ret_ht = rte_member_lookup(setsum_ht, &keys[i], &set_ht);
		ret_cache = rte_member_lookup(setsum_cache, &keys[i],
						&set_cache);
		ret_vbf = rte_member_lookup(setsum_vbf, &keys[i], &set_vbf);
		TEST_ASSERT(ret_ht >= 0 && ret_cache >= 0,
				"key lookup function error");
		TEST_ASSERT(set_ht == RTE_MEMBER_NO_MATCH &&
				ret_cache == RTE_MEMBER_NO_MATCH,
				"key deletion failed");
	}
	/* Reset vbf for other following tests */
	rte_member_reset(setsum_vbf);

	printf("delete success\n");
	return 0;
}

static int test_member_multimatch(void)
{
	int ret_ht, ret_vbf, ret_cache;
	member_set_t set_ids_ht[MAX_MATCH] = {0};
	member_set_t set_ids_vbf[MAX_MATCH] = {0};
	member_set_t set_ids_cache[MAX_MATCH] = {0};

	member_set_t set_ids_ht_m[NUM_SAMPLES][MAX_MATCH] = {{0} };
	member_set_t set_ids_vbf_m[NUM_SAMPLES][MAX_MATCH] = {{0} };
	member_set_t set_ids_cache_m[NUM_SAMPLES][MAX_MATCH] = {{0} };

	uint32_t match_count_ht[NUM_SAMPLES];
	uint32_t match_count_vbf[NUM_SAMPLES];
	uint32_t match_count_cache[NUM_SAMPLES];

	uint32_t num_key_ht = NUM_SAMPLES;
	uint32_t num_key_vbf = NUM_SAMPLES;
	uint32_t num_key_cache = NUM_SAMPLES;

	const void *key_array[NUM_SAMPLES];

	uint32_t i, j;

	/* Same key at most inserted 2*entry_per_bucket times for HT mode */
	for (i = M_MATCH_S; i <= M_MATCH_E; i += M_MATCH_STEP) {
		for (j = 0; j < NUM_SAMPLES; j++) {
			ret_ht = rte_member_add(setsum_ht, &keys[j], i);
			ret_vbf = rte_member_add(setsum_vbf, &keys[j], i);
			ret_cache = rte_member_add(setsum_cache, &keys[j], i);

			TEST_ASSERT(ret_ht >= 0 && ret_vbf >= 0 &&
					ret_cache >= 0,
					"insert function error");
		}
	}

	/* Single multimatch test */
	for (i = 0; i < NUM_SAMPLES; i++) {
		ret_vbf = rte_member_lookup_multi(setsum_vbf, &keys[i],
							MAX_MATCH, set_ids_vbf);
		ret_ht = rte_member_lookup_multi(setsum_ht, &keys[i],
							MAX_MATCH, set_ids_ht);
		ret_cache = rte_member_lookup_multi(setsum_cache, &keys[i],
						MAX_MATCH, set_ids_cache);
		/*
		 * For cache mode, keys overwrite when signature same.
		 * the multimatch should work like single match.
		 */
		TEST_ASSERT(ret_ht == M_MATCH_CNT && ret_vbf == M_MATCH_CNT &&
				ret_cache == 1,
				"single lookup_multi error");
		TEST_ASSERT(set_ids_cache[0] == M_MATCH_E,
				"single lookup_multi cache error");

		for (j = 1; j <= M_MATCH_CNT; j++) {
			TEST_ASSERT(set_ids_ht[j-1] == j * M_MATCH_STEP - 1 &&
					set_ids_vbf[j-1] ==
							j * M_MATCH_STEP - 1,
					"single multimatch lookup error");
		}
	}
	printf("lookup single key for multimatch success\n");

	/* Bulk multimatch test */
	for (i = 0; i < NUM_SAMPLES; i++)
		key_array[i] = &keys[i];
	ret_vbf = rte_member_lookup_multi_bulk(setsum_vbf,
			&key_array[0], num_key_ht, MAX_MATCH, match_count_vbf,
			(member_set_t *)set_ids_vbf_m);

	ret_ht = rte_member_lookup_multi_bulk(setsum_ht,
			&key_array[0], num_key_vbf, MAX_MATCH, match_count_ht,
			(member_set_t *)set_ids_ht_m);

	ret_cache = rte_member_lookup_multi_bulk(setsum_cache,
			&key_array[0], num_key_cache, MAX_MATCH,
			match_count_cache, (member_set_t *)set_ids_cache_m);


	for (j = 0; j < NUM_SAMPLES; j++) {
		TEST_ASSERT(match_count_ht[j] == M_MATCH_CNT,
			"bulk multimatch lookup HT match count error");
		TEST_ASSERT(match_count_vbf[j] == M_MATCH_CNT,
			"bulk multimatch lookup vBF match count error");
		TEST_ASSERT(match_count_cache[j] == 1,
			"bulk multimatch lookup CACHE match count error");
		TEST_ASSERT(set_ids_cache_m[j][0] == M_MATCH_E,
			"bulk multimatch lookup CACHE set value error");

		for (i = 1; i <= M_MATCH_CNT; i++) {
			TEST_ASSERT(set_ids_ht_m[j][i-1] ==
							i * M_MATCH_STEP - 1,
				"bulk multimatch lookup HT set value error");
			TEST_ASSERT(set_ids_vbf_m[j][i-1] ==
							i * M_MATCH_STEP - 1,
				"bulk multimatch lookup vBF set value error");
		}
	}

	printf("lookup for bulk multimatch success\n");

	return 0;
}

static int key_compare(const void *key1, const void *key2)
{
	return memcmp(key1, key2, KEY_SIZE);
}

static void
setup_keys_and_data(void)
{
	unsigned int i, j;
	int num_duplicates;

	/* Reset all arrays */
	for (i = 0; i < KEY_SIZE; i++)
		generated_keys[0][i] = 0;

	/* Generate a list of keys, some of which may be duplicates */
	for (i = 0; i < MAX_ENTRIES; i++) {
		for (j = 0; j < KEY_SIZE; j++)
			generated_keys[i][j] = rte_rand() & 0xFF;
	}

	/* Remove duplicates from the keys array */
	do {
		num_duplicates = 0;
		/* Sort the list of keys to make it easier to find duplicates */
		qsort(generated_keys, MAX_ENTRIES, KEY_SIZE, key_compare);

		/* Sift through the list of keys and look for duplicates */
		int num_duplicates = 0;
		for (i = 0; i < MAX_ENTRIES - 1; i++) {
			if (memcmp(generated_keys[i], generated_keys[i + 1],
					KEY_SIZE) == 0) {
				/* This key already exists, try again */
				num_duplicates++;
				for (j = 0; j < KEY_SIZE; j++)
					generated_keys[i][j] =
							rte_rand() & 0xFF;
			}
		}
	} while (num_duplicates != 0);
}

static inline int
add_generated_keys(struct rte_member_setsum *setsum, unsigned int *added_keys)
{
	int ret = 0;

	for (*added_keys = 0; ret >= 0 && *added_keys < MAX_ENTRIES;
			(*added_keys)++) {
		uint16_t set = (rte_rand() & 0xf) + 1;
		ret = rte_member_add(setsum, &generated_keys[*added_keys], set);
	}
	return ret;
}

static inline int
add_generated_keys_cache(struct rte_member_setsum *setsum,
				unsigned int *added_keys)
{
	int ret = 0;

	for (*added_keys = 0; ret == 0 && *added_keys < MAX_ENTRIES;
			(*added_keys)++) {
		uint16_t set = (rte_rand() & 0xf) + 1;
		ret = rte_member_add(setsum, &generated_keys[*added_keys], set);
	}
	return ret;
}

static int
test_member_loadfactor(void)
{
	unsigned  int j;
	unsigned int added_keys, average_keys_added = 0;
	int ret;

	setup_keys_and_data();

	rte_member_free(setsum_ht);
	rte_member_free(setsum_cache);
	rte_member_free(setsum_vbf);

	params.key_len = KEY_SIZE;
	params.name = "test_member_ht";
	params.is_cache = 0;
	params.type = RTE_MEMBER_TYPE_HT;
	setsum_ht = rte_member_create(&params);

	params.name = "test_member_cache";
	params.is_cache = 1;
	setsum_cache = rte_member_create(&params);


	if (setsum_ht == NULL || setsum_cache == NULL) {
		printf("Creation of setsums fail\n");
		return -1;
	}
	/* Test HT non-cache mode */
	for (j = 0; j < ITERATIONS; j++) {
		/* Add random entries until key cannot be added */
		ret = add_generated_keys(setsum_ht, &added_keys);
		if (ret != -ENOSPC) {
			printf("Unexpected error when adding keys\n");
			return -1;
		}
		average_keys_added += added_keys;

		/* Reset the table */
		rte_member_reset(setsum_ht);

		/* Print a dot to show progress on operations */
		printf(".");
		fflush(stdout);
	}

	average_keys_added /= ITERATIONS;

	printf("\nKeys inserted when no space(non-cache) = %.2f%% (%u/%u)\n",
		((double) average_keys_added / params.num_keys * 100),
		average_keys_added, params.num_keys);

	/* Test cache mode */
	added_keys = average_keys_added = 0;
	for (j = 0; j < ITERATIONS; j++) {
		/* Add random entries until key cannot be added */
		ret = add_generated_keys_cache(setsum_cache, &added_keys);
		if (ret != 1) {
			printf("Unexpected error when adding keys\n");
			return -1;
		}
		average_keys_added += added_keys;

		/* Reset the table */
		rte_member_reset(setsum_cache);

		/* Print a dot to show progress on operations */
		printf(".");
		fflush(stdout);
	}

	average_keys_added /= ITERATIONS;

	printf("\nKeys inserted when eviction happens(cache)= %.2f%% (%u/%u)\n",
		((double) average_keys_added / params.num_keys * 100),
		average_keys_added, params.num_keys);
	return 0;
}

static void
perform_free(void)
{
	rte_member_free(setsum_ht);
	rte_member_free(setsum_cache);
	rte_member_free(setsum_vbf);
}

static int
test_member(void)
{
	if (test_member_create_bad_param() < 0)
		return -1;

	if (test_member_find_existing() < 0)
		return -1;

	if (test_member_create() < 0) {
		perform_free();
		return -1;
	}
	if (test_member_insert() < 0) {
		perform_free();
		return -1;
	}
	if (test_member_lookup() < 0) {
		perform_free();
		return -1;
	}
	if (test_member_delete() < 0) {
		perform_free();
		return -1;
	}
	if (test_member_multimatch() < 0) {
		perform_free();
		return -1;
	}
	if (test_member_loadfactor() < 0) {
		rte_member_free(setsum_ht);
		rte_member_free(setsum_cache);
		return -1;
	}

	perform_free();
	return 0;
}

#endif /* !RTE_EXEC_ENV_WINDOWS */

REGISTER_TEST_COMMAND(member_autotest, test_member);
