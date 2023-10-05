/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Ericsson AB
 */

#ifndef _RTE_SEQCOUNT_H_
#define _RTE_SEQCOUNT_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Seqcount
 *
 * The sequence counter synchronizes a single writer with multiple,
 * parallel readers. It is used as the basis for the RTE sequence
 * lock.
 *
 * @see rte_seqlock.h
 */

#include <stdbool.h>
#include <stdint.h>

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_compat.h>
#include <rte_stdatomic.h>

/**
 * The RTE seqcount type.
 */
typedef struct {
	RTE_ATOMIC(uint32_t) sn; /**< A sequence number for the protected data. */
} rte_seqcount_t;

/**
 * A static seqcount initializer.
 */
#define RTE_SEQCOUNT_INITIALIZER { .sn = 0 }

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Initialize the sequence counter.
 *
 * @param seqcount
 *   A pointer to the sequence counter.
 */
__rte_experimental
static inline void
rte_seqcount_init(rte_seqcount_t *seqcount)
{
	seqcount->sn = 0;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Begin a read-side critical section.
 *
 * A call to this function marks the beginning of a read-side critical
 * section, for @p seqcount.
 *
 * rte_seqcount_read_begin() returns a sequence number, which is later
 * used in rte_seqcount_read_retry() to check if the protected data
 * underwent any modifications during the read transaction.
 *
 * After (in program order) rte_seqcount_read_begin() has been called,
 * the calling thread reads the protected data, for later use. The
 * protected data read *must* be copied (either in pristine form, or
 * in the form of some derivative), since the caller may only read the
 * data from within the read-side critical section (i.e., after
 * rte_seqcount_read_begin() and before rte_seqcount_read_retry()),
 * but must not act upon the retrieved data while in the critical
 * section, since it does not yet know if it is consistent.
 *
 * The protected data may be read using atomic and/or non-atomic
 * operations.
 *
 * After (in program order) all required data loads have been
 * performed, rte_seqcount_read_retry() should be called, marking
 * the end of the read-side critical section.
 *
 * If rte_seqcount_read_retry() returns true, the just-read data is
 * inconsistent and should be discarded. The caller has the option to
 * either restart the whole procedure right away (i.e., calling
 * rte_seqcount_read_begin() again), or do the same at some later time.
 *
 * If rte_seqcount_read_retry() returns false, the data was read
 * atomically and the copied data is consistent.
 *
 * @param seqcount
 *   A pointer to the sequence counter.
 * @return
 *   The seqcount sequence number for this critical section, to
 *   later be passed to rte_seqcount_read_retry().
 *
 * @see rte_seqcount_read_retry()
 */

__rte_experimental
static inline uint32_t
rte_seqcount_read_begin(const rte_seqcount_t *seqcount)
{
	/* rte_memory_order_acquire to prevent loads after (in program order)
	 * from happening before the sn load. Synchronizes-with the
	 * store release in rte_seqcount_write_end().
	 */
	return rte_atomic_load_explicit(&seqcount->sn, rte_memory_order_acquire);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * End a read-side critical section.
 *
 * A call to this function marks the end of a read-side critical
 * section, for @p seqcount. The application must supply the sequence
 * number produced by the corresponding rte_seqcount_read_begin() call.
 *
 * After this function has been called, the caller should not access
 * the protected data.
 *
 * In case rte_seqcount_read_retry() returns true, the just-read data
 * was modified as it was being read and may be inconsistent, and thus
 * should be discarded.
 *
 * In case this function returns false, the data is consistent and the
 * set of atomic and non-atomic load operations performed between
 * rte_seqcount_read_begin() and rte_seqcount_read_retry() were atomic,
 * as a whole.
 *
 * @param seqcount
 *   A pointer to the sequence counter.
 * @param begin_sn
 *   The sequence number returned by rte_seqcount_read_begin().
 * @return
 *   true or false, if the just-read seqcount-protected data was
 *   inconsistent or consistent, respectively, at the time it was
 *   read.
 *
 * @see rte_seqcount_read_begin()
 */

__rte_experimental
static inline bool
rte_seqcount_read_retry(const rte_seqcount_t *seqcount, uint32_t begin_sn)
{
	uint32_t end_sn;

	/* An odd sequence number means the protected data was being
	 * modified already at the point of the rte_seqcount_read_begin()
	 * call.
	 */
	if (unlikely(begin_sn & 1))
		return true;

	/* make sure the data loads happens before the sn load */
	rte_atomic_thread_fence(rte_memory_order_acquire);

	end_sn = rte_atomic_load_explicit(&seqcount->sn, rte_memory_order_relaxed);

	/* A writer incremented the sequence number during this read
	 * critical section.
	 */
	return begin_sn != end_sn;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * Begin a write-side critical section.
 *
 * A call to this function marks the beginning of a write-side
 * critical section, after which the caller may go on to modify (both
 * read and write) the protected data, in an atomic or non-atomic
 * manner.
 *
 * After the necessary updates have been performed, the application
 * calls rte_seqcount_write_end().
 *
 * Multiple, parallel writers must use some external serialization.
 *
 * This function is not preemption-safe in the sense that preemption
 * of the calling thread may block reader progress until the writer
 * thread is rescheduled.
 *
 * @param seqcount
 *   A pointer to the sequence counter.
 *
 * @see rte_seqcount_write_end()
 */

__rte_experimental
static inline void
rte_seqcount_write_begin(rte_seqcount_t *seqcount)
{
	uint32_t sn;

	sn = seqcount->sn + 1;

	rte_atomic_store_explicit(&seqcount->sn, sn, rte_memory_order_relaxed);

	/* rte_memory_order_release to prevent stores after (in program order)
	 * from happening before the sn store.
	 */
	rte_atomic_thread_fence(rte_memory_order_release);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * End a write-side critical section.
 *
 * A call to this function marks the end of the write-side critical
 * section, for @p seqcount. After this call has been made, the
 * protected data may no longer be modified.
 *
 * @param seqcount
 *   A pointer to the sequence counter.
 *
 * @see rte_seqcount_write_begin()
 */
__rte_experimental
static inline void
rte_seqcount_write_end(rte_seqcount_t *seqcount)
{
	uint32_t sn;

	sn = seqcount->sn + 1;

	/* Synchronizes-with the load acquire in rte_seqcount_read_begin(). */
	rte_atomic_store_explicit(&seqcount->sn, sn, rte_memory_order_release);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_SEQCOUNT_H_ */
