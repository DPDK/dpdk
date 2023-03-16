/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <ethdev_driver.h>
#include <rte_net.h>
#include <rte_vect.h>

#include "cpfl_ethdev.h"
#include "cpfl_rxtx.h"
#include "cpfl_rxtx_vec_common.h"

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
	struct idpf_vport *vport = dev->data->dev_private;
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
	bufq->rx_buf_len = len;

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

int
cpfl_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		    uint16_t nb_desc, unsigned int socket_id,
		    const struct rte_eth_rxconf *rx_conf,
		    struct rte_mempool *mp)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *base = vport->adapter;
	struct idpf_hw *hw = &base->hw;
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
		idpf_qc_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Setup Rx queue */
	rxq = rte_zmalloc_socket("cpfl rxq",
				 sizeof(struct idpf_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (rxq == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for rx queue data structure");
		ret = -ENOMEM;
		goto err_rxq_alloc;
	}

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

	rxq->q_set = true;
	dev->data->rx_queues[queue_idx] = rxq;

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

	/* Free memory if needed. */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		idpf_qc_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

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
	txq->ops = &def_txq_ops;
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

int
cpfl_rx_queue_init(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct idpf_rx_queue *rxq;
	uint16_t max_pkt_len;
	uint32_t frame_size;
	int err;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	rxq = dev->data->rx_queues[rx_queue_id];

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

		rte_wmb();

		/* Init the RX tail register. */
		IDPF_PCI_REG_WRITE(rxq->bufq1->qrx_tail, rxq->bufq1->rx_tail);
		IDPF_PCI_REG_WRITE(rxq->bufq2->qrx_tail, rxq->bufq2->rx_tail);
	}

	return err;
}

int
cpfl_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_rx_queue *rxq =
		dev->data->rx_queues[rx_queue_id];
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
	struct idpf_tx_queue *txq;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	txq = dev->data->tx_queues[tx_queue_id];

	/* Init the RX tail register. */
	IDPF_PCI_REG_WRITE(txq->qtx_tail, 0);

	return 0;
}

int
cpfl_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_tx_queue *txq =
		dev->data->tx_queues[tx_queue_id];
	int err = 0;

	err = idpf_vc_txq_config(vport, txq);
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
		txq->q_started = true;
		dev->data->tx_queue_state[tx_queue_id] =
			RTE_ETH_QUEUE_STATE_STARTED;
	}

	return err;
}

int
cpfl_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_rx_queue *rxq;
	int err;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	err = idpf_vc_queue_switch(vport, rx_queue_id, true, false);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u off",
			    rx_queue_id);
		return err;
	}

	rxq = dev->data->rx_queues[rx_queue_id];
	rxq->q_started = false;
	if (vport->rxq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		rxq->ops->release_mbufs(rxq);
		idpf_qc_single_rx_queue_reset(rxq);
	} else {
		rxq->bufq1->ops->release_mbufs(rxq->bufq1);
		rxq->bufq2->ops->release_mbufs(rxq->bufq2);
		idpf_qc_split_rx_queue_reset(rxq);
	}
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

int
cpfl_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_tx_queue *txq;
	int err;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	err = idpf_vc_queue_switch(vport, tx_queue_id, false, false);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to switch TX queue %u off",
			    tx_queue_id);
		return err;
	}

	txq = dev->data->tx_queues[tx_queue_id];
	txq->q_started = false;
	txq->ops->release_mbufs(txq);
	if (vport->txq_model == VIRTCHNL2_QUEUE_MODEL_SINGLE) {
		idpf_qc_single_tx_queue_reset(txq);
	} else {
		idpf_qc_split_tx_descq_reset(txq);
		idpf_qc_split_tx_complq_reset(txq->complq);
	}
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

void
cpfl_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	idpf_qc_rx_queue_release(dev->data->rx_queues[qid]);
}

void
cpfl_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	idpf_qc_tx_queue_release(dev->data->tx_queues[qid]);
}

void
cpfl_stop_queues(struct rte_eth_dev *dev)
{
	struct idpf_rx_queue *rxq;
	struct idpf_tx_queue *txq;
	int i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq == NULL)
			continue;

		if (cpfl_rx_queue_stop(dev, i) != 0)
			PMD_DRV_LOG(WARNING, "Fail to stop Rx queue %d", i);
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq == NULL)
			continue;

		if (cpfl_tx_queue_stop(dev, i) != 0)
			PMD_DRV_LOG(WARNING, "Fail to stop Tx queue %d", i);
	}
}

void
cpfl_set_rx_function(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
#ifdef RTE_ARCH_X86
	struct idpf_rx_queue *rxq;
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
				rxq = dev->data->rx_queues[i];
				(void)idpf_qc_splitq_rx_vec_setup(rxq);
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
				rxq = dev->data->rx_queues[i];
				(void)idpf_qc_singleq_rx_vec_setup(rxq);
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
	struct idpf_vport *vport = dev->data->dev_private;
#ifdef RTE_ARCH_X86
#ifdef CC_AVX512_SUPPORT
	struct idpf_tx_queue *txq;
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
					txq = dev->data->tx_queues[i];
					idpf_qc_tx_vec_avx512_setup(txq);
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
					txq = dev->data->tx_queues[i];
					if (txq == NULL)
						continue;
					idpf_qc_tx_vec_avx512_setup(txq);
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
