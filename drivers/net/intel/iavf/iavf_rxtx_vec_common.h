/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _IAVF_RXTX_VEC_COMMON_H_
#define _IAVF_RXTX_VEC_COMMON_H_
#include <stdint.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "iavf.h"
#include "iavf_rxtx.h"

#ifdef RTE_ARCH_X86
#include "../common/tx_vec_x86.h"
#endif

static inline int
iavf_tx_desc_done(struct ci_tx_queue *txq, uint16_t idx)
{
	return (txq->ci_tx_ring[idx].cmd_type_offset_bsz &
			rte_cpu_to_le_64(CI_TXD_QW1_DTYPE_M)) ==
				rte_cpu_to_le_64(CI_TX_DESC_DTYPE_DESC_DONE);
}

static inline void
_iavf_rx_queue_release_mbufs_vec(struct ci_rx_queue *rxq)
{
	const unsigned int mask = rxq->nb_rx_desc - 1;
	unsigned int i;

	if (!rxq->sw_ring || rxq->rxrearm_nb >= rxq->nb_rx_desc)
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
iavf_rx_vec_queue_default(struct ci_rx_queue *rxq)
{
	if (!rxq)
		return -1;

	if (!rte_is_power_of_2(rxq->nb_rx_desc))
		return -1;

	if (rxq->rx_free_thresh < IAVF_VPMD_RX_BURST)
		return -1;

	if (rxq->nb_rx_desc % rxq->rx_free_thresh)
		return -1;

	if (rxq->proto_xtr != IAVF_PROTO_XTR_NONE)
		return -1;

	return 0;
}

static inline int
iavf_tx_vec_queue_default(struct ci_tx_queue *txq)
{
	if (!txq)
		return -1;

	if (txq->tx_rs_thresh < IAVF_VPMD_TX_BURST ||
	    txq->tx_rs_thresh > IAVF_VPMD_TX_MAX_FREE_BUF)
		return -1;

	return 0;
}

static inline int
iavf_rx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct ci_rx_queue *rxq;
	int ret = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		ret = iavf_rx_vec_queue_default(rxq);

		if (ret < 0)
			break;
	}

	return ret;
}

static inline int
iavf_tx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct ci_tx_queue *txq;
	int ret;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		ret = iavf_tx_vec_queue_default(txq);

		if (ret < 0)
			break;
	}

	return ret;
}

#endif
