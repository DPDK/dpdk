/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Ericsson AB
 */

#ifndef _RTE_SEQLOCK_H_
#define _RTE_SEQLOCK_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Seqlock
 *
 * A sequence lock (seqlock) is a synchronization primitive allowing
 * multiple, parallel, readers to efficiently and safely (i.e., in a
 * data-race free manner) access lock-protected data. The RTE seqlock
 * permits multiple writers as well. A spinlock is used for
 * writer-writer synchronization.
 *
 * A reader never blocks a writer. Very high frequency writes may
 * prevent readers from making progress.
 *
 * A seqlock is not preemption-safe on the writer side. If a writer is
 * preempted, it may block readers until the writer thread is allowed
 * to continue. Heavy computations should be kept out of the
 * writer-side critical section, to avoid delaying readers.
 *
 * Seqlocks are useful for data which are read by many cores, at a
 * high frequency, and relatively infrequently written to.
 *
 * One way to think about seqlocks is that they provide means to
 * perform atomic operations on objects larger than what the native
 * machine instructions allow for.
 *
 * To avoid resource reclamation issues, the data protected by a
 * seqlock should typically be kept self-contained (e.g., no pointers
 * to mutable, dynamically allocated data).
 *
 * Example usage:
 * @code{.c}
 * #define MAX_Y_LEN 16
 * // Application-defined example data structure, protected by a seqlock.
 * struct config {
 *         rte_seqlock_t lock;
 *         int param_x;
 *         char param_y[MAX_Y_LEN];
 * };
 *
 * // Accessor function for reading config fields.
 * void
 * config_read(const struct config *config, int *param_x, char *param_y)
 * {
 *         uint32_t sn;
 *
 *         do {
 *                 sn = rte_seqlock_read_begin(&config->lock);
 *
 *                 // Loads may be atomic or non-atomic, as in this example.
 *                 *param_x = config->param_x;
 *                 strcpy(param_y, config->param_y);
 *                 // An alternative to an immediate retry is to abort and
 *                 // try again at some later time, assuming progress is
 *                 // possible without the data.
 *         } while (rte_seqlock_read_retry(&config->lock, sn));
 * }
 *
 * // Accessor function for writing config fields.
 * void
 * config_update(struct config *config, int param_x, const char *param_y)
 * {
 *         rte_seqlock_write_lock(&config->lock);
 *         // Stores may be atomic or non-atomic, as in this example.
 *         config->param_x = param_x;
 *         strcpy(config->param_y, param_y);
 *         rte_seqlock_write_unlock(&config->lock);
 * }
 * @endcode
 *
 * In case there is only a single writer, or writer-writer
 * serialization is provided by other means, the use of sequence lock
 * (i.e., rte_seqlock_t) can be replaced with the use of the "raw"
 * rte_seqcount_t type instead.
 *
 * @see
 * https://en.wikipedia.org/wiki/Seqlock.
 */

#include <stdbool.h>
#include <stdint.h>

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_seqcount.h>
#include <rte_spinlock.h>

/**
 * The RTE seqlock type.
 */
typedef struct {
	rte_seqcount_t count; /**< Sequence count for the protected data. */
	rte_spinlock_t lock; /**< Spinlock used to serialize writers. */
} rte_seqlock_t;

/**
 * A static seqlock initializer.
 */
#define RTE_SEQLOCK_INITIALIZER \
	{							\
		.count = RTE_SEQCOUNT_INITIALIZER,		\
		.lock = RTE_SPINLOCK_INITIALIZER		\
	}

/**
 * Initialize the seqlock.
 *
 * This function initializes the seqlock, and leaves the writer-side
 * spinlock unlocked.
 *
 * @param seqlock
 *   A pointer to the seqlock.
 */
static inline void
rte_seqlock_init(rte_seqlock_t *seqlock)
{
	rte_seqcount_init(&seqlock->count);
	rte_spinlock_init(&seqlock->lock);
}

/**
 * Begin a read-side critical section.
 *
 * See rte_seqcount_read_retry() for details.
 *
 * @param seqlock
 *   A pointer to the seqlock.
 * @return
 *   The seqlock sequence number for this critical section, to
 *   later be passed to rte_seqlock_read_retry().
 *
 * @see rte_seqlock_read_retry()
 * @see rte_seqcount_read_retry()
 */
static inline uint32_t
rte_seqlock_read_begin(const rte_seqlock_t *seqlock)
{
	return rte_seqcount_read_begin(&seqlock->count);
}

/**
 * End a read-side critical section.
 *
 * See rte_seqcount_read_retry() for details.
 *
 * @param seqlock
 *   A pointer to the seqlock.
 * @param begin_sn
 *   The seqlock sequence number returned by rte_seqlock_read_begin().
 * @return
 *   true or false, if the just-read seqlock-protected data was
 *   inconsistent or consistent, respectively, at the time it was
 *   read.
 *
 * @see rte_seqlock_read_begin()
 */
static inline bool
rte_seqlock_read_retry(const rte_seqlock_t *seqlock, uint32_t begin_sn)
{
	return rte_seqcount_read_retry(&seqlock->count, begin_sn);
}

/**
 * Begin a write-side critical section.
 *
 * A call to this function acquires the write lock associated @p
 * seqlock, and marks the beginning of a write-side critical section.
 *
 * After having called this function, the caller may go on to modify
 * (both read and write) the protected data, in an atomic or
 * non-atomic manner.
 *
 * After the necessary updates have been performed, the application
 * calls rte_seqlock_write_unlock().
 *
 * This function is not preemption-safe in the sense that preemption
 * of the calling thread may block reader progress until the writer
 * thread is rescheduled.
 *
 * Unlike rte_seqlock_read_begin(), each call made to
 * rte_seqlock_write_lock() must be matched with an unlock call.
 *
 * @param seqlock
 *   A pointer to the seqlock.
 *
 * @see rte_seqlock_write_unlock()
 */
static inline void
rte_seqlock_write_lock(rte_seqlock_t *seqlock)
	__rte_exclusive_lock_function(&seqlock->lock)
{
	/* To synchronize with other writers. */
	rte_spinlock_lock(&seqlock->lock);

	rte_seqcount_write_begin(&seqlock->count);
}

/**
 * End a write-side critical section.
 *
 * A call to this function marks the end of the write-side critical
 * section, for @p seqlock. After this call has been made, the protected
 * data may no longer be modified.
 *
 * @param seqlock
 *   A pointer to the seqlock.
 *
 * @see rte_seqlock_write_lock()
 */
static inline void
rte_seqlock_write_unlock(rte_seqlock_t *seqlock)
	__rte_unlock_function(&seqlock->lock)
{
	rte_seqcount_write_end(&seqlock->count);

	rte_spinlock_unlock(&seqlock->lock);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_SEQLOCK_H_ */
