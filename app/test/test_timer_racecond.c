/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Akamai Technologies.
 * All rights reserved.
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

#ifdef RTE_EXEC_ENV_LINUX
#define usec_delay(us) usleep(us)
#else
#define usec_delay(us) rte_delay_us(us)
#endif

#define BILLION (1UL << 30)

#define TEST_DURATION_S 4 /* in seconds */
#define N_TIMERS    50

static struct rte_timer timer[N_TIMERS];
static unsigned int timer_lcore_id[N_TIMERS];

static unsigned int main_lcore;
static volatile unsigned int stop_workers;

static int reload_timer(struct rte_timer *tim);

RTE_LOG_REGISTER(timer_logtype_test, test.timer, INFO);

static void
timer_cb(struct rte_timer *tim, void *arg __rte_unused)
{
	/* Simulate slow callback function, 100 us. */
	rte_delay_us(100);
	if (tim == &timer[0])
		rte_log(RTE_LOG_DEBUG, timer_logtype_test,
			"------------------------------------------------\n");
	rte_log(RTE_LOG_DEBUG, timer_logtype_test, "%s: core %u timer %"
		PRIuPTR "\n", __func__, rte_lcore_id(), tim - timer);
	(void)reload_timer(tim);
}

RTE_DEFINE_PER_LCORE(unsigned, n_reset_collisions);

static int
reload_timer(struct rte_timer *tim)
{
	/* Make timer expire roughly when the TSC hits the next BILLION
	 * multiple. Add in timer's index to make them expire in nearly
	 * sorted order. This makes all timers somewhat synchronized,
	 * firing ~2-3 times per second, assuming 2-3 GHz TSCs.
	 */
	uint64_t ticks = BILLION - (rte_get_timer_cycles() % BILLION) +
	    (tim - timer);
	int ret;

	ret = rte_timer_reset(tim, ticks, PERIODICAL, main_lcore, timer_cb, NULL);
	if (ret != 0) {
		rte_log(RTE_LOG_DEBUG, timer_logtype_test,
			"- core %u failed to reset timer %" PRIuPTR " (OK)\n",
			rte_lcore_id(), tim - timer);
		RTE_PER_LCORE(n_reset_collisions) += 1;
	}
	return ret;
}

static int
worker_main_loop(__rte_unused void *arg)
{
	unsigned lcore_id = rte_lcore_id();
	unsigned i;

	RTE_PER_LCORE(n_reset_collisions) = 0;

	printf("Starting main loop on core %u\n", lcore_id);

	while (!stop_workers) {
		/* Wait until the timer manager is running.
		 * We know it's running when we see timer[0] NOT pending.
		 */
		if (rte_timer_pending(&timer[0])) {
			rte_pause();
			continue;
		}

		/* Now, go cause some havoc!
		 * Reload our timers.
		 */
		for (i = 0; i < N_TIMERS; i++) {
			if (timer_lcore_id[i] == lcore_id)
				(void)reload_timer(&timer[i]);
		}
		usec_delay(100*1000); /* sleep 100 ms */
	}

	if (RTE_PER_LCORE(n_reset_collisions) != 0) {
		printf("- core %u, %u reset collisions (OK)\n",
			lcore_id, RTE_PER_LCORE(n_reset_collisions));
	}
	return 0;
}

static int
test_timer_racecond(void)
{
	int ret;
	uint64_t hz;
	uint64_t cur_time;
	uint64_t end_time;
	int64_t diff = 0;
	unsigned lcore_id;
	unsigned i;

	main_lcore = lcore_id = rte_lcore_id();
	hz = rte_get_timer_hz();

	/* init and start timers */
	for (i = 0; i < N_TIMERS; i++) {
		rte_timer_init(&timer[i]);
		ret = reload_timer(&timer[i]);
		TEST_ASSERT(ret == 0, "reload_timer failed");

		/* Distribute timers to workers.
		 * Note that we assign timer[0] to the main.
		 */
		timer_lcore_id[i] = lcore_id;
		lcore_id = rte_get_next_lcore(lcore_id, 1, 1);
	}

	/* calculate the "end of test" time */
	cur_time = rte_get_timer_cycles();
	end_time = cur_time + (hz * TEST_DURATION_S);

	/* start worker cores */
	stop_workers = 0;
	printf("Start timer manage race condition test (%u seconds)\n",
			TEST_DURATION_S);
	rte_eal_mp_remote_launch(worker_main_loop, NULL, SKIP_MAIN);

	while (diff >= 0) {
		/* run the timers */
		rte_timer_manage();

		/* wait 100 ms */
		usec_delay(100*1000);

		cur_time = rte_get_timer_cycles();
		diff = end_time - cur_time;
	}

	/* stop worker cores */
	printf("Stopping timer manage race condition test\n");
	stop_workers = 1;
	rte_eal_mp_wait_lcore();

	/* stop timers */
	for (i = 0; i < N_TIMERS; i++) {
		ret = rte_timer_stop(&timer[i]);
		TEST_ASSERT(ret == 0, "rte_timer_stop failed");
	}

	return TEST_SUCCESS;
}

REGISTER_PERF_TEST(timer_racecond_autotest, test_timer_racecond);
