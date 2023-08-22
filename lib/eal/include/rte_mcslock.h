/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Arm Limited
 */

#ifndef _RTE_MCSLOCK_H_
#define _RTE_MCSLOCK_H_

/**
 * @file
 *
 * RTE MCS lock
 *
 * This file defines the main data structure and APIs for MCS queued lock.
 *
 * The MCS lock (proposed by John M. Mellor-Crummey and Michael L. Scott)
 * provides scalability by spinning on a CPU/thread local variable which
 * avoids expensive cache bouncings. It provides fairness by maintaining
 * a list of acquirers and passing the lock to each CPU/thread in the order
 * they acquired the lock.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_pause.h>
#include <rte_branch_prediction.h>
#include <rte_stdatomic.h>

/**
 * The rte_mcslock_t type.
 */
typedef struct rte_mcslock {
	RTE_ATOMIC(struct rte_mcslock *) next;
	RTE_ATOMIC(int) locked; /* 1 if the queue locked, 0 otherwise */
} rte_mcslock_t;

/**
 * Take the MCS lock.
 *
 * @param msl
 *   A pointer to the pointer of a MCS lock.
 *   When the lock is initialized or declared, the msl pointer should be
 *   set to NULL.
 * @param me
 *   A pointer to a new node of MCS lock. Each CPU/thread acquiring the
 *   lock should use its 'own node'.
 */
static inline void
rte_mcslock_lock(RTE_ATOMIC(rte_mcslock_t *) *msl, rte_mcslock_t *me)
{
	rte_mcslock_t *prev;

	/* Init me node */
	rte_atomic_store_explicit(&me->locked, 1, rte_memory_order_relaxed);
	rte_atomic_store_explicit(&me->next, NULL, rte_memory_order_relaxed);

	/* If the queue is empty, the exchange operation is enough to acquire
	 * the lock. Hence, the exchange operation requires acquire semantics.
	 * The store to me->next above should complete before the node is
	 * visible to other CPUs/threads. Hence, the exchange operation requires
	 * release semantics as well.
	 */
	prev = rte_atomic_exchange_explicit(msl, me, rte_memory_order_acq_rel);
	if (likely(prev == NULL)) {
		/* Queue was empty, no further action required,
		 * proceed with lock taken.
		 */
		return;
	}
	/* The store to me->next above should also complete before the node is
	 * visible to predecessor thread releasing the lock. Hence, the store
	 * prev->next also requires release semantics. Note that, for example,
	 * on ARM, the release semantics in the exchange operation is not
	 * strong as a release fence and is not sufficient to enforce the
	 * desired order here.
	 */
	rte_atomic_store_explicit(&prev->next, me, rte_memory_order_release);

	/* The while-load of me->locked should not move above the previous
	 * store to prev->next. Otherwise it will cause a deadlock. Need a
	 * store-load barrier.
	 */
	__rte_atomic_thread_fence(rte_memory_order_acq_rel);
	/* If the lock has already been acquired, it first atomically
	 * places the node at the end of the queue and then proceeds
	 * to spin on me->locked until the previous lock holder resets
	 * the me->locked using mcslock_unlock().
	 */
	rte_wait_until_equal_32((uint32_t *)(uintptr_t)&me->locked, 0, rte_memory_order_acquire);
}

/**
 * Release the MCS lock.
 *
 * @param msl
 *   A pointer to the pointer of a MCS lock.
 * @param me
 *   A pointer to the node of MCS lock passed in rte_mcslock_lock.
 */
static inline void
rte_mcslock_unlock(RTE_ATOMIC(rte_mcslock_t *) *msl, RTE_ATOMIC(rte_mcslock_t *) me)
{
	/* Check if there are more nodes in the queue. */
	if (likely(rte_atomic_load_explicit(&me->next, rte_memory_order_relaxed) == NULL)) {
		/* No, last member in the queue. */
		rte_mcslock_t *save_me = rte_atomic_load_explicit(&me, rte_memory_order_relaxed);

		/* Release the lock by setting it to NULL */
		if (likely(rte_atomic_compare_exchange_strong_explicit(msl, &save_me, NULL,
				rte_memory_order_release, rte_memory_order_relaxed)))
			return;

		/* Speculative execution would be allowed to read in the
		 * while-loop first. This has the potential to cause a
		 * deadlock. Need a load barrier.
		 */
		__rte_atomic_thread_fence(rte_memory_order_acquire);
		/* More nodes added to the queue by other CPUs.
		 * Wait until the next pointer is set.
		 */
		RTE_ATOMIC(uintptr_t) *next;
		next = (__rte_atomic uintptr_t *)&me->next;
		RTE_WAIT_UNTIL_MASKED(next, UINTPTR_MAX, !=, 0, rte_memory_order_relaxed);
	}

	/* Pass lock to next waiter. */
	rte_atomic_store_explicit(&me->next->locked, 0, rte_memory_order_release);
}

/**
 * Try to take the lock.
 *
 * @param msl
 *   A pointer to the pointer of a MCS lock.
 * @param me
 *   A pointer to a new node of MCS lock.
 * @return
 *   1 if the lock is successfully taken; 0 otherwise.
 */
static inline int
rte_mcslock_trylock(RTE_ATOMIC(rte_mcslock_t *) *msl, rte_mcslock_t *me)
{
	/* Init me node */
	rte_atomic_store_explicit(&me->next, NULL, rte_memory_order_relaxed);

	/* Try to lock */
	rte_mcslock_t *expected = NULL;

	/* The lock can be taken only when the queue is empty. Hence,
	 * the compare-exchange operation requires acquire semantics.
	 * The store to me->next above should complete before the node
	 * is visible to other CPUs/threads. Hence, the compare-exchange
	 * operation requires release semantics as well.
	 */
	return rte_atomic_compare_exchange_strong_explicit(msl, &expected, me,
			rte_memory_order_acq_rel, rte_memory_order_relaxed);
}

/**
 * Test if the lock is taken.
 *
 * @param msl
 *   A pointer to a MCS lock node.
 * @return
 *   1 if the lock is currently taken; 0 otherwise.
 */
static inline int
rte_mcslock_is_locked(RTE_ATOMIC(rte_mcslock_t *) msl)
{
	return (rte_atomic_load_explicit(&msl, rte_memory_order_relaxed) != NULL);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MCSLOCK_H_ */
