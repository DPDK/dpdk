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

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_lock_annotations.h>
#include <rte_pause.h>

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

typedef struct __rte_lockable {
	int32_t cnt;
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
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_read_lock(rte_rwlock_t *rwl)
	__rte_shared_lock_function(rwl)
	__rte_no_thread_safety_analysis
{
	int32_t x;

	while (1) {
		/* Wait while writer is present or pending */
		while (__atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED)
		       & RTE_RWLOCK_MASK)
			rte_pause();

		/* Try to get read lock */
		x = __atomic_fetch_add(&rwl->cnt, RTE_RWLOCK_READ,
				       __ATOMIC_ACQUIRE) + RTE_RWLOCK_READ;

		/* If no writer, then acquire was successful */
		if (likely(!(x & RTE_RWLOCK_MASK)))
			return;

		/* Lost race with writer, backout the change. */
		__atomic_fetch_sub(&rwl->cnt, RTE_RWLOCK_READ,
				   __ATOMIC_RELAXED);
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
	__rte_shared_trylock_function(0, rwl)
	__rte_no_thread_safety_analysis
{
	int32_t x;

	x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);

	/* fail if write lock is held or writer is pending */
	if (x & RTE_RWLOCK_MASK)
		return -EBUSY;

	/* Try to get read lock */
	x = __atomic_fetch_add(&rwl->cnt, RTE_RWLOCK_READ,
			       __ATOMIC_ACQUIRE) + RTE_RWLOCK_READ;

	/* Back out if writer raced in */
	if (unlikely(x & RTE_RWLOCK_MASK)) {
		__atomic_fetch_sub(&rwl->cnt, RTE_RWLOCK_READ,
				   __ATOMIC_RELEASE);

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
	__rte_unlock_function(rwl)
	__rte_no_thread_safety_analysis
{
	__atomic_fetch_sub(&rwl->cnt, RTE_RWLOCK_READ, __ATOMIC_RELEASE);
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
	__rte_exclusive_trylock_function(0, rwl)
	__rte_no_thread_safety_analysis
{
	int32_t x;

	x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);
	if (x < RTE_RWLOCK_WRITE &&
	    __atomic_compare_exchange_n(&rwl->cnt, &x, x + RTE_RWLOCK_WRITE,
					1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
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
	__rte_exclusive_lock_function(rwl)
	__rte_no_thread_safety_analysis
{
	int32_t x;

	while (1) {
		x = __atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED);

		/* No readers or writers? */
		if (likely(x < RTE_RWLOCK_WRITE)) {
			/* Turn off RTE_RWLOCK_WAIT, turn on RTE_RWLOCK_WRITE */
			if (__atomic_compare_exchange_n(&rwl->cnt, &x, RTE_RWLOCK_WRITE, 1,
							__ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
				return;
		}

		/* Turn on writer wait bit */
		if (!(x & RTE_RWLOCK_WAIT))
			__atomic_fetch_or(&rwl->cnt, RTE_RWLOCK_WAIT, __ATOMIC_RELAXED);

		/* Wait until no readers before trying again */
		while (__atomic_load_n(&rwl->cnt, __ATOMIC_RELAXED) > RTE_RWLOCK_WAIT)
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
	__rte_unlock_function(rwl)
	__rte_no_thread_safety_analysis
{
	__atomic_fetch_sub(&rwl->cnt, RTE_RWLOCK_WRITE, __ATOMIC_RELEASE);
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
	__rte_shared_lock_function(rwl);

/**
 * Commit hardware memory transaction or release the read lock if the lock is used as a fall-back
 *
 * @param rwl
 *   A pointer to the rwlock structure.
 */
static inline void
rte_rwlock_read_unlock_tm(rte_rwlock_t *rwl)
	__rte_unlock_function(rwl);

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
	__rte_exclusive_lock_function(rwl);

/**
 * Commit hardware memory transaction or release the write lock if the lock is used as a fall-back
 *
 * @param rwl
 *   A pointer to a rwlock structure.
 */
static inline void
rte_rwlock_write_unlock_tm(rte_rwlock_t *rwl)
	__rte_unlock_function(rwl);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RWLOCK_H_ */
