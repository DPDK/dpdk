/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Hisilicon Limited.
 */

#ifndef _HNS3_RXTX_VEC_H_
#define _HNS3_RXTX_VEC_H_

#include "hns3_rxtx.h"
#include "hns3_ethdev.h"

static inline void
hns3_tx_free_buffers(struct hns3_tx_queue *txq)
{
	struct rte_mbuf **free = txq->free;
	struct hns3_entry *tx_entry;
	struct hns3_desc *tx_desc;
	struct rte_mbuf *m;
	int nb_free = 0;
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
#endif /* _HNS3_RXTX_VEC_H_ */
