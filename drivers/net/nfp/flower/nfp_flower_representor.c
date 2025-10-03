/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_flower_representor.h"

#include "../nfd3/nfp_nfd3.h"
#include "../nfpcore/nfp_nsp.h"
#include "../nfp_logs.h"
#include "../nfp_mtr.h"
#include "nfp_flower_service.h"

/* Type of representor */
enum nfp_repr_type {
	NFP_REPR_TYPE_PHYS_PORT,    /*<< External NIC port */
	NFP_REPR_TYPE_PF,           /*<< Physical function */
	NFP_REPR_TYPE_VF,           /*<< Virtual function */
	NFP_REPR_TYPE_MAX,          /*<< Number of representor types */
};

struct nfp_repr_init {
	struct nfp_flower_representor *flower_repr;
	struct nfp_net_hw_priv *hw_priv;
};

bool
nfp_flower_repr_is_pf(struct rte_eth_dev *dev)
{
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_flower_representor *repr;

	hw_priv = dev->process_private;
	repr = dev->data->dev_private;

	if (hw_priv->pf_dev->multi_pf.enabled)
		return repr->repr_type == NFP_REPR_TYPE_PHYS_PORT;
	else
		return repr->repr_type == NFP_REPR_TYPE_PF;
}

static int
nfp_repr_get_eeprom_len(struct rte_eth_dev *dev)
{
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	if (!nfp_flower_repr_is_phy(repr))
		return -EOPNOTSUPP;

	return nfp_net_get_eeprom_len(dev);
}

static int
nfp_repr_get_eeprom(struct rte_eth_dev *dev,
		struct rte_dev_eeprom_info *eeprom)
{
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	if (!nfp_flower_repr_is_phy(repr))
		return -EOPNOTSUPP;

	return nfp_net_get_eeprom(dev, eeprom);
}

static int
nfp_repr_set_eeprom(struct rte_eth_dev *dev,
		struct rte_dev_eeprom_info *eeprom)
{
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	if (!nfp_flower_repr_is_phy(repr))
		return -EOPNOTSUPP;

	return nfp_net_set_eeprom(dev, eeprom);
}

static int
nfp_repr_get_module_info(struct rte_eth_dev *dev,
		struct rte_eth_dev_module_info *info)
{
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	if (!nfp_flower_repr_is_phy(repr))
		return -EOPNOTSUPP;

	return nfp_net_get_module_info(dev, info);
}

static int
nfp_repr_get_module_eeprom(struct rte_eth_dev *dev,
		struct rte_dev_eeprom_info *info)
{
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	if (!nfp_flower_repr_is_phy(repr))
		return -EOPNOTSUPP;

	return nfp_net_get_module_eeprom(dev, info);
}

static int
nfp_flower_repr_led_on(struct rte_eth_dev *dev)
{
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	if (!nfp_flower_repr_is_phy(repr))
		return -EOPNOTSUPP;

	return nfp_net_led_on(dev);
}

static int
nfp_flower_repr_led_off(struct rte_eth_dev *dev)
{
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	if (!nfp_flower_repr_is_phy(repr))
		return -EOPNOTSUPP;

	return nfp_net_led_off(dev);
}

int
nfp_flower_repr_link_update(struct rte_eth_dev *dev,
		__rte_unused int wait_to_complete)
{
	int ret;
	struct rte_eth_link *link;
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	link = &repr->link;

	ret = nfp_net_link_update_common(dev, link, link->link_status);

	if (nfp_flower_repr_is_pf(dev))
		nfp_net_notify_port_speed(repr->app_fw_flower->pf_hw, link);

	return ret;
}

static int
nfp_flower_repr_dev_infos_get(__rte_unused struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct nfp_net_hw *pf_hw;
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	pf_hw = repr->app_fw_flower->pf_hw;

	/* Hardcoded pktlen and queues for now */
	dev_info->max_rx_queues = (uint16_t)pf_hw->max_rx_queues;
	dev_info->max_tx_queues = (uint16_t)pf_hw->max_tx_queues;
	dev_info->min_rx_bufsize = RTE_ETHER_MIN_MTU;
	dev_info->max_rx_pktlen = 9000;

	dev_info->rx_offload_capa = RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
			RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
			RTE_ETH_RX_OFFLOAD_TCP_CKSUM;

	dev_info->tx_offload_capa = RTE_ETH_TX_OFFLOAD_VLAN_INSERT;
	dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
			RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
			RTE_ETH_TX_OFFLOAD_TCP_CKSUM;
	dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_TCP_TSO;
	dev_info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	dev_info->max_mac_addrs = 1;

	if ((pf_hw->super.cap & NFP_NET_CFG_CTRL_RSS_ANY) != 0) {
		dev_info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_RSS_HASH;
		dev_info->flow_type_rss_offloads = NFP_NET_RSS_CAP;
		dev_info->reta_size = NFP_NET_CFG_RSS_ITBL_SZ;
		dev_info->hash_key_size = NFP_NET_CFG_RSS_KEY_SZ;
		nfp_net_rss_algo_capa_get(pf_hw, dev_info);
	}

	return 0;
}

static int
nfp_flower_repr_dev_start(struct rte_eth_dev *dev)
{
	int ret;
	uint16_t i;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_flower_representor *repr;
	struct nfp_app_fw_flower *app_fw_flower;

	repr = dev->data->dev_private;
	hw_priv = dev->process_private;
	app_fw_flower = repr->app_fw_flower;

	if (nfp_flower_repr_is_phy(repr)) {
		ret = nfp_eth_set_configured(hw_priv->pf_dev->cpp, repr->nfp_idx, 1);
		if (ret < 0)
			return ret;
	}

	nfp_flower_cmsg_port_mod(app_fw_flower, repr->port_id, true);

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static int
nfp_flower_repr_dev_stop(struct rte_eth_dev *dev)
{
	uint16_t i;
	int ret = 0;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_flower_representor *repr;
	struct nfp_app_fw_flower *app_fw_flower;

	repr = dev->data->dev_private;
	hw_priv = dev->process_private;
	app_fw_flower = repr->app_fw_flower;

	nfp_flower_cmsg_port_mod(app_fw_flower, repr->port_id, false);

	if (nfp_flower_repr_is_phy(repr)) {
		ret = nfp_eth_set_configured(hw_priv->pf_dev->cpp, repr->nfp_idx, 0);
		if (ret == 1)
			ret = 0;
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	return ret;
}

static int
nfp_flower_repr_rx_queue_setup(struct rte_eth_dev *dev,
		uint16_t rx_queue_id,
		uint16_t nb_rx_desc,
		unsigned int socket_id,
		__rte_unused const struct rte_eth_rxconf *rx_conf,
		__rte_unused struct rte_mempool *mb_pool)
{
	struct nfp_net_rxq *rxq;
	struct nfp_net_hw *pf_hw;
	char ring_name[RTE_RING_NAMESIZE];
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	pf_hw = repr->app_fw_flower->pf_hw;

	/* Allocating rx queue data structure */
	rxq = rte_zmalloc_socket("ethdev RX queue", sizeof(struct nfp_net_rxq),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL)
		return -ENOMEM;

	snprintf(ring_name, sizeof(ring_name), "%s-%s-%u", repr->name, "Rx", rx_queue_id);
	repr->ring[rx_queue_id] = rte_ring_create(ring_name, nb_rx_desc,
			rte_socket_id(), 0);
	if (repr->ring[rx_queue_id] == NULL) {
		PMD_DRV_LOG(ERR, "The rte_ring_create failed for rx queue %u.", rx_queue_id);
		rte_free(rxq);
		return -ENOMEM;
	}

	rxq->hw = pf_hw;
	rxq->qidx = rx_queue_id;
	rxq->port_id = dev->data->port_id;
	dev->data->rx_queues[rx_queue_id] = rxq;

	return 0;
}

static int
nfp_flower_repr_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t tx_queue_id,
		__rte_unused uint16_t nb_tx_desc,
		unsigned int socket_id,
		__rte_unused const struct rte_eth_txconf *tx_conf)
{
	struct nfp_net_txq *txq;
	struct nfp_net_hw *pf_hw;
	struct nfp_flower_representor *repr;

	repr = dev->data->dev_private;
	pf_hw = repr->app_fw_flower->pf_hw;

	/* Allocating tx queue data structure */
	txq = rte_zmalloc_socket("ethdev TX queue", sizeof(struct nfp_net_txq),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL)
		return -ENOMEM;

	txq->hw = pf_hw;
	txq->qidx = tx_queue_id;
	txq->port_id = dev->data->port_id;
	dev->data->tx_queues[tx_queue_id] = txq;

	return 0;
}

static int
nfp_flower_repr_stats_get(struct rte_eth_dev *ethdev,
		struct rte_eth_stats *stats, struct eth_queue_stats *qstats __rte_unused)
{
	uint16_t i;
	struct nfp_flower_representor *repr;

	repr = ethdev->data->dev_private;

	repr->repr_stats.ipackets = 0;
	repr->repr_stats.ibytes = 0;
	for (i = 0; i < ethdev->data->nb_rx_queues; i++) {
		repr->repr_stats.ipackets += repr->repr_qstats.q_ipackets[i];
		repr->repr_stats.ibytes += repr->repr_qstats.q_ibytes[i];
	}

	repr->repr_stats.opackets = 0;
	repr->repr_stats.obytes = 0;
	for (i = 0; i < ethdev->data->nb_tx_queues; i++) {
		repr->repr_stats.opackets += repr->repr_qstats.q_opackets[i];
		repr->repr_stats.obytes += repr->repr_qstats.q_obytes[i];
	}

	*stats = repr->repr_stats;

	return 0;
}

int
nfp_flower_repr_stats_reset(struct rte_eth_dev *ethdev)
{
	struct nfp_flower_representor *repr;

	repr = ethdev->data->dev_private;
	memset(&repr->repr_stats, 0, sizeof(struct rte_eth_stats));
	memset(&repr->repr_qstats, 0, sizeof(struct eth_queue_stats));

	return 0;
}

static int
nfp_flower_repr_mac_addr_set(struct rte_eth_dev *ethdev,
		struct rte_ether_addr *mac_addr)
{
	struct nfp_flower_representor *repr;

	repr = ethdev->data->dev_private;
	rte_ether_addr_copy(mac_addr, &repr->mac_addr);
	rte_ether_addr_copy(mac_addr, ethdev->data->mac_addrs);

	return 0;
}

static uint16_t
nfp_flower_repr_rx_burst(void *rx_queue,
		struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	uint32_t i;
	uint32_t data_len;
	unsigned int available = 0;
	unsigned int total_dequeue;
	struct nfp_net_rxq *rxq;
	struct rte_eth_dev *dev;
	struct nfp_flower_representor *repr;

	rxq = rx_queue;
	if (unlikely(rxq == NULL)) {
		PMD_RX_LOG(ERR, "RX Bad queue.");
		return 0;
	}

	dev = &rte_eth_devices[rxq->port_id];
	repr = dev->data->dev_private;
	if (unlikely(repr->ring == NULL) ||
			unlikely(repr->ring[rxq->qidx] == NULL)) {
		PMD_RX_LOG(ERR, "Representor %s has no ring configured!",
				repr->name);
		return 0;
	}

	total_dequeue = rte_ring_dequeue_burst(repr->ring[rxq->qidx],
			(void *)rx_pkts, nb_pkts, &available);
	if (total_dequeue != 0) {
		PMD_RX_LOG(DEBUG, "Port: %#x, queue: %hu received: %u, available: %u.",
				repr->port_id, rxq->qidx, total_dequeue, available);

		data_len = 0;
		for (i = 0; i < total_dequeue; i++)
			data_len += rx_pkts[i]->data_len;

		repr->repr_qstats.q_ipackets[rxq->qidx] += total_dequeue;
		repr->repr_qstats.q_ibytes[rxq->qidx] += data_len;
	}

	return total_dequeue;
}

static uint16_t
nfp_flower_repr_tx_burst(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	uint16_t i;
	uint16_t sent;
	uint32_t data_len;
	void *pf_tx_queue;
	struct nfp_net_txq *txq;
	struct rte_eth_dev *dev;
	struct rte_eth_dev *repr_dev;
	struct nfp_flower_representor *repr;

	txq = tx_queue;
	if (unlikely(txq == NULL)) {
		PMD_TX_LOG(ERR, "TX Bad queue.");
		return 0;
	}

	/* Grab a handle to the representor struct */
	repr_dev = &rte_eth_devices[txq->port_id];
	repr = repr_dev->data->dev_private;

	for (i = 0; i < nb_pkts; i++)
		nfp_flower_pkt_add_metadata(repr->app_fw_flower,
				tx_pkts[i], repr->port_id);

	/* This points to the PF vNIC that owns this representor */
	dev = repr->app_fw_flower->pf_ethdev;

	pf_tx_queue = dev->data->tx_queues[txq->qidx];
	sent = nfp_flower_pf_xmit_pkts(pf_tx_queue, tx_pkts, nb_pkts);
	if (sent != 0) {
		PMD_TX_LOG(DEBUG, "Port: %#x transmitted: %hu queue: %u.",
				repr->port_id, sent, txq->qidx);

		data_len = 0;
		for (i = 0; i < sent; i++)
			data_len += tx_pkts[i]->data_len;

		repr->repr_qstats.q_opackets[txq->qidx] += sent;
		repr->repr_qstats.q_obytes[txq->qidx] += data_len;
	}

	return sent;
}

static void
nfp_flower_repr_free_queue(struct rte_eth_dev *eth_dev)
{
	uint16_t i;
	struct nfp_flower_representor *repr;

	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		rte_free(eth_dev->data->tx_queues[i]);

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		repr = eth_dev->data->dev_private;
		rte_ring_free(repr->ring[i]);
		rte_free(eth_dev->data->rx_queues[i]);
	}
}

static void
nfp_flower_pf_repr_close_queue(struct rte_eth_dev *eth_dev)
{
	/*
	 * We assume that the DPDK application is stopping all the
	 * threads/queues before calling the device close function.
	 */
	nfp_net_disable_queues(eth_dev);

	/* Clear queues */
	nfp_net_close_tx_queue(eth_dev);
	nfp_net_close_rx_queue(eth_dev);
}

static void
nfp_flower_repr_close_queue(struct rte_eth_dev *eth_dev,
		enum nfp_repr_type repr_type)
{
	struct nfp_net_hw_priv *hw_priv;

	switch (repr_type) {
	case NFP_REPR_TYPE_PHYS_PORT:
		hw_priv = eth_dev->process_private;
		if (hw_priv->pf_dev->multi_pf.enabled)
			nfp_flower_pf_repr_close_queue(eth_dev);
		else
			nfp_flower_repr_free_queue(eth_dev);
		break;
	case NFP_REPR_TYPE_PF:
		nfp_flower_pf_repr_close_queue(eth_dev);
		break;
	case NFP_REPR_TYPE_VF:
		nfp_flower_repr_free_queue(eth_dev);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported repr port type.");
		break;
	}
}

static void
nfp_flower_repr_base_uninit(struct nfp_flower_representor *repr)
{
	rte_free(repr->repr_xstats_base);
}

static int
nfp_flower_repr_uninit(struct rte_eth_dev *eth_dev)
{
	uint16_t index;
	struct nfp_flower_representor *repr;

	repr = eth_dev->data->dev_private;
	nfp_flower_repr_base_uninit(repr);
	rte_free(repr->ring);

	if (nfp_flower_repr_is_phy(repr)) {
		index = NFP_FLOWER_CMSG_PORT_PHYS_PORT_NUM(repr->port_id);
		repr->app_fw_flower->phy_reprs[index] = NULL;
	} else {
		index = repr->vf_id;
		repr->app_fw_flower->vf_reprs[index] = NULL;
	}

	return 0;
}

static int
nfp_flower_pf_repr_uninit(struct rte_eth_dev *eth_dev)
{
	struct nfp_flower_representor *repr = eth_dev->data->dev_private;

	repr->app_fw_flower->pf_repr = NULL;

	return 0;
}

static void
nfp_flower_repr_free(struct rte_eth_dev *eth_dev,
		enum nfp_repr_type repr_type)
{
	switch (repr_type) {
	case NFP_REPR_TYPE_PHYS_PORT:
		nfp_flower_repr_uninit(eth_dev);
		break;
	case NFP_REPR_TYPE_PF:
		nfp_flower_pf_repr_uninit(eth_dev);
		break;
	case NFP_REPR_TYPE_VF:
		nfp_flower_repr_uninit(eth_dev);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported repr port type.");
		break;
	}
}

/* Reset and stop device. The device can not be restarted. */
static int
nfp_flower_repr_dev_close(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct nfp_pf_dev *pf_dev;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_flower_representor *repr;
	struct nfp_app_fw_flower *app_fw_flower;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	repr = dev->data->dev_private;
	hw_priv = dev->process_private;

	app_fw_flower = repr->app_fw_flower;
	pf_dev = hw_priv->pf_dev;

	if (pf_dev->app_fw_id != NFP_APP_FW_FLOWER_NIC)
		return -EINVAL;

	nfp_flower_repr_close_queue(dev, repr->repr_type);
	nfp_flower_repr_free(dev, repr->repr_type);

	for (i = 0; i < MAX_FLOWER_VFS; i++) {
		if (app_fw_flower->vf_reprs[i] != NULL)
			return 0;
	}

	for (i = 0; i < NFP_MAX_PHYPORTS; i++) {
		if (app_fw_flower->phy_reprs[i] != NULL)
			return 0;
	}

	if (app_fw_flower->pf_repr != NULL)
		return 0;

	/* Stop flower service first */
	nfp_flower_service_stop(hw_priv);

	/* Now it is safe to free all PF resources */
	nfp_uninit_app_fw_flower(hw_priv);
	nfp_pf_uninit(hw_priv);

	return 0;
}

static const struct eth_dev_ops nfp_flower_pf_repr_dev_ops = {
	.dev_infos_get        = nfp_flower_repr_dev_infos_get,

	.dev_start            = nfp_flower_pf_start,
	.dev_configure        = nfp_net_configure,
	.dev_stop             = nfp_net_stop,
	.dev_close            = nfp_flower_repr_dev_close,

	.rx_queue_setup       = nfp_net_rx_queue_setup,
	.tx_queue_setup       = nfp_net_tx_queue_setup,

	.link_update          = nfp_flower_repr_link_update,

	.stats_get            = nfp_flower_repr_stats_get,
	.stats_reset          = nfp_flower_repr_stats_reset,

	.promiscuous_enable   = nfp_net_promisc_enable,
	.promiscuous_disable  = nfp_net_promisc_disable,

	.mac_addr_set         = nfp_flower_repr_mac_addr_set,
	.fw_version_get       = nfp_net_firmware_version_get,
};

static const struct eth_dev_ops nfp_flower_multiple_pf_repr_dev_ops = {
	.dev_infos_get        = nfp_flower_repr_dev_infos_get,

	.dev_start            = nfp_flower_pf_start,
	.dev_configure        = nfp_net_configure,
	.dev_stop             = nfp_flower_pf_stop,
	.dev_close            = nfp_flower_repr_dev_close,

	.rx_queue_setup       = nfp_net_rx_queue_setup,
	.tx_queue_setup       = nfp_net_tx_queue_setup,

	.link_update          = nfp_flower_repr_link_update,

	.stats_get            = nfp_flower_repr_stats_get,
	.stats_reset          = nfp_flower_repr_stats_reset,

	.promiscuous_enable   = nfp_net_promisc_enable,
	.promiscuous_disable  = nfp_net_promisc_disable,

	.mac_addr_set         = nfp_flower_repr_mac_addr_set,
	.fw_version_get       = nfp_net_firmware_version_get,

	.flow_ops_get         = nfp_flow_ops_get,
	.mtr_ops_get          = nfp_net_mtr_ops_get,

	.xstats_get             = nfp_net_xstats_get,
	.xstats_reset           = nfp_net_xstats_reset,
	.xstats_get_names       = nfp_net_xstats_get_names,
	.xstats_get_by_id       = nfp_net_xstats_get_by_id,
	.xstats_get_names_by_id = nfp_net_xstats_get_names_by_id,

	.get_eeprom_length    = nfp_repr_get_eeprom_len,
	.get_eeprom           = nfp_repr_get_eeprom,
	.set_eeprom           = nfp_repr_set_eeprom,
	.get_module_info      = nfp_repr_get_module_info,
	.get_module_eeprom    = nfp_repr_get_module_eeprom,

	.dev_led_on           = nfp_flower_repr_led_on,
	.dev_led_off          = nfp_flower_repr_led_off,
};

static const struct eth_dev_ops nfp_flower_repr_dev_ops = {
	.dev_infos_get        = nfp_flower_repr_dev_infos_get,

	.dev_start            = nfp_flower_repr_dev_start,
	.dev_configure        = nfp_net_configure,
	.dev_stop             = nfp_flower_repr_dev_stop,
	.dev_close            = nfp_flower_repr_dev_close,

	.rx_queue_setup       = nfp_flower_repr_rx_queue_setup,
	.tx_queue_setup       = nfp_flower_repr_tx_queue_setup,

	.link_update          = nfp_flower_repr_link_update,

	.stats_get            = nfp_flower_repr_stats_get,
	.stats_reset          = nfp_flower_repr_stats_reset,

	.promiscuous_enable   = nfp_net_promisc_enable,
	.promiscuous_disable  = nfp_net_promisc_disable,

	.mac_addr_set         = nfp_flower_repr_mac_addr_set,
	.fw_version_get       = nfp_net_firmware_version_get,

	.flow_ops_get         = nfp_flow_ops_get,
	.mtr_ops_get          = nfp_net_mtr_ops_get,

	.xstats_get             = nfp_net_xstats_get,
	.xstats_reset           = nfp_net_xstats_reset,
	.xstats_get_names       = nfp_net_xstats_get_names,
	.xstats_get_by_id       = nfp_net_xstats_get_by_id,
	.xstats_get_names_by_id = nfp_net_xstats_get_names_by_id,

	.get_eeprom_length    = nfp_repr_get_eeprom_len,
	.get_eeprom           = nfp_repr_get_eeprom,
	.set_eeprom           = nfp_repr_set_eeprom,
	.get_module_info      = nfp_repr_get_module_info,
	.get_module_eeprom    = nfp_repr_get_module_eeprom,

	.dev_led_on           = nfp_flower_repr_led_on,
	.dev_led_off          = nfp_flower_repr_led_off,
};

static uint32_t
nfp_flower_get_phys_port_id(uint8_t port)
{
	return (NFP_FLOWER_CMSG_PORT_TYPE_PHYS_PORT << 28) | port;
}

static uint32_t
nfp_get_pcie_port_id(struct nfp_cpp *cpp,
		int type,
		uint8_t vnic,
		uint8_t queue)
{
	uint8_t nfp_pcie;
	uint32_t port_id;

	nfp_pcie = NFP_CPP_INTERFACE_UNIT_of(nfp_cpp_interface(cpp));
	port_id = ((nfp_pcie & 0x3) << 14) |
			((type & 0x3) << 12) |
			((vnic & 0x3f) << 6) |
			(queue & 0x3f) |
			((NFP_FLOWER_CMSG_PORT_TYPE_PCIE_PORT & 0xf) << 28);

	return port_id;
}

static int
nfp_flower_pf_repr_init(struct rte_eth_dev *eth_dev,
		void *init_params)
{
	struct nfp_flower_representor *repr;
	struct nfp_flower_representor *init_repr_data;

	/* Cast the input representor data to the correct struct here */
	init_repr_data = init_params;

	/* Memory has been allocated in the eth_dev_create() function */
	repr = eth_dev->data->dev_private;

	/* Copy data here from the input representor template */
	repr->vf_id            = init_repr_data->vf_id;
	repr->switch_domain_id = init_repr_data->switch_domain_id;
	repr->repr_type        = init_repr_data->repr_type;
	repr->app_fw_flower    = init_repr_data->app_fw_flower;

	strlcpy(repr->name, init_repr_data->name, sizeof(repr->name));

	eth_dev->dev_ops = &nfp_flower_pf_repr_dev_ops;
	eth_dev->rx_pkt_burst = nfp_net_recv_pkts;
	eth_dev->tx_pkt_burst = nfp_flower_pf_xmit_pkts;
	eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR |
			RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	eth_dev->data->representor_id = 0;

	/* This backer port is that of the eth_device created for the PF vNIC */
	eth_dev->data->backer_port_id = 0;

	/* Allocating memory for mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr", RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for repr MAC.");
		return -ENOMEM;
	}

	rte_ether_addr_copy(&init_repr_data->mac_addr, &repr->mac_addr);
	rte_ether_addr_copy(&init_repr_data->mac_addr, eth_dev->data->mac_addrs);

	repr->app_fw_flower->pf_repr = repr;
	repr->app_fw_flower->pf_ethdev = eth_dev;

	return 0;
}

static int
nfp_flower_repr_base_init(struct rte_eth_dev *eth_dev,
		struct nfp_flower_representor *repr,
		struct nfp_repr_init *repr_init)
{
	int ret;
	struct nfp_flower_representor *init_repr_data;

	/* Cast the input representor data to the correct struct here */
	init_repr_data = repr_init->flower_repr;

	/* Copy data here from the input representor template */
	repr->idx              = init_repr_data->idx;
	repr->vf_id            = init_repr_data->vf_id;
	repr->switch_domain_id = init_repr_data->switch_domain_id;
	repr->port_id          = init_repr_data->port_id;
	repr->nfp_idx          = init_repr_data->nfp_idx;
	repr->repr_type        = init_repr_data->repr_type;
	repr->app_fw_flower    = init_repr_data->app_fw_flower;

	snprintf(repr->name, sizeof(repr->name), "%s", init_repr_data->name);

	/* This backer port is that of the eth_device created for the PF vNIC */
	eth_dev->data->backer_port_id = 0;

	/* Allocating memory for mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr", RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for repr MAC.");
		return -ENOMEM;
	}

	rte_ether_addr_copy(&init_repr_data->mac_addr, &repr->mac_addr);
	rte_ether_addr_copy(&init_repr_data->mac_addr, eth_dev->data->mac_addrs);

	/* Send reify message to hardware to inform it about the new repr */
	ret = nfp_flower_cmsg_repr_reify(init_repr_data->app_fw_flower, repr);
	if (ret != 0) {
		PMD_INIT_LOG(WARNING, "Failed to send repr reify message.");
		goto mac_cleanup;
	}

	/* Allocate memory for extended statistics counters */
	repr->repr_xstats_base = rte_zmalloc("rte_eth_xstat",
			sizeof(struct rte_eth_xstat) * nfp_net_xstats_size(eth_dev), 0);
	if (repr->repr_xstats_base == NULL) {
		PMD_INIT_LOG(ERR, "No memory for xstats base on device %s!", repr->name);
		ret = -ENOMEM;
		goto mac_cleanup;
	}

	return 0;

mac_cleanup:
	rte_free(eth_dev->data->mac_addrs);
	return ret;
}

static int
nfp_flower_repr_init(struct rte_eth_dev *eth_dev,
		void *init_params)
{
	int ret;
	uint16_t index;
	unsigned int numa_node;
	struct nfp_repr_init *repr_init;
	struct nfp_net_hw_priv *hw_priv;
	char ring_name[RTE_ETH_NAME_MAX_LEN];
	struct nfp_app_fw_flower *app_fw_flower;
	struct nfp_flower_representor *repr;
	struct nfp_flower_representor *init_repr_data;

	/* Cast the input representor data to the correct struct here */
	repr_init = init_params;
	init_repr_data = repr_init->flower_repr;
	app_fw_flower = init_repr_data->app_fw_flower;

	/* Memory has been allocated in the eth_dev_create() function */
	repr = eth_dev->data->dev_private;
	hw_priv = repr_init->hw_priv;

	/*
	 * We need multiproduce rings as we can have multiple PF ports.
	 * On the other hand, we need single consumer rings, as just one
	 * representor PMD will try to read from the ring.
	 */
	snprintf(ring_name, sizeof(ring_name), "%s_%s", init_repr_data->name, "ring");
	numa_node = rte_socket_id();
	repr->ring = rte_zmalloc_socket(ring_name,
			sizeof(struct rte_ring *) * app_fw_flower->pf_hw->max_rx_queues,
			RTE_CACHE_LINE_SIZE, numa_node);
	if (repr->ring == NULL) {
		PMD_DRV_LOG(ERR, "Ring create failed for %s.", ring_name);
		return -ENOMEM;
	}

	eth_dev->dev_ops = &nfp_flower_repr_dev_ops;
	eth_dev->rx_pkt_burst = nfp_flower_repr_rx_burst;
	eth_dev->tx_pkt_burst = nfp_flower_repr_tx_burst;
	eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR |
			RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	ret = nfp_flower_repr_base_init(eth_dev, repr, repr_init);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Flower repr base init failed.");
		goto ring_cleanup;
	}

	if (nfp_flower_repr_is_phy(repr))
		eth_dev->data->representor_id = repr->vf_id;
	else
		eth_dev->data->representor_id = repr->vf_id +
				app_fw_flower->num_phyport_reprs + 1;

	/* Add repr to correct array */
	if (nfp_flower_repr_is_phy(repr)) {
		index = NFP_FLOWER_CMSG_PORT_PHYS_PORT_NUM(repr->port_id);
		app_fw_flower->phy_reprs[index] = repr;
	} else {
		index = repr->vf_id;
		app_fw_flower->vf_reprs[index] = repr;
	}

	if (nfp_flower_repr_is_phy(repr)) {
		repr->mac_stats = hw_priv->pf_dev->mac_stats_bar +
				(repr->nfp_idx * NFP_MAC_STATS_SIZE);
	}

	return 0;

ring_cleanup:
	rte_free(repr->ring);

	return ret;
}

static int
nfp_flower_multiple_pf_repr_init(struct rte_eth_dev *eth_dev,
		void *init_params)
{
	int ret;
	uint16_t index;
	struct nfp_repr_init *repr_init;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_flower_representor *repr;
	struct nfp_app_fw_flower *app_fw_flower;

	/* Cast the input representor data to the correct struct here */
	repr_init = init_params;
	app_fw_flower = repr_init->flower_repr->app_fw_flower;

	/* Memory has been allocated in the eth_dev_create() function */
	repr = eth_dev->data->dev_private;
	hw_priv = repr_init->hw_priv;

	eth_dev->dev_ops = &nfp_flower_multiple_pf_repr_dev_ops;
	eth_dev->rx_pkt_burst = nfp_flower_multiple_pf_recv_pkts;
	eth_dev->tx_pkt_burst = nfp_flower_multiple_pf_xmit_pkts;
	eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR |
			RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	ret = nfp_flower_repr_base_init(eth_dev, repr, repr_init);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Flower multiple PF repr base init failed.");
		return -ENOMEM;
	}

	eth_dev->data->representor_id = repr->vf_id;

	/* Add repr to correct array */
	index = NFP_FLOWER_CMSG_PORT_PHYS_PORT_NUM(repr->port_id);
	app_fw_flower->phy_reprs[index] = repr;

	repr->mac_stats = hw_priv->pf_dev->mac_stats_bar +
			(repr->nfp_idx * NFP_MAC_STATS_SIZE);

	app_fw_flower->pf_ethdev = eth_dev;

	return 0;
}

static void
nfp_flower_repr_free_all(struct nfp_app_fw_flower *app_fw_flower)
{
	uint32_t i;
	struct rte_eth_dev *eth_dev;
	struct nfp_flower_representor *repr;

	for (i = 0; i < MAX_FLOWER_VFS; i++) {
		repr = app_fw_flower->vf_reprs[i];
		if (repr != NULL) {
			eth_dev = rte_eth_dev_get_by_name(repr->name);
			if (eth_dev != NULL)
				rte_eth_dev_destroy(eth_dev, nfp_flower_repr_uninit);
		}
	}

	for (i = 0; i < NFP_MAX_PHYPORTS; i++) {
		repr = app_fw_flower->phy_reprs[i];
		if (repr != NULL) {
			eth_dev = rte_eth_dev_get_by_name(repr->name);
			if (eth_dev != NULL)
				rte_eth_dev_destroy(eth_dev, nfp_flower_repr_uninit);
		}
	}

	repr = app_fw_flower->pf_repr;
	if (repr != NULL) {
		eth_dev = rte_eth_dev_get_by_name(repr->name);
		if (eth_dev != NULL)
			rte_eth_dev_destroy(eth_dev, nfp_flower_pf_repr_uninit);
	}
}

static void
nfp_flower_repr_priv_init(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_net_hw_priv *hw_priv)
{
	uint32_t i;
	struct rte_eth_dev *eth_dev;
	struct nfp_flower_representor *repr;

	repr = app_fw_flower->pf_repr;
	if (repr != NULL) {
		eth_dev = rte_eth_dev_get_by_name(repr->name);
		if (eth_dev != NULL)
			eth_dev->process_private = hw_priv;
	}

	for (i = 0; i < NFP_MAX_PHYPORTS; i++) {
		repr = app_fw_flower->phy_reprs[i];
		if (repr != NULL) {
			eth_dev = rte_eth_dev_get_by_name(repr->name);
			if (eth_dev != NULL)
				eth_dev->process_private = hw_priv;
		}
	}

	for (i = 0; i < MAX_FLOWER_VFS; i++) {
		repr = app_fw_flower->vf_reprs[i];
		if (repr != NULL) {
			eth_dev = rte_eth_dev_get_by_name(repr->name);
			if (eth_dev != NULL)
				eth_dev->process_private = hw_priv;
		}
	}
}

static int
nfp_flower_phy_repr_alloc(struct nfp_net_hw_priv *hw_priv,
		struct nfp_flower_representor *flower_repr,
		const char *pci_name)
{
	int i;
	int ret;
	uint8_t id;
	struct nfp_pf_dev *pf_dev;
	ethdev_init_t ethdev_init;
	struct nfp_repr_init repr_init;
	struct nfp_eth_table_port *eth_port;
	struct nfp_app_fw_flower *app_fw_flower;

	pf_dev = hw_priv->pf_dev;
	repr_init.hw_priv = hw_priv;
	app_fw_flower = flower_repr->app_fw_flower;
	for (i = 0; i < app_fw_flower->num_phyport_reprs; i++) {
		id = nfp_function_id_get(pf_dev, i);
		eth_port = &pf_dev->nfp_eth_table->ports[id];
		flower_repr->repr_type = NFP_REPR_TYPE_PHYS_PORT;
		flower_repr->port_id = nfp_flower_get_phys_port_id(eth_port->index);
		flower_repr->nfp_idx = eth_port->index;
		flower_repr->idx = id;

		/* Copy the real mac of the interface to the representor struct */
		rte_ether_addr_copy(&eth_port->mac_addr, &flower_repr->mac_addr);

		/*
		 * Create a eth_dev for this representor.
		 * This will also allocate private memory for the device.
		 */
		repr_init.flower_repr = flower_repr;
		if (pf_dev->multi_pf.enabled) {
			repr_init.flower_repr->vf_id = i;
			snprintf(flower_repr->name, sizeof(flower_repr->name),
					"%s_repr_p", pci_name);
			ethdev_init = nfp_flower_multiple_pf_repr_init;
		} else {
			repr_init.flower_repr->vf_id = i + 1;
			snprintf(flower_repr->name, sizeof(flower_repr->name),
					"%s_repr_p%d", pci_name, id);
			ethdev_init = nfp_flower_repr_init;
		}
		ret = rte_eth_dev_create(&pf_dev->pci_dev->device, flower_repr->name,
				sizeof(struct nfp_flower_representor),
				NULL, NULL, ethdev_init, &repr_init);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Could not create eth_dev for repr.");
			break;
		}
	}

	if (i < app_fw_flower->num_phyport_reprs)
		return -EIO;

	return 0;
}

static int
nfp_flower_vf_repr_alloc(struct nfp_net_hw_priv *hw_priv,
		struct nfp_flower_representor *flower_repr,
		const char *pci_name)
{
	int i;
	int ret;
	struct nfp_pf_dev *pf_dev;
	struct nfp_repr_init repr_init;
	struct nfp_app_fw_flower *app_fw_flower;

	pf_dev = hw_priv->pf_dev;
	repr_init.hw_priv = hw_priv;
	app_fw_flower = flower_repr->app_fw_flower;
	for (i = 0; i < app_fw_flower->num_vf_reprs; i++) {
		flower_repr->repr_type = NFP_REPR_TYPE_VF;
		flower_repr->port_id = nfp_get_pcie_port_id(pf_dev->cpp,
				NFP_FLOWER_CMSG_PORT_VNIC_TYPE_VF, i + pf_dev->vf_base_id, 0);
		flower_repr->nfp_idx = 0;
		flower_repr->vf_id = i;
		flower_repr->idx = nfp_function_id_get(pf_dev, 0);

		/* VF reprs get a random MAC address */
		rte_eth_random_addr(flower_repr->mac_addr.addr_bytes);
		snprintf(flower_repr->name, sizeof(flower_repr->name),
				"%s_repr_vf%d", pci_name, i);

		repr_init.flower_repr = flower_repr;
		/* This will also allocate private memory for the device */
		ret = rte_eth_dev_create(&pf_dev->pci_dev->device, flower_repr->name,
				sizeof(struct nfp_flower_representor),
				NULL, NULL, nfp_flower_repr_init, &repr_init);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Could not create eth_dev for repr.");
			break;
		}
	}

	if (i < app_fw_flower->num_vf_reprs)
		return -EIO;

	return 0;
}

static int
nfp_flower_pf_repr_alloc(struct nfp_net_hw_priv *hw_priv,
		struct nfp_flower_representor *flower_repr,
		const char *pci_name)
{
	int ret;
	struct nfp_pf_dev *pf_dev;

	pf_dev = hw_priv->pf_dev;
	if (pf_dev->multi_pf.enabled)
		return 0;

	/* Create a rte_eth_dev for PF vNIC representor */
	flower_repr->repr_type = NFP_REPR_TYPE_PF;
	flower_repr->idx = 0;

	/* PF vNIC reprs get a random MAC address */
	rte_eth_random_addr(flower_repr->mac_addr.addr_bytes);

	snprintf(flower_repr->name, sizeof(flower_repr->name),
			"%s_repr_pf", pci_name);

	/* Create a eth_dev for this representor */
	ret = rte_eth_dev_create(&pf_dev->pci_dev->device, flower_repr->name,
			sizeof(struct nfp_flower_representor),
			NULL, NULL, nfp_flower_pf_repr_init, flower_repr);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init the pf repr.");
		return -EINVAL;
	}

	return 0;
}

static int
nfp_flower_repr_alloc(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_net_hw_priv *hw_priv)
{
	int ret;
	const char *pci_name;
	struct nfp_flower_representor flower_repr = {
		.switch_domain_id = app_fw_flower->switch_domain_id,
		.app_fw_flower    = app_fw_flower,
	};

	/* Send a NFP_FLOWER_CMSG_TYPE_MAC_REPR cmsg to hardware */
	ret = nfp_flower_cmsg_mac_repr(app_fw_flower, hw_priv->pf_dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not send mac repr cmsgs.");
		return ret;
	}

	pci_name = strchr(hw_priv->pf_dev->pci_dev->name, ':') + 1;

	ret = nfp_flower_pf_repr_alloc(hw_priv, &flower_repr, pci_name);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Could not alloc pf repr.");
		return ret;
	}

	/* Create a rte_eth_dev for every phyport representor */
	ret = nfp_flower_phy_repr_alloc(hw_priv, &flower_repr, pci_name);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init the phy repr.");
		goto repr_free;
	}

	/*
	 * Now allocate eth_dev's for VF representors.
	 * Also send reify messages.
	 */
	ret = nfp_flower_vf_repr_alloc(hw_priv, &flower_repr, pci_name);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init the vf repr.");
		goto repr_free;
	}

	nfp_flower_repr_priv_init(app_fw_flower, hw_priv);

	return 0;

repr_free:
	nfp_flower_repr_free_all(app_fw_flower);

	return ret;
}

int
nfp_flower_repr_create(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_net_hw_priv *hw_priv)
{
	int ret;
	uint8_t num_pf_reprs;
	struct nfp_pf_dev *pf_dev;
	struct rte_pci_device *pci_dev;
	struct rte_eth_devargs eth_da = {
		.nb_representor_ports = 0
	};

	pf_dev = hw_priv->pf_dev;
	pci_dev = pf_dev->pci_dev;

	/* Allocate a switch domain for the flower app */
	ret = rte_eth_switch_domain_alloc(&app_fw_flower->switch_domain_id);
	if (ret != 0)
		PMD_INIT_LOG(WARNING, "Failed to allocate switch domain for device.");

	/* Now parse PCI device args passed for representor info */
	if (pci_dev->device.devargs != NULL) {
		ret = rte_eth_devargs_parse(pci_dev->device.devargs->args, &eth_da, 1);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "Devarg parse failed.");
			return -EINVAL;
		}
	}

	if (eth_da.nb_representor_ports == 0) {
		PMD_INIT_LOG(DEBUG, "No representor port need to create.");
		return 0;
	}

	/* Calculate the number of pf repr */
	if (pf_dev->multi_pf.enabled)
		num_pf_reprs = 0;
	else
		num_pf_reprs = 1;

	if (eth_da.nb_representor_ports < pf_dev->total_phyports + num_pf_reprs) {
		PMD_INIT_LOG(ERR, "Should also create repr port for phy port and PF vNIC.");
		return -ERANGE;
	}

	/* Only support VF representor creation via the command line */
	if (eth_da.type != RTE_ETH_REPRESENTOR_VF) {
		PMD_INIT_LOG(ERR, "Unsupported representor type: %d.", eth_da.type);
		return -ENOTSUP;
	}

	/* Fill in flower app with repr counts */
	app_fw_flower->num_phyport_reprs = pf_dev->total_phyports;
	app_fw_flower->num_vf_reprs = eth_da.nb_representor_ports -
			pf_dev->total_phyports - num_pf_reprs;
	if (pf_dev->max_vfs != 0 && pf_dev->sriov_vf < app_fw_flower->num_vf_reprs) {
		PMD_INIT_LOG(ERR, "The VF repr nums %d is bigger than VF nums %d.",
				app_fw_flower->num_vf_reprs, pf_dev->sriov_vf);
		return -ERANGE;
	}

	PMD_INIT_LOG(INFO, "%d number of VF reprs.", app_fw_flower->num_vf_reprs);
	PMD_INIT_LOG(INFO, "%d number of phyport reprs.", app_fw_flower->num_phyport_reprs);

	ret = nfp_flower_repr_alloc(app_fw_flower, hw_priv);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Representors allocation failed.");
		ret = -EINVAL;
		goto domain_free;
	}

	return 0;

domain_free:
	if (rte_eth_switch_domain_free(app_fw_flower->switch_domain_id) != 0)
		PMD_INIT_LOG(WARNING, "Failed to free switch domain for device.");

	return ret;
}

bool
nfp_flower_repr_is_vf(struct nfp_flower_representor *repr)
{
	return repr->repr_type == NFP_REPR_TYPE_VF;
}

bool
nfp_flower_repr_is_phy(struct nfp_flower_representor *repr)
{
	return repr->repr_type == NFP_REPR_TYPE_PHYS_PORT;
}
