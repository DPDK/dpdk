/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_compat.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl8168h.h"

/* For RTL8168M, CFG_METHOD_37 */

const struct rtl_hw_ops rtl8168m_ops = {
	.hw_config         = hw_config_8168h,
	.hw_init_rxcfg     = hw_init_rxcfg_8168h,
	.hw_ephy_config    = hw_ephy_config_8168h,
	.hw_phy_config     = hw_phy_config_8168h,
	.hw_mac_mcu_config = hw_mac_mcu_config_8168h,
	.hw_phy_mcu_config = hw_phy_mcu_config_8168h,
};
