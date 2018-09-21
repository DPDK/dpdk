/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2011 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 */

#ifndef __COMPAT_H
#define __COMPAT_H

#include <sched.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <linux/types.h>
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
#include <rte_byteorder.h>
#include <rte_atomic.h>
#include <rte_spinlock.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_cycles.h>

/* The following definitions are primarily to allow the single-source driver
 * interfaces to be included by arbitrary program code. Ie. for interfaces that
 * are also available in kernel-space, these definitions provide compatibility
 * with certain attributes and types used in those interfaces.
 */

/* Required compiler attributes */
#ifndef __maybe_unused
#define __maybe_unused	__rte_unused
#endif
#ifndef __always_unused
#define __always_unused	__rte_unused
#endif
#ifndef __packed
#define __packed	__rte_packed
#endif
#ifndef noinline
#define noinline	__attribute__((noinline))
#endif
#define L1_CACHE_BYTES 64
#define ____cacheline_aligned __attribute__((aligned(L1_CACHE_BYTES)))
#define __stringify_1(x) #x
#define __stringify(x)	__stringify_1(x)

#ifdef ARRAY_SIZE
#undef ARRAY_SIZE
#endif
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* Debugging */
#define prflush(fmt, args...) \
	do { \
		printf(fmt, ##args); \
		fflush(stdout); \
	} while (0)
#ifndef pr_crit
#define pr_crit(fmt, args...)	 prflush("CRIT:" fmt, ##args)
#endif
#ifndef pr_err
#define pr_err(fmt, args...)	 prflush("ERR:" fmt, ##args)
#endif
#ifndef pr_warn
#define pr_warn(fmt, args...)	 prflush("WARN:" fmt, ##args)
#endif
#ifndef pr_info
#define pr_info(fmt, args...)	 prflush(fmt, ##args)
#endif
#ifndef pr_debug
#ifdef RTE_LIBRTE_DPAA_DEBUG_BUS
#define pr_debug(fmt, args...)	printf(fmt, ##args)
#else
#define pr_debug(fmt, args...) {}
#endif
#endif

#define DPAA_BUG_ON(x) RTE_ASSERT(x)

/* Required types */
typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;
typedef uint64_t	dma_addr_t;
typedef cpu_set_t	cpumask_t;
typedef uint32_t	phandle;
typedef uint32_t	gfp_t;
typedef uint32_t	irqreturn_t;

#define IRQ_HANDLED	0
#define request_irq	qbman_request_irq
#define free_irq	qbman_free_irq

#define __iomem
#define GFP_KERNEL	0
#define __raw_readb(p)	(*(const volatile unsigned char *)(p))
#define __raw_readl(p)	(*(const volatile unsigned int *)(p))
#define __raw_writel(v, p) {*(volatile unsigned int *)(p) = (v); }

/* to be used as an upper-limit only */
#define NR_CPUS			64

/* Waitqueue stuff */
typedef struct { }		wait_queue_head_t;
#define DECLARE_WAIT_QUEUE_HEAD(x) int dummy_##x __always_unused
#define wake_up(x)		do { } while (0)

/* I/O operations */
static inline u32 in_be32(volatile void *__p)
{
	volatile u32 *p = __p;
	return rte_be_to_cpu_32(*p);
}

static inline void out_be32(volatile void *__p, u32 val)
{
	volatile u32 *p = __p;
	*p = rte_cpu_to_be_32(val);
}

#define hwsync() rte_rmb()
#define lwsync() rte_wmb()

#define dcbt_ro(p) __builtin_prefetch(p, 0)
#define dcbt_rw(p) __builtin_prefetch(p, 1)

#if defined(RTE_ARCH_ARM64)
#define dcbz(p) { asm volatile("dc zva, %0" : : "r" (p) : "memory"); }
#define dcbz_64(p) dcbz(p)
#define dcbf(p) { asm volatile("dc cvac, %0" : : "r"(p) : "memory"); }
#define dcbf_64(p) dcbf(p)
#define dccivac(p) { asm volatile("dc civac, %0" : : "r"(p) : "memory"); }

#define dcbit_ro(p) \
	do { \
		dccivac(p);						\
		asm volatile("prfm pldl1keep, [%0, #64]" : : "r" (p));	\
	} while (0)

#elif defined(RTE_ARCH_ARM)
#define dcbz(p) memset((p), 0, 32)
#define dcbz_64(p) memset((p), 0, 64)
#define dcbf(p)	RTE_SET_USED(p)
#define dcbf_64(p) dcbf(p)
#define dccivac(p)	RTE_SET_USED(p)
#define dcbit_ro(p)	RTE_SET_USED(p)

#else
#define dcbz(p)	RTE_SET_USED(p)
#define dcbz_64(p) dcbz(p)
#define dcbf(p)	RTE_SET_USED(p)
#define dcbf_64(p) dcbf(p)
#define dccivac(p)	RTE_SET_USED(p)
#define dcbit_ro(p)	RTE_SET_USED(p)
#endif

#define barrier() { asm volatile ("" : : : "memory"); }
#define cpu_relax barrier

#if defined(RTE_ARCH_ARM64)
static inline uint64_t mfatb(void)
{
	uint64_t ret, ret_new, timeout = 200;

	asm volatile ("mrs %0, cntvct_el0" : "=r" (ret));
	asm volatile ("mrs %0, cntvct_el0" : "=r" (ret_new));
	while (ret != ret_new && timeout--) {
		ret = ret_new;
		asm volatile ("mrs %0, cntvct_el0" : "=r" (ret_new));
	}
	DPAA_BUG_ON(!timeout && (ret != ret_new));
	return ret * 64;
}
#else

#define mfatb rte_rdtsc

#endif

/* Spin for a few cycles without bothering the bus */
static inline void cpu_spin(int cycles)
{
	uint64_t now = mfatb();

	while (mfatb() < (now + cycles))
		;
}

/* Qman/Bman API inlines and macros; */
#ifdef lower_32_bits
#undef lower_32_bits
#endif
#define lower_32_bits(x) ((u32)(x))

#ifdef upper_32_bits
#undef upper_32_bits
#endif
#define upper_32_bits(x) ((u32)(((x) >> 16) >> 16))

/*
 * Swap bytes of a 48-bit value.
 */
static inline uint64_t
__bswap_48(uint64_t x)
{
	return  ((x & 0x0000000000ffULL) << 40) |
		((x & 0x00000000ff00ULL) << 24) |
		((x & 0x000000ff0000ULL) <<  8) |
		((x & 0x0000ff000000ULL) >>  8) |
		((x & 0x00ff00000000ULL) >> 24) |
		((x & 0xff0000000000ULL) >> 40);
}

/*
 * Swap bytes of a 40-bit value.
 */
static inline uint64_t
__bswap_40(uint64_t x)
{
	return  ((x & 0x00000000ffULL) << 32) |
		((x & 0x000000ff00ULL) << 16) |
		((x & 0x0000ff0000ULL)) |
		((x & 0x00ff000000ULL) >> 16) |
		((x & 0xff00000000ULL) >> 32);
}

/*
 * Swap bytes of a 24-bit value.
 */
static inline uint32_t
__bswap_24(uint32_t x)
{
	return  ((x & 0x0000ffULL) << 16) |
		((x & 0x00ff00ULL)) |
		((x & 0xff0000ULL) >> 16);
}

#define be64_to_cpu(x) rte_be_to_cpu_64(x)
#define be32_to_cpu(x) rte_be_to_cpu_32(x)
#define be16_to_cpu(x) rte_be_to_cpu_16(x)

#define cpu_to_be64(x) rte_cpu_to_be_64(x)
#if !defined(cpu_to_be32)
#define cpu_to_be32(x) rte_cpu_to_be_32(x)
#endif
#define cpu_to_be16(x) rte_cpu_to_be_16(x)

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN

#define cpu_to_be48(x) __bswap_48(x)
#define be48_to_cpu(x) __bswap_48(x)

#define cpu_to_be40(x) __bswap_40(x)
#define be40_to_cpu(x) __bswap_40(x)

#define cpu_to_be24(x) __bswap_24(x)
#define be24_to_cpu(x) __bswap_24(x)

#else /* RTE_BIG_ENDIAN */

#define cpu_to_be48(x) (x)
#define be48_to_cpu(x) (x)

#define cpu_to_be40(x) (x)
#define be40_to_cpu(x) (x)

#define cpu_to_be24(x) (x)
#define be24_to_cpu(x) (x)

#endif /* RTE_BIG_ENDIAN */

/* When copying aligned words or shorts, try to avoid memcpy() */
/* memcpy() stuff - when you know alignments in advance */
#define CONFIG_TRY_BETTER_MEMCPY

#ifdef CONFIG_TRY_BETTER_MEMCPY
static inline void copy_words(void *dest, const void *src, size_t sz)
{
	u32 *__dest = dest;
	const u32 *__src = src;
	size_t __sz = sz >> 2;

	DPAA_BUG_ON((unsigned long)dest & 0x3);
	DPAA_BUG_ON((unsigned long)src & 0x3);
	DPAA_BUG_ON(sz & 0x3);
	while (__sz--)
		*(__dest++) = *(__src++);
}

static inline void copy_shorts(void *dest, const void *src, size_t sz)
{
	u16 *__dest = dest;
	const u16 *__src = src;
	size_t __sz = sz >> 1;

	DPAA_BUG_ON((unsigned long)dest & 0x1);
	DPAA_BUG_ON((unsigned long)src & 0x1);
	DPAA_BUG_ON(sz & 0x1);
	while (__sz--)
		*(__dest++) = *(__src++);
}

static inline void copy_bytes(void *dest, const void *src, size_t sz)
{
	u8 *__dest = dest;
	const u8 *__src = src;

	while (sz--)
		*(__dest++) = *(__src++);
}
#else
#define copy_words memcpy
#define copy_shorts memcpy
#define copy_bytes memcpy
#endif

/* Allocator stuff */
#define kmalloc(sz, t)	malloc(sz)
#define vmalloc(sz)	malloc(sz)
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

/* Spinlock stuff */
#define spinlock_t		rte_spinlock_t
#define __SPIN_LOCK_UNLOCKED(x)	RTE_SPINLOCK_INITIALIZER
#define DEFINE_SPINLOCK(x)	spinlock_t x = __SPIN_LOCK_UNLOCKED(x)
#define spin_lock_init(x)	rte_spinlock_init(x)
#define spin_lock_destroy(x)
#define spin_lock(x)		rte_spinlock_lock(x)
#define spin_unlock(x)		rte_spinlock_unlock(x)
#define spin_lock_irq(x)	spin_lock(x)
#define spin_unlock_irq(x)	spin_unlock(x)
#define spin_lock_irqsave(x, f) spin_lock_irq(x)
#define spin_unlock_irqrestore(x, f) spin_unlock_irq(x)

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

#include <dpaa_list.h>
#include <dpaa_bits.h>

#endif /* __COMPAT_H */
