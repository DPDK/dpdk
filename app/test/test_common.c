/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <rte_common.h>
#include <rte_bitops.h>
#include <rte_hexdump.h>
#include <rte_random.h>
#include <rte_pause.h>

#include "test.h"

#define MAX_NUM (1 << 20)

#define FAIL(x)\
	{printf(x "() test failed!\n");\
	return -1;}

/* test_ptr_add_sub_align independent test parameters */
#define RTE_TEST_COMMON_MAX_ALIGNMENT RTE_CACHE_LINE_SIZE
#define RTE_TEST_COMMON_MAX_OFFSET 256
#define RTE_TEST_COMMON_MAX_INCREMENT 128
/* test_ptr_add_sub_align dependent: computed based on test requirements */
/* Extra RTE_TEST_COMMON_MAX_ALIGNMENT to ensure CEIL can round up without going out of bounds */
#define TEST_BUFFER_SIZE (RTE_TEST_COMMON_MAX_OFFSET + RTE_TEST_COMMON_MAX_INCREMENT + \
	(2 * RTE_TEST_COMMON_MAX_ALIGNMENT) + 16)

/* test_ptr_align_edge_cases independent test parameters */
#define RTE_COMMON_TEST_PAGE_SIZE 4096
#define RTE_COMMON_TEST_CACHE_LINE_ALIGN RTE_CACHE_LINE_SIZE
/* test_ptr_align_edge_cases dependent: computed based on test requirements */
/* Must fit PAGE_SIZE alignment tests */
#define RTE_COMMON_TEST_EDGE_CASE_BUFFER_SIZE (2 * RTE_COMMON_TEST_PAGE_SIZE)
#define RTE_COMMON_TEST_DOUBLE_PAGE_SIZE (2 * RTE_COMMON_TEST_PAGE_SIZE)
/* Must be >= CACHE_LINE_ALIGN to prevent overflow in CEIL boundary test */
#define RTE_COMMON_TEST_BOUNDARY_TEST_OFFSET (2 * RTE_COMMON_TEST_CACHE_LINE_ALIGN)

static int
test_ptr_add_sub_align(void)
{
	/* Unaligned buffer for testing unaligned pointer types */
	char unaligned_buffer[TEST_BUFFER_SIZE];
	/* Aligned buffer for testing aligned pointer types */
	alignas(RTE_TEST_COMMON_MAX_ALIGNMENT) char aligned_buffer[TEST_BUFFER_SIZE];
	size_t offset;
	uint8_t uval, aval;
	uint16_t u16_uval, u16_aval;
	uint32_t u32_uval, u32_aval;
	uint64_t u64_uval, u64_aval;

	uval = (uint8_t)rte_rand();
	aval = (uint8_t)rte_rand();
	if (uval == aval)
		aval = (uint8_t)~aval;

	/* Compute expected values for each type width by replicating byte pattern */
	memset(&u16_uval, uval, sizeof(u16_uval));
	memset(&u16_aval, aval, sizeof(u16_aval));
	memset(&u32_uval, uval, sizeof(u32_uval));
	memset(&u32_aval, aval, sizeof(u32_aval));
	memset(&u64_uval, uval, sizeof(u64_uval));
	memset(&u64_aval, aval, sizeof(u64_aval));

	/* Initialize buffers - prevents compiler optimization and tests unaligned access */
	memset(unaligned_buffer, uval, sizeof(unaligned_buffer));
	memset(aligned_buffer, aval, sizeof(aligned_buffer));

	/* Test various offsets to ensure correctness across memory range */
	for (offset = 0; offset < RTE_TEST_COMMON_MAX_OFFSET; offset++) {
		void *ubase = unaligned_buffer + offset;
		void *abase = aligned_buffer + offset;
		size_t increment;

		/* Test different increment values */
		for (increment = 0; increment < RTE_TEST_COMMON_MAX_INCREMENT; increment++) {
			void *result;
			char *cp_result;
			const void *cvp_result;
			unaligned_uint16_t *u16p_result;
			unaligned_uint32_t *u32p_result;
			unaligned_uint64_t *u64p_result;
			uintptr_t uptr_val, aptr_val;
			uintptr_t uexp_floor, uexp_ceil, aexp_floor, aexp_ceil;
			size_t align;

			/* Test void* ADD and SUB using unaligned buffer */
			result = RTE_PTR_ADD(ubase, increment);
			RTE_TEST_ASSERT_EQUAL(result, (void *)((char *)ubase + increment),
				"RTE_PTR_ADD for void* at offset=%zu inc=%zu",
				offset, increment);
			result = RTE_PTR_SUB(result, increment);
			RTE_TEST_ASSERT_EQUAL(result, ubase,
				"RTE_PTR_SUB for void* at offset=%zu inc=%zu",
				offset, increment);

			/* Test char* type preservation using unaligned buffer */
			cp_result = RTE_PTR_ADD((char *)ubase, increment);
			RTE_TEST_ASSERT_EQUAL(cp_result, (char *)ubase + increment,
				"RTE_PTR_ADD for char* at offset=%zu inc=%zu",
				offset, increment);
			RTE_TEST_ASSERT_EQUAL((unsigned char)*cp_result, (unsigned char)uval,
				"char* dereference at offset=%zu inc=%zu",
				offset, increment);
			cp_result = RTE_PTR_SUB(cp_result, increment);
			RTE_TEST_ASSERT_EQUAL(cp_result, (char *)ubase,
				"RTE_PTR_SUB for char* at offset=%zu inc=%zu",
				offset, increment);

			/* Test const void* preservation using unaligned buffer */
			cvp_result = RTE_PTR_ADD((const void *)ubase, increment);
			RTE_TEST_ASSERT_EQUAL(cvp_result,
				(const void *)((char *)ubase + increment),
				"RTE_PTR_ADD for const void* at offset=%zu inc=%zu",
				offset, increment);
			cvp_result = RTE_PTR_SUB(cvp_result, increment);
			RTE_TEST_ASSERT_EQUAL(cvp_result, (const void *)ubase,
				"RTE_PTR_SUB for const void* at offset=%zu inc=%zu",
				offset, increment);

			/* Test unaligned_uint16_t* using unaligned buffer */
			u16p_result = RTE_PTR_ADD((unaligned_uint16_t *)ubase, increment);
			RTE_TEST_ASSERT_EQUAL(u16p_result,
				(unaligned_uint16_t *)((char *)ubase + increment),
				"RTE_PTR_ADD for u16* at offset=%zu inc=%zu",
				offset, increment);
			RTE_TEST_ASSERT_EQUAL(*u16p_result, u16_uval,
				"unaligned u16 dereference at offset=%zu inc=%zu",
				offset, increment);
			u16p_result = RTE_PTR_SUB(u16p_result, increment);
			RTE_TEST_ASSERT_EQUAL(u16p_result, (unaligned_uint16_t *)ubase,
				"RTE_PTR_SUB for u16* at offset=%zu inc=%zu",
				offset, increment);

			/* Test unaligned_uint32_t* using unaligned buffer */
			u32p_result = RTE_PTR_ADD((unaligned_uint32_t *)ubase, increment);
			RTE_TEST_ASSERT_EQUAL(u32p_result,
				(unaligned_uint32_t *)((char *)ubase + increment),
				"RTE_PTR_ADD for u32* at offset=%zu inc=%zu",
				offset, increment);
			RTE_TEST_ASSERT_EQUAL(*u32p_result, u32_uval,
				"unaligned u32 dereference at offset=%zu inc=%zu",
				offset, increment);
			u32p_result = RTE_PTR_SUB(u32p_result, increment);
			RTE_TEST_ASSERT_EQUAL(u32p_result, (unaligned_uint32_t *)ubase,
				"RTE_PTR_SUB for u32* at offset=%zu inc=%zu",
				offset, increment);

			/* Test unaligned_uint64_t* using unaligned buffer */
			u64p_result = RTE_PTR_ADD((unaligned_uint64_t *)ubase, increment);
			RTE_TEST_ASSERT_EQUAL(u64p_result,
				(unaligned_uint64_t *)((char *)ubase + increment),
				"RTE_PTR_ADD for u64* at offset=%zu inc=%zu",
				offset, increment);
			RTE_TEST_ASSERT_EQUAL(*u64p_result, u64_uval,
				"unaligned u64 dereference at offset=%zu inc=%zu",
				offset, increment);
			u64p_result = RTE_PTR_SUB(u64p_result, increment);
			RTE_TEST_ASSERT_EQUAL(u64p_result, (unaligned_uint64_t *)ubase,
				"RTE_PTR_SUB for u64* at offset=%zu inc=%zu",
				offset, increment);

			/* Test aligned uint16_t* at 2-byte aligned offsets */
			if (offset % sizeof(uint16_t) == 0 &&
					increment % sizeof(uint16_t) == 0) {
				uint16_t *a16p_result;
				a16p_result = RTE_PTR_ADD((uint16_t *)abase, increment);
				RTE_TEST_ASSERT_EQUAL(a16p_result,
					(uint16_t *)((char *)abase + increment),
					"RTE_PTR_ADD for uint16_t* at offset=%zu inc=%zu",
					offset, increment);
				RTE_TEST_ASSERT_EQUAL(*a16p_result, u16_aval,
					"aligned u16 dereference at offset=%zu inc=%zu",
					offset, increment);
				a16p_result = RTE_PTR_SUB(a16p_result, increment);
				RTE_TEST_ASSERT_EQUAL(a16p_result, (uint16_t *)abase,
					"RTE_PTR_SUB for uint16_t* at offset=%zu inc=%zu",
					offset, increment);
			}

			/* Test aligned uint32_t* at 4-byte aligned offsets */
			if (offset % sizeof(uint32_t) == 0 &&
					increment % sizeof(uint32_t) == 0) {
				uint32_t *a32p_result;
				a32p_result = RTE_PTR_ADD((uint32_t *)abase, increment);
				RTE_TEST_ASSERT_EQUAL(a32p_result,
					(uint32_t *)((char *)abase + increment),
					"RTE_PTR_ADD for uint32_t* at offset=%zu inc=%zu",
					offset, increment);
				RTE_TEST_ASSERT_EQUAL(*a32p_result, u32_aval,
					"aligned u32 dereference at offset=%zu inc=%zu",
					offset, increment);
				a32p_result = RTE_PTR_SUB(a32p_result, increment);
				RTE_TEST_ASSERT_EQUAL(a32p_result, (uint32_t *)abase,
					"RTE_PTR_SUB for uint32_t* at offset=%zu inc=%zu",
					offset, increment);
			}

			/* Test aligned uint64_t* at 8-byte aligned offsets */
			if (offset % sizeof(uint64_t) == 0 &&
					increment % sizeof(uint64_t) == 0) {
				uint64_t *a64p_result;
				a64p_result = RTE_PTR_ADD((uint64_t *)abase, increment);
				RTE_TEST_ASSERT_EQUAL(a64p_result,
					(uint64_t *)((char *)abase + increment),
					"RTE_PTR_ADD for uint64_t* at offset=%zu inc=%zu",
					offset, increment);
				RTE_TEST_ASSERT_EQUAL(*a64p_result, u64_aval,
					"aligned u64 dereference at offset=%zu inc=%zu",
					offset, increment);
				a64p_result = RTE_PTR_SUB(a64p_result, increment);
				RTE_TEST_ASSERT_EQUAL(a64p_result, (uint64_t *)abase,
					"RTE_PTR_SUB for uint64_t* at offset=%zu inc=%zu",
					offset, increment);
			}

			/* Test alignment functions with various alignments */
			uptr_val = (uintptr_t)RTE_PTR_ADD(ubase, increment);
			aptr_val = (uintptr_t)RTE_PTR_ADD(abase, increment);

			/* Test power-of-2 alignments: 1, 2, 4, 8, 16 */
			for (align = 1; align <= RTE_TEST_COMMON_MAX_ALIGNMENT; align <<= 1) {
				/* Compute expected values using arithmetic, not masking */
				uexp_floor = (uptr_val / align) * align;
				uexp_ceil = ((uptr_val + align - 1) / align) * align;
				aexp_floor = (aptr_val / align) * align;
				aexp_ceil = ((aptr_val + align - 1) / align) * align;

				result = RTE_PTR_ADD(ubase, increment);
				result = RTE_PTR_ALIGN_FLOOR(result, align);
				RTE_TEST_ASSERT_EQUAL((uintptr_t)result, uexp_floor,
					"ALIGN_FLOOR offset=%zu inc=%zu align=%zu",
					offset, increment, align);
				RTE_TEST_ASSERT_EQUAL((uintptr_t)result % align, 0,
					"ALIGN_FLOOR not aligned offset=%zu inc=%zu align=%zu",
					offset, increment, align);

				result = RTE_PTR_ADD(ubase, increment);
				result = RTE_PTR_ALIGN_CEIL(result, align);
				RTE_TEST_ASSERT_EQUAL((uintptr_t)result, uexp_ceil,
					"ALIGN_CEIL offset=%zu inc=%zu align=%zu",
					offset, increment, align);
				RTE_TEST_ASSERT_EQUAL((uintptr_t)result % align, 0,
					"ALIGN_CEIL not aligned offset=%zu inc=%zu align=%zu",
					offset, increment, align);

				result = RTE_PTR_ADD(ubase, increment);
				result = RTE_PTR_ALIGN(result, align);
				RTE_TEST_ASSERT_EQUAL((uintptr_t)result, uexp_ceil,
					"ALIGN != CEIL offset=%zu inc=%zu align=%zu",
					offset, increment, align);

				/* Test type preservation */
				cp_result = RTE_PTR_ADD((char *)ubase, increment);
				cp_result = RTE_PTR_ALIGN_FLOOR(cp_result, align);
				RTE_TEST_ASSERT_EQUAL((uintptr_t)cp_result, uexp_floor,
					"char* ALIGN_FLOOR offset=%zu inc=%zu align=%zu",
					offset, increment, align);

				cp_result = RTE_PTR_ADD((char *)ubase, increment);
				cp_result = RTE_PTR_ALIGN_CEIL(cp_result, align);
				RTE_TEST_ASSERT_EQUAL((uintptr_t)cp_result, uexp_ceil,
					"char* ALIGN_CEIL offset=%zu inc=%zu align=%zu",
					offset, increment, align);

				cp_result = RTE_PTR_ADD((char *)ubase, increment);
				cp_result = RTE_PTR_ALIGN(cp_result, align);
				RTE_TEST_ASSERT_EQUAL((uintptr_t)cp_result, uexp_ceil,
					"char* ALIGN != CEIL offset=%zu inc=%zu align=%zu",
					offset, increment, align);

				/* Test aligned uint16_t* at 2-byte aligned offsets */
				if (offset % sizeof(uint16_t) == 0 && align >= sizeof(uint16_t)) {
					uint16_t *a16p_result;

					a16p_result = RTE_PTR_ADD((uint16_t *)abase, increment);
					a16p_result = RTE_PTR_ALIGN_FLOOR(a16p_result, align);
					RTE_TEST_ASSERT_EQUAL((uintptr_t)a16p_result, aexp_floor,
						"uint16_t* ALIGN_FLOOR offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);
					RTE_TEST_ASSERT_EQUAL(*a16p_result, u16_aval,
						"uint16_t* ALIGN_FLOOR dereference offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);

					a16p_result = RTE_PTR_ADD((uint16_t *)abase, increment);
					a16p_result = RTE_PTR_ALIGN_CEIL(a16p_result, align);
					RTE_TEST_ASSERT_EQUAL((uintptr_t)a16p_result, aexp_ceil,
						"uint16_t* ALIGN_CEIL offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);
					RTE_TEST_ASSERT_EQUAL(*a16p_result, u16_aval,
						"uint16_t* ALIGN_CEIL dereference offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);

					a16p_result = RTE_PTR_ADD((uint16_t *)abase, increment);
					a16p_result = RTE_PTR_ALIGN(a16p_result, align);
					RTE_TEST_ASSERT_EQUAL((uintptr_t)a16p_result, aexp_ceil,
						"uint16_t* ALIGN != CEIL offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);
					RTE_TEST_ASSERT_EQUAL(*a16p_result, u16_aval,
						"uint16_t* ALIGN dereference offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);
				}

				/* Test aligned uint32_t* at 4-byte aligned offsets */
				if (offset % sizeof(uint32_t) == 0 && align >= sizeof(uint32_t)) {
					uint32_t *a32p_result;

					a32p_result = RTE_PTR_ADD((uint32_t *)abase, increment);
					a32p_result = RTE_PTR_ALIGN_FLOOR(a32p_result, align);
					RTE_TEST_ASSERT_EQUAL((uintptr_t)a32p_result, aexp_floor,
						"uint32_t* ALIGN_FLOOR offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);
					RTE_TEST_ASSERT_EQUAL(*a32p_result, u32_aval,
						"uint32_t* ALIGN_FLOOR dereference offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);

					a32p_result = RTE_PTR_ADD((uint32_t *)abase, increment);
					a32p_result = RTE_PTR_ALIGN_CEIL(a32p_result, align);
					RTE_TEST_ASSERT_EQUAL((uintptr_t)a32p_result, aexp_ceil,
						"uint32_t* ALIGN_CEIL offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);
					RTE_TEST_ASSERT_EQUAL(*a32p_result, u32_aval,
						"uint32_t* ALIGN_CEIL dereference offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);

					a32p_result = RTE_PTR_ADD((uint32_t *)abase, increment);
					a32p_result = RTE_PTR_ALIGN(a32p_result, align);
					RTE_TEST_ASSERT_EQUAL((uintptr_t)a32p_result, aexp_ceil,
						"uint32_t* ALIGN != CEIL offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);
					RTE_TEST_ASSERT_EQUAL(*a32p_result, u32_aval,
						"uint32_t* ALIGN dereference offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);
				}

				/* Test aligned uint64_t* at 8-byte aligned offsets */
				if (offset % sizeof(uint64_t) == 0 && align >= sizeof(uint64_t)) {
					uint64_t *a64p_result;

					a64p_result = RTE_PTR_ADD((uint64_t *)abase, increment);
					a64p_result = RTE_PTR_ALIGN_FLOOR(a64p_result, align);
					RTE_TEST_ASSERT_EQUAL((uintptr_t)a64p_result, aexp_floor,
						"uint64_t* ALIGN_FLOOR offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);
					RTE_TEST_ASSERT_EQUAL(*a64p_result, u64_aval,
						"uint64_t* ALIGN_FLOOR dereference offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);

					a64p_result = RTE_PTR_ADD((uint64_t *)abase, increment);
					a64p_result = RTE_PTR_ALIGN_CEIL(a64p_result, align);
					RTE_TEST_ASSERT_EQUAL((uintptr_t)a64p_result, aexp_ceil,
						"uint64_t* ALIGN_CEIL offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);
					RTE_TEST_ASSERT_EQUAL(*a64p_result, u64_aval,
						"uint64_t* ALIGN_CEIL dereference offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);

					a64p_result = RTE_PTR_ADD((uint64_t *)abase, increment);
					a64p_result = RTE_PTR_ALIGN(a64p_result, align);
					RTE_TEST_ASSERT_EQUAL((uintptr_t)a64p_result, aexp_ceil,
						"uint64_t* ALIGN != CEIL offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);
					RTE_TEST_ASSERT_EQUAL(*a64p_result, u64_aval,
						"uint64_t* ALIGN dereference offset=%zu inc=%zu "
						"align=%zu", offset, increment, align);
				}
			}
		}
	}

	return 0;
}

static int
test_ptr_align_edge_cases(void)
{
	/* Ensure BOUNDARY_TEST_OFFSET is large enough to prevent overflow in CEIL test */
	/* near_max + CACHE_LINE_ALIGN - 1 must not wrap, so
	 * BOUNDARY_TEST_OFFSET >= CACHE_LINE_ALIGN.
	 */
	RTE_BUILD_BUG_ON(RTE_COMMON_TEST_BOUNDARY_TEST_OFFSET < RTE_COMMON_TEST_CACHE_LINE_ALIGN);

	alignas(RTE_CACHE_LINE_SIZE) char test_buffer[RTE_COMMON_TEST_EDGE_CASE_BUFFER_SIZE];
	void *result;
	uint64_t *typed_result;

	/* Initialize buffer */
	memset(test_buffer, 0xAA, sizeof(test_buffer));

	/* Test 1: Very large alignment values (page size and beyond) */
	const size_t large_alignments[] = {RTE_COMMON_TEST_PAGE_SIZE,
					    RTE_COMMON_TEST_DOUBLE_PAGE_SIZE};
	for (size_t i = 0; i < RTE_DIM(large_alignments); i++) {
		size_t align = large_alignments[i];
		void *unaligned_ptr = test_buffer + 1;  /* Intentionally misaligned by 1 byte */

		/* Ensure buffer is large enough for this alignment */
		RTE_TEST_ASSERT(align <= RTE_COMMON_TEST_EDGE_CASE_BUFFER_SIZE,
			"Buffer too small for alignment %zu", align);

		result = RTE_PTR_ALIGN_FLOOR(unaligned_ptr, align);
		RTE_TEST_ASSERT((uintptr_t)result % align == 0,
			"FLOOR with alignment %zu not aligned", align);
		RTE_TEST_ASSERT(result <= unaligned_ptr,
			"FLOOR with alignment %zu went forward", align);

		result = RTE_PTR_ALIGN_CEIL(unaligned_ptr, align);
		RTE_TEST_ASSERT((uintptr_t)result % align == 0,
			"CEIL with alignment %zu not aligned", align);
		RTE_TEST_ASSERT(result >= unaligned_ptr,
			"CEIL with alignment %zu went backward", align);
	}

	/* Test 2: Address space boundary arithmetic (no dereferencing) */
	/* Test FLOOR lower bound - pointer near zero */
	/* Dynamically compute offset that allows FLOOR to align down without underflow */
	uintptr_t near_zero = RTE_COMMON_TEST_BOUNDARY_TEST_OFFSET;
	void *low_ptr = (void *)near_zero;
	uintptr_t expected_floor = (near_zero / RTE_COMMON_TEST_CACHE_LINE_ALIGN) *
				   RTE_COMMON_TEST_CACHE_LINE_ALIGN;

	result = RTE_PTR_ALIGN_FLOOR(low_ptr, RTE_COMMON_TEST_CACHE_LINE_ALIGN);
	RTE_TEST_ASSERT((uintptr_t)result % RTE_COMMON_TEST_CACHE_LINE_ALIGN == 0,
		"Low address FLOOR not aligned to %d", RTE_COMMON_TEST_CACHE_LINE_ALIGN);
	RTE_TEST_ASSERT((uintptr_t)result == expected_floor,
		"Low address FLOOR computed incorrectly: got %p, expected %p",
		result, (void *)expected_floor);
	RTE_TEST_ASSERT((uintptr_t)result <= near_zero,
		"Low address FLOOR went forward");

	/* Test CEIL upper bound - pointer near UINTPTR_MAX */
	/* Compute offset that allows CEIL to align up without wrapping */
	/* Ensure no overflow: near_max + CACHE_LINE_ALIGN - 1 must not wrap */
	uintptr_t near_max = UINTPTR_MAX - RTE_COMMON_TEST_BOUNDARY_TEST_OFFSET;
	void *high_ptr = (void *)near_max;
	uintptr_t expected_ceil = ((near_max + RTE_COMMON_TEST_CACHE_LINE_ALIGN - 1) /
				   RTE_COMMON_TEST_CACHE_LINE_ALIGN) *
				  RTE_COMMON_TEST_CACHE_LINE_ALIGN;

	result = RTE_PTR_ALIGN_CEIL(high_ptr, RTE_COMMON_TEST_CACHE_LINE_ALIGN);
	RTE_TEST_ASSERT((uintptr_t)result % RTE_COMMON_TEST_CACHE_LINE_ALIGN == 0,
		"High address CEIL not aligned to %d", RTE_COMMON_TEST_CACHE_LINE_ALIGN);
	RTE_TEST_ASSERT((uintptr_t)result == expected_ceil,
		"High address CEIL computed incorrectly: got %p, expected %p",
		result, (void *)expected_ceil);
	RTE_TEST_ASSERT((uintptr_t)result >= near_max,
		"High address CEIL went backward");

	/* Test 3: Type preservation with extreme alignments */
	/* Test CEIL with PAGE_SIZE - aligns upward into buffer */
	typed_result = (uint64_t *)test_buffer;
	typed_result = RTE_PTR_ALIGN_CEIL(typed_result, RTE_COMMON_TEST_PAGE_SIZE);
	RTE_TEST_ASSERT((uintptr_t)typed_result % RTE_COMMON_TEST_PAGE_SIZE == 0,
		"CEIL type preservation failed with PAGE_SIZE alignment");
	RTE_TEST_ASSERT((uintptr_t)typed_result <
			(uintptr_t)test_buffer + RTE_COMMON_TEST_EDGE_CASE_BUFFER_SIZE,
		"CEIL went beyond buffer bounds");
	/* Verify we can dereference as uint64_t* (compiler should allow this) */
	*typed_result = 0x123456789ABCDEF0ULL;
	RTE_TEST_ASSERT(*typed_result == 0x123456789ABCDEF0ULL,
		"CEIL type-preserved pointer dereference failed");

	/* Test FLOOR with CACHE_LINE_ALIGN - buffer is guaranteed cache-line aligned */
	/* Use cache line alignment since buffer is only guaranteed RTE_CACHE_LINE_SIZE aligned */
	typed_result = (uint64_t *)(test_buffer + RTE_COMMON_TEST_CACHE_LINE_ALIGN);
	typed_result = RTE_PTR_ALIGN_FLOOR(typed_result, RTE_COMMON_TEST_CACHE_LINE_ALIGN);
	RTE_TEST_ASSERT((uintptr_t)typed_result % RTE_COMMON_TEST_CACHE_LINE_ALIGN == 0,
		"FLOOR type preservation failed with CACHE_LINE alignment");
	RTE_TEST_ASSERT((uintptr_t)typed_result >= (uintptr_t)test_buffer,
		"FLOOR went before buffer start");
	RTE_TEST_ASSERT((uintptr_t)typed_result <
			(uintptr_t)test_buffer + RTE_COMMON_TEST_EDGE_CASE_BUFFER_SIZE,
		"FLOOR went beyond buffer bounds");
	/* Safe to dereference now */
	*typed_result = 0xDEADBEEFCAFEBABEULL;
	RTE_TEST_ASSERT(*typed_result == 0xDEADBEEFCAFEBABEULL,
		"FLOOR type-preserved pointer dereference failed");

	return 0;
}

/* this is really a sanity check */
static int
test_macros(void)
{
#define SMALLER 0x1000U
#define BIGGER 0x2000U
#define PTR_DIFF BIGGER - SMALLER

	uintptr_t unused = 0;
	unsigned int smaller = SMALLER, bigger  = BIGGER;
	uint32_t arr[3];

	RTE_SET_USED(unused);

	RTE_SWAP(smaller, bigger);
	RTE_TEST_ASSERT(smaller == BIGGER && bigger == SMALLER,
		"RTE_SWAP");
	RTE_TEST_ASSERT_EQUAL(RTE_PTR_DIFF(BIGGER, SMALLER), PTR_DIFF,
		"RTE_PTR_DIFF");
	RTE_TEST_ASSERT_EQUAL(RTE_MAX(SMALLER, BIGGER), BIGGER,
		"RTE_MAX");
	RTE_TEST_ASSERT_EQUAL(RTE_MIN(SMALLER, BIGGER), SMALLER,
		"RTE_MIN");
	RTE_TEST_ASSERT_EQUAL(RTE_DIV_ROUND_UP(10, 5), 2,
		"RTE_DIV_ROUND_UP exact");
	RTE_TEST_ASSERT_EQUAL(RTE_DIV_ROUND_UP(11, 5), 3,
		"RTE_DIV_ROUND_UP remainder");
	RTE_TEST_ASSERT_EQUAL(RTE_DIV_ROUND_UP(0, 5), 0,
		"RTE_DIV_ROUND_UP n=0");
	RTE_TEST_ASSERT_EQUAL(RTE_DIV_ROUND_UP(0, 1), 0,
		"RTE_DIV_ROUND_UP n=0, d=1");
	RTE_TEST_ASSERT_EQUAL(RTE_DIV_ROUND_UP(5, 1), 5,
		"RTE_DIV_ROUND_UP d=1");

	RTE_TEST_ASSERT_EQUAL(RTE_PTR_ADD(arr + 1, sizeof(arr[0])), &arr[2],
		"RTE_PTR_ADD(expr, x)");
	RTE_TEST_ASSERT_EQUAL(RTE_PTR_SUB(arr + 1, sizeof(arr[0])), &arr[0],
		"RTE_PTR_SUB(expr, x)");
	RTE_TEST_ASSERT_EQUAL(RTE_PTR_ALIGN_FLOOR(arr + 2, 4), &arr[2],
		"RTE_PTR_ALIGN_FLOOR(expr, x)");
	RTE_TEST_ASSERT_EQUAL(RTE_PTR_ALIGN_CEIL(arr + 2, 4), &arr[2],
		"RTE_PTR_ALIGN_CEIL(expr, x)");
	RTE_TEST_ASSERT_EQUAL(RTE_PTR_ALIGN(arr + 2, 4), &arr[2],
		"RTE_PTR_ALIGN(expr, x)");

	RTE_TEST_ASSERT_EQUAL(
		RTE_PTR_ALIGN_FLOOR(RTE_PTR_ADD(&arr[1], 1), 4), &arr[1],
		"RTE_PTR_ALIGN_FLOOR(x < y/2, y)");
	RTE_TEST_ASSERT_EQUAL(
		RTE_PTR_ALIGN_FLOOR(RTE_PTR_ADD(&arr[1], 3), 4), &arr[1],
		"RTE_PTR_ALIGN_FLOOR(x > y/2, y)");
	RTE_TEST_ASSERT_EQUAL(
		RTE_PTR_ALIGN_CEIL(RTE_PTR_ADD(&arr[1], 3), 4), &arr[2],
		"RTE_PTR_ALIGN_CEIL(x < y/2, y)");
	RTE_TEST_ASSERT_EQUAL(
		RTE_PTR_ALIGN_CEIL(RTE_PTR_ADD(&arr[1], 1), 4), &arr[2],
		"RTE_PTR_ALIGN_CEIL(x > y/2, y)");

	RTE_TEST_ASSERT(strncmp(RTE_STR(test), "test", sizeof("test")) == 0,
		"RTE_STR");

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

			val = RTE_ALIGN_FLOOR(i, p);
			if (ERROR_FLOOR(val, i, p))
				FAIL_ALIGN("RTE_ALIGN_FLOOR", i, p);

			/* align ceiling */
			val = RTE_ALIGN(i, p);
			if (ERROR_CEIL(val, i, p))
				FAIL_ALIGN("RTE_ALIGN", i, p);

			val = RTE_ALIGN_CEIL(i, p);
			if (ERROR_CEIL(val, i, p))
				FAIL_ALIGN("RTE_ALIGN_CEIL", i, p);

			/* by this point we know that val is aligned to p */
			if (!rte_is_aligned((void*)(uintptr_t) val, p))
				FAIL("rte_is_aligned");
		}
	}

	/* testing the whole space of 2^20^2 takes too long. */
	for (j = 1; j <= MAX_NUM ; j++) {
		i = rte_rand_max(MAX_NUM - 1) + 1;
		p = rte_rand_max(MAX_NUM - 1) + 1;

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

static struct unit_test_suite common_test_suite = {
	.suite_name = "common autotest",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
		TEST_CASE(test_ptr_add_sub_align),
		TEST_CASE(test_ptr_align_edge_cases),
		TEST_CASE(test_align),
		TEST_CASE(test_macros),
		TEST_CASE(test_misc),
		TEST_CASE(test_bsf),
		TEST_CASE(test_log2),
		TEST_CASE(test_fls),
		TEST_CASES_END()
	}
};

static int
test_common(void)
{
	return unit_test_suite_runner(&common_test_suite);
}

REGISTER_FAST_TEST(common_autotest, NOHUGE_OK, ASAN_OK, test_common);
