/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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

#include "test.h"

/*
 * Timer
 * =====
 *
 * #. Stress test 1.
 *
 *    The objective of the timer stress tests is to check that there are no
 *    race conditions in list and status management. This test launches,
 *    resets and stops the timer very often on many cores at the same
 *    time.
 *
 *    - Only one timer is used for this test.
 *    - On each core, the rte_timer_manage() function is called from the main
 *      loop every 3 microseconds.
 *    - In the main loop, the timer may be reset (randomly, with a
 *      probability of 0.5 %) 100 microseconds later on a random core, or
 *      stopped (with a probability of 0.5 % also).
 *    - In callback, the timer is can be reset (randomly, with a
 *      probability of 0.5 %) 100 microseconds later on the same core or
 *      on another core (same probability), or stopped (same
 *      probability).
 *
 * # Stress test 2.
 *
 *    The objective of this test is similar to the first in that it attempts
 *    to find if there are any race conditions in the timer library. However,
 *    it is less complex in terms of operations performed and duration, as it
 *    is designed to have a predictable outcome that can be tested.
 *
 *    - A set of timers is initialized for use by the test
 *    - All cores then simultaneously are set to schedule all the timers at
 *      the same time, so conflicts should occur.
 *    - Then there is a delay while we wait for the timers to expire
 *    - Then the master lcore calls timer_manage() and we check that all
 *      timers have had their callbacks called exactly once - no more no less.
 *    - Then we repeat the process, except after setting up the timers, we have
 *      all cores randomly reschedule them.
 *    - Again we check that the expected number of callbacks has occurred when
 *      we call timer-manage.
 *
 * #. Basic test.
 *
 *    This test performs basic functional checks of the timers. The test
 *    uses four different timers that are loaded and stopped under
 *    specific conditions in specific contexts.
 *
 *    - Four timers are used for this test.
 *    - On each core, the rte_timer_manage() function is called from main loop
 *      every 3 microseconds.
 *
 *    The autotest python script checks that the behavior is correct:
 *
 *    - timer0
 *
 *      - At initialization, timer0 is loaded by the master core, on master core
 *        in "single" mode (time = 1 second).
 *      - In the first 19 callbacks, timer0 is reloaded on the same core,
 *        then, it is explicitly stopped at the 20th call.
 *      - At t=25s, timer0 is reloaded once by timer2.
 *
 *    - timer1
 *
 *      - At initialization, timer1 is loaded by the master core, on the
 *        master core in "single" mode (time = 2 seconds).
 *      - In the first 9 callbacks, timer1 is reloaded on another
 *        core. After the 10th callback, timer1 is not reloaded anymore.
 *
 *    - timer2
 *
 *      - At initialization, timer2 is loaded by the master core, on the
 *        master core in "periodical" mode (time = 1 second).
 *      - In the callback, when t=25s, it stops timer3 and reloads timer0
 *        on the current core.
 *
 *    - timer3
 *
 *      - At initialization, timer3 is loaded by the master core, on
 *        another core in "periodical" mode (time = 1 second).
 *      - It is stopped at t=25s by timer2.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <math.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_timer.h>
#include <rte_random.h>
#include <rte_malloc.h>
#include <rte_pause.h>

#define TEST_DURATION_S 1 /* in seconds */
#define NB_TIMER 4

#define RTE_LOGTYPE_TESTTIMER RTE_LOGTYPE_USER3

static volatile uint64_t end_time;
static volatile int test_failed;

struct mytimerinfo {
	struct rte_timer tim;
	unsigned id;
	unsigned count;
};

static struct mytimerinfo mytiminfo[NB_TIMER];

static void timer_basic_cb(struct rte_timer *tim, void *arg);

static void
mytimer_reset(struct mytimerinfo *timinfo, uint64_t ticks,
	      enum rte_timer_type type, unsigned tim_lcore,
	      rte_timer_cb_t fct)
{
	rte_timer_reset_sync(&timinfo->tim, ticks, type, tim_lcore,
			     fct, timinfo);
}

/* timer callback for stress tests */
static void
timer_stress_cb(__attribute__((unused)) struct rte_timer *tim,
		__attribute__((unused)) void *arg)
{
	long r;
	unsigned lcore_id = rte_lcore_id();
	uint64_t hz = rte_get_timer_hz();

	if (rte_timer_pending(tim))
		return;

	r = rte_rand();
	if ((r & 0xff) == 0) {
		mytimer_reset(&mytiminfo[0], hz, SINGLE, lcore_id,
			      timer_stress_cb);
	}
	else if ((r & 0xff) == 1) {
		mytimer_reset(&mytiminfo[0], hz, SINGLE,
			      rte_get_next_lcore(lcore_id, 0, 1),
			      timer_stress_cb);
	}
	else if ((r & 0xff) == 2) {
		rte_timer_stop(&mytiminfo[0].tim);
	}
}

static int
timer_stress_main_loop(__attribute__((unused)) void *arg)
{
	uint64_t hz = rte_get_timer_hz();
	unsigned lcore_id = rte_lcore_id();
	uint64_t cur_time;
	int64_t diff = 0;
	long r;

	while (diff >= 0) {

		/* call the timer handler on each core */
		rte_timer_manage();

		/* simulate the processing of a packet
		 * (1 us = 2000 cycles at 2 Ghz) */
		rte_delay_us(1);

		/* randomly stop or reset timer */
		r = rte_rand();
		lcore_id = rte_get_next_lcore(lcore_id, 0, 1);
		if ((r & 0xff) == 0) {
			/* 100 us */
			mytimer_reset(&mytiminfo[0], hz/10000, SINGLE, lcore_id,
				      timer_stress_cb);
		}
		else if ((r & 0xff) == 1) {
			rte_timer_stop_sync(&mytiminfo[0].tim);
		}
		cur_time = rte_get_timer_cycles();
		diff = end_time - cur_time;
	}

	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, TESTTIMER, "core %u finished\n", lcore_id);

	return 0;
}

/* Need to synchronize slave lcores through multiple steps. */
enum { SLAVE_WAITING = 1, SLAVE_RUN_SIGNAL, SLAVE_RUNNING, SLAVE_FINISHED };
static rte_atomic16_t slave_state[RTE_MAX_LCORE];

static void
master_init_slaves(void)
{
	unsigned i;

	RTE_LCORE_FOREACH_SLAVE(i) {
		rte_atomic16_set(&slave_state[i], SLAVE_WAITING);
	}
}

static void
master_start_slaves(void)
{
	unsigned i;

	RTE_LCORE_FOREACH_SLAVE(i) {
		rte_atomic16_set(&slave_state[i], SLAVE_RUN_SIGNAL);
	}
	RTE_LCORE_FOREACH_SLAVE(i) {
		while (rte_atomic16_read(&slave_state[i]) != SLAVE_RUNNING)
			rte_pause();
	}
}

static void
master_wait_for_slaves(void)
{
	unsigned i;

	RTE_LCORE_FOREACH_SLAVE(i) {
		while (rte_atomic16_read(&slave_state[i]) != SLAVE_FINISHED)
			rte_pause();
	}
}

static void
slave_wait_to_start(void)
{
	unsigned lcore_id = rte_lcore_id();

	while (rte_atomic16_read(&slave_state[lcore_id]) != SLAVE_RUN_SIGNAL)
		rte_pause();
	rte_atomic16_set(&slave_state[lcore_id], SLAVE_RUNNING);
}

static void
slave_finish(void)
{
	unsigned lcore_id = rte_lcore_id();

	rte_atomic16_set(&slave_state[lcore_id], SLAVE_FINISHED);
}


static volatile int cb_count = 0;

/* callback for second stress test. will only be called
 * on master lcore */
static void
timer_stress2_cb(struct rte_timer *tim __rte_unused, void *arg __rte_unused)
{
	cb_count++;
}

#define NB_STRESS2_TIMERS 8192

static int
timer_stress2_main_loop(__attribute__((unused)) void *arg)
{
	static struct rte_timer *timers;
	int i, ret;
	uint64_t delay = rte_get_timer_hz() / 20;
	unsigned lcore_id = rte_lcore_id();
	unsigned master = rte_get_master_lcore();
	int32_t my_collisions = 0;
	static rte_atomic32_t collisions;

	if (lcore_id == master) {
		cb_count = 0;
		test_failed = 0;
		rte_atomic32_set(&collisions, 0);
		master_init_slaves();
		timers = rte_malloc(NULL, sizeof(*timers) * NB_STRESS2_TIMERS, 0);
		if (timers == NULL) {
			printf("Test Failed\n");
			printf("- Cannot allocate memory for timers\n" );
			test_failed = 1;
			master_start_slaves();
			goto cleanup;
		}
		for (i = 0; i < NB_STRESS2_TIMERS; i++)
			rte_timer_init(&timers[i]);
		master_start_slaves();
	} else {
		slave_wait_to_start();
		if (test_failed)
			goto cleanup;
	}

	/* have all cores schedule all timers on master lcore */
	for (i = 0; i < NB_STRESS2_TIMERS; i++) {
		ret = rte_timer_reset(&timers[i], delay, SINGLE, master,
				timer_stress2_cb, NULL);
		/* there will be collisions when multiple cores simultaneously
		 * configure the same timers */
		if (ret != 0)
			my_collisions++;
	}
	if (my_collisions != 0)
		rte_atomic32_add(&collisions, my_collisions);

	/* wait long enough for timers to expire */
	rte_delay_ms(100);

	/* all cores rendezvous */
	if (lcore_id == master) {
		master_wait_for_slaves();
	} else {
		slave_finish();
	}

	/* now check that we get the right number of callbacks */
	if (lcore_id == master) {
		my_collisions = rte_atomic32_read(&collisions);
		if (my_collisions != 0)
			printf("- %d timer reset collisions (OK)\n", my_collisions);
		rte_timer_manage();
		if (cb_count != NB_STRESS2_TIMERS) {
			printf("Test Failed\n");
			printf("- Stress test 2, part 1 failed\n");
			printf("- Expected %d callbacks, got %d\n", NB_STRESS2_TIMERS,
					cb_count);
			test_failed = 1;
			master_start_slaves();
			goto cleanup;
		}
		cb_count = 0;

		/* proceed */
		master_start_slaves();
	} else {
		/* proceed */
		slave_wait_to_start();
		if (test_failed)
			goto cleanup;
	}

	/* now test again, just stop and restart timers at random after init*/
	for (i = 0; i < NB_STRESS2_TIMERS; i++)
		rte_timer_reset(&timers[i], delay, SINGLE, master,
				timer_stress2_cb, NULL);

	/* pick random timer to reset, stopping them first half the time */
	for (i = 0; i < 100000; i++) {
		int r = rand() % NB_STRESS2_TIMERS;
		if (i % 2)
			rte_timer_stop(&timers[r]);
		rte_timer_reset(&timers[r], delay, SINGLE, master,
				timer_stress2_cb, NULL);
	}

	/* wait long enough for timers to expire */
	rte_delay_ms(100);

	/* now check that we get the right number of callbacks */
	if (lcore_id == master) {
		master_wait_for_slaves();

		rte_timer_manage();
		if (cb_count != NB_STRESS2_TIMERS) {
			printf("Test Failed\n");
			printf("- Stress test 2, part 2 failed\n");
			printf("- Expected %d callbacks, got %d\n", NB_STRESS2_TIMERS,
					cb_count);
			test_failed = 1;
		} else {
			printf("Test OK\n");
		}
	}

cleanup:
	if (lcore_id == master) {
		master_wait_for_slaves();
		if (timers != NULL) {
			rte_free(timers);
			timers = NULL;
		}
	} else {
		slave_finish();
	}

	return 0;
}

/* timer callback for basic tests */
static void
timer_basic_cb(struct rte_timer *tim, void *arg)
{
	struct mytimerinfo *timinfo = arg;
	uint64_t hz = rte_get_timer_hz();
	unsigned lcore_id = rte_lcore_id();
	uint64_t cur_time = rte_get_timer_cycles();

	if (rte_timer_pending(tim))
		return;

	timinfo->count ++;

	RTE_LOG(INFO, TESTTIMER,
		"%"PRIu64": callback id=%u count=%u on core %u\n",
		cur_time, timinfo->id, timinfo->count, lcore_id);

	/* reload timer 0 on same core */
	if (timinfo->id == 0 && timinfo->count < 20) {
		mytimer_reset(timinfo, hz, SINGLE, lcore_id, timer_basic_cb);
		return;
	}

	/* reload timer 1 on next core */
	if (timinfo->id == 1 && timinfo->count < 10) {
		mytimer_reset(timinfo, hz*2, SINGLE,
			      rte_get_next_lcore(lcore_id, 0, 1),
			      timer_basic_cb);
		return;
	}

	/* Explicitelly stop timer 0. Once stop() called, we can even
	 * erase the content of the structure: it is not referenced
	 * anymore by any code (in case of dynamic structure, it can
	 * be freed) */
	if (timinfo->id == 0 && timinfo->count == 20) {

		/* stop_sync() is not needed, because we know that the
		 * status of timer is only modified by this core */
		rte_timer_stop(tim);
		memset(tim, 0xAA, sizeof(struct rte_timer));
		return;
	}

	/* stop timer3, and restart a new timer0 (it was removed 5
	 * seconds ago) for a single shot */
	if (timinfo->id == 2 && timinfo->count == 25) {
		rte_timer_stop_sync(&mytiminfo[3].tim);

		/* need to reinit because structure was erased with 0xAA */
		rte_timer_init(&mytiminfo[0].tim);
		mytimer_reset(&mytiminfo[0], hz, SINGLE, lcore_id,
			      timer_basic_cb);
	}
}

static int
timer_basic_main_loop(__attribute__((unused)) void *arg)
{
	uint64_t hz = rte_get_timer_hz();
	unsigned lcore_id = rte_lcore_id();
	uint64_t cur_time;
	int64_t diff = 0;

	/* launch all timers on core 0 */
	if (lcore_id == rte_get_master_lcore()) {
		mytimer_reset(&mytiminfo[0], hz/4, SINGLE, lcore_id,
			      timer_basic_cb);
		mytimer_reset(&mytiminfo[1], hz/2, SINGLE, lcore_id,
			      timer_basic_cb);
		mytimer_reset(&mytiminfo[2], hz/4, PERIODICAL, lcore_id,
			      timer_basic_cb);
		mytimer_reset(&mytiminfo[3], hz/4, PERIODICAL,
			      rte_get_next_lcore(lcore_id, 0, 1),
			      timer_basic_cb);
	}

	while (diff >= 0) {

		/* call the timer handler on each core */
		rte_timer_manage();

		/* simulate the processing of a packet
		 * (3 us = 6000 cycles at 2 Ghz) */
		rte_delay_us(3);

		cur_time = rte_get_timer_cycles();
		diff = end_time - cur_time;
	}
	RTE_LOG(INFO, TESTTIMER, "core %u finished\n", lcore_id);

	return 0;
}

static int
timer_sanity_check(void)
{
#ifdef RTE_LIBEAL_USE_HPET
	if (eal_timer_source != EAL_TIMER_HPET) {
		printf("Not using HPET, can't sanity check timer sources\n");
		return 0;
	}

	const uint64_t t_hz = rte_get_tsc_hz();
	const uint64_t h_hz = rte_get_hpet_hz();
	printf("Hertz values: TSC = %"PRIu64", HPET = %"PRIu64"\n", t_hz, h_hz);

	const uint64_t tsc_start = rte_get_tsc_cycles();
	const uint64_t hpet_start = rte_get_hpet_cycles();
	rte_delay_ms(100); /* delay 1/10 second */
	const uint64_t tsc_end = rte_get_tsc_cycles();
	const uint64_t hpet_end = rte_get_hpet_cycles();
	printf("Measured cycles: TSC = %"PRIu64", HPET = %"PRIu64"\n",
			tsc_end-tsc_start, hpet_end-hpet_start);

	const double tsc_time = (double)(tsc_end - tsc_start)/t_hz;
	const double hpet_time = (double)(hpet_end - hpet_start)/h_hz;
	/* get the percentage that the times differ by */
	const double time_diff = fabs(tsc_time - hpet_time)*100/tsc_time;
	printf("Measured time: TSC = %.4f, HPET = %.4f\n", tsc_time, hpet_time);

	printf("Elapsed time measured by TSC and HPET differ by %f%%\n",
			time_diff);
	if (time_diff > 0.1) {
		printf("Error times differ by >0.1%%");
		return -1;
	}
#endif
	return 0;
}

static int
test_timer(void)
{
	unsigned i;
	uint64_t cur_time;
	uint64_t hz;

	/* sanity check our timer sources and timer config values */
	if (timer_sanity_check() < 0) {
		printf("Timer sanity checks failed\n");
		return TEST_FAILED;
	}

	if (rte_lcore_count() < 2) {
		printf("not enough lcores for this test\n");
		return TEST_FAILED;
	}

	/* init timer */
	for (i=0; i<NB_TIMER; i++) {
		memset(&mytiminfo[i], 0, sizeof(struct mytimerinfo));
		mytiminfo[i].id = i;
		rte_timer_init(&mytiminfo[i].tim);
	}

	/* calculate the "end of test" time */
	cur_time = rte_get_timer_cycles();
	hz = rte_get_timer_hz();
	end_time = cur_time + (hz * TEST_DURATION_S);

	/* start other cores */
	printf("Start timer stress tests\n");
	rte_eal_mp_remote_launch(timer_stress_main_loop, NULL, CALL_MASTER);
	rte_eal_mp_wait_lcore();

	/* stop timer 0 used for stress test */
	rte_timer_stop_sync(&mytiminfo[0].tim);

	/* run a second, slightly different set of stress tests */
	printf("\nStart timer stress tests 2\n");
	test_failed = 0;
	rte_eal_mp_remote_launch(timer_stress2_main_loop, NULL, CALL_MASTER);
	rte_eal_mp_wait_lcore();
	if (test_failed)
		return TEST_FAILED;

	/* calculate the "end of test" time */
	cur_time = rte_get_timer_cycles();
	hz = rte_get_timer_hz();
	end_time = cur_time + (hz * TEST_DURATION_S);

	/* start other cores */
	printf("\nStart timer basic tests\n");
	rte_eal_mp_remote_launch(timer_basic_main_loop, NULL, CALL_MASTER);
	rte_eal_mp_wait_lcore();

	/* stop all timers */
	for (i=0; i<NB_TIMER; i++) {
		rte_timer_stop_sync(&mytiminfo[i].tim);
	}

	rte_timer_dump_stats(stdout);

	return TEST_SUCCESS;
}

REGISTER_TEST_COMMAND(timer_autotest, test_timer);
