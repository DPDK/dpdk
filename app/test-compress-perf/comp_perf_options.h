/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _COMP_PERF_OPS_
#define _COMP_PERF_OPS_

#define MAX_DRIVER_NAME		64
#define MAX_INPUT_FILE_NAME	64
#define MAX_LIST		32
#define MIN_COMPRESSED_BUF_SIZE 8
#define EXPANSE_RATIO 1.05
#define MAX_MBUF_DATA_SIZE (UINT16_MAX - RTE_PKTMBUF_HEADROOM)
#define MAX_SEG_SIZE ((int)(MAX_MBUF_DATA_SIZE / EXPANSE_RATIO))

enum comp_operation {
	COMPRESS_ONLY,
	DECOMPRESS_ONLY,
	COMPRESS_DECOMPRESS
};

struct range_list {
	uint8_t min;
	uint8_t max;
	uint8_t inc;
	uint8_t count;
	uint8_t list[MAX_LIST];
};

struct comp_test_data {
	char driver_name[64];
	char input_file[64];
	struct rte_mbuf **comp_bufs;
	struct rte_mbuf **decomp_bufs;
	uint32_t total_bufs;
	uint8_t *input_data;
	size_t input_data_sz;
	uint8_t *compressed_data;
	uint8_t *decompressed_data;
	struct rte_mempool *comp_buf_pool;
	struct rte_mempool *decomp_buf_pool;
	struct rte_mempool *op_pool;
	int8_t cdev_id;
	uint16_t seg_sz;
	uint16_t out_seg_sz;
	uint16_t burst_sz;
	uint32_t pool_sz;
	uint32_t num_iter;
	uint16_t max_sgl_segs;
	enum rte_comp_huffman huffman_enc;
	enum comp_operation test_op;
	int window_sz;
	struct range_list level;
	/* Store TSC duration for all levels (including level 0) */
	uint64_t comp_tsc_duration[RTE_COMP_LEVEL_MAX + 1];
	uint64_t decomp_tsc_duration[RTE_COMP_LEVEL_MAX + 1];
	size_t comp_data_sz;
	size_t decomp_data_sz;
	double ratio;
	double comp_gbps;
	double decomp_gbps;
	double comp_tsc_byte;
	double decomp_tsc_byte;
};

int
comp_perf_options_parse(struct comp_test_data *test_data, int argc,
			char **argv);

void
comp_perf_options_default(struct comp_test_data *test_data);

int
comp_perf_options_check(struct comp_test_data *test_data);

#endif
