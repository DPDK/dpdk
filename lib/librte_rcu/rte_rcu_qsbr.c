/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2018 Arm Limited
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_atomic.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_errno.h>

#include "rte_rcu_qsbr.h"

/* Get the memory size of QSBR variable */
size_t __rte_experimental
rte_rcu_qsbr_get_memsize(uint32_t max_threads)
{
	size_t sz;

	if (max_threads == 0) {
		rte_log(RTE_LOG_ERR, rte_rcu_log_type,
			"%s(): Invalid max_threads %u\n",
			__func__, max_threads);
		rte_errno = EINVAL;

		return 1;
	}

	sz = sizeof(struct rte_rcu_qsbr);

	/* Add the size of quiescent state counter array */
	sz += sizeof(struct rte_rcu_qsbr_cnt) * max_threads;

	/* Add the size of the registered thread ID bitmap array */
	sz += __RTE_QSBR_THRID_ARRAY_SIZE(max_threads);

	return sz;
}

/* Initialize a quiescent state variable */
int __rte_experimental
rte_rcu_qsbr_init(struct rte_rcu_qsbr *v, uint32_t max_threads)
{
	size_t sz;

	if (v == NULL) {
		rte_log(RTE_LOG_ERR, rte_rcu_log_type,
			"%s(): Invalid input parameter\n", __func__);
		rte_errno = EINVAL;

		return 1;
	}

	sz = rte_rcu_qsbr_get_memsize(max_threads);
	if (sz == 1)
		return 1;

	/* Set all the threads to offline */
	memset(v, 0, sz);
	v->max_threads = max_threads;
	v->num_elems = RTE_ALIGN_MUL_CEIL(max_threads,
			__RTE_QSBR_THRID_ARRAY_ELM_SIZE) /
			__RTE_QSBR_THRID_ARRAY_ELM_SIZE;
	v->token = __RTE_QSBR_CNT_INIT;

	return 0;
}

/* Register a reader thread to report its quiescent state
 * on a QS variable.
 */
int __rte_experimental
rte_rcu_qsbr_thread_register(struct rte_rcu_qsbr *v, unsigned int thread_id)
{
	unsigned int i, id, success;
	uint64_t old_bmap, new_bmap;

	if (v == NULL || thread_id >= v->max_threads) {
		rte_log(RTE_LOG_ERR, rte_rcu_log_type,
			"%s(): Invalid input parameter\n", __func__);
		rte_errno = EINVAL;

		return 1;
	}

	__RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, ERR, "Lock counter %u\n",
				v->qsbr_cnt[thread_id].lock_cnt);

	id = thread_id & __RTE_QSBR_THRID_MASK;
	i = thread_id >> __RTE_QSBR_THRID_INDEX_SHIFT;

	/* Make sure that the counter for registered threads does not
	 * go out of sync. Hence, additional checks are required.
	 */
	/* Check if the thread is already registered */
	old_bmap = __atomic_load_n(__RTE_QSBR_THRID_ARRAY_ELM(v, i),
					__ATOMIC_RELAXED);
	if (old_bmap & 1UL << id)
		return 0;

	do {
		new_bmap = old_bmap | (1UL << id);
		success = __atomic_compare_exchange(
					__RTE_QSBR_THRID_ARRAY_ELM(v, i),
					&old_bmap, &new_bmap, 0,
					__ATOMIC_RELEASE, __ATOMIC_RELAXED);

		if (success)
			__atomic_fetch_add(&v->num_threads,
						1, __ATOMIC_RELAXED);
		else if (old_bmap & (1UL << id))
			/* Someone else registered this thread.
			 * Counter should not be incremented.
			 */
			return 0;
	} while (success == 0);

	return 0;
}

/* Remove a reader thread, from the list of threads reporting their
 * quiescent state on a QS variable.
 */
int __rte_experimental
rte_rcu_qsbr_thread_unregister(struct rte_rcu_qsbr *v, unsigned int thread_id)
{
	unsigned int i, id, success;
	uint64_t old_bmap, new_bmap;

	if (v == NULL || thread_id >= v->max_threads) {
		rte_log(RTE_LOG_ERR, rte_rcu_log_type,
			"%s(): Invalid input parameter\n", __func__);
		rte_errno = EINVAL;

		return 1;
	}

	__RTE_RCU_IS_LOCK_CNT_ZERO(v, thread_id, ERR, "Lock counter %u\n",
				v->qsbr_cnt[thread_id].lock_cnt);

	id = thread_id & __RTE_QSBR_THRID_MASK;
	i = thread_id >> __RTE_QSBR_THRID_INDEX_SHIFT;

	/* Make sure that the counter for registered threads does not
	 * go out of sync. Hence, additional checks are required.
	 */
	/* Check if the thread is already unregistered */
	old_bmap = __atomic_load_n(__RTE_QSBR_THRID_ARRAY_ELM(v, i),
					__ATOMIC_RELAXED);
	if (old_bmap & ~(1UL << id))
		return 0;

	do {
		new_bmap = old_bmap & ~(1UL << id);
		/* Make sure any loads of the shared data structure are
		 * completed before removal of the thread from the list of
		 * reporting threads.
		 */
		success = __atomic_compare_exchange(
					__RTE_QSBR_THRID_ARRAY_ELM(v, i),
					&old_bmap, &new_bmap, 0,
					__ATOMIC_RELEASE, __ATOMIC_RELAXED);

		if (success)
			__atomic_fetch_sub(&v->num_threads,
						1, __ATOMIC_RELAXED);
		else if (old_bmap & ~(1UL << id))
			/* Someone else unregistered this thread.
			 * Counter should not be incremented.
			 */
			return 0;
	} while (success == 0);

	return 0;
}

/* Wait till the reader threads have entered quiescent state. */
void __rte_experimental
rte_rcu_qsbr_synchronize(struct rte_rcu_qsbr *v, unsigned int thread_id)
{
	uint64_t t;

	RTE_ASSERT(v != NULL);

	t = rte_rcu_qsbr_start(v);

	/* If the current thread has readside critical section,
	 * update its quiescent state status.
	 */
	if (thread_id != RTE_QSBR_THRID_INVALID)
		rte_rcu_qsbr_quiescent(v, thread_id);

	/* Wait for other readers to enter quiescent state */
	rte_rcu_qsbr_check(v, t, true);
}

/* Dump the details of a single quiescent state variable to a file. */
int __rte_experimental
rte_rcu_qsbr_dump(FILE *f, struct rte_rcu_qsbr *v)
{
	uint64_t bmap;
	uint32_t i, t, id;

	if (v == NULL || f == NULL) {
		rte_log(RTE_LOG_ERR, rte_rcu_log_type,
			"%s(): Invalid input parameter\n", __func__);
		rte_errno = EINVAL;

		return 1;
	}

	fprintf(f, "\nQuiescent State Variable @%p\n", v);

	fprintf(f, "  QS variable memory size = %zu\n",
				rte_rcu_qsbr_get_memsize(v->max_threads));
	fprintf(f, "  Given # max threads = %u\n", v->max_threads);
	fprintf(f, "  Current # threads = %u\n", v->num_threads);

	fprintf(f, "  Registered thread IDs = ");
	for (i = 0; i < v->num_elems; i++) {
		bmap = __atomic_load_n(__RTE_QSBR_THRID_ARRAY_ELM(v, i),
					__ATOMIC_ACQUIRE);
		id = i << __RTE_QSBR_THRID_INDEX_SHIFT;
		while (bmap) {
			t = __builtin_ctzl(bmap);
			fprintf(f, "%d ", id + t);

			bmap &= ~(1UL << t);
		}
	}

	fprintf(f, "\n");

	fprintf(f, "  Token = %"PRIu64"\n",
			__atomic_load_n(&v->token, __ATOMIC_ACQUIRE));

	fprintf(f, "Quiescent State Counts for readers:\n");
	for (i = 0; i < v->num_elems; i++) {
		bmap = __atomic_load_n(__RTE_QSBR_THRID_ARRAY_ELM(v, i),
					__ATOMIC_ACQUIRE);
		id = i << __RTE_QSBR_THRID_INDEX_SHIFT;
		while (bmap) {
			t = __builtin_ctzl(bmap);
			fprintf(f, "thread ID = %d, count = %"PRIu64", lock count = %u\n",
				id + t,
				__atomic_load_n(
					&v->qsbr_cnt[id + t].cnt,
					__ATOMIC_RELAXED),
				__atomic_load_n(
					&v->qsbr_cnt[id + t].lock_cnt,
					__ATOMIC_RELAXED));
			bmap &= ~(1UL << t);
		}
	}

	return 0;
}

int rte_rcu_log_type;

RTE_INIT(rte_rcu_register)
{
	rte_rcu_log_type = rte_log_register("lib.rcu");
	if (rte_rcu_log_type >= 0)
		rte_log_set_level(rte_rcu_log_type, RTE_LOG_ERR);
}
