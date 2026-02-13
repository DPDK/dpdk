/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_ethdev.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl8125d_mcu.h"

/*
 * For RTL8125D, CFG_METHOD_56,57
 * For RTL8168KD, CFG_METHOD_59
 * For RTL8125K, CFG_METHOD_61
 */

static void
hw_init_rxcfg_8125d(struct rtl_hw *hw)
{
	RTL_W32(hw, RxConfig, Rx_Fetch_Number_8 | Rx_Close_Multiple |
		RxCfg_pause_slot_en | (RX_DMA_BURST_256 << RxCfgDMAShift));
}

static void
hw_ephy_config_8125d(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_59:
	case CFG_METHOD_61:
		/* Nothing to do */
		break;
	}
}

static void
rtl_hw_phy_config_8125d_1(struct rtl_hw *hw)
{
	rtl_set_eth_phy_ocp_bit(hw, 0xA442, BIT_11);

	rtl_set_phy_mcu_patch_request(hw);

	rtl_set_eth_phy_ocp_bit(hw, 0xBF96, BIT_15);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBF94, 0x0007, 0x0005);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBF8E, 0x3C00, 0x2800);

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBCD8, 0xC000, 0x4000);
	rtl_set_eth_phy_ocp_bit(hw, 0xBCD8, BIT_15 | BIT_14);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBCD8, 0xC000, 0x4000);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBC80, 0x001F, 0x0004);
	rtl_set_eth_phy_ocp_bit(hw, 0xBC82, BIT_15 | BIT_14 | BIT_13);
	rtl_set_eth_phy_ocp_bit(hw, 0xBC82, BIT_12 | BIT_11 | BIT_10);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBC80, 0x001F, 0x0005);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBC82, 0x00E0, 0x0040);
	rtl_set_eth_phy_ocp_bit(hw, 0xBC82, BIT_4 | BIT_3 | BIT_2);
	rtl_clear_eth_phy_ocp_bit(hw, 0xBCD8, BIT_15 | BIT_14);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBCD8, 0xC000, 0x8000);
	rtl_clear_eth_phy_ocp_bit(hw, 0xBCD8, BIT_15 | BIT_14);

	rtl_clear_eth_phy_ocp_bit(hw, 0xBD70, BIT_8);
	rtl_set_eth_phy_ocp_bit(hw, 0xA466, BIT_1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x836a);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, 0xFF00);

	rtl_clear_phy_mcu_patch_request(hw);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x832C);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x0500);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB106, 0x0700, 0x0100);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB206, 0x0700, 0x0200);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB306, 0x0700, 0x0300);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x80CB);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x0300);
	rtl_mdio_direct_write_phy_ocp(hw, 0xBCF4, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xBCF6, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xBC12, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x844d);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x0200);
	if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(hw)) {
		rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8feb);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x0100);
		rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8fe9);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x0600);
	}

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAC7E, 0x01FC, 0x00B4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8105);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x7A00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8117);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x3A00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8103);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x7400);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8115);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x3400);

	rtl_clear_eth_phy_ocp_bit(hw, 0xAD40, BIT_5 | BIT_4);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD66, 0x000F, 0x0007);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD68, 0xF000, 0x8000);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD68, 0x0F00, 0x0500);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD68, 0x000F, 0x0002);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD6A, 0xF000, 0x7000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAC50, 0x01E8);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81FA);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x5400);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA864, 0x00F0, 0x00C0);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA42C, 0x00FF, 0x0002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80E1);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x0F00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80DE);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xF000, 0x0700);
	rtl_set_eth_phy_ocp_bit(hw, 0xA846, BIT_7);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80BA);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8A04);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80BD);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xCA00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80B7);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xB300);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80CE);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8A04);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80D1);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xCA00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80CB);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xBB00);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80A6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4909);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80A8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x05B8);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8200);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x5800);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8FF1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7078);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8FF3);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5D78);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8FF5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7862);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8FF7);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x1400);

	if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(hw)) {
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x814C);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8455);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x814E);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x84A6);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8163);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x0600);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x816A);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x0500);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8171);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x1f00);
	}

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBC3A, 0x000F, 0x0006);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8064);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_10 | BIT_9 | BIT_8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8067);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_10 | BIT_9 | BIT_8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x806A);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_10 | BIT_9 | BIT_8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x806D);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_10 | BIT_9 | BIT_8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8070);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_10 | BIT_9 | BIT_8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8073);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_10 | BIT_9 | BIT_8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8076);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_10 | BIT_9 | BIT_8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8079);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_10 | BIT_9 | BIT_8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x807C);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_10 | BIT_9 | BIT_8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x807F);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_10 | BIT_9 | BIT_8);

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBFA0, 0xFF70, 0x5500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xBFA2, 0x9D00);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8165);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0x0700, 0x0200);

	if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(hw)) {
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8019);
		rtl_set_eth_phy_ocp_bit(hw, 0xA438, BIT_8);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8FE3);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0005);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00ED);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0502);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0B00);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xD401);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x2900);
	}

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8018);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x1700);

	if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(hw)) {
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x815B);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x1700);
	}

	rtl_clear_eth_phy_ocp_bit(hw, 0xA4E0, BIT_15);

	rtl_clear_eth_phy_ocp_bit(hw, 0xA5D4, BIT_5);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA654, BIT_11);

	rtl_set_eth_phy_ocp_bit(hw, 0xA430, BIT_12 | BIT_0);
	rtl_set_eth_phy_ocp_bit(hw, 0xA442, BIT_7);
}

static void
rtl_hw_phy_config_8125d_2(struct rtl_hw *hw)
{
	rtl_set_eth_phy_ocp_bit(hw, 0xA442, BIT_11);

	rtl_set_phy_mcu_patch_request(hw);

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBCD8, 0xC000, 0x4000);
	rtl_set_eth_phy_ocp_bit(hw, 0xBCD8, BIT_15 | BIT_14);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBCD8, 0xC000, 0x4000);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBC80, 0x001F, 0x0004);
	rtl_set_eth_phy_ocp_bit(hw, 0xBC82, BIT_15 | BIT_14 | BIT_13);
	rtl_set_eth_phy_ocp_bit(hw, 0xBC82, BIT_12 | BIT_11 | BIT_10);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBC80, 0x001F, 0x0005);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBC82, 0x00E0, 0x0040);
	rtl_set_eth_phy_ocp_bit(hw, 0xBC82, BIT_4 | BIT_3 | BIT_2);
	rtl_clear_eth_phy_ocp_bit(hw, 0xBCD8, BIT_15 | BIT_14);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBCD8, 0xC000, 0x8000);
	rtl_clear_eth_phy_ocp_bit(hw, 0xBCD8, BIT_15 | BIT_14);

	rtl_clear_phy_mcu_patch_request(hw);

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAC7E, 0x01FC, 0x00B4);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8105);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x7A00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8117);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x3A00);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8103);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x7400);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8115);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x3400);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FEB);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x0500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FEA);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x0700);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x80D6);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0xEF00);

	rtl_clear_eth_phy_ocp_bit(hw, 0xA5D4, BIT_5);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA654, BIT_11);

	rtl_clear_eth_phy_ocp_bit(hw, 0xA448, BIT_10);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA586, BIT_10);

	rtl_clear_eth_phy_ocp_bit(hw, 0xA4E0, BIT_15);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8155);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x0200);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x815C);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x0200);

	rtl_set_eth_phy_ocp_bit(hw, 0xA430, BIT_12 | BIT_0);
	rtl_set_eth_phy_ocp_bit(hw, 0xA442, BIT_7);
}

static void
hw_phy_config_8125d(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_56:
		rtl_hw_phy_config_8125d_1(hw);
		break;
	case CFG_METHOD_57:
	case CFG_METHOD_59:
	case CFG_METHOD_61:
		rtl_hw_phy_config_8125d_2(hw);
		break;
	}
}

static void
hw_mac_mcu_config_8125d(struct rtl_hw *hw)
{
	if (hw->NotWrMcuPatchCode)
		return;

	rtl_hw_disable_mac_mcu_bps(hw);

	/* Get H/W mac mcu patch code version */
	hw->hw_mcu_patch_code_ver = rtl_get_hw_mcu_patch_code_ver(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_56:
		rtl_set_mac_mcu_8125d_1(hw);
		break;
	case CFG_METHOD_57:
	case CFG_METHOD_59:
		rtl_set_mac_mcu_8125d_2(hw);
		break;
	default:
		/* no mac mcu patch code */
		break;
	}
}

static void
hw_phy_mcu_config_8125d(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_56:
		rtl_set_phy_mcu_8125d_1(hw);
		break;
	case CFG_METHOD_57:
	case CFG_METHOD_59:
	case CFG_METHOD_61:
		rtl_set_phy_mcu_8125d_2(hw);
		break;
	}
}

const struct rtl_hw_ops rtl8125d_ops = {
	.hw_init_rxcfg     = hw_init_rxcfg_8125d,
	.hw_ephy_config    = hw_ephy_config_8125d,
	.hw_phy_config     = hw_phy_config_8125d,
	.hw_mac_mcu_config = hw_mac_mcu_config_8125d,
	.hw_phy_mcu_config = hw_phy_mcu_config_8125d,
};
