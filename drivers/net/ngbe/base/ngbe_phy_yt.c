/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#include "ngbe_phy_yt.h"

#define YT_PHY_RST_WAIT_PERIOD		5

s32 ngbe_read_phy_reg_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 *phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;

	ngbe_mdi_map_register(&reg, &reg22);

	/* Read MII reg according to media type */
	if (hw->phy.media_type == ngbe_media_type_fiber) {
		ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY,
					reg22.device_type, YT_SMI_PHY_SDS);
		ngbe_read_phy_reg_mdi(hw, reg22.addr,
					reg22.device_type, phy_data);
		ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY,
					reg22.device_type, 0);
	} else {
		ngbe_read_phy_reg_mdi(hw, reg22.addr,
					reg22.device_type, phy_data);
	}

	return 0;
}

s32 ngbe_write_phy_reg_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;

	ngbe_mdi_map_register(&reg, &reg22);

	/* Write MII reg according to media type */
	if (hw->phy.media_type == ngbe_media_type_fiber) {
		ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY,
					reg22.device_type, YT_SMI_PHY_SDS);
		ngbe_write_phy_reg_mdi(hw, reg22.addr,
					reg22.device_type, phy_data);
		ngbe_write_phy_reg_ext_yt(hw, YT_SMI_PHY,
					reg22.device_type, 0);
	} else {
		ngbe_write_phy_reg_mdi(hw, reg22.addr,
					reg22.device_type, phy_data);
	}

	return 0;
}

s32 ngbe_read_phy_reg_ext_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 *phy_data)
{
	ngbe_write_phy_reg_mdi(hw, 0x1E, device_type, reg_addr);
	ngbe_read_phy_reg_mdi(hw, 0x1F, device_type, phy_data);

	return 0;
}

s32 ngbe_write_phy_reg_ext_yt(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 phy_data)
{
	ngbe_write_phy_reg_mdi(hw, 0x1E, device_type, reg_addr);
	ngbe_write_phy_reg_mdi(hw, 0x1F, device_type, phy_data);

	return 0;
}

s32 ngbe_reset_phy_yt(struct ngbe_hw *hw)
{
	u32 i;
	u16 ctrl = 0;
	s32 status = 0;

	DEBUGFUNC("ngbe_reset_phy_yt");

	if (hw->phy.type != ngbe_phy_yt8521s &&
		hw->phy.type != ngbe_phy_yt8521s_sfi)
		return NGBE_ERR_PHY_TYPE;

	status = hw->phy.read_reg(hw, YT_BCR, 0, &ctrl);
	/* sds software reset */
	ctrl |= YT_BCR_RESET;
	status = hw->phy.write_reg(hw, YT_BCR, 0, ctrl);

	for (i = 0; i < YT_PHY_RST_WAIT_PERIOD; i++) {
		status = hw->phy.read_reg(hw, YT_BCR, 0, &ctrl);
		if (!(ctrl & YT_BCR_RESET))
			break;
		msleep(1);
	}

	if (i == YT_PHY_RST_WAIT_PERIOD) {
		DEBUGOUT("PHY reset polling failed to complete.\n");
		return NGBE_ERR_RESET_FAILED;
	}

	return status;
}

