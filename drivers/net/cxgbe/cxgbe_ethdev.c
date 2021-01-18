/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Chelsio Communications.
 * All rights reserved.
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
#include <rte_bus_pci.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_dev.h>

#include "cxgbe.h"
#include "cxgbe_pfvf.h"
#include "cxgbe_flow.h"

/*
 * Macros needed to support the PCI Device ID Table ...
 */
#define CH_PCI_DEVICE_ID_TABLE_DEFINE_BEGIN \
	static const struct rte_pci_id cxgb4_pci_tbl[] = {
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
#include "base/t4_pci_id_tbl.h"

uint16_t cxgbe_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			 uint16_t nb_pkts)
{
	struct sge_eth_txq *txq = (struct sge_eth_txq *)tx_queue;
	uint16_t pkts_sent, pkts_remain;
	uint16_t total_sent = 0;
	uint16_t idx = 0;
	int ret = 0;

	t4_os_lock(&txq->txq_lock);
	/* free up desc from already completed tx */
	reclaim_completed_tx(&txq->q);
	if (unlikely(!nb_pkts))
		goto out_unlock;

	rte_prefetch0(rte_pktmbuf_mtod(tx_pkts[0], volatile void *));
	while (total_sent < nb_pkts) {
		pkts_remain = nb_pkts - total_sent;

		for (pkts_sent = 0; pkts_sent < pkts_remain; pkts_sent++) {
			idx = total_sent + pkts_sent;
			if ((idx + 1) < nb_pkts)
				rte_prefetch0(rte_pktmbuf_mtod(tx_pkts[idx + 1],
							volatile void *));
			ret = t4_eth_xmit(txq, tx_pkts[idx], nb_pkts);
			if (ret < 0)
				break;
		}
		if (!pkts_sent)
			break;
		total_sent += pkts_sent;
		/* reclaim as much as possible */
		reclaim_completed_tx(&txq->q);
	}

out_unlock:
	t4_os_unlock(&txq->txq_lock);
	return total_sent;
}

uint16_t cxgbe_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	struct sge_eth_rxq *rxq = (struct sge_eth_rxq *)rx_queue;
	unsigned int work_done;

	if (cxgbe_poll(&rxq->rspq, rx_pkts, (unsigned int)nb_pkts, &work_done))
		dev_err(adapter, "error in cxgbe poll\n");

	return work_done;
}

int cxgbe_dev_info_get(struct rte_eth_dev *eth_dev,
			struct rte_eth_dev_info *device_info)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;

	static const struct rte_eth_desc_lim cxgbe_desc_lim = {
		.nb_max = CXGBE_MAX_RING_DESC_SIZE,
		.nb_min = CXGBE_MIN_RING_DESC_SIZE,
		.nb_align = 1,
	};

	device_info->min_rx_bufsize = CXGBE_MIN_RX_BUFSIZE;
	device_info->max_rx_pktlen = CXGBE_MAX_RX_PKTLEN;
	device_info->max_rx_queues = adapter->sge.max_ethqsets;
	device_info->max_tx_queues = adapter->sge.max_ethqsets;
	device_info->max_mac_addrs = 1;
	/* XXX: For now we support one MAC/port */
	device_info->max_vfs = adapter->params.arch.vfcount;
	device_info->max_vmdq_pools = 0; /* XXX: For now no support for VMDQ */

	device_info->rx_queue_offload_capa = 0UL;
	device_info->rx_offload_capa = CXGBE_RX_OFFLOADS;

	device_info->tx_queue_offload_capa = 0UL;
	device_info->tx_offload_capa = CXGBE_TX_OFFLOADS;

	device_info->reta_size = pi->rss_size;
	device_info->hash_key_size = CXGBE_DEFAULT_RSS_KEY_LEN;
	device_info->flow_type_rss_offloads = CXGBE_RSS_HF_ALL;

	device_info->rx_desc_lim = cxgbe_desc_lim;
	device_info->tx_desc_lim = cxgbe_desc_lim;
	cxgbe_get_speed_caps(pi, &device_info->speed_capa);

	return 0;
}

int cxgbe_dev_promiscuous_enable(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;

	return t4_set_rxmode(adapter, adapter->mbox, pi->viid, -1,
			     1, -1, 1, -1, false);
}

int cxgbe_dev_promiscuous_disable(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;

	return t4_set_rxmode(adapter, adapter->mbox, pi->viid, -1,
			     0, -1, 1, -1, false);
}

int cxgbe_dev_allmulticast_enable(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;

	/* TODO: address filters ?? */

	return t4_set_rxmode(adapter, adapter->mbox, pi->viid, -1,
			     -1, 1, 1, -1, false);
}

int cxgbe_dev_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;

	/* TODO: address filters ?? */

	return t4_set_rxmode(adapter, adapter->mbox, pi->viid, -1,
			     -1, 0, 1, -1, false);
}

int cxgbe_dev_link_update(struct rte_eth_dev *eth_dev,
			  int wait_to_complete)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	struct sge *s = &adapter->sge;
	struct rte_eth_link new_link = { 0 };
	unsigned int i, work_done, budget = 32;
	u8 old_link = pi->link_cfg.link_ok;

	for (i = 0; i < CXGBE_LINK_STATUS_POLL_CNT; i++) {
		if (!s->fw_evtq.desc)
			break;

		cxgbe_poll(&s->fw_evtq, NULL, budget, &work_done);

		/* Exit if link status changed or always forced up */
		if (pi->link_cfg.link_ok != old_link ||
		    cxgbe_force_linkup(adapter))
			break;

		if (!wait_to_complete)
			break;

		rte_delay_ms(CXGBE_LINK_STATUS_POLL_MS);
	}

	new_link.link_status = cxgbe_force_linkup(adapter) ?
			       ETH_LINK_UP : pi->link_cfg.link_ok;
	new_link.link_autoneg = pi->link_cfg.autoneg;
	new_link.link_duplex = ETH_LINK_FULL_DUPLEX;
	new_link.link_speed = pi->link_cfg.speed;

	return rte_eth_linkstatus_set(eth_dev, &new_link);
}

/**
 * Set device link up.
 */
int cxgbe_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct port_info *pi = dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	unsigned int work_done, budget = 32;
	struct sge *s = &adapter->sge;
	int ret;

	if (!s->fw_evtq.desc)
		return -ENOMEM;

	/* Flush all link events */
	cxgbe_poll(&s->fw_evtq, NULL, budget, &work_done);

	/* If link already up, nothing to do */
	if (pi->link_cfg.link_ok)
		return 0;

	ret = cxgbe_set_link_status(pi, true);
	if (ret)
		return ret;

	cxgbe_dev_link_update(dev, 1);
	return 0;
}

/**
 * Set device link down.
 */
int cxgbe_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct port_info *pi = dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	unsigned int work_done, budget = 32;
	struct sge *s = &adapter->sge;
	int ret;

	if (!s->fw_evtq.desc)
		return -ENOMEM;

	/* Flush all link events */
	cxgbe_poll(&s->fw_evtq, NULL, budget, &work_done);

	/* If link already down, nothing to do */
	if (!pi->link_cfg.link_ok)
		return 0;

	ret = cxgbe_set_link_status(pi, false);
	if (ret)
		return ret;

	cxgbe_dev_link_update(dev, 0);
	return 0;
}

int cxgbe_dev_mtu_set(struct rte_eth_dev *eth_dev, uint16_t mtu)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	struct rte_eth_dev_info dev_info;
	int err;
	uint16_t new_mtu = mtu + RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;

	err = cxgbe_dev_info_get(eth_dev, &dev_info);
	if (err != 0)
		return err;

	/* Must accommodate at least RTE_ETHER_MIN_MTU */
	if (new_mtu < RTE_ETHER_MIN_MTU || new_mtu > dev_info.max_rx_pktlen)
		return -EINVAL;

	/* set to jumbo mode if needed */
	if (new_mtu > CXGBE_ETH_MAX_LEN)
		eth_dev->data->dev_conf.rxmode.offloads |=
			DEV_RX_OFFLOAD_JUMBO_FRAME;
	else
		eth_dev->data->dev_conf.rxmode.offloads &=
			~DEV_RX_OFFLOAD_JUMBO_FRAME;

	err = t4_set_rxmode(adapter, adapter->mbox, pi->viid, new_mtu, -1, -1,
			    -1, -1, true);
	if (!err)
		eth_dev->data->dev_conf.rxmode.max_rx_pkt_len = new_mtu;

	return err;
}

/*
 * Stop device.
 */
int cxgbe_dev_close(struct rte_eth_dev *eth_dev)
{
	struct port_info *temp_pi, *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	u8 i;

	CXGBE_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (!(adapter->flags & FULL_INIT_DONE))
		return 0;

	if (!pi->viid)
		return 0;

	cxgbe_down(pi);
	t4_sge_eth_release_queues(pi);
	t4_free_vi(adapter, adapter->mbox, adapter->pf, 0, pi->viid);
	pi->viid = 0;

	/* Free up the adapter-wide resources only after all the ports
	 * under this PF have been closed.
	 */
	for_each_port(adapter, i) {
		temp_pi = adap2pinfo(adapter, i);
		if (temp_pi->viid)
			return 0;
	}

	cxgbe_close(adapter);
	rte_free(adapter);

	return 0;
}

/* Start the device.
 * It returns 0 on success.
 */
int cxgbe_dev_start(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct rte_eth_rxmode *rx_conf = &eth_dev->data->dev_conf.rxmode;
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

	if (rx_conf->offloads & DEV_RX_OFFLOAD_SCATTER)
		eth_dev->data->scattered_rx = 1;
	else
		eth_dev->data->scattered_rx = 0;

	cxgbe_enable_rx_queues(pi);

	err = cxgbe_setup_rss(pi);
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

	err = cxgbe_link_start(pi);
	if (err)
		goto out;

out:
	return err;
}

/*
 * Stop device: disable rx and tx functions to allow for reconfiguring.
 */
int cxgbe_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;

	CXGBE_FUNC_TRACE();

	if (!(adapter->flags & FULL_INIT_DONE))
		return 0;

	cxgbe_down(pi);

	/*
	 *  We clear queues only if both tx and rx path of the port
	 *  have been disabled
	 */
	t4_sge_eth_clear_queues(pi);
	eth_dev->data->scattered_rx = 0;

	return 0;
}

int cxgbe_dev_configure(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	int err;

	CXGBE_FUNC_TRACE();

	if (eth_dev->data->dev_conf.rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG)
		eth_dev->data->dev_conf.rxmode.offloads |=
			DEV_RX_OFFLOAD_RSS_HASH;

	if (!(adapter->flags & FW_QUEUE_BOUND)) {
		err = cxgbe_setup_sge_fwevtq(adapter);
		if (err)
			return err;
		adapter->flags |= FW_QUEUE_BOUND;
		if (is_pf4(adapter)) {
			err = cxgbe_setup_sge_ctrl_txq(adapter);
			if (err)
				return err;
		}
	}

	err = cxgbe_cfg_queue_count(eth_dev);
	if (err)
		return err;

	return 0;
}

int cxgbe_dev_tx_queue_start(struct rte_eth_dev *eth_dev, uint16_t tx_queue_id)
{
	int ret;
	struct sge_eth_txq *txq = (struct sge_eth_txq *)
				  (eth_dev->data->tx_queues[tx_queue_id]);

	dev_debug(NULL, "%s: tx_queue_id = %d\n", __func__, tx_queue_id);

	ret = t4_sge_eth_txq_start(txq);
	if (ret == 0)
		eth_dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return ret;
}

int cxgbe_dev_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t tx_queue_id)
{
	int ret;
	struct sge_eth_txq *txq = (struct sge_eth_txq *)
				  (eth_dev->data->tx_queues[tx_queue_id]);

	dev_debug(NULL, "%s: tx_queue_id = %d\n", __func__, tx_queue_id);

	ret = t4_sge_eth_txq_stop(txq);
	if (ret == 0)
		eth_dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return ret;
}

int cxgbe_dev_tx_queue_setup(struct rte_eth_dev *eth_dev,
			     uint16_t queue_idx, uint16_t nb_desc,
			     unsigned int socket_id,
			     const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	struct sge *s = &adapter->sge;
	unsigned int temp_nb_desc;
	struct sge_eth_txq *txq;
	int err = 0;

	txq = &s->ethtxq[pi->first_txqset + queue_idx];
	dev_debug(adapter, "%s: eth_dev->data->nb_tx_queues = %d; queue_idx = %d; nb_desc = %d; socket_id = %d; pi->first_qset = %u\n",
		  __func__, eth_dev->data->nb_tx_queues, queue_idx, nb_desc,
		  socket_id, pi->first_txqset);

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

	dev_debug(adapter, "%s: txq->q.cntxt_id= %u txq->q.abs_id= %u err = %d\n",
		  __func__, txq->q.cntxt_id, txq->q.abs_id, err);
	return err;
}

void cxgbe_dev_tx_queue_release(void *q)
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

int cxgbe_dev_rx_queue_start(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adap = pi->adapter;
	struct sge_eth_rxq *rxq;
	int ret;

	dev_debug(adapter, "%s: pi->port_id = %d; rx_queue_id = %d\n",
		  __func__, pi->port_id, rx_queue_id);

	rxq = eth_dev->data->rx_queues[rx_queue_id];
	ret = t4_sge_eth_rxq_start(adap, rxq);
	if (ret == 0)
		eth_dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return ret;
}

int cxgbe_dev_rx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adap = pi->adapter;
	struct sge_eth_rxq *rxq;
	int ret;

	dev_debug(adapter, "%s: pi->port_id = %d; rx_queue_id = %d\n",
		  __func__, pi->port_id, rx_queue_id);

	rxq = eth_dev->data->rx_queues[rx_queue_id];
	ret = t4_sge_eth_rxq_stop(adap, rxq);
	if (ret == 0)
		eth_dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return ret;
}

int cxgbe_dev_rx_queue_setup(struct rte_eth_dev *eth_dev,
			     uint16_t queue_idx, uint16_t nb_desc,
			     unsigned int socket_id,
			     const struct rte_eth_rxconf *rx_conf __rte_unused,
			     struct rte_mempool *mp)
{
	unsigned int pkt_len = eth_dev->data->dev_conf.rxmode.max_rx_pkt_len;
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	struct rte_eth_dev_info dev_info;
	struct sge *s = &adapter->sge;
	unsigned int temp_nb_desc;
	int err = 0, msi_idx = 0;
	struct sge_eth_rxq *rxq;

	rxq = &s->ethrxq[pi->first_rxqset + queue_idx];
	dev_debug(adapter, "%s: eth_dev->data->nb_rx_queues = %d; queue_idx = %d; nb_desc = %d; socket_id = %d; mp = %p\n",
		  __func__, eth_dev->data->nb_rx_queues, queue_idx, nb_desc,
		  socket_id, mp);

	err = cxgbe_dev_info_get(eth_dev, &dev_info);
	if (err != 0) {
		dev_err(adap, "%s: error during getting ethernet device info",
			__func__);
		return err;
	}

	/* Must accommodate at least RTE_ETHER_MIN_MTU */
	if ((pkt_len < dev_info.min_rx_bufsize) ||
	    (pkt_len > dev_info.max_rx_pktlen)) {
		dev_err(adap, "%s: max pkt len must be > %d and <= %d\n",
			__func__, dev_info.min_rx_bufsize,
			dev_info.max_rx_pktlen);
		return -EINVAL;
	}

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

	/* Set to jumbo mode if necessary */
	if (pkt_len > CXGBE_ETH_MAX_LEN)
		eth_dev->data->dev_conf.rxmode.offloads |=
			DEV_RX_OFFLOAD_JUMBO_FRAME;
	else
		eth_dev->data->dev_conf.rxmode.offloads &=
			~DEV_RX_OFFLOAD_JUMBO_FRAME;

	err = t4_sge_alloc_rxq(adapter, &rxq->rspq, false, eth_dev, msi_idx,
			       &rxq->fl, NULL,
			       is_pf4(adapter) ?
			       t4_get_tp_ch_map(adapter, pi->tx_chan) : 0, mp,
			       queue_idx, socket_id);

	dev_debug(adapter, "%s: err = %d; port_id = %d; cntxt_id = %u; abs_id = %u\n",
		  __func__, err, pi->port_id, rxq->rspq.cntxt_id,
		  rxq->rspq.abs_id);
	return err;
}

void cxgbe_dev_rx_queue_release(void *q)
{
	struct sge_eth_rxq *rxq = (struct sge_eth_rxq *)q;

	if (rxq) {
		struct port_info *pi = (struct port_info *)
				       (rxq->rspq.eth_dev->data->dev_private);
		struct adapter *adap = pi->adapter;

		dev_debug(adapter, "%s: pi->port_id = %d; rx_queue_id = %d\n",
			  __func__, pi->port_id, rxq->rspq.cntxt_id);

		t4_sge_eth_rxq_release(adap, rxq);
	}
}

/*
 * Get port statistics.
 */
static int cxgbe_dev_stats_get(struct rte_eth_dev *eth_dev,
				struct rte_eth_stats *eth_stats)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	struct sge *s = &adapter->sge;
	struct port_stats ps;
	unsigned int i;

	cxgbe_stats_get(pi, &ps);

	/* RX Stats */
	eth_stats->imissed  = ps.rx_ovflow0 + ps.rx_ovflow1 +
			      ps.rx_ovflow2 + ps.rx_ovflow3 +
			      ps.rx_trunc0 + ps.rx_trunc1 +
			      ps.rx_trunc2 + ps.rx_trunc3;
	eth_stats->ierrors  = ps.rx_symbol_err + ps.rx_fcs_err +
			      ps.rx_jabber + ps.rx_too_long + ps.rx_runt +
			      ps.rx_len_err;

	/* TX Stats */
	eth_stats->opackets = ps.tx_frames;
	eth_stats->obytes   = ps.tx_octets;
	eth_stats->oerrors  = ps.tx_error_frames;

	for (i = 0; i < pi->n_rx_qsets; i++) {
		struct sge_eth_rxq *rxq =
			&s->ethrxq[pi->first_rxqset + i];

		eth_stats->q_ipackets[i] = rxq->stats.pkts;
		eth_stats->q_ibytes[i] = rxq->stats.rx_bytes;
		eth_stats->ipackets += eth_stats->q_ipackets[i];
		eth_stats->ibytes += eth_stats->q_ibytes[i];
	}

	for (i = 0; i < pi->n_tx_qsets; i++) {
		struct sge_eth_txq *txq =
			&s->ethtxq[pi->first_txqset + i];

		eth_stats->q_opackets[i] = txq->stats.pkts;
		eth_stats->q_obytes[i] = txq->stats.tx_bytes;
	}
	return 0;
}

/*
 * Reset port statistics.
 */
static int cxgbe_dev_stats_reset(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	struct sge *s = &adapter->sge;
	unsigned int i;

	cxgbe_stats_reset(pi);
	for (i = 0; i < pi->n_rx_qsets; i++) {
		struct sge_eth_rxq *rxq =
			&s->ethrxq[pi->first_rxqset + i];

		rxq->stats.pkts = 0;
		rxq->stats.rx_bytes = 0;
	}
	for (i = 0; i < pi->n_tx_qsets; i++) {
		struct sge_eth_txq *txq =
			&s->ethtxq[pi->first_txqset + i];

		txq->stats.pkts = 0;
		txq->stats.tx_bytes = 0;
		txq->stats.mapping_err = 0;
	}

	return 0;
}

static int cxgbe_flow_ctrl_get(struct rte_eth_dev *eth_dev,
			       struct rte_eth_fc_conf *fc_conf)
{
	struct port_info *pi = eth_dev->data->dev_private;
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
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	struct link_config *lc = &pi->link_cfg;

	if (lc->pcaps & FW_PORT_CAP32_ANEG) {
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

const uint32_t *
cxgbe_dev_supported_ptypes_get(struct rte_eth_dev *eth_dev)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_UNKNOWN
	};

	if (eth_dev->rx_pkt_burst == cxgbe_recv_pkts)
		return ptypes;
	return NULL;
}

/* Update RSS hash configuration
 */
static int cxgbe_dev_rss_hash_update(struct rte_eth_dev *dev,
				     struct rte_eth_rss_conf *rss_conf)
{
	struct port_info *pi = dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	int err;

	err = cxgbe_write_rss_conf(pi, rss_conf->rss_hf);
	if (err)
		return err;

	pi->rss_hf = rss_conf->rss_hf;

	if (rss_conf->rss_key) {
		u32 key[10], mod_key[10];
		int i, j;

		memcpy(key, rss_conf->rss_key, CXGBE_DEFAULT_RSS_KEY_LEN);

		for (i = 9, j = 0; i >= 0; i--, j++)
			mod_key[j] = cpu_to_be32(key[i]);

		t4_write_rss_key(adapter, mod_key, -1);
	}

	return 0;
}

/* Get RSS hash configuration
 */
static int cxgbe_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
				       struct rte_eth_rss_conf *rss_conf)
{
	struct port_info *pi = dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	u64 rss_hf = 0;
	u64 flags = 0;
	int err;

	err = t4_read_config_vi_rss(adapter, adapter->mbox, pi->viid,
				    &flags, NULL);

	if (err)
		return err;

	if (flags & F_FW_RSS_VI_CONFIG_CMD_IP6FOURTUPEN) {
		rss_hf |= CXGBE_RSS_HF_TCP_IPV6_MASK;
		if (flags & F_FW_RSS_VI_CONFIG_CMD_UDPEN)
			rss_hf |= CXGBE_RSS_HF_UDP_IPV6_MASK;
	}

	if (flags & F_FW_RSS_VI_CONFIG_CMD_IP6TWOTUPEN)
		rss_hf |= CXGBE_RSS_HF_IPV6_MASK;

	if (flags & F_FW_RSS_VI_CONFIG_CMD_IP4FOURTUPEN) {
		rss_hf |= ETH_RSS_NONFRAG_IPV4_TCP;
		if (flags & F_FW_RSS_VI_CONFIG_CMD_UDPEN)
			rss_hf |= ETH_RSS_NONFRAG_IPV4_UDP;
	}

	if (flags & F_FW_RSS_VI_CONFIG_CMD_IP4TWOTUPEN)
		rss_hf |= CXGBE_RSS_HF_IPV4_MASK;

	rss_conf->rss_hf = rss_hf;

	if (rss_conf->rss_key) {
		u32 key[10], mod_key[10];
		int i, j;

		t4_read_rss_key(adapter, key);

		for (i = 9, j = 0; i >= 0; i--, j++)
			mod_key[j] = be32_to_cpu(key[i]);

		memcpy(rss_conf->rss_key, mod_key, CXGBE_DEFAULT_RSS_KEY_LEN);
	}

	return 0;
}

static int cxgbe_dev_rss_reta_update(struct rte_eth_dev *dev,
				     struct rte_eth_rss_reta_entry64 *reta_conf,
				     uint16_t reta_size)
{
	struct port_info *pi = dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	u16 i, idx, shift, *rss;
	int ret;

	if (!(adapter->flags & FULL_INIT_DONE))
		return -ENOMEM;

	if (!reta_size || reta_size > pi->rss_size)
		return -EINVAL;

	rss = rte_calloc(NULL, pi->rss_size, sizeof(u16), 0);
	if (!rss)
		return -ENOMEM;

	rte_memcpy(rss, pi->rss, pi->rss_size * sizeof(u16));
	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (!(reta_conf[idx].mask & (1ULL << shift)))
			continue;

		rss[i] = reta_conf[idx].reta[shift];
	}

	ret = cxgbe_write_rss(pi, rss);
	if (!ret)
		rte_memcpy(pi->rss, rss, pi->rss_size * sizeof(u16));

	rte_free(rss);
	return ret;
}

static int cxgbe_dev_rss_reta_query(struct rte_eth_dev *dev,
				    struct rte_eth_rss_reta_entry64 *reta_conf,
				    uint16_t reta_size)
{
	struct port_info *pi = dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	u16 i, idx, shift;

	if (!(adapter->flags & FULL_INIT_DONE))
		return -ENOMEM;

	if (!reta_size || reta_size > pi->rss_size)
		return -EINVAL;

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (!(reta_conf[idx].mask & (1ULL << shift)))
			continue;

		reta_conf[idx].reta[shift] = pi->rss[i];
	}

	return 0;
}

static int cxgbe_get_eeprom_length(struct rte_eth_dev *dev)
{
	RTE_SET_USED(dev);
	return EEPROMSIZE;
}

/**
 * eeprom_ptov - translate a physical EEPROM address to virtual
 * @phys_addr: the physical EEPROM address
 * @fn: the PCI function number
 * @sz: size of function-specific area
 *
 * Translate a physical EEPROM address to virtual.  The first 1K is
 * accessed through virtual addresses starting at 31K, the rest is
 * accessed through virtual addresses starting at 0.
 *
 * The mapping is as follows:
 * [0..1K) -> [31K..32K)
 * [1K..1K+A) -> [31K-A..31K)
 * [1K+A..ES) -> [0..ES-A-1K)
 *
 * where A = @fn * @sz, and ES = EEPROM size.
 */
static int eeprom_ptov(unsigned int phys_addr, unsigned int fn, unsigned int sz)
{
	fn *= sz;
	if (phys_addr < 1024)
		return phys_addr + (31 << 10);
	if (phys_addr < 1024 + fn)
		return fn + phys_addr - 1024;
	if (phys_addr < EEPROMSIZE)
		return phys_addr - 1024 - fn;
	if (phys_addr < EEPROMVSIZE)
		return phys_addr - 1024;
	return -EINVAL;
}

/* The next two routines implement eeprom read/write from physical addresses.
 */
static int eeprom_rd_phys(struct adapter *adap, unsigned int phys_addr, u32 *v)
{
	int vaddr = eeprom_ptov(phys_addr, adap->pf, EEPROMPFSIZE);

	if (vaddr >= 0)
		vaddr = t4_seeprom_read(adap, vaddr, v);
	return vaddr < 0 ? vaddr : 0;
}

static int eeprom_wr_phys(struct adapter *adap, unsigned int phys_addr, u32 v)
{
	int vaddr = eeprom_ptov(phys_addr, adap->pf, EEPROMPFSIZE);

	if (vaddr >= 0)
		vaddr = t4_seeprom_write(adap, vaddr, v);
	return vaddr < 0 ? vaddr : 0;
}

#define EEPROM_MAGIC 0x38E2F10C

static int cxgbe_get_eeprom(struct rte_eth_dev *dev,
			    struct rte_dev_eeprom_info *e)
{
	struct port_info *pi = dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	u32 i, err = 0;
	u8 *buf = rte_zmalloc(NULL, EEPROMSIZE, 0);

	if (!buf)
		return -ENOMEM;

	e->magic = EEPROM_MAGIC;
	for (i = e->offset & ~3; !err && i < e->offset + e->length; i += 4)
		err = eeprom_rd_phys(adapter, i, (u32 *)&buf[i]);

	if (!err)
		rte_memcpy(e->data, buf + e->offset, e->length);
	rte_free(buf);
	return err;
}

static int cxgbe_set_eeprom(struct rte_eth_dev *dev,
			    struct rte_dev_eeprom_info *eeprom)
{
	struct port_info *pi = dev->data->dev_private;
	struct adapter *adapter = pi->adapter;
	u8 *buf;
	int err = 0;
	u32 aligned_offset, aligned_len, *p;

	if (eeprom->magic != EEPROM_MAGIC)
		return -EINVAL;

	aligned_offset = eeprom->offset & ~3;
	aligned_len = (eeprom->length + (eeprom->offset & 3) + 3) & ~3;

	if (adapter->pf > 0) {
		u32 start = 1024 + adapter->pf * EEPROMPFSIZE;

		if (aligned_offset < start ||
		    aligned_offset + aligned_len > start + EEPROMPFSIZE)
			return -EPERM;
	}

	if (aligned_offset != eeprom->offset || aligned_len != eeprom->length) {
		/* RMW possibly needed for first or last words.
		 */
		buf = rte_zmalloc(NULL, aligned_len, 0);
		if (!buf)
			return -ENOMEM;
		err = eeprom_rd_phys(adapter, aligned_offset, (u32 *)buf);
		if (!err && aligned_len > 4)
			err = eeprom_rd_phys(adapter,
					     aligned_offset + aligned_len - 4,
					     (u32 *)&buf[aligned_len - 4]);
		if (err)
			goto out;
		rte_memcpy(buf + (eeprom->offset & 3), eeprom->data,
			   eeprom->length);
	} else {
		buf = eeprom->data;
	}

	err = t4_seeprom_wp(adapter, false);
	if (err)
		goto out;

	for (p = (u32 *)buf; !err && aligned_len; aligned_len -= 4, p++) {
		err = eeprom_wr_phys(adapter, aligned_offset, *p);
		aligned_offset += 4;
	}

	if (!err)
		err = t4_seeprom_wp(adapter, true);
out:
	if (buf != eeprom->data)
		rte_free(buf);
	return err;
}

static int cxgbe_get_regs_len(struct rte_eth_dev *eth_dev)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;

	return t4_get_regs_len(adapter) / sizeof(uint32_t);
}

static int cxgbe_get_regs(struct rte_eth_dev *eth_dev,
			  struct rte_dev_reg_info *regs)
{
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = pi->adapter;

	regs->version = CHELSIO_CHIP_VERSION(adapter->params.chip) |
		(CHELSIO_CHIP_RELEASE(adapter->params.chip) << 10) |
		(1 << 16);

	if (regs->data == NULL) {
		regs->length = cxgbe_get_regs_len(eth_dev);
		regs->width = sizeof(uint32_t);

		return 0;
	}

	t4_get_regs(adapter, regs->data, (regs->length * sizeof(uint32_t)));

	return 0;
}

int cxgbe_mac_addr_set(struct rte_eth_dev *dev, struct rte_ether_addr *addr)
{
	struct port_info *pi = dev->data->dev_private;
	int ret;

	ret = cxgbe_mpstcam_modify(pi, (int)pi->xact_addr_filt, (u8 *)addr);
	if (ret < 0) {
		dev_err(adapter, "failed to set mac addr; err = %d\n",
			ret);
		return ret;
	}
	pi->xact_addr_filt = ret;
	return 0;
}

static const struct eth_dev_ops cxgbe_eth_dev_ops = {
	.dev_start		= cxgbe_dev_start,
	.dev_stop		= cxgbe_dev_stop,
	.dev_close		= cxgbe_dev_close,
	.promiscuous_enable	= cxgbe_dev_promiscuous_enable,
	.promiscuous_disable	= cxgbe_dev_promiscuous_disable,
	.allmulticast_enable	= cxgbe_dev_allmulticast_enable,
	.allmulticast_disable	= cxgbe_dev_allmulticast_disable,
	.dev_configure		= cxgbe_dev_configure,
	.dev_infos_get		= cxgbe_dev_info_get,
	.dev_supported_ptypes_get = cxgbe_dev_supported_ptypes_get,
	.link_update		= cxgbe_dev_link_update,
	.dev_set_link_up        = cxgbe_dev_set_link_up,
	.dev_set_link_down      = cxgbe_dev_set_link_down,
	.mtu_set		= cxgbe_dev_mtu_set,
	.tx_queue_setup         = cxgbe_dev_tx_queue_setup,
	.tx_queue_start		= cxgbe_dev_tx_queue_start,
	.tx_queue_stop		= cxgbe_dev_tx_queue_stop,
	.tx_queue_release	= cxgbe_dev_tx_queue_release,
	.rx_queue_setup         = cxgbe_dev_rx_queue_setup,
	.rx_queue_start		= cxgbe_dev_rx_queue_start,
	.rx_queue_stop		= cxgbe_dev_rx_queue_stop,
	.rx_queue_release	= cxgbe_dev_rx_queue_release,
	.filter_ctrl            = cxgbe_dev_filter_ctrl,
	.stats_get		= cxgbe_dev_stats_get,
	.stats_reset		= cxgbe_dev_stats_reset,
	.flow_ctrl_get		= cxgbe_flow_ctrl_get,
	.flow_ctrl_set		= cxgbe_flow_ctrl_set,
	.get_eeprom_length	= cxgbe_get_eeprom_length,
	.get_eeprom		= cxgbe_get_eeprom,
	.set_eeprom		= cxgbe_set_eeprom,
	.get_reg		= cxgbe_get_regs,
	.rss_hash_update	= cxgbe_dev_rss_hash_update,
	.rss_hash_conf_get	= cxgbe_dev_rss_hash_conf_get,
	.mac_addr_set		= cxgbe_mac_addr_set,
	.reta_update            = cxgbe_dev_rss_reta_update,
	.reta_query             = cxgbe_dev_rss_reta_query,
};

/*
 * Initialize driver
 * It returns 0 on success.
 */
static int eth_cxgbe_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev;
	struct port_info *pi = eth_dev->data->dev_private;
	struct adapter *adapter = NULL;
	char name[RTE_ETH_NAME_MAX_LEN];
	int err = 0;

	CXGBE_FUNC_TRACE();

	eth_dev->dev_ops = &cxgbe_eth_dev_ops;
	eth_dev->rx_pkt_burst = &cxgbe_recv_pkts;
	eth_dev->tx_pkt_burst = &cxgbe_xmit_pkts;
	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* for secondary processes, we attach to ethdevs allocated by primary
	 * and do minimal initialization.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		int i;

		for (i = 1; i < MAX_NPORTS; i++) {
			struct rte_eth_dev *rest_eth_dev;
			char namei[RTE_ETH_NAME_MAX_LEN];

			snprintf(namei, sizeof(namei), "%s_%d",
				 pci_dev->device.name, i);
			rest_eth_dev = rte_eth_dev_attach_secondary(namei);
			if (rest_eth_dev) {
				rest_eth_dev->device = &pci_dev->device;
				rest_eth_dev->dev_ops =
					eth_dev->dev_ops;
				rest_eth_dev->rx_pkt_burst =
					eth_dev->rx_pkt_burst;
				rest_eth_dev->tx_pkt_burst =
					eth_dev->tx_pkt_burst;
				rte_eth_dev_probing_finish(rest_eth_dev);
			}
		}
		return 0;
	}

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

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

	cxgbe_process_devargs(adapter);

	err = cxgbe_probe(adapter);
	if (err) {
		dev_err(adapter, "%s: cxgbe probe failed with err %d\n",
			__func__, err);
		goto out_free_adapter;
	}

	return 0;

out_free_adapter:
	rte_free(adapter);
	return err;
}

static int eth_cxgbe_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	uint16_t port_id;
	int err = 0;

	/* Free up other ports and all resources */
	RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device)
		err |= rte_eth_dev_close(port_id);

	return err == 0 ? 0 : -EIO;
}

static int eth_cxgbe_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct port_info), eth_cxgbe_dev_init);
}

static int eth_cxgbe_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_cxgbe_dev_uninit);
}

static struct rte_pci_driver rte_cxgbe_pmd = {
	.id_table = cxgb4_pci_tbl,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_cxgbe_pci_probe,
	.remove = eth_cxgbe_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_cxgbe, rte_cxgbe_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_cxgbe, cxgb4_pci_tbl);
RTE_PMD_REGISTER_KMOD_DEP(net_cxgbe, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_cxgbe,
			      CXGBE_DEVARG_CMN_KEEP_OVLAN "=<0|1> "
			      CXGBE_DEVARG_CMN_TX_MODE_LATENCY "=<0|1> "
			      CXGBE_DEVARG_PF_FILTER_MODE "=<uint32> "
			      CXGBE_DEVARG_PF_FILTER_MASK "=<uint32> ");
RTE_LOG_REGISTER(cxgbe_logtype, pmd.net.cxgbe, NOTICE);
RTE_LOG_REGISTER(cxgbe_mbox_logtype, pmd.net.cxgbe.mbox, NOTICE);
