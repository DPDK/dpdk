/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <rte_mbuf_dyn.h>
#include "idpf_common_rxtx.h"

int
idpf_check_rx_thresh(uint16_t nb_desc, uint16_t thresh)
{
	/* The following constraints must be satisfied:
	 * thresh < rxq->nb_rx_desc
	 */
	if (thresh >= nb_desc) {
		DRV_LOG(ERR, "rx_free_thresh (%u) must be less than %u",
			thresh, nb_desc);
		return -EINVAL;
	}

	return 0;
}

int
idpf_check_tx_thresh(uint16_t nb_desc, uint16_t tx_rs_thresh,
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
		DRV_LOG(ERR, "tx_rs_thresh (%u) must be less than the "
			"number of TX descriptors (%u) minus 2",
			tx_rs_thresh, nb_desc);
		return -EINVAL;
	}
	if (tx_free_thresh >= (nb_desc - 3)) {
		DRV_LOG(ERR, "tx_free_thresh (%u) must be less than the "
			"number of TX descriptors (%u) minus 3.",
			tx_free_thresh, nb_desc);
		return -EINVAL;
	}
	if (tx_rs_thresh > tx_free_thresh) {
		DRV_LOG(ERR, "tx_rs_thresh (%u) must be less than or "
			"equal to tx_free_thresh (%u).",
			tx_rs_thresh, tx_free_thresh);
		return -EINVAL;
	}
	if ((nb_desc % tx_rs_thresh) != 0) {
		DRV_LOG(ERR, "tx_rs_thresh (%u) must be a divisor of the "
			"number of TX descriptors (%u).",
			tx_rs_thresh, nb_desc);
		return -EINVAL;
	}

	return 0;
}

void
idpf_release_rxq_mbufs(struct idpf_rx_queue *rxq)
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

void
idpf_release_txq_mbufs(struct idpf_tx_queue *txq)
{
	uint16_t nb_desc, i;

	if (txq == NULL || txq->sw_ring == NULL) {
		DRV_LOG(DEBUG, "Pointer to rxq or sw_ring is NULL");
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

void
idpf_reset_split_rx_descq(struct idpf_rx_queue *rxq)
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

void
idpf_reset_split_rx_bufq(struct idpf_rx_queue *rxq)
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

void
idpf_reset_split_rx_queue(struct idpf_rx_queue *rxq)
{
	idpf_reset_split_rx_descq(rxq);
	idpf_reset_split_rx_bufq(rxq->bufq1);
	idpf_reset_split_rx_bufq(rxq->bufq2);
}

void
idpf_reset_single_rx_queue(struct idpf_rx_queue *rxq)
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

void
idpf_reset_split_tx_descq(struct idpf_tx_queue *txq)
{
	struct idpf_tx_entry *txe;
	uint32_t i, size;
	uint16_t prev;

	if (txq == NULL) {
		DRV_LOG(DEBUG, "Pointer to txq is NULL");
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

void
idpf_reset_split_tx_complq(struct idpf_tx_queue *cq)
{
	uint32_t i, size;

	if (cq == NULL) {
		DRV_LOG(DEBUG, "Pointer to complq is NULL");
		return;
	}

	size = sizeof(struct idpf_splitq_tx_compl_desc) * cq->nb_tx_desc;
	for (i = 0; i < size; i++)
		((volatile char *)cq->compl_ring)[i] = 0;

	cq->tx_tail = 0;
	cq->expected_gen_id = 1;
}

void
idpf_reset_single_tx_queue(struct idpf_tx_queue *txq)
{
	struct idpf_tx_entry *txe;
	uint32_t i, size;
	uint16_t prev;

	if (txq == NULL) {
		DRV_LOG(DEBUG, "Pointer to txq is NULL");
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

void
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

void
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

int
idpf_alloc_single_rxq_mbufs(struct idpf_rx_queue *rxq)
{
	volatile struct virtchnl2_singleq_rx_buf_desc *rxd;
	struct rte_mbuf *mbuf = NULL;
	uint64_t dma_addr;
	uint16_t i;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		mbuf = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(mbuf == NULL)) {
			DRV_LOG(ERR, "Failed to allocate mbuf for RX");
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

int
idpf_alloc_split_rxq_mbufs(struct idpf_rx_queue *rxq)
{
	volatile struct virtchnl2_splitq_rx_buf_desc *rxd;
	struct rte_mbuf *mbuf = NULL;
	uint64_t dma_addr;
	uint16_t i;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		mbuf = rte_mbuf_raw_alloc(rxq->mp);
		if (unlikely(mbuf == NULL)) {
			DRV_LOG(ERR, "Failed to allocate mbuf for RX");
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
