/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 HiSilicon Limited.
 */

#ifndef _HNS3_RXTX_VEC_H_
#define _HNS3_RXTX_VEC_H_

#include "hns3_rxtx.h"
#include "hns3_ethdev.h"

static inline void
hns3_tx_bulk_free_buffers(struct hns3_tx_queue *txq)
{
	struct rte_mbuf **free = txq->free;
	struct hns3_entry *tx_entry;
	struct rte_mbuf *m;
	int nb_free = 0;
	int i;

	tx_entry = &txq->sw_ring[txq->next_to_clean];
	for (i = 0; i < txq->tx_rs_thresh; i++, tx_entry++) {
		m = rte_pktmbuf_prefree_seg(tx_entry->mbuf);
		tx_entry->mbuf = NULL;

		if (m == NULL)
			continue;

		if (nb_free && m->pool != free[0]->pool) {
			rte_mempool_put_bulk(free[0]->pool, (void **)free,
					     nb_free);
			nb_free = 0;
		}
		free[nb_free++] = m;
	}

	if (nb_free)
		rte_mempool_put_bulk(free[0]->pool, (void **)free, nb_free);

	/* Update numbers of available descriptor due to buffer freed */
	txq->tx_bd_ready += txq->tx_rs_thresh;
	txq->next_to_clean += txq->tx_rs_thresh;
	if (txq->next_to_clean >= txq->nb_tx_desc)
		txq->next_to_clean = 0;
}

static inline void
hns3_tx_free_buffers(struct hns3_tx_queue *txq)
{
	struct hns3_desc *tx_desc;
	int i;

	/*
	 * All mbufs can be released only when the VLD bits of all
	 * descriptors in a batch are cleared.
	 */
	tx_desc = &txq->tx_ring[txq->next_to_clean];
	for (i = 0; i < txq->tx_rs_thresh; i++, tx_desc++) {
		if (tx_desc->tx.tp_fe_sc_vld_ra_ri &
				rte_le_to_cpu_16(BIT(HNS3_TXD_VLD_B)))
			return;
	}

	hns3_tx_bulk_free_buffers(txq);
}

static inline uint16_t
hns3_rx_reassemble_pkts(struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts,
			uint64_t pkt_err_mask)
{
	uint16_t count, i;
	uint64_t mask;

	if (likely(pkt_err_mask == 0))
		return nb_pkts;

	count = 0;
	for (i = 0; i < nb_pkts; i++) {
		mask = ((uint64_t)1u) << i;
		if (pkt_err_mask & mask)
			rte_pktmbuf_free_seg(rx_pkts[i]);
		else
			rx_pkts[count++] = rx_pkts[i];
	}

	return count;
}
#endif /* _HNS3_RXTX_VEC_H_ */
