/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#include "ngbe_phy_mvl.h"

#define MVL_PHY_RST_WAIT_PERIOD  5

s32 ngbe_read_phy_reg_mvl(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 *phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;

	if (hw->phy.media_type == ngbe_media_type_fiber)
		ngbe_write_phy_reg_mdi(hw, MVL_PAGE_SEL, 0, 1);
	else
		ngbe_write_phy_reg_mdi(hw, MVL_PAGE_SEL, 0, 0);

	ngbe_mdi_map_register(&reg, &reg22);

	ngbe_read_phy_reg_mdi(hw, reg22.addr, reg22.device_type, phy_data);

	return 0;
}

s32 ngbe_write_phy_reg_mvl(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;

	if (hw->phy.media_type == ngbe_media_type_fiber)
		ngbe_write_phy_reg_mdi(hw, MVL_PAGE_SEL, 0, 1);
	else
		ngbe_write_phy_reg_mdi(hw, MVL_PAGE_SEL, 0, 0);

	ngbe_mdi_map_register(&reg, &reg22);

	ngbe_write_phy_reg_mdi(hw, reg22.addr, reg22.device_type, phy_data);

	return 0;
}

s32 ngbe_reset_phy_mvl(struct ngbe_hw *hw)
{
	u32 i;
	u16 ctrl = 0;
	s32 status = 0;

	DEBUGFUNC("ngbe_reset_phy_mvl");

	if (hw->phy.type != ngbe_phy_mvl && hw->phy.type != ngbe_phy_mvl_sfi)
		return NGBE_ERR_PHY_TYPE;

	/* select page 18 reg 20 */
	status = ngbe_write_phy_reg_mdi(hw, MVL_PAGE_SEL, 0, 18);

	/* mode select to RGMII-to-copper or RGMII-to-sfi*/
	if (hw->phy.type == ngbe_phy_mvl)
		ctrl = MVL_GEN_CTL_MODE_COPPER;
	else
		ctrl = MVL_GEN_CTL_MODE_FIBER;
	status = ngbe_write_phy_reg_mdi(hw, MVL_GEN_CTL, 0, ctrl);
	/* mode reset */
	ctrl |= MVL_GEN_CTL_RESET;
	status = ngbe_write_phy_reg_mdi(hw, MVL_GEN_CTL, 0, ctrl);

	for (i = 0; i < MVL_PHY_RST_WAIT_PERIOD; i++) {
		status = ngbe_read_phy_reg_mdi(hw, MVL_GEN_CTL, 0, &ctrl);
		if (!(ctrl & MVL_GEN_CTL_RESET))
			break;
		msleep(1);
	}

	if (i == MVL_PHY_RST_WAIT_PERIOD) {
		DEBUGOUT("PHY reset polling failed to complete.\n");
		return NGBE_ERR_RESET_FAILED;
	}

	return status;
}

