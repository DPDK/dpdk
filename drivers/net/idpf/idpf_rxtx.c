/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <ethdev_driver.h>
#include <rte_net.h>

#include "idpf_ethdev.h"
#include "idpf_rxtx.h"

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

	if (rxq->pkt_first_seg != NULL)
		rte_pktmbuf_free(rxq->pkt_first_seg);

	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
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
