/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "ngbe_type.h"
#include "ngbe_eeprom.h"
#include "ngbe_mng.h"
#include "ngbe_hw.h"

/**
 *  ngbe_set_lan_id_multi_port - Set LAN id for PCIe multiple port devices
 *  @hw: pointer to the HW structure
 *
 *  Determines the LAN function id by reading memory-mapped registers and swaps
 *  the port value if requested, and set MAC instance for devices.
 **/
void ngbe_set_lan_id_multi_port(struct ngbe_hw *hw)
{
	struct ngbe_bus_info *bus = &hw->bus;
	u32 reg = 0;

	DEBUGFUNC("ngbe_set_lan_id_multi_port");

	reg = rd32(hw, NGBE_PORTSTAT);
	bus->lan_id = NGBE_PORTSTAT_ID(reg);
	bus->func = bus->lan_id;
}

/**
 *  ngbe_acquire_swfw_sync - Acquire SWFW semaphore
 *  @hw: pointer to hardware structure
 *  @mask: Mask to specify which semaphore to acquire
 *
 *  Acquires the SWFW semaphore through the MNGSEM register for the specified
 *  function (CSR, PHY0, PHY1, EEPROM, Flash)
 **/
s32 ngbe_acquire_swfw_sync(struct ngbe_hw *hw, u32 mask)
{
	u32 mngsem = 0;
	u32 swmask = NGBE_MNGSEM_SW(mask);
	u32 fwmask = NGBE_MNGSEM_FW(mask);
	u32 timeout = 200;
	u32 i;

	DEBUGFUNC("ngbe_acquire_swfw_sync");

	for (i = 0; i < timeout; i++) {
		/*
		 * SW NVM semaphore bit is used for access to all
		 * SW_FW_SYNC bits (not just NVM)
		 */
		if (ngbe_get_eeprom_semaphore(hw))
			return NGBE_ERR_SWFW_SYNC;

		mngsem = rd32(hw, NGBE_MNGSEM);
		if (mngsem & (fwmask | swmask)) {
			/* Resource is currently in use by FW or SW */
			ngbe_release_eeprom_semaphore(hw);
			msec_delay(5);
		} else {
			mngsem |= swmask;
			wr32(hw, NGBE_MNGSEM, mngsem);
			ngbe_release_eeprom_semaphore(hw);
			return 0;
		}
	}

	/* If time expired clear the bits holding the lock and retry */
	if (mngsem & (fwmask | swmask))
		ngbe_release_swfw_sync(hw, mngsem & (fwmask | swmask));

	msec_delay(5);
	return NGBE_ERR_SWFW_SYNC;
}

/**
 *  ngbe_release_swfw_sync - Release SWFW semaphore
 *  @hw: pointer to hardware structure
 *  @mask: Mask to specify which semaphore to release
 *
 *  Releases the SWFW semaphore through the MNGSEM register for the specified
 *  function (CSR, PHY0, PHY1, EEPROM, Flash)
 **/
void ngbe_release_swfw_sync(struct ngbe_hw *hw, u32 mask)
{
	u32 mngsem;
	u32 swmask = mask;

	DEBUGFUNC("ngbe_release_swfw_sync");

	ngbe_get_eeprom_semaphore(hw);

	mngsem = rd32(hw, NGBE_MNGSEM);
	mngsem &= ~swmask;
	wr32(hw, NGBE_MNGSEM, mngsem);

	ngbe_release_eeprom_semaphore(hw);
}

/**
 *  ngbe_set_mac_type - Sets MAC type
 *  @hw: pointer to the HW structure
 *
 *  This function sets the mac type of the adapter based on the
 *  vendor ID and device ID stored in the hw structure.
 **/
s32 ngbe_set_mac_type(struct ngbe_hw *hw)
{
	s32 err = 0;

	DEBUGFUNC("ngbe_set_mac_type");

	if (hw->vendor_id != PCI_VENDOR_ID_WANGXUN) {
		DEBUGOUT("Unsupported vendor id: %x", hw->vendor_id);
		return NGBE_ERR_DEVICE_NOT_SUPPORTED;
	}

	switch (hw->sub_device_id) {
	case NGBE_SUB_DEV_ID_EM_RTL_SGMII:
	case NGBE_SUB_DEV_ID_EM_MVL_RGMII:
		hw->phy.media_type = ngbe_media_type_copper;
		hw->mac.type = ngbe_mac_em;
		break;
	case NGBE_SUB_DEV_ID_EM_MVL_SFP:
	case NGBE_SUB_DEV_ID_EM_YT8521S_SFP:
		hw->phy.media_type = ngbe_media_type_fiber;
		hw->mac.type = ngbe_mac_em;
		break;
	case NGBE_SUB_DEV_ID_EM_VF:
		hw->phy.media_type = ngbe_media_type_virtual;
		hw->mac.type = ngbe_mac_em_vf;
		break;
	default:
		err = NGBE_ERR_DEVICE_NOT_SUPPORTED;
		hw->phy.media_type = ngbe_media_type_unknown;
		hw->mac.type = ngbe_mac_unknown;
		DEBUGOUT("Unsupported device id: %x", hw->device_id);
		break;
	}

	DEBUGOUT("found mac: %d media: %d, returns: %d\n",
		  hw->mac.type, hw->phy.media_type, err);
	return err;
}

void ngbe_map_device_id(struct ngbe_hw *hw)
{
	u16 oem = hw->sub_system_id & NGBE_OEM_MASK;
	u16 internal = hw->sub_system_id & NGBE_INTERNAL_MASK;
	hw->is_pf = true;

	/* move subsystem_device_id to device_id */
	switch (hw->device_id) {
	case NGBE_DEV_ID_EM_WX1860AL_W_VF:
	case NGBE_DEV_ID_EM_WX1860A2_VF:
	case NGBE_DEV_ID_EM_WX1860A2S_VF:
	case NGBE_DEV_ID_EM_WX1860A4_VF:
	case NGBE_DEV_ID_EM_WX1860A4S_VF:
	case NGBE_DEV_ID_EM_WX1860AL2_VF:
	case NGBE_DEV_ID_EM_WX1860AL2S_VF:
	case NGBE_DEV_ID_EM_WX1860AL4_VF:
	case NGBE_DEV_ID_EM_WX1860AL4S_VF:
	case NGBE_DEV_ID_EM_WX1860NCSI_VF:
	case NGBE_DEV_ID_EM_WX1860A1_VF:
	case NGBE_DEV_ID_EM_WX1860A1L_VF:
		hw->device_id = NGBE_DEV_ID_EM_VF;
		hw->sub_device_id = NGBE_SUB_DEV_ID_EM_VF;
		hw->is_pf = false;
		break;
	case NGBE_DEV_ID_EM_WX1860AL_W:
	case NGBE_DEV_ID_EM_WX1860A2:
	case NGBE_DEV_ID_EM_WX1860A2S:
	case NGBE_DEV_ID_EM_WX1860A4:
	case NGBE_DEV_ID_EM_WX1860A4S:
	case NGBE_DEV_ID_EM_WX1860AL2:
	case NGBE_DEV_ID_EM_WX1860AL2S:
	case NGBE_DEV_ID_EM_WX1860AL4:
	case NGBE_DEV_ID_EM_WX1860AL4S:
	case NGBE_DEV_ID_EM_WX1860NCSI:
	case NGBE_DEV_ID_EM_WX1860A1:
	case NGBE_DEV_ID_EM_WX1860A1L:
		hw->device_id = NGBE_DEV_ID_EM;
		if (oem == NGBE_LY_M88E1512_SFP ||
				internal == NGBE_INTERNAL_SFP)
			hw->sub_device_id = NGBE_SUB_DEV_ID_EM_MVL_SFP;
		else if (hw->sub_system_id == NGBE_SUB_DEV_ID_EM_M88E1512_RJ45)
			hw->sub_device_id = NGBE_SUB_DEV_ID_EM_MVL_RGMII;
		else if (oem == NGBE_YT8521S_SFP ||
				oem == NGBE_LY_YT8521S_SFP)
			hw->sub_device_id = NGBE_SUB_DEV_ID_EM_YT8521S_SFP;
		else
			hw->sub_device_id = NGBE_SUB_DEV_ID_EM_RTL_SGMII;
		break;
	default:
		break;
	}
}

/**
 *  ngbe_init_ops_pf - Inits func ptrs and MAC type
 *  @hw: pointer to hardware structure
 *
 *  Initialize the function pointers and assign the MAC type.
 *  Does not touch the hardware.
 **/
s32 ngbe_init_ops_pf(struct ngbe_hw *hw)
{
	struct ngbe_bus_info *bus = &hw->bus;
	struct ngbe_mac_info *mac = &hw->mac;
	struct ngbe_rom_info *rom = &hw->rom;

	DEBUGFUNC("ngbe_init_ops_pf");

	/* BUS */
	bus->set_lan_id = ngbe_set_lan_id_multi_port;

	/* MAC */
	mac->acquire_swfw_sync = ngbe_acquire_swfw_sync;
	mac->release_swfw_sync = ngbe_release_swfw_sync;

	/* EEPROM */
	rom->init_params = ngbe_init_eeprom_params;
	rom->validate_checksum = ngbe_validate_eeprom_checksum_em;

	return 0;
}

/**
 *  ngbe_init_shared_code - Initialize the shared code
 *  @hw: pointer to hardware structure
 *
 *  This will assign function pointers and assign the MAC type and PHY code.
 *  Does not touch the hardware. This function must be called prior to any
 *  other function in the shared code. The ngbe_hw structure should be
 *  memset to 0 prior to calling this function.  The following fields in
 *  hw structure should be filled in prior to calling this function:
 *  hw_addr, back, device_id, vendor_id, subsystem_device_id
 **/
s32 ngbe_init_shared_code(struct ngbe_hw *hw)
{
	s32 status = 0;

	DEBUGFUNC("ngbe_init_shared_code");

	/*
	 * Set the mac type
	 */
	ngbe_set_mac_type(hw);

	ngbe_init_ops_dummy(hw);
	switch (hw->mac.type) {
	case ngbe_mac_em:
		ngbe_init_ops_pf(hw);
		break;
	default:
		status = NGBE_ERR_DEVICE_NOT_SUPPORTED;
		break;
	}

	hw->bus.set_lan_id(hw);

	return status;
}

