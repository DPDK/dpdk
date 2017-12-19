/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef _CPERF_
#define _CPERF_

#include <rte_crypto.h>

#include "cperf_ops.h"

struct cperf_options;
struct cperf_test_vector;
struct cperf_op_fns;

typedef void  *(*cperf_constructor_t)(
		struct rte_mempool *sess_mp,
		uint8_t dev_id,
		uint16_t qp_id,
		const struct cperf_options *options,
		const struct cperf_test_vector *t_vec,
		const struct cperf_op_fns *op_fns);

typedef int (*cperf_runner_t)(void *test_ctx);
typedef void (*cperf_destructor_t)(void *test_ctx);

struct cperf_test {
	cperf_constructor_t constructor;
	cperf_runner_t runner;
	cperf_destructor_t destructor;
};

#endif /* _CPERF_ */
