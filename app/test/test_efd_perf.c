/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <stdio.h>
#include <inttypes.h>

#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_efd.h>
#include <rte_memcpy.h>
#include <rte_thash.h>

#include "test.h"

#define NUM_KEYSIZES 10
#define NUM_SHUFFLES 10
#define MAX_KEYSIZE 64
#define MAX_ENTRIES (1 << 19)
#define KEYS_TO_ADD (MAX_ENTRIES * 3 / 4) /* 75% table utilization */
#define NUM_LOOKUPS (KEYS_TO_ADD * 5) /* Loop among keys added, several times */

#if RTE_EFD_VALUE_NUM_BITS == 32
#define VALUE_BITMASK 0xffffffff
#else
#define VALUE_BITMASK ((1 << RTE_EFD_VALUE_NUM_BITS) - 1)
#endif
static unsigned int test_socket_id;

static inline uint64_t efd_get_all_sockets_bitmask(void)
{
	uint64_t all_cpu_sockets_bitmask = 0;
	unsigned int i;
	unsigned int next_lcore = rte_get_main_lcore();
	const int val_true = 1, val_false = 0;
	for (i = 0; i < rte_lcore_count(); i++) {
		all_cpu_sockets_bitmask |= 1 << rte_lcore_to_socket_id(next_lcore);
		next_lcore = rte_get_next_lcore(next_lcore, val_false, val_true);
	}

	return all_cpu_sockets_bitmask;
}

enum operations {
	ADD = 0,
	LOOKUP,
	LOOKUP_MULTI,
	DELETE,
	NUM_OPERATIONS
};

struct efd_perf_params {
	struct rte_efd_table *efd_table;
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
static uint64_t cycles[NUM_KEYSIZES][NUM_OPERATIONS];

/* Array to store the data */
static efd_value_t data[KEYS_TO_ADD];

/* Array to store all input keys */
static uint8_t keys[KEYS_TO_ADD][MAX_KEYSIZE];

/* Shuffle the keys that have been added, so lookups will be totally random */
static void
shuffle_input_keys(struct efd_perf_params *params)
{
	efd_value_t temp_data;
	unsigned int i;
	uint32_t swap_idx;
	uint8_t temp_key[MAX_KEYSIZE];

	for (i = KEYS_TO_ADD - 1; i > 0; i--) {
		swap_idx = rte_rand() % i;

		memcpy(temp_key, keys[i], hashtest_key_lens[params->cycle]);
		temp_data = data[i];

		memcpy(keys[i], keys[swap_idx], hashtest_key_lens[params->cycle]);
		data[i] = data[swap_idx];

		memcpy(keys[swap_idx], temp_key, hashtest_key_lens[params->cycle]);
		data[swap_idx] = temp_data;
	}
}

static int key_compare(const void *key1, const void *key2)
{
	return memcmp(key1, key2, MAX_KEYSIZE);
}

/*
 * TODO: we could "error proof" these as done in test_hash_perf.c ln 165:
 *
 * The current setup may give errors if too full in some cases which we check
 * for. However, since EFD allows for ~99% capacity, these errors are rare for
 * #"KEYS_TO_ADD" which is 75% capacity.
 */
static int
setup_keys_and_data(struct efd_perf_params *params, unsigned int cycle)
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

		data[i] = rte_rand() & VALUE_BITMASK;
	}

	/* Remove duplicates from the keys array */
	do {
		num_duplicates = 0;

		/* Sort the list of keys to make it easier to find duplicates */
		qsort(keys, KEYS_TO_ADD, MAX_KEYSIZE, key_compare);

		/* Sift through the list of keys and look for duplicates */
		int num_duplicates = 0;
		for (i = 0; i < KEYS_TO_ADD - 1; i++) {
			if (memcmp(keys[i], keys[i + 1], params->key_size) == 0) {
				/* This key already exists, try again */
				num_duplicates++;
				for (j = 0; j < params->key_size; j++)
					keys[i][j] = rte_rand() & 0xFF;
			}
		}
	} while (num_duplicates != 0);

	/* Shuffle the random values again */
	shuffle_input_keys(params);

	params->efd_table = rte_efd_create("test_efd_perf",
			MAX_ENTRIES, params->key_size,
			efd_get_all_sockets_bitmask(), test_socket_id);
	TEST_ASSERT_NOT_NULL(params->efd_table, "Error creating the efd table\n");

	return 0;
}

static int
timed_adds(struct efd_perf_params *params)
{
	const uint64_t start_tsc = rte_rdtsc();
	unsigned int i, a;
	int32_t ret;

	for (i = 0; i < KEYS_TO_ADD; i++) {
		ret = rte_efd_update(params->efd_table, test_socket_id, keys[i],
				data[i]);
		if (ret != 0) {
			printf("Error %d in rte_efd_update - key=0x", ret);
			for (a = 0; a < params->key_size; a++)
				printf("%02x", keys[i][a]);
			printf(" value=%d\n", data[i]);

			return -1;
		}
	}

	const uint64_t end_tsc = rte_rdtsc();
	const uint64_t time_taken = end_tsc - start_tsc;

	cycles[params->cycle][ADD] = time_taken / KEYS_TO_ADD;
	return 0;
}

static int
timed_lookups(struct efd_perf_params *params)
{
	unsigned int i, j, a;
	const uint64_t start_tsc = rte_rdtsc();
	efd_value_t ret_data;

	for (i = 0; i < NUM_LOOKUPS / KEYS_TO_ADD; i++) {
		for (j = 0; j < KEYS_TO_ADD; j++) {
			ret_data = rte_efd_lookup(params->efd_table,
					test_socket_id, keys[j]);
			if (ret_data != data[j]) {
				printf("Value mismatch using rte_efd_lookup: "
						"key #%d (0x", i);
				for (a = 0; a < params->key_size; a++)
					printf("%02x", keys[i][a]);
				printf(")\n");
				printf("  Expected %d, got %d\n", data[i],
						ret_data);

				return -1;
			}

		}
	}

	const uint64_t end_tsc = rte_rdtsc();
	const uint64_t time_taken = end_tsc - start_tsc;

	cycles[params->cycle][LOOKUP] = time_taken / NUM_LOOKUPS;

	return 0;
}

static int
timed_lookups_multi(struct efd_perf_params *params)
{
	unsigned int i, j, k, a;
	efd_value_t result[RTE_EFD_BURST_MAX] = {0};
	const void *keys_burst[RTE_EFD_BURST_MAX];
	const uint64_t start_tsc = rte_rdtsc();

	for (i = 0; i < NUM_LOOKUPS / KEYS_TO_ADD; i++) {
		for (j = 0; j < KEYS_TO_ADD / RTE_EFD_BURST_MAX; j++) {
			for (k = 0; k < RTE_EFD_BURST_MAX; k++)
				keys_burst[k] = keys[j * RTE_EFD_BURST_MAX + k];

			rte_efd_lookup_bulk(params->efd_table, test_socket_id,
					RTE_EFD_BURST_MAX,
					keys_burst, result);

			for (k = 0; k < RTE_EFD_BURST_MAX; k++) {
				uint32_t data_idx = j * RTE_EFD_BURST_MAX + k;
				if (result[k] != data[data_idx]) {
					printf("Value mismatch using "
						"rte_efd_lookup_bulk: key #%d "
						"(0x", i);
					for (a = 0; a < params->key_size; a++)
						printf("%02x",
							keys[data_idx][a]);
					printf(")\n");
					printf("  Expected %d, got %d\n",
						data[data_idx], result[k]);

					return -1;
				}
			}
		}
	}

	const uint64_t end_tsc = rte_rdtsc();
	const uint64_t time_taken = end_tsc - start_tsc;

	cycles[params->cycle][LOOKUP_MULTI] = time_taken / NUM_LOOKUPS;

	return 0;
}

static int
timed_deletes(struct efd_perf_params *params)
{
	unsigned int i, a;
	const uint64_t start_tsc = rte_rdtsc();
	int32_t ret;

	for (i = 0; i < KEYS_TO_ADD; i++) {
		ret = rte_efd_delete(params->efd_table, test_socket_id, keys[i],
				NULL);

		if (ret != 0) {
			printf("Error %d in rte_efd_delete - key=0x", ret);
			for (a = 0; a < params->key_size; a++)
				printf("%02x", keys[i][a]);
			printf("\n");

			return -1;
		}
	}

	const uint64_t end_tsc = rte_rdtsc();
	const uint64_t time_taken = end_tsc - start_tsc;

	cycles[params->cycle][DELETE] = time_taken / KEYS_TO_ADD;

	return 0;
}

static void
perform_frees(struct efd_perf_params *params)
{
	if (params->efd_table != NULL) {
		rte_efd_free(params->efd_table);
		params->efd_table = NULL;
	}
}

static int
exit_with_fail(const char *testname, struct efd_perf_params *params,
		unsigned int i)
{

	printf("<<<<<Test %s failed at keysize %d iteration %d >>>>>\n",
			testname, hashtest_key_lens[params->cycle], i);
	perform_frees(params);
	return -1;
}

static int
run_all_tbl_perf_tests(void)
{
	unsigned int i, j;
	struct efd_perf_params params;

	printf("Measuring performance, please wait\n");
	fflush(stdout);

	test_socket_id = rte_socket_id();

	for (i = 0; i < NUM_KEYSIZES; i++) {

		if (setup_keys_and_data(&params, i) < 0) {
			printf("Could not create keys/data/table\n");
			return -1;
		}

		if (timed_adds(&params) < 0)
			return exit_with_fail("timed_adds", &params, i);

		for (j = 0; j < NUM_SHUFFLES; j++)
			shuffle_input_keys(&params);

		if (timed_lookups(&params) < 0)
			return exit_with_fail("timed_lookups", &params, i);

		if (timed_lookups_multi(&params) < 0)
			return exit_with_fail("timed_lookups_multi", &params, i);

		if (timed_deletes(&params) < 0)
			return exit_with_fail("timed_deletes", &params, i);

		/* Print a dot to show progress on operations */
		printf(".");
		fflush(stdout);

		perform_frees(&params);
	}

	printf("\nResults (in CPU cycles/operation)\n");
	printf("-----------------------------------\n");
	printf("\n%-18s%-18s%-18s%-18s%-18s\n",
			"Keysize", "Add", "Lookup", "Lookup_bulk", "Delete");
	for (i = 0; i < NUM_KEYSIZES; i++) {
		printf("%-18d", hashtest_key_lens[i]);
		for (j = 0; j < NUM_OPERATIONS; j++)
			printf("%-18"PRIu64, cycles[i][j]);
		printf("\n");
	}
	return 0;
}

static int
test_efd_perf(void)
{

	if (run_all_tbl_perf_tests() < 0)
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(efd_perf_autotest, test_efd_perf);
