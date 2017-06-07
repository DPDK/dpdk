/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#include <rte_net.h>
#include "qede_rxtx.h"

static inline int qede_alloc_rx_buffer(struct qede_rx_queue *rxq)
{
	struct rte_mbuf *new_mb = NULL;
	struct eth_rx_bd *rx_bd;
	dma_addr_t mapping;
	uint16_t idx = rxq->sw_rx_prod & NUM_RX_BDS(rxq);

	new_mb = rte_mbuf_raw_alloc(rxq->mb_pool);
	if (unlikely(!new_mb)) {
		PMD_RX_LOG(ERR, rxq,
			   "Failed to allocate rx buffer "
			   "sw_rx_prod %u sw_rx_cons %u mp entries %u free %u",
			   idx, rxq->sw_rx_cons & NUM_RX_BDS(rxq),
			   rte_mempool_avail_count(rxq->mb_pool),
			   rte_mempool_in_use_count(rxq->mb_pool));
		return -ENOMEM;
	}
	rxq->sw_rx_ring[idx].mbuf = new_mb;
	rxq->sw_rx_ring[idx].page_offset = 0;
	mapping = rte_mbuf_data_dma_addr_default(new_mb);
	/* Advance PROD and get BD pointer */
	rx_bd = (struct eth_rx_bd *)ecore_chain_produce(&rxq->rx_bd_ring);
	rx_bd->addr.hi = rte_cpu_to_le_32(U64_HI(mapping));
	rx_bd->addr.lo = rte_cpu_to_le_32(U64_LO(mapping));
	rxq->sw_rx_prod++;
	return 0;
}

int
qede_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		    uint16_t nb_desc, unsigned int socket_id,
		    __rte_unused const struct rte_eth_rxconf *rx_conf,
		    struct rte_mempool *mp)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	struct qede_rx_queue *rxq;
	uint16_t max_rx_pkt_len;
	uint16_t bufsz;
	size_t size;
	int rc;

	PMD_INIT_FUNC_TRACE(edev);

	/* Note: Ring size/align is controlled by struct rte_eth_desc_lim */
	if (!rte_is_power_of_2(nb_desc)) {
		DP_ERR(edev, "Ring size %u is not power of 2\n",
			  nb_desc);
		return -EINVAL;
	}

	/* Free memory prior to re-allocation if needed... */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		qede_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* First allocate the rx queue data structure */
	rxq = rte_zmalloc_socket("qede_rx_queue", sizeof(struct qede_rx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);

	if (!rxq) {
		DP_ERR(edev, "Unable to allocate memory for rxq on socket %u",
			  socket_id);
		return -ENOMEM;
	}

	rxq->qdev = qdev;
	rxq->mb_pool = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->queue_id = queue_idx;
	rxq->port_id = dev->data->port_id;
	max_rx_pkt_len = (uint16_t)rxmode->max_rx_pkt_len;
	qdev->mtu = max_rx_pkt_len;

	/* Fix up RX buffer size */
	bufsz = (uint16_t)rte_pktmbuf_data_room_size(mp) - RTE_PKTMBUF_HEADROOM;
	if ((rxmode->enable_scatter)			||
	    (max_rx_pkt_len + QEDE_ETH_OVERHEAD) > bufsz) {
		if (!dev->data->scattered_rx) {
			DP_INFO(edev, "Forcing scatter-gather mode\n");
			dev->data->scattered_rx = 1;
		}
	}
	if (dev->data->scattered_rx)
		rxq->rx_buf_size = bufsz + QEDE_ETH_OVERHEAD;
	else
		rxq->rx_buf_size = qdev->mtu + QEDE_ETH_OVERHEAD;
	/* Align to cache-line size if needed */
	rxq->rx_buf_size = QEDE_CEIL_TO_CACHE_LINE_SIZE(rxq->rx_buf_size);

	DP_INFO(edev, "mtu %u mbufsz %u bd_max_bytes %u scatter_mode %d\n",
		qdev->mtu, bufsz, rxq->rx_buf_size, dev->data->scattered_rx);

	/* Allocate the parallel driver ring for Rx buffers */
	size = sizeof(*rxq->sw_rx_ring) * rxq->nb_rx_desc;
	rxq->sw_rx_ring = rte_zmalloc_socket("sw_rx_ring", size,
					     RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq->sw_rx_ring) {
		DP_NOTICE(edev, false,
			  "Unable to alloc memory for sw_rx_ring on socket %u\n",
			  socket_id);
		rte_free(rxq);
		return -ENOMEM;
	}

	/* Allocate FW Rx ring  */
	rc = qdev->ops->common->chain_alloc(edev,
					    ECORE_CHAIN_USE_TO_CONSUME_PRODUCE,
					    ECORE_CHAIN_MODE_NEXT_PTR,
					    ECORE_CHAIN_CNT_TYPE_U16,
					    rxq->nb_rx_desc,
					    sizeof(struct eth_rx_bd),
					    &rxq->rx_bd_ring,
					    NULL);

	if (rc != ECORE_SUCCESS) {
		DP_NOTICE(edev, false,
			  "Unable to alloc memory for rxbd ring on socket %u\n",
			  socket_id);
		rte_free(rxq->sw_rx_ring);
		rte_free(rxq);
		return -ENOMEM;
	}

	/* Allocate FW completion ring */
	rc = qdev->ops->common->chain_alloc(edev,
					    ECORE_CHAIN_USE_TO_CONSUME,
					    ECORE_CHAIN_MODE_PBL,
					    ECORE_CHAIN_CNT_TYPE_U16,
					    rxq->nb_rx_desc,
					    sizeof(union eth_rx_cqe),
					    &rxq->rx_comp_ring,
					    NULL);

	if (rc != ECORE_SUCCESS) {
		DP_NOTICE(edev, false,
			  "Unable to alloc memory for cqe ring on socket %u\n",
			  socket_id);
		/* TBD: Freeing RX BD ring */
		rte_free(rxq->sw_rx_ring);
		rte_free(rxq);
		return -ENOMEM;
	}

	dev->data->rx_queues[queue_idx] = rxq;
	qdev->fp_array[queue_idx].rxq = rxq;

	DP_INFO(edev, "rxq %d num_desc %u rx_buf_size=%u socket %u\n",
		  queue_idx, nb_desc, qdev->mtu, socket_id);

	return 0;
}

static void
qede_rx_queue_reset(__rte_unused struct qede_dev *qdev,
		    struct qede_rx_queue *rxq)
{
	DP_INFO(&qdev->edev, "Reset RX queue %u\n", rxq->queue_id);
	ecore_chain_reset(&rxq->rx_bd_ring);
	ecore_chain_reset(&rxq->rx_comp_ring);
	rxq->sw_rx_prod = 0;
	rxq->sw_rx_cons = 0;
	*rxq->hw_cons_ptr = 0;
}

static void qede_rx_queue_release_mbufs(struct qede_rx_queue *rxq)
{
	uint16_t i;

	if (rxq->sw_rx_ring) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_rx_ring[i].mbuf) {
				rte_pktmbuf_free(rxq->sw_rx_ring[i].mbuf);
				rxq->sw_rx_ring[i].mbuf = NULL;
			}
		}
	}
}

void qede_rx_queue_release(void *rx_queue)
{
	struct qede_rx_queue *rxq = rx_queue;

	if (rxq) {
		qede_rx_queue_release_mbufs(rxq);
		rte_free(rxq->sw_rx_ring);
		rte_free(rxq);
	}
}

/* Stops a given RX queue in the HW */
static int qede_rx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_hwfn *p_hwfn;
	struct qede_rx_queue *rxq;
	int hwfn_index;
	int rc;

	if (rx_queue_id < eth_dev->data->nb_rx_queues) {
		rxq = eth_dev->data->rx_queues[rx_queue_id];
		hwfn_index = rx_queue_id % edev->num_hwfns;
		p_hwfn = &edev->hwfns[hwfn_index];
		rc = ecore_eth_rx_queue_stop(p_hwfn, rxq->handle,
				true, false);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "RX queue %u stop fails\n", rx_queue_id);
			return -1;
		}
		qede_rx_queue_release_mbufs(rxq);
		qede_rx_queue_reset(qdev, rxq);
		eth_dev->data->rx_queue_state[rx_queue_id] =
			RTE_ETH_QUEUE_STATE_STOPPED;
		DP_INFO(edev, "RX queue %u stopped\n", rx_queue_id);
	} else {
		DP_ERR(edev, "RX queue %u is not in range\n", rx_queue_id);
		rc = -EINVAL;
	}

	return rc;
}

int
qede_tx_queue_setup(struct rte_eth_dev *dev,
		    uint16_t queue_idx,
		    uint16_t nb_desc,
		    unsigned int socket_id,
		    const struct rte_eth_txconf *tx_conf)
{
	struct qede_dev *qdev = dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;
	struct qede_tx_queue *txq;
	int rc;

	PMD_INIT_FUNC_TRACE(edev);

	if (!rte_is_power_of_2(nb_desc)) {
		DP_ERR(edev, "Ring size %u is not power of 2\n",
		       nb_desc);
		return -EINVAL;
	}

	/* Free memory prior to re-allocation if needed... */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		qede_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	txq = rte_zmalloc_socket("qede_tx_queue", sizeof(struct qede_tx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);

	if (txq == NULL) {
		DP_ERR(edev,
		       "Unable to allocate memory for txq on socket %u",
		       socket_id);
		return -ENOMEM;
	}

	txq->nb_tx_desc = nb_desc;
	txq->qdev = qdev;
	txq->port_id = dev->data->port_id;

	rc = qdev->ops->common->chain_alloc(edev,
					    ECORE_CHAIN_USE_TO_CONSUME_PRODUCE,
					    ECORE_CHAIN_MODE_PBL,
					    ECORE_CHAIN_CNT_TYPE_U16,
					    txq->nb_tx_desc,
					    sizeof(union eth_tx_bd_types),
					    &txq->tx_pbl,
					    NULL);
	if (rc != ECORE_SUCCESS) {
		DP_ERR(edev,
		       "Unable to allocate memory for txbd ring on socket %u",
		       socket_id);
		qede_tx_queue_release(txq);
		return -ENOMEM;
	}

	/* Allocate software ring */
	txq->sw_tx_ring = rte_zmalloc_socket("txq->sw_tx_ring",
					     (sizeof(struct qede_tx_entry) *
					      txq->nb_tx_desc),
					     RTE_CACHE_LINE_SIZE, socket_id);

	if (!txq->sw_tx_ring) {
		DP_ERR(edev,
		       "Unable to allocate memory for txbd ring on socket %u",
		       socket_id);
		qede_tx_queue_release(txq);
		return -ENOMEM;
	}

	txq->queue_id = queue_idx;

	txq->nb_tx_avail = txq->nb_tx_desc;

	txq->tx_free_thresh =
	    tx_conf->tx_free_thresh ? tx_conf->tx_free_thresh :
	    (txq->nb_tx_desc - QEDE_DEFAULT_TX_FREE_THRESH);

	dev->data->tx_queues[queue_idx] = txq;
	qdev->fp_array[queue_idx].txq = txq;

	DP_INFO(edev,
		  "txq %u num_desc %u tx_free_thresh %u socket %u\n",
		  queue_idx, nb_desc, txq->tx_free_thresh, socket_id);

	return 0;
}

static void
qede_tx_queue_reset(__rte_unused struct qede_dev *qdev,
		    struct qede_tx_queue *txq)
{
	DP_INFO(&qdev->edev, "Reset TX queue %u\n", txq->queue_id);
	ecore_chain_reset(&txq->tx_pbl);
	txq->sw_tx_cons = 0;
	txq->sw_tx_prod = 0;
	*txq->hw_cons_ptr = 0;
}

static void qede_tx_queue_release_mbufs(struct qede_tx_queue *txq)
{
	uint16_t i;

	if (txq->sw_tx_ring) {
		for (i = 0; i < txq->nb_tx_desc; i++) {
			if (txq->sw_tx_ring[i].mbuf) {
				rte_pktmbuf_free(txq->sw_tx_ring[i].mbuf);
				txq->sw_tx_ring[i].mbuf = NULL;
			}
		}
	}
}

void qede_tx_queue_release(void *tx_queue)
{
	struct qede_tx_queue *txq = tx_queue;

	if (txq) {
		qede_tx_queue_release_mbufs(txq);
		rte_free(txq->sw_tx_ring);
		rte_free(txq);
	}
}

/* This function allocates fast-path status block memory */
static int
qede_alloc_mem_sb(struct qede_dev *qdev, struct ecore_sb_info *sb_info,
		  uint16_t sb_id)
{
	struct ecore_dev *edev = &qdev->edev;
	struct status_block *sb_virt;
	dma_addr_t sb_phys;
	int rc;

	sb_virt = OSAL_DMA_ALLOC_COHERENT(edev, &sb_phys, sizeof(*sb_virt));

	if (!sb_virt) {
		DP_ERR(edev, "Status block allocation failed\n");
		return -ENOMEM;
	}

	rc = qdev->ops->common->sb_init(edev, sb_info, sb_virt,
					sb_phys, sb_id);
	if (rc) {
		DP_ERR(edev, "Status block initialization failed\n");
		/* TBD: No dma_free_coherent possible */
		return rc;
	}

	return 0;
}

int qede_alloc_fp_resc(struct qede_dev *qdev)
{
	struct ecore_dev *edev = &qdev->edev;
	struct qede_fastpath *fp;
	uint32_t num_sbs;
	uint16_t sb_idx;

	if (IS_VF(edev))
		ecore_vf_get_num_sbs(ECORE_LEADING_HWFN(edev), &num_sbs);
	else
		num_sbs = ecore_cxt_get_proto_cid_count
			  (ECORE_LEADING_HWFN(edev), PROTOCOLID_ETH, NULL);

	if (num_sbs == 0) {
		DP_ERR(edev, "No status blocks available\n");
		return -EINVAL;
	}

	qdev->fp_array = rte_calloc("fp", QEDE_RXTX_MAX(qdev),
				sizeof(*qdev->fp_array), RTE_CACHE_LINE_SIZE);

	if (!qdev->fp_array) {
		DP_ERR(edev, "fp array allocation failed\n");
		return -ENOMEM;
	}

	memset((void *)qdev->fp_array, 0, QEDE_RXTX_MAX(qdev) *
			sizeof(*qdev->fp_array));

	for (sb_idx = 0; sb_idx < QEDE_RXTX_MAX(qdev); sb_idx++) {
		fp = &qdev->fp_array[sb_idx];
		fp->sb_info = rte_calloc("sb", 1, sizeof(struct ecore_sb_info),
				RTE_CACHE_LINE_SIZE);
		if (!fp->sb_info) {
			DP_ERR(edev, "FP sb_info allocation fails\n");
			return -1;
		}
		if (qede_alloc_mem_sb(qdev, fp->sb_info, sb_idx)) {
			DP_ERR(edev, "FP status block allocation fails\n");
			return -1;
		}
		DP_INFO(edev, "sb_info idx 0x%x initialized\n",
				fp->sb_info->igu_sb_id);
	}

	return 0;
}

void qede_dealloc_fp_resc(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	__rte_unused struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct qede_fastpath *fp;
	uint16_t sb_idx;

	PMD_INIT_FUNC_TRACE(edev);

	for (sb_idx = 0; sb_idx < QEDE_RXTX_MAX(qdev); sb_idx++) {
		fp = &qdev->fp_array[sb_idx];
		DP_INFO(edev, "Free sb_info index 0x%x\n",
				fp->sb_info->igu_sb_id);
		if (fp->sb_info)
			rte_free(fp->sb_info);
		fp->sb_info = NULL;
	}
	if (qdev->fp_array)
		rte_free(qdev->fp_array);
	qdev->fp_array = NULL;
}

static inline void
qede_update_rx_prod(__rte_unused struct qede_dev *edev,
		    struct qede_rx_queue *rxq)
{
	uint16_t bd_prod = ecore_chain_get_prod_idx(&rxq->rx_bd_ring);
	uint16_t cqe_prod = ecore_chain_get_prod_idx(&rxq->rx_comp_ring);
	struct eth_rx_prod_data rx_prods = { 0 };

	/* Update producers */
	rx_prods.bd_prod = rte_cpu_to_le_16(bd_prod);
	rx_prods.cqe_prod = rte_cpu_to_le_16(cqe_prod);

	/* Make sure that the BD and SGE data is updated before updating the
	 * producers since FW might read the BD/SGE right after the producer
	 * is updated.
	 */
	rte_wmb();

	internal_ram_wr(rxq->hw_rxq_prod_addr, sizeof(rx_prods),
			(uint32_t *)&rx_prods);

	/* mmiowb is needed to synchronize doorbell writes from more than one
	 * processor. It guarantees that the write arrives to the device before
	 * the napi lock is released and another qede_poll is called (possibly
	 * on another CPU). Without this barrier, the next doorbell can bypass
	 * this doorbell. This is applicable to IA64/Altix systems.
	 */
	rte_wmb();

	PMD_RX_LOG(DEBUG, rxq, "bd_prod %u  cqe_prod %u", bd_prod, cqe_prod);
}

/* Starts a given RX queue in HW */
static int
qede_rx_queue_start(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_queue_start_common_params params;
	struct ecore_rxq_start_ret_params ret_params;
	struct qede_rx_queue *rxq;
	struct qede_fastpath *fp;
	struct ecore_hwfn *p_hwfn;
	dma_addr_t p_phys_table;
	uint16_t page_cnt;
	uint16_t j;
	int hwfn_index;
	int rc;

	if (rx_queue_id < eth_dev->data->nb_rx_queues) {
		fp = &qdev->fp_array[rx_queue_id];
		rxq = eth_dev->data->rx_queues[rx_queue_id];
		/* Allocate buffers for the Rx ring */
		for (j = 0; j < rxq->nb_rx_desc; j++) {
			rc = qede_alloc_rx_buffer(rxq);
			if (rc) {
				DP_ERR(edev, "RX buffer allocation failed"
						" for rxq = %u\n", rx_queue_id);
				return -ENOMEM;
			}
		}
		/* disable interrupts */
		ecore_sb_ack(fp->sb_info, IGU_INT_DISABLE, 0);
		/* Prepare ramrod */
		memset(&params, 0, sizeof(params));
		params.queue_id = rx_queue_id;
		params.vport_id = 0;
		params.sb = fp->sb_info->igu_sb_id;
		DP_INFO(edev, "rxq %u igu_sb_id 0x%x\n",
				fp->rxq->queue_id, fp->sb_info->igu_sb_id);
		params.sb_idx = RX_PI;
		hwfn_index = rx_queue_id % edev->num_hwfns;
		p_hwfn = &edev->hwfns[hwfn_index];
		p_phys_table = ecore_chain_get_pbl_phys(&fp->rxq->rx_comp_ring);
		page_cnt = ecore_chain_get_page_cnt(&fp->rxq->rx_comp_ring);
		memset(&ret_params, 0, sizeof(ret_params));
		rc = ecore_eth_rx_queue_start(p_hwfn,
				p_hwfn->hw_info.opaque_fid,
				&params, fp->rxq->rx_buf_size,
				fp->rxq->rx_bd_ring.p_phys_addr,
				p_phys_table, page_cnt,
				&ret_params);
		if (rc) {
			DP_ERR(edev, "RX queue %u could not be started, rc = %d\n",
					rx_queue_id, rc);
			return -1;
		}
		/* Update with the returned parameters */
		fp->rxq->hw_rxq_prod_addr = ret_params.p_prod;
		fp->rxq->handle = ret_params.p_handle;

		fp->rxq->hw_cons_ptr = &fp->sb_info->sb_virt->pi_array[RX_PI];
		qede_update_rx_prod(qdev, fp->rxq);
		eth_dev->data->rx_queue_state[rx_queue_id] =
			RTE_ETH_QUEUE_STATE_STARTED;
		DP_INFO(edev, "RX queue %u started\n", rx_queue_id);
	} else {
		DP_ERR(edev, "RX queue %u is not in range\n", rx_queue_id);
		rc = -EINVAL;
	}

	return rc;
}

static int
qede_tx_queue_start(struct rte_eth_dev *eth_dev, uint16_t tx_queue_id)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_queue_start_common_params params;
	struct ecore_txq_start_ret_params ret_params;
	struct ecore_hwfn *p_hwfn;
	dma_addr_t p_phys_table;
	struct qede_tx_queue *txq;
	struct qede_fastpath *fp;
	uint16_t page_cnt;
	int hwfn_index;
	int rc;

	if (tx_queue_id < eth_dev->data->nb_tx_queues) {
		txq = eth_dev->data->tx_queues[tx_queue_id];
		fp = &qdev->fp_array[tx_queue_id];
		memset(&params, 0, sizeof(params));
		params.queue_id = tx_queue_id;
		params.vport_id = 0;
		params.sb = fp->sb_info->igu_sb_id;
		DP_INFO(edev, "txq %u igu_sb_id 0x%x\n",
				fp->txq->queue_id, fp->sb_info->igu_sb_id);
		params.sb_idx = TX_PI(0); /* tc = 0 */
		p_phys_table = ecore_chain_get_pbl_phys(&txq->tx_pbl);
		page_cnt = ecore_chain_get_page_cnt(&txq->tx_pbl);
		hwfn_index = tx_queue_id % edev->num_hwfns;
		p_hwfn = &edev->hwfns[hwfn_index];
		if (qdev->dev_info.is_legacy)
			fp->txq->is_legacy = true;
		rc = ecore_eth_tx_queue_start(p_hwfn,
				p_hwfn->hw_info.opaque_fid,
				&params, 0 /* tc */,
				p_phys_table, page_cnt,
				&ret_params);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "TX queue %u couldn't be started, rc=%d\n",
					tx_queue_id, rc);
			return -1;
		}
		txq->doorbell_addr = ret_params.p_doorbell;
		txq->handle = ret_params.p_handle;

		txq->hw_cons_ptr = &fp->sb_info->sb_virt->pi_array[TX_PI(0)];
		SET_FIELD(txq->tx_db.data.params, ETH_DB_DATA_DEST,
				DB_DEST_XCM);
		SET_FIELD(txq->tx_db.data.params, ETH_DB_DATA_AGG_CMD,
				DB_AGG_CMD_SET);
		SET_FIELD(txq->tx_db.data.params,
				ETH_DB_DATA_AGG_VAL_SEL,
				DQ_XCM_ETH_TX_BD_PROD_CMD);
		txq->tx_db.data.agg_flags = DQ_XCM_ETH_DQ_CF_CMD;
		eth_dev->data->tx_queue_state[tx_queue_id] =
			RTE_ETH_QUEUE_STATE_STARTED;
		DP_INFO(edev, "TX queue %u started\n", tx_queue_id);
	} else {
		DP_ERR(edev, "TX queue %u is not in range\n", tx_queue_id);
		rc = -EINVAL;
	}

	return rc;
}

static inline void
qede_free_tx_pkt(struct qede_tx_queue *txq)
{
	struct rte_mbuf *mbuf;
	uint16_t nb_segs;
	uint16_t idx;

	idx = TX_CONS(txq);
	mbuf = txq->sw_tx_ring[idx].mbuf;
	if (mbuf) {
		nb_segs = mbuf->nb_segs;
		PMD_TX_LOG(DEBUG, txq, "nb_segs to free %u\n", nb_segs);
		while (nb_segs) {
			/* It's like consuming rxbuf in recv() */
			ecore_chain_consume(&txq->tx_pbl);
			txq->nb_tx_avail++;
			nb_segs--;
		}
		rte_pktmbuf_free(mbuf);
		txq->sw_tx_ring[idx].mbuf = NULL;
		txq->sw_tx_cons++;
		PMD_TX_LOG(DEBUG, txq, "Freed tx packet\n");
	} else {
		ecore_chain_consume(&txq->tx_pbl);
		txq->nb_tx_avail++;
	}
}

static inline void
qede_process_tx_compl(__rte_unused struct ecore_dev *edev,
		      struct qede_tx_queue *txq)
{
	uint16_t hw_bd_cons;
#ifdef RTE_LIBRTE_QEDE_DEBUG_TX
	uint16_t sw_tx_cons;
#endif

	rte_compiler_barrier();
	hw_bd_cons = rte_le_to_cpu_16(*txq->hw_cons_ptr);
#ifdef RTE_LIBRTE_QEDE_DEBUG_TX
	sw_tx_cons = ecore_chain_get_cons_idx(&txq->tx_pbl);
	PMD_TX_LOG(DEBUG, txq, "Tx Completions = %u\n",
		   abs(hw_bd_cons - sw_tx_cons));
#endif
	while (hw_bd_cons !=  ecore_chain_get_cons_idx(&txq->tx_pbl))
		qede_free_tx_pkt(txq);
}


static int qede_drain_txq(struct qede_dev *qdev,
			  struct qede_tx_queue *txq, bool allow_drain)
{
	struct ecore_dev *edev = &qdev->edev;
	int rc, cnt = 1000;

	while (txq->sw_tx_cons != txq->sw_tx_prod) {
		qede_process_tx_compl(edev, txq);
		if (!cnt) {
			if (allow_drain) {
				DP_ERR(edev, "Tx queue[%u] is stuck,"
					  "requesting MCP to drain\n",
					  txq->queue_id);
				rc = qdev->ops->common->drain(edev);
				if (rc)
					return rc;
				return qede_drain_txq(qdev, txq, false);
			}
			DP_ERR(edev, "Timeout waiting for tx queue[%d]:"
				  "PROD=%d, CONS=%d\n",
				  txq->queue_id, txq->sw_tx_prod,
				  txq->sw_tx_cons);
			return -1;
		}
		cnt--;
		DELAY(1000);
		rte_compiler_barrier();
	}

	/* FW finished processing, wait for HW to transmit all tx packets */
	DELAY(2000);

	return 0;
}


/* Stops a given TX queue in the HW */
static int qede_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t tx_queue_id)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	struct ecore_dev *edev = QEDE_INIT_EDEV(qdev);
	struct ecore_hwfn *p_hwfn;
	struct qede_tx_queue *txq;
	int hwfn_index;
	int rc;

	if (tx_queue_id < eth_dev->data->nb_tx_queues) {
		txq = eth_dev->data->tx_queues[tx_queue_id];
		/* Drain txq */
		if (qede_drain_txq(qdev, txq, true))
			return -1; /* For the lack of retcodes */
		/* Stop txq */
		hwfn_index = tx_queue_id % edev->num_hwfns;
		p_hwfn = &edev->hwfns[hwfn_index];
		rc = ecore_eth_tx_queue_stop(p_hwfn, txq->handle);
		if (rc != ECORE_SUCCESS) {
			DP_ERR(edev, "TX queue %u stop fails\n", tx_queue_id);
			return -1;
		}
		qede_tx_queue_release_mbufs(txq);
		qede_tx_queue_reset(qdev, txq);
		eth_dev->data->tx_queue_state[tx_queue_id] =
			RTE_ETH_QUEUE_STATE_STOPPED;
		DP_INFO(edev, "TX queue %u stopped\n", tx_queue_id);
	} else {
		DP_ERR(edev, "TX queue %u is not in range\n", tx_queue_id);
		rc = -EINVAL;
	}

	return rc;
}

int qede_start_queues(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	uint8_t id;
	int rc;

	for_each_rss(id) {
		rc = qede_rx_queue_start(eth_dev, id);
		if (rc != ECORE_SUCCESS)
			return -1;
	}

	for_each_tss(id) {
		rc = qede_tx_queue_start(eth_dev, id);
		if (rc != ECORE_SUCCESS)
			return -1;
	}

	return rc;
}

void qede_stop_queues(struct rte_eth_dev *eth_dev)
{
	struct qede_dev *qdev = QEDE_INIT_QDEV(eth_dev);
	uint8_t id;

	/* Stopping RX/TX queues */
	for_each_tss(id) {
		qede_tx_queue_stop(eth_dev, id);
	}

	for_each_rss(id) {
		qede_rx_queue_stop(eth_dev, id);
	}
}

static bool qede_tunn_exist(uint16_t flag)
{
	return !!((PARSING_AND_ERR_FLAGS_TUNNELEXIST_MASK <<
		    PARSING_AND_ERR_FLAGS_TUNNELEXIST_SHIFT) & flag);
}

/*
 * qede_check_tunn_csum_l4:
 * Returns:
 * 1 : If L4 csum is enabled AND if the validation has failed.
 * 0 : Otherwise
 */
static inline uint8_t qede_check_tunn_csum_l4(uint16_t flag)
{
	if ((PARSING_AND_ERR_FLAGS_TUNNELL4CHKSMWASCALCULATED_MASK <<
	     PARSING_AND_ERR_FLAGS_TUNNELL4CHKSMWASCALCULATED_SHIFT) & flag)
		return !!((PARSING_AND_ERR_FLAGS_TUNNELL4CHKSMERROR_MASK <<
			PARSING_AND_ERR_FLAGS_TUNNELL4CHKSMERROR_SHIFT) & flag);

	return 0;
}

static inline uint8_t qede_check_notunn_csum_l4(uint16_t flag)
{
	if ((PARSING_AND_ERR_FLAGS_L4CHKSMWASCALCULATED_MASK <<
	     PARSING_AND_ERR_FLAGS_L4CHKSMWASCALCULATED_SHIFT) & flag)
		return !!((PARSING_AND_ERR_FLAGS_L4CHKSMERROR_MASK <<
			   PARSING_AND_ERR_FLAGS_L4CHKSMERROR_SHIFT) & flag);

	return 0;
}

static inline uint32_t qede_rx_cqe_to_pkt_type(uint16_t flags)
{
	uint16_t val;

	/* Lookup table */
	static const uint32_t
	ptype_lkup_tbl[QEDE_PKT_TYPE_MAX] __rte_cache_aligned = {
		[QEDE_PKT_TYPE_IPV4] = RTE_PTYPE_L3_IPV4,
		[QEDE_PKT_TYPE_IPV6] = RTE_PTYPE_L3_IPV6,
		[QEDE_PKT_TYPE_IPV4_TCP] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_TCP,
		[QEDE_PKT_TYPE_IPV6_TCP] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_TCP,
		[QEDE_PKT_TYPE_IPV4_UDP] = RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L4_UDP,
		[QEDE_PKT_TYPE_IPV6_UDP] = RTE_PTYPE_L3_IPV6 | RTE_PTYPE_L4_UDP,
	};

	/* Bits (0..3) provides L3/L4 protocol type */
	val = ((PARSING_AND_ERR_FLAGS_L3TYPE_MASK <<
	       PARSING_AND_ERR_FLAGS_L3TYPE_SHIFT) |
	       (PARSING_AND_ERR_FLAGS_L4PROTOCOL_MASK <<
		PARSING_AND_ERR_FLAGS_L4PROTOCOL_SHIFT)) & flags;

	if (val < QEDE_PKT_TYPE_MAX)
		return ptype_lkup_tbl[val] | RTE_PTYPE_L2_ETHER;
	else
		return RTE_PTYPE_UNKNOWN;
}

static inline uint8_t
qede_check_notunn_csum_l3(struct rte_mbuf *m, uint16_t flag)
{
	struct ipv4_hdr *ip;
	uint16_t pkt_csum;
	uint16_t calc_csum;
	uint16_t val;

	val = ((PARSING_AND_ERR_FLAGS_IPHDRERROR_MASK <<
		PARSING_AND_ERR_FLAGS_IPHDRERROR_SHIFT) & flag);

	if (unlikely(val)) {
		m->packet_type = qede_rx_cqe_to_pkt_type(flag);
		if (RTE_ETH_IS_IPV4_HDR(m->packet_type)) {
			ip = rte_pktmbuf_mtod_offset(m, struct ipv4_hdr *,
					   sizeof(struct ether_hdr));
			pkt_csum = ip->hdr_checksum;
			ip->hdr_checksum = 0;
			calc_csum = rte_ipv4_cksum(ip);
			ip->hdr_checksum = pkt_csum;
			return (calc_csum != pkt_csum);
		} else if (RTE_ETH_IS_IPV6_HDR(m->packet_type)) {
			return 1;
		}
	}
	return 0;
}

static inline void qede_rx_bd_ring_consume(struct qede_rx_queue *rxq)
{
	ecore_chain_consume(&rxq->rx_bd_ring);
	rxq->sw_rx_cons++;
}

static inline void
qede_reuse_page(__rte_unused struct qede_dev *qdev,
		struct qede_rx_queue *rxq, struct qede_rx_entry *curr_cons)
{
	struct eth_rx_bd *rx_bd_prod = ecore_chain_produce(&rxq->rx_bd_ring);
	uint16_t idx = rxq->sw_rx_cons & NUM_RX_BDS(rxq);
	struct qede_rx_entry *curr_prod;
	dma_addr_t new_mapping;

	curr_prod = &rxq->sw_rx_ring[idx];
	*curr_prod = *curr_cons;

	new_mapping = rte_mbuf_data_dma_addr_default(curr_prod->mbuf) +
		      curr_prod->page_offset;

	rx_bd_prod->addr.hi = rte_cpu_to_le_32(U64_HI(new_mapping));
	rx_bd_prod->addr.lo = rte_cpu_to_le_32(U64_LO(new_mapping));

	rxq->sw_rx_prod++;
}

static inline void
qede_recycle_rx_bd_ring(struct qede_rx_queue *rxq,
			struct qede_dev *qdev, uint8_t count)
{
	struct qede_rx_entry *curr_cons;

	for (; count > 0; count--) {
		curr_cons = &rxq->sw_rx_ring[rxq->sw_rx_cons & NUM_RX_BDS(rxq)];
		qede_reuse_page(qdev, rxq, curr_cons);
		qede_rx_bd_ring_consume(rxq);
	}
}

static inline void
qede_rx_process_tpa_cmn_cont_end_cqe(__rte_unused struct qede_dev *qdev,
				     struct qede_rx_queue *rxq,
				     uint8_t agg_index, uint16_t len)
{
	struct qede_agg_info *tpa_info;
	struct rte_mbuf *curr_frag; /* Pointer to currently filled TPA seg */
	uint16_t cons_idx;

	/* Under certain conditions it is possible that FW may not consume
	 * additional or new BD. So decision to consume the BD must be made
	 * based on len_list[0].
	 */
	if (rte_le_to_cpu_16(len)) {
		tpa_info = &rxq->tpa_info[agg_index];
		cons_idx = rxq->sw_rx_cons & NUM_RX_BDS(rxq);
		curr_frag = rxq->sw_rx_ring[cons_idx].mbuf;
		assert(curr_frag);
		curr_frag->nb_segs = 1;
		curr_frag->pkt_len = rte_le_to_cpu_16(len);
		curr_frag->data_len = curr_frag->pkt_len;
		tpa_info->tpa_tail->next = curr_frag;
		tpa_info->tpa_tail = curr_frag;
		qede_rx_bd_ring_consume(rxq);
		if (unlikely(qede_alloc_rx_buffer(rxq) != 0)) {
			PMD_RX_LOG(ERR, rxq, "mbuf allocation fails\n");
			rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
			rxq->rx_alloc_errors++;
		}
	}
}

static inline void
qede_rx_process_tpa_cont_cqe(struct qede_dev *qdev,
			     struct qede_rx_queue *rxq,
			     struct eth_fast_path_rx_tpa_cont_cqe *cqe)
{
	PMD_RX_LOG(INFO, rxq, "TPA cont[%d] - len [%d]\n",
		   cqe->tpa_agg_index, rte_le_to_cpu_16(cqe->len_list[0]));
	/* only len_list[0] will have value */
	qede_rx_process_tpa_cmn_cont_end_cqe(qdev, rxq, cqe->tpa_agg_index,
					     cqe->len_list[0]);
}

static inline void
qede_rx_process_tpa_end_cqe(struct qede_dev *qdev,
			    struct qede_rx_queue *rxq,
			    struct eth_fast_path_rx_tpa_end_cqe *cqe)
{
	struct rte_mbuf *rx_mb; /* Pointer to head of the chained agg */

	qede_rx_process_tpa_cmn_cont_end_cqe(qdev, rxq, cqe->tpa_agg_index,
					     cqe->len_list[0]);
	/* Update total length and frags based on end TPA */
	rx_mb = rxq->tpa_info[cqe->tpa_agg_index].tpa_head;
	/* TODO:  Add Sanity Checks */
	rx_mb->nb_segs = cqe->num_of_bds;
	rx_mb->pkt_len = cqe->total_packet_len;

	PMD_RX_LOG(INFO, rxq, "TPA End[%d] reason %d cqe_len %d nb_segs %d"
		   " pkt_len %d\n", cqe->tpa_agg_index, cqe->end_reason,
		   rte_le_to_cpu_16(cqe->len_list[0]), rx_mb->nb_segs,
		   rx_mb->pkt_len);
}

static inline uint32_t qede_rx_cqe_to_tunn_pkt_type(uint16_t flags)
{
	uint32_t val;

	/* Lookup table */
	static const uint32_t
	ptype_tunn_lkup_tbl[QEDE_PKT_TYPE_TUNN_MAX_TYPE] __rte_cache_aligned = {
		[QEDE_PKT_TYPE_UNKNOWN] = RTE_PTYPE_UNKNOWN,
		[QEDE_PKT_TYPE_TUNN_GENEVE] = RTE_PTYPE_TUNNEL_GENEVE,
		[QEDE_PKT_TYPE_TUNN_GRE] = RTE_PTYPE_TUNNEL_GRE,
		[QEDE_PKT_TYPE_TUNN_VXLAN] = RTE_PTYPE_TUNNEL_VXLAN,
		[QEDE_PKT_TYPE_TUNN_L2_TENID_NOEXIST_GENEVE] =
				RTE_PTYPE_TUNNEL_GENEVE | RTE_PTYPE_L2_ETHER,
		[QEDE_PKT_TYPE_TUNN_L2_TENID_NOEXIST_GRE] =
				RTE_PTYPE_TUNNEL_GRE | RTE_PTYPE_L2_ETHER,
		[QEDE_PKT_TYPE_TUNN_L2_TENID_NOEXIST_VXLAN] =
				RTE_PTYPE_TUNNEL_VXLAN | RTE_PTYPE_L2_ETHER,
		[QEDE_PKT_TYPE_TUNN_L2_TENID_EXIST_GENEVE] =
				RTE_PTYPE_TUNNEL_GENEVE | RTE_PTYPE_L2_ETHER,
		[QEDE_PKT_TYPE_TUNN_L2_TENID_EXIST_GRE] =
				RTE_PTYPE_TUNNEL_GRE | RTE_PTYPE_L2_ETHER,
		[QEDE_PKT_TYPE_TUNN_L2_TENID_EXIST_VXLAN] =
				RTE_PTYPE_TUNNEL_VXLAN | RTE_PTYPE_L2_ETHER,
		[QEDE_PKT_TYPE_TUNN_IPV4_TENID_NOEXIST_GENEVE] =
				RTE_PTYPE_TUNNEL_GENEVE | RTE_PTYPE_L3_IPV4,
		[QEDE_PKT_TYPE_TUNN_IPV4_TENID_NOEXIST_GRE] =
				RTE_PTYPE_TUNNEL_GRE | RTE_PTYPE_L3_IPV4,
		[QEDE_PKT_TYPE_TUNN_IPV4_TENID_NOEXIST_VXLAN] =
				RTE_PTYPE_TUNNEL_VXLAN | RTE_PTYPE_L3_IPV4,
		[QEDE_PKT_TYPE_TUNN_IPV4_TENID_EXIST_GENEVE] =
				RTE_PTYPE_TUNNEL_GENEVE | RTE_PTYPE_L3_IPV4,
		[QEDE_PKT_TYPE_TUNN_IPV4_TENID_EXIST_GRE] =
				RTE_PTYPE_TUNNEL_GRE | RTE_PTYPE_L3_IPV4,
		[QEDE_PKT_TYPE_TUNN_IPV4_TENID_EXIST_VXLAN] =
				RTE_PTYPE_TUNNEL_VXLAN | RTE_PTYPE_L3_IPV4,
		[QEDE_PKT_TYPE_TUNN_IPV6_TENID_NOEXIST_GENEVE] =
				RTE_PTYPE_TUNNEL_GENEVE | RTE_PTYPE_L3_IPV6,
		[QEDE_PKT_TYPE_TUNN_IPV6_TENID_NOEXIST_GRE] =
				RTE_PTYPE_TUNNEL_GRE | RTE_PTYPE_L3_IPV6,
		[QEDE_PKT_TYPE_TUNN_IPV6_TENID_NOEXIST_VXLAN] =
				RTE_PTYPE_TUNNEL_VXLAN | RTE_PTYPE_L3_IPV6,
		[QEDE_PKT_TYPE_TUNN_IPV6_TENID_EXIST_GENEVE] =
				RTE_PTYPE_TUNNEL_GENEVE | RTE_PTYPE_L3_IPV6,
		[QEDE_PKT_TYPE_TUNN_IPV6_TENID_EXIST_GRE] =
				RTE_PTYPE_TUNNEL_GRE | RTE_PTYPE_L3_IPV6,
		[QEDE_PKT_TYPE_TUNN_IPV6_TENID_EXIST_VXLAN] =
				RTE_PTYPE_TUNNEL_VXLAN | RTE_PTYPE_L3_IPV6,
	};

	/* Cover bits[4-0] to include tunn_type and next protocol */
	val = ((ETH_TUNNEL_PARSING_FLAGS_TYPE_MASK <<
		ETH_TUNNEL_PARSING_FLAGS_TYPE_SHIFT) |
		(ETH_TUNNEL_PARSING_FLAGS_NEXT_PROTOCOL_MASK <<
		ETH_TUNNEL_PARSING_FLAGS_NEXT_PROTOCOL_SHIFT)) & flags;

	if (val < QEDE_PKT_TYPE_TUNN_MAX_TYPE)
		return ptype_tunn_lkup_tbl[val];
	else
		return RTE_PTYPE_UNKNOWN;
}

static inline int
qede_process_sg_pkts(void *p_rxq,  struct rte_mbuf *rx_mb,
		     uint8_t num_segs, uint16_t pkt_len)
{
	struct qede_rx_queue *rxq = p_rxq;
	struct qede_dev *qdev = rxq->qdev;
	register struct rte_mbuf *seg1 = NULL;
	register struct rte_mbuf *seg2 = NULL;
	uint16_t sw_rx_index;
	uint16_t cur_size;

	seg1 = rx_mb;
	while (num_segs) {
		cur_size = pkt_len > rxq->rx_buf_size ? rxq->rx_buf_size :
							pkt_len;
		if (unlikely(!cur_size)) {
			PMD_RX_LOG(ERR, rxq, "Length is 0 while %u BDs"
				   " left for mapping jumbo", num_segs);
			qede_recycle_rx_bd_ring(rxq, qdev, num_segs);
			return -EINVAL;
		}
		sw_rx_index = rxq->sw_rx_cons & NUM_RX_BDS(rxq);
		seg2 = rxq->sw_rx_ring[sw_rx_index].mbuf;
		qede_rx_bd_ring_consume(rxq);
		pkt_len -= cur_size;
		seg2->data_len = cur_size;
		seg1->next = seg2;
		seg1 = seg1->next;
		num_segs--;
		rxq->rx_segs++;
	}

	return 0;
}

uint16_t
qede_recv_pkts(void *p_rxq, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct qede_rx_queue *rxq = p_rxq;
	struct qede_dev *qdev = rxq->qdev;
	struct ecore_dev *edev = &qdev->edev;
	uint16_t hw_comp_cons, sw_comp_cons, sw_rx_index;
	uint16_t rx_pkt = 0;
	union eth_rx_cqe *cqe;
	struct eth_fast_path_rx_reg_cqe *fp_cqe = NULL;
	register struct rte_mbuf *rx_mb = NULL;
	register struct rte_mbuf *seg1 = NULL;
	enum eth_rx_cqe_type cqe_type;
	uint16_t pkt_len = 0; /* Sum of all BD segments */
	uint16_t len; /* Length of first BD */
	uint8_t num_segs = 1;
	uint16_t preload_idx;
	uint16_t parse_flag;
#ifdef RTE_LIBRTE_QEDE_DEBUG_RX
	uint8_t bitfield_val;
	enum rss_hash_type htype;
#endif
	uint8_t tunn_parse_flag;
	uint8_t j;
	struct eth_fast_path_rx_tpa_start_cqe *cqe_start_tpa;
	uint64_t ol_flags;
	uint32_t packet_type;
	uint16_t vlan_tci;
	bool tpa_start_flg;
	uint8_t offset, tpa_agg_idx, flags;
	struct qede_agg_info *tpa_info = NULL;
	uint32_t rss_hash;

	hw_comp_cons = rte_le_to_cpu_16(*rxq->hw_cons_ptr);
	sw_comp_cons = ecore_chain_get_cons_idx(&rxq->rx_comp_ring);

	rte_rmb();

	if (hw_comp_cons == sw_comp_cons)
		return 0;

	while (sw_comp_cons != hw_comp_cons) {
		ol_flags = 0;
		packet_type = RTE_PTYPE_UNKNOWN;
		vlan_tci = 0;
		tpa_start_flg = false;
		rss_hash = 0;

		/* Get the CQE from the completion ring */
		cqe =
		    (union eth_rx_cqe *)ecore_chain_consume(&rxq->rx_comp_ring);
		cqe_type = cqe->fast_path_regular.type;
		PMD_RX_LOG(INFO, rxq, "Rx CQE type %d\n", cqe_type);

		switch (cqe_type) {
		case ETH_RX_CQE_TYPE_REGULAR:
			fp_cqe = &cqe->fast_path_regular;
		break;
		case ETH_RX_CQE_TYPE_TPA_START:
			cqe_start_tpa = &cqe->fast_path_tpa_start;
			tpa_info = &rxq->tpa_info[cqe_start_tpa->tpa_agg_index];
			tpa_start_flg = true;
			/* Mark it as LRO packet */
			ol_flags |= PKT_RX_LRO;
			/* In split mode,  seg_len is same as len_on_first_bd
			 * and ext_bd_len_list will be empty since there are
			 * no additional buffers
			 */
			PMD_RX_LOG(INFO, rxq,
			    "TPA start[%d] - len_on_first_bd %d header %d"
			    " [bd_list[0] %d], [seg_len %d]\n",
			    cqe_start_tpa->tpa_agg_index,
			    rte_le_to_cpu_16(cqe_start_tpa->len_on_first_bd),
			    cqe_start_tpa->header_len,
			    rte_le_to_cpu_16(cqe_start_tpa->ext_bd_len_list[0]),
			    rte_le_to_cpu_16(cqe_start_tpa->seg_len));

		break;
		case ETH_RX_CQE_TYPE_TPA_CONT:
			qede_rx_process_tpa_cont_cqe(qdev, rxq,
						     &cqe->fast_path_tpa_cont);
			goto next_cqe;
		case ETH_RX_CQE_TYPE_TPA_END:
			qede_rx_process_tpa_end_cqe(qdev, rxq,
						    &cqe->fast_path_tpa_end);
			tpa_agg_idx = cqe->fast_path_tpa_end.tpa_agg_index;
			tpa_info = &rxq->tpa_info[tpa_agg_idx];
			rx_mb = rxq->tpa_info[tpa_agg_idx].tpa_head;
			goto tpa_end;
		case ETH_RX_CQE_TYPE_SLOW_PATH:
			PMD_RX_LOG(INFO, rxq, "Got unexpected slowpath CQE\n");
			ecore_eth_cqe_completion(
				&edev->hwfns[rxq->queue_id % edev->num_hwfns],
				(struct eth_slow_path_rx_cqe *)cqe);
			/* fall-thru */
		default:
			goto next_cqe;
		}

		/* Get the data from the SW ring */
		sw_rx_index = rxq->sw_rx_cons & NUM_RX_BDS(rxq);
		rx_mb = rxq->sw_rx_ring[sw_rx_index].mbuf;
		assert(rx_mb != NULL);

		/* Handle regular CQE or TPA start CQE */
		if (!tpa_start_flg) {
			parse_flag = rte_le_to_cpu_16(fp_cqe->pars_flags.flags);
			offset = fp_cqe->placement_offset;
			len = rte_le_to_cpu_16(fp_cqe->len_on_first_bd);
			pkt_len = rte_le_to_cpu_16(fp_cqe->pkt_len);
			vlan_tci = rte_le_to_cpu_16(fp_cqe->vlan_tag);
			rss_hash = rte_le_to_cpu_32(fp_cqe->rss_hash);
#ifdef RTE_LIBRTE_QEDE_DEBUG_RX
			bitfield_val = fp_cqe->bitfields;
			htype = (uint8_t)GET_FIELD(bitfield_val,
					ETH_FAST_PATH_RX_REG_CQE_RSS_HASH_TYPE);
#endif
		} else {
			parse_flag =
			    rte_le_to_cpu_16(cqe_start_tpa->pars_flags.flags);
			offset = cqe_start_tpa->placement_offset;
			/* seg_len = len_on_first_bd */
			len = rte_le_to_cpu_16(cqe_start_tpa->len_on_first_bd);
			vlan_tci = rte_le_to_cpu_16(cqe_start_tpa->vlan_tag);
#ifdef RTE_LIBRTE_QEDE_DEBUG_RX
			bitfield_val = cqe_start_tpa->bitfields;
			htype = (uint8_t)GET_FIELD(bitfield_val,
				ETH_FAST_PATH_RX_TPA_START_CQE_RSS_HASH_TYPE);
#endif
			rss_hash = rte_le_to_cpu_32(cqe_start_tpa->rss_hash);
		}
		if (qede_tunn_exist(parse_flag)) {
			PMD_RX_LOG(INFO, rxq, "Rx tunneled packet\n");
			if (unlikely(qede_check_tunn_csum_l4(parse_flag))) {
				PMD_RX_LOG(ERR, rxq,
					    "L4 csum failed, flags = 0x%x\n",
					    parse_flag);
				rxq->rx_hw_errors++;
				ol_flags |= PKT_RX_L4_CKSUM_BAD;
			} else {
				ol_flags |= PKT_RX_L4_CKSUM_GOOD;
				if (tpa_start_flg)
					flags =
					 cqe_start_tpa->tunnel_pars_flags.flags;
				else
					flags = fp_cqe->tunnel_pars_flags.flags;
				tunn_parse_flag = flags;
				packet_type =
				qede_rx_cqe_to_tunn_pkt_type(tunn_parse_flag);
			}
		} else {
			PMD_RX_LOG(INFO, rxq, "Rx non-tunneled packet\n");
			if (unlikely(qede_check_notunn_csum_l4(parse_flag))) {
				PMD_RX_LOG(ERR, rxq,
					    "L4 csum failed, flags = 0x%x\n",
					    parse_flag);
				rxq->rx_hw_errors++;
				ol_flags |= PKT_RX_L4_CKSUM_BAD;
			} else {
				ol_flags |= PKT_RX_L4_CKSUM_GOOD;
			}
			if (unlikely(qede_check_notunn_csum_l3(rx_mb,
							parse_flag))) {
				PMD_RX_LOG(ERR, rxq,
					   "IP csum failed, flags = 0x%x\n",
					   parse_flag);
				rxq->rx_hw_errors++;
				ol_flags |= PKT_RX_IP_CKSUM_BAD;
			} else {
				ol_flags |= PKT_RX_IP_CKSUM_GOOD;
				packet_type =
					qede_rx_cqe_to_pkt_type(parse_flag);
			}
		}

		if (CQE_HAS_VLAN(parse_flag)) {
			ol_flags |= PKT_RX_VLAN_PKT;
			if (qdev->vlan_strip_flg) {
				ol_flags |= PKT_RX_VLAN_STRIPPED;
				rx_mb->vlan_tci = vlan_tci;
			}
		}
		if (CQE_HAS_OUTER_VLAN(parse_flag)) {
			ol_flags |= PKT_RX_QINQ_PKT;
			if (qdev->vlan_strip_flg) {
				rx_mb->vlan_tci = vlan_tci;
				ol_flags |= PKT_RX_QINQ_STRIPPED;
			}
			rx_mb->vlan_tci_outer = 0;
		}
		/* RSS Hash */
		if (qdev->rss_enable) {
			ol_flags |= PKT_RX_RSS_HASH;
			rx_mb->hash.rss = rss_hash;
		}

		if (unlikely(qede_alloc_rx_buffer(rxq) != 0)) {
			PMD_RX_LOG(ERR, rxq,
				   "New buffer allocation failed,"
				   "dropping incoming packet\n");
			qede_recycle_rx_bd_ring(rxq, qdev, fp_cqe->bd_num);
			rte_eth_devices[rxq->port_id].
			    data->rx_mbuf_alloc_failed++;
			rxq->rx_alloc_errors++;
			break;
		}
		qede_rx_bd_ring_consume(rxq);

		if (!tpa_start_flg && fp_cqe->bd_num > 1) {
			PMD_RX_LOG(DEBUG, rxq, "Jumbo-over-BD packet: %02x BDs"
				   " len on first: %04x Total Len: %04x",
				   fp_cqe->bd_num, len, pkt_len);
			num_segs = fp_cqe->bd_num - 1;
			seg1 = rx_mb;
			if (qede_process_sg_pkts(p_rxq, seg1, num_segs,
						 pkt_len - len))
				goto next_cqe;
			for (j = 0; j < num_segs; j++) {
				if (qede_alloc_rx_buffer(rxq)) {
					PMD_RX_LOG(ERR, rxq,
						"Buffer allocation failed");
					rte_eth_devices[rxq->port_id].
						data->rx_mbuf_alloc_failed++;
					rxq->rx_alloc_errors++;
					break;
				}
				rxq->rx_segs++;
			}
		}
		rxq->rx_segs++; /* for the first segment */

		/* Prefetch next mbuf while processing current one. */
		preload_idx = rxq->sw_rx_cons & NUM_RX_BDS(rxq);
		rte_prefetch0(rxq->sw_rx_ring[preload_idx].mbuf);

		/* Update rest of the MBUF fields */
		rx_mb->data_off = offset + RTE_PKTMBUF_HEADROOM;
		rx_mb->port = rxq->port_id;
		rx_mb->ol_flags = ol_flags;
		rx_mb->data_len = len;
		rx_mb->packet_type = packet_type;
		PMD_RX_LOG(INFO, rxq,
			   "pkt_type 0x%04x len %u hash_type %d hash_val 0x%x"
			   " ol_flags 0x%04lx\n",
			   packet_type, len, htype, rx_mb->hash.rss,
			   (unsigned long)ol_flags);
		if (!tpa_start_flg) {
			rx_mb->nb_segs = fp_cqe->bd_num;
			rx_mb->pkt_len = pkt_len;
		} else {
			/* store ref to the updated mbuf */
			tpa_info->tpa_head = rx_mb;
			tpa_info->tpa_tail = tpa_info->tpa_head;
		}
		rte_prefetch1(rte_pktmbuf_mtod(rx_mb, void *));
tpa_end:
		if (!tpa_start_flg) {
			rx_pkts[rx_pkt] = rx_mb;
			rx_pkt++;
		}
next_cqe:
		ecore_chain_recycle_consumed(&rxq->rx_comp_ring);
		sw_comp_cons = ecore_chain_get_cons_idx(&rxq->rx_comp_ring);
		if (rx_pkt == nb_pkts) {
			PMD_RX_LOG(DEBUG, rxq,
				   "Budget reached nb_pkts=%u received=%u",
				   rx_pkt, nb_pkts);
			break;
		}
	}

	qede_update_rx_prod(qdev, rxq);

	rxq->rcv_pkts += rx_pkt;

	PMD_RX_LOG(DEBUG, rxq, "rx_pkts=%u core=%d", rx_pkt, rte_lcore_id());

	return rx_pkt;
}


/* Populate scatter gather buffer descriptor fields */
static inline uint8_t
qede_encode_sg_bd(struct qede_tx_queue *p_txq, struct rte_mbuf *m_seg,
		  struct eth_tx_2nd_bd **bd2, struct eth_tx_3rd_bd **bd3)
{
	struct qede_tx_queue *txq = p_txq;
	struct eth_tx_bd *tx_bd = NULL;
	dma_addr_t mapping;
	uint8_t nb_segs = 0;

	/* Check for scattered buffers */
	while (m_seg) {
		if (nb_segs == 0) {
			if (!*bd2) {
				*bd2 = (struct eth_tx_2nd_bd *)
					ecore_chain_produce(&txq->tx_pbl);
				memset(*bd2, 0, sizeof(struct eth_tx_2nd_bd));
				nb_segs++;
			}
			mapping = rte_mbuf_data_dma_addr(m_seg);
			QEDE_BD_SET_ADDR_LEN(*bd2, mapping, m_seg->data_len);
			PMD_TX_LOG(DEBUG, txq, "BD2 len %04x", m_seg->data_len);
		} else if (nb_segs == 1) {
			if (!*bd3) {
				*bd3 = (struct eth_tx_3rd_bd *)
					ecore_chain_produce(&txq->tx_pbl);
				memset(*bd3, 0, sizeof(struct eth_tx_3rd_bd));
				nb_segs++;
			}
			mapping = rte_mbuf_data_dma_addr(m_seg);
			QEDE_BD_SET_ADDR_LEN(*bd3, mapping, m_seg->data_len);
			PMD_TX_LOG(DEBUG, txq, "BD3 len %04x", m_seg->data_len);
		} else {
			tx_bd = (struct eth_tx_bd *)
				ecore_chain_produce(&txq->tx_pbl);
			memset(tx_bd, 0, sizeof(*tx_bd));
			nb_segs++;
			mapping = rte_mbuf_data_dma_addr(m_seg);
			QEDE_BD_SET_ADDR_LEN(tx_bd, mapping, m_seg->data_len);
			PMD_TX_LOG(DEBUG, txq, "BD len %04x", m_seg->data_len);
		}
		m_seg = m_seg->next;
	}

	/* Return total scattered buffers */
	return nb_segs;
}

#ifdef RTE_LIBRTE_QEDE_DEBUG_TX
static inline void
print_tx_bd_info(struct qede_tx_queue *txq,
		 struct eth_tx_1st_bd *bd1,
		 struct eth_tx_2nd_bd *bd2,
		 struct eth_tx_3rd_bd *bd3,
		 uint64_t tx_ol_flags)
{
	char ol_buf[256] = { 0 }; /* for verbose prints */

	if (bd1)
		PMD_TX_LOG(INFO, txq,
			   "BD1: nbytes=%u nbds=%u bd_flags=%04x bf=%04x",
			   rte_cpu_to_le_16(bd1->nbytes), bd1->data.nbds,
			   bd1->data.bd_flags.bitfields,
			   rte_cpu_to_le_16(bd1->data.bitfields));
	if (bd2)
		PMD_TX_LOG(INFO, txq,
			   "BD2: nbytes=%u bf=%04x\n",
			   rte_cpu_to_le_16(bd2->nbytes), bd2->data.bitfields1);
	if (bd3)
		PMD_TX_LOG(INFO, txq,
			   "BD3: nbytes=%u bf=%04x mss=%u\n",
			   rte_cpu_to_le_16(bd3->nbytes),
			   rte_cpu_to_le_16(bd3->data.bitfields),
			   rte_cpu_to_le_16(bd3->data.lso_mss));

	rte_get_tx_ol_flag_list(tx_ol_flags, ol_buf, sizeof(ol_buf));
	PMD_TX_LOG(INFO, txq, "TX offloads = %s\n", ol_buf);
}
#endif

/* TX prepare to check packets meets TX conditions */
uint16_t
#ifdef RTE_LIBRTE_QEDE_DEBUG_TX
qede_xmit_prep_pkts(void *p_txq, struct rte_mbuf **tx_pkts,
		    uint16_t nb_pkts)
{
	struct qede_tx_queue *txq = p_txq;
#else
qede_xmit_prep_pkts(__rte_unused void *p_txq, struct rte_mbuf **tx_pkts,
		    uint16_t nb_pkts)
{
#endif
	uint64_t ol_flags;
	struct rte_mbuf *m;
	uint16_t i;
	int ret;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;
		if (ol_flags & PKT_TX_TCP_SEG) {
			if (m->nb_segs >= ETH_TX_MAX_BDS_PER_LSO_PACKET) {
				rte_errno = -EINVAL;
				break;
			}
			/* TBD: confirm its ~9700B for both ? */
			if (m->tso_segsz > ETH_TX_MAX_NON_LSO_PKT_LEN) {
				rte_errno = -EINVAL;
				break;
			}
		} else {
			if (m->nb_segs >= ETH_TX_MAX_BDS_PER_NON_LSO_PACKET) {
				rte_errno = -EINVAL;
				break;
			}
		}
		if (ol_flags & QEDE_TX_OFFLOAD_NOTSUP_MASK) {
			rte_errno = -ENOTSUP;
			break;
		}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = ret;
			break;
		}
#endif
		/* TBD: pseudo csum calcuation required if
		 * ETH_TX_DATA_2ND_BD_L4_PSEUDO_CSUM_MODE not set?
		 */
		ret = rte_net_intel_cksum_prepare(m);
		if (ret != 0) {
			rte_errno = ret;
			break;
		}
	}

#ifdef RTE_LIBRTE_QEDE_DEBUG_TX
	if (unlikely(i != nb_pkts))
		PMD_TX_LOG(ERR, txq, "TX prepare failed for %u\n",
			   nb_pkts - i);
#endif
	return i;
}

uint16_t
qede_xmit_pkts(void *p_txq, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct qede_tx_queue *txq = p_txq;
	struct qede_dev *qdev = txq->qdev;
	struct ecore_dev *edev = &qdev->edev;
	struct rte_mbuf *mbuf;
	struct rte_mbuf *m_seg = NULL;
	uint16_t nb_tx_pkts;
	uint16_t bd_prod;
	uint16_t idx;
	uint16_t nb_frags;
	uint16_t nb_pkt_sent = 0;
	uint8_t nbds;
	bool ipv6_ext_flg;
	bool lso_flg;
	__rte_unused bool tunn_flg;
	struct eth_tx_1st_bd *bd1;
	struct eth_tx_2nd_bd *bd2;
	struct eth_tx_3rd_bd *bd3;
	uint64_t tx_ol_flags;
	uint16_t hdr_size;
	/* BD1 */
	uint16_t bd1_bf;
	uint8_t bd1_bd_flags_bf;
	uint16_t vlan;
	/* BD2 */
	uint16_t bd2_bf1;
	uint16_t bd2_bf2;
	/* BD3 */
	uint16_t mss;
	uint16_t bd3_bf;


	if (unlikely(txq->nb_tx_avail < txq->tx_free_thresh)) {
		PMD_TX_LOG(DEBUG, txq, "send=%u avail=%u free_thresh=%u",
			   nb_pkts, txq->nb_tx_avail, txq->tx_free_thresh);
		qede_process_tx_compl(edev, txq);
	}

	nb_tx_pkts  = nb_pkts;
	bd_prod = rte_cpu_to_le_16(ecore_chain_get_prod_idx(&txq->tx_pbl));
	while (nb_tx_pkts--) {
		/* Init flags/values */
		ipv6_ext_flg = false;
		tunn_flg = false;
		lso_flg = false;
		nbds = 0;
		vlan = 0;
		bd1 = NULL;
		bd2 = NULL;
		bd3 = NULL;
		hdr_size = 0;
		bd1_bf = 0;
		bd1_bd_flags_bf = 0;
		bd2_bf1 = 0;
		bd2_bf2 = 0;
		mss = 0;
		bd3_bf = 0;

		mbuf = *tx_pkts++;
		assert(mbuf);

		/* Check minimum TX BDS availability against available BDs */
		if (unlikely(txq->nb_tx_avail < mbuf->nb_segs))
			break;

		tx_ol_flags = mbuf->ol_flags;
		bd1_bd_flags_bf |= 1 << ETH_TX_1ST_BD_FLAGS_START_BD_SHIFT;

#define RTE_ETH_IS_IPV6_HDR_EXT(ptype) ((ptype) & RTE_PTYPE_L3_IPV6_EXT)
		if (RTE_ETH_IS_IPV6_HDR_EXT(mbuf->packet_type)) {
			ipv6_ext_flg = true;
			if (unlikely(txq->nb_tx_avail <
					ETH_TX_MIN_BDS_PER_IPV6_WITH_EXT_PKT))
				break;
		}

		if (RTE_ETH_IS_TUNNEL_PKT(mbuf->packet_type)) {
			if (ipv6_ext_flg) {
				if (unlikely(txq->nb_tx_avail <
				    ETH_TX_MIN_BDS_PER_TUNN_IPV6_WITH_EXT_PKT))
					break;
			}
			tunn_flg = true;
			/* First indicate its a tunnel pkt */
			bd1_bf |= ETH_TX_DATA_1ST_BD_TUNN_FLAG_MASK <<
				  ETH_TX_DATA_1ST_BD_TUNN_FLAG_SHIFT;
			/* Legacy FW had flipped behavior in regard to this bit
			 * i.e. it needed to set to prevent FW from touching
			 * encapsulated packets when it didn't need to.
			 */
			if (unlikely(txq->is_legacy)) {
				bd1_bf ^= 1 <<
					ETH_TX_DATA_1ST_BD_TUNN_FLAG_SHIFT;
			}
			/* Outer IP checksum offload */
			if (tx_ol_flags & PKT_TX_OUTER_IP_CKSUM) {
				bd1_bd_flags_bf |=
					ETH_TX_1ST_BD_FLAGS_TUNN_IP_CSUM_MASK <<
					ETH_TX_1ST_BD_FLAGS_TUNN_IP_CSUM_SHIFT;
			}
			/* Outer UDP checksum offload */
			bd1_bd_flags_bf |=
				ETH_TX_1ST_BD_FLAGS_TUNN_L4_CSUM_MASK <<
				ETH_TX_1ST_BD_FLAGS_TUNN_L4_CSUM_SHIFT;
		}

		if (tx_ol_flags & PKT_TX_TCP_SEG) {
			lso_flg = true;
			if (unlikely(txq->nb_tx_avail <
						ETH_TX_MIN_BDS_PER_LSO_PKT))
				break;
			/* For LSO, packet header and payload must reside on
			 * buffers pointed by different BDs. Using BD1 for HDR
			 * and BD2 onwards for data.
			 */
			hdr_size = mbuf->l2_len + mbuf->l3_len + mbuf->l4_len;
			bd1_bd_flags_bf |= 1 << ETH_TX_1ST_BD_FLAGS_LSO_SHIFT;
			bd1_bd_flags_bf |=
					1 << ETH_TX_1ST_BD_FLAGS_IP_CSUM_SHIFT;
			/* PKT_TX_TCP_SEG implies PKT_TX_TCP_CKSUM */
			bd1_bd_flags_bf |=
					1 << ETH_TX_1ST_BD_FLAGS_L4_CSUM_SHIFT;
			mss = rte_cpu_to_le_16(mbuf->tso_segsz);
			/* Using one header BD */
			bd3_bf |= rte_cpu_to_le_16(1 <<
					ETH_TX_DATA_3RD_BD_HDR_NBD_SHIFT);
		} else {
			if (unlikely(txq->nb_tx_avail <
					ETH_TX_MIN_BDS_PER_NON_LSO_PKT))
				break;
			bd1_bf |=
			       (mbuf->pkt_len & ETH_TX_DATA_1ST_BD_PKT_LEN_MASK)
				<< ETH_TX_DATA_1ST_BD_PKT_LEN_SHIFT;
		}

		/* Descriptor based VLAN insertion */
		if (tx_ol_flags & (PKT_TX_VLAN_PKT | PKT_TX_QINQ_PKT)) {
			vlan = rte_cpu_to_le_16(mbuf->vlan_tci);
			bd1_bd_flags_bf |=
			    1 << ETH_TX_1ST_BD_FLAGS_VLAN_INSERTION_SHIFT;
		}

		/* Offload the IP checksum in the hardware */
		if (tx_ol_flags & PKT_TX_IP_CKSUM)
			bd1_bd_flags_bf |=
				1 << ETH_TX_1ST_BD_FLAGS_IP_CSUM_SHIFT;

		/* L4 checksum offload (tcp or udp) */
		if (tx_ol_flags & (PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM))
			bd1_bd_flags_bf |=
				1 << ETH_TX_1ST_BD_FLAGS_L4_CSUM_SHIFT;

		if (ipv6_ext_flg) {
			/* TBD: check pseudo csum if tx_prepare not called? */
			bd2_bf1 |= ETH_L4_PSEUDO_CSUM_ZERO_LENGTH <<
				ETH_TX_DATA_2ND_BD_L4_PSEUDO_CSUM_MODE_SHIFT;
		}

		/* Fill the entry in the SW ring and the BDs in the FW ring */
		idx = TX_PROD(txq);
		txq->sw_tx_ring[idx].mbuf = mbuf;

		/* BD1 */
		bd1 = (struct eth_tx_1st_bd *)ecore_chain_produce(&txq->tx_pbl);
		memset(bd1, 0, sizeof(struct eth_tx_1st_bd));
		nbds++;

		/* Map MBUF linear data for DMA and set in the BD1 */
		QEDE_BD_SET_ADDR_LEN(bd1, rte_mbuf_data_dma_addr(mbuf),
					mbuf->data_len);
		bd1->data.bitfields = bd1_bf;
		bd1->data.bd_flags.bitfields = bd1_bd_flags_bf;
		bd1->data.vlan = vlan;

		if (lso_flg || ipv6_ext_flg) {
			bd2 = (struct eth_tx_2nd_bd *)ecore_chain_produce
							(&txq->tx_pbl);
			memset(bd2, 0, sizeof(struct eth_tx_2nd_bd));
			nbds++;

			/* BD1 */
			QEDE_BD_SET_ADDR_LEN(bd1, rte_mbuf_data_dma_addr(mbuf),
					     hdr_size);
			/* BD2 */
			QEDE_BD_SET_ADDR_LEN(bd2, (hdr_size +
					     rte_mbuf_data_dma_addr(mbuf)),
					     mbuf->data_len - hdr_size);
			bd2->data.bitfields1 = bd2_bf1;
			bd2->data.bitfields2 = bd2_bf2;

			/* BD3 */
			bd3 = (struct eth_tx_3rd_bd *)ecore_chain_produce
							(&txq->tx_pbl);
			memset(bd3, 0, sizeof(struct eth_tx_3rd_bd));
			nbds++;
			bd3->data.bitfields = bd3_bf;
			bd3->data.lso_mss = mss;
		}

		/* Handle fragmented MBUF */
		m_seg = mbuf->next;
		/* Encode scatter gather buffer descriptors if required */
		nb_frags = qede_encode_sg_bd(txq, m_seg, &bd2, &bd3);
		bd1->data.nbds = nbds + nb_frags;
		txq->nb_tx_avail -= bd1->data.nbds;
		txq->sw_tx_prod++;
		rte_prefetch0(txq->sw_tx_ring[TX_PROD(txq)].mbuf);
		bd_prod =
		    rte_cpu_to_le_16(ecore_chain_get_prod_idx(&txq->tx_pbl));
#ifdef RTE_LIBRTE_QEDE_DEBUG_TX
		print_tx_bd_info(txq, bd1, bd2, bd3, tx_ol_flags);
		PMD_TX_LOG(INFO, txq, "lso=%d tunn=%d ipv6_ext=%d\n",
			   lso_flg, tunn_flg, ipv6_ext_flg);
#endif
		nb_pkt_sent++;
		txq->xmit_pkts++;
	}

	/* Write value of prod idx into bd_prod */
	txq->tx_db.data.bd_prod = bd_prod;
	rte_wmb();
	rte_compiler_barrier();
	DIRECT_REG_WR_RELAXED(edev, txq->doorbell_addr, txq->tx_db.raw);
	rte_wmb();

	/* Check again for Tx completions */
	qede_process_tx_compl(edev, txq);

	PMD_TX_LOG(DEBUG, txq, "to_send=%u sent=%u bd_prod=%u core=%d",
		   nb_pkts, nb_pkt_sent, TX_PROD(txq), rte_lcore_id());

	return nb_pkt_sent;
}

uint16_t
qede_rxtx_pkts_dummy(__rte_unused void *p_rxq,
		     __rte_unused struct rte_mbuf **pkts,
		     __rte_unused uint16_t nb_pkts)
{
	return 0;
}
