/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "ngbe_type.h"
#include "ngbe_eeprom.h"
#include "ngbe_mng.h"
#include "ngbe_hw.h"

/**
 *  ngbe_init_hw - Generic hardware initialization
 *  @hw: pointer to hardware structure
 *
 *  Initialize the hardware by resetting the hardware, filling the bus info
 *  structure and media type, clears all on chip counters, initializes receive
 *  address registers, multicast table, VLAN filter table, calls routine to set
 *  up link and flow control settings, and leaves transmit and receive units
 *  disabled and uninitialized
 **/
s32 ngbe_init_hw(struct ngbe_hw *hw)
{
	s32 status;

	DEBUGFUNC("ngbe_init_hw");

	/* Reset the hardware */
	status = hw->mac.reset_hw(hw);

	if (status != 0)
		DEBUGOUT("Failed to initialize HW, STATUS = %d\n", status);

	return status;
}

static void
ngbe_reset_misc_em(struct ngbe_hw *hw)
{
	int i;

	wr32(hw, NGBE_ISBADDRL, hw->isb_dma & 0xFFFFFFFF);
	wr32(hw, NGBE_ISBADDRH, hw->isb_dma >> 32);

	/* receive packets that size > 2048 */
	wr32m(hw, NGBE_MACRXCFG,
		NGBE_MACRXCFG_JUMBO, NGBE_MACRXCFG_JUMBO);

	wr32m(hw, NGBE_FRMSZ, NGBE_FRMSZ_MAX_MASK,
		NGBE_FRMSZ_MAX(NGBE_FRAME_SIZE_DFT));

	/* clear counters on read */
	wr32m(hw, NGBE_MACCNTCTL,
		NGBE_MACCNTCTL_RC, NGBE_MACCNTCTL_RC);

	wr32m(hw, NGBE_RXFCCFG,
		NGBE_RXFCCFG_FC, NGBE_RXFCCFG_FC);
	wr32m(hw, NGBE_TXFCCFG,
		NGBE_TXFCCFG_FC, NGBE_TXFCCFG_FC);

	wr32m(hw, NGBE_MACRXFLT,
		NGBE_MACRXFLT_PROMISC, NGBE_MACRXFLT_PROMISC);

	wr32m(hw, NGBE_RSTSTAT,
		NGBE_RSTSTAT_TMRINIT_MASK, NGBE_RSTSTAT_TMRINIT(30));

	/* errata 4: initialize mng flex tbl and wakeup flex tbl*/
	wr32(hw, NGBE_MNGFLEXSEL, 0);
	for (i = 0; i < 16; i++) {
		wr32(hw, NGBE_MNGFLEXDWL(i), 0);
		wr32(hw, NGBE_MNGFLEXDWH(i), 0);
		wr32(hw, NGBE_MNGFLEXMSK(i), 0);
	}
	wr32(hw, NGBE_LANFLEXSEL, 0);
	for (i = 0; i < 16; i++) {
		wr32(hw, NGBE_LANFLEXDWL(i), 0);
		wr32(hw, NGBE_LANFLEXDWH(i), 0);
		wr32(hw, NGBE_LANFLEXMSK(i), 0);
	}

	/* set pause frame dst mac addr */
	wr32(hw, NGBE_RXPBPFCDMACL, 0xC2000001);
	wr32(hw, NGBE_RXPBPFCDMACH, 0x0180);

	wr32(hw, NGBE_MDIOMODE, 0xF);

	wr32m(hw, NGBE_GPIE, NGBE_GPIE_MSIX, NGBE_GPIE_MSIX);

	if ((hw->sub_system_id & NGBE_OEM_MASK) == NGBE_LY_M88E1512_SFP ||
		(hw->sub_system_id & NGBE_OEM_MASK) == NGBE_LY_YT8521S_SFP) {
		/* gpio0 is used to power on/off control*/
		wr32(hw, NGBE_GPIODIR, NGBE_GPIODIR_DDR(1));
		wr32(hw, NGBE_GPIODATA, NGBE_GPIOBIT_0);
	}

	hw->mac.init_thermal_sensor_thresh(hw);

	/* enable mac transmitter */
	wr32m(hw, NGBE_MACTXCFG, NGBE_MACTXCFG_TE, NGBE_MACTXCFG_TE);

	/* sellect GMII */
	wr32m(hw, NGBE_MACTXCFG,
		NGBE_MACTXCFG_SPEED_MASK, NGBE_MACTXCFG_SPEED_1G);

	for (i = 0; i < 4; i++)
		wr32m(hw, NGBE_IVAR(i), 0x80808080, 0);
}

/**
 *  ngbe_reset_hw_em - Perform hardware reset
 *  @hw: pointer to hardware structure
 *
 *  Resets the hardware by resetting the transmit and receive units, masks
 *  and clears all interrupts, perform a PHY reset, and perform a link (MAC)
 *  reset.
 **/
s32 ngbe_reset_hw_em(struct ngbe_hw *hw)
{
	s32 status;

	DEBUGFUNC("ngbe_reset_hw_em");

	/* Call adapter stop to disable tx/rx and clear interrupts */
	status = hw->mac.stop_hw(hw);
	if (status != 0)
		return status;

	wr32(hw, NGBE_RST, NGBE_RST_LAN(hw->bus.lan_id));
	ngbe_flush(hw);
	msec_delay(50);

	ngbe_reset_misc_em(hw);

	msec_delay(50);

	return status;
}

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
 *  ngbe_stop_hw - Generic stop Tx/Rx units
 *  @hw: pointer to hardware structure
 *
 *  Sets the adapter_stopped flag within ngbe_hw struct. Clears interrupts,
 *  disables transmit and receive units. The adapter_stopped flag is used by
 *  the shared code and drivers to determine if the adapter is in a stopped
 *  state and should not touch the hardware.
 **/
s32 ngbe_stop_hw(struct ngbe_hw *hw)
{
	u32 reg_val;
	u16 i;

	DEBUGFUNC("ngbe_stop_hw");

	/*
	 * Set the adapter_stopped flag so other driver functions stop touching
	 * the hardware
	 */
	hw->adapter_stopped = true;

	/* Disable the receive unit */
	ngbe_disable_rx(hw);

	/* Clear interrupt mask to stop interrupts from being generated */
	wr32(hw, NGBE_IENMISC, 0);
	wr32(hw, NGBE_IMS(0), NGBE_IMS_MASK);

	/* Clear any pending interrupts, flush previous writes */
	wr32(hw, NGBE_ICRMISC, NGBE_ICRMISC_MASK);
	wr32(hw, NGBE_ICR(0), NGBE_ICR_MASK);

	/* Disable the transmit unit.  Each queue must be disabled. */
	for (i = 0; i < hw->mac.max_tx_queues; i++)
		wr32(hw, NGBE_TXCFG(i), NGBE_TXCFG_FLUSH);

	/* Disable the receive unit by stopping each queue */
	for (i = 0; i < hw->mac.max_rx_queues; i++) {
		reg_val = rd32(hw, NGBE_RXCFG(i));
		reg_val &= ~NGBE_RXCFG_ENA;
		wr32(hw, NGBE_RXCFG(i), reg_val);
	}

	/* flush all queues disables */
	ngbe_flush(hw);
	msec_delay(2);

	return 0;
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
 *  ngbe_init_thermal_sensor_thresh - Inits thermal sensor thresholds
 *  @hw: pointer to hardware structure
 *
 *  Inits the thermal sensor thresholds according to the NVM map
 *  and save off the threshold and location values into mac.thermal_sensor_data
 **/
s32 ngbe_init_thermal_sensor_thresh(struct ngbe_hw *hw)
{
	struct ngbe_thermal_sensor_data *data = &hw->mac.thermal_sensor_data;

	DEBUGFUNC("ngbe_init_thermal_sensor_thresh");

	memset(data, 0, sizeof(struct ngbe_thermal_sensor_data));

	if (hw->bus.lan_id != 0)
		return NGBE_NOT_IMPLEMENTED;

	wr32(hw, NGBE_TSINTR,
		NGBE_TSINTR_AEN | NGBE_TSINTR_DEN);
	wr32(hw, NGBE_TSEN, NGBE_TSEN_ENA);


	data->sensor[0].alarm_thresh = 115;
	wr32(hw, NGBE_TSATHRE, 0x344);
	data->sensor[0].dalarm_thresh = 110;
	wr32(hw, NGBE_TSDTHRE, 0x330);

	return 0;
}

void ngbe_disable_rx(struct ngbe_hw *hw)
{
	u32 pfdtxgswc;

	pfdtxgswc = rd32(hw, NGBE_PSRCTL);
	if (pfdtxgswc & NGBE_PSRCTL_LBENA) {
		pfdtxgswc &= ~NGBE_PSRCTL_LBENA;
		wr32(hw, NGBE_PSRCTL, pfdtxgswc);
		hw->mac.set_lben = true;
	} else {
		hw->mac.set_lben = false;
	}

	wr32m(hw, NGBE_PBRXCTL, NGBE_PBRXCTL_ENA, 0);
	wr32m(hw, NGBE_MACRXCFG, NGBE_MACRXCFG_ENA, 0);
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
	mac->init_hw = ngbe_init_hw;
	mac->reset_hw = ngbe_reset_hw_em;
	mac->stop_hw = ngbe_stop_hw;
	mac->acquire_swfw_sync = ngbe_acquire_swfw_sync;
	mac->release_swfw_sync = ngbe_release_swfw_sync;

	/* Manageability interface */
	mac->init_thermal_sensor_thresh = ngbe_init_thermal_sensor_thresh;

	/* EEPROM */
	rom->init_params = ngbe_init_eeprom_params;
	rom->validate_checksum = ngbe_validate_eeprom_checksum_em;

	mac->max_rx_queues	= NGBE_EM_MAX_RX_QUEUES;
	mac->max_tx_queues	= NGBE_EM_MAX_TX_QUEUES;

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

