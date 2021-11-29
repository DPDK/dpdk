/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_compressdev.h>

#include "comp_perf_test_verify.h"
#include "comp_perf_test_common.h"

void
cperf_verify_test_destructor(void *arg)
{
	if (arg) {
		comp_perf_free_memory(
				((struct cperf_verify_ctx *)arg)->options,
				&((struct cperf_verify_ctx *)arg)->mem);
		rte_free(arg);
	}
}

void *
cperf_verify_test_constructor(uint8_t dev_id, uint16_t qp_id,
		struct comp_test_data *options)
{
	struct cperf_verify_ctx *ctx = NULL;

	ctx = rte_malloc(NULL, sizeof(struct cperf_verify_ctx), 0);

	if (ctx == NULL)
		return NULL;

	ctx->mem.dev_id = dev_id;
	ctx->mem.qp_id = qp_id;
	ctx->options = options;

	if (!comp_perf_allocate_memory(ctx->options, &ctx->mem) &&
			!prepare_bufs(ctx->options, &ctx->mem))
		return ctx;

	cperf_verify_test_destructor(ctx);
	return NULL;
}

static int
main_loop(struct cperf_verify_ctx *ctx, enum rte_comp_xform_type type)
{
	struct comp_test_data *test_data = ctx->options;
	uint8_t *output_data_ptr = NULL;
	size_t *output_data_sz = NULL;
	struct cperf_mem_resources *mem = &ctx->mem;

	uint8_t dev_id = mem->dev_id;
	uint32_t i, iter, num_iter;
	struct rte_comp_op **ops, **deq_ops;
	void *priv_xform = NULL;
	struct rte_comp_xform xform;
	size_t output_size = 0;
	struct rte_mbuf **input_bufs, **output_bufs;
	int res = 0;
	int allocated = 0;
	uint32_t out_seg_sz;

	if (test_data == NULL || !test_data->burst_sz) {
		RTE_LOG(ERR, USER1,
			"Unknown burst size\n");
		return -1;
	}

	ops = rte_zmalloc_socket(NULL,
		2 * mem->total_bufs * sizeof(struct rte_comp_op *),
		0, rte_socket_id());

	if (ops == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate memory for ops structures\n");
		return -1;
	}

	deq_ops = &ops[mem->total_bufs];

	if (type == RTE_COMP_COMPRESS) {
		xform = (struct rte_comp_xform) {
			.type = RTE_COMP_COMPRESS,
			.compress = {
				.algo = RTE_COMP_ALGO_DEFLATE,
				.deflate.huffman = test_data->huffman_enc,
				.level = test_data->level,
				.window_size = test_data->window_sz,
				.chksum = RTE_COMP_CHECKSUM_NONE,
				.hash_algo = RTE_COMP_HASH_ALGO_NONE
			}
		};
		output_data_ptr = ctx->mem.compressed_data;
		output_data_sz = &ctx->comp_data_sz;
		input_bufs = mem->decomp_bufs;
		output_bufs = mem->comp_bufs;
		out_seg_sz = test_data->out_seg_sz;
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
		output_data_ptr = ctx->mem.decompressed_data;
		output_data_sz = &ctx->decomp_data_sz;
		input_bufs = mem->comp_bufs;
		output_bufs = mem->decomp_bufs;
		out_seg_sz = test_data->seg_sz;
	}

	/* Create private xform */
	if (rte_compressdev_private_xform_create(dev_id, &xform,
			&priv_xform) < 0) {
		RTE_LOG(ERR, USER1, "Private xform could not be created\n");
		res = -1;
		goto end;
	}

	num_iter = 1;

	for (iter = 0; iter < num_iter; iter++) {
		uint32_t total_ops = mem->total_bufs;
		uint32_t remaining_ops = mem->total_bufs;
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
						mem->op_pool,
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

				m->pkt_len = out_seg_sz * m->nb_segs;
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

			if (unlikely(test_data->perf_comp_force_stop))
				goto end;

			num_enq = rte_compressdev_enqueue_burst(dev_id,
								mem->qp_id, ops,
								num_ops);
			if (num_enq == 0) {
				struct rte_compressdev_stats stats;

				rte_compressdev_stats_get(dev_id, &stats);
				if (stats.enqueue_err_count) {
					res = -1;
					goto end;
				}
			}

			ops_unused = num_ops - num_enq;
			remaining_ops -= num_enq;
			total_enq_ops += num_enq;

			num_deq = rte_compressdev_dequeue_burst(dev_id,
							   mem->qp_id,
							   deq_ops,
							   test_data->burst_sz);
			total_deq_ops += num_deq;

			for (i = 0; i < num_deq; i++) {
				struct rte_comp_op *op = deq_ops[i];

				if (op->status ==
				  RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED ||
				  op->status ==
				  RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE) {
					RTE_LOG(ERR, USER1,
"Out of space error occurred due to uncompressible input data expanding to larger than destination buffer. Increase the EXPANSE_RATIO constant to use this data.\n");
					res = -1;
					goto end;
				} else if (op->status !=
						RTE_COMP_OP_STATUS_SUCCESS) {
					RTE_LOG(ERR, USER1,
						"Some operations were not successful\n");
					goto end;
				}

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
						   rte_pktmbuf_mtod(op->m_dst,
								    uint8_t *),
						   op->produced);
				output_data_ptr += op->produced;
				output_size += op->produced;

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
							out_seg_sz);
						m->data_len = data_to_append;
						remaining_data -=
								data_to_append;
						m = m->next;
					}
				}
			}
			rte_mempool_put_bulk(mem->op_pool,
					     (void **)deq_ops, num_deq);
			allocated -= num_deq;
		}

		/* Dequeue the last operations */
		while (total_deq_ops < total_ops) {
			if (unlikely(test_data->perf_comp_force_stop))
				goto end;

			num_deq = rte_compressdev_dequeue_burst(dev_id,
							mem->qp_id,
							deq_ops,
							test_data->burst_sz);
			if (num_deq == 0) {
				struct rte_compressdev_stats stats;

				rte_compressdev_stats_get(dev_id, &stats);
				if (stats.dequeue_err_count) {
					res = -1;
					goto end;
				}
			}

			total_deq_ops += num_deq;

			for (i = 0; i < num_deq; i++) {
				struct rte_comp_op *op = deq_ops[i];

				if (op->status ==
				  RTE_COMP_OP_STATUS_OUT_OF_SPACE_TERMINATED ||
				  op->status ==
				  RTE_COMP_OP_STATUS_OUT_OF_SPACE_RECOVERABLE) {
					RTE_LOG(ERR, USER1,
"Out of space error occurred due to uncompressible input data expanding to larger than destination buffer. Increase the EXPANSE_RATIO constant to use this data.\n");
					res = -1;
					goto end;
				} else if (op->status !=
						RTE_COMP_OP_STATUS_SUCCESS) {
					RTE_LOG(ERR, USER1,
						"Some operations were not successful\n");
					goto end;
				}
				const void *read_data_addr =
						rte_pktmbuf_read(op->m_dst,
								 op->dst.offset,
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
							out_seg_sz);
						m->data_len = data_to_append;
						remaining_data -=
								data_to_append;
						m = m->next;
					}
				}
			}
			rte_mempool_put_bulk(mem->op_pool,
					     (void **)deq_ops, num_deq);
			allocated -= num_deq;
		}
	}

	if (output_data_sz)
		*output_data_sz = output_size;
end:
	rte_mempool_put_bulk(mem->op_pool, (void **)ops, allocated);
	rte_compressdev_private_xform_free(dev_id, priv_xform);
	rte_free(ops);

	if (test_data->perf_comp_force_stop) {
		RTE_LOG(ERR, USER1,
		      "lcore: %d Perf. test has been aborted by user\n",
			mem->lcore_id);
		res = -1;
	}

	return res;
}

int
cperf_verify_test_runner(void *test_ctx)
{
	struct cperf_verify_ctx *ctx = test_ctx;
	struct comp_test_data *test_data = ctx->options;
	int ret = EXIT_SUCCESS;
	static uint16_t display_once;
	uint32_t lcore = rte_lcore_id();

	ctx->mem.lcore_id = lcore;

	test_data->ratio = 0;

	if (main_loop(ctx, RTE_COMP_COMPRESS) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	if (main_loop(ctx, RTE_COMP_DECOMPRESS) < 0) {
		ret = EXIT_FAILURE;
		goto end;
	}

	if (ctx->decomp_data_sz != test_data->input_data_sz) {
		RTE_LOG(ERR, USER1,
	   "Decompressed data length not equal to input data length\n");
		RTE_LOG(ERR, USER1,
			"Decompressed size = %zu, expected = %zu\n",
			ctx->decomp_data_sz, test_data->input_data_sz);
		ret = EXIT_FAILURE;
		goto end;
	} else {
		if (memcmp(ctx->mem.decompressed_data,
				test_data->input_data,
				test_data->input_data_sz) != 0) {
			RTE_LOG(ERR, USER1,
		    "Decompressed data is not the same as file data\n");
			ret = EXIT_FAILURE;
			goto end;
		}
	}

	ctx->ratio = (double) ctx->comp_data_sz /
			test_data->input_data_sz * 100;

	uint16_t exp = 0;
	if (!ctx->silent) {
		if (__atomic_compare_exchange_n(&display_once, &exp, 1, 0,
				__ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
			printf("%12s%6s%12s%17s\n",
			    "lcore id", "Level", "Comp size", "Comp ratio [%]");
		}
		printf("%12u%6u%12zu%17.2f\n",
		       ctx->mem.lcore_id,
		       test_data->level, ctx->comp_data_sz, ctx->ratio);
	}

end:
	return ret;
}
