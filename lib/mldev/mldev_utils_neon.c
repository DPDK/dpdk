/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "mldev_utils.h"

#include <arm_neon.h>

/* Description:
 * This file implements vector versions of Machine Learning utility functions used to convert data
 * types from higher precision to lower precision and vice-versa, except bfloat16. Implementation
 * is based on Arm Neon intrinsics.
 */

static inline void
__float32_to_int8_neon_s8x8(float scale, float *input, int8_t *output)
{
	int16x4_t s16x4_l;
	int16x4_t s16x4_h;
	float32x4_t f32x4;
	int16x8_t s16x8;
	int32x4_t s32x4;
	int8x8_t s8x8;

	/* load 4 float32 elements, scale, convert, saturate narrow to int16.
	 * Use round to nearest with ties away rounding mode.
	 */
	f32x4 = vld1q_f32(input);
	f32x4 = vmulq_n_f32(f32x4, scale);
	s32x4 = vcvtaq_s32_f32(f32x4);
	s16x4_l = vqmovn_s32(s32x4);

	/* load next 4 float32 elements, scale, convert, saturate narrow to int16.
	 * Use round to nearest with ties away rounding mode.
	 */
	f32x4 = vld1q_f32(input + 4);
	f32x4 = vmulq_n_f32(f32x4, scale);
	s32x4 = vcvtaq_s32_f32(f32x4);
	s16x4_h = vqmovn_s32(s32x4);

	/* combine lower and higher int16x4_t to int16x8_t */
	s16x8 = vcombine_s16(s16x4_l, s16x4_h);

	/* narrow to int8_t */
	s8x8 = vqmovn_s16(s16x8);

	/* store 8 elements */
	vst1_s8(output, s8x8);
}

static inline void
__float32_to_int8_neon_s8x1(float scale, float *input, int8_t *output)
{
	int32_t s32;
	int16_t s16;

	/* scale and convert, round to nearest with ties away rounding mode */
	s32 = vcvtas_s32_f32(scale * (*input));

	/* saturate narrow */
	s16 = vqmovns_s32(s32);

	/* convert to int8_t */
	*output = vqmovnh_s16(s16);
}

int
rte_ml_io_float32_to_int8(float scale, uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	int8_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (int8_t *)output;
	vlen = 2 * sizeof(float) / sizeof(int8_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_int8_neon_s8x8(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_int8_neon_s8x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__int8_to_float32_neon_f32x8(float scale, int8_t *input, float *output)
{
	float32x4_t f32x4;
	int16x8_t s16x8;
	int16x4_t s16x4;
	int32x4_t s32x4;
	int8x8_t s8x8;

	/* load 8 x int8_t elements */
	s8x8 = vld1_s8(input);

	/* widen int8_t to int16_t */
	s16x8 = vmovl_s8(s8x8);

	/* convert lower 4 elements: widen to int32_t, convert to float, scale and store */
	s16x4 = vget_low_s16(s16x8);
	s32x4 = vmovl_s16(s16x4);
	f32x4 = vcvtq_f32_s32(s32x4);
	f32x4 = vmulq_n_f32(f32x4, scale);
	vst1q_f32(output, f32x4);

	/* convert higher 4 elements: widen to int32_t, convert to float, scale and store */
	s16x4 = vget_high_s16(s16x8);
	s32x4 = vmovl_s16(s16x4);
	f32x4 = vcvtq_f32_s32(s32x4);
	f32x4 = vmulq_n_f32(f32x4, scale);
	vst1q_f32(output + 4, f32x4);
}

static inline void
__int8_to_float32_neon_f32x1(float scale, int8_t *input, float *output)
{
	*output = scale * vcvts_f32_s32((int32_t)*input);
}

int
rte_ml_io_int8_to_float32(float scale, uint64_t nb_elements, void *input, void *output)
{
	int8_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (int8_t *)input;
	output_buffer = (float *)output;
	vlen = 2 * sizeof(float) / sizeof(int8_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__int8_to_float32_neon_f32x8(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__int8_to_float32_neon_f32x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_uint8_neon_u8x8(float scale, float *input, uint8_t *output)
{
	uint16x4_t u16x4_l;
	uint16x4_t u16x4_h;
	float32x4_t f32x4;
	uint32x4_t u32x4;
	uint16x8_t u16x8;
	uint8x8_t u8x8;

	/* load 4 float elements, scale, convert, saturate narrow to uint16_t.
	 * use round to nearest with ties away rounding mode.
	 */
	f32x4 = vld1q_f32(input);
	f32x4 = vmulq_n_f32(f32x4, scale);
	u32x4 = vcvtaq_u32_f32(f32x4);
	u16x4_l = vqmovn_u32(u32x4);

	/* load next 4 float elements, scale, convert, saturate narrow to uint16_t
	 * use round to nearest with ties away rounding mode.
	 */
	f32x4 = vld1q_f32(input + 4);
	f32x4 = vmulq_n_f32(f32x4, scale);
	u32x4 = vcvtaq_u32_f32(f32x4);
	u16x4_h = vqmovn_u32(u32x4);

	/* combine lower and higher uint16x4_t */
	u16x8 = vcombine_u16(u16x4_l, u16x4_h);

	/* narrow to uint8x8_t */
	u8x8 = vqmovn_u16(u16x8);

	/* store 8 elements */
	vst1_u8(output, u8x8);
}

static inline void
__float32_to_uint8_neon_u8x1(float scale, float *input, uint8_t *output)
{
	uint32_t u32;
	uint16_t u16;

	/* scale and convert, round to nearest with ties away rounding mode */
	u32 = vcvtas_u32_f32(scale * (*input));

	/* saturate narrow */
	u16 = vqmovns_u32(u32);

	/* convert to uint8_t */
	*output = vqmovnh_u16(u16);
}

int
rte_ml_io_float32_to_uint8(float scale, uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	uint8_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (uint8_t *)output;
	vlen = 2 * sizeof(float) / sizeof(uint8_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_uint8_neon_u8x8(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_uint8_neon_u8x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__uint8_to_float32_neon_f32x8(float scale, uint8_t *input, float *output)
{
	float32x4_t f32x4;
	uint16x8_t u16x8;
	uint16x4_t u16x4;
	uint32x4_t u32x4;
	uint8x8_t u8x8;

	/* load 8 x uint8_t elements */
	u8x8 = vld1_u8(input);

	/* widen uint8_t to uint16_t */
	u16x8 = vmovl_u8(u8x8);

	/* convert lower 4 elements: widen to uint32_t, convert to float, scale and store */
	u16x4 = vget_low_u16(u16x8);
	u32x4 = vmovl_u16(u16x4);
	f32x4 = vcvtq_f32_u32(u32x4);
	f32x4 = vmulq_n_f32(f32x4, scale);
	vst1q_f32(output, f32x4);

	/* convert higher 4 elements: widen to uint32_t, convert to float, scale and store */
	u16x4 = vget_high_u16(u16x8);
	u32x4 = vmovl_u16(u16x4);
	f32x4 = vcvtq_f32_u32(u32x4);
	f32x4 = vmulq_n_f32(f32x4, scale);
	vst1q_f32(output + 4, f32x4);
}

static inline void
__uint8_to_float32_neon_f32x1(float scale, uint8_t *input, float *output)
{
	*output = scale * vcvts_f32_u32((uint32_t)*input);
}

int
rte_ml_io_uint8_to_float32(float scale, uint64_t nb_elements, void *input, void *output)
{
	uint8_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint64_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (uint8_t *)input;
	output_buffer = (float *)output;
	vlen = 2 * sizeof(float) / sizeof(uint8_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__uint8_to_float32_neon_f32x8(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__uint8_to_float32_neon_f32x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_int16_neon_s16x4(float scale, float *input, int16_t *output)
{
	float32x4_t f32x4;
	int16x4_t s16x4;
	int32x4_t s32x4;

	/* load 4 x float elements */
	f32x4 = vld1q_f32(input);

	/* scale */
	f32x4 = vmulq_n_f32(f32x4, scale);

	/* convert to int32x4_t using round to nearest with ties away rounding mode */
	s32x4 = vcvtaq_s32_f32(f32x4);

	/* saturate narrow to int16x4_t */
	s16x4 = vqmovn_s32(s32x4);

	/* store 4 elements */
	vst1_s16(output, s16x4);
}

static inline void
__float32_to_int16_neon_s16x1(float scale, float *input, int16_t *output)
{
	int32_t s32;

	/* scale and convert, round to nearest with ties away rounding mode */
	s32 = vcvtas_s32_f32(scale * (*input));

	/* saturate narrow */
	*output = vqmovns_s32(s32);
}

int
rte_ml_io_float32_to_int16(float scale, uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	int16_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (int16_t *)output;
	vlen = 2 * sizeof(float) / sizeof(int16_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_int16_neon_s16x4(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_int16_neon_s16x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__int16_to_float32_neon_f32x4(float scale, int16_t *input, float *output)
{
	float32x4_t f32x4;
	int16x4_t s16x4;
	int32x4_t s32x4;

	/* load 4 x int16_t elements */
	s16x4 = vld1_s16(input);

	/* widen int16_t to int32_t */
	s32x4 = vmovl_s16(s16x4);

	/* convert int32_t to float */
	f32x4 = vcvtq_f32_s32(s32x4);

	/* scale */
	f32x4 = vmulq_n_f32(f32x4, scale);

	/* store float32x4_t */
	vst1q_f32(output, f32x4);
}

static inline void
__int16_to_float32_neon_f32x1(float scale, int16_t *input, float *output)
{
	*output = scale * vcvts_f32_s32((int32_t)*input);
}

int
rte_ml_io_int16_to_float32(float scale, uint64_t nb_elements, void *input, void *output)
{
	int16_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (int16_t *)input;
	output_buffer = (float *)output;
	vlen = 2 * sizeof(float) / sizeof(int16_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__int16_to_float32_neon_f32x4(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__int16_to_float32_neon_f32x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_uint16_neon_u16x4(float scale, float *input, uint16_t *output)
{
	float32x4_t f32x4;
	uint16x4_t u16x4;
	uint32x4_t u32x4;

	/* load 4 float elements */
	f32x4 = vld1q_f32(input);

	/* scale */
	f32x4 = vmulq_n_f32(f32x4, scale);

	/* convert using round to nearest with ties to away rounding mode */
	u32x4 = vcvtaq_u32_f32(f32x4);

	/* saturate narrow */
	u16x4 = vqmovn_u32(u32x4);

	/* store 4 elements */
	vst1_u16(output, u16x4);
}

static inline void
__float32_to_uint16_neon_u16x1(float scale, float *input, uint16_t *output)
{
	uint32_t u32;

	/* scale and convert, round to nearest with ties away rounding mode */
	u32 = vcvtas_u32_f32(scale * (*input));

	/* saturate narrow */
	*output = vqmovns_u32(u32);
}

int
rte_ml_io_float32_to_uint16(float scale, uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	uint16_t *output_buffer;
	uint64_t nb_iterations;
	uint64_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (uint16_t *)output;
	vlen = 2 * sizeof(float) / sizeof(uint16_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_uint16_neon_u16x4(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_uint16_neon_u16x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__uint16_to_float32_neon_f32x4(float scale, uint16_t *input, float *output)
{
	float32x4_t f32x4;
	uint16x4_t u16x4;
	uint32x4_t u32x4;

	/* load 4 x uint16_t elements */
	u16x4 = vld1_u16(input);

	/* widen uint16_t to uint32_t */
	u32x4 = vmovl_u16(u16x4);

	/* convert uint32_t to float */
	f32x4 = vcvtq_f32_u32(u32x4);

	/* scale */
	f32x4 = vmulq_n_f32(f32x4, scale);

	/* store float32x4_t */
	vst1q_f32(output, f32x4);
}

static inline void
__uint16_to_float32_neon_f32x1(float scale, uint16_t *input, float *output)
{
	*output = scale * vcvts_f32_u32((uint32_t)*input);
}

int
rte_ml_io_uint16_to_float32(float scale, uint64_t nb_elements, void *input, void *output)
{
	uint16_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (uint16_t *)input;
	output_buffer = (float *)output;
	vlen = 2 * sizeof(float) / sizeof(uint16_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__uint16_to_float32_neon_f32x4(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__uint16_to_float32_neon_f32x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_int32_neon_s32x4(float scale, float *input, int32_t *output)
{
	float32x4_t f32x4;
	int32x4_t s32x4;

	/* load 4 x float elements */
	f32x4 = vld1q_f32(input);

	/* scale */
	f32x4 = vmulq_n_f32(f32x4, scale);

	/* convert to int32x4_t using round to nearest with ties away rounding mode */
	s32x4 = vcvtaq_s32_f32(f32x4);

	/* store 4 elements */
	vst1q_s32(output, s32x4);
}

static inline void
__float32_to_int32_neon_s32x1(float scale, float *input, int32_t *output)
{
	/* scale and convert, round to nearest with ties away rounding mode */
	*output = vcvtas_s32_f32(scale * (*input));
}

int
rte_ml_io_float32_to_int32(float scale, uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	int32_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (int32_t *)output;
	vlen = 2 * sizeof(float) / sizeof(int32_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_int32_neon_s32x4(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_int32_neon_s32x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__int32_to_float32_neon_f32x4(float scale, int32_t *input, float *output)
{
	float32x4_t f32x4;
	int32x4_t s32x4;

	/* load 4 x int32_t elements */
	s32x4 = vld1q_s32(input);

	/* convert int32_t to float */
	f32x4 = vcvtq_f32_s32(s32x4);

	/* scale */
	f32x4 = vmulq_n_f32(f32x4, scale);

	/* store float32x4_t */
	vst1q_f32(output, f32x4);
}

static inline void
__int32_to_float32_neon_f32x1(float scale, int32_t *input, float *output)
{
	*output = scale * vcvts_f32_s32(*input);
}

int
rte_ml_io_int32_to_float32(float scale, uint64_t nb_elements, void *input, void *output)
{
	int32_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (int32_t *)input;
	output_buffer = (float *)output;
	vlen = 2 * sizeof(float) / sizeof(int32_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__int32_to_float32_neon_f32x4(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__int32_to_float32_neon_f32x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_uint32_neon_u32x4(float scale, float *input, uint32_t *output)
{
	float32x4_t f32x4;
	uint32x4_t u32x4;

	/* load 4 float elements */
	f32x4 = vld1q_f32(input);

	/* scale */
	f32x4 = vmulq_n_f32(f32x4, scale);

	/* convert using round to nearest with ties to away rounding mode */
	u32x4 = vcvtaq_u32_f32(f32x4);

	/* store 4 elements */
	vst1q_u32(output, u32x4);
}

static inline void
__float32_to_uint32_neon_u32x1(float scale, float *input, uint32_t *output)
{
	/* scale and convert, round to nearest with ties away rounding mode */
	*output = vcvtas_u32_f32(scale * (*input));
}

int
rte_ml_io_float32_to_uint32(float scale, uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	uint32_t *output_buffer;
	uint64_t nb_iterations;
	uint64_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (uint32_t *)output;
	vlen = 2 * sizeof(float) / sizeof(uint32_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_uint32_neon_u32x4(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_uint32_neon_u32x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__uint32_to_float32_neon_f32x4(float scale, uint32_t *input, float *output)
{
	float32x4_t f32x4;
	uint32x4_t u32x4;

	/* load 4 x uint32_t elements */
	u32x4 = vld1q_u32(input);

	/* convert uint32_t to float */
	f32x4 = vcvtq_f32_u32(u32x4);

	/* scale */
	f32x4 = vmulq_n_f32(f32x4, scale);

	/* store float32x4_t */
	vst1q_f32(output, f32x4);
}

static inline void
__uint32_to_float32_neon_f32x1(float scale, uint32_t *input, float *output)
{
	*output = scale * vcvts_f32_u32(*input);
}

int
rte_ml_io_uint32_to_float32(float scale, uint64_t nb_elements, void *input, void *output)
{
	uint32_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (uint32_t *)input;
	output_buffer = (float *)output;
	vlen = 2 * sizeof(float) / sizeof(uint32_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__uint32_to_float32_neon_f32x4(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__uint32_to_float32_neon_f32x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_int64_neon_s64x2(float scale, float *input, int64_t *output)
{
	float32x2_t f32x2;
	float64x2_t f64x2;
	int64x2_t s64x2;

	/* load 2 x float elements */
	f32x2 = vld1_f32(input);

	/* scale */
	f32x2 = vmul_n_f32(f32x2, scale);

	/* convert to float64x2_t */
	f64x2 = vcvt_f64_f32(f32x2);

	/* convert to int64x2_t */
	s64x2 = vcvtaq_s64_f64(f64x2);

	/* store 2 elements */
	vst1q_s64(output, s64x2);
}

static inline void
__float32_to_int64_neon_s64x1(float scale, float *input, int64_t *output)
{
	float32x2_t f32x2;
	float64x2_t f64x2;
	int64x2_t s64x2;

	/* load 1 x float element */
	f32x2 = vdup_n_f32(*input);

	/* scale */
	f32x2 = vmul_n_f32(f32x2, scale);

	/* convert to float64x2_t */
	f64x2 = vcvt_f64_f32(f32x2);

	/* convert to int64x2_t */
	s64x2 = vcvtaq_s64_f64(f64x2);

	/* store lane 0 of int64x2_t */
	vst1q_lane_s64(output, s64x2, 0);
}

int
rte_ml_io_float32_to_int64(float scale, uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	int64_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (int64_t *)output;
	vlen = 4 * sizeof(float) / sizeof(int64_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_int64_neon_s64x2(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_int64_neon_s64x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__int64_to_float32_neon_f32x2(float scale, int64_t *input, float *output)
{
	int64x2_t s64x2;
	float64x2_t f64x2;
	float32x2_t f32x2;

	/* load 2 x int64_t elements */
	s64x2 = vld1q_s64(input);

	/* convert int64x2_t to float64x2_t */
	f64x2 = vcvtq_f64_s64(s64x2);

	/* convert float64x2_t to float32x2_t */
	f32x2 = vcvt_f32_f64(f64x2);

	/* scale */
	f32x2 = vmul_n_f32(f32x2, scale);

	/* store float32x2_t */
	vst1_f32(output, f32x2);
}

static inline void
__int64_to_float32_neon_f32x1(float scale, int64_t *input, float *output)
{
	int64x2_t s64x2;
	float64x2_t f64x2;
	float32x2_t f32x2;

	/* load 2 x int64_t elements */
	s64x2 = vld1q_lane_s64(input, vdupq_n_s64(0), 0);

	/* convert int64x2_t to float64x2_t */
	f64x2 = vcvtq_f64_s64(s64x2);

	/* convert float64x2_t to float32x2_t */
	f32x2 = vcvt_f32_f64(f64x2);

	/* scale */
	f32x2 = vmul_n_f32(f32x2, scale);

	/* store float32x2_t */
	vst1_lane_f32(output, f32x2, 0);
}

int
rte_ml_io_int64_to_float32(float scale, uint64_t nb_elements, void *input, void *output)
{
	int64_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (int64_t *)input;
	output_buffer = (float *)output;
	vlen = 4 * sizeof(float) / sizeof(int64_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__int64_to_float32_neon_f32x2(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__int64_to_float32_neon_f32x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_uint64_neon_u64x2(float scale, float *input, uint64_t *output)
{
	float32x2_t f32x2;
	float64x2_t f64x2;
	uint64x2_t u64x2;

	/* load 2 x float elements */
	f32x2 = vld1_f32(input);

	/* scale */
	f32x2 = vmul_n_f32(f32x2, scale);

	/* convert to float64x2_t */
	f64x2 = vcvt_f64_f32(f32x2);

	/* convert to int64x2_t */
	u64x2 = vcvtaq_u64_f64(f64x2);

	/* store 2 elements */
	vst1q_u64(output, u64x2);
}

static inline void
__float32_to_uint64_neon_u64x1(float scale, float *input, uint64_t *output)
{
	float32x2_t f32x2;
	float64x2_t f64x2;
	uint64x2_t u64x2;

	/* load 1 x float element */
	f32x2 = vld1_lane_f32(input, vdup_n_f32(0), 0);

	/* scale */
	f32x2 = vmul_n_f32(f32x2, scale);

	/* convert to float64x2_t */
	f64x2 = vcvt_f64_f32(f32x2);

	/* convert to int64x2_t */
	u64x2 = vcvtaq_u64_f64(f64x2);

	/* store 2 elements */
	vst1q_lane_u64(output, u64x2, 0);
}

int
rte_ml_io_float32_to_uint64(float scale, uint64_t nb_elements, void *input, void *output)
{
	float *input_buffer;
	uint64_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float *)input;
	output_buffer = (uint64_t *)output;
	vlen = 4 * sizeof(float) / sizeof(uint64_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_uint64_neon_u64x2(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_uint64_neon_u64x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__uint64_to_float32_neon_f32x2(float scale, uint64_t *input, float *output)
{
	uint64x2_t u64x2;
	float64x2_t f64x2;
	float32x2_t f32x2;

	/* load 2 x int64_t elements */
	u64x2 = vld1q_u64(input);

	/* convert int64x2_t to float64x2_t */
	f64x2 = vcvtq_f64_u64(u64x2);

	/* convert float64x2_t to float32x2_t */
	f32x2 = vcvt_f32_f64(f64x2);

	/* scale */
	f32x2 = vmul_n_f32(f32x2, scale);

	/* store float32x2_t */
	vst1_f32(output, f32x2);
}

static inline void
__uint64_to_float32_neon_f32x1(float scale, uint64_t *input, float *output)
{
	uint64x2_t u64x2;
	float64x2_t f64x2;
	float32x2_t f32x2;

	/* load 2 x int64_t elements */
	u64x2 = vld1q_lane_u64(input, vdupq_n_u64(0), 0);

	/* convert int64x2_t to float64x2_t */
	f64x2 = vcvtq_f64_u64(u64x2);

	/* convert float64x2_t to float32x2_t */
	f32x2 = vcvt_f32_f64(f64x2);

	/* scale */
	f32x2 = vmul_n_f32(f32x2, scale);

	/* store float32x2_t */
	vst1_lane_f32(output, f32x2, 0);
}

int
rte_ml_io_uint64_to_float32(float scale, uint64_t nb_elements, void *input, void *output)
{
	uint64_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (uint64_t *)input;
	output_buffer = (float *)output;
	vlen = 4 * sizeof(float) / sizeof(uint64_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__uint64_to_float32_neon_f32x2(scale, input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__uint64_to_float32_neon_f32x1(scale, input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_float16_neon_f16x4(float32_t *input, float16_t *output)
{
	float32x4_t f32x4;
	float16x4_t f16x4;

	/* load 4 x float32_t elements */
	f32x4 = vld1q_f32(input);

	/* convert to float16x4_t */
	f16x4 = vcvt_f16_f32(f32x4);

	/* store float16x4_t */
	vst1_f16(output, f16x4);
}

static inline void
__float32_to_float16_neon_f16x1(float32_t *input, float16_t *output)
{
	float32x4_t f32x4;
	float16x4_t f16x4;

	/* load element to 4 lanes */
	f32x4 = vld1q_dup_f32(input);

	/* convert float32_t to float16_t */
	f16x4 = vcvt_f16_f32(f32x4);

	/* store lane 0 / 1 element */
	vst1_lane_f16(output, f16x4, 0);
}

int
rte_ml_io_float32_to_float16(uint64_t nb_elements, void *input, void *output)
{
	float32_t *input_buffer;
	float16_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float32_t *)input;
	output_buffer = (float16_t *)output;
	vlen = 2 * sizeof(float32_t) / sizeof(float16_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_float16_neon_f16x4(input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_float16_neon_f16x1(input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float16_to_float32_neon_f32x4(float16_t *input, float32_t *output)
{
	float16x4_t f16x4;
	float32x4_t f32x4;

	/* load 4 x float16_t elements */
	f16x4 = vld1_f16(input);

	/* convert float16x4_t to float32x4_t */
	f32x4 = vcvt_f32_f16(f16x4);

	/* store float32x4_t */
	vst1q_f32(output, f32x4);
}

static inline void
__float16_to_float32_neon_f32x1(float16_t *input, float32_t *output)
{
	float16x4_t f16x4;
	float32x4_t f32x4;

	/* load element to 4 lanes */
	f16x4 = vld1_dup_f16(input);

	/* convert float16_t to float32_t */
	f32x4 = vcvt_f32_f16(f16x4);

	/* store 1 element */
	vst1q_lane_f32(output, f32x4, 0);
}

int
rte_ml_io_float16_to_float32(uint64_t nb_elements, void *input, void *output)
{
	float16_t *input_buffer;
	float32_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float16_t *)input;
	output_buffer = (float32_t *)output;
	vlen = 2 * sizeof(float32_t) / sizeof(float16_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float16_to_float32_neon_f32x4(input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float16_to_float32_neon_f32x1(input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}
