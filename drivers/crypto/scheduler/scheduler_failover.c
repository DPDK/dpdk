/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <cryptodev_pmd.h>
#include <rte_malloc.h>

#include "rte_cryptodev_scheduler_operations.h"
#include "scheduler_pmd_private.h"

#define PRIMARY_WORKER_IDX	0
#define SECONDARY_WORKER_IDX	1
#define NB_FAILOVER_WORKERS	2
#define WORKER_SWITCH_MASK	(0x01)

struct fo_scheduler_qp_ctx {
	struct scheduler_worker primary_worker;
	struct scheduler_worker secondary_worker;
	uint8_t primary_worker_index;
	uint8_t secondary_worker_index;

	uint8_t deq_idx;
};

static __rte_always_inline uint16_t
failover_worker_enqueue(struct scheduler_worker *worker,
		struct rte_crypto_op **ops, uint16_t nb_ops, uint8_t index)
{
	uint16_t processed_ops;

	scheduler_set_worker_sessions(ops, nb_ops, index);

	processed_ops = rte_cryptodev_enqueue_burst(worker->dev_id,
			worker->qp_id, ops, nb_ops);
	worker->nb_inflight_cops += processed_ops;

	return processed_ops;
}

static uint16_t
schedule_enqueue(void *qp, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct fo_scheduler_qp_ctx *qp_ctx =
			((struct scheduler_qp_ctx *)qp)->private_qp_ctx;
	uint16_t enqueued_ops;

	if (unlikely(nb_ops == 0))
		return 0;

	enqueued_ops = failover_worker_enqueue(&qp_ctx->primary_worker,
			ops, nb_ops, PRIMARY_WORKER_IDX);

	if (enqueued_ops < nb_ops) {
		scheduler_retrieve_sessions(&ops[enqueued_ops],
						nb_ops - enqueued_ops);
		enqueued_ops += failover_worker_enqueue(
				&qp_ctx->secondary_worker,
				&ops[enqueued_ops],
				nb_ops - enqueued_ops,
				SECONDARY_WORKER_IDX);
		if (enqueued_ops < nb_ops)
			scheduler_retrieve_sessions(&ops[enqueued_ops],
						nb_ops - enqueued_ops);
	}

	return enqueued_ops;
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
	struct fo_scheduler_qp_ctx *qp_ctx =
			((struct scheduler_qp_ctx *)qp)->private_qp_ctx;
	struct scheduler_worker *workers[NB_FAILOVER_WORKERS] = {
			&qp_ctx->primary_worker, &qp_ctx->secondary_worker};
	struct scheduler_worker *worker = workers[qp_ctx->deq_idx];
	uint16_t nb_deq_ops = 0, nb_deq_ops2 = 0;

	if (worker->nb_inflight_cops) {
		nb_deq_ops = rte_cryptodev_dequeue_burst(worker->dev_id,
			worker->qp_id, ops, nb_ops);
		worker->nb_inflight_cops -= nb_deq_ops;
	}

	qp_ctx->deq_idx = (~qp_ctx->deq_idx) & WORKER_SWITCH_MASK;

	if (nb_deq_ops == nb_ops)
		goto retrieve_sessions;

	worker = workers[qp_ctx->deq_idx];

	if (worker->nb_inflight_cops) {
		nb_deq_ops2 = rte_cryptodev_dequeue_burst(worker->dev_id,
			worker->qp_id, &ops[nb_deq_ops], nb_ops - nb_deq_ops);
		worker->nb_inflight_cops -= nb_deq_ops2;
	}

retrieve_sessions:
	scheduler_retrieve_sessions(ops, nb_deq_ops + nb_deq_ops2);

	return nb_deq_ops + nb_deq_ops2;
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

	if (sched_ctx->nb_workers < 2) {
		CR_SCHED_LOG(ERR, "Number of workers shall no less than 2");
		return -ENOMEM;
	}

	if (sched_ctx->reordering_enabled) {
		dev->enqueue_burst = schedule_enqueue_ordering;
		dev->dequeue_burst = schedule_dequeue_ordering;
	} else {
		dev->enqueue_burst = schedule_enqueue;
		dev->dequeue_burst = schedule_dequeue;
	}

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		struct fo_scheduler_qp_ctx *qp_ctx =
			((struct scheduler_qp_ctx *)
				dev->data->queue_pairs[i])->private_qp_ctx;

		sched_ctx->workers[PRIMARY_WORKER_IDX].qp_id = i;
		sched_ctx->workers[SECONDARY_WORKER_IDX].qp_id = i;

		rte_memcpy(&qp_ctx->primary_worker,
				&sched_ctx->workers[PRIMARY_WORKER_IDX],
				sizeof(struct scheduler_worker));
		rte_memcpy(&qp_ctx->secondary_worker,
				&sched_ctx->workers[SECONDARY_WORKER_IDX],
				sizeof(struct scheduler_worker));
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
	struct fo_scheduler_qp_ctx *fo_qp_ctx;

	fo_qp_ctx = rte_zmalloc_socket(NULL, sizeof(*fo_qp_ctx), 0,
			rte_socket_id());
	if (!fo_qp_ctx) {
		CR_SCHED_LOG(ERR, "failed allocate memory for private queue pair");
		return -ENOMEM;
	}

	qp_ctx->private_qp_ctx = (void *)fo_qp_ctx;

	return 0;
}

static int
scheduler_create_private_ctx(__rte_unused struct rte_cryptodev *dev)
{
	return 0;
}

static struct rte_cryptodev_scheduler_ops scheduler_fo_ops = {
	worker_attach,
	worker_detach,
	scheduler_start,
	scheduler_stop,
	scheduler_config_qp,
	scheduler_create_private_ctx,
	NULL,	/* option_set */
	NULL	/*option_get */
};

static struct rte_cryptodev_scheduler fo_scheduler = {
		.name = "failover-scheduler",
		.description = "scheduler which enqueues to the primary worker, "
				"and only then enqueues to the secondary worker "
				"upon failing on enqueuing to primary",
		.mode = CDEV_SCHED_MODE_FAILOVER,
		.ops = &scheduler_fo_ops
};

struct rte_cryptodev_scheduler *crypto_scheduler_failover = &fo_scheduler;
