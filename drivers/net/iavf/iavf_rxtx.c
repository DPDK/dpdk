/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/queue.h>

#include <rte_string_fns.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_net.h>

#include "iavf.h"
#include "iavf_rxtx.h"

static inline int
check_rx_thresh(uint16_t nb_desc, uint16_t thresh)
{
	/* The following constraints must be satisfied:
	 *   thresh < rxq->nb_rx_desc
	 */
	if (thresh >= nb_desc) {
		PMD_INIT_LOG(ERR, "rx_free_thresh (%u) must be less than %u",
			     thresh, nb_desc);
		return -EINVAL;
	}
	return 0;
}

static inline int
check_tx_thresh(uint16_t nb_desc, uint16_t tx_rs_thresh,
		uint16_t tx_free_thresh)
{
	/* TX descriptors will have their RS bit set after tx_rs_thresh
	 * descriptors have been used. The TX descriptor ring will be cleaned
	 * after tx_free_thresh descriptors are used or if the number of
	 * descriptors required to transmit a packet is greater than the
	 * number of free TX descriptors.
	 *
	 * The following constraints must be satisfied:
	 *  - tx_rs_thresh must be less than the size of the ring minus 2.
	 *  - tx_free_thresh must be less than the size of the ring minus 3.
	 *  - tx_rs_thresh must be less than or equal to tx_free_thresh.
	 *  - tx_rs_thresh must be a divisor of the ring size.
	 *
	 * One descriptor in the TX ring is used as a sentinel to avoid a H/W
	 * race condition, hence the maximum threshold constraints. When set
	 * to zero use default values.
	 */
	if (tx_rs_thresh >= (nb_desc - 2)) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh (%u) must be less than the "
			     "number of TX descriptors (%u) minus 2",
			     tx_rs_thresh, nb_desc);
		return -EINVAL;
	}
	if (tx_free_thresh >= (nb_desc - 3)) {
		PMD_INIT_LOG(ERR, "tx_free_thresh (%u) must be less than the "
			     "number of TX descriptors (%u) minus 3.",
			     tx_free_thresh, nb_desc);
		return -EINVAL;
	}
	if (tx_rs_thresh > tx_free_thresh) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh (%u) must be less than or "
			     "equal to tx_free_thresh (%u).",
			     tx_rs_thresh, tx_free_thresh);
		return -EINVAL;
	}
	if ((nb_desc % tx_rs_thresh) != 0) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh (%u) must be a divisor of the "
			     "number of TX descriptors (%u).",
			     tx_rs_thresh, nb_desc);
		return -EINVAL;
	}

	return 0;
}

static inline bool
check_rx_vec_allow(struct iavf_rx_queue *rxq)
{
	if (rxq->rx_free_thresh >= IAVF_VPMD_RX_MAX_BURST &&
	    rxq->nb_rx_desc % rxq->rx_free_thresh == 0) {
		PMD_INIT_LOG(DEBUG, "Vector Rx can be enabled on this rxq.");
		return true;
	}

	PMD_INIT_LOG(DEBUG, "Vector Rx cannot be enabled on this rxq.");
	return false;
}

static inline bool
check_tx_vec_allow(struct iavf_tx_queue *txq)
{
	if (!(txq->offloads & IAVF_NO_VECTOR_FLAGS) &&
	    txq->rs_thresh >= IAVF_VPMD_TX_MAX_BURST &&
	    txq->rs_thresh <= IAVF_VPMD_TX_MAX_FREE_BUF) {
		PMD_INIT_LOG(DEBUG, "Vector tx can be enabled on this txq.");
		return true;
	}
	PMD_INIT_LOG(DEBUG, "Vector Tx cannot be enabled on this txq.");
	return false;
}

static inline bool
check_rx_bulk_allow(struct iavf_rx_queue *rxq)
{
	int ret = true;

	if (!(rxq->rx_free_thresh >= IAVF_RX_MAX_BURST)) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->rx_free_thresh=%d, "
			     "IAVF_RX_MAX_BURST=%d",
			     rxq->rx_free_thresh, IAVF_RX_MAX_BURST);
		ret = false;
	} else if (rxq->nb_rx_desc % rxq->rx_free_thresh != 0) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->nb_rx_desc=%d, "
			     "rxq->rx_free_thresh=%d",
			     rxq->nb_rx_desc, rxq->rx_free_thresh);
		ret = false;
	}
	return ret;
}

static inline void
reset_rx_queue(struct iavf_rx_queue *rxq)
{
	uint16_t len;
	uint32_t i;

	if (!rxq)
		return;

	len = rxq->nb_rx_desc + IAVF_RX_MAX_BURST;

	for (i = 0; i < len * sizeof(union iavf_rx_desc); i++)
		((volatile char *)rxq->rx_ring)[i] = 0;

	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));

	for (i = 0; i < IAVF_RX_MAX_BURST; i++)
		rxq->sw_ring[rxq->nb_rx_desc + i] = &rxq->fake_mbuf;

	/* for rx bulk */
	rxq->rx_nb_avail = 0;
	rxq->rx_next_avail = 0;
	rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);

	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

static inline void
reset_tx_queue(struct iavf_tx_queue *txq)
{
	struct iavf_tx_entry *txe;
	uint32_t i, size;
	uint16_t prev;

	if (!txq) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq is NULL");
		return;
	}

	txe = txq->sw_ring;
	size = sizeof(struct iavf_tx_desc) * txq->nb_tx_desc;
	for (i = 0; i < size; i++)
		((volatile char *)txq->tx_ring)[i] = 0;

	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		txq->tx_ring[i].cmd_type_offset_bsz =
			rte_cpu_to_le_64(IAVF_TX_DESC_DTYPE_DESC_DONE);
		txe[i].mbuf =  NULL;
		txe[i].last_id = i;
		txe[prev].next_id = i;
		prev = i;
	}

	txq->tx_tail = 0;
	txq->nb_used = 0;

	txq->last_desc_cleaned = txq->nb_tx_desc - 1;
	txq->nb_free = txq->nb_tx_desc - 1;

	txq->next_dd = txq->rs_thresh - 1;
	txq->next_rs = txq->rs_thresh - 1;
}

static int
alloc_rxq_mbufs(struct iavf_rx_queue *rxq)
{
	volatile union iavf_rx_desc *rxd;
	struct rte_mbuf *mbuf = NULL;
	uint64_t dma_addr;
	uint16_t i;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		mbuf = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(!mbuf)) {
			PMD_DRV_LOG(ERR, "Failed to allocate mbuf for RX");
			return -ENOMEM;
		}

		rte_mbuf_refcnt_set(mbuf, 1);
		mbuf->next = NULL;
		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->nb_segs = 1;
		mbuf->port = rxq->port_id;

		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

		rxd = &rxq->rx_ring[i];
		rxd->read.pkt_addr = dma_addr;
		rxd->read.hdr_addr = 0;
#ifndef RTE_LIBRTE_IAVF_16BYTE_RX_DESC
		rxd->read.rsvd1 = 0;
		rxd->read.rsvd2 = 0;
#endif

		rxq->sw_ring[i] = mbuf;
	}

	return 0;
}

static inline void
release_rxq_mbufs(struct iavf_rx_queue *rxq)
{
	uint16_t i;

	if (!rxq->sw_ring)
		return;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		if (rxq->sw_ring[i]) {
			rte_pktmbuf_free_seg(rxq->sw_ring[i]);
			rxq->sw_ring[i] = NULL;
		}
	}

	/* for rx bulk */
	if (rxq->rx_nb_avail == 0)
		return;
	for (i = 0; i < rxq->rx_nb_avail; i++) {
		struct rte_mbuf *mbuf;

		mbuf = rxq->rx_stage[rxq->rx_next_avail + i];
		rte_pktmbuf_free_seg(mbuf);
	}
	rxq->rx_nb_avail = 0;
}

static inline void
release_txq_mbufs(struct iavf_tx_queue *txq)
{
	uint16_t i;

	if (!txq || !txq->sw_ring) {
		PMD_DRV_LOG(DEBUG, "Pointer to rxq or sw_ring is NULL");
		return;
	}

	for (i = 0; i < txq->nb_tx_desc; i++) {
		if (txq->sw_ring[i].mbuf) {
			rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
			txq->sw_ring[i].mbuf = NULL;
		}
	}
}

static const struct iavf_rxq_ops def_rxq_ops = {
	.release_mbufs = release_rxq_mbufs,
};

static const struct iavf_txq_ops def_txq_ops = {
	.release_mbufs = release_txq_mbufs,
};

int
iavf_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		       uint16_t nb_desc, unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mp)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct iavf_adapter *ad =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf =
		IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_vsi *vsi = &vf->vsi;
	struct iavf_rx_queue *rxq;
	const struct rte_memzone *mz;
	uint32_t ring_size;
	uint16_t len;
	uint16_t rx_free_thresh;

	PMD_INIT_FUNC_TRACE();

	if (nb_desc % IAVF_ALIGN_RING_DESC != 0 ||
	    nb_desc > IAVF_MAX_RING_DESC ||
	    nb_desc < IAVF_MIN_RING_DESC) {
		PMD_INIT_LOG(ERR, "Number (%u) of receive descriptors is "
			     "invalid", nb_desc);
		return -EINVAL;
	}

	/* Check free threshold */
	rx_free_thresh = (rx_conf->rx_free_thresh == 0) ?
			 IAVF_DEFAULT_RX_FREE_THRESH :
			 rx_conf->rx_free_thresh;
	if (check_rx_thresh(nb_desc, rx_free_thresh) != 0)
		return -EINVAL;

	/* Free memory if needed */
	if (dev->data->rx_queues[queue_idx]) {
		iavf_dev_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Allocate the rx queue data structure */
	rxq = rte_zmalloc_socket("iavf rxq",
				 sizeof(struct iavf_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (!rxq) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for "
			     "rx queue data structure");
		return -ENOMEM;
	}

	if (vf->vf_res->vf_cap_flags &
	    VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC &&
	    vf->supported_rxdid & BIT(IAVF_RXDID_COMMS_OVS_1)) {
		rxq->rxdid = IAVF_RXDID_COMMS_OVS_1;
	} else {
		rxq->rxdid = IAVF_RXDID_LEGACY_1;
	}

	rxq->mp = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->port_id = dev->data->port_id;
	rxq->crc_len = 0; /* crc stripping by default */
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;
	rxq->rx_hdr_len = 0;
	rxq->vsi = vsi;

	len = rte_pktmbuf_data_room_size(rxq->mp) - RTE_PKTMBUF_HEADROOM;
	rxq->rx_buf_len = RTE_ALIGN(len, (1 << IAVF_RXQ_CTX_DBUFF_SHIFT));

	/* Allocate the software ring. */
	len = nb_desc + IAVF_RX_MAX_BURST;
	rxq->sw_ring =
		rte_zmalloc_socket("iavf rx sw ring",
				   sizeof(struct rte_mbuf *) * len,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (!rxq->sw_ring) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW ring");
		rte_free(rxq);
		return -ENOMEM;
	}

	/* Allocate the maximun number of RX ring hardware descriptor with
	 * a liitle more to support bulk allocate.
	 */
	len = IAVF_MAX_RING_DESC + IAVF_RX_MAX_BURST;
	ring_size = RTE_ALIGN(len * sizeof(union iavf_rx_desc),
			      IAVF_DMA_MEM_ALIGN);
	mz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx,
				      ring_size, IAVF_RING_BASE_ALIGN,
				      socket_id);
	if (!mz) {
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for RX");
		rte_free(rxq->sw_ring);
		rte_free(rxq);
		return -ENOMEM;
	}
	/* Zero all the descriptors in the ring. */
	memset(mz->addr, 0, ring_size);
	rxq->rx_ring_phys_addr = mz->iova;
	rxq->rx_ring = (union iavf_rx_desc *)mz->addr;

	rxq->mz = mz;
	reset_rx_queue(rxq);
	rxq->q_set = true;
	dev->data->rx_queues[queue_idx] = rxq;
	rxq->qrx_tail = hw->hw_addr + IAVF_QRX_TAIL1(rxq->queue_id);
	rxq->ops = &def_rxq_ops;

	if (check_rx_bulk_allow(rxq) == true) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
			     "satisfied. Rx Burst Bulk Alloc function will be "
			     "used on port=%d, queue=%d.",
			     rxq->port_id, rxq->queue_id);
	} else {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
			     "not satisfied, Scattered Rx is requested "
			     "on port=%d, queue=%d.",
			     rxq->port_id, rxq->queue_id);
		ad->rx_bulk_alloc_allowed = false;
	}

	if (check_rx_vec_allow(rxq) == false)
		ad->rx_vec_allowed = false;

	return 0;
}

int
iavf_dev_tx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t queue_idx,
		       uint16_t nb_desc,
		       unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct iavf_tx_queue *txq;
	const struct rte_memzone *mz;
	uint32_t ring_size;
	uint16_t tx_rs_thresh, tx_free_thresh;
	uint64_t offloads;

	PMD_INIT_FUNC_TRACE();

	offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;

	if (nb_desc % IAVF_ALIGN_RING_DESC != 0 ||
	    nb_desc > IAVF_MAX_RING_DESC ||
	    nb_desc < IAVF_MIN_RING_DESC) {
		PMD_INIT_LOG(ERR, "Number (%u) of transmit descriptors is "
			    "invalid", nb_desc);
		return -EINVAL;
	}

	tx_rs_thresh = (uint16_t)((tx_conf->tx_rs_thresh) ?
		tx_conf->tx_rs_thresh : DEFAULT_TX_RS_THRESH);
	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh) ?
		tx_conf->tx_free_thresh : DEFAULT_TX_FREE_THRESH);
	check_tx_thresh(nb_desc, tx_rs_thresh, tx_rs_thresh);

	/* Free memory if needed. */
	if (dev->data->tx_queues[queue_idx]) {
		iavf_dev_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket("iavf txq",
				 sizeof(struct iavf_tx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (!txq) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for "
			     "tx queue structure");
		return -ENOMEM;
	}

	txq->nb_tx_desc = nb_desc;
	txq->rs_thresh = tx_rs_thresh;
	txq->free_thresh = tx_free_thresh;
	txq->queue_id = queue_idx;
	txq->port_id = dev->data->port_id;
	txq->offloads = offloads;
	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	/* Allocate software ring */
	txq->sw_ring =
		rte_zmalloc_socket("iavf tx sw ring",
				   sizeof(struct iavf_tx_entry) * nb_desc,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (!txq->sw_ring) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW TX ring");
		rte_free(txq);
		return -ENOMEM;
	}

	/* Allocate TX hardware ring descriptors. */
	ring_size = sizeof(struct iavf_tx_desc) * IAVF_MAX_RING_DESC;
	ring_size = RTE_ALIGN(ring_size, IAVF_DMA_MEM_ALIGN);
	mz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx,
				      ring_size, IAVF_RING_BASE_ALIGN,
				      socket_id);
	if (!mz) {
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for TX");
		rte_free(txq->sw_ring);
		rte_free(txq);
		return -ENOMEM;
	}
	txq->tx_ring_phys_addr = mz->iova;
	txq->tx_ring = (struct iavf_tx_desc *)mz->addr;

	txq->mz = mz;
	reset_tx_queue(txq);
	txq->q_set = true;
	dev->data->tx_queues[queue_idx] = txq;
	txq->qtx_tail = hw->hw_addr + IAVF_QTX_TAIL1(queue_idx);
	txq->ops = &def_txq_ops;

	if (check_tx_vec_allow(txq) == false) {
		struct iavf_adapter *ad =
			IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
		ad->tx_vec_allowed = false;
	}

	return 0;
}

int
iavf_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct iavf_rx_queue *rxq;
	int err = 0;

	PMD_DRV_FUNC_TRACE();

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	rxq = dev->data->rx_queues[rx_queue_id];

	err = alloc_rxq_mbufs(rxq);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to allocate RX queue mbuf");
		return err;
	}

	rte_wmb();

	/* Init the RX tail register. */
	IAVF_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);
	IAVF_WRITE_FLUSH(hw);

	/* Ready to switch the queue on */
	err = iavf_switch_queue(adapter, rx_queue_id, true, true);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u on",
			    rx_queue_id);
	else
		dev->data->rx_queue_state[rx_queue_id] =
			RTE_ETH_QUEUE_STATE_STARTED;

	return err;
}

int
iavf_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct iavf_tx_queue *txq;
	int err = 0;

	PMD_DRV_FUNC_TRACE();

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	txq = dev->data->tx_queues[tx_queue_id];

	/* Init the RX tail register. */
	IAVF_PCI_REG_WRITE(txq->qtx_tail, 0);
	IAVF_WRITE_FLUSH(hw);

	/* Ready to switch the queue on */
	err = iavf_switch_queue(adapter, tx_queue_id, false, true);

	if (err)
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u on",
			    tx_queue_id);
	else
		dev->data->tx_queue_state[tx_queue_id] =
			RTE_ETH_QUEUE_STATE_STARTED;

	return err;
}

int
iavf_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_rx_queue *rxq;
	int err;

	PMD_DRV_FUNC_TRACE();

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	err = iavf_switch_queue(adapter, rx_queue_id, true, false);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u off",
			    rx_queue_id);
		return err;
	}

	rxq = dev->data->rx_queues[rx_queue_id];
	rxq->ops->release_mbufs(rxq);
	reset_rx_queue(rxq);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

int
iavf_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_tx_queue *txq;
	int err;

	PMD_DRV_FUNC_TRACE();

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	err = iavf_switch_queue(adapter, tx_queue_id, false, false);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u off",
			    tx_queue_id);
		return err;
	}

	txq = dev->data->tx_queues[tx_queue_id];
	txq->ops->release_mbufs(txq);
	reset_tx_queue(txq);
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

void
iavf_dev_rx_queue_release(void *rxq)
{
	struct iavf_rx_queue *q = (struct iavf_rx_queue *)rxq;

	if (!q)
		return;

	q->ops->release_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_free(q);
}

void
iavf_dev_tx_queue_release(void *txq)
{
	struct iavf_tx_queue *q = (struct iavf_tx_queue *)txq;

	if (!q)
		return;

	q->ops->release_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_free(q);
}

void
iavf_stop_queues(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_rx_queue *rxq;
	struct iavf_tx_queue *txq;
	int ret, i;

	/* Stop All queues */
	ret = iavf_disable_queues(adapter);
	if (ret)
		PMD_DRV_LOG(WARNING, "Fail to stop queues");

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (!txq)
			continue;
		txq->ops->release_mbufs(txq);
		reset_tx_queue(txq);
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (!rxq)
			continue;
		rxq->ops->release_mbufs(rxq);
		reset_rx_queue(rxq);
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
}

static inline void
iavf_rxd_to_vlan_tci(struct rte_mbuf *mb, volatile union iavf_rx_desc *rxdp)
{
	if (rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len) &
		(1 << IAVF_RX_DESC_STATUS_L2TAG1P_SHIFT)) {
		mb->ol_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
		mb->vlan_tci =
			rte_le_to_cpu_16(rxdp->wb.qword0.lo_dword.l2tag1);
	} else {
		mb->vlan_tci = 0;
	}
}

static inline void
iavf_flex_rxd_to_vlan_tci(struct rte_mbuf *mb,
			  volatile union iavf_rx_flex_desc *rxdp)
{
	if (rte_le_to_cpu_64(rxdp->wb.status_error0) &
		(1 << IAVF_RX_FLEX_DESC_STATUS0_L2TAG1P_S)) {
		mb->ol_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
		mb->vlan_tci =
			rte_le_to_cpu_16(rxdp->wb.l2tag1);
	} else {
		mb->vlan_tci = 0;
	}
}

/* Translate the rx descriptor status and error fields to pkt flags */
static inline uint64_t
iavf_rxd_to_pkt_flags(uint64_t qword)
{
	uint64_t flags;
	uint64_t error_bits = (qword >> IAVF_RXD_QW1_ERROR_SHIFT);

#define IAVF_RX_ERR_BITS 0x3f

	/* Check if RSS_HASH */
	flags = (((qword >> IAVF_RX_DESC_STATUS_FLTSTAT_SHIFT) &
					IAVF_RX_DESC_FLTSTAT_RSS_HASH) ==
			IAVF_RX_DESC_FLTSTAT_RSS_HASH) ? PKT_RX_RSS_HASH : 0;

	/* Check if FDIR Match */
	flags |= (qword & (1 << IAVF_RX_DESC_STATUS_FLM_SHIFT) ?
				PKT_RX_FDIR : 0);

	if (likely((error_bits & IAVF_RX_ERR_BITS) == 0)) {
		flags |= (PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD);
		return flags;
	}

	if (unlikely(error_bits & (1 << IAVF_RX_DESC_ERROR_IPE_SHIFT)))
		flags |= PKT_RX_IP_CKSUM_BAD;
	else
		flags |= PKT_RX_IP_CKSUM_GOOD;

	if (unlikely(error_bits & (1 << IAVF_RX_DESC_ERROR_L4E_SHIFT)))
		flags |= PKT_RX_L4_CKSUM_BAD;
	else
		flags |= PKT_RX_L4_CKSUM_GOOD;

	/* TODO: Oversize error bit is not processed here */

	return flags;
}

static inline uint64_t
iavf_rxd_build_fdir(volatile union iavf_rx_desc *rxdp, struct rte_mbuf *mb)
{
	uint64_t flags = 0;
#ifndef RTE_LIBRTE_IAVF_16BYTE_RX_DESC
	uint16_t flexbh;

	flexbh = (rte_le_to_cpu_32(rxdp->wb.qword2.ext_status) >>
		IAVF_RX_DESC_EXT_STATUS_FLEXBH_SHIFT) &
		IAVF_RX_DESC_EXT_STATUS_FLEXBH_MASK;

	if (flexbh == IAVF_RX_DESC_EXT_STATUS_FLEXBH_FD_ID) {
		mb->hash.fdir.hi =
			rte_le_to_cpu_32(rxdp->wb.qword3.hi_dword.fd_id);
		flags |= PKT_RX_FDIR_ID;
	}
#else
	mb->hash.fdir.hi =
		rte_le_to_cpu_32(rxdp->wb.qword0.hi_dword.fd_id);
	flags |= PKT_RX_FDIR_ID;
#endif
	return flags;
}


/* Translate the rx flex descriptor status to pkt flags */
static inline void
iavf_rxd_to_pkt_fields(struct rte_mbuf *mb,
		       volatile union iavf_rx_flex_desc *rxdp)
{
	volatile struct iavf_32b_rx_flex_desc_comms_ovs *desc =
			(volatile struct iavf_32b_rx_flex_desc_comms_ovs *)rxdp;
#ifndef RTE_LIBRTE_IAVF_16BYTE_RX_DESC
	uint16_t stat_err;

	stat_err = rte_le_to_cpu_16(desc->status_error0);
	if (likely(stat_err & (1 << IAVF_RX_FLEX_DESC_STATUS0_RSS_VALID_S))) {
		mb->ol_flags |= PKT_RX_RSS_HASH;
		mb->hash.rss = rte_le_to_cpu_32(desc->rss_hash);
	}
#endif

	if (desc->flow_id != 0xFFFFFFFF) {
		mb->ol_flags |= PKT_RX_FDIR | PKT_RX_FDIR_ID;
		mb->hash.fdir.hi = rte_le_to_cpu_32(desc->flow_id);
	}
}

#define IAVF_RX_FLEX_ERR0_BITS	\
	((1 << IAVF_RX_FLEX_DESC_STATUS0_HBO_S) |	\
	 (1 << IAVF_RX_FLEX_DESC_STATUS0_XSUM_IPE_S) |	\
	 (1 << IAVF_RX_FLEX_DESC_STATUS0_XSUM_L4E_S) |	\
	 (1 << IAVF_RX_FLEX_DESC_STATUS0_XSUM_EIPE_S) |	\
	 (1 << IAVF_RX_FLEX_DESC_STATUS0_XSUM_EUDPE_S) |	\
	 (1 << IAVF_RX_FLEX_DESC_STATUS0_RXE_S))

/* Rx L3/L4 checksum */
static inline uint64_t
iavf_flex_rxd_error_to_pkt_flags(uint16_t stat_err0)
{
	uint64_t flags = 0;

	/* check if HW has decoded the packet and checksum */
	if (unlikely(!(stat_err0 & (1 << IAVF_RX_FLEX_DESC_STATUS0_L3L4P_S))))
		return 0;

	if (likely(!(stat_err0 & IAVF_RX_FLEX_ERR0_BITS))) {
		flags |= (PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD);
		return flags;
	}

	if (unlikely(stat_err0 & (1 << IAVF_RX_FLEX_DESC_STATUS0_XSUM_IPE_S)))
		flags |= PKT_RX_IP_CKSUM_BAD;
	else
		flags |= PKT_RX_IP_CKSUM_GOOD;

	if (unlikely(stat_err0 & (1 << IAVF_RX_FLEX_DESC_STATUS0_XSUM_L4E_S)))
		flags |= PKT_RX_L4_CKSUM_BAD;
	else
		flags |= PKT_RX_L4_CKSUM_GOOD;

	if (unlikely(stat_err0 & (1 << IAVF_RX_FLEX_DESC_STATUS0_XSUM_EIPE_S)))
		flags |= PKT_RX_EIP_CKSUM_BAD;

	return flags;
}

/* If the number of free RX descriptors is greater than the RX free
 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
 * register. Update the RDT with the value of the last processed RX
 * descriptor minus 1, to guarantee that the RDT register is never
 * equal to the RDH register, which creates a "full" ring situation
 * from the hardware point of view.
 */
static inline void
iavf_update_rx_tail(struct iavf_rx_queue *rxq, uint16_t nb_hold, uint16_t rx_id)
{
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);

	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG,
			   "port_id=%u queue_id=%u rx_tail=%u nb_hold=%u",
			   rxq->port_id, rxq->queue_id, rx_id, nb_hold);
		rx_id = (uint16_t)((rx_id == 0) ?
			(rxq->nb_rx_desc - 1) : (rx_id - 1));
		IAVF_PCI_REG_WRITE(rxq->qrx_tail, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;
}

/* implement recv_pkts */
uint16_t
iavf_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	volatile union iavf_rx_desc *rx_ring;
	volatile union iavf_rx_desc *rxdp;
	struct iavf_rx_queue *rxq;
	union iavf_rx_desc rxd;
	struct rte_mbuf *rxe;
	struct rte_eth_dev *dev;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	uint16_t nb_rx;
	uint32_t rx_status;
	uint64_t qword1;
	uint16_t rx_packet_len;
	uint16_t rx_id, nb_hold;
	uint64_t dma_addr;
	uint64_t pkt_flags;
	const uint32_t *ptype_tbl;

	nb_rx = 0;
	nb_hold = 0;
	rxq = rx_queue;
	rx_id = rxq->rx_tail;
	rx_ring = rxq->rx_ring;
	ptype_tbl = rxq->vsi->adapter->ptype_tbl;

	while (nb_rx < nb_pkts) {
		rxdp = &rx_ring[rx_id];
		qword1 = rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len);
		rx_status = (qword1 & IAVF_RXD_QW1_STATUS_MASK) >>
			    IAVF_RXD_QW1_STATUS_SHIFT;

		/* Check the DD bit first */
		if (!(rx_status & (1 << IAVF_RX_DESC_STATUS_DD_SHIFT)))
			break;
		IAVF_DUMP_RX_DESC(rxq, rxdp, rx_id);

		nmb = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(!nmb)) {
			dev = &rte_eth_devices[rxq->port_id];
			dev->data->rx_mbuf_alloc_failed++;
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u", rxq->port_id, rxq->queue_id);
			break;
		}

		rxd = *rxdp;
		nb_hold++;
		rxe = rxq->sw_ring[rx_id];
		rx_id++;
		if (unlikely(rx_id == rxq->nb_rx_desc))
			rx_id = 0;

		/* Prefetch next mbuf */
		rte_prefetch0(rxq->sw_ring[rx_id]);

		/* When next RX descriptor is on a cache line boundary,
		 * prefetch the next 4 RX descriptors and next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_prefetch0(&rx_ring[rx_id]);
			rte_prefetch0(rxq->sw_ring[rx_id]);
		}
		rxm = rxe;
		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		rxdp->read.hdr_addr = 0;
		rxdp->read.pkt_addr = dma_addr;

		rx_packet_len = ((qword1 & IAVF_RXD_QW1_LENGTH_PBUF_MASK) >>
				IAVF_RXD_QW1_LENGTH_PBUF_SHIFT) - rxq->crc_len;

		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rte_prefetch0(RTE_PTR_ADD(rxm->buf_addr, RTE_PKTMBUF_HEADROOM));
		rxm->nb_segs = 1;
		rxm->next = NULL;
		rxm->pkt_len = rx_packet_len;
		rxm->data_len = rx_packet_len;
		rxm->port = rxq->port_id;
		rxm->ol_flags = 0;
		iavf_rxd_to_vlan_tci(rxm, &rxd);
		pkt_flags = iavf_rxd_to_pkt_flags(qword1);
		rxm->packet_type =
			ptype_tbl[(uint8_t)((qword1 &
			IAVF_RXD_QW1_PTYPE_MASK) >> IAVF_RXD_QW1_PTYPE_SHIFT)];

		if (pkt_flags & PKT_RX_RSS_HASH)
			rxm->hash.rss =
				rte_le_to_cpu_32(rxd.wb.qword0.hi_dword.rss);

		if (pkt_flags & PKT_RX_FDIR)
			pkt_flags |= iavf_rxd_build_fdir(&rxd, rxm);

		rxm->ol_flags |= pkt_flags;

		rx_pkts[nb_rx++] = rxm;
	}
	rxq->rx_tail = rx_id;

	iavf_update_rx_tail(rxq, nb_hold, rx_id);

	return nb_rx;
}

/* implement recv_pkts for flexible Rx descriptor */
uint16_t
iavf_recv_pkts_flex_rxd(void *rx_queue,
			struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	volatile union iavf_rx_desc *rx_ring;
	volatile union iavf_rx_flex_desc *rxdp;
	struct iavf_rx_queue *rxq;
	union iavf_rx_flex_desc rxd;
	struct rte_mbuf *rxe;
	struct rte_eth_dev *dev;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	uint16_t nb_rx;
	uint16_t rx_stat_err0;
	uint16_t rx_packet_len;
	uint16_t rx_id, nb_hold;
	uint64_t dma_addr;
	uint64_t pkt_flags;
	const uint32_t *ptype_tbl;

	nb_rx = 0;
	nb_hold = 0;
	rxq = rx_queue;
	rx_id = rxq->rx_tail;
	rx_ring = rxq->rx_ring;
	ptype_tbl = rxq->vsi->adapter->ptype_tbl;

	while (nb_rx < nb_pkts) {
		rxdp = (volatile union iavf_rx_flex_desc *)&rx_ring[rx_id];
		rx_stat_err0 = rte_le_to_cpu_16(rxdp->wb.status_error0);

		/* Check the DD bit first */
		if (!(rx_stat_err0 & (1 << IAVF_RX_FLEX_DESC_STATUS0_DD_S)))
			break;
		IAVF_DUMP_RX_DESC(rxq, rxdp, rx_id);

		nmb = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(!nmb)) {
			dev = &rte_eth_devices[rxq->port_id];
			dev->data->rx_mbuf_alloc_failed++;
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u", rxq->port_id, rxq->queue_id);
			break;
		}

		rxd = *rxdp;
		nb_hold++;
		rxe = rxq->sw_ring[rx_id];
		rx_id++;
		if (unlikely(rx_id == rxq->nb_rx_desc))
			rx_id = 0;

		/* Prefetch next mbuf */
		rte_prefetch0(rxq->sw_ring[rx_id]);

		/* When next RX descriptor is on a cache line boundary,
		 * prefetch the next 4 RX descriptors and next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_prefetch0(&rx_ring[rx_id]);
			rte_prefetch0(rxq->sw_ring[rx_id]);
		}
		rxm = rxe;
		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		rxdp->read.hdr_addr = 0;
		rxdp->read.pkt_addr = dma_addr;

		rx_packet_len = (rte_le_to_cpu_16(rxd.wb.pkt_len) &
				IAVF_RX_FLX_DESC_PKT_LEN_M) - rxq->crc_len;

		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rte_prefetch0(RTE_PTR_ADD(rxm->buf_addr, RTE_PKTMBUF_HEADROOM));
		rxm->nb_segs = 1;
		rxm->next = NULL;
		rxm->pkt_len = rx_packet_len;
		rxm->data_len = rx_packet_len;
		rxm->port = rxq->port_id;
		rxm->ol_flags = 0;
		rxm->packet_type = ptype_tbl[IAVF_RX_FLEX_DESC_PTYPE_M &
			rte_le_to_cpu_16(rxd.wb.ptype_flex_flags0)];
		iavf_flex_rxd_to_vlan_tci(rxm, &rxd);
		iavf_rxd_to_pkt_fields(rxm, &rxd);
		pkt_flags = iavf_flex_rxd_error_to_pkt_flags(rx_stat_err0);
		rxm->ol_flags |= pkt_flags;

		rx_pkts[nb_rx++] = rxm;
	}
	rxq->rx_tail = rx_id;

	iavf_update_rx_tail(rxq, nb_hold, rx_id);

	return nb_rx;
}

/* implement recv_scattered_pkts for flexible Rx descriptor */
uint16_t
iavf_recv_scattered_pkts_flex_rxd(void *rx_queue, struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts)
{
	struct iavf_rx_queue *rxq = rx_queue;
	union iavf_rx_flex_desc rxd;
	struct rte_mbuf *rxe;
	struct rte_mbuf *first_seg = rxq->pkt_first_seg;
	struct rte_mbuf *last_seg = rxq->pkt_last_seg;
	struct rte_mbuf *nmb, *rxm;
	uint16_t rx_id = rxq->rx_tail;
	uint16_t nb_rx = 0, nb_hold = 0, rx_packet_len;
	struct rte_eth_dev *dev;
	uint16_t rx_stat_err0;
	uint64_t dma_addr;
	uint64_t pkt_flags;

	volatile union iavf_rx_desc *rx_ring = rxq->rx_ring;
	volatile union iavf_rx_flex_desc *rxdp;
	const uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;

	while (nb_rx < nb_pkts) {
		rxdp = (volatile union iavf_rx_flex_desc *)&rx_ring[rx_id];
		rx_stat_err0 = rte_le_to_cpu_16(rxdp->wb.status_error0);

		/* Check the DD bit */
		if (!(rx_stat_err0 & (1 << IAVF_RX_FLEX_DESC_STATUS0_DD_S)))
			break;
		IAVF_DUMP_RX_DESC(rxq, rxdp, rx_id);

		nmb = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(!nmb)) {
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u", rxq->port_id, rxq->queue_id);
			dev = &rte_eth_devices[rxq->port_id];
			dev->data->rx_mbuf_alloc_failed++;
			break;
		}

		rxd = *rxdp;
		nb_hold++;
		rxe = rxq->sw_ring[rx_id];
		rx_id++;
		if (rx_id == rxq->nb_rx_desc)
			rx_id = 0;

		/* Prefetch next mbuf */
		rte_prefetch0(rxq->sw_ring[rx_id]);

		/* When next RX descriptor is on a cache line boundary,
		 * prefetch the next 4 RX descriptors and next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_prefetch0(&rx_ring[rx_id]);
			rte_prefetch0(rxq->sw_ring[rx_id]);
		}

		rxm = rxe;
		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));

		/* Set data buffer address and data length of the mbuf */
		rxdp->read.hdr_addr = 0;
		rxdp->read.pkt_addr = dma_addr;
		rx_packet_len = rte_le_to_cpu_16(rxd.wb.pkt_len) &
				IAVF_RX_FLX_DESC_PKT_LEN_M;
		rxm->data_len = rx_packet_len;
		rxm->data_off = RTE_PKTMBUF_HEADROOM;

		/* If this is the first buffer of the received packet, set the
		 * pointer to the first mbuf of the packet and initialize its
		 * context. Otherwise, update the total length and the number
		 * of segments of the current scattered packet, and update the
		 * pointer to the last mbuf of the current packet.
		 */
		if (!first_seg) {
			first_seg = rxm;
			first_seg->nb_segs = 1;
			first_seg->pkt_len = rx_packet_len;
		} else {
			first_seg->pkt_len =
				(uint16_t)(first_seg->pkt_len +
						rx_packet_len);
			first_seg->nb_segs++;
			last_seg->next = rxm;
		}

		/* If this is not the last buffer of the received packet,
		 * update the pointer to the last mbuf of the current scattered
		 * packet and continue to parse the RX ring.
		 */
		if (!(rx_stat_err0 & (1 << IAVF_RX_FLEX_DESC_STATUS0_EOF_S))) {
			last_seg = rxm;
			continue;
		}

		/* This is the last buffer of the received packet. If the CRC
		 * is not stripped by the hardware:
		 *  - Subtract the CRC length from the total packet length.
		 *  - If the last buffer only contains the whole CRC or a part
		 *  of it, free the mbuf associated to the last buffer. If part
		 *  of the CRC is also contained in the previous mbuf, subtract
		 *  the length of that CRC part from the data length of the
		 *  previous mbuf.
		 */
		rxm->next = NULL;
		if (unlikely(rxq->crc_len > 0)) {
			first_seg->pkt_len -= RTE_ETHER_CRC_LEN;
			if (rx_packet_len <= RTE_ETHER_CRC_LEN) {
				rte_pktmbuf_free_seg(rxm);
				first_seg->nb_segs--;
				last_seg->data_len =
					(uint16_t)(last_seg->data_len -
					(RTE_ETHER_CRC_LEN - rx_packet_len));
				last_seg->next = NULL;
			} else {
				rxm->data_len = (uint16_t)(rx_packet_len -
							RTE_ETHER_CRC_LEN);
			}
		}

		first_seg->port = rxq->port_id;
		first_seg->ol_flags = 0;
		first_seg->packet_type = ptype_tbl[IAVF_RX_FLEX_DESC_PTYPE_M &
			rte_le_to_cpu_16(rxd.wb.ptype_flex_flags0)];
		iavf_flex_rxd_to_vlan_tci(first_seg, &rxd);
		iavf_rxd_to_pkt_fields(first_seg, &rxd);
		pkt_flags = iavf_flex_rxd_error_to_pkt_flags(rx_stat_err0);

		first_seg->ol_flags |= pkt_flags;

		/* Prefetch data of first segment, if configured to do so. */
		rte_prefetch0(RTE_PTR_ADD(first_seg->buf_addr,
					  first_seg->data_off));
		rx_pkts[nb_rx++] = first_seg;
		first_seg = NULL;
	}

	/* Record index of the next RX descriptor to probe. */
	rxq->rx_tail = rx_id;
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg = last_seg;

	iavf_update_rx_tail(rxq, nb_hold, rx_id);

	return nb_rx;
}

/* implement recv_scattered_pkts  */
uint16_t
iavf_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	struct iavf_rx_queue *rxq = rx_queue;
	union iavf_rx_desc rxd;
	struct rte_mbuf *rxe;
	struct rte_mbuf *first_seg = rxq->pkt_first_seg;
	struct rte_mbuf *last_seg = rxq->pkt_last_seg;
	struct rte_mbuf *nmb, *rxm;
	uint16_t rx_id = rxq->rx_tail;
	uint16_t nb_rx = 0, nb_hold = 0, rx_packet_len;
	struct rte_eth_dev *dev;
	uint32_t rx_status;
	uint64_t qword1;
	uint64_t dma_addr;
	uint64_t pkt_flags;

	volatile union iavf_rx_desc *rx_ring = rxq->rx_ring;
	volatile union iavf_rx_desc *rxdp;
	const uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;

	while (nb_rx < nb_pkts) {
		rxdp = &rx_ring[rx_id];
		qword1 = rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len);
		rx_status = (qword1 & IAVF_RXD_QW1_STATUS_MASK) >>
			    IAVF_RXD_QW1_STATUS_SHIFT;

		/* Check the DD bit */
		if (!(rx_status & (1 << IAVF_RX_DESC_STATUS_DD_SHIFT)))
			break;
		IAVF_DUMP_RX_DESC(rxq, rxdp, rx_id);

		nmb = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(!nmb)) {
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u", rxq->port_id, rxq->queue_id);
			dev = &rte_eth_devices[rxq->port_id];
			dev->data->rx_mbuf_alloc_failed++;
			break;
		}

		rxd = *rxdp;
		nb_hold++;
		rxe = rxq->sw_ring[rx_id];
		rx_id++;
		if (rx_id == rxq->nb_rx_desc)
			rx_id = 0;

		/* Prefetch next mbuf */
		rte_prefetch0(rxq->sw_ring[rx_id]);

		/* When next RX descriptor is on a cache line boundary,
		 * prefetch the next 4 RX descriptors and next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_prefetch0(&rx_ring[rx_id]);
			rte_prefetch0(rxq->sw_ring[rx_id]);
		}

		rxm = rxe;
		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));

		/* Set data buffer address and data length of the mbuf */
		rxdp->read.hdr_addr = 0;
		rxdp->read.pkt_addr = dma_addr;
		rx_packet_len = (qword1 & IAVF_RXD_QW1_LENGTH_PBUF_MASK) >>
				 IAVF_RXD_QW1_LENGTH_PBUF_SHIFT;
		rxm->data_len = rx_packet_len;
		rxm->data_off = RTE_PKTMBUF_HEADROOM;

		/* If this is the first buffer of the received packet, set the
		 * pointer to the first mbuf of the packet and initialize its
		 * context. Otherwise, update the total length and the number
		 * of segments of the current scattered packet, and update the
		 * pointer to the last mbuf of the current packet.
		 */
		if (!first_seg) {
			first_seg = rxm;
			first_seg->nb_segs = 1;
			first_seg->pkt_len = rx_packet_len;
		} else {
			first_seg->pkt_len =
				(uint16_t)(first_seg->pkt_len +
						rx_packet_len);
			first_seg->nb_segs++;
			last_seg->next = rxm;
		}

		/* If this is not the last buffer of the received packet,
		 * update the pointer to the last mbuf of the current scattered
		 * packet and continue to parse the RX ring.
		 */
		if (!(rx_status & (1 << IAVF_RX_DESC_STATUS_EOF_SHIFT))) {
			last_seg = rxm;
			continue;
		}

		/* This is the last buffer of the received packet. If the CRC
		 * is not stripped by the hardware:
		 *  - Subtract the CRC length from the total packet length.
		 *  - If the last buffer only contains the whole CRC or a part
		 *  of it, free the mbuf associated to the last buffer. If part
		 *  of the CRC is also contained in the previous mbuf, subtract
		 *  the length of that CRC part from the data length of the
		 *  previous mbuf.
		 */
		rxm->next = NULL;
		if (unlikely(rxq->crc_len > 0)) {
			first_seg->pkt_len -= RTE_ETHER_CRC_LEN;
			if (rx_packet_len <= RTE_ETHER_CRC_LEN) {
				rte_pktmbuf_free_seg(rxm);
				first_seg->nb_segs--;
				last_seg->data_len =
					(uint16_t)(last_seg->data_len -
					(RTE_ETHER_CRC_LEN - rx_packet_len));
				last_seg->next = NULL;
			} else
				rxm->data_len = (uint16_t)(rx_packet_len -
							RTE_ETHER_CRC_LEN);
		}

		first_seg->port = rxq->port_id;
		first_seg->ol_flags = 0;
		iavf_rxd_to_vlan_tci(first_seg, &rxd);
		pkt_flags = iavf_rxd_to_pkt_flags(qword1);
		first_seg->packet_type =
			ptype_tbl[(uint8_t)((qword1 &
			IAVF_RXD_QW1_PTYPE_MASK) >> IAVF_RXD_QW1_PTYPE_SHIFT)];

		if (pkt_flags & PKT_RX_RSS_HASH)
			first_seg->hash.rss =
				rte_le_to_cpu_32(rxd.wb.qword0.hi_dword.rss);

		if (pkt_flags & PKT_RX_FDIR)
			pkt_flags |= iavf_rxd_build_fdir(&rxd, first_seg);

		first_seg->ol_flags |= pkt_flags;

		/* Prefetch data of first segment, if configured to do so. */
		rte_prefetch0(RTE_PTR_ADD(first_seg->buf_addr,
					  first_seg->data_off));
		rx_pkts[nb_rx++] = first_seg;
		first_seg = NULL;
	}

	/* Record index of the next RX descriptor to probe. */
	rxq->rx_tail = rx_id;
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg = last_seg;

	iavf_update_rx_tail(rxq, nb_hold, rx_id);

	return nb_rx;
}

#define IAVF_LOOK_AHEAD 8
static inline int
iavf_rx_scan_hw_ring_flex_rxd(struct iavf_rx_queue *rxq)
{
	volatile union iavf_rx_flex_desc *rxdp;
	struct rte_mbuf **rxep;
	struct rte_mbuf *mb;
	uint16_t stat_err0;
	uint16_t pkt_len;
	int32_t s[IAVF_LOOK_AHEAD], nb_dd;
	int32_t i, j, nb_rx = 0;
	uint64_t pkt_flags;
	const uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;

	rxdp = (volatile union iavf_rx_flex_desc *)&rxq->rx_ring[rxq->rx_tail];
	rxep = &rxq->sw_ring[rxq->rx_tail];

	stat_err0 = rte_le_to_cpu_16(rxdp->wb.status_error0);

	/* Make sure there is at least 1 packet to receive */
	if (!(stat_err0 & (1 << IAVF_RX_FLEX_DESC_STATUS0_DD_S)))
		return 0;

	/* Scan LOOK_AHEAD descriptors at a time to determine which
	 * descriptors reference packets that are ready to be received.
	 */
	for (i = 0; i < IAVF_RX_MAX_BURST; i += IAVF_LOOK_AHEAD,
	     rxdp += IAVF_LOOK_AHEAD, rxep += IAVF_LOOK_AHEAD) {
		/* Read desc statuses backwards to avoid race condition */
		for (j = IAVF_LOOK_AHEAD - 1; j >= 0; j--)
			s[j] = rte_le_to_cpu_16(rxdp[j].wb.status_error0);

		rte_smp_rmb();

		/* Compute how many status bits were set */
		for (j = 0, nb_dd = 0; j < IAVF_LOOK_AHEAD; j++)
			nb_dd += s[j] & (1 << IAVF_RX_FLEX_DESC_STATUS0_DD_S);

		nb_rx += nb_dd;

		/* Translate descriptor info to mbuf parameters */
		for (j = 0; j < nb_dd; j++) {
			IAVF_DUMP_RX_DESC(rxq, &rxdp[j],
					  rxq->rx_tail +
					  i * IAVF_LOOK_AHEAD + j);

			mb = rxep[j];
			pkt_len = (rte_le_to_cpu_16(rxdp[j].wb.pkt_len) &
				IAVF_RX_FLX_DESC_PKT_LEN_M) - rxq->crc_len;
			mb->data_len = pkt_len;
			mb->pkt_len = pkt_len;
			mb->ol_flags = 0;

			mb->packet_type = ptype_tbl[IAVF_RX_FLEX_DESC_PTYPE_M &
				rte_le_to_cpu_16(rxdp[j].wb.ptype_flex_flags0)];
			iavf_flex_rxd_to_vlan_tci(mb, &rxdp[j]);
			iavf_rxd_to_pkt_fields(mb, &rxdp[j]);
			stat_err0 = rte_le_to_cpu_16(rxdp[j].wb.status_error0);
			pkt_flags = iavf_flex_rxd_error_to_pkt_flags(stat_err0);

			mb->ol_flags |= pkt_flags;
		}

		for (j = 0; j < IAVF_LOOK_AHEAD; j++)
			rxq->rx_stage[i + j] = rxep[j];

		if (nb_dd != IAVF_LOOK_AHEAD)
			break;
	}

	/* Clear software ring entries */
	for (i = 0; i < nb_rx; i++)
		rxq->sw_ring[rxq->rx_tail + i] = NULL;

	return nb_rx;
}

static inline int
iavf_rx_scan_hw_ring(struct iavf_rx_queue *rxq)
{
	volatile union iavf_rx_desc *rxdp;
	struct rte_mbuf **rxep;
	struct rte_mbuf *mb;
	uint16_t pkt_len;
	uint64_t qword1;
	uint32_t rx_status;
	int32_t s[IAVF_LOOK_AHEAD], nb_dd;
	int32_t i, j, nb_rx = 0;
	uint64_t pkt_flags;
	const uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;

	rxdp = &rxq->rx_ring[rxq->rx_tail];
	rxep = &rxq->sw_ring[rxq->rx_tail];

	qword1 = rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len);
	rx_status = (qword1 & IAVF_RXD_QW1_STATUS_MASK) >>
		    IAVF_RXD_QW1_STATUS_SHIFT;

	/* Make sure there is at least 1 packet to receive */
	if (!(rx_status & (1 << IAVF_RX_DESC_STATUS_DD_SHIFT)))
		return 0;

	/* Scan LOOK_AHEAD descriptors at a time to determine which
	 * descriptors reference packets that are ready to be received.
	 */
	for (i = 0; i < IAVF_RX_MAX_BURST; i += IAVF_LOOK_AHEAD,
	     rxdp += IAVF_LOOK_AHEAD, rxep += IAVF_LOOK_AHEAD) {
		/* Read desc statuses backwards to avoid race condition */
		for (j = IAVF_LOOK_AHEAD - 1; j >= 0; j--) {
			qword1 = rte_le_to_cpu_64(
				rxdp[j].wb.qword1.status_error_len);
			s[j] = (qword1 & IAVF_RXD_QW1_STATUS_MASK) >>
			       IAVF_RXD_QW1_STATUS_SHIFT;
		}

		rte_smp_rmb();

		/* Compute how many status bits were set */
		for (j = 0, nb_dd = 0; j < IAVF_LOOK_AHEAD; j++)
			nb_dd += s[j] & (1 << IAVF_RX_DESC_STATUS_DD_SHIFT);

		nb_rx += nb_dd;

		/* Translate descriptor info to mbuf parameters */
		for (j = 0; j < nb_dd; j++) {
			IAVF_DUMP_RX_DESC(rxq, &rxdp[j],
					 rxq->rx_tail + i * IAVF_LOOK_AHEAD + j);

			mb = rxep[j];
			qword1 = rte_le_to_cpu_64
					(rxdp[j].wb.qword1.status_error_len);
			pkt_len = ((qword1 & IAVF_RXD_QW1_LENGTH_PBUF_MASK) >>
				  IAVF_RXD_QW1_LENGTH_PBUF_SHIFT) - rxq->crc_len;
			mb->data_len = pkt_len;
			mb->pkt_len = pkt_len;
			mb->ol_flags = 0;
			iavf_rxd_to_vlan_tci(mb, &rxdp[j]);
			pkt_flags = iavf_rxd_to_pkt_flags(qword1);
			mb->packet_type =
				ptype_tbl[(uint8_t)((qword1 &
				IAVF_RXD_QW1_PTYPE_MASK) >>
				IAVF_RXD_QW1_PTYPE_SHIFT)];

			if (pkt_flags & PKT_RX_RSS_HASH)
				mb->hash.rss = rte_le_to_cpu_32(
					rxdp[j].wb.qword0.hi_dword.rss);

			if (pkt_flags & PKT_RX_FDIR)
				pkt_flags |= iavf_rxd_build_fdir(&rxdp[j], mb);

			mb->ol_flags |= pkt_flags;
		}

		for (j = 0; j < IAVF_LOOK_AHEAD; j++)
			rxq->rx_stage[i + j] = rxep[j];

		if (nb_dd != IAVF_LOOK_AHEAD)
			break;
	}

	/* Clear software ring entries */
	for (i = 0; i < nb_rx; i++)
		rxq->sw_ring[rxq->rx_tail + i] = NULL;

	return nb_rx;
}

static inline uint16_t
iavf_rx_fill_from_stage(struct iavf_rx_queue *rxq,
		       struct rte_mbuf **rx_pkts,
		       uint16_t nb_pkts)
{
	uint16_t i;
	struct rte_mbuf **stage = &rxq->rx_stage[rxq->rx_next_avail];

	nb_pkts = (uint16_t)RTE_MIN(nb_pkts, rxq->rx_nb_avail);

	for (i = 0; i < nb_pkts; i++)
		rx_pkts[i] = stage[i];

	rxq->rx_nb_avail = (uint16_t)(rxq->rx_nb_avail - nb_pkts);
	rxq->rx_next_avail = (uint16_t)(rxq->rx_next_avail + nb_pkts);

	return nb_pkts;
}

static inline int
iavf_rx_alloc_bufs(struct iavf_rx_queue *rxq)
{
	volatile union iavf_rx_desc *rxdp;
	struct rte_mbuf **rxep;
	struct rte_mbuf *mb;
	uint16_t alloc_idx, i;
	uint64_t dma_addr;
	int diag;

	/* Allocate buffers in bulk */
	alloc_idx = (uint16_t)(rxq->rx_free_trigger -
				(rxq->rx_free_thresh - 1));
	rxep = &rxq->sw_ring[alloc_idx];
	diag = rte_mempool_get_bulk(rxq->mp, (void *)rxep,
				    rxq->rx_free_thresh);
	if (unlikely(diag != 0)) {
		PMD_RX_LOG(ERR, "Failed to get mbufs in bulk");
		return -ENOMEM;
	}

	rxdp = &rxq->rx_ring[alloc_idx];
	for (i = 0; i < rxq->rx_free_thresh; i++) {
		if (likely(i < (rxq->rx_free_thresh - 1)))
			/* Prefetch next mbuf */
			rte_prefetch0(rxep[i + 1]);

		mb = rxep[i];
		rte_mbuf_refcnt_set(mb, 1);
		mb->next = NULL;
		mb->data_off = RTE_PKTMBUF_HEADROOM;
		mb->nb_segs = 1;
		mb->port = rxq->port_id;
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mb));
		rxdp[i].read.hdr_addr = 0;
		rxdp[i].read.pkt_addr = dma_addr;
	}

	/* Update rx tail register */
	rte_wmb();
	IAVF_PCI_REG_WRITE_RELAXED(rxq->qrx_tail, rxq->rx_free_trigger);

	rxq->rx_free_trigger =
		(uint16_t)(rxq->rx_free_trigger + rxq->rx_free_thresh);
	if (rxq->rx_free_trigger >= rxq->nb_rx_desc)
		rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);

	return 0;
}

static inline uint16_t
rx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct iavf_rx_queue *rxq = (struct iavf_rx_queue *)rx_queue;
	uint16_t nb_rx = 0;

	if (!nb_pkts)
		return 0;

	if (rxq->rx_nb_avail)
		return iavf_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	if (rxq->rxdid == IAVF_RXDID_COMMS_OVS_1)
		nb_rx = (uint16_t)iavf_rx_scan_hw_ring_flex_rxd(rxq);
	else
		nb_rx = (uint16_t)iavf_rx_scan_hw_ring(rxq);
	rxq->rx_next_avail = 0;
	rxq->rx_nb_avail = nb_rx;
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_rx);

	if (rxq->rx_tail > rxq->rx_free_trigger) {
		if (iavf_rx_alloc_bufs(rxq) != 0) {
			uint16_t i, j;

			/* TODO: count rx_mbuf_alloc_failed here */

			rxq->rx_nb_avail = 0;
			rxq->rx_tail = (uint16_t)(rxq->rx_tail - nb_rx);
			for (i = 0, j = rxq->rx_tail; i < nb_rx; i++, j++)
				rxq->sw_ring[j] = rxq->rx_stage[i];

			return 0;
		}
	}

	if (rxq->rx_tail >= rxq->nb_rx_desc)
		rxq->rx_tail = 0;

	PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_tail=%u, nb_rx=%u",
		   rxq->port_id, rxq->queue_id,
		   rxq->rx_tail, nb_rx);

	if (rxq->rx_nb_avail)
		return iavf_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	return 0;
}

static uint16_t
iavf_recv_pkts_bulk_alloc(void *rx_queue,
			 struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	uint16_t nb_rx = 0, n, count;

	if (unlikely(nb_pkts == 0))
		return 0;

	if (likely(nb_pkts <= IAVF_RX_MAX_BURST))
		return rx_recv_pkts(rx_queue, rx_pkts, nb_pkts);

	while (nb_pkts) {
		n = RTE_MIN(nb_pkts, IAVF_RX_MAX_BURST);
		count = rx_recv_pkts(rx_queue, &rx_pkts[nb_rx], n);
		nb_rx = (uint16_t)(nb_rx + count);
		nb_pkts = (uint16_t)(nb_pkts - count);
		if (count < n)
			break;
	}

	return nb_rx;
}

static inline int
iavf_xmit_cleanup(struct iavf_tx_queue *txq)
{
	struct iavf_tx_entry *sw_ring = txq->sw_ring;
	uint16_t last_desc_cleaned = txq->last_desc_cleaned;
	uint16_t nb_tx_desc = txq->nb_tx_desc;
	uint16_t desc_to_clean_to;
	uint16_t nb_tx_to_clean;

	volatile struct iavf_tx_desc *txd = txq->tx_ring;

	desc_to_clean_to = (uint16_t)(last_desc_cleaned + txq->rs_thresh);
	if (desc_to_clean_to >= nb_tx_desc)
		desc_to_clean_to = (uint16_t)(desc_to_clean_to - nb_tx_desc);

	desc_to_clean_to = sw_ring[desc_to_clean_to].last_id;
	if ((txd[desc_to_clean_to].cmd_type_offset_bsz &
			rte_cpu_to_le_64(IAVF_TXD_QW1_DTYPE_MASK)) !=
			rte_cpu_to_le_64(IAVF_TX_DESC_DTYPE_DESC_DONE)) {
		PMD_TX_FREE_LOG(DEBUG, "TX descriptor %4u is not done "
				"(port=%d queue=%d)", desc_to_clean_to,
				txq->port_id, txq->queue_id);
		return -1;
	}

	if (last_desc_cleaned > desc_to_clean_to)
		nb_tx_to_clean = (uint16_t)((nb_tx_desc - last_desc_cleaned) +
							desc_to_clean_to);
	else
		nb_tx_to_clean = (uint16_t)(desc_to_clean_to -
					last_desc_cleaned);

	txd[desc_to_clean_to].cmd_type_offset_bsz = 0;

	txq->last_desc_cleaned = desc_to_clean_to;
	txq->nb_free = (uint16_t)(txq->nb_free + nb_tx_to_clean);

	return 0;
}

/* Check if the context descriptor is needed for TX offloading */
static inline uint16_t
iavf_calc_context_desc(uint64_t flags)
{
	static uint64_t mask = PKT_TX_TCP_SEG;

	return (flags & mask) ? 1 : 0;
}

static inline void
iavf_txd_enable_checksum(uint64_t ol_flags,
			uint32_t *td_cmd,
			uint32_t *td_offset,
			union iavf_tx_offload tx_offload)
{
	/* Set MACLEN */
	*td_offset |= (tx_offload.l2_len >> 1) <<
		      IAVF_TX_DESC_LENGTH_MACLEN_SHIFT;

	/* Enable L3 checksum offloads */
	if (ol_flags & PKT_TX_IP_CKSUM) {
		*td_cmd |= IAVF_TX_DESC_CMD_IIPT_IPV4_CSUM;
		*td_offset |= (tx_offload.l3_len >> 2) <<
			      IAVF_TX_DESC_LENGTH_IPLEN_SHIFT;
	} else if (ol_flags & PKT_TX_IPV4) {
		*td_cmd |= IAVF_TX_DESC_CMD_IIPT_IPV4;
		*td_offset |= (tx_offload.l3_len >> 2) <<
			      IAVF_TX_DESC_LENGTH_IPLEN_SHIFT;
	} else if (ol_flags & PKT_TX_IPV6) {
		*td_cmd |= IAVF_TX_DESC_CMD_IIPT_IPV6;
		*td_offset |= (tx_offload.l3_len >> 2) <<
			      IAVF_TX_DESC_LENGTH_IPLEN_SHIFT;
	}

	if (ol_flags & PKT_TX_TCP_SEG) {
		*td_cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_TCP;
		*td_offset |= (tx_offload.l4_len >> 2) <<
			      IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		return;
	}

	/* Enable L4 checksum offloads */
	switch (ol_flags & PKT_TX_L4_MASK) {
	case PKT_TX_TCP_CKSUM:
		*td_cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_TCP;
		*td_offset |= (sizeof(struct rte_tcp_hdr) >> 2) <<
			      IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case PKT_TX_SCTP_CKSUM:
		*td_cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_SCTP;
		*td_offset |= (sizeof(struct rte_sctp_hdr) >> 2) <<
			      IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case PKT_TX_UDP_CKSUM:
		*td_cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_UDP;
		*td_offset |= (sizeof(struct rte_udp_hdr) >> 2) <<
			      IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	default:
		break;
	}
}

/* set TSO context descriptor
 * support IP -> L4 and IP -> IP -> L4
 */
static inline uint64_t
iavf_set_tso_ctx(struct rte_mbuf *mbuf, union iavf_tx_offload tx_offload)
{
	uint64_t ctx_desc = 0;
	uint32_t cd_cmd, hdr_len, cd_tso_len;

	if (!tx_offload.l4_len) {
		PMD_TX_LOG(DEBUG, "L4 length set to 0");
		return ctx_desc;
	}

	hdr_len = tx_offload.l2_len +
		  tx_offload.l3_len +
		  tx_offload.l4_len;

	cd_cmd = IAVF_TX_CTX_DESC_TSO;
	cd_tso_len = mbuf->pkt_len - hdr_len;
	ctx_desc |= ((uint64_t)cd_cmd << IAVF_TXD_CTX_QW1_CMD_SHIFT) |
		     ((uint64_t)cd_tso_len << IAVF_TXD_CTX_QW1_TSO_LEN_SHIFT) |
		     ((uint64_t)mbuf->tso_segsz << IAVF_TXD_CTX_QW1_MSS_SHIFT);

	return ctx_desc;
}

/* Construct the tx flags */
static inline uint64_t
iavf_build_ctob(uint32_t td_cmd, uint32_t td_offset, unsigned int size,
	       uint32_t td_tag)
{
	return rte_cpu_to_le_64(IAVF_TX_DESC_DTYPE_DATA |
				((uint64_t)td_cmd  << IAVF_TXD_QW1_CMD_SHIFT) |
				((uint64_t)td_offset <<
				 IAVF_TXD_QW1_OFFSET_SHIFT) |
				((uint64_t)size  <<
				 IAVF_TXD_QW1_TX_BUF_SZ_SHIFT) |
				((uint64_t)td_tag  <<
				 IAVF_TXD_QW1_L2TAG1_SHIFT));
}

/* TX function */
uint16_t
iavf_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	volatile struct iavf_tx_desc *txd;
	volatile struct iavf_tx_desc *txr;
	struct iavf_tx_queue *txq;
	struct iavf_tx_entry *sw_ring;
	struct iavf_tx_entry *txe, *txn;
	struct rte_mbuf *tx_pkt;
	struct rte_mbuf *m_seg;
	uint16_t tx_id;
	uint16_t nb_tx;
	uint32_t td_cmd;
	uint32_t td_offset;
	uint32_t td_tag;
	uint64_t ol_flags;
	uint16_t nb_used;
	uint16_t nb_ctx;
	uint16_t tx_last;
	uint16_t slen;
	uint64_t buf_dma_addr;
	union iavf_tx_offload tx_offload = {0};

	txq = tx_queue;
	sw_ring = txq->sw_ring;
	txr = txq->tx_ring;
	tx_id = txq->tx_tail;
	txe = &sw_ring[tx_id];

	/* Check if the descriptor ring needs to be cleaned. */
	if (txq->nb_free < txq->free_thresh)
		iavf_xmit_cleanup(txq);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		td_cmd = 0;
		td_tag = 0;
		td_offset = 0;

		tx_pkt = *tx_pkts++;
		RTE_MBUF_PREFETCH_TO_FREE(txe->mbuf);

		ol_flags = tx_pkt->ol_flags;
		tx_offload.l2_len = tx_pkt->l2_len;
		tx_offload.l3_len = tx_pkt->l3_len;
		tx_offload.l4_len = tx_pkt->l4_len;
		tx_offload.tso_segsz = tx_pkt->tso_segsz;

		/* Calculate the number of context descriptors needed. */
		nb_ctx = iavf_calc_context_desc(ol_flags);

		/* The number of descriptors that must be allocated for
		 * a packet equals to the number of the segments of that
		 * packet plus 1 context descriptor if needed.
		 */
		nb_used = (uint16_t)(tx_pkt->nb_segs + nb_ctx);
		tx_last = (uint16_t)(tx_id + nb_used - 1);

		/* Circular ring */
		if (tx_last >= txq->nb_tx_desc)
			tx_last = (uint16_t)(tx_last - txq->nb_tx_desc);

		PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u"
			   " tx_first=%u tx_last=%u",
			   txq->port_id, txq->queue_id, tx_id, tx_last);

		if (nb_used > txq->nb_free) {
			if (iavf_xmit_cleanup(txq)) {
				if (nb_tx == 0)
					return 0;
				goto end_of_tx;
			}
			if (unlikely(nb_used > txq->rs_thresh)) {
				while (nb_used > txq->nb_free) {
					if (iavf_xmit_cleanup(txq)) {
						if (nb_tx == 0)
							return 0;
						goto end_of_tx;
					}
				}
			}
		}

		/* Descriptor based VLAN insertion */
		if (ol_flags & PKT_TX_VLAN_PKT) {
			td_cmd |= IAVF_TX_DESC_CMD_IL2TAG1;
			td_tag = tx_pkt->vlan_tci;
		}

		/* According to datasheet, the bit2 is reserved and must be
		 * set to 1.
		 */
		td_cmd |= 0x04;

		/* Enable checksum offloading */
		if (ol_flags & IAVF_TX_CKSUM_OFFLOAD_MASK)
			iavf_txd_enable_checksum(ol_flags, &td_cmd,
						&td_offset, tx_offload);

		if (nb_ctx) {
			/* Setup TX context descriptor if required */
			uint64_t cd_type_cmd_tso_mss =
				IAVF_TX_DESC_DTYPE_CONTEXT;
			volatile struct iavf_tx_context_desc *ctx_txd =
				(volatile struct iavf_tx_context_desc *)
							&txr[tx_id];

			txn = &sw_ring[txe->next_id];
			RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);
			if (txe->mbuf) {
				rte_pktmbuf_free_seg(txe->mbuf);
				txe->mbuf = NULL;
			}

			/* TSO enabled */
			if (ol_flags & PKT_TX_TCP_SEG)
				cd_type_cmd_tso_mss |=
					iavf_set_tso_ctx(tx_pkt, tx_offload);

			ctx_txd->type_cmd_tso_mss =
				rte_cpu_to_le_64(cd_type_cmd_tso_mss);

			IAVF_DUMP_TX_DESC(txq, &txr[tx_id], tx_id);
			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
		}

		m_seg = tx_pkt;
		do {
			txd = &txr[tx_id];
			txn = &sw_ring[txe->next_id];

			if (txe->mbuf)
				rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = m_seg;

			/* Setup TX Descriptor */
			slen = m_seg->data_len;
			buf_dma_addr = rte_mbuf_data_iova(m_seg);
			txd->buffer_addr = rte_cpu_to_le_64(buf_dma_addr);
			txd->cmd_type_offset_bsz = iavf_build_ctob(td_cmd,
								  td_offset,
								  slen,
								  td_tag);

			IAVF_DUMP_TX_DESC(txq, txd, tx_id);
			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
			m_seg = m_seg->next;
		} while (m_seg);

		/* The last packet data descriptor needs End Of Packet (EOP) */
		td_cmd |= IAVF_TX_DESC_CMD_EOP;
		txq->nb_used = (uint16_t)(txq->nb_used + nb_used);
		txq->nb_free = (uint16_t)(txq->nb_free - nb_used);

		if (txq->nb_used >= txq->rs_thresh) {
			PMD_TX_LOG(DEBUG, "Setting RS bit on TXD id="
				   "%4u (port=%d queue=%d)",
				   tx_last, txq->port_id, txq->queue_id);

			td_cmd |= IAVF_TX_DESC_CMD_RS;

			/* Update txq RS bit counters */
			txq->nb_used = 0;
		}

		txd->cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)td_cmd) <<
					 IAVF_TXD_QW1_CMD_SHIFT);
		IAVF_DUMP_TX_DESC(txq, txd, tx_id);
	}

end_of_tx:
	rte_wmb();

	PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u tx_tail=%u nb_tx=%u",
		   txq->port_id, txq->queue_id, tx_id, nb_tx);

	IAVF_PCI_REG_WRITE_RELAXED(txq->qtx_tail, tx_id);
	txq->tx_tail = tx_id;

	return nb_tx;
}

/* TX prep functions */
uint16_t
iavf_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
	      uint16_t nb_pkts)
{
	int i, ret;
	uint64_t ol_flags;
	struct rte_mbuf *m;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;

		/* Check condition for nb_segs > IAVF_TX_MAX_MTU_SEG. */
		if (!(ol_flags & PKT_TX_TCP_SEG)) {
			if (m->nb_segs > IAVF_TX_MAX_MTU_SEG) {
				rte_errno = EINVAL;
				return i;
			}
		} else if ((m->tso_segsz < IAVF_MIN_TSO_MSS) ||
			   (m->tso_segsz > IAVF_MAX_TSO_MSS)) {
			/* MSS outside the range are considered malicious */
			rte_errno = EINVAL;
			return i;
		}

		if (ol_flags & IAVF_TX_OFFLOAD_NOTSUP_MASK) {
			rte_errno = ENOTSUP;
			return i;
		}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
#endif
		ret = rte_net_intel_cksum_prepare(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
	}

	return i;
}

/* choose rx function*/
void
iavf_set_rx_function(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
#ifdef RTE_ARCH_X86
	struct iavf_rx_queue *rxq;
	int i;
	bool use_avx2 = false;

	if (!iavf_rx_vec_dev_check(dev)) {
		for (i = 0; i < dev->data->nb_rx_queues; i++) {
			rxq = dev->data->rx_queues[i];
			(void)iavf_rxq_vec_setup(rxq);
		}

		if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2) == 1 ||
		    rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) == 1)
			use_avx2 = true;

		if (dev->data->scattered_rx) {
			PMD_DRV_LOG(DEBUG,
				    "Using %sVector Scattered Rx (port %d).",
				    use_avx2 ? "avx2 " : "",
				    dev->data->port_id);
			if (vf->vf_res->vf_cap_flags &
				VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC)
				dev->rx_pkt_burst = use_avx2 ?
					iavf_recv_scattered_pkts_vec_avx2_flex_rxd :
					iavf_recv_scattered_pkts_vec_flex_rxd;
			else
				dev->rx_pkt_burst = use_avx2 ?
					iavf_recv_scattered_pkts_vec_avx2 :
					iavf_recv_scattered_pkts_vec;
		} else {
			PMD_DRV_LOG(DEBUG, "Using %sVector Rx (port %d).",
				    use_avx2 ? "avx2 " : "",
				    dev->data->port_id);
			if (vf->vf_res->vf_cap_flags &
				VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC)
				dev->rx_pkt_burst = use_avx2 ?
					iavf_recv_pkts_vec_avx2_flex_rxd :
					iavf_recv_pkts_vec_flex_rxd;
			else
				dev->rx_pkt_burst = use_avx2 ?
					iavf_recv_pkts_vec_avx2 :
					iavf_recv_pkts_vec;
		}

		return;
	}
#endif

	if (dev->data->scattered_rx) {
		PMD_DRV_LOG(DEBUG, "Using a Scattered Rx callback (port=%d).",
			    dev->data->port_id);
		if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC)
			dev->rx_pkt_burst = iavf_recv_scattered_pkts_flex_rxd;
		else
			dev->rx_pkt_burst = iavf_recv_scattered_pkts;
	} else if (adapter->rx_bulk_alloc_allowed) {
		PMD_DRV_LOG(DEBUG, "Using bulk Rx callback (port=%d).",
			    dev->data->port_id);
		dev->rx_pkt_burst = iavf_recv_pkts_bulk_alloc;
	} else {
		PMD_DRV_LOG(DEBUG, "Using Basic Rx callback (port=%d).",
			    dev->data->port_id);
		if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC)
			dev->rx_pkt_burst = iavf_recv_pkts_flex_rxd;
		else
			dev->rx_pkt_burst = iavf_recv_pkts;
	}
}

/* choose tx function*/
void
iavf_set_tx_function(struct rte_eth_dev *dev)
{
#ifdef RTE_ARCH_X86
	struct iavf_tx_queue *txq;
	int i;
	bool use_avx2 = false;

	if (!iavf_tx_vec_dev_check(dev)) {
		for (i = 0; i < dev->data->nb_tx_queues; i++) {
			txq = dev->data->tx_queues[i];
			if (!txq)
				continue;
			iavf_txq_vec_setup(txq);
		}

		if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2) == 1 ||
		    rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) == 1)
			use_avx2 = true;

		PMD_DRV_LOG(DEBUG, "Using %sVector Tx (port %d).",
			    use_avx2 ? "avx2 " : "",
			    dev->data->port_id);
		dev->tx_pkt_burst = use_avx2 ?
				    iavf_xmit_pkts_vec_avx2 :
				    iavf_xmit_pkts_vec;
		dev->tx_pkt_prepare = NULL;

		return;
	}
#endif

	PMD_DRV_LOG(DEBUG, "Using Basic Tx callback (port=%d).",
		    dev->data->port_id);
	dev->tx_pkt_burst = iavf_xmit_pkts;
	dev->tx_pkt_prepare = iavf_prep_pkts;
}

void
iavf_dev_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		     struct rte_eth_rxq_info *qinfo)
{
	struct iavf_rx_queue *rxq;

	rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mp;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = rxq->nb_rx_desc;

	qinfo->conf.rx_free_thresh = rxq->rx_free_thresh;
	qinfo->conf.rx_drop_en = true;
	qinfo->conf.rx_deferred_start = rxq->rx_deferred_start;
}

void
iavf_dev_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		     struct rte_eth_txq_info *qinfo)
{
	struct iavf_tx_queue *txq;

	txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_tx_desc;

	qinfo->conf.tx_free_thresh = txq->free_thresh;
	qinfo->conf.tx_rs_thresh = txq->rs_thresh;
	qinfo->conf.offloads = txq->offloads;
	qinfo->conf.tx_deferred_start = txq->tx_deferred_start;
}

/* Get the number of used descriptors of a rx queue */
uint32_t
iavf_dev_rxq_count(struct rte_eth_dev *dev, uint16_t queue_id)
{
#define IAVF_RXQ_SCAN_INTERVAL 4
	volatile union iavf_rx_desc *rxdp;
	struct iavf_rx_queue *rxq;
	uint16_t desc = 0;

	rxq = dev->data->rx_queues[queue_id];
	rxdp = &rxq->rx_ring[rxq->rx_tail];

	while ((desc < rxq->nb_rx_desc) &&
	       ((rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len) &
		 IAVF_RXD_QW1_STATUS_MASK) >> IAVF_RXD_QW1_STATUS_SHIFT) &
	       (1 << IAVF_RX_DESC_STATUS_DD_SHIFT)) {
		/* Check the DD bit of a rx descriptor of each 4 in a group,
		 * to avoid checking too frequently and downgrading performance
		 * too much.
		 */
		desc += IAVF_RXQ_SCAN_INTERVAL;
		rxdp += IAVF_RXQ_SCAN_INTERVAL;
		if (rxq->rx_tail + desc >= rxq->nb_rx_desc)
			rxdp = &(rxq->rx_ring[rxq->rx_tail +
					desc - rxq->nb_rx_desc]);
	}

	return desc;
}

int
iavf_dev_rx_desc_status(void *rx_queue, uint16_t offset)
{
	struct iavf_rx_queue *rxq = rx_queue;
	volatile uint64_t *status;
	uint64_t mask;
	uint32_t desc;

	if (unlikely(offset >= rxq->nb_rx_desc))
		return -EINVAL;

	if (offset >= rxq->nb_rx_desc - rxq->nb_rx_hold)
		return RTE_ETH_RX_DESC_UNAVAIL;

	desc = rxq->rx_tail + offset;
	if (desc >= rxq->nb_rx_desc)
		desc -= rxq->nb_rx_desc;

	status = &rxq->rx_ring[desc].wb.qword1.status_error_len;
	mask = rte_le_to_cpu_64((1ULL << IAVF_RX_DESC_STATUS_DD_SHIFT)
		<< IAVF_RXD_QW1_STATUS_SHIFT);
	if (*status & mask)
		return RTE_ETH_RX_DESC_DONE;

	return RTE_ETH_RX_DESC_AVAIL;
}

int
iavf_dev_tx_desc_status(void *tx_queue, uint16_t offset)
{
	struct iavf_tx_queue *txq = tx_queue;
	volatile uint64_t *status;
	uint64_t mask, expect;
	uint32_t desc;

	if (unlikely(offset >= txq->nb_tx_desc))
		return -EINVAL;

	desc = txq->tx_tail + offset;
	/* go to next desc that has the RS bit */
	desc = ((desc + txq->rs_thresh - 1) / txq->rs_thresh) *
		txq->rs_thresh;
	if (desc >= txq->nb_tx_desc) {
		desc -= txq->nb_tx_desc;
		if (desc >= txq->nb_tx_desc)
			desc -= txq->nb_tx_desc;
	}

	status = &txq->tx_ring[desc].cmd_type_offset_bsz;
	mask = rte_le_to_cpu_64(IAVF_TXD_QW1_DTYPE_MASK);
	expect = rte_cpu_to_le_64(
		 IAVF_TX_DESC_DTYPE_DESC_DONE << IAVF_TXD_QW1_DTYPE_SHIFT);
	if ((*status & mask) == expect)
		return RTE_ETH_TX_DESC_DONE;

	return RTE_ETH_TX_DESC_FULL;
}

const uint32_t *
iavf_get_default_ptype_table(void)
{
	static const uint32_t ptype_tbl[IAVF_MAX_PKT_TYPE]
		__rte_cache_aligned = {
		/* L2 types */
		/* [0] reserved */
		[1] = RTE_PTYPE_L2_ETHER,
		[2] = RTE_PTYPE_L2_ETHER_TIMESYNC,
		/* [3] - [5] reserved */
		[6] = RTE_PTYPE_L2_ETHER_LLDP,
		/* [7] - [10] reserved */
		[11] = RTE_PTYPE_L2_ETHER_ARP,
		/* [12] - [21] reserved */

		/* Non tunneled IPv4 */
		[22] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_L4_FRAG,
		[23] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_L4_NONFRAG,
		[24] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_L4_UDP,
		/* [25] reserved */
		[26] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_L4_TCP,
		[27] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_L4_SCTP,
		[28] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_L4_ICMP,

		/* IPv4 --> IPv4 */
		[29] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[30] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[31] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [32] reserved */
		[33] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[34] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_SCTP,
		[35] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> IPv6 */
		[36] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[37] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[38] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [39] reserved */
		[40] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[41] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_SCTP,
		[42] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> GRE/Teredo/VXLAN */
		[43] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT,

		/* IPv4 --> GRE/Teredo/VXLAN --> IPv4 */
		[44] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[45] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[46] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [47] reserved */
		[48] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[49] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_SCTP,
		[50] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> GRE/Teredo/VXLAN --> IPv6 */
		[51] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[52] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[53] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [54] reserved */
		[55] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[56] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_SCTP,
		[57] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> GRE/Teredo/VXLAN --> MAC */
		[58] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER,

		/* IPv4 --> GRE/Teredo/VXLAN --> MAC --> IPv4 */
		[59] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[60] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[61] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [62] reserved */
		[63] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[64] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_SCTP,
		[65] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> GRE/Teredo/VXLAN --> MAC --> IPv6 */
		[66] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[67] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[68] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [69] reserved */
		[70] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[71] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_SCTP,
		[72] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
		       RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_ICMP,
		/* [73] - [87] reserved */

		/* Non tunneled IPv6 */
		[88] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_L4_FRAG,
		[89] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_L4_NONFRAG,
		[90] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_L4_UDP,
		/* [91] reserved */
		[92] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_L4_TCP,
		[93] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_L4_SCTP,
		[94] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_L4_ICMP,

		/* IPv6 --> IPv4 */
		[95] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_FRAG,
		[96] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_NONFRAG,
		[97] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_UDP,
		/* [98] reserved */
		[99] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
		       RTE_PTYPE_TUNNEL_IP |
		       RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
		       RTE_PTYPE_INNER_L4_TCP,
		[100] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_SCTP,
		[101] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> IPv6 */
		[102] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[103] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[104] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		/* [105] reserved */
		[106] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[107] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_SCTP,
		[108] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_IP |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> GRE/Teredo/VXLAN */
		[109] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT,

		/* IPv6 --> GRE/Teredo/VXLAN --> IPv4 */
		[110] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[111] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[112] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		/* [113] reserved */
		[114] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[115] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_SCTP,
		[116] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> GRE/Teredo/VXLAN --> IPv6 */
		[117] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[118] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[119] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		/* [120] reserved */
		[121] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[122] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_SCTP,
		[123] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> GRE/Teredo/VXLAN --> MAC */
		[124] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER,

		/* IPv6 --> GRE/Teredo/VXLAN --> MAC --> IPv4 */
		[125] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[126] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[127] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		/* [128] reserved */
		[129] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[130] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_SCTP,
		[131] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> GRE/Teredo/VXLAN --> MAC --> IPv6 */
		[132] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[133] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[134] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		/* [135] reserved */
		[136] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[137] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_SCTP,
		[138] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GRENAT | RTE_PTYPE_INNER_L2_ETHER |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,
		/* [139] - [299] reserved */

		/* PPPoE */
		[300] = RTE_PTYPE_L2_ETHER_PPPOE,
		[301] = RTE_PTYPE_L2_ETHER_PPPOE,

		/* PPPoE --> IPv4 */
		[302] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_FRAG,
		[303] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_NONFRAG,
		[304] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[305] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_TCP,
		[306] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_SCTP,
		[307] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_L4_ICMP,

		/* PPPoE --> IPv6 */
		[308] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_FRAG,
		[309] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_NONFRAG,
		[310] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_UDP,
		[311] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_TCP,
		[312] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_SCTP,
		[313] = RTE_PTYPE_L2_ETHER_PPPOE |
			RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_L4_ICMP,
		/* [314] - [324] reserved */

		/* IPv4/IPv6 --> GTPC/GTPU */
		[325] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPC,
		[326] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPC,
		[327] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPC,
		[328] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPC,
		[329] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU,
		[330] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU,

		/* IPv4 --> GTPU --> IPv4 */
		[331] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[332] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[333] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		[334] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[335] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> GTPU --> IPv4 */
		[336] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[337] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[338] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		[339] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[340] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv4 --> GTPU --> IPv6 */
		[341] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[342] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[343] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		[344] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[345] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV4_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,

		/* IPv6 --> GTPU --> IPv6 */
		[346] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_FRAG,
		[347] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_NONFRAG,
		[348] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_UDP,
		[349] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_TCP,
		[350] = RTE_PTYPE_L2_ETHER | RTE_PTYPE_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_TUNNEL_GTPU |
			RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN |
			RTE_PTYPE_INNER_L4_ICMP,
		/* All others reserved */
	};

	return ptype_tbl;
}
