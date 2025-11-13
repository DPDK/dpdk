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

	/*
	 * A0/R0: Queue might be empty, perform the exchange (RMW) with both acquire and
	 * release semantics:
	 * A0: Acquire — synchronizes with both R0 and R2.
	 *     Must synchronize with R0 to ensure that this thread observes predecessor's
	 *     initialization of its lock object or risk them overwriting this thread's
	 *     update to the next of the same object via store to prev->next.
	 *
	 *     Must synchronize with R2 the releasing CAS in unlock(), this will ensure
	 *     that all prior critical-section writes become visible to this thread.
	 *
	 * R0: Release — ensures the successor observes our initialization of me->next;
	 *     without it, me->next could be overwritten to NULL after the successor
	 *     sets its own address, causing deadlock. This release synchronizes with
	 *     A0 above.
	 */
	prev = __atomic_exchange_n(msl, me, __ATOMIC_ACQ_REL);
	if (likely(prev == NULL)) {
		/* Queue was empty, no further action required,
		 * proceed with lock taken.
		 */
		return;
	}

	/*
	 * R1: With the relaxed memory model of C/C++, it's essential that after
	 * we link ourselves by storing prev->next = me, the owner of prev must
	 * observe our prior initialization of me->locked. Otherwise it could
	 * clear me->locked before we set it to 1, which may deadlock.
	 * Perform a releasing store so the predecessor's acquire loads A2 and A3
	 * observes our initialization, establishing a happens-before from those
	 * writes.
	 */
	__atomic_store_n(&prev->next, me, __ATOMIC_RELEASE);

	/*
	 * A1: If the lock has already been acquired, it first atomically
	 * places the node at the end of the queue and then proceeds
	 * to spin on me->locked until the previous lock holder resets
	 * the me->locked in rte_mcslock_unlock().
	 * This load must synchronize with store-release R3 to ensure that
	 * all updates to critical section by previous lock holder is visible
	 * to this thread after acquiring the lock.
	 */
	rte_wait_until_equal_32((uint32_t *)&me->locked, 0, __ATOMIC_ACQUIRE);
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
	/*
	 * A2: Check whether a successor is already queued.
	 * Load me->next with acquire semantics so it can synchronize with the
	 * successor’s release store R1. This guarantees that the successor’s
	 * initialization of its lock object (me) is completed before we observe
	 * it here, preventing a race between this thread’s store-release to
	 * me->next->locked and the successor’s store to me->locked.
	 */
	if (likely(__atomic_load_n(&me->next, __ATOMIC_ACQUIRE) == NULL)) {
		/* No, last member in the queue. */
		rte_mcslock_t *save_me = me;

		/*
		 * R2: Try to release the lock by swinging *msl from save_me to NULL.
		 * Use release semantics so all critical section writes become
		 * visible to the next lock acquirer.
		 */
		if (likely(__atomic_compare_exchange_n(msl, &save_me, NULL, 0,
				__ATOMIC_RELEASE, __ATOMIC_RELAXED)))
			return;

		/*
		 * A3: Another thread was enqueued concurrently, so the CAS and the lock
		 * release failed. Wait until the successor sets our 'next' pointer.
		 * This load must synchronize with the successor’s release store (R1) to
		 * ensure that the successor’s initialization completes before we observe
		 * it here. This ordering prevents a race between this thread’s later
		 * store-release to me->next->locked and the successor’s store to me->locked.
		 */
		uintptr_t *next;
		next = (uintptr_t *)&me->next;
		RTE_WAIT_UNTIL_MASKED(next, UINTPTR_MAX, !=, 0, __ATOMIC_ACQUIRE);
	}

	/*
	 * R3: Pass the lock to the successor.
	 * Use a release store to synchronize with A1 when clearing me->next->locked
	 * so the successor observes our critical section writes after it sees locked
	 * become 0.
	 */
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

	/*
	 * A4/R4: The lock can be acquired only when the queue is empty.
	 * The compare-and-exchange operation must use acquire and release
	 * semantics for the same reasons described in the rte_mcslock_lock()
	 * function’s empty-queue case (see A0/R0 for details).
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

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MCSLOCK_H_ */
