/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <ethdev_driver.h>
#include <rte_net.h>
#include <rte_vect.h>

#include "cpfl_ethdev.h"
#include "cpfl_rxtx.h"
#include "cpfl_rxtx_vec_common.h"

static inline void
cpfl_tx_hairpin_descq_reset(struct idpf_tx_queue *txq)
{
	uint32_t i, size;

	if (!txq) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq is NULL");
		return;
	}

	size = txq->nb_tx_desc * CPFL_P2P_DESC_LEN;
	for (i = 0; i < size; i++)
		((volatile char *)txq->desc_ring)[i] = 0;
}

static inline void
cpfl_tx_hairpin_complq_reset(struct idpf_tx_queue *cq)
{
	uint32_t i, size;

	if (!cq) {
		PMD_DRV_LOG(DEBUG, "Pointer to complq is NULL");
		return;
	}

	size = cq->nb_tx_desc * CPFL_P2P_DESC_LEN;
	for (i = 0; i < size; i++)
		((volatile char *)cq->compl_ring)[i] = 0;
}

static inline void
cpfl_rx_hairpin_descq_reset(struct idpf_rx_queue *rxq)
{
	uint16_t len;
	uint32_t i;

	if (!rxq)
		return;

	len = rxq->nb_rx_desc;
	for (i = 0; i < len * CPFL_P2P_DESC_LEN; i++)
		((volatile char *)rxq->rx_ring)[i] = 0;
}

static inline void
cpfl_rx_hairpin_bufq_reset(struct idpf_rx_queue *rxbq)
{
	uint16_t len;
	uint32_t i;

	if (!rxbq)
		return;

	len = rxbq->nb_rx_desc;
	for (i = 0; i < len * CPFL_P2P_DESC_LEN; i++)
		((volatile char *)rxbq->rx_ring)[i] = 0;

	rxbq->bufq1 = NULL;
	rxbq->bufq2 = NULL;
}

static uint64_t
cpfl_rx_offload_convert(uint64_t offload)
{
	uint64_t ol = 0;

	if ((offload & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM) != 0)
		ol |= IDPF_RX_OFFLOAD_IPV4_CKSUM;
	if ((offload & RTE_ETH_RX_OFFLOAD_UDP_CKSUM) != 0)
		ol |= IDPF_RX_OFFLOAD_UDP_CKSUM;
	if ((offload & RTE_ETH_RX_OFFLOAD_TCP_CKSUM) != 0)
		ol |= IDPF_RX_OFFLOAD_TCP_CKSUM;
	if ((offload & RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM) != 0)
		ol |= IDPF_RX_OFFLOAD_OUTER_IPV4_CKSUM;
	if ((offload & RTE_ETH_RX_OFFLOAD_TIMESTAMP) != 0)
		ol |= IDPF_RX_OFFLOAD_TIMESTAMP;

	return ol;
}

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

static const struct idpf_rxq_ops def_rxq_ops = {
	.release_mbufs = idpf_qc_rxq_mbufs_release,
};

static const struct idpf_txq_ops def_txq_ops = {
	.release_mbufs = idpf_qc_txq_mbufs_release,
};

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
cpfl_rx_split_bufq_setup(struct rte_eth_dev *dev, struct idpf_rx_queue *rxq,
			 uint16_t queue_idx, uint16_t rx_free_thresh,
			 uint16_t nb_desc, unsigned int socket_id,
			 struct rte_mempool *mp, uint8_t bufq_id)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct idpf_adapter *base = vport->adapter;
	struct idpf_hw *hw = &base->hw;
	const struct rte_memzone *mz;
	struct idpf_rx_queue *bufq;
	uint16_t len;
	int ret;

	bufq = rte_zmalloc_socket("cpfl bufq",
				   sizeof(struct idpf_rx_queue),
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (bufq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for rx buffer queue.");
		ret = -ENOMEM;
		goto err_bufq1_alloc;
	}

	bufq->mp = mp;
	bufq->nb_rx_desc = nb_desc;
	bufq->rx_free_thresh = rx_free_thresh;
	bufq->queue_id = vport->chunks_info.rx_buf_start_qid + queue_idx;
	bufq->port_id = dev->data->port_id;
	bufq->rx_hdr_len = 0;
	bufq->adapter = base;

	len = rte_pktmbuf_data_room_size(bufq->mp) - RTE_PKTMBUF_HEADROOM;
	bufq->rx_buf_len = RTE_ALIGN_FLOOR(len, (1 << IDPF_RLAN_CTX_DBUF_S));
	bufq->rx_buf_len = RTE_MIN(bufq->rx_buf_len, IDPF_RX_MAX_DATA_BUF_SIZE);

	/* Allocate a little more to support bulk allocate. */
	len = nb_desc + IDPF_RX_MAX_BURST;

	mz = cpfl_dma_zone_reserve(dev, queue_idx, len,
				   VIRTCHNL2_QUEUE_TYPE_RX_BUFFER,
				   socket_id, true);
	if (mz == NULL) {
		ret = -ENOMEM;
		goto err_mz_reserve;
	}

	bufq->rx_ring_phys_addr = mz->iova;
	bufq->rx_ring = mz->addr;
	bufq->mz = mz;

	bufq->sw_ring =
		rte_zmalloc_socket("cpfl rx bufq sw ring",
				   sizeof(struct rte_mbuf *) * len,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (bufq->sw_ring == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW ring");
		ret = -ENOMEM;
		goto err_sw_ring_alloc;
	}

	idpf_qc_split_rx_bufq_reset(bufq);
	bufq->qrx_tail = hw->hw_addr + (vport->chunks_info.rx_buf_qtail_start +
			 queue_idx * vport->chunks_info.rx_buf_qtail_spacing);
	bufq->ops = &def_rxq_ops;
	bufq->q_set = true;

	if (bufq_id == IDPF_RX_SPLIT_BUFQ1_ID) {
		rxq->bufq1 = bufq;
	} else if (bufq_id == IDPF_RX_SPLIT_BUFQ2_ID) {
		rxq->bufq2 = bufq;
	} else {
		PMD_INIT_LOG(ERR, "Invalid buffer queue index.");
		ret = -EINVAL;
		goto err_bufq_id;
	}

	return 0;

err_bufq_id:
	rte_free(bufq->sw_ring);
err_sw_ring_alloc:
	cpfl_dma_zone_release(mz);
err_mz_reserve:
	rte_free(bufq);
err_bufq1_alloc:
	return ret;
}

static void
cpfl_rx_split_bufq_release(struct idpf_rx_queue *bufq)
{
	rte_free(bufq->sw_ring);
	cpfl_dma_zone_release(bufq->mz);
	rte_free(bufq);
}

static void
cpfl_rx_queue_release(void *rxq)
{
	struct cpfl_rx_queue *cpfl_rxq = rxq;
	struct idpf_rx_queue *q = NULL;

	if (cpfl_rxq == NULL)
		return;

	q = &cpfl_rxq->base;

	/* Split queue */
	if (!q->adapter->is_rx_singleq) {
		/* the mz is shared between Tx/Rx hairpin, let Rx_release
		 * free the buf, q->bufq1->mz and q->mz.
		 */
		if (!cpfl_rxq->hairpin_info.hairpin_q && q->bufq2)
			cpfl_rx_split_bufq_release(q->bufq2);

		if (q->bufq1)
			cpfl_rx_split_bufq_release(q->bufq1);

		rte_memzone_free(q->mz);
		rte_free(cpfl_rxq);
		return;
	}

	/* Single queue */
	q->ops->release_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_free(cpfl_rxq);
}

static void
cpfl_tx_queue_release(void *txq)
{
	struct cpfl_tx_queue *cpfl_txq = txq;
	struct idpf_tx_queue *q = NULL;

	if (cpfl_txq == NULL)
		return;

	q = &cpfl_txq->base;

	if (q->complq) {
		rte_memzone_free(q->complq->mz);
		rte_free(q->complq);
	}

	q->ops->release_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_free(cpfl_txq);
}

int
cpfl_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		    uint16_t nb_desc, unsigned int socket_id,
		    const struct rte_eth_rxconf *rx_conf,
		    struct rte_mempool *mp)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct idpf_adapter *base = vport->adapter;
	struct idpf_hw *hw = &base->hw;
	struct cpfl_rx_queue *cpfl_rxq;
	const struct rte_memzone *mz;
	struct idpf_rx_queue *rxq;
	uint16_t rx_free_thresh;
	uint64_t offloads;
	bool is_splitq;
	uint16_t len;
	int ret;

	offloads = rx_conf->offloads | dev->data->dev_conf.rxmode.offloads;

	/* Check free threshold */
	rx_free_thresh = (rx_conf->rx_free_thresh == 0) ?
		CPFL_DEFAULT_RX_FREE_THRESH :
		rx_conf->rx_free_thresh;
	if (idpf_qc_rx_thresh_check(nb_desc, rx_free_thresh) != 0)
		return -EINVAL;

	/* Free memory if needed */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		cpfl_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Setup Rx queue */
	cpfl_rxq = rte_zmalloc_socket("cpfl rxq",
				 sizeof(struct cpfl_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (cpfl_rxq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for rx queue data structure");
		ret = -ENOMEM;
		goto err_rxq_alloc;
	}

	rxq = &cpfl_rxq->base;

	is_splitq = !!(vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT);

	rxq->mp = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_free_thresh;
	rxq->queue_id = vport->chunks_info.rx_start_qid + queue_idx;
	rxq->port_id = dev->data->port_id;
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;
	rxq->rx_hdr_len = 0;
	rxq->adapter = base;
	rxq->offloads = cpfl_rx_offload_convert(offloads);

	len = rte_pktmbuf_data_room_size(rxq->mp) - RTE_PKTMBUF_HEADROOM;
	rxq->rx_buf_len = len;

	/* Allocate a little more to support bulk allocate. */
	len = nb_desc + IDPF_RX_MAX_BURST;
	mz = cpfl_dma_zone_reserve(dev, queue_idx, len, VIRTCHNL2_QUEUE_TYPE_RX,
				   socket_id, is_splitq);
	if (mz == NULL) {
		ret = -ENOMEM;
		goto err_mz_reserve;
	}
	rxq->rx_ring_phys_addr = mz->iova;
	rxq->rx_ring = mz->addr;
	rxq->mz = mz;

	if (!is_splitq) {
		rxq->sw_ring = rte_zmalloc_socket("cpfl rxq sw ring",
						  sizeof(struct rte_mbuf *) * len,
						  RTE_CACHE_LINE_SIZE,
						  socket_id);
		if (rxq->sw_ring == NULL) {
			PMD_INIT_LOG(ERR, "Failed to allocate memory for SW ring");
			ret = -ENOMEM;
			goto err_sw_ring_alloc;
		}

		idpf_qc_single_rx_queue_reset(rxq);
		rxq->qrx_tail = hw->hw_addr + (vport->chunks_info.rx_qtail_start +
				queue_idx * vport->chunks_info.rx_qtail_spacing);
		rxq->ops = &def_rxq_ops;
	} else {
		idpf_qc_split_rx_descq_reset(rxq);

		/* Setup Rx buffer queues */
		ret = cpfl_rx_split_bufq_setup(dev, rxq, 2 * queue_idx,
					       rx_free_thresh, nb_desc,
					       socket_id, mp, 1);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Failed to setup buffer queue 1");
			ret = -EINVAL;
			goto err_bufq1_setup;
		}

		ret = cpfl_rx_split_bufq_setup(dev, rxq, 2 * queue_idx + 1,
					       rx_free_thresh, nb_desc,
					       socket_id, mp, 2);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Failed to setup buffer queue 2");
			ret = -EINVAL;
			goto err_bufq2_setup;
		}
	}

	cpfl_vport->nb_data_rxq++;
	rxq->q_set = true;
	dev->data->rx_queues[queue_idx] = cpfl_rxq;

	return 0;

err_bufq2_setup:
	cpfl_rx_split_bufq_release(rxq->bufq1);
err_bufq1_setup:
err_sw_ring_alloc:
	cpfl_dma_zone_release(mz);
err_mz_reserve:
	rte_free(rxq);
err_rxq_alloc:
	return ret;
}

static int
cpfl_tx_complq_setup(struct rte_eth_dev *dev, struct idpf_tx_queue *txq,
		     uint16_t queue_idx, uint16_t nb_desc,
		     unsigned int socket_id)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
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
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct idpf_adapter *base = vport->adapter;
	uint16_t tx_rs_thresh, tx_free_thresh;
	struct cpfl_tx_queue *cpfl_txq;
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

	/* Free memory if needed. */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		cpfl_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* Allocate the TX queue data structure. */
	cpfl_txq = rte_zmalloc_socket("cpfl txq",
				 sizeof(struct cpfl_tx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (cpfl_txq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for tx queue structure");
		ret = -ENOMEM;
		goto err_txq_alloc;
	}

	txq = &cpfl_txq->base;

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
	txq->ops = &def_txq_ops;
	cpfl_vport->nb_data_txq++;
	txq->q_set = true;
	dev->data->tx_queues[queue_idx] = cpfl_txq;

	return 0;

err_complq_setup:
err_sw_ring_alloc:
	cpfl_dma_zone_release(mz);
err_mz_reserve:
	rte_free(txq);
err_txq_alloc:
	return ret;
}

static int
cpfl_rx_hairpin_bufq_setup(struct rte_eth_dev *dev, struct idpf_rx_queue *bufq,
			   uint16_t logic_qid, uint16_t nb_desc)
{
	struct cpfl_vport *cpfl_vport =
	    (struct cpfl_vport *)dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct idpf_adapter *adapter = vport->adapter;
	struct rte_mempool *mp;
	char pool_name[RTE_MEMPOOL_NAMESIZE];

	mp = cpfl_vport->p2p_mp;
	if (!mp) {
		snprintf(pool_name, RTE_MEMPOOL_NAMESIZE, "p2p_mb_pool_%u",
			 dev->data->port_id);
		mp = rte_pktmbuf_pool_create(pool_name, CPFL_P2P_NB_MBUF * CPFL_MAX_P2P_NB_QUEUES,
					     CPFL_P2P_CACHE_SIZE, 0, CPFL_P2P_MBUF_SIZE,
					     dev->device->numa_node);
		if (!mp) {
			PMD_INIT_LOG(ERR, "Failed to allocate mbuf pool for p2p");
			return -ENOMEM;
		}
		cpfl_vport->p2p_mp = mp;
	}

	bufq->mp = mp;
	bufq->nb_rx_desc = nb_desc;
	bufq->queue_id = cpfl_hw_qid_get(cpfl_vport->p2p_q_chunks_info->rx_buf_start_qid,
					 logic_qid);
	bufq->port_id = dev->data->port_id;
	bufq->adapter = adapter;
	bufq->rx_buf_len = CPFL_P2P_MBUF_SIZE - RTE_PKTMBUF_HEADROOM;

	bufq->q_set = true;
	bufq->ops = &def_rxq_ops;

	return 0;
}

int
cpfl_rx_hairpin_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			    uint16_t nb_desc,
			    const struct rte_eth_hairpin_conf *conf)
{
	struct cpfl_vport *cpfl_vport = (struct cpfl_vport *)dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct idpf_adapter *adapter_base = vport->adapter;
	uint16_t logic_qid = cpfl_vport->nb_p2p_rxq;
	struct cpfl_rxq_hairpin_info *hairpin_info;
	struct cpfl_rx_queue *cpfl_rxq;
	struct idpf_rx_queue *bufq1 = NULL;
	struct idpf_rx_queue *rxq;
	uint16_t peer_port, peer_q;
	uint16_t qid;
	int ret;

	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		PMD_INIT_LOG(ERR, "Only spilt queue model supports hairpin queue.");
		return -EINVAL;
	}

	if (conf->peer_count != 1) {
		PMD_INIT_LOG(ERR, "Can't support Rx hairpin queue peer count %d", conf->peer_count);
		return -EINVAL;
	}

	peer_port = conf->peers[0].port;
	peer_q = conf->peers[0].queue;

	if (nb_desc % CPFL_ALIGN_RING_DESC != 0 ||
	    nb_desc > CPFL_MAX_RING_DESC ||
	    nb_desc < CPFL_MIN_RING_DESC) {
		PMD_INIT_LOG(ERR, "Number (%u) of receive descriptors is invalid", nb_desc);
		return -EINVAL;
	}

	/* Free memory if needed */
	if (dev->data->rx_queues[queue_idx]) {
		cpfl_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Setup Rx description queue */
	cpfl_rxq = rte_zmalloc_socket("cpfl hairpin rxq",
				 sizeof(struct cpfl_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 SOCKET_ID_ANY);
	if (!cpfl_rxq) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for rx queue data structure");
		return -ENOMEM;
	}

	rxq = &cpfl_rxq->base;
	hairpin_info = &cpfl_rxq->hairpin_info;
	rxq->nb_rx_desc = nb_desc * 2;
	rxq->queue_id = cpfl_hw_qid_get(cpfl_vport->p2p_q_chunks_info->rx_start_qid, logic_qid);
	rxq->port_id = dev->data->port_id;
	rxq->adapter = adapter_base;
	rxq->rx_buf_len = CPFL_P2P_MBUF_SIZE - RTE_PKTMBUF_HEADROOM;
	hairpin_info->hairpin_q = true;
	hairpin_info->peer_txp = peer_port;
	hairpin_info->peer_txq_id = peer_q;

	if (conf->manual_bind != 0)
		cpfl_vport->p2p_manual_bind = true;
	else
		cpfl_vport->p2p_manual_bind = false;

	if (cpfl_vport->p2p_rx_bufq == NULL) {
		bufq1 = rte_zmalloc_socket("hairpin rx bufq1",
					   sizeof(struct idpf_rx_queue),
					   RTE_CACHE_LINE_SIZE,
					   SOCKET_ID_ANY);
		if (!bufq1) {
			PMD_INIT_LOG(ERR, "Failed to allocate memory for hairpin Rx buffer queue 1.");
			ret = -ENOMEM;
			goto err_alloc_bufq1;
		}
		qid = 2 * logic_qid;
		ret = cpfl_rx_hairpin_bufq_setup(dev, bufq1, qid, nb_desc);
		if (ret) {
			PMD_INIT_LOG(ERR, "Failed to setup hairpin Rx buffer queue 1");
			ret = -EINVAL;
			goto err_setup_bufq1;
		}
		cpfl_vport->p2p_rx_bufq = bufq1;
	}

	rxq->bufq1 = cpfl_vport->p2p_rx_bufq;
	rxq->bufq2 = NULL;

	cpfl_vport->nb_p2p_rxq++;
	rxq->q_set = true;
	dev->data->rx_queues[queue_idx] = cpfl_rxq;

	return 0;

err_setup_bufq1:
	rte_mempool_free(cpfl_vport->p2p_mp);
	rte_free(bufq1);
err_alloc_bufq1:
	rte_free(cpfl_rxq);

	return ret;
}

int
cpfl_tx_hairpin_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			    uint16_t nb_desc,
			    const struct rte_eth_hairpin_conf *conf)
{
	struct cpfl_vport *cpfl_vport =
	    (struct cpfl_vport *)dev->data->dev_private;

	struct idpf_vport *vport = &cpfl_vport->base;
	struct idpf_adapter *adapter_base = vport->adapter;
	uint16_t logic_qid = cpfl_vport->nb_p2p_txq;
	struct cpfl_txq_hairpin_info *hairpin_info;
	struct idpf_hw *hw = &adapter_base->hw;
	struct cpfl_tx_queue *cpfl_txq;
	struct idpf_tx_queue *txq, *cq;
	const struct rte_memzone *mz;
	uint32_t ring_size;
	uint16_t peer_port, peer_q;
	int ret;

	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		PMD_INIT_LOG(ERR, "Only spilt queue model supports hairpin queue.");
		return -EINVAL;
	}

	if (conf->peer_count != 1) {
		PMD_INIT_LOG(ERR, "Can't support Tx hairpin queue peer count %d", conf->peer_count);
		return -EINVAL;
	}

	peer_port = conf->peers[0].port;
	peer_q = conf->peers[0].queue;

	if (nb_desc % CPFL_ALIGN_RING_DESC != 0 ||
	    nb_desc > CPFL_MAX_RING_DESC ||
	    nb_desc < CPFL_MIN_RING_DESC) {
		PMD_INIT_LOG(ERR, "Number (%u) of transmit descriptors is invalid",
			     nb_desc);
		return -EINVAL;
	}

	/* Free memory if needed. */
	if (dev->data->tx_queues[queue_idx]) {
		cpfl_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* Allocate the TX queue data structure. */
	cpfl_txq = rte_zmalloc_socket("cpfl hairpin txq",
				 sizeof(struct cpfl_tx_queue),
				 RTE_CACHE_LINE_SIZE,
				 SOCKET_ID_ANY);
	if (!cpfl_txq) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for tx queue structure");
		return -ENOMEM;
	}

	txq = &cpfl_txq->base;
	hairpin_info = &cpfl_txq->hairpin_info;
	/* Txq ring length should be 2 times of Tx completion queue size. */
	txq->nb_tx_desc = nb_desc * 2;
	txq->queue_id = cpfl_hw_qid_get(cpfl_vport->p2p_q_chunks_info->tx_start_qid, logic_qid);
	txq->port_id = dev->data->port_id;
	hairpin_info->hairpin_q = true;
	hairpin_info->peer_rxp = peer_port;
	hairpin_info->peer_rxq_id = peer_q;

	if (conf->manual_bind != 0)
		cpfl_vport->p2p_manual_bind = true;
	else
		cpfl_vport->p2p_manual_bind = false;

	/* Always Tx hairpin queue allocates Tx HW ring */
	ring_size = RTE_ALIGN(txq->nb_tx_desc * CPFL_P2P_DESC_LEN,
			      CPFL_DMA_MEM_ALIGN);
	mz = rte_eth_dma_zone_reserve(dev, "hairpin_tx_ring", logic_qid,
				      ring_size + CPFL_P2P_RING_BUF,
				      CPFL_RING_BASE_ALIGN,
				      dev->device->numa_node);
	if (!mz) {
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for TX");
		ret = -ENOMEM;
		goto err_txq_mz_rsv;
	}

	txq->tx_ring_phys_addr = mz->iova;
	txq->desc_ring = mz->addr;
	txq->mz = mz;

	cpfl_tx_hairpin_descq_reset(txq);
	txq->qtx_tail = hw->hw_addr +
		cpfl_hw_qtail_get(cpfl_vport->p2p_q_chunks_info->tx_qtail_start,
				  logic_qid, cpfl_vport->p2p_q_chunks_info->tx_qtail_spacing);
	txq->ops = &def_txq_ops;

	if (cpfl_vport->p2p_tx_complq == NULL) {
		cq = rte_zmalloc_socket("cpfl hairpin cq",
					sizeof(struct idpf_tx_queue),
					RTE_CACHE_LINE_SIZE,
					dev->device->numa_node);
		if (!cq) {
			PMD_INIT_LOG(ERR, "Failed to allocate memory for tx queue structure");
			ret = -ENOMEM;
			goto err_cq_alloc;
		}

		cq->nb_tx_desc = nb_desc;
		cq->queue_id = cpfl_hw_qid_get(cpfl_vport->p2p_q_chunks_info->tx_compl_start_qid,
					       0);
		cq->port_id = dev->data->port_id;

		/* Tx completion queue always allocates the HW ring */
		ring_size = RTE_ALIGN(cq->nb_tx_desc * CPFL_P2P_DESC_LEN,
				      CPFL_DMA_MEM_ALIGN);
		mz = rte_eth_dma_zone_reserve(dev, "hairpin_tx_compl_ring", logic_qid,
					      ring_size + CPFL_P2P_RING_BUF,
					      CPFL_RING_BASE_ALIGN,
					      dev->device->numa_node);
		if (!mz) {
			PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for TX completion queue");
			ret = -ENOMEM;
			goto err_cq_mz_rsv;
		}
		cq->tx_ring_phys_addr = mz->iova;
		cq->compl_ring = mz->addr;
		cq->mz = mz;

		cpfl_tx_hairpin_complq_reset(cq);
		cpfl_vport->p2p_tx_complq = cq;
	}

	txq->complq = cpfl_vport->p2p_tx_complq;

	cpfl_vport->nb_p2p_txq++;
	txq->q_set = true;
	dev->data->tx_queues[queue_idx] = cpfl_txq;

	return 0;

err_cq_mz_rsv:
	rte_free(cq);
err_cq_alloc:
	cpfl_dma_zone_release(mz);
err_txq_mz_rsv:
	rte_free(cpfl_txq);
	return ret;
}

int
cpfl_hairpin_rx_bufq_config(struct cpfl_vport *cpfl_vport)
{
	struct idpf_rx_queue *rx_bufq = cpfl_vport->p2p_rx_bufq;
	struct virtchnl2_rxq_info rxq_info;

	memset(&rxq_info, 0, sizeof(rxq_info));

	rxq_info.type = VIRTCHNL2_QUEUE_TYPE_RX_BUFFER;
	rxq_info.queue_id = rx_bufq->queue_id;
	rxq_info.ring_len = rx_bufq->nb_rx_desc;
	rxq_info.dma_ring_addr = rx_bufq->rx_ring_phys_addr;
	rxq_info.desc_ids = VIRTCHNL2_RXDID_2_FLEX_SPLITQ_M;
	rxq_info.rx_buffer_low_watermark = CPFL_RXBUF_LOW_WATERMARK;
	rxq_info.model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
	rxq_info.data_buffer_size = rx_bufq->rx_buf_len;
	rxq_info.buffer_notif_stride = CPFL_RX_BUF_STRIDE;

	return idpf_vc_rxq_config_by_info(&cpfl_vport->base, &rxq_info, 1);
}

int
cpfl_hairpin_rxq_config(struct idpf_vport *vport, struct cpfl_rx_queue *cpfl_rxq)
{
	struct virtchnl2_rxq_info rxq_info;
	struct idpf_rx_queue *rxq = &cpfl_rxq->base;

	memset(&rxq_info, 0, sizeof(rxq_info));

	rxq_info.type = VIRTCHNL2_QUEUE_TYPE_RX;
	rxq_info.queue_id = rxq->queue_id;
	rxq_info.ring_len = rxq->nb_rx_desc;
	rxq_info.dma_ring_addr = rxq->rx_ring_phys_addr;
	rxq_info.rx_bufq1_id = rxq->bufq1->queue_id;
	rxq_info.max_pkt_size = vport->max_pkt_len;
	rxq_info.desc_ids = VIRTCHNL2_RXDID_2_FLEX_SPLITQ_M;
	rxq_info.qflags |= VIRTCHNL2_RX_DESC_SIZE_16BYTE;

	rxq_info.data_buffer_size = rxq->rx_buf_len;
	rxq_info.model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
	rxq_info.rx_buffer_low_watermark = CPFL_RXBUF_LOW_WATERMARK;

	PMD_DRV_LOG(NOTICE, "hairpin: vport %u, Rxq id 0x%x",
		vport->vport_id, rxq_info.queue_id);

	return idpf_vc_rxq_config_by_info(vport, &rxq_info, 1);
}

int
cpfl_hairpin_tx_complq_config(struct cpfl_vport *cpfl_vport)
{
	struct idpf_tx_queue *tx_complq = cpfl_vport->p2p_tx_complq;
	struct virtchnl2_txq_info txq_info;

	memset(&txq_info, 0, sizeof(txq_info));

	txq_info.dma_ring_addr = tx_complq->tx_ring_phys_addr;
	txq_info.type = VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION;
	txq_info.queue_id = tx_complq->queue_id;
	txq_info.ring_len = tx_complq->nb_tx_desc;
	txq_info.peer_rx_queue_id = cpfl_vport->p2p_rx_bufq->queue_id;
	txq_info.model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
	txq_info.sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_FLOW;

	return idpf_vc_txq_config_by_info(&cpfl_vport->base, &txq_info, 1);
}

int
cpfl_hairpin_txq_config(struct idpf_vport *vport, struct cpfl_tx_queue *cpfl_txq)
{
	struct idpf_tx_queue *txq = &cpfl_txq->base;
	struct virtchnl2_txq_info txq_info;

	memset(&txq_info, 0, sizeof(txq_info));

	txq_info.dma_ring_addr = txq->tx_ring_phys_addr;
	txq_info.type = VIRTCHNL2_QUEUE_TYPE_TX;
	txq_info.queue_id = txq->queue_id;
	txq_info.ring_len = txq->nb_tx_desc;
	txq_info.tx_compl_queue_id = txq->complq->queue_id;
	txq_info.relative_queue_id = txq->queue_id;
	txq_info.peer_rx_queue_id = cpfl_txq->hairpin_info.peer_rxq_id;
	txq_info.model = VIRTCHNL2_QUEUE_MODEL_SPLIT;
	txq_info.sched_mode = VIRTCHNL2_TXQ_SCHED_MODE_FLOW;

	return idpf_vc_txq_config_by_info(vport, &txq_info, 1);
}

int
cpfl_switch_hairpin_complq(struct cpfl_vport *cpfl_vport, bool on)
{
	struct idpf_vport *vport = &cpfl_vport->base;
	uint32_t type;
	int err, queue_id;

	type = VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION;
	queue_id = cpfl_vport->p2p_tx_complq->queue_id;
	err = idpf_vc_ena_dis_one_queue(vport, queue_id, type, on);

	return err;
}

int
cpfl_switch_hairpin_bufq(struct cpfl_vport *cpfl_vport, bool on)
{
	struct idpf_vport *vport = &cpfl_vport->base;
	uint32_t type;
	int err, queue_id;

	type = VIRTCHNL2_QUEUE_TYPE_RX_BUFFER;
	queue_id = cpfl_vport->p2p_rx_bufq->queue_id;
	err = idpf_vc_ena_dis_one_queue(vport, queue_id, type, on);

	return err;
}

int
cpfl_switch_hairpin_rxtx_queue(struct cpfl_vport *cpfl_vport, uint16_t logic_qid,
			       bool rx, bool on)
{
	struct idpf_vport *vport = &cpfl_vport->base;
	uint32_t type;
	int err, queue_id;

	type = rx ? VIRTCHNL2_QUEUE_TYPE_RX : VIRTCHNL2_QUEUE_TYPE_TX;

	if (type == VIRTCHNL2_QUEUE_TYPE_RX)
		queue_id = cpfl_hw_qid_get(cpfl_vport->p2p_q_chunks_info->rx_start_qid, logic_qid);
	else
		queue_id = cpfl_hw_qid_get(cpfl_vport->p2p_q_chunks_info->tx_start_qid, logic_qid);
	err = idpf_vc_ena_dis_one_queue(vport, queue_id, type, on);
	if (err)
		return err;

	return err;
}

static int
cpfl_alloc_split_p2p_rxq_mbufs(struct idpf_rx_queue *rxq)
{
	volatile struct virtchnl2_p2p_rx_buf_desc *rxd;
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
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

		rxd = &((volatile struct virtchnl2_p2p_rx_buf_desc *)(rxq->rx_ring))[i];
		rxd->reserve0 = 0;
		rxd->pkt_addr = dma_addr;
	}

	rxq->nb_rx_hold = 0;
	/* The value written in the RX buffer queue tail register, must be a multiple of 8.*/
	rxq->rx_tail = rxq->nb_rx_desc - CPFL_HAIRPIN_Q_TAIL_AUX_VALUE;

	return 0;
}

int
cpfl_rx_queue_init(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct cpfl_rx_queue *cpfl_rxq;
	struct idpf_rx_queue *rxq;
	uint16_t max_pkt_len;
	uint32_t frame_size;
	int err;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	cpfl_rxq = dev->data->rx_queues[rx_queue_id];
	rxq = &cpfl_rxq->base;

	if (rxq == NULL || !rxq->q_set) {
		PMD_DRV_LOG(ERR, "RX queue %u not available or setup",
					rx_queue_id);
		return -EINVAL;
	}

	frame_size = dev->data->mtu + CPFL_ETH_OVERHEAD;

	max_pkt_len =
	    RTE_MIN((uint32_t)CPFL_SUPPORT_CHAIN_NUM * rxq->rx_buf_len,
		    frame_size);

	rxq->max_pkt_len = max_pkt_len;
	if ((dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_SCATTER) ||
	    frame_size > rxq->rx_buf_len)
		dev->data->scattered_rx = 1;

	err = idpf_qc_ts_mbuf_register(rxq);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "fail to register timestamp mbuf %u",
			    rx_queue_id);
		return -EIO;
	}

	if (rxq->adapter->is_rx_singleq) {
		/* Single queue */
		err = idpf_qc_single_rxq_mbufs_alloc(rxq);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to allocate RX queue mbuf");
			return err;
		}

		rte_wmb();

		/* Init the RX tail register. */
		IDPF_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);
	} else {
		/* Split queue */
		if (cpfl_rxq->hairpin_info.hairpin_q) {
			err = cpfl_alloc_split_p2p_rxq_mbufs(rxq->bufq1);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Failed to allocate p2p RX buffer queue mbuf");
				return err;
			}
		} else {
			err = idpf_qc_split_rxq_mbufs_alloc(rxq->bufq1);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Failed to allocate RX buffer queue mbuf");
				return err;
			}
			err = idpf_qc_split_rxq_mbufs_alloc(rxq->bufq2);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Failed to allocate RX buffer queue mbuf");
				return err;
			}
		}

		rte_wmb();

		/* Init the RX tail register. */
		IDPF_PCI_REG_WRITE(rxq->bufq1->qrx_tail, rxq->bufq1->rx_tail);
		if (rxq->bufq2)
			IDPF_PCI_REG_WRITE(rxq->bufq2->qrx_tail, rxq->bufq2->rx_tail);
	}

	return err;
}

int
cpfl_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct cpfl_rx_queue *cpfl_rxq = dev->data->rx_queues[rx_queue_id];
	struct idpf_rx_queue *rxq = &cpfl_rxq->base;
	int err = 0;

	err = idpf_vc_rxq_config(vport, rxq);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Fail to configure Rx queue %u", rx_queue_id);
		return err;
	}

	err = cpfl_rx_queue_init(dev, rx_queue_id);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to init RX queue %u",
			    rx_queue_id);
		return err;
	}

	/* Ready to switch the queue on */
	err = idpf_vc_queue_switch(vport, rx_queue_id, true, true);
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
cpfl_tx_queue_init(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct cpfl_tx_queue *cpfl_txq;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	cpfl_txq = dev->data->tx_queues[tx_queue_id];

	/* Init the RX tail register. */
	IDPF_PCI_REG_WRITE(cpfl_txq->base.qtx_tail, 0);

	return 0;
}

int
cpfl_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct cpfl_tx_queue *cpfl_txq =
		dev->data->tx_queues[tx_queue_id];
	int err = 0;

	err = idpf_vc_txq_config(vport, &cpfl_txq->base);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Fail to configure Tx queue %u", tx_queue_id);
		return err;
	}

	err = cpfl_tx_queue_init(dev, tx_queue_id);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to init TX queue %u",
			    tx_queue_id);
		return err;
	}

	/* Ready to switch the queue on */
	err = idpf_vc_queue_switch(vport, tx_queue_id, false, true);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u on",
			    tx_queue_id);
	} else {
		cpfl_txq->base.q_started = true;
		dev->data->tx_queue_state[tx_queue_id] =
			RTE_ETH_QUEUE_STATE_STARTED;
	}

	return err;
}

int
cpfl_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct cpfl_rx_queue *cpfl_rxq;
	struct idpf_rx_queue *rxq;
	int err;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	cpfl_rxq = dev->data->rx_queues[rx_queue_id];
	if (cpfl_rxq->hairpin_info.hairpin_q)
		err = cpfl_switch_hairpin_rxtx_queue(cpfl_vport,
						     rx_queue_id - cpfl_vport->nb_data_txq,
						     true, false);
	else
		err = idpf_vc_queue_switch(vport, rx_queue_id, true, false);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u off",
			    rx_queue_id);
		return err;
	}

	rxq = &cpfl_rxq->base;
	rxq->q_started = false;
	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		rxq->ops->release_mbufs(rxq);
		idpf_qc_single_rx_queue_reset(rxq);
	} else {
		rxq->bufq1->ops->release_mbufs(rxq->bufq1);
		if (rxq->bufq2)
			rxq->bufq2->ops->release_mbufs(rxq->bufq2);
		if (cpfl_rxq->hairpin_info.hairpin_q) {
			cpfl_rx_hairpin_descq_reset(rxq);
			cpfl_rx_hairpin_bufq_reset(rxq->bufq1);
		} else {
			idpf_qc_split_rx_queue_reset(rxq);
		}
	}
	if (!cpfl_rxq->hairpin_info.hairpin_q)
		dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

int
cpfl_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
	struct cpfl_tx_queue *cpfl_txq;
	struct idpf_tx_queue *txq;
	int err;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	cpfl_txq = dev->data->tx_queues[tx_queue_id];

	if (cpfl_txq->hairpin_info.hairpin_q)
		err = cpfl_switch_hairpin_rxtx_queue(cpfl_vport,
						     tx_queue_id - cpfl_vport->nb_data_txq,
						     false, false);
	else
		err = idpf_vc_queue_switch(vport, tx_queue_id, false, false);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u off",
			    tx_queue_id);
		return err;
	}

	txq = &cpfl_txq->base;
	txq->q_started = false;
	txq->ops->release_mbufs(txq);
	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		idpf_qc_single_tx_queue_reset(txq);
	} else {
		if (cpfl_txq->hairpin_info.hairpin_q) {
			cpfl_tx_hairpin_descq_reset(txq);
			cpfl_tx_hairpin_complq_reset(txq->complq);
		} else {
			idpf_qc_split_tx_descq_reset(txq);
			idpf_qc_split_tx_complq_reset(txq->complq);
		}
	}

	if (!cpfl_txq->hairpin_info.hairpin_q)
		dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

void
cpfl_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	cpfl_rx_queue_release(dev->data->rx_queues[qid]);
}

void
cpfl_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	cpfl_tx_queue_release(dev->data->tx_queues[qid]);
}

void
cpfl_stop_queues(struct rte_eth_dev *dev)
{
	struct cpfl_vport *cpfl_vport =
		(struct cpfl_vport *)dev->data->dev_private;
	struct cpfl_rx_queue *cpfl_rxq;
	struct cpfl_tx_queue *cpfl_txq;
	int i;

	if (cpfl_vport->p2p_tx_complq != NULL) {
		if (cpfl_switch_hairpin_complq(cpfl_vport, false) != 0)
			PMD_DRV_LOG(ERR, "Failed to stop hairpin Tx complq");
	}

	if (cpfl_vport->p2p_rx_bufq != NULL) {
		if (cpfl_switch_hairpin_bufq(cpfl_vport, false) != 0)
			PMD_DRV_LOG(ERR, "Failed to stop hairpin Rx bufq");
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		cpfl_rxq = dev->data->rx_queues[i];
		if (cpfl_rxq == NULL)
			continue;

		if (cpfl_rx_queue_stop(dev, i) != 0)
			PMD_DRV_LOG(WARNING, "Fail to stop Rx queue %d", i);
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		cpfl_txq = dev->data->tx_queues[i];
		if (cpfl_txq == NULL)
			continue;

		if (cpfl_tx_queue_stop(dev, i) != 0)
			PMD_DRV_LOG(WARNING, "Fail to stop Tx queue %d", i);
	}
}

void
cpfl_set_rx_function(struct rte_eth_dev *dev)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
#ifdef RTE_ARCH_X86
	struct cpfl_rx_queue *cpfl_rxq;
	int i;

	if (cpfl_rx_vec_dev_check_default(dev) == CPFL_VECTOR_PATH &&
	    rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128) {
		vport->rx_vec_allowed = true;

		if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_512)
#ifdef CC_AVX512_SUPPORT
			if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) == 1 &&
			    rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512BW) == 1 &&
			    rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512DQ))
				vport->rx_use_avx512 = true;
#else
		PMD_DRV_LOG(NOTICE,
			    "AVX512 is not supported in build env");
#endif /* CC_AVX512_SUPPORT */
	} else {
		vport->rx_vec_allowed = false;
	}
#endif /* RTE_ARCH_X86 */

#ifdef RTE_ARCH_X86
	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
		if (vport->rx_vec_allowed) {
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				cpfl_rxq = dev->data->rx_queues[i];
				if (cpfl_rxq->hairpin_info.hairpin_q)
					continue;
				(void)idpf_qc_splitq_rx_vec_setup(&cpfl_rxq->base);
			}
#ifdef CC_AVX512_SUPPORT
			if (vport->rx_use_avx512) {
				PMD_DRV_LOG(NOTICE,
					    "Using Split AVX512 Vector Rx (port %d).",
					    dev->data->port_id);
				dev->rx_pkt_burst = idpf_dp_splitq_recv_pkts_avx512;
				return;
			}
#endif /* CC_AVX512_SUPPORT */
		}
		PMD_DRV_LOG(NOTICE,
			    "Using Split Scalar Rx (port %d).",
			    dev->data->port_id);
		dev->rx_pkt_burst = idpf_dp_splitq_recv_pkts;
	} else {
		if (vport->rx_vec_allowed) {
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				cpfl_rxq = dev->data->rx_queues[i];
				(void)idpf_qc_singleq_rx_vec_setup(&cpfl_rxq->base);
			}
#ifdef CC_AVX512_SUPPORT
			if (vport->rx_use_avx512) {
				PMD_DRV_LOG(NOTICE,
					    "Using Single AVX512 Vector Rx (port %d).",
					    dev->data->port_id);
				dev->rx_pkt_burst = idpf_dp_singleq_recv_pkts_avx512;
				return;
			}
#endif /* CC_AVX512_SUPPORT */
		}
		if (dev->data->scattered_rx) {
			PMD_DRV_LOG(NOTICE,
				    "Using Single Scalar Scatterd Rx (port %d).",
				    dev->data->port_id);
			dev->rx_pkt_burst = idpf_dp_singleq_recv_scatter_pkts;
			return;
		}
		PMD_DRV_LOG(NOTICE,
			    "Using Single Scalar Rx (port %d).",
			    dev->data->port_id);
		dev->rx_pkt_burst = idpf_dp_singleq_recv_pkts;
	}
#else
	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
		PMD_DRV_LOG(NOTICE,
			    "Using Split Scalar Rx (port %d).",
			    dev->data->port_id);
		dev->rx_pkt_burst = idpf_dp_splitq_recv_pkts;
	} else {
		if (dev->data->scattered_rx) {
			PMD_DRV_LOG(NOTICE,
				    "Using Single Scalar Scatterd Rx (port %d).",
				    dev->data->port_id);
			dev->rx_pkt_burst = idpf_dp_singleq_recv_scatter_pkts;
			return;
		}
		PMD_DRV_LOG(NOTICE,
			    "Using Single Scalar Rx (port %d).",
			    dev->data->port_id);
		dev->rx_pkt_burst = idpf_dp_singleq_recv_pkts;
	}
#endif /* RTE_ARCH_X86 */
}

void
cpfl_set_tx_function(struct rte_eth_dev *dev)
{
	struct cpfl_vport *cpfl_vport = dev->data->dev_private;
	struct idpf_vport *vport = &cpfl_vport->base;
#ifdef RTE_ARCH_X86
#ifdef CC_AVX512_SUPPORT
	struct cpfl_tx_queue *cpfl_txq;
	int i;
#endif /* CC_AVX512_SUPPORT */

	if (cpfl_tx_vec_dev_check_default(dev) == CPFL_VECTOR_PATH &&
	    rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128) {
		vport->tx_vec_allowed = true;
		if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_512)
#ifdef CC_AVX512_SUPPORT
		{
			if (rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512F) == 1 &&
			    rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX512BW) == 1)
				vport->tx_use_avx512 = true;
			if (vport->tx_use_avx512) {
				for (i = 0; i < dev->data->nb_tx_queues; i++) {
					cpfl_txq = dev->data->tx_queues[i];
					idpf_qc_tx_vec_avx512_setup(&cpfl_txq->base);
				}
			}
		}
#else
		PMD_DRV_LOG(NOTICE,
			    "AVX512 is not supported in build env");
#endif /* CC_AVX512_SUPPORT */
	} else {
		vport->tx_vec_allowed = false;
	}
#endif /* RTE_ARCH_X86 */

#ifdef RTE_ARCH_X86
	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
		if (vport->tx_vec_allowed) {
#ifdef CC_AVX512_SUPPORT
			if (vport->tx_use_avx512) {
				PMD_DRV_LOG(NOTICE,
					    "Using Split AVX512 Vector Tx (port %d).",
					    dev->data->port_id);
				dev->tx_pkt_burst = idpf_dp_splitq_xmit_pkts_avx512;
				dev->tx_pkt_prepare = idpf_dp_prep_pkts;
				return;
			}
#endif /* CC_AVX512_SUPPORT */
		}
		PMD_DRV_LOG(NOTICE,
			    "Using Split Scalar Tx (port %d).",
			    dev->data->port_id);
		dev->tx_pkt_burst = idpf_dp_splitq_xmit_pkts;
		dev->tx_pkt_prepare = idpf_dp_prep_pkts;
	} else {
		if (vport->tx_vec_allowed) {
#ifdef CC_AVX512_SUPPORT
			if (vport->tx_use_avx512) {
				for (i = 0; i < dev->data->nb_tx_queues; i++) {
					cpfl_txq = dev->data->tx_queues[i];
					if (cpfl_txq == NULL)
						continue;
					idpf_qc_tx_vec_avx512_setup(&cpfl_txq->base);
				}
				PMD_DRV_LOG(NOTICE,
					    "Using Single AVX512 Vector Tx (port %d).",
					    dev->data->port_id);
				dev->tx_pkt_burst = idpf_dp_singleq_xmit_pkts_avx512;
				dev->tx_pkt_prepare = idpf_dp_prep_pkts;
				return;
			}
#endif /* CC_AVX512_SUPPORT */
		}
		PMD_DRV_LOG(NOTICE,
			    "Using Single Scalar Tx (port %d).",
			    dev->data->port_id);
		dev->tx_pkt_burst = idpf_dp_singleq_xmit_pkts;
		dev->tx_pkt_prepare = idpf_dp_prep_pkts;
	}
#else
	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SPLIT) {
		PMD_DRV_LOG(NOTICE,
			    "Using Split Scalar Tx (port %d).",
			    dev->data->port_id);
		dev->tx_pkt_burst = idpf_dp_splitq_xmit_pkts;
		dev->tx_pkt_prepare = idpf_dp_prep_pkts;
	} else {
		PMD_DRV_LOG(NOTICE,
			    "Using Single Scalar Tx (port %d).",
			    dev->data->port_id);
		dev->tx_pkt_burst = idpf_dp_singleq_xmit_pkts;
		dev->tx_pkt_prepare = idpf_dp_prep_pkts;
	}
#endif /* RTE_ARCH_X86 */
}
