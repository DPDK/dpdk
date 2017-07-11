/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_cycles.h>

#include <rte_service.h>
#include <rte_service_component.h>

#include "test.h"

/* used as the service core ID */
static uint32_t slcore_id;
/* used as timestamp to detect if a service core is running */
static uint64_t service_tick;
/* used as a flag to check if a function was run */
static uint32_t service_remote_launch_flag;

#define SERVICE_DELAY 1

#define DUMMY_SERVICE_NAME "dummy_service"
#define MT_SAFE_SERVICE_NAME "mt_safe_service"

static int
testsuite_setup(void)
{
	slcore_id = rte_get_next_lcore(/* start core */ -1,
				       /* skip master */ 1,
				       /* wrap */ 0);

	return TEST_SUCCESS;
}

static void
testsuite_teardown(void)
{
	/* release service cores? */
}

static int32_t dummy_cb(void *args)
{
	RTE_SET_USED(args);
	service_tick++;
	rte_delay_ms(SERVICE_DELAY);
	return 0;
}

static int32_t dummy_mt_unsafe_cb(void *args)
{
	/* before running test, the initialization has set pass_test to 1.
	 * If the cmpset in service-cores is working correctly, the code here
	 * should never fail to take the lock. If the lock *is* taken, fail the
	 * test, because two threads are concurrently in a non-MT safe callback.
	 */
	uint32_t *test_params = args;
	uint32_t *atomic_lock = &test_params[0];
	uint32_t *pass_test = &test_params[1];
	int lock_taken = rte_atomic32_cmpset(atomic_lock, 0, 1);
	if (lock_taken) {
		/* delay with the lock held */
		rte_delay_ms(250);
		rte_atomic32_clear((rte_atomic32_t *)atomic_lock);
	} else {
		/* 2nd thread will fail to take lock, so set pass flag */
		*pass_test = 0;
	}

	return 0;
}


static int32_t dummy_mt_safe_cb(void *args)
{
	/* Atomic checks to ensure MT safe services allow > 1 thread to
	 * concurrently run the callback. The concept is as follows;
	 * 1) if lock is available, take the lock then delay
	 * 2) if first lock is taken, and a thread arrives in the CB, we know
	 *    that 2 threads are running the callback at the same time: MT safe
	 */
	uint32_t *test_params = args;
	uint32_t *atomic_lock = &test_params[0];
	uint32_t *pass_test = &test_params[1];
	int lock_taken = rte_atomic32_cmpset(atomic_lock, 0, 1);
	if (lock_taken) {
		/* delay with the lock held */
		rte_delay_ms(250);
		rte_atomic32_clear((rte_atomic32_t *)atomic_lock);
	} else {
		/* 2nd thread will fail to take lock, so set pass flag */
		*pass_test = 1;
	}

	return 0;
}

/* unregister all services */
static int
unregister_all(void)
{
	uint32_t i;
	struct rte_service_spec *dead = (struct rte_service_spec *)0xdead;

	TEST_ASSERT_EQUAL(-EINVAL, rte_service_unregister(0),
			"Unregistered NULL pointer");
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_unregister(dead),
			"Unregistered invalid pointer");

	uint32_t c = rte_service_get_count();
	for (i = 0; i < c; i++) {
		struct rte_service_spec *s = rte_service_get_by_id(i);
		TEST_ASSERT_EQUAL(0, rte_service_unregister(s),
				"Error unregistering a valid service");
	}

	rte_service_lcore_reset_all();

	return TEST_SUCCESS;
}

/* register a single dummy service */
static int
dummy_register(void)
{
	/* make sure there are no remains from previous tests */
	unregister_all();

	struct rte_service_spec service;
	memset(&service, 0, sizeof(struct rte_service_spec));

	TEST_ASSERT_EQUAL(-EINVAL, rte_service_register(&service),
			"Invalid callback");
	service.callback = dummy_cb;

	TEST_ASSERT_EQUAL(-EINVAL, rte_service_register(&service),
			"Invalid name");
	snprintf(service.name, sizeof(service.name), DUMMY_SERVICE_NAME);

	TEST_ASSERT_EQUAL(0, rte_service_register(&service),
			"Failed to register valid service");

	return TEST_SUCCESS;
}

/* verify get_by_name() service lookup */
static int
service_get_by_name(void)
{
	unregister_all();

	/* ensure with no services registered returns NULL */
	TEST_ASSERT_EQUAL(0, rte_service_get_by_name(DUMMY_SERVICE_NAME),
			"Service get by name should return NULL");

	/* register service */
	struct rte_service_spec service;
	memset(&service, 0, sizeof(struct rte_service_spec));
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_register(&service),
			"Invalid callback");
	service.callback = dummy_cb;
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_register(&service),
			"Invalid name");
	snprintf(service.name, sizeof(service.name), DUMMY_SERVICE_NAME);
	TEST_ASSERT_EQUAL(0, rte_service_register(&service),
			"Failed to register valid service");

	/* ensure with dummy services registered returns same ptr as ID */
	struct rte_service_spec *s_by_id = rte_service_get_by_id(0);
	TEST_ASSERT_EQUAL(s_by_id, rte_service_get_by_name(DUMMY_SERVICE_NAME),
			"Service get_by_name should equal get_by_id()");

	unregister_all();

	/* ensure after unregister, get_by_name returns NULL */
	TEST_ASSERT_EQUAL(0, rte_service_get_by_name(DUMMY_SERVICE_NAME),
			"get by name should return NULL after unregister");

	return TEST_SUCCESS;
}

/* verify probe of capabilities */
static int
service_probe_capability(void)
{
	unregister_all();

	struct rte_service_spec service;
	memset(&service, 0, sizeof(struct rte_service_spec));
	service.callback = dummy_cb;
	snprintf(service.name, sizeof(service.name), DUMMY_SERVICE_NAME);
	service.capabilities |= RTE_SERVICE_CAP_MT_SAFE;
	TEST_ASSERT_EQUAL(0, rte_service_register(&service),
			"Register of MT SAFE service failed");

	/* verify flag is enabled */
	struct rte_service_spec *s = rte_service_get_by_id(0);
	int32_t mt = rte_service_probe_capability(s, RTE_SERVICE_CAP_MT_SAFE);
	TEST_ASSERT_EQUAL(1, mt, "MT SAFE capability flag not set.");


	unregister_all();

	memset(&service, 0, sizeof(struct rte_service_spec));
	service.callback = dummy_cb;
	snprintf(service.name, sizeof(service.name), DUMMY_SERVICE_NAME);
	TEST_ASSERT_EQUAL(0, rte_service_register(&service),
			"Register of non-MT safe service failed");

	/* verify flag is enabled */
	s = rte_service_get_by_id(0);
	mt = rte_service_probe_capability(s, RTE_SERVICE_CAP_MT_SAFE);
	TEST_ASSERT_EQUAL(0, mt, "MT SAFE cap flag set on non MT SAFE service");

	return unregister_all();
}

/* verify the service name */
static int
service_name(void)
{
	struct rte_service_spec *service = rte_service_get_by_id(0);

	int equal = strcmp(service->name, DUMMY_SERVICE_NAME);
	TEST_ASSERT_EQUAL(0, equal, "Error: Service name not correct");

	return unregister_all();
}

/* verify service dump */
static int
service_dump(void)
{
	struct rte_service_spec *service = rte_service_get_by_id(0);
	rte_service_set_stats_enable(service, 1);
	rte_service_dump(stdout, service);
	rte_service_set_stats_enable(service, 0);
	rte_service_dump(stdout, service);
	return unregister_all();
}

/* start and stop a service */
static int
service_start_stop(void)
{
	struct rte_service_spec *service = rte_service_get_by_id(0);

	/* is_running() returns if service is running and slcore is mapped */
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_id),
			"Service core add did not return zero");
	int ret = rte_service_enable_on_lcore(service, slcore_id);
	TEST_ASSERT_EQUAL(0, ret,
			"Enabling service core, expected 0 got %d", ret);

	TEST_ASSERT_EQUAL(0, rte_service_is_running(service),
			"Error: Service should be stopped");

	TEST_ASSERT_EQUAL(0, rte_service_stop(service),
			"Error: Service stopped returned non-zero");

	TEST_ASSERT_EQUAL(0, rte_service_is_running(service),
			"Error: Service is running - should be stopped");

	TEST_ASSERT_EQUAL(0, rte_service_start(service),
			"Error: Service start returned non-zero");

	TEST_ASSERT_EQUAL(1, rte_service_is_running(service),
			"Error: Service is not running");

	return unregister_all();
}


static int
service_remote_launch_func(void *arg)
{
	RTE_SET_USED(arg);
	service_remote_launch_flag = 1;
	return 0;
}

/* enable and disable a lcore for a service */
static int
service_lcore_en_dis_able(void)
{
	struct rte_service_spec *s = rte_service_get_by_id(0);

	/* expected failure cases */
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_enable_on_lcore(s, 100000),
			"Enable on invalid core did not fail");
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_disable_on_lcore(s, 100000),
			"Disable on invalid core did not fail");

	/* add service core to allow enabling */
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_id),
			"Add service core failed when not in use before");

	/* valid enable */
	TEST_ASSERT_EQUAL(0, rte_service_enable_on_lcore(s, slcore_id),
			"Enabling valid service and core failed");
	TEST_ASSERT_EQUAL(1, rte_service_get_enabled_on_lcore(s, slcore_id),
			"Enabled core returned not-enabled");

	/* valid disable */
	TEST_ASSERT_EQUAL(0, rte_service_disable_on_lcore(s, slcore_id),
			"Disabling valid service and lcore failed");
	TEST_ASSERT_EQUAL(0, rte_service_get_enabled_on_lcore(s, slcore_id),
			"Disabled core returned enabled");

	/* call remote_launch to verify that app can launch ex-service lcore */
	service_remote_launch_flag = 0;
	int ret = rte_eal_remote_launch(service_remote_launch_func, NULL,
					slcore_id);
	TEST_ASSERT_EQUAL(0, ret, "Ex-service core remote launch failed.");
	rte_eal_mp_wait_lcore();
	TEST_ASSERT_EQUAL(1, service_remote_launch_flag,
			"Ex-service core function call had no effect.");

	return unregister_all();
}

static int
service_lcore_running_check(void)
{
	uint64_t tick = service_tick;
	rte_delay_ms(SERVICE_DELAY * 10);
	/* if (tick != service_tick) we know the lcore as polled the service */
	return tick != service_tick;
}

static int
service_lcore_add_del(void)
{
	/* check initial count */
	TEST_ASSERT_EQUAL(0, rte_service_lcore_count(),
			"Service lcore count has value before adding a lcore");

	/* check service lcore add */
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_id),
			"Add service core failed when not in use before");
	TEST_ASSERT_EQUAL(-EALREADY, rte_service_lcore_add(slcore_id),
			"Add service core failed to refuse in-use lcore");

	/* check count */
	TEST_ASSERT_EQUAL(1, rte_service_lcore_count(),
			"Service core count not equal to one");

	/* retrieve core list, checking lcore ids */
	const uint32_t size = 4;
	uint32_t service_core_ids[size];
	int32_t n = rte_service_lcore_list(service_core_ids, size);
	TEST_ASSERT_EQUAL(1, n, "Service core list return should equal 1");
	TEST_ASSERT_EQUAL(slcore_id, service_core_ids[0],
				"Service core list lcore must equal slcore_id");

	/* recheck count, add more cores, and check count */
	TEST_ASSERT_EQUAL(1, rte_service_lcore_count(),
			"Service core count not equal to one");
	uint32_t slcore_1 = rte_get_next_lcore(/* start core */ -1,
					       /* skip master */ 1,
					       /* wrap */ 0);
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_1),
			"Service core add did not return zero");
	uint32_t slcore_2 = rte_get_next_lcore(/* start core */ slcore_1,
					       /* skip master */ 1,
					       /* wrap */ 0);
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_2),
			"Service core add did not return zero");

	uint32_t count = rte_service_lcore_count();
	const uint32_t cores_at_this_point = 3;
	TEST_ASSERT_EQUAL(cores_at_this_point, count,
			"Service core count %d, expected %d", count,
			cores_at_this_point);

	/* check longer service core list */
	n = rte_service_lcore_list(service_core_ids, size);
	TEST_ASSERT_EQUAL(3, n, "Service core list return should equal 3");
	TEST_ASSERT_EQUAL(slcore_id, service_core_ids[0],
				"Service core list[0] lcore must equal 1");
	TEST_ASSERT_EQUAL(slcore_1, service_core_ids[1],
				"Service core list[1] lcore must equal 2");
	TEST_ASSERT_EQUAL(slcore_2, service_core_ids[2],
				"Service core list[2] lcore must equal 3");

	/* recheck count, remove lcores, check remaining lcore_id is correct */
	TEST_ASSERT_EQUAL(3, rte_service_lcore_count(),
			"Service core count not equal to three");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_del(slcore_1),
			"Service core add did not return zero");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_del(slcore_2),
			"Service core add did not return zero");
	TEST_ASSERT_EQUAL(1, rte_service_lcore_count(),
			"Service core count not equal to one");
	n = rte_service_lcore_list(service_core_ids, size);
	TEST_ASSERT_EQUAL(1, n, "Service core list return should equal one");
	TEST_ASSERT_EQUAL(slcore_id, service_core_ids[0],
				"Service core list[0] lcore must equal %d",
				slcore_id);

	return unregister_all();
}

static int
service_threaded_test(int mt_safe)
{
	unregister_all();

	/* add next 2 cores */
	uint32_t slcore_1 = rte_get_next_lcore(/* start core */ -1,
					       /* skip master */ 1,
					       /* wrap */ 0);
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_1),
			"mt safe lcore add fail");
	uint32_t slcore_2 = rte_get_next_lcore(/* start core */ slcore_1,
					       /* skip master */ 1,
					       /* wrap */ 0);
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_2),
			"mt safe lcore add fail");

	/* Use atomic locks to verify that two threads are in the same function
	 * at the same time. These are passed to the unit tests through the
	 * callback userdata parameter
	 */
	uint32_t test_params[2];
	memset(test_params, 0, sizeof(uint32_t) * 2);

	/* register MT safe service. */
	struct rte_service_spec service;
	memset(&service, 0, sizeof(struct rte_service_spec));
	service.callback_userdata = test_params;
	snprintf(service.name, sizeof(service.name), MT_SAFE_SERVICE_NAME);

	if (mt_safe) {
		service.callback = dummy_mt_safe_cb;
		service.capabilities |= RTE_SERVICE_CAP_MT_SAFE;
	} else {
		/* initialize to pass, see callback comment for details */
		test_params[1] = 1;
		service.callback = dummy_mt_unsafe_cb;
	}

	TEST_ASSERT_EQUAL(0, rte_service_register(&service),
			"Register of MT SAFE service failed");

	struct rte_service_spec *s = rte_service_get_by_id(0);
	TEST_ASSERT_EQUAL(0, rte_service_start(s),
			"Starting valid service failed");
	TEST_ASSERT_EQUAL(0, rte_service_enable_on_lcore(s, slcore_1),
			"Failed to enable lcore 1 on mt safe service");
	TEST_ASSERT_EQUAL(0, rte_service_enable_on_lcore(s, slcore_2),
			"Failed to enable lcore 2 on mt safe service");
	rte_service_lcore_start(slcore_1);
	rte_service_lcore_start(slcore_2);

	/* wait for the worker threads to run */
	rte_delay_ms(500);
	rte_service_lcore_stop(slcore_1);
	rte_service_lcore_stop(slcore_2);

	TEST_ASSERT_EQUAL(1, test_params[1],
			"MT Safe service not run by two cores concurrently");

	TEST_ASSERT_EQUAL(0, rte_service_stop(s),
			"Failed to stop MT Safe service");

	unregister_all();

	/* return the value of the callback pass_test variable to caller */
	return test_params[1];
}

/* tests an MT SAFE service with two cores. The callback function ensures that
 * two threads access the callback concurrently.
 */
static int
service_mt_safe_poll(void)
{
	int mt_safe = 1;
	TEST_ASSERT_EQUAL(1, service_threaded_test(mt_safe),
			"Error: MT Safe service not run by two cores concurrently");
	return TEST_SUCCESS;
}

/* tests a NON mt safe service with two cores, the callback is serialized
 * using the atomic cmpset.
 */
static int
service_mt_unsafe_poll(void)
{
	int mt_safe = 0;
	TEST_ASSERT_EQUAL(1, service_threaded_test(mt_safe),
			"Error: NON MT Safe service run by two cores concurrently");
	return TEST_SUCCESS;
}

/* start and stop a service core - ensuring it goes back to sleep */
static int
service_lcore_start_stop(void)
{
	/* start service core and service, create mapping so tick() runs */
	struct rte_service_spec *s = rte_service_get_by_id(0);
	TEST_ASSERT_EQUAL(0, rte_service_start(s),
			"Starting valid service failed");
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_enable_on_lcore(s, slcore_id),
			"Enabling valid service on non-service core must fail");

	/* core start */
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_lcore_start(slcore_id),
			"Service core start without add should return EINVAL");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_id),
			"Service core add did not return zero");
	TEST_ASSERT_EQUAL(0, rte_service_enable_on_lcore(s, slcore_id),
			"Enabling valid service on valid core failed");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_start(slcore_id),
			"Service core start after add failed");
	TEST_ASSERT_EQUAL(-EALREADY, rte_service_lcore_start(slcore_id),
			"Service core expected as running but was stopped");

	/* ensures core really is running the service function */
	TEST_ASSERT_EQUAL(1, service_lcore_running_check(),
			"Service core expected to poll service but it didn't");

	/* core stop */
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_lcore_stop(100000),
			"Invalid Service core stop should return -EINVAL");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_stop(slcore_id),
			"Service core stop expected to return 0");
	TEST_ASSERT_EQUAL(-EALREADY, rte_service_lcore_stop(slcore_id),
			"Already stopped service core should return -EALREADY");

	/* ensure service is not longer running */
	TEST_ASSERT_EQUAL(0, service_lcore_running_check(),
			"Service core expected to poll service but it didn't");

	TEST_ASSERT_EQUAL(0, rte_service_lcore_del(slcore_id),
			"Service core del did not return zero");

	return unregister_all();
}

static struct unit_test_suite service_tests  = {
	.suite_name = "service core test suite",
	.setup = testsuite_setup,
	.teardown = testsuite_teardown,
	.unit_test_cases = {
		TEST_CASE_ST(dummy_register, NULL, unregister_all),
		TEST_CASE_ST(dummy_register, NULL, service_name),
		TEST_CASE_ST(dummy_register, NULL, service_get_by_name),
		TEST_CASE_ST(dummy_register, NULL, service_dump),
		TEST_CASE_ST(dummy_register, NULL, service_probe_capability),
		TEST_CASE_ST(dummy_register, NULL, service_start_stop),
		TEST_CASE_ST(dummy_register, NULL, service_lcore_add_del),
		TEST_CASE_ST(dummy_register, NULL, service_lcore_start_stop),
		TEST_CASE_ST(dummy_register, NULL, service_lcore_en_dis_able),
		TEST_CASE_ST(dummy_register, NULL, service_mt_unsafe_poll),
		TEST_CASE_ST(dummy_register, NULL, service_mt_safe_poll),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_service_common(void)
{
	return unit_test_suite_runner(&service_tests);
}

REGISTER_TEST_COMMAND(service_autotest, test_service_common);
