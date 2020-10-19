/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include "txgbe_type.h"
#include "txgbe_phy.h"
#include "txgbe_eeprom.h"
#include "txgbe_mng.h"
#include "txgbe_hw.h"

#define TXGBE_RAPTOR_RAR_ENTRIES   128

/**
 *  txgbe_init_hw - Generic hardware initialization
 *  @hw: pointer to hardware structure
 *
 *  Initialize the hardware by resetting the hardware, filling the bus info
 *  structure and media type, clears all on chip counters, initializes receive
 *  address registers, multicast table, VLAN filter table, calls routine to set
 *  up link and flow control settings, and leaves transmit and receive units
 *  disabled and uninitialized
 **/
s32 txgbe_init_hw(struct txgbe_hw *hw)
{
	s32 status;

	DEBUGFUNC("txgbe_init_hw");

	/* Reset the hardware */
	status = hw->mac.reset_hw(hw);
	if (status == 0 || status == TXGBE_ERR_SFP_NOT_PRESENT) {
		/* Start the HW */
		status = hw->mac.start_hw(hw);
	}

	if (status != 0)
		DEBUGOUT("Failed to initialize HW, STATUS = %d\n", status);

	return status;
}


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
 *  txgbe_validate_mac_addr - Validate MAC address
 *  @mac_addr: pointer to MAC address.
 *
 *  Tests a MAC address to ensure it is a valid Individual Address.
 **/
s32 txgbe_validate_mac_addr(u8 *mac_addr)
{
	s32 status = 0;

	DEBUGFUNC("txgbe_validate_mac_addr");

	/* Make sure it is not a multicast address */
	if (TXGBE_IS_MULTICAST(mac_addr)) {
		status = TXGBE_ERR_INVALID_MAC_ADDR;
	/* Not a broadcast address */
	} else if (TXGBE_IS_BROADCAST(mac_addr)) {
		status = TXGBE_ERR_INVALID_MAC_ADDR;
	/* Reject the zero address */
	} else if (mac_addr[0] == 0 && mac_addr[1] == 0 && mac_addr[2] == 0 &&
		   mac_addr[3] == 0 && mac_addr[4] == 0 && mac_addr[5] == 0) {
		status = TXGBE_ERR_INVALID_MAC_ADDR;
	}
	return status;
}

/**
 * txgbe_clear_tx_pending - Clear pending TX work from the PCIe fifo
 * @hw: pointer to the hardware structure
 *
 * The MACs can experience issues if TX work is still pending
 * when a reset occurs.  This function prevents this by flushing the PCIe
 * buffers on the system.
 **/
void txgbe_clear_tx_pending(struct txgbe_hw *hw)
{
	u32 hlreg0, i, poll;

	/*
	 * If double reset is not requested then all transactions should
	 * already be clear and as such there is no work to do
	 */
	if (!(hw->mac.flags & TXGBE_FLAGS_DOUBLE_RESET_REQUIRED))
		return;

	hlreg0 = rd32(hw, TXGBE_PSRCTL);
	wr32(hw, TXGBE_PSRCTL, hlreg0 | TXGBE_PSRCTL_LBENA);

	/* Wait for a last completion before clearing buffers */
	txgbe_flush(hw);
	msec_delay(3);

	/*
	 * Before proceeding, make sure that the PCIe block does not have
	 * transactions pending.
	 */
	poll = (800 * 11) / 10;
	for (i = 0; i < poll; i++)
		usec_delay(100);

	/* Flush all writes and allow 20usec for all transactions to clear */
	txgbe_flush(hw);
	usec_delay(20);

	/* restore previous register values */
	wr32(hw, TXGBE_PSRCTL, hlreg0);
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

void txgbe_init_mac_link_ops(struct txgbe_hw *hw)
{
	struct txgbe_mac_info *mac = &hw->mac;

	DEBUGFUNC("txgbe_init_mac_link_ops");
	RTE_SET_USED(mac);
}

/**
 *  txgbe_init_phy_raptor - PHY/SFP specific init
 *  @hw: pointer to hardware structure
 *
 *  Initialize any function pointers that were not able to be
 *  set during init_shared_code because the PHY/SFP type was
 *  not known.  Perform the SFP init if necessary.
 *
 **/
s32 txgbe_init_phy_raptor(struct txgbe_hw *hw)
{
	struct txgbe_phy_info *phy = &hw->phy;
	s32 err = 0;

	DEBUGFUNC("txgbe_init_phy_raptor");

	if (hw->device_id == TXGBE_DEV_ID_RAPTOR_QSFP) {
		/* Store flag indicating I2C bus access control unit. */
		hw->phy.qsfp_shared_i2c_bus = TRUE;

		/* Initialize access to QSFP+ I2C bus */
		txgbe_flush(hw);
	}

	/* Identify the PHY or SFP module */
	err = phy->identify(hw);
	if (err == TXGBE_ERR_SFP_NOT_SUPPORTED)
		goto init_phy_ops_out;

	/* Setup function pointers based on detected SFP module and speeds */
	txgbe_init_mac_link_ops(hw);

init_phy_ops_out:
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
	struct txgbe_phy_info *phy = &hw->phy;
	struct txgbe_rom_info *rom = &hw->rom;

	DEBUGFUNC("txgbe_init_ops_pf");

	/* BUS */
	bus->set_lan_id = txgbe_set_lan_id_multi_port;

	/* PHY */
	phy->identify = txgbe_identify_phy;
	phy->init = txgbe_init_phy_raptor;

	/* MAC */
	mac->init_hw = txgbe_init_hw;
	mac->reset_hw = txgbe_reset_hw;
	mac->num_rar_entries	= TXGBE_RAPTOR_RAR_ENTRIES;

	/* EEPROM */
	rom->init_params = txgbe_init_eeprom_params;
	rom->read16 = txgbe_ee_read16;
	rom->readw_buffer = txgbe_ee_readw_buffer;
	rom->readw_sw = txgbe_ee_readw_sw;
	rom->read32 = txgbe_ee_read32;
	rom->write16 = txgbe_ee_write16;
	rom->writew_buffer = txgbe_ee_writew_buffer;
	rom->writew_sw = txgbe_ee_writew_sw;
	rom->write32 = txgbe_ee_write32;
	rom->validate_checksum = txgbe_validate_eeprom_checksum;
	rom->update_checksum = txgbe_update_eeprom_checksum;
	rom->calc_checksum = txgbe_calc_eeprom_checksum;

	return 0;
}

static int
txgbe_check_flash_load(struct txgbe_hw *hw, u32 check_bit)
{
	u32 reg = 0;
	u32 i;
	int err = 0;
	/* if there's flash existing */
	if (!(rd32(hw, TXGBE_SPISTAT) & TXGBE_SPISTAT_BPFLASH)) {
		/* wait hw load flash done */
		for (i = 0; i < 10; i++) {
			reg = rd32(hw, TXGBE_ILDRSTAT);
			if (!(reg & check_bit)) {
				/* done */
				break;
			}
			msleep(100);
		}
		if (i == 10)
			err = TXGBE_ERR_FLASH_LOADING_FAILED;
	}
	return err;
}

/**
 *  txgbe_reset_hw - Perform hardware reset
 *  @hw: pointer to hardware structure
 *
 *  Resets the hardware by resetting the transmit and receive units, masks
 *  and clears all interrupts, perform a PHY reset, and perform a link (MAC)
 *  reset.
 **/
s32 txgbe_reset_hw(struct txgbe_hw *hw)
{
	s32 status;
	u32 autoc;

	DEBUGFUNC("txgbe_reset_hw");

	/* Call adapter stop to disable tx/rx and clear interrupts */
	status = hw->mac.stop_hw(hw);
	if (status != 0)
		return status;

	/* flush pending Tx transactions */
	txgbe_clear_tx_pending(hw);

	/* Identify PHY and related function pointers */
	status = hw->phy.init(hw);
	if (status == TXGBE_ERR_SFP_NOT_SUPPORTED)
		return status;

	/* Setup SFP module if there is one present. */
	if (hw->phy.sfp_setup_needed) {
		status = hw->mac.setup_sfp(hw);
		hw->phy.sfp_setup_needed = false;
	}
	if (status == TXGBE_ERR_SFP_NOT_SUPPORTED)
		return status;

	/* Reset PHY */
	if (!hw->phy.reset_disable)
		hw->phy.reset(hw);

	/* remember AUTOC from before we reset */
	autoc = hw->mac.autoc_read(hw);

mac_reset_top:
	/*
	 * Issue global reset to the MAC.  Needs to be SW reset if link is up.
	 * If link reset is used when link is up, it might reset the PHY when
	 * mng is using it.  If link is down or the flag to force full link
	 * reset is set, then perform link reset.
	 */
	if (txgbe_mng_present(hw)) {
		txgbe_hic_reset(hw);
	} else {
		wr32(hw, TXGBE_RST, TXGBE_RST_LAN(hw->bus.lan_id));
		txgbe_flush(hw);
	}
	usec_delay(10);

	if (hw->bus.lan_id == 0) {
		status = txgbe_check_flash_load(hw,
				TXGBE_ILDRSTAT_SWRST_LAN0);
	} else {
		status = txgbe_check_flash_load(hw,
				TXGBE_ILDRSTAT_SWRST_LAN1);
	}
	if (status != 0)
		return status;

	msec_delay(50);

	/*
	 * Double resets are required for recovery from certain error
	 * conditions.  Between resets, it is necessary to stall to
	 * allow time for any pending HW events to complete.
	 */
	if (hw->mac.flags & TXGBE_FLAGS_DOUBLE_RESET_REQUIRED) {
		hw->mac.flags &= ~TXGBE_FLAGS_DOUBLE_RESET_REQUIRED;
		goto mac_reset_top;
	}

	/*
	 * Store the original AUTOC/AUTOC2 values if they have not been
	 * stored off yet.  Otherwise restore the stored original
	 * values since the reset operation sets back to defaults.
	 */
	if (!hw->mac.orig_link_settings_stored) {
		hw->mac.orig_autoc = hw->mac.autoc_read(hw);
		hw->mac.autoc_write(hw, hw->mac.orig_autoc);
		hw->mac.orig_link_settings_stored = true;
	} else {
		hw->mac.orig_autoc = autoc;
	}

	/* Store the permanent mac address */
	hw->mac.get_mac_addr(hw, hw->mac.perm_addr);

	/*
	 * Store MAC address from RAR0, clear receive address registers, and
	 * clear the multicast table.  Also reset num_rar_entries to 128,
	 * since we modify this value when programming the SAN MAC address.
	 */
	hw->mac.num_rar_entries = 128;
	hw->mac.init_rx_addrs(hw);

	/* Store the permanent SAN mac address */
	hw->mac.get_san_mac_addr(hw, hw->mac.san_addr);

	/* Add the SAN MAC address to the RAR only if it's a valid address */
	if (txgbe_validate_mac_addr(hw->mac.san_addr) == 0) {
		/* Save the SAN MAC RAR index */
		hw->mac.san_mac_rar_index = hw->mac.num_rar_entries - 1;

		hw->mac.set_rar(hw, hw->mac.san_mac_rar_index,
				    hw->mac.san_addr, 0, true);

		/* clear VMDq pool/queue selection for this RAR */
		hw->mac.clear_vmdq(hw, hw->mac.san_mac_rar_index,
				       BIT_MASK32);

		/* Reserve the last RAR for the SAN MAC address */
		hw->mac.num_rar_entries--;
	}

	/* Store the alternative WWNN/WWPN prefix */
	hw->mac.get_wwn_prefix(hw, &hw->mac.wwnn_prefix,
				   &hw->mac.wwpn_prefix);

	return status;
}

