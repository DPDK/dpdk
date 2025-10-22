/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_TXRX_OPS_H_
#define _NBL_TXRX_OPS_H_

#define NBL_TX_MAX_FREE_BUF_SZ		64
#define NBL_RXQ_REARM_THRESH		32

static __rte_always_inline struct rte_mbuf *nbl_pktmbuf_prefree_seg(struct rte_mbuf *m)
{
	if (likely(m))
		return rte_pktmbuf_prefree_seg(m);

	return NULL;
}

static __rte_always_inline int
desc_is_used(volatile struct nbl_packed_desc *desc, bool wrap_counter)
{
	uint16_t used, avail, flags;

	flags = desc->flags;
	used = !!(flags & NBL_PACKED_DESC_F_USED_BIT);
	avail = !!(flags & NBL_PACKED_DESC_F_AVAIL_BIT);

	return avail == used && used == wrap_counter;
}

static __rte_always_inline int
nbl_tx_free_bufs(struct nbl_res_tx_ring *txq)
{
	struct rte_mbuf *m, *free[NBL_TX_MAX_FREE_BUF_SZ];
	struct nbl_tx_entry *txep;
	uint32_t n;
	uint32_t i;
	uint32_t next_to_clean;
	int nb_free = 0;

	next_to_clean = txq->tx_entry[txq->next_to_clean].first_id;
	/* check DD bits on threshold descriptor */
	if (!desc_is_used(&txq->desc[next_to_clean], txq->used_wrap_counter))
		return 0;

	n = 32;

	 /* first buffer to free from S/W ring is at index
	  * tx_next_dd - (tx_rs_thresh-1)
	  */
	/* consider headroom */
	txep = &txq->tx_entry[txq->next_to_clean - (n - 1)];
	m = nbl_pktmbuf_prefree_seg(txep[0].mbuf);
	if (likely(m)) {
		free[0] = m;
		nb_free = 1;
		for (i = 1; i < n; i++) {
			m = nbl_pktmbuf_prefree_seg(txep[i].mbuf);
			if (likely(m)) {
				if (likely(m->pool == free[0]->pool)) {
					free[nb_free++] = m;
				} else {
					rte_mempool_put_bulk(free[0]->pool,
							     (void *)free,
							     nb_free);
					free[0] = m;
					nb_free = 1;
				}
			}
		}
		rte_mempool_put_bulk(free[0]->pool, (void **)free, nb_free);
	} else {
		for (i = 1; i < n; i++) {
			m = nbl_pktmbuf_prefree_seg(txep[i].mbuf);
			if (m)
				rte_mempool_put(m->pool, m);
		}
	}

	/* buffers were freed, update counters */
	txq->vq_free_cnt = (uint16_t)(txq->vq_free_cnt + NBL_TX_RS_THRESH);
	txq->next_to_clean = (uint16_t)(txq->next_to_clean + NBL_TX_RS_THRESH);
	if (txq->next_to_clean >= txq->nb_desc) {
		txq->next_to_clean = NBL_TX_RS_THRESH - 1;
		txq->used_wrap_counter ^= 1;
	}

	return 32;
}

#endif
