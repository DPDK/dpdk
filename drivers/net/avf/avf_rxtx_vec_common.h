/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _AVF_RXTX_VEC_COMMON_H_
#define _AVF_RXTX_VEC_COMMON_H_
#include <stdint.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>

#include "avf.h"
#include "avf_rxtx.h"

static inline uint16_t
reassemble_packets(struct avf_rx_queue *rxq, struct rte_mbuf **rx_bufs,
		   uint16_t nb_bufs, uint8_t *split_flags)
{
	struct rte_mbuf *pkts[AVF_VPMD_RX_MAX_BURST];
	struct rte_mbuf *start = rxq->pkt_first_seg;
	struct rte_mbuf *end =  rxq->pkt_last_seg;
	unsigned int pkt_idx, buf_idx;

	for (buf_idx = 0, pkt_idx = 0; buf_idx < nb_bufs; buf_idx++) {
		if (end) {
			/* processing a split packet */
			end->next = rx_bufs[buf_idx];
			rx_bufs[buf_idx]->data_len += rxq->crc_len;

			start->nb_segs++;
			start->pkt_len += rx_bufs[buf_idx]->data_len;
			end = end->next;

			if (!split_flags[buf_idx]) {
				/* it's the last packet of the set */
				start->hash = end->hash;
				start->ol_flags = end->ol_flags;
				/* we need to strip crc for the whole packet */
				start->pkt_len -= rxq->crc_len;
				if (end->data_len > rxq->crc_len) {
					end->data_len -= rxq->crc_len;
				} else {
					/* free up last mbuf */
					struct rte_mbuf *secondlast = start;

					start->nb_segs--;
					while (secondlast->next != end)
						secondlast = secondlast->next;
					secondlast->data_len -= (rxq->crc_len -
							end->data_len);
					secondlast->next = NULL;
					rte_pktmbuf_free_seg(end);
				}
				pkts[pkt_idx++] = start;
				start = NULL;
				end = NULL;
			}
		} else {
			/* not processing a split packet */
			if (!split_flags[buf_idx]) {
				/* not a split packet, save and skip */
				pkts[pkt_idx++] = rx_bufs[buf_idx];
				continue;
			}
			end = start = rx_bufs[buf_idx];
			rx_bufs[buf_idx]->data_len += rxq->crc_len;
			rx_bufs[buf_idx]->pkt_len += rxq->crc_len;
		}
	}

	/* save the partial packet for next time */
	rxq->pkt_first_seg = start;
	rxq->pkt_last_seg = end;
	memcpy(rx_bufs, pkts, pkt_idx * (sizeof(*pkts)));
	return pkt_idx;
}

static __rte_always_inline int
avf_tx_free_bufs(struct avf_tx_queue *txq)
{
	struct avf_tx_entry *txep;
	uint32_t n;
	uint32_t i;
	int nb_free = 0;
	struct rte_mbuf *m, *free[AVF_VPMD_TX_MAX_FREE_BUF];

	/* check DD bits on threshold descriptor */
	if ((txq->tx_ring[txq->next_dd].cmd_type_offset_bsz &
			rte_cpu_to_le_64(AVF_TXD_QW1_DTYPE_MASK)) !=
			rte_cpu_to_le_64(AVF_TX_DESC_DTYPE_DESC_DONE))
		return 0;

	n = txq->rs_thresh;

	 /* first buffer to free from S/W ring is at index
	  * tx_next_dd - (tx_rs_thresh-1)
	  */
	txep = &txq->sw_ring[txq->next_dd - (n - 1)];
	m = rte_pktmbuf_prefree_seg(txep[0].mbuf);
	if (likely(m != NULL)) {
		free[0] = m;
		nb_free = 1;
		for (i = 1; i < n; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (likely(m != NULL)) {
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
			m = rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (m)
				rte_mempool_put(m->pool, m);
		}
	}

	/* buffers were freed, update counters */
	txq->nb_free = (uint16_t)(txq->nb_free + txq->rs_thresh);
	txq->next_dd = (uint16_t)(txq->next_dd + txq->rs_thresh);
	if (txq->next_dd >= txq->nb_tx_desc)
		txq->next_dd = (uint16_t)(txq->rs_thresh - 1);

	return txq->rs_thresh;
}

static __rte_always_inline void
tx_backlog_entry(struct avf_tx_entry *txep,
		 struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i;

	for (i = 0; i < (int)nb_pkts; ++i)
		txep[i].mbuf = tx_pkts[i];
}

static inline void
_avf_rx_queue_release_mbufs_vec(struct avf_rx_queue *rxq)
{
	const unsigned int mask = rxq->nb_rx_desc - 1;
	unsigned int i;

	if (!rxq->sw_ring || rxq->rxrearm_nb >= rxq->nb_rx_desc)
		return;

	/* free all mbufs that are valid in the ring */
	if (rxq->rxrearm_nb == 0) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i])
				rte_pktmbuf_free_seg(rxq->sw_ring[i]);
		}
	} else {
		for (i = rxq->rx_tail;
		     i != rxq->rxrearm_start;
		     i = (i + 1) & mask) {
			if (rxq->sw_ring[i])
				rte_pktmbuf_free_seg(rxq->sw_ring[i]);
		}
	}

	rxq->rxrearm_nb = rxq->nb_rx_desc;

	/* set all entries to NULL */
	memset(rxq->sw_ring, 0, sizeof(rxq->sw_ring[0]) * rxq->nb_rx_desc);
}

static inline void
_avf_tx_queue_release_mbufs_vec(struct avf_tx_queue *txq)
{
	unsigned i;
	const uint16_t max_desc = (uint16_t)(txq->nb_tx_desc - 1);

	if (!txq->sw_ring || txq->nb_free == max_desc)
		return;

	i = txq->next_dd - txq->rs_thresh + 1;
	if (txq->tx_tail < i) {
		for (; i < txq->nb_tx_desc; i++) {
			rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
			txq->sw_ring[i].mbuf = NULL;
		}
		i = 0;
	}
}

static inline int
avf_rxq_vec_setup_default(struct avf_rx_queue *rxq)
{
	uintptr_t p;
	struct rte_mbuf mb_def = { .buf_addr = 0 }; /* zeroed mbuf */

	mb_def.nb_segs = 1;
	mb_def.data_off = RTE_PKTMBUF_HEADROOM;
	mb_def.port = rxq->port_id;
	rte_mbuf_refcnt_set(&mb_def, 1);

	/* prevent compiler reordering: rearm_data covers previous fields */
	rte_compiler_barrier();
	p = (uintptr_t)&mb_def.rearm_data;
	rxq->mbuf_initializer = *(uint64_t *)p;
	return 0;
}
#endif
