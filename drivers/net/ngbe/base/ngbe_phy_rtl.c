/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 */

#include "ngbe_phy_rtl.h"

#define RTL_PHY_RST_WAIT_PERIOD               5

s32 ngbe_read_phy_reg_rtl(struct ngbe_hw *hw,
		u32 reg_addr, u32 device_type, u16 *phy_data)
{
	mdi_reg_t reg;
	mdi_reg_22_t reg22;

	reg.device_type = device_type;
	reg.addr = reg_addr;
	ngbe_mdi_map_register(&reg, &reg22);

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

	wr32(hw, NGBE_PHY_CONFIG(RTL_PAGE_SELECT), reg22.page);
	wr32(hw, NGBE_PHY_CONFIG(reg22.addr), phy_data);

	return 0;
}

s32 ngbe_init_phy_rtl(struct ngbe_hw *hw)
{
	int i;
	u16 value = 0;

	/* enable interrupts, only link status change and an done is allowed */
	value = RTL_INER_LSC | RTL_INER_ANC;
	hw->phy.write_reg(hw, RTL_INER, 0xa42, value);

	hw->phy.read_reg(hw, RTL_INSR, 0xa43, &value);

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

	for (i = 0; i < 1000; i++) {
		hw->phy.read_reg(hw, RTL_INSR, 0xa43, &value);
		if (value & RTL_INSR_ACCESS)
			break;
	}

	hw->phy.write_reg(hw, RTL_SCR, 0xa46, RTL_SCR_EFUSE);
	for (i = 0; i < 1000; i++) {
		hw->phy.read_reg(hw, RTL_INSR, 0xa43, &value);
		if (value & RTL_INSR_ACCESS)
			break;
	}
	if (i == 1000)
		return NGBE_ERR_PHY_TIMEOUT;

	hw->phy.write_reg(hw, RTL_SCR, 0xa46, RTL_SCR_EXTINI);
	for (i = 0; i < 1000; i++) {
		hw->phy.read_reg(hw, RTL_INSR, 0xa43, &value);
		if (value & RTL_INSR_ACCESS)
			break;
	}
	if (i == 1000)
		return NGBE_ERR_PHY_TIMEOUT;

	for (i = 0; i < 1000; i++) {
		hw->phy.read_reg(hw, RTL_GSR, 0xa42, &value);
		if ((value & RTL_GSR_ST) == RTL_GSR_ST_LANON)
			break;
	}
	if (i == 1000)
		return NGBE_ERR_PHY_TIMEOUT;

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
	autoneg_reg = 0x205B;
	hw->phy.write_reg(hw, RTL_LCR, 0xd04, autoneg_reg);
	hw->phy.write_reg(hw, RTL_EEELCR, 0xd04, 0);

	hw->phy.read_reg(hw, RTL_LPCR, 0xd04, &autoneg_reg);
	autoneg_reg = autoneg_reg & 0xFFFC;
	/* act led blinking mode set to 60ms */
	autoneg_reg |= 0x2;
	hw->phy.write_reg(hw, RTL_LPCR, 0xd04, autoneg_reg);

	return 0;
}

s32 ngbe_reset_phy_rtl(struct ngbe_hw *hw)
{
	u16 value = 0, i;
	s32 status = 0;

	value |= RTL_BMCR_RESET;
	status = hw->phy.write_reg(hw, RTL_BMCR, RTL_DEV_ZERO, value);

	for (i = 0; i < RTL_PHY_RST_WAIT_PERIOD; i++) {
		status = hw->phy.read_reg(hw, RTL_BMCR, RTL_DEV_ZERO, &value);
		if (!(value & RTL_BMCR_RESET))
			break;
		msleep(1);
	}

	if (i == RTL_PHY_RST_WAIT_PERIOD) {
		DEBUGOUT("PHY reset polling failed to complete.\n");
		return NGBE_ERR_RESET_FAILED;
	}

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

	return status;
}

