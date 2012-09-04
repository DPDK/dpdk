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
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_atomic.h>
#include <rte_malloc.h>

#include "ixgbe_logs.h"
#include "ixgbe/ixgbe_api.h"
#include "ixgbe/ixgbe_vf.h"
#include "ixgbe/ixgbe_common.h"
#include "ixgbe_ethdev.h"

/*
 * High threshold controlling when to start sending XOFF frames. Must be at
 * least 8 bytes less than receive packet buffer size. This value is in units
 * of 1024 bytes.
 */
#define IXGBE_FC_HI    0x80

/*
 * Low threshold controlling when to start sending XON frames. This value is
 * in units of 1024 bytes.
 */
#define IXGBE_FC_LO    0x40

/* Timer value included in XOFF frames. */
#define IXGBE_FC_PAUSE 0x680

#define IXGBE_LINK_DOWN_CHECK_TIMEOUT 4000 /* ms */
#define IXGBE_LINK_UP_CHECK_TIMEOUT   1000 /* ms */

static int eth_ixgbe_dev_init(struct eth_driver *eth_drv,
		struct rte_eth_dev *eth_dev);
static int  ixgbe_dev_configure(struct rte_eth_dev *dev, uint16_t nb_rx_q,
				uint16_t nb_tx_q);
static int  ixgbe_dev_start(struct rte_eth_dev *dev);
static void ixgbe_dev_stop(struct rte_eth_dev *dev);
static void ixgbe_dev_close(struct rte_eth_dev *dev);
static void ixgbe_dev_promiscuous_enable(struct rte_eth_dev *dev);
static void ixgbe_dev_promiscuous_disable(struct rte_eth_dev *dev);
static void ixgbe_dev_allmulticast_enable(struct rte_eth_dev *dev);
static void ixgbe_dev_allmulticast_disable(struct rte_eth_dev *dev);
static int ixgbe_dev_link_update(struct rte_eth_dev *dev,
				int wait_to_complete);
static void ixgbe_dev_stats_get(struct rte_eth_dev *dev,
				struct rte_eth_stats *stats);
static void ixgbe_dev_stats_reset(struct rte_eth_dev *dev);
static void ixgbe_dev_info_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);
static void ixgbe_vlan_filter_set(struct rte_eth_dev *dev,
				  uint16_t vlan_id,
				  int on);
static int ixgbe_dev_led_on(struct rte_eth_dev *dev);
static int ixgbe_dev_led_off(struct rte_eth_dev *dev);
static int  ixgbe_flow_ctrl_set(struct rte_eth_dev *dev,
				struct rte_eth_fc_conf *fc_conf);
static void ixgbe_dev_link_status_print(struct rte_eth_dev *dev);
static int ixgbe_dev_interrupt_setup(struct rte_eth_dev *dev);
static int ixgbe_dev_interrupt_get_status(struct rte_eth_dev *dev);
static int ixgbe_dev_interrupt_action(struct rte_eth_dev *dev);
static void ixgbe_dev_interrupt_handler(struct rte_intr_handle *handle,
							void *param);
static void ixgbe_dev_interrupt_delayed_handler(void *param);
static void ixgbe_add_rar(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
				uint32_t index, uint32_t pool);
static void ixgbe_remove_rar(struct rte_eth_dev *dev, uint32_t index);

/* For Virtual Function support */
static int eth_ixgbevf_dev_init(struct eth_driver *eth_drv,
		struct rte_eth_dev *eth_dev);
static int  ixgbevf_dev_configure(struct rte_eth_dev *dev, uint16_t nb_rx_q,
		uint16_t nb_tx_q);
static int  ixgbevf_dev_start(struct rte_eth_dev *dev);
static void ixgbevf_dev_stop(struct rte_eth_dev *dev);
static void ixgbevf_intr_disable(struct ixgbe_hw *hw);
static void ixgbevf_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats);
static void ixgbevf_dev_stats_reset(struct rte_eth_dev *dev);

/*
 *  * Define VF Stats MACRO for Non "cleared on read" register
 *   */
#define UPDATE_VF_STAT(reg, last, cur)	                        \
{                                                               \
	u32 latest = IXGBE_READ_REG(hw, reg);                   \
	cur += latest - last;                                   \
	last = latest;                                          \
}

#define UPDATE_VF_STAT_36BIT(lsb, msb, last, cur)                \
{                                                                \
	u64 new_lsb = IXGBE_READ_REG(hw, lsb);                   \
	u64 new_msb = IXGBE_READ_REG(hw, msb);                   \
	u64 latest = ((new_msb << 32) | new_lsb);                \
	cur += (0x1000000000LL + latest - last) & 0xFFFFFFFFFLL; \
	last = latest;                                           \
}

/*
 * The set of PCI devices this driver supports
 */
static struct rte_pci_id pci_id_ixgbe_map[] = {

#undef RTE_LIBRTE_IGB_PMD
#define RTE_PCI_DEV_ID_DECL(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#include "rte_pci_dev_ids.h"

{ .vendor_id = 0, /* sentinel */ },
};


/*
 * The set of PCI devices this driver supports (for 82599 VF)
 */
static struct rte_pci_id pci_id_ixgbevf_map[] = {
{
	.vendor_id = PCI_VENDOR_ID_INTEL,
	.device_id = IXGBE_DEV_ID_82599_VF,
	.subsystem_vendor_id = PCI_ANY_ID,
	.subsystem_device_id = PCI_ANY_ID,
},
{ .vendor_id = 0, /* sentinel */ },
};

static struct eth_dev_ops ixgbe_eth_dev_ops = {
	.dev_configure        = ixgbe_dev_configure,
	.dev_start            = ixgbe_dev_start,
	.dev_stop             = ixgbe_dev_stop,
	.dev_close            = ixgbe_dev_close,
	.promiscuous_enable   = ixgbe_dev_promiscuous_enable,
	.promiscuous_disable  = ixgbe_dev_promiscuous_disable,
	.allmulticast_enable  = ixgbe_dev_allmulticast_enable,
	.allmulticast_disable = ixgbe_dev_allmulticast_disable,
	.link_update          = ixgbe_dev_link_update,
	.stats_get            = ixgbe_dev_stats_get,
	.stats_reset          = ixgbe_dev_stats_reset,
	.dev_infos_get        = ixgbe_dev_info_get,
	.vlan_filter_set      = ixgbe_vlan_filter_set,
	.rx_queue_setup       = ixgbe_dev_rx_queue_setup,
	.tx_queue_setup       = ixgbe_dev_tx_queue_setup,
	.dev_led_on           = ixgbe_dev_led_on,
	.dev_led_off          = ixgbe_dev_led_off,
	.flow_ctrl_set        = ixgbe_flow_ctrl_set,
	.mac_addr_add         = ixgbe_add_rar,
	.mac_addr_remove      = ixgbe_remove_rar,
	.fdir_add_signature_filter    = ixgbe_fdir_add_signature_filter,
	.fdir_update_signature_filter = ixgbe_fdir_update_signature_filter,
	.fdir_remove_signature_filter = ixgbe_fdir_remove_signature_filter,
	.fdir_infos_get               = ixgbe_fdir_info_get,
	.fdir_add_perfect_filter      = ixgbe_fdir_add_perfect_filter,
	.fdir_update_perfect_filter   = ixgbe_fdir_update_perfect_filter,
	.fdir_remove_perfect_filter   = ixgbe_fdir_remove_perfect_filter,
	.fdir_set_masks               = ixgbe_fdir_set_masks,
};

/*
 * dev_ops for virtual function, bare necessities for basic vf
 * operation have been implemented
 */
static struct eth_dev_ops ixgbevf_eth_dev_ops = {

	.dev_configure        = ixgbevf_dev_configure,
	.dev_start            = ixgbevf_dev_start,
	.dev_stop             = ixgbevf_dev_stop,
	.link_update          = ixgbe_dev_link_update,
	.stats_get            = ixgbevf_dev_stats_get,
	.stats_reset          = ixgbevf_dev_stats_reset,
	.dev_close            = ixgbevf_dev_stop,

	.dev_infos_get        = ixgbe_dev_info_get,
	.rx_queue_setup       = ixgbe_dev_rx_queue_setup,
	.tx_queue_setup       = ixgbe_dev_tx_queue_setup,
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
rte_ixgbe_dev_atomic_read_link_status(struct rte_eth_dev *dev,
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
rte_ixgbe_dev_atomic_write_link_status(struct rte_eth_dev *dev,
				struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &(dev->data->dev_link);
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

/*
 * This function is the same as ixgbe_is_sfp() in ixgbe/ixgbe.h.
 */
static inline int
ixgbe_is_sfp(struct ixgbe_hw *hw)
{
	switch (hw->phy.type) {
	case ixgbe_phy_sfp_avago:
	case ixgbe_phy_sfp_ftl:
	case ixgbe_phy_sfp_intel:
	case ixgbe_phy_sfp_unknown:
	case ixgbe_phy_sfp_passive_tyco:
	case ixgbe_phy_sfp_passive_unknown:
		return 1;
	default:
		return 0;
	}
}

/*
 * This function is based on ixgbe_disable_intr() in ixgbe/ixgbe.h.
 */
static void
ixgbe_disable_intr(struct ixgbe_hw *hw)
{
	PMD_INIT_FUNC_TRACE();

	if (hw->mac.type == ixgbe_mac_82598EB) {
		IXGBE_WRITE_REG(hw, IXGBE_EIMC, ~0);
	} else {
		IXGBE_WRITE_REG(hw, IXGBE_EIMC, 0xFFFF0000);
		IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(0), ~0);
		IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(1), ~0);
	}
	IXGBE_WRITE_FLUSH(hw);
}

/*
 * This function resets queue statistics mapping registers.
 * From Niantic datasheet, Initialization of Statistics section:
 * "...if software requires the queue counters, the RQSMR and TQSM registers
 * must be re-programmed following a device reset.
 */
static void
ixgbe_reset_qstat_mappings(struct ixgbe_hw *hw)
{
	uint32_t i;
	for(i = 0; i != 16; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_RQSMR(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TQSM(i), 0);
	}
}

/*
 * This function is based on code in ixgbe_attach() in ixgbe/ixgbe.c.
 * It returns 0 on success.
 */
static int
eth_ixgbe_dev_init(__attribute__((unused)) struct eth_driver *eth_drv,
		     struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	struct ixgbe_vfta * shadow_vfta =
		IXGBE_DEV_PRIVATE_TO_VFTA(eth_dev->data->dev_private);
	uint32_t ctrl_ext;
	uint16_t csum;
	int diag, i;

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &ixgbe_eth_dev_ops;
	eth_dev->rx_pkt_burst = &ixgbe_recv_pkts;
	eth_dev->tx_pkt_burst = &ixgbe_xmit_pkts;

	/* for secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY){
		if (eth_dev->data->scattered_rx)
			eth_dev->rx_pkt_burst = ixgbe_recv_scattered_pkts;
		return 0;
	}
	pci_dev = eth_dev->pci_dev;

	/* Vendor and Device ID need to be set before init of shared code */
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->hw_addr = (void *)pci_dev->mem_resource.addr;

	/* Initialize the shared code */
	diag = ixgbe_init_shared_code(hw);
	if (diag != IXGBE_SUCCESS) {
		PMD_INIT_LOG(ERR, "Shared code init failed: %d", diag);
		return -EIO;
	}

	/* Get Hardware Flow Control setting */
	hw->fc.requested_mode = ixgbe_fc_full;
	hw->fc.current_mode = ixgbe_fc_full;
	hw->fc.pause_time = IXGBE_FC_PAUSE;
	hw->fc.low_water = IXGBE_FC_LO;
	for (i = 0; i < MAX_TRAFFIC_CLASS; i++)
		hw->fc.high_water[i] = IXGBE_FC_HI;
	hw->fc.send_xon = 1;

	ixgbe_disable_intr(hw);

	/* Make sure we have a good EEPROM before we read from it */
	diag = ixgbe_validate_eeprom_checksum(hw, &csum);
	if (diag != IXGBE_SUCCESS) {
		PMD_INIT_LOG(ERR, "The EEPROM checksum is not valid: %d", diag);
		return -EIO;
	}

	diag = ixgbe_init_hw(hw);

	/*
	 * Devices with copper phys will fail to initialise if ixgbe_init_hw()
	 * is called too soon after the kernel driver unbinding/binding occurs.
	 * The failure occurs in ixgbe_identify_phy_generic() for all devices,
	 * but for non-copper devies, ixgbe_identify_sfp_module_generic() is
	 * also called. See ixgbe_identify_phy_82599(). The reason for the
	 * failure is not known, and only occuts when virtualisation features
	 * are disabled in the bios. A delay of 100ms  was found to be enough by
	 * trial-and-error, and is doubled to be safe.
	 */
	if (diag && (hw->mac.ops.get_media_type(hw) == ixgbe_media_type_copper)) {
		rte_delay_ms(200);
		diag = ixgbe_init_hw(hw);
	}

	if (diag == IXGBE_ERR_EEPROM_VERSION) {
		PMD_INIT_LOG(ERR, "This device is a pre-production adapter/"
		    "LOM.  Please be aware there may be issues associated "
		    "with your hardware.\n If you are experiencing problems "
		    "please contact your Intel or hardware representative "
		    "who provided you with this hardware.\n");
	} else if (diag == IXGBE_ERR_SFP_NOT_SUPPORTED)
		PMD_INIT_LOG(ERR, "Unsupported SFP+ Module\n");
	if (diag) {
		PMD_INIT_LOG(ERR, "Hardware Initialization Failure: %d", diag);
		return -EIO;
	}

	/* pick up the PCI bus settings for reporting later */
	ixgbe_get_bus_info(hw);

	/* reset mappings for queue statistics hw counters*/
	ixgbe_reset_qstat_mappings(hw);

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("ixgbe", ETHER_ADDR_LEN *
			hw->mac.num_rar_entries, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			"Failed to allocate %d bytes needed to store MAC addresses",
			ETHER_ADDR_LEN * hw->mac.num_rar_entries);
		return -ENOMEM;
	}
	/* Copy the permanent MAC address */
	ether_addr_copy((struct ether_addr *) hw->mac.perm_addr,
			&eth_dev->data->mac_addrs[0]);

	/* initialize the vfta */
	memset(shadow_vfta, 0, sizeof(*shadow_vfta));

	/* let hardware know driver is loaded */
	ctrl_ext = IXGBE_READ_REG(hw, IXGBE_CTRL_EXT);
	ctrl_ext |= IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(hw, IXGBE_CTRL_EXT, ctrl_ext);

	if (ixgbe_is_sfp(hw) && hw->phy.sfp_type != ixgbe_sfp_type_not_present)
		PMD_INIT_LOG(DEBUG,
			     "MAC: %d, PHY: %d, SFP+: %d<n",
			     (int) hw->mac.type, (int) hw->phy.type,
			     (int) hw->phy.sfp_type);
	else
		PMD_INIT_LOG(DEBUG, "MAC: %d, PHY: %d\n",
			     (int) hw->mac.type, (int) hw->phy.type);

	PMD_INIT_LOG(DEBUG, "port %d vendorID=0x%x deviceID=0x%x",
			eth_dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);

	rte_intr_callback_register(&(pci_dev->intr_handle),
		ixgbe_dev_interrupt_handler, (void *)eth_dev);

	return 0;
}

/*
 * Virtual Function device init
 */
static int
eth_ixgbevf_dev_init(__attribute__((unused)) struct eth_driver *eth_drv,
		     struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	int diag;

	PMD_INIT_LOG(DEBUG, "eth_ixgbevf_dev_init");

	eth_dev->dev_ops = &ixgbevf_eth_dev_ops;
	pci_dev = eth_dev->pci_dev;

	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->hw_addr = (void *)pci_dev->mem_resource.addr;

	/* Initialize the shared code */
	diag = ixgbe_init_shared_code(hw);
	if (diag != IXGBE_SUCCESS) {
		PMD_INIT_LOG(ERR, "Shared code init failed for ixgbevf: %d", diag);
		return -EIO;
	}

	/* init_mailbox_params */
	hw->mbx.ops.init_params(hw);

	/* Disable the interrupts for VF */
	ixgbevf_intr_disable(hw);

	hw->mac.num_rar_entries = hw->mac.max_rx_queues;
	diag = hw->mac.ops.reset_hw(hw);

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("ixgbevf", ETHER_ADDR_LEN *
			hw->mac.num_rar_entries, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			"Failed to allocate %d bytes needed to store MAC addresses",
			ETHER_ADDR_LEN * hw->mac.num_rar_entries);
		return -ENOMEM;
	}
	/* Copy the permanent MAC address */
	ether_addr_copy((struct ether_addr *) hw->mac.perm_addr,
			&eth_dev->data->mac_addrs[0]);

	/* reset the hardware with the new settings */
	diag = hw->mac.ops.start_hw(hw);
	switch (diag) {
		case  0:
			break;

		default:
			PMD_INIT_LOG(ERR, "VF Initialization Failure: %d", diag);
			return (diag);
	}

	PMD_INIT_LOG(DEBUG, "\nport %d vendorID=0x%x deviceID=0x%x mac.type=%s\n",
			 eth_dev->data->port_id, pci_dev->id.vendor_id, pci_dev->id.device_id,
			 "ixgbe_mac_82599_vf");

	return 0;
}

static struct eth_driver rte_ixgbe_pmd = {
	{
		.name = "rte_ixgbe_pmd",
		.id_table = pci_id_ixgbe_map,
		.drv_flags = RTE_PCI_DRV_NEED_IGB_UIO,
	},
	.eth_dev_init = eth_ixgbe_dev_init,
	.dev_private_size = sizeof(struct ixgbe_adapter),
};

/*
 * virtual function driver struct
 */
static struct eth_driver rte_ixgbevf_pmd = {
	{
		.name = "rte_ixgbevf_pmd",
		.id_table = pci_id_ixgbevf_map,
		.drv_flags = RTE_PCI_DRV_NEED_IGB_UIO,
	},
	.eth_dev_init = eth_ixgbevf_dev_init,
	.dev_private_size = sizeof(struct ixgbe_adapter),
};

/*
 * Driver initialization routine.
 * Invoked once at EAL init time.
 * Register itself as the [Poll Mode] Driver of PCI IXGBE devices.
 */
int
rte_ixgbe_pmd_init(void)
{
	PMD_INIT_FUNC_TRACE();

	rte_eth_driver_register(&rte_ixgbe_pmd);
	return 0;
}

/*
 * VF Driver initialization routine.
 * Invoked one at EAL init time.
 * Register itself as the [Virtual Poll Mode] Driver of PCI niantic devices.
 */
int
rte_ixgbevf_pmd_init(void)
{
	DEBUGFUNC("rte_ixgbevf_pmd_init");

	rte_eth_driver_register(&rte_ixgbevf_pmd);
	return (0);
}

static void
ixgbe_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_vfta * shadow_vfta =
		IXGBE_DEV_PRIVATE_TO_VFTA(dev->data->dev_private);
	uint32_t vfta;
	uint32_t vid_idx;
	uint32_t vid_bit;

	vid_idx = (uint32_t) ((vlan_id >> 5) & 0x7F);
	vid_bit = (uint32_t) (1 << (vlan_id & 0x1F));
	vfta = IXGBE_READ_REG(hw, IXGBE_VFTA(vid_idx));
	if (on)
		vfta |= vid_bit;
	else
		vfta &= ~vid_bit;
	IXGBE_WRITE_REG(hw, IXGBE_VFTA(vid_idx), vfta);

	/* update local VFTA copy */
	shadow_vfta->vfta[vid_idx] = vfta;
}

static void
ixgbe_vlan_hw_support_disable(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t vlnctrl;
	uint32_t rxdctl;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	/* Filter Table Disable */
	vlnctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
	vlnctrl &= ~IXGBE_VLNCTRL_VFE;

	if (hw->mac.type == ixgbe_mac_82598EB)
		vlnctrl &= ~IXGBE_VLNCTRL_VME;
	else {
		/* On 82599 the VLAN enable is per/queue in RXDCTL */
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(i));
			rxdctl &= ~IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(i), rxdctl);
		}
	}
	IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlnctrl);
}

static void
ixgbe_vlan_hw_support_enable(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_vfta * shadow_vfta =
		IXGBE_DEV_PRIVATE_TO_VFTA(dev->data->dev_private);
	uint32_t vlnctrl;
	uint32_t rxdctl;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	/* Filter Table Enable */
	vlnctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
	vlnctrl &= ~IXGBE_VLNCTRL_CFIEN;
	vlnctrl |= IXGBE_VLNCTRL_VFE;

	if (hw->mac.type == ixgbe_mac_82598EB)
		vlnctrl |= IXGBE_VLNCTRL_VME;
	else {
		/* On 82599 the VLAN enable is per/queue in RXDCTL */
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(i));
			rxdctl |= IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(i), rxdctl);
		}
	}
	IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlnctrl);

	/* write whatever is in local vfta copy */
	for (i = 0; i < IXGBE_VFTA_SIZE; i++)
		IXGBE_WRITE_REG(hw, IXGBE_VFTA(i), shadow_vfta->vfta[i]);
}

static int
ixgbe_dev_configure(struct rte_eth_dev *dev, uint16_t nb_rx_q, uint16_t nb_tx_q)
{
	struct ixgbe_interrupt *intr =
		IXGBE_DEV_PRIVATE_TO_INTR(dev->data->dev_private);
	int diag;

	PMD_INIT_FUNC_TRACE();

	/* Allocate the array of pointers to RX queue structures */
	diag = ixgbe_dev_rx_queue_alloc(dev, nb_rx_q);
	if (diag != 0) {
		PMD_INIT_LOG(ERR, "ethdev port_id=%d allocation of array of %d"
			     "pointers to RX queues failed", dev->data->port_id,
			     nb_rx_q);
		return diag;
	}

	/* Allocate the array of pointers to TX queue structures */
	diag = ixgbe_dev_tx_queue_alloc(dev, nb_tx_q);
	if (diag != 0) {
		PMD_INIT_LOG(ERR, "ethdev port_id=%d allocation of array of %d"
			     "pointers to TX queues failed", dev->data->port_id,
			     nb_tx_q);
		return diag;
	}

	/* set flag to update link status after init */
	intr->flags |= IXGBE_FLAG_NEED_LINK_UPDATE;

	return 0;
}

/*
 * Configure device link speed and setup link.
 * It returns 0 on success.
 */
static int
ixgbe_dev_start(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int err, link_up = 0, negotiate = 0;
	uint32_t speed = 0;

	PMD_INIT_FUNC_TRACE();

	/* IXGBE devices don't support half duplex */
	if ((dev->data->dev_conf.link_duplex != ETH_LINK_AUTONEG_DUPLEX) &&
			(dev->data->dev_conf.link_duplex != ETH_LINK_FULL_DUPLEX)) {
		PMD_INIT_LOG(ERR, "Invalid link_duplex (%u) for port %u\n",
				dev->data->dev_conf.link_duplex,
				dev->data->port_id);
		return -EINVAL;
	}

	/* stop adapter */
	hw->adapter_stopped = FALSE;
	ixgbe_stop_adapter(hw);

	/* reinitialize adapter
	 * this calls reset and start */
	ixgbe_init_hw(hw);

	/* initialize transmission unit */
	ixgbe_dev_tx_init(dev);

	/* This can fail when allocating mbufs for descriptor rings */
	err = ixgbe_dev_rx_init(dev);
	if (err) {
		PMD_INIT_LOG(ERR, "Unable to initialize RX hardware\n");
		return err;
	}

	ixgbe_dev_rxtx_start(dev);

	if (ixgbe_is_sfp(hw) && hw->phy.multispeed_fiber) {
		err = hw->mac.ops.setup_sfp(hw);
		if (err)
			goto error;
	}

	/* Turn on the laser */
	if (hw->phy.multispeed_fiber)
		ixgbe_enable_tx_laser(hw);

	err = ixgbe_check_link(hw, &speed, &link_up, 0);
	if (err)
		goto error;
	err = ixgbe_get_link_capabilities(hw, &speed, &negotiate);
	if (err)
		goto error;

	switch(dev->data->dev_conf.link_speed) {
	case ETH_LINK_SPEED_AUTONEG:
		speed = (hw->mac.type != ixgbe_mac_82598EB) ?
				IXGBE_LINK_SPEED_82599_AUTONEG :
				IXGBE_LINK_SPEED_82598_AUTONEG;
		break;
	case ETH_LINK_SPEED_100:
		/*
		 * Invalid for 82598 but error will be detected by
		 * ixgbe_setup_link()
		 */
		speed = IXGBE_LINK_SPEED_100_FULL;
		break;
	case ETH_LINK_SPEED_1000:
		speed = IXGBE_LINK_SPEED_1GB_FULL;
		break;
	case ETH_LINK_SPEED_10000:
		speed = IXGBE_LINK_SPEED_10GB_FULL;
		break;
	default:
		PMD_INIT_LOG(ERR, "Invalid link_speed (%u) for port %u\n",
				dev->data->dev_conf.link_speed, dev->data->port_id);
		return -EINVAL;
	}

	err = ixgbe_setup_link(hw, speed, negotiate, link_up);
	if (err)
		goto error;

	/* check if lsc interrupt is enabled */
	if (dev->data->dev_conf.intr_conf.lsc != 0) {
		err = ixgbe_dev_interrupt_setup(dev);
		if (err)
			goto error;
	}

	/*
	 * If VLAN filtering is enabled, set up VLAN tag offload and filtering
	 * and restore VFTA.
	 */
	if (dev->data->dev_conf.rxmode.hw_vlan_filter)
		ixgbe_vlan_hw_support_enable(dev);
	else
		ixgbe_vlan_hw_support_disable(dev);

	if (dev->data->dev_conf.fdir_conf.mode != RTE_FDIR_MODE_NONE) {
		err = ixgbe_fdir_configure(dev);
		if (err)
			goto error;
	}

	return (0);

error:
	PMD_INIT_LOG(ERR, "failure in ixgbe_dev_start(): %d", err);
	return -EIO;
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static void
ixgbe_dev_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_link link;
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	/* disable interrupts */
	ixgbe_disable_intr(hw);

	/* reset the NIC */
	ixgbe_reset_hw(hw);
	hw->adapter_stopped = FALSE;

	/* stop adapter */
	ixgbe_stop_adapter(hw);

	/* Turn off the laser */
	if (hw->phy.multispeed_fiber)
		ixgbe_disable_tx_laser(hw);

	ixgbe_dev_clear_queues(dev);

	/* Clear recorded link status */
	memset(&link, 0, sizeof(link));
	rte_ixgbe_dev_atomic_write_link_status(dev, &link);
}

/*
 * Reest and stop device.
 */
static void
ixgbe_dev_close(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	ixgbe_reset_hw(hw);


	ixgbe_dev_stop(dev);
	hw->adapter_stopped = 1;

	ixgbe_disable_pcie_master(hw);

	/* reprogram the RAR[0] in case user changed it. */
	ixgbe_set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);
}

/*
 * This function is based on ixgbe_update_stats_counters() in ixgbe/ixgbe.c
 */
static void
ixgbe_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct ixgbe_hw *hw =
			IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_hw_stats *hw_stats =
			IXGBE_DEV_PRIVATE_TO_STATS(dev->data->dev_private);
	uint32_t bprc, lxon, lxoff, total;
	uint64_t total_missed_rx, total_qbrc, total_qprc;
	unsigned i;

	total_missed_rx = 0;
	total_qbrc = 0;
	total_qprc = 0;

	hw_stats->crcerrs += IXGBE_READ_REG(hw, IXGBE_CRCERRS);
	hw_stats->illerrc += IXGBE_READ_REG(hw, IXGBE_ILLERRC);
	hw_stats->errbc += IXGBE_READ_REG(hw, IXGBE_ERRBC);
	hw_stats->mspdc += IXGBE_READ_REG(hw, IXGBE_MSPDC);

	for (i = 0; i < 8; i++) {
		uint32_t mp;
		mp = IXGBE_READ_REG(hw, IXGBE_MPC(i));
		/* global total per queue */
		hw_stats->mpc[i] += mp;
		/* Running comprehensive total for stats display */
		total_missed_rx += hw_stats->mpc[i];
		if (hw->mac.type == ixgbe_mac_82598EB)
			hw_stats->rnbc[i] +=
			    IXGBE_READ_REG(hw, IXGBE_RNBC(i));
		hw_stats->pxontxc[i] +=
		    IXGBE_READ_REG(hw, IXGBE_PXONTXC(i));
		hw_stats->pxonrxc[i] +=
		    IXGBE_READ_REG(hw, IXGBE_PXONRXC(i));
		hw_stats->pxofftxc[i] +=
		    IXGBE_READ_REG(hw, IXGBE_PXOFFTXC(i));
		hw_stats->pxoffrxc[i] +=
		    IXGBE_READ_REG(hw, IXGBE_PXOFFRXC(i));
		hw_stats->pxon2offc[i] +=
		    IXGBE_READ_REG(hw, IXGBE_PXON2OFFCNT(i));
	}
	for (i = 0; i < 16; i++) {
		hw_stats->qprc[i] += IXGBE_READ_REG(hw, IXGBE_QPRC(i));
		hw_stats->qptc[i] += IXGBE_READ_REG(hw, IXGBE_QPTC(i));
		hw_stats->qbrc[i] += IXGBE_READ_REG(hw, IXGBE_QBRC_L(i));
		hw_stats->qbrc[i] +=
		    ((uint64_t)IXGBE_READ_REG(hw, IXGBE_QBRC_H(i)) << 32);
		hw_stats->qbtc[i] += IXGBE_READ_REG(hw, IXGBE_QBTC_L(i));
		hw_stats->qbtc[i] +=
		    ((uint64_t)IXGBE_READ_REG(hw, IXGBE_QBTC_H(i)) << 32);
		hw_stats->qprdc[i] += IXGBE_READ_REG(hw, IXGBE_QPRDC(i));

		total_qprc += hw_stats->qprc[i];
		total_qbrc += hw_stats->qbrc[i];
	}
	hw_stats->mlfc += IXGBE_READ_REG(hw, IXGBE_MLFC);
	hw_stats->mrfc += IXGBE_READ_REG(hw, IXGBE_MRFC);
	hw_stats->rlec += IXGBE_READ_REG(hw, IXGBE_RLEC);

	/* Note that gprc counts missed packets */
	hw_stats->gprc += IXGBE_READ_REG(hw, IXGBE_GPRC);

	if (hw->mac.type != ixgbe_mac_82598EB) {
		hw_stats->gorc += IXGBE_READ_REG(hw, IXGBE_GORCL) +
		    ((u64)IXGBE_READ_REG(hw, IXGBE_GORCH) << 32);
		hw_stats->gotc += IXGBE_READ_REG(hw, IXGBE_GOTCL) +
		    ((u64)IXGBE_READ_REG(hw, IXGBE_GOTCH) << 32);
		hw_stats->tor += IXGBE_READ_REG(hw, IXGBE_TORL) +
		    ((u64)IXGBE_READ_REG(hw, IXGBE_TORH) << 32);
		hw_stats->lxonrxc += IXGBE_READ_REG(hw, IXGBE_LXONRXCNT);
		hw_stats->lxoffrxc += IXGBE_READ_REG(hw, IXGBE_LXOFFRXCNT);
	} else {
		hw_stats->lxonrxc += IXGBE_READ_REG(hw, IXGBE_LXONRXC);
		hw_stats->lxoffrxc += IXGBE_READ_REG(hw, IXGBE_LXOFFRXC);
		/* 82598 only has a counter in the high register */
		hw_stats->gorc += IXGBE_READ_REG(hw, IXGBE_GORCH);
		hw_stats->gotc += IXGBE_READ_REG(hw, IXGBE_GOTCH);
		hw_stats->tor += IXGBE_READ_REG(hw, IXGBE_TORH);
	}

	/*
	 * Workaround: mprc hardware is incorrectly counting
	 * broadcasts, so for now we subtract those.
	 */
	bprc = IXGBE_READ_REG(hw, IXGBE_BPRC);
	hw_stats->bprc += bprc;
	hw_stats->mprc += IXGBE_READ_REG(hw, IXGBE_MPRC);
	if (hw->mac.type == ixgbe_mac_82598EB)
		hw_stats->mprc -= bprc;

	hw_stats->prc64 += IXGBE_READ_REG(hw, IXGBE_PRC64);
	hw_stats->prc127 += IXGBE_READ_REG(hw, IXGBE_PRC127);
	hw_stats->prc255 += IXGBE_READ_REG(hw, IXGBE_PRC255);
	hw_stats->prc511 += IXGBE_READ_REG(hw, IXGBE_PRC511);
	hw_stats->prc1023 += IXGBE_READ_REG(hw, IXGBE_PRC1023);
	hw_stats->prc1522 += IXGBE_READ_REG(hw, IXGBE_PRC1522);

	lxon = IXGBE_READ_REG(hw, IXGBE_LXONTXC);
	hw_stats->lxontxc += lxon;
	lxoff = IXGBE_READ_REG(hw, IXGBE_LXOFFTXC);
	hw_stats->lxofftxc += lxoff;
	total = lxon + lxoff;

	hw_stats->gptc += IXGBE_READ_REG(hw, IXGBE_GPTC);
	hw_stats->mptc += IXGBE_READ_REG(hw, IXGBE_MPTC);
	hw_stats->ptc64 += IXGBE_READ_REG(hw, IXGBE_PTC64);
	hw_stats->gptc -= total;
	hw_stats->mptc -= total;
	hw_stats->ptc64 -= total;
	hw_stats->gotc -= total * ETHER_MIN_LEN;

	hw_stats->ruc += IXGBE_READ_REG(hw, IXGBE_RUC);
	hw_stats->rfc += IXGBE_READ_REG(hw, IXGBE_RFC);
	hw_stats->roc += IXGBE_READ_REG(hw, IXGBE_ROC);
	hw_stats->rjc += IXGBE_READ_REG(hw, IXGBE_RJC);
	hw_stats->mngprc += IXGBE_READ_REG(hw, IXGBE_MNGPRC);
	hw_stats->mngpdc += IXGBE_READ_REG(hw, IXGBE_MNGPDC);
	hw_stats->mngptc += IXGBE_READ_REG(hw, IXGBE_MNGPTC);
	hw_stats->tpr += IXGBE_READ_REG(hw, IXGBE_TPR);
	hw_stats->tpt += IXGBE_READ_REG(hw, IXGBE_TPT);
	hw_stats->ptc127 += IXGBE_READ_REG(hw, IXGBE_PTC127);
	hw_stats->ptc255 += IXGBE_READ_REG(hw, IXGBE_PTC255);
	hw_stats->ptc511 += IXGBE_READ_REG(hw, IXGBE_PTC511);
	hw_stats->ptc1023 += IXGBE_READ_REG(hw, IXGBE_PTC1023);
	hw_stats->ptc1522 += IXGBE_READ_REG(hw, IXGBE_PTC1522);
	hw_stats->bptc += IXGBE_READ_REG(hw, IXGBE_BPTC);
	hw_stats->xec += IXGBE_READ_REG(hw, IXGBE_XEC);
	hw_stats->fccrc += IXGBE_READ_REG(hw, IXGBE_FCCRC);
	hw_stats->fclast += IXGBE_READ_REG(hw, IXGBE_FCLAST);
	/* Only read FCOE on 82599 */
	if (hw->mac.type != ixgbe_mac_82598EB) {
		hw_stats->fcoerpdc += IXGBE_READ_REG(hw, IXGBE_FCOERPDC);
		hw_stats->fcoeprc += IXGBE_READ_REG(hw, IXGBE_FCOEPRC);
		hw_stats->fcoeptc += IXGBE_READ_REG(hw, IXGBE_FCOEPTC);
		hw_stats->fcoedwrc += IXGBE_READ_REG(hw, IXGBE_FCOEDWRC);
		hw_stats->fcoedwtc += IXGBE_READ_REG(hw, IXGBE_FCOEDWTC);
	}

	if (stats == NULL)
		return;

	/* Fill out the rte_eth_stats statistics structure */
	stats->ipackets = total_qprc;
	stats->ibytes = total_qbrc;
	stats->opackets = hw_stats->gptc;
	stats->obytes = hw_stats->gotc;
	stats->imcasts = hw_stats->mprc;

	/* Rx Errors */
	stats->ierrors = total_missed_rx + hw_stats->crcerrs +
		hw_stats->rlec;

	stats->oerrors  = 0;

	/* Flow Director Stats registers */
	hw_stats->fdirmatch += IXGBE_READ_REG(hw, IXGBE_FDIRMATCH);
	hw_stats->fdirmiss += IXGBE_READ_REG(hw, IXGBE_FDIRMISS);
	stats->fdirmatch = hw_stats->fdirmatch;
	stats->fdirmiss = hw_stats->fdirmiss;
}

static void
ixgbe_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct ixgbe_hw_stats *stats =
			IXGBE_DEV_PRIVATE_TO_STATS(dev->data->dev_private);

	/* HW registers are cleared on read */
	ixgbe_dev_stats_get(dev, NULL);

	/* Reset software totals */
	memset(stats, 0, sizeof(*stats));
}

static void
ixgbevf_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbevf_hw_stats *hw_stats = (struct ixgbevf_hw_stats*)
			  IXGBE_DEV_PRIVATE_TO_STATS(dev->data->dev_private);

	/* Good Rx packet, include VF loopback */
	UPDATE_VF_STAT(IXGBE_VFGPRC,
	    hw_stats->last_vfgprc, hw_stats->vfgprc);

	/* Good Rx octets, include VF loopback */
	UPDATE_VF_STAT_36BIT(IXGBE_VFGORC_LSB, IXGBE_VFGORC_MSB,
	    hw_stats->last_vfgorc, hw_stats->vfgorc);

	/* Good Tx packet, include VF loopback */
	UPDATE_VF_STAT(IXGBE_VFGPTC,
	    hw_stats->last_vfgptc, hw_stats->vfgptc);

	/* Good Tx octets, include VF loopback */
	UPDATE_VF_STAT_36BIT(IXGBE_VFGOTC_LSB, IXGBE_VFGOTC_MSB,
	    hw_stats->last_vfgotc, hw_stats->vfgotc);

	/* Rx Multicst Packet */
	UPDATE_VF_STAT(IXGBE_VFMPRC,
	    hw_stats->last_vfmprc, hw_stats->vfmprc);

	if (stats == NULL)
		return;

	memset(stats, 0, sizeof(*stats));
	stats->ipackets = hw_stats->vfgprc;
	stats->ibytes = hw_stats->vfgorc;
	stats->opackets = hw_stats->vfgptc;
	stats->obytes = hw_stats->vfgotc;
	stats->imcasts = hw_stats->vfmprc;
}

static void
ixgbevf_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct ixgbevf_hw_stats *hw_stats = (struct ixgbevf_hw_stats*)
			IXGBE_DEV_PRIVATE_TO_STATS(dev->data->dev_private);

	/* Sync HW register to the last stats */
	ixgbevf_dev_stats_get(dev, NULL);

	/* reset HW current stats*/
	hw_stats->vfgprc = 0;
	hw_stats->vfgorc = 0;
	hw_stats->vfgptc = 0;
	hw_stats->vfgotc = 0;
	hw_stats->vfmprc = 0;

}

static void
ixgbe_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	dev_info->max_rx_queues = hw->mac.max_rx_queues;
	dev_info->max_tx_queues = hw->mac.max_tx_queues;
	dev_info->min_rx_bufsize = 1024; /* cf BSIZEPACKET in SRRCTL register */
	dev_info->max_rx_pktlen = 15872; /* includes CRC, cf MAXFRS register */
	dev_info->max_mac_addrs = hw->mac.num_rar_entries;
}

/* return 0 means link status changed, -1 means not changed */
static int
ixgbe_dev_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_link link, old;
	ixgbe_link_speed link_speed;
	int link_up;
	int diag;

	link.link_status = 0;
	link.link_speed = 0;
	link.link_duplex = 0;
	memset(&old, 0, sizeof(old));
	rte_ixgbe_dev_atomic_read_link_status(dev, &old);

	/* check if it needs to wait to complete, if lsc interrupt is enabled */
	if (wait_to_complete == 0 || dev->data->dev_conf.intr_conf.lsc != 0)
		diag = ixgbe_check_link(hw, &link_speed, &link_up, 0);
	else
		diag = ixgbe_check_link(hw, &link_speed, &link_up, 1);
	if (diag != 0) {
		link.link_speed = ETH_LINK_SPEED_100;
		link.link_duplex = ETH_LINK_HALF_DUPLEX;
		rte_ixgbe_dev_atomic_write_link_status(dev, &link);
		if (link.link_status == old.link_status)
			return -1;
		return 0;
	}

	if (link_up == 0) {
		rte_ixgbe_dev_atomic_write_link_status(dev, &link);
		if (link.link_status == old.link_status)
			return -1;
		return 0;
	}
	link.link_status = 1;
	link.link_duplex = ETH_LINK_FULL_DUPLEX;

	switch (link_speed) {
	default:
	case IXGBE_LINK_SPEED_UNKNOWN:
		link.link_duplex = ETH_LINK_HALF_DUPLEX;
		link.link_speed = ETH_LINK_SPEED_100;
		break;

	case IXGBE_LINK_SPEED_100_FULL:
		link.link_speed = ETH_LINK_SPEED_100;
		break;

	case IXGBE_LINK_SPEED_1GB_FULL:
		link.link_speed = ETH_LINK_SPEED_1000;
		break;

	case IXGBE_LINK_SPEED_10GB_FULL:
		link.link_speed = ETH_LINK_SPEED_10000;
		break;
	}
	rte_ixgbe_dev_atomic_write_link_status(dev, &link);

	if (link.link_status == old.link_status)
		return -1;

	return 0;
}

static void
ixgbe_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t fctrl;

	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);
}

static void
ixgbe_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t fctrl;

	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	fctrl &= (~IXGBE_FCTRL_UPE);
	if (dev->data->all_multicast == 1)
		fctrl |= IXGBE_FCTRL_MPE;
	else
		fctrl &= (~IXGBE_FCTRL_MPE);
	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);
}

static void
ixgbe_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t fctrl;

	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	fctrl |= IXGBE_FCTRL_MPE;
	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);
}

static void
ixgbe_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t fctrl;

	if (dev->data->promiscuous == 1)
		return; /* must remain in all_multicast mode */

	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	fctrl &= (~IXGBE_FCTRL_MPE);
	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);
}

/**
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
ixgbe_dev_interrupt_setup(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	ixgbe_dev_link_status_print(dev);
	IXGBE_WRITE_REG(hw, IXGBE_EIMS, IXGBE_EICR_LSC);
	IXGBE_WRITE_FLUSH(hw);
	rte_intr_enable(&(dev->pci_dev->intr_handle));

	return 0;
}

/*
 * It reads ICR and sets flag (IXGBE_EICR_LSC) for the link_update.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
ixgbe_dev_interrupt_get_status(struct rte_eth_dev *dev)
{
	uint32_t eicr;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ixgbe_interrupt *intr =
		IXGBE_DEV_PRIVATE_TO_INTR(dev->data->dev_private);

	IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_EICR_LSC);
	IXGBE_WRITE_FLUSH(hw);

	/* read-on-clear nic registers here */
	eicr = IXGBE_READ_REG(hw, IXGBE_EICR);
	PMD_INIT_LOG(INFO, "eicr %x", eicr);
	if (eicr & IXGBE_EICR_LSC) {
		/* set flag for async link update */
		intr->flags |= IXGBE_FLAG_NEED_LINK_UPDATE;
	}

	return 0;
}

/**
 * It gets and then prints the link status.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static void
ixgbe_dev_link_status_print(struct rte_eth_dev *dev)
{
	struct rte_eth_link link;

	memset(&link, 0, sizeof(link));
	rte_ixgbe_dev_atomic_read_link_status(dev, &link);
	if (link.link_status) {
		PMD_INIT_LOG(INFO, "Port %d: Link Up - speed %u Mbps - %s",
					(int)(dev->data->port_id),
					(unsigned)link.link_speed,
			link.link_duplex == ETH_LINK_FULL_DUPLEX ?
					"full-duplex" : "half-duplex");
	} else {
		PMD_INIT_LOG(INFO, " Port %d: Link Down",
				(int)(dev->data->port_id));
	}
	PMD_INIT_LOG(INFO, "PCI Address: %04d:%02d:%02d:%d",
				dev->pci_dev->addr.domain,
				dev->pci_dev->addr.bus,
				dev->pci_dev->addr.devid,
				dev->pci_dev->addr.function);
}

/*
 * It executes link_update after knowing an interrupt occured.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  - On success, zero.
 *  - On failure, a negative value.
 */
static int
ixgbe_dev_interrupt_action(struct rte_eth_dev *dev)
{
	struct ixgbe_interrupt *intr =
		IXGBE_DEV_PRIVATE_TO_INTR(dev->data->dev_private);

	if (!(intr->flags & IXGBE_FLAG_NEED_LINK_UPDATE)) {
		return -1;
	}
	ixgbe_dev_link_update(dev, 0);

	return 0;
}

/**
 * Interrupt handler which shall be registered for alarm callback for delayed
 * handling specific interrupt to wait for the stable nic state. As the
 * NIC interrupt state is not stable for ixgbe after link is just down,
 * it needs to wait 4 seconds to get the stable status.
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
ixgbe_dev_interrupt_delayed_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct ixgbe_interrupt *intr =
		IXGBE_DEV_PRIVATE_TO_INTR(dev->data->dev_private);
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	IXGBE_READ_REG(hw, IXGBE_EICR);
	ixgbe_dev_interrupt_action(dev);
	if (intr->flags & IXGBE_FLAG_NEED_LINK_UPDATE) {
		intr->flags &= ~IXGBE_FLAG_NEED_LINK_UPDATE;
		rte_intr_enable(&(dev->pci_dev->intr_handle));
		IXGBE_WRITE_REG(hw, IXGBE_EIMS, IXGBE_EICR_LSC);
		IXGBE_WRITE_FLUSH(hw);
		ixgbe_dev_link_status_print(dev);
		_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC);
	}
}

/**
 * Interrupt handler triggered by NIC  for handling
 * specific interrupt.
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
ixgbe_dev_interrupt_handler(struct rte_intr_handle *handle, void *param)
{
	int64_t timeout;
	struct rte_eth_link link;
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct ixgbe_interrupt *intr =
		IXGBE_DEV_PRIVATE_TO_INTR(dev->data->dev_private);

	/* get the link status before link update, for predicting later */
	memset(&link, 0, sizeof(link));
	rte_ixgbe_dev_atomic_read_link_status(dev, &link);
	ixgbe_dev_interrupt_get_status(dev);
	ixgbe_dev_interrupt_action(dev);

	if (!(intr->flags & IXGBE_FLAG_NEED_LINK_UPDATE))
		return;

	/* likely to up */
	if (!link.link_status)
		/* handle it 1 sec later, wait it being stable */
		timeout = IXGBE_LINK_UP_CHECK_TIMEOUT;
	/* likely to down */
	else
		/* handle it 4 sec later, wait it being stable */
		timeout = IXGBE_LINK_DOWN_CHECK_TIMEOUT;

	ixgbe_dev_link_status_print(dev);
	if (rte_eal_alarm_set(timeout * 1000,
		ixgbe_dev_interrupt_delayed_handler, param) < 0)
		PMD_INIT_LOG(ERR, "Error setting alarm");
}

static int
ixgbe_dev_led_on(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	return (ixgbe_led_on(hw, 0) == IXGBE_SUCCESS ? 0 : -ENOTSUP);
}

static int
ixgbe_dev_led_off(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	return (ixgbe_led_off(hw, 0) == IXGBE_SUCCESS ? 0 : -ENOTSUP);
}

static int
ixgbe_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	struct ixgbe_hw *hw;
	int err;
	uint32_t rx_buf_size;
	uint32_t max_high_water;
	enum ixgbe_fc_mode rte_fcmode_2_ixgbe_fcmode[] = {
		ixgbe_fc_none,
		ixgbe_fc_rx_pause,
		ixgbe_fc_tx_pause,
		ixgbe_fc_full
	};

	PMD_INIT_FUNC_TRACE();

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	rx_buf_size = IXGBE_READ_REG(hw, IXGBE_RXPBSIZE(0));
	PMD_INIT_LOG(DEBUG, "Rx packet buffer size = 0x%x \n", rx_buf_size);

	/*
	 * At least reserve one Ethernet frame for watermark
	 * high_water/low_water in kilo bytes for ixgbe
	 */
	max_high_water = (rx_buf_size - ETHER_MAX_LEN) >> IXGBE_RXPBSIZE_SHIFT;
	if ((fc_conf->high_water > max_high_water) ||
		(fc_conf->high_water < fc_conf->low_water)) {
		PMD_INIT_LOG(ERR, "Invalid high/low water setup value in KB\n");
		PMD_INIT_LOG(ERR, "High_water must <=  0x%x\n", max_high_water);
		return (-EINVAL);
	}

	hw->fc.requested_mode = rte_fcmode_2_ixgbe_fcmode[fc_conf->mode];
	hw->fc.pause_time     = fc_conf->pause_time;
	hw->fc.high_water[0]  = fc_conf->high_water;
	hw->fc.low_water      = fc_conf->low_water;
	hw->fc.send_xon       = fc_conf->send_xon;

	err = ixgbe_fc_enable(hw, 0);
	/* Not negotiated is not an error case */
	if ((err == IXGBE_SUCCESS) || (err == IXGBE_ERR_FC_NOT_NEGOTIATED)) {
		return 0;
	}

	PMD_INIT_LOG(ERR, "ixgbe_fc_enable = 0x%x \n", err);
	return -EIO;
}

static void
ixgbe_add_rar(struct rte_eth_dev *dev, struct ether_addr *mac_addr,
				uint32_t index, uint32_t pool)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t enable_addr = 1;

	ixgbe_set_rar(hw, index, mac_addr->addr_bytes, pool, enable_addr);
}

static void
ixgbe_remove_rar(struct rte_eth_dev *dev, uint32_t index)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	ixgbe_clear_rar(hw, index);
}

/*
 * Virtual Function operations
 */
static void
ixgbevf_intr_disable(struct ixgbe_hw *hw)
{
	PMD_INIT_LOG(DEBUG, "ixgbevf_intr_disable");

	/* Clear interrupt mask to stop from interrupts being generated */
	IXGBE_WRITE_REG(hw, IXGBE_VTEIMC, IXGBE_VF_IRQ_CLEAR_MASK);

	IXGBE_WRITE_FLUSH(hw);
}

static int
ixgbevf_dev_configure(struct rte_eth_dev *dev, uint16_t nb_rx_q, uint16_t nb_tx_q)
{
	int diag;
	struct rte_eth_conf* conf = &dev->data->dev_conf;

	PMD_INIT_FUNC_TRACE();

	/* Allocate the array of pointers to RX queue structures */
	diag = ixgbe_dev_rx_queue_alloc(dev, nb_rx_q);
	if (diag != 0) {
		PMD_INIT_LOG(ERR, "ethdev port_id=%d allocation of array of %d"
			     "pointers to RX queues failed", dev->data->port_id,
			     nb_rx_q);
		return diag;
	}

	/* Allocate the array of pointers to TX queue structures */
	diag = ixgbe_dev_tx_queue_alloc(dev, nb_tx_q);
	if (diag != 0) {
		PMD_INIT_LOG(ERR, "ethdev port_id=%d allocation of array of %d"
			     "pointers to TX queues failed", dev->data->port_id,
			     nb_tx_q);
		return diag;
	}

	if (!conf->rxmode.hw_strip_crc) {
		/*
		 * VF has no ability to enable/disable HW CRC
		 * Keep the persistent behavior the same as Host PF
		 */
		PMD_INIT_LOG(INFO, "VF can't disable HW CRC Strip\n");
		conf->rxmode.hw_strip_crc = 1;
	}

	return 0;
}

static int
ixgbevf_dev_start(struct rte_eth_dev *dev)
{
	int err = 0;
	PMD_INIT_LOG(DEBUG, "ixgbevf_dev_start");

	ixgbevf_dev_tx_init(dev);
	err = ixgbevf_dev_rx_init(dev);
	if(err){
		ixgbe_dev_clear_queues(dev);
		PMD_INIT_LOG(ERR,"Unable to initialize RX hardware\n");
		return err;
	}
	ixgbevf_dev_rxtx_start(dev);

	return 0;
}

static void
ixgbevf_dev_stop(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_LOG(DEBUG, "ixgbevf_dev_stop");

	ixgbe_reset_hw(hw);
	hw->adapter_stopped = 0;
	ixgbe_stop_adapter(hw);
	/* reprogram the RAR[0] in case user changed it. */
	ixgbe_set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);
}
