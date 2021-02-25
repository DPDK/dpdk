/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2018-2019 Pensando Systems, Inc. All rights reserved.
 */

#ifndef _IONIC_OSDEP_
#define _IONIC_OSDEP_

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_byteorder.h>
#include <rte_io.h>
#include <rte_memory.h>
#include <rte_eal_paging.h>

#include "ionic_logs.h"

#define BIT(nr)            (1UL << (nr))
#define BIT_ULL(nr)        (1ULL << (nr))

#ifndef PAGE_SHIFT
#define PAGE_SHIFT      12
#endif

#define __iomem

typedef uint8_t	 u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint64_t __le64;

#define ioread8(reg)		rte_read8(reg)
#define ioread32(reg)		rte_read32(rte_le_to_cpu_32(reg))
#define iowrite8(value, reg)	rte_write8(value, reg)
#define iowrite32(value, reg)	rte_write32(rte_cpu_to_le_32(value), reg)

#endif
