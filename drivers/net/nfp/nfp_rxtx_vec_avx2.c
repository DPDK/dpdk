/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Corigine, Inc.
 * All rights reserved.
 */

#include <stdbool.h>

#include <bus_pci_driver.h>
#include <ethdev_driver.h>
#include <rte_cpuflags.h>
#include <rte_vect.h>

#include "nfp_logs.h"
#include "nfp_net_common.h"
#include "nfp_net_meta.h"
#include "nfp_rxtx_vec.h"

bool
nfp_net_get_avx2_supported(void)
{
	if (rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_256 &&
			rte_cpu_get_flag_enabled(RTE_CPUFLAG_AVX2) == 1)
		return true;

	return false;
}

static inline void
nfp_vec_avx2_recv_set_des1(struct nfp_net_rxq *rxq,
		struct nfp_net_rx_desc *rxds,
		struct rte_mbuf *rxb)
{
	__m128i dma;
	__m128i dma_hi;
	__m128i vaddr0;
	__m128i hdr_room = _mm_set_epi64x(0, RTE_PKTMBUF_HEADROOM);

	dma = _mm_add_epi64(_mm_loadu_si128((__m128i *)&rxb->buf_addr), hdr_room);
	dma_hi = _mm_srli_epi64(dma, 32);
	vaddr0 = _mm_unpacklo_epi32(dma_hi, dma);

	_mm_storel_epi64((void *)rxds, vaddr0);

	rxq->rd_p = (rxq->rd_p + 1) & (rxq->rx_count - 1);
}

static inline void
nfp_vec_avx2_recv_set_des4(struct nfp_net_rxq *rxq,
		struct nfp_net_rx_desc *rxds,
		struct rte_mbuf **rxb)
{
	__m128i dma;
	__m128i dma_hi;
	__m128i vaddr0;
	__m128i vaddr1;
	__m128i vaddr2;
	__m128i vaddr3;
	__m128i vaddr0_1;
	__m128i vaddr2_3;
	__m256i vaddr0_3;
	__m128i hdr_room = _mm_set_epi64x(0, RTE_PKTMBUF_HEADROOM);

	dma = _mm_add_epi64(_mm_loadu_si128((__m128i *)&rxb[0]->buf_addr), hdr_room);
	dma_hi = _mm_srli_epi64(dma, 32);
	vaddr0 = _mm_unpacklo_epi32(dma_hi, dma);

	dma = _mm_add_epi64(_mm_loadu_si128((__m128i *)&rxb[1]->buf_addr), hdr_room);
	dma_hi = _mm_srli_epi64(dma, 32);
	vaddr1 = _mm_unpacklo_epi32(dma_hi, dma);

	dma = _mm_add_epi64(_mm_loadu_si128((__m128i *)&rxb[2]->buf_addr), hdr_room);
	dma_hi = _mm_srli_epi64(dma, 32);
	vaddr2 = _mm_unpacklo_epi32(dma_hi, dma);

	dma = _mm_add_epi64(_mm_loadu_si128((__m128i *)&rxb[3]->buf_addr), hdr_room);
	dma_hi = _mm_srli_epi64(dma, 32);
	vaddr3 = _mm_unpacklo_epi32(dma_hi, dma);

	vaddr0_1 = _mm_unpacklo_epi64(vaddr0, vaddr1);
	vaddr2_3 = _mm_unpacklo_epi64(vaddr2, vaddr3);

	vaddr0_3 = _mm256_inserti128_si256(_mm256_castsi128_si256(vaddr0_1),
			vaddr2_3, 1);

	_mm256_store_si256((void *)rxds, vaddr0_3);

	rxq->rd_p = (rxq->rd_p + 4) & (rxq->rx_count - 1);
}

static inline void
nfp_vec_avx2_recv_set_rxpkt1(struct nfp_net_rxq *rxq,
		struct nfp_net_rx_desc *rxds,
		struct rte_mbuf *rx_pkt)
{
	uint16_t data_len;
	struct nfp_net_hw *hw = rxq->hw;
	struct nfp_net_meta_parsed meta;

	data_len = rte_le_to_cpu_16(rxds->rxd.data_len);
	rx_pkt->data_len = data_len - NFP_DESC_META_LEN(rxds);
	/* Size of the whole packet. We just support 1 segment */
	rx_pkt->pkt_len = data_len - NFP_DESC_META_LEN(rxds);

	/* Filling the received mbuf with packet info */
	if (hw->rx_offset)
		rx_pkt->data_off = RTE_PKTMBUF_HEADROOM + hw->rx_offset;
	else
		rx_pkt->data_off = RTE_PKTMBUF_HEADROOM + NFP_DESC_META_LEN(rxds);

	rx_pkt->port = rxq->port_id;
	rx_pkt->nb_segs = 1;
	rx_pkt->next = NULL;

	nfp_net_meta_parse(rxds, rxq, hw, rx_pkt, &meta);

	nfp_net_parse_ptype(rxq, rxds, rx_pkt);

	/* Checking the checksum flag */
	nfp_net_rx_cksum(rxq, rxds, rx_pkt);
}

static inline int
nfp_vec_avx2_recv1(struct nfp_net_rxq *rxq,
		struct nfp_net_rx_desc *rxds,
		struct rte_mbuf **rxb,
		struct rte_mbuf *rx_pkt)
{
	/* Allocate a new mbuf into the software ring. */
	if (rte_pktmbuf_alloc_bulk(rxq->mem_pool, rxb, 1) < 0) {
		PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u queue_id=%hu.",
				rxq->port_id, rxq->qidx);
		nfp_net_mbuf_alloc_failed(rxq);
		return -ENOMEM;
	}

	nfp_vec_avx2_recv_set_rxpkt1(rxq, rxds, rx_pkt);

	nfp_vec_avx2_recv_set_des1(rxq, rxds, *rxb);

	return 0;
}

static inline int
nfp_vec_avx2_recv4(struct nfp_net_rxq *rxq,
		struct nfp_net_rx_desc *rxds,
		struct rte_mbuf **rxb,
		struct rte_mbuf **rx_pkts)
{
	/* Allocate 4 new mbufs into the software ring. */
	if (rte_pktmbuf_alloc_bulk(rxq->mem_pool, rxb, 4) < 0) {
		PMD_RX_LOG(DEBUG, "RX mbuf bulk alloc failed port_id=%u queue_id=%hu.",
				rxq->port_id, rxq->qidx);
		return -ENOMEM;
	}

	nfp_vec_avx2_recv_set_rxpkt1(rxq, rxds, rx_pkts[0]);
	nfp_vec_avx2_recv_set_rxpkt1(rxq, rxds + 1, rx_pkts[1]);
	nfp_vec_avx2_recv_set_rxpkt1(rxq, rxds + 2, rx_pkts[2]);
	nfp_vec_avx2_recv_set_rxpkt1(rxq, rxds + 3, rx_pkts[3]);

	nfp_vec_avx2_recv_set_des4(rxq, rxds, rxb);

	return 0;
}

static inline bool
nfp_vec_avx2_recv_check_packets4(struct nfp_net_rx_desc *rxds)
{
	__m256i data = _mm256_loadu_si256((void *)rxds);

	if ((_mm256_extract_epi8(data, 3) & PCIE_DESC_RX_DD) == 0 ||
			(_mm256_extract_epi8(data, 11) & PCIE_DESC_RX_DD) == 0 ||
			(_mm256_extract_epi8(data, 19) & PCIE_DESC_RX_DD) == 0 ||
			(_mm256_extract_epi8(data, 27) & PCIE_DESC_RX_DD) == 0)
		return false;

	return true;
}

uint16_t
nfp_net_vec_avx2_recv_pkts(void *rx_queue,
		struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	uint16_t avail;
	uint16_t nb_hold;
	bool burst_receive;
	struct rte_mbuf **rxb;
	struct nfp_net_rx_desc *rxds;
	struct nfp_net_rxq *rxq = rx_queue;

	if (unlikely(rxq == NULL)) {
		PMD_RX_LOG(ERR, "RX Bad queue.");
		return 0;
	}

	avail = 0;
	nb_hold = 0;
	burst_receive = true;
	while (avail < nb_pkts) {
		rxds = &rxq->rxds[rxq->rd_p];
		rxb = &rxq->rxbufs[rxq->rd_p].mbuf;

		if ((_mm_extract_epi8(_mm_loadu_si128((void *)(rxds)), 3)
				& PCIE_DESC_RX_DD) == 0)
			goto recv_end;

		rte_prefetch0(rxq->rxbufs[rxq->rd_p].mbuf);

		if ((rxq->rd_p & 0x3) == 0) {
			rte_prefetch0(&rxq->rxds[rxq->rd_p]);
			rte_prefetch0(&rxq->rxbufs[rxq->rd_p]);
			rte_prefetch0(rxq->rxbufs[rxq->rd_p + 1].mbuf);
			rte_prefetch0(rxq->rxbufs[rxq->rd_p + 2].mbuf);
			rte_prefetch0(rxq->rxbufs[rxq->rd_p + 3].mbuf);
		}

		if ((rxq->rd_p & 0x7) == 0) {
			rte_prefetch0(rxq->rxbufs[rxq->rd_p + 4].mbuf);
			rte_prefetch0(rxq->rxbufs[rxq->rd_p + 5].mbuf);
			rte_prefetch0(rxq->rxbufs[rxq->rd_p + 6].mbuf);
			rte_prefetch0(rxq->rxbufs[rxq->rd_p + 7].mbuf);
		}

		/*
		 * If can not receive burst, just receive one.
		 * 1. Rx ring will coming to the tail.
		 * 2. Do not need to receive 4 packets.
		 * 3. If pointer address unaligned on 32-bit boundary.
		 * 4. Rx ring does not have 4 packets or alloc 4 mbufs failed.
		 */
		if ((rxq->rx_count - rxq->rd_p) < 4 ||
				(nb_pkts - avail) < 4 ||
				((uintptr_t)rxds & 0x1F) != 0 ||
				!burst_receive) {
			_mm_storel_epi64((void *)&rx_pkts[avail],
					_mm_loadu_si128((void *)rxb));

			if (nfp_vec_avx2_recv1(rxq, rxds, rxb, rx_pkts[avail]) != 0)
				goto recv_end;

			avail++;
			nb_hold++;
			continue;
		}

		burst_receive = nfp_vec_avx2_recv_check_packets4(rxds);
		if (!burst_receive)
			continue;

		_mm256_storeu_si256((void *)&rx_pkts[avail],
				_mm256_loadu_si256((void *)rxb));

		if (nfp_vec_avx2_recv4(rxq, rxds, rxb, &rx_pkts[avail]) != 0) {
			burst_receive = false;
			continue;
		}

		avail += 4;
		nb_hold += 4;
	}

recv_end:
	if (nb_hold == 0)
		return nb_hold;

	PMD_RX_LOG(DEBUG, "RX port_id=%u queue_id=%u, %d packets received.",
			rxq->port_id, (unsigned int)rxq->qidx, nb_hold);

	nb_hold += rxq->nb_rx_hold;

	/*
	 * FL descriptors needs to be written before incrementing the
	 * FL queue WR pointer
	 */
	rte_wmb();
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "The port=%hu queue=%hu nb_hold=%hu avail=%hu.",
				rxq->port_id, rxq->qidx, nb_hold, avail);
		nfp_qcp_ptr_add(rxq->qcp_fl, NFP_QCP_WRITE_PTR, nb_hold);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;

	return avail;
}
