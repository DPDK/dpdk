/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
