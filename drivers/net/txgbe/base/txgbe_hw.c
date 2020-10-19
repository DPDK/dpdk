/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include "txgbe_type.h"
#include "txgbe_hw.h"

#define TXGBE_RAPTOR_RAR_ENTRIES   128

/**
 *  txgbe_set_lan_id_multi_port - Set LAN id for PCIe multiple port devices
 *  @hw: pointer to the HW structure
 *
 *  Determines the LAN function id by reading memory-mapped registers and swaps
 *  the port value if requested, and set MAC instance for devices.
 **/
void txgbe_set_lan_id_multi_port(struct txgbe_hw *hw)
{
	struct txgbe_bus_info *bus = &hw->bus;
	u32 reg;

	DEBUGFUNC("txgbe_set_lan_id_multi_port_pcie");

	reg = rd32(hw, TXGBE_PORTSTAT);
	bus->lan_id = TXGBE_PORTSTAT_ID(reg);

	/* check for single port */
	reg = rd32(hw, TXGBE_PWR);
	if (TXGBE_PWR_LANID(reg) == TXGBE_PWR_LANID_SWAP)
		bus->func = 0;
	else
		bus->func = bus->lan_id;
}

/**
 *  txgbe_init_shared_code - Initialize the shared code
 *  @hw: pointer to hardware structure
 *
 *  This will assign function pointers and assign the MAC type and PHY code.
 *  Does not touch the hardware. This function must be called prior to any
 *  other function in the shared code. The txgbe_hw structure should be
 *  memset to 0 prior to calling this function.  The following fields in
 *  hw structure should be filled in prior to calling this function:
 *  hw_addr, back, device_id, vendor_id, subsystem_device_id,
 *  subsystem_vendor_id, and revision_id
 **/
s32 txgbe_init_shared_code(struct txgbe_hw *hw)
{
	s32 status;

	DEBUGFUNC("txgbe_init_shared_code");

	/*
	 * Set the mac type
	 */
	txgbe_set_mac_type(hw);

	txgbe_init_ops_dummy(hw);
	switch (hw->mac.type) {
	case txgbe_mac_raptor:
		status = txgbe_init_ops_pf(hw);
		break;
	default:
		status = TXGBE_ERR_DEVICE_NOT_SUPPORTED;
		break;
	}
	hw->mac.max_link_up_time = TXGBE_LINK_UP_TIME;

	hw->bus.set_lan_id(hw);

	return status;
}

/**
 *  txgbe_set_mac_type - Sets MAC type
 *  @hw: pointer to the HW structure
 *
 *  This function sets the mac type of the adapter based on the
 *  vendor ID and device ID stored in the hw structure.
 **/
s32 txgbe_set_mac_type(struct txgbe_hw *hw)
{
	s32 err = 0;

	DEBUGFUNC("txgbe_set_mac_type");

	if (hw->vendor_id != PCI_VENDOR_ID_WANGXUN) {
		DEBUGOUT("Unsupported vendor id: %x", hw->vendor_id);
		return TXGBE_ERR_DEVICE_NOT_SUPPORTED;
	}

	switch (hw->device_id) {
	case TXGBE_DEV_ID_RAPTOR_KR_KX_KX4:
		hw->phy.media_type = txgbe_media_type_backplane;
		hw->mac.type = txgbe_mac_raptor;
		break;
	case TXGBE_DEV_ID_RAPTOR_XAUI:
	case TXGBE_DEV_ID_RAPTOR_SGMII:
		hw->phy.media_type = txgbe_media_type_copper;
		hw->mac.type = txgbe_mac_raptor;
		break;
	case TXGBE_DEV_ID_RAPTOR_SFP:
	case TXGBE_DEV_ID_WX1820_SFP:
		hw->phy.media_type = txgbe_media_type_fiber;
		hw->mac.type = txgbe_mac_raptor;
		break;
	case TXGBE_DEV_ID_RAPTOR_QSFP:
		hw->phy.media_type = txgbe_media_type_fiber_qsfp;
		hw->mac.type = txgbe_mac_raptor;
		break;
	case TXGBE_DEV_ID_RAPTOR_VF:
	case TXGBE_DEV_ID_RAPTOR_VF_HV:
		hw->phy.media_type = txgbe_media_type_virtual;
		hw->mac.type = txgbe_mac_raptor_vf;
		break;
	default:
		err = TXGBE_ERR_DEVICE_NOT_SUPPORTED;
		DEBUGOUT("Unsupported device id: %x", hw->device_id);
		break;
	}

	DEBUGOUT("found mac: %d media: %d, returns: %d\n",
		  hw->mac.type, hw->phy.media_type, err);
	return err;
}

/**
 *  txgbe_init_ops_pf - Inits func ptrs and MAC type
 *  @hw: pointer to hardware structure
 *
 *  Initialize the function pointers and assign the MAC type.
 *  Does not touch the hardware.
 **/
s32 txgbe_init_ops_pf(struct txgbe_hw *hw)
{
	struct txgbe_bus_info *bus = &hw->bus;
	struct txgbe_mac_info *mac = &hw->mac;

	DEBUGFUNC("txgbe_init_ops_pf");

	/* BUS */
	bus->set_lan_id = txgbe_set_lan_id_multi_port;

	mac->num_rar_entries	= TXGBE_RAPTOR_RAR_ENTRIES;

	return 0;
}
