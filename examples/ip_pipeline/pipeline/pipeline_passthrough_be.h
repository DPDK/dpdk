/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef __INCLUDE_PIPELINE_PASSTHROUGH_BE_H__
#define __INCLUDE_PIPELINE_PASSTHROUGH_BE_H__

#include "pipeline_common_be.h"

#define PIPELINE_PASSTHROUGH_DMA_SIZE_MAX                             64

#ifndef PIPELINE_PASSTHROUGH_SWAP_N_FIELDS_MAX
#define PIPELINE_PASSTHROUGH_SWAP_N_FIELDS_MAX                        8
#endif

#ifndef PIPELINE_PASSTHROUGH_SWAP_FIELD_SIZE_MAX
#define PIPELINE_PASSTHROUGH_SWAP_FIELD_SIZE_MAX                      16
#endif

struct pipeline_passthrough_params {
	uint32_t dma_enabled;
	uint32_t dma_dst_offset;
	uint32_t dma_src_offset;
	uint8_t dma_src_mask[PIPELINE_PASSTHROUGH_DMA_SIZE_MAX];
	uint32_t dma_size;

	uint32_t dma_hash_enabled;
	uint32_t dma_hash_offset;

	uint32_t dma_hash_lb_enabled;

	uint32_t swap_enabled;
	uint32_t swap_field0_offset[PIPELINE_PASSTHROUGH_SWAP_N_FIELDS_MAX];
	uint32_t swap_field1_offset[PIPELINE_PASSTHROUGH_SWAP_N_FIELDS_MAX];
	uint32_t swap_n_fields;
};

int
pipeline_passthrough_parse_args(struct pipeline_passthrough_params *p,
	struct pipeline_params *params);

extern struct pipeline_be_ops pipeline_passthrough_be_ops;

#endif
