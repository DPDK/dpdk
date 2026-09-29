/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2008-2016 Freescale Semiconductor, Inc.
 * Copyright 2017,2021 NXP
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
#include <rte_branch_prediction.h>

/* The following definitions are primarily to allow the single-source driver
 * interfaces to be included by arbitrary program code. Ie. for interfaces that
 * are also available in kernel-space, these definitions provide compatibility
 * with certain attributes and types used in those interfaces.
 */

/* Required types */
typedef uint64_t	dma_addr_t;

/* Debugging */
#define prflush(fmt, ...) \
	do { \
		printf(fmt, ##__VA_ARGS__); \
		fflush(stdout); \
	} while (0)
#define pr_crit(fmt, ...)	 prflush("CRIT:" fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...)	 prflush("ERR:" fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)	 prflush("WARN:" fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)	 prflush(fmt, ##__VA_ARGS__)

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
#define pr_debug(fmt, ...)	printf(fmt, ##__VA_ARGS__)
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
#define pr_debug(fmt, ...) {}
#endif

/* Other miscellaneous interfaces our APIs depend on; */

#define lower_32_bits(x) ((uint32_t)(x))
#define upper_32_bits(x) ((uint32_t)(((x) >> 16) >> 16))

#define __iomem

#define __raw_readb(p)	(*(const volatile unsigned char *)(p))
#define __raw_readl(p)	(*(const volatile unsigned int *)(p))
#define __raw_writel(v, p) {*(volatile unsigned int *)(p) = (v); }

#define dma_wmb()		rte_io_wmb()

typedef RTE_ATOMIC(uint32_t) atomic_t;

#define atomic_set(v, i)        rte_atomic_store_explicit((v), (i), rte_memory_order_relaxed)
#define atomic_inc(v)           ((void)rte_atomic_fetch_add_explicit((v), 1, \
								     rte_memory_order_seq_cst))
#define atomic_dec_and_test(v)  (rte_atomic_fetch_sub_explicit((v), 1, \
							       rte_memory_order_seq_cst) == 1)

#endif /* HEADER_COMPAT_H */
