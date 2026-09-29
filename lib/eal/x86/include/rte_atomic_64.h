/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
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

/*------------------------- 64 bit atomic operations -------------------------*/


/*------------------------ 128 bit atomic operations -------------------------*/

static inline int
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
