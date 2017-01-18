/*
 * Copyright 2008-2014 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * Copyright (c) 2014, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
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

static inline uint16_t ioread16(volatile void *addr)
{
	return rte_read16(addr);
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

static inline void iowrite16(uint16_t val, volatile void *addr)
{
	rte_write16(val, addr);
}

static inline void iowrite8(uint8_t val, volatile void *addr)
{
	rte_write8(val, addr);
}

static inline unsigned int readl(volatile void __iomem *addr)
{
	return rte_read32(addr);
}

static inline unsigned int readl_relaxed(volatile void __iomem *addr)
{
	return rte_read32_relaxed(addr);
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
