/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#include <rte_dev_info.h>
#include <ethdev_pci.h>

#include "xsc_log.h"
#include "xsc_defs.h"
#include "xsc_ethdev.h"
#include "xsc_rx.h"
#include "xsc_tx.h"
#include "xsc_dev.h"
#include "xsc_cmd.h"

static int
xsc_ethdev_rss_hash_conf_get(struct rte_eth_dev *dev,
			     struct rte_eth_rss_conf *rss_conf)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);

	if (rss_conf->rss_key != NULL && rss_conf->rss_key_len >= priv->rss_conf.rss_key_len)
		memcpy(rss_conf->rss_key, priv->rss_conf.rss_key, priv->rss_conf.rss_key_len);

	rss_conf->rss_key_len = priv->rss_conf.rss_key_len;
	rss_conf->rss_hf = priv->rss_conf.rss_hf;
	return 0;
}

static int
xsc_ethdev_rss_hash_update(struct rte_eth_dev *dev,
			   struct rte_eth_rss_conf *rss_conf)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	int ret = 0;

	ret = xsc_dev_rss_key_modify(priv->xdev, rss_conf->rss_key, rss_conf->rss_key_len);
	if (ret == 0) {
		memcpy(priv->rss_conf.rss_key, rss_conf->rss_key,
		       priv->rss_conf.rss_key_len);
		priv->rss_conf.rss_key_len = rss_conf->rss_key_len;
		priv->rss_conf.rss_hf = rss_conf->rss_hf;
	}

	return ret;
}

static int
xsc_ethdev_configure(struct rte_eth_dev *dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	int ret;
	struct rte_eth_rss_conf *rss_conf;

	priv->num_sq = dev->data->nb_tx_queues;
	priv->num_rq = dev->data->nb_rx_queues;

	if (dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG)
		dev->data->dev_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;

	if (priv->rss_conf.rss_key == NULL) {
		priv->rss_conf.rss_key = rte_zmalloc(NULL, XSC_RSS_HASH_KEY_LEN,
						     RTE_CACHE_LINE_SIZE);
		if (priv->rss_conf.rss_key == NULL) {
			PMD_DRV_LOG(ERR, "Failed to alloc rss key");
			rte_errno = ENOMEM;
			ret = -rte_errno;
			goto error;
		}
		priv->rss_conf.rss_key_len = XSC_RSS_HASH_KEY_LEN;
	}

	if (dev->data->dev_conf.rx_adv_conf.rss_conf.rss_key != NULL) {
		rss_conf = &dev->data->dev_conf.rx_adv_conf.rss_conf;
		ret = xsc_ethdev_rss_hash_update(dev, rss_conf);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Xsc pmd set rss key error!");
			rte_errno = -ENOEXEC;
			goto error;
		}
	}

	priv->txqs = (void *)dev->data->tx_queues;
	priv->rxqs = (void *)dev->data->rx_queues;
	return 0;

error:
	return -rte_errno;
}

static void
xsc_ethdev_txq_release(struct rte_eth_dev *dev, uint16_t idx)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	struct xsc_txq_data *txq_data = xsc_txq_get(priv, idx);

	if (txq_data == NULL)
		return;

	xsc_dev_set_qpsetid(priv->xdev, txq_data->qpn, 0);
	xsc_txq_obj_release(priv->xdev, txq_data);
	rte_free(txq_data->fcqs);
	txq_data->fcqs = NULL;
	xsc_txq_elts_free(txq_data);
	rte_free(txq_data);
	(*priv->txqs)[idx] = NULL;

	dev->data->tx_queues[idx] = NULL;
	dev->data->tx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STOPPED;
}

static void
xsc_ethdev_rxq_release(struct rte_eth_dev *dev, uint16_t idx)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	struct xsc_rxq_data *rxq_data = xsc_rxq_get(priv, idx);

	if (rxq_data == NULL)
		return;
	xsc_rxq_rss_obj_release(priv->xdev, rxq_data);
	xsc_rxq_elts_free(rxq_data);
	rte_free(rxq_data);
	(*priv->rxqs)[idx] = NULL;

	dev->data->rx_queues[idx] = NULL;
	dev->data->rx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STOPPED;
}

static int
xsc_ethdev_enable(struct rte_eth_dev *dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	struct xsc_hwinfo *hwinfo;
	int peer_dstinfo = 0;
	int peer_logicalport = 0;
	int logical_port = 0;
	int local_dstinfo = 0;
	int pcie_logic_port = 0;
	int qp_set_id;
	int repr_id;
	struct xsc_rxq_data *rxq;
	uint16_t rx_qpn;
	int i, vld;
	struct xsc_txq_data *txq;
	struct xsc_repr_port *repr;
	struct xsc_repr_info *repr_info;
	uint8_t mac_filter_en = !dev->data->promiscuous;

	if (priv->funcid_type != XSC_PHYPORT_MAC_FUNCID)
		return -ENODEV;

	rxq = xsc_rxq_get(priv, 0);
	if (rxq == NULL)
		return -EINVAL;

	rx_qpn = (uint16_t)rxq->qpn;
	hwinfo = &priv->xdev->hwinfo;
	repr_id = priv->representor_id;
	repr = &priv->xdev->repr_ports[repr_id];
	repr_info = &repr->info;

	qp_set_id = xsc_dev_qp_set_id_get(priv->xdev, repr_id);
	logical_port = repr_info->logical_port;
	local_dstinfo = repr_info->local_dstinfo;
	peer_logicalport = repr_info->peer_logical_port;
	peer_dstinfo = repr_info->peer_dstinfo;

	pcie_logic_port = hwinfo->pcie_no + 8;

	for (i = 0; i < priv->num_sq; i++) {
		txq = xsc_txq_get(priv, i);
		if (txq == NULL)
			return -EINVAL;
		xsc_dev_modify_qp_status(priv->xdev, txq->qpn, 1, XSC_CMD_OP_RTR2RTS_QP);
		xsc_dev_modify_qp_qostree(priv->xdev, txq->qpn);
		xsc_dev_set_qpsetid(priv->xdev, txq->qpn, qp_set_id);
	}

	if (!xsc_dev_is_vf(priv->xdev)) {
		xsc_dev_create_ipat(priv->xdev, logical_port, peer_dstinfo);
		xsc_dev_create_vfos_baselp(priv->xdev);
		xsc_dev_create_epat(priv->xdev, local_dstinfo, pcie_logic_port,
				    rx_qpn - hwinfo->raw_rss_qp_id_base,
				    priv->num_rq, &priv->rss_conf,
				    mac_filter_en, priv->mac[0].addr_bytes);
		xsc_dev_create_pct(priv->xdev, repr_id, logical_port, peer_dstinfo);
		xsc_dev_create_pct(priv->xdev, repr_id, peer_logicalport, local_dstinfo);
	} else {
		vld = xsc_dev_get_ipat_vld(priv->xdev, logical_port);
		if (vld == 0)
			xsc_dev_create_ipat(priv->xdev, logical_port, peer_dstinfo);
		xsc_dev_vf_modify_epat(priv->xdev, local_dstinfo,
				       rx_qpn - hwinfo->raw_rss_qp_id_base,
				       priv->num_rq, &priv->rss_conf,
				       mac_filter_en, priv->mac[0].addr_bytes);
	}

	return 0;
}

static void
xsc_rxq_stop(struct rte_eth_dev *dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	uint16_t i;

	for (i = 0; i != priv->num_rq; ++i)
		xsc_ethdev_rxq_release(dev, i);
	priv->rxqs = NULL;
	priv->flags &= ~XSC_FLAG_RX_QUEUE_INIT;
}

static void
xsc_txq_stop(struct rte_eth_dev *dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	uint16_t i;

	for (i = 0; i != priv->num_sq; ++i)
		xsc_ethdev_txq_release(dev, i);
	priv->txqs = NULL;
	priv->flags &= ~XSC_FLAG_TX_QUEUE_INIT;
}

static int
xsc_txq_start(struct xsc_ethdev_priv *priv)
{
	struct xsc_txq_data *txq_data;
	struct rte_eth_dev *dev = priv->eth_dev;
	uint16_t i;
	int ret;
	size_t size;

	if (priv->flags & XSC_FLAG_TX_QUEUE_INIT) {
		for (i = 0; i != priv->num_sq; ++i)
			dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
		return 0;
	}

	for (i = 0; i != priv->num_sq; ++i) {
		txq_data = xsc_txq_get(priv, i);
		if (txq_data == NULL)
			goto error;
		xsc_txq_elts_alloc(txq_data);
		ret = xsc_txq_obj_new(priv->xdev, txq_data, i);
		if (ret < 0)
			goto error;
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
		PMD_DRV_LOG(INFO, "Port %u create tx success", dev->data->port_id);

		size = txq_data->cqe_s * sizeof(*txq_data->fcqs);
		txq_data->fcqs = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
		if (!txq_data->fcqs) {
			PMD_DRV_LOG(ERR, "Port %u txq %u alloc fcqs memory failed",
				    dev->data->port_id, i);
			rte_errno = ENOMEM;
			goto error;
		}
	}

	priv->flags |= XSC_FLAG_TX_QUEUE_INIT;
	return 0;

error:
	/* Queue resources are released by xsc_ethdev_start calling the stop interface */
	return -rte_errno;
}

static int
xsc_rxq_start(struct xsc_ethdev_priv *priv)
{
	struct xsc_rxq_data *rxq_data;
	struct rte_eth_dev *dev = priv->eth_dev;
	uint16_t i;
	int ret;

	if (priv->flags & XSC_FLAG_RX_QUEUE_INIT) {
		for (i = 0; i != priv->num_sq; ++i)
			dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
		return 0;
	}

	for (i = 0; i != priv->num_rq; ++i) {
		rxq_data = xsc_rxq_get(priv, i);
		if (rxq_data == NULL)
			goto error;
		if (dev->data->rx_queue_state[i] != RTE_ETH_QUEUE_STATE_STARTED) {
			ret = xsc_rxq_elts_alloc(rxq_data);
			if (ret != 0)
				goto error;
		}
	}

	ret = xsc_rxq_rss_obj_new(priv, priv->dev_data->port_id);
	if (ret != 0)
		goto error;

	priv->flags |= XSC_FLAG_RX_QUEUE_INIT;
	return 0;
error:
	/* Queue resources are released by xsc_ethdev_start calling the stop interface */
	return -rte_errno;
}

static int
xsc_ethdev_start(struct rte_eth_dev *dev)
{
	int ret;
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);

	ret = xsc_txq_start(priv);
	if (ret) {
		PMD_DRV_LOG(ERR, "Port %u txq start failed: %s",
			    dev->data->port_id, strerror(rte_errno));
		goto error;
	}

	ret = xsc_rxq_start(priv);
	if (ret) {
		PMD_DRV_LOG(ERR, "Port %u Rx queue start failed: %s",
			    dev->data->port_id, strerror(rte_errno));
		goto error;
	}

	dev->data->dev_started = 1;

	dev->rx_pkt_burst = xsc_rx_burst;
	dev->tx_pkt_burst = xsc_tx_burst;

	ret = xsc_ethdev_enable(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to enable port: %u",
			    dev->data->port_id);
		goto error;
	}

	return 0;

error:
	dev->data->dev_started = 0;
	xsc_txq_stop(dev);
	xsc_rxq_stop(dev);
	return -rte_errno;
}

static int
xsc_ethdev_stop(struct rte_eth_dev *dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	uint16_t i;

	PMD_DRV_LOG(DEBUG, "Port %u stopping", dev->data->port_id);
	dev->data->dev_started = 0;
	dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
	rte_wmb();

	rte_delay_us_sleep(1000 * priv->num_rq);
	for (i = 0; i < priv->num_rq; ++i)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < priv->num_sq; ++i)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

static int
xsc_ethdev_close(struct rte_eth_dev *dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);

	PMD_DRV_LOG(DEBUG, "Port %u closing", dev->data->port_id);
	dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;
	rte_wmb();

	xsc_txq_stop(dev);
	xsc_rxq_stop(dev);

	rte_free(priv->rss_conf.rss_key);
	xsc_dev_close(priv->xdev, priv->representor_id);
	dev->data->mac_addrs = NULL;
	return 0;
}

static int
xsc_ethdev_set_link_up(struct rte_eth_dev *dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	struct xsc_dev *xdev = priv->xdev;

	if (xsc_dev_is_vf(xdev))
		return -ENOTSUP;
	return xsc_dev_link_status_set(xdev, RTE_ETH_LINK_UP);
}

static int
xsc_ethdev_set_link_down(struct rte_eth_dev *dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	struct xsc_dev *xdev = priv->xdev;

	if (xsc_dev_is_vf(xdev))
		return -ENOTSUP;
	return xsc_dev_link_status_set(xdev, RTE_ETH_LINK_DOWN);
}

static int
xsc_ethdev_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	struct xsc_dev *xdev = priv->xdev;
	struct rte_eth_link link = { };
	int ret = 0;

	RTE_SET_USED(wait_to_complete);
	ret = xsc_dev_link_get(xdev, &link);
	if (ret != 0)
		return ret;
	rte_eth_linkstatus_set(dev, &link);

	return 0;
}

static uint64_t
xsc_get_rx_queue_offloads(struct rte_eth_dev *dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	struct xsc_dev_config *config = &priv->config;
	uint64_t offloads = 0;

	if (config->hw_csum)
		offloads |= (RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
			     RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
			     RTE_ETH_RX_OFFLOAD_TCP_CKSUM);

	return offloads;
}

static uint64_t
xsc_get_tx_port_offloads(struct rte_eth_dev *dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	uint64_t offloads = 0;
	struct xsc_dev_config *config = &priv->config;

	if (config->hw_csum)
		offloads |= (RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
			     RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
			     RTE_ETH_TX_OFFLOAD_TCP_CKSUM);
	if (config->tso)
		offloads |= RTE_ETH_TX_OFFLOAD_TCP_TSO;
	return offloads;
}

static int
xsc_ethdev_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *info)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);

	info->min_rx_bufsize = 64;
	info->max_rx_pktlen = 65536;
	info->max_lro_pkt_size = 0;
	info->max_rx_queues = 256;
	info->max_tx_queues = 1024;
	info->rx_desc_lim.nb_max = 4096;
	info->rx_desc_lim.nb_min = 16;
	info->tx_desc_lim.nb_max = 8192;
	info->tx_desc_lim.nb_min = 128;

	info->rx_queue_offload_capa = xsc_get_rx_queue_offloads(dev);
	info->rx_offload_capa = info->rx_queue_offload_capa;
	info->tx_offload_capa = xsc_get_tx_port_offloads(dev);

	info->if_index = priv->ifindex;
	info->speed_capa = priv->xdev->link_speed_capa;
	info->hash_key_size = XSC_RSS_HASH_KEY_LEN;
	info->tx_desc_lim.nb_seg_max = 8;
	info->tx_desc_lim.nb_mtu_seg_max = 8;
	info->switch_info.name = dev->data->name;
	info->switch_info.port_id = priv->representor_id;
	return 0;
}

static int
xsc_ethdev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
			  uint32_t socket, const struct rte_eth_rxconf *conf,
			  struct rte_mempool *mp)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	struct xsc_rxq_data *rxq_data = NULL;
	uint16_t desc_n;
	uint16_t rx_free_thresh;
	uint64_t offloads = conf->offloads | dev->data->dev_conf.rxmode.offloads;

	desc = (desc > XSC_MAX_DESC_NUMBER) ? XSC_MAX_DESC_NUMBER : desc;
	desc_n = desc;

	if (!rte_is_power_of_2(desc))
		desc_n = 1 << rte_log2_u32(desc);

	rxq_data = rte_malloc_socket(NULL, sizeof(*rxq_data) + desc_n * sizeof(struct rte_mbuf *),
				     RTE_CACHE_LINE_SIZE, socket);
	if (rxq_data == NULL) {
		PMD_DRV_LOG(ERR, "Port %u create rxq idx %d failure",
			    dev->data->port_id, idx);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	rxq_data->idx = idx;
	rxq_data->priv = priv;
	(*priv->rxqs)[idx] = rxq_data;

	rx_free_thresh = (conf->rx_free_thresh) ? conf->rx_free_thresh : XSC_RX_FREE_THRESH;
	rxq_data->rx_free_thresh = rx_free_thresh;

	rxq_data->elts = (struct rte_mbuf *(*)[desc_n])(rxq_data + 1);
	rxq_data->mp = mp;
	rxq_data->socket = socket;

	rxq_data->csum = !!(offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM);
	rxq_data->hw_timestamp = !!(offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP);
	rxq_data->crc_present = 0;

	rxq_data->wqe_n = rte_log2_u32(desc_n);
	rxq_data->wqe_s = desc_n;
	rxq_data->wqe_m = desc_n - 1;

	rxq_data->port_id = dev->data->port_id;
	dev->data->rx_queues[idx] = rxq_data;
	return 0;
}

static int
xsc_ethdev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
			  uint32_t socket, const struct rte_eth_txconf *conf)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	struct xsc_txq_data *txq;
	uint16_t desc_n;

	desc = (desc > XSC_MAX_DESC_NUMBER) ? XSC_MAX_DESC_NUMBER : desc;
	desc_n = desc;

	if (!rte_is_power_of_2(desc))
		desc_n = 1 << rte_log2_u32(desc);

	txq = rte_malloc_socket(NULL, sizeof(*txq) + desc_n * sizeof(struct rte_mbuf *),
				RTE_CACHE_LINE_SIZE, socket);
	txq->offloads = conf->offloads | dev->data->dev_conf.txmode.offloads;
	txq->priv = priv;
	txq->socket = socket;

	txq->elts_n = rte_log2_u32(desc_n);
	txq->elts_s = desc_n;
	txq->elts_m = desc_n - 1;
	txq->port_id = dev->data->port_id;
	txq->idx = idx;

	(*priv->txqs)[idx] = txq;
	return 0;
}

static int
xsc_ethdev_set_mtu(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	int ret = 0;

	if (priv->eth_type != RTE_ETH_REPRESENTOR_PF) {
		priv->mtu = mtu;
		return 0;
	}

	ret = xsc_dev_set_mtu(priv->xdev, mtu);
	if (ret) {
		PMD_DRV_LOG(ERR, "Mtu set to %u failure", mtu);
		return -EAGAIN;
	}

	priv->mtu = mtu;
	return 0;
}

static int
xsc_ethdev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	uint32_t rxqs_n = priv->num_rq;
	uint32_t txqs_n = priv->num_sq;
	uint32_t i, idx;
	struct xsc_rxq_data *rxq;
	struct xsc_txq_data *txq;

	for (i = 0; i < rxqs_n; ++i) {
		rxq = xsc_rxq_get(priv, i);
		if (unlikely(rxq == NULL))
			continue;

		idx = rxq->idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_ipackets[idx] += rxq->stats.rx_pkts;
			stats->q_ibytes[idx] += rxq->stats.rx_bytes;
			stats->q_errors[idx] += rxq->stats.rx_errors +
						rxq->stats.rx_nombuf;
		}
		stats->ipackets += rxq->stats.rx_pkts;
		stats->ibytes += rxq->stats.rx_bytes;
		stats->ierrors += rxq->stats.rx_errors;
		stats->rx_nombuf += rxq->stats.rx_nombuf;
	}

	for (i = 0; i < txqs_n; ++i) {
		txq = xsc_txq_get(priv, i);
		if (unlikely(txq == NULL))
			continue;

		idx = txq->idx;
		if (idx < RTE_ETHDEV_QUEUE_STAT_CNTRS) {
			stats->q_opackets[idx] += txq->stats.tx_pkts;
			stats->q_obytes[idx] += txq->stats.tx_bytes;
			stats->q_errors[idx] += txq->stats.tx_errors;
		}
		stats->opackets += txq->stats.tx_pkts;
		stats->obytes += txq->stats.tx_bytes;
		stats->oerrors += txq->stats.tx_errors;
	}

	return 0;
}

static int
xsc_ethdev_stats_reset(struct rte_eth_dev *dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);
	uint32_t rxqs_n = priv->num_rq;
	uint32_t txqs_n = priv->num_sq;
	uint32_t i;
	struct xsc_rxq_data *rxq;
	struct xsc_txq_data *txq;

	for (i = 0; i < rxqs_n; ++i) {
		rxq = xsc_rxq_get(priv, i);
		if (unlikely(rxq == NULL))
			continue;
		memset(&rxq->stats, 0, sizeof(struct xsc_rxq_stats));
	}
	for (i = 0; i < txqs_n; ++i) {
		txq = xsc_txq_get(priv, i);
		if (unlikely(txq == NULL))
			continue;
		memset(&txq->stats, 0, sizeof(struct xsc_txq_stats));
	}

	return 0;
}

static int
xsc_ethdev_mac_addr_add(struct rte_eth_dev *dev, struct rte_ether_addr *mac, uint32_t index)
{
	int i;

	rte_errno = EINVAL;
	if (index > XSC_MAX_MAC_ADDRESSES)
		return -rte_errno;

	if (rte_is_zero_ether_addr(mac))
		return -rte_errno;

	for (i = 0; i != XSC_MAX_MAC_ADDRESSES; ++i) {
		if (i == (int)index)
			continue;
		if (memcmp(&dev->data->mac_addrs[i], mac, sizeof(*mac)) != 0)
			continue;
		/* Address already configured elsewhere, return with error */
		rte_errno = EADDRINUSE;
		return -rte_errno;
	}

	dev->data->mac_addrs[index] = *mac;
	return 0;
}

static int
xsc_set_promiscuous(struct rte_eth_dev *dev, uint8_t mac_filter_en)
{
	int repr_id;
	struct xsc_repr_port *repr;
	struct xsc_repr_info *repr_info;
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);

	repr_id = priv->representor_id;
	repr = &priv->xdev->repr_ports[repr_id];
	repr_info = &repr->info;

	return xsc_dev_modify_epat_mac_filter(priv->xdev,
					      repr_info->local_dstinfo,
					      mac_filter_en);
}

static int
xsc_ethdev_promiscuous_enable(struct rte_eth_dev *dev)
{
	int ret;

	ret = xsc_set_promiscuous(dev, 0);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Enable port %u promiscuous failure",
			    dev->data->port_id);
		return ret;
	}

	dev->data->promiscuous = 1;
	return 0;
}

static int
xsc_ethdev_promiscuous_disable(struct rte_eth_dev *dev)
{
	int ret;

	ret = xsc_set_promiscuous(dev, 1);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Disable port %u promiscuous failure",
			    dev->data->port_id);
		return ret;
	}

	dev->data->promiscuous = 0;
	return 0;
}

static int
xsc_ethdev_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);

	return xsc_dev_fw_version_get(priv->xdev, fw_version, fw_size);
}

static int
xsc_ethdev_get_module_info(struct rte_eth_dev *dev,
			   struct rte_eth_dev_module_info *modinfo)
{
	int size_read = 0;
	uint8_t data[4] = { 0 };
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);

	size_read = xsc_dev_query_module_eeprom(priv->xdev, 0, 3, data);
	if (size_read < 3)
		return -1;

	/* data[0] = identifier byte */
	switch (data[0]) {
	case XSC_MODULE_ID_QSFP:
		modinfo->type       = RTE_ETH_MODULE_SFF_8436;
		modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8436_MAX_LEN;
		break;
	case XSC_MODULE_ID_QSFP_PLUS:
	case XSC_MODULE_ID_QSFP28:
		/* data[1] = revision id */
		if (data[0] == XSC_MODULE_ID_QSFP28 || data[1] >= 0x3) {
			modinfo->type       = RTE_ETH_MODULE_SFF_8636;
			modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8636_MAX_LEN;
		} else {
			modinfo->type       = RTE_ETH_MODULE_SFF_8436;
			modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8436_MAX_LEN;
		}
		break;
	case XSC_MODULE_ID_SFP:
		modinfo->type       = RTE_ETH_MODULE_SFF_8472;
		modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8472_LEN;
		break;
	case XSC_MODULE_ID_QSFP_DD:
	case XSC_MODULE_ID_DSFP:
	case XSC_MODULE_ID_QSFP_PLUS_CMIS:
		modinfo->type       = RTE_ETH_MODULE_SFF_8636;
		/* Verify if module EEPROM is a flat memory. In case of flat
		 * memory only page 00h (0-255 bytes) can be read. Otherwise
		 * upper pages 01h and 02h can also be read. Upper pages 10h
		 * and 11h are currently not supported by the driver.
		 */
		if (data[2] & 0x80)
			modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8636_LEN;
		else
			modinfo->eeprom_len = RTE_ETH_MODULE_SFF_8472_LEN;
		break;
	default:
		PMD_DRV_LOG(ERR, "Cable type 0x%x not recognized",
			    data[0]);
		return -EINVAL;
	}

	return 0;
}

static int
xsc_ethdev_get_module_eeprom(struct rte_eth_dev *dev,
			     struct rte_dev_eeprom_info *info)
{
	uint32_t i = 0;
	uint8_t *data;
	int size_read;
	uint32_t offset = info->offset;
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(dev);

	if (info->length == 0) {
		PMD_DRV_LOG(ERR, "Failed to get module eeprom, eeprom length is 0");
		rte_errno = EINVAL;
		return -rte_errno;
	}

	data = malloc(info->length);
	if (data == NULL) {
		PMD_DRV_LOG(ERR, "Failed to get module eeprom, cannot allocate memory");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	memset(data, 0, info->length);

	while (i < info->length) {
		size_read = xsc_dev_query_module_eeprom(priv->xdev, offset,
							info->length - i, data + i);
		if (!size_read)
			/* Done reading */
			goto exit;

		if (size_read < 0) {
			PMD_DRV_LOG(ERR, "Failed to get module eeprom, size read=%d",
				    size_read);
			goto exit;
		}

		i += size_read;
		offset += size_read;
	}

	memcpy(info->data, data, info->length);

exit:
	free(data);
	return 0;
}

const struct eth_dev_ops xsc_eth_dev_ops = {
	.dev_configure = xsc_ethdev_configure,
	.dev_start = xsc_ethdev_start,
	.dev_stop = xsc_ethdev_stop,
	.dev_set_link_up = xsc_ethdev_set_link_up,
	.dev_set_link_down = xsc_ethdev_set_link_down,
	.dev_close = xsc_ethdev_close,
	.link_update = xsc_ethdev_link_update,
	.promiscuous_enable = xsc_ethdev_promiscuous_enable,
	.promiscuous_disable = xsc_ethdev_promiscuous_disable,
	.stats_get = xsc_ethdev_stats_get,
	.stats_reset = xsc_ethdev_stats_reset,
	.dev_infos_get = xsc_ethdev_infos_get,
	.rx_queue_setup = xsc_ethdev_rx_queue_setup,
	.tx_queue_setup = xsc_ethdev_tx_queue_setup,
	.rx_queue_release = xsc_ethdev_rxq_release,
	.tx_queue_release = xsc_ethdev_txq_release,
	.mtu_set = xsc_ethdev_set_mtu,
	.rss_hash_update = xsc_ethdev_rss_hash_update,
	.rss_hash_conf_get = xsc_ethdev_rss_hash_conf_get,
	.fw_version_get = xsc_ethdev_fw_version_get,
	.get_module_info = xsc_ethdev_get_module_info,
	.get_module_eeprom = xsc_ethdev_get_module_eeprom,
};

static int
xsc_ethdev_init_one_representor(struct rte_eth_dev *eth_dev, void *init_params)
{
	int ret;
	struct xsc_repr_port *repr_port = (struct xsc_repr_port *)init_params;
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(eth_dev);
	struct xsc_dev_config *config = &priv->config;
	struct rte_ether_addr mac;

	priv->repr_port = repr_port;
	repr_port->drv_data = eth_dev;
	priv->xdev = repr_port->xdev;
	priv->mtu = RTE_ETHER_MTU;
	priv->funcid_type = (repr_port->info.funcid & XSC_FUNCID_TYPE_MASK) >> 14;
	priv->funcid = repr_port->info.funcid & XSC_FUNCID_MASK;
	if (repr_port->info.port_type == XSC_PORT_TYPE_UPLINK ||
	    repr_port->info.port_type == XSC_PORT_TYPE_UPLINK_BOND)
		priv->eth_type = RTE_ETH_REPRESENTOR_PF;
	else
		priv->eth_type = RTE_ETH_REPRESENTOR_VF;
	priv->representor_id = repr_port->info.repr_id;
	priv->dev_data = eth_dev->data;
	priv->ifindex = repr_port->info.ifindex;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;
	eth_dev->data->mac_addrs = priv->mac;
	if (rte_is_zero_ether_addr(eth_dev->data->mac_addrs)) {
		ret = xsc_dev_get_mac(priv->xdev, mac.addr_bytes);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Port %u cannot get MAC address",
				    eth_dev->data->port_id);
			return -ENODEV;
		}
	}

	xsc_ethdev_mac_addr_add(eth_dev, &mac, 0);

	config->hw_csum = 1;
	config->pph_flag =  priv->xdev->devargs.pph_mode;
	if ((config->pph_flag & XSC_TX_PPH) != 0) {
		config->tso = 0;
	} else {
		config->tso = 1;
		config->tso_max_payload_sz = 1500;
	}

	priv->is_representor = 1;
	eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;
	eth_dev->data->representor_id = priv->representor_id;
	eth_dev->data->backer_port_id = eth_dev->data->port_id;

	eth_dev->dev_ops = &xsc_eth_dev_ops;
	eth_dev->rx_pkt_burst = rte_eth_pkt_burst_dummy;
	eth_dev->tx_pkt_burst = rte_eth_pkt_burst_dummy;

	rte_eth_dev_probing_finish(eth_dev);

	return 0;
}

static int
xsc_ethdev_init_representors(struct rte_eth_dev *eth_dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(eth_dev);
	struct rte_eth_devargs eth_da = { .nb_representor_ports = 0 };
	struct rte_device *dev;
	struct xsc_dev *xdev;
	struct xsc_repr_port *repr_port;
	char name[RTE_ETH_NAME_MAX_LEN];
	int i;
	int ret;

	PMD_INIT_FUNC_TRACE();

	dev = &priv->pci_dev->device;
	if (dev->devargs != NULL) {
		ret = rte_eth_devargs_parse(dev->devargs->args, &eth_da, 1);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to parse device arguments: %s",
				    dev->devargs->args);
			return -EINVAL;
		}
	}

	xdev = priv->xdev;
	ret = xsc_dev_repr_ports_probe(xdev, eth_da.nb_representor_ports, RTE_MAX_ETHPORTS);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to probe %d xsc device representors",
			    eth_da.nb_representor_ports);
		return ret;
	}

	/* PF rep init */
	repr_port = &xdev->repr_ports[xdev->num_repr_ports - 1];
	ret = xsc_ethdev_init_one_representor(eth_dev, repr_port);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to init backing representor");
		return ret;
	}

	/* VF rep init */
	for (i = 0; i < eth_da.nb_representor_ports; i++) {
		repr_port = &xdev->repr_ports[i];
		snprintf(name, sizeof(name), "%s_rep_%d",
			 xdev->name, repr_port->info.repr_id);
		ret = rte_eth_dev_create(dev,
					 name,
					 sizeof(struct xsc_ethdev_priv),
					 NULL, NULL,
					 xsc_ethdev_init_one_representor,
					 repr_port);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Failed to create representor: %d", i);
			goto destroy_reprs;
		}
	}

	if (!xsc_dev_is_vf(priv->xdev)) {
		ret = xsc_ethdev_set_link_up(eth_dev);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Failed to set port %u link up", eth_dev->data->port_id);
			goto destroy_reprs;
		}
	}

	return 0;

destroy_reprs:
	/* Destroy vf reprs */
	while ((i--) > 1) {
		repr_port = &xdev->repr_ports[i];
		rte_eth_dev_destroy((struct rte_eth_dev *)repr_port->drv_data, NULL);
	}

	/* Destroy pf repr */
	repr_port = &xdev->repr_ports[xdev->num_repr_ports - 1];
	rte_eth_dev_destroy((struct rte_eth_dev *)repr_port->drv_data, NULL);
	return ret;
}

static int
xsc_ethdev_init(struct rte_eth_dev *eth_dev)
{
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(eth_dev);
	int ret;

	PMD_INIT_FUNC_TRACE();

	priv->eth_dev = eth_dev;
	priv->pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	ret = xsc_dev_init(priv->pci_dev, &priv->xdev);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to initialize xsc device");
		return ret;
	}
	priv->xdev->port_id = eth_dev->data->port_id;

	ret = xsc_ethdev_init_representors(eth_dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to initialize representors");
		goto uninit_xsc_dev;
	}

	return 0;

uninit_xsc_dev:
	xsc_dev_uninit(priv->xdev);
	return ret;
}

static int
xsc_ethdev_uninit(struct rte_eth_dev *eth_dev)
{
	int ret = 0;
	uint16_t port_id;
	struct xsc_ethdev_priv *priv = TO_XSC_ETHDEV_PRIV(eth_dev);

	PMD_INIT_FUNC_TRACE();
	RTE_ETH_FOREACH_DEV_OF(port_id, eth_dev->device) {
		if (port_id != eth_dev->data->port_id)
			ret |= rte_eth_dev_close(port_id);
	}

	ret |= xsc_ethdev_close(eth_dev);
	xsc_dev_pct_uninit();
	rte_free(priv->xdev);

	return ret == 0 ? 0 : -EIO;
}

static int
xsc_ethdev_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		     struct rte_pci_device *pci_dev)
{
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = rte_eth_dev_pci_generic_probe(pci_dev,
					    sizeof(struct xsc_ethdev_priv),
					    xsc_ethdev_init);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to probe ethdev: %s", pci_dev->name);
		return ret;
	}

	return 0;
}

static int
xsc_ethdev_pci_remove(struct rte_pci_device *pci_dev)
{
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = rte_eth_dev_pci_generic_remove(pci_dev, xsc_ethdev_uninit);
	if (ret) {
		PMD_DRV_LOG(ERR, "Could not remove ethdev: %s", pci_dev->name);
		return ret;
	}

	return 0;
}

static const struct rte_pci_id xsc_ethdev_pci_id_map[] = {
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MS) },
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MSVF) },
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MVH) },
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MVHVF) },
	{ RTE_PCI_DEVICE(XSC_PCI_VENDOR_ID, XSC_PCI_DEV_ID_MVS) },
	{ RTE_PCI_DEVICE(0, 0) },
};

static struct rte_pci_driver xsc_ethdev_pci_driver = {
	.id_table  = xsc_ethdev_pci_id_map,
	.probe = xsc_ethdev_pci_probe,
	.remove = xsc_ethdev_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_xsc, xsc_ethdev_pci_driver);
RTE_PMD_REGISTER_PCI_TABLE(net_xsc, xsc_ethdev_pci_id_map);
RTE_PMD_REGISTER_PARAM_STRING(net_xsc,
			      XSC_PPH_MODE_ARG "=<x>"
			      XSC_NIC_MODE_ARG "=<x>"
			      XSC_FLOW_MODE_ARG "=<x>");

RTE_LOG_REGISTER_SUFFIX(xsc_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(xsc_logtype_driver, driver, NOTICE);
