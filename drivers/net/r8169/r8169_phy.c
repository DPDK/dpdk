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
#include "r8169_logs.h"
#include "r8169_dash.h"
#include "r8169_fiber.h"

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
rtl_mdio_real_direct_read_phy_ocp(struct rtl_hw *hw, u32 RegAddr)
{
	u32 data32;
	int i, value = 0;

	data32 = RegAddr / 2;
	data32 <<= OCPR_Addr_Reg_shift;

	RTL_W32(hw, PHYOCP, data32);
	for (i = 0; i < RTL_CHANNEL_WAIT_COUNT; i++) {
		rte_delay_us(RTL_CHANNEL_WAIT_TIME);

		if (RTL_R32(hw, PHYOCP) & OCPR_Flag)
			break;
	}
	value = RTL_R32(hw, PHYOCP) & OCPDR_Data_Mask;

	return value;
}

u32
rtl_mdio_direct_read_phy_ocp(struct rtl_hw *hw, u32 RegAddr)
{
	return rtl_mdio_real_direct_read_phy_ocp(hw, RegAddr);
}

u32
rtl_mdio_real_read_phy_ocp(struct rtl_hw *hw, u16 PageNum, u32 RegAddr)
{
	u16 ocp_addr;

	ocp_addr = rtl_map_phy_ocp_addr(PageNum, RegAddr);

	return rtl_mdio_real_direct_read_phy_ocp(hw, ocp_addr);
}

static u32
rtl_mdio_real_read(struct rtl_hw *hw, u32 RegAddr)
{
	return rtl_mdio_real_read_phy_ocp(hw, hw->cur_page, RegAddr);
}

u32
rtl_mdio_read(struct rtl_hw *hw, u32 RegAddr)
{
	return rtl_mdio_real_read(hw, RegAddr);
}

static void
rtl_mdio_real_direct_write_phy_ocp(struct rtl_hw *hw, u32 RegAddr, u32 value)
{
	u32 data32;
	int i;

	data32 = RegAddr / 2;
	data32 <<= OCPR_Addr_Reg_shift;
	data32 |= OCPR_Write | value;

	RTL_W32(hw, PHYOCP, data32);
	for (i = 0; i < RTL_CHANNEL_WAIT_COUNT; i++) {
		rte_delay_us(RTL_CHANNEL_WAIT_TIME);

		if (!(RTL_R32(hw, PHYOCP) & OCPR_Flag))
			break;
	}
}

void
rtl_mdio_direct_write_phy_ocp(struct rtl_hw *hw, u32 RegAddr, u32 value)
{
	rtl_mdio_real_direct_write_phy_ocp(hw, RegAddr, value);
}

void
rtl_mdio_real_write_phy_ocp(struct rtl_hw *hw, u16 PageNum, u32 RegAddr, u32 value)
{
	u16 ocp_addr;

	ocp_addr = rtl_map_phy_ocp_addr(PageNum, RegAddr);

	rtl_mdio_direct_write_phy_ocp(hw, ocp_addr, value);
}

static void
rtl_mdio_real_write(struct rtl_hw *hw, u32 RegAddr, u32 value)
{
	if (RegAddr == 0x1F) {
		hw->cur_page = value;
		return;
	}
	rtl_mdio_real_write_phy_ocp(hw, hw->cur_page, RegAddr, value);
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
	u16 val;

	val = rtl_mdio_direct_read_phy_ocp(hw, addr);
	val &= ~clearmask;
	val |= setmask;
	rtl_mdio_direct_write_phy_ocp(hw, addr, val);
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

static u8
rtl8168_check_ephy_addr(struct rtl_hw *hw, int addr)
{
	if (hw->mcfg != CFG_METHOD_35 && hw->mcfg != CFG_METHOD_36)
		goto exit;

	if (addr & (BIT_6 | BIT_5))
		rtl8168_clear_and_set_mcu_ocp_bit(hw, 0xDE28, (BIT_1 | BIT_0),
						  (addr >> 5) & (BIT_1 | BIT_0));

	addr &= 0x1F;

exit:
	return addr;
}

static void
_rtl_ephy_write(struct rtl_hw *hw, int addr, int value, unsigned int mask)
{
	int i;

	RTL_W32(hw, EPHYAR, EPHYAR_Write | (addr & mask) << EPHYAR_Reg_shift |
		(value & EPHYAR_Data_Mask));

	for (i = 0; i < RTL_CHANNEL_WAIT_COUNT; i++) {
		rte_delay_us(RTL_CHANNEL_WAIT_TIME);

		/* Check if the NIC has completed EPHY write */
		if (!(RTL_R32(hw, EPHYAR) & EPHYAR_Flag))
			break;
	}

	rte_delay_us(RTL_CHANNEL_EXIT_DELAY_TIME);
}

static void
rtl8127_set_ephy_ext_addr(struct rtl_hw *hw, int addr)
{
	_rtl_ephy_write(hw, EPHYAR_EXT_ADDR, addr, EPHYAR_Reg_Mask_v2);
}

static int
rtl8127_check_ephy_ext_addr(struct rtl_hw *hw, int addr)
{
	int data;

	data = ((u16)addr >> 12);

	rtl8127_set_ephy_ext_addr(hw, data);

	return (addr & 0xfff);
}

void
rtl_ephy_write(struct rtl_hw *hw, int addr, int value)
{
	unsigned int mask;

	if (!rtl_is_8125(hw)) {
		mask = EPHYAR_Reg_Mask;
		addr = rtl8168_check_ephy_addr(hw, addr);
	} else if (hw->mcfg >= CFG_METHOD_91) {
		mask = EPHYAR_Reg_Mask_v2;
		addr = rtl8127_check_ephy_ext_addr(hw, addr);
	} else {
		mask = EPHYAR_Reg_Mask_v2;
	}

	_rtl_ephy_write(hw, addr, value, mask);
}

static u16
_rtl_ephy_read(struct rtl_hw *hw, int addr, unsigned int mask)
{
	int i;
	u16 value = 0xffff;

	RTL_W32(hw, EPHYAR, EPHYAR_Read | (addr & mask) << EPHYAR_Reg_shift);

	for (i = 0; i < RTL_CHANNEL_WAIT_COUNT; i++) {
		rte_delay_us(RTL_CHANNEL_WAIT_TIME);

		/* Check if the NIC has completed EPHY read */
		if (RTL_R32(hw, EPHYAR) & EPHYAR_Flag) {
			value = (u16)(RTL_R32(hw, EPHYAR) & EPHYAR_Data_Mask);
			break;
		}
	}

	rte_delay_us(RTL_CHANNEL_EXIT_DELAY_TIME);

	return value;
}

u16
rtl_ephy_read(struct rtl_hw *hw, int addr)
{
	unsigned int mask;

	if (!rtl_is_8125(hw)) {
		mask = EPHYAR_Reg_Mask;
		addr = rtl8168_check_ephy_addr(hw, addr);
	} else if (hw->mcfg >= CFG_METHOD_91) {
		mask = EPHYAR_Reg_Mask_v2;
		addr = rtl8127_check_ephy_ext_addr(hw, addr);
	} else {
		mask = EPHYAR_Reg_Mask_v2;
	}

	return _rtl_ephy_read(hw, addr, mask);
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

bool
rtl_set_phy_mcu_patch_request(struct rtl_hw *hw)
{
	u16 gphy_val;
	u16 wait_cnt;
	bool bool_success = TRUE;

	if (rtl_is_8125(hw)) {
		rtl_set_eth_phy_ocp_bit(hw, 0xB820, BIT_4);

		wait_cnt = 0;
		do {
			gphy_val = rtl_mdio_direct_read_phy_ocp(hw, 0xB800);
			rte_delay_us(100);
			wait_cnt++;
		} while (!(gphy_val & BIT_6) && (wait_cnt < 1000));

		if (!(gphy_val & BIT_6) && wait_cnt == 1000)
			bool_success = FALSE;
	} else {
		rtl_mdio_write(hw, 0x1f, 0x0B82);
		rtl_set_eth_phy_bit(hw, 0x10, BIT_4);

		rtl_mdio_write(hw, 0x1f, 0x0B80);
		wait_cnt = 0;
		do {
			gphy_val = rtl_mdio_read(hw, 0x10);
			rte_delay_us(100);
			wait_cnt++;
		}  while (!(gphy_val & BIT_6) && (wait_cnt < 1000));

		if (!(gphy_val & BIT_6) && wait_cnt == 1000)
			bool_success = FALSE;

		rtl_mdio_write(hw, 0x1f, 0x0000);
	}

	if (!bool_success)
		PMD_INIT_LOG(NOTICE, "%s fail.", __func__);

	return bool_success;
}

bool
rtl_clear_phy_mcu_patch_request(struct rtl_hw *hw)
{
	u16 gphy_val;
	u16 wait_cnt;
	bool bool_success = TRUE;

	if (rtl_is_8125(hw)) {
		rtl_clear_eth_phy_ocp_bit(hw, 0xB820, BIT_4);

		wait_cnt = 0;
		do {
			gphy_val = rtl_mdio_direct_read_phy_ocp(hw, 0xB800);
			rte_delay_us(100);
			wait_cnt++;
		} while ((gphy_val & BIT_6) && (wait_cnt < 1000));

		if ((gphy_val & BIT_6) && wait_cnt == 1000)
			bool_success = FALSE;
	} else {
		rtl_mdio_write(hw, 0x1f, 0x0B82);
		rtl_clear_eth_phy_bit(hw, 0x10, BIT_4);

		rtl_mdio_write(hw, 0x1f, 0x0B80);
		wait_cnt = 0;
		do {
			gphy_val = rtl_mdio_read(hw, 0x10);
			rte_delay_us(100);
			wait_cnt++;
		} while ((gphy_val & BIT_6) && (wait_cnt < 1000));

		if ((gphy_val & BIT_6) && wait_cnt == 1000)
			bool_success = FALSE;

		rtl_mdio_write(hw, 0x1f, 0x0000);
	}

	if (!bool_success)
		PMD_INIT_LOG(NOTICE, "%s fail.", __func__);

	return bool_success;
}

void
rtl_set_phy_mcu_ram_code(struct rtl_hw *hw, const u16 *ramcode, u16 codesize)
{
	u16 i;
	u16 addr;
	u16 val;

	if (ramcode == NULL || codesize % 2)
		goto out;

	for (i = 0; i < codesize; i += 2) {
		addr = ramcode[i];
		val = ramcode[i + 1];
		if (addr == 0xFFFF && val == 0xFFFF)
			break;
		rtl_mdio_direct_write_phy_ocp(hw, addr, val);
	}

out:
	return;
}

static u8
rtl_is_phy_disable_mode_enabled(struct rtl_hw *hw)
{
	u8 phy_disable_mode_enabled = FALSE;

	switch (hw->HwSuppCheckPhyDisableModeVer) {
	case 1:
		if (rtl_mac_ocp_read(hw, 0xDC20) & BIT_1)
			phy_disable_mode_enabled = TRUE;
		break;
	case 2:
	case 3:
		if (RTL_R8(hw, 0xF2) & BIT_5)
			phy_disable_mode_enabled = TRUE;
		break;
	}

	return phy_disable_mode_enabled;
}

static u8
rtl_is_gpio_low(struct rtl_hw *hw)
{
	u8 gpio_low = FALSE;

	switch (hw->HwSuppCheckPhyDisableModeVer) {
	case 1:
	case 2:
		if (!(rtl_mac_ocp_read(hw, 0xDC04) & BIT_9))
			gpio_low = TRUE;
		break;
	case 3:
		if (!(rtl_mac_ocp_read(hw, 0xDC04) & BIT_13))
			gpio_low = TRUE;
		break;
	}

	return gpio_low;
}

static u8
rtl_is_in_phy_disable_mode(struct rtl_hw *hw)
{
	u8 in_phy_disable_mode = FALSE;

	if (rtl_is_phy_disable_mode_enabled(hw) && rtl_is_gpio_low(hw))
		in_phy_disable_mode = TRUE;

	return in_phy_disable_mode;
}

static void
rtl_wait_phy_ups_resume(struct rtl_hw *hw, u16 PhyState)
{
	u16 tmp_phy_state;
	int i = 0;

	switch (hw->mcfg) {
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
		do {
			tmp_phy_state = rtl_mdio_real_read_phy_ocp(hw, 0x0A42, 0x10);
			tmp_phy_state &= 0x7;
			rte_delay_ms(1);
			i++;
		} while ((i < 100) && (tmp_phy_state != PhyState));
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_58:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		do {
			tmp_phy_state = rtl_mdio_direct_read_phy_ocp(hw, 0xA420);
			tmp_phy_state &= 0x7;
			rte_delay_ms(1);
			i++;
		} while ((i < 100) && (tmp_phy_state != PhyState));
		break;
	}
}

static void
rtl_phy_power_up(struct rtl_hw *hw)
{
	if (rtl_is_in_phy_disable_mode(hw))
		return;

	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, MII_BMCR, BMCR_ANENABLE);

	/* Wait mdc/mdio ready */
	switch (hw->mcfg) {
	case CFG_METHOD_23:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
		rte_delay_ms(10);
		break;
	}

	/* Wait ups resume (phy state 3) */
	rtl_wait_phy_ups_resume(hw, 3);
}

void
rtl_powerup_pll(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_26:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_58:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		RTL_W8(hw, PMCH, RTL_R8(hw, PMCH) | BIT_7 | BIT_6);
		break;
	}

	rtl_phy_power_up(hw);
}

static void
rtl_phy_power_down(struct rtl_hw *hw)
{
	u32 csi_tmp;

	/* MCU PME intr masks */
	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_23:
	case CFG_METHOD_24:
		csi_tmp = rtl_eri_read(hw, 0x1AB, 1, ERIAR_ExGMAC);
		csi_tmp &= ~(BIT_2 | BIT_3 | BIT_4 | BIT_5 | BIT_6 | BIT_7);
		rtl_eri_write(hw, 0x1AB, 1, csi_tmp, ERIAR_ExGMAC);
		break;
	case CFG_METHOD_25:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
		csi_tmp = rtl_eri_read(hw, 0x1AB, 1, ERIAR_ExGMAC);
		csi_tmp &= ~(BIT_3 | BIT_6);
		rtl_eri_write(hw, 0x1AB, 1, csi_tmp, ERIAR_ExGMAC);
		break;
	}

	rtl_mdio_write(hw, 0x1F, 0x0000);

	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_23:
	case CFG_METHOD_24:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_58:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		rtl_mdio_write(hw, MII_BMCR, BMCR_ANENABLE | BMCR_PDOWN);
		break;
	default:
		rtl_mdio_write(hw, MII_BMCR, BMCR_PDOWN);
		break;
	}
}

void
rtl_powerdown_pll(struct rtl_hw *hw)
{
	if (hw->DASH)
		return;

	rtl_phy_power_down(hw);

	if (!hw->HwIcVerUnknown) {
		switch (hw->mcfg) {
		case CFG_METHOD_21:
		case CFG_METHOD_22:
		case CFG_METHOD_24:
		case CFG_METHOD_25:
		case CFG_METHOD_26:
		case CFG_METHOD_27:
		case CFG_METHOD_28:
		case CFG_METHOD_29:
		case CFG_METHOD_30:
		case CFG_METHOD_31:
		case CFG_METHOD_32:
		case CFG_METHOD_33:
		case CFG_METHOD_34:
		case CFG_METHOD_35:
		case CFG_METHOD_36:
		case CFG_METHOD_48:
		case CFG_METHOD_49:
		case CFG_METHOD_50:
		case CFG_METHOD_51:
		case CFG_METHOD_52:
		case CFG_METHOD_53:
		case CFG_METHOD_54:
		case CFG_METHOD_55:
		case CFG_METHOD_56:
		case CFG_METHOD_57:
		case CFG_METHOD_58:
		case CFG_METHOD_69:
		case CFG_METHOD_70:
		case CFG_METHOD_71:
		case CFG_METHOD_91:
			RTL_W8(hw, PMCH, RTL_R8(hw, PMCH) & ~BIT_7);
			break;
		}
	}

	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_23:
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_26:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
		RTL_W8(hw, 0xD0, RTL_R8(hw, 0xD0) & ~BIT_6);
		RTL_W8(hw, 0xF2, RTL_R8(hw, 0xF2) & ~BIT_6);
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_58:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		RTL_W8(hw, 0xF2, RTL_R8(hw, 0xF2) & ~BIT_6);
		break;
	}
}

void
rtl_hw_ephy_config(struct rtl_hw *hw)
{
	hw->hw_ops.hw_ephy_config(hw);
}

static int
rtl_wait_phy_reset_complete(struct rtl_hw *hw)
{
	int i, val;

	for (i = 0; i < 2500; i++) {
		val = rtl_mdio_read(hw, MII_BMCR) & BMCR_RESET;
		if (!val)
			return 0;

		rte_delay_ms(1);
	}

	return -1;
}

static void
rtl_xmii_reset_enable(struct rtl_hw *hw)
{
	u32 val;

	if (rtl_is_in_phy_disable_mode(hw))
		return;

	rtl_mdio_write(hw, 0x1F, 0x0000);

	val = rtl_mdio_read(hw, MII_ADVERTISE);
	val &= ~(ADVERTISE_10HALF | ADVERTISE_10FULL | ADVERTISE_100HALF |
		 ADVERTISE_100FULL);
	rtl_mdio_write(hw, MII_ADVERTISE, val);

	val = rtl_mdio_read(hw, MII_CTRL1000);
	val &= ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL);
	rtl_mdio_write(hw, MII_CTRL1000, val);

	if (rtl_is_8125(hw)) {
		val = rtl_mdio_direct_read_phy_ocp(hw, 0xA5D4);
		val &= ~(RTK_ADVERTISE_2500FULL | RTK_ADVERTISE_5000FULL |
			 RTK_ADVERTISE_10000FULL);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA5D4, val);
	}

	rtl_mdio_write(hw, MII_BMCR, BMCR_RESET | BMCR_ANENABLE);

	if (rtl_wait_phy_reset_complete(hw))
		PMD_INIT_LOG(NOTICE, "PHY reset failed.");
}

static void
rtl8125_set_hw_phy_before_init_phy_mcu(struct rtl_hw *hw)
{
	u16 val;

	switch (hw->mcfg) {
	case CFG_METHOD_50:
		rtl_mdio_direct_write_phy_ocp(hw, 0xBF86, 0x9000);

		rtl_set_eth_phy_ocp_bit(hw, 0xC402, BIT_10);
		rtl_clear_eth_phy_ocp_bit(hw, 0xC402, BIT_10);

		val = rtl_mdio_direct_read_phy_ocp(hw, 0xBF86);
		val &= (BIT_1 | BIT_0);
		if (val != 0)
			PMD_INIT_LOG(NOTICE, "PHY watch dog not clear, value = 0x%x",
				     val);

		rtl_mdio_direct_write_phy_ocp(hw, 0xBD86, 0x1010);
		rtl_mdio_direct_write_phy_ocp(hw, 0xBD88, 0x1010);

		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBD4E, (BIT_11 | BIT_10), BIT_11);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBF46, (BIT_11 | BIT_10 | BIT_9 | BIT_8),
						  (BIT_10 | BIT_9 | BIT_8));
		break;
	}
}

static u16
rtl_get_hw_phy_mcu_code_ver(struct rtl_hw *hw)
{
	u16 hw_ram_code_ver = ~0;

	if (rtl_is_8125(hw)) {
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x801E);
		hw_ram_code_ver = rtl_mdio_direct_read_phy_ocp(hw, 0xA438);
	} else {
		rtl_mdio_write(hw, 0x1F, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x801E);
		hw_ram_code_ver = rtl_mdio_read(hw, 0x14);
		rtl_mdio_write(hw, 0x1F, 0x0000);
	}

	return hw_ram_code_ver;
}

static int
rtl_check_hw_phy_mcu_code_ver(struct rtl_hw *hw)
{
	int ram_code_ver_match = 0;

	hw->hw_ram_code_ver = rtl_get_hw_phy_mcu_code_ver(hw);

	if (hw->hw_ram_code_ver == hw->sw_ram_code_ver) {
		ram_code_ver_match = 1;
		hw->HwHasWrRamCodeToMicroP = TRUE;
	} else {
		hw->HwHasWrRamCodeToMicroP = FALSE;
	}

	return ram_code_ver_match;
}

static void
rtl_write_hw_phy_mcu_code_ver(struct rtl_hw *hw)
{
	if (rtl_is_8125(hw)) {
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x801E);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, hw->sw_ram_code_ver);
		hw->hw_ram_code_ver = hw->sw_ram_code_ver;
	} else {
		rtl_mdio_write(hw, 0x1F, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x801E);
		rtl_mdio_write(hw, 0x14, hw->sw_ram_code_ver);
		rtl_mdio_write(hw, 0x1F, 0x0000);
		hw->hw_ram_code_ver = hw->sw_ram_code_ver;
	}
}

static void
rtl_enable_phy_disable_mode(struct rtl_hw *hw)
{
	switch (hw->HwSuppCheckPhyDisableModeVer) {
	case 1:
		rtl_mac_ocp_write(hw, 0xDC20, rtl_mac_ocp_read(hw, 0xDC20) |
					      BIT_1);
		break;
	case 2:
	case 3:
		RTL_W8(hw, 0xF2, RTL_R8(hw, 0xF2) | BIT_5);
		break;
	}
}

static void
rtl_disable_phy_disable_mode(struct rtl_hw *hw)
{
	switch (hw->HwSuppCheckPhyDisableModeVer) {
	case 1:
		rtl_mac_ocp_write(hw, 0xDC20, rtl_mac_ocp_read(hw, 0xDC20) &
					      ~BIT_1);
		break;
	case 2:
	case 3:
		RTL_W8(hw, 0xF2, RTL_R8(hw, 0xF2) & ~BIT_5);
		break;
	}

	rte_delay_ms(1);
}

static int
rtl8168_phy_ram_code_check(struct rtl_hw *hw)
{
	u16 val;
	int retval = TRUE;

	if (hw->mcfg == CFG_METHOD_21) {
		rtl_mdio_write(hw, 0x1f, 0x0A40);
		val = rtl_mdio_read(hw, 0x10);
		val &= ~BIT_11;
		rtl_mdio_write(hw, 0x10, val);

		rtl_mdio_write(hw, 0x1f, 0x0A00);
		val = rtl_mdio_read(hw, 0x10);
		val &= ~(BIT_12 | BIT_13 | BIT_14 | BIT_15);
		rtl_mdio_write(hw, 0x10, val);

		rtl_mdio_write(hw, 0x1f, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x8010);
		val = rtl_mdio_read(hw, 0x14);
		val &= ~BIT_11;
		rtl_mdio_write(hw, 0x14, val);

		retval = rtl_set_phy_mcu_patch_request(hw);

		rtl_mdio_write(hw, 0x1f, 0x0A40);
		rtl_mdio_write(hw, 0x10, 0x0140);

		rtl_mdio_write(hw, 0x1f, 0x0A4A);
		val = rtl_mdio_read(hw, 0x13);
		val &= ~BIT_6;
		val |= BIT_7;
		rtl_mdio_write(hw, 0x13, val);

		rtl_mdio_write(hw, 0x1f, 0x0A44);
		val = rtl_mdio_read(hw, 0x14);
		val |= BIT_2;
		rtl_mdio_write(hw, 0x14, val);

		rtl_mdio_write(hw, 0x1f, 0x0A50);
		val = rtl_mdio_read(hw, 0x11);
		val |= (BIT_11 | BIT_12);
		rtl_mdio_write(hw, 0x11, val);

		retval = rtl_clear_phy_mcu_patch_request(hw);

		rtl_mdio_write(hw, 0x1f, 0x0A40);
		rtl_mdio_write(hw, 0x10, 0x1040);

		rtl_mdio_write(hw, 0x1f, 0x0A4A);
		val = rtl_mdio_read(hw, 0x13);
		val &= ~(BIT_6 | BIT_7);
		rtl_mdio_write(hw, 0x13, val);

		rtl_mdio_write(hw, 0x1f, 0x0A44);
		val = rtl_mdio_read(hw, 0x14);
		val &= ~BIT_2;
		rtl_mdio_write(hw, 0x14, val);

		rtl_mdio_write(hw, 0x1f, 0x0A50);
		val = rtl_mdio_read(hw, 0x11);
		val &= ~(BIT_11 | BIT_12);
		rtl_mdio_write(hw, 0x11, val);

		rtl_mdio_write(hw, 0x1f, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x8010);
		val = rtl_mdio_read(hw, 0x14);
		val |= BIT_11;
		rtl_mdio_write(hw, 0x14, val);

		retval = rtl_set_phy_mcu_patch_request(hw);

		rtl_mdio_write(hw, 0x1f, 0x0A20);
		val = rtl_mdio_read(hw, 0x13);
		if (val & BIT_11) {
			if (val & BIT_10)
				retval = FALSE;
		}

		retval = rtl_clear_phy_mcu_patch_request(hw);

		rte_delay_ms(2);
	}

	rtl_mdio_write(hw, 0x1F, 0x0000);

	return retval;
}

static void
rtl8168_set_phy_ram_code_check_fail_flag(struct rtl_hw *hw)
{
	u16 tmp_ushort;

	if (hw->mcfg == CFG_METHOD_21) {
		tmp_ushort = rtl_mac_ocp_read(hw, 0xD3C0);
		tmp_ushort |= BIT_0;
		rtl_mac_ocp_write(hw, 0xD3C0, tmp_ushort);
	}
}

static void
rtl_init_hw_phy_mcu(struct rtl_hw *hw)
{
	u8 require_disable_phy_disable_mode = FALSE;

	if (hw->NotWrRamCodeToMicroP)
		return;

	if (rtl_check_hw_phy_mcu_code_ver(hw))
		return;

	if (!rtl_is_8125(hw) && !rtl8168_phy_ram_code_check(hw)) {
		rtl8168_set_phy_ram_code_check_fail_flag(hw);
		return;
	}

	if (HW_SUPPORT_CHECK_PHY_DISABLE_MODE(hw) && rtl_is_in_phy_disable_mode(hw))
		require_disable_phy_disable_mode = TRUE;

	if (require_disable_phy_disable_mode)
		rtl_disable_phy_disable_mode(hw);

	hw->hw_ops.hw_phy_mcu_config(hw);

	if (require_disable_phy_disable_mode)
		rtl_enable_phy_disable_mode(hw);

	rtl_write_hw_phy_mcu_code_ver(hw);

	rtl_mdio_write(hw, 0x1F, 0x0000);

	hw->HwHasWrRamCodeToMicroP = TRUE;
}

static void
rtl_disable_aldps(struct rtl_hw *hw)
{
	u16 tmp_ushort;
	u32 timeout = 0;
	u32 wait_cnt = 200;

	if (rtl_is_8125(hw)) {
		tmp_ushort = rtl_mdio_real_direct_read_phy_ocp(hw, 0xA430);
		if (tmp_ushort & BIT_2) {
			rtl_clear_eth_phy_ocp_bit(hw, 0xA430, BIT_2);

			do {
				rte_delay_us(100);
				tmp_ushort = rtl_mac_ocp_read(hw, 0xE908);
				timeout++;
			} while (!(tmp_ushort & BIT_7) && timeout < wait_cnt);
		}
	} else {
		tmp_ushort = rtl_mdio_real_direct_read_phy_ocp(hw, 0xA430);
		if (tmp_ushort & BIT_2)
			rtl_clear_eth_phy_ocp_bit(hw, 0xA430, BIT_2);
	}
}

static bool
rtl_is_adv_eee_enabled(struct rtl_hw *hw)
{
	bool enabled = false;

	switch (hw->mcfg) {
	case CFG_METHOD_25:
	case CFG_METHOD_26:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
		rtl_mdio_write(hw, 0x1F, 0x0A43);
		if (rtl_mdio_read(hw, 0x10) & BIT_15)
			enabled = true;
		rtl_mdio_write(hw, 0x1F, 0x0000);
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_58:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		if (rtl_mdio_direct_read_phy_ocp(hw, 0xA430) & BIT_15)
			enabled = true;
		break;
	default:
		break;
	}

	return enabled;
}

static void
_rtl_disable_adv_eee(struct rtl_hw *hw)
{
	bool lock;
	u16 data;

	if (rtl_is_adv_eee_enabled(hw))
		lock = true;
	else
		lock = false;

	if (lock)
		rtl_set_phy_mcu_patch_request(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_25:
		rtl_eri_write(hw, 0x1EA, 1, 0x00, ERIAR_ExGMAC);

		rtl_mdio_write(hw, 0x1F, 0x0A42);
		data = rtl_mdio_read(hw, 0x16);
		data &= ~BIT_1;
		rtl_mdio_write(hw, 0x16, data);
		rtl_mdio_write(hw, 0x1F, 0x0000);
		break;
	case CFG_METHOD_26:
		data = rtl_mac_ocp_read(hw, 0xE052);
		data &= ~BIT_0;
		rtl_mac_ocp_write(hw, 0xE052, data);

		rtl_mdio_write(hw, 0x1F, 0x0A42);
		data = rtl_mdio_read(hw, 0x16);
		data &= ~BIT_1;
		rtl_mdio_write(hw, 0x16, data);
		rtl_mdio_write(hw, 0x1F, 0x0000);
		break;
	case CFG_METHOD_27:
	case CFG_METHOD_28:
		data = rtl_mac_ocp_read(hw, 0xE052);
		data &= ~BIT_0;
		rtl_mac_ocp_write(hw, 0xE052, data);
		break;
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
		data = rtl_mac_ocp_read(hw, 0xE052);
		data &= ~BIT_0;
		rtl_mac_ocp_write(hw, 0xE052, data);

		rtl_mdio_write(hw, 0x1F, 0x0A43);
		data = rtl_mdio_read(hw, 0x10) & ~(BIT_15);
		rtl_mdio_write(hw, 0x10, data);

		rtl_mdio_write(hw, 0x1F, 0x0A44);
		data = rtl_mdio_read(hw, 0x11) & ~(BIT_12 | BIT_13 | BIT_14);
		rtl_mdio_write(hw, 0x11, data);
		rtl_mdio_write(hw, 0x1F, 0x0000);
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_58:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		rtl_clear_mac_ocp_bit(hw, 0xE052, BIT_0);
		rtl_clear_eth_phy_ocp_bit(hw, 0xA442, (BIT_12 | BIT_13));
		rtl_clear_eth_phy_ocp_bit(hw, 0xA430, BIT_15);
		break;
	}

	if (lock)
		rtl_clear_phy_mcu_patch_request(hw);
}

static void
rtl_disable_adv_eee(struct rtl_hw *hw)
{
	if (hw->mcfg < CFG_METHOD_25 || hw->mcfg == CFG_METHOD_37)
		return;

	rtl_oob_mutex_lock(hw);

	_rtl_disable_adv_eee(hw);

	rtl_oob_mutex_unlock(hw);
}

static void
rtl_disable_eee(struct rtl_hw *hw)
{
	u16 data;
	u16 mask;
	u32 csi_tmp;

	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_23:
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_26:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
		csi_tmp = rtl_eri_read(hw, 0x1B0, 4, ERIAR_ExGMAC);
		csi_tmp &= ~(BIT_1 | BIT_0);
		rtl_eri_write(hw, 0x1B0, 4, csi_tmp, ERIAR_ExGMAC);
		rtl_mdio_write(hw, 0x1F, 0x0A43);
		data = rtl_mdio_read(hw, 0x11);
		if (hw->mcfg == CFG_METHOD_36)
			rtl_mdio_write(hw, 0x11, data | BIT_4);
		else
			rtl_mdio_write(hw, 0x11, data & ~BIT_4);
		rtl_mdio_write(hw, 0x1F, 0x0A5D);
		rtl_mdio_write(hw, 0x10, 0x0000);
		rtl_mdio_write(hw, 0x1F, 0x0000);
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_52:
		rtl_clear_mac_ocp_bit(hw, 0xE040, (BIT_1 | BIT_0));
		rtl_clear_mac_ocp_bit(hw, 0xEB62, (BIT_2 | BIT_1));

		rtl_clear_eth_phy_ocp_bit(hw, 0xA432, BIT_4);
		rtl_clear_eth_phy_ocp_bit(hw, 0xA5D0, (BIT_2 | BIT_1));
		rtl_clear_eth_phy_ocp_bit(hw, 0xA6D4, BIT_0);

		rtl_clear_eth_phy_ocp_bit(hw, 0xA6D8, BIT_4);
		rtl_clear_eth_phy_ocp_bit(hw, 0xA428, BIT_7);
		rtl_clear_eth_phy_ocp_bit(hw, 0xA4A2, BIT_9);
		break;
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_58:
		rtl_clear_mac_ocp_bit(hw, 0xE040, (BIT_1 | BIT_0));

		rtl_set_eth_phy_ocp_bit(hw, 0xA432, BIT_4);
		rtl_clear_eth_phy_ocp_bit(hw, 0xA5D0, (BIT_2 | BIT_1));
		rtl_clear_eth_phy_ocp_bit(hw, 0xA6D4, BIT_0);

		rtl_clear_eth_phy_ocp_bit(hw, 0xA6D8, BIT_4);
		rtl_clear_eth_phy_ocp_bit(hw, 0xA428, BIT_7);
		rtl_clear_eth_phy_ocp_bit(hw, 0xA4A2, BIT_9);
		break;
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		rtl_clear_mac_ocp_bit(hw, 0xE040, (BIT_1 | BIT_0));

		if (HW_SUPP_PHY_LINK_SPEED_10000M(hw))
			mask = MDIO_EEE_100TX | MDIO_EEE_1000T | MDIO_EEE_10GT;
		else
			mask = MDIO_EEE_100TX | MDIO_EEE_1000T;
		rtl_clear_eth_phy_ocp_bit(hw, 0xA5D0, mask);

		rtl_clear_eth_phy_ocp_bit(hw, 0xA6D4, MDIO_EEE_2_5GT | MDIO_EEE_5GT);

		rtl_clear_eth_phy_ocp_bit(hw, 0xA6D8, BIT_4);
		rtl_clear_eth_phy_ocp_bit(hw, 0xA428, BIT_7);
		rtl_clear_eth_phy_ocp_bit(hw, 0xA4A2, BIT_9);
		break;
	default:
		/* Not support EEE */
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
		rtl_mdio_write(hw, 0x1F, 0x0A42);
		rtl_clear_eth_phy_bit(hw, 0x14, BIT_7);
		rtl_mdio_write(hw, 0x1F, 0x0A4A);
		rtl_clear_eth_phy_bit(hw, 0x11, BIT_9);
		rtl_mdio_write(hw, 0x1F, 0x0000);
		break;
	}

	/* Advanced EEE */
	rtl_disable_adv_eee(hw);
}

void
rtl_hw_phy_config(struct rtl_hw *hw)
{
	rtl_xmii_reset_enable(hw);

	if (HW_DASH_SUPPORT_TYPE_3(hw) && hw->HwPkgDet == 0x06)
		return;

	rtl8125_set_hw_phy_before_init_phy_mcu(hw);

	rtl_init_hw_phy_mcu(hw);

	hw->hw_ops.hw_phy_config(hw);

	rtl_disable_aldps(hw);

	/* Legacy force mode (chap 22) */
	if (rtl_is_8125(hw))
		rtl_clear_eth_phy_ocp_bit(hw, 0xA5B4, BIT_15);

	if (HW_FIBER_MODE_ENABLED(hw))
		rtl8127_hw_fiber_phy_config(hw);

	rtl_mdio_write(hw, 0x1F, 0x0000);

	if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(hw))
		rtl_disable_eee(hw);
}

static void
rtl_phy_restart_nway(struct rtl_hw *hw)
{
	if (rtl_is_in_phy_disable_mode(hw))
		return;

	rtl_mdio_write(hw, 0x1F, 0x0000);
	if (rtl_is_8125(hw))
		rtl_mdio_write(hw, MII_BMCR, BMCR_ANENABLE | BMCR_ANRESTART);
	else
		rtl_mdio_write(hw, MII_BMCR, BMCR_RESET | BMCR_ANENABLE |
					     BMCR_ANRESTART);
}

static void
rtl_phy_setup_force_mode(struct rtl_hw *hw, u32 speed, u8 duplex)
{
	u16 bmcr_true_force = 0;

	if (rtl_is_in_phy_disable_mode(hw))
		return;

	if (speed == SPEED_10 && duplex == DUPLEX_HALF)
		bmcr_true_force = BMCR_SPEED10;
	else if (speed == SPEED_10 && duplex == DUPLEX_FULL)
		bmcr_true_force = BMCR_SPEED10 | BMCR_FULLDPLX;
	else if (speed == SPEED_100 && duplex == DUPLEX_HALF)
		bmcr_true_force = BMCR_SPEED100;
	else if (speed == SPEED_100 && duplex == DUPLEX_FULL)
		bmcr_true_force = BMCR_SPEED100 | BMCR_FULLDPLX;
	else
		return;

	rtl_mdio_write(hw, 0x1F, 0x0000);
	rtl_mdio_write(hw, MII_BMCR, bmcr_true_force);
}

static int
rtl_set_speed_xmii(struct rtl_hw *hw, u8 autoneg, u32 speed, u8 duplex, u64 adv)
{
	u16 mask = 0;
	int auto_nego = 0;
	int giga_ctrl = 0;
	int ctrl_2500 = 0;
	int rc = -EINVAL;

	/* Disable giga lite */
	switch (hw->mcfg) {
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
		rtl_mdio_write(hw, 0x1F, 0x0A42);
		rtl_clear_eth_phy_bit(hw, 0x14, BIT_9);
		rtl_mdio_write(hw, 0x1F, 0x0A40);
		rtl_mdio_write(hw, 0x1F, 0x0000);
		break;
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		rtl_mdio_write(hw, 0x1F, 0x0A42);
		rtl_clear_eth_phy_bit(hw, 0x14, BIT_9 | BIT_7);
		rtl_mdio_write(hw, 0x1F, 0x0A40);
		rtl_mdio_write(hw, 0x1F, 0x0000);
		break;
	case CFG_METHOD_91:
		mask |= BIT_2;
	/* Fall through */
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
		mask |= BIT_1;
	/* Fall through */
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_58:
		mask |= BIT_0;
		rtl_clear_eth_phy_ocp_bit(hw, 0xA428, BIT_9);
		rtl_clear_eth_phy_ocp_bit(hw, 0xA5EA, mask);
		break;
	}

	if (!rtl_is_speed_mode_valid(hw, speed)) {
		speed = hw->HwSuppMaxPhyLinkSpeed;
		duplex = DUPLEX_FULL;
		adv |= hw->advertising;
	}

	if (HW_FIBER_MODE_ENABLED(hw))
		goto set_speed;

	giga_ctrl = rtl_mdio_read(hw, MII_CTRL1000);
	giga_ctrl &= ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL);
	if (rtl_is_8125(hw)) {
		ctrl_2500 = rtl_mdio_direct_read_phy_ocp(hw, 0xA5D4);
		ctrl_2500 &= ~(RTK_ADVERTISE_2500FULL | RTK_ADVERTISE_5000FULL |
			       RTK_ADVERTISE_10000FULL);
	}

	if (autoneg == AUTONEG_ENABLE) {
		/* N-way force */
		auto_nego = rtl_mdio_read(hw, MII_ADVERTISE);
		auto_nego &= ~(ADVERTISE_10HALF | ADVERTISE_10FULL |
			       ADVERTISE_100HALF | ADVERTISE_100FULL |
			       ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);

		if (adv & ADVERTISE_10_HALF)
			auto_nego |= ADVERTISE_10HALF;
		if (adv & ADVERTISE_10_FULL)
			auto_nego |= ADVERTISE_10FULL;
		if (adv & ADVERTISE_100_HALF)
			auto_nego |= ADVERTISE_100HALF;
		if (adv & ADVERTISE_100_FULL)
			auto_nego |= ADVERTISE_100FULL;
		if (adv & ADVERTISE_1000_HALF)
			giga_ctrl |= ADVERTISE_1000HALF;
		if (adv & ADVERTISE_1000_FULL)
			giga_ctrl |= ADVERTISE_1000FULL;
		if (adv & ADVERTISE_2500_FULL)
			ctrl_2500 |= RTK_ADVERTISE_2500FULL;
		if (adv & ADVERTISE_5000_FULL)
			ctrl_2500 |= RTK_ADVERTISE_5000FULL;
		if (adv & ADVERTISE_10000_FULL)
			ctrl_2500 |= RTK_ADVERTISE_10000FULL;

		/* Flow control */
		if (hw->fcpause == rtl_fc_full)
			auto_nego |= ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;

		rtl_mdio_write(hw, 0x1f, 0x0000);
		rtl_mdio_write(hw, MII_ADVERTISE, auto_nego);
		rtl_mdio_write(hw, MII_CTRL1000, giga_ctrl);
		if (rtl_is_8125(hw))
			rtl_mdio_direct_write_phy_ocp(hw, 0xA5D4, ctrl_2500);
		rtl_phy_restart_nway(hw);
		rte_delay_ms(20);
	} else {
		/* True force */
		if (speed == SPEED_10 || speed == SPEED_100)
			rtl_phy_setup_force_mode(hw, speed, duplex);
		else
			goto out;
	}

set_speed:
	hw->autoneg = autoneg;
	hw->speed = speed;
	hw->duplex = duplex;
	hw->advertising = adv;

	if (HW_FIBER_MODE_ENABLED(hw))
		rtl8127_hw_fiber_phy_config(hw);

	rc = 0;
out:
	return rc;
}

int
rtl_set_speed(struct rtl_hw *hw)
{
	int ret;

	ret = rtl_set_speed_xmii(hw, hw->autoneg, hw->speed, hw->duplex,
				 hw->advertising);

	return ret;
}

void
rtl_clear_and_set_eth_phy_bit(struct rtl_hw *hw, u8 addr, u16 clearmask,
			      u16 setmask)
{
	u16 val;

	val = rtl_mdio_read(hw, addr);
	val &= ~clearmask;
	val |= setmask;
	rtl_mdio_write(hw, addr, val);
}

void
rtl_clear_eth_phy_bit(struct rtl_hw *hw, u8 addr, u16 mask)
{
	rtl_clear_and_set_eth_phy_bit(hw, addr, mask, 0);
}

void
rtl_set_eth_phy_bit(struct rtl_hw *hw, u8 addr, u16 mask)
{
	rtl_clear_and_set_eth_phy_bit(hw, addr, 0, mask);
}

void
rtl8127_clear_ephy_ext_addr(struct rtl_hw *hw)
{
	rtl8127_set_ephy_ext_addr(hw, 0x0000);
}
