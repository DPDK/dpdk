/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2010-2020 Intel Corporation
 * Copyright (c) 2007-2009 Kip Macy kmacy@freebsd.org
 * All rights reserved.
 * Derived from FreeBSD's bufring.h
 * Used as BSD-3 Licensed with permission from Kip Macy.
 */

#ifndef _RTE_RING_PEEK_ELEM_PVT_H_
#define _RTE_RING_PEEK_ELEM_PVT_H_

/**
 * @file rte_ring_peek_elem_pvt.h
 * It is not recommended to include this file directly,
 * include <rte_ring.h> instead.
 * Contains internal helper functions for rte_ring peek API.
 * For more information please refer to <rte_ring_peek.h>.
 */

/**
 * @internal get current tail value.
 * This function should be used only for single thread producer/consumer.
 * Check that user didn't request to move tail above the head.
 * In that situation:
 * - return zero, that will cause abort any pending changes and
 *   return head to its previous position.
 * - throw an assert in debug mode.
 */
static __rte_always_inline uint32_t
__rte_ring_st_get_tail(struct rte_ring_headtail *ht, uint32_t *tail,
	uint32_t num)
{
	uint32_t h, n, t;

	h = ht->head;
	t = ht->tail;
	n = h - t;

	RTE_ASSERT(n >= num);
	num = (n >= num) ? num : 0;

	*tail = t;
	return num;
}

/**
 * @internal set new values for head and tail.
 * This function should be used only for single thread producer/consumer.
 * Should be used only in conjunction with __rte_ring_st_get_tail.
 */
static __rte_always_inline void
__rte_ring_st_set_head_tail(struct rte_ring_headtail *ht, uint32_t tail,
	uint32_t num, uint32_t enqueue)
{
	uint32_t pos;

	RTE_SET_USED(enqueue);

	pos = tail + num;
	ht->head = pos;
	__atomic_store_n(&ht->tail, pos, __ATOMIC_RELEASE);
}

/**
 * @internal get current tail value.
 * This function should be used only for producer/consumer in MT_HTS mode.
 * Check that user didn't request to move tail above the head.
 * In that situation:
 * - return zero, that will cause abort any pending changes and
 *   return head to its previous position.
 * - throw an assert in debug mode.
 */
static __rte_always_inline uint32_t
__rte_ring_hts_get_tail(struct rte_ring_hts_headtail *ht, uint32_t *tail,
	uint32_t num)
{
	uint32_t n;
	union __rte_ring_hts_pos p;

	p.raw = __atomic_load_n(&ht->ht.raw, __ATOMIC_RELAXED);
	n = p.pos.head - p.pos.tail;

	RTE_ASSERT(n >= num);
	num = (n >= num) ? num : 0;

	*tail = p.pos.tail;
	return num;
}

/**
 * @internal set new values for head and tail as one atomic 64 bit operation.
 * This function should be used only for producer/consumer in MT_HTS mode.
 * Should be used only in conjunction with __rte_ring_hts_get_tail.
 */
static __rte_always_inline void
__rte_ring_hts_set_head_tail(struct rte_ring_hts_headtail *ht, uint32_t tail,
	uint32_t num, uint32_t enqueue)
{
	union __rte_ring_hts_pos p;

	RTE_SET_USED(enqueue);

	p.pos.head = tail + num;
	p.pos.tail = p.pos.head;

	__atomic_store_n(&ht->ht.raw, p.raw, __ATOMIC_RELEASE);
}

/**
 * @internal This function moves prod head value.
 */
static __rte_always_inline unsigned int
__rte_ring_do_enqueue_start(struct rte_ring *r, uint32_t n,
		enum rte_ring_queue_behavior behavior, uint32_t *free_space)
{
	uint32_t free, head, next;

	switch (r->prod.sync_type) {
	case RTE_RING_SYNC_ST:
		n = __rte_ring_move_prod_head(r, RTE_RING_SYNC_ST, n,
			behavior, &head, &next, &free);
		break;
	case RTE_RING_SYNC_MT_HTS:
		n =  __rte_ring_hts_move_prod_head(r, n, behavior,
			&head, &free);
		break;
	case RTE_RING_SYNC_MT:
	case RTE_RING_SYNC_MT_RTS:
	default:
		/* unsupported mode, shouldn't be here */
		RTE_ASSERT(0);
		n = 0;
		free = 0;
	}

	if (free_space != NULL)
		*free_space = free - n;
	return n;
}

/**
 * @internal This function moves cons head value and copies up to *n*
 * objects from the ring to the user provided obj_table.
 */
static __rte_always_inline unsigned int
__rte_ring_do_dequeue_start(struct rte_ring *r, void *obj_table,
	uint32_t esize, uint32_t n, enum rte_ring_queue_behavior behavior,
	uint32_t *available)
{
	uint32_t avail, head, next;

	switch (r->cons.sync_type) {
	case RTE_RING_SYNC_ST:
		n = __rte_ring_move_cons_head(r, RTE_RING_SYNC_ST, n,
			behavior, &head, &next, &avail);
		break;
	case RTE_RING_SYNC_MT_HTS:
		n =  __rte_ring_hts_move_cons_head(r, n, behavior,
			&head, &avail);
		break;
	case RTE_RING_SYNC_MT:
	case RTE_RING_SYNC_MT_RTS:
	default:
		/* unsupported mode, shouldn't be here */
		RTE_ASSERT(0);
		n = 0;
		avail = 0;
	}

	if (n != 0)
		__rte_ring_dequeue_elems(r, head, obj_table, esize, n);

	if (available != NULL)
		*available = avail - n;
	return n;
}

#endif /* _RTE_RING_PEEK_ELEM_PVT_H_ */
