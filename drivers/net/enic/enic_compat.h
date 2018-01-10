/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _ENIC_COMPAT_H_
#define _ENIC_COMPAT_H_

#include <stdio.h>
#include <unistd.h>

#include <rte_atomic.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_io.h>

#define ENIC_PAGE_ALIGN 4096UL
#define ENIC_ALIGN      ENIC_PAGE_ALIGN
#define NAME_MAX        255
#define ETH_ALEN        6

#define __iomem

#define rmb()     rte_rmb() /* dpdk rte provided rmb */
#define wmb()     rte_wmb() /* dpdk rte provided wmb */

#define le16_to_cpu
#define le32_to_cpu
#define le64_to_cpu
#define cpu_to_le16
#define cpu_to_le32
#define cpu_to_le64

#ifndef offsetof
#define offsetof(t, m) ((size_t) &((t *)0)->m)
#endif

#define pr_err(y, args...) dev_err(0, y, ##args)
#define pr_warn(y, args...) dev_warning(0, y, ##args)
#define BUG() pr_err("BUG at %s:%d", __func__, __LINE__)

#define VNIC_ALIGN(x, a)         __ALIGN_MASK(x, (typeof(x))(a)-1)
#define __ALIGN_MASK(x, mask)    (((x)+(mask))&~(mask))
#define udelay usleep
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

#define kzalloc(size, flags) calloc(1, size)
#define kfree(x) free(x)

#define dev_printk(level, fmt, args...)	\
	RTE_LOG(level, PMD, "rte_enic_pmd: " fmt, ## args)

#define dev_err(x, args...) dev_printk(ERR, args)
#define dev_info(x, args...) dev_printk(INFO,  args)
#define dev_warning(x, args...) dev_printk(WARNING, args)
#define dev_debug(x, args...) dev_printk(DEBUG, args)

extern int enicpmd_logtype_flow;

#define __le16 u16
#define __le32 u32
#define __le64 u64

typedef		unsigned char       u8;
typedef		unsigned short      u16;
typedef		unsigned int        u32;
typedef         unsigned long long  u64;
typedef         unsigned long long  dma_addr_t;

static inline uint32_t ioread32(volatile void *addr)
{
	return rte_read32(addr);
}

static inline uint8_t ioread8(volatile void *addr)
{
	return rte_read8(addr);
}

static inline void iowrite32(uint32_t val, volatile void *addr)
{
	rte_write32(val, addr);
}

static inline void iowrite32_relaxed(uint32_t val, volatile void *addr)
{
	rte_write32_relaxed(val, addr);
}

static inline unsigned int readl(volatile void __iomem *addr)
{
	return rte_read32(addr);
}

static inline void writel(unsigned int val, volatile void __iomem *addr)
{
	rte_write32(val, addr);
}

#define min_t(type, x, y) ({                    \
	type __min1 = (x);                      \
	type __min2 = (y);                      \
	__min1 < __min2 ? __min1 : __min2; })

#define max_t(type, x, y) ({                    \
	type __max1 = (x);                      \
	type __max2 = (y);                      \
	__max1 > __max2 ? __max1 : __max2; })

#endif /* _ENIC_COMPAT_H_ */
