/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "base/ixgbe_type.h"
#include "base/ixgbe_82599.h"
#include "base/ixgbe_api.h"
#include "base/ixgbe_common.h"
#include "base/ixgbe_phy.h"
#include "ixgbe_bypass_defines.h"
#include "ixgbe_bypass.h"

/**
 *  ixgbe_set_fiber_fixed_speed - Set module link speed for fixed fiber
 *  @hw: pointer to hardware structure
 *  @speed: link speed to set
 *
 *  We set the module speed differently for fixed fiber.  For other
 *  multi-speed devices we don't have an error value so here if we
 *  detect an error we just log it and exit.
 */
static void
ixgbe_set_fiber_fixed_speed(struct ixgbe_hw *hw, ixgbe_link_speed speed)
{
	s32 status;
	u8 rs, eeprom_data;

	switch (speed) {
	case IXGBE_LINK_SPEED_10GB_FULL:
		/* one bit mask same as setting on */
		rs = IXGBE_SFF_SOFT_RS_SELECT_10G;
		break;
	case IXGBE_LINK_SPEED_1GB_FULL:
		rs = IXGBE_SFF_SOFT_RS_SELECT_1G;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid fixed module speed");
		return;
	}

	/* Set RS0 */
	status = hw->phy.ops.read_i2c_byte(hw, IXGBE_SFF_SFF_8472_OSCB,
					   IXGBE_I2C_EEPROM_DEV_ADDR2,
					   &eeprom_data);
	if (status) {
		PMD_DRV_LOG(ERR, "Failed to read Rx Rate Select RS0");
		goto out;
	}

	eeprom_data = (eeprom_data & ~IXGBE_SFF_SOFT_RS_SELECT_MASK) & rs;

	status = hw->phy.ops.write_i2c_byte(hw, IXGBE_SFF_SFF_8472_OSCB,
					    IXGBE_I2C_EEPROM_DEV_ADDR2,
					    eeprom_data);
	if (status) {
		PMD_DRV_LOG(ERR, "Failed to write Rx Rate Select RS0");
		goto out;
	}

	/* Set RS1 */
	status = hw->phy.ops.read_i2c_byte(hw, IXGBE_SFF_SFF_8472_ESCB,
					   IXGBE_I2C_EEPROM_DEV_ADDR2,
					   &eeprom_data);
	if (status) {
		PMD_DRV_LOG(ERR, "Failed to read Rx Rate Select RS1");
		goto out;
	}

	eeprom_data = (eeprom_data & ~IXGBE_SFF_SOFT_RS_SELECT_MASK) & rs;

	status = hw->phy.ops.write_i2c_byte(hw, IXGBE_SFF_SFF_8472_ESCB,
					    IXGBE_I2C_EEPROM_DEV_ADDR2,
					    eeprom_data);
	if (status) {
		PMD_DRV_LOG(ERR, "Failed to write Rx Rate Select RS1");
		goto out;
	}
out:
	return;
}

/**
 *  ixgbe_setup_mac_link_multispeed_fixed_fiber - Set MAC link speed
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg_wait_to_complete: true when waiting for completion is needed
 *
 *  Set the link speed in the AUTOC register and restarts link.
 **/
static s32
ixgbe_setup_mac_link_multispeed_fixed_fiber(struct ixgbe_hw *hw,
				     ixgbe_link_speed speed,
				     bool autoneg_wait_to_complete)
{
	s32 status = IXGBE_SUCCESS;
	ixgbe_link_speed link_speed = IXGBE_LINK_SPEED_UNKNOWN;
	ixgbe_link_speed highest_link_speed = IXGBE_LINK_SPEED_UNKNOWN;
	u32 speedcnt = 0;
	u32 esdp_reg = IXGBE_READ_REG(hw, IXGBE_ESDP);
	u32 i = 0;
	bool link_up = false;
	bool negotiation;

	PMD_INIT_FUNC_TRACE();

	/* Mask off requested but non-supported speeds */
	status = ixgbe_get_link_capabilities(hw, &link_speed, &negotiation);
	if (status != IXGBE_SUCCESS)
		return status;

	speed &= link_speed;

	/*
	 * Try each speed one by one, highest priority first.  We do this in
	 * software because 10gb fiber doesn't support speed autonegotiation.
	 */
	if (speed & IXGBE_LINK_SPEED_10GB_FULL) {
		speedcnt++;
		highest_link_speed = IXGBE_LINK_SPEED_10GB_FULL;

		/* If we already have link at this speed, just jump out */
		status = ixgbe_check_link(hw, &link_speed, &link_up, false);
		if (status != IXGBE_SUCCESS)
			return status;

		if ((link_speed == IXGBE_LINK_SPEED_10GB_FULL) && link_up)
			goto out;
		/* Set the module link speed */
		ixgbe_set_fiber_fixed_speed(hw, IXGBE_LINK_SPEED_10GB_FULL);

		/* Set the module link speed */
		esdp_reg |= (IXGBE_ESDP_SDP5_DIR | IXGBE_ESDP_SDP5);
		IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
		IXGBE_WRITE_FLUSH(hw);

		/* Allow module to change analog characteristics (1G->10G) */
		msec_delay(40);

		status = ixgbe_setup_mac_link_82599(hw,
						    IXGBE_LINK_SPEED_10GB_FULL,
						    autoneg_wait_to_complete);
		if (status != IXGBE_SUCCESS)
			return status;

		/* Flap the tx laser if it has not already been done */
		ixgbe_flap_tx_laser(hw);

		/*
		 * Wait for the controller to acquire link.  Per IEEE 802.3ap,
		 * Section 73.10.2, we may have to wait up to 500ms if KR is
		 * attempted.  82599 uses the same timing for 10g SFI.
		 */
		for (i = 0; i < 5; i++) {
			/* Wait for the link partner to also set speed */
			msec_delay(100);

			/* If we have link, just jump out */
			status = ixgbe_check_link(hw, &link_speed,
						  &link_up, false);
			if (status != IXGBE_SUCCESS)
				return status;

			if (link_up)
				goto out;
		}
	}

	if (speed & IXGBE_LINK_SPEED_1GB_FULL) {
		speedcnt++;
		if (highest_link_speed == IXGBE_LINK_SPEED_UNKNOWN)
			highest_link_speed = IXGBE_LINK_SPEED_1GB_FULL;

		/* If we already have link at this speed, just jump out */
		status = ixgbe_check_link(hw, &link_speed, &link_up, false);
		if (status != IXGBE_SUCCESS)
			return status;

		if ((link_speed == IXGBE_LINK_SPEED_1GB_FULL) && link_up)
			goto out;

		/* Set the module link speed */
		ixgbe_set_fiber_fixed_speed(hw, IXGBE_LINK_SPEED_1GB_FULL);

		/* Allow module to change analog characteristics (10G->1G) */
		msec_delay(40);

		status = ixgbe_setup_mac_link_82599(hw,
						    IXGBE_LINK_SPEED_1GB_FULL,
						    autoneg_wait_to_complete);
		if (status != IXGBE_SUCCESS)
			return status;

		/* Flap the tx laser if it has not already been done */
		ixgbe_flap_tx_laser(hw);

		/* Wait for the link partner to also set speed */
		msec_delay(100);

		/* If we have link, just jump out */
		status = ixgbe_check_link(hw, &link_speed, &link_up, false);
		if (status != IXGBE_SUCCESS)
			return status;

		if (link_up)
			goto out;
	}

	/*
	 * We didn't get link.  Configure back to the highest speed we tried,
	 * (if there was more than one).  We call ourselves back with just the
	 * single highest speed that the user requested.
	 */
	if (speedcnt > 1)
		status = ixgbe_setup_mac_link_multispeed_fixed_fiber(hw,
			highest_link_speed, autoneg_wait_to_complete);

out:
	/* Set autoneg_advertised value based on input link speed */
	hw->phy.autoneg_advertised = 0;

	if (speed & IXGBE_LINK_SPEED_10GB_FULL)
		hw->phy.autoneg_advertised |= IXGBE_LINK_SPEED_10GB_FULL;

	if (speed & IXGBE_LINK_SPEED_1GB_FULL)
		hw->phy.autoneg_advertised |= IXGBE_LINK_SPEED_1GB_FULL;

	return status;
}

static enum ixgbe_media_type
ixgbe_bypass_get_media_type(struct ixgbe_hw *hw)
{
	enum ixgbe_media_type media_type;

	PMD_INIT_FUNC_TRACE();

	if (hw->device_id == IXGBE_DEV_ID_82599_BYPASS) {
		media_type = ixgbe_media_type_fiber;
	} else {
		media_type = ixgbe_get_media_type_82599(hw);
	}
	return media_type;
}

/*
 * Wrapper around shared code (base driver) to support BYPASS nic.
 */
s32
ixgbe_bypass_init_shared_code(struct ixgbe_hw *hw)
{
	s32 ret_val;

	if (hw->device_id == IXGBE_DEV_ID_82599_BYPASS) {
		hw->mac.type = ixgbe_mac_82599EB;
	}

	ret_val = ixgbe_init_shared_code(hw);
	if (hw->device_id == IXGBE_DEV_ID_82599_BYPASS) {
		hw->mac.ops.get_media_type = &ixgbe_bypass_get_media_type;
		ixgbe_init_mac_link_ops_82599(hw);
	}

	return ret_val;
}

s32
ixgbe_bypass_init_hw(struct ixgbe_hw *hw)
{
	int rc;

	rc  = ixgbe_init_hw(hw);
	if (rc == 0 && hw->device_id == IXGBE_DEV_ID_82599_BYPASS) {

		hw->mac.ops.setup_link =
			&ixgbe_setup_mac_link_multispeed_fixed_fiber;

		hw->mac.ops.get_media_type = &ixgbe_bypass_get_media_type;

		hw->mac.ops.disable_tx_laser = NULL;
		hw->mac.ops.enable_tx_laser = NULL;
		hw->mac.ops.flap_tx_laser = NULL;
	}

	return rc;
}
