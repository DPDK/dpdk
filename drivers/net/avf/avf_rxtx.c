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
#include <rte_ethdev.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_udp.h>
#include <rte_ip.h>
#include <rte_net.h>

#include "avf_log.h"
#include "base/avf_prototype.h"
#include "base/avf_type.h"
#include "avf.h"
#include "avf_rxtx.h"

static inline int
check_rx_thresh(uint16_t nb_desc, uint16_t thresh)
{
	/* The following constraints must be satisfied:
	 *   thresh >= AVF_RX_MAX_BURST
	 *   thresh < rxq->nb_rx_desc
	 *   (rxq->nb_rx_desc % thresh) == 0
	 */
	if (thresh < AVF_RX_MAX_BURST ||
	    thresh >= nb_desc ||
	    (nb_desc % thresh != 0)) {
		PMD_INIT_LOG(ERR, "rx_free_thresh (%u) must be less than %u, "
			     "greater than or equal to %u, "
			     "and a divisor of %u",
			     thresh, nb_desc, AVF_RX_MAX_BURST, nb_desc);
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

static inline void
reset_rx_queue(struct avf_rx_queue *rxq)
{
	uint16_t len, i;

	if (!rxq)
		return;

	len = rxq->nb_rx_desc + AVF_RX_MAX_BURST;

	for (i = 0; i < len * sizeof(union avf_rx_desc); i++)
		((volatile char *)rxq->rx_ring)[i] = 0;

	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));

	for (i = 0; i < AVF_RX_MAX_BURST; i++)
		rxq->sw_ring[rxq->nb_rx_desc + i] = &rxq->fake_mbuf;

	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

static inline void
reset_tx_queue(struct avf_tx_queue *txq)
{
	struct avf_tx_entry *txe;
	uint16_t i, prev, size;

	if (!txq) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq is NULL");
		return;
	}

	txe = txq->sw_ring;
	size = sizeof(struct avf_tx_desc) * txq->nb_tx_desc;
	for (i = 0; i < size; i++)
		((volatile char *)txq->tx_ring)[i] = 0;

	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		txq->tx_ring[i].cmd_type_offset_bsz =
			rte_cpu_to_le_64(AVF_TX_DESC_DTYPE_DESC_DONE);
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
alloc_rxq_mbufs(struct avf_rx_queue *rxq)
{
	volatile union avf_rx_desc *rxd;
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
#ifndef RTE_LIBRTE_AVF_16BYTE_RX_DESC
		rxd->read.rsvd1 = 0;
		rxd->read.rsvd2 = 0;
#endif

		rxq->sw_ring[i] = mbuf;
	}

	return 0;
}

static inline void
release_rxq_mbufs(struct avf_rx_queue *rxq)
{
	struct rte_mbuf *mbuf;
	uint16_t i;

	if (!rxq->sw_ring)
		return;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		if (rxq->sw_ring[i]) {
			rte_pktmbuf_free_seg(rxq->sw_ring[i]);
			rxq->sw_ring[i] = NULL;
		}
	}
}

static inline void
release_txq_mbufs(struct avf_tx_queue *txq)
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

int
avf_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		       uint16_t nb_desc, unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mp)
{
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct avf_adapter *ad =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_rx_queue *rxq;
	const struct rte_memzone *mz;
	uint32_t ring_size;
	uint16_t len, i;
	uint16_t rx_free_thresh;
	uint16_t base, bsf, tc_mapping;

	PMD_INIT_FUNC_TRACE();

	if (nb_desc % AVF_ALIGN_RING_DESC != 0 ||
	    nb_desc > AVF_MAX_RING_DESC ||
	    nb_desc < AVF_MIN_RING_DESC) {
		PMD_INIT_LOG(ERR, "Number (%u) of receive descriptors is "
			     "invalid", nb_desc);
		return -EINVAL;
	}

	/* Check free threshold */
	rx_free_thresh = (rx_conf->rx_free_thresh == 0) ?
			 AVF_DEFAULT_RX_FREE_THRESH :
			 rx_conf->rx_free_thresh;
	if (check_rx_thresh(nb_desc, rx_free_thresh) != 0)
		return -EINVAL;

	/* Free memory if needed */
	if (dev->data->rx_queues[queue_idx]) {
		avf_dev_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Allocate the rx queue data structure */
	rxq = rte_zmalloc_socket("avf rxq",
				 sizeof(struct avf_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (!rxq) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for "
			     "rx queue data structure");
		return -ENOMEM;
	}

	rxq->mp = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->port_id = dev->data->port_id;
	rxq->crc_len = 0; /* crc stripping by default */
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;
	rxq->rx_hdr_len = 0;

	len = rte_pktmbuf_data_room_size(rxq->mp) - RTE_PKTMBUF_HEADROOM;
	rxq->rx_buf_len = RTE_ALIGN(len, (1 << AVF_RXQ_CTX_DBUFF_SHIFT));

	/* Allocate the software ring. */
	len = nb_desc + AVF_RX_MAX_BURST;
	rxq->sw_ring =
		rte_zmalloc_socket("avf rx sw ring",
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
	len = AVF_MAX_RING_DESC + AVF_RX_MAX_BURST;
	ring_size = RTE_ALIGN(len * sizeof(union avf_rx_desc),
			      AVF_DMA_MEM_ALIGN);
	mz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx,
				      ring_size, AVF_RING_BASE_ALIGN,
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
	rxq->rx_ring = (union avf_rx_desc *)mz->addr;

	rxq->mz = mz;
	reset_rx_queue(rxq);
	rxq->q_set = TRUE;
	dev->data->rx_queues[queue_idx] = rxq;
	rxq->qrx_tail = hw->hw_addr + AVF_QRX_TAIL1(rxq->queue_id);

	return 0;
}

int
avf_dev_tx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t queue_idx,
		       uint16_t nb_desc,
		       unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf)
{
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct avf_tx_queue *txq;
	const struct rte_memzone *mz;
	uint32_t ring_size;
	uint16_t tx_rs_thresh, tx_free_thresh;
	uint16_t i, base, bsf, tc_mapping;

	PMD_INIT_FUNC_TRACE();

	if (nb_desc % AVF_ALIGN_RING_DESC != 0 ||
	    nb_desc > AVF_MAX_RING_DESC ||
	    nb_desc < AVF_MIN_RING_DESC) {
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
		avf_dev_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket("avf txq",
				 sizeof(struct avf_tx_queue),
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
	txq->txq_flags = tx_conf->txq_flags;
	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	/* Allocate software ring */
	txq->sw_ring =
		rte_zmalloc_socket("avf tx sw ring",
				   sizeof(struct avf_tx_entry) * nb_desc,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (!txq->sw_ring) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW TX ring");
		rte_free(txq);
		return -ENOMEM;
	}

	/* Allocate TX hardware ring descriptors. */
	ring_size = sizeof(struct avf_tx_desc) * AVF_MAX_RING_DESC;
	ring_size = RTE_ALIGN(ring_size, AVF_DMA_MEM_ALIGN);
	mz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx,
				      ring_size, AVF_RING_BASE_ALIGN,
				      socket_id);
	if (!mz) {
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for TX");
		rte_free(txq->sw_ring);
		rte_free(txq);
		return -ENOMEM;
	}
	txq->tx_ring_phys_addr = mz->iova;
	txq->tx_ring = (struct avf_tx_desc *)mz->addr;

	txq->mz = mz;
	reset_tx_queue(txq);
	txq->q_set = TRUE;
	dev->data->tx_queues[queue_idx] = txq;
	txq->qtx_tail = hw->hw_addr + AVF_QTX_TAIL1(queue_idx);

	return 0;
}

int
avf_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct avf_rx_queue *rxq;
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
	AVF_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);
	AVF_WRITE_FLUSH(hw);

	/* Ready to switch the queue on */
	err = avf_switch_queue(adapter, rx_queue_id, TRUE, TRUE);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u on",
			    rx_queue_id);
	else
		dev->data->rx_queue_state[rx_queue_id] =
			RTE_ETH_QUEUE_STATE_STARTED;

	return err;
}

int
avf_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct avf_tx_queue *txq;
	int err = 0;

	PMD_DRV_FUNC_TRACE();

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	txq = dev->data->tx_queues[tx_queue_id];

	/* Init the RX tail register. */
	AVF_PCI_REG_WRITE(txq->qtx_tail, 0);
	AVF_WRITE_FLUSH(hw);

	/* Ready to switch the queue on */
	err = avf_switch_queue(adapter, tx_queue_id, FALSE, TRUE);

	if (err)
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u on",
			    tx_queue_id);
	else
		dev->data->tx_queue_state[tx_queue_id] =
			RTE_ETH_QUEUE_STATE_STARTED;

	return err;
}

int
avf_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_rx_queue *rxq;
	int err;

	PMD_DRV_FUNC_TRACE();

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	err = avf_switch_queue(adapter, rx_queue_id, TRUE, FALSE);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u off",
			    rx_queue_id);
		return err;
	}

	rxq = dev->data->rx_queues[rx_queue_id];
	release_rxq_mbufs(rxq);
	reset_rx_queue(rxq);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

int
avf_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_tx_queue *txq;
	int err;

	PMD_DRV_FUNC_TRACE();

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	err = avf_switch_queue(adapter, tx_queue_id, FALSE, FALSE);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u off",
			    tx_queue_id);
		return err;
	}

	txq = dev->data->tx_queues[tx_queue_id];
	release_txq_mbufs(txq);
	reset_tx_queue(txq);
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

void
avf_dev_rx_queue_release(void *rxq)
{
	struct avf_rx_queue *q = (struct avf_rx_queue *)rxq;

	if (!q)
		return;

	release_rxq_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_free(q);
}

void
avf_dev_tx_queue_release(void *txq)
{
	struct avf_tx_queue *q = (struct avf_tx_queue *)txq;

	if (!q)
		return;

	release_txq_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_free(q);
}

void
avf_stop_queues(struct rte_eth_dev *dev)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_rx_queue *rxq;
	struct avf_tx_queue *txq;
	int ret, i;

	/* Stop All queues */
	ret = avf_disable_queues(adapter);
	if (ret)
		PMD_DRV_LOG(WARNING, "Fail to stop queues");

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (!txq)
			continue;
		release_txq_mbufs(txq);
		reset_tx_queue(txq);
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (!rxq)
			continue;
		release_rxq_mbufs(rxq);
		reset_rx_queue(rxq);
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
}
