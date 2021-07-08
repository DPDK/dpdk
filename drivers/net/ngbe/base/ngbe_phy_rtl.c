/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#include "ngbe_phy_rtl.h"

#define RTL_PHY_RST_WAIT_PERIOD               5

s32 ngbe_read_phy_reg_rtl(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 *phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;
	ngbe_mdi_map_register(&reg, &reg22);

	wr32(hw, NGBE_PHY_CONFIG(RTL_PAGE_SELECT), reg22.page);
	*phy_data = 0xFFFF & rd32(hw, NGBE_PHY_CONFIG(reg22.addr));

	return 0;
}

s32 ngbe_write_phy_reg_rtl(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;
	ngbe_mdi_map_register(&reg, &reg22);

	wr32(hw, NGBE_PHY_CONFIG(RTL_PAGE_SELECT), reg22.page);
	wr32(hw, NGBE_PHY_CONFIG(reg22.addr), phy_data);

	return 0;
}

s32 ngbe_reset_phy_rtl(struct ngbe_hw *hw)
{
	u16 value = 0, i;
	s32 status = 0;

	DEBUGFUNC("ngbe_reset_phy_rtl");

	value |= RTL_BMCR_RESET;
	status = hw->phy.write_reg(hw, RTL_BMCR, RTL_DEV_ZERO, value);

	for (i = 0; i < RTL_PHY_RST_WAIT_PERIOD; i++) {
		status = hw->phy.read_reg(hw, RTL_BMCR, RTL_DEV_ZERO, &value);
		if (!(value & RTL_BMCR_RESET))
			break;
		msleep(1);
	}

	if (i == RTL_PHY_RST_WAIT_PERIOD) {
		DEBUGOUT("PHY reset polling failed to complete.\n");
		return NGBE_ERR_RESET_FAILED;
	}

	return status;
}

