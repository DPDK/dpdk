/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "ngbe_type.h"
#include "ngbe_mbx.h"
#include "ngbe_phy.h"
#include "ngbe_eeprom.h"
#include "ngbe_mng.h"
#include "ngbe_hw.h"

/**
 *  ngbe_start_hw - Prepare hardware for Tx/Rx
 *  @hw: pointer to hardware structure
 *
 *  Starts the hardware.
 **/
s32 ngbe_start_hw(struct ngbe_hw *hw)
{
	s32 err;

	/* Clear the VLAN filter table */
	hw->mac.clear_vfta(hw);

	/* Clear statistics registers */
	hw->mac.clear_hw_cntrs(hw);

	/* Setup flow control */
	err = hw->mac.setup_fc(hw);
	if (err != 0 && err != NGBE_NOT_IMPLEMENTED) {
		DEBUGOUT("Flow control setup failed, returning %d", err);
		return err;
	}

	/* Clear adapter stopped flag */
	hw->adapter_stopped = false;

	return 0;
}

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

	ngbe_read_efuse(hw);
	ngbe_save_eeprom_version(hw);

	/* Reset the hardware */
	status = hw->mac.reset_hw(hw);
	if (status == 0) {
		/* Start the HW */
		status = hw->mac.start_hw(hw);
	}

	if (status != 0)
		DEBUGOUT("Failed to initialize HW, STATUS = %d", status);

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

	if (hw->gpio_ctl) {
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

	/* Call adapter stop to disable tx/rx and clear interrupts */
	status = hw->mac.stop_hw(hw);
	if (status != 0)
		return status;

	/* Identify PHY and related function pointers */
	status = ngbe_init_phy(hw);
	if (status)
		return status;

	/* Reset PHY */
	if (!hw->phy.reset_disable)
		hw->phy.reset_hw(hw);

	wr32(hw, NGBE_RST, NGBE_RST_LAN(hw->bus.lan_id));
	ngbe_flush(hw);
	msec_delay(50);

	ngbe_reset_misc_em(hw);
	hw->mac.clear_hw_cntrs(hw);

	msec_delay(50);

	/* Store the permanent mac address */
	hw->mac.get_mac_addr(hw, hw->mac.perm_addr);

	/*
	 * Store MAC address from RAR0, clear receive address registers, and
	 * clear the multicast table.
	 */
	hw->mac.num_rar_entries = NGBE_EM_RAR_ENTRIES;
	hw->mac.init_rx_addrs(hw);

	return status;
}

/**
 *  ngbe_clear_hw_cntrs - Generic clear hardware counters
 *  @hw: pointer to hardware structure
 *
 *  Clears all hardware statistics counters by reading them from the hardware
 *  Statistics counters are clear on read.
 **/
s32 ngbe_clear_hw_cntrs(struct ngbe_hw *hw)
{
	u16 i = 0;

	/* QP Stats */
	/* don't write clear queue stats */
	for (i = 0; i < NGBE_MAX_QP; i++) {
		hw->qp_last[i].rx_qp_packets = 0;
		hw->qp_last[i].tx_qp_packets = 0;
		hw->qp_last[i].rx_qp_bytes = 0;
		hw->qp_last[i].tx_qp_bytes = 0;
		hw->qp_last[i].rx_qp_mc_packets = 0;
		hw->qp_last[i].tx_qp_mc_packets = 0;
		hw->qp_last[i].rx_qp_bc_packets = 0;
		hw->qp_last[i].tx_qp_bc_packets = 0;
	}

	/* PB Stats */
	rd32(hw, NGBE_PBRXLNKXON);
	rd32(hw, NGBE_PBRXLNKXOFF);
	rd32(hw, NGBE_PBTXLNKXON);
	rd32(hw, NGBE_PBTXLNKXOFF);

	/* DMA Stats */
	rd32(hw, NGBE_DMARXPKT);
	rd32(hw, NGBE_DMATXPKT);

	rd64(hw, NGBE_DMARXOCTL);
	rd64(hw, NGBE_DMATXOCTL);

	/* MAC Stats */
	rd64(hw, NGBE_MACRXERRCRCL);
	rd64(hw, NGBE_MACRXMPKTL);
	rd64(hw, NGBE_MACTXMPKTL);

	rd64(hw, NGBE_MACRXPKTL);
	rd64(hw, NGBE_MACTXPKTL);
	rd64(hw, NGBE_MACRXGBOCTL);

	rd64(hw, NGBE_MACRXOCTL);
	rd32(hw, NGBE_MACTXOCTL);

	rd64(hw, NGBE_MACRX1TO64L);
	rd64(hw, NGBE_MACRX65TO127L);
	rd64(hw, NGBE_MACRX128TO255L);
	rd64(hw, NGBE_MACRX256TO511L);
	rd64(hw, NGBE_MACRX512TO1023L);
	rd64(hw, NGBE_MACRX1024TOMAXL);
	rd64(hw, NGBE_MACTX1TO64L);
	rd64(hw, NGBE_MACTX65TO127L);
	rd64(hw, NGBE_MACTX128TO255L);
	rd64(hw, NGBE_MACTX256TO511L);
	rd64(hw, NGBE_MACTX512TO1023L);
	rd64(hw, NGBE_MACTX1024TOMAXL);

	rd64(hw, NGBE_MACRXERRLENL);
	rd32(hw, NGBE_MACRXOVERSIZE);
	rd32(hw, NGBE_MACRXJABBER);

	/* MACsec Stats */
	rd32(hw, NGBE_LSECTX_UTPKT);
	rd32(hw, NGBE_LSECTX_ENCPKT);
	rd32(hw, NGBE_LSECTX_PROTPKT);
	rd32(hw, NGBE_LSECTX_ENCOCT);
	rd32(hw, NGBE_LSECTX_PROTOCT);
	rd32(hw, NGBE_LSECRX_UTPKT);
	rd32(hw, NGBE_LSECRX_BTPKT);
	rd32(hw, NGBE_LSECRX_NOSCIPKT);
	rd32(hw, NGBE_LSECRX_UNSCIPKT);
	rd32(hw, NGBE_LSECRX_DECOCT);
	rd32(hw, NGBE_LSECRX_VLDOCT);
	rd32(hw, NGBE_LSECRX_UNCHKPKT);
	rd32(hw, NGBE_LSECRX_DLYPKT);
	rd32(hw, NGBE_LSECRX_LATEPKT);
	for (i = 0; i < 2; i++) {
		rd32(hw, NGBE_LSECRX_OKPKT(i));
		rd32(hw, NGBE_LSECRX_INVPKT(i));
		rd32(hw, NGBE_LSECRX_BADPKT(i));
	}
	for (i = 0; i < 4; i++) {
		rd32(hw, NGBE_LSECRX_INVSAPKT(i));
		rd32(hw, NGBE_LSECRX_BADSAPKT(i));
	}

	return 0;
}

/**
 *  ngbe_get_mac_addr - Generic get MAC address
 *  @hw: pointer to hardware structure
 *  @mac_addr: Adapter MAC address
 *
 *  Reads the adapter's MAC address from first Receive Address Register (RAR0)
 *  A reset of the adapter must be performed prior to calling this function
 *  in order for the MAC address to have been loaded from the EEPROM into RAR0
 **/
s32 ngbe_get_mac_addr(struct ngbe_hw *hw, u8 *mac_addr)
{
	u32 rar_high;
	u32 rar_low;
	u16 i;

	wr32(hw, NGBE_ETHADDRIDX, 0);
	rar_high = rd32(hw, NGBE_ETHADDRH);
	rar_low = rd32(hw, NGBE_ETHADDRL);

	for (i = 0; i < 2; i++)
		mac_addr[i] = (u8)(rar_high >> (1 - i) * 8);

	for (i = 0; i < 4; i++)
		mac_addr[i + 2] = (u8)(rar_low >> (3 - i) * 8);

	return 0;
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
	u16 i;
	s32 status = 0;

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

	wr32(hw, NGBE_BMECTL, 0x3);

	/* Disable the receive unit by stopping each queue */
	for (i = 0; i < hw->mac.max_rx_queues; i++)
		wr32(hw, NGBE_RXCFG(i), 0);

	/* flush all queues disables */
	ngbe_flush(hw);
	msec_delay(2);

	/*
	 * Prevent the PCI-E bus from hanging by disabling PCI-E master
	 * access and verify no pending requests
	 */
	status = ngbe_set_pcie_master(hw, false);
	if (status)
		return status;

	/* Disable the transmit unit.  Each queue must be disabled. */
	for (i = 0; i < hw->mac.max_tx_queues; i++)
		wr32(hw, NGBE_TXCFG(i), 0);

	/* flush all queues disables */
	ngbe_flush(hw);
	msec_delay(2);

	return 0;
}

/**
 *  ngbe_led_on - Turns on the software controllable LEDs.
 *  @hw: pointer to hardware structure
 *  @index: led number to turn on
 **/
s32 ngbe_led_on(struct ngbe_hw *hw, u32 index)
{
	u32 led_reg = rd32(hw, NGBE_LEDCTL);

	if (index > 3)
		return NGBE_ERR_PARAM;

	/* To turn on the LED, set mode to ON. */
	led_reg |= NGBE_LEDCTL_100M;
	wr32(hw, NGBE_LEDCTL, led_reg);
	ngbe_flush(hw);

	return 0;
}

/**
 *  ngbe_led_off - Turns off the software controllable LEDs.
 *  @hw: pointer to hardware structure
 *  @index: led number to turn off
 **/
s32 ngbe_led_off(struct ngbe_hw *hw, u32 index)
{
	u32 led_reg = rd32(hw, NGBE_LEDCTL);

	if (index > 3)
		return NGBE_ERR_PARAM;

	/* To turn off the LED, set mode to OFF. */
	led_reg &= ~NGBE_LEDCTL_100M;
	wr32(hw, NGBE_LEDCTL, led_reg);
	ngbe_flush(hw);

	return 0;
}

/**
 *  ngbe_validate_mac_addr - Validate MAC address
 *  @mac_addr: pointer to MAC address.
 *
 *  Tests a MAC address to ensure it is a valid Individual Address.
 **/
s32 ngbe_validate_mac_addr(u8 *mac_addr)
{
	s32 status = 0;

	/* Make sure it is not a multicast address */
	if (NGBE_IS_MULTICAST((struct rte_ether_addr *)mac_addr)) {
		status = NGBE_ERR_INVALID_MAC_ADDR;
	/* Not a broadcast address */
	} else if (NGBE_IS_BROADCAST((struct rte_ether_addr *)mac_addr)) {
		status = NGBE_ERR_INVALID_MAC_ADDR;
	/* Reject the zero address */
	} else if (mac_addr[0] == 0 && mac_addr[1] == 0 && mac_addr[2] == 0 &&
		   mac_addr[3] == 0 && mac_addr[4] == 0 && mac_addr[5] == 0) {
		status = NGBE_ERR_INVALID_MAC_ADDR;
	}
	return status;
}

/**
 *  ngbe_set_rar - Set Rx address register
 *  @hw: pointer to hardware structure
 *  @index: Receive address register to write
 *  @addr: Address to put into receive address register
 *  @vmdq: VMDq "set" or "pool" index
 *  @enable_addr: set flag that address is active
 *
 *  Puts an ethernet address into a receive address register.
 **/
s32 ngbe_set_rar(struct ngbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
			  u32 enable_addr)
{
	u32 rar_low, rar_high;
	u32 rar_entries = hw->mac.num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (index >= rar_entries) {
		DEBUGOUT("RAR index %d is out of range.", index);
		return NGBE_ERR_INVALID_ARGUMENT;
	}

	/* setup VMDq pool selection before this RAR gets enabled */
	hw->mac.set_vmdq(hw, index, vmdq);

	/*
	 * HW expects these in little endian so we reverse the byte
	 * order from network order (big endian) to little endian
	 */
	rar_low = NGBE_ETHADDRL_AD0(addr[5]) |
		  NGBE_ETHADDRL_AD1(addr[4]) |
		  NGBE_ETHADDRL_AD2(addr[3]) |
		  NGBE_ETHADDRL_AD3(addr[2]);
	/*
	 * Some parts put the VMDq setting in the extra RAH bits,
	 * so save everything except the lower 16 bits that hold part
	 * of the address and the address valid bit.
	 */
	rar_high = rd32(hw, NGBE_ETHADDRH);
	rar_high &= ~NGBE_ETHADDRH_AD_MASK;
	rar_high |= (NGBE_ETHADDRH_AD4(addr[1]) |
		     NGBE_ETHADDRH_AD5(addr[0]));

	rar_high &= ~NGBE_ETHADDRH_VLD;
	if (enable_addr != 0)
		rar_high |= NGBE_ETHADDRH_VLD;

	wr32(hw, NGBE_ETHADDRIDX, index);
	wr32(hw, NGBE_ETHADDRL, rar_low);
	wr32(hw, NGBE_ETHADDRH, rar_high);

	return 0;
}

/**
 *  ngbe_clear_rar - Remove Rx address register
 *  @hw: pointer to hardware structure
 *  @index: Receive address register to write
 *
 *  Clears an ethernet address from a receive address register.
 **/
s32 ngbe_clear_rar(struct ngbe_hw *hw, u32 index)
{
	u32 rar_high;
	u32 rar_entries = hw->mac.num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (index >= rar_entries) {
		DEBUGOUT("RAR index %d is out of range.", index);
		return NGBE_ERR_INVALID_ARGUMENT;
	}

	/*
	 * Some parts put the VMDq setting in the extra RAH bits,
	 * so save everything except the lower 16 bits that hold part
	 * of the address and the address valid bit.
	 */
	wr32(hw, NGBE_ETHADDRIDX, index);
	rar_high = rd32(hw, NGBE_ETHADDRH);
	rar_high &= ~(NGBE_ETHADDRH_AD_MASK | NGBE_ETHADDRH_VLD);

	wr32(hw, NGBE_ETHADDRL, 0);
	wr32(hw, NGBE_ETHADDRH, rar_high);

	/* clear VMDq pool/queue selection for this RAR */
	hw->mac.clear_vmdq(hw, index, BIT_MASK32);

	return 0;
}

/**
 *  ngbe_init_rx_addrs - Initializes receive address filters.
 *  @hw: pointer to hardware structure
 *
 *  Places the MAC address in receive address register 0 and clears the rest
 *  of the receive address registers. Clears the multicast table. Assumes
 *  the receiver is in reset when the routine is called.
 **/
s32 ngbe_init_rx_addrs(struct ngbe_hw *hw)
{
	u32 i;
	u32 psrctl;
	u32 rar_entries = hw->mac.num_rar_entries;

	/*
	 * If the current mac address is valid, assume it is a software override
	 * to the permanent address.
	 * Otherwise, use the permanent address from the eeprom.
	 */
	if (ngbe_validate_mac_addr(hw->mac.addr) ==
	    NGBE_ERR_INVALID_MAC_ADDR) {
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

	/* Zero out the other receive addresses. */
	DEBUGOUT("Clearing RAR[1-%d]", rar_entries - 1);
	for (i = 1; i < rar_entries; i++) {
		wr32(hw, NGBE_ETHADDRIDX, i);
		wr32(hw, NGBE_ETHADDRL, 0);
		wr32(hw, NGBE_ETHADDRH, 0);
	}

	/* Clear the MTA */
	hw->addr_ctrl.mta_in_use = 0;
	psrctl = rd32(hw, NGBE_PSRCTL);
	psrctl &= ~(NGBE_PSRCTL_ADHF12_MASK | NGBE_PSRCTL_MCHFENA);
	psrctl |= NGBE_PSRCTL_ADHF12(hw->mac.mc_filter_type);
	wr32(hw, NGBE_PSRCTL, psrctl);

	DEBUGOUT(" Clearing MTA");
	for (i = 0; i < hw->mac.mcft_size; i++)
		wr32(hw, NGBE_MCADDRTBL(i), 0);

	ngbe_init_uta_tables(hw);

	return 0;
}

/**
 *  ngbe_mta_vector - Determines bit-vector in multicast table to set
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
static s32 ngbe_mta_vector(struct ngbe_hw *hw, u8 *mc_addr)
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
 *  ngbe_set_mta - Set bit-vector in multicast table
 *  @hw: pointer to hardware structure
 *  @mc_addr: Multicast address
 *
 *  Sets the bit-vector in the multicast table.
 **/
void ngbe_set_mta(struct ngbe_hw *hw, u8 *mc_addr)
{
	u32 vector;
	u32 vector_bit;
	u32 vector_reg;

	hw->addr_ctrl.mta_in_use++;

	vector = ngbe_mta_vector(hw, mc_addr);
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
 *  ngbe_update_mc_addr_list - Updates MAC list of multicast addresses
 *  @hw: pointer to hardware structure
 *  @mc_addr_list: the list of new multicast addresses
 *  @mc_addr_count: number of addresses
 *  @next: iterator function to walk the multicast address list
 *  @clear: flag, when set clears the table beforehand
 *
 *  When the clear flag is set, the given list replaces any existing list.
 *  Hashes the given addresses into the multicast table.
 **/
s32 ngbe_update_mc_addr_list(struct ngbe_hw *hw, u8 *mc_addr_list,
				      u32 mc_addr_count, ngbe_mc_addr_itr next,
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
		ngbe_set_mta(hw, next(hw, &mc_addr_list, &vmdq));
	}

	/* Enable mta */
	for (i = 0; i < hw->mac.mcft_size; i++)
		wr32a(hw, NGBE_MCADDRTBL(0), i,
				      hw->mac.mta_shadow[i]);

	if (hw->addr_ctrl.mta_in_use > 0) {
		u32 psrctl = rd32(hw, NGBE_PSRCTL);
		psrctl &= ~(NGBE_PSRCTL_ADHF12_MASK | NGBE_PSRCTL_MCHFENA);
		psrctl |= NGBE_PSRCTL_MCHFENA |
			 NGBE_PSRCTL_ADHF12(hw->mac.mc_filter_type);
		wr32(hw, NGBE_PSRCTL, psrctl);
	}

	DEBUGOUT("ngbe update mc addr list complete");
	return 0;
}

/**
 *  ngbe_setup_fc_em - Set up flow control
 *  @hw: pointer to hardware structure
 *
 *  Called at init time to set up flow control.
 **/
s32 ngbe_setup_fc_em(struct ngbe_hw *hw)
{
	s32 err = 0;
	u16 reg_cu = 0;

	/* Validate the requested mode */
	if (hw->fc.strict_ieee && hw->fc.requested_mode == ngbe_fc_rx_pause) {
		DEBUGOUT("ngbe_fc_rx_pause not valid in strict IEEE mode");
		err = NGBE_ERR_INVALID_LINK_SETTINGS;
		goto out;
	}

	/*
	 * 1gig parts do not have a word in the EEPROM to determine the
	 * default flow control setting, so we explicitly set it to full.
	 */
	if (hw->fc.requested_mode == ngbe_fc_default)
		hw->fc.requested_mode = ngbe_fc_full;

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
	case ngbe_fc_none:
		/* Flow control completely disabled by software override. */
		break;
	case ngbe_fc_tx_pause:
		/*
		 * Tx Flow control is enabled, and Rx Flow control is
		 * disabled by software override.
		 */
		if (hw->phy.type == ngbe_phy_mvl_sfi ||
			hw->phy.type == ngbe_phy_yt8521s_sfi)
			reg_cu |= MVL_FANA_ASM_PAUSE;
		else
			reg_cu |= 0x800; /*need to merge rtl and mvl on page 0*/
		break;
	case ngbe_fc_rx_pause:
		/*
		 * Rx Flow control is enabled and Tx Flow control is
		 * disabled by software override. Since there really
		 * isn't a way to advertise that we are capable of RX
		 * Pause ONLY, we will advertise that we support both
		 * symmetric and asymmetric Rx PAUSE, as such we fall
		 * through to the fc_full statement.  Later, we will
		 * disable the adapter's ability to send PAUSE frames.
		 */
	case ngbe_fc_full:
		/* Flow control (both Rx and Tx) is enabled by SW override. */
		if (hw->phy.type == ngbe_phy_mvl_sfi ||
			hw->phy.type == ngbe_phy_yt8521s_sfi)
			reg_cu |= MVL_FANA_SYM_PAUSE;
		else
			reg_cu |= 0xC00; /*need to merge rtl and mvl on page 0*/
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly");
		err = NGBE_ERR_CONFIG;
		goto out;
	}

	err = hw->phy.set_pause_adv(hw, reg_cu);

out:
	return err;
}

/**
 *  ngbe_fc_enable - Enable flow control
 *  @hw: pointer to hardware structure
 *
 *  Enable flow control according to the current settings.
 **/
s32 ngbe_fc_enable(struct ngbe_hw *hw)
{
	s32 err = 0;
	u32 mflcn_reg, fccfg_reg;
	u32 pause_time;
	u32 fcrtl, fcrth;

	/* Validate the water mark configuration */
	if (!hw->fc.pause_time) {
		err = NGBE_ERR_INVALID_LINK_SETTINGS;
		goto out;
	}

	/* Low water mark of zero causes XOFF floods */
	if ((hw->fc.current_mode & ngbe_fc_tx_pause) && hw->fc.high_water) {
		if (!hw->fc.low_water ||
			hw->fc.low_water >= hw->fc.high_water) {
			DEBUGOUT("Invalid water mark configuration");
			err = NGBE_ERR_INVALID_LINK_SETTINGS;
			goto out;
		}
	}

	/* Negotiate the fc mode to use */
	hw->mac.fc_autoneg(hw);

	/* Disable any previous flow control settings */
	mflcn_reg = rd32(hw, NGBE_RXFCCFG);
	mflcn_reg &= ~NGBE_RXFCCFG_FC;

	fccfg_reg = rd32(hw, NGBE_TXFCCFG);
	fccfg_reg &= ~NGBE_TXFCCFG_FC;
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
	case ngbe_fc_none:
		/*
		 * Flow control is disabled by software override or autoneg.
		 * The code below will actually disable it in the HW.
		 */
		break;
	case ngbe_fc_rx_pause:
		/*
		 * Rx Flow control is enabled and Tx Flow control is
		 * disabled by software override. Since there really
		 * isn't a way to advertise that we are capable of RX
		 * Pause ONLY, we will advertise that we support both
		 * symmetric and asymmetric Rx PAUSE.  Later, we will
		 * disable the adapter's ability to send PAUSE frames.
		 */
		mflcn_reg |= NGBE_RXFCCFG_FC;
		break;
	case ngbe_fc_tx_pause:
		/*
		 * Tx Flow control is enabled, and Rx Flow control is
		 * disabled by software override.
		 */
		fccfg_reg |= NGBE_TXFCCFG_FC;
		break;
	case ngbe_fc_full:
		/* Flow control (both Rx and Tx) is enabled by SW override. */
		mflcn_reg |= NGBE_RXFCCFG_FC;
		fccfg_reg |= NGBE_TXFCCFG_FC;
		break;
	default:
		DEBUGOUT("Flow control param set incorrectly");
		err = NGBE_ERR_CONFIG;
		goto out;
	}

	/* Set 802.3x based flow control settings. */
	wr32(hw, NGBE_RXFCCFG, mflcn_reg);
	wr32(hw, NGBE_TXFCCFG, fccfg_reg);

	/* Set up and enable Rx high/low water mark thresholds, enable XON. */
	if ((hw->fc.current_mode & ngbe_fc_tx_pause) &&
		hw->fc.high_water) {
		fcrtl = NGBE_FCWTRLO_TH(hw->fc.low_water) |
			NGBE_FCWTRLO_XON;
		fcrth = NGBE_FCWTRHI_TH(hw->fc.high_water) |
			NGBE_FCWTRHI_XOFF;
	} else {
		/*
		 * In order to prevent Tx hangs when the internal Tx
		 * switch is enabled we must set the high water mark
		 * to the Rx packet buffer size - 24KB.  This allows
		 * the Tx switch to function even under heavy Rx
		 * workloads.
		 */
		fcrtl = 0;
		fcrth = rd32(hw, NGBE_PBRXSIZE) - 24576;
	}
	wr32(hw, NGBE_FCWTRLO, fcrtl);
	wr32(hw, NGBE_FCWTRHI, fcrth);

	/* Configure pause time */
	pause_time = NGBE_RXFCFSH_TIME(hw->fc.pause_time);
	wr32(hw, NGBE_FCXOFFTM, pause_time * 0x00010000);

	/* Configure flow control refresh threshold value */
	wr32(hw, NGBE_RXFCRFSH, hw->fc.pause_time / 2);

out:
	return err;
}

/**
 *  ngbe_negotiate_fc - Negotiate flow control
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
s32 ngbe_negotiate_fc(struct ngbe_hw *hw, u32 adv_reg, u32 lp_reg,
		       u32 adv_sym, u32 adv_asm, u32 lp_sym, u32 lp_asm)
{
	if ((!(adv_reg)) ||  (!(lp_reg))) {
		DEBUGOUT("Local or link partner's advertised flow control settings are NULL. Local: %x, link partner: %x",
			      adv_reg, lp_reg);
		return NGBE_ERR_FC_NOT_NEGOTIATED;
	}

	if ((adv_reg & adv_sym) && (lp_reg & lp_sym)) {
		/*
		 * Now we need to check if the user selected Rx ONLY
		 * of pause frames.  In this case, we had to advertise
		 * FULL flow control because we could not advertise RX
		 * ONLY. Hence, we must now check to see if we need to
		 * turn OFF the TRANSMISSION of PAUSE frames.
		 */
		if (hw->fc.requested_mode == ngbe_fc_full) {
			hw->fc.current_mode = ngbe_fc_full;
			DEBUGOUT("Flow Control = FULL.");
		} else {
			hw->fc.current_mode = ngbe_fc_rx_pause;
			DEBUGOUT("Flow Control=RX PAUSE frames only");
		}
	} else if (!(adv_reg & adv_sym) && (adv_reg & adv_asm) &&
		   (lp_reg & lp_sym) && (lp_reg & lp_asm)) {
		hw->fc.current_mode = ngbe_fc_tx_pause;
		DEBUGOUT("Flow Control = TX PAUSE frames only.");
	} else if ((adv_reg & adv_sym) && (adv_reg & adv_asm) &&
		   !(lp_reg & lp_sym) && (lp_reg & lp_asm)) {
		hw->fc.current_mode = ngbe_fc_rx_pause;
		DEBUGOUT("Flow Control = RX PAUSE frames only.");
	} else {
		hw->fc.current_mode = ngbe_fc_none;
		DEBUGOUT("Flow Control = NONE.");
	}
	return 0;
}

/**
 *  ngbe_fc_autoneg_em - Enable flow control IEEE clause 37
 *  @hw: pointer to hardware structure
 *
 *  Enable flow control according to IEEE clause 37.
 **/
STATIC s32 ngbe_fc_autoneg_em(struct ngbe_hw *hw)
{
	u8 technology_ability_reg = 0;
	u8 lp_technology_ability_reg = 0;

	hw->phy.get_adv_pause(hw, &technology_ability_reg);
	hw->phy.get_lp_adv_pause(hw, &lp_technology_ability_reg);

	return ngbe_negotiate_fc(hw, (u32)technology_ability_reg,
				  (u32)lp_technology_ability_reg,
				  NGBE_TAF_SYM_PAUSE, NGBE_TAF_ASM_PAUSE,
				  NGBE_TAF_SYM_PAUSE, NGBE_TAF_ASM_PAUSE);
}

/**
 *  ngbe_fc_autoneg - Configure flow control
 *  @hw: pointer to hardware structure
 *
 *  Compares our advertised flow control capabilities to those advertised by
 *  our link partner, and determines the proper flow control mode to use.
 **/
void ngbe_fc_autoneg(struct ngbe_hw *hw)
{
	s32 err = NGBE_ERR_FC_NOT_NEGOTIATED;
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

	err = ngbe_fc_autoneg_em(hw);

out:
	if (err == 0) {
		hw->fc.fc_was_autonegged = true;
	} else {
		hw->fc.fc_was_autonegged = false;
		hw->fc.current_mode = hw->fc.requested_mode;
	}
}

/**
 *  ngbe_set_pcie_master - Disable or Enable PCI-express master access
 *  @hw: pointer to hardware structure
 *
 *  Disables PCI-Express master access and verifies there are no pending
 *  requests. NGBE_ERR_MASTER_REQUESTS_PENDING is returned if master disable
 *  bit hasn't caused the master requests to be disabled, else 0
 *  is returned signifying master requests disabled.
 **/
s32 ngbe_set_pcie_master(struct ngbe_hw *hw, bool enable)
{
	struct rte_pci_device *pci_dev = (struct rte_pci_device *)hw->back;
	s32 status = 0;
	s32 ret = 0;
	u32 i;
	u16 reg;

	ret = rte_pci_read_config(pci_dev, &reg,
			sizeof(reg), PCI_COMMAND);
	if (ret != sizeof(reg)) {
		DEBUGOUT("Cannot read command from PCI config space!\n");
		return -1;
	}

	if (enable)
		reg |= PCI_COMMAND_MASTER;
	else
		reg &= ~PCI_COMMAND_MASTER;

	ret = rte_pci_write_config(pci_dev, &reg,
			sizeof(reg), PCI_COMMAND);
	if (ret != sizeof(reg)) {
		DEBUGOUT("Cannot write command to PCI config space!\n");
		return -1;
	}

	if (enable)
		goto out;

	/* Exit if master requests are blocked */
	if (!(rd32(hw, NGBE_BMEPEND)) ||
	    NGBE_REMOVED(hw->hw_addr))
		goto out;

	/* Poll for master request bit to clear */
	for (i = 0; i < NGBE_PCI_MASTER_DISABLE_TIMEOUT; i++) {
		usec_delay(100);
		if (!(rd32(hw, NGBE_BMEPEND)))
			goto out;
	}

	DEBUGOUT("PCIe transaction pending bit also did not clear.");
	status = NGBE_ERR_MASTER_REQUESTS_PENDING;

out:
	return status;
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
	u32 fwsm = 0;
	u32 swmask = NGBE_MNGSEM_SW(mask);
	u32 fwmask = NGBE_MNGSEM_FW(mask);
	u32 timeout = 200;
	u32 i;

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

	fwsm = rd32(hw, NGBE_MNGFWSYNC);
	DEBUGOUT("SWFW semaphore not granted: MNG_SWFW_SYNC = 0x%x, MNG_FW_SM = 0x%x",
			mngsem, fwsm);

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

	ngbe_get_eeprom_semaphore(hw);

	mngsem = rd32(hw, NGBE_MNGSEM);
	mngsem &= ~swmask;
	wr32(hw, NGBE_MNGSEM, mngsem);

	ngbe_release_eeprom_semaphore(hw);
}

/**
 *  ngbe_disable_sec_rx_path - Stops the receive data path
 *  @hw: pointer to hardware structure
 *
 *  Stops the receive data path and waits for the HW to internally empty
 *  the Rx security block
 **/
s32 ngbe_disable_sec_rx_path(struct ngbe_hw *hw)
{
#define NGBE_MAX_SECRX_POLL 4000

	int i;
	u32 secrxreg;

	secrxreg = rd32(hw, NGBE_SECRXCTL);
	secrxreg |= NGBE_SECRXCTL_XDSA;
	wr32(hw, NGBE_SECRXCTL, secrxreg);
	for (i = 0; i < NGBE_MAX_SECRX_POLL; i++) {
		secrxreg = rd32(hw, NGBE_SECRXSTAT);
		if (!(secrxreg & NGBE_SECRXSTAT_RDY))
			/* Use interrupt-safe sleep just in case */
			usec_delay(10);
		else
			break;
	}

	/* For informational purposes only */
	if (i >= NGBE_MAX_SECRX_POLL)
		DEBUGOUT("Rx unit being enabled before security path fully disabled.  Continuing with init.");

	return 0;
}

/**
 *  ngbe_enable_sec_rx_path - Enables the receive data path
 *  @hw: pointer to hardware structure
 *
 *  Enables the receive data path.
 **/
s32 ngbe_enable_sec_rx_path(struct ngbe_hw *hw)
{
	u32 secrxreg;

	secrxreg = rd32(hw, NGBE_SECRXCTL);
	secrxreg &= ~NGBE_SECRXCTL_XDSA;
	wr32(hw, NGBE_SECRXCTL, secrxreg);
	ngbe_flush(hw);

	return 0;
}

/**
 *  ngbe_clear_vmdq - Disassociate a VMDq pool index from a rx address
 *  @hw: pointer to hardware struct
 *  @rar: receive address register index to disassociate
 *  @vmdq: VMDq pool index to remove from the rar
 **/
s32 ngbe_clear_vmdq(struct ngbe_hw *hw, u32 rar, u32 vmdq)
{
	u32 mpsar;
	u32 rar_entries = hw->mac.num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (rar >= rar_entries) {
		DEBUGOUT("RAR index %d is out of range.", rar);
		return NGBE_ERR_INVALID_ARGUMENT;
	}

	wr32(hw, NGBE_ETHADDRIDX, rar);
	mpsar = rd32(hw, NGBE_ETHADDRASS);

	if (NGBE_REMOVED(hw->hw_addr))
		goto done;

	if (!mpsar)
		goto done;

	mpsar &= ~(1 << vmdq);
	wr32(hw, NGBE_ETHADDRASS, mpsar);

	/* was that the last pool using this rar? */
	if (mpsar == 0 && rar != 0)
		hw->mac.clear_rar(hw, rar);
done:
	return 0;
}

/**
 *  ngbe_set_vmdq - Associate a VMDq pool index with a rx address
 *  @hw: pointer to hardware struct
 *  @rar: receive address register index to associate with a VMDq index
 *  @vmdq: VMDq pool index
 **/
s32 ngbe_set_vmdq(struct ngbe_hw *hw, u32 rar, u32 vmdq)
{
	u32 mpsar;
	u32 rar_entries = hw->mac.num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (rar >= rar_entries) {
		DEBUGOUT("RAR index %d is out of range.", rar);
		return NGBE_ERR_INVALID_ARGUMENT;
	}

	wr32(hw, NGBE_ETHADDRIDX, rar);

	mpsar = rd32(hw, NGBE_ETHADDRASS);
	mpsar |= 1 << vmdq;
	wr32(hw, NGBE_ETHADDRASS, mpsar);

	return 0;
}

/**
 *  ngbe_init_uta_tables - Initialize the Unicast Table Array
 *  @hw: pointer to hardware structure
 **/
s32 ngbe_init_uta_tables(struct ngbe_hw *hw)
{
	int i;

	DEBUGOUT(" Clearing UTA");

	for (i = 0; i < 128; i++)
		wr32(hw, NGBE_UCADDRTBL(i), 0);

	return 0;
}

/**
 *  ngbe_find_vlvf_slot - find the vlanid or the first empty slot
 *  @hw: pointer to hardware structure
 *  @vlan: VLAN id to write to VLAN filter
 *  @vlvf_bypass: true to find vlanid only, false returns first empty slot if
 *		  vlanid not found
 *
 *
 *  return the VLVF index where this VLAN id should be placed
 *
 **/
s32 ngbe_find_vlvf_slot(struct ngbe_hw *hw, u32 vlan, bool vlvf_bypass)
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
	first_empty_slot = vlvf_bypass ? NGBE_ERR_NO_SPACE : 0;

	/* add VLAN enable bit for comparison */
	vlan |= NGBE_PSRVLAN_EA;

	/* Search for the vlan id in the VLVF entries. Save off the first empty
	 * slot found along the way.
	 *
	 * pre-decrement loop covering (NGBE_NUM_POOL - 1) .. 1
	 */
	for (regindex = NGBE_NUM_POOL; --regindex;) {
		wr32(hw, NGBE_PSRVLANIDX, regindex);
		bits = rd32(hw, NGBE_PSRVLAN);
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

	return first_empty_slot ? first_empty_slot : NGBE_ERR_NO_SPACE;
}

/**
 *  ngbe_set_vfta - Set VLAN filter table
 *  @hw: pointer to hardware structure
 *  @vlan: VLAN id to write to VLAN filter
 *  @vind: VMDq output index that maps queue to VLAN id in VLVFB
 *  @vlan_on: boolean flag to turn on/off VLAN
 *  @vlvf_bypass: boolean flag indicating updating default pool is okay
 *
 *  Turn on/off specified VLAN in the VLAN filter table.
 **/
s32 ngbe_set_vfta(struct ngbe_hw *hw, u32 vlan, u32 vind,
			   bool vlan_on, bool vlvf_bypass)
{
	u32 regidx, vfta_delta, vfta;
	s32 err;

	if (vlan > 4095 || vind > 63)
		return NGBE_ERR_PARAM;

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
	vfta = rd32(hw, NGBE_VLANTBL(regidx));

	/*
	 * vfta_delta represents the difference between the current value
	 * of vfta and the value we want in the register.  Since the diff
	 * is an XOR mask we can just update the vfta using an XOR
	 */
	vfta_delta &= vlan_on ? ~vfta : vfta;
	vfta ^= vfta_delta;

	/* Part 2
	 * Call ngbe_set_vlvf to set VLVFB and VLVF
	 */
	err = ngbe_set_vlvf(hw, vlan, vind, vlan_on, &vfta_delta,
					 vfta, vlvf_bypass);
	if (err != 0) {
		if (vlvf_bypass)
			goto vfta_update;
		return err;
	}

vfta_update:
	/* Update VFTA now that we are ready for traffic */
	if (vfta_delta)
		wr32(hw, NGBE_VLANTBL(regidx), vfta);

	return 0;
}

/**
 *  ngbe_set_vlvf - Set VLAN Pool Filter
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
s32 ngbe_set_vlvf(struct ngbe_hw *hw, u32 vlan, u32 vind,
			   bool vlan_on, u32 *vfta_delta, u32 vfta,
			   bool vlvf_bypass)
{
	u32 bits;
	u32 portctl;
	s32 vlvf_index;

	if (vlan > 4095 || vind > 63)
		return NGBE_ERR_PARAM;

	/* If VT Mode is set
	 *   Either vlan_on
	 *     make sure the vlan is in PSRVLAN
	 *     set the vind bit in the matching PSRVLANPLM
	 *   Or !vlan_on
	 *     clear the pool bit and possibly the vind
	 */
	portctl = rd32(hw, NGBE_PORTCTL);
	if (!(portctl & NGBE_PORTCTL_NUMVT_MASK))
		return 0;

	vlvf_index = ngbe_find_vlvf_slot(hw, vlan, vlvf_bypass);
	if (vlvf_index < 0)
		return vlvf_index;

	wr32(hw, NGBE_PSRVLANIDX, vlvf_index);
	bits = rd32(hw, NGBE_PSRVLANPLM(vind / 32));

	/* set the pool bit */
	bits |= 1 << (vind % 32);
	if (vlan_on)
		goto vlvf_update;

	/* clear the pool bit */
	bits ^= 1 << (vind % 32);

	if (!bits &&
	    !rd32(hw, NGBE_PSRVLANPLM(vind / 32))) {
		/* Clear PSRVLANPLM first, then disable PSRVLAN. Otherwise
		 * we run the risk of stray packets leaking into
		 * the PF via the default pool
		 */
		if (*vfta_delta)
			wr32(hw, NGBE_PSRVLANPLM(vlan / 32), vfta);

		/* disable VLVF and clear remaining bit from pool */
		wr32(hw, NGBE_PSRVLAN, 0);
		wr32(hw, NGBE_PSRVLANPLM(vind / 32), 0);

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
	wr32(hw, NGBE_PSRVLANPLM(vind / 32), bits);
	wr32(hw, NGBE_PSRVLAN, NGBE_PSRVLAN_EA | vlan);

	return 0;
}

/**
 *  ngbe_clear_vfta - Clear VLAN filter table
 *  @hw: pointer to hardware structure
 *
 *  Clears the VLAN filer table, and the VMDq index associated with the filter
 **/
s32 ngbe_clear_vfta(struct ngbe_hw *hw)
{
	u32 offset;

	for (offset = 0; offset < hw->mac.vft_size; offset++)
		wr32(hw, NGBE_VLANTBL(offset), 0);

	for (offset = 0; offset < NGBE_NUM_POOL; offset++) {
		wr32(hw, NGBE_PSRVLANIDX, offset);
		wr32(hw, NGBE_PSRVLAN, 0);
		wr32(hw, NGBE_PSRVLANPLM(0), 0);
	}

	return 0;
}

/**
 *  ngbe_check_mac_link_em - Determine link and speed status
 *  @hw: pointer to hardware structure
 *  @speed: pointer to link speed
 *  @link_up: true when link is up
 *  @link_up_wait_to_complete: bool used to wait for link up or not
 *
 *  Reads the links register to determine if link is up and the current speed
 **/
s32 ngbe_check_mac_link_em(struct ngbe_hw *hw, u32 *speed,
			bool *link_up, bool link_up_wait_to_complete)
{
	u32 i;
	s32 status = 0;

	if (hw->lsc) {
		u32 reg;

		reg = rd32(hw, NGBE_GPIOINTSTAT);
		wr32(hw, NGBE_GPIOEOI, reg);
	}

	if (link_up_wait_to_complete) {
		for (i = 0; i < hw->mac.max_link_up_time; i++) {
			status = hw->phy.check_link(hw, speed, link_up);
			if (*link_up)
				break;
			msec_delay(100);
		}
	} else {
		status = hw->phy.check_link(hw, speed, link_up);
	}

	return status;
}

s32 ngbe_get_link_capabilities_em(struct ngbe_hw *hw,
				      u32 *speed,
				      bool *autoneg)
{
	s32 status = 0;
	u16 value = 0;

	hw->mac.autoneg = *autoneg;

	if (hw->phy.type == ngbe_phy_rtl) {
		*speed = NGBE_LINK_SPEED_1GB_FULL |
			NGBE_LINK_SPEED_100M_FULL |
			NGBE_LINK_SPEED_10M_FULL;
	}

	if (hw->phy.type == ngbe_phy_yt8521s_sfi) {
		ngbe_read_phy_reg_ext_yt(hw, YT_CHIP, 0, &value);
		if ((value & YT_CHIP_MODE_MASK) == YT_CHIP_MODE_SEL(1))
			*speed = NGBE_LINK_SPEED_1GB_FULL;
	}

	return status;
}

s32 ngbe_setup_mac_link_em(struct ngbe_hw *hw,
			       u32 speed,
			       bool autoneg_wait_to_complete)
{
	s32 status;

	/* Setup the PHY according to input speed */
	status = hw->phy.setup_link(hw, speed, autoneg_wait_to_complete);

	return status;
}

/**
 *  ngbe_set_mac_anti_spoofing - Enable/Disable MAC anti-spoofing
 *  @hw: pointer to hardware structure
 *  @enable: enable or disable switch for MAC anti-spoofing
 *  @vf: Virtual Function pool - VF Pool to set for MAC anti-spoofing
 *
 **/
void ngbe_set_mac_anti_spoofing(struct ngbe_hw *hw, bool enable, int vf)
{
	u32 pfvfspoof;

	pfvfspoof = rd32(hw, NGBE_POOLTXASMAC);
	if (enable)
		pfvfspoof |= (1 << vf);
	else
		pfvfspoof &= ~(1 << vf);
	wr32(hw, NGBE_POOLTXASMAC, pfvfspoof);
}

/**
 * ngbe_set_pba - Initialize Rx packet buffer
 * @hw: pointer to hardware structure
 * @headroom: reserve n KB of headroom
 **/
void ngbe_set_pba(struct ngbe_hw *hw)
{
	u32 rxpktsize = hw->mac.rx_pb_size;
	u32 txpktsize, txpbthresh;

	/* Reserve 256 KB of headroom */
	rxpktsize -= 256;

	rxpktsize <<= 10;
	wr32(hw, NGBE_PBRXSIZE, rxpktsize);

	/* Only support an equally distributed Tx packet buffer strategy. */
	txpktsize = NGBE_PBTXSIZE_MAX;
	txpbthresh = (txpktsize / 1024) - NGBE_TXPKT_SIZE_MAX;

	wr32(hw, NGBE_PBTXSIZE, txpktsize);
	wr32(hw, NGBE_PBTXDMATH, txpbthresh);
}

/**
 *  ngbe_set_vlan_anti_spoofing - Enable/Disable VLAN anti-spoofing
 *  @hw: pointer to hardware structure
 *  @enable: enable or disable switch for VLAN anti-spoofing
 *  @vf: Virtual Function pool - VF Pool to set for VLAN anti-spoofing
 *
 **/
void ngbe_set_vlan_anti_spoofing(struct ngbe_hw *hw, bool enable, int vf)
{
	u32 pfvfspoof;

	pfvfspoof = rd32(hw, NGBE_POOLTXASVLAN);
	if (enable)
		pfvfspoof |= (1 << vf);
	else
		pfvfspoof &= ~(1 << vf);
	wr32(hw, NGBE_POOLTXASVLAN, pfvfspoof);
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

s32 ngbe_mac_check_overtemp(struct ngbe_hw *hw)
{
	s32 status = 0;
	u32 ts_state;

	/* Check that the LASI temp alarm status was triggered */
	ts_state = rd32(hw, NGBE_TSALM);

	if (ts_state & NGBE_TSALM_HI)
		status = NGBE_ERR_UNDERTEMP;
	else if (ts_state & NGBE_TSALM_LO)
		status = NGBE_ERR_OVERTEMP;

	return status;
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

void ngbe_enable_rx(struct ngbe_hw *hw)
{
	u32 pfdtxgswc;

	wr32m(hw, NGBE_MACRXCFG, NGBE_MACRXCFG_ENA, NGBE_MACRXCFG_ENA);
	wr32m(hw, NGBE_PBRXCTL, NGBE_PBRXCTL_ENA, NGBE_PBRXCTL_ENA);

	if (hw->mac.set_lben) {
		pfdtxgswc = rd32(hw, NGBE_PSRCTL);
		pfdtxgswc |= NGBE_PSRCTL_LBENA;
		wr32(hw, NGBE_PSRCTL, pfdtxgswc);
		hw->mac.set_lben = false;
	}
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

	if (hw->vendor_id != PCI_VENDOR_ID_WANGXUN) {
		DEBUGOUT("Unsupported vendor id: %x", hw->vendor_id);
		return NGBE_ERR_DEVICE_NOT_SUPPORTED;
	}

	switch (hw->sub_device_id) {
	case NGBE_SUB_DEV_ID_EM_RTL_SGMII:
	case NGBE_SUB_DEV_ID_EM_MVL_RGMII:
		hw->phy.media_type = ngbe_media_type_copper;
		hw->mac.type = ngbe_mac_em;
		hw->mac.link_type = ngbe_link_copper;
		break;
	case NGBE_SUB_DEV_ID_EM_RTL_YT8521S_SFP:
		hw->phy.media_type = ngbe_media_type_copper;
		hw->mac.type = ngbe_mac_em;
		hw->mac.link_type = ngbe_link_fiber;
		break;
	case NGBE_SUB_DEV_ID_EM_MVL_SFP:
	case NGBE_SUB_DEV_ID_EM_YT8521S_SFP:
		hw->phy.media_type = ngbe_media_type_fiber;
		hw->mac.type = ngbe_mac_em;
		hw->mac.link_type = ngbe_link_fiber;
		break;
	case NGBE_SUB_DEV_ID_EM_MVL_MIX:
		hw->phy.media_type = ngbe_media_type_unknown;
		hw->mac.type = ngbe_mac_em;
		hw->mac.link_type = ngbe_link_type_unknown;
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

	DEBUGOUT("found mac: %d media: %d, returns: %d",
		  hw->mac.type, hw->phy.media_type, err);
	return err;
}

/**
 *  ngbe_enable_rx_dma - Enable the Rx DMA unit
 *  @hw: pointer to hardware structure
 *  @regval: register value to write to RXCTRL
 *
 *  Enables the Rx DMA unit
 **/
s32 ngbe_enable_rx_dma(struct ngbe_hw *hw, u32 regval)
{
	/*
	 * Workaround silicon errata when enabling the Rx datapath.
	 * If traffic is incoming before we enable the Rx unit, it could hang
	 * the Rx DMA unit.  Therefore, make sure the security engine is
	 * completely disabled prior to enabling the Rx unit.
	 */
	hw->mac.disable_sec_rx_path(hw);

	if (regval & NGBE_PBRXCTL_ENA)
		ngbe_enable_rx(hw);
	else
		ngbe_disable_rx(hw);

	hw->mac.enable_sec_rx_path(hw);

	return 0;
}

/* cmd_addr is used for some special command:
 * 1. to be sector address, when implemented erase sector command
 * 2. to be flash address when implemented read, write flash address
 *
 * Return 0 on success, return 1 on failure.
 */
u32 ngbe_fmgr_cmd_op(struct ngbe_hw *hw, u32 cmd, u32 cmd_addr)
{
	u32 cmd_val, i;

	cmd_val = NGBE_SPICMD_CMD(cmd) | NGBE_SPICMD_CLK(3) | cmd_addr;
	wr32(hw, NGBE_SPICMD, cmd_val);

	for (i = 0; i < NGBE_SPI_TIMEOUT; i++) {
		if (rd32(hw, NGBE_SPISTAT) & NGBE_SPISTAT_OPDONE)
			break;

		usec_delay(10);
	}
	if (i == NGBE_SPI_TIMEOUT)
		return 1;

	return 0;
}

u32 ngbe_flash_read_dword(struct ngbe_hw *hw, u32 addr)
{
	u32 status;

	status = ngbe_fmgr_cmd_op(hw, 1, addr);
	if (status == 0x1) {
		DEBUGOUT("Read flash timeout.");
		return status;
	}

	return rd32(hw, NGBE_SPIDAT);
}

void ngbe_read_efuse(struct ngbe_hw *hw)
{
	u32 efuse[2];
	u8 lan_id = hw->bus.lan_id;

	efuse[0] = ngbe_flash_read_dword(hw, 0xfe010 + lan_id * 8);
	efuse[1] = ngbe_flash_read_dword(hw, 0xfe010 + lan_id * 8 + 4);

	DEBUGOUT("port %d efuse[0] = %08x, efuse[1] = %08x\n",
		lan_id, efuse[0], efuse[1]);

	hw->gphy_efuse[0] = efuse[0];
	hw->gphy_efuse[1] = efuse[1];
}

void ngbe_map_device_id(struct ngbe_hw *hw)
{
	u16 oem = hw->sub_system_id & NGBE_OEM_MASK;

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
		if (oem == NGBE_M88E1512_SFP || oem == NGBE_LY_M88E1512_SFP)
			hw->sub_device_id = NGBE_SUB_DEV_ID_EM_MVL_SFP;
		else if (oem == NGBE_M88E1512_RJ45 ||
			(hw->sub_system_id == NGBE_SUB_DEV_ID_EM_M88E1512_RJ45))
			hw->sub_device_id = NGBE_SUB_DEV_ID_EM_MVL_RGMII;
		else if (oem == NGBE_M88E1512_MIX)
			hw->sub_device_id = NGBE_SUB_DEV_ID_EM_MVL_MIX;
		else if (oem == NGBE_YT8521S_SFP ||
			 oem == NGBE_YT8521S_SFP_GPIO ||
			 oem == NGBE_LY_YT8521S_SFP)
			hw->sub_device_id = NGBE_SUB_DEV_ID_EM_YT8521S_SFP;
		else if (oem == NGBE_INTERNAL_YT8521S_SFP ||
			 oem == NGBE_INTERNAL_YT8521S_SFP_GPIO)
			hw->sub_device_id = NGBE_SUB_DEV_ID_EM_RTL_YT8521S_SFP;
		else
			hw->sub_device_id = NGBE_SUB_DEV_ID_EM_RTL_SGMII;
		break;
	default:
		break;
	}

	if (oem == NGBE_LY_M88E1512_SFP || oem == NGBE_YT8521S_SFP_GPIO ||
			oem == NGBE_INTERNAL_YT8521S_SFP_GPIO ||
			oem == NGBE_LY_YT8521S_SFP)
		hw->gpio_ctl = true;
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
	struct ngbe_phy_info *phy = &hw->phy;
	struct ngbe_rom_info *rom = &hw->rom;
	struct ngbe_mbx_info *mbx = &hw->mbx;

	/* BUS */
	bus->set_lan_id = ngbe_set_lan_id_multi_port;

	/* PHY */
	phy->identify = ngbe_identify_phy;
	phy->read_reg = ngbe_read_phy_reg;
	phy->write_reg = ngbe_write_phy_reg;
	phy->read_reg_unlocked = ngbe_read_phy_reg_mdi;
	phy->write_reg_unlocked = ngbe_write_phy_reg_mdi;
	phy->reset_hw = ngbe_reset_phy;
	phy->led_oem_chk = ngbe_phy_led_oem_chk;

	/* MAC */
	mac->init_hw = ngbe_init_hw;
	mac->reset_hw = ngbe_reset_hw_em;
	mac->start_hw = ngbe_start_hw;
	mac->clear_hw_cntrs = ngbe_clear_hw_cntrs;
	mac->enable_rx_dma = ngbe_enable_rx_dma;
	mac->get_mac_addr = ngbe_get_mac_addr;
	mac->stop_hw = ngbe_stop_hw;
	mac->acquire_swfw_sync = ngbe_acquire_swfw_sync;
	mac->release_swfw_sync = ngbe_release_swfw_sync;

	mac->disable_sec_rx_path = ngbe_disable_sec_rx_path;
	mac->enable_sec_rx_path = ngbe_enable_sec_rx_path;

	/* LEDs */
	mac->led_on = ngbe_led_on;
	mac->led_off = ngbe_led_off;

	/* RAR, VLAN, Multicast */
	mac->set_rar = ngbe_set_rar;
	mac->clear_rar = ngbe_clear_rar;
	mac->init_rx_addrs = ngbe_init_rx_addrs;
	mac->update_mc_addr_list = ngbe_update_mc_addr_list;
	mac->set_vmdq = ngbe_set_vmdq;
	mac->clear_vmdq = ngbe_clear_vmdq;
	mac->set_vfta = ngbe_set_vfta;
	mac->set_vlvf = ngbe_set_vlvf;
	mac->clear_vfta = ngbe_clear_vfta;
	mac->set_mac_anti_spoofing = ngbe_set_mac_anti_spoofing;
	mac->set_vlan_anti_spoofing = ngbe_set_vlan_anti_spoofing;

	/* Flow Control */
	mac->fc_enable = ngbe_fc_enable;
	mac->fc_autoneg = ngbe_fc_autoneg;
	mac->setup_fc = ngbe_setup_fc_em;

	/* Link */
	mac->get_link_capabilities = ngbe_get_link_capabilities_em;
	mac->check_link = ngbe_check_mac_link_em;
	mac->setup_link = ngbe_setup_mac_link_em;

	mac->setup_pba = ngbe_set_pba;

	/* Manageability interface */
	mac->init_thermal_sensor_thresh = ngbe_init_thermal_sensor_thresh;
	mac->check_overtemp = ngbe_mac_check_overtemp;

	mbx->init_params = ngbe_init_mbx_params_pf;
	mbx->read = ngbe_read_mbx_pf;
	mbx->write = ngbe_write_mbx_pf;
	mbx->check_for_msg = ngbe_check_for_msg_pf;
	mbx->check_for_ack = ngbe_check_for_ack_pf;
	mbx->check_for_rst = ngbe_check_for_rst_pf;

	/* EEPROM */
	rom->init_params = ngbe_init_eeprom_params;
	rom->readw_buffer = ngbe_ee_readw_buffer;
	rom->read32 = ngbe_ee_read32;
	rom->writew_buffer = ngbe_ee_writew_buffer;
	rom->validate_checksum = ngbe_validate_eeprom_checksum_em;

	mac->mcft_size		= NGBE_EM_MC_TBL_SIZE;
	mac->vft_size		= NGBE_EM_VFT_TBL_SIZE;
	mac->num_rar_entries	= NGBE_EM_RAR_ENTRIES;
	mac->rx_pb_size		= NGBE_EM_RX_PB_SIZE;
	mac->max_rx_queues	= NGBE_EM_MAX_RX_QUEUES;
	mac->max_tx_queues	= NGBE_EM_MAX_TX_QUEUES;

	mac->default_speeds = NGBE_LINK_SPEED_10M_FULL |
				NGBE_LINK_SPEED_100M_FULL |
				NGBE_LINK_SPEED_1GB_FULL;

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
	hw->mac.max_link_up_time = NGBE_LINK_UP_TIME;

	hw->bus.set_lan_id(hw);

	return status;
}

