/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_compat.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl8168h.h"

/* For RTL8168H, CFG_METHOD_29,30,35,36 */

void
hw_init_rxcfg_8168h(struct rtl_hw *hw)
{
	RTL_W32(hw, RxConfig, Rx_Single_fetch_V2 |
		(RX_DMA_BURST_unlimited << RxCfgDMAShift) | RxEarly_off_V2);
}

void
hw_ephy_config_8168h(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_37:
		rtl_clear_pcie_phy_bit(hw, 0x1E, BIT_11);

		rtl_set_pcie_phy_bit(hw, 0x1E, BIT_0);
		rtl_set_pcie_phy_bit(hw, 0x1D, BIT_11);

		rtl_ephy_write(hw, 0x05, 0x2089);
		rtl_ephy_write(hw, 0x06, 0x5881);

		rtl_ephy_write(hw, 0x04, 0x854A);
		rtl_ephy_write(hw, 0x01, 0x068B);

		break;
	case CFG_METHOD_35:
		rtl8168_clear_mcu_ocp_bit(hw, 0xD438, BIT_2);

		rtl_clear_pcie_phy_bit(hw, 0x24, BIT_9);

		rtl8168_clear_mcu_ocp_bit(hw, 0xDE28, (BIT_1 | BIT_0));

		rtl8168_set_mcu_ocp_bit(hw, 0xD438, BIT_2);

		break;
	case CFG_METHOD_36:
		rtl8168_clear_mcu_ocp_bit(hw, 0xD438, BIT_2);

		rtl8168_clear_mcu_ocp_bit(hw, 0xDE28, (BIT_1 | BIT_0));

		rtl8168_set_mcu_ocp_bit(hw, 0xD438, BIT_2);

		break;
	default:
		break;
	}
}

static int
rtl8168h_require_adc_bias_patch_check(struct rtl_hw *hw, u16 *offset)
{
	int ret;
	u16 ioffset_p3, ioffset_p2, ioffset_p1, ioffset_p0;
	u16 tmp_ushort;

	rtl_mac_ocp_write(hw, 0xDD02, 0x807D);
	tmp_ushort = rtl_mac_ocp_read(hw, 0xDD02);
	ioffset_p3 = ((tmp_ushort & BIT_7) >> 7);
	ioffset_p3 <<= 3;
	tmp_ushort = rtl_mac_ocp_read(hw, 0xDD00);

	ioffset_p3 |= ((tmp_ushort & (BIT_15 | BIT_14 | BIT_13)) >> 13);

	ioffset_p2 = ((tmp_ushort & (BIT_12 | BIT_11 | BIT_10 | BIT_9)) >> 9);
	ioffset_p1 = ((tmp_ushort & (BIT_8 | BIT_7 | BIT_6 | BIT_5)) >> 5);

	ioffset_p0 = ((tmp_ushort & BIT_4) >> 4);
	ioffset_p0 <<= 3;
	ioffset_p0 |= (tmp_ushort & (BIT_2 | BIT_1 | BIT_0));

	if (ioffset_p3 == 0x0F && ioffset_p2 == 0x0F && ioffset_p1 == 0x0F &&
	    ioffset_p0 == 0x0F) {
		ret = FALSE;
	} else {
		ret = TRUE;
		*offset = (ioffset_p3 << 12) | (ioffset_p2 << 8) |
			   (ioffset_p1 << 4) | ioffset_p0;
	}

	return ret;
}

static void
hw_phy_config_8168h_1(struct rtl_hw *hw)
{
	u16 dout_tapbin;
	u16 gphy_val;

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x809b);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xF800, 0x8000);
	rtl_mdio_write(hw, 0x13, 0x80A2);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x8000);
	rtl_mdio_write(hw, 0x13, 0x80A4);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x8500);
	rtl_mdio_write(hw, 0x13, 0x809C);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0xbd00);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x80AD);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xF800, 0x7000);
	rtl_mdio_write(hw, 0x13, 0x80B4);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x5000);
	rtl_mdio_write(hw, 0x13, 0x80AC);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x4000);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x808E);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x1200);
	rtl_mdio_write(hw, 0x13, 0x8090);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0xE500);
	rtl_mdio_write(hw, 0x13, 0x8092);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x9F00);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(hw)) {
		dout_tapbin = 0x0000;
		rtl_mdio_write(hw, 0x1F, 0x0A46);
		gphy_val = rtl_mdio_read(hw, 0x13);
		gphy_val &= (BIT_1 | BIT_0);
		gphy_val <<= 2;
		dout_tapbin |= gphy_val;

		gphy_val = rtl_mdio_read(hw, 0x12);
		gphy_val &= (BIT_15 | BIT_14);
		gphy_val >>= 14;
		dout_tapbin |= gphy_val;

		dout_tapbin = ~(dout_tapbin ^ BIT_3);
		dout_tapbin <<= 12;
		dout_tapbin &= 0xF000;

		rtl_mdio_write(hw, 0x1F, 0x0A43);

		rtl_mdio_write(hw, 0x13, 0x827A);
		rtl_clear_and_set_eth_phy_bit(hw, 0x14,
					      (BIT_15 | BIT_14 | BIT_13 | BIT_12),
					      dout_tapbin);

		rtl_mdio_write(hw, 0x13, 0x827B);
		rtl_clear_and_set_eth_phy_bit(hw, 0x14,
					      (BIT_15 | BIT_14 | BIT_13 | BIT_12),
					      dout_tapbin);

		rtl_mdio_write(hw, 0x13, 0x827C);
		rtl_clear_and_set_eth_phy_bit(hw, 0x14,
					      (BIT_15 | BIT_14 | BIT_13 | BIT_12),
					      dout_tapbin);

		rtl_mdio_write(hw, 0x13, 0x827D);
		rtl_clear_and_set_eth_phy_bit(hw, 0x14,
					      (BIT_15 | BIT_14 | BIT_13 | BIT_12),
					      dout_tapbin);

		rtl_mdio_write(hw, 0x1F, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x8011);
		rtl_set_eth_phy_bit(hw, 0x14, BIT_11);
		rtl_mdio_write(hw, 0x1F, 0x0A42);
		rtl_set_eth_phy_bit(hw, 0x16, BIT_1);
	}

	rtl_mdio_write(hw, 0x1F, 0x0A44);
	rtl_set_eth_phy_bit(hw, 0x11, BIT_11);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0BCA);
	rtl_clear_and_set_eth_phy_bit(hw, 0x17, (BIT_13 | BIT_12), BIT_14);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x803F);
	rtl_clear_eth_phy_bit(hw, 0x14, (BIT_13 | BIT_12));
	rtl_mdio_write(hw, 0x13, 0x8047);
	rtl_clear_eth_phy_bit(hw, 0x14, (BIT_13 | BIT_12));
	rtl_mdio_write(hw, 0x13, 0x804F);
	rtl_clear_eth_phy_bit(hw, 0x14, (BIT_13 | BIT_12));
	rtl_mdio_write(hw, 0x13, 0x8057);
	rtl_clear_eth_phy_bit(hw, 0x14, (BIT_13 | BIT_12));
	rtl_mdio_write(hw, 0x13, 0x805F);
	rtl_clear_eth_phy_bit(hw, 0x14, (BIT_13 | BIT_12));
	rtl_mdio_write(hw, 0x13, 0x8067);
	rtl_clear_eth_phy_bit(hw, 0x14, (BIT_13 | BIT_12));
	rtl_mdio_write(hw, 0x13, 0x806F);
	rtl_clear_eth_phy_bit(hw, 0x14, (BIT_13 | BIT_12));
	rtl_mdio_write(hw, 0x1F, 0x0000);
}

static void
hw_phy_config_8168h_2(struct rtl_hw *hw)
{
	u16 gphy_val;
	u16 offset;
	u16 rlen;

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x808A);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14,
				      (BIT_5 | BIT_4 | BIT_3 | BIT_2 | BIT_1 | BIT_0),
				      0x0A);

	if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(hw)) {
		rtl_mdio_write(hw, 0x1F, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x8011);
		rtl_set_eth_phy_bit(hw, 0x14, BIT_11);
		rtl_mdio_write(hw, 0x1F, 0x0A42);
		rtl_set_eth_phy_bit(hw, 0x16, BIT_1);
	}

	rtl_mdio_write(hw, 0x1F, 0x0A44);
	rtl_set_eth_phy_bit(hw, 0x11, BIT_11);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	if (rtl8168h_require_adc_bias_patch_check(hw, &offset)) {
		rtl_mdio_write(hw, 0x1F, 0x0BCF);
		rtl_mdio_write(hw, 0x16, offset);
		rtl_mdio_write(hw, 0x1F, 0x0000);
	}

	rtl_mdio_write(hw, 0x1F, 0x0BCD);
	gphy_val = rtl_mdio_read(hw, 0x16);
	gphy_val &= 0x000F;

	if (gphy_val > 3)
		rlen = gphy_val - 3;
	else
		rlen = 0;

	gphy_val = rlen | (rlen << 4) | (rlen << 8) | (rlen << 12);

	rtl_mdio_write(hw, 0x1F, 0x0BCD);
	rtl_mdio_write(hw, 0x17, gphy_val);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	if (HW_HAS_WRITE_PHY_MCU_RAM_CODE(hw)) {
		rtl_mdio_write(hw, 0x1F, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x85FE);
		rtl_clear_and_set_eth_phy_bit(hw, 0x14, (BIT_15 | BIT_14 |
					      BIT_13 | BIT_12 | BIT_11 | BIT_10 | BIT_8),
					      BIT_9);
		rtl_mdio_write(hw, 0x13, 0x85FF);
		rtl_clear_and_set_eth_phy_bit(hw, 0x14,
					      (BIT_15 | BIT_14 | BIT_13 | BIT_12),
					      (BIT_11 | BIT_10 | BIT_9 | BIT_8));
		rtl_mdio_write(hw, 0x13, 0x814B);
		rtl_clear_and_set_eth_phy_bit(hw, 0x14,
					      (BIT_15 | BIT_14 | BIT_13 |
					      BIT_11 | BIT_10 | BIT_9 | BIT_8),
					      BIT_12);
	}

	rtl_mdio_write(hw, 0x1F, 0x0C41);
	rtl_clear_eth_phy_bit(hw, 0x15, BIT_1);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_set_eth_phy_bit(hw, 0x10, BIT_0);
	rtl_mdio_write(hw, 0x1F, 0x0000);
}

static void
hw_phy_config_8168h_3(struct rtl_hw *hw)
{
	rtl_mdio_write(hw, 0x1F, 0x0A44);
	rtl_set_eth_phy_bit(hw, 0x11, BIT_11);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0A4C);
	rtl_clear_eth_phy_bit(hw, 0x15, (BIT_14 | BIT_13));
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x81B9);
	rtl_mdio_write(hw, 0x14, 0x2000);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x81D4);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x6600);
	rtl_mdio_write(hw, 0x13, 0x81CB);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x3500);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0A80);
	rtl_clear_and_set_eth_phy_bit(hw, 0x16, 0x000F, 0x0005);
	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x8016);
	rtl_set_eth_phy_bit(hw, 0x14, BIT_13);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x811E);
	rtl_mdio_write(hw, 0x14, 0xDECA);

	rtl_mdio_write(hw, 0x13, 0x811C);
	rtl_mdio_write(hw, 0x14, 0x8008);
	rtl_mdio_write(hw, 0x13, 0x8118);
	rtl_mdio_write(hw, 0x14, 0xF8B4);
	rtl_mdio_write(hw, 0x13, 0x811A);
	rtl_mdio_write(hw, 0x14, 0x1A04);

	rtl_mdio_write(hw, 0x13, 0x8134);
	rtl_mdio_write(hw, 0x14, 0xDECA);
	rtl_mdio_write(hw, 0x13, 0x8132);
	rtl_mdio_write(hw, 0x14, 0xA008);
	rtl_mdio_write(hw, 0x13, 0x812E);
	rtl_mdio_write(hw, 0x14, 0x00B5);
	rtl_mdio_write(hw, 0x13, 0x8130);
	rtl_mdio_write(hw, 0x14, 0x1A04);

	rtl_mdio_write(hw, 0x13, 0x8112);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x7300);
	rtl_mdio_write(hw, 0x13, 0x8106);
	rtl_mdio_write(hw, 0x14, 0xA209);
	rtl_mdio_write(hw, 0x13, 0x8108);
	rtl_mdio_write(hw, 0x14, 0x13B0);
	rtl_mdio_write(hw, 0x13, 0x8103);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xF800, 0xB800);
	rtl_mdio_write(hw, 0x13, 0x8105);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x0A00);

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x87EB);
	rtl_mdio_write(hw, 0x14, 0x0018);
	rtl_mdio_write(hw, 0x13, 0x87EB);
	rtl_mdio_write(hw, 0x14, 0x0018);
	rtl_mdio_write(hw, 0x13, 0x87ED);
	rtl_mdio_write(hw, 0x14, 0x0733);
	rtl_mdio_write(hw, 0x13, 0x87EF);
	rtl_mdio_write(hw, 0x14, 0x08DC);
	rtl_mdio_write(hw, 0x13, 0x87F1);
	rtl_mdio_write(hw, 0x14, 0x08DF);
	rtl_mdio_write(hw, 0x13, 0x87F3);
	rtl_mdio_write(hw, 0x14, 0x0C79);
	rtl_mdio_write(hw, 0x13, 0x87F5);
	rtl_mdio_write(hw, 0x14, 0x0D93);
	rtl_mdio_write(hw, 0x13, 0x87F9);
	rtl_mdio_write(hw, 0x14, 0x0010);
	rtl_mdio_write(hw, 0x13, 0x87FB);
	rtl_mdio_write(hw, 0x14, 0x0800);
	rtl_mdio_write(hw, 0x13, 0x8015);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0x7000, 0x7000);

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x8111);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x7C00);
	rtl_mdio_write(hw, 0x1F, 0x0000);
}

void
hw_phy_config_8168h(struct rtl_hw *hw)
{
	if (hw->mcfg == CFG_METHOD_29)
		hw_phy_config_8168h_1(hw);
	else if (hw->mcfg == CFG_METHOD_30 || hw->mcfg == CFG_METHOD_37)
		hw_phy_config_8168h_2(hw);
	else if (hw->mcfg == CFG_METHOD_35)
		hw_phy_config_8168h_3(hw);

	/* Enable EthPhyPPSW */
	if (hw->mcfg != CFG_METHOD_37) {
		rtl_mdio_write(hw, 0x1F, 0x0A44);
		rtl_clear_eth_phy_bit(hw, 0x11, BIT_7);
		rtl_mdio_write(hw, 0x1F, 0x0000);
	}
}

void
hw_config_8168h(struct rtl_hw *hw)
{
	u32 csi_tmp;
	u16 mac_ocp_data;

	/* Share fifo rx params */
	rtl_eri_write(hw, 0xC8, 4, 0x00080002, ERIAR_ExGMAC);
	rtl_eri_write(hw, 0xCC, 1, 0x38, ERIAR_ExGMAC);
	rtl_eri_write(hw, 0xD0, 1, 0x48, ERIAR_ExGMAC);
	rtl_eri_write(hw, 0xE8, 4, 0x00100006, ERIAR_ExGMAC);

	/* Adjust the trx fifo*/
	rtl_eri_write(hw, 0xCA, 2, 0x0370, ERIAR_ExGMAC);
	rtl_eri_write(hw, 0xEA, 1, 0x10, ERIAR_ExGMAC);

	/* Disable share fifo */
	RTL_W32(hw, TxConfig, RTL_R32(hw, TxConfig) & ~BIT_7);

	if (hw->mcfg == CFG_METHOD_35 || hw->mcfg == CFG_METHOD_36)
		rtl8168_set_mcu_ocp_bit(hw, 0xD438, (BIT_1 | BIT_0));

	/* EPHY err mask */
	mac_ocp_data = rtl_mac_ocp_read(hw, 0xE0D6);
	mac_ocp_data &= ~(BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4 | BIT_3 |
			  BIT_2 | BIT_1 | BIT_0);
	mac_ocp_data |= 0x17F;
	rtl_mac_ocp_write(hw, 0xE0D6, mac_ocp_data);

	RTL_W8(hw, Config3, RTL_R8(hw, Config3) & ~Beacon_en);

	/* EEE led enable */
	RTL_W8(hw, 0x1B, RTL_R8(hw, 0x1B) & ~0x07);

	RTL_W8(hw, Config2, RTL_R8(hw, Config2) & ~PMSTS_En);

	csi_tmp = rtl_eri_read(hw, 0xDC, 4, ERIAR_ExGMAC);
	csi_tmp |= (BIT_2 | BIT_3 | BIT_4);
	rtl_eri_write(hw, 0xDC, 4, csi_tmp, ERIAR_ExGMAC);

	/* CRC wake disable */
	rtl_mac_ocp_write(hw, 0xC140, 0xFFFF);
	rtl_mac_ocp_write(hw, 0xC142, 0xFFFF);

	csi_tmp = rtl_eri_read(hw, 0x1B0, 4, ERIAR_ExGMAC);
	csi_tmp &= ~BIT_12;
	rtl_eri_write(hw, 0x1B0, 4, csi_tmp, ERIAR_ExGMAC);

	csi_tmp = rtl_eri_read(hw, 0x2FC, 1, ERIAR_ExGMAC);
	csi_tmp &= ~BIT_2;
	rtl_eri_write(hw, 0x2FC, 1, csi_tmp, ERIAR_ExGMAC);

	if (hw->mcfg != CFG_METHOD_37) {
		csi_tmp = rtl_eri_read(hw, 0x1D0, 1, ERIAR_ExGMAC);
		csi_tmp |= BIT_1;
		rtl_eri_write(hw, 0x1D0, 1, csi_tmp, ERIAR_ExGMAC);
	}
}

const struct rtl_hw_ops rtl8168h_ops = {
	.hw_config         = hw_config_8168h,
	.hw_init_rxcfg     = hw_init_rxcfg_8168h,
	.hw_ephy_config    = hw_ephy_config_8168h,
	.hw_phy_config     = hw_phy_config_8168h,
	.hw_mac_mcu_config = hw_mac_mcu_config_8168h,
	.hw_phy_mcu_config = hw_phy_mcu_config_8168h,
};
