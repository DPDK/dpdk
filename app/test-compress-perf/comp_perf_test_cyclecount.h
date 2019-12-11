/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _COMP_PERF_TEST_CYCLECOUNT_
#define _COMP_PERF_TEST_CYCLECOUNT_

#include <stdint.h>

#include "comp_perf_options.h"
#include "comp_perf_test_common.h"
#include "comp_perf_test_verify.h"

void
cperf_cyclecount_test_destructor(void *arg);

int
cperf_cyclecount_test_runner(void *test_ctx);

void *
cperf_cyclecount_test_constructor(uint8_t dev_id, uint16_t qp_id,
		struct comp_test_data *options);

#endif
