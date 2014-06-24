/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
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
#include <rte_string_fns.h>
#include <rte_malloc.h>
#include <rte_dev.h>

#include "vmxnet3/vmxnet3_defs.h"

#include "vmxnet3_ring.h"
#include "vmxnet3_logs.h"
#include "vmxnet3_ethdev.h"

#define PROCESS_SYS_EVENTS 0

static int eth_vmxnet3_dev_init(struct eth_driver *eth_drv,
		struct rte_eth_dev *eth_dev);
static int vmxnet3_dev_configure(struct rte_eth_dev *dev);
static int vmxnet3_dev_start(struct rte_eth_dev *dev);
static void vmxnet3_dev_stop(struct rte_eth_dev *dev);
static void vmxnet3_dev_close(struct rte_eth_dev *dev);
static void vmxnet3_dev_set_rxmode(struct vmxnet3_hw *hw, uint32_t feature, int set);
static void vmxnet3_dev_promiscuous_enable(struct rte_eth_dev *dev);
static void vmxnet3_dev_promiscuous_disable(struct rte_eth_dev *dev);
static void vmxnet3_dev_allmulticast_enable(struct rte_eth_dev *dev);
static void vmxnet3_dev_allmulticast_disable(struct rte_eth_dev *dev);
static int vmxnet3_dev_link_update(struct rte_eth_dev *dev,
				int wait_to_complete);
static void vmxnet3_dev_stats_get(struct rte_eth_dev *dev,
				struct rte_eth_stats *stats);
static void vmxnet3_dev_info_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);
#if PROCESS_SYS_EVENTS == 1
static void vmxnet3_process_events(struct vmxnet3_hw *);
#endif
/*
 * The set of PCI devices this driver supports
 */
static struct rte_pci_id pci_id_vmxnet3_map[] = {

#define RTE_PCI_DEV_ID_DECL_VMXNET3(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#include "rte_pci_dev_ids.h"

{ .vendor_id = 0, /* sentinel */ },
};

static struct eth_dev_ops vmxnet3_eth_dev_ops = {
	.dev_configure        = vmxnet3_dev_configure,
	.dev_start            = vmxnet3_dev_start,
	.dev_stop             = vmxnet3_dev_stop,
	.dev_close            = vmxnet3_dev_close,
	.promiscuous_enable   = vmxnet3_dev_promiscuous_enable,
	.promiscuous_disable  = vmxnet3_dev_promiscuous_disable,
	.allmulticast_enable  = vmxnet3_dev_allmulticast_enable,
	.allmulticast_disable = vmxnet3_dev_allmulticast_disable,
	.link_update          = vmxnet3_dev_link_update,
	.stats_get            = vmxnet3_dev_stats_get,
	.dev_infos_get        = vmxnet3_dev_info_get,
	.rx_queue_setup       = vmxnet3_dev_rx_queue_setup,
	.rx_queue_release     = vmxnet3_dev_rx_queue_release,
	.tx_queue_setup       = vmxnet3_dev_tx_queue_setup,
	.tx_queue_release     = vmxnet3_dev_tx_queue_release,
};

static const struct rte_memzone *
gpa_zone_reserve(struct rte_eth_dev *dev, uint32_t size,
		const char *post_string, int socket_id, uint16_t align)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	snprintf(z_name, sizeof(z_name), "%s_%d_%s",
					dev->driver->pci_drv.name, dev->data->port_id, post_string);

	mz = rte_memzone_lookup(z_name);
	if (mz)
		return mz;

	return rte_memzone_reserve_aligned(z_name, size,
			socket_id, 0, align);
}

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
rte_vmxnet3_dev_atomic_write_link_status(struct rte_eth_dev *dev,
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
 * This function is based on vmxnet3_disable_intr()
 */
static void
vmxnet3_disable_intr(struct vmxnet3_hw *hw)
{
	int i;

	PMD_INIT_FUNC_TRACE();

	hw->shared->devRead.intrConf.intrCtrl |= VMXNET3_IC_DISABLE_ALL;
	for (i = 0; i < VMXNET3_MAX_INTRS; i++)
			VMXNET3_WRITE_BAR0_REG(hw, VMXNET3_REG_IMR + i * 8, 1);
}

/*
 * It returns 0 on success.
 */
static int
eth_vmxnet3_dev_init(__attribute__((unused)) struct eth_driver *eth_drv,
		     struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct vmxnet3_hw *hw =
		VMXNET3_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);
	uint32_t mac_hi, mac_lo, ver;

	PMD_INIT_FUNC_TRACE();

	eth_dev->dev_ops = &vmxnet3_eth_dev_ops;
	eth_dev->rx_pkt_burst = &vmxnet3_recv_pkts;
	eth_dev->tx_pkt_burst = &vmxnet3_xmit_pkts;
	pci_dev = eth_dev->pci_dev;

	/*
	 * for secondary processes, we don't initialise any further as primary
	 * has already done this work.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Vendor and Device ID need to be set before init of shared code */
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->hw_addr0 = (void *)pci_dev->mem_resource[0].addr;
	hw->hw_addr1 = (void *)pci_dev->mem_resource[1].addr;

	hw->num_rx_queues = 1;
	hw->num_tx_queues = 1;
	hw->cur_mtu = ETHER_MTU;
	hw->bufs_per_pkt = 1;

	/* Check h/w version compatibility with driver. */
    ver = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_VRRS);
    PMD_INIT_LOG(DEBUG, "Harware version : %d\n", ver);
    if (ver & 0x1)
		VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_VRRS, 1);
    else {
		PMD_INIT_LOG(ERR, "Uncompatiable h/w version, should be 0x1\n");
		return -EIO;
    }

    /* Check UPT version compatibility with driver. */
    ver = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_UVRS);
    PMD_INIT_LOG(DEBUG, "UPT harware version : %d\n", ver);
    if (ver & 0x1)
		VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_UVRS, 1);
    else {
		PMD_INIT_LOG(ERR, "Incompatiable UPT version.\n");
		return -EIO;
    }

	/* Getting MAC Address */
	mac_lo = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_MACL);
	mac_hi = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_MACH);
	memcpy(hw->perm_addr  , &mac_lo, 4);
	memcpy(hw->perm_addr+4, &mac_hi, 2);

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("vmxnet3", ETHER_ADDR_LEN *
			VMXNET3_MAX_MAC_ADDRS, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
			"Failed to allocate %d bytes needed to store MAC addresses",
			ETHER_ADDR_LEN * VMXNET3_MAX_MAC_ADDRS);
		return -ENOMEM;
	}
	/* Copy the permanent MAC address */
	ether_addr_copy((struct ether_addr *) hw->perm_addr,
			&eth_dev->data->mac_addrs[0]);

	PMD_INIT_LOG(DEBUG, "MAC Address : %02x:%02x:%02x:%02x:%02x:%02x \n",
	               hw->perm_addr[0], hw->perm_addr[1], hw->perm_addr[2],
	               hw->perm_addr[3], hw->perm_addr[4], hw->perm_addr[5]);

	/* Put device in Quiesce Mode */
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_QUIESCE_DEV);

	return 0;
}

static struct eth_driver rte_vmxnet3_pmd = {
	{
		.name = "rte_vmxnet3_pmd",
		.id_table = pci_id_vmxnet3_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	},
	.eth_dev_init = eth_vmxnet3_dev_init,
	.dev_private_size = sizeof(struct vmxnet3_adapter),
};

/*
 * Driver initialization routine.
 * Invoked once at EAL init time.
 * Register itself as the [Poll Mode] Driver of Virtual PCI VMXNET3 devices.
 */
static int
rte_vmxnet3_pmd_init(const char *name __rte_unused, const char *param __rte_unused)
{
	PMD_INIT_FUNC_TRACE();

	rte_eth_driver_register(&rte_vmxnet3_pmd);
	return 0;
}

static int
vmxnet3_dev_configure(struct rte_eth_dev *dev)
{
	const struct rte_memzone *mz;
	struct vmxnet3_hw *hw =
			VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	size_t size;

	PMD_INIT_FUNC_TRACE();

	if (dev->data->nb_rx_queues > UINT8_MAX ||
			dev->data->nb_tx_queues > UINT8_MAX)
		return (-EINVAL);

	size = dev->data->nb_rx_queues * sizeof(struct Vmxnet3_TxQueueDesc) +
	          dev->data->nb_tx_queues * sizeof (struct Vmxnet3_RxQueueDesc);

	if (size > UINT16_MAX)
		return (-EINVAL);

	hw->num_rx_queues = (uint8_t)dev->data->nb_rx_queues;
	hw->num_tx_queues = (uint8_t)dev->data->nb_tx_queues;

	/*
	 * Allocate a memzone for Vmxnet3_DriverShared - Vmxnet3_DSDevRead
	 * on current socket
	 */
	mz = gpa_zone_reserve(dev, sizeof (struct Vmxnet3_DriverShared),
		"shared", rte_socket_id(), 8);

	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "ERROR: Creating shared zone\n");
		return (-ENOMEM);
	}
	memset(mz->addr, 0, mz->len);

	hw->shared = mz->addr;
	hw->sharedPA = mz->phys_addr;

	/*
	* Allocate a memzone for Vmxnet3_RxQueueDesc - Vmxnet3_TxQueueDesc
	* on current socket
	*/
	mz = gpa_zone_reserve(dev, size, "queuedesc",
					rte_socket_id(), VMXNET3_QUEUE_DESC_ALIGN);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "ERROR: Creating queue descriptors zone\n");
		return (-ENOMEM);
	}
	memset(mz->addr, 0, mz->len);

	hw->tqd_start = (Vmxnet3_TxQueueDesc *)mz->addr;
	hw->rqd_start = (Vmxnet3_RxQueueDesc *)(hw->tqd_start + hw->num_tx_queues);

	hw->queueDescPA = mz->phys_addr;
	hw->queue_desc_len = (uint16_t)size;

	if(dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_RSS) {

		/* Allocate memory structure for UPT1_RSSConf and configure */
		mz = gpa_zone_reserve(dev, sizeof (struct VMXNET3_RSSConf), "rss_conf",
				rte_socket_id(), CACHE_LINE_SIZE);
		if (mz == NULL) {
			PMD_INIT_LOG(ERR, "ERROR: Creating rss_conf structure zone\n");
			return (-ENOMEM);
		}
		memset(mz->addr, 0, mz->len);

		hw->rss_conf = mz->addr;
		hw->rss_confPA = mz->phys_addr;
	}

	return 0;
}

static int
vmxnet3_setup_driver_shared(struct rte_eth_dev *dev)
{
	struct rte_eth_conf port_conf = dev->data->dev_conf;
	struct vmxnet3_hw *hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	Vmxnet3_DriverShared *shared = hw->shared;
	Vmxnet3_DSDevRead *devRead = &shared->devRead;
	uint32_t *mac_ptr;
	uint32_t val, i;
	int ret;

	shared->magic = VMXNET3_REV1_MAGIC;
	devRead->misc.driverInfo.version = VMXNET3_DRIVER_VERSION_NUM;

	/* Setting up Guest OS information */
	devRead->misc.driverInfo.gos.gosBits   = sizeof(void *) == 4 ?
											VMXNET3_GOS_BITS_32 :
											VMXNET3_GOS_BITS_64;
	devRead->misc.driverInfo.gos.gosType   = VMXNET3_GOS_TYPE_LINUX;
	devRead->misc.driverInfo.vmxnet3RevSpt = 1;
	devRead->misc.driverInfo.uptVerSpt     = 1;

	devRead->misc.queueDescPA  = hw->queueDescPA;
	devRead->misc.queueDescLen = hw->queue_desc_len;
	devRead->misc.mtu          = hw->cur_mtu;
	devRead->misc.numTxQueues  = hw->num_tx_queues;
	devRead->misc.numRxQueues  = hw->num_rx_queues;

	/*
	* Set number of interrupts to 1
	* PMD disables all the interrupts but this is MUST to activate device
	* It needs at least one interrupt for link events to handle
	* So we'll disable it later after device activation if needed
	*/
	devRead->intrConf.numIntrs = 1;
	devRead->intrConf.intrCtrl |= VMXNET3_IC_DISABLE_ALL;

	for (i = 0; i < hw->num_tx_queues; i++) {
		Vmxnet3_TxQueueDesc *tqd = &hw->tqd_start[i];
		vmxnet3_tx_queue_t *txq   = dev->data->tx_queues[i];

		tqd->ctrl.txNumDeferred  = 0;
		tqd->ctrl.txThreshold    = 1;
		tqd->conf.txRingBasePA   = txq->cmd_ring.basePA;
		tqd->conf.compRingBasePA = txq->comp_ring.basePA;

		tqd->conf.txRingSize   = txq->cmd_ring.size;
		tqd->conf.compRingSize = txq->comp_ring.size;
		tqd->conf.intrIdx      = txq->comp_ring.intr_idx;
		tqd->status.stopped    = TRUE;
		tqd->status.error      = 0;
		memset(&tqd->stats, 0, sizeof(tqd->stats));
	}

	for (i = 0; i < hw->num_rx_queues; i++) {
		Vmxnet3_RxQueueDesc *rqd  = &hw->rqd_start[i];
		vmxnet3_rx_queue_t *rxq    = dev->data->rx_queues[i];

		rqd->conf.rxRingBasePA[0] = rxq->cmd_ring[0].basePA;
		rqd->conf.rxRingBasePA[1] = rxq->cmd_ring[1].basePA;
		rqd->conf.compRingBasePA  = rxq->comp_ring.basePA;

		rqd->conf.rxRingSize[0]   = rxq->cmd_ring[0].size;
		rqd->conf.rxRingSize[1]   = rxq->cmd_ring[1].size;
		rqd->conf.compRingSize    = rxq->comp_ring.size;
		rqd->conf.intrIdx         = rxq->comp_ring.intr_idx;
		rqd->status.stopped       = TRUE;
		rqd->status.error         = 0;
		memset(&rqd->stats, 0, sizeof(rqd->stats));
	}

	/* RxMode set to 0 of VMXNET3_RXM_xxx */
	devRead->rxFilterConf.rxMode = 0;

	/* Setting up feature flags */
	if(dev->data->dev_conf.rxmode.hw_ip_checksum) {
		devRead->misc.uptFeatures |= VMXNET3_F_RXCSUM;
	}

	if(dev->data->dev_conf.rxmode.hw_vlan_strip) {
		devRead->misc.uptFeatures |= VMXNET3_F_RXVLAN;
	}

	if(port_conf.rxmode.mq_mode == ETH_MQ_RX_RSS) {
		ret = vmxnet3_rss_configure(dev);
		if(ret != VMXNET3_SUCCESS) {
			return ret;
		}
		devRead->misc.uptFeatures |= VMXNET3_F_RSS;
		devRead->rssConfDesc.confVer = 1;
		devRead->rssConfDesc.confLen = sizeof(struct VMXNET3_RSSConf);
		devRead->rssConfDesc.confPA  = hw->rss_confPA;
	}

	if(dev->data->dev_conf.rxmode.hw_vlan_filter) {
		ret = vmxnet3_vlan_configure(dev);
		if(ret != VMXNET3_SUCCESS) {
			return ret;
		}
	}

	PMD_INIT_LOG(DEBUG, "Writing MAC Address : %02x:%02x:%02x:%02x:%02x:%02x \n",
					hw->perm_addr[0], hw->perm_addr[1], hw->perm_addr[2],
					hw->perm_addr[3], hw->perm_addr[4], hw->perm_addr[5]);

	/* Write MAC Address back to device */
	mac_ptr = (uint32_t *)hw->perm_addr;
	val = *mac_ptr;
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_MACL, val);

	val = (hw->perm_addr[5] << 8) | hw->perm_addr[4];
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_MACH, val);

	return VMXNET3_SUCCESS;
}

/*
 * Configure device link speed and setup link.
 * Must be called after eth_vmxnet3_dev_init. Other wise it might fail
 * It returns 0 on success.
 */
static int
vmxnet3_dev_start(struct rte_eth_dev *dev)
{
	int status, ret;
	struct vmxnet3_hw *hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	ret = vmxnet3_setup_driver_shared(dev);
	if(ret != VMXNET3_SUCCESS) {
		return ret;
	}

	/* Exchange shared data with device */
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_DSAL, VMXNET3_GET_ADDR_LO(
	                      hw->sharedPA));
    VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_DSAH, VMXNET3_GET_ADDR_HI(
						  hw->sharedPA));

    /* Activate device by register write */
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_ACTIVATE_DEV);
	status = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_CMD);

	if (status != 0) {
		PMD_INIT_LOG(ERR, "Device activation in %s(): UNSUCCESSFUL\n", __func__);
		return -1;
	}

	/* Disable interrupts */
	vmxnet3_disable_intr(hw);

	/*
	 * Load RX queues with blank mbufs and update next2fill index for device
	 * Update RxMode of the device
	 */
	ret = vmxnet3_dev_rxtx_init(dev);
	if(ret != VMXNET3_SUCCESS) {
		PMD_INIT_LOG(ERR, "Device receive init in %s: UNSUCCESSFUL\n", __func__);
		return ret;
	}

	/* Setting proper Rx Mode and issue Rx Mode Update command */
	vmxnet3_dev_set_rxmode(hw, VMXNET3_RXM_UCAST | VMXNET3_RXM_ALL_MULTI, 1);

	/*
	 * Don't need to handle events for now
	 */
#if PROCESS_SYS_EVENTS == 1
	events = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_ECR);
	PMD_INIT_LOG(DEBUG, "Reading events: 0x%X\n\n", events);
	vmxnet3_process_events(hw);
#endif
	return status;
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static void
vmxnet3_dev_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_link link;
	struct vmxnet3_hw *hw =
		VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	if(hw->adapter_stopped == TRUE) {
		PMD_INIT_LOG(DEBUG, "Device already closed.\n");
		return;
	}

	/* disable interrupts */
	vmxnet3_disable_intr(hw);

	/* quiesce the device first */
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_QUIESCE_DEV);
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_DSAL, 0);
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_DSAH, 0);

	/* reset the device */
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_RESET_DEV);
	PMD_INIT_LOG(DEBUG, "Device reset.\n");
	hw->adapter_stopped = FALSE;

	vmxnet3_dev_clear_queues(dev);

	/* Clear recorded link status */
	memset(&link, 0, sizeof(link));
	rte_vmxnet3_dev_atomic_write_link_status(dev, &link);
}

/*
 * Reset and stop device.
 */
static void
vmxnet3_dev_close(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw =
		VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	vmxnet3_dev_stop(dev);
	hw->adapter_stopped = TRUE;

}

static void
vmxnet3_dev_stats_get( struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	unsigned int i;
	struct vmxnet3_hw *hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_GET_STATS);

	stats->opackets = 0;
	stats->obytes = 0;
	stats->oerrors = 0;
	stats->ipackets = 0;
	stats->ibytes = 0;
	stats->rx_nombuf = 0;
	stats->ierrors = 0;
	stats->imcasts  = 0;
	stats->fdirmatch = 0;
	stats->fdirmiss = 0;

	for (i = 0; i < hw->num_tx_queues; i++) {
		stats->opackets += hw->tqd_start[i].stats.ucastPktsTxOK +
				hw->tqd_start[i].stats.mcastPktsTxOK +
				hw->tqd_start[i].stats.bcastPktsTxOK;
		stats->obytes   += hw->tqd_start[i].stats.ucastBytesTxOK +
				hw->tqd_start[i].stats.mcastBytesTxOK +
				hw->tqd_start[i].stats.bcastBytesTxOK;
		stats->oerrors  += hw->tqd_start[i].stats.pktsTxError +
				hw->tqd_start[i].stats.pktsTxDiscard;
	}

	for (i = 0; i < hw->num_rx_queues; i++) {
		stats->ipackets  += hw->rqd_start[i].stats.ucastPktsRxOK +
				hw->rqd_start[i].stats.mcastPktsRxOK +
				hw->rqd_start[i].stats.bcastPktsRxOK;
		stats->ibytes    += hw->rqd_start[i].stats.ucastBytesRxOK +
				hw->rqd_start[i].stats.mcastBytesRxOK +
				hw->rqd_start[i].stats.bcastBytesRxOK;
		stats->rx_nombuf += hw->rqd_start[i].stats.pktsRxOutOfBuf;
		stats->ierrors   += hw->rqd_start[i].stats.pktsRxError;
	}

}

static void
vmxnet3_dev_info_get(__attribute__((unused))struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	dev_info->max_rx_queues = VMXNET3_MAX_RX_QUEUES;
	dev_info->max_tx_queues = VMXNET3_MAX_TX_QUEUES;
	dev_info->min_rx_bufsize = 1518 + RTE_PKTMBUF_HEADROOM;
	dev_info->max_rx_pktlen = 16384; /* includes CRC, cf MAXFRS register */
	dev_info->max_mac_addrs = VMXNET3_MAX_MAC_ADDRS;
}

/* return 0 means link status changed, -1 means not changed */
static int
vmxnet3_dev_link_update(struct rte_eth_dev *dev, __attribute__((unused)) int wait_to_complete)
{
	struct vmxnet3_hw *hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_link link;
	uint32_t ret;

	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_GET_LINK);
	ret = VMXNET3_READ_BAR1_REG(hw, VMXNET3_REG_CMD);

	if (!ret) {
		PMD_INIT_LOG(ERR, "Link Status Negative : %s()\n", __func__);
		return -1;
	}

	if (ret & 0x1) {
		link.link_status = 1;
		link.link_duplex = ETH_LINK_FULL_DUPLEX;
		link.link_speed = ETH_LINK_SPEED_10000;

		rte_vmxnet3_dev_atomic_write_link_status(dev, &link);

		return 0;
	}

	return -1;
}

/* Updating rxmode through Vmxnet3_DriverShared structure in adapter */
static void
vmxnet3_dev_set_rxmode(struct vmxnet3_hw *hw, uint32_t feature, int set) {

	struct Vmxnet3_RxFilterConf *rxConf = &hw->shared->devRead.rxFilterConf;
	if(set)
		rxConf->rxMode = rxConf->rxMode | feature;
	else
		rxConf->rxMode = rxConf->rxMode & (~feature);

	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_UPDATE_RX_MODE);
}

/* Promiscuous supported only if Vmxnet3_DriverShared is initialized in adapter */
static void
vmxnet3_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	vmxnet3_dev_set_rxmode(hw, VMXNET3_RXM_PROMISC, 1);
}

/* Promiscuous supported only if Vmxnet3_DriverShared is initialized in adapter */
static void
vmxnet3_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	vmxnet3_dev_set_rxmode(hw, VMXNET3_RXM_PROMISC, 0);
}

/* Allmulticast supported only if Vmxnet3_DriverShared is initialized in adapter */
static void
vmxnet3_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	vmxnet3_dev_set_rxmode(hw, VMXNET3_RXM_PROMISC, 1);
}

/* Allmulticast supported only if Vmxnet3_DriverShared is initialized in adapter */
static void
vmxnet3_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct vmxnet3_hw *hw = VMXNET3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	vmxnet3_dev_set_rxmode(hw, VMXNET3_RXM_PROMISC, 0);
}

#if PROCESS_SYS_EVENTS == 1
static void
vmxnet3_process_events(struct vmxnet3_hw *hw)
{
	uint32_t events = hw->shared->ecr;
	if (!events){
		PMD_INIT_LOG(ERR, "No events to process in %s()\n", __func__);
		return;
	}

	/*
	* ECR bits when written with 1b are cleared. Hence write
	* events back to ECR so that the bits which were set will be reset.
	*/
	VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_ECR, events);

	/* Check if link state has changed */
   if (events & VMXNET3_ECR_LINK){
	   PMD_INIT_LOG(ERR, "Process events in %s(): VMXNET3_ECR_LINK event\n", __func__);
   }

	/* Check if there is an error on xmit/recv queues */
	if (events & (VMXNET3_ECR_TQERR | VMXNET3_ECR_RQERR)) {
		VMXNET3_WRITE_BAR1_REG(hw, VMXNET3_REG_CMD, VMXNET3_CMD_GET_QUEUE_STATUS);

		if (hw->tqd_start->status.stopped)
			PMD_INIT_LOG(ERR, "tq error 0x%x\n",
                     hw->tqd_start->status.error);

		if (hw->rqd_start->status.stopped)
			PMD_INIT_LOG(ERR, "rq error 0x%x\n",
                     hw->rqd_start->status.error);

      /* Reset the device */
      /* Have to reset the device */
	}

	if (events & VMXNET3_ECR_DIC)
		PMD_INIT_LOG(ERR, "Device implementation change event.\n");

	if (events & VMXNET3_ECR_DEBUG)
		PMD_INIT_LOG(ERR, "Debug event generated by device.\n");

}
#endif

static struct rte_driver rte_vmxnet3_driver = {
	.type = PMD_PDEV,
	.init = rte_vmxnet3_pmd_init,
};

PMD_REGISTER_DRIVER(rte_vmxnet3_driver);
