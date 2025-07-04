/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2010-2020 Intel Corporation
 * Copyright (c) 2007-2009 Kip Macy kmacy@freebsd.org
 * All rights reserved.
 * Derived from FreeBSD's bufring.h
 * Used as BSD-3 Licensed with permission from Kip Macy.
 */

#ifndef _RTE_RING_HTS_ELEM_PVT_H_
#define _RTE_RING_HTS_ELEM_PVT_H_

#include <rte_stdatomic.h>

/**
 * @file rte_ring_hts_elem_pvt.h
 * It is not recommended to include this file directly,
 * include <rte_ring.h> instead.
 * Contains internal helper functions for head/tail sync (HTS) ring mode.
 * For more information please refer to <rte_ring_hts.h>.
 */

/**
 * @internal update tail with new value.
 */
static __rte_always_inline void
__rte_ring_hts_update_tail(struct rte_ring_hts_headtail *ht, uint32_t old_tail,
	uint32_t num, uint32_t enqueue)
{
	uint32_t tail;

	RTE_SET_USED(enqueue);

	tail = old_tail + num;
	rte_atomic_store_explicit(&ht->ht.pos.tail, tail, rte_memory_order_release);
}

/**
 * @internal waits till tail will become equal to head.
 * Means no writer/reader is active for that ring.
 * Suppose to work as serialization point.
 */
static __rte_always_inline void
__rte_ring_hts_head_wait(const struct rte_ring_hts_headtail *ht,
		union __rte_ring_hts_pos *p)
{
	while (p->pos.head != p->pos.tail) {
		rte_pause();
		p->raw = rte_atomic_load_explicit(&ht->ht.raw, rte_memory_order_acquire);
	}
}

/**
 * @internal This is a helper function that moves the producer/consumer head
 *
 * @param d
 *   A pointer to the headtail structure with head value to be moved
 * @param s
 *   A pointer to the counter-part headtail structure. Note that this
 *   function only reads tail value from it
 * @param capacity
 *   Either ring capacity value (for producer), or zero (for consumer)
 *   Indicates whether multi-thread safe path is needed or not
 * @param num
 *   The number of elements we want to move head value on
 * @param behavior
 *   RTE_RING_QUEUE_FIXED:    Move on a fixed number of items
 *   RTE_RING_QUEUE_VARIABLE: Move on as many items as possible
 * @param old_head
 *   Returns head value as it was before the move
 * @param entries
 *   Returns the number of ring entries available BEFORE head was moved
 * @return
 *   Actual number of objects the head was moved on
 *   If behavior == RTE_RING_QUEUE_FIXED, this will be 0 or n only
 */
static __rte_always_inline uint32_t
__rte_ring_hts_move_head(struct rte_ring_hts_headtail *d,
	const struct rte_ring_headtail *s, uint32_t capacity, unsigned int num,
	enum rte_ring_queue_behavior behavior, uint32_t *old_head,
	uint32_t *entries)
{
	uint32_t n;
	union __rte_ring_hts_pos np, op;

	op.raw = rte_atomic_load_explicit(&d->ht.raw, rte_memory_order_acquire);

	do {
		/* Reset n to the initial burst count */
		n = num;

		/*
		 * wait for tail to be equal to head,
		 * make sure that we read prod head/tail *before*
		 * reading cons tail.
		 */
		__rte_ring_hts_head_wait(d, &op);

		/*
		 *  The subtraction is done between two unsigned 32bits value
		 * (the result is always modulo 32 bits even if we have
		 * *old_head > cons_tail). So 'entries' is always between 0
		 * and capacity (which is < size).
		 */
		*entries = capacity + s->tail - op.pos.head;

		/* check that we have enough room in ring */
		if (unlikely(n > *entries))
			n = (behavior == RTE_RING_QUEUE_FIXED) ?
					0 : *entries;

		if (n == 0)
			break;

		np.pos.tail = op.pos.tail;
		np.pos.head = op.pos.head + n;

	/*
	 * this CAS(ACQUIRE, ACQUIRE) serves as a hoist barrier to prevent:
	 *  - OOO reads of cons tail value
	 *  - OOO copy of elems from the ring
	 */
	} while (rte_atomic_compare_exchange_strong_explicit(&d->ht.raw,
			(uint64_t *)(uintptr_t)&op.raw, np.raw,
			rte_memory_order_acquire,
			rte_memory_order_acquire) == 0);

	*old_head = op.pos.head;
	return n;
}
/**
 * @internal This function updates the producer head for enqueue
 */
static __rte_always_inline unsigned int
__rte_ring_hts_move_prod_head(struct rte_ring *r, unsigned int num,
	enum rte_ring_queue_behavior behavior, uint32_t *old_head,
	uint32_t *free_entries)
{
	return __rte_ring_hts_move_head(&r->hts_prod, &r->cons,
			r->capacity, num, behavior, old_head, free_entries);
}

/**
 * @internal This function updates the consumer head for dequeue
 */
static __rte_always_inline unsigned int
__rte_ring_hts_move_cons_head(struct rte_ring *r, unsigned int num,
	enum rte_ring_queue_behavior behavior, uint32_t *old_head,
	uint32_t *entries)
{
	return __rte_ring_hts_move_head(&r->hts_cons, &r->prod,
			0, num, behavior, old_head, entries);
}

/**
 * @internal Enqueue several objects on the HTS ring.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to add in the ring from the obj_table.
 * @param behavior
 *   RTE_RING_QUEUE_FIXED:    Enqueue a fixed number of items from a ring
 *   RTE_RING_QUEUE_VARIABLE: Enqueue as many items as possible from ring
 * @param free_space
 *   returns the amount of space after the enqueue operation has finished
 * @return
 *   Actual number of objects enqueued.
 *   If behavior == RTE_RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __rte_always_inline unsigned int
__rte_ring_do_hts_enqueue_elem(struct rte_ring *r, const void *obj_table,
	uint32_t esize, uint32_t n, enum rte_ring_queue_behavior behavior,
	uint32_t *free_space)
{
	uint32_t free, head;

	n =  __rte_ring_hts_move_prod_head(r, n, behavior, &head, &free);

	if (n != 0) {
		__rte_ring_enqueue_elems(r, head, obj_table, esize, n);
		__rte_ring_hts_update_tail(&r->hts_prod, head, n, 1);
	}

	if (free_space != NULL)
		*free_space = free - n;
	return n;
}

/**
 * @internal Dequeue several objects from the HTS ring.
 *
 * @param r
 *   A pointer to the ring structure.
 * @param obj_table
 *   A pointer to a table of objects.
 * @param esize
 *   The size of ring element, in bytes. It must be a multiple of 4.
 *   This must be the same value used while creating the ring. Otherwise
 *   the results are undefined.
 * @param n
 *   The number of objects to pull from the ring.
 * @param behavior
 *   RTE_RING_QUEUE_FIXED:    Dequeue a fixed number of items from a ring
 *   RTE_RING_QUEUE_VARIABLE: Dequeue as many items as possible from ring
 * @param available
 *   returns the number of remaining ring entries after the dequeue has finished
 * @return
 *   - Actual number of objects dequeued.
 *     If behavior == RTE_RING_QUEUE_FIXED, this will be 0 or n only.
 */
static __rte_always_inline unsigned int
__rte_ring_do_hts_dequeue_elem(struct rte_ring *r, void *obj_table,
	uint32_t esize, uint32_t n, enum rte_ring_queue_behavior behavior,
	uint32_t *available)
{
	uint32_t entries, head;

	n = __rte_ring_hts_move_cons_head(r, n, behavior, &head, &entries);

	if (n != 0) {
		__rte_ring_dequeue_elems(r, head, obj_table, esize, n);
		__rte_ring_hts_update_tail(&r->hts_cons, head, n, 0);
	}

	if (available != NULL)
		*available = entries - n;
	return n;
}

#endif /* _RTE_RING_HTS_ELEM_PVT_H_ */
