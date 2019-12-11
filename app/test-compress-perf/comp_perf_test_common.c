/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_compressdev.h>

#include "comp_perf.h"
#include "comp_perf_options.h"
#include "comp_perf_test_throughput.h"
#include "comp_perf_test_cyclecount.h"
#include "comp_perf_test_common.h"
#include "comp_perf_test_verify.h"


#define DIV_CEIL(a, b)  ((a) / (b) + ((a) % (b) != 0))

struct cperf_buffer_info {
	uint16_t total_segments;
	uint16_t segment_sz;
	uint16_t last_segment_sz;
	uint32_t total_buffs;	      /*number of buffers = number of ops*/
	uint16_t segments_per_buff;
	uint16_t segments_per_last_buff;
	size_t input_data_sz;
};

static struct cperf_buffer_info buffer_info;

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

	/* We're looking for nearest power of 2 buffer size, which is greater
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
comp_perf_free_memory(struct comp_test_data *test_data,
		      struct cperf_mem_resources *mem)
{
	uint32_t i;

	if (mem->decomp_bufs != NULL)
		for (i = 0; i < mem->total_bufs; i++)
			rte_pktmbuf_free(mem->decomp_bufs[i]);

	if (mem->comp_bufs != NULL)
		for (i = 0; i < mem->total_bufs; i++)
			rte_pktmbuf_free(mem->comp_bufs[i]);

	rte_free(mem->decomp_bufs);
	rte_free(mem->comp_bufs);
	rte_free(mem->decompressed_data);
	rte_free(mem->compressed_data);
	rte_mempool_free(mem->op_pool);
	rte_mempool_free(mem->decomp_buf_pool);
	rte_mempool_free(mem->comp_buf_pool);

	/* external mbuf support */
	if (mem->decomp_memzones != NULL) {
		for (i = 0; i < test_data->total_segs; i++)
			rte_memzone_free(mem->decomp_memzones[i]);
		rte_free(mem->decomp_memzones);
	}
	if (mem->comp_memzones != NULL) {
		for (i = 0; i < test_data->total_segs; i++)
			rte_memzone_free(mem->comp_memzones[i]);
		rte_free(mem->comp_memzones);
	}
	rte_free(mem->decomp_buf_infos);
	rte_free(mem->comp_buf_infos);
}

static void
comp_perf_extbuf_free_cb(void *addr __rte_unused, void *opaque __rte_unused)
{
}

static const struct rte_memzone *
comp_perf_make_memzone(const char *name, struct cperf_mem_resources *mem,
		       unsigned int number, size_t size)
{
	unsigned int socket_id = rte_socket_id();
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *memzone;

	snprintf(mz_name, RTE_MEMZONE_NAMESIZE, "%s_s%u_d%u_q%u_%d", name,
		 socket_id, mem->dev_id, mem->qp_id, number);
	memzone = rte_memzone_lookup(mz_name);
	if (memzone != NULL && memzone->len != size) {
		rte_memzone_free(memzone);
		memzone = NULL;
	}
	if (memzone == NULL) {
		memzone = rte_memzone_reserve_aligned(mz_name, size, socket_id,
				RTE_MEMZONE_IOVA_CONTIG, RTE_CACHE_LINE_SIZE);
		if (memzone == NULL)
			RTE_LOG(ERR, USER1, "Can't allocate memory zone %s\n",
				mz_name);
	}
	return memzone;
}

static int
comp_perf_allocate_external_mbufs(struct comp_test_data *test_data,
				  struct cperf_mem_resources *mem)
{
	uint32_t i;

	mem->comp_memzones = rte_zmalloc_socket(NULL,
		test_data->total_segs * sizeof(struct rte_memzone *),
		0, rte_socket_id());

	if (mem->comp_memzones == NULL) {
		RTE_LOG(ERR, USER1,
			"Memory to hold the compression memzones could not be allocated\n");
		return -1;
	}

	mem->decomp_memzones = rte_zmalloc_socket(NULL,
		test_data->total_segs * sizeof(struct rte_memzone *),
		0, rte_socket_id());

	if (mem->decomp_memzones == NULL) {
		RTE_LOG(ERR, USER1,
			"Memory to hold the decompression memzones could not be allocated\n");
		return -1;
	}

	mem->comp_buf_infos = rte_zmalloc_socket(NULL,
		test_data->total_segs * sizeof(struct rte_mbuf_ext_shared_info),
		0, rte_socket_id());

	if (mem->comp_buf_infos == NULL) {
		RTE_LOG(ERR, USER1,
			"Memory to hold the compression buf infos could not be allocated\n");
		return -1;
	}

	mem->decomp_buf_infos = rte_zmalloc_socket(NULL,
		test_data->total_segs * sizeof(struct rte_mbuf_ext_shared_info),
		0, rte_socket_id());

	if (mem->decomp_buf_infos == NULL) {
		RTE_LOG(ERR, USER1,
			"Memory to hold the decompression buf infos could not be allocated\n");
		return -1;
	}

	for (i = 0; i < test_data->total_segs; i++) {
		mem->comp_memzones[i] = comp_perf_make_memzone("comp", mem,
				i, test_data->out_seg_sz);
		if (mem->comp_memzones[i] == NULL) {
			RTE_LOG(ERR, USER1,
				"Memory to hold the compression memzone could not be allocated\n");
			return -1;
		}

		mem->decomp_memzones[i] = comp_perf_make_memzone("decomp", mem,
				i, test_data->seg_sz);
		if (mem->decomp_memzones[i] == NULL) {
			RTE_LOG(ERR, USER1,
				"Memory to hold the decompression memzone could not be allocated\n");
			return -1;
		}

		mem->comp_buf_infos[i].free_cb =
				comp_perf_extbuf_free_cb;
		mem->comp_buf_infos[i].fcb_opaque = NULL;
		rte_mbuf_ext_refcnt_set(&mem->comp_buf_infos[i], 1);

		mem->decomp_buf_infos[i].free_cb =
				comp_perf_extbuf_free_cb;
		mem->decomp_buf_infos[i].fcb_opaque = NULL;
		rte_mbuf_ext_refcnt_set(&mem->decomp_buf_infos[i], 1);
	}

	return 0;
}

int
comp_perf_allocate_memory(struct comp_test_data *test_data,
			  struct cperf_mem_resources *mem)
{
	uint16_t comp_mbuf_size;
	uint16_t decomp_mbuf_size;

	test_data->out_seg_sz = find_buf_size(test_data->seg_sz);

	/* Number of segments for input and output
	 * (compression and decompression)
	 */
	test_data->total_segs = DIV_CEIL(test_data->input_data_sz,
			test_data->seg_sz);

	if (test_data->use_external_mbufs != 0) {
		if (comp_perf_allocate_external_mbufs(test_data, mem) < 0)
			return -1;
		comp_mbuf_size = 0;
		decomp_mbuf_size = 0;
	} else {
		comp_mbuf_size = test_data->out_seg_sz + RTE_PKTMBUF_HEADROOM;
		decomp_mbuf_size = test_data->seg_sz + RTE_PKTMBUF_HEADROOM;
	}

	char pool_name[32] = "";

	snprintf(pool_name, sizeof(pool_name), "comp_buf_pool_%u_qp_%u",
			mem->dev_id, mem->qp_id);
	mem->comp_buf_pool = rte_pktmbuf_pool_create(pool_name,
				test_data->total_segs,
				0, 0,
				comp_mbuf_size,
				rte_socket_id());
	if (mem->comp_buf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Mbuf mempool could not be created\n");
		return -1;
	}

	snprintf(pool_name, sizeof(pool_name), "decomp_buf_pool_%u_qp_%u",
			mem->dev_id, mem->qp_id);
	mem->decomp_buf_pool = rte_pktmbuf_pool_create(pool_name,
				test_data->total_segs,
				0, 0,
				decomp_mbuf_size,
				rte_socket_id());
	if (mem->decomp_buf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Mbuf mempool could not be created\n");
		return -1;
	}

	mem->total_bufs = DIV_CEIL(test_data->total_segs,
				   test_data->max_sgl_segs);

	snprintf(pool_name, sizeof(pool_name), "op_pool_%u_qp_%u",
			mem->dev_id, mem->qp_id);

	/* one mempool for both src and dst mbufs */
	mem->op_pool = rte_comp_op_pool_create(pool_name,
				mem->total_bufs * 2,
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
				RTE_MAX(
				    (size_t) test_data->out_seg_sz *
							  test_data->total_segs,
				    (size_t) MIN_COMPRESSED_BUF_SIZE),
				0,
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

	buffer_info.total_segments = test_data->total_segs;
	buffer_info.segment_sz = test_data->seg_sz;
	buffer_info.total_buffs = mem->total_bufs;
	buffer_info.segments_per_buff = test_data->max_sgl_segs;
	buffer_info.input_data_sz = test_data->input_data_sz;

	return 0;
}

int
prepare_bufs(struct comp_test_data *test_data, struct cperf_mem_resources *mem)
{
	uint32_t remaining_data = test_data->input_data_sz;
	uint8_t *input_data_ptr = test_data->input_data;
	size_t data_sz = 0;
	uint8_t *data_addr;
	uint32_t i, j;
	uint16_t segs_per_mbuf = 0;
	uint32_t cmz = 0;
	uint32_t dmz = 0;

	for (i = 0; i < mem->total_bufs; i++) {
		/* Allocate data in input mbuf and copy data from input file */
		mem->decomp_bufs[i] =
			rte_pktmbuf_alloc(mem->decomp_buf_pool);
		if (mem->decomp_bufs[i] == NULL) {
			RTE_LOG(ERR, USER1, "Could not allocate mbuf\n");
			return -1;
		}

		data_sz = RTE_MIN(remaining_data, test_data->seg_sz);

		if (test_data->use_external_mbufs != 0) {
			rte_pktmbuf_attach_extbuf(mem->decomp_bufs[i],
					mem->decomp_memzones[dmz]->addr,
					mem->decomp_memzones[dmz]->iova,
					test_data->seg_sz,
					&mem->decomp_buf_infos[dmz]);
			dmz++;
		}

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
		segs_per_mbuf = 1;

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

			if (test_data->use_external_mbufs != 0) {
				rte_pktmbuf_attach_extbuf(
					next_seg,
					mem->decomp_memzones[dmz]->addr,
					mem->decomp_memzones[dmz]->iova,
					test_data->seg_sz,
					&mem->decomp_buf_infos[dmz]);
				dmz++;
			}

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

		if (test_data->use_external_mbufs != 0) {
			rte_pktmbuf_attach_extbuf(mem->comp_bufs[i],
					mem->comp_memzones[cmz]->addr,
					mem->comp_memzones[cmz]->iova,
					test_data->out_seg_sz,
					&mem->comp_buf_infos[cmz]);
			cmz++;
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

			if (test_data->use_external_mbufs != 0) {
				rte_pktmbuf_attach_extbuf(
					next_seg,
					mem->comp_memzones[cmz]->addr,
					mem->comp_memzones[cmz]->iova,
					test_data->out_seg_sz,
					&mem->comp_buf_infos[cmz]);
				cmz++;
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

	buffer_info.segments_per_last_buff = segs_per_mbuf;
	buffer_info.last_segment_sz = data_sz;

	return 0;
}

void
print_test_dynamics(const struct comp_test_data *test_data)
{
	uint32_t opt_total_segs = DIV_CEIL(buffer_info.input_data_sz,
			MAX_SEG_SIZE);

	if (buffer_info.total_buffs > 1) {
		if (test_data->test == CPERF_TEST_TYPE_THROUGHPUT) {
			printf("\nWarning: for the current input parameters, number"
				" of ops is higher than one, which may result"
				" in sub-optimal performance.\n");
			printf("To improve the performance (for the current"
				" input data) following parameters are"
				" suggested:\n");
			printf("	* Segment size: %d\n",
			       MAX_SEG_SIZE);
			printf("	* Number of segments: %u\n",
			       opt_total_segs);
		}
	} else if (buffer_info.total_buffs == 1) {
		printf("\nInfo: there is only one op with %u segments -"
				" the compression ratio is the best.\n",
			buffer_info.segments_per_last_buff);
		if (buffer_info.segment_sz < MAX_SEG_SIZE)
			printf("To reduce compression time, please use"
					" bigger segment size: %d.\n",
				MAX_SEG_SIZE);
		else if (buffer_info.segment_sz == MAX_SEG_SIZE)
			printf("Segment size is optimal for the best"
					" performance.\n");
	} else
		printf("Warning: something wrong happened!!\n");

	printf("\nFor the current input parameters (segment size = %u,"
			" maximum segments per SGL = %u):\n",
		buffer_info.segment_sz,
		buffer_info.segments_per_buff);
	printf("	* Total number of buffers: %d\n",
		buffer_info.total_segments);
	printf("	* %u buffer(s) %u bytes long, last buffer %u"
			" byte(s) long\n",
		buffer_info.total_segments - 1,
		buffer_info.segment_sz,
		buffer_info.last_segment_sz);
	printf("	* Number of ops: %u\n", buffer_info.total_buffs);
	printf("	* Total memory allocation: %u\n",
		(buffer_info.total_segments - 1) * buffer_info.segment_sz
		+ buffer_info.last_segment_sz);
	if (buffer_info.total_buffs > 1)
		printf("	* %u ops: %u segment(s) in each,"
				" segment size %u\n",
			buffer_info.total_buffs - 1,
			buffer_info.segments_per_buff,
			buffer_info.segment_sz);
	if (buffer_info.segments_per_last_buff > 1) {
		printf("	* 1 op %u segments:\n",
				buffer_info.segments_per_last_buff);
		printf("		o %u segment size %u\n",
			buffer_info.segments_per_last_buff - 1,
			buffer_info.segment_sz);
		printf("		o last segment size %u\n",
			buffer_info.last_segment_sz);
	} else if (buffer_info.segments_per_last_buff == 1) {
		printf("	* 1 op (the last one): %u segment %u"
				" byte(s) long\n\n",
			buffer_info.segments_per_last_buff,
			buffer_info.last_segment_sz);
	}
	printf("\n");
}
