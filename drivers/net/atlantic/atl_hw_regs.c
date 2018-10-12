// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/* Copyright (C) 2014-2017 aQuantia Corporation. */

/* File aq_hw_utils.c: Definitions of helper functions used across
 * hardware layer.
 */

#include "atl_hw_regs.h"

#include <rte_io.h>
#include <rte_byteorder.h>

void aq_hw_write_reg_bit(struct aq_hw_s *aq_hw, u32 addr, u32 msk,
			 u32 shift, u32 val)
{
	if (msk ^ ~0) {
		u32 reg_old, reg_new;

		reg_old = aq_hw_read_reg(aq_hw, addr);
		reg_new = (reg_old & (~msk)) | (val << shift);

		if (reg_old != reg_new)
			aq_hw_write_reg(aq_hw, addr, reg_new);
	} else {
		aq_hw_write_reg(aq_hw, addr, val);
	}
}

u32 aq_hw_read_reg_bit(struct aq_hw_s *aq_hw, u32 addr, u32 msk, u32 shift)
{
	return ((aq_hw_read_reg(aq_hw, addr) & msk) >> shift);
}

u32 aq_hw_read_reg(struct aq_hw_s *hw, u32 reg)
{
	return rte_le_to_cpu_32(rte_read32((u8 *)hw->mmio + reg));
}

void aq_hw_write_reg(struct aq_hw_s *hw, u32 reg, u32 value)
{
	rte_write32((rte_cpu_to_le_32(value)), (u8 *)hw->mmio + reg);
}

int aq_hw_err_from_flags(struct aq_hw_s *hw)
{
	int err = 0;

	if (aq_hw_read_reg(hw, 0x10U) == ~0U)
		return -ENXIO;

	return err;
}
