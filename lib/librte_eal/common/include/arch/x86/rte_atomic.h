/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_ATOMIC_X86_H_
#define _RTE_ATOMIC_X86_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <rte_common.h>
#include <rte_config.h>
#include <emmintrin.h>
#include "generic/rte_atomic.h"

#if RTE_MAX_LCORE == 1
#define MPLOCKED                        /**< No need to insert MP lock prefix. */
#else
#define MPLOCKED        "lock ; "       /**< Insert MP lock prefix. */
#endif

#define	rte_mb() _mm_mfence()

#define	rte_wmb() _mm_sfence()

#define	rte_rmb() _mm_lfence()

#define rte_smp_mb() rte_mb()

#define rte_smp_wmb() rte_compiler_barrier()

#define rte_smp_rmb() rte_compiler_barrier()

#define rte_io_mb() rte_mb()

#define rte_io_wmb() rte_compiler_barrier()

#define rte_io_rmb() rte_compiler_barrier()

/*------------------------- 16 bit atomic operations -------------------------*/

#ifndef RTE_FORCE_INTRINSICS
static inline int
rte_atomic16_cmpset(volatile uint16_t *dst, uint16_t exp, uint16_t src)
{
	uint8_t res;

	asm volatile(
			MPLOCKED
			"cmpxchgw %[src], %[dst];"
			"sete %[res];"
			: [res] "=a" (res),     /* output */
			  [dst] "=m" (*dst)
			: [src] "r" (src),      /* input */
			  "a" (exp),
			  "m" (*dst)
			: "memory");            /* no-clobber list */
	return res;
}

static inline int rte_atomic16_test_and_set(rte_atomic16_t *v)
{
	return rte_atomic16_cmpset((volatile uint16_t *)&v->cnt, 0, 1);
}

static inline void
rte_atomic16_inc(rte_atomic16_t *v)
{
	asm volatile(
			MPLOCKED
			"incw %[cnt]"
			: [cnt] "=m" (v->cnt)   /* output */
			: "m" (v->cnt)          /* input */
			);
}

static inline void
rte_atomic16_dec(rte_atomic16_t *v)
{
	asm volatile(
			MPLOCKED
			"decw %[cnt]"
			: [cnt] "=m" (v->cnt)   /* output */
			: "m" (v->cnt)          /* input */
			);
}

static inline int rte_atomic16_inc_and_test(rte_atomic16_t *v)
{
	uint8_t ret;

	asm volatile(
			MPLOCKED
			"incw %[cnt] ; "
			"sete %[ret]"
			: [cnt] "+m" (v->cnt),  /* output */
			  [ret] "=qm" (ret)
			);
	return ret != 0;
}

static inline int rte_atomic16_dec_and_test(rte_atomic16_t *v)
{
	uint8_t ret;

	asm volatile(MPLOCKED
			"decw %[cnt] ; "
			"sete %[ret]"
			: [cnt] "+m" (v->cnt),  /* output */
			  [ret] "=qm" (ret)
			);
	return ret != 0;
}

/*------------------------- 32 bit atomic operations -------------------------*/

static inline int
rte_atomic32_cmpset(volatile uint32_t *dst, uint32_t exp, uint32_t src)
{
	uint8_t res;

	asm volatile(
			MPLOCKED
			"cmpxchgl %[src], %[dst];"
			"sete %[res];"
			: [res] "=a" (res),     /* output */
			  [dst] "=m" (*dst)
			: [src] "r" (src),      /* input */
			  "a" (exp),
			  "m" (*dst)
			: "memory");            /* no-clobber list */
	return res;
}

static inline int rte_atomic32_test_and_set(rte_atomic32_t *v)
{
	return rte_atomic32_cmpset((volatile uint32_t *)&v->cnt, 0, 1);
}

static inline void
rte_atomic32_inc(rte_atomic32_t *v)
{
	asm volatile(
			MPLOCKED
			"incl %[cnt]"
			: [cnt] "=m" (v->cnt)   /* output */
			: "m" (v->cnt)          /* input */
			);
}

static inline void
rte_atomic32_dec(rte_atomic32_t *v)
{
	asm volatile(
			MPLOCKED
			"decl %[cnt]"
			: [cnt] "=m" (v->cnt)   /* output */
			: "m" (v->cnt)          /* input */
			);
}

static inline int rte_atomic32_inc_and_test(rte_atomic32_t *v)
{
	uint8_t ret;

	asm volatile(
			MPLOCKED
			"incl %[cnt] ; "
			"sete %[ret]"
			: [cnt] "+m" (v->cnt),  /* output */
			  [ret] "=qm" (ret)
			);
	return ret != 0;
}

static inline int rte_atomic32_dec_and_test(rte_atomic32_t *v)
{
	uint8_t ret;

	asm volatile(MPLOCKED
			"decl %[cnt] ; "
			"sete %[ret]"
			: [cnt] "+m" (v->cnt),  /* output */
			  [ret] "=qm" (ret)
			);
	return ret != 0;
}
#endif

#ifdef RTE_ARCH_I686
#include "rte_atomic_32.h"
#else
#include "rte_atomic_64.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ATOMIC_X86_H_ */
