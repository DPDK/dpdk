/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_ethdev.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl8125bp_mcu.h"

/* For RTL8125BP, CFG_METHOD_54,55 */

static void
hw_init_rxcfg_8125bp(struct rtl_hw *hw)
{
	RTL_W32(hw, RxConfig, Rx_Fetch_Number_8 | Rx_Close_Multiple |
		RxCfg_pause_slot_en | (RX_DMA_BURST_256 << RxCfgDMAShift));
}

static void
hw_ephy_config_8125bp(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_54:
	case CFG_METHOD_55:
		/* Nothing to do */
		break;
	}
}

static void
rtl_hw_phy_config_8125bp_1(struct rtl_hw *hw)
{
	rtl_set_eth_phy_ocp_bit(hw, 0xA442, BIT_11);

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA80C, BIT_14,
					  (BIT_15 | BIT_11 | BIT_10));

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8010);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_11);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8088);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x9000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x808F);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x9000);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8174);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, BIT_13, (BIT_12 | BIT_11));
}

static void
rtl_hw_phy_config_8125bp_2(struct rtl_hw *hw)
{
	rtl_set_eth_phy_ocp_bit(hw, 0xA442, BIT_11);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8010);
	rtl_clear_eth_phy_ocp_bit(hw, 0xA438, BIT_11);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8088);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x9000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x808F);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x9000);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8174);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xA438, BIT_13, (BIT_12 | BIT_11));
}

static void
hw_phy_config_8125bp(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_54:
		rtl_hw_phy_config_8125bp_1(hw);
		break;
	case CFG_METHOD_55:
		rtl_hw_phy_config_8125bp_2(hw);
		break;
	}
}

static void
hw_mac_mcu_config_8125bp(struct rtl_hw *hw)
{
	if (hw->NotWrMcuPatchCode)
		return;

	rtl_hw_disable_mac_mcu_bps(hw);

	/* Get H/W mac mcu patch code version */
	hw->hw_mcu_patch_code_ver = rtl_get_hw_mcu_patch_code_ver(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_54:
		rtl_set_mac_mcu_8125bp_1(hw);
		break;
	case CFG_METHOD_55:
		rtl_set_mac_mcu_8125bp_2(hw);
		break;
	}
}

static void
hw_phy_mcu_config_8125bp(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_54:
		rtl_set_phy_mcu_8125bp_1(hw);
		break;
	case CFG_METHOD_55:
		/* Nothing to do */
		break;
	}
}

const struct rtl_hw_ops rtl8125bp_ops = {
	.hw_init_rxcfg     = hw_init_rxcfg_8125bp,
	.hw_ephy_config    = hw_ephy_config_8125bp,
	.hw_phy_config     = hw_phy_config_8125bp,
	.hw_mac_mcu_config = hw_mac_mcu_config_8125bp,
	.hw_phy_mcu_config = hw_phy_mcu_config_8125bp,
};
