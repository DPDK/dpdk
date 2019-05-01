/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018 Arm Limited
 */

#ifndef _RTE_RCU_QSBR_H_
#define _RTE_RCU_QSBR_H_

/**
 * @file
 * RTE Quiescent State Based Reclamation (QSBR)
 *
 * Quiescent State (QS) is any point in the thread execution
 * where the thread does not hold a reference to a data structure
 * in shared memory. While using lock-less data structures, the writer
 * can safely free memory once all the reader threads have entered
 * quiescent state.
 *
 * This library provides the ability for the readers to report quiescent
 * state and for the writers to identify when all the readers have
 * entered quiescent state.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <rte_common.h>
#include <rte_memory.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_atomic.h>

extern int rte_rcu_log_type;

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
#define __RTE_RCU_DP_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, rte_rcu_log_type, \
		"%s(): " fmt "\n", __func__, ## args)
#else
#define __RTE_RCU_DP_LOG(level, fmt, args...)
#endif

#if defined(RTE_LIBRTE_RCU_DEBUG)
#define __RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, level, fmt, args...) do {\
	if (v->qsbr_cnt[thread_id].lock_cnt) \
		rte_log(RTE_LOG_ ## level, rte_rcu_log_type, \
			"%s(): " fmt "\n", __func__, ## args); \
} while (0)
#else
#define __RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, level, fmt, args...)
#endif

/* Registered thread IDs are stored as a bitmap of 64b element array.
 * Given thread id needs to be converted to index into the array and
 * the id within the array element.
 */
#define __RTE_QSBR_THRID_ARRAY_ELM_SIZE (sizeof(uint64_t) * 8)
#define __RTE_QSBR_THRID_ARRAY_SIZE(max_threads) \
	RTE_ALIGN(RTE_ALIGN_MUL_CEIL(max_threads, \
		__RTE_QSBR_THRID_ARRAY_ELM_SIZE) >> 3, RTE_CACHE_LINE_SIZE)
#define __RTE_QSBR_THRID_ARRAY_ELM(v, i) ((uint64_t *) \
	((struct rte_rcu_qsbr_cnt *)(v + 1) + v->max_threads) + i)
#define __RTE_QSBR_THRID_INDEX_SHIFT 6
#define __RTE_QSBR_THRID_MASK 0x3f
#define RTE_QSBR_THRID_INVALID 0xffffffff

/* Worker thread counter */
struct rte_rcu_qsbr_cnt {
	uint64_t cnt;
	/**< Quiescent state counter. Value 0 indicates the thread is offline
	 *   64b counter is used to avoid adding more code to address
	 *   counter overflow. Changing this to 32b would require additional
	 *   changes to various APIs.
	 */
	uint32_t lock_cnt;
	/**< Lock counter. Used when CONFIG_RTE_LIBRTE_RCU_DEBUG is enabled */
} __rte_cache_aligned;

#define __RTE_QSBR_CNT_THR_OFFLINE 0
#define __RTE_QSBR_CNT_INIT 1

/* RTE Quiescent State variable structure.
 * This structure has two elements that vary in size based on the
 * 'max_threads' parameter.
 * 1) Quiescent state counter array
 * 2) Register thread ID array
 */
struct rte_rcu_qsbr {
	uint64_t token __rte_cache_aligned;
	/**< Counter to allow for multiple concurrent quiescent state queries */

	uint32_t num_elems __rte_cache_aligned;
	/**< Number of elements in the thread ID array */
	uint32_t num_threads;
	/**< Number of threads currently using this QS variable */
	uint32_t max_threads;
	/**< Maximum number of threads using this QS variable */

	struct rte_rcu_qsbr_cnt qsbr_cnt[0] __rte_cache_aligned;
	/**< Quiescent state counter array of 'max_threads' elements */

	/**< Registered thread IDs are stored in a bitmap array,
	 *   after the quiescent state counter array.
	 */
} __rte_cache_aligned;

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Return the size of the memory occupied by a Quiescent State variable.
 *
 * @param max_threads
 *   Maximum number of threads reporting quiescent state on this variable.
 * @return
 *   On success - size of memory in bytes required for this QS variable.
 *   On error - 1 with error code set in rte_errno.
 *   Possible rte_errno codes are:
 *   - EINVAL - max_threads is 0
 */
size_t __rte_experimental
rte_rcu_qsbr_get_memsize(uint32_t max_threads);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Initialize a Quiescent State (QS) variable.
 *
 * @param v
 *   QS variable
 * @param max_threads
 *   Maximum number of threads reporting quiescent state on this variable.
 *   This should be the same value as passed to rte_rcu_qsbr_get_memsize.
 * @return
 *   On success - 0
 *   On error - 1 with error code set in rte_errno.
 *   Possible rte_errno codes are:
 *   - EINVAL - max_threads is 0 or 'v' is NULL.
 *
 */
int __rte_experimental
rte_rcu_qsbr_init(struct rte_rcu_qsbr *v, uint32_t max_threads);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Register a reader thread to report its quiescent state
 * on a QS variable.
 *
 * This is implemented as a lock-free function. It is multi-thread
 * safe.
 * Any reader thread that wants to report its quiescent state must
 * call this API. This can be called during initialization or as part
 * of the packet processing loop.
 *
 * Note that rte_rcu_qsbr_thread_online must be called before the
 * thread updates its quiescent state using rte_rcu_qsbr_quiescent.
 *
 * @param v
 *   QS variable
 * @param thread_id
 *   Reader thread with this thread ID will report its quiescent state on
 *   the QS variable. thread_id is a value between 0 and (max_threads - 1).
 *   'max_threads' is the parameter passed in 'rte_rcu_qsbr_init' API.
 */
int __rte_experimental
rte_rcu_qsbr_thread_register(struct rte_rcu_qsbr *v, unsigned int thread_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Remove a reader thread, from the list of threads reporting their
 * quiescent state on a QS variable.
 *
 * This is implemented as a lock-free function. It is multi-thread safe.
 * This API can be called from the reader threads during shutdown.
 * Ongoing quiescent state queries will stop waiting for the status from this
 * unregistered reader thread.
 *
 * @param v
 *   QS variable
 * @param thread_id
 *   Reader thread with this thread ID will stop reporting its quiescent
 *   state on the QS variable.
 */
int __rte_experimental
rte_rcu_qsbr_thread_unregister(struct rte_rcu_qsbr *v, unsigned int thread_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Add a registered reader thread, to the list of threads reporting their
 * quiescent state on a QS variable.
 *
 * This is implemented as a lock-free function. It is multi-thread
 * safe.
 *
 * Any registered reader thread that wants to report its quiescent state must
 * call this API before calling rte_rcu_qsbr_quiescent. This can be called
 * during initialization or as part of the packet processing loop.
 *
 * The reader thread must call rte_rcu_thread_offline API, before
 * calling any functions that block, to ensure that rte_rcu_qsbr_check
 * API does not wait indefinitely for the reader thread to update its QS.
 *
 * The reader thread must call rte_rcu_thread_online API, after the blocking
 * function call returns, to ensure that rte_rcu_qsbr_check API
 * waits for the reader thread to update its quiescent state.
 *
 * @param v
 *   QS variable
 * @param thread_id
 *   Reader thread with this thread ID will report its quiescent state on
 *   the QS variable.
 */
static __rte_always_inline void __rte_experimental
rte_rcu_qsbr_thread_online(struct rte_rcu_qsbr *v, unsigned int thread_id)
{
	uint64_t t;

	RTE_ASSERT(v != NULL && thread_id < v->max_threads);

	__RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, ERR, "Lock counter %u\n",
				v->qsbr_cnt[thread_id].lock_cnt);

	/* Copy the current value of token.
	 * The fence at the end of the function will ensure that
	 * the following will not move down after the load of any shared
	 * data structure.
	 */
	t = __atomic_load_n(&v->token, __ATOMIC_RELAXED);

	/* __atomic_store_n(cnt, __ATOMIC_RELAXED) is used to ensure
	 * 'cnt' (64b) is accessed atomically.
	 */
	__atomic_store_n(&v->qsbr_cnt[thread_id].cnt,
		t, __ATOMIC_RELAXED);

	/* The subsequent load of the data structure should not
	 * move above the store. Hence a store-load barrier
	 * is required.
	 * If the load of the data structure moves above the store,
	 * writer might not see that the reader is online, even though
	 * the reader is referencing the shared data structure.
	 */
#ifdef RTE_ARCH_X86_64
	/* rte_smp_mb() for x86 is lighter */
	rte_smp_mb();
#else
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Remove a registered reader thread from the list of threads reporting their
 * quiescent state on a QS variable.
 *
 * This is implemented as a lock-free function. It is multi-thread
 * safe.
 *
 * This can be called during initialization or as part of the packet
 * processing loop.
 *
 * The reader thread must call rte_rcu_thread_offline API, before
 * calling any functions that block, to ensure that rte_rcu_qsbr_check
 * API does not wait indefinitely for the reader thread to update its QS.
 *
 * @param v
 *   QS variable
 * @param thread_id
 *   rte_rcu_qsbr_check API will not wait for the reader thread with
 *   this thread ID to report its quiescent state on the QS variable.
 */
static __rte_always_inline void __rte_experimental
rte_rcu_qsbr_thread_offline(struct rte_rcu_qsbr *v, unsigned int thread_id)
{
	RTE_ASSERT(v != NULL && thread_id < v->max_threads);

	__RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, ERR, "Lock counter %u\n",
				v->qsbr_cnt[thread_id].lock_cnt);

	/* The reader can go offline only after the load of the
	 * data structure is completed. i.e. any load of the
	 * data strcture can not move after this store.
	 */

	__atomic_store_n(&v->qsbr_cnt[thread_id].cnt,
		__RTE_QSBR_CNT_THR_OFFLINE, __ATOMIC_RELEASE);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Acquire a lock for accessing a shared data structure.
 *
 * This is implemented as a lock-free function. It is multi-thread
 * safe.
 *
 * This API is provided to aid debugging. This should be called before
 * accessing a shared data structure.
 *
 * When CONFIG_RTE_LIBRTE_RCU_DEBUG is enabled a lock counter is incremented.
 * Similarly rte_rcu_qsbr_unlock will decrement the counter. When the
 * rte_rcu_qsbr_check API will verify that this counter is 0.
 *
 * When CONFIG_RTE_LIBRTE_RCU_DEBUG is disabled, this API will do nothing.
 *
 * @param v
 *   QS variable
 * @param thread_id
 *   Reader thread id
 */
static __rte_always_inline void __rte_experimental
rte_rcu_qsbr_lock(__rte_unused struct rte_rcu_qsbr *v,
			__rte_unused unsigned int thread_id)
{
	RTE_ASSERT(v != NULL && thread_id < v->max_threads);

#if defined(RTE_LIBRTE_RCU_DEBUG)
	/* Increment the lock counter */
	__atomic_fetch_add(&v->qsbr_cnt[thread_id].lock_cnt,
				1, __ATOMIC_ACQUIRE);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Release a lock after accessing a shared data structure.
 *
 * This is implemented as a lock-free function. It is multi-thread
 * safe.
 *
 * This API is provided to aid debugging. This should be called after
 * accessing a shared data structure.
 *
 * When CONFIG_RTE_LIBRTE_RCU_DEBUG is enabled, rte_rcu_qsbr_unlock will
 * decrement a lock counter. rte_rcu_qsbr_check API will verify that this
 * counter is 0.
 *
 * When CONFIG_RTE_LIBRTE_RCU_DEBUG is disabled, this API will do nothing.
 *
 * @param v
 *   QS variable
 * @param thread_id
 *   Reader thread id
 */
static __rte_always_inline void __rte_experimental
rte_rcu_qsbr_unlock(__rte_unused struct rte_rcu_qsbr *v,
			__rte_unused unsigned int thread_id)
{
	RTE_ASSERT(v != NULL && thread_id < v->max_threads);

#if defined(RTE_LIBRTE_RCU_DEBUG)
	/* Decrement the lock counter */
	__atomic_fetch_sub(&v->qsbr_cnt[thread_id].lock_cnt,
				1, __ATOMIC_RELEASE);

	__RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, WARNING,
				"Lock counter %u. Nested locks?\n",
				v->qsbr_cnt[thread_id].lock_cnt);
#endif
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Ask the reader threads to report the quiescent state
 * status.
 *
 * This is implemented as a lock-free function. It is multi-thread
 * safe and can be called from worker threads.
 *
 * @param v
 *   QS variable
 * @return
 *   - This is the token for this call of the API. This should be
 *     passed to rte_rcu_qsbr_check API.
 */
static __rte_always_inline uint64_t __rte_experimental
rte_rcu_qsbr_start(struct rte_rcu_qsbr *v)
{
	uint64_t t;

	RTE_ASSERT(v != NULL);

	/* Release the changes to the shared data structure.
	 * This store release will ensure that changes to any data
	 * structure are visible to the workers before the token
	 * update is visible.
	 */
	t = __atomic_add_fetch(&v->token, 1, __ATOMIC_RELEASE);

	return t;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Update quiescent state for a reader thread.
 *
 * This is implemented as a lock-free function. It is multi-thread safe.
 * All the reader threads registered to report their quiescent state
 * on the QS variable must call this API.
 *
 * @param v
 *   QS variable
 * @param thread_id
 *   Update the quiescent state for the reader with this thread ID.
 */
static __rte_always_inline void __rte_experimental
rte_rcu_qsbr_quiescent(struct rte_rcu_qsbr *v, unsigned int thread_id)
{
	uint64_t t;

	RTE_ASSERT(v != NULL && thread_id < v->max_threads);

	__RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, ERR, "Lock counter %u\n",
				v->qsbr_cnt[thread_id].lock_cnt);

	/* Acquire the changes to the shared data structure released
	 * by rte_rcu_qsbr_start.
	 * Later loads of the shared data structure should not move
	 * above this load. Hence, use load-acquire.
	 */
	t = __atomic_load_n(&v->token, __ATOMIC_ACQUIRE);

	/* Inform the writer that updates are visible to this reader.
	 * Prior loads of the shared data structure should not move
	 * beyond this store. Hence use store-release.
	 */
	__atomic_store_n(&v->qsbr_cnt[thread_id].cnt,
			 t, __ATOMIC_RELEASE);

	__RTE_RCU_DP_LOG(DEBUG, "%s: update: token = %"PRIu64", Thread ID = %d",
		__func__, t, thread_id);
}

/* Check the quiescent state counter for registered threads only, assuming
 * that not all threads have registered.
 */
static __rte_always_inline int
__rte_rcu_qsbr_check_selective(struct rte_rcu_qsbr *v, uint64_t t, bool wait)
{
	uint32_t i, j, id;
	uint64_t bmap;
	uint64_t c;
	uint64_t *reg_thread_id;

	for (i = 0, reg_thread_id = __RTE_QSBR_THRID_ARRAY_ELM(v, 0);
		i < v->num_elems;
		i++, reg_thread_id++) {
		/* Load the current registered thread bit map before
		 * loading the reader thread quiescent state counters.
		 */
		bmap = __atomic_load_n(reg_thread_id, __ATOMIC_ACQUIRE);
		id = i << __RTE_QSBR_THRID_INDEX_SHIFT;

		while (bmap) {
			j = __builtin_ctzl(bmap);
			__RTE_RCU_DP_LOG(DEBUG,
				"%s: check: token = %"PRIu64", wait = %d, Bit Map = 0x%"PRIx64", Thread ID = %d",
				__func__, t, wait, bmap, id + j);
			c = __atomic_load_n(
					&v->qsbr_cnt[id + j].cnt,
					__ATOMIC_ACQUIRE);
			__RTE_RCU_DP_LOG(DEBUG,
				"%s: status: token = %"PRIu64", wait = %d, Thread QS cnt = %"PRIu64", Thread ID = %d",
				__func__, t, wait, c, id+j);
			/* Counter is not checked for wrap-around condition
			 * as it is a 64b counter.
			 */
			if (unlikely(c !=
				__RTE_QSBR_CNT_THR_OFFLINE && c < t)) {
				/* This thread is not in quiescent state */
				if (!wait)
					return 0;

				rte_pause();
				/* This thread might have unregistered.
				 * Re-read the bitmap.
				 */
				bmap = __atomic_load_n(reg_thread_id,
						__ATOMIC_ACQUIRE);

				continue;
			}

			bmap &= ~(1UL << j);
		}
	}

	return 1;
}

/* Check the quiescent state counter for all threads, assuming that
 * all the threads have registered.
 */
static __rte_always_inline int
__rte_rcu_qsbr_check_all(struct rte_rcu_qsbr *v, uint64_t t, bool wait)
{
	uint32_t i;
	struct rte_rcu_qsbr_cnt *cnt;
	uint64_t c;

	for (i = 0, cnt = v->qsbr_cnt; i < v->max_threads; i++, cnt++) {
		__RTE_RCU_DP_LOG(DEBUG,
			"%s: check: token = %"PRIu64", wait = %d, Thread ID = %d",
			__func__, t, wait, i);
		while (1) {
			c = __atomic_load_n(&cnt->cnt, __ATOMIC_ACQUIRE);
			__RTE_RCU_DP_LOG(DEBUG,
				"%s: status: token = %"PRIu64", wait = %d, Thread QS cnt = %"PRIu64", Thread ID = %d",
				__func__, t, wait, c, i);
			/* Counter is not checked for wrap-around condition
			 * as it is a 64b counter.
			 */
			if (likely(c == __RTE_QSBR_CNT_THR_OFFLINE || c >= t))
				break;

			/* This thread is not in quiescent state */
			if (!wait)
				return 0;

			rte_pause();
		}
	}

	return 1;
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Checks if all the reader threads have entered the quiescent state
 * referenced by token.
 *
 * This is implemented as a lock-free function. It is multi-thread
 * safe and can be called from the worker threads as well.
 *
 * If this API is called with 'wait' set to true, the following
 * factors must be considered:
 *
 * 1) If the calling thread is also reporting the status on the
 * same QS variable, it must update the quiescent state status, before
 * calling this API.
 *
 * 2) In addition, while calling from multiple threads, only
 * one of those threads can be reporting the quiescent state status
 * on a given QS variable.
 *
 * @param v
 *   QS variable
 * @param t
 *   Token returned by rte_rcu_qsbr_start API
 * @param wait
 *   If true, block till all the reader threads have completed entering
 *   the quiescent state referenced by token 't'.
 * @return
 *   - 0 if all reader threads have NOT passed through specified number
 *     of quiescent states.
 *   - 1 if all reader threads have passed through specified number
 *     of quiescent states.
 */
static __rte_always_inline int __rte_experimental
rte_rcu_qsbr_check(struct rte_rcu_qsbr *v, uint64_t t, bool wait)
{
	RTE_ASSERT(v != NULL);

	if (likely(v->num_threads == v->max_threads))
		return __rte_rcu_qsbr_check_all(v, t, wait);
	else
		return __rte_rcu_qsbr_check_selective(v, t, wait);
}

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Wait till the reader threads have entered quiescent state.
 *
 * This is implemented as a lock-free function. It is multi-thread safe.
 * This API can be thought of as a wrapper around rte_rcu_qsbr_start and
 * rte_rcu_qsbr_check APIs.
 *
 * If this API is called from multiple threads, only one of
 * those threads can be reporting the quiescent state status on a
 * given QS variable.
 *
 * @param v
 *   QS variable
 * @param thread_id
 *   Thread ID of the caller if it is registered to report quiescent state
 *   on this QS variable (i.e. the calling thread is also part of the
 *   readside critical section). If not, pass RTE_QSBR_THRID_INVALID.
 */
void __rte_experimental
rte_rcu_qsbr_synchronize(struct rte_rcu_qsbr *v, unsigned int thread_id);

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Dump the details of a single QS variables to a file.
 *
 * It is NOT multi-thread safe.
 *
 * @param f
 *   A pointer to a file for output
 * @param v
 *   QS variable
 * @return
 *   On success - 0
 *   On error - 1 with error code set in rte_errno.
 *   Possible rte_errno codes are:
 *   - EINVAL - NULL parameters are passed
 */
int __rte_experimental
rte_rcu_qsbr_dump(FILE *f, struct rte_rcu_qsbr *v);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RCU_QSBR_H_ */
