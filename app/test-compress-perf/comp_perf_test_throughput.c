/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdlib.h>

#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include <rte_compressdev.h>

#include "comp_perf_test_throughput.h"

void
cperf_throughput_test_destructor(void *arg)
{
	if (arg) {
		comp_perf_free_memory(
			((struct cperf_benchmark_ctx *)arg)->ver.options,
			&((struct cperf_benchmark_ctx *)arg)->ver.mem);
		rte_free(arg);
	}
}

void *
cperf_throughput_test_constructor(uint8_t dev_id, uint16_t qp_id,
		struct comp_test_data *options)
{
	struct cperf_benchmark_ctx *ctx = NULL;

	ctx = rte_malloc(NULL, sizeof(struct cperf_benchmark_ctx), 0);

	if (ctx == NULL)
		return NULL;

	ctx->ver.mem.dev_id = dev_id;
	ctx->ver.mem.qp_id = qp_id;
	ctx->ver.options = options;
	ctx->ver.silent = 1; /* ver. part will be silent */

	if (!comp_perf_allocate_memory(ctx->ver.options, &ctx->ver.mem)
			&& !prepare_bufs(ctx->ver.options, &ctx->ver.mem))
		return ctx;

	cperf_throughput_test_destructor(ctx);
	return NULL;
}

static int
main_loop(struct cperf_benchmark_ctx *ctx, enum rte_comp_xform_type type)
{
	struct comp_test_data *test_data = ctx->ver.options;
	struct cperf_mem_resources *mem = &ctx->ver.mem;
	uint8_t dev_id = mem->dev_id;
	uint32_t i, iter, num_iter;
	struct rte_comp_op **ops, **deq_ops;
	void *priv_xform = NULL;
	struct rte_comp_xform xform;
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
				.algo = test_data->test_algo,
				.level = test_data->level,
				.window_size = test_data->window_sz,
				.chksum = RTE_COMP_CHECKSUM_NONE,
				.hash_algo = RTE_COMP_HASH_ALGO_NONE
			}
		};
		if (test_data->test_algo == RTE_COMP_ALGO_DEFLATE)
			xform.compress.deflate.huffman = test_data->huffman_enc;
		else if (test_data->test_algo == RTE_COMP_ALGO_LZ4)
			xform.compress.lz4.flags = test_data->lz4_flags;
		input_bufs = mem->decomp_bufs;
		output_bufs = mem->comp_bufs;
		out_seg_sz = test_data->out_seg_sz;
	} else {
		xform = (struct rte_comp_xform) {
			.type = RTE_COMP_DECOMPRESS,
			.decompress = {
				.algo = test_data->test_algo,
				.chksum = RTE_COMP_CHECKSUM_NONE,
				.window_size = test_data->window_sz,
				.hash_algo = RTE_COMP_HASH_ALGO_NONE
			}
		};
		if (test_data->test_algo == RTE_COMP_ALGO_LZ4)
			xform.decompress.lz4.flags = test_data->lz4_flags;
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

	uint64_t tsc_start, tsc_end, tsc_duration;

	num_iter = test_data->num_iter;
	tsc_start = tsc_end = tsc_duration = 0;
	tsc_start = rte_rdtsc_precise();

	for (iter = 0; iter < num_iter; iter++) {
		uint32_t total_ops = mem->total_bufs;
		uint32_t remaining_ops = mem->total_bufs;
		uint32_t total_deq_ops = 0;
		uint32_t total_enq_ops = 0;
		uint16_t ops_unused = 0;
		uint16_t num_enq = 0;
		uint16_t num_deq = 0;

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

			if (iter == num_iter - 1) {
				for (i = 0; i < num_deq; i++) {
					struct rte_comp_op *op = deq_ops[i];

					if (op->status !=
						RTE_COMP_OP_STATUS_SUCCESS) {
						RTE_LOG(ERR, USER1,
				       "Some operations were not successful\n");
						goto end;
					}

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

			if (iter == num_iter - 1) {
				for (i = 0; i < num_deq; i++) {
					struct rte_comp_op *op = deq_ops[i];

					if (op->status !=
						RTE_COMP_OP_STATUS_SUCCESS) {
						RTE_LOG(ERR, USER1,
				       "Some operations were not successful\n");
						goto end;
					}

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

	tsc_end = rte_rdtsc_precise();
	tsc_duration = tsc_end - tsc_start;

	if (type == RTE_COMP_COMPRESS)
		ctx->comp_tsc_duration[test_data->level] =
				tsc_duration / num_iter;
	else
		ctx->decomp_tsc_duration[test_data->level] =
				tsc_duration / num_iter;

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
cperf_throughput_test_runner(void *test_ctx)
{
	struct cperf_benchmark_ctx *ctx = test_ctx;
	struct comp_test_data *test_data = ctx->ver.options;
	uint32_t lcore = rte_lcore_id();
	static uint16_t display_once;
	int i, ret = EXIT_SUCCESS;

	ctx->ver.mem.lcore_id = lcore;

	uint16_t exp = 0;
	/*
	 * printing information about current compression thread
	 */
	if (__atomic_compare_exchange_n(&ctx->ver.mem.print_info_once, &exp,
				1, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
		printf("    lcore: %u,"
				" driver name: %s,"
				" device name: %s,"
				" device id: %u,"
				" socket id: %u,"
				" queue pair id: %u\n",
			lcore,
			ctx->ver.options->driver_name,
			rte_compressdev_name_get(ctx->ver.mem.dev_id),
			ctx->ver.mem.dev_id,
			rte_compressdev_socket_id(ctx->ver.mem.dev_id),
			ctx->ver.mem.qp_id);

	/*
	 * First the verification part is needed
	 */
	if (cperf_verify_test_runner(&ctx->ver)) {
		ret = EXIT_FAILURE;
		goto end;
	}

	if (test_data->test_op & COMPRESS) {
		/*
		 * Run the test twice, discarding the first performance
		 * results, before the cache is warmed up
		 */
		for (i = 0; i < 2; i++) {
			if (main_loop(ctx, RTE_COMP_COMPRESS) < 0) {
				ret = EXIT_FAILURE;
				goto end;
			}
		}

		ctx->comp_tsc_byte =
			(double)(ctx->comp_tsc_duration[test_data->level]) /
						       test_data->input_data_sz;
		ctx->comp_gbps = rte_get_tsc_hz() / ctx->comp_tsc_byte * 8 /
								     1000000000;
	} else {
		ctx->comp_tsc_byte = 0;
		ctx->comp_gbps = 0;
	}

	if (test_data->test_op & DECOMPRESS) {
		/*
		 * Run the test twice, discarding the first performance
		 * results, before the cache is warmed up
		 */
		for (i = 0; i < 2; i++) {
			if (main_loop(ctx, RTE_COMP_DECOMPRESS) < 0) {
				ret = EXIT_FAILURE;
				goto end;
			}
		}

		ctx->decomp_tsc_byte =
			(double)(ctx->decomp_tsc_duration[test_data->level]) /
						       test_data->input_data_sz;
		ctx->decomp_gbps = rte_get_tsc_hz() / ctx->decomp_tsc_byte * 8 /
								     1000000000;
	} else {
		ctx->decomp_tsc_byte = 0;
		ctx->decomp_gbps = 0;
	}

	exp = 0;
	if (__atomic_compare_exchange_n(&display_once, &exp, 1, 0,
			__ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
		printf("\n%12s%6s%12s%17s%15s%16s\n",
			"lcore id", "Level", "Comp size", "Comp ratio [%]",
			"Comp [Gbps]", "Decomp [Gbps]");
	}

	printf("%12u%6u%12zu%17.2f%15.2f%16.2f\n",
		ctx->ver.mem.lcore_id,
		test_data->level, ctx->ver.comp_data_sz, ctx->ver.ratio,
		ctx->comp_gbps,
		ctx->decomp_gbps);

end:
	return ret;
}
