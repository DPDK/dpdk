/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include <rte_ether.h>
#include <ethdev_pci.h>

#include "r8169_ethdev.h"
#include "r8169_hw.h"
#include "r8169_phy.h"

static void
rtl_clear_set_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 clearmask,
			  u16 setmask)
{
	u16 phy_reg_value;

	phy_reg_value = rtl_mac_ocp_read(hw, addr);
	phy_reg_value &= ~clearmask;
	phy_reg_value |= setmask;
	rtl_mac_ocp_write(hw, addr, phy_reg_value);
}

void
rtl_clear_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask)
{
	rtl_clear_set_mac_ocp_bit(hw, addr, mask, 0);
}

void
rtl_set_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask)
{
	rtl_clear_set_mac_ocp_bit(hw, addr, 0, mask);
}

static u16
rtl_map_phy_ocp_addr(u16 PageNum, u8 RegNum)
{
	u8 ocp_reg_num = 0;
	u16 ocp_page_num = 0;
	u16 ocp_phy_address = 0;

	if (PageNum == 0) {
		ocp_page_num = OCP_STD_PHY_BASE_PAGE + (RegNum / 8);
		ocp_reg_num = 0x10 + (RegNum % 8);
	} else {
		ocp_page_num = PageNum;
		ocp_reg_num = RegNum;
	}

	ocp_page_num <<= 4;

	if (ocp_reg_num < 16) {
		ocp_phy_address = 0;
	} else {
		ocp_reg_num -= 16;
		ocp_reg_num <<= 1;

		ocp_phy_address = ocp_page_num + ocp_reg_num;
	}

	return ocp_phy_address;
}

static u32
rtl_mdio_real_read_phy_ocp(struct rtl_hw *hw, u32 RegAddr)
{
	u32 data32;
	int i, value = 0;

	data32 = RegAddr / 2;
	data32 <<= OCPR_Addr_Reg_shift;

	RTL_W32(hw, PHYOCP, data32);
	for (i = 0; i < 100; i++) {
		rte_delay_us(1);

		if (RTL_R32(hw, PHYOCP) & OCPR_Flag)
			break;
	}
	value = RTL_R32(hw, PHYOCP) & OCPDR_Data_Mask;

	return value;
}

u32
rtl_mdio_direct_read_phy_ocp(struct rtl_hw *hw, u32 RegAddr)
{
	return rtl_mdio_real_read_phy_ocp(hw, RegAddr);
}

static u32
rtl_mdio_read_phy_ocp(struct rtl_hw *hw, u16 PageNum, u32 RegAddr)
{
	u16 ocp_addr;

	ocp_addr = rtl_map_phy_ocp_addr(PageNum, RegAddr);

	return rtl_mdio_direct_read_phy_ocp(hw, ocp_addr);
}

static u32
rtl_mdio_real_read(struct rtl_hw *hw, u32 RegAddr)
{
	return rtl_mdio_read_phy_ocp(hw, hw->cur_page, RegAddr);
}

static void
rtl_mdio_real_write_phy_ocp(struct rtl_hw *hw, u32 RegAddr, u32 value)
{
	u32 data32;
	int i;

	data32 = RegAddr / 2;
	data32 <<= OCPR_Addr_Reg_shift;
	data32 |= OCPR_Write | value;

	RTL_W32(hw, PHYOCP, data32);
	for (i = 0; i < 100; i++) {
		rte_delay_us(1);

		if (!(RTL_R32(hw, PHYOCP) & OCPR_Flag))
			break;
	}
}

void
rtl_mdio_direct_write_phy_ocp(struct rtl_hw *hw, u32 RegAddr, u32 value)
{
	rtl_mdio_real_write_phy_ocp(hw, RegAddr, value);
}

static void
rtl_mdio_write_phy_ocp(struct rtl_hw *hw, u16 PageNum, u32 RegAddr, u32 value)
{
	u16 ocp_addr;

	ocp_addr = rtl_map_phy_ocp_addr(PageNum, RegAddr);

	rtl_mdio_direct_write_phy_ocp(hw, ocp_addr, value);
}

static void
rtl_mdio_real_write(struct rtl_hw *hw, u32 RegAddr, u32 value)
{
	if (RegAddr == 0x1F)
		hw->cur_page = value;
	rtl_mdio_write_phy_ocp(hw, hw->cur_page, RegAddr, value);
}

u32
rtl_mdio_read(struct rtl_hw *hw, u32 RegAddr)
{
	return rtl_mdio_real_read(hw, RegAddr);
}

void
rtl_mdio_write(struct rtl_hw *hw, u32 RegAddr, u32 value)
{
	rtl_mdio_real_write(hw, RegAddr, value);
}

void
rtl_clear_and_set_eth_phy_ocp_bit(struct rtl_hw *hw, u16 addr, u16 clearmask,
				  u16 setmask)
{
	u16 phy_reg_value;

	phy_reg_value = rtl_mdio_direct_read_phy_ocp(hw, addr);
	phy_reg_value &= ~clearmask;
	phy_reg_value |= setmask;
	rtl_mdio_direct_write_phy_ocp(hw, addr, phy_reg_value);
}

void
rtl_clear_eth_phy_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask)
{
	rtl_clear_and_set_eth_phy_ocp_bit(hw, addr, mask, 0);
}

void
rtl_set_eth_phy_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask)
{
	rtl_clear_and_set_eth_phy_ocp_bit(hw, addr, 0, mask);
}

void
rtl_ephy_write(struct rtl_hw *hw, int addr, int value)
{
	int i;

	RTL_W32(hw, EPHYAR, EPHYAR_Write |
		(addr & EPHYAR_Reg_Mask_v2) << EPHYAR_Reg_shift |
		(value & EPHYAR_Data_Mask));

	for (i = 0; i < 10; i++) {
		rte_delay_us(100);

		/* Check if the NIC has completed EPHY write */
		if (!(RTL_R32(hw, EPHYAR) & EPHYAR_Flag))
			break;
	}

	rte_delay_us(20);
}

static u16
rtl_ephy_read(struct rtl_hw *hw, int addr)
{
	int i;
	u16 value = 0xffff;

	RTL_W32(hw, EPHYAR, EPHYAR_Read | (addr & EPHYAR_Reg_Mask_v2) <<
		EPHYAR_Reg_shift);

	for (i = 0; i < 10; i++) {
		rte_delay_us(100);

		/* Check if the NIC has completed EPHY read */
		if (RTL_R32(hw, EPHYAR) & EPHYAR_Flag) {
			value = (u16)(RTL_R32(hw, EPHYAR) & EPHYAR_Data_Mask);
			break;
		}
	}

	rte_delay_us(20);

	return value;
}

void
rtl_clear_and_set_pcie_phy_bit(struct rtl_hw *hw, u8 addr, u16 clearmask,
			       u16 setmask)
{
	u16 ephy_value;

	ephy_value = rtl_ephy_read(hw, addr);
	ephy_value &= ~clearmask;
	ephy_value |= setmask;
	rtl_ephy_write(hw, addr, ephy_value);
}

void
rtl_clear_pcie_phy_bit(struct rtl_hw *hw, u8 addr, u16 mask)
{
	rtl_clear_and_set_pcie_phy_bit(hw, addr, mask, 0);
}

void
rtl_set_pcie_phy_bit(struct rtl_hw *hw, u8 addr, u16 mask)
{
	rtl_clear_and_set_pcie_phy_bit(hw, addr, 0, mask);
}
