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
#include <rte_common.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_stdatomic.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_pause.h>

#include "test.h"

#ifndef RTE_TOOLCHAIN_MSVC
/*
 * The 128-bit compare and exchange is only implemented for
 * x86_64 and aarch64 with GCC/Clang
 */
#if defined(RTE_ARCH_X86_64) || defined(RTE_ARCH_ARM64)
/*
 * Atomic Variables
 * ================
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
 */

#define N_BASE 1000000u
#define N_MIN  10000u

/*
 * Number of iterations for each test, scaled inversely with core count.
 * More cores means more contention which increases time per operation.
 * Calculated once at test start to avoid repeated computation in workers.
 */
static unsigned int num_iterations;

static RTE_ATOMIC(uint32_t) synchro;

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

	while (rte_atomic_load_explicit(&synchro, rte_memory_order_relaxed) == 0)
		rte_pause();

	expected = count128;

	for (i = 0; i < num_iterations; i++) {
		do {
			rte_int128_t desired;

			desired.val[0] = expected.val[0] + 2;
			desired.val[1] = expected.val[1] + 1;

			success = rte_atomic128_cmp_exchange(&count128,
				&expected, &desired, 1,
				rte_memory_order_acquire, rte_memory_order_relaxed);
		} while (success == 0);

		do {
			rte_int128_t desired;

			desired.val[0] = expected.val[0] + 2;
			desired.val[1] = expected.val[1] + 1;

			success = rte_atomic128_cmp_exchange(&count128,
					&expected, &desired, 1,
					rte_memory_order_release, rte_memory_order_relaxed);
		} while (success == 0);

		do {
			rte_int128_t desired;

			desired.val[0] = expected.val[0] + 2;
			desired.val[1] = expected.val[1] + 1;

			success = rte_atomic128_cmp_exchange(&count128,
					&expected, &desired, 1,
					rte_memory_order_acq_rel, rte_memory_order_relaxed);
		} while (success == 0);

		do {
			rte_int128_t desired;

			desired.val[0] = expected.val[0] + 2;
			desired.val[1] = expected.val[1] + 1;

			success = rte_atomic128_cmp_exchange(&count128,
					&expected, &desired, 1,
					rte_memory_order_relaxed, rte_memory_order_relaxed);
		} while (success == 0);
	}

	return 0;
}

static int
test_atomic(void)
{
	uint64_t iterations;

	num_iterations = test_scale_iterations(N_BASE, N_MIN);

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

	rte_atomic_store_explicit(&synchro, 0, rte_memory_order_relaxed);
	count128.val[0] = 0;
	count128.val[1] = 0;

	rte_eal_mp_remote_launch(test_atomic128_cmp_exchange, NULL, SKIP_MAIN);
	rte_atomic_store_explicit(&synchro, 1, rte_memory_order_relaxed);
	rte_eal_mp_wait_lcore();
	rte_atomic_store_explicit(&synchro, 0, rte_memory_order_relaxed);

	iterations = count128.val[0] - count128.val[1];
	if (iterations != (uint64_t)4 * num_iterations * (rte_lcore_count() - 1)) {
		printf("128-bit compare and swap failed\n");
		return -1;
	}

	return 0;
}

#else

static int
test_atomic(void)
{
	printf("128 bit exchange not implemented\n");
	return TEST_SKIPPED;
}

#endif

REGISTER_FAST_TEST(atomic_autotest, NOHUGE_SKIP, ASAN_OK, test_atomic);
#endif /* RTE_TOOLCHAIN_MSVC */
