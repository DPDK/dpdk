/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_QP_H_
#define _NITROX_QP_H_

#include <stdbool.h>

#include <rte_io.h>
#include "nitrox_hal.h"

struct nitrox_softreq;

enum nitrox_queue_type {
	NITROX_QUEUE_SE,
	NITROX_QUEUE_AE,
	NITROX_QUEUE_ZIP,
};

struct command_queue {
	const struct rte_memzone *mz;
	uint8_t *dbell_csr_addr;
	uint8_t *ring;
	uint8_t instr_size;
};

struct rid {
	struct nitrox_softreq *sr;
};

struct nitrox_qp_stats {
	uint64_t enqueued_count;
	uint64_t dequeued_count;
	uint64_t enqueue_err_count;
	uint64_t dequeue_err_count;
};

struct nitrox_qp {
	enum nitrox_queue_type type;
	uint8_t *bar_addr;
	struct command_queue cmdq;
	struct rid *ridq;
	uint32_t count;
	uint32_t head;
	uint32_t tail;
	struct rte_mempool *sr_mp;
	struct nitrox_qp_stats stats;
	uint16_t qno;
	RTE_ATOMIC(uint16_t) pending_count;
};

static inline uint16_t
nitrox_qp_free_count(struct nitrox_qp *qp)
{
	uint16_t pending_count;

	pending_count = rte_atomic_load_explicit(&qp->pending_count,
						 rte_memory_order_relaxed);
	RTE_ASSERT(qp->count >= pending_count);
	return (qp->count - pending_count);
}

static inline bool
nitrox_qp_is_empty(struct nitrox_qp *qp)
{
	return (rte_atomic_load_explicit(&qp->pending_count,
					 rte_memory_order_relaxed) == 0);
}

static inline uint16_t
nitrox_qp_used_count(struct nitrox_qp *qp)
{
	return rte_atomic_load_explicit(&qp->pending_count,
					 rte_memory_order_relaxed);
}

static inline struct nitrox_softreq *
nitrox_qp_get_softreq(struct nitrox_qp *qp)
{
	uint32_t tail = qp->tail % qp->count;

	rte_atomic_thread_fence(rte_memory_order_acquire);
	return qp->ridq[tail].sr;
}

static inline void
nitrox_ring_dbell(struct nitrox_qp *qp, uint16_t cnt)
{
	struct command_queue *cmdq = &qp->cmdq;

	if (!cnt)
		return;

	rte_io_wmb();
	rte_write64(cnt, cmdq->dbell_csr_addr);
}

static inline void
nitrox_qp_enqueue(struct nitrox_qp *qp, void *instr, struct nitrox_softreq *sr)
{
	uint32_t head = qp->head % qp->count;

	qp->head++;
	memcpy(&qp->cmdq.ring[head * qp->cmdq.instr_size],
	       instr, qp->cmdq.instr_size);
	qp->ridq[head].sr = sr;
	rte_atomic_thread_fence(rte_memory_order_release);
	rte_atomic_fetch_add_explicit(&qp->pending_count, 1,
				      rte_memory_order_relaxed);
}

static inline int
nitrox_qp_enqueue_sr(struct nitrox_qp *qp, struct nitrox_softreq *sr)
{
	uint32_t head = qp->head % qp->count;
	int err;

	err = inc_zqmq_next_cmd(qp->bar_addr, qp->qno);
	if (unlikely(err))
		return err;

	qp->head++;
	qp->ridq[head].sr = sr;
	rte_atomic_thread_fence(rte_memory_order_release);
	rte_atomic_fetch_add_explicit(&qp->pending_count, 1,
				      rte_memory_order_relaxed);
	return 0;
}

static inline void
nitrox_qp_dequeue(struct nitrox_qp *qp)
{
	qp->tail++;
	rte_atomic_fetch_sub_explicit(&qp->pending_count, 1,
				      rte_memory_order_relaxed);
}

__rte_internal
int nitrox_qp_setup(struct nitrox_qp *qp, uint8_t *bar_addr,
		    const char *dev_name, uint32_t nb_descriptors,
		    uint8_t inst_size, int socket_id);
__rte_internal
int nitrox_qp_release(struct nitrox_qp *qp, uint8_t *bar_addr);

#endif /* _NITROX_QP_H_ */
