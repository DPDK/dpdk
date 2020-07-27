/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_cycles.h>

#include "test.h"

#define N 10000

/*
 * Cycles test
 * ===========
 *
 * - Loop N times and check that the timer always increments and
 *   never decrements during this loop.
 *
 * - Wait one second using rte_usleep() and check that the increment
 *   of cycles is correct with regard to the frequency of the timer.
 */

static int
check_wait_one_second(void)
{
	uint64_t cycles, prev_cycles;
	uint64_t hz = rte_get_timer_hz();
	uint64_t max_inc = (hz / 100); /* 10 ms max between 2 reads */

	/* check that waiting 1 second is precise */
	prev_cycles = rte_get_timer_cycles();
	rte_delay_us(1000000);
	cycles = rte_get_timer_cycles();

	if ((uint64_t)(cycles - prev_cycles) > (hz + max_inc)) {
		printf("delay_us is not accurate: too long\n");
		return -1;
	}
	if ((uint64_t)(cycles - prev_cycles) < (hz - max_inc)) {
		printf("delay_us is not accurate: too short\n");
		return -1;
	}

	return 0;
}

static int
test_cycles(void)
{
	unsigned i;
	uint64_t start_cycles, cycles, prev_cycles;
	uint64_t hz = rte_get_timer_hz();
	uint64_t max_inc = (hz / 100); /* 10 ms max between 2 reads */

	/* check that the timer is always incrementing */
	start_cycles = rte_get_timer_cycles();
	prev_cycles = start_cycles;
	for (i=0; i<N; i++) {
		cycles = rte_get_timer_cycles();
		if ((uint64_t)(cycles - prev_cycles) > max_inc) {
			printf("increment too high or going backwards\n");
			return -1;
		}
		prev_cycles = cycles;
	}

	return check_wait_one_second();
}

REGISTER_TEST_COMMAND(cycles_autotest, test_cycles);

/*
 * One second precision test with rte_delay_us_sleep.
 */

static int
test_delay_us_sleep(void)
{
	int rv;

	rte_delay_us_callback_register(rte_delay_us_sleep);
	rv = check_wait_one_second();
	/* restore original delay function */
	rte_delay_us_callback_register(rte_delay_us_block);

	return rv;
}

REGISTER_TEST_COMMAND(delay_us_sleep_autotest, test_delay_us_sleep);

/*
 * rte_delay_us_callback test
 *
 * - check if callback is correctly registered/unregistered
 *
 */

static unsigned int pattern;
static void my_rte_delay_us(unsigned int us)
{
	pattern += us;
}

static int
test_user_delay_us(void)
{
	pattern = 0;

	rte_delay_us(2);
	if (pattern != 0)
		return -1;

	/* register custom delay function */
	rte_delay_us_callback_register(my_rte_delay_us);

	rte_delay_us(2);
	if (pattern != 2)
		return -1;

	rte_delay_us(3);
	if (pattern != 5)
		return -1;

	/* restore original delay function */
	rte_delay_us_callback_register(rte_delay_us_block);

	rte_delay_us(3);
	if (pattern != 5)
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(user_delay_us, test_user_delay_us);
