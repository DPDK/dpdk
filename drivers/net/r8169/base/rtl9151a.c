/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_ethdev.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl9151a.h"

/* For RTL9151A, CFG_METHOD_60 */

static void
hw_init_rxcfg_9151a(struct rtl_hw *hw)
{
	RTL_W32(hw, RxConfig, Rx_Fetch_Number_8 | Rx_Close_Multiple |
		RxCfg_pause_slot_en | (RX_DMA_BURST_512 << RxCfgDMAShift));
}

static void
hw_ephy_config_9151a(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_60:
		/* nothing to do */
		break;
	}
}

static void
rtl_hw_phy_config_9151a_1(struct rtl_hw *hw)
{
	rtl_set_eth_phy_ocp_bit(hw, 0xA442, BIT_11);

	rtl_mdio_direct_write_phy_ocp(hw, 0xB87C, 0x8079);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xB87E, 0xFF00, 0x4400);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAC16, 0x00FF, 0x0001);
	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xAD0E, 0x007F, 0x000D);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x80B6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xB6C3);
}

static void
hw_phy_config_9151a(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_60:
		rtl_hw_phy_config_9151a_1(hw);
		break;
	}
}

static void
hw_mac_mcu_config_9151a(struct rtl_hw *hw)
{
	if (hw->NotWrMcuPatchCode)
		return;

	rtl_hw_disable_mac_mcu_bps(hw);

	/* Get H/W mac mcu patch code version */
	hw->hw_mcu_patch_code_ver = rtl_get_hw_mcu_patch_code_ver(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_60:
		/* no mac mcu patch code */
		break;
	}
}

static void
hw_phy_mcu_config_9151a(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_60:
		rtl_set_phy_mcu_9151a_1(hw);
		break;
	}
}

const struct rtl_hw_ops rtl9151a_ops = {
	.hw_init_rxcfg     = hw_init_rxcfg_9151a,
	.hw_ephy_config    = hw_ephy_config_9151a,
	.hw_phy_config     = hw_phy_config_9151a,
	.hw_mac_mcu_config = hw_mac_mcu_config_9151a,
	.hw_phy_mcu_config = hw_phy_mcu_config_9151a,
};
