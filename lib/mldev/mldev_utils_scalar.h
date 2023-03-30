/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <errno.h>
#include <math.h>
#include <stdint.h>

#include "mldev_utils.h"

#ifndef BIT
#define BIT(nr) (1UL << (nr))
#endif

#ifndef BITS_PER_LONG
#define BITS_PER_LONG (__SIZEOF_LONG__ * 8)
#endif

#ifndef GENMASK_U32
#define GENMASK_U32(h, l) (((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#endif

/* float32: bit index of MSB & LSB of sign, exponent and mantissa */
#define FP32_LSB_M 0
#define FP32_MSB_M 22
#define FP32_LSB_E 23
#define FP32_MSB_E 30
#define FP32_LSB_S 31
#define FP32_MSB_S 31

/* float32: bitmask for sign, exponent and mantissa */
#define FP32_MASK_S GENMASK_U32(FP32_MSB_S, FP32_LSB_S)
#define FP32_MASK_E GENMASK_U32(FP32_MSB_E, FP32_LSB_E)
#define FP32_MASK_M GENMASK_U32(FP32_MSB_M, FP32_LSB_M)

/* float16: bit index of MSB & LSB of sign, exponent and mantissa */
#define FP16_LSB_M 0
#define FP16_MSB_M 9
#define FP16_LSB_E 10
#define FP16_MSB_E 14
#define FP16_LSB_S 15
#define FP16_MSB_S 15

/* float16: bitmask for sign, exponent and mantissa */
#define FP16_MASK_S GENMASK_U32(FP16_MSB_S, FP16_LSB_S)
#define FP16_MASK_E GENMASK_U32(FP16_MSB_E, FP16_LSB_E)
#define FP16_MASK_M GENMASK_U32(FP16_MSB_M, FP16_LSB_M)

/* bfloat16: bit index of MSB & LSB of sign, exponent and mantissa */
#define BF16_LSB_M 0
#define BF16_MSB_M 6
#define BF16_LSB_E 7
#define BF16_MSB_E 14
#define BF16_LSB_S 15
#define BF16_MSB_S 15

/* bfloat16: bitmask for sign, exponent and mantissa */
#define BF16_MASK_S GENMASK_U32(BF16_MSB_S, BF16_LSB_S)
#define BF16_MASK_E GENMASK_U32(BF16_MSB_E, BF16_LSB_E)
#define BF16_MASK_M GENMASK_U32(BF16_MSB_M, BF16_LSB_M)

/* Exponent bias */
#define FP32_BIAS_E 127
#define FP16_BIAS_E 15
#define BF16_BIAS_E 127

#define FP32_PACK(sign, exponent, mantissa)                                                        \
	(((sign) << FP32_LSB_S) | ((exponent) << FP32_LSB_E) | (mantissa))

#define FP16_PACK(sign, exponent, mantissa)                                                        \
	(((sign) << FP16_LSB_S) | ((exponent) << FP16_LSB_E) | (mantissa))

#define BF16_PACK(sign, exponent, mantissa)                                                        \
	(((sign) << BF16_LSB_S) | ((exponent) << BF16_LSB_E) | (mantissa))

/* Represent float32 as float and uint32_t */
union float32 {
	float f;
	uint32_t u;
};
