/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_ethdev.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl8125cp_mcu.h"

/* For RTL8125CP, CFG_METHOD_58 */

static void
hw_init_rxcfg_8125cp(struct rtl_hw *hw)
{
	RTL_W32(hw, RxConfig, Rx_Fetch_Number_8 | Rx_Close_Multiple |
		RxCfg_pause_slot_en | (RX_DMA_BURST_256 << RxCfgDMAShift));
}

static void
hw_ephy_config_8125cp(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_58:
		/* nothing to do */
		break;
	}
}

static void
rtl_hw_phy_config_8125cp_1(struct rtl_hw *hw)
{
	rtl_set_eth_phy_ocp_bit(hw, 0xA442, BIT_11);

	rtl_clear_and_set_eth_phy_ocp_bit(hw, 0xad0e, 0x007F, 0x000B);
	rtl_set_eth_phy_ocp_bit(hw, 0xad78, BIT_4);
}

static void
hw_phy_config_8125cp(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_58:
		rtl_hw_phy_config_8125cp_1(hw);
		break;
	}
}

static void
hw_mac_mcu_config_8125cp(struct rtl_hw *hw)
{
	if (hw->NotWrMcuPatchCode)
		return;

	rtl_hw_disable_mac_mcu_bps(hw);
}

static void
hw_phy_mcu_config_8125cp(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_58:
		rtl_set_phy_mcu_8125cp_1(hw);
		break;
	}
}

const struct rtl_hw_ops rtl8125cp_ops = {
	.hw_init_rxcfg     = hw_init_rxcfg_8125cp,
	.hw_ephy_config    = hw_ephy_config_8125cp,
	.hw_phy_config     = hw_phy_config_8125cp,
	.hw_mac_mcu_config = hw_mac_mcu_config_8125cp,
	.hw_phy_mcu_config = hw_phy_mcu_config_8125cp,
};
