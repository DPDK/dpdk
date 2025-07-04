/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#ifndef _RTL8168FP_H_
#define _RTL8168FP_H_

#include "../r8169_compat.h"

extern const struct rtl_hw_ops rtl8168fp_ops;

void hw_mac_mcu_config_8168fp(struct rtl_hw *hw);

#endif
