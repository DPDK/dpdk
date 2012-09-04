/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without 
 *   modification, are permitted provided that the following conditions 
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright 
 *       notice, this list of conditions and the following disclaimer in 
 *       the documentation and/or other materials provided with the 
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its 
 *       contributors may be used to endorse or promote products derived 
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 *  version: DPDK.L.1.2.3-3
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>

#include <rte_common.h>
#include <rte_interrupts.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_atomic.h>
#include <rte_malloc.h>

#include "e1000_logs.h"
#include "igb/e1000_api.h"
#include "igb/e1000_hw.h"
#include "e1000_ethdev.h"

static int  eth_igb_configure(struct rte_eth_dev *dev, uint16_t nb_rx_q,
		uint16_t nb_tx_q);
static int  eth_igb_start(struct rte_eth_dev *dev);
static void eth_igb_stop(struct rte_eth_dev *dev);
static void eth_igb_close(struct rte_eth_dev *dev);
static void eth_igb_promiscuous_enable(struct rte_eth_dev *dev);
static void eth_igb_promiscuous_disable(struct rte_eth_dev *dev);
static void eth_igb_allmulticast_enable(struct rte_eth_dev *dev);
static void eth_igb_allmulticast_disable(struct rte_eth_dev *dev);
static int  eth_igb_link_update(struct rte_eth_dev *dev,
				int wait_to_complete);
static void eth_igb_stats_get(struct rte_eth_dev *dev,
				struct rte_eth_stats *rte_stats);
static void eth_igb_stats_reset(struct rte_eth_dev *dev);
static void eth_igb_infos_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);
static int  eth_igb_flow_ctrl_set(struct rte_eth_dev *dev,
				struct rte_eth_fc_conf *fc_conf);
static int eth_igb_interrupt_setup(struct rte_eth_dev *dev);
static int eth_igb_interrupt_get_status(struct rte_eth_dev *dev);
static int eth_igb_interrupt_action(struct rte_eth_dev *dev);
static void eth_igb_interrupt_handler(struct rte_intr_handle *handle,
							void *param);
static int  igb_hardware_init(struct e1000_hw *hw);
static void igb_hw_control_acquire(struct e1000_hw *hw);
static void igb_hw_control_release(struct e1000_hw *hw);
static void igb_init_manageability(struct e1000_hw *hw);
static void igb_release_manageability(struct e1000_hw *hw);
static void igb_vlan_hw_support_enable(struct rte_eth_dev *dev);
static void igb_vlan_hw_support_disable(struct rte_eth_dev *dev);
static void eth_igb_vlan_filter_set(struct rte_eth_dev *dev,
				      uint16_t vlan_id,
				      int on);
static int eth_igb_led_on(struct rte_eth_dev *dev);
static int eth_igb_led_off(struct rte_eth_dev *dev);

static void igb_intr_disable(struct e1000_hw *hw);
static int  igb_get_rx_buffer_size(struct e1000_hw *hw);
static void eth_igb_rar_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
		uint32_t index, uint32_t pool);
static void eth_igb_rar_clear(struct rte_eth_dev *dev, uint32_t index);

#define IGB_FC_PAUSE_TIME 0x0680
#define IGB_LINK_UPDATE_CHECK_TIMEOUT  90  /* 9s */
#define IGB_LINK_UPDATE_CHECK_INTERVAL 100 /* ms */

static enum e1000_fc_mode igb_fc_setting = e1000_fc_full;

/*
 * The set of PCI devices this driver supports
 */
static struct rte_pci_id pci_id_igb_map[] = {

#undef RTE_LIBRTE_IXGBE_PMD
#define RTE_PCI_DEV_ID_DECL(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#include "rte_pci_dev_ids.h"

{.device_id = 0},
};

static struct eth_dev_ops eth_igb_ops = {
	.dev_configure        = eth_igb_configure,
	.dev_start            = eth_igb_start,
	.dev_stop             = eth_igb_stop,
	.dev_close            = eth_igb_close,
	.promiscuous_enable   = eth_igb_promiscuous_enable,
	.promiscuous_disable  = eth_igb_promiscuous_disable,
	.allmulticast_enable  = eth_igb_allmulticast_enable,
	.allmulticast_disable = eth_igb_allmulticast_disable,
	.link_update          = eth_igb_link_update,
	.stats_get            = eth_igb_stats_get,
	.stats_reset          = eth_igb_stats_reset,
	.dev_infos_get        = eth_igb_infos_get,
	.vlan_filter_set      = eth_igb_vlan_filter_set,
	.rx_queue_setup       = eth_igb_rx_queue_setup,
	.tx_queue_setup       = eth_igb_tx_queue_setup,
	.dev_led_on           = eth_igb_led_on,
	.dev_led_off          = eth_igb_led_off,
	.flow_ctrl_set        = eth_igb_flow_ctrl_set,
	.mac_addr_add         = eth_igb_rar_set,
	.mac_addr_remove      = eth_igb_rar_clear,
};

/**
 * Atomically reads the link status information from global
 * structure rte_eth_dev.
 *
 * @param dev
 *   - Pointer to the structure rte_eth_dev to read from.
 *   - Pointer to the buffer to be saved with the link status.
 *
 * @return
 *   - On success, zero.
 *   - On failure, negative value.
 */
static inline int
rte_igb_dev_atomic_read_link_status(struct rte_eth_dev *dev,
				struct rte_eth_link *link)
{
	struct rte_eth_link *dst = link;
	struct rte_eth_link *src = &(dev->data->dev_link);

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

/**
 * Atomically writes the link status information into global
 * structure rte_eth_dev.
 *
 * @param dev
 *   - Pointer to the structure rte_eth_dev to read from.
 *   - Pointer to the buffer to be saved with the link status.
 *
 * @return
 *   - On success, zero.
 *   - On failure, negative value.
 */
static inline int
rte_igb_dev_atomic_write_link_status(struct rte_eth_dev *dev,
				struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &(dev->data->dev_link);
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static void
igb_identify_hardware(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	hw->vendor_id = dev->pci_dev->id.vendor_id;
	hw->device_id = dev->pci_dev->id.device_id;
	hw->subsystem_vendor_id = dev->pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = dev->pci_dev->id.subsystem_device_id;

	e1000_set_mac_type(hw);

	/* need to check if it is a vf device below */
}

static int
eth_igb_dev_init(__attribute__((unused)) struct eth_driver *eth_drv,
		   struct rte_eth_dev *eth_dev)
{
	int error = 0;
	struct rte_pci_device *pci_dev;
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct e1000_vfta * shadow_vfta =
		E1000_DEV_PRIVATE_TO_VFTA(eth_dev->data->dev_private);

	pci_dev = eth_dev->pci_dev;
	eth_dev->dev_ops = &eth_igb_ops;
	eth_dev->rx_pkt_burst = &eth_igb_recv_pkts;
	eth_dev->tx_pkt_burst = &eth_igb_xmit_pkts;

	/* for secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY){
		if (eth_dev->data->scattered_rx)
			eth_dev->rx_pkt_burst = &eth_igb_recv_scattered_pkts;
		return 0;
	}

	hw->hw_addr= (void *)pci_dev->mem_resource.addr;

	igb_identify_hardware(eth_dev);

	if (e1000_setup_init_funcs(hw, TRUE) != E1000_SUCCESS) {
		error = -EIO;
		goto err_late;
	}

	e1000_get_bus_info(hw);

	hw->mac.autoneg = 1;
	hw->phy.autoneg_wait_to_complete = 0;
	hw->phy.autoneg_advertised = E1000_ALL_SPEED_DUPLEX;

	/* Copper options */
	if (hw->phy.media_type == e1000_media_type_copper) {
		hw->phy.mdix = 0; /* AUTO_ALL_MODES */
		hw->phy.disable_polarity_correction = 0;
		hw->phy.ms_type = e1000_ms_hw_default;
	}

	/*
	 * Start from a known state, this is important in reading the nvm
	 * and mac from that.
	 */
	e1000_reset_hw(hw);

	/* Make sure we have a good EEPROM before we read from it */
	if (e1000_validate_nvm_checksum(hw) < 0) {
		/*
		 * Some PCI-E parts fail the first check due to
		 * the link being in sleep state, call it again,
		 * if it fails a second time its a real issue.
		 */
		if (e1000_validate_nvm_checksum(hw) < 0) {
			PMD_INIT_LOG(ERR, "EEPROM checksum invalid");
			error = -EIO;
			goto err_late;
		}
	}

	/* Read the permanent MAC address out of the EEPROM */
	if (e1000_read_mac_addr(hw) != 0) {
		PMD_INIT_LOG(ERR, "EEPROM error while reading MAC address");
		error = -EIO;
		goto err_late;
	}

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("e1000",
		ETHER_ADDR_LEN * hw->mac.rar_entry_count, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate %d bytes needed to "
						"store MAC addresses",
				ETHER_ADDR_LEN * hw->mac.rar_entry_count);
		error = -ENOMEM;
		goto err_late;
	}

	/* Copy the permanent MAC address */
	ether_addr_copy((struct ether_addr *)hw->mac.addr, &eth_dev->data->mac_addrs[0]);

	/* initialize the vfta */
	memset(shadow_vfta, 0, sizeof(*shadow_vfta));

	/* Now initialize the hardware */
	if (igb_hardware_init(hw) != 0) {
		PMD_INIT_LOG(ERR, "Hardware initialization failed");
		rte_free(eth_dev->data->mac_addrs);
		eth_dev->data->mac_addrs = NULL;
		error = -ENODEV;
		goto err_late;
	}
	hw->mac.get_link_status = 1;

	/* Indicate SOL/IDER usage */
	if (e1000_check_reset_block(hw) < 0) {
		PMD_INIT_LOG(ERR, "PHY reset is blocked due to"
					"SOL/IDER session");
	}

	PMD_INIT_LOG(INFO, "port_id %d vendorID=0x%x deviceID=0x%x\n",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id);

	rte_intr_callback_register(&(pci_dev->intr_handle),
		eth_igb_interrupt_handler, (void *)eth_dev);

	return 0;

err_late:
	igb_hw_control_release(hw);

	return (error);
}

static struct eth_driver rte_igb_pmd = {
	{
		.name = "rte_igb_pmd",
		.id_table = pci_id_igb_map,
		.drv_flags = RTE_PCI_DRV_NEED_IGB_UIO,
	},
	.eth_dev_init = eth_igb_dev_init,
	.dev_private_size = sizeof(struct e1000_adapter),
};

int
rte_igb_pmd_init(void)
{
	rte_eth_driver_register(&rte_igb_pmd);
	return 0;
}

static int
eth_igb_configure(struct rte_eth_dev *dev, uint16_t nb_rx_q, uint16_t nb_tx_q)
{
	struct e1000_interrupt *intr =
		E1000_DEV_PRIVATE_TO_INTR(dev->data->dev_private);
	int diag;

	PMD_INIT_LOG(DEBUG, ">>");

	intr->flags |= E1000_FLAG_NEED_LINK_UPDATE;

	/* Allocate the array of pointers to RX structures */
	diag = igb_dev_rx_queue_alloc(dev, nb_rx_q);
	if (diag != 0) {
		PMD_INIT_LOG(ERR, "ethdev port_id=%u allocation of array of %u"
					" pointers to RX queues failed",
					dev->data->port_id, nb_rx_q);
		return diag;
	}

	/* Allocate the array of pointers to TX structures */
	diag = igb_dev_tx_queue_alloc(dev, nb_tx_q);
	if (diag != 0) {
		PMD_INIT_LOG(ERR, "ethdev port_id=%u allocation of array of %u"
					" pointers to TX queues failed",
					dev->data->port_id, nb_tx_q);

		return diag;
	}

	PMD_INIT_LOG(DEBUG, "<<");

	return (0);
}

static int
eth_igb_start(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int ret, i;

	PMD_INIT_LOG(DEBUG, ">>");

	igb_intr_disable(hw);

	/* Power up the phy. Needed to make the link go Up */
	e1000_power_up_phy(hw);

	/*
	 * Packet Buffer Allocation (PBA)
	 * Writing PBA sets the receive portion of the buffer
	 * the remainder is used for the transmit buffer.
	 */
	if (hw->mac.type == e1000_82575) {
		uint32_t pba;

		pba = E1000_PBA_32K; /* 32K for Rx, 16K for Tx */
		E1000_WRITE_REG(hw, E1000_PBA, pba);
	}

	/* Put the address into the Receive Address Array */
	e1000_rar_set(hw, hw->mac.addr, 0);

	/* Initialize the hardware */
	if (igb_hardware_init(hw)) {
		PMD_INIT_LOG(ERR, "Unable to initialize the hardware");
		return (-1);
	}

	E1000_WRITE_REG(hw, E1000_VET, ETHER_TYPE_VLAN);

	/* Configure for OS presence */
	igb_init_manageability(hw);

	eth_igb_tx_init(dev);

	/* This can fail when allocating mbufs for descriptor rings */
	ret = eth_igb_rx_init(dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Unable to initialize RX hardware");
		return ret;
	}

	e1000_clear_hw_cntrs_base_generic(hw);

	/*
	 * If VLAN filtering is enabled, set up VLAN tag offload and filtering
	 * and restore the VFTA.
	 */
	if (dev->data->dev_conf.rxmode.hw_vlan_filter)
		igb_vlan_hw_support_enable(dev);
	else
		igb_vlan_hw_support_disable(dev);

	/*
	 * Configure the Interrupt Moderation register (EITR) with the maximum
	 * possible value (0xFFFF) to minimize "System Partial Write" issued by
	 * spurious [DMA] memory updates of RX and TX ring descriptors.
	 *
	 * With a EITR granularity of 2 microseconds in the 82576, only 7/8
	 * spurious memory updates per second should be expected.
	 * ((65535 * 2) / 1000.1000 ~= 0.131 second).
	 *
	 * Because interrupts are not used at all, the MSI-X is not activated
	 * and interrupt moderation is controlled by EITR[0].
	 *
	 * Note that having [almost] disabled memory updates of RX and TX ring
	 * descriptors through the Interrupt Moderation mechanism, memory
	 * updates of ring descriptors are now moderated by the configurable
	 * value of Write-Back Threshold registers.
	 */
	if ((hw->mac.type == e1000_82576) || (hw->mac.type == e1000_82580) ||
			(hw->mac.type == e1000_i350)) {
		uint32_t ivar;

		/* Enable all RX & TX queues in the IVAR registers */
		ivar = (uint32_t) ((E1000_IVAR_VALID << 16) | E1000_IVAR_VALID);
		for (i = 0; i < 8; i++)
			E1000_WRITE_REG_ARRAY(hw, E1000_IVAR0, i, ivar);

		/* Configure EITR with the maximum possible value (0xFFFF) */
		E1000_WRITE_REG(hw, E1000_EITR(0), 0xFFFF);
	}

	/* Don't reset the phy next time init gets called */
	hw->phy.reset_disable = 1;

	/* Setup link speed and duplex */
	switch (dev->data->dev_conf.link_speed) {
	case ETH_LINK_SPEED_AUTONEG:
		if (dev->data->dev_conf.link_duplex == ETH_LINK_AUTONEG_DUPLEX)
			hw->phy.autoneg_advertised = E1000_ALL_SPEED_DUPLEX;
		else if (dev->data->dev_conf.link_duplex == ETH_LINK_HALF_DUPLEX)
			hw->phy.autoneg_advertised = E1000_ALL_HALF_DUPLEX;
		else if (dev->data->dev_conf.link_duplex == ETH_LINK_FULL_DUPLEX)
			hw->phy.autoneg_advertised = E1000_ALL_FULL_DUPLEX;
		else
			goto error_invalid_config;
		break;
	case ETH_LINK_SPEED_10:
		if (dev->data->dev_conf.link_duplex == ETH_LINK_AUTONEG_DUPLEX)
			hw->phy.autoneg_advertised = E1000_ALL_10_SPEED;
		else if (dev->data->dev_conf.link_duplex == ETH_LINK_HALF_DUPLEX)
			hw->phy.autoneg_advertised = ADVERTISE_10_HALF;
		else if (dev->data->dev_conf.link_duplex == ETH_LINK_FULL_DUPLEX)
			hw->phy.autoneg_advertised = ADVERTISE_10_FULL;
		else
			goto error_invalid_config;
		break;
	case ETH_LINK_SPEED_100:
		if (dev->data->dev_conf.link_duplex == ETH_LINK_AUTONEG_DUPLEX)
			hw->phy.autoneg_advertised = E1000_ALL_100_SPEED;
		else if (dev->data->dev_conf.link_duplex == ETH_LINK_HALF_DUPLEX)
			hw->phy.autoneg_advertised = ADVERTISE_100_HALF;
		else if (dev->data->dev_conf.link_duplex == ETH_LINK_FULL_DUPLEX)
			hw->phy.autoneg_advertised = ADVERTISE_100_FULL;
		else
			goto error_invalid_config;
		break;
	case ETH_LINK_SPEED_1000:
		if ((dev->data->dev_conf.link_duplex == ETH_LINK_AUTONEG_DUPLEX) ||
				(dev->data->dev_conf.link_duplex == ETH_LINK_FULL_DUPLEX))
			hw->phy.autoneg_advertised = ADVERTISE_1000_FULL;
		else
			goto error_invalid_config;
		break;
	case ETH_LINK_SPEED_10000:
	default:
		goto error_invalid_config;
	}
	e1000_setup_link(hw);

	PMD_INIT_LOG(DEBUG, "<<");

	/* check if lsc interrupt feature is enabled */
	if (dev->data->dev_conf.intr_conf.lsc != 0)
		return eth_igb_interrupt_setup(dev);

	return (0);

error_invalid_config:
	PMD_INIT_LOG(ERR, "Invalid link_speed/link_duplex (%u/%u) for port %u\n",
			dev->data->dev_conf.link_speed,
			dev->data->dev_conf.link_duplex, dev->data->port_id);
	return -1;
}

/*********************************************************************
 *
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC.
 *
 **********************************************************************/
static void
eth_igb_stop(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_link link;

	igb_intr_disable(hw);
	e1000_reset_hw(hw);
	E1000_WRITE_REG(hw, E1000_WUC, 0);

	/* Power down the phy. Needed to make the link go Down */
	e1000_power_down_phy(hw);

	igb_dev_clear_queues(dev);

	/* clear the recorded link status */
	memset(&link, 0, sizeof(link));
	rte_igb_dev_atomic_write_link_status(dev, &link);
}

static void
eth_igb_close(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_link link;

	eth_igb_stop(dev);
	e1000_phy_hw_reset(hw);
	igb_release_manageability(hw);
	igb_hw_control_release(hw);

	igb_dev_clear_queues(dev);

	memset(&link, 0, sizeof(link));
	rte_igb_dev_atomic_write_link_status(dev, &link);
}

static int
igb_get_rx_buffer_size(struct e1000_hw *hw)
{
	uint32_t rx_buf_size;
	if (hw->mac.type == e1000_82576) {
		rx_buf_size = (E1000_READ_REG(hw, E1000_RXPBS) & 0xffff) << 10;
	} else if (hw->mac.type == e1000_82580) {
		/* PBS needs to be translated according to a lookup table */
		rx_buf_size = (E1000_READ_REG(hw, E1000_RXPBS) & 0xf);
		rx_buf_size = (uint32_t) e1000_rxpbs_adjust_82580(rx_buf_size);
		rx_buf_size = (rx_buf_size << 10);
	} else {
		rx_buf_size = (E1000_READ_REG(hw, E1000_PBA) & 0xffff) << 10;
	}

	return rx_buf_size;
}

/*********************************************************************
 *
 *  Initialize the hardware
 *
 **********************************************************************/
static int
igb_hardware_init(struct e1000_hw *hw)
{
	uint32_t rx_buf_size;
	int diag;

	/* Let the firmware know the OS is in control */
	igb_hw_control_acquire(hw);

	/*
	 * These parameters control the automatic generation (Tx) and
	 * response (Rx) to Ethernet PAUSE frames.
	 * - High water mark should allow for at least two standard size (1518)
	 *   frames to be received after sending an XOFF.
	 * - Low water mark works best when it is very near the high water mark.
	 *   This allows the receiver to restart by sending XON when it has
	 *   drained a bit. Here we use an arbitary value of 1500 which will
	 *   restart after one full frame is pulled from the buffer. There
	 *   could be several smaller frames in the buffer and if so they will
	 *   not trigger the XON until their total number reduces the buffer
	 *   by 1500.
	 * - The pause time is fairly large at 1000 x 512ns = 512 usec.
	 */
	rx_buf_size = igb_get_rx_buffer_size(hw);

	hw->fc.high_water = rx_buf_size - (ETHER_MAX_LEN * 2);
	hw->fc.low_water = hw->fc.high_water - 1500;
	hw->fc.pause_time = IGB_FC_PAUSE_TIME;
	hw->fc.send_xon = 1;

	/* Set Flow control, use the tunable location if sane */
	if ((igb_fc_setting != e1000_fc_none) && (igb_fc_setting < 4))
		hw->fc.requested_mode = igb_fc_setting;
	else
		hw->fc.requested_mode = e1000_fc_none;

	/* Issue a global reset */
	e1000_reset_hw(hw);
	E1000_WRITE_REG(hw, E1000_WUC, 0);

	diag = e1000_init_hw(hw);
	if (diag < 0)
		return (diag);

	E1000_WRITE_REG(hw, E1000_VET, ETHER_TYPE_VLAN);
	e1000_get_phy_info(hw);
	e1000_check_for_link(hw);

	return (0);
}

/* This function is based on igb_update_stats_counters() in igb/if_igb.c */
static void
eth_igb_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *rte_stats)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_hw_stats *stats =
			E1000_DEV_PRIVATE_TO_STATS(dev->data->dev_private);
	int pause_frames;

	if(hw->phy.media_type == e1000_media_type_copper ||
	    (E1000_READ_REG(hw, E1000_STATUS) & E1000_STATUS_LU)) {
		stats->symerrs +=
		    E1000_READ_REG(hw,E1000_SYMERRS);
		stats->sec += E1000_READ_REG(hw, E1000_SEC);
	}

	stats->crcerrs += E1000_READ_REG(hw, E1000_CRCERRS);
	stats->mpc += E1000_READ_REG(hw, E1000_MPC);
	stats->scc += E1000_READ_REG(hw, E1000_SCC);
	stats->ecol += E1000_READ_REG(hw, E1000_ECOL);

	stats->mcc += E1000_READ_REG(hw, E1000_MCC);
	stats->latecol += E1000_READ_REG(hw, E1000_LATECOL);
	stats->colc += E1000_READ_REG(hw, E1000_COLC);
	stats->dc += E1000_READ_REG(hw, E1000_DC);
	stats->rlec += E1000_READ_REG(hw, E1000_RLEC);
	stats->xonrxc += E1000_READ_REG(hw, E1000_XONRXC);
	stats->xontxc += E1000_READ_REG(hw, E1000_XONTXC);
	/*
	** For watchdog management we need to know if we have been
	** paused during the last interval, so capture that here.
	*/
	pause_frames = E1000_READ_REG(hw, E1000_XOFFRXC);
	stats->xoffrxc += pause_frames;
	stats->xofftxc += E1000_READ_REG(hw, E1000_XOFFTXC);
	stats->fcruc += E1000_READ_REG(hw, E1000_FCRUC);
	stats->prc64 += E1000_READ_REG(hw, E1000_PRC64);
	stats->prc127 += E1000_READ_REG(hw, E1000_PRC127);
	stats->prc255 += E1000_READ_REG(hw, E1000_PRC255);
	stats->prc511 += E1000_READ_REG(hw, E1000_PRC511);
	stats->prc1023 += E1000_READ_REG(hw, E1000_PRC1023);
	stats->prc1522 += E1000_READ_REG(hw, E1000_PRC1522);
	stats->gprc += E1000_READ_REG(hw, E1000_GPRC);
	stats->bprc += E1000_READ_REG(hw, E1000_BPRC);
	stats->mprc += E1000_READ_REG(hw, E1000_MPRC);
	stats->gptc += E1000_READ_REG(hw, E1000_GPTC);

	/* For the 64-bit byte counters the low dword must be read first. */
	/* Both registers clear on the read of the high dword */

	stats->gorc += E1000_READ_REG(hw, E1000_GORCL);
	stats->gorc += ((uint64_t)E1000_READ_REG(hw, E1000_GORCH) << 32);
	stats->gotc += E1000_READ_REG(hw, E1000_GOTCL);
	stats->gotc += ((uint64_t)E1000_READ_REG(hw, E1000_GOTCH) << 32);

	stats->rnbc += E1000_READ_REG(hw, E1000_RNBC);
	stats->ruc += E1000_READ_REG(hw, E1000_RUC);
	stats->rfc += E1000_READ_REG(hw, E1000_RFC);
	stats->roc += E1000_READ_REG(hw, E1000_ROC);
	stats->rjc += E1000_READ_REG(hw, E1000_RJC);

	stats->tor += E1000_READ_REG(hw, E1000_TORH);
	stats->tot += E1000_READ_REG(hw, E1000_TOTH);

	stats->tpr += E1000_READ_REG(hw, E1000_TPR);
	stats->tpt += E1000_READ_REG(hw, E1000_TPT);
	stats->ptc64 += E1000_READ_REG(hw, E1000_PTC64);
	stats->ptc127 += E1000_READ_REG(hw, E1000_PTC127);
	stats->ptc255 += E1000_READ_REG(hw, E1000_PTC255);
	stats->ptc511 += E1000_READ_REG(hw, E1000_PTC511);
	stats->ptc1023 += E1000_READ_REG(hw, E1000_PTC1023);
	stats->ptc1522 += E1000_READ_REG(hw, E1000_PTC1522);
	stats->mptc += E1000_READ_REG(hw, E1000_MPTC);
	stats->bptc += E1000_READ_REG(hw, E1000_BPTC);

	/* Interrupt Counts */

	stats->iac += E1000_READ_REG(hw, E1000_IAC);
	stats->icrxptc += E1000_READ_REG(hw, E1000_ICRXPTC);
	stats->icrxatc += E1000_READ_REG(hw, E1000_ICRXATC);
	stats->ictxptc += E1000_READ_REG(hw, E1000_ICTXPTC);
	stats->ictxatc += E1000_READ_REG(hw, E1000_ICTXATC);
	stats->ictxqec += E1000_READ_REG(hw, E1000_ICTXQEC);
	stats->ictxqmtc += E1000_READ_REG(hw, E1000_ICTXQMTC);
	stats->icrxdmtc += E1000_READ_REG(hw, E1000_ICRXDMTC);
	stats->icrxoc += E1000_READ_REG(hw, E1000_ICRXOC);

	/* Host to Card Statistics */

	stats->cbtmpc += E1000_READ_REG(hw, E1000_CBTMPC);
	stats->htdpmc += E1000_READ_REG(hw, E1000_HTDPMC);
	stats->cbrdpc += E1000_READ_REG(hw, E1000_CBRDPC);
	stats->cbrmpc += E1000_READ_REG(hw, E1000_CBRMPC);
	stats->rpthc += E1000_READ_REG(hw, E1000_RPTHC);
	stats->hgptc += E1000_READ_REG(hw, E1000_HGPTC);
	stats->htcbdpc += E1000_READ_REG(hw, E1000_HTCBDPC);
	stats->hgorc += E1000_READ_REG(hw, E1000_HGORCL);
	stats->hgorc += ((uint64_t)E1000_READ_REG(hw, E1000_HGORCH) << 32);
	stats->hgotc += E1000_READ_REG(hw, E1000_HGOTCL);
	stats->hgotc += ((uint64_t)E1000_READ_REG(hw, E1000_HGOTCH) << 32);
	stats->lenerrs += E1000_READ_REG(hw, E1000_LENERRS);
	stats->scvpc += E1000_READ_REG(hw, E1000_SCVPC);
	stats->hrmpc += E1000_READ_REG(hw, E1000_HRMPC);

	stats->algnerrc += E1000_READ_REG(hw, E1000_ALGNERRC);
	stats->rxerrc += E1000_READ_REG(hw, E1000_RXERRC);
	stats->tncrs += E1000_READ_REG(hw, E1000_TNCRS);
	stats->cexterr += E1000_READ_REG(hw, E1000_CEXTERR);
	stats->tsctc += E1000_READ_REG(hw, E1000_TSCTC);
	stats->tsctfc += E1000_READ_REG(hw, E1000_TSCTFC);

	if (rte_stats == NULL)
		return;

	/* Rx Errors */
	rte_stats->ierrors = stats->rxerrc + stats->crcerrs + stats->algnerrc +
	    stats->ruc + stats->roc + stats->mpc + stats->cexterr;

	/* Tx Errors */
	rte_stats->oerrors = stats->ecol + stats->latecol;

	rte_stats->ipackets = stats->gprc;
	rte_stats->opackets = stats->gptc;
	rte_stats->ibytes   = stats->gorc;
	rte_stats->obytes   = stats->gotc;
}

static void
eth_igb_stats_reset(struct rte_eth_dev *dev)
{
	struct e1000_hw_stats *hw_stats =
			E1000_DEV_PRIVATE_TO_STATS(dev->data->dev_private);

	/* HW registers are cleared on read */
	eth_igb_stats_get(dev, NULL);

	/* Reset software totals */
	memset(hw_stats, 0, sizeof(*hw_stats));
}

static void
eth_igb_infos_get(struct rte_eth_dev *dev,
		    struct rte_eth_dev_info *dev_info)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	dev_info->min_rx_bufsize = 256; /* See BSIZE field of RCTL register. */
	dev_info->max_rx_pktlen  = 0x3FFF; /* See RLPML register. */
	dev_info->max_mac_addrs = hw->mac.rar_entry_count;

	switch (hw->mac.type) {
	case e1000_82575:
		dev_info->max_rx_queues = 4;
		dev_info->max_tx_queues = 4;
		break;

	case e1000_82576:
		dev_info->max_rx_queues = 16;
		dev_info->max_tx_queues = 16;
		break;

	case e1000_82580:
		dev_info->max_rx_queues = 8;
		dev_info->max_tx_queues = 8;
		break;

	case e1000_i350:
		dev_info->max_rx_queues = 8;
		dev_info->max_tx_queues = 8;
		break;

	default:
		/* Should not happen */
		dev_info->max_rx_queues = 0;
		dev_info->max_tx_queues = 0;
	}
}

/* return 0 means link status changed, -1 means not changed */
static int
eth_igb_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_link link, old;
	int link_check, count;

	link_check = 0;
	hw->mac.get_link_status = 1;

	/* possible wait-to-complete in up to 9 seconds */
	for (count = 0; count < IGB_LINK_UPDATE_CHECK_TIMEOUT; count ++) {
		/* Read the real link status */
		switch (hw->phy.media_type) {
		case e1000_media_type_copper:
			/* Do the work to read phy */
			e1000_check_for_link(hw);
			link_check = !hw->mac.get_link_status;
			break;

		case e1000_media_type_fiber:
			e1000_check_for_link(hw);
			link_check = (E1000_READ_REG(hw, E1000_STATUS) &
				      E1000_STATUS_LU);
			break;

		case e1000_media_type_internal_serdes:
			e1000_check_for_link(hw);
			link_check = hw->mac.serdes_has_link;
			break;

		default:
		case e1000_media_type_unknown:
			break;
		}
		if (link_check || wait_to_complete == 0)
			break;
		rte_delay_ms(IGB_LINK_UPDATE_CHECK_INTERVAL);
	}
	memset(&link, 0, sizeof(link));
	rte_igb_dev_atomic_read_link_status(dev, &link);
	old = link;

	/* Now we check if a transition has happened */
	if (link_check) {
		hw->mac.ops.get_link_up_info(hw, &link.link_speed,
					  &link.link_duplex);
		link.link_status = 1;
	} else if (!link_check) {
		link.link_speed = 0;
		link.link_duplex = 0;
		link.link_status = 0;
	}
	rte_igb_dev_atomic_write_link_status(dev, &link);

	/* not changed */
	if (old.link_status == link.link_status)
		return -1;

	/* changed */
	return 0;
}

/*
 * igb_hw_control_acquire sets CTRL_EXT:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means
 * that the driver is loaded.
 */
static void
igb_hw_control_acquire(struct e1000_hw *hw)
{
	uint32_t ctrl_ext;

	/* Let firmware know the driver has taken over */
	ctrl_ext = E1000_READ_REG(hw, E1000_CTRL_EXT);
	E1000_WRITE_REG(hw, E1000_CTRL_EXT, ctrl_ext | E1000_CTRL_EXT_DRV_LOAD);
}

/*
 * igb_hw_control_release resets CTRL_EXT:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that the
 * driver is no longer loaded.
 */
static void
igb_hw_control_release(struct e1000_hw *hw)
{
	uint32_t ctrl_ext;

	/* Let firmware taken over control of h/w */
	ctrl_ext = E1000_READ_REG(hw, E1000_CTRL_EXT);
	E1000_WRITE_REG(hw, E1000_CTRL_EXT,
			ctrl_ext & ~E1000_CTRL_EXT_DRV_LOAD);
}

/*
 * Bit of a misnomer, what this really means is
 * to enable OS management of the system... aka
 * to disable special hardware management features.
 */
static void
igb_init_manageability(struct e1000_hw *hw)
{
	if (e1000_enable_mng_pass_thru(hw)) {
		uint32_t manc2h = E1000_READ_REG(hw, E1000_MANC2H);
		uint32_t manc = E1000_READ_REG(hw, E1000_MANC);

		/* disable hardware interception of ARP */
		manc &= ~(E1000_MANC_ARP_EN);

		/* enable receiving management packets to the host */
		manc |= E1000_MANC_EN_MNG2HOST;
		manc2h |= 1 << 5;  /* Mng Port 623 */
		manc2h |= 1 << 6;  /* Mng Port 664 */
		E1000_WRITE_REG(hw, E1000_MANC2H, manc2h);
		E1000_WRITE_REG(hw, E1000_MANC, manc);
	}
}

static void
igb_release_manageability(struct e1000_hw *hw)
{
	if (e1000_enable_mng_pass_thru(hw)) {
		uint32_t manc = E1000_READ_REG(hw, E1000_MANC);

		manc |= E1000_MANC_ARP_EN;
		manc &= ~E1000_MANC_EN_MNG2HOST;

		E1000_WRITE_REG(hw, E1000_MANC, manc);
	}
}

static void
eth_igb_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t rctl;

	rctl = E1000_READ_REG(hw, E1000_RCTL);
	rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);
}

static void
eth_igb_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t rctl;

	rctl = E1000_READ_REG(hw, E1000_RCTL);
	rctl &= (~E1000_RCTL_UPE);
	if (dev->data->all_multicast == 1)
		rctl |= E1000_RCTL_MPE;
	else
		rctl &= (~E1000_RCTL_MPE);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);
}

static void
eth_igb_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t rctl;

	rctl = E1000_READ_REG(hw, E1000_RCTL);
	rctl |= E1000_RCTL_MPE;
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);
}

static void
eth_igb_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t rctl;

	if (dev->data->promiscuous == 1)
		return; /* must remain in all_multicast mode */
	rctl = E1000_READ_REG(hw, E1000_RCTL);
	rctl &= (~E1000_RCTL_MPE);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);
}

static void
eth_igb_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_vfta * shadow_vfta =
		E1000_DEV_PRIVATE_TO_VFTA(dev->data->dev_private);
	uint32_t vfta;
	uint32_t vid_idx;
	uint32_t vid_bit;

	vid_idx = (uint32_t) ((vlan_id >> E1000_VFTA_ENTRY_SHIFT) &
			      E1000_VFTA_ENTRY_MASK);
	vid_bit = (uint32_t) (1 << (vlan_id & E1000_VFTA_ENTRY_BIT_SHIFT_MASK));
	vfta = E1000_READ_REG_ARRAY(hw, E1000_VFTA, vid_idx);
	if (on)
		vfta |= vid_bit;
	else
		vfta &= ~vid_bit;
	E1000_WRITE_REG_ARRAY(hw, E1000_VFTA, vid_idx, vfta);

	/* update local VFTA copy */
	shadow_vfta->vfta[vid_idx] = vfta;
}

static void
igb_vlan_hw_support_enable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_vfta * shadow_vfta =
		E1000_DEV_PRIVATE_TO_VFTA(dev->data->dev_private);
	uint32_t reg;
	int i;

	/* VLAN Mode Enable */
	reg = E1000_READ_REG(hw, E1000_CTRL);
	reg |= E1000_CTRL_VME;
	E1000_WRITE_REG(hw, E1000_CTRL, reg);

	/* Filter Table Enable */
	reg = E1000_READ_REG(hw, E1000_RCTL);
	reg &= ~E1000_RCTL_CFIEN;
	reg |= E1000_RCTL_VFE;
	E1000_WRITE_REG(hw, E1000_RCTL, reg);

	/* Update maximum frame size */
	reg = E1000_READ_REG(hw, E1000_RLPML);
	reg += VLAN_TAG_SIZE;
	E1000_WRITE_REG(hw, E1000_RLPML, reg);

	/* restore VFTA table */
	for (i = 0; i < E1000_VFTA_SIZE; i++)
		E1000_WRITE_REG_ARRAY(hw, E1000_VFTA, i, shadow_vfta->vfta[i]);
}

static void
igb_vlan_hw_support_disable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t reg;

	/* VLAN Mode disable */
	reg = E1000_READ_REG(hw, E1000_CTRL);
	reg &= ~E1000_CTRL_VME;
	E1000_WRITE_REG(hw, E1000_CTRL, reg);
}

static void
igb_intr_disable(struct e1000_hw *hw)
{
	E1000_WRITE_REG(hw, E1000_IMC, ~0);
	E1000_WRITE_FLUSH(hw);
}

/**
 * It enables the interrupt mask and then enable the interrupt.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
eth_igb_interrupt_setup(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	E1000_WRITE_REG(hw, E1000_IMS, E1000_ICR_LSC);
	E1000_WRITE_FLUSH(hw);
	rte_intr_enable(&(dev->pci_dev->intr_handle));

	return 0;
}

/*
 * It reads ICR and gets interrupt causes, check it and set a bit flag
 * to update link status.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
eth_igb_interrupt_get_status(struct rte_eth_dev *dev)
{
	uint32_t icr;
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_interrupt *intr =
		E1000_DEV_PRIVATE_TO_INTR(dev->data->dev_private);

	/* read-on-clear nic registers here */
	icr = E1000_READ_REG(hw, E1000_ICR);
	if (icr & E1000_ICR_LSC) {
		intr->flags |= E1000_FLAG_NEED_LINK_UPDATE;
	}

	return 0;
}

/*
 * It executes link_update after knowing an interrupt is prsent.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
eth_igb_interrupt_action(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_interrupt *intr =
		E1000_DEV_PRIVATE_TO_INTR(dev->data->dev_private);
	uint32_t tctl, rctl;
	struct rte_eth_link link;
	int ret;

	if (!(intr->flags & E1000_FLAG_NEED_LINK_UPDATE))
		return -1;

	intr->flags &= ~E1000_FLAG_NEED_LINK_UPDATE;
	rte_intr_enable(&(dev->pci_dev->intr_handle));

	/* set get_link_status to check register later */
	hw->mac.get_link_status = 1;
	ret = eth_igb_link_update(dev, 0);

	/* check if link has changed */
	if (ret < 0)
		return 0;

	memset(&link, 0, sizeof(link));
	rte_igb_dev_atomic_read_link_status(dev, &link);
	if (link.link_status) {
		PMD_INIT_LOG(INFO,
			" Port %d: Link Up - speed %u Mbps - %s\n",
			dev->data->port_id, (unsigned)link.link_speed,
			link.link_duplex == ETH_LINK_FULL_DUPLEX ?
				"full-duplex" : "half-duplex");
	} else {
		PMD_INIT_LOG(INFO, " Port %d: Link Down\n",
					dev->data->port_id);
	}
	PMD_INIT_LOG(INFO, "PCI Address: %04d:%02d:%02d:%d",
				dev->pci_dev->addr.domain,
				dev->pci_dev->addr.bus,
				dev->pci_dev->addr.devid,
				dev->pci_dev->addr.function);
	tctl = E1000_READ_REG(hw, E1000_TCTL);
	rctl = E1000_READ_REG(hw, E1000_RCTL);
	if (link.link_status) {
		/* enable Tx/Rx */
		tctl |= E1000_TCTL_EN;
		rctl |= E1000_RCTL_EN;
	} else {
		/* disable Tx/Rx */
		tctl &= ~E1000_TCTL_EN;
		rctl &= ~E1000_RCTL_EN;
	}
	E1000_WRITE_REG(hw, E1000_TCTL, tctl);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);
	E1000_WRITE_FLUSH(hw);

	return 0;
}

/**
 * Interrupt handler which shall be registered at first.
 *
 * @param handle
 *  Pointer to interrupt handle.
 * @param param
 *  The address of parameter (struct rte_eth_dev *) regsitered before.
 *
 * @return
 *  void
 */
static void
eth_igb_interrupt_handler(struct rte_intr_handle *handle, void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	eth_igb_interrupt_get_status(dev);
	eth_igb_interrupt_action(dev);
	_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC);
}

static int
eth_igb_led_on(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	return (e1000_led_on(hw) == E1000_SUCCESS ? 0 : -ENOTSUP);
}

static int
eth_igb_led_off(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	return (e1000_led_off(hw) == E1000_SUCCESS ? 0 : -ENOTSUP);
}

static int
eth_igb_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct e1000_hw *hw;
	int err;
	enum e1000_fc_mode rte_fcmode_2_e1000_fcmode[] = {
		e1000_fc_none,
		e1000_fc_rx_pause,
		e1000_fc_tx_pause,
		e1000_fc_full
	};
	uint32_t rx_buf_size;
	uint32_t max_high_water;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	rx_buf_size = igb_get_rx_buffer_size(hw);
	PMD_INIT_LOG(DEBUG, "Rx packet buffer size = 0x%x \n", rx_buf_size);

	/* At least reserve one Ethernet frame for watermark */
	max_high_water = rx_buf_size - ETHER_MAX_LEN;
	if ((fc_conf->high_water > max_high_water) ||
		(fc_conf->high_water < fc_conf->low_water)) {
		PMD_INIT_LOG(ERR, "e1000 incorrect high/low water value \n");
		PMD_INIT_LOG(ERR, "high water must <=  0x%x \n", max_high_water);
		return (-EINVAL);
	}

	hw->fc.requested_mode = rte_fcmode_2_e1000_fcmode[fc_conf->mode];
	hw->fc.pause_time     = fc_conf->pause_time;
	hw->fc.high_water     = fc_conf->high_water;
	hw->fc.low_water      = fc_conf->low_water;
	hw->fc.send_xon	      = fc_conf->send_xon;

	err = e1000_setup_link_generic(hw);
	if (err == E1000_SUCCESS) {
		return 0;
	}

	PMD_INIT_LOG(ERR, "e1000_setup_link_generic = 0x%x \n", err);
	return (-EIO);
}

static void
eth_igb_rar_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
	        uint32_t index, __rte_unused uint32_t pool)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	e1000_rar_set(hw, mac_addr->addr_bytes, index);
}

static void
eth_igb_rar_clear(struct rte_eth_dev *dev, uint32_t index)
{
	uint8_t addr[ETHER_ADDR_LEN];
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	memset(addr, 0, sizeof(addr));

	e1000_rar_set(hw, addr, index);
}
