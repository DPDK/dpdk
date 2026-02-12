/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2017,2018 HXT-semitech Corporation.
 * Copyright (c) 2007-2009 Kip Macy kmacy@freebsd.org
 * Copyright (c) 2021 Arm Limited
 * All rights reserved.
 * Derived from FreeBSD's bufring.h
 * Used as BSD-3 Licensed with permission from Kip Macy.
 */

#ifndef _RTE_RING_C11_PVT_H_
#define _RTE_RING_C11_PVT_H_

/**
 * @file rte_ring_c11_pvt.h
 * It is not recommended to include this file directly,
 * include <rte_ring.h> instead.
 * Contains internal helper functions for MP/SP and MC/SC ring modes.
 * For more information please refer to <rte_ring.h>.
 */

/**
 * @internal This function updates tail values.
 */
static __rte_always_inline void
__rte_ring_update_tail(struct rte_ring_headtail *ht, uint32_t old_val,
		uint32_t new_val, uint32_t single, uint32_t enqueue)
{
	RTE_SET_USED(enqueue);

	/*
	 * If there are other enqueues/dequeues in progress that preceded us,
	 * we need to wait for them to complete
	 */
	if (!single)
		rte_wait_until_equal_32((uint32_t *)(uintptr_t)&ht->tail, old_val,
			rte_memory_order_relaxed);

	/*
	 * R0: Establishes a synchronizing edge with load-acquire of tail at A1.
	 * Ensures that memory effects by this thread on ring elements array
	 * is observed by a different thread of the other type.
	 */
	rte_atomic_store_explicit(&ht->tail, new_val, rte_memory_order_release);
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
 * @param is_st
 *   Indicates whether multi-thread safe path is needed or not
 * @param n
 *   The number of elements we want to move head value on
 * @param behavior
 *   RTE_RING_QUEUE_FIXED:    Move on a fixed number of items
 *   RTE_RING_QUEUE_VARIABLE: Move on as many items as possible
 * @param old_head
 *   Returns head value as it was before the move
 * @param new_head
 *   Returns the new head value
 * @param entries
 *   Returns the number of ring entries available BEFORE head was moved
 * @return
 *   Actual number of objects the head was moved on
 *   If behavior == RTE_RING_QUEUE_FIXED, this will be 0 or n only
 */
static __rte_always_inline unsigned int
__rte_ring_headtail_move_head(struct rte_ring_headtail *d,
		const struct rte_ring_headtail *s, uint32_t capacity,
		unsigned int is_st, unsigned int n,
		enum rte_ring_queue_behavior behavior,
		uint32_t *old_head, uint32_t *new_head, uint32_t *entries)
{
	uint32_t stail;
	int success;
	unsigned int max = n;

	/*
	 * A0: Establishes a synchronizing edge with R1.
	 * Ensure that this thread observes same values
	 * to stail observed by the thread that updated
	 * d->head.
	 * If not, an unsafe partial order may ensue.
	 */
	*old_head = rte_atomic_load_explicit(&d->head,
			rte_memory_order_acquire);
	do {
		/* Reset n to the initial burst count */
		n = max;

		/*
		 * A1: Establishes a synchronizing edge with R0.
		 * Ensures that other thread's memory effects on
		 * ring elements array is observed by the time
		 * this thread observes its tail update.
		 */
		stail = rte_atomic_load_explicit(&s->tail,
					rte_memory_order_acquire);

		/* The subtraction is done between two unsigned 32bits value
		 * (the result is always modulo 32 bits even if we have
		 * *old_head > s->tail). So 'entries' is always between 0
		 * and capacity (which is < size).
		 */
		*entries = (capacity + stail - *old_head);

		/* check that we have enough room in ring */
		if (unlikely(n > *entries))
			n = (behavior == RTE_RING_QUEUE_FIXED) ?
					0 : *entries;

		if (n == 0)
			return 0;

		*new_head = *old_head + n;
		if (is_st) {
			d->head = *new_head;
			success = 1;
		} else
			/* on failure, *old_head is updated */
			/*
			 * R1/A2.
			 * R1: Establishes a synchronizing edge with A0 of a
			 * different thread.
			 * A2: Establishes a synchronizing edge with R1 of a
			 * different thread to observe same value for stail
			 * observed by that thread on CAS failure (to retry
			 * with an updated *old_head).
			 */
			success = rte_atomic_compare_exchange_strong_explicit(
					&d->head, old_head, *new_head,
					rte_memory_order_release,
					rte_memory_order_acquire);
	} while (unlikely(success == 0));
	return n;
}

#endif /* _RTE_RING_C11_PVT_H_ */
