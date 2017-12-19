/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _CPERF_TEST_COMMON_H_
#define _CPERF_TEST_COMMON_H_

#include <stdint.h>

#include <rte_mempool.h>

#include "cperf_options.h"
#include "cperf_test_vectors.h"

int
cperf_alloc_common_memory(const struct cperf_options *options,
			const struct cperf_test_vector *test_vector,
			uint8_t dev_id, uint16_t qp_id,
			size_t extra_op_priv_size,
			uint32_t *src_buf_offset,
			uint32_t *dst_buf_offset,
			struct rte_mempool **pool);

#endif /* _CPERF_TEST_COMMON_H_ */
