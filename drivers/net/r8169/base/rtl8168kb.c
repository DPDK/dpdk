/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_ethdev.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl8125a.h"
#include "rtl8125b.h"

/* For RTL8168KB, CFG_METHOD_52,53 */

static void
hw_init_rxcfg_8168kb(struct rtl_hw *hw)
{
	if (hw->mcfg == CFG_METHOD_52)
		RTL_W32(hw, RxConfig, Rx_Fetch_Number_8 |
			(RX_DMA_BURST_256 << RxCfgDMAShift));
	else if (hw->mcfg == CFG_METHOD_53)
		RTL_W32(hw, RxConfig, Rx_Fetch_Number_8 | RxCfg_pause_slot_en |
			(RX_DMA_BURST_256 << RxCfgDMAShift));
}

static void
hw_ephy_config_8168kb(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_52:
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
	case CFG_METHOD_53:
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
hw_phy_config_8168kb(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_52:
		rtl_hw_phy_config_8125a_2(hw);
		break;
	case CFG_METHOD_53:
		rtl_hw_phy_config_8125b_2(hw);
		break;
	}
}

static void
hw_mac_mcu_config_8168kb(struct rtl_hw *hw)
{
	if (hw->NotWrMcuPatchCode)
		return;

	rtl_hw_disable_mac_mcu_bps(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_52:
		/* Get H/W mac mcu patch code version */
		hw->hw_mcu_patch_code_ver = rtl_get_hw_mcu_patch_code_ver(hw);

		rtl_set_mac_mcu_8125a_2(hw);
		break;
	case CFG_METHOD_53:
		rtl_set_mac_mcu_8125b_2(hw);
		break;
	}
}

static void
hw_phy_mcu_config_8168kb(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_52:
		rtl_set_phy_mcu_8125a_2(hw);
		break;
	case CFG_METHOD_53:
		rtl_set_phy_mcu_8125b_2(hw);
		break;
	}
}

const struct rtl_hw_ops rtl8168kb_ops = {
	.hw_init_rxcfg     = hw_init_rxcfg_8168kb,
	.hw_ephy_config    = hw_ephy_config_8168kb,
	.hw_phy_config     = hw_phy_config_8168kb,
	.hw_mac_mcu_config = hw_mac_mcu_config_8168kb,
	.hw_phy_mcu_config = hw_phy_mcu_config_8168kb,
};
