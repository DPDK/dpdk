/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 */

#ifndef _RTE_VECT_H_
#define _RTE_VECT_H_

/**
 * @file
 * SIMD vector types
 *
 * This file defines types to use vector instructions with generic C code.
 */

#include <stdint.h>

/* Unsigned vector types */

/**
 * 64 bits vector size to use with unsigned 8 bits elements.
 *
 * a = (rte_v64u8_t){ a0, a1, a2, a3, a4, a5, a6, a7 }
 */
typedef uint8_t rte_v64u8_t __attribute__((vector_size(8), aligned(8)));

/**
 * 64 bits vector size to use with unsigned 16 bits elements.
 *
 * a = (rte_v64u16_t){ a0, a1, a2, a3 }
 */
typedef uint16_t rte_v64u16_t __attribute__((vector_size(8), aligned(8)));

/**
 * 64 bits vector size to use with unsigned 32 bits elements.
 *
 * a = (rte_v64u32_t){ a0, a1 }
 */
typedef uint32_t rte_v64u32_t __attribute__((vector_size(8), aligned(8)));

/**
 * 128 bits vector size to use with unsigned 8 bits elements.
 *
 * a = (rte_v128u8_t){ a00, a01, a02, a03, a04, a05, a06, a07,
 *                     a08, a09, a10, a11, a12, a13, a14, a15 }
 */
typedef uint8_t rte_v128u8_t __attribute__((vector_size(16), aligned(16)));

/**
 * 128 bits vector size to use with unsigned 16 bits elements.
 *
 * a = (rte_v128u16_t){ a0, a1, a2, a3, a4, a5, a6, a7 }
 */
typedef uint16_t rte_v128u16_t __attribute__((vector_size(16), aligned(16)));

/**
 * 128 bits vector size to use with unsigned 32 bits elements.
 *
 * a = (rte_v128u32_t){ a0, a1, a2, a3 }
 */
typedef uint32_t rte_v128u32_t __attribute__((vector_size(16), aligned(16)));

/**
 * 128 bits vector size to use with unsigned 64 bits elements.
 *
 * a = (rte_v128u64_t){ a0, a1 }
 */
typedef uint64_t rte_v128u64_t __attribute__((vector_size(16), aligned(16)));

/**
 * 256 bits vector size to use with unsigned 8 bits elements.
 *
 * a = (rte_v256u8_t){ a00, a01, a02, a03, a04, a05, a06, a07,
 *                     a08, a09, a10, a11, a12, a13, a14, a15,
 *                     a16, a17, a18, a19, a20, a21, a22, a23,
 *                     a24, a25, a26, a27, a28, a29, a30, a31 }
 */
typedef uint8_t rte_v256u8_t __attribute__((vector_size(32), aligned(32)));

/**
 * 256 bits vector size to use with unsigned 16 bits elements.
 *
 * a = (rte_v256u16_t){ a00, a01, a02, a03, a04, a05, a06, a07,
 *                      a08, a09, a10, a11, a12, a13, a14, a15 }
 */
typedef uint16_t rte_v256u16_t __attribute__((vector_size(32), aligned(32)));

/**
 * 256 bits vector size to use with unsigned 32 bits elements.
 *
 * a = (rte_v256u32_t){ a0, a1, a2, a3, a4, a5, a6, a7 }
 */
typedef uint32_t rte_v256u32_t __attribute__((vector_size(32), aligned(32)));

/**
 * 256 bits vector size to use with unsigned 64 bits elements.
 *
 * a = (rte_v256u64_t){ a0, a1, a2, a3 }
 */
typedef uint64_t rte_v256u64_t __attribute__((vector_size(32), aligned(32)));


/* Signed vector types */

/**
 * 64 bits vector size to use with 8 bits elements.
 *
 * a = (rte_v64s8_t){ a0, a1, a2, a3, a4, a5, a6, a7 }
 */
typedef int8_t rte_v64s8_t __attribute__((vector_size(8), aligned(8)));

/**
 * 64 bits vector size to use with 16 bits elements.
 *
 * a = (rte_v64s16_t){ a0, a1, a2, a3 }
 */
typedef int16_t rte_v64s16_t __attribute__((vector_size(8), aligned(8)));

/**
 * 64 bits vector size to use with 32 bits elements.
 *
 * a = (rte_v64s32_t){ a0, a1 }
 */
typedef int32_t rte_v64s32_t __attribute__((vector_size(8), aligned(8)));

/**
 * 128 bits vector size to use with 8 bits elements.
 *
 * a = (rte_v128s8_t){ a00, a01, a02, a03, a04, a05, a06, a07,
 *                     a08, a09, a10, a11, a12, a13, a14, a15 }
 */
typedef int8_t rte_v128s8_t __attribute__((vector_size(16), aligned(16)));

/**
 * 128 bits vector size to use with 16 bits elements.
 *
 * a = (rte_v128s16_t){ a0, a1, a2, a3, a4, a5, a6, a7 }
 */
typedef int16_t rte_v128s16_t __attribute__((vector_size(16), aligned(16)));

/**
 * 128 bits vector size to use with 32 bits elements.
 *
 * a = (rte_v128s32_t){ a0, a1, a2, a3 }
 */
typedef int32_t rte_v128s32_t __attribute__((vector_size(16), aligned(16)));

/**
 * 128 bits vector size to use with 64 bits elements.
 *
 * a = (rte_v128s64_t){ a1, a2 }
 */
typedef int64_t rte_v128s64_t __attribute__((vector_size(16), aligned(16)));

/**
 * 256 bits vector size to use with 8 bits elements.
 *
 * a = (rte_v256s8_t){ a00, a01, a02, a03, a04, a05, a06, a07,
 *                     a08, a09, a10, a11, a12, a13, a14, a15,
 *                     a16, a17, a18, a19, a20, a21, a22, a23,
 *                     a24, a25, a26, a27, a28, a29, a30, a31 }
 */
typedef int8_t rte_v256s8_t __attribute__((vector_size(32), aligned(32)));

/**
 * 256 bits vector size to use with 16 bits elements.
 *
 * a = (rte_v256s16_t){ a00, a01, a02, a03, a04, a05, a06, a07,
 *                      a08, a09, a10, a11, a12, a13, a14, a15 }
 */
typedef int16_t rte_v256s16_t __attribute__((vector_size(32), aligned(32)));

/**
 * 256 bits vector size to use with 32 bits elements.
 *
 * a = (rte_v256s32_t){ a0, a1, a2, a3, a4, a5, a6, a7 }
 */
typedef int32_t rte_v256s32_t __attribute__((vector_size(32), aligned(32)));

/**
 * 256 bits vector size to use with 64 bits elements.
 *
 * a = (rte_v256s64_t){ a0, a1, a2, a3 }
 */
typedef int64_t rte_v256s64_t __attribute__((vector_size(32), aligned(32)));

#endif /* _RTE_VECT_H_ */
