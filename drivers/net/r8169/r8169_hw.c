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
#include "r8169_dash.h"

static u32
rtl_eri_read_with_oob_base_address(struct rtl_hw *hw, int addr, int len,
				   int type, const u32 base_address)
{
	int i, val_shift, shift = 0;
	u32 value1 = 0;
	u32 value2 = 0;
	u32 eri_cmd, tmp, mask;
	const u32 transformed_base_address = ((base_address & 0x00FFF000) << 6) |
					     (base_address & 0x000FFF);

	if (len > 4 || len <= 0)
		return -1;

	while (len > 0) {
		val_shift = addr % ERIAR_Addr_Align;
		addr = addr & ~0x3;

		eri_cmd = ERIAR_Read | transformed_base_address |
			  type << ERIAR_Type_shift |
			  ERIAR_ByteEn << ERIAR_ByteEn_shift |
			  (addr & 0x0FFF);
		if (addr & 0xF000) {
			tmp = addr & 0xF000;
			tmp >>= 12;
			eri_cmd |= (tmp << 20) & 0x00F00000;
		}

		RTL_W32(hw, ERIAR, eri_cmd);

		for (i = 0; i < RTL_CHANNEL_WAIT_COUNT; i++) {
			rte_delay_us(RTL_CHANNEL_WAIT_TIME);

			/* Check if the NIC has completed ERI read */
			if (RTL_R32(hw, ERIAR) & ERIAR_Flag)
				break;
		}

		if (len == 1)
			mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 2)
			mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 3)
			mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else
			mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

		value1 = RTL_R32(hw, ERIDR) & mask;
		value2 |= (value1 >> val_shift * 8) << shift * 8;

		if (len <= 4 - val_shift) {
			len = 0;
		} else {
			len -= (4 - val_shift);
			shift = 4 - val_shift;
			addr += 4;
		}
	}

	rte_delay_us(RTL_CHANNEL_EXIT_DELAY_TIME);

	return value2;
}

u32
rtl_eri_read(struct rtl_hw *hw, int addr, int len, int type)
{
	return rtl_eri_read_with_oob_base_address(hw, addr, len, type, 0);
}

static int
rtl_eri_write_with_oob_base_address(struct rtl_hw *hw, int addr,
				    int len, u32 value, int type,
				    const u32 base_address)
{
	int i, val_shift, shift = 0;
	u32 value1 = 0;
	u32 eri_cmd, mask, tmp;
	const u32 transformed_base_address = ((base_address & 0x00FFF000) << 6) |
					     (base_address & 0x000FFF);

	if (len > 4 || len <= 0)
		return -1;

	while (len > 0) {
		val_shift = addr % ERIAR_Addr_Align;
		addr = addr & ~0x3;

		if (len == 1)
			mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 2)
			mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 3)
			mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else
			mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

		value1 = rtl_eri_read_with_oob_base_address(hw, addr, 4, type,
							    base_address) & ~mask;
		value1 |= ((value << val_shift * 8) >> shift * 8);

		RTL_W32(hw, ERIDR, value1);

		eri_cmd = ERIAR_Write | transformed_base_address |
			  type << ERIAR_Type_shift |
			  ERIAR_ByteEn << ERIAR_ByteEn_shift |
			  (addr & 0x0FFF);
		if (addr & 0xF000) {
			tmp = addr & 0xF000;
			tmp >>= 12;
			eri_cmd |= (tmp << 20) & 0x00F00000;
		}

		RTL_W32(hw, ERIAR, eri_cmd);

		for (i = 0; i < RTL_CHANNEL_WAIT_COUNT; i++) {
			rte_delay_us(RTL_CHANNEL_WAIT_TIME);

			/* Check if the NIC has completed ERI write */
			if (!(RTL_R32(hw, ERIAR) & ERIAR_Flag))
				break;
		}

		if (len <= 4 - val_shift) {
			len = 0;
		} else {
			len -= (4 - val_shift);
			shift = 4 - val_shift;
			addr += 4;
		}
	}

	rte_delay_us(RTL_CHANNEL_EXIT_DELAY_TIME);

	return 0;
}

int
rtl_eri_write(struct rtl_hw *hw, int addr, int len, u32 value, int type)
{
	return rtl_eri_write_with_oob_base_address(hw, addr, len, value, type,
						   NO_BASE_ADDRESS);
}

static u32
rtl_ocp_read_with_oob_base_address(struct rtl_hw *hw, u16 addr, u8 len,
				   const u32 base_address)
{
	return rtl_eri_read_with_oob_base_address(hw, addr, len, ERIAR_OOB,
						  base_address);
}

static u32
rtl8168_real_ocp_read(struct rtl_hw *hw, u16 addr, u8 len)
{
	int i, val_shift, shift = 0;
	u32 value1, value2, mask;

	value1 = 0;
	value2 = 0;

	if (len > 4 || len <= 0)
		return -1;

	while (len > 0) {
		val_shift = addr % 4;
		addr = addr & ~0x3;

		RTL_W32(hw, OCPAR, (0x0F << 12) | (addr & 0xFFF));

		for (i = 0; i < 20; i++) {
			rte_delay_us(100);
			if (RTL_R32(hw, OCPAR) & OCPAR_Flag)
				break;
		}

		if (len == 1)
			mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 2)
			mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 3)
			mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else
			mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

		value1 = RTL_R32(hw, OCPDR) & mask;
		value2 |= (value1 >> val_shift * 8) << shift * 8;

		if (len <= 4 - val_shift) {
			len = 0;
		} else {
			len -= (4 - val_shift);
			shift = 4 - val_shift;
			addr += 4;
		}
	}

	rte_delay_us(20);

	return value2;
}

static int
rtl8168_real_ocp_write(struct rtl_hw *hw, u16 addr, u8 len, u32 value)
{
	int i, val_shift, shift = 0;
	u32 mask, value1 = 0;

	if (len > 4 || len <= 0)
		return -1;

	while (len > 0) {
		val_shift = addr % 4;
		addr = addr & ~0x3;

		if (len == 1)
			mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 2)
			mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else if (len == 3)
			mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
		else
			mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

		value1 = rtl_ocp_read(hw, addr, 4) & ~mask;
		value1 |= ((value << val_shift * 8) >> shift * 8);

		RTL_W32(hw, OCPDR, value1);
		RTL_W32(hw, OCPAR, OCPAR_Flag | (0x0F << 12) | (addr & 0xFFF));

		for (i = 0; i < 10; i++) {
			rte_delay_us(100);

			/* Check if the RTL8168 has completed ERI write */
			if (!(RTL_R32(hw, OCPAR) & OCPAR_Flag))
				break;
		}

		if (len <= 4 - val_shift) {
			len = 0;
		} else {
			len -= (4 - val_shift);
			shift = 4 - val_shift;
			addr += 4;
		}
	}

	rte_delay_us(20);

	return 0;
}

u32
rtl_ocp_read(struct rtl_hw *hw, u16 addr, u8 len)
{
	u32 value = 0;

	if (rtl_is_8125(hw) && !hw->AllowAccessDashOcp)
		return 0xffffffff;

	if (hw->HwSuppOcpChannelVer == 2)
		value = rtl_ocp_read_with_oob_base_address(hw, addr, len,
							   NO_BASE_ADDRESS);
	else if (hw->HwSuppOcpChannelVer == 3)
		value = rtl_ocp_read_with_oob_base_address(hw, addr, len,
							   RTL8168FP_OOBMAC_BASE);
	else
		value = rtl8168_real_ocp_read(hw, addr, len);

	return value;
}

static u32
rtl_ocp_write_with_oob_base_address(struct rtl_hw *hw, u16 addr, u8 len,
				    u32 value, const u32 base_address)
{
	return rtl_eri_write_with_oob_base_address(hw, addr, len, value,
						   ERIAR_OOB, base_address);
}

void
rtl_ocp_write(struct rtl_hw *hw, u16 addr, u8 len, u32 value)
{
	if (rtl_is_8125(hw) && !hw->AllowAccessDashOcp)
		return;

	if (hw->HwSuppOcpChannelVer == 2)
		rtl_ocp_write_with_oob_base_address(hw, addr, len, value,
						    NO_BASE_ADDRESS);
	else if (hw->HwSuppOcpChannelVer == 3)
		rtl_ocp_write_with_oob_base_address(hw, addr, len, value,
						    RTL8168FP_OOBMAC_BASE);
	else
		rtl8168_real_ocp_write(hw, addr, len, value);
}

void
rtl_oob_mutex_lock(struct rtl_hw *hw)
{
	u8 reg_16, reg_a0;
	u16 ocp_reg_mutex_ib;
	u16 ocp_reg_mutex_oob;
	u16 ocp_reg_mutex_prio;
	u32 wait_cnt_0, wait_cnt_1;

	if (!hw->DASH)
		return;

	switch (hw->mcfg) {
	case CFG_METHOD_23:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_52:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_91:
		ocp_reg_mutex_oob = 0x110;
		ocp_reg_mutex_ib = 0x114;
		ocp_reg_mutex_prio = 0x11C;
		break;
	default:
		return;
	}

	rtl_ocp_write(hw, ocp_reg_mutex_ib, 1, BIT_0);
	reg_16 = rtl_ocp_read(hw, ocp_reg_mutex_oob, 1);
	wait_cnt_0 = 0;
	while (reg_16) {
		reg_a0 = rtl_ocp_read(hw, ocp_reg_mutex_prio, 1);
		if (reg_a0) {
			rtl_ocp_write(hw, ocp_reg_mutex_ib, 1, 0x00);
			reg_a0 = rtl_ocp_read(hw, ocp_reg_mutex_prio, 1);
			wait_cnt_1 = 0;
			while (reg_a0) {
				reg_a0 = rtl_ocp_read(hw, ocp_reg_mutex_prio, 1);

				wait_cnt_1++;

				if (wait_cnt_1 > 2000)
					break;
			};
			rtl_ocp_write(hw, ocp_reg_mutex_ib, 1, BIT_0);
		}
		reg_16 = rtl_ocp_read(hw, ocp_reg_mutex_oob, 1);

		wait_cnt_0++;

		if (wait_cnt_0 > 2000)
			break;
	};
}

void
rtl_oob_mutex_unlock(struct rtl_hw *hw)
{
	u16 ocp_reg_mutex_ib;
	u16 ocp_reg_mutex_prio;

	if (!hw->DASH)
		return;

	switch (hw->mcfg) {
	case CFG_METHOD_23:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_52:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_91:
		ocp_reg_mutex_ib = 0x114;
		ocp_reg_mutex_prio = 0x11C;
		break;
	default:
		return;
	}

	rtl_ocp_write(hw, ocp_reg_mutex_prio, 1, BIT_0);
	rtl_ocp_write(hw, ocp_reg_mutex_ib, 1, 0x00);
}

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

static void
rtl_clear_set_mac_ocp_bit(struct rtl_hw *hw, u16 addr, u16 clearmask,
			  u16 setmask)
{
	u16 val;

	val = rtl_mac_ocp_read(hw, addr);
	val &= ~clearmask;
	val |= setmask;
	rtl_mac_ocp_write(hw, addr, val);
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

u32
rtl_csi_other_fun_read(struct rtl_hw *hw, u8 multi_fun_sel_bit, u32 addr)
{
	u32 cmd;
	int i;
	u32 value = 0xffffffff;

	cmd = CSIAR_Read | CSIAR_ByteEn << CSIAR_ByteEn_shift |
	      (addr & CSIAR_Addr_Mask);

	if (multi_fun_sel_bit > 7)
		goto exit;

	cmd |= multi_fun_sel_bit << 16;

	RTL_W32(hw, CSIAR, cmd);

	for (i = 0; i < RTL_CHANNEL_WAIT_COUNT; i++) {
		rte_delay_us(RTL_CHANNEL_WAIT_TIME);

		/* Check if the NIC has completed CSI read */
		if (RTL_R32(hw, CSIAR) & CSIAR_Flag) {
			value = (u32)RTL_R32(hw, CSIDR);
			break;
		}
	}

	rte_delay_us(RTL_CHANNEL_EXIT_DELAY_TIME);

exit:
	return value;
}

u32
rtl_csi_read(struct rtl_hw *hw, u32 addr)
{
	u8 multi_fun_sel_bit;

	switch (hw->mcfg) {
	case CFG_METHOD_26:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		multi_fun_sel_bit = 1;
		break;
	default:
		multi_fun_sel_bit = 0;
		break;
	}

	return rtl_csi_other_fun_read(hw, multi_fun_sel_bit, addr);
}

void
rtl_csi_other_fun_write(struct rtl_hw *hw, u8 multi_fun_sel_bit, u32 addr,
			u32 value)
{
	u32 cmd;
	int i;

	RTL_W32(hw, CSIDR, value);
	cmd = CSIAR_Write | CSIAR_ByteEn << CSIAR_ByteEn_shift |
	      (addr & CSIAR_Addr_Mask);

	if (multi_fun_sel_bit > 7)
		return;

	cmd |= multi_fun_sel_bit << 16;

	RTL_W32(hw, CSIAR, cmd);

	for (i = 0; i < RTL_CHANNEL_WAIT_COUNT; i++) {
		rte_delay_us(RTL_CHANNEL_WAIT_TIME);

		/* Check if the NIC has completed CSI write */
		if (!(RTL_R32(hw, CSIAR) & CSIAR_Flag))
			break;
	}

	rte_delay_us(RTL_CHANNEL_EXIT_DELAY_TIME);
}

void
rtl_csi_write(struct rtl_hw *hw, u32 addr, u32 value)
{
	u8 multi_fun_sel_bit;

	switch (hw->mcfg) {
	case CFG_METHOD_26:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		multi_fun_sel_bit = 1;
		break;
	default:
		multi_fun_sel_bit = 0;
		break;
	}

	rtl_csi_other_fun_write(hw, multi_fun_sel_bit, addr, value);
}

void
rtl8168_clear_and_set_mcu_ocp_bit(struct rtl_hw *hw, u16 addr, u16 clearmask,
				  u16 setmask)
{
	u16 reg_value;

	reg_value = rtl_mac_ocp_read(hw, addr);
	reg_value &= ~clearmask;
	reg_value |= setmask;
	rtl_mac_ocp_write(hw, addr, reg_value);
}

void
rtl8168_clear_mcu_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask)
{
	rtl8168_clear_and_set_mcu_ocp_bit(hw, addr, mask, 0);
}

void
rtl8168_set_mcu_ocp_bit(struct rtl_hw *hw, u16 addr, u16 mask)
{
	rtl8168_clear_and_set_mcu_ocp_bit(hw, addr, 0, mask);
}

static void
rtl_enable_rxdvgate(struct rtl_hw *hw)
{
	RTL_W8(hw, 0xF2, RTL_R8(hw, 0xF2) | BIT_3);

	if (!rtl_is_8125(hw))
		rte_delay_ms(2);
}

void
rtl_disable_rxdvgate(struct rtl_hw *hw)
{
	RTL_W8(hw, 0xF2, RTL_R8(hw, 0xF2) & ~BIT_3);

	if (!rtl_is_8125(hw))
		rte_delay_ms(2);
}

static void
rtl_stop_all_request(struct rtl_hw *hw)
{
	int i;

	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_23:
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_26:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
		rte_delay_ms(2);
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_52:
		RTL_W8(hw, ChipCmd, RTL_R8(hw, ChipCmd) | StopReq);
		for (i = 0; i < 20; i++) {
			rte_delay_us(10);
			if (!(RTL_R8(hw, ChipCmd) & StopReq))
				break;
		}
		break;
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		RTL_W8(hw, ChipCmd, RTL_R8(hw, ChipCmd) | StopReq);
		rte_delay_us(200);
		break;
	}
}

static void
rtl_clear_stop_all_request(struct rtl_hw *hw)
{
	RTL_W8(hw, ChipCmd, RTL_R8(hw, ChipCmd) & (CmdTxEnb | CmdRxEnb));
}

static void
rtl_wait_txrx_fifo_empty(struct rtl_hw *hw)
{
	int i;

	if (rtl_is_8125(hw)) {
		for (i = 0; i < 3000; i++) {
			rte_delay_us(50);
			if ((RTL_R8(hw, MCUCmd_reg) & (Txfifo_empty | Rxfifo_empty)) ==
			    (Txfifo_empty | Rxfifo_empty))
				break;
		}
	} else {
		for (i = 0; i < 10; i++) {
			rte_delay_us(100);
			if (RTL_R32(hw, TxConfig) & BIT_11)
				break;
		}

		for (i = 0; i < 10; i++) {
			rte_delay_us(100);
			if ((RTL_R8(hw, MCUCmd_reg) & (Txfifo_empty | Rxfifo_empty)) ==
			    (Txfifo_empty | Rxfifo_empty))
				break;
		}

		rte_delay_ms(1);
	}

	switch (hw->mcfg) {
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		for (i = 0; i < 3000; i++) {
			rte_delay_us(50);
			if ((RTL_R16(hw, IntrMitigate) & (BIT_0 | BIT_1 | BIT_8)) ==
			    (BIT_0 | BIT_1 | BIT_8))
				break;
		}
		break;
	}
}

static void
rtl_disable_rx_packet_filter(struct rtl_hw *hw)
{
	RTL_W32(hw, RxConfig, RTL_R32(hw, RxConfig) &
		~(AcceptErr | AcceptRunt | AcceptBroadcast | AcceptMulticast |
		AcceptMyPhys | AcceptAllPhys));
}

void
rtl_nic_reset(struct rtl_hw *hw)
{
	int i;

	rtl_disable_rx_packet_filter(hw);

	rtl_enable_rxdvgate(hw);

	rtl_stop_all_request(hw);

	rtl_wait_txrx_fifo_empty(hw);

	rtl_clear_stop_all_request(hw);

	/* Soft reset the chip. */
	RTL_W8(hw, ChipCmd, CmdReset);

	/* Check that the chip has finished the reset. */
	for (i = 100; i > 0; i--) {
		rte_delay_us(100);
		if ((RTL_R8(hw, ChipCmd) & CmdReset) == 0)
			break;
	}
}

void
rtl_enable_cfg9346_write(struct rtl_hw *hw)
{
	RTL_W8(hw, Cfg9346, RTL_R8(hw, Cfg9346) | Cfg9346_Unlock);
}

void
rtl_disable_cfg9346_write(struct rtl_hw *hw)
{
	RTL_W8(hw, Cfg9346, RTL_R8(hw, Cfg9346) & ~Cfg9346_Unlock);
}

static void
rtl_enable_force_clkreq(struct rtl_hw *hw, bool enable)
{
	if (enable)
		RTL_W8(hw, 0xF1, RTL_R8(hw, 0xF1) | BIT_7);
	else
		RTL_W8(hw, 0xF1, RTL_R8(hw, 0xF1) & ~BIT_7);
}

static void
rtl_enable_aspm_clkreq_lock(struct rtl_hw *hw, bool enable)
{
	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_23:
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_26:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
		if (enable) {
			RTL_W8(hw, Config2, RTL_R8(hw, Config2) | BIT_7);
			RTL_W8(hw, Config5, RTL_R8(hw, Config5) | BIT_0);
		} else {
			RTL_W8(hw, Config2, RTL_R8(hw, Config2) & ~BIT_7);
			RTL_W8(hw, Config5, RTL_R8(hw, Config5) & ~BIT_0);
		}
		rte_delay_us(10);
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_69:
		if (enable) {
			RTL_W8(hw, Config2, RTL_R8(hw, Config2) | BIT_7);
			RTL_W8(hw, Config5, RTL_R8(hw, Config5) | BIT_0);
		} else {
			RTL_W8(hw, Config2, RTL_R8(hw, Config2) & ~BIT_7);
			RTL_W8(hw, Config5, RTL_R8(hw, Config5) & ~BIT_0);
		}
		break;
	case CFG_METHOD_70:
	case CFG_METHOD_71:
		if (enable) {
			RTL_W8(hw, INT_CFG0_8125, RTL_R8(hw, INT_CFG0_8125) | BIT_3);
			RTL_W8(hw, Config5, RTL_R8(hw, Config5) | BIT_0);
		} else {
			RTL_W8(hw, INT_CFG0_8125, RTL_R8(hw, INT_CFG0_8125) & ~BIT_3);
			RTL_W8(hw, Config5, RTL_R8(hw, Config5) & ~BIT_0);
		}
		break;
	}
}

static void
rtl_disable_l1_timeout(struct rtl_hw *hw)
{
	rtl_csi_write(hw, 0x890, rtl_csi_read(hw, 0x890) & ~BIT_0);
}

static void
rtl8125_disable_eee_plus(struct rtl_hw *hw)
{
	rtl_mac_ocp_write(hw, 0xE080, rtl_mac_ocp_read(hw, 0xE080) & ~BIT_1);
}

static void
rtl_hw_clear_timer_int(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_23:
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_26:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
		RTL_W32(hw, TimeInt0, 0x0000);
		RTL_W32(hw, TimeInt1, 0x0000);
		RTL_W32(hw, TimeInt2, 0x0000);
		RTL_W32(hw, TimeInt3, 0x0000);
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		RTL_W32(hw, TIMER_INT0_8125, 0x0000);
		RTL_W32(hw, TIMER_INT1_8125, 0x0000);
		RTL_W32(hw, TIMER_INT2_8125, 0x0000);
		RTL_W32(hw, TIMER_INT3_8125, 0x0000);
		break;
	}
}

static void
rtl8125_hw_clear_int_miti(struct rtl_hw *hw)
{
	int i;

	switch (hw->HwSuppIntMitiVer) {
	case 3:
	case 6:
		/* IntMITI_0-IntMITI_31 */
		for (i = 0xA00; i < 0xB00; i += 4)
			RTL_W32(hw, i, 0x0000);
		break;
	case 4:
	case 5:
		/* IntMITI_0-IntMITI_15 */
		for (i = 0xA00; i < 0xA80; i += 4)
			RTL_W32(hw, i, 0x0000);

		if (hw->HwSuppIntMitiVer == 5)
			RTL_W8(hw, INT_CFG0_8125, RTL_R8(hw, INT_CFG0_8125) &
			       ~(INT_CFG0_TIMEOUT0_BYPASS_8125 |
			       INT_CFG0_MITIGATION_BYPASS_8125 |
			       INT_CFG0_RDU_BYPASS_8126));
		else
			RTL_W8(hw, INT_CFG0_8125, RTL_R8(hw, INT_CFG0_8125) &
			       ~(INT_CFG0_TIMEOUT0_BYPASS_8125 | INT_CFG0_MITIGATION_BYPASS_8125));

		RTL_W16(hw, INT_CFG1_8125, 0x0000);
		break;
	}
}

static void
rtl8125_hw_config(struct rtl_hw *hw)
{
	u32 mac_ocp_data;

	rtl_nic_reset(hw);

	rtl_enable_cfg9346_write(hw);

	/* Disable aspm clkreq internal */
	rtl_enable_force_clkreq(hw, 0);
	rtl_enable_aspm_clkreq_lock(hw, 0);

	/* Disable magic packet */
	rtl_mac_ocp_write(hw, 0xC0B6, 0);

	/* Set DMA burst size and interframe gap time */
	RTL_W32(hw, TxConfig, (TX_DMA_BURST_unlimited << TxDMAShift) |
		(InterFrameGap << TxInterFrameGapShift));

	if (hw->EnableTxNoClose)
		RTL_W32(hw, TxConfig, (RTL_R32(hw, TxConfig) | BIT_6));

	/* TCAM */
	switch (hw->mcfg) {
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
		RTL_W16(hw, 0x382, 0x221B);
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		rtl_disable_l1_timeout(hw);
		break;
	}

	/* RSS_control_0 */
	RTL_W32(hw, RSS_CTRL_8125, 0x00);

	/* VMQ_control */
	RTL_W16(hw, Q_NUM_CTRL_8125, 0x0000);

	/* Disable speed down */
	RTL_W8(hw, Config1, RTL_R8(hw, Config1) & ~0x10);

	/* CRC disable set */
	rtl_mac_ocp_write(hw, 0xC140, 0xFFFF);
	rtl_mac_ocp_write(hw, 0xC142, 0xFFFF);

	/* Disable new TX desc format */
	mac_ocp_data = rtl_mac_ocp_read(hw, 0xEB58);
	if (hw->mcfg == CFG_METHOD_70 || hw->mcfg == CFG_METHOD_71 ||
	    hw->mcfg == CFG_METHOD_91)
		mac_ocp_data &= ~(BIT_0 | BIT_1);
	else
		mac_ocp_data &= ~BIT_0;
	rtl_mac_ocp_write(hw, 0xEB58, mac_ocp_data);

	if (hw->mcfg >= CFG_METHOD_91) {
		if (hw->EnableTxNoClose)
			RTL_W8(hw, 0x20E4, RTL_R8(hw, 0x20E4) | BIT_2);
		else
			RTL_W8(hw, 0x20E4, RTL_R8(hw, 0x20E4) & ~BIT_2);
	}

	switch (hw->mcfg) {
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		RTL_W8(hw, 0xD8, RTL_R8(hw, 0xD8) & ~EnableRxDescV4_0);
		break;
	}

	if (hw->mcfg >= CFG_METHOD_91) {
		rtl_clear_mac_ocp_bit(hw, 0xE00C, BIT_12);
		rtl_clear_mac_ocp_bit(hw, 0xC0C2, BIT_6);
	}

	mac_ocp_data = rtl_mac_ocp_read(hw, 0xE63E);
	mac_ocp_data &= ~(BIT_5 | BIT_4);
	if (hw->mcfg == CFG_METHOD_48 || hw->mcfg == CFG_METHOD_49 ||
	    hw->mcfg == CFG_METHOD_52 || hw->mcfg == CFG_METHOD_69 ||
	    hw->mcfg == CFG_METHOD_70 || hw->mcfg == CFG_METHOD_71 ||
	    hw->mcfg == CFG_METHOD_91)
		mac_ocp_data |= ((0x02 & 0x03) << 4);
	rtl_mac_ocp_write(hw, 0xE63E, mac_ocp_data);

	/*
	 * FTR_MCU_CTRL
	 * 3-2 txpla packet valid start
	 */
	mac_ocp_data = rtl_mac_ocp_read(hw, 0xC0B4);
	mac_ocp_data &= ~BIT_0;
	rtl_mac_ocp_write(hw, 0xC0B4, mac_ocp_data);
	mac_ocp_data |= BIT_0;
	rtl_mac_ocp_write(hw, 0xC0B4, mac_ocp_data);

	mac_ocp_data = rtl_mac_ocp_read(hw, 0xC0B4);
	mac_ocp_data |= (BIT_3 | BIT_2);
	rtl_mac_ocp_write(hw, 0xC0B4, mac_ocp_data);

	mac_ocp_data = rtl_mac_ocp_read(hw, 0xEB6A);
	mac_ocp_data &= ~(BIT_7 | BIT_6 | BIT_5 | BIT_4 | BIT_3 | BIT_2 |
			  BIT_1 | BIT_0);
	mac_ocp_data |= (BIT_5 | BIT_4 | BIT_1 | BIT_0);
	rtl_mac_ocp_write(hw, 0xEB6A, mac_ocp_data);

	mac_ocp_data = rtl_mac_ocp_read(hw, 0xEB50);
	mac_ocp_data &= ~(BIT_9 | BIT_8 | BIT_7 | BIT_6 | BIT_5);
	mac_ocp_data |= BIT_6;
	rtl_mac_ocp_write(hw, 0xEB50, mac_ocp_data);

	mac_ocp_data = rtl_mac_ocp_read(hw, 0xE056);
	mac_ocp_data &= ~(BIT_7 | BIT_6 | BIT_5 | BIT_4);
	rtl_mac_ocp_write(hw, 0xE056, mac_ocp_data);

	/* EEE_CR */
	mac_ocp_data = rtl_mac_ocp_read(hw, 0xE040);
	mac_ocp_data &= ~BIT_12;
	rtl_mac_ocp_write(hw, 0xE040, mac_ocp_data);

	mac_ocp_data = rtl_mac_ocp_read(hw, 0xEA1C);
	mac_ocp_data &= ~(BIT_1 | BIT_0);
	mac_ocp_data |= BIT_0;
	rtl_mac_ocp_write(hw, 0xEA1C, mac_ocp_data);

	rtl_oob_mutex_lock(hw);

	/* MAC_PWRDWN_CR0 */
	rtl_mac_ocp_write(hw, 0xE0C0, 0x4000);

	rtl_set_mac_ocp_bit(hw, 0xE052, (BIT_6 | BIT_5));
	rtl_clear_mac_ocp_bit(hw, 0xE052, (BIT_3 | BIT_7));

	rtl_oob_mutex_unlock(hw);

	/*
	 * DMY_PWR_REG_0
	 * (1)ERI(0xD4)(OCP 0xC0AC).bit[7:12]=6'b111111, L1 Mask
	 */
	rtl_set_mac_ocp_bit(hw, 0xC0AC, (BIT_7 | BIT_8 | BIT_9 | BIT_10 |
					 BIT_11 | BIT_12));

	mac_ocp_data = rtl_mac_ocp_read(hw, 0xD430);
	mac_ocp_data &= ~(BIT_11 | BIT_10 | BIT_9 | BIT_8 | BIT_7 | BIT_6 |
			  BIT_5 | BIT_4 | BIT_3 | BIT_2 | BIT_1 | BIT_0);
	mac_ocp_data |= 0x45F;
	rtl_mac_ocp_write(hw, 0xD430, mac_ocp_data);

	if (!hw->DASH)
		RTL_W8(hw, 0xD0, RTL_R8(hw, 0xD0) | BIT_6 | BIT_7);
	else
		RTL_W8(hw, 0xD0, RTL_R8(hw, 0xD0) & ~(BIT_6 | BIT_7));

	if (hw->mcfg == CFG_METHOD_48 || hw->mcfg == CFG_METHOD_49 ||
	    hw->mcfg == CFG_METHOD_52)
		RTL_W8(hw, MCUCmd_reg, RTL_R8(hw, MCUCmd_reg) | BIT_0);

	rtl8125_disable_eee_plus(hw);

	mac_ocp_data = rtl_mac_ocp_read(hw, 0xEA1C);
	mac_ocp_data &= ~BIT_2;
	if (hw->mcfg == CFG_METHOD_70 || hw->mcfg == CFG_METHOD_71 ||
	    hw->mcfg == CFG_METHOD_91)
		mac_ocp_data &= ~(BIT_9 | BIT_8);
	rtl_mac_ocp_write(hw, 0xEA1C, mac_ocp_data);

	/* Clear TCAM entries */
	rtl_set_mac_ocp_bit(hw, 0xEB54, BIT_0);
	rte_delay_us(1);
	rtl_clear_mac_ocp_bit(hw, 0xEB54, BIT_0);

	RTL_W16(hw, 0x1880, RTL_R16(hw, 0x1880) & ~(BIT_4 | BIT_5));

	if (hw->mcfg == CFG_METHOD_91)
		rtl_clear_set_mac_ocp_bit(hw, 0xD40C, 0xE038, 0x8020);

	/* Other hw parameters */
	rtl_hw_clear_timer_int(hw);

	rtl8125_hw_clear_int_miti(hw);

	rtl_mac_ocp_write(hw, 0xE098, 0xC302);

	rtl_disable_cfg9346_write(hw);

	rte_delay_us(10);
}

static void
rtl8168_hw_config(struct rtl_hw *hw)
{
	u32 csi_tmp;
	int timeout;

	rtl_nic_reset(hw);

	rtl_enable_cfg9346_write(hw);

	/* Disable aspm clkreq internal */
	rtl_enable_force_clkreq(hw, 0);
	rtl_enable_aspm_clkreq_lock(hw, 0);

	/* Clear io_rdy_l23 */
	RTL_W8(hw, Config3, RTL_R8(hw, Config3) & ~BIT_1);

	/* Keep magic packet only */
	csi_tmp = rtl_eri_read(hw, 0xDE, 1, ERIAR_ExGMAC);
	csi_tmp &= BIT_0;
	rtl_eri_write(hw, 0xDE, 1, csi_tmp, ERIAR_ExGMAC);

	/* Set TxConfig to default */
	RTL_W32(hw, TxConfig, (TX_DMA_BURST_unlimited << TxDMAShift) |
		(InterFrameGap << TxInterFrameGapShift));

	hw->hw_ops.hw_config(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_23:
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_26:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
		rtl_eri_write(hw, 0x2F8, 2, 0x1D8F, ERIAR_ExGMAC);
		break;
	}

	rtl_hw_clear_timer_int(hw);

	/* Clkreq exit masks */
	csi_tmp = rtl_eri_read(hw, 0xD4, 4, ERIAR_ExGMAC);
	csi_tmp |= (BIT_7 | BIT_8 | BIT_9 | BIT_10 | BIT_11 | BIT_12);
	rtl_eri_write(hw, 0xD4, 4, csi_tmp, ERIAR_ExGMAC);

	switch (hw->mcfg) {
	case CFG_METHOD_25:
		rtl_mac_ocp_write(hw, 0xD3C0, 0x0B00);
		rtl_mac_ocp_write(hw, 0xD3C2, 0x0000);
		break;
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
		rtl_mac_ocp_write(hw, 0xE098, 0x0AA2);
		break;
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		rtl_mac_ocp_write(hw, 0xE098, 0xC302);
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_26:
		for (timeout = 0; timeout < 10; timeout++) {
			if ((rtl_eri_read(hw, 0x1AE, 2, ERIAR_ExGMAC) & BIT_13) == 0)
				break;
			rte_delay_ms(1);
		}
		break;
	}

	rtl_disable_cfg9346_write(hw);

	rte_delay_us(10);
}

void
rtl_hw_config(struct rtl_hw *hw)
{
	if (rtl_is_8125(hw))
		rtl8125_hw_config(hw);
	else
		rtl8168_hw_config(hw);
}

int
rtl_set_hw_ops(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	/* 8168G */
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	/* 8168GU */
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		hw->hw_ops = rtl8168g_ops;
		return 0;
	/* 8168EP */
	case CFG_METHOD_23:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
		hw->hw_ops = rtl8168ep_ops;
		return 0;
	/* 8168H */
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
		hw->hw_ops = rtl8168h_ops;
		return 0;
	/* 8168FP */
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		hw->hw_ops = rtl8168fp_ops;
		return 0;
	/* 8168M */
	case CFG_METHOD_37:
		hw->hw_ops = rtl8168m_ops;
		return 0;
	/* 8125A */
	case CFG_METHOD_48:
	case CFG_METHOD_49:
		hw->hw_ops = rtl8125a_ops;
		return 0;
	/* 8125B */
	case CFG_METHOD_50:
	case CFG_METHOD_51:
		hw->hw_ops = rtl8125b_ops;
		return 0;
	/* 8168KB */
	case CFG_METHOD_52:
	case CFG_METHOD_53:
		hw->hw_ops = rtl8168kb_ops;
		return 0;
	/* 8125BP */
	case CFG_METHOD_54:
	case CFG_METHOD_55:
		hw->hw_ops = rtl8125bp_ops;
		return 0;
	/* 8125D */
	case CFG_METHOD_56:
	case CFG_METHOD_57:
		hw->hw_ops = rtl8125d_ops;
		return 0;
	/* 8126A */
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
		hw->hw_ops = rtl8126a_ops;
		return 0;
	case CFG_METHOD_91:
		hw->hw_ops = rtl8127_ops;
		return 0;
	default:
		return -ENOTSUP;
	}
}

void
rtl_hw_disable_mac_mcu_bps(struct rtl_hw *hw)
{
	u16 reg_addr;

	rtl_enable_cfg9346_write(hw);
	rtl_enable_aspm_clkreq_lock(hw, 0);
	rtl_disable_cfg9346_write(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
		rtl_mac_ocp_write(hw, 0xFC38, 0x0000);
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		rtl_mac_ocp_write(hw, 0xFC48, 0x0000);
		break;
	}

	if (rtl_is_8125(hw)) {
		for (reg_addr = 0xFC28; reg_addr < 0xFC48; reg_addr += 2)
			rtl_mac_ocp_write(hw, reg_addr, 0x0000);
	} else {
		for (reg_addr = 0xFC28; reg_addr < 0xFC38; reg_addr += 2)
			rtl_mac_ocp_write(hw, reg_addr, 0x0000);
	}

	rte_delay_ms(3);
	rtl_mac_ocp_write(hw, 0xFC26, 0x0000);
}

static void
rtl_switch_mac_mcu_ram_code_page(struct rtl_hw *hw, u16 page)
{
	u16 tmp_ushort;

	page &= (BIT_1 | BIT_0);
	tmp_ushort = rtl_mac_ocp_read(hw, 0xE446);
	tmp_ushort &= ~(BIT_1 | BIT_0);
	tmp_ushort |= page;
	rtl_mac_ocp_write(hw, 0xE446, tmp_ushort);
}

static void
_rtl_write_mac_mcu_ram_code(struct rtl_hw *hw, const u16 *entry, u16 entry_cnt)
{
	u16 i;

	for (i = 0; i < entry_cnt; i++)
		rtl_mac_ocp_write(hw, 0xF800 + i * 2, entry[i]);
}

static void
_rtl_write_mac_mcu_ram_code_with_page(struct rtl_hw *hw, const u16 *entry,
				      u16 entry_cnt, u16 page_size)
{
	u16 i;
	u16 offset;
	u16 page;

	if (page_size == 0)
		return;

	for (i = 0; i < entry_cnt; i++) {
		offset = i % page_size;
		if (offset == 0) {
			page = (i / page_size);
			rtl_switch_mac_mcu_ram_code_page(hw, page);
		}
		rtl_mac_ocp_write(hw, 0xF800 + offset * 2, entry[i]);
	}
}

static void
_rtl_set_hw_mcu_patch_code_ver(struct rtl_hw *hw, u64 ver)
{
	int i;

	/* Switch to page 2 */
	rtl_switch_mac_mcu_ram_code_page(hw, 2);

	for (i = 0; i < 8; i += 2) {
		rtl_mac_ocp_write(hw, 0xF9F8 + 6 - i, (u16)ver);
		ver >>= 16;
	}

	/* Switch back to page 0 */
	rtl_switch_mac_mcu_ram_code_page(hw, 0);
}

static void
rtl_set_hw_mcu_patch_code_ver(struct rtl_hw *hw, u64 ver)
{
	_rtl_set_hw_mcu_patch_code_ver(hw, ver);

	hw->hw_mcu_patch_code_ver = ver;
}

void
rtl_write_mac_mcu_ram_code(struct rtl_hw *hw, const u16 *entry, u16 entry_cnt)
{
	if (!HW_SUPPORT_MAC_MCU(hw))
		return;
	if (!entry || entry_cnt == 0)
		return;

	if (hw->MacMcuPageSize > 0)
		_rtl_write_mac_mcu_ram_code_with_page(hw, entry, entry_cnt,
						      hw->MacMcuPageSize);
	else
		_rtl_write_mac_mcu_ram_code(hw, entry, entry_cnt);

	if (hw->bin_mcu_patch_code_ver > 0)
		rtl_set_hw_mcu_patch_code_ver(hw, hw->bin_mcu_patch_code_ver);
}

bool
rtl_is_speed_mode_valid(u32 speed)
{
	switch (speed) {
	case SPEED_10000:
	case SPEED_5000:
	case SPEED_2500:
	case SPEED_1000:
	case SPEED_100:
	case SPEED_10:
		return true;
	default:
		return false;
	}
}

static bool
rtl_is_duplex_mode_valid(u8 duplex)
{
	switch (duplex) {
	case DUPLEX_FULL:
	case DUPLEX_HALF:
		return true;
	default:
		return false;
	}
}

static bool
rtl_is_autoneg_mode_valid(u32 autoneg)
{
	switch (autoneg) {
	case AUTONEG_ENABLE:
	case AUTONEG_DISABLE:
		return true;
	default:
		return false;
	}
}

void
rtl_set_link_option(struct rtl_hw *hw, u8 autoneg, u32 speed, u8 duplex,
		    enum rtl_fc_mode fc)
{
	u64 adv;

	if (!rtl_is_speed_mode_valid(speed))
		speed = hw->HwSuppMaxPhyLinkSpeed;

	if (!rtl_is_duplex_mode_valid(duplex))
		duplex = DUPLEX_FULL;

	if (!rtl_is_autoneg_mode_valid(autoneg))
		autoneg = AUTONEG_ENABLE;

	speed = RTE_MIN(speed, hw->HwSuppMaxPhyLinkSpeed);

	adv = 0;
	switch (speed) {
	case SPEED_10000:
		adv |= ADVERTISE_10000_FULL;
	/* Fall through */
	case SPEED_5000:
		adv |= ADVERTISE_5000_FULL;
	/* Fall through */
	case SPEED_2500:
		adv |= ADVERTISE_2500_FULL;
	/* Fall through */
	default:
		adv |= (ADVERTISE_10_HALF | ADVERTISE_10_FULL |
			ADVERTISE_100_HALF | ADVERTISE_100_FULL |
			ADVERTISE_1000_HALF | ADVERTISE_1000_FULL);
		break;
	}

	hw->autoneg = autoneg;
	hw->speed = speed;
	hw->duplex = duplex;
	hw->advertising = adv;
	hw->fcpause = fc;
}

static void
rtl_init_software_variable(struct rtl_hw *hw)
{
	int tx_no_close_enable = 1;
	unsigned int speed_mode;
	unsigned int duplex_mode = DUPLEX_FULL;
	unsigned int autoneg_mode = AUTONEG_ENABLE;
	u32 tmp;

	switch (hw->mcfg) {
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
		speed_mode = SPEED_2500;
		break;
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
		speed_mode = SPEED_5000;
		break;
	case CFG_METHOD_91:
		speed_mode = SPEED_10000;
		break;
	default:
		speed_mode = SPEED_1000;
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_23:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
		hw->HwSuppDashVer = 2;
		break;
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		hw->HwSuppDashVer = 3;
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_91:
		tmp = (u8)rtl_mac_ocp_read(hw, 0xD006);
		if (tmp == 0x02 || tmp == 0x04)
			hw->HwSuppDashVer = 2;
		else if (tmp == 0x03)
			hw->HwSuppDashVer = 4;
		break;
	case CFG_METHOD_54:
	case CFG_METHOD_55:
		hw->HwSuppDashVer = 4;
		break;
	default:
		hw->HwSuppDashVer = 0;
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		tmp = rtl_mac_ocp_read(hw, 0xDC00);
		hw->HwPkgDet = (tmp >> 3) & 0x0F;
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		if (hw->HwPkgDet == 0x06) {
			tmp = rtl_eri_read(hw, 0xE6, 1, ERIAR_ExGMAC);
			if (tmp == 0x02)
				hw->HwSuppSerDesPhyVer = 1;
			else if (tmp == 0x00)
				hw->HwSuppSerDesPhyVer = 2;
		}
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_23:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
		hw->HwSuppOcpChannelVer = 2;
		break;
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		hw->HwSuppOcpChannelVer = 3;
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_91:
		if (HW_DASH_SUPPORT_DASH(hw))
			hw->HwSuppOcpChannelVer = 2;
		break;
	default:
		hw->HwSuppOcpChannelVer = 0;
		break;
	}

	if (rtl_is_8125(hw))
		hw->AllowAccessDashOcp = rtl_is_allow_access_dash_ocp(hw);

	if (HW_DASH_SUPPORT_DASH(hw) && rtl_check_dash(hw))
		hw->DASH = 1;
	else
		hw->DASH = 0;

	if (HW_DASH_SUPPORT_TYPE_2(hw))
		hw->cmac_ioaddr = hw->mmio_addr;

	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		hw->chipset_name = RTL8168G;
		break;
	case CFG_METHOD_23:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
		hw->chipset_name = RTL8168EP;
		break;
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
		hw->chipset_name = RTL8168H;
		break;
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		hw->chipset_name = RTL8168FP;
		break;
	case CFG_METHOD_37:
		hw->chipset_name = RTL8168M;
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
		hw->chipset_name = RTL8125A;
		break;
	case CFG_METHOD_50:
	case CFG_METHOD_51:
		hw->chipset_name = RTL8125B;
		break;
	case CFG_METHOD_52:
	case CFG_METHOD_53:
		hw->chipset_name = RTL8168KB;
		break;
	case CFG_METHOD_54:
	case CFG_METHOD_55:
		hw->chipset_name = RTL8125BP;
		break;
	case CFG_METHOD_56:
	case CFG_METHOD_57:
		hw->chipset_name = RTL8125D;
		break;
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
		hw->chipset_name = RTL8126A;
		break;
	case CFG_METHOD_91:
		hw->chipset_name = RTL8127;
		break;
	default:
		hw->chipset_name = UNKNOWN;
		break;
	}

	hw->HwSuppNowIsOobVer = 1;

	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_26:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
		hw->HwSuppCheckPhyDisableModeVer = 2;
		break;
	case CFG_METHOD_23:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		hw->HwSuppCheckPhyDisableModeVer = 3;
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
		hw->HwSuppMaxPhyLinkSpeed = SPEED_2500;
		break;
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
		hw->HwSuppMaxPhyLinkSpeed = SPEED_5000;
		break;
	case CFG_METHOD_91:
		hw->HwSuppMaxPhyLinkSpeed = SPEED_10000;
		break;
	default:
		hw->HwSuppMaxPhyLinkSpeed = SPEED_1000;
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
		hw->HwSuppTxNoCloseVer = 3;
		break;
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_91:
		hw->HwSuppTxNoCloseVer = 6;
		break;
	case CFG_METHOD_69:
		hw->HwSuppTxNoCloseVer = 4;
		break;
	case CFG_METHOD_70:
	case CFG_METHOD_71:
		hw->HwSuppTxNoCloseVer = 5;
		break;
	}

	switch (hw->HwSuppTxNoCloseVer) {
	case 5:
	case 6:
		hw->MaxTxDescPtrMask = MAX_TX_NO_CLOSE_DESC_PTR_MASK_V4;
		break;
	case 4:
		hw->MaxTxDescPtrMask = MAX_TX_NO_CLOSE_DESC_PTR_MASK_V3;
		break;
	case 3:
		hw->MaxTxDescPtrMask = MAX_TX_NO_CLOSE_DESC_PTR_MASK_V2;
		break;
	default:
		tx_no_close_enable = 0;
		break;
	}

	if (hw->HwSuppTxNoCloseVer > 0 && tx_no_close_enable == 1)
		hw->EnableTxNoClose = TRUE;

	switch (hw->HwSuppTxNoCloseVer) {
	case 4:
	case 5:
		hw->hw_clo_ptr_reg = HW_CLO_PTR0_8126;
		hw->sw_tail_ptr_reg = SW_TAIL_PTR0_8126;
		break;
	case 6:
		hw->hw_clo_ptr_reg = HW_CLO_PTR0_8125BP;
		hw->sw_tail_ptr_reg = SW_TAIL_PTR0_8125BP;
		break;
	default:
		hw->hw_clo_ptr_reg = HW_CLO_PTR0_8125;
		hw->sw_tail_ptr_reg = SW_TAIL_PTR0_8125;
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_21;
		break;
	case CFG_METHOD_23:
	case CFG_METHOD_27:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_23;
		break;
	case CFG_METHOD_24:
	case CFG_METHOD_25:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_24;
		break;
	case CFG_METHOD_26:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_26;
		break;
	case CFG_METHOD_28:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_28;
		break;
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_37:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_29;
		break;
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_31;
		break;
	case CFG_METHOD_35:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_35;
		break;
	case CFG_METHOD_36:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_36;
		break;
	case CFG_METHOD_48:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_48;
		break;
	case CFG_METHOD_49:
	case CFG_METHOD_52:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_49;
		break;
	case CFG_METHOD_50:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_50;
		break;
	case CFG_METHOD_51:
	case CFG_METHOD_53:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_51;
		break;
	case CFG_METHOD_54:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_54;
		break;
	case CFG_METHOD_55:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_55;
		break;
	case CFG_METHOD_56:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_56;
		break;
	case CFG_METHOD_57:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_57;
		break;
	case CFG_METHOD_69:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_69;
		break;
	case CFG_METHOD_70:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_70;
		break;
	case CFG_METHOD_71:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_71;
		break;
	case CFG_METHOD_91:
		hw->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_91;
		break;
	}

	if (hw->HwIcVerUnknown) {
		hw->NotWrRamCodeToMicroP = TRUE;
		hw->NotWrMcuPatchCode = TRUE;
	}

	if (rtl_is_8125(hw)) {
		hw->HwSuppMacMcuVer = 2;
		hw->MacMcuPageSize = RTL_MAC_MCU_PAGE_SIZE;
		hw->mcu_pme_setting = rtl_mac_ocp_read(hw, 0xE00A);
	}

	switch (hw->mcfg) {
	case CFG_METHOD_49:
	case CFG_METHOD_52:
		if ((rtl_mac_ocp_read(hw, 0xD442) & BIT_5) &&
		    (rtl_mdio_direct_read_phy_ocp(hw, 0xD068) & BIT_1))
			hw->RequirePhyMdiSwapPatch = TRUE;
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_52:
		hw->HwSuppIntMitiVer = 3;
		break;
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_53:
	case CFG_METHOD_69:
		hw->HwSuppIntMitiVer = 4;
		break;
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_91:
		hw->HwSuppIntMitiVer = 6;
		break;
	case CFG_METHOD_70:
	case CFG_METHOD_71:
		hw->HwSuppIntMitiVer = 5;
		break;
	}

	rtl_set_link_option(hw, autoneg_mode, speed_mode, duplex_mode, rtl_fc_full);

	hw->mtu = RTL_DEFAULT_MTU;
}

static void
rtl_exit_realwow(struct rtl_hw *hw)
{
	u32 csi_tmp;

	/* Disable realwow function */
	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
		RTL_W32(hw, MACOCP, 0x605E0000);
		RTL_W32(hw, MACOCP, (0xE05E << 16) |
				    (RTL_R32(hw, MACOCP) & 0xFFFE));
		RTL_W32(hw, MACOCP, 0xE9720000);
		RTL_W32(hw, MACOCP, 0xF2140010);
		break;
	case CFG_METHOD_26:
		RTL_W32(hw, MACOCP, 0xE05E00FF);
		RTL_W32(hw, MACOCP, 0xE9720000);
		rtl_mac_ocp_write(hw, 0xE428, 0x0010);
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		rtl_mac_ocp_write(hw, 0xC0BC, 0x00FF);
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
		rtl_eri_write(hw, 0x174, 2, 0x0000, ERIAR_ExGMAC);
		rtl_mac_ocp_write(hw, 0xE428, 0x0010);
		break;
	case CFG_METHOD_24:
	case CFG_METHOD_25:
	case CFG_METHOD_26:
	case CFG_METHOD_28:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		rtl_eri_write(hw, 0x174, 2, 0x00FF, ERIAR_ExGMAC);
		rtl_mac_ocp_write(hw, 0xE428, 0x0010);
		break;
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
		csi_tmp = rtl_eri_read(hw, 0x174, 2, ERIAR_ExGMAC);
		csi_tmp &= ~BIT_8;
		csi_tmp |= BIT_15;
		rtl_eri_write(hw, 0x174, 2, csi_tmp, ERIAR_ExGMAC);
		rtl_mac_ocp_write(hw, 0xE428, 0x0010);
		break;
	}
}

static void
rtl_disable_now_is_oob(struct rtl_hw *hw)
{
	if (hw->HwSuppNowIsOobVer == 1)
		RTL_W8(hw, MCUCmd_reg, RTL_R8(hw, MCUCmd_reg) & ~Now_is_oob);
}

static void
rtl_wait_ll_share_fifo_ready(struct rtl_hw *hw)
{
	int i;

	for (i = 0; i < 10; i++) {
		rte_delay_us(100);
		if (RTL_R16(hw, 0xD2) & BIT_9)
			break;
	}
}

static void
rtl8168_switch_to_sgmii_mode(struct rtl_hw *hw)
{
	rtl_mac_ocp_write(hw, 0xEB00, 0x2);
	rtl8168_set_mcu_ocp_bit(hw, 0xEB16, BIT_1);
}

static void
rtl_exit_oob(struct rtl_hw *hw)
{
	u16 data16;

	rtl_disable_rx_packet_filter(hw);

	if (HW_SUPP_SERDES_PHY(hw)) {
		if (hw->HwSuppSerDesPhyVer == 1)
			rtl8168_switch_to_sgmii_mode(hw);
	}

	if (HW_DASH_SUPPORT_DASH(hw)) {
		rtl_driver_start(hw);
		rtl_dash2_disable_txrx(hw);
	}

	rtl_exit_realwow(hw);

	rtl_nic_reset(hw);

	rtl_disable_now_is_oob(hw);

	data16 = rtl_mac_ocp_read(hw, 0xE8DE) & ~BIT_14;
	rtl_mac_ocp_write(hw, 0xE8DE, data16);
	rtl_wait_ll_share_fifo_ready(hw);

	if (rtl_is_8125(hw)) {
		rtl_mac_ocp_write(hw, 0xC0AA, 0x07D0);

		rtl_mac_ocp_write(hw, 0xC0A6, 0x01B5);

		rtl_mac_ocp_write(hw, 0xC01E, 0x5555);

	} else {
		data16 = rtl_mac_ocp_read(hw, 0xE8DE) | BIT_15;
		rtl_mac_ocp_write(hw, 0xE8DE, data16);
	}

	rtl_wait_ll_share_fifo_ready(hw);
}

static void
rtl_disable_ups(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
		rtl_mac_ocp_write(hw, 0xD400,
				  rtl_mac_ocp_read(hw, 0xD400) & ~BIT_0);
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
	case CFG_METHOD_56:
	case CFG_METHOD_57:
	case CFG_METHOD_69:
	case CFG_METHOD_70:
	case CFG_METHOD_71:
	case CFG_METHOD_91:
		rtl_mac_ocp_write(hw, 0xD40A,
				  rtl_mac_ocp_read(hw, 0xD40A) & ~BIT_4);
		break;
	}
}

static void
rtl_disable_ocp_phy_power_saving(struct rtl_hw *hw)
{
	u16 val;

	switch (hw->mcfg) {
	case CFG_METHOD_25:
	case CFG_METHOD_26:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
		val = rtl_mdio_real_read_phy_ocp(hw, 0x0C41, 0x13);
		if (val != 0x0500) {
			rtl_set_phy_mcu_patch_request(hw);
			rtl_mdio_real_write_phy_ocp(hw, 0x0C41, 0x13, 0x0000);
			rtl_mdio_real_write_phy_ocp(hw, 0x0C41, 0x13, 0x0500);
			rtl_clear_phy_mcu_patch_request(hw);
		}
		break;
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_52:
		val = rtl_mdio_direct_read_phy_ocp(hw, 0xC416);
		if (val != 0x0050) {
			rtl_set_phy_mcu_patch_request(hw);
			rtl_mdio_direct_write_phy_ocp(hw, 0xC416, 0x0000);
			rtl_mdio_direct_write_phy_ocp(hw, 0xC416, 0x0500);
			rtl_clear_phy_mcu_patch_request(hw);
		}
		break;
	}
}

static void
rtl8168_disable_dma_agg(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
		rtl_mac_ocp_write(hw, 0xE63E, rtl_mac_ocp_read(hw, 0xE63E) &
					      ~(BIT_3 | BIT_2 | BIT_1));
		rtl_mac_ocp_write(hw, 0xE63E,
				  rtl_mac_ocp_read(hw, 0xE63E) | (BIT_0));
		rtl_mac_ocp_write(hw, 0xE63E,
				  rtl_mac_ocp_read(hw, 0xE63E) & ~(BIT_0));
		rtl_mac_ocp_write(hw, 0xC094, 0x0);
		rtl_mac_ocp_write(hw, 0xC09E, 0x0);
		break;
	}
}

static void
rtl_hw_init(struct rtl_hw *hw)
{
	u32 csi_tmp;

	/* Disable aspm clkreq internal */
	rtl_enable_force_clkreq(hw, 0);
	rtl_enable_cfg9346_write(hw);
	rtl_enable_aspm_clkreq_lock(hw, 0);
	rtl_disable_cfg9346_write(hw);

	rtl_disable_ups(hw);

	/* Disable DMA aggregation */
	rtl8168_disable_dma_agg(hw);

	hw->hw_ops.hw_mac_mcu_config(hw);

	/* Disable ocp phy power saving */
	rtl_disable_ocp_phy_power_saving(hw);

	/* Set PCIE uncorrectable error status mask pcie 0x108 */
	csi_tmp = rtl_csi_read(hw, 0x108);
	csi_tmp |= BIT_20;
	rtl_csi_write(hw, 0x108, csi_tmp);

	/* MCU PME setting */
	switch (hw->mcfg) {
	case CFG_METHOD_21:
	case CFG_METHOD_22:
	case CFG_METHOD_23:
	case CFG_METHOD_24:
		csi_tmp = rtl_eri_read(hw, 0x1AB, 1, ERIAR_ExGMAC);
		csi_tmp |= (BIT_2 | BIT_3 | BIT_4 | BIT_5 | BIT_6 | BIT_7);
		rtl_eri_write(hw, 0x1AB, 1, csi_tmp, ERIAR_ExGMAC);
		break;
	case CFG_METHOD_25:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
	case CFG_METHOD_29:
	case CFG_METHOD_30:
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
	case CFG_METHOD_35:
	case CFG_METHOD_36:
	case CFG_METHOD_37:
		csi_tmp = rtl_eri_read(hw, 0x1AB, 1, ERIAR_ExGMAC);
		csi_tmp |= (BIT_3 | BIT_6);
		rtl_eri_write(hw, 0x1AB, 1, csi_tmp, ERIAR_ExGMAC);
		break;
	}
}

void
rtl_hw_initialize(struct rtl_hw *hw)
{
	rtl_init_software_variable(hw);

	rtl_exit_oob(hw);

	rtl_hw_init(hw);

	rtl_nic_reset(hw);
}

void
rtl_get_mac_version(struct rtl_hw *hw, struct rte_pci_device *pci_dev)
{
	u32 reg, val32;
	u32 ic_version_id;

	val32 = RTL_R32(hw, TxConfig);
	reg = val32 & 0x7c800000;
	ic_version_id = val32 & 0x00700000;

	switch (reg) {
	case 0x30000000:
		hw->mcfg = CFG_METHOD_1;
		break;
	case 0x38000000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_2;
		} else if (ic_version_id == 0x00500000) {
			hw->mcfg = CFG_METHOD_3;
		} else {
			hw->mcfg = CFG_METHOD_3;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x3C000000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_4;
		} else if (ic_version_id == 0x00200000) {
			hw->mcfg = CFG_METHOD_5;
		} else if (ic_version_id == 0x00400000) {
			hw->mcfg = CFG_METHOD_6;
		} else {
			hw->mcfg = CFG_METHOD_6;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x3C800000:
		if (ic_version_id == 0x00100000) {
			hw->mcfg = CFG_METHOD_7;
		} else if (ic_version_id == 0x00300000) {
			hw->mcfg = CFG_METHOD_8;
		} else {
			hw->mcfg = CFG_METHOD_8;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x28000000:
		if (ic_version_id == 0x00100000) {
			hw->mcfg = CFG_METHOD_9;
		} else if (ic_version_id == 0x00300000) {
			hw->mcfg = CFG_METHOD_10;
		} else {
			hw->mcfg = CFG_METHOD_10;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x28800000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_11;
		} else if (ic_version_id == 0x00200000) {
			hw->mcfg = CFG_METHOD_12;
			RTL_W32(hw, 0xD0, RTL_R32(hw, 0xD0) | 0x00020000);
		} else if (ic_version_id == 0x00300000) {
			hw->mcfg = CFG_METHOD_13;
		} else {
			hw->mcfg = CFG_METHOD_13;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x2C000000:
		if (ic_version_id == 0x00100000) {
			hw->mcfg = CFG_METHOD_14;
		} else if (ic_version_id == 0x00200000) {
			hw->mcfg = CFG_METHOD_15;
		} else {
			hw->mcfg = CFG_METHOD_15;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x2C800000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_16;
		} else if (ic_version_id == 0x00100000) {
			hw->mcfg = CFG_METHOD_17;
		} else {
			hw->mcfg = CFG_METHOD_17;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x48000000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_18;
		} else if (ic_version_id == 0x00100000) {
			hw->mcfg = CFG_METHOD_19;
		} else {
			hw->mcfg = CFG_METHOD_19;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x48800000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_20;
		} else {
			hw->mcfg = CFG_METHOD_20;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x4C000000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_21;
		} else if (ic_version_id == 0x00100000) {
			hw->mcfg = CFG_METHOD_22;
		} else {
			hw->mcfg = CFG_METHOD_22;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x50000000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_23;
		} else if (ic_version_id == 0x00100000) {
			hw->mcfg = CFG_METHOD_27;
		} else if (ic_version_id == 0x00200000) {
			hw->mcfg = CFG_METHOD_28;
		} else {
			hw->mcfg = CFG_METHOD_28;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x50800000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_24;
		} else if (ic_version_id == 0x00100000) {
			hw->mcfg = CFG_METHOD_25;
		} else {
			hw->mcfg = CFG_METHOD_25;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x5C800000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_26;
		} else {
			hw->mcfg = CFG_METHOD_26;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x54000000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_29;
		} else if (ic_version_id == 0x00100000) {
			hw->mcfg = CFG_METHOD_30;
		} else {
			hw->mcfg = CFG_METHOD_30;
			hw->HwIcVerUnknown = TRUE;
		}

		if (hw->mcfg == CFG_METHOD_30) {
			if ((rtl_mac_ocp_read(hw, 0xD006) & 0xFF00) == 0x0100)
				hw->mcfg = CFG_METHOD_35;
			else if ((rtl_mac_ocp_read(hw, 0xD006) & 0xFF00) == 0x0300)
				hw->mcfg = CFG_METHOD_36;
		}
		break;
	case 0x6C000000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_37;
		} else {
			hw->mcfg = CFG_METHOD_37;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x54800000:
		if (ic_version_id == 0x00100000) {
			hw->mcfg = CFG_METHOD_31;
		} else if (ic_version_id == 0x00200000) {
			hw->mcfg = CFG_METHOD_32;
		} else if (ic_version_id == 0x00300000) {
			hw->mcfg = CFG_METHOD_33;
		} else if (ic_version_id == 0x00400000) {
			hw->mcfg = CFG_METHOD_34;
		} else {
			hw->mcfg = CFG_METHOD_34;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x60800000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_48;
		} else if (ic_version_id == 0x100000) {
			hw->mcfg = CFG_METHOD_49;
		} else {
			hw->mcfg = CFG_METHOD_49;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x64000000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_50;
		} else if (ic_version_id == 0x100000) {
			hw->mcfg = CFG_METHOD_51;
		} else {
			hw->mcfg = CFG_METHOD_51;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x68000000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_54;
		} else if (ic_version_id == 0x100000) {
			hw->mcfg = CFG_METHOD_55;
		} else {
			hw->mcfg = CFG_METHOD_55;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x68800000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_56;
		} else if (ic_version_id == 0x100000) {
			hw->mcfg = CFG_METHOD_57;
		} else {
			hw->mcfg = CFG_METHOD_57;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x64800000:
		if (ic_version_id == 0x00000000) {
			hw->mcfg = CFG_METHOD_69;
		} else if (ic_version_id == 0x100000) {
			hw->mcfg = CFG_METHOD_70;
		} else if (ic_version_id == 0x200000) {
			hw->mcfg = CFG_METHOD_71;
		} else {
			hw->mcfg = CFG_METHOD_71;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	case 0x6C800000:
		if (ic_version_id == 0x100000) {
			hw->mcfg = CFG_METHOD_91;
		} else {
			hw->mcfg = CFG_METHOD_91;
			hw->HwIcVerUnknown = TRUE;
		}
		break;
	default:
		PMD_INIT_LOG(NOTICE, "unknown chip version (%x)", reg);
		hw->mcfg = CFG_METHOD_DEFAULT;
		hw->HwIcVerUnknown = TRUE;
		break;
	}

	if (pci_dev->id.device_id == 0x8162) {
		if (hw->mcfg == CFG_METHOD_49)
			hw->mcfg = CFG_METHOD_52;
		else if (hw->mcfg == CFG_METHOD_51)
			hw->mcfg = CFG_METHOD_53;
	}
}

int
rtl_get_mac_address(struct rtl_hw *hw, struct rte_ether_addr *ea)
{
	u8 mac_addr[RTE_ETHER_ADDR_LEN] = {0};

	if (rtl_is_8125(hw)) {
		*(u32 *)&mac_addr[0] = RTL_R32(hw, BACKUP_ADDR0_8125);
		*(u16 *)&mac_addr[4] = RTL_R16(hw, BACKUP_ADDR1_8125);
	} else {
		*(u32 *)&mac_addr[0] = rtl_eri_read(hw, 0xE0, 4, ERIAR_ExGMAC);
		*(u16 *)&mac_addr[4] = rtl_eri_read(hw, 0xE4, 2, ERIAR_ExGMAC);
	}

	rte_ether_addr_copy((struct rte_ether_addr *)mac_addr, ea);

	return 0;
}

/* Puts an ethernet address into a receive address register. */
void
rtl_rar_set(struct rtl_hw *hw, uint8_t *addr)
{
	uint32_t rar_low = 0;
	uint32_t rar_high = 0;

	rar_low = ((uint32_t)addr[0] | ((uint32_t)addr[1] << 8) |
		   ((uint32_t)addr[2] << 16) | ((uint32_t)addr[3] << 24));

	rar_high = ((uint32_t)addr[4] | ((uint32_t)addr[5] << 8));

	rtl_enable_cfg9346_write(hw);

	RTL_W32(hw, MAC0, rar_low);
	RTL_W32(hw, MAC4, rar_high);

	rtl_disable_cfg9346_write(hw);
}

void
rtl_get_tally_stats(struct rtl_hw *hw, struct rte_eth_stats *rte_stats)
{
	struct rtl_counters *counters;
	uint64_t paddr;
	u32 cmd;
	u32 wait_cnt;

	counters = hw->tally_vaddr;
	paddr = hw->tally_paddr;
	if (!counters)
		return;

	RTL_W32(hw, CounterAddrHigh, (u64)paddr >> 32);
	cmd = (u64)paddr & DMA_BIT_MASK(32);
	RTL_W32(hw, CounterAddrLow, cmd);
	RTL_W32(hw, CounterAddrLow, cmd | CounterDump);

	wait_cnt = 0;
	while (RTL_R32(hw, CounterAddrLow) & CounterDump) {
		rte_delay_us(10);

		wait_cnt++;
		if (wait_cnt > 20)
			break;
	}

	/* RX errors */
	rte_stats->imissed = rte_le_to_cpu_64(counters->rx_missed);
	rte_stats->ierrors = rte_le_to_cpu_64(counters->rx_errors);

	/* TX errors */
	rte_stats->oerrors = rte_le_to_cpu_64(counters->tx_errors);

	rte_stats->ipackets = rte_le_to_cpu_64(counters->rx_packets);
	rte_stats->opackets = rte_le_to_cpu_64(counters->tx_packets);
}

void
rtl_clear_tally_stats(struct rtl_hw *hw)
{
	if (!hw->tally_paddr)
		return;

	RTL_W32(hw, CounterAddrHigh, (u64)hw->tally_paddr >> 32);
	RTL_W32(hw, CounterAddrLow,
		((u64)hw->tally_paddr & (DMA_BIT_MASK(32))) | CounterReset);
}

int
rtl_tally_init(struct rte_eth_dev *dev)
{
	struct rtl_adapter *adapter = RTL_DEV_PRIVATE(dev);
	struct rtl_hw *hw = &adapter->hw;
	const struct rte_memzone *mz;

	mz = rte_eth_dma_zone_reserve(dev, "tally_counters", 0,
				      sizeof(struct rtl_counters), 64, rte_socket_id());
	if (mz == NULL)
		return -ENOMEM;

	hw->tally_vaddr = mz->addr;
	hw->tally_paddr = mz->iova;

	/* Fill tally addrs */
	RTL_W32(hw, CounterAddrHigh, (u64)hw->tally_paddr >> 32);
	RTL_W32(hw, CounterAddrLow, (u64)hw->tally_paddr & (DMA_BIT_MASK(32)));

	/* Reset the hw statistics */
	rtl_clear_tally_stats(hw);

	return 0;
}

void
rtl_tally_free(struct rte_eth_dev *dev)
{
	rte_eth_dma_zone_free(dev, "tally_counters", 0);
}

bool
rtl_is_8125(struct rtl_hw *hw)
{
	return hw->mcfg >= CFG_METHOD_48;
}

u64
rtl_get_hw_mcu_patch_code_ver(struct rtl_hw *hw)
{
	u64 ver;
	int i;

	/* Switch to page 2 */
	rtl_switch_mac_mcu_ram_code_page(hw, 2);

	ver = 0;
	for (i = 0; i < 8; i += 2) {
		ver <<= 16;
		ver |= rtl_mac_ocp_read(hw, 0xF9F8 + i);
	}

	/* Switch back to page 0 */
	rtl_switch_mac_mcu_ram_code_page(hw, 0);

	return ver;
}

u64
rtl_get_bin_mcu_patch_code_ver(const u16 *entry, u16 entry_cnt)
{
	u64 ver;
	int i;

	if (!entry || entry_cnt == 0 || entry_cnt < 4)
		return 0;

	ver = 0;
	for (i = 0; i < 4; i++) {
		ver <<= 16;
		ver |= entry[entry_cnt - 4 + i];
	}

	return ver;
}
