/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_RWLOCK_H_
#define _RTE_RWLOCK_H_

/**
 * @file
 *
 * RTE Read-Write Locks
 *
 * This file defines an API for read-write locks. The lock is used to
 * protect data that allows multiple readers in parallel, but only
 * one writer. All readers are blocked until the writer is finished
 * writing.
 *
 * This version does not give preference to readers or writers
 * and does not starve either readers or writers.
 *
 * See also:
 *  https://locklessinc.com/articles/locks/
 */

#include <errno.h>
#include <stdbool.h>

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_lock_annotations.h>
#include <rte_pause.h>
#include <rte_stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The rte_rwlock_t type.
 *
 * Readers increment the counter by RTE_RWLOCK_READ (4)
 * Writers set the RTE_RWLOCK_WRITE bit when lock is held
 *     and set the RTE_RWLOCK_WAIT bit while waiting.
 *
 * 31                 2 1 0
 * +-------------------+-+-+
 * |  readers          | | |
 * +-------------------+-+-+
 *                      ^ ^
 *                      | |
 * WRITE: lock held ----/ |
 * WAIT: writer pending --/
 */

#define RTE_RWLOCK_WAIT	 0x1	/* Writer is waiting */
#define RTE_RWLOCK_WRITE 0x2	/* Writer has the lock */
#define RTE_RWLOCK_MASK  (RTE_RWLOCK_WAIT | RTE_RWLOCK_WRITE)
				/* Writer is waiting or has lock */
#define RTE_RWLOCK_READ	 0x4	/* Reader increment */

typedef struct __rte_capability("rwlock") {
	RTE_ATOMIC(int32_t) cnt;
} rte_rwlock_t;

/**
 * A static rwlock initializer.
 */
#define RTE_RWLOCK_INITIALIZER { 0 }

/**
 * Initialize the rwlock to an unlocked state.
 *
 * @param rwl
 *   A pointer to the rwlock structure.
 */
static inline void
rte_rwlock_init(rte_rwlock_t *rwl)
{
	rwl->cnt = 0;
}

/**
 * Take a read lock. Loop until the lock is held.
 *
 * @note The RW lock isn't recursive, so calling this function on the same
 * lock twice without releasing it could potentially result in a deadlock
 * scenario when a write lock is involved.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_read_lock(rte_rwlock_t *rwl)
	__rte_acquire_shared_capability(rwl)
	__rte_no_thread_safety_analysis
{
	int32_t x;

	while (1) {
		/* Wait while writer is present or pending */
		while (rte_atomic_load_explicit(&rwl->cnt, rte_memory_order_relaxed)
		       & RTE_RWLOCK_MASK)
			rte_pause();

		/* Try to get read lock */
		x = rte_atomic_fetch_add_explicit(&rwl->cnt, RTE_RWLOCK_READ,
				       rte_memory_order_acquire) + RTE_RWLOCK_READ;

		/* If no writer, then acquire was successful */
		if (likely(!(x & RTE_RWLOCK_MASK)))
			return;

		/* Lost race with writer, backout the change. */
		rte_atomic_fetch_sub_explicit(&rwl->cnt, RTE_RWLOCK_READ,
				   rte_memory_order_relaxed);
	}
}

/**
 * Try to take a read lock.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 * @return
 *   - zero if the lock is successfully taken
 *   - -EBUSY if lock could not be acquired for reading because a
 *     writer holds the lock
 */
static inline int
rte_rwlock_read_trylock(rte_rwlock_t *rwl)
	__rte_try_acquire_shared_capability(false, rwl)
	__rte_no_thread_safety_analysis
{
	int32_t x;

	x = rte_atomic_load_explicit(&rwl->cnt, rte_memory_order_relaxed);

	/* fail if write lock is held or writer is pending */
	if (x & RTE_RWLOCK_MASK)
		return -EBUSY;

	/* Try to get read lock */
	x = rte_atomic_fetch_add_explicit(&rwl->cnt, RTE_RWLOCK_READ,
			       rte_memory_order_acquire) + RTE_RWLOCK_READ;

	/* Back out if writer raced in */
	if (unlikely(x & RTE_RWLOCK_MASK)) {
		rte_atomic_fetch_sub_explicit(&rwl->cnt, RTE_RWLOCK_READ,
				   rte_memory_order_release);

		return -EBUSY;
	}
	return 0;
}

/**
 * Release a read lock.
 *
 * @param rwl
 *   A pointer to the rwlock structure.
 */
static inline void
rte_rwlock_read_unlock(rte_rwlock_t *rwl)
	__rte_release_shared_capability(rwl)
	__rte_no_thread_safety_analysis
{
	rte_atomic_fetch_sub_explicit(&rwl->cnt, RTE_RWLOCK_READ, rte_memory_order_release);
}

/**
 * Try to take a write lock.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 * @return
 *   - zero if the lock is successfully taken
 *   - -EBUSY if lock could not be acquired for writing because
 *     it was already locked for reading or writing
 */
static inline int
rte_rwlock_write_trylock(rte_rwlock_t *rwl)
	__rte_try_acquire_capability(false, rwl)
	__rte_no_thread_safety_analysis
{
	int32_t x;

	x = rte_atomic_load_explicit(&rwl->cnt, rte_memory_order_relaxed);
	if (x < RTE_RWLOCK_WRITE &&
	    rte_atomic_compare_exchange_weak_explicit(&rwl->cnt, &x, x + RTE_RWLOCK_WRITE,
					rte_memory_order_acquire, rte_memory_order_relaxed))
		return 0;
	else
		return -EBUSY;
}

/**
 * Take a write lock. Loop until the lock is held.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_write_lock(rte_rwlock_t *rwl)
	__rte_acquire_capability(rwl)
	__rte_no_thread_safety_analysis
{
	int32_t x;

	while (1) {
		x = rte_atomic_load_explicit(&rwl->cnt, rte_memory_order_relaxed);

		/* No readers or writers? */
		if (likely(x < RTE_RWLOCK_WRITE)) {
			/* Turn off RTE_RWLOCK_WAIT, turn on RTE_RWLOCK_WRITE */
			if (rte_atomic_compare_exchange_weak_explicit(&rwl->cnt, &x,
					RTE_RWLOCK_WRITE, rte_memory_order_acquire,
					rte_memory_order_relaxed))
				return;
		}

		/* Turn on writer wait bit */
		if (!(x & RTE_RWLOCK_WAIT))
			rte_atomic_fetch_or_explicit(&rwl->cnt, RTE_RWLOCK_WAIT,
				rte_memory_order_relaxed);

		/* Wait until no readers before trying again */
		while (rte_atomic_load_explicit(&rwl->cnt, rte_memory_order_relaxed)
				> RTE_RWLOCK_WAIT)
			rte_pause();

	}
}

/**
 * Release a write lock.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_write_unlock(rte_rwlock_t *rwl)
	__rte_release_capability(rwl)
	__rte_no_thread_safety_analysis
{
	rte_atomic_fetch_sub_explicit(&rwl->cnt, RTE_RWLOCK_WRITE, rte_memory_order_release);
}

/**
 * Test if the write lock is taken.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 * @return
 *   1 if the write lock is currently taken; 0 otherwise.
 */
static inline int
rte_rwlock_write_is_locked(rte_rwlock_t *rwl)
{
	if (rte_atomic_load_explicit(&rwl->cnt, rte_memory_order_relaxed) & RTE_RWLOCK_WRITE)
		return 1;

	return 0;
}

/**
 * Try to execute critical section in a hardware memory transaction, if it
 * fails or not available take a read lock
 *
 * NOTE: An attempt to perform a HW I/O operation inside a hardware memory
 * transaction always aborts the transaction since the CPU is not able to
 * roll-back should the transaction fail. Therefore, hardware transactional
 * locks are not advised to be used around rte_eth_rx_burst() and
 * rte_eth_tx_burst() calls.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_read_lock_tm(rte_rwlock_t *rwl)
	__rte_acquire_shared_capability(rwl);

/**
 * Commit hardware memory transaction or release the read lock if the lock is used as a fall-back
 *
 * @param rwl
 *   A pointer to the rwlock structure.
 */
static inline void
rte_rwlock_read_unlock_tm(rte_rwlock_t *rwl)
	__rte_release_shared_capability(rwl);

/**
 * Try to execute critical section in a hardware memory transaction, if it
 * fails or not available take a write lock
 *
 * NOTE: An attempt to perform a HW I/O operation inside a hardware memory
 * transaction always aborts the transaction since the CPU is not able to
 * roll-back should the transaction fail. Therefore, hardware transactional
 * locks are not advised to be used around rte_eth_rx_burst() and
 * rte_eth_tx_burst() calls.
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_write_lock_tm(rte_rwlock_t *rwl)
	__rte_acquire_capability(rwl);

/**
 * Commit hardware memory transaction or release the write lock if the lock is used as a fall-back
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_write_unlock_tm(rte_rwlock_t *rwl)
	__rte_release_capability(rwl);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RWLOCK_H_ */
