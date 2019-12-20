/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_pause.h>

#include "test.h"

#define MAX_NUM 1 << 20

#define FAIL(x)\
	{printf(x "() test failed!\n");\
	return -1;}

/* this is really a sanity check */
static int
test_macros(int __rte_unused unused_parm)
{
#define SMALLER 0x1000U
#define BIGGER 0x2000U
#define PTR_DIFF BIGGER - SMALLER
#define FAIL_MACRO(x)\
	{printf(#x "() test failed!\n");\
	return -1;}

	uintptr_t unused = 0;

	RTE_SET_USED(unused);

	if ((uintptr_t)RTE_PTR_ADD(SMALLER, PTR_DIFF) != BIGGER)
		FAIL_MACRO(RTE_PTR_ADD);
	if ((uintptr_t)RTE_PTR_SUB(BIGGER, PTR_DIFF) != SMALLER)
		FAIL_MACRO(RTE_PTR_SUB);
	if (RTE_PTR_DIFF(BIGGER, SMALLER) != PTR_DIFF)
		FAIL_MACRO(RTE_PTR_DIFF);
	if (RTE_MAX(SMALLER, BIGGER) != BIGGER)
		FAIL_MACRO(RTE_MAX);
	if (RTE_MIN(SMALLER, BIGGER) != SMALLER)
		FAIL_MACRO(RTE_MIN);

	if (strncmp(RTE_STR(test), "test", sizeof("test")))
		FAIL_MACRO(RTE_STR);

	return 0;
}

static int
test_bsf(void)
{
	uint32_t shift, pos;

	/* safe versions should be able to handle 0 */
	if (rte_bsf32_safe(0, &pos) != 0)
		FAIL("rte_bsf32_safe");
	if (rte_bsf64_safe(0, &pos) != 0)
		FAIL("rte_bsf64_safe");

	for (shift = 0; shift < 63; shift++) {
		uint32_t val32;
		uint64_t val64;

		val64 = 1ULL << shift;
		if ((uint32_t)rte_bsf64(val64) != shift)
			FAIL("rte_bsf64");
		if (rte_bsf64_safe(val64, &pos) != 1)
			FAIL("rte_bsf64_safe");
		if (pos != shift)
			FAIL("rte_bsf64_safe");

		if (shift > 31)
			continue;

		val32 = 1U << shift;
		if ((uint32_t)rte_bsf32(val32) != shift)
			FAIL("rte_bsf32");
		if (rte_bsf32_safe(val32, &pos) != 1)
			FAIL("rte_bsf32_safe");
		if (pos != shift)
			FAIL("rte_bsf32_safe");
	}

	return 0;
}

static int
test_misc(void)
{
	char memdump[] = "memdump_test";

	rte_memdump(stdout, "test", memdump, sizeof(memdump));
	rte_hexdump(stdout, "test", memdump, sizeof(memdump));

	rte_pause();

	return 0;
}

static int
test_align(void)
{
#define FAIL_ALIGN(x, i, p)\
	{printf(x "() test failed: %u %u\n", i, p);\
	return -1;}
#define FAIL_ALIGN64(x, j, q)\
	{printf(x "() test failed: %"PRIu64" %"PRIu64"\n", j, q);\
	return -1; }
#define ERROR_FLOOR(res, i, pow) \
		(res % pow) || 						/* check if not aligned */ \
		((res / pow) != (i / pow))  		/* check if correct alignment */
#define ERROR_CEIL(res, i, pow) \
		(res % pow) ||						/* check if not aligned */ \
			((i % pow) == 0 ?				/* check if ceiling is invoked */ \
			val / pow != i / pow :			/* if aligned */ \
			val / pow != (i / pow) + 1)		/* if not aligned, hence +1 */

	uint32_t i, p, val;
	uint64_t j, q;

	for (i = 1, p = 1; i <= MAX_NUM; i ++) {
		if (rte_align32pow2(i) != p)
			FAIL_ALIGN("rte_align32pow2", i, p);
		if (i == p)
			p <<= 1;
	}

	for (i = 1, p = 1; i <= MAX_NUM; i++) {
		if (rte_align32prevpow2(i) != p)
			FAIL_ALIGN("rte_align32prevpow2", i, p);
		if (rte_is_power_of_2(i + 1))
			p = i + 1;
	}

	for (j = 1, q = 1; j <= MAX_NUM ; j++) {
		if (rte_align64pow2(j) != q)
			FAIL_ALIGN64("rte_align64pow2", j, q);
		if (j == q)
			q <<= 1;
	}

	for (j = 1, q = 1; j <= MAX_NUM ; j++) {
		if (rte_align64prevpow2(j) != q)
			FAIL_ALIGN64("rte_align64prevpow2", j, q);
		if (rte_is_power_of_2(j + 1))
			q = j + 1;
	}

	for (p = 2; p <= MAX_NUM; p <<= 1) {

		if (!rte_is_power_of_2(p))
			FAIL("rte_is_power_of_2");

		for (i = 1; i <= MAX_NUM; i++) {
			/* align floor */
			if (RTE_ALIGN_FLOOR((uintptr_t)i, p) % p)
				FAIL_ALIGN("RTE_ALIGN_FLOOR", i, p);

			val = RTE_PTR_ALIGN_FLOOR((uintptr_t) i, p);
			if (ERROR_FLOOR(val, i, p))
				FAIL_ALIGN("RTE_PTR_ALIGN_FLOOR", i, p);

			val = RTE_ALIGN_FLOOR(i, p);
			if (ERROR_FLOOR(val, i, p))
				FAIL_ALIGN("RTE_ALIGN_FLOOR", i, p);

			/* align ceiling */
			val = RTE_PTR_ALIGN((uintptr_t) i, p);
			if (ERROR_CEIL(val, i, p))
				FAIL_ALIGN("RTE_PTR_ALIGN", i, p);

			val = RTE_ALIGN(i, p);
			if (ERROR_CEIL(val, i, p))
				FAIL_ALIGN("RTE_ALIGN", i, p);

			val = RTE_ALIGN_CEIL(i, p);
			if (ERROR_CEIL(val, i, p))
				FAIL_ALIGN("RTE_ALIGN_CEIL", i, p);

			val = RTE_PTR_ALIGN_CEIL((uintptr_t)i, p);
			if (ERROR_CEIL(val, i, p))
				FAIL_ALIGN("RTE_PTR_ALIGN_CEIL", i, p);

			/* by this point we know that val is aligned to p */
			if (!rte_is_aligned((void*)(uintptr_t) val, p))
				FAIL("rte_is_aligned");
		}
	}

	for (p = 1; p <= MAX_NUM / 2; p++) {
		for (i = 1; i <= MAX_NUM / 2; i++) {
			val = RTE_ALIGN_MUL_CEIL(i, p);
			if (val % p != 0 || val < i)
				FAIL_ALIGN("RTE_ALIGN_MUL_CEIL", i, p);
			val = RTE_ALIGN_MUL_FLOOR(i, p);
			if (val % p != 0 || val > i)
				FAIL_ALIGN("RTE_ALIGN_MUL_FLOOR", i, p);
			val = RTE_ALIGN_MUL_NEAR(i, p);
			if (val % p != 0 || ((val != RTE_ALIGN_MUL_CEIL(i, p))
				& (val != RTE_ALIGN_MUL_FLOOR(i, p))))
				FAIL_ALIGN("RTE_ALIGN_MUL_NEAR", i, p);
		}
	}

	return 0;
}

static int
test_log2(void)
{
	uint32_t i, base, compare;
	const uint32_t max = 0x10000;
	const uint32_t step = 1;

	compare = rte_log2_u32(0);
	if (compare != 0) {
		printf("Wrong rte_log2_u32(0) val %x, expected 0\n", compare);
		return TEST_FAILED;
	}

	compare = rte_log2_u64(0);
	if (compare != 0) {
		printf("Wrong rte_log2_u64(0) val %x, expected 0\n", compare);
		return TEST_FAILED;
	}

	for (i = 1; i < max; i = i + step) {
		uint64_t i64;

		/* extend range for 64-bit */
		i64 = (uint64_t)i << 32;
		base = (uint32_t)ceilf(log2(i64));
		compare = rte_log2_u64(i64);
		if (base != compare) {
			printf("Wrong rte_log2_u64(%" PRIx64 ") val %x, expected %x\n",
				i64, compare, base);
			return TEST_FAILED;
		}

		base = (uint32_t)ceilf(log2((uint32_t)i));
		compare = rte_log2_u32((uint32_t)i);
		if (base != compare) {
			printf("Wrong rte_log2_u32(%x) val %x, expected %x\n",
				i, compare, base);
			return TEST_FAILED;
		}
		compare = rte_log2_u64((uint64_t)i);
		if (base != compare) {
			printf("Wrong rte_log2_u64(%x) val %x, expected %x\n",
				i, compare, base);
			return TEST_FAILED;
		}
	}
	return 0;
}

static int
test_fls(void)
{
	struct fls_test_vector {
		uint32_t arg;
		int rc;
	};
	int expected, rc;
	uint32_t i, arg;

	const struct fls_test_vector test[] = {
		{0x0, 0},
		{0x1, 1},
		{0x4000, 15},
		{0x80000000, 32},
	};

	for (i = 0; i < RTE_DIM(test); i++) {
		uint64_t arg64;

		arg = test[i].arg;
		rc = rte_fls_u32(arg);
		expected = test[i].rc;
		if (rc != expected) {
			printf("Wrong rte_fls_u32(0x%x) rc=%d, expected=%d\n",
				arg, rc, expected);
			return TEST_FAILED;
		}
		/* 64-bit version */
		arg = test[i].arg;
		rc = rte_fls_u64(arg);
		expected = test[i].rc;
		if (rc != expected) {
			printf("Wrong rte_fls_u64(0x%x) rc=%d, expected=%d\n",
				arg, rc, expected);
			return TEST_FAILED;
		}
		/* 64-bit version shifted by 32 bits */
		arg64 = (uint64_t)test[i].arg << 32;
		rc = rte_fls_u64(arg64);
		/* don't shift zero */
		expected = test[i].rc == 0 ? 0 : test[i].rc + 32;
		if (rc != expected) {
			printf("Wrong rte_fls_u64(0x%" PRIx64 ") rc=%d, expected=%d\n",
				arg64, rc, expected);
			return TEST_FAILED;
		}
	}

	return 0;
}

static int
test_common(void)
{
	int ret = 0;
	ret |= test_align();
	ret |= test_macros(0);
	ret |= test_misc();
	ret |= test_bsf();
	ret |= test_log2();
	ret |= test_fls();

	return ret;
}

REGISTER_TEST_COMMAND(common_autotest, test_common);
