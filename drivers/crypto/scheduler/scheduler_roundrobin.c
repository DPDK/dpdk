/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <rte_cryptodev.h>
#include <rte_malloc.h>

#include "rte_cryptodev_scheduler_operations.h"
#include "scheduler_pmd_private.h"

struct rr_scheduler_qp_ctx {
	struct scheduler_worker workers[RTE_CRYPTODEV_SCHEDULER_MAX_NB_WORKERS];
	uint32_t nb_workers;

	uint32_t last_enq_worker_idx;
	uint32_t last_deq_worker_idx;
};

static uint16_t
schedule_enqueue(void *qp, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct rr_scheduler_qp_ctx *rr_qp_ctx =
			((struct scheduler_qp_ctx *)qp)->private_qp_ctx;
	uint32_t worker_idx = rr_qp_ctx->last_enq_worker_idx;
	struct scheduler_worker *worker = &rr_qp_ctx->workers[worker_idx];
	uint16_t i, processed_ops;

	if (unlikely(nb_ops == 0))
		return 0;

	for (i = 0; i < nb_ops && i < 4; i++)
		rte_prefetch0(ops[i]->sym->session);

	processed_ops = rte_cryptodev_enqueue_burst(worker->dev_id,
			worker->qp_id, ops, nb_ops);

	worker->nb_inflight_cops += processed_ops;

	rr_qp_ctx->last_enq_worker_idx += 1;
	rr_qp_ctx->last_enq_worker_idx %= rr_qp_ctx->nb_workers;

	return processed_ops;
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
	struct rr_scheduler_qp_ctx *rr_qp_ctx =
			((struct scheduler_qp_ctx *)qp)->private_qp_ctx;
	struct scheduler_worker *worker;
	uint32_t last_worker_idx = rr_qp_ctx->last_deq_worker_idx;
	uint16_t nb_deq_ops;

	if (unlikely(rr_qp_ctx->workers[last_worker_idx].nb_inflight_cops
			== 0)) {
		do {
			last_worker_idx += 1;

			if (unlikely(last_worker_idx >= rr_qp_ctx->nb_workers))
				last_worker_idx = 0;
			/* looped back, means no inflight cops in the queue */
			if (last_worker_idx == rr_qp_ctx->last_deq_worker_idx)
				return 0;
		} while (rr_qp_ctx->workers[last_worker_idx].nb_inflight_cops
				== 0);
	}

	worker = &rr_qp_ctx->workers[last_worker_idx];

	nb_deq_ops = rte_cryptodev_dequeue_burst(worker->dev_id,
			worker->qp_id, ops, nb_ops);

	last_worker_idx += 1;
	last_worker_idx %= rr_qp_ctx->nb_workers;

	rr_qp_ctx->last_deq_worker_idx = last_worker_idx;

	worker->nb_inflight_cops -= nb_deq_ops;

	return nb_deq_ops;
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
	uint16_t i;

	if (sched_ctx->reordering_enabled) {
		dev->enqueue_burst = &schedule_enqueue_ordering;
		dev->dequeue_burst = &schedule_dequeue_ordering;
	} else {
		dev->enqueue_burst = &schedule_enqueue;
		dev->dequeue_burst = &schedule_dequeue;
	}

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		struct scheduler_qp_ctx *qp_ctx = dev->data->queue_pairs[i];
		struct rr_scheduler_qp_ctx *rr_qp_ctx =
				qp_ctx->private_qp_ctx;
		uint32_t j;

		memset(rr_qp_ctx->workers, 0,
				RTE_CRYPTODEV_SCHEDULER_MAX_NB_WORKERS *
				sizeof(struct scheduler_worker));
		for (j = 0; j < sched_ctx->nb_workers; j++) {
			rr_qp_ctx->workers[j].dev_id =
					sched_ctx->workers[j].dev_id;
			rr_qp_ctx->workers[j].qp_id = i;
		}

		rr_qp_ctx->nb_workers = sched_ctx->nb_workers;

		rr_qp_ctx->last_enq_worker_idx = 0;
		rr_qp_ctx->last_deq_worker_idx = 0;
	}

	return 0;
}

static int
scheduler_stop(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

static int
scheduler_config_qp(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct scheduler_qp_ctx *qp_ctx = dev->data->queue_pairs[qp_id];
	struct rr_scheduler_qp_ctx *rr_qp_ctx;

	rr_qp_ctx = rte_zmalloc_socket(NULL, sizeof(*rr_qp_ctx), 0,
			rte_socket_id());
	if (!rr_qp_ctx) {
		CR_SCHED_LOG(ERR, "failed allocate memory for private queue pair");
		return -ENOMEM;
	}

	qp_ctx->private_qp_ctx = (void *)rr_qp_ctx;

	return 0;
}

static int
scheduler_create_private_ctx(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

static struct rte_cryptodev_scheduler_ops scheduler_rr_ops = {
	worker_attach,
	worker_detach,
	scheduler_start,
	scheduler_stop,
	scheduler_config_qp,
	scheduler_create_private_ctx,
	NULL,	/* option_set */
	NULL	/* option_get */
};

static struct rte_cryptodev_scheduler scheduler = {
		.name = "roundrobin-scheduler",
		.description = "scheduler which will round robin burst across "
				"worker crypto devices",
		.mode = CDEV_SCHED_MODE_ROUNDROBIN,
		.ops = &scheduler_rr_ops
};

struct rte_cryptodev_scheduler *crypto_scheduler_roundrobin = &scheduler;
