/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) IBM Corporation 2014,2021
 */

#ifndef _RTE_MEMCPY_PPC_64_H_
#define _RTE_MEMCPY_PPC_64_H_

#include <stdint.h>
#include <string.h>

#include "rte_altivec.h"
#include "rte_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "generic/rte_memcpy.h"

#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION >= 90000)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION >= 100000)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

static inline void
rte_mov16(uint8_t *dst, const uint8_t *src)
{
	vec_vsx_st(vec_vsx_ld(0, src), 0, dst);
}

static inline void
rte_mov32(uint8_t *dst, const uint8_t *src)
{
	vec_vsx_st(vec_vsx_ld(0, src), 0, dst);
	vec_vsx_st(vec_vsx_ld(16, src), 16, dst);
}

static inline void
rte_mov48(uint8_t *dst, const uint8_t *src)
{
	vec_vsx_st(vec_vsx_ld(0, src), 0, dst);
	vec_vsx_st(vec_vsx_ld(16, src), 16, dst);
	vec_vsx_st(vec_vsx_ld(32, src), 32, dst);
}

static inline void
rte_mov64(uint8_t *dst, const uint8_t *src)
{
	vec_vsx_st(vec_vsx_ld(0, src), 0, dst);
	vec_vsx_st(vec_vsx_ld(16, src), 16, dst);
	vec_vsx_st(vec_vsx_ld(32, src), 32, dst);
	vec_vsx_st(vec_vsx_ld(48, src), 48, dst);
}

static inline void
rte_mov128(uint8_t *dst, const uint8_t *src)
{
	vec_vsx_st(vec_vsx_ld(0, src), 0, dst);
	vec_vsx_st(vec_vsx_ld(16, src), 16, dst);
	vec_vsx_st(vec_vsx_ld(32, src), 32, dst);
	vec_vsx_st(vec_vsx_ld(48, src), 48, dst);
	vec_vsx_st(vec_vsx_ld(64, src), 64, dst);
	vec_vsx_st(vec_vsx_ld(80, src), 80, dst);
	vec_vsx_st(vec_vsx_ld(96, src), 96, dst);
	vec_vsx_st(vec_vsx_ld(112, src), 112, dst);
}

static inline void
rte_mov256(uint8_t *dst, const uint8_t *src)
{
	rte_mov128(dst, src);
	rte_mov128(dst + 128, src + 128);
}

#define rte_memcpy(dst, src, n)              \
	__extension__ ({                     \
	(__builtin_constant_p(n)) ?          \
	memcpy((dst), (src), (n)) :          \
	rte_memcpy_func((dst), (src), (n)); })

static inline void *
rte_memcpy_func(void *dst, const void *src, size_t n)
{
	void *ret = dst;

	/* We can't copy < 16 bytes using XMM registers so do it manually. */
	if (n < 16) {
		if (n & 0x01) {
			*(uint8_t *)dst = *(const uint8_t *)src;
			dst = (uint8_t *)dst + 1;
			src = (const uint8_t *)src + 1;
		}
		if (n & 0x02) {
			*(uint16_t *)dst = *(const uint16_t *)src;
			dst = (uint16_t *)dst + 1;
			src = (const uint16_t *)src + 1;
		}
		if (n & 0x04) {
			*(uint32_t *)dst = *(const uint32_t *)src;
			dst = (uint32_t *)dst + 1;
			src = (const uint32_t *)src + 1;
		}
		if (n & 0x08)
			*(uint64_t *)dst = *(const uint64_t *)src;
		return ret;
	}

	/* Special fast cases for <= 128 bytes */
	if (n <= 32) {
		rte_mov16((uint8_t *)dst, (const uint8_t *)src);
		rte_mov16((uint8_t *)dst - 16 + n,
			(const uint8_t *)src - 16 + n);
		return ret;
	}

	if (n <= 64) {
		rte_mov32((uint8_t *)dst, (const uint8_t *)src);
		rte_mov32((uint8_t *)dst - 32 + n,
			(const uint8_t *)src - 32 + n);
		return ret;
	}

	if (n <= 128) {
		rte_mov64((uint8_t *)dst, (const uint8_t *)src);
		rte_mov64((uint8_t *)dst - 64 + n,
			(const uint8_t *)src - 64 + n);
		return ret;
	}

	/*
	 * For large copies > 128 bytes. This combination of 256, 64 and 16 byte
	 * copies was found to be faster than doing 128 and 32 byte copies as
	 * well.
	 */
	for ( ; n >= 256; n -= 256) {
		rte_mov256((uint8_t *)dst, (const uint8_t *)src);
		dst = (uint8_t *)dst + 256;
		src = (const uint8_t *)src + 256;
	}

	/*
	 * We split the remaining bytes (which will be less than 256) into
	 * 64byte (2^6) chunks.
	 * Using incrementing integers in the case labels of a switch statement
	 * encourages the compiler to use a jump table. To get incrementing
	 * integers, we shift the 2 relevant bits to the LSB position to first
	 * get decrementing integers, and then subtract.
	 */
	switch (3 - (n >> 6)) {
	case 0x00:
		rte_mov64((uint8_t *)dst, (const uint8_t *)src);
		n -= 64;
		dst = (uint8_t *)dst + 64;
		src = (const uint8_t *)src + 64;      /* fallthrough */
	case 0x01:
		rte_mov64((uint8_t *)dst, (const uint8_t *)src);
		n -= 64;
		dst = (uint8_t *)dst + 64;
		src = (const uint8_t *)src + 64;      /* fallthrough */
	case 0x02:
		rte_mov64((uint8_t *)dst, (const uint8_t *)src);
		n -= 64;
		dst = (uint8_t *)dst + 64;
		src = (const uint8_t *)src + 64;      /* fallthrough */
	default:
		;
	}

	/*
	 * We split the remaining bytes (which will be less than 64) into
	 * 16byte (2^4) chunks, using the same switch structure as above.
	 */
	switch (3 - (n >> 4)) {
	case 0x00:
		rte_mov16((uint8_t *)dst, (const uint8_t *)src);
		n -= 16;
		dst = (uint8_t *)dst + 16;
		src = (const uint8_t *)src + 16;      /* fallthrough */
	case 0x01:
		rte_mov16((uint8_t *)dst, (const uint8_t *)src);
		n -= 16;
		dst = (uint8_t *)dst + 16;
		src = (const uint8_t *)src + 16;      /* fallthrough */
	case 0x02:
		rte_mov16((uint8_t *)dst, (const uint8_t *)src);
		n -= 16;
		dst = (uint8_t *)dst + 16;
		src = (const uint8_t *)src + 16;      /* fallthrough */
	default:
		;
	}

	/* Copy any remaining bytes, without going beyond end of buffers */
	if (n != 0)
		rte_mov16((uint8_t *)dst - 16 + n,
			(const uint8_t *)src - 16 + n);
	return ret;
}

#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION >= 100000)
#pragma GCC diagnostic pop
#endif

#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION >= 90000)
#pragma GCC diagnostic pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MEMCPY_PPC_64_H_ */
