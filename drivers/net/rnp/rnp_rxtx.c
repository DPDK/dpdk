/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#include <stdint.h>

#include <rte_ethdev.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>

#include "base/rnp_bdq_if.h"
#include "base/rnp_dma_regs.h"
#include "rnp_rxtx.h"
#include "rnp_logs.h"
#include "rnp.h"

static void rnp_tx_queue_release_mbuf(struct rnp_tx_queue *txq);
static void rnp_tx_queue_sw_reset(struct rnp_tx_queue *txq);
static void rnp_tx_queue_release(void *_txq);

static __rte_always_inline phys_addr_t
rnp_get_dma_addr(struct rnp_queue_attr *attr, struct rte_mbuf *mbuf)
{
	phys_addr_t dma_addr;

	dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova(mbuf));
	if (attr->sriov_st)
		dma_addr |= (attr->sriov_st << 56);

	return dma_addr;
}

static void rnp_rx_queue_release_mbuf(struct rnp_rx_queue *rxq)
{
	uint16_t i;

	if (!rxq)
		return;

	if (rxq->sw_ring) {
		for (i = 0; i < rxq->attr.nb_desc; i++) {
			if (rxq->sw_ring[i].mbuf)
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
		}
		memset(rxq->sw_ring, 0,
				sizeof(rxq->sw_ring[0]) * rxq->attr.nb_desc);
	}
}

static void rnp_rx_queue_release(void *_rxq)
{
	struct rnp_rx_queue *rxq = (struct rnp_rx_queue *)_rxq;

	PMD_INIT_FUNC_TRACE();

	if (rxq) {
		rnp_rx_queue_release_mbuf(rxq);
		rte_memzone_free(rxq->rz);
		rte_free(rxq->sw_ring);
		rte_free(rxq);
	}
}

static int
rnp_tx_queue_reset(struct rnp_eth_port *port,
		   struct rnp_tx_queue *txq)
{
	struct rnp_hw *hw = port->hw;

	rnp_reset_hw_txq_op(hw, txq);

	return 0;
}

static int
rnp_rx_queue_reset(struct rnp_eth_port *port,
		   struct rnp_rx_queue *rxq)
{
	struct rte_eth_dev_data *data = port->eth_dev->data;
	struct rnp_eth_adapter *adapter = port->hw->back;
	struct rte_eth_dev *dev = port->eth_dev;
	struct rnp_rxq_reset_res res = {0};
	uint16_t qidx = rxq->attr.queue_id;
	struct rnp_tx_queue *txq = NULL;
	struct rte_eth_txconf def_conf;
	struct rnp_hw *hw = port->hw;
	struct rte_mbuf *m_mbuf[2];
	bool tx_new = false;
	uint16_t index;
	int err = 0;

	index = rxq->attr.index;
	/* disable eth send pkts to this ring */
	rxq->rx_tail = RNP_E_REG_RD(hw, RNP_RXQ_HEAD(index));
	if (!rxq->rx_tail)
		return 0;
	if (qidx < data->nb_tx_queues && data->tx_queues[qidx]) {
		txq = (struct rnp_tx_queue *)data->tx_queues[qidx];
	} else {
		/* tx queues has been release or txq num less than rxq num */
		def_conf.tx_deferred_start = true;
		def_conf.tx_free_thresh = 32;
		def_conf.tx_rs_thresh = 32;
		if (dev->dev_ops->tx_queue_setup)
			err = dev->dev_ops->tx_queue_setup(dev, qidx,
				rxq->attr.nb_desc,
				dev->data->numa_node, &def_conf);
		if (err) {
			RNP_PMD_ERR("rxq[%u] reset pair txq setup fail", qidx);
			return err;
		}
		txq = port->tx_queues[qidx];
		tx_new = true;
	}
	if (unlikely(rte_mempool_get_bulk(adapter->reset_pool, (void *)m_mbuf,
					2) < 0)) {
		RNP_PMD_LOG(WARNING, "port[%u] reset rx queue[%u] failed "
				"because mbuf alloc failed",
				data->port_id, qidx);
		return -ENOMEM;
	}
	rnp_rxq_flow_disable(hw, index);
	rte_mbuf_refcnt_set(m_mbuf[0], 1);
	rte_mbuf_refcnt_set(m_mbuf[1], 1);
	m_mbuf[0]->data_off = RTE_PKTMBUF_HEADROOM;
	m_mbuf[1]->data_off = RTE_PKTMBUF_HEADROOM;
	res.eth_hdr = rte_pktmbuf_mtod(m_mbuf[0], uint8_t *);
	res.rx_pkt_addr = rnp_get_dma_addr(&rxq->attr, m_mbuf[1]);
	res.tx_pkt_addr = rnp_get_dma_addr(&txq->attr, m_mbuf[0]);
	rnp_reset_hw_rxq_op(hw, rxq, txq, &res);
	if (tx_new)
		rnp_tx_queue_release(txq);
	else
		txq->tx_tail = RNP_E_REG_RD(hw, RNP_TXQ_HEAD(index));
	if (!tx_new) {
		if (txq->tx_tail) {
			rnp_tx_queue_release_mbuf(txq);
			rnp_tx_queue_reset(port, txq);
			rnp_tx_queue_sw_reset(txq);
		}
	}
	rte_mempool_put_bulk(adapter->reset_pool, (void **)m_mbuf, 2);
	rnp_rxq_flow_enable(hw, index);
	rte_io_wmb();
	RNP_E_REG_WR(hw, RNP_RXQ_LEN(index), rxq->attr.nb_desc);

	return 0;
}

static int
rnp_alloc_rxbdr(struct rte_eth_dev *dev,
		struct rnp_rx_queue *rxq,
		uint16_t nb_rx_desc, int socket_id)
{
	const struct rte_memzone *rz = NULL;
	uint32_t size = 0;

	size = (nb_rx_desc + RNP_RX_MAX_BURST_SIZE) *
		sizeof(struct rnp_rxsw_entry);
	rxq->sw_ring = rte_zmalloc_socket("rx_swring", size,
			RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_ring == NULL)
		return -ENOMEM;
	rz = rte_eth_dma_zone_reserve(dev, "rx_ring", rxq->attr.queue_id,
			RNP_RX_MAX_RING_SZ, RNP_BD_RING_ALIGN, socket_id);
	if (rz == NULL) {
		rte_free(rxq->sw_ring);
		rxq->sw_ring = NULL;
		return -ENOMEM;
	}
	memset(rz->addr, 0, RNP_RX_MAX_RING_SZ);
	rxq->rx_bdr = (struct rnp_rx_desc *)rz->addr;
	rxq->ring_phys_addr = rz->iova;
	rxq->rz = rz;

	return 0;
}

static void
rnp_rx_queue_sw_reset(struct rnp_rx_queue *rxq)
{
	uint32_t size = 0;
	uint32_t idx = 0;

	rxq->nb_rx_free = rxq->attr.nb_desc - 1;
	rxq->rx_tail = 0;

	size = rxq->attr.nb_desc + RNP_RX_MAX_BURST_SIZE;
	for (idx = 0; idx < size * sizeof(struct rnp_rx_desc); idx++)
		((volatile char *)rxq->rx_bdr)[idx] = 0;
}


int rnp_rx_queue_setup(struct rte_eth_dev *eth_dev,
		       uint16_t qidx,
		       uint16_t nb_rx_desc,
		       unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mb_pool)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct rnp_hw *hw = port->hw;
	struct rnp_rx_queue *rxq = NULL;
	uint64_t offloads;
	int err = 0;

	RNP_PMD_LOG(INFO, "RXQ[%u] setup nb-desc %u", qidx, nb_rx_desc);
	offloads = rx_conf->offloads | data->dev_conf.rxmode.offloads;
	if (rte_is_power_of_2(nb_rx_desc) == 0) {
		RNP_PMD_ERR("Rxq Desc Num Must power of 2");
		return -EINVAL;
	}
	if (nb_rx_desc > RNP_MAX_BD_COUNT)
		return -EINVAL;
	/* check whether queue has been created if so release it */
	if (qidx < data->nb_rx_queues &&
			data->rx_queues[qidx] != NULL) {
		rnp_rx_queue_release(data->rx_queues[qidx]);
		data->rx_queues[qidx] = NULL;
	}
	rxq = rte_zmalloc_socket("rnp_rxq", sizeof(struct rnp_rx_queue),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL) {
		RNP_PMD_ERR("Failed to allocate rx ring memory");
		return -ENOMEM;
	}
	rxq->rx_offloads = offloads;
	/* queue hw info */
	rxq->attr.index = rnp_get_dma_ring_index(port, qidx);
	rxq->attr.nb_desc_mask = nb_rx_desc - 1;
	rxq->attr.nb_desc = nb_rx_desc;
	rxq->attr.queue_id = qidx;
	/* queue map to port hw info */
	rxq->attr.vf_num = hw->mbx.vf_num;
	rxq->attr.sriov_st = hw->mbx.sriov_st;
	rxq->attr.lane_id = port->attr.nr_lane;
	rxq->attr.port_id = data->port_id;
#define RNP_RXQ_BD_TIMEOUT   (5000000)
	rxq->nodesc_tm_thresh = RNP_RXQ_BD_TIMEOUT;
	rxq->rx_buf_len = (uint16_t)(rte_pktmbuf_data_room_size(mb_pool) -
			RTE_PKTMBUF_HEADROOM);
	rxq->mb_pool = mb_pool;
	err = rnp_alloc_rxbdr(eth_dev, rxq, nb_rx_desc, socket_id);
	if (err)
		goto fail;
	RNP_PMD_LOG(INFO, "PF[%u] dev:[%u] hw-lane[%u] rx_qid[%u] "
			"hw_ridx %u socket %u",
			hw->mbx.pf_num, rxq->attr.port_id,
			rxq->attr.lane_id, qidx,
			rxq->attr.index, socket_id);
	rxq->rx_free_thresh = (rx_conf->rx_free_thresh) ?
		rx_conf->rx_free_thresh : RNP_DEFAULT_RX_FREE_THRESH;
	rxq->pthresh = (rx_conf->rx_thresh.pthresh) ?
		rx_conf->rx_thresh.pthresh : RNP_RX_DESC_FETCH_TH;
	rxq->pburst = (rx_conf->rx_thresh.hthresh) ?
		rx_conf->rx_thresh.hthresh : RNP_RX_DESC_FETCH_BURST;
	rnp_setup_rxbdr(hw, rxq);
	if (rxq->rx_tail) {
		err = rnp_rx_queue_reset(port, rxq);
		if (err) {
			RNP_PMD_ERR("PF[%u] dev:[%u] lane[%u] rx_qid[%u] "
				    "hw_ridx[%u] bdr setup failed",
				    hw->mbx.pf_num, rxq->attr.port_id,
				    rxq->attr.lane_id, qidx, rxq->attr.index);
			goto rxbd_setup_failed;
		}
	}
	rnp_rx_queue_sw_reset(rxq);
	data->rx_queues[qidx] = rxq;

	return 0;
rxbd_setup_failed:
	rte_memzone_free(rxq->rz);
fail:
	rte_free(rxq);

	return err;
}

static void rnp_tx_queue_release_mbuf(struct rnp_tx_queue *txq)
{
	uint16_t i;

	if (!txq)
		return;
	if (txq->sw_ring) {
		for (i = 0; i < txq->attr.nb_desc; i++) {
			if (txq->sw_ring[i].mbuf) {
				rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
				txq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static void rnp_tx_queue_release(void *_txq)
{
	struct rnp_tx_queue *txq = (struct rnp_tx_queue *)_txq;

	PMD_INIT_FUNC_TRACE();

	if (txq) {
		rnp_tx_queue_release_mbuf(txq);

		rte_memzone_free(txq->rz);
		rte_free(txq->sw_ring);
		rte_free(txq);
	}
}

void
rnp_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	rnp_rx_queue_release(dev->data->rx_queues[qid]);
}

void
rnp_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	rnp_tx_queue_release(dev->data->tx_queues[qid]);
}

static int rnp_alloc_txbdr(struct rte_eth_dev *dev,
			   struct rnp_tx_queue *txq,
			   uint16_t nb_desc, int socket_id)
{
	const struct rte_memzone *rz = NULL;
	int size;

	size = nb_desc * sizeof(struct rnp_txsw_entry);
	txq->sw_ring = rte_zmalloc_socket("tx_swq", size,
			RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL)
		return -ENOMEM;

	rz = rte_eth_dma_zone_reserve(dev, "tx_ring", txq->attr.queue_id,
			RNP_TX_MAX_RING_SZ, RNP_BD_RING_ALIGN, socket_id);
	if (rz == NULL) {
		rte_free(txq->sw_ring);
		txq->sw_ring = NULL;
		return -ENOMEM;
	}
	memset(rz->addr, 0, RNP_TX_MAX_RING_SZ);
	txq->ring_phys_addr = rz->iova;
	txq->tx_bdr = rz->addr;
	txq->rz = rz;

	return 0;
}

static void
rnp_tx_queue_sw_reset(struct rnp_tx_queue *txq)
{
	struct rnp_txsw_entry *sw_ring = txq->sw_ring;
	uint32_t idx = 0, prev = 0;
	uint32_t size = 0;

	prev = (uint16_t)(txq->attr.nb_desc - 1);
	for (idx = 0; idx < txq->attr.nb_desc; idx++) {
		sw_ring[idx].mbuf = NULL;
		sw_ring[idx].last_id = idx;
		sw_ring[prev].next_id = idx;
		prev = idx;
	}
	txq->nb_tx_free = txq->attr.nb_desc - 1;
	txq->tx_next_dd = txq->tx_rs_thresh - 1;
	txq->tx_next_rs = txq->tx_rs_thresh - 1;

	size = (txq->attr.nb_desc + RNP_TX_MAX_BURST_SIZE);
	for (idx = 0; idx < size * sizeof(struct rnp_tx_desc); idx++)
		((volatile char *)txq->tx_bdr)[idx] = 0;
}

int
rnp_tx_queue_setup(struct rte_eth_dev *dev,
		   uint16_t qidx, uint16_t nb_desc,
		   unsigned int socket_id,
		   const struct rte_eth_txconf *tx_conf)
{
	struct rnp_eth_port *port = RNP_DEV_TO_PORT(dev);
	struct rte_eth_dev_data *data = dev->data;
	struct rnp_hw *hw = port->hw;
	struct rnp_tx_queue *txq;
	uint64_t offloads = 0;
	int err = 0;

	PMD_INIT_FUNC_TRACE();
	offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;
	RNP_PMD_INFO("TXQ[%u] setup nb-desc %u", qidx, nb_desc);
	if (rte_is_power_of_2(nb_desc) == 0) {
		RNP_PMD_ERR("txq Desc num must power of 2");
		return -EINVAL;
	}
	if (nb_desc > RNP_MAX_BD_COUNT)
		return -EINVAL;
	/* check whether queue Has been create if so release it */
	if (qidx < data->nb_tx_queues && data->tx_queues[qidx]) {
		rnp_tx_queue_release(data->tx_queues[qidx]);
		data->tx_queues[qidx] = NULL;
	}
	txq = rte_zmalloc_socket("rnp_txq", sizeof(struct rnp_tx_queue),
			RTE_CACHE_LINE_SIZE, socket_id);
	if (!txq) {
		RNP_PMD_ERR("Failed to allocate TX ring memory");
		return -ENOMEM;
	}
	txq->tx_rs_thresh = tx_conf->tx_rs_thresh ?
		tx_conf->tx_rs_thresh : RNP_DEFAULT_TX_RS_THRESH;
	txq->tx_free_thresh = tx_conf->tx_free_thresh ?
		tx_conf->tx_free_thresh : RNP_DEFAULT_TX_FREE_THRESH;
	if (txq->tx_rs_thresh > txq->tx_free_thresh) {
		RNP_PMD_ERR("tx_rs_thresh must be less than or "
			     "equal to tx_free_thresh. (tx_free_thresh=%u"
			     " tx_rs_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_conf->tx_free_thresh,
			     (unsigned int)tx_conf->tx_rs_thresh,
			     (int)data->port_id,
			     (int)qidx);
		err = -EINVAL;
		goto txbd_setup_failed;
	}
	if (txq->tx_rs_thresh + txq->tx_free_thresh >= nb_desc) {
		RNP_PMD_ERR("tx_rs_thresh + tx_free_thresh >= nb_desc"
				"%d + %d >= %d", txq->tx_rs_thresh,
				txq->tx_free_thresh, nb_desc);
		err = -EINVAL;
		goto txbd_setup_failed;
	}
	txq->pthresh = (tx_conf->tx_thresh.pthresh) ?
		tx_conf->tx_thresh.pthresh : RNP_TX_DESC_FETCH_TH;
	txq->pburst = (tx_conf->tx_thresh.hthresh) ?
		tx_conf->tx_thresh.hthresh : RNP_TX_DESC_FETCH_BURST;
	txq->free_mbufs = rte_zmalloc_socket("txq->free_mbufs",
			sizeof(struct rte_mbuf *) * txq->tx_rs_thresh,
			RTE_CACHE_LINE_SIZE, socket_id);
	txq->attr.index = rnp_get_dma_ring_index(port, qidx);
	txq->attr.lane_id = port->attr.nr_lane;
	txq->attr.port_id = dev->data->port_id;
	txq->attr.nb_desc_mask = nb_desc - 1;
	txq->attr.vf_num = hw->mbx.vf_num;
	txq->attr.nb_desc = nb_desc;
	txq->attr.queue_id = qidx;

	err = rnp_alloc_txbdr(dev, txq, nb_desc, socket_id);
	if (err)
		goto txbd_setup_failed;
	rnp_setup_txbdr(hw, txq);
	if (txq->tx_tail)
		rnp_reset_hw_txq_op(hw, txq);
	rnp_tx_queue_sw_reset(txq);
	RNP_PMD_LOG(INFO, "PF[%u] dev:[%u] hw-lane[%u] txq queue_id[%u] "
			"dma_idx %u socket %u",
			hw->mbx.pf_num, txq->attr.port_id,
			txq->attr.lane_id, qidx,
			txq->attr.index, socket_id);
	if (qidx < dev->data->nb_tx_queues)
		data->tx_queues[qidx] = txq;
	port->tx_queues[qidx] = txq;

	txq->tx_deferred_start = tx_conf->tx_deferred_start;
	txq->tx_offloads = offloads;

	return 0;
txbd_setup_failed:

	rte_free(txq);

	return err;
}
