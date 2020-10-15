/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
 */

#include <inttypes.h>
#include <locale.h>

#include <rte_cycles.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_launch.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_spinlock.h>
#include <rte_jhash.h>

#include "test.h"

/*
 * Check condition and return an error if true. Assumes that "handle" is the
 * name of the hash structure pointer to be freed.
 */
#define RETURN_IF_ERROR(cond, str, ...) do {                            \
	if (cond) {                                                     \
		printf("ERROR line %d: " str "\n", __LINE__,            \
							##__VA_ARGS__);	\
		if (handle)                                             \
			rte_hash_free(handle);                          \
		return -1;                                              \
	}                                                               \
} while (0)

#define RTE_APP_TEST_HASH_MULTIWRITER_FAILED 0

struct {
	uint32_t *keys;
	uint32_t *found;
	uint32_t nb_tsx_insertion;
	struct rte_hash *h;
} tbl_multiwriter_test_params;

const uint32_t nb_entries = 5*1024*1024;
const uint32_t nb_total_tsx_insertion = 4.5*1024*1024;
uint32_t rounded_nb_total_tsx_insertion;

static rte_atomic64_t gcycles;
static rte_atomic64_t ginsertions;

static int use_htm;

static int
test_hash_multiwriter_worker(void *arg)
{
	uint64_t i, offset;
	uint16_t pos_core;
	uint32_t lcore_id = rte_lcore_id();
	uint64_t begin, cycles;
	uint16_t *enabled_core_ids = (uint16_t *)arg;

	for (pos_core = 0; pos_core < rte_lcore_count(); pos_core++) {
		if (enabled_core_ids[pos_core] == lcore_id)
			break;
	}

	/*
	 * Calculate offset for entries based on the position of the
	 * logical core, from the main core (not counting not enabled cores)
	 */
	offset = pos_core * tbl_multiwriter_test_params.nb_tsx_insertion;

	printf("Core #%d inserting %d: %'"PRId64" - %'"PRId64"\n",
	       lcore_id, tbl_multiwriter_test_params.nb_tsx_insertion,
	       offset,
	       offset + tbl_multiwriter_test_params.nb_tsx_insertion - 1);

	begin = rte_rdtsc_precise();

	for (i = offset;
	     i < offset + tbl_multiwriter_test_params.nb_tsx_insertion;
	     i++) {
		if (rte_hash_add_key(tbl_multiwriter_test_params.h,
				     tbl_multiwriter_test_params.keys + i) < 0)
			break;
	}

	cycles = rte_rdtsc_precise() - begin;
	rte_atomic64_add(&gcycles, cycles);
	rte_atomic64_add(&ginsertions, i - offset);

	for (; i < offset + tbl_multiwriter_test_params.nb_tsx_insertion; i++)
		tbl_multiwriter_test_params.keys[i]
			= RTE_APP_TEST_HASH_MULTIWRITER_FAILED;

	return 0;
}


static int
test_hash_multiwriter(void)
{
	unsigned int i, rounded_nb_total_tsx_insertion;
	static unsigned calledCount = 1;
	uint16_t enabled_core_ids[RTE_MAX_LCORE];
	uint16_t core_id;

	uint32_t *keys;
	uint32_t *found;

	struct rte_hash_parameters hash_params = {
		.entries = nb_entries,
		.key_len = sizeof(uint32_t),
		.hash_func = rte_jhash,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};
	if (use_htm)
		hash_params.extra_flag =
			RTE_HASH_EXTRA_FLAGS_TRANS_MEM_SUPPORT
				| RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD;
	else
		hash_params.extra_flag =
			RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD;

	struct rte_hash *handle;
	char name[RTE_HASH_NAMESIZE];

	const void *next_key;
	void *next_data;
	uint32_t iter = 0;

	uint32_t duplicated_keys = 0;
	uint32_t lost_keys = 0;
	uint32_t count;

	snprintf(name, 32, "test%u", calledCount++);
	hash_params.name = name;

	handle = rte_hash_create(&hash_params);
	RETURN_IF_ERROR(handle == NULL, "hash creation failed");

	tbl_multiwriter_test_params.h = handle;
	tbl_multiwriter_test_params.nb_tsx_insertion =
		nb_total_tsx_insertion / rte_lcore_count();

	rounded_nb_total_tsx_insertion = (nb_total_tsx_insertion /
		tbl_multiwriter_test_params.nb_tsx_insertion)
		* tbl_multiwriter_test_params.nb_tsx_insertion;

	rte_srand(rte_rdtsc());

	keys = rte_malloc(NULL, sizeof(uint32_t) * nb_entries, 0);

	if (keys == NULL) {
		printf("RTE_MALLOC failed\n");
		goto err1;
	}

	for (i = 0; i < nb_entries; i++)
		keys[i] = i;

	tbl_multiwriter_test_params.keys = keys;

	found = rte_zmalloc(NULL, sizeof(uint32_t) * nb_entries, 0);
	if (found == NULL) {
		printf("RTE_ZMALLOC failed\n");
		goto err2;
	}

	tbl_multiwriter_test_params.found = found;

	rte_atomic64_init(&gcycles);
	rte_atomic64_clear(&gcycles);

	rte_atomic64_init(&ginsertions);
	rte_atomic64_clear(&ginsertions);

	/* Get list of enabled cores */
	i = 0;
	for (core_id = 0; core_id < RTE_MAX_LCORE; core_id++) {
		if (i == rte_lcore_count())
			break;

		if (rte_lcore_is_enabled(core_id)) {
			enabled_core_ids[i] = core_id;
			i++;
		}
	}

	if (i != rte_lcore_count()) {
		printf("Number of enabled cores in list is different from "
				"number given by rte_lcore_count()\n");
		goto err3;
	}

	/* Fire all threads. */
	rte_eal_mp_remote_launch(test_hash_multiwriter_worker,
				 enabled_core_ids, CALL_MAIN);
	rte_eal_mp_wait_lcore();

	count = rte_hash_count(handle);
	if (count != rounded_nb_total_tsx_insertion) {
		printf("rte_hash_count returned wrong value %u, %d\n",
				rounded_nb_total_tsx_insertion, count);
		goto err3;
	}

	while (rte_hash_iterate(handle, &next_key, &next_data, &iter) >= 0) {
		/* Search for the key in the list of keys added .*/
		i = *(const uint32_t *)next_key;
		tbl_multiwriter_test_params.found[i]++;
	}

	for (i = 0; i < rounded_nb_total_tsx_insertion; i++) {
		if (tbl_multiwriter_test_params.keys[i]
		    != RTE_APP_TEST_HASH_MULTIWRITER_FAILED) {
			if (tbl_multiwriter_test_params.found[i] > 1) {
				duplicated_keys++;
				break;
			}
			if (tbl_multiwriter_test_params.found[i] == 0) {
				lost_keys++;
				printf("key %d is lost\n", i);
				break;
			}
		}
	}

	if (duplicated_keys > 0) {
		printf("%d key duplicated\n", duplicated_keys);
		goto err3;
	}

	if (lost_keys > 0) {
		printf("%d key lost\n", lost_keys);
		goto err3;
	}

	printf("No key corrupted during multiwriter insertion.\n");

	unsigned long long int cycles_per_insertion =
		rte_atomic64_read(&gcycles)/
		rte_atomic64_read(&ginsertions);

	printf(" cycles per insertion: %llu\n", cycles_per_insertion);

	rte_free(tbl_multiwriter_test_params.found);
	rte_free(tbl_multiwriter_test_params.keys);
	rte_hash_free(handle);
	return 0;

err3:
	rte_free(tbl_multiwriter_test_params.found);
err2:
	rte_free(tbl_multiwriter_test_params.keys);
err1:
	rte_hash_free(handle);
	return -1;
}

static int
test_hash_multiwriter_main(void)
{
	if (rte_lcore_count() < 2) {
		printf("Not enough cores for distributor_autotest, expecting at least 2\n");
		return TEST_SKIPPED;
	}

	setlocale(LC_NUMERIC, "");


	if (!rte_tm_supported()) {
		printf("Hardware transactional memory (lock elision) "
			"is NOT supported\n");
	} else {
		printf("Hardware transactional memory (lock elision) "
			"is supported\n");

		printf("Test multi-writer with Hardware transactional memory\n");

		use_htm = 1;
		if (test_hash_multiwriter() < 0)
			return -1;
	}

	printf("Test multi-writer without Hardware transactional memory\n");
	use_htm = 0;
	if (test_hash_multiwriter() < 0)
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(hash_multiwriter_autotest, test_hash_multiwriter_main);
