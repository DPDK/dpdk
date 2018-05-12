/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_SPINLOCK_X86_64_H_
#define _RTE_SPINLOCK_X86_64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "generic/rte_spinlock.h"
#include "rte_rtm.h"
#include "rte_cpuflags.h"
#include "rte_branch_prediction.h"
#include "rte_common.h"
#include "rte_pause.h"

#define RTE_RTM_MAX_RETRIES (10)
#define RTE_XABORT_LOCK_BUSY (0xff)

#ifndef RTE_FORCE_INTRINSICS
static inline void
rte_spinlock_lock(rte_spinlock_t *sl)
{
	int lock_val = 1;
	asm volatile (
			"1:\n"
			"xchg %[locked], %[lv]\n"
			"test %[lv], %[lv]\n"
			"jz 3f\n"
			"2:\n"
			"pause\n"
			"cmpl $0, %[locked]\n"
			"jnz 2b\n"
			"jmp 1b\n"
			"3:\n"
			: [locked] "=m" (sl->locked), [lv] "=q" (lock_val)
			: "[lv]" (lock_val)
			: "memory");
}

static inline void
rte_spinlock_unlock (rte_spinlock_t *sl)
{
	int unlock_val = 0;
	asm volatile (
			"xchg %[locked], %[ulv]\n"
			: [locked] "=m" (sl->locked), [ulv] "=q" (unlock_val)
			: "[ulv]" (unlock_val)
			: "memory");
}

static inline int
rte_spinlock_trylock (rte_spinlock_t *sl)
{
	int lockval = 1;

	asm volatile (
			"xchg %[locked], %[lockval]"
			: [locked] "=m" (sl->locked), [lockval] "=q" (lockval)
			: "[lockval]" (lockval)
			: "memory");

	return lockval == 0;
}
#endif

extern uint8_t rte_rtm_supported;

static inline int rte_tm_supported(void)
{
	return rte_rtm_supported;
}

static inline int
rte_try_tm(volatile int *lock)
{
	int retries;

	if (!rte_rtm_supported)
		return 0;

	retries = RTE_RTM_MAX_RETRIES;

	while (likely(retries--)) {

		unsigned int status = rte_xbegin();

		if (likely(RTE_XBEGIN_STARTED == status)) {
			if (unlikely(*lock))
				rte_xabort(RTE_XABORT_LOCK_BUSY);
			else
				return 1;
		}
		while (*lock)
			rte_pause();

		if ((status & RTE_XABORT_EXPLICIT) &&
			(RTE_XABORT_CODE(status) == RTE_XABORT_LOCK_BUSY))
			continue;

		if ((status & RTE_XABORT_RETRY) == 0) /* do not retry */
			break;
	}
	return 0;
}

static inline void
rte_spinlock_lock_tm(rte_spinlock_t *sl)
{
	if (likely(rte_try_tm(&sl->locked)))
		return;

	rte_spinlock_lock(sl); /* fall-back */
}

static inline int
rte_spinlock_trylock_tm(rte_spinlock_t *sl)
{
	if (likely(rte_try_tm(&sl->locked)))
		return 1;

	return rte_spinlock_trylock(sl);
}

static inline void
rte_spinlock_unlock_tm(rte_spinlock_t *sl)
{
	if (unlikely(sl->locked))
		rte_spinlock_unlock(sl);
	else
		rte_xend();
}

static inline void
rte_spinlock_recursive_lock_tm(rte_spinlock_recursive_t *slr)
{
	if (likely(rte_try_tm(&slr->sl.locked)))
		return;

	rte_spinlock_recursive_lock(slr); /* fall-back */
}

static inline void
rte_spinlock_recursive_unlock_tm(rte_spinlock_recursive_t *slr)
{
	if (unlikely(slr->sl.locked))
		rte_spinlock_recursive_unlock(slr);
	else
		rte_xend();
}

static inline int
rte_spinlock_recursive_trylock_tm(rte_spinlock_recursive_t *slr)
{
	if (likely(rte_try_tm(&slr->sl.locked)))
		return 1;

	return rte_spinlock_recursive_trylock(slr);
}


#ifdef __cplusplus
}
#endif

#endif /* _RTE_SPINLOCK_X86_64_H_ */
