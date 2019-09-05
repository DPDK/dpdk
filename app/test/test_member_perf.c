/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdio.h>
#include <inttypes.h>

#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_memcpy.h>
#include <rte_thash.h>
#include <rte_member.h>

#include "test.h"

#define NUM_KEYSIZES 10
#define NUM_SHUFFLES 10
#define MAX_KEYSIZE 64
#define MAX_ENTRIES (1 << 19)
#define KEYS_TO_ADD (MAX_ENTRIES * 75 / 100) /* 75% table utilization */
#define NUM_LOOKUPS (KEYS_TO_ADD * 5) /* Loop among keys added, several times */
#define VBF_SET_CNT 16
#define BURST_SIZE 64
#define VBF_FALSE_RATE 0.03

static unsigned int test_socket_id;

enum sstype {
	HT = 0,
	CACHE,
	VBF,
	NUM_TYPE
};

enum operations {
	ADD = 0,
	LOOKUP,
	LOOKUP_BULK,
	LOOKUP_MULTI,
	LOOKUP_MULTI_BULK,
	DELETE,
	LOOKUP_MISS,
	NUM_OPERATIONS
};

struct  member_perf_params {
	struct rte_member_setsum *setsum[NUM_TYPE];
	uint32_t key_size;
	unsigned int cycle;
};

static uint32_t hashtest_key_lens[] = {
	/* standard key sizes */
	4, 8, 16, 32, 48, 64,
	/* IPv4 SRC + DST + protocol, unpadded */
	9,
	/* IPv4 5-tuple, unpadded */
	13,
	/* IPv6 5-tuple, unpadded */
	37,
	/* IPv6 5-tuple, padded to 8-byte boundary */
	40
};

/* Array to store number of cycles per operation */
static uint64_t cycles[NUM_TYPE][NUM_KEYSIZES][NUM_OPERATIONS];
static uint64_t false_data[NUM_TYPE][NUM_KEYSIZES];
static uint64_t false_data_bulk[NUM_TYPE][NUM_KEYSIZES];
static uint64_t false_data_multi[NUM_TYPE][NUM_KEYSIZES];
static uint64_t false_data_multi_bulk[NUM_TYPE][NUM_KEYSIZES];

static uint64_t false_hit[NUM_TYPE][NUM_KEYSIZES];

static member_set_t data[NUM_TYPE][/* Array to store the data */KEYS_TO_ADD];

/* Array to store all input keys */
static uint8_t keys[KEYS_TO_ADD][MAX_KEYSIZE];

/* Shuffle the keys that have been added, so lookups will be totally random */
static void
shuffle_input_keys(struct member_perf_params *params)
{
	member_set_t temp_data;
	unsigned int i, j;
	uint32_t swap_idx;
	uint8_t temp_key[MAX_KEYSIZE];

	for (i = KEYS_TO_ADD - 1; i > 0; i--) {
		swap_idx = rte_rand() % i;
		memcpy(temp_key, keys[i], hashtest_key_lens[params->cycle]);
		memcpy(keys[i], keys[swap_idx],
			hashtest_key_lens[params->cycle]);
		memcpy(keys[swap_idx], temp_key,
			hashtest_key_lens[params->cycle]);
		for (j = 0; j < NUM_TYPE; j++) {
			temp_data = data[j][i];
			data[j][i] = data[j][swap_idx];
			data[j][swap_idx] = temp_data;
		}
	}
}

static int key_compare(const void *key1, const void *key2)
{
	return memcmp(key1, key2, MAX_KEYSIZE);
}

struct rte_member_parameters member_params = {
		.num_keys = MAX_ENTRIES,	/* Total hash table entries. */
		.key_len = 4,			/* Length of hash key. */

		/* num_set and false_positive_rate only relevant to vBF */
		.num_set = VBF_SET_CNT,
		.false_positive_rate = 0.03,
		.prim_hash_seed = 0,
		.sec_hash_seed = 1,
		.socket_id = 0,			/* NUMA Socket ID for memory. */
	};

static int
setup_keys_and_data(struct member_perf_params *params, unsigned int cycle,
		int miss)
{
	unsigned int i, j;
	int num_duplicates;

	params->key_size = hashtest_key_lens[cycle];
	params->cycle = cycle;

	/* Reset all arrays */
	for (i = 0; i < params->key_size; i++)
		keys[0][i] = 0;

	/* Generate a list of keys, some of which may be duplicates */
	for (i = 0; i < KEYS_TO_ADD; i++) {
		for (j = 0; j < params->key_size; j++)
			keys[i][j] = rte_rand() & 0xFF;

		data[HT][i] = data[CACHE][i] = (rte_rand() & 0x7FFE) + 1;
		data[VBF][i] = rte_rand() % VBF_SET_CNT + 1;
	}

	/* Remove duplicates from the keys array */
	do {
		num_duplicates = 0;

		/* Sort the list of keys to make it easier to find duplicates */
		qsort(keys, KEYS_TO_ADD, MAX_KEYSIZE, key_compare);

		/* Sift through the list of keys and look for duplicates */
		int num_duplicates = 0;
		for (i = 0; i < KEYS_TO_ADD - 1; i++) {
			if (memcmp(keys[i], keys[i + 1],
					params->key_size) == 0) {
				/* This key already exists, try again */
				num_duplicates++;
				for (j = 0; j < params->key_size; j++)
					keys[i][j] = rte_rand() & 0xFF;
			}
		}
	} while (num_duplicates != 0);

	/* Shuffle the random values again */
	shuffle_input_keys(params);

	/* For testing miss lookup, we insert half and lookup the other half */
	unsigned int entry_cnt, bf_key_cnt;
	if (!miss) {
		entry_cnt = MAX_ENTRIES;
		bf_key_cnt = KEYS_TO_ADD;
	} else {
		entry_cnt = MAX_ENTRIES / 2;
		bf_key_cnt = KEYS_TO_ADD / 2;
	}
	member_params.false_positive_rate = VBF_FALSE_RATE;
	member_params.key_len = params->key_size;
	member_params.socket_id = test_socket_id;
	member_params.num_keys = entry_cnt;
	member_params.name = "test_member_ht";
	member_params.is_cache = 0;
	member_params.type = RTE_MEMBER_TYPE_HT;
	params->setsum[HT] = rte_member_create(&member_params);
	if (params->setsum[HT] == NULL)
		fprintf(stderr, "ht create fail\n");

	member_params.name = "test_member_cache";
	member_params.is_cache = 1;
	params->setsum[CACHE] = rte_member_create(&member_params);
	if (params->setsum[CACHE] == NULL)
		fprintf(stderr, "CACHE create fail\n");

	member_params.name = "test_member_vbf";
	member_params.type = RTE_MEMBER_TYPE_VBF;
	member_params.num_keys = bf_key_cnt;
	params->setsum[VBF] = rte_member_create(&member_params);
	if (params->setsum[VBF] == NULL)
		fprintf(stderr, "VBF create fail\n");
	for (i = 0; i < NUM_TYPE; i++) {
		if (params->setsum[i] == NULL)
			return -1;
	}

	return 0;
}

static int
timed_adds(struct member_perf_params *params, int type)
{
	const uint64_t start_tsc = rte_rdtsc();
	unsigned int i, a;
	int32_t ret;

	for (i = 0; i < KEYS_TO_ADD; i++) {
		ret = rte_member_add(params->setsum[type], &keys[i],
					data[type][i]);
		if (ret < 0) {
			printf("Error %d in rte_member_add - key=0x", ret);
			for (a = 0; a < params->key_size; a++)
				printf("%02x", keys[i][a]);
			printf(" value=%d, type: %d\n", data[type][i], type);

			return -1;
		}
	}

	const uint64_t end_tsc = rte_rdtsc();
	const uint64_t time_taken = end_tsc - start_tsc;

	cycles[type][params->cycle][ADD] = time_taken / KEYS_TO_ADD;
	return 0;
}

static int
timed_lookups(struct member_perf_params *params, int type)
{
	unsigned int i, j;

	false_data[type][params->cycle] = 0;

	const uint64_t start_tsc = rte_rdtsc();
	member_set_t result;
	int ret;

	for (i = 0; i < NUM_LOOKUPS / KEYS_TO_ADD; i++) {
		for (j = 0; j < KEYS_TO_ADD; j++) {
			ret = rte_member_lookup(params->setsum[type], &keys[j],
						&result);
			if (ret < 0) {
				printf("lookup wrong internally");
				return -1;
			}
			if (type == HT && result == RTE_MEMBER_NO_MATCH) {
				printf("HT mode shouldn't have false negative");
				return -1;
			}
			if (result != data[type][j])
				false_data[type][params->cycle]++;
		}
	}

	const uint64_t end_tsc = rte_rdtsc();
	const uint64_t time_taken = end_tsc - start_tsc;

	cycles[type][params->cycle][LOOKUP] = time_taken / NUM_LOOKUPS;

	return 0;
}

static int
timed_lookups_bulk(struct member_perf_params *params, int type)
{
	unsigned int i, j, k;
	member_set_t result[BURST_SIZE] = {0};
	const void *keys_burst[BURST_SIZE];
	int ret;

	false_data_bulk[type][params->cycle] = 0;

	const uint64_t start_tsc = rte_rdtsc();

	for (i = 0; i < NUM_LOOKUPS / KEYS_TO_ADD; i++) {
		for (j = 0; j < KEYS_TO_ADD / BURST_SIZE; j++) {
			for (k = 0; k < BURST_SIZE; k++)
				keys_burst[k] = keys[j * BURST_SIZE + k];

			ret = rte_member_lookup_bulk(params->setsum[type],
				keys_burst,
				BURST_SIZE,
				result);
			if  (ret <= 0) {
				printf("lookup bulk has wrong return value\n");
				return -1;
			}
			for (k = 0; k < BURST_SIZE; k++) {
				uint32_t data_idx = j * BURST_SIZE + k;
				if (type == HT && result[k] ==
						RTE_MEMBER_NO_MATCH) {
					printf("HT mode shouldn't have "
						"false negative");
					return -1;
				}
				if (result[k] != data[type][data_idx])
					false_data_bulk[type][params->cycle]++;
			}
		}
	}

	const uint64_t end_tsc = rte_rdtsc();
	const uint64_t time_taken = end_tsc - start_tsc;

	cycles[type][params->cycle][LOOKUP_BULK] = time_taken / NUM_LOOKUPS;

	return 0;
}

static int
timed_lookups_multimatch(struct member_perf_params *params, int type)
{
	unsigned int i, j;
	member_set_t result[RTE_MEMBER_BUCKET_ENTRIES] = {0};
	int ret;
	false_data_multi[type][params->cycle] = 0;

	const uint64_t start_tsc = rte_rdtsc();

	for (i = 0; i < NUM_LOOKUPS / KEYS_TO_ADD; i++) {
		for (j = 0; j < KEYS_TO_ADD; j++) {
			ret = rte_member_lookup_multi(params->setsum[type],
				&keys[j], RTE_MEMBER_BUCKET_ENTRIES, result);
			if (type != CACHE && ret <= 0) {
				printf("lookup multi has wrong return value %d,"
					"type %d\n", ret, type);
			}
			if (type == HT && ret == 0) {
				printf("HT mode shouldn't have false negative");
				return -1;
			}
			/*
			 * For performance test purpose, we do not iterate all
			 * results here. We assume most likely each key can only
			 * find one match which is result[0].
			 */
			if (result[0] != data[type][j])
				false_data_multi[type][params->cycle]++;
		}
	}

	const uint64_t end_tsc = rte_rdtsc();
	const uint64_t time_taken = end_tsc - start_tsc;

	cycles[type][params->cycle][LOOKUP_MULTI] = time_taken / NUM_LOOKUPS;

	return 0;
}

static int
timed_lookups_multimatch_bulk(struct member_perf_params *params, int type)
{
	unsigned int i, j, k;
	member_set_t result[BURST_SIZE][RTE_MEMBER_BUCKET_ENTRIES] = {{0} };
	const void *keys_burst[BURST_SIZE];
	uint32_t match_count[BURST_SIZE];
	int ret;

	false_data_multi_bulk[type][params->cycle] = 0;

	const uint64_t start_tsc = rte_rdtsc();

	for (i = 0; i < NUM_LOOKUPS / KEYS_TO_ADD; i++) {
		for (j = 0; j < KEYS_TO_ADD / BURST_SIZE; j++) {
			for (k = 0; k < BURST_SIZE; k++)
				keys_burst[k] = keys[j * BURST_SIZE + k];

			ret = rte_member_lookup_multi_bulk(
				params->setsum[type],
				keys_burst, BURST_SIZE,
				RTE_MEMBER_BUCKET_ENTRIES, match_count,
				(member_set_t *)result);
			if (ret < 0) {
				printf("lookup multimatch bulk has wrong return"
					" value\n");
				return -1;
			}
			for (k = 0; k < BURST_SIZE; k++) {
				if (type != CACHE && match_count[k] == 0) {
					printf("lookup multimatch bulk get "
						"wrong match count\n");
					return -1;
				}
				if (type == HT && match_count[k] == 0) {
					printf("HT mode shouldn't have "
						"false negative");
					return -1;
				}
				uint32_t data_idx = j * BURST_SIZE + k;
				if (result[k][0] != data[type][data_idx])
					false_data_multi_bulk[type][params->cycle]++;
			}
		}
	}

	const uint64_t end_tsc = rte_rdtsc();
	const uint64_t time_taken = end_tsc - start_tsc;

	cycles[type][params->cycle][LOOKUP_MULTI_BULK] = time_taken /
							NUM_LOOKUPS;

	return 0;
}

static int
timed_deletes(struct member_perf_params *params, int type)
{
	unsigned int i;
	int32_t ret;

	if (type == VBF)
		return 0;
	const uint64_t start_tsc = rte_rdtsc();
	for (i = 0; i < KEYS_TO_ADD; i++) {
		ret = rte_member_delete(params->setsum[type], &keys[i],
					data[type][i]);
		if (type != CACHE && ret < 0) {
			printf("delete error\n");
			return -1;
		}
	}

	const uint64_t end_tsc = rte_rdtsc();
	const uint64_t time_taken = end_tsc - start_tsc;

	cycles[type][params->cycle][DELETE] = time_taken / KEYS_TO_ADD;

	return 0;
}

static int
timed_miss_lookup(struct member_perf_params *params, int type)
{
	unsigned int i, j;
	int ret;

	false_hit[type][params->cycle] = 0;

	for (i = 0; i < KEYS_TO_ADD / 2; i++) {
		ret = rte_member_add(params->setsum[type], &keys[i],
					data[type][i]);
		if (ret < 0) {
			unsigned int a;
			printf("Error %d in rte_member_add - key=0x", ret);
			for (a = 0; a < params->key_size; a++)
				printf("%02x", keys[i][a]);
			printf(" value=%d, type: %d\n", data[type][i], type);

			return -1;
		}
	}

	const uint64_t start_tsc = rte_rdtsc();
	member_set_t result;

	for (i = 0; i < 2 * NUM_LOOKUPS / KEYS_TO_ADD; i++) {
		for (j = KEYS_TO_ADD / 2; j < KEYS_TO_ADD; j++) {
			ret = rte_member_lookup(params->setsum[type], &keys[j],
						&result);
			if (ret < 0) {
				printf("lookup wrong internally");
				return -1;
			}
			if (result != RTE_MEMBER_NO_MATCH)
				false_hit[type][params->cycle]++;
		}
	}

	const uint64_t end_tsc = rte_rdtsc();
	const uint64_t time_taken = end_tsc - start_tsc;

	cycles[type][params->cycle][LOOKUP_MISS] = time_taken / NUM_LOOKUPS;

	return 0;
}

static void
perform_frees(struct member_perf_params *params)
{
	int i;
	for (i = 0; i < NUM_TYPE; i++) {
		if (params->setsum[i] != NULL) {
			rte_member_free(params->setsum[i]);
			params->setsum[i] = NULL;
		}
	}
}

static int
exit_with_fail(const char *testname, struct member_perf_params *params,
		unsigned int i, unsigned int j)
{
	printf("<<<<<Test %s failed at keysize %d iteration %d type %d>>>>>\n",
			testname, hashtest_key_lens[params->cycle], i, j);
	perform_frees(params);
	return -1;
}

static int
run_all_tbl_perf_tests(void)
{
	unsigned int i, j, k;
	struct member_perf_params params;

	printf("Measuring performance, please wait\n");
	fflush(stdout);

	test_socket_id = rte_socket_id();

	for (i = 0; i < NUM_KEYSIZES; i++) {
		if (setup_keys_and_data(&params, i, 0) < 0) {
			printf("Could not create keys/data/table\n");
			return -1;
		}
		for (j = 0; j < NUM_TYPE; j++) {

			if (timed_adds(&params, j) < 0)
				return exit_with_fail("timed_adds", &params,
							i, j);

			for (k = 0; k < NUM_SHUFFLES; k++)
				shuffle_input_keys(&params);

			if (timed_lookups(&params, j) < 0)
				return exit_with_fail("timed_lookups", &params,
							i, j);

			if (timed_lookups_bulk(&params, j) < 0)
				return exit_with_fail("timed_lookups_bulk",
						&params, i, j);

			if (timed_lookups_multimatch(&params, j) < 0)
				return exit_with_fail("timed_lookups_multi",
						&params, i, j);

			if (timed_lookups_multimatch_bulk(&params, j) < 0)
				return exit_with_fail("timed_lookups_multi_bulk",
							&params, i, j);

			if (timed_deletes(&params, j) < 0)
				return exit_with_fail("timed_deletes", &params,
							i, j);

			/* Print a dot to show progress on operations */
		}
		printf(".");
		fflush(stdout);

		perform_frees(&params);
	}

	/* Test false positive rate using un-inserted keys */
	for (i = 0; i < NUM_KEYSIZES; i++) {
		if (setup_keys_and_data(&params, i, 1) < 0) {
			printf("Could not create keys/data/table\n");
			return -1;
			}
		for (j = 0; j < NUM_TYPE; j++) {
			if (timed_miss_lookup(&params, j) < 0)
				return exit_with_fail("timed_miss_lookup",
						&params, i, j);
		}
		perform_frees(&params);
	}

	printf("\nResults (in CPU cycles/operation)\n");
	printf("-----------------------------------\n");
	printf("\n%-18s%-18s%-18s%-18s%-18s%-18s%-18s%-18s%-18s\n",
			"Keysize", "type",  "Add", "Lookup", "Lookup_bulk",
			"lookup_multi", "lookup_multi_bulk", "Delete",
			"miss_lookup");
	for (i = 0; i < NUM_KEYSIZES; i++) {
		for (j = 0; j < NUM_TYPE; j++) {
			printf("%-18d", hashtest_key_lens[i]);
			printf("%-18d", j);
			for (k = 0; k < NUM_OPERATIONS; k++)
				printf("%-18"PRIu64, cycles[j][i][k]);
			printf("\n");
		}
	}

	printf("\nFalse results rate (and false positive rate)\n");
	printf("-----------------------------------\n");
	printf("\n%-18s%-18s%-18s%-18s%-18s%-18s%-18s\n",
			"Keysize", "type",  "fr_single", "fr_bulk", "fr_multi",
			"fr_multi_bulk", "false_positive_rate");
	/* Key size not influence False rate so just print out one key size */
	for (i = 0; i < 1; i++) {
		for (j = 0; j < NUM_TYPE; j++) {
			printf("%-18d", hashtest_key_lens[i]);
			printf("%-18d", j);
			printf("%-18f", (float)false_data[j][i] / NUM_LOOKUPS);
			printf("%-18f", (float)false_data_bulk[j][i] /
						NUM_LOOKUPS);
			printf("%-18f", (float)false_data_multi[j][i] /
						NUM_LOOKUPS);
			printf("%-18f", (float)false_data_multi_bulk[j][i] /
						NUM_LOOKUPS);
			printf("%-18f", (float)false_hit[j][i] /
						NUM_LOOKUPS);
			printf("\n");
		}
	}
	return 0;
}

static int
test_member_perf(void)
{

	if (run_all_tbl_perf_tests() < 0)
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(member_perf_autotest, test_member_perf);
