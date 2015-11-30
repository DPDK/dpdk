/*
 * Copyright (c) 2014, 2015 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_net.c
 *
 * Netronome vNIC DPDK Poll-Mode Driver: Main entry point
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/io.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <inttypes.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_dev.h>
#include <rte_ether.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_mempool.h>
#include <rte_version.h>
#include <rte_string_fns.h>
#include <rte_alarm.h>

#include "nfp_net_pmd.h"
#include "nfp_net_logs.h"
#include "nfp_net_ctrl.h"

/* Prototypes */
static void nfp_net_close(struct rte_eth_dev *dev);
static int nfp_net_configure(struct rte_eth_dev *dev);
static int nfp_net_init(struct rte_eth_dev *eth_dev);
static int nfp_net_start(struct rte_eth_dev *dev);
static void nfp_net_stop(struct rte_eth_dev *dev);

/*
 * The offset of the queue controller queues in the PCIe Target. These
 * happen to be at the same offset on the NFP6000 and the NFP3200 so
 * we use a single macro here.
 */
#define NFP_PCIE_QUEUE(_q)	(0x80000 + (0x800 * ((_q) & 0xff)))

/* Maximum value which can be added to a queue with one transaction */
#define NFP_QCP_MAX_ADD	0x7f

#define RTE_MBUF_DMA_ADDR_DEFAULT(mb) \
	(uint64_t)((mb)->buf_physaddr + RTE_PKTMBUF_HEADROOM)

/* nfp_qcp_ptr - Read or Write Pointer of a queue */
enum nfp_qcp_ptr {
	NFP_QCP_READ_PTR = 0,
	NFP_QCP_WRITE_PTR
};

/*
 * nfp_qcp_ptr_add - Add the value to the selected pointer of a queue
 * @q: Base address for queue structure
 * @ptr: Add to the Read or Write pointer
 * @val: Value to add to the queue pointer
 *
 * If @val is greater than @NFP_QCP_MAX_ADD multiple writes are performed.
 */
static inline void
nfp_qcp_ptr_add(uint8_t *q, enum nfp_qcp_ptr ptr, uint32_t val)
{
	uint32_t off;

	if (ptr == NFP_QCP_READ_PTR)
		off = NFP_QCP_QUEUE_ADD_RPTR;
	else
		off = NFP_QCP_QUEUE_ADD_WPTR;

	while (val > NFP_QCP_MAX_ADD) {
		nn_writel(rte_cpu_to_le_32(NFP_QCP_MAX_ADD), q + off);
		val -= NFP_QCP_MAX_ADD;
	}

	nn_writel(rte_cpu_to_le_32(val), q + off);
}

/*
 * nfp_qcp_read - Read the current Read/Write pointer value for a queue
 * @q:  Base address for queue structure
 * @ptr: Read or Write pointer
 */
static inline uint32_t
nfp_qcp_read(uint8_t *q, enum nfp_qcp_ptr ptr)
{
	uint32_t off;
	uint32_t val;

	if (ptr == NFP_QCP_READ_PTR)
		off = NFP_QCP_QUEUE_STS_LO;
	else
		off = NFP_QCP_QUEUE_STS_HI;

	val = rte_cpu_to_le_32(nn_readl(q + off));

	if (ptr == NFP_QCP_READ_PTR)
		return val & NFP_QCP_QUEUE_STS_LO_READPTR_mask;
	else
		return val & NFP_QCP_QUEUE_STS_HI_WRITEPTR_mask;
}

/*
 * Functions to read/write from/to Config BAR
 * Performs any endian conversion necessary.
 */
static inline uint8_t
nn_cfg_readb(struct nfp_net_hw *hw, int off)
{
	return nn_readb(hw->ctrl_bar + off);
}

static inline void
nn_cfg_writeb(struct nfp_net_hw *hw, int off, uint8_t val)
{
	nn_writeb(val, hw->ctrl_bar + off);
}

static inline uint32_t
nn_cfg_readl(struct nfp_net_hw *hw, int off)
{
	return rte_le_to_cpu_32(nn_readl(hw->ctrl_bar + off));
}

static inline void
nn_cfg_writel(struct nfp_net_hw *hw, int off, uint32_t val)
{
	nn_writel(rte_cpu_to_le_32(val), hw->ctrl_bar + off);
}

static inline uint64_t
nn_cfg_readq(struct nfp_net_hw *hw, int off)
{
	return rte_le_to_cpu_64(nn_readq(hw->ctrl_bar + off));
}

static inline void
nn_cfg_writeq(struct nfp_net_hw *hw, int off, uint64_t val)
{
	nn_writeq(rte_cpu_to_le_64(val), hw->ctrl_bar + off);
}

static int
__nfp_net_reconfig(struct nfp_net_hw *hw, uint32_t update)
{
	int cnt;
	uint32_t new;
	struct timespec wait;

	PMD_DRV_LOG(DEBUG, "Writing to the configuration queue (%p)...\n",
		    hw->qcp_cfg);

	if (hw->qcp_cfg == NULL)
		rte_panic("Bad configuration queue pointer\n");

	nfp_qcp_ptr_add(hw->qcp_cfg, NFP_QCP_WRITE_PTR, 1);

	wait.tv_sec = 0;
	wait.tv_nsec = 1000000;

	PMD_DRV_LOG(DEBUG, "Polling for update ack...\n");

	/* Poll update field, waiting for NFP to ack the config */
	for (cnt = 0; ; cnt++) {
		new = nn_cfg_readl(hw, NFP_NET_CFG_UPDATE);
		if (new == 0)
			break;
		if (new & NFP_NET_CFG_UPDATE_ERR) {
			PMD_INIT_LOG(ERR, "Reconfig error: 0x%08x\n", new);
			return -1;
		}
		if (cnt >= NFP_NET_POLL_TIMEOUT) {
			PMD_INIT_LOG(ERR, "Reconfig timeout for 0x%08x after"
					  " %dms\n", update, cnt);
			rte_panic("Exiting\n");
		}
		nanosleep(&wait, 0); /* waiting for a 1ms */
	}
	PMD_DRV_LOG(DEBUG, "Ack DONE\n");
	return 0;
}

/*
 * Reconfigure the NIC
 * @nn:    device to reconfigure
 * @ctrl:    The value for the ctrl field in the BAR config
 * @update:  The value for the update field in the BAR config
 *
 * Write the update word to the BAR and ping the reconfig queue. Then poll
 * until the firmware has acknowledged the update by zeroing the update word.
 */
static int
nfp_net_reconfig(struct nfp_net_hw *hw, uint32_t ctrl, uint32_t update)
{
	uint32_t err;

	PMD_DRV_LOG(DEBUG, "nfp_net_reconfig: ctrl=%08x update=%08x\n",
		    ctrl, update);

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, ctrl);
	nn_cfg_writel(hw, NFP_NET_CFG_UPDATE, update);

	rte_wmb();

	err = __nfp_net_reconfig(hw, update);

	if (!err)
		return 0;

	/*
	 * Reconfig errors imply situations where they can be handled.
	 * Otherwise, rte_panic is called inside __nfp_net_reconfig
	 */
	PMD_INIT_LOG(ERR, "Error nfp_net reconfig for ctrl: %x update: %x\n",
		     ctrl, update);
	return -EIO;
}

/*
 * Configure an Ethernet device. This function must be invoked first
 * before any other function in the Ethernet API. This function can
 * also be re-invoked when a device is in the stopped state.
 */
static int
nfp_net_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rxmode *rxmode;
	struct rte_eth_txmode *txmode;
	uint32_t new_ctrl = 0;
	uint32_t update = 0;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * A DPDK app sends info about how many queues to use and how
	 * those queues need to be configured. This is used by the
	 * DPDK core and it makes sure no more queues than those
	 * advertised by the driver are requested. This function is
	 * called after that internal process
	 */

	PMD_INIT_LOG(DEBUG, "Configure\n");

	dev_conf = &dev->data->dev_conf;
	rxmode = &dev_conf->rxmode;
	txmode = &dev_conf->txmode;

	/* Checking TX mode */
	if (txmode->mq_mode) {
		PMD_INIT_LOG(INFO, "TX mq_mode DCB and VMDq not supported\n");
		return -EINVAL;
	}

	/* Checking RX mode */
	if (rxmode->mq_mode & ETH_MQ_RX_RSS) {
		if (hw->cap & NFP_NET_CFG_CTRL_RSS) {
			update = NFP_NET_CFG_UPDATE_RSS;
			new_ctrl = NFP_NET_CFG_CTRL_RSS;
		} else {
			PMD_INIT_LOG(INFO, "RSS not supported\n");
			return -EINVAL;
		}
	}

	if (rxmode->split_hdr_size) {
		PMD_INIT_LOG(INFO, "rxmode does not support split header\n");
		return -EINVAL;
	}

	if (rxmode->hw_ip_checksum) {
		if (hw->cap & NFP_NET_CFG_CTRL_RXCSUM) {
			new_ctrl |= NFP_NET_CFG_CTRL_RXCSUM;
		} else {
			PMD_INIT_LOG(INFO, "RXCSUM not supported\n");
			return -EINVAL;
		}
	}

	if (rxmode->hw_vlan_filter) {
		PMD_INIT_LOG(INFO, "VLAN filter not supported\n");
		return -EINVAL;
	}

	if (rxmode->hw_vlan_strip) {
		if (hw->cap & NFP_NET_CFG_CTRL_RXVLAN) {
			new_ctrl |= NFP_NET_CFG_CTRL_RXVLAN;
		} else {
			PMD_INIT_LOG(INFO, "hw vlan strip not supported\n");
			return -EINVAL;
		}
	}

	if (rxmode->hw_vlan_extend) {
		PMD_INIT_LOG(INFO, "VLAN extended not supported\n");
		return -EINVAL;
	}

	/* Supporting VLAN insertion by default */
	if (hw->cap & NFP_NET_CFG_CTRL_TXVLAN)
		new_ctrl |= NFP_NET_CFG_CTRL_TXVLAN;

	if (rxmode->jumbo_frame)
		/* this is handled in rte_eth_dev_configure */

	if (rxmode->hw_strip_crc) {
		PMD_INIT_LOG(INFO, "strip CRC not supported\n");
		return -EINVAL;
	}

	if (rxmode->enable_scatter) {
		PMD_INIT_LOG(INFO, "Scatter not supported\n");
		return -EINVAL;
	}

	if (!new_ctrl)
		return 0;

	update |= NFP_NET_CFG_UPDATE_GEN;

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, new_ctrl);
	if (nfp_net_reconfig(hw, new_ctrl, update) < 0)
		return -EIO;

	hw->ctrl = new_ctrl;

	return 0;
}

static void
nfp_net_enable_queues(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	uint64_t enabled_queues = 0;
	int i;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Enabling the required TX queues in the device */
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		enabled_queues |= (1 << i);

	nn_cfg_writeq(hw, NFP_NET_CFG_TXRS_ENABLE, enabled_queues);

	enabled_queues = 0;

	/* Enabling the required RX queues in the device */
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		enabled_queues |= (1 << i);

	nn_cfg_writeq(hw, NFP_NET_CFG_RXRS_ENABLE, enabled_queues);
}

static void
nfp_net_disable_queues(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	uint32_t new_ctrl, update = 0;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	nn_cfg_writeq(hw, NFP_NET_CFG_TXRS_ENABLE, 0);
	nn_cfg_writeq(hw, NFP_NET_CFG_RXRS_ENABLE, 0);

	new_ctrl = hw->ctrl & ~NFP_NET_CFG_CTRL_ENABLE;
	update = NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING |
		 NFP_NET_CFG_UPDATE_MSIX;

	if (hw->cap & NFP_NET_CFG_CTRL_RINGCFG)
		new_ctrl &= ~NFP_NET_CFG_CTRL_RINGCFG;

	/* If an error when reconfig we avoid to change hw state */
	if (nfp_net_reconfig(hw, new_ctrl, update) < 0)
		return;

	hw->ctrl = new_ctrl;
}

static void
nfp_net_params_setup(struct nfp_net_hw *hw)
{
	uint32_t *mac_address;

	nn_cfg_writel(hw, NFP_NET_CFG_MTU, hw->mtu);
	nn_cfg_writel(hw, NFP_NET_CFG_FLBUFSZ, hw->flbufsz);

	/* A MAC address is 8 bytes long */
	mac_address = (uint32_t *)(hw->mac_addr);

	nn_cfg_writel(hw, NFP_NET_CFG_MACADDR,
		      rte_cpu_to_be_32(*mac_address));
	nn_cfg_writel(hw, NFP_NET_CFG_MACADDR + 4,
		      rte_cpu_to_be_32(*(mac_address + 4)));
}

static void
nfp_net_cfg_queue_setup(struct nfp_net_hw *hw)
{
	hw->qcp_cfg = hw->tx_bar + NFP_QCP_QUEUE_ADDR_SZ;
}

static int
nfp_net_start(struct rte_eth_dev *dev)
{
	uint32_t new_ctrl, update = 0;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_LOG(DEBUG, "Start\n");

	/* Disabling queues just in case... */
	nfp_net_disable_queues(dev);

	/* Writing configuration parameters in the device */
	nfp_net_params_setup(hw);

	/* Enabling the required queues in the device */
	nfp_net_enable_queues(dev);

	/* Enable device */
	new_ctrl = hw->ctrl | NFP_NET_CFG_CTRL_ENABLE | NFP_NET_CFG_UPDATE_MSIX;
	update = NFP_NET_CFG_UPDATE_GEN | NFP_NET_CFG_UPDATE_RING;

	if (hw->cap & NFP_NET_CFG_CTRL_RINGCFG)
		new_ctrl |= NFP_NET_CFG_CTRL_RINGCFG;

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, new_ctrl);
	if (nfp_net_reconfig(hw, new_ctrl, update) < 0)
		return -EIO;

	hw->ctrl = new_ctrl;

	return 0;
}

/* Stop device: disable rx and tx functions to allow for reconfiguring. */
static void
nfp_net_stop(struct rte_eth_dev *dev)
{
	PMD_INIT_LOG(DEBUG, "Stop\n");

	nfp_net_disable_queues(dev);
}

/* Reset and stop device. The device can not be restarted. */
static void
nfp_net_close(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;

	PMD_INIT_LOG(DEBUG, "Close\n");

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * We assume that the DPDK application is stopping all the
	 * threads/queues before calling the device close function.
	 */

	nfp_net_stop(dev);

	nn_cfg_writeb(hw, NFP_NET_CFG_LSC, 0xff);

	/*
	 * The ixgbe PMD driver disables the pcie master on the
	 * device. The i40e does not...
	 */
}

/* Initialise and register driver with DPDK Application */
static struct eth_dev_ops nfp_net_eth_dev_ops = {
	.dev_configure		= nfp_net_configure,
	.dev_start		= nfp_net_start,
	.dev_stop		= nfp_net_stop,
	.dev_close		= nfp_net_close,
};

static int
nfp_net_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct nfp_net_hw *hw;

	uint32_t tx_bar_off, rx_bar_off;
	uint32_t start_q;
	int stride = 4;

	PMD_INIT_FUNC_TRACE();

	hw = NFP_NET_DEV_PRIVATE_TO_HW(eth_dev->data->dev_private);

	eth_dev->dev_ops = &nfp_net_eth_dev_ops;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pci_dev = eth_dev->pci_dev;
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

	PMD_INIT_LOG(DEBUG, "nfp_net: device (%u:%u) %u:%u:%u:%u\n",
		     pci_dev->id.vendor_id, pci_dev->id.device_id,
		     pci_dev->addr.domain, pci_dev->addr.bus,
		     pci_dev->addr.devid, pci_dev->addr.function);

	hw->ctrl_bar = (uint8_t *)pci_dev->mem_resource[0].addr;
	if (hw->ctrl_bar == NULL) {
		RTE_LOG(ERR, PMD,
			"hw->ctrl_bar is NULL. BAR0 not configured\n");
		return -ENODEV;
	}
	hw->max_rx_queues = nn_cfg_readl(hw, NFP_NET_CFG_MAX_RXRINGS);
	hw->max_tx_queues = nn_cfg_readl(hw, NFP_NET_CFG_MAX_TXRINGS);

	/* Work out where in the BAR the queues start. */
	switch (pci_dev->id.device_id) {
	case PCI_DEVICE_ID_NFP6000_VF_NIC:
		start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_TXQ);
		tx_bar_off = NFP_PCIE_QUEUE(start_q);
		start_q = nn_cfg_readl(hw, NFP_NET_CFG_START_RXQ);
		rx_bar_off = NFP_PCIE_QUEUE(start_q);
		break;
	default:
		RTE_LOG(ERR, PMD, "nfp_net: no device ID matching\n");
		return -ENODEV;
	}

	PMD_INIT_LOG(DEBUG, "tx_bar_off: 0x%08x\n", tx_bar_off);
	PMD_INIT_LOG(DEBUG, "rx_bar_off: 0x%08x\n", rx_bar_off);

	hw->tx_bar = (uint8_t *)pci_dev->mem_resource[2].addr + tx_bar_off;
	hw->rx_bar = (uint8_t *)pci_dev->mem_resource[2].addr + rx_bar_off;

	PMD_INIT_LOG(DEBUG, "ctrl_bar: %p, tx_bar: %p, rx_bar: %p\n",
		     hw->ctrl_bar, hw->tx_bar, hw->rx_bar);

	nfp_net_cfg_queue_setup(hw);

	/* Get some of the read-only fields from the config BAR */
	hw->ver = nn_cfg_readl(hw, NFP_NET_CFG_VERSION);
	hw->cap = nn_cfg_readl(hw, NFP_NET_CFG_CAP);
	hw->max_mtu = nn_cfg_readl(hw, NFP_NET_CFG_MAX_MTU);
	hw->mtu = hw->max_mtu;

	if (NFD_CFG_MAJOR_VERSION_of(hw->ver) < 2)
		hw->rx_offset = NFP_NET_RX_OFFSET;
	else
		hw->rx_offset = nn_cfg_readl(hw, NFP_NET_CFG_RX_OFFSET_ADDR);

	PMD_INIT_LOG(INFO, "VER: %#x, Maximum supported MTU: %d\n",
		     hw->ver, hw->max_mtu);
	PMD_INIT_LOG(INFO, "CAP: %#x, %s%s%s%s%s%s%s%s%s\n", hw->cap,
		     hw->cap & NFP_NET_CFG_CTRL_PROMISC ? "PROMISC " : "",
		     hw->cap & NFP_NET_CFG_CTRL_RXCSUM  ? "RXCSUM "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_TXCSUM  ? "TXCSUM "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_RXVLAN  ? "RXVLAN "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_TXVLAN  ? "TXVLAN "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_SCATTER ? "SCATTER " : "",
		     hw->cap & NFP_NET_CFG_CTRL_GATHER  ? "GATHER "  : "",
		     hw->cap & NFP_NET_CFG_CTRL_LSO     ? "TSO "     : "",
		     hw->cap & NFP_NET_CFG_CTRL_RSS     ? "RSS "     : "");

	pci_dev = eth_dev->pci_dev;
	hw->ctrl = 0;

	hw->stride_rx = stride;
	hw->stride_tx = stride;

	PMD_INIT_LOG(INFO, "max_rx_queues: %u, max_tx_queues: %u\n",
		     hw->max_rx_queues, hw->max_tx_queues);

	/* Allocating memory for mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr", ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to space for MAC address");
		return -ENOMEM;
	}

	/* Using random mac addresses for VFs */
	eth_random_addr(&hw->mac_addr[0]);

	/* Copying mac address to DPDK eth_dev struct */
	ether_addr_copy(&eth_dev->data->mac_addrs[0],
			(struct ether_addr *)hw->mac_addr);

	PMD_INIT_LOG(INFO, "port %d VendorID=0x%x DeviceID=0x%x "
		     "mac=%02x:%02x:%02x:%02x:%02x:%02x",
		     eth_dev->data->port_id, pci_dev->id.vendor_id,
		     pci_dev->id.device_id,
		     hw->mac_addr[0], hw->mac_addr[1], hw->mac_addr[2],
		     hw->mac_addr[3], hw->mac_addr[4], hw->mac_addr[5]);

	return 0;
}

static struct rte_pci_id pci_id_nfp_net_map[] = {
	{
		.vendor_id = PCI_VENDOR_ID_NETRONOME,
		.device_id = PCI_DEVICE_ID_NFP6000_PF_NIC,
		.subsystem_vendor_id = PCI_ANY_ID,
		.subsystem_device_id = PCI_ANY_ID,
	},
	{
		.vendor_id = PCI_VENDOR_ID_NETRONOME,
		.device_id = PCI_DEVICE_ID_NFP6000_VF_NIC,
		.subsystem_vendor_id = PCI_ANY_ID,
		.subsystem_device_id = PCI_ANY_ID,
	},
	{
		.vendor_id = 0,
	},
};

static struct eth_driver rte_nfp_net_pmd = {
	{
		.name = "rte_nfp_net_pmd",
		.id_table = pci_id_nfp_net_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	},
	.eth_dev_init = nfp_net_init,
	.dev_private_size = sizeof(struct nfp_net_adapter),
};

static int
nfp_net_pmd_init(const char *name __rte_unused,
		 const char *params __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
	PMD_INIT_LOG(INFO, "librte_pmd_nfp_net version %s\n",
		     NFP_NET_PMD_VERSION);

	rte_eth_driver_register(&rte_nfp_net_pmd);
	return 0;
}

static struct rte_driver rte_nfp_net_driver = {
	.type = PMD_PDEV,
	.init = nfp_net_pmd_init,
};

PMD_REGISTER_DRIVER(rte_nfp_net_driver);

/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */
