/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <errno.h>
#include <math.h>
#include <stdint.h>

#include "mldev_utils_scalar.h"

/* Description:
 * This file implements scalar versions of Machine Learning utility functions used to convert data
 * types from bfloat16 to float32 and vice-versa.
 */

/* Convert a single precision floating point number (float32) into a
 * brain float number (bfloat16) using round to nearest rounding mode.
 */
static uint16_t
__float32_to_bfloat16_scalar_rtn(float x)
{
	union float32 f32; /* float32 input */
	uint32_t f32_s;	   /* float32 sign */
	uint32_t f32_e;	   /* float32 exponent */
	uint32_t f32_m;	   /* float32 mantissa */
	uint16_t b16_s;	   /* float16 sign */
	uint16_t b16_e;	   /* float16 exponent */
	uint16_t b16_m;	   /* float16 mantissa */
	uint32_t tbits;	   /* number of truncated bits */
	uint16_t u16;	   /* float16 output */

	f32.f = x;
	f32_s = (f32.u & FP32_MASK_S) >> FP32_LSB_S;
	f32_e = (f32.u & FP32_MASK_E) >> FP32_LSB_E;
	f32_m = (f32.u & FP32_MASK_M) >> FP32_LSB_M;

	b16_s = f32_s;
	b16_e = 0;
	b16_m = 0;

	switch (f32_e) {
	case (0): /* float32: zero or subnormal number */
		b16_e = 0;
		if (f32_m == 0) /* zero */
			b16_m = 0;
		else /* subnormal float32 number, normal bfloat16 */
			goto bf16_normal;
		break;
	case (FP32_MASK_E >> FP32_LSB_E): /* float32: infinity or nan */
		b16_e = BF16_MASK_E >> BF16_LSB_E;
		if (f32_m == 0) { /* infinity */
			b16_m = 0;
		} else { /* nan, propagate mantissa and set MSB of mantissa to 1 */
			b16_m = f32_m >> (FP32_MSB_M - BF16_MSB_M);
			b16_m |= BIT(BF16_MSB_M);
		}
		break;
	default: /* float32: normal number, normal bfloat16 */
		goto bf16_normal;
	}

	goto bf16_pack;

bf16_normal:
	b16_e = f32_e;
	tbits = FP32_MSB_M - BF16_MSB_M;
	b16_m = f32_m >> tbits;

	/* if non-leading truncated bits are set */
	if ((f32_m & GENMASK_U32(tbits - 1, 0)) > BIT(tbits - 1)) {
		b16_m++;

		/* if overflow into exponent */
		if (((b16_m & BF16_MASK_E) >> BF16_LSB_E) == 0x1)
			b16_e++;
	} else if ((f32_m & GENMASK_U32(tbits - 1, 0)) == BIT(tbits - 1)) {
		/* if only leading truncated bit is set */
		if ((b16_m & 0x1) == 0x1) {
			b16_m++;

			/* if overflow into exponent */
			if (((b16_m & BF16_MASK_E) >> BF16_LSB_E) == 0x1)
				b16_e++;
		}
	}
	b16_m = b16_m & BF16_MASK_M;

bf16_pack:
	u16 = BF16_PACK(b16_s, b16_e, b16_m);

	return u16;
}

int
rte_ml_io_float32_to_bfloat16(uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	uint16_t *output_buffer;
	uint64_t i;

	if ((nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (uint16_t *)output;

	for (i = 0; i < nb_elements; i++) {
		*output_buffer = __float32_to_bfloat16_scalar_rtn(*input_buffer);

		input_buffer = input_buffer + 1;
		output_buffer = output_buffer + 1;
	}

	return 0;
}

/* Convert a brain float number (bfloat16) into a
 * single precision floating point number (float32).
 */
static float
__bfloat16_to_float32_scalar_rtx(uint16_t f16)
{
	union float32 f32; /* float32 output */
	uint16_t b16_s;	   /* float16 sign */
	uint16_t b16_e;	   /* float16 exponent */
	uint16_t b16_m;	   /* float16 mantissa */
	uint32_t f32_s;	   /* float32 sign */
	uint32_t f32_e;	   /* float32 exponent */
	uint32_t f32_m;	   /* float32 mantissa*/
	uint8_t shift;	   /* number of bits to be shifted */

	b16_s = (f16 & BF16_MASK_S) >> BF16_LSB_S;
	b16_e = (f16 & BF16_MASK_E) >> BF16_LSB_E;
	b16_m = (f16 & BF16_MASK_M) >> BF16_LSB_M;

	f32_s = b16_s;
	switch (b16_e) {
	case (BF16_MASK_E >> BF16_LSB_E): /* bfloat16: infinity or nan */
		f32_e = FP32_MASK_E >> FP32_LSB_E;
		if (b16_m == 0x0) { /* infinity */
			f32_m = 0;
		} else { /* nan, propagate mantissa, set MSB of mantissa to 1 */
			f32_m = b16_m;
			shift = FP32_MSB_M - BF16_MSB_M;
			f32_m = (f32_m << shift) & FP32_MASK_M;
			f32_m |= BIT(FP32_MSB_M);
		}
		break;
	case 0: /* bfloat16: zero or subnormal */
		f32_m = b16_m;
		if (b16_m == 0) { /* zero signed */
			f32_e = 0;
		} else { /* subnormal numbers */
			goto fp32_normal;
		}
		break;
	default: /* bfloat16: normal number */
		goto fp32_normal;
	}

	goto fp32_pack;

fp32_normal:
	f32_m = b16_m;
	f32_e = FP32_BIAS_E + b16_e - BF16_BIAS_E;

	shift = (FP32_MSB_M - BF16_MSB_M);
	f32_m = (f32_m << shift) & FP32_MASK_M;

fp32_pack:
	f32.u = FP32_PACK(f32_s, f32_e, f32_m);

	return f32.f;
}

int
rte_ml_io_bfloat16_to_float32(uint64_t nb_elements, void *input, void *output)
{
	uint16_t *input_buffer;
	float *output_buffer;
	uint64_t i;

	if ((nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (uint16_t *)input;
	output_buffer = (float *)output;

	for (i = 0; i < nb_elements; i++) {
		*output_buffer = __bfloat16_to_float32_scalar_rtx(*input_buffer);

		input_buffer = input_buffer + 1;
		output_buffer = output_buffer + 1;
	}

	return 0;
}
