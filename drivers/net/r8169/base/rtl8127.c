/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_ethdev.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl8127_mcu.h"

/* For RTL8127, CFG_METHOD_91 */

static void
hw_init_rxcfg_8127(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_91:
		RTL_W32(hw, RxConfig, Rx_Fetch_Number_8 | Rx_Close_Multiple |
			RxCfg_pause_slot_en | (RX_DMA_BURST_512 << RxCfgDMAShift));
		break;
	}
}

static void
hw_ephy_config_8127(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_91:
		rtl_ephy_write(hw, 0x8088, 0x0064);
		rtl_ephy_write(hw, 0x8488, 0x0064);
		rtl_ephy_write(hw, 0x8888, 0x0064);
		rtl_ephy_write(hw, 0x8C88, 0x0064);
		rtl_ephy_write(hw, 0x8188, 0x0064);
		rtl_ephy_write(hw, 0x8588, 0x0064);
		rtl_ephy_write(hw, 0x8988, 0x0064);
		rtl_ephy_write(hw, 0x8D88, 0x0064);
		rtl_ephy_write(hw, 0x808C, 0x09B0);
		rtl_ephy_write(hw, 0x848C, 0x09B0);
		rtl_ephy_write(hw, 0x888C, 0x0F90);
		rtl_ephy_write(hw, 0x8C8C, 0x0F90);
		rtl_ephy_write(hw, 0x818C, 0x09B0);
		rtl_ephy_write(hw, 0x858C, 0x09B0);
		rtl_ephy_write(hw, 0x898C, 0x0F90);
		rtl_ephy_write(hw, 0x8D8C, 0x0F90);
		rtl_ephy_write(hw, 0x808A, 0x09B8);
		rtl_ephy_write(hw, 0x848A, 0x09B8);
		rtl_ephy_write(hw, 0x888A, 0x0F98);
		rtl_ephy_write(hw, 0x8C8A, 0x0F98);
		rtl_ephy_write(hw, 0x818A, 0x09B8);
		rtl_ephy_write(hw, 0x858A, 0x09B8);
		rtl_ephy_write(hw, 0x898A, 0x0F98);
		rtl_ephy_write(hw, 0x8D8A, 0x0F98);
		rtl_ephy_write(hw, 0x9020, 0x0080);
		rtl_ephy_write(hw, 0x9420, 0x0080);
		rtl_ephy_write(hw, 0x9820, 0x0080);
		rtl_ephy_write(hw, 0x9C20, 0x0080);
		rtl_ephy_write(hw, 0x901E, 0x0190);
		rtl_ephy_write(hw, 0x941E, 0x0190);
		rtl_ephy_write(hw, 0x981E, 0x0140);
		rtl_ephy_write(hw, 0x9C1E, 0x0140);
		rtl_ephy_write(hw, 0x901C, 0x0190);
		rtl_ephy_write(hw, 0x941C, 0x0190);
		rtl_ephy_write(hw, 0x981C, 0x0140);
		rtl_ephy_write(hw, 0x9C1C, 0x0140);

		/* Clear extended address */
		rtl8127_clear_ephy_ext_addr(hw);
		break;
	default:
		/* nothing to do */
		break;
	}
}

static void
rtl8127_tgphy_irq_mask_and_ack(struct rtl_hw *hw)
{
	rtl_mdio_direct_write_phy_ocp(hw, 0xA4D2, 0x0000);
	(void)rtl_mdio_direct_read_phy_ocp(hw, 0xA4D4);
}

static void
rtl_hw_phy_config_8127a_1(struct rtl_hw *hw)
{
	rtl8127_tgphy_irq_mask_and_ack(hw);

	rtl_clear_eth_phy_ocp_bit(hw, 0xA442, BIT_11);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8415);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x9300);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81A3);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x0F00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81AE);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x0F00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81B9);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xB900);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x83B0);
	rtl_clear_eth_phy_ocp_bit(hw, 0xB87E, 0x0E00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x83C5);
	rtl_clear_eth_phy_ocp_bit(hw, 0xB87E, 0x0E00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x83DA);
	rtl_clear_eth_phy_ocp_bit(hw, 0xB87E, 0x0E00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x83EF);
	rtl_clear_eth_phy_ocp_bit(hw, 0xB87E, 0x0E00);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8173);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8620);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8175);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8671);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x817C);
	rtl_set_eth_phy_ocp_bit(hw, 0xA438, BIT_13);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8187);
	rtl_set_eth_phy_ocp_bit(hw, 0xA438, BIT_13);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8192);
	rtl_set_eth_phy_ocp_bit(hw, 0xA438, BIT_13);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x819D);
	rtl_set_eth_phy_ocp_bit(hw, 0xA438, BIT_13);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81A8);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_13);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81B3);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_13);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81BE);
	rtl_set_eth_phy_ocp_bit(hw, 0xA438, BIT_13);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x817D);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xA600);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8188);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xA600);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8193);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xA600);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x819E);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xA600);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81A9);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x1400);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81B4);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x1400);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81BF);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0xA600);

	rtl_clear_eth_phy_ocp_bit(hw, 0xAEAA, BIT_5 | BIT_3);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x84F0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x201C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x84F2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x3117);

	rtl_mdio_direct_write_phy_ocp(hw, 0xAEC6, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAE20, 0xFFFF);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAECE, 0xFFFF);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAED2, 0xFFFF);
	rtl_mdio_direct_write_phy_ocp(hw, 0xAEC8, 0x0000);
	rtl_clear_eth_phy_ocp_bit(hw, 0xAED0, BIT_0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xADB8, 0x0150);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8197);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x5000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8231);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x5000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x82CB);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x5000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x82CD);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x5700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8233);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x5700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8199);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x5700);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x815A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0150);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x81F4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0150);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x828E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0150);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x81B1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x824B);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x82E5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0000);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x84F7);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x2800);
	rtl_set_eth_phy_ocp_bit(hw, 0xAEC2, BIT_12);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x81B3);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0xAD00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x824D);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0xAD00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x82E7);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0xAD00);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAE4E, 0x000F, 0x0001);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x82CE);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xF000, 0x4000);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x84AC);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x84AE);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x84B0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xF818);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x84B2);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x6000);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FFC);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x6008);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FFE);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xF450);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8015);
	rtl_set_eth_phy_ocp_bit(hw, 0xB87E, BIT_9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8016);
	rtl_set_eth_phy_ocp_bit(hw, 0xB87E, BIT_11);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FE6);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x0800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FE4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x2114);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8647);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xA7B1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8649);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xBBCA);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x864B);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0xDC00);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8154);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xC000, 0x4000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8158);
	rtl_clear_eth_phy_ocp_bit(hw, 0xB87E, 0xC000);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x826C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xFFFF);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x826E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xFFFF);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8872);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x0E00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8012);
	rtl_set_eth_phy_ocp_bit(hw, 0xA438, BIT_11);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8012);
	rtl_set_eth_phy_ocp_bit(hw, 0xA438, BIT_14);
	rtl_set_eth_phy_ocp_bit(hw, 0xB576, BIT_0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x834A);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x0700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8217);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0x3F00, 0x2A00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81B1);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0xFF00, 0x0B00);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8370);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8671);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8372);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86C8);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8401);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86C8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8403);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86DA);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8406);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0x1800, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8408);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0x1800, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x840A);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0x1800, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x840C);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0x1800, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x840E);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0x1800, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8410);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0x1800, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8412);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0x1800, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8414);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0x1800, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8416);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, 0x1800, 0x1000);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x82BD);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1F40);

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xBFB4, 0x07FF, 0x0328);
	rtl_mdio_direct_write_phy_ocp(hw, 0xBFB6, 0x3E14);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x81C4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x003B);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0086);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00B7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00DB);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00FE);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00FE);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00FE);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00FE);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00C3);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0078);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0047);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0023);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x88D7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x01A0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x88D9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x01A0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FFA);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x002A);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FEE);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xFFDF);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FF0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xFFDF);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FF1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0xDF0A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FF3);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x4AAA);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FF5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x5A0A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FF7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87E, 0x4AAA);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8FF9);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x5A00);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x88D5);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x0200);

	rtl_set_eth_phy_ocp_bit(hw, 0xA430, BIT_1 | BIT_0);
}

static void
hw_phy_config_8127(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_91:
		rtl_hw_phy_config_8127a_1(hw);
		break;
	}
}

static void
hw_mac_mcu_config_8127(struct rtl_hw *hw)
{
	if (hw->NotWrMcuPatchCode)
		return;

	rtl_hw_disable_mac_mcu_bps(hw);

	/* Get H/W mac mcu patch code version */
	hw->hw_mcu_patch_code_ver = rtl_get_hw_mcu_patch_code_ver(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_91:
		rtl_set_mac_mcu_8127a_1(hw);
		break;
	}
}

static void
hw_phy_mcu_config_8127(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_91:
		rtl_set_phy_mcu_8127a_1(hw);
		break;
	}
}

const struct rtl_hw_ops rtl8127_ops = {
	.hw_init_rxcfg     = hw_init_rxcfg_8127,
	.hw_ephy_config    = hw_ephy_config_8127,
	.hw_phy_config     = hw_phy_config_8127,
	.hw_mac_mcu_config = hw_mac_mcu_config_8127,
	.hw_phy_mcu_config = hw_phy_mcu_config_8127,
};
