/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 StarFive
 * Copyright(c) 2022 SiFive
 * Copyright(c) 2022 Semihalf
 * Copyright(c) 2025 ISCAS
 */

#ifndef RTE_MEMCPY_RISCV_H
#define RTE_MEMCPY_RISCV_H

#include <stdint.h>
#include <string.h>

#include "rte_common.h"

#include "generic/rte_memcpy.h"

#ifdef RTE_ARCH_RISCV_MEMCPY

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This implementation is improved from eal/arm/include/rte_memcpy_64.h,
 * targeting only cases of < 64 bytes.
 * Currently shows significant performance improvement over various glibc versions,
 * but is disabled by default due to uncertainty about potential performance
 * degradation in future versions.
 * You can use memcpy_perf_autotest to test the performance.
 */

static __rte_always_inline
void rte_mov16(uint8_t *dst, const uint8_t *src)
{
	__uint128_t *dst128 = (__uint128_t *)dst;
	const __uint128_t *src128 = (const __uint128_t *)src;
	*dst128 = *src128;
}

static __rte_always_inline
void rte_mov32(uint8_t *dst, const uint8_t *src)
{
	__uint128_t *dst128 = (__uint128_t *)dst;
	const __uint128_t *src128 = (const __uint128_t *)src;
	const __uint128_t x0 = src128[0], x1 = src128[1];
	dst128[0] = x0;
	dst128[1] = x1;
}

static __rte_always_inline
void rte_mov48(uint8_t *dst, const uint8_t *src)
{
	__uint128_t *dst128 = (__uint128_t *)dst;
	const __uint128_t *src128 = (const __uint128_t *)src;
	const __uint128_t x0 = src128[0], x1 = src128[1], x2 = src128[2];
	dst128[0] = x0;
	dst128[1] = x1;
	dst128[2] = x2;
}

static __rte_always_inline void
rte_mov64(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 64);
}

static __rte_always_inline void
rte_mov128(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 128);
}

static __rte_always_inline void
rte_mov256(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 256);
}

static __rte_always_inline void
rte_memcpy_lt16(uint8_t *dst, const uint8_t *src, size_t n)
{
	if (n & 0x08) {
		/* copy 8 ~ 15 bytes */
		*(uint64_t *)dst = *(const uint64_t *)src;
		*(uint64_t *)(dst - 8 + n) = *(const uint64_t *)(src - 8 + n);
	} else if (n & 0x04) {
		/* copy 4 ~ 7 bytes */
		*(uint32_t *)dst = *(const uint32_t *)src;
		*(uint32_t *)(dst - 4 + n) = *(const uint32_t *)(src - 4 + n);
	} else if (n & 0x02) {
		/* copy 2 ~ 3 bytes */
		*(uint16_t *)dst = *(const uint16_t *)src;
		*(uint16_t *)(dst - 2 + n) = *(const uint16_t *)(src - 2 + n);
	} else if (n & 0x01) {
		/* copy 1 byte */
		*dst = *src;
	}
}

static __rte_always_inline void
rte_memcpy_ge16_lt64(uint8_t *dst, const uint8_t *src, size_t n)
{
	if (n == 16) {
		rte_mov16(dst, src);
	} else if (n <= 32) {
		rte_mov16(dst, src);
		rte_mov16(dst - 16 + n, src - 16 + n);
	} else if (n <= 48) {
		rte_mov32(dst, src);
		rte_mov16(dst - 16 + n, src - 16 + n);
	} else {
		rte_mov48(dst, src);
		rte_mov16(dst - 16 + n, src - 16 + n);
	}
}

static __rte_always_inline void *
rte_memcpy(void *dst, const void *src, size_t n)
{
	if (n >= 64)
		return memcpy(dst, src, n);
	if (n < 16) {
		rte_memcpy_lt16((uint8_t *)dst, (const uint8_t *)src, n);
		return dst;
	}
	rte_memcpy_ge16_lt64((uint8_t *)dst, (const uint8_t *)src, n);
	return dst;
}

#ifdef __cplusplus
}
#endif

#else /* RTE_ARCH_RISCV_MEMCPY */

#ifdef __cplusplus
extern "C" {
#endif

static inline void
rte_mov16(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 16);
}

static inline void
rte_mov32(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 32);
}

static inline void
rte_mov48(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 48);
}

static inline void
rte_mov64(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 64);
}

static inline void
rte_mov128(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 128);
}

static inline void
rte_mov256(uint8_t *dst, const uint8_t *src)
{
	memcpy(dst, src, 256);
}

#define rte_memcpy(d, s, n)	memcpy((d), (s), (n))

#ifdef __cplusplus
}
#endif

#endif /* RTE_ARCH_RISCV_MEMCPY */

#endif /* RTE_MEMCPY_RISCV_H */
