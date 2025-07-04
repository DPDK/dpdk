/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2019-2020 Arm Limited
 */

#include <stdio.h>
#include <string.h>
#include <rte_pause.h>
#include <rte_rcu_qsbr.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_random.h>
#include <unistd.h>

#include "test.h"

/* Check condition and return an error if true. */
#define TEST_RCU_QSBR_RETURN_IF_ERROR(cond, str, ...) \
do { \
	if (cond) { \
		printf("ERROR file %s, line %d: " str "\n", __FILE__, \
			__LINE__, ##__VA_ARGS__); \
		return -1; \
	} \
} while (0)

/* Check condition and go to label if true. */
#define TEST_RCU_QSBR_GOTO_IF_ERROR(label, cond, str, ...) \
do { \
	if (cond) { \
		printf("ERROR file %s, line %d: " str "\n", __FILE__, \
			__LINE__, ##__VA_ARGS__); \
		goto label; \
	} \
} while (0)

/* Make sure that this has the same value as __RTE_QSBR_CNT_INIT */
#define TEST_RCU_QSBR_CNT_INIT 1

static uint16_t enabled_core_ids[RTE_MAX_LCORE];
static unsigned int num_cores;

static uint32_t *keys;
#define TOTAL_ENTRY (1024 * 8)
#define COUNTER_VALUE 4096
static uint32_t *hash_data[RTE_MAX_LCORE][TOTAL_ENTRY];
static uint8_t writer_done;
static uint8_t cb_failed;

static struct rte_rcu_qsbr *t[RTE_MAX_LCORE];
static struct rte_hash *h[RTE_MAX_LCORE];
static char hash_name[RTE_MAX_LCORE][8];

struct test_rcu_thread_info {
	/* Index in RCU array */
	int ir;
	/* Index in hash array */
	int ih;
	/* lcore IDs registered on the RCU variable */
	uint16_t r_core_ids[2];
};
static struct test_rcu_thread_info thread_info[RTE_MAX_LCORE/4];

static int
alloc_rcu(void)
{
	int i;
	size_t sz;

	sz = rte_rcu_qsbr_get_memsize(RTE_MAX_LCORE);

	for (i = 0; i < RTE_MAX_LCORE; i++)
		t[i] = (struct rte_rcu_qsbr *)rte_zmalloc(NULL, sz,
						RTE_CACHE_LINE_SIZE);

	return 0;
}

static int
free_rcu(void)
{
	int i;

	for (i = 0; i < RTE_MAX_LCORE; i++)
		rte_free(t[i]);

	return 0;
}

/*
 * rte_rcu_qsbr_thread_register: Add a reader thread, to the list of threads
 * reporting their quiescent state on a QS variable.
 */
static int
test_rcu_qsbr_get_memsize(void)
{
	size_t sz;

	printf("\nTest rte_rcu_qsbr_thread_register()\n");

	sz = rte_rcu_qsbr_get_memsize(0);
	TEST_RCU_QSBR_RETURN_IF_ERROR((sz != 1), "Get Memsize for 0 threads");

	sz = rte_rcu_qsbr_get_memsize(128);
	/* For 128 threads,
	 * for machines with cache line size of 64B - 8384
	 * for machines with cache line size of 128 - 16768
	 */
	if (RTE_CACHE_LINE_SIZE == 64)
		TEST_RCU_QSBR_RETURN_IF_ERROR((sz != 8384),
			"Get Memsize for 128 threads");
	else if (RTE_CACHE_LINE_SIZE == 128)
		TEST_RCU_QSBR_RETURN_IF_ERROR((sz != 16768),
			"Get Memsize for 128 threads");

	return 0;
}

/*
 * rte_rcu_qsbr_init: Initialize a QSBR variable.
 */
static int
test_rcu_qsbr_init(void)
{
	int r;

	printf("\nTest rte_rcu_qsbr_init()\n");

	r = rte_rcu_qsbr_init(NULL, RTE_MAX_LCORE);
	TEST_RCU_QSBR_RETURN_IF_ERROR((r != 1), "NULL variable");

	return 0;
}

/*
 * rte_rcu_qsbr_thread_register: Add a reader thread, to the list of threads
 * reporting their quiescent state on a QS variable.
 */
static int
test_rcu_qsbr_thread_register(void)
{
	int ret;

	printf("\nTest rte_rcu_qsbr_thread_register()\n");

	ret = rte_rcu_qsbr_thread_register(NULL, enabled_core_ids[0]);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "NULL variable check");

	ret = rte_rcu_qsbr_thread_register(NULL, 100000);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0),
		"NULL variable, invalid thread id");

	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);

	/* Register valid thread id */
	ret = rte_rcu_qsbr_thread_register(t[0], enabled_core_ids[0]);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 1), "Valid thread id");

	/* Re-registering should not return error */
	ret = rte_rcu_qsbr_thread_register(t[0], enabled_core_ids[0]);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 1),
		"Already registered thread id");

	/* Register valid thread id - max allowed thread id */
	ret = rte_rcu_qsbr_thread_register(t[0], RTE_MAX_LCORE - 1);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 1), "Max thread id");

	ret = rte_rcu_qsbr_thread_register(t[0], 100000);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0),
		"NULL variable, invalid thread id");

	return 0;
}

/*
 * rte_rcu_qsbr_thread_unregister: Remove a reader thread, from the list of
 * threads reporting their quiescent state on a QS variable.
 */
static int
test_rcu_qsbr_thread_unregister(void)
{
	unsigned int num_threads[3] = {1, RTE_MAX_LCORE, 1};
	unsigned int i, j;
	unsigned int skip_thread_id;
	uint64_t token;
	int ret;

	printf("\nTest rte_rcu_qsbr_thread_unregister()\n");

	ret = rte_rcu_qsbr_thread_unregister(NULL, enabled_core_ids[0]);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "NULL variable check");

	ret = rte_rcu_qsbr_thread_unregister(NULL, 100000);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0),
		"NULL variable, invalid thread id");

	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);

	rte_rcu_qsbr_thread_register(t[0], enabled_core_ids[0]);

	ret = rte_rcu_qsbr_thread_unregister(t[0], 100000);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0),
		"NULL variable, invalid thread id");

	/* Find first disabled core */
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (enabled_core_ids[i] == 0)
			break;
	}
	/* Test with disabled lcore */
	ret = rte_rcu_qsbr_thread_unregister(t[0], i);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 1),
		"disabled thread id");
	/* Unregister already unregistered core */
	ret = rte_rcu_qsbr_thread_unregister(t[0], i);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 1),
		"Already unregistered core");

	/* Test with enabled lcore */
	ret = rte_rcu_qsbr_thread_unregister(t[0], enabled_core_ids[0]);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 1),
		"enabled thread id");
	/* Unregister already unregistered core */
	ret = rte_rcu_qsbr_thread_unregister(t[0], enabled_core_ids[0]);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 1),
		"Already unregistered core");

	/*
	 * Test with different thread_ids:
	 * 1 - thread_id = 0
	 * 2 - All possible thread_ids, from 0 to RTE_MAX_LCORE
	 * 3 - thread_id = RTE_MAX_LCORE - 1
	 */
	for (j = 0; j < 3; j++) {
		rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);

		for (i = 0; i < num_threads[j]; i++)
			rte_rcu_qsbr_thread_register(t[0],
				(j == 2) ? (RTE_MAX_LCORE - 1) : i);

		token = rte_rcu_qsbr_start(t[0]);
		TEST_RCU_QSBR_RETURN_IF_ERROR(
			(token != (TEST_RCU_QSBR_CNT_INIT + 1)), "QSBR Start");
		skip_thread_id = rte_rand() % RTE_MAX_LCORE;
		/* Update quiescent state counter */
		for (i = 0; i < num_threads[j]; i++) {
			/* Skip one update */
			if ((j == 1) && (i == skip_thread_id))
				continue;
			rte_rcu_qsbr_quiescent(t[0],
				(j == 2) ? (RTE_MAX_LCORE - 1) : i);
		}

		if (j == 1) {
			/* Validate the updates */
			ret = rte_rcu_qsbr_check(t[0], token, false);
			TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0),
						"Non-blocking QSBR check");
			/* Update the previously skipped thread */
			rte_rcu_qsbr_quiescent(t[0], skip_thread_id);
		}

		/* Validate the updates */
		ret = rte_rcu_qsbr_check(t[0], token, false);
		TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0),
						"Non-blocking QSBR check");

		for (i = 0; i < num_threads[j]; i++)
			rte_rcu_qsbr_thread_unregister(t[0],
				(j == 2) ? (RTE_MAX_LCORE - 1) : i);

		/* Check with no thread registered */
		ret = rte_rcu_qsbr_check(t[0], token, true);
		TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0),
						"Blocking QSBR check");
	}
	return 0;
}

/*
 * rte_rcu_qsbr_start: Ask the worker threads to report the quiescent state
 * status.
 */
static int
test_rcu_qsbr_start(void)
{
	uint64_t token;
	unsigned int i;

	printf("\nTest rte_rcu_qsbr_start()\n");

	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);

	for (i = 0; i < num_cores; i++)
		rte_rcu_qsbr_thread_register(t[0], enabled_core_ids[i]);

	token = rte_rcu_qsbr_start(t[0]);
	TEST_RCU_QSBR_RETURN_IF_ERROR(
		(token != (TEST_RCU_QSBR_CNT_INIT + 1)), "QSBR Start");
	return 0;
}

static int
test_rcu_qsbr_check_reader(void *arg)
{
	struct rte_rcu_qsbr *temp;
	uint8_t read_type = (uint8_t)((uintptr_t)arg);
	unsigned int i;

	temp = t[read_type];

	/* Update quiescent state counter */
	for (i = 0; i < num_cores; i++) {
		if (i % 2 == 0)
			rte_rcu_qsbr_quiescent(temp, enabled_core_ids[i]);
		else
			rte_rcu_qsbr_thread_unregister(temp,
							enabled_core_ids[i]);
	}
	return 0;
}

/*
 * rte_rcu_qsbr_check: Checks if all the worker threads have entered the queis-
 * cent state 'n' number of times. 'n' is provided in rte_rcu_qsbr_start API.
 */
static int
test_rcu_qsbr_check(void)
{
	int ret;
	unsigned int i;
	uint64_t token;

	printf("\nTest rte_rcu_qsbr_check()\n");

	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);

	token = rte_rcu_qsbr_start(t[0]);
	TEST_RCU_QSBR_RETURN_IF_ERROR(
		(token != (TEST_RCU_QSBR_CNT_INIT + 1)), "QSBR Start");


	ret = rte_rcu_qsbr_check(t[0], 0, false);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "Token = 0");

	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "Blocking QSBR check");

	for (i = 0; i < num_cores; i++)
		rte_rcu_qsbr_thread_register(t[0], enabled_core_ids[i]);

	ret = rte_rcu_qsbr_check(t[0], token, false);
	/* Threads are offline, hence this should pass */
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "Non-blocking QSBR check");

	token = rte_rcu_qsbr_start(t[0]);
	TEST_RCU_QSBR_RETURN_IF_ERROR(
		(token != (TEST_RCU_QSBR_CNT_INIT + 2)), "QSBR Start");

	ret = rte_rcu_qsbr_check(t[0], token, false);
	/* Threads are offline, hence this should pass */
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "Non-blocking QSBR check");

	for (i = 0; i < num_cores; i++)
		rte_rcu_qsbr_thread_unregister(t[0], enabled_core_ids[i]);

	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "Blocking QSBR check");

	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);

	for (i = 0; i < num_cores; i++)
		rte_rcu_qsbr_thread_register(t[0], enabled_core_ids[i]);

	token = rte_rcu_qsbr_start(t[0]);
	TEST_RCU_QSBR_RETURN_IF_ERROR(
		(token != (TEST_RCU_QSBR_CNT_INIT + 1)), "QSBR Start");

	rte_eal_remote_launch(test_rcu_qsbr_check_reader, NULL,
							enabled_core_ids[0]);

	rte_eal_mp_wait_lcore();
	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret != 1), "Blocking QSBR check");

	return 0;
}

static int
test_rcu_qsbr_synchronize_reader(void *arg)
{
	uint32_t lcore_id = rte_lcore_id();
	(void)arg;

	/* Register and become online */
	rte_rcu_qsbr_thread_register(t[0], lcore_id);
	rte_rcu_qsbr_thread_online(t[0], lcore_id);

	while (!writer_done)
		rte_rcu_qsbr_quiescent(t[0], lcore_id);

	rte_rcu_qsbr_thread_offline(t[0], lcore_id);
	rte_rcu_qsbr_thread_unregister(t[0], lcore_id);

	return 0;
}

/*
 * rte_rcu_qsbr_synchronize: Wait till all the reader threads have entered
 * the quiescent state.
 */
static int
test_rcu_qsbr_synchronize(void)
{
	unsigned int i;

	printf("\nTest rte_rcu_qsbr_synchronize()\n");

	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);

	/* Test if the API returns when there are no threads reporting
	 * QS on the variable.
	 */
	rte_rcu_qsbr_synchronize(t[0], RTE_QSBR_THRID_INVALID);

	/* Test if the API returns when there are threads registered
	 * but not online.
	 */
	for (i = 0; i < RTE_MAX_LCORE; i++)
		rte_rcu_qsbr_thread_register(t[0], i);
	rte_rcu_qsbr_synchronize(t[0], RTE_QSBR_THRID_INVALID);

	/* Test if the API returns when the caller is also
	 * reporting the QS status.
	 */
	rte_rcu_qsbr_thread_online(t[0], 0);
	rte_rcu_qsbr_synchronize(t[0], 0);
	rte_rcu_qsbr_thread_offline(t[0], 0);

	/* Check the other boundary */
	rte_rcu_qsbr_thread_online(t[0], RTE_MAX_LCORE - 1);
	rte_rcu_qsbr_synchronize(t[0], RTE_MAX_LCORE - 1);
	rte_rcu_qsbr_thread_offline(t[0], RTE_MAX_LCORE - 1);

	/* Test if the API returns after unregistering all the threads */
	for (i = 0; i < RTE_MAX_LCORE; i++)
		rte_rcu_qsbr_thread_unregister(t[0], i);
	rte_rcu_qsbr_synchronize(t[0], RTE_QSBR_THRID_INVALID);

	/* Test if the API returns with the live threads */
	writer_done = 0;
	for (i = 0; i < num_cores; i++)
		rte_eal_remote_launch(test_rcu_qsbr_synchronize_reader,
			NULL, enabled_core_ids[i]);
	rte_rcu_qsbr_synchronize(t[0], RTE_QSBR_THRID_INVALID);
	rte_rcu_qsbr_synchronize(t[0], RTE_QSBR_THRID_INVALID);
	rte_rcu_qsbr_synchronize(t[0], RTE_QSBR_THRID_INVALID);
	rte_rcu_qsbr_synchronize(t[0], RTE_QSBR_THRID_INVALID);
	rte_rcu_qsbr_synchronize(t[0], RTE_QSBR_THRID_INVALID);

	writer_done = 1;
	rte_eal_mp_wait_lcore();

	return 0;
}

/*
 * rte_rcu_qsbr_thread_online: Add a registered reader thread, to
 * the list of threads reporting their quiescent state on a QS variable.
 */
static int
test_rcu_qsbr_thread_online(void)
{
	int i, ret;
	uint64_t token;

	printf("Test rte_rcu_qsbr_thread_online()\n");

	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);

	/* Register 2 threads to validate that only the
	 * online thread is waited upon.
	 */
	rte_rcu_qsbr_thread_register(t[0], enabled_core_ids[0]);
	rte_rcu_qsbr_thread_register(t[0], enabled_core_ids[1]);

	/* Use qsbr_start to verify that the thread_online API
	 * succeeded.
	 */
	token = rte_rcu_qsbr_start(t[0]);

	/* Make the thread online */
	rte_rcu_qsbr_thread_online(t[0], enabled_core_ids[0]);

	/* Check if the thread is online */
	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "thread online");

	/* Check if the online thread, can report QS */
	token = rte_rcu_qsbr_start(t[0]);
	rte_rcu_qsbr_quiescent(t[0], enabled_core_ids[0]);
	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "thread update");

	/* Make all the threads online */
	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);
	token = rte_rcu_qsbr_start(t[0]);
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		rte_rcu_qsbr_thread_register(t[0], i);
		rte_rcu_qsbr_thread_online(t[0], i);
	}
	/* Check if all the threads are online */
	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "thread online");
	/* Check if all the online threads can report QS */
	token = rte_rcu_qsbr_start(t[0]);
	for (i = 0; i < RTE_MAX_LCORE; i++)
		rte_rcu_qsbr_quiescent(t[0], i);
	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "thread update");

	return 0;
}

/*
 * rte_rcu_qsbr_thread_offline: Remove a registered reader thread, from
 * the list of threads reporting their quiescent state on a QS variable.
 */
static int
test_rcu_qsbr_thread_offline(void)
{
	int i, ret;
	uint64_t token;

	printf("\nTest rte_rcu_qsbr_thread_offline()\n");

	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);

	rte_rcu_qsbr_thread_register(t[0], enabled_core_ids[0]);

	/* Make the thread offline */
	rte_rcu_qsbr_thread_offline(t[0], enabled_core_ids[0]);

	/* Use qsbr_start to verify that the thread_offline API
	 * succeeded.
	 */
	token = rte_rcu_qsbr_start(t[0]);
	/* Check if the thread is offline */
	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "thread offline");

	/* Bring an offline thread online and check if it can
	 * report QS.
	 */
	rte_rcu_qsbr_thread_online(t[0], enabled_core_ids[0]);
	/* Check if the online thread, can report QS */
	token = rte_rcu_qsbr_start(t[0]);
	rte_rcu_qsbr_quiescent(t[0], enabled_core_ids[0]);
	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "offline to online");

	/*
	 * Check a sequence of online/status/offline/status/online/status
	 */
	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);
	token = rte_rcu_qsbr_start(t[0]);
	/* Make the threads online */
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		rte_rcu_qsbr_thread_register(t[0], i);
		rte_rcu_qsbr_thread_online(t[0], i);
	}

	/* Check if all the threads are online */
	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "thread online");

	/* Check if all the online threads can report QS */
	token = rte_rcu_qsbr_start(t[0]);
	for (i = 0; i < RTE_MAX_LCORE; i++)
		rte_rcu_qsbr_quiescent(t[0], i);
	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "report QS");

	/* Make all the threads offline */
	for (i = 0; i < RTE_MAX_LCORE; i++)
		rte_rcu_qsbr_thread_offline(t[0], i);
	/* Make sure these threads are not being waited on */
	token = rte_rcu_qsbr_start(t[0]);
	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "offline QS");

	/* Make the threads online */
	for (i = 0; i < RTE_MAX_LCORE; i++)
		rte_rcu_qsbr_thread_online(t[0], i);
	/* Check if all the online threads can report QS */
	token = rte_rcu_qsbr_start(t[0]);
	for (i = 0; i < RTE_MAX_LCORE; i++)
		rte_rcu_qsbr_quiescent(t[0], i);
	ret = rte_rcu_qsbr_check(t[0], token, true);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "online again");

	return 0;
}

static void
test_rcu_qsbr_free_resource1(void *p, void *e, unsigned int n)
{
	if (p != NULL || e != NULL || n != 1) {
		printf("%s: Test failed\n", __func__);
		cb_failed = 1;
	}
}

static void
test_rcu_qsbr_free_resource2(void *p, void *e, unsigned int n)
{
	if (p != NULL || e == NULL || n != 1) {
		printf("%s: Test failed\n", __func__);
		cb_failed = 1;
	}
}

/*
 * rte_rcu_qsbr_dq_create: create a queue used to store the data structure
 * elements that can be freed later. This queue is referred to as 'defer queue'.
 */
static int
test_rcu_qsbr_dq_create(void)
{
	char rcu_dq_name[RTE_RCU_QSBR_DQ_NAMESIZE];
	struct rte_rcu_qsbr_dq_parameters params;
	struct rte_rcu_qsbr_dq *dq;

	printf("\nTest rte_rcu_qsbr_dq_create()\n");

	/* Pass invalid parameters */
	dq = rte_rcu_qsbr_dq_create(NULL);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq != NULL), "dq create invalid params");

	memset(&params, 0, sizeof(struct rte_rcu_qsbr_dq_parameters));
	dq = rte_rcu_qsbr_dq_create(&params);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq != NULL), "dq create invalid params");

	snprintf(rcu_dq_name, sizeof(rcu_dq_name), "TEST_RCU");
	params.name = rcu_dq_name;
	dq = rte_rcu_qsbr_dq_create(&params);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq != NULL), "dq create invalid params");

	params.free_fn = test_rcu_qsbr_free_resource1;
	dq = rte_rcu_qsbr_dq_create(&params);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq != NULL), "dq create invalid params");

	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);
	params.v = t[0];
	dq = rte_rcu_qsbr_dq_create(&params);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq != NULL), "dq create invalid params");

	params.size = 1;
	dq = rte_rcu_qsbr_dq_create(&params);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq != NULL), "dq create invalid params");

	params.esize = 3;
	dq = rte_rcu_qsbr_dq_create(&params);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq != NULL), "dq create invalid params");

	params.trigger_reclaim_limit = 0;
	params.max_reclaim_size = 0;
	dq = rte_rcu_qsbr_dq_create(&params);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq != NULL), "dq create invalid params");

	/* Pass all valid parameters */
	params.esize = 16;
	params.trigger_reclaim_limit = 0;
	params.max_reclaim_size = params.size;
	dq = rte_rcu_qsbr_dq_create(&params);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq == NULL), "dq create valid params");
	rte_rcu_qsbr_dq_delete(dq);

	params.esize = 16;
	params.flags = RTE_RCU_QSBR_DQ_MT_UNSAFE;
	dq = rte_rcu_qsbr_dq_create(&params);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq == NULL), "dq create valid params");
	rte_rcu_qsbr_dq_delete(dq);

	return 0;
}

/*
 * rte_rcu_qsbr_dq_enqueue: enqueue one resource to the defer queue,
 * to be freed later after at least one grace period is over.
 */
static int
test_rcu_qsbr_dq_enqueue(void)
{
	int ret;
	uint64_t r;
	char rcu_dq_name[RTE_RCU_QSBR_DQ_NAMESIZE];
	struct rte_rcu_qsbr_dq_parameters params;
	struct rte_rcu_qsbr_dq *dq;

	printf("\nTest rte_rcu_qsbr_dq_enqueue()\n");

	/* Create a queue with simple parameters */
	memset(&params, 0, sizeof(struct rte_rcu_qsbr_dq_parameters));
	snprintf(rcu_dq_name, sizeof(rcu_dq_name), "TEST_RCU");
	params.name = rcu_dq_name;
	params.free_fn = test_rcu_qsbr_free_resource1;
	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);
	params.v = t[0];
	params.size = 1;
	params.esize = 16;
	params.trigger_reclaim_limit = 0;
	params.max_reclaim_size = params.size;
	dq = rte_rcu_qsbr_dq_create(&params);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq == NULL), "dq create valid params");

	/* Pass invalid parameters */
	ret = rte_rcu_qsbr_dq_enqueue(NULL, NULL);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "dq enqueue invalid params");

	ret = rte_rcu_qsbr_dq_enqueue(dq, NULL);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "dq enqueue invalid params");

	ret = rte_rcu_qsbr_dq_enqueue(NULL, &r);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "dq enqueue invalid params");

	ret = rte_rcu_qsbr_dq_delete(dq);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 1), "dq delete valid params");

	return 0;
}

/*
 * rte_rcu_qsbr_dq_reclaim: Reclaim resources from the defer queue.
 */
static int
test_rcu_qsbr_dq_reclaim(void)
{
	int ret;
	char rcu_dq_name[RTE_RCU_QSBR_DQ_NAMESIZE];
	struct rte_rcu_qsbr_dq_parameters params;
	struct rte_rcu_qsbr_dq *dq;

	printf("\nTest rte_rcu_qsbr_dq_reclaim()\n");

	/* Pass invalid parameters */
	ret = rte_rcu_qsbr_dq_reclaim(NULL, 10, NULL, NULL, NULL);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret != 1), "dq reclaim invalid params");

	/* Pass invalid parameters */
	memset(&params, 0, sizeof(struct rte_rcu_qsbr_dq_parameters));
	snprintf(rcu_dq_name, sizeof(rcu_dq_name), "TEST_RCU");
	params.name = rcu_dq_name;
	params.free_fn = test_rcu_qsbr_free_resource1;
	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);
	params.v = t[0];
	params.size = 1;
	params.esize = 3;
	params.trigger_reclaim_limit = 0;
	params.max_reclaim_size = params.size;
	dq = rte_rcu_qsbr_dq_create(&params);
	ret = rte_rcu_qsbr_dq_reclaim(dq, 0, NULL, NULL, NULL);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret != 1), "dq reclaim invalid params");

	return 0;
}

/*
 * rte_rcu_qsbr_dq_delete: Delete a defer queue.
 */
static int
test_rcu_qsbr_dq_delete(void)
{
	int ret;
	char rcu_dq_name[RTE_RCU_QSBR_DQ_NAMESIZE];
	struct rte_rcu_qsbr_dq_parameters params;
	struct rte_rcu_qsbr_dq *dq;

	printf("\nTest rte_rcu_qsbr_dq_delete()\n");

	/* Pass invalid parameters */
	ret = rte_rcu_qsbr_dq_delete(NULL);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret != 0), "dq delete invalid params");

	memset(&params, 0, sizeof(struct rte_rcu_qsbr_dq_parameters));
	snprintf(rcu_dq_name, sizeof(rcu_dq_name), "TEST_RCU");
	params.name = rcu_dq_name;
	params.free_fn = test_rcu_qsbr_free_resource1;
	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);
	params.v = t[0];
	params.size = 1;
	params.esize = 16;
	params.trigger_reclaim_limit = 0;
	params.max_reclaim_size = params.size;
	dq = rte_rcu_qsbr_dq_create(&params);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq == NULL), "dq create valid params");
	ret = rte_rcu_qsbr_dq_delete(dq);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret != 0), "dq delete valid params");

	return 0;
}

/*
 * rte_rcu_qsbr_dq_enqueue: enqueue one resource to the defer queue,
 * to be freed later after at least one grace period is over.
 */
static int
test_rcu_qsbr_dq_functional(int32_t size, int32_t esize, uint32_t flags)
{
	int i, j, ret;
	char rcu_dq_name[RTE_RCU_QSBR_DQ_NAMESIZE];
	struct rte_rcu_qsbr_dq_parameters params;
	struct rte_rcu_qsbr_dq *dq;
	uint64_t *e;
	uint64_t sc = 200;
	int max_entries;

	printf("\nTest rte_rcu_qsbr_dq_xxx functional tests()\n");
	printf("Size = %d, esize = %d, flags = 0x%x\n", size, esize, flags);

	e = (uint64_t *)rte_zmalloc(NULL, esize, RTE_CACHE_LINE_SIZE);
	if (e == NULL)
		return 0;
	cb_failed = 0;

	/* Initialize the RCU variable. No threads are registered */
	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);

	/* Create a queue with simple parameters */
	memset(&params, 0, sizeof(struct rte_rcu_qsbr_dq_parameters));
	snprintf(rcu_dq_name, sizeof(rcu_dq_name), "TEST_RCU");
	params.name = rcu_dq_name;
	params.flags = flags;
	params.free_fn = test_rcu_qsbr_free_resource2;
	params.v = t[0];
	params.size = size;
	params.esize = esize;
	params.trigger_reclaim_limit = size >> 3;
	params.max_reclaim_size = (size >> 4)?(size >> 4):1;
	dq = rte_rcu_qsbr_dq_create(&params);
	TEST_RCU_QSBR_RETURN_IF_ERROR((dq == NULL), "dq create valid params");

	/* Given the size calculate the maximum number of entries
	 * that can be stored on the defer queue (look at the logic used
	 * in capacity calculation of rte_ring).
	 */
	max_entries = rte_align32pow2(size + 1) - 1;
	printf("max_entries = %d\n", max_entries);

	/* Enqueue few counters starting with the value 'sc' */
	/* The queue size will be rounded up to 2. The enqueue API also
	 * reclaims if the queue size is above certain limit. Since, there
	 * are no threads registered, reclamation succeeds. Hence, it should
	 * be possible to enqueue more than the provided queue size.
	 */
	for (i = 0; i < 10; i++) {
		ret = rte_rcu_qsbr_dq_enqueue(dq, e);
		TEST_RCU_QSBR_GOTO_IF_ERROR(end, (ret != 0),
			"dq enqueue functional, i = %d", i);
		for (j = 0; j < esize/8; j++)
			e[j] = sc++;
	}

	/* Validate that call back function did not return any error */
	TEST_RCU_QSBR_GOTO_IF_ERROR(end, (cb_failed == 1), "CB failed");

	/* Register a thread on the RCU QSBR variable. Reclamation will not
	 * succeed. It should not be possible to enqueue more than the size
	 * number of resources.
	 */
	rte_rcu_qsbr_thread_register(t[0], 1);
	rte_rcu_qsbr_thread_online(t[0], 1);

	for (i = 0; i < max_entries; i++) {
		ret = rte_rcu_qsbr_dq_enqueue(dq, e);
		TEST_RCU_QSBR_GOTO_IF_ERROR(end, (ret != 0),
			"dq enqueue functional, max_entries = %d, i = %d",
			max_entries, i);
		for (j = 0; j < esize/8; j++)
			e[j] = sc++;
	}

	/* Enqueue fails as queue is full */
	ret = rte_rcu_qsbr_dq_enqueue(dq, e);
	TEST_RCU_QSBR_GOTO_IF_ERROR(end, (ret == 0), "defer queue is not full");

	/* Delete should fail as there are elements in defer queue which
	 * cannot be reclaimed.
	 */
	ret = rte_rcu_qsbr_dq_delete(dq);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret == 0), "dq delete valid params");

	/* Report quiescent state, enqueue should succeed */
	rte_rcu_qsbr_quiescent(t[0], 1);
	for (i = 0; i < max_entries; i++) {
		ret = rte_rcu_qsbr_dq_enqueue(dq, e);
		TEST_RCU_QSBR_GOTO_IF_ERROR(end, (ret != 0),
			"dq enqueue functional");
		for (j = 0; j < esize/8; j++)
			e[j] = sc++;
	}

	/* Validate that call back function did not return any error */
	TEST_RCU_QSBR_GOTO_IF_ERROR(end, (cb_failed == 1), "CB failed");

	/* Queue is full */
	ret = rte_rcu_qsbr_dq_enqueue(dq, e);
	TEST_RCU_QSBR_GOTO_IF_ERROR(end, (ret == 0), "defer queue is not full");

	/* Report quiescent state, delete should succeed */
	rte_rcu_qsbr_quiescent(t[0], 1);
	ret = rte_rcu_qsbr_dq_delete(dq);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret != 0), "dq delete valid params");

	rte_free(e);

	/* Validate that call back function did not return any error */
	TEST_RCU_QSBR_RETURN_IF_ERROR((cb_failed == 1), "CB failed");

	return 0;

end:
	rte_free(e);
	ret = rte_rcu_qsbr_dq_delete(dq);
	TEST_RCU_QSBR_RETURN_IF_ERROR((ret != 0), "dq delete valid params");
	return -1;
}

/*
 * rte_rcu_qsbr_dump: Dump status of a single QS variable to a file
 */
static int
test_rcu_qsbr_dump(void)
{
	unsigned int i;

	printf("\nTest rte_rcu_qsbr_dump()\n");

	/* Negative tests */
	rte_rcu_qsbr_dump(NULL, t[0]);
	rte_rcu_qsbr_dump(stdout, NULL);
	rte_rcu_qsbr_dump(NULL, NULL);

	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);
	rte_rcu_qsbr_init(t[1], RTE_MAX_LCORE);

	/* QS variable with 0 core mask */
	rte_rcu_qsbr_dump(stdout, t[0]);

	rte_rcu_qsbr_thread_register(t[0], enabled_core_ids[0]);

	for (i = 1; i < num_cores; i++)
		rte_rcu_qsbr_thread_register(t[1], enabled_core_ids[i]);

	rte_rcu_qsbr_dump(stdout, t[0]);
	rte_rcu_qsbr_dump(stdout, t[1]);
	printf("\n");
	return 0;
}

static int
test_rcu_qsbr_reader(void *arg)
{
	struct rte_rcu_qsbr *temp;
	struct rte_hash *hash = NULL;
	int i;
	uint32_t lcore_id = rte_lcore_id();
	struct test_rcu_thread_info *ti;
	uint32_t *pdata;

	ti = (struct test_rcu_thread_info *)arg;
	temp = t[ti->ir];
	hash = h[ti->ih];

	do {
		rte_rcu_qsbr_thread_register(temp, lcore_id);
		rte_rcu_qsbr_thread_online(temp, lcore_id);
		for (i = 0; i < TOTAL_ENTRY; i++) {
			rte_rcu_qsbr_lock(temp, lcore_id);
			if (rte_hash_lookup_data(hash, keys+i,
					(void **)&pdata) != -ENOENT) {
				pdata[lcore_id] = 0;
				while (pdata[lcore_id] < COUNTER_VALUE)
					pdata[lcore_id]++;
			}
			rte_rcu_qsbr_unlock(temp, lcore_id);
		}
		/* Update quiescent state counter */
		rte_rcu_qsbr_quiescent(temp, lcore_id);
		rte_rcu_qsbr_thread_offline(temp, lcore_id);
		rte_rcu_qsbr_thread_unregister(temp, lcore_id);
	} while (!writer_done);

	return 0;
}

static int
test_rcu_qsbr_writer(void *arg)
{
	uint64_t token;
	int32_t i, pos, del;
	uint32_t c;
	struct rte_rcu_qsbr *temp;
	struct rte_hash *hash = NULL;
	struct test_rcu_thread_info *ti;

	ti = (struct test_rcu_thread_info *)arg;
	temp = t[ti->ir];
	hash = h[ti->ih];

	/* Delete element from the shared data structure */
	del = rte_lcore_id() % TOTAL_ENTRY;
	pos = rte_hash_del_key(hash, keys + del);
	if (pos < 0) {
		printf("Delete key failed #%d\n", keys[del]);
		return -1;
	}
	/* Start the quiescent state query process */
	token = rte_rcu_qsbr_start(temp);
	/* Check the quiescent state status */
	rte_rcu_qsbr_check(temp, token, true);
	for (i = 0; i < 2; i++) {
		c = hash_data[ti->ih][del][ti->r_core_ids[i]];
		if (c != COUNTER_VALUE && c != 0) {
			printf("Reader lcore id %u did not complete = %u\t",
				rte_lcore_id(), c);
			return -1;
		}
	}

	if (rte_hash_free_key_with_position(hash, pos) < 0) {
		printf("Failed to free the key #%d\n", keys[del]);
		return -1;
	}
	rte_free(hash_data[ti->ih][del]);
	hash_data[ti->ih][del] = NULL;

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
		hash_data[hash_id][i] =
			rte_zmalloc(NULL, sizeof(uint32_t) * RTE_MAX_LCORE, 0);
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
 * Single writer, Single QS variable, simultaneous QSBR Queries
 */
static int
test_rcu_qsbr_sw_sv_3qs(void)
{
	uint64_t token[3];
	uint32_t c;
	int i, num_readers;
	int32_t pos[3];

	writer_done = 0;

	printf("Test: 1 writer, 1 QSBR variable, simultaneous QSBR queries\n");

	rte_rcu_qsbr_init(t[0], RTE_MAX_LCORE);

	/* Shared data structure created */
	h[0] = init_hash(0);
	if (h[0] == NULL) {
		printf("Hash init failed\n");
		goto error;
	}

	/* No need to fill the registered core IDs as the writer
	 * thread is not launched.
	 */
	thread_info[0].ir = 0;
	thread_info[0].ih = 0;

	/* Reader threads are launched */
	/* Keep the number of reader threads low to reduce
	 * the execution time.
	 */
	num_readers = num_cores < 4 ? num_cores : 4;
	for (i = 0; i < num_readers; i++)
		rte_eal_remote_launch(test_rcu_qsbr_reader, &thread_info[0],
					enabled_core_ids[i]);

	/* Delete element from the shared data structure */
	pos[0] = rte_hash_del_key(h[0], keys + 0);
	if (pos[0] < 0) {
		printf("Delete key failed #%d\n", keys[0]);
		goto error;
	}
	/* Start the quiescent state query process */
	token[0] = rte_rcu_qsbr_start(t[0]);

	/* Delete element from the shared data structure */
	pos[1] = rte_hash_del_key(h[0], keys + 3);
	if (pos[1] < 0) {
		printf("Delete key failed #%d\n", keys[3]);
		goto error;
	}
	/* Start the quiescent state query process */
	token[1] = rte_rcu_qsbr_start(t[0]);

	/* Delete element from the shared data structure */
	pos[2] = rte_hash_del_key(h[0], keys + 6);
	if (pos[2] < 0) {
		printf("Delete key failed #%d\n", keys[6]);
		goto error;
	}
	/* Start the quiescent state query process */
	token[2] = rte_rcu_qsbr_start(t[0]);

	/* Check the quiescent state status */
	rte_rcu_qsbr_check(t[0], token[0], true);
	for (i = 0; i < num_readers; i++) {
		c = hash_data[0][0][enabled_core_ids[i]];
		if (c != COUNTER_VALUE && c != 0) {
			printf("Reader lcore %d did not complete #0 = %d\n",
				enabled_core_ids[i], c);
			goto error;
		}
	}

	if (rte_hash_free_key_with_position(h[0], pos[0]) < 0) {
		printf("Failed to free the key #%d\n", keys[0]);
		goto error;
	}
	rte_free(hash_data[0][0]);
	hash_data[0][0] = NULL;

	/* Check the quiescent state status */
	rte_rcu_qsbr_check(t[0], token[1], true);
	for (i = 0; i < num_readers; i++) {
		c = hash_data[0][3][enabled_core_ids[i]];
		if (c != COUNTER_VALUE && c != 0) {
			printf("Reader lcore %d did not complete #3 = %d\n",
				enabled_core_ids[i], c);
			goto error;
		}
	}

	if (rte_hash_free_key_with_position(h[0], pos[1]) < 0) {
		printf("Failed to free the key #%d\n", keys[3]);
		goto error;
	}
	rte_free(hash_data[0][3]);
	hash_data[0][3] = NULL;

	/* Check the quiescent state status */
	rte_rcu_qsbr_check(t[0], token[2], true);
	for (i = 0; i < num_readers; i++) {
		c = hash_data[0][6][enabled_core_ids[i]];
		if (c != COUNTER_VALUE && c != 0) {
			printf("Reader lcore %d did not complete #6 = %d\n",
				enabled_core_ids[i], c);
			goto error;
		}
	}

	if (rte_hash_free_key_with_position(h[0], pos[2]) < 0) {
		printf("Failed to free the key #%d\n", keys[6]);
		goto error;
	}
	rte_free(hash_data[0][6]);
	hash_data[0][6] = NULL;

	writer_done = 1;

	/* Wait and check return value from reader threads */
	for (i = 0; i < num_readers; i++)
		if (rte_eal_wait_lcore(enabled_core_ids[i]) < 0)
			goto error;
	rte_hash_free(h[0]);
	rte_free(keys);

	return 0;

error:
	writer_done = 1;
	/* Wait until all readers have exited */
	rte_eal_mp_wait_lcore();

	rte_hash_free(h[0]);
	rte_free(keys);
	for (i = 0; i < TOTAL_ENTRY; i++)
		rte_free(hash_data[0][i]);

	return -1;
}

/*
 * Multi writer, Multiple QS variable, simultaneous QSBR queries
 */
static int
test_rcu_qsbr_mw_mv_mqs(void)
{
	unsigned int i, j;
	unsigned int test_cores;

	if (RTE_MAX_LCORE < 5 || num_cores < 4) {
		printf("Not enough cores for %s, expecting at least 5\n",
			__func__);
		return TEST_SKIPPED;
	}

	writer_done = 0;
	test_cores = num_cores / 4;
	test_cores = test_cores * 4;

	printf("Test: %d writers, %d QSBR variable, simultaneous QSBR queries\n",
	       test_cores / 2, test_cores / 4);

	for (i = 0; i < test_cores / 4; i++) {
		j = i * 4;
		rte_rcu_qsbr_init(t[i], RTE_MAX_LCORE);
		h[i] = init_hash(i);
		if (h[i] == NULL) {
			printf("Hash init failed\n");
			goto error;
		}
		thread_info[i].ir = i;
		thread_info[i].ih = i;
		thread_info[i].r_core_ids[0] = enabled_core_ids[j];
		thread_info[i].r_core_ids[1] = enabled_core_ids[j + 1];

		/* Reader threads are launched */
		rte_eal_remote_launch(test_rcu_qsbr_reader,
					(void *)&thread_info[i],
					enabled_core_ids[j]);
		rte_eal_remote_launch(test_rcu_qsbr_reader,
					(void *)&thread_info[i],
					enabled_core_ids[j + 1]);

		/* Writer threads are launched */
		rte_eal_remote_launch(test_rcu_qsbr_writer,
					(void *)&thread_info[i],
					enabled_core_ids[j + 2]);
		rte_eal_remote_launch(test_rcu_qsbr_writer,
					(void *)&thread_info[i],
					enabled_core_ids[j + 3]);
	}

	/* Wait and check return value from writer threads */
	for (i = 0; i < test_cores / 4; i++) {
		j = i * 4;
		if (rte_eal_wait_lcore(enabled_core_ids[j + 2]) < 0)
			goto error;

		if (rte_eal_wait_lcore(enabled_core_ids[j + 3]) < 0)
			goto error;
	}
	writer_done = 1;

	/* Wait and check return value from reader threads */
	for (i = 0; i < test_cores / 4; i++) {
		j = i * 4;
		if (rte_eal_wait_lcore(enabled_core_ids[j]) < 0)
			goto error;

		if (rte_eal_wait_lcore(enabled_core_ids[j + 1]) < 0)
			goto error;
	}

	for (i = 0; i < test_cores / 4; i++)
		rte_hash_free(h[i]);

	rte_free(keys);

	return 0;

error:
	writer_done = 1;
	/* Wait until all readers and writers have exited */
	rte_eal_mp_wait_lcore();

	for (i = 0; i < test_cores / 4; i++)
		rte_hash_free(h[i]);
	rte_free(keys);
	for (j = 0; j < test_cores / 4; j++)
		for (i = 0; i < TOTAL_ENTRY; i++)
			rte_free(hash_data[j][i]);

	return -1;
}

static int
test_rcu_qsbr_main(void)
{
	uint16_t core_id;

	num_cores = 0;
	RTE_LCORE_FOREACH_WORKER(core_id) {
		enabled_core_ids[num_cores] = core_id;
		num_cores++;
	}

	/* Error-checking test cases */
	if (test_rcu_qsbr_get_memsize() < 0)
		goto test_fail;

	if (test_rcu_qsbr_init() < 0)
		goto test_fail;

	alloc_rcu();

	if (test_rcu_qsbr_thread_register() < 0)
		goto test_fail;

	if (test_rcu_qsbr_thread_unregister() < 0)
		goto test_fail;

	if (test_rcu_qsbr_start() < 0)
		goto test_fail;

	if (test_rcu_qsbr_check() < 0)
		goto test_fail;

	if (test_rcu_qsbr_synchronize() < 0)
		goto test_fail;

	if (test_rcu_qsbr_dump() < 0)
		goto test_fail;

	if (test_rcu_qsbr_thread_online() < 0)
		goto test_fail;

	if (test_rcu_qsbr_thread_offline() < 0)
		goto test_fail;

	if (test_rcu_qsbr_dq_create() < 0)
		goto test_fail;

	if (test_rcu_qsbr_dq_reclaim() < 0)
		goto test_fail;

	if (test_rcu_qsbr_dq_delete() < 0)
		goto test_fail;

	if (test_rcu_qsbr_dq_enqueue() < 0)
		goto test_fail;

	printf("\nFunctional tests\n");

	if (test_rcu_qsbr_sw_sv_3qs() < 0)
		goto test_fail;

	if (test_rcu_qsbr_mw_mv_mqs() < 0)
		goto test_fail;

	if (test_rcu_qsbr_dq_functional(1, 8, 0) < 0)
		goto test_fail;

	if (test_rcu_qsbr_dq_functional(2, 8, RTE_RCU_QSBR_DQ_MT_UNSAFE) < 0)
		goto test_fail;

	if (test_rcu_qsbr_dq_functional(303, 16, 0) < 0)
		goto test_fail;

	if (test_rcu_qsbr_dq_functional(7, 128, RTE_RCU_QSBR_DQ_MT_UNSAFE) < 0)
		goto test_fail;

	free_rcu();

	printf("\n");
	return 0;

test_fail:
	free_rcu();

	return -1;
}

REGISTER_FAST_TEST(rcu_qsbr_autotest, true, true, test_rcu_qsbr_main);
