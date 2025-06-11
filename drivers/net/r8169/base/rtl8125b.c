/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_ethdev.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl8125b.h"

/* For RTL8125B, CFG_METHOD_50,51 */

static void
hw_init_rxcfg_8125b(struct rtl_hw *hw)
{
	RTL_W32(hw, RxConfig, Rx_Fetch_Number_8 | RxCfg_pause_slot_en |
		(RX_DMA_BURST_256 << RxCfgDMAShift));
}

static void
hw_ephy_config_8125b(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_50:
		rtl_ephy_write(hw, 0x06, 0x001F);
		rtl_ephy_write(hw, 0x0A, 0xB66B);
		rtl_ephy_write(hw, 0x01, 0xA852);
		rtl_ephy_write(hw, 0x24, 0x0008);
		rtl_ephy_write(hw, 0x2F, 0x6052);
		rtl_ephy_write(hw, 0x0D, 0xF716);
		rtl_ephy_write(hw, 0x20, 0xD477);
		rtl_ephy_write(hw, 0x21, 0x4477);
		rtl_ephy_write(hw, 0x22, 0x0013);
		rtl_ephy_write(hw, 0x23, 0xBB66);
		rtl_ephy_write(hw, 0x0B, 0xA909);
		rtl_ephy_write(hw, 0x29, 0xFF04);
		rtl_ephy_write(hw, 0x1B, 0x1EA0);

		rtl_ephy_write(hw, 0x46, 0x001F);
		rtl_ephy_write(hw, 0x4A, 0xB66B);
		rtl_ephy_write(hw, 0x41, 0xA84A);
		rtl_ephy_write(hw, 0x64, 0x000C);
		rtl_ephy_write(hw, 0x6F, 0x604A);
		rtl_ephy_write(hw, 0x4D, 0xF716);
		rtl_ephy_write(hw, 0x60, 0xD477);
		rtl_ephy_write(hw, 0x61, 0x4477);
		rtl_ephy_write(hw, 0x62, 0x0013);
		rtl_ephy_write(hw, 0x63, 0xBB66);
		rtl_ephy_write(hw, 0x4B, 0xA909);
		rtl_ephy_write(hw, 0x69, 0xFF04);
		rtl_ephy_write(hw, 0x5B, 0x1EA0);
		break;
	case CFG_METHOD_51:
		rtl_ephy_write(hw, 0x0B, 0xA908);
		rtl_ephy_write(hw, 0x1E, 0x20EB);
		rtl_ephy_write(hw, 0x22, 0x0023);
		rtl_ephy_write(hw, 0x02, 0x60C2);
		rtl_ephy_write(hw, 0x29, 0xFF00);

		rtl_ephy_write(hw, 0x4B, 0xA908);
		rtl_ephy_write(hw, 0x5E, 0x28EB);
		rtl_ephy_write(hw, 0x62, 0x0023);
		rtl_ephy_write(hw, 0x42, 0x60C2);
		rtl_ephy_write(hw, 0x69, 0xFF00);
		break;
	}
}

static void
rtl_hw_phy_config_8125b_1(struct rtl_hw *hw)
{
	rtl_set_eth_phy_ocp_bit(hw, 0xA442, BIT_11);

	rtl_set_eth_phy_ocp_bit(hw, 0xBC08, (BIT_3 | BIT_2));

	if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(hw)) {
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8FFF);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x0400);
	}
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8560);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x19CC);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8562);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x19CC);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8564);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x19CC);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8566);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x147D);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8568);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x147D);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x856A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x147D);
	if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(hw)) {
		rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FFE);
		rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0907);
	}
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xACDA, 0xFF00, 0xFF00);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xACDE, 0xF000, 0xF000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x80D6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x2801);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x80F2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x2801);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x80F4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x6077);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB506, 0x01E7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAC8C, 0x0FFC);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAC46, 0xB7B4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAC50, 0x0FBC);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAC3C, 0x9240);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAC4E, 0x0DB4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xACC6, 0x0707);
	rtl_mdio_direct_write_phy_ocp(hw, 0xACC8, 0xA0D3);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAD08, 0x0007);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8013);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FB9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x2801);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FBA);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0100);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FBC);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x1900);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FBE);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xE100);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FC0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FC2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xE500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FC4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0F00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FC6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xF100);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FC8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0400);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FCa);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xF300);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FCc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xFD00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FCe);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xFF00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FD0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xFB00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FD2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0100);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FD4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xF400);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FD6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xFF00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FD8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xF600);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x813D);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x390E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x814F);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x790E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x80B0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0F31);
	rtl_set_eth_phy_ocp_bit(hw, 0xBF4C, BIT_1);
	rtl_set_eth_phy_ocp_bit(hw, 0xBCCA, (BIT_9 | BIT_8));
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8141);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x320E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8153);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x720E);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA432, BIT_6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8529);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x050E);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x816C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xC4A0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8170);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xC4A0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8174);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x04A0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8178);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x04A0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x817C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0719);
	if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(hw)) {
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8FF4);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0400);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8FF1);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0404);
	}
	rtl_mdio_direct_write_phy_ocp(hw, 0xBF4A, 0x001B);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8033);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x7C13);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8037);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x7C13);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x803B);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xFC32);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x803F);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x7C13);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8043);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x7C13);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8047);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x7C13);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8145);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x370E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8157);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x770E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8169);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0D0A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x817B);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x1D0A);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8217);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x5000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x821A);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x5000);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80DA);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0403);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80DC);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80B3);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0384);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80B7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2007);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80BA);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x6C00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80B5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xF009);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80BD);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x9F00);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80C7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf083);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80DD);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x03f0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80DF);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80CB);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2007);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80CE);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x6C00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80C9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8009);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80D1);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x8000);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80A3);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x200A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80A5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xF0AD);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x809F);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x6073);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80A1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x000B);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80A9);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xC000);

	rtl_set_phy_mcu_patch_request(hw);

	rtl_clear_eth_phy_ocp_bit(hw, 0xB896, BIT_0);
	rtl_clear_eth_phy_ocp_bit(hw, 0xB892, 0xFF00);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC23E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC240);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x0103);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC242);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x0507);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC244);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x090B);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC246);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x0C0E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC248);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x1012);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC24A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x1416);

	rtl_set_eth_phy_ocp_bit(hw, 0xB896, BIT_0);

	rtl_clear_phy_mcu_patch_request(hw);

	rtl_set_eth_phy_ocp_bit(hw, 0xA86A, BIT_0);
	rtl_set_eth_phy_ocp_bit(hw, 0xA6F0, BIT_0);

	rtl_mdio_direct_write_phy_ocp(hw, 0xBFA0, 0xD70D);
	rtl_mdio_direct_write_phy_ocp(hw, 0xBFA2, 0x4100);
	rtl_mdio_direct_write_phy_ocp(hw, 0xBFA4, 0xE868);
	rtl_mdio_direct_write_phy_ocp(hw, 0xBFA6, 0xDC59);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB54C, 0x3C18);
	rtl_clear_eth_phy_ocp_bit(hw, 0xBFA4, BIT_5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x817D);
	rtl_set_eth_phy_ocp_bit(hw, 0xA438, BIT_12);
}

void
rtl_hw_phy_config_8125b_2(struct rtl_hw *hw)
{
	rtl_set_eth_phy_ocp_bit(hw, 0xA442, BIT_11);

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAC46, 0x00F0, 0x0090);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD30, 0x0003, 0x0001);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x80F5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x760E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8107);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x360E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8551);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E,
					  (BIT_15 | BIT_14 | BIT_13 | BIT_12 |
					  BIT_11 | BIT_10 | BIT_9 | BIT_8),
					  BIT_11);

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xbf00, 0xE000, 0xA000);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xbf46, 0x0F00, 0x0300);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa436, 0x8044);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa438, 0x2417);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa436, 0x804A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa438, 0x2417);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa436, 0x8050);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa438, 0x2417);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa436, 0x8056);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa438, 0x2417);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa436, 0x805C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa438, 0x2417);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa436, 0x8062);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa438, 0x2417);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa436, 0x8068);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa438, 0x2417);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa436, 0x806E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa438, 0x2417);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa436, 0x8074);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa438, 0x2417);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa436, 0x807A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xa438, 0x2417);

	rtl_set_eth_phy_ocp_bit(hw, 0xA4CA, BIT_6);

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBF84, (BIT_15 | BIT_14 | BIT_13),
					  (BIT_15 | BIT_13));

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8170);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438,
					  (BIT_13 | BIT_10 | BIT_9 | BIT_8),
					  (BIT_15 | BIT_14 | BIT_12 | BIT_11));

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8015);
	rtl_set_eth_phy_ocp_bit(hw, 0xB87E, BIT_8);
	rtl_mdio_direct_read_phy_ocp(hw, 0xB906);

	rtl_set_eth_phy_ocp_bit(hw, 0xA424, BIT_3);
}

static void
hw_phy_config_8125b(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_50:
		rtl_hw_phy_config_8125b_1(hw);
		break;
	case CFG_METHOD_51:
		rtl_hw_phy_config_8125b_2(hw);
		break;
	}
}

static void
hw_mac_mcu_config_8125b(struct rtl_hw *hw)
{
	if (hw->NotWrMcuPatchCode)
		return;

	rtl_hw_disable_mac_mcu_bps(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_51:
		rtl_set_mac_mcu_8125b_2(hw);
		break;
	}
}

static void
hw_phy_mcu_config_8125b(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_50:
		rtl_set_phy_mcu_8125b_1(hw);
		break;
	case CFG_METHOD_51:
		rtl_set_phy_mcu_8125b_2(hw);
		break;
	}
}

const struct rtl_hw_ops rtl8125b_ops = {
	.hw_init_rxcfg     = hw_init_rxcfg_8125b,
	.hw_ephy_config    = hw_ephy_config_8125b,
	.hw_phy_config     = hw_phy_config_8125b,
	.hw_mac_mcu_config = hw_mac_mcu_config_8125b,
	.hw_phy_mcu_config = hw_phy_mcu_config_8125b,
};
