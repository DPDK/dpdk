/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _ICE_RXTX_VEC_COMMON_H_
#define _ICE_RXTX_VEC_COMMON_H_

#include "../common/rx.h"
#include "ice_rxtx.h"

static inline int
ice_tx_desc_done(struct ci_tx_queue *txq, uint16_t idx)
{
	return (txq->ci_tx_ring[idx].cmd_type_offset_bsz &
			rte_cpu_to_le_64(CI_TXD_QW1_DTYPE_M)) ==
				rte_cpu_to_le_64(CI_TX_DESC_DTYPE_DESC_DONE);
}

static inline void
_ice_rx_queue_release_mbufs_vec(struct ci_rx_queue *rxq)
{
	const unsigned int mask = rxq->nb_rx_desc - 1;
	unsigned int i;

	if (unlikely(!rxq->sw_ring)) {
		PMD_DRV_LOG(DEBUG, "sw_ring is NULL");
		return;
	}

	if (rxq->rxrearm_nb >= rxq->nb_rx_desc)
		return;

	/* free all mbufs that are valid in the ring */
	if (rxq->rxrearm_nb == 0) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf)
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
		}
	} else {
		for (i = rxq->rx_tail;
		     i != rxq->rxrearm_start;
		     i = (i + 1) & mask) {
			if (rxq->sw_ring[i].mbuf)
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
		}
	}

	rxq->rxrearm_nb = rxq->nb_rx_desc;

	/* set all entries to NULL */
	memset(rxq->sw_ring, 0, sizeof(rxq->sw_ring[0]) * rxq->nb_rx_desc);
}

static inline int
ice_rx_vec_queue_default(struct ci_rx_queue *rxq)
{
	if (!rxq)
		return -1;

	if (!ci_rxq_vec_capable(rxq->nb_rx_desc, rxq->rx_free_thresh))
		return -1;

	if (rxq->proto_xtr != PROTO_XTR_NONE)
		return -1;

	return 0;
}

static inline int
ice_tx_vec_queue_default(struct ci_tx_queue *txq)
{
	if (!txq)
		return -1;

	if (!ci_txq_vec_capable(txq->tx_rs_thresh))
		return -1;

	return 0;
}

static inline int
ice_rx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct ci_rx_queue *rxq;
	int ret = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		ret = (ice_rx_vec_queue_default(rxq));
		if (ret < 0)
			break;
	}

	return ret;
}

static inline int
ice_tx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct ci_tx_queue *txq;
	int ret = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		ret = ice_tx_vec_queue_default(txq);
		if (ret < 0)
			return -1;
	}

	return ret;
}

#endif
