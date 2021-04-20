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

#include <rte_lcore.h>
#include <rte_common.h>
#include <rte_pause.h>
#include <rte_branch_prediction.h>

/**
 * The rte_mcslock_t type.
 */
typedef struct rte_mcslock {
	struct rte_mcslock *next;
	int locked; /* 1 if the queue locked, 0 otherwise */
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
rte_mcslock_lock(rte_mcslock_t **msl, rte_mcslock_t *me)
{
	rte_mcslock_t *prev;

	/* Init me node */
	__atomic_store_n(&me->locked, 1, __ATOMIC_RELAXED);
	__atomic_store_n(&me->next, NULL, __ATOMIC_RELAXED);

	/* If the queue is empty, the exchange operation is enough to acquire
	 * the lock. Hence, the exchange operation requires acquire semantics.
	 * The store to me->next above should complete before the node is
	 * visible to other CPUs/threads. Hence, the exchange operation requires
	 * release semantics as well.
	 */
	prev = __atomic_exchange_n(msl, me, __ATOMIC_ACQ_REL);
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
	__atomic_store_n(&prev->next, me, __ATOMIC_RELEASE);

	/* The while-load of me->locked should not move above the previous
	 * store to prev->next. Otherwise it will cause a deadlock. Need a
	 * store-load barrier.
	 */
	__atomic_thread_fence(__ATOMIC_ACQ_REL);
	/* If the lock has already been acquired, it first atomically
	 * places the node at the end of the queue and then proceeds
	 * to spin on me->locked until the previous lock holder resets
	 * the me->locked using mcslock_unlock().
	 */
	while (__atomic_load_n(&me->locked, __ATOMIC_ACQUIRE))
		rte_pause();
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
rte_mcslock_unlock(rte_mcslock_t **msl, rte_mcslock_t *me)
{
	/* Check if there are more nodes in the queue. */
	if (likely(__atomic_load_n(&me->next, __ATOMIC_RELAXED) == NULL)) {
		/* No, last member in the queue. */
		rte_mcslock_t *save_me = __atomic_load_n(&me, __ATOMIC_RELAXED);

		/* Release the lock by setting it to NULL */
		if (likely(__atomic_compare_exchange_n(msl, &save_me, NULL, 0,
				__ATOMIC_RELEASE, __ATOMIC_RELAXED)))
			return;

		/* Speculative execution would be allowed to read in the
		 * while-loop first. This has the potential to cause a
		 * deadlock. Need a load barrier.
		 */
		__atomic_thread_fence(__ATOMIC_ACQUIRE);
		/* More nodes added to the queue by other CPUs.
		 * Wait until the next pointer is set.
		 */
		while (__atomic_load_n(&me->next, __ATOMIC_RELAXED) == NULL)
			rte_pause();
	}

	/* Pass lock to next waiter. */
	__atomic_store_n(&me->next->locked, 0, __ATOMIC_RELEASE);
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
rte_mcslock_trylock(rte_mcslock_t **msl, rte_mcslock_t *me)
{
	/* Init me node */
	__atomic_store_n(&me->next, NULL, __ATOMIC_RELAXED);

	/* Try to lock */
	rte_mcslock_t *expected = NULL;

	/* The lock can be taken only when the queue is empty. Hence,
	 * the compare-exchange operation requires acquire semantics.
	 * The store to me->next above should complete before the node
	 * is visible to other CPUs/threads. Hence, the compare-exchange
	 * operation requires release semantics as well.
	 */
	return __atomic_compare_exchange_n(msl, &expected, me, 0,
			__ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
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
rte_mcslock_is_locked(rte_mcslock_t *msl)
{
	return (__atomic_load_n(&msl, __ATOMIC_RELAXED) != NULL);
}

#endif /* _RTE_MCSLOCK_H_ */
