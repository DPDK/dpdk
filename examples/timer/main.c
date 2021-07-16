/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_timer.h>
#include <rte_debug.h>

static uint64_t timer_resolution_cycles;
static struct rte_timer timer0;
static struct rte_timer timer1;

/* timer0 callback. 8< */
static void
timer0_cb(__rte_unused struct rte_timer *tim,
	  __rte_unused void *arg)
{
	static unsigned counter = 0;
	unsigned lcore_id = rte_lcore_id();

	printf("%s() on lcore %u\n", __func__, lcore_id);

	/* this timer is automatically reloaded until we decide to
	 * stop it, when counter reaches 20. */
	if ((counter ++) == 20)
		rte_timer_stop(tim);
}
/* >8 End of timer0 callback. */

/* timer1 callback. 8< */
static void
timer1_cb(__rte_unused struct rte_timer *tim,
	  __rte_unused void *arg)
{
	unsigned lcore_id = rte_lcore_id();
	uint64_t hz;

	printf("%s() on lcore %u\n", __func__, lcore_id);

	/* reload it on another lcore */
	hz = rte_get_timer_hz();
	lcore_id = rte_get_next_lcore(lcore_id, 0, 1);
	rte_timer_reset(tim, hz/3, SINGLE, lcore_id, timer1_cb, NULL);
}
/* >8 End of timer1 callback. */

static __rte_noreturn int
lcore_mainloop(__rte_unused void *arg)
{
	uint64_t prev_tsc = 0, cur_tsc, diff_tsc;
	unsigned lcore_id;

	lcore_id = rte_lcore_id();
	printf("Starting mainloop on core %u\n", lcore_id);

	/* Main loop. 8< */
	while (1) {
		/*
		 * Call the timer handler on each core: as we don't need a
		 * very precise timer, so only call rte_timer_manage()
		 * every ~10ms. In a real application, this will enhance
		 * performances as reading the HPET timer is not efficient.
		 */
		cur_tsc = rte_get_timer_cycles();
		diff_tsc = cur_tsc - prev_tsc;
		if (diff_tsc > timer_resolution_cycles) {
			rte_timer_manage();
			prev_tsc = cur_tsc;
		}
	}
	/* >8 End of main loop. */
}

int
main(int argc, char **argv)
{
	int ret;
	uint64_t hz;
	unsigned lcore_id;

	/* Init EAL. 8< */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	/* init RTE timer library */
	rte_timer_subsystem_init();
	/* >8 End of init EAL. */

	/* Init timer structures. 8< */
	rte_timer_init(&timer0);
	rte_timer_init(&timer1);
	/* >8 End of init timer structures. */

	/* Load timer0, every second, on main lcore, reloaded automatically. 8< */
	hz = rte_get_timer_hz();
	timer_resolution_cycles = hz * 10 / 1000; /* around 10ms */

	lcore_id = rte_lcore_id();
	rte_timer_reset(&timer0, hz, PERIODICAL, lcore_id, timer0_cb, NULL);

	/* load timer1, every second/3, on next lcore, reloaded manually */
	lcore_id = rte_get_next_lcore(lcore_id, 0, 1);
	rte_timer_reset(&timer1, hz/3, SINGLE, lcore_id, timer1_cb, NULL);

	/* >8 End of two timers configured. */

	/* Call lcore_mainloop() on every worker lcore. 8< */
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		rte_eal_remote_launch(lcore_mainloop, NULL, lcore_id);
	}

	/* call it on main lcore too */
	(void) lcore_mainloop(NULL);
	/* >8 End of call lcore_mainloop() on every worker lcore. */

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
