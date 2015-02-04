/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2013-2015 Intel Corporation. All rights reserved.
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

#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_string_fns.h>
#include <rte_dev.h>
#include <rte_spinlock.h>

#include "fm10k.h"
#include "base/fm10k_api.h"

/* Default delay to acquire mailbox lock */
#define FM10K_MBXLOCK_DELAY_US 20

static void
fm10k_mbx_initlock(struct fm10k_hw *hw)
{
	rte_spinlock_init(FM10K_DEV_PRIVATE_TO_MBXLOCK(hw->back));
}

static void
fm10k_mbx_lock(struct fm10k_hw *hw)
{
	while (!rte_spinlock_trylock(FM10K_DEV_PRIVATE_TO_MBXLOCK(hw->back)))
		rte_delay_us(FM10K_MBXLOCK_DELAY_US);
}

static void
fm10k_mbx_unlock(struct fm10k_hw *hw)
{
	rte_spinlock_unlock(FM10K_DEV_PRIVATE_TO_MBXLOCK(hw->back));
}

static int
fm10k_dev_configure(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();

	if (dev->data->dev_conf.rxmode.hw_strip_crc == 0)
		PMD_INIT_LOG(WARNING, "fm10k always strip CRC");

	return 0;
}

static void
fm10k_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	uint64_t ipackets, opackets, ibytes, obytes;
	struct fm10k_hw *hw =
		FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct fm10k_hw_stats *hw_stats =
		FM10K_DEV_PRIVATE_TO_STATS(dev->data->dev_private);
	int i;

	PMD_INIT_FUNC_TRACE();

	fm10k_update_hw_stats(hw, hw_stats);

	ipackets = opackets = ibytes = obytes = 0;
	for (i = 0; (i < RTE_ETHDEV_QUEUE_STAT_CNTRS) &&
		(i < FM10K_MAX_QUEUES_PF); ++i) {
		stats->q_ipackets[i] = hw_stats->q[i].rx_packets.count;
		stats->q_opackets[i] = hw_stats->q[i].tx_packets.count;
		stats->q_ibytes[i]   = hw_stats->q[i].rx_bytes.count;
		stats->q_obytes[i]   = hw_stats->q[i].tx_bytes.count;
		ipackets += stats->q_ipackets[i];
		opackets += stats->q_opackets[i];
		ibytes   += stats->q_ibytes[i];
		obytes   += stats->q_obytes[i];
	}
	stats->ipackets = ipackets;
	stats->opackets = opackets;
	stats->ibytes = ibytes;
	stats->obytes = obytes;
}

static void
fm10k_stats_reset(struct rte_eth_dev *dev)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct fm10k_hw_stats *hw_stats =
		FM10K_DEV_PRIVATE_TO_STATS(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	memset(hw_stats, 0, sizeof(*hw_stats));
	fm10k_rebind_hw_stats(hw, hw_stats);
}

/* Mailbox message handler in VF */
static const struct fm10k_msg_data fm10k_msgdata_vf[] = {
	FM10K_TLV_MSG_TEST_HANDLER(fm10k_tlv_msg_test),
	FM10K_VF_MSG_MAC_VLAN_HANDLER(fm10k_msg_mac_vlan_vf),
	FM10K_VF_MSG_LPORT_STATE_HANDLER(fm10k_msg_lport_state_vf),
	FM10K_TLV_MSG_ERROR_HANDLER(fm10k_tlv_msg_error),
};

/* Mailbox message handler in PF */
static const struct fm10k_msg_data fm10k_msgdata_pf[] = {
	FM10K_PF_MSG_ERR_HANDLER(XCAST_MODES, fm10k_msg_err_pf),
	FM10K_PF_MSG_ERR_HANDLER(UPDATE_MAC_FWD_RULE, fm10k_msg_err_pf),
	FM10K_PF_MSG_LPORT_MAP_HANDLER(fm10k_msg_lport_map_pf),
	FM10K_PF_MSG_ERR_HANDLER(LPORT_CREATE, fm10k_msg_err_pf),
	FM10K_PF_MSG_ERR_HANDLER(LPORT_DELETE, fm10k_msg_err_pf),
	FM10K_PF_MSG_UPDATE_PVID_HANDLER(fm10k_msg_update_pvid_pf),
	FM10K_TLV_MSG_ERROR_HANDLER(fm10k_tlv_msg_error),
};

static int
fm10k_setup_mbx_service(struct fm10k_hw *hw)
{
	int err;

	/* Initialize mailbox lock */
	fm10k_mbx_initlock(hw);

	/* Replace default message handler with new ones */
	if (hw->mac.type == fm10k_mac_pf)
		err = hw->mbx.ops.register_handlers(&hw->mbx, fm10k_msgdata_pf);
	else
		err = hw->mbx.ops.register_handlers(&hw->mbx, fm10k_msgdata_vf);

	if (err) {
		PMD_INIT_LOG(ERR, "Failed to register mailbox handler.err:%d",
				err);
		return err;
	}
	/* Connect to SM for PF device or PF for VF device */
	return hw->mbx.ops.connect(hw, &hw->mbx);
}

static struct eth_dev_ops fm10k_eth_dev_ops = {
	.dev_configure		= fm10k_dev_configure,
	.stats_get		= fm10k_stats_get,
	.stats_reset		= fm10k_stats_reset,
};

static int
eth_fm10k_dev_init(__rte_unused struct eth_driver *eth_drv,
	struct rte_eth_dev *dev)
{
	struct fm10k_hw *hw = FM10K_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	int diag;

	PMD_INIT_FUNC_TRACE();

	dev->dev_ops = &fm10k_eth_dev_ops;

	/* only initialize in the primary process */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Vendor and Device ID need to be set before init of shared code */
	memset(hw, 0, sizeof(*hw));
	hw->device_id = dev->pci_dev->id.device_id;
	hw->vendor_id = dev->pci_dev->id.vendor_id;
	hw->subsystem_device_id = dev->pci_dev->id.subsystem_device_id;
	hw->subsystem_vendor_id = dev->pci_dev->id.subsystem_vendor_id;
	hw->revision_id = 0;
	hw->hw_addr = (void *)dev->pci_dev->mem_resource[0].addr;
	if (hw->hw_addr == NULL) {
		PMD_INIT_LOG(ERR, "Bad mem resource."
			" Try to blacklist unused devices.");
		return -EIO;
	}

	/* Store fm10k_adapter pointer */
	hw->back = dev->data->dev_private;

	/* Initialize the shared code */
	diag = fm10k_init_shared_code(hw);
	if (diag != FM10K_SUCCESS) {
		PMD_INIT_LOG(ERR, "Shared code init failed: %d", diag);
		return -EIO;
	}

	/*
	 * Inialize bus info. Normally we would call fm10k_get_bus_info(), but
	 * there is no way to get link status without reading BAR4.  Until this
	 * works, assume we have maximum bandwidth.
	 * @todo - fix bus info
	 */
	hw->bus_caps.speed = fm10k_bus_speed_8000;
	hw->bus_caps.width = fm10k_bus_width_pcie_x8;
	hw->bus_caps.payload = fm10k_bus_payload_512;
	hw->bus.speed = fm10k_bus_speed_8000;
	hw->bus.width = fm10k_bus_width_pcie_x8;
	hw->bus.payload = fm10k_bus_payload_256;

	/* Initialize the hw */
	diag = fm10k_init_hw(hw);
	if (diag != FM10K_SUCCESS) {
		PMD_INIT_LOG(ERR, "Hardware init failed: %d", diag);
		return -EIO;
	}

	/* Initialize MAC address(es) */
	dev->data->mac_addrs = rte_zmalloc("fm10k", ETHER_ADDR_LEN, 0);
	if (dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate memory for MAC addresses");
		return -ENOMEM;
	}

	diag = fm10k_read_mac_addr(hw);
	if (diag != FM10K_SUCCESS) {
		/*
		 * TODO: remove special handling on VF. Need shared code to
		 * fix first.
		 */
		if (hw->mac.type == fm10k_mac_pf) {
			PMD_INIT_LOG(ERR, "Read MAC addr failed: %d", diag);
			return -EIO;
		} else {
			/* Generate a random addr */
			eth_random_addr(hw->mac.addr);
			memcpy(hw->mac.perm_addr, hw->mac.addr, ETH_ALEN);
		}
	}

	ether_addr_copy((const struct ether_addr *)hw->mac.addr,
			&dev->data->mac_addrs[0]);

	/* Reset the hw statistics */
	fm10k_stats_reset(dev);

	/* Reset the hw */
	diag = fm10k_reset_hw(hw);
	if (diag != FM10K_SUCCESS) {
		PMD_INIT_LOG(ERR, "Hardware reset failed: %d", diag);
		return -EIO;
	}

	/* Setup mailbox service */
	diag = fm10k_setup_mbx_service(hw);
	if (diag != FM10K_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to setup mailbox: %d", diag);
		return -EIO;
	}

	/*
	 * Below function will trigger operations on mailbox, acquire lock to
	 * avoid race condition from interrupt handler. Operations on mailbox
	 * FIFO will trigger interrupt to PF/SM, in which interrupt handler
	 * will handle and generate an interrupt to our side. Then,  FIFO in
	 * mailbox will be touched.
	 */
	fm10k_mbx_lock(hw);
	/* Enable port first */
	hw->mac.ops.update_lport_state(hw, 0, 0, 1);

	/* Update default vlan */
	hw->mac.ops.update_vlan(hw, hw->mac.default_vid, 0, true);

	/*
	 * Add default mac/vlan filter. glort is assigned by SM for PF, while is
	 * unused for VF. PF will assign correct glort for VF.
	 */
	hw->mac.ops.update_uc_addr(hw, hw->mac.dglort_map, hw->mac.addr,
			      hw->mac.default_vid, 1, 0);

	/* Set unicast mode by default. App can change to other mode in other
	 * API func.
	 */
	hw->mac.ops.update_xcast_mode(hw, hw->mac.dglort_map,
					FM10K_XCAST_MODE_MULTI);

	fm10k_mbx_unlock(hw);

	return 0;
}

/*
 * The set of PCI devices this driver supports. This driver will enable both PF
 * and SRIOV-VF devices.
 */
static struct rte_pci_id pci_id_fm10k_map[] = {
#define RTE_PCI_DEV_ID_DECL_FM10K(vend, dev) { RTE_PCI_DEVICE(vend, dev) },
#include "rte_pci_dev_ids.h"
	{ .vendor_id = 0, /* sentinel */ },
};

static struct eth_driver rte_pmd_fm10k = {
	{
		.name = "rte_pmd_fm10k",
		.id_table = pci_id_fm10k_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	},
	.eth_dev_init = eth_fm10k_dev_init,
	.dev_private_size = sizeof(struct fm10k_adapter),
};

/*
 * Driver initialization routine.
 * Invoked once at EAL init time.
 * Register itself as the [Poll Mode] Driver of PCI FM10K devices.
 */
static int
rte_pmd_fm10k_init(__rte_unused const char *name,
	__rte_unused const char *params)
{
	PMD_INIT_FUNC_TRACE();
	rte_eth_driver_register(&rte_pmd_fm10k);
	return 0;
}

static struct rte_driver rte_fm10k_driver = {
	.type = PMD_PDEV,
	.init = rte_pmd_fm10k_init,
};

PMD_REGISTER_DRIVER(rte_fm10k_driver);
