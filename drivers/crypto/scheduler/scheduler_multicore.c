/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */
#include <unistd.h>

#include <rte_cryptodev.h>
#include <rte_malloc.h>

#include "rte_cryptodev_scheduler_operations.h"
#include "scheduler_pmd_private.h"

#define MC_SCHED_ENQ_RING_NAME_PREFIX	"MCS_ENQR_"
#define MC_SCHED_DEQ_RING_NAME_PREFIX	"MCS_DEQR_"

#define MC_SCHED_BUFFER_SIZE 32

#define CRYPTO_OP_STATUS_BIT_COMPLETE	0x80

/** multi-core scheduler context */
struct mc_scheduler_ctx {
	uint32_t num_workers;             /**< Number of workers polling */
	uint32_t stop_signal;

	struct rte_ring *sched_enq_ring[RTE_MAX_LCORE];
	struct rte_ring *sched_deq_ring[RTE_MAX_LCORE];
};

struct mc_scheduler_qp_ctx {
	struct scheduler_slave slaves[RTE_CRYPTODEV_SCHEDULER_MAX_NB_SLAVES];
	uint32_t nb_slaves;

	uint32_t last_enq_worker_idx;
	uint32_t last_deq_worker_idx;

	struct mc_scheduler_ctx *mc_private_ctx;
};

static uint16_t
schedule_enqueue(void *qp, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct mc_scheduler_qp_ctx *mc_qp_ctx =
			((struct scheduler_qp_ctx *)qp)->private_qp_ctx;
	struct mc_scheduler_ctx *mc_ctx = mc_qp_ctx->mc_private_ctx;
	uint32_t worker_idx = mc_qp_ctx->last_enq_worker_idx;
	uint16_t i, processed_ops = 0;

	if (unlikely(nb_ops == 0))
		return 0;

	for (i = 0; i <  mc_ctx->num_workers && nb_ops != 0; i++) {
		struct rte_ring *enq_ring = mc_ctx->sched_enq_ring[worker_idx];
		uint16_t nb_queue_ops = rte_ring_enqueue_burst(enq_ring,
			(void *)(&ops[processed_ops]), nb_ops, NULL);

		nb_ops -= nb_queue_ops;
		processed_ops += nb_queue_ops;

		if (++worker_idx == mc_ctx->num_workers)
			worker_idx = 0;
	}
	mc_qp_ctx->last_enq_worker_idx = worker_idx;

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
	struct mc_scheduler_qp_ctx *mc_qp_ctx =
			((struct scheduler_qp_ctx *)qp)->private_qp_ctx;
	struct mc_scheduler_ctx *mc_ctx = mc_qp_ctx->mc_private_ctx;
	uint32_t worker_idx = mc_qp_ctx->last_deq_worker_idx;
	uint16_t i, processed_ops = 0;

	for (i = 0; i < mc_ctx->num_workers && nb_ops != 0; i++) {
		struct rte_ring *deq_ring = mc_ctx->sched_deq_ring[worker_idx];
		uint16_t nb_deq_ops = rte_ring_dequeue_burst(deq_ring,
			(void *)(&ops[processed_ops]), nb_ops, NULL);

		nb_ops -= nb_deq_ops;
		processed_ops += nb_deq_ops;
		if (++worker_idx == mc_ctx->num_workers)
			worker_idx = 0;
	}

	mc_qp_ctx->last_deq_worker_idx = worker_idx;

	return processed_ops;

}

static uint16_t
schedule_dequeue_ordering(void *qp, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct rte_ring *order_ring = ((struct scheduler_qp_ctx *)qp)->order_ring;
	struct rte_crypto_op *op;
	uint32_t nb_objs = rte_ring_count(order_ring);
	uint32_t nb_ops_to_deq = 0;
	uint32_t nb_ops_deqd = 0;

	if (nb_objs > nb_ops)
		nb_objs = nb_ops;

	while (nb_ops_to_deq < nb_objs) {
		SCHEDULER_GET_RING_OBJ(order_ring, nb_ops_to_deq, op);

		if (!(op->status & CRYPTO_OP_STATUS_BIT_COMPLETE))
			break;

		op->status &= ~CRYPTO_OP_STATUS_BIT_COMPLETE;
		nb_ops_to_deq++;
	}

	if (nb_ops_to_deq) {
		nb_ops_deqd = rte_ring_sc_dequeue_bulk(order_ring,
				(void **)ops, nb_ops_to_deq, NULL);
	}

	return nb_ops_deqd;
}

static int
slave_attach(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint8_t slave_id)
{
	return 0;
}

static int
slave_detach(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint8_t slave_id)
{
	return 0;
}

static int
mc_scheduler_worker(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	struct mc_scheduler_ctx *mc_ctx = sched_ctx->private_ctx;
	struct rte_ring *enq_ring;
	struct rte_ring *deq_ring;
	uint32_t core_id = rte_lcore_id();
	int i, worker_idx = -1;
	struct scheduler_slave *slave;
	struct rte_crypto_op *enq_ops[MC_SCHED_BUFFER_SIZE];
	struct rte_crypto_op *deq_ops[MC_SCHED_BUFFER_SIZE];
	uint16_t processed_ops;
	uint16_t pending_enq_ops = 0;
	uint16_t pending_enq_ops_idx = 0;
	uint16_t pending_deq_ops = 0;
	uint16_t pending_deq_ops_idx = 0;
	uint16_t inflight_ops = 0;
	const uint8_t reordering_enabled = sched_ctx->reordering_enabled;

	for (i = 0; i < (int)sched_ctx->nb_wc; i++) {
		if (sched_ctx->wc_pool[i] == core_id) {
			worker_idx = i;
			break;
		}
	}
	if (worker_idx == -1) {
		CR_SCHED_LOG(ERR, "worker on core %u:cannot find worker index!",
			core_id);
		return -1;
	}

	slave = &sched_ctx->slaves[worker_idx];
	enq_ring = mc_ctx->sched_enq_ring[worker_idx];
	deq_ring = mc_ctx->sched_deq_ring[worker_idx];

	while (!mc_ctx->stop_signal) {
		if (pending_enq_ops) {
			processed_ops =
				rte_cryptodev_enqueue_burst(slave->dev_id,
					slave->qp_id, &enq_ops[pending_enq_ops_idx],
					pending_enq_ops);
			pending_enq_ops -= processed_ops;
			pending_enq_ops_idx += processed_ops;
			inflight_ops += processed_ops;
		} else {
			processed_ops = rte_ring_dequeue_burst(enq_ring, (void *)enq_ops,
							MC_SCHED_BUFFER_SIZE, NULL);
			if (processed_ops) {
				pending_enq_ops_idx = rte_cryptodev_enqueue_burst(
							slave->dev_id, slave->qp_id,
							enq_ops, processed_ops);
				pending_enq_ops = processed_ops - pending_enq_ops_idx;
				inflight_ops += pending_enq_ops_idx;
			}
		}

		if (pending_deq_ops) {
			processed_ops = rte_ring_enqueue_burst(
					deq_ring, (void *)&deq_ops[pending_deq_ops_idx],
							pending_deq_ops, NULL);
			pending_deq_ops -= processed_ops;
			pending_deq_ops_idx += processed_ops;
		} else if (inflight_ops) {
			processed_ops = rte_cryptodev_dequeue_burst(slave->dev_id,
					slave->qp_id, deq_ops, MC_SCHED_BUFFER_SIZE);
			if (processed_ops) {
				inflight_ops -= processed_ops;
				if (reordering_enabled) {
					uint16_t j;

					for (j = 0; j < processed_ops; j++) {
						deq_ops[j]->status |=
							CRYPTO_OP_STATUS_BIT_COMPLETE;
					}
				} else {
					pending_deq_ops_idx = rte_ring_enqueue_burst(
						deq_ring, (void *)deq_ops, processed_ops,
						NULL);
					pending_deq_ops = processed_ops -
								pending_deq_ops_idx;
				}
			}
		}

		rte_pause();
	}

	return 0;
}

static int
scheduler_start(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	struct mc_scheduler_ctx *mc_ctx = sched_ctx->private_ctx;
	uint16_t i;

	mc_ctx->stop_signal = 0;

	for (i = 0; i < sched_ctx->nb_wc; i++)
		rte_eal_remote_launch(
			(lcore_function_t *)mc_scheduler_worker, dev,
					sched_ctx->wc_pool[i]);

	if (sched_ctx->reordering_enabled) {
		dev->enqueue_burst = &schedule_enqueue_ordering;
		dev->dequeue_burst = &schedule_dequeue_ordering;
	} else {
		dev->enqueue_burst = &schedule_enqueue;
		dev->dequeue_burst = &schedule_dequeue;
	}

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		struct scheduler_qp_ctx *qp_ctx = dev->data->queue_pairs[i];
		struct mc_scheduler_qp_ctx *mc_qp_ctx =
				qp_ctx->private_qp_ctx;
		uint32_t j;

		memset(mc_qp_ctx->slaves, 0,
				RTE_CRYPTODEV_SCHEDULER_MAX_NB_SLAVES *
				sizeof(struct scheduler_slave));
		for (j = 0; j < sched_ctx->nb_slaves; j++) {
			mc_qp_ctx->slaves[j].dev_id =
					sched_ctx->slaves[j].dev_id;
			mc_qp_ctx->slaves[j].qp_id = i;
		}

		mc_qp_ctx->nb_slaves = sched_ctx->nb_slaves;

		mc_qp_ctx->last_enq_worker_idx = 0;
		mc_qp_ctx->last_deq_worker_idx = 0;
	}

	return 0;
}

static int
scheduler_stop(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	struct mc_scheduler_ctx *mc_ctx = sched_ctx->private_ctx;
	uint16_t i;

	mc_ctx->stop_signal = 1;

	for (i = 0; i < sched_ctx->nb_wc; i++)
		rte_eal_wait_lcore(sched_ctx->wc_pool[i]);

	return 0;
}

static int
scheduler_config_qp(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct scheduler_qp_ctx *qp_ctx = dev->data->queue_pairs[qp_id];
	struct mc_scheduler_qp_ctx *mc_qp_ctx;
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	struct mc_scheduler_ctx *mc_ctx = sched_ctx->private_ctx;

	mc_qp_ctx = rte_zmalloc_socket(NULL, sizeof(*mc_qp_ctx), 0,
			rte_socket_id());
	if (!mc_qp_ctx) {
		CR_SCHED_LOG(ERR, "failed allocate memory for private queue pair");
		return -ENOMEM;
	}

	mc_qp_ctx->mc_private_ctx = mc_ctx;
	qp_ctx->private_qp_ctx = (void *)mc_qp_ctx;


	return 0;
}

static int
scheduler_create_private_ctx(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	struct mc_scheduler_ctx *mc_ctx = NULL;
	uint16_t i;

	if (sched_ctx->private_ctx) {
		rte_free(sched_ctx->private_ctx);
		sched_ctx->private_ctx = NULL;
	}

	mc_ctx = rte_zmalloc_socket(NULL, sizeof(struct mc_scheduler_ctx), 0,
			rte_socket_id());
	if (!mc_ctx) {
		CR_SCHED_LOG(ERR, "failed allocate memory");
		return -ENOMEM;
	}

	mc_ctx->num_workers = sched_ctx->nb_wc;
	for (i = 0; i < sched_ctx->nb_wc; i++) {
		char r_name[16];

		snprintf(r_name, sizeof(r_name), MC_SCHED_ENQ_RING_NAME_PREFIX
				"%u_%u", dev->data->dev_id, i);
		mc_ctx->sched_enq_ring[i] = rte_ring_lookup(r_name);
		if (!mc_ctx->sched_enq_ring[i]) {
			mc_ctx->sched_enq_ring[i] = rte_ring_create(r_name,
						PER_SLAVE_BUFF_SIZE,
						rte_socket_id(),
						RING_F_SC_DEQ | RING_F_SP_ENQ);
			if (!mc_ctx->sched_enq_ring[i]) {
				CR_SCHED_LOG(ERR, "Cannot create ring for worker %u",
					   i);
				goto exit;
			}
		}
		snprintf(r_name, sizeof(r_name), MC_SCHED_DEQ_RING_NAME_PREFIX
				"%u_%u", dev->data->dev_id, i);
		mc_ctx->sched_deq_ring[i] = rte_ring_lookup(r_name);
		if (!mc_ctx->sched_deq_ring[i]) {
			mc_ctx->sched_deq_ring[i] = rte_ring_create(r_name,
						PER_SLAVE_BUFF_SIZE,
						rte_socket_id(),
						RING_F_SC_DEQ | RING_F_SP_ENQ);
			if (!mc_ctx->sched_deq_ring[i]) {
				CR_SCHED_LOG(ERR, "Cannot create ring for worker %u",
					   i);
				goto exit;
			}
		}
	}

	sched_ctx->private_ctx = (void *)mc_ctx;

	return 0;

exit:
	for (i = 0; i < sched_ctx->nb_wc; i++) {
		rte_ring_free(mc_ctx->sched_enq_ring[i]);
		rte_ring_free(mc_ctx->sched_deq_ring[i]);
	}
	rte_free(mc_ctx);

	return -1;
}

static struct rte_cryptodev_scheduler_ops scheduler_mc_ops = {
	slave_attach,
	slave_detach,
	scheduler_start,
	scheduler_stop,
	scheduler_config_qp,
	scheduler_create_private_ctx,
	NULL,	/* option_set */
	NULL	/* option_get */
};

static struct rte_cryptodev_scheduler mc_scheduler = {
		.name = "multicore-scheduler",
		.description = "scheduler which will run burst across multiple cpu cores",
		.mode = CDEV_SCHED_MODE_MULTICORE,
		.ops = &scheduler_mc_ops
};

struct rte_cryptodev_scheduler *crypto_scheduler_multicore = &mc_scheduler;
