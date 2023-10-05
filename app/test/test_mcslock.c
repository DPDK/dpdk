/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Arm Limited
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
#include <rte_mcslock.h>

#include "test.h"

/*
 * RTE MCS lock test
 * =================
 *
 * These tests are derived from spin lock test cases.
 *
 * - The functional test takes all of these locks and launches the
 *   ''test_mcslock_per_core()'' function on each core (except the main).
 *
 *   - The function takes the global lock, display something, then releases
 *     the global lock on each core.
 *
 * - A load test is carried out, with all cores attempting to lock a single
 *   lock multiple times.
 */

RTE_ATOMIC(rte_mcslock_t *) p_ml;
RTE_ATOMIC(rte_mcslock_t *) p_ml_try;
RTE_ATOMIC(rte_mcslock_t *) p_ml_perf;

static unsigned int count;

static uint32_t synchro;

static int
test_mcslock_per_core(__rte_unused void *arg)
{
	/* Per core me node. */
	rte_mcslock_t ml_me;

	rte_mcslock_lock(&p_ml, &ml_me);
	printf("MCS lock taken on core %u\n", rte_lcore_id());
	rte_mcslock_unlock(&p_ml, &ml_me);
	printf("MCS lock released on core %u\n", rte_lcore_id());

	return 0;
}

static uint64_t time_count[RTE_MAX_LCORE] = {0};

#define MAX_LOOP 1000000

static int
load_loop_fn(void *func_param)
{
	uint64_t time_diff = 0, begin;
	uint64_t hz = rte_get_timer_hz();
	volatile uint64_t lcount = 0;
	const int use_lock = *(int *)func_param;
	const unsigned int lcore = rte_lcore_id();

	/**< Per core me node. */
	rte_mcslock_t ml_perf_me;

	/* wait synchro */
	rte_wait_until_equal_32(&synchro, 1, __ATOMIC_RELAXED);

	begin = rte_get_timer_cycles();
	while (lcount < MAX_LOOP) {
		if (use_lock)
			rte_mcslock_lock(&p_ml_perf, &ml_perf_me);

		lcount++;
		if (use_lock)
			rte_mcslock_unlock(&p_ml_perf, &ml_perf_me);
	}
	time_diff = rte_get_timer_cycles() - begin;
	time_count[lcore] = time_diff * 1000000 / hz;
	return 0;
}

static int
test_mcslock_perf(void)
{
	unsigned int i;
	uint64_t total = 0;
	int lock = 0;
	const unsigned int lcore = rte_lcore_id();

	printf("\nTest with no lock on single core...\n");
	__atomic_store_n(&synchro, 1, __ATOMIC_RELAXED);
	load_loop_fn(&lock);
	printf("Core [%u] Cost Time = %"PRIu64" us\n",
			lcore, time_count[lcore]);
	memset(time_count, 0, sizeof(time_count));

	printf("\nTest with lock on single core...\n");
	__atomic_store_n(&synchro, 1, __ATOMIC_RELAXED);
	lock = 1;
	load_loop_fn(&lock);
	printf("Core [%u] Cost Time = %"PRIu64" us\n",
			lcore, time_count[lcore]);
	memset(time_count, 0, sizeof(time_count));

	printf("\nTest with lock on %u cores...\n", (rte_lcore_count()));

	__atomic_store_n(&synchro, 0, __ATOMIC_RELAXED);
	rte_eal_mp_remote_launch(load_loop_fn, &lock, SKIP_MAIN);

	/* start synchro and launch test on main */
	__atomic_store_n(&synchro, 1, __ATOMIC_RELAXED);
	load_loop_fn(&lock);

	rte_eal_mp_wait_lcore();

	RTE_LCORE_FOREACH(i) {
		printf("Core [%u] Cost Time = %"PRIu64" us\n",
				i, time_count[i]);
		total += time_count[i];
	}

	printf("Total Cost Time = %"PRIu64" us\n", total);

	return 0;
}

/*
 * Use rte_mcslock_trylock() to trylock a mcs lock object,
 * If it could not lock the object successfully, it would
 * return immediately.
 */
static int
test_mcslock_try(__rte_unused void *arg)
{
	/**< Per core me node. */
	rte_mcslock_t ml_me;
	rte_mcslock_t ml_try_me;

	/* Locked ml_try in the main lcore, so it should fail
	 * when trying to lock it in the worker lcore.
	 */
	if (rte_mcslock_trylock(&p_ml_try, &ml_try_me) == 0) {
		rte_mcslock_lock(&p_ml, &ml_me);
		count++;
		rte_mcslock_unlock(&p_ml, &ml_me);
	}

	return 0;
}


/*
 * Test rte_eal_get_lcore_state() in addition to mcs locks
 * as we have "waiting" then "running" lcores.
 */
static int
test_mcslock(void)
{
	int ret = 0;
	int i;

	/* Define per core me node. */
	rte_mcslock_t ml_me;
	rte_mcslock_t ml_try_me;

	/*
	 * Test mcs lock & unlock on each core
	 */

	/* worker cores should be waiting: print it */
	RTE_LCORE_FOREACH_WORKER(i) {
		printf("lcore %d state: %d\n", i,
				(int) rte_eal_get_lcore_state(i));
	}

	rte_mcslock_lock(&p_ml, &ml_me);

	RTE_LCORE_FOREACH_WORKER(i) {
		rte_eal_remote_launch(test_mcslock_per_core, NULL, i);
	}

	/* worker cores should be busy: print it */
	RTE_LCORE_FOREACH_WORKER(i) {
		printf("lcore %d state: %d\n", i,
				(int) rte_eal_get_lcore_state(i));
	}

	rte_mcslock_unlock(&p_ml, &ml_me);

	rte_eal_mp_wait_lcore();

	/*
	 * Test if it could return immediately from try-locking a locked object.
	 * Here it will lock the mcs lock object first, then launch all the
	 * worker lcores to trylock the same mcs lock object.
	 * All the worker lcores should give up try-locking a locked object and
	 * return immediately, and then increase the "count" initialized with
	 * zero by one per times.
	 * We can check if the "count" is finally equal to the number of all
	 * worker lcores to see if the behavior of try-locking a locked
	 * mcslock object is correct.
	 */
	if (rte_mcslock_trylock(&p_ml_try, &ml_try_me) == 0)
		return -1;

	count = 0;
	RTE_LCORE_FOREACH_WORKER(i) {
		rte_eal_remote_launch(test_mcslock_try, NULL, i);
	}
	rte_eal_mp_wait_lcore();
	rte_mcslock_unlock(&p_ml_try, &ml_try_me);

	/* Test is_locked API */
	if (rte_mcslock_is_locked(p_ml)) {
		printf("mcslock is locked but it should not be\n");
		return -1;
	}

	/* Counting the locked times in each core */
	rte_mcslock_lock(&p_ml, &ml_me);
	if (count != (rte_lcore_count() - 1))
		ret = -1;
	rte_mcslock_unlock(&p_ml, &ml_me);

	/* mcs lock perf test */
	if (test_mcslock_perf() < 0)
		return -1;

	return ret;
}

REGISTER_FAST_TEST(mcslock_autotest, false, true, test_mcslock);
