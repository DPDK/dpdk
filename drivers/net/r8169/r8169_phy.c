/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include <rte_ether.h>
#include <ethdev_pci.h>

#include "r8169_ethdev.h"
#include "r8169_hw.h"
#include "r8169_phy.h"

static void
rtl_clear_set_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 clearmask,
			  u16 setmask)
{
	u16 phy_reg_value;

	phy_reg_value = rtl_mac_ocp_read(hw, addr);
	phy_reg_value &= ~clearmask;
	phy_reg_value |= setmask;
	rtl_mac_ocp_write(hw, addr, phy_reg_value);
}

void
rtl_clear_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask)
{
	rtl_clear_set_mac_ocp_bit(hw, addr, mask, 0);
}

void
rtl_set_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask)
{
	rtl_clear_set_mac_ocp_bit(hw, addr, 0, mask);
}
