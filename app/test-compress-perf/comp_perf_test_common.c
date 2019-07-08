/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_compressdev.h>

#include "comp_perf_options.h"
#include "comp_perf_test_verify.h"
#include "comp_perf_test_benchmark.h"
#include "comp_perf.h"
#include "comp_perf_test_common.h"

#define DIV_CEIL(a, b)  ((a) / (b) + ((a) % (b) != 0))

int
param_range_check(uint16_t size, const struct rte_param_log2_range *range)
{
	unsigned int next_size;

	/* Check lower/upper bounds */
	if (size < range->min)
		return -1;

	if (size > range->max)
		return -1;

	/* If range is actually only one value, size is correct */
	if (range->increment == 0)
		return 0;

	/* Check if value is one of the supported sizes */
	for (next_size = range->min; next_size <= range->max;
			next_size += range->increment)
		if (size == next_size)
			return 0;

	return -1;
}

static uint32_t
find_buf_size(uint32_t input_size)
{
	uint32_t i;

	/* From performance point of view the buffer size should be a
	 * power of 2 but also should be enough to store incompressible data
	 */

	/* We're looking for nearest power of 2 buffer size, which is greather
	 * than input_size
	 */
	uint32_t size =
		!input_size ? MIN_COMPRESSED_BUF_SIZE : (input_size << 1);

	for (i = UINT16_MAX + 1; !(i & size); i >>= 1)
		;

	return i > ((UINT16_MAX + 1) >> 1)
			? (uint32_t)((float)input_size * EXPANSE_RATIO)
			: i;
}

void
comp_perf_free_memory(struct cperf_mem_resources *mem)
{
	uint32_t i;

	for (i = 0; i < mem->total_bufs; i++) {
		rte_pktmbuf_free(mem->comp_bufs[i]);
		rte_pktmbuf_free(mem->decomp_bufs[i]);
	}

	rte_free(mem->decomp_bufs);
	rte_free(mem->comp_bufs);
	rte_free(mem->decompressed_data);
	rte_free(mem->compressed_data);
	rte_mempool_free(mem->op_pool);
	rte_mempool_free(mem->decomp_buf_pool);
	rte_mempool_free(mem->comp_buf_pool);
}

int
comp_perf_allocate_memory(struct comp_test_data *test_data,
			  struct cperf_mem_resources *mem)
{
	test_data->out_seg_sz = find_buf_size(test_data->seg_sz);
	/* Number of segments for input and output
	 * (compression and decompression)
	 */
	uint32_t total_segs = DIV_CEIL(test_data->input_data_sz,
			test_data->seg_sz);
	char pool_name[32] = "";

	snprintf(pool_name, sizeof(pool_name), "comp_buf_pool_%u_qp_%u",
			mem->dev_id, mem->qp_id);
	mem->comp_buf_pool = rte_pktmbuf_pool_create(pool_name,
				total_segs,
				0, 0,
				test_data->out_seg_sz + RTE_PKTMBUF_HEADROOM,
				rte_socket_id());
	if (mem->comp_buf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Mbuf mempool could not be created\n");
		return -1;
	}

	snprintf(pool_name, sizeof(pool_name), "decomp_buf_pool_%u_qp_%u",
			mem->dev_id, mem->qp_id);
	mem->decomp_buf_pool = rte_pktmbuf_pool_create(pool_name,
				total_segs,
				0, 0, test_data->seg_sz + RTE_PKTMBUF_HEADROOM,
				rte_socket_id());
	if (mem->decomp_buf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Mbuf mempool could not be created\n");
		return -1;
	}

	mem->total_bufs = DIV_CEIL(total_segs, test_data->max_sgl_segs);

	snprintf(pool_name, sizeof(pool_name), "op_pool_%u_qp_%u",
			mem->dev_id, mem->qp_id);
	mem->op_pool = rte_comp_op_pool_create(pool_name,
				  mem->total_bufs,
				  0, 0, rte_socket_id());
	if (mem->op_pool == NULL) {
		RTE_LOG(ERR, USER1, "Comp op mempool could not be created\n");
		return -1;
	}

	/*
	 * Compressed data might be a bit larger than input data,
	 * if data cannot be compressed
	 */
	mem->compressed_data = rte_zmalloc_socket(NULL,
				test_data->input_data_sz * EXPANSE_RATIO
						+ MIN_COMPRESSED_BUF_SIZE, 0,
				rte_socket_id());
	if (mem->compressed_data == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the data from the input "
				"file could not be allocated\n");
		return -1;
	}

	mem->decompressed_data = rte_zmalloc_socket(NULL,
				test_data->input_data_sz, 0,
				rte_socket_id());
	if (mem->decompressed_data == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the data from the input "
				"file could not be allocated\n");
		return -1;
	}

	mem->comp_bufs = rte_zmalloc_socket(NULL,
			mem->total_bufs * sizeof(struct rte_mbuf *),
			0, rte_socket_id());
	if (mem->comp_bufs == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the compression mbufs"
				" could not be allocated\n");
		return -1;
	}

	mem->decomp_bufs = rte_zmalloc_socket(NULL,
			mem->total_bufs * sizeof(struct rte_mbuf *),
			0, rte_socket_id());
	if (mem->decomp_bufs == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the decompression mbufs"
				" could not be allocated\n");
		return -1;
	}
	return 0;
}

int
prepare_bufs(struct comp_test_data *test_data, struct cperf_mem_resources *mem)
{
	uint32_t remaining_data = test_data->input_data_sz;
	uint8_t *input_data_ptr = test_data->input_data;
	size_t data_sz;
	uint8_t *data_addr;
	uint32_t i, j;

	for (i = 0; i < mem->total_bufs; i++) {
		/* Allocate data in input mbuf and copy data from input file */
		mem->decomp_bufs[i] =
			rte_pktmbuf_alloc(mem->decomp_buf_pool);
		if (mem->decomp_bufs[i] == NULL) {
			RTE_LOG(ERR, USER1, "Could not allocate mbuf\n");
			return -1;
		}

		data_sz = RTE_MIN(remaining_data, test_data->seg_sz);
		data_addr = (uint8_t *) rte_pktmbuf_append(
					mem->decomp_bufs[i], data_sz);
		if (data_addr == NULL) {
			RTE_LOG(ERR, USER1, "Could not append data\n");
			return -1;
		}
		rte_memcpy(data_addr, input_data_ptr, data_sz);

		input_data_ptr += data_sz;
		remaining_data -= data_sz;

		/* Already one segment in the mbuf */
		uint16_t segs_per_mbuf = 1;

		/* Chain mbufs if needed for input mbufs */
		while (segs_per_mbuf < test_data->max_sgl_segs
				&& remaining_data > 0) {
			struct rte_mbuf *next_seg =
				rte_pktmbuf_alloc(mem->decomp_buf_pool);

			if (next_seg == NULL) {
				RTE_LOG(ERR, USER1,
					"Could not allocate mbuf\n");
				return -1;
			}

			data_sz = RTE_MIN(remaining_data, test_data->seg_sz);
			data_addr = (uint8_t *)rte_pktmbuf_append(next_seg,
				data_sz);

			if (data_addr == NULL) {
				RTE_LOG(ERR, USER1, "Could not append data\n");
				return -1;
			}

			rte_memcpy(data_addr, input_data_ptr, data_sz);
			input_data_ptr += data_sz;
			remaining_data -= data_sz;

			if (rte_pktmbuf_chain(mem->decomp_bufs[i],
					next_seg) < 0) {
				RTE_LOG(ERR, USER1, "Could not chain mbufs\n");
				return -1;
			}
			segs_per_mbuf++;
		}

		/* Allocate data in output mbuf */
		mem->comp_bufs[i] =
			rte_pktmbuf_alloc(mem->comp_buf_pool);
		if (mem->comp_bufs[i] == NULL) {
			RTE_LOG(ERR, USER1, "Could not allocate mbuf\n");
			return -1;
		}
		data_addr = (uint8_t *) rte_pktmbuf_append(
					mem->comp_bufs[i],
					test_data->out_seg_sz);
		if (data_addr == NULL) {
			RTE_LOG(ERR, USER1, "Could not append data\n");
			return -1;
		}

		/* Chain mbufs if needed for output mbufs */
		for (j = 1; j < segs_per_mbuf; j++) {
			struct rte_mbuf *next_seg =
				rte_pktmbuf_alloc(mem->comp_buf_pool);

			if (next_seg == NULL) {
				RTE_LOG(ERR, USER1,
					"Could not allocate mbuf\n");
				return -1;
			}

			data_addr = (uint8_t *)rte_pktmbuf_append(next_seg,
				test_data->out_seg_sz);

			if (data_addr == NULL) {
				RTE_LOG(ERR, USER1, "Could not append data\n");
				return -1;
			}

			if (rte_pktmbuf_chain(mem->comp_bufs[i],
					next_seg) < 0) {
				RTE_LOG(ERR, USER1, "Could not chain mbufs\n");
				return -1;
			}
		}
	}

	return 0;
}
