/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include <rte_compressdev.h>

#include "comp_perf_options.h"

#define NUM_MAX_XFORMS 16
#define NUM_MAX_INFLIGHT_OPS 512
#define EXPANSE_RATIO 1.05
#define MIN_COMPRESSED_BUF_SIZE 8

#define DIV_CEIL(a, b)  ((a) / (b) + ((a) % (b) != 0))

/* Cleanup state machine */
static enum cleanup_st {
	ST_CLEAR = 0,
	ST_TEST_DATA,
	ST_COMPDEV,
	ST_INPUT_DATA,
	ST_MEMORY_ALLOC,
	ST_PREPARE_BUF,
	ST_DURING_TEST
} cleanup = ST_CLEAR;

static int
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

static int
comp_perf_check_capabilities(struct comp_test_data *test_data)
{
	const struct rte_compressdev_capabilities *cap;

	cap = rte_compressdev_capability_get(test_data->cdev_id,
					     RTE_COMP_ALGO_DEFLATE);

	if (cap == NULL) {
		RTE_LOG(ERR, USER1,
			"Compress device does not support DEFLATE\n");
		return -1;
	}

	uint64_t comp_flags = cap->comp_feature_flags;

	/* Huffman enconding */
	if (test_data->huffman_enc == RTE_COMP_HUFFMAN_FIXED &&
			(comp_flags & RTE_COMP_FF_HUFFMAN_FIXED) == 0) {
		RTE_LOG(ERR, USER1,
			"Compress device does not supported Fixed Huffman\n");
		return -1;
	}

	if (test_data->huffman_enc == RTE_COMP_HUFFMAN_DYNAMIC &&
			(comp_flags & RTE_COMP_FF_HUFFMAN_DYNAMIC) == 0) {
		RTE_LOG(ERR, USER1,
			"Compress device does not supported Dynamic Huffman\n");
		return -1;
	}

	/* Window size */
	if (test_data->window_sz != -1) {
		if (param_range_check(test_data->window_sz, &cap->window_size)
				< 0) {
			RTE_LOG(ERR, USER1,
				"Compress device does not support "
				"this window size\n");
			return -1;
		}
	} else
		/* Set window size to PMD maximum if none was specified */
		test_data->window_sz = cap->window_size.max;

	/* Check if chained mbufs is supported */
	if (test_data->max_sgl_segs > 1  &&
			(comp_flags & RTE_COMP_FF_OOP_SGL_IN_SGL_OUT) == 0) {
		RTE_LOG(INFO, USER1, "Compress device does not support "
				"chained mbufs. Max SGL segments set to 1\n");
		test_data->max_sgl_segs = 1;
	}

	/* Level 0 support */
	if (test_data->level.min == 0 &&
			(comp_flags & RTE_COMP_FF_NONCOMPRESSED_BLOCKS) == 0) {
		RTE_LOG(ERR, USER1, "Compress device does not support "
				"level 0 (no compression)\n");
		return -1;
	}

	return 0;
}

static int
comp_perf_allocate_memory(struct comp_test_data *test_data)
{
	/* Number of segments for input and output
	 * (compression and decompression)
	 */
	uint32_t total_segs = DIV_CEIL(test_data->input_data_sz,
			test_data->seg_sz);
	test_data->comp_buf_pool = rte_pktmbuf_pool_create("comp_buf_pool",
				total_segs,
				0, 0, test_data->seg_sz + RTE_PKTMBUF_HEADROOM,
				rte_socket_id());
	if (test_data->comp_buf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Mbuf mempool could not be created\n");
		return -1;
	}

	cleanup = ST_MEMORY_ALLOC;
	test_data->decomp_buf_pool = rte_pktmbuf_pool_create("decomp_buf_pool",
				total_segs,
				0, 0, test_data->seg_sz + RTE_PKTMBUF_HEADROOM,
				rte_socket_id());
	if (test_data->decomp_buf_pool == NULL) {
		RTE_LOG(ERR, USER1, "Mbuf mempool could not be created\n");
		return -1;
	}

	test_data->total_bufs = DIV_CEIL(total_segs, test_data->max_sgl_segs);

	test_data->op_pool = rte_comp_op_pool_create("op_pool",
				  test_data->total_bufs,
				  0, 0, rte_socket_id());
	if (test_data->op_pool == NULL) {
		RTE_LOG(ERR, USER1, "Comp op mempool could not be created\n");
		return -1;
	}

	/*
	 * Compressed data might be a bit larger than input data,
	 * if data cannot be compressed
	 */
	test_data->compressed_data = rte_zmalloc_socket(NULL,
				test_data->input_data_sz * EXPANSE_RATIO
						+ MIN_COMPRESSED_BUF_SIZE, 0,
				rte_socket_id());
	if (test_data->compressed_data == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the data from the input "
				"file could not be allocated\n");
		return -1;
	}

	test_data->decompressed_data = rte_zmalloc_socket(NULL,
				test_data->input_data_sz, 0,
				rte_socket_id());
	if (test_data->decompressed_data == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the data from the input "
				"file could not be allocated\n");
		return -1;
	}

	test_data->comp_bufs = rte_zmalloc_socket(NULL,
			test_data->total_bufs * sizeof(struct rte_mbuf *),
			0, rte_socket_id());
	if (test_data->comp_bufs == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the compression mbufs"
				" could not be allocated\n");
		return -1;
	}

	test_data->decomp_bufs = rte_zmalloc_socket(NULL,
			test_data->total_bufs * sizeof(struct rte_mbuf *),
			0, rte_socket_id());
	if (test_data->decomp_bufs == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the decompression mbufs"
				" could not be allocated\n");
		return -1;
	}
	return 0;
}

static int
comp_perf_dump_input_data(struct comp_test_data *test_data)
{
	FILE *f = fopen(test_data->input_file, "r");
	int ret = -1;

	if (f == NULL) {
		RTE_LOG(ERR, USER1, "Input file could not be opened\n");
		return -1;
	}

	if (fseek(f, 0, SEEK_END) != 0) {
		RTE_LOG(ERR, USER1, "Size of input could not be calculated\n");
		goto end;
	}
	size_t actual_file_sz = ftell(f);
	/* If extended input data size has not been set,
	 * input data size = file size
	 */

	if (test_data->input_data_sz == 0)
		test_data->input_data_sz = actual_file_sz;

	if (fseek(f, 0, SEEK_SET) != 0) {
		RTE_LOG(ERR, USER1, "Size of input could not be calculated\n");
		goto end;
	}

	test_data->input_data = rte_zmalloc_socket(NULL,
				test_data->input_data_sz, 0, rte_socket_id());

	if (test_data->input_data == NULL) {
		RTE_LOG(ERR, USER1, "Memory to hold the data from the input "
				"file could not be allocated\n");
		goto end;
	}

	size_t remaining_data = test_data->input_data_sz;
	uint8_t *data = test_data->input_data;

	while (remaining_data > 0) {
		size_t data_to_read = RTE_MIN(remaining_data, actual_file_sz);

		if (fread(data, data_to_read, 1, f) != 1) {
			RTE_LOG(ERR, USER1, "Input file could not be read\n");
			goto end;
		}
		if (fseek(f, 0, SEEK_SET) != 0) {
			RTE_LOG(ERR, USER1,
				"Size of input could not be calculated\n");
			goto end;
		}
		remaining_data -= data_to_read;
		data += data_to_read;
	}

	if (test_data->input_data_sz > actual_file_sz)
		RTE_LOG(INFO, USER1,
		  "%zu bytes read from file %s, extending the file %.2f times\n",
			test_data->input_data_sz, test_data->input_file,
			(double)test_data->input_data_sz/actual_file_sz);
	else
		RTE_LOG(INFO, USER1,
			"%zu bytes read from file %s\n",
			test_data->input_data_sz, test_data->input_file);

	ret = 0;

end:
	fclose(f);
	return ret;
}

static int
comp_perf_initialize_compressdev(struct comp_test_data *test_data)
{
	uint8_t enabled_cdev_count;
	uint8_t enabled_cdevs[RTE_COMPRESS_MAX_DEVS];

	enabled_cdev_count = rte_compressdev_devices_get(test_data->driver_name,
			enabled_cdevs, RTE_COMPRESS_MAX_DEVS);
	if (enabled_cdev_count == 0) {
		RTE_LOG(ERR, USER1, "No compress devices type %s available\n",
				test_data->driver_name);
		return -EINVAL;
	}

	if (enabled_cdev_count > 1)
		RTE_LOG(INFO, USER1,
			"Only the first compress device will be used\n");

	test_data->cdev_id = enabled_cdevs[0];

	if (comp_perf_check_capabilities(test_data) < 0)
		return -1;

	/* Configure compressdev (one device, one queue pair) */
	struct rte_compressdev_config config = {
		.socket_id = rte_socket_id(),
		.nb_queue_pairs = 1,
		.max_nb_priv_xforms = NUM_MAX_XFORMS,
		.max_nb_streams = 0
	};

	if (rte_compressdev_configure(test_data->cdev_id, &config) < 0) {
		RTE_LOG(ERR, USER1, "Device configuration failed\n");
		return -1;
	}

	if (rte_compressdev_queue_pair_setup(test_data->cdev_id, 0,
			NUM_MAX_INFLIGHT_OPS, rte_socket_id()) < 0) {
		RTE_LOG(ERR, USER1, "Queue pair setup failed\n");
		return -1;
	}

	if (rte_compressdev_start(test_data->cdev_id) < 0) {
		RTE_LOG(ERR, USER1, "Device could not be started\n");
		return -1;
	}

	return 0;
}

static int
prepare_bufs(struct comp_test_data *test_data)
{
	uint32_t remaining_data = test_data->input_data_sz;
	uint8_t *input_data_ptr = test_data->input_data;
	size_t data_sz;
	uint8_t *data_addr;
	uint32_t i, j;

	for (i = 0; i < test_data->total_bufs; i++) {
		/* Allocate data in input mbuf and copy data from input file */
		test_data->decomp_bufs[i] =
			rte_pktmbuf_alloc(test_data->decomp_buf_pool);
		if (test_data->decomp_bufs[i] == NULL) {
			RTE_LOG(ERR, USER1, "Could not allocate mbuf\n");
			return -1;
		}

		cleanup = ST_PREPARE_BUF;
		data_sz = RTE_MIN(remaining_data, test_data->seg_sz);
		data_addr = (uint8_t *) rte_pktmbuf_append(
					test_data->decomp_bufs[i], data_sz);
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
				rte_pktmbuf_alloc(test_data->decomp_buf_pool);

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

			if (rte_pktmbuf_chain(test_data->decomp_bufs[i],
					next_seg) < 0) {
				RTE_LOG(ERR, USER1, "Could not chain mbufs\n");
				return -1;
			}
			segs_per_mbuf++;
		}

		/* Allocate data in output mbuf */
		test_data->comp_bufs[i] =
			rte_pktmbuf_alloc(test_data->comp_buf_pool);
		if (test_data->comp_bufs[i] == NULL) {
			RTE_LOG(ERR, USER1, "Could not allocate mbuf\n");
			return -1;
		}
		data_addr = (uint8_t *) rte_pktmbuf_append(
					test_data->comp_bufs[i],
					test_data->seg_sz);
		if (data_addr == NULL) {
			RTE_LOG(ERR, USER1, "Could not append data\n");
			return -1;
		}

		/* Chain mbufs if needed for output mbufs */
		for (j = 1; j < segs_per_mbuf; j++) {
			struct rte_mbuf *next_seg =
				rte_pktmbuf_alloc(test_data->comp_buf_pool);

			if (next_seg == NULL) {
				RTE_LOG(ERR, USER1,
					"Could not allocate mbuf\n");
				return -1;
			}

			data_addr = (uint8_t *)rte_pktmbuf_append(next_seg,
				test_data->seg_sz);

			if (data_addr == NULL) {
				RTE_LOG(ERR, USER1, "Could not append data\n");
				return -1;
			}

			if (rte_pktmbuf_chain(test_data->comp_bufs[i],
					next_seg) < 0) {
				RTE_LOG(ERR, USER1, "Could not chain mbufs\n");
				return -1;
			}
		}
	}

	return 0;
}

static void
free_bufs(struct comp_test_data *test_data)
{
	uint32_t i;

	for (i = 0; i < test_data->total_bufs; i++) {
		rte_pktmbuf_free(test_data->comp_bufs[i]);
		rte_pktmbuf_free(test_data->decomp_bufs[i]);
	}
}

static int
main_loop(struct comp_test_data *test_data, uint8_t level,
			enum rte_comp_xform_type type,
			uint8_t *output_data_ptr,
			size_t *output_data_sz,
			unsigned int benchmarking)
{
	uint8_t dev_id = test_data->cdev_id;
	uint32_t i, iter, num_iter;
	struct rte_comp_op **ops, **deq_ops;
	void *priv_xform = NULL;
	struct rte_comp_xform xform;
	size_t output_size = 0;
	struct rte_mbuf **input_bufs, **output_bufs;
	int res = 0;
	int allocated = 0;

	if (test_data == NULL || !test_data->burst_sz) {
		RTE_LOG(ERR, USER1,
			"Unknown burst size\n");
		return -1;
	}

	ops = rte_zmalloc_socket(NULL,
		2 * test_data->total_bufs * sizeof(struct rte_comp_op *),
		0, rte_socket_id());

	if (ops == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate memory for ops strucures\n");
		return -1;
	}

	deq_ops = &ops[test_data->total_bufs];

	if (type == RTE_COMP_COMPRESS) {
		xform = (struct rte_comp_xform) {
			.type = RTE_COMP_COMPRESS,
			.compress = {
				.algo = RTE_COMP_ALGO_DEFLATE,
				.deflate.huffman = test_data->huffman_enc,
				.level = level,
				.window_size = test_data->window_sz,
				.chksum = RTE_COMP_CHECKSUM_NONE,
				.hash_algo = RTE_COMP_HASH_ALGO_NONE
			}
		};
		input_bufs = test_data->decomp_bufs;
		output_bufs = test_data->comp_bufs;
	} else {
		xform = (struct rte_comp_xform) {
			.type = RTE_COMP_DECOMPRESS,
			.decompress = {
				.algo = RTE_COMP_ALGO_DEFLATE,
				.chksum = RTE_COMP_CHECKSUM_NONE,
				.window_size = test_data->window_sz,
				.hash_algo = RTE_COMP_HASH_ALGO_NONE
			}
		};
		input_bufs = test_data->comp_bufs;
		output_bufs = test_data->decomp_bufs;
	}

	/* Create private xform */
	if (rte_compressdev_private_xform_create(dev_id, &xform,
			&priv_xform) < 0) {
		RTE_LOG(ERR, USER1, "Private xform could not be created\n");
		res = -1;
		goto end;
	}

	uint64_t tsc_start, tsc_end, tsc_duration;

	tsc_start = tsc_end = tsc_duration = 0;
	if (benchmarking) {
		tsc_start = rte_rdtsc();
		num_iter = test_data->num_iter;
	} else
		num_iter = 1;

	for (iter = 0; iter < num_iter; iter++) {
		uint32_t total_ops = test_data->total_bufs;
		uint32_t remaining_ops = test_data->total_bufs;
		uint32_t total_deq_ops = 0;
		uint32_t total_enq_ops = 0;
		uint16_t ops_unused = 0;
		uint16_t num_enq = 0;
		uint16_t num_deq = 0;

		output_size = 0;

		while (remaining_ops > 0) {
			uint16_t num_ops = RTE_MIN(remaining_ops,
						   test_data->burst_sz);
			uint16_t ops_needed = num_ops - ops_unused;

			/*
			 * Move the unused operations from the previous
			 * enqueue_burst call to the front, to maintain order
			 */
			if ((ops_unused > 0) && (num_enq > 0)) {
				size_t nb_b_to_mov =
				      ops_unused * sizeof(struct rte_comp_op *);

				memmove(ops, &ops[num_enq], nb_b_to_mov);
			}

			/* Allocate compression operations */
			if (ops_needed && !rte_comp_op_bulk_alloc(
						test_data->op_pool,
						&ops[ops_unused],
						ops_needed)) {
				RTE_LOG(ERR, USER1,
				      "Could not allocate enough operations\n");
				res = -1;
				goto end;
			}
			allocated += ops_needed;

			for (i = 0; i < ops_needed; i++) {
				/*
				 * Calculate next buffer to attach to operation
				 */
				uint32_t buf_id = total_enq_ops + i +
						ops_unused;
				uint16_t op_id = ops_unused + i;
				/* Reset all data in output buffers */
				struct rte_mbuf *m = output_bufs[buf_id];

				m->pkt_len = test_data->seg_sz * m->nb_segs;
				while (m) {
					m->data_len = m->buf_len - m->data_off;
					m = m->next;
				}
				ops[op_id]->m_src = input_bufs[buf_id];
				ops[op_id]->m_dst = output_bufs[buf_id];
				ops[op_id]->src.offset = 0;
				ops[op_id]->src.length =
					rte_pktmbuf_pkt_len(input_bufs[buf_id]);
				ops[op_id]->dst.offset = 0;
				ops[op_id]->flush_flag = RTE_COMP_FLUSH_FINAL;
				ops[op_id]->input_chksum = buf_id;
				ops[op_id]->private_xform = priv_xform;
			}

			num_enq = rte_compressdev_enqueue_burst(dev_id, 0, ops,
								num_ops);
			ops_unused = num_ops - num_enq;
			remaining_ops -= num_enq;
			total_enq_ops += num_enq;

			num_deq = rte_compressdev_dequeue_burst(dev_id, 0,
							   deq_ops,
							   test_data->burst_sz);
			total_deq_ops += num_deq;
			if (benchmarking == 0) {
				for (i = 0; i < num_deq; i++) {
					struct rte_comp_op *op = deq_ops[i];
					const void *read_data_addr =
						rte_pktmbuf_read(op->m_dst, 0,
						op->produced, output_data_ptr);
					if (read_data_addr == NULL) {
						RTE_LOG(ERR, USER1,
				      "Could not copy buffer in destination\n");
						res = -1;
						goto end;
					}

					if (read_data_addr != output_data_ptr)
						rte_memcpy(output_data_ptr,
							rte_pktmbuf_mtod(
							  op->m_dst, uint8_t *),
							op->produced);
					output_data_ptr += op->produced;
					output_size += op->produced;

				}
			}

			if (iter == num_iter - 1) {
				for (i = 0; i < num_deq; i++) {
					struct rte_comp_op *op = deq_ops[i];
					struct rte_mbuf *m = op->m_dst;

					m->pkt_len = op->produced;
					uint32_t remaining_data = op->produced;
					uint16_t data_to_append;

					while (remaining_data > 0) {
						data_to_append =
							RTE_MIN(remaining_data,
							     test_data->seg_sz);
						m->data_len = data_to_append;
						remaining_data -=
								data_to_append;
						m = m->next;
					}
				}
			}
			rte_mempool_put_bulk(test_data->op_pool,
					     (void **)deq_ops, num_deq);
			allocated -= num_deq;
		}

		/* Dequeue the last operations */
		while (total_deq_ops < total_ops) {
			num_deq = rte_compressdev_dequeue_burst(dev_id, 0,
						deq_ops, test_data->burst_sz);
			total_deq_ops += num_deq;
			if (benchmarking == 0) {
				for (i = 0; i < num_deq; i++) {
					struct rte_comp_op *op = deq_ops[i];
					const void *read_data_addr =
						rte_pktmbuf_read(op->m_dst,
							op->dst.offset,
							op->produced,
							output_data_ptr);
					if (read_data_addr == NULL) {
						RTE_LOG(ERR, USER1,
				      "Could not copy buffer in destination\n");
						res = -1;
						goto end;
					}

					if (read_data_addr != output_data_ptr)
						rte_memcpy(output_data_ptr,
							rte_pktmbuf_mtod(
							op->m_dst, uint8_t *),
							op->produced);
					output_data_ptr += op->produced;
					output_size += op->produced;

				}
			}

			if (iter == num_iter - 1) {
				for (i = 0; i < num_deq; i++) {
					struct rte_comp_op *op = deq_ops[i];
					struct rte_mbuf *m = op->m_dst;

					m->pkt_len = op->produced;
					uint32_t remaining_data = op->produced;
					uint16_t data_to_append;

					while (remaining_data > 0) {
						data_to_append =
						RTE_MIN(remaining_data,
							test_data->seg_sz);
						m->data_len = data_to_append;
						remaining_data -=
								data_to_append;
						m = m->next;
					}
				}
			}
			rte_mempool_put_bulk(test_data->op_pool,
					     (void **)deq_ops, num_deq);
			allocated -= num_deq;
		}
	}

	if (benchmarking) {
		tsc_end = rte_rdtsc();
		tsc_duration = tsc_end - tsc_start;

		if (type == RTE_COMP_COMPRESS)
			test_data->comp_tsc_duration[level] =
					tsc_duration / num_iter;
		else
			test_data->decomp_tsc_duration[level] =
					tsc_duration / num_iter;
	}

	if (benchmarking == 0 && output_data_sz)
		*output_data_sz = output_size;
end:
	rte_mempool_put_bulk(test_data->op_pool, (void **)ops, allocated);
	rte_compressdev_private_xform_free(dev_id, priv_xform);
	rte_free(ops);
	return res;
}

int
main(int argc, char **argv)
{
	uint8_t level, level_idx = 0;
	int ret, i;
	struct comp_test_data *test_data;

	/* Initialise DPDK EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments!\n");
	argc -= ret;
	argv += ret;

	test_data = rte_zmalloc_socket(NULL, sizeof(struct comp_test_data),
					0, rte_socket_id());

	if (test_data == NULL)
		rte_exit(EXIT_FAILURE, "Cannot reserve memory in socket %d\n",
				rte_socket_id());

	cleanup = ST_TEST_DATA;
	comp_perf_options_default(test_data);

	if (comp_perf_options_parse(test_data, argc, argv) < 0) {
		RTE_LOG(ERR, USER1,
			"Parsing one or more user options failed\n");
		ret = EXIT_FAILURE;
		goto end;
	}

	if (comp_perf_options_check(test_data) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	if (comp_perf_initialize_compressdev(test_data) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	cleanup = ST_COMPDEV;
	if (comp_perf_dump_input_data(test_data) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	cleanup = ST_INPUT_DATA;
	if (comp_perf_allocate_memory(test_data) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	if (prepare_bufs(test_data) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	if (test_data->level.inc != 0)
		level = test_data->level.min;
	else
		level = test_data->level.list[0];

	size_t comp_data_sz;
	size_t decomp_data_sz;

	printf("Burst size = %u\n", test_data->burst_sz);
	printf("File size = %zu\n", test_data->input_data_sz);

	printf("%6s%12s%17s%19s%21s%15s%21s%23s%16s\n",
		"Level", "Comp size", "Comp ratio [%]",
		"Comp [Cycles/it]", "Comp [Cycles/Byte]", "Comp [Gbps]",
		"Decomp [Cycles/it]", "Decomp [Cycles/Byte]", "Decomp [Gbps]");

	cleanup = ST_DURING_TEST;
	while (level <= test_data->level.max) {
		/*
		 * Run a first iteration, to verify compression and
		 * get the compression ratio for the level
		 */
		if (main_loop(test_data, level, RTE_COMP_COMPRESS,
			      test_data->compressed_data,
			      &comp_data_sz, 0) < 0) {
			ret = EXIT_FAILURE;
			goto end;
		}

		if (main_loop(test_data, level, RTE_COMP_DECOMPRESS,
			      test_data->decompressed_data,
			      &decomp_data_sz, 0) < 0) {
			ret = EXIT_FAILURE;
			goto end;
		}

		if (decomp_data_sz != test_data->input_data_sz) {
			RTE_LOG(ERR, USER1,
		   "Decompressed data length not equal to input data length\n");
			RTE_LOG(ERR, USER1,
				"Decompressed size = %zu, expected = %zu\n",
				decomp_data_sz, test_data->input_data_sz);
			ret = EXIT_FAILURE;
			goto end;
		} else {
			if (memcmp(test_data->decompressed_data,
					test_data->input_data,
					test_data->input_data_sz) != 0) {
				RTE_LOG(ERR, USER1,
			    "Decompressed data is not the same as file data\n");
				ret = EXIT_FAILURE;
				goto end;
			}
		}

		double ratio = (double) comp_data_sz /
						test_data->input_data_sz * 100;

		/*
		 * Run the tests twice, discarding the first performance
		 * results, before the cache is warmed up
		 */
		for (i = 0; i < 2; i++) {
			if (main_loop(test_data, level, RTE_COMP_COMPRESS,
					NULL, NULL, 1) < 0) {
				ret = EXIT_FAILURE;
				goto end;
			}
		}

		for (i = 0; i < 2; i++) {
			if (main_loop(test_data, level, RTE_COMP_DECOMPRESS,
					NULL, NULL, 1) < 0) {
				ret = EXIT_FAILURE;
				goto end;
			}
		}

		uint64_t comp_tsc_duration =
				test_data->comp_tsc_duration[level];
		double comp_tsc_byte = (double)comp_tsc_duration /
						test_data->input_data_sz;
		double comp_gbps = rte_get_tsc_hz() / comp_tsc_byte * 8 /
				1000000000;
		uint64_t decomp_tsc_duration =
				test_data->decomp_tsc_duration[level];
		double decomp_tsc_byte = (double)decomp_tsc_duration /
						test_data->input_data_sz;
		double decomp_gbps = rte_get_tsc_hz() / decomp_tsc_byte * 8 /
				1000000000;

		printf("%6u%12zu%17.2f%19"PRIu64"%21.2f"
					"%15.2f%21"PRIu64"%23.2f%16.2f\n",
		       level, comp_data_sz, ratio, comp_tsc_duration,
		       comp_tsc_byte, comp_gbps, decomp_tsc_duration,
		       decomp_tsc_byte, decomp_gbps);

		if (test_data->level.inc != 0)
			level += test_data->level.inc;
		else {
			if (++level_idx == test_data->level.count)
				break;
			level = test_data->level.list[level_idx];
		}
	}

	ret = EXIT_SUCCESS;

end:
	switch (cleanup) {

	case ST_DURING_TEST:
	case ST_PREPARE_BUF:
		free_bufs(test_data);
		/* fallthrough */
	case ST_MEMORY_ALLOC:
		rte_free(test_data->decomp_bufs);
		rte_free(test_data->comp_bufs);
		rte_free(test_data->decompressed_data);
		rte_free(test_data->compressed_data);
		rte_mempool_free(test_data->op_pool);
		rte_mempool_free(test_data->decomp_buf_pool);
		rte_mempool_free(test_data->comp_buf_pool);
		/* fallthrough */
	case ST_INPUT_DATA:
		rte_free(test_data->input_data);
		/* fallthrough */
	case ST_COMPDEV:
		if (test_data->cdev_id != -1)
			rte_compressdev_stop(test_data->cdev_id);
		/* fallthrough */
	case ST_TEST_DATA:
		rte_free(test_data);
		/* fallthrough */
	case ST_CLEAR:
	default:
		i = rte_eal_cleanup();
		if (i) {
			RTE_LOG(ERR, USER1,
				"Error from rte_eal_cleanup(), %d\n", i);
			ret = i;
		}
		break;
	}
	return ret;
}
