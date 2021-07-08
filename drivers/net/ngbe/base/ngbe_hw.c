/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include "ngbe_type.h"
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
	DEBUGFUNC("ngbe_start_hw");

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

	DEBUGFUNC("ngbe_init_hw");

	/* Reset the hardware */
	status = hw->mac.reset_hw(hw);
	if (status == 0) {
		/* Start the HW */
		status = hw->mac.start_hw(hw);
	}

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

	DEBUGFUNC("ngbe_get_mac_addr");

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
 *  ngbe_validate_mac_addr - Validate MAC address
 *  @mac_addr: pointer to MAC address.
 *
 *  Tests a MAC address to ensure it is a valid Individual Address.
 **/
s32 ngbe_validate_mac_addr(u8 *mac_addr)
{
	s32 status = 0;

	DEBUGFUNC("ngbe_validate_mac_addr");

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

	DEBUGFUNC("ngbe_set_rar");

	/* Make sure we are using a valid rar index range */
	if (index >= rar_entries) {
		DEBUGOUT("RAR index %d is out of range.\n", index);
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

	DEBUGFUNC("ngbe_clear_rar");

	/* Make sure we are using a valid rar index range */
	if (index >= rar_entries) {
		DEBUGOUT("RAR index %d is out of range.\n", index);
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

	DEBUGFUNC("ngbe_init_rx_addrs");

	/*
	 * If the current mac address is valid, assume it is a software override
	 * to the permanent address.
	 * Otherwise, use the permanent address from the eeprom.
	 */
	if (ngbe_validate_mac_addr(hw->mac.addr) ==
	    NGBE_ERR_INVALID_MAC_ADDR) {
		/* Get the MAC address from the RAR0 for later reference */
		hw->mac.get_mac_addr(hw, hw->mac.addr);

		DEBUGOUT(" Keeping Current RAR0 Addr =%.2X %.2X %.2X ",
			  hw->mac.addr[0], hw->mac.addr[1],
			  hw->mac.addr[2]);
		DEBUGOUT("%.2X %.2X %.2X\n", hw->mac.addr[3],
			  hw->mac.addr[4], hw->mac.addr[5]);
	} else {
		/* Setup the receive address. */
		DEBUGOUT("Overriding MAC Address in RAR[0]\n");
		DEBUGOUT(" New MAC Addr =%.2X %.2X %.2X ",
			  hw->mac.addr[0], hw->mac.addr[1],
			  hw->mac.addr[2]);
		DEBUGOUT("%.2X %.2X %.2X\n", hw->mac.addr[3],
			  hw->mac.addr[4], hw->mac.addr[5]);

		hw->mac.set_rar(hw, 0, hw->mac.addr, 0, true);
	}

	/* clear VMDq pool/queue selection for RAR 0 */
	hw->mac.clear_vmdq(hw, 0, BIT_MASK32);

	/* Zero out the other receive addresses. */
	DEBUGOUT("Clearing RAR[1-%d]\n", rar_entries - 1);
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

	DEBUGOUT(" Clearing MTA\n");
	for (i = 0; i < hw->mac.mcft_size; i++)
		wr32(hw, NGBE_MCADDRTBL(i), 0);

	ngbe_init_uta_tables(hw);

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

	DEBUGFUNC("ngbe_disable_sec_rx_path");


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
		DEBUGOUT("Rx unit being enabled before security "
			 "path fully disabled.  Continuing with init.\n");

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

	DEBUGFUNC("ngbe_enable_sec_rx_path");

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

	DEBUGFUNC("ngbe_clear_vmdq");

	/* Make sure we are using a valid rar index range */
	if (rar >= rar_entries) {
		DEBUGOUT("RAR index %d is out of range.\n", rar);
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

	DEBUGFUNC("ngbe_set_vmdq");

	/* Make sure we are using a valid rar index range */
	if (rar >= rar_entries) {
		DEBUGOUT("RAR index %d is out of range.\n", rar);
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

	DEBUGFUNC("ngbe_init_uta_tables");
	DEBUGOUT(" Clearing UTA\n");

	for (i = 0; i < 128; i++)
		wr32(hw, NGBE_UCADDRTBL(i), 0);

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
	u32 i, reg;
	s32 status = 0;

	DEBUGFUNC("ngbe_check_mac_link_em");

	reg = rd32(hw, NGBE_GPIOINTSTAT);
	wr32(hw, NGBE_GPIOEOI, reg);

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

	DEBUGFUNC("\n");

	hw->mac.autoneg = *autoneg;

	switch (hw->sub_device_id) {
	case NGBE_SUB_DEV_ID_EM_RTL_SGMII:
		*speed = NGBE_LINK_SPEED_1GB_FULL |
			NGBE_LINK_SPEED_100M_FULL |
			NGBE_LINK_SPEED_10M_FULL;
		break;
	default:
		break;
	}

	return status;
}

s32 ngbe_setup_mac_link_em(struct ngbe_hw *hw,
			       u32 speed,
			       bool autoneg_wait_to_complete)
{
	s32 status;

	DEBUGFUNC("\n");

	/* Setup the PHY according to input speed */
	status = hw->phy.setup_link(hw, speed, autoneg_wait_to_complete);

	return status;
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

s32 ngbe_mac_check_overtemp(struct ngbe_hw *hw)
{
	s32 status = 0;
	u32 ts_state;

	DEBUGFUNC("ngbe_mac_check_overtemp");

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

/**
 *  ngbe_enable_rx_dma - Enable the Rx DMA unit
 *  @hw: pointer to hardware structure
 *  @regval: register value to write to RXCTRL
 *
 *  Enables the Rx DMA unit
 **/
s32 ngbe_enable_rx_dma(struct ngbe_hw *hw, u32 regval)
{
	DEBUGFUNC("ngbe_enable_rx_dma");

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
	struct ngbe_phy_info *phy = &hw->phy;
	struct ngbe_rom_info *rom = &hw->rom;

	DEBUGFUNC("ngbe_init_ops_pf");

	/* BUS */
	bus->set_lan_id = ngbe_set_lan_id_multi_port;

	/* PHY */
	phy->identify = ngbe_identify_phy;
	phy->read_reg = ngbe_read_phy_reg;
	phy->write_reg = ngbe_write_phy_reg;
	phy->read_reg_unlocked = ngbe_read_phy_reg_mdi;
	phy->write_reg_unlocked = ngbe_write_phy_reg_mdi;
	phy->reset_hw = ngbe_reset_phy;

	/* MAC */
	mac->init_hw = ngbe_init_hw;
	mac->reset_hw = ngbe_reset_hw_em;
	mac->start_hw = ngbe_start_hw;
	mac->enable_rx_dma = ngbe_enable_rx_dma;
	mac->get_mac_addr = ngbe_get_mac_addr;
	mac->stop_hw = ngbe_stop_hw;
	mac->acquire_swfw_sync = ngbe_acquire_swfw_sync;
	mac->release_swfw_sync = ngbe_release_swfw_sync;

	mac->disable_sec_rx_path = ngbe_disable_sec_rx_path;
	mac->enable_sec_rx_path = ngbe_enable_sec_rx_path;
	/* RAR */
	mac->set_rar = ngbe_set_rar;
	mac->clear_rar = ngbe_clear_rar;
	mac->init_rx_addrs = ngbe_init_rx_addrs;
	mac->set_vmdq = ngbe_set_vmdq;
	mac->clear_vmdq = ngbe_clear_vmdq;

	/* Link */
	mac->get_link_capabilities = ngbe_get_link_capabilities_em;
	mac->check_link = ngbe_check_mac_link_em;
	mac->setup_link = ngbe_setup_mac_link_em;

	/* Manageability interface */
	mac->init_thermal_sensor_thresh = ngbe_init_thermal_sensor_thresh;
	mac->check_overtemp = ngbe_mac_check_overtemp;

	/* EEPROM */
	rom->init_params = ngbe_init_eeprom_params;
	rom->validate_checksum = ngbe_validate_eeprom_checksum_em;

	mac->mcft_size		= NGBE_EM_MC_TBL_SIZE;
	mac->num_rar_entries	= NGBE_EM_RAR_ENTRIES;
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
	hw->mac.max_link_up_time = NGBE_LINK_UP_TIME;

	hw->bus.set_lan_id(hw);

	return status;
}

