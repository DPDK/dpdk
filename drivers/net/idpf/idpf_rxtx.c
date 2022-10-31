/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <ethdev_driver.h>
#include <rte_net.h>

#include "idpf_ethdev.h"
#include "idpf_rxtx.h"

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
