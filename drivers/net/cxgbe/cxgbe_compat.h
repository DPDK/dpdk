/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef _CXGBE_COMPAT_H_
#define _CXGBE_COMPAT_H_

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#include <rte_common.h>
#include <rte_memcpy.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_spinlock.h>
#include <rte_log.h>
#include <rte_io.h>

#define dev_printf(level, fmt, args...) \
	RTE_LOG(level, PMD, "rte_cxgbe_pmd: " fmt, ## args)

#define dev_err(x, args...) dev_printf(ERR, args)
#define dev_info(x, args...) dev_printf(INFO, args)
#define dev_warn(x, args...) dev_printf(WARNING, args)

#ifdef RTE_LIBRTE_CXGBE_DEBUG
#define dev_debug(x, args...) dev_printf(DEBUG, args)
#else
#define dev_debug(x, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_CXGBE_DEBUG_REG
#define CXGBE_DEBUG_REG(x, args...) dev_printf(DEBUG, "REG:" args)
#else
#define CXGBE_DEBUG_REG(x, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_CXGBE_DEBUG_MBOX
#define CXGBE_DEBUG_MBOX(x, args...) dev_printf(DEBUG, "MBOX:" args)
#else
#define CXGBE_DEBUG_MBOX(x, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_CXGBE_DEBUG_TX
#define CXGBE_DEBUG_TX(x, args...) dev_printf(DEBUG, "TX:" args)
#else
#define CXGBE_DEBUG_TX(x, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_CXGBE_DEBUG_RX
#define CXGBE_DEBUG_RX(x, args...) dev_printf(DEBUG, "RX:" args)
#else
#define CXGBE_DEBUG_RX(x, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_CXGBE_DEBUG
#define CXGBE_FUNC_TRACE() \
	RTE_LOG(DEBUG, PMD, "CXGBE trace: %s\n", __func__)
#else
#define CXGBE_FUNC_TRACE() do { } while (0)
#endif

#define pr_err(y, args...) dev_err(0, y, ##args)
#define pr_warn(y, args...) dev_warn(0, y, ##args)
#define pr_info(y, args...) dev_info(0, y, ##args)
#define BUG() pr_err("BUG at %s:%d", __func__, __LINE__)

#define ASSERT(x) do {\
	if (!(x)) \
		rte_panic("CXGBE: x"); \
} while (0)
#define BUG_ON(x) ASSERT(!(x))

#ifndef WARN_ON
#define WARN_ON(x) do { \
	int ret = !!(x); \
	if (unlikely(ret)) \
		pr_warn("WARN_ON: \"" #x "\" at %s:%d\n", __func__, __LINE__); \
} while (0)
#endif

#define __iomem

#ifndef BIT
#define BIT(n) (1 << (n))
#endif

#define L1_CACHE_SHIFT  6
#define L1_CACHE_BYTES  BIT(L1_CACHE_SHIFT)

#define PAGE_SHIFT  12
#define CXGBE_ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define PTR_ALIGN(p, a) ((typeof(p))CXGBE_ALIGN((unsigned long)(p), (a)))

#define VLAN_HLEN 4

#define rmb()     rte_rmb() /* dpdk rte provided rmb */
#define wmb()     rte_wmb() /* dpdk rte provided wmb */

typedef uint8_t   u8;
typedef int8_t    s8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef int32_t   s32;
typedef uint64_t  u64;
typedef int       bool;
typedef uint64_t  dma_addr_t;

#ifndef __le16
#define __le16	uint16_t
#endif
#ifndef __le32
#define __le32	uint32_t
#endif
#ifndef __le64
#define __le64	uint64_t
#endif
#ifndef __be16
#define __be16	uint16_t
#endif
#ifndef __be32
#define __be32	uint32_t
#endif
#ifndef __be64
#define __be64	uint64_t
#endif
#ifndef __u8
#define __u8	uint8_t
#endif
#ifndef __u16
#define __u16	uint16_t
#endif
#ifndef __u32
#define __u32	uint32_t
#endif
#ifndef __u64
#define __u64	uint64_t
#endif

#define FALSE	0
#define TRUE	1
#define false	0
#define true	1

#define min(a, b) RTE_MIN(a, b)
#define max(a, b) RTE_MAX(a, b)

/*
 * round up val _p to a power of 2 size _s
 */
#define cxgbe_roundup(_p, _s) (((unsigned long)(_p) + (_s - 1)) & ~(_s - 1))

#undef container_of
#define container_of(ptr, type, member) ({ \
		typeof(((type *)0)->member)(*__mptr) = (ptr); \
		(type *)((char *)__mptr - offsetof(type, member)); })

#define ARRAY_SIZE(arr) RTE_DIM(arr)

#define cpu_to_be16(o) rte_cpu_to_be_16(o)
#define cpu_to_be32(o) rte_cpu_to_be_32(o)
#define cpu_to_be64(o) rte_cpu_to_be_64(o)
#define cpu_to_le32(o) rte_cpu_to_le_32(o)
#define be16_to_cpu(o) rte_be_to_cpu_16(o)
#define be32_to_cpu(o) rte_be_to_cpu_32(o)
#define be64_to_cpu(o) rte_be_to_cpu_64(o)
#define le32_to_cpu(o) rte_le_to_cpu_32(o)

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define DELAY(x) rte_delay_us(x)
#define udelay(x) DELAY(x)
#define msleep(x) DELAY(1000 * (x))
#define usleep_range(min, max) msleep(DIV_ROUND_UP(min, 1000))

static inline uint8_t hweight32(uint32_t word32)
{
	uint32_t res = word32 - ((word32 >> 1) & 0x55555555);

	res = (res & 0x33333333) + ((res >> 2) & 0x33333333);
	res = (res + (res >> 4)) & 0x0F0F0F0F;
	res = res + (res >> 8);
	return (res + (res >> 16)) & 0x000000FF;

} /* weight32 */

/**
 * cxgbe_fls - find last (most-significant) bit set
 * @x: the word to search
 *
 * This is defined the same way as ffs.
 * Note cxgbe_fls(0) = 0, cxgbe_fls(1) = 1, cxgbe_fls(0x80000000) = 32.
 */
static inline int cxgbe_fls(int x)
{
	return x ? sizeof(x) * 8 - __builtin_clz(x) : 0;
}

/**
 * cxgbe_ffs - find first bit set
 * @x: the word to search
 */
static inline int cxgbe_ffs(int x)
{
	return x ? __builtin_ffs(x) : 0;
}

static inline unsigned long ilog2(unsigned long n)
{
	unsigned int e = 0;

	while (n) {
		if (n & ~((1 << 8) - 1)) {
			e += 8;
			n >>= 8;
			continue;
		}

		if (n & ~((1 << 4) - 1)) {
			e += 4;
			n >>= 4;
		}

		for (;;) {
			n >>= 1;
			if (n == 0)
				break;
			e++;
		}
	}

	return e;
}

static inline void writel(unsigned int val, volatile void __iomem *addr)
{
	rte_write32(val, addr);
}

static inline void writeq(u64 val, volatile void __iomem *addr)
{
	writel(val, addr);
	writel(val >> 32, (void *)((uintptr_t)addr + 4));
}

static inline void writel_relaxed(unsigned int val, volatile void __iomem *addr)
{
	rte_write32_relaxed(val, addr);
}

#endif /* _CXGBE_COMPAT_H_ */
