/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Arm Limited
 * Copyright(c) 2024 Ericsson AB
 */

#include <inttypes.h>
#include <stdbool.h>

#include <rte_bitops.h>
#include <rte_cycles.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_random.h>
#include "test.h"

static unsigned int
get_worker_lcore(void)
{
	unsigned int lcore_id = rte_get_next_lcore(-1, 1, 0);

	/* avoid checkers (like Coverity) false positives */
	RTE_VERIFY(lcore_id < RTE_MAX_LCORE);

	return lcore_id;
}

#define GEN_TEST_BIT_ACCESS(test_name, set_fun, clear_fun, assign_fun, flip_fun, test_fun, size, \
		mod) \
static int \
test_name(void) \
{ \
	uint ## size ## _t reference = (uint ## size ## _t)rte_rand(); \
	unsigned int bit_nr; \
	mod uint ## size ## _t word = (uint ## size ## _t)rte_rand(); \
	for (bit_nr = 0; bit_nr < size; bit_nr++) { \
		bool reference_bit = (reference >> bit_nr) & 1; \
		bool assign = rte_rand() & 1; \
		if (assign) { \
			assign_fun(&word, bit_nr, reference_bit); \
		} else { \
			if (reference_bit) \
				set_fun(&word, bit_nr); \
			else \
				clear_fun(&word, bit_nr); \
		} \
		TEST_ASSERT(test_fun(&word, bit_nr) == reference_bit, \
			"Bit %d had unexpected value", bit_nr); \
		flip_fun(&word, bit_nr); \
		TEST_ASSERT(test_fun(&word, bit_nr) != reference_bit, \
			"Bit %d had unflipped value", bit_nr); \
		flip_fun(&word, bit_nr); \
		const mod uint ## size ## _t *const_ptr = &word; \
		TEST_ASSERT(test_fun(const_ptr, bit_nr) == reference_bit, \
			"Bit %d had unexpected value", bit_nr); \
	} \
	for (bit_nr = 0; bit_nr < size; bit_nr++) { \
		bool reference_bit = (reference >> bit_nr) & 1; \
		TEST_ASSERT(test_fun(&word, bit_nr) == reference_bit, \
			"Bit %d had unexpected value", bit_nr); \
	} \
	TEST_ASSERT(reference == word, "Word had unexpected value"); \
	return TEST_SUCCESS; \
}

GEN_TEST_BIT_ACCESS(test_bit_access32, rte_bit_set, rte_bit_clear, rte_bit_assign, rte_bit_flip,
	rte_bit_test, 32,)

GEN_TEST_BIT_ACCESS(test_bit_access64, rte_bit_set, rte_bit_clear, rte_bit_assign, rte_bit_flip,
	rte_bit_test, 64,)

GEN_TEST_BIT_ACCESS(test_bit_v_access32, rte_bit_set, rte_bit_clear, rte_bit_assign, rte_bit_flip,
	rte_bit_test, 32, volatile)

GEN_TEST_BIT_ACCESS(test_bit_v_access64, rte_bit_set, rte_bit_clear, rte_bit_assign, rte_bit_flip,
	rte_bit_test, 64, volatile)

#define bit_atomic_set(addr, nr) \
	rte_bit_atomic_set(addr, nr, rte_memory_order_relaxed)

#define bit_atomic_clear(addr, nr) \
	rte_bit_atomic_clear(addr, nr, rte_memory_order_relaxed)

#define bit_atomic_assign(addr, nr, value) \
	rte_bit_atomic_assign(addr, nr, value, rte_memory_order_relaxed)

#define bit_atomic_flip(addr, nr) \
	rte_bit_atomic_flip(addr, nr, rte_memory_order_relaxed)

#define bit_atomic_test(addr, nr) \
	rte_bit_atomic_test(addr, nr, rte_memory_order_relaxed)

GEN_TEST_BIT_ACCESS(test_bit_atomic_access32, bit_atomic_set, bit_atomic_clear, bit_atomic_assign,
	bit_atomic_flip, bit_atomic_test, 32,)

GEN_TEST_BIT_ACCESS(test_bit_atomic_access64, bit_atomic_set, bit_atomic_clear, bit_atomic_assign,
	bit_atomic_flip, bit_atomic_test, 64,)

GEN_TEST_BIT_ACCESS(test_bit_atomic_v_access32, bit_atomic_set, bit_atomic_clear, bit_atomic_assign,
	bit_atomic_flip, bit_atomic_test, 32, volatile)

GEN_TEST_BIT_ACCESS(test_bit_atomic_v_access64, bit_atomic_set, bit_atomic_clear, bit_atomic_assign,
	bit_atomic_flip, bit_atomic_test, 64, volatile)

#define PARALLEL_TEST_RUNTIME 0.25

#define GEN_TEST_BIT_PARALLEL_ASSIGN(size) \
struct parallel_access_lcore ## size \
{ \
	unsigned int bit; \
	uint ## size ##_t *word; \
	bool failed; \
}; \
static int \
run_parallel_assign ## size(void *arg) \
{ \
	struct parallel_access_lcore ## size *lcore = arg; \
	uint64_t deadline = rte_get_timer_cycles() + PARALLEL_TEST_RUNTIME * rte_get_timer_hz(); \
	bool value = false; \
	do { \
		bool new_value = rte_rand() & 1; \
		bool use_test_and_modify = rte_rand() & 1; \
		bool use_assign = rte_rand() & 1; \
		if (rte_bit_atomic_test(lcore->word, lcore->bit, \
					rte_memory_order_relaxed) != value) { \
			lcore->failed = true; \
			break; \
		} \
		if (use_test_and_modify) { \
			bool old_value; \
			if (use_assign) { \
				old_value = rte_bit_atomic_test_and_assign(lcore->word, \
					lcore->bit, new_value, rte_memory_order_relaxed); \
			} else { \
				old_value = new_value ? \
					rte_bit_atomic_test_and_set(lcore->word, lcore->bit, \
						rte_memory_order_relaxed) : \
					rte_bit_atomic_test_and_clear(lcore->word, lcore->bit, \
						rte_memory_order_relaxed); \
			} \
			if (old_value != value) { \
				lcore->failed = true; \
				break; \
			} \
		} else { \
			if (use_assign) { \
				rte_bit_atomic_assign(lcore->word, lcore->bit, new_value, \
					rte_memory_order_relaxed); \
			} else { \
				if (new_value) \
					rte_bit_atomic_set(lcore->word, lcore->bit, \
						rte_memory_order_relaxed); \
				else \
					rte_bit_atomic_clear(lcore->word, lcore->bit, \
						rte_memory_order_relaxed); \
			} \
		} \
		value = new_value; \
	} while (rte_get_timer_cycles() < deadline); \
	return 0; \
} \
static int \
test_bit_atomic_parallel_assign ## size(void) \
{ \
	unsigned int worker_lcore_id; \
	uint ## size ## _t word = 0; \
	struct parallel_access_lcore ## size lmain = { .word = &word }; \
	struct parallel_access_lcore ## size lworker = { .word = &word }; \
	if (rte_lcore_count() < 2) { \
		printf("Need multiple cores to run parallel test.\n"); \
		return TEST_SKIPPED; \
	} \
	worker_lcore_id = get_worker_lcore(); \
	lmain.bit = rte_rand_max(size); \
	do { \
		lworker.bit = rte_rand_max(size); \
	} while (lworker.bit == lmain.bit); \
	int rc = rte_eal_remote_launch(run_parallel_assign ## size, &lworker, worker_lcore_id); \
	TEST_ASSERT(rc == 0, "Worker thread launch failed"); \
	run_parallel_assign ## size(&lmain); \
	rte_eal_mp_wait_lcore(); \
	TEST_ASSERT(!lmain.failed, "Main lcore atomic access failed"); \
	TEST_ASSERT(!lworker.failed, "Worker lcore atomic access failed"); \
	return TEST_SUCCESS; \
}

GEN_TEST_BIT_PARALLEL_ASSIGN(32)
GEN_TEST_BIT_PARALLEL_ASSIGN(64)

#define GEN_TEST_BIT_PARALLEL_TEST_AND_MODIFY(size) \
struct parallel_test_and_set_lcore ## size \
{ \
	uint ## size ##_t *word; \
	unsigned int bit; \
	uint64_t flips; \
}; \
static int \
run_parallel_test_and_modify ## size(void *arg) \
{ \
	struct parallel_test_and_set_lcore ## size *lcore = arg; \
	uint64_t deadline = rte_get_timer_cycles() + PARALLEL_TEST_RUNTIME * rte_get_timer_hz(); \
	do { \
		bool old_value; \
		bool new_value = rte_rand() & 1; \
		bool use_assign = rte_rand() & 1; \
		if (use_assign) \
			old_value = rte_bit_atomic_test_and_assign(lcore->word, lcore->bit, \
				new_value, rte_memory_order_relaxed); \
		else \
			old_value = new_value ? \
				rte_bit_atomic_test_and_set(lcore->word, lcore->bit, \
					rte_memory_order_relaxed) : \
				rte_bit_atomic_test_and_clear(lcore->word, lcore->bit, \
					rte_memory_order_relaxed); \
		if (old_value != new_value) \
			lcore->flips++; \
	} while (rte_get_timer_cycles() < deadline); \
	return 0; \
} \
static int \
test_bit_atomic_parallel_test_and_modify ## size(void) \
{ \
	unsigned int worker_lcore_id; \
	uint ## size ## _t word = 0; \
	unsigned int bit = rte_rand_max(size); \
	struct parallel_test_and_set_lcore ## size lmain = { .word = &word, .bit = bit }; \
	struct parallel_test_and_set_lcore ## size lworker = { .word = &word, .bit = bit }; \
	if (rte_lcore_count() < 2) { \
		printf("Need multiple cores to run parallel test.\n"); \
		return TEST_SKIPPED; \
	} \
	worker_lcore_id = get_worker_lcore(); \
	int rc = rte_eal_remote_launch(run_parallel_test_and_modify ## size, &lworker, \
		worker_lcore_id); \
	TEST_ASSERT(rc == 0, "Worker thread launch failed"); \
	run_parallel_test_and_modify ## size(&lmain); \
	rte_eal_mp_wait_lcore(); \
	uint64_t total_flips = lmain.flips + lworker.flips; \
	bool expected_value = total_flips % 2; \
	TEST_ASSERT(expected_value == rte_bit_test(&word, bit), \
		"After %"PRId64" flips, the bit value should be %d", total_flips, expected_value); \
	uint ## size ## _t expected_word = 0; \
	rte_bit_assign(&expected_word, bit, expected_value); \
	TEST_ASSERT(expected_word == word, "Untouched bits have changed value"); \
	return TEST_SUCCESS; \
}

GEN_TEST_BIT_PARALLEL_TEST_AND_MODIFY(32)
GEN_TEST_BIT_PARALLEL_TEST_AND_MODIFY(64)

#define GEN_TEST_BIT_PARALLEL_FLIP(size) \
struct parallel_flip_lcore ## size \
{ \
	uint ## size ##_t *word; \
	unsigned int bit; \
	uint64_t flips; \
}; \
static int \
run_parallel_flip ## size(void *arg) \
{ \
	struct parallel_flip_lcore ## size *lcore = arg; \
	uint64_t deadline = rte_get_timer_cycles() + PARALLEL_TEST_RUNTIME * rte_get_timer_hz(); \
	do { \
		rte_bit_atomic_flip(lcore->word, lcore->bit, rte_memory_order_relaxed); \
		lcore->flips++; \
	} while (rte_get_timer_cycles() < deadline); \
	return 0; \
} \
static int \
test_bit_atomic_parallel_flip ## size(void) \
{ \
	unsigned int worker_lcore_id; \
	uint ## size ## _t word = 0; \
	unsigned int bit = rte_rand_max(size); \
	struct parallel_flip_lcore ## size lmain = { .word = &word, .bit = bit }; \
	struct parallel_flip_lcore ## size lworker = { .word = &word, .bit = bit }; \
	if (rte_lcore_count() < 2) { \
		printf("Need multiple cores to run parallel test.\n"); \
		return TEST_SKIPPED; \
	} \
	worker_lcore_id = get_worker_lcore(); \
	int rc = rte_eal_remote_launch(run_parallel_flip ## size, &lworker, worker_lcore_id); \
	TEST_ASSERT(rc == 0, "Worker thread launch failed"); \
	run_parallel_flip ## size(&lmain); \
	rte_eal_mp_wait_lcore(); \
	uint64_t total_flips = lmain.flips + lworker.flips; \
	bool expected_value = total_flips % 2; \
	TEST_ASSERT(expected_value == rte_bit_test(&word, bit), \
		"After %"PRId64" flips, the bit value should be %d", total_flips, expected_value); \
	uint ## size ## _t expected_word = 0; \
	rte_bit_assign(&expected_word, bit, expected_value); \
	TEST_ASSERT(expected_word == word, "Untouched bits have changed value"); \
	return TEST_SUCCESS; \
}

GEN_TEST_BIT_PARALLEL_FLIP(32)
GEN_TEST_BIT_PARALLEL_FLIP(64)

static uint32_t val32;
static uint64_t val64;

#define MAX_BITS_32 32
#define MAX_BITS_64 64

/*
 * Bitops functions
 * ================
 *
 * - The main test function performs several subtests.
 * - Check bit operations on one core.
 *   - Initialize valXX to specified values, then set each bit of valXX
 *     to 1 one by one in "test_bit_relaxed_set".
 *
 *   - Clear each bit of valXX to 0 one by one in "test_bit_relaxed_clear".
 *
 *   - Function "test_bit_relaxed_test_set_clear" checks whether each bit
 *     of valXX can do "test and set" and "test and clear" correctly.
 */

static int
test_bit_relaxed_set(void)
{
	unsigned int i;

	for (i = 0; i < MAX_BITS_32; i++)
		rte_bit_relaxed_set32(i, &val32);

	for (i = 0; i < MAX_BITS_32; i++)
		if (!rte_bit_relaxed_get32(i, &val32)) {
			printf("Failed to set bit in relaxed version.\n");
			return TEST_FAILED;
		}

	for (i = 0; i < MAX_BITS_64; i++)
		rte_bit_relaxed_set64(i, &val64);

	for (i = 0; i < MAX_BITS_64; i++)
		if (!rte_bit_relaxed_get64(i, &val64)) {
			printf("Failed to set bit in relaxed version.\n");
			return TEST_FAILED;
		}

	return TEST_SUCCESS;
}

static int
test_bit_relaxed_clear(void)
{
	unsigned int i;

	for (i = 0; i < MAX_BITS_32; i++)
		rte_bit_relaxed_clear32(i, &val32);

	for (i = 0; i < MAX_BITS_32; i++)
		if (rte_bit_relaxed_get32(i, &val32)) {
			printf("Failed to clear bit in relaxed version.\n");
			return TEST_FAILED;
		}

	for (i = 0; i < MAX_BITS_64; i++)
		rte_bit_relaxed_clear64(i, &val64);

	for (i = 0; i < MAX_BITS_64; i++)
		if (rte_bit_relaxed_get64(i, &val64)) {
			printf("Failed to clear bit in relaxed version.\n");
			return TEST_FAILED;
		}

	return TEST_SUCCESS;
}

static int
test_bit_relaxed_test_set_clear(void)
{
	unsigned int i;

	for (i = 0; i < MAX_BITS_32; i++)
		rte_bit_relaxed_test_and_set32(i, &val32);

	for (i = 0; i < MAX_BITS_32; i++)
		if (!rte_bit_relaxed_test_and_clear32(i, &val32)) {
			printf("Failed to set and test bit in relaxed version.\n");
			return TEST_FAILED;
	}

	for (i = 0; i < MAX_BITS_32; i++)
		if (rte_bit_relaxed_get32(i, &val32)) {
			printf("Failed to test and clear bit in relaxed version.\n");
			return TEST_FAILED;
		}

	for (i = 0; i < MAX_BITS_64; i++)
		rte_bit_relaxed_test_and_set64(i, &val64);

	for (i = 0; i < MAX_BITS_64; i++)
		if (!rte_bit_relaxed_test_and_clear64(i, &val64)) {
			printf("Failed to set and test bit in relaxed version.\n");
			return TEST_FAILED;
		}

	for (i = 0; i < MAX_BITS_64; i++)
		if (rte_bit_relaxed_get64(i, &val64)) {
			printf("Failed to test and clear bit in relaxed version.\n");
			return TEST_FAILED;
		}

	return TEST_SUCCESS;
}

static struct unit_test_suite test_suite = {
	.suite_name = "Bitops test suite",
	.unit_test_cases = {
		TEST_CASE(test_bit_access32),
		TEST_CASE(test_bit_access64),
		TEST_CASE(test_bit_access32),
		TEST_CASE(test_bit_access64),
		TEST_CASE(test_bit_v_access32),
		TEST_CASE(test_bit_v_access64),
		TEST_CASE(test_bit_atomic_access32),
		TEST_CASE(test_bit_atomic_access64),
		TEST_CASE(test_bit_atomic_v_access32),
		TEST_CASE(test_bit_atomic_v_access64),
		TEST_CASE(test_bit_atomic_parallel_assign32),
		TEST_CASE(test_bit_atomic_parallel_assign64),
		TEST_CASE(test_bit_atomic_parallel_test_and_modify32),
		TEST_CASE(test_bit_atomic_parallel_test_and_modify64),
		TEST_CASE(test_bit_atomic_parallel_flip32),
		TEST_CASE(test_bit_atomic_parallel_flip64),
		TEST_CASE(test_bit_relaxed_set),
		TEST_CASE(test_bit_relaxed_clear),
		TEST_CASE(test_bit_relaxed_test_set_clear),
		TEST_CASES_END()
	}
};

static int
test_bitops(void)
{
	return unit_test_suite_runner(&test_suite);
}

REGISTER_FAST_TEST(bitops_autotest, NOHUGE_OK, ASAN_OK, test_bitops);
