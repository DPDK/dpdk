/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Realtek Corporation. All rights reserved
 */

#include "../r8169_ethdev.h"
#include "../r8169_hw.h"
#include "../r8169_phy.h"
#include "rtl8125a.h"

/* For RTL8125A, CFG_METHOD_48,49 */

/* ------------------------------------MAC 8125A------------------------------------- */

void
rtl_set_mac_mcu_8125a_2(struct rtl_hw *hw)
{
	u16 entry_cnt;
	static const u16 mcu_patch_code_8125a_2[] = {
		0xE010, 0xE012, 0xE022, 0xE024, 0xE029, 0xE02B, 0xE094, 0xE09D, 0xE09F,
		0xE0AA, 0xE0B5, 0xE0C6, 0xE0CC, 0xE0D1, 0xE0D6, 0xE0D8, 0xC602, 0xBE00,
		0x0000, 0xC60F, 0x73C4, 0x49B3, 0xF106, 0x73C2, 0xC608, 0xB406, 0xC609,
		0xFF80, 0xC605, 0xB406, 0xC605, 0xFF80, 0x0544, 0x0568, 0xE906, 0xCDE8,
		0xC602, 0xBE00, 0x0000, 0x48C1, 0x48C2, 0x9C46, 0xC402, 0xBC00, 0x0A12,
		0xC602, 0xBE00, 0x0EBA, 0x1501, 0xF02A, 0x1500, 0xF15D, 0xC661, 0x75C8,
		0x49D5, 0xF00A, 0x49D6, 0xF008, 0x49D7, 0xF006, 0x49D8, 0xF004, 0x75D2,
		0x49D9, 0xF150, 0xC553, 0x77A0, 0x75C8, 0x4855, 0x4856, 0x4857, 0x4858,
		0x48DA, 0x48DB, 0x49FE, 0xF002, 0x485A, 0x49FF, 0xF002, 0x485B, 0x9DC8,
		0x75D2, 0x4859, 0x9DD2, 0xC643, 0x75C0, 0x49D4, 0xF033, 0x49D0, 0xF137,
		0xE030, 0xC63A, 0x75C8, 0x49D5, 0xF00E, 0x49D6, 0xF00C, 0x49D7, 0xF00A,
		0x49D8, 0xF008, 0x75D2, 0x49D9, 0xF005, 0xC62E, 0x75C0, 0x49D7, 0xF125,
		0xC528, 0x77A0, 0xC627, 0x75C8, 0x4855, 0x4856, 0x4857, 0x4858, 0x48DA,
		0x48DB, 0x49FE, 0xF002, 0x485A, 0x49FF, 0xF002, 0x485B, 0x9DC8, 0x75D2,
		0x4859, 0x9DD2, 0xC616, 0x75C0, 0x4857, 0x9DC0, 0xC613, 0x75C0, 0x49DA,
		0xF003, 0x49D0, 0xF107, 0xC60B, 0xC50E, 0x48D9, 0x9DC0, 0x4859, 0x9DC0,
		0xC608, 0xC702, 0xBF00, 0x3AE0, 0xE860, 0xB400, 0xB5D4, 0xE908, 0xE86C,
		0x1200, 0xC409, 0x6780, 0x48F1, 0x8F80, 0xC404, 0xC602, 0xBE00, 0x10AA,
		0xC010, 0xEA7C, 0xC602, 0xBE00, 0x0000, 0x740A, 0x4846, 0x4847, 0x9C0A,
		0xC607, 0x74C0, 0x48C6, 0x9CC0, 0xC602, 0xBE00, 0x13FE, 0xE054, 0x72CA,
		0x4826, 0x4827, 0x9ACA, 0xC607, 0x72C0, 0x48A6, 0x9AC0, 0xC602, 0xBE00,
		0x07DC, 0xE054, 0xC60F, 0x74C4, 0x49CC, 0xF109, 0xC60C, 0x74CA, 0x48C7,
		0x9CCA, 0xC609, 0x74C0, 0x4846, 0x9CC0, 0xC602, 0xBE00, 0x2480, 0xE092,
		0xE0C0, 0xE054, 0x7420, 0x48C0, 0x9C20, 0x7444, 0xC602, 0xBE00, 0x12F8,
		0x1BFF, 0x46EB, 0x1BFF, 0xC102, 0xB900, 0x0D5A, 0x1BFF, 0x46EB, 0x1BFF,
		0xC102, 0xB900, 0x0E2A, 0xC602, 0xBE00, 0x0000, 0xC602, 0xBE00, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x6486,
		0x0B15, 0x090E, 0x1139
	};

	entry_cnt = ARRAY_SIZE(mcu_patch_code_8125a_2);

	/* Get BIN mac mcu patch code version */
	hw->bin_mcu_patch_code_ver = rtl_get_bin_mcu_patch_code_ver(mcu_patch_code_8125a_2,
								    entry_cnt);

	if (hw->hw_mcu_patch_code_ver != hw->bin_mcu_patch_code_ver)
		rtl_write_mac_mcu_ram_code(hw, mcu_patch_code_8125a_2, entry_cnt);

	rtl_mac_ocp_write(hw, 0xFC26, 0x8000);

	rtl_mac_ocp_write(hw, 0xFC2A, 0x0540);
	rtl_mac_ocp_write(hw, 0xFC2E, 0x0A06);
	rtl_mac_ocp_write(hw, 0xFC30, 0x0EB8);
	rtl_mac_ocp_write(hw, 0xFC32, 0x3A5C);
	rtl_mac_ocp_write(hw, 0xFC34, 0x10A8);
	rtl_mac_ocp_write(hw, 0xFC40, 0x0D54);
	rtl_mac_ocp_write(hw, 0xFC42, 0x0E24);

	rtl_mac_ocp_write(hw, 0xFC48, 0x307A);
}

/* ------------------------------------PHY 8125A--------------------------------------- */

static void
rtl_acquire_phy_mcu_patch_key_lock(struct rtl_hw *hw)
{
	u16 patch_key;

	switch (hw->mcfg) {
	case CFG_METHOD_48:
		patch_key = 0x8600;
		break;
	case CFG_METHOD_49:
	case CFG_METHOD_52:
		patch_key = 0x8601;
		break;
	case CFG_METHOD_50:
		patch_key = 0x3700;
		break;
	case CFG_METHOD_51:
	case CFG_METHOD_53:
		patch_key = 0x3701;
		break;
	default:
		return;
	}
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8024);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, patch_key);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xB82E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0001);
}

static void
rtl_release_phy_mcu_patch_key_lock(struct rtl_hw *hw)
{
	switch (hw->mcfg) {
	case CFG_METHOD_48:
	case CFG_METHOD_49:
	case CFG_METHOD_50:
	case CFG_METHOD_51:
	case CFG_METHOD_52:
	case CFG_METHOD_53:
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x0000);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
		rtl_clear_eth_phy_ocp_bit(hw, 0xB82E, BIT_0);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0x8024);
		rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
		break;
	default:
		break;
	}
}

static void
rtl_real_set_phy_mcu_8125a_1(struct rtl_hw *hw)
{
	rtl_acquire_phy_mcu_patch_key_lock(hw);

	rtl_set_eth_phy_ocp_bit(hw, 0xB820, BIT_7);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA016);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA012);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA014);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8013);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8021);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x802f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x803d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8042);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8051);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8051);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa088);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a50);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8008);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd014);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd1a3);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x401a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd707);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x40c2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x60a6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5f8b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a86);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a6c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8080);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd019);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd1a2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x401a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd707);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x40c4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x60a6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5f8b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a86);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a84);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd503);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8970);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c07);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0901);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xcf09);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd705);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xceff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xaf0a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1213);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8401);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8580);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1253);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd064);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd181);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd704);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4018);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xc50f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd706);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2c59);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x804d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xc60f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xc605);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xae02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x10fd);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA026);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA024);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA022);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x10f4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA020);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1252);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA006);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1206);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA004);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a78);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a60);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a4f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA008);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x3f00);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA016);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA012);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA014);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8066);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x807c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8089);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x808e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x80a0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x80b2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x80c2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd501);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x62db);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x655c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd73e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x60e9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x614a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x61ab);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0501);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0503);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0505);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0509);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x653c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd73e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x60e9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x614a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x61ab);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0503);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0502);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0506);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x050a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd73e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x60e9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x614a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x61ab);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0505);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0506);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x050c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd73e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x60e9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x614a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x61ab);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0509);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x050a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x050c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0508);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0304);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd501);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd73e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x60e9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x614a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x61ab);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0501);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0321);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0502);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0321);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0321);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0508);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0321);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0346);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd501);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8208);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x609d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa50f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x001a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0503);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x001a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x607d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00ab);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00ab);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd501);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x60fd);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa50f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xaa0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x017b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0503);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a05);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x017b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd501);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x60fd);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa50f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xaa0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x01e0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0503);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a05);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x01e0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x60fd);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa50f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xaa0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0231);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0503);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a05);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0231);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA08E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA08C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0221);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA08A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x01ce);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA088);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0169);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA086);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00a6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA084);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x000d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA082);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0308);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA080);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x029f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA090);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x007f);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA016);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0020);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA012);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA014);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8017);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x801b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8029);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8054);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x805a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8064);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x80a7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x9430);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x9480);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb408);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd120);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd057);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x064b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xcb80);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x9906);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0567);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xcb94);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8190);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x82a0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x800a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8406);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8dff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07e4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa840);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0773);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xcb91);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4063);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd139);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd140);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd040);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb404);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07dc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa610);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa110);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa2a0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa404);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd704);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4045);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa180);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd704);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x405d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa720);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0742);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07ec);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5f74);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0742);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd702);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7fb6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8190);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x82a0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8404);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8610);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07dc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x064b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07c0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5fa7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0481);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x94bc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x870c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa190);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa00a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa280);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa404);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8220);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x078e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xcb92);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa840);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4063);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd140);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd150);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd040);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd703);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x60a0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x6121);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x61a2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x6223);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf02f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d10);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf00f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d20);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf00a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d30);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf005);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d40);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07e4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa610);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa008);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd704);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4046);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd704);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x405d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa720);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0742);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07f7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5f74);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0742);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd702);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7fb5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x800a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07e4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd701);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x3ad4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0537);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8610);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8840);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x064b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8301);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x800a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8190);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x82a0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8404);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa70c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x9402);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x890c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8840);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x064b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA10E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0642);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA10C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0686);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA10A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0788);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA108);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x047b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA106);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x065c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA104);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0769);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA102);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0565);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA100);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x06f9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA110);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00ff);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb87c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8530);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb87e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xaf85);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x3caf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8593);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xaf85);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x9caf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x85a5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf86);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd702);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5afb);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xe083);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfb0c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x020d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x021b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x10bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86d7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86da);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfbe0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x83fc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1b10);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf86);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xda02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf86);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xdd02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5afb);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xe083);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfd0c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x020d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x021b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x10bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86dd);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86e0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfbe0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x83fe);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1b10);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf86);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xe002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xaf2f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbd02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2cac);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0286);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x65af);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x212b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x022c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x6002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86b6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xaf21);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cd1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x03bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8710);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x870d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8719);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8716);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x871f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x871c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8728);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8725);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8707);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfbad);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x281c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd100);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1302);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2202);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2b02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xae1a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd101);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1302);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2202);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2b02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd101);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x3402);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x3102);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x3d02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x3a02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4302);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4c02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4902);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd100);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2e02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x3702);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4602);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf87);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4f02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ab7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xaf35);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7ff8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfaef);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x69bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86e3);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfbbf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86fb);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86e6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfbbf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86fe);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86e9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfbbf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8701);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86ec);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfbbf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8704);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x025a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7bf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86ef);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0262);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7cbf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86f2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0262);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7cbf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86f5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0262);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7cbf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x86f8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0262);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7cef);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x96fe);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfc04);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf8fa);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xef69);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf86);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xef02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x6273);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf86);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf202);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x6273);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf86);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf502);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x6273);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbf86);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf802);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x6273);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xef96);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfefc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0420);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb540);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x53b5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4086);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb540);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb9b5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x40c8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb03a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xc8b0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbac8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb13a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xc8b1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xba77);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbd26);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffbd);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2677);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbd28);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffbd);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2840);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbd26);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xc8bd);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2640);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbd28);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xc8bd);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x28bb);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa430);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x98b0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1eba);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb01e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xdcb0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1e98);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb09e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbab0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x9edc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb09e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x98b1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1eba);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb11e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xdcb1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1e98);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb19e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbab1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x9edc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb19e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x11b0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1e22);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb01e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x33b0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1e11);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb09e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x22b0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x9e33);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb09e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x11b1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1e22);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb11e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x33b1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1e11);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb19e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x22b1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x9e33);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb19e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb85e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2f71);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb860);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x20d9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb862);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x2109);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb864);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x34e7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb878);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x000f);

	rtl_clear_eth_phy_ocp_bit(hw, 0xB820, BIT_7);

	rtl_release_phy_mcu_patch_key_lock(hw);
}

static void
rtl_real_set_phy_mcu_8125a_2(struct rtl_hw *hw)
{
	rtl_acquire_phy_mcu_patch_key_lock(hw);

	rtl_set_eth_phy_ocp_bit(hw, 0xB820, BIT_7);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA016);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA012);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA014);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x808b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x808f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8093);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8097);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x809d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x80a1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x80aa);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd718);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x607b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x40da);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf00e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x42da);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf01e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd718);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x615b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1456);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x14a4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x14bc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd718);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5f2e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf01c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1456);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x14a4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x14bc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd718);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5f2e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf024);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1456);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x14a4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x14bc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd718);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5f2e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf02c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1456);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x14a4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x14bc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd718);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5f2e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf034);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd719);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4118);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xac11);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd501);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa410);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4779);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xac0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xae01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1444);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf034);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd719);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4118);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xac22);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd501);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa420);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4559);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xac0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xae01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1444);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf023);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd719);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4118);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xac44);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd501);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa440);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4339);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xac0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xae01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1444);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf012);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd719);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4118);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xac88);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd501);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa480);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xce00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4119);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xac0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xae01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1444);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf001);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1456);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd718);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5fac);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xc48f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x141b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd504);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x121a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd0b4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd1bb);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0898);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd0b4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd1bb);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a0e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd064);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd18a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0b7e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x401c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd501);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa804);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8804);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x053b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd500);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa301);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0648);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xc520);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa201);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd701);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x252d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1646);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd708);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4006);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1646);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0308);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA026);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0307);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA024);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1645);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA022);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0647);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA020);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x053a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA006);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0b7c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA004);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0a0c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0896);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x11a1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA008);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xff00);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA016);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA012);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA014);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8015);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x801a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x801a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x801a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x801a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x801a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x801a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xad02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x02d7);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00ed);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0509);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xc100);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x008f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA08E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA08C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA08A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA088);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA086);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA084);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA082);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x008d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA080);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00eb);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA090);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0103);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA016);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0020);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA012);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA014);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8014);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8018);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8024);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8051);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8055);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8072);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x80dc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfffd);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfffd);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8301);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x800a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8190);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x82a0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8404);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa70c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x9402);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x890c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8840);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa380);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x066e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xcb91);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4063);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd139);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd140);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd040);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb404);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07e0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa610);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa110);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa2a0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa404);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd704);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4085);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa180);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa404);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8280);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd704);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x405d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa720);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0743);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07f0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5f74);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0743);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd702);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7fb6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8190);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x82a0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8404);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8610);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0c0f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07e0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x066e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd158);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd04d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x03d4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x94bc);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x870c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8380);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd10d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd040);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07c4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5fb4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa190);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa00a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa280);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa404);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa220);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd130);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd040);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07c4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5fb4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xbb80);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd1c4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd074);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa301);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd704);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x604b);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa90c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0556);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xcb92);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4063);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd116);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd119);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd040);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd703);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x60a0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x6241);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x63e2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x6583);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf054);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd701);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x611e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd701);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x40da);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d10);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf02f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d50);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf02a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd701);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x611e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd701);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x40da);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d20);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf021);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d60);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf01c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd701);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x611e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd701);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x40da);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d30);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf013);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d70);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf00e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd701);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x611e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd701);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x40da);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d40);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf005);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d80);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07e8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa610);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd704);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x405d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa720);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd700);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x5ff4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa008);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd704);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x4046);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa002);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0743);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07fb);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd703);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7f6f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7f4e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7f2d);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7f0c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x800a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0cf0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0d00);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07e8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8010);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa740);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0743);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd702);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x7fb5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd701);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x3ad4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0556);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8610);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x066e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd1f5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xd049);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x1800);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x01ec);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA10E);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x01ea);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA10C);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x06a9);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA10A);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x078a);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA108);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x03d2);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA106);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x067f);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA104);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0665);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA102);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA100);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0000);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xA110);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00fc);

	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb87c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8530);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb87e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xaf85);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x3caf);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8545);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xaf85);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x45af);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8545);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xee82);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xf900);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0103);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xaf03);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb7f8);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xe0a6);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x00e1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa601);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xef01);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x58f0);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa080);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x37a1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8402);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xae16);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa185);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x02ae);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x11a1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8702);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xae0c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xa188);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x02ae);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x07a1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x8902);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xae02);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xae1c);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xe0b4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x62e1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb463);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x6901);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xe4b4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x62e5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb463);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xe0b4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x62e1);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb463);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x6901);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xe4b4);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x62e5);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xb463);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xfc04);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb85e);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x03b3);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb860);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb862);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb864);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0xffff);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA436, 0xb878);
	rtl_mdio_direct_write_phy_ocp(hw, 0xA438, 0x0001);

	rtl_clear_eth_phy_ocp_bit(hw, 0xB820, BIT_7);

	rtl_release_phy_mcu_patch_key_lock(hw);
}

void
rtl_set_phy_mcu_8125a_1(struct rtl_hw *hw)
{
	rtl_set_phy_mcu_patch_request(hw);

	rtl_real_set_phy_mcu_8125a_1(hw);

	rtl_clear_phy_mcu_patch_request(hw);
}

void
rtl_set_phy_mcu_8125a_2(struct rtl_hw *hw)
{
	rtl_set_phy_mcu_patch_request(hw);

	rtl_real_set_phy_mcu_8125a_2(hw);

	rtl_clear_phy_mcu_patch_request(hw);
}
