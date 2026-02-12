/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#ifndef R8169_FIBER_H
#define R8169_FIBER_H

#include <stdint.h>

#include <bus_pci_driver.h>
#include <rte_ethdev.h>
#include <rte_ethdev_core.h>

#include "r8169_compat.h"
#include "r8169_ethdev.h"
#include "r8169_phy.h"
#include "r8169_hw.h"

enum {
	FIBER_MODE_NIC_ONLY = 0,
	FIBER_MODE_RTL8127ATF,
	FIBER_MODE_MAX
};

#define HW_FIBER_MODE_ENABLED(_M)   ((_M)->HwFiberModeVer > 0)

/* sds address */
#define R8127_SDS_8127_CMD      0x2348
#define R8127_SDS_8127_ADDR     0x234A
#define R8127_SDS_8127_DATA_IN  0x234C
#define R8127_SDS_8127_DATA_OUT 0x234E

#define R8127_MAKE_SDS_8127_ADDR(_index, _page, _reg) \
	(((_index) << 11) | ((_page) << 5) | (_reg))

/* sds command */
#define R8127_SDS_8127_CMD_IN BIT_0
#define R8127_SDS_8127_WE_IN  BIT_1

void rtl8127_hw_fiber_phy_config(struct rtl_hw *hw);

#endif /* R8169_FIBER_H */
