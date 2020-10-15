/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright(c) 2019 Arm Limited
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_random.h>
#include <rte_hash_crc.h>

#include "test.h"

/*
 * Atomic Variables
 * ================
 *
 * - The main test function performs several subtests. The first
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
 * - Test "test and set" functions.
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
 * - Test "add/sub and return" functions
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
 *
 * - Test "128-bit compare and swap" (aarch64 and x86_64 only)
 *
 *   - Initialize 128-bit atomic variables to zero.
 *
 *   - Invoke ``test_atomic128_cmp_exchange()`` on each lcore. Before doing
 *     anything else, the cores are waiting a synchro. Each lcore does
 *     these compare and swap (CAS) operations several times::
 *
 *       Acquired CAS update counter.val[0] + 2; counter.val[1] + 1;
 *       Released CAS update counter.val[0] + 2; counter.val[1] + 1;
 *       Acquired_Released CAS update counter.val[0] + 2; counter.val[1] + 1;
 *       Relaxed CAS update counter.val[0] + 2; counter.val[1] + 1;
 *
 *   - At the end of the test, the *count128* first 64-bit value and
 *     second 64-bit value differ by the total iterations.
 *
 * - Test "atomic exchange" functions
 *
 *   - Create a 64 bit token that can be tested for data integrity
 *
 *   - Invoke ``test_atomic_exchange`` on each lcore.  Before doing
 *     anything else, the cores wait for a synchronization event.
 *     Each core then does the follwoing for N iterations:
 *
 *       Generate a new token with a data integrity check
 *       Exchange the new token for previously generated token
 *       Increment a counter if a corrupt token was received
 *
 *   - At the end of the test, the number of corrupted tokens must be 0.
 */

#define NUM_ATOMIC_TYPES 3

#define N 1000000

static rte_atomic16_t a16;
static rte_atomic32_t a32;
static rte_atomic64_t a64;
static rte_atomic64_t count;
static rte_atomic32_t synchro;

static int
test_atomic_usual(__rte_unused void *arg)
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
test_atomic_tas(__rte_unused void *arg)
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
test_atomic_addsub_and_return(__rte_unused void *arg)
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
test_atomic_inc_and_test(__rte_unused void *arg)
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
test_atomic_dec_and_test(__rte_unused void *arg)
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

#if defined(RTE_ARCH_X86_64) || defined(RTE_ARCH_ARM64)
static rte_int128_t count128;

/*
 * rte_atomic128_cmp_exchange() should update a 128 bits counter's first 64
 * bits by 2 and the second 64 bits by 1 in this test. It should return true
 * if the compare exchange operation is successful.
 * This test repeats 128 bits compare and swap operations N rounds. In each
 * iteration it runs compare and swap operation with different memory models.
 */
static int
test_atomic128_cmp_exchange(__rte_unused void *arg)
{
	rte_int128_t expected;
	int success;
	unsigned int i;

	while (rte_atomic32_read(&synchro) == 0)
		;

	expected = count128;

	for (i = 0; i < N; i++) {
		do {
			rte_int128_t desired;

			desired.val[0] = expected.val[0] + 2;
			desired.val[1] = expected.val[1] + 1;

			success = rte_atomic128_cmp_exchange(&count128,
				&expected, &desired, 1,
				__ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
		} while (success == 0);

		do {
			rte_int128_t desired;

			desired.val[0] = expected.val[0] + 2;
			desired.val[1] = expected.val[1] + 1;

			success = rte_atomic128_cmp_exchange(&count128,
					&expected, &desired, 1,
					__ATOMIC_RELEASE, __ATOMIC_RELAXED);
		} while (success == 0);

		do {
			rte_int128_t desired;

			desired.val[0] = expected.val[0] + 2;
			desired.val[1] = expected.val[1] + 1;

			success = rte_atomic128_cmp_exchange(&count128,
					&expected, &desired, 1,
					__ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
		} while (success == 0);

		do {
			rte_int128_t desired;

			desired.val[0] = expected.val[0] + 2;
			desired.val[1] = expected.val[1] + 1;

			success = rte_atomic128_cmp_exchange(&count128,
					&expected, &desired, 1,
					__ATOMIC_RELAXED, __ATOMIC_RELAXED);
		} while (success == 0);
	}

	return 0;
}
#endif

/*
 * Helper definitions/variables/functions for
 * atomic exchange tests
 */
typedef union {
	uint16_t u16;
	uint8_t  u8[2];
} test16_t;

typedef union {
	uint32_t u32;
	uint16_t u16[2];
	uint8_t  u8[4];
} test32_t;

typedef union {
	uint64_t u64;
	uint32_t u32[2];
	uint16_t u16[4];
	uint8_t  u8[8];
} test64_t;

const uint8_t CRC8_POLY = 0x91;
uint8_t crc8_table[256];

volatile uint16_t token16;
volatile uint32_t token32;
volatile uint64_t token64;

static void
build_crc8_table(void)
{
	uint8_t val;
	int i, j;

	for (i = 0; i < 256; i++) {
		val = i;
		for (j = 0; j < 8; j++) {
			if (val & 1)
				val ^= CRC8_POLY;
			val >>= 1;
		}
		crc8_table[i] = val;
	}
}

static uint8_t
get_crc8(uint8_t *message, int length)
{
	uint8_t crc = 0;
	int i;

	for (i = 0; i < length; i++)
		crc = crc8_table[crc ^ message[i]];
	return crc;
}

/*
 * The atomic exchange test sets up a token in memory and
 * then spins up multiple lcores whose job is to generate
 * new tokens, exchange that new token for the old one held
 * in memory, and then verify that the old token is still
 * valid (i.e. the exchange did not corrupt the token).
 *
 * A token is made up of random data and 8 bits of crc
 * covering that random data.  The following is an example
 * of a 64bit token.
 *
 * +------------+------------+
 * | 63      56 | 55       0 |
 * +------------+------------+
 * |    CRC8    |    Data    |
 * +------------+------------+
 */
static int
test_atomic_exchange(__rte_unused void *arg)
{
	int i;
	test16_t nt16, ot16; /* new token, old token */
	test32_t nt32, ot32;
	test64_t nt64, ot64;

	/* Wait until all of the other threads have been dispatched */
	while (rte_atomic32_read(&synchro) == 0)
		;

	/*
	 * Let the battle begin! Every thread attempts to steal the current
	 * token with an atomic exchange operation and install its own newly
	 * generated token. If the old token is valid (i.e. it has the
	 * appropriate crc32 hash for the data) then the test iteration has
	 * passed.  If the token is invalid, increment the counter.
	 */
	for (i = 0; i < N; i++) {

		/* Test 64bit Atomic Exchange */
		nt64.u64 = rte_rand();
		nt64.u8[7] = get_crc8(&nt64.u8[0], sizeof(nt64) - 1);
		ot64.u64 = rte_atomic64_exchange(&token64, nt64.u64);
		if (ot64.u8[7] != get_crc8(&ot64.u8[0], sizeof(ot64) - 1))
			rte_atomic64_inc(&count);

		/* Test 32bit Atomic Exchange */
		nt32.u32 = (uint32_t)rte_rand();
		nt32.u8[3] = get_crc8(&nt32.u8[0], sizeof(nt32) - 1);
		ot32.u32 = rte_atomic32_exchange(&token32, nt32.u32);
		if (ot32.u8[3] != get_crc8(&ot32.u8[0], sizeof(ot32) - 1))
			rte_atomic64_inc(&count);

		/* Test 16bit Atomic Exchange */
		nt16.u16 = (uint16_t)rte_rand();
		nt16.u8[1] = get_crc8(&nt16.u8[0], sizeof(nt16) - 1);
		ot16.u16 = rte_atomic16_exchange(&token16, nt16.u16);
		if (ot16.u8[1] != get_crc8(&ot16.u8[0], sizeof(ot16) - 1))
			rte_atomic64_inc(&count);
	}

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

	rte_eal_mp_remote_launch(test_atomic_usual, NULL, SKIP_MAIN);
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
	rte_eal_mp_remote_launch(test_atomic_tas, NULL, SKIP_MAIN);
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
				 SKIP_MAIN);
	rte_atomic32_set(&synchro, 1);
	rte_eal_mp_wait_lcore();
	rte_atomic32_set(&synchro, 0);

	if (rte_atomic64_read(&count) != 0) {
		printf("Atomic add/sub+return failed\n");
		return -1;
	}

	/*
	 * Set a64, a32 and a16 with the same value of minus "number of worker
	 * lcores", launch all worker lcores to atomically increase by one and
	 * test them respectively.
	 * Each lcore should have only one chance to increase a64 by one and
	 * then check if it is equal to 0, but there should be only one lcore
	 * that finds that it is 0. It is similar for a32 and a16.
	 * Then a variable of "count", initialized to zero, is increased by
	 * one if a64, a32 or a16 is 0 after being increased and tested
	 * atomically.
	 * We can check if "count" is finally equal to 3 to see if all worker
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
	rte_eal_mp_remote_launch(test_atomic_inc_and_test, NULL, SKIP_MAIN);
	rte_atomic32_set(&synchro, 1);
	rte_eal_mp_wait_lcore();
	rte_atomic32_clear(&synchro);

	if (rte_atomic64_read(&count) != NUM_ATOMIC_TYPES) {
		printf("Atomic inc and test failed %d\n", (int)count.cnt);
		return -1;
	}

	/*
	 * Same as above, but this time we set the values to "number of worker
	 * lcores", and decrement instead of increment.
	 */
	printf("dec and test\n");

	rte_atomic32_clear(&synchro);
	rte_atomic64_clear(&count);

	rte_atomic64_set(&a64, (int64_t)(rte_lcore_count() - 1));
	rte_atomic32_set(&a32, (int32_t)(rte_lcore_count() - 1));
	rte_atomic16_set(&a16, (int16_t)(rte_lcore_count() - 1));
	rte_eal_mp_remote_launch(test_atomic_dec_and_test, NULL, SKIP_MAIN);
	rte_atomic32_set(&synchro, 1);
	rte_eal_mp_wait_lcore();
	rte_atomic32_clear(&synchro);

	if (rte_atomic64_read(&count) != NUM_ATOMIC_TYPES) {
		printf("Atomic dec and test failed\n");
		return -1;
	}

#if defined(RTE_ARCH_X86_64) || defined(RTE_ARCH_ARM64)
	/*
	 * This case tests the functionality of rte_atomic128_cmp_exchange
	 * API. It calls rte_atomic128_cmp_exchange with four kinds of memory
	 * models successively on each worker core. Once each 128-bit atomic
	 * compare and swap operation is successful, it updates the global
	 * 128-bit counter by 2 for the first 64-bit and 1 for the second
	 * 64-bit. Each worker core iterates this test N times.
	 * At the end of test, verify whether the first 64-bits of the 128-bit
	 * counter and the second 64bits is differ by the total iterations. If
	 * it is, the test passes.
	 */
	printf("128-bit compare and swap test\n");
	uint64_t iterations = 0;

	rte_atomic32_clear(&synchro);
	count128.val[0] = 0;
	count128.val[1] = 0;

	rte_eal_mp_remote_launch(test_atomic128_cmp_exchange, NULL,
				 SKIP_MAIN);
	rte_atomic32_set(&synchro, 1);
	rte_eal_mp_wait_lcore();
	rte_atomic32_clear(&synchro);

	iterations = count128.val[0] - count128.val[1];
	if (iterations != 4*N*(rte_lcore_count()-1)) {
		printf("128-bit compare and swap failed\n");
		return -1;
	}
#endif

	/*
	 * Test 16/32/64bit atomic exchange.
	 */
	test64_t t;

	printf("exchange test\n");

	rte_atomic32_clear(&synchro);
	rte_atomic64_clear(&count);

	/* Generate the CRC8 lookup table */
	build_crc8_table();

	/* Create the initial tokens used by the test */
	t.u64 = rte_rand();
	token16 = (get_crc8(&t.u8[0], sizeof(token16) - 1) << 8)
		| (t.u16[0] & 0x00ff);
	token32 = ((uint32_t)get_crc8(&t.u8[0], sizeof(token32) - 1) << 24)
		| (t.u32[0] & 0x00ffffff);
	token64 = ((uint64_t)get_crc8(&t.u8[0], sizeof(token64) - 1) << 56)
		| (t.u64 & 0x00ffffffffffffff);

	rte_eal_mp_remote_launch(test_atomic_exchange, NULL, SKIP_MAIN);
	rte_atomic32_set(&synchro, 1);
	rte_eal_mp_wait_lcore();
	rte_atomic32_clear(&synchro);

	if (rte_atomic64_read(&count) > 0) {
		printf("Atomic exchange test failed\n");
		return -1;
	}

	return 0;
}
REGISTER_TEST_COMMAND(atomic_autotest, test_atomic);
