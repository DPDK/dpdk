/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#ifndef _RTL8168EP_H_
#define _RTL8168EP_H_

#include "../r8169_compat.h"

extern const struct rtl_hw_ops rtl8168ep_ops;

void hw_mac_mcu_config_8168ep(struct rtl_hw *hw);
void hw_phy_mcu_config_8168ep(struct rtl_hw *hw);

#endif
