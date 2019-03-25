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
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_lcore.h>
#include <rte_pause.h>

/**
 * The rte_ticketlock_t type.
 */
typedef union {
	uint32_t tickets;
	struct {
		uint16_t current;
		uint16_t next;
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
static inline __rte_experimental void
rte_ticketlock_init(rte_ticketlock_t *tl)
{
	__atomic_store_n(&tl->tickets, 0, __ATOMIC_RELAXED);
}

/**
 * Take the ticketlock.
 *
 * @param tl
 *   A pointer to the ticketlock.
 */
static inline __rte_experimental void
rte_ticketlock_lock(rte_ticketlock_t *tl)
{
	uint16_t me = __atomic_fetch_add(&tl->s.next, 1, __ATOMIC_RELAXED);
	while (__atomic_load_n(&tl->s.current, __ATOMIC_ACQUIRE) != me)
		rte_pause();
}

/**
 * Release the ticketlock.
 *
 * @param tl
 *   A pointer to the ticketlock.
 */
static inline __rte_experimental void
rte_ticketlock_unlock(rte_ticketlock_t *tl)
{
	uint16_t i = __atomic_load_n(&tl->s.current, __ATOMIC_RELAXED);
	__atomic_store_n(&tl->s.current, i + 1, __ATOMIC_RELEASE);
}

/**
 * Try to take the lock.
 *
 * @param tl
 *   A pointer to the ticketlock.
 * @return
 *   1 if the lock is successfully taken; 0 otherwise.
 */
static inline __rte_experimental int
rte_ticketlock_trylock(rte_ticketlock_t *tl)
{
	rte_ticketlock_t old, new;
	old.tickets = __atomic_load_n(&tl->tickets, __ATOMIC_RELAXED);
	new.tickets = old.tickets;
	new.s.next++;
	if (old.s.next == old.s.current) {
		if (__atomic_compare_exchange_n(&tl->tickets, &old.tickets,
		    new.tickets, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
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
static inline __rte_experimental int
rte_ticketlock_is_locked(rte_ticketlock_t *tl)
{
	rte_ticketlock_t tic;
	tic.tickets = __atomic_load_n(&tl->tickets, __ATOMIC_ACQUIRE);
	return (tic.s.current != tic.s.next);
}

/**
 * The rte_ticketlock_recursive_t type.
 */
#define TICKET_LOCK_INVALID_ID -1

typedef struct {
	rte_ticketlock_t tl; /**< the actual ticketlock */
	int user; /**< core id using lock, TICKET_LOCK_INVALID_ID for unused */
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
static inline __rte_experimental void
rte_ticketlock_recursive_init(rte_ticketlock_recursive_t *tlr)
{
	rte_ticketlock_init(&tlr->tl);
	__atomic_store_n(&tlr->user, TICKET_LOCK_INVALID_ID, __ATOMIC_RELAXED);
	tlr->count = 0;
}

/**
 * Take the recursive ticketlock.
 *
 * @param tlr
 *   A pointer to the recursive ticketlock.
 */
static inline __rte_experimental void
rte_ticketlock_recursive_lock(rte_ticketlock_recursive_t *tlr)
{
	int id = rte_gettid();

	if (__atomic_load_n(&tlr->user, __ATOMIC_RELAXED) != id) {
		rte_ticketlock_lock(&tlr->tl);
		__atomic_store_n(&tlr->user, id, __ATOMIC_RELAXED);
	}
	tlr->count++;
}

/**
 * Release the recursive ticketlock.
 *
 * @param tlr
 *   A pointer to the recursive ticketlock.
 */
static inline __rte_experimental void
rte_ticketlock_recursive_unlock(rte_ticketlock_recursive_t *tlr)
{
	if (--(tlr->count) == 0) {
		__atomic_store_n(&tlr->user, TICKET_LOCK_INVALID_ID,
				 __ATOMIC_RELAXED);
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
static inline __rte_experimental int
rte_ticketlock_recursive_trylock(rte_ticketlock_recursive_t *tlr)
{
	int id = rte_gettid();

	if (__atomic_load_n(&tlr->user, __ATOMIC_RELAXED) != id) {
		if (rte_ticketlock_trylock(&tlr->tl) == 0)
			return 0;
		__atomic_store_n(&tlr->user, id, __ATOMIC_RELAXED);
	}
	tlr->count++;
	return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_TICKETLOCK_H_ */
