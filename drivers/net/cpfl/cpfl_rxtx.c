/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <ethdev_driver.h>
#include <rte_net.h>
#include <rte_vect.h>

#include "cpfl_ethdev.h"
#include "cpfl_rxtx.h"

static uint64_t
cpfl_tx_offload_convert(uint64_t offload)
{
	uint64_t ol = 0;

	if ((offload & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM) != 0)
		ol |= IDPF_TX_OFFLOAD_IPV4_CKSUM;
	if ((offload & RTE_ETH_TX_OFFLOAD_UDP_CKSUM) != 0)
		ol |= IDPF_TX_OFFLOAD_UDP_CKSUM;
	if ((offload & RTE_ETH_TX_OFFLOAD_TCP_CKSUM) != 0)
		ol |= IDPF_TX_OFFLOAD_TCP_CKSUM;
	if ((offload & RTE_ETH_TX_OFFLOAD_SCTP_CKSUM) != 0)
		ol |= IDPF_TX_OFFLOAD_SCTP_CKSUM;
	if ((offload & RTE_ETH_TX_OFFLOAD_MULTI_SEGS) != 0)
		ol |= IDPF_TX_OFFLOAD_MULTI_SEGS;
	if ((offload & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) != 0)
		ol |= IDPF_TX_OFFLOAD_MBUF_FAST_FREE;

	return ol;
}

static const struct rte_memzone *
cpfl_dma_zone_reserve(struct rte_eth_dev *dev, uint16_t queue_idx,
		      uint16_t len, uint16_t queue_type,
		      unsigned int socket_id, bool splitq)
{
	char ring_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	uint32_t ring_size;

	memset(ring_name, 0, RTE_MEMZONE_NAMESIZE);
	switch (queue_type) {
	case VIRTCHNL2_QUEUE_TYPE_TX:
		if (splitq)
			ring_size = RTE_ALIGN(len * sizeof(struct idpf_flex_tx_sched_desc),
					      CPFL_DMA_MEM_ALIGN);
		else
			ring_size = RTE_ALIGN(len * sizeof(struct idpf_flex_tx_desc),
					      CPFL_DMA_MEM_ALIGN);
		memcpy(ring_name, "cpfl Tx ring", sizeof("cpfl Tx ring"));
		break;
	case VIRTCHNL2_QUEUE_TYPE_RX:
		if (splitq)
			ring_size = RTE_ALIGN(len * sizeof(struct virtchnl2_rx_flex_desc_adv_nic_3),
					      CPFL_DMA_MEM_ALIGN);
		else
			ring_size = RTE_ALIGN(len * sizeof(struct virtchnl2_singleq_rx_buf_desc),
					      CPFL_DMA_MEM_ALIGN);
		memcpy(ring_name, "cpfl Rx ring", sizeof("cpfl Rx ring"));
		break;
	case VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION:
		ring_size = RTE_ALIGN(len * sizeof(struct idpf_splitq_tx_compl_desc),
				      CPFL_DMA_MEM_ALIGN);
		memcpy(ring_name, "cpfl Tx compl ring", sizeof("cpfl Tx compl ring"));
		break;
	case VIRTCHNL2_QUEUE_TYPE_RX_BUFFER:
		ring_size = RTE_ALIGN(len * sizeof(struct virtchnl2_splitq_rx_buf_desc),
				      CPFL_DMA_MEM_ALIGN);
		memcpy(ring_name, "cpfl Rx buf ring", sizeof("cpfl Rx buf ring"));
		break;
	default:
		PMD_INIT_LOG(ERR, "Invalid queue type");
		return NULL;
	}

	mz = rte_eth_dma_zone_reserve(dev, ring_name, queue_idx,
				      ring_size, CPFL_RING_BASE_ALIGN,
				      socket_id);
	if (mz == NULL) {
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for ring");
		return NULL;
	}

	/* Zero all the descriptors in the ring. */
	memset(mz->addr, 0, ring_size);

	return mz;
}

static void
cpfl_dma_zone_release(const struct rte_memzone *mz)
{
	rte_memzone_free(mz);
}

static int
cpfl_tx_complq_setup(struct rte_eth_dev *dev, struct idpf_tx_queue *txq,
		     uint16_t queue_idx, uint16_t nb_desc,
		     unsigned int socket_id)
{
	struct idpf_vport *vport = dev->data->dev_private;
	const struct rte_memzone *mz;
	struct idpf_tx_queue *cq;
	int ret;

	cq = rte_zmalloc_socket("cpfl splitq cq",
				sizeof(struct idpf_tx_queue),
				RTE_CACHE_LINE_SIZE,
				socket_id);
	if (cq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for Tx compl queue");
		ret = -ENOMEM;
		goto err_cq_alloc;
	}

	cq->nb_tx_desc = nb_desc;
	cq->queue_id = vport->chunks_info.tx_compl_start_qid + queue_idx;
	cq->port_id = dev->data->port_id;
	cq->txqs = dev->data->tx_queues;
	cq->tx_start_qid = vport->chunks_info.tx_start_qid;

	mz = cpfl_dma_zone_reserve(dev, queue_idx, nb_desc,
				   VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION,
				   socket_id, true);
	if (mz == NULL) {
		ret = -ENOMEM;
		goto err_mz_reserve;
	}
	cq->tx_ring_phys_addr = mz->iova;
	cq->compl_ring = mz->addr;
	cq->mz = mz;
	idpf_qc_split_tx_complq_reset(cq);

	txq->complq = cq;

	return 0;

err_mz_reserve:
	rte_free(cq);
err_cq_alloc:
	return ret;
}

int
cpfl_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		    uint16_t nb_desc, unsigned int socket_id,
		    const struct rte_eth_txconf *tx_conf)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *base = vport->adapter;
	uint16_t tx_rs_thresh, tx_free_thresh;
	struct idpf_hw *hw = &base->hw;
	const struct rte_memzone *mz;
	struct idpf_tx_queue *txq;
	uint64_t offloads;
	uint16_t len;
	bool is_splitq;
	int ret;

	offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;

	tx_rs_thresh = (uint16_t)((tx_conf->tx_rs_thresh > 0) ?
		tx_conf->tx_rs_thresh : CPFL_DEFAULT_TX_RS_THRESH);
	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh > 0) ?
		tx_conf->tx_free_thresh : CPFL_DEFAULT_TX_FREE_THRESH);
	if (idpf_qc_tx_thresh_check(nb_desc, tx_rs_thresh, tx_free_thresh) != 0)
		return -EINVAL;

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket("cpfl txq",
				 sizeof(struct idpf_tx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (txq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for tx queue structure");
		ret = -ENOMEM;
		goto err_txq_alloc;
	}

	is_splitq = !!(vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT);

	txq->nb_tx_desc = nb_desc;
	txq->rs_thresh = tx_rs_thresh;
	txq->free_thresh = tx_free_thresh;
	txq->queue_id = vport->chunks_info.tx_start_qid + queue_idx;
	txq->port_id = dev->data->port_id;
	txq->offloads = cpfl_tx_offload_convert(offloads);
	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	if (is_splitq)
		len = 2 * nb_desc;
	else
		len = nb_desc;
	txq->sw_nb_desc = len;

	/* Allocate TX hardware ring descriptors. */
	mz = cpfl_dma_zone_reserve(dev, queue_idx, nb_desc, VIRTCHNL2_QUEUE_TYPE_TX,
				   socket_id, is_splitq);
	if (mz == NULL) {
		ret = -ENOMEM;
		goto err_mz_reserve;
	}
	txq->tx_ring_phys_addr = mz->iova;
	txq->mz = mz;

	txq->sw_ring = rte_zmalloc_socket("cpfl tx sw ring",
					  sizeof(struct idpf_tx_entry) * len,
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW TX ring");
		ret = -ENOMEM;
		goto err_sw_ring_alloc;
	}

	if (!is_splitq) {
		txq->tx_ring = mz->addr;
		idpf_qc_single_tx_queue_reset(txq);
	} else {
		txq->desc_ring = mz->addr;
		idpf_qc_split_tx_descq_reset(txq);

		/* Setup tx completion queue if split model */
		ret = cpfl_tx_complq_setup(dev, txq, queue_idx,
					   2 * nb_desc, socket_id);
		if (ret != 0)
			goto err_complq_setup;
	}

	txq->qtx_tail = hw->hw_addr + (vport->chunks_info.tx_qtail_start +
			queue_idx * vport->chunks_info.tx_qtail_spacing);
	txq->q_set = true;
	dev->data->tx_queues[queue_idx] = txq;

	return 0;

err_complq_setup:
err_sw_ring_alloc:
	cpfl_dma_zone_release(mz);
err_mz_reserve:
	rte_free(txq);
err_txq_alloc:
	return ret;
}
