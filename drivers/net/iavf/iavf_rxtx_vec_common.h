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

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static __rte_always_inline uint16_t
reassemble_packets(struct iavf_rx_queue *rxq, struct rte_mbuf **rx_bufs,
		   uint16_t nb_bufs, uint8_t *split_flags)
{
	struct rte_mbuf *pkts[IAVF_VPMD_RX_MAX_BURST];
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
iavf_tx_free_bufs(struct iavf_tx_queue *txq)
{
	struct iavf_tx_entry *txep;
	uint32_t n;
	uint32_t i;
	int nb_free = 0;
	struct rte_mbuf *m, *free[IAVF_VPMD_TX_MAX_FREE_BUF];

	/* check DD bits on threshold descriptor */
	if ((txq->tx_ring[txq->next_dd].cmd_type_offset_bsz &
			rte_cpu_to_le_64(IAVF_TXD_QW1_DTYPE_MASK)) !=
			rte_cpu_to_le_64(IAVF_TX_DESC_DTYPE_DESC_DONE))
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
tx_backlog_entry(struct iavf_tx_entry *txep,
		 struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i;

	for (i = 0; i < (int)nb_pkts; ++i)
		txep[i].mbuf = tx_pkts[i];
}

static inline void
_iavf_rx_queue_release_mbufs_vec(struct iavf_rx_queue *rxq)
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
_iavf_tx_queue_release_mbufs_vec(struct iavf_tx_queue *txq)
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
iavf_rxq_vec_setup_default(struct iavf_rx_queue *rxq)
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
iavf_rx_vec_queue_default(struct iavf_rx_queue *rxq)
{
	if (!rxq)
		return -1;

	if (!rte_is_power_of_2(rxq->nb_rx_desc))
		return -1;

	if (rxq->rx_free_thresh < IAVF_VPMD_RX_MAX_BURST)
		return -1;

	if (rxq->nb_rx_desc % rxq->rx_free_thresh)
		return -1;

	if (rxq->proto_xtr != IAVF_PROTO_XTR_NONE)
		return -1;

	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
		return -1;

	if (rxq->offloads & IAVF_RX_VECTOR_OFFLOAD)
		return IAVF_VECTOR_OFFLOAD_PATH;

	return IAVF_VECTOR_PATH;
}

static inline int
iavf_tx_vec_queue_default(struct iavf_tx_queue *txq)
{
	if (!txq)
		return -1;

	if (txq->rs_thresh < IAVF_VPMD_TX_MAX_BURST ||
	    txq->rs_thresh > IAVF_VPMD_TX_MAX_FREE_BUF)
		return -1;

	if (txq->offloads & IAVF_TX_NO_VECTOR_FLAGS)
		return -1;

	/**
	 * Vlan tci needs to be inserted via ctx desc, if the vlan_flag is L2TAG2.
	 * Tunneling parameters and other fields need be configured in ctx desc
	 * if the outer checksum offload is enabled.
	 */
	if (txq->offloads & (IAVF_TX_VECTOR_OFFLOAD | IAVF_TX_VECTOR_OFFLOAD_CTX)) {
		if (txq->offloads & IAVF_TX_VECTOR_OFFLOAD_CTX) {
			if (txq->vlan_flag == IAVF_TX_FLAGS_VLAN_TAG_LOC_L2TAG2) {
				txq->use_ctx = 1;
				return IAVF_VECTOR_CTX_OFFLOAD_PATH;
			} else {
				return -1;
			}
		} else {
			return IAVF_VECTOR_OFFLOAD_PATH;
		}
	} else {
		return IAVF_VECTOR_PATH;
	}
}

static inline int
iavf_rx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct iavf_rx_queue *rxq;
	int ret;
	int result = 0;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		ret = iavf_rx_vec_queue_default(rxq);

		if (ret < 0)
			return -1;
		if (ret > result)
			result = ret;
	}

	return result;
}

static inline int
iavf_tx_vec_dev_check_default(struct rte_eth_dev *dev)
{
	int i;
	struct iavf_tx_queue *txq;
	int ret;
	int result = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		ret = iavf_tx_vec_queue_default(txq);

		if (ret < 0)
			return -1;
		if (ret > result)
			result = ret;
	}

	return result;
}

/******************************************************************************
 * If user knows a specific offload is not enabled by APP,
 * the macro can be commented to save the effort of fast path.
 * Currently below 2 features are supported in TX path,
 * 1, checksum offload
 * 2, VLAN/QINQ insertion
 ******************************************************************************/
#define IAVF_TX_CSUM_OFFLOAD
#define IAVF_TX_VLAN_QINQ_OFFLOAD

static __rte_always_inline void
iavf_txd_enable_offload(__rte_unused struct rte_mbuf *tx_pkt,
			uint64_t *txd_hi)
{
#if defined(IAVF_TX_CSUM_OFFLOAD) || defined(IAVF_TX_VLAN_QINQ_OFFLOAD)
	uint64_t ol_flags = tx_pkt->ol_flags;
#endif
	uint32_t td_cmd = 0;
#ifdef IAVF_TX_CSUM_OFFLOAD
	uint32_t td_offset = 0;
#endif

#ifdef IAVF_TX_CSUM_OFFLOAD
	/* Set MACLEN */
	if (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK)
		td_offset |= (tx_pkt->outer_l2_len >> 1)
			<< IAVF_TX_DESC_LENGTH_MACLEN_SHIFT;
	else
		td_offset |= (tx_pkt->l2_len >> 1)
			<< IAVF_TX_DESC_LENGTH_MACLEN_SHIFT;

	/* Enable L3 checksum offloads */
	if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM) {
		if (ol_flags & RTE_MBUF_F_TX_IPV4) {
			td_cmd |= IAVF_TX_DESC_CMD_IIPT_IPV4_CSUM;
			td_offset |= (tx_pkt->l3_len >> 2) <<
				     IAVF_TX_DESC_LENGTH_IPLEN_SHIFT;
		}
	} else if (ol_flags & RTE_MBUF_F_TX_IPV4) {
		td_cmd |= IAVF_TX_DESC_CMD_IIPT_IPV4;
		td_offset |= (tx_pkt->l3_len >> 2) <<
			     IAVF_TX_DESC_LENGTH_IPLEN_SHIFT;
	} else if (ol_flags & RTE_MBUF_F_TX_IPV6) {
		td_cmd |= IAVF_TX_DESC_CMD_IIPT_IPV6;
		td_offset |= (tx_pkt->l3_len >> 2) <<
			     IAVF_TX_DESC_LENGTH_IPLEN_SHIFT;
	}

	/* Enable L4 checksum offloads */
	switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
	case RTE_MBUF_F_TX_TCP_CKSUM:
		td_cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_TCP;
		td_offset |= (sizeof(struct rte_tcp_hdr) >> 2) <<
			     IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case RTE_MBUF_F_TX_SCTP_CKSUM:
		td_cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_SCTP;
		td_offset |= (sizeof(struct rte_sctp_hdr) >> 2) <<
			     IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	case RTE_MBUF_F_TX_UDP_CKSUM:
		td_cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_UDP;
		td_offset |= (sizeof(struct rte_udp_hdr) >> 2) <<
			     IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
		break;
	default:
		break;
	}

	*txd_hi |= ((uint64_t)td_offset) << IAVF_TXD_QW1_OFFSET_SHIFT;
#endif

#ifdef IAVF_TX_VLAN_QINQ_OFFLOAD
	if (ol_flags & (RTE_MBUF_F_TX_VLAN | RTE_MBUF_F_TX_QINQ)) {
		td_cmd |= IAVF_TX_DESC_CMD_IL2TAG1;
		*txd_hi |= ((uint64_t)tx_pkt->vlan_tci <<
			    IAVF_TXD_QW1_L2TAG1_SHIFT);
	}
#endif

	*txd_hi |= ((uint64_t)td_cmd) << IAVF_TXD_QW1_CMD_SHIFT;
}

#ifdef CC_AVX2_SUPPORT
static __rte_always_inline void
iavf_rxq_rearm_common(struct iavf_rx_queue *rxq, __rte_unused bool avx512)
{
	int i;
	uint16_t rx_id;
	volatile union iavf_rx_desc *rxdp;
	struct rte_mbuf **rxp = &rxq->sw_ring[rxq->rxrearm_start];

	rxdp = rxq->rx_ring + rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mp,
				 (void *)rxp,
				 IAVF_RXQ_REARM_THRESH) < 0) {
		if (rxq->rxrearm_nb + IAVF_RXQ_REARM_THRESH >=
		    rxq->nb_rx_desc) {
			__m128i dma_addr0;

			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < IAVF_VPMD_DESCS_PER_LOOP; i++) {
				rxp[i] = &rxq->fake_mbuf;
				_mm_store_si128((__m128i *)&rxdp[i].read,
						dma_addr0);
			}
		}
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			IAVF_RXQ_REARM_THRESH;
		return;
	}

#ifndef RTE_LIBRTE_IAVF_16BYTE_RX_DESC
	struct rte_mbuf *mb0, *mb1;
	__m128i dma_addr0, dma_addr1;
	__m128i hdr_room = _mm_set_epi64x(RTE_PKTMBUF_HEADROOM,
			RTE_PKTMBUF_HEADROOM);
	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < IAVF_RXQ_REARM_THRESH; i += 2, rxp += 2) {
		__m128i vaddr0, vaddr1;

		mb0 = rxp[0];
		mb1 = rxp[1];

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
		for (i = 0; i < IAVF_RXQ_REARM_THRESH;
				i += 8, rxp += 8, rxdp += 8) {
			__m128i vaddr0, vaddr1, vaddr2, vaddr3;
			__m128i vaddr4, vaddr5, vaddr6, vaddr7;
			__m256i vaddr0_1, vaddr2_3;
			__m256i vaddr4_5, vaddr6_7;
			__m512i vaddr0_3, vaddr4_7;

			mb0 = rxp[0];
			mb1 = rxp[1];
			mb2 = rxp[2];
			mb3 = rxp[3];
			mb4 = rxp[4];
			mb5 = rxp[5];
			mb6 = rxp[6];
			mb7 = rxp[7];

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
		for (i = 0; i < IAVF_RXQ_REARM_THRESH;
				i += 4, rxp += 4, rxdp += 4) {
			__m128i vaddr0, vaddr1, vaddr2, vaddr3;
			__m256i vaddr0_1, vaddr2_3;

			mb0 = rxp[0];
			mb1 = rxp[1];
			mb2 = rxp[2];
			mb3 = rxp[3];

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

	rxq->rxrearm_start += IAVF_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= IAVF_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			     (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	IAVF_PCI_REG_WC_WRITE(rxq->qrx_tail, rx_id);
}
#endif

#endif
