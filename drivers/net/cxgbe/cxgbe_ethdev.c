/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014-2015 Chelsio Communications.
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
 *     * Neither the name of Chelsio Communications nor the names of its
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
#include <inttypes.h>
#include <netinet/in.h>

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
#include <rte_random.h>
#include <rte_dev.h>

#include "cxgbe.h"

/*
 * Macros needed to support the PCI Device ID Table ...
 */
#define CH_PCI_DEVICE_ID_TABLE_DEFINE_BEGIN \
	static struct rte_pci_id cxgb4_pci_tbl[] = {
#define CH_PCI_DEVICE_ID_FUNCTION 0x4

#define PCI_VENDOR_ID_CHELSIO 0x1425

#define CH_PCI_ID_TABLE_ENTRY(devid) \
		{ RTE_PCI_DEVICE(PCI_VENDOR_ID_CHELSIO, (devid)) }

#define CH_PCI_DEVICE_ID_TABLE_DEFINE_END \
		{ .vendor_id = 0, } \
	}

/*
 *... and the PCI ID Table itself ...
 */
#include "t4_pci_id_tbl.h"

static uint16_t cxgbe_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
				uint16_t nb_pkts)
{
	struct sge_eth_txq *txq = (struct sge_eth_txq *)tx_queue;
	uint16_t pkts_sent, pkts_remain;
	uint16_t total_sent = 0;
	int ret = 0;

	CXGBE_DEBUG_TX(adapter, "%s: txq = %p; tx_pkts = %p; nb_pkts = %d\n",
		       __func__, txq, tx_pkts, nb_pkts);

	t4_os_lock(&txq->txq_lock);
	/* free up desc from already completed tx */
	reclaim_completed_tx(&txq->q);
	while (total_sent < nb_pkts) {
		pkts_remain = nb_pkts - total_sent;

		for (pkts_sent = 0; pkts_sent < pkts_remain; pkts_sent++) {
			ret = t4_eth_xmit(txq, tx_pkts[total_sent + pkts_sent]);
			if (ret < 0)
				break;
		}
		if (!pkts_sent)
			break;
		total_sent += pkts_sent;
		/* reclaim as much as possible */
		reclaim_completed_tx(&txq->q);
	}

	t4_os_unlock(&txq->txq_lock);
	return total_sent;
}

static uint16_t cxgbe_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
				uint16_t nb_pkts)
{
	struct sge_eth_rxq *rxq = (struct sge_eth_rxq *)rx_queue;
	unsigned int work_done;

	CXGBE_DEBUG_RX(adapter, "%s: rxq->rspq.cntxt_id = %u; nb_pkts = %d\n",
		       __func__, rxq->rspq.cntxt_id, nb_pkts);

	if (cxgbe_poll(&rxq->rspq, rx_pkts, (unsigned int)nb_pkts, &work_done))
		dev_err(adapter, "error in cxgbe poll\n");

	CXGBE_DEBUG_RX(adapter, "%s: work_done = %u\n", __func__, work_done);
	return work_done;
}

static void cxgbe_dev_info_get(struct rte_eth_dev *eth_dev,
			       struct rte_eth_dev_info *device_info)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;
	int max_queues = adapter->sge.max_ethqsets / adapter->params.nports;

	device_info->min_rx_bufsize = 68; /* XXX: Smallest pkt size */
	device_info->max_rx_pktlen = 1500; /* XXX: For now we support mtu */
	device_info->max_rx_queues = max_queues;
	device_info->max_tx_queues = max_queues;
	device_info->max_mac_addrs = 1;
	/* XXX: For now we support one MAC/port */
	device_info->max_vfs = adapter->params.arch.vfcount;
	device_info->max_vmdq_pools = 0; /* XXX: For now no support for VMDQ */

	device_info->rx_offload_capa = DEV_RX_OFFLOAD_VLAN_STRIP |
				       DEV_RX_OFFLOAD_IPV4_CKSUM |
				       DEV_RX_OFFLOAD_UDP_CKSUM |
				       DEV_RX_OFFLOAD_TCP_CKSUM;

	device_info->tx_offload_capa = DEV_TX_OFFLOAD_VLAN_INSERT |
				       DEV_TX_OFFLOAD_IPV4_CKSUM |
				       DEV_TX_OFFLOAD_UDP_CKSUM |
				       DEV_TX_OFFLOAD_TCP_CKSUM |
				       DEV_TX_OFFLOAD_TCP_TSO;

	device_info->reta_size = pi->rss_size;
}

static void cxgbe_dev_promiscuous_enable(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;

	t4_set_rxmode(adapter, adapter->mbox, pi->viid, -1,
		      1, -1, 1, -1, false);
}

static void cxgbe_dev_promiscuous_disable(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;

	t4_set_rxmode(adapter, adapter->mbox, pi->viid, -1,
		      0, -1, 1, -1, false);
}

static void cxgbe_dev_allmulticast_enable(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;

	/* TODO: address filters ?? */

	t4_set_rxmode(adapter, adapter->mbox, pi->viid, -1,
		      -1, 1, 1, -1, false);
}

static void cxgbe_dev_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;

	/* TODO: address filters ?? */

	t4_set_rxmode(adapter, adapter->mbox, pi->viid, -1,
		      -1, 0, 1, -1, false);
}

static int cxgbe_dev_link_update(struct rte_eth_dev *eth_dev,
				 __rte_unused int wait_to_complete)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;
	struct sge *s = &adapter->sge;
	struct rte_eth_link *old_link = &eth_dev->data->dev_link;
	unsigned int work_done, budget = 4;

	cxgbe_poll(&s->fw_evtq, NULL, budget, &work_done);
	if (old_link->link_status == pi->link_cfg.link_ok)
		return -1;  /* link not changed */

	eth_dev->data->dev_link.link_status = pi->link_cfg.link_ok;
	eth_dev->data->dev_link.link_duplex = ETH_LINK_FULL_DUPLEX;
	eth_dev->data->dev_link.link_speed = pi->link_cfg.speed;

	/* link has changed */
	return 0;
}

static int cxgbe_dev_tx_queue_start(struct rte_eth_dev *eth_dev,
				    uint16_t tx_queue_id);
static int cxgbe_dev_rx_queue_start(struct rte_eth_dev *eth_dev,
				    uint16_t tx_queue_id);
static void cxgbe_dev_tx_queue_release(void *q);
static void cxgbe_dev_rx_queue_release(void *q);

/*
 * Stop device.
 */
static void cxgbe_dev_close(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;
	int i, dev_down = 0;

	CXGBE_FUNC_TRACE();

	if (!(adapter->flags & FULL_INIT_DONE))
		return;

	cxgbe_down(pi);

	/*
	 *  We clear queues only if both tx and rx path of the port
	 *  have been disabled
	 */
	t4_sge_eth_clear_queues(pi);

	/*  See if all ports are down */
	for_each_port(adapter, i) {
		pi = adap2pinfo(adapter, i);
		/*
		 * Skip first port of the adapter since it will be closed
		 * by DPDK
		 */
		if (i == 0)
			continue;
		dev_down += (pi->eth_dev->data->dev_started == 0) ? 1 : 0;
	}

	/* If rest of the ports are stopped, then free up resources */
	if (dev_down == (adapter->params.nports - 1))
		cxgbe_close(adapter);
}

/* Start the device.
 * It returns 0 on success.
 */
static int cxgbe_dev_start(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;
	int err = 0, i;

	CXGBE_FUNC_TRACE();

	/*
	 * If we don't have a connection to the firmware there's nothing we
	 * can do.
	 */
	if (!(adapter->flags & FW_OK)) {
		err = -ENXIO;
		goto out;
	}

	if (!(adapter->flags & FULL_INIT_DONE)) {
		err = cxgbe_up(adapter);
		if (err < 0)
			goto out;
	}

	err = setup_rss(pi);
	if (err)
		goto out;

	for (i = 0; i < pi->n_tx_qsets; i++) {
		err = cxgbe_dev_tx_queue_start(eth_dev, i);
		if (err)
			goto out;
	}

	for (i = 0; i < pi->n_rx_qsets; i++) {
		err = cxgbe_dev_rx_queue_start(eth_dev, i);
		if (err)
			goto out;
	}

	err = link_start(pi);
	if (err)
		goto out;

out:
	return err;
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
static void cxgbe_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;

	CXGBE_FUNC_TRACE();

	if (!(adapter->flags & FULL_INIT_DONE))
		return;

	cxgbe_down(pi);

	/*
	 *  We clear queues only if both tx and rx path of the port
	 *  have been disabled
	 */
	t4_sge_eth_clear_queues(pi);
}

static int cxgbe_dev_configure(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;
	int err;

	CXGBE_FUNC_TRACE();

	if (!(adapter->flags & FW_QUEUE_BOUND)) {
		err = setup_sge_fwevtq(adapter);
		if (err)
			return err;
		adapter->flags |= FW_QUEUE_BOUND;
	}

	err = cfg_queue_count(eth_dev);
	if (err)
		return err;

	return 0;
}

static int cxgbe_dev_tx_queue_start(struct rte_eth_dev *eth_dev,
				    uint16_t tx_queue_id)
{
	struct sge_eth_txq *txq = (struct sge_eth_txq *)
				  (eth_dev->data->tx_queues[tx_queue_id]);

	dev_debug(NULL, "%s: tx_queue_id = %d\n", __func__, tx_queue_id);

	return t4_sge_eth_txq_start(txq);
}

static int cxgbe_dev_tx_queue_stop(struct rte_eth_dev *eth_dev,
				   uint16_t tx_queue_id)
{
	struct sge_eth_txq *txq = (struct sge_eth_txq *)
				  (eth_dev->data->tx_queues[tx_queue_id]);

	dev_debug(NULL, "%s: tx_queue_id = %d\n", __func__, tx_queue_id);

	return t4_sge_eth_txq_stop(txq);
}

static int cxgbe_dev_tx_queue_setup(struct rte_eth_dev *eth_dev,
				    uint16_t queue_idx,	uint16_t nb_desc,
				    unsigned int socket_id,
				    const struct rte_eth_txconf *tx_conf)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;
	struct sge *s = &adapter->sge;
	struct sge_eth_txq *txq = &s->ethtxq[pi->first_qset + queue_idx];
	int err = 0;
	unsigned int temp_nb_desc;

	RTE_SET_USED(tx_conf);

	dev_debug(adapter, "%s: eth_dev->data->nb_tx_queues = %d; queue_idx = %d; nb_desc = %d; socket_id = %d; pi->first_qset = %u\n",
		  __func__, eth_dev->data->nb_tx_queues, queue_idx, nb_desc,
		  socket_id, pi->first_qset);

	/*  Free up the existing queue  */
	if (eth_dev->data->tx_queues[queue_idx]) {
		cxgbe_dev_tx_queue_release(eth_dev->data->tx_queues[queue_idx]);
		eth_dev->data->tx_queues[queue_idx] = NULL;
	}

	eth_dev->data->tx_queues[queue_idx] = (void *)txq;

	/* Sanity Checking
	 *
	 * nb_desc should be > 1023 and <= CXGBE_MAX_RING_DESC_SIZE
	 */
	temp_nb_desc = nb_desc;
	if (nb_desc < CXGBE_MIN_RING_DESC_SIZE) {
		dev_warn(adapter, "%s: number of descriptors must be >= %d. Using default [%d]\n",
			 __func__, CXGBE_MIN_RING_DESC_SIZE,
			 CXGBE_DEFAULT_TX_DESC_SIZE);
		temp_nb_desc = CXGBE_DEFAULT_TX_DESC_SIZE;
	} else if (nb_desc > CXGBE_MAX_RING_DESC_SIZE) {
		dev_err(adapter, "%s: number of descriptors must be between %d and %d inclusive. Default [%d]\n",
			__func__, CXGBE_MIN_RING_DESC_SIZE,
			CXGBE_MAX_RING_DESC_SIZE, CXGBE_DEFAULT_TX_DESC_SIZE);
		return -(EINVAL);
	}

	txq->q.size = temp_nb_desc;

	err = t4_sge_alloc_eth_txq(adapter, txq, eth_dev, queue_idx,
				   s->fw_evtq.cntxt_id, socket_id);

	dev_debug(adapter, "%s: txq->q.cntxt_id= %d err = %d\n",
		  __func__, txq->q.cntxt_id, err);

	return err;
}

static void cxgbe_dev_tx_queue_release(void *q)
{
	struct sge_eth_txq *txq = (struct sge_eth_txq *)q;

	if (txq) {
		struct port_info *pi = (struct port_info *)
				       (txq->eth_dev->data->dev_private);
		struct adapter *adap = pi->adapter;

		dev_debug(adapter, "%s: pi->port_id = %d; tx_queue_id = %d\n",
			  __func__, pi->port_id, txq->q.cntxt_id);

		t4_sge_eth_txq_release(adap, txq);
	}
}

static int cxgbe_dev_rx_queue_start(struct rte_eth_dev *eth_dev,
				    uint16_t rx_queue_id)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adap = pi->adapter;
	struct sge_rspq *q;

	dev_debug(adapter, "%s: pi->port_id = %d; rx_queue_id = %d\n",
		  __func__, pi->port_id, rx_queue_id);

	q = eth_dev->data->rx_queues[rx_queue_id];
	return t4_sge_eth_rxq_start(adap, q);
}

static int cxgbe_dev_rx_queue_stop(struct rte_eth_dev *eth_dev,
				   uint16_t rx_queue_id)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adap = pi->adapter;
	struct sge_rspq *q;

	dev_debug(adapter, "%s: pi->port_id = %d; rx_queue_id = %d\n",
		  __func__, pi->port_id, rx_queue_id);

	q = eth_dev->data->rx_queues[rx_queue_id];
	return t4_sge_eth_rxq_stop(adap, q);
}

static int cxgbe_dev_rx_queue_setup(struct rte_eth_dev *eth_dev,
				    uint16_t queue_idx,	uint16_t nb_desc,
				    unsigned int socket_id,
				    const struct rte_eth_rxconf *rx_conf,
				    struct rte_mempool *mp)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;
	struct sge *s = &adapter->sge;
	struct sge_eth_rxq *rxq = &s->ethrxq[pi->first_qset + queue_idx];
	int err = 0;
	int msi_idx = 0;
	unsigned int temp_nb_desc;

	RTE_SET_USED(rx_conf);

	dev_debug(adapter, "%s: eth_dev->data->nb_rx_queues = %d; queue_idx = %d; nb_desc = %d; socket_id = %d; mp = %p\n",
		  __func__, eth_dev->data->nb_rx_queues, queue_idx, nb_desc,
		  socket_id, mp);

	/*  Free up the existing queue  */
	if (eth_dev->data->rx_queues[queue_idx]) {
		cxgbe_dev_rx_queue_release(eth_dev->data->rx_queues[queue_idx]);
		eth_dev->data->rx_queues[queue_idx] = NULL;
	}

	eth_dev->data->rx_queues[queue_idx] = (void *)rxq;

	/* Sanity Checking
	 *
	 * nb_desc should be > 0 and <= CXGBE_MAX_RING_DESC_SIZE
	 */
	temp_nb_desc = nb_desc;
	if (nb_desc < CXGBE_MIN_RING_DESC_SIZE) {
		dev_warn(adapter, "%s: number of descriptors must be >= %d. Using default [%d]\n",
			 __func__, CXGBE_MIN_RING_DESC_SIZE,
			 CXGBE_DEFAULT_RX_DESC_SIZE);
		temp_nb_desc = CXGBE_DEFAULT_RX_DESC_SIZE;
	} else if (nb_desc > CXGBE_MAX_RING_DESC_SIZE) {
		dev_err(adapter, "%s: number of descriptors must be between %d and %d inclusive. Default [%d]\n",
			__func__, CXGBE_MIN_RING_DESC_SIZE,
			CXGBE_MAX_RING_DESC_SIZE, CXGBE_DEFAULT_RX_DESC_SIZE);
		return -(EINVAL);
	}

	rxq->rspq.size = temp_nb_desc;
	if ((&rxq->fl) != NULL)
		rxq->fl.size = temp_nb_desc;

	err = t4_sge_alloc_rxq(adapter, &rxq->rspq, false, eth_dev, msi_idx,
			       &rxq->fl, t4_ethrx_handler,
			       t4_get_mps_bg_map(adapter, pi->tx_chan), mp,
			       queue_idx, socket_id);

	dev_debug(adapter, "%s: err = %d; port_id = %d; cntxt_id = %u\n",
		  __func__, err, pi->port_id, rxq->rspq.cntxt_id);
	return err;
}

static void cxgbe_dev_rx_queue_release(void *q)
{
	struct sge_eth_rxq *rxq = (struct sge_eth_rxq *)q;
	struct sge_rspq *rq = &rxq->rspq;

	if (rq) {
		struct port_info *pi = (struct port_info *)
				       (rq->eth_dev->data->dev_private);
		struct adapter *adap = pi->adapter;

		dev_debug(adapter, "%s: pi->port_id = %d; rx_queue_id = %d\n",
			  __func__, pi->port_id, rxq->rspq.cntxt_id);

		t4_sge_eth_rxq_release(adap, rxq);
	}
}

/*
 * Get port statistics.
 */
static void cxgbe_dev_stats_get(struct rte_eth_dev *eth_dev,
				struct rte_eth_stats *eth_stats)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;
	struct sge *s = &adapter->sge;
	struct port_stats ps;
	unsigned int i;

	cxgbe_stats_get(pi, &ps);

	/* RX Stats */
	eth_stats->ipackets = ps.rx_frames;
	eth_stats->ibytes   = ps.rx_octets;
	eth_stats->imcasts  = ps.rx_mcast_frames;
	eth_stats->imissed  = ps.rx_ovflow0 + ps.rx_ovflow1 +
			      ps.rx_ovflow2 + ps.rx_ovflow3 +
			      ps.rx_trunc0 + ps.rx_trunc1 +
			      ps.rx_trunc2 + ps.rx_trunc3;
	eth_stats->ibadcrc  = ps.rx_fcs_err;
	eth_stats->ibadlen  = ps.rx_jabber + ps.rx_too_long + ps.rx_runt;
	eth_stats->ierrors  = ps.rx_symbol_err + eth_stats->ibadcrc +
			      eth_stats->ibadlen + ps.rx_len_err +
			      eth_stats->imissed;
	eth_stats->rx_pause_xon  = ps.rx_pause;

	/* TX Stats */
	eth_stats->opackets = ps.tx_frames;
	eth_stats->obytes   = ps.tx_octets;
	eth_stats->oerrors  = ps.tx_error_frames;
	eth_stats->tx_pause_xon  = ps.tx_pause;

	for (i = 0; i < pi->n_rx_qsets; i++) {
		struct sge_eth_rxq *rxq =
			&s->ethrxq[pi->first_qset + i];

		eth_stats->q_ipackets[i] = rxq->stats.pkts;
		eth_stats->q_ibytes[i] = rxq->stats.rx_bytes;
	}

	for (i = 0; i < pi->n_tx_qsets; i++) {
		struct sge_eth_txq *txq =
			&s->ethtxq[pi->first_qset + i];

		eth_stats->q_opackets[i] = txq->stats.pkts;
		eth_stats->q_obytes[i] = txq->stats.tx_bytes;
		eth_stats->q_errors[i] = txq->stats.mapping_err;
	}
}

/*
 * Reset port statistics.
 */
static void cxgbe_dev_stats_reset(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;
	struct sge *s = &adapter->sge;
	unsigned int i;

	cxgbe_stats_reset(pi);
	for (i = 0; i < pi->n_rx_qsets; i++) {
		struct sge_eth_rxq *rxq =
			&s->ethrxq[pi->first_qset + i];

		rxq->stats.pkts = 0;
		rxq->stats.rx_bytes = 0;
	}
	for (i = 0; i < pi->n_tx_qsets; i++) {
		struct sge_eth_txq *txq =
			&s->ethtxq[pi->first_qset + i];

		txq->stats.pkts = 0;
		txq->stats.tx_bytes = 0;
		txq->stats.mapping_err = 0;
	}
}

static int cxgbe_flow_ctrl_get(struct rte_eth_dev *eth_dev,
			       struct rte_eth_fc_conf *fc_conf)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct link_config *lc = &pi->link_cfg;
	int rx_pause, tx_pause;

	fc_conf->autoneg = lc->fc & PAUSE_AUTONEG;
	rx_pause = lc->fc & PAUSE_RX;
	tx_pause = lc->fc & PAUSE_TX;

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

static int cxgbe_flow_ctrl_set(struct rte_eth_dev *eth_dev,
			       struct rte_eth_fc_conf *fc_conf)
{
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = pi->adapter;
	struct link_config *lc = &pi->link_cfg;

	if (lc->supported & FW_PORT_CAP_ANEG) {
		if (fc_conf->autoneg)
			lc->requested_fc |= PAUSE_AUTONEG;
		else
			lc->requested_fc &= ~PAUSE_AUTONEG;
	}

	if (((fc_conf->mode & RTE_FC_FULL) == RTE_FC_FULL) ||
	    (fc_conf->mode & RTE_FC_RX_PAUSE))
		lc->requested_fc |= PAUSE_RX;
	else
		lc->requested_fc &= ~PAUSE_RX;

	if (((fc_conf->mode & RTE_FC_FULL) == RTE_FC_FULL) ||
	    (fc_conf->mode & RTE_FC_TX_PAUSE))
		lc->requested_fc |= PAUSE_TX;
	else
		lc->requested_fc &= ~PAUSE_TX;

	return t4_link_l1cfg(adapter, adapter->mbox, pi->tx_chan,
			     &pi->link_cfg);
}

static struct eth_dev_ops cxgbe_eth_dev_ops = {
	.dev_start		= cxgbe_dev_start,
	.dev_stop		= cxgbe_dev_stop,
	.dev_close		= cxgbe_dev_close,
	.promiscuous_enable	= cxgbe_dev_promiscuous_enable,
	.promiscuous_disable	= cxgbe_dev_promiscuous_disable,
	.allmulticast_enable	= cxgbe_dev_allmulticast_enable,
	.allmulticast_disable	= cxgbe_dev_allmulticast_disable,
	.dev_configure		= cxgbe_dev_configure,
	.dev_infos_get		= cxgbe_dev_info_get,
	.link_update		= cxgbe_dev_link_update,
	.tx_queue_setup         = cxgbe_dev_tx_queue_setup,
	.tx_queue_start		= cxgbe_dev_tx_queue_start,
	.tx_queue_stop		= cxgbe_dev_tx_queue_stop,
	.tx_queue_release	= cxgbe_dev_tx_queue_release,
	.rx_queue_setup         = cxgbe_dev_rx_queue_setup,
	.rx_queue_start		= cxgbe_dev_rx_queue_start,
	.rx_queue_stop		= cxgbe_dev_rx_queue_stop,
	.rx_queue_release	= cxgbe_dev_rx_queue_release,
	.stats_get		= cxgbe_dev_stats_get,
	.stats_reset		= cxgbe_dev_stats_reset,
	.flow_ctrl_get		= cxgbe_flow_ctrl_get,
	.flow_ctrl_set		= cxgbe_flow_ctrl_set,
};

/*
 * Initialize driver
 * It returns 0 on success.
 */
static int eth_cxgbe_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct port_info *pi = (struct port_info *)(eth_dev->data->dev_private);
	struct adapter *adapter = NULL;
	char name[RTE_ETH_NAME_MAX_LEN];
	int err = 0;

	CXGBE_FUNC_TRACE();

	eth_dev->dev_ops = &cxgbe_eth_dev_ops;
	eth_dev->rx_pkt_burst = &cxgbe_recv_pkts;
	eth_dev->tx_pkt_burst = &cxgbe_xmit_pkts;

	/* for secondary processes, we don't initialise any further as primary
	 * has already done this work.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pci_dev = eth_dev->pci_dev;
	snprintf(name, sizeof(name), "cxgbeadapter%d", eth_dev->data->port_id);
	adapter = rte_zmalloc(name, sizeof(*adapter), 0);
	if (!adapter)
		return -1;

	adapter->use_unpacked_mode = 1;
	adapter->regs = (void *)pci_dev->mem_resource[0].addr;
	if (!adapter->regs) {
		dev_err(adapter, "%s: cannot map device registers\n", __func__);
		err = -ENOMEM;
		goto out_free_adapter;
	}
	adapter->pdev = pci_dev;
	adapter->eth_dev = eth_dev;
	pi->adapter = adapter;

	err = cxgbe_probe(adapter);
	if (err)
		dev_err(adapter, "%s: cxgbe probe failed with err %d\n",
			__func__, err);

out_free_adapter:
	return err;
}

static struct eth_driver rte_cxgbe_pmd = {
	{
		.name = "rte_cxgbe_pmd",
		.id_table = cxgb4_pci_tbl,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	},
	.eth_dev_init = eth_cxgbe_dev_init,
	.dev_private_size = sizeof(struct port_info),
};

/*
 * Driver initialization routine.
 * Invoked once at EAL init time.
 * Register itself as the [Poll Mode] Driver of PCI CXGBE devices.
 */
static int rte_cxgbe_pmd_init(const char *name __rte_unused,
			      const char *params __rte_unused)
{
	CXGBE_FUNC_TRACE();

	rte_eth_driver_register(&rte_cxgbe_pmd);
	return 0;
}

static struct rte_driver rte_cxgbe_driver = {
	.name = "cxgbe_driver",
	.type = PMD_PDEV,
	.init = rte_cxgbe_pmd_init,
};

PMD_REGISTER_DRIVER(rte_cxgbe_driver);
