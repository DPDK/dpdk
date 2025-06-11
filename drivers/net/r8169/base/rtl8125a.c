/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_ethdev.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl8125a.h"

/* For RTL8125A, CFG_METHOD_48,49 */

static void
hw_init_rxcfg_8125a(struct rtl_hw *hw)
{
	RTL_W32(hw, RxConfig, Rx_Fetch_Number_8 | (RX_DMA_BURST_256 << RxCfgDMAShift));
}

static void
hw_ephy_config_8125a(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_48:
		rtl_ephy_write(hw, 0x01, 0xA812);
		rtl_ephy_write(hw, 0x09, 0x520C);
		rtl_ephy_write(hw, 0x04, 0xD000);
		rtl_ephy_write(hw, 0x0D, 0xF702);
		rtl_ephy_write(hw, 0x0A, 0x8653);
		rtl_ephy_write(hw, 0x06, 0x001E);
		rtl_ephy_write(hw, 0x08, 0x3595);
		rtl_ephy_write(hw, 0x20, 0x9455);
		rtl_ephy_write(hw, 0x21, 0x99FF);
		rtl_ephy_write(hw, 0x02, 0x6046);
		rtl_ephy_write(hw, 0x29, 0xFE00);
		rtl_ephy_write(hw, 0x23, 0xAB62);

		rtl_ephy_write(hw, 0x41, 0xA80C);
		rtl_ephy_write(hw, 0x49, 0x520C);
		rtl_ephy_write(hw, 0x44, 0xD000);
		rtl_ephy_write(hw, 0x4D, 0xF702);
		rtl_ephy_write(hw, 0x4A, 0x8653);
		rtl_ephy_write(hw, 0x46, 0x001E);
		rtl_ephy_write(hw, 0x48, 0x3595);
		rtl_ephy_write(hw, 0x60, 0x9455);
		rtl_ephy_write(hw, 0x61, 0x99FF);
		rtl_ephy_write(hw, 0x42, 0x6046);
		rtl_ephy_write(hw, 0x69, 0xFE00);
		rtl_ephy_write(hw, 0x63, 0xAB62);
		break;
	case CFG_METHOD_49:
		rtl_ephy_write(hw, 0x04, 0xD000);
		rtl_ephy_write(hw, 0x0A, 0x8653);
		rtl_ephy_write(hw, 0x23, 0xAB66);
		rtl_ephy_write(hw, 0x20, 0x9455);
		rtl_ephy_write(hw, 0x21, 0x99FF);
		rtl_ephy_write(hw, 0x29, 0xFE04);

		rtl_ephy_write(hw, 0x44, 0xD000);
		rtl_ephy_write(hw, 0x4A, 0x8653);
		rtl_ephy_write(hw, 0x63, 0xAB66);
		rtl_ephy_write(hw, 0x60, 0x9455);
		rtl_ephy_write(hw, 0x61, 0x99FF);
		rtl_ephy_write(hw, 0x69, 0xFE04);

		rtl_clear_and_set_pcie_phy_bit(hw, 0x2A, (BIT_14 | BIT_13 | BIT_12),
					       (BIT_13 | BIT_12));
		rtl_clear_pcie_phy_bit(hw, 0x19, BIT_6);
		rtl_set_pcie_phy_bit(hw, 0x1B, (BIT_11 | BIT_10 | BIT_9));
		rtl_clear_pcie_phy_bit(hw, 0x1B, (BIT_14 | BIT_13 | BIT_12));
		rtl_ephy_write(hw, 0x02, 0x6042);
		rtl_ephy_write(hw, 0x06, 0x0014);

		rtl_clear_and_set_pcie_phy_bit(hw, 0x6A, (BIT_14 | BIT_13 | BIT_12),
					       (BIT_13 | BIT_12));
		rtl_clear_pcie_phy_bit(hw, 0x59, BIT_6);
		rtl_set_pcie_phy_bit(hw, 0x5B, (BIT_11 | BIT_10 | BIT_9));
		rtl_clear_pcie_phy_bit(hw, 0x5B, (BIT_14 | BIT_13 | BIT_12));
		rtl_ephy_write(hw, 0x42, 0x6042);
		rtl_ephy_write(hw, 0x46, 0x0014);
		break;
	}
}

static void
rtl_hw_phy_config_8125a_1(struct rtl_hw *hw)
{
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD40, 0x03FF, 0x84);

	rtl_set_eth_phy_ocp_bit(hw, 0xAD4E, BIT_4);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD16, 0x03FF, 0x0006);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD32, 0x003F, 0x0006);
	rtl_clear_eth_phy_ocp_bit(hw, 0xAC08, BIT_12);
	rtl_clear_eth_phy_ocp_bit(hw, 0xAC08, BIT_8);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAC8A,
					  (BIT_15 | BIT_14 | BIT_13 | BIT_12),
					  (BIT_14 | BIT_13 | BIT_12));
	rtl_set_eth_phy_ocp_bit(hw, 0xAD18, BIT_10);
	rtl_set_eth_phy_ocp_bit(hw, 0xAD1A, 0x3FF);
	rtl_set_eth_phy_ocp_bit(hw, 0xAD1C, 0x3FF);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80EA);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xC400);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80EB);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0x0700, 0x0300);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80F8);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x1C00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80F1);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x3000);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80FE);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xA500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8102);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x5000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8105);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x3300);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8100);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x7000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8104);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xF000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8106);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x6500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80DC);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xED00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80DF);
	rtl_set_eth_phy_ocp_bit(hw, 0xA438, BIT_8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80E1);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_8);

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBF06, 0x003F, 0x38);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x819F);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xD0B6);

	rtl_mdio_direct_write_phy_ocp(hw, 0xBC34, 0x5555);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBF0A, (BIT_11 | BIT_10 | BIT_9),
					  (BIT_11 | BIT_9));

	rtl_clear_eth_phy_ocp_bit(hw, 0xA5C0, BIT_10);

	rtl_set_eth_phy_ocp_bit(hw, 0xA442, BIT_11);
}

void
rtl_hw_phy_config_8125a_2(struct rtl_hw *hw)
{
	u16 adccal_offset_p0;
	u16 adccal_offset_p1;
	u16 adccal_offset_p2;
	u16 adccal_offset_p3;
	u16 rg_lpf_cap_xg_p0;
	u16 rg_lpf_cap_xg_p1;
	u16 rg_lpf_cap_xg_p2;
	u16 rg_lpf_cap_xg_p3;
	u16 rg_lpf_cap_p0;
	u16 rg_lpf_cap_p1;
	u16 rg_lpf_cap_p2;
	u16 rg_lpf_cap_p3;

	rtl_set_eth_phy_ocp_bit(hw, 0xAD4E, BIT_4);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD16, 0x03FF, 0x03FF);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD32, 0x003F, 0x0006);
	rtl_clear_eth_phy_ocp_bit(hw, 0xAC08, BIT_12);
	rtl_clear_eth_phy_ocp_bit(hw, 0xAC08, BIT_8);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xACC0, (BIT_1 | BIT_0), BIT_1);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD40, (BIT_7 | BIT_6 | BIT_5),
					  BIT_6);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD40, (BIT_2 | BIT_1 | BIT_0),
					  BIT_2);
	rtl_clear_eth_phy_ocp_bit(hw, 0xAC14, BIT_7);
	rtl_clear_eth_phy_ocp_bit(hw, 0xAC80, BIT_9 | BIT_8);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAC5E, (BIT_2 | BIT_1 | BIT_0),
					  BIT_1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAD4C, 0x00A8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAC5C, 0x01FF);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAC8A, (BIT_7 | BIT_6 | BIT_5 | BIT_4),
					  (BIT_5 | BIT_4));
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8157);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x0500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8159);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x0700);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x80A2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0153);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x809C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0153);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81B3);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0043);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00A7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00D6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00EC);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00F6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00FB);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00FD);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00FF);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00BB);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0058);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0029);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0013);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0009);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0004);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8257);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x020F);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80EA);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7843);

	rtl_set_phy_mcu_patch_request(hw);

	rtl_clear_eth_phy_ocp_bit(hw, 0xB896, BIT_0);
	rtl_clear_eth_phy_ocp_bit(hw, 0xB892, 0xFF00);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC091);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x6E12);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC092);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x1214);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC094);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x1516);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC096);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x171B);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC098);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x1B1C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC09A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x1F1F);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC09C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x2021);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC09E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x2224);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC0A0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x2424);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC0A2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x2424);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC0A4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x2424);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC018);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x0AF2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC01A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x0D4A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC01C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x0F26);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC01E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x118D);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC020);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x14F3);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC022);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x175A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC024);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x19C0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC026);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x1C26);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC089);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x6050);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC08A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x5F6E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC08C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x6E6E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC08E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x6E6E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB88E, 0xC090);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB890, 0x6E12);

	rtl_set_eth_phy_ocp_bit(hw, 0xB896, BIT_0);

	rtl_clear_phy_mcu_patch_request(hw);

	rtl_set_eth_phy_ocp_bit(hw, 0xD068, BIT_13);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81A2);
	rtl_set_eth_phy_ocp_bit(hw, 0xA438, BIT_8);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB54C, 0xFF00, 0xDB00);

	rtl_clear_eth_phy_ocp_bit(hw, 0xA454, BIT_0);

	rtl_set_eth_phy_ocp_bit(hw, 0xA5D4, BIT_5);
	rtl_clear_eth_phy_ocp_bit(hw, 0xAD4E, BIT_4);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA86A, BIT_0);

	rtl_set_eth_phy_ocp_bit(hw, 0xA442, BIT_11);

	if (hw->RequirePhyMdiSwapPatch) {
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD068, 0x0007, 0x0001);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD068, 0x0018, 0x0000);
		adccal_offset_p0 = rtl_mdio_direct_read_phy_ocp(hw, 0xD06A);
		adccal_offset_p0 &= 0x07FF;
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD068, 0x0018, 0x0008);
		adccal_offset_p1 = rtl_mdio_direct_read_phy_ocp(hw, 0xD06A);
		adccal_offset_p1 &= 0x07FF;
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD068, 0x0018, 0x0010);
		adccal_offset_p2 = rtl_mdio_direct_read_phy_ocp(hw, 0xD06A);
		adccal_offset_p2 &= 0x07FF;
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD068, 0x0018, 0x0018);
		adccal_offset_p3 = rtl_mdio_direct_read_phy_ocp(hw, 0xD06A);
		adccal_offset_p3 &= 0x07FF;

		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD068, 0x0018, 0x0000);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD06A, 0x07FF, adccal_offset_p3);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD068, 0x0018, 0x0008);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD06A, 0x07FF, adccal_offset_p2);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD068, 0x0018, 0x0010);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD06A, 0x07FF, adccal_offset_p1);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD068, 0x0018, 0x0018);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xD06A, 0x07FF, adccal_offset_p0);

		rg_lpf_cap_xg_p0 = rtl_mdio_direct_read_phy_ocp(hw, 0xBD5A);
		rg_lpf_cap_xg_p0 &= 0x001F;
		rg_lpf_cap_xg_p1 = rtl_mdio_direct_read_phy_ocp(hw, 0xBD5A);
		rg_lpf_cap_xg_p1 &= 0x1F00;
		rg_lpf_cap_xg_p2 = rtl_mdio_direct_read_phy_ocp(hw, 0xBD5C);
		rg_lpf_cap_xg_p2 &= 0x001F;
		rg_lpf_cap_xg_p3 = rtl_mdio_direct_read_phy_ocp(hw, 0xBD5C);
		rg_lpf_cap_xg_p3 &= 0x1F00;
		rg_lpf_cap_p0 = rtl_mdio_direct_read_phy_ocp(hw, 0xBC18);
		rg_lpf_cap_p0 &= 0x001F;
		rg_lpf_cap_p1 = rtl_mdio_direct_read_phy_ocp(hw, 0xBC18);
		rg_lpf_cap_p1 &= 0x1F00;
		rg_lpf_cap_p2 = rtl_mdio_direct_read_phy_ocp(hw, 0xBC1A);
		rg_lpf_cap_p2 &= 0x001F;
		rg_lpf_cap_p3 = rtl_mdio_direct_read_phy_ocp(hw, 0xBC1A);
		rg_lpf_cap_p3 &= 0x1F00;

		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBD5A, 0x001F,
						  rg_lpf_cap_xg_p3 >> 8);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBD5A, 0x1F00,
						  rg_lpf_cap_xg_p2 << 8);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBD5C, 0x001F,
						  rg_lpf_cap_xg_p1 >> 8);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBD5C, 0x1F00,
						  rg_lpf_cap_xg_p0 << 8);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBC18, 0x001F, rg_lpf_cap_p3 >> 8);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBC18, 0x1F00, rg_lpf_cap_p2 << 8);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBC1A, 0x001F, rg_lpf_cap_p1 >> 8);
		rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBC1A, 0x1F00, rg_lpf_cap_p0 << 8);
	}

	rtl_set_eth_phy_ocp_bit(hw, 0xA424, BIT_3);
}

static void
hw_phy_config_8125a(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_48:
		rtl_hw_phy_config_8125a_1(hw);
		break;
	case CFG_METHOD_49:
		rtl_hw_phy_config_8125a_2(hw);
		break;
	}
}

static void
hw_mac_mcu_config_8125a(struct rtl_hw *hw)
{
	if (hw->NotWrMcuPatchCode)
		return;

	rtl_hw_disable_mac_mcu_bps(hw);

	/* Get H/W mac mcu patch code version */
	hw->hw_mcu_patch_code_ver = rtl_get_hw_mcu_patch_code_ver(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_49:
		rtl_set_mac_mcu_8125a_2(hw);
		break;
	}
}

static void
hw_phy_mcu_config_8125a(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_48:
		rtl_set_phy_mcu_8125a_1(hw);
		break;
	case CFG_METHOD_49:
		rtl_set_phy_mcu_8125a_2(hw);
		break;
	}
}

const struct rtl_hw_ops rtl8125a_ops = {
	.hw_init_rxcfg     = hw_init_rxcfg_8125a,
	.hw_ephy_config    = hw_ephy_config_8125a,
	.hw_phy_config     = hw_phy_config_8125a,
	.hw_mac_mcu_config = hw_mac_mcu_config_8125a,
	.hw_phy_mcu_config = hw_phy_mcu_config_8125a,
};
