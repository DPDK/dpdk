/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _COMP_PERF_TEST_BENCHMARK_
#define _COMP_PERF_TEST_BENCHMARK_

#include <stdint.h>

#include "comp_perf_options.h"
#include "comp_perf_test_common.h"
#include "comp_perf_test_verify.h"

struct cperf_benchmark_ctx {
	struct cperf_verify_ctx ver;

	/* Store TSC duration for all levels (including level 0) */
	uint64_t comp_tsc_duration[RTE_COMP_LEVEL_MAX + 1];
	uint64_t decomp_tsc_duration[RTE_COMP_LEVEL_MAX + 1];
	double comp_gbps;
	double decomp_gbps;
	double comp_tsc_byte;
	double decomp_tsc_byte;
};

void
cperf_throughput_test_destructor(void *arg);

int
cperf_throughput_test_runner(void *test_ctx);

void *
cperf_throughput_test_constructor(uint8_t dev_id, uint16_t qp_id,
		struct comp_test_data *options);

#endif
