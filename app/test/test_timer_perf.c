/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include "test.h"

#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <rte_cycles.h>
#include <rte_timer.h>
#include <rte_common.h>
#include <rte_lcore.h>
#include <rte_random.h>
#include <rte_malloc.h>
#include <rte_pause.h>

#define MAX_ITERATIONS 1000000

int outstanding_count = 0;

static void
timer_cb(struct rte_timer *t __rte_unused, void *param __rte_unused)
{
	outstanding_count--;
}

#define DELAY_SECONDS 1

#ifdef RTE_EXEC_ENV_LINUXAPP
#define do_delay() usleep(10)
#else
#define do_delay() rte_pause()
#endif

static int
test_timer_perf(void)
{
	unsigned iterations = 100;
	unsigned i;
	struct rte_timer *tms;
	uint64_t start_tsc, end_tsc, delay_start;
	unsigned lcore_id = rte_lcore_id();

	tms = rte_malloc(NULL, sizeof(*tms) * MAX_ITERATIONS, 0);

	for (i = 0; i < MAX_ITERATIONS; i++)
		rte_timer_init(&tms[i]);

	const uint64_t ticks = rte_get_timer_hz() * DELAY_SECONDS;
	const uint64_t ticks_per_ms = rte_get_tsc_hz()/1000;
	const uint64_t ticks_per_us = ticks_per_ms/1000;

	while (iterations <= MAX_ITERATIONS) {

		printf("Appending %u timers\n", iterations);
		start_tsc = rte_rdtsc();
		for (i = 0; i < iterations; i++)
			rte_timer_reset(&tms[i], ticks, SINGLE, lcore_id,
					timer_cb, NULL);
		end_tsc = rte_rdtsc();
		printf("Time for %u timers: %"PRIu64" (%"PRIu64"ms), ", iterations,
				end_tsc-start_tsc, (end_tsc-start_tsc+ticks_per_ms/2)/(ticks_per_ms));
		printf("Time per timer: %"PRIu64" (%"PRIu64"us)\n",
				(end_tsc-start_tsc)/iterations,
				((end_tsc-start_tsc)/iterations+ticks_per_us/2)/(ticks_per_us));
		outstanding_count = iterations;
		delay_start = rte_get_timer_cycles();
		while (rte_get_timer_cycles() < delay_start + ticks)
			do_delay();

		start_tsc = rte_rdtsc();
		while (outstanding_count)
			rte_timer_manage();
		end_tsc = rte_rdtsc();
		printf("Time for %u callbacks: %"PRIu64" (%"PRIu64"ms), ", iterations,
				end_tsc-start_tsc, (end_tsc-start_tsc+ticks_per_ms/2)/(ticks_per_ms));
		printf("Time per callback: %"PRIu64" (%"PRIu64"us)\n",
				(end_tsc-start_tsc)/iterations,
				((end_tsc-start_tsc)/iterations+ticks_per_us/2)/(ticks_per_us));

		printf("Resetting %u timers\n", iterations);
		start_tsc = rte_rdtsc();
		for (i = 0; i < iterations; i++)
			rte_timer_reset(&tms[i], rte_rand() % ticks, SINGLE, lcore_id,
					timer_cb, NULL);
		end_tsc = rte_rdtsc();
		printf("Time for %u timers: %"PRIu64" (%"PRIu64"ms), ", iterations,
				end_tsc-start_tsc, (end_tsc-start_tsc+ticks_per_ms/2)/(ticks_per_ms));
		printf("Time per timer: %"PRIu64" (%"PRIu64"us)\n",
				(end_tsc-start_tsc)/iterations,
				((end_tsc-start_tsc)/iterations+ticks_per_us/2)/(ticks_per_us));
		outstanding_count = iterations;

		delay_start = rte_get_timer_cycles();
		while (rte_get_timer_cycles() < delay_start + ticks)
			do_delay();

		rte_timer_manage();
		if (outstanding_count != 0) {
			printf("Error: outstanding callback count = %d\n", outstanding_count);
			return -1;
		}

		iterations *= 10;
		printf("\n");
	}

	printf("All timers processed ok\n");

	/* measure time to poll an empty timer list */
	start_tsc = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		rte_timer_manage();
	end_tsc = rte_rdtsc();
	printf("\nTime per rte_timer_manage with zero timers: %"PRIu64" cycles\n",
			(end_tsc - start_tsc + iterations/2) / iterations);

	/* measure time to poll a timer list with timers, but without
	 * calling any callbacks */
	rte_timer_reset(&tms[0], ticks * 100, SINGLE, lcore_id,
			timer_cb, NULL);
	start_tsc = rte_rdtsc();
	for (i = 0; i < iterations; i++)
		rte_timer_manage();
	end_tsc = rte_rdtsc();
	printf("Time per rte_timer_manage with zero callbacks: %"PRIu64" cycles\n",
			(end_tsc - start_tsc + iterations/2) / iterations);

	rte_free(tms);
	return 0;
}

REGISTER_TEST_COMMAND(timer_perf_autotest, test_timer_perf);
