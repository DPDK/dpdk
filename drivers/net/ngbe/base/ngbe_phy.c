/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "ngbe_hw.h"
#include "ngbe_phy.h"

s32 ngbe_mdi_map_register(mdi_reg_t *reg, mdi_reg_22_t *reg22)
{
	bool match = 1;
	switch (reg->device_type) {
	case NGBE_MD_DEV_PMA_PMD:
		switch (reg->addr) {
		case NGBE_MD_PHY_ID_HIGH:
		case NGBE_MD_PHY_ID_LOW:
			reg22->page = 0;
			reg22->addr = reg->addr;
			reg22->device_type = 0;
			break;
		default:
			match = 0;
		}
		break;
	default:
		match = 0;
		break;
	}

	if (!match) {
		reg22->page = reg->device_type;
		reg22->device_type = reg->device_type;
		reg22->addr = reg->addr;
	}

	return 0;
}

/**
 * ngbe_probe_phy - Identify a single address for a PHY
 * @hw: pointer to hardware structure
 * @phy_addr: PHY address to probe
 *
 * Returns true if PHY found
 */
static bool ngbe_probe_phy(struct ngbe_hw *hw, u16 phy_addr)
{
	if (!ngbe_validate_phy_addr(hw, phy_addr)) {
		DEBUGOUT("Unable to validate PHY address 0x%04X",
			phy_addr);
		return false;
	}

	if (ngbe_get_phy_id(hw))
		return false;

	hw->phy.type = ngbe_get_phy_type_from_id(hw);
	if (hw->phy.type == ngbe_phy_unknown)
		return false;

	return true;
}

/**
 *  ngbe_identify_phy - Get physical layer module
 *  @hw: pointer to hardware structure
 *
 *  Determines the physical layer module found on the current adapter.
 **/
s32 ngbe_identify_phy(struct ngbe_hw *hw)
{
	s32 err = NGBE_ERR_PHY_ADDR_INVALID;
	u16 phy_addr;

	if (hw->phy.type != ngbe_phy_unknown)
		return 0;

	/* select clause22 */
	wr32(hw, NGBE_MDIOMODE, NGBE_MDIOMODE_MASK);

	for (phy_addr = 0; phy_addr < NGBE_MAX_PHY_ADDR; phy_addr++) {
		if (ngbe_probe_phy(hw, phy_addr)) {
			err = 0;
			break;
		}
	}

	return err;
}

/**
 * ngbe_check_reset_blocked - check status of MNG FW veto bit
 * @hw: pointer to the hardware structure
 *
 * This function checks the STAT.MNGVETO bit to see if there are
 * any constraints on link from manageability.  For MAC's that don't
 * have this bit just return faluse since the link can not be blocked
 * via this method.
 **/
s32 ngbe_check_reset_blocked(struct ngbe_hw *hw)
{
	u32 mmngc;

	mmngc = rd32(hw, NGBE_STAT);
	if (mmngc & NGBE_STAT_MNGVETO) {
		DEBUGOUT("MNG_VETO bit detected.");
		return true;
	}

	return false;
}

/**
 *  ngbe_validate_phy_addr - Determines phy address is valid
 *  @hw: pointer to hardware structure
 *  @phy_addr: PHY address
 *
 **/
bool ngbe_validate_phy_addr(struct ngbe_hw *hw, u32 phy_addr)
{
	u16 phy_id = 0;
	bool valid = false;

	hw->phy.addr = phy_addr;
	hw->phy.read_reg(hw, NGBE_MD_PHY_ID_LOW,
			     NGBE_MD_DEV_PMA_PMD, &phy_id);

	if (phy_id != 0xFFFF && phy_id != 0x0)
		valid = true;

	DEBUGOUT("PHY ID LOW is 0x%04X", phy_id);

	return valid;
}

/**
 *  ngbe_get_phy_id - Get the phy ID
 *  @hw: pointer to hardware structure
 *
 **/
s32 ngbe_get_phy_id(struct ngbe_hw *hw)
{
	u32 err;
	u16 phy_id_high = 0;
	u16 phy_id_low = 0;

	err = hw->phy.read_reg(hw, NGBE_MD_PHY_ID_HIGH,
				      NGBE_MD_DEV_PMA_PMD,
				      &phy_id_high);
	hw->phy.id = (u32)(phy_id_high << 16);

	err = hw->phy.read_reg(hw, NGBE_MD_PHY_ID_LOW,
				NGBE_MD_DEV_PMA_PMD,
				&phy_id_low);
	hw->phy.id |= (u32)(phy_id_low & NGBE_PHY_REVISION_MASK);
	hw->phy.revision = (u32)(phy_id_low & ~NGBE_PHY_REVISION_MASK);

	DEBUGOUT("PHY_ID_HIGH 0x%04X, PHY_ID_LOW 0x%04X",
		  phy_id_high, phy_id_low);

	return err;
}

/**
 *  ngbe_get_phy_type_from_id - Get the phy type
 *  @phy_id: PHY ID information
 *
 **/
enum ngbe_phy_type ngbe_get_phy_type_from_id(struct ngbe_hw *hw)
{
	enum ngbe_phy_type phy_type;

	switch (hw->phy.id) {
	case NGBE_PHYID_RTL:
		phy_type = ngbe_phy_rtl;
		break;
	case NGBE_PHYID_MVL:
		if (hw->phy.media_type == ngbe_media_type_fiber)
			phy_type = ngbe_phy_mvl_sfi;
		else
			phy_type = ngbe_phy_mvl;
		break;
	case NGBE_PHYID_YT:
		if (hw->phy.media_type == ngbe_media_type_fiber)
			phy_type = ngbe_phy_yt8521s_sfi;
		else
			phy_type = ngbe_phy_yt8521s;
		break;
	default:
		phy_type = ngbe_phy_unknown;
		break;
	}

	return phy_type;
}

/**
 *  ngbe_reset_phy - Performs a PHY reset
 *  @hw: pointer to hardware structure
 **/
s32 ngbe_reset_phy(struct ngbe_hw *hw)
{
	s32 err = 0;

	if (hw->phy.type == ngbe_phy_unknown)
		err = ngbe_identify_phy(hw);

	if (err != 0 || hw->phy.type == ngbe_phy_none)
		return err;

	/* Don't reset PHY if it's shut down due to overtemp. */
	if (hw->mac.check_overtemp(hw) == NGBE_ERR_OVERTEMP)
		return err;

	/* Blocked by MNG FW so bail */
	if (ngbe_check_reset_blocked(hw))
		return err;

	switch (hw->phy.type) {
	case ngbe_phy_rtl:
		err = ngbe_reset_phy_rtl(hw);
		break;
	case ngbe_phy_mvl:
	case ngbe_phy_mvl_sfi:
		err = ngbe_reset_phy_mvl(hw);
		break;
	case ngbe_phy_yt8521s:
	case ngbe_phy_yt8521s_sfi:
		err = ngbe_reset_phy_yt(hw);
		break;
	default:
		break;
	}

	return err;
}

/**
 *  ngbe_read_phy_mdi - Reads a value from a specified PHY register without
 *  the SWFW lock
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit address of PHY register to read
 *  @device_type: 5 bit device type
 *  @phy_data: Pointer to read data from PHY register
 **/
s32 ngbe_read_phy_reg_mdi(struct ngbe_hw *hw, u32 reg_addr, u32 device_type,
			   u16 *phy_data)
{
	u32 command, data;

	/* Setup and write the address cycle command */
	command = NGBE_MDIOSCA_REG(reg_addr) |
		  NGBE_MDIOSCA_DEV(device_type) |
		  NGBE_MDIOSCA_PORT(hw->phy.addr);
	wr32(hw, NGBE_MDIOSCA, command);

	command = NGBE_MDIOSCD_CMD_READ |
		  NGBE_MDIOSCD_BUSY |
		  NGBE_MDIOSCD_CLOCK(6);
	wr32(hw, NGBE_MDIOSCD, command);

	/*
	 * Check every 10 usec to see if the address cycle completed.
	 * The MDI Command bit will clear when the operation is
	 * complete
	 */
	if (!po32m(hw, NGBE_MDIOSCD, NGBE_MDIOSCD_BUSY,
		0, NULL, 100, 100)) {
		DEBUGOUT("PHY address command did not complete");
		return NGBE_ERR_PHY;
	}

	data = rd32(hw, NGBE_MDIOSCD);
	*phy_data = (u16)NGBE_MDIOSCD_DAT_R(data);

	return 0;
}

/**
 *  ngbe_read_phy_reg - Reads a value from a specified PHY register
 *  using the SWFW lock - this function is needed in most cases
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit address of PHY register to read
 *  @device_type: 5 bit device type
 *  @phy_data: Pointer to read data from PHY register
 **/
s32 ngbe_read_phy_reg(struct ngbe_hw *hw, u32 reg_addr,
			       u32 device_type, u16 *phy_data)
{
	s32 err;
	u32 gssr = hw->phy.phy_semaphore_mask;

	if (hw->mac.acquire_swfw_sync(hw, gssr))
		return NGBE_ERR_SWFW_SYNC;

	err = hw->phy.read_reg_unlocked(hw, reg_addr, device_type,
					phy_data);

	hw->mac.release_swfw_sync(hw, gssr);

	return err;
}

/**
 *  ngbe_write_phy_reg_mdi - Writes a value to specified PHY register
 *  without SWFW lock
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit PHY register to write
 *  @device_type: 5 bit device type
 *  @phy_data: Data to write to the PHY register
 **/
s32 ngbe_write_phy_reg_mdi(struct ngbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 phy_data)
{
	u32 command;

	/* write command */
	command = NGBE_MDIOSCA_REG(reg_addr) |
		  NGBE_MDIOSCA_DEV(device_type) |
		  NGBE_MDIOSCA_PORT(hw->phy.addr);
	wr32(hw, NGBE_MDIOSCA, command);

	command = NGBE_MDIOSCD_CMD_WRITE |
		  NGBE_MDIOSCD_DAT(phy_data) |
		  NGBE_MDIOSCD_BUSY |
		  NGBE_MDIOSCD_CLOCK(6);
	wr32(hw, NGBE_MDIOSCD, command);

	/* wait for completion */
	if (!po32m(hw, NGBE_MDIOSCD, NGBE_MDIOSCD_BUSY,
		0, NULL, 100, 100)) {
		DEBUGOUT("PHY write cmd didn't complete");
		return NGBE_ERR_PHY;
	}

	return 0;
}

/**
 *  ngbe_write_phy_reg - Writes a value to specified PHY register
 *  using SWFW lock- this function is needed in most cases
 *  @hw: pointer to hardware structure
 *  @reg_addr: 32 bit PHY register to write
 *  @device_type: 5 bit device type
 *  @phy_data: Data to write to the PHY register
 **/
s32 ngbe_write_phy_reg(struct ngbe_hw *hw, u32 reg_addr,
				u32 device_type, u16 phy_data)
{
	s32 err;
	u32 gssr = hw->phy.phy_semaphore_mask;

	if (hw->mac.acquire_swfw_sync(hw, gssr))
		err = NGBE_ERR_SWFW_SYNC;

	err = hw->phy.write_reg_unlocked(hw, reg_addr, device_type,
					 phy_data);

	hw->mac.release_swfw_sync(hw, gssr);

	return err;
}

/**
 *  ngbe_init_phy - PHY specific init
 *  @hw: pointer to hardware structure
 *
 *  Initialize any function pointers that were not able to be
 *  set during init_shared_code because the PHY type was
 *  not known.
 *
 **/
s32 ngbe_init_phy(struct ngbe_hw *hw)
{
	struct ngbe_phy_info *phy = &hw->phy;
	s32 err = 0;

	hw->phy.addr = 0;

	switch (hw->sub_device_id) {
	case NGBE_SUB_DEV_ID_EM_RTL_SGMII:
		hw->phy.read_reg_unlocked = ngbe_read_phy_reg_rtl;
		hw->phy.write_reg_unlocked = ngbe_write_phy_reg_rtl;
		break;
	case NGBE_SUB_DEV_ID_EM_MVL_RGMII:
	case NGBE_SUB_DEV_ID_EM_MVL_SFP:
		hw->phy.read_reg_unlocked = ngbe_read_phy_reg_mvl;
		hw->phy.write_reg_unlocked = ngbe_write_phy_reg_mvl;
		break;
	case NGBE_SUB_DEV_ID_EM_YT8521S_SFP:
		hw->phy.read_reg_unlocked = ngbe_read_phy_reg_yt;
		hw->phy.write_reg_unlocked = ngbe_write_phy_reg_yt;
		break;
	default:
		break;
	}

	hw->phy.phy_semaphore_mask = NGBE_MNGSEM_SWPHY;

	/* Identify the PHY */
	err = phy->identify(hw);
	if (err == NGBE_ERR_PHY_ADDR_INVALID)
		goto init_phy_ops_out;

	/* Set necessary function pointers based on PHY type */
	switch (hw->phy.type) {
	case ngbe_phy_rtl:
		hw->phy.init_hw = ngbe_init_phy_rtl;
		hw->phy.check_link = ngbe_check_phy_link_rtl;
		hw->phy.setup_link = ngbe_setup_phy_link_rtl;
		hw->phy.get_adv_pause = ngbe_get_phy_advertised_pause_rtl;
		hw->phy.get_lp_adv_pause = ngbe_get_phy_lp_advertised_pause_rtl;
		hw->phy.set_pause_adv = ngbe_set_phy_pause_adv_rtl;
		break;
	case ngbe_phy_mvl:
	case ngbe_phy_mvl_sfi:
		hw->phy.init_hw = ngbe_init_phy_mvl;
		hw->phy.check_link = ngbe_check_phy_link_mvl;
		hw->phy.setup_link = ngbe_setup_phy_link_mvl;
		hw->phy.get_adv_pause = ngbe_get_phy_advertised_pause_mvl;
		hw->phy.get_lp_adv_pause = ngbe_get_phy_lp_advertised_pause_mvl;
		hw->phy.set_pause_adv = ngbe_set_phy_pause_adv_mvl;
		break;
	case ngbe_phy_yt8521s:
	case ngbe_phy_yt8521s_sfi:
		hw->phy.init_hw = ngbe_init_phy_yt;
		hw->phy.check_link = ngbe_check_phy_link_yt;
		hw->phy.setup_link = ngbe_setup_phy_link_yt;
		hw->phy.get_adv_pause = ngbe_get_phy_advertised_pause_yt;
		hw->phy.get_lp_adv_pause = ngbe_get_phy_lp_advertised_pause_yt;
		hw->phy.set_pause_adv = ngbe_set_phy_pause_adv_yt;
	default:
		break;
	}

init_phy_ops_out:
	return err;
}

