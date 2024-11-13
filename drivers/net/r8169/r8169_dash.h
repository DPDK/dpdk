/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#ifndef R8169_DASH_H
#define R8169_DASH_H

#include <stdint.h>
#include <stdbool.h>

#include <rte_ethdev.h>
#include <rte_ethdev_core.h>

#include "r8169_compat.h"
#include "r8169_hw.h"

#define HW_DASH_SUPPORT_DASH(_M)        ((_M)->HwSuppDashVer > 0)
#define HW_DASH_SUPPORT_TYPE_1(_M)      ((_M)->HwSuppDashVer == 1)
#define HW_DASH_SUPPORT_TYPE_2(_M)      ((_M)->HwSuppDashVer == 2)
#define HW_DASH_SUPPORT_TYPE_3(_M)      ((_M)->HwSuppDashVer == 3)
#define HW_DASH_SUPPORT_TYPE_4(_M)      ((_M)->HwSuppDashVer == 4)

#define HW_DASH_SUPPORT_GET_FIRMWARE_VERSION(_M) (HW_DASH_SUPPORT_TYPE_2(_M) || \
						  HW_DASH_SUPPORT_TYPE_3(_M) || \
						  HW_DASH_SUPPORT_TYPE_4(_M))

#define OCP_REG_FIRMWARE_MAJOR_VERSION 0x120

bool rtl_is_allow_access_dash_ocp(struct rtl_hw *hw);

int rtl_check_dash(struct rtl_hw *hw);

#endif /* R8169_DASH_H */
