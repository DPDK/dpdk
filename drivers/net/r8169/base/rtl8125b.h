/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#ifndef RTL8125B_H
#define RTL8125B_H

void rtl_set_mac_mcu_8125b_2(struct rtl_hw *hw);

void rtl_set_phy_mcu_8125b_1(struct rtl_hw *hw);
void rtl_set_phy_mcu_8125b_2(struct rtl_hw *hw);

void rtl_hw_phy_config_8125b_2(struct rtl_hw *hw);

#endif /* RTL8125B_H */
