/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "txgbe_type.h"
#include "txgbe_mbx.h"
#include "txgbe_phy.h"
#include "txgbe_dcb.h"
#include "txgbe_vf.h"
#include "txgbe_eeprom.h"
#include "txgbe_mng.h"
#include "txgbe_hw.h"
#include "txgbe_aml.h"
#include "txgbe_aml40.h"

#define TXGBE_RAPTOR_MAX_TX_QUEUES 128
#define TXGBE_RAPTOR_MAX_RX_QUEUES 128
#define TXGBE_RAPTOR_RAR_ENTRIES   128
#define TXGBE_RAPTOR_MC_TBL_SIZE   128
#define TXGBE_RAPTOR_VFT_TBL_SIZE  128
#define TXGBE_RAPTOR_RX_PB_SIZE	  512 /*KB*/
#define TXGBE_AML_RX_PB_SIZE	  768

static s32 txgbe_setup_copper_link_raptor(struct txgbe_hw *hw,
					 u32 speed,
					 bool autoneg_wait_to_complete);

static s32 txgbe_mta_vector(struct txgbe_hw *hw, u8 *mc_addr);
static s32 txgbe_get_san_mac_addr_offset(struct txgbe_hw *hw,
					 u16 *san_mac_offset);

static s32 txgbe_is_lldp(struct txgbe_hw *hw)
{
	u32 tmp = 0, lldp_flash_data = 0, i;
	s32 err = 0;

	if ((hw->fw_version & TXGBE_FW_MASK) >= TXGBE_FW_GET_LLDP) {
		err = txgbe_hic_get_lldp(hw);
		if (err == 0)
			return 0;
	}

	for (i = 0; i < 1024; i++) {
		err = txgbe_flash_read_dword(hw, TXGBE_LLDP_REG + i * 4, &tmp);
		if (err)
			return err;

		if (tmp == BIT_MASK32)
			break;
		lldp_flash_data = tmp;
	}

	if (lldp_flash_data & MS(hw->bus.lan_id, 1))
		hw->lldp_enabled = true;
	else
		hw->lldp_enabled = false;

	return 0;
}

static void txgbe_disable_lldp(struct txgbe_hw *hw)
{
	s32 status = 0;

	if ((hw->fw_version & TXGBE_FW_MASK) < TXGBE_FW_SUPPORT_LLDP)
		return;

	status = txgbe_is_lldp(hw);
	if (status) {
		PMD_INIT_LOG(INFO, "Can not get LLDP status.");
	} else if (hw->lldp_enabled) {
		status = txgbe_hic_set_lldp(hw, false);
		if (!status)
			PMD_INIT_LOG(INFO,
				"LLDP detected on port %d, turn it off by default.",
				hw->port_id);
		else
			PMD_INIT_LOG(INFO, "Can not set LLDP status.");
	}
}

/**
 * txgbe_device_supports_autoneg_fc - Check if device supports autonegotiation
 * of flow control
 * @hw: pointer to hardware structure
 *
 * This function returns true if the device supports flow control
 * autonegotiation, and false if it does not.
 *
 **/
bool txgbe_device_supports_autoneg_fc(struct txgbe_hw *hw)
{
	bool supported = false;
	u32 speed;
	bool link_up;

	switch (hw->phy.media_type) {
	case txgbe_media_type_fiber_qsfp:
	case txgbe_media_type_fiber:
		hw->mac.check_link(hw, &speed, &link_up, false);
		/* if link is down, assume supported */
		if (link_up)
			supported = speed == TXGBE_LINK_SPEED_1GB_FULL ?
			true : false;
		else
			supported = true;

		break;
	case txgbe_media_type_backplane:
		supported = true;
		break;
	case txgbe_media_type_copper:
		/* only some copper devices support flow control autoneg */
		switch (hw->subsystem_device_id & 0xFF) {
		case TXGBE_DEV_ID_XAUI:
		case TXGBE_DEV_ID_SGMII:
			supported = true;
			break;
		default:
			supported = false;
		}
	default:
		break;
	}

	if (!supported)
		DEBUGOUT("Device %x does not support flow control autoneg",
			      hw->device_id);
	return supported;
}

/**
 *  txgbe_setup_fc - Set up flow control
 *  @hw: pointer to hardware structure
 *
 *  Called at init time to set up flow control.
 **/
s32 txgbe_setup_fc(struct txgbe_hw *hw)
{
	s32 err = 0;
	u32 reg = 0;
	u16 reg_cu = 0;
	u32 value = 0;
	u64 reg_bp = 0;

	/* Validate the requested mode */
	if (hw->fc.strict_ieee && hw->fc.requested_mode == txgbe_fc_rx_pause) {
		DEBUGOUT("txgbe_fc_rx_pause not valid in strict IEEE mode");
		err = TXGBE_ERR_INVALID_LINK_SETTINGS;
		goto out;
	}

	/*
	 * 10gig parts do not have a word in the EEPROM to determine the
	 * default flow control setting, so we explicitly set it to full.
	 */
	if (hw->fc.requested_mode == txgbe_fc_default)
		hw->fc.requested_mode = txgbe_fc_full;

	/*
	 * The possible values of fc.requested_mode are:
	 * 0: Flow control is completely disabled
	 * 1: Rx flow control is enabled (we can receive pause frames,
	 *    but not send pause frames).
	 * 2: Tx flow control is enabled (we can send pause frames but
	 *    we do not support receiving pause frames).
	 * 3: Both Rx and Tx flow control (symmetric) are enabled.
	 * other: Invalid.
	 */
	switch (hw->fc.requested_mode) {
	case txgbe_fc_none:
		/* Flow control completely disabled by software override. */
		break;
	case txgbe_fc_tx_pause:
		/*
		 * Tx Flow control is enabled, and Rx Flow control is
		 * disabled by software override.
		 */
		reg |= SR_MII_MMD_AN_ADV_PAUSE_ASM;
		reg_bp |= SR_AN_MMD_ADV_REG1_PAUSE_ASM;
		break;
	case txgbe_fc_rx_pause:
		/*
		 * Rx Flow control is enabled and Tx Flow control is
		 * disabled by software override. Since there really
		 * isn't a way to advertise that we are capable of RX
		 * Pause ONLY, we will advertise that we support both
		 * symmetric and asymmetric Rx PAUSE, as such we fall
		 * through to the fc_full statement.  Later, we will
		 * disable the adapter's ability to send PAUSE frames.
		 */
	case txgbe_fc_full:
		/* Flow control (both Rx and Tx) is enabled by SW override. */
		reg |= SR_MII_MMD_AN_ADV_PAUSE_SYM |
			SR_MII_MMD_AN_ADV_PAUSE_ASM;
		reg_bp |= SR_AN_MMD_ADV_REG1_PAUSE_SYM |
			SR_AN_MMD_ADV_REG1_PAUSE_ASM;
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly");
		err = TXGBE_ERR_CONFIG;
		goto out;
	}

	/*
	 * Enable auto-negotiation between the MAC & PHY;
	 * the MAC will advertise clause 37 flow control.
	 */
	value = rd32_epcs(hw, SR_MII_MMD_AN_ADV);
	value = (value & ~(SR_MII_MMD_AN_ADV_PAUSE_ASM |
		SR_MII_MMD_AN_ADV_PAUSE_SYM)) | reg;
	wr32_epcs(hw, SR_MII_MMD_AN_ADV, value);

	/*
	 * AUTOC restart handles negotiation of 1G and 10G on backplane
	 * and copper. There is no need to set the PCS1GCTL register.
	 *
	 */
	if (hw->phy.media_type == txgbe_media_type_backplane) {
		value = rd32_epcs(hw, SR_AN_MMD_ADV_REG1);
		value = (value & ~(SR_AN_MMD_ADV_REG1_PAUSE_ASM |
			SR_AN_MMD_ADV_REG1_PAUSE_SYM)) |
			reg_bp;
		wr32_epcs(hw, SR_AN_MMD_ADV_REG1, value);
	} else if ((hw->phy.media_type == txgbe_media_type_copper) &&
		    (txgbe_device_supports_autoneg_fc(hw))) {
		hw->phy.write_reg(hw, TXGBE_MD_AUTO_NEG_ADVT,
				      TXGBE_MD_DEV_AUTO_NEG, reg_cu);
	}

	/*
	 * Reconfig mac ctrl frame fwd rule to make sure it still
	 * working after port stop/start.
	 */
	wr32m(hw, TXGBE_MACRXFLT, TXGBE_MACRXFLT_CTL_MASK,
	      (hw->fc.mac_ctrl_frame_fwd ?
	       TXGBE_MACRXFLT_CTL_NOPS : TXGBE_MACRXFLT_CTL_DROP));
	txgbe_flush(hw);

	DEBUGOUT("Set up FC; reg = 0x%08X", reg);
out:
	return err;
}

/**
 *  txgbe_start_hw - Prepare hardware for Tx/Rx
 *  @hw: pointer to hardware structure
 *
 *  Starts the hardware by filling the bus info structure and media type, clears
 *  all on chip counters, initializes receive address registers, multicast
 *  table, VLAN filter table, calls routine to set up link and flow control
 *  settings, and leaves transmit and receive units disabled and uninitialized
 **/
s32 txgbe_start_hw(struct txgbe_hw *hw)
{
	s32 err;
	u16 device_caps;

	/* Set the media type */
	hw->phy.media_type = hw->phy.get_media_type(hw);

	/* Clear the VLAN filter table */
	hw->mac.clear_vfta(hw);

	/* Clear statistics registers */
	hw->mac.clear_hw_cntrs(hw);

	/* Setup flow control */
	err = txgbe_setup_fc(hw);
	if (err != 0 && err != TXGBE_NOT_IMPLEMENTED) {
		DEBUGOUT("Flow control setup failed, returning %d", err);
		return err;
	}

	/* Cache bit indicating need for crosstalk fix */
	switch (hw->mac.type) {
	case txgbe_mac_sp:
	case txgbe_mac_aml:
	case txgbe_mac_aml40:
		hw->mac.get_device_caps(hw, &device_caps);
		if (device_caps & TXGBE_DEVICE_CAPS_NO_CROSSTALK_WR)
			hw->need_crosstalk_fix = false;
		else
			hw->need_crosstalk_fix = true;
		break;
	default:
		hw->need_crosstalk_fix = false;
		break;
	}

	/* Clear adapter stopped flag */
	hw->adapter_stopped = false;

	return 0;
}

/**
 *  txgbe_start_hw_gen2 - Init sequence for common device family
 *  @hw: pointer to hw structure
 *
 * Performs the init sequence common to the second generation
 * of 10 GbE devices.
 **/
s32 txgbe_start_hw_gen2(struct txgbe_hw *hw)
{
	u32 i;

	/* Clear the rate limiters */
	for (i = 0; i < hw->mac.max_tx_queues; i++) {
		wr32(hw, TXGBE_ARBPOOLIDX, i);
		wr32(hw, TXGBE_ARBTXRATE, 0);
	}
	txgbe_flush(hw);

	/* We need to run link autotry after the driver loads */
	hw->mac.autotry_restart = true;

	return 0;
}

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

	/* Get firmware version */
	hw->phy.get_fw_version(hw, &hw->fw_version);

	txgbe_disable_lldp(hw);

	/* Reset the hardware */
	status = hw->mac.reset_hw(hw);
	if (status == 0 || status == TXGBE_ERR_SFP_NOT_PRESENT) {
		/* Start the HW */
		status = hw->mac.start_hw(hw);
	}

	if (status != 0)
		DEBUGOUT("Failed to initialize HW, STATUS = %d", status);

	return status;
}

/**
 *  txgbe_clear_hw_cntrs - Generic clear hardware counters
 *  @hw: pointer to hardware structure
 *
 *  Clears all hardware statistics counters by reading them from the hardware
 *  Statistics counters are clear on read.
 **/
s32 txgbe_clear_hw_cntrs(struct txgbe_hw *hw)
{
	u16 i = 0;

	/* QP Stats */
	/* don't write clear queue stats */
	for (i = 0; i < TXGBE_MAX_QP; i++) {
		hw->qp_last[i].rx_qp_packets = 0;
		hw->qp_last[i].tx_qp_packets = 0;
		hw->qp_last[i].rx_qp_bytes = 0;
		hw->qp_last[i].tx_qp_bytes = 0;
		hw->qp_last[i].rx_qp_mc_packets = 0;
	}

	/* PB Stats */
	for (i = 0; i < TXGBE_MAX_UP; i++) {
		rd32(hw, TXGBE_PBRXUPXON(i));
		rd32(hw, TXGBE_PBRXUPXOFF(i));
		rd32(hw, TXGBE_PBTXUPXON(i));
		rd32(hw, TXGBE_PBTXUPXOFF(i));
		rd32(hw, TXGBE_PBTXUPOFF(i));

		rd32(hw, TXGBE_PBRXMISS(i));
	}
	rd32(hw, TXGBE_PBRXLNKXON);
	rd32(hw, TXGBE_PBRXLNKXOFF);
	rd32(hw, TXGBE_PBTXLNKXON);
	rd32(hw, TXGBE_PBTXLNKXOFF);

	/* DMA Stats */
	rd32(hw, TXGBE_DMARXPKT);
	rd32(hw, TXGBE_DMATXPKT);

	rd64(hw, TXGBE_DMARXOCTL);
	rd64(hw, TXGBE_DMATXOCTL);

	/* MAC Stats */
	rd64(hw, TXGBE_MACRXERRCRCL);
	rd64(hw, TXGBE_MACRXMPKTL);
	rd64(hw, TXGBE_MACTXMPKTL);

	rd64(hw, TXGBE_MACRXPKTL);
	rd64(hw, TXGBE_MACTXPKTL);
	rd64(hw, TXGBE_MACRXGBOCTL);

	rd64(hw, TXGBE_MACRXOCTL);
	rd32(hw, TXGBE_MACTXOCTL);

	rd64(hw, TXGBE_MACRX1TO64L);
	rd64(hw, TXGBE_MACRX65TO127L);
	rd64(hw, TXGBE_MACRX128TO255L);
	rd64(hw, TXGBE_MACRX256TO511L);
	rd64(hw, TXGBE_MACRX512TO1023L);
	rd64(hw, TXGBE_MACRX1024TOMAXL);
	rd64(hw, TXGBE_MACTX1TO64L);
	rd64(hw, TXGBE_MACTX65TO127L);
	rd64(hw, TXGBE_MACTX128TO255L);
	rd64(hw, TXGBE_MACTX256TO511L);
	rd64(hw, TXGBE_MACTX512TO1023L);
	rd64(hw, TXGBE_MACTX1024TOMAXL);

	rd64(hw, TXGBE_MACRXERRLENL);
	rd32(hw, TXGBE_MACRXOVERSIZE);
	rd32(hw, TXGBE_MACRXJABBER);

	/* FCoE Stats */
	rd32(hw, TXGBE_FCOECRC);
	rd32(hw, TXGBE_FCOELAST);
	rd32(hw, TXGBE_FCOERPDC);
	rd32(hw, TXGBE_FCOEPRC);
	rd32(hw, TXGBE_FCOEPTC);
	rd32(hw, TXGBE_FCOEDWRC);
	rd32(hw, TXGBE_FCOEDWTC);

	/* Flow Director Stats */
	rd32(hw, TXGBE_FDIRMATCH);
	rd32(hw, TXGBE_FDIRMISS);
	rd32(hw, TXGBE_FDIRUSED);
	rd32(hw, TXGBE_FDIRUSED);
	rd32(hw, TXGBE_FDIRFAIL);
	rd32(hw, TXGBE_FDIRFAIL);

	/* MACsec Stats */
	rd32(hw, TXGBE_LSECTX_UTPKT);
	rd32(hw, TXGBE_LSECTX_ENCPKT);
	rd32(hw, TXGBE_LSECTX_PROTPKT);
	rd32(hw, TXGBE_LSECTX_ENCOCT);
	rd32(hw, TXGBE_LSECTX_PROTOCT);
	rd32(hw, TXGBE_LSECRX_UTPKT);
	rd32(hw, TXGBE_LSECRX_BTPKT);
	rd32(hw, TXGBE_LSECRX_NOSCIPKT);
	rd32(hw, TXGBE_LSECRX_UNSCIPKT);
	rd32(hw, TXGBE_LSECRX_DECOCT);
	rd32(hw, TXGBE_LSECRX_VLDOCT);
	rd32(hw, TXGBE_LSECRX_UNCHKPKT);
	rd32(hw, TXGBE_LSECRX_DLYPKT);
	rd32(hw, TXGBE_LSECRX_LATEPKT);
	for (i = 0; i < 2; i++) {
		rd32(hw, TXGBE_LSECRX_OKPKT(i));
		rd32(hw, TXGBE_LSECRX_INVPKT(i));
		rd32(hw, TXGBE_LSECRX_BADPKT(i));
	}
	rd32(hw, TXGBE_LSECRX_INVSAPKT);
	rd32(hw, TXGBE_LSECRX_BADSAPKT);

	return 0;
}

/**
 *  txgbe_get_mac_addr - Generic get MAC address
 *  @hw: pointer to hardware structure
 *  @mac_addr: Adapter MAC address
 *
 *  Reads the adapter's MAC address from first Receive Address Register (RAR0)
 *  A reset of the adapter must be performed prior to calling this function
 *  in order for the MAC address to have been loaded from the EEPROM into RAR0
 **/
s32 txgbe_get_mac_addr(struct txgbe_hw *hw, u8 *mac_addr)
{
	u32 rar_high;
	u32 rar_low;
	u16 i;

	wr32(hw, TXGBE_ETHADDRIDX, 0);
	rar_high = rd32(hw, TXGBE_ETHADDRH);
	rar_low = rd32(hw, TXGBE_ETHADDRL);

	for (i = 0; i < 2; i++)
		mac_addr[i] = (u8)(rar_high >> (1 - i) * 8);

	for (i = 0; i < 4; i++)
		mac_addr[i + 2] = (u8)(rar_low >> (3 - i) * 8);

	return 0;
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
 *  txgbe_stop_hw - Generic stop Tx/Rx units
 *  @hw: pointer to hardware structure
 *
 *  Sets the adapter_stopped flag within txgbe_hw struct. Clears interrupts,
 *  disables transmit and receive units. The adapter_stopped flag is used by
 *  the shared code and drivers to determine if the adapter is in a stopped
 *  state and should not touch the hardware.
 **/
s32 txgbe_stop_hw(struct txgbe_hw *hw)
{
	s32 status = 0;
	u16 i;

	/*
	 * Set the adapter_stopped flag so other driver functions stop touching
	 * the hardware
	 */
	hw->adapter_stopped = true;

	/* Disable the receive unit */
	txgbe_disable_rx(hw);

	/* Clear interrupt mask to stop interrupts from being generated */
	wr32(hw, TXGBE_IENMISC, 0);
	wr32(hw, TXGBE_IMS(0), TXGBE_IMS_MASK);
	wr32(hw, TXGBE_IMS(1), TXGBE_IMS_MASK);

	/* Clear any pending interrupts, flush previous writes */
	wr32(hw, TXGBE_ICRMISC, TXGBE_ICRMISC_MASK);
	wr32(hw, TXGBE_ICR(0), TXGBE_ICR_MASK);
	wr32(hw, TXGBE_ICR(1), TXGBE_ICR_MASK);

	wr32(hw, TXGBE_BMECTL, 0x3);

	/* Disable the receive unit by stopping each queue */
	for (i = 0; i < hw->mac.max_rx_queues; i++)
		wr32(hw, TXGBE_RXCFG(i), 0);

	/* flush all queues disables */
	txgbe_flush(hw);
	msec_delay(2);

	/* Prevent the PCI-E bus from hanging by disabling PCI-E master
	 * access and verify no pending requests
	 */
	status = txgbe_set_pcie_master(hw, false);
	if (status)
		return status;

	/* Disable the transmit unit.  Each queue must be disabled. */
	for (i = 0; i < hw->mac.max_tx_queues; i++)
		wr32(hw, TXGBE_TXCFG(i), 0);

	/* flush all queues disables */
	txgbe_flush(hw);
	msec_delay(2);

	return 0;
}

/**
 *  txgbe_led_on - Turns on the software controllable LEDs.
 *  @hw: pointer to hardware structure
 *  @index: led number to turn on
 **/
s32 txgbe_led_on(struct txgbe_hw *hw, u32 index)
{
	u32 led_reg = rd32(hw, TXGBE_LEDCTL);

	/* To turn on the LED, set mode to ON. */
	led_reg |= index << TXGBE_LEDCTL_ORD_SHIFT;
	led_reg |= index;
	wr32(hw, TXGBE_LEDCTL, led_reg);
	txgbe_flush(hw);

	return 0;
}

/**
 *  txgbe_led_off - Turns off the software controllable LEDs.
 *  @hw: pointer to hardware structure
 *  @index: led number to turn off
 **/
s32 txgbe_led_off(struct txgbe_hw *hw, u32 index)
{
	u32 led_reg = rd32(hw, TXGBE_LEDCTL);

	/* To turn off the LED, set mode to OFF. */
	led_reg &= ~(index << TXGBE_LEDCTL_ORD_SHIFT);
	led_reg |= index;
	wr32(hw, TXGBE_LEDCTL, led_reg);
	txgbe_flush(hw);

	return 0;
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
 *  txgbe_set_rar - Set Rx address register
 *  @hw: pointer to hardware structure
 *  @index: Receive address register to write
 *  @addr: Address to put into receive address register
 *  @vmdq: VMDq "set" or "pool" index
 *  @enable_addr: set flag that address is active
 *
 *  Puts an ethernet address into a receive address register.
 **/
s32 txgbe_set_rar(struct txgbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
			  u32 enable_addr)
{
	u32 rar_low, rar_high;
	u32 rar_entries = hw->mac.num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (index >= rar_entries) {
		DEBUGOUT("RAR index %d is out of range.", index);
		return TXGBE_ERR_INVALID_ARGUMENT;
	}

	/* setup VMDq pool selection before this RAR gets enabled */
	hw->mac.set_vmdq(hw, index, vmdq);

	/*
	 * HW expects these in little endian so we reverse the byte
	 * order from network order (big endian) to little endian
	 */
	rar_low = TXGBE_ETHADDRL_AD0(addr[5]) |
		  TXGBE_ETHADDRL_AD1(addr[4]) |
		  TXGBE_ETHADDRL_AD2(addr[3]) |
		  TXGBE_ETHADDRL_AD3(addr[2]);
	/*
	 * Some parts put the VMDq setting in the extra RAH bits,
	 * so save everything except the lower 16 bits that hold part
	 * of the address and the address valid bit.
	 */
	rar_high = rd32(hw, TXGBE_ETHADDRH);
	rar_high &= ~TXGBE_ETHADDRH_AD_MASK;
	rar_high |= (TXGBE_ETHADDRH_AD4(addr[1]) |
		     TXGBE_ETHADDRH_AD5(addr[0]));

	rar_high &= ~TXGBE_ETHADDRH_VLD;
	if (enable_addr != 0)
		rar_high |= TXGBE_ETHADDRH_VLD;

	wr32(hw, TXGBE_ETHADDRIDX, index);
	wr32(hw, TXGBE_ETHADDRL, rar_low);
	wr32(hw, TXGBE_ETHADDRH, rar_high);

	return 0;
}

/**
 *  txgbe_clear_rar - Remove Rx address register
 *  @hw: pointer to hardware structure
 *  @index: Receive address register to write
 *
 *  Clears an ethernet address from a receive address register.
 **/
s32 txgbe_clear_rar(struct txgbe_hw *hw, u32 index)
{
	u32 rar_high;
	u32 rar_entries = hw->mac.num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (index >= rar_entries) {
		DEBUGOUT("RAR index %d is out of range.", index);
		return TXGBE_ERR_INVALID_ARGUMENT;
	}

	/*
	 * Some parts put the VMDq setting in the extra RAH bits,
	 * so save everything except the lower 16 bits that hold part
	 * of the address and the address valid bit.
	 */
	wr32(hw, TXGBE_ETHADDRIDX, index);
	rar_high = rd32(hw, TXGBE_ETHADDRH);
	rar_high &= ~(TXGBE_ETHADDRH_AD_MASK | TXGBE_ETHADDRH_VLD);

	wr32(hw, TXGBE_ETHADDRL, 0);
	wr32(hw, TXGBE_ETHADDRH, rar_high);

	/* clear VMDq pool/queue selection for this RAR */
	hw->mac.clear_vmdq(hw, index, BIT_MASK32);

	return 0;
}

/**
 *  txgbe_init_rx_addrs - Initializes receive address filters.
 *  @hw: pointer to hardware structure
 *
 *  Places the MAC address in receive address register 0 and clears the rest
 *  of the receive address registers. Clears the multicast table. Assumes
 *  the receiver is in reset when the routine is called.
 **/
s32 txgbe_init_rx_addrs(struct txgbe_hw *hw)
{
	u32 i;
	u32 psrctl;
	u32 rar_entries = hw->mac.num_rar_entries;

	/*
	 * If the current mac address is valid, assume it is a software override
	 * to the permanent address.
	 * Otherwise, use the permanent address from the eeprom.
	 */
	if (txgbe_validate_mac_addr(hw->mac.addr) ==
	    TXGBE_ERR_INVALID_MAC_ADDR) {
		/* Get the MAC address from the RAR0 for later reference */
		hw->mac.get_mac_addr(hw, hw->mac.addr);

		DEBUGOUT(" Keeping Current RAR0 Addr = "
			  RTE_ETHER_ADDR_PRT_FMT,
			  hw->mac.addr[0], hw->mac.addr[1],
			  hw->mac.addr[2], hw->mac.addr[3],
			  hw->mac.addr[4], hw->mac.addr[5]);
	} else {
		/* Setup the receive address. */
		DEBUGOUT("Overriding MAC Address in RAR[0]");
		DEBUGOUT(" New MAC Addr = "
			  RTE_ETHER_ADDR_PRT_FMT,
			  hw->mac.addr[0], hw->mac.addr[1],
			  hw->mac.addr[2], hw->mac.addr[3],
			  hw->mac.addr[4], hw->mac.addr[5]);

		hw->mac.set_rar(hw, 0, hw->mac.addr, 0, true);
	}

	/* clear VMDq pool/queue selection for RAR 0 */
	hw->mac.clear_vmdq(hw, 0, BIT_MASK32);

	hw->addr_ctrl.overflow_promisc = 0;

	hw->addr_ctrl.rar_used_count = 1;

	/* Zero out the other receive addresses. */
	DEBUGOUT("Clearing RAR[1-%d]", rar_entries - 1);
	for (i = 1; i < rar_entries; i++) {
		wr32(hw, TXGBE_ETHADDRIDX, i);
		wr32(hw, TXGBE_ETHADDRL, 0);
		wr32(hw, TXGBE_ETHADDRH, 0);
	}

	/* Clear the MTA */
	hw->addr_ctrl.mta_in_use = 0;
	psrctl = rd32(hw, TXGBE_PSRCTL);
	psrctl &= ~(TXGBE_PSRCTL_ADHF12_MASK | TXGBE_PSRCTL_MCHFENA);
	psrctl |= TXGBE_PSRCTL_ADHF12(hw->mac.mc_filter_type);
	wr32(hw, TXGBE_PSRCTL, psrctl);

	DEBUGOUT(" Clearing MTA");
	for (i = 0; i < hw->mac.mcft_size; i++)
		wr32(hw, TXGBE_MCADDRTBL(i), 0);

	txgbe_init_uta_tables(hw);

	return 0;
}

/**
 *  txgbe_mta_vector - Determines bit-vector in multicast table to set
 *  @hw: pointer to hardware structure
 *  @mc_addr: the multicast address
 *
 *  Extracts the 12 bits, from a multicast address, to determine which
 *  bit-vector to set in the multicast table. The hardware uses 12 bits, from
 *  incoming rx multicast addresses, to determine the bit-vector to check in
 *  the MTA. Which of the 4 combination, of 12-bits, the hardware uses is set
 *  by the MO field of the PSRCTRL. The MO field is set during initialization
 *  to mc_filter_type.
 **/
static s32 txgbe_mta_vector(struct txgbe_hw *hw, u8 *mc_addr)
{
	u32 vector = 0;

	switch (hw->mac.mc_filter_type) {
	case 0:   /* use bits [47:36] of the address */
		vector = ((mc_addr[4] >> 4) | (((u16)mc_addr[5]) << 4));
		break;
	case 1:   /* use bits [46:35] of the address */
		vector = ((mc_addr[4] >> 3) | (((u16)mc_addr[5]) << 5));
		break;
	case 2:   /* use bits [45:34] of the address */
		vector = ((mc_addr[4] >> 2) | (((u16)mc_addr[5]) << 6));
		break;
	case 3:   /* use bits [43:32] of the address */
		vector = ((mc_addr[4]) | (((u16)mc_addr[5]) << 8));
		break;
	default:  /* Invalid mc_filter_type */
		DEBUGOUT("MC filter type param set incorrectly");
		ASSERT(0);
		break;
	}

	/* vector can only be 12-bits or boundary will be exceeded */
	vector &= 0xFFF;
	return vector;
}

/**
 *  txgbe_set_mta - Set bit-vector in multicast table
 *  @hw: pointer to hardware structure
 *  @mc_addr: Multicast address
 *
 *  Sets the bit-vector in the multicast table.
 **/
void txgbe_set_mta(struct txgbe_hw *hw, u8 *mc_addr)
{
	u32 vector;
	u32 vector_bit;
	u32 vector_reg;

	hw->addr_ctrl.mta_in_use++;

	vector = txgbe_mta_vector(hw, mc_addr);
	DEBUGOUT(" bit-vector = 0x%03X", vector);

	/*
	 * The MTA is a register array of 128 32-bit registers. It is treated
	 * like an array of 4096 bits.  We want to set bit
	 * BitArray[vector_value]. So we figure out what register the bit is
	 * in, read it, OR in the new bit, then write back the new value.  The
	 * register is determined by the upper 7 bits of the vector value and
	 * the bit within that register are determined by the lower 5 bits of
	 * the value.
	 */
	vector_reg = (vector >> 5) & 0x7F;
	vector_bit = vector & 0x1F;
	hw->mac.mta_shadow[vector_reg] |= (1 << vector_bit);
}

/**
 *  txgbe_update_mc_addr_list - Updates MAC list of multicast addresses
 *  @hw: pointer to hardware structure
 *  @mc_addr_list: the list of new multicast addresses
 *  @mc_addr_count: number of addresses
 *  @next: iterator function to walk the multicast address list
 *  @clear: flag, when set clears the table beforehand
 *
 *  When the clear flag is set, the given list replaces any existing list.
 *  Hashes the given addresses into the multicast table.
 **/
s32 txgbe_update_mc_addr_list(struct txgbe_hw *hw, u8 *mc_addr_list,
				      u32 mc_addr_count, txgbe_mc_addr_itr next,
				      bool clear)
{
	u32 i;
	u32 vmdq;

	/*
	 * Set the new number of MC addresses that we are being requested to
	 * use.
	 */
	hw->addr_ctrl.num_mc_addrs = mc_addr_count;
	hw->addr_ctrl.mta_in_use = 0;

	/* Clear mta_shadow */
	if (clear) {
		DEBUGOUT(" Clearing MTA");
		memset(&hw->mac.mta_shadow, 0, sizeof(hw->mac.mta_shadow));
	}

	/* Update mta_shadow */
	for (i = 0; i < mc_addr_count; i++) {
		DEBUGOUT(" Adding the multicast addresses:");
		txgbe_set_mta(hw, next(hw, &mc_addr_list, &vmdq));
	}

	/* Enable mta */
	for (i = 0; i < hw->mac.mcft_size; i++)
		wr32a(hw, TXGBE_MCADDRTBL(0), i,
				      hw->mac.mta_shadow[i]);

	if (hw->addr_ctrl.mta_in_use > 0) {
		u32 psrctl = rd32(hw, TXGBE_PSRCTL);
		psrctl &= ~(TXGBE_PSRCTL_ADHF12_MASK | TXGBE_PSRCTL_MCHFENA);
		psrctl |= TXGBE_PSRCTL_MCHFENA |
			 TXGBE_PSRCTL_ADHF12(hw->mac.mc_filter_type);
		wr32(hw, TXGBE_PSRCTL, psrctl);
	}

	DEBUGOUT("txgbe update mc addr list complete");
	return 0;
}

/**
 *  txgbe_fc_enable - Enable flow control
 *  @hw: pointer to hardware structure
 *
 *  Enable flow control according to the current settings.
 **/
s32 txgbe_fc_enable(struct txgbe_hw *hw)
{
	s32 err = 0;
	u32 mflcn_reg, fccfg_reg;
	u32 pause_time;
	u32 fcrtl, fcrth;
	int i;

	/* Validate the water mark configuration */
	if (!hw->fc.pause_time) {
		err = TXGBE_ERR_INVALID_LINK_SETTINGS;
		goto out;
	}

	/* Low water mark of zero causes XOFF floods */
	for (i = 0; i < TXGBE_DCB_TC_MAX; i++) {
		if ((hw->fc.current_mode & txgbe_fc_tx_pause) &&
		    hw->fc.high_water[i]) {
			if (!hw->fc.low_water[i] ||
			    hw->fc.low_water[i] >= hw->fc.high_water[i]) {
				DEBUGOUT("Invalid water mark configuration");
				err = TXGBE_ERR_INVALID_LINK_SETTINGS;
				goto out;
			}
		}
	}

	/* Negotiate the fc mode to use */
	hw->mac.fc_autoneg(hw);

	/* Disable any previous flow control settings */
	mflcn_reg = rd32(hw, TXGBE_RXFCCFG);
	mflcn_reg &= ~(TXGBE_RXFCCFG_FC | TXGBE_RXFCCFG_PFC);

	fccfg_reg = rd32(hw, TXGBE_TXFCCFG);
	fccfg_reg &= ~(TXGBE_TXFCCFG_FC | TXGBE_TXFCCFG_PFC);

	/*
	 * The possible values of fc.current_mode are:
	 * 0: Flow control is completely disabled
	 * 1: Rx flow control is enabled (we can receive pause frames,
	 *    but not send pause frames).
	 * 2: Tx flow control is enabled (we can send pause frames but
	 *    we do not support receiving pause frames).
	 * 3: Both Rx and Tx flow control (symmetric) are enabled.
	 * other: Invalid.
	 */
	switch (hw->fc.current_mode) {
	case txgbe_fc_none:
		/*
		 * Flow control is disabled by software override or autoneg.
		 * The code below will actually disable it in the HW.
		 */
		break;
	case txgbe_fc_rx_pause:
		/*
		 * Rx Flow control is enabled and Tx Flow control is
		 * disabled by software override. Since there really
		 * isn't a way to advertise that we are capable of RX
		 * Pause ONLY, we will advertise that we support both
		 * symmetric and asymmetric Rx PAUSE.  Later, we will
		 * disable the adapter's ability to send PAUSE frames.
		 */
		mflcn_reg |= TXGBE_RXFCCFG_FC;
		break;
	case txgbe_fc_tx_pause:
		/*
		 * Tx Flow control is enabled, and Rx Flow control is
		 * disabled by software override.
		 */
		fccfg_reg |= TXGBE_TXFCCFG_FC;
		break;
	case txgbe_fc_full:
		/* Flow control (both Rx and Tx) is enabled by SW override. */
		mflcn_reg |= TXGBE_RXFCCFG_FC;
		fccfg_reg |= TXGBE_TXFCCFG_FC;
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly");
		err = TXGBE_ERR_CONFIG;
		goto out;
	}

	/* Set 802.3x based flow control settings. */
	wr32(hw, TXGBE_RXFCCFG, mflcn_reg);
	wr32(hw, TXGBE_TXFCCFG, fccfg_reg);

	/* Set up and enable Rx high/low water mark thresholds, enable XON. */
	for (i = 0; i < TXGBE_DCB_TC_MAX; i++) {
		if ((hw->fc.current_mode & txgbe_fc_tx_pause) &&
		    hw->fc.high_water[i]) {
			fcrtl = TXGBE_FCWTRLO_TH(hw->fc.low_water[i]) |
				TXGBE_FCWTRLO_XON;
			fcrth = TXGBE_FCWTRHI_TH(hw->fc.high_water[i]) |
				TXGBE_FCWTRHI_XOFF;
		} else {
			/*
			 * In order to prevent Tx hangs when the internal Tx
			 * switch is enabled we must set the high water mark
			 * to the Rx packet buffer size - 24KB.  This allows
			 * the Tx switch to function even under heavy Rx
			 * workloads.
			 */
			fcrtl = 0;
			fcrth = rd32(hw, TXGBE_PBRXSIZE(i)) - 24576;
		}
		wr32(hw, TXGBE_FCWTRLO(i), fcrtl);
		wr32(hw, TXGBE_FCWTRHI(i), fcrth);
	}

	/* Configure pause time (2 TCs per register) */
	pause_time = TXGBE_RXFCFSH_TIME(hw->fc.pause_time);
	for (i = 0; i < (TXGBE_DCB_TC_MAX / 2); i++)
		wr32(hw, TXGBE_FCXOFFTM(i), pause_time * 0x00010001);

	/* Configure flow control refresh threshold value */
	wr32(hw, TXGBE_RXFCRFSH, hw->fc.pause_time / 2);

out:
	return err;
}

/**
 *  txgbe_negotiate_fc - Negotiate flow control
 *  @hw: pointer to hardware structure
 *  @adv_reg: flow control advertised settings
 *  @lp_reg: link partner's flow control settings
 *  @adv_sym: symmetric pause bit in advertisement
 *  @adv_asm: asymmetric pause bit in advertisement
 *  @lp_sym: symmetric pause bit in link partner advertisement
 *  @lp_asm: asymmetric pause bit in link partner advertisement
 *
 *  Find the intersection between advertised settings and link partner's
 *  advertised settings
 **/
s32 txgbe_negotiate_fc(struct txgbe_hw *hw, u32 adv_reg, u32 lp_reg,
		       u32 adv_sym, u32 adv_asm, u32 lp_sym, u32 lp_asm)
{
	if ((!(adv_reg)) ||  (!(lp_reg))) {
		DEBUGOUT("Local or link partner's advertised flow control settings are NULL. Local: %x, link partner: %x",
			      adv_reg, lp_reg);
		return TXGBE_ERR_FC_NOT_NEGOTIATED;
	}

	if ((adv_reg & adv_sym) && (lp_reg & lp_sym)) {
		/*
		 * Now we need to check if the user selected Rx ONLY
		 * of pause frames.  In this case, we had to advertise
		 * FULL flow control because we could not advertise RX
		 * ONLY. Hence, we must now check to see if we need to
		 * turn OFF the TRANSMISSION of PAUSE frames.
		 */
		if (hw->fc.requested_mode == txgbe_fc_full) {
			hw->fc.current_mode = txgbe_fc_full;
			DEBUGOUT("Flow Control = FULL.");
		} else {
			hw->fc.current_mode = txgbe_fc_rx_pause;
			DEBUGOUT("Flow Control=RX PAUSE frames only");
		}
	} else if (!(adv_reg & adv_sym) && (adv_reg & adv_asm) &&
		   (lp_reg & lp_sym) && (lp_reg & lp_asm)) {
		hw->fc.current_mode = txgbe_fc_tx_pause;
		DEBUGOUT("Flow Control = TX PAUSE frames only.");
	} else if ((adv_reg & adv_sym) && (adv_reg & adv_asm) &&
		   !(lp_reg & lp_sym) && (lp_reg & lp_asm)) {
		hw->fc.current_mode = txgbe_fc_rx_pause;
		DEBUGOUT("Flow Control = RX PAUSE frames only.");
	} else {
		hw->fc.current_mode = txgbe_fc_none;
		DEBUGOUT("Flow Control = NONE.");
	}
	return 0;
}

/**
 *  txgbe_fc_autoneg_fiber - Enable flow control on 1 gig fiber
 *  @hw: pointer to hardware structure
 *
 *  Enable flow control according on 1 gig fiber.
 **/
STATIC s32 txgbe_fc_autoneg_fiber(struct txgbe_hw *hw)
{
	u32 pcs_anadv_reg, pcs_lpab_reg;
	s32 err = TXGBE_ERR_FC_NOT_NEGOTIATED;

	/*
	 * On multispeed fiber at 1g, bail out if
	 * - link is up but AN did not complete, or if
	 * - link is up and AN completed but timed out
	 */

	pcs_anadv_reg = rd32_epcs(hw, SR_MII_MMD_AN_ADV);
	pcs_lpab_reg = rd32_epcs(hw, SR_MII_MMD_LP_BABL);

	err =  txgbe_negotiate_fc(hw, pcs_anadv_reg,
				      pcs_lpab_reg,
				      SR_MII_MMD_AN_ADV_PAUSE_SYM,
				      SR_MII_MMD_AN_ADV_PAUSE_ASM,
				      SR_MII_MMD_AN_ADV_PAUSE_SYM,
				      SR_MII_MMD_AN_ADV_PAUSE_ASM);

	return err;
}

/**
 *  txgbe_fc_autoneg_backplane - Enable flow control IEEE clause 37
 *  @hw: pointer to hardware structure
 *
 *  Enable flow control according to IEEE clause 37.
 **/
STATIC s32 txgbe_fc_autoneg_backplane(struct txgbe_hw *hw)
{
	u32 anlp1_reg, autoc_reg;
	s32 err = TXGBE_ERR_FC_NOT_NEGOTIATED;

	/*
	 * Read the 10g AN autoc and LP ability registers and resolve
	 * local flow control settings accordingly
	 */
	autoc_reg = rd32_epcs(hw, SR_AN_MMD_ADV_REG1);
	anlp1_reg = rd32_epcs(hw, SR_AN_MMD_LP_ABL1);

	err = txgbe_negotiate_fc(hw, autoc_reg,
		anlp1_reg,
		SR_AN_MMD_ADV_REG1_PAUSE_SYM,
		SR_AN_MMD_ADV_REG1_PAUSE_ASM,
		SR_AN_MMD_ADV_REG1_PAUSE_SYM,
		SR_AN_MMD_ADV_REG1_PAUSE_ASM);

	return err;
}

/**
 *  txgbe_fc_autoneg_copper - Enable flow control IEEE clause 37
 *  @hw: pointer to hardware structure
 *
 *  Enable flow control according to IEEE clause 37.
 **/
STATIC s32 txgbe_fc_autoneg_copper(struct txgbe_hw *hw)
{
	u16 technology_ability_reg = 0;
	u16 lp_technology_ability_reg = 0;

	hw->phy.read_reg(hw, TXGBE_MD_AUTO_NEG_ADVT,
			     TXGBE_MD_DEV_AUTO_NEG,
			     &technology_ability_reg);
	hw->phy.read_reg(hw, TXGBE_MD_AUTO_NEG_LP,
			     TXGBE_MD_DEV_AUTO_NEG,
			     &lp_technology_ability_reg);

	return txgbe_negotiate_fc(hw, (u32)technology_ability_reg,
				  (u32)lp_technology_ability_reg,
				  TXGBE_TAF_SYM_PAUSE, TXGBE_TAF_ASM_PAUSE,
				  TXGBE_TAF_SYM_PAUSE, TXGBE_TAF_ASM_PAUSE);
}

/**
 *  txgbe_fc_autoneg - Configure flow control
 *  @hw: pointer to hardware structure
 *
 *  Compares our advertised flow control capabilities to those advertised by
 *  our link partner, and determines the proper flow control mode to use.
 **/
void txgbe_fc_autoneg(struct txgbe_hw *hw)
{
	s32 err = TXGBE_ERR_FC_NOT_NEGOTIATED;
	u32 speed;
	bool link_up;

	/*
	 * AN should have completed when the cable was plugged in.
	 * Look for reasons to bail out.  Bail out if:
	 * - FC autoneg is disabled, or if
	 * - link is not up.
	 */
	if (hw->fc.disable_fc_autoneg) {
		DEBUGOUT("Flow control autoneg is disabled");
		goto out;
	}

	hw->mac.check_link(hw, &speed, &link_up, false);
	if (!link_up) {
		DEBUGOUT("The link is down");
		goto out;
	}

	switch (hw->phy.media_type) {
	/* Autoneg flow control on fiber adapters */
	case txgbe_media_type_fiber_qsfp:
	case txgbe_media_type_fiber:
		if (speed == TXGBE_LINK_SPEED_1GB_FULL)
			err = txgbe_fc_autoneg_fiber(hw);
		break;

	/* Autoneg flow control on backplane adapters */
	case txgbe_media_type_backplane:
		err = txgbe_fc_autoneg_backplane(hw);
		break;

	/* Autoneg flow control on copper adapters */
	case txgbe_media_type_copper:
		if (txgbe_device_supports_autoneg_fc(hw))
			err = txgbe_fc_autoneg_copper(hw);
		break;

	default:
		break;
	}

out:
	if (err == 0) {
		hw->fc.fc_was_autonegged = true;
	} else {
		hw->fc.fc_was_autonegged = false;
		hw->fc.current_mode = hw->fc.requested_mode;
	}
}

s32 txgbe_set_pcie_master(struct txgbe_hw *hw, bool enable)
{
	struct rte_pci_device *pci_dev = (struct rte_pci_device *)hw->back;
	s32 status = 0;
	u32 i;

	if (rte_pci_set_bus_master(pci_dev, enable) < 0) {
		DEBUGOUT("Cannot configure PCI bus master.");
		return -1;
	}

	if (enable)
		goto out;

	/* Exit if master requests are blocked */
	if (!(rd32(hw, TXGBE_BMEPEND)))
		goto out;

	/* Poll for master request bit to clear */
	for (i = 0; i < TXGBE_PCI_MASTER_DISABLE_TIMEOUT; i++) {
		usec_delay(100);
		if (!(rd32(hw, TXGBE_BMEPEND)))
			goto out;
	}

	DEBUGOUT("PCIe transaction pending bit also did not clear.");
	status = TXGBE_ERR_MASTER_REQUESTS_PENDING;

out:
	return status;
}

/**
 *  txgbe_acquire_swfw_sync - Acquire SWFW semaphore
 *  @hw: pointer to hardware structure
 *  @mask: Mask to specify which semaphore to acquire
 *
 *  Acquires the SWFW semaphore through the MNGSEM register for the specified
 *  function (CSR, PHY0, PHY1, EEPROM, Flash)
 **/
s32 txgbe_acquire_swfw_sync(struct txgbe_hw *hw, u32 mask)
{
	u32 mngsem = 0;
	u32 swmask = TXGBE_MNGSEM_SW(mask);
	u32 fwmask = TXGBE_MNGSEM_FW(mask);
	u32 timeout = 200;
	u32 i;

	for (i = 0; i < timeout; i++) {
		/*
		 * SW NVM semaphore bit is used for access to all
		 * SW_FW_SYNC bits (not just NVM)
		 */
		if (txgbe_get_eeprom_semaphore(hw))
			return TXGBE_ERR_SWFW_SYNC;

		mngsem = rd32(hw, TXGBE_MNGSEM);
		if (mngsem & (fwmask | swmask)) {
			/* Resource is currently in use by FW or SW */
			txgbe_release_eeprom_semaphore(hw);
			msec_delay(5);
		} else {
			mngsem |= swmask;
			wr32(hw, TXGBE_MNGSEM, mngsem);
			txgbe_release_eeprom_semaphore(hw);
			return 0;
		}
	}

	/* If time expired clear the bits holding the lock and retry */
	if (mngsem & (fwmask | swmask))
		txgbe_release_swfw_sync(hw, mngsem & (fwmask | swmask));

	msec_delay(5);
	return TXGBE_ERR_SWFW_SYNC;
}

/**
 *  txgbe_release_swfw_sync - Release SWFW semaphore
 *  @hw: pointer to hardware structure
 *  @mask: Mask to specify which semaphore to release
 *
 *  Releases the SWFW semaphore through the MNGSEM register for the specified
 *  function (CSR, PHY0, PHY1, EEPROM, Flash)
 **/
void txgbe_release_swfw_sync(struct txgbe_hw *hw, u32 mask)
{
	u32 mngsem;
	u32 swmask = mask;

	txgbe_get_eeprom_semaphore(hw);

	mngsem = rd32(hw, TXGBE_MNGSEM);
	mngsem &= ~swmask;
	wr32(hw, TXGBE_MNGSEM, mngsem);

	txgbe_release_eeprom_semaphore(hw);
}

/**
 *  txgbe_disable_sec_rx_path - Stops the receive data path
 *  @hw: pointer to hardware structure
 *
 *  Stops the receive data path and waits for the HW to internally empty
 *  the Rx security block
 **/
s32 txgbe_disable_sec_rx_path(struct txgbe_hw *hw)
{
#define TXGBE_MAX_SECRX_POLL 4000

	int i;
	u32 secrxreg;

	secrxreg = rd32(hw, TXGBE_SECRXCTL);
	secrxreg |= TXGBE_SECRXCTL_XDSA;
	wr32(hw, TXGBE_SECRXCTL, secrxreg);
	for (i = 0; i < TXGBE_MAX_SECRX_POLL; i++) {
		secrxreg = rd32(hw, TXGBE_SECRXSTAT);
		if (!(secrxreg & TXGBE_SECRXSTAT_RDY))
			/* Use interrupt-safe sleep just in case */
			usec_delay(10);
		else
			break;
	}

	/* For informational purposes only */
	if (i >= TXGBE_MAX_SECRX_POLL)
		DEBUGOUT("Rx unit being enabled before security path fully disabled.  Continuing with init.");

	return 0;
}

/**
 *  txgbe_enable_sec_rx_path - Enables the receive data path
 *  @hw: pointer to hardware structure
 *
 *  Enables the receive data path.
 **/
s32 txgbe_enable_sec_rx_path(struct txgbe_hw *hw)
{
	u32 secrxreg;

	secrxreg = rd32(hw, TXGBE_SECRXCTL);
	secrxreg &= ~TXGBE_SECRXCTL_XDSA;
	wr32(hw, TXGBE_SECRXCTL, secrxreg);
	txgbe_flush(hw);

	return 0;
}

/**
 *  txgbe_disable_sec_tx_path - Stops the transmit data path
 *  @hw: pointer to hardware structure
 *
 *  Stops the transmit data path and waits for the HW to internally empty
 *  the Tx security block
 **/
int txgbe_disable_sec_tx_path(struct txgbe_hw *hw)
{
#define TXGBE_MAX_SECTX_POLL 40

	int i;
	u32 sectxreg;

	sectxreg = rd32(hw, TXGBE_SECTXCTL);
	sectxreg |= TXGBE_SECTXCTL_XDSA;
	wr32(hw, TXGBE_SECTXCTL, sectxreg);
	for (i = 0; i < TXGBE_MAX_SECTX_POLL; i++) {
		sectxreg = rd32(hw, TXGBE_SECTXSTAT);
		if (sectxreg & TXGBE_SECTXSTAT_RDY)
			break;
		/* Use interrupt-safe sleep just in case */
		usec_delay(1000);
	}

	/* For informational purposes only */
	if (i >= TXGBE_MAX_SECTX_POLL)
		DEBUGOUT("Tx unit being enabled before security path fully disabled.  Continuing with init.");

	return 0;
}

/**
 *  txgbe_enable_sec_tx_path - Enables the transmit data path
 *  @hw: pointer to hardware structure
 *
 *  Enables the transmit data path.
 **/
int txgbe_enable_sec_tx_path(struct txgbe_hw *hw)
{
	uint32_t sectxreg;

	sectxreg = rd32(hw, TXGBE_SECTXCTL);
	sectxreg &= ~TXGBE_SECTXCTL_XDSA;
	wr32(hw, TXGBE_SECTXCTL, sectxreg);
	txgbe_flush(hw);

	return 0;
}

/**
 *  txgbe_get_san_mac_addr_offset - Get SAN MAC address offset from the EEPROM
 *  @hw: pointer to hardware structure
 *  @san_mac_offset: SAN MAC address offset
 *
 *  This function will read the EEPROM location for the SAN MAC address
 *  pointer, and returns the value at that location.  This is used in both
 *  get and set mac_addr routines.
 **/
static s32 txgbe_get_san_mac_addr_offset(struct txgbe_hw *hw,
					 u16 *san_mac_offset)
{
	s32 err;

	/*
	 * First read the EEPROM pointer to see if the MAC addresses are
	 * available.
	 */
	err = hw->rom.readw_sw(hw, TXGBE_SAN_MAC_ADDR_PTR,
				      san_mac_offset);
	if (err) {
		DEBUGOUT("eeprom at offset %d failed",
			 TXGBE_SAN_MAC_ADDR_PTR);
	}

	return err;
}

/**
 *  txgbe_get_san_mac_addr - SAN MAC address retrieval from the EEPROM
 *  @hw: pointer to hardware structure
 *  @san_mac_addr: SAN MAC address
 *
 *  Reads the SAN MAC address from the EEPROM, if it's available.  This is
 *  per-port, so set_lan_id() must be called before reading the addresses.
 *  set_lan_id() is called by identify_sfp(), but this cannot be relied
 *  upon for non-SFP connections, so we must call it here.
 **/
s32 txgbe_get_san_mac_addr(struct txgbe_hw *hw, u8 *san_mac_addr)
{
	u16 san_mac_data, san_mac_offset;
	u8 i;
	s32 err;

	/*
	 * First read the EEPROM pointer to see if the MAC addresses are
	 * available. If they're not, no point in calling set_lan_id() here.
	 */
	err = txgbe_get_san_mac_addr_offset(hw, &san_mac_offset);
	if (err || san_mac_offset == 0 || san_mac_offset == 0xFFFF)
		goto san_mac_addr_out;

	/* apply the port offset to the address offset */
	(hw->bus.func) ? (san_mac_offset += TXGBE_SAN_MAC_ADDR_PORT1_OFFSET) :
			 (san_mac_offset += TXGBE_SAN_MAC_ADDR_PORT0_OFFSET);
	for (i = 0; i < 3; i++) {
		err = hw->rom.read16(hw, san_mac_offset,
					      &san_mac_data);
		if (err) {
			DEBUGOUT("eeprom read at offset %d failed",
				 san_mac_offset);
			goto san_mac_addr_out;
		}
		san_mac_addr[i * 2] = (u8)(san_mac_data);
		san_mac_addr[i * 2 + 1] = (u8)(san_mac_data >> 8);
		san_mac_offset++;
	}
	return 0;

san_mac_addr_out:
	/*
	 * No addresses available in this EEPROM.  It's not an
	 * error though, so just wipe the local address and return.
	 */
	for (i = 0; i < 6; i++)
		san_mac_addr[i] = 0xFF;
	return 0;
}

/**
 *  txgbe_set_san_mac_addr - Write the SAN MAC address to the EEPROM
 *  @hw: pointer to hardware structure
 *  @san_mac_addr: SAN MAC address
 *
 *  Write a SAN MAC address to the EEPROM.
 **/
s32 txgbe_set_san_mac_addr(struct txgbe_hw *hw, u8 *san_mac_addr)
{
	s32 err;
	u16 san_mac_data, san_mac_offset;
	u8 i;

	/* Look for SAN mac address pointer.  If not defined, return */
	err = txgbe_get_san_mac_addr_offset(hw, &san_mac_offset);
	if (err || san_mac_offset == 0 || san_mac_offset == 0xFFFF)
		return TXGBE_ERR_NO_SAN_ADDR_PTR;

	/* Apply the port offset to the address offset */
	(hw->bus.func) ? (san_mac_offset += TXGBE_SAN_MAC_ADDR_PORT1_OFFSET) :
			 (san_mac_offset += TXGBE_SAN_MAC_ADDR_PORT0_OFFSET);

	for (i = 0; i < 3; i++) {
		san_mac_data = (u16)((u16)(san_mac_addr[i * 2 + 1]) << 8);
		san_mac_data |= (u16)(san_mac_addr[i * 2]);
		hw->rom.write16(hw, san_mac_offset, san_mac_data);
		san_mac_offset++;
	}

	return 0;
}

/**
 *  txgbe_clear_vmdq - Disassociate a VMDq pool index from a rx address
 *  @hw: pointer to hardware struct
 *  @rar: receive address register index to disassociate
 *  @vmdq: VMDq pool index to remove from the rar
 **/
s32 txgbe_clear_vmdq(struct txgbe_hw *hw, u32 rar, u32 vmdq)
{
	u32 mpsar_lo, mpsar_hi;
	u32 rar_entries = hw->mac.num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (rar >= rar_entries) {
		DEBUGOUT("RAR index %d is out of range.", rar);
		return TXGBE_ERR_INVALID_ARGUMENT;
	}

	wr32(hw, TXGBE_ETHADDRIDX, rar);
	mpsar_lo = rd32(hw, TXGBE_ETHADDRASSL);
	mpsar_hi = rd32(hw, TXGBE_ETHADDRASSH);

	if (TXGBE_REMOVED(hw->hw_addr))
		goto done;

	if (!mpsar_lo && !mpsar_hi)
		goto done;

	if (vmdq == BIT_MASK32) {
		if (mpsar_lo) {
			wr32(hw, TXGBE_ETHADDRASSL, 0);
			mpsar_lo = 0;
		}
		if (mpsar_hi) {
			wr32(hw, TXGBE_ETHADDRASSH, 0);
			mpsar_hi = 0;
		}
	} else if (vmdq < 32) {
		mpsar_lo &= ~(1 << vmdq);
		wr32(hw, TXGBE_ETHADDRASSL, mpsar_lo);
	} else {
		mpsar_hi &= ~(1 << (vmdq - 32));
		wr32(hw, TXGBE_ETHADDRASSH, mpsar_hi);
	}

	/* was that the last pool using this rar? */
	if (mpsar_lo == 0 && mpsar_hi == 0 &&
	    rar != 0 && rar != hw->mac.san_mac_rar_index)
		hw->mac.clear_rar(hw, rar);
done:
	return 0;
}

/**
 *  txgbe_set_vmdq - Associate a VMDq pool index with a rx address
 *  @hw: pointer to hardware struct
 *  @rar: receive address register index to associate with a VMDq index
 *  @vmdq: VMDq pool index
 **/
s32 txgbe_set_vmdq(struct txgbe_hw *hw, u32 rar, u32 vmdq)
{
	u32 mpsar;
	u32 rar_entries = hw->mac.num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (rar >= rar_entries) {
		DEBUGOUT("RAR index %d is out of range.", rar);
		return TXGBE_ERR_INVALID_ARGUMENT;
	}

	wr32(hw, TXGBE_ETHADDRIDX, rar);
	if (vmdq < 32) {
		mpsar = rd32(hw, TXGBE_ETHADDRASSL);
		mpsar |= 1 << vmdq;
		wr32(hw, TXGBE_ETHADDRASSL, mpsar);
	} else {
		mpsar = rd32(hw, TXGBE_ETHADDRASSH);
		mpsar |= 1 << (vmdq - 32);
		wr32(hw, TXGBE_ETHADDRASSH, mpsar);
	}
	return 0;
}

/**
 *  txgbe_init_uta_tables - Initialize the Unicast Table Array
 *  @hw: pointer to hardware structure
 **/
s32 txgbe_init_uta_tables(struct txgbe_hw *hw)
{
	int i;

	DEBUGOUT(" Clearing UTA");

	for (i = 0; i < 128; i++)
		wr32(hw, TXGBE_UCADDRTBL(i), 0);

	return 0;
}

/**
 *  txgbe_find_vlvf_slot - find the vlanid or the first empty slot
 *  @hw: pointer to hardware structure
 *  @vlan: VLAN id to write to VLAN filter
 *  @vlvf_bypass: true to find vlanid only, false returns first empty slot if
 *		  vlanid not found
 *
 *
 *  return the VLVF index where this VLAN id should be placed
 *
 **/
s32 txgbe_find_vlvf_slot(struct txgbe_hw *hw, u32 vlan, bool vlvf_bypass)
{
	s32 regindex, first_empty_slot;
	u32 bits;

	/* short cut the special case */
	if (vlan == 0)
		return 0;

	/* if vlvf_bypass is set we don't want to use an empty slot, we
	 * will simply bypass the VLVF if there are no entries present in the
	 * VLVF that contain our VLAN
	 */
	first_empty_slot = vlvf_bypass ? TXGBE_ERR_NO_SPACE : 0;

	/* add VLAN enable bit for comparison */
	vlan |= TXGBE_PSRVLAN_EA;

	/* Search for the vlan id in the VLVF entries. Save off the first empty
	 * slot found along the way.
	 *
	 * pre-decrement loop covering (TXGBE_NUM_POOL - 1) .. 1
	 */
	for (regindex = TXGBE_NUM_POOL; --regindex;) {
		wr32(hw, TXGBE_PSRVLANIDX, regindex);
		bits = rd32(hw, TXGBE_PSRVLAN);
		if (bits == vlan)
			return regindex;
		if (!first_empty_slot && !bits)
			first_empty_slot = regindex;
	}

	/* If we are here then we didn't find the VLAN.  Return first empty
	 * slot we found during our search, else error.
	 */
	if (!first_empty_slot)
		DEBUGOUT("No space in VLVF.");

	return first_empty_slot ? first_empty_slot : TXGBE_ERR_NO_SPACE;
}

/**
 *  txgbe_set_vfta - Set VLAN filter table
 *  @hw: pointer to hardware structure
 *  @vlan: VLAN id to write to VLAN filter
 *  @vind: VMDq output index that maps queue to VLAN id in VLVFB
 *  @vlan_on: boolean flag to turn on/off VLAN
 *  @vlvf_bypass: boolean flag indicating updating default pool is okay
 *
 *  Turn on/off specified VLAN in the VLAN filter table.
 **/
s32 txgbe_set_vfta(struct txgbe_hw *hw, u32 vlan, u32 vind,
			   bool vlan_on, bool vlvf_bypass)
{
	u32 regidx, vfta_delta, vfta;
	s32 err;

	if (vlan > 4095 || vind > 63)
		return TXGBE_ERR_PARAM;

	/*
	 * this is a 2 part operation - first the VFTA, then the
	 * VLVF and VLVFB if VT Mode is set
	 * We don't write the VFTA until we know the VLVF part succeeded.
	 */

	/* Part 1
	 * The VFTA is a bitstring made up of 128 32-bit registers
	 * that enable the particular VLAN id, much like the MTA:
	 *    bits[11-5]: which register
	 *    bits[4-0]:  which bit in the register
	 */
	regidx = vlan / 32;
	vfta_delta = 1 << (vlan % 32);
	vfta = rd32(hw, TXGBE_VLANTBL(regidx));

	/*
	 * vfta_delta represents the difference between the current value
	 * of vfta and the value we want in the register.  Since the diff
	 * is an XOR mask we can just update the vfta using an XOR
	 */
	vfta_delta &= vlan_on ? ~vfta : vfta;
	vfta ^= vfta_delta;

	/* Part 2
	 * Call txgbe_set_vlvf to set VLVFB and VLVF
	 */
	err = txgbe_set_vlvf(hw, vlan, vind, vlan_on, &vfta_delta,
					 vfta, vlvf_bypass);
	if (err != 0) {
		if (vlvf_bypass)
			goto vfta_update;
		return err;
	}

vfta_update:
	/* Update VFTA now that we are ready for traffic */
	if (vfta_delta)
		wr32(hw, TXGBE_VLANTBL(regidx), vfta);

	return 0;
}

/**
 *  txgbe_set_vlvf - Set VLAN Pool Filter
 *  @hw: pointer to hardware structure
 *  @vlan: VLAN id to write to VLAN filter
 *  @vind: VMDq output index that maps queue to VLAN id in PSRVLANPLM
 *  @vlan_on: boolean flag to turn on/off VLAN in PSRVLAN
 *  @vfta_delta: pointer to the difference between the current value
 *		 of PSRVLANPLM and the desired value
 *  @vfta: the desired value of the VFTA
 *  @vlvf_bypass: boolean flag indicating updating default pool is okay
 *
 *  Turn on/off specified bit in VLVF table.
 **/
s32 txgbe_set_vlvf(struct txgbe_hw *hw, u32 vlan, u32 vind,
			   bool vlan_on, u32 *vfta_delta, u32 vfta,
			   bool vlvf_bypass)
{
	u32 bits;
	u32 portctl;
	s32 vlvf_index;

	if (vlan > 4095 || vind > 63)
		return TXGBE_ERR_PARAM;

	/* If VT Mode is set
	 *   Either vlan_on
	 *     make sure the vlan is in PSRVLAN
	 *     set the vind bit in the matching PSRVLANPLM
	 *   Or !vlan_on
	 *     clear the pool bit and possibly the vind
	 */
	portctl = rd32(hw, TXGBE_PORTCTL);
	if (!(portctl & TXGBE_PORTCTL_NUMVT_MASK))
		return 0;

	vlvf_index = txgbe_find_vlvf_slot(hw, vlan, vlvf_bypass);
	if (vlvf_index < 0)
		return vlvf_index;

	wr32(hw, TXGBE_PSRVLANIDX, vlvf_index);
	bits = rd32(hw, TXGBE_PSRVLANPLM(vind / 32));

	/* set the pool bit */
	bits |= 1 << (vind % 32);
	if (vlan_on)
		goto vlvf_update;

	/* clear the pool bit */
	bits ^= 1 << (vind % 32);

	if (!bits &&
	    !rd32(hw, TXGBE_PSRVLANPLM(vind / 32))) {
		/* Clear PSRVLANPLM first, then disable PSRVLAN. Otherwise
		 * we run the risk of stray packets leaking into
		 * the PF via the default pool
		 */
		if (*vfta_delta)
			wr32(hw, TXGBE_PSRVLANPLM(vlan / 32), vfta);

		/* disable VLVF and clear remaining bit from pool */
		wr32(hw, TXGBE_PSRVLAN, 0);
		wr32(hw, TXGBE_PSRVLANPLM(vind / 32), 0);

		return 0;
	}

	/* If there are still bits set in the PSRVLANPLM registers
	 * for the VLAN ID indicated we need to see if the
	 * caller is requesting that we clear the PSRVLANPLM entry bit.
	 * If the caller has requested that we clear the PSRVLANPLM
	 * entry bit but there are still pools/VFs using this VLAN
	 * ID entry then ignore the request.  We're not worried
	 * about the case where we're turning the PSRVLANPLM VLAN ID
	 * entry bit on, only when requested to turn it off as
	 * there may be multiple pools and/or VFs using the
	 * VLAN ID entry.  In that case we cannot clear the
	 * PSRVLANPLM bit until all pools/VFs using that VLAN ID have also
	 * been cleared.  This will be indicated by "bits" being
	 * zero.
	 */
	*vfta_delta = 0;

vlvf_update:
	/* record pool change and enable VLAN ID if not already enabled */
	wr32(hw, TXGBE_PSRVLANPLM(vind / 32), bits);
	wr32(hw, TXGBE_PSRVLAN, TXGBE_PSRVLAN_EA | vlan);

	return 0;
}

/**
 *  txgbe_clear_vfta - Clear VLAN filter table
 *  @hw: pointer to hardware structure
 *
 *  Clears the VLAN filer table, and the VMDq index associated with the filter
 **/
s32 txgbe_clear_vfta(struct txgbe_hw *hw)
{
	u32 offset;

	for (offset = 0; offset < hw->mac.vft_size; offset++)
		wr32(hw, TXGBE_VLANTBL(offset), 0);

	for (offset = 0; offset < TXGBE_NUM_POOL; offset++) {
		wr32(hw, TXGBE_PSRVLANIDX, offset);
		wr32(hw, TXGBE_PSRVLAN, 0);
		wr32(hw, TXGBE_PSRVLANPLM(0), 0);
		wr32(hw, TXGBE_PSRVLANPLM(1), 0);
	}

	return 0;
}

/**
 *  txgbe_need_crosstalk_fix - Determine if we need to do cross talk fix
 *  @hw: pointer to hardware structure
 *
 *  Contains the logic to identify if we need to verify link for the
 *  crosstalk fix
 **/
static bool txgbe_need_crosstalk_fix(struct txgbe_hw *hw)
{
	/* Does FW say we need the fix */
	if (!hw->need_crosstalk_fix)
		return false;

	/* Only consider SFP+ PHYs i.e. media type fiber */
	switch (hw->phy.media_type) {
	case txgbe_media_type_fiber:
	case txgbe_media_type_fiber_qsfp:
		break;
	default:
		return false;
	}

	return true;
}

/**
 *  txgbe_check_mac_link - Determine link and speed status
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @link_up: true when link is up
 *  @link_up_wait_to_complete: bool used to wait for link up or not
 *
 *  Reads the links register to determine if link is up and the current speed
 **/
s32 txgbe_check_mac_link_sp(struct txgbe_hw *hw, u32 *speed,
				 bool *link_up, bool link_up_wait_to_complete)
{
	u32 links_reg, links_orig;
	u32 i;

	/* If Crosstalk fix enabled do the sanity check of making sure
	 * the SFP+ cage is full.
	 */
	if (txgbe_need_crosstalk_fix(hw)) {
		u32 sfp_cage_full;

		switch (hw->mac.type) {
		case txgbe_mac_sp:
			sfp_cage_full = !rd32m(hw, TXGBE_GPIODATA,
					TXGBE_GPIOBIT_2);
			break;
		default:
			/* sanity check - No SFP+ devices here */
			sfp_cage_full = false;
			break;
		}

		if (!sfp_cage_full) {
			*link_up = false;
			*speed = TXGBE_LINK_SPEED_UNKNOWN;
			return 0;
		}
	}

	/* clear the old state */
	links_orig = rd32(hw, TXGBE_PORTSTAT);

	links_reg = rd32(hw, TXGBE_PORTSTAT);

	if (links_orig != links_reg) {
		DEBUGOUT("LINKS changed from %08X to %08X",
			  links_orig, links_reg);
	}

	if (link_up_wait_to_complete) {
		for (i = 0; i < hw->mac.max_link_up_time; i++) {
			if (!(links_reg & TXGBE_PORTSTAT_UP)) {
				*link_up = false;
			} else {
				*link_up = true;
				break;
			}
			msec_delay(100);
			links_reg = rd32(hw, TXGBE_PORTSTAT);
		}
	} else {
		if (links_reg & TXGBE_PORTSTAT_UP)
			*link_up = true;
		else
			*link_up = false;
	}

	switch (links_reg & TXGBE_PORTSTAT_BW_MASK) {
	case TXGBE_PORTSTAT_BW_10G:
		*speed = TXGBE_LINK_SPEED_10GB_FULL;
		break;
	case TXGBE_PORTSTAT_BW_1G:
		*speed = TXGBE_LINK_SPEED_1GB_FULL;
		break;
	case TXGBE_PORTSTAT_BW_100M:
		*speed = TXGBE_LINK_SPEED_100M_FULL;
		break;
	default:
		*speed = TXGBE_LINK_SPEED_UNKNOWN;
	}

	return 0;
}

/**
 *  txgbe_get_wwn_prefix - Get alternative WWNN/WWPN prefix from
 *  the EEPROM
 *  @hw: pointer to hardware structure
 *  @wwnn_prefix: the alternative WWNN prefix
 *  @wwpn_prefix: the alternative WWPN prefix
 *
 *  This function will read the EEPROM from the alternative SAN MAC address
 *  block to check the support for the alternative WWNN/WWPN prefix support.
 **/
s32 txgbe_get_wwn_prefix(struct txgbe_hw *hw, u16 *wwnn_prefix,
				 u16 *wwpn_prefix)
{
	u16 offset, caps;
	u16 alt_san_mac_blk_offset;

	/* clear output first */
	*wwnn_prefix = 0xFFFF;
	*wwpn_prefix = 0xFFFF;

	/* check if alternative SAN MAC is supported */
	offset = TXGBE_ALT_SAN_MAC_ADDR_BLK_PTR;
	if (hw->rom.readw_sw(hw, offset, &alt_san_mac_blk_offset))
		goto wwn_prefix_err;

	if (alt_san_mac_blk_offset == 0 || alt_san_mac_blk_offset == 0xFFFF)
		goto wwn_prefix_out;

	/* check capability in alternative san mac address block */
	offset = alt_san_mac_blk_offset + TXGBE_ALT_SAN_MAC_ADDR_CAPS_OFFSET;
	if (hw->rom.read16(hw, offset, &caps))
		goto wwn_prefix_err;
	if (!(caps & TXGBE_ALT_SAN_MAC_ADDR_CAPS_ALTWWN))
		goto wwn_prefix_out;

	/* get the corresponding prefix for WWNN/WWPN */
	offset = alt_san_mac_blk_offset + TXGBE_ALT_SAN_MAC_ADDR_WWNN_OFFSET;
	if (hw->rom.read16(hw, offset, wwnn_prefix))
		DEBUGOUT("eeprom read at offset %d failed", offset);

	offset = alt_san_mac_blk_offset + TXGBE_ALT_SAN_MAC_ADDR_WWPN_OFFSET;
	if (hw->rom.read16(hw, offset, wwpn_prefix))
		goto wwn_prefix_err;

wwn_prefix_out:
	return 0;

wwn_prefix_err:
	DEBUGOUT("eeprom read at offset %d failed", offset);
	return 0;
}

/**
 *  txgbe_set_mac_anti_spoofing - Enable/Disable MAC anti-spoofing
 *  @hw: pointer to hardware structure
 *  @enable: enable or disable switch for MAC anti-spoofing
 *  @vf: Virtual Function pool - VF Pool to set for MAC anti-spoofing
 *
 **/
void txgbe_set_mac_anti_spoofing(struct txgbe_hw *hw, bool enable, int vf)
{
	int vf_target_reg = vf >> 3;
	int vf_target_shift = vf % 8;
	u32 pfvfspoof;

	pfvfspoof = rd32(hw, TXGBE_POOLTXASMAC(vf_target_reg));
	if (enable)
		pfvfspoof |= (1 << vf_target_shift);
	else
		pfvfspoof &= ~(1 << vf_target_shift);
	wr32(hw, TXGBE_POOLTXASMAC(vf_target_reg), pfvfspoof);
}

/**
 * txgbe_set_ethertype_anti_spoofing - Configure Ethertype anti-spoofing
 * @hw: pointer to hardware structure
 * @enable: enable or disable switch for Ethertype anti-spoofing
 * @vf: Virtual Function pool - VF Pool to set for Ethertype anti-spoofing
 *
 **/
void txgbe_set_ethertype_anti_spoofing(struct txgbe_hw *hw,
		bool enable, int vf)
{
	int vf_target_reg = vf >> 3;
	int vf_target_shift = vf % 8;
	u32 pfvfspoof;

	pfvfspoof = rd32(hw, TXGBE_POOLTXASET(vf_target_reg));
	if (enable)
		pfvfspoof |= (1 << vf_target_shift);
	else
		pfvfspoof &= ~(1 << vf_target_shift);
	wr32(hw, TXGBE_POOLTXASET(vf_target_reg), pfvfspoof);
}

/**
 *  txgbe_get_device_caps - Get additional device capabilities
 *  @hw: pointer to hardware structure
 *  @device_caps: the EEPROM word with the extra device capabilities
 *
 *  This function will read the EEPROM location for the device capabilities,
 *  and return the word through device_caps.
 **/
s32 txgbe_get_device_caps(struct txgbe_hw *hw, u16 *device_caps)
{
	hw->rom.readw_sw(hw, TXGBE_DEVICE_CAPS, device_caps);

	return 0;
}

/**
 * txgbe_set_pba - Initialize Rx packet buffer
 * @hw: pointer to hardware structure
 * @num_pb: number of packet buffers to allocate
 * @headroom: reserve n KB of headroom
 * @strategy: packet buffer allocation strategy
 **/
void txgbe_set_pba(struct txgbe_hw *hw, int num_pb, u32 headroom,
			     int strategy)
{
	u32 pbsize = hw->mac.rx_pb_size;
	int i = 0;
	u32 rxpktsize, txpktsize, txpbthresh;

	UNREFERENCED_PARAMETER(hw);
	UNREFERENCED_PARAMETER(headroom);

	if (!num_pb)
		num_pb = 1;

	/* Divide remaining packet buffer space amongst the number of packet
	 * buffers requested using supplied strategy.
	 */
	switch (strategy) {
	case PBA_STRATEGY_WEIGHTED:
		/* txgbe_dcb_pba_80_48 strategy weight first half of packet
		 * buffer with 5/8 of the packet buffer space.
		 */
		rxpktsize = (pbsize * 5) / (num_pb * 4);
		pbsize -= rxpktsize * (num_pb / 2);
		rxpktsize <<= 10;
		for (; i < (num_pb / 2); i++)
			wr32(hw, TXGBE_PBRXSIZE(i), rxpktsize);
		/* fall through - configure remaining packet buffers */
	case PBA_STRATEGY_EQUAL:
		rxpktsize = (pbsize / (num_pb - i));
		rxpktsize <<= 10;
		for (; i < num_pb; i++)
			wr32(hw, TXGBE_PBRXSIZE(i), rxpktsize);
		break;
	default:
		break;
	}

	/* Only support an equally distributed Tx packet buffer strategy. */
	txpktsize = TXGBE_PBTXSIZE_MAX / num_pb;
	txpbthresh = (txpktsize / 1024) - TXGBE_TXPKT_SIZE_MAX;
	for (i = 0; i < num_pb; i++) {
		wr32(hw, TXGBE_PBTXSIZE(i), txpktsize);
		wr32(hw, TXGBE_PBTXDMATH(i), txpbthresh);
	}

	/* Clear unused TCs, if any, to zero buffer size*/
	for (; i < TXGBE_MAX_UP; i++) {
		wr32(hw, TXGBE_PBRXSIZE(i), 0);
		wr32(hw, TXGBE_PBTXSIZE(i), 0);
		wr32(hw, TXGBE_PBTXDMATH(i), 0);
	}
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
 *  txgbe_get_thermal_sensor_data - Gathers thermal sensor data
 *  @hw: pointer to hardware structure
 *
 *  Returns the thermal sensor data structure
 **/
s32 txgbe_get_thermal_sensor_data(struct txgbe_hw *hw)
{
	struct txgbe_thermal_sensor_data *data = &hw->mac.thermal_sensor_data;
	s64 tsv;
	u32 ts_stat;

	/* Only support thermal sensors attached to physical port 0 */
	if (hw->bus.lan_id != 0)
		return TXGBE_NOT_IMPLEMENTED;

	ts_stat = rd32(hw, TXGBE_TSSTAT);
	tsv = (s64)TXGBE_TSSTAT_DATA(ts_stat);
	tsv = tsv > 1200 ? tsv : 1200;
	tsv = -(48380 << 8) / 1000
		+ tsv * (31020 << 8) / 100000
		- tsv * tsv * (18201 << 8) / 100000000
		+ tsv * tsv * tsv * (81542 << 8) / 1000000000000
		- tsv * tsv * tsv * tsv * (16743 << 8) / 1000000000000000;
	tsv >>= 8;

	data->sensor[0].temp = (s16)tsv;

	return 0;
}

/**
 *  txgbe_init_thermal_sensor_thresh - Inits thermal sensor thresholds
 *  @hw: pointer to hardware structure
 *
 *  Inits the thermal sensor thresholds according to the NVM map
 *  and save off the threshold and location values into mac.thermal_sensor_data
 **/
s32 txgbe_init_thermal_sensor_thresh(struct txgbe_hw *hw)
{
	struct txgbe_thermal_sensor_data *data = &hw->mac.thermal_sensor_data;

	memset(data, 0, sizeof(struct txgbe_thermal_sensor_data));

	if (hw->bus.lan_id != 0)
		return TXGBE_NOT_IMPLEMENTED;

	wr32(hw, TXGBE_TSCTRL, TXGBE_TSCTRL_EVALMD);
	wr32(hw, TXGBE_TSINTR,
		TXGBE_TSINTR_AEN | TXGBE_TSINTR_DEN);
	wr32(hw, TXGBE_TSEN, TXGBE_TSEN_ENA);


	data->sensor[0].alarm_thresh = 100;
	wr32(hw, TXGBE_TSATHRE, 677);
	data->sensor[0].dalarm_thresh = 90;
	wr32(hw, TXGBE_TSDTHRE, 614);

	return 0;
}

void txgbe_disable_rx(struct txgbe_hw *hw)
{
	u32 pfdtxgswc;

	pfdtxgswc = rd32(hw, TXGBE_PSRCTL);
	if (pfdtxgswc & TXGBE_PSRCTL_LBENA) {
		pfdtxgswc &= ~TXGBE_PSRCTL_LBENA;
		wr32(hw, TXGBE_PSRCTL, pfdtxgswc);
		hw->mac.set_lben = true;
	} else {
		hw->mac.set_lben = false;
	}

	wr32m(hw, TXGBE_PBRXCTL, TXGBE_PBRXCTL_ENA, 0);
	wr32m(hw, TXGBE_MACRXCFG, TXGBE_MACRXCFG_ENA, 0);
}

void txgbe_enable_rx(struct txgbe_hw *hw)
{
	u32 pfdtxgswc;

	wr32m(hw, TXGBE_MACRXCFG, TXGBE_MACRXCFG_ENA, TXGBE_MACRXCFG_ENA);
	wr32m(hw, TXGBE_PBRXCTL, TXGBE_PBRXCTL_ENA, TXGBE_PBRXCTL_ENA);

	if (hw->mac.set_lben) {
		pfdtxgswc = rd32(hw, TXGBE_PSRCTL);
		pfdtxgswc |= TXGBE_PSRCTL_LBENA;
		wr32(hw, TXGBE_PSRCTL, pfdtxgswc);
		hw->mac.set_lben = false;
	}
}

/**
 *  txgbe_setup_mac_link_multispeed_fiber - Set MAC link speed
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg_wait_to_complete: true when waiting for completion is needed
 *
 *  Set the link speed in the MAC and/or PHY register and restarts link.
 **/
s32 txgbe_setup_mac_link_multispeed_fiber(struct txgbe_hw *hw,
					  u32 speed,
					  bool autoneg_wait_to_complete)
{
	u32 link_speed = TXGBE_LINK_SPEED_UNKNOWN;
	u32 highest_link_speed = TXGBE_LINK_SPEED_UNKNOWN;
	s32 status = 0;
	u32 speedcnt = 0;
	u32 i = 0;
	bool autoneg, link_up = false;

	/* Mask off requested but non-supported speeds */
	status = hw->mac.get_link_capabilities(hw, &link_speed, &autoneg);
	if (status != 0)
		return status;

	speed &= link_speed;

	/* Try each speed one by one, highest priority first.  We do this in
	 * software because 10Gb fiber doesn't support speed autonegotiation.
	 */
	if (speed & TXGBE_LINK_SPEED_10GB_FULL) {
		speedcnt++;
		highest_link_speed = TXGBE_LINK_SPEED_10GB_FULL;

		/* Set the module link speed */
		switch (hw->phy.media_type) {
		case txgbe_media_type_fiber:
			hw->mac.set_rate_select_speed(hw,
				TXGBE_LINK_SPEED_10GB_FULL);
			break;
		case txgbe_media_type_fiber_qsfp:
			/* QSFP module automatically detects MAC link speed */
			break;
		default:
			DEBUGOUT("Unexpected media type.");
			break;
		}

		/* Allow module to change analog characteristics (1G->10G) */
		msec_delay(40);

		status = hw->mac.setup_mac_link(hw,
				TXGBE_LINK_SPEED_10GB_FULL,
				autoneg_wait_to_complete);
		if (status != 0)
			return status;

		/* Flap the Tx laser if it has not already been done */
		hw->mac.flap_tx_laser(hw);

		/* Wait for the controller to acquire link.  Per IEEE 802.3ap,
		 * Section 73.10.2, we may have to wait up to 500ms if KR is
		 * attempted.  uses the same timing for 10g SFI.
		 */
		for (i = 0; i < 5; i++) {
			/* Wait for the link partner to also set speed */
			msec_delay(100);

			/* If we have link, just jump out */
			status = hw->mac.check_link(hw, &link_speed,
				&link_up, false);
			if (status != 0)
				return status;

			if (link_up)
				goto out;
		}
	}

	if (speed & TXGBE_LINK_SPEED_1GB_FULL) {
		u32 curr_autoneg;

		speedcnt++;
		if (highest_link_speed == TXGBE_LINK_SPEED_UNKNOWN)
			highest_link_speed = TXGBE_LINK_SPEED_1GB_FULL;

		status = hw->mac.check_link(hw, &link_speed, &link_up, false);
		if (status != 0)
			return status;

		/* If we already have link at this speed, just jump out */
		if (link_speed == TXGBE_LINK_SPEED_1GB_FULL) {
			curr_autoneg = rd32_epcs(hw, SR_MII_MMD_CTL);
			if (link_up && (hw->autoneg ==
					!!(curr_autoneg & SR_MII_MMD_CTL_AN_EN)))
				goto out;
		}

		/* Set the module link speed */
		switch (hw->phy.media_type) {
		case txgbe_media_type_fiber:
			hw->mac.set_rate_select_speed(hw,
				TXGBE_LINK_SPEED_1GB_FULL);
			break;
		case txgbe_media_type_fiber_qsfp:
			/* QSFP module automatically detects link speed */
			break;
		default:
			DEBUGOUT("Unexpected media type.");
			break;
		}

		/* Allow module to change analog characteristics (10G->1G) */
		msec_delay(40);

		status = hw->mac.setup_mac_link(hw,
				TXGBE_LINK_SPEED_1GB_FULL,
				autoneg_wait_to_complete);
		if (status != 0)
			return status;

		/* Flap the Tx laser if it has not already been done */
		hw->mac.flap_tx_laser(hw);

		/* Wait for the link partner to also set speed */
		msec_delay(100);

		/* If we have link, just jump out */
		status = hw->mac.check_link(hw, &link_speed, &link_up, false);
		if (status != 0)
			return status;

		if (link_up)
			goto out;
	}

	/* We didn't get link.  Configure back to the highest speed we tried,
	 * (if there was more than one).  We call ourselves back with just the
	 * single highest speed that the user requested.
	 */
	if (speedcnt > 1)
		status = txgbe_setup_mac_link_multispeed_fiber(hw,
						      highest_link_speed,
						      autoneg_wait_to_complete);

out:
	/* Set autoneg_advertised value based on input link speed */
	hw->phy.autoneg_advertised = 0;

	if (speed & TXGBE_LINK_SPEED_10GB_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_10GB_FULL;

	if (speed & TXGBE_LINK_SPEED_1GB_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_1GB_FULL;

	return status;
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
	s32 status = 0;

	/*
	 * Set the mac type
	 */
	txgbe_set_mac_type(hw);

	txgbe_init_ops_dummy(hw);
	switch (hw->mac.type) {
	case txgbe_mac_sp:
		txgbe_init_ops_sp(hw);
		break;
	case txgbe_mac_aml:
		txgbe_init_ops_aml(hw);
		break;
	case txgbe_mac_aml40:
		txgbe_init_ops_aml40(hw);
		break;
	case txgbe_mac_sp_vf:
	case txgbe_mac_aml_vf:
		status = txgbe_init_ops_vf(hw);
		break;
	default:
		status = TXGBE_ERR_DEVICE_NOT_SUPPORTED;
		break;
	}
	hw->mac.max_link_up_time = TXGBE_LINK_UP_TIME;

	hw->bus.set_lan_id(hw);

	return status;
}

bool txgbe_is_pf(struct txgbe_hw *hw)
{
	switch (hw->mac.type) {
	case txgbe_mac_sp:
	case txgbe_mac_aml:
	case txgbe_mac_aml40:
		return true;
	default:
		return false;
	}
}

bool txgbe_is_vf(struct txgbe_hw *hw)
{
	switch (hw->mac.type) {
	case txgbe_mac_sp_vf:
	case txgbe_mac_aml_vf:
		return true;
	default:
		return false;
	}
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

	if (hw->vendor_id != PCI_VENDOR_ID_WANGXUN) {
		DEBUGOUT("Unsupported vendor id: %x", hw->vendor_id);
		return TXGBE_ERR_DEVICE_NOT_SUPPORTED;
	}

	switch (hw->device_id) {
	case TXGBE_DEV_ID_SP1000:
	case TXGBE_DEV_ID_WX1820:
		hw->mac.type = txgbe_mac_sp;
		break;
	case TXGBE_DEV_ID_AML:
	case TXGBE_DEV_ID_AML5025:
	case TXGBE_DEV_ID_AML5125:
		hw->mac.type = txgbe_mac_aml;
		break;
	case TXGBE_DEV_ID_AML5040:
	case TXGBE_DEV_ID_AML5140:
		hw->mac.type = txgbe_mac_aml40;
		break;
	case TXGBE_DEV_ID_SP1000_VF:
	case TXGBE_DEV_ID_WX1820_VF:
		hw->phy.media_type = txgbe_media_type_virtual;
		hw->mac.type = txgbe_mac_sp_vf;
		break;
	case TXGBE_DEV_ID_AML_VF:
	case TXGBE_DEV_ID_AML5024_VF:
	case TXGBE_DEV_ID_AML5124_VF:
		hw->phy.media_type = txgbe_media_type_virtual;
		hw->mac.type = txgbe_mac_aml_vf;
		break;
	default:
		err = TXGBE_ERR_DEVICE_NOT_SUPPORTED;
		DEBUGOUT("Unsupported device id: %x", hw->device_id);
		break;
	}

	DEBUGOUT("found mac: %d, returns: %d",
		  hw->mac.type, err);
	return err;
}

void txgbe_init_mac_link_ops_sp(struct txgbe_hw *hw)
{
	struct txgbe_mac_info *mac = &hw->mac;

	/*
	 * enable the laser control functions for SFP+ fiber
	 * and MNG not enabled
	 */
	if (hw->phy.media_type == txgbe_media_type_fiber &&
	    !txgbe_mng_enabled(hw)) {
		mac->disable_tx_laser =
			txgbe_disable_tx_laser_multispeed_fiber;
		mac->enable_tx_laser =
			txgbe_enable_tx_laser_multispeed_fiber;
		mac->flap_tx_laser =
			txgbe_flap_tx_laser_multispeed_fiber;
	}

	if ((hw->phy.media_type == txgbe_media_type_fiber ||
	     hw->phy.media_type == txgbe_media_type_fiber_qsfp) &&
	    hw->phy.multispeed_fiber) {
		/* Set up dual speed SFP+ support */
		mac->setup_link = txgbe_setup_mac_link_multispeed_fiber;
		mac->setup_mac_link = txgbe_setup_mac_link;
		mac->set_rate_select_speed = txgbe_set_hard_rate_select_speed;
	} else {
		mac->setup_link = txgbe_setup_mac_link;
		mac->set_rate_select_speed = txgbe_set_hard_rate_select_speed;
	}
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
	struct txgbe_mac_info *mac = &hw->mac;
	struct txgbe_phy_info *phy = &hw->phy;
	s32 err = 0;

	if ((hw->device_id & 0xFF) == TXGBE_DEV_ID_QSFP) {
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
	hw->mac.init_mac_link_ops(hw);

	/* If copper media, overwrite with copper function pointers */
	if (phy->media_type == txgbe_media_type_copper) {
		mac->setup_link = txgbe_setup_copper_link_raptor;
		mac->get_link_capabilities =
				  txgbe_get_copper_link_capabilities;
	}

	if (phy->media_type == txgbe_media_type_backplane) {
		mac->kr_handle = txgbe_kr_handle;
		mac->bp_down_event = txgbe_bp_down_event;
	}

	/* Set necessary function pointers based on PHY type */
	switch (hw->phy.type) {
	case txgbe_phy_tn:
		phy->setup_link = txgbe_setup_phy_link_tnx;
		phy->check_link = txgbe_check_phy_link_tnx;
		break;
	default:
		break;
	}

init_phy_ops_out:
	return err;
}

s32 txgbe_setup_sfp_modules(struct txgbe_hw *hw)
{
	s32 err = 0;

	if (hw->phy.sfp_type == txgbe_sfp_type_unknown)
		return 0;

	hw->mac.init_mac_link_ops(hw);

	/* PHY config will finish before releasing the semaphore */
	err = hw->mac.acquire_swfw_sync(hw, TXGBE_MNGSEM_SWPHY);
	if (err != 0)
		return TXGBE_ERR_SWFW_SYNC;

	/* Release the semaphore */
	hw->mac.release_swfw_sync(hw, TXGBE_MNGSEM_SWPHY);

	/* Delay obtaining semaphore again to allow FW access
	 * prot_autoc_write uses the semaphore too.
	 */
	msec_delay(hw->rom.semaphore_delay);

	if (err) {
		DEBUGOUT("sfp module setup not complete");
		return TXGBE_ERR_SFP_SETUP_NOT_COMPLETE;
	}

	return err;
}

/**
 *  txgbe_prot_autoc_read_raptor - Hides MAC differences needed for AUTOC read
 *  @hw: pointer to hardware structure
 *  @locked: Return the if we locked for this read.
 *  @value: Value we read from AUTOC
 *
 *  For this part we need to wrap read-modify-writes with a possible
 *  FW/SW lock.  It is assumed this lock will be freed with the next
 *  prot_autoc_write_raptor().
 */
s32 txgbe_prot_autoc_read_raptor(struct txgbe_hw *hw, bool *locked, u64 *value)
{
	s32 err;
	bool lock_state = false;

	 /* If LESM is on then we need to hold the SW/FW semaphore. */
	if (txgbe_verify_lesm_fw_enabled_raptor(hw)) {
		err = hw->mac.acquire_swfw_sync(hw,
					TXGBE_MNGSEM_SWPHY);
		if (err != 0)
			return TXGBE_ERR_SWFW_SYNC;

		lock_state = true;
	}

	if (locked)
		*locked = lock_state;

	*value = txgbe_autoc_read(hw);
	return 0;
}

/**
 * txgbe_prot_autoc_write_raptor - Hides MAC differences needed for AUTOC write
 * @hw: pointer to hardware structure
 * @autoc: value to write to AUTOC
 * @locked: bool to indicate whether the SW/FW lock was already taken by
 *           previous prot_autoc_read_raptor.
 *
 * This part may need to hold the SW/FW lock around all writes to
 * AUTOC. Likewise after a write we need to do a pipeline reset.
 */
s32 txgbe_prot_autoc_write_raptor(struct txgbe_hw *hw, bool locked, u64 autoc)
{
	int err = 0;

	/* Blocked by MNG FW so bail */
	if (txgbe_check_reset_blocked(hw))
		goto out;

	/* We only need to get the lock if:
	 *  - We didn't do it already (in the read part of a read-modify-write)
	 *  - LESM is enabled.
	 */
	if (!locked && txgbe_verify_lesm_fw_enabled_raptor(hw)) {
		err = hw->mac.acquire_swfw_sync(hw,
					TXGBE_MNGSEM_SWPHY);
		if (err != 0)
			return TXGBE_ERR_SWFW_SYNC;

		locked = true;
	}

	txgbe_autoc_write(hw, autoc);
	err = txgbe_reset_pipeline_raptor(hw);

out:
	/* Free the SW/FW semaphore as we either grabbed it here or
	 * already had it when this function was called.
	 */
	if (locked)
		hw->mac.release_swfw_sync(hw, TXGBE_MNGSEM_SWPHY);

	return err;
}

/* cmd_addr is used for some special command:
 * 1. to be sector address, when implemented erase sector command
 * 2. to be flash address when implemented read, write flash address
 *
 * Return 0 on success, return TXGBE_ERR_TIMEOUT on failure.
 */
s32 txgbe_fmgr_cmd_op(struct txgbe_hw *hw, u32 cmd, u32 cmd_addr)
{
	u32 cmd_val, i;

	cmd_val = TXGBE_SPICMD_CMD(cmd) | TXGBE_SPICMD_CLK(3) | cmd_addr;
	wr32(hw, TXGBE_SPICMD, cmd_val);

	for (i = 0; i < TXGBE_SPI_TIMEOUT; i++) {
		if (rd32(hw, TXGBE_SPISTAT) & TXGBE_SPISTAT_OPDONE)
			break;

		usec_delay(10);
	}

	if (i == TXGBE_SPI_TIMEOUT)
		return TXGBE_ERR_TIMEOUT;

	return 0;
}

s32 txgbe_flash_read_dword(struct txgbe_hw *hw, u32 addr, u32 *data)
{
	s32 status;

	status = txgbe_fmgr_cmd_op(hw, 1, addr);
	if (status < 0) {
		DEBUGOUT("Read flash timeout.");
		return status;
	}

	*data = rd32(hw, TXGBE_SPIDAT);

	return 0;
}

/**
 *  txgbe_init_ops_generic - Inits func ptrs and MAC type
 *  @hw: pointer to hardware structure
 *
 *  Initialize the function pointers and assign the MAC type.
 *  Does not touch the hardware.
 **/
s32 txgbe_init_ops_generic(struct txgbe_hw *hw)
{
	struct txgbe_bus_info *bus = &hw->bus;
	struct txgbe_mac_info *mac = &hw->mac;
	struct txgbe_phy_info *phy = &hw->phy;
	struct txgbe_rom_info *rom = &hw->rom;
	struct txgbe_mbx_info *mbx = &hw->mbx;

	/* BUS */
	bus->set_lan_id = txgbe_set_lan_id_multi_port;

	/* PHY */
	phy->identify = txgbe_identify_phy;
	phy->init = txgbe_init_phy_raptor;
	phy->read_reg = txgbe_read_phy_reg;
	phy->write_reg = txgbe_write_phy_reg;
	phy->read_reg_mdi = txgbe_read_phy_reg_mdi;
	phy->write_reg_mdi = txgbe_write_phy_reg_mdi;
	phy->setup_link = txgbe_setup_phy_link;
	phy->setup_link_speed = txgbe_setup_phy_link_speed;
	phy->get_fw_version = txgbe_get_phy_fw_version;
	phy->read_i2c_byte = txgbe_read_i2c_byte;
	phy->write_i2c_byte = txgbe_write_i2c_byte;
	phy->read_i2c_sff8472 = txgbe_read_i2c_sff8472;
	phy->read_i2c_eeprom = txgbe_read_i2c_eeprom;
	phy->write_i2c_eeprom = txgbe_write_i2c_eeprom;
	phy->identify_sfp = txgbe_identify_module;
	phy->read_i2c_byte_unlocked = txgbe_read_i2c_byte_unlocked;
	phy->write_i2c_byte_unlocked = txgbe_write_i2c_byte_unlocked;
	phy->check_overtemp = txgbe_check_overtemp;
	phy->reset = txgbe_reset_phy;
	phy->set_link_hostif = txgbe_hic_ephy_set_link;

	/* MAC */
	mac->init_hw = txgbe_init_hw;
	mac->start_hw = txgbe_start_hw_raptor;
	mac->clear_hw_cntrs = txgbe_clear_hw_cntrs;
	mac->enable_rx_dma = txgbe_enable_rx_dma_raptor;
	mac->get_mac_addr = txgbe_get_mac_addr;
	mac->stop_hw = txgbe_stop_hw;
	mac->acquire_swfw_sync = txgbe_acquire_swfw_sync;
	mac->release_swfw_sync = txgbe_release_swfw_sync;
	mac->reset_hw = txgbe_reset_hw;
	mac->update_mc_addr_list = txgbe_update_mc_addr_list;

	mac->disable_sec_rx_path = txgbe_disable_sec_rx_path;
	mac->enable_sec_rx_path = txgbe_enable_sec_rx_path;
	mac->disable_sec_tx_path = txgbe_disable_sec_tx_path;
	mac->enable_sec_tx_path = txgbe_enable_sec_tx_path;
	mac->get_san_mac_addr = txgbe_get_san_mac_addr;
	mac->set_san_mac_addr = txgbe_set_san_mac_addr;
	mac->get_device_caps = txgbe_get_device_caps;
	mac->get_wwn_prefix = txgbe_get_wwn_prefix;
	mac->autoc_read = txgbe_autoc_read;
	mac->autoc_write = txgbe_autoc_write;
	mac->prot_autoc_read = txgbe_prot_autoc_read_raptor;
	mac->prot_autoc_write = txgbe_prot_autoc_write_raptor;

	/* RAR, Multicast, VLAN */
	mac->set_rar = txgbe_set_rar;
	mac->clear_rar = txgbe_clear_rar;
	mac->init_rx_addrs = txgbe_init_rx_addrs;
	mac->enable_rx = txgbe_enable_rx;
	mac->disable_rx = txgbe_disable_rx;
	mac->set_vmdq = txgbe_set_vmdq;
	mac->clear_vmdq = txgbe_clear_vmdq;
	mac->set_vfta = txgbe_set_vfta;
	mac->set_vlvf = txgbe_set_vlvf;
	mac->clear_vfta = txgbe_clear_vfta;
	mac->init_uta_tables = txgbe_init_uta_tables;
	mac->setup_sfp = txgbe_setup_sfp_modules;
	mac->set_mac_anti_spoofing = txgbe_set_mac_anti_spoofing;
	mac->set_ethertype_anti_spoofing = txgbe_set_ethertype_anti_spoofing;

	/* Flow Control */
	mac->fc_enable = txgbe_fc_enable;
	mac->setup_fc = txgbe_setup_fc;
	mac->fc_autoneg = txgbe_fc_autoneg;

	/* Link */
	mac->setup_pba = txgbe_set_pba;

	/* Manageability interface */
	mac->set_fw_drv_ver = txgbe_hic_set_drv_ver;
	mac->get_thermal_sensor_data = txgbe_get_thermal_sensor_data;
	mac->init_thermal_sensor_thresh = txgbe_init_thermal_sensor_thresh;

	mbx->init_params = txgbe_init_mbx_params_pf;
	mbx->read = txgbe_read_mbx_pf;
	mbx->write = txgbe_write_mbx_pf;
	mbx->check_for_msg = txgbe_check_for_msg_pf;
	mbx->check_for_ack = txgbe_check_for_ack_pf;
	mbx->check_for_rst = txgbe_check_for_rst_pf;

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

	mac->mcft_size		= TXGBE_RAPTOR_MC_TBL_SIZE;
	mac->vft_size		= TXGBE_RAPTOR_VFT_TBL_SIZE;
	mac->num_rar_entries	= TXGBE_RAPTOR_RAR_ENTRIES;
	mac->rx_pb_size		= TXGBE_RAPTOR_RX_PB_SIZE;
	mac->max_rx_queues	= TXGBE_RAPTOR_MAX_RX_QUEUES;
	mac->max_tx_queues	= TXGBE_RAPTOR_MAX_TX_QUEUES;

	if (hw->mac.type == txgbe_mac_aml || hw->mac.type == txgbe_mac_aml40)
		mac->rx_pb_size = TXGBE_AML_RX_PB_SIZE;

	return 0;
}

void txgbe_init_ops_sp(struct txgbe_hw *hw)
{
	struct txgbe_mac_info *mac = &hw->mac;
	struct txgbe_phy_info *phy = &hw->phy;

	txgbe_init_ops_generic(hw);

	/* PHY */
	phy->get_media_type = txgbe_get_media_type_sp;

	/* LINK */
	mac->init_mac_link_ops = txgbe_init_mac_link_ops_sp;
	mac->get_link_capabilities = txgbe_get_link_capabilities_sp;
	mac->check_link = txgbe_check_mac_link_sp;
}

/**
 *  txgbe_get_link_capabilities_sp - Determines link capabilities
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @autoneg: true when autoneg or autotry is enabled
 *
 *  Determines the link capabilities by reading the AUTOC register.
 **/
s32 txgbe_get_link_capabilities_sp(struct txgbe_hw *hw,
				      u32 *speed,
				      bool *autoneg)
{
	s32 status = 0;
	u32 autoc = 0;

	/* Check if 1G SFP module. */
	if (hw->phy.sfp_type == txgbe_sfp_type_1g_cu_core0 ||
	    hw->phy.sfp_type == txgbe_sfp_type_1g_cu_core1 ||
	    hw->phy.sfp_type == txgbe_sfp_type_1g_lx_core0 ||
	    hw->phy.sfp_type == txgbe_sfp_type_1g_lx_core1 ||
	    hw->phy.sfp_type == txgbe_sfp_type_1g_sx_core0 ||
	    hw->phy.sfp_type == txgbe_sfp_type_1g_sx_core1) {
		*speed = TXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = true;
		return 0;
	}

	/*
	 * Determine link capabilities based on the stored value of AUTOC,
	 * which represents EEPROM defaults.  If AUTOC value has not
	 * been stored, use the current register values.
	 */
	if (hw->mac.orig_link_settings_stored)
		autoc = hw->mac.orig_autoc;
	else
		autoc = hw->mac.autoc_read(hw);

	switch (autoc & TXGBE_AUTOC_LMS_MASK) {
	case TXGBE_AUTOC_LMS_1G_LINK_NO_AN:
		*speed = TXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = false;
		break;

	case TXGBE_AUTOC_LMS_10G_LINK_NO_AN:
		*speed = TXGBE_LINK_SPEED_10GB_FULL;
		*autoneg = false;
		break;

	case TXGBE_AUTOC_LMS_1G_AN:
		*speed = TXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = true;
		break;

	case TXGBE_AUTOC_LMS_10G:
		*speed = TXGBE_LINK_SPEED_10GB_FULL;
		*autoneg = false;
		break;

	case TXGBE_AUTOC_LMS_KX4_KX_KR:
	case TXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN:
		*speed = TXGBE_LINK_SPEED_UNKNOWN;
		if (autoc & TXGBE_AUTOC_KR_SUPP)
			*speed |= TXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & TXGBE_AUTOC_KX4_SUPP)
			*speed |= TXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & TXGBE_AUTOC_KX_SUPP)
			*speed |= TXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = true;
		break;

	case TXGBE_AUTOC_LMS_KX4_KX_KR_SGMII:
		*speed = TXGBE_LINK_SPEED_100M_FULL;
		if (autoc & TXGBE_AUTOC_KR_SUPP)
			*speed |= TXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & TXGBE_AUTOC_KX4_SUPP)
			*speed |= TXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & TXGBE_AUTOC_KX_SUPP)
			*speed |= TXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = true;
		break;

	case TXGBE_AUTOC_LMS_SGMII_1G_100M:
		*speed = TXGBE_LINK_SPEED_1GB_FULL |
			 TXGBE_LINK_SPEED_100M_FULL |
			 TXGBE_LINK_SPEED_10M_FULL;
		*autoneg = false;
		break;

	default:
		return TXGBE_ERR_LINK_SETUP;
	}

	if (hw->phy.multispeed_fiber) {
		*speed |= TXGBE_LINK_SPEED_10GB_FULL |
			  TXGBE_LINK_SPEED_1GB_FULL;

		/* QSFP must not enable full auto-negotiation
		 * Limited autoneg is enabled at 1G
		 */
		if (hw->phy.media_type == txgbe_media_type_fiber_qsfp)
			*autoneg = false;
		else
			*autoneg = true;
	}

	return status;
}

/**
 *  txgbe_get_media_type_sp - Get media type
 *  @hw: pointer to hardware structure
 *
 *  Returns the media type (fiber, copper, backplane)
 **/
u32 txgbe_get_media_type_sp(struct txgbe_hw *hw)
{
	u32 media_type;

	if (hw->phy.ffe_set)
		txgbe_bp_mode_set(hw);

	/* Detect if there is a copper PHY attached. */
	switch (hw->phy.type) {
	case txgbe_phy_cu_unknown:
	case txgbe_phy_tn:
		media_type = txgbe_media_type_copper;
		return media_type;
	default:
		break;
	}

	switch (hw->subsystem_device_id & 0xFF) {
	case TXGBE_DEV_ID_KR_KX_KX4:
	case TXGBE_DEV_ID_MAC_SGMII:
	case TXGBE_DEV_ID_MAC_XAUI:
		/* Default device ID is mezzanine card KX/KX4 */
		media_type = txgbe_media_type_backplane;
		break;
	case TXGBE_DEV_ID_SFP:
		media_type = txgbe_media_type_fiber;
		break;
	case TXGBE_DEV_ID_QSFP:
		media_type = txgbe_media_type_fiber_qsfp;
		break;
	case TXGBE_DEV_ID_XAUI:
	case TXGBE_DEV_ID_SGMII:
		media_type = txgbe_media_type_copper;
		break;
	case TXGBE_DEV_ID_SFI_XAUI:
		if (hw->bus.lan_id == 0)
			media_type = txgbe_media_type_fiber;
		else
			media_type = txgbe_media_type_copper;
		break;
	default:
		media_type = txgbe_media_type_unknown;
		break;
	}

	return media_type;
}

/**
 *  txgbe_start_mac_link_raptor - Setup MAC link settings
 *  @hw: pointer to hardware structure
 *  @autoneg_wait_to_complete: true when waiting for completion is needed
 *
 *  Configures link settings based on values in the txgbe_hw struct.
 *  Restarts the link.  Performs autonegotiation if needed.
 **/
s32 txgbe_start_mac_link_raptor(struct txgbe_hw *hw,
			       bool autoneg_wait_to_complete)
{
	s32 status = 0;
	bool got_lock = false;

	UNREFERENCED_PARAMETER(autoneg_wait_to_complete);

	/*  reset_pipeline requires us to hold this lock as it writes to
	 *  AUTOC.
	 */
	if (txgbe_verify_lesm_fw_enabled_raptor(hw)) {
		status = hw->mac.acquire_swfw_sync(hw, TXGBE_MNGSEM_SWPHY);
		if (status != 0)
			goto out;

		got_lock = true;
	}

	/* Restart link */
	txgbe_reset_pipeline_raptor(hw);

	if (got_lock)
		hw->mac.release_swfw_sync(hw, TXGBE_MNGSEM_SWPHY);

	/* Add delay to filter out noises during initial link setup */
	msec_delay(50);

out:
	return status;
}

/**
 *  txgbe_disable_tx_laser_multispeed_fiber - Disable Tx laser
 *  @hw: pointer to hardware structure
 *
 *  The base drivers may require better control over SFP+ module
 *  PHY states.  This includes selectively shutting down the Tx
 *  laser on the PHY, effectively halting physical link.
 **/
void txgbe_disable_tx_laser_multispeed_fiber(struct txgbe_hw *hw)
{
	u32 esdp_reg = rd32(hw, TXGBE_GPIODATA);

	if (txgbe_close_notify(hw))
		txgbe_led_off(hw, TXGBE_LEDCTL_UP | TXGBE_LEDCTL_10G |
				TXGBE_LEDCTL_1G | TXGBE_LEDCTL_ACTIVE);

	/* Disable Tx laser; allow 100us to go dark per spec */
	esdp_reg |= (TXGBE_GPIOBIT_0 | TXGBE_GPIOBIT_1);
	wr32(hw, TXGBE_GPIODATA, esdp_reg);
	txgbe_flush(hw);
	usec_delay(100);
}

/**
 *  txgbe_enable_tx_laser_multispeed_fiber - Enable Tx laser
 *  @hw: pointer to hardware structure
 *
 *  The base drivers may require better control over SFP+ module
 *  PHY states.  This includes selectively turning on the Tx
 *  laser on the PHY, effectively starting physical link.
 **/
void txgbe_enable_tx_laser_multispeed_fiber(struct txgbe_hw *hw)
{
	u32 esdp_reg = rd32(hw, TXGBE_GPIODATA);

	if (txgbe_open_notify(hw))
		wr32(hw, TXGBE_LEDCTL, 0);

	/* Enable Tx laser; allow 100ms to light up */
	esdp_reg &= ~(TXGBE_GPIOBIT_0 | TXGBE_GPIOBIT_1);
	wr32(hw, TXGBE_GPIODATA, esdp_reg);
	txgbe_flush(hw);
	msec_delay(100);
}

/**
 *  txgbe_flap_tx_laser_multispeed_fiber - Flap Tx laser
 *  @hw: pointer to hardware structure
 *
 *  When the driver changes the link speeds that it can support,
 *  it sets autotry_restart to true to indicate that we need to
 *  initiate a new autotry session with the link partner.  To do
 *  so, we set the speed then disable and re-enable the Tx laser, to
 *  alert the link partner that it also needs to restart autotry on its
 *  end.  This is consistent with true clause 37 autoneg, which also
 *  involves a loss of signal.
 **/
void txgbe_flap_tx_laser_multispeed_fiber(struct txgbe_hw *hw)
{
	if (hw->mac.autotry_restart) {
		txgbe_disable_tx_laser_multispeed_fiber(hw);
		txgbe_enable_tx_laser_multispeed_fiber(hw);
		hw->mac.autotry_restart = false;
	}
}

/**
 *  txgbe_set_hard_rate_select_speed - Set module link speed
 *  @hw: pointer to hardware structure
 *  @speed: link speed to set
 *
 *  Set module link speed via RS0/RS1 rate select pins.
 */
void txgbe_set_hard_rate_select_speed(struct txgbe_hw *hw,
					u32 speed)
{
	u32 esdp_reg = rd32(hw, TXGBE_GPIODATA);

	switch (speed) {
	case TXGBE_LINK_SPEED_10GB_FULL:
		esdp_reg |= (TXGBE_GPIOBIT_4 | TXGBE_GPIOBIT_5);
		break;
	case TXGBE_LINK_SPEED_1GB_FULL:
		esdp_reg &= ~(TXGBE_GPIOBIT_4 | TXGBE_GPIOBIT_5);
		break;
	default:
		DEBUGOUT("Invalid fixed module speed");
		return;
	}

	wr32(hw, TXGBE_GPIODATA, esdp_reg);
	txgbe_flush(hw);
}

/**
 *  txgbe_setup_mac_link_smartspeed - Set MAC link speed using SmartSpeed
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg_wait_to_complete: true when waiting for completion is needed
 *
 *  Implements the Intel SmartSpeed algorithm.
 **/
s32 txgbe_setup_mac_link_smartspeed(struct txgbe_hw *hw,
				    u32 speed,
				    bool autoneg_wait_to_complete)
{
	s32 status = 0;
	u32 link_speed = TXGBE_LINK_SPEED_UNKNOWN;
	s32 i, j;
	bool link_up = false;
	u32 autoc_reg = rd32_epcs(hw, SR_AN_MMD_ADV_REG1);

	 /* Set autoneg_advertised value based on input link speed */
	hw->phy.autoneg_advertised = 0;

	if (speed & TXGBE_LINK_SPEED_10GB_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_10GB_FULL;

	if (speed & TXGBE_LINK_SPEED_1GB_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_1GB_FULL;

	if (speed & TXGBE_LINK_SPEED_100M_FULL)
		hw->phy.autoneg_advertised |= TXGBE_LINK_SPEED_100M_FULL;

	/*
	 * Implement Intel SmartSpeed algorithm.  SmartSpeed will reduce the
	 * autoneg advertisement if link is unable to be established at the
	 * highest negotiated rate.  This can sometimes happen due to integrity
	 * issues with the physical media connection.
	 */

	/* First, try to get link with full advertisement */
	hw->phy.smart_speed_active = false;
	for (j = 0; j < TXGBE_SMARTSPEED_MAX_RETRIES; j++) {
		status = txgbe_setup_mac_link(hw, speed,
						    autoneg_wait_to_complete);
		if (status != 0)
			goto out;

		/*
		 * Wait for the controller to acquire link.  Per IEEE 802.3ap,
		 * Section 73.10.2, we may have to wait up to 500ms if KR is
		 * attempted, or 200ms if KX/KX4/BX/BX4 is attempted, per
		 * Table 9 in the AN MAS.
		 */
		for (i = 0; i < 5; i++) {
			msec_delay(100);

			/* If we have link, just jump out */
			status = hw->mac.check_link(hw, &link_speed, &link_up,
						  false);
			if (status != 0)
				goto out;

			if (link_up)
				goto out;
		}
	}

	/*
	 * We didn't get link.  If we advertised KR plus one of KX4/KX
	 * (or BX4/BX), then disable KR and try again.
	 */
	if (((autoc_reg & TXGBE_AUTOC_KR_SUPP) == 0) ||
	    ((autoc_reg & TXGBE_AUTOC_KX_SUPP) == 0 &&
	     (autoc_reg & TXGBE_AUTOC_KX4_SUPP) == 0))
		goto out;

	/* Turn SmartSpeed on to disable KR support */
	hw->phy.smart_speed_active = true;
	status = txgbe_setup_mac_link(hw, speed,
					    autoneg_wait_to_complete);
	if (status != 0)
		goto out;

	/*
	 * Wait for the controller to acquire link.  600ms will allow for
	 * the AN link_fail_inhibit_timer as well for multiple cycles of
	 * parallel detect, both 10g and 1g. This allows for the maximum
	 * connect attempts as defined in the AN MAS table 73-7.
	 */
	for (i = 0; i < 6; i++) {
		msec_delay(100);

		/* If we have link, just jump out */
		status = hw->mac.check_link(hw, &link_speed, &link_up, false);
		if (status != 0)
			goto out;

		if (link_up)
			goto out;
	}

	/* We didn't get link.  Turn SmartSpeed back off. */
	hw->phy.smart_speed_active = false;
	status = txgbe_setup_mac_link(hw, speed,
					    autoneg_wait_to_complete);

out:
	if (link_up && link_speed == TXGBE_LINK_SPEED_1GB_FULL)
		DEBUGOUT("Smartspeed has downgraded the link speed from the maximum advertised");
	return status;
}

/**
 *  txgbe_setup_mac_link - Set MAC link speed
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg_wait_to_complete: true when waiting for completion is needed
 *
 *  Set the link speed in the AUTOC register and restarts link.
 **/
s32 txgbe_setup_mac_link(struct txgbe_hw *hw,
			       u32 speed,
			       bool autoneg_wait_to_complete)
{
	bool autoneg = false;
	s32 status = 0;

	u64 autoc = hw->mac.autoc_read(hw);
	u64 pma_pmd_10gs = autoc & TXGBE_AUTOC_10GS_PMA_PMD_MASK;
	u64 pma_pmd_1g = autoc & TXGBE_AUTOC_1G_PMA_PMD_MASK;
	u64 link_mode = autoc & TXGBE_AUTOC_LMS_MASK;
	u64 orig_autoc = 0;
	u32 link_capabilities = TXGBE_LINK_SPEED_UNKNOWN;

	UNREFERENCED_PARAMETER(autoneg_wait_to_complete);

	/* Check to see if speed passed in is supported. */
	status = hw->mac.get_link_capabilities(hw,
			&link_capabilities, &autoneg);
	if (status)
		return status;

	speed &= link_capabilities;
	if (speed == TXGBE_LINK_SPEED_UNKNOWN)
		return TXGBE_ERR_LINK_SETUP;

	/* Use stored value (EEPROM defaults) of AUTOC to find KR/KX4 support*/
	if (hw->mac.orig_link_settings_stored)
		orig_autoc = hw->mac.orig_autoc;
	else
		orig_autoc = autoc;

	link_mode = autoc & TXGBE_AUTOC_LMS_MASK;
	pma_pmd_1g = autoc & TXGBE_AUTOC_1G_PMA_PMD_MASK;

	if (link_mode == TXGBE_AUTOC_LMS_KX4_KX_KR ||
	    link_mode == TXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN ||
	    link_mode == TXGBE_AUTOC_LMS_KX4_KX_KR_SGMII) {
		/* Set KX4/KX/KR support according to speed requested */
		autoc &= ~(TXGBE_AUTOC_KX_SUPP |
			   TXGBE_AUTOC_KX4_SUPP |
			   TXGBE_AUTOC_KR_SUPP);
		if (speed & TXGBE_LINK_SPEED_10GB_FULL) {
			if (orig_autoc & TXGBE_AUTOC_KX4_SUPP)
				autoc |= TXGBE_AUTOC_KX4_SUPP;
			if (orig_autoc & TXGBE_AUTOC_KR_SUPP)
				autoc |= TXGBE_AUTOC_KR_SUPP;
		}
		if (speed & TXGBE_LINK_SPEED_1GB_FULL)
			autoc |= TXGBE_AUTOC_KX_SUPP;
	} else if ((pma_pmd_1g == TXGBE_AUTOC_1G_SFI) &&
		   (link_mode == TXGBE_AUTOC_LMS_1G_LINK_NO_AN ||
		    link_mode == TXGBE_AUTOC_LMS_1G_AN)) {
		/* Switch from 1G SFI to 10G SFI if requested */
		if (speed == TXGBE_LINK_SPEED_10GB_FULL &&
		    pma_pmd_10gs == TXGBE_AUTOC_10GS_SFI) {
			autoc &= ~TXGBE_AUTOC_LMS_MASK;
			autoc |= TXGBE_AUTOC_LMS_10G;
		}
	} else if ((pma_pmd_10gs == TXGBE_AUTOC_10GS_SFI) &&
		   (link_mode == TXGBE_AUTOC_LMS_10G)) {
		/* Switch from 10G SFI to 1G SFI if requested */
		if (speed == TXGBE_LINK_SPEED_1GB_FULL &&
		    pma_pmd_1g == TXGBE_AUTOC_1G_SFI) {
			autoc &= ~TXGBE_AUTOC_LMS_MASK;
			if (autoneg || hw->phy.type == txgbe_phy_qsfp_intel)
				autoc |= TXGBE_AUTOC_LMS_1G_AN;
			else
				autoc |= TXGBE_AUTOC_LMS_1G_LINK_NO_AN;
		}
	}

	autoc &= ~TXGBE_AUTOC_SPEED_MASK;
	autoc |= TXGBE_AUTOC_SPEED(speed);
	autoc &= ~TXGBE_AUTOC_AUTONEG;
	autoc |= (autoneg ? TXGBE_AUTOC_AUTONEG : 0);

	/* Restart link */
	hw->mac.autoc_write(hw, autoc);

	/* Add delay to filter out noises during initial link setup */
	msec_delay(50);

	return status;
}

/**
 *  txgbe_setup_copper_link_raptor - Set the PHY autoneg advertised field
 *  @hw: pointer to hardware structure
 *  @speed: new link speed
 *  @autoneg_wait_to_complete: true if waiting is needed to complete
 *
 *  Restarts link on PHY and MAC based on settings passed in.
 **/
static s32 txgbe_setup_copper_link_raptor(struct txgbe_hw *hw,
					 u32 speed,
					 bool autoneg_wait_to_complete)
{
	s32 status;

	/* Setup the PHY according to input speed */
	status = hw->phy.setup_link_speed(hw, speed,
					      autoneg_wait_to_complete);
	/* Set up MAC */
	txgbe_start_mac_link_raptor(hw, autoneg_wait_to_complete);

	return status;
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

int txgbe_reconfig_mac(struct txgbe_hw *hw)
{
	u32 mac_wdg_timeout;
	u32 mac_flow_ctrl;

	mac_wdg_timeout = rd32(hw, TXGBE_MAC_WDG_TIMEOUT);
	mac_flow_ctrl = rd32(hw, TXGBE_RXFCCFG);

	if (hw->bus.lan_id == 0)
		wr32(hw, TXGBE_RST, TXGBE_RST_MAC_LAN_0);
	else if (hw->bus.lan_id == 1)
		wr32(hw, TXGBE_RST, TXGBE_RST_MAC_LAN_1);

	/* wait for mac reset complete */
	usec_delay(1500);
	wr32m(hw, TXGBE_MAC_MISC_CTL, TXGBE_MAC_MISC_LINK_STS_MOD,
		TXGBE_LINK_BOTH_PCS_MAC);

	/* receive packets that size > 2048 */
	wr32m(hw, TXGBE_MACRXCFG,
		TXGBE_MACRXCFG_JUMBO, TXGBE_MACRXCFG_JUMBO);

	/* clear counters on read */
	wr32m(hw, TXGBE_MACCNTCTL,
		TXGBE_MACCNTCTL_RC, TXGBE_MACCNTCTL_RC);

	wr32m(hw, TXGBE_RXFCCFG,
		TXGBE_RXFCCFG_FC, TXGBE_RXFCCFG_FC);

	wr32(hw, TXGBE_MACRXFLT, TXGBE_MACRXFLT_PROMISC);

	wr32(hw, TXGBE_MAC_WDG_TIMEOUT, mac_wdg_timeout);
	wr32(hw, TXGBE_RXFCCFG, mac_flow_ctrl);

	return 0;
}

static void
txgbe_reset_misc(struct txgbe_hw *hw)
{
	int i;
	u32 value;
	int err = 0;
	u32 speed;

	wr32(hw, TXGBE_ISBADDRL, hw->isb_dma & 0x00000000FFFFFFFF);
	wr32(hw, TXGBE_ISBADDRH, hw->isb_dma >> 32);

	if (hw->mac.type == txgbe_mac_aml) {
		if ((rd32(hw, TXGBE_EPHY_STAT) & TXGBE_EPHY_STAT_PPL_LOCK)
						!= TXGBE_EPHY_STAT_PPL_LOCK) {
			speed = TXGBE_LINK_SPEED_25GB_FULL
			      | TXGBE_LINK_SPEED_10GB_FULL;
			err = hw->mac.setup_link(hw, speed, false);
			if (err) {
				DEBUGOUT("setup phy failed");
				return;
			}
		}
	} else if (hw->mac.type == txgbe_mac_aml40) {
		if (!(rd32(hw, TXGBE_EPHY_STAT) & TXGBE_EPHY_STAT_PPL_LOCK)) {
			speed = TXGBE_LINK_SPEED_40GB_FULL;
			err = hw->mac.setup_link(hw, speed, false);
			if (err) {
				DEBUGOUT("setup phy failed");
				return;
			}
		}
	} else {
		value = rd32_epcs(hw, SR_XS_PCS_CTRL2);
		if ((value & 0x3) != SR_PCS_CTRL2_TYPE_SEL_X)
			hw->link_status = TXGBE_LINK_STATUS_NONE;
	}

	/* receive packets that size > 2048 */
	wr32m(hw, TXGBE_MACRXCFG,
		TXGBE_MACRXCFG_JUMBO, TXGBE_MACRXCFG_JUMBO);

	wr32m(hw, TXGBE_FRMSZ, TXGBE_FRMSZ_MAX_MASK,
		TXGBE_FRMSZ_MAX(TXGBE_FRAME_SIZE_DFT));

	/* clear counters on read */
	wr32m(hw, TXGBE_MACCNTCTL,
		TXGBE_MACCNTCTL_RC, TXGBE_MACCNTCTL_RC);

	wr32m(hw, TXGBE_RXFCCFG,
		TXGBE_RXFCCFG_FC, TXGBE_RXFCCFG_FC);
	wr32m(hw, TXGBE_TXFCCFG,
		TXGBE_TXFCCFG_FC, TXGBE_TXFCCFG_FC);

	wr32m(hw, TXGBE_MACRXFLT,
		TXGBE_MACRXFLT_PROMISC, TXGBE_MACRXFLT_PROMISC);

	wr32m(hw, TXGBE_RSTSTAT,
		TXGBE_RSTSTAT_TMRINIT_MASK, TXGBE_RSTSTAT_TMRINIT(30));

	/* errata 4: initialize mng flex tbl and wakeup flex tbl*/
	wr32(hw, TXGBE_MNGFLEXSEL, 0);
	for (i = 0; i < 16; i++) {
		wr32(hw, TXGBE_MNGFLEXDWL(i), 0);
		wr32(hw, TXGBE_MNGFLEXDWH(i), 0);
		wr32(hw, TXGBE_MNGFLEXMSK(i), 0);
	}
	wr32(hw, TXGBE_LANFLEXSEL, 0);
	for (i = 0; i < 16; i++) {
		wr32(hw, TXGBE_LANFLEXDWL(i), 0);
		wr32(hw, TXGBE_LANFLEXDWH(i), 0);
		wr32(hw, TXGBE_LANFLEXMSK(i), 0);
	}

	/* set pause frame dst mac addr */
	wr32(hw, TXGBE_RXPBPFCDMACL, 0xC2000001);
	wr32(hw, TXGBE_RXPBPFCDMACH, 0x0180);

	hw->mac.init_thermal_sensor_thresh(hw);

	/* enable mac transmitter */
	wr32m(hw, TXGBE_MACTXCFG, TXGBE_MACTXCFG_TXE, TXGBE_MACTXCFG_TXE);

	hw->mac.autoc = hw->mac.orig_autoc;
	for (i = 0; i < 4; i++)
		wr32m(hw, TXGBE_IVAR(i), 0x80808080, 0);
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
	u32 autoc = 0;

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

	if (hw->mac.type == txgbe_mac_sp) {
		/* remember AUTOC from before we reset */
		autoc = hw->mac.autoc_read(hw);
	}

mac_reset_top:
	/* Do LAN reset, the MNG domain will not be reset. */
	wr32(hw, TXGBE_RST, TXGBE_RST_LAN(hw->bus.lan_id));
	txgbe_flush(hw);
	usec_delay(10);

	txgbe_reset_misc(hw);

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

	/* amlite TODO*/
	if (hw->mac.type == txgbe_mac_aml || hw->mac.type == txgbe_mac_aml40) {
		wr32(hw, TXGBE_LINKUP_FILTER, 30);
		wr32m(hw, TXGBE_MAC_MISC_CTL, TXGBE_MAC_MISC_LINK_STS_MOD,
			  TXGBE_LINK_BOTH_PCS_MAC);
		/* amlite: bme */
		wr32(hw, TXGBE_PX_PF_BME, TXGBE_PX_PF_BME_EN);
		/* amlite: rdm_rsc_ctl_free_ctl set to 1 */
		wr32m(hw, TXGBE_RDM_RSC_CTL, TXGBE_RDM_RSC_CTL_FREE_CTL,
			  TXGBE_RDM_RSC_CTL_FREE_CTL);
	} else {
		/*
		 * Store the original AUTOC/AUTOC2 values if they have not been
		 * stored off yet.  Otherwise restore the stored original
		 * values since the reset operation sets back to defaults.
		 */
		if (!hw->mac.orig_link_settings_stored) {
			hw->mac.orig_autoc = hw->mac.autoc_read(hw);
			hw->mac.orig_link_settings_stored = true;
		} else {
			hw->mac.orig_autoc = autoc;
		}
	}

	if (hw->phy.ffe_set) {
		/* Make sure phy power is up */
		msec_delay(50);

		/* A temporary solution to set phy */
		if (hw->mac.type == txgbe_mac_sp)
			txgbe_set_phy_temp(hw);
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

/**
 * txgbe_fdir_check_cmd_complete - poll to check whether FDIRPICMD is complete
 * @hw: pointer to hardware structure
 * @fdircmd: current value of FDIRCMD register
 */
static s32 txgbe_fdir_check_cmd_complete(struct txgbe_hw *hw, u32 *fdircmd)
{
	int i;

	for (i = 0; i < TXGBE_FDIRCMD_CMD_POLL; i++) {
		*fdircmd = rd32(hw, TXGBE_FDIRPICMD);
		if (!(*fdircmd & TXGBE_FDIRPICMD_OP_MASK))
			return 0;
		usec_delay(10);
	}

	return TXGBE_ERR_FDIR_CMD_INCOMPLETE;
}

/**
 *  txgbe_reinit_fdir_tables - Reinitialize Flow Director tables.
 *  @hw: pointer to hardware structure
 **/
s32 txgbe_reinit_fdir_tables(struct txgbe_hw *hw)
{
	s32 err;
	int i;
	u32 fdirctrl = rd32(hw, TXGBE_FDIRCTL);
	u32 fdircmd;
	fdirctrl &= ~TXGBE_FDIRCTL_INITDONE;

	/*
	 * Before starting reinitialization process,
	 * FDIRPICMD.OP must be zero.
	 */
	err = txgbe_fdir_check_cmd_complete(hw, &fdircmd);
	if (err) {
		DEBUGOUT("Flow Director previous command did not complete, aborting table re-initialization.");
		return err;
	}

	wr32(hw, TXGBE_FDIRFREE, 0);
	txgbe_flush(hw);
	/*
	 * adapters flow director init flow cannot be restarted,
	 * Workaround silicon errata by performing the following steps
	 * before re-writing the FDIRCTL control register with the same value.
	 * - write 1 to bit 8 of FDIRPICMD register &
	 * - write 0 to bit 8 of FDIRPICMD register
	 */
	wr32m(hw, TXGBE_FDIRPICMD, TXGBE_FDIRPICMD_CLR, TXGBE_FDIRPICMD_CLR);
	txgbe_flush(hw);
	wr32m(hw, TXGBE_FDIRPICMD, TXGBE_FDIRPICMD_CLR, 0);
	txgbe_flush(hw);
	/*
	 * Clear FDIR Hash register to clear any leftover hashes
	 * waiting to be programmed.
	 */
	wr32(hw, TXGBE_FDIRPIHASH, 0x00);
	txgbe_flush(hw);

	wr32(hw, TXGBE_FDIRCTL, fdirctrl);
	txgbe_flush(hw);

	/* Poll init-done after we write FDIRCTL register */
	for (i = 0; i < TXGBE_FDIR_INIT_DONE_POLL; i++) {
		if (rd32m(hw, TXGBE_FDIRCTL, TXGBE_FDIRCTL_INITDONE))
			break;
		msec_delay(1);
	}
	if (i >= TXGBE_FDIR_INIT_DONE_POLL) {
		DEBUGOUT("Flow Director Signature poll time exceeded!");
		return TXGBE_ERR_FDIR_REINIT_FAILED;
	}

	/* Clear FDIR statistics registers (read to clear) */
	rd32(hw, TXGBE_FDIRUSED);
	rd32(hw, TXGBE_FDIRFAIL);
	rd32(hw, TXGBE_FDIRMATCH);
	rd32(hw, TXGBE_FDIRMISS);
	rd32(hw, TXGBE_FDIRLEN);

	return 0;
}

/**
 *  txgbe_start_hw_raptor - Prepare hardware for Tx/Rx
 *  @hw: pointer to hardware structure
 *
 *  Starts the hardware using the generic start_hw function
 *  and the generation start_hw function.
 *  Then performs revision-specific operations, if any.
 **/
s32 txgbe_start_hw_raptor(struct txgbe_hw *hw)
{
	s32 err = 0;

	err = txgbe_start_hw(hw);
	if (err != 0)
		goto out;

	err = txgbe_start_hw_gen2(hw);
	if (err != 0)
		goto out;

	/* We need to run link autotry after the driver loads */
	hw->mac.autotry_restart = true;

out:
	return err;
}

/**
 *  txgbe_enable_rx_dma_raptor - Enable the Rx DMA unit
 *  @hw: pointer to hardware structure
 *  @regval: register value to write to RXCTRL
 *
 *  Enables the Rx DMA unit
 **/
s32 txgbe_enable_rx_dma_raptor(struct txgbe_hw *hw, u32 regval)
{
	/*
	 * Workaround silicon errata when enabling the Rx datapath.
	 * If traffic is incoming before we enable the Rx unit, it could hang
	 * the Rx DMA unit.  Therefore, make sure the security engine is
	 * completely disabled prior to enabling the Rx unit.
	 */

	hw->mac.disable_sec_rx_path(hw);

	if (regval & TXGBE_PBRXCTL_ENA)
		txgbe_enable_rx(hw);
	else
		txgbe_disable_rx(hw);

	hw->mac.enable_sec_rx_path(hw);

	return 0;
}

/**
 *  txgbe_verify_lesm_fw_enabled_raptor - Checks LESM FW module state.
 *  @hw: pointer to hardware structure
 *
 *  Returns true if the LESM FW module is present and enabled. Otherwise
 *  returns false. Smart Speed must be disabled if LESM FW module is enabled.
 **/
bool txgbe_verify_lesm_fw_enabled_raptor(struct txgbe_hw *hw)
{
	bool lesm_enabled = false;
	u16 fw_offset, fw_lesm_param_offset, fw_lesm_state;
	s32 status;

	/* get the offset to the Firmware Module block */
	status = hw->rom.read16(hw, TXGBE_FW_PTR, &fw_offset);

	if (status != 0 || fw_offset == 0 || fw_offset == 0xFFFF)
		goto out;

	/* get the offset to the LESM Parameters block */
	status = hw->rom.read16(hw, (fw_offset +
				     TXGBE_FW_LESM_PARAMETERS_PTR),
				     &fw_lesm_param_offset);

	if (status != 0 ||
	    fw_lesm_param_offset == 0 || fw_lesm_param_offset == 0xFFFF)
		goto out;

	/* get the LESM state word */
	status = hw->rom.read16(hw, (fw_lesm_param_offset +
				     TXGBE_FW_LESM_STATE_1),
				     &fw_lesm_state);

	if (status == 0 && (fw_lesm_state & TXGBE_FW_LESM_STATE_ENABLED))
		lesm_enabled = true;

out:
	lesm_enabled = false;
	return lesm_enabled;
}

/**
 * txgbe_reset_pipeline_raptor - perform pipeline reset
 *
 *  @hw: pointer to hardware structure
 *
 * Reset pipeline by asserting Restart_AN together with LMS change to ensure
 * full pipeline reset.  This function assumes the SW/FW lock is held.
 **/
s32 txgbe_reset_pipeline_raptor(struct txgbe_hw *hw)
{
	s32 err = 0;
	u64 autoc;

	autoc = hw->mac.autoc_read(hw);

	/* Enable link if disabled in NVM */
	if (autoc & TXGBE_AUTOC_LINK_DIA_MASK)
		autoc &= ~TXGBE_AUTOC_LINK_DIA_MASK;

	autoc |= TXGBE_AUTOC_AN_RESTART;
	/* Write AUTOC register with toggled LMS[2] bit and Restart_AN */
	hw->mac.autoc_write(hw, autoc ^ TXGBE_AUTOC_LMS_AN);

	/* Write AUTOC register with original LMS field and Restart_AN */
	hw->mac.autoc_write(hw, autoc);
	txgbe_flush(hw);

	return err;
}

s32 txgbe_e56_check_phy_link(struct txgbe_hw *hw, u32 *speed,
				bool *link_up)
{
	u32 rdata = 0;
	u32 links_reg = 0;

	/* must read it twice because the state may
	 * not be correct the first time you read it
	 */
	rdata = rd32_epcs(hw, 0x30001);
	rdata = rd32_epcs(hw, 0x30001);

	if (rdata & TXGBE_AML_PHY_LINK_UP)
		*link_up = true;
	else
		*link_up = false;

	links_reg = rd32(hw, TXGBE_PORTSTAT);
	if (*link_up) {
		if ((links_reg & TXGBE_CFG_PORT_ST_AML_LINK_40G) ==
				TXGBE_CFG_PORT_ST_AML_LINK_40G)
			*speed = TXGBE_LINK_SPEED_40GB_FULL;
		else if ((links_reg & TXGBE_CFG_PORT_ST_AML_LINK_25G) ==
				TXGBE_CFG_PORT_ST_AML_LINK_25G)
			*speed = TXGBE_LINK_SPEED_25GB_FULL;
		else if ((links_reg & TXGBE_CFG_PORT_ST_AML_LINK_10G) ==
				TXGBE_CFG_PORT_ST_AML_LINK_10G)
			*speed = TXGBE_LINK_SPEED_10GB_FULL;
	} else {
		*speed = TXGBE_LINK_SPEED_UNKNOWN;
	}

	return 0;
}
