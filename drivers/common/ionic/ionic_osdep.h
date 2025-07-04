/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2024 Advanced Micro Devices, Inc.
 */

#ifndef _IONIC_OSDEP_
#define _IONIC_OSDEP_

#include <stdint.h>
#include <stdarg.h>

#include <rte_common.h>
#include <rte_io.h>
#include <rte_byteorder.h>

#define BIT(nr)		(1UL << (nr))
#define BIT_ULL(nr)	(1ULL << (nr))

#define __iomem

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#ifndef __le16
#define __le16 uint16_t
#endif
#ifndef __le32
#define __le32 uint32_t
#endif
#ifndef __le64
#define __le64 uint64_t
#endif

#define ioread8(reg)		rte_read8(reg)
#define ioread32(reg)		rte_read32(rte_le_to_cpu_32(reg))
#define iowrite8(value, reg)	rte_write8(value, reg)
#define iowrite32(value, reg)	rte_write32(rte_cpu_to_le_32(value), reg)

#endif /* _IONIC_OSDEP_ */
