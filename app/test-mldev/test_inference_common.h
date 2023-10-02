/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef TEST_INFERENCE_COMMON_H
#define TEST_INFERENCE_COMMON_H

#include <rte_common.h>
#include <rte_mempool.h>
#include <rte_mldev.h>

#include "test_model_common.h"

#define ML_TEST_MAX_IO_SIZE 32

struct ml_request {
	uint8_t *input;
	uint8_t *output;
	uint16_t fid;
	uint64_t niters;

	struct rte_ml_buff_seg *inp_buf_segs[ML_TEST_MAX_IO_SIZE];
	struct rte_ml_buff_seg *out_buf_segs[ML_TEST_MAX_IO_SIZE];
};

struct ml_core_args {
	uint64_t nb_reqs;
	uint16_t start_fid;
	uint16_t end_fid;
	uint32_t qp_id;

	struct rte_ml_op **enq_ops;
	struct rte_ml_op **deq_ops;
	struct ml_request **reqs;

	uint64_t start_cycles;
	uint64_t end_cycles;
};

struct test_inference {
	/* common data */
	struct test_common cmn;

	/* test specific data */
	struct ml_model model[ML_TEST_MAX_MODELS];
	struct rte_mempool *buf_seg_pool;
	struct rte_mempool *op_pool;

	uint64_t nb_used;
	uint64_t nb_valid;
	uint16_t fid;

	int (*enqueue)(void *arg);
	int (*dequeue)(void *arg);

	struct ml_core_args args[RTE_MAX_LCORE];
	uint64_t error_count[RTE_MAX_LCORE];

	struct rte_ml_dev_xstats_map *xstats_map;
	uint64_t *xstats_values;
	int xstats_size;
} __rte_cache_aligned;

bool test_inference_cap_check(struct ml_options *opt);
int test_inference_opt_check(struct ml_options *opt);
void test_inference_opt_dump(struct ml_options *opt);
int test_inference_setup(struct ml_test *test, struct ml_options *opt);
void test_inference_destroy(struct ml_test *test, struct ml_options *opt);

int ml_inference_mldev_setup(struct ml_test *test, struct ml_options *opt);
int ml_inference_mldev_destroy(struct ml_test *test, struct ml_options *opt);
int ml_inference_iomem_setup(struct ml_test *test, struct ml_options *opt, uint16_t fid);
void ml_inference_iomem_destroy(struct ml_test *test, struct ml_options *opt, uint16_t fid);
int ml_inference_mem_setup(struct ml_test *test, struct ml_options *opt);
void ml_inference_mem_destroy(struct ml_test *test, struct ml_options *opt);
int ml_inference_result(struct ml_test *test, struct ml_options *opt, uint16_t fid);
int ml_inference_launch_cores(struct ml_test *test, struct ml_options *opt, uint16_t start_fid,
			      uint16_t end_fid);

#endif /* TEST_INFERENCE_COMMON_H */
