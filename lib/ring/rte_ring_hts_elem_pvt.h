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

	/*
	 * R0: Release the tail update. Establishes a synchronization edge with
	 * the load-acquire at A1/A3. This release ensures that all updates to
	 * *ht and the ring array made by this thread become visible to the
	 * opposing thread once the tail value written here is observed.
	 */
	rte_atomic_store_explicit(&ht->ht.pos.tail, tail, rte_memory_order_release);
}

/**
 * @internal
 * Waits until the tail becomes equal to the head.
 * This indicates that another thread has finished its transaction, and there
 * is a chance that we could be the next writer or reader in line.
 *
 * Returns ht.raw at this point. The value may be imprecise, since another
 * thread might change the state before we observe ht.raw, but that does not
 * matter. The function __rte_ring_hts_move_head() can detect and recall this
 * function when it reaches the linearization point (CAS).
 */
static __rte_always_inline union __rte_ring_hts_pos
__rte_ring_hts_head_wait(const struct rte_ring_hts_headtail *ht,
			 int memorder)
{
	union __rte_ring_hts_pos p;
	p.raw = rte_atomic_load_explicit(&ht->ht.raw, memorder);

	while (p.pos.head != p.pos.tail) {
		rte_pause();
		p.raw = rte_atomic_load_explicit(&ht->ht.raw, memorder);
	}

	return p;
}

/**
 * @internal This function updates the producer head for enqueue
 */
static __rte_always_inline unsigned int
__rte_ring_hts_move_prod_head(struct rte_ring *r, unsigned int num,
	enum rte_ring_queue_behavior behavior, uint32_t *old_head,
	uint32_t *free_entries)
{
	uint32_t n, cons_tail;
	union __rte_ring_hts_pos np, op;

	const uint32_t capacity = r->capacity;

	do {
		/* Reset n to the initial burst count */
		n = num;

		/*
		 * wait for tail to be equal to head,
		 * make sure that we read prod head/tail *before*
		 * reading cons tail.
		 */
		/*
		 * A0: Synchronizes with the CAS at R1.
		 * Establishes a happens-before relationship with a thread of the same
		 * type that released the ht.raw, ensuring this thread observes all of
		 * its memory effects needed to maintain a safe partial order.
		 */
		op = __rte_ring_hts_head_wait(&r->hts_prod, rte_memory_order_acquire);

		/*
		 * A1: Establish a synchronizes-with edge using a store-release at R0.
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
		*free_entries = capacity + cons_tail - op.pos.head;

		/* check that we have enough room in ring */
		if (unlikely(n > *free_entries))
			n = (behavior == RTE_RING_QUEUE_FIXED) ?
					0 : *free_entries;

		if (n == 0)
			break;

		np.pos.tail = op.pos.tail;
		np.pos.head = op.pos.head + n;

	/*
	 * R1: Establishes a synchronizes-with edge with the load-acquire
	 * of ht.raw at A0. This makes sure that the store-release to the
	 * tail by this thread, if it was of the opposite type, becomes
	 * visible to another thread of the current type. That thread will
	 * then observe the updates in the same order, keeping a safe
	 * partial order.
	 */
	} while (rte_atomic_compare_exchange_strong_explicit(&r->hts_prod.ht.raw,
			(uint64_t *)(uintptr_t)&op.raw, np.raw,
			rte_memory_order_release, rte_memory_order_relaxed) == 0);

	*old_head = op.pos.head;
	return n;
}

/**
 * @internal This function updates the consumer head for dequeue
 */
static __rte_always_inline unsigned int
__rte_ring_hts_move_cons_head(struct rte_ring *r, unsigned int num,
	enum rte_ring_queue_behavior behavior, uint32_t *old_head,
	uint32_t *entries)
{
	uint32_t n, prod_tail;
	union __rte_ring_hts_pos np, op;

	/* move cons.head atomically */
	do {
		/* Restore n as it may change every loop */
		n = num;

		/*
		 * wait for tail to be equal to head,
		 * make sure that we read cons head/tail *before*
		 * reading prod tail.
		 */
		/*
		 * A2: Synchronizes with the CAS at R2.
		 * Establishes a happens-before relationship with a thread of the same
		 * type that released the ht.raw, ensuring this thread observes all of
		 * its memory effects needed to maintain a safe partial order.
		 */
		op = __rte_ring_hts_head_wait(&r->hts_cons, rte_memory_order_acquire);

		/*
		 * A3: Establish a synchronizes-with edge using a store-release at R0.
		 * This ensures that all memory effects from the preceding opposing
		 * thread are observed.
		 */
		prod_tail = rte_atomic_load_explicit(&r->prod.tail, rte_memory_order_acquire);

		/* The subtraction is done between two unsigned 32bits value
		 * (the result is always modulo 32 bits even if we have
		 * cons_head > prod_tail). So 'entries' is always between 0
		 * and size(ring)-1.
		 */
		*entries = prod_tail - op.pos.head;

		/* Set the actual entries for dequeue */
		if (n > *entries)
			n = (behavior == RTE_RING_QUEUE_FIXED) ? 0 : *entries;

		if (unlikely(n == 0))
			break;

		np.pos.tail = op.pos.tail;
		np.pos.head = op.pos.head + n;

	/*
	 * R2: Establishes a synchronizes-with edge with the load-acquire
	 * of ht.raw at A2. This makes sure that the store-release to the
	 * tail by this thread, if it was of the opposite type, becomes
	 * visible to another thread of the current type. That thread will
	 * then observe the updates in the same order, keeping a safe
	 * partial order.
	 */
	} while (rte_atomic_compare_exchange_strong_explicit(&r->hts_cons.ht.raw,
			(uint64_t *)(uintptr_t)&op.raw, np.raw,
			rte_memory_order_release, rte_memory_order_relaxed) == 0);

	*old_head = op.pos.head;
	return n;
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
