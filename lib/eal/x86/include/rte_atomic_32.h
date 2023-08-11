/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 */

/*
 * Inspired from FreeBSD src/sys/i386/include/atomic.h
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 */

#ifndef _RTE_ATOMIC_X86_H_
#error do not include this file directly, use <rte_atomic.h> instead
#endif

#ifndef _RTE_ATOMIC_I686_H_
#define _RTE_ATOMIC_I686_H_

#include <stdint.h>
#include <rte_common.h>

/*------------------------- 64 bit atomic operations -------------------------*/

#ifndef RTE_FORCE_INTRINSICS
static inline int
rte_atomic64_cmpset(volatile uint64_t *dst, uint64_t exp, uint64_t src)
{
	uint8_t res;
	union {
		struct {
			uint32_t l32;
			uint32_t h32;
		};
		uint64_t u64;
	} _exp, _src;

	_exp.u64 = exp;
	_src.u64 = src;

#ifndef __PIC__
    asm volatile (
            MPLOCKED
            "cmpxchg8b (%[dst]);"
            "setz %[res];"
            : [res] "=a" (res)      /* result in eax */
            : [dst] "S" (dst),      /* esi */
             "b" (_src.l32),       /* ebx */
             "c" (_src.h32),       /* ecx */
             "a" (_exp.l32),       /* eax */
             "d" (_exp.h32)        /* edx */
			: "memory" );           /* no-clobber list */
#else
	asm volatile (
            "xchgl %%ebx, %%edi;\n"
			MPLOCKED
			"cmpxchg8b (%[dst]);"
			"setz %[res];"
            "xchgl %%ebx, %%edi;\n"
			: [res] "=a" (res)      /* result in eax */
			: [dst] "S" (dst),      /* esi */
			  "D" (_src.l32),       /* ebx */
			  "c" (_src.h32),       /* ecx */
			  "a" (_exp.l32),       /* eax */
			  "d" (_exp.h32)        /* edx */
			: "memory" );           /* no-clobber list */
#endif

	return res;
}

static inline uint64_t
rte_atomic64_exchange(volatile uint64_t *dest, uint64_t val)
{
	uint64_t old;

	do {
		old = *dest;
	} while (rte_atomic64_cmpset(dest, old, val) == 0);

	return old;
}

static inline void
rte_atomic64_init(rte_atomic64_t *v)
{
	int success = 0;
	uint64_t tmp;

	while (success == 0) {
		tmp = v->cnt;
		success = rte_atomic64_cmpset((volatile uint64_t *)&v->cnt,
		                              tmp, 0);
	}
}

static inline int64_t
rte_atomic64_read(rte_atomic64_t *v)
{
	int success = 0;
	uint64_t tmp;

	while (success == 0) {
		tmp = v->cnt;
		/* replace the value by itself */
		success = rte_atomic64_cmpset((volatile uint64_t *)&v->cnt,
		                              tmp, tmp);
	}
	return tmp;
}

static inline void
rte_atomic64_set(rte_atomic64_t *v, int64_t new_value)
{
	int success = 0;
	uint64_t tmp;

	while (success == 0) {
		tmp = v->cnt;
		success = rte_atomic64_cmpset((volatile uint64_t *)&v->cnt,
		                              tmp, new_value);
	}
}

static inline void
rte_atomic64_add(rte_atomic64_t *v, int64_t inc)
{
	int success = 0;
	uint64_t tmp;

	while (success == 0) {
		tmp = v->cnt;
		success = rte_atomic64_cmpset((volatile uint64_t *)&v->cnt,
		                              tmp, tmp + inc);
	}
}

static inline void
rte_atomic64_sub(rte_atomic64_t *v, int64_t dec)
{
	int success = 0;
	uint64_t tmp;

	while (success == 0) {
		tmp = v->cnt;
		success = rte_atomic64_cmpset((volatile uint64_t *)&v->cnt,
		                              tmp, tmp - dec);
	}
}

static inline void
rte_atomic64_inc(rte_atomic64_t *v)
{
	rte_atomic64_add(v, 1);
}

static inline void
rte_atomic64_dec(rte_atomic64_t *v)
{
	rte_atomic64_sub(v, 1);
}

static inline int64_t
rte_atomic64_add_return(rte_atomic64_t *v, int64_t inc)
{
	int success = 0;
	uint64_t tmp;

	while (success == 0) {
		tmp = v->cnt;
		success = rte_atomic64_cmpset((volatile uint64_t *)&v->cnt,
		                              tmp, tmp + inc);
	}

	return tmp + inc;
}

static inline int64_t
rte_atomic64_sub_return(rte_atomic64_t *v, int64_t dec)
{
	int success = 0;
	uint64_t tmp;

	while (success == 0) {
		tmp = v->cnt;
		success = rte_atomic64_cmpset((volatile uint64_t *)&v->cnt,
		                              tmp, tmp - dec);
	}

	return tmp - dec;
}

static inline int rte_atomic64_inc_and_test(rte_atomic64_t *v)
{
	return rte_atomic64_add_return(v, 1) == 0;
}

static inline int rte_atomic64_dec_and_test(rte_atomic64_t *v)
{
	return rte_atomic64_sub_return(v, 1) == 0;
}

static inline int rte_atomic64_test_and_set(rte_atomic64_t *v)
{
	return rte_atomic64_cmpset((volatile uint64_t *)&v->cnt, 0, 1);
}

static inline void rte_atomic64_clear(rte_atomic64_t *v)
{
	rte_atomic64_set(v, 0);
}
#endif

#endif /* _RTE_ATOMIC_I686_H_ */
