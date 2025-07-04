/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_SPINLOCK_H_
#define _RTE_SPINLOCK_H_

/**
 * @file
 *
 * RTE Spinlocks
 *
 * This file defines an API for read-write locks, which are implemented
 * in an architecture-specific way. This kind of lock simply waits in
 * a loop repeatedly checking until the lock becomes available.
 *
 * All locks must be initialised before use, and only initialised once.
 */

#include <rte_lcore.h>
#ifdef RTE_FORCE_INTRINSICS
#include <rte_common.h>
#endif
#include <rte_lock_annotations.h>
#include <rte_pause.h>
#include <rte_stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The rte_spinlock_t type.
 */
typedef struct __rte_capability("spinlock") {
	volatile RTE_ATOMIC(int) locked; /**< lock status 0 = unlocked, 1 = locked */
} rte_spinlock_t;

/**
 * A static spinlock initializer.
 */
#define RTE_SPINLOCK_INITIALIZER { 0 }

/**
 * Initialize the spinlock to an unlocked state.
 *
 * @param sl
 *   A pointer to the spinlock.
 */
static inline void
rte_spinlock_init(rte_spinlock_t *sl)
{
	sl->locked = 0;
}

/**
 * Take the spinlock.
 *
 * @param sl
 *   A pointer to the spinlock.
 */
static inline void
rte_spinlock_lock(rte_spinlock_t *sl)
	__rte_acquire_capability(sl);

#ifdef RTE_FORCE_INTRINSICS
static inline void
rte_spinlock_lock(rte_spinlock_t *sl)
	__rte_no_thread_safety_analysis
{
	int exp = 0;

	while (!rte_atomic_compare_exchange_strong_explicit(&sl->locked, &exp, 1,
				rte_memory_order_acquire, rte_memory_order_relaxed)) {
		rte_wait_until_equal_32((volatile uint32_t *)(uintptr_t)&sl->locked,
			       0, rte_memory_order_relaxed);
		exp = 0;
	}
}
#endif

/**
 * Release the spinlock.
 *
 * @param sl
 *   A pointer to the spinlock.
 */
static inline void
rte_spinlock_unlock(rte_spinlock_t *sl)
	__rte_release_capability(sl);

#ifdef RTE_FORCE_INTRINSICS
static inline void
rte_spinlock_unlock(rte_spinlock_t *sl)
	__rte_no_thread_safety_analysis
{
	rte_atomic_store_explicit(&sl->locked, 0, rte_memory_order_release);
}
#endif

/**
 * Try to take the lock.
 *
 * @param sl
 *   A pointer to the spinlock.
 * @return
 *   1 if the lock is successfully taken; 0 otherwise.
 */
__rte_warn_unused_result
static inline int
rte_spinlock_trylock(rte_spinlock_t *sl)
	__rte_try_acquire_capability(true, sl);

#ifdef RTE_FORCE_INTRINSICS
static inline int
rte_spinlock_trylock(rte_spinlock_t *sl)
	__rte_no_thread_safety_analysis
{
	int exp = 0;
	return rte_atomic_compare_exchange_strong_explicit(&sl->locked, &exp, 1,
				rte_memory_order_acquire, rte_memory_order_relaxed);
}
#endif

/**
 * Test if the lock is taken.
 *
 * @param sl
 *   A pointer to the spinlock.
 * @return
 *   1 if the lock is currently taken; 0 otherwise.
 */
static inline int rte_spinlock_is_locked (rte_spinlock_t *sl)
{
	return rte_atomic_load_explicit(&sl->locked, rte_memory_order_acquire);
}

/**
 * Test if hardware transactional memory (lock elision) is supported
 *
 * @return
 *   1 if the hardware transactional memory is supported; 0 otherwise.
 */
static inline int rte_tm_supported(void);

/**
 * Try to execute critical section in a hardware memory transaction,
 * if it fails or not available take the spinlock.
 *
 * NOTE: An attempt to perform a HW I/O operation inside a hardware memory
 * transaction always aborts the transaction since the CPU is not able to
 * roll-back should the transaction fail. Therefore, hardware transactional
 * locks are not advised to be used around rte_eth_rx_burst() and
 * rte_eth_tx_burst() calls.
 *
 * @param sl
 *   A pointer to the spinlock.
 */
static inline void
rte_spinlock_lock_tm(rte_spinlock_t *sl)
	__rte_acquire_capability(sl);

/**
 * Commit hardware memory transaction or release the spinlock if
 * the spinlock is used as a fall-back
 *
 * @param sl
 *   A pointer to the spinlock.
 */
static inline void
rte_spinlock_unlock_tm(rte_spinlock_t *sl)
	__rte_release_capability(sl);

/**
 * Try to execute critical section in a hardware memory transaction,
 * if it fails or not available try to take the lock.
 *
 * NOTE: An attempt to perform a HW I/O operation inside a hardware memory
 * transaction always aborts the transaction since the CPU is not able to
 * roll-back should the transaction fail. Therefore, hardware transactional
 * locks are not advised to be used around rte_eth_rx_burst() and
 * rte_eth_tx_burst() calls.
 *
 * @param sl
 *   A pointer to the spinlock.
 * @return
 *   1 if the hardware memory transaction is successfully started
 *   or lock is successfully taken; 0 otherwise.
 */
__rte_warn_unused_result
static inline int
rte_spinlock_trylock_tm(rte_spinlock_t *sl)
	__rte_try_acquire_capability(true, sl);

/**
 * The rte_spinlock_recursive_t type.
 */
typedef struct {
	rte_spinlock_t sl; /**< the actual spinlock */
	volatile int user; /**< core id using lock, -1 for unused */
	volatile int count; /**< count of time this lock has been called */
} rte_spinlock_recursive_t;

/**
 * A static recursive spinlock initializer.
 */
#define RTE_SPINLOCK_RECURSIVE_INITIALIZER {RTE_SPINLOCK_INITIALIZER, -1, 0}

/**
 * Initialize the recursive spinlock to an unlocked state.
 *
 * @param slr
 *   A pointer to the recursive spinlock.
 */
static inline void rte_spinlock_recursive_init(rte_spinlock_recursive_t *slr)
{
	rte_spinlock_init(&slr->sl);
	slr->user = -1;
	slr->count = 0;
}

/**
 * Take the recursive spinlock.
 *
 * @param slr
 *   A pointer to the recursive spinlock.
 */
static inline void rte_spinlock_recursive_lock(rte_spinlock_recursive_t *slr)
	__rte_no_thread_safety_analysis
{
	int id = rte_gettid();

	if (slr->user != id) {
		rte_spinlock_lock(&slr->sl);
		slr->user = id;
	}
	slr->count++;
}
/**
 * Release the recursive spinlock.
 *
 * @param slr
 *   A pointer to the recursive spinlock.
 */
static inline void rte_spinlock_recursive_unlock(rte_spinlock_recursive_t *slr)
	__rte_no_thread_safety_analysis
{
	if (--(slr->count) == 0) {
		slr->user = -1;
		rte_spinlock_unlock(&slr->sl);
	}

}

/**
 * Try to take the recursive lock.
 *
 * @param slr
 *   A pointer to the recursive spinlock.
 * @return
 *   1 if the lock is successfully taken; 0 otherwise.
 */
__rte_warn_unused_result
static inline int rte_spinlock_recursive_trylock(rte_spinlock_recursive_t *slr)
	__rte_no_thread_safety_analysis
{
	int id = rte_gettid();

	if (slr->user != id) {
		if (rte_spinlock_trylock(&slr->sl) == 0)
			return 0;
		slr->user = id;
	}
	slr->count++;
	return 1;
}


/**
 * Try to execute critical section in a hardware memory transaction,
 * if it fails or not available take the recursive spinlocks
 *
 * NOTE: An attempt to perform a HW I/O operation inside a hardware memory
 * transaction always aborts the transaction since the CPU is not able to
 * roll-back should the transaction fail. Therefore, hardware transactional
 * locks are not advised to be used around rte_eth_rx_burst() and
 * rte_eth_tx_burst() calls.
 *
 * @param slr
 *   A pointer to the recursive spinlock.
 */
static inline void rte_spinlock_recursive_lock_tm(
	rte_spinlock_recursive_t *slr);

/**
 * Commit hardware memory transaction or release the recursive spinlock
 * if the recursive spinlock is used as a fall-back
 *
 * @param slr
 *   A pointer to the recursive spinlock.
 */
static inline void rte_spinlock_recursive_unlock_tm(
	rte_spinlock_recursive_t *slr);

/**
 * Try to execute critical section in a hardware memory transaction,
 * if it fails or not available try to take the recursive lock
 *
 * NOTE: An attempt to perform a HW I/O operation inside a hardware memory
 * transaction always aborts the transaction since the CPU is not able to
 * roll-back should the transaction fail. Therefore, hardware transactional
 * locks are not advised to be used around rte_eth_rx_burst() and
 * rte_eth_tx_burst() calls.
 *
 * @param slr
 *   A pointer to the recursive spinlock.
 * @return
 *   1 if the hardware memory transaction is successfully started
 *   or lock is successfully taken; 0 otherwise.
 */
__rte_warn_unused_result
static inline int rte_spinlock_recursive_trylock_tm(
	rte_spinlock_recursive_t *slr);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_SPINLOCK_H_ */
