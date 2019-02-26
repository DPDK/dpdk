/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_interrupts.h>
#include <rte_atomic.h>
#include <rte_alarm.h>

#include "test.h"

#define US_PER_MS 1000

#define RTE_TEST_ALARM_TIMEOUT 10 /* ms */
#define RTE_TEST_CHECK_PERIOD   3 /* ms */
#define RTE_TEST_MAX_REPEAT    20

static volatile int flag;

static void
test_alarm_callback(void *cb_arg)
{
	flag = 1;
	printf("Callback setting flag - OK. [cb_arg = %p]\n", cb_arg);
}

static rte_atomic32_t cb_count;

static void
test_multi_cb(void *arg)
{
	rte_atomic32_inc(&cb_count);
	printf("In %s - arg = %p\n", __func__, arg);
}

static volatile int recursive_error = 0;

static void
test_remove_in_callback(void *arg)
{
	printf("In %s - arg = %p\n", __func__, arg);
	if (rte_eal_alarm_cancel(test_remove_in_callback, arg) ||
			rte_eal_alarm_cancel(test_remove_in_callback, (void *)-1)) {
		printf("Error - cancelling callback from within function succeeded!\n");
		recursive_error = 1;
	}
	flag = (int)((uintptr_t)arg);
}

static volatile int flag_2;

static void
test_remove_in_callback_2(void *arg)
{
	if (rte_eal_alarm_cancel(test_remove_in_callback_2, arg) || rte_eal_alarm_cancel(test_remove_in_callback_2, (void *)-1)) {
		printf("Error - cancelling callback of test_remove_in_callback_2\n");
		return;
	}
	flag_2 = 1;
}

static int
test_multi_alarms(void)
{
	int rm_count = 0;
	int count = 0;
	cb_count.cnt = 0;

	printf("Expect 6 callbacks in order...\n");
	/* add two alarms in order */
	rte_eal_alarm_set(10 * US_PER_MS, test_multi_cb, (void *)1);
	rte_eal_alarm_set(20 * US_PER_MS, test_multi_cb, (void *)2);

	/* now add in reverse order */
	rte_eal_alarm_set(60 * US_PER_MS, test_multi_cb, (void *)6);
	rte_eal_alarm_set(50 * US_PER_MS, test_multi_cb, (void *)5);
	rte_eal_alarm_set(40 * US_PER_MS, test_multi_cb, (void *)4);
	rte_eal_alarm_set(30 * US_PER_MS, test_multi_cb, (void *)3);

	/* wait for expiry */
	rte_delay_ms(65);
	if (cb_count.cnt != 6) {
		printf("Missing callbacks\n");
		/* remove any callbacks that might remain */
		rte_eal_alarm_cancel(test_multi_cb, (void *)-1);
		return -1;
	}

	cb_count.cnt = 0;
	printf("Expect only callbacks with args 1 and 3...\n");
	/* Add 3 flags, then delete one */
	rte_eal_alarm_set(30 * US_PER_MS, test_multi_cb, (void *)3);
	rte_eal_alarm_set(20 * US_PER_MS, test_multi_cb, (void *)2);
	rte_eal_alarm_set(10 * US_PER_MS, test_multi_cb, (void *)1);
	rm_count = rte_eal_alarm_cancel(test_multi_cb, (void *)2);

	rte_delay_ms(35);
	if (cb_count.cnt != 2 || rm_count != 1) {
		printf("Error: invalid flags count or alarm removal failure"
				" -  flags value = %d, expected = %d\n",
				(int)cb_count.cnt, 2);
		/* remove any callbacks that might remain */
		rte_eal_alarm_cancel(test_multi_cb, (void *)-1);
		return -1;
	}

	printf("Testing adding and then removing multiple alarms\n");
	/* finally test that no callbacks are called if we delete them all*/
	rte_eal_alarm_set(10 * US_PER_MS, test_multi_cb, (void *)1);
	rte_eal_alarm_set(10 * US_PER_MS, test_multi_cb, (void *)2);
	rte_eal_alarm_set(10 * US_PER_MS, test_multi_cb, (void *)3);
	rm_count = rte_eal_alarm_cancel(test_alarm_callback, (void *)-1);
	if (rm_count != 0) {
		printf("Error removing non-existant alarm succeeded\n");
		rte_eal_alarm_cancel(test_multi_cb, (void *) -1);
		return -1;
	}
	rm_count = rte_eal_alarm_cancel(test_multi_cb, (void *) -1);
	if (rm_count != 3) {
		printf("Error removing all pending alarm callbacks\n");
		return -1;
	}

	/* Test that we cannot cancel an alarm from within the callback itself
	 * Also test that we can cancel head-of-line callbacks ok.*/
	flag = 0;
	recursive_error = 0;
	rte_eal_alarm_set(10 * US_PER_MS, test_remove_in_callback, (void *)1);
	rte_eal_alarm_set(20 * US_PER_MS, test_remove_in_callback, (void *)2);
	rm_count = rte_eal_alarm_cancel(test_remove_in_callback, (void *)1);
	if (rm_count != 1) {
		printf("Error cancelling head-of-list callback\n");
		return -1;
	}
	rte_delay_ms(15);
	if (flag != 0) {
		printf("Error, cancelling head-of-list leads to premature callback\n");
		return -1;
	}

	while (flag != 2 && count++ < RTE_TEST_MAX_REPEAT)
		rte_delay_ms(10);

	if (flag != 2) {
		printf("Error - expected callback not called\n");
		rte_eal_alarm_cancel(test_remove_in_callback, (void *)-1);
		return -1;
	}
	if (recursive_error == 1)
		return -1;

	/* Check if it can cancel all for the same callback */
	printf("Testing canceling all for the same callback\n");
	flag_2 = 0;
	rte_eal_alarm_set(10 * US_PER_MS, test_remove_in_callback, (void *)1);
	rte_eal_alarm_set(20 * US_PER_MS, test_remove_in_callback_2, (void *)2);
	rte_eal_alarm_set(30 * US_PER_MS, test_remove_in_callback_2, (void *)3);
	rte_eal_alarm_set(40 * US_PER_MS, test_remove_in_callback, (void *)4);
	rm_count = rte_eal_alarm_cancel(test_remove_in_callback_2, (void *)-1);
	if (rm_count != 2) {
		printf("Error, cannot cancel all for the same callback\n");
		return -1;
	}
	rm_count = rte_eal_alarm_cancel(test_remove_in_callback, (void *)-1);
	if (rm_count != 2) {
		printf("Error, cannot cancel all for the same callback\n");
		return -1;
	}

	return 0;
}

static int
test_alarm(void)
{
	int count = 0;
#ifdef RTE_EXEC_ENV_BSDAPP
	printf("The alarm API is not supported on FreeBSD\n");
	return 0;
#endif
	/* check if the callback will be called */
	printf("check if the callback will be called\n");
	flag = 0;
	if (rte_eal_alarm_set(RTE_TEST_ALARM_TIMEOUT * US_PER_MS,
			test_alarm_callback, NULL) < 0) {
		printf("fail to set alarm callback\n");
		return -1;
	}
	while (flag == 0 && count++ < RTE_TEST_MAX_REPEAT)
		rte_delay_ms(RTE_TEST_CHECK_PERIOD);

	if (flag == 0){
		printf("Callback not called\n");
		return -1;
	}

	/* check if it will fail to set alarm with wrong us value */
	printf("check if it will fail to set alarm with wrong ms values\n");
	if (rte_eal_alarm_set(0, test_alarm_callback,
						NULL) >= 0) {
		printf("should not be successful with 0 us value\n");
		return -1;
	}
	if (rte_eal_alarm_set(UINT64_MAX - 1, test_alarm_callback,
						NULL) >= 0) {
		printf("should not be successful with (UINT64_MAX-1) us value\n");
		return -1;
	}

	/* check if it will fail to set alarm with null callback parameter */
	printf("check if it will fail to set alarm with null callback parameter\n");
	if (rte_eal_alarm_set(RTE_TEST_ALARM_TIMEOUT, NULL, NULL) >= 0) {
		printf("should not be successful to set alarm with null callback parameter\n");
		return -1;
	}

	/* check if it will fail to remove alarm with null callback parameter */
	printf("check if it will fail to remove alarm with null callback parameter\n");
	if (rte_eal_alarm_cancel(NULL, NULL) == 0) {
		printf("should not be successful to remove alarm with null callback parameter");
		return -1;
	}

	if (test_multi_alarms() != 0)
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(alarm_autotest, test_alarm);
