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
#define HW_DASH_SUPPORT_CMAC(_M)        (HW_DASH_SUPPORT_TYPE_2(_M) || HW_DASH_SUPPORT_TYPE_3(_M))
#define HW_DASH_SUPPORT_GET_FIRMWARE_VERSION(_M) (HW_DASH_SUPPORT_TYPE_2(_M) || \
						  HW_DASH_SUPPORT_TYPE_3(_M) || \
						  HW_DASH_SUPPORT_TYPE_4(_M))

#define OOB_CMD_DRIVER_START 0x05
#define OOB_CMD_DRIVER_STOP  0x06

#define OCP_REG_FIRMWARE_MAJOR_VERSION 0x120

#define ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE BIT_5

/* CMAC write/read MMIO register */
#define RTL_CMAC_REG_ADDR(hw, reg) ((u8 *)(hw)->cmac_ioaddr + (reg))
#define RTL_CMAC_R32(hw, reg) rtl_read32(RTL_CMAC_REG_ADDR(hw, reg))
#define RTL_CMAC_R16(hw, reg) rtl_read16(RTL_CMAC_REG_ADDR(hw, reg))
#define RTL_CMAC_R8(hw, reg) rte_read8(RTL_CMAC_REG_ADDR(hw, reg))

#define RTL_CMAC_W32(hw, reg, val) \
	rte_write32((rte_cpu_to_le_32(val)), RTL_CMAC_REG_ADDR(hw, reg))

#define RTL_CMAC_W16(hw, reg, val) \
	rte_write16((rte_cpu_to_le_16(val)), RTL_CMAC_REG_ADDR(hw, reg))

#define RTL_CMAC_W8(hw, reg, val) \
	rte_write8((val), RTL_CMAC_REG_ADDR(hw, reg))

bool rtl_is_allow_access_dash_ocp(struct rtl_hw *hw);

int rtl_check_dash(struct rtl_hw *hw);

void rtl_driver_start(struct rtl_hw *hw);
void rtl_driver_stop(struct rtl_hw *hw);
void rtl_dash2_disable_txrx(struct rtl_hw *hw);

bool rtl8168_check_dash_other_fun_present(struct rtl_hw *hw);

#endif /* R8169_DASH_H */
