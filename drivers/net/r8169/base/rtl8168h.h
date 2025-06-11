/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#ifndef _RTL8168H_H_
#define _RTL8168H_H_

#include "../r8169_compat.h"

extern const struct rtl_hw_ops rtl8168h_ops;
extern const struct rtl_hw_ops rtl8168m_ops;

void hw_mac_mcu_config_8168h(struct rtl_hw *hw);
void hw_phy_mcu_config_8168h(struct rtl_hw *hw);

void hw_init_rxcfg_8168h(struct rtl_hw *hw);
void hw_ephy_config_8168h(struct rtl_hw *hw);
void hw_phy_config_8168h(struct rtl_hw *hw);
void hw_config_8168h(struct rtl_hw *hw);

#endif
