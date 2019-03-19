/*
 *   BSD LICENSE
 *
 *   Copyright (C) IBM Corporation 2014.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of IBM Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * Inspired from FreeBSD src/sys/powerpc/include/atomic.h
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

#define rte_cio_wmb() rte_wmb()

#define rte_cio_rmb() rte_rmb()

/*------------------------- 16 bit atomic operations -------------------------*/
/* To be compatible with Power7, use GCC built-in functions for 16 bit
 * operations */

#ifndef RTE_FORCE_INTRINSICS
static inline int
rte_atomic16_cmpset(volatile uint16_t *dst, uint16_t exp, uint16_t src)
{
	return __atomic_compare_exchange(dst, &exp, &src, 0, __ATOMIC_ACQUIRE,
		__ATOMIC_ACQUIRE) ? 1 : 0;
}

static inline int rte_atomic16_test_and_set(rte_atomic16_t *v)
{
	return rte_atomic16_cmpset((volatile uint16_t *)&v->cnt, 0, 1);
}

static inline void
rte_atomic16_inc(rte_atomic16_t *v)
{
	__atomic_add_fetch(&v->cnt, 1, __ATOMIC_ACQUIRE);
}

static inline void
rte_atomic16_dec(rte_atomic16_t *v)
{
	__atomic_sub_fetch(&v->cnt, 1, __ATOMIC_ACQUIRE);
}

static inline int rte_atomic16_inc_and_test(rte_atomic16_t *v)
{
	return __atomic_add_fetch(&v->cnt, 1, __ATOMIC_ACQUIRE) == 0;
}

static inline int rte_atomic16_dec_and_test(rte_atomic16_t *v)
{
	return __atomic_sub_fetch(&v->cnt, 1, __ATOMIC_ACQUIRE) == 0;
}

static inline uint16_t
rte_atomic16_exchange(volatile uint16_t *dst, uint16_t val)
{
	return __atomic_exchange_2(dst, val, __ATOMIC_SEQ_CST);
}

/*------------------------- 32 bit atomic operations -------------------------*/

static inline int
rte_atomic32_cmpset(volatile uint32_t *dst, uint32_t exp, uint32_t src)
{
	unsigned int ret = 0;

	asm volatile(
			"\tlwsync\n"
			"1:\tlwarx %[ret], 0, %[dst]\n"
			"cmplw %[exp], %[ret]\n"
			"bne 2f\n"
			"stwcx. %[src], 0, %[dst]\n"
			"bne- 1b\n"
			"li %[ret], 1\n"
			"b 3f\n"
			"2:\n"
			"stwcx. %[ret], 0, %[dst]\n"
			"li %[ret], 0\n"
			"3:\n"
			"isync\n"
			: [ret] "=&r" (ret), "=m" (*dst)
			: [dst] "r" (dst),
			  [exp] "r" (exp),
			  [src] "r" (src),
			  "m" (*dst)
			: "cc", "memory");

	return ret;
}

static inline int rte_atomic32_test_and_set(rte_atomic32_t *v)
{
	return rte_atomic32_cmpset((volatile uint32_t *)&v->cnt, 0, 1);
}

static inline void
rte_atomic32_inc(rte_atomic32_t *v)
{
	int t;

	asm volatile(
			"1: lwarx %[t],0,%[cnt]\n"
			"addic %[t],%[t],1\n"
			"stwcx. %[t],0,%[cnt]\n"
			"bne- 1b\n"
			: [t] "=&r" (t), "=m" (v->cnt)
			: [cnt] "r" (&v->cnt), "m" (v->cnt)
			: "cc", "xer", "memory");
}

static inline void
rte_atomic32_dec(rte_atomic32_t *v)
{
	int t;

	asm volatile(
			"1: lwarx %[t],0,%[cnt]\n"
			"addic %[t],%[t],-1\n"
			"stwcx. %[t],0,%[cnt]\n"
			"bne- 1b\n"
			: [t] "=&r" (t), "=m" (v->cnt)
			: [cnt] "r" (&v->cnt), "m" (v->cnt)
			: "cc", "xer", "memory");
}

static inline int rte_atomic32_inc_and_test(rte_atomic32_t *v)
{
	int ret;

	asm volatile(
			"\n\tlwsync\n"
			"1: lwarx %[ret],0,%[cnt]\n"
			"addic	%[ret],%[ret],1\n"
			"stwcx. %[ret],0,%[cnt]\n"
			"bne- 1b\n"
			"isync\n"
			: [ret] "=&r" (ret)
			: [cnt] "r" (&v->cnt)
			: "cc", "xer", "memory");

	return ret == 0;
}

static inline int rte_atomic32_dec_and_test(rte_atomic32_t *v)
{
	int ret;

	asm volatile(
			"\n\tlwsync\n"
			"1: lwarx %[ret],0,%[cnt]\n"
			"addic %[ret],%[ret],-1\n"
			"stwcx. %[ret],0,%[cnt]\n"
			"bne- 1b\n"
			"isync\n"
			: [ret] "=&r" (ret)
			: [cnt] "r" (&v->cnt)
			: "cc", "xer", "memory");

	return ret == 0;
}

static inline uint32_t
rte_atomic32_exchange(volatile uint32_t *dst, uint32_t val)
{
	return __atomic_exchange_4(dst, val, __ATOMIC_SEQ_CST);
}

/*------------------------- 64 bit atomic operations -------------------------*/

static inline int
rte_atomic64_cmpset(volatile uint64_t *dst, uint64_t exp, uint64_t src)
{
	unsigned int ret = 0;

	asm volatile (
			"\tlwsync\n"
			"1: ldarx %[ret], 0, %[dst]\n"
			"cmpld %[exp], %[ret]\n"
			"bne 2f\n"
			"stdcx. %[src], 0, %[dst]\n"
			"bne- 1b\n"
			"li %[ret], 1\n"
			"b 3f\n"
			"2:\n"
			"stdcx. %[ret], 0, %[dst]\n"
			"li %[ret], 0\n"
			"3:\n"
			"isync\n"
			: [ret] "=&r" (ret), "=m" (*dst)
			: [dst] "r" (dst),
			  [exp] "r" (exp),
			  [src] "r" (src),
			  "m" (*dst)
			: "cc", "memory");
	return ret;
}

static inline void
rte_atomic64_init(rte_atomic64_t *v)
{
	v->cnt = 0;
}

static inline int64_t
rte_atomic64_read(rte_atomic64_t *v)
{
	long ret;

	asm volatile("ld%U1%X1 %[ret],%[cnt]"
		: [ret] "=r"(ret)
		: [cnt] "m"(v->cnt));

	return ret;
}

static inline void
rte_atomic64_set(rte_atomic64_t *v, int64_t new_value)
{
	asm volatile("std%U0%X0 %[new_value],%[cnt]"
		: [cnt] "=m"(v->cnt)
		: [new_value] "r"(new_value));
}

static inline void
rte_atomic64_add(rte_atomic64_t *v, int64_t inc)
{
	long t;

	asm volatile(
			"1: ldarx %[t],0,%[cnt]\n"
			"add %[t],%[inc],%[t]\n"
			"stdcx. %[t],0,%[cnt]\n"
			"bne- 1b\n"
			: [t] "=&r" (t), "=m" (v->cnt)
			: [cnt] "r" (&v->cnt), [inc] "r" (inc), "m" (v->cnt)
			: "cc", "memory");
}

static inline void
rte_atomic64_sub(rte_atomic64_t *v, int64_t dec)
{
	long t;

	asm volatile(
			"1: ldarx %[t],0,%[cnt]\n"
			"subf %[t],%[dec],%[t]\n"
			"stdcx. %[t],0,%[cnt]\n"
			"bne- 1b\n"
			: [t] "=&r" (t), "+m" (v->cnt)
			: [cnt] "r" (&v->cnt), [dec] "r" (dec), "m" (v->cnt)
			: "cc", "memory");
}

static inline void
rte_atomic64_inc(rte_atomic64_t *v)
{
	long t;

	asm volatile(
			"1: ldarx %[t],0,%[cnt]\n"
			"addic %[t],%[t],1\n"
			"stdcx. %[t],0,%[cnt]\n"
			"bne- 1b\n"
			: [t] "=&r" (t), "+m" (v->cnt)
			: [cnt] "r" (&v->cnt), "m" (v->cnt)
			: "cc", "xer", "memory");
}

static inline void
rte_atomic64_dec(rte_atomic64_t *v)
{
	long t;

	asm volatile(
			"1: ldarx %[t],0,%[cnt]\n"
			"addic %[t],%[t],-1\n"
			"stdcx. %[t],0,%[cnt]\n"
			"bne- 1b\n"
			: [t] "=&r" (t), "+m" (v->cnt)
			: [cnt] "r" (&v->cnt), "m" (v->cnt)
			: "cc", "xer", "memory");
}

static inline int64_t
rte_atomic64_add_return(rte_atomic64_t *v, int64_t inc)
{
	long ret;

	asm volatile(
			"\n\tlwsync\n"
			"1: ldarx %[ret],0,%[cnt]\n"
			"add %[ret],%[inc],%[ret]\n"
			"stdcx. %[ret],0,%[cnt]\n"
			"bne- 1b\n"
			"isync\n"
			: [ret] "=&r" (ret)
			: [inc] "r" (inc), [cnt] "r" (&v->cnt)
			: "cc", "memory");

	return ret;
}

static inline int64_t
rte_atomic64_sub_return(rte_atomic64_t *v, int64_t dec)
{
	long ret;

	asm volatile(
			"\n\tlwsync\n"
			"1: ldarx %[ret],0,%[cnt]\n"
			"subf %[ret],%[dec],%[ret]\n"
			"stdcx. %[ret],0,%[cnt]\n"
			"bne- 1b\n"
			"isync\n"
			: [ret] "=&r" (ret)
			: [dec] "r" (dec), [cnt] "r" (&v->cnt)
			: "cc", "memory");

	return ret;
}

static inline int rte_atomic64_inc_and_test(rte_atomic64_t *v)
{
	long ret;

	asm volatile(
			"\n\tlwsync\n"
			"1: ldarx %[ret],0,%[cnt]\n"
			"addic %[ret],%[ret],1\n"
			"stdcx. %[ret],0,%[cnt]\n"
			"bne- 1b\n"
			"isync\n"
			: [ret] "=&r" (ret)
			: [cnt] "r" (&v->cnt)
			: "cc", "xer", "memory");

	return ret == 0;
}

static inline int rte_atomic64_dec_and_test(rte_atomic64_t *v)
{
	long ret;

	asm volatile(
			"\n\tlwsync\n"
			"1: ldarx %[ret],0,%[cnt]\n"
			"addic %[ret],%[ret],-1\n"
			"stdcx. %[ret],0,%[cnt]\n"
			"bne- 1b\n"
			"isync\n"
			: [ret] "=&r" (ret)
			: [cnt] "r" (&v->cnt)
			: "cc", "xer", "memory");

	return ret == 0;
}

static inline int rte_atomic64_test_and_set(rte_atomic64_t *v)
{
	return rte_atomic64_cmpset((volatile uint64_t *)&v->cnt, 0, 1);
}
/**
 * Atomically set a 64-bit counter to 0.
 *
 * @param v
 *   A pointer to the atomic counter.
 */
static inline void rte_atomic64_clear(rte_atomic64_t *v)
{
	v->cnt = 0;
}

static inline uint64_t
rte_atomic64_exchange(volatile uint64_t *dst, uint64_t val)
{
	return __atomic_exchange_4(dst, val, __ATOMIC_SEQ_CST);
}

#endif

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ATOMIC_PPC_64_H_ */
