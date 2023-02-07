/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) IBM Corporation 2014.
 */

#ifndef _RTE_SPINLOCK_PPC_64_H_
#define _RTE_SPINLOCK_PPC_64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_pause.h>
#include "generic/rte_spinlock.h"

/* Fixme: Use intrinsics to implement the spinlock on Power architecture */

#ifndef RTE_FORCE_INTRINSICS

static inline void
rte_spinlock_lock(rte_spinlock_t *sl)
	__rte_no_thread_safety_analysis
{
	while (__sync_lock_test_and_set(&sl->locked, 1))
		while (sl->locked)
			rte_pause();
}

static inline void
rte_spinlock_unlock(rte_spinlock_t *sl)
	__rte_no_thread_safety_analysis
{
	__sync_lock_release(&sl->locked);
}

static inline int
rte_spinlock_trylock(rte_spinlock_t *sl)
	__rte_no_thread_safety_analysis
{
	return __sync_lock_test_and_set(&sl->locked, 1) == 0;
}

#endif

static inline int rte_tm_supported(void)
{
	return 0;
}

static inline void
rte_spinlock_lock_tm(rte_spinlock_t *sl)
{
	rte_spinlock_lock(sl); /* fall-back */
}

static inline int
rte_spinlock_trylock_tm(rte_spinlock_t *sl)
{
	return rte_spinlock_trylock(sl);
}

static inline void
rte_spinlock_unlock_tm(rte_spinlock_t *sl)
{
	rte_spinlock_unlock(sl);
}

static inline void
rte_spinlock_recursive_lock_tm(rte_spinlock_recursive_t *slr)
{
	rte_spinlock_recursive_lock(slr); /* fall-back */
}

static inline void
rte_spinlock_recursive_unlock_tm(rte_spinlock_recursive_t *slr)
{
	rte_spinlock_recursive_unlock(slr);
}

static inline int
rte_spinlock_recursive_trylock_tm(rte_spinlock_recursive_t *slr)
{
	return rte_spinlock_recursive_trylock(slr);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_SPINLOCK_PPC_64_H_ */
