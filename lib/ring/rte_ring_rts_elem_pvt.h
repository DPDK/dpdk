/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2010-2020 Intel Corporation
 * Copyright (c) 2007-2009 Kip Macy kmacy@freebsd.org
 * All rights reserved.
 * Derived from FreeBSD's bufring.h
 * Used as BSD-3 Licensed with permission from Kip Macy.
 */

#ifndef _RTE_RING_RTS_ELEM_PVT_H_
#define _RTE_RING_RTS_ELEM_PVT_H_

/**
 * @file rte_ring_rts_elem_pvt.h
 * It is not recommended to include this file directly,
 * include <rte_ring.h> instead.
 * Contains internal helper functions for Relaxed Tail Sync (RTS) ring mode.
 * For more information please refer to <rte_ring_rts.h>.
 */

/**
 * @internal This function updates tail values.
 */
static __rte_always_inline void
__rte_ring_rts_update_tail(struct rte_ring_rts_headtail *ht)
{
	union __rte_ring_rts_poscnt h, ot, nt;

	/*
	 * If there are other enqueues/dequeues in progress that
	 * might preceded us, then don't update tail with new value.
	 */

	/*
	 * A0 = {A0.a, A0.b}: Synchronizes with the CAS at R0.
	 * The CAS at R0 in same typed thread establishes a happens-before
	 * relationship with this load acquire. Ensures that this thread
	 * observes the same or later values for h.raw/h.val.cnt
	 * observed by the other thread when it updated ht->tail.raw.
	 * If not, ht->tail.raw may get updated out of sync (e.g. getting
	 * updated to the same value twice). A0.a makes sure this condition
	 * holds when CAS succeeds and A0.b when it fails.
	 */
	/* A0.a */
	ot.raw = rte_atomic_load_explicit(&ht->tail.raw, rte_memory_order_acquire);

	do {
		/* on 32-bit systems we have to do atomic read here */
		h.raw = rte_atomic_load_explicit(&ht->head.raw, rte_memory_order_relaxed);

		nt.raw = ot.raw;
		if (++nt.val.cnt == h.val.cnt)
			nt.val.pos = h.val.pos;

	/*
	 * R0: Synchronizes with A2 of a different thread of the opposite type and A0.b
	 * of a different thread of the same type.
	 */
	/* A0.b */
	} while (rte_atomic_compare_exchange_strong_explicit(&ht->tail.raw,
			(uint64_t *)(uintptr_t)&ot.raw, nt.raw,
			rte_memory_order_release, rte_memory_order_acquire) == 0);
}

/**
 * @internal This function waits till head/tail distance wouldn't
 * exceed pre-defined max value.
 */
static __rte_always_inline union __rte_ring_rts_poscnt
__rte_ring_rts_head_wait(const struct rte_ring_rts_headtail *ht,
			 int memorder)
{
	union __rte_ring_rts_poscnt h;
	uint32_t max = ht->htd_max;


	h.raw = rte_atomic_load_explicit(&ht->head.raw, memorder);

	while (h.val.pos - ht->tail.val.pos > max) {
		rte_pause();
		h.raw = rte_atomic_load_explicit(&ht->head.raw, memorder);
	}

	return h;
}

/**
 * @internal This function updates the producer head for enqueue.
 */
static __rte_always_inline uint32_t
__rte_ring_rts_move_prod_head(struct rte_ring *r, uint32_t num,
	enum rte_ring_queue_behavior behavior, uint32_t *old_head,
	uint32_t *free_entries)
{
	uint32_t n, cons_tail;
	union __rte_ring_rts_poscnt nh, oh;

	const uint32_t capacity = r->capacity;

	do {
		/* Reset n to the initial burst count */
		n = num;

		/*
		 * wait for prod head/tail distance,
		 * make sure that we read prod head *before*
		 * reading cons tail.
		 */
		/*
		 * A1 Synchronizes with the CAS at R1.
		 * Establishes a happens-before relationship with a thread of the same
		 * type that released the ht.raw, ensuring this thread observes all of
		 * its memory effects needed to maintain a safe partial order.
		 */
		oh = __rte_ring_rts_head_wait(&r->rts_prod, rte_memory_order_acquire);

		/*
		 * A2: Establish a synchronizes-with edge using a store-release at R0.
		 * This ensures that all memory effects from the preceding opposing
		 * thread are observed.
		 */
		cons_tail = rte_atomic_load_explicit(&r->cons.tail, rte_memory_order_acquire);

		/*
		 *  The subtraction is done between two unsigned 32bits value
		 * (the result is always modulo 32 bits even if we have
		 * *old_head > cons_tail). So 'free_entries' is always between 0
		 * and capacity (which is < size).
		 */
		*free_entries = capacity + cons_tail - oh.val.pos;

		/* check that we have enough room in ring */
		if (unlikely(n > *free_entries))
			n = (behavior == RTE_RING_QUEUE_FIXED) ?
					0 : *free_entries;

		if (n == 0)
			break;

		nh.val.pos = oh.val.pos + n;
		nh.val.cnt = oh.val.cnt + 1;

	/*
	 * R1: Establishes a synchronizes-with edge with the load-acquire
	 * of ht.raw at A1. Ensures that the store-release to the tail by
	 * this thread, if it was of the opposite type, becomes
	 * visible to another thread of the current type. That thread will
	 * then observe the updates in the same order, keeping a safe
	 * partial order.
	 */
	} while (rte_atomic_compare_exchange_strong_explicit(&r->rts_prod.head.raw,
			(uint64_t *)(uintptr_t)&oh.raw, nh.raw,
			rte_memory_order_release, rte_memory_order_relaxed) == 0);

	*old_head = oh.val.pos;
	return n;
}

/**
 * @internal This function updates the consumer head for dequeue
 */
static __rte_always_inline unsigned int
__rte_ring_rts_move_cons_head(struct rte_ring *r, uint32_t num,
	enum rte_ring_queue_behavior behavior, uint32_t *old_head,
	uint32_t *entries)
{
	uint32_t n, prod_tail;
	union __rte_ring_rts_poscnt nh, oh;

	/* move cons.head atomically */
	do {
		/* Restore n as it may change every loop */
		n = num;

		/*
		 * wait for cons head/tail distance,
		 * make sure that we read cons head *before*
		 * reading prod tail.
		 */
		/*
		 * A3: Synchronizes with the CAS at R2.
		 * Establishes a happens-before relationship with a thread of the same
		 * type that released the ht.raw, ensuring this thread observes all of
		 * its memory effects needed to maintain a safe partial order.
		 */
		oh = __rte_ring_rts_head_wait(&r->rts_cons, rte_memory_order_acquire);

		/*
		 * A4: Establish a synchronizes-with edge using a store-release at R0.
		 * This ensures that all memory effects from the preceding opposing
		 * thread are observed.
		 */
		prod_tail = rte_atomic_load_explicit(&r->prod.tail, rte_memory_order_acquire);

		/* The subtraction is done between two unsigned 32bits value
		 * (the result is always modulo 32 bits even if we have
		 * cons_head > prod_tail). So 'entries' is always between 0
		 * and size(ring)-1.
		 */
		*entries = prod_tail - oh.val.pos;

		/* Set the actual entries for dequeue */
		if (n > *entries)
			n = (behavior == RTE_RING_QUEUE_FIXED) ? 0 : *entries;

		if (unlikely(n == 0))
			break;

		nh.val.pos = oh.val.pos + n;
		nh.val.cnt = oh.val.cnt + 1;

	/*
	 * R2: Establishes a synchronizes-with edge with the load-acquire
	 * of ht.raw at A3. Ensures that the store-release to the tail by
	 * this thread, if it was of the opposite type, becomes
	 * visible to another thread of the current type. That thread will
	 * then observe the updates in the same order, keeping a safe
	 * partial order.
	 */
	} while (rte_atomic_compare_exchange_strong_explicit(&r->rts_cons.head.raw,
			(uint64_t *)(uintptr_t)&oh.raw, nh.raw,
			rte_memory_order_release, rte_memory_order_relaxed) == 0);

	*old_head = oh.val.pos;
	return n;
}

/**
 * @internal Enqueue several objects on the RTS ring.
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
__rte_ring_do_rts_enqueue_elem(struct rte_ring *r, const void *obj_table,
	uint32_t esize, uint32_t n, enum rte_ring_queue_behavior behavior,
	uint32_t *free_space)
{
	uint32_t free, head;

	n =  __rte_ring_rts_move_prod_head(r, n, behavior, &head, &free);

	if (n != 0) {
		__rte_ring_enqueue_elems(r, head, obj_table, esize, n);
		__rte_ring_rts_update_tail(&r->rts_prod);
	}

	if (free_space != NULL)
		*free_space = free - n;
	return n;
}

/**
 * @internal Dequeue several objects from the RTS ring.
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
__rte_ring_do_rts_dequeue_elem(struct rte_ring *r, void *obj_table,
	uint32_t esize, uint32_t n, enum rte_ring_queue_behavior behavior,
	uint32_t *available)
{
	uint32_t entries, head;

	n = __rte_ring_rts_move_cons_head(r, n, behavior, &head, &entries);

	if (n != 0) {
		__rte_ring_dequeue_elems(r, head, obj_table, esize, n);
		__rte_ring_rts_update_tail(&r->rts_cons);
	}

	if (available != NULL)
		*available = entries - n;
	return n;
}

#endif /* _RTE_RING_RTS_ELEM_PVT_H_ */
