/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2008-2016 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *	 names of its contributors may be used to endorse or promote products
 *	 derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HEADER_COMPAT_H
#define HEADER_COMPAT_H

#include <sched.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <net/ethernet.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <limits.h>
#include <assert.h>
#include <dirent.h>
#include <inttypes.h>
#include <error.h>
#include <rte_atomic.h>

/* The following definitions are primarily to allow the single-source driver
 * interfaces to be included by arbitrary program code. Ie. for interfaces that
 * are also available in kernel-space, these definitions provide compatibility
 * with certain attributes and types used in those interfaces.
 */

/* Required compiler attributes */
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#define ____cacheline_aligned __attribute__((aligned(L1_CACHE_BYTES)))

#ifdef ARRAY_SIZE
#undef ARRAY_SIZE
#endif
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* Required types */
typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;
typedef uint64_t	dma_addr_t;
typedef cpu_set_t	cpumask_t;
typedef unsigned int	gfp_t;
typedef uint32_t	phandle;

/* I/O operations */
static inline u32 in_be32(volatile void *__p)
{
	volatile u32 *p = __p;
	return *p;
}

static inline void out_be32(volatile void *__p, u32 val)
{
	volatile u32 *p = __p;
	*p = val;
}

/* Debugging */
#define prflush(fmt, args...) \
	do { \
		printf(fmt, ##args); \
		fflush(stdout); \
	} while (0)
#define pr_crit(fmt, args...)	 prflush("CRIT:" fmt, ##args)
#define pr_err(fmt, args...)	 prflush("ERR:" fmt, ##args)
#define pr_warn(fmt, args...)	 prflush("WARN:" fmt, ##args)
#define pr_info(fmt, args...)	 prflush(fmt, ##args)

#ifdef RTE_LIBRTE_DPAA2_DEBUG_BUS
#ifdef pr_debug
#undef pr_debug
#endif
#define pr_debug(fmt, args...) {}
#define WARN_ON(c, str) \
do { \
	static int warned_##__LINE__; \
	if ((c) && !warned_##__LINE__) { \
		pr_warn("%s\n", str); \
		pr_warn("(%s:%d)\n", __FILE__, __LINE__); \
		warned_##__LINE__ = 1; \
	} \
} while (0)
#ifdef CONFIG_BUGON
#define QBMAN_BUG_ON(c) WARN_ON(c, "BUG")
#else
#define QBMAN_BUG_ON(c) {}
#endif

#define ALIGN(x, a) (((x) + ((typeof(x))(a) - 1)) & ~((typeof(x))(a) - 1))

/* Other miscellaneous interfaces our APIs depend on; */
#define lower_32_bits(x) ((u32)(x))
#define upper_32_bits(x) ((u32)(((x) >> 16) >> 16))

/* Compiler/type stuff */

#define __iomem
#define GFP_KERNEL	0
#define __raw_readb(p)	(*(const volatile unsigned char *)(p))
#define __raw_readl(p)	(*(const volatile unsigned int *)(p))
#define __raw_writel(v, p) {*(volatile unsigned int *)(p) = (v); }

/* Allocator stuff */
#define kmalloc(sz, t)	malloc(sz)
#define kfree(p)	{ if (p) free(p); }
static inline void *kzalloc(size_t sz, gfp_t __foo __rte_unused)
{
	void *ptr = malloc(sz);

	if (ptr)
		memset(ptr, 0, sz);
	return ptr;
}

static inline unsigned long get_zeroed_page(gfp_t __foo __rte_unused)
{
	void *p;

	if (posix_memalign(&p, 4096, 4096))
		return 0;
	memset(p, 0, 4096);
	return (unsigned long)p;
}

static inline void free_page(unsigned long p)
{
	free((void *)p);
}

#define atomic_t                rte_atomic32_t
#define atomic_read(v)          rte_atomic32_read(v)
#define atomic_set(v, i)        rte_atomic32_set(v, i)

#define atomic_inc(v)           rte_atomic32_add(v, 1)
#define atomic_dec(v)           rte_atomic32_sub(v, 1)

#define atomic_inc_and_test(v)  rte_atomic32_inc_and_test(v)
#define atomic_dec_and_test(v)  rte_atomic32_dec_and_test(v)

#define atomic_inc_return(v)    rte_atomic32_add_return(v, 1)
#define atomic_dec_return(v)    rte_atomic32_sub_return(v, 1)
#define atomic_sub_and_test(i, v) (rte_atomic32_sub_return(v, i) == 0)

#endif /* HEADER_COMPAT_H */
