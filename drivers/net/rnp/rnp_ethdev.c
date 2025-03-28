/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include <ethdev_pci.h>
#include <ethdev_driver.h>
#include <rte_io.h>
#include <rte_malloc.h>

#include "rnp.h"
#include "rnp_logs.h"
#include "base/rnp_mbx.h"
#include "base/rnp_fw_cmd.h"
#include "base/rnp_mbx_fw.h"
#include "base/rnp_mac.h"
#include "base/rnp_eth_regs.h"
#include "base/rnp_common.h"
#include "base/rnp_dma_regs.h"
#include "base/rnp_mac_regs.h"
#include "rnp_rxtx.h"
#include "rnp_rss.h"

static struct rte_eth_dev *
rnp_alloc_eth_port(struct rte_pci_device *pci, char *name)
{
	struct rte_eth_dev *eth_dev = NULL;
	struct rnp_eth_port *port = NULL;

	eth_dev = rte_eth_dev_allocate(name);
	if (!eth_dev) {
		RNP_PMD_ERR("Could not allocate eth_dev for %s", name);
		return NULL;
	}
	port = rte_zmalloc_socket(name,
			sizeof(*port),
			RTE_CACHE_LINE_SIZE,
			pci->device.numa_node);
	if (!port) {
		RNP_PMD_ERR("Could not allocate rnp_eth_port for %s", name);
		goto fail_calloc;
	}
	rte_eth_copy_pci_info(eth_dev, pci);
	eth_dev->data->dev_private = port;
	eth_dev->device = &pci->device;

	return eth_dev;
fail_calloc:
	rte_free(port);
	rte_eth_dev_release_port(eth_dev);

	return NULL;
}

static void rnp_dev_interrupt_handler(void *param)
{
	RTE_SET_USED(param);
}

static void rnp_mac_rx_enable(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t mac_cfg;

	rte_spinlock_lock(&port->rx_mac_lock);
	mac_cfg = RNP_MAC_REG_RD(hw, lane, RNP_MAC_RX_CFG);
	mac_cfg |= RNP_MAC_RE;

	mac_cfg &= ~RNP_MAC_GPSL_MASK;
	mac_cfg |= (RNP_MAC_MAX_GPSL << RNP_MAC_CPSL_SHIFT);
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_RX_CFG, mac_cfg);
	rte_spinlock_unlock(&port->rx_mac_lock);
}

static void rnp_mac_rx_disable(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t mac_cfg;

	/* to protect conflict hw resource */
	rte_spinlock_lock(&port->rx_mac_lock);
	mac_cfg = RNP_MAC_REG_RD(hw, lane, RNP_MAC_RX_CFG);
	mac_cfg &= ~RNP_MAC_RE;

	RNP_MAC_REG_WR(hw, lane, RNP_MAC_RX_CFG, mac_cfg);
	rte_spinlock_unlock(&port->rx_mac_lock);
}

static void rnp_mac_tx_enable(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t mac_cfg;

	mac_cfg = RNP_MAC_REG_RD(hw, lane, RNP_MAC_TX_CFG);
	mac_cfg |= RNP_MAC_TE;
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_TX_CFG, mac_cfg);
}

static void rnp_mac_tx_disable(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t ctrl;

	/* must wait for tx side has send finish
	 * before fisable tx side
	 */
	ctrl = RNP_MAC_REG_RD(hw, lane, RNP_MAC_TX_CFG);
	ctrl &= ~RNP_MAC_TE;
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_TX_CFG, ctrl);
}

static void rnp_mac_init(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint16_t lane = port->attr.nr_lane;
	struct rnp_hw *hw = port->hw;
	uint32_t mac_cfg;

	rnp_mac_tx_enable(dev);
	rnp_mac_rx_enable(dev);

	mac_cfg = RNP_MAC_REG_RD(hw, lane, RNP_MAC_LPI_CTRL);
	mac_cfg |= RNP_MAC_PLSDIS | RNP_MAC_PLS;
	RNP_MAC_REG_WR(hw, lane, RNP_MAC_LPI_CTRL, mac_cfg);
}

static int
rnp_rx_scattered_setup(struct rte_eth_dev *dev)
{
	uint16_t max_pkt_size =
		dev->data->dev_conf.rxmode.mtu + RNP_ETH_OVERHEAD;
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	struct rnp_hw *hw = port->hw;
	struct rnp_rx_queue *rxq;
	uint16_t dma_buf_size;
	uint16_t queue_id;
	uint32_t dma_ctrl;

	if (dev->data->rx_queues == NULL)
		return -ENOMEM;
	for (queue_id = 0; queue_id < dev->data->nb_rx_queues; queue_id++) {
		rxq = dev->data->rx_queues[queue_id];
		if (!rxq)
			continue;
		if (hw->min_dma_size == 0)
			hw->min_dma_size = rxq->rx_buf_len;
		else
			hw->min_dma_size = RTE_MIN(hw->min_dma_size,
					rxq->rx_buf_len);
	}
	if (hw->min_dma_size < RNP_MIN_DMA_BUF_SIZE) {
		RNP_PMD_ERR("port[%d] scatter dma len is not support %d",
				dev->data->port_id, hw->min_dma_size);
		return -ENOTSUP;
	}
	dma_buf_size = hw->min_dma_size;
	/* Setup max dma scatter engine split size */
	if (max_pkt_size == dma_buf_size)
		dma_buf_size += (dma_buf_size % 16);
	RNP_PMD_INFO("PF[%d] MaxPktLen %d MbSize %d MbHeadRoom %d",
			hw->mbx.pf_num, max_pkt_size,
			dma_buf_size, RTE_PKTMBUF_HEADROOM);
	dma_ctrl = RNP_E_REG_RD(hw,  RNP_DMA_CTRL);
	dma_ctrl &= ~RNP_DMA_SCATTER_MEM_MASK;
	dma_ctrl |= ((dma_buf_size / 16) << RNP_DMA_SCATTER_MEN_S);
	RNP_E_REG_WR(hw, RNP_DMA_CTRL, dma_ctrl);

	return 0;
}

static int rnp_enable_all_rx_queue(struct rte_eth_dev *dev)
{
	struct rnp_rx_queue *rxq;
	uint16_t idx;
	int ret = 0;

	for (idx = 0; idx < dev->data->nb_rx_queues; idx++) {
		rxq = dev->data->rx_queues[idx];
		if (!rxq || rxq->rx_deferred_start)
			continue;
		if (dev->data->rx_queue_state[idx] ==
				RTE_ETH_QUEUE_STATE_STOPPED) {
			ret = rnp_rx_queue_start(dev, idx);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static int rnp_enable_all_tx_queue(struct rte_eth_dev *dev)
{
	struct rnp_tx_queue *txq;
	uint16_t idx;
	int ret = 0;

	for (idx = 0; idx < dev->data->nb_tx_queues; idx++) {
		txq = dev->data->tx_queues[idx];
		if (!txq || txq->tx_deferred_start)
			continue;
		if (dev->data->tx_queue_state[idx] ==
				RTE_ETH_QUEUE_STATE_STOPPED) {
			ret = rnp_tx_queue_start(dev, idx);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static int rnp_dev_start(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct rnp_hw *hw = port->hw;
	uint16_t lane = 0;
	uint16_t idx = 0;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	lane = port->attr.nr_lane;
	ret = rnp_clock_valid_check(hw, lane);
	if (ret) {
		RNP_PMD_ERR("port[%d] function[%d] lane[%d] hw clock error",
				data->port_id, hw->mbx.pf_num, lane);
		return ret;
	}
	/* disable eth rx flow */
	RNP_RX_ETH_DISABLE(hw, lane);
	ret = rnp_dev_rss_configure(eth_dev);
	if (ret)
		return ret;
	ret = rnp_rx_scattered_setup(eth_dev);
	if (ret)
		return ret;
	ret = rnp_enable_all_tx_queue(eth_dev);
	if (ret)
		goto txq_start_failed;
	ret = rnp_enable_all_rx_queue(eth_dev);
	if (ret)
		goto rxq_start_failed;
	rnp_mac_init(eth_dev);
	/* enable eth rx flow */
	RNP_RX_ETH_ENABLE(hw, lane);
	port->port_stopped = 0;

	return 0;
rxq_start_failed:
	for (idx = 0; idx < data->nb_rx_queues; idx++)
		rnp_rx_queue_stop(eth_dev, idx);
txq_start_failed:
	for (idx = 0; idx < data->nb_tx_queues; idx++)
		rnp_tx_queue_stop(eth_dev, idx);

	return ret;
}

static int rnp_disable_all_rx_queue(struct rte_eth_dev *dev)
{
	struct rnp_rx_queue *rxq;
	uint16_t idx;
	int ret = 0;

	for (idx = 0; idx < dev->data->nb_rx_queues; idx++) {
		rxq = dev->data->rx_queues[idx];
		if (!rxq || rxq->rx_deferred_start)
			continue;
		if (dev->data->rx_queue_state[idx] ==
				RTE_ETH_QUEUE_STATE_STARTED) {
			ret = rnp_rx_queue_stop(dev, idx);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static int rnp_disable_all_tx_queue(struct rte_eth_dev *dev)
{
	struct rnp_tx_queue *txq;
	uint16_t idx;
	int ret = 0;

	for (idx = 0; idx < dev->data->nb_tx_queues; idx++) {
		txq = dev->data->tx_queues[idx];
		if (!txq || txq->tx_deferred_start)
			continue;
		if (dev->data->tx_queue_state[idx] ==
				RTE_ETH_QUEUE_STATE_STARTED) {
			ret = rnp_tx_queue_stop(dev, idx);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static int rnp_dev_configure(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);

	if (port->last_rx_num != eth_dev->data->nb_rx_queues)
		port->rxq_num_changed = true;
	else
		port->rxq_num_changed = false;
	port->last_rx_num = eth_dev->data->nb_rx_queues;

	return 0;
}

static int rnp_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);
	const struct rte_eth_dev_data *data = eth_dev->data;
	struct rte_eth_link link;
	int ret;

	if (port->port_stopped)
		return 0;
	eth_dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	eth_dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
	eth_dev->tx_pkt_prepare = rte_eth_pkt_burst_dummy;

	/* clear the recorded link status */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(eth_dev, &link);

	ret = rnp_disable_all_tx_queue(eth_dev);
	if (ret < 0) {
		RNP_PMD_ERR("port[%u] disable tx queue failed", data->port_id);
		return ret;
	}
	ret = rnp_disable_all_rx_queue(eth_dev);
	if (ret < 0) {
		RNP_PMD_ERR("port[%u] disable rx queue failed", data->port_id);
		return ret;
	}
	rnp_mac_tx_disable(eth_dev);
	rnp_mac_rx_disable(eth_dev);

	eth_dev->data->dev_started = 0;
	port->port_stopped = 1;

	return 0;
}

static int rnp_dev_close(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_adapter *adapter = RNP_DEV_TO_ADAPTER(eth_dev);
	int ret = 0;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;
	ret = rnp_dev_stop(eth_dev);
	if (ret < 0)
		return ret;
	if (adapter->closed_ports == adapter->inited_ports) {
		struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI((void *)eth_dev->device);
		if (adapter->intr_registered) {
			/* disable uio irq before callback unregister */
			rte_intr_disable(pci_dev->intr_handle);
			rte_intr_callback_unregister(pci_dev->intr_handle,
					rnp_dev_interrupt_handler,
					(void *)eth_dev);
			adapter->intr_registered = false;
		}
		rnp_dma_mem_free(&adapter->hw, &adapter->hw.fw_info.mem);
		rte_free(adapter);
	}
	adapter->closed_ports++;

	return 0;
}

static uint32_t
rnp_get_speed_caps(struct rte_eth_dev *dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	uint32_t speed_cap = 0;
	uint32_t i = 0, speed;
	uint32_t support_link;
	uint32_t link_types;

	support_link = port->attr.phy_meta.supported_link;
	link_types = rte_popcount64(support_link);
	if (!link_types)
		return 0;
	for (i = 0; i < link_types; i++) {
		speed = ffs(support_link) - 1;
		switch (RTE_BIT32(speed)) {
		case RNP_SPEED_CAP_10M_FULL:
			speed_cap |= RTE_ETH_LINK_SPEED_10M;
			break;
		case RNP_SPEED_CAP_100M_FULL:
			speed_cap |= RTE_ETH_LINK_SPEED_100M;
			break;
		case RNP_SPEED_CAP_1GB_FULL:
			speed_cap |= RTE_ETH_LINK_SPEED_1G;
			break;
		case RNP_SPEED_CAP_10GB_FULL:
			speed_cap |= RTE_ETH_LINK_SPEED_10G;
			break;
		case RNP_SPEED_CAP_40GB_FULL:
			speed_cap |= RTE_ETH_LINK_SPEED_40G;
			break;
		case RNP_SPEED_CAP_25GB_FULL:
			speed_cap |= RTE_ETH_LINK_SPEED_25G;
			break;
		case RNP_SPEED_CAP_10M_HALF:
			speed_cap |= RTE_ETH_LINK_SPEED_10M_HD;
			break;
		case RNP_SPEED_CAP_100M_HALF:
			speed_cap |= RTE_ETH_LINK_SPEED_100M_HD;
			break;
		default:
			speed_cap |= 0;
		}
		support_link &= ~RTE_BIT32(speed);
	}
	if (!port->attr.phy_meta.link_autoneg)
		speed_cap |= RTE_ETH_LINK_SPEED_FIXED;

	return speed_cap;
}

static int rnp_dev_infos_get(struct rte_eth_dev *eth_dev,
			     struct rte_eth_dev_info *dev_info)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);

	PMD_INIT_FUNC_TRACE();

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim){
		.nb_max = RNP_MAX_BD_COUNT,
		.nb_min = RNP_MIN_BD_COUNT,
		.nb_align = RNP_BD_ALIGN,
		.nb_seg_max = RNP_RX_MAX_SEG,
		.nb_mtu_seg_max = RNP_RX_MAX_MTU_SEG,
	};
	dev_info->tx_desc_lim = (struct rte_eth_desc_lim){
		.nb_max = RNP_MAX_BD_COUNT,
		.nb_min = RNP_MIN_BD_COUNT,
		.nb_align = RNP_BD_ALIGN,
		.nb_seg_max = RNP_TX_MAX_SEG,
		.nb_mtu_seg_max = RNP_TX_MAX_MTU_SEG,
	};

	dev_info->max_rx_pktlen = RNP_MAC_MAXFRM_SIZE;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;
	dev_info->max_mtu = dev_info->max_rx_pktlen - RNP_ETH_OVERHEAD;
	dev_info->min_rx_bufsize = RNP_MIN_DMA_BUF_SIZE;
	dev_info->max_rx_queues = port->attr.max_rx_queues;
	dev_info->max_tx_queues = port->attr.max_tx_queues;
	/* mac filter info */
	dev_info->max_mac_addrs = port->attr.max_mac_addrs;
	dev_info->max_hash_mac_addrs = port->attr.max_uc_mac_hash;
	/* for RSS offload just support four tuple */
	dev_info->flow_type_rss_offloads = RNP_SUPPORT_RSS_OFFLOAD_ALL;
	dev_info->hash_key_size = RNP_MAX_HASH_KEY_SIZE * sizeof(uint32_t);
	dev_info->reta_size = RNP_RSS_INDIR_SIZE;
	/* speed cap info */
	dev_info->speed_capa = rnp_get_speed_caps(eth_dev);
	/* default ring configure */
	dev_info->default_rxportconf.burst_size = 32;
	dev_info->default_txportconf.burst_size = 32;
	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_rxportconf.ring_size = 256;
	dev_info->default_txportconf.ring_size = 256;
	/* default port configure */
	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.offloads = 0,
	};

	return 0;
}

static int rnp_promiscuous_enable(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);

	PMD_INIT_FUNC_TRACE();

	return rnp_update_mpfm(port, RNP_MPF_MODE_PROMISC, 1);
}

static int rnp_promiscuous_disable(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);

	PMD_INIT_FUNC_TRACE();

	return rnp_update_mpfm(port, RNP_MPF_MODE_PROMISC, 0);
}

static int rnp_allmulticast_enable(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);

	PMD_INIT_FUNC_TRACE();

	return rnp_update_mpfm(port, RNP_MPF_MODE_ALLMULTI, 1);
}

static int rnp_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);

	PMD_INIT_FUNC_TRACE();
	if (eth_dev->data->promiscuous == 1)
		return 0;
	return rnp_update_mpfm(port, RNP_MPF_MODE_ALLMULTI, 0);
}

/* Features supported by this driver */
static const struct eth_dev_ops rnp_eth_dev_ops = {
	.dev_configure                = rnp_dev_configure,
	.dev_close                    = rnp_dev_close,
	.dev_start                    = rnp_dev_start,
	.dev_stop                     = rnp_dev_stop,
	.dev_infos_get                = rnp_dev_infos_get,

	/* PROMISC */
	.promiscuous_enable           = rnp_promiscuous_enable,
	.promiscuous_disable          = rnp_promiscuous_disable,
	.allmulticast_enable          = rnp_allmulticast_enable,
	.allmulticast_disable         = rnp_allmulticast_disable,

	.rx_queue_setup               = rnp_rx_queue_setup,
	.rx_queue_release             = rnp_dev_rx_queue_release,
	.rx_queue_stop                = rnp_rx_queue_stop,
	.rx_queue_start               = rnp_rx_queue_start,
	.tx_queue_setup               = rnp_tx_queue_setup,
	.tx_queue_release             = rnp_dev_tx_queue_release,
	.tx_queue_stop                = rnp_tx_queue_stop,
	.tx_queue_start               = rnp_tx_queue_start,
	/* rss impl */
	.reta_update                  = rnp_dev_rss_reta_update,
	.reta_query                   = rnp_dev_rss_reta_query,
	.rss_hash_update              = rnp_dev_rss_hash_update,
	.rss_hash_conf_get            = rnp_dev_rss_hash_conf_get,
};

static void
rnp_setup_port_attr(struct rnp_eth_port *port,
		    struct rte_eth_dev *eth_dev,
		    uint8_t sw_id)
{
	struct rnp_port_attr *attr = &port->attr;
	struct rnp_hw *hw = port->hw;
	uint32_t lane;

	PMD_INIT_FUNC_TRACE();

	lane = hw->phy_port_ids[sw_id] & (hw->max_port_num - 1);
	attr->port_id = eth_dev->data->port_id;
	attr->port_offset = RNP_E_REG_RD(hw, RNP_TC_PORT_OFFSET(lane));
	attr->nr_lane = lane;
	attr->sw_id = sw_id;

	attr->max_rx_queues = RNP_MAX_RX_QUEUE_NUM / hw->max_port_num;
	attr->max_tx_queues = RNP_MAX_TX_QUEUE_NUM / hw->max_port_num;

	attr->max_mac_addrs = RNP_MAX_MAC_ADDRS;
	attr->max_uc_mac_hash = RNP_MAX_HASH_UC_MAC_SIZE;
	attr->max_mc_mac_hash = RNP_MAX_HASH_MC_MAC_SIZE;
	attr->uc_hash_tb_size = RNP_MAX_UC_HASH_TABLE;
	attr->mc_hash_tb_size = RNP_MAC_MC_HASH_TABLE;
	rnp_mbx_fw_get_lane_stat(port);

	RNP_PMD_INFO("PF[%d] SW-ETH-PORT[%d]<->PHY_LANE[%d]",
			hw->mbx.pf_num, sw_id, lane);
}

static int
rnp_init_port_resource(struct rnp_eth_adapter *adapter,
		       struct rte_eth_dev *eth_dev,
		       char *name,
		       uint8_t p_id)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);
	struct rte_pci_device *pci_dev = adapter->pdev;
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE] = " ";

	PMD_INIT_FUNC_TRACE();

	port->eth_dev = eth_dev;
	port->hw = &adapter->hw;

	eth_dev->dev_ops = &rnp_eth_dev_ops;
	eth_dev->device = &pci_dev->device;
	eth_dev->data->mtu = RTE_ETHER_MTU;

	rnp_setup_port_attr(port, eth_dev, p_id);
	eth_dev->data->mac_addrs = rte_zmalloc(name,
			sizeof(struct rte_ether_addr) *
			port->attr.max_mac_addrs, 0);
	if (!eth_dev->data->mac_addrs) {
		RNP_PMD_ERR("zmalloc for mac failed! Exiting.");
		return -ENOMEM;
	}
	rnp_get_mac_addr(port, port->mac_addr.addr_bytes);
	rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					&port->mac_addr);
	RNP_PMD_INFO("get mac addr from firmware %s", mac_str);
	if (!rte_is_valid_assigned_ether_addr(&port->mac_addr)) {
		RNP_PMD_WARN("get mac_addr is invalid, just use random");
		rte_eth_random_addr(port->mac_addr.addr_bytes);
	}
	rte_ether_addr_copy(&port->mac_addr, &eth_dev->data->mac_addrs[0]);

	rte_spinlock_init(&port->rx_mac_lock);
	adapter->ports[p_id] = port;
	adapter->inited_ports++;

	return 0;
}

static int
rnp_proc_priv_init(struct rte_eth_dev *dev)
{
	struct rnp_proc_priv *priv;

	priv = rte_zmalloc_socket("rnp_proc_priv",
			sizeof(struct rnp_proc_priv),
			RTE_CACHE_LINE_SIZE,
			dev->device->numa_node);
	if (!priv)
		return -ENOMEM;
	dev->process_private = priv;

	return 0;
}

static int
rnp_rx_reset_pool_setup(struct rnp_eth_adapter *adapter)
{
	struct rte_eth_dev *eth_dev = adapter->eth_dev;
	char name[RTE_MEMPOOL_NAMESIZE];

	snprintf(name, sizeof(name), "rx_reset_pool_%d:%d",
			eth_dev->data->port_id, eth_dev->device->numa_node);

	adapter->reset_pool = rte_pktmbuf_pool_create(name, 2,
			0, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
			eth_dev->device->numa_node);
	if (adapter->reset_pool == NULL) {
		RNP_PMD_ERR("mempool %s create failed", name);
		return -ENOMEM;
	}

	return 0;
}

static int
rnp_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI((void *)eth_dev->device);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);
	char name[RTE_ETH_NAME_MAX_LEN] = " ";
	struct rnp_eth_adapter *adapter;
	struct rte_eth_dev *sub_eth_dev;
	struct rnp_hw *hw;
	uint16_t p_id;
	int ret = -1;

	PMD_INIT_FUNC_TRACE();

	snprintf(name, sizeof(name), "rnp_adapter_%d", eth_dev->data->port_id);
	adapter = rte_zmalloc(name, sizeof(struct rnp_eth_adapter), 0);
	if (!adapter) {
		RNP_PMD_ERR("rnp_adapter zmalloc mem failed");
		return -ENOMEM;
	}
	hw = &adapter->hw;
	adapter->pdev = pci_dev;
	adapter->eth_dev = eth_dev;
	adapter->ports[0] = port;
	hw->back = (void *)adapter;
	port->eth_dev = eth_dev;
	port->hw = hw;

	hw->e_ctrl = (u8 *)pci_dev->mem_resource[4].addr;
	hw->c_ctrl = (u8 *)pci_dev->mem_resource[0].addr;
	hw->c_blen = pci_dev->mem_resource[0].len;
	hw->device_id = pci_dev->id.device_id;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->mbx.en_vfs = pci_dev->max_vfs;
	if (hw->mbx.en_vfs > hw->max_vfs) {
		ret = -EINVAL;
		RNP_PMD_ERR("sriov vfs max support 64");
		goto free_ad;
	}

	strlcpy(hw->device_name, pci_dev->device.name,
			strlen(pci_dev->device.name) + 1);
	ret = rnp_proc_priv_init(eth_dev);
	if (ret < 0) {
		RNP_PMD_ERR("proc_priv_alloc failed");
		goto free_ad;
	}
	ret = rnp_init_mbx_pf(hw);
	if (ret < 0) {
		RNP_PMD_ERR("mailbox hardware init failed");
		goto free_ad;
	}
	ret = rnp_init_hw(hw);
	if (ret < 0) {
		RNP_PMD_ERR("Hardware initialization failed");
		goto free_ad;
	}
	ret = rnp_setup_common_ops(hw);
	if (ret < 0) {
		RNP_PMD_ERR("hardware common ops setup failed");
		goto free_ad;
	}
	for (p_id = 0; p_id < hw->max_port_num; p_id++) {
		/* port 0 resource has been allocated when probe */
		if (!p_id) {
			sub_eth_dev = eth_dev;
		} else {
			if (strlen(hw->device_name) + 4 > sizeof(name))
				return -EINVAL;
			snprintf(name, sizeof(name),
					"%s_%d", hw->device_name, p_id);
			sub_eth_dev = rnp_alloc_eth_port(pci_dev, name);
			if (!sub_eth_dev) {
				RNP_PMD_ERR("%s sub_eth alloc failed",
						hw->device_name);
				ret = -ENOMEM;
				goto eth_alloc_error;
			}
			ret = rnp_proc_priv_init(sub_eth_dev);
			if (ret < 0) {
				RNP_PMD_ERR("proc_priv_alloc failed");
				goto eth_alloc_error;
			}
			memcpy(sub_eth_dev->process_private,
					eth_dev->process_private,
					sizeof(struct rnp_proc_priv));
		}
		ret = rnp_init_port_resource(adapter, sub_eth_dev, name, p_id);
		if (ret)
			goto eth_alloc_error;
		rnp_mac_rx_disable(sub_eth_dev);
		rnp_mac_tx_disable(sub_eth_dev);
		if (p_id) {
			/* port 0 will be probe by platform */
			rte_eth_dev_probing_finish(sub_eth_dev);
		}
	}
	ret = rnp_rx_reset_pool_setup(adapter);
	if (ret)
		goto eth_alloc_error;
	/* enable link update event interrupt */
	rte_intr_callback_register(intr_handle,
			rnp_dev_interrupt_handler, adapter);
	rte_intr_enable(intr_handle);
	adapter->intr_registered = true;
	hw->fw_info.fw_irq_en = true;

	return 0;

eth_alloc_error:
	for (p_id = 0; p_id < adapter->inited_ports; p_id++) {
		port = adapter->ports[p_id];
		if (!port)
			continue;
		if (port->eth_dev) {
			rnp_dev_close(port->eth_dev);
			/* just release eth_dev allocated by myself */
			if (port->eth_dev != adapter->eth_dev)
				rte_eth_dev_release_port(port->eth_dev);
		}
	}
free_ad:
	if (hw->fw_info.cookie_pool)
		rnp_dma_mem_free(hw, &hw->fw_info.mem);
	rte_free(adapter);

	return ret;

}

static int
rnp_eth_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI((void *)eth_dev->device);
	uint16_t port_id;
	int err = 0;

	/* Free up other ports and all resources */
	RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device)
		err |= rte_eth_dev_close(port_id);

	return err == 0 ? 0 : -EIO;
}

static int
rnp_pci_remove(struct rte_pci_device *pci_dev)
{
	char device_name[RTE_ETH_NAME_MAX_LEN] = "";
	struct rte_eth_dev *eth_dev = NULL;
	uint16_t idx = 0;

	/* Find a port belong to pf that not be called dev_close */
	for (idx = 0; idx < RNP_MAX_PORT_OF_PF; idx++) {
		if (idx)
			snprintf(device_name, sizeof(device_name), "%s_%d",
					pci_dev->device.name,
					idx);
		else
			snprintf(device_name, sizeof(device_name), "%s",
					pci_dev->device.name);
		eth_dev = rte_eth_dev_allocated(device_name);
		if (eth_dev)
			break;
	}
	if (eth_dev)
		/* Cleanup eth dev */
		return rnp_eth_dev_uninit(eth_dev);
	return -ENODEV;
}

static int
rnp_pci_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	int rc;

	RTE_SET_USED(pci_drv);

	rc = rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct rnp_eth_port),
			rnp_eth_dev_init);

	return rc;
}

static const struct rte_pci_id pci_id_rnp_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_MUCSE, RNP_DEV_ID_N10G_X2) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_MUCSE, RNP_DEV_ID_N10G_X4) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_MUCSE, RNP_DEV_ID_N10G_X8) },
	{ .vendor_id = 0, },
};

static struct rte_pci_driver rte_rnp_pmd = {
	.id_table = pci_id_rnp_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = rnp_pci_probe,
	.remove = rnp_pci_remove,
};

RTE_LOG_REGISTER_SUFFIX(rnp_init_logtype, init, NOTICE);

RTE_PMD_REGISTER_PCI(net_rnp, rte_rnp_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_rnp, pci_id_rnp_map);
RTE_PMD_REGISTER_KMOD_DEP(net_rnp, "igb_uio | uio_pci_generic | vfio-pci");
