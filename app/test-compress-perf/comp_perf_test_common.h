/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _COMP_PERF_TEST_COMMON_H_
#define _COMP_PERF_TEST_COMMON_H_

#include <stdint.h>

#include <rte_mempool.h>

struct cperf_mem_resources {
	uint8_t dev_id;
	uint16_t qp_id;
	uint8_t lcore_id;

	uint16_t print_info_once;

	uint32_t total_bufs;
	uint8_t *compressed_data;
	uint8_t *decompressed_data;

	struct rte_mbuf **comp_bufs;
	struct rte_mbuf **decomp_bufs;

	struct rte_mempool *comp_buf_pool;
	struct rte_mempool *decomp_buf_pool;
	struct rte_mempool *op_pool;

	/* external mbuf support */
	const struct rte_memzone **comp_memzones;
	const struct rte_memzone **decomp_memzones;
	struct rte_mbuf_ext_shared_info *comp_buf_infos;
	struct rte_mbuf_ext_shared_info *decomp_buf_infos;
};

int
param_range_check(uint16_t size, const struct rte_param_log2_range *range);

void
comp_perf_free_memory(struct comp_test_data *test_data,
		      struct cperf_mem_resources *mem);

int
comp_perf_allocate_memory(struct comp_test_data *test_data,
			  struct cperf_mem_resources *mem);

int
prepare_bufs(struct comp_test_data *test_data, struct cperf_mem_resources *mem);

void
print_test_dynamics(const struct comp_test_data *test_data);

#endif /* _COMP_PERF_TEST_COMMON_H_ */
