/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _RTE_VECT_X86_H_
#define _RTE_VECT_X86_H_

/**
 * @file
 *
 * RTE SSE/AVX related header.
 */

#include <assert.h>
#include <stdint.h>
#include <rte_config.h>
#include <rte_common.h>
#include "generic/rte_vect.h"

#if defined(_WIN64)
#include <smmintrin.h> /* SSE4 */
#include <immintrin.h>
#else
#include <x86intrin.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_VECT_DEFAULT_SIMD_BITWIDTH RTE_VECT_SIMD_256

typedef __m128i xmm_t;

#define	XMM_SIZE	16
#define	XMM_MASK	(XMM_SIZE - 1)

static_assert(sizeof(xmm_t) == XMM_SIZE, "");

typedef union rte_xmm {
	xmm_t    x;
	uint8_t  u8[XMM_SIZE / sizeof(uint8_t)];
	uint16_t u16[XMM_SIZE / sizeof(uint16_t)];
	uint32_t u32[XMM_SIZE / sizeof(uint32_t)];
	uint64_t u64[XMM_SIZE / sizeof(uint64_t)];
	double   pd[XMM_SIZE / sizeof(double)];
} rte_xmm_t;

#ifdef __AVX__

typedef __m256i ymm_t;

#define	YMM_SIZE	(sizeof(ymm_t))
#define	YMM_MASK	(YMM_SIZE - 1)

typedef union rte_ymm {
	ymm_t    y;
	xmm_t    x[YMM_SIZE / sizeof(xmm_t)];
	uint8_t  u8[YMM_SIZE / sizeof(uint8_t)];
	uint16_t u16[YMM_SIZE / sizeof(uint16_t)];
	uint32_t u32[YMM_SIZE / sizeof(uint32_t)];
	uint64_t u64[YMM_SIZE / sizeof(uint64_t)];
	double   pd[YMM_SIZE / sizeof(double)];
} rte_ymm_t;

#endif /* __AVX__ */

#ifdef RTE_ARCH_I686
#define _mm_cvtsi128_si64(a)    \
__extension__ ({                \
	rte_xmm_t m;            \
	m.x = (a);              \
	(m.u64[0]);             \
})
#endif

#ifdef __AVX512F__

#define RTE_X86_ZMM_SIZE        64
#define RTE_X86_ZMM_MASK	(RTE_X86_ZMM_SIZE - 1)

/*
 * MSVC does not allow __rte_aligned(sizeof(__m512i)). It only accepts
 * numbers that are power of 2. So, even though sizeof(__m512i) represents a
 * number that is a power of two it cannot be used directly.
 * Ref: https://learn.microsoft.com/en-us/cpp/cpp/align-cpp?view=msvc-170
 * The static assert below ensures that the hardcoded value defined as
 * RTE_X86_ZMM_SIZE is equal to sizeof(__m512i).
 */
static_assert(RTE_X86_ZMM_SIZE == (sizeof(__m512i)), "Unexpected size of __m512i");
typedef union __rte_aligned(RTE_X86_ZMM_SIZE) __rte_x86_zmm {
	__m512i	 z;
	ymm_t    y[RTE_X86_ZMM_SIZE / sizeof(ymm_t)];
	xmm_t    x[RTE_X86_ZMM_SIZE / sizeof(xmm_t)];
	uint8_t  u8[RTE_X86_ZMM_SIZE / sizeof(uint8_t)];
	uint16_t u16[RTE_X86_ZMM_SIZE / sizeof(uint16_t)];
	uint32_t u32[RTE_X86_ZMM_SIZE / sizeof(uint32_t)];
	uint64_t u64[RTE_X86_ZMM_SIZE / sizeof(uint64_t)];
	double   pd[RTE_X86_ZMM_SIZE / sizeof(double)];
} __rte_x86_zmm_t;

#endif /* __AVX512F__ */

#ifdef __cplusplus
}
#endif

#endif /* _RTE_VECT_X86_H_ */
