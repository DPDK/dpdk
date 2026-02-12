/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _IXGBE_RXTX_VEC_COMMON_H_
#define _IXGBE_RXTX_VEC_COMMON_H_
#include <stdint.h>
#include <ethdev_driver.h>

#include "../common/rx.h"
#include "ixgbe_ethdev.h"
#include "ixgbe_rxtx.h"

int ixgbe_rx_vec_dev_conf_condition_check(struct rte_eth_dev *dev);
int ixgbe_rxq_vec_setup(struct ci_rx_queue *rxq);
int ixgbe_txq_vec_setup(struct ci_tx_queue *txq);
void ixgbe_rx_queue_release_mbufs_vec(struct ci_rx_queue *rxq);
void ixgbe_reset_tx_queue_vec(struct ci_tx_queue *txq);
void ixgbe_tx_free_swring_vec(struct ci_tx_queue *txq);
uint16_t ixgbe_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t ixgbe_recv_scattered_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t ixgbe_xmit_fixed_burst_vec(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
uint16_t ixgbe_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
void ixgbe_recycle_rx_descriptors_refill_vec(void *rx_queue, uint16_t nb_mbufs);
uint16_t ixgbe_recycle_tx_mbufs_reuse_vec(void *tx_queue,
		struct rte_eth_recycle_rxq_info *recycle_rxq_info);

static __rte_always_inline int
ixgbe_tx_free_bufs_vec(struct ci_tx_queue *txq)
{
	struct ci_tx_entry_vec *txep;
	uint32_t n;
	uint32_t i;
	int nb_free = 0;
	struct rte_mbuf *m, *free[IXGBE_TX_MAX_FREE_BUF_SZ];

	/* check DD bit on threshold descriptor */
	if (!ixgbe_tx_desc_done(txq, txq->tx_next_dd))
		return 0;

	n = txq->tx_rs_thresh;

	/*
	 * first buffer to free from S/W ring is at index
	 * tx_next_dd - (tx_rs_thresh-1)
	 */
	txep = &txq->sw_ring_vec[txq->tx_next_dd - (n - 1)];
	m = rte_pktmbuf_prefree_seg(txep[0].mbuf);
	if (likely(m != NULL)) {
		free[0] = m;
		nb_free = 1;
		for (i = 1; i < n; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (likely(m != NULL)) {
				if (likely(m->pool == free[0]->pool))
					free[nb_free++] = m;
				else {
					rte_mbuf_raw_free_bulk(free[0]->pool, free, nb_free);
					free[0] = m;
					nb_free = 1;
				}
			}
		}
		rte_mbuf_raw_free_bulk(free[0]->pool, free, nb_free);
	} else {
		for (i = 1; i < n; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (m != NULL)
				rte_mempool_put(m->pool, m);
		}
	}

	/* buffers were freed, update counters */
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + txq->tx_rs_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_rs_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);

	return txq->tx_rs_thresh;
}
#endif
