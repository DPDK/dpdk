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

struct rr_scheduler_qp_ctx {
	struct scheduler_slave slaves[MAX_SLAVES_NUM];
	uint32_t nb_slaves;

	uint32_t last_enq_slave_idx;
	uint32_t last_deq_slave_idx;
};

static uint16_t
schedule_enqueue(void *qp_ctx, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct rr_scheduler_qp_ctx *rr_qp_ctx =
			((struct scheduler_qp_ctx *)qp_ctx)->private_qp_ctx;
	uint32_t slave_idx = rr_qp_ctx->last_enq_slave_idx;
	struct scheduler_slave *slave = &rr_qp_ctx->slaves[slave_idx];
	uint16_t i, processed_ops;
	struct rte_cryptodev_sym_session *sessions[nb_ops];
	struct scheduler_session *sess0, *sess1, *sess2, *sess3;

	if (unlikely(nb_ops == 0))
		return 0;

	for (i = 0; i < nb_ops && i < 4; i++)
		rte_prefetch0(ops[i]->sym->session);

	for (i = 0; (i < (nb_ops - 8)) && (nb_ops > 8); i += 4) {
		sess0 = (struct scheduler_session *)
				ops[i]->sym->session->_private;
		sess1 = (struct scheduler_session *)
				ops[i+1]->sym->session->_private;
		sess2 = (struct scheduler_session *)
				ops[i+2]->sym->session->_private;
		sess3 = (struct scheduler_session *)
				ops[i+3]->sym->session->_private;

		sessions[i] = ops[i]->sym->session;
		sessions[i + 1] = ops[i + 1]->sym->session;
		sessions[i + 2] = ops[i + 2]->sym->session;
		sessions[i + 3] = ops[i + 3]->sym->session;

		ops[i]->sym->session = sess0->sessions[slave_idx];
		ops[i + 1]->sym->session = sess1->sessions[slave_idx];
		ops[i + 2]->sym->session = sess2->sessions[slave_idx];
		ops[i + 3]->sym->session = sess3->sessions[slave_idx];

		rte_prefetch0(ops[i + 4]->sym->session);
		rte_prefetch0(ops[i + 5]->sym->session);
		rte_prefetch0(ops[i + 6]->sym->session);
		rte_prefetch0(ops[i + 7]->sym->session);
	}

	for (; i < nb_ops; i++) {
		sess0 = (struct scheduler_session *)
				ops[i]->sym->session->_private;
		sessions[i] = ops[i]->sym->session;
		ops[i]->sym->session = sess0->sessions[slave_idx];
	}

	processed_ops = rte_cryptodev_enqueue_burst(slave->dev_id,
			slave->qp_id, ops, nb_ops);

	slave->nb_inflight_cops += processed_ops;

	rr_qp_ctx->last_enq_slave_idx += 1;
	rr_qp_ctx->last_enq_slave_idx %= rr_qp_ctx->nb_slaves;

	/* recover session if enqueue is failed */
	if (unlikely(processed_ops < nb_ops)) {
		for (i = processed_ops; i < nb_ops; i++)
			ops[i]->sym->session = sessions[i];
	}

	return processed_ops;
}

static uint16_t
schedule_enqueue_ordering(void *qp_ctx, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct scheduler_qp_ctx *gen_qp_ctx = qp_ctx;
	struct rr_scheduler_qp_ctx *rr_qp_ctx =
			gen_qp_ctx->private_qp_ctx;
	uint32_t slave_idx = rr_qp_ctx->last_enq_slave_idx;
	struct scheduler_slave *slave = &rr_qp_ctx->slaves[slave_idx];
	uint16_t i, processed_ops;
	struct rte_cryptodev_sym_session *sessions[nb_ops];
	struct scheduler_session *sess0, *sess1, *sess2, *sess3;

	if (unlikely(nb_ops == 0))
		return 0;

	for (i = 0; i < nb_ops && i < 4; i++) {
		rte_prefetch0(ops[i]->sym->session);
		rte_prefetch0(ops[i]->sym->m_src);
	}

	for (i = 0; (i < (nb_ops - 8)) && (nb_ops > 8); i += 4) {
		sess0 = (struct scheduler_session *)
				ops[i]->sym->session->_private;
		sess1 = (struct scheduler_session *)
				ops[i+1]->sym->session->_private;
		sess2 = (struct scheduler_session *)
				ops[i+2]->sym->session->_private;
		sess3 = (struct scheduler_session *)
				ops[i+3]->sym->session->_private;

		sessions[i] = ops[i]->sym->session;
		sessions[i + 1] = ops[i + 1]->sym->session;
		sessions[i + 2] = ops[i + 2]->sym->session;
		sessions[i + 3] = ops[i + 3]->sym->session;

		ops[i]->sym->session = sess0->sessions[slave_idx];
		ops[i]->sym->m_src->seqn = gen_qp_ctx->seqn++;
		ops[i + 1]->sym->session = sess1->sessions[slave_idx];
		ops[i + 1]->sym->m_src->seqn = gen_qp_ctx->seqn++;
		ops[i + 2]->sym->session = sess2->sessions[slave_idx];
		ops[i + 2]->sym->m_src->seqn = gen_qp_ctx->seqn++;
		ops[i + 3]->sym->session = sess3->sessions[slave_idx];
		ops[i + 3]->sym->m_src->seqn = gen_qp_ctx->seqn++;

		rte_prefetch0(ops[i + 4]->sym->session);
		rte_prefetch0(ops[i + 4]->sym->m_src);
		rte_prefetch0(ops[i + 5]->sym->session);
		rte_prefetch0(ops[i + 5]->sym->m_src);
		rte_prefetch0(ops[i + 6]->sym->session);
		rte_prefetch0(ops[i + 6]->sym->m_src);
		rte_prefetch0(ops[i + 7]->sym->session);
		rte_prefetch0(ops[i + 7]->sym->m_src);
	}

	for (; i < nb_ops; i++) {
		sess0 = (struct scheduler_session *)
				ops[i]->sym->session->_private;
		sessions[i] = ops[i]->sym->session;
		ops[i]->sym->session = sess0->sessions[slave_idx];
		ops[i]->sym->m_src->seqn = gen_qp_ctx->seqn++;
	}

	processed_ops = rte_cryptodev_enqueue_burst(slave->dev_id,
			slave->qp_id, ops, nb_ops);

	slave->nb_inflight_cops += processed_ops;

	rr_qp_ctx->last_enq_slave_idx += 1;
	rr_qp_ctx->last_enq_slave_idx %= rr_qp_ctx->nb_slaves;

	/* recover session if enqueue is failed */
	if (unlikely(processed_ops < nb_ops)) {
		for (i = processed_ops; i < nb_ops; i++)
			ops[i]->sym->session = sessions[i];
	}

	return processed_ops;
}


static uint16_t
schedule_dequeue(void *qp_ctx, struct rte_crypto_op **ops, uint16_t nb_ops)
{
	struct rr_scheduler_qp_ctx *rr_qp_ctx =
			((struct scheduler_qp_ctx *)qp_ctx)->private_qp_ctx;
	struct scheduler_slave *slave;
	uint32_t last_slave_idx = rr_qp_ctx->last_deq_slave_idx;
	uint16_t nb_deq_ops;

	if (unlikely(rr_qp_ctx->slaves[last_slave_idx].nb_inflight_cops == 0)) {
		do {
			last_slave_idx += 1;

			if (unlikely(last_slave_idx >= rr_qp_ctx->nb_slaves))
				last_slave_idx = 0;
			/* looped back, means no inflight cops in the queue */
			if (last_slave_idx == rr_qp_ctx->last_deq_slave_idx)
				return 0;
		} while (rr_qp_ctx->slaves[last_slave_idx].nb_inflight_cops
				== 0);
	}

	slave = &rr_qp_ctx->slaves[last_slave_idx];

	nb_deq_ops = rte_cryptodev_dequeue_burst(slave->dev_id,
			slave->qp_id, ops, nb_ops);

	last_slave_idx += 1;
	last_slave_idx %= rr_qp_ctx->nb_slaves;

	rr_qp_ctx->last_deq_slave_idx = last_slave_idx;

	slave->nb_inflight_cops -= nb_deq_ops;

	return nb_deq_ops;
}

static uint16_t
schedule_dequeue_ordering(void *qp_ctx, struct rte_crypto_op **ops,
		uint16_t nb_ops)
{
	struct scheduler_qp_ctx *gen_qp_ctx = (struct scheduler_qp_ctx *)qp_ctx;
	struct rr_scheduler_qp_ctx *rr_qp_ctx = (gen_qp_ctx->private_qp_ctx);
	struct scheduler_slave *slave;
	struct rte_reorder_buffer *reorder_buff = gen_qp_ctx->reorder_buf;
	struct rte_mbuf *mbuf0, *mbuf1, *mbuf2, *mbuf3;
	uint16_t nb_deq_ops, nb_drained_mbufs;
	const uint16_t nb_op_ops = nb_ops;
	struct rte_crypto_op *op_ops[nb_op_ops];
	struct rte_mbuf *reorder_mbufs[nb_op_ops];
	uint32_t last_slave_idx = rr_qp_ctx->last_deq_slave_idx;
	uint16_t i;

	if (unlikely(rr_qp_ctx->slaves[last_slave_idx].nb_inflight_cops == 0)) {
		do {
			last_slave_idx += 1;

			if (unlikely(last_slave_idx >= rr_qp_ctx->nb_slaves))
				last_slave_idx = 0;
			/* looped back, means no inflight cops in the queue */
			if (last_slave_idx == rr_qp_ctx->last_deq_slave_idx)
				return 0;
		} while (rr_qp_ctx->slaves[last_slave_idx].nb_inflight_cops
				== 0);
	}

	slave = &rr_qp_ctx->slaves[last_slave_idx];

	nb_deq_ops = rte_cryptodev_dequeue_burst(slave->dev_id,
			slave->qp_id, op_ops, nb_ops);

	rr_qp_ctx->last_deq_slave_idx += 1;
	rr_qp_ctx->last_deq_slave_idx %= rr_qp_ctx->nb_slaves;

	slave->nb_inflight_cops -= nb_deq_ops;

	for (i = 0; i < nb_deq_ops && i < 4; i++)
		rte_prefetch0(op_ops[i]->sym->m_src);

	for (i = 0; (i < (nb_deq_ops - 8)) && (nb_deq_ops > 8); i += 4) {
		mbuf0 = op_ops[i]->sym->m_src;
		mbuf1 = op_ops[i + 1]->sym->m_src;
		mbuf2 = op_ops[i + 2]->sym->m_src;
		mbuf3 = op_ops[i + 3]->sym->m_src;

		mbuf0->userdata = op_ops[i];
		mbuf1->userdata = op_ops[i + 1];
		mbuf2->userdata = op_ops[i + 2];
		mbuf3->userdata = op_ops[i + 3];

		rte_reorder_insert(reorder_buff, mbuf0);
		rte_reorder_insert(reorder_buff, mbuf1);
		rte_reorder_insert(reorder_buff, mbuf2);
		rte_reorder_insert(reorder_buff, mbuf3);

		rte_prefetch0(op_ops[i + 4]->sym->m_src);
		rte_prefetch0(op_ops[i + 5]->sym->m_src);
		rte_prefetch0(op_ops[i + 6]->sym->m_src);
		rte_prefetch0(op_ops[i + 7]->sym->m_src);
	}

	for (; i < nb_deq_ops; i++) {
		mbuf0 = op_ops[i]->sym->m_src;
		mbuf0->userdata = op_ops[i];
		rte_reorder_insert(reorder_buff, mbuf0);
	}

	nb_drained_mbufs = rte_reorder_drain(reorder_buff, reorder_mbufs,
			nb_ops);
	for (i = 0; i < nb_drained_mbufs && i < 4; i++)
		rte_prefetch0(reorder_mbufs[i]);

	for (i = 0; (i < (nb_drained_mbufs - 8)) && (nb_drained_mbufs > 8);
			i += 4) {
		ops[i] = *(struct rte_crypto_op **)reorder_mbufs[i]->userdata;
		ops[i + 1] = *(struct rte_crypto_op **)
			reorder_mbufs[i + 1]->userdata;
		ops[i + 2] = *(struct rte_crypto_op **)
			reorder_mbufs[i + 2]->userdata;
		ops[i + 3] = *(struct rte_crypto_op **)
			reorder_mbufs[i + 3]->userdata;

		reorder_mbufs[i]->userdata = NULL;
		reorder_mbufs[i + 1]->userdata = NULL;
		reorder_mbufs[i + 2]->userdata = NULL;
		reorder_mbufs[i + 3]->userdata = NULL;

		rte_prefetch0(reorder_mbufs[i + 4]);
		rte_prefetch0(reorder_mbufs[i + 5]);
		rte_prefetch0(reorder_mbufs[i + 6]);
		rte_prefetch0(reorder_mbufs[i + 7]);
	}

	for (; i < nb_drained_mbufs; i++) {
		ops[i] = *(struct rte_crypto_op **)
			reorder_mbufs[i]->userdata;
		reorder_mbufs[i]->userdata = NULL;
	}

	return nb_drained_mbufs;
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

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		struct scheduler_qp_ctx *qp_ctx = dev->data->queue_pairs[i];
		struct rr_scheduler_qp_ctx *rr_qp_ctx =
				qp_ctx->private_qp_ctx;
		uint32_t j;

		memset(rr_qp_ctx->slaves, 0, MAX_SLAVES_NUM *
				sizeof(struct scheduler_slave));
		for (j = 0; j < sched_ctx->nb_slaves; j++) {
			rr_qp_ctx->slaves[j].dev_id =
					sched_ctx->slaves[j].dev_id;
			rr_qp_ctx->slaves[j].qp_id = i;
		}

		rr_qp_ctx->nb_slaves = sched_ctx->nb_slaves;

		rr_qp_ctx->last_enq_slave_idx = 0;
		rr_qp_ctx->last_deq_slave_idx = 0;

		if (sched_ctx->reordering_enabled) {
			qp_ctx->schedule_enqueue = &schedule_enqueue_ordering;
			qp_ctx->schedule_dequeue = &schedule_dequeue_ordering;
		} else {
			qp_ctx->schedule_enqueue = &schedule_enqueue;
			qp_ctx->schedule_dequeue = &schedule_dequeue;
		}
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
		CS_LOG_ERR("failed allocate memory for private queue pair");
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

struct rte_cryptodev_scheduler_ops scheduler_rr_ops = {
	slave_attach,
	slave_detach,
	scheduler_start,
	scheduler_stop,
	scheduler_config_qp,
	scheduler_create_private_ctx
};

struct rte_cryptodev_scheduler scheduler = {
		.name = "roundrobin-scheduler",
		.description = "scheduler which will round robin burst across "
				"slave crypto devices",
		.mode = CDEV_SCHED_MODE_ROUNDROBIN,
		.ops = &scheduler_rr_ops
};

struct rte_cryptodev_scheduler *roundrobin_scheduler = &scheduler;
