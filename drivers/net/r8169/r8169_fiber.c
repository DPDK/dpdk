/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include <rte_ether.h>
#include <ethdev_driver.h>

#include "r8169_fiber.h"

static bool
rtl8127_wait_8127_sds_cmd_done(struct rtl_hw *hw)
{
	u32 timeout = 0;
	u32 waitcount = 100;

	do {
		if (RTL_R16(hw, R8127_SDS_8127_CMD) & R8127_SDS_8127_CMD_IN)
			rte_delay_us(1);
		else
			return true;
	} while (++timeout < waitcount);

	return false;
}

static u16
rtl8127_sds_phy_read_8127(struct rtl_hw *hw, u16 index, u16 page, u16 reg)
{
	RTL_W16(hw, R8127_SDS_8127_ADDR,
		R8127_MAKE_SDS_8127_ADDR(index, page, reg));
	RTL_W16(hw, R8127_SDS_8127_CMD, R8127_SDS_8127_CMD_IN);

	if (rtl8127_wait_8127_sds_cmd_done(hw))
		return RTL_R16(hw, R8127_SDS_8127_DATA_OUT);
	else
		return 0xffff;
}

static void
rtl8127_sds_phy_write_8127(struct rtl_hw *hw, u16 index, u16 page, u16 reg,
			   u16 val)
{
	RTL_W16(hw, R8127_SDS_8127_DATA_IN, val);
	RTL_W16(hw, R8127_SDS_8127_ADDR,
		R8127_MAKE_SDS_8127_ADDR(index, page, reg));
	RTL_W16(hw, R8127_SDS_8127_CMD,
		R8127_SDS_8127_CMD_IN | R8127_SDS_8127_WE_IN);

	rtl8127_wait_8127_sds_cmd_done(hw);
}

static void
rtl8127_clear_and_set_sds_phy_bit(struct rtl_hw *hw, u16 index, u16 page,
				  u16 addr, u16 clearmask, u16 setmask)
{
	u16 val;

	val = rtl8127_sds_phy_read_8127(hw, index, page, addr);
	val &= ~clearmask;
	val |= setmask;
	rtl8127_sds_phy_write_8127(hw, index, page, addr, val);
}

static void
rtl8127_clear_sds_phy_bit(struct rtl_hw *hw, u16 index, u16 page,
			  u16 addr, u16 mask)
{
	rtl8127_clear_and_set_sds_phy_bit(hw, index, page, addr, mask, 0);
}

static void
rtl8127_set_sds_phy_bit(struct rtl_hw *hw, u16 index, u16 page, u16 addr,
			u16 mask)
{
	rtl8127_clear_and_set_sds_phy_bit(hw, index, page, addr, 0, mask);
}

static void
rtl8127_sds_phy_reset_8127(struct rtl_hw *hw)
{
	RTL_W8(hw, 0x2350, RTL_R8(hw, 0x2350) & ~BIT_0);
	rte_delay_us(1);

	RTL_W16(hw, 0x233A, 0x801F);
	RTL_W8(hw, 0x2350, RTL_R8(hw, 0x2350) | BIT_0);
	rte_delay_us(10);
}

static void
rtl8127_sds_phy_reset(struct rtl_hw *hw)
{
	switch (hw->HwFiberModeVer) {
	case FIBER_MODE_RTL8127ATF:
		rtl8127_sds_phy_reset_8127(hw);
		break;
	default:
		break;
	}
}

static void
rtl8127_set_sds_phy_caps_1g_8127(struct rtl_hw *hw)
{
	u16 val;

	if (hw->fcpause == rtl_fc_full)
		rtl8127_set_sds_phy_bit(hw, 0, 2, 4, BIT_8 | BIT_7);
	else
		rtl8127_clear_sds_phy_bit(hw, 0, 2, 4, BIT_8 | BIT_7);

	rtl8127_set_sds_phy_bit(hw, 0, 1, 31, BIT_3);
	rtl8127_clear_and_set_sds_phy_bit(hw, 0, 2, 0, BIT_13 | BIT_12 | BIT_6,
					  BIT_12 | BIT_6);
	rtl8127_set_sds_phy_bit(hw, 0, 0, 4, BIT_2);
	RTL_W16(hw, 0x233A, 0x8004);

	val = RTL_R16(hw, 0x233E);
	val &= ~(BIT_13 | BIT_12 | BIT_1 | BIT_0);
	val |= BIT_1;
	RTL_W16(hw, 0x233E, val);

	rtl_mdio_direct_write_phy_ocp(hw, 0xC40A, 0x0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xC466, 0x0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xC808, 0x0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xC80A, 0x0);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xC804, 0x000F, 0x000C);
}

static void
rtl8127_sds_phy_exit_1g_8127(struct rtl_hw *hw)
{
	rtl8127_clear_sds_phy_bit(hw, 0, 1, 31, BIT_3);
	rtl8127_clear_and_set_sds_phy_bit(hw, 0, 2, 0, BIT_13 | BIT_12 | BIT_6,
					  BIT_6);

	rtl8127_sds_phy_reset(hw);
}

static void
rtl8127_set_sds_phy_caps_10g_8127(struct rtl_hw *hw)
{
	u16 val;

	if (hw->fcpause == rtl_fc_full)
		rtl8127_set_sds_phy_bit(hw, 0, 31, 11, BIT_3 | BIT_2);
	else
		rtl8127_clear_sds_phy_bit(hw, 0, 31, 11, BIT_3 | BIT_2);

	RTL_W16(hw, 0x233A, 0x801A);

	val = RTL_R16(hw, 0x233E);
	val &= ~(BIT_13 | BIT_12 | BIT_1 | BIT_0);
	val |= BIT_12;
	RTL_W16(hw, 0x233E, val);

	rtl_mdio_direct_write_phy_ocp(hw, 0xC40A, 0x0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xC466, 0x3);
	rtl_mdio_direct_write_phy_ocp(hw, 0xC808, 0x0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xC80A, 0x0);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xC804, 0x000F, 0x000C);
}

static void
rtl8127_set_sds_phy_caps_8127(struct rtl_hw *hw)
{
	rtl8127_sds_phy_exit_1g_8127(hw);

	switch (hw->speed) {
	case SPEED_10000:
		rtl8127_set_sds_phy_caps_10g_8127(hw);
		break;
	case SPEED_1000:
		rtl8127_set_sds_phy_caps_1g_8127(hw);
		break;
	default:
		break;
	}
}

static void
rtl8127_set_sds_phy_caps(struct rtl_hw *hw)
{
	switch (hw->HwFiberModeVer) {
	case FIBER_MODE_RTL8127ATF:
		rtl8127_set_sds_phy_caps_8127(hw);
		break;
	default:
		break;
	}
}

static void
rtl8127_hw_sds_phy_config(struct rtl_hw *hw)
{
	rtl8127_set_sds_phy_caps(hw);
}

void
rtl8127_hw_fiber_phy_config(struct rtl_hw *hw)
{
	switch (hw->HwFiberModeVer) {
	case FIBER_MODE_RTL8127ATF:
		rtl8127_hw_sds_phy_config(hw);
		break;
	default:
		break;
	}
}
