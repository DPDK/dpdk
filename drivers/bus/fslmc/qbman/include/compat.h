/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2008-2016 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP
 *
 */

#ifndef HEADER_COMPAT_H
#define HEADER_COMPAT_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <linux/types.h>
#include <rte_atomic.h>

/* The following definitions are primarily to allow the single-source driver
 * interfaces to be included by arbitrary program code. Ie. for interfaces that
 * are also available in kernel-space, these definitions provide compatibility
 * with certain attributes and types used in those interfaces.
 */

/* Required compiler attributes */
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

/* Required types */
typedef uint64_t	dma_addr_t;

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

/* Trace the 3 different classes of read/write access to QBMan. #undef as
 * required.
 */
#define QBMAN_CCSR_TRACE
#define QBMAN_CINH_TRACE
#define QBMAN_CENA_TRACE

#define QBMAN_CHECKING

#ifdef pr_debug
#undef pr_debug
#endif
#define pr_debug(fmt, args...)	printf(fmt, ##args)
#define QBMAN_BUG_ON(c) \
do { \
	static int warned_##__LINE__; \
	if ((c) && !warned_##__LINE__) { \
		pr_warn("(%s:%d)\n", __FILE__, __LINE__); \
		warned_##__LINE__ = 1; \
	} \
} while (0)
#else
#define QBMAN_BUG_ON(c) {}
#define pr_debug(fmt, args...) {}
#endif

/* Other miscellaneous interfaces our APIs depend on; */

#define lower_32_bits(x) ((uint32_t)(x))
#define upper_32_bits(x) ((uint32_t)(((x) >> 16) >> 16))

#define __iomem

#define __raw_readb(p)	(*(const volatile unsigned char *)(p))
#define __raw_readl(p)	(*(const volatile unsigned int *)(p))
#define __raw_writel(v, p) {*(volatile unsigned int *)(p) = (v); }

#define dma_wmb()		rte_smp_mb()

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
