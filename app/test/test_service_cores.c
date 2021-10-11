/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
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
				       /* skip main */ 1,
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
	 * If the CAS in service-cores is working correctly, the code here
	 * should never fail to take the lock. If the lock *is* taken, fail the
	 * test, because two threads are concurrently in a non-MT safe callback.
	 */
	uint32_t *test_params = args;
	uint32_t *lock = &test_params[0];
	uint32_t *pass_test = &test_params[1];
	uint32_t exp = 0;
	int lock_taken = __atomic_compare_exchange_n(lock, &exp, 1, 0,
					__ATOMIC_RELAXED, __ATOMIC_RELAXED);
	if (lock_taken) {
		/* delay with the lock held */
		rte_delay_ms(250);
		__atomic_store_n(lock, 0, __ATOMIC_RELAXED);
	} else {
		/* 2nd thread will fail to take lock, so clear pass flag */
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
	uint32_t *lock = &test_params[0];
	uint32_t *pass_test = &test_params[1];
	uint32_t exp = 0;
	int lock_taken = __atomic_compare_exchange_n(lock, &exp, 1, 0,
					__ATOMIC_RELAXED, __ATOMIC_RELAXED);
	if (lock_taken) {
		/* delay with the lock held */
		rte_delay_ms(250);
		__atomic_store_n(lock, 0, __ATOMIC_RELAXED);
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

	TEST_ASSERT_EQUAL(-EINVAL, rte_service_component_unregister(1000),
			"Unregistered invalid service id");

	uint32_t c = rte_service_get_count();
	for (i = 0; i < c; i++) {
		TEST_ASSERT_EQUAL(0, rte_service_component_unregister(i),
				"Error unregistering a valid service");
	}

	rte_service_lcore_reset_all();
	rte_eal_mp_wait_lcore();

	return TEST_SUCCESS;
}

/* Wait until service lcore not active, or for 100x SERVICE_DELAY */
static void
wait_slcore_inactive(uint32_t slcore_id)
{
	int i;

	for (i = 0; rte_service_lcore_may_be_active(slcore_id) == 1 &&
			i < 100; i++)
		rte_delay_ms(SERVICE_DELAY);
}

/* register a single dummy service */
static int
dummy_register(void)
{
	/* make sure there are no remains from previous tests */
	unregister_all();

	struct rte_service_spec service;
	memset(&service, 0, sizeof(struct rte_service_spec));

	TEST_ASSERT_EQUAL(-EINVAL,
			rte_service_component_register(&service, NULL),
			"Invalid callback");
	service.callback = dummy_cb;

	TEST_ASSERT_EQUAL(-EINVAL,
			rte_service_component_register(&service, NULL),
			"Invalid name");
	snprintf(service.name, sizeof(service.name), DUMMY_SERVICE_NAME);

	uint32_t id;
	TEST_ASSERT_EQUAL(0, rte_service_component_register(&service, &id),
			"Failed to register valid service");

	rte_service_component_runstate_set(id, 1);

	return TEST_SUCCESS;
}

/* verify get_by_name() service lookup */
static int
service_get_by_name(void)
{
	unregister_all();

	uint32_t sid;
	TEST_ASSERT_EQUAL(-ENODEV,
			rte_service_get_by_name(DUMMY_SERVICE_NAME, &sid),
			"get by name with invalid name should return -ENODEV");
	TEST_ASSERT_EQUAL(-EINVAL,
			rte_service_get_by_name(DUMMY_SERVICE_NAME, 0x0),
			"get by name with NULL ptr should return -ENODEV");

	/* register service */
	struct rte_service_spec service;
	memset(&service, 0, sizeof(struct rte_service_spec));
	TEST_ASSERT_EQUAL(-EINVAL,
			rte_service_component_register(&service, NULL),
			"Invalid callback");
	service.callback = dummy_cb;
	TEST_ASSERT_EQUAL(-EINVAL,
			rte_service_component_register(&service, NULL),
			"Invalid name");
	snprintf(service.name, sizeof(service.name), DUMMY_SERVICE_NAME);
	TEST_ASSERT_EQUAL(0, rte_service_component_register(&service, NULL),
			"Failed to register valid service");

	/* we unregistered all service, now registering 1, should be id 0 */
	uint32_t service_id_as_expected = 0;
	TEST_ASSERT_EQUAL(0, rte_service_get_by_name(DUMMY_SERVICE_NAME, &sid),
			"Service get_by_name should return 0 on valid inputs");
	TEST_ASSERT_EQUAL(service_id_as_expected, sid,
			"Service get_by_name should equal expected id");

	unregister_all();

	/* ensure after unregister, get_by_name returns NULL */
	TEST_ASSERT_EQUAL(-ENODEV,
			rte_service_get_by_name(DUMMY_SERVICE_NAME, &sid),
			"get by name should return -ENODEV after unregister");

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
	TEST_ASSERT_EQUAL(0, rte_service_component_register(&service, NULL),
			"Register of MT SAFE service failed");

	/* verify flag is enabled */
	const uint32_t sid = 0;
	int32_t mt = rte_service_probe_capability(sid, RTE_SERVICE_CAP_MT_SAFE);
	TEST_ASSERT_EQUAL(1, mt, "MT SAFE capability flag not set.");


	unregister_all();

	memset(&service, 0, sizeof(struct rte_service_spec));
	service.callback = dummy_cb;
	snprintf(service.name, sizeof(service.name), DUMMY_SERVICE_NAME);
	TEST_ASSERT_EQUAL(0, rte_service_component_register(&service, NULL),
			"Register of non-MT safe service failed");

	/* verify flag is enabled */
	mt = rte_service_probe_capability(sid, RTE_SERVICE_CAP_MT_SAFE);
	TEST_ASSERT_EQUAL(0, mt, "MT SAFE cap flag set on non MT SAFE service");

	return unregister_all();
}

/* verify the service name */
static int
service_name(void)
{
	const char *name = rte_service_get_name(0);
	int equal = strcmp(name, DUMMY_SERVICE_NAME);
	TEST_ASSERT_EQUAL(0, equal, "Error: Service name not correct");

	return unregister_all();
}

/* verify service attr get */
static int
service_attr_get(void)
{
	/* ensure all services unregistered so cycle counts are zero */
	unregister_all();

	struct rte_service_spec service;
	memset(&service, 0, sizeof(struct rte_service_spec));
	service.callback = dummy_cb;
	snprintf(service.name, sizeof(service.name), DUMMY_SERVICE_NAME);
	service.capabilities |= RTE_SERVICE_CAP_MT_SAFE;
	uint32_t id;
	TEST_ASSERT_EQUAL(0, rte_service_component_register(&service, &id),
			"Register of  service failed");
	rte_service_component_runstate_set(id, 1);
	TEST_ASSERT_EQUAL(0, rte_service_runstate_set(id, 1),
			"Error: Service start returned non-zero");
	rte_service_set_stats_enable(id, 1);

	uint32_t attr_id = UINT32_MAX;
	uint64_t attr_value = 0xdead;
	/* check error return values */
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_attr_get(id, attr_id,
							&attr_value),
			"Invalid attr_id didn't return -EINVAL");

	attr_id = RTE_SERVICE_ATTR_CYCLES;
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_attr_get(UINT32_MAX, attr_id,
							&attr_value),
			"Invalid service id didn't return -EINVAL");

	TEST_ASSERT_EQUAL(-EINVAL, rte_service_attr_get(id, attr_id, NULL),
			"Invalid attr_value pointer id didn't return -EINVAL");

	/* check correct (zero) return value and correct value (zero) */
	TEST_ASSERT_EQUAL(0, rte_service_attr_get(id, attr_id, &attr_value),
			"Valid attr_get() call didn't return success");
	TEST_ASSERT_EQUAL(0, attr_value,
			"attr_get() call didn't set correct cycles (zero)");
	/* check correct call count */
	const int attr_calls = RTE_SERVICE_ATTR_CALL_COUNT;
	TEST_ASSERT_EQUAL(0, rte_service_attr_get(id, attr_calls, &attr_value),
			"Valid attr_get() call didn't return success");
	TEST_ASSERT_EQUAL(0, attr_value,
			"attr_get() call didn't get call count (zero)");

	/* Call service to increment cycle count */
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_id),
			"Service core add did not return zero");
	TEST_ASSERT_EQUAL(0, rte_service_map_lcore_set(id, slcore_id, 1),
			"Enabling valid service and core failed");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_start(slcore_id),
			"Starting service core failed");

	/* wait for the service lcore to run */
	rte_delay_ms(200);

	TEST_ASSERT_EQUAL(0, rte_service_attr_get(id, attr_id, &attr_value),
			"Valid attr_get() call didn't return success");
	int cycles_gt_zero = attr_value > 0;
	TEST_ASSERT_EQUAL(1, cycles_gt_zero,
			"attr_get() failed to get cycles (expected > zero)");

	TEST_ASSERT_EQUAL(0, rte_service_map_lcore_set(id, slcore_id, 0),
			"Disabling valid service and core failed");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_stop(slcore_id),
			"Failed to stop service lcore");

	wait_slcore_inactive(slcore_id);

	TEST_ASSERT_EQUAL(0, rte_service_lcore_may_be_active(slcore_id),
			  "Service lcore not stopped after waiting.");

	TEST_ASSERT_EQUAL(0, rte_service_attr_get(id, attr_calls, &attr_value),
			"Valid attr_get() call didn't return success");
	TEST_ASSERT_EQUAL(1, (attr_value > 0),
			"attr_get() call didn't get call count (zero)");

	TEST_ASSERT_EQUAL(0, rte_service_attr_reset_all(id),
			"Valid attr_reset_all() return success");

	TEST_ASSERT_EQUAL(0, rte_service_attr_get(id, attr_id, &attr_value),
			"Valid attr_get() call didn't return success");
	TEST_ASSERT_EQUAL(0, attr_value,
			"attr_get() call didn't set correct cycles (zero)");
	/* ensure call count > zero */
	TEST_ASSERT_EQUAL(0, rte_service_attr_get(id, attr_calls, &attr_value),
			"Valid attr_get() call didn't return success");
	TEST_ASSERT_EQUAL(0, (attr_value > 0),
			"attr_get() call didn't get call count (zero)");

	return unregister_all();
}

/* verify service lcore attr get */
static int
service_lcore_attr_get(void)
{
	/* ensure all services unregistered so cycle counts are zero */
	unregister_all();

	struct rte_service_spec service;
	memset(&service, 0, sizeof(struct rte_service_spec));
	service.callback = dummy_cb;
	snprintf(service.name, sizeof(service.name), DUMMY_SERVICE_NAME);
	service.capabilities |= RTE_SERVICE_CAP_MT_SAFE;
	uint32_t id;
	TEST_ASSERT_EQUAL(0, rte_service_component_register(&service, &id),
			"Register of  service failed");
	rte_service_component_runstate_set(id, 1);
	TEST_ASSERT_EQUAL(0, rte_service_runstate_set(id, 1),
			"Error: Service start returned non-zero");
	rte_service_set_stats_enable(id, 1);

	uint64_t lcore_attr_value = 0xdead;
	uint32_t lcore_attr_id = UINT32_MAX;

	/* check error return values */
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_lcore_attr_get(UINT32_MAX,
			lcore_attr_id, &lcore_attr_value),
			"Invalid lcore_id didn't return -EINVAL");
	TEST_ASSERT_EQUAL(-ENOTSUP, rte_service_lcore_attr_get(rte_lcore_id(),
			lcore_attr_id, &lcore_attr_value),
			"Non-service core didn't return -ENOTSUP");

	/* Start service core to increment loop count */
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_id),
			"Service core add did not return zero");
	TEST_ASSERT_EQUAL(0, rte_service_map_lcore_set(id, slcore_id, 1),
			"Enabling valid service and core failed");
	/* Ensure service is not active before starting */
	TEST_ASSERT_EQUAL(0, rte_service_lcore_may_be_active(slcore_id),
			"Not-active service core reported as active");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_start(slcore_id),
			"Starting service core failed");

	/* wait for the service lcore to run */
	rte_delay_ms(200);

	lcore_attr_id = RTE_SERVICE_LCORE_ATTR_LOOPS;
	TEST_ASSERT_EQUAL(0, rte_service_lcore_attr_get(slcore_id,
			lcore_attr_id, &lcore_attr_value),
			"Valid lcore_attr_get() call didn't return success");
	int loops_gt_zero = lcore_attr_value > 0;
	TEST_ASSERT_EQUAL(1, loops_gt_zero,
			"lcore_attr_get() failed to get loops "
			"(expected > zero)");

	lcore_attr_id++;  // invalid lcore attr id
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_lcore_attr_get(slcore_id,
			lcore_attr_id, &lcore_attr_value),
			"Invalid lcore attr didn't return -EINVAL");

	/* Ensure service is active */
	TEST_ASSERT_EQUAL(1, rte_service_lcore_may_be_active(slcore_id),
			"Active service core reported as not-active");

	TEST_ASSERT_EQUAL(0, rte_service_map_lcore_set(id, slcore_id, 0),
			"Disabling valid service and core failed");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_stop(slcore_id),
			"Failed to stop service lcore");

	wait_slcore_inactive(slcore_id);

	TEST_ASSERT_EQUAL(0, rte_service_lcore_may_be_active(slcore_id),
			  "Service lcore not stopped after waiting.");

	TEST_ASSERT_EQUAL(0, rte_service_lcore_attr_reset_all(slcore_id),
			  "Valid lcore_attr_reset_all() didn't return success");

	lcore_attr_id = RTE_SERVICE_LCORE_ATTR_LOOPS;
	TEST_ASSERT_EQUAL(0, rte_service_lcore_attr_get(slcore_id,
			lcore_attr_id, &lcore_attr_value),
			"Valid lcore_attr_get() call didn't return success");
	TEST_ASSERT_EQUAL(0, lcore_attr_value,
			"lcore_attr_get() didn't get correct loop count "
			"(zero)");

	return unregister_all();
}

/* verify service dump */
static int
service_dump(void)
{
	const uint32_t sid = 0;
	rte_service_set_stats_enable(sid, 1);
	rte_service_dump(stdout, 0);
	rte_service_set_stats_enable(sid, 0);
	rte_service_dump(stdout, 0);
	return unregister_all();
}

/* start and stop a service */
static int
service_start_stop(void)
{
	const uint32_t sid = 0;

	/* runstate_get() returns if service is running and slcore is mapped */
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_id),
			"Service core add did not return zero");
	int ret = rte_service_map_lcore_set(sid, slcore_id, 1);
	TEST_ASSERT_EQUAL(0, ret,
			"Enabling service core, expected 0 got %d", ret);

	TEST_ASSERT_EQUAL(0, rte_service_runstate_get(sid),
			"Error: Service should be stopped");

	TEST_ASSERT_EQUAL(0, rte_service_runstate_set(sid, 0),
			"Error: Service stopped returned non-zero");

	TEST_ASSERT_EQUAL(0, rte_service_runstate_get(sid),
			"Error: Service is running - should be stopped");

	TEST_ASSERT_EQUAL(0, rte_service_runstate_set(sid, 1),
			"Error: Service start returned non-zero");

	TEST_ASSERT_EQUAL(1, rte_service_runstate_get(sid),
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
	const uint32_t sid = 0;

	/* expected failure cases */
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_map_lcore_set(sid, 100000, 1),
			"Enable on invalid core did not fail");
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_map_lcore_set(sid, 100000, 0),
			"Disable on invalid core did not fail");

	/* add service core to allow enabling */
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_id),
			"Add service core failed when not in use before");

	/* valid enable */
	TEST_ASSERT_EQUAL(0, rte_service_map_lcore_set(sid, slcore_id, 1),
			"Enabling valid service and core failed");
	TEST_ASSERT_EQUAL(1, rte_service_map_lcore_get(sid, slcore_id),
			"Enabled core returned not-enabled");

	/* valid disable */
	TEST_ASSERT_EQUAL(0, rte_service_map_lcore_set(sid, slcore_id, 0),
			"Disabling valid service and lcore failed");
	TEST_ASSERT_EQUAL(0, rte_service_map_lcore_get(sid, slcore_id),
			"Disabled core returned enabled");

	/* call remote_launch to verify that app can launch ex-service lcore */
	service_remote_launch_flag = 0;
	rte_eal_wait_lcore(slcore_id);
	int ret = rte_eal_remote_launch(service_remote_launch_func, NULL,
					slcore_id);
	TEST_ASSERT_EQUAL(0, ret, "Ex-service core remote launch failed.");
	rte_eal_wait_lcore(slcore_id);
	TEST_ASSERT_EQUAL(1, service_remote_launch_flag,
			"Ex-service core function call had no effect.");

	return unregister_all();
}

static int
service_lcore_running_check(void)
{
	uint64_t tick = service_tick;
	rte_delay_ms(SERVICE_DELAY * 100);
	/* if (tick != service_tick) we know the lcore as polled the service */
	return tick != service_tick;
}

static int
service_lcore_add_del(void)
{
	if (!rte_lcore_is_enabled(0) || !rte_lcore_is_enabled(1) ||
	    !rte_lcore_is_enabled(2) || !rte_lcore_is_enabled(3))
		return TEST_SKIPPED;

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
					       /* skip main */ 1,
					       /* wrap */ 0);
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_1),
			"Service core add did not return zero");
	uint32_t slcore_2 = rte_get_next_lcore(/* start core */ slcore_1,
					       /* skip main */ 1,
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
					       /* skip main */ 1,
					       /* wrap */ 0);
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_1),
			"mt safe lcore add fail");
	uint32_t slcore_2 = rte_get_next_lcore(/* start core */ slcore_1,
					       /* skip main */ 1,
					       /* wrap */ 0);
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_2),
			"mt safe lcore add fail");

	/* Use locks to verify that two threads are in the same function
	 * at the same time. These are passed to the unit tests through
	 * the callback userdata parameter.
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
	} else
		service.callback = dummy_mt_unsafe_cb;

	uint32_t id;
	TEST_ASSERT_EQUAL(0, rte_service_component_register(&service, &id),
			"Register of MT SAFE service failed");

	const uint32_t sid = 0;
	TEST_ASSERT_EQUAL(0, rte_service_runstate_set(sid, 1),
			"Starting valid service failed");
	TEST_ASSERT_EQUAL(0, rte_service_map_lcore_set(sid, slcore_1, 1),
			"Failed to enable lcore 1 on mt safe service");
	TEST_ASSERT_EQUAL(0, rte_service_map_lcore_set(sid, slcore_2, 1),
			"Failed to enable lcore 2 on mt safe service");
	rte_service_lcore_start(slcore_1);
	rte_service_lcore_start(slcore_2);

	/* wait for the worker threads to run */
	rte_delay_ms(500);
	rte_service_lcore_stop(slcore_1);
	rte_service_lcore_stop(slcore_2);

	TEST_ASSERT_EQUAL(0, test_params[1],
			"Service run with component runstate = 0");

	/* enable backend runstate: the service should run after this */
	rte_service_component_runstate_set(id, 1);

	/* initialize to pass, see callback comment for details */
	if (!mt_safe)
		test_params[1] = 1;

	/* wait for lcores before start() */
	rte_eal_wait_lcore(slcore_1);
	rte_eal_wait_lcore(slcore_2);

	rte_service_lcore_start(slcore_1);
	rte_service_lcore_start(slcore_2);

	/* wait for the worker threads to run */
	rte_delay_ms(500);
	rte_service_lcore_stop(slcore_1);
	rte_service_lcore_stop(slcore_2);

	TEST_ASSERT_EQUAL(1, test_params[1],
			"MT Safe service not run by two cores concurrently");
	TEST_ASSERT_EQUAL(0, rte_service_runstate_set(sid, 0),
			"Failed to stop MT Safe service");

	rte_eal_wait_lcore(slcore_1);
	rte_eal_wait_lcore(slcore_2);
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

	if (!rte_lcore_is_enabled(0) || !rte_lcore_is_enabled(1) ||
	    !rte_lcore_is_enabled(2))
		return TEST_SKIPPED;

	TEST_ASSERT_EQUAL(1, service_threaded_test(mt_safe),
			"Error: MT Safe service not run by two cores concurrently");
	return TEST_SUCCESS;
}

/* tests a NON mt safe service with two cores, the callback is serialized
 * using the CAS.
 */
static int
service_mt_unsafe_poll(void)
{
	int mt_safe = 0;

	if (!rte_lcore_is_enabled(0) || !rte_lcore_is_enabled(1) ||
	    !rte_lcore_is_enabled(2))
		return TEST_SKIPPED;

	TEST_ASSERT_EQUAL(1, service_threaded_test(mt_safe),
			"Error: NON MT Safe service run by two cores concurrently");
	return TEST_SUCCESS;
}

static int32_t
delay_as_a_mt_safe_service(void *args)
{
	RTE_SET_USED(args);
	uint32_t *params = args;

	/* retrieve done flag and lock to add/sub */
	uint32_t *done = &params[0];
	uint32_t *lock = &params[1];

	while (!*done) {
		__atomic_add_fetch(lock, 1, __ATOMIC_RELAXED);
		rte_delay_us(500);
		if (__atomic_load_n(lock, __ATOMIC_RELAXED) > 1)
			/* pass: second core has simultaneously incremented */
			*done = 1;
		__atomic_sub_fetch(lock, 1, __ATOMIC_RELAXED);
	}

	return 0;
}

static int32_t
delay_as_a_service(void *args)
{
	uint32_t *done = (uint32_t *)args;
	while (!*done)
		rte_delay_ms(5);
	return 0;
}

static int
service_run_on_app_core_func(void *arg)
{
	uint32_t *delay_service_id = (uint32_t *)arg;
	return rte_service_run_iter_on_app_lcore(*delay_service_id, 1);
}

static int
service_app_lcore_poll_impl(const int mt_safe)
{
	uint32_t params[2] = {0};

	struct rte_service_spec service;
	memset(&service, 0, sizeof(struct rte_service_spec));
	snprintf(service.name, sizeof(service.name), MT_SAFE_SERVICE_NAME);
	if (mt_safe) {
		service.callback = delay_as_a_mt_safe_service;
		service.callback_userdata = params;
		service.capabilities |= RTE_SERVICE_CAP_MT_SAFE;
	} else {
		service.callback = delay_as_a_service;
		service.callback_userdata = &params;
	}

	uint32_t id;
	TEST_ASSERT_EQUAL(0, rte_service_component_register(&service, &id),
			"Register of app lcore delay service failed");

	rte_service_component_runstate_set(id, 1);
	rte_service_runstate_set(id, 1);

	uint32_t app_core2 = rte_get_next_lcore(slcore_id, 1, 1);
	rte_eal_wait_lcore(app_core2);
	int app_core2_ret = rte_eal_remote_launch(service_run_on_app_core_func,
						  &id, app_core2);

	rte_delay_ms(100);

	int app_core1_ret = service_run_on_app_core_func(&id);

	/* flag done, then wait for the spawned 2nd core to return */
	params[0] = 1;
	rte_eal_mp_wait_lcore();

	/* core two gets launched first - and should hold the service lock */
	TEST_ASSERT_EQUAL(0, app_core2_ret,
			"App core2 : run service didn't return zero");

	if (mt_safe) {
		/* mt safe should have both cores return 0 for success */
		TEST_ASSERT_EQUAL(0, app_core1_ret,
				"MT Safe: App core1 didn't return 0");
	} else {
		/* core one attempts to run later - should be blocked */
		TEST_ASSERT_EQUAL(-EBUSY, app_core1_ret,
				"MT Unsafe: App core1 didn't return -EBUSY");
	}

	/* Performance test: call in a loop, and measure tsc() */
	const uint32_t perf_iters = (1 << 12);
	uint64_t start = rte_rdtsc();
	uint32_t i;
	for (i = 0; i < perf_iters; i++) {
		int err = service_run_on_app_core_func(&id);
		TEST_ASSERT_EQUAL(0, err, "perf test: returned run failure");
	}
	uint64_t end = rte_rdtsc();
	printf("perf test for %s: %0.1f cycles per call\n", mt_safe ?
		"MT Safe" : "MT Unsafe", (end - start)/(float)perf_iters);

	unregister_all();
	return TEST_SUCCESS;
}

static int
service_app_lcore_mt_safe(void)
{
	const int mt_safe = 1;
	return service_app_lcore_poll_impl(mt_safe);
}

static int
service_app_lcore_mt_unsafe(void)
{
	const int mt_safe = 0;
	return service_app_lcore_poll_impl(mt_safe);
}

/* start and stop a service core - ensuring it goes back to sleep */
static int
service_lcore_start_stop(void)
{
	/* start service core and service, create mapping so tick() runs */
	const uint32_t sid = 0;
	TEST_ASSERT_EQUAL(0, rte_service_runstate_set(sid, 1),
			"Starting valid service failed");
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_map_lcore_set(sid, slcore_id, 1),
			"Enabling valid service on non-service core must fail");

	/* core start */
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_lcore_start(slcore_id),
			"Service core start without add should return EINVAL");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_id),
			"Service core add did not return zero");
	TEST_ASSERT_EQUAL(0, rte_service_map_lcore_set(sid, slcore_id, 1),
			"Enabling valid service on valid core failed");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_start(slcore_id),
			"Service core start after add failed");
	TEST_ASSERT_EQUAL(-EALREADY, rte_service_lcore_start(slcore_id),
			"Service core expected as running but was stopped");

	/* ensures core really is running the service function */
	TEST_ASSERT_EQUAL(1, service_lcore_running_check(),
			"Service core expected to poll service but it didn't");

	/* core stop */
	TEST_ASSERT_EQUAL(-EBUSY, rte_service_lcore_stop(slcore_id),
			"Service core running a service should return -EBUSY");
	TEST_ASSERT_EQUAL(0, rte_service_runstate_set(sid, 0),
			"Stopping valid service failed");
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

/* stop a service and wait for it to become inactive */
static int
service_may_be_active(void)
{
	const uint32_t sid = 0;
	int i;

	/* expected failure cases */
	TEST_ASSERT_EQUAL(-EINVAL, rte_service_may_be_active(10000),
			"Invalid service may be active check did not fail");

	/* start the service */
	TEST_ASSERT_EQUAL(0, rte_service_runstate_set(sid, 1),
			"Starting valid service failed");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore_id),
			"Add service core failed when not in use before");
	TEST_ASSERT_EQUAL(0, rte_service_map_lcore_set(sid, slcore_id, 1),
			"Enabling valid service on valid core failed");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_start(slcore_id),
			"Service core start after add failed");

	/* ensures core really is running the service function */
	TEST_ASSERT_EQUAL(1, service_lcore_running_check(),
			"Service core expected to poll service but it didn't");

	/* stop the service */
	TEST_ASSERT_EQUAL(0, rte_service_runstate_set(sid, 0),
			"Error: Service stop returned non-zero");

	/* give the service 100ms to stop running */
	for (i = 0; i < 100; i++) {
		if (!rte_service_may_be_active(sid))
			break;
		rte_delay_ms(SERVICE_DELAY);
	}

	TEST_ASSERT_EQUAL(0, rte_service_may_be_active(sid),
			  "Error: Service not stopped after 100ms");

	return unregister_all();
}

/* check service may be active when service is running on a second lcore */
static int
service_active_two_cores(void)
{
	if (!rte_lcore_is_enabled(0) || !rte_lcore_is_enabled(1) ||
	    !rte_lcore_is_enabled(2))
		return TEST_SKIPPED;

	const uint32_t sid = 0;
	int i;

	uint32_t lcore = rte_get_next_lcore(/* start core */ -1,
					    /* skip main */ 1,
					    /* wrap */ 0);
	uint32_t slcore = rte_get_next_lcore(/* start core */ lcore,
					     /* skip main */ 1,
					     /* wrap */ 0);

	/* start the service on the second available lcore */
	TEST_ASSERT_EQUAL(0, rte_service_runstate_set(sid, 1),
			"Starting valid service failed");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_add(slcore),
			"Add service core failed when not in use before");
	TEST_ASSERT_EQUAL(0, rte_service_map_lcore_set(sid, slcore, 1),
			"Enabling valid service on valid core failed");
	TEST_ASSERT_EQUAL(0, rte_service_lcore_start(slcore),
			"Service core start after add failed");

	/* ensures core really is running the service function */
	TEST_ASSERT_EQUAL(1, service_lcore_running_check(),
			"Service core expected to poll service but it didn't");

	/* ensures that service may be active reports running state */
	TEST_ASSERT_EQUAL(1, rte_service_may_be_active(sid),
			"Service may be active did not report running state");

	/* stop the service */
	TEST_ASSERT_EQUAL(0, rte_service_runstate_set(sid, 0),
			"Error: Service stop returned non-zero");

	/* give the service 100ms to stop running */
	for (i = 0; i < 100; i++) {
		if (!rte_service_may_be_active(sid))
			break;
		rte_delay_ms(SERVICE_DELAY);
	}

	TEST_ASSERT_EQUAL(0, rte_service_may_be_active(sid),
			  "Error: Service not stopped after 100ms");

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
		TEST_CASE_ST(dummy_register, NULL, service_attr_get),
		TEST_CASE_ST(dummy_register, NULL, service_lcore_attr_get),
		TEST_CASE_ST(dummy_register, NULL, service_probe_capability),
		TEST_CASE_ST(dummy_register, NULL, service_start_stop),
		TEST_CASE_ST(dummy_register, NULL, service_lcore_add_del),
		TEST_CASE_ST(dummy_register, NULL, service_lcore_start_stop),
		TEST_CASE_ST(dummy_register, NULL, service_lcore_en_dis_able),
		TEST_CASE_ST(dummy_register, NULL, service_mt_unsafe_poll),
		TEST_CASE_ST(dummy_register, NULL, service_mt_safe_poll),
		TEST_CASE_ST(dummy_register, NULL, service_app_lcore_mt_safe),
		TEST_CASE_ST(dummy_register, NULL, service_app_lcore_mt_unsafe),
		TEST_CASE_ST(dummy_register, NULL, service_may_be_active),
		TEST_CASE_ST(dummy_register, NULL, service_active_two_cores),
		TEST_CASES_END() /**< NULL terminate unit test array */
	}
};

static int
test_service_common(void)
{
	return unit_test_suite_runner(&service_tests);
}

REGISTER_TEST_COMMAND(service_autotest, test_service_common);
