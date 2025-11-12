/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Arm Limited
 */

#ifndef RTE_PTR_COMPRESS_H
#define RTE_PTR_COMPRESS_H

/**
 * @file
 * Pointer compression and decompression functions.
 *
 * When passing arrays full of pointers between threads, memory containing
 * the pointers is copied multiple times which is especially costly between
 * cores. These functions allow us to compress the pointers.
 *
 * Compression takes advantage of the fact that pointers are usually located in
 * a limited memory region. We compress them by converting them to offsets from
 * a base memory address. Offsets can be stored in fewer bytes.
 *
 * The compression functions come in two varieties: 32-bit and 16-bit.
 *
 * To determine how many bits are needed to compress the pointer, calculate
 * the biggest offset possible (highest value pointer - base pointer)
 * and shift the value right according to alignment (shift by exponent of the
 * power of 2 of alignment: aligned by 4 - shift by 2, aligned by 8 - shift by
 * 3, etc.). The resulting value must fit in either 32 or 16 bits. You may
 * use the macros provided in this file to do it programmatically.
 *
 * For usage example and further explanation please see this library's
 * documentation in the programming guide.
 */

#include <stdint.h>
#include <inttypes.h>

#include <rte_bitops.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_vect.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Calculate how many bits are required to store pointers within a given memory
 * region as offsets. This can help decide which pointer compression functions
 * can be used.
 *
 * @param mem_length
 *   Length of the memory region the pointers are constrained to.
 * @return
 *   Number of bits required to store a value.
 */
#define RTE_PTR_COMPRESS_BITS_NEEDED_FOR_POINTER_WITHIN_RANGE(mem_length) \
	(((uint64_t)mem_length) < 2 ? 1 : \
		(sizeof(uint64_t) * CHAR_BIT - \
		 rte_clz64((uint64_t)mem_length - 1)))

/**
 * Calculate how many bits in the address can be dropped without losing any
 * information thanks to the alignment of the address.
 *
 * @param alignment
 *   Memory alignment.
 * @return
 *   Size of shift allowed without dropping any information from the pointer.
 */
#define RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT(alignment) \
	((alignment) == 0 ? 0 : rte_ctz64((uint64_t)alignment))

/**
 * Determine if rte_ptr_compress_16_shift can be used to compress pointers
 * that contain addresses of memory objects whose memory is aligned by
 * a given amount and contained in a given memory region.
 *
 * @param mem_length
 *   The length of the memory region that contains the objects pointed to.
 * @param obj_alignment
 *   The alignment of objects pointed to.
 * @return
 *   1 if function can be used, 0 otherwise.
 */
#define RTE_PTR_COMPRESS_CAN_COMPRESS_16_SHIFT(mem_length, obj_alignment) \
	((RTE_PTR_COMPRESS_BITS_NEEDED_FOR_POINTER_WITHIN_RANGE(mem_length) - \
	RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT(obj_alignment)) <= 16 ? 1 : 0)

/**
 * Determine if rte_ptr_compress_32_shift can be used to compress pointers
 * that contain addresses of memory objects whose memory is aligned by
 * a given amount and contained in a given memory region.
 *
 * @param mem_length
 *   The length of the memory region that contains the objects pointed to.
 * @param obj_alignment
 *   The alignment of objects pointed to.
 * @return
 *   1 if function can be used, 0 otherwise.
 */
#define RTE_PTR_COMPRESS_CAN_COMPRESS_32_SHIFT(mem_length, obj_alignment) \
	((RTE_PTR_COMPRESS_BITS_NEEDED_FOR_POINTER_WITHIN_RANGE(mem_length) - \
	RTE_PTR_COMPRESS_BIT_SHIFT_FROM_ALIGNMENT(obj_alignment)) <= 32 ? 1 : 0)

/**
 * Compress pointers into 32-bit offsets from base pointer.
 *
 * @note It is programmer's responsibility to ensure the resulting offsets fit
 * into 32 bits. Alignment of the structures pointed to by the pointers allows
 * us to drop bits from the offsets. This is controlled by the bit_shift
 * parameter. This means that if structures are aligned by 8 bytes they must be
 * within 32GB of the base pointer. If there is no such alignment guarantee they
 * must be within 4GB.
 *
 * @param ptr_base
 *   A pointer used to calculate offsets of pointers in src_table.
 * @param src_table
 *   A pointer to an array of pointers.
 * @param dest_table
 *   A pointer to an array of compressed pointers returned by this function.
 * @param n
 *   The number of objects to compress, must be strictly positive.
 * @param bit_shift
 *   Byte alignment of memory pointed to by the pointers allows for
 *   bits to be dropped from the offset and hence widen the memory region that
 *   can be covered. This controls how many bits are right shifted.
 */
static __rte_always_inline void
rte_ptr_compress_32_shift(void *ptr_base, void * const *src_table,
		uint32_t *dest_table, size_t n, uint8_t bit_shift)
{
	size_t i = 0;
#if defined RTE_HAS_SVE_ACLE && !defined RTE_ARCH_ARMv8_AARCH32
	svuint64_t v_ptr_table;
	do {
		svbool_t pg = svwhilelt_b64(i, n);
		v_ptr_table = svld1_u64(pg, (const uint64_t *)src_table + i);
		v_ptr_table = svsub_x(pg, v_ptr_table, (uint64_t)ptr_base);
		v_ptr_table = svlsr_x(pg, v_ptr_table, bit_shift);
		svst1w(pg, &dest_table[i], v_ptr_table);
		i += svcntd();
	} while (i < n);
#elif defined __ARM_NEON && !defined RTE_ARCH_ARMv8_AARCH32
	uintptr_t ptr_diff;
	uint64x2_t v_ptr_table;
	/* right shift is done by left shifting by negative int */
	int64x2_t v_shift = vdupq_n_s64(-bit_shift);
	uint64x2_t v_ptr_base = vdupq_n_u64((uint64_t)ptr_base);
	const size_t n_even = n & ~0x1;
	for (; i < n_even; i += 2) {
		v_ptr_table = vld1q_u64((const uint64_t *)src_table + i);
		v_ptr_table = vsubq_u64(v_ptr_table, v_ptr_base);
		v_ptr_table = vshlq_u64(v_ptr_table, v_shift);
		vst1_u32(dest_table + i, vqmovn_u64(v_ptr_table));
	}
	/* process leftover single item in case of odd number of n */
	if (unlikely(n & 0x1)) {
		ptr_diff = RTE_PTR_DIFF(src_table[i], ptr_base);
		dest_table[i] = (uint32_t) (ptr_diff >> bit_shift);
	}
#else
	uintptr_t ptr_diff;
	for (; i < n; i++) {
		ptr_diff = RTE_PTR_DIFF(src_table[i], ptr_base);
		ptr_diff = ptr_diff >> bit_shift;
		RTE_ASSERT(ptr_diff <= UINT32_MAX);
		dest_table[i] = (uint32_t) ptr_diff;
	}
#endif
}

/**
 * Decompress pointers from 32-bit offsets from base pointer.
 *
 * @param ptr_base
 *   A pointer which was used to calculate offsets in src_table.
 * @param src_table
 *   A pointer to an array to compressed pointers.
 * @param dest_table
 *   A pointer to an array of decompressed pointers returned by this function.
 * @param n
 *   The number of objects to decompress, must be strictly positive.
 * @param bit_shift
 *   Byte alignment of memory pointed to by the pointers allows for
 *   bits to be dropped from the offset and hence widen the memory region that
 *   can be covered. This controls how many bits are left shifted when pointers
 *   are recovered from the offsets.
 */
static __rte_always_inline void
rte_ptr_decompress_32_shift(void *ptr_base, uint32_t const *src_table,
		void **dest_table, size_t n, uint8_t bit_shift)
{
	size_t i = 0;
#if defined RTE_HAS_SVE_ACLE && !defined RTE_ARCH_ARMv8_AARCH32
	svuint64_t v_ptr_table;
	do {
		svbool_t pg = svwhilelt_b64(i, n);
		v_ptr_table = svld1uw_u64(pg, &src_table[i]);
		v_ptr_table = svlsl_x(pg, v_ptr_table, bit_shift);
		v_ptr_table = svadd_x(pg, v_ptr_table, (uint64_t)ptr_base);
		svst1(pg, (uint64_t *)dest_table + i, v_ptr_table);
		i += svcntd();
	} while (i < n);
#elif defined __ARM_NEON && !defined RTE_ARCH_ARMv8_AARCH32
	uintptr_t ptr_diff;
	uint64x2_t v_ptr_table;
	int64x2_t v_shift = vdupq_n_s64(bit_shift);
	uint64x2_t v_ptr_base = vdupq_n_u64((uint64_t)ptr_base);
	const size_t n_even = n & ~0x1;
	for (; i < n_even; i += 2) {
		v_ptr_table = vmovl_u32(vld1_u32(src_table + i));
		v_ptr_table = vshlq_u64(v_ptr_table, v_shift);
		v_ptr_table = vaddq_u64(v_ptr_table, v_ptr_base);
		vst1q_u64((uint64_t *)dest_table + i, v_ptr_table);
	}
	/* process leftover single item in case of odd number of n */
	if (unlikely(n & 0x1)) {
		ptr_diff = ((uintptr_t) src_table[i]) << bit_shift;
		dest_table[i] = RTE_PTR_ADD(ptr_base, ptr_diff);
	}
#else
	uintptr_t ptr_diff;
	for (; i < n; i++) {
		ptr_diff = ((uintptr_t) src_table[i]) << bit_shift;
		dest_table[i] = RTE_PTR_ADD(ptr_base, ptr_diff);
	}
#endif
}

/**
 * Compress pointers into 16-bit offsets from base pointer.
 *
 * @note It is programmer's responsibility to ensure the resulting offsets fit
 * into 16 bits. Alignment of the structures pointed to by the pointers allows
 * us to drop bits from the offsets. This is controlled by the bit_shift
 * parameter. This means that if structures are aligned by 8 bytes they must be
 * within 256KB of the base pointer. If there is no such alignment guarantee
 * they must be within 64KB.
 *
 * @param ptr_base
 *   A pointer used to calculate offsets of pointers in src_table.
 * @param src_table
 *   A pointer to an array of pointers.
 * @param dest_table
 *   A pointer to an array of compressed pointers returned by this function.
 * @param n
 *   The number of objects to compress, must be strictly positive.
 * @param bit_shift
 *   Byte alignment of memory pointed to by the pointers allows for
 *   bits to be dropped from the offset and hence widen the memory region that
 *   can be covered. This controls how many bits are right shifted.
 */
static __rte_always_inline void
rte_ptr_compress_16_shift(void *ptr_base, void * const *src_table,
		uint16_t *dest_table, size_t n, uint8_t bit_shift)
{

	size_t i = 0;
#if defined RTE_HAS_SVE_ACLE && !defined RTE_ARCH_ARMv8_AARCH32
	svuint64_t v_ptr_table;
	do {
		svbool_t pg = svwhilelt_b64(i, n);
		v_ptr_table = svld1_u64(pg, (const uint64_t *)src_table + i);
		v_ptr_table = svsub_x(pg, v_ptr_table, (uint64_t)ptr_base);
		v_ptr_table = svlsr_x(pg, v_ptr_table, bit_shift);
		svst1h(pg, &dest_table[i], v_ptr_table);
		i += svcntd();
	} while (i < n);
#else
	uintptr_t ptr_diff;
	for (; i < n; i++) {
		ptr_diff = RTE_PTR_DIFF(src_table[i], ptr_base);
		ptr_diff = ptr_diff >> bit_shift;
		RTE_ASSERT(ptr_diff <= UINT16_MAX);
		dest_table[i] = (uint16_t) ptr_diff;
	}
#endif
}

/**
 * Decompress pointers from 16-bit offsets from base pointer.
 *
 * @param ptr_base
 *   A pointer which was used to calculate offsets in src_table.
 * @param src_table
 *   A pointer to an array to compressed pointers.
 * @param dest_table
 *   A pointer to an array of decompressed pointers returned by this function.
 * @param n
 *   The number of objects to decompress, must be strictly positive.
 * @param bit_shift
 *   Byte alignment of memory pointed to by the pointers allows for
 *   bits to be dropped from the offset and hence widen the memory region that
 *   can be covered. This controls how many bits are left shifted when pointers
 *   are recovered from the offsets.
 */
static __rte_always_inline void
rte_ptr_decompress_16_shift(void *ptr_base, uint16_t const *src_table,
		void **dest_table, size_t n, uint8_t bit_shift)
{
	size_t i = 0;
#if defined RTE_HAS_SVE_ACLE && !defined RTE_ARCH_ARMv8_AARCH32
	svuint64_t v_ptr_table;
	do {
		svbool_t pg = svwhilelt_b64(i, n);
		v_ptr_table = svld1uh_u64(pg, &src_table[i]);
		v_ptr_table = svlsl_x(pg, v_ptr_table, bit_shift);
		v_ptr_table = svadd_x(pg, v_ptr_table, (uint64_t)ptr_base);
		svst1(pg, (uint64_t *)dest_table + i, v_ptr_table);
		i += svcntd();
	} while (i < n);
#else
	uintptr_t ptr_diff;
	for (; i < n; i++) {
		ptr_diff = ((uintptr_t) src_table[i]) << bit_shift;
		dest_table[i] = RTE_PTR_ADD(ptr_base, ptr_diff);
	}
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* RTE_PTR_COMPRESS_H */
