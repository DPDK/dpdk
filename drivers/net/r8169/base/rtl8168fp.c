/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_compat.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl8168fp.h"

/* For RTL8168FP, CFG_METHOD_31,32,33,34 */

static void
hw_init_rxcfg_8168fp(struct rtl_hw *hw)
{
	RTL_W32(hw, RxConfig, Rx_Single_fetch_V2 |
		(RX_DMA_BURST_unlimited << RxCfgDMAShift) | RxEarly_off_V2);
}

static void
hw_ephy_config_8168fp(struct rtl_hw *hw)
{
	rtl_clear_and_set_pcie_phy_bit(hw, 0x19, BIT_6, BIT_12 | BIT_8);
	rtl_clear_and_set_pcie_phy_bit(hw, 0x59, BIT_6, BIT_12 | BIT_8);

	rtl_clear_pcie_phy_bit(hw, 0x0C, BIT_4);
	rtl_clear_pcie_phy_bit(hw, 0x4C, BIT_4);
	rtl_clear_pcie_phy_bit(hw, 0x0B, BIT_0);
}

static void
hw_phy_config_8168fp(struct rtl_hw *hw)
{
	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x808E);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x4800);
	rtl_mdio_write(hw, 0x13, 0x8090);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0xCC00);
	rtl_mdio_write(hw, 0x13, 0x8092);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0xB000);
	rtl_mdio_write(hw, 0x13, 0x8088);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x6000);
	rtl_mdio_write(hw, 0x13, 0x808B);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0x3F00, 0x0B00);
	rtl_mdio_write(hw, 0x13, 0x808D);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0x1F00, 0x0600);
	rtl_mdio_write(hw, 0x13, 0x808C);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0xB000);

	rtl_mdio_write(hw, 0x13, 0x80A0);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x2800);
	rtl_mdio_write(hw, 0x13, 0x80A2);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x5000);
	rtl_mdio_write(hw, 0x13, 0x809B);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xF800, 0xB000);
	rtl_mdio_write(hw, 0x13, 0x809A);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x4B00);
	rtl_mdio_write(hw, 0x13, 0x809D);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0x3F00, 0x0800);
	rtl_mdio_write(hw, 0x13, 0x80A1);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x7000);
	rtl_mdio_write(hw, 0x13, 0x809F);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0x1F00, 0x0300);
	rtl_mdio_write(hw, 0x13, 0x809E);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x8800);

	rtl_mdio_write(hw, 0x13, 0x80B2);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x2200);
	rtl_mdio_write(hw, 0x13, 0x80AD);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xF800, 0x9800);
	rtl_mdio_write(hw, 0x13, 0x80AF);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0x3F00, 0x0800);
	rtl_mdio_write(hw, 0x13, 0x80B3);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x6F00);
	rtl_mdio_write(hw, 0x13, 0x80B1);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0x1F00, 0x0300);
	rtl_mdio_write(hw, 0x13, 0x80B0);
	rtl_clear_and_set_eth_phy_bit(hw, 0x14, 0xFF00, 0x9300);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x8011);
	rtl_set_eth_phy_bit(hw, 0x14, BIT_11);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0A44);
	rtl_set_eth_phy_bit(hw, 0x11, BIT_11);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	rtl_mdio_write(hw, 0x1F, 0x0A43);
	rtl_mdio_write(hw, 0x13, 0x8016);
	rtl_set_eth_phy_bit(hw, 0x14, BIT_10);
	rtl_mdio_write(hw, 0x1F, 0x0000);

	/* Enable EthPhyPPSW */
	rtl_mdio_write(hw, 0x1F, 0x0A44);
	rtl_clear_eth_phy_bit(hw, 0x11, BIT_7);
	rtl_mdio_write(hw, 0x1F, 0x0000);
}

static void
hw_config_8168fp(struct rtl_hw *hw)
{
	u16 mac_ocp_data;
	u32 csi_tmp;

	rtl_eri_write(hw, 0xC8, 4, 0x00080002, ERIAR_ExGMAC);
	rtl_eri_write(hw, 0xCC, 1, 0x2F, ERIAR_ExGMAC);
	rtl_eri_write(hw, 0xD0, 1, 0x5F, ERIAR_ExGMAC);
	rtl_eri_write(hw, 0xE8, 4, 0x00100006, ERIAR_ExGMAC);

	/* Adjust the trx fifo*/
	rtl_eri_write(hw, 0xCA, 2, 0x0370, ERIAR_ExGMAC);
	rtl_eri_write(hw, 0xEA, 1, 0x10, ERIAR_ExGMAC);

	/* Disable share fifo */
	RTL_W32(hw, TxConfig, RTL_R32(hw, TxConfig) & ~BIT_7);

	csi_tmp = rtl_eri_read(hw, 0xDC, 1, ERIAR_ExGMAC);
	csi_tmp &= ~BIT_0;
	rtl_eri_write(hw, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);
	csi_tmp |= BIT_0;
	rtl_eri_write(hw, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);

	/* EEE pwrsave params */
	mac_ocp_data = rtl_mac_ocp_read(hw, 0xE056);
	mac_ocp_data &= ~(BIT_7 | BIT_6 | BIT_5 | BIT_4);
	rtl_mac_ocp_write(hw, 0xE056, mac_ocp_data);

	if (hw->HwPkgDet == 0x0F)
		rtl_mac_ocp_write(hw, 0xEA80, 0x0003);
	else
		rtl_mac_ocp_write(hw, 0xEA80, 0x0000);

	rtl_oob_mutex_lock(hw);
	mac_ocp_data = rtl_mac_ocp_read(hw, 0xE052);
	mac_ocp_data &= ~(BIT_3 | BIT_0);
	if (hw->HwPkgDet == 0x0F)
		mac_ocp_data |= BIT_0;
	rtl_mac_ocp_write(hw, 0xE052, mac_ocp_data);
	rtl_oob_mutex_unlock(hw);

	RTL_W8(hw, Config3, RTL_R8(hw, Config3) & ~Beacon_en);

	RTL_W8(hw, 0x1B, RTL_R8(hw, 0x1B) & ~0x07);

	RTL_W8(hw, Config2, RTL_R8(hw, Config2) & ~PMSTS_En);

	if (!HW_SUPP_SERDES_PHY(hw)) {
		RTL_W8(hw, 0xD0, RTL_R8(hw, 0xD0) | BIT_6);
		RTL_W8(hw, 0xF2, RTL_R8(hw, 0xF2) | BIT_6);
		RTL_W8(hw, 0xD0, RTL_R8(hw, 0xD0) | BIT_7);
	} else {
		RTL_W8(hw, 0xD0, RTL_R8(hw, 0xD0) & ~BIT_6);
		RTL_W8(hw, 0xF2, RTL_R8(hw, 0xF2) & ~BIT_6);
		RTL_W8(hw, 0xD0, RTL_R8(hw, 0xD0) & ~BIT_7);
	}

	rtl_oob_mutex_lock(hw);
	if (hw->HwPkgDet == 0x0F)
		rtl_eri_write(hw, 0x5F0, 2, 0x4F00, ERIAR_ExGMAC);
	else
		rtl_eri_write(hw, 0x5F0, 2, 0x4000, ERIAR_ExGMAC);
	rtl_oob_mutex_unlock(hw);

	csi_tmp = rtl_eri_read(hw, 0xDC, 4, ERIAR_ExGMAC);
	csi_tmp |= (BIT_2 | BIT_3);
	rtl_eri_write(hw, 0xDC, 4, csi_tmp, ERIAR_ExGMAC);

	if (hw->mcfg == CFG_METHOD_32 || hw->mcfg == CFG_METHOD_33 ||
	    hw->mcfg == CFG_METHOD_34) {
		csi_tmp = rtl_eri_read(hw, 0xD4, 4, ERIAR_ExGMAC);
		csi_tmp |= BIT_4;
		rtl_eri_write(hw, 0xD4, 4, csi_tmp, ERIAR_ExGMAC);
	}

	rtl_mac_ocp_write(hw, 0xC140, 0xFFFF);
	rtl_mac_ocp_write(hw, 0xC142, 0xFFFF);

	csi_tmp = rtl_eri_read(hw, 0x2FC, 1, ERIAR_ExGMAC);
	csi_tmp &= ~(BIT_0 | BIT_1);
	csi_tmp |= BIT_0;
	rtl_eri_write(hw, 0x2FC, 1, csi_tmp, ERIAR_ExGMAC);

	csi_tmp = rtl_eri_read(hw, 0x1D0, 1, ERIAR_ExGMAC);
	csi_tmp &= ~BIT_1;
	rtl_eri_write(hw, 0x1D0, 1, csi_tmp, ERIAR_ExGMAC);
}

const struct rtl_hw_ops rtl8168fp_ops = {
	.hw_config         = hw_config_8168fp,
	.hw_init_rxcfg     = hw_init_rxcfg_8168fp,
	.hw_ephy_config    = hw_ephy_config_8168fp,
	.hw_phy_config     = hw_phy_config_8168fp,
	.hw_mac_mcu_config = hw_mac_mcu_config_8168fp,
};
