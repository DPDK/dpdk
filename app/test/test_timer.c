/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 */

/*
 * Timer
 * =====
 *
 * #. Stress tests.
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

#include <cmdline_parse.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_cycles.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_timer.h>
#include <rte_random.h>

#include "test.h"

#define TEST_DURATION_S 30 /* in seconds */
#define NB_TIMER 4

#define RTE_LOGTYPE_TESTTIMER RTE_LOGTYPE_USER3

static volatile uint64_t end_time;

struct mytimerinfo {
	struct rte_timer tim;
	unsigned id;
	unsigned count;
};

static struct mytimerinfo mytiminfo[NB_TIMER];

static void timer_basic_cb(struct rte_timer *tim, void *arg);

static void
mytimer_reset(struct mytimerinfo *timinfo, unsigned ticks,
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
	uint64_t hz = rte_get_hpet_hz();

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
	uint64_t hz = rte_get_hpet_hz();
	unsigned lcore_id = rte_lcore_id();
	uint64_t cur_time;
	int64_t diff = 0;
	long r;

	while (diff >= 0) {

		/* call the timer handler on each core */
		rte_timer_manage();

		/* simulate the processing of a packet
		 * (3 us = 6000 cycles at 2 Ghz) */
		rte_delay_us(3);

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
		cur_time = rte_get_hpet_cycles();
		diff = end_time - cur_time;
	}

	lcore_id = rte_lcore_id();
	RTE_LOG(INFO, TESTTIMER, "core %u finished\n", lcore_id);

	return 0;
}

/* timer callback for basic tests */
static void
timer_basic_cb(struct rte_timer *tim, void *arg)
{
	struct mytimerinfo *timinfo = arg;
	uint64_t hz = rte_get_hpet_hz();
	unsigned lcore_id = rte_lcore_id();
	uint64_t cur_time = rte_get_hpet_cycles();

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
	uint64_t hz = rte_get_hpet_hz();
	unsigned lcore_id = rte_lcore_id();
	uint64_t cur_time;
	int64_t diff = 0;

	/* launch all timers on core 0 */
	if (lcore_id == rte_get_master_lcore()) {
		mytimer_reset(&mytiminfo[0], hz, SINGLE, lcore_id,
			      timer_basic_cb);
		mytimer_reset(&mytiminfo[1], hz*2, SINGLE, lcore_id,
			      timer_basic_cb);
		mytimer_reset(&mytiminfo[2], hz, PERIODICAL, lcore_id,
			      timer_basic_cb);
		mytimer_reset(&mytiminfo[3], hz, PERIODICAL,
			      rte_get_next_lcore(lcore_id, 0, 1),
			      timer_basic_cb);
	}

	while (diff >= 0) {

		/* call the timer handler on each core */
		rte_timer_manage();

		/* simulate the processing of a packet
		 * (3 us = 6000 cycles at 2 Ghz) */
		rte_delay_us(3);

		cur_time = rte_get_hpet_cycles();
		diff = end_time - cur_time;
	}
	RTE_LOG(INFO, TESTTIMER, "core %u finished\n", lcore_id);

	return 0;
}

int
test_timer(void)
{
	unsigned i;
	uint64_t cur_time;
	uint64_t hz;

	if (rte_lcore_count() < 2) {
		printf("not enough lcores for this test\n");
		return -1;
	}

	/* init timer */
	for (i=0; i<NB_TIMER; i++) {
		memset(&mytiminfo[i], 0, sizeof(struct mytimerinfo));
		mytiminfo[i].id = i;
		rte_timer_init(&mytiminfo[i].tim);
	}

	/* calculate the "end of test" time */
	cur_time = rte_get_hpet_cycles();
	hz = rte_get_hpet_hz();
	end_time = cur_time + (hz * TEST_DURATION_S);

	/* start other cores */
	printf("Start timer stress tests (%d seconds)\n", TEST_DURATION_S);
	rte_eal_mp_remote_launch(timer_stress_main_loop, NULL, CALL_MASTER);
	rte_eal_mp_wait_lcore();

	/* stop timer 0 used for stress test */
	rte_timer_stop_sync(&mytiminfo[0].tim);

	/* calculate the "end of test" time */
	cur_time = rte_get_hpet_cycles();
	hz = rte_get_hpet_hz();
	end_time = cur_time + (hz * TEST_DURATION_S);

	/* start other cores */
	printf("Start timer basic tests (%d seconds)\n", TEST_DURATION_S);
	rte_eal_mp_remote_launch(timer_basic_main_loop, NULL, CALL_MASTER);
	rte_eal_mp_wait_lcore();

	/* stop all timers */
	for (i=0; i<NB_TIMER; i++) {
		rte_timer_stop_sync(&mytiminfo[i].tim);
	}

	rte_timer_dump_stats();

	return 0;
}
