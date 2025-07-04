/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <inttypes.h>
#include <stdlib.h>

#include <rte_bitset.h>
#include <rte_random.h>

#include "test.h"

#define MAGIC UINT64_C(0xdeadbeefdeadbeef)

static void
rand_buf(void *buf, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		((unsigned char *)buf)[i] = rte_rand();
}

static uint64_t *
alloc_bitset(size_t size)
{
	uint64_t *p;

	p = malloc(RTE_BITSET_SIZE(size) + 2 * sizeof(uint64_t));
	if (p == NULL)
		rte_panic("Unable to allocate memory\n");

	rand_buf(&p[0], RTE_BITSET_SIZE(size));

	p[0] = MAGIC;
	p[RTE_BITSET_NUM_WORDS(size) + 1] = MAGIC;

	return p + 1;
}


static int
free_bitset(uint64_t *bitset, size_t size)
{
	uint64_t *p;

	p = bitset - 1;

	if (p[0] != MAGIC)
		return TEST_FAILED;

	if (p[RTE_BITSET_NUM_WORDS(size) + 1] != MAGIC)
		return TEST_FAILED;

	free(p);

	return TEST_SUCCESS;
}

static bool
rand_bool(void)
{
	return rte_rand_max(2);
}

static void
rand_bool_ary(bool *ary, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		ary[i] = rand_bool();
}

static void
rand_unused_bits(uint64_t *bitset, size_t size)
{
	uint64_t bits = rte_rand() & ~__RTE_BITSET_USED_MASK(size);

	bitset[RTE_BITSET_NUM_WORDS(size) - 1] |= bits;
}

static void
rand_bitset(uint64_t *bitset, size_t size)
{
	size_t i;

	rte_bitset_init(bitset, size);

	for (i = 0; i < size; i++)
		rte_bitset_assign(bitset, i, rand_bool());

	rand_unused_bits(bitset, size);
}

typedef bool test_fun(const uint64_t *bitset, size_t bit_num);
typedef void set_fun(uint64_t *bitset, size_t bit_num);
typedef void clear_fun(uint64_t *bitset, size_t bit_num);
typedef void assign_fun(uint64_t *bitset, size_t bit_num, bool value);
typedef void flip_fun(uint64_t *bitset, size_t bit_num);

#define RAND_SET_MAX_SIZE 1000

static int
test_set_clear_size(test_fun test_fun, set_fun set_fun, clear_fun clear_fun, size_t size)
{
	size_t i;
	bool reference[RAND_SET_MAX_SIZE];
	uint64_t *bitset;

	rand_bool_ary(reference, size);

	bitset = alloc_bitset(size);

	TEST_ASSERT(bitset != NULL, "Failed to allocate memory");

	rte_bitset_init(bitset, size);

	for (i = 0; i < size; i++) {
		if (reference[i])
			set_fun(bitset, i);
		else
			clear_fun(bitset, i);
	}

	for (i = 0; i < size; i++)
		if (reference[i] != test_fun(bitset, i))
			return TEST_FAILED;

	TEST_ASSERT_EQUAL(free_bitset(bitset, size), TEST_SUCCESS,
		"Buffer over- or underrun detected");

	return TEST_SUCCESS;
}

#define RAND_ITERATIONS 10000

static int
test_set_clear_fun(test_fun test_fun, set_fun set_fun, clear_fun clear_fun)
{
	size_t i;

	for (i = 0; i < RAND_ITERATIONS; i++) {
		size_t size = 1 + rte_rand_max(RAND_SET_MAX_SIZE - 1);

		if (test_set_clear_size(test_fun, set_fun, clear_fun, size) != TEST_SUCCESS)
			return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
test_set_clear(void)
{
	return test_set_clear_fun(rte_bitset_test, rte_bitset_set, rte_bitset_clear);
}

static int
test_flip_size(test_fun test_fun, assign_fun assign_fun, flip_fun flip_fun, size_t size)
{
	size_t i;
	uint64_t *bitset;

	bitset = alloc_bitset(size);

	TEST_ASSERT(bitset != NULL, "Failed to allocate memory");

	rand_bitset(bitset, size);

	for (i = 0; i < size; i++) {
		uint64_t *reference = alloca(RTE_BITSET_SIZE(size));

		rte_bitset_copy(reference, bitset, size);

		bool value = test_fun(bitset, i);

		flip_fun(bitset, i);

		TEST_ASSERT(test_fun(bitset, i) != value, "Bit %zd was not flipped", i);

		assign_fun(reference, i, !value);

		TEST_ASSERT(rte_bitset_equal(bitset, reference, size),
			"Not only the target bit %zd was flipped", i);


	}

	TEST_ASSERT_EQUAL(free_bitset(bitset, size), TEST_SUCCESS,
		"Buffer over- or underrun detected");

	return TEST_SUCCESS;
}

static int
test_flip_fun(test_fun test_fun, assign_fun assign_fun, flip_fun flip_fun)
{
	size_t i;

	for (i = 0; i < RAND_ITERATIONS; i++) {
		size_t size = 1 + rte_rand_max(RAND_SET_MAX_SIZE - 1);

		if (test_flip_size(test_fun, assign_fun, flip_fun, size) != TEST_SUCCESS)
			return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
test_flip(void)
{
	return test_flip_fun(rte_bitset_test, rte_bitset_assign, rte_bitset_flip);
}

static bool
bitset_atomic_test(const uint64_t *bitset, size_t bit_num)
{
	return rte_bitset_atomic_test(bitset, bit_num, rte_memory_order_relaxed);
}

static void
bitset_atomic_set(uint64_t *bitset, size_t bit_num)
{
	rte_bitset_atomic_set(bitset, bit_num, rte_memory_order_relaxed);
}

static void
bitset_atomic_clear(uint64_t *bitset, size_t bit_num)
{
	rte_bitset_atomic_clear(bitset, bit_num, rte_memory_order_relaxed);
}

static void
bitset_atomic_flip(uint64_t *bitset, size_t bit_num)
{
	rte_bitset_atomic_flip(bitset, bit_num, rte_memory_order_relaxed);
}

static void
bitset_atomic_assign(uint64_t *bitset, size_t bit_num, bool bit_value)
{
	rte_bitset_atomic_assign(bitset, bit_num, bit_value, rte_memory_order_relaxed);
}

static int
test_atomic_set_clear(void)
{
	return test_set_clear_fun(bitset_atomic_test, bitset_atomic_set, bitset_atomic_clear);
}

static int
test_atomic_flip(void)
{
	return test_flip_fun(bitset_atomic_test, bitset_atomic_assign, bitset_atomic_flip);
}

static ssize_t
find(const bool *ary, size_t num_bools, size_t start, size_t len, bool set)
{
	size_t i;

	for (i = 0; i < len; i++) {
		ssize_t idx = (start + i) % num_bools;

		if (ary[idx] == set)
			return idx;
	}

	return -1;
}

static ssize_t
find_set(const bool *ary, size_t num_bools, size_t start, size_t len)
{
	return find(ary, num_bools, start, len, true);
}

static ssize_t
find_clear(const bool *ary, size_t num_bools, size_t start, size_t len)
{
	return find(ary, num_bools, start, len, false);
}

#define FFS_ITERATIONS 100

static int
test_find_size(size_t size, bool set)
{
	uint64_t *bitset;
	bool reference[RAND_SET_MAX_SIZE];
	size_t i;

	bitset = alloc_bitset(size);

	TEST_ASSERT(bitset != NULL, "Failed to allocate memory");

	rte_bitset_init(bitset, size);

	for (i = 0; i < size; i++) {
		bool bit = rand_bool();
		reference[i] = bit;

		if (bit)
			rte_bitset_set(bitset, i);
		else /* redundant, still useful for testing */
			rte_bitset_clear(bitset, i);
	}

	for (i = 0; i < FFS_ITERATIONS; i++) {
		size_t start_bit = rte_rand_max(size);
		size_t len = rte_rand_max(size + 1);
		bool full_range = len == size && start_bit == 0;
		bool wraps = start_bit + len > size;
		ssize_t rc;

		if (set) {
			if (full_range && rand_bool())
				rc = rte_bitset_find_first_set(bitset, size);
			else if (wraps || rand_bool())
				rc = rte_bitset_find_set_wrap(bitset, size, start_bit, len);
			else
				rc = rte_bitset_find_set(bitset, size, start_bit, len);

			if (rc != find_set(reference, size, start_bit, len))
				return TEST_FAILED;
		} else {
			if (full_range && rand_bool())
				rc = rte_bitset_find_first_clear(bitset, size);
			else if (wraps || rand_bool())
				rc = rte_bitset_find_clear_wrap(bitset, size, start_bit, len);
			else
				rc = rte_bitset_find_clear(bitset, size, start_bit, len);

			if (rc != find_clear(reference, size, start_bit, len))
				return TEST_FAILED;
		}

	}

	TEST_ASSERT_EQUAL(free_bitset(bitset, size), TEST_SUCCESS,
		"Buffer over- or underrun detected");

	return TEST_SUCCESS;
}

static int
test_find_set_size(size_t size)
{
	return test_find_size(size, true);
}

static int
test_find_clear_size(size_t size)
{
	return test_find_size(size, false);
}

static int
test_find(void)
{
	size_t i;

	for (i = 0; i < RAND_ITERATIONS; i++) {
		size_t size = 2 + rte_rand_max(RAND_SET_MAX_SIZE - 2);

		if (test_find_set_size(size) != TEST_SUCCESS)
			return TEST_FAILED;

		if (test_find_clear_size(size) != TEST_SUCCESS)
			return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
record_match(ssize_t match_idx, size_t size, int *calls)
{
	if (match_idx < 0 || (size_t)match_idx >= size)
		return TEST_FAILED;

	calls[match_idx]++;

	return TEST_SUCCESS;
}

static int
test_foreach_size(ssize_t size, bool may_wrap, bool set)
{
	bool reference[RAND_SET_MAX_SIZE];
	int calls[RAND_SET_MAX_SIZE];
	uint64_t *bitset;
	ssize_t i;
	ssize_t start_bit;
	ssize_t len;
	bool full_range;
	size_t total_calls = 0;

	rand_bool_ary(reference, size);

	bitset = alloc_bitset(size);

	TEST_ASSERT(bitset != NULL, "Failed to allocate memory");

	memset(calls, 0, sizeof(calls));

	start_bit = rte_rand_max(size);
	len = may_wrap ? rte_rand_max(size + 1) :
		rte_rand_max(size - start_bit + 1);

	rte_bitset_init(bitset, size);

	/* random data in the unused bits should not matter */
	rand_buf(bitset, RTE_BITSET_SIZE(size));

	for (i = start_bit; i < start_bit + len; i++) {
		size_t idx = i % size;

		if (reference[idx])
			rte_bitset_set(bitset, idx);
		else
			rte_bitset_clear(bitset, idx);

		if (rte_bitset_test(bitset, idx) != reference[idx])
			return TEST_FAILED;
	}

	full_range = (len == size && start_bit == 0);

	/* XXX: verify iteration order as well */
	if (set) {
		if (full_range && rand_bool()) {
			RTE_BITSET_FOREACH_SET(i, bitset, size) {
				if (record_match(i, size, calls) != TEST_SUCCESS)
					return TEST_FAILED;
			}
		} else if (may_wrap) {
			RTE_BITSET_FOREACH_SET_WRAP(i, bitset, size, start_bit, len) {
				if (record_match(i, size, calls) != TEST_SUCCESS) {
					printf("failed\n");
					return TEST_FAILED;
				}
			}
		} else {
			RTE_BITSET_FOREACH_SET_RANGE(i, bitset, size, start_bit, len) {
				if (record_match(i, size, calls) != TEST_SUCCESS)
					return TEST_FAILED;
			}
		}
	} else {
		if (full_range && rand_bool()) {
			RTE_BITSET_FOREACH_CLEAR(i, bitset, size)
				if (record_match(i, size, calls) != TEST_SUCCESS)
					return TEST_FAILED;
		} else if (may_wrap) {
			RTE_BITSET_FOREACH_CLEAR_WRAP(i, bitset, size, start_bit, len) {
				if (record_match(i, size, calls) != TEST_SUCCESS)
					return TEST_FAILED;
			}
		} else {
			RTE_BITSET_FOREACH_CLEAR_RANGE(i, bitset, size, start_bit, len)
				if (record_match(i, size, calls) != TEST_SUCCESS)
					return TEST_FAILED;
		}
	}

	for (i = 0; i < len; i++) {
		size_t idx = (start_bit + i) % size;

		if (reference[idx] == set && calls[idx] != 1) {
			printf("bit %zd shouldn't have been found %d times\n", idx, calls[idx]);
			return TEST_FAILED;
		}

		if (reference[idx] != set && calls[idx] != 0) {
			puts("bar");
			return TEST_FAILED;
		}

		total_calls += calls[idx];
	}

	if (full_range) {
		size_t count;

		count = set ? rte_bitset_count_set(bitset, size) :
			rte_bitset_count_clear(bitset, size);

		if (count != total_calls)
			return TEST_FAILED;
	}

	TEST_ASSERT_EQUAL(free_bitset(bitset, size), TEST_SUCCESS,
		"Buffer over- or underrun detected");

	return TEST_SUCCESS;
}

static int
test_foreach(void)
{
	size_t i;

	for (i = 0; i < RAND_ITERATIONS; i++) {
		size_t size = 1 + rte_rand_max(RAND_SET_MAX_SIZE - 1);

		if (test_foreach_size(size, false, true) != TEST_SUCCESS)
			return TEST_FAILED;

		if (test_foreach_size(size, false, false) != TEST_SUCCESS)
			return TEST_FAILED;

		if (test_foreach_size(size, true, true) != TEST_SUCCESS)
			return TEST_FAILED;

		if (test_foreach_size(size, true, false) != TEST_SUCCESS)
			return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
test_count_size(size_t size)
{
	uint64_t *bitset;

	bitset = alloc_bitset(size);

	TEST_ASSERT(bitset != NULL, "Failed to allocate memory");

	rte_bitset_init(bitset, size);

	rand_unused_bits(bitset, size);

	if (rte_bitset_count_set(bitset, size) != 0)
		return TEST_FAILED;

	if (rte_bitset_count_clear(bitset, size) != size)
		return TEST_FAILED;

	rte_bitset_set_all(bitset, size);

	if (rte_bitset_count_set(bitset, size) != size)
		return TEST_FAILED;

	if (rte_bitset_count_clear(bitset, size) != 0)
		return TEST_FAILED;

	rte_bitset_clear_all(bitset, size);

	if (rte_bitset_count_set(bitset, size) != 0)
		return TEST_FAILED;

	if (rte_bitset_count_clear(bitset, size) != size)
		return TEST_FAILED;

	rte_bitset_set(bitset, rte_rand_max(size));

	if (rte_bitset_count_set(bitset, size) != 1)
		return TEST_FAILED;

	if (rte_bitset_count_clear(bitset, size) != (size - 1))
		return TEST_FAILED;

	rte_bitset_clear_all(bitset, size);
	if (rte_bitset_count_set(bitset, size) != 0)
		return TEST_FAILED;
	if (rte_bitset_count_clear(bitset, size) != size)
		return TEST_FAILED;

	rte_bitset_set_all(bitset, size);
	if (rte_bitset_count_set(bitset, size) != size)
		return TEST_FAILED;
	if (rte_bitset_count_clear(bitset, size) != 0)
		return TEST_FAILED;

	TEST_ASSERT_EQUAL(free_bitset(bitset, size), TEST_SUCCESS,
		"Buffer over- or underrun detected");

	return TEST_SUCCESS;
}

static int
test_count(void)
{
	size_t i;

	if (test_count_size(128) != TEST_SUCCESS)
		return TEST_FAILED;
	if (test_count_size(1) != TEST_SUCCESS)
		return TEST_FAILED;
	if (test_count_size(63) != TEST_SUCCESS)
		return TEST_FAILED;
	if (test_count_size(64) != TEST_SUCCESS)
		return TEST_FAILED;
	if (test_count_size(65) != TEST_SUCCESS)
		return TEST_FAILED;

	for (i = 0; i < RAND_ITERATIONS; i++) {
		size_t size = 1 + rte_rand_max(RAND_SET_MAX_SIZE - 1);

		if (test_count_size(size) != TEST_SUCCESS)
			return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

#define GEN_DECLARE(size) \
{ \
	RTE_BITSET_DECLARE(bitset, size); \
	size_t idx = rte_rand_max(size); \
	rte_bitset_init(bitset, size); \
	rte_bitset_set(bitset, idx); \
	if (!rte_bitset_test(bitset, idx)) \
		return TEST_FAILED; \
	if (rte_bitset_count_set(bitset, size) != 1) \
		return TEST_FAILED; \
	return TEST_SUCCESS; \
}

static int
test_define(void)
{
	GEN_DECLARE(1);
	GEN_DECLARE(64);
	GEN_DECLARE(65);
	GEN_DECLARE(4097);
}

typedef void bitset_op(uint64_t *dst, const uint64_t *a, const uint64_t *b, size_t bit_num);
typedef bool bool_op(bool a, bool b);

#define LOGIC_MAX_SET_SIZE 200

static int
test_logic_op(bitset_op bitset_op, bool_op bool_op)
{
	const size_t size = 1 + rte_rand_max(LOGIC_MAX_SET_SIZE);
	RTE_BITSET_DECLARE(bitset_a, LOGIC_MAX_SET_SIZE);
	RTE_BITSET_DECLARE(bitset_b, LOGIC_MAX_SET_SIZE);
	RTE_BITSET_DECLARE(bitset_d, LOGIC_MAX_SET_SIZE);

	bool ary_a[LOGIC_MAX_SET_SIZE];
	bool ary_b[LOGIC_MAX_SET_SIZE];
	bool ary_d[LOGIC_MAX_SET_SIZE];

	rand_bool_ary(ary_a, size);
	rand_bool_ary(ary_b, size);

	size_t i;
	for (i = 0; i < size; i++) {
		rte_bitset_assign(bitset_a, i, ary_a[i]);
		rte_bitset_assign(bitset_b, i, ary_b[i]);
		ary_d[i] = bool_op(ary_a[i], ary_b[i]);
	}

	bitset_op(bitset_d, bitset_a, bitset_b, size);

	for (i = 0; i < size; i++)
		TEST_ASSERT_EQUAL(rte_bitset_test(bitset_d, i), ary_d[i],
			"Unexpected value of bit %zd", i);

	return TEST_SUCCESS;
}

static bool
bool_or(bool a, bool b)
{
	return a || b;
}

static int
test_or(void)
{
	return test_logic_op(rte_bitset_or, bool_or);
}

static bool
bool_and(bool a, bool b)
{
	return a && b;
}

static int
test_and(void)
{
	return test_logic_op(rte_bitset_and, bool_and);
}

static bool
bool_xor(bool a, bool b)
{
	return a != b;
}

static int
test_xor(void)
{
	return test_logic_op(rte_bitset_xor, bool_xor);
}

static int
test_complement(void)
{
	int i;

	for (i = 0; i < RAND_ITERATIONS; i++) {
		const size_t size = 1 + rte_rand_max(RAND_SET_MAX_SIZE - 1);

		RTE_BITSET_DECLARE(src, RAND_SET_MAX_SIZE);

		rand_bitset(src, size);

		bool bit_idx = rte_rand_max(size);
		bool bit_value = rte_bitset_test(src, bit_idx);

		RTE_BITSET_DECLARE(dst, RAND_SET_MAX_SIZE);

		rte_bitset_complement(dst, src, size);

		TEST_ASSERT(bit_value != rte_bitset_test(dst, bit_idx),
			"Bit %d was not flipped", bit_idx);
	}

	return TEST_SUCCESS;
}

#define SHIFT_SET_MAX_SIZE 500

static int
test_shift(bool right)
{
	int i;

	const char *direction = right ? "right" : "left";

	for (i = 0; i < 10000; i++) {
		const int size = 1 + (int)rte_rand_max(SHIFT_SET_MAX_SIZE);
		const int shift_count = (int)rte_rand_max(1.5 * size);
		int src_idx;

		RTE_BITSET_DECLARE(src, SHIFT_SET_MAX_SIZE);
		RTE_BITSET_DECLARE(reference, SHIFT_SET_MAX_SIZE);

		rte_bitset_init(src, size);
		rte_bitset_init(reference, size);

		rand_unused_bits(src, size);
		rand_unused_bits(reference, size);

		for (src_idx = 0; src_idx < size; src_idx++) {
			bool value = rand_bool();

			rte_bitset_assign(src, src_idx, value);

			int dst_idx = right ? src_idx - shift_count : src_idx + shift_count;

			if (dst_idx >= 0 && dst_idx < size)
				rte_bitset_assign(reference, dst_idx, value);
		}

		uint64_t *dst = alloc_bitset(size);

		if (right)
			rte_bitset_shift_right(dst, src, size, shift_count);
		else
			rte_bitset_shift_left(dst, src, size, shift_count);

		TEST_ASSERT(rte_bitset_equal(dst, reference, size),
			"Unexpected result from shifting bitset of size %d bits %d bits %s",
			size, shift_count, direction);

		TEST_ASSERT_EQUAL(free_bitset(dst, size), TEST_SUCCESS,
			"Shift %s operation overwrote buffer", direction);
	}

	return TEST_SUCCESS;
}

static int
test_shift_right(void)
{
	return test_shift(true);
}

static int
test_shift_left(void)
{
	return test_shift(false);
}

#define EQUAL_SET_MAX_SIZE 100

static int
test_equal(void)
{
	const size_t size = EQUAL_SET_MAX_SIZE;
	RTE_BITSET_DECLARE(bitset_a, EQUAL_SET_MAX_SIZE);
	RTE_BITSET_DECLARE(bitset_b, EQUAL_SET_MAX_SIZE);

	rand_buf(bitset_a, RTE_BITSET_SIZE(size));
	rand_buf(bitset_b, RTE_BITSET_SIZE(size));

	rte_bitset_init(bitset_a, size);
	rte_bitset_init(bitset_b, size);

	rte_bitset_set(bitset_a, 9);
	rte_bitset_set(bitset_b, 9);
	rte_bitset_set(bitset_a, 90);
	rte_bitset_set(bitset_b, 90);

	if (!rte_bitset_equal(bitset_a, bitset_b, size))
		return TEST_FAILED;

	/* set unused bit, which should be ignored */
	rte_bitset_set(&bitset_a[1], 60);

	if (!rte_bitset_equal(bitset_a, bitset_b, size))
		return TEST_FAILED;

	return TEST_SUCCESS;
}

static int
test_copy(void)
{
	const size_t size = EQUAL_SET_MAX_SIZE;
	RTE_BITSET_DECLARE(bitset_a, EQUAL_SET_MAX_SIZE);
	RTE_BITSET_DECLARE(bitset_b, EQUAL_SET_MAX_SIZE);

	rand_buf(bitset_a, RTE_BITSET_SIZE(size));
	rand_buf(bitset_b, RTE_BITSET_SIZE(size));

	rte_bitset_copy(bitset_a, bitset_b, size);

	if (!rte_bitset_equal(bitset_a, bitset_b, size))
		return TEST_FAILED;

	return TEST_SUCCESS;
}

static int
test_to_str(void)
{
	char buf[1024];
	RTE_BITSET_DECLARE(bitset, 128);

	rte_bitset_init(bitset, 128);
	rte_bitset_set(bitset, 1);

	if (rte_bitset_to_str(bitset, 2, buf, 3) != 3)
		return TEST_FAILED;
	if (strcmp(buf, "10") != 0)
		return TEST_FAILED;

	rte_bitset_set(bitset, 0);

	if (rte_bitset_to_str(bitset, 1, buf, sizeof(buf)) != 2)
		return TEST_FAILED;
	if (strcmp(buf, "1") != 0)
		return TEST_FAILED;

	rte_bitset_init(bitset, 99);
	rte_bitset_set(bitset, 98);

	if (rte_bitset_to_str(bitset, 99, buf, sizeof(buf)) != 100)
		return TEST_FAILED;

	if (buf[0] != '1' || strchr(&buf[1], '1') != NULL)
		return TEST_FAILED;

	if (rte_bitset_to_str(bitset, 128, buf, 64) != -EINVAL)
		return TEST_FAILED;

	return TEST_SUCCESS;
}

static struct unit_test_suite bitset_tests = {
	.suite_name = "bitset test suite",
	.unit_test_cases = {
		TEST_CASE_ST(NULL, NULL, test_set_clear),
		TEST_CASE_ST(NULL, NULL, test_flip),
		TEST_CASE_ST(NULL, NULL, test_atomic_set_clear),
		TEST_CASE_ST(NULL, NULL, test_atomic_flip),
		TEST_CASE_ST(NULL, NULL, test_find),
		TEST_CASE_ST(NULL, NULL, test_foreach),
		TEST_CASE_ST(NULL, NULL, test_count),
		TEST_CASE_ST(NULL, NULL, test_define),
		TEST_CASE_ST(NULL, NULL, test_or),
		TEST_CASE_ST(NULL, NULL, test_and),
		TEST_CASE_ST(NULL, NULL, test_xor),
		TEST_CASE_ST(NULL, NULL, test_complement),
		TEST_CASE_ST(NULL, NULL, test_shift_right),
		TEST_CASE_ST(NULL, NULL, test_shift_left),
		TEST_CASE_ST(NULL, NULL, test_equal),
		TEST_CASE_ST(NULL, NULL, test_copy),
		TEST_CASE_ST(NULL, NULL, test_to_str),
		TEST_CASES_END()
	}
};

static int
test_bitset(void)
{
	return unit_test_suite_runner(&bitset_tests);
}

REGISTER_FAST_TEST(bitset_autotest, true, true, test_bitset);
