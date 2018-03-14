/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2014-2016 Freescale Semiconductor, Inc.
 *
 */
#include <compat.h>
#include <fsl_qbman_base.h>

/* Sanity check */
#if (__BYTE_ORDER__ != __ORDER_BIG_ENDIAN__) && \
	(__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error "Unknown endianness!"
#endif

	/****************/
	/* arch assists */
	/****************/
#if defined(RTE_ARCH_ARM64)
#define dcbz(p) { asm volatile("dc zva, %0" : : "r" (p) : "memory"); }
#define lwsync() { asm volatile("dmb st" : : : "memory"); }
#define dcbf(p) { asm volatile("dc cvac, %0" : : "r"(p) : "memory"); }
#define dccivac(p) { asm volatile("dc civac, %0" : : "r"(p) : "memory"); }
static inline void prefetch_for_load(void *p)
{
	asm volatile("prfm pldl1keep, [%0, #0]" : : "r" (p));
}

static inline void prefetch_for_store(void *p)
{
	asm volatile("prfm pstl1keep, [%0, #0]" : : "r" (p));
}
#elif defined(RTE_ARCH_ARM)
#define dcbz(p) memset(p, 0, 64)
#define lwsync() { asm volatile("dmb st" : : : "memory"); }
#define dcbf(p)	RTE_SET_USED(p)
#define dccivac(p)	RTE_SET_USED(p)
#define prefetch_for_load(p) { asm volatile ("pld [%0]" : : "r" (p)); }
#define prefetch_for_store(p) { asm volatile ("pld [%0]" : : "r" (p)); }

#else
#define dcbz(p)	RTE_SET_USED(p)
#define lwsync()
#define dcbf(p)	RTE_SET_USED(p)
#define dccivac(p)	RTE_SET_USED(p)
static inline void prefetch_for_load(void *p)
{
	RTE_SET_USED(p);
}
static inline void prefetch_for_store(void *p)
{
	RTE_SET_USED(p);
}
#endif
