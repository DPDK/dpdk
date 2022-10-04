/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef _CPERF_OPS_
#define _CPERF_OPS_

#include <rte_crypto.h>

#include "cperf.h"
#include "cperf_options.h"
#include "cperf_test_vectors.h"


typedef void *(*cperf_sessions_create_t)(
		struct rte_mempool *sess_mp,
		uint8_t dev_id, const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset);

typedef void (*cperf_populate_ops_t)(struct rte_crypto_op **ops,
		uint32_t src_buf_offset, uint32_t dst_buf_offset,
		uint16_t nb_ops, void *sess,
		const struct cperf_options *options,
		const struct cperf_test_vector *test_vector,
		uint16_t iv_offset, uint32_t *imix_idx,
		uint64_t *tsc_start);

struct cperf_op_fns {
	cperf_sessions_create_t sess_create;
	cperf_populate_ops_t populate_ops;
};

int
cperf_get_op_functions(const struct cperf_options *options,
		struct cperf_op_fns *op_fns);

#endif /* _CPERF_OPS_ */
