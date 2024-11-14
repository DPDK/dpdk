/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <cnxk_rep.h>
#include <cnxk_rep_msg.h>

#define MEMPOOL_CACHE_SIZE 256
#define TX_DESC_PER_QUEUE  512
#define RX_DESC_PER_QUEUE  256
#define NB_REP_VDEV_MBUF   1024

static const struct rte_eth_xstat_name cnxk_rep_xstats_string[] = {
	{"rep_nb_rx"},
	{"rep_nb_tx"},
};

static uint16_t
cnxk_rep_tx_burst(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct cnxk_rep_txq *txq = tx_queue;
	struct cnxk_rep_dev *rep_dev;
	uint16_t n_tx;

	if (unlikely(!txq))
		return 0;

	rep_dev = txq->rep_dev;
	plt_rep_dbg("Transmitting %d packets on eswitch queue %d", nb_pkts, txq->qid);
	n_tx = cnxk_eswitch_dev_tx_burst(rep_dev->parent_dev, txq->qid, tx_pkts, nb_pkts,
					 NIX_TX_OFFLOAD_VLAN_QINQ_F);
	txq->stats.pkts += n_tx;
	return n_tx;
}

static uint16_t
cnxk_rep_rx_burst(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct cnxk_rep_rxq *rxq = rx_queue;
	struct cnxk_rep_dev *rep_dev;
	uint16_t n_rx;

	if (unlikely(!rxq))
		return 0;

	rep_dev = rxq->rep_dev;
	n_rx = cnxk_eswitch_dev_rx_burst(rep_dev->parent_dev, rxq->qid, rx_pkts, nb_pkts);
	if (n_rx == 0)
		return 0;

	plt_rep_dbg("Received %d packets on eswitch queue %d", n_rx, rxq->qid);
	rxq->stats.pkts += n_rx;
	return n_rx;
}

uint16_t
cnxk_rep_tx_burst_dummy(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	PLT_SET_USED(tx_queue);
	PLT_SET_USED(tx_pkts);
	PLT_SET_USED(nb_pkts);

	return 0;
}

uint16_t
cnxk_rep_rx_burst_dummy(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	PLT_SET_USED(rx_queue);
	PLT_SET_USED(rx_pkts);
	PLT_SET_USED(nb_pkts);

	return 0;
}

int
cnxk_rep_link_update(struct rte_eth_dev *ethdev, int wait_to_complete)
{
	struct rte_eth_link link;
	PLT_SET_USED(wait_to_complete);

	memset(&link, 0, sizeof(link));
	if (ethdev->data->dev_started) {
		link.link_status = RTE_ETH_LINK_UP;
		link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
		link.link_autoneg = RTE_ETH_LINK_FIXED;
		link.link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
	}

	return rte_eth_linkstatus_set(ethdev, &link);
}

int
cnxk_rep_dev_info_get(struct rte_eth_dev *ethdev, struct rte_eth_dev_info *dev_info)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);
	uint32_t max_rx_pktlen;

	max_rx_pktlen = (roc_nix_max_pkt_len(&rep_dev->parent_dev->nix) + RTE_ETHER_CRC_LEN -
			 CNXK_NIX_MAX_VTAG_ACT_SIZE);

	dev_info->min_rx_bufsize = NIX_MIN_HW_FRS + RTE_ETHER_CRC_LEN;
	dev_info->max_rx_pktlen = max_rx_pktlen;
	dev_info->max_mac_addrs = roc_nix_mac_max_entries_get(&rep_dev->parent_dev->nix);

	dev_info->rx_offload_capa = CNXK_REP_RX_OFFLOAD_CAPA;
	dev_info->tx_offload_capa = CNXK_REP_TX_OFFLOAD_CAPA;
	dev_info->rx_queue_offload_capa = 0;
	dev_info->tx_queue_offload_capa = 0;

	/* For the sake of symmetry, max_rx_queues = max_tx_queues */
	dev_info->max_rx_queues = 1;
	dev_info->max_tx_queues = 1;

	/* MTU specifics */
	dev_info->max_mtu = dev_info->max_rx_pktlen - (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN);
	dev_info->min_mtu = dev_info->min_rx_bufsize - CNXK_NIX_L2_OVERHEAD;

	/* Switch info specific */
	dev_info->switch_info.name = ethdev->device->name;
	dev_info->switch_info.domain_id = rep_dev->switch_domain_id;
	dev_info->switch_info.port_id = rep_dev->port_id;

	return 0;
}

int
cnxk_rep_representor_info_get(struct rte_eth_dev *ethdev, struct rte_eth_representor_info *info)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);

	return cnxk_eswitch_representor_info_get(rep_dev->parent_dev, info);
}

static int
rep_eth_conf_chk(const struct rte_eth_conf *conf, uint16_t nb_rx_queues)
{
	const struct rte_eth_rss_conf *rss_conf;
	int ret = 0;

	if (conf->link_speeds != 0) {
		plt_err("specific link speeds not supported");
		ret = -EINVAL;
	}

	switch (conf->rxmode.mq_mode) {
	case RTE_ETH_MQ_RX_RSS:
		if (nb_rx_queues != 1) {
			plt_err("Rx RSS is not supported with %u queues", nb_rx_queues);
			ret = -EINVAL;
			break;
		}

		rss_conf = &conf->rx_adv_conf.rss_conf;
		if (rss_conf->rss_key != NULL || rss_conf->rss_key_len != 0 ||
		    rss_conf->rss_hf != 0) {
			plt_err("Rx RSS configuration is not supported");
			ret = -EINVAL;
		}
		break;
	case RTE_ETH_MQ_RX_NONE:
		break;
	default:
		plt_err("Rx mode MQ modes other than RSS not supported");
		ret = -EINVAL;
		break;
	}

	if (conf->txmode.mq_mode != RTE_ETH_MQ_TX_NONE) {
		plt_err("Tx mode MQ modes not supported");
		ret = -EINVAL;
	}

	if (conf->lpbk_mode != 0) {
		plt_err("loopback not supported");
		ret = -EINVAL;
	}

	if (conf->dcb_capability_en != 0) {
		plt_err("priority-based flow control not supported");
		ret = -EINVAL;
	}

	if (conf->intr_conf.lsc != 0) {
		plt_err("link status change interrupt not supported");
		ret = -EINVAL;
	}

	if (conf->intr_conf.rxq != 0) {
		plt_err("receive queue interrupt not supported");
		ret = -EINVAL;
	}

	if (conf->intr_conf.rmv != 0) {
		plt_err("remove interrupt not supported");
		ret = -EINVAL;
	}

	return ret;
}

int
cnxk_rep_dev_configure(struct rte_eth_dev *ethdev)
{
	struct rte_eth_dev_data *ethdev_data = ethdev->data;
	int rc = -1;

	rc = rep_eth_conf_chk(&ethdev_data->dev_conf, ethdev_data->nb_rx_queues);
	if (rc)
		goto fail;

	return 0;
fail:
	return rc;
}

int
cnxk_rep_promiscuous_enable(struct rte_eth_dev *ethdev)
{
	PLT_SET_USED(ethdev);
	return 0;
}

int
cnxk_rep_promiscuous_disable(struct rte_eth_dev *ethdev)
{
	PLT_SET_USED(ethdev);
	return 0;
}

int
cnxk_rep_dev_start(struct rte_eth_dev *ethdev)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);
	struct rte_eth_link link;
	int rc = 0, qid;

	ethdev->rx_pkt_burst = cnxk_rep_rx_burst;
	ethdev->tx_pkt_burst = cnxk_rep_tx_burst;

	if (!rep_dev->is_vf_active)
		return 0;

	if (!rep_dev->rxq || !rep_dev->txq) {
		plt_err("Invalid rxq or txq for representor id %d", rep_dev->rep_id);
		rc = -EINVAL;
		goto fail;
	}

	/* Start rx queues */
	qid = rep_dev->rxq->qid;
	rc = cnxk_eswitch_rxq_start(rep_dev->parent_dev, qid);
	if (rc) {
		plt_err("Failed to start rxq %d, rc=%d", qid, rc);
		goto fail;
	}

	/* Start tx queues  */
	qid = rep_dev->txq->qid;
	rc = cnxk_eswitch_txq_start(rep_dev->parent_dev, qid);
	if (rc) {
		plt_err("Failed to start txq %d, rc=%d", qid, rc);
		goto fail;
	}

	/* Start rep_xport device only once after first representor gets active */
	if (!rep_dev->parent_dev->repr_cnt.nb_repr_started) {
		rc = cnxk_eswitch_nix_rsrc_start(rep_dev->parent_dev);
		if (rc) {
			plt_err("Failed to start nix dev, rc %d", rc);
			goto fail;
		}
	}

	ethdev->data->tx_queue_state[0] = RTE_ETH_QUEUE_STATE_STARTED;
	ethdev->data->rx_queue_state[0] = RTE_ETH_QUEUE_STATE_STARTED;

	rep_dev->parent_dev->repr_cnt.nb_repr_started++;

	link.link_status = RTE_ETH_LINK_UP;
	link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	link.link_autoneg = RTE_ETH_LINK_FIXED;
	link.link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
	rte_eth_linkstatus_set(ethdev, &link);
	ethdev->data->dev_started = 1;

	return 0;
fail:
	return rc;
}

int
cnxk_rep_dev_close(struct rte_eth_dev *ethdev)
{
	return cnxk_rep_dev_uninit(ethdev);
}

int
cnxk_rep_dev_stop(struct rte_eth_dev *ethdev)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);
	struct rte_eth_link link;

	ethdev->rx_pkt_burst = cnxk_rep_rx_burst_dummy;
	ethdev->tx_pkt_burst = cnxk_rep_tx_burst_dummy;
	cnxk_rep_rx_queue_stop(ethdev, 0);
	cnxk_rep_tx_queue_stop(ethdev, 0);
	rep_dev->parent_dev->repr_cnt.nb_repr_started--;

	/* Bring down link status internally */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(ethdev, &link);
	ethdev->data->dev_started = 0;

	return 0;
}

int
cnxk_rep_rx_queue_setup(struct rte_eth_dev *ethdev, uint16_t rx_queue_id, uint16_t nb_rx_desc,
			unsigned int socket_id, const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mb_pool)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);
	struct cnxk_rep_rxq *rxq = NULL;
	uint16_t qid = 0;
	int rc;

	PLT_SET_USED(socket_id);
	/* If no representee assigned, store the respective rxq parameters */
	if (!rep_dev->is_vf_active && !rep_dev->rxq) {
		rxq = plt_zmalloc(sizeof(*rxq), RTE_CACHE_LINE_SIZE);
		if (!rxq) {
			rc = -ENOMEM;
			plt_err("Failed to alloc RxQ for rep id %d", rep_dev->rep_id);
			goto fail;
		}

		rxq->qid = qid;
		rxq->nb_desc = nb_rx_desc;
		rxq->rep_dev = rep_dev;
		rxq->mpool = mb_pool;
		rxq->rx_conf = rx_conf;
		rep_dev->rxq = rxq;
		ethdev->data->rx_queues[rx_queue_id] = NULL;

		return 0;
	}

	qid = rep_dev->rep_id;
	rc = cnxk_eswitch_rxq_setup(rep_dev->parent_dev, qid, nb_rx_desc, rx_conf, mb_pool);
	if (rc) {
		plt_err("failed to setup eswitch queue id %d", qid);
		goto fail;
	}

	rxq = rep_dev->rxq;
	if (!rxq) {
		plt_err("Invalid RXQ handle for representor port %d rep id %d", rep_dev->port_id,
			rep_dev->rep_id);
		goto free_queue;
	}

	rxq->qid = qid;
	ethdev->data->rx_queues[rx_queue_id] = rxq;
	ethdev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	plt_rep_dbg("representor id %d portid %d rxq id %d", rep_dev->port_id,
		    ethdev->data->port_id, rxq->qid);

	return 0;
free_queue:
	cnxk_eswitch_rxq_release(rep_dev->parent_dev, qid);
fail:
	return rc;
}

void
cnxk_rep_rx_queue_stop(struct rte_eth_dev *ethdev, uint16_t queue_id)
{
	struct cnxk_rep_rxq *rxq = ethdev->data->rx_queues[queue_id];
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);
	int rc;

	if (!rxq)
		return;

	plt_rep_dbg("Stopping rxq %u", rxq->qid);

	rc = cnxk_eswitch_rxq_stop(rep_dev->parent_dev, rxq->qid);
	if (rc)
		plt_err("Failed to stop rxq %d, rc=%d", rc, rxq->qid);

	ethdev->data->rx_queue_state[queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
}

void
cnxk_rep_rx_queue_release(struct rte_eth_dev *ethdev, uint16_t queue_id)
{
	struct cnxk_rep_rxq *rxq = ethdev->data->rx_queues[queue_id];
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);
	int rc;

	if (!rxq) {
		plt_err("Invalid rxq retrieved for rep_id %d", rep_dev->rep_id);
		return;
	}

	plt_rep_dbg("Releasing rxq %u", rxq->qid);

	rc = cnxk_eswitch_rxq_release(rep_dev->parent_dev, rxq->qid);
	if (rc)
		plt_err("Failed to release rxq %d, rc=%d", rc, rxq->qid);
}

int
cnxk_rep_tx_queue_setup(struct rte_eth_dev *ethdev, uint16_t tx_queue_id, uint16_t nb_tx_desc,
			unsigned int socket_id, const struct rte_eth_txconf *tx_conf)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);
	struct cnxk_rep_txq *txq = NULL;
	int rc = 0, qid = 0;

	PLT_SET_USED(socket_id);
	/* If no representee assigned, store the respective rxq parameters */
	if (!rep_dev->is_vf_active && !rep_dev->txq) {
		txq = plt_zmalloc(sizeof(*txq), RTE_CACHE_LINE_SIZE);
		if (!txq) {
			rc = -ENOMEM;
			plt_err("failed to alloc txq for rep id %d", rep_dev->rep_id);
			goto free_queue;
		}

		txq->qid = qid;
		txq->nb_desc = nb_tx_desc;
		txq->tx_conf = tx_conf;
		txq->rep_dev = rep_dev;
		rep_dev->txq = txq;

		ethdev->data->tx_queues[tx_queue_id] = NULL;

		return 0;
	}

	qid = rep_dev->rep_id;
	rc = cnxk_eswitch_txq_setup(rep_dev->parent_dev, qid, nb_tx_desc, tx_conf);
	if (rc) {
		plt_err("failed to setup eswitch queue id %d", qid);
		goto fail;
	}

	txq = rep_dev->txq;
	if (!txq) {
		plt_err("Invalid TXQ handle for representor port %d rep id %d", rep_dev->port_id,
			rep_dev->rep_id);
		goto free_queue;
	}

	txq->qid = qid;
	ethdev->data->tx_queues[tx_queue_id] = txq;
	ethdev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	plt_rep_dbg("representor id %d portid %d txq id %d", rep_dev->port_id,
		    ethdev->data->port_id, txq->qid);

	return 0;
free_queue:
	cnxk_eswitch_txq_release(rep_dev->parent_dev, qid);
fail:
	return rc;
}

void
cnxk_rep_tx_queue_stop(struct rte_eth_dev *ethdev, uint16_t queue_id)
{
	struct cnxk_rep_txq *txq = ethdev->data->tx_queues[queue_id];
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);
	int rc;

	if (!txq)
		return;

	plt_rep_dbg("Releasing txq %u", txq->qid);

	rc = cnxk_eswitch_txq_stop(rep_dev->parent_dev, txq->qid);
	if (rc)
		plt_err("Failed to stop txq %d, rc=%d", rc, txq->qid);

	ethdev->data->tx_queue_state[queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
}

void
cnxk_rep_tx_queue_release(struct rte_eth_dev *ethdev, uint16_t queue_id)
{
	struct cnxk_rep_txq *txq = ethdev->data->tx_queues[queue_id];
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);
	int rc;

	if (!txq) {
		plt_err("Invalid txq retrieved for rep_id %d", rep_dev->rep_id);
		return;
	}

	plt_rep_dbg("Releasing txq %u", txq->qid);

	rc = cnxk_eswitch_txq_release(rep_dev->parent_dev, txq->qid);
	if (rc)
		plt_err("Failed to release txq %d, rc=%d", rc, txq->qid);
}

static int
process_eth_stats(struct cnxk_rep_dev *rep_dev, cnxk_rep_msg_ack_data_t *adata, cnxk_rep_msg_t msg)
{
	cnxk_rep_msg_eth_stats_meta_t msg_st_meta;
	uint32_t len = 0, rc;
	void *buffer;
	size_t size;

	size = CNXK_REP_MSG_MAX_BUFFER_SZ;
	buffer = plt_zmalloc(size, 0);
	if (!buffer) {
		plt_err("Failed to allocate mem");
		rc = -ENOMEM;
		goto fail;
	}

	cnxk_rep_msg_populate_header(buffer, &len);

	msg_st_meta.portid = rep_dev->rep_id;
	cnxk_rep_msg_populate_command_meta(buffer, &len, &msg_st_meta,
					   sizeof(cnxk_rep_msg_eth_stats_meta_t), msg);
	cnxk_rep_msg_populate_msg_end(buffer, &len);

	rc = cnxk_rep_msg_send_process(rep_dev, buffer, len, adata);
	if (rc) {
		plt_err("Failed to process the message, err %d", rc);
		goto fail;
	}

	rte_free(buffer);

	return 0;
fail:
	rte_free(buffer);
	return rc;
}

static int
native_repte_eth_stats(struct cnxk_rep_dev *rep_dev, struct rte_eth_stats *stats)
{
	struct roc_nix_stats nix_stats;
	int rc = 0;

	rc = roc_eswitch_nix_repte_stats(&rep_dev->parent_dev->nix, rep_dev->hw_func, &nix_stats);
	if (rc) {
		plt_err("Failed to get stats for representee %x, err %d", rep_dev->hw_func, rc);
		goto fail;
	}

	memset(stats, 0, sizeof(struct rte_eth_stats));
	stats->opackets = nix_stats.tx_ucast;
	stats->opackets += nix_stats.tx_mcast;
	stats->opackets += nix_stats.tx_bcast;
	stats->oerrors = nix_stats.tx_drop;
	stats->obytes = nix_stats.tx_octs;

	stats->ipackets = nix_stats.rx_ucast;
	stats->ipackets += nix_stats.rx_mcast;
	stats->ipackets += nix_stats.rx_bcast;
	stats->imissed = nix_stats.rx_drop;
	stats->ibytes = nix_stats.rx_octs;
	stats->ierrors = nix_stats.rx_err;

	return 0;
fail:
	return rc;
}

int
cnxk_rep_stats_get(struct rte_eth_dev *ethdev, struct rte_eth_stats *stats)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);
	struct rte_eth_stats vf_stats;
	cnxk_rep_msg_ack_data_t adata;
	int rc;

	/* If representor not representing any active VF, return 0 */
	if (!rep_dev->is_vf_active)
		return 0;

	if (rep_dev->native_repte) {
		/* For representees which are independent */
		rc = native_repte_eth_stats(rep_dev, &vf_stats);
		if (rc) {
			plt_err("Failed to get stats for vf rep %x (hw_func %x), err %d",
				rep_dev->port_id, rep_dev->hw_func, rc);
			goto fail;
		}
	} else {
		/* For representees which are part of companian app */
		rc = process_eth_stats(rep_dev, &adata, CNXK_REP_MSG_ETH_STATS_GET);
		if (rc || adata.u.sval < 0) {
			if (adata.u.sval < 0)
				rc = adata.u.sval;

			plt_err("Failed to get stats for vf rep %x, err %d", rep_dev->port_id, rc);
		}

		if (adata.size != sizeof(struct rte_eth_stats)) {
			rc = -EINVAL;
			plt_err("Incomplete stats received for vf rep %d", rep_dev->port_id);
			goto fail;
		}

		rte_memcpy(&vf_stats, adata.u.data, adata.size);
	}

	stats->q_ipackets[0] = vf_stats.ipackets;
	stats->q_ibytes[0] = vf_stats.ibytes;
	stats->ipackets = vf_stats.ipackets;
	stats->ibytes = vf_stats.ibytes;

	stats->q_opackets[0] = vf_stats.opackets;
	stats->q_obytes[0] = vf_stats.obytes;
	stats->opackets = vf_stats.opackets;
	stats->obytes = vf_stats.obytes;

	plt_rep_dbg("Input packets %" PRId64 " Output packets %" PRId64 "", stats->ipackets,
		    stats->opackets);

	return 0;
fail:
	return rc;
}

int
cnxk_rep_stats_reset(struct rte_eth_dev *ethdev)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(ethdev);
	cnxk_rep_msg_ack_data_t adata;
	int rc = 0;

	/* If representor not representing any active VF, return 0 */
	if (!rep_dev->is_vf_active)
		return 0;

	if (rep_dev->native_repte)
		return -ENOTSUP;

	rc = process_eth_stats(rep_dev, &adata, CNXK_REP_MSG_ETH_STATS_CLEAR);
	if (rc || adata.u.sval < 0) {
		if (adata.u.sval < 0)
			rc = adata.u.sval;

		plt_err("Failed to clear stats for vf rep %x, err %d", rep_dev->port_id, rc);
	}

	return rc;
}

int
cnxk_rep_flow_ops_get(struct rte_eth_dev *ethdev, const struct rte_flow_ops **ops)
{
	PLT_SET_USED(ethdev);
	*ops = &cnxk_rep_flow_ops;

	return 0;
}

int
cnxk_rep_mac_addr_set(struct rte_eth_dev *eth_dev, struct rte_ether_addr *addr)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);
	cnxk_rep_msg_eth_set_mac_meta_t msg_sm_meta;
	cnxk_rep_msg_ack_data_t adata;
	uint32_t len = 0, rc;
	void *buffer;
	size_t size;

	/* If representor not representing any VF, return 0 */
	if (!rep_dev->is_vf_active)
		return 0;

	size = CNXK_REP_MSG_MAX_BUFFER_SZ;
	buffer = plt_zmalloc(size, 0);
	if (!buffer) {
		plt_err("Failed to allocate mem");
		rc = -ENOMEM;
		goto fail;
	}

	cnxk_rep_msg_populate_header(buffer, &len);

	msg_sm_meta.portid = rep_dev->rep_id;
	rte_memcpy(&msg_sm_meta.addr_bytes, addr->addr_bytes, RTE_ETHER_ADDR_LEN);
	cnxk_rep_msg_populate_command_meta(buffer, &len, &msg_sm_meta,
					   sizeof(cnxk_rep_msg_eth_set_mac_meta_t),
					   CNXK_REP_MSG_ETH_SET_MAC);
	cnxk_rep_msg_populate_msg_end(buffer, &len);

	rc = cnxk_rep_msg_send_process(rep_dev, buffer, len, &adata);
	if (rc) {
		plt_err("Failed to process the message, err %d", rc);
		goto fail;
	}

	if (adata.u.sval < 0) {
		rc = adata.u.sval;
		plt_err("Failed to set mac address, err %d", rc);
		goto fail;
	}

	rte_free(buffer);

	return 0;
fail:
	rte_free(buffer);
	return rc;
}

int
cnxk_rep_xstats_get(struct rte_eth_dev *eth_dev, struct rte_eth_xstat *stats, unsigned int n)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);
	unsigned int num = RTE_DIM(cnxk_rep_xstats_string);
	int cnt = 0;

	if (!rep_dev)
		return -EINVAL;

	if (n < num)
		return num;

	stats[cnt].id = cnt;
	stats[cnt].value = rep_dev->rxq->stats.pkts;
	cnt++;
	stats[cnt].id = cnt;
	stats[cnt].value = rep_dev->txq->stats.pkts;
	cnt++;

	return cnt;
}

int
cnxk_rep_xstats_reset(struct rte_eth_dev *eth_dev)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);
	int rc;

	if (!rep_dev)
		return -EINVAL;

	rc = cnxk_rep_stats_reset(eth_dev);
	if (rc < 0 && rc != -ENOTSUP)
		return rc;

	rep_dev->rxq->stats.pkts = 0;
	rep_dev->txq->stats.pkts = 0;

	return 0;
}

int
cnxk_rep_xstats_get_names(__rte_unused struct rte_eth_dev *eth_dev,
			  struct rte_eth_xstat_name *xstats_names, unsigned int n)
{
	unsigned int num = RTE_DIM(cnxk_rep_xstats_string);
	unsigned int i;

	if (xstats_names == NULL)
		return num;

	if (n < num)
		return num;

	for (i = 0; i < num; i++)
		rte_strscpy(xstats_names[i].name, cnxk_rep_xstats_string[i].name,
			    sizeof(xstats_names[i].name));

	return num;
}

int
cnxk_rep_xstats_get_by_id(struct rte_eth_dev *eth_dev, const uint64_t *ids, uint64_t *values,
			  unsigned int n)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);
	unsigned int num = RTE_DIM(cnxk_rep_xstats_string);
	unsigned int i;

	if (!rep_dev)
		return -EINVAL;

	if (n < num)
		return num;

	if (n > num)
		return -EINVAL;

	for (i = 0; i < n; i++) {
		switch (ids[i]) {
		case 0:
			values[i] = rep_dev->rxq->stats.pkts;
			break;
		case 1:
			values[i] = rep_dev->txq->stats.pkts;
			break;
		default:
			return -EINVAL;
		}
	}

	return n;
}

int
cnxk_rep_xstats_get_names_by_id(__rte_unused struct rte_eth_dev *eth_dev, const uint64_t *ids,
				struct rte_eth_xstat_name *xstats_names, unsigned int n)
{
	unsigned int num = RTE_DIM(cnxk_rep_xstats_string);
	unsigned int i;

	if (n < num)
		return num;

	if (n > num)
		return -EINVAL;

	for (i = 0; i < n; i++) {
		if (ids[i] >= num)
			return -EINVAL;
		rte_strscpy(xstats_names[i].name, cnxk_rep_xstats_string[ids[i]].name,
			    sizeof(xstats_names[i].name));
	}

	return n;
}

int
cnxk_rep_mtu_set(struct rte_eth_dev *eth_dev, uint16_t mtu)
{
	struct cnxk_rep_dev *rep_dev = cnxk_rep_pmd_priv(eth_dev);
	uint32_t frame_size = mtu + CNXK_NIX_L2_OVERHEAD;
	int rc = -EINVAL;

	/* Check if MTU is within the allowed range */
	if ((frame_size - RTE_ETHER_CRC_LEN) < NIX_MIN_HW_FRS) {
		plt_err("MTU is lesser than minimum");
		goto exit;
	}

	if ((frame_size - RTE_ETHER_CRC_LEN) >
	    ((uint32_t)roc_nix_max_pkt_len(&rep_dev->parent_dev->nix))) {
		plt_err("MTU is greater than maximum");
		goto exit;
	}

	frame_size -= RTE_ETHER_CRC_LEN;

	/* Set frame size on Rx */
	rc = roc_nix_mac_max_rx_len_set(&rep_dev->parent_dev->nix, frame_size);
	if (rc) {
		plt_err("Failed to max Rx frame length, rc=%d", rc);
		goto exit;
	}
exit:
	return rc;
}

/* CNXK platform representor dev ops */
struct eth_dev_ops cnxk_rep_dev_ops = {
	.dev_infos_get = cnxk_rep_dev_info_get,
	.representor_info_get = cnxk_rep_representor_info_get,
	.dev_configure = cnxk_rep_dev_configure,
	.dev_start = cnxk_rep_dev_start,
	.rx_queue_setup = cnxk_rep_rx_queue_setup,
	.rx_queue_release = cnxk_rep_rx_queue_release,
	.tx_queue_setup = cnxk_rep_tx_queue_setup,
	.tx_queue_release = cnxk_rep_tx_queue_release,
	.promiscuous_enable   = cnxk_rep_promiscuous_enable,
	.promiscuous_disable   = cnxk_rep_promiscuous_disable,
	.mac_addr_set = cnxk_rep_mac_addr_set,
	.link_update = cnxk_rep_link_update,
	.dev_close = cnxk_rep_dev_close,
	.dev_stop = cnxk_rep_dev_stop,
	.stats_get = cnxk_rep_stats_get,
	.stats_reset = cnxk_rep_stats_reset,
	.flow_ops_get = cnxk_rep_flow_ops_get,
	.xstats_get = cnxk_rep_xstats_get,
	.xstats_reset = cnxk_rep_xstats_reset,
	.xstats_get_names = cnxk_rep_xstats_get_names,
	.xstats_get_by_id = cnxk_rep_xstats_get_by_id,
	.xstats_get_names_by_id = cnxk_rep_xstats_get_names_by_id,
	.mtu_set = cnxk_rep_mtu_set
};
