/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#ifndef R8169_PHY_H
#define R8169_PHY_H

#include <stdint.h>

#include <rte_ethdev.h>
#include <rte_ethdev_core.h>

#include "r8169_compat.h"
#include "r8169_ethdev.h"

void rtl_clear_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask);
void rtl_set_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask);

#endif /* R8169_PHY_H */
