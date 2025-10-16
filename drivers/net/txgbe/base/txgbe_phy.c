/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "txgbe_hw.h"
#include "txgbe_eeprom.h"
#include "txgbe_mng.h"
#include "txgbe_phy.h"

static void txgbe_i2c_start(struct txgbe_hw *hw, u8 dev_addr);
static void txgbe_i2c_stop(struct txgbe_hw *hw);
static s32 txgbe_handle_bp_flow(u32 link_mode, struct txgbe_hw *hw);
static void txgbe_get_bp_ability(struct txgbe_backplane_ability *ability,
	u32 link_partner, struct txgbe_hw *hw);
static s32 txgbe_check_bp_ability(struct txgbe_backplane_ability *local_ability,
	struct txgbe_backplane_ability *lp_ability, struct txgbe_hw *hw);
static void txgbe_clear_bp_intr(u32 bit, u32 bit_high, struct txgbe_hw *hw);
static s32 txgbe_enable_kr_training(struct txgbe_hw *hw);
static s32 txgbe_disable_kr_training(struct txgbe_hw *hw, s32 post, s32 mode);
static s32 txgbe_check_kr_training(struct txgbe_hw *hw);
static void txgbe_read_phy_lane_tx_eq(u16 lane, struct txgbe_hw *hw,
				s32 post, s32 mode);
static s32 txgbe_set_link_to_sfi(struct txgbe_hw *hw, u32 speed);

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
		DEBUGOUT("Unable to validate PHY address 0x%04X",
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

	if (!hw->phy.phy_semaphore_mask)
		hw->phy.phy_semaphore_mask = TXGBE_MNGSEM_SWPHY;

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

	mmngc = rd32(hw, TXGBE_STAT);
	if (mmngc & TXGBE_STAT_MNGVETO) {
		DEBUGOUT("MNG_VETO bit detected.");
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

	hw->phy.addr = phy_addr;
	hw->phy.read_reg(hw, TXGBE_MD_PHY_ID_HIGH,
			     TXGBE_MD_DEV_PMA_PMD, &phy_id);

	if (phy_id != 0xFFFF && phy_id != 0x0)
		valid = true;

	DEBUGOUT("PHY ID HIGH is 0x%04X", phy_id);

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
	u32 i = 0;
	u32 status;

	if (hw->mac.type == txgbe_mac_aml) {
		hw->phy.addr = 0;

		for (i = 0; i < 32; i++) {
			hw->phy.addr = i;
			status = txgbe_read_phy_reg_mdi(hw, TXGBE_MD_PHY_ID_HIGH, 0, &phy_id_high);
			if (status) {
				DEBUGOUT("txgbe_read_phy_reg_mdi failed 1");
				return status;
			}
			DEBUGOUT("%d: phy_id_high 0x%x", i, phy_id_high);
			if ((phy_id_high & 0xFFFF) == 0x0141)
				break;
		}

		if (i == 32) {
			DEBUGOUT("txgbe_read_phy_reg_mdi failed");
			return TXGBE_ERR_PHY;
		}

		status = txgbe_read_phy_reg_mdi(hw, TXGBE_MD_PHY_ID_LOW, 0, &phy_id_low);
		if (status) {
			DEBUGOUT("txgbe_read_phy_reg_mdi failed 2");
			return status;
		}
		hw->phy.id = (u32)(phy_id_high & 0xFFFF) << 6;
		hw->phy.id |= (u32)((phy_id_low & 0xFC00) >> 10);

		DEBUGOUT("phy_id 0x%x", hw->phy.id);

		return status;
	}

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
	DEBUGOUT("PHY_ID_HIGH 0x%04X, PHY_ID_LOW 0x%04X",
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
		DEBUGOUT("PHY reset polling failed to complete.");
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
		DEBUGOUT("PHY address command did not complete");
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
		DEBUGOUT("PHY write cmd didn't complete");
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

s32 txgbe_get_phy_fw_version(struct txgbe_hw *hw, u32 *fw_version)
{
	u16 eeprom_verh, eeprom_verl;

	hw->rom.readw_sw(hw, TXGBE_EEPROM_VERSION_H, &eeprom_verh);
	hw->rom.readw_sw(hw, TXGBE_EEPROM_VERSION_L, &eeprom_verl);

	*fw_version = (eeprom_verh << 16) | eeprom_verl;

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
	u8 comp_codes_25g = 0;
	u8 comp_copper_len = 0;
	u8 oui_bytes[3] = {0, 0, 0};
	u8 cable_tech = 0;
	u8 cable_spec = 0;
	u16 enforce_sfp = 0;
	u32 value;

	if (hw->mac.type == txgbe_mac_aml) {
		value = rd32(hw, TXGBE_GPIOEXT);
		if (value & TXGBE_SFP1_MOD_ABS_LS) {
			hw->phy.sfp_type = txgbe_sfp_type_not_present;
			return TXGBE_ERR_SFP_NOT_PRESENT;
		}
	}

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

	err = hw->phy.read_i2c_eeprom(hw, TXGBE_SFF_25GBE_COMP_CODES,
					      &comp_codes_25g);
	if (err != 0)
		goto ERR_I2C;

	err = hw->phy.read_i2c_eeprom(hw, TXGBE_SFF_COPPER_LENGTH,
					      &comp_copper_len);
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
	if (cable_tech & TXGBE_SFF_CABLE_DA_ACTIVE) {
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
	} else if (comp_codes_25g == TXGBE_SFF_25GBASESR_CAPABLE ||
		   comp_codes_25g == TXGBE_SFF_25GBASEER_CAPABLE) {
		if (hw->bus.lan_id == 0)
			hw->phy.sfp_type = txgbe_sfp_type_25g_sr_core0;
		else
			hw->phy.sfp_type = txgbe_sfp_type_25g_sr_core1;
	} else if (comp_codes_25g == TXGBE_SFF_25GBASELR_CAPABLE) {
		if (hw->bus.lan_id == 0)
			hw->phy.sfp_type = txgbe_sfp_type_25g_lr_core0;
		else
			hw->phy.sfp_type = txgbe_sfp_type_25g_lr_core1;
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
	if (hw->mac.type == txgbe_mac_aml) {
		if ((comp_codes_25g == TXGBE_SFF_25GBASESR_CAPABLE ||
		     comp_codes_25g == TXGBE_SFF_25GBASELR_CAPABLE ||
		     comp_codes_25g == TXGBE_SFF_25GBASEER_CAPABLE) &&
		   ((comp_codes_10g & TXGBE_SFF_10GBASESR_CAPABLE) ||
		    (comp_codes_10g & TXGBE_SFF_10GBASELR_CAPABLE)))
			hw->phy.multispeed_fiber = true;
	} else {
		if (((comp_codes_1g & TXGBE_SFF_1GBASESX_CAPABLE) &&
		     (comp_codes_10g & TXGBE_SFF_10GBASESR_CAPABLE)) ||
		    ((comp_codes_1g & TXGBE_SFF_1GBASELX_CAPABLE) &&
		     (comp_codes_10g & TXGBE_SFF_10GBASELR_CAPABLE)))
			hw->phy.multispeed_fiber = true;
	}

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
	if (comp_codes_10g == 0 && comp_codes_25g == 0 &&
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
		DEBUGOUT("SFP+ module not supported");
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
	u32 value;

	if (hw->phy.media_type != txgbe_media_type_fiber_qsfp) {
		hw->phy.sfp_type = txgbe_sfp_type_not_present;
		err = TXGBE_ERR_SFP_NOT_PRESENT;
		goto out;
	}

	if (hw->mac.type == txgbe_mac_aml40) {
		/* config GPIO before read i2c */
		wr32(hw, TXGBE_GPIODATA, TXGBE_GPIOBIT_1);
		value = rd32(hw, TXGBE_GPIOEXT);
		if (value & TXGBE_SFP1_MOD_PRST_LS) {
			hw->phy.sfp_type = txgbe_sfp_type_not_present;
			return TXGBE_ERR_SFP_NOT_PRESENT;
		}
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
		if (hw->mac.type == txgbe_mac_aml40) {
			if (hw->bus.lan_id == 0)
				hw->phy.sfp_type = txgbe_qsfp_type_40g_cu_core0;
			else
				hw->phy.sfp_type = txgbe_qsfp_type_40g_cu_core1;
		} else {
			if (hw->bus.lan_id == 0)
				hw->phy.sfp_type = txgbe_sfp_type_da_cu_core0;
			else
				hw->phy.sfp_type = txgbe_sfp_type_da_cu_core1;
		}
	} else if (comp_codes_10g & TXGBE_SFF_40GBASE_SR4) {
		if (hw->bus.lan_id == 0)
			hw->phy.sfp_type = txgbe_qsfp_type_40g_sr_core0;
		else
			hw->phy.sfp_type = txgbe_qsfp_type_40g_sr_core1;
	} else if (comp_codes_10g & TXGBE_SFF_40GBASE_LR4) {
		if (hw->bus.lan_id == 0)
			hw->phy.sfp_type = txgbe_qsfp_type_40g_lr_core0;
		else
			hw->phy.sfp_type = txgbe_qsfp_type_40g_lr_core1;
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
						"Wangxun Corporation is not responsible for any harm caused by using untested modules.");
					err = 0;
				} else {
					DEBUGOUT("QSFP module not supported");
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
	txgbe_i2c_start(hw, dev_addr);

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
	txgbe_i2c_start(hw, dev_addr);

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
static void txgbe_i2c_start(struct txgbe_hw *hw, u8 dev_addr)
{
	wr32(hw, TXGBE_I2CENA, 0);

	wr32(hw, TXGBE_I2CCON,
		(TXGBE_I2CCON_MENA |
		TXGBE_I2CCON_SPEED(1) |
		TXGBE_I2CCON_RESTART |
		TXGBE_I2CCON_SDIA));
	wr32(hw, TXGBE_I2CTAR, dev_addr >> 1);
	wr32(hw, TXGBE_I2CSSSCLHCNT, 200);
	wr32(hw, TXGBE_I2CSSSCLLCNT, 200);
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
	/* wait for completion */
	if (!po32m(hw, TXGBE_I2CSTAT, TXGBE_I2CSTAT_MST,
		0, NULL, 100, 100)) {
		DEBUGOUT("i2c stop timeout.");
	}

	wr32(hw, TXGBE_I2CENA, 0);
}

/**
 *  txgbe_check_overtemp - Checks if an overtemp occurred.
 *  @hw: pointer to hardware structure
 *
 *  Checks if the temp alarm status was triggered due to overtemp
 **/
s32 txgbe_check_overtemp(struct txgbe_hw *hw)
{
	s32 status = 0;
	u32 ts_state;

	/* Check that the temp alarm status was triggered */
	ts_state = rd32(hw, TXGBE_TS_ALARM_ST);

	if (ts_state & TXGBE_TS_ALARM_ST_DALARM)
		status = TXGBE_ERR_UNDERTEMP;
	else if (ts_state & TXGBE_TS_ALARM_ST_ALARM)
		status = TXGBE_ERR_OVERTEMP;

	return status;
}

static void
txgbe_set_sgmii_an37_ability(struct txgbe_hw *hw)
{
	u32 value;
	u8 device_type = hw->subsystem_device_id & 0xF0;

	wr32_epcs(hw, VR_XS_OR_PCS_MMD_DIGI_CTL1, 0x3002);
	/* for sgmii + external phy, set to 0x0105 (phy sgmii mode) */
	/* for sgmii direct link, set to 0x010c (mac sgmii mode) */
	if (device_type == TXGBE_DEV_ID_MAC_SGMII ||
			hw->phy.media_type == txgbe_media_type_fiber)
		wr32_epcs(hw, SR_MII_MMD_AN_CTL, 0x010C);
	else if (device_type == TXGBE_DEV_ID_SGMII ||
			device_type == TXGBE_DEV_ID_XAUI)
		wr32_epcs(hw, SR_MII_MMD_AN_CTL, 0x0105);
	wr32_epcs(hw, SR_MII_MMD_DIGI_CTL, 0x0200);
	value = rd32_epcs(hw, SR_MII_MMD_CTL);
	value = (value & ~0x1200) | (0x1 << 9);
	if (hw->autoneg)
		value |= SR_MII_MMD_CTL_AN_EN;
	wr32_epcs(hw, SR_MII_MMD_CTL, value);
}

static s32
txgbe_set_link_to_kr(struct txgbe_hw *hw, bool autoneg)
{
	u32 i;
	u16 value;
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
	BP_LOG("It is set to kr.\n");

	wr32_epcs(hw, VR_AN_INTR_MSK, 0x7);
	wr32_epcs(hw, TXGBE_PHY_TX_POWER_ST_CTL, 0x00FC);
	wr32_epcs(hw, TXGBE_PHY_RX_POWER_ST_CTL, 0x00FC);

	if (!autoneg) {
		/* 2. Disable xpcs AN-73 */
		wr32_epcs(hw, SR_AN_CTRL,
			SR_AN_CTRL_AN_EN | SR_AN_CTRL_EXT_NP);

		wr32_epcs(hw, VR_AN_KR_MODE_CL, VR_AN_KR_MODE_CL_PDET);

		if (!(hw->devarg.auto_neg == 1)) {
			wr32_epcs(hw, SR_AN_CTRL, 0);
			wr32_epcs(hw, VR_AN_KR_MODE_CL, 0);
		} else {
			value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1);
			value &= ~(1 << 6);
			wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
		}
		if (hw->devarg.present == 1) {
			value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1);
			value |= TXGBE_PHY_TX_EQ_CTL1_DEF;
			wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
		}
		if (hw->devarg.poll == 1) {
			wr32_epcs(hw, VR_PMA_KRTR_TIMER_CTRL0,
				VR_PMA_KRTR_TIMER_MAX_WAIT);
			wr32_epcs(hw, VR_PMA_KRTR_TIMER_CTRL2, 0xA697);
		}

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

	if (hw->phy.ffe_set == TXGBE_BP_M_KR) {
		value = (0x1804 & ~0x3F3F);
		value |= hw->phy.ffe_main << 8 | hw->phy.ffe_pre;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);

		value = (0x50 & ~0x7F) | (1 << 6) | hw->phy.ffe_post;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
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

	BP_LOG("It is set to kx4.\n");
	wr32_epcs(hw, TXGBE_PHY_TX_POWER_ST_CTL, 0);
	wr32_epcs(hw, TXGBE_PHY_RX_POWER_ST_CTL, 0);

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

	wr32m(hw, TXGBE_MACTXCFG, TXGBE_MACTXCFG_TXE, ~TXGBE_MACTXCFG_TXE);
	wr32m(hw, TXGBE_MACRXCFG, TXGBE_MACRXCFG_ENA, ~TXGBE_MACRXCFG_ENA);
	hw->mac.disable_sec_tx_path(hw);

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

	value = (0xf5f0 & ~0x7F0) |  (0x5 << 8) | (0x7 << 5) | 0xF0;
	wr32_epcs(hw, TXGBE_PHY_TX_GENCTRL1, value);

	if ((hw->subsystem_device_id & 0xFF) == TXGBE_DEV_ID_MAC_XAUI)
		wr32_epcs(hw, TXGBE_PHY_MISC_CTL0, 0xCF00);
	else
		wr32_epcs(hw, TXGBE_PHY_MISC_CTL0, 0x4F00);

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

	if (hw->phy.ffe_set == TXGBE_BP_M_KX4) {
		value = (0x1804 & ~0x3F3F);
		value |= hw->phy.ffe_main << 8 | hw->phy.ffe_pre;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);

		value = (0x50 & ~0x7F) | (1 << 6) | hw->phy.ffe_post;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
	} else if (hw->fw_version <= TXGBE_FW_N_TXEQ) {
		value = (0x1804 & ~0x3F3F);
		value |= 40 << 8;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);

		value = (0x50 & ~0x7F) | (1 << 6);
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
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

	BP_LOG("It is set to kx. speed =0x%x\n", speed);
	wr32_epcs(hw, TXGBE_PHY_TX_POWER_ST_CTL, 0x00FC);
	wr32_epcs(hw, TXGBE_PHY_RX_POWER_ST_CTL, 0x00FC);

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

	wr32m(hw, TXGBE_MACTXCFG, TXGBE_MACTXCFG_TXE, ~TXGBE_MACTXCFG_TXE);
	wr32m(hw, TXGBE_MACRXCFG, TXGBE_MACRXCFG_ENA, ~TXGBE_MACRXCFG_ENA);
	hw->mac.disable_sec_tx_path(hw);

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

	value = (0xf5f0 & ~0x710) | (0x5 << 8) | 0x10;
	wr32_epcs(hw, TXGBE_PHY_TX_GENCTRL1, value);

	if (hw->devarg.sgmii == 1)
		wr32_epcs(hw, TXGBE_PHY_MISC_CTL0, 0x4F00);
	else
		wr32_epcs(hw, TXGBE_PHY_MISC_CTL0, 0xCF00);

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

	if (hw->phy.ffe_set == TXGBE_BP_M_KX) {
		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0) & ~0x3F3F;
		value |= hw->phy.ffe_main << 8 | hw->phy.ffe_pre;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);

		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0) & ~0x7F;
		value |= hw->phy.ffe_post | (1 << 6);
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
	} else if (hw->fw_version <= TXGBE_FW_N_TXEQ) {
		value = (0x1804 & ~0x3F3F) | (40 << 8);
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);

		value = (0x50 & ~0x7F) | (1 << 6);
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
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

	wr32m(hw, TXGBE_MACTXCFG, TXGBE_MACTXCFG_TXE, ~TXGBE_MACTXCFG_TXE);
	wr32m(hw, TXGBE_MACRXCFG, TXGBE_MACRXCFG_ENA, ~TXGBE_MACRXCFG_ENA);
	hw->mac.disable_sec_tx_path(hw);

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

	if (hw->phy.ffe_set == TXGBE_BP_M_SFI) {
		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0) & ~0x3F3F;
		value |= hw->phy.ffe_main << 8 | hw->phy.ffe_pre;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);

		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0) & ~0x7F;
		value |= hw->phy.ffe_post | (1 << 6);
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
	} else if (hw->fw_version <= TXGBE_FW_N_TXEQ) {
		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0);
		value = (value & ~0x3F3F) | (24 << 8) | 4;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);

		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1);
		value = (value & ~0x7F) | 16 | (1 << 6);
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
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
	u64 autoc;
	u32 sr_pcs_ctl;
	u32 sr_pma_ctl1;
	u32 sr_an_ctl;
	u32 sr_an_adv_reg2;
	u8 type = hw->subsystem_device_id & 0xFF;

	autoc = hw->mac.autoc;

	if (hw->phy.multispeed_fiber) {
		autoc |= TXGBE_AUTOC_LMS_10G;
	} else if (type == TXGBE_DEV_ID_SFP) {
		autoc |= TXGBE_AUTOC_LMS_10G;
		autoc |= TXGBE_AUTOC_10GS_SFI;
	} else if (type == TXGBE_DEV_ID_QSFP) {
		autoc = 0; /*TBD*/
	} else if (type == TXGBE_DEV_ID_XAUI || type == TXGBE_DEV_ID_SFI_XAUI) {
		autoc |= TXGBE_AUTOC_LMS_10G_LINK_NO_AN;
		autoc |= TXGBE_AUTOC_10G_XAUI;
		hw->phy.link_mode = TXGBE_PHYSICAL_LAYER_10GBASE_T;
	} else if (type == TXGBE_DEV_ID_SGMII) {
		autoc |= TXGBE_AUTOC_LMS_SGMII_1G_100M;
		hw->phy.link_mode = TXGBE_PHYSICAL_LAYER_1000BASE_T |
				TXGBE_PHYSICAL_LAYER_100BASE_TX;
	} else if (type == TXGBE_DEV_ID_MAC_XAUI) {
		autoc |= TXGBE_AUTOC_LMS_10G_LINK_NO_AN;
		hw->phy.link_mode = TXGBE_PHYSICAL_LAYER_10GBASE_KX4;
	} else if (type == TXGBE_DEV_ID_MAC_SGMII) {
		autoc |= TXGBE_AUTOC_LMS_1G_LINK_NO_AN;
		hw->phy.link_mode = TXGBE_PHYSICAL_LAYER_1000BASE_KX;
	}

	if (type != TXGBE_DEV_ID_KR_KX_KX4)
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
			autoc |= TXGBE_AUTOC_KR_SUPP;
		if (sr_an_adv_reg2 & SR_AN_MMD_ADV_REG2_BP_TYPE_KX4)
			autoc |= TXGBE_AUTOC_KX4_SUPP;
		if (sr_an_adv_reg2 & SR_AN_MMD_ADV_REG2_BP_TYPE_KX)
			autoc |= TXGBE_AUTOC_KX_SUPP;
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
	u8 device_type = hw->subsystem_device_id & 0xFF;

	speed = TXGBD_AUTOC_SPEED(autoc);
	autoc &= ~TXGBE_AUTOC_SPEED_MASK;
	autoneg = (autoc & TXGBE_AUTOC_AUTONEG ? true : false);
	autoc &= ~TXGBE_AUTOC_AUTONEG;

	if (device_type == TXGBE_DEV_ID_KR_KX_KX4) {
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
		} else {
			txgbe_set_link_to_kr(hw, !autoneg);
		}
	} else if (device_type == TXGBE_DEV_ID_XAUI ||
		   device_type == TXGBE_DEV_ID_SGMII ||
		   device_type == TXGBE_DEV_ID_MAC_XAUI ||
		   device_type == TXGBE_DEV_ID_MAC_SGMII ||
		   (device_type == TXGBE_DEV_ID_SFI_XAUI &&
		   hw->phy.media_type == txgbe_media_type_copper)) {
		if (speed == TXGBE_LINK_SPEED_10GB_FULL) {
			txgbe_set_link_to_kx4(hw, 0);
		} else {
			txgbe_set_link_to_kx(hw, speed, 0);
			if (hw->devarg.auto_neg == 1)
				txgbe_set_sgmii_an37_ability(hw);
		}
	} else if (hw->phy.media_type == txgbe_media_type_fiber) {
		txgbe_set_link_to_sfi(hw, speed);
		if (speed == TXGBE_LINK_SPEED_1GB_FULL)
			txgbe_set_sgmii_an37_ability(hw);
	}

	hw->mac.enable_sec_tx_path(hw);

	if (speed == TXGBE_LINK_SPEED_10GB_FULL)
		mactxcfg = TXGBE_MACTXCFG_SPEED_10G;
	else if (speed == TXGBE_LINK_SPEED_1GB_FULL)
		mactxcfg = TXGBE_MACTXCFG_SPEED_1G;

	/* enable mac transmitter */
	wr32m(hw, TXGBE_MACTXCFG,
		TXGBE_MACTXCFG_SPEED_MASK | TXGBE_MACTXCFG_TXE,
		mactxcfg | TXGBE_MACTXCFG_TXE);
	wr32m(hw, TXGBE_MACRXCFG, TXGBE_MACRXCFG_ENA, TXGBE_MACRXCFG_ENA);
}

void txgbe_bp_down_event(struct txgbe_hw *hw)
{
	if (!(hw->devarg.auto_neg == 1))
		return;

	BP_LOG("restart phy power.\n");
	wr32_epcs(hw, VR_AN_KR_MODE_CL, 0);
	wr32_epcs(hw, SR_AN_CTRL, 0);
	wr32_epcs(hw, VR_AN_INTR_MSK, 0);

	msleep(1050);
	txgbe_set_link_to_kr(hw, 0);
}

void txgbe_bp_mode_set(struct txgbe_hw *hw)
{
	if (hw->phy.ffe_set == TXGBE_BP_M_SFI)
		hw->subsystem_device_id = TXGBE_DEV_ID_WX1820_SFP;
	else if (hw->phy.ffe_set == TXGBE_BP_M_KR)
		hw->subsystem_device_id = TXGBE_DEV_ID_WX1820_KR_KX_KX4;
	else if (hw->phy.ffe_set == TXGBE_BP_M_KX4)
		hw->subsystem_device_id = TXGBE_DEV_ID_WX1820_MAC_XAUI;
	else if (hw->phy.ffe_set == TXGBE_BP_M_KX)
		hw->subsystem_device_id = TXGBE_DEV_ID_WX1820_MAC_SGMII;
}

void txgbe_set_phy_temp(struct txgbe_hw *hw)
{
	u32 value;

	if (hw->phy.ffe_set == TXGBE_BP_M_SFI) {
		BP_LOG("Set SFI TX_EQ MAIN:%d PRE:%d POST:%d\n",
			hw->phy.ffe_main, hw->phy.ffe_pre, hw->phy.ffe_post);

		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0);
		value = (value & ~0x3F3F) | (hw->phy.ffe_main << 8) |
			hw->phy.ffe_pre;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);

		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1);
		value = (value & ~0x7F) | hw->phy.ffe_post | (1 << 6);
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
	}

	if (hw->phy.ffe_set == TXGBE_BP_M_KR) {
		BP_LOG("Set KR TX_EQ MAIN:%d PRE:%d POST:%d\n",
			hw->phy.ffe_main, hw->phy.ffe_pre, hw->phy.ffe_post);
		value = (0x1804 & ~0x3F3F);
		value |= hw->phy.ffe_main << 8 | hw->phy.ffe_pre;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);

		value = (0x50 & ~0x7F) | (1 << 6) | hw->phy.ffe_post;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
		wr32_epcs(hw, 0x18035, 0x00FF);
		wr32_epcs(hw, 0x18055, 0x00FF);
	}

	if (hw->phy.ffe_set == TXGBE_BP_M_KX) {
		BP_LOG("Set KX TX_EQ MAIN:%d PRE:%d POST:%d\n",
			hw->phy.ffe_main, hw->phy.ffe_pre, hw->phy.ffe_post);
		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0);
		value = (value & ~0x3F3F) | (hw->phy.ffe_main << 8) |
			hw->phy.ffe_pre;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL0, value);

		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1);
		value = (value & ~0x7F) | hw->phy.ffe_post | (1 << 6);
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);

		wr32_epcs(hw, 0x18035, 0x00FF);
		wr32_epcs(hw, 0x18055, 0x00FF);
	}
}

/**
 * txgbe_kr_handle - Handle the interrupt of auto-negotiation
 * @hw: pointer to hardware structure
 */
s32 txgbe_kr_handle(struct txgbe_hw *hw)
{
	u32 value;
	s32 status = 0;

	value = rd32_epcs(hw, VR_AN_INTR);
	BP_LOG("AN INTERRUPT!! value: 0x%x\n", value);
	if (!(value & VR_AN_INTR_PG_RCV)) {
		wr32_epcs(hw, VR_AN_INTR, 0);
		return status;
	}

	status = txgbe_handle_bp_flow(0, hw);

	return status;
}

/**
 * txgbe_handle_bp_flow - Handle backplane AN73 flow
 * @hw: pointer to hardware structure
 * @link_mode: local AN73 link mode
 */
static s32 txgbe_handle_bp_flow(u32 link_mode, struct txgbe_hw *hw)
{
	u32 value, i, lp_reg, ld_reg;
	s32 status = 0;
	struct txgbe_backplane_ability local_ability, lp_ability;

	local_ability.current_link_mode = link_mode;

	/* 1. Get the local AN73 Base Page Ability */
	BP_LOG("<1>. Get the local AN73 Base Page Ability ...\n");
	txgbe_get_bp_ability(&local_ability, 0, hw);

	/* 2. Check and clear the AN73 Interrupt Status */
	BP_LOG("<2>. Check the AN73 Interrupt Status ...\n");
	txgbe_clear_bp_intr(2, 0, hw);

	/* 3.1. Get the link partner AN73 Base Page Ability */
	BP_LOG("<3.1>. Get the link partner AN73 Base Page Ability ...\n");
	txgbe_get_bp_ability(&lp_ability, 1, hw);

	/* 3.2. Check the AN73 Link Ability with Link Partner */
	BP_LOG("<3.2>. Check the AN73 Link Ability with Link Partner ...\n");
	BP_LOG("       Local Link Ability: 0x%x\n", local_ability.link_ability);
	BP_LOG("   Link Partner Link Ability: 0x%x\n", lp_ability.link_ability);

	status = txgbe_check_bp_ability(&local_ability, &lp_ability, hw);

	wr32_epcs(hw, SR_AN_CTRL, 0);
	wr32_epcs(hw, VR_AN_KR_MODE_CL, 0);

	/* 3.3. Check the FEC and KR Training for KR mode */
	BP_LOG("<3.3>. Check the FEC for KR mode ...\n");
	if ((local_ability.fec_ability & lp_ability.fec_ability) == 0x03) {
		BP_LOG("Enable the Backplane KR FEC ...\n");
		wr32_epcs(hw, SR_PMA_KR_FEC_CTRL, SR_PMA_KR_FEC_CTRL_EN);
	} else {
		BP_LOG("Backplane KR FEC is disabled.\n");
	}

	printf("Enter training.\n");
	/* CL72 KR training on */
	for (i = 0; i < 2; i++) {
		/* 3.4. Check the CL72 KR Training for KR mode */
		BP_LOG("<3.4>. Check the CL72 KR Training for KR mode ...\n");
		BP_LOG("==================%d==================\n", i);
		status = txgbe_enable_kr_training(hw);
		BP_LOG("Check the Clause 72 KR Training status ...\n");
		status |= txgbe_check_kr_training(hw);

		lp_reg = rd32_epcs(hw, SR_PMA_KR_LP_CESTS);
		lp_reg &= SR_PMA_KR_LP_CESTS_RR;
		BP_LOG("SR PMA MMD 10GBASE-KR LP Coefficient Status Register: 0x%x\n",
			lp_reg);
		ld_reg = rd32_epcs(hw, SR_PMA_KR_LD_CESTS);
		ld_reg &= SR_PMA_KR_LD_CESTS_RR;
		BP_LOG("SR PMA MMD 10GBASE-KR LD Coefficient Status Register: 0x%x\n",
			ld_reg);
		if (hw->devarg.poll == 0 && status != 0)
			lp_reg = SR_PMA_KR_LP_CESTS_RR;

		if (lp_reg & ld_reg) {
			BP_LOG("==================out==================\n");
			status = txgbe_disable_kr_training(hw, 0, 0);
			wr32_epcs(hw, SR_AN_CTRL, 0);
			txgbe_clear_bp_intr(2, 0, hw);
			txgbe_clear_bp_intr(1, 0, hw);
			txgbe_clear_bp_intr(0, 0, hw);
			for (i = 0; i < 10; i++) {
				value = rd32_epcs(hw, SR_XS_PCS_KR_STS1);
				if (value & SR_XS_PCS_KR_STS1_PLU) {
					BP_LOG("\nINT_AN_INT_CMPLT =1, AN73 Done Success.\n");
					wr32_epcs(hw, SR_AN_CTRL, 0);
					return 0;
				}
				msec_delay(10);
			}
			msec_delay(1000);
			txgbe_set_link_to_kr(hw, 0);

			return 0;
		}

		status |= txgbe_disable_kr_training(hw, 0, 0);
	}

	txgbe_clear_bp_intr(2, 0, hw);
	txgbe_clear_bp_intr(1, 0, hw);
	txgbe_clear_bp_intr(0, 0, hw);

	return status;
}

/**
 * txgbe_get_bp_ability
 * @hw: pointer to hardware structure
 * @ability: pointer to blackplane ability structure
 * @link_partner:
 *	1: Get Link Partner Base Page
 *	2: Get Link Partner Next Page
 *		(only get NXP Ability Register 1 at the moment)
 *	0: Get Local Device Base Page
 */
static void txgbe_get_bp_ability(struct txgbe_backplane_ability *ability,
		u32 link_partner, struct txgbe_hw *hw)
{
	u32 value = 0;

	/* Link Partner Base Page */
	if (link_partner == 1) {
		/* Read the link partner AN73 Base Page Ability Registers */
		BP_LOG("Read the link partner AN73 Base Page Ability Registers...\n");
		value = rd32_epcs(hw, SR_AN_MMD_LP_ABL1);
		BP_LOG("SR AN MMD LP Base Page Ability Register 1: 0x%x\n",
			value);
		ability->next_page = SR_MMD_LP_ABL1_ADV_NP(value);
		BP_LOG("  Next Page (bit15): %d\n", ability->next_page);

		value = rd32_epcs(hw, SR_AN_MMD_LP_ABL2);
		BP_LOG("SR AN MMD LP Base Page Ability Register 2: 0x%x\n",
			value);
		ability->link_ability =
			value & SR_AN_MMD_LP_ABL2_BP_TYPE_KR_KX4_KX;
		BP_LOG("  Link Ability (bit[15:0]): 0x%x\n",
			ability->link_ability);
		BP_LOG("  (0x20- KX_ONLY, 0x40- KX4_ONLY, 0x60- KX4_KX\n");
		BP_LOG("   0x80- KR_ONLY, 0xA0- KR_KX, 0xC0- KR_KX4, 0xE0- KR_KX4_KX)\n");

		value = rd32_epcs(hw, SR_AN_MMD_LP_ABL3);
		BP_LOG("SR AN MMD LP Base Page Ability Register 3: 0x%x\n",
			value);
		BP_LOG("  FEC Request (bit15): %d\n", ((value >> 15) & 0x01));
		BP_LOG("  FEC Enable  (bit14): %d\n", ((value >> 14) & 0x01));
		ability->fec_ability = SR_AN_MMD_LP_ABL3_FCE(value);
	} else if (link_partner == 2) {
		/* Read the link partner AN73 Next Page Ability Registers */
		BP_LOG("\nRead the link partner AN73 Next Page Ability Registers...\n");
		value = rd32_epcs(hw, SR_AN_LP_XNP_ABL1);
		BP_LOG(" SR AN MMD LP XNP Ability Register 1: 0x%x\n", value);
		ability->next_page = SR_AN_LP_XNP_ABL1_NP(value);
		BP_LOG("  Next Page (bit15): %d\n", ability->next_page);
	} else {
		/* Read the local AN73 Base Page Ability Registers */
		BP_LOG("Read the local AN73 Base Page Ability Registers...\n");
		value = rd32_epcs(hw, SR_AN_MMD_ADV_REG1);
		BP_LOG("SR AN MMD Advertisement Register 1: 0x%x\n", value);
		ability->next_page = SR_AN_MMD_ADV_REG1_NP(value);
		BP_LOG("  Next Page (bit15): %d\n", ability->next_page);

		value = rd32_epcs(hw, SR_AN_MMD_ADV_REG2);
		BP_LOG("SR AN MMD Advertisement Register 2: 0x%x\n", value);
		ability->link_ability =
			value & SR_AN_MMD_ADV_REG2_BP_TYPE_KR_KX4_KX;
		BP_LOG("  Link Ability (bit[15:0]): 0x%x\n",
			ability->link_ability);
		BP_LOG("  (0x20- KX_ONLY, 0x40- KX4_ONLY, 0x60- KX4_KX\n");
		BP_LOG("   0x80- KR_ONLY, 0xA0- KR_KX, 0xC0- KR_KX4, 0xE0- KR_KX4_KX)\n");

		value = rd32_epcs(hw, SR_AN_MMD_ADV_REG3);
		BP_LOG("SR AN MMD Advertisement Register 3: 0x%x\n", value);
		BP_LOG("  FEC Request (bit15): %d\n", ((value >> 15) & 0x01));
		BP_LOG("  FEC Enable  (bit14): %d\n", ((value >> 14) & 0x01));
		ability->fec_ability = SR_AN_MMD_ADV_REG3_FCE(value);
	}

	BP_LOG("done.\n");
}

/**
 * txgbe_check_bp_ability
 * @hw: pointer to hardware structure
 * @ability: pointer to blackplane ability structure
 */
static s32 txgbe_check_bp_ability(struct txgbe_backplane_ability *local_ability,
	struct txgbe_backplane_ability *lp_ability, struct txgbe_hw *hw)
{
	u32 com_link_abi;
	s32 ret = 0;

	com_link_abi = local_ability->link_ability & lp_ability->link_ability;
	BP_LOG("com_link_abi = 0x%x, local_ability = 0x%x, lp_ability = 0x%x\n",
		com_link_abi, local_ability->link_ability,
		lp_ability->link_ability);

	if (!com_link_abi) {
		BP_LOG("The Link Partner does not support any compatible speed mode.\n");
		ret = -1;
	} else if (com_link_abi & BP_TYPE_KR) {
		if (local_ability->current_link_mode) {
			BP_LOG("Link mode is not matched with Link Partner: [LINK_KR].\n");
			BP_LOG("Set the local link mode to [LINK_KR] ...\n");
			txgbe_set_link_to_kr(hw, 0);
			ret = 1;
		} else {
			BP_LOG("Link mode is matched with Link Partner: [LINK_KR].\n");
			ret = 0;
		}
	} else if (com_link_abi & BP_TYPE_KX4) {
		if (local_ability->current_link_mode == 0x10) {
			BP_LOG("Link mode is matched with Link Partner: [LINK_KX4].\n");
			ret = 0;
		} else {
			BP_LOG("Link mode is not matched with Link Partner: [LINK_KX4].\n");
			BP_LOG("Set the local link mode to [LINK_KX4] ...\n");
			txgbe_set_link_to_kx4(hw, 1);
			ret = 1;
		}
	} else if (com_link_abi & BP_TYPE_KX) {
		if (local_ability->current_link_mode == 0x1) {
			BP_LOG("Link mode is matched with Link Partner: [LINK_KX].\n");
			ret = 0;
		} else {
			BP_LOG("Link mode is not matched with Link Partner: [LINK_KX].\n");
			BP_LOG("Set the local link mode to [LINK_KX] ...\n");
			txgbe_set_link_to_kx(hw, 1, 1);
			ret = 1;
		}
	}

	return ret;
}

/**
 * txgbe_clear_bp_intr
 * @hw: pointer to hardware structure
 * @index: the bit will be cleared
 * @index_high:
 *	index_high = 0: Only the index bit will be cleared
 *	index_high != 0: the [index_high, index] range will be cleared
 */
static void txgbe_clear_bp_intr(u32 bit, u32 bit_high, struct txgbe_hw *hw)
{
	u32 rdata = 0, wdata, i;

	rdata = rd32_epcs(hw, VR_AN_INTR);
	BP_LOG("[Before clear]Read VR AN MMD Interrupt Register: 0x%x\n",
			rdata);
	BP_LOG("Interrupt: 0- AN_INT_CMPLT, 1-  AN_INC_LINK, 2- AN_PG_RCV\n\n");

	wdata = rdata;
	if (bit_high) {
		for (i = bit; i <= bit_high; i++)
			wdata &= ~(1 << i);
	} else {
		wdata &= ~(1 << bit);
	}

	wr32_epcs(hw, VR_AN_INTR, wdata);

	rdata = rd32_epcs(hw, VR_AN_INTR);
	BP_LOG("[After clear]Read VR AN MMD Interrupt Register: 0x%x\n", rdata);
}

static s32 txgbe_enable_kr_training(struct txgbe_hw *hw)
{
	s32 status = 0;
	u32 value = 0;

	BP_LOG("Enable Clause 72 KR Training ...\n");

	if (CL72_KRTR_PRBS_MODE_EN != 0xFFFF) {
		/* Set PRBS Timer Duration Control to maximum 6.7ms in
		 * VR_PMA_KRTR_PRBS_CTRL2 Register
		 */
		value = CL72_KRTR_PRBS_MODE_EN;
		wr32_epcs(hw, VR_PMA_KRTR_PRBS_CTRL2, value);
		/* Set PRBS Timer Duration Control to maximum 6.7ms in
		 * VR_PMA_KRTR_PRBS_CTRL1 Register
		 */
		wr32_epcs(hw, VR_PMA_KRTR_PRBS_CTRL1,
			VR_PMA_KRTR_PRBS_TIME_LMT);
		/* Enable PRBS Mode to determine KR Training Status by setting
		 * Bit 0 of VR_PMA_KRTR_PRBS_CTRL0 Register
		 */
		value = VR_PMA_KRTR_PRBS_MODE_EN;
	}
#ifdef CL72_KRTR_PRBS31_EN
	/* Enable PRBS Mode to determine KR Training Status by setting
	 * Bit 1 of VR_PMA_KRTR_PRBS_CTRL0 Register
	 */
	value = VR_PMA_KRTR_PRBS31_EN;
#endif
	wr32_epcs(hw, VR_PMA_KRTR_PRBS_CTRL0, value);
	/* Read PHY Lane0 TX EQ before Clause 72 KR Training. */
	txgbe_read_phy_lane_tx_eq(0, hw, 0, 0);

	/* Enable the Clause 72 start-up protocol
	 *   by setting Bit 1 of SR_PMA_KR_PMD_CTRL Register.
	 * Restart the Clause 72 start-up protocol
	 *   by setting Bit 0 of SR_PMA_KR_PMD_CTRL Register.
	 */
	wr32_epcs(hw, SR_PMA_KR_PMD_CTRL,
		SR_PMA_KR_PMD_CTRL_EN_TR | SR_PMA_KR_PMD_CTRL_RS_TR);

	return status;
}

static s32 txgbe_disable_kr_training(struct txgbe_hw *hw, s32 post, s32 mode)
{
	s32 status = 0;

	BP_LOG("Disable Clause 72 KR Training ...\n");
	/* Read PHY Lane0 TX EQ before Clause 72 KR Training. */
	txgbe_read_phy_lane_tx_eq(0, hw, post, mode);

	wr32_epcs(hw, SR_PMA_KR_PMD_CTRL, SR_PMA_KR_PMD_CTRL_RS_TR);

	return status;
}

static s32 txgbe_check_kr_training(struct txgbe_hw *hw)
{
	s32 status = 0;
	u32 value, test;
	int i;
	int times = hw->devarg.poll ? 35 : 20;

	for (i = 0; i < times; i++) {
		value = rd32_epcs(hw, SR_PMA_KR_LP_CEU);
		BP_LOG("SR PMA MMD 10GBASE-KR LP Coefficient Update Register: 0x%x\n",
			value);
		value = rd32_epcs(hw, SR_PMA_KR_LP_CESTS);
		BP_LOG("SR PMA MMD 10GBASE-KR LP Coefficient Status Register: 0x%x\n",
			value);
		value = rd32_epcs(hw, SR_PMA_KR_LD_CEU);
		BP_LOG("SR PMA MMD 10GBASE-KR LD Coefficient Update: 0x%x\n",
			value);
		value = rd32_epcs(hw, SR_PMA_KR_LD_CESTS);
		BP_LOG("SR PMA MMD 10GBASE-KR LD Coefficient Status: 0x%x\n",
			value);
		value = rd32_epcs(hw, SR_PMA_KR_PMD_STS);
		BP_LOG("SR PMA MMD 10GBASE-KR Status Register: 0x%x\n", value);
		BP_LOG("  Training Failure         (bit3): %d\n",
			((value >> 3) & 0x01));
		BP_LOG("  Start-Up Protocol Status (bit2): %d\n",
			((value >> 2) & 0x01));
		BP_LOG("  Frame Lock               (bit1): %d\n",
			((value >> 1) & 0x01));
		BP_LOG("  Receiver Status          (bit0): %d\n",
			((value >> 0) & 0x01));

		test = rd32_epcs(hw, SR_PMA_KR_LP_CESTS);
		if (test & SR_PMA_KR_LP_CESTS_RR) {
			BP_LOG("TEST Coefficient Status Register: 0x%x\n",
				test);
			status = 1;
		}

		if (value & SR_PMA_KR_PMD_STS_TR_FAIL) {
			BP_LOG("Training is completed with failure.\n");
			txgbe_read_phy_lane_tx_eq(0, hw, 0, 0);
			return 0;
		}

		if (value & SR_PMA_KR_PMD_STS_RCV) {
			BP_LOG("Receiver trained and ready to receive data.\n");
			txgbe_read_phy_lane_tx_eq(0, hw, 0, 0);
			return 0;
		}

		msec_delay(20);
	}

	BP_LOG("ERROR: Check Clause 72 KR Training Complete Timeout.\n");
	return status;
}

static void txgbe_read_phy_lane_tx_eq(u16 lane, struct txgbe_hw *hw,
				s32 post, s32 mode)
{
	u32 value = 0;
	u32 addr;
	u32 tx_main_cursor, tx_pre_cursor, tx_post_cursor, lmain;

	addr = TXGBE_PHY_LANE0_TX_EQ_CTL1 | (lane << 8);
	value = rd32_ephy(hw, addr);
	BP_LOG("PHY LANE TX EQ Read Value: %x\n", lane);
	tx_main_cursor = TXGBE_PHY_LANE0_TX_EQ_CTL1_MAIN(value);
	BP_LOG("TX_MAIN_CURSOR: %x\n", tx_main_cursor);
	UNREFERENCED_PARAMETER(tx_main_cursor);

	addr = TXGBE_PHY_LANE0_TX_EQ_CTL2 | (lane << 8);
	value = rd32_ephy(hw, addr);
	tx_pre_cursor = value & TXGBE_PHY_LANE0_TX_EQ_CTL2_PRE;
	tx_post_cursor = TXGBE_PHY_LANE0_TX_EQ_CTL2_POST(value);
	BP_LOG("TX_PRE_CURSOR: %x\n", tx_pre_cursor);
	BP_LOG("TX_POST_CURSOR: %x\n", tx_post_cursor);

	if (mode == 1) {
		lmain = 160 - tx_pre_cursor - tx_post_cursor;
		if (lmain < 88)
			lmain = 88;

		if (post)
			tx_post_cursor = post;

		wr32_epcs(hw, TXGBE_PHY_EQ_INIT_CTL1, tx_post_cursor);
		wr32_epcs(hw, TXGBE_PHY_EQ_INIT_CTL0,
				tx_pre_cursor | (lmain << 8));
		value = rd32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1);
		value &= ~TXGBE_PHY_TX_EQ_CTL1_DEF;
		wr32_epcs(hw, TXGBE_PHY_TX_EQ_CTL1, value);
	}
}
