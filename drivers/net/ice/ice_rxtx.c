/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_ethdev_driver.h>
#include <rte_net.h>

#include "ice_rxtx.h"

#define ICE_TD_CMD ICE_TX_DESC_CMD_EOP

#define ICE_TX_CKSUM_OFFLOAD_MASK (		 \
		PKT_TX_IP_CKSUM |		 \
		PKT_TX_L4_MASK |		 \
		PKT_TX_TCP_SEG |		 \
		PKT_TX_OUTER_IP_CKSUM)

#define ICE_RX_ERR_BITS 0x3f

static enum ice_status
ice_program_hw_rx_queue(struct ice_rx_queue *rxq)
{
	struct ice_vsi *vsi = rxq->vsi;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct rte_eth_dev *dev = ICE_VSI_TO_ETH_DEV(rxq->vsi);
	struct ice_rlan_ctx rx_ctx;
	enum ice_status err;
	uint16_t buf_size, len;
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	uint32_t regval;

	/**
	 * The kernel driver uses flex descriptor. It sets the register
	 * to flex descriptor mode.
	 * DPDK uses legacy descriptor. It should set the register back
	 * to the default value, then uses legacy descriptor mode.
	 */
	regval = (0x01 << QRXFLXP_CNTXT_RXDID_PRIO_S) &
		 QRXFLXP_CNTXT_RXDID_PRIO_M;
	ICE_WRITE_REG(hw, QRXFLXP_CNTXT(rxq->reg_idx), regval);

	/* Set buffer size as the head split is disabled. */
	buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mp) -
			      RTE_PKTMBUF_HEADROOM);
	rxq->rx_hdr_len = 0;
	rxq->rx_buf_len = RTE_ALIGN(buf_size, (1 << ICE_RLAN_CTX_DBUF_S));
	len = ICE_SUPPORT_CHAIN_NUM * rxq->rx_buf_len;
	rxq->max_pkt_len = RTE_MIN(len,
				   dev->data->dev_conf.rxmode.max_rx_pkt_len);

	if (rxmode->offloads & DEV_RX_OFFLOAD_JUMBO_FRAME) {
		if (rxq->max_pkt_len <= ETHER_MAX_LEN ||
		    rxq->max_pkt_len > ICE_FRAME_SIZE_MAX) {
			PMD_DRV_LOG(ERR, "maximum packet length must "
				    "be larger than %u and smaller than %u,"
				    "as jumbo frame is enabled",
				    (uint32_t)ETHER_MAX_LEN,
				    (uint32_t)ICE_FRAME_SIZE_MAX);
			return -EINVAL;
		}
	} else {
		if (rxq->max_pkt_len < ETHER_MIN_LEN ||
		    rxq->max_pkt_len > ETHER_MAX_LEN) {
			PMD_DRV_LOG(ERR, "maximum packet length must be "
				    "larger than %u and smaller than %u, "
				    "as jumbo frame is disabled",
				    (uint32_t)ETHER_MIN_LEN,
				    (uint32_t)ETHER_MAX_LEN);
			return -EINVAL;
		}
	}

	memset(&rx_ctx, 0, sizeof(rx_ctx));

	rx_ctx.base = rxq->rx_ring_phys_addr / ICE_QUEUE_BASE_ADDR_UNIT;
	rx_ctx.qlen = rxq->nb_rx_desc;
	rx_ctx.dbuf = rxq->rx_buf_len >> ICE_RLAN_CTX_DBUF_S;
	rx_ctx.hbuf = rxq->rx_hdr_len >> ICE_RLAN_CTX_HBUF_S;
	rx_ctx.dtype = 0; /* No Header Split mode */
#ifndef RTE_LIBRTE_ICE_16BYTE_RX_DESC
	rx_ctx.dsize = 1; /* 32B descriptors */
#endif
	rx_ctx.rxmax = rxq->max_pkt_len;
	/* TPH: Transaction Layer Packet (TLP) processing hints */
	rx_ctx.tphrdesc_ena = 1;
	rx_ctx.tphwdesc_ena = 1;
	rx_ctx.tphdata_ena = 1;
	rx_ctx.tphhead_ena = 1;
	/* Low Receive Queue Threshold defined in 64 descriptors units.
	 * When the number of free descriptors goes below the lrxqthresh,
	 * an immediate interrupt is triggered.
	 */
	rx_ctx.lrxqthresh = 2;
	/*default use 32 byte descriptor, vlan tag extract to L2TAG2(1st)*/
	rx_ctx.l2tsel = 1;
	rx_ctx.showiv = 0;

	err = ice_clear_rxq_ctx(hw, rxq->reg_idx);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to clear Lan Rx queue (%u) context",
			    rxq->queue_id);
		return -EINVAL;
	}
	err = ice_write_rxq_ctx(hw, &rx_ctx, rxq->reg_idx);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to write Lan Rx queue (%u) context",
			    rxq->queue_id);
		return -EINVAL;
	}

	buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mp) -
			      RTE_PKTMBUF_HEADROOM);

	rxq->qrx_tail = hw->hw_addr + QRX_TAIL(rxq->reg_idx);

	/* Init the Rx tail register*/
	ICE_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);

	return 0;
}

/* Allocate mbufs for all descriptors in rx queue */
static int
ice_alloc_rx_queue_mbufs(struct ice_rx_queue *rxq)
{
	struct ice_rx_entry *rxe = rxq->sw_ring;
	uint64_t dma_addr;
	uint16_t i;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		volatile union ice_rx_desc *rxd;
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mp);

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
#ifndef RTE_LIBRTE_ICE_16BYTE_RX_DESC
		rxd->read.rsvd1 = 0;
		rxd->read.rsvd2 = 0;
#endif
		rxe[i].mbuf = mbuf;
	}

	return 0;
}

/* Free all mbufs for descriptors in rx queue */
static void
ice_rx_queue_release_mbufs(struct ice_rx_queue *rxq)
{
	uint16_t i;

	if (!rxq || !rxq->sw_ring) {
		PMD_DRV_LOG(DEBUG, "Pointer to sw_ring is NULL");
		return;
	}

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		if (rxq->sw_ring[i].mbuf) {
			rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
			rxq->sw_ring[i].mbuf = NULL;
		}
	}
#ifdef RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC
		if (rxq->rx_nb_avail == 0)
			return;
		for (i = 0; i < rxq->rx_nb_avail; i++) {
			struct rte_mbuf *mbuf;

			mbuf = rxq->rx_stage[rxq->rx_next_avail + i];
			rte_pktmbuf_free_seg(mbuf);
		}
		rxq->rx_nb_avail = 0;
#endif /* RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC */
}

/* turn on or off rx queue
 * @q_idx: queue index in pf scope
 * @on: turn on or off the queue
 */
static int
ice_switch_rx_queue(struct ice_hw *hw, uint16_t q_idx, bool on)
{
	uint32_t reg;
	uint16_t j;

	/* QRX_CTRL = QRX_ENA */
	reg = ICE_READ_REG(hw, QRX_CTRL(q_idx));

	if (on) {
		if (reg & QRX_CTRL_QENA_STAT_M)
			return 0; /* Already on, skip */
		reg |= QRX_CTRL_QENA_REQ_M;
	} else {
		if (!(reg & QRX_CTRL_QENA_STAT_M))
			return 0; /* Already off, skip */
		reg &= ~QRX_CTRL_QENA_REQ_M;
	}

	/* Write the register */
	ICE_WRITE_REG(hw, QRX_CTRL(q_idx), reg);
	/* Check the result. It is said that QENA_STAT
	 * follows the QENA_REQ not more than 10 use.
	 * TODO: need to change the wait counter later
	 */
	for (j = 0; j < ICE_CHK_Q_ENA_COUNT; j++) {
		rte_delay_us(ICE_CHK_Q_ENA_INTERVAL_US);
		reg = ICE_READ_REG(hw, QRX_CTRL(q_idx));
		if (on) {
			if ((reg & QRX_CTRL_QENA_REQ_M) &&
			    (reg & QRX_CTRL_QENA_STAT_M))
				break;
		} else {
			if (!(reg & QRX_CTRL_QENA_REQ_M) &&
			    !(reg & QRX_CTRL_QENA_STAT_M))
				break;
		}
	}

	/* Check if it is timeout */
	if (j >= ICE_CHK_Q_ENA_COUNT) {
		PMD_DRV_LOG(ERR, "Failed to %s rx queue[%u]",
			    (on ? "enable" : "disable"), q_idx);
		return -ETIMEDOUT;
	}

	return 0;
}

static inline int
#ifdef RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC
ice_check_rx_burst_bulk_alloc_preconditions(struct ice_rx_queue *rxq)
#else
ice_check_rx_burst_bulk_alloc_preconditions
	(__rte_unused struct ice_rx_queue *rxq)
#endif
{
	int ret = 0;

#ifdef RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC
	if (!(rxq->rx_free_thresh >= ICE_RX_MAX_BURST)) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->rx_free_thresh=%d, "
			     "ICE_RX_MAX_BURST=%d",
			     rxq->rx_free_thresh, ICE_RX_MAX_BURST);
		ret = -EINVAL;
	} else if (!(rxq->rx_free_thresh < rxq->nb_rx_desc)) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->rx_free_thresh=%d, "
			     "rxq->nb_rx_desc=%d",
			     rxq->rx_free_thresh, rxq->nb_rx_desc);
		ret = -EINVAL;
	} else if (rxq->nb_rx_desc % rxq->rx_free_thresh != 0) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->nb_rx_desc=%d, "
			     "rxq->rx_free_thresh=%d",
			     rxq->nb_rx_desc, rxq->rx_free_thresh);
		ret = -EINVAL;
	}
#else
	ret = -EINVAL;
#endif

	return ret;
}

/* reset fields in ice_rx_queue back to default */
static void
ice_reset_rx_queue(struct ice_rx_queue *rxq)
{
	unsigned int i;
	uint16_t len;

	if (!rxq) {
		PMD_DRV_LOG(DEBUG, "Pointer to rxq is NULL");
		return;
	}

#ifdef RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC
	if (ice_check_rx_burst_bulk_alloc_preconditions(rxq) == 0)
		len = (uint16_t)(rxq->nb_rx_desc + ICE_RX_MAX_BURST);
	else
#endif /* RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC */
		len = rxq->nb_rx_desc;

	for (i = 0; i < len * sizeof(union ice_rx_desc); i++)
		((volatile char *)rxq->rx_ring)[i] = 0;

#ifdef RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC
	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));
	for (i = 0; i < ICE_RX_MAX_BURST; ++i)
		rxq->sw_ring[rxq->nb_rx_desc + i].mbuf = &rxq->fake_mbuf;

	rxq->rx_nb_avail = 0;
	rxq->rx_next_avail = 0;
	rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);
#endif /* RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC */

	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

int
ice_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ice_rx_queue *rxq;
	int err;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	if (rx_queue_id >= dev->data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, "RX queue %u is out of range %u",
			    rx_queue_id, dev->data->nb_rx_queues);
		return -EINVAL;
	}

	rxq = dev->data->rx_queues[rx_queue_id];
	if (!rxq || !rxq->q_set) {
		PMD_DRV_LOG(ERR, "RX queue %u not available or setup",
			    rx_queue_id);
		return -EINVAL;
	}

	err = ice_program_hw_rx_queue(rxq);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to program RX queue %u",
			    rx_queue_id);
		return -EIO;
	}

	err = ice_alloc_rx_queue_mbufs(rxq);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to allocate RX queue mbuf");
		return -ENOMEM;
	}

	rte_wmb();

	/* Init the RX tail register. */
	ICE_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);

	err = ice_switch_rx_queue(hw, rxq->reg_idx, TRUE);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to switch RX queue %u on",
			    rx_queue_id);

		ice_rx_queue_release_mbufs(rxq);
		ice_reset_rx_queue(rxq);
		return -EINVAL;
	}

	dev->data->rx_queue_state[rx_queue_id] =
		RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

int
ice_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ice_rx_queue *rxq;
	int err;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (rx_queue_id < dev->data->nb_rx_queues) {
		rxq = dev->data->rx_queues[rx_queue_id];

		err = ice_switch_rx_queue(hw, rxq->reg_idx, FALSE);
		if (err) {
			PMD_DRV_LOG(ERR, "Failed to switch RX queue %u off",
				    rx_queue_id);
			return -EINVAL;
		}
		ice_rx_queue_release_mbufs(rxq);
		ice_reset_rx_queue(rxq);
		dev->data->rx_queue_state[rx_queue_id] =
			RTE_ETH_QUEUE_STATE_STOPPED;
	}

	return 0;
}

int
ice_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ice_tx_queue *txq;
	int err;
	struct ice_vsi *vsi;
	struct ice_hw *hw;
	struct ice_aqc_add_tx_qgrp txq_elem;
	struct ice_tlan_ctx tx_ctx;

	PMD_INIT_FUNC_TRACE();

	if (tx_queue_id >= dev->data->nb_tx_queues) {
		PMD_DRV_LOG(ERR, "TX queue %u is out of range %u",
			    tx_queue_id, dev->data->nb_tx_queues);
		return -EINVAL;
	}

	txq = dev->data->tx_queues[tx_queue_id];
	if (!txq || !txq->q_set) {
		PMD_DRV_LOG(ERR, "TX queue %u is not available or setup",
			    tx_queue_id);
		return -EINVAL;
	}

	vsi = txq->vsi;
	hw = ICE_VSI_TO_HW(vsi);

	memset(&txq_elem, 0, sizeof(txq_elem));
	memset(&tx_ctx, 0, sizeof(tx_ctx));
	txq_elem.num_txqs = 1;
	txq_elem.txqs[0].txq_id = rte_cpu_to_le_16(txq->reg_idx);

	tx_ctx.base = txq->tx_ring_phys_addr / ICE_QUEUE_BASE_ADDR_UNIT;
	tx_ctx.qlen = txq->nb_tx_desc;
	tx_ctx.pf_num = hw->pf_id;
	tx_ctx.vmvf_type = ICE_TLAN_CTX_VMVF_TYPE_PF;
	tx_ctx.src_vsi = vsi->vsi_id;
	tx_ctx.port_num = hw->port_info->lport;
	tx_ctx.tso_ena = 1; /* tso enable */
	tx_ctx.tso_qnum = txq->reg_idx; /* index for tso state structure */
	tx_ctx.legacy_int = 1; /* Legacy or Advanced Host Interface */

	ice_set_ctx((uint8_t *)&tx_ctx, txq_elem.txqs[0].txq_ctx,
		    ice_tlan_ctx_info);

	txq->qtx_tail = hw->hw_addr + QTX_COMM_DBELL(txq->reg_idx);

	/* Init the Tx tail register*/
	ICE_PCI_REG_WRITE(txq->qtx_tail, 0);

	err = ice_ena_vsi_txq(hw->port_info, vsi->idx, 0, 1, &txq_elem,
			      sizeof(txq_elem), NULL);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to add lan txq");
		return -EIO;
	}
	/* store the schedule node id */
	txq->q_teid = txq_elem.txqs[0].q_teid;

	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;
}

/* Free all mbufs for descriptors in tx queue */
static void
ice_tx_queue_release_mbufs(struct ice_tx_queue *txq)
{
	uint16_t i;

	if (!txq || !txq->sw_ring) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq or sw_ring is NULL");
		return;
	}

	for (i = 0; i < txq->nb_tx_desc; i++) {
		if (txq->sw_ring[i].mbuf) {
			rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
			txq->sw_ring[i].mbuf = NULL;
		}
	}
}

static void
ice_reset_tx_queue(struct ice_tx_queue *txq)
{
	struct ice_tx_entry *txe;
	uint16_t i, prev, size;

	if (!txq) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq is NULL");
		return;
	}

	txe = txq->sw_ring;
	size = sizeof(struct ice_tx_desc) * txq->nb_tx_desc;
	for (i = 0; i < size; i++)
		((volatile char *)txq->tx_ring)[i] = 0;

	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		volatile struct ice_tx_desc *txd = &txq->tx_ring[i];

		txd->cmd_type_offset_bsz =
			rte_cpu_to_le_64(ICE_TX_DESC_DTYPE_DESC_DONE);
		txe[i].mbuf =  NULL;
		txe[i].last_id = i;
		txe[prev].next_id = i;
		prev = i;
	}

	txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);
	txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

	txq->tx_tail = 0;
	txq->nb_tx_used = 0;

	txq->last_desc_cleaned = (uint16_t)(txq->nb_tx_desc - 1);
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_desc - 1);
}

int
ice_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ice_tx_queue *txq;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	enum ice_status status;
	uint16_t q_ids[1];
	uint32_t q_teids[1];

	if (tx_queue_id >= dev->data->nb_tx_queues) {
		PMD_DRV_LOG(ERR, "TX queue %u is out of range %u",
			    tx_queue_id, dev->data->nb_tx_queues);
		return -EINVAL;
	}

	txq = dev->data->tx_queues[tx_queue_id];
	if (!txq) {
		PMD_DRV_LOG(ERR, "TX queue %u is not available",
			    tx_queue_id);
		return -EINVAL;
	}

	q_ids[0] = txq->reg_idx;
	q_teids[0] = txq->q_teid;

	status = ice_dis_vsi_txq(hw->port_info, 1, q_ids, q_teids,
				 ICE_NO_RESET, 0, NULL);
	if (status != ICE_SUCCESS) {
		PMD_DRV_LOG(DEBUG, "Failed to disable Lan Tx queue");
		return -EINVAL;
	}

	ice_tx_queue_release_mbufs(txq);
	ice_reset_tx_queue(txq);
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

int
ice_rx_queue_setup(struct rte_eth_dev *dev,
		   uint16_t queue_idx,
		   uint16_t nb_desc,
		   unsigned int socket_id,
		   const struct rte_eth_rxconf *rx_conf,
		   struct rte_mempool *mp)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct ice_rx_queue *rxq;
	const struct rte_memzone *rz;
	uint32_t ring_size;
	uint16_t len;
	int use_def_burst_func = 1;

	if (nb_desc % ICE_ALIGN_RING_DESC != 0 ||
	    nb_desc > ICE_MAX_RING_DESC ||
	    nb_desc < ICE_MIN_RING_DESC) {
		PMD_INIT_LOG(ERR, "Number (%u) of receive descriptors is "
			     "invalid", nb_desc);
		return -EINVAL;
	}

	/* Free memory if needed */
	if (dev->data->rx_queues[queue_idx]) {
		ice_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* Allocate the rx queue data structure */
	rxq = rte_zmalloc_socket(NULL,
				 sizeof(struct ice_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (!rxq) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for "
			     "rx queue data structure");
		return -ENOMEM;
	}
	rxq->mp = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->queue_id = queue_idx;

	rxq->reg_idx = vsi->base_queue + queue_idx;
	rxq->port_id = dev->data->port_id;
	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_KEEP_CRC)
		rxq->crc_len = ETHER_CRC_LEN;
	else
		rxq->crc_len = 0;

	rxq->drop_en = rx_conf->rx_drop_en;
	rxq->vsi = vsi;
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;

	/* Allocate the maximun number of RX ring hardware descriptor. */
	len = ICE_MAX_RING_DESC;

#ifdef RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC
	/**
	 * Allocating a little more memory because vectorized/bulk_alloc Rx
	 * functions doesn't check boundaries each time.
	 */
	len += ICE_RX_MAX_BURST;
#endif

	/* Allocate the maximum number of RX ring hardware descriptor. */
	ring_size = sizeof(union ice_rx_desc) * len;
	ring_size = RTE_ALIGN(ring_size, ICE_DMA_MEM_ALIGN);
	rz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx,
				      ring_size, ICE_RING_BASE_ALIGN,
				      socket_id);
	if (!rz) {
		ice_rx_queue_release(rxq);
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for RX");
		return -ENOMEM;
	}

	/* Zero all the descriptors in the ring. */
	memset(rz->addr, 0, ring_size);

	rxq->rx_ring_phys_addr = rz->phys_addr;
	rxq->rx_ring = (union ice_rx_desc *)rz->addr;

#ifdef RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC
	len = (uint16_t)(nb_desc + ICE_RX_MAX_BURST);
#else
	len = nb_desc;
#endif

	/* Allocate the software ring. */
	rxq->sw_ring = rte_zmalloc_socket(NULL,
					  sizeof(struct ice_rx_entry) * len,
					  RTE_CACHE_LINE_SIZE,
					  socket_id);
	if (!rxq->sw_ring) {
		ice_rx_queue_release(rxq);
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW ring");
		return -ENOMEM;
	}

	ice_reset_rx_queue(rxq);
	rxq->q_set = TRUE;
	dev->data->rx_queues[queue_idx] = rxq;

	use_def_burst_func = ice_check_rx_burst_bulk_alloc_preconditions(rxq);

	if (!use_def_burst_func) {
#ifdef RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
			     "satisfied. Rx Burst Bulk Alloc function will be "
			     "used on port=%d, queue=%d.",
			     rxq->port_id, rxq->queue_id);
#endif /* RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC */
	} else {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
			     "not satisfied, Scattered Rx is requested, "
			     "or RTE_LIBRTE_ICE_RX_ALLOW_BULK_ALLOC is "
			     "not enabled on port=%d, queue=%d.",
			     rxq->port_id, rxq->queue_id);
		ad->rx_bulk_alloc_allowed = false;
	}

	return 0;
}

void
ice_rx_queue_release(void *rxq)
{
	struct ice_rx_queue *q = (struct ice_rx_queue *)rxq;

	if (!q) {
		PMD_DRV_LOG(DEBUG, "Pointer to rxq is NULL");
		return;
	}

	ice_rx_queue_release_mbufs(q);
	rte_free(q->sw_ring);
	rte_free(q);
}

int
ice_tx_queue_setup(struct rte_eth_dev *dev,
		   uint16_t queue_idx,
		   uint16_t nb_desc,
		   unsigned int socket_id,
		   const struct rte_eth_txconf *tx_conf)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct ice_tx_queue *txq;
	const struct rte_memzone *tz;
	uint32_t ring_size;
	uint16_t tx_rs_thresh, tx_free_thresh;
	uint64_t offloads;

	offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;

	if (nb_desc % ICE_ALIGN_RING_DESC != 0 ||
	    nb_desc > ICE_MAX_RING_DESC ||
	    nb_desc < ICE_MIN_RING_DESC) {
		PMD_INIT_LOG(ERR, "Number (%u) of transmit descriptors is "
			     "invalid", nb_desc);
		return -EINVAL;
	}

	/**
	 * The following two parameters control the setting of the RS bit on
	 * transmit descriptors. TX descriptors will have their RS bit set
	 * after txq->tx_rs_thresh descriptors have been used. The TX
	 * descriptor ring will be cleaned after txq->tx_free_thresh
	 * descriptors are used or if the number of descriptors required to
	 * transmit a packet is greater than the number of free TX descriptors.
	 *
	 * The following constraints must be satisfied:
	 *  - tx_rs_thresh must be greater than 0.
	 *  - tx_rs_thresh must be less than the size of the ring minus 2.
	 *  - tx_rs_thresh must be less than or equal to tx_free_thresh.
	 *  - tx_rs_thresh must be a divisor of the ring size.
	 *  - tx_free_thresh must be greater than 0.
	 *  - tx_free_thresh must be less than the size of the ring minus 3.
	 *
	 * One descriptor in the TX ring is used as a sentinel to avoid a H/W
	 * race condition, hence the maximum threshold constraints. When set
	 * to zero use default values.
	 */
	tx_rs_thresh = (uint16_t)(tx_conf->tx_rs_thresh ?
				  tx_conf->tx_rs_thresh :
				  ICE_DEFAULT_TX_RSBIT_THRESH);
	tx_free_thresh = (uint16_t)(tx_conf->tx_free_thresh ?
				    tx_conf->tx_free_thresh :
				    ICE_DEFAULT_TX_FREE_THRESH);
	if (tx_rs_thresh >= (nb_desc - 2)) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh must be less than the "
			     "number of TX descriptors minus 2. "
			     "(tx_rs_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -EINVAL;
	}
	if (tx_free_thresh >= (nb_desc - 3)) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh must be less than the "
			     "tx_free_thresh must be less than the "
			     "number of TX descriptors minus 3. "
			     "(tx_free_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -EINVAL;
	}
	if (tx_rs_thresh > tx_free_thresh) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh must be less than or "
			     "equal to tx_free_thresh. (tx_free_thresh=%u"
			     " tx_rs_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -EINVAL;
	}
	if ((nb_desc % tx_rs_thresh) != 0) {
		PMD_INIT_LOG(ERR, "tx_rs_thresh must be a divisor of the "
			     "number of TX descriptors. (tx_rs_thresh=%u"
			     " port=%d queue=%d)",
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -EINVAL;
	}
	if (tx_rs_thresh > 1 && tx_conf->tx_thresh.wthresh != 0) {
		PMD_INIT_LOG(ERR, "TX WTHRESH must be set to 0 if "
			     "tx_rs_thresh is greater than 1. "
			     "(tx_rs_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_rs_thresh,
			     (int)dev->data->port_id,
			     (int)queue_idx);
		return -EINVAL;
	}

	/* Free memory if needed. */
	if (dev->data->tx_queues[queue_idx]) {
		ice_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket(NULL,
				 sizeof(struct ice_tx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (!txq) {
		PMD_INIT_LOG(ERR, "Failed to allocate memory for "
			     "tx queue structure");
		return -ENOMEM;
	}

	/* Allocate TX hardware ring descriptors. */
	ring_size = sizeof(struct ice_tx_desc) * ICE_MAX_RING_DESC;
	ring_size = RTE_ALIGN(ring_size, ICE_DMA_MEM_ALIGN);
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx,
				      ring_size, ICE_RING_BASE_ALIGN,
				      socket_id);
	if (!tz) {
		ice_tx_queue_release(txq);
		PMD_INIT_LOG(ERR, "Failed to reserve DMA memory for TX");
		return -ENOMEM;
	}

	txq->nb_tx_desc = nb_desc;
	txq->tx_rs_thresh = tx_rs_thresh;
	txq->tx_free_thresh = tx_free_thresh;
	txq->pthresh = tx_conf->tx_thresh.pthresh;
	txq->hthresh = tx_conf->tx_thresh.hthresh;
	txq->wthresh = tx_conf->tx_thresh.wthresh;
	txq->queue_id = queue_idx;

	txq->reg_idx = vsi->base_queue + queue_idx;
	txq->port_id = dev->data->port_id;
	txq->offloads = offloads;
	txq->vsi = vsi;
	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	txq->tx_ring_phys_addr = tz->phys_addr;
	txq->tx_ring = (struct ice_tx_desc *)tz->addr;

	/* Allocate software ring */
	txq->sw_ring =
		rte_zmalloc_socket(NULL,
				   sizeof(struct ice_tx_entry) * nb_desc,
				   RTE_CACHE_LINE_SIZE,
				   socket_id);
	if (!txq->sw_ring) {
		ice_tx_queue_release(txq);
		PMD_INIT_LOG(ERR, "Failed to allocate memory for SW TX ring");
		return -ENOMEM;
	}

	ice_reset_tx_queue(txq);
	txq->q_set = TRUE;
	dev->data->tx_queues[queue_idx] = txq;

	return 0;
}

void
ice_tx_queue_release(void *txq)
{
	struct ice_tx_queue *q = (struct ice_tx_queue *)txq;

	if (!q) {
		PMD_DRV_LOG(DEBUG, "Pointer to TX queue is NULL");
		return;
	}

	ice_tx_queue_release_mbufs(q);
	rte_free(q->sw_ring);
	rte_free(q);
}

void
ice_clear_queues(struct rte_eth_dev *dev)
{
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		ice_tx_queue_release_mbufs(dev->data->tx_queues[i]);
		ice_reset_tx_queue(dev->data->tx_queues[i]);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		ice_rx_queue_release_mbufs(dev->data->rx_queues[i]);
		ice_reset_rx_queue(dev->data->rx_queues[i]);
	}
}

void
ice_free_queues(struct rte_eth_dev *dev)
{
	uint16_t i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (!dev->data->rx_queues[i])
			continue;
		ice_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		if (!dev->data->tx_queues[i])
			continue;
		ice_tx_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}
