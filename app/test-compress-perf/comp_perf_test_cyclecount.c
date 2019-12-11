/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include "rte_spinlock.h"
#include <rte_compressdev.h>

#include "comp_perf_test_cyclecount.h"

struct cperf_cyclecount_ctx {
	struct cperf_verify_ctx ver;

	uint32_t ops_enq_retries;
	uint32_t ops_deq_retries;

	uint64_t duration_op;
	uint64_t duration_enq;
	uint64_t duration_deq;
};

void
cperf_cyclecount_test_destructor(void *arg)
{
	struct cperf_cyclecount_ctx *ctx = arg;

	if (arg) {
		comp_perf_free_memory(ctx->ver.options, &ctx->ver.mem);
		rte_free(arg);
	}
}

void *
cperf_cyclecount_test_constructor(uint8_t dev_id, uint16_t qp_id,
		struct comp_test_data *options)
{
	struct cperf_cyclecount_ctx *ctx = NULL;

	ctx = rte_malloc(NULL, sizeof(struct cperf_cyclecount_ctx), 0);

	if (ctx == NULL)
		return NULL;

	ctx->ver.mem.dev_id = dev_id;
	ctx->ver.mem.qp_id = qp_id;
	ctx->ver.options = options;
	ctx->ver.silent = 1; /* ver. part will be silent */

	if (!comp_perf_allocate_memory(ctx->ver.options, &ctx->ver.mem)
			&& !prepare_bufs(ctx->ver.options, &ctx->ver.mem))
		return ctx;

	cperf_cyclecount_test_destructor(ctx);
	return NULL;
}

static int
cperf_cyclecount_op_setup(struct rte_comp_op **ops,
				 struct cperf_cyclecount_ctx *ctx,
				 struct rte_mbuf **input_bufs,
				 struct rte_mbuf **output_bufs,
				 void *priv_xform,
				 uint32_t out_seg_sz)
{
	struct comp_test_data *test_data = ctx->ver.options;
	struct cperf_mem_resources *mem = &ctx->ver.mem;

	uint32_t i, iter, num_iter;
	int res = 0;
	uint16_t ops_needed;

	num_iter = test_data->num_iter;

	for (iter = 0; iter < num_iter; iter++) {
		uint32_t remaining_ops = mem->total_bufs;
		uint32_t total_deq_ops = 0;
		uint32_t total_enq_ops = 0;
		uint16_t num_enq = 0;
		uint16_t num_deq = 0;

		while (remaining_ops > 0) {
			uint16_t num_ops = RTE_MIN(remaining_ops,
						   test_data->burst_sz);
			ops_needed = num_ops;

			/* Allocate compression operations */
			if (ops_needed && rte_mempool_get_bulk(
						mem->op_pool,
						(void **)ops,
						ops_needed) != 0) {
				RTE_LOG(ERR, USER1,
				      "Cyclecount: could not allocate enough operations\n");
				res = -1;
				goto end;
			}

			for (i = 0; i < ops_needed; i++) {

				/* Calculate next buffer to attach */
				/* to operation */
				uint32_t buf_id = total_enq_ops + i;
				uint16_t op_id = i;

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

			/* E N Q U E U I N G */
			/* assuming that all ops are enqueued */
			/* instead of the real enqueue operation */
			num_enq = num_ops;

			remaining_ops -= num_enq;
			total_enq_ops += num_enq;

			/* D E Q U E U I N G */
			/* assuming that all ops dequeued */
			/* instead of the real dequeue operation */
			num_deq = num_ops;

			total_deq_ops += num_deq;
			rte_mempool_put_bulk(mem->op_pool,
					     (void **)ops, num_deq);
		}
	}
	return res;
end:
	rte_mempool_put_bulk(mem->op_pool, (void **)ops, ops_needed);
	rte_free(ops);

	return res;
}

static int
main_loop(struct cperf_cyclecount_ctx *ctx, enum rte_comp_xform_type type)
{
	struct comp_test_data *test_data = ctx->ver.options;
	struct cperf_mem_resources *mem = &ctx->ver.mem;
	uint8_t dev_id = mem->dev_id;
	uint32_t i, iter, num_iter;
	struct rte_comp_op **ops, **deq_ops;
	void *priv_xform = NULL;
	struct rte_comp_xform xform;
	struct rte_mbuf **input_bufs, **output_bufs;
	int ret, res = 0;
	int allocated = 0;
	uint32_t out_seg_sz;

	uint64_t tsc_start, tsc_end, tsc_duration;

	if (test_data == NULL || !test_data->burst_sz) {
		RTE_LOG(ERR, USER1, "Unknown burst size\n");
		return -1;
	}
	ctx->duration_enq = 0;
	ctx->duration_deq = 0;
	ctx->ops_enq_retries = 0;
	ctx->ops_deq_retries = 0;

	/* one array for both enqueue and dequeue */
	ops = rte_zmalloc_socket(NULL,
		2 * mem->total_bufs * sizeof(struct rte_comp_op *),
		0, rte_socket_id());

	if (ops == NULL) {
		RTE_LOG(ERR, USER1,
			"Can't allocate memory for ops strucures\n");
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

	tsc_start = rte_rdtsc_precise();
	ret = cperf_cyclecount_op_setup(ops,
				ctx,
				input_bufs,
				output_bufs,
				priv_xform,
				out_seg_sz);

	tsc_end = rte_rdtsc_precise();

	/* ret value check postponed a bit to cancel extra 'if' bias */
	if (ret < 0) {
		RTE_LOG(ERR, USER1, "Setup function failed\n");
		res = -1;
		goto end;
	}

	tsc_duration = tsc_end - tsc_start;
	ctx->duration_op = tsc_duration;

	num_iter = test_data->num_iter;
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
			if (ops_needed && rte_mempool_get_bulk(
						mem->op_pool,
						(void **)ops,
						ops_needed) != 0) {
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

			tsc_start = rte_rdtsc_precise();
			num_enq = rte_compressdev_enqueue_burst(dev_id,
								mem->qp_id, ops,
								num_ops);
			tsc_end = rte_rdtsc_precise();
			tsc_duration = tsc_end - tsc_start;
			ctx->duration_enq += tsc_duration;

			if (num_enq < num_ops)
				ctx->ops_enq_retries++;

			if (test_data->cyclecount_delay)
				rte_delay_us_block(test_data->cyclecount_delay);

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

			tsc_start = rte_rdtsc_precise();
			num_deq = rte_compressdev_dequeue_burst(dev_id,
							   mem->qp_id,
							   deq_ops,
							   allocated);
			tsc_end = rte_rdtsc_precise();
			tsc_duration = tsc_end - tsc_start;
			ctx->duration_deq += tsc_duration;

			if (num_deq < allocated)
				ctx->ops_deq_retries++;

			total_deq_ops += num_deq;

			if (iter == num_iter - 1) {
				for (i = 0; i < num_deq; i++) {
					struct rte_comp_op *op = deq_ops[i];

					if (op->status !=
						RTE_COMP_OP_STATUS_SUCCESS) {
						RTE_LOG(ERR, USER1, "Some operations were not successful\n");
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

			tsc_start = rte_rdtsc_precise();
			num_deq = rte_compressdev_dequeue_burst(dev_id,
						mem->qp_id,
						deq_ops,
						test_data->burst_sz);
			tsc_end = rte_rdtsc_precise();
			tsc_duration = tsc_end - tsc_start;
			ctx->duration_deq += tsc_duration;
			ctx->ops_deq_retries++;

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
						RTE_LOG(ERR, USER1, "Some operations were not successful\n");
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
	allocated = 0;

end:
	if (allocated)
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
cperf_cyclecount_test_runner(void *test_ctx)
{
	struct cperf_cyclecount_ctx *ctx = test_ctx;
	struct comp_test_data *test_data = ctx->ver.options;
	uint32_t lcore = rte_lcore_id();
	static rte_atomic16_t display_once = RTE_ATOMIC16_INIT(0);
	static rte_spinlock_t print_spinlock;
	int i;

	uint32_t ops_enq_retries_comp;
	uint32_t ops_deq_retries_comp;

	uint32_t ops_enq_retries_decomp;
	uint32_t ops_deq_retries_decomp;

	uint32_t duration_setup_per_op;

	uint32_t duration_enq_per_op_comp;
	uint32_t duration_deq_per_op_comp;

	uint32_t duration_enq_per_op_decomp;
	uint32_t duration_deq_per_op_decomp;

	ctx->ver.mem.lcore_id = lcore;

	/*
	 * printing information about current compression thread
	 */
	if (rte_atomic16_test_and_set(&ctx->ver.mem.print_info_once))
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
	if (cperf_verify_test_runner(&ctx->ver))
		return EXIT_FAILURE;

	/*
	 * Run the tests twice, discarding the first performance
	 * results, before the cache is warmed up
	 */

	/* C O M P R E S S */
	for (i = 0; i < 2; i++) {
		if (main_loop(ctx, RTE_COMP_COMPRESS) < 0)
			return EXIT_FAILURE;
	}

	ops_enq_retries_comp = ctx->ops_enq_retries;
	ops_deq_retries_comp = ctx->ops_deq_retries;

	duration_enq_per_op_comp = ctx->duration_enq /
			(ctx->ver.mem.total_bufs * test_data->num_iter);
	duration_deq_per_op_comp = ctx->duration_deq /
			(ctx->ver.mem.total_bufs * test_data->num_iter);

	/* D E C O M P R E S S */
	for (i = 0; i < 2; i++) {
		if (main_loop(ctx, RTE_COMP_DECOMPRESS) < 0)
			return EXIT_FAILURE;
	}

	ops_enq_retries_decomp = ctx->ops_enq_retries;
	ops_deq_retries_decomp = ctx->ops_deq_retries;

	duration_enq_per_op_decomp = ctx->duration_enq /
			(ctx->ver.mem.total_bufs * test_data->num_iter);
	duration_deq_per_op_decomp = ctx->duration_deq /
			(ctx->ver.mem.total_bufs * test_data->num_iter);

	duration_setup_per_op = ctx->duration_op /
			(ctx->ver.mem.total_bufs * test_data->num_iter);

	/* R E P O R T processing */
	if (rte_atomic16_test_and_set(&display_once)) {

		rte_spinlock_lock(&print_spinlock);

		printf("\nLegend for the table\n"
		"  - Retries section: number of retries for the following operations:\n"
		"    [C-e] - compression enqueue\n"
		"    [C-d] - compression dequeue\n"
		"    [D-e] - decompression enqueue\n"
		"    [D-d] - decompression dequeue\n"
		"  - Cycles section: number of cycles per 'op' for the following operations:\n"
		"    setup/op - memory allocation, op configuration and memory dealocation\n"
		"    [C-e] - compression enqueue\n"
		"    [C-d] - compression dequeue\n"
		"    [D-e] - decompression enqueue\n"
		"    [D-d] - decompression dequeue\n\n");

		printf("\n%12s%6s%12s%17s",
			"lcore id", "Level", "Comp size", "Comp ratio [%]");

		printf("  |%10s %6s %8s %6s %8s",
			" Retries:",
			"[C-e]", "[C-d]",
			"[D-e]", "[D-d]");

		printf("  |%9s %9s %9s %9s %9s %9s\n",
			" Cycles:",
			"setup/op",
			"[C-e]", "[C-d]",
			"[D-e]", "[D-d]");

		rte_spinlock_unlock(&print_spinlock);
	}

	rte_spinlock_lock(&print_spinlock);

	printf("%12u"
	       "%6u"
	       "%12zu"
	       "%17.2f",
		ctx->ver.mem.lcore_id,
		test_data->level,
		ctx->ver.comp_data_sz,
		ctx->ver.ratio);

	printf("  |%10s %6u %8u %6u %8u",
	       " ",
		ops_enq_retries_comp,
		ops_deq_retries_comp,
		ops_enq_retries_decomp,
		ops_deq_retries_decomp);

	printf("  |%9s %9u %9u %9u %9u %9u\n",
	       " ",
		duration_setup_per_op,
		duration_enq_per_op_comp,
		duration_deq_per_op_comp,
		duration_enq_per_op_decomp,
		duration_deq_per_op_decomp);

	rte_spinlock_unlock(&print_spinlock);

	return EXIT_SUCCESS;
}
