/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_lcore.h>

#include "test.h"

/*
 * Atomic Variables
 * ================
 *
 * - The main test function performs three subtests. The first test
 *   checks that the usual inc/dec/add/sub functions are working
 *   correctly:
 *
 *   - Initialize 16-bit, 32-bit and 64-bit atomic variables to specific
 *     values.
 *
 *   - These variables are incremented and decremented on each core at
 *     the same time in ``test_atomic_usual()``.
 *
 *   - The function checks that once all lcores finish their function,
 *     the value of the atomic variables are still the same.
 *
 * - The second test verifies the behavior of "test and set" functions.
 *
 *   - Initialize 16-bit, 32-bit and 64-bit atomic variables to zero.
 *
 *   - Invoke ``test_atomic_tas()`` on each lcore: before doing anything
 *     else. The cores are waiting a synchro using ``while
 *     (rte_atomic32_read(&val) == 0)`` which is triggered by the main test
 *     function. Then all cores do a
 *     ``rte_atomicXX_test_and_set()`` at the same time. If it is successful,
 *     it increments another atomic counter.
 *
 *   - The main function checks that the atomic counter was incremented
 *     twice only (one for 16-bit, one for 32-bit and one for 64-bit values).
 *
 * - Test "add/sub and return"
 *
 *   - Initialize 16-bit, 32-bit and 64-bit atomic variables to zero.
 *
 *   - Invoke ``test_atomic_addsub_return()`` on each lcore. Before doing
 *     anything else, the cores are waiting a synchro. Each lcore does
 *     this operation several times::
 *
 *       tmp = rte_atomicXX_add_return(&a, 1);
 *       atomic_add(&count, tmp);
 *       tmp = rte_atomicXX_sub_return(&a, 1);
 *       atomic_sub(&count, tmp+1);
 *
 *   - At the end of the test, the *count* value must be 0.
 */

#define NUM_ATOMIC_TYPES 3

#define N 10000

static rte_atomic16_t a16;
static rte_atomic32_t a32;
static rte_atomic64_t a64;
static rte_atomic64_t count;
static rte_atomic32_t synchro;

static int
test_atomic_usual(__attribute__((unused)) void *arg)
{
	unsigned i;

	while (rte_atomic32_read(&synchro) == 0)
		;

	for (i = 0; i < N; i++)
		rte_atomic16_inc(&a16);
	for (i = 0; i < N; i++)
		rte_atomic16_dec(&a16);
	for (i = 0; i < (N / 5); i++)
		rte_atomic16_add(&a16, 5);
	for (i = 0; i < (N / 5); i++)
		rte_atomic16_sub(&a16, 5);

	for (i = 0; i < N; i++)
		rte_atomic32_inc(&a32);
	for (i = 0; i < N; i++)
		rte_atomic32_dec(&a32);
	for (i = 0; i < (N / 5); i++)
		rte_atomic32_add(&a32, 5);
	for (i = 0; i < (N / 5); i++)
		rte_atomic32_sub(&a32, 5);

	for (i = 0; i < N; i++)
		rte_atomic64_inc(&a64);
	for (i = 0; i < N; i++)
		rte_atomic64_dec(&a64);
	for (i = 0; i < (N / 5); i++)
		rte_atomic64_add(&a64, 5);
	for (i = 0; i < (N / 5); i++)
		rte_atomic64_sub(&a64, 5);

	return 0;
}

static int
test_atomic_tas(__attribute__((unused)) void *arg)
{
	while (rte_atomic32_read(&synchro) == 0)
		;

	if (rte_atomic16_test_and_set(&a16))
		rte_atomic64_inc(&count);
	if (rte_atomic32_test_and_set(&a32))
		rte_atomic64_inc(&count);
	if (rte_atomic64_test_and_set(&a64))
		rte_atomic64_inc(&count);

	return 0;
}

static int
test_atomic_addsub_and_return(__attribute__((unused)) void *arg)
{
	uint32_t tmp16;
	uint32_t tmp32;
	uint64_t tmp64;
	unsigned i;

	while (rte_atomic32_read(&synchro) == 0)
		;

	for (i = 0; i < N; i++) {
		tmp16 = rte_atomic16_add_return(&a16, 1);
		rte_atomic64_add(&count, tmp16);

		tmp16 = rte_atomic16_sub_return(&a16, 1);
		rte_atomic64_sub(&count, tmp16+1);

		tmp32 = rte_atomic32_add_return(&a32, 1);
		rte_atomic64_add(&count, tmp32);

		tmp32 = rte_atomic32_sub_return(&a32, 1);
		rte_atomic64_sub(&count, tmp32+1);

		tmp64 = rte_atomic64_add_return(&a64, 1);
		rte_atomic64_add(&count, tmp64);

		tmp64 = rte_atomic64_sub_return(&a64, 1);
		rte_atomic64_sub(&count, tmp64+1);
	}

	return 0;
}

/*
 * rte_atomic32_inc_and_test() would increase a 32 bits counter by one and then
 * test if that counter is equal to 0. It would return true if the counter is 0
 * and false if the counter is not 0. rte_atomic64_inc_and_test() could do the
 * same thing but for a 64 bits counter.
 * Here checks that if the 32/64 bits counter is equal to 0 after being atomically
 * increased by one. If it is, increase the variable of "count" by one which would
 * be checked as the result later.
 *
 */
static int
test_atomic_inc_and_test(__attribute__((unused)) void *arg)
{
	while (rte_atomic32_read(&synchro) == 0)
		;

	if (rte_atomic16_inc_and_test(&a16)) {
		rte_atomic64_inc(&count);
	}
	if (rte_atomic32_inc_and_test(&a32)) {
		rte_atomic64_inc(&count);
	}
	if (rte_atomic64_inc_and_test(&a64)) {
		rte_atomic64_inc(&count);
	}

	return 0;
}

/*
 * rte_atomicXX_dec_and_test() should decrease a 32 bits counter by one and then
 * test if that counter is equal to 0. It should return true if the counter is 0
 * and false if the counter is not 0.
 * This test checks if the counter is equal to 0 after being atomically
 * decreased by one. If it is, increase the value of "count" by one which is to
 * be checked as the result later.
 */
static int
test_atomic_dec_and_test(__attribute__((unused)) void *arg)
{
	while (rte_atomic32_read(&synchro) == 0)
		;

	if (rte_atomic16_dec_and_test(&a16))
		rte_atomic64_inc(&count);

	if (rte_atomic32_dec_and_test(&a32))
		rte_atomic64_inc(&count);

	if (rte_atomic64_dec_and_test(&a64))
		rte_atomic64_inc(&count);

	return 0;
}

static int
test_atomic(void)
{
	rte_atomic16_init(&a16);
	rte_atomic32_init(&a32);
	rte_atomic64_init(&a64);
	rte_atomic64_init(&count);
	rte_atomic32_init(&synchro);

	rte_atomic16_set(&a16, 1UL << 10);
	rte_atomic32_set(&a32, 1UL << 10);
	rte_atomic64_set(&a64, 1ULL << 33);

	printf("usual inc/dec/add/sub functions\n");

	rte_eal_mp_remote_launch(test_atomic_usual, NULL, SKIP_MASTER);
	rte_atomic32_set(&synchro, 1);
	rte_eal_mp_wait_lcore();
	rte_atomic32_set(&synchro, 0);

	if (rte_atomic16_read(&a16) != 1UL << 10) {
		printf("Atomic16 usual functions failed\n");
		return -1;
	}

	if (rte_atomic32_read(&a32) != 1UL << 10) {
		printf("Atomic32 usual functions failed\n");
		return -1;
	}

	if (rte_atomic64_read(&a64) != 1ULL << 33) {
		printf("Atomic64 usual functions failed\n");
		return -1;
	}

	printf("test and set\n");

	rte_atomic64_set(&a64, 0);
	rte_atomic32_set(&a32, 0);
	rte_atomic16_set(&a16, 0);
	rte_atomic64_set(&count, 0);
	rte_eal_mp_remote_launch(test_atomic_tas, NULL, SKIP_MASTER);
	rte_atomic32_set(&synchro, 1);
	rte_eal_mp_wait_lcore();
	rte_atomic32_set(&synchro, 0);

	if (rte_atomic64_read(&count) != NUM_ATOMIC_TYPES) {
		printf("Atomic test and set failed\n");
		return -1;
	}

	printf("add/sub and return\n");

	rte_atomic64_set(&a64, 0);
	rte_atomic32_set(&a32, 0);
	rte_atomic16_set(&a16, 0);
	rte_atomic64_set(&count, 0);
	rte_eal_mp_remote_launch(test_atomic_addsub_and_return, NULL,
				 SKIP_MASTER);
	rte_atomic32_set(&synchro, 1);
	rte_eal_mp_wait_lcore();
	rte_atomic32_set(&synchro, 0);

	if (rte_atomic64_read(&count) != 0) {
		printf("Atomic add/sub+return failed\n");
		return -1;
	}

	/*
	 * Set a64, a32 and a16 with the same value of minus "number of slave
	 * lcores", launch all slave lcores to atomically increase by one and
	 * test them respectively.
	 * Each lcore should have only one chance to increase a64 by one and
	 * then check if it is equal to 0, but there should be only one lcore
	 * that finds that it is 0. It is similar for a32 and a16.
	 * Then a variable of "count", initialized to zero, is increased by
	 * one if a64, a32 or a16 is 0 after being increased and tested
	 * atomically.
	 * We can check if "count" is finally equal to 3 to see if all slave
	 * lcores performed "atomic inc and test" right.
	 */
	printf("inc and test\n");

	rte_atomic64_clear(&a64);
	rte_atomic32_clear(&a32);
	rte_atomic16_clear(&a16);
	rte_atomic32_clear(&synchro);
	rte_atomic64_clear(&count);

	rte_atomic64_set(&a64, (int64_t)(1 - (int64_t)rte_lcore_count()));
	rte_atomic32_set(&a32, (int32_t)(1 - (int32_t)rte_lcore_count()));
	rte_atomic16_set(&a16, (int16_t)(1 - (int16_t)rte_lcore_count()));
	rte_eal_mp_remote_launch(test_atomic_inc_and_test, NULL, SKIP_MASTER);
	rte_atomic32_set(&synchro, 1);
	rte_eal_mp_wait_lcore();
	rte_atomic32_clear(&synchro);

	if (rte_atomic64_read(&count) != NUM_ATOMIC_TYPES) {
		printf("Atomic inc and test failed %d\n", (int)count.cnt);
		return -1;
	}

	/*
	 * Same as above, but this time we set the values to "number of slave
	 * lcores", and decrement instead of increment.
	 */
	printf("dec and test\n");

	rte_atomic32_clear(&synchro);
	rte_atomic64_clear(&count);

	rte_atomic64_set(&a64, (int64_t)(rte_lcore_count() - 1));
	rte_atomic32_set(&a32, (int32_t)(rte_lcore_count() - 1));
	rte_atomic16_set(&a16, (int16_t)(rte_lcore_count() - 1));
	rte_eal_mp_remote_launch(test_atomic_dec_and_test, NULL, SKIP_MASTER);
	rte_atomic32_set(&synchro, 1);
	rte_eal_mp_wait_lcore();
	rte_atomic32_clear(&synchro);

	if (rte_atomic64_read(&count) != NUM_ATOMIC_TYPES) {
		printf("Atomic dec and test failed\n");
		return -1;
	}

	return 0;
}

REGISTER_TEST_COMMAND(atomic_autotest, test_atomic);
