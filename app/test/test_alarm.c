/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include <rte_alarm.h>
#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_errno.h>

#include "test.h"

#ifndef US_PER_SEC
#define US_PER_SEC	1000000u
#endif

static void
test_alarm_callback(void *cb_arg)
{
	RTE_ATOMIC(bool) *triggered = cb_arg;

	rte_atomic_store_explicit(triggered, 1, rte_memory_order_release);
}


static int
test_alarm(void)
{
	RTE_ATOMIC(bool) triggered = false;
	RTE_ATOMIC(bool) later = false;
	int ret;

	/* check if it will fail to set alarm with wrong us values */
	TEST_ASSERT_FAIL(rte_eal_alarm_set(0, test_alarm_callback, NULL),
			 "Expected rte_eal_alarm_set to fail with 0 us value");

	/* check it if will fail with a very large timeout value */
	TEST_ASSERT_FAIL(rte_eal_alarm_set(UINT64_MAX - 1, test_alarm_callback, NULL),
			 "Expected rte_eal_alarm_set to fail with (UINT64_MAX-1) us value");

	/* check if it will fail to set alarm with null callback parameter */
	TEST_ASSERT_FAIL(rte_eal_alarm_set(US_PER_SEC, NULL, NULL),
			 "Expected rte_eal_alarm_set to fail with null callback parameter");

	/* check if it will fail to remove alarm with null callback parameter */
	TEST_ASSERT_FAIL(rte_eal_alarm_cancel(NULL, NULL),
			 "Expected rte_eal_alarm_cancel to fail with null callback parameter");

	/* check if can set a alarm for one second */
	TEST_ASSERT_SUCCESS(rte_eal_alarm_set(US_PER_SEC, test_alarm_callback,
			    RTE_PTR_UNQUAL(&triggered)), "Setting one second alarm failed");

	/* set a longer alarm that will be canceled. */
	TEST_ASSERT_SUCCESS(rte_eal_alarm_set(10 * US_PER_SEC, test_alarm_callback,
			    RTE_PTR_UNQUAL(&later)), "Setting ten second alarm failed");

	/* wait for alarm to happen */
	while (rte_atomic_load_explicit(&triggered, rte_memory_order_acquire) == false)
		rte_delay_us_sleep(US_PER_SEC / 10);

	TEST_ASSERT(!rte_atomic_load_explicit(&later, rte_memory_order_acquire),
		    "Only one alarm should have fired.");

	ret = rte_eal_alarm_cancel(test_alarm_callback, RTE_PTR_UNQUAL(&triggered));
	TEST_ASSERT(ret == 0 && rte_errno == ENOENT,
		    "Canceling alarm after run ret %d: %s", ret, rte_strerror(rte_errno));

	ret = rte_eal_alarm_cancel(test_alarm_callback, RTE_PTR_UNQUAL(&later));
	TEST_ASSERT(ret == 1, "Canceling ten second alarm failed %d: %s",
		    ret, rte_strerror(rte_errno));

	return 0;
}

REGISTER_FAST_TEST(alarm_autotest, NOHUGE_OK, ASAN_OK, test_alarm);
