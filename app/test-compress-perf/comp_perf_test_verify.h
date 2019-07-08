/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Intel Corporation
 */

#ifndef _COMP_PERF_TEST_VERIFY_
#define _COMP_PERF_TEST_VERIFY_

#include <stdint.h>

#include "comp_perf_options.h"
#include "comp_perf_test_common.h"

struct cperf_verify_ctx {
	struct cperf_mem_resources mem;
	struct comp_test_data *options;

	int silent;
	size_t comp_data_sz;
	size_t decomp_data_sz;
	double ratio;
};

void
cperf_verify_test_destructor(void *arg);

int
cperf_verify_test_runner(void *test_ctx);

void *
cperf_verify_test_constructor(uint8_t dev_id, uint16_t qp_id,
		struct comp_test_data *options);

#endif
