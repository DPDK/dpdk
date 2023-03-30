/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "mldev_utils.h"

#include <arm_neon.h>

/* Description:
 * This file implements vector versions of Machine Learning utility functions used to convert data
 * types from bfloat16 to float and vice-versa. Implementation is based on Arm Neon intrinsics.
 */

#ifdef __ARM_FEATURE_BF16

static inline void
__float32_to_bfloat16_neon_f16x4(float32_t *input, bfloat16_t *output)
{
	float32x4_t f32x4;
	bfloat16x4_t bf16x4;

	/* load 4 x float32_t elements */
	f32x4 = vld1q_f32(input);

	/* convert float32x4_t to bfloat16x4_t */
	bf16x4 = vcvt_bf16_f32(f32x4);

	/* store bfloat16x4_t */
	vst1_bf16(output, bf16x4);
}

static inline void
__float32_to_bfloat16_neon_f16x1(float32_t *input, bfloat16_t *output)
{
	float32x4_t f32x4;
	bfloat16x4_t bf16x4;

	/* load element to 4 lanes */
	f32x4 = vld1q_dup_f32(input);

	/* convert float32_t to bfloat16_t */
	bf16x4 = vcvt_bf16_f32(f32x4);

	/* store lane 0 / 1 element */
	vst1_lane_bf16(output, bf16x4, 0);
}

int
rte_ml_io_float32_to_bfloat16(uint64_t nb_elements, void *input, void *output)
{
	float32_t *input_buffer;
	bfloat16_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (float32_t *)input;
	output_buffer = (bfloat16_t *)output;
	vlen = 2 * sizeof(float32_t) / sizeof(bfloat16_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__float32_to_bfloat16_neon_f16x4(input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__float32_to_bfloat16_neon_f16x1(input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

static inline void
__bfloat16_to_float32_neon_f32x4(bfloat16_t *input, float32_t *output)
{
	bfloat16x4_t bf16x4;
	float32x4_t f32x4;

	/* load 4 x bfloat16_t elements */
	bf16x4 = vld1_bf16(input);

	/* convert bfloat16x4_t to float32x4_t */
	f32x4 = vcvt_f32_bf16(bf16x4);

	/* store float32x4_t */
	vst1q_f32(output, f32x4);
}

static inline void
__bfloat16_to_float32_neon_f32x1(bfloat16_t *input, float32_t *output)
{
	bfloat16x4_t bf16x4;
	float32x4_t f32x4;

	/* load element to 4 lanes */
	bf16x4 = vld1_dup_bf16(input);

	/* convert bfloat16_t to float32_t */
	f32x4 = vcvt_f32_bf16(bf16x4);

	/* store lane 0 / 1 element */
	vst1q_lane_f32(output, f32x4, 0);
}

int
rte_ml_io_bfloat16_to_float32(uint64_t nb_elements, void *input, void *output)
{
	bfloat16_t *input_buffer;
	float32_t *output_buffer;
	uint64_t nb_iterations;
	uint32_t vlen;
	uint64_t i;

	if ((nb_elements == 0) || (input == NULL) || (output == NULL))
		return -EINVAL;

	input_buffer = (bfloat16_t *)input;
	output_buffer = (float32_t *)output;
	vlen = 2 * sizeof(float32_t) / sizeof(bfloat16_t);
	nb_iterations = nb_elements / vlen;

	/* convert vlen elements in each iteration */
	for (i = 0; i < nb_iterations; i++) {
		__bfloat16_to_float32_neon_f32x4(input_buffer, output_buffer);
		input_buffer += vlen;
		output_buffer += vlen;
	}

	/* convert leftover elements */
	i = i * vlen;
	for (; i < nb_elements; i++) {
		__bfloat16_to_float32_neon_f32x1(input_buffer, output_buffer);
		input_buffer++;
		output_buffer++;
	}

	return 0;
}

#endif /* __ARM_FEATURE_BF16 */
