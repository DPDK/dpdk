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
#include <stdbool.h>

#include <rte_common.h>
#include <rte_memcpy.h>
#include <rte_byteorder.h>
#include <rte_cycles.h>
#include <rte_spinlock.h>
#include <rte_log.h>
#include <rte_io.h>
#include <rte_net.h>

extern int cxgbe_logtype;
extern int cxgbe_mbox_logtype;

#define dev_printf(level, logtype, fmt, ...) \
	rte_log(RTE_LOG_ ## level, logtype, \
		"rte_cxgbe_pmd: " fmt, ##__VA_ARGS__)

#define dev_err(x, fmt, ...) \
	dev_printf(ERR, cxgbe_logtype, fmt, ##__VA_ARGS__)
#define dev_info(x, fmt, ...) \
	dev_printf(INFO, cxgbe_logtype, fmt, ##__VA_ARGS__)
#define dev_warn(x, fmt, ...) \
	dev_printf(WARNING, cxgbe_logtype, fmt, ##__VA_ARGS__)
#define dev_debug(x, fmt, ...) \
	dev_printf(DEBUG, cxgbe_logtype, fmt, ##__VA_ARGS__)

#define CXGBE_DEBUG_MBOX(x, fmt, ...) \
	dev_printf(DEBUG, cxgbe_mbox_logtype, "MBOX:" fmt, ##__VA_ARGS__)

#define CXGBE_FUNC_TRACE() \
	dev_printf(DEBUG, cxgbe_logtype, "CXGBE trace: %s\n", __func__)

#define pr_err(fmt, ...) dev_err(0, fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...) dev_warn(0, fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...) dev_info(0, fmt, ##__VA_ARGS__)
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

#define ETHER_ADDR_LEN 6

#define rmb()     rte_rmb() /* dpdk rte provided rmb */
#define wmb()     rte_wmb() /* dpdk rte provided wmb */

typedef uint8_t   u8;
typedef int8_t    s8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef int32_t   s32;
typedef uint64_t  u64;
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

#ifndef min
#define min(a, b) RTE_MIN(a, b)
#endif

#ifndef max
#define max(a, b) RTE_MAX(a, b)
#endif

/*
 * round up val _p to a power of 2 size _s
 */
#define cxgbe_roundup(_p, _s) (((unsigned long)(_p) + (_s - 1)) & ~(_s - 1))

#ifndef container_of
#define container_of(ptr, type, member) ({ \
		typeof(((type *)0)->member)(*__mptr) = (ptr); \
		(type *)((char *)__mptr - offsetof(type, member)); })
#endif

#define ARRAY_SIZE(arr) RTE_DIM(arr)

#define cpu_to_be16(o) rte_cpu_to_be_16(o)
#define cpu_to_be32(o) rte_cpu_to_be_32(o)
#define cpu_to_be64(o) rte_cpu_to_be_64(o)
#define cpu_to_le32(o) rte_cpu_to_le_32(o)
#define be16_to_cpu(o) rte_be_to_cpu_16(o)
#define be32_to_cpu(o) rte_be_to_cpu_32(o)
#define be64_to_cpu(o) rte_be_to_cpu_64(o)
#define le32_to_cpu(o) rte_le_to_cpu_32(o)

#ifndef ntohs
#define ntohs(o) be16_to_cpu(o)
#endif

#ifndef ntohl
#define ntohl(o) be32_to_cpu(o)
#endif

#ifndef htons
#define htons(o) cpu_to_be16(o)
#endif

#ifndef htonl
#define htonl(o) cpu_to_be32(o)
#endif

#ifndef caddr_t
typedef char *caddr_t;
#endif

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
	return x ? sizeof(x) * 8 - rte_clz32(x) : 0;
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

/*
 * Multiplies an integer by a fraction, while avoiding unnecessary
 * overflow or loss of precision.
 */
static inline unsigned int mult_frac(unsigned int x, unsigned int numer,
				     unsigned int denom)
{
	unsigned int quot = x / denom;
	unsigned int rem = x % denom;

	return (quot * numer) + ((rem * numer) / denom);
}
#endif /* _CXGBE_COMPAT_H_ */
