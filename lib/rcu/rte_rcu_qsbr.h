/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018-2020 Arm Limited
 */

#ifndef _RTE_RCU_QSBR_H_
#define _RTE_RCU_QSBR_H_

/**
 * @file
 *
 * RTE Quiescent State Based Reclamation (QSBR).
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

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_atomic.h>
#include <rte_ring.h>

extern int rte_rcu_log_type;
#define RTE_LOGTYPE_RCU rte_rcu_log_type

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
#define __RTE_RCU_DP_LOG(level, fmt, args...) \
	RTE_LOG_LINE(level, RCU, "%s(): " fmt, __func__, ## args)
#else
#define __RTE_RCU_DP_LOG(level, fmt, args...)
#endif

#if defined(RTE_LIBRTE_RCU_DEBUG)
#define __RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, level, fmt, args...) do { \
	if (v->qsbr_cnt[thread_id].lock_cnt) \
		RTE_LOG_LINE(level, RCU, "%s(): " fmt, __func__, ## args); \
} while (0)
#else
#define __RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, level, fmt, args...)
#endif

/* Registered thread IDs are stored as a bitmap of 64b element array.
 * Given thread id needs to be converted to index into the array and
 * the id within the array element.
 */
#define __RTE_QSBR_THRID_ARRAY_ELM_SIZE (sizeof(RTE_ATOMIC(uint64_t)) * 8)
#define __RTE_QSBR_THRID_ARRAY_SIZE(max_threads) \
	RTE_ALIGN(RTE_ALIGN_MUL_CEIL(max_threads, \
		__RTE_QSBR_THRID_ARRAY_ELM_SIZE) >> 3, RTE_CACHE_LINE_SIZE)
#define __RTE_QSBR_THRID_ARRAY_ELM(v, i) ((uint64_t __rte_atomic *) \
	((struct rte_rcu_qsbr_cnt *)(v + 1) + v->max_threads) + i)
#define __RTE_QSBR_THRID_INDEX_SHIFT 6
#define __RTE_QSBR_THRID_MASK 0x3f
#define RTE_QSBR_THRID_INVALID 0xffffffff

/* Worker thread counter */
struct rte_rcu_qsbr_cnt {
	RTE_ATOMIC(uint64_t) cnt;
	/**< Quiescent state counter. Value 0 indicates the thread is offline
	 *   64b counter is used to avoid adding more code to address
	 *   counter overflow. Changing this to 32b would require additional
	 *   changes to various APIs.
	 */
	RTE_ATOMIC(uint32_t) lock_cnt;
	/**< Lock counter. Used when RTE_LIBRTE_RCU_DEBUG is enabled */
} __rte_cache_aligned;

#define __RTE_QSBR_CNT_THR_OFFLINE 0
#define __RTE_QSBR_CNT_INIT 1
#define __RTE_QSBR_CNT_MAX ((uint64_t)~0)
#define __RTE_QSBR_TOKEN_SIZE sizeof(uint64_t)

/* RTE Quiescent State variable structure.
 * This structure has two elements that vary in size based on the
 * 'max_threads' parameter.
 * 1) Quiescent state counter array
 * 2) Register thread ID array
 */
struct rte_rcu_qsbr {
	RTE_ATOMIC(uint64_t) token __rte_cache_aligned;
	/**< Counter to allow for multiple concurrent quiescent state queries */
	RTE_ATOMIC(uint64_t) acked_token;
	/**< Least token acked by all the threads in the last call to
	 *   rte_rcu_qsbr_check API.
	 */

	uint32_t num_elems __rte_cache_aligned;
	/**< Number of elements in the thread ID array */
	RTE_ATOMIC(uint32_t) num_threads;
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
 * Call back function called to free the resources.
 *
 * @param p
 *   Pointer provided while creating the defer queue
 * @param e
 *   Pointer to the resource data stored on the defer queue
 * @param n
 *   Number of resources to free. Currently, this is set to 1.
 *
 * @return
 *   None
 */
typedef void (*rte_rcu_qsbr_free_resource_t)(void *p, void *e, unsigned int n);

#define RTE_RCU_QSBR_DQ_NAMESIZE RTE_RING_NAMESIZE

/**
 * Various flags supported.
 */
/**< Enqueue and reclaim operations are multi-thread safe by default.
 *   The call back functions registered to free the resources are
 *   assumed to be multi-thread safe.
 *   Set this flag if multi-thread safety is not required.
 */
#define RTE_RCU_QSBR_DQ_MT_UNSAFE 1

/**
 * Parameters used when creating the defer queue.
 */
struct rte_rcu_qsbr_dq_parameters {
	const char *name;
	/**< Name of the queue. */
	uint32_t flags;
	/**< Flags to control API behaviors */
	uint32_t size;
	/**< Number of entries in queue. Typically, this will be
	 *   the same as the maximum number of entries supported in the
	 *   lock free data structure.
	 *   Data structures with unbounded number of entries is not
	 *   supported currently.
	 */
	uint32_t esize;
	/**< Size (in bytes) of each element in the defer queue.
	 *   This has to be multiple of 4B.
	 */
	uint32_t trigger_reclaim_limit;
	/**< Trigger automatic reclamation after the defer queue
	 *   has at least these many resources waiting. This auto
	 *   reclamation is triggered in rte_rcu_qsbr_dq_enqueue API
	 *   call.
	 *   If this is greater than 'size', auto reclamation is
	 *   not triggered.
	 *   If this is set to 0, auto reclamation is triggered
	 *   in every call to rte_rcu_qsbr_dq_enqueue API.
	 */
	uint32_t max_reclaim_size;
	/**< When automatic reclamation is enabled, reclaim at the max
	 *   these many resources. This should contain a valid value, if
	 *   auto reclamation is on. Setting this to 'size' or greater will
	 *   reclaim all possible resources currently on the defer queue.
	 */
	rte_rcu_qsbr_free_resource_t free_fn;
	/**< Function to call to free the resource. */
	void *p;
	/**< Pointer passed to the free function. Typically, this is the
	 *   pointer to the data structure to which the resource to free
	 *   belongs. This can be NULL.
	 */
	struct rte_rcu_qsbr *v;
	/**< RCU QSBR variable to use for this defer queue */
};

/* RTE defer queue structure.
 * This structure holds the defer queue. The defer queue is used to
 * hold the deleted entries from the data structure that are not
 * yet freed.
 */
struct rte_rcu_qsbr_dq;

/**
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
size_t
rte_rcu_qsbr_get_memsize(uint32_t max_threads);

/**
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
 */
int
rte_rcu_qsbr_init(struct rte_rcu_qsbr *v, uint32_t max_threads);

/**
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
int
rte_rcu_qsbr_thread_register(struct rte_rcu_qsbr *v, unsigned int thread_id);

/**
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
int
rte_rcu_qsbr_thread_unregister(struct rte_rcu_qsbr *v, unsigned int thread_id);

/**
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
 * The reader thread must call rte_rcu_qsbr_thread_offline API, before
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
static __rte_always_inline void
rte_rcu_qsbr_thread_online(struct rte_rcu_qsbr *v, unsigned int thread_id)
{
	uint64_t t;

	RTE_ASSERT(v != NULL && thread_id < v->max_threads);

	__RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, ERR, "Lock counter %u",
				v->qsbr_cnt[thread_id].lock_cnt);

	/* Copy the current value of token.
	 * The fence at the end of the function will ensure that
	 * the following will not move down after the load of any shared
	 * data structure.
	 */
	t = rte_atomic_load_explicit(&v->token, rte_memory_order_relaxed);

	/* rte_atomic_store_explicit(cnt, rte_memory_order_relaxed) is used to ensure
	 * 'cnt' (64b) is accessed atomically.
	 */
	rte_atomic_store_explicit(&v->qsbr_cnt[thread_id].cnt,
		t, rte_memory_order_relaxed);

	/* The subsequent load of the data structure should not
	 * move above the store. Hence a store-load barrier
	 * is required.
	 * If the load of the data structure moves above the store,
	 * writer might not see that the reader is online, even though
	 * the reader is referencing the shared data structure.
	 */
	rte_atomic_thread_fence(rte_memory_order_seq_cst);
}

/**
 * Remove a registered reader thread from the list of threads reporting their
 * quiescent state on a QS variable.
 *
 * This is implemented as a lock-free function. It is multi-thread
 * safe.
 *
 * This can be called during initialization or as part of the packet
 * processing loop.
 *
 * The reader thread must call rte_rcu_qsbr_thread_offline API, before
 * calling any functions that block, to ensure that rte_rcu_qsbr_check
 * API does not wait indefinitely for the reader thread to update its QS.
 *
 * @param v
 *   QS variable
 * @param thread_id
 *   rte_rcu_qsbr_check API will not wait for the reader thread with
 *   this thread ID to report its quiescent state on the QS variable.
 */
static __rte_always_inline void
rte_rcu_qsbr_thread_offline(struct rte_rcu_qsbr *v, unsigned int thread_id)
{
	RTE_ASSERT(v != NULL && thread_id < v->max_threads);

	__RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, ERR, "Lock counter %u",
				v->qsbr_cnt[thread_id].lock_cnt);

	/* The reader can go offline only after the load of the
	 * data structure is completed. i.e. any load of the
	 * data structure can not move after this store.
	 */

	rte_atomic_store_explicit(&v->qsbr_cnt[thread_id].cnt,
		__RTE_QSBR_CNT_THR_OFFLINE, rte_memory_order_release);
}

/**
 * Acquire a lock for accessing a shared data structure.
 *
 * This is implemented as a lock-free function. It is multi-thread
 * safe.
 *
 * This API is provided to aid debugging. This should be called before
 * accessing a shared data structure.
 *
 * When RTE_LIBRTE_RCU_DEBUG is enabled a lock counter is incremented.
 * Similarly rte_rcu_qsbr_unlock will decrement the counter. When the
 * rte_rcu_qsbr_check API will verify that this counter is 0.
 *
 * When RTE_LIBRTE_RCU_DEBUG is disabled, this API will do nothing.
 *
 * @param v
 *   QS variable
 * @param thread_id
 *   Reader thread id
 */
static __rte_always_inline void
rte_rcu_qsbr_lock(__rte_unused struct rte_rcu_qsbr *v,
			__rte_unused unsigned int thread_id)
{
	RTE_ASSERT(v != NULL && thread_id < v->max_threads);

#if defined(RTE_LIBRTE_RCU_DEBUG)
	/* Increment the lock counter */
	rte_atomic_fetch_add_explicit(&v->qsbr_cnt[thread_id].lock_cnt,
				1, rte_memory_order_acquire);
#endif
}

/**
 * Release a lock after accessing a shared data structure.
 *
 * This is implemented as a lock-free function. It is multi-thread
 * safe.
 *
 * This API is provided to aid debugging. This should be called after
 * accessing a shared data structure.
 *
 * When RTE_LIBRTE_RCU_DEBUG is enabled, rte_rcu_qsbr_unlock will
 * decrement a lock counter. rte_rcu_qsbr_check API will verify that this
 * counter is 0.
 *
 * When RTE_LIBRTE_RCU_DEBUG is disabled, this API will do nothing.
 *
 * @param v
 *   QS variable
 * @param thread_id
 *   Reader thread id
 */
static __rte_always_inline void
rte_rcu_qsbr_unlock(__rte_unused struct rte_rcu_qsbr *v,
			__rte_unused unsigned int thread_id)
{
	RTE_ASSERT(v != NULL && thread_id < v->max_threads);

#if defined(RTE_LIBRTE_RCU_DEBUG)
	/* Decrement the lock counter */
	rte_atomic_fetch_sub_explicit(&v->qsbr_cnt[thread_id].lock_cnt,
				1, rte_memory_order_release);

	__RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, WARNING,
				"Lock counter %u. Nested locks?",
				v->qsbr_cnt[thread_id].lock_cnt);
#endif
}

/**
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
static __rte_always_inline uint64_t
rte_rcu_qsbr_start(struct rte_rcu_qsbr *v)
{
	uint64_t t;

	RTE_ASSERT(v != NULL);

	/* Release the changes to the shared data structure.
	 * This store release will ensure that changes to any data
	 * structure are visible to the workers before the token
	 * update is visible.
	 */
	t = rte_atomic_fetch_add_explicit(&v->token, 1, rte_memory_order_release) + 1;

	return t;
}

/**
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
static __rte_always_inline void
rte_rcu_qsbr_quiescent(struct rte_rcu_qsbr *v, unsigned int thread_id)
{
	uint64_t t;

	RTE_ASSERT(v != NULL && thread_id < v->max_threads);

	__RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, ERR, "Lock counter %u",
				v->qsbr_cnt[thread_id].lock_cnt);

	/* Acquire the changes to the shared data structure released
	 * by rte_rcu_qsbr_start.
	 * Later loads of the shared data structure should not move
	 * above this load. Hence, use load-acquire.
	 */
	t = rte_atomic_load_explicit(&v->token, rte_memory_order_acquire);

	/* Check if there are updates available from the writer.
	 * Inform the writer that updates are visible to this reader.
	 * Prior loads of the shared data structure should not move
	 * beyond this store. Hence use store-release.
	 */
	if (t != rte_atomic_load_explicit(&v->qsbr_cnt[thread_id].cnt, rte_memory_order_relaxed))
		rte_atomic_store_explicit(&v->qsbr_cnt[thread_id].cnt,
					 t, rte_memory_order_release);

	__RTE_RCU_DP_LOG(DEBUG, "%s: update: token = %" PRIu64 ", Thread ID = %d",
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
	RTE_ATOMIC(uint64_t) *reg_thread_id;
	uint64_t acked_token = __RTE_QSBR_CNT_MAX;

	for (i = 0, reg_thread_id = __RTE_QSBR_THRID_ARRAY_ELM(v, 0);
		i < v->num_elems;
		i++, reg_thread_id++) {
		/* Load the current registered thread bit map before
		 * loading the reader thread quiescent state counters.
		 */
		bmap = rte_atomic_load_explicit(reg_thread_id, rte_memory_order_acquire);
		id = i << __RTE_QSBR_THRID_INDEX_SHIFT;

		while (bmap) {
			j = rte_ctz64(bmap);
			__RTE_RCU_DP_LOG(DEBUG,
				"%s: check: token = %" PRIu64 ", wait = %d, Bit Map = 0x%" PRIx64 ", Thread ID = %d",
				__func__, t, wait, bmap, id + j);
			c = rte_atomic_load_explicit(
					&v->qsbr_cnt[id + j].cnt,
					rte_memory_order_acquire);
			__RTE_RCU_DP_LOG(DEBUG,
				"%s: status: token = %" PRIu64 ", wait = %d, Thread QS cnt = %" PRIu64 ", Thread ID = %d",
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
				bmap = rte_atomic_load_explicit(reg_thread_id,
						rte_memory_order_acquire);

				continue;
			}

			/* This thread is in quiescent state. Use the counter
			 * to find the least acknowledged token among all the
			 * readers.
			 */
			if (c != __RTE_QSBR_CNT_THR_OFFLINE && acked_token > c)
				acked_token = c;

			bmap &= ~(1UL << j);
		}
	}

	/* All readers are checked, update least acknowledged token.
	 * There might be multiple writers trying to update this. There is
	 * no need to update this very accurately using compare-and-swap.
	 */
	if (acked_token != __RTE_QSBR_CNT_MAX)
		rte_atomic_store_explicit(&v->acked_token, acked_token,
			rte_memory_order_relaxed);

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
	uint64_t acked_token = __RTE_QSBR_CNT_MAX;

	for (i = 0, cnt = v->qsbr_cnt; i < v->max_threads; i++, cnt++) {
		__RTE_RCU_DP_LOG(DEBUG,
			"%s: check: token = %" PRIu64 ", wait = %d, Thread ID = %d",
			__func__, t, wait, i);
		while (1) {
			c = rte_atomic_load_explicit(&cnt->cnt, rte_memory_order_acquire);
			__RTE_RCU_DP_LOG(DEBUG,
				"%s: status: token = %" PRIu64 ", wait = %d, Thread QS cnt = %" PRIu64 ", Thread ID = %d",
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

		/* This thread is in quiescent state. Use the counter to find
		 * the least acknowledged token among all the readers.
		 */
		if (likely(c != __RTE_QSBR_CNT_THR_OFFLINE && acked_token > c))
			acked_token = c;
	}

	/* All readers are checked, update least acknowledged token.
	 * There might be multiple writers trying to update this. There is
	 * no need to update this very accurately using compare-and-swap.
	 */
	if (acked_token != __RTE_QSBR_CNT_MAX)
		rte_atomic_store_explicit(&v->acked_token, acked_token,
			rte_memory_order_relaxed);

	return 1;
}

/**
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
static __rte_always_inline int
rte_rcu_qsbr_check(struct rte_rcu_qsbr *v, uint64_t t, bool wait)
{
	RTE_ASSERT(v != NULL);

	/* Check if all the readers have already acknowledged this token */
	if (likely(t <= v->acked_token)) {
		__RTE_RCU_DP_LOG(DEBUG,
			"%s: check: token = %" PRIu64 ", wait = %d",
			__func__, t, wait);
		__RTE_RCU_DP_LOG(DEBUG,
			"%s: status: least acked token = %" PRIu64,
			__func__, v->acked_token);
		return 1;
	}

	if (likely(v->num_threads == v->max_threads))
		return __rte_rcu_qsbr_check_all(v, t, wait);
	else
		return __rte_rcu_qsbr_check_selective(v, t, wait);
}

/**
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
void
rte_rcu_qsbr_synchronize(struct rte_rcu_qsbr *v, unsigned int thread_id);

/**
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
int
rte_rcu_qsbr_dump(FILE *f, struct rte_rcu_qsbr *v);

/**
 * Create a queue used to store the data structure elements that can
 * be freed later. This queue is referred to as 'defer queue'.
 *
 * @param params
 *   Parameters to create a defer queue.
 * @return
 *   On success - Valid pointer to defer queue
 *   On error - NULL
 *   Possible rte_errno codes are:
 *   - EINVAL - NULL parameters are passed
 *   - ENOMEM - Not enough memory
 */
struct rte_rcu_qsbr_dq *
rte_rcu_qsbr_dq_create(const struct rte_rcu_qsbr_dq_parameters *params);

/**
 * Enqueue one resource to the defer queue and start the grace period.
 * The resource will be freed later after at least one grace period
 * is over.
 *
 * If the defer queue is full, it will attempt to reclaim resources.
 * It will also reclaim resources at regular intervals to avoid
 * the defer queue from growing too big.
 *
 * Multi-thread safety is provided as the defer queue configuration.
 * When multi-thread safety is requested, it is possible that the
 * resources are not stored in their order of deletion. This results
 * in resources being held in the defer queue longer than they should.
 *
 * @param dq
 *   Defer queue to allocate an entry from.
 * @param e
 *   Pointer to resource data to copy to the defer queue. The size of
 *   the data to copy is equal to the element size provided when the
 *   defer queue was created.
 * @return
 *   On success - 0
 *   On error - 1 with rte_errno set to
 *   - EINVAL - NULL parameters are passed
 *   - ENOSPC - Defer queue is full. This condition can not happen
 *		if the defer queue size is equal (or larger) than the
 *		number of elements in the data structure.
 */
int
rte_rcu_qsbr_dq_enqueue(struct rte_rcu_qsbr_dq *dq, void *e);

/**
 * Free resources from the defer queue.
 *
 * This API is multi-thread safe.
 *
 * @param dq
 *   Defer queue to free an entry from.
 * @param n
 *   Maximum number of resources to free.
 * @param freed
 *   Number of resources that were freed.
 * @param pending
 *   Number of resources pending on the defer queue. This number might not
 *   be accurate if multi-thread safety is configured.
 * @param available
 *   Number of resources that can be added to the defer queue.
 *   This number might not be accurate if multi-thread safety is configured.
 * @return
 *   On successful reclamation of at least 1 resource - 0
 *   On error - 1 with rte_errno set to
 *   - EINVAL - NULL parameters are passed
 */
int
rte_rcu_qsbr_dq_reclaim(struct rte_rcu_qsbr_dq *dq, unsigned int n,
	unsigned int *freed, unsigned int *pending, unsigned int *available);

/**
 * Delete a defer queue.
 *
 * It tries to reclaim all the resources on the defer queue.
 * If any of the resources have not completed the grace period
 * the reclamation stops and returns immediately. The rest of
 * the resources are not reclaimed and the defer queue is not
 * freed.
 *
 * @param dq
 *   Defer queue to delete.
 * @return
 *   On success - 0
 *   On error - 1
 *   Possible rte_errno codes are:
 *   - EAGAIN - Some of the resources have not completed at least 1 grace
 *		period, try again.
 */
int
rte_rcu_qsbr_dq_delete(struct rte_rcu_qsbr_dq *dq);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RCU_QSBR_H_ */
