/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "mldev_utils.h"

#include <arm_neon.h>

#include <eal_export.h>

/* Description:
 * This file implements vector versions of Machine Learning utility functions used to convert data
 * types from higher precision to lower precision and vice-versa, except bfloat16. Implementation
 * is based on Arm Neon intrinsics.
 */

static inline void
__float32_to_int8_neon_s8x8(const float *input, int8_t *output, float scale, int8_t zero_point)
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
	f32x4 = vdivq_f32(f32x4, vdupq_n_f32(scale));
	f32x4 = vaddq_f32(f32x4, vdupq_n_f32((float)zero_point));
	s32x4 = vcvtaq_s32_f32(f32x4);
	s16x4_l = vqmovn_s32(s32x4);

	/* load next 4 float32 elements, scale, convert, saturate narrow to int16.
	 * Use round to nearest with ties away rounding mode.
	 */
	f32x4 = vld1q_f32(input + 4);
	f32x4 = vdivq_f32(f32x4, vdupq_n_f32(scale));
	f32x4 = vaddq_f32(f32x4, vdupq_n_f32((float)zero_point));
	s32x4 = vcvtaq_s32_f32(f32x4);
	s16x4_h = vqmovn_s32(s32x4);

	/* combine lower and higher int16x4_t to int16x8_t */
	s16x8 = vcombine_s16(s16x4_l, s16x4_h);

	/* narrow to int8_t */
	s8x8 = vqmovn_s16(s16x8);
	s8x8 = vmax_s8(s8x8, vdup_n_s8(INT8_MIN + 1));

	/* store 8 elements */
	vst1_s8(output, s8x8);
}

static inline void
__float32_to_int8_neon_s8x1(const float *input, int8_t *output, float scale, int8_t zero_point)
{
	float32x2_t f32x2;
	int32x2_t s32x2;
	int16_t s16;

	/* scale and convert, round to nearest with ties away rounding mode */
	f32x2 = vdiv_f32(vdup_n_f32(*input), vdup_n_f32(scale));
	f32x2 = vadd_f32(f32x2, vdup_n_f32((float)zero_point));
	s32x2 = vcvta_s32_f32(f32x2);
	s32x2 = vmax_s32(s32x2, vdup_n_s32(INT8_MIN + 1));

	/* saturate narrow */
	s16 = vqmovns_s32(vget_lane_s32(s32x2, 0));

	/* convert to int8_t */
	*output = vqmovnh_s16(s16);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_float32_to_int8, 22.11)
int
rte_ml_io_float32_to_int8(const void *input, void *output, uint64_t nb_elements, float scale,
			  int8_t zero_point)
{
	const float *input_buffer;
	int8_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const float *)input;
	output_buffer = (int8_t *)output;
	vlen = 2 * sizeof(float) / sizeof(int8_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_int8_neon_s8x8(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_int8_neon_s8x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__int8_to_float32_neon_f32x8(const int8_t *input, float *output, float scale, int8_t zero_point)
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
	f32x4 = vsubq_f32(f32x4, vdupq_n_f32((float)zero_point));
	f32x4 = vmulq_n_f32(f32x4, scale);
	vst1q_f32(output, f32x4);

	/* convert higher 4 elements: widen to int32_t, convert to float, scale and store */
	s16x4 = vget_high_s16(s16x8);
	s32x4 = vmovl_s16(s16x4);
	f32x4 = vcvtq_f32_s32(s32x4);
	f32x4 = vsubq_f32(f32x4, vdupq_n_f32((float)zero_point));
	f32x4 = vmulq_n_f32(f32x4, scale);
	vst1q_f32(output + 4, f32x4);
}

static inline void
__int8_to_float32_neon_f32x1(const int8_t *input, float *output, float scale, int8_t zero_point)
{
	*output = scale * (vcvts_f32_s32((int32_t)*input) - (float)zero_point);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_int8_to_float32, 22.11)
int
rte_ml_io_int8_to_float32(const void *input, void *output, uint64_t nb_elements, float scale,
			  int8_t zero_point)
{
	const int8_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const int8_t *)input;
	output_buffer = (float *)output;
	vlen = 2 * sizeof(float) / sizeof(int8_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__int8_to_float32_neon_f32x8(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__int8_to_float32_neon_f32x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_uint8_neon_u8x8(const float *input, uint8_t *output, float scale, uint8_t zero_point)
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
	f32x4 = vdivq_f32(f32x4, vdupq_n_f32(scale));
	f32x4 = vaddq_f32(f32x4, vdupq_n_f32((float)zero_point));
	u32x4 = vcvtaq_u32_f32(f32x4);
	u16x4_l = vqmovn_u32(u32x4);

	/* load next 4 float elements, scale, convert, saturate narrow to uint16_t
	 * use round to nearest with ties away rounding mode.
	 */
	f32x4 = vld1q_f32(input + 4);
	f32x4 = vdivq_f32(f32x4, vdupq_n_f32(scale));
	f32x4 = vaddq_f32(f32x4, vdupq_n_f32((float)zero_point));
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
__float32_to_uint8_neon_u8x1(const float *input, uint8_t *output, float scale, uint8_t zero_point)
{
	float32x2_t f32x2;
	uint32x2_t u32x2;
	uint16_t u16;

	/* scale and convert, round to nearest with ties away rounding mode */
	f32x2 = vdiv_f32(vdup_n_f32(*input), vdup_n_f32(scale));
	f32x2 = vadd_f32(f32x2, vdup_n_f32((float)zero_point));
	u32x2 = vcvta_u32_f32(f32x2);

	/* saturate narrow */
	u16 = vqmovns_u32(vget_lane_u32(u32x2, 0));

	/* convert to uint8_t */
	*output = vqmovnh_u16(u16);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_float32_to_uint8, 22.11)
int
rte_ml_io_float32_to_uint8(const void *input, void *output, uint64_t nb_elements, float scale,
			   uint8_t zero_point)
{
	const float *input_buffer;
	uint8_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const float *)input;
	output_buffer = (uint8_t *)output;
	vlen = 2 * sizeof(float) / sizeof(uint8_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_uint8_neon_u8x8(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_uint8_neon_u8x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__uint8_to_float32_neon_f32x8(const uint8_t *input, float *output, float scale, uint8_t zero_point)
{
	float32x4_t f32x4;
	uint16x8_t u16x8;
	int16x8_t s16x8;
	int16x4_t s16x4;
	int32x4_t s32x4;
	uint8x8_t u8x8;

	/* load 8 x uint8_t elements */
	u8x8 = vld1_u8(input);
	u16x8 = vmovl_u8(u8x8);
	s16x8 = vreinterpretq_s16_u16(u16x8);

	/* convert lower 4 elements: widen to uint32_t, convert to float, scale and store */
	s16x4 = vget_low_s16(s16x8);
	s32x4 = vmovl_s16(s16x4);
	f32x4 = vcvtq_f32_s32(s32x4);
	f32x4 = vsubq_f32(f32x4, vdupq_n_f32((float)zero_point));
	f32x4 = vmulq_n_f32(f32x4, scale);
	vst1q_f32(output, f32x4);

	/* convert higher 4 elements: widen to uint32_t, convert to float, scale and store */
	s16x4 = vget_high_s16(s16x8);
	s32x4 = vmovl_s16(s16x4);
	f32x4 = vcvtq_f32_s32(s32x4);
	f32x4 = vsubq_f32(f32x4, vdupq_n_f32((float)zero_point));
	f32x4 = vmulq_n_f32(f32x4, scale);
	vst1q_f32(output + 4, f32x4);
}

static inline void
__uint8_to_float32_neon_f32x1(const uint8_t *input, float *output, float scale, uint8_t zero_point)
{
	*output = scale * (vcvts_f32_u32((uint32_t)*input) - (float)zero_point);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_uint8_to_float32, 22.11)
int
rte_ml_io_uint8_to_float32(const void *input, void *output, uint64_t nb_elements, float scale,
			   uint8_t zero_point)
{
	const uint8_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint64_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const uint8_t *)input;
	output_buffer = (float *)output;
	vlen = 2 * sizeof(float) / sizeof(uint8_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__uint8_to_float32_neon_f32x8(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__uint8_to_float32_neon_f32x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_int16_neon_s16x4(const float *input, int16_t *output, float scale, int16_t zero_point)
{
	float32x4_t f32x4;
	int16x4_t s16x4;
	int32x4_t s32x4;

	/* load 4 x float elements */
	f32x4 = vld1q_f32(input);

	/* scale */
	f32x4 = vdivq_f32(f32x4, vdupq_n_f32(scale));

	/* add zero point */
	f32x4 = vaddq_f32(f32x4, vdupq_n_f32((float)zero_point));

	/* convert to int32x4_t using round to nearest with ties away rounding mode */
	s32x4 = vcvtaq_s32_f32(f32x4);

	/* saturate narrow to int16x4_t */
	s16x4 = vqmovn_s32(s32x4);
	s16x4 = vmax_s16(s16x4, vdup_n_s16(INT16_MIN + 1));

	/* store 4 elements */
	vst1_s16(output, s16x4);
}

static inline void
__float32_to_int16_neon_s16x1(const float *input, int16_t *output, float scale, int16_t zero_point)
{
	float32x2_t f32x2;
	int32x2_t s32x2;

	/* scale and convert, round to nearest with ties away rounding mode */
	f32x2 = vdiv_f32(vdup_n_f32(*input), vdup_n_f32(scale));
	f32x2 = vadd_f32(f32x2, vdup_n_f32((float)zero_point));
	s32x2 = vcvta_s32_f32(f32x2);
	s32x2 = vmax_s32(s32x2, vdup_n_s32(INT16_MIN + 1));

	/* saturate narrow */
	*output = vqmovns_s32(vget_lane_s32(s32x2, 0));
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_float32_to_int16, 22.11)
int
rte_ml_io_float32_to_int16(const void *input, void *output, uint64_t nb_elements, float scale,
			   int16_t zero_point)
{
	const float *input_buffer;
	int16_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const float *)input;
	output_buffer = (int16_t *)output;
	vlen = 2 * sizeof(float) / sizeof(int16_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_int16_neon_s16x4(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_int16_neon_s16x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__int16_to_float32_neon_f32x4(const int16_t *input, float *output, float scale, int16_t zero_point)
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

	/* subtract zero point */
	f32x4 = vsubq_f32(f32x4, vdupq_n_f32((float)zero_point));

	/* scale */
	f32x4 = vmulq_n_f32(f32x4, scale);

	/* store float32x4_t */
	vst1q_f32(output, f32x4);
}

static inline void
__int16_to_float32_neon_f32x1(const int16_t *input, float *output, float scale, int16_t zero_point)
{
	*output = scale * (vcvts_f32_s32((int32_t)*input) - (float)zero_point);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_int16_to_float32, 22.11)
int
rte_ml_io_int16_to_float32(const void *input, void *output, uint64_t nb_elements, float scale,
			   int16_t zero_point)
{
	const int16_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const int16_t *)input;
	output_buffer = (float *)output;
	vlen = 2 * sizeof(float) / sizeof(int16_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__int16_to_float32_neon_f32x4(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__int16_to_float32_neon_f32x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_uint16_neon_u16x4(const float *input, uint16_t *output, float scale,
			       uint16_t zero_point)
{
	float32x4_t f32x4;
	uint16x4_t u16x4;
	uint32x4_t u32x4;

	/* load 4 float elements */
	f32x4 = vld1q_f32(input);

	/* scale */
	f32x4 = vdivq_f32(f32x4, vdupq_n_f32(scale));

	/* add zero point */
	f32x4 = vaddq_f32(f32x4, vdupq_n_f32((float)zero_point));

	/* convert using round to nearest with ties to away rounding mode */
	u32x4 = vcvtaq_u32_f32(f32x4);

	/* saturate narrow */
	u16x4 = vqmovn_u32(u32x4);

	/* store 4 elements */
	vst1_u16(output, u16x4);
}

static inline void
__float32_to_uint16_neon_u16x1(const float *input, uint16_t *output, float scale,
			       uint16_t zero_point)
{
	uint32_t u32;

	/* scale and convert, round to nearest with ties away rounding mode */
	u32 = vcvtas_u32_f32((*input) / scale + (float)zero_point);

	/* saturate narrow */
	*output = vqmovns_u32(u32) + zero_point;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_float32_to_uint16, 22.11)
int
rte_ml_io_float32_to_uint16(const void *input, void *output, uint64_t nb_elements, float scale,
			   uint16_t zero_point)
{
	const float *input_buffer;
	uint16_t *output_buffer;
	uint64_t nb_iterations;
	uint64_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const float *)input;
	output_buffer = (uint16_t *)output;
	vlen = 2 * sizeof(float) / sizeof(uint16_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_uint16_neon_u16x4(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_uint16_neon_u16x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__uint16_to_float32_neon_f32x4(const uint16_t *input, float *output, float scale,
			       uint16_t zero_point)
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

	/* subtract zero point */
	f32x4 = vsubq_f32(f32x4, vdupq_n_f32((float)zero_point));

	/* scale */
	f32x4 = vmulq_n_f32(f32x4, scale);

	/* store float32x4_t */
	vst1q_f32(output, f32x4);
}

static inline void
__uint16_to_float32_neon_f32x1(const uint16_t *input, float *output, float scale,
			       uint16_t zero_point)
{
	*output = scale * (vcvts_f32_u32((uint32_t)*input) - (float)zero_point);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_uint16_to_float32, 22.11)
int
rte_ml_io_uint16_to_float32(const void *input, void *output, uint64_t nb_elements, float scale,
			   uint16_t zero_point)
{
	const uint16_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const uint16_t *)input;
	output_buffer = (float *)output;
	vlen = 2 * sizeof(float) / sizeof(uint16_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__uint16_to_float32_neon_f32x4(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__uint16_to_float32_neon_f32x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_int32_neon_s32x4(const float *input, int32_t *output, float scale, int32_t zero_point)
{
	float32x4_t f32x4;
	int32x4_t s32x4;

	/* load 4 x float elements */
	f32x4 = vld1q_f32(input);

	/* scale */
	f32x4 = vdivq_f32(f32x4, vdupq_n_f32(scale));

	/* add zero point */
	f32x4 = vaddq_f32(f32x4, vdupq_n_f32((float)zero_point));

	/* convert to int32x4_t using round to nearest with ties away rounding mode */
	s32x4 = vcvtaq_s32_f32(f32x4);

	/* add zero_point */
	s32x4 = vaddq_s32(s32x4, vdupq_n_s32(zero_point));
	s32x4 = vmaxq_s32(s32x4, vdupq_n_s32(INT32_MIN + 1));

	/* store 4 elements */
	vst1q_s32(output, s32x4);
}

static inline void
__float32_to_int32_neon_s32x1(const float *input, int32_t *output, float scale, int32_t zero_point)
{
	float32x2_t f32x2;
	int32x2_t s32x2;

	/* scale and convert, round to nearest with ties away rounding mode */
	f32x2 = vdiv_f32(vdup_n_f32(*input), vdup_n_f32(scale));
	f32x2 = vadd_f32(f32x2, vdup_n_f32((float)zero_point));
	s32x2 = vcvta_s32_f32(f32x2);
	s32x2 = vmax_s32(s32x2, vdup_n_s32(INT16_MIN + 1));

	/* saturate narrow */
	vst1_lane_s32(output, s32x2, 0);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_float32_to_int32, 22.11)
int
rte_ml_io_float32_to_int32(const void *input, void *output, uint64_t nb_elements, float scale,
			   int32_t zero_point)
{
	const float *input_buffer;
	int32_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const float *)input;
	output_buffer = (int32_t *)output;
	vlen = 2 * sizeof(float) / sizeof(int32_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_int32_neon_s32x4(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_int32_neon_s32x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__int32_to_float32_neon_f32x4(const int32_t *input, float *output, float scale, int32_t zero_point)
{
	float32x4_t f32x4;
	int32x4_t s32x4;

	/* load 4 x int32_t elements */
	s32x4 = vld1q_s32(input);

	/* convert int32_t to float */
	f32x4 = vcvtq_f32_s32(s32x4);

	/* subtract zero point */
	f32x4 = vsubq_f32(f32x4, vdupq_n_f32((float)zero_point));

	/* scale */
	f32x4 = vmulq_n_f32(f32x4, scale);

	/* store float32x4_t */
	vst1q_f32(output, f32x4);
}

static inline void
__int32_to_float32_neon_f32x1(const int32_t *input, float *output, float scale, int32_t zero_point)
{
	*output = scale * (vcvts_f32_s32(*input) - (float)zero_point);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_int32_to_float32, 22.11)
int
rte_ml_io_int32_to_float32(const void *input, void *output, uint64_t nb_elements, float scale,
			   int32_t zero_point)
{
	const int32_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const int32_t *)input;
	output_buffer = (float *)output;
	vlen = 2 * sizeof(float) / sizeof(int32_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__int32_to_float32_neon_f32x4(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__int32_to_float32_neon_f32x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_uint32_neon_u32x4(const float *input, uint32_t *output, float scale,
			       uint32_t zero_point)
{
	float32x4_t f32x4;
	uint32x4_t u32x4;

	/* load 4 float elements */
	f32x4 = vld1q_f32(input);

	/* scale */
	f32x4 = vdivq_f32(f32x4, vdupq_n_f32(scale));

	/* add zero point */
	f32x4 = vaddq_f32(f32x4, vdupq_n_f32((float)zero_point));

	/* convert using round to nearest with ties to away rounding mode */
	u32x4 = vcvtaq_u32_f32(f32x4);

	/* store 4 elements */
	vst1q_u32(output, u32x4);
}

static inline void
__float32_to_uint32_neon_u32x1(const float *input, uint32_t *output, float scale,
			       uint32_t zero_point)
{
	/* scale and convert, round to nearest with ties away rounding mode */
	*output = vcvtas_u32_f32((*input) / scale + (float)zero_point);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_float32_to_uint32, 22.11)
int
rte_ml_io_float32_to_uint32(const void *input, void *output, uint64_t nb_elements, float scale,
			   uint32_t zero_point)
{
	const float *input_buffer;
	uint32_t *output_buffer;
	uint64_t nb_iterations;
	uint64_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const float *)input;
	output_buffer = (uint32_t *)output;
	vlen = 2 * sizeof(float) / sizeof(uint32_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_uint32_neon_u32x4(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_uint32_neon_u32x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__uint32_to_float32_neon_f32x4(const uint32_t *input, float *output, float scale,
			       uint32_t zero_point)
{
	float32x4_t f32x4;
	uint32x4_t u32x4;

	/* load 4 x uint32_t elements */
	u32x4 = vld1q_u32(input);

	/* convert uint32_t to float */
	f32x4 = vcvtq_f32_u32(u32x4);

	/* subtract zero point */
	f32x4 = vsubq_f32(f32x4, vdupq_n_f32((float)zero_point));

	/* scale */
	f32x4 = vmulq_n_f32(f32x4, scale);

	/* store float32x4_t */
	vst1q_f32(output, f32x4);
}

static inline void
__uint32_to_float32_neon_f32x1(const uint32_t *input, float *output, float scale,
			       uint32_t zero_point)
{
	*output = scale * (vcvts_f32_u32(*input) - (float)zero_point);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_uint32_to_float32, 22.11)
int
rte_ml_io_uint32_to_float32(const void *input, void *output, uint64_t nb_elements, float scale,
			   uint32_t zero_point)
{
	const uint32_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const uint32_t *)input;
	output_buffer = (float *)output;
	vlen = 2 * sizeof(float) / sizeof(uint32_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__uint32_to_float32_neon_f32x4(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__uint32_to_float32_neon_f32x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_int64_neon_s64x2(const float *input, int64_t *output, float scale, int64_t zero_point)
{
	float32x2_t f32x2;
	float64x2_t f64x2;
	int64x2_t s64x2;
	int64_t s64;

	/* load 2 x float elements */
	f32x2 = vld1_f32(input);

	/* scale */
	f32x2 = vdiv_f32(f32x2, vdup_n_f32(scale));

	/* add zero point */
	f32x2 = vadd_f32(f32x2, vdup_n_f32((float)zero_point));

	/* convert to float64x2_t */
	f64x2 = vcvt_f64_f32(f32x2);

	/* convert to int64x2_t */
	s64x2 = vcvtaq_s64_f64(f64x2);
	s64 = vgetq_lane_s64(s64x2, 0);
	s64 = (s64 == INT64_MIN) ? INT64_MIN + 1 : s64;

	/* store lane 0 of int64x2_t */
	*output = s64;
}

static inline void
__float32_to_int64_neon_s64x1(const float *input, int64_t *output, float scale, int64_t zero_point)
{
	float32x2_t f32x2;
	float64x2_t f64x2;
	int64x2_t s64x2;
	int64_t s64;

	/* load 1 x float element */
	f32x2 = vdup_n_f32(*input);

	/* scale */
	f32x2 = vdiv_f32(f32x2, vdup_n_f32(scale));

	/* add zero point */
	f32x2 = vadd_f32(f32x2, vdup_n_f32((float)zero_point));

	/* convert to float64x2_t */
	f64x2 = vcvt_f64_f32(f32x2);

	/* convert to int64x2_t */
	s64x2 = vcvtaq_s64_f64(f64x2);
	s64 = vgetq_lane_s64(s64x2, 0);
	s64 = (s64 == INT64_MIN) ? INT64_MIN + 1 : s64;

	/* store lane 0 of int64x2_t */
	*output = s64;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_float32_to_int64, 22.11)
int
rte_ml_io_float32_to_int64(const void *input, void *output, uint64_t nb_elements, float scale,
			   int64_t zero_point)
{
	const float *input_buffer;
	int64_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const float *)input;
	output_buffer = (int64_t *)output;
	vlen = 4 * sizeof(float) / sizeof(int64_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_int64_neon_s64x2(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_int64_neon_s64x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__int64_to_float32_neon_f32x2(const int64_t *input, float *output, float scale, int64_t zero_point)
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

	/* subtract zero_point */
	f32x2 = vsub_f32(f32x2, vdup_n_f32(zero_point));

	/* scale */
	f32x2 = vmul_n_f32(f32x2, scale);

	/* store float32x2_t */
	vst1_f32(output, f32x2);
}

static inline void
__int64_to_float32_neon_f32x1(const int64_t *input, float *output, float scale, int64_t zero_point)
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

	/* subtract zero_point */
	f32x2 = vsub_f32(f32x2, vdup_n_f32(zero_point));

	/* scale */
	f32x2 = vmul_n_f32(f32x2, scale);

	/* store float32x2_t lane 0 */
	vst1_lane_f32(output, f32x2, 0);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_int64_to_float32, 22.11)
int
rte_ml_io_int64_to_float32(const void *input, void *output, uint64_t nb_elements, float scale,
			   int64_t zero_point)
{
	const int64_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const int64_t *)input;
	output_buffer = (float *)output;
	vlen = 4 * sizeof(float) / sizeof(int64_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__int64_to_float32_neon_f32x2(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__int64_to_float32_neon_f32x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_uint64_neon_u64x2(const float *input, uint64_t *output, float scale,
			       uint64_t zero_point)
{
	float32x2_t f32x2;
	float64x2_t f64x2;
	uint64x2_t u64x2;

	/* load 2 x float elements */
	f32x2 = vld1_f32(input);

	/* scale */
	f32x2 = vdiv_f32(f32x2, vdup_n_f32(scale));

	/* add zero point */
	f32x2 = vadd_f32(f32x2, vdup_n_f32((float)zero_point));

	/* convert to float64x2_t */
	f64x2 = vcvt_f64_f32(f32x2);

	/* convert to int64x2_t */
	u64x2 = vcvtaq_u64_f64(f64x2);

	/* store 2 elements */
	vst1q_u64(output, u64x2);
}

static inline void
__float32_to_uint64_neon_u64x1(const float *input, uint64_t *output, float scale,
			       uint64_t zero_point)
{
	float32x2_t f32x2;
	float64x2_t f64x2;
	uint64x2_t u64x2;

	/* load 1 x float element */
	f32x2 = vld1_lane_f32(input, vdup_n_f32(0), 0);

	/* scale */
	f32x2 = vdiv_f32(f32x2, vdup_n_f32(scale));

	/* add zero_point */
	f32x2 = vadd_f32(f32x2, vdup_n_f32((float)zero_point));

	/* convert to float64x2_t */
	f64x2 = vcvt_f64_f32(f32x2);

	/* convert to int64x2_t */
	u64x2 = vcvtaq_u64_f64(f64x2);

	/* store 2 elements */
	vst1q_lane_u64(output, u64x2, 0);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_float32_to_uint64, 22.11)
int
rte_ml_io_float32_to_uint64(const void *input, void *output, uint64_t nb_elements, float scale,
			   uint64_t zero_point)
{
	const float *input_buffer;
	uint64_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const float *)input;
	output_buffer = (uint64_t *)output;
	vlen = 4 * sizeof(float) / sizeof(uint64_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_uint64_neon_u64x2(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_uint64_neon_u64x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__uint64_to_float32_neon_f32x2(const uint64_t *input, float *output, float scale,
			       uint64_t zero_point)
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

	/* subtract zero_point */
	f32x2 = vsub_f32(f32x2, vdup_n_f32((float)zero_point));

	/* scale */
	f32x2 = vmul_n_f32(f32x2, scale);

	/* store float32x2_t */
	vst1_f32(output, f32x2);
}

static inline void
__uint64_to_float32_neon_f32x1(const uint64_t *input, float *output, float scale,
			       uint64_t zero_point)
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

	/* subtract zero_point */
	f32x2 = vsub_f32(f32x2, vdup_n_f32((float)zero_point));

	/* scale */
	f32x2 = vmul_n_f32(f32x2, scale);

	/* store float32x2_t lane 0 */
	vst1_lane_f32(output, f32x2, 0);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_uint64_to_float32, 22.11)
int
rte_ml_io_uint64_to_float32(const void *input, void *output, uint64_t nb_elements, float scale,
			   uint64_t zero_point)
{
	const uint64_t *input_buffer;
	float *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((scale == 0) || (nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const uint64_t *)input;
	output_buffer = (float *)output;
	vlen = 4 * sizeof(float) / sizeof(uint64_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__uint64_to_float32_neon_f32x2(input_buffer, output_buffer, scale, zero_point);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__uint64_to_float32_neon_f32x1(input_buffer, output_buffer, scale, zero_point);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__float32_to_float16_neon_f16x4(const float32_t *input, float16_t *output)
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
__float32_to_float16_neon_f16x1(const float32_t *input, float16_t *output)
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

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_float32_to_float16, 22.11)
int
rte_ml_io_float32_to_float16(const void *input, void *output, uint64_t nb_elements)
{
	const float32_t *input_buffer;
	float16_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const float32_t *)input;
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
__float16_to_float32_neon_f32x4(const float16_t *input, float32_t *output)
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
__float16_to_float32_neon_f32x1(const float16_t *input, float32_t *output)
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

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_ml_io_float16_to_float32, 22.11)
int
rte_ml_io_float16_to_float32(const void *input, void *output, uint64_t nb_elements)
{
	const float16_t *input_buffer;
	float32_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (const float16_t *)input;
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
