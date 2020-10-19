/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include "txgbe_hw.h"
#include "txgbe_eeprom.h"
#include "txgbe_mng.h"
#include "txgbe_phy.h"

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
	RTE_SET_USED(hw);
	return 0;
}

/**
 *  txgbe_identify_qsfp_module - Identifies QSFP modules
 *  @hw: pointer to hardware structure
 *
 *  Searches for and identifies the QSFP module and assigns appropriate PHY type
 **/
s32 txgbe_identify_qsfp_module(struct txgbe_hw *hw)
{
	RTE_SET_USED(hw);
	return 0;
}

