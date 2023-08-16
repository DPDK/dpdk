/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018 Arm Limited
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <rte_pause.h>
#include <rte_rcu_qsbr.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <unistd.h>

#include "test.h"

/* Check condition and return an error if true. */
static uint16_t enabled_core_ids[RTE_MAX_LCORE];
static unsigned int num_cores;

static uint32_t *keys;
#define TOTAL_ENTRY (1024 * 8)
#define COUNTER_VALUE 4096
static uint32_t *hash_data[TOTAL_ENTRY];
static volatile uint8_t writer_done;
static volatile uint8_t all_registered;
static volatile uint32_t thr_id;

static struct rte_rcu_qsbr *t[RTE_MAX_LCORE];
static struct rte_hash *h;
static char hash_name[8];
static uint64_t updates, checks;
static uint64_t update_cycles, check_cycles;

/* Scale down results to 1000 operations to support lower
 * granularity clocks.
 */
#define RCU_SCALE_DOWN 1000

/* Simple way to allocate thread ids in 0 to RTE_MAX_LCORE space */
static inline uint32_t
alloc_thread_id(void)
{
	uint32_t tmp_thr_id;

	tmp_thr_id = __atomic_fetch_add(&thr_id, 1, __ATOMIC_RELAXED);
	if (tmp_thr_id >= RTE_MAX_LCORE)
		printf("Invalid thread id %u\n", tmp_thr_id);

	return tmp_thr_id;
}

static int
test_rcu_qsbr_reader_perf(void *arg)
{
	bool writer_present = (bool)arg;
	uint32_t thread_id = alloc_thread_id();
	uint64_t loop_cnt = 0;
	uint64_t begin, cycles;

	/* Register for report QS */
	rte_rcu_qsbr_thread_register(t[0], thread_id);
	/* Make the thread online */
	rte_rcu_qsbr_thread_online(t[0], thread_id);

	begin = rte_rdtsc_precise();

	if (writer_present) {
		while (!writer_done) {
			/* Update quiescent state counter */
			rte_rcu_qsbr_quiescent(t[0], thread_id);
			loop_cnt++;
		}
	} else {
		while (loop_cnt < 100000000) {
			/* Update quiescent state counter */
			rte_rcu_qsbr_quiescent(t[0], thread_id);
			loop_cnt++;
		}
	}

	cycles = rte_rdtsc_precise() - begin;
	__atomic_fetch_add(&update_cycles, cycles, __ATOMIC_RELAXED);
	__atomic_fetch_add(&updates, loop_cnt, __ATOMIC_RELAXED);

	/* Make the thread offline */
	rte_rcu_qsbr_thread_offline(t[0], thread_id);
	/* Unregister before exiting to avoid writer from waiting */
	rte_rcu_qsbr_thread_unregister(t[0], thread_id);

	return 0;
}

static int
test_rcu_qsbr_writer_perf(void *arg)
{
	bool wait = (bool)arg;
	uint64_t token = 0;
	uint64_t loop_cnt = 0;
	uint64_t begin, cycles;

	begin = rte_rdtsc_precise();

	do {
		/* Start the quiescent state query process */
		if (wait)
			token = rte_rcu_qsbr_start(t[0]);

		/* Check quiescent state status */
		rte_rcu_qsbr_check(t[0], token, wait);
		loop_cnt++;
	} while (loop_cnt < 20000000);

	cycles = rte_rdtsc_precise() - begin;
	__atomic_fetch_add(&check_cycles, cycles, __ATOMIC_RELAXED);
	__atomic_fetch_add(&checks, loop_cnt, __ATOMIC_RELAXED);
	return 0;
}

/*
 * Perf test: Reader/writer
 * Single writer, Multiple Readers, Single QS var, Non-Blocking rcu_qsbr_check
 */
static int
test_rcu_qsbr_perf(void)
{
	size_t sz;
	unsigned int i, tmp_num_cores;

	writer_done = 0;

	__atomic_store_n(&updates, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&update_cycles, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&checks, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&check_cycles, 0, __ATOMIC_RELAXED);

	printf("\nPerf Test: %d Readers/1 Writer('wait' in qsbr_check == true)\n",
		num_cores - 1);

	__atomic_store_n(&thr_id, 0, __ATOMIC_SEQ_CST);

	if (all_registered == 1)
		tmp_num_cores = num_cores - 1;
	else
		tmp_num_cores = RTE_MAX_LCORE;

	sz = rte_rcu_qsbr_get_memsize(tmp_num_cores);
	t[0] = (struct rte_rcu_qsbr *)rte_zmalloc("rcu0", sz,
						RTE_CACHE_LINE_SIZE);
	/* QS variable is initialized */
	rte_rcu_qsbr_init(t[0], tmp_num_cores);

	/* Reader threads are launched */
	for (i = 0; i < num_cores - 1; i++)
		rte_eal_remote_launch(test_rcu_qsbr_reader_perf, (void *)1,
					enabled_core_ids[i]);

	/* Writer thread is launched */
	rte_eal_remote_launch(test_rcu_qsbr_writer_perf,
			      (void *)1, enabled_core_ids[i]);

	/* Wait for the writer thread */
	rte_eal_wait_lcore(enabled_core_ids[i]);
	writer_done = 1;

	/* Wait until all readers have exited */
	rte_eal_mp_wait_lcore();

	printf("Total quiescent state updates = %"PRIi64"\n",
		__atomic_load_n(&updates, __ATOMIC_RELAXED));
	printf("Cycles per %d quiescent state updates: %"PRIi64"\n",
		RCU_SCALE_DOWN,
		__atomic_load_n(&update_cycles, __ATOMIC_RELAXED) /
		(__atomic_load_n(&updates, __ATOMIC_RELAXED) / RCU_SCALE_DOWN));
	printf("Total RCU checks = %"PRIi64"\n", __atomic_load_n(&checks, __ATOMIC_RELAXED));
	printf("Cycles per %d checks: %"PRIi64"\n", RCU_SCALE_DOWN,
		__atomic_load_n(&check_cycles, __ATOMIC_RELAXED) /
		(__atomic_load_n(&checks, __ATOMIC_RELAXED) / RCU_SCALE_DOWN));

	rte_free(t[0]);

	return 0;
}

/*
 * Perf test: Readers
 * Single writer, Multiple readers, Single QS variable
 */
static int
test_rcu_qsbr_rperf(void)
{
	size_t sz;
	unsigned int i, tmp_num_cores;

	__atomic_store_n(&updates, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&update_cycles, 0, __ATOMIC_RELAXED);

	__atomic_store_n(&thr_id, 0, __ATOMIC_SEQ_CST);

	printf("\nPerf Test: %d Readers\n", num_cores);

	if (all_registered == 1)
		tmp_num_cores = num_cores;
	else
		tmp_num_cores = RTE_MAX_LCORE;

	sz = rte_rcu_qsbr_get_memsize(tmp_num_cores);
	t[0] = (struct rte_rcu_qsbr *)rte_zmalloc("rcu0", sz,
						RTE_CACHE_LINE_SIZE);
	/* QS variable is initialized */
	rte_rcu_qsbr_init(t[0], tmp_num_cores);

	/* Reader threads are launched */
	for (i = 0; i < num_cores; i++)
		rte_eal_remote_launch(test_rcu_qsbr_reader_perf, NULL,
					enabled_core_ids[i]);

	/* Wait until all readers have exited */
	rte_eal_mp_wait_lcore();

	printf("Total quiescent state updates = %"PRIi64"\n",
		__atomic_load_n(&updates, __ATOMIC_RELAXED));
	printf("Cycles per %d quiescent state updates: %"PRIi64"\n",
		RCU_SCALE_DOWN,
		__atomic_load_n(&update_cycles, __ATOMIC_RELAXED) /
		(__atomic_load_n(&updates, __ATOMIC_RELAXED) / RCU_SCALE_DOWN));

	rte_free(t[0]);

	return 0;
}

/*
 * Perf test:
 * Multiple writer, Single QS variable, Non-blocking rcu_qsbr_check
 */
static int
test_rcu_qsbr_wperf(void)
{
	size_t sz;
	unsigned int i;

	__atomic_store_n(&checks, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&check_cycles, 0, __ATOMIC_RELAXED);

	__atomic_store_n(&thr_id, 0, __ATOMIC_SEQ_CST);

	printf("\nPerf test: %d Writers ('wait' in qsbr_check == false)\n",
		num_cores);

	/* Number of readers does not matter for QS variable in this test
	 * case as no reader will be registered.
	 */
	sz = rte_rcu_qsbr_get_memsize(RTE_MAX_LCORE);
	t[0] = (struct rte_rcu_qsbr *)rte_zmalloc("rcu0", sz,
						RTE_CACHE_LINE_SIZE);
	/* QS variable is initialized */
	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);

	/* Writer threads are launched */
	for (i = 0; i < num_cores; i++)
		rte_eal_remote_launch(test_rcu_qsbr_writer_perf,
				(void *)0, enabled_core_ids[i]);

	/* Wait until all readers have exited */
	rte_eal_mp_wait_lcore();

	printf("Total RCU checks = %"PRIi64"\n", __atomic_load_n(&checks, __ATOMIC_RELAXED));
	printf("Cycles per %d checks: %"PRIi64"\n", RCU_SCALE_DOWN,
		__atomic_load_n(&check_cycles, __ATOMIC_RELAXED) /
		(__atomic_load_n(&checks, __ATOMIC_RELAXED) / RCU_SCALE_DOWN));

	rte_free(t[0]);

	return 0;
}

/*
 * RCU test cases using rte_hash data structure.
 */
static int
test_rcu_qsbr_hash_reader(void *arg)
{
	struct rte_rcu_qsbr *temp;
	struct rte_hash *hash = NULL;
	int i;
	uint64_t loop_cnt = 0;
	uint64_t begin, cycles;
	uint32_t thread_id = alloc_thread_id();
	uint8_t read_type = (uint8_t)((uintptr_t)arg);
	uint32_t *pdata;

	temp = t[read_type];
	hash = h;

	rte_rcu_qsbr_thread_register(temp, thread_id);

	begin = rte_rdtsc_precise();

	do {
		rte_rcu_qsbr_thread_online(temp, thread_id);
		for (i = 0; i < TOTAL_ENTRY; i++) {
			rte_rcu_qsbr_lock(temp, thread_id);
			if (rte_hash_lookup_data(hash, keys + i,
					(void **)&pdata) != -ENOENT) {
				pdata[thread_id] = 0;
				while (pdata[thread_id] < COUNTER_VALUE)
					pdata[thread_id]++;
			}
			rte_rcu_qsbr_unlock(temp, thread_id);
		}
		/* Update quiescent state counter */
		rte_rcu_qsbr_quiescent(temp, thread_id);
		rte_rcu_qsbr_thread_offline(temp, thread_id);
		loop_cnt++;
	} while (!writer_done);

	cycles = rte_rdtsc_precise() - begin;
	__atomic_fetch_add(&update_cycles, cycles, __ATOMIC_RELAXED);
	__atomic_fetch_add(&updates, loop_cnt, __ATOMIC_RELAXED);

	rte_rcu_qsbr_thread_unregister(temp, thread_id);

	return 0;
}

static struct rte_hash *init_hash(void)
{
	int i;
	struct rte_hash *hash = NULL;

	snprintf(hash_name, 8, "hash");
	struct rte_hash_parameters hash_params = {
		.entries = TOTAL_ENTRY,
		.key_len = sizeof(uint32_t),
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
		.hash_func = rte_hash_crc,
		.extra_flag =
			RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF,
		.name = hash_name,
	};

	hash = rte_hash_create(&hash_params);
	if (hash == NULL) {
		printf("Hash create Failed\n");
		return NULL;
	}

	for (i = 0; i < TOTAL_ENTRY; i++) {
		hash_data[i] = rte_zmalloc(NULL,
				sizeof(uint32_t) * RTE_MAX_LCORE, 0);
		if (hash_data[i] == NULL) {
			printf("No memory\n");
			return NULL;
		}
	}
	keys = rte_malloc(NULL, sizeof(uint32_t) * TOTAL_ENTRY, 0);
	if (keys == NULL) {
		printf("No memory\n");
		return NULL;
	}

	for (i = 0; i < TOTAL_ENTRY; i++)
		keys[i] = i;

	for (i = 0; i < TOTAL_ENTRY; i++) {
		if (rte_hash_add_key_data(hash, keys + i,
				(void *)((uintptr_t)hash_data[i])) < 0) {
			printf("Hash key add Failed #%d\n", i);
			return NULL;
		}
	}
	return hash;
}

/*
 * Functional test:
 * Single writer, Single QS variable Single QSBR query, Blocking rcu_qsbr_check
 */
static int
test_rcu_qsbr_sw_sv_1qs(void)
{
	uint64_t token, begin, cycles;
	size_t sz;
	unsigned int i, j, tmp_num_cores;
	int32_t pos;

	writer_done = 0;

	__atomic_store_n(&updates, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&update_cycles, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&checks, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&check_cycles, 0, __ATOMIC_RELAXED);

	__atomic_store_n(&thr_id, 0, __ATOMIC_SEQ_CST);

	printf("\nPerf test: 1 writer, %d readers, 1 QSBR variable, 1 QSBR Query, Blocking QSBR Check\n", num_cores);

	if (all_registered == 1)
		tmp_num_cores = num_cores;
	else
		tmp_num_cores = RTE_MAX_LCORE;

	sz = rte_rcu_qsbr_get_memsize(tmp_num_cores);
	t[0] = (struct rte_rcu_qsbr *)rte_zmalloc("rcu0", sz,
						RTE_CACHE_LINE_SIZE);
	/* QS variable is initialized */
	rte_rcu_qsbr_init(t[0], tmp_num_cores);

	/* Shared data structure created */
	h = init_hash();
	if (h == NULL) {
		printf("Hash init failed\n");
		goto error;
	}

	/* Reader threads are launched */
	for (i = 0; i < num_cores; i++)
		rte_eal_remote_launch(test_rcu_qsbr_hash_reader, NULL,
					enabled_core_ids[i]);

	begin = rte_rdtsc_precise();

	for (i = 0; i < TOTAL_ENTRY; i++) {
		/* Delete elements from the shared data structure */
		pos = rte_hash_del_key(h, keys + i);
		if (pos < 0) {
			printf("Delete key failed #%d\n", keys[i]);
			goto error;
		}
		/* Start the quiescent state query process */
		token = rte_rcu_qsbr_start(t[0]);

		/* Check the quiescent state status */
		rte_rcu_qsbr_check(t[0], token, true);
		for (j = 0; j < tmp_num_cores; j++) {
			if (hash_data[i][j] != COUNTER_VALUE &&
				hash_data[i][j] != 0) {
				printf("Reader thread ID %u did not complete #%d =  %d\n",
					j, i, hash_data[i][j]);
				goto error;
			}
		}

		if (rte_hash_free_key_with_position(h, pos) < 0) {
			printf("Failed to free the key #%d\n", keys[i]);
			goto error;
		}
		rte_free(hash_data[i]);
		hash_data[i] = NULL;
	}

	cycles = rte_rdtsc_precise() - begin;
	__atomic_fetch_add(&check_cycles, cycles, __ATOMIC_RELAXED);
	__atomic_fetch_add(&checks, i, __ATOMIC_RELAXED);

	writer_done = 1;

	/* Wait and check return value from reader threads */
	for (i = 0; i < num_cores; i++)
		if (rte_eal_wait_lcore(enabled_core_ids[i]) < 0)
			goto error;
	rte_hash_free(h);
	rte_free(keys);

	printf("Following numbers include calls to rte_hash functions\n");
	printf("Cycles per 1 quiescent state update(online/update/offline): %"PRIi64"\n",
		__atomic_load_n(&update_cycles, __ATOMIC_RELAXED) /
		__atomic_load_n(&updates, __ATOMIC_RELAXED));

	printf("Cycles per 1 check(start, check): %"PRIi64"\n\n",
		__atomic_load_n(&check_cycles, __ATOMIC_RELAXED) /
		__atomic_load_n(&checks, __ATOMIC_RELAXED));

	rte_free(t[0]);

	return 0;

error:
	writer_done = 1;
	/* Wait until all readers have exited */
	rte_eal_mp_wait_lcore();

	rte_hash_free(h);
	rte_free(keys);
	for (i = 0; i < TOTAL_ENTRY; i++)
		rte_free(hash_data[i]);

	rte_free(t[0]);

	return -1;
}

/*
 * Functional test:
 * Single writer, Single QS variable, Single QSBR query,
 * Non-blocking rcu_qsbr_check
 */
static int
test_rcu_qsbr_sw_sv_1qs_non_blocking(void)
{
	uint64_t token, begin, cycles;
	int ret;
	size_t sz;
	unsigned int i, j, tmp_num_cores;
	int32_t pos;

	writer_done = 0;

	printf("Perf test: 1 writer, %d readers, 1 QSBR variable, 1 QSBR Query, Non-Blocking QSBR check\n", num_cores);

	__atomic_store_n(&thr_id, 0, __ATOMIC_RELAXED);

	if (all_registered == 1)
		tmp_num_cores = num_cores;
	else
		tmp_num_cores = RTE_MAX_LCORE;

	sz = rte_rcu_qsbr_get_memsize(tmp_num_cores);
	t[0] = (struct rte_rcu_qsbr *)rte_zmalloc("rcu0", sz,
						RTE_CACHE_LINE_SIZE);
	/* QS variable is initialized */
	rte_rcu_qsbr_init(t[0], tmp_num_cores);

	/* Shared data structure created */
	h = init_hash();
	if (h == NULL) {
		printf("Hash init failed\n");
		goto error;
	}

	/* Reader threads are launched */
	for (i = 0; i < num_cores; i++)
		rte_eal_remote_launch(test_rcu_qsbr_hash_reader, NULL,
					enabled_core_ids[i]);

	begin = rte_rdtsc_precise();

	for (i = 0; i < TOTAL_ENTRY; i++) {
		/* Delete elements from the shared data structure */
		pos = rte_hash_del_key(h, keys + i);
		if (pos < 0) {
			printf("Delete key failed #%d\n", keys[i]);
			goto error;
		}
		/* Start the quiescent state query process */
		token = rte_rcu_qsbr_start(t[0]);

		/* Check the quiescent state status */
		do {
			ret = rte_rcu_qsbr_check(t[0], token, false);
		} while (ret == 0);
		for (j = 0; j < tmp_num_cores; j++) {
			if (hash_data[i][j] != COUNTER_VALUE &&
				hash_data[i][j] != 0) {
				printf("Reader thread ID %u did not complete #%d =  %d\n",
					j, i, hash_data[i][j]);
				goto error;
			}
		}

		if (rte_hash_free_key_with_position(h, pos) < 0) {
			printf("Failed to free the key #%d\n", keys[i]);
			goto error;
		}
		rte_free(hash_data[i]);
		hash_data[i] = NULL;
	}

	cycles = rte_rdtsc_precise() - begin;
	__atomic_fetch_add(&check_cycles, cycles, __ATOMIC_RELAXED);
	__atomic_fetch_add(&checks, i, __ATOMIC_RELAXED);

	writer_done = 1;
	/* Wait and check return value from reader threads */
	for (i = 0; i < num_cores; i++)
		if (rte_eal_wait_lcore(enabled_core_ids[i]) < 0)
			goto error;
	rte_hash_free(h);
	rte_free(keys);

	printf("Following numbers include calls to rte_hash functions\n");
	printf("Cycles per 1 quiescent state update(online/update/offline): %"PRIi64"\n",
		__atomic_load_n(&update_cycles, __ATOMIC_RELAXED) /
		__atomic_load_n(&updates, __ATOMIC_RELAXED));

	printf("Cycles per 1 check(start, check): %"PRIi64"\n\n",
		__atomic_load_n(&check_cycles, __ATOMIC_RELAXED) /
		__atomic_load_n(&checks, __ATOMIC_RELAXED));

	rte_free(t[0]);

	return 0;

error:
	writer_done = 1;
	/* Wait until all readers have exited */
	rte_eal_mp_wait_lcore();

	rte_hash_free(h);
	rte_free(keys);
	for (i = 0; i < TOTAL_ENTRY; i++)
		rte_free(hash_data[i]);

	rte_free(t[0]);

	return -1;
}

static int
test_rcu_qsbr_main(void)
{
	uint16_t core_id;

	if (RTE_EXEC_ENV_IS_WINDOWS)
		return TEST_SKIPPED;

	if (rte_lcore_count() < 3) {
		printf("Not enough cores for rcu_qsbr_perf_autotest, expecting at least 3\n");
		return TEST_SKIPPED;
	}

	__atomic_store_n(&updates, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&update_cycles, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&checks, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&check_cycles, 0, __ATOMIC_RELAXED);

	num_cores = 0;
	RTE_LCORE_FOREACH_WORKER(core_id) {
		enabled_core_ids[num_cores] = core_id;
		num_cores++;
	}

	printf("Number of cores provided = %d\n", num_cores);
	printf("Perf test with all reader threads registered\n");
	printf("--------------------------------------------\n");
	all_registered = 1;

	if (test_rcu_qsbr_perf() < 0)
		goto test_fail;

	if (test_rcu_qsbr_rperf() < 0)
		goto test_fail;

	if (test_rcu_qsbr_wperf() < 0)
		goto test_fail;

	if (test_rcu_qsbr_sw_sv_1qs() < 0)
		goto test_fail;

	if (test_rcu_qsbr_sw_sv_1qs_non_blocking() < 0)
		goto test_fail;

	/* Make sure the actual number of cores provided is less than
	 * RTE_MAX_LCORE. This will allow for some threads not
	 * to be registered on the QS variable.
	 */
	if (num_cores >= RTE_MAX_LCORE) {
		printf("Test failed! number of cores provided should be less than %d\n",
			RTE_MAX_LCORE);
		goto test_fail;
	}

	printf("Perf test with some of reader threads registered\n");
	printf("------------------------------------------------\n");
	all_registered = 0;

	if (test_rcu_qsbr_perf() < 0)
		goto test_fail;

	if (test_rcu_qsbr_rperf() < 0)
		goto test_fail;

	if (test_rcu_qsbr_wperf() < 0)
		goto test_fail;

	if (test_rcu_qsbr_sw_sv_1qs() < 0)
		goto test_fail;

	if (test_rcu_qsbr_sw_sv_1qs_non_blocking() < 0)
		goto test_fail;

	printf("\n");

	return 0;

test_fail:
	return -1;
}

REGISTER_PERF_TEST(rcu_qsbr_perf_autotest, test_rcu_qsbr_main);
