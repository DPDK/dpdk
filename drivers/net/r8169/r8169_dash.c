/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include <rte_ether.h>
#include <ethdev_driver.h>

#include "r8169_compat.h"
#include "r8169_dash.h"
#include "r8169_hw.h"

bool
rtl_is_allow_access_dash_ocp(struct rtl_hw *hw)
{
	bool allow_access = false;
	u16 mac_ocp_data;

	if (!HW_DASH_SUPPORT_DASH(hw))
		goto exit;

	allow_access = true;
	switch (hw->mcfg) {
	case CFG_METHOD_2:
	case CFG_METHOD_3:
		mac_ocp_data = rtl_mac_ocp_read(hw, 0xd460);
		if (mac_ocp_data == 0xffff || !(mac_ocp_data & BIT_0))
			allow_access = false;
		break;
	case CFG_METHOD_8:
	case CFG_METHOD_9:
		mac_ocp_data = rtl_mac_ocp_read(hw, 0xd4c0);
		if (mac_ocp_data == 0xffff || (mac_ocp_data & BIT_3))
			allow_access = false;
		break;
	default:
		goto exit;
	}
exit:
	return allow_access;
}

static u32
rtl_get_dash_fw_ver(struct rtl_hw *hw)
{
	u32 ver = 0xffffffff;

	if (HW_DASH_SUPPORT_GET_FIRMWARE_VERSION(hw) == FALSE)
		goto exit;

	ver = rtl_ocp_read(hw, OCP_REG_FIRMWARE_MAJOR_VERSION, 4);

exit:
	return ver;
}

static int
_rtl_check_dash(struct rtl_hw *hw)
{
	if (!hw->AllowAccessDashOcp)
		return 0;

	if (HW_DASH_SUPPORT_TYPE_2(hw) || HW_DASH_SUPPORT_TYPE_4(hw)) {
		if (rtl_ocp_read(hw, 0x128, 1) & BIT_0)
			return 1;
	}

	return 0;
}

int
rtl_check_dash(struct rtl_hw *hw)
{
	u32 ver;

	if (_rtl_check_dash(hw)) {
		ver = rtl_get_dash_fw_ver(hw);
		if (!(ver == 0 || ver == 0xffffffff))
			return 1;
	}

	return 0;
}
