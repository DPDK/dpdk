/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _CPT_COMMON_H_
#define _CPT_COMMON_H_

#include <rte_prefetch.h>
#include <rte_mempool.h>

/*
 * This file defines common macros and structs
 */

#define TIME_IN_RESET_COUNT	5

/* Default command timeout in seconds */
#define DEFAULT_COMMAND_TIMEOUT	4

#define CPT_COUNT_THOLD		32
#define CPT_TIMER_THOLD		0x3F

#define MOD_INC(i, l)   ((i) == (l - 1) ? (i) = 0 : (i)++)

struct cpt_qp_meta_info {
	struct rte_mempool *pool;
	int sg_mlen;
	int lb_mlen;
};

/*
 * Pending queue structure
 *
 */
struct pending_queue {
	/** Array of pending requests */
	void **rid_queue;
	/** Tail of queue to be used for enqueue */
	unsigned int tail;
	/** Head of queue to be used for dequeue */
	unsigned int head;
};

struct cpt_request_info {
	/** Data path fields */
	uint64_t comp_baddr;
	volatile uint64_t *completion_addr;
	volatile uint64_t *alternate_caddr;
	void *op;
	struct {
		uint64_t ei0;
		uint64_t ei1;
		uint64_t ei2;
	} ist;
	uint8_t *rptr;
	const void *qp;

	/** Control path fields */
	uint64_t time_out;
	uint8_t extra_time;
} __rte_aligned(8);

static __rte_always_inline void
pending_queue_push(struct pending_queue *q, void *rid, unsigned int off,
			const int qsize)
{
	/* NOTE: no free space check, but it is expected that one is made */
	q->rid_queue[(q->tail + off) & (qsize - 1)] = rid;
}

static __rte_always_inline void
pending_queue_commit(struct pending_queue *q, unsigned int cnt,
			const unsigned int qsize)
{
	/* Ensure ordering between setting the entry and updating the tail */
	rte_atomic_thread_fence(__ATOMIC_RELEASE);

	q->tail = (q->tail + cnt) & (qsize - 1);
}

static __rte_always_inline void
pending_queue_pop(struct pending_queue *q, const int qsize)
{
	/* NOTE: no empty check, but it is expected that one is made prior */

	q->head = (q->head + 1) & (qsize - 1);
}

static __rte_always_inline void
pending_queue_peek(struct pending_queue *q, void **rid, const int qsize,
			int prefetch_next)
{
	void *next_rid;
	/* NOTE: no empty check, but it is expected that one is made */

	*rid = q->rid_queue[q->head];

	if (likely(prefetch_next)) {
		next_rid = q->rid_queue[(q->head + 1) & (qsize - 1)];
		rte_prefetch_non_temporal((void *)next_rid);
	}
}

static __rte_always_inline unsigned int
pending_queue_level(struct pending_queue *q, const int qsize)
{
	return (q->tail - q->head) & (qsize - 1);
}

static __rte_always_inline unsigned int
pending_queue_free_slots(struct pending_queue *q, const int qsize,
		const int reserved_slots)
{
	int free_slots;

	free_slots = qsize - pending_queue_level(q, qsize);

	/* Use only use qsize - 1 */
	free_slots -= 1 + reserved_slots;

	if (unlikely(free_slots < 0))
		return 0;

	return free_slots;
}

#endif /* _CPT_COMMON_H_ */
