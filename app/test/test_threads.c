/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2022 Microsoft Corporation
 */

#include <string.h>

#include <rte_thread.h>
#include <rte_debug.h>

#include "test.h"

RTE_LOG_REGISTER(threads_logtype_test, test.threads, INFO);

static uint32_t thread_id_ready;

static uint32_t
thread_main(void *arg)
{
	if (arg != NULL)
		*(rte_thread_t *)arg = rte_thread_self();

	__atomic_store_n(&thread_id_ready, 1, __ATOMIC_RELEASE);

	while (__atomic_load_n(&thread_id_ready, __ATOMIC_ACQUIRE) == 1)
		;

	return 0;
}

static int
test_thread_create_join(void)
{
	rte_thread_t thread_id;
	rte_thread_t thread_main_id;

	thread_id_ready = 0;
	RTE_TEST_ASSERT(rte_thread_create(&thread_id, NULL, thread_main, &thread_main_id) == 0,
		"Failed to create thread.");

	while (__atomic_load_n(&thread_id_ready, __ATOMIC_ACQUIRE) == 0)
		;

	RTE_TEST_ASSERT(rte_thread_equal(thread_id, thread_main_id) != 0,
		"Unexpected thread id.");

	__atomic_store_n(&thread_id_ready, 2, __ATOMIC_RELEASE);

	RTE_TEST_ASSERT(rte_thread_join(thread_id, NULL) == 0,
		"Failed to join thread.");

	return 0;
}

static int
test_thread_create_detach(void)
{
	rte_thread_t thread_id;
	rte_thread_t thread_main_id;

	thread_id_ready = 0;
	RTE_TEST_ASSERT(rte_thread_create(&thread_id, NULL, thread_main,
		&thread_main_id) == 0, "Failed to create thread.");

	while (__atomic_load_n(&thread_id_ready, __ATOMIC_ACQUIRE) == 0)
		;

	RTE_TEST_ASSERT(rte_thread_equal(thread_id, thread_main_id) != 0,
		"Unexpected thread id.");

	__atomic_store_n(&thread_id_ready, 2, __ATOMIC_RELEASE);

	RTE_TEST_ASSERT(rte_thread_detach(thread_id) == 0,
		"Failed to detach thread.");

	return 0;
}

static int
test_thread_priority(void)
{
	rte_thread_t thread_id;
	enum rte_thread_priority priority;

	thread_id_ready = 0;
	RTE_TEST_ASSERT(rte_thread_create(&thread_id, NULL, thread_main, NULL) == 0,
		"Failed to create thread");

	while (__atomic_load_n(&thread_id_ready, __ATOMIC_ACQUIRE) == 0)
		;

	priority = RTE_THREAD_PRIORITY_NORMAL;
	RTE_TEST_ASSERT(rte_thread_set_priority(thread_id, priority) == 0,
		"Failed to set thread priority");
	RTE_TEST_ASSERT(rte_thread_get_priority(thread_id, &priority) == 0,
		"Failed to get thread priority");
	RTE_TEST_ASSERT(priority == RTE_THREAD_PRIORITY_NORMAL,
		"Priority set mismatches priority get");

	priority = RTE_THREAD_PRIORITY_REALTIME_CRITICAL;
#ifndef RTE_EXEC_ENV_WINDOWS
	RTE_TEST_ASSERT(rte_thread_set_priority(thread_id, priority) == ENOTSUP,
		"Priority set to critical should fail");
	RTE_TEST_ASSERT(rte_thread_get_priority(thread_id, &priority) == 0,
		"Failed to get thread priority");
	RTE_TEST_ASSERT(priority == RTE_THREAD_PRIORITY_NORMAL,
		"Failed set to critical should have retained normal");
#else
	RTE_TEST_ASSERT(rte_thread_set_priority(thread_id, priority) == 0,
		"Priority set to critical should succeed");
	RTE_TEST_ASSERT(rte_thread_get_priority(thread_id, &priority) == 0,
		"Failed to get thread priority");
	RTE_TEST_ASSERT(priority == RTE_THREAD_PRIORITY_REALTIME_CRITICAL,
		"Priority set mismatches priority get");
#endif

	priority = RTE_THREAD_PRIORITY_NORMAL;
	RTE_TEST_ASSERT(rte_thread_set_priority(thread_id, priority) == 0,
		"Failed to set thread priority");
	RTE_TEST_ASSERT(rte_thread_get_priority(thread_id, &priority) == 0,
		"Failed to get thread priority");
	RTE_TEST_ASSERT(priority == RTE_THREAD_PRIORITY_NORMAL,
		"Priority set mismatches priority get");

	__atomic_store_n(&thread_id_ready, 2, __ATOMIC_RELEASE);

	return 0;
}

static int
test_thread_affinity(void)
{
	rte_thread_t thread_id;
	rte_cpuset_t cpuset0;
	rte_cpuset_t cpuset1;

	thread_id_ready = 0;
	RTE_TEST_ASSERT(rte_thread_create(&thread_id, NULL, thread_main, NULL) == 0,
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

static int
test_thread_attributes_affinity(void)
{
	rte_thread_t thread_id;
	rte_thread_attr_t attr;
	rte_cpuset_t cpuset0;
	rte_cpuset_t cpuset1;

	RTE_TEST_ASSERT(rte_thread_attr_init(&attr) == 0,
		"Failed to initialize thread attributes");

	CPU_ZERO(&cpuset0);
	RTE_TEST_ASSERT(rte_thread_get_affinity_by_id(rte_thread_self(), &cpuset0) == 0,
		"Failed to get thread affinity");
	RTE_TEST_ASSERT(rte_thread_attr_set_affinity(&attr, &cpuset0) == 0,
		"Failed to set thread attributes affinity");
	RTE_TEST_ASSERT(rte_thread_attr_get_affinity(&attr, &cpuset1) == 0,
		"Failed to get thread attributes affinity");
	RTE_TEST_ASSERT(memcmp(&cpuset0, &cpuset1, sizeof(rte_cpuset_t)) == 0,
		"Affinity should be stable");

	thread_id_ready = 0;
	RTE_TEST_ASSERT(rte_thread_create(&thread_id, &attr, thread_main, NULL) == 0,
		"Failed to create attributes affinity thread.");

	while (__atomic_load_n(&thread_id_ready, __ATOMIC_ACQUIRE) == 0)
		;

	RTE_TEST_ASSERT(rte_thread_get_affinity_by_id(thread_id, &cpuset1) == 0,
		"Failed to get attributes thread affinity");
	RTE_TEST_ASSERT(memcmp(&cpuset0, &cpuset1, sizeof(rte_cpuset_t)) == 0,
		"Failed to apply affinity attributes");

	__atomic_store_n(&thread_id_ready, 2, __ATOMIC_RELEASE);

	return 0;
}

static int
test_thread_attributes_priority(void)
{
	rte_thread_t thread_id;
	rte_thread_attr_t attr;
	enum rte_thread_priority priority;

	RTE_TEST_ASSERT(rte_thread_attr_init(&attr) == 0,
		"Failed to initialize thread attributes");
	RTE_TEST_ASSERT(rte_thread_attr_set_priority(&attr, RTE_THREAD_PRIORITY_NORMAL) == 0,
		"Failed to set thread attributes priority");

	thread_id_ready = 0;
	RTE_TEST_ASSERT(rte_thread_create(&thread_id, &attr, thread_main, NULL) == 0,
		"Failed to create attributes priority thread.");

	while (__atomic_load_n(&thread_id_ready, __ATOMIC_ACQUIRE) == 0)
		;

	RTE_TEST_ASSERT(rte_thread_get_priority(thread_id, &priority) == 0,
		"Failed to get thread priority");
	RTE_TEST_ASSERT(priority == RTE_THREAD_PRIORITY_NORMAL,
		"Failed to apply priority attributes");

	__atomic_store_n(&thread_id_ready, 2, __ATOMIC_RELEASE);

	return 0;
}

static int
test_thread_control_create_join(void)
{
	rte_thread_t thread_id;
	rte_thread_t thread_main_id;

	thread_id_ready = 0;
	RTE_TEST_ASSERT(rte_thread_create_control(&thread_id, "dpdk-test-thcc",
		thread_main, &thread_main_id) == 0,
		"Failed to create thread.");

	while (__atomic_load_n(&thread_id_ready, __ATOMIC_ACQUIRE) == 0)
		;

	RTE_TEST_ASSERT(rte_thread_equal(thread_id, thread_main_id) != 0,
		"Unexpected thread id.");

	__atomic_store_n(&thread_id_ready, 2, __ATOMIC_RELEASE);

	RTE_TEST_ASSERT(rte_thread_join(thread_id, NULL) == 0,
		"Failed to join thread.");

	return 0;
}

static struct unit_test_suite threads_test_suite = {
	.suite_name = "threads autotest",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
		TEST_CASE(test_thread_create_join),
		TEST_CASE(test_thread_create_detach),
		TEST_CASE(test_thread_affinity),
		TEST_CASE(test_thread_priority),
		TEST_CASE(test_thread_attributes_affinity),
		TEST_CASE(test_thread_attributes_priority),
		TEST_CASE(test_thread_control_create_join),
		TEST_CASES_END()
	}
};

static int
test_threads(void)
{
	return unit_test_suite_runner(&threads_test_suite);
}

REGISTER_FAST_TEST(threads_autotest, true, true, test_threads);
