/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Inspired from FreeBSD src/sys/powerpc/include/atomic.h
 * Copyright (c) 2021 IBM Corporation
 * Copyright (c) 2008 Marcel Moolenaar
 * Copyright (c) 2001 Benno Rice
 * Copyright (c) 2001 David E. O'Brien
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 */

#ifndef _RTE_ATOMIC_PPC_64_H_
#define _RTE_ATOMIC_PPC_64_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "generic/rte_atomic.h"

#define	rte_mb()  asm volatile("sync" : : : "memory")

#define	rte_wmb() asm volatile("sync" : : : "memory")

#define	rte_rmb() asm volatile("sync" : : : "memory")

#define rte_smp_mb() rte_mb()

#define rte_smp_wmb() rte_wmb()

#define rte_smp_rmb() rte_rmb()

#define rte_io_mb() rte_mb()

#define rte_io_wmb() rte_wmb()

#define rte_io_rmb() rte_rmb()

static __rte_always_inline void
rte_atomic_thread_fence(rte_memory_order memorder)
{
	__rte_atomic_thread_fence(memorder);
}

/*------------------------- 16 bit atomic operations -------------------------*/
#ifndef RTE_FORCE_INTRINSICS
static inline int
rte_atomic16_cmpset(volatile uint16_t *dst, uint16_t exp, uint16_t src)
{
	return __atomic_compare_exchange(dst, &exp, &src, 0, rte_memory_order_acquire,
		rte_memory_order_acquire) ? 1 : 0;
}

static inline int rte_atomic16_test_and_set(rte_atomic16_t *v)
{
	return rte_atomic16_cmpset((volatile uint16_t *)&v->cnt, 0, 1);
}

static inline void
rte_atomic16_inc(rte_atomic16_t *v)
{
	rte_atomic_fetch_add_explicit(&v->cnt, 1, rte_memory_order_acquire);
}

static inline void
rte_atomic16_dec(rte_atomic16_t *v)
{
	rte_atomic_fetch_sub_explicit(&v->cnt, 1, rte_memory_order_acquire);
}

static inline int rte_atomic16_inc_and_test(rte_atomic16_t *v)
{
	return rte_atomic_fetch_add_explicit(&v->cnt, 1, rte_memory_order_acquire) + 1 == 0;
}

static inline int rte_atomic16_dec_and_test(rte_atomic16_t *v)
{
	return rte_atomic_fetch_sub_explicit(&v->cnt, 1, rte_memory_order_acquire) - 1 == 0;
}

static inline uint16_t
rte_atomic16_exchange(volatile uint16_t *dst, uint16_t val)
{
	return __atomic_exchange_2(dst, val, rte_memory_order_seq_cst);
}

/*------------------------- 32 bit atomic operations -------------------------*/

static inline int
rte_atomic32_cmpset(volatile uint32_t *dst, uint32_t exp, uint32_t src)
{
	return __atomic_compare_exchange(dst, &exp, &src, 0, rte_memory_order_acquire,
		rte_memory_order_acquire) ? 1 : 0;
}

static inline int rte_atomic32_test_and_set(rte_atomic32_t *v)
{
	return rte_atomic32_cmpset((volatile uint32_t *)&v->cnt, 0, 1);
}

static inline void
rte_atomic32_inc(rte_atomic32_t *v)
{
	rte_atomic_fetch_add_explicit(&v->cnt, 1, rte_memory_order_acquire);
}

static inline void
rte_atomic32_dec(rte_atomic32_t *v)
{
	rte_atomic_fetch_sub_explicit(&v->cnt, 1, rte_memory_order_acquire);
}

static inline int rte_atomic32_inc_and_test(rte_atomic32_t *v)
{
	return rte_atomic_fetch_add_explicit(&v->cnt, 1, rte_memory_order_acquire) + 1 == 0;
}

static inline int rte_atomic32_dec_and_test(rte_atomic32_t *v)
{
	return rte_atomic_fetch_sub_explicit(&v->cnt, 1, rte_memory_order_acquire) - 1 == 0;
}

static inline uint32_t
rte_atomic32_exchange(volatile uint32_t *dst, uint32_t val)
{
	return __atomic_exchange_4(dst, val, rte_memory_order_seq_cst);
}

/*------------------------- 64 bit atomic operations -------------------------*/

static inline int
rte_atomic64_cmpset(volatile uint64_t *dst, uint64_t exp, uint64_t src)
{
	return __atomic_compare_exchange(dst, &exp, &src, 0, rte_memory_order_acquire,
		rte_memory_order_acquire) ? 1 : 0;
}

static inline void
rte_atomic64_init(rte_atomic64_t *v)
{
	v->cnt = 0;
}

static inline int64_t
rte_atomic64_read(rte_atomic64_t *v)
{
	return v->cnt;
}

static inline void
rte_atomic64_set(rte_atomic64_t *v, int64_t new_value)
{
	v->cnt = new_value;
}

static inline void
rte_atomic64_add(rte_atomic64_t *v, int64_t inc)
{
	rte_atomic_fetch_add_explicit(&v->cnt, inc, rte_memory_order_acquire);
}

static inline void
rte_atomic64_sub(rte_atomic64_t *v, int64_t dec)
{
	rte_atomic_fetch_sub_explicit(&v->cnt, dec, rte_memory_order_acquire);
}

static inline void
rte_atomic64_inc(rte_atomic64_t *v)
{
	rte_atomic_fetch_add_explicit(&v->cnt, 1, rte_memory_order_acquire);
}

static inline void
rte_atomic64_dec(rte_atomic64_t *v)
{
	rte_atomic_fetch_sub_explicit(&v->cnt, 1, rte_memory_order_acquire);
}

static inline int64_t
rte_atomic64_add_return(rte_atomic64_t *v, int64_t inc)
{
	return rte_atomic_fetch_add_explicit(&v->cnt, inc, rte_memory_order_acquire) + inc;
}

static inline int64_t
rte_atomic64_sub_return(rte_atomic64_t *v, int64_t dec)
{
	return rte_atomic_fetch_sub_explicit(&v->cnt, dec, rte_memory_order_acquire) - dec;
}

static inline int rte_atomic64_inc_and_test(rte_atomic64_t *v)
{
	return rte_atomic_fetch_add_explicit(&v->cnt, 1, rte_memory_order_acquire) + 1 == 0;
}

static inline int rte_atomic64_dec_and_test(rte_atomic64_t *v)
{
	return rte_atomic_fetch_sub_explicit(&v->cnt, 1, rte_memory_order_acquire) - 1 == 0;
}

static inline int rte_atomic64_test_and_set(rte_atomic64_t *v)
{
	return rte_atomic64_cmpset((volatile uint64_t *)&v->cnt, 0, 1);
}

static inline void rte_atomic64_clear(rte_atomic64_t *v)
{
	v->cnt = 0;
}

static inline uint64_t
rte_atomic64_exchange(volatile uint64_t *dst, uint64_t val)
{
	return __atomic_exchange_8(dst, val, rte_memory_order_seq_cst);
}

#endif

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ATOMIC_PPC_64_H_ */
