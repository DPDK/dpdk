/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#include <eal_export.h>
#include <rte_vect.h>

#include "idpf_common_rxtx.h"
#include "idpf_common_device.h"
#include "idpf_rxtx_vec_common.h"

static __rte_always_inline void
idpf_singleq_rx_rearm(struct idpf_rx_queue *rxq)
{
	int i;
	uint16_t rx_id;
	volatile union virtchnl2_rx_desc *rxdp = rxq->rx_ring;
	struct rte_mbuf **rxep = &rxq->sw_ring[rxq->rxrearm_start];

	rxdp += rxq->rxrearm_start;

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mp,
				 (void *)rxep,
				 IDPF_RXQ_REARM_THRESH) < 0) {
		if (rxq->rxrearm_nb + IDPF_RXQ_REARM_THRESH >=
		    rxq->nb_rx_desc) {
			__m128i dma_addr0;

			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < IDPF_VPMD_DESCS_PER_LOOP; i++) {
				rxep[i] = &rxq->fake_mbuf;
				_mm_store_si128(RTE_CAST_PTR(__m128i *, &rxdp[i].read), dma_addr0);
			}
		}
		rte_atomic_fetch_add_explicit(&rxq->rx_stats.mbuf_alloc_failed,
				IDPF_RXQ_REARM_THRESH, rte_memory_order_relaxed);
		return;
	}

	struct rte_mbuf *mb0, *mb1;
	__m128i dma_addr0, dma_addr1;
	__m128i hdr_room = _mm_set_epi64x(RTE_PKTMBUF_HEADROOM,
			RTE_PKTMBUF_HEADROOM);
	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < IDPF_RXQ_REARM_THRESH; i += 2, rxep += 2) {
		__m128i vaddr0, vaddr1;

		mb0 = rxep[0];
		mb1 = rxep[1];

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
		_mm_store_si128(RTE_CAST_PTR(__m128i *, &rxdp++->read), dma_addr0);
		_mm_store_si128(RTE_CAST_PTR(__m128i *, &rxdp++->read), dma_addr1);
	}

	rxq->rxrearm_start += IDPF_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= IDPF_RXQ_REARM_THRESH;

	rx_id = (uint16_t)((rxq->rxrearm_start == 0) ?
			     (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	IDPF_PCI_REG_WRITE(rxq->qrx_tail, rx_id);
}

static inline uint16_t
_idpf_singleq_recv_raw_pkts_vec_avx2(struct idpf_rx_queue *rxq, struct rte_mbuf **rx_pkts,
				     uint16_t nb_pkts)
{
#define IDPF_DESCS_PER_LOOP_AVX 8

	const uint32_t *ptype_tbl = rxq->adapter->ptype_tbl;
	const __m256i mbuf_init = _mm256_set_epi64x(0, 0,
			0, rxq->mbuf_initializer);
	struct rte_mbuf **sw_ring = &rxq->sw_ring[rxq->rx_tail];
	volatile union virtchnl2_rx_desc *rxdp = rxq->rx_ring;
	const int avx_aligned = ((rxq->rx_tail & 1) == 0);

	rxdp += rxq->rx_tail;

	rte_prefetch0(rxdp);

	/* nb_pkts has to be floor-aligned to IDPF_DESCS_PER_LOOP_AVX */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, IDPF_DESCS_PER_LOOP_AVX);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > IDPF_RXQ_REARM_THRESH)
		idpf_singleq_rx_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->flex_nic_wb.status_error0 &
			rte_cpu_to_le_32(1 << VIRTCHNL2_RX_FLEX_DESC_STATUS0_DD_S)))
		return 0;

	/* 8 packets DD mask, LSB in each 32-bit value */
	const __m256i dd_check = _mm256_set1_epi32(1);

	/* mask to shuffle from desc. to mbuf (2 descriptors)*/
	const __m256i shuf_msk =
		_mm256_set_epi8
			(/* first descriptor */
			 0xFF, 0xFF,
			 0xFF, 0xFF,	/* rss hash parsed separately */
			 11, 10,	/* octet 10~11, 16 bits vlan_macip */
			 5, 4,		/* octet 4~5, 16 bits data_len */
			 0xFF, 0xFF,	/* skip hi 16 bits pkt_len, zero out */
			 5, 4,		/* octet 4~5, 16 bits pkt_len */
			 0xFF, 0xFF,	/* pkt_type set as unknown */
			 0xFF, 0xFF,	/*pkt_type set as unknown */
			 /* second descriptor */
			 0xFF, 0xFF,
			 0xFF, 0xFF,	/* rss hash parsed separately */
			 11, 10,	/* octet 10~11, 16 bits vlan_macip */
			 5, 4,		/* octet 4~5, 16 bits data_len */
			 0xFF, 0xFF,	/* skip hi 16 bits pkt_len, zero out */
			 5, 4,		/* octet 4~5, 16 bits pkt_len */
			 0xFF, 0xFF,	/* pkt_type set as unknown */
			 0xFF, 0xFF	/*pkt_type set as unknown */
			);
	/**
	 * compile-time check the above crc and shuffle layout is correct.
	 * NOTE: the first field (lowest address) is given last in set_epi
	 * calls above.
	 */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, vlan_tci) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 10);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, hash) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 12);

	/* Status/Error flag masks */
	/**
	 * mask everything except Checksum Reports, RSS indication
	 * and VLAN indication.
	 * bit6:4 for IP/L4 checksum errors.
	 * bit12 is for RSS indication.
	 * bit13 is for VLAN indication.
	 */
	const __m256i flags_mask =
		 _mm256_set1_epi32((0xF << 4) | (1 << 12) | (1 << 13));
	/**
	 * data to be shuffled by the result of the flags mask shifted by 4
	 * bits.  This gives use the l3_l4 flags.
	 */
	const __m256i l3_l4_flags_shuf =
		_mm256_set_epi8((RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
		 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
		  RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_BAD  |
		 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_BAD  |
		 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
		 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
		 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_BAD |
		 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_BAD |
		 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
		 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
		 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		/**
		 * second 128-bits
		 * shift right 20 bits to use the low two bits to indicate
		 * outer checksum status
		 * shift right 1 bit to make sure it not exceed 255
		 */
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_BAD  |
		 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_BAD  |
		 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
		 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
		 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
		 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_BAD |
		 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_BAD |
		 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
		 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
		(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
		 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1);
	const __m256i cksum_mask =
		 _mm256_set1_epi32(RTE_MBUF_F_RX_IP_CKSUM_MASK |
				   RTE_MBUF_F_RX_L4_CKSUM_MASK |
				   RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
				   RTE_MBUF_F_RX_OUTER_L4_CKSUM_MASK);
	/**
	 * data to be shuffled by result of flag mask, shifted down 12.
	 * If RSS(bit12)/VLAN(bit13) are set,
	 * shuffle moves appropriate flags in place.
	 */
	const __m256i rss_vlan_flags_shuf = _mm256_set_epi8(0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			RTE_MBUF_F_RX_RSS_HASH | RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
			RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
			RTE_MBUF_F_RX_RSS_HASH, 0,
			/* end up 128-bits */
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			RTE_MBUF_F_RX_RSS_HASH | RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
			RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
			RTE_MBUF_F_RX_RSS_HASH, 0);

	RTE_SET_USED(avx_aligned); /* for 32B descriptors we don't use this */

	uint16_t i, received;

	for (i = 0, received = 0; i < nb_pkts;
	     i += IDPF_DESCS_PER_LOOP_AVX,
	     rxdp += IDPF_DESCS_PER_LOOP_AVX) {
		/* step 1, copy over 8 mbuf pointers to rx_pkts array */
		_mm256_storeu_si256((void *)&rx_pkts[i],
				    _mm256_loadu_si256((void *)&sw_ring[i]));
#ifdef RTE_ARCH_X86_64
		_mm256_storeu_si256
			((void *)&rx_pkts[i + 4],
			 _mm256_loadu_si256((void *)&sw_ring[i + 4]));
#endif

		__m256i raw_desc0_1, raw_desc2_3, raw_desc4_5, raw_desc6_7;

		const __m128i raw_desc7 = _mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 7));
		rte_compiler_barrier();
		const __m128i raw_desc6 = _mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 6));
		rte_compiler_barrier();
		const __m128i raw_desc5 = _mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 5));
		rte_compiler_barrier();
		const __m128i raw_desc4 = _mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 4));
		rte_compiler_barrier();
		const __m128i raw_desc3 = _mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 3));
		rte_compiler_barrier();
		const __m128i raw_desc2 = _mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 2));
		rte_compiler_barrier();
		const __m128i raw_desc1 = _mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 1));
		rte_compiler_barrier();
		const __m128i raw_desc0 = _mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 0));

		raw_desc6_7 = _mm256_inserti128_si256(_mm256_castsi128_si256(raw_desc6),
		raw_desc7, 1);
		raw_desc4_5 = _mm256_inserti128_si256(_mm256_castsi128_si256(raw_desc4),
		raw_desc5, 1);
		raw_desc2_3 = _mm256_inserti128_si256(_mm256_castsi128_si256(raw_desc2),
		raw_desc3, 1);
		raw_desc0_1 = _mm256_inserti128_si256(_mm256_castsi128_si256(raw_desc0),
		raw_desc1, 1);

		/**
		 * convert descriptors 4-7 into mbufs, re-arrange fields.
		 * Then write into the mbuf.
		 */
		__m256i mb6_7 = _mm256_shuffle_epi8(raw_desc6_7, shuf_msk);
		__m256i mb4_5 = _mm256_shuffle_epi8(raw_desc4_5, shuf_msk);

		/**
		 * to get packet types, ptype is located in bit16-25
		 * of each 128bits
		 */
		const __m256i ptype_mask = _mm256_set1_epi16(VIRTCHNL2_RX_FLEX_DESC_PTYPE_M);
		const __m256i ptypes6_7 = _mm256_and_si256(raw_desc6_7, ptype_mask);
		const __m256i ptypes4_5 = _mm256_and_si256(raw_desc4_5, ptype_mask);
		const uint16_t ptype7 = _mm256_extract_epi16(ptypes6_7, 9);
		const uint16_t ptype6 = _mm256_extract_epi16(ptypes6_7, 1);
		const uint16_t ptype5 = _mm256_extract_epi16(ptypes4_5, 9);
		const uint16_t ptype4 = _mm256_extract_epi16(ptypes4_5, 1);

		mb6_7 = _mm256_insert_epi32(mb6_7, ptype_tbl[ptype7], 4);
		mb6_7 = _mm256_insert_epi32(mb6_7, ptype_tbl[ptype6], 0);
		mb4_5 = _mm256_insert_epi32(mb4_5, ptype_tbl[ptype5], 4);
		mb4_5 = _mm256_insert_epi32(mb4_5, ptype_tbl[ptype4], 0);
		/* merge the status bits into one register */
		const __m256i status4_7 = _mm256_unpackhi_epi32(raw_desc6_7,
			raw_desc4_5);

		/**
		 * convert descriptors 0-3 into mbufs, re-arrange fields.
		 * Then write into the mbuf.
		 */
		__m256i mb2_3 = _mm256_shuffle_epi8(raw_desc2_3, shuf_msk);
		__m256i mb0_1 = _mm256_shuffle_epi8(raw_desc0_1, shuf_msk);

		/**
		 * to get packet types, ptype is located in bit16-25
		 * of each 128bits
		 */
		const __m256i ptypes2_3 = _mm256_and_si256(raw_desc2_3, ptype_mask);
		const __m256i ptypes0_1 = _mm256_and_si256(raw_desc0_1, ptype_mask);
		const uint16_t ptype3 = _mm256_extract_epi16(ptypes2_3, 9);
		const uint16_t ptype2 = _mm256_extract_epi16(ptypes2_3, 1);
		const uint16_t ptype1 = _mm256_extract_epi16(ptypes0_1, 9);
		const uint16_t ptype0 = _mm256_extract_epi16(ptypes0_1, 1);

		mb2_3 = _mm256_insert_epi32(mb2_3, ptype_tbl[ptype3], 4);
		mb2_3 = _mm256_insert_epi32(mb2_3, ptype_tbl[ptype2], 0);
		mb0_1 = _mm256_insert_epi32(mb0_1, ptype_tbl[ptype1], 4);
		mb0_1 = _mm256_insert_epi32(mb0_1, ptype_tbl[ptype0], 0);
		/* merge the status bits into one register */
		const __m256i status0_3 = _mm256_unpackhi_epi32(raw_desc2_3,
			raw_desc0_1);

		/**
		 * take the two sets of status bits and merge to one
		 * After merge, the packets status flags are in the
		 * order (hi->lo): [1, 3, 5, 7, 0, 2, 4, 6]
		 */
		__m256i status0_7 = _mm256_unpacklo_epi64(status4_7,
			status0_3);

		/* now do flag manipulation */

		/* get only flag/error bits we want */
		const __m256i flag_bits = _mm256_and_si256(status0_7, flags_mask);
		/**
		 * l3_l4_error flags, shuffle, then shift to correct adjustment
		 * of flags in flags_shuf, and finally mask out extra bits
		 */
		__m256i l3_l4_flags = _mm256_shuffle_epi8(l3_l4_flags_shuf,
			_mm256_srli_epi32(flag_bits, 4));
		l3_l4_flags = _mm256_slli_epi32(l3_l4_flags, 1);

		__m256i l4_outer_mask = _mm256_set1_epi32(0x6);
		__m256i l4_outer_flags = _mm256_and_si256(l3_l4_flags, l4_outer_mask);
		l4_outer_flags = _mm256_slli_epi32(l4_outer_flags, 20);

		__m256i l3_l4_mask = _mm256_set1_epi32(~0x6);
		l3_l4_flags = _mm256_and_si256(l3_l4_flags, l3_l4_mask);
		l3_l4_flags = _mm256_or_si256(l3_l4_flags, l4_outer_flags);
		l3_l4_flags = _mm256_and_si256(l3_l4_flags, cksum_mask);
		/* set rss and vlan flags */
		const __m256i rss_vlan_flag_bits = _mm256_srli_epi32(flag_bits, 12);
		const __m256i rss_vlan_flags = _mm256_shuffle_epi8(rss_vlan_flags_shuf,
			rss_vlan_flag_bits);

		/* merge flags */
		__m256i mbuf_flags = _mm256_or_si256(l3_l4_flags, rss_vlan_flags);

		/**
		 * At this point, we have the 8 sets of flags in the low 16-bits
		 * of each 32-bit value in vlan0.
		 * We want to extract these, and merge them with the mbuf init
		 * data so we can do a single write to the mbuf to set the flags
		 * and all the other initialization fields. Extracting the
		 * appropriate flags means that we have to do a shift and blend
		 * for each mbuf before we do the write. However, we can also
		 * add in the previously computed rx_descriptor fields to
		 * make a single 256-bit write per mbuf
		 */
		/* check the structure matches expectations */
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, ol_flags) != offsetof(struct rte_mbuf,
			rearm_data) + 8);
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, rearm_data) != RTE_ALIGN
			(offsetof(struct rte_mbuf, rearm_data), 16));
		/* build up data and do writes */
		__m256i rearm0, rearm1, rearm2, rearm3, rearm4, rearm5, rearm6, rearm7;
		rearm6 = _mm256_blend_epi32(mbuf_init, _mm256_slli_si256(mbuf_flags, 8), 0x04);
		rearm4 = _mm256_blend_epi32(mbuf_init, _mm256_slli_si256(mbuf_flags, 4), 0x04);
		rearm2 = _mm256_blend_epi32(mbuf_init, mbuf_flags, 0x04);
		rearm0 = _mm256_blend_epi32(mbuf_init, _mm256_srli_si256(mbuf_flags, 4), 0x04);
		/* permute to add in the rx_descriptor e.g. rss fields */
		rearm6 = _mm256_permute2f128_si256(rearm6, mb6_7, 0x20);
		rearm4 = _mm256_permute2f128_si256(rearm4, mb4_5, 0x20);
		rearm2 = _mm256_permute2f128_si256(rearm2, mb2_3, 0x20);
		rearm0 = _mm256_permute2f128_si256(rearm0, mb0_1, 0x20);
		/* write to mbuf */
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 6]->rearm_data, rearm6);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 4]->rearm_data, rearm4);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 2]->rearm_data, rearm2);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 0]->rearm_data, rearm0);

		/* repeat for the odd mbufs */
		const __m256i odd_flags = _mm256_castsi128_si256(_mm256_extracti128_si256
			(mbuf_flags, 1));
		rearm7 = _mm256_blend_epi32(mbuf_init, _mm256_slli_si256(odd_flags, 8), 0x04);
		rearm5 = _mm256_blend_epi32(mbuf_init, _mm256_slli_si256(odd_flags, 4), 0x04);
		rearm3 = _mm256_blend_epi32(mbuf_init, odd_flags, 0x04);
		rearm1 = _mm256_blend_epi32(mbuf_init, _mm256_srli_si256(odd_flags, 4), 0x04);
		/* since odd mbufs are already in hi 128-bits use blend */
		rearm7 = _mm256_blend_epi32(rearm7, mb6_7, 0xF0);
		rearm5 = _mm256_blend_epi32(rearm5, mb4_5, 0xF0);
		rearm3 = _mm256_blend_epi32(rearm3, mb2_3, 0xF0);
		rearm1 = _mm256_blend_epi32(rearm1, mb0_1, 0xF0);
		/* again write to mbufs */
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 7]->rearm_data, rearm7);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 5]->rearm_data, rearm5);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 3]->rearm_data, rearm3);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 1]->rearm_data, rearm1);

	    /* perform dd_check */
		status0_7 = _mm256_and_si256(status0_7, dd_check);
		status0_7 = _mm256_packs_epi32(status0_7, _mm256_setzero_si256());

		uint64_t burst = rte_popcount64(_mm_cvtsi128_si64(_mm256_extracti128_si256
			(status0_7, 1)));
		burst += rte_popcount64(_mm_cvtsi128_si64(_mm256_castsi256_si128
			(status0_7)));

		received += burst;
		if (burst != IDPF_DESCS_PER_LOOP_AVX)
			break;
	}

	/* update tail pointers */
	rxq->rx_tail += received;
	rxq->rx_tail &= (rxq->nb_rx_desc - 1);
	if ((rxq->rx_tail & 1) == 1 && received > 1) { /* keep avx2 aligned */
		rxq->rx_tail--;
		received--;
	}
	rxq->rxrearm_nb += received;
	return received;
}

/**
 * Notice:
 * - nb_pkts < IDPF_DESCS_PER_LOOP, just return no packet
 */
RTE_EXPORT_INTERNAL_SYMBOL(idpf_dp_singleq_recv_pkts_avx2)
uint16_t
idpf_dp_singleq_recv_pkts_avx2(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	return _idpf_singleq_recv_raw_pkts_vec_avx2(rx_queue, rx_pkts, nb_pkts);
}

static inline void
idpf_singleq_vtx1(volatile struct idpf_base_tx_desc *txdp,
		  struct rte_mbuf *pkt, uint64_t flags)
{
	uint64_t high_qw =
		(IDPF_TX_DESC_DTYPE_DATA |
		 ((uint64_t)flags  << IDPF_TXD_QW1_CMD_S) |
		 ((uint64_t)pkt->data_len << IDPF_TXD_QW1_TX_BUF_SZ_S));

	__m128i descriptor = _mm_set_epi64x(high_qw,
				pkt->buf_iova + pkt->data_off);
	_mm_store_si128(RTE_CAST_PTR(__m128i *, txdp), descriptor);
}

static inline void
idpf_singleq_vtx(volatile struct idpf_base_tx_desc *txdp,
		 struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags)
{
	const uint64_t hi_qw_tmpl = (IDPF_TX_DESC_DTYPE_DATA |
			((uint64_t)flags  << IDPF_TXD_QW1_CMD_S));

	/* if unaligned on 32-bit boundary, do one to align */
	if (((uintptr_t)txdp & 0x1F) != 0 && nb_pkts != 0) {
		idpf_singleq_vtx1(txdp, *pkt, flags);
		nb_pkts--, txdp++, pkt++;
	}

	/* do two at a time while possible, in bursts */
	for (; nb_pkts > 3; txdp += 4, pkt += 4, nb_pkts -= 4) {
		uint64_t hi_qw3 =
			hi_qw_tmpl |
			((uint64_t)pkt[3]->data_len <<
			 IDPF_TXD_QW1_TX_BUF_SZ_S);
		uint64_t hi_qw2 =
			hi_qw_tmpl |
			((uint64_t)pkt[2]->data_len <<
			 IDPF_TXD_QW1_TX_BUF_SZ_S);
		uint64_t hi_qw1 =
			hi_qw_tmpl |
			((uint64_t)pkt[1]->data_len <<
			 IDPF_TXD_QW1_TX_BUF_SZ_S);
		uint64_t hi_qw0 =
			hi_qw_tmpl |
			((uint64_t)pkt[0]->data_len <<
			 IDPF_TXD_QW1_TX_BUF_SZ_S);

		__m256i desc2_3 =
			_mm256_set_epi64x
				(hi_qw3,
				 pkt[3]->buf_iova + pkt[3]->data_off,
				 hi_qw2,
				 pkt[2]->buf_iova + pkt[2]->data_off);
		__m256i desc0_1 =
			_mm256_set_epi64x
				(hi_qw1,
				 pkt[1]->buf_iova + pkt[1]->data_off,
				 hi_qw0,
				 pkt[0]->buf_iova + pkt[0]->data_off);
		_mm256_store_si256(RTE_CAST_PTR(__m256i *, txdp + 2), desc2_3);
		_mm256_store_si256(RTE_CAST_PTR(__m256i *, txdp), desc0_1);
	}

	/* do any last ones */
	while (nb_pkts) {
		idpf_singleq_vtx1(txdp, *pkt, flags);
		txdp++, pkt++, nb_pkts--;
	}
}

static inline uint16_t
idpf_singleq_xmit_fixed_burst_vec_avx2(void *tx_queue, struct rte_mbuf **tx_pkts,
				       uint16_t nb_pkts)
{
	struct ci_tx_queue *txq = (struct ci_tx_queue *)tx_queue;
	volatile struct idpf_base_tx_desc *txdp;
	struct ci_tx_entry_vec *txep;
	uint16_t n, nb_commit, tx_id;
	uint64_t flags = IDPF_TX_DESC_CMD_EOP;
	uint64_t rs = IDPF_TX_DESC_CMD_RS | flags;

	/* cross rx_thresh boundary is not allowed */
	nb_pkts = RTE_MIN(nb_pkts, txq->tx_rs_thresh);

	if (txq->nb_tx_free < txq->tx_free_thresh)
		ci_tx_free_bufs_vec(txq, idpf_tx_desc_done, false);

	nb_commit = nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->idpf_tx_ring[tx_id];
	txep = &txq->sw_ring_vec[tx_id];

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {
		ci_tx_backlog_entry_vec(txep, tx_pkts, n);

		idpf_singleq_vtx(txdp, tx_pkts, n - 1, flags);
		tx_pkts += (n - 1);
		txdp += (n - 1);

		idpf_singleq_vtx1(txdp, *tx_pkts++, rs);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &txq->idpf_tx_ring[tx_id];
		txep = &txq->sw_ring_vec[tx_id];
	}

	ci_tx_backlog_entry_vec(txep, tx_pkts, nb_commit);

	idpf_singleq_vtx(txdp, tx_pkts, nb_commit, flags);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->tx_next_rs) {
		txq->idpf_tx_ring[txq->tx_next_rs].qw1 |=
			rte_cpu_to_le_64(((uint64_t)IDPF_TX_DESC_CMD_RS) <<
					 IDPF_TXD_QW1_CMD_S);
		txq->tx_next_rs =
			(uint16_t)(txq->tx_next_rs + txq->tx_rs_thresh);
	}

	txq->tx_tail = tx_id;

	IDPF_PCI_REG_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

RTE_EXPORT_INTERNAL_SYMBOL(idpf_dp_singleq_xmit_pkts_avx2)
uint16_t
idpf_dp_singleq_xmit_pkts_avx2(void *tx_queue, struct rte_mbuf **tx_pkts,
			       uint16_t nb_pkts)
{
	uint16_t nb_tx = 0;
	struct ci_tx_queue *txq = (struct ci_tx_queue *)tx_queue;

	while (nb_pkts) {
		uint16_t ret, num;

		num = (uint16_t)RTE_MIN(nb_pkts, txq->tx_rs_thresh);
		ret = idpf_singleq_xmit_fixed_burst_vec_avx2(tx_queue, &tx_pkts[nb_tx],
						    num);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}
