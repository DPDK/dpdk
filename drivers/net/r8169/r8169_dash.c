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
	case CFG_METHOD_48:
	case CFG_METHOD_49:
		mac_ocp_data = rtl_mac_ocp_read(hw, 0xd460);
		if (mac_ocp_data == 0xffff || !(mac_ocp_data & BIT_0))
			allow_access = false;
		break;
	case CFG_METHOD_54:
	case CFG_METHOD_55:
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

	if (!HW_DASH_SUPPORT_GET_FIRMWARE_VERSION(hw))
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

	if (HW_DASH_SUPPORT_TYPE_2(hw) || HW_DASH_SUPPORT_TYPE_3(hw) ||
	    HW_DASH_SUPPORT_TYPE_4(hw)) {
		if (rtl_ocp_read(hw, 0x128, 1) & BIT_0)
			return 1;
		else
			return 0;
	} else if (HW_DASH_SUPPORT_TYPE_1(hw)) {
		if (rtl_ocp_read(hw, 0x10, 2) & 0x00008000)
			return 1;
		else
			return 0;
	} else {
		return 0;
	}
}

int
rtl_check_dash(struct rtl_hw *hw)
{
	int dash_enabled;
	u32 fw_ver;

	dash_enabled = _rtl_check_dash(hw);

	if (!dash_enabled)
		goto exit;

	if (!HW_DASH_SUPPORT_GET_FIRMWARE_VERSION(hw))
		goto exit;

	fw_ver = rtl_get_dash_fw_ver(hw);
	if (fw_ver == 0 || fw_ver == 0xffffffff)
		dash_enabled = 0;
exit:
	return dash_enabled;
}

static u32
rtl8168_csi_to_cmac_r32(struct rtl_hw *hw)
{
	u32 cmd;
	int i;
	u32 value = 0;

	cmd = CSIAR_Read | CSIAR_ByteEn << CSIAR_ByteEn_shift | 0xf9;

	cmd |= 1 << 16;

	RTL_W32(hw, CSIAR, cmd);

	for (i = 0; i < RTL_CHANNEL_WAIT_COUNT; i++) {
		rte_delay_us(RTL_CHANNEL_WAIT_TIME);

		/* Check if the RTL8168 has completed CSI read */
		if (RTL_R32(hw, CSIAR) & CSIAR_Flag) {
			value = RTL_R32(hw, CSIDR);
			break;
		}
	}

	rte_delay_us(RTL_CHANNEL_EXIT_DELAY_TIME);

	return value;
}

static u8
rtl8168_csi_to_cmac_r8(struct rtl_hw *hw, u32 reg)
{
	u32 mask, value1 = 0;
	u8 val_shift, value2 = 0;

	val_shift = reg - 0xf8;

	if (val_shift == 0)
		mask = 0xFF;
	else if (val_shift == 1)
		mask = 0xFF00;
	else if (val_shift == 2)
		mask = 0xFF0000;
	else
		mask = 0xFF000000;

	value1 = rtl8168_csi_to_cmac_r32(hw) & mask;
	value2 = value1 >> (val_shift * 8);

	return value2;
}

static void
rtl8168_csi_to_cmac_w8(struct rtl_hw *hw, u32 reg, u8 value)
{
	int i;
	u8 val_shift;
	u32 value32, cmd, mask;

	val_shift = reg - 0xf8;

	if (val_shift == 0)
		mask = 0xFF;
	else if (val_shift == 1)
		mask = 0xFF00;
	else if (val_shift == 2)
		mask = 0xFF0000;
	else
		mask = 0xFF000000;

	value32 = rtl8168_csi_to_cmac_r32(hw) & ~mask;
	value32 |= value << (val_shift * 8);
	RTL_W32(hw, CSIDR, value32);

	cmd = CSIAR_Write | CSIAR_ByteEn << CSIAR_ByteEn_shift | 0xf9;

	cmd |= 1 << 16;

	RTL_W32(hw, CSIAR, cmd);

	for (i = 0; i < RTL_CHANNEL_WAIT_COUNT; i++) {
		rte_delay_us(RTL_CHANNEL_WAIT_TIME);

		/* Check if the RTL8168 has completed CSI write */
		if (!(RTL_R32(hw, CSIAR) & CSIAR_Flag))
			break;
	}

	rte_delay_us(RTL_CHANNEL_EXIT_DELAY_TIME);
}

static void
rtl_cmac_w8(struct rtl_hw *hw, u32 reg, u8 value)
{
	if (HW_DASH_SUPPORT_TYPE_2(hw) || HW_DASH_SUPPORT_TYPE_4(hw))
		RTL_CMAC_W8(hw, reg, value);
	else if (HW_DASH_SUPPORT_TYPE_3(hw))
		rtl8168_csi_to_cmac_w8(hw, reg, value);
}

static u8
rtl_cmac_r8(struct rtl_hw *hw, u32 reg)
{
	if (HW_DASH_SUPPORT_TYPE_2(hw) || HW_DASH_SUPPORT_TYPE_4(hw))
		return RTL_CMAC_R8(hw, reg);
	else if (HW_DASH_SUPPORT_TYPE_3(hw))
		return rtl8168_csi_to_cmac_r8(hw, reg);
	else
		return 0;
}

static void
rtl_dash2_disable_tx(struct rtl_hw *hw)
{
	u16 wait_cnt = 0;
	u8 tmp_uchar;

	/* Disable oob Tx */
	rtl_cmac_w8(hw, CMAC_IBCR2, rtl_cmac_r8(hw, CMAC_IBCR2) & ~BIT_0);

	/* Wait oob Tx disable */
	do {
		tmp_uchar = rtl_cmac_r8(hw, CMAC_IBISR0);
		if (tmp_uchar & ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE)
			break;

		rte_delay_us(50);
		wait_cnt++;
	} while (wait_cnt < 2000);

	/* Clear ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE */
	rtl_cmac_w8(hw, CMAC_IBISR0, rtl_cmac_r8(hw, CMAC_IBISR0) |
		    ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE);
}

static void
rtl_dash2_disable_rx(struct rtl_hw *hw)
{
	rtl_cmac_w8(hw, CMAC_IBCR0, rtl_cmac_r8(hw, CMAC_IBCR0) & ~BIT_0);
}

void
rtl_dash2_disable_txrx(struct rtl_hw *hw)
{
	if (!HW_DASH_SUPPORT_CMAC(hw))
		return;

	if (!hw->DASH)
		return;

	rtl_dash2_disable_tx(hw);
	rtl_dash2_disable_rx(hw);
}

static void
rtl8125_notify_dash_oob_cmac(struct rtl_hw *hw, u32 cmd)
{
	u32 tmp_value;

	if (!HW_DASH_SUPPORT_CMAC(hw))
		return;

	rtl_ocp_write(hw, 0x180, 4, cmd);
	tmp_value = rtl_ocp_read(hw, 0x30, 4);
	tmp_value |= BIT_0;
	rtl_ocp_write(hw, 0x30, 4, tmp_value);
}

static void
rtl8125_notify_dash_oob_ipc2(struct rtl_hw *hw, u32 cmd)
{
	if (!HW_DASH_SUPPORT_TYPE_4(hw))
		return;

	rtl_ocp_write(hw, IB2SOC_DATA, 4, cmd);
	rtl_ocp_write(hw, IB2SOC_CMD, 4, 0x00);
	rtl_ocp_write(hw, IB2SOC_SET, 4, 0x01);
}

static void
rtl8125_notify_dash_oob(struct rtl_hw *hw, u32 cmd)
{
	switch (hw->HwSuppDashVer) {
	case 2:
	case 3:
		rtl8125_notify_dash_oob_cmac(hw, cmd);
		break;
	case 4:
		rtl8125_notify_dash_oob_ipc2(hw, cmd);
		break;
	default:
		break;
	}
}

static int
rtl_wait_dash_fw_ready(struct rtl_hw *hw)
{
	int rc = -1;
	int timeout;

	if (!HW_DASH_SUPPORT_DASH(hw))
		goto out;

	if (!hw->DASH)
		goto out;

	if (HW_DASH_SUPPORT_TYPE_2(hw) || HW_DASH_SUPPORT_TYPE_3(hw) ||
	    HW_DASH_SUPPORT_TYPE_4(hw)) {
		for (timeout = 0; timeout < 10; timeout++) {
			rte_delay_ms(10);
			if (rtl_ocp_read(hw, 0x124, 1) & BIT_0) {
				rc = 1;
				goto out;
			}
		}
	} else if (HW_DASH_SUPPORT_TYPE_1(hw)) {
		for (timeout = 0; timeout < 10; timeout++) {
			rte_delay_ms(10);
			if (rtl_ocp_read(hw, 0x10, 2) & BIT_11) {
				rc = 1;
				goto out;
			}
		}
	} else {
		goto out;
	}

	rc = 0;

out:
	return rc;
}

static void
rtl8125_driver_start(struct rtl_hw *hw)
{
	if (!hw->AllowAccessDashOcp)
		return;

	rtl8125_notify_dash_oob(hw, OOB_CMD_DRIVER_START);

	rtl_wait_dash_fw_ready(hw);
}

static void
rtl8168_clear_and_set_other_fun_pci_bit(struct rtl_hw *hw, u8 multi_fun_sel_bit,
					u32 addr, u32 clearmask, u32 setmask)
{
	u32 tmp_ulong;

	tmp_ulong = rtl_csi_other_fun_read(hw, multi_fun_sel_bit, addr);
	tmp_ulong &= ~clearmask;
	tmp_ulong |= setmask;
	rtl_csi_other_fun_write(hw, multi_fun_sel_bit, addr, tmp_ulong);
}

static void
rtl8168_other_fun_dev_pci_setting(struct rtl_hw *hw, u32 addr, u32 clearmask,
				  u32 setmask, u8 multi_fun_sel_bit)
{
	u32 tmp_ulong;
	u8 i;
	u8 fun_bit;
	u8 set_other_fun;

	for (i = 0; i < 8; i++) {
		fun_bit = (1 << i);
		if (!(fun_bit & multi_fun_sel_bit))
			continue;

		set_other_fun = TRUE;

		switch (hw->mcfg) {
		case CFG_METHOD_23:
		case CFG_METHOD_27:
		case CFG_METHOD_28:
			/*
			 * 0: UMAC, 1: TCR1, 2: TCR2, 3: KCS,
			 * 4: EHCI(Control by EHCI Driver)
			 */
			if (i < 5) {
				tmp_ulong = rtl_csi_other_fun_read(hw, i, 0x00);
				if (tmp_ulong == 0xFFFFFFFF)
					set_other_fun = TRUE;
				else
					set_other_fun = FALSE;
			}
			break;
		case CFG_METHOD_31:
		case CFG_METHOD_32:
		case CFG_METHOD_33:
		case CFG_METHOD_34:
			/*
			 * 0: BMC, 1: NIC, 2: TCR, 3: VGA / PCIE_TO_USB,
			 * 4: EHCI, 5: WIFI, 6: WIFI, 7: KCS
			 */
			if (i == 5 || i == 6) {
				if (hw->DASH) {
					tmp_ulong = rtl_ocp_read(hw, 0x184, 4);
					if (tmp_ulong & BIT_26)
						set_other_fun = FALSE;
					else
						set_other_fun = TRUE;
				}
			} else { /* Function 0/1/2/3/4/7 */
				tmp_ulong = rtl_csi_other_fun_read(hw, i, 0x00);
				if (tmp_ulong == 0xFFFFFFFF)
					set_other_fun = TRUE;
				else
					set_other_fun = FALSE;
			}
			break;
		default:
			return;
		}

		if (set_other_fun)
			rtl8168_clear_and_set_other_fun_pci_bit(hw, i, addr,
								clearmask, setmask);
	}
}

static void
rtl8168_set_dash_other_fun_dev_state_change(struct rtl_hw *hw, u8 dev_state,
					    u8 multi_fun_sel_bit)
{
	u32 clearmask;
	u32 setmask;

	if (dev_state == 0) {
		/* Goto D0 */
		clearmask = (BIT_0 | BIT_1);
		setmask = 0;

		rtl8168_other_fun_dev_pci_setting(hw, 0x44, clearmask, setmask,
						  multi_fun_sel_bit);
	} else {
		/* Goto D3 */
		clearmask = 0;
		setmask = (BIT_0 | BIT_1);

		rtl8168_other_fun_dev_pci_setting(hw, 0x44, clearmask, setmask,
						  multi_fun_sel_bit);
	}
}

static void
rtl8168_set_dash_other_fun_dev_aspm_clkreq(struct rtl_hw *hw, u8 aspm_val,
					   u8 clkreq_en, u8 multi_fun_sel_bit)
{
	u32 clearmask;
	u32 setmask;

	aspm_val &= (BIT_0 | BIT_1);
	clearmask = (BIT_0 | BIT_1 | BIT_8);
	setmask = aspm_val;
	if (clkreq_en)
		setmask |= BIT_8;

	rtl8168_other_fun_dev_pci_setting(hw, 0x80, clearmask, setmask,
					  multi_fun_sel_bit);
}

static void
rtl8168_oob_notify(struct rtl_hw *hw, u8 cmd)
{
	rtl_eri_write(hw, 0xE8, 1, cmd, ERIAR_ExGMAC);

	rtl_ocp_write(hw, 0x30, 1, 0x01);
}

static void
rtl8168_driver_start(struct rtl_hw *hw)
{
	u32 tmp_value;

	/* Change other device state to D0. */
	switch (hw->mcfg) {
	case CFG_METHOD_23:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
		rtl8168_set_dash_other_fun_dev_aspm_clkreq(hw, 3, 1, 0x1E);
		rtl8168_set_dash_other_fun_dev_state_change(hw, 3, 0x1E);
		break;
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		rtl8168_set_dash_other_fun_dev_aspm_clkreq(hw, 3, 1, 0xFC);
		rtl8168_set_dash_other_fun_dev_state_change(hw, 3, 0xFC);
		break;
	}

	if (HW_DASH_SUPPORT_TYPE_2(hw) || HW_DASH_SUPPORT_TYPE_3(hw)) {
		rtl_ocp_write(hw, 0x180, 1, OOB_CMD_DRIVER_START);
		tmp_value = rtl_ocp_read(hw, 0x30, 1);
		tmp_value |= BIT_0;
		rtl_ocp_write(hw, 0x30, 1, tmp_value);
	} else {
		rtl8168_oob_notify(hw, OOB_CMD_DRIVER_START);
	}

	rtl_wait_dash_fw_ready(hw);
}

void
rtl_driver_start(struct rtl_hw *hw)
{
	if (rtl_is_8125(hw))
		rtl8125_driver_start(hw);
	else
		rtl8168_driver_start(hw);
}

static void
rtl8168_driver_stop(struct rtl_hw *hw)
{
	u32 tmp_value;

	if (HW_DASH_SUPPORT_TYPE_2(hw) || HW_DASH_SUPPORT_TYPE_3(hw)) {
		rtl_dash2_disable_txrx(hw);

		rtl_ocp_write(hw, 0x180, 1, OOB_CMD_DRIVER_STOP);
		tmp_value = rtl_ocp_read(hw, 0x30, 1);
		tmp_value |= BIT_0;
		rtl_ocp_write(hw, 0x30, 1, tmp_value);
	} else if (HW_DASH_SUPPORT_TYPE_1(hw)) {
		rtl8168_oob_notify(hw, OOB_CMD_DRIVER_STOP);
	}

	rtl_wait_dash_fw_ready(hw);

	/* Change other device state to D3. */
	switch (hw->mcfg) {
	case CFG_METHOD_23:
	case CFG_METHOD_27:
	case CFG_METHOD_28:
		rtl8168_set_dash_other_fun_dev_state_change(hw, 3, 0x0E);
		break;
	case CFG_METHOD_31:
	case CFG_METHOD_32:
	case CFG_METHOD_33:
	case CFG_METHOD_34:
		rtl8168_set_dash_other_fun_dev_state_change(hw, 3, 0xFD);
		break;
	}
}

static void
rtl8125_driver_stop(struct rtl_hw *hw)
{
	if (!hw->AllowAccessDashOcp)
		return;

	if (HW_DASH_SUPPORT_CMAC(hw))
		rtl_dash2_disable_txrx(hw);

	rtl8125_notify_dash_oob(hw, OOB_CMD_DRIVER_STOP);

	rtl_wait_dash_fw_ready(hw);
}

void
rtl_driver_stop(struct rtl_hw *hw)
{
	if (rtl_is_8125(hw))
		rtl8125_driver_stop(hw);
	else
		rtl8168_driver_stop(hw);
}

bool
rtl8168_check_dash_other_fun_present(struct rtl_hw *hw)
{
	/* Check if func 2 exist */
	if (rtl_csi_other_fun_read(hw, 2, 0x00) != 0xffffffff)
		return true;
	else
		return false;
}
