/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <cryptodev_pmd.h>
#include <rte_malloc.h>

#include "rte_cryptodev_scheduler_operations.h"
#include "scheduler_pmd_private.h"

#define DEF_PKT_SIZE_THRESHOLD			(0xffffff80)
#define WORKER_IDX_SWITCH_MASK			(0x01)
#define PRIMARY_WORKER_IDX			0
#define SECONDARY_WORKER_IDX			1
#define NB_PKT_SIZE_WORKERS			2

/** pkt size based scheduler context */
struct psd_scheduler_ctx {
	uint32_t threshold;
};

/** pkt size based scheduler queue pair context */
struct psd_scheduler_qp_ctx {
	struct scheduler_worker primary_worker;
	struct scheduler_worker secondary_worker;
	uint32_t threshold;
	uint8_t deq_idx;
} __rte_cache_aligned;

/** scheduling operation variables' wrapping */
struct psd_schedule_op {
	uint8_t worker_idx;
	uint16_t pos;
};

static uint16_t
schedule_enqueue(void *qp, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct scheduler_qp_ctx *qp_ctx = qp;
	struct psd_scheduler_qp_ctx *psd_qp_ctx = qp_ctx->private_qp_ctx;
	struct rte_crypto_op *sched_ops[NB_PKT_SIZE_WORKERS][nb_ops];
	uint32_t in_flight_ops[NB_PKT_SIZE_WORKERS] = {
			psd_qp_ctx->primary_worker.nb_inflight_cops,
			psd_qp_ctx->secondary_worker.nb_inflight_cops
	};
	struct psd_schedule_op enq_ops[NB_PKT_SIZE_WORKERS] = {
		{PRIMARY_WORKER_IDX, 0}, {SECONDARY_WORKER_IDX, 0}
	};
	struct psd_schedule_op *p_enq_op;
	uint16_t i, processed_ops_pri = 0, processed_ops_sec = 0;
	uint32_t job_len;

	if (unlikely(nb_ops == 0))
		return 0;

	for (i = 0; i < nb_ops && i < 4; i++) {
		rte_prefetch0(ops[i]->sym);
		rte_prefetch0(ops[i]->sym->session);
	}

	for (i = 0; (i < (nb_ops - 8)) && (nb_ops > 8); i += 4) {
		rte_prefetch0(ops[i + 4]->sym);
		rte_prefetch0(ops[i + 4]->sym->session);
		rte_prefetch0(ops[i + 5]->sym);
		rte_prefetch0(ops[i + 5]->sym->session);
		rte_prefetch0(ops[i + 6]->sym);
		rte_prefetch0(ops[i + 6]->sym->session);
		rte_prefetch0(ops[i + 7]->sym);
		rte_prefetch0(ops[i + 7]->sym->session);

		/* job_len is initialized as cipher data length, once
		 * it is 0, equals to auth data length
		 */
		job_len = ops[i]->sym->cipher.data.length;
		job_len += (ops[i]->sym->cipher.data.length == 0) *
				ops[i]->sym->auth.data.length;
		/* decide the target op based on the job length */
		p_enq_op = &enq_ops[!(job_len & psd_qp_ctx->threshold)];

		/* stop schedule cops before the queue is full, this shall
		 * prevent the failed enqueue
		 */
		if (p_enq_op->pos + in_flight_ops[p_enq_op->worker_idx] ==
				qp_ctx->max_nb_objs) {
			i = nb_ops;
			break;
		}

		sched_ops[p_enq_op->worker_idx][p_enq_op->pos] = ops[i];
		p_enq_op->pos++;

		job_len = ops[i+1]->sym->cipher.data.length;
		job_len += (ops[i+1]->sym->cipher.data.length == 0) *
				ops[i+1]->sym->auth.data.length;
		p_enq_op = &enq_ops[!(job_len & psd_qp_ctx->threshold)];

		if (p_enq_op->pos + in_flight_ops[p_enq_op->worker_idx] ==
				qp_ctx->max_nb_objs) {
			i = nb_ops;
			break;
		}

		sched_ops[p_enq_op->worker_idx][p_enq_op->pos] = ops[i+1];
		p_enq_op->pos++;

		job_len = ops[i+2]->sym->cipher.data.length;
		job_len += (ops[i+2]->sym->cipher.data.length == 0) *
				ops[i+2]->sym->auth.data.length;
		p_enq_op = &enq_ops[!(job_len & psd_qp_ctx->threshold)];

		if (p_enq_op->pos + in_flight_ops[p_enq_op->worker_idx] ==
				qp_ctx->max_nb_objs) {
			i = nb_ops;
			break;
		}

		sched_ops[p_enq_op->worker_idx][p_enq_op->pos] = ops[i+2];
		p_enq_op->pos++;

		job_len = ops[i+3]->sym->cipher.data.length;
		job_len += (ops[i+3]->sym->cipher.data.length == 0) *
				ops[i+3]->sym->auth.data.length;
		p_enq_op = &enq_ops[!(job_len & psd_qp_ctx->threshold)];

		if (p_enq_op->pos + in_flight_ops[p_enq_op->worker_idx] ==
				qp_ctx->max_nb_objs) {
			i = nb_ops;
			break;
		}

		sched_ops[p_enq_op->worker_idx][p_enq_op->pos] = ops[i+3];
		p_enq_op->pos++;
	}

	for (; i < nb_ops; i++) {
		job_len = ops[i]->sym->cipher.data.length;
		job_len += (ops[i]->sym->cipher.data.length == 0) *
				ops[i]->sym->auth.data.length;
		p_enq_op = &enq_ops[!(job_len & psd_qp_ctx->threshold)];

		if (p_enq_op->pos + in_flight_ops[p_enq_op->worker_idx] ==
				qp_ctx->max_nb_objs) {
			i = nb_ops;
			break;
		}

		sched_ops[p_enq_op->worker_idx][p_enq_op->pos] = ops[i];
		p_enq_op->pos++;
	}

	processed_ops_pri = rte_cryptodev_enqueue_burst(
			psd_qp_ctx->primary_worker.dev_id,
			psd_qp_ctx->primary_worker.qp_id,
			sched_ops[PRIMARY_WORKER_IDX],
			enq_ops[PRIMARY_WORKER_IDX].pos);
	/* enqueue shall not fail as the worker queue is monitored */
	RTE_ASSERT(processed_ops_pri == enq_ops[PRIMARY_WORKER_IDX].pos);

	psd_qp_ctx->primary_worker.nb_inflight_cops += processed_ops_pri;

	processed_ops_sec = rte_cryptodev_enqueue_burst(
			psd_qp_ctx->secondary_worker.dev_id,
			psd_qp_ctx->secondary_worker.qp_id,
			sched_ops[SECONDARY_WORKER_IDX],
			enq_ops[SECONDARY_WORKER_IDX].pos);
	RTE_ASSERT(processed_ops_sec == enq_ops[SECONDARY_WORKER_IDX].pos);

	psd_qp_ctx->secondary_worker.nb_inflight_cops += processed_ops_sec;

	return processed_ops_pri + processed_ops_sec;
}

static uint16_t
schedule_enqueue_ordering(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct rte_ring *order_ring =
			((struct scheduler_qp_ctx *)qp)->order_ring;
	uint16_t nb_ops_to_enq = get_max_enqueue_order_count(order_ring,
			nb_ops);
	uint16_t nb_ops_enqd = schedule_enqueue(qp, ops,
			nb_ops_to_enq);

	scheduler_order_insert(order_ring, ops, nb_ops_enqd);

	return nb_ops_enqd;
}

static uint16_t
schedule_dequeue(void *qp, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct psd_scheduler_qp_ctx *qp_ctx =
			((struct scheduler_qp_ctx *)qp)->private_qp_ctx;
	struct scheduler_worker *workers[NB_PKT_SIZE_WORKERS] = {
			&qp_ctx->primary_worker, &qp_ctx->secondary_worker};
	struct scheduler_worker *worker = workers[qp_ctx->deq_idx];
	uint16_t nb_deq_ops_pri = 0, nb_deq_ops_sec = 0;

	if (worker->nb_inflight_cops) {
		nb_deq_ops_pri = rte_cryptodev_dequeue_burst(worker->dev_id,
			worker->qp_id, ops, nb_ops);
		worker->nb_inflight_cops -= nb_deq_ops_pri;
	}

	qp_ctx->deq_idx = (~qp_ctx->deq_idx) & WORKER_IDX_SWITCH_MASK;

	if (nb_deq_ops_pri == nb_ops)
		return nb_deq_ops_pri;

	worker = workers[qp_ctx->deq_idx];

	if (worker->nb_inflight_cops) {
		nb_deq_ops_sec = rte_cryptodev_dequeue_burst(worker->dev_id,
				worker->qp_id, &ops[nb_deq_ops_pri],
				nb_ops - nb_deq_ops_pri);
		worker->nb_inflight_cops -= nb_deq_ops_sec;

		if (!worker->nb_inflight_cops)
			qp_ctx->deq_idx = (~qp_ctx->deq_idx) &
					WORKER_IDX_SWITCH_MASK;
	}

	return nb_deq_ops_pri + nb_deq_ops_sec;
}

static uint16_t
schedule_dequeue_ordering(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct rte_ring *order_ring =
			((struct scheduler_qp_ctx *)qp)->order_ring;

	schedule_dequeue(qp, ops, nb_ops);

	return scheduler_order_drain(order_ring, ops, nb_ops);
}

static int
worker_attach(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint8_t worker_id)
{
	return 0;
}

static int
worker_detach(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint8_t worker_id)
{
	return 0;
}

static int
scheduler_start(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	struct psd_scheduler_ctx *psd_ctx = sched_ctx->private_ctx;
	uint16_t i;

	/* for packet size based scheduler, nb_workers have to >= 2 */
	if (sched_ctx->nb_workers < NB_PKT_SIZE_WORKERS) {
		CR_SCHED_LOG(ERR, "not enough workers to start");
		return -1;
	}

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		struct scheduler_qp_ctx *qp_ctx = dev->data->queue_pairs[i];
		struct psd_scheduler_qp_ctx *ps_qp_ctx =
				qp_ctx->private_qp_ctx;

		ps_qp_ctx->primary_worker.dev_id =
				sched_ctx->workers[PRIMARY_WORKER_IDX].dev_id;
		ps_qp_ctx->primary_worker.qp_id = i;
		ps_qp_ctx->primary_worker.nb_inflight_cops = 0;

		ps_qp_ctx->secondary_worker.dev_id =
				sched_ctx->workers[SECONDARY_WORKER_IDX].dev_id;
		ps_qp_ctx->secondary_worker.qp_id = i;
		ps_qp_ctx->secondary_worker.nb_inflight_cops = 0;

		ps_qp_ctx->threshold = psd_ctx->threshold;
	}

	if (sched_ctx->reordering_enabled) {
		dev->enqueue_burst = &schedule_enqueue_ordering;
		dev->dequeue_burst = &schedule_dequeue_ordering;
	} else {
		dev->enqueue_burst = &schedule_enqueue;
		dev->dequeue_burst = &schedule_dequeue;
	}

	return 0;
}

static int
scheduler_stop(struct rte_cryptodev *dev)
{
	uint16_t i;

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		struct scheduler_qp_ctx *qp_ctx = dev->data->queue_pairs[i];
		struct psd_scheduler_qp_ctx *ps_qp_ctx = qp_ctx->private_qp_ctx;

		if (ps_qp_ctx->primary_worker.nb_inflight_cops +
				ps_qp_ctx->secondary_worker.nb_inflight_cops) {
			CR_SCHED_LOG(ERR, "Some crypto ops left in worker queue");
			return -1;
		}
	}

	return 0;
}

static int
scheduler_config_qp(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct scheduler_qp_ctx *qp_ctx = dev->data->queue_pairs[qp_id];
	struct psd_scheduler_qp_ctx *ps_qp_ctx;

	ps_qp_ctx = rte_zmalloc_socket(NULL, sizeof(*ps_qp_ctx), 0,
			rte_socket_id());
	if (!ps_qp_ctx) {
		CR_SCHED_LOG(ERR, "failed allocate memory for private queue pair");
		return -ENOMEM;
	}

	qp_ctx->private_qp_ctx = (void *)ps_qp_ctx;

	return 0;
}

static int
scheduler_create_private_ctx(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	struct psd_scheduler_ctx *psd_ctx;

	if (sched_ctx->private_ctx) {
		rte_free(sched_ctx->private_ctx);
		sched_ctx->private_ctx = NULL;
	}

	psd_ctx = rte_zmalloc_socket(NULL, sizeof(struct psd_scheduler_ctx), 0,
			rte_socket_id());
	if (!psd_ctx) {
		CR_SCHED_LOG(ERR, "failed allocate memory");
		return -ENOMEM;
	}

	psd_ctx->threshold = DEF_PKT_SIZE_THRESHOLD;

	sched_ctx->private_ctx = (void *)psd_ctx;

	return 0;
}
static int
scheduler_option_set(struct rte_cryptodev *dev, uint32_t option_type,
		void *option)
{
	struct psd_scheduler_ctx *psd_ctx = ((struct scheduler_ctx *)
			dev->data->dev_private)->private_ctx;
	uint32_t threshold;

	if ((enum rte_cryptodev_schedule_option_type)option_type !=
			CDEV_SCHED_OPTION_THRESHOLD) {
		CR_SCHED_LOG(ERR, "Option not supported");
		return -EINVAL;
	}

	threshold = ((struct rte_cryptodev_scheduler_threshold_option *)
			option)->threshold;
	if (!rte_is_power_of_2(threshold)) {
		CR_SCHED_LOG(ERR, "Threshold is not power of 2");
		return -EINVAL;
	}

	psd_ctx->threshold = ~(threshold - 1);

	return 0;
}

static int
scheduler_option_get(struct rte_cryptodev *dev, uint32_t option_type,
		void *option)
{
	struct psd_scheduler_ctx *psd_ctx = ((struct scheduler_ctx *)
			dev->data->dev_private)->private_ctx;
	struct rte_cryptodev_scheduler_threshold_option *threshold_option;

	if ((enum rte_cryptodev_schedule_option_type)option_type !=
			CDEV_SCHED_OPTION_THRESHOLD) {
		CR_SCHED_LOG(ERR, "Option not supported");
		return -EINVAL;
	}

	threshold_option = option;
	threshold_option->threshold = (~psd_ctx->threshold) + 1;

	return 0;
}

static struct rte_cryptodev_scheduler_ops scheduler_ps_ops = {
	worker_attach,
	worker_detach,
	scheduler_start,
	scheduler_stop,
	scheduler_config_qp,
	scheduler_create_private_ctx,
	scheduler_option_set,
	scheduler_option_get
};

static struct rte_cryptodev_scheduler psd_scheduler = {
		.name = "packet-size-based-scheduler",
		.description = "scheduler which will distribute crypto op "
				"burst based on the packet size",
		.mode = CDEV_SCHED_MODE_PKT_SIZE_DISTR,
		.ops = &scheduler_ps_ops
};

struct rte_cryptodev_scheduler *crypto_scheduler_pkt_size_based_distr = &psd_scheduler;
