/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include "txgbe_hw.h"
#include "txgbe_eeprom.h"
#include "txgbe_mng.h"
#include "txgbe_phy.h"

static void txgbe_i2c_start(struct txgbe_hw *hw);
static void txgbe_i2c_stop(struct txgbe_hw *hw);

/**
 * txgbe_identify_extphy - Identify a single address for a PHY
 * @hw: pointer to hardware structure
 * @phy_addr: PHY address to probe
 *
 * Returns true if PHY found
 */
static bool txgbe_identify_extphy(struct txgbe_hw *hw)
{
	u16 phy_addr = 0;

	if (!txgbe_validate_phy_addr(hw, phy_addr)) {
		DEBUGOUT("Unable to validate PHY address 0x%04X\n",
			phy_addr);
		return false;
	}

	if (txgbe_get_phy_id(hw))
		return false;

	hw->phy.type = txgbe_get_phy_type_from_id(hw->phy.id);
	if (hw->phy.type == txgbe_phy_unknown) {
		u16 ext_ability = 0;
		hw->phy.read_reg(hw, TXGBE_MD_PHY_EXT_ABILITY,
				 TXGBE_MD_DEV_PMA_PMD,
				 &ext_ability);

		if (ext_ability & (TXGBE_MD_PHY_10GBASET_ABILITY |
			TXGBE_MD_PHY_1000BASET_ABILITY))
			hw->phy.type = txgbe_phy_cu_unknown;
		else
			hw->phy.type = txgbe_phy_generic;
	}

	return true;
}

/**
 *  txgbe_read_phy_if - Read TXGBE_ETHPHYIF register
 *  @hw: pointer to hardware structure
 *
 *  Read TXGBE_ETHPHYIF register and save field values,
 *  and check for valid field values.
 **/
static s32 txgbe_read_phy_if(struct txgbe_hw *hw)
{
	hw->phy.media_type = hw->phy.get_media_type(hw);

	/* Save NW management interface connected on board. This is used
	 * to determine internal PHY mode.
	 */
	hw->phy.nw_mng_if_sel = rd32(hw, TXGBE_ETHPHYIF);

	/* If MDIO is connected to external PHY, then set PHY address. */
	if (hw->phy.nw_mng_if_sel & TXGBE_ETHPHYIF_MDIO_ACT)
		hw->phy.addr = TXGBE_ETHPHYIF_MDIO_BASE(hw->phy.nw_mng_if_sel);

	if (!hw->phy.phy_semaphore_mask) {
		if (hw->bus.lan_id)
			hw->phy.phy_semaphore_mask = TXGBE_MNGSEM_SWPHY;
		else
			hw->phy.phy_semaphore_mask = TXGBE_MNGSEM_SWPHY;
	}

	return 0;
}

/**
 *  txgbe_identify_phy - Get physical layer module
 *  @hw: pointer to hardware structure
 *
 *  Determines the physical layer module found on the current adapter.
 **/
s32 txgbe_identify_phy(struct txgbe_hw *hw)
{
	s32 err = TXGBE_ERR_PHY_ADDR_INVALID;

	DEBUGFUNC("txgbe_identify_phy");

	txgbe_read_phy_if(hw);

	if (hw->phy.type != txgbe_phy_unknown)
		return 0;

	/* Raptor 10GBASE-T requires an external PHY */
	if (hw->phy.media_type == txgbe_media_type_copper) {
		err = txgbe_identify_extphy(hw);
	} else if (hw->phy.media_type == txgbe_media_type_fiber) {
		err = txgbe_identify_module(hw);
	} else {
		hw->phy.type = txgbe_phy_none;
		return 0;
	}

	/* Return error if SFP module has been detected but is not supported */
	if (hw->phy.type == txgbe_phy_sfp_unsupported)
		return TXGBE_ERR_SFP_NOT_SUPPORTED;

	return err;
}

/**
 * txgbe_check_reset_blocked - check status of MNG FW veto bit
 * @hw: pointer to the hardware structure
 *
 * This function checks the STAT.MNGVETO bit to see if there are
 * any constraints on link from manageability.  For MAC's that don't
 * have this bit just return faluse since the link can not be blocked
 * via this method.
 **/
s32 txgbe_check_reset_blocked(struct txgbe_hw *hw)
{
	u32 mmngc;

	DEBUGFUNC("txgbe_check_reset_blocked");

	mmngc = rd32(hw, TXGBE_STAT);
	if (mmngc & TXGBE_STAT_MNGVETO) {
		DEBUGOUT("MNG_VETO bit detected.\n");
		return true;
	}

	return false;
}

/**
 *  txgbe_validate_phy_addr - Determines phy address is valid
 *  @hw: pointer to hardware structure
 *  @phy_addr: PHY address
 *
 **/
bool txgbe_validate_phy_addr(struct txgbe_hw *hw, u32 phy_addr)
{
	u16 phy_id = 0;
	bool valid = false;

	DEBUGFUNC("txgbe_validate_phy_addr");

	hw->phy.addr = phy_addr;
	hw->phy.read_reg(hw, TXGBE_MD_PHY_ID_HIGH,
			     TXGBE_MD_DEV_PMA_PMD, &phy_id);

	if (phy_id != 0xFFFF && phy_id != 0x0)
		valid = true;

	DEBUGOUT("PHY ID HIGH is 0x%04X\n", phy_id);

	return valid;
}

/**
 *  txgbe_get_phy_id - Get the phy type
 *  @hw: pointer to hardware structure
 *
 **/
s32 txgbe_get_phy_id(struct txgbe_hw *hw)
{
	u32 err;
	u16 phy_id_high = 0;
	u16 phy_id_low = 0;

	DEBUGFUNC("txgbe_get_phy_id");

	err = hw->phy.read_reg(hw, TXGBE_MD_PHY_ID_HIGH,
				      TXGBE_MD_DEV_PMA_PMD,
				      &phy_id_high);

	if (err == 0) {
		hw->phy.id = (u32)(phy_id_high << 16);
		err = hw->phy.read_reg(hw, TXGBE_MD_PHY_ID_LOW,
					      TXGBE_MD_DEV_PMA_PMD,
					      &phy_id_low);
		hw->phy.id |= (u32)(phy_id_low & TXGBE_PHY_REVISION_MASK);
		hw->phy.revision = (u32)(phy_id_low & ~TXGBE_PHY_REVISION_MASK);
	}
	DEBUGOUT("PHY_ID_HIGH 0x%04X, PHY_ID_LOW 0x%04X\n",
		  phy_id_high, phy_id_low);

	return err;
}

/**
 *  txgbe_get_phy_type_from_id - Get the phy type
 *  @phy_id: PHY ID information
 *
 **/
enum txgbe_phy_type txgbe_get_phy_type_from_id(u32 phy_id)
{
	enum txgbe_phy_type phy_type;

	DEBUGFUNC("txgbe_get_phy_type_from_id");

	switch (phy_id) {
	case TXGBE_PHYID_TN1010:
		phy_type = txgbe_phy_tn;
		break;
	case TXGBE_PHYID_QT2022:
		phy_type = txgbe_phy_qt;
		break;
	case TXGBE_PHYID_ATH:
		phy_type = txgbe_phy_nl;
		break;
	case TXGBE_PHYID_MTD3310:
		phy_type = txgbe_phy_cu_mtd;
		break;
	default:
		phy_type = txgbe_phy_unknown;
		break;
	}

	return phy_type;
}

static s32
txgbe_reset_extphy(struct txgbe_hw *hw)
{
	u16 ctrl = 0;
	int err, i;

	err = hw->phy.read_reg(hw, TXGBE_MD_PORT_CTRL,
			TXGBE_MD_DEV_GENERAL, &ctrl);
	if (err != 0)
		return err;
	ctrl |= TXGBE_MD_PORT_CTRL_RESET;
	err = hw->phy.write_reg(hw, TXGBE_MD_PORT_CTRL,
			TXGBE_MD_DEV_GENERAL, ctrl);
	if (err != 0)
		return err;

	/*
	 * Poll for reset bit to self-clear indicating reset is complete.
	 * Some PHYs could take up to 3 seconds to complete and need about
	 * 1.7 usec delay after the reset is complete.
	 */
	for (i = 0; i < 30; i++) {
		msec_delay(100);
		err = hw->phy.read_reg(hw, TXGBE_MD_PORT_CTRL,
			TXGBE_MD_DEV_GENERAL, &ctrl);
		if (err != 0)
			return err;

		if (!(ctrl & TXGBE_MD_PORT_CTRL_RESET)) {
			usec_delay(2);
			break;
		}
	}

	if (ctrl & TXGBE_MD_PORT_CTRL_RESET) {
		err = TXGBE_ERR_RESET_FAILED;
		DEBUGOUT("PHY reset polling failed to complete.\n");
	}

	return err;
}

/**
 *  txgbe_reset_phy - Performs a PHY reset
 *  @hw: pointer to hardware structure
 **/
s32 txgbe_reset_phy(struct txgbe_hw *hw)
{
	s32 err = 0;

	DEBUGFUNC("txgbe_reset_phy");

	if (hw->phy.type == txgbe_phy_unknown)
		err = txgbe_identify_phy(hw);

	if (err != 0 || hw->phy.type == txgbe_phy_none)
		return err;

	/* Don't reset PHY if it's shut down due to overtemp. */
	if (hw->phy.check_overtemp(hw) == TXGBE_ERR_OVERTEMP)
		return err;

	/* Blocked by MNG FW so bail */
	if (txgbe_check_reset_blocked(hw))
		return err;

	switch (hw->phy.type) {
	case txgbe_phy_cu_mtd:
		err = txgbe_reset_extphy(hw);
		break;
	default:
		break;
	}

	return err;
}

/**
 *  txgbe_read_phy_mdi - Reads a value from a specified PHY register without
 *  the SWFW lock
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit address of PHY register to read
 *  @device_type: 5 bit device type
 *  @phy_data: Pointer to read data from PHY register
 **/
s32 txgbe_read_phy_reg_mdi(struct txgbe_hw *hw, u32 reg_addr, u32 device_type,
			   u16 *phy_data)
{
	u32 command, data;

	/* Setup and write the address cycle command */
	command = TXGBE_MDIOSCA_REG(reg_addr) |
		  TXGBE_MDIOSCA_DEV(device_type) |
		  TXGBE_MDIOSCA_PORT(hw->phy.addr);
	wr32(hw, TXGBE_MDIOSCA, command);

	command = TXGBE_MDIOSCD_CMD_READ |
		  TXGBE_MDIOSCD_BUSY;
	wr32(hw, TXGBE_MDIOSCD, command);

	/*
	 * Check every 10 usec to see if the address cycle completed.
	 * The MDI Command bit will clear when the operation is
	 * complete
	 */
	if (!po32m(hw, TXGBE_MDIOSCD, TXGBE_MDIOSCD_BUSY,
		0, NULL, 100, 100)) {
		DEBUGOUT("PHY address command did not complete\n");
		return TXGBE_ERR_PHY;
	}

	data = rd32(hw, TXGBE_MDIOSCD);
	*phy_data = (u16)TXGBD_MDIOSCD_DAT(data);

	return 0;
}

/**
 *  txgbe_read_phy_reg - Reads a value from a specified PHY register
 *  using the SWFW lock - this function is needed in most cases
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit address of PHY register to read
 *  @device_type: 5 bit device type
 *  @phy_data: Pointer to read data from PHY register
 **/
s32 txgbe_read_phy_reg(struct txgbe_hw *hw, u32 reg_addr,
			       u32 device_type, u16 *phy_data)
{
	s32 err;
	u32 gssr = hw->phy.phy_semaphore_mask;

	DEBUGFUNC("txgbe_read_phy_reg");

	if (hw->mac.acquire_swfw_sync(hw, gssr))
		return TXGBE_ERR_SWFW_SYNC;

	err = hw->phy.read_reg_mdi(hw, reg_addr, device_type, phy_data);

	hw->mac.release_swfw_sync(hw, gssr);

	return err;
}

/**
 *  txgbe_write_phy_reg_mdi - Writes a value to specified PHY register
 *  without SWFW lock
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit PHY register to write
 *  @device_type: 5 bit device type
 *  @phy_data: Data to write to the PHY register
 **/
s32 txgbe_write_phy_reg_mdi(struct txgbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 phy_data)
{
	u32 command;

	/* write command */
	command = TXGBE_MDIOSCA_REG(reg_addr) |
		  TXGBE_MDIOSCA_DEV(device_type) |
		  TXGBE_MDIOSCA_PORT(hw->phy.addr);
	wr32(hw, TXGBE_MDIOSCA, command);

	command = TXGBE_MDIOSCD_CMD_WRITE |
		  TXGBE_MDIOSCD_DAT(phy_data) |
		  TXGBE_MDIOSCD_BUSY;
	wr32(hw, TXGBE_MDIOSCD, command);

	/* wait for completion */
	if (!po32m(hw, TXGBE_MDIOSCD, TXGBE_MDIOSCD_BUSY,
		0, NULL, 100, 100)) {
		TLOG_DEBUG("PHY write cmd didn't complete\n");
		return -TERR_PHY;
	}

	return 0;
}

/**
 *  txgbe_write_phy_reg - Writes a value to specified PHY register
 *  using SWFW lock- this function is needed in most cases
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit PHY register to write
 *  @device_type: 5 bit device type
 *  @phy_data: Data to write to the PHY register
 **/
s32 txgbe_write_phy_reg(struct txgbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 phy_data)
{
	s32 err;
	u32 gssr = hw->phy.phy_semaphore_mask;

	DEBUGFUNC("txgbe_write_phy_reg");

	if (hw->mac.acquire_swfw_sync(hw, gssr))
		err = TXGBE_ERR_SWFW_SYNC;

	err = hw->phy.write_reg_mdi(hw, reg_addr, device_type,
					 phy_data);
	hw->mac.release_swfw_sync(hw, gssr);

	return err;
}

/**
 *  txgbe_setup_phy_link - Set and restart auto-neg
 *  @hw: pointer to hardware structure
 *
 *  Restart auto-negotiation and PHY and waits for completion.
 **/
s32 txgbe_setup_phy_link(struct txgbe_hw *hw)
{
	s32 err = 0;
	u16 autoneg_reg = TXGBE_MII_AUTONEG_REG;
	bool autoneg = false;
	u32 speed;

	DEBUGFUNC("txgbe_setup_phy_link");

	txgbe_get_copper_link_capabilities(hw, &speed, &autoneg);

	/* Set or unset auto-negotiation 10G advertisement */
	hw->phy.read_reg(hw, TXGBE_MII_10GBASE_T_AUTONEG_CTRL_REG,
			     TXGBE_MD_DEV_AUTO_NEG,
			     &autoneg_reg);

	autoneg_reg &= ~TXGBE_MII_10GBASE_T_ADVERTISE;
	if ((hw->phy.autoneg_advertised & TXGBE_LINK_SPEED_10GB_FULL) &&
	    (speed & TXGBE_LINK_SPEED_10GB_FULL))
		autoneg_reg |= TXGBE_MII_10GBASE_T_ADVERTISE;

	hw->phy.write_reg(hw, TXGBE_MII_10GBASE_T_AUTONEG_CTRL_REG,
			      TXGBE_MD_DEV_AUTO_NEG,
			      autoneg_reg);

	hw->phy.read_reg(hw, TXGBE_MII_AUTONEG_VENDOR_PROVISION_1_REG,
			     TXGBE_MD_DEV_AUTO_NEG,
			     &autoneg_reg);

	/* Set or unset auto-negotiation 5G advertisement */
	autoneg_reg &= ~TXGBE_MII_5GBASE_T_ADVERTISE;
	if ((hw->phy.autoneg_advertised & TXGBE_LINK_SPEED_5GB_FULL) &&
	    (speed & TXGBE_LINK_SPEED_5GB_FULL))
		autoneg_reg |= TXGBE_MII_5GBASE_T_ADVERTISE;

	/* Set or unset auto-negotiation 2.5G advertisement */
	autoneg_reg &= ~TXGBE_MII_2_5GBASE_T_ADVERTISE;
	if ((hw->phy.autoneg_advertised &
	     TXGBE_LINK_SPEED_2_5GB_FULL) &&
	    (speed & TXGBE_LINK_SPEED_2_5GB_FULL))
		autoneg_reg |= TXGBE_MII_2_5GBASE_T_ADVERTISE;
	/* Set or unset auto-negotiation 1G advertisement */
	autoneg_reg &= ~TXGBE_MII_1GBASE_T_ADVERTISE;
	if ((hw->phy.autoneg_advertised & TXGBE_LINK_SPEED_1GB_FULL) &&
	    (speed & TXGBE_LINK_SPEED_1GB_FULL))
		autoneg_reg |= TXGBE_MII_1GBASE_T_ADVERTISE;

	hw->phy.write_reg(hw, TXGBE_MII_AUTONEG_VENDOR_PROVISION_1_REG,
			      TXGBE_MD_DEV_AUTO_NEG,
			      autoneg_reg);

	/* Set or unset auto-negotiation 100M advertisement */
	hw->phy.read_reg(hw, TXGBE_MII_AUTONEG_ADVERTISE_REG,
			     TXGBE_MD_DEV_AUTO_NEG,
			     &autoneg_reg);

	autoneg_reg &= ~(TXGBE_MII_100BASE_T_ADVERTISE |
			 TXGBE_MII_100BASE_T_ADVERTISE_HALF);
	if ((hw->phy.autoneg_advertised & TXGBE_LINK_SPEED_100M_FULL) &&
	    (speed & TXGBE_LINK_SPEED_100M_FULL))
		autoneg_reg |= TXGBE_MII_100BASE_T_ADVERTISE;

	hw->phy.write_reg(hw, TXGBE_MII_AUTONEG_ADVERTISE_REG,
			      TXGBE_MD_DEV_AUTO_NEG,
			      autoneg_reg);

	/* Blocked by MNG FW so don't reset PHY */
	if (txgbe_check_reset_blocked(hw))
		return err;

	/* Restart PHY auto-negotiation. */
	hw->phy.read_reg(hw, TXGBE_MD_AUTO_NEG_CONTROL,
			     TXGBE_MD_DEV_AUTO_NEG, &autoneg_reg);

	autoneg_reg |= TXGBE_MII_RESTART;

	hw->phy.write_reg(hw, TXGBE_MD_AUTO_NEG_CONTROL,
			      TXGBE_MD_DEV_AUTO_NEG, autoneg_reg);

	return err;
}

/**
 *  txgbe_setup_phy_link_speed - Sets the auto advertised capabilities
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg_wait_to_complete: unused
 **/
s32 txgbe_setup_phy_link_speed(struct txgbe_hw *hw,
				       u32 speed,
				       bool autoneg_wait_to_complete)
{
	UNREFERENCED_PARAMETER(autoneg_wait_to_complete);

	DEBUGFUNC("txgbe_setup_phy_link_speed");

	/*
	 * Clear autoneg_advertised and set new values based on input link
	 * speed.
	 */
	hw->phy.autoneg_advertised = 0;

	if (speed & TXGBE_LINK_SPEED_10GB_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_10GB_FULL;

	if (speed & TXGBE_LINK_SPEED_5GB_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_5GB_FULL;

	if (speed & TXGBE_LINK_SPEED_2_5GB_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_2_5GB_FULL;

	if (speed & TXGBE_LINK_SPEED_1GB_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_1GB_FULL;

	if (speed & TXGBE_LINK_SPEED_100M_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_100M_FULL;

	if (speed & TXGBE_LINK_SPEED_10M_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_10M_FULL;

	/* Setup link based on the new speed settings */
	hw->phy.setup_link(hw);

	return 0;
}

/**
 * txgbe_get_copper_speeds_supported - Get copper link speeds from phy
 * @hw: pointer to hardware structure
 *
 * Determines the supported link capabilities by reading the PHY auto
 * negotiation register.
 **/
static s32 txgbe_get_copper_speeds_supported(struct txgbe_hw *hw)
{
	s32 err;
	u16 speed_ability;

	err = hw->phy.read_reg(hw, TXGBE_MD_PHY_SPEED_ABILITY,
				      TXGBE_MD_DEV_PMA_PMD,
				      &speed_ability);
	if (err)
		return err;

	if (speed_ability & TXGBE_MD_PHY_SPEED_10G)
		hw->phy.speeds_supported |= TXGBE_LINK_SPEED_10GB_FULL;
	if (speed_ability & TXGBE_MD_PHY_SPEED_1G)
		hw->phy.speeds_supported |= TXGBE_LINK_SPEED_1GB_FULL;
	if (speed_ability & TXGBE_MD_PHY_SPEED_100M)
		hw->phy.speeds_supported |= TXGBE_LINK_SPEED_100M_FULL;

	return err;
}

/**
 *  txgbe_get_copper_link_capabilities - Determines link capabilities
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @autoneg: boolean auto-negotiation value
 **/
s32 txgbe_get_copper_link_capabilities(struct txgbe_hw *hw,
					       u32 *speed,
					       bool *autoneg)
{
	s32 err = 0;

	DEBUGFUNC("txgbe_get_copper_link_capabilities");

	*autoneg = true;
	if (!hw->phy.speeds_supported)
		err = txgbe_get_copper_speeds_supported(hw);

	*speed = hw->phy.speeds_supported;
	return err;
}

/**
 *  txgbe_check_phy_link_tnx - Determine link and speed status
 *  @hw: pointer to hardware structure
 *  @speed: current link speed
 *  @link_up: true is link is up, false otherwise
 *
 *  Reads the VS1 register to determine if link is up and the current speed for
 *  the PHY.
 **/
s32 txgbe_check_phy_link_tnx(struct txgbe_hw *hw, u32 *speed,
			     bool *link_up)
{
	s32 err = 0;
	u32 time_out;
	u32 max_time_out = 10;
	u16 phy_link = 0;
	u16 phy_speed = 0;
	u16 phy_data = 0;

	DEBUGFUNC("txgbe_check_phy_link_tnx");

	/* Initialize speed and link to default case */
	*link_up = false;
	*speed = TXGBE_LINK_SPEED_10GB_FULL;

	/*
	 * Check current speed and link status of the PHY register.
	 * This is a vendor specific register and may have to
	 * be changed for other copper PHYs.
	 */
	for (time_out = 0; time_out < max_time_out; time_out++) {
		usec_delay(10);
		err = hw->phy.read_reg(hw,
					TXGBE_MD_VENDOR_SPECIFIC_1_STATUS,
					TXGBE_MD_DEV_VENDOR_1,
					&phy_data);
		phy_link = phy_data & TXGBE_MD_VENDOR_SPECIFIC_1_LINK_STATUS;
		phy_speed = phy_data &
				 TXGBE_MD_VENDOR_SPECIFIC_1_SPEED_STATUS;
		if (phy_link == TXGBE_MD_VENDOR_SPECIFIC_1_LINK_STATUS) {
			*link_up = true;
			if (phy_speed ==
			    TXGBE_MD_VENDOR_SPECIFIC_1_SPEED_STATUS)
				*speed = TXGBE_LINK_SPEED_1GB_FULL;
			break;
		}
	}

	return err;
}

/**
 *  txgbe_setup_phy_link_tnx - Set and restart auto-neg
 *  @hw: pointer to hardware structure
 *
 *  Restart auto-negotiation and PHY and waits for completion.
 **/
s32 txgbe_setup_phy_link_tnx(struct txgbe_hw *hw)
{
	s32 err = 0;
	u16 autoneg_reg = TXGBE_MII_AUTONEG_REG;
	bool autoneg = false;
	u32 speed;

	DEBUGFUNC("txgbe_setup_phy_link_tnx");

	txgbe_get_copper_link_capabilities(hw, &speed, &autoneg);

	if (speed & TXGBE_LINK_SPEED_10GB_FULL) {
		/* Set or unset auto-negotiation 10G advertisement */
		hw->phy.read_reg(hw, TXGBE_MII_10GBASE_T_AUTONEG_CTRL_REG,
				     TXGBE_MD_DEV_AUTO_NEG,
				     &autoneg_reg);

		autoneg_reg &= ~TXGBE_MII_10GBASE_T_ADVERTISE;
		if (hw->phy.autoneg_advertised & TXGBE_LINK_SPEED_10GB_FULL)
			autoneg_reg |= TXGBE_MII_10GBASE_T_ADVERTISE;

		hw->phy.write_reg(hw, TXGBE_MII_10GBASE_T_AUTONEG_CTRL_REG,
				      TXGBE_MD_DEV_AUTO_NEG,
				      autoneg_reg);
	}

	if (speed & TXGBE_LINK_SPEED_1GB_FULL) {
		/* Set or unset auto-negotiation 1G advertisement */
		hw->phy.read_reg(hw, TXGBE_MII_AUTONEG_XNP_TX_REG,
				     TXGBE_MD_DEV_AUTO_NEG,
				     &autoneg_reg);

		autoneg_reg &= ~TXGBE_MII_1GBASE_T_ADVERTISE_XNP_TX;
		if (hw->phy.autoneg_advertised & TXGBE_LINK_SPEED_1GB_FULL)
			autoneg_reg |= TXGBE_MII_1GBASE_T_ADVERTISE_XNP_TX;

		hw->phy.write_reg(hw, TXGBE_MII_AUTONEG_XNP_TX_REG,
				      TXGBE_MD_DEV_AUTO_NEG,
				      autoneg_reg);
	}

	if (speed & TXGBE_LINK_SPEED_100M_FULL) {
		/* Set or unset auto-negotiation 100M advertisement */
		hw->phy.read_reg(hw, TXGBE_MII_AUTONEG_ADVERTISE_REG,
				     TXGBE_MD_DEV_AUTO_NEG,
				     &autoneg_reg);

		autoneg_reg &= ~TXGBE_MII_100BASE_T_ADVERTISE;
		if (hw->phy.autoneg_advertised & TXGBE_LINK_SPEED_100M_FULL)
			autoneg_reg |= TXGBE_MII_100BASE_T_ADVERTISE;

		hw->phy.write_reg(hw, TXGBE_MII_AUTONEG_ADVERTISE_REG,
				      TXGBE_MD_DEV_AUTO_NEG,
				      autoneg_reg);
	}

	/* Blocked by MNG FW so don't reset PHY */
	if (txgbe_check_reset_blocked(hw))
		return err;

	/* Restart PHY auto-negotiation. */
	hw->phy.read_reg(hw, TXGBE_MD_AUTO_NEG_CONTROL,
			     TXGBE_MD_DEV_AUTO_NEG, &autoneg_reg);

	autoneg_reg |= TXGBE_MII_RESTART;

	hw->phy.write_reg(hw, TXGBE_MD_AUTO_NEG_CONTROL,
			      TXGBE_MD_DEV_AUTO_NEG, autoneg_reg);

	return err;
}

/**
 *  txgbe_identify_module - Identifies module type
 *  @hw: pointer to hardware structure
 *
 *  Determines HW type and calls appropriate function.
 **/
s32 txgbe_identify_module(struct txgbe_hw *hw)
{
	s32 err = TXGBE_ERR_SFP_NOT_PRESENT;

	DEBUGFUNC("txgbe_identify_module");

	switch (hw->phy.media_type) {
	case txgbe_media_type_fiber:
		err = txgbe_identify_sfp_module(hw);
		break;

	case txgbe_media_type_fiber_qsfp:
		err = txgbe_identify_qsfp_module(hw);
		break;

	default:
		hw->phy.sfp_type = txgbe_sfp_type_not_present;
		err = TXGBE_ERR_SFP_NOT_PRESENT;
		break;
	}

	return err;
}

/**
 *  txgbe_identify_sfp_module - Identifies SFP modules
 *  @hw: pointer to hardware structure
 *
 *  Searches for and identifies the SFP module and assigns appropriate PHY type.
 **/
s32 txgbe_identify_sfp_module(struct txgbe_hw *hw)
{
	s32 err = TXGBE_ERR_PHY_ADDR_INVALID;
	u32 vendor_oui = 0;
	enum txgbe_sfp_type stored_sfp_type = hw->phy.sfp_type;
	u8 identifier = 0;
	u8 comp_codes_1g = 0;
	u8 comp_codes_10g = 0;
	u8 oui_bytes[3] = {0, 0, 0};
	u8 cable_tech = 0;
	u8 cable_spec = 0;
	u16 enforce_sfp = 0;

	DEBUGFUNC("txgbe_identify_sfp_module");

	if (hw->phy.media_type != txgbe_media_type_fiber) {
		hw->phy.sfp_type = txgbe_sfp_type_not_present;
		return TXGBE_ERR_SFP_NOT_PRESENT;
	}

	err = hw->phy.read_i2c_eeprom(hw, TXGBE_SFF_IDENTIFIER,
					     &identifier);
	if (err != 0) {
ERR_I2C:
		hw->phy.sfp_type = txgbe_sfp_type_not_present;
		if (hw->phy.type != txgbe_phy_nl) {
			hw->phy.id = 0;
			hw->phy.type = txgbe_phy_unknown;
		}
		return TXGBE_ERR_SFP_NOT_PRESENT;
	}

	if (identifier != TXGBE_SFF_IDENTIFIER_SFP) {
		hw->phy.type = txgbe_phy_sfp_unsupported;
		return TXGBE_ERR_SFP_NOT_SUPPORTED;
	}

	err = hw->phy.read_i2c_eeprom(hw, TXGBE_SFF_1GBE_COMP_CODES,
					     &comp_codes_1g);
	if (err != 0)
		goto ERR_I2C;

	err = hw->phy.read_i2c_eeprom(hw, TXGBE_SFF_10GBE_COMP_CODES,
					     &comp_codes_10g);
	if (err != 0)
		goto ERR_I2C;

	err = hw->phy.read_i2c_eeprom(hw, TXGBE_SFF_CABLE_TECHNOLOGY,
					     &cable_tech);
	if (err != 0)
		goto ERR_I2C;

	 /* ID Module
	  * =========
	  * 0   SFP_DA_CU
	  * 1   SFP_SR
	  * 2   SFP_LR
	  * 3   SFP_DA_CORE0 - chip-specific
	  * 4   SFP_DA_CORE1 - chip-specific
	  * 5   SFP_SR/LR_CORE0 - chip-specific
	  * 6   SFP_SR/LR_CORE1 - chip-specific
	  * 7   SFP_act_lmt_DA_CORE0 - chip-specific
	  * 8   SFP_act_lmt_DA_CORE1 - chip-specific
	  * 9   SFP_1g_cu_CORE0 - chip-specific
	  * 10  SFP_1g_cu_CORE1 - chip-specific
	  * 11  SFP_1g_sx_CORE0 - chip-specific
	  * 12  SFP_1g_sx_CORE1 - chip-specific
	  */
	if (cable_tech & TXGBE_SFF_CABLE_DA_PASSIVE) {
		if (hw->bus.lan_id == 0)
			hw->phy.sfp_type = txgbe_sfp_type_da_cu_core0;
		else
			hw->phy.sfp_type = txgbe_sfp_type_da_cu_core1;
	} else if (cable_tech & TXGBE_SFF_CABLE_DA_ACTIVE) {
		err = hw->phy.read_i2c_eeprom(hw,
			TXGBE_SFF_CABLE_SPEC_COMP, &cable_spec);
		if (err != 0)
			goto ERR_I2C;
		if (cable_spec & TXGBE_SFF_DA_SPEC_ACTIVE_LIMITING) {
			hw->phy.sfp_type = (hw->bus.lan_id == 0
				? txgbe_sfp_type_da_act_lmt_core0
				: txgbe_sfp_type_da_act_lmt_core1);
		} else {
			hw->phy.sfp_type = txgbe_sfp_type_unknown;
		}
	} else if (comp_codes_10g &
		   (TXGBE_SFF_10GBASESR_CAPABLE |
		    TXGBE_SFF_10GBASELR_CAPABLE)) {
		hw->phy.sfp_type = (hw->bus.lan_id == 0
				? txgbe_sfp_type_srlr_core0
				: txgbe_sfp_type_srlr_core1);
	} else if (comp_codes_1g & TXGBE_SFF_1GBASET_CAPABLE) {
		hw->phy.sfp_type = (hw->bus.lan_id == 0
				? txgbe_sfp_type_1g_cu_core0
				: txgbe_sfp_type_1g_cu_core1);
	} else if (comp_codes_1g & TXGBE_SFF_1GBASESX_CAPABLE) {
		hw->phy.sfp_type = (hw->bus.lan_id == 0
				? txgbe_sfp_type_1g_sx_core0
				: txgbe_sfp_type_1g_sx_core1);
	} else if (comp_codes_1g & TXGBE_SFF_1GBASELX_CAPABLE) {
		hw->phy.sfp_type = (hw->bus.lan_id == 0
				? txgbe_sfp_type_1g_lx_core0
				: txgbe_sfp_type_1g_lx_core1);
	} else {
		hw->phy.sfp_type = txgbe_sfp_type_unknown;
	}

	if (hw->phy.sfp_type != stored_sfp_type)
		hw->phy.sfp_setup_needed = true;

	/* Determine if the SFP+ PHY is dual speed or not. */
	hw->phy.multispeed_fiber = false;
	if (((comp_codes_1g & TXGBE_SFF_1GBASESX_CAPABLE) &&
	     (comp_codes_10g & TXGBE_SFF_10GBASESR_CAPABLE)) ||
	    ((comp_codes_1g & TXGBE_SFF_1GBASELX_CAPABLE) &&
	     (comp_codes_10g & TXGBE_SFF_10GBASELR_CAPABLE)))
		hw->phy.multispeed_fiber = true;

	/* Determine PHY vendor */
	if (hw->phy.type != txgbe_phy_nl) {
		hw->phy.id = identifier;
		err = hw->phy.read_i2c_eeprom(hw,
			TXGBE_SFF_VENDOR_OUI_BYTE0, &oui_bytes[0]);
		if (err != 0)
			goto ERR_I2C;

		err = hw->phy.read_i2c_eeprom(hw,
			TXGBE_SFF_VENDOR_OUI_BYTE1, &oui_bytes[1]);
		if (err != 0)
			goto ERR_I2C;

		err = hw->phy.read_i2c_eeprom(hw,
			TXGBE_SFF_VENDOR_OUI_BYTE2, &oui_bytes[2]);
		if (err != 0)
			goto ERR_I2C;

		vendor_oui = ((u32)oui_bytes[0] << 24) |
			     ((u32)oui_bytes[1] << 16) |
			     ((u32)oui_bytes[2] << 8);
		switch (vendor_oui) {
		case TXGBE_SFF_VENDOR_OUI_TYCO:
			if (cable_tech & TXGBE_SFF_CABLE_DA_PASSIVE)
				hw->phy.type = txgbe_phy_sfp_tyco_passive;
			break;
		case TXGBE_SFF_VENDOR_OUI_FTL:
			if (cable_tech & TXGBE_SFF_CABLE_DA_ACTIVE)
				hw->phy.type = txgbe_phy_sfp_ftl_active;
			else
				hw->phy.type = txgbe_phy_sfp_ftl;
			break;
		case TXGBE_SFF_VENDOR_OUI_AVAGO:
			hw->phy.type = txgbe_phy_sfp_avago;
			break;
		case TXGBE_SFF_VENDOR_OUI_INTEL:
			hw->phy.type = txgbe_phy_sfp_intel;
			break;
		default:
			if (cable_tech & TXGBE_SFF_CABLE_DA_PASSIVE)
				hw->phy.type = txgbe_phy_sfp_unknown_passive;
			else if (cable_tech & TXGBE_SFF_CABLE_DA_ACTIVE)
				hw->phy.type = txgbe_phy_sfp_unknown_active;
			else
				hw->phy.type = txgbe_phy_sfp_unknown;
			break;
		}
	}

	/* Allow any DA cable vendor */
	if (cable_tech & (TXGBE_SFF_CABLE_DA_PASSIVE |
			  TXGBE_SFF_CABLE_DA_ACTIVE)) {
		return 0;
	}

	/* Verify supported 1G SFP modules */
	if (comp_codes_10g == 0 &&
	    !(hw->phy.sfp_type == txgbe_sfp_type_1g_cu_core1 ||
	      hw->phy.sfp_type == txgbe_sfp_type_1g_cu_core0 ||
	      hw->phy.sfp_type == txgbe_sfp_type_1g_lx_core0 ||
	      hw->phy.sfp_type == txgbe_sfp_type_1g_lx_core1 ||
	      hw->phy.sfp_type == txgbe_sfp_type_1g_sx_core0 ||
	      hw->phy.sfp_type == txgbe_sfp_type_1g_sx_core1)) {
		hw->phy.type = txgbe_phy_sfp_unsupported;
		return TXGBE_ERR_SFP_NOT_SUPPORTED;
	}

	hw->mac.get_device_caps(hw, &enforce_sfp);
	if (!(enforce_sfp & TXGBE_DEVICE_CAPS_ALLOW_ANY_SFP) &&
	    !hw->allow_unsupported_sfp &&
	    !(hw->phy.sfp_type == txgbe_sfp_type_1g_cu_core0 ||
	      hw->phy.sfp_type == txgbe_sfp_type_1g_cu_core1 ||
	      hw->phy.sfp_type == txgbe_sfp_type_1g_lx_core0 ||
	      hw->phy.sfp_type == txgbe_sfp_type_1g_lx_core1 ||
	      hw->phy.sfp_type == txgbe_sfp_type_1g_sx_core0 ||
	      hw->phy.sfp_type == txgbe_sfp_type_1g_sx_core1)) {
		DEBUGOUT("SFP+ module not supported\n");
		hw->phy.type = txgbe_phy_sfp_unsupported;
		return TXGBE_ERR_SFP_NOT_SUPPORTED;
	}

	return err;
}

/**
 *  txgbe_identify_qsfp_module - Identifies QSFP modules
 *  @hw: pointer to hardware structure
 *
 *  Searches for and identifies the QSFP module and assigns appropriate PHY type
 **/
s32 txgbe_identify_qsfp_module(struct txgbe_hw *hw)
{
	s32 err = TXGBE_ERR_PHY_ADDR_INVALID;
	u32 vendor_oui = 0;
	enum txgbe_sfp_type stored_sfp_type = hw->phy.sfp_type;
	u8 identifier = 0;
	u8 comp_codes_1g = 0;
	u8 comp_codes_10g = 0;
	u8 oui_bytes[3] = {0, 0, 0};
	u16 enforce_sfp = 0;
	u8 connector = 0;
	u8 cable_length = 0;
	u8 device_tech = 0;
	bool active_cable = false;

	DEBUGFUNC("txgbe_identify_qsfp_module");

	if (hw->phy.media_type != txgbe_media_type_fiber_qsfp) {
		hw->phy.sfp_type = txgbe_sfp_type_not_present;
		err = TXGBE_ERR_SFP_NOT_PRESENT;
		goto out;
	}

	err = hw->phy.read_i2c_eeprom(hw, TXGBE_SFF_IDENTIFIER,
					     &identifier);
ERR_I2C:
	if (err != 0) {
		hw->phy.sfp_type = txgbe_sfp_type_not_present;
		hw->phy.id = 0;
		hw->phy.type = txgbe_phy_unknown;
		return TXGBE_ERR_SFP_NOT_PRESENT;
	}
	if (identifier != TXGBE_SFF_IDENTIFIER_QSFP_PLUS) {
		hw->phy.type = txgbe_phy_sfp_unsupported;
		err = TXGBE_ERR_SFP_NOT_SUPPORTED;
		goto out;
	}

	hw->phy.id = identifier;

	err = hw->phy.read_i2c_eeprom(hw, TXGBE_SFF_QSFP_10GBE_COMP,
					     &comp_codes_10g);

	if (err != 0)
		goto ERR_I2C;

	err = hw->phy.read_i2c_eeprom(hw, TXGBE_SFF_QSFP_1GBE_COMP,
					     &comp_codes_1g);

	if (err != 0)
		goto ERR_I2C;

	if (comp_codes_10g & TXGBE_SFF_QSFP_DA_PASSIVE_CABLE) {
		hw->phy.type = txgbe_phy_qsfp_unknown_passive;
		if (hw->bus.lan_id == 0)
			hw->phy.sfp_type = txgbe_sfp_type_da_cu_core0;
		else
			hw->phy.sfp_type = txgbe_sfp_type_da_cu_core1;
	} else if (comp_codes_10g & (TXGBE_SFF_10GBASESR_CAPABLE |
				     TXGBE_SFF_10GBASELR_CAPABLE)) {
		if (hw->bus.lan_id == 0)
			hw->phy.sfp_type = txgbe_sfp_type_srlr_core0;
		else
			hw->phy.sfp_type = txgbe_sfp_type_srlr_core1;
	} else {
		if (comp_codes_10g & TXGBE_SFF_QSFP_DA_ACTIVE_CABLE)
			active_cable = true;

		if (!active_cable) {
			hw->phy.read_i2c_eeprom(hw,
					TXGBE_SFF_QSFP_CONNECTOR,
					&connector);

			hw->phy.read_i2c_eeprom(hw,
					TXGBE_SFF_QSFP_CABLE_LENGTH,
					&cable_length);

			hw->phy.read_i2c_eeprom(hw,
					TXGBE_SFF_QSFP_DEVICE_TECH,
					&device_tech);

			if (connector ==
				     TXGBE_SFF_QSFP_CONNECTOR_NOT_SEPARABLE &&
			    cable_length > 0 &&
			    ((device_tech >> 4) ==
				     TXGBE_SFF_QSFP_TRANSMITTER_850NM_VCSEL))
				active_cable = true;
		}

		if (active_cable) {
			hw->phy.type = txgbe_phy_qsfp_unknown_active;
			if (hw->bus.lan_id == 0)
				hw->phy.sfp_type =
					txgbe_sfp_type_da_act_lmt_core0;
			else
				hw->phy.sfp_type =
					txgbe_sfp_type_da_act_lmt_core1;
		} else {
			/* unsupported module type */
			hw->phy.type = txgbe_phy_sfp_unsupported;
			err = TXGBE_ERR_SFP_NOT_SUPPORTED;
			goto out;
		}
	}

	if (hw->phy.sfp_type != stored_sfp_type)
		hw->phy.sfp_setup_needed = true;

	/* Determine if the QSFP+ PHY is dual speed or not. */
	hw->phy.multispeed_fiber = false;
	if (((comp_codes_1g & TXGBE_SFF_1GBASESX_CAPABLE) &&
	   (comp_codes_10g & TXGBE_SFF_10GBASESR_CAPABLE)) ||
	   ((comp_codes_1g & TXGBE_SFF_1GBASELX_CAPABLE) &&
	   (comp_codes_10g & TXGBE_SFF_10GBASELR_CAPABLE)))
		hw->phy.multispeed_fiber = true;

	/* Determine PHY vendor for optical modules */
	if (comp_codes_10g & (TXGBE_SFF_10GBASESR_CAPABLE |
			      TXGBE_SFF_10GBASELR_CAPABLE))  {
		err = hw->phy.read_i2c_eeprom(hw,
					    TXGBE_SFF_QSFP_VENDOR_OUI_BYTE0,
					    &oui_bytes[0]);

		if (err != 0)
			goto ERR_I2C;

		err = hw->phy.read_i2c_eeprom(hw,
					    TXGBE_SFF_QSFP_VENDOR_OUI_BYTE1,
					    &oui_bytes[1]);

		if (err != 0)
			goto ERR_I2C;

		err = hw->phy.read_i2c_eeprom(hw,
					    TXGBE_SFF_QSFP_VENDOR_OUI_BYTE2,
					    &oui_bytes[2]);

		if (err != 0)
			goto ERR_I2C;

		vendor_oui =
		  ((oui_bytes[0] << 24) |
		   (oui_bytes[1] << 16) |
		   (oui_bytes[2] << 8));

		if (vendor_oui == TXGBE_SFF_VENDOR_OUI_INTEL)
			hw->phy.type = txgbe_phy_qsfp_intel;
		else
			hw->phy.type = txgbe_phy_qsfp_unknown;

		hw->mac.get_device_caps(hw, &enforce_sfp);
		if (!(enforce_sfp & TXGBE_DEVICE_CAPS_ALLOW_ANY_SFP)) {
			/* Make sure we're a supported PHY type */
			if (hw->phy.type == txgbe_phy_qsfp_intel) {
				err = 0;
			} else {
				if (hw->allow_unsupported_sfp) {
					DEBUGOUT("WARNING: Wangxun (R) Network Connections are quality tested using Wangxun (R) Ethernet Optics. "
						"Using untested modules is not supported and may cause unstable operation or damage to the module or the adapter. "
						"Wangxun Corporation is not responsible for any harm caused by using untested modules.\n");
					err = 0;
				} else {
					DEBUGOUT("QSFP module not supported\n");
					hw->phy.type =
						txgbe_phy_sfp_unsupported;
					err = TXGBE_ERR_SFP_NOT_SUPPORTED;
				}
			}
		} else {
			err = 0;
		}
	}

out:
	return err;
}

/**
 *  txgbe_read_i2c_eeprom - Reads 8 bit EEPROM word over I2C interface
 *  @hw: pointer to hardware structure
 *  @byte_offset: EEPROM byte offset to read
 *  @eeprom_data: value read
 *
 *  Performs byte read operation to SFP module's EEPROM over I2C interface.
 **/
s32 txgbe_read_i2c_eeprom(struct txgbe_hw *hw, u8 byte_offset,
				  u8 *eeprom_data)
{
	DEBUGFUNC("txgbe_read_i2c_eeprom");

	return hw->phy.read_i2c_byte(hw, byte_offset,
					 TXGBE_I2C_EEPROM_DEV_ADDR,
					 eeprom_data);
}

/**
 *  txgbe_read_i2c_sff8472 - Reads 8 bit word over I2C interface
 *  @hw: pointer to hardware structure
 *  @byte_offset: byte offset at address 0xA2
 *  @sff8472_data: value read
 *
 *  Performs byte read operation to SFP module's SFF-8472 data over I2C
 **/
s32 txgbe_read_i2c_sff8472(struct txgbe_hw *hw, u8 byte_offset,
					  u8 *sff8472_data)
{
	return hw->phy.read_i2c_byte(hw, byte_offset,
					 TXGBE_I2C_EEPROM_DEV_ADDR2,
					 sff8472_data);
}

/**
 *  txgbe_write_i2c_eeprom - Writes 8 bit EEPROM word over I2C interface
 *  @hw: pointer to hardware structure
 *  @byte_offset: EEPROM byte offset to write
 *  @eeprom_data: value to write
 *
 *  Performs byte write operation to SFP module's EEPROM over I2C interface.
 **/
s32 txgbe_write_i2c_eeprom(struct txgbe_hw *hw, u8 byte_offset,
				   u8 eeprom_data)
{
	DEBUGFUNC("txgbe_write_i2c_eeprom");

	return hw->phy.write_i2c_byte(hw, byte_offset,
					  TXGBE_I2C_EEPROM_DEV_ADDR,
					  eeprom_data);
}

/**
 *  txgbe_read_i2c_byte_unlocked - Reads 8 bit word over I2C
 *  @hw: pointer to hardware structure
 *  @byte_offset: byte offset to read
 *  @dev_addr: address to read from
 *  @data: value read
 *
 *  Performs byte read operation to SFP module's EEPROM over I2C interface at
 *  a specified device address.
 **/
s32 txgbe_read_i2c_byte_unlocked(struct txgbe_hw *hw, u8 byte_offset,
					   u8 dev_addr, u8 *data)
{
	UNREFERENCED_PARAMETER(dev_addr);

	DEBUGFUNC("txgbe_read_i2c_byte");

	txgbe_i2c_start(hw);

	/* wait tx empty */
	if (!po32m(hw, TXGBE_I2CICR, TXGBE_I2CICR_TXEMPTY,
		TXGBE_I2CICR_TXEMPTY, NULL, 100, 100)) {
		return -TERR_TIMEOUT;
	}

	/* read data */
	wr32(hw, TXGBE_I2CDATA,
			byte_offset | TXGBE_I2CDATA_STOP);
	wr32(hw, TXGBE_I2CDATA, TXGBE_I2CDATA_READ);

	/* wait for read complete */
	if (!po32m(hw, TXGBE_I2CICR, TXGBE_I2CICR_RXFULL,
		TXGBE_I2CICR_RXFULL, NULL, 100, 100)) {
		return -TERR_TIMEOUT;
	}

	txgbe_i2c_stop(hw);

	*data = 0xFF & rd32(hw, TXGBE_I2CDATA);

	return 0;
}

/**
 *  txgbe_read_i2c_byte - Reads 8 bit word over I2C
 *  @hw: pointer to hardware structure
 *  @byte_offset: byte offset to read
 *  @dev_addr: address to read from
 *  @data: value read
 *
 *  Performs byte read operation to SFP module's EEPROM over I2C interface at
 *  a specified device address.
 **/
s32 txgbe_read_i2c_byte(struct txgbe_hw *hw, u8 byte_offset,
				u8 dev_addr, u8 *data)
{
	u32 swfw_mask = hw->phy.phy_semaphore_mask;
	int err = 0;

	if (hw->mac.acquire_swfw_sync(hw, swfw_mask))
		return TXGBE_ERR_SWFW_SYNC;
	err = txgbe_read_i2c_byte_unlocked(hw, byte_offset, dev_addr, data);
	hw->mac.release_swfw_sync(hw, swfw_mask);
	return err;
}

/**
 *  txgbe_write_i2c_byte_unlocked - Writes 8 bit word over I2C
 *  @hw: pointer to hardware structure
 *  @byte_offset: byte offset to write
 *  @dev_addr: address to write to
 *  @data: value to write
 *
 *  Performs byte write operation to SFP module's EEPROM over I2C interface at
 *  a specified device address.
 **/
s32 txgbe_write_i2c_byte_unlocked(struct txgbe_hw *hw, u8 byte_offset,
					    u8 dev_addr, u8 data)
{
	UNREFERENCED_PARAMETER(dev_addr);

	DEBUGFUNC("txgbe_write_i2c_byte");

	txgbe_i2c_start(hw);

	/* wait tx empty */
	if (!po32m(hw, TXGBE_I2CICR, TXGBE_I2CICR_TXEMPTY,
		TXGBE_I2CICR_TXEMPTY, NULL, 100, 100)) {
		return -TERR_TIMEOUT;
	}

	wr32(hw, TXGBE_I2CDATA, byte_offset | TXGBE_I2CDATA_STOP);
	wr32(hw, TXGBE_I2CDATA, data | TXGBE_I2CDATA_WRITE);

	/* wait for write complete */
	if (!po32m(hw, TXGBE_I2CICR, TXGBE_I2CICR_RXFULL,
		TXGBE_I2CICR_RXFULL, NULL, 100, 100)) {
		return -TERR_TIMEOUT;
	}
	txgbe_i2c_stop(hw);

	return 0;
}

/**
 *  txgbe_write_i2c_byte - Writes 8 bit word over I2C
 *  @hw: pointer to hardware structure
 *  @byte_offset: byte offset to write
 *  @dev_addr: address to write to
 *  @data: value to write
 *
 *  Performs byte write operation to SFP module's EEPROM over I2C interface at
 *  a specified device address.
 **/
s32 txgbe_write_i2c_byte(struct txgbe_hw *hw, u8 byte_offset,
				 u8 dev_addr, u8 data)
{
	u32 swfw_mask = hw->phy.phy_semaphore_mask;
	int err = 0;

	if (hw->mac.acquire_swfw_sync(hw, swfw_mask))
		return TXGBE_ERR_SWFW_SYNC;
	err = txgbe_write_i2c_byte_unlocked(hw, byte_offset, dev_addr, data);
	hw->mac.release_swfw_sync(hw, swfw_mask);

	return err;
}

/**
 *  txgbe_i2c_start - Sets I2C start condition
 *  @hw: pointer to hardware structure
 *
 *  Sets I2C start condition (High -> Low on SDA while SCL is High)
 **/
static void txgbe_i2c_start(struct txgbe_hw *hw)
{
	DEBUGFUNC("txgbe_i2c_start");

	wr32(hw, TXGBE_I2CENA, 0);

	wr32(hw, TXGBE_I2CCON,
		(TXGBE_I2CCON_MENA |
		TXGBE_I2CCON_SPEED(1) |
		TXGBE_I2CCON_RESTART |
		TXGBE_I2CCON_SDIA));
	wr32(hw, TXGBE_I2CTAR, TXGBE_I2C_SLAVEADDR);
	wr32(hw, TXGBE_I2CSSSCLHCNT, 600);
	wr32(hw, TXGBE_I2CSSSCLLCNT, 600);
	wr32(hw, TXGBE_I2CRXTL, 0); /* 1byte for rx full signal */
	wr32(hw, TXGBE_I2CTXTL, 4);
	wr32(hw, TXGBE_I2CSCLTMOUT, 0xFFFFFF);
	wr32(hw, TXGBE_I2CSDATMOUT, 0xFFFFFF);

	wr32(hw, TXGBE_I2CICM, 0);
	wr32(hw, TXGBE_I2CENA, 1);
}

/**
 *  txgbe_i2c_stop - Sets I2C stop condition
 *  @hw: pointer to hardware structure
 *
 *  Sets I2C stop condition (Low -> High on SDA while SCL is High)
 **/
static void txgbe_i2c_stop(struct txgbe_hw *hw)
{
	DEBUGFUNC("txgbe_i2c_stop");

	/* wait for completion */
	if (!po32m(hw, TXGBE_I2CSTAT, TXGBE_I2CSTAT_MST,
		0, NULL, 100, 100)) {
		DEBUGFUNC("i2c stop timeout.");
	}

	wr32(hw, TXGBE_I2CENA, 0);
}

static s32
txgbe_set_sgmii_an37_ability(struct txgbe_hw *hw)
{
	u32 value;

	wr32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_CTL1, 0x3002);
	wr32_epcs(hw, SR_MII_MMD_AN_CTL, 0x0105);
	wr32_epcs(hw, SR_MII_MMD_DIGI_CTL, 0x0200);
	value = rd32_epcs(hw, SR_MII_MMD_CTL);
	value = (value & ~0x1200) | (0x1 << 12) | (0x1 << 9);
	wr32_epcs(hw, SR_MII_MMD_CTL, value);
	return 0;
}

static s32
txgbe_set_link_to_kr(struct txgbe_hw *hw, bool autoneg)
{
	u32 i;
	s32 err = 0;

	/* 1. Wait xpcs power-up good */
	for (i = 0; i < 100; i++) {
		if ((rd32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_STATUS) &
			VR_XS_OR_PCS_MMD_DIGI_STATUS_PSEQ_MASK) ==
			VR_XS_OR_PCS_MMD_DIGI_STATUS_PSEQ_POWER_GOOD)
			break;
		msec_delay(10);
	}
	if (i == 100) {
		err = TXGBE_ERR_XPCS_POWER_UP_FAILED;
		goto out;
	}

	if (!autoneg) {
		/* 2. Disable xpcs AN-73 */
		wr32_epcs(hw, SR_AN_CTRL, 0x0);
		/* Disable PHY MPLLA for eth mode change(after ECO) */
		wr32_ephy(hw, 0x4, 0x243A);
		txgbe_flush(hw);
		msec_delay(1);
		/* Set the eth change_mode bit first in mis_rst register
		 * for corresponding LAN port
		 */
		wr32(hw, TXGBE_RST, TXGBE_RST_ETH(hw->bus.lan_id));

		/* 3. Set VR_XS_PMA_Gen5_12G_MPLLA_CTRL3 Register
		 * Bit[10:0](MPLLA_BANDWIDTH) = 11'd123 (default: 11'd16)
		 */
		wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL3,
			TXGBE_PHY_MPLLA_CTL3_MULTIPLIER_BW_10GBASER_KR);

		/* 4. Set VR_XS_PMA_Gen5_12G_MISC_CTRL0 Register
		 * Bit[12:8](RX_VREF_CTRL) = 5'hF (default: 5'h11)
		 */
		wr32_epcs(hw, TXGBE_PHY_MISC_CTL0, 0xCF00);

		/* 5. Set VR_XS_PMA_Gen5_12G_RX_EQ_CTRL0 Register
		 * Bit[15:8](VGA1/2_GAIN_0) = 8'h77
		 * Bit[7:5](CTLE_POLE_0) = 3'h2
		 * Bit[4:0](CTLE_BOOST_0) = 4'hA
		 */
		wr32_epcs(hw, TXGBE_PHY_RX_EQ_CTL0, 0x774A);

		/* 6. Set VR_MII_Gen5_12G_RX_GENCTRL3 Register
		 * Bit[2:0](LOS_TRSHLD_0) = 3'h4 (default: 3)
		 */
		wr32_epcs(hw, TXGBE_PHY_RX_GEN_CTL3, 0x0004);

		/* 7. Initialize the mode by setting VR XS or PCS MMD Digital
		 * Control1 Register Bit[15](VR_RST)
		 */
		wr32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_CTL1, 0xA000);

		/* Wait phy initialization done */
		for (i = 0; i < 100; i++) {
			if ((rd32_epcs(hw,
				VR_XS_OR_PCS_MMD_DIGI_CTL1) &
				VR_XS_OR_PCS_MMD_DIGI_CTL1_VR_RST) == 0)
				break;
			msleep(100);
		}
		if (i == 100) {
			err = TXGBE_ERR_PHY_INIT_NOT_DONE;
			goto out;
		}
	} else {
		wr32_epcs(hw, VR_AN_KR_MODE_CL, 0x1);
	}
out:
	return err;
}

static s32
txgbe_set_link_to_kx4(struct txgbe_hw *hw, bool autoneg)
{
	u32 i;
	s32 err = 0;
	u32 value;

	/* Check link status, if already set, skip setting it again */
	if (hw->link_status == TXGBE_LINK_STATUS_KX4)
		goto out;

	/* 1. Wait xpcs power-up good */
	for (i = 0; i < 100; i++) {
		if ((rd32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_STATUS) &
			VR_XS_OR_PCS_MMD_DIGI_STATUS_PSEQ_MASK) ==
			VR_XS_OR_PCS_MMD_DIGI_STATUS_PSEQ_POWER_GOOD)
			break;
		msec_delay(10);
	}
	if (i == 100) {
		err = TXGBE_ERR_XPCS_POWER_UP_FAILED;
		goto out;
	}

	wr32m(hw, TXGBE_MACTXCFG, TXGBE_MACTXCFG_TXE,
			~TXGBE_MACTXCFG_TXE);

	/* 2. Disable xpcs AN-73 */
	if (!autoneg)
		wr32_epcs(hw, SR_AN_CTRL, 0x0);
	else
		wr32_epcs(hw, SR_AN_CTRL, 0x3000);

	/* Disable PHY MPLLA for eth mode change(after ECO) */
	wr32_ephy(hw, 0x4, 0x250A);
	txgbe_flush(hw);
	msec_delay(1);

	/* Set the eth change_mode bit first in mis_rst register
	 * for corresponding LAN port
	 */
	wr32(hw, TXGBE_RST, TXGBE_RST_ETH(hw->bus.lan_id));

	/* Set SR PCS Control2 Register Bits[1:0] = 2'b01
	 * PCS_TYPE_SEL: non KR
	 */
	wr32_epcs(hw, SR_XS_PCS_CTRL2,
			SR_PCS_CTRL2_TYPE_SEL_X);

	/* Set SR PMA MMD Control1 Register Bit[13] = 1'b1
	 * SS13: 10G speed
	 */
	wr32_epcs(hw, SR_PMA_CTRL1,
			SR_PMA_CTRL1_SS13_KX4);

	value = (0xf5f0 & ~0x7F0) |  (0x5 << 8) | (0x7 << 5) | 0x10;
	wr32_epcs(hw, TXGBE_PHY_TX_GENCTRL1, value);

	wr32_epcs(hw, TXGBE_PHY_MISC_CTL0, 0x4F00);

	value = (0x1804 & ~0x3F3F);
	wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);

	value = (0x50 & ~0x7F) | 40 | (1 << 6);
	wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);

	for (i = 0; i < 4; i++) {
		if (i == 0)
			value = (0x45 & ~0xFFFF) | (0x7 << 12) |
				(0x7 << 8) | 0x6;
		else
			value = (0xff06 & ~0xFFFF) | (0x7 << 12) |
				(0x7 << 8) | 0x6;
		wr32_epcs(hw, TXGBE_PHY_RX_EQ_CTL0 + i, value);
	}

	value = 0x0 & ~0x7777;
	wr32_epcs(hw, TXGBE_PHY_RX_EQ_ATT_LVL0, value);

	wr32_epcs(hw, TXGBE_PHY_DFE_TAP_CTL0, 0x0);

	value = (0x6db & ~0xFFF) | (0x1 << 9) | (0x1 << 6) | (0x1 << 3) | 0x1;
	wr32_epcs(hw, TXGBE_PHY_RX_GEN_CTL3, value);

	/* Set VR XS, PMA, or MII Gen5 12G PHY MPLLA
	 * Control 0 Register Bit[7:0] = 8'd40  //MPLLA_MULTIPLIER
	 */
	wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL0,
			TXGBE_PHY_MPLLA_CTL0_MULTIPLIER_OTHER);

	/* Set VR XS, PMA or MII Gen5 12G PHY MPLLA
	 * Control 3 Register Bit[10:0] = 11'd86  //MPLLA_BANDWIDTH
	 */
	wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL3,
			TXGBE_PHY_MPLLA_CTL3_MULTIPLIER_BW_OTHER);

	/* Set VR XS, PMA, or MII Gen5 12G PHY VCO
	 * Calibration Load 0 Register  Bit[12:0] = 13'd1360  //VCO_LD_VAL_0
	 */
	wr32_epcs(hw, TXGBE_PHY_VCO_CAL_LD0,
			TXGBE_PHY_VCO_CAL_LD0_OTHER);

	/* Set VR XS, PMA, or MII Gen5 12G PHY VCO
	 * Calibration Load 1 Register  Bit[12:0] = 13'd1360  //VCO_LD_VAL_1
	 */
	wr32_epcs(hw, TXGBE_PHY_VCO_CAL_LD1,
			TXGBE_PHY_VCO_CAL_LD0_OTHER);

	/* Set VR XS, PMA, or MII Gen5 12G PHY VCO
	 * Calibration Load 2 Register  Bit[12:0] = 13'd1360  //VCO_LD_VAL_2
	 */
	wr32_epcs(hw, TXGBE_PHY_VCO_CAL_LD2,
			TXGBE_PHY_VCO_CAL_LD0_OTHER);
	/* Set VR XS, PMA, or MII Gen5 12G PHY VCO
	 * Calibration Load 3 Register  Bit[12:0] = 13'd1360  //VCO_LD_VAL_3
	 */
	wr32_epcs(hw, TXGBE_PHY_VCO_CAL_LD3,
			TXGBE_PHY_VCO_CAL_LD0_OTHER);
	/* Set VR XS, PMA, or MII Gen5 12G PHY VCO
	 * Calibration Reference 0 Register Bit[5:0] = 6'd34  //VCO_REF_LD_0/1
	 */
	wr32_epcs(hw, TXGBE_PHY_VCO_CAL_REF0, 0x2222);

	/* Set VR XS, PMA, or MII Gen5 12G PHY VCO
	 * Calibration Reference 1 Register Bit[5:0] = 6'd34  //VCO_REF_LD_2/3
	 */
	wr32_epcs(hw, TXGBE_PHY_VCO_CAL_REF1, 0x2222);

	/* Set VR XS, PMA, or MII Gen5 12G PHY AFE-DFE
	 * Enable Register Bit[7:0] = 8'd0  //AFE_EN_0/3_1, DFE_EN_0/3_1
	 */
	wr32_epcs(hw, TXGBE_PHY_AFE_DFE_ENABLE, 0x0);

	/* Set  VR XS, PMA, or MII Gen5 12G PHY Rx
	 * Equalization Control 4 Register Bit[3:0] = 4'd0  //CONT_ADAPT_0/3_1
	 */
	wr32_epcs(hw, TXGBE_PHY_RX_EQ_CTL, 0x00F0);

	/* Set VR XS, PMA, or MII Gen5 12G PHY Tx Rate
	 * Control Register Bit[14:12], Bit[10:8], Bit[6:4], Bit[2:0],
	 * all rates to 3'b010  //TX0/1/2/3_RATE
	 */
	wr32_epcs(hw, TXGBE_PHY_TX_RATE_CTL, 0x2222);

	/* Set VR XS, PMA, or MII Gen5 12G PHY Rx Rate
	 * Control Register Bit[13:12], Bit[9:8], Bit[5:4], Bit[1:0],
	 * all rates to 2'b10  //RX0/1/2/3_RATE
	 */
	wr32_epcs(hw, TXGBE_PHY_RX_RATE_CTL, 0x2222);

	/* Set VR XS, PMA, or MII Gen5 12G PHY Tx General
	 * Control 2 Register Bit[15:8] = 2'b01  //TX0/1/2/3_WIDTH: 10bits
	 */
	wr32_epcs(hw, TXGBE_PHY_TX_GEN_CTL2, 0x5500);

	/* Set VR XS, PMA, or MII Gen5 12G PHY Rx General
	 * Control 2 Register Bit[15:8] = 2'b01  //RX0/1/2/3_WIDTH: 10bits
	 */
	wr32_epcs(hw, TXGBE_PHY_RX_GEN_CTL2, 0x5500);

	/* Set VR XS, PMA, or MII Gen5 12G PHY MPLLA Control
	 * 2 Register Bit[10:8] = 3'b010
	 * MPLLA_DIV16P5_CLK_EN=0, MPLLA_DIV10_CLK_EN=1, MPLLA_DIV8_CLK_EN=0
	 */
	wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL2,
			TXGBE_PHY_MPLLA_CTL2_DIV_CLK_EN_10);

	wr32_epcs(hw, 0x1f0000, 0x0);
	wr32_epcs(hw, 0x1f8001, 0x0);
	wr32_epcs(hw, SR_MII_MMD_DIGI_CTL, 0x0);

	/* 10. Initialize the mode by setting VR XS or PCS MMD Digital Control1
	 * Register Bit[15](VR_RST)
	 */
	wr32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_CTL1, 0xA000);

	/* Wait phy initialization done */
	for (i = 0; i < 100; i++) {
		if ((rd32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_CTL1) &
			VR_XS_OR_PCS_MMD_DIGI_CTL1_VR_RST) == 0)
			break;
		msleep(100);
	}

	/* If success, set link status */
	hw->link_status = TXGBE_LINK_STATUS_KX4;

	if (i == 100) {
		err = TXGBE_ERR_PHY_INIT_NOT_DONE;
		goto out;
	}

out:
	return err;
}

static s32
txgbe_set_link_to_kx(struct txgbe_hw *hw,
			       u32 speed,
			       bool autoneg)
{
	u32 i;
	s32 err = 0;
	u32 wdata = 0;
	u32 value;

	/* Check link status, if already set, skip setting it again */
	if (hw->link_status == TXGBE_LINK_STATUS_KX)
		goto out;

	/* 1. Wait xpcs power-up good */
	for (i = 0; i < 100; i++) {
		if ((rd32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_STATUS) &
			VR_XS_OR_PCS_MMD_DIGI_STATUS_PSEQ_MASK) ==
			VR_XS_OR_PCS_MMD_DIGI_STATUS_PSEQ_POWER_GOOD)
			break;
		msec_delay(10);
	}
	if (i == 100) {
		err = TXGBE_ERR_XPCS_POWER_UP_FAILED;
		goto out;
	}

	wr32m(hw, TXGBE_MACTXCFG, TXGBE_MACTXCFG_TXE,
				~TXGBE_MACTXCFG_TXE);

	/* 2. Disable xpcs AN-73 */
	if (!autoneg)
		wr32_epcs(hw, SR_AN_CTRL, 0x0);
	else
		wr32_epcs(hw, SR_AN_CTRL, 0x3000);

	/* Disable PHY MPLLA for eth mode change(after ECO) */
	wr32_ephy(hw, 0x4, 0x240A);
	txgbe_flush(hw);
	msec_delay(1);

	/* Set the eth change_mode bit first in mis_rst register
	 * for corresponding LAN port
	 */
	wr32(hw, TXGBE_RST, TXGBE_RST_ETH(hw->bus.lan_id));

	/* Set SR PCS Control2 Register Bits[1:0] = 2'b01
	 * PCS_TYPE_SEL: non KR
	 */
	wr32_epcs(hw, SR_XS_PCS_CTRL2,
			SR_PCS_CTRL2_TYPE_SEL_X);

	/* Set SR PMA MMD Control1 Register Bit[13] = 1'b0
	 * SS13: 1G speed
	 */
	wr32_epcs(hw, SR_PMA_CTRL1,
			SR_PMA_CTRL1_SS13_KX);

	/* Set SR MII MMD Control Register to corresponding speed: {Bit[6],
	 * Bit[13]}=[2'b00,2'b01,2'b10]->[10M,100M,1G]
	 */
	if (speed == TXGBE_LINK_SPEED_100M_FULL)
		wdata = 0x2100;
	else if (speed == TXGBE_LINK_SPEED_1GB_FULL)
		wdata = 0x0140;
	else if (speed == TXGBE_LINK_SPEED_10M_FULL)
		wdata = 0x0100;
	wr32_epcs(hw, SR_MII_MMD_CTL,
			wdata);

	value = (0xf5f0 & ~0x710) |  (0x5 << 8);
	wr32_epcs(hw, TXGBE_PHY_TX_GENCTRL1, value);

	wr32_epcs(hw, TXGBE_PHY_MISC_CTL0, 0x4F00);

	value = (0x1804 & ~0x3F3F) | (24 << 8) | 4;
	wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);

	value = (0x50 & ~0x7F) | 16 | (1 << 6);
	wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);

	for (i = 0; i < 4; i++) {
		if (i) {
			value = 0xff06;
		} else {
			value = (0x45 & ~0xFFFF) | (0x7 << 12) |
				(0x7 << 8) | 0x6;
		}
		wr32_epcs(hw, TXGBE_PHY_RX_EQ_CTL0 + i, value);
	}

	value = 0x0 & ~0x7;
	wr32_epcs(hw, TXGBE_PHY_RX_EQ_ATT_LVL0, value);

	wr32_epcs(hw, TXGBE_PHY_DFE_TAP_CTL0, 0x0);

	value = (0x6db & ~0x7) | 0x4;
	wr32_epcs(hw, TXGBE_PHY_RX_GEN_CTL3, value);

	/* Set VR XS, PMA, or MII Gen5 12G PHY MPLLA Control
	 * 0 Register Bit[7:0] = 8'd32  //MPLLA_MULTIPLIER
	 */
	wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL0,
			TXGBE_PHY_MPLLA_CTL0_MULTIPLIER_1GBASEX_KX);

	/* Set VR XS, PMA or MII Gen5 12G PHY MPLLA Control
	 * 3 Register Bit[10:0] = 11'd70  //MPLLA_BANDWIDTH
	 */
	wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL3,
			TXGBE_PHY_MPLLA_CTL3_MULTIPLIER_BW_1GBASEX_KX);

	/* Set VR XS, PMA, or MII Gen5 12G PHY VCO
	 * Calibration Load 0 Register  Bit[12:0] = 13'd1344  //VCO_LD_VAL_0
	 */
	wr32_epcs(hw, TXGBE_PHY_VCO_CAL_LD0,
			TXGBE_PHY_VCO_CAL_LD0_1GBASEX_KX);

	wr32_epcs(hw, TXGBE_PHY_VCO_CAL_LD1, 0x549);
	wr32_epcs(hw, TXGBE_PHY_VCO_CAL_LD2, 0x549);
	wr32_epcs(hw, TXGBE_PHY_VCO_CAL_LD3, 0x549);

	/* Set VR XS, PMA, or MII Gen5 12G PHY VCO
	 * Calibration Reference 0 Register Bit[5:0] = 6'd42  //VCO_REF_LD_0
	 */
	wr32_epcs(hw, TXGBE_PHY_VCO_CAL_REF0,
			TXGBE_PHY_VCO_CAL_REF0_LD0_1GBASEX_KX);

	wr32_epcs(hw, TXGBE_PHY_VCO_CAL_REF1, 0x2929);

	/* Set VR XS, PMA, or MII Gen5 12G PHY AFE-DFE
	 * Enable Register Bit[4], Bit[0] = 1'b0  //AFE_EN_0, DFE_EN_0
	 */
	wr32_epcs(hw, TXGBE_PHY_AFE_DFE_ENABLE,
			0x0);
	/* Set VR XS, PMA, or MII Gen5 12G PHY Rx
	 * Equalization Control 4 Register Bit[0] = 1'b0  //CONT_ADAPT_0
	 */
	wr32_epcs(hw, TXGBE_PHY_RX_EQ_CTL,
			0x0010);
	/* Set VR XS, PMA, or MII Gen5 12G PHY Tx Rate
	 * Control Register Bit[2:0] = 3'b011  //TX0_RATE
	 */
	wr32_epcs(hw, TXGBE_PHY_TX_RATE_CTL,
			TXGBE_PHY_TX_RATE_CTL_TX0_RATE_1GBASEX_KX);

	/* Set VR XS, PMA, or MII Gen5 12G PHY Rx Rate
	 * Control Register Bit[2:0] = 3'b011 //RX0_RATE
	 */
	wr32_epcs(hw, TXGBE_PHY_RX_RATE_CTL,
			TXGBE_PHY_RX_RATE_CTL_RX0_RATE_1GBASEX_KX);

	/* Set VR XS, PMA, or MII Gen5 12G PHY Tx General
	 * Control 2 Register Bit[9:8] = 2'b01  //TX0_WIDTH: 10bits
	 */
	wr32_epcs(hw, TXGBE_PHY_TX_GEN_CTL2,
			TXGBE_PHY_TX_GEN_CTL2_TX0_WIDTH_OTHER);
	/* Set VR XS, PMA, or MII Gen5 12G PHY Rx General
	 * Control 2 Register Bit[9:8] = 2'b01  //RX0_WIDTH: 10bits
	 */
	wr32_epcs(hw, TXGBE_PHY_RX_GEN_CTL2,
			TXGBE_PHY_RX_GEN_CTL2_RX0_WIDTH_OTHER);
	/* Set VR XS, PMA, or MII Gen5 12G PHY MPLLA Control
	 * 2 Register Bit[10:8] = 3'b010   //MPLLA_DIV16P5_CLK_EN=0,
	 * MPLLA_DIV10_CLK_EN=1, MPLLA_DIV8_CLK_EN=0
	 */
	wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL2,
			TXGBE_PHY_MPLLA_CTL2_DIV_CLK_EN_10);

	/* VR MII MMD AN Control Register Bit[8] = 1'b1 //MII_CTRL
	 * Set to 8bit MII (required in 10M/100M SGMII)
	 */
	wr32_epcs(hw, SR_MII_MMD_AN_CTL,
			0x0100);

	/* 10. Initialize the mode by setting VR XS or PCS MMD Digital Control1
	 * Register Bit[15](VR_RST)
	 */
	wr32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_CTL1, 0xA000);

	/* Wait phy initialization done */
	for (i = 0; i < 100; i++) {
		if ((rd32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_CTL1) &
			VR_XS_OR_PCS_MMD_DIGI_CTL1_VR_RST) == 0)
			break;
		msleep(100);
	}

	/* If success, set link status */
	hw->link_status = TXGBE_LINK_STATUS_KX;

	if (i == 100) {
		err = TXGBE_ERR_PHY_INIT_NOT_DONE;
		goto out;
	}

out:
	return err;
}

static s32
txgbe_set_link_to_sfi(struct txgbe_hw *hw,
			       u32 speed)
{
	u32 i;
	s32 err = 0;
	u32 value = 0;

	/* Set the module link speed */
	hw->mac.set_rate_select_speed(hw, speed);
	/* 1. Wait xpcs power-up good */
	for (i = 0; i < 100; i++) {
		if ((rd32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_STATUS) &
			VR_XS_OR_PCS_MMD_DIGI_STATUS_PSEQ_MASK) ==
			VR_XS_OR_PCS_MMD_DIGI_STATUS_PSEQ_POWER_GOOD)
			break;
		msec_delay(10);
	}
	if (i == 100) {
		err = TXGBE_ERR_XPCS_POWER_UP_FAILED;
		goto out;
	}

	wr32m(hw, TXGBE_MACTXCFG, TXGBE_MACTXCFG_TXE,
			~TXGBE_MACTXCFG_TXE);

	/* 2. Disable xpcs AN-73 */
	wr32_epcs(hw, SR_AN_CTRL, 0x0);

	/* Disable PHY MPLLA for eth mode change(after ECO) */
	wr32_ephy(hw, 0x4, 0x243A);
	txgbe_flush(hw);
	msec_delay(1);
	/* Set the eth change_mode bit first in mis_rst register
	 * for corresponding LAN port
	 */
	wr32(hw, TXGBE_RST, TXGBE_RST_ETH(hw->bus.lan_id));

	if (speed == TXGBE_LINK_SPEED_10GB_FULL) {
		/* Set SR PCS Control2 Register Bits[1:0] = 2'b00
		 * PCS_TYPE_SEL: KR
		 */
		wr32_epcs(hw, SR_XS_PCS_CTRL2, 0);
		value = rd32_epcs(hw, SR_PMA_CTRL1);
		value = value | 0x2000;
		wr32_epcs(hw, SR_PMA_CTRL1, value);
		/* Set VR_XS_PMA_Gen5_12G_MPLLA_CTRL0 Register Bit[7:0] = 8'd33
		 * MPLLA_MULTIPLIER
		 */
		wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL0, 0x0021);
		/* 3. Set VR_XS_PMA_Gen5_12G_MPLLA_CTRL3 Register
		 * Bit[10:0](MPLLA_BANDWIDTH) = 11'd0
		 */
		wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL3, 0);
		value = rd32_epcs(hw, TXGBE_PHY_TX_GENCTRL1);
		value = (value & ~0x700) | 0x500;
		wr32_epcs(hw, TXGBE_PHY_TX_GENCTRL1, value);
		/* 4. Set VR_XS_PMA_Gen5_12G_MISC_CTRL0 Register
		 * Bit[12:8](RX_VREF_CTRL) = 5'hF
		 */
		wr32_epcs(hw, TXGBE_PHY_MISC_CTL0, 0xCF00);
		/* Set VR_XS_PMA_Gen5_12G_VCO_CAL_LD0 Register
		 * Bit[12:0] = 13'd1353  //VCO_LD_VAL_0
		 */
		wr32_epcs(hw, TXGBE_PHY_VCO_CAL_LD0, 0x0549);
		/* Set VR_XS_PMA_Gen5_12G_VCO_CAL_REF0 Register
		 * Bit[5:0] = 6'd41  //VCO_REF_LD_0
		 */
		wr32_epcs(hw, TXGBE_PHY_VCO_CAL_REF0, 0x0029);
		/* Set VR_XS_PMA_Gen5_12G_TX_RATE_CTRL Register
		 * Bit[2:0] = 3'b000  //TX0_RATE
		 */
		wr32_epcs(hw, TXGBE_PHY_TX_RATE_CTL, 0);
		/* Set VR_XS_PMA_Gen5_12G_RX_RATE_CTRL Register
		 * Bit[2:0] = 3'b000  //RX0_RATE
		 */
		wr32_epcs(hw, TXGBE_PHY_RX_RATE_CTL, 0);
		/* Set VR_XS_PMA_Gen5_12G_TX_GENCTRL2 Register Bit[9:8] = 2'b11
		 * TX0_WIDTH: 20bits
		 */
		wr32_epcs(hw, TXGBE_PHY_TX_GEN_CTL2, 0x0300);
		/* Set VR_XS_PMA_Gen5_12G_RX_GENCTRL2 Register Bit[9:8] = 2'b11
		 * RX0_WIDTH: 20bits
		 */
		wr32_epcs(hw, TXGBE_PHY_RX_GEN_CTL2, 0x0300);
		/* Set VR_XS_PMA_Gen5_12G_MPLLA_CTRL2 Register
		 * Bit[10:8] = 3'b110
		 * MPLLA_DIV16P5_CLK_EN=1
		 * MPLLA_DIV10_CLK_EN=1
		 * MPLLA_DIV8_CLK_EN=0
		 */
		wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL2, 0x0600);
		/* 5. Set VR_XS_PMA_Gen5_12G_TX_EQ_CTRL0 Register
		 * Bit[13:8](TX_EQ_MAIN) = 6'd30, Bit[5:0](TX_EQ_PRE) = 6'd4
		 */
		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0);
		value = (value & ~0x3F3F) | (24 << 8) | 4;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);
		/* 6. Set VR_XS_PMA_Gen5_12G_TX_EQ_CTRL1 Register
		 * Bit[6](TX_EQ_OVR_RIDE) = 1'b1, Bit[5:0](TX_EQ_POST) = 6'd36
		 */
		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1);
		value = (value & ~0x7F) | 16 | (1 << 6);
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
		if (hw->phy.sfp_type == txgbe_sfp_type_da_cu_core0 ||
			hw->phy.sfp_type == txgbe_sfp_type_da_cu_core1) {
			/* 7. Set VR_XS_PMA_Gen5_12G_RX_EQ_CTRL0 Register
			 * Bit[15:8](VGA1/2_GAIN_0) = 8'h77
			 * Bit[7:5](CTLE_POLE_0) = 3'h2
			 * Bit[4:0](CTLE_BOOST_0) = 4'hF
			 */
			wr32_epcs(hw, TXGBE_PHY_RX_EQ_CTL0, 0x774F);

		} else {
			/* 7. Set VR_XS_PMA_Gen5_12G_RX_EQ_CTRL0 Register
			 * Bit[15:8](VGA1/2_GAIN_0) = 8'h00
			 * Bit[7:5](CTLE_POLE_0) = 3'h2
			 * Bit[4:0](CTLE_BOOST_0) = 4'hA
			 */
			value = rd32_epcs(hw, TXGBE_PHY_RX_EQ_CTL0);
			value = (value & ~0xFFFF) | (2 << 5) | 0x05;
			wr32_epcs(hw, TXGBE_PHY_RX_EQ_CTL0, value);
		}
		value = rd32_epcs(hw, TXGBE_PHY_RX_EQ_ATT_LVL0);
		value = (value & ~0x7) | 0x0;
		wr32_epcs(hw, TXGBE_PHY_RX_EQ_ATT_LVL0, value);

		if (hw->phy.sfp_type == txgbe_sfp_type_da_cu_core0 ||
			hw->phy.sfp_type == txgbe_sfp_type_da_cu_core1) {
			/* 8. Set VR_XS_PMA_Gen5_12G_DFE_TAP_CTRL0 Register
			 * Bit[7:0](DFE_TAP1_0) = 8'd20
			 */
			wr32_epcs(hw, TXGBE_PHY_DFE_TAP_CTL0, 0x0014);
			value = rd32_epcs(hw, TXGBE_PHY_AFE_DFE_ENABLE);
			value = (value & ~0x11) | 0x11;
			wr32_epcs(hw, TXGBE_PHY_AFE_DFE_ENABLE, value);
		} else {
			/* 8. Set VR_XS_PMA_Gen5_12G_DFE_TAP_CTRL0 Register
			 * Bit[7:0](DFE_TAP1_0) = 8'd20
			 */
			wr32_epcs(hw, TXGBE_PHY_DFE_TAP_CTL0, 0xBE);
			/* 9. Set VR_MII_Gen5_12G_AFE_DFE_EN_CTRL Register
			 * Bit[4](DFE_EN_0) = 1'b0, Bit[0](AFE_EN_0) = 1'b0
			 */
			value = rd32_epcs(hw, TXGBE_PHY_AFE_DFE_ENABLE);
			value = (value & ~0x11) | 0x0;
			wr32_epcs(hw, TXGBE_PHY_AFE_DFE_ENABLE, value);
		}
		value = rd32_epcs(hw, TXGBE_PHY_RX_EQ_CTL);
		value = value & ~0x1;
		wr32_epcs(hw, TXGBE_PHY_RX_EQ_CTL, value);
	} else {
		/* Set SR PCS Control2 Register Bits[1:0] = 2'b00
		 * PCS_TYPE_SEL: KR
		 */
		wr32_epcs(hw, SR_XS_PCS_CTRL2, 0x1);
		/* Set SR PMA MMD Control1 Register Bit[13] = 1'b0
		 * SS13: 1G speed
		 */
		wr32_epcs(hw, SR_PMA_CTRL1, 0x0000);
		/* Set SR MII MMD Control Register to corresponding speed */
		wr32_epcs(hw, SR_MII_MMD_CTL, 0x0140);

		value = rd32_epcs(hw, TXGBE_PHY_TX_GENCTRL1);
		value = (value & ~0x710) | 0x500;
		wr32_epcs(hw, TXGBE_PHY_TX_GENCTRL1, value);
		/* 4. Set VR_XS_PMA_Gen5_12G_MISC_CTRL0 Register
		 * Bit[12:8](RX_VREF_CTRL) = 5'hF
		 */
		wr32_epcs(hw, TXGBE_PHY_MISC_CTL0, 0xCF00);
		/* 5. Set VR_XS_PMA_Gen5_12G_TX_EQ_CTRL0 Register
		 * Bit[13:8](TX_EQ_MAIN) = 6'd30, Bit[5:0](TX_EQ_PRE) = 6'd4
		 */
		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0);
		value = (value & ~0x3F3F) | (24 << 8) | 4;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);
		/* 6. Set VR_XS_PMA_Gen5_12G_TX_EQ_CTRL1 Register Bit[6]
		 * (TX_EQ_OVR_RIDE) = 1'b1, Bit[5:0](TX_EQ_POST) = 6'd36
		 */
		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1);
		value = (value & ~0x7F) | 16 | (1 << 6);
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
		if (hw->phy.sfp_type == txgbe_sfp_type_da_cu_core0 ||
			hw->phy.sfp_type == txgbe_sfp_type_da_cu_core1) {
			wr32_epcs(hw, TXGBE_PHY_RX_EQ_CTL0, 0x774F);
		} else {
			/* 7. Set VR_XS_PMA_Gen5_12G_RX_EQ_CTRL0 Register
			 * Bit[15:8](VGA1/2_GAIN_0) = 8'h00
			 * Bit[7:5](CTLE_POLE_0) = 3'h2
			 * Bit[4:0](CTLE_BOOST_0) = 4'hA
			 */
			value = rd32_epcs(hw, TXGBE_PHY_RX_EQ_CTL0);
			value = (value & ~0xFFFF) | 0x7706;
			wr32_epcs(hw, TXGBE_PHY_RX_EQ_CTL0, value);
		}
		value = rd32_epcs(hw, TXGBE_PHY_RX_EQ_ATT_LVL0);
		value = (value & ~0x7) | 0x0;
		wr32_epcs(hw, TXGBE_PHY_RX_EQ_ATT_LVL0, value);
		/* 8. Set VR_XS_PMA_Gen5_12G_DFE_TAP_CTRL0 Register
		 * Bit[7:0](DFE_TAP1_0) = 8'd00
		 */
		wr32_epcs(hw, TXGBE_PHY_DFE_TAP_CTL0, 0x0);
		/* 9. Set VR_MII_Gen5_12G_AFE_DFE_EN_CTRL Register
		 * Bit[4](DFE_EN_0) = 1'b0, Bit[0](AFE_EN_0) = 1'b0
		 */
		value = rd32_epcs(hw, TXGBE_PHY_RX_GEN_CTL3);
		value = (value & ~0x7) | 0x4;
		wr32_epcs(hw, TXGBE_PHY_RX_GEN_CTL3, value);
		wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL0, 0x0020);
		wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL3, 0x0046);
		wr32_epcs(hw, TXGBE_PHY_VCO_CAL_LD0, 0x0540);
		wr32_epcs(hw, TXGBE_PHY_VCO_CAL_REF0, 0x002A);
		wr32_epcs(hw, TXGBE_PHY_AFE_DFE_ENABLE, 0x0);
		wr32_epcs(hw, TXGBE_PHY_RX_EQ_CTL, 0x0010);
		wr32_epcs(hw, TXGBE_PHY_TX_RATE_CTL, 0x0003);
		wr32_epcs(hw, TXGBE_PHY_RX_RATE_CTL, 0x0003);
		wr32_epcs(hw, TXGBE_PHY_TX_GEN_CTL2, 0x0100);
		wr32_epcs(hw, TXGBE_PHY_RX_GEN_CTL2, 0x0100);
		wr32_epcs(hw, TXGBE_PHY_MPLLA_CTL2, 0x0200);
		wr32_epcs(hw, SR_MII_MMD_AN_CTL, 0x0100);
	}
	/* 10. Initialize the mode by setting VR XS or PCS MMD Digital Control1
	 * Register Bit[15](VR_RST)
	 */
	wr32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_CTL1, 0xA000);

	/* Wait phy initialization done */
	for (i = 0; i < 100; i++) {
		if ((rd32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_CTL1) &
			VR_XS_OR_PCS_MMD_DIGI_CTL1_VR_RST) == 0)
			break;
		msleep(100);
	}
	if (i == 100) {
		err = TXGBE_ERR_PHY_INIT_NOT_DONE;
		goto out;
	}

out:
	return err;
}

/**
 *  txgbe_autoc_read - Hides MAC differences needed for AUTOC read
 *  @hw: pointer to hardware structure
 */
u64 txgbe_autoc_read(struct txgbe_hw *hw)
{
	u64 autoc = 0;
	u32 sr_pcs_ctl;
	u32 sr_pma_ctl1;
	u32 sr_an_ctl;
	u32 sr_an_adv_reg2;

	if (hw->phy.multispeed_fiber) {
		autoc |= TXGBE_AUTOC_LMS_10G;
	} else if (hw->device_id == TXGBE_DEV_ID_RAPTOR_SFP ||
		   hw->device_id == TXGBE_DEV_ID_WX1820_SFP) {
		autoc |= TXGBE_AUTOC_LMS_10G |
			 TXGBE_AUTOC_10GS_SFI;
	} else if (hw->device_id == TXGBE_DEV_ID_RAPTOR_QSFP) {
		autoc = 0; /*TBD*/
	} else if (hw->device_id == TXGBE_DEV_ID_RAPTOR_XAUI) {
		autoc |= TXGBE_AUTOC_LMS_10G_LINK_NO_AN |
			 TXGBE_AUTOC_10G_XAUI;
		hw->phy.link_mode = TXGBE_PHYSICAL_LAYER_10GBASE_T;
	} else if (hw->device_id == TXGBE_DEV_ID_RAPTOR_SGMII) {
		autoc |= TXGBE_AUTOC_LMS_SGMII_1G_100M;
		hw->phy.link_mode = TXGBE_PHYSICAL_LAYER_1000BASE_T |
				TXGBE_PHYSICAL_LAYER_100BASE_TX;
	}

	if (hw->device_id != TXGBE_DEV_ID_RAPTOR_SGMII)
		return autoc;

	sr_pcs_ctl = rd32_epcs(hw, SR_XS_PCS_CTRL2);
	sr_pma_ctl1 = rd32_epcs(hw, SR_PMA_CTRL1);
	sr_an_ctl = rd32_epcs(hw, SR_AN_CTRL);
	sr_an_adv_reg2 = rd32_epcs(hw, SR_AN_MMD_ADV_REG2);

	if ((sr_pcs_ctl & SR_PCS_CTRL2_TYPE_SEL) == SR_PCS_CTRL2_TYPE_SEL_X &&
	    (sr_pma_ctl1 & SR_PMA_CTRL1_SS13) == SR_PMA_CTRL1_SS13_KX &&
	    (sr_an_ctl & SR_AN_CTRL_AN_EN) == 0) {
		/* 1G or KX - no backplane auto-negotiation */
		autoc |= TXGBE_AUTOC_LMS_1G_LINK_NO_AN |
			 TXGBE_AUTOC_1G_KX;
		hw->phy.link_mode = TXGBE_PHYSICAL_LAYER_1000BASE_KX;
	} else if ((sr_pcs_ctl & SR_PCS_CTRL2_TYPE_SEL) ==
		SR_PCS_CTRL2_TYPE_SEL_X &&
		(sr_pma_ctl1 & SR_PMA_CTRL1_SS13) == SR_PMA_CTRL1_SS13_KX4 &&
		(sr_an_ctl & SR_AN_CTRL_AN_EN) == 0) {
		autoc |= TXGBE_AUTOC_LMS_10G |
			 TXGBE_AUTOC_10G_KX4;
		hw->phy.link_mode = TXGBE_PHYSICAL_LAYER_10GBASE_KX4;
	} else if ((sr_pcs_ctl & SR_PCS_CTRL2_TYPE_SEL) ==
		SR_PCS_CTRL2_TYPE_SEL_R &&
		(sr_an_ctl & SR_AN_CTRL_AN_EN) == 0) {
		/* 10 GbE serial link (KR -no backplane auto-negotiation) */
		autoc |= TXGBE_AUTOC_LMS_10G |
			 TXGBE_AUTOC_10GS_KR;
		hw->phy.link_mode = TXGBE_PHYSICAL_LAYER_10GBASE_KR;
	} else if ((sr_an_ctl & SR_AN_CTRL_AN_EN)) {
		/* KX/KX4/KR backplane auto-negotiation enable */
		if (sr_an_adv_reg2 & SR_AN_MMD_ADV_REG2_BP_TYPE_KR)
			autoc |= TXGBE_AUTOC_10G_KR;
		if (sr_an_adv_reg2 & SR_AN_MMD_ADV_REG2_BP_TYPE_KX4)
			autoc |= TXGBE_AUTOC_10G_KX4;
		if (sr_an_adv_reg2 & SR_AN_MMD_ADV_REG2_BP_TYPE_KX)
			autoc |= TXGBE_AUTOC_1G_KX;
		autoc |= TXGBE_AUTOC_LMS_KX4_KX_KR;
		hw->phy.link_mode = TXGBE_PHYSICAL_LAYER_10GBASE_KR |
				TXGBE_PHYSICAL_LAYER_10GBASE_KX4 |
				TXGBE_PHYSICAL_LAYER_1000BASE_KX;
	}

	return autoc;
}

/**
 * txgbe_autoc_write - Hides MAC differences needed for AUTOC write
 * @hw: pointer to hardware structure
 * @autoc: value to write to AUTOC
 */
void txgbe_autoc_write(struct txgbe_hw *hw, u64 autoc)
{
	bool autoneg;
	u32 speed;
	u32 mactxcfg = 0;

	speed = TXGBE_AUTOC_SPEED(autoc);
	autoc &= ~TXGBE_AUTOC_SPEED_MASK;
	autoneg = (autoc & TXGBE_AUTOC_AUTONEG ? true : false);
	autoc &= ~TXGBE_AUTOC_AUTONEG;

	if (hw->device_id == TXGBE_DEV_ID_RAPTOR_KR_KX_KX4) {
		if (!autoneg) {
			switch (hw->phy.link_mode) {
			case TXGBE_PHYSICAL_LAYER_10GBASE_KR:
				txgbe_set_link_to_kr(hw, autoneg);
				break;
			case TXGBE_PHYSICAL_LAYER_10GBASE_KX4:
				txgbe_set_link_to_kx4(hw, autoneg);
				break;
			case TXGBE_PHYSICAL_LAYER_1000BASE_KX:
				txgbe_set_link_to_kx(hw, speed, autoneg);
				break;
			default:
				return;
			}
		}
	} else if (hw->device_id == TXGBE_DEV_ID_RAPTOR_XAUI ||
		   hw->device_id == TXGBE_DEV_ID_RAPTOR_SGMII) {
		if (speed == TXGBE_LINK_SPEED_10GB_FULL) {
			txgbe_set_link_to_kx4(hw, autoneg);
		} else {
			txgbe_set_link_to_kx(hw, speed, 0);
			txgbe_set_sgmii_an37_ability(hw);
		}
	} else if (hw->device_id == TXGBE_DEV_ID_RAPTOR_SFP ||
		   hw->device_id == TXGBE_DEV_ID_WX1820_SFP) {
		txgbe_set_link_to_sfi(hw, speed);
	}

	if (speed == TXGBE_LINK_SPEED_10GB_FULL)
		mactxcfg = TXGBE_MACTXCFG_SPEED_10G;
	else if (speed == TXGBE_LINK_SPEED_1GB_FULL)
		mactxcfg = TXGBE_MACTXCFG_SPEED_1G;

	/* enable mac transmitter */
	wr32m(hw, TXGBE_MACTXCFG, TXGBE_MACTXCFG_SPEED_MASK, mactxcfg);
}

