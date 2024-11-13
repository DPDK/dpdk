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

static int
rtl_eri_write_with_oob_base_address(struct rtl_hw *hw, int addr,
				    int len, u32 value, int type, const u32 base_address)
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

static u32
rtl_ocp_read_with_oob_base_address(struct rtl_hw *hw, u16 addr, u8 len,
				   const u32 base_address)
{
	return rtl_eri_read_with_oob_base_address(hw, addr, len, ERIAR_OOB,
						  base_address);
}

u32
rtl_ocp_read(struct rtl_hw *hw, u16 addr, u8 len)
{
	u32 value = 0;

	if (!hw->AllowAccessDashOcp)
		return 0xffffffff;

	if (hw->HwSuppOcpChannelVer == 2)
		value = rtl_ocp_read_with_oob_base_address(hw, addr, len, NO_BASE_ADDRESS);

	return value;
}

static u32
rtl_ocp_write_with_oob_base_address(struct rtl_hw *hw, u16 addr, u8 len,
				    u32 value, const u32 base_address)
{
	return rtl_eri_write_with_oob_base_address(hw, addr, len, value, ERIAR_OOB,
						   base_address);
}

void
rtl_ocp_write(struct rtl_hw *hw, u16 addr, u8 len, u32 value)
{
	if (!hw->AllowAccessDashOcp)
		return;

	if (hw->HwSuppOcpChannelVer == 2)
		rtl_ocp_write_with_oob_base_address(hw, addr, len, value, NO_BASE_ADDRESS);
}

void
rtl8125_oob_mutex_lock(struct rtl_hw *hw)
{
	u8 reg_16, reg_a0;
	u16 ocp_reg_mutex_ib;
	u16 ocp_reg_mutex_oob;
	u16 ocp_reg_mutex_prio;
	u32 wait_cnt_0, wait_cnt_1;

	if (!hw->DASH)
		return;

	switch (hw->mcfg) {
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_52:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
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
rtl8125_oob_mutex_unlock(struct rtl_hw *hw)
{
	u16 ocp_reg_mutex_ib;
	u16 ocp_reg_mutex_prio;

	if (!hw->DASH)
		return;

	switch (hw->mcfg) {
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_52:
	case CFG_METHOD_54:
	case CFG_METHOD_55:
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

static void
rtl_enable_rxdvgate(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:
		RTL_W8(hw, 0xF2, RTL_R8(hw, 0xF2) | BIT_3);
		rte_delay_ms(2);
	}
}

void
rtl_disable_rxdvgate(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:
		RTL_W8(hw, 0xF2, RTL_R8(hw, 0xF2) & ~BIT_3);
		rte_delay_ms(2);
	}
}

static void
rtl_stop_all_request(struct rtl_hw *hw)
{
	int i;

	RTL_W8(hw, ChipCmd, RTL_R8(hw, ChipCmd) | StopReq);

	switch (hw->mcfg) {
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_52:
		for (i = 0; i < 20; i++) {
			rte_delay_us(10);
			if (!(RTL_R8(hw, ChipCmd) & StopReq))
				break;
		}

		break;
	default:
		rte_delay_us(200);
		break;
	}

	RTL_W8(hw, ChipCmd, RTL_R8(hw, ChipCmd) & (CmdTxEnb | CmdRxEnb));
}

static void
rtl_wait_txrx_fifo_empty(struct rtl_hw *hw)
{
	int i;

	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:
		for (i = 0; i < 3000; i++) {
			rte_delay_us(50);
			if ((RTL_R8(hw, MCUCmd_reg) & (Txfifo_empty | Rxfifo_empty)) ==
			    (Txfifo_empty | Rxfifo_empty))
				break;
		}
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_53 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:
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

	rte_delay_ms(2);

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
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69:
		rtl_enable_cfg9346_write(hw);
		if (enable) {
			RTL_W8(hw, Config2, RTL_R8(hw, Config2) | BIT_7);
			RTL_W8(hw, Config5, RTL_R8(hw, Config5) | BIT_0);
		} else {
			RTL_W8(hw, Config2, RTL_R8(hw, Config2) & ~BIT_7);
			RTL_W8(hw, Config5, RTL_R8(hw, Config5) & ~BIT_0);
		}
		rtl_disable_cfg9346_write(hw);
		break;
	case CFG_METHOD_70:
	case CFG_METHOD_71:
		rtl_enable_cfg9346_write(hw);
		if (enable) {
			RTL_W8(hw, INT_CFG0_8125, RTL_R8(hw, INT_CFG0_8125) | BIT_3);
			RTL_W8(hw, Config5, RTL_R8(hw, Config5) | BIT_0);
		} else {
			RTL_W8(hw, INT_CFG0_8125, RTL_R8(hw, INT_CFG0_8125) & ~BIT_3);
			RTL_W8(hw, Config5, RTL_R8(hw, Config5) & ~BIT_0);
		}
		rtl_disable_cfg9346_write(hw);
		break;
	}
}

static void
rtl_disable_l1_timeout(struct rtl_hw *hw)
{
	rtl_csi_write(hw, 0x890, rtl_csi_read(hw, 0x890) & ~BIT_0);
}

static void
rtl_disable_eee_plus(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:
		rtl_mac_ocp_write(hw, 0xE080, rtl_mac_ocp_read(hw, 0xE080) & ~BIT_1);
		break;

	default:
		/* Not support EEEPlus */
		break;
	}
}

static void
rtl_hw_clear_timer_int(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:
		RTL_W32(hw, TIMER_INT0_8125, 0x0000);
		RTL_W32(hw, TIMER_INT1_8125, 0x0000);
		RTL_W32(hw, TIMER_INT2_8125, 0x0000);
		RTL_W32(hw, TIMER_INT3_8125, 0x0000);
		break;
	}
}

static void
rtl_hw_clear_int_miti(struct rtl_hw *hw)
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

void
rtl_hw_config(struct rtl_hw *hw)
{
	u32 mac_ocp_data;

	/* Set RxConfig to default */
	RTL_W32(hw, RxConfig, (RX_DMA_BURST_unlimited << RxCfgDMAShift));

	rtl_nic_reset(hw);

	rtl_enable_cfg9346_write(hw);

	/* Disable aspm clkreq internal */
	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:
		rtl_enable_force_clkreq(hw, 0);
		rtl_enable_aspm_clkreq_lock(hw, 0);
		break;
	}

	/* Disable magic packet */
	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:
		mac_ocp_data = 0;
		rtl_mac_ocp_write(hw, 0xC0B6, mac_ocp_data);
		break;
	}

	/* Set DMA burst size and interframe gap time */
	RTL_W32(hw, TxConfig, (TX_DMA_BURST_unlimited << TxDMAShift) |
		(InterFrameGap << TxInterFrameGapShift));

	if (hw->EnableTxNoClose)
		RTL_W32(hw, TxConfig, (RTL_R32(hw, TxConfig) | BIT_6));

	/* TCAM */
	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_53:
		RTL_W16(hw, 0x382, 0x221B);
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_69 ... CFG_METHOD_71:
		rtl_disable_l1_timeout(hw);
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:

		/* RSS_control_0 */
		RTL_W32(hw, RSS_CTRL_8125, 0x00);

		/* VMQ_control */
		RTL_W16(hw, Q_NUM_CTRL_8125, 0x0000);

		/* Disable speed down */
		RTL_W8(hw, Config1, RTL_R8(hw, Config1) & ~0x10);

		/* CRC disable set */
		rtl_mac_ocp_write(hw, 0xC140, 0xFFFF);
		rtl_mac_ocp_write(hw, 0xC142, 0xFFFF);

		/* New TX desc format */
		mac_ocp_data = rtl_mac_ocp_read(hw, 0xEB58);
		if (hw->mcfg == CFG_METHOD_70 || hw->mcfg == CFG_METHOD_71)
			mac_ocp_data &= ~(BIT_0 | BIT_1);
		mac_ocp_data |= BIT_0;
		rtl_mac_ocp_write(hw, 0xEB58, mac_ocp_data);

		if (hw->mcfg == CFG_METHOD_70 || hw->mcfg == CFG_METHOD_71)
			RTL_W8(hw, 0xD8, RTL_R8(hw, 0xD8) & ~BIT_1);

		/*
		 * MTPS
		 * 15-8 maximum tx use credit number
		 * 7-0 reserved for pcie product line
		 */
		mac_ocp_data = rtl_mac_ocp_read(hw, 0xE614);
		mac_ocp_data &= ~(BIT_10 | BIT_9 | BIT_8);
		if (hw->mcfg == CFG_METHOD_50 || hw->mcfg == CFG_METHOD_51 ||
		    hw->mcfg == CFG_METHOD_53)
			mac_ocp_data |= ((2 & 0x07) << 8);
		else if (hw->mcfg == CFG_METHOD_69 || hw->mcfg == CFG_METHOD_70 ||
			 hw->mcfg == CFG_METHOD_71)
			mac_ocp_data |= ((4 & 0x07) << 8);
		else
			mac_ocp_data |= ((3 & 0x07) << 8);
		rtl_mac_ocp_write(hw, 0xE614, mac_ocp_data);

		mac_ocp_data = rtl_mac_ocp_read(hw, 0xE63E);
		mac_ocp_data &= ~(BIT_5 | BIT_4);
		if (hw->mcfg == CFG_METHOD_48 || hw->mcfg == CFG_METHOD_49 ||
		    hw->mcfg == CFG_METHOD_52 || hw->mcfg == CFG_METHOD_69 ||
		    hw->mcfg == CFG_METHOD_70 || hw->mcfg == CFG_METHOD_71)
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
		mac_ocp_data &= ~(BIT_7 | BIT_6 | BIT_5 | BIT_4 | BIT_3 | BIT_2 | BIT_1 |
				  BIT_0);
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

		switch (hw->mcfg) {
		case CFG_METHOD_48:
		case CFG_METHOD_49:
		case CFG_METHOD_52:
		case CFG_METHOD_54:
		case CFG_METHOD_55:
			rtl8125_oob_mutex_lock(hw);
			break;
		}

		/* MAC_PWRDWN_CR0 */
		rtl_mac_ocp_write(hw, 0xE0C0, 0x4000);

		rtl_set_mac_ocp_bit(hw, 0xE052, (BIT_6 | BIT_5));
		rtl_clear_mac_ocp_bit(hw, 0xE052, (BIT_3 | BIT_7));

		switch (hw->mcfg) {
		case CFG_METHOD_48:
		case CFG_METHOD_49:
		case CFG_METHOD_52:
		case CFG_METHOD_54:
		case CFG_METHOD_55:
			rtl8125_oob_mutex_unlock(hw);
			break;
		}

		/*
		 * DMY_PWR_REG_0
		 * (1)ERI(0xD4)(OCP 0xC0AC).bit[7:12]=6'b111111, L1 Mask
		 */
		rtl_set_mac_ocp_bit(hw, 0xC0AC,
				    (BIT_7 | BIT_8 | BIT_9 | BIT_10 | BIT_11 | BIT_12));

		mac_ocp_data = rtl_mac_ocp_read(hw, 0xD430);
		mac_ocp_data &= ~(BIT_11 | BIT_10 | BIT_9 | BIT_8 | BIT_7 | BIT_6 | BIT_5 |
				  BIT_4 | BIT_3 | BIT_2 | BIT_1 | BIT_0);
		mac_ocp_data |= 0x45F;
		rtl_mac_ocp_write(hw, 0xD430, mac_ocp_data);

		if (!hw->DASH)
			RTL_W8(hw, 0xD0, RTL_R8(hw, 0xD0) | BIT_6 | BIT_7);
		else
			RTL_W8(hw, 0xD0, RTL_R8(hw, 0xD0) & ~(BIT_6 | BIT_7));

		if (hw->mcfg == CFG_METHOD_48 || hw->mcfg == CFG_METHOD_49 ||
		    hw->mcfg == CFG_METHOD_52)
			RTL_W8(hw, MCUCmd_reg, RTL_R8(hw, MCUCmd_reg) | BIT_0);

		rtl_disable_eee_plus(hw);

		mac_ocp_data = rtl_mac_ocp_read(hw, 0xEA1C);
		mac_ocp_data &= ~BIT_2;
		if (hw->mcfg == CFG_METHOD_70 || hw->mcfg == CFG_METHOD_71)
			mac_ocp_data &= ~(BIT_9 | BIT_8);
		rtl_mac_ocp_write(hw, 0xEA1C, mac_ocp_data);

		/* Clear TCAM entries */
		rtl_set_mac_ocp_bit(hw, 0xEB54, BIT_0);
		rte_delay_us(1);
		rtl_clear_mac_ocp_bit(hw, 0xEB54, BIT_0);

		RTL_W16(hw, 0x1880, RTL_R16(hw, 0x1880) & ~(BIT_4 | BIT_5));

		switch (hw->mcfg) {
		case CFG_METHOD_54 ... CFG_METHOD_57:
			RTL_W8(hw, 0xd8, RTL_R8(hw, 0xd8) & ~EnableRxDescV4_0);
			break;
		}
	}

	/* Other hw parameters */
	rtl_hw_clear_timer_int(hw);

	rtl_hw_clear_int_miti(hw);

	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:
		rtl_mac_ocp_write(hw, 0xE098, 0xC302);
		break;
	}

	rtl_disable_cfg9346_write(hw);

	rte_delay_us(10);
}

int
rtl_set_hw_ops(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
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
	case CFG_METHOD_69 ... CFG_METHOD_71:
		hw->hw_ops = rtl8126a_ops;
		return 0;
	default:
		return -ENOTSUP;
	}
}

void
rtl_hw_disable_mac_mcu_bps(struct rtl_hw *hw)
{
	u16 reg_addr;

	rtl_enable_aspm_clkreq_lock(hw, 0);

	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:
		rtl_mac_ocp_write(hw, 0xFC48, 0x0000);
		break;
	}

	switch (hw->mcfg) {
	case CFG_METHOD_48 ... CFG_METHOD_57:
	case CFG_METHOD_69 ... CFG_METHOD_71:
		for (reg_addr = 0xFC28; reg_addr < 0xFC48; reg_addr += 2)
			rtl_mac_ocp_write(hw, reg_addr, 0x0000);

		rte_delay_ms(3);

		rtl_mac_ocp_write(hw, 0xFC26, 0x0000);
		break;
	}
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

void
rtl_write_mac_mcu_ram_code(struct rtl_hw *hw, const u16 *entry, u16 entry_cnt)
{
	if (HW_SUPPORT_MAC_MCU(hw) == FALSE)
		return;
	if (entry == NULL || entry_cnt == 0)
		return;

	if (hw->MacMcuPageSize > 0)
		_rtl_write_mac_mcu_ram_code_with_page(hw, entry, entry_cnt,
						      hw->MacMcuPageSize);
	else
		_rtl_write_mac_mcu_ram_code(hw, entry, entry_cnt);
}
