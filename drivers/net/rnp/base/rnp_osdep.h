/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_OSDEP_H
#define _RNP_OSDEP_H

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include <rte_io.h>
#include <rte_log.h>
#include <rte_cycles.h>

#include "../rnp_logs.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

#define mb()	rte_mb()
#define wmb()	rte_wmb()

#define udelay(x) rte_delay_us(x)

#define _MSI_(off)	((off) + (0xA0000))

#define __iomem
static inline u32
rnp_reg_read32(const void *base, size_t offset)
{
	u32 v = rte_read32(((const u8 *)base + offset));

	RNP_PMD_REG_LOG(DEBUG, "offset=%p val=%#"PRIx32"", offset, v);
	return v;
}

static inline void
rnp_reg_write32(volatile void *base, size_t offset, u32 val)
{
	RNP_PMD_REG_LOG(DEBUG, "offset=%p val=%#"PRIx32"", offset, val);
	rte_write32(val, ((volatile u8 *)base + offset));
}

#define RNP_REG_RD(base, offset)	rnp_reg_read32(base, offset)
#define RNP_REG_WR(base, offset, value)	rnp_reg_write32(base, offset, value)
#define RNP_E_REG_WR(hw, off, value)	rnp_reg_write32((hw)->e_ctrl, (off), (value))
#define RNP_E_REG_RD(hw, off)		rnp_reg_read32((hw)->e_ctrl, (off))
#define RNP_C_REG_WR(hw, off, value)	\
	rnp_reg_write32((hw)->c_ctrl, ((off) & ((hw)->c_blen - 1)), (value))
#define RNP_C_REG_RD(hw, off)		\
	rnp_reg_read32((hw)->c_ctrl, ((off) & ((hw)->c_blen - 1)))

#endif /* _RNP_OSDEP_H_ */
