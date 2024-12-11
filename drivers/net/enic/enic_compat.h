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

#define pr_err(y, ...) dev_err(0, y, ##__VA_ARGS__)
#define pr_warn(y, ...) dev_warning(0, y, ##__VA_ARGS__)
#define BUG() pr_err("BUG at %s:%d", __func__, __LINE__)

#define VNIC_ALIGN(x, a)         __ALIGN_MASK(x, (typeof(x))(a)-1)
#define __ALIGN_MASK(x, mask)    (((x)+(mask))&~(mask))

extern int enic_pmd_logtype;
#define RTE_LOGTYPE_ENIC_PMD enic_pmd_logtype

#define dev_printk(level, fmt, ...)	\
	rte_log(RTE_LOG_ ## level, enic_pmd_logtype, \
		"PMD: rte_enic_pmd: " fmt, ##__VA_ARGS__)

#define dev_err(x, ...) dev_printk(ERR, __VA_ARGS__)
#define dev_info(x, ...) dev_printk(INFO, __VA_ARGS__)
#define dev_warning(x, ...) dev_printk(WARNING, __VA_ARGS__)
#define dev_debug(x, ...) dev_printk(DEBUG, __VA_ARGS__)

#define ENICPMD_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ENIC_PMD, "%s ", __func__, __VA_ARGS__)
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
