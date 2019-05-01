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
#define TEST_RCU_MAX_LCORE 128
static uint16_t enabled_core_ids[TEST_RCU_MAX_LCORE];
static uint8_t num_cores;

static uint32_t *keys;
#define TOTAL_ENTRY (1024 * 8)
#define COUNTER_VALUE 4096
static uint32_t *hash_data[TEST_RCU_MAX_LCORE][TOTAL_ENTRY];
static volatile uint8_t writer_done;
static volatile uint8_t all_registered;
static volatile uint32_t thr_id;

static struct rte_rcu_qsbr *t[TEST_RCU_MAX_LCORE];
static struct rte_hash *h[TEST_RCU_MAX_LCORE];
static char hash_name[TEST_RCU_MAX_LCORE][8];
static rte_atomic64_t updates, checks;
static rte_atomic64_t update_cycles, check_cycles;

/* Scale down results to 1000 operations to support lower
 * granularity clocks.
 */
#define RCU_SCALE_DOWN 1000

/* Simple way to allocate thread ids in 0 to TEST_RCU_MAX_LCORE space */
static inline uint32_t
alloc_thread_id(void)
{
	uint32_t tmp_thr_id;

	tmp_thr_id = __atomic_fetch_add(&thr_id, 1, __ATOMIC_RELAXED);
	if (tmp_thr_id >= TEST_RCU_MAX_LCORE)
		printf("Invalid thread id %u\n", tmp_thr_id);

	return tmp_thr_id;
}

static inline int
get_enabled_cores_mask(void)
{
	uint16_t core_id;
	uint32_t max_cores = rte_lcore_count();

	if (max_cores > TEST_RCU_MAX_LCORE) {
		printf("Number of cores exceed %d\n", TEST_RCU_MAX_LCORE);
		return -1;
	}

	core_id = 0;
	num_cores = 0;
	RTE_LCORE_FOREACH_SLAVE(core_id) {
		enabled_core_ids[num_cores] = core_id;
		num_cores++;
	}

	return 0;
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
	rte_atomic64_add(&update_cycles, cycles);
	rte_atomic64_add(&updates, loop_cnt);

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
	rte_atomic64_add(&check_cycles, cycles);
	rte_atomic64_add(&checks, loop_cnt);
	return 0;
}

/*
 * Perf test: Reader/writer
 * Single writer, Multiple Readers, Single QS var, Non-Blocking rcu_qsbr_check
 */
static int
test_rcu_qsbr_perf(void)
{
	int i, sz;
	int tmp_num_cores;

	writer_done = 0;

	rte_atomic64_clear(&updates);
	rte_atomic64_clear(&update_cycles);
	rte_atomic64_clear(&checks);
	rte_atomic64_clear(&check_cycles);

	printf("\nPerf Test: %d Readers/1 Writer('wait' in qsbr_check == true)\n",
		num_cores - 1);

	__atomic_store_n(&thr_id, 0, __ATOMIC_SEQ_CST);

	if (all_registered == 1)
		tmp_num_cores = num_cores - 1;
	else
		tmp_num_cores = TEST_RCU_MAX_LCORE;

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

	printf("Total RCU updates = %"PRIi64"\n", rte_atomic64_read(&updates));
	printf("Cycles per %d updates: %"PRIi64"\n", RCU_SCALE_DOWN,
		rte_atomic64_read(&update_cycles) /
		(rte_atomic64_read(&updates) / RCU_SCALE_DOWN));
	printf("Total RCU checks = %"PRIi64"\n", rte_atomic64_read(&checks));
	printf("Cycles per %d checks: %"PRIi64"\n", RCU_SCALE_DOWN,
		rte_atomic64_read(&check_cycles) /
		(rte_atomic64_read(&checks) / RCU_SCALE_DOWN));

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
	int i, sz;
	int tmp_num_cores;

	rte_atomic64_clear(&updates);
	rte_atomic64_clear(&update_cycles);

	__atomic_store_n(&thr_id, 0, __ATOMIC_SEQ_CST);

	printf("\nPerf Test: %d Readers\n", num_cores);

	if (all_registered == 1)
		tmp_num_cores = num_cores;
	else
		tmp_num_cores = TEST_RCU_MAX_LCORE;

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

	printf("Total RCU updates = %"PRIi64"\n", rte_atomic64_read(&updates));
	printf("Cycles per %d updates: %"PRIi64"\n", RCU_SCALE_DOWN,
		rte_atomic64_read(&update_cycles) /
		(rte_atomic64_read(&updates) / RCU_SCALE_DOWN));

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
	int i, sz;

	rte_atomic64_clear(&checks);
	rte_atomic64_clear(&check_cycles);

	__atomic_store_n(&thr_id, 0, __ATOMIC_SEQ_CST);

	printf("\nPerf test: %d Writers ('wait' in qsbr_check == false)\n",
		num_cores);

	/* Number of readers does not matter for QS variable in this test
	 * case as no reader will be registered.
	 */
	sz = rte_rcu_qsbr_get_memsize(TEST_RCU_MAX_LCORE);
	t[0] = (struct rte_rcu_qsbr *)rte_zmalloc("rcu0", sz,
						RTE_CACHE_LINE_SIZE);
	/* QS variable is initialized */
	rte_rcu_qsbr_init(t[0], TEST_RCU_MAX_LCORE);

	/* Writer threads are launched */
	for (i = 0; i < num_cores; i++)
		rte_eal_remote_launch(test_rcu_qsbr_writer_perf,
				(void *)0, enabled_core_ids[i]);

	/* Wait until all readers have exited */
	rte_eal_mp_wait_lcore();

	printf("Total RCU checks = %"PRIi64"\n", rte_atomic64_read(&checks));
	printf("Cycles per %d checks: %"PRIi64"\n", RCU_SCALE_DOWN,
		rte_atomic64_read(&check_cycles) /
		(rte_atomic64_read(&checks) / RCU_SCALE_DOWN));

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
	hash = h[read_type];

	rte_rcu_qsbr_thread_register(temp, thread_id);

	begin = rte_rdtsc_precise();

	do {
		rte_rcu_qsbr_thread_online(temp, thread_id);
		for (i = 0; i < TOTAL_ENTRY; i++) {
			rte_rcu_qsbr_lock(temp, thread_id);
			if (rte_hash_lookup_data(hash, keys+i,
					(void **)&pdata) != -ENOENT) {
				*pdata = 0;
				while (*pdata < COUNTER_VALUE)
					++*pdata;
			}
			rte_rcu_qsbr_unlock(temp, thread_id);
		}
		/* Update quiescent state counter */
		rte_rcu_qsbr_quiescent(temp, thread_id);
		rte_rcu_qsbr_thread_offline(temp, thread_id);
		loop_cnt++;
	} while (!writer_done);

	cycles = rte_rdtsc_precise() - begin;
	rte_atomic64_add(&update_cycles, cycles);
	rte_atomic64_add(&updates, loop_cnt);

	rte_rcu_qsbr_thread_unregister(temp, thread_id);

	return 0;
}

static struct rte_hash *
init_hash(int hash_id)
{
	int i;
	struct rte_hash *h = NULL;

	sprintf(hash_name[hash_id], "hash%d", hash_id);
	struct rte_hash_parameters hash_params = {
		.entries = TOTAL_ENTRY,
		.key_len = sizeof(uint32_t),
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
		.hash_func = rte_hash_crc,
		.extra_flag =
			RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY_LF,
		.name = hash_name[hash_id],
	};

	h = rte_hash_create(&hash_params);
	if (h == NULL) {
		printf("Hash create Failed\n");
		return NULL;
	}

	for (i = 0; i < TOTAL_ENTRY; i++) {
		hash_data[hash_id][i] = rte_zmalloc(NULL, sizeof(uint32_t), 0);
		if (hash_data[hash_id][i] == NULL) {
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
		if (rte_hash_add_key_data(h, keys + i,
				(void *)((uintptr_t)hash_data[hash_id][i]))
				< 0) {
			printf("Hash key add Failed #%d\n", i);
			return NULL;
		}
	}
	return h;
}

/*
 * Functional test:
 * Single writer, Single QS variable Single QSBR query, Blocking rcu_qsbr_check
 */
static int
test_rcu_qsbr_sw_sv_1qs(void)
{
	uint64_t token, begin, cycles;
	int i, tmp_num_cores, sz;
	int32_t pos;

	writer_done = 0;

	rte_atomic64_clear(&updates);
	rte_atomic64_clear(&update_cycles);
	rte_atomic64_clear(&checks);
	rte_atomic64_clear(&check_cycles);

	__atomic_store_n(&thr_id, 0, __ATOMIC_SEQ_CST);

	printf("\nPerf test: 1 writer, %d readers, 1 QSBR variable, 1 QSBR Query, Blocking QSBR Check\n", num_cores);

	if (all_registered == 1)
		tmp_num_cores = num_cores;
	else
		tmp_num_cores = TEST_RCU_MAX_LCORE;

	sz = rte_rcu_qsbr_get_memsize(tmp_num_cores);
	t[0] = (struct rte_rcu_qsbr *)rte_zmalloc("rcu0", sz,
						RTE_CACHE_LINE_SIZE);
	/* QS variable is initialized */
	rte_rcu_qsbr_init(t[0], tmp_num_cores);

	/* Shared data structure created */
	h[0] = init_hash(0);
	if (h[0] == NULL) {
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
		pos = rte_hash_del_key(h[0], keys + i);
		if (pos < 0) {
			printf("Delete key failed #%d\n", keys[i]);
			goto error;
		}
		/* Start the quiescent state query process */
		token = rte_rcu_qsbr_start(t[0]);

		/* Check the quiescent state status */
		rte_rcu_qsbr_check(t[0], token, true);
		if (*hash_data[0][i] != COUNTER_VALUE &&
			*hash_data[0][i] != 0) {
			printf("Reader did not complete #%d =  %d\n", i,
							*hash_data[0][i]);
			goto error;
		}

		if (rte_hash_free_key_with_position(h[0], pos) < 0) {
			printf("Failed to free the key #%d\n", keys[i]);
			goto error;
		}
		rte_free(hash_data[0][i]);
		hash_data[0][i] = NULL;
	}

	cycles = rte_rdtsc_precise() - begin;
	rte_atomic64_add(&check_cycles, cycles);
	rte_atomic64_add(&checks, i);

	writer_done = 1;

	/* Wait until all readers have exited */
	rte_eal_mp_wait_lcore();
	/* Check return value from threads */
	for (i = 0; i < num_cores; i++)
		if (lcore_config[enabled_core_ids[i]].ret < 0)
			goto error;
	rte_hash_free(h[0]);
	rte_free(keys);

	printf("Following numbers include calls to rte_hash functions\n");
	printf("Cycles per 1 update(online/update/offline): %"PRIi64"\n",
		rte_atomic64_read(&update_cycles) /
		rte_atomic64_read(&updates));

	printf("Cycles per 1 check(start, check): %"PRIi64"\n\n",
		rte_atomic64_read(&check_cycles) /
		rte_atomic64_read(&checks));

	rte_free(t[0]);

	return 0;

error:
	writer_done = 1;
	/* Wait until all readers have exited */
	rte_eal_mp_wait_lcore();

	rte_hash_free(h[0]);
	rte_free(keys);
	for (i = 0; i < TOTAL_ENTRY; i++)
		rte_free(hash_data[0][i]);

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
	int i, ret, tmp_num_cores, sz;
	int32_t pos;

	writer_done = 0;

	printf("Perf test: 1 writer, %d readers, 1 QSBR variable, 1 QSBR Query, Non-Blocking QSBR check\n", num_cores);

	__atomic_store_n(&thr_id, 0, __ATOMIC_SEQ_CST);

	if (all_registered == 1)
		tmp_num_cores = num_cores;
	else
		tmp_num_cores = TEST_RCU_MAX_LCORE;

	sz = rte_rcu_qsbr_get_memsize(tmp_num_cores);
	t[0] = (struct rte_rcu_qsbr *)rte_zmalloc("rcu0", sz,
						RTE_CACHE_LINE_SIZE);
	/* QS variable is initialized */
	rte_rcu_qsbr_init(t[0], tmp_num_cores);

	/* Shared data structure created */
	h[0] = init_hash(0);
	if (h[0] == NULL) {
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
		pos = rte_hash_del_key(h[0], keys + i);
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
		if (*hash_data[0][i] != COUNTER_VALUE &&
			*hash_data[0][i] != 0) {
			printf("Reader did not complete  #%d = %d\n", i,
							*hash_data[0][i]);
			goto error;
		}

		if (rte_hash_free_key_with_position(h[0], pos) < 0) {
			printf("Failed to free the key #%d\n", keys[i]);
			goto error;
		}
		rte_free(hash_data[0][i]);
		hash_data[0][i] = NULL;
	}

	cycles = rte_rdtsc_precise() - begin;
	rte_atomic64_add(&check_cycles, cycles);
	rte_atomic64_add(&checks, i);

	writer_done = 1;
	/* Wait until all readers have exited */
	rte_eal_mp_wait_lcore();
	/* Check return value from threads */
	for (i = 0; i < num_cores; i++)
		if (lcore_config[enabled_core_ids[i]].ret < 0)
			goto error;
	rte_hash_free(h[0]);
	rte_free(keys);

	printf("Following numbers include calls to rte_hash functions\n");
	printf("Cycles per 1 update(online/update/offline): %"PRIi64"\n",
		rte_atomic64_read(&update_cycles) /
		rte_atomic64_read(&updates));

	printf("Cycles per 1 check(start, check): %"PRIi64"\n\n",
		rte_atomic64_read(&check_cycles) /
		rte_atomic64_read(&checks));

	rte_free(t[0]);

	return 0;

error:
	writer_done = 1;
	/* Wait until all readers have exited */
	rte_eal_mp_wait_lcore();

	rte_hash_free(h[0]);
	rte_free(keys);
	for (i = 0; i < TOTAL_ENTRY; i++)
		rte_free(hash_data[0][i]);

	rte_free(t[0]);

	return -1;
}

static int
test_rcu_qsbr_main(void)
{
	rte_atomic64_init(&updates);
	rte_atomic64_init(&update_cycles);
	rte_atomic64_init(&checks);
	rte_atomic64_init(&check_cycles);

	if (get_enabled_cores_mask() != 0)
		return -1;

	printf("Number of cores provided = %d\n", num_cores);
	if (num_cores < 2) {
		printf("Test failed! Need 2 or more cores\n");
		goto test_fail;
	}
	if (num_cores > TEST_RCU_MAX_LCORE) {
		printf("Test failed! %d cores supported\n", TEST_RCU_MAX_LCORE);
		goto test_fail;
	}

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
	 * TEST_RCU_MAX_LCORE. This will allow for some threads not
	 * to be registered on the QS variable.
	 */
	if (num_cores >= TEST_RCU_MAX_LCORE) {
		printf("Test failed! number of cores provided should be less than %d\n",
			TEST_RCU_MAX_LCORE);
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

REGISTER_TEST_COMMAND(rcu_qsbr_perf_autotest, test_rcu_qsbr_main);
