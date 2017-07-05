/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_cryptodev.h>
#include <rte_malloc.h>

#include "rte_cryptodev_scheduler_operations.h"
#include "scheduler_pmd_private.h"

#define PRIMARY_SLAVE_IDX	0
#define SECONDARY_SLAVE_IDX	1
#define NB_FAILOVER_SLAVES	2
#define SLAVE_SWITCH_MASK	(0x01)

struct fo_scheduler_qp_ctx {
	struct scheduler_slave primary_slave;
	struct scheduler_slave secondary_slave;

	uint8_t deq_idx;
};

static __rte_always_inline uint16_t
failover_slave_enqueue(struct scheduler_slave *slave,
		struct rte_crypto_op **ops, uint16_t nb_ops)
{
	uint16_t i, processed_ops;

	for (i = 0; i < nb_ops && i < 4; i++)
		rte_prefetch0(ops[i]->sym->session);

	processed_ops = rte_cryptodev_enqueue_burst(slave->dev_id,
			slave->qp_id, ops, nb_ops);
	slave->nb_inflight_cops += processed_ops;

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

	enqueued_ops = failover_slave_enqueue(&qp_ctx->primary_slave,
			ops, nb_ops);

	if (enqueued_ops < nb_ops)
		enqueued_ops += failover_slave_enqueue(&qp_ctx->secondary_slave,
				&ops[enqueued_ops],
				nb_ops - enqueued_ops);

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
	struct scheduler_slave *slaves[NB_FAILOVER_SLAVES] = {
			&qp_ctx->primary_slave, &qp_ctx->secondary_slave};
	struct scheduler_slave *slave = slaves[qp_ctx->deq_idx];
	uint16_t nb_deq_ops = 0, nb_deq_ops2 = 0;

	if (slave->nb_inflight_cops) {
		nb_deq_ops = rte_cryptodev_dequeue_burst(slave->dev_id,
			slave->qp_id, ops, nb_ops);
		slave->nb_inflight_cops -= nb_deq_ops;
	}

	qp_ctx->deq_idx = (~qp_ctx->deq_idx) & SLAVE_SWITCH_MASK;

	if (nb_deq_ops == nb_ops)
		return nb_deq_ops;

	slave = slaves[qp_ctx->deq_idx];

	if (slave->nb_inflight_cops) {
		nb_deq_ops2 = rte_cryptodev_dequeue_burst(slave->dev_id,
			slave->qp_id, &ops[nb_deq_ops], nb_ops - nb_deq_ops);
		slave->nb_inflight_cops -= nb_deq_ops2;
	}

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
scheduler_start(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint16_t i;

	if (sched_ctx->nb_slaves < 2) {
		CS_LOG_ERR("Number of slaves shall no less than 2");
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

		rte_memcpy(&qp_ctx->primary_slave,
				&sched_ctx->slaves[PRIMARY_SLAVE_IDX],
				sizeof(struct scheduler_slave));
		rte_memcpy(&qp_ctx->secondary_slave,
				&sched_ctx->slaves[SECONDARY_SLAVE_IDX],
				sizeof(struct scheduler_slave));
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
		CS_LOG_ERR("failed allocate memory for private queue pair");
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

struct rte_cryptodev_scheduler_ops scheduler_fo_ops = {
	slave_attach,
	slave_detach,
	scheduler_start,
	scheduler_stop,
	scheduler_config_qp,
	scheduler_create_private_ctx,
	NULL,	/* option_set */
	NULL	/*option_get */
};

struct rte_cryptodev_scheduler fo_scheduler = {
		.name = "failover-scheduler",
		.description = "scheduler which enqueues to the primary slave, "
				"and only then enqueues to the secondary slave "
				"upon failing on enqueuing to primary",
		.mode = CDEV_SCHED_MODE_FAILOVER,
		.ops = &scheduler_fo_ops
};

struct rte_cryptodev_scheduler *failover_scheduler = &fo_scheduler;
