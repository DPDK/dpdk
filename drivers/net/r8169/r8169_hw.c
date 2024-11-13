/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include <rte_ether.h>
#include <ethdev_driver.h>

#include "r8169_hw.h"
#include "r8169_logs.h"

void
rtl_mac_ocp_write(struct rtl_hw *hw, u16 addr, u16 value)
{
	u32 data32;

	data32 = addr / 2;
	data32 <<= OCPR_Addr_Reg_shift;
	data32 += value;
	data32 |= OCPR_Write;

	RTL_W32(hw, MACOCP, data32);
}

u16
rtl_mac_ocp_read(struct rtl_hw *hw, u16 addr)
{
	u32 data32;
	u16 data16 = 0;

	data32 = addr / 2;
	data32 <<= OCPR_Addr_Reg_shift;

	RTL_W32(hw, MACOCP, data32);
	data16 = (u16)RTL_R32(hw, MACOCP);

	return data16;
}

u32
rtl_csi_read(struct rtl_hw *hw, u32 addr)
{
	u32 cmd;
	int i;
	u32 value = 0;

	cmd = CSIAR_Read | CSIAR_ByteEn << CSIAR_ByteEn_shift |
	      (addr & CSIAR_Addr_Mask);

	RTL_W32(hw, CSIAR, cmd);

	for (i = 0; i < 10; i++) {
		rte_delay_us(100);

		/* Check if the NIC has completed CSI read */
		if (RTL_R32(hw, CSIAR) & CSIAR_Flag) {
			value = RTL_R32(hw, CSIDR);
			break;
		}
	}

	rte_delay_us(20);

	return value;
}

void
rtl_csi_write(struct rtl_hw *hw, u32 addr, u32 value)
{
	u32 cmd;
	int i;

	RTL_W32(hw, CSIDR, value);
	cmd = CSIAR_Write | CSIAR_ByteEn << CSIAR_ByteEn_shift |
	      (addr & CSIAR_Addr_Mask);

	RTL_W32(hw, CSIAR, cmd);

	for (i = 0; i < RTL_CHANNEL_WAIT_COUNT; i++) {
		rte_delay_us(RTL_CHANNEL_WAIT_TIME);

		/* Check if the NIC has completed CSI write */
		if (!(RTL_R32(hw, CSIAR) & CSIAR_Flag))
			break;
	}

	rte_delay_us(RTL_CHANNEL_EXIT_DELAY_TIME);
}
