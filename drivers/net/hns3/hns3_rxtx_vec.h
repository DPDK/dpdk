/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 HiSilicon Limited.
 */

#ifndef HNS3_RXTX_VEC_H
#define HNS3_RXTX_VEC_H

#include "hns3_rxtx.h"
#include "hns3_ethdev.h"

static inline void
hns3_tx_bulk_free_buffers(struct hns3_tx_queue *txq)
{
	struct rte_mbuf **free = txq->free;
	struct hns3_entry *tx_entry;
	struct rte_mbuf *m;
	int nb_free = 0;
	uint16_t i;

	tx_entry = &txq->sw_ring[txq->next_to_clean];
	if (txq->mbuf_fast_free_en) {
		rte_mempool_put_bulk(tx_entry->mbuf->pool, (void **)tx_entry,
				     txq->tx_rs_thresh);
		for (i = 0; i < txq->tx_rs_thresh; i++)
			tx_entry[i].mbuf = NULL;
		goto update_field;
	}

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

update_field:
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
	uint16_t i;

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

static inline void
hns3_rxq_rearm_mbuf(struct hns3_rx_queue *rxq)
{
#define REARM_LOOP_STEP_NUM	4
	struct hns3_entry *rxep = &rxq->sw_ring[rxq->rx_rearm_start];
	struct hns3_desc *rxdp = rxq->rx_ring + rxq->rx_rearm_start;
	uint64_t dma_addr;
	int i;

	if (unlikely(rte_mempool_get_bulk(rxq->mb_pool, (void *)rxep,
					  HNS3_DEFAULT_RXQ_REARM_THRESH) < 0)) {
		/*
		 * Clear VLD bit for the first descriptor rearmed in case
		 * of going to receive packets later.
		 */
		rxdp[0].rx.bd_base_info = 0;
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
		return;
	}

	for (i = 0; i < HNS3_DEFAULT_RXQ_REARM_THRESH; i += REARM_LOOP_STEP_NUM,
		rxep += REARM_LOOP_STEP_NUM, rxdp += REARM_LOOP_STEP_NUM) {
		if (likely(i <
			HNS3_DEFAULT_RXQ_REARM_THRESH - REARM_LOOP_STEP_NUM)) {
			rte_prefetch_non_temporal(rxep[4].mbuf);
			rte_prefetch_non_temporal(rxep[5].mbuf);
			rte_prefetch_non_temporal(rxep[6].mbuf);
			rte_prefetch_non_temporal(rxep[7].mbuf);
		}

		dma_addr = rte_mbuf_data_iova_default(rxep[0].mbuf);
		rxdp[0].addr = rte_cpu_to_le_64(dma_addr);
		rxdp[0].rx.bd_base_info = 0;

		dma_addr = rte_mbuf_data_iova_default(rxep[1].mbuf);
		rxdp[1].addr = rte_cpu_to_le_64(dma_addr);
		rxdp[1].rx.bd_base_info = 0;

		dma_addr = rte_mbuf_data_iova_default(rxep[2].mbuf);
		rxdp[2].addr = rte_cpu_to_le_64(dma_addr);
		rxdp[2].rx.bd_base_info = 0;

		dma_addr = rte_mbuf_data_iova_default(rxep[3].mbuf);
		rxdp[3].addr = rte_cpu_to_le_64(dma_addr);
		rxdp[3].rx.bd_base_info = 0;
	}

	rxq->rx_rearm_start += HNS3_DEFAULT_RXQ_REARM_THRESH;
	if (rxq->rx_rearm_start >= rxq->nb_rx_desc)
		rxq->rx_rearm_start = 0;

	rxq->rx_rearm_nb -= HNS3_DEFAULT_RXQ_REARM_THRESH;

	hns3_write_reg_opt(rxq->io_head_reg, HNS3_DEFAULT_RXQ_REARM_THRESH);
}
#endif /* HNS3_RXTX_VEC_H */
