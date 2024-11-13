/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#ifndef R8169_PHY_H
#define R8169_PHY_H

#include <stdint.h>
#include <stdbool.h>

#include <rte_ethdev.h>
#include <rte_ethdev_core.h>

#include "r8169_compat.h"
#include "r8169_ethdev.h"

void rtl_clear_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask);
void rtl_set_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask);

u32 rtl_mdio_direct_read_phy_ocp(struct rtl_hw *hw, u32 RegAddr);
void rtl_mdio_direct_write_phy_ocp(struct rtl_hw *hw, u32 RegAddr, u32 value);

u32 rtl_mdio_read(struct rtl_hw *hw, u32 RegAddr);
void rtl_mdio_write(struct rtl_hw *hw, u32 RegAddr, u32 value);

void rtl_clear_and_set_eth_phy_ocp_bit(struct rtl_hw *hw, u16 addr,
				       u16 clearmask, u16 setmask);
void rtl_clear_eth_phy_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask);
void rtl_set_eth_phy_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask);

void rtl_ephy_write(struct rtl_hw *hw, int addr, int value);

void rtl_clear_and_set_pcie_phy_bit(struct rtl_hw *hw, u8 addr, u16 clearmask,
				    u16 setmask);
void rtl_clear_pcie_phy_bit(struct rtl_hw *hw, u8 addr, u16 mask);
void rtl_set_pcie_phy_bit(struct rtl_hw *hw, u8 addr, u16 mask);

bool rtl_set_phy_mcu_patch_request(struct rtl_hw *hw);
bool rtl_clear_phy_mcu_patch_request(struct rtl_hw *hw);

void rtl_set_phy_mcu_ram_code(struct rtl_hw *hw, const u16 *ramcode,
			      u16 codesize);

#endif /* R8169_PHY_H */
