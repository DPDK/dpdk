/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * Inspired from FreeBSD src/sys/amd64/include/atomic.h
 * Copyright (c) 1998 Doug Rabson
 * Copyright (c) 2019 Intel Corporation
 * All rights reserved.
 */

#ifndef _RTE_ATOMIC_X86_H_
#error do not include this file directly, use <rte_atomic.h> instead
#endif

#ifndef _RTE_ATOMIC_X86_64_H_
#define _RTE_ATOMIC_X86_64_H_

#include <stdint.h>
#include <rte_common.h>
#include <rte_compat.h>
#include <rte_atomic.h>

/*------------------------- 64 bit atomic operations -------------------------*/

#ifndef RTE_FORCE_INTRINSICS
static inline int
rte_atomic64_cmpset(volatile uint64_t *dst, uint64_t exp, uint64_t src)
{
	uint8_t res;


	asm volatile(
			MPLOCKED
			"cmpxchgq %[src], %[dst];"
			"sete %[res];"
			: [res] "=a" (res),     /* output */
			  [dst] "=m" (*dst)
			: [src] "r" (src),      /* input */
			  "a" (exp),
			  "m" (*dst)
			: "memory");            /* no-clobber list */

	return res;
}

static inline uint64_t
rte_atomic64_exchange(volatile uint64_t *dst, uint64_t val)
{
	asm volatile(
			MPLOCKED
			"xchgq %0, %1;"
			: "=r" (val), "=m" (*dst)
			: "0" (val),  "m" (*dst)
			: "memory");         /* no-clobber list */
	return val;
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
	asm volatile(
			MPLOCKED
			"addq %[inc], %[cnt]"
			: [cnt] "=m" (v->cnt)   /* output */
			: [inc] "ir" (inc),     /* input */
			  "m" (v->cnt)
			);
}

static inline void
rte_atomic64_sub(rte_atomic64_t *v, int64_t dec)
{
	asm volatile(
			MPLOCKED
			"subq %[dec], %[cnt]"
			: [cnt] "=m" (v->cnt)   /* output */
			: [dec] "ir" (dec),     /* input */
			  "m" (v->cnt)
			);
}

static inline void
rte_atomic64_inc(rte_atomic64_t *v)
{
	asm volatile(
			MPLOCKED
			"incq %[cnt]"
			: [cnt] "=m" (v->cnt)   /* output */
			: "m" (v->cnt)          /* input */
			);
}

static inline void
rte_atomic64_dec(rte_atomic64_t *v)
{
	asm volatile(
			MPLOCKED
			"decq %[cnt]"
			: [cnt] "=m" (v->cnt)   /* output */
			: "m" (v->cnt)          /* input */
			);
}

static inline int64_t
rte_atomic64_add_return(rte_atomic64_t *v, int64_t inc)
{
	int64_t prev = inc;

	asm volatile(
			MPLOCKED
			"xaddq %[prev], %[cnt]"
			: [prev] "+r" (prev),   /* output */
			  [cnt] "=m" (v->cnt)
			: "m" (v->cnt)          /* input */
			);
	return prev + inc;
}

static inline int64_t
rte_atomic64_sub_return(rte_atomic64_t *v, int64_t dec)
{
	return rte_atomic64_add_return(v, -dec);
}

static inline int rte_atomic64_inc_and_test(rte_atomic64_t *v)
{
	uint8_t ret;

	asm volatile(
			MPLOCKED
			"incq %[cnt] ; "
			"sete %[ret]"
			: [cnt] "+m" (v->cnt), /* output */
			  [ret] "=qm" (ret)
			);

	return ret != 0;
}

static inline int rte_atomic64_dec_and_test(rte_atomic64_t *v)
{
	uint8_t ret;

	asm volatile(
			MPLOCKED
			"decq %[cnt] ; "
			"sete %[ret]"
			: [cnt] "+m" (v->cnt),  /* output */
			  [ret] "=qm" (ret)
			);
	return ret != 0;
}

static inline int rte_atomic64_test_and_set(rte_atomic64_t *v)
{
	return rte_atomic64_cmpset((volatile uint64_t *)&v->cnt, 0, 1);
}

static inline void rte_atomic64_clear(rte_atomic64_t *v)
{
	v->cnt = 0;
}
#endif

/*------------------------ 128 bit atomic operations -------------------------*/

/**
 * 128-bit integer structure.
 */
RTE_STD_C11
typedef struct {
	RTE_STD_C11
	union {
		uint64_t val[2];
		__extension__ __int128 int128;
	};
} __rte_aligned(16) rte_int128_t;

static inline int __rte_experimental
rte_atomic128_cmp_exchange(rte_int128_t *dst,
			   rte_int128_t *exp,
			   const rte_int128_t *src,
			   unsigned int weak,
			   int success,
			   int failure)
{
	RTE_SET_USED(weak);
	RTE_SET_USED(success);
	RTE_SET_USED(failure);
	uint8_t res;

	asm volatile (
		      MPLOCKED
		      "cmpxchg16b %[dst];"
		      " sete %[res]"
		      : [dst] "=m" (dst->val[0]),
			"=a" (exp->val[0]),
			"=d" (exp->val[1]),
			[res] "=r" (res)
		      : "b" (src->val[0]),
			"c" (src->val[1]),
			"a" (exp->val[0]),
			"d" (exp->val[1]),
			"m" (dst->val[0])
		      : "memory");

	return res;
}

#endif /* _RTE_ATOMIC_X86_64_H_ */
