/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _ICE_RXTX_VEC_COMMON_H_
#define _ICE_RXTX_VEC_COMMON_H_

#include "ice_rxtx.h"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static inline uint16_t
ice_rx_reassemble_packets(struct ice_rx_queue *rxq, struct rte_mbuf **rx_bufs,
			  uint16_t nb_bufs, uint8_t *split_flags)
{
	struct rte_mbuf *pkts[ICE_VPMD_RX_BURST] = {0}; /*finished pkts*/
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
				start->vlan_tci = end->vlan_tci;
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
			start = rx_bufs[buf_idx];
			end = start;
			rx_bufs[buf_idx]->data_len += rxq->crc_len;
			rx_bufs[buf_idx]->pkt_len += rxq->crc_len;
		}
	}

	/* save the partial packet for next time */
	rxq->pkt_first_seg = start;
	rxq->pkt_last_seg = end;
	rte_memcpy(rx_bufs, pkts, pkt_idx * (sizeof(*pkts)));
	return pkt_idx;
}

static __rte_always_inline int
ice_tx_free_bufs(struct ice_tx_queue *txq)
{
	struct ice_tx_entry *txep;
	uint32_t n;
	uint32_t i;
	int nb_free = 0;
	struct rte_mbuf *m, *free[ICE_TX_MAX_FREE_BUF_SZ];

	/* check DD bits on threshold descriptor */
	if ((txq->tx_ring[txq->tx_next_dd].cmd_type_offset_bsz &
			rte_cpu_to_le_64(ICE_TXD_QW1_DTYPE_M)) !=
			rte_cpu_to_le_64(ICE_TX_DESC_DTYPE_DESC_DONE))
		return 0;

	n = txq->tx_rs_thresh;

	 /* first buffer to free from S/W ring is at index
	  * tx_next_dd - (tx_rs_thresh-1)
	  */
	txep = &txq->sw_ring[txq->tx_next_dd - (n - 1)];
	m = rte_pktmbuf_prefree_seg(txep[0].mbuf);
	if (likely(m)) {
		free[0] = m;
		nb_free = 1;
		for (i = 1; i < n; i++) {
			m = rte_pktmbuf_prefree_seg(txep[i].mbuf);
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
			m = rte_pktmbuf_prefree_seg(txep[i].mbuf);
			if (m)
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

static __rte_always_inline void
ice_tx_backlog_entry(struct ice_tx_entry *txep,
		     struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i;

	for (i = 0; i < (int)nb_pkts; ++i)
		txep[i].mbuf = tx_pkts[i];
}

static inline void
_ice_rx_queue_release_mbufs_vec(struct ice_rx_queue *rxq)
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

static inline void
_ice_tx_queue_release_mbufs_vec(struct ice_tx_queue *txq)
{
	uint16_t i;

	if (unlikely(!txq || !txq->sw_ring)) {
		PMD_DRV_LOG(DEBUG, "Pointer to rxq or sw_ring is NULL");
		return;
	}

	/**
	 *  vPMD tx will not set sw_ring's mbuf to NULL after free,
	 *  so need to free remains more carefully.
	 */
	i = txq->tx_next_dd - txq->tx_rs_thresh + 1;

#ifdef CC_AVX512_SUPPORT
	struct rte_eth_dev *dev = &rte_eth_devices[txq->vsi->adapter->pf.dev_data->port_id];

	if (dev->tx_pkt_burst == ice_xmit_pkts_vec_avx512) {
		struct ice_vec_tx_entry *swr = (void *)txq->sw_ring;

		if (txq->tx_tail < i) {
			for (; i < txq->nb_tx_desc; i++) {
				rte_pktmbuf_free_seg(swr[i].mbuf);
				swr[i].mbuf = NULL;
			}
			i = 0;
		}
		for (; i < txq->tx_tail; i++) {
			rte_pktmbuf_free_seg(swr[i].mbuf);
			swr[i].mbuf = NULL;
		}
	} else
#endif
	{
		if (txq->tx_tail < i) {
			for (; i < txq->nb_tx_desc; i++) {
				rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
				txq->sw_ring[i].mbuf = NULL;
			}
			i = 0;
		}
		for (; i < txq->tx_tail; i++) {
			rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
			txq->sw_ring[i].mbuf = NULL;
		}
	}
}

static inline int
ice_rxq_vec_setup_default(struct ice_rx_queue *rxq)
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

static inline int
ice_rx_vec_queue_default(struct ice_rx_queue *rxq)
{
	if (!rxq)
		return -1;

	if (!rte_is_power_of_2(rxq->nb_rx_desc))
		return -1;

	if (rxq->rx_free_thresh < ICE_VPMD_RX_BURST)
		return -1;

	if (rxq->nb_rx_desc % rxq->rx_free_thresh)
		return -1;

	if (rxq->proto_xtr != PROTO_XTR_NONE)
		return -1;

	return 0;
}

#define ICE_NO_VECTOR_FLAGS (				 \
		DEV_TX_OFFLOAD_MULTI_SEGS |		 \
		DEV_TX_OFFLOAD_VLAN_INSERT |		 \
		DEV_TX_OFFLOAD_IPV4_CKSUM |		 \
		DEV_TX_OFFLOAD_SCTP_CKSUM |		 \
		DEV_TX_OFFLOAD_UDP_CKSUM |		 \
		DEV_TX_OFFLOAD_TCP_TSO |		 \
		DEV_TX_OFFLOAD_TCP_CKSUM)

static inline int
ice_tx_vec_queue_default(struct ice_tx_queue *txq)
{
	if (!txq)
		return -1;

	if (txq->offloads & ICE_NO_VECTOR_FLAGS)
		return -1;

	if (txq->tx_rs_thresh < ICE_VPMD_TX_BURST ||
	    txq->tx_rs_thresh > ICE_TX_MAX_FREE_BUF_SZ)
		return -1;

	return 0;
}

static inline int
ice_rx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct ice_rx_queue *rxq;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (ice_rx_vec_queue_default(rxq))
			return -1;
	}

	return 0;
}

static inline int
ice_tx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct ice_tx_queue *txq;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (ice_tx_vec_queue_default(txq))
			return -1;
	}

	return 0;
}

#ifdef CC_AVX2_SUPPORT
static __rte_always_inline void
ice_rxq_rearm_common(struct ice_rx_queue *rxq, __rte_unused bool avx512)
{
	int i;
	uint16_t rx_id;
	volatile union ice_rx_flex_desc *rxdp;
	struct ice_rx_entry *rxep = &rxq->sw_ring[rxq->rxrearm_start];

	rxdp = rxq->rx_ring + rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mp,
				 (void *)rxep,
				 ICE_RXQ_REARM_THRESH) < 0) {
		if (rxq->rxrearm_nb + ICE_RXQ_REARM_THRESH >=
		    rxq->nb_rx_desc) {
			__m128i dma_addr0;

			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < ICE_DESCS_PER_LOOP; i++) {
				rxep[i].mbuf = &rxq->fake_mbuf;
				_mm_store_si128((__m128i *)&rxdp[i].read,
						dma_addr0);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			ICE_RXQ_REARM_THRESH;
		return;
	}

#ifndef RTE_LIBRTE_ICE_16BYTE_RX_DESC
	struct rte_mbuf *mb0, *mb1;
	__m128i dma_addr0, dma_addr1;
	__m128i hdr_room = _mm_set_epi64x(RTE_PKTMBUF_HEADROOM,
			RTE_PKTMBUF_HEADROOM);
	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < ICE_RXQ_REARM_THRESH; i += 2, rxep += 2) {
		__m128i vaddr0, vaddr1;

		mb0 = rxep[0].mbuf;
		mb1 = rxep[1].mbuf;

		/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
				offsetof(struct rte_mbuf, buf_addr) + 8);
		vaddr0 = _mm_loadu_si128((__m128i *)&mb0->buf_addr);
		vaddr1 = _mm_loadu_si128((__m128i *)&mb1->buf_addr);

		/* convert pa to dma_addr hdr/data */
		dma_addr0 = _mm_unpackhi_epi64(vaddr0, vaddr0);
		dma_addr1 = _mm_unpackhi_epi64(vaddr1, vaddr1);

		/* add headroom to pa values */
		dma_addr0 = _mm_add_epi64(dma_addr0, hdr_room);
		dma_addr1 = _mm_add_epi64(dma_addr1, hdr_room);

		/* flush desc with pa dma_addr */
		_mm_store_si128((__m128i *)&rxdp++->read, dma_addr0);
		_mm_store_si128((__m128i *)&rxdp++->read, dma_addr1);
	}
#else
#ifdef CC_AVX512_SUPPORT
	if (avx512) {
		struct rte_mbuf *mb0, *mb1, *mb2, *mb3;
		struct rte_mbuf *mb4, *mb5, *mb6, *mb7;
		__m512i dma_addr0_3, dma_addr4_7;
		__m512i hdr_room = _mm512_set1_epi64(RTE_PKTMBUF_HEADROOM);
		/* Initialize the mbufs in vector, process 8 mbufs in one loop */
		for (i = 0; i < ICE_RXQ_REARM_THRESH;
				i += 8, rxep += 8, rxdp += 8) {
			__m128i vaddr0, vaddr1, vaddr2, vaddr3;
			__m128i vaddr4, vaddr5, vaddr6, vaddr7;
			__m256i vaddr0_1, vaddr2_3;
			__m256i vaddr4_5, vaddr6_7;
			__m512i vaddr0_3, vaddr4_7;

			mb0 = rxep[0].mbuf;
			mb1 = rxep[1].mbuf;
			mb2 = rxep[2].mbuf;
			mb3 = rxep[3].mbuf;
			mb4 = rxep[4].mbuf;
			mb5 = rxep[5].mbuf;
			mb6 = rxep[6].mbuf;
			mb7 = rxep[7].mbuf;

			/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
			RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
					offsetof(struct rte_mbuf, buf_addr) + 8);
			vaddr0 = _mm_loadu_si128((__m128i *)&mb0->buf_addr);
			vaddr1 = _mm_loadu_si128((__m128i *)&mb1->buf_addr);
			vaddr2 = _mm_loadu_si128((__m128i *)&mb2->buf_addr);
			vaddr3 = _mm_loadu_si128((__m128i *)&mb3->buf_addr);
			vaddr4 = _mm_loadu_si128((__m128i *)&mb4->buf_addr);
			vaddr5 = _mm_loadu_si128((__m128i *)&mb5->buf_addr);
			vaddr6 = _mm_loadu_si128((__m128i *)&mb6->buf_addr);
			vaddr7 = _mm_loadu_si128((__m128i *)&mb7->buf_addr);

			/**
			 * merge 0 & 1, by casting 0 to 256-bit and inserting 1
			 * into the high lanes. Similarly for 2 & 3, and so on.
			 */
			vaddr0_1 =
				_mm256_inserti128_si256(_mm256_castsi128_si256(vaddr0),
							vaddr1, 1);
			vaddr2_3 =
				_mm256_inserti128_si256(_mm256_castsi128_si256(vaddr2),
							vaddr3, 1);
			vaddr4_5 =
				_mm256_inserti128_si256(_mm256_castsi128_si256(vaddr4),
							vaddr5, 1);
			vaddr6_7 =
				_mm256_inserti128_si256(_mm256_castsi128_si256(vaddr6),
							vaddr7, 1);
			vaddr0_3 =
				_mm512_inserti64x4(_mm512_castsi256_si512(vaddr0_1),
							vaddr2_3, 1);
			vaddr4_7 =
				_mm512_inserti64x4(_mm512_castsi256_si512(vaddr4_5),
							vaddr6_7, 1);

			/* convert pa to dma_addr hdr/data */
			dma_addr0_3 = _mm512_unpackhi_epi64(vaddr0_3, vaddr0_3);
			dma_addr4_7 = _mm512_unpackhi_epi64(vaddr4_7, vaddr4_7);

			/* add headroom to pa values */
			dma_addr0_3 = _mm512_add_epi64(dma_addr0_3, hdr_room);
			dma_addr4_7 = _mm512_add_epi64(dma_addr4_7, hdr_room);

			/* flush desc with pa dma_addr */
			_mm512_store_si512((__m512i *)&rxdp->read, dma_addr0_3);
			_mm512_store_si512((__m512i *)&(rxdp + 4)->read, dma_addr4_7);
		}
	} else
#endif
	{
		struct rte_mbuf *mb0, *mb1, *mb2, *mb3;
		__m256i dma_addr0_1, dma_addr2_3;
		__m256i hdr_room = _mm256_set1_epi64x(RTE_PKTMBUF_HEADROOM);
		/* Initialize the mbufs in vector, process 4 mbufs in one loop */
		for (i = 0; i < ICE_RXQ_REARM_THRESH;
				i += 4, rxep += 4, rxdp += 4) {
			__m128i vaddr0, vaddr1, vaddr2, vaddr3;
			__m256i vaddr0_1, vaddr2_3;

			mb0 = rxep[0].mbuf;
			mb1 = rxep[1].mbuf;
			mb2 = rxep[2].mbuf;
			mb3 = rxep[3].mbuf;

			/* load buf_addr(lo 64bit) and buf_iova(hi 64bit) */
			RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
					offsetof(struct rte_mbuf, buf_addr) + 8);
			vaddr0 = _mm_loadu_si128((__m128i *)&mb0->buf_addr);
			vaddr1 = _mm_loadu_si128((__m128i *)&mb1->buf_addr);
			vaddr2 = _mm_loadu_si128((__m128i *)&mb2->buf_addr);
			vaddr3 = _mm_loadu_si128((__m128i *)&mb3->buf_addr);

			/**
			 * merge 0 & 1, by casting 0 to 256-bit and inserting 1
			 * into the high lanes. Similarly for 2 & 3
			 */
			vaddr0_1 =
				_mm256_inserti128_si256(_mm256_castsi128_si256(vaddr0),
							vaddr1, 1);
			vaddr2_3 =
				_mm256_inserti128_si256(_mm256_castsi128_si256(vaddr2),
							vaddr3, 1);

			/* convert pa to dma_addr hdr/data */
			dma_addr0_1 = _mm256_unpackhi_epi64(vaddr0_1, vaddr0_1);
			dma_addr2_3 = _mm256_unpackhi_epi64(vaddr2_3, vaddr2_3);

			/* add headroom to pa values */
			dma_addr0_1 = _mm256_add_epi64(dma_addr0_1, hdr_room);
			dma_addr2_3 = _mm256_add_epi64(dma_addr2_3, hdr_room);

			/* flush desc with pa dma_addr */
			_mm256_store_si256((__m256i *)&rxdp->read, dma_addr0_1);
			_mm256_store_si256((__m256i *)&(rxdp + 2)->read, dma_addr2_3);
		}
	}

#endif

	rxq->rxrearm_start += ICE_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= ICE_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			     (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	ICE_PCI_REG_WC_WRITE(rxq->qrx_tail, rx_id);
}
#endif

#endif
