/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2018 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_common.c
 *
 * Netronome vNIC DPDK Poll-Mode Driver: Common files
 */

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <dev_driver.h>
#include <rte_ether.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_mempool.h>
#include <rte_version.h>
#include <rte_alarm.h>
#include <rte_spinlock.h>
#include <rte_service_component.h>

#include "nfpcore/nfp_cpp.h"
#include "nfpcore/nfp_nffw.h"
#include "nfpcore/nfp_hwinfo.h"
#include "nfpcore/nfp_mip.h"
#include "nfpcore/nfp_rtsym.h"
#include "nfpcore/nfp_nsp.h"

#include "flower/nfp_flower_representor.h"

#include "nfp_common.h"
#include "nfp_ctrl.h"
#include "nfp_rxtx.h"
#include "nfp_logs.h"
#include "nfp_cpp_bridge.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>

static const uint32_t nfp_net_link_speed_nfp2rte[] = {
	[NFP_NET_CFG_STS_LINK_RATE_UNSUPPORTED] = RTE_ETH_SPEED_NUM_NONE,
	[NFP_NET_CFG_STS_LINK_RATE_UNKNOWN]     = RTE_ETH_SPEED_NUM_NONE,
	[NFP_NET_CFG_STS_LINK_RATE_1G]          = RTE_ETH_SPEED_NUM_1G,
	[NFP_NET_CFG_STS_LINK_RATE_10G]         = RTE_ETH_SPEED_NUM_10G,
	[NFP_NET_CFG_STS_LINK_RATE_25G]         = RTE_ETH_SPEED_NUM_25G,
	[NFP_NET_CFG_STS_LINK_RATE_40G]         = RTE_ETH_SPEED_NUM_40G,
	[NFP_NET_CFG_STS_LINK_RATE_50G]         = RTE_ETH_SPEED_NUM_50G,
	[NFP_NET_CFG_STS_LINK_RATE_100G]        = RTE_ETH_SPEED_NUM_100G,
};

static uint16_t
nfp_net_link_speed_rte2nfp(uint16_t speed)
{
	uint16_t i;

	for (i = 0; i < RTE_DIM(nfp_net_link_speed_nfp2rte); i++) {
		if (speed == nfp_net_link_speed_nfp2rte[i])
			return i;
	}

	return NFP_NET_CFG_STS_LINK_RATE_UNKNOWN;
}

static void
nfp_net_notify_port_speed(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	struct nfp_eth_table *eth_table;
	uint32_t nn_link_status;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	eth_table = hw->pf_dev->nfp_eth_table;

	/**
	 * Read the link status from NFP_NET_CFG_STS. If the link is down
	 * then write the link speed NFP_NET_CFG_STS_LINK_RATE_UNKNOWN to
	 * NFP_NET_CFG_STS_NSP_LINK_RATE.
	 */
	nn_link_status = nn_cfg_readw(hw, NFP_NET_CFG_STS);
	if ((nn_link_status & NFP_NET_CFG_STS_LINK) == 0) {
		nn_cfg_writew(hw, NFP_NET_CFG_STS_NSP_LINK_RATE, NFP_NET_CFG_STS_LINK_RATE_UNKNOWN);
		return;
	}
	/**
	 * Link is up so read the link speed from the eth_table and write to
	 * NFP_NET_CFG_STS_NSP_LINK_RATE.
	 */
	nn_cfg_writew(hw, NFP_NET_CFG_STS_NSP_LINK_RATE,
		      nfp_net_link_speed_rte2nfp(eth_table->ports[hw->idx].speed));
}

static int
__nfp_net_reconfig(struct nfp_net_hw *hw, uint32_t update)
{
	int cnt;
	uint32_t new;
	struct timespec wait;

	PMD_DRV_LOG(DEBUG, "Writing to the configuration queue (%p)...",
		    hw->qcp_cfg);

	if (hw->qcp_cfg == NULL) {
		PMD_INIT_LOG(ERR, "Bad configuration queue pointer");
		return -ENXIO;
	}

	nfp_qcp_ptr_add(hw->qcp_cfg, NFP_QCP_WRITE_PTR, 1);

	wait.tv_sec = 0;
	wait.tv_nsec = 1000000;

	PMD_DRV_LOG(DEBUG, "Polling for update ack...");

	/* Poll update field, waiting for NFP to ack the config */
	for (cnt = 0; ; cnt++) {
		new = nn_cfg_readl(hw, NFP_NET_CFG_UPDATE);
		if (new == 0)
			break;
		if (new & NFP_NET_CFG_UPDATE_ERR) {
			PMD_INIT_LOG(ERR, "Reconfig error: 0x%08x", new);
			return -1;
		}
		if (cnt >= NFP_NET_POLL_TIMEOUT) {
			PMD_INIT_LOG(ERR, "Reconfig timeout for 0x%08x after"
					  " %dms", update, cnt);
			return -EIO;
		}
		nanosleep(&wait, 0); /* waiting for a 1ms */
	}
	PMD_DRV_LOG(DEBUG, "Ack DONE");
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
int
nfp_net_reconfig(struct nfp_net_hw *hw, uint32_t ctrl, uint32_t update)
{
	uint32_t err;

	PMD_DRV_LOG(DEBUG, "nfp_net_reconfig: ctrl=%08x update=%08x",
		    ctrl, update);

	if (hw->pf_dev != NULL && hw->pf_dev->app_fw_id == NFP_APP_FW_CORE_NIC)
		nfp_net_notify_port_speed(hw->eth_dev);

	rte_spinlock_lock(&hw->reconfig_lock);

	nn_cfg_writel(hw, NFP_NET_CFG_CTRL, ctrl);
	nn_cfg_writel(hw, NFP_NET_CFG_UPDATE, update);

	rte_wmb();

	err = __nfp_net_reconfig(hw, update);

	rte_spinlock_unlock(&hw->reconfig_lock);

	if (err != 0) {
		PMD_INIT_LOG(ERR, "Error nfp_net reconfig for ctrl: %x update: %x",
			     ctrl, update);
		return -EIO;
	}

	return 0;

}

/*
 * Configure an Ethernet device. This function must be invoked first
 * before any other function in the Ethernet API. This function can
 * also be re-invoked when a device is in the stopped state.
 */
int
nfp_net_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rxmode *rxmode;
	struct rte_eth_txmode *txmode;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * A DPDK app sends info about how many queues to use and how
	 * those queues need to be configured. This is used by the
	 * DPDK core and it makes sure no more queues than those
	 * advertised by the driver are requested. This function is
	 * called after that internal process
	 */

	PMD_INIT_LOG(DEBUG, "Configure");

	dev_conf = &dev->data->dev_conf;
	rxmode = &dev_conf->rxmode;
	txmode = &dev_conf->txmode;

	if (rxmode->mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		rxmode->offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	/* Checking TX mode */
	if (txmode->mq_mode) {
		PMD_INIT_LOG(INFO, "TX mq_mode DCB and VMDq not supported");
		return -EINVAL;
	}

	/* Checking RX mode */
	if (rxmode->mq_mode & RTE_ETH_MQ_RX_RSS_FLAG &&
	    !(hw->cap & NFP_NET_CFG_CTRL_RSS_ANY)) {
		PMD_INIT_LOG(INFO, "RSS not supported");
		return -EINVAL;
	}

	/* Checking MTU set */
	if (rxmode->mtu > NFP_FRAME_SIZE_MAX) {
		PMD_INIT_LOG(ERR, "MTU (%u) larger than NFP_FRAME_SIZE_MAX (%u) not supported",
				    rxmode->mtu, NFP_FRAME_SIZE_MAX);
		return -ERANGE;
	}

	return 0;
}

void
nfp_net_log_device_information(const struct nfp_net_hw *hw)
{
	PMD_INIT_LOG(INFO, "VER: %u.%u, Maximum supported MTU: %d",
			NFD_CFG_MAJOR_VERSION_of(hw->ver),
			NFD_CFG_MINOR_VERSION_of(hw->ver), hw->max_mtu);

	PMD_INIT_LOG(INFO, "CAP: %#x, %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", hw->cap,
			hw->cap & NFP_NET_CFG_CTRL_PROMISC   ? "PROMISC "   : "",
			hw->cap & NFP_NET_CFG_CTRL_L2BC      ? "L2BCFILT "  : "",
			hw->cap & NFP_NET_CFG_CTRL_L2MC      ? "L2MCFILT "  : "",
			hw->cap & NFP_NET_CFG_CTRL_RXCSUM    ? "RXCSUM "    : "",
			hw->cap & NFP_NET_CFG_CTRL_TXCSUM    ? "TXCSUM "    : "",
			hw->cap & NFP_NET_CFG_CTRL_RXVLAN    ? "RXVLAN "    : "",
			hw->cap & NFP_NET_CFG_CTRL_TXVLAN    ? "TXVLAN "    : "",
			hw->cap & NFP_NET_CFG_CTRL_RXVLAN_V2 ? "RXVLANv2 "  : "",
			hw->cap & NFP_NET_CFG_CTRL_TXVLAN_V2 ? "TXVLANv2 "  : "",
			hw->cap & NFP_NET_CFG_CTRL_RXQINQ    ? "RXQINQ "    : "",
			hw->cap & NFP_NET_CFG_CTRL_SCATTER   ? "SCATTER "   : "",
			hw->cap & NFP_NET_CFG_CTRL_GATHER    ? "GATHER "    : "",
			hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR ? "LIVE_ADDR " : "",
			hw->cap & NFP_NET_CFG_CTRL_LSO       ? "TSO "       : "",
			hw->cap & NFP_NET_CFG_CTRL_LSO2      ? "TSOv2 "     : "",
			hw->cap & NFP_NET_CFG_CTRL_RSS       ? "RSS "       : "",
			hw->cap & NFP_NET_CFG_CTRL_RSS2      ? "RSSv2 "     : "");

	PMD_INIT_LOG(INFO, "max_rx_queues: %u, max_tx_queues: %u",
			hw->max_rx_queues, hw->max_tx_queues);
}

static inline void
nfp_net_enbable_rxvlan_cap(struct nfp_net_hw *hw, uint32_t *ctrl)
{
	if ((hw->cap & NFP_NET_CFG_CTRL_RXVLAN_V2) != 0)
		*ctrl |= NFP_NET_CFG_CTRL_RXVLAN_V2;
	else if ((hw->cap & NFP_NET_CFG_CTRL_RXVLAN) != 0)
		*ctrl |= NFP_NET_CFG_CTRL_RXVLAN;
}

void
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

void
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

void
nfp_net_params_setup(struct nfp_net_hw *hw)
{
	nn_cfg_writel(hw, NFP_NET_CFG_MTU, hw->mtu);
	nn_cfg_writel(hw, NFP_NET_CFG_FLBUFSZ, hw->flbufsz);
}

void
nfp_net_cfg_queue_setup(struct nfp_net_hw *hw)
{
	hw->qcp_cfg = hw->tx_bar + NFP_QCP_QUEUE_ADDR_SZ;
}

#define ETH_ADDR_LEN	6

void
nfp_eth_copy_mac(uint8_t *dst, const uint8_t *src)
{
	int i;

	for (i = 0; i < ETH_ADDR_LEN; i++)
		dst[i] = src[i];
}

void
nfp_net_write_mac(struct nfp_net_hw *hw, uint8_t *mac)
{
	uint32_t mac0 = *(uint32_t *)mac;
	uint16_t mac1;

	nn_writel(rte_cpu_to_be_32(mac0), hw->ctrl_bar + NFP_NET_CFG_MACADDR);

	mac += 4;
	mac1 = *(uint16_t *)mac;
	nn_writew(rte_cpu_to_be_16(mac1),
		  hw->ctrl_bar + NFP_NET_CFG_MACADDR + 6);
}

int
nfp_net_set_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr)
{
	struct nfp_net_hw *hw;
	uint32_t update, ctrl;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if ((hw->ctrl & NFP_NET_CFG_CTRL_ENABLE) &&
	    !(hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR)) {
		PMD_INIT_LOG(INFO, "MAC address unable to change when"
				  " port enabled");
		return -EBUSY;
	}

	/* Writing new MAC to the specific port BAR address */
	nfp_net_write_mac(hw, (uint8_t *)mac_addr);

	/* Signal the NIC about the change */
	update = NFP_NET_CFG_UPDATE_MACADDR;
	ctrl = hw->ctrl;
	if ((hw->ctrl & NFP_NET_CFG_CTRL_ENABLE) &&
	    (hw->cap & NFP_NET_CFG_CTRL_LIVE_ADDR))
		ctrl |= NFP_NET_CFG_CTRL_LIVE_ADDR;
	if (nfp_net_reconfig(hw, ctrl, update) < 0) {
		PMD_INIT_LOG(INFO, "MAC address update failed");
		return -EIO;
	}
	return 0;
}

int
nfp_configure_rx_interrupt(struct rte_eth_dev *dev,
			   struct rte_intr_handle *intr_handle)
{
	struct nfp_net_hw *hw;
	int i;

	if (rte_intr_vec_list_alloc(intr_handle, "intr_vec",
				    dev->data->nb_rx_queues)) {
		PMD_INIT_LOG(ERR, "Failed to allocate %d rx_queues"
			     " intr_vec", dev->data->nb_rx_queues);
		return -ENOMEM;
	}

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (rte_intr_type_get(intr_handle) == RTE_INTR_HANDLE_UIO) {
		PMD_INIT_LOG(INFO, "VF: enabling RX interrupt with UIO");
		/* UIO just supports one queue and no LSC*/
		nn_cfg_writeb(hw, NFP_NET_CFG_RXR_VEC(0), 0);
		if (rte_intr_vec_list_index_set(intr_handle, 0, 0))
			return -1;
	} else {
		PMD_INIT_LOG(INFO, "VF: enabling RX interrupt with VFIO");
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			/*
			 * The first msix vector is reserved for non
			 * efd interrupts
			*/
			nn_cfg_writeb(hw, NFP_NET_CFG_RXR_VEC(i), i + 1);
			if (rte_intr_vec_list_index_set(intr_handle, i,
							       i + 1))
				return -1;
			PMD_INIT_LOG(DEBUG, "intr_vec[%d]= %d", i,
				rte_intr_vec_list_index_get(intr_handle,
								   i));
		}
	}

	/* Avoiding TX interrupts */
	hw->ctrl |= NFP_NET_CFG_CTRL_MSIX_TX_OFF;
	return 0;
}

uint32_t
nfp_check_offloads(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rxmode *rxmode;
	struct rte_eth_txmode *txmode;
	uint32_t ctrl = 0;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	dev_conf = &dev->data->dev_conf;
	rxmode = &dev_conf->rxmode;
	txmode = &dev_conf->txmode;

	if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM) {
		if (hw->cap & NFP_NET_CFG_CTRL_RXCSUM)
			ctrl |= NFP_NET_CFG_CTRL_RXCSUM;
	}

	if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
		nfp_net_enbable_rxvlan_cap(hw, &ctrl);

	if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_QINQ_STRIP) {
		if (hw->cap & NFP_NET_CFG_CTRL_RXQINQ)
			ctrl |= NFP_NET_CFG_CTRL_RXQINQ;
	}

	hw->mtu = dev->data->mtu;

	if (txmode->offloads & RTE_ETH_TX_OFFLOAD_VLAN_INSERT) {
		if (hw->cap & NFP_NET_CFG_CTRL_TXVLAN_V2)
			ctrl |= NFP_NET_CFG_CTRL_TXVLAN_V2;
		else if (hw->cap & NFP_NET_CFG_CTRL_TXVLAN)
			ctrl |= NFP_NET_CFG_CTRL_TXVLAN;
	}

	/* L2 broadcast */
	if (hw->cap & NFP_NET_CFG_CTRL_L2BC)
		ctrl |= NFP_NET_CFG_CTRL_L2BC;

	/* L2 multicast */
	if (hw->cap & NFP_NET_CFG_CTRL_L2MC)
		ctrl |= NFP_NET_CFG_CTRL_L2MC;

	/* TX checksum offload */
	if (txmode->offloads & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM ||
	    txmode->offloads & RTE_ETH_TX_OFFLOAD_UDP_CKSUM ||
	    txmode->offloads & RTE_ETH_TX_OFFLOAD_TCP_CKSUM)
		ctrl |= NFP_NET_CFG_CTRL_TXCSUM;

	/* LSO offload */
	if (txmode->offloads & RTE_ETH_TX_OFFLOAD_TCP_TSO ||
	    txmode->offloads & RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO) {
		if (hw->cap & NFP_NET_CFG_CTRL_LSO)
			ctrl |= NFP_NET_CFG_CTRL_LSO;
		else
			ctrl |= NFP_NET_CFG_CTRL_LSO2;
	}

	/* RX gather */
	if (txmode->offloads & RTE_ETH_TX_OFFLOAD_MULTI_SEGS)
		ctrl |= NFP_NET_CFG_CTRL_GATHER;

	return ctrl;
}

int
nfp_net_promisc_enable(struct rte_eth_dev *dev)
{
	uint32_t new_ctrl, update = 0;
	struct nfp_net_hw *hw;
	int ret;
	struct nfp_flower_representor *repr;

	PMD_DRV_LOG(DEBUG, "Promiscuous mode enable");

	if ((dev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR) != 0) {
		repr = dev->data->dev_private;
		hw = repr->app_fw_flower->pf_hw;
	} else {
		hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	}

	if ((hw->cap & NFP_NET_CFG_CTRL_PROMISC) == 0) {
		PMD_INIT_LOG(INFO, "Promiscuous mode not supported");
		return -ENOTSUP;
	}

	if (hw->ctrl & NFP_NET_CFG_CTRL_PROMISC) {
		PMD_DRV_LOG(INFO, "Promiscuous mode already enabled");
		return 0;
	}

	new_ctrl = hw->ctrl | NFP_NET_CFG_CTRL_PROMISC;
	update = NFP_NET_CFG_UPDATE_GEN;

	/*
	 * DPDK sets promiscuous mode on just after this call assuming
	 * it can not fail ...
	 */
	ret = nfp_net_reconfig(hw, new_ctrl, update);
	if (ret < 0)
		return ret;

	hw->ctrl = new_ctrl;

	return 0;
}

int
nfp_net_promisc_disable(struct rte_eth_dev *dev)
{
	uint32_t new_ctrl, update = 0;
	struct nfp_net_hw *hw;
	int ret;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if ((hw->ctrl & NFP_NET_CFG_CTRL_PROMISC) == 0) {
		PMD_DRV_LOG(INFO, "Promiscuous mode already disabled");
		return 0;
	}

	new_ctrl = hw->ctrl & ~NFP_NET_CFG_CTRL_PROMISC;
	update = NFP_NET_CFG_UPDATE_GEN;

	/*
	 * DPDK sets promiscuous mode off just before this call
	 * assuming it can not fail ...
	 */
	ret = nfp_net_reconfig(hw, new_ctrl, update);
	if (ret < 0)
		return ret;

	hw->ctrl = new_ctrl;

	return 0;
}

/*
 * return 0 means link status changed, -1 means not changed
 *
 * Wait to complete is needed as it can take up to 9 seconds to get the Link
 * status.
 */
int
nfp_net_link_update(struct rte_eth_dev *dev, __rte_unused int wait_to_complete)
{
	struct nfp_net_hw *hw;
	struct rte_eth_link link;
	uint16_t nn_link_status;
	int ret;

	PMD_DRV_LOG(DEBUG, "Link update");

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Read link status */
	nn_link_status = nn_cfg_readw(hw, NFP_NET_CFG_STS);

	memset(&link, 0, sizeof(struct rte_eth_link));

	if (nn_link_status & NFP_NET_CFG_STS_LINK)
		link.link_status = RTE_ETH_LINK_UP;

	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;

	/**
	 * Shift and mask nn_link_status so that it is effectively the value
	 * at offset NFP_NET_CFG_STS_NSP_LINK_RATE.
	 */
	nn_link_status = (nn_link_status >> NFP_NET_CFG_STS_LINK_RATE_SHIFT) &
			 NFP_NET_CFG_STS_LINK_RATE_MASK;

	if (nn_link_status >= RTE_DIM(nfp_net_link_speed_nfp2rte))
		link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	else
		link.link_speed = nfp_net_link_speed_nfp2rte[nn_link_status];

	ret = rte_eth_linkstatus_set(dev, &link);
	if (ret == 0) {
		if (link.link_status)
			PMD_DRV_LOG(INFO, "NIC Link is Up");
		else
			PMD_DRV_LOG(INFO, "NIC Link is Down");
	}
	return ret;
}

int
nfp_net_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	int i;
	struct nfp_net_hw *hw;
	struct rte_eth_stats nfp_dev_stats;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* RTE_ETHDEV_QUEUE_STAT_CNTRS default value is 16 */

	memset(&nfp_dev_stats, 0, sizeof(nfp_dev_stats));

	/* reading per RX ring stats */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (i == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		nfp_dev_stats.q_ipackets[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_RXR_STATS(i));

		nfp_dev_stats.q_ipackets[i] -=
			hw->eth_stats_base.q_ipackets[i];

		nfp_dev_stats.q_ibytes[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_RXR_STATS(i) + 0x8);

		nfp_dev_stats.q_ibytes[i] -=
			hw->eth_stats_base.q_ibytes[i];
	}

	/* reading per TX ring stats */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (i == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		nfp_dev_stats.q_opackets[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_TXR_STATS(i));

		nfp_dev_stats.q_opackets[i] -=
			hw->eth_stats_base.q_opackets[i];

		nfp_dev_stats.q_obytes[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_TXR_STATS(i) + 0x8);

		nfp_dev_stats.q_obytes[i] -=
			hw->eth_stats_base.q_obytes[i];
	}

	nfp_dev_stats.ipackets =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_FRAMES);

	nfp_dev_stats.ipackets -= hw->eth_stats_base.ipackets;

	nfp_dev_stats.ibytes =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_OCTETS);

	nfp_dev_stats.ibytes -= hw->eth_stats_base.ibytes;

	nfp_dev_stats.opackets =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_TX_FRAMES);

	nfp_dev_stats.opackets -= hw->eth_stats_base.opackets;

	nfp_dev_stats.obytes =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_TX_OCTETS);

	nfp_dev_stats.obytes -= hw->eth_stats_base.obytes;

	/* reading general device stats */
	nfp_dev_stats.ierrors =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_ERRORS);

	nfp_dev_stats.ierrors -= hw->eth_stats_base.ierrors;

	nfp_dev_stats.oerrors =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_TX_ERRORS);

	nfp_dev_stats.oerrors -= hw->eth_stats_base.oerrors;

	/* RX ring mbuf allocation failures */
	nfp_dev_stats.rx_nombuf = dev->data->rx_mbuf_alloc_failed;

	nfp_dev_stats.imissed =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_DISCARDS);

	nfp_dev_stats.imissed -= hw->eth_stats_base.imissed;

	if (stats) {
		memcpy(stats, &nfp_dev_stats, sizeof(*stats));
		return 0;
	}
	return -EINVAL;
}

int
nfp_net_stats_reset(struct rte_eth_dev *dev)
{
	int i;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * hw->eth_stats_base records the per counter starting point.
	 * Lets update it now
	 */

	/* reading per RX ring stats */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (i == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		hw->eth_stats_base.q_ipackets[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_RXR_STATS(i));

		hw->eth_stats_base.q_ibytes[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_RXR_STATS(i) + 0x8);
	}

	/* reading per TX ring stats */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (i == RTE_ETHDEV_QUEUE_STAT_CNTRS)
			break;

		hw->eth_stats_base.q_opackets[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_TXR_STATS(i));

		hw->eth_stats_base.q_obytes[i] =
			nn_cfg_readq(hw, NFP_NET_CFG_TXR_STATS(i) + 0x8);
	}

	hw->eth_stats_base.ipackets =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_FRAMES);

	hw->eth_stats_base.ibytes =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_OCTETS);

	hw->eth_stats_base.opackets =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_TX_FRAMES);

	hw->eth_stats_base.obytes =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_TX_OCTETS);

	/* reading general device stats */
	hw->eth_stats_base.ierrors =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_ERRORS);

	hw->eth_stats_base.oerrors =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_TX_ERRORS);

	/* RX ring mbuf allocation failures */
	dev->data->rx_mbuf_alloc_failed = 0;

	hw->eth_stats_base.imissed =
		nn_cfg_readq(hw, NFP_NET_CFG_STATS_RX_DISCARDS);

	return 0;
}

int
nfp_net_rx_desc_limits(struct nfp_net_hw *hw,
		uint16_t *min_rx_desc,
		uint16_t *max_rx_desc)
{
	*max_rx_desc = NFP_NET_MAX_RX_DESC;

	switch (hw->device_id) {
	case PCI_DEVICE_ID_NFP3800_PF_NIC:
	case PCI_DEVICE_ID_NFP3800_VF_NIC:
		*min_rx_desc = NFP3800_NET_MIN_RX_DESC;
		return 0;
	case PCI_DEVICE_ID_NFP4000_PF_NIC:
	case PCI_DEVICE_ID_NFP6000_PF_NIC:
	case PCI_DEVICE_ID_NFP6000_VF_NIC:
		*min_rx_desc = NFP_NET_MIN_RX_DESC;
		return 0;
	default:
		PMD_DRV_LOG(ERR, "Unknown NFP device id.");
		return -EINVAL;
	}
}

int
nfp_net_tx_desc_limits(struct nfp_net_hw *hw,
		uint16_t *min_tx_desc,
		uint16_t *max_tx_desc)
{
	uint16_t tx_dpp;

	switch (NFD_CFG_CLASS_VER_of(hw->ver)) {
	case NFP_NET_CFG_VERSION_DP_NFD3:
		tx_dpp = NFD3_TX_DESC_PER_SIMPLE_PKT;
		break;
	case NFP_NET_CFG_VERSION_DP_NFDK:
		if (NFD_CFG_MAJOR_VERSION_of(hw->ver) < 5) {
			PMD_DRV_LOG(ERR, "NFDK must use ABI 5 or newer, found: %d",
				NFD_CFG_MAJOR_VERSION_of(hw->ver));
			return -EINVAL;
		}
		tx_dpp = NFDK_TX_DESC_PER_SIMPLE_PKT;
		break;
	default:
		PMD_DRV_LOG(ERR, "The version of firmware is not correct.");
		return -EINVAL;
	}

	*max_tx_desc = NFP_NET_MAX_TX_DESC / tx_dpp;

	switch (hw->device_id) {
	case PCI_DEVICE_ID_NFP3800_PF_NIC:
	case PCI_DEVICE_ID_NFP3800_VF_NIC:
		*min_tx_desc = NFP3800_NET_MIN_TX_DESC / tx_dpp;
		return 0;
	case PCI_DEVICE_ID_NFP4000_PF_NIC:
	case PCI_DEVICE_ID_NFP6000_PF_NIC:
	case PCI_DEVICE_ID_NFP6000_VF_NIC:
		*min_tx_desc = NFP_NET_MIN_TX_DESC / tx_dpp;
		return 0;
	default:
		PMD_DRV_LOG(ERR, "Unknown NFP device id.");
		return -EINVAL;
	}
}

int
nfp_net_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	int ret;
	uint16_t min_rx_desc;
	uint16_t max_rx_desc;
	uint16_t min_tx_desc;
	uint16_t max_tx_desc;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	ret = nfp_net_rx_desc_limits(hw, &min_rx_desc, &max_rx_desc);
	if (ret != 0)
		return ret;

	ret = nfp_net_tx_desc_limits(hw, &min_tx_desc, &max_tx_desc);
	if (ret != 0)
		return ret;

	dev_info->max_rx_queues = (uint16_t)hw->max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)hw->max_tx_queues;
	dev_info->min_rx_bufsize = RTE_ETHER_MIN_MTU;
	/*
	 * The maximum rx packet length (max_rx_pktlen) is set to the
	 * maximum supported frame size that the NFP can handle. This
	 * includes layer 2 headers, CRC and other metadata that can
	 * optionally be used.
	 * The maximum layer 3 MTU (max_mtu) is read from hardware,
	 * which was set by the firmware loaded onto the card.
	 */
	dev_info->max_rx_pktlen = NFP_FRAME_SIZE_MAX;
	dev_info->max_mtu = hw->max_mtu;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	/* Next should change when PF support is implemented */
	dev_info->max_mac_addrs = 1;

	if (hw->cap & (NFP_NET_CFG_CTRL_RXVLAN | NFP_NET_CFG_CTRL_RXVLAN_V2))
		dev_info->rx_offload_capa = RTE_ETH_RX_OFFLOAD_VLAN_STRIP;

	if (hw->cap & NFP_NET_CFG_CTRL_RXQINQ)
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_QINQ_STRIP;

	if (hw->cap & NFP_NET_CFG_CTRL_RXCSUM)
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
					     RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
					     RTE_ETH_RX_OFFLOAD_TCP_CKSUM;

	if (hw->cap & (NFP_NET_CFG_CTRL_TXVLAN | NFP_NET_CFG_CTRL_TXVLAN_V2))
		dev_info->tx_offload_capa = RTE_ETH_TX_OFFLOAD_VLAN_INSERT;

	if (hw->cap & NFP_NET_CFG_CTRL_TXCSUM)
		dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
					     RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
					     RTE_ETH_TX_OFFLOAD_TCP_CKSUM;

	if (hw->cap & NFP_NET_CFG_CTRL_LSO_ANY) {
		dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_TCP_TSO;
		if (hw->cap & NFP_NET_CFG_CTRL_VXLAN)
			dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO;
	}

	if (hw->cap & NFP_NET_CFG_CTRL_GATHER)
		dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = DEFAULT_RX_PTHRESH,
			.hthresh = DEFAULT_RX_HTHRESH,
			.wthresh = DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = DEFAULT_TX_PTHRESH,
			.hthresh = DEFAULT_TX_HTHRESH,
			.wthresh = DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = DEFAULT_TX_RSBIT_THRESH,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = max_rx_desc,
		.nb_min = min_rx_desc,
		.nb_align = NFP_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = max_tx_desc,
		.nb_min = min_tx_desc,
		.nb_align = NFP_ALIGN_RING_DESC,
		.nb_seg_max = NFP_TX_MAX_SEG,
		.nb_mtu_seg_max = NFP_TX_MAX_MTU_SEG,
	};

	if (hw->cap & NFP_NET_CFG_CTRL_RSS_ANY) {
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

		dev_info->flow_type_rss_offloads = RTE_ETH_RSS_IPV4 |
						   RTE_ETH_RSS_NONFRAG_IPV4_TCP |
						   RTE_ETH_RSS_NONFRAG_IPV4_UDP |
						   RTE_ETH_RSS_NONFRAG_IPV4_SCTP |
						   RTE_ETH_RSS_IPV6 |
						   RTE_ETH_RSS_NONFRAG_IPV6_TCP |
						   RTE_ETH_RSS_NONFRAG_IPV6_UDP |
						   RTE_ETH_RSS_NONFRAG_IPV6_SCTP;

		dev_info->reta_size = NFP_NET_CFG_RSS_ITBL_SZ;
		dev_info->hash_key_size = NFP_NET_CFG_RSS_KEY_SZ;
	}

	dev_info->speed_capa = RTE_ETH_LINK_SPEED_1G | RTE_ETH_LINK_SPEED_10G |
			       RTE_ETH_LINK_SPEED_25G | RTE_ETH_LINK_SPEED_40G |
			       RTE_ETH_LINK_SPEED_50G | RTE_ETH_LINK_SPEED_100G;

	return 0;
}

const uint32_t *
nfp_net_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/* refers to nfp_net_set_hash() */
		RTE_PTYPE_INNER_L3_IPV4,
		RTE_PTYPE_INNER_L3_IPV6,
		RTE_PTYPE_INNER_L3_IPV6_EXT,
		RTE_PTYPE_INNER_L4_MASK,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == nfp_net_recv_pkts)
		return ptypes;
	return NULL;
}

int
nfp_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev;
	struct nfp_net_hw *hw;
	int base = 0;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (rte_intr_type_get(pci_dev->intr_handle) !=
							RTE_INTR_HANDLE_UIO)
		base = 1;

	/* Make sure all updates are written before un-masking */
	rte_wmb();
	nn_cfg_writeb(hw, NFP_NET_CFG_ICR(base + queue_id),
		      NFP_NET_CFG_ICR_UNMASKED);
	return 0;
}

int
nfp_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev;
	struct nfp_net_hw *hw;
	int base = 0;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (rte_intr_type_get(pci_dev->intr_handle) !=
							RTE_INTR_HANDLE_UIO)
		base = 1;

	/* Make sure all updates are written before un-masking */
	rte_wmb();
	nn_cfg_writeb(hw, NFP_NET_CFG_ICR(base + queue_id), 0x1);
	return 0;
}

static void
nfp_net_dev_link_status_print(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_eth_link link;

	rte_eth_linkstatus_get(dev, &link);
	if (link.link_status)
		PMD_DRV_LOG(INFO, "Port %d: Link Up - speed %u Mbps - %s",
			    dev->data->port_id, link.link_speed,
			    link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX
			    ? "full-duplex" : "half-duplex");
	else
		PMD_DRV_LOG(INFO, " Port %d: Link Down",
			    dev->data->port_id);

	PMD_DRV_LOG(INFO, "PCI Address: " PCI_PRI_FMT,
		    pci_dev->addr.domain, pci_dev->addr.bus,
		    pci_dev->addr.devid, pci_dev->addr.function);
}

/* Interrupt configuration and handling */

/*
 * nfp_net_irq_unmask - Unmask an interrupt
 *
 * If MSI-X auto-masking is enabled clear the mask bit, otherwise
 * clear the ICR for the entry.
 */
static void
nfp_net_irq_unmask(struct rte_eth_dev *dev)
{
	struct nfp_net_hw *hw;
	struct rte_pci_device *pci_dev;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (hw->ctrl & NFP_NET_CFG_CTRL_MSIXAUTO) {
		/* If MSI-X auto-masking is used, clear the entry */
		rte_wmb();
		rte_intr_ack(pci_dev->intr_handle);
	} else {
		/* Make sure all updates are written before un-masking */
		rte_wmb();
		nn_cfg_writeb(hw, NFP_NET_CFG_ICR(NFP_NET_IRQ_LSC_IDX),
			      NFP_NET_CFG_ICR_UNMASKED);
	}
}

/*
 * Interrupt handler which shall be registered for alarm callback for delayed
 * handling specific interrupt to wait for the stable nic state. As the NIC
 * interrupt state is not stable for nfp after link is just down, it needs
 * to wait 4 seconds to get the stable status.
 *
 * @param handle   Pointer to interrupt handle.
 * @param param    The address of parameter (struct rte_eth_dev *)
 *
 * @return  void
 */
void
nfp_net_dev_interrupt_delayed_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	nfp_net_link_update(dev, 0);
	rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);

	nfp_net_dev_link_status_print(dev);

	/* Unmasking */
	nfp_net_irq_unmask(dev);
}

void
nfp_net_dev_interrupt_handler(void *param)
{
	int64_t timeout;
	struct rte_eth_link link;
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;

	PMD_DRV_LOG(DEBUG, "We got a LSC interrupt!!!");

	rte_eth_linkstatus_get(dev, &link);

	nfp_net_link_update(dev, 0);

	/* likely to up */
	if (link.link_status == 0) {
		/* handle it 1 sec later, wait it being stable */
		timeout = NFP_NET_LINK_UP_CHECK_TIMEOUT;
		/* likely to down */
	} else {
		/* handle it 4 sec later, wait it being stable */
		timeout = NFP_NET_LINK_DOWN_CHECK_TIMEOUT;
	}

	if (rte_eal_alarm_set(timeout * 1000,
			      nfp_net_dev_interrupt_delayed_handler,
			      (void *)dev) < 0) {
		PMD_INIT_LOG(ERR, "Error setting alarm");
		/* Unmasking */
		nfp_net_irq_unmask(dev);
	}
}

int
nfp_net_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* mtu setting is forbidden if port is started */
	if (dev->data->dev_started) {
		PMD_DRV_LOG(ERR, "port %d must be stopped before configuration",
			    dev->data->port_id);
		return -EBUSY;
	}

	/* MTU larger than current mbufsize not supported */
	if (mtu > hw->flbufsz) {
		PMD_DRV_LOG(ERR, "MTU (%u) larger than current mbufsize (%u) not supported",
			    mtu, hw->flbufsz);
		return -ERANGE;
	}

	/* writing to configuration space */
	nn_cfg_writel(hw, NFP_NET_CFG_MTU, mtu);

	hw->mtu = mtu;

	return 0;
}

int
nfp_net_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	uint32_t new_ctrl, update;
	struct nfp_net_hw *hw;
	struct rte_eth_conf *dev_conf;
	uint32_t rxvlan_ctrl;
	int ret;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	dev_conf = &dev->data->dev_conf;
	new_ctrl = hw->ctrl;
	rxvlan_ctrl = 0;

	nfp_net_enbable_rxvlan_cap(hw, &rxvlan_ctrl);

	/* VLAN stripping setting */
	if (mask & RTE_ETH_VLAN_STRIP_MASK) {
		if (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			new_ctrl |= rxvlan_ctrl;
		else
			new_ctrl &= ~rxvlan_ctrl;
	}

	/* QinQ stripping setting */
	if (mask & RTE_ETH_QINQ_STRIP_MASK) {
		if (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_QINQ_STRIP)
			new_ctrl |= NFP_NET_CFG_CTRL_RXQINQ;
		else
			new_ctrl &= ~NFP_NET_CFG_CTRL_RXQINQ;
	}

	if (new_ctrl == hw->ctrl)
		return 0;

	update = NFP_NET_CFG_UPDATE_GEN;

	ret = nfp_net_reconfig(hw, new_ctrl, update);
	if (ret == 0)
		hw->ctrl = new_ctrl;

	return ret;
}

static int
nfp_net_rss_reta_write(struct rte_eth_dev *dev,
		    struct rte_eth_rss_reta_entry64 *reta_conf,
		    uint16_t reta_size)
{
	uint32_t reta, mask;
	int i, j;
	int idx, shift;
	struct nfp_net_hw *hw =
		NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (reta_size != NFP_NET_CFG_RSS_ITBL_SZ) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, NFP_NET_CFG_RSS_ITBL_SZ);
		return -EINVAL;
	}

	/*
	 * Update Redirection Table. There are 128 8bit-entries which can be
	 * manage as 32 32bit-entries
	 */
	for (i = 0; i < reta_size; i += 4) {
		/* Handling 4 RSS entries per loop */
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		mask = (uint8_t)((reta_conf[idx].mask >> shift) & 0xF);

		if (mask == 0)
			continue;

		reta = 0;
		/* If all 4 entries were set, don't need read RETA register */
		if (mask != 0xF)
			reta = nn_cfg_readl(hw, NFP_NET_CFG_RSS_ITBL + i);

		for (j = 0; j < 4; j++) {
			if ((mask & (0x1 << j)) == 0)
				continue;
			if (mask != 0xF)
				/* Clearing the entry bits */
				reta &= ~(0xFF << (8 * j));
			reta |= reta_conf[idx].reta[shift + j] << (8 * j);
		}
		nn_cfg_writel(hw, NFP_NET_CFG_RSS_ITBL + (idx * 64) + shift,
			      reta);
	}
	return 0;
}

/* Update Redirection Table(RETA) of Receive Side Scaling of Ethernet device */
int
nfp_net_reta_update(struct rte_eth_dev *dev,
		    struct rte_eth_rss_reta_entry64 *reta_conf,
		    uint16_t reta_size)
{
	struct nfp_net_hw *hw =
		NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t update;
	int ret;

	if ((hw->ctrl & NFP_NET_CFG_CTRL_RSS_ANY) == 0)
		return -EINVAL;

	ret = nfp_net_rss_reta_write(dev, reta_conf, reta_size);
	if (ret != 0)
		return ret;

	update = NFP_NET_CFG_UPDATE_RSS;

	if (nfp_net_reconfig(hw, hw->ctrl, update) < 0)
		return -EIO;

	return 0;
}

 /* Query Redirection Table(RETA) of Receive Side Scaling of Ethernet device. */
int
nfp_net_reta_query(struct rte_eth_dev *dev,
		   struct rte_eth_rss_reta_entry64 *reta_conf,
		   uint16_t reta_size)
{
	uint8_t i, j, mask;
	int idx, shift;
	uint32_t reta;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if ((hw->ctrl & NFP_NET_CFG_CTRL_RSS_ANY) == 0)
		return -EINVAL;

	if (reta_size != NFP_NET_CFG_RSS_ITBL_SZ) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number hardware can supported "
			"(%d)", reta_size, NFP_NET_CFG_RSS_ITBL_SZ);
		return -EINVAL;
	}

	/*
	 * Reading Redirection Table. There are 128 8bit-entries which can be
	 * manage as 32 32bit-entries
	 */
	for (i = 0; i < reta_size; i += 4) {
		/* Handling 4 RSS entries per loop */
		idx = i / RTE_ETH_RETA_GROUP_SIZE;
		shift = i % RTE_ETH_RETA_GROUP_SIZE;
		mask = (uint8_t)((reta_conf[idx].mask >> shift) & 0xF);

		if (mask == 0)
			continue;

		reta = nn_cfg_readl(hw, NFP_NET_CFG_RSS_ITBL + (idx * 64) +
				    shift);
		for (j = 0; j < 4; j++) {
			if ((mask & (0x1 << j)) == 0)
				continue;
			reta_conf[idx].reta[shift + j] =
				(uint8_t)((reta >> (8 * j)) & 0xF);
		}
	}
	return 0;
}

static int
nfp_net_rss_hash_write(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct nfp_net_hw *hw;
	uint64_t rss_hf;
	uint32_t cfg_rss_ctrl = 0;
	uint8_t key;
	int i;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Writing the key byte a byte */
	for (i = 0; i < rss_conf->rss_key_len; i++) {
		memcpy(&key, &rss_conf->rss_key[i], 1);
		nn_cfg_writeb(hw, NFP_NET_CFG_RSS_KEY + i, key);
	}

	rss_hf = rss_conf->rss_hf;

	if (rss_hf & RTE_ETH_RSS_IPV4)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4;

	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4_TCP;

	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4_UDP;

	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_SCTP)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV4_SCTP;

	if (rss_hf & RTE_ETH_RSS_IPV6)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6;

	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6_TCP;

	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6_UDP;

	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_SCTP)
		cfg_rss_ctrl |= NFP_NET_CFG_RSS_IPV6_SCTP;

	cfg_rss_ctrl |= NFP_NET_CFG_RSS_MASK;
	cfg_rss_ctrl |= NFP_NET_CFG_RSS_TOEPLITZ;

	/* configuring where to apply the RSS hash */
	nn_cfg_writel(hw, NFP_NET_CFG_RSS_CTRL, cfg_rss_ctrl);

	/* Writing the key size */
	nn_cfg_writeb(hw, NFP_NET_CFG_RSS_KEY_SZ, rss_conf->rss_key_len);

	return 0;
}

int
nfp_net_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	uint32_t update;
	uint64_t rss_hf;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	rss_hf = rss_conf->rss_hf;

	/* Checking if RSS is enabled */
	if ((hw->ctrl & NFP_NET_CFG_CTRL_RSS_ANY) == 0) {
		if (rss_hf != 0) { /* Enable RSS? */
			PMD_DRV_LOG(ERR, "RSS unsupported");
			return -EINVAL;
		}
		return 0; /* Nothing to do */
	}

	if (rss_conf->rss_key_len > NFP_NET_CFG_RSS_KEY_SZ) {
		PMD_DRV_LOG(ERR, "hash key too long");
		return -EINVAL;
	}

	nfp_net_rss_hash_write(dev, rss_conf);

	update = NFP_NET_CFG_UPDATE_RSS;

	if (nfp_net_reconfig(hw, hw->ctrl, update) < 0)
		return -EIO;

	return 0;
}

int
nfp_net_rss_hash_conf_get(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	uint64_t rss_hf;
	uint32_t cfg_rss_ctrl;
	uint8_t key;
	int i;
	struct nfp_net_hw *hw;

	hw = NFP_NET_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if ((hw->ctrl & NFP_NET_CFG_CTRL_RSS_ANY) == 0)
		return -EINVAL;

	rss_hf = rss_conf->rss_hf;
	cfg_rss_ctrl = nn_cfg_readl(hw, NFP_NET_CFG_RSS_CTRL);

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV4)
		rss_hf |= RTE_ETH_RSS_IPV4;

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV4_TCP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_TCP;

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV6_TCP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_TCP;

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV4_UDP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_UDP;

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV6_UDP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_UDP;

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV6)
		rss_hf |= RTE_ETH_RSS_IPV6;

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV4_SCTP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_SCTP;

	if (cfg_rss_ctrl & NFP_NET_CFG_RSS_IPV6_SCTP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_SCTP;

	/* Propagate current RSS hash functions to caller */
	rss_conf->rss_hf = rss_hf;

	/* Reading the key size */
	rss_conf->rss_key_len = nn_cfg_readl(hw, NFP_NET_CFG_RSS_KEY_SZ);

	/* Reading the key byte a byte */
	for (i = 0; i < rss_conf->rss_key_len; i++) {
		key = nn_cfg_readb(hw, NFP_NET_CFG_RSS_KEY + i);
		memcpy(&rss_conf->rss_key[i], &key, 1);
	}

	return 0;
}

int
nfp_net_rss_config_default(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *dev_conf;
	struct rte_eth_rss_conf rss_conf;
	struct rte_eth_rss_reta_entry64 nfp_reta_conf[2];
	uint16_t rx_queues = dev->data->nb_rx_queues;
	uint16_t queue;
	int i, j, ret;

	PMD_DRV_LOG(INFO, "setting default RSS conf for %u queues",
		rx_queues);

	nfp_reta_conf[0].mask = ~0x0;
	nfp_reta_conf[1].mask = ~0x0;

	queue = 0;
	for (i = 0; i < 0x40; i += 8) {
		for (j = i; j < (i + 8); j++) {
			nfp_reta_conf[0].reta[j] = queue;
			nfp_reta_conf[1].reta[j] = queue++;
			queue %= rx_queues;
		}
	}
	ret = nfp_net_rss_reta_write(dev, nfp_reta_conf, 0x80);
	if (ret != 0)
		return ret;

	dev_conf = &dev->data->dev_conf;
	if (dev_conf == NULL) {
		PMD_DRV_LOG(INFO, "wrong rss conf");
		return -EINVAL;
	}
	rss_conf = dev_conf->rx_adv_conf.rss_conf;

	ret = nfp_net_rss_hash_write(dev, &rss_conf);

	return ret;
}

void
nfp_net_stop_rx_queue(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_rxq *this_rx_q;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		this_rx_q = (struct nfp_net_rxq *)dev->data->rx_queues[i];
		nfp_net_reset_rx_queue(this_rx_q);
	}
}

void
nfp_net_close_rx_queue(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_rxq *this_rx_q;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		this_rx_q = (struct nfp_net_rxq *)dev->data->rx_queues[i];
		nfp_net_reset_rx_queue(this_rx_q);
		nfp_net_rx_queue_release(dev, i);
	}
}

void
nfp_net_stop_tx_queue(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_txq *this_tx_q;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		this_tx_q = (struct nfp_net_txq *)dev->data->tx_queues[i];
		nfp_net_reset_tx_queue(this_tx_q);
	}
}

void
nfp_net_close_tx_queue(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_net_txq *this_tx_q;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		this_tx_q = (struct nfp_net_txq *)dev->data->tx_queues[i];
		nfp_net_reset_tx_queue(this_tx_q);
		nfp_net_tx_queue_release(dev, i);
	}
}

int
nfp_net_set_vxlan_port(struct nfp_net_hw *hw,
		size_t idx,
		uint16_t port)
{
	int ret;
	uint32_t i;

	if (idx >= NFP_NET_N_VXLAN_PORTS) {
		PMD_DRV_LOG(ERR, "The idx value is out of range.");
		return -ERANGE;
	}

	hw->vxlan_ports[idx] = port;

	for (i = 0; i < NFP_NET_N_VXLAN_PORTS; i += 2) {
		nn_cfg_writel(hw, NFP_NET_CFG_VXLAN_PORT + i * sizeof(port),
			(hw->vxlan_ports[i + 1] << 16) | hw->vxlan_ports[i]);
	}

	rte_spinlock_lock(&hw->reconfig_lock);

	nn_cfg_writel(hw, NFP_NET_CFG_UPDATE, NFP_NET_CFG_UPDATE_VXLAN);
	rte_wmb();

	ret = __nfp_net_reconfig(hw, NFP_NET_CFG_UPDATE_VXLAN);

	rte_spinlock_unlock(&hw->reconfig_lock);

	return ret;
}

/*
 * The firmware with NFD3 can not handle DMA address requiring more
 * than 40 bits
 */
int
nfp_net_check_dma_mask(struct nfp_net_hw *hw, char *name)
{
	if (NFD_CFG_CLASS_VER_of(hw->ver) == NFP_NET_CFG_VERSION_DP_NFD3 &&
			rte_mem_check_dma_mask(40) != 0) {
		PMD_DRV_LOG(ERR,
			"The device %s can't be used: restricted dma mask to 40 bits!",
			name);
		return -ENODEV;
	}

	return 0;
}

void
nfp_net_init_metadata_format(struct nfp_net_hw *hw)
{
	/*
	 * ABI 4.x and ctrl vNIC always use chained metadata, in other cases we allow use of
	 * single metadata if only RSS(v1) is supported by hw capability, and RSS(v2)
	 * also indicate that we are using chained metadata.
	 */
	if (NFD_CFG_MAJOR_VERSION_of(hw->ver) == 4) {
		hw->meta_format = NFP_NET_METAFORMAT_CHAINED;
	} else if ((hw->cap & NFP_NET_CFG_CTRL_CHAIN_META) != 0) {
		hw->meta_format = NFP_NET_METAFORMAT_CHAINED;
		/*
		 * RSS is incompatible with chained metadata. hw->cap just represents
		 * firmware's ability rather than the firmware's configuration. We decide
		 * to reduce the confusion to allow us can use hw->cap to identify RSS later.
		 */
		hw->cap &= ~NFP_NET_CFG_CTRL_RSS;
	} else {
		hw->meta_format = NFP_NET_METAFORMAT_SINGLE;
	}
}

/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */
