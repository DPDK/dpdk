/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Arm Limited
 */

#include "test.h"
#include <stdint.h>
#include <string.h>

#include <rte_ptr_compress.h>

#define MAX_ALIGN_EXPONENT 3
#define MAX_PTRS 16
#define NUM_BASES 2
#define NUM_REGIONS 4
#define MAX_32BIT_REGION ((uint64_t)UINT32_MAX + 1)
#define MAX_16BIT_REGION (UINT16_MAX + 1)

static int
test_ptr_compress_params(
	void *base,
	uint64_t mem_sz,
	unsigned int align_exp,
	unsigned int num_ptrs,
	bool use_32_bit)
{
	unsigned int i;
	unsigned int align = 1 << align_exp;
	void *ptrs[MAX_PTRS] = {0};
	void *ptrs_out[MAX_PTRS] = {0};
	uint32_t offsets32[MAX_PTRS] = {0};
	uint16_t offsets16[MAX_PTRS] = {0};

	for (i = 0; i < num_ptrs; i++) {
		/* make pointers point at memory in steps of align */
		/* alternate steps from the start and end of memory region */
		if ((i & 1) == 1)
			ptrs[i] = (char *)base + mem_sz - i * align;
		else
			ptrs[i] = (char *)base + i * align;
	}

	if (use_32_bit) {
		rte_ptr_compress_32_shift(
				base, ptrs, offsets32, num_ptrs, align_exp);
		rte_ptr_decompress_32_shift(base, offsets32, ptrs_out, num_ptrs,
				align_exp);
	} else {
		rte_ptr_compress_16_shift(
				base, ptrs, offsets16, num_ptrs, align_exp);
		rte_ptr_decompress_16_shift(base, offsets16, ptrs_out, num_ptrs,
				align_exp);
	}

	TEST_ASSERT_BUFFERS_ARE_EQUAL(ptrs, ptrs_out, sizeof(void *) * num_ptrs,
		"Decompressed pointers corrupted\nbase pointer: %p, "
		"memory region size: %" PRIu64 ", alignment exponent: %u, "
		"num of pointers: %u, using %s offsets",
		base, mem_sz, align_exp, num_ptrs,
		use_32_bit ? "32-bit" : "16-bit");

	return 0;
}

static int
test_ptr_compress(void)
{
	unsigned int j, k, n;
	int ret = 0;
	/* the test is run with multiple memory regions and base addresses */
	void * const bases[NUM_BASES] = { (void *)0, (void *)UINT16_MAX };
	/* maximum size for pointers aligned by consecutive powers of 2 */
	const uint64_t region_sizes_16[NUM_REGIONS] = {
		MAX_16BIT_REGION,
		MAX_16BIT_REGION * 2,
		MAX_16BIT_REGION * 4,
		MAX_16BIT_REGION * 8,
	};
	const uint64_t region_sizes_32[NUM_REGIONS] = {
		MAX_32BIT_REGION,
		MAX_32BIT_REGION * 2,
		MAX_32BIT_REGION * 4,
		MAX_32BIT_REGION * 8,
	};

	/* main test compresses and decompresses arrays of pointers
	 * and compares the array before and after to verify that
	 * pointers are successfully decompressed
	 */

	for (j = 0; j < NUM_REGIONS; j++) {
		for (k = 0; k < NUM_BASES; k++) {
			for (n = 1; n < MAX_PTRS; n++) {
				ret |= test_ptr_compress_params(
					bases[k],
					region_sizes_16[j],
					j /* exponent of alignment */,
					n,
					false
				);
				ret |= test_ptr_compress_params(
					bases[k],
					region_sizes_32[j],
					j /* exponent of alignment */,
					n,
					true
				);
				if (ret != 0)
					return ret;
			}
		}
	}

	/* verify helper macro computations */

	n = RTE_PTR_COMPRESS_BITS_NEEDED_FOR_POINTER_WITHIN_RANGE(0);
	TEST_ASSERT_EQUAL(n, 1,
			"RTE_PTR_COMPRESS_BITS_NEEDED_FOR_POINTER_WITHIN_RANGE "
			"macro computation incorrect\n");

	n = RTE_PTR_COMPRESS_BITS_NEEDED_FOR_POINTER_WITHIN_RANGE(1);
	TEST_ASSERT_EQUAL(n, 1,
			"RTE_PTR_COMPREtopSS_BITS_REQUIRED_TO_STORE_VALUE "
			"macro computation incorrect\n");

	n = RTE_PTR_COMPRESS_BITS_NEEDED_FOR_POINTER_WITHIN_RANGE(MAX_16BIT_REGION);
	TEST_ASSERT_EQUAL(n, 16,
			"RTE_PTR_COMPRESS_BITS_NEEDED_FOR_POINTER_WITHIN_RANGE "
			"macro computation incorrect\n");

	n = RTE_PTR_COMPRESS_BITS_NEEDED_FOR_POINTER_WITHIN_RANGE(MAX_32BIT_REGION);
	TEST_ASSERT_EQUAL(n, 32,
			"RTE_PTR_COMPRESS_BITS_NEEDED_FOR_POINTER_WITHIN_RANGE "
			"macro computation incorrect\n");

	n = RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT(0);
	TEST_ASSERT_EQUAL(n, 0,
			"RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT "
			"macro computation incorrect\n");

	n = RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT(1);
	TEST_ASSERT_EQUAL(n, 0,
			"RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT "
			"macro computation incorrect\n");

	n = RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT(2);
	TEST_ASSERT_EQUAL(n, 1,
			"RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT "
			"macro computation incorrect\n");

	n = RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT(3);
	TEST_ASSERT_EQUAL(n, 0,
			"RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT "
			"macro computation incorrect\n");

	n = RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT(4);
	TEST_ASSERT_EQUAL(n, 2,
			"RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT "
			"macro computation incorrect\n");

	ret = RTE_PTR_COMPRESS_CAN_COMPRESS_16_SHIFT(MAX_16BIT_REGION, 1);
	TEST_ASSERT_EQUAL(ret, 1,
			"RTE_PTR_COMPRESS_CAN_COMPRESS_16_SHIFT "
			"macro computation incorrect\n");

	ret = RTE_PTR_COMPRESS_CAN_COMPRESS_16_SHIFT(MAX_16BIT_REGION + 1, 1);
	TEST_ASSERT_EQUAL(ret, 0,
			"RTE_PTR_COMPRESS_CAN_COMPRESS_16_SHIFT "
			"macro computation incorrect\n");

	ret = RTE_PTR_COMPRESS_CAN_COMPRESS_16_SHIFT(MAX_16BIT_REGION + 1, 2);
	TEST_ASSERT_EQUAL(ret, 1,
			"RTE_PTR_COMPRESS_CAN_COMPRESS_16_SHIFT "
			"macro computation incorrect\n");

	ret = RTE_PTR_COMPRESS_CAN_COMPRESS_32_SHIFT(MAX_32BIT_REGION, 1);
	TEST_ASSERT_EQUAL(ret, 1,
			"RTE_PTR_COMPRESS_CAN_COMPRESS_32_SHIFT "
			"macro computation incorrect\n");

	ret = RTE_PTR_COMPRESS_CAN_COMPRESS_32_SHIFT(MAX_32BIT_REGION + 1, 1);
	TEST_ASSERT_EQUAL(ret, 0,
			"RTE_PTR_COMPRESS_CAN_COMPRESS_32_SHIFT "
			"macro computation incorrect\n");

	ret = RTE_PTR_COMPRESS_CAN_COMPRESS_32_SHIFT(MAX_32BIT_REGION + 1, 2);
	TEST_ASSERT_EQUAL(ret, 1,
			"RTE_PTR_COMPRESS_CAN_COMPRESS_32_SHIFT "
			"macro computation incorrect\n");

	return 0;
}

REGISTER_FAST_TEST(ptr_compress_autotest, true, true, test_ptr_compress);
