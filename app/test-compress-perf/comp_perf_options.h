/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _COMP_PERF_OPS_
#define _COMP_PERF_OPS_

#include <rte_dev.h>

#define MAX_LIST		32
#define MIN_COMPRESSED_BUF_SIZE 8
#define EXPANSE_RATIO 1.1
#define MAX_MBUF_DATA_SIZE (UINT16_MAX - RTE_PKTMBUF_HEADROOM)
#define MAX_SEG_SIZE ((int)(MAX_MBUF_DATA_SIZE / EXPANSE_RATIO))

extern const char *comp_perf_test_type_strs[];

/* Cleanup state machine */
enum cleanup_st {
	ST_CLEAR = 0,
	ST_TEST_DATA,
	ST_COMPDEV,
	ST_INPUT_DATA,
	ST_MEMORY_ALLOC,
	ST_DURING_TEST
};

enum cperf_test_type {
	CPERF_TEST_TYPE_THROUGHPUT,
	CPERF_TEST_TYPE_VERIFY,
	CPERF_TEST_TYPE_PMDCC
};

enum comp_operation {
	COMPRESS = (1 << 0),
	DECOMPRESS = (1 << 1),
	COMPRESS_DECOMPRESS = (COMPRESS | DECOMPRESS),
};

struct range_list {
	uint8_t min;
	uint8_t max;
	uint8_t inc;
	uint8_t count;
	uint8_t list[MAX_LIST];
};

struct comp_test_data {
	char driver_name[RTE_DEV_NAME_MAX_LEN];
	char input_file[PATH_MAX];
	enum cperf_test_type test;

	uint8_t *input_data;
	size_t input_data_sz;
	uint16_t nb_qps;
	uint16_t seg_sz;
	uint16_t out_seg_sz;
	uint16_t burst_sz;
	uint32_t pool_sz;
	uint32_t num_iter;
	uint16_t max_sgl_segs;
	uint32_t total_segs;

	uint8_t lz4_flags;
	enum rte_comp_huffman huffman_enc;
	enum comp_operation test_op;
	enum rte_comp_algorithm test_algo;

	int window_sz;
	struct range_list level_lst;
	uint8_t level;
	int use_external_mbufs;

	double ratio;
	enum cleanup_st cleanup;
	int perf_comp_force_stop;

	uint32_t cyclecount_delay;
};

int
comp_perf_options_parse(struct comp_test_data *test_data, int argc,
			char **argv);

void
comp_perf_options_default(struct comp_test_data *test_data);

int
comp_perf_options_check(struct comp_test_data *test_data);

#endif
