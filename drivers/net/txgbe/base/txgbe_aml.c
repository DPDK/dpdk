/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2025 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "txgbe_type.h"
#include "txgbe_mbx.h"
#include "txgbe_phy.h"
#include "txgbe_dcb.h"
#include "txgbe_vf.h"
#include "txgbe_eeprom.h"
#include "txgbe_mng.h"
#include "txgbe_hw.h"
#include "txgbe_aml.h"

void txgbe_init_ops_aml(struct txgbe_hw *hw)
{
	struct txgbe_mac_info *mac = &hw->mac;
	struct txgbe_phy_info *phy = &hw->phy;

	txgbe_init_ops_generic(hw);

	/* PHY */
	phy->get_media_type = txgbe_get_media_type_aml;

	/* LINK */
	mac->init_mac_link_ops = txgbe_init_mac_link_ops_aml;
	mac->get_link_capabilities = txgbe_get_link_capabilities_aml;
	mac->check_link = txgbe_check_mac_link_aml;
}

s32 txgbe_check_mac_link_aml(struct txgbe_hw *hw, u32 *speed,
				 bool *link_up, bool link_up_wait_to_complete)
{
	u32 links_reg, links_orig;
	u32 i;

	/* clear the old state */
	links_orig = rd32(hw, TXGBE_PORTSTAT);

	links_reg = rd32(hw, TXGBE_PORTSTAT);

	if (links_orig != links_reg) {
		DEBUGOUT("LINKS changed from %08X to %08X",
			  links_orig, links_reg);
	}

	if (link_up_wait_to_complete) {
		for (i = 0; i < hw->mac.max_link_up_time; i++) {
			if (!(links_reg & TXGBE_PORTSTAT_UP)) {
				*link_up = false;
			} else {
				*link_up = true;
				break;
			}
			msec_delay(100);
			links_reg = rd32(hw, TXGBE_PORTSTAT);
		}
	} else {
		if (links_reg & TXGBE_PORTSTAT_UP)
			*link_up = true;
		else
			*link_up = false;
	}

	if (link_up) {
		switch (links_reg & TXGBE_CFG_PORT_ST_AML_LINK_MASK) {
		case TXGBE_CFG_PORT_ST_AML_LINK_25G:
			*speed = TXGBE_LINK_SPEED_25GB_FULL;
			break;
		case TXGBE_CFG_PORT_ST_AML_LINK_10G:
			*speed = TXGBE_LINK_SPEED_10GB_FULL;
			break;
		default:
			*speed = TXGBE_LINK_SPEED_UNKNOWN;
		}
	} else {
		*speed = TXGBE_LINK_SPEED_UNKNOWN;
	}

	return 0;
}

s32 txgbe_get_link_capabilities_aml(struct txgbe_hw *hw,
				      u32 *speed,
				      bool *autoneg)
{
	if (hw->phy.multispeed_fiber) {
		*speed = TXGBE_LINK_SPEED_10GB_FULL |
			 TXGBE_LINK_SPEED_25GB_FULL;
		*autoneg = true;
	} else if (hw->phy.sfp_type == txgbe_sfp_type_25g_sr_core0 ||
		hw->phy.sfp_type == txgbe_sfp_type_25g_sr_core1 ||
		hw->phy.sfp_type == txgbe_sfp_type_25g_lr_core0 ||
		hw->phy.sfp_type == txgbe_sfp_type_25g_lr_core1) {
		*speed = TXGBE_LINK_SPEED_25GB_FULL;
		*autoneg = false;
	} else if (hw->phy.sfp_type == txgbe_sfp_type_25g_aoc_core0 ||
		   hw->phy.sfp_type == txgbe_sfp_type_25g_aoc_core1) {
		*speed = TXGBE_LINK_SPEED_25GB_FULL;
		*autoneg = false;
	} else {
		/* SFP */
		if (hw->phy.sfp_type == txgbe_sfp_type_not_present)
			*speed = TXGBE_LINK_SPEED_25GB_FULL;
		else
			*speed = TXGBE_LINK_SPEED_10GB_FULL;
		*autoneg = true;
	}

	return 0;
}

u32 txgbe_get_media_type_aml(struct txgbe_hw *hw)
{
	u8 device_type = hw->subsystem_device_id & 0xF0;
	enum txgbe_media_type media_type;

	switch (device_type) {
	case TXGBE_DEV_ID_KR_KX_KX4:
		media_type = txgbe_media_type_backplane;
		break;
	case TXGBE_DEV_ID_SFP:
		media_type = txgbe_media_type_fiber;
		break;
	default:
		media_type = txgbe_media_type_unknown;
		break;
	}

	return media_type;
}

static int
txgbe_phy_fec_get(struct txgbe_hw *hw)
{
	int value = 0;

	rte_spinlock_lock(&hw->phy_lock);
	value = rd32_epcs(hw, SR_PMA_RS_FEC_CTRL);
	rte_spinlock_unlock(&hw->phy_lock);
	if (value & 0x4)
		return TXGBE_PHY_FEC_RS;

	rte_spinlock_lock(&hw->phy_lock);
	value = rd32_epcs(hw, SR_PMA_KR_FEC_CTRL);
	rte_spinlock_unlock(&hw->phy_lock);
	if (value & 0x1)
		return TXGBE_PHY_FEC_BASER;

	return TXGBE_PHY_FEC_OFF;
}

void txgbe_wait_for_link_up_aml(struct txgbe_hw *hw, u32 speed)
{
	u32 link_speed = TXGBE_LINK_SPEED_UNKNOWN;
	bool link_up = false;
	int cnt = 0;
	int i;

	if (speed == TXGBE_LINK_SPEED_25GB_FULL)
		cnt = 4;
	else
		cnt = 1;

	for (i = 0; i < (4 * cnt); i++) {
		hw->mac.check_link(hw, &link_speed, &link_up, false);
		if (link_up)
			break;
		msleep(250);
	}
}

s32 txgbe_setup_mac_link_aml(struct txgbe_hw *hw,
			       u32 speed,
			       bool autoneg_wait_to_complete)
{
	bool autoneg = false;
	s32 status = 0;
	u32 link_speed = TXGBE_LINK_SPEED_UNKNOWN;
	bool link_up = false;
	u32 link_capabilities = TXGBE_LINK_SPEED_UNKNOWN;
	u32 value = 0;

	if (hw->phy.sfp_type == txgbe_sfp_type_not_present) {
		DEBUGOUT("SFP not detected, skip setup mac link");
		return 0;
	}

	/* Check to see if speed passed in is supported. */
	status = hw->mac.get_link_capabilities(hw,
			&link_capabilities, &autoneg);
	if (status)
		return status;

	speed &= link_capabilities;
	if (speed == TXGBE_LINK_SPEED_UNKNOWN)
		return TXGBE_ERR_LINK_SETUP;

	value = rd32(hw, TXGBE_GPIOEXT);
	if (value & (TXGBE_SFP1_MOD_ABS_LS | TXGBE_SFP1_RX_LOS_LS))
		return status;

	status = hw->mac.check_link(hw, &link_speed, &link_up,
				    autoneg_wait_to_complete);

	if (link_up && speed == TXGBE_LINK_SPEED_25GB_FULL)
		hw->cur_fec_link = txgbe_phy_fec_get(hw);

	if (link_speed == speed && link_up &&
	   !(speed == TXGBE_LINK_SPEED_25GB_FULL &&
	   !(hw->fec_mode & hw->cur_fec_link)))
		return status;

	if (speed & TXGBE_LINK_SPEED_25GB_FULL)
		speed = 0x10;
	else if (speed & TXGBE_LINK_SPEED_10GB_FULL)
		speed = 0x08;

	status = hw->phy.set_link_hostif(hw, (u8)speed, autoneg, true);

	txgbe_wait_for_link_up_aml(hw, speed);

	return status;
}

/**
 *  txgbe_setup_mac_link_multispeed_fiber_aml - Set MAC link speed
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg_wait_to_complete: true when waiting for completion is needed
 *
 *  Set the link speed in the MAC and/or PHY register and restarts link.
 **/
static s32 txgbe_setup_mac_link_multispeed_fiber_aml(struct txgbe_hw *hw,
					  u32 speed,
					  bool autoneg_wait_to_complete)
{
	u32 link_speed = TXGBE_LINK_SPEED_UNKNOWN;
	u32 highest_link_speed = TXGBE_LINK_SPEED_UNKNOWN;
	s32 status = 0;
	u32 speedcnt = 0;
	bool autoneg, link_up = false;

	/* Mask off requested but non-supported speeds */
	status = hw->mac.get_link_capabilities(hw, &link_speed, &autoneg);
	if (status != 0)
		return status;

	speed &= link_speed;

	/* Try each speed one by one, highest priority first.  We do this in
	 * software because 25Gb fiber doesn't support speed autonegotiation.
	 */
	if (speed & TXGBE_LINK_SPEED_25GB_FULL) {
		speedcnt++;
		highest_link_speed = TXGBE_LINK_SPEED_25GB_FULL;

		/* If we already have link at this speed, just jump out */
		txgbe_e56_check_phy_link(hw, &link_speed, &link_up);

		if (link_speed == TXGBE_LINK_SPEED_25GB_FULL && link_up)
			goto out;

		/* Allow module to change analog characteristics (10G -> 25G) */
		msec_delay(40);

		status = hw->mac.setup_mac_link(hw,
				TXGBE_LINK_SPEED_25GB_FULL,
				autoneg_wait_to_complete);
		if (status != 0)
			return status;

		/* Aml wait link in setup, no need to repeatedly wait */
		/* If we have link, just jump out */
		txgbe_e56_check_phy_link(hw, &link_speed, &link_up);

		if (link_up)
			goto out;
	}

	if (speed & TXGBE_LINK_SPEED_10GB_FULL) {
		speedcnt++;
		if (highest_link_speed == TXGBE_LINK_SPEED_UNKNOWN)
			highest_link_speed = TXGBE_LINK_SPEED_10GB_FULL;

		/* If we already have link at this speed, just jump out */
		txgbe_e56_check_phy_link(hw, &link_speed, &link_up);

		if (link_speed == TXGBE_LINK_SPEED_10GB_FULL && link_up)
			goto out;

		/* Allow module to change analog characteristics (25G->10G) */
		msec_delay(40);

		status = hw->mac.setup_mac_link(hw, TXGBE_LINK_SPEED_10GB_FULL,
				autoneg_wait_to_complete);
		if (status != 0)
			return status;

		/* Aml wait link in setup, no need to repeatedly wait */
		/* If we have link, just jump out */
		txgbe_e56_check_phy_link(hw, &link_speed, &link_up);

		if (link_up)
			goto out;
	}

	/* We didn't get link.  Configure back to the highest speed we tried,
	 * (if there was more than one).  We call ourselves back with just the
	 * single highest speed that the user requested.
	 */
	if (speedcnt > 1)
		status = txgbe_setup_mac_link_multispeed_fiber_aml(hw,
						      highest_link_speed,
						      autoneg_wait_to_complete);

out:
	/* Set autoneg_advertised value based on input link speed */
	hw->phy.autoneg_advertised = 0;

	if (speed & TXGBE_LINK_SPEED_25GB_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_25GB_FULL;

	if (speed & TXGBE_LINK_SPEED_10GB_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_10GB_FULL;

	return status;
}

void txgbe_init_mac_link_ops_aml(struct txgbe_hw *hw)
{
	struct txgbe_mac_info *mac = &hw->mac;

	if (hw->phy.media_type == txgbe_media_type_fiber ||
	    hw->phy.media_type == txgbe_media_type_fiber_qsfp) {
		mac->disable_tx_laser =
			txgbe_disable_tx_laser_multispeed_fiber;
		mac->enable_tx_laser =
			txgbe_enable_tx_laser_multispeed_fiber;
		mac->flap_tx_laser =
			txgbe_flap_tx_laser_multispeed_fiber;

		if (hw->phy.multispeed_fiber) {
			/* Set up dual speed SFP+ support */
			mac->setup_link = txgbe_setup_mac_link_multispeed_fiber_aml;
			mac->setup_mac_link = txgbe_setup_mac_link_aml;
			mac->set_rate_select_speed = txgbe_set_hard_rate_select_speed;
		} else {
			mac->setup_link = txgbe_setup_mac_link_aml;
			mac->set_rate_select_speed = txgbe_set_hard_rate_select_speed;
		}
	}
}
