/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
#include <rte_eal.h>
#include <rte_atomic.h>
#include <rte_malloc.h>
#include <rte_dev.h>

#include "e1000_logs.h"
#include "base/e1000_api.h"
#include "e1000_ethdev.h"

#define EM_EIAC			0x000DC

#define PMD_ROUNDUP(x,y)	(((x) + (y) - 1)/(y) * (y))


static int eth_em_configure(struct rte_eth_dev *dev);
static int eth_em_start(struct rte_eth_dev *dev);
static void eth_em_stop(struct rte_eth_dev *dev);
static void eth_em_close(struct rte_eth_dev *dev);
static void eth_em_promiscuous_enable(struct rte_eth_dev *dev);
static void eth_em_promiscuous_disable(struct rte_eth_dev *dev);
static void eth_em_allmulticast_enable(struct rte_eth_dev *dev);
static void eth_em_allmulticast_disable(struct rte_eth_dev *dev);
static int eth_em_link_update(struct rte_eth_dev *dev,
				int wait_to_complete);
static void eth_em_stats_get(struct rte_eth_dev *dev,
				struct rte_eth_stats *rte_stats);
static void eth_em_stats_reset(struct rte_eth_dev *dev);
static void eth_em_infos_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);
static int eth_em_flow_ctrl_get(struct rte_eth_dev *dev,
				struct rte_eth_fc_conf *fc_conf);
static int eth_em_flow_ctrl_set(struct rte_eth_dev *dev,
				struct rte_eth_fc_conf *fc_conf);
static int eth_em_interrupt_setup(struct rte_eth_dev *dev);
static int eth_em_rxq_interrupt_setup(struct rte_eth_dev *dev);
static int eth_em_interrupt_get_status(struct rte_eth_dev *dev);
static int eth_em_interrupt_action(struct rte_eth_dev *dev);
static void eth_em_interrupt_handler(struct rte_intr_handle *handle,
							void *param);

static int em_hw_init(struct e1000_hw *hw);
static int em_hardware_init(struct e1000_hw *hw);
static void em_hw_control_acquire(struct e1000_hw *hw);
static void em_hw_control_release(struct e1000_hw *hw);
static void em_init_manageability(struct e1000_hw *hw);
static void em_release_manageability(struct e1000_hw *hw);

static int eth_em_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);

static int eth_em_vlan_filter_set(struct rte_eth_dev *dev,
		uint16_t vlan_id, int on);
static void eth_em_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static void em_vlan_hw_filter_enable(struct rte_eth_dev *dev);
static void em_vlan_hw_filter_disable(struct rte_eth_dev *dev);
static void em_vlan_hw_strip_enable(struct rte_eth_dev *dev);
static void em_vlan_hw_strip_disable(struct rte_eth_dev *dev);

/*
static void eth_em_vlan_filter_set(struct rte_eth_dev *dev,
					uint16_t vlan_id, int on);
*/

static int eth_em_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id);
static int eth_em_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id);
static void em_lsc_intr_disable(struct e1000_hw *hw);
static void em_rxq_intr_enable(struct e1000_hw *hw);
static void em_rxq_intr_disable(struct e1000_hw *hw);

static int eth_em_led_on(struct rte_eth_dev *dev);
static int eth_em_led_off(struct rte_eth_dev *dev);

static int em_get_rx_buffer_size(struct e1000_hw *hw);
static void eth_em_rar_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
		uint32_t index, uint32_t pool);
static void eth_em_rar_clear(struct rte_eth_dev *dev, uint32_t index);

static int eth_em_set_mc_addr_list(struct rte_eth_dev *dev,
				   struct ether_addr *mc_addr_set,
				   uint32_t nb_mc_addr);

#define EM_FC_PAUSE_TIME 0x0680
#define EM_LINK_UPDATE_CHECK_TIMEOUT  90  /* 9s */
#define EM_LINK_UPDATE_CHECK_INTERVAL 100 /* ms */

static enum e1000_fc_mode em_fc_setting = e1000_fc_full;

/*
 * The set of PCI devices this driver supports
 */
static const struct rte_pci_id pci_id_em_map[] = {
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82540EM) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82545EM_COPPER) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82545EM_FIBER) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82546EB_COPPER) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82546EB_FIBER) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82546EB_QUAD_COPPER) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82571EB_COPPER) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82571EB_FIBER) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82571EB_SERDES) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82571EB_SERDES_DUAL) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82571EB_SERDES_QUAD) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82571EB_QUAD_COPPER) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82571PT_QUAD_COPPER) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82571EB_QUAD_FIBER) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82571EB_QUAD_COPPER_LP) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82572EI_COPPER) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82572EI_FIBER) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82572EI_SERDES) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82572EI) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82573L) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82574L) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82574LA) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_82583V) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_PCH_LPT_I217_LM) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_PCH_LPT_I217_V) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_PCH_LPTLP_I218_LM) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_PCH_LPTLP_I218_V) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_PCH_I218_LM2) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_PCH_I218_V2) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_PCH_I218_LM3) },
	{ RTE_PCI_DEVICE(E1000_INTEL_VENDOR_ID, E1000_DEV_ID_PCH_I218_V3) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops eth_em_ops = {
	.dev_configure        = eth_em_configure,
	.dev_start            = eth_em_start,
	.dev_stop             = eth_em_stop,
	.dev_close            = eth_em_close,
	.promiscuous_enable   = eth_em_promiscuous_enable,
	.promiscuous_disable  = eth_em_promiscuous_disable,
	.allmulticast_enable  = eth_em_allmulticast_enable,
	.allmulticast_disable = eth_em_allmulticast_disable,
	.link_update          = eth_em_link_update,
	.stats_get            = eth_em_stats_get,
	.stats_reset          = eth_em_stats_reset,
	.dev_infos_get        = eth_em_infos_get,
	.mtu_set              = eth_em_mtu_set,
	.vlan_filter_set      = eth_em_vlan_filter_set,
	.vlan_offload_set     = eth_em_vlan_offload_set,
	.rx_queue_setup       = eth_em_rx_queue_setup,
	.rx_queue_release     = eth_em_rx_queue_release,
	.rx_queue_count       = eth_em_rx_queue_count,
	.rx_descriptor_done   = eth_em_rx_descriptor_done,
	.tx_queue_setup       = eth_em_tx_queue_setup,
	.tx_queue_release     = eth_em_tx_queue_release,
	.rx_queue_intr_enable = eth_em_rx_queue_intr_enable,
	.rx_queue_intr_disable = eth_em_rx_queue_intr_disable,
	.dev_led_on           = eth_em_led_on,
	.dev_led_off          = eth_em_led_off,
	.flow_ctrl_get        = eth_em_flow_ctrl_get,
	.flow_ctrl_set        = eth_em_flow_ctrl_set,
	.mac_addr_add         = eth_em_rar_set,
	.mac_addr_remove      = eth_em_rar_clear,
	.set_mc_addr_list     = eth_em_set_mc_addr_list,
	.rxq_info_get         = em_rxq_info_get,
	.txq_info_get         = em_txq_info_get,
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
rte_em_dev_atomic_read_link_status(struct rte_eth_dev *dev,
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
rte_em_dev_atomic_write_link_status(struct rte_eth_dev *dev,
				struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &(dev->data->dev_link);
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

/**
 *  eth_em_dev_is_ich8 - Check for ICH8 device
 *  @hw: pointer to the HW structure
 *
 *  return TRUE for ICH8, otherwise FALSE
 **/
static bool
eth_em_dev_is_ich8(struct e1000_hw *hw)
{
	DEBUGFUNC("eth_em_dev_is_ich8");

	switch (hw->device_id) {
	case E1000_DEV_ID_PCH_LPT_I217_LM:
	case E1000_DEV_ID_PCH_LPT_I217_V:
	case E1000_DEV_ID_PCH_LPTLP_I218_LM:
	case E1000_DEV_ID_PCH_LPTLP_I218_V:
	case E1000_DEV_ID_PCH_I218_V2:
	case E1000_DEV_ID_PCH_I218_LM2:
	case E1000_DEV_ID_PCH_I218_V3:
	case E1000_DEV_ID_PCH_I218_LM3:
		return 1;
	default:
		return 0;
	}
}

static int
eth_em_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct e1000_adapter *adapter =
		E1000_DEV_PRIVATE(eth_dev->data->dev_private);
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct e1000_vfta * shadow_vfta =
		E1000_DEV_PRIVATE_TO_VFTA(eth_dev->data->dev_private);

	pci_dev = eth_dev->pci_dev;

	eth_dev->dev_ops = &eth_em_ops;
	eth_dev->rx_pkt_burst = (eth_rx_burst_t)&eth_em_recv_pkts;
	eth_dev->tx_pkt_burst = (eth_tx_burst_t)&eth_em_xmit_pkts;

	/* for secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY){
		if (eth_dev->data->scattered_rx)
			eth_dev->rx_pkt_burst =
				(eth_rx_burst_t)&eth_em_recv_scattered_pkts;
		return 0;
	}

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;
	hw->device_id = pci_dev->id.device_id;
	adapter->stopped = 0;

	/* For ICH8 support we'll need to map the flash memory BAR */
	if (eth_em_dev_is_ich8(hw))
		hw->flash_address = (void *)pci_dev->mem_resource[1].addr;

	if (e1000_setup_init_funcs(hw, TRUE) != E1000_SUCCESS ||
			em_hw_init(hw) != 0) {
		PMD_INIT_LOG(ERR, "port_id %d vendorID=0x%x deviceID=0x%x: "
			"failed to init HW",
			eth_dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);
		return -ENODEV;
	}

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("e1000", ETHER_ADDR_LEN *
			hw->mac.rar_entry_count, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate %d bytes needed to "
			"store MAC addresses",
			ETHER_ADDR_LEN * hw->mac.rar_entry_count);
		return -ENOMEM;
	}

	/* Copy the permanent MAC address */
	ether_addr_copy((struct ether_addr *) hw->mac.addr,
		eth_dev->data->mac_addrs);

	/* initialize the vfta */
	memset(shadow_vfta, 0, sizeof(*shadow_vfta));

	PMD_INIT_LOG(DEBUG, "port_id %d vendorID=0x%x deviceID=0x%x",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id);

	rte_intr_callback_register(&(pci_dev->intr_handle),
		eth_em_interrupt_handler, (void *)eth_dev);

	return 0;
}

static int
eth_em_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct e1000_adapter *adapter =
		E1000_DEV_PRIVATE(eth_dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	pci_dev = eth_dev->pci_dev;

	if (adapter->stopped == 0)
		eth_em_close(eth_dev);

	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;

	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;

	/* disable uio intr before callback unregister */
	rte_intr_disable(&(pci_dev->intr_handle));
	rte_intr_callback_unregister(&(pci_dev->intr_handle),
		eth_em_interrupt_handler, (void *)eth_dev);

	return 0;
}

static struct eth_driver rte_em_pmd = {
	.pci_drv = {
		.name = "rte_em_pmd",
		.id_table = pci_id_em_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC |
			RTE_PCI_DRV_DETACHABLE,
	},
	.eth_dev_init = eth_em_dev_init,
	.eth_dev_uninit = eth_em_dev_uninit,
	.dev_private_size = sizeof(struct e1000_adapter),
};

static int
rte_em_pmd_init(const char *name __rte_unused, const char *params __rte_unused)
{
	rte_eth_driver_register(&rte_em_pmd);
	return 0;
}

static int
em_hw_init(struct e1000_hw *hw)
{
	int diag;

	diag = hw->mac.ops.init_params(hw);
	if (diag != 0) {
		PMD_INIT_LOG(ERR, "MAC Initialization Error");
		return diag;
	}
	diag = hw->nvm.ops.init_params(hw);
	if (diag != 0) {
		PMD_INIT_LOG(ERR, "NVM Initialization Error");
		return diag;
	}
	diag = hw->phy.ops.init_params(hw);
	if (diag != 0) {
		PMD_INIT_LOG(ERR, "PHY Initialization Error");
		return diag;
	}
	(void) e1000_get_bus_info(hw);

	hw->mac.autoneg = 1;
	hw->phy.autoneg_wait_to_complete = 0;
	hw->phy.autoneg_advertised = E1000_ALL_SPEED_DUPLEX;

	e1000_init_script_state_82541(hw, TRUE);
	e1000_set_tbi_compatibility_82543(hw, TRUE);

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
		diag = e1000_validate_nvm_checksum(hw);
		if (diag < 0) {
			PMD_INIT_LOG(ERR, "EEPROM checksum invalid");
			goto error;
		}
	}

	/* Read the permanent MAC address out of the EEPROM */
	diag = e1000_read_mac_addr(hw);
	if (diag != 0) {
		PMD_INIT_LOG(ERR, "EEPROM error while reading MAC address");
		goto error;
	}

	/* Now initialize the hardware */
	diag = em_hardware_init(hw);
	if (diag != 0) {
		PMD_INIT_LOG(ERR, "Hardware initialization failed");
		goto error;
	}

	hw->mac.get_link_status = 1;

	/* Indicate SOL/IDER usage */
	diag = e1000_check_reset_block(hw);
	if (diag < 0) {
		PMD_INIT_LOG(ERR, "PHY reset is blocked due to "
			"SOL/IDER session");
	}
	return 0;

error:
	em_hw_control_release(hw);
	return diag;
}

static int
eth_em_configure(struct rte_eth_dev *dev)
{
	struct e1000_interrupt *intr =
		E1000_DEV_PRIVATE_TO_INTR(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();
	intr->flags |= E1000_FLAG_NEED_LINK_UPDATE;
	PMD_INIT_FUNC_TRACE();

	return 0;
}

static void
em_set_pba(struct e1000_hw *hw)
{
	uint32_t pba;

	/*
	 * Packet Buffer Allocation (PBA)
	 * Writing PBA sets the receive portion of the buffer
	 * the remainder is used for the transmit buffer.
	 * Devices before the 82547 had a Packet Buffer of 64K.
	 * After the 82547 the buffer was reduced to 40K.
	 */
	switch (hw->mac.type) {
		case e1000_82547:
		case e1000_82547_rev_2:
		/* 82547: Total Packet Buffer is 40K */
			pba = E1000_PBA_22K; /* 22K for Rx, 18K for Tx */
			break;
		case e1000_82571:
		case e1000_82572:
		case e1000_80003es2lan:
			pba = E1000_PBA_32K; /* 32K for Rx, 16K for Tx */
			break;
		case e1000_82573: /* 82573: Total Packet Buffer is 32K */
			pba = E1000_PBA_12K; /* 12K for Rx, 20K for Tx */
			break;
		case e1000_82574:
		case e1000_82583:
			pba = E1000_PBA_20K; /* 20K for Rx, 20K for Tx */
			break;
		case e1000_ich8lan:
			pba = E1000_PBA_8K;
			break;
		case e1000_ich9lan:
		case e1000_ich10lan:
			pba = E1000_PBA_10K;
			break;
		case e1000_pchlan:
		case e1000_pch2lan:
		case e1000_pch_lpt:
			pba = E1000_PBA_26K;
			break;
		default:
			pba = E1000_PBA_40K; /* 40K for Rx, 24K for Tx */
	}

	E1000_WRITE_REG(hw, E1000_PBA, pba);
}

static int
eth_em_start(struct rte_eth_dev *dev)
{
	struct e1000_adapter *adapter =
		E1000_DEV_PRIVATE(dev->data->dev_private);
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_intr_handle *intr_handle = &dev->pci_dev->intr_handle;
	int ret, mask;
	uint32_t intr_vector = 0;
	uint32_t *speeds;
	int num_speeds;
	bool autoneg;

	PMD_INIT_FUNC_TRACE();

	eth_em_stop(dev);

	e1000_power_up_phy(hw);

	/* Set default PBA value */
	em_set_pba(hw);

	/* Put the address into the Receive Address Array */
	e1000_rar_set(hw, hw->mac.addr, 0);

	/*
	 * With the 82571 adapter, RAR[0] may be overwritten
	 * when the other port is reset, we make a duplicate
	 * in RAR[14] for that eventuality, this assures
	 * the interface continues to function.
	 */
	if (hw->mac.type == e1000_82571) {
		e1000_set_laa_state_82571(hw, TRUE);
		e1000_rar_set(hw, hw->mac.addr, E1000_RAR_ENTRIES - 1);
	}

	/* Initialize the hardware */
	if (em_hardware_init(hw)) {
		PMD_INIT_LOG(ERR, "Unable to initialize the hardware");
		return -EIO;
	}

	E1000_WRITE_REG(hw, E1000_VET, ETHER_TYPE_VLAN);

	/* Configure for OS presence */
	em_init_manageability(hw);

	if (dev->data->dev_conf.intr_conf.rxq != 0) {
		intr_vector = dev->data->nb_rx_queues;
		if (rte_intr_efd_enable(intr_handle, intr_vector))
			return -1;
	}

	if (rte_intr_dp_is_en(intr_handle)) {
		intr_handle->intr_vec =
			rte_zmalloc("intr_vec",
					dev->data->nb_rx_queues * sizeof(int), 0);
		if (intr_handle->intr_vec == NULL) {
			PMD_INIT_LOG(ERR, "Failed to allocate %d rx_queues"
						" intr_vec\n", dev->data->nb_rx_queues);
			return -ENOMEM;
		}

		/* enable rx interrupt */
		em_rxq_intr_enable(hw);
	}

	eth_em_tx_init(dev);

	ret = eth_em_rx_init(dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Unable to initialize RX hardware");
		em_dev_clear_queues(dev);
		return ret;
	}

	e1000_clear_hw_cntrs_base_generic(hw);

	mask = ETH_VLAN_STRIP_MASK | ETH_VLAN_FILTER_MASK | \
			ETH_VLAN_EXTEND_MASK;
	eth_em_vlan_offload_set(dev, mask);

	/* Set Interrupt Throttling Rate to maximum allowed value. */
	E1000_WRITE_REG(hw, E1000_ITR, UINT16_MAX);

	/* Setup link speed and duplex */
	speeds = &dev->data->dev_conf.link_speeds;
	if (*speeds == ETH_LINK_SPEED_AUTONEG) {
		hw->phy.autoneg_advertised = E1000_ALL_SPEED_DUPLEX;
	} else {
		num_speeds = 0;
		autoneg = (*speeds & ETH_LINK_SPEED_FIXED) == 0;

		/* Reset */
		hw->phy.autoneg_advertised = 0;

		if (*speeds & ~(ETH_LINK_SPEED_10M_HD | ETH_LINK_SPEED_10M |
				ETH_LINK_SPEED_100M_HD | ETH_LINK_SPEED_100M |
				ETH_LINK_SPEED_1G | ETH_LINK_SPEED_FIXED)) {
			num_speeds = -1;
			goto error_invalid_config;
		}
		if (*speeds & ETH_LINK_SPEED_10M_HD) {
			hw->phy.autoneg_advertised |= ADVERTISE_10_HALF;
			num_speeds++;
		}
		if (*speeds & ETH_LINK_SPEED_10M) {
			hw->phy.autoneg_advertised |= ADVERTISE_10_FULL;
			num_speeds++;
		}
		if (*speeds & ETH_LINK_SPEED_100M_HD) {
			hw->phy.autoneg_advertised |= ADVERTISE_100_HALF;
			num_speeds++;
		}
		if (*speeds & ETH_LINK_SPEED_100M) {
			hw->phy.autoneg_advertised |= ADVERTISE_100_FULL;
			num_speeds++;
		}
		if (*speeds & ETH_LINK_SPEED_1G) {
			hw->phy.autoneg_advertised |= ADVERTISE_1000_FULL;
			num_speeds++;
		}
		if (num_speeds == 0 || (!autoneg && (num_speeds > 1)))
			goto error_invalid_config;
	}

	e1000_setup_link(hw);

	if (rte_intr_allow_others(intr_handle)) {
		/* check if lsc interrupt is enabled */
		if (dev->data->dev_conf.intr_conf.lsc != 0) {
			ret = eth_em_interrupt_setup(dev);
			if (ret) {
				PMD_INIT_LOG(ERR, "Unable to setup interrupts");
				em_dev_clear_queues(dev);
				return ret;
			}
		}
	} else {
		rte_intr_callback_unregister(intr_handle,
						eth_em_interrupt_handler,
						(void *)dev);
		if (dev->data->dev_conf.intr_conf.lsc != 0)
			PMD_INIT_LOG(INFO, "lsc won't enable because of"
				     " no intr multiplex\n");
	}
	/* check if rxq interrupt is enabled */
	if (dev->data->dev_conf.intr_conf.rxq != 0)
		eth_em_rxq_interrupt_setup(dev);

	rte_intr_enable(intr_handle);

	adapter->stopped = 0;

	PMD_INIT_LOG(DEBUG, "<<");

	return 0;

error_invalid_config:
	PMD_INIT_LOG(ERR, "Invalid advertised speeds (%u) for port %u",
		     dev->data->dev_conf.link_speeds, dev->data->port_id);
	em_dev_clear_queues(dev);
	return -EINVAL;
}

/*********************************************************************
 *
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC.
 *
 **********************************************************************/
static void
eth_em_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_link link;
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_intr_handle *intr_handle = &dev->pci_dev->intr_handle;

	em_rxq_intr_disable(hw);
	em_lsc_intr_disable(hw);

	e1000_reset_hw(hw);
	if (hw->mac.type >= e1000_82544)
		E1000_WRITE_REG(hw, E1000_WUC, 0);

	/* Power down the phy. Needed to make the link go down */
	e1000_power_down_phy(hw);

	em_dev_clear_queues(dev);

	/* clear the recorded link status */
	memset(&link, 0, sizeof(link));
	rte_em_dev_atomic_write_link_status(dev, &link);

	if (!rte_intr_allow_others(intr_handle))
		/* resume to the default handler */
		rte_intr_callback_register(intr_handle,
					   eth_em_interrupt_handler,
					   (void *)dev);

	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	if (intr_handle->intr_vec != NULL) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}
}

static void
eth_em_close(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_adapter *adapter =
		E1000_DEV_PRIVATE(dev->data->dev_private);

	eth_em_stop(dev);
	adapter->stopped = 1;
	em_dev_free_queues(dev);
	e1000_phy_hw_reset(hw);
	em_release_manageability(hw);
	em_hw_control_release(hw);
}

static int
em_get_rx_buffer_size(struct e1000_hw *hw)
{
	uint32_t rx_buf_size;

	rx_buf_size = ((E1000_READ_REG(hw, E1000_PBA) & UINT16_MAX) << 10);
	return rx_buf_size;
}

/*********************************************************************
 *
 *  Initialize the hardware
 *
 **********************************************************************/
static int
em_hardware_init(struct e1000_hw *hw)
{
	uint32_t rx_buf_size;
	int diag;

	/* Issue a global reset */
	e1000_reset_hw(hw);

	/* Let the firmware know the OS is in control */
	em_hw_control_acquire(hw);

	/*
	 * These parameters control the automatic generation (Tx) and
	 * response (Rx) to Ethernet PAUSE frames.
	 * - High water mark should allow for at least two standard size (1518)
	 *   frames to be received after sending an XOFF.
	 * - Low water mark works best when it is very near the high water mark.
	 *   This allows the receiver to restart by sending XON when it has
	 *   drained a bit. Here we use an arbitrary value of 1500 which will
	 *   restart after one full frame is pulled from the buffer. There
	 *   could be several smaller frames in the buffer and if so they will
	 *   not trigger the XON until their total number reduces the buffer
	 *   by 1500.
	 * - The pause time is fairly large at 1000 x 512ns = 512 usec.
	 */
	rx_buf_size = em_get_rx_buffer_size(hw);

	hw->fc.high_water = rx_buf_size - PMD_ROUNDUP(ETHER_MAX_LEN * 2, 1024);
	hw->fc.low_water = hw->fc.high_water - 1500;

	if (hw->mac.type == e1000_80003es2lan)
		hw->fc.pause_time = UINT16_MAX;
	else
		hw->fc.pause_time = EM_FC_PAUSE_TIME;

	hw->fc.send_xon = 1;

	/* Set Flow control, use the tunable location if sane */
	if (em_fc_setting <= e1000_fc_full)
		hw->fc.requested_mode = em_fc_setting;
	else
		hw->fc.requested_mode = e1000_fc_none;

	/* Workaround: no TX flow ctrl for PCH */
	if (hw->mac.type == e1000_pchlan)
		hw->fc.requested_mode = e1000_fc_rx_pause;

	/* Override - settings for PCH2LAN, ya its magic :) */
	if (hw->mac.type == e1000_pch2lan) {
		hw->fc.high_water = 0x5C20;
		hw->fc.low_water = 0x5048;
		hw->fc.pause_time = 0x0650;
		hw->fc.refresh_time = 0x0400;
	} else if (hw->mac.type == e1000_pch_lpt) {
		hw->fc.requested_mode = e1000_fc_full;
	}

	diag = e1000_init_hw(hw);
	if (diag < 0)
		return diag;
	e1000_check_for_link(hw);
	return 0;
}

/* This function is based on em_update_stats_counters() in e1000/if_em.c */
static void
eth_em_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *rte_stats)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_hw_stats *stats =
			E1000_DEV_PRIVATE_TO_STATS(dev->data->dev_private);
	int pause_frames;

	if(hw->phy.media_type == e1000_media_type_copper ||
			(E1000_READ_REG(hw, E1000_STATUS) & E1000_STATUS_LU)) {
		stats->symerrs += E1000_READ_REG(hw,E1000_SYMERRS);
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
	 * For watchdog management we need to know if we have been
	 * paused during the last interval, so capture that here.
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

	/*
	 * For the 64-bit byte counters the low dword must be read first.
	 * Both registers clear on the read of the high dword.
	 */

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

	if (hw->mac.type >= e1000_82571) {
		stats->iac += E1000_READ_REG(hw, E1000_IAC);
		stats->icrxptc += E1000_READ_REG(hw, E1000_ICRXPTC);
		stats->icrxatc += E1000_READ_REG(hw, E1000_ICRXATC);
		stats->ictxptc += E1000_READ_REG(hw, E1000_ICTXPTC);
		stats->ictxatc += E1000_READ_REG(hw, E1000_ICTXATC);
		stats->ictxqec += E1000_READ_REG(hw, E1000_ICTXQEC);
		stats->ictxqmtc += E1000_READ_REG(hw, E1000_ICTXQMTC);
		stats->icrxdmtc += E1000_READ_REG(hw, E1000_ICRXDMTC);
		stats->icrxoc += E1000_READ_REG(hw, E1000_ICRXOC);
	}

	if (hw->mac.type >= e1000_82543) {
		stats->algnerrc += E1000_READ_REG(hw, E1000_ALGNERRC);
		stats->rxerrc += E1000_READ_REG(hw, E1000_RXERRC);
		stats->tncrs += E1000_READ_REG(hw, E1000_TNCRS);
		stats->cexterr += E1000_READ_REG(hw, E1000_CEXTERR);
		stats->tsctc += E1000_READ_REG(hw, E1000_TSCTC);
		stats->tsctfc += E1000_READ_REG(hw, E1000_TSCTFC);
	}

	if (rte_stats == NULL)
		return;

	/* Rx Errors */
	rte_stats->imissed = stats->mpc;
	rte_stats->ierrors = stats->crcerrs +
	                     stats->rlec + stats->ruc + stats->roc +
	                     stats->rxerrc + stats->algnerrc + stats->cexterr;

	/* Tx Errors */
	rte_stats->oerrors = stats->ecol + stats->latecol;

	rte_stats->ipackets = stats->gprc;
	rte_stats->opackets = stats->gptc;
	rte_stats->ibytes   = stats->gorc;
	rte_stats->obytes   = stats->gotc;
}

static void
eth_em_stats_reset(struct rte_eth_dev *dev)
{
	struct e1000_hw_stats *hw_stats =
			E1000_DEV_PRIVATE_TO_STATS(dev->data->dev_private);

	/* HW registers are cleared on read */
	eth_em_stats_get(dev, NULL);

	/* Reset software totals */
	memset(hw_stats, 0, sizeof(*hw_stats));
}

static int
eth_em_rx_queue_intr_enable(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	em_rxq_intr_enable(hw);
	rte_intr_enable(&dev->pci_dev->intr_handle);

	return 0;
}

static int
eth_em_rx_queue_intr_disable(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	em_rxq_intr_disable(hw);

	return 0;
}

static uint32_t
em_get_max_pktlen(const struct e1000_hw *hw)
{
	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
	case e1000_ich9lan:
	case e1000_ich10lan:
	case e1000_pch2lan:
	case e1000_pch_lpt:
	case e1000_82574:
	case e1000_80003es2lan: /* 9K Jumbo Frame size */
	case e1000_82583:
		return 0x2412;
	case e1000_pchlan:
		return 0x1000;
	/* Adapters that do not support jumbo frames */
	case e1000_ich8lan:
		return ETHER_MAX_LEN;
	default:
		return MAX_JUMBO_FRAME_SIZE;
	}
}

static void
eth_em_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	dev_info->min_rx_bufsize = 256; /* See BSIZE field of RCTL register. */
	dev_info->max_rx_pktlen = em_get_max_pktlen(hw);
	dev_info->max_mac_addrs = hw->mac.rar_entry_count;

	/*
	 * Starting with 631xESB hw supports 2 TX/RX queues per port.
	 * Unfortunatelly, all these nics have just one TX context.
	 * So we have few choises for TX:
	 * - Use just one TX queue.
	 * - Allow cksum offload only for one TX queue.
	 * - Don't allow TX cksum offload at all.
	 * For now, option #1 was chosen.
	 * To use second RX queue we have to use extended RX descriptor
	 * (Multiple Receive Queues are mutually exclusive with UDP
	 * fragmentation and are not supported when a legacy receive
	 * descriptor format is used).
	 * Which means separate RX routinies - as legacy nics (82540, 82545)
	 * don't support extended RXD.
	 * To avoid it we support just one RX queue for now (no RSS).
	 */

	dev_info->max_rx_queues = 1;
	dev_info->max_tx_queues = 1;

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = E1000_MAX_RING_DESC,
		.nb_min = E1000_MIN_RING_DESC,
		.nb_align = EM_RXD_ALIGN,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = E1000_MAX_RING_DESC,
		.nb_min = E1000_MIN_RING_DESC,
		.nb_align = EM_TXD_ALIGN,
	};

	dev_info->speed_capa = ETH_LINK_SPEED_10M_HD | ETH_LINK_SPEED_10M |
			ETH_LINK_SPEED_100M_HD | ETH_LINK_SPEED_100M |
			ETH_LINK_SPEED_1G;
}

/* return 0 means link status changed, -1 means not changed */
static int
eth_em_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_link link, old;
	int link_check, count;

	link_check = 0;
	hw->mac.get_link_status = 1;

	/* possible wait-to-complete in up to 9 seconds */
	for (count = 0; count < EM_LINK_UPDATE_CHECK_TIMEOUT; count ++) {
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
			break;
		}
		if (link_check || wait_to_complete == 0)
			break;
		rte_delay_ms(EM_LINK_UPDATE_CHECK_INTERVAL);
	}
	memset(&link, 0, sizeof(link));
	rte_em_dev_atomic_read_link_status(dev, &link);
	old = link;

	/* Now we check if a transition has happened */
	if (link_check && (link.link_status == ETH_LINK_DOWN)) {
		uint16_t duplex, speed;
		hw->mac.ops.get_link_up_info(hw, &speed, &duplex);
		link.link_duplex = (duplex == FULL_DUPLEX) ?
				ETH_LINK_FULL_DUPLEX :
				ETH_LINK_HALF_DUPLEX;
		link.link_speed = speed;
		link.link_status = ETH_LINK_UP;
		link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				ETH_LINK_SPEED_FIXED);
	} else if (!link_check && (link.link_status == ETH_LINK_UP)) {
		link.link_speed = 0;
		link.link_duplex = ETH_LINK_HALF_DUPLEX;
		link.link_status = ETH_LINK_DOWN;
		link.link_autoneg = ETH_LINK_SPEED_FIXED;
	}
	rte_em_dev_atomic_write_link_status(dev, &link);

	/* not changed */
	if (old.link_status == link.link_status)
		return -1;

	/* changed */
	return 0;
}

/*
 * em_hw_control_acquire sets {CTRL_EXT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means
 * that the driver is loaded. For AMT version type f/w
 * this means that the network i/f is open.
 */
static void
em_hw_control_acquire(struct e1000_hw *hw)
{
	uint32_t ctrl_ext, swsm;

	/* Let firmware know the driver has taken over */
	if (hw->mac.type == e1000_82573) {
		swsm = E1000_READ_REG(hw, E1000_SWSM);
		E1000_WRITE_REG(hw, E1000_SWSM, swsm | E1000_SWSM_DRV_LOAD);

	} else {
		ctrl_ext = E1000_READ_REG(hw, E1000_CTRL_EXT);
		E1000_WRITE_REG(hw, E1000_CTRL_EXT,
			ctrl_ext | E1000_CTRL_EXT_DRV_LOAD);
	}
}

/*
 * em_hw_control_release resets {CTRL_EXTT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that the
 * driver is no longer loaded. For AMT versions of the
 * f/w this means that the network i/f is closed.
 */
static void
em_hw_control_release(struct e1000_hw *hw)
{
	uint32_t ctrl_ext, swsm;

	/* Let firmware taken over control of h/w */
	if (hw->mac.type == e1000_82573) {
		swsm = E1000_READ_REG(hw, E1000_SWSM);
		E1000_WRITE_REG(hw, E1000_SWSM, swsm & ~E1000_SWSM_DRV_LOAD);
	} else {
		ctrl_ext = E1000_READ_REG(hw, E1000_CTRL_EXT);
		E1000_WRITE_REG(hw, E1000_CTRL_EXT,
			ctrl_ext & ~E1000_CTRL_EXT_DRV_LOAD);
	}
}

/*
 * Bit of a misnomer, what this really means is
 * to enable OS management of the system... aka
 * to disable special hardware management features.
 */
static void
em_init_manageability(struct e1000_hw *hw)
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

/*
 * Give control back to hardware management
 * controller if there is one.
 */
static void
em_release_manageability(struct e1000_hw *hw)
{
	uint32_t manc;

	if (e1000_enable_mng_pass_thru(hw)) {
		manc = E1000_READ_REG(hw, E1000_MANC);

		/* re-enable hardware interception of ARP */
		manc |= E1000_MANC_ARP_EN;
		manc &= ~E1000_MANC_EN_MNG2HOST;

		E1000_WRITE_REG(hw, E1000_MANC, manc);
	}
}

static void
eth_em_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t rctl;

	rctl = E1000_READ_REG(hw, E1000_RCTL);
	rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);
}

static void
eth_em_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t rctl;

	rctl = E1000_READ_REG(hw, E1000_RCTL);
	rctl &= ~(E1000_RCTL_UPE | E1000_RCTL_SBP);
	if (dev->data->all_multicast == 1)
		rctl |= E1000_RCTL_MPE;
	else
		rctl &= (~E1000_RCTL_MPE);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);
}

static void
eth_em_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t rctl;

	rctl = E1000_READ_REG(hw, E1000_RCTL);
	rctl |= E1000_RCTL_MPE;
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);
}

static void
eth_em_allmulticast_disable(struct rte_eth_dev *dev)
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

static int
eth_em_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
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

	return 0;
}

static void
em_vlan_hw_filter_disable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t reg;

	/* Filter Table Disable */
	reg = E1000_READ_REG(hw, E1000_RCTL);
	reg &= ~E1000_RCTL_CFIEN;
	reg &= ~E1000_RCTL_VFE;
	E1000_WRITE_REG(hw, E1000_RCTL, reg);
}

static void
em_vlan_hw_filter_enable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct e1000_vfta * shadow_vfta =
		E1000_DEV_PRIVATE_TO_VFTA(dev->data->dev_private);
	uint32_t reg;
	int i;

	/* Filter Table Enable, CFI not used for packet acceptance */
	reg = E1000_READ_REG(hw, E1000_RCTL);
	reg &= ~E1000_RCTL_CFIEN;
	reg |= E1000_RCTL_VFE;
	E1000_WRITE_REG(hw, E1000_RCTL, reg);

	/* restore vfta from local copy */
	for (i = 0; i < IGB_VFTA_SIZE; i++)
		E1000_WRITE_REG_ARRAY(hw, E1000_VFTA, i, shadow_vfta->vfta[i]);
}

static void
em_vlan_hw_strip_disable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t reg;

	/* VLAN Mode Disable */
	reg = E1000_READ_REG(hw, E1000_CTRL);
	reg &= ~E1000_CTRL_VME;
	E1000_WRITE_REG(hw, E1000_CTRL, reg);

}

static void
em_vlan_hw_strip_enable(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t reg;

	/* VLAN Mode Enable */
	reg = E1000_READ_REG(hw, E1000_CTRL);
	reg |= E1000_CTRL_VME;
	E1000_WRITE_REG(hw, E1000_CTRL, reg);
}

static void
eth_em_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	if(mask & ETH_VLAN_STRIP_MASK){
		if (dev->data->dev_conf.rxmode.hw_vlan_strip)
			em_vlan_hw_strip_enable(dev);
		else
			em_vlan_hw_strip_disable(dev);
	}

	if(mask & ETH_VLAN_FILTER_MASK){
		if (dev->data->dev_conf.rxmode.hw_vlan_filter)
			em_vlan_hw_filter_enable(dev);
		else
			em_vlan_hw_filter_disable(dev);
	}
}

/*
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
eth_em_interrupt_setup(struct rte_eth_dev *dev)
{
	uint32_t regval;
	struct e1000_hw *hw =
		E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* clear interrupt */
	E1000_READ_REG(hw, E1000_ICR);
	regval = E1000_READ_REG(hw, E1000_IMS);
	E1000_WRITE_REG(hw, E1000_IMS, regval | E1000_ICR_LSC);
	return 0;
}

/*
 * It clears the interrupt causes and enables the interrupt.
 * It will be called once only during nic initialized.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
eth_em_rxq_interrupt_setup(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw =
	E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	E1000_READ_REG(hw, E1000_ICR);
	em_rxq_intr_enable(hw);
	return 0;
}

/*
 * It enable receive packet interrupt.
 * @param hw
 * Pointer to struct e1000_hw
 *
 * @return
 */
static void
em_rxq_intr_enable(struct e1000_hw *hw)
{
	E1000_WRITE_REG(hw, E1000_IMS, E1000_IMS_RXT0);
	E1000_WRITE_FLUSH(hw);
}

/*
 * It disabled lsc interrupt.
 * @param hw
 * Pointer to struct e1000_hw
 *
 * @return
 */
static void
em_lsc_intr_disable(struct e1000_hw *hw)
{
	E1000_WRITE_REG(hw, E1000_IMC, E1000_IMS_LSC);
	E1000_WRITE_FLUSH(hw);
}

/*
 * It disabled receive packet interrupt.
 * @param hw
 * Pointer to struct e1000_hw
 *
 * @return
 */
static void
em_rxq_intr_disable(struct e1000_hw *hw)
{
	E1000_READ_REG(hw, E1000_ICR);
	E1000_WRITE_REG(hw, E1000_IMC, E1000_IMS_RXT0);
	E1000_WRITE_FLUSH(hw);
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
eth_em_interrupt_get_status(struct rte_eth_dev *dev)
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
eth_em_interrupt_action(struct rte_eth_dev *dev)
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
	ret = eth_em_link_update(dev, 0);

	/* check if link has changed */
	if (ret < 0)
		return 0;

	memset(&link, 0, sizeof(link));
	rte_em_dev_atomic_read_link_status(dev, &link);
	if (link.link_status) {
		PMD_INIT_LOG(INFO, " Port %d: Link Up - speed %u Mbps - %s",
			     dev->data->port_id, (unsigned)link.link_speed,
			     link.link_duplex == ETH_LINK_FULL_DUPLEX ?
			     "full-duplex" : "half-duplex");
	} else {
		PMD_INIT_LOG(INFO, " Port %d: Link Down", dev->data->port_id);
	}
	PMD_INIT_LOG(DEBUG, "PCI Address: %04d:%02d:%02d:%d",
		     dev->pci_dev->addr.domain, dev->pci_dev->addr.bus,
		     dev->pci_dev->addr.devid, dev->pci_dev->addr.function);

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
eth_em_interrupt_handler(__rte_unused struct rte_intr_handle *handle,
							void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	eth_em_interrupt_get_status(dev);
	eth_em_interrupt_action(dev);
	_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC);
}

static int
eth_em_led_on(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	return e1000_led_on(hw) == E1000_SUCCESS ? 0 : -ENOTSUP;
}

static int
eth_em_led_off(struct rte_eth_dev *dev)
{
	struct e1000_hw *hw;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	return e1000_led_off(hw) == E1000_SUCCESS ? 0 : -ENOTSUP;
}

static int
eth_em_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct e1000_hw *hw;
	uint32_t ctrl;
	int tx_pause;
	int rx_pause;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	fc_conf->pause_time = hw->fc.pause_time;
	fc_conf->high_water = hw->fc.high_water;
	fc_conf->low_water = hw->fc.low_water;
	fc_conf->send_xon = hw->fc.send_xon;
	fc_conf->autoneg = hw->mac.autoneg;

	/*
	 * Return rx_pause and tx_pause status according to actual setting of
	 * the TFCE and RFCE bits in the CTRL register.
	 */
	ctrl = E1000_READ_REG(hw, E1000_CTRL);
	if (ctrl & E1000_CTRL_TFCE)
		tx_pause = 1;
	else
		tx_pause = 0;

	if (ctrl & E1000_CTRL_RFCE)
		rx_pause = 1;
	else
		rx_pause = 0;

	if (rx_pause && tx_pause)
		fc_conf->mode = RTE_FC_FULL;
	else if (rx_pause)
		fc_conf->mode = RTE_FC_RX_PAUSE;
	else if (tx_pause)
		fc_conf->mode = RTE_FC_TX_PAUSE;
	else
		fc_conf->mode = RTE_FC_NONE;

	return 0;
}

static int
eth_em_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
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
	uint32_t rctl;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if (fc_conf->autoneg != hw->mac.autoneg)
		return -ENOTSUP;
	rx_buf_size = em_get_rx_buffer_size(hw);
	PMD_INIT_LOG(DEBUG, "Rx packet buffer size = 0x%x", rx_buf_size);

	/* At least reserve one Ethernet frame for watermark */
	max_high_water = rx_buf_size - ETHER_MAX_LEN;
	if ((fc_conf->high_water > max_high_water) ||
	    (fc_conf->high_water < fc_conf->low_water)) {
		PMD_INIT_LOG(ERR, "e1000 incorrect high/low water value");
		PMD_INIT_LOG(ERR, "high water must <= 0x%x", max_high_water);
		return -EINVAL;
	}

	hw->fc.requested_mode = rte_fcmode_2_e1000_fcmode[fc_conf->mode];
	hw->fc.pause_time     = fc_conf->pause_time;
	hw->fc.high_water     = fc_conf->high_water;
	hw->fc.low_water      = fc_conf->low_water;
	hw->fc.send_xon	      = fc_conf->send_xon;

	err = e1000_setup_link_generic(hw);
	if (err == E1000_SUCCESS) {

		/* check if we want to forward MAC frames - driver doesn't have native
		 * capability to do that, so we'll write the registers ourselves */

		rctl = E1000_READ_REG(hw, E1000_RCTL);

		/* set or clear MFLCN.PMCF bit depending on configuration */
		if (fc_conf->mac_ctrl_frame_fwd != 0)
			rctl |= E1000_RCTL_PMCF;
		else
			rctl &= ~E1000_RCTL_PMCF;

		E1000_WRITE_REG(hw, E1000_RCTL, rctl);
		E1000_WRITE_FLUSH(hw);

		return 0;
	}

	PMD_INIT_LOG(ERR, "e1000_setup_link_generic = 0x%x", err);
	return -EIO;
}

static void
eth_em_rar_set(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
		uint32_t index, __rte_unused uint32_t pool)
{
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	e1000_rar_set(hw, mac_addr->addr_bytes, index);
}

static void
eth_em_rar_clear(struct rte_eth_dev *dev, uint32_t index)
{
	uint8_t addr[ETHER_ADDR_LEN];
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	memset(addr, 0, sizeof(addr));

	e1000_rar_set(hw, addr, index);
}

static int
eth_em_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct rte_eth_dev_info dev_info;
	struct e1000_hw *hw;
	uint32_t frame_size;
	uint32_t rctl;

	eth_em_infos_get(dev, &dev_info);
	frame_size = mtu + ETHER_HDR_LEN + ETHER_CRC_LEN + VLAN_TAG_SIZE;

	/* check that mtu is within the allowed range */
	if ((mtu < ETHER_MIN_MTU) || (frame_size > dev_info.max_rx_pktlen))
		return -EINVAL;

	/* refuse mtu that requires the support of scattered packets when this
	 * feature has not been enabled before. */
	if (!dev->data->scattered_rx &&
	    frame_size > dev->data->min_rx_buf_size - RTE_PKTMBUF_HEADROOM)
		return -EINVAL;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	rctl = E1000_READ_REG(hw, E1000_RCTL);

	/* switch to jumbo mode if needed */
	if (frame_size > ETHER_MAX_LEN) {
		dev->data->dev_conf.rxmode.jumbo_frame = 1;
		rctl |= E1000_RCTL_LPE;
	} else {
		dev->data->dev_conf.rxmode.jumbo_frame = 0;
		rctl &= ~E1000_RCTL_LPE;
	}
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);

	/* update max frame size */
	dev->data->dev_conf.rxmode.max_rx_pkt_len = frame_size;
	return 0;
}

static int
eth_em_set_mc_addr_list(struct rte_eth_dev *dev,
			struct ether_addr *mc_addr_set,
			uint32_t nb_mc_addr)
{
	struct e1000_hw *hw;

	hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	e1000_update_mc_addr_list(hw, (u8 *)mc_addr_set, nb_mc_addr);
	return 0;
}

struct rte_driver em_pmd_drv = {
	.type = PMD_PDEV,
	.init = rte_em_pmd_init,
};

PMD_REGISTER_DRIVER(em_pmd_drv, em);
DRIVER_REGISTER_PCI_TABLE(em, pci_id_em_map);
