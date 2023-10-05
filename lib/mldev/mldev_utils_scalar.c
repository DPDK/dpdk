/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include "mldev_utils_scalar.h"

/* Description:
 * This file implements scalar versions of Machine Learning utility functions used to convert data
 * types from higher precision to lower precision and vice-versa, except bfloat16.
 */

int
rte_ml_io_float32_to_int8(float scale, uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	int8_t *output_buffer;
	uint64_t i;
	int i32;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (int8_t *)output;

	for (i = 0; i < nb_elements; i++) {
		i32 = (int32_t)round((*input_buffer) * scale);

		if (i32 < INT8_MIN)
			i32 = INT8_MIN;

		if (i32 > INT8_MAX)
			i32 = INT8_MAX;

		*output_buffer = (int8_t)i32;

		input_buffer++;
		output_buffer++;
	}

	return 0;
}

int
rte_ml_io_int8_to_float32(float scale, uint64_t nb_elements, void *input, void *output)
{
	int8_t *input_buffer;
	float *output_buffer;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (int8_t *)input;
	output_buffer = (float *)output;

	for (i = 0; i < nb_elements; i++) {
		*output_buffer = scale * (float)(*input_buffer);

		input_buffer++;
		output_buffer++;
	}

	return 0;
}

int
rte_ml_io_float32_to_uint8(float scale, uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	uint8_t *output_buffer;
	int32_t i32;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (uint8_t *)output;

	for (i = 0; i < nb_elements; i++) {
		i32 = (int32_t)round((*input_buffer) * scale);

		if (i32 < 0)
			i32 = 0;

		if (i32 > UINT8_MAX)
			i32 = UINT8_MAX;

		*output_buffer = (uint8_t)i32;

		input_buffer++;
		output_buffer++;
	}

	return 0;
}

int
rte_ml_io_uint8_to_float32(float scale, uint64_t nb_elements, void *input, void *output)
{
	uint8_t *input_buffer;
	float *output_buffer;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (uint8_t *)input;
	output_buffer = (float *)output;

	for (i = 0; i < nb_elements; i++) {
		*output_buffer = scale * (float)(*input_buffer);

		input_buffer++;
		output_buffer++;
	}

	return 0;
}

int
rte_ml_io_float32_to_int16(float scale, uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	int16_t *output_buffer;
	int32_t i32;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (int16_t *)output;

	for (i = 0; i < nb_elements; i++) {
		i32 = (int32_t)round((*input_buffer) * scale);

		if (i32 < INT16_MIN)
			i32 = INT16_MIN;

		if (i32 > INT16_MAX)
			i32 = INT16_MAX;

		*output_buffer = (int16_t)i32;

		input_buffer++;
		output_buffer++;
	}

	return 0;
}

int
rte_ml_io_int16_to_float32(float scale, uint64_t nb_elements, void *input, void *output)
{
	int16_t *input_buffer;
	float *output_buffer;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (int16_t *)input;
	output_buffer = (float *)output;

	for (i = 0; i < nb_elements; i++) {
		*output_buffer = scale * (float)(*input_buffer);

		input_buffer++;
		output_buffer++;
	}

	return 0;
}

int
rte_ml_io_float32_to_uint16(float scale, uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	uint16_t *output_buffer;
	int32_t i32;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (uint16_t *)output;

	for (i = 0; i < nb_elements; i++) {
		i32 = (int32_t)round((*input_buffer) * scale);

		if (i32 < 0)
			i32 = 0;

		if (i32 > UINT16_MAX)
			i32 = UINT16_MAX;

		*output_buffer = (uint16_t)i32;

		input_buffer++;
		output_buffer++;
	}

	return 0;
}

int
rte_ml_io_uint16_to_float32(float scale, uint64_t nb_elements, void *input, void *output)
{
	uint16_t *input_buffer;
	float *output_buffer;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (uint16_t *)input;
	output_buffer = (float *)output;

	for (i = 0; i < nb_elements; i++) {
		*output_buffer = scale * (float)(*input_buffer);

		input_buffer++;
		output_buffer++;
	}

	return 0;
}

/* Convert a single precision floating point number (float32) into a half precision
 * floating point number (float16) using round to nearest rounding mode.
 */
static uint16_t
__float32_to_float16_scalar_rtn(float x)
{
	union float32 f32; /* float32 input */
	uint32_t f32_s;	   /* float32 sign */
	uint32_t f32_e;	   /* float32 exponent */
	uint32_t f32_m;	   /* float32 mantissa */
	uint16_t f16_s;	   /* float16 sign */
	uint16_t f16_e;	   /* float16 exponent */
	uint16_t f16_m;	   /* float16 mantissa */
	uint32_t tbits;	   /* number of truncated bits */
	uint32_t tmsb;	   /* MSB position of truncated bits */
	uint32_t m_32;	   /* temporary float32 mantissa */
	uint16_t m_16;	   /* temporary float16 mantissa */
	uint16_t u16;	   /* float16 output */
	int be_16;	   /* float16 biased exponent, signed */

	f32.f = x;
	f32_s = (f32.u & FP32_MASK_S) >> FP32_LSB_S;
	f32_e = (f32.u & FP32_MASK_E) >> FP32_LSB_E;
	f32_m = (f32.u & FP32_MASK_M) >> FP32_LSB_M;

	f16_s = f32_s;
	f16_e = 0;
	f16_m = 0;

	switch (f32_e) {
	case (0): /* float32: zero or subnormal number */
		f16_e = 0;
		f16_m = 0; /* convert to zero */
		break;
	case (FP32_MASK_E >> FP32_LSB_E): /* float32: infinity or nan */
		f16_e = FP16_MASK_E >> FP16_LSB_E;
		if (f32_m == 0) { /* infinity */
			f16_m = 0;
		} else { /* nan, propagate mantissa and set MSB of mantissa to 1 */
			f16_m = f32_m >> (FP32_MSB_M - FP16_MSB_M);
			f16_m |= BIT(FP16_MSB_M);
		}
		break;
	default: /* float32: normal number */
		/* compute biased exponent for float16 */
		be_16 = (int)f32_e - FP32_BIAS_E + FP16_BIAS_E;

		/* overflow, be_16 = [31-INF], set to infinity */
		if (be_16 >= (int)(FP16_MASK_E >> FP16_LSB_E)) {
			f16_e = FP16_MASK_E >> FP16_LSB_E;
			f16_m = 0;
		} else if ((be_16 >= 1) && (be_16 < (int)(FP16_MASK_E >> FP16_LSB_E))) {
			/* normal float16, be_16 = [1:30]*/
			f16_e = be_16;
			m_16 = f32_m >> (FP32_LSB_E - FP16_LSB_E);
			tmsb = FP32_MSB_M - FP16_MSB_M - 1;
			if ((f32_m & GENMASK_U32(tmsb, 0)) > BIT(tmsb)) {
				/* round: non-zero truncated bits except MSB */
				m_16++;

				/* overflow into exponent */
				if (((m_16 & FP16_MASK_E) >> FP16_LSB_E) == 0x1)
					f16_e++;
			} else if ((f32_m & GENMASK_U32(tmsb, 0)) == BIT(tmsb)) {
				/* round: MSB of truncated bits and LSB of m_16 is set */
				if ((m_16 & 0x1) == 0x1) {
					m_16++;

					/* overflow into exponent */
					if (((m_16 & FP16_MASK_E) >> FP16_LSB_E) == 0x1)
						f16_e++;
				}
			}
			f16_m = m_16 & FP16_MASK_M;
		} else if ((be_16 >= -(int)(FP16_MSB_M)) && (be_16 < 1)) {
			/* underflow: zero / subnormal, be_16 = [-9:0] */
			f16_e = 0;

			/* add implicit leading zero */
			m_32 = f32_m | BIT(FP32_LSB_E);
			tbits = FP32_LSB_E - FP16_LSB_E - be_16 + 1;
			m_16 = m_32 >> tbits;

			/* if non-leading truncated bits are set */
			if ((f32_m & GENMASK_U32(tbits - 1, 0)) > BIT(tbits - 1)) {
				m_16++;

				/* overflow into exponent */
				if (((m_16 & FP16_MASK_E) >> FP16_LSB_E) == 0x1)
					f16_e++;
			} else if ((f32_m & GENMASK_U32(tbits - 1, 0)) == BIT(tbits - 1)) {
				/* if leading truncated bit is set */
				if ((m_16 & 0x1) == 0x1) {
					m_16++;

					/* overflow into exponent */
					if (((m_16 & FP16_MASK_E) >> FP16_LSB_E) == 0x1)
						f16_e++;
				}
			}
			f16_m = m_16 & FP16_MASK_M;
		} else if (be_16 == -(int)(FP16_MSB_M + 1)) {
			/* underflow: zero, be_16 = [-10] */
			f16_e = 0;
			if (f32_m != 0)
				f16_m = 1;
			else
				f16_m = 0;
		} else {
			/* underflow: zero, be_16 = [-INF:-11] */
			f16_e = 0;
			f16_m = 0;
		}

		break;
	}

	u16 = FP16_PACK(f16_s, f16_e, f16_m);

	return u16;
}

int
rte_ml_io_float32_to_float16(uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	uint16_t *output_buffer;
	uint64_t i;

	if ((nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (uint16_t *)output;

	for (i = 0; i < nb_elements; i++) {
		*output_buffer = __float32_to_float16_scalar_rtn(*input_buffer);

		input_buffer = input_buffer + 1;
		output_buffer = output_buffer + 1;
	}

	return 0;
}

/* Convert a half precision floating point number (float16) into a single precision
 * floating point number (float32).
 */
static float
__float16_to_float32_scalar_rtx(uint16_t f16)
{
	union float32 f32; /* float32 output */
	uint16_t f16_s;	   /* float16 sign */
	uint16_t f16_e;	   /* float16 exponent */
	uint16_t f16_m;	   /* float16 mantissa */
	uint32_t f32_s;	   /* float32 sign */
	uint32_t f32_e;	   /* float32 exponent */
	uint32_t f32_m;	   /* float32 mantissa*/
	uint8_t shift;	   /* number of bits to be shifted */
	uint32_t clz;	   /* count of leading zeroes */
	int e_16;	   /* float16 exponent unbiased */

	f16_s = (f16 & FP16_MASK_S) >> FP16_LSB_S;
	f16_e = (f16 & FP16_MASK_E) >> FP16_LSB_E;
	f16_m = (f16 & FP16_MASK_M) >> FP16_LSB_M;

	f32_s = f16_s;
	switch (f16_e) {
	case (FP16_MASK_E >> FP16_LSB_E): /* float16: infinity or nan */
		f32_e = FP32_MASK_E >> FP32_LSB_E;
		if (f16_m == 0x0) { /* infinity */
			f32_m = f16_m;
		} else { /* nan, propagate mantissa, set MSB of mantissa to 1 */
			f32_m = f16_m;
			shift = FP32_MSB_M - FP16_MSB_M;
			f32_m = (f32_m << shift) & FP32_MASK_M;
			f32_m |= BIT(FP32_MSB_M);
		}
		break;
	case 0: /* float16: zero or sub-normal */
		f32_m = f16_m;
		if (f16_m == 0) { /* zero signed */
			f32_e = 0;
		} else { /* subnormal numbers */
			clz = rte_clz32((uint32_t)f16_m) - sizeof(uint32_t) * 8 + FP16_LSB_E;
			e_16 = (int)f16_e - clz;
			f32_e = FP32_BIAS_E + e_16 - FP16_BIAS_E;

			shift = clz + (FP32_MSB_M - FP16_MSB_M) + 1;
			f32_m = (f32_m << shift) & FP32_MASK_M;
		}
		break;
	default: /* normal numbers */
		f32_m = f16_m;
		e_16 = (int)f16_e;
		f32_e = FP32_BIAS_E + e_16 - FP16_BIAS_E;

		shift = (FP32_MSB_M - FP16_MSB_M);
		f32_m = (f32_m << shift) & FP32_MASK_M;
	}

	f32.u = FP32_PACK(f32_s, f32_e, f32_m);

	return f32.f;
}

int
rte_ml_io_float16_to_float32(uint64_t nb_elements, void *input, void *output)
{
	uint16_t *input_buffer;
	float *output_buffer;
	uint64_t i;

	if ((nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (uint16_t *)input;
	output_buffer = (float *)output;

	for (i = 0; i < nb_elements; i++) {
		*output_buffer = __float16_to_float32_scalar_rtx(*input_buffer);

		input_buffer = input_buffer + 1;
		output_buffer = output_buffer + 1;
	}

	return 0;
}
