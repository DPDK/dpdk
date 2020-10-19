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

