/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _ICE_RXTX_COMMON_AVX_H_
#define _ICE_RXTX_COMMON_AVX_H_

#include "ice_rxtx.h"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#ifdef __AVX2__
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
#ifdef __AVX512VL__
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
#endif /* __AVX512VL__ */
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
#endif /* __AVX2__ */

#endif /* _ICE_RXTX_COMMON_AVX_H_ */
