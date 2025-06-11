/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_compat.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl8168ep.h"

/* For RTL8168EP, CFG_METHOD_23,27,28 */

static void
hw_init_rxcfg_8168ep(struct rtl_hw *hw)
{
	RTL_W32(hw, RxConfig, Rx_Single_fetch_V2 |
		(RX_DMA_BURST_unlimited << RxCfgDMAShift) | RxEarly_off_V2);
}

static void
hw_ephy_config_8168ep(struct rtl_hw *hw)
{
	u16 ephy_data;

	switch (hw->mcfg) {
	case CFG_METHOD_23:
		rtl_ephy_write(hw, 0x00, 0x10AB);
		rtl_ephy_write(hw, 0x06, 0xf030);
		rtl_ephy_write(hw, 0x08, 0x2006);
		rtl_ephy_write(hw, 0x0D, 0x1666);

		ephy_data = rtl_ephy_read(hw, 0x0C);
		ephy_data &= ~(BIT_13 | BIT_12 | BIT_11 | BIT_10 | BIT_9 |
			       BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4);
		rtl_ephy_write(hw, 0x0C, ephy_data);
		break;
	case CFG_METHOD_27:
		rtl_ephy_write(hw, 0x00, 0x10A3);
		rtl_ephy_write(hw, 0x19, 0xFC00);
		rtl_ephy_write(hw, 0x1E, 0x20EA);
		break;
	case CFG_METHOD_28:
		rtl_ephy_write(hw, 0x00, 0x10AB);
		rtl_ephy_write(hw, 0x19, 0xFC00);
		rtl_ephy_write(hw, 0x1E, 0x20EB);
		rtl_ephy_write(hw, 0x0D, 0x1666);
		rtl_clear_pcie_phy_bit(hw, 0x0B, BIT_0);
		rtl_set_pcie_phy_bit(hw, 0x1D, BIT_14);
		rtl_clear_and_set_pcie_phy_bit(hw, 0x0C, (BIT_13 | BIT_12 | BIT_11 |
					       BIT_10 | BIT_8 | BIT_7 | BIT_6 | BIT_5),
					       BIT_9 | BIT_4);
		break;
	default:
		break;
	}
}

static void
hw_phy_config_8168ep(struct rtl_hw *hw)
{
	if (hw->mcfg == CFG_METHOD_23) {
		rtl_mdio_write(hw, 0x1F, 0x0A44);
		rtl_mdio_write(hw, 0x11, rtl_mdio_read(hw, 0x11) |
					 (BIT_3 | BIT_2));
		rtl_mdio_write(hw, 0x1F, 0x0000);

		rtl_mdio_write(hw, 0x1F, 0x0BCC);
		rtl_mdio_write(hw, 0x14, rtl_mdio_read(hw, 0x14) & ~BIT_8);
		rtl_mdio_write(hw, 0x1F, 0x0A44);
		rtl_mdio_write(hw, 0x11, rtl_mdio_read(hw, 0x11) | BIT_7);
		rtl_mdio_write(hw, 0x11, rtl_mdio_read(hw, 0x11) | BIT_6);
		rtl_mdio_write(hw, 0x1F, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x8084);
		rtl_mdio_write(hw, 0x14, rtl_mdio_read(hw, 0x14) &
					 ~(BIT_14 | BIT_13));
		rtl_mdio_write(hw, 0x10, rtl_mdio_read(hw, 0x10) | BIT_12);
		rtl_mdio_write(hw, 0x10, rtl_mdio_read(hw, 0x10) | BIT_1);
		rtl_mdio_write(hw, 0x10, rtl_mdio_read(hw, 0x10) | BIT_0);

		rtl_mdio_write(hw, 0x1F, 0x0A4B);
		rtl_mdio_write(hw, 0x11, rtl_mdio_read(hw, 0x11) | BIT_2);
		rtl_mdio_write(hw, 0x1F, 0x0000);

		rtl_mdio_write(hw, 0x1F, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x8012);
		rtl_mdio_write(hw, 0x14, rtl_mdio_read(hw, 0x14) | BIT_15);
		rtl_mdio_write(hw, 0x1F, 0x0000);

		rtl_mdio_write(hw, 0x1F, 0x0C42);
		rtl_clear_and_set_eth_phy_bit(hw, 0x11, BIT_13, BIT_14);
		rtl_mdio_write(hw, 0x1F, 0x0000);

	} else if (hw->mcfg == CFG_METHOD_27 || hw->mcfg == CFG_METHOD_28) {
		rtl_mdio_write(hw, 0x1F, 0x0BCC);
		rtl_mdio_write(hw, 0x14, rtl_mdio_read(hw, 0x14) & ~BIT_8);
		rtl_mdio_write(hw, 0x1F, 0x0A44);
		rtl_mdio_write(hw, 0x11, rtl_mdio_read(hw, 0x11) | BIT_7);
		rtl_mdio_write(hw, 0x11, rtl_mdio_read(hw, 0x11) | BIT_6);
		rtl_mdio_write(hw, 0x1F, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x8084);
		rtl_mdio_write(hw, 0x14, rtl_mdio_read(hw, 0x14) & ~(BIT_14 | BIT_13));
		rtl_mdio_write(hw, 0x10, rtl_mdio_read(hw, 0x10) | BIT_12);
		rtl_mdio_write(hw, 0x10, rtl_mdio_read(hw, 0x10) | BIT_1);
		rtl_mdio_write(hw, 0x10, rtl_mdio_read(hw, 0x10) | BIT_0);

		rtl_mdio_write(hw, 0x1F, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x8012);
		rtl_mdio_write(hw, 0x14, rtl_mdio_read(hw, 0x14) | BIT_15);

		rtl_mdio_write(hw, 0x1F, 0x0C42);
		rtl_mdio_write(hw, 0x11, (rtl_mdio_read(hw, 0x11) & ~BIT_13) | BIT_14);

		rtl_mdio_write(hw, 0x1F, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x80F3);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x8B00);
		rtl_mdio_write(hw, 0x13, 0x80F0);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x3A00);
		rtl_mdio_write(hw, 0x13, 0x80EF);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x0500);
		rtl_mdio_write(hw, 0x13, 0x80F6);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x6E00);
		rtl_mdio_write(hw, 0x13, 0x80EC);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x6800);
		rtl_mdio_write(hw, 0x13, 0x80ED);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x7C00);
		rtl_mdio_write(hw, 0x13, 0x80F2);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0xF400);
		rtl_mdio_write(hw, 0x13, 0x80F4);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x8500);
		rtl_mdio_write(hw, 0x1F, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x8110);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0xA800);
		rtl_mdio_write(hw, 0x13, 0x810F);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x1D00);
		rtl_mdio_write(hw, 0x13, 0x8111);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0xF500);
		rtl_mdio_write(hw, 0x13, 0x8113);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x6100);
		rtl_mdio_write(hw, 0x13, 0x8115);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x9200);
		rtl_mdio_write(hw, 0x13, 0x810E);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x0400);
		rtl_mdio_write(hw, 0x13, 0x810C);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x7C00);
		rtl_mdio_write(hw, 0x13, 0x810B);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x5A00);
		rtl_mdio_write(hw, 0x1F, 0x0A43);
		rtl_mdio_write(hw, 0x13, 0x80D1);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0xFF00);
		rtl_mdio_write(hw, 0x13, 0x80CD);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x9E00);
		rtl_mdio_write(hw, 0x13, 0x80D3);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x0E00);
		rtl_mdio_write(hw, 0x13, 0x80D5);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0xCA00);
		rtl_mdio_write(hw, 0x13, 0x80D7);
		rtl_mdio_write(hw, 0x14, (rtl_mdio_read(hw, 0x14) & ~0xFF00) | 0x8400);
	}
}

static void
hw_config_8168ep(struct rtl_hw *hw)
{
	u16 mac_ocp_data;
	u32 csi_tmp;

	rtl_eri_write(hw, 0xC8, 4, 0x00080002, ERIAR_ExGMAC);
	rtl_eri_write(hw, 0xCC, 1, 0x2F, ERIAR_ExGMAC);
	rtl_eri_write(hw, 0xD0, 1, 0x5F, ERIAR_ExGMAC);
	rtl_eri_write(hw, 0xE8, 4, 0x00100006, ERIAR_ExGMAC);

	/* Adjust the trx fifo */
	rtl_eri_write(hw, 0xCA, 2, 0x0370, ERIAR_ExGMAC);
	rtl_eri_write(hw, 0xEA, 1, 0x10, ERIAR_ExGMAC);

	/* Disable share fifo */
	RTL_W32(hw, TxConfig, RTL_R32(hw, TxConfig) & ~BIT_7);

	csi_tmp = rtl_eri_read(hw, 0xDC, 1, ERIAR_ExGMAC);
	csi_tmp &= ~BIT_0;
	rtl_eri_write(hw, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);
	csi_tmp |= BIT_0;
	rtl_eri_write(hw, 0xDC, 1, csi_tmp, ERIAR_ExGMAC);

	RTL_W8(hw, Config3, RTL_R8(hw, Config3) & ~Beacon_en);

	/* EEE led enable */
	RTL_W8(hw, 0x1B, RTL_R8(hw, 0x1B) & ~0x07);

	if (hw->mcfg == CFG_METHOD_27 || hw->mcfg == CFG_METHOD_28) {
		rtl_oob_mutex_lock(hw);
		rtl_eri_write(hw, 0x5F0, 2, 0x4F87, ERIAR_ExGMAC);
		rtl_oob_mutex_unlock(hw);
	}

	rtl_mac_ocp_write(hw, 0xC140, 0xFFFF);
	rtl_mac_ocp_write(hw, 0xC142, 0xFFFF);

	if (hw->mcfg == CFG_METHOD_28) {
		mac_ocp_data = rtl_mac_ocp_read(hw, 0xD3E2);
		mac_ocp_data &= 0xF000;
		mac_ocp_data |= 0xAFD;
		rtl_mac_ocp_write(hw, 0xD3E2, mac_ocp_data);

		mac_ocp_data = rtl_mac_ocp_read(hw, 0xD3E4);
		mac_ocp_data &= 0xFF00;
		rtl_mac_ocp_write(hw, 0xD3E4, mac_ocp_data);

		mac_ocp_data = rtl_mac_ocp_read(hw, 0xE860);
		mac_ocp_data |= BIT_7;
		rtl_mac_ocp_write(hw, 0xE860, mac_ocp_data);
	}
}

const struct rtl_hw_ops rtl8168ep_ops = {
	.hw_config         = hw_config_8168ep,
	.hw_init_rxcfg     = hw_init_rxcfg_8168ep,
	.hw_ephy_config    = hw_ephy_config_8168ep,
	.hw_phy_config     = hw_phy_config_8168ep,
	.hw_mac_mcu_config = hw_mac_mcu_config_8168ep,
	.hw_phy_mcu_config = hw_phy_mcu_config_8168ep,
};
