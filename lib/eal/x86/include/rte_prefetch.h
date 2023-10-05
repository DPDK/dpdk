/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _RTE_PREFETCH_X86_64_H_
#define _RTE_PREFETCH_X86_64_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RTE_TOOLCHAIN_MSVC
#include <emmintrin.h>
#endif

#include <rte_compat.h>
#include <rte_common.h>
#include "generic/rte_prefetch.h"

static inline void rte_prefetch0(const volatile void *p)
{
#ifdef RTE_TOOLCHAIN_MSVC
	_mm_prefetch((const void *)p, _MM_HINT_T0);
#else
	asm volatile ("prefetcht0 %[p]" : : [p] "m" (*(const volatile char *)p));
#endif
}

static inline void rte_prefetch1(const volatile void *p)
{
#ifdef RTE_TOOLCHAIN_MSVC
	_mm_prefetch((const void *)p, _MM_HINT_T1);
#else
	asm volatile ("prefetcht1 %[p]" : : [p] "m" (*(const volatile char *)p));
#endif
}

static inline void rte_prefetch2(const volatile void *p)
{
#ifdef RTE_TOOLCHAIN_MSVC
	_mm_prefetch((const void *)p, _MM_HINT_T2);
#else
	asm volatile ("prefetcht2 %[p]" : : [p] "m" (*(const volatile char *)p));
#endif
}

static inline void rte_prefetch_non_temporal(const volatile void *p)
{
#ifdef RTE_TOOLCHAIN_MSVC
	_mm_prefetch((const void *)p, _MM_HINT_NTA);
#else
	asm volatile ("prefetchnta %[p]" : : [p] "m" (*(const volatile char *)p));
#endif
}

__rte_experimental
static inline void
rte_cldemote(const volatile void *p)
{
#ifdef RTE_TOOLCHAIN_MSVC
	_mm_cldemote(p);
#else
	/*
	 * We use raw byte codes for now as only the newest compiler
	 * versions support this instruction natively.
	 */
	asm volatile(".byte 0x0f, 0x1c, 0x06" :: "S" (p));
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PREFETCH_X86_64_H_ */
