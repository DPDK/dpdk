/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_hash_crc.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_mldev.h>

#include "ml_common.h"
#include "test_inference_common.h"

#define ML_OPEN_WRITE_GET_ERR(name, buffer, size, err) \
	do { \
		FILE *fp = fopen(name, "w+"); \
		if (fp == NULL) { \
			ml_err("Unable to create file: %s, error: %s", name, strerror(errno)); \
			err = true; \
		} else { \
			if (fwrite(buffer, 1, size, fp) != size) { \
				ml_err("Error writing output, file: %s, error: %s", name, \
				       strerror(errno)); \
				err = true; \
			} \
			fclose(fp); \
		} \
	} while (0)

/* Enqueue inference requests with burst size equal to 1 */
static int
ml_enqueue_single(void *arg)
{
	struct test_inference *t = ml_test_priv((struct ml_test *)arg);
	struct ml_request *req = NULL;
	struct rte_ml_op *op = NULL;
	struct ml_core_args *args;
	uint64_t model_enq = 0;
	uint64_t start_cycle;
	uint32_t burst_enq;
	uint32_t lcore_id;
	uint64_t offset;
	uint64_t bufsz;
	uint16_t fid;
	uint32_t i;
	int ret;

	lcore_id = rte_lcore_id();
	args = &t->args[lcore_id];
	args->start_cycles = 0;
	model_enq = 0;

	if (args->nb_reqs == 0)
		return 0;

next_rep:
	fid = args->start_fid;

next_model:
	ret = rte_mempool_get(t->op_pool, (void **)&op);
	if (ret != 0)
		goto next_model;

retry_req:
	ret = rte_mempool_get(t->model[fid].io_pool, (void **)&req);
	if (ret != 0)
		goto retry_req;

retry_inp_segs:
	ret = rte_mempool_get_bulk(t->buf_seg_pool, (void **)req->inp_buf_segs,
				   t->model[fid].info.nb_inputs);
	if (ret != 0)
		goto retry_inp_segs;

retry_out_segs:
	ret = rte_mempool_get_bulk(t->buf_seg_pool, (void **)req->out_buf_segs,
				   t->model[fid].info.nb_outputs);
	if (ret != 0)
		goto retry_out_segs;

	op->model_id = t->model[fid].id;
	op->nb_batches = t->model[fid].info.min_batches;
	op->mempool = t->op_pool;
	op->input = req->inp_buf_segs;
	op->output = req->out_buf_segs;
	op->user_ptr = req;

	if (t->model[fid].info.io_layout == RTE_ML_IO_LAYOUT_PACKED) {
		op->input[0]->addr = req->input;
		op->input[0]->iova_addr = rte_mem_virt2iova(req->input);
		op->input[0]->length = t->model[fid].inp_qsize;
		op->input[0]->next = NULL;

		op->output[0]->addr = req->output;
		op->output[0]->iova_addr = rte_mem_virt2iova(req->output);
		op->output[0]->length = t->model[fid].out_qsize;
		op->output[0]->next = NULL;
	} else {
		offset = 0;
		for (i = 0; i < t->model[fid].info.nb_inputs; i++) {
			bufsz = RTE_ALIGN_CEIL(t->model[fid].info.input_info[i].size,
					       t->cmn.dev_info.align_size);
			op->input[i]->addr = req->input + offset;
			op->input[i]->iova_addr = rte_mem_virt2iova(req->input + offset);
			op->input[i]->length = bufsz;
			op->input[i]->next = NULL;
			offset += bufsz;
		}

		offset = 0;
		for (i = 0; i < t->model[fid].info.nb_outputs; i++) {
			bufsz = RTE_ALIGN_CEIL(t->model[fid].info.output_info[i].size,
					       t->cmn.dev_info.align_size);
			op->output[i]->addr = req->output + offset;
			op->output[i]->iova_addr = rte_mem_virt2iova(req->output + offset);
			op->output[i]->length = bufsz;
			op->output[i]->next = NULL;
			offset += bufsz;
		}
	}

	req->niters++;
	req->fid = fid;

enqueue_req:
	start_cycle = rte_get_tsc_cycles();
	burst_enq = rte_ml_enqueue_burst(t->cmn.opt->dev_id, args->qp_id, &op, 1);
	if (burst_enq == 0)
		goto enqueue_req;

	args->start_cycles += start_cycle;
	fid++;
	if (likely(fid <= args->end_fid))
		goto next_model;

	model_enq++;
	if (likely(model_enq < args->nb_reqs))
		goto next_rep;

	return 0;
}

/* Dequeue inference requests with burst size equal to 1 */
static int
ml_dequeue_single(void *arg)
{
	struct test_inference *t = ml_test_priv((struct ml_test *)arg);
	struct rte_ml_op_error error;
	struct rte_ml_op *op = NULL;
	struct ml_core_args *args;
	struct ml_request *req;
	uint64_t total_deq = 0;
	uint8_t nb_filelist;
	uint32_t burst_deq;
	uint64_t end_cycle;
	uint32_t lcore_id;

	lcore_id = rte_lcore_id();
	args = &t->args[lcore_id];
	args->end_cycles = 0;
	nb_filelist = args->end_fid - args->start_fid + 1;

	if (args->nb_reqs == 0)
		return 0;

dequeue_req:
	burst_deq = rte_ml_dequeue_burst(t->cmn.opt->dev_id, args->qp_id, &op, 1);
	end_cycle = rte_get_tsc_cycles();

	if (likely(burst_deq == 1)) {
		total_deq += burst_deq;
		args->end_cycles += end_cycle;
		if (unlikely(op->status == RTE_ML_OP_STATUS_ERROR)) {
			rte_ml_op_error_get(t->cmn.opt->dev_id, op, &error);
			ml_err("error_code = 0x%" PRIx64 ", error_message = %s\n", error.errcode,
			       error.message);
			t->error_count[lcore_id]++;
		}
		req = (struct ml_request *)op->user_ptr;
		rte_mempool_put(t->model[req->fid].io_pool, req);
		rte_mempool_put_bulk(t->buf_seg_pool, (void **)op->input,
				     t->model[req->fid].info.nb_inputs);
		rte_mempool_put_bulk(t->buf_seg_pool, (void **)op->output,
				     t->model[req->fid].info.nb_outputs);
		rte_mempool_put(t->op_pool, op);
	}

	if (likely(total_deq < args->nb_reqs * nb_filelist))
		goto dequeue_req;

	return 0;
}

/* Enqueue inference requests with burst size greater than 1 */
static int
ml_enqueue_burst(void *arg)
{
	struct test_inference *t = ml_test_priv((struct ml_test *)arg);
	struct ml_core_args *args;
	uint64_t start_cycle;
	uint16_t ops_count;
	uint64_t model_enq;
	uint16_t burst_enq;
	uint32_t lcore_id;
	uint16_t pending;
	uint64_t offset;
	uint64_t bufsz;
	uint16_t idx;
	uint16_t fid;
	uint16_t i;
	uint16_t j;
	int ret;

	lcore_id = rte_lcore_id();
	args = &t->args[lcore_id];
	args->start_cycles = 0;
	model_enq = 0;

	if (args->nb_reqs == 0)
		return 0;

next_rep:
	fid = args->start_fid;

next_model:
	ops_count = RTE_MIN(t->cmn.opt->burst_size, args->nb_reqs - model_enq);
	ret = rte_mempool_get_bulk(t->op_pool, (void **)args->enq_ops, ops_count);
	if (ret != 0)
		goto next_model;

retry_reqs:
	ret = rte_mempool_get_bulk(t->model[fid].io_pool, (void **)args->reqs, ops_count);
	if (ret != 0)
		goto retry_reqs;

	for (i = 0; i < ops_count; i++) {
retry_inp_segs:
		ret = rte_mempool_get_bulk(t->buf_seg_pool, (void **)args->reqs[i]->inp_buf_segs,
					   t->model[fid].info.nb_inputs);
		if (ret != 0)
			goto retry_inp_segs;

retry_out_segs:
		ret = rte_mempool_get_bulk(t->buf_seg_pool, (void **)args->reqs[i]->out_buf_segs,
					   t->model[fid].info.nb_outputs);
		if (ret != 0)
			goto retry_out_segs;

		args->enq_ops[i]->model_id = t->model[fid].id;
		args->enq_ops[i]->nb_batches = t->model[fid].info.min_batches;
		args->enq_ops[i]->mempool = t->op_pool;
		args->enq_ops[i]->input = args->reqs[i]->inp_buf_segs;
		args->enq_ops[i]->output = args->reqs[i]->out_buf_segs;
		args->enq_ops[i]->user_ptr = args->reqs[i];

		if (t->model[fid].info.io_layout == RTE_ML_IO_LAYOUT_PACKED) {
			args->enq_ops[i]->input[0]->addr = args->reqs[i]->input;
			args->enq_ops[i]->input[0]->iova_addr =
				rte_mem_virt2iova(args->reqs[i]->input);
			args->enq_ops[i]->input[0]->length = t->model[fid].inp_qsize;
			args->enq_ops[i]->input[0]->next = NULL;

			args->enq_ops[i]->output[0]->addr = args->reqs[i]->output;
			args->enq_ops[i]->output[0]->iova_addr =
				rte_mem_virt2iova(args->reqs[i]->output);
			args->enq_ops[i]->output[0]->length = t->model[fid].out_qsize;
			args->enq_ops[i]->output[0]->next = NULL;
		} else {
			offset = 0;
			for (j = 0; j < t->model[fid].info.nb_inputs; j++) {
				bufsz = RTE_ALIGN_CEIL(t->model[fid].info.input_info[i].size,
						       t->cmn.dev_info.align_size);

				args->enq_ops[i]->input[j]->addr = args->reqs[i]->input + offset;
				args->enq_ops[i]->input[j]->iova_addr =
					rte_mem_virt2iova(args->reqs[i]->input + offset);
				args->enq_ops[i]->input[j]->length = t->model[fid].inp_qsize;
				args->enq_ops[i]->input[j]->next = NULL;
				offset += bufsz;
			}

			offset = 0;
			for (j = 0; j < t->model[fid].info.nb_outputs; j++) {
				bufsz = RTE_ALIGN_CEIL(t->model[fid].info.output_info[i].size,
						       t->cmn.dev_info.align_size);
				args->enq_ops[i]->output[j]->addr = args->reqs[i]->output + offset;
				args->enq_ops[i]->output[j]->iova_addr =
					rte_mem_virt2iova(args->reqs[i]->output + offset);
				args->enq_ops[i]->output[j]->length = t->model[fid].out_qsize;
				args->enq_ops[i]->output[j]->next = NULL;
				offset += bufsz;
			}
		}

		args->reqs[i]->niters++;
		args->reqs[i]->fid = fid;
	}

	idx = 0;
	pending = ops_count;

enqueue_reqs:
	start_cycle = rte_get_tsc_cycles();
	burst_enq =
		rte_ml_enqueue_burst(t->cmn.opt->dev_id, args->qp_id, &args->enq_ops[idx], pending);
	args->start_cycles += burst_enq * start_cycle;
	pending = pending - burst_enq;

	if (pending > 0) {
		idx = idx + burst_enq;
		goto enqueue_reqs;
	}

	fid++;
	if (fid <= args->end_fid)
		goto next_model;

	model_enq = model_enq + ops_count;
	if (model_enq < args->nb_reqs)
		goto next_rep;

	return 0;
}

/* Dequeue inference requests with burst size greater than 1 */
static int
ml_dequeue_burst(void *arg)
{
	struct test_inference *t = ml_test_priv((struct ml_test *)arg);
	struct rte_ml_op_error error;
	struct ml_core_args *args;
	struct ml_request *req;
	uint64_t total_deq = 0;
	uint16_t burst_deq = 0;
	uint8_t nb_filelist;
	uint64_t end_cycle;
	uint32_t lcore_id;
	uint32_t i;

	lcore_id = rte_lcore_id();
	args = &t->args[lcore_id];
	args->end_cycles = 0;
	nb_filelist = args->end_fid - args->start_fid + 1;

	if (args->nb_reqs == 0)
		return 0;

dequeue_burst:
	burst_deq = rte_ml_dequeue_burst(t->cmn.opt->dev_id, args->qp_id, args->deq_ops,
					 t->cmn.opt->burst_size);
	end_cycle = rte_get_tsc_cycles();

	if (likely(burst_deq > 0)) {
		total_deq += burst_deq;
		args->end_cycles += burst_deq * end_cycle;

		for (i = 0; i < burst_deq; i++) {
			if (unlikely(args->deq_ops[i]->status == RTE_ML_OP_STATUS_ERROR)) {
				rte_ml_op_error_get(t->cmn.opt->dev_id, args->deq_ops[i], &error);
				ml_err("error_code = 0x%" PRIx64 ", error_message = %s\n",
				       error.errcode, error.message);
				t->error_count[lcore_id]++;
			}
			req = (struct ml_request *)args->deq_ops[i]->user_ptr;
			if (req != NULL) {
				rte_mempool_put(t->model[req->fid].io_pool, req);
				rte_mempool_put_bulk(t->buf_seg_pool,
						     (void **)args->deq_ops[i]->input,
						     t->model[req->fid].info.nb_inputs);
				rte_mempool_put_bulk(t->buf_seg_pool,
						     (void **)args->deq_ops[i]->output,
						     t->model[req->fid].info.nb_outputs);
			}
		}
		rte_mempool_put_bulk(t->op_pool, (void *)args->deq_ops, burst_deq);
	}

	if (total_deq < args->nb_reqs * nb_filelist)
		goto dequeue_burst;

	return 0;
}

bool
test_inference_cap_check(struct ml_options *opt)
{
	struct rte_ml_dev_info dev_info;

	if (!ml_test_cap_check(opt))
		return false;

	rte_ml_dev_info_get(opt->dev_id, &dev_info);

	if (opt->queue_pairs > dev_info.max_queue_pairs) {
		ml_err("Insufficient capabilities: queue_pairs = %u > (max_queue_pairs = %u)",
		       opt->queue_pairs, dev_info.max_queue_pairs);
		return false;
	}

	if (opt->queue_size > dev_info.max_desc) {
		ml_err("Insufficient capabilities: queue_size = %u > (max_desc = %u)",
		       opt->queue_size, dev_info.max_desc);
		return false;
	}

	if (opt->nb_filelist > dev_info.max_models) {
		ml_err("Insufficient capabilities:  Filelist count exceeded device limit, count = %u > (max limit = %u)",
		       opt->nb_filelist, dev_info.max_models);
		return false;
	}

	if (dev_info.max_io < ML_TEST_MAX_IO_SIZE) {
		ml_err("Insufficient capabilities:  Max I/O, count = %u > (max limit = %u)",
		       ML_TEST_MAX_IO_SIZE, dev_info.max_io);
		return false;
	}

	return true;
}

int
test_inference_opt_check(struct ml_options *opt)
{
	uint32_t i;
	int ret;

	/* check common opts */
	ret = ml_test_opt_check(opt);
	if (ret != 0)
		return ret;

	/* check for at least one filelist */
	if (opt->nb_filelist == 0) {
		ml_err("Filelist empty, need at least one filelist to run the test\n");
		return -EINVAL;
	}

	/* check file availability */
	for (i = 0; i < opt->nb_filelist; i++) {
		if (access(opt->filelist[i].model, F_OK) == -1) {
			ml_err("Model file not accessible: id = %u, file = %s", i,
			       opt->filelist[i].model);
			return -ENOENT;
		}

		if (access(opt->filelist[i].input, F_OK) == -1) {
			ml_err("Input file not accessible: id = %u, file = %s", i,
			       opt->filelist[i].input);
			return -ENOENT;
		}
	}

	if (opt->repetitions == 0) {
		ml_err("Invalid option, repetitions = %" PRIu64 "\n", opt->repetitions);
		return -EINVAL;
	}

	if (opt->burst_size == 0) {
		ml_err("Invalid option, burst_size = %u\n", opt->burst_size);
		return -EINVAL;
	}

	if (opt->burst_size > ML_TEST_MAX_POOL_SIZE) {
		ml_err("Invalid option, burst_size = %u (> max supported = %d)\n", opt->burst_size,
		       ML_TEST_MAX_POOL_SIZE);
		return -EINVAL;
	}

	if (opt->queue_pairs == 0) {
		ml_err("Invalid option, queue_pairs = %u\n", opt->queue_pairs);
		return -EINVAL;
	}

	if (opt->queue_size == 0) {
		ml_err("Invalid option, queue_size = %u\n", opt->queue_size);
		return -EINVAL;
	}

	/* check number of available lcores. */
	if (rte_lcore_count() < (uint32_t)(opt->queue_pairs * 2 + 1)) {
		ml_err("Insufficient lcores = %u\n", rte_lcore_count());
		ml_err("Minimum lcores required to create %u queue-pairs = %u\n", opt->queue_pairs,
		       (opt->queue_pairs * 2 + 1));
		return -EINVAL;
	}

	return 0;
}

void
test_inference_opt_dump(struct ml_options *opt)
{
	uint32_t i;

	/* dump common opts */
	ml_test_opt_dump(opt);

	/* dump test opts */
	ml_dump("repetitions", "%" PRIu64, opt->repetitions);
	ml_dump("burst_size", "%u", opt->burst_size);
	ml_dump("queue_pairs", "%u", opt->queue_pairs);
	ml_dump("queue_size", "%u", opt->queue_size);
	ml_dump("tolerance", "%-7.3f", opt->tolerance);
	ml_dump("stats", "%s", (opt->stats ? "true" : "false"));

	ml_dump_begin("filelist");
	for (i = 0; i < opt->nb_filelist; i++) {
		ml_dump_list("model", i, opt->filelist[i].model);
		ml_dump_list("input", i, opt->filelist[i].input);
		ml_dump_list("output", i, opt->filelist[i].output);
		if (strcmp(opt->filelist[i].reference, "\0") != 0)
			ml_dump_list("reference", i, opt->filelist[i].reference);
	}
	ml_dump_end;
}

int
test_inference_setup(struct ml_test *test, struct ml_options *opt)
{
	struct test_inference *t;
	void *test_inference;
	uint32_t lcore_id;
	int ret = 0;
	uint32_t i;

	test_inference = rte_zmalloc_socket(test->name, sizeof(struct test_inference),
					    RTE_CACHE_LINE_SIZE, opt->socket_id);
	if (test_inference == NULL) {
		ml_err("failed to allocate memory for test_model");
		ret = -ENOMEM;
		goto error;
	}
	test->test_priv = test_inference;
	t = ml_test_priv(test);

	t->nb_used = 0;
	t->nb_valid = 0;
	t->cmn.result = ML_TEST_FAILED;
	t->cmn.opt = opt;
	memset(t->error_count, 0, RTE_MAX_LCORE * sizeof(uint64_t));

	/* get device info */
	ret = rte_ml_dev_info_get(opt->dev_id, &t->cmn.dev_info);
	if (ret < 0) {
		ml_err("failed to get device info");
		goto error;
	}

	if (opt->burst_size == 1) {
		t->enqueue = ml_enqueue_single;
		t->dequeue = ml_dequeue_single;
	} else {
		t->enqueue = ml_enqueue_burst;
		t->dequeue = ml_dequeue_burst;
	}

	/* set model initial state */
	for (i = 0; i < opt->nb_filelist; i++)
		t->model[i].state = MODEL_INITIAL;

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		t->args[lcore_id].enq_ops = rte_zmalloc_socket(
			"ml_test_enq_ops", opt->burst_size * sizeof(struct rte_ml_op *),
			RTE_CACHE_LINE_SIZE, opt->socket_id);
		t->args[lcore_id].deq_ops = rte_zmalloc_socket(
			"ml_test_deq_ops", opt->burst_size * sizeof(struct rte_ml_op *),
			RTE_CACHE_LINE_SIZE, opt->socket_id);
		t->args[lcore_id].reqs = rte_zmalloc_socket(
			"ml_test_requests", opt->burst_size * sizeof(struct ml_request *),
			RTE_CACHE_LINE_SIZE, opt->socket_id);
	}

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		t->args[i].start_cycles = 0;
		t->args[i].end_cycles = 0;
	}

	return 0;

error:
	rte_free(test_inference);

	return ret;
}

void
test_inference_destroy(struct ml_test *test, struct ml_options *opt)
{
	struct test_inference *t;
	uint32_t lcore_id;

	RTE_SET_USED(opt);

	t = ml_test_priv(test);

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		rte_free(t->args[lcore_id].enq_ops);
		rte_free(t->args[lcore_id].deq_ops);
		rte_free(t->args[lcore_id].reqs);
	}

	rte_free(t);
}

int
ml_inference_mldev_setup(struct ml_test *test, struct ml_options *opt)
{
	struct rte_ml_dev_qp_conf qp_conf;
	struct test_inference *t;
	uint16_t qp_id;
	int ret;

	t = ml_test_priv(test);

	RTE_SET_USED(t);

	ret = ml_test_device_configure(test, opt);
	if (ret != 0)
		return ret;

	/* setup queue pairs */
	qp_conf.nb_desc = opt->queue_size;
	qp_conf.cb = NULL;

	for (qp_id = 0; qp_id < opt->queue_pairs; qp_id++) {
		qp_conf.nb_desc = opt->queue_size;
		qp_conf.cb = NULL;

		ret = rte_ml_dev_queue_pair_setup(opt->dev_id, qp_id, &qp_conf, opt->socket_id);
		if (ret != 0) {
			ml_err("Failed to setup ml device queue-pair, dev_id = %d, qp_id = %u\n",
			       opt->dev_id, qp_id);
			return ret;
		}
	}

	ret = ml_test_device_start(test, opt);
	if (ret != 0)
		goto error;

	return 0;

error:
	ml_test_device_close(test, opt);

	return ret;
}

int
ml_inference_mldev_destroy(struct ml_test *test, struct ml_options *opt)
{
	int ret;

	ret = ml_test_device_stop(test, opt);
	if (ret != 0)
		goto error;

	ret = ml_test_device_close(test, opt);
	if (ret != 0)
		return ret;

	return 0;

error:
	ml_test_device_close(test, opt);

	return ret;
}

/* Callback for IO pool create. This function would compute the fields of ml_request
 * structure and prepare the quantized input data.
 */
static void
ml_request_initialize(struct rte_mempool *mp, void *opaque, void *obj, unsigned int obj_idx)
{
	struct test_inference *t = ml_test_priv((struct ml_test *)opaque);
	struct ml_request *req = (struct ml_request *)obj;
	struct rte_ml_buff_seg dbuff_seg[ML_TEST_MAX_IO_SIZE];
	struct rte_ml_buff_seg qbuff_seg[ML_TEST_MAX_IO_SIZE];
	struct rte_ml_buff_seg *q_segs[ML_TEST_MAX_IO_SIZE];
	struct rte_ml_buff_seg *d_segs[ML_TEST_MAX_IO_SIZE];
	uint64_t offset;
	uint64_t bufsz;
	uint32_t i;

	RTE_SET_USED(mp);
	RTE_SET_USED(obj_idx);

	req->input = (uint8_t *)obj +
		     RTE_ALIGN_CEIL(sizeof(struct ml_request), t->cmn.dev_info.align_size);
	req->output =
		req->input + RTE_ALIGN_CEIL(t->model[t->fid].inp_qsize, t->cmn.dev_info.align_size);
	req->niters = 0;

	if (t->model[t->fid].info.io_layout == RTE_ML_IO_LAYOUT_PACKED) {
		dbuff_seg[0].addr = t->model[t->fid].input;
		dbuff_seg[0].iova_addr = rte_mem_virt2iova(t->model[t->fid].input);
		dbuff_seg[0].length = t->model[t->fid].inp_dsize;
		dbuff_seg[0].next = NULL;
		d_segs[0] = &dbuff_seg[0];

		qbuff_seg[0].addr = req->input;
		qbuff_seg[0].iova_addr = rte_mem_virt2iova(req->input);
		qbuff_seg[0].length = t->model[t->fid].inp_qsize;
		qbuff_seg[0].next = NULL;
		q_segs[0] = &qbuff_seg[0];
	} else {
		offset = 0;
		for (i = 0; i < t->model[t->fid].info.nb_inputs; i++) {
			bufsz = t->model[t->fid].info.input_info[i].nb_elements * sizeof(float);
			dbuff_seg[i].addr = t->model[t->fid].input + offset;
			dbuff_seg[i].iova_addr = rte_mem_virt2iova(t->model[t->fid].input + offset);
			dbuff_seg[i].length = bufsz;
			dbuff_seg[i].next = NULL;
			d_segs[i] = &dbuff_seg[i];
			offset += bufsz;
		}

		offset = 0;
		for (i = 0; i < t->model[t->fid].info.nb_inputs; i++) {
			bufsz = RTE_ALIGN_CEIL(t->model[t->fid].info.input_info[i].size,
					       t->cmn.dev_info.align_size);
			qbuff_seg[i].addr = req->input + offset;
			qbuff_seg[i].iova_addr = rte_mem_virt2iova(req->input + offset);
			qbuff_seg[i].length = bufsz;
			qbuff_seg[i].next = NULL;
			q_segs[i] = &qbuff_seg[i];
			offset += bufsz;
		}
	}

	/* quantize data */
	rte_ml_io_quantize(t->cmn.opt->dev_id, t->model[t->fid].id, d_segs, q_segs);
}

int
ml_inference_iomem_setup(struct ml_test *test, struct ml_options *opt, uint16_t fid)
{
	struct test_inference *t = ml_test_priv(test);
	char mz_name[RTE_MEMZONE_NAMESIZE];
	char mp_name[RTE_MEMPOOL_NAMESIZE];
	const struct rte_memzone *mz;
	uint64_t nb_buffers;
	char *buffer = NULL;
	uint32_t buff_size;
	uint32_t mz_size;
	size_t fsize;
	uint32_t i;
	int ret;

	/* get input buffer size */
	t->model[fid].inp_qsize = 0;
	for (i = 0; i < t->model[fid].info.nb_inputs; i++) {
		if (t->model[fid].info.io_layout == RTE_ML_IO_LAYOUT_PACKED)
			t->model[fid].inp_qsize += t->model[fid].info.input_info[i].size;
		else
			t->model[fid].inp_qsize += RTE_ALIGN_CEIL(
				t->model[fid].info.input_info[i].size, t->cmn.dev_info.align_size);
	}

	/* get output buffer size */
	t->model[fid].out_qsize = 0;
	for (i = 0; i < t->model[fid].info.nb_outputs; i++) {
		if (t->model[fid].info.io_layout == RTE_ML_IO_LAYOUT_PACKED)
			t->model[fid].out_qsize += t->model[fid].info.output_info[i].size;
		else
			t->model[fid].out_qsize += RTE_ALIGN_CEIL(
				t->model[fid].info.output_info[i].size, t->cmn.dev_info.align_size);
	}

	t->model[fid].inp_dsize = 0;
	for (i = 0; i < t->model[fid].info.nb_inputs; i++) {
		if (opt->quantized_io)
			t->model[fid].inp_dsize += t->model[fid].info.input_info[i].size;
		else
			t->model[fid].inp_dsize +=
				t->model[fid].info.input_info[i].nb_elements * sizeof(float);
	}

	t->model[fid].out_dsize = 0;
	for (i = 0; i < t->model[fid].info.nb_outputs; i++) {
		if (opt->quantized_io)
			t->model[fid].out_dsize += t->model[fid].info.output_info[i].size;
		else
			t->model[fid].out_dsize +=
				t->model[fid].info.output_info[i].nb_elements * sizeof(float);
	}

	/* allocate buffer for user data */
	mz_size = t->model[fid].inp_dsize + t->model[fid].out_dsize;
	if (strcmp(opt->filelist[fid].reference, "\0") != 0)
		mz_size += t->model[fid].out_dsize;

	sprintf(mz_name, "ml_user_data_%d", fid);
	mz = rte_memzone_reserve(mz_name, mz_size, opt->socket_id, 0);
	if (mz == NULL) {
		ml_err("Memzone allocation failed for ml_user_data\n");
		ret = -ENOMEM;
		goto error;
	}

	t->model[fid].input = mz->addr;
	t->model[fid].output = t->model[fid].input + t->model[fid].inp_dsize;
	if (strcmp(opt->filelist[fid].reference, "\0") != 0)
		t->model[fid].reference = t->model[fid].output + t->model[fid].out_dsize;
	else
		t->model[fid].reference = NULL;

	/* load input file */
	ret = ml_read_file(opt->filelist[fid].input, &fsize, &buffer);
	if (ret != 0)
		goto error;

	if (fsize == t->model[fid].inp_dsize) {
		rte_memcpy(t->model[fid].input, buffer, fsize);
		free(buffer);
	} else {
		ml_err("Invalid input file, size = %zu (expected size = %" PRIu64 ")\n", fsize,
		       t->model[fid].inp_dsize);
		ret = -EINVAL;
		free(buffer);
		goto error;
	}

	/* load reference file */
	buffer = NULL;
	if (t->model[fid].reference != NULL) {
		ret = ml_read_file(opt->filelist[fid].reference, &fsize, &buffer);
		if (ret != 0)
			goto error;

		if (fsize == t->model[fid].out_dsize) {
			rte_memcpy(t->model[fid].reference, buffer, fsize);
			free(buffer);
		} else {
			ml_err("Invalid reference file, size = %zu (expected size = %" PRIu64 ")\n",
			       fsize, t->model[fid].out_dsize);
			ret = -EINVAL;
			free(buffer);
			goto error;
		}
	}

	/* create mempool for quantized input and output buffers. ml_request_initialize is
	 * used as a callback for object creation.
	 */
	buff_size = RTE_ALIGN_CEIL(sizeof(struct ml_request), t->cmn.dev_info.align_size) +
		    RTE_ALIGN_CEIL(t->model[fid].inp_qsize, t->cmn.dev_info.align_size) +
		    RTE_ALIGN_CEIL(t->model[fid].out_qsize, t->cmn.dev_info.align_size);
	nb_buffers = RTE_MIN((uint64_t)ML_TEST_MAX_POOL_SIZE, opt->repetitions);

	t->fid = fid;
	sprintf(mp_name, "ml_io_pool_%d", fid);
	t->model[fid].io_pool = rte_mempool_create(mp_name, nb_buffers, buff_size, 0, 0, NULL, NULL,
						   ml_request_initialize, test, opt->socket_id, 0);
	if (t->model[fid].io_pool == NULL) {
		ml_err("Failed to create io pool : %s\n", "ml_io_pool");
		ret = -ENOMEM;
		goto error;
	}

	return 0;

error:
	rte_memzone_free(mz);

	if (t->model[fid].io_pool != NULL) {
		rte_mempool_free(t->model[fid].io_pool);
		t->model[fid].io_pool = NULL;
	}

	return ret;
}

void
ml_inference_iomem_destroy(struct ml_test *test, struct ml_options *opt, uint16_t fid)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	char mp_name[RTE_MEMPOOL_NAMESIZE];
	const struct rte_memzone *mz;
	struct rte_mempool *mp;

	RTE_SET_USED(test);
	RTE_SET_USED(opt);

	/* release user data memzone */
	sprintf(mz_name, "ml_user_data_%d", fid);
	mz = rte_memzone_lookup(mz_name);
	rte_memzone_free(mz);

	/* destroy io pool */
	sprintf(mp_name, "ml_io_pool_%d", fid);
	mp = rte_mempool_lookup(mp_name);
	rte_mempool_free(mp);
}

int
ml_inference_mem_setup(struct ml_test *test, struct ml_options *opt)
{
	struct test_inference *t = ml_test_priv(test);

	/* create op pool */
	t->op_pool = rte_ml_op_pool_create("ml_test_op_pool", ML_TEST_MAX_POOL_SIZE, 0, 0,
					   opt->socket_id);
	if (t->op_pool == NULL) {
		ml_err("Failed to create op pool : %s\n", "ml_op_pool");
		return -ENOMEM;
	}

	/* create buf_segs pool of with element of uint8_t. external buffers are attached to the
	 * buf_segs while queuing inference requests.
	 */
	t->buf_seg_pool = rte_mempool_create("ml_test_mbuf_pool", ML_TEST_MAX_POOL_SIZE * 2,
					     sizeof(struct rte_ml_buff_seg), 0, 0, NULL, NULL, NULL,
					     NULL, opt->socket_id, 0);
	if (t->buf_seg_pool == NULL) {
		ml_err("Failed to create buf_segs pool : %s\n", "ml_test_mbuf_pool");
		rte_ml_op_pool_free(t->op_pool);
		return -ENOMEM;
	}

	return 0;
}

void
ml_inference_mem_destroy(struct ml_test *test, struct ml_options *opt)
{
	struct test_inference *t = ml_test_priv(test);

	RTE_SET_USED(opt);

	/* release op pool */
	rte_mempool_free(t->op_pool);

	/* release buf_segs pool */
	rte_mempool_free(t->buf_seg_pool);
}

static bool
ml_inference_validation(struct ml_test *test, struct ml_request *req)
{
	struct test_inference *t = ml_test_priv((struct ml_test *)test);
	struct ml_model *model;
	float *reference;
	float *output;
	float deviation;
	bool match;
	uint32_t i;
	uint32_t j;

	model = &t->model[req->fid];

	/* compare crc when tolerance is 0 */
	if (t->cmn.opt->tolerance == 0.0) {
		match = (rte_hash_crc(model->output, model->out_dsize, 0) ==
			 rte_hash_crc(model->reference, model->out_dsize, 0));
	} else {
		output = (float *)model->output;
		reference = (float *)model->reference;

		i = 0;
next_output:
		j = 0;
next_element:
		match = false;
		if ((*reference == 0) && (*output == 0))
			deviation = 0;
		else
			deviation = 100 * fabs(*output - *reference) / fabs(*reference);
		if (deviation <= t->cmn.opt->tolerance)
			match = true;
		else
			ml_err("id = %d, element = %d, output = %f, reference = %f, deviation = %f %%\n",
			       i, j, *output, *reference, deviation);

		output++;
		reference++;

		if (!match)
			goto done;

		j++;
		if (j < model->info.output_info[i].nb_elements)
			goto next_element;

		i++;
		if (i < model->info.nb_outputs)
			goto next_output;
	}
done:
	return match;
}

/* Callback for mempool object iteration. This call would dequantize output data. */
static void
ml_request_finish(struct rte_mempool *mp, void *opaque, void *obj, unsigned int obj_idx)
{
	struct test_inference *t = ml_test_priv((struct ml_test *)opaque);
	struct ml_request *req = (struct ml_request *)obj;
	struct ml_model *model = &t->model[req->fid];
	bool error = false;
	char *dump_path;

	struct rte_ml_buff_seg qbuff_seg[ML_TEST_MAX_IO_SIZE];
	struct rte_ml_buff_seg dbuff_seg[ML_TEST_MAX_IO_SIZE];
	struct rte_ml_buff_seg *q_segs[ML_TEST_MAX_IO_SIZE];
	struct rte_ml_buff_seg *d_segs[ML_TEST_MAX_IO_SIZE];
	uint64_t offset;
	uint64_t bufsz;
	uint32_t i;

	RTE_SET_USED(mp);

	if (req->niters == 0)
		return;

	t->nb_used++;

	if (t->model[req->fid].info.io_layout == RTE_ML_IO_LAYOUT_PACKED) {
		qbuff_seg[0].addr = req->output;
		qbuff_seg[0].iova_addr = rte_mem_virt2iova(req->output);
		qbuff_seg[0].length = t->model[req->fid].out_qsize;
		qbuff_seg[0].next = NULL;
		q_segs[0] = &qbuff_seg[0];

		dbuff_seg[0].addr = model->output;
		dbuff_seg[0].iova_addr = rte_mem_virt2iova(model->output);
		dbuff_seg[0].length = t->model[req->fid].out_dsize;
		dbuff_seg[0].next = NULL;
		d_segs[0] = &dbuff_seg[0];
	} else {
		offset = 0;
		for (i = 0; i < t->model[req->fid].info.nb_outputs; i++) {
			bufsz = RTE_ALIGN_CEIL(t->model[req->fid].info.output_info[i].size,
					       t->cmn.dev_info.align_size);
			qbuff_seg[i].addr = req->output + offset;
			qbuff_seg[i].iova_addr = rte_mem_virt2iova(req->output + offset);
			qbuff_seg[i].length = bufsz;
			qbuff_seg[i].next = NULL;
			q_segs[i] = &qbuff_seg[i];
			offset += bufsz;
		}

		offset = 0;
		for (i = 0; i < t->model[req->fid].info.nb_outputs; i++) {
			bufsz = t->model[req->fid].info.output_info[i].nb_elements * sizeof(float);
			dbuff_seg[i].addr = model->output + offset;
			dbuff_seg[i].iova_addr = rte_mem_virt2iova(model->output + offset);
			dbuff_seg[i].length = bufsz;
			dbuff_seg[i].next = NULL;
			d_segs[i] = &dbuff_seg[i];
			offset += bufsz;
		}
	}

	rte_ml_io_dequantize(t->cmn.opt->dev_id, model->id, q_segs, d_segs);

	if (model->reference == NULL)
		goto dump_output_pass;

	if (!ml_inference_validation(opaque, req))
		goto dump_output_fail;
	else
		goto dump_output_pass;

dump_output_pass:
	if (obj_idx == 0) {
		/* write quantized output */
		if (asprintf(&dump_path, "%s.q", t->cmn.opt->filelist[req->fid].output) == -1)
			return;
		ML_OPEN_WRITE_GET_ERR(dump_path, req->output, model->out_qsize, error);
		free(dump_path);
		if (error)
			return;

		/* write dequantized output */
		if (asprintf(&dump_path, "%s", t->cmn.opt->filelist[req->fid].output) == -1)
			return;
		ML_OPEN_WRITE_GET_ERR(dump_path, model->output, model->out_dsize, error);
		free(dump_path);
		if (error)
			return;
	}
	t->nb_valid++;

	return;

dump_output_fail:
	if (t->cmn.opt->debug) {
		/* dump quantized output buffer */
		if (asprintf(&dump_path, "%s.q.%u", t->cmn.opt->filelist[req->fid].output,
			     obj_idx) == -1)
			return;
		ML_OPEN_WRITE_GET_ERR(dump_path, req->output, model->out_qsize, error);
		free(dump_path);
		if (error)
			return;

		/* dump dequantized output buffer */
		if (asprintf(&dump_path, "%s.%u", t->cmn.opt->filelist[req->fid].output, obj_idx) ==
		    -1)
			return;
		ML_OPEN_WRITE_GET_ERR(dump_path, model->output, model->out_dsize, error);
		free(dump_path);
		if (error)
			return;
	}
}

int
ml_inference_result(struct ml_test *test, struct ml_options *opt, uint16_t fid)
{
	struct test_inference *t = ml_test_priv(test);
	uint64_t error_count = 0;
	uint32_t i;

	RTE_SET_USED(opt);

	/* check for errors */
	for (i = 0; i < RTE_MAX_LCORE; i++)
		error_count += t->error_count[i];

	rte_mempool_obj_iter(t->model[fid].io_pool, ml_request_finish, test);

	if ((t->nb_used == t->nb_valid) && (error_count == 0))
		t->cmn.result = ML_TEST_SUCCESS;
	else
		t->cmn.result = ML_TEST_FAILED;

	return t->cmn.result;
}

int
ml_inference_launch_cores(struct ml_test *test, struct ml_options *opt, uint16_t start_fid,
			  uint16_t end_fid)
{
	struct test_inference *t = ml_test_priv(test);
	uint32_t lcore_id;
	uint32_t nb_reqs;
	uint32_t id = 0;
	uint32_t qp_id;

	nb_reqs = opt->repetitions / opt->queue_pairs;

	RTE_LCORE_FOREACH_WORKER(lcore_id)
	{
		if (id >= opt->queue_pairs * 2)
			break;

		qp_id = id / 2;
		t->args[lcore_id].qp_id = qp_id;
		t->args[lcore_id].nb_reqs = nb_reqs;
		if (qp_id == 0)
			t->args[lcore_id].nb_reqs += opt->repetitions - nb_reqs * opt->queue_pairs;

		if (t->args[lcore_id].nb_reqs == 0) {
			id++;
			break;
		}

		t->args[lcore_id].start_fid = start_fid;
		t->args[lcore_id].end_fid = end_fid;

		if (id % 2 == 0)
			rte_eal_remote_launch(t->enqueue, test, lcore_id);
		else
			rte_eal_remote_launch(t->dequeue, test, lcore_id);

		id++;
	}

	return 0;
}
