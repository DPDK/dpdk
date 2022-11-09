/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <ethdev_driver.h>
#include <rte_net.h>
#include <rte_vect.h>

#include "idpf_ethdev.h"
#include "idpf_rxtx.h"
#include "idpf_rxtx_vec_common.h"

static int idpf_timestamp_dynfield_offset = -1;

static int
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

static int
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

static void
release_rxq_mbufs(struct idpf_rx_queue *rxq)
{
	uint16_t i;

	if (rxq->sw_ring == NULL)
		return;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		if (rxq->sw_ring[i] != NULL) {
			rte_pktmbuf_free_seg(rxq->sw_ring[i]);
			rxq->sw_ring[i] = NULL;
		}
	}
}

static void
release_txq_mbufs(struct idpf_tx_queue *txq)
{
	uint16_t nb_desc, i;

	if (txq == NULL || txq->sw_ring == NULL) {
		PMD_DRV_LOG(DEBUG, "Pointer to rxq or sw_ring is NULL");
		return;
	}

	if (txq->sw_nb_desc != 0) {
		/* For split queue model, descriptor ring */
		nb_desc = txq->sw_nb_desc;
	} else {
		/* For single queue model */
		nb_desc = txq->nb_tx_desc;
	}
	for (i = 0; i < nb_desc; i++) {
		if (txq->sw_ring[i].mbuf != NULL) {
			rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
			txq->sw_ring[i].mbuf = NULL;
		}
	}
}

static const struct idpf_rxq_ops def_rxq_ops = {
	.release_mbufs = release_rxq_mbufs,
};

static const struct idpf_txq_ops def_txq_ops = {
	.release_mbufs = release_txq_mbufs,
};

static void
reset_split_rx_descq(struct idpf_rx_queue *rxq)
{
	uint16_t len;
	uint32_t i;

	if (rxq == NULL)
		return;

	len = rxq->nb_rx_desc + IDPF_RX_MAX_BURST;

	for (i = 0; i < len * sizeof(struct virtchnl2_rx_flex_desc_adv_nic_3);
	     i++)
		((volatile char *)rxq->rx_ring)[i] = 0;

	rxq->rx_tail = 0;
	rxq->expected_gen_id = 1;
}

static void
reset_split_rx_bufq(struct idpf_rx_queue *rxq)
{
	uint16_t len;
	uint32_t i;

	if (rxq == NULL)
		return;

	len = rxq->nb_rx_desc + IDPF_RX_MAX_BURST;

	for (i = 0; i < len * sizeof(struct virtchnl2_splitq_rx_buf_desc);
	     i++)
		((volatile char *)rxq->rx_ring)[i] = 0;

	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));

	for (i = 0; i < IDPF_RX_MAX_BURST; i++)
		rxq->sw_ring[rxq->nb_rx_desc + i] = &rxq->fake_mbuf;

	/* The next descriptor id which can be received. */
	rxq->rx_next_avail = 0;

	/* The next descriptor id which can be refilled. */
	rxq->rx_tail = 0;
	/* The number of descriptors which can be refilled. */
	rxq->nb_rx_hold = rxq->nb_rx_desc - 1;

	rxq->bufq1 = NULL;
	rxq->bufq2 = NULL;
}

static void
idpf_rx_queue_release(void *rxq)
{
	struct idpf_rx_queue *q = rxq;

	if (q == NULL)
		return;

	/* Split queue */
	if (q->bufq1 != NULL && q->bufq2 != NULL) {
		q->bufq1->ops->release_mbufs(q->bufq1);
		rte_free(q->bufq1->sw_ring);
		rte_memzone_free(q->bufq1->mz);
		rte_free(q->bufq1);
		q->bufq2->ops->release_mbufs(q->bufq2);
		rte_free(q->bufq2->sw_ring);
		rte_memzone_free(q->bufq2->mz);
		rte_free(q->bufq2);
		rte_memzone_free(q->mz);
		rte_free(q);
		return;
	}

	/* Single queue */
	q->ops->release_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_free(q);
}

static void
idpf_tx_queue_release(void *txq)
{
	struct idpf_tx_queue *q = txq;

	if (q == NULL)
		return;

	if (q->complq) {
		rte_memzone_free(q->complq->mz);
		rte_free(q->complq);
	}

	q->ops->release_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_free(q);
}

static inline void
reset_split_rx_queue(struct idpf_rx_queue *rxq)
{
	reset_split_rx_descq(rxq);
	reset_split_rx_bufq(rxq->bufq1);
	reset_split_rx_bufq(rxq->bufq2);
}

static void
reset_single_rx_queue(struct idpf_rx_queue *rxq)
{
	uint16_t len;
	uint32_t i;

	if (rxq == NULL)
		return;

	len = rxq->nb_rx_desc + IDPF_RX_MAX_BURST;

	for (i = 0; i < len * sizeof(struct virtchnl2_singleq_rx_buf_desc);
	     i++)
		((volatile char *)rxq->rx_ring)[i] = 0;

	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));

	for (i = 0; i < IDPF_RX_MAX_BURST; i++)
		rxq->sw_ring[rxq->nb_rx_desc + i] = &rxq->fake_mbuf;

	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;

	rte_pktmbuf_free(rxq->pkt_first_seg);

	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
	rxq->rxrearm_start = 0;
	rxq->rxrearm_nb = 0;
}

static void
reset_split_tx_descq(struct idpf_tx_queue *txq)
{
	struct idpf_tx_entry *txe;
	uint32_t i, size;
	uint16_t prev;

	if (txq == NULL) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq is NULL");
		return;
	}

	size = sizeof(struct idpf_flex_tx_sched_desc) * txq->nb_tx_desc;
	for (i = 0; i < size; i++)
		((volatile char *)txq->desc_ring)[i] = 0;

	txe = txq->sw_ring;
	prev = (uint16_t)(txq->sw_nb_desc - 1);
	for (i = 0; i < txq->sw_nb_desc; i++) {
		txe[i].mbuf = NULL;
		txe[i].last_id = i;
		txe[prev].next_id = i;
		prev = i;
	}

	txq->tx_tail = 0;
	txq->nb_used = 0;

	/* Use this as next to clean for split desc queue */
	txq->last_desc_cleaned = 0;
	txq->sw_tail = 0;
	txq->nb_free = txq->nb_tx_desc - 1;
}

static void
reset_split_tx_complq(struct idpf_tx_queue *cq)
{
	uint32_t i, size;

	if (cq == NULL) {
		PMD_DRV_LOG(DEBUG, "Pointer to complq is NULL");
		return;
	}

	size = sizeof(struct idpf_splitq_tx_compl_desc) * cq->nb_tx_desc;
	for (i = 0; i < size; i++)
		((volatile char *)cq->compl_ring)[i] = 0;

	cq->tx_tail = 0;
	cq->expected_gen_id = 1;
}

static void
reset_single_tx_queue(struct idpf_tx_queue *txq)
{
	struct idpf_tx_entry *txe;
	uint32_t i, size;
	uint16_t prev;

	if (txq == NULL) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq is NULL");
		return;
	}

	txe = txq->sw_ring;
	size = sizeof(struct idpf_flex_tx_desc) * txq->nb_tx_desc;
	for (i = 0; i < size; i++)
		((volatile char *)txq->tx_ring)[i] = 0;

	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		txq->tx_ring[i].qw1.cmd_dtype =
			rte_cpu_to_le_16(IDPF_TX_DESC_DTYPE_DESC_DONE);
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
idpf_rx_split_bufq_setup(struct rte_eth_dev *dev, struct idpf_rx_queue *bufq,
			 uint16_t queue_idx, uint16_t rx_free_thresh,
			 uint16_t nb_desc, unsigned int socket_id,
			 struct rte_mempool *mp)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *adapter = vport->adapter;
	struct idpf_hw *hw = &adapter->hw;
	const struct rte_memzone *mz;
	uint32_t ring_size;
	uint16_t len;

	bufq->mp = mp;
	bufq->nb_rx_desc = nb_desc;
	bufq->rx_free_thresh = rx_free_thresh;
	bufq->queue_id = vport->chunks_info.rx_buf_start_qid + queue_idx;
	bufq->port_id = dev->data->port_id;
	bufq->rx_hdr_len = 0;
	bufq->adapter = adapter;

	len = rte_pktmbuf_data_room_size(bufq->mp) - RTE_PKTMBUF_HEADROOM;
	bufq->rx_buf_len = len;

	/* Allocate the software ring. */
	len = nb_desc + IDPF_RX_MAX_BURST;
	bufq->sw_ring =
		rte_zmalloc_socket("idpf rx bufq sw ring",
				   sizeof(struct rte_mbuf *) * len,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (bufq->sw_ring == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW ring");
		return -ENOMEM;
	}

	/* Allocate a liitle more to support bulk allocate. */
	len = nb_desc + IDPF_RX_MAX_BURST;
	ring_size = RTE_ALIGN(len *
			      sizeof(struct virtchnl2_splitq_rx_buf_desc),
			      IDPF_DMA_MEM_ALIGN);
	mz = rte_eth_dma_zone_reserve(dev, "rx_buf_ring", queue_idx,
				      ring_size, IDPF_RING_BASE_ALIGN,
				      socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for RX buffer queue.");
		rte_free(bufq->sw_ring);
		return -ENOMEM;
	}

	/* Zero all the descriptors in the ring. */
	memset(mz->addr, 0, ring_size);
	bufq->rx_ring_phys_addr = mz->iova;
	bufq->rx_ring = mz->addr;

	bufq->mz = mz;
	reset_split_rx_bufq(bufq);
	bufq->q_set = true;
	bufq->qrx_tail = hw->hw_addr + (vport->chunks_info.rx_buf_qtail_start +
			 queue_idx * vport->chunks_info.rx_buf_qtail_spacing);
	bufq->ops = &def_rxq_ops;

	/* TODO: allow bulk or vec */

	return 0;
}

static int
idpf_rx_split_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			  uint16_t nb_desc, unsigned int socket_id,
			  const struct rte_eth_rxconf *rx_conf,
			  struct rte_mempool *mp)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *adapter = vport->adapter;
	struct idpf_rx_queue *bufq1, *bufq2;
	const struct rte_memzone *mz;
	struct idpf_rx_queue *rxq;
	uint16_t rx_free_thresh;
	uint32_t ring_size;
	uint64_t offloads;
	uint16_t qid;
	uint16_t len;
	int ret;

	offloads = rx_conf->offloads | dev->data->dev_conf.rxmode.offloads;

	/* Check free threshold */
	rx_free_thresh = (rx_conf->rx_free_thresh == 0) ?
		IDPF_DEFAULT_RX_FREE_THRESH :
		rx_conf->rx_free_thresh;
	if (check_rx_thresh(nb_desc, rx_free_thresh) != 0)
		return -EINVAL;

	/* Free memory if needed */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		idpf_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Setup Rx description queue */
	rxq = rte_zmalloc_socket("idpf rxq",
				 sizeof(struct idpf_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (rxq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for rx queue data structure");
		return -ENOMEM;
	}

	rxq->mp = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_free_thresh;
	rxq->queue_id = vport->chunks_info.rx_start_qid + queue_idx;
	rxq->port_id = dev->data->port_id;
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;
	rxq->rx_hdr_len = 0;
	rxq->adapter = adapter;
	rxq->offloads = offloads;

	len = rte_pktmbuf_data_room_size(rxq->mp) - RTE_PKTMBUF_HEADROOM;
	rxq->rx_buf_len = len;

	len = rxq->nb_rx_desc + IDPF_RX_MAX_BURST;
	ring_size = RTE_ALIGN(len *
			      sizeof(struct virtchnl2_rx_flex_desc_adv_nic_3),
			      IDPF_DMA_MEM_ALIGN);
	mz = rte_eth_dma_zone_reserve(dev, "rx_cpmpl_ring", queue_idx,
				      ring_size, IDPF_RING_BASE_ALIGN,
				      socket_id);

	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for RX");
		ret = -ENOMEM;
		goto free_rxq;
	}

	/* Zero all the descriptors in the ring. */
	memset(mz->addr, 0, ring_size);
	rxq->rx_ring_phys_addr = mz->iova;
	rxq->rx_ring = mz->addr;

	rxq->mz = mz;
	reset_split_rx_descq(rxq);

	/* TODO: allow bulk or vec */

	/* setup Rx buffer queue */
	bufq1 = rte_zmalloc_socket("idpf bufq1",
				   sizeof(struct idpf_rx_queue),
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (bufq1 == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for rx buffer queue 1.");
		ret = -ENOMEM;
		goto free_mz;
	}
	qid = 2 * queue_idx;
	ret = idpf_rx_split_bufq_setup(dev, bufq1, qid, rx_free_thresh,
				       nb_desc, socket_id, mp);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to setup buffer queue 1");
		ret = -EINVAL;
		goto free_bufq1;
	}
	rxq->bufq1 = bufq1;

	bufq2 = rte_zmalloc_socket("idpf bufq2",
				   sizeof(struct idpf_rx_queue),
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (bufq2 == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for rx buffer queue 2.");
		rte_free(bufq1->sw_ring);
		rte_memzone_free(bufq1->mz);
		ret = -ENOMEM;
		goto free_bufq1;
	}
	qid = 2 * queue_idx + 1;
	ret = idpf_rx_split_bufq_setup(dev, bufq2, qid, rx_free_thresh,
				       nb_desc, socket_id, mp);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to setup buffer queue 2");
		rte_free(bufq1->sw_ring);
		rte_memzone_free(bufq1->mz);
		ret = -EINVAL;
		goto free_bufq2;
	}
	rxq->bufq2 = bufq2;

	rxq->q_set = true;
	dev->data->rx_queues[queue_idx] = rxq;

	return 0;

free_bufq2:
	rte_free(bufq2);
free_bufq1:
	rte_free(bufq1);
free_mz:
	rte_memzone_free(mz);
free_rxq:
	rte_free(rxq);

	return ret;
}

static int
idpf_rx_single_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			   uint16_t nb_desc, unsigned int socket_id,
			   const struct rte_eth_rxconf *rx_conf,
			   struct rte_mempool *mp)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *adapter = vport->adapter;
	struct idpf_hw *hw = &adapter->hw;
	const struct rte_memzone *mz;
	struct idpf_rx_queue *rxq;
	uint16_t rx_free_thresh;
	uint32_t ring_size;
	uint64_t offloads;
	uint16_t len;

	offloads = rx_conf->offloads | dev->data->dev_conf.rxmode.offloads;

	/* Check free threshold */
	rx_free_thresh = (rx_conf->rx_free_thresh == 0) ?
		IDPF_DEFAULT_RX_FREE_THRESH :
		rx_conf->rx_free_thresh;
	if (check_rx_thresh(nb_desc, rx_free_thresh) != 0)
		return -EINVAL;

	/* Free memory if needed */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		idpf_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Setup Rx description queue */
	rxq = rte_zmalloc_socket("idpf rxq",
				 sizeof(struct idpf_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (rxq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for rx queue data structure");
		return -ENOMEM;
	}

	rxq->mp = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_free_thresh;
	rxq->queue_id = vport->chunks_info.rx_start_qid + queue_idx;
	rxq->port_id = dev->data->port_id;
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;
	rxq->rx_hdr_len = 0;
	rxq->adapter = adapter;
	rxq->offloads = offloads;

	len = rte_pktmbuf_data_room_size(rxq->mp) - RTE_PKTMBUF_HEADROOM;
	rxq->rx_buf_len = len;

	len = nb_desc + IDPF_RX_MAX_BURST;
	rxq->sw_ring =
		rte_zmalloc_socket("idpf rxq sw ring",
				   sizeof(struct rte_mbuf *) * len,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (rxq->sw_ring == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW ring");
		rte_free(rxq);
		return -ENOMEM;
	}

	/* Allocate a liitle more to support bulk allocate. */
	len = nb_desc + IDPF_RX_MAX_BURST;
	ring_size = RTE_ALIGN(len *
			      sizeof(struct virtchnl2_singleq_rx_buf_desc),
			      IDPF_DMA_MEM_ALIGN);
	mz = rte_eth_dma_zone_reserve(dev, "rx ring", queue_idx,
				      ring_size, IDPF_RING_BASE_ALIGN,
				      socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for RX buffer queue.");
		rte_free(rxq->sw_ring);
		rte_free(rxq);
		return -ENOMEM;
	}

	/* Zero all the descriptors in the ring. */
	memset(mz->addr, 0, ring_size);
	rxq->rx_ring_phys_addr = mz->iova;
	rxq->rx_ring = mz->addr;

	rxq->mz = mz;
	reset_single_rx_queue(rxq);
	rxq->q_set = true;
	dev->data->rx_queues[queue_idx] = rxq;
	rxq->qrx_tail = hw->hw_addr + (vport->chunks_info.rx_qtail_start +
			queue_idx * vport->chunks_info.rx_qtail_spacing);
	rxq->ops = &def_rxq_ops;

	return 0;
}

int
idpf_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		    uint16_t nb_desc, unsigned int socket_id,
		    const struct rte_eth_rxconf *rx_conf,
		    struct rte_mempool *mp)
{
	struct idpf_vport *vport = dev->data->dev_private;

	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE)
		return idpf_rx_single_queue_setup(dev, queue_idx, nb_desc,
						  socket_id, rx_conf, mp);
	else
		return idpf_rx_split_queue_setup(dev, queue_idx, nb_desc,
						 socket_id, rx_conf, mp);
}

static int
idpf_tx_split_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			  uint16_t nb_desc, unsigned int socket_id,
			  const struct rte_eth_txconf *tx_conf)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *adapter = vport->adapter;
	uint16_t tx_rs_thresh, tx_free_thresh;
	struct idpf_hw *hw = &adapter->hw;
	struct idpf_tx_queue *txq, *cq;
	const struct rte_memzone *mz;
	uint32_t ring_size;
	uint64_t offloads;
	int ret;

	offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;

	tx_rs_thresh = (uint16_t)((tx_conf->tx_rs_thresh != 0) ?
		tx_conf->tx_rs_thresh : IDPF_DEFAULT_TX_RS_THRESH);
	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh != 0) ?
		tx_conf->tx_free_thresh : IDPF_DEFAULT_TX_FREE_THRESH);
	if (check_tx_thresh(nb_desc, tx_rs_thresh, tx_free_thresh) != 0)
		return -EINVAL;

	/* Free memory if needed. */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		idpf_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket("idpf split txq",
				 sizeof(struct idpf_tx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (txq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for tx queue structure");
		return -ENOMEM;
	}

	txq->nb_tx_desc = nb_desc;
	txq->rs_thresh = tx_rs_thresh;
	txq->free_thresh = tx_free_thresh;
	txq->queue_id = vport->chunks_info.tx_start_qid + queue_idx;
	txq->port_id = dev->data->port_id;
	txq->offloads = offloads;
	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	/* Allocate software ring */
	txq->sw_nb_desc = 2 * nb_desc;
	txq->sw_ring =
		rte_zmalloc_socket("idpf split tx sw ring",
				   sizeof(struct idpf_tx_entry) *
				   txq->sw_nb_desc,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (txq->sw_ring == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW TX ring");
		ret = -ENOMEM;
		goto err_txq_sw_ring;
	}

	/* Allocate TX hardware ring descriptors. */
	ring_size = sizeof(struct idpf_flex_tx_sched_desc) * txq->nb_tx_desc;
	ring_size = RTE_ALIGN(ring_size, IDPF_DMA_MEM_ALIGN);
	mz = rte_eth_dma_zone_reserve(dev, "split_tx_ring", queue_idx,
				      ring_size, IDPF_RING_BASE_ALIGN,
				      socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for TX");
		ret = -ENOMEM;
		goto err_txq_mz;
	}
	txq->tx_ring_phys_addr = mz->iova;
	txq->desc_ring = mz->addr;

	txq->mz = mz;
	reset_split_tx_descq(txq);
	txq->qtx_tail = hw->hw_addr + (vport->chunks_info.tx_qtail_start +
			queue_idx * vport->chunks_info.tx_qtail_spacing);
	txq->ops = &def_txq_ops;

	/* Allocate the TX completion queue data structure. */
	txq->complq = rte_zmalloc_socket("idpf splitq cq",
					 sizeof(struct idpf_tx_queue),
					 RTE_CACHE_LINE_SIZE,
					 socket_id);
	cq = txq->complq;
	if (cq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for tx queue structure");
		ret = -ENOMEM;
		goto err_cq;
	}
	cq->nb_tx_desc = 2 * nb_desc;
	cq->queue_id = vport->chunks_info.tx_compl_start_qid + queue_idx;
	cq->port_id = dev->data->port_id;
	cq->txqs = dev->data->tx_queues;
	cq->tx_start_qid = vport->chunks_info.tx_start_qid;

	ring_size = sizeof(struct idpf_splitq_tx_compl_desc) * cq->nb_tx_desc;
	ring_size = RTE_ALIGN(ring_size, IDPF_DMA_MEM_ALIGN);
	mz = rte_eth_dma_zone_reserve(dev, "tx_split_compl_ring", queue_idx,
				      ring_size, IDPF_RING_BASE_ALIGN,
				      socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for TX completion queue");
		ret = -ENOMEM;
		goto err_cq_mz;
	}
	cq->tx_ring_phys_addr = mz->iova;
	cq->compl_ring = mz->addr;
	cq->mz = mz;
	reset_split_tx_complq(cq);

	txq->q_set = true;
	dev->data->tx_queues[queue_idx] = txq;

	return 0;

err_cq_mz:
	rte_free(cq);
err_cq:
	rte_memzone_free(txq->mz);
err_txq_mz:
	rte_free(txq->sw_ring);
err_txq_sw_ring:
	rte_free(txq);

	return ret;
}

static int
idpf_tx_single_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			   uint16_t nb_desc, unsigned int socket_id,
			   const struct rte_eth_txconf *tx_conf)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *adapter = vport->adapter;
	uint16_t tx_rs_thresh, tx_free_thresh;
	struct idpf_hw *hw = &adapter->hw;
	const struct rte_memzone *mz;
	struct idpf_tx_queue *txq;
	uint32_t ring_size;
	uint64_t offloads;

	offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;

	tx_rs_thresh = (uint16_t)((tx_conf->tx_rs_thresh > 0) ?
		tx_conf->tx_rs_thresh : IDPF_DEFAULT_TX_RS_THRESH);
	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh > 0) ?
		tx_conf->tx_free_thresh : IDPF_DEFAULT_TX_FREE_THRESH);
	if (check_tx_thresh(nb_desc, tx_rs_thresh, tx_free_thresh) != 0)
		return -EINVAL;

	/* Free memory if needed. */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		idpf_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket("idpf txq",
				 sizeof(struct idpf_tx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (txq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for tx queue structure");
		return -ENOMEM;
	}

	/* TODO: vlan offload */

	txq->nb_tx_desc = nb_desc;
	txq->rs_thresh = tx_rs_thresh;
	txq->free_thresh = tx_free_thresh;
	txq->queue_id = vport->chunks_info.tx_start_qid + queue_idx;
	txq->port_id = dev->data->port_id;
	txq->offloads = offloads;
	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	/* Allocate software ring */
	txq->sw_ring =
		rte_zmalloc_socket("idpf tx sw ring",
				   sizeof(struct idpf_tx_entry) * nb_desc,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (txq->sw_ring == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW TX ring");
		rte_free(txq);
		return -ENOMEM;
	}

	/* Allocate TX hardware ring descriptors. */
	ring_size = sizeof(struct idpf_flex_tx_desc) * nb_desc;
	ring_size = RTE_ALIGN(ring_size, IDPF_DMA_MEM_ALIGN);
	mz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx,
				      ring_size, IDPF_RING_BASE_ALIGN,
				      socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for TX");
		rte_free(txq->sw_ring);
		rte_free(txq);
		return -ENOMEM;
	}

	txq->tx_ring_phys_addr = mz->iova;
	txq->tx_ring = mz->addr;

	txq->mz = mz;
	reset_single_tx_queue(txq);
	txq->q_set = true;
	dev->data->tx_queues[queue_idx] = txq;
	txq->qtx_tail = hw->hw_addr + (vport->chunks_info.tx_qtail_start +
			queue_idx * vport->chunks_info.tx_qtail_spacing);
	txq->ops = &def_txq_ops;

	return 0;
}

int
idpf_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		    uint16_t nb_desc, unsigned int socket_id,
		    const struct rte_eth_txconf *tx_conf)
{
	struct idpf_vport *vport = dev->data->dev_private;

	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE)
		return idpf_tx_single_queue_setup(dev, queue_idx, nb_desc,
						  socket_id, tx_conf);
	else
		return idpf_tx_split_queue_setup(dev, queue_idx, nb_desc,
						 socket_id, tx_conf);
}

static int
idpf_register_ts_mbuf(struct idpf_rx_queue *rxq)
{
	int err;
	if ((rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) != 0) {
		/* Register mbuf field and flag for Rx timestamp */
		err = rte_mbuf_dyn_rx_timestamp_register(&idpf_timestamp_dynfield_offset,
							 &idpf_timestamp_dynflag);
		if (err != 0) {
			PMD_DRV_LOG(ERR,
				"Cannot register mbuf field/flag for timestamp");
			return -EINVAL;
		}
	}
	return 0;
}

static int
idpf_alloc_single_rxq_mbufs(struct idpf_rx_queue *rxq)
{
	volatile struct virtchnl2_singleq_rx_buf_desc *rxd;
	struct rte_mbuf *mbuf = NULL;
	uint64_t dma_addr;
	uint16_t i;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		mbuf = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(mbuf == NULL)) {
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

		rxd = &((volatile struct virtchnl2_singleq_rx_buf_desc *)(rxq->rx_ring))[i];
		rxd->pkt_addr = dma_addr;
		rxd->hdr_addr = 0;
		rxd->rsvd1 = 0;
		rxd->rsvd2 = 0;
		rxq->sw_ring[i] = mbuf;
	}

	return 0;
}

static int
idpf_alloc_split_rxq_mbufs(struct idpf_rx_queue *rxq)
{
	volatile struct virtchnl2_splitq_rx_buf_desc *rxd;
	struct rte_mbuf *mbuf = NULL;
	uint64_t dma_addr;
	uint16_t i;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		mbuf = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(mbuf == NULL)) {
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

		rxd = &((volatile struct virtchnl2_splitq_rx_buf_desc *)(rxq->rx_ring))[i];
		rxd->qword0.buf_id = i;
		rxd->qword0.rsvd0 = 0;
		rxd->qword0.rsvd1 = 0;
		rxd->pkt_addr = dma_addr;
		rxd->hdr_addr = 0;
		rxd->rsvd2 = 0;

		rxq->sw_ring[i] = mbuf;
	}

	rxq->nb_rx_hold = 0;
	rxq->rx_tail = rxq->nb_rx_desc - 1;

	return 0;
}

int
idpf_rx_queue_init(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct idpf_rx_queue *rxq;
	int err;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	rxq = dev->data->rx_queues[rx_queue_id];

	if (rxq == NULL || !rxq->q_set) {
		PMD_DRV_LOG(ERR, "RX queue %u not available or setup",
					rx_queue_id);
		return -EINVAL;
	}

	err = idpf_register_ts_mbuf(rxq);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "fail to regidter timestamp mbuf %u",
					rx_queue_id);
		return -EIO;
	}

	if (rxq->bufq1 == NULL) {
		/* Single queue */
		err = idpf_alloc_single_rxq_mbufs(rxq);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to allocate RX queue mbuf");
			return err;
		}

		rte_wmb();

		/* Init the RX tail register. */
		IDPF_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);
	} else {
		/* Split queue */
		err = idpf_alloc_split_rxq_mbufs(rxq->bufq1);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to allocate RX buffer queue mbuf");
			return err;
		}
		err = idpf_alloc_split_rxq_mbufs(rxq->bufq2);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to allocate RX buffer queue mbuf");
			return err;
		}

		rte_wmb();

		/* Init the RX tail register. */
		IDPF_PCI_REG_WRITE(rxq->bufq1->qrx_tail, rxq->bufq1->rx_tail);
		IDPF_PCI_REG_WRITE(rxq->bufq2->qrx_tail, rxq->bufq2->rx_tail);
	}

	return err;
}

int
idpf_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_rx_queue *rxq =
		dev->data->rx_queues[rx_queue_id];
	int err = 0;

	err = idpf_vc_config_rxq(vport, rx_queue_id);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Fail to configure Rx queue %u", rx_queue_id);
		return err;
	}

	err = idpf_rx_queue_init(dev, rx_queue_id);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to init RX queue %u",
			    rx_queue_id);
		return err;
	}

	/* Ready to switch the queue on */
	err = idpf_switch_queue(vport, rx_queue_id, true, true);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u on",
			    rx_queue_id);
	} else {
		rxq->q_started = true;
		dev->data->rx_queue_state[rx_queue_id] =
			RTE_ETH_QUEUE_STATE_STARTED;
	}

	return err;
}

int
idpf_tx_queue_init(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct idpf_tx_queue *txq;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	txq = dev->data->tx_queues[tx_queue_id];

	/* Init the RX tail register. */
	IDPF_PCI_REG_WRITE(txq->qtx_tail, 0);

	return 0;
}

int
idpf_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_tx_queue *txq =
		dev->data->tx_queues[tx_queue_id];
	int err = 0;

	err = idpf_vc_config_txq(vport, tx_queue_id);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Fail to configure Tx queue %u", tx_queue_id);
		return err;
	}

	err = idpf_tx_queue_init(dev, tx_queue_id);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to init TX queue %u",
			    tx_queue_id);
		return err;
	}

	/* Ready to switch the queue on */
	err = idpf_switch_queue(vport, tx_queue_id, false, true);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u on",
			    tx_queue_id);
	} else {
		txq->q_started = true;
		dev->data->tx_queue_state[tx_queue_id] =
			RTE_ETH_QUEUE_STATE_STARTED;
	}

	return err;
}

int
idpf_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_rx_queue *rxq;
	int err;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	err = idpf_switch_queue(vport, rx_queue_id, true, false);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u off",
			    rx_queue_id);
		return err;
	}

	rxq = dev->data->rx_queues[rx_queue_id];
	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		rxq->ops->release_mbufs(rxq);
		reset_single_rx_queue(rxq);
	} else {
		rxq->bufq1->ops->release_mbufs(rxq->bufq1);
		rxq->bufq2->ops->release_mbufs(rxq->bufq2);
		reset_split_rx_queue(rxq);
	}
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

int
idpf_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_tx_queue *txq;
	int err;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	err = idpf_switch_queue(vport, tx_queue_id, false, false);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u off",
			    tx_queue_id);
		return err;
	}

	txq = dev->data->tx_queues[tx_queue_id];
	txq->ops->release_mbufs(txq);
	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		reset_single_tx_queue(txq);
	} else {
		reset_split_tx_descq(txq);
		reset_split_tx_complq(txq->complq);
	}
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

void
idpf_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	idpf_rx_queue_release(dev->data->rx_queues[qid]);
}

void
idpf_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	idpf_tx_queue_release(dev->data->tx_queues[qid]);
}

void
idpf_stop_queues(struct rte_eth_dev *dev)
{
	struct idpf_rx_queue *rxq;
	struct idpf_tx_queue *txq;
	int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq == NULL)
			continue;

		if (idpf_rx_queue_stop(dev, i) != 0)
			PMD_DRV_LOG(WARNING, "Fail to stop Rx queue %d", i);
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq == NULL)
			continue;

		if (idpf_tx_queue_stop(dev, i) != 0)
			PMD_DRV_LOG(WARNING, "Fail to stop Tx queue %d", i);
	}
}

#define IDPF_RX_FLEX_DESC_ADV_STATUS0_XSUM_S				\
	(RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_IPE_S) |     \
	 RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_L4E_S) |     \
	 RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_EIPE_S) |    \
	 RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_EUDPE_S))

static inline uint64_t
idpf_splitq_rx_csum_offload(uint8_t err)
{
	uint64_t flags = 0;

	if (unlikely((err & RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_L3L4P_S)) == 0))
		return flags;

	if (likely((err & IDPF_RX_FLEX_DESC_ADV_STATUS0_XSUM_S) == 0)) {
		flags |= (RTE_MBUF_F_RX_IP_CKSUM_GOOD |
			  RTE_MBUF_F_RX_L4_CKSUM_GOOD);
		return flags;
	}

	if (unlikely((err & RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_IPE_S)) != 0))
		flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
	else
		flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;

	if (unlikely((err & RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_L4E_S)) != 0))
		flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
	else
		flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;

	if (unlikely((err & RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_EIPE_S)) != 0))
		flags |= RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD;

	if (unlikely((err & RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_XSUM_EUDPE_S)) != 0))
		flags |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD;
	else
		flags |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD;

	return flags;
}

#define IDPF_RX_FLEX_DESC_ADV_HASH1_S  0
#define IDPF_RX_FLEX_DESC_ADV_HASH2_S  16
#define IDPF_RX_FLEX_DESC_ADV_HASH3_S  24

static inline uint64_t
idpf_splitq_rx_rss_offload(struct rte_mbuf *mb,
			   volatile struct virtchnl2_rx_flex_desc_adv_nic_3 *rx_desc)
{
	uint8_t status_err0_qw0;
	uint64_t flags = 0;

	status_err0_qw0 = rx_desc->status_err0_qw0;

	if ((status_err0_qw0 & RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_ADV_STATUS0_RSS_VALID_S)) != 0) {
		flags |= RTE_MBUF_F_RX_RSS_HASH;
		mb->hash.rss = (rte_le_to_cpu_16(rx_desc->hash1) <<
				IDPF_RX_FLEX_DESC_ADV_HASH1_S) |
			((uint32_t)(rx_desc->ff2_mirrid_hash2.hash2) <<
			 IDPF_RX_FLEX_DESC_ADV_HASH2_S) |
			((uint32_t)(rx_desc->hash3) <<
			 IDPF_RX_FLEX_DESC_ADV_HASH3_S);
	}

	return flags;
}

static void
idpf_split_rx_bufq_refill(struct idpf_rx_queue *rx_bufq)
{
	volatile struct virtchnl2_splitq_rx_buf_desc *rx_buf_ring;
	volatile struct virtchnl2_splitq_rx_buf_desc *rx_buf_desc;
	uint16_t nb_refill = rx_bufq->rx_free_thresh;
	uint16_t nb_desc = rx_bufq->nb_rx_desc;
	uint16_t next_avail = rx_bufq->rx_tail;
	struct rte_mbuf *nmb[rx_bufq->rx_free_thresh];
	struct rte_eth_dev *dev;
	uint64_t dma_addr;
	uint16_t delta;
	int i;

	if (rx_bufq->nb_rx_hold < rx_bufq->rx_free_thresh)
		return;

	rx_buf_ring = rx_bufq->rx_ring;
	delta = nb_desc - next_avail;
	if (unlikely(delta < nb_refill)) {
		if (likely(rte_pktmbuf_alloc_bulk(rx_bufq->mp, nmb, delta) == 0)) {
			for (i = 0; i < delta; i++) {
				rx_buf_desc = &rx_buf_ring[next_avail + i];
				rx_bufq->sw_ring[next_avail + i] = nmb[i];
				dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb[i]));
				rx_buf_desc->hdr_addr = 0;
				rx_buf_desc->pkt_addr = dma_addr;
			}
			nb_refill -= delta;
			next_avail = 0;
			rx_bufq->nb_rx_hold -= delta;
		} else {
			dev = &rte_eth_devices[rx_bufq->port_id];
			dev->data->rx_mbuf_alloc_failed += nb_desc - next_avail;
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u queue_id=%u",
				   rx_bufq->port_id, rx_bufq->queue_id);
			return;
		}
	}

	if (nb_desc - next_avail >= nb_refill) {
		if (likely(rte_pktmbuf_alloc_bulk(rx_bufq->mp, nmb, nb_refill) == 0)) {
			for (i = 0; i < nb_refill; i++) {
				rx_buf_desc = &rx_buf_ring[next_avail + i];
				rx_bufq->sw_ring[next_avail + i] = nmb[i];
				dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb[i]));
				rx_buf_desc->hdr_addr = 0;
				rx_buf_desc->pkt_addr = dma_addr;
			}
			next_avail += nb_refill;
			rx_bufq->nb_rx_hold -= nb_refill;
		} else {
			dev = &rte_eth_devices[rx_bufq->port_id];
			dev->data->rx_mbuf_alloc_failed += nb_desc - next_avail;
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u queue_id=%u",
				   rx_bufq->port_id, rx_bufq->queue_id);
		}
	}

	IDPF_PCI_REG_WRITE(rx_bufq->qrx_tail, next_avail);

	rx_bufq->rx_tail = next_avail;
}

uint16_t
idpf_splitq_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		      uint16_t nb_pkts)
{
	volatile struct virtchnl2_rx_flex_desc_adv_nic_3 *rx_desc_ring;
	volatile struct virtchnl2_rx_flex_desc_adv_nic_3 *rx_desc;
	uint16_t pktlen_gen_bufq_id;
	struct idpf_rx_queue *rxq;
	const uint32_t *ptype_tbl;
	uint8_t status_err0_qw1;
	struct idpf_adapter *ad;
	struct rte_mbuf *rxm;
	uint16_t rx_id_bufq1;
	uint16_t rx_id_bufq2;
	uint64_t pkt_flags;
	uint16_t pkt_len;
	uint16_t bufq_id;
	uint16_t gen_id;
	uint16_t rx_id;
	uint16_t nb_rx;
	uint64_t ts_ns;

	nb_rx = 0;
	rxq = rx_queue;
	ad = rxq->adapter;

	if (unlikely(rxq == NULL) || unlikely(!rxq->q_started))
		return nb_rx;

	rx_id = rxq->rx_tail;
	rx_id_bufq1 = rxq->bufq1->rx_next_avail;
	rx_id_bufq2 = rxq->bufq2->rx_next_avail;
	rx_desc_ring = rxq->rx_ring;
	ptype_tbl = rxq->adapter->ptype_tbl;

	if ((rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) != 0)
		rxq->hw_register_set = 1;

	while (nb_rx < nb_pkts) {
		rx_desc = &rx_desc_ring[rx_id];

		pktlen_gen_bufq_id =
			rte_le_to_cpu_16(rx_desc->pktlen_gen_bufq_id);
		gen_id = (pktlen_gen_bufq_id &
			  VIRTCHNL2_RX_FLEX_DESC_ADV_GEN_M) >>
			VIRTCHNL2_RX_FLEX_DESC_ADV_GEN_S;
		if (gen_id != rxq->expected_gen_id)
			break;

		pkt_len = (pktlen_gen_bufq_id &
			   VIRTCHNL2_RX_FLEX_DESC_ADV_LEN_PBUF_M) >>
			VIRTCHNL2_RX_FLEX_DESC_ADV_LEN_PBUF_S;
		if (pkt_len == 0)
			PMD_RX_LOG(ERR, "Packet length is 0");

		rx_id++;
		if (unlikely(rx_id == rxq->nb_rx_desc)) {
			rx_id = 0;
			rxq->expected_gen_id ^= 1;
		}

		bufq_id = (pktlen_gen_bufq_id &
			   VIRTCHNL2_RX_FLEX_DESC_ADV_BUFQ_ID_M) >>
			VIRTCHNL2_RX_FLEX_DESC_ADV_BUFQ_ID_S;
		if (bufq_id == 0) {
			rxm = rxq->bufq1->sw_ring[rx_id_bufq1];
			rx_id_bufq1++;
			if (unlikely(rx_id_bufq1 == rxq->bufq1->nb_rx_desc))
				rx_id_bufq1 = 0;
			rxq->bufq1->nb_rx_hold++;
		} else {
			rxm = rxq->bufq2->sw_ring[rx_id_bufq2];
			rx_id_bufq2++;
			if (unlikely(rx_id_bufq2 == rxq->bufq2->nb_rx_desc))
				rx_id_bufq2 = 0;
			rxq->bufq2->nb_rx_hold++;
		}

		rxm->pkt_len = pkt_len;
		rxm->data_len = pkt_len;
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->next = NULL;
		rxm->nb_segs = 1;
		rxm->port = rxq->port_id;
		rxm->ol_flags = 0;
		rxm->packet_type =
			ptype_tbl[(rte_le_to_cpu_16(rx_desc->ptype_err_fflags0) &
				   VIRTCHNL2_RX_FLEX_DESC_ADV_PTYPE_M) >>
				  VIRTCHNL2_RX_FLEX_DESC_ADV_PTYPE_S];

		status_err0_qw1 = rx_desc->status_err0_qw1;
		pkt_flags = idpf_splitq_rx_csum_offload(status_err0_qw1);
		pkt_flags |= idpf_splitq_rx_rss_offload(rxm, rx_desc);
		if (idpf_timestamp_dynflag > 0 &&
		    (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)) {
			/* timestamp */
			ts_ns = idpf_tstamp_convert_32b_64b(ad,
				rxq->hw_register_set,
				rte_le_to_cpu_32(rx_desc->ts_high));
			rxq->hw_register_set = 0;
			*RTE_MBUF_DYNFIELD(rxm,
					   idpf_timestamp_dynfield_offset,
					   rte_mbuf_timestamp_t *) = ts_ns;
			rxm->ol_flags |= idpf_timestamp_dynflag;
		}

		rxm->ol_flags |= pkt_flags;

		rx_pkts[nb_rx++] = rxm;
	}

	if (nb_rx > 0) {
		rxq->rx_tail = rx_id;
		if (rx_id_bufq1 != rxq->bufq1->rx_next_avail)
			rxq->bufq1->rx_next_avail = rx_id_bufq1;
		if (rx_id_bufq2 != rxq->bufq2->rx_next_avail)
			rxq->bufq2->rx_next_avail = rx_id_bufq2;

		idpf_split_rx_bufq_refill(rxq->bufq1);
		idpf_split_rx_bufq_refill(rxq->bufq2);
	}

	return nb_rx;
}

static inline void
idpf_split_tx_free(struct idpf_tx_queue *cq)
{
	volatile struct idpf_splitq_tx_compl_desc *compl_ring = cq->compl_ring;
	volatile struct idpf_splitq_tx_compl_desc *txd;
	uint16_t next = cq->tx_tail;
	struct idpf_tx_entry *txe;
	struct idpf_tx_queue *txq;
	uint16_t gen, qid, q_head;
	uint8_t ctype;

	txd = &compl_ring[next];
	gen = (rte_le_to_cpu_16(txd->qid_comptype_gen) &
		IDPF_TXD_COMPLQ_GEN_M) >> IDPF_TXD_COMPLQ_GEN_S;
	if (gen != cq->expected_gen_id)
		return;

	ctype = (rte_le_to_cpu_16(txd->qid_comptype_gen) &
		IDPF_TXD_COMPLQ_COMPL_TYPE_M) >> IDPF_TXD_COMPLQ_COMPL_TYPE_S;
	qid = (rte_le_to_cpu_16(txd->qid_comptype_gen) &
		IDPF_TXD_COMPLQ_QID_M) >> IDPF_TXD_COMPLQ_QID_S;
	q_head = rte_le_to_cpu_16(txd->q_head_compl_tag.compl_tag);
	txq = cq->txqs[qid - cq->tx_start_qid];

	switch (ctype) {
	case IDPF_TXD_COMPLT_RE:
		if (q_head == 0)
			txq->last_desc_cleaned = txq->nb_tx_desc - 1;
		else
			txq->last_desc_cleaned = q_head - 1;
		if (unlikely((txq->last_desc_cleaned % 32) == 0)) {
			PMD_DRV_LOG(ERR, "unexpected desc (head = %u) completion.",
						q_head);
			return;
		}

		break;
	case IDPF_TXD_COMPLT_RS:
		txq->nb_free++;
		txq->nb_used--;
		txe = &txq->sw_ring[q_head];
		if (txe->mbuf != NULL) {
			rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = NULL;
		}
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown completion type.");
		return;
	}

	if (++next == cq->nb_tx_desc) {
		next = 0;
		cq->expected_gen_id ^= 1;
	}

	cq->tx_tail = next;
}

/* Check if the context descriptor is needed for TX offloading */
static inline uint16_t
idpf_calc_context_desc(uint64_t flags)
{
	if ((flags & RTE_MBUF_F_TX_TCP_SEG) != 0)
		return 1;

	return 0;
}

/* set TSO context descriptor
 */
static inline void
idpf_set_splitq_tso_ctx(struct rte_mbuf *mbuf,
			union idpf_tx_offload tx_offload,
			volatile union idpf_flex_tx_ctx_desc *ctx_desc)
{
	uint16_t cmd_dtype;
	uint32_t tso_len;
	uint8_t hdr_len;

	if (tx_offload.l4_len == 0) {
		PMD_TX_LOG(DEBUG, "L4 length set to 0");
		return;
	}

	hdr_len = tx_offload.l2_len +
		tx_offload.l3_len +
		tx_offload.l4_len;
	cmd_dtype = IDPF_TX_DESC_DTYPE_FLEX_TSO_CTX |
		IDPF_TX_FLEX_CTX_DESC_CMD_TSO;
	tso_len = mbuf->pkt_len - hdr_len;

	ctx_desc->tso.qw1.cmd_dtype = rte_cpu_to_le_16(cmd_dtype);
	ctx_desc->tso.qw0.hdr_len = hdr_len;
	ctx_desc->tso.qw0.mss_rt =
		rte_cpu_to_le_16((uint16_t)mbuf->tso_segsz &
				 IDPF_TXD_FLEX_CTX_MSS_RT_M);
	ctx_desc->tso.qw0.flex_tlen =
		rte_cpu_to_le_32(tso_len &
				 IDPF_TXD_FLEX_CTX_MSS_RT_M);
}

uint16_t
idpf_splitq_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		      uint16_t nb_pkts)
{
	struct idpf_tx_queue *txq = (struct idpf_tx_queue *)tx_queue;
	volatile struct idpf_flex_tx_sched_desc *txr;
	volatile struct idpf_flex_tx_sched_desc *txd;
	struct idpf_tx_entry *sw_ring;
	union idpf_tx_offload tx_offload = {0};
	struct idpf_tx_entry *txe, *txn;
	uint16_t nb_used, tx_id, sw_id;
	struct rte_mbuf *tx_pkt;
	uint16_t nb_to_clean;
	uint16_t nb_tx = 0;
	uint64_t ol_flags;
	uint16_t nb_ctx;

	if (unlikely(txq == NULL) || unlikely(!txq->q_started))
		return nb_tx;

	txr = txq->desc_ring;
	sw_ring = txq->sw_ring;
	tx_id = txq->tx_tail;
	sw_id = txq->sw_tail;
	txe = &sw_ring[sw_id];

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		tx_pkt = tx_pkts[nb_tx];

		if (txq->nb_free <= txq->free_thresh) {
			/* TODO: Need to refine
			 * 1. free and clean: Better to decide a clean destination instead of
			 * loop times. And don't free mbuf when RS got immediately, free when
			 * transmit or according to the clean destination.
			 * Now, just ignore the RE write back, free mbuf when get RS
			 * 2. out-of-order rewrite back haven't be supported, SW head and HW head
			 * need to be separated.
			 **/
			nb_to_clean = 2 * txq->rs_thresh;
			while (nb_to_clean--)
				idpf_split_tx_free(txq->complq);
		}

		if (txq->nb_free < tx_pkt->nb_segs)
			break;

		ol_flags = tx_pkt->ol_flags;
		tx_offload.l2_len = tx_pkt->l2_len;
		tx_offload.l3_len = tx_pkt->l3_len;
		tx_offload.l4_len = tx_pkt->l4_len;
		tx_offload.tso_segsz = tx_pkt->tso_segsz;
		/* Calculate the number of context descriptors needed. */
		nb_ctx = idpf_calc_context_desc(ol_flags);
		nb_used = tx_pkt->nb_segs + nb_ctx;

		/* context descriptor */
		if (nb_ctx != 0) {
			volatile union idpf_flex_tx_ctx_desc *ctx_desc =
			(volatile union idpf_flex_tx_ctx_desc *)&txr[tx_id];

			if ((ol_flags & RTE_MBUF_F_TX_TCP_SEG) != 0)
				idpf_set_splitq_tso_ctx(tx_pkt, tx_offload,
							ctx_desc);

			tx_id++;
			if (tx_id == txq->nb_tx_desc)
				tx_id = 0;
		}

		do {
			txd = &txr[tx_id];
			txn = &sw_ring[txe->next_id];
			txe->mbuf = tx_pkt;

			/* Setup TX descriptor */
			txd->buf_addr =
				rte_cpu_to_le_64(rte_mbuf_data_iova(tx_pkt));
			txd->qw1.cmd_dtype =
				rte_cpu_to_le_16(IDPF_TX_DESC_DTYPE_FLEX_FLOW_SCHE);
			txd->qw1.rxr_bufsize = tx_pkt->data_len;
			txd->qw1.compl_tag = sw_id;
			tx_id++;
			if (tx_id == txq->nb_tx_desc)
				tx_id = 0;
			sw_id = txe->next_id;
			txe = txn;
			tx_pkt = tx_pkt->next;
		} while (tx_pkt);

		/* fill the last descriptor with End of Packet (EOP) bit */
		txd->qw1.cmd_dtype |= IDPF_TXD_FLEX_FLOW_CMD_EOP;

		if (unlikely((tx_id % 32) == 0))
			txd->qw1.cmd_dtype |= IDPF_TXD_FLEX_FLOW_CMD_RE;
		if (ol_flags & IDPF_TX_CKSUM_OFFLOAD_MASK)
			txd->qw1.cmd_dtype |= IDPF_TXD_FLEX_FLOW_CMD_CS_EN;
		txq->nb_free = (uint16_t)(txq->nb_free - nb_used);
		txq->nb_used = (uint16_t)(txq->nb_used + nb_used);
	}

	/* update the tail pointer if any packets were processed */
	if (likely(nb_tx > 0)) {
		IDPF_PCI_REG_WRITE(txq->qtx_tail, tx_id);
		txq->tx_tail = tx_id;
		txq->sw_tail = sw_id;
	}

	return nb_tx;
}

#define IDPF_RX_FLEX_DESC_STATUS0_XSUM_S				\
	(RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_IPE_S) |		\
	 RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_L4E_S) |		\
	 RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_EIPE_S) |	\
	 RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_EUDPE_S))

/* Translate the rx descriptor status and error fields to pkt flags */
static inline uint64_t
idpf_rxd_to_pkt_flags(uint16_t status_error)
{
	uint64_t flags = 0;

	if (unlikely((status_error & RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_STATUS0_L3L4P_S)) == 0))
		return flags;

	if (likely((status_error & IDPF_RX_FLEX_DESC_STATUS0_XSUM_S) == 0)) {
		flags |= (RTE_MBUF_F_RX_IP_CKSUM_GOOD |
			  RTE_MBUF_F_RX_L4_CKSUM_GOOD);
		return flags;
	}

	if (unlikely((status_error & RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_IPE_S)) != 0))
		flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
	else
		flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;

	if (unlikely((status_error & RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_L4E_S)) != 0))
		flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
	else
		flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;

	if (unlikely((status_error & RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_EIPE_S)) != 0))
		flags |= RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD;

	if (unlikely((status_error & RTE_BIT32(VIRTCHNL2_RX_FLEX_DESC_STATUS0_XSUM_EUDPE_S)) != 0))
		flags |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD;
	else
		flags |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD;

	return flags;
}

static inline void
idpf_update_rx_tail(struct idpf_rx_queue *rxq, uint16_t nb_hold,
		    uint16_t rx_id)
{
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);

	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG,
			   "port_id=%u queue_id=%u rx_tail=%u nb_hold=%u",
			   rxq->port_id, rxq->queue_id, rx_id, nb_hold);
		rx_id = (uint16_t)((rx_id == 0) ?
				   (rxq->nb_rx_desc - 1) : (rx_id - 1));
		IDPF_PCI_REG_WRITE(rxq->qrx_tail, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;
}

uint16_t
idpf_singleq_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		       uint16_t nb_pkts)
{
	volatile union virtchnl2_rx_desc *rx_ring;
	volatile union virtchnl2_rx_desc *rxdp;
	union virtchnl2_rx_desc rxd;
	struct idpf_rx_queue *rxq;
	const uint32_t *ptype_tbl;
	uint16_t rx_id, nb_hold;
	struct rte_eth_dev *dev;
	struct idpf_adapter *ad;
	uint16_t rx_packet_len;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	uint16_t rx_status0;
	uint64_t pkt_flags;
	uint64_t dma_addr;
	uint64_t ts_ns;
	uint16_t nb_rx;

	nb_rx = 0;
	nb_hold = 0;
	rxq = rx_queue;

	ad = rxq->adapter;

	if (unlikely(rxq == NULL) || unlikely(!rxq->q_started))
		return nb_rx;

	rx_id = rxq->rx_tail;
	rx_ring = rxq->rx_ring;
	ptype_tbl = rxq->adapter->ptype_tbl;

	if ((rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) != 0)
		rxq->hw_register_set = 1;

	while (nb_rx < nb_pkts) {
		rxdp = &rx_ring[rx_id];
		rx_status0 = rte_le_to_cpu_16(rxdp->flex_nic_wb.status_error0);

		/* Check the DD bit first */
		if ((rx_status0 & (1 << VIRTCHNL2_RX_FLEX_DESC_STATUS0_DD_S)) == 0)
			break;

		nmb = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(nmb == NULL)) {
			dev = &rte_eth_devices[rxq->port_id];
			dev->data->rx_mbuf_alloc_failed++;
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u", rxq->port_id, rxq->queue_id);
			break;
		}
		rxd = *rxdp; /* copy descriptor in ring to temp variable*/

		nb_hold++;
		rxm = rxq->sw_ring[rx_id];
		rxq->sw_ring[rx_id] = nmb;
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
		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		rxdp->read.hdr_addr = 0;
		rxdp->read.pkt_addr = dma_addr;

		rx_packet_len = (rte_cpu_to_le_16(rxd.flex_nic_wb.pkt_len) &
				 VIRTCHNL2_RX_FLEX_DESC_PKT_LEN_M);

		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rte_prefetch0(RTE_PTR_ADD(rxm->buf_addr, RTE_PKTMBUF_HEADROOM));
		rxm->nb_segs = 1;
		rxm->next = NULL;
		rxm->pkt_len = rx_packet_len;
		rxm->data_len = rx_packet_len;
		rxm->port = rxq->port_id;
		rxm->ol_flags = 0;
		pkt_flags = idpf_rxd_to_pkt_flags(rx_status0);
		rxm->packet_type =
			ptype_tbl[(uint8_t)(rte_cpu_to_le_16(rxd.flex_nic_wb.ptype_flex_flags0) &
					    VIRTCHNL2_RX_FLEX_DESC_PTYPE_M)];

		rxm->ol_flags |= pkt_flags;

		if (idpf_timestamp_dynflag > 0 &&
		   (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) != 0) {
			/* timestamp */
			ts_ns = idpf_tstamp_convert_32b_64b(ad,
				rxq->hw_register_set,
				rte_le_to_cpu_32(rxd.flex_nic_wb.flex_ts.ts_high));
			rxq->hw_register_set = 0;
			*RTE_MBUF_DYNFIELD(rxm,
					   idpf_timestamp_dynfield_offset,
					   rte_mbuf_timestamp_t *) = ts_ns;
			rxm->ol_flags |= idpf_timestamp_dynflag;
		}

		rx_pkts[nb_rx++] = rxm;
	}
	rxq->rx_tail = rx_id;

	idpf_update_rx_tail(rxq, nb_hold, rx_id);

	return nb_rx;
}

static inline int
idpf_xmit_cleanup(struct idpf_tx_queue *txq)
{
	uint16_t last_desc_cleaned = txq->last_desc_cleaned;
	struct idpf_tx_entry *sw_ring = txq->sw_ring;
	uint16_t nb_tx_desc = txq->nb_tx_desc;
	uint16_t desc_to_clean_to;
	uint16_t nb_tx_to_clean;
	uint16_t i;

	volatile struct idpf_flex_tx_desc *txd = txq->tx_ring;

	desc_to_clean_to = (uint16_t)(last_desc_cleaned + txq->rs_thresh);
	if (desc_to_clean_to >= nb_tx_desc)
		desc_to_clean_to = (uint16_t)(desc_to_clean_to - nb_tx_desc);

	desc_to_clean_to = sw_ring[desc_to_clean_to].last_id;
	/* In the writeback Tx desccriptor, the only significant fields are the 4-bit DTYPE */
	if ((txd[desc_to_clean_to].qw1.cmd_dtype &
			rte_cpu_to_le_16(IDPF_TXD_QW1_DTYPE_M)) !=
			rte_cpu_to_le_16(IDPF_TX_DESC_DTYPE_DESC_DONE)) {
		PMD_TX_LOG(DEBUG, "TX descriptor %4u is not done "
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

	txd[desc_to_clean_to].qw1.cmd_dtype = 0;
	txd[desc_to_clean_to].qw1.buf_size = 0;
	for (i = 0; i < RTE_DIM(txd[desc_to_clean_to].qw1.flex.raw); i++)
		txd[desc_to_clean_to].qw1.flex.raw[i] = 0;

	txq->last_desc_cleaned = desc_to_clean_to;
	txq->nb_free = (uint16_t)(txq->nb_free + nb_tx_to_clean);

	return 0;
}

/* TX function */
uint16_t
idpf_singleq_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts)
{
	volatile struct idpf_flex_tx_desc *txd;
	volatile struct idpf_flex_tx_desc *txr;
	union idpf_tx_offload tx_offload = {0};
	struct idpf_tx_entry *txe, *txn;
	struct idpf_tx_entry *sw_ring;
	struct idpf_tx_queue *txq;
	struct rte_mbuf *tx_pkt;
	struct rte_mbuf *m_seg;
	uint64_t buf_dma_addr;
	uint64_t ol_flags;
	uint16_t tx_last;
	uint16_t nb_used;
	uint16_t nb_ctx;
	uint16_t td_cmd;
	uint16_t tx_id;
	uint16_t nb_tx;
	uint16_t slen;

	nb_tx = 0;
	txq = tx_queue;

	if (unlikely(txq == NULL) || unlikely(!txq->q_started))
		return nb_tx;

	sw_ring = txq->sw_ring;
	txr = txq->tx_ring;
	tx_id = txq->tx_tail;
	txe = &sw_ring[tx_id];

	/* Check if the descriptor ring needs to be cleaned. */
	if (txq->nb_free < txq->free_thresh)
		(void)idpf_xmit_cleanup(txq);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		td_cmd = 0;

		tx_pkt = *tx_pkts++;
		RTE_MBUF_PREFETCH_TO_FREE(txe->mbuf);

		ol_flags = tx_pkt->ol_flags;
		tx_offload.l2_len = tx_pkt->l2_len;
		tx_offload.l3_len = tx_pkt->l3_len;
		tx_offload.l4_len = tx_pkt->l4_len;
		tx_offload.tso_segsz = tx_pkt->tso_segsz;
		/* Calculate the number of context descriptors needed. */
		nb_ctx = idpf_calc_context_desc(ol_flags);

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
			if (idpf_xmit_cleanup(txq) != 0) {
				if (nb_tx == 0)
					return 0;
				goto end_of_tx;
			}
			if (unlikely(nb_used > txq->rs_thresh)) {
				while (nb_used > txq->nb_free) {
					if (idpf_xmit_cleanup(txq) != 0) {
						if (nb_tx == 0)
							return 0;
						goto end_of_tx;
					}
				}
			}
		}

		if (nb_ctx != 0) {
			/* Setup TX context descriptor if required */
			volatile union idpf_flex_tx_ctx_desc *ctx_txd =
				(volatile union idpf_flex_tx_ctx_desc *)
							&txr[tx_id];

			txn = &sw_ring[txe->next_id];
			RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);
			if (txe->mbuf != NULL) {
				rte_pktmbuf_free_seg(txe->mbuf);
				txe->mbuf = NULL;
			}

			/* TSO enabled */
			if ((ol_flags & RTE_MBUF_F_TX_TCP_SEG) != 0)
				idpf_set_splitq_tso_ctx(tx_pkt, tx_offload,
							ctx_txd);

			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
		}

		m_seg = tx_pkt;
		do {
			txd = &txr[tx_id];
			txn = &sw_ring[txe->next_id];

			if (txe->mbuf != NULL)
				rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = m_seg;

			/* Setup TX Descriptor */
			slen = m_seg->data_len;
			buf_dma_addr = rte_mbuf_data_iova(m_seg);
			txd->buf_addr = rte_cpu_to_le_64(buf_dma_addr);
			txd->qw1.buf_size = slen;
			txd->qw1.cmd_dtype = rte_cpu_to_le_16(IDPF_TX_DESC_DTYPE_FLEX_DATA <<
							      IDPF_FLEX_TXD_QW1_DTYPE_S);

			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
			m_seg = m_seg->next;
		} while (m_seg);

		/* The last packet data descriptor needs End Of Packet (EOP) */
		td_cmd |= IDPF_TX_FLEX_DESC_CMD_EOP;
		txq->nb_used = (uint16_t)(txq->nb_used + nb_used);
		txq->nb_free = (uint16_t)(txq->nb_free - nb_used);

		if (txq->nb_used >= txq->rs_thresh) {
			PMD_TX_LOG(DEBUG, "Setting RS bit on TXD id="
				   "%4u (port=%d queue=%d)",
				   tx_last, txq->port_id, txq->queue_id);

			td_cmd |= IDPF_TX_FLEX_DESC_CMD_RS;

			/* Update txq RS bit counters */
			txq->nb_used = 0;
		}

		if (ol_flags & IDPF_TX_CKSUM_OFFLOAD_MASK)
			td_cmd |= IDPF_TX_FLEX_DESC_CMD_CS_EN;

		txd->qw1.cmd_dtype |= rte_cpu_to_le_16(td_cmd << IDPF_FLEX_TXD_QW1_CMD_S);
	}

end_of_tx:
	rte_wmb();

	PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u tx_tail=%u nb_tx=%u",
		   txq->port_id, txq->queue_id, tx_id, nb_tx);

	IDPF_PCI_REG_WRITE(txq->qtx_tail, tx_id);
	txq->tx_tail = tx_id;

	return nb_tx;
}

/* TX prep functions */
uint16_t
idpf_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
	       uint16_t nb_pkts)
{
#ifdef RTE_LIBRTE_ETHDEV_DEBUG
	int ret;
#endif
	int i;
	uint64_t ol_flags;
	struct rte_mbuf *m;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;

		/* Check condition for nb_segs > IDPF_TX_MAX_MTU_SEG. */
		if ((ol_flags & RTE_MBUF_F_TX_TCP_SEG) == 0) {
			if (m->nb_segs > IDPF_TX_MAX_MTU_SEG) {
				rte_errno = EINVAL;
				return i;
			}
		} else if ((m->tso_segsz < IDPF_MIN_TSO_MSS) ||
			   (m->tso_segsz > IDPF_MAX_TSO_MSS) ||
			   (m->pkt_len > IDPF_MAX_TSO_FRAME_SIZE)) {
			/* MSS outside the range are considered malicious */
			rte_errno = EINVAL;
			return i;
		}

		if ((ol_flags & IDPF_TX_OFFLOAD_NOTSUP_MASK) != 0) {
			rte_errno = ENOTSUP;
			return i;
		}

		if (m->pkt_len < IDPF_MIN_FRAME_SIZE) {
			rte_errno = EINVAL;
			return i;
		}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = -ret;
			return i;
		}
#endif
	}

	return i;
}

static void __rte_cold
release_rxq_mbufs_vec(struct idpf_rx_queue *rxq)
{
	const uint16_t mask = rxq->nb_rx_desc - 1;
	uint16_t i;

	if (rxq->sw_ring == NULL || rxq->rxrearm_nb >= rxq->nb_rx_desc)
		return;

	/* free all mbufs that are valid in the ring */
	if (rxq->rxrearm_nb == 0) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i] != NULL)
				rte_pktmbuf_free_seg(rxq->sw_ring[i]);
		}
	} else {
		for (i = rxq->rx_tail; i != rxq->rxrearm_start; i = (i + 1) & mask) {
			if (rxq->sw_ring[i] != NULL)
				rte_pktmbuf_free_seg(rxq->sw_ring[i]);
		}
	}

	rxq->rxrearm_nb = rxq->nb_rx_desc;

	/* set all entries to NULL */
	memset(rxq->sw_ring, 0, sizeof(rxq->sw_ring[0]) * rxq->nb_rx_desc);
}

static const struct idpf_rxq_ops def_singleq_rx_ops_vec = {
	.release_mbufs = release_rxq_mbufs_vec,
};

static inline int
idpf_singleq_rx_vec_setup_default(struct idpf_rx_queue *rxq)
{
	uintptr_t p;
	struct rte_mbuf mb_def = { .buf_addr = 0 }; /* zeroed mbuf */

	mb_def.nb_segs = 1;
	mb_def.data_off = RTE_PKTMBUF_HEADROOM;
	mb_def.port = rxq->port_id;
	rte_mbuf_refcnt_set(&mb_def, 1);

	/* prevent compiler reordering: rearm_data covers previous fields */
	rte_compiler_barrier();
	p = (uintptr_t)&mb_def.rearm_data;
	rxq->mbuf_initializer = *(uint64_t *)p;
	return 0;
}

int __rte_cold
idpf_singleq_rx_vec_setup(struct idpf_rx_queue *rxq)
{
	rxq->ops = &def_singleq_rx_ops_vec;
	return idpf_singleq_rx_vec_setup_default(rxq);
}

void
idpf_set_rx_function(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
#ifdef RTE_ARCH_X86
	struct idpf_adapter *ad = vport->adapter;
	struct idpf_rx_queue *rxq;
	int i;

	if (idpf_rx_vec_dev_check_default(dev) == IDPF_VECTOR_PATH &&
	    rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128) {
		ad->rx_vec_allowed = true;

		if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_512)
#ifdef CC_AVX512_SUPPORT
			if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) == 1 &&
			    rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512BW) == 1)
				ad->rx_use_avx512 = true;
#else
		PMD_DRV_LOG(NOTICE,
			    "AVX512 is not supported in build env");
#endif /* CC_AVX512_SUPPORT */
	} else {
		ad->rx_vec_allowed = false;
	}
#endif /* RTE_ARCH_X86 */

#ifdef RTE_ARCH_X86
	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
		dev->rx_pkt_burst = idpf_splitq_recv_pkts;
	} else {
		if (ad->rx_vec_allowed) {
			for (i = 0; i < dev->data->nb_tx_queues; i++) {
				rxq = dev->data->rx_queues[i];
				(void)idpf_singleq_rx_vec_setup(rxq);
			}
#ifdef CC_AVX512_SUPPORT
			if (ad->rx_use_avx512) {
				dev->rx_pkt_burst = idpf_singleq_recv_pkts_avx512;
				return;
			}
#endif /* CC_AVX512_SUPPORT */
		}

		dev->rx_pkt_burst = idpf_singleq_recv_pkts;
	}
#else
	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT)
		dev->rx_pkt_burst = idpf_splitq_recv_pkts;
	else
		dev->rx_pkt_burst = idpf_singleq_recv_pkts;
#endif /* RTE_ARCH_X86 */
}

void
idpf_set_tx_function(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
#ifdef RTE_ARCH_X86
	struct idpf_adapter *ad = vport->adapter;
#ifdef CC_AVX512_SUPPORT
	struct idpf_tx_queue *txq;
	int i;
#endif /* CC_AVX512_SUPPORT */

	if (idpf_rx_vec_dev_check_default(dev) == IDPF_VECTOR_PATH &&
	    rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128) {
		ad->tx_vec_allowed = true;
		if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_512)
#ifdef CC_AVX512_SUPPORT
			if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) == 1 &&
			    rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512BW) == 1)
				ad->tx_use_avx512 = true;
#else
		PMD_DRV_LOG(NOTICE,
			    "AVX512 is not supported in build env");
#endif /* CC_AVX512_SUPPORT */
	} else {
		ad->tx_vec_allowed = false;
	}
#endif /* RTE_ARCH_X86 */

	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
		dev->tx_pkt_burst = idpf_splitq_xmit_pkts;
		dev->tx_pkt_prepare = idpf_prep_pkts;
	} else {
#ifdef RTE_ARCH_X86
		if (ad->tx_vec_allowed) {
#ifdef CC_AVX512_SUPPORT
			if (ad->tx_use_avx512) {
				for (i = 0; i < dev->data->nb_tx_queues; i++) {
					txq = dev->data->tx_queues[i];
					if (txq == NULL)
						continue;
					idpf_singleq_tx_vec_setup_avx512(txq);
				}
				dev->tx_pkt_burst = idpf_singleq_xmit_pkts_avx512;
				dev->tx_pkt_prepare = idpf_prep_pkts;
				return;
			}
#endif /* CC_AVX512_SUPPORT */
		}
#endif /* RTE_ARCH_X86 */
		dev->tx_pkt_burst = idpf_singleq_xmit_pkts;
		dev->tx_pkt_prepare = idpf_prep_pkts;
	}
}
