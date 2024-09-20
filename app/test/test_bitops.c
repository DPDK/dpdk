/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Arm Limited
 * Copyright(c) 2024 Ericsson AB
 */

#include <stdbool.h>

#include <rte_launch.h>
#include <rte_bitops.h>
#include <rte_random.h>
#include "test.h"

#define GEN_TEST_BIT_ACCESS(test_name, set_fun, clear_fun, assign_fun, flip_fun, test_fun, size) \
static int \
test_name(void) \
{ \
	uint ## size ## _t reference = (uint ## size ## _t)rte_rand(); \
	unsigned int bit_nr; \
	uint ## size ## _t word = (uint ## size ## _t)rte_rand(); \
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
		const uint ## size ## _t *const_ptr = &word; \
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
	rte_bit_test, 32)

GEN_TEST_BIT_ACCESS(test_bit_access64, rte_bit_set, rte_bit_clear, rte_bit_assign, rte_bit_flip,
	rte_bit_test, 64)

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

REGISTER_FAST_TEST(bitops_autotest, true, true, test_bitops);
