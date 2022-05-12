/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2022 Microsoft Corporation
 */

#include <string.h>
#include <pthread.h>

#include <rte_thread.h>
#include <rte_debug.h>

#include "test.h"

RTE_LOG_REGISTER(threads_logtype_test, test.threads, INFO);

static uint32_t thread_id_ready;

static void *
thread_main(void *arg)
{
	*(rte_thread_t *)arg = rte_thread_self();
	__atomic_store_n(&thread_id_ready, 1, __ATOMIC_RELEASE);

	return NULL;
}

static int
test_thread_affinity(void)
{
	pthread_t id;
	rte_thread_t thread_id;
	rte_cpuset_t cpuset0;
	rte_cpuset_t cpuset1;

	RTE_TEST_ASSERT(pthread_create(&id, NULL, thread_main, &thread_id) == 0,
		"Failed to create thread");

	while (__atomic_load_n(&thread_id_ready, __ATOMIC_ACQUIRE) == 0)
		;

	RTE_TEST_ASSERT(rte_thread_get_affinity_by_id(thread_id, &cpuset0) == 0,
		"Failed to get thread affinity");
	RTE_TEST_ASSERT(rte_thread_get_affinity_by_id(thread_id, &cpuset1) == 0,
		"Failed to get thread affinity");
	RTE_TEST_ASSERT(memcmp(&cpuset0, &cpuset1, sizeof(rte_cpuset_t)) == 0,
		"Affinity should be stable");

	size_t i;
	for (i = 1; i < CPU_SETSIZE; i++)
		if (CPU_ISSET(i, &cpuset0)) {
			CPU_ZERO(&cpuset0);
			CPU_SET(i, &cpuset0);

			break;
		}
	RTE_TEST_ASSERT(rte_thread_set_affinity_by_id(thread_id, &cpuset0) == 0,
		"Failed to set thread affinity");
	RTE_TEST_ASSERT(rte_thread_get_affinity_by_id(thread_id, &cpuset1) == 0,
		"Failed to get thread affinity");
	RTE_TEST_ASSERT(memcmp(&cpuset0, &cpuset1, sizeof(rte_cpuset_t)) == 0,
		"Affinity should be stable");

	return 0;
}

static struct unit_test_suite threads_test_suite = {
	.suite_name = "threads autotest",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
		TEST_CASE(test_thread_affinity),
		TEST_CASES_END()
	}
};

static int
test_threads(void)
{
	return unit_test_suite_runner(&threads_test_suite);
}

REGISTER_TEST_COMMAND(threads_autotest, test_threads);
