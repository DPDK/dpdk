/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Microsoft Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/queue.h>
#include <string.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_pflock.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_cycles.h>

#include "test.h"

/*
 * phase-fair lock test
 * ====================
 * Provides UT for phase-fair lock API.
 * Main concern is on functional testing, but also provides some
 * performance measurements.
 * Obviously for proper testing need to be executed with more than one lcore.
 */

static rte_pflock_t sl;
static rte_pflock_t sl_tab[RTE_MAX_LCORE];
static uint32_t synchro;

static int
test_pflock_per_core(__rte_unused void *arg)
{
	rte_pflock_write_lock(&sl);
	printf("Global write lock taken on core %u\n", rte_lcore_id());
	rte_pflock_write_unlock(&sl);

	rte_pflock_write_lock(&sl_tab[rte_lcore_id()]);
	printf("Hello from core %u !\n", rte_lcore_id());
	rte_pflock_write_unlock(&sl_tab[rte_lcore_id()]);

	rte_pflock_read_lock(&sl);
	printf("Global read lock taken on core %u\n", rte_lcore_id());
	rte_delay_ms(100);
	printf("Release global read lock on core %u\n", rte_lcore_id());
	rte_pflock_read_unlock(&sl);

	return 0;
}

static rte_pflock_t lk = RTE_PFLOCK_INITIALIZER;
static uint64_t time_count[RTE_MAX_LCORE] = {0};

#define MAX_LOOP 10000

static int
load_loop_fn(void *arg)
{
	uint64_t time_diff = 0, begin;
	uint64_t hz = rte_get_timer_hz();
	uint64_t lcount = 0;
	const int use_lock = *(int *)arg;
	const unsigned int lcore = rte_lcore_id();

	/* wait synchro for workers */
	if (lcore != rte_get_main_lcore())
		rte_wait_until_equal_32(&synchro, 1, __ATOMIC_RELAXED);

	begin = rte_rdtsc_precise();
	while (lcount < MAX_LOOP) {
		if (use_lock)
			rte_pflock_write_lock(&lk);
		lcount++;
		if (use_lock)
			rte_pflock_write_unlock(&lk);

		if (use_lock) {
			rte_pflock_read_lock(&lk);
			rte_pflock_read_unlock(&lk);
		}
	}

	time_diff = rte_rdtsc_precise() - begin;
	time_count[lcore] = time_diff * 1000000 / hz;
	return 0;
}

static int
test_pflock_perf(void)
{
	unsigned int i;
	int lock = 0;
	uint64_t total = 0;
	const unsigned int lcore = rte_lcore_id();

	printf("\nTest with no lock on single core...\n");
	__atomic_store_n(&synchro, 1, __ATOMIC_RELAXED);
	load_loop_fn(&lock);
	printf("Core [%u] Cost Time = %"PRIu64" us\n",
			lcore, time_count[lcore]);
	memset(time_count, 0, sizeof(time_count));

	printf("\nTest with phase-fair lock on single core...\n");
	lock = 1;
	__atomic_store_n(&synchro, 1, __ATOMIC_RELAXED);
	load_loop_fn(&lock);
	printf("Core [%u] Cost Time = %"PRIu64" us\n",
			lcore, time_count[lcore]);
	memset(time_count, 0, sizeof(time_count));

	printf("\nPhase-fair test on %u cores...\n", rte_lcore_count());

	/* clear synchro and start workers */
	__atomic_store_n(&synchro, 0, __ATOMIC_RELAXED);
	if (rte_eal_mp_remote_launch(load_loop_fn, &lock, SKIP_MAIN) < 0)
		return -1;

	/* start synchro and launch test on main */
	__atomic_store_n(&synchro, 1, __ATOMIC_RELAXED);
	load_loop_fn(&lock);

	rte_eal_mp_wait_lcore();

	RTE_LCORE_FOREACH(i) {
		printf("Core [%u] cost time = %"PRIu64" us\n",
			i, time_count[i]);
		total += time_count[i];
	}

	printf("Total cost time = %"PRIu64" us\n", total);
	memset(time_count, 0, sizeof(time_count));

	return 0;
}

/*
 * - There is a global pflock and a table of pflocks (one per lcore).
 *
 * - The test function takes all of these locks and launches the
 *   ``test_pflock_per_core()`` function on each core (except the main).
 *
 *   - The function takes the global write lock, display something,
 *     then releases the global lock.
 *   - Then, it takes the per-lcore write lock, display something, and
 *     releases the per-core lock.
 *   - Finally, a read lock is taken during 100 ms, then released.
 *
 * - The main function unlocks the per-lcore locks sequentially and
 *   waits between each lock. This triggers the display of a message
 *   for each core, in the correct order.
 *
 *   Then, it tries to take the global write lock and display the last
 *   message. The autotest script checks that the message order is correct.
 */
static int
test_pflock(void)
{
	int i;

	rte_pflock_init(&sl);
	for (i = 0; i < RTE_MAX_LCORE; i++)
		rte_pflock_init(&sl_tab[i]);

	rte_pflock_write_lock(&sl);

	RTE_LCORE_FOREACH_WORKER(i) {
		rte_pflock_write_lock(&sl_tab[i]);
		rte_eal_remote_launch(test_pflock_per_core, NULL, i);
	}

	rte_pflock_write_unlock(&sl);

	RTE_LCORE_FOREACH_WORKER(i) {
		rte_pflock_write_unlock(&sl_tab[i]);
		rte_delay_ms(100);
	}

	rte_pflock_write_lock(&sl);
	/* this message should be the last message of test */
	printf("Global write lock taken on main core %u\n", rte_lcore_id());
	rte_pflock_write_unlock(&sl);

	rte_eal_mp_wait_lcore();

	if (test_pflock_perf() < 0)
		return -1;

	return 0;
}

REGISTER_FAST_TEST(pflock_autotest, true, true, test_pflock);
