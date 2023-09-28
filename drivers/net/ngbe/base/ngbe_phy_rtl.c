/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#include "ngbe_phy_rtl.h"

s32 ngbe_read_phy_reg_rtl(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 *phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;
	ngbe_mdi_map_register(&reg, &reg22);

	if (!(reg22.page == 0xa43 &&
			(reg22.addr == 0x1a || reg22.addr == 0x1d)))
		wr32(hw, NGBE_PHY_CONFIG(RTL_PAGE_SELECT), reg22.page);
	*phy_data = 0xFFFF & rd32(hw, NGBE_PHY_CONFIG(reg22.addr));

	return 0;
}

s32 ngbe_write_phy_reg_rtl(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;
	ngbe_mdi_map_register(&reg, &reg22);

	if (!(reg22.page == 0xa43 &&
			(reg22.addr == 0x1a || reg22.addr == 0x1d)))
		wr32(hw, NGBE_PHY_CONFIG(RTL_PAGE_SELECT), reg22.page);
	wr32(hw, NGBE_PHY_CONFIG(reg22.addr), phy_data);

	return 0;
}

static void ngbe_phy_led_ctrl_rtl(struct ngbe_hw *hw)
{
	u16 value = 0;

	if (hw->led_conf != 0xFFFF)
		value = hw->led_conf & 0xFFFF;
	else
		value = 0x205B;

	hw->phy.write_reg(hw, RTL_LCR, 0xd04, value);
	hw->phy.write_reg(hw, RTL_EEELCR, 0xd04, 0);

	hw->phy.read_reg(hw, RTL_LPCR, 0xd04, &value);
	if (hw->led_conf != 0xFFFF) {
		value &= ~0x73;
		value |= hw->led_conf >> 16;
	} else {
		value &= 0xFFFC;
		/*act led blinking mode set to 60ms*/
		value |= 0x2;
	}
	hw->phy.write_reg(hw, RTL_LPCR, 0xd04, value);
}

static s32 ngbe_wait_mdio_access_on(struct ngbe_hw *hw)
{
	int i;
	u16 val = 0;

	for (i = 0; i < 100; i++) {
		/* irq status */
		hw->phy.read_reg(hw, RTL_INSR, 0xa43, &val);
		if (val & RTL_INSR_ACCESS)
			break;
		msec_delay(1);
	}

	if (i == 100) {
		DEBUGOUT("wait_mdio_access_on timeout");
		return NGBE_ERR_PHY_TIMEOUT;
	}

	return 0;
}

static void ngbe_efuse_calibration(struct ngbe_hw *hw)
{
	u32 efuse[2];

	ngbe_wait_mdio_access_on(hw);

	efuse[0] = hw->gphy_efuse[0];
	efuse[1] = hw->gphy_efuse[1];

	if (!efuse[0] && !efuse[1]) {
		efuse[0] = 0xFFFFFFFF;
		efuse[1] = 0xFFFFFFFF;
	}

	/* calibration */
	efuse[0] |= 0xF0000100;
	efuse[1] |= 0xFF807FFF;
	DEBUGOUT("port %d efuse[0] = %08x, efuse[1] = %08x",
		hw->bus.lan_id, efuse[0], efuse[1]);

	/* EODR, Efuse Output Data Register */
	hw->phy.write_reg(hw, 16, 0xa46, (efuse[0] >>  0) & 0xFFFF);
	hw->phy.write_reg(hw, 17, 0xa46, (efuse[0] >> 16) & 0xFFFF);
	hw->phy.write_reg(hw, 18, 0xa46, (efuse[1] >>  0) & 0xFFFF);
	hw->phy.write_reg(hw, 19, 0xa46, (efuse[1] >> 16) & 0xFFFF);
}

s32 ngbe_init_phy_rtl(struct ngbe_hw *hw)
{
	int i;
	u16 value = 0;

	hw->init_phy = true;
	msec_delay(1);

	hw->phy.set_phy_power(hw, true);

	for (i = 0; i < 15; i++) {
		if (!rd32m(hw, NGBE_STAT,
			NGBE_STAT_GPHY_IN_RST(hw->bus.lan_id)))
			break;

		msec_delay(10);
	}
	if (i == 15) {
		DEBUGOUT("GPhy reset exceeds maximum times.");
		return NGBE_ERR_PHY_TIMEOUT;
	}

	ngbe_efuse_calibration(hw);

	hw->phy.write_reg(hw, RTL_SCR, 0xa46, RTL_SCR_EFUSE);
	hw->phy.read_reg(hw, RTL_SCR, 0xa46, &value);
	if (!(value & RTL_SCR_EFUSE)) {
		DEBUGOUT("Write EFUSE failed.");
		return NGBE_ERR_PHY_TIMEOUT;
	}

	ngbe_wait_mdio_access_on(hw);

	hw->phy.write_reg(hw, 27, 0xa42, 0x8011);
	hw->phy.write_reg(hw, 28, 0xa42, 0x5737);

	/* Disable fall to 100m if signal is not good */
	hw->phy.read_reg(hw, 17, 0xa44, &value);
	value &= ~0x8;
	hw->phy.write_reg(hw, 17, 0xa44, value);

	hw->phy.write_reg(hw, RTL_SCR, 0xa46, RTL_SCR_EXTINI);
	hw->phy.read_reg(hw, RTL_SCR, 0xa46, &value);
	if (!(value & RTL_SCR_EXTINI)) {
		DEBUGOUT("Write EXIINI failed.");
		return NGBE_ERR_PHY_TIMEOUT;
	}

	ngbe_wait_mdio_access_on(hw);

	for (i = 0; i < 100; i++) {
		hw->phy.read_reg(hw, RTL_GSR, 0xa42, &value);
		if ((value & RTL_GSR_ST) == RTL_GSR_ST_LANON)
			break;
		msec_delay(1);
	}
	if (i == 100)
		return NGBE_ERR_PHY_TIMEOUT;

	/* Disable EEE */
	hw->phy.write_reg(hw, 0x11, 0xa4b, 0x1110);
	hw->phy.write_reg(hw, 0xd, 0x0, 0x0007);
	hw->phy.write_reg(hw, 0xe, 0x0, 0x003c);
	hw->phy.write_reg(hw, 0xd, 0x0, 0x4007);
	hw->phy.write_reg(hw, 0xe, 0x0, 0x0000);

	hw->init_phy = false;

	return 0;
}

/**
 *  ngbe_setup_phy_link_rtl - Set and restart auto-neg
 *  @hw: pointer to hardware structure
 *
 *  Restart auto-negotiation and PHY and waits for completion.
 **/
s32 ngbe_setup_phy_link_rtl(struct ngbe_hw *hw,
		u32 speed, bool autoneg_wait_to_complete)
{
	u16 autoneg_reg = NGBE_MII_AUTONEG_REG;
	u16 value = 0;

	UNREFERENCED_PARAMETER(autoneg_wait_to_complete);

	hw->init_phy = true;
	msec_delay(1);

	hw->phy.read_reg(hw, RTL_INSR, 0xa43, &autoneg_reg);

	if (!hw->mac.autoneg) {
		hw->phy.reset_hw(hw);

		switch (speed) {
		case NGBE_LINK_SPEED_1GB_FULL:
			value = RTL_BMCR_SPEED_SELECT1;
			break;
		case NGBE_LINK_SPEED_100M_FULL:
			value = RTL_BMCR_SPEED_SELECT0;
			break;
		case NGBE_LINK_SPEED_10M_FULL:
			value = 0;
			break;
		default:
			value = RTL_BMCR_SPEED_SELECT1 | RTL_BMCR_SPEED_SELECT0;
			DEBUGOUT("unknown speed = 0x%x.", speed);
			break;
		}
		/* duplex full */
		value |= RTL_BMCR_DUPLEX;
		hw->phy.write_reg(hw, RTL_BMCR, RTL_DEV_ZERO, value);

		goto skip_an;
	}

	/*
	 * Clear autoneg_advertised and set new values based on input link
	 * speed.
	 */
	if (speed) {
		hw->phy.autoneg_advertised = 0;

		if (speed & NGBE_LINK_SPEED_1GB_FULL)
			hw->phy.autoneg_advertised |= NGBE_LINK_SPEED_1GB_FULL;

		if (speed & NGBE_LINK_SPEED_100M_FULL)
			hw->phy.autoneg_advertised |= NGBE_LINK_SPEED_100M_FULL;

		if (speed & NGBE_LINK_SPEED_10M_FULL)
			hw->phy.autoneg_advertised |= NGBE_LINK_SPEED_10M_FULL;
	}

	/* disable 10/100M Half Duplex */
	hw->phy.read_reg(hw, RTL_ANAR, RTL_DEV_ZERO, &autoneg_reg);
	autoneg_reg &= 0xFF5F;
	hw->phy.write_reg(hw, RTL_ANAR, RTL_DEV_ZERO, autoneg_reg);

	/* set advertise enable according to input speed */
	if (!(speed & NGBE_LINK_SPEED_1GB_FULL)) {
		hw->phy.read_reg(hw, RTL_GBCR,
			RTL_DEV_ZERO, &autoneg_reg);
		autoneg_reg &= ~RTL_GBCR_1000F;
		hw->phy.write_reg(hw, RTL_GBCR,
			RTL_DEV_ZERO, autoneg_reg);
	} else {
		hw->phy.read_reg(hw, RTL_GBCR,
			RTL_DEV_ZERO, &autoneg_reg);
		autoneg_reg |= RTL_GBCR_1000F;
		hw->phy.write_reg(hw, RTL_GBCR,
			RTL_DEV_ZERO, autoneg_reg);
	}

	if (!(speed & NGBE_LINK_SPEED_100M_FULL)) {
		hw->phy.read_reg(hw, RTL_ANAR,
			RTL_DEV_ZERO, &autoneg_reg);
		autoneg_reg &= ~RTL_ANAR_100F;
		autoneg_reg &= ~RTL_ANAR_100H;
		hw->phy.write_reg(hw, RTL_ANAR,
			RTL_DEV_ZERO, autoneg_reg);
	} else {
		hw->phy.read_reg(hw, RTL_ANAR,
			RTL_DEV_ZERO, &autoneg_reg);
		autoneg_reg |= RTL_ANAR_100F;
		hw->phy.write_reg(hw, RTL_ANAR,
			RTL_DEV_ZERO, autoneg_reg);
	}

	if (!(speed & NGBE_LINK_SPEED_10M_FULL)) {
		hw->phy.read_reg(hw, RTL_ANAR,
			RTL_DEV_ZERO, &autoneg_reg);
		autoneg_reg &= ~RTL_ANAR_10F;
		autoneg_reg &= ~RTL_ANAR_10H;
		hw->phy.write_reg(hw, RTL_ANAR,
			RTL_DEV_ZERO, autoneg_reg);
	} else {
		hw->phy.read_reg(hw, RTL_ANAR,
			RTL_DEV_ZERO, &autoneg_reg);
		autoneg_reg |= RTL_ANAR_10F;
		hw->phy.write_reg(hw, RTL_ANAR,
			RTL_DEV_ZERO, autoneg_reg);
	}

	/* restart AN and wait AN done interrupt */
	autoneg_reg = RTL_BMCR_RESTART_AN | RTL_BMCR_ANE;
	hw->phy.write_reg(hw, RTL_BMCR, RTL_DEV_ZERO, autoneg_reg);

skip_an:
	ngbe_phy_led_ctrl_rtl(hw);

	hw->init_phy = false;

	return 0;
}

s32 ngbe_reset_phy_rtl(struct ngbe_hw *hw)
{
	u16 value = 0;
	s32 status = 0;

	value |= RTL_BMCR_RESET;
	status = hw->phy.write_reg(hw, RTL_BMCR, RTL_DEV_ZERO, value);

	msec_delay(5);

	return status;
}

s32 ngbe_get_phy_advertised_pause_rtl(struct ngbe_hw *hw, u8 *pause_bit)
{
	u16 value;
	s32 status = 0;

	status = hw->phy.read_reg(hw, RTL_ANAR, RTL_DEV_ZERO, &value);
	value &= RTL_ANAR_APAUSE | RTL_ANAR_PAUSE;
	*pause_bit = (u8)(value >> 10);
	return status;
}

s32 ngbe_get_phy_lp_advertised_pause_rtl(struct ngbe_hw *hw, u8 *pause_bit)
{
	u16 value;
	s32 status = 0;

	status = hw->phy.read_reg(hw, RTL_INSR, 0xa43, &value);

	status = hw->phy.read_reg(hw, RTL_BMSR, RTL_DEV_ZERO, &value);
	value = value & RTL_BMSR_ANC;

	/* if AN complete then check lp adv pause */
	status = hw->phy.read_reg(hw, RTL_ANLPAR, RTL_DEV_ZERO, &value);
	value &= RTL_ANLPAR_LP;
	*pause_bit = (u8)(value >> 10);
	return status;
}

s32 ngbe_set_phy_pause_adv_rtl(struct ngbe_hw *hw, u16 pause_bit)
{
	u16 value;
	s32 status = 0;

	status = hw->phy.read_reg(hw, RTL_ANAR, RTL_DEV_ZERO, &value);
	value &= ~(RTL_ANAR_APAUSE | RTL_ANAR_PAUSE);
	value |= pause_bit;

	status = hw->phy.write_reg(hw, RTL_ANAR, RTL_DEV_ZERO, value);

	return status;
}

s32 ngbe_check_phy_link_rtl(struct ngbe_hw *hw, u32 *speed, bool *link_up)
{
	s32 status = 0;
	u16 phy_link = 0;
	u16 phy_speed = 0;
	u16 phy_data = 0;
	u16 insr = 0;

	if (hw->init_phy)
		return -1;

	hw->phy.read_reg(hw, RTL_INSR, 0xa43, &insr);

	/* Initialize speed and link to default case */
	*link_up = false;
	*speed = NGBE_LINK_SPEED_UNKNOWN;

	/*
	 * Check current speed and link status of the PHY register.
	 * This is a vendor specific register and may have to
	 * be changed for other copper PHYs.
	 */
	status = hw->phy.read_reg(hw, RTL_PHYSR, 0xa43, &phy_data);
	phy_link = phy_data & RTL_PHYSR_RTLS;
	phy_speed = phy_data & (RTL_PHYSR_SPEED_MASK | RTL_PHYSR_DP);
	if (phy_link == RTL_PHYSR_RTLS) {
		*link_up = true;

		if (phy_speed == (RTL_PHYSR_SPEED_1000M | RTL_PHYSR_DP))
			*speed = NGBE_LINK_SPEED_1GB_FULL;
		else if (phy_speed == (RTL_PHYSR_SPEED_100M | RTL_PHYSR_DP))
			*speed = NGBE_LINK_SPEED_100M_FULL;
		else if (phy_speed == (RTL_PHYSR_SPEED_10M | RTL_PHYSR_DP))
			*speed = NGBE_LINK_SPEED_10M_FULL;
	}

	if (hw->lsc)
		return status;

	/*
	 * Because of the slow speed of getting link state, RTL_PHYSR
	 * may still be up while the actual link state is down.
	 * So we read RTL_GBSR to get accurate state when speed is 1G
	 * in polling mode.
	 */
	if (*speed == NGBE_LINK_SPEED_1GB_FULL) {
		status = hw->phy.read_reg(hw, RTL_GBSR,
				RTL_DEV_ZERO, &phy_data);
		phy_link = phy_data & RTL_GBSR_LRS;

		/* Only need to detect link down */
		if (!phy_link) {
			*link_up = false;
			*speed = NGBE_LINK_SPEED_UNKNOWN;
		}
	}
	return status;
}

s32 ngbe_set_phy_power_rtl(struct ngbe_hw *hw, bool on)
{
	u16 value = 0;

	hw->phy.read_reg(hw, RTL_BMCR, 0, &value);
	if (on)
		value &= ~RTL_BMCR_PWDN;
	else
		value |= RTL_BMCR_PWDN;
	hw->phy.write_reg(hw, RTL_BMCR, 0, value);

	return 0;
}
