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

#define ETH_ALEN        6

#define __iomem

#define pr_err(y, args...) dev_err(0, y, ##args)
#define pr_warn(y, args...) dev_warning(0, y, ##args)
#define BUG() pr_err("BUG at %s:%d", __func__, __LINE__)

#define VNIC_ALIGN(x, a)         __ALIGN_MASK(x, (typeof(x))(a)-1)
#define __ALIGN_MASK(x, mask)    (((x)+(mask))&~(mask))

extern int enic_pmd_logtype;

#define dev_printk(level, fmt, args...)	\
	rte_log(RTE_LOG_ ## level, enic_pmd_logtype, \
		"PMD: rte_enic_pmd: " fmt, ##args)

#define dev_err(x, args...) dev_printk(ERR, args)
#define dev_info(x, args...) dev_printk(INFO,  args)
#define dev_warning(x, args...) dev_printk(WARNING, args)
#define dev_debug(x, args...) dev_printk(DEBUG, args)

#define ENICPMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, enic_pmd_logtype, \
		"%s " fmt "\n", __func__, ##args)
#define ENICPMD_FUNC_TRACE() ENICPMD_LOG(DEBUG, ">>")

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

#endif /* _ENIC_COMPAT_H_ */
