/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Arm Limited
 */

#ifndef _RTE_TICKETLOCK_H_
#define _RTE_TICKETLOCK_H_

/**
 * @file
 *
 * RTE ticket locks
 *
 * This file defines an API for ticket locks, which give each waiting
 * thread a ticket and take the lock one by one, first come, first
 * serviced.
 *
 * All locks must be initialised before use, and only initialised once.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_lcore.h>
#include <rte_pause.h>
#include <rte_stdatomic.h>

/**
 * The rte_ticketlock_t type.
 */
typedef union {
	RTE_ATOMIC(uint32_t) tickets;
	struct {
		RTE_ATOMIC(uint16_t) current;
		RTE_ATOMIC(uint16_t) next;
	} s;
} rte_ticketlock_t;

/**
 * A static ticketlock initializer.
 */
#define RTE_TICKETLOCK_INITIALIZER { 0 }

/**
 * Initialize the ticketlock to an unlocked state.
 *
 * @param tl
 *   A pointer to the ticketlock.
 */
static inline void
rte_ticketlock_init(rte_ticketlock_t *tl)
{
	rte_atomic_store_explicit(&tl->tickets, 0, rte_memory_order_relaxed);
}

/**
 * Take the ticketlock.
 *
 * @param tl
 *   A pointer to the ticketlock.
 */
static inline void
rte_ticketlock_lock(rte_ticketlock_t *tl)
{
	uint16_t me = rte_atomic_fetch_add_explicit(&tl->s.next, 1, rte_memory_order_relaxed);
	rte_wait_until_equal_16((uint16_t *)(uintptr_t)&tl->s.current, me,
		rte_memory_order_acquire);
}

/**
 * Release the ticketlock.
 *
 * @param tl
 *   A pointer to the ticketlock.
 */
static inline void
rte_ticketlock_unlock(rte_ticketlock_t *tl)
{
	uint16_t i = rte_atomic_load_explicit(&tl->s.current, rte_memory_order_relaxed);
	rte_atomic_store_explicit(&tl->s.current, i + 1, rte_memory_order_release);
}

/**
 * Try to take the lock.
 *
 * @param tl
 *   A pointer to the ticketlock.
 * @return
 *   1 if the lock is successfully taken; 0 otherwise.
 */
static inline int
rte_ticketlock_trylock(rte_ticketlock_t *tl)
{
	rte_ticketlock_t oldl, newl;
	oldl.tickets = rte_atomic_load_explicit(&tl->tickets, rte_memory_order_relaxed);
	newl.tickets = oldl.tickets;
	newl.s.next++;
	if (oldl.s.next == oldl.s.current) {
		if (rte_atomic_compare_exchange_strong_explicit(&tl->tickets,
				(uint32_t *)(uintptr_t)&oldl.tickets, newl.tickets,
				rte_memory_order_acquire, rte_memory_order_relaxed))
			return 1;
	}

	return 0;
}

/**
 * Test if the lock is taken.
 *
 * @param tl
 *   A pointer to the ticketlock.
 * @return
 *   1 if the lock is currently taken; 0 otherwise.
 */
static inline int
rte_ticketlock_is_locked(rte_ticketlock_t *tl)
{
	rte_ticketlock_t tic;
	tic.tickets = rte_atomic_load_explicit(&tl->tickets, rte_memory_order_acquire);
	return (tic.s.current != tic.s.next);
}

/**
 * The rte_ticketlock_recursive_t type.
 */
#define TICKET_LOCK_INVALID_ID -1

typedef struct {
	rte_ticketlock_t tl; /**< the actual ticketlock */
	RTE_ATOMIC(int) user; /**< core id using lock, TICKET_LOCK_INVALID_ID for unused */
	unsigned int count; /**< count of time this lock has been called */
} rte_ticketlock_recursive_t;

/**
 * A static recursive ticketlock initializer.
 */
#define RTE_TICKETLOCK_RECURSIVE_INITIALIZER {RTE_TICKETLOCK_INITIALIZER, \
					      TICKET_LOCK_INVALID_ID, 0}

/**
 * Initialize the recursive ticketlock to an unlocked state.
 *
 * @param tlr
 *   A pointer to the recursive ticketlock.
 */
static inline void
rte_ticketlock_recursive_init(rte_ticketlock_recursive_t *tlr)
{
	rte_ticketlock_init(&tlr->tl);
	rte_atomic_store_explicit(&tlr->user, TICKET_LOCK_INVALID_ID, rte_memory_order_relaxed);
	tlr->count = 0;
}

/**
 * Take the recursive ticketlock.
 *
 * @param tlr
 *   A pointer to the recursive ticketlock.
 */
static inline void
rte_ticketlock_recursive_lock(rte_ticketlock_recursive_t *tlr)
{
	int id = rte_gettid();

	if (rte_atomic_load_explicit(&tlr->user, rte_memory_order_relaxed) != id) {
		rte_ticketlock_lock(&tlr->tl);
		rte_atomic_store_explicit(&tlr->user, id, rte_memory_order_relaxed);
	}
	tlr->count++;
}

/**
 * Release the recursive ticketlock.
 *
 * @param tlr
 *   A pointer to the recursive ticketlock.
 */
static inline void
rte_ticketlock_recursive_unlock(rte_ticketlock_recursive_t *tlr)
{
	if (--(tlr->count) == 0) {
		rte_atomic_store_explicit(&tlr->user, TICKET_LOCK_INVALID_ID,
				 rte_memory_order_relaxed);
		rte_ticketlock_unlock(&tlr->tl);
	}
}

/**
 * Try to take the recursive lock.
 *
 * @param tlr
 *   A pointer to the recursive ticketlock.
 * @return
 *   1 if the lock is successfully taken; 0 otherwise.
 */
static inline int
rte_ticketlock_recursive_trylock(rte_ticketlock_recursive_t *tlr)
{
	int id = rte_gettid();

	if (rte_atomic_load_explicit(&tlr->user, rte_memory_order_relaxed) != id) {
		if (rte_ticketlock_trylock(&tlr->tl) == 0)
			return 0;
		rte_atomic_store_explicit(&tlr->user, id, rte_memory_order_relaxed);
	}
	tlr->count++;
	return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_TICKETLOCK_H_ */
