/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Arm Limited
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_per_lcore.h>
#include <rte_ticketlock.h>

#include "test.h"

/*
 * Ticketlock test
 * =============
 *
 * - There is a global ticketlock and a table of ticketlocks (one per lcore).
 *
 * - The test function takes all of these locks and launches the
 *   ``test_ticketlock_per_core()`` function on each core (except the main).
 *
 *   - The function takes the global lock, display something, then releases
 *     the global lock.
 *   - The function takes the per-lcore lock, display something, then releases
 *     the per-core lock.
 *
 * - The main function unlocks the per-lcore locks sequentially and
 *   waits between each lock. This triggers the display of a message
 *   for each core, in the correct order. The autotest script checks that
 *   this order is correct.
 *
 * - A load test is carried out, with all cores attempting to lock a single lock
 *   multiple times
 */

static rte_ticketlock_t tl, tl_try;
static rte_ticketlock_t tl_tab[RTE_MAX_LCORE];
static rte_ticketlock_recursive_t tlr;
static unsigned int count;

static uint32_t synchro;

static int
test_ticketlock_per_core(__rte_unused void *arg)
{
	rte_ticketlock_lock(&tl);
	printf("Global lock taken on core %u\n", rte_lcore_id());
	rte_ticketlock_unlock(&tl);

	rte_ticketlock_lock(&tl_tab[rte_lcore_id()]);
	printf("Hello from core %u !\n", rte_lcore_id());
	rte_ticketlock_unlock(&tl_tab[rte_lcore_id()]);

	return 0;
}

static int
test_ticketlock_recursive_per_core(__rte_unused void *arg)
{
	unsigned int id = rte_lcore_id();

	rte_ticketlock_recursive_lock(&tlr);
	printf("Global recursive lock taken on core %u - count = %d\n",
	       id, tlr.count);
	rte_ticketlock_recursive_lock(&tlr);
	printf("Global recursive lock taken on core %u - count = %d\n",
	       id, tlr.count);
	rte_ticketlock_recursive_lock(&tlr);
	printf("Global recursive lock taken on core %u - count = %d\n",
	       id, tlr.count);

	printf("Hello from within recursive locks from core %u !\n", id);

	rte_ticketlock_recursive_unlock(&tlr);
	printf("Global recursive lock released on core %u - count = %d\n",
	       id, tlr.count);
	rte_ticketlock_recursive_unlock(&tlr);
	printf("Global recursive lock released on core %u - count = %d\n",
	       id, tlr.count);
	rte_ticketlock_recursive_unlock(&tlr);
	printf("Global recursive lock released on core %u - count = %d\n",
	       id, tlr.count);

	return 0;
}

static rte_ticketlock_t lk = RTE_TICKETLOCK_INITIALIZER;
static uint64_t lcount __rte_cache_aligned;
static uint64_t lcore_count[RTE_MAX_LCORE] __rte_cache_aligned;
static uint64_t time_cost[RTE_MAX_LCORE];

#define MAX_LOOP 10000

static int
load_loop_fn(void *func_param)
{
	uint64_t time_diff = 0, begin;
	uint64_t hz = rte_get_timer_hz();
	const int use_lock = *(int *)func_param;
	const unsigned int lcore = rte_lcore_id();

	/* wait synchro for workers */
	if (lcore != rte_get_main_lcore())
		rte_wait_until_equal_32(&synchro, 1, __ATOMIC_RELAXED);

	begin = rte_rdtsc_precise();
	while (lcore_count[lcore] < MAX_LOOP) {
		if (use_lock)
			rte_ticketlock_lock(&lk);
		lcore_count[lcore]++;
		lcount++;
		if (use_lock)
			rte_ticketlock_unlock(&lk);
	}
	time_diff = rte_rdtsc_precise() - begin;
	time_cost[lcore] = time_diff * 1000000 / hz;
	return 0;
}

static int
test_ticketlock_perf(void)
{
	unsigned int i;
	uint64_t tcount = 0;
	uint64_t total_time = 0;
	int lock = 0;
	const unsigned int lcore = rte_lcore_id();

	printf("\nTest with no lock on single core...\n");
	load_loop_fn(&lock);
	printf("Core [%u] cost time = %"PRIu64" us\n", lcore, time_cost[lcore]);
	memset(lcore_count, 0, sizeof(lcore_count));
	memset(time_cost, 0, sizeof(time_cost));

	printf("\nTest with lock on single core...\n");
	lock = 1;
	load_loop_fn(&lock);
	printf("Core [%u] cost time = %"PRIu64" us\n", lcore, time_cost[lcore]);
	memset(lcore_count, 0, sizeof(lcore_count));
	memset(time_cost, 0, sizeof(time_cost));

	lcount = 0;
	printf("\nTest with lock on %u cores...\n", rte_lcore_count());

	/* Clear synchro and start workers */
	__atomic_store_n(&synchro, 0, __ATOMIC_RELAXED);
	rte_eal_mp_remote_launch(load_loop_fn, &lock, SKIP_MAIN);

	/* start synchro and launch test on main */
	__atomic_store_n(&synchro, 1, __ATOMIC_RELAXED);
	load_loop_fn(&lock);

	rte_eal_mp_wait_lcore();

	RTE_LCORE_FOREACH(i) {
		printf("Core [%u] cost time = %"PRIu64" us\n", i, time_cost[i]);
		tcount += lcore_count[i];
		total_time += time_cost[i];
	}

	if (tcount != lcount)
		return -1;

	printf("Total cost time = %"PRIu64" us\n", total_time);

	return 0;
}

/*
 * Use rte_ticketlock_trylock() to trylock a ticketlock object,
 * If it could not lock the object successfully, it would
 * return immediately and the variable of "count" would be
 * increased by one per times. the value of "count" could be
 * checked as the result later.
 */
static int
test_ticketlock_try(__rte_unused void *arg)
{
	if (rte_ticketlock_trylock(&tl_try) == 0) {
		rte_ticketlock_lock(&tl);
		count++;
		rte_ticketlock_unlock(&tl);
	}

	return 0;
}


/*
 * Test rte_eal_get_lcore_state() in addition to ticketlocks
 * as we have "waiting" then "running" lcores.
 */
static int
test_ticketlock(void)
{
	int ret = 0;
	int i;

	/* worker cores should be waiting: print it */
	RTE_LCORE_FOREACH_WORKER(i) {
		printf("lcore %d state: %d\n", i,
		       (int) rte_eal_get_lcore_state(i));
	}

	rte_ticketlock_init(&tl);
	rte_ticketlock_init(&tl_try);
	rte_ticketlock_recursive_init(&tlr);
	RTE_LCORE_FOREACH_WORKER(i) {
		rte_ticketlock_init(&tl_tab[i]);
	}

	rte_ticketlock_lock(&tl);

	RTE_LCORE_FOREACH_WORKER(i) {
		rte_ticketlock_lock(&tl_tab[i]);
		rte_eal_remote_launch(test_ticketlock_per_core, NULL, i);
	}

	/* worker cores should be busy: print it */
	RTE_LCORE_FOREACH_WORKER(i) {
		printf("lcore %d state: %d\n", i,
		       (int) rte_eal_get_lcore_state(i));
	}
	rte_ticketlock_unlock(&tl);

	RTE_LCORE_FOREACH_WORKER(i) {
		rte_ticketlock_unlock(&tl_tab[i]);
		rte_delay_ms(10);
	}

	rte_eal_mp_wait_lcore();

	rte_ticketlock_recursive_lock(&tlr);

	/*
	 * Try to acquire a lock that we already own
	 */
	if (!rte_ticketlock_recursive_trylock(&tlr)) {
		printf("rte_ticketlock_recursive_trylock failed on a lock that "
		       "we already own\n");
		ret = -1;
	} else
		rte_ticketlock_recursive_unlock(&tlr);

	RTE_LCORE_FOREACH_WORKER(i) {
		rte_eal_remote_launch(test_ticketlock_recursive_per_core,
					NULL, i);
	}
	rte_ticketlock_recursive_unlock(&tlr);
	rte_eal_mp_wait_lcore();

	/*
	 * Test if it could return immediately from try-locking a locked object.
	 * Here it will lock the ticketlock object first, then launch all the
	 * worker lcores to trylock the same ticketlock object.
	 * All the worker lcores should give up try-locking a locked object and
	 * return immediately, and then increase the "count" initialized with
	 * zero by one per times.
	 * We can check if the "count" is finally equal to the number of all
	 * worker lcores to see if the behavior of try-locking a locked
	 * ticketlock object is correct.
	 */
	if (rte_ticketlock_trylock(&tl_try) == 0)
		return -1;

	count = 0;
	RTE_LCORE_FOREACH_WORKER(i) {
		rte_eal_remote_launch(test_ticketlock_try, NULL, i);
	}
	rte_eal_mp_wait_lcore();
	rte_ticketlock_unlock(&tl_try);
	if (rte_ticketlock_is_locked(&tl)) {
		printf("ticketlock is locked but it should not be\n");
		return -1;
	}
	rte_ticketlock_lock(&tl);
	if (count != (rte_lcore_count() - 1))
		ret = -1;

	rte_ticketlock_unlock(&tl);

	/*
	 * Test if it can trylock recursively.
	 * Use rte_ticketlock_recursive_trylock() to check if it can lock
	 * a ticketlock object recursively. Here it will try to lock a
	 * ticketlock object twice.
	 */
	if (rte_ticketlock_recursive_trylock(&tlr) == 0) {
		printf("It failed to do the first ticketlock_recursive_trylock "
			   "but it should able to do\n");
		return -1;
	}
	if (rte_ticketlock_recursive_trylock(&tlr) == 0) {
		printf("It failed to do the second ticketlock_recursive_trylock "
			   "but it should able to do\n");
		return -1;
	}
	rte_ticketlock_recursive_unlock(&tlr);
	rte_ticketlock_recursive_unlock(&tlr);

	if (test_ticketlock_perf() < 0)
		return -1;

	return ret;
}

REGISTER_TEST_COMMAND(ticketlock_autotest, test_ticketlock);
