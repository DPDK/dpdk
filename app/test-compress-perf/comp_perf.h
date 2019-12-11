/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _COMP_PERF_
#define _COMP_PERF_

#include <rte_mempool.h>

struct comp_test_data;

typedef void  *(*cperf_constructor_t)(
		uint8_t dev_id,
		uint16_t qp_id,
		struct comp_test_data *options);

typedef int (*cperf_runner_t)(void *test_ctx);
typedef void (*cperf_destructor_t)(void *test_ctx);

struct cperf_test {
	cperf_constructor_t constructor;
	cperf_runner_t runner;
	cperf_destructor_t destructor;
};

/* Needed for weak functions*/

void *
cperf_throughput_test_constructor(uint8_t dev_id __rte_unused,
				 uint16_t qp_id __rte_unused,
				 struct comp_test_data *options __rte_unused);

void
cperf_throughput_test_destructor(void *arg __rte_unused);

int
cperf_throughput_test_runner(void *test_ctx __rte_unused);

void *
cperf_verify_test_constructor(uint8_t dev_id __rte_unused,
				 uint16_t qp_id __rte_unused,
				 struct comp_test_data *options __rte_unused);

void
cperf_verify_test_destructor(void *arg __rte_unused);

int
cperf_verify_test_runner(void *test_ctx __rte_unused);

#endif /* _COMP_PERF_ */
