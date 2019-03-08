/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <sys/queue.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_spinlock.h>
#include <rte_atomic.h>

#include "test.h"

/*
 * Spinlock test
 * =============
 *
 * - There is a global spinlock and a table of spinlocks (one per lcore).
 *
 * - The test function takes all of these locks and launches the
 *   ``test_spinlock_per_core()`` function on each core (except the master).
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

static rte_spinlock_t sl, sl_try;
static rte_spinlock_t sl_tab[RTE_MAX_LCORE];
static rte_spinlock_recursive_t slr;
static unsigned count = 0;

static rte_atomic32_t synchro;

static int
test_spinlock_per_core(__attribute__((unused)) void *arg)
{
	rte_spinlock_lock(&sl);
	printf("Global lock taken on core %u\n", rte_lcore_id());
	rte_spinlock_unlock(&sl);

	rte_spinlock_lock(&sl_tab[rte_lcore_id()]);
	printf("Hello from core %u !\n", rte_lcore_id());
	rte_spinlock_unlock(&sl_tab[rte_lcore_id()]);

	return 0;
}

static int
test_spinlock_recursive_per_core(__attribute__((unused)) void *arg)
{
	unsigned id = rte_lcore_id();

	rte_spinlock_recursive_lock(&slr);
	printf("Global recursive lock taken on core %u - count = %d\n",
	       id, slr.count);
	rte_spinlock_recursive_lock(&slr);
	printf("Global recursive lock taken on core %u - count = %d\n",
	       id, slr.count);
	rte_spinlock_recursive_lock(&slr);
	printf("Global recursive lock taken on core %u - count = %d\n",
	       id, slr.count);

	printf("Hello from within recursive locks from core %u !\n", id);

	rte_spinlock_recursive_unlock(&slr);
	printf("Global recursive lock released on core %u - count = %d\n",
	       id, slr.count);
	rte_spinlock_recursive_unlock(&slr);
	printf("Global recursive lock released on core %u - count = %d\n",
	       id, slr.count);
	rte_spinlock_recursive_unlock(&slr);
	printf("Global recursive lock released on core %u - count = %d\n",
	       id, slr.count);

	return 0;
}

static rte_spinlock_t lk = RTE_SPINLOCK_INITIALIZER;
static uint64_t time_count[RTE_MAX_LCORE] = {0};

#define MAX_LOOP 10000

static int
load_loop_fn(void *func_param)
{
	uint64_t time_diff = 0, begin;
	uint64_t hz = rte_get_timer_hz();
	volatile uint64_t lcount = 0;
	const int use_lock = *(int*)func_param;
	const unsigned lcore = rte_lcore_id();

	/* wait synchro for slaves */
	if (lcore != rte_get_master_lcore())
		while (rte_atomic32_read(&synchro) == 0);

	begin = rte_get_timer_cycles();
	while (lcount < MAX_LOOP) {
		if (use_lock)
			rte_spinlock_lock(&lk);
		lcount++;
		if (use_lock)
			rte_spinlock_unlock(&lk);
	}
	time_diff = rte_get_timer_cycles() - begin;
	time_count[lcore] = time_diff * 1000000 / hz;
	return 0;
}

static int
test_spinlock_perf(void)
{
	unsigned int i;
	uint64_t total = 0;
	int lock = 0;
	const unsigned lcore = rte_lcore_id();

	printf("\nTest with no lock on single core...\n");
	load_loop_fn(&lock);
	printf("Core [%u] Cost Time = %"PRIu64" us\n", lcore,
						time_count[lcore]);
	memset(time_count, 0, sizeof(time_count));

	printf("\nTest with lock on single core...\n");
	lock = 1;
	load_loop_fn(&lock);
	printf("Core [%u] Cost Time = %"PRIu64" us\n", lcore,
						time_count[lcore]);
	memset(time_count, 0, sizeof(time_count));

	printf("\nTest with lock on %u cores...\n", rte_lcore_count());

	/* Clear synchro and start slaves */
	rte_atomic32_set(&synchro, 0);
	rte_eal_mp_remote_launch(load_loop_fn, &lock, SKIP_MASTER);

	/* start synchro and launch test on master */
	rte_atomic32_set(&synchro, 1);
	load_loop_fn(&lock);

	rte_eal_mp_wait_lcore();

	RTE_LCORE_FOREACH(i) {
		printf("Core [%u] Cost Time = %"PRIu64" us\n", i,
						time_count[i]);
		total += time_count[i];
	}

	printf("Total Cost Time = %"PRIu64" us\n", total);

	return 0;
}

/*
 * Use rte_spinlock_trylock() to trylock a spinlock object,
 * If it could not lock the object successfully, it would
 * return immediately and the variable of "count" would be
 * increased by one per times. the value of "count" could be
 * checked as the result later.
 */
static int
test_spinlock_try(__attribute__((unused)) void *arg)
{
	if (rte_spinlock_trylock(&sl_try) == 0) {
		rte_spinlock_lock(&sl);
		count ++;
		rte_spinlock_unlock(&sl);
	}

	return 0;
}


/*
 * Test rte_eal_get_lcore_state() in addition to spinlocks
 * as we have "waiting" then "running" lcores.
 */
static int
test_spinlock(void)
{
	int ret = 0;
	int i;

	/* slave cores should be waiting: print it */
	RTE_LCORE_FOREACH_SLAVE(i) {
		printf("lcore %d state: %d\n", i,
		       (int) rte_eal_get_lcore_state(i));
	}

	rte_spinlock_init(&sl);
	rte_spinlock_init(&sl_try);
	rte_spinlock_recursive_init(&slr);
	for (i=0; i<RTE_MAX_LCORE; i++)
		rte_spinlock_init(&sl_tab[i]);

	rte_spinlock_lock(&sl);

	RTE_LCORE_FOREACH_SLAVE(i) {
		rte_spinlock_lock(&sl_tab[i]);
		rte_eal_remote_launch(test_spinlock_per_core, NULL, i);
	}

	/* slave cores should be busy: print it */
	RTE_LCORE_FOREACH_SLAVE(i) {
		printf("lcore %d state: %d\n", i,
		       (int) rte_eal_get_lcore_state(i));
	}
	rte_spinlock_unlock(&sl);

	RTE_LCORE_FOREACH_SLAVE(i) {
		rte_spinlock_unlock(&sl_tab[i]);
		rte_delay_ms(10);
	}

	rte_eal_mp_wait_lcore();

	rte_spinlock_recursive_lock(&slr);

	/*
	 * Try to acquire a lock that we already own
	 */
	if(!rte_spinlock_recursive_trylock(&slr)) {
		printf("rte_spinlock_recursive_trylock failed on a lock that "
		       "we already own\n");
		ret = -1;
	} else
		rte_spinlock_recursive_unlock(&slr);

	RTE_LCORE_FOREACH_SLAVE(i) {
		rte_eal_remote_launch(test_spinlock_recursive_per_core, NULL, i);
	}
	rte_spinlock_recursive_unlock(&slr);
	rte_eal_mp_wait_lcore();

	/*
	 * Test if it could return immediately from try-locking a locked object.
	 * Here it will lock the spinlock object first, then launch all the slave
	 * lcores to trylock the same spinlock object.
	 * All the slave lcores should give up try-locking a locked object and
	 * return immediately, and then increase the "count" initialized with zero
	 * by one per times.
	 * We can check if the "count" is finally equal to the number of all slave
	 * lcores to see if the behavior of try-locking a locked spinlock object
	 * is correct.
	 */
	if (rte_spinlock_trylock(&sl_try) == 0) {
		return -1;
	}
	count = 0;
	RTE_LCORE_FOREACH_SLAVE(i) {
		rte_eal_remote_launch(test_spinlock_try, NULL, i);
	}
	rte_eal_mp_wait_lcore();
	rte_spinlock_unlock(&sl_try);
	if (rte_spinlock_is_locked(&sl)) {
		printf("spinlock is locked but it should not be\n");
		return -1;
	}
	rte_spinlock_lock(&sl);
	if (count != ( rte_lcore_count() - 1)) {
		ret = -1;
	}
	rte_spinlock_unlock(&sl);

	/*
	 * Test if it can trylock recursively.
	 * Use rte_spinlock_recursive_trylock() to check if it can lock a spinlock
	 * object recursively. Here it will try to lock a spinlock object twice.
	 */
	if (rte_spinlock_recursive_trylock(&slr) == 0) {
		printf("It failed to do the first spinlock_recursive_trylock but it should able to do\n");
		return -1;
	}
	if (rte_spinlock_recursive_trylock(&slr) == 0) {
		printf("It failed to do the second spinlock_recursive_trylock but it should able to do\n");
		return -1;
	}
	rte_spinlock_recursive_unlock(&slr);
	rte_spinlock_recursive_unlock(&slr);

	if (test_spinlock_perf() < 0)
		return -1;

	return ret;
}

REGISTER_TEST_COMMAND(spinlock_autotest, test_spinlock);
