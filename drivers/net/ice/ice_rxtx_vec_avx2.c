/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include "ice_rxtx_vec_common.h"
#include "ice_rxtx_common_avx.h"

#include <rte_vect.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static __rte_always_inline void
ice_rxq_rearm(struct ice_rx_queue *rxq)
{
	return ice_rxq_rearm_common(rxq, false);
}

static __rte_always_inline __m256i
ice_flex_rxd_to_fdir_flags_vec_avx2(const __m256i fdir_id0_7)
{
#define FDID_MIS_MAGIC 0xFFFFFFFF
	RTE_BUILD_BUG_ON(RTE_MBUF_F_RX_FDIR != (1 << 2));
	RTE_BUILD_BUG_ON(RTE_MBUF_F_RX_FDIR_ID != (1 << 13));
	const __m256i pkt_fdir_bit = _mm256_set1_epi32(RTE_MBUF_F_RX_FDIR |
			RTE_MBUF_F_RX_FDIR_ID);
	/* desc->flow_id field == 0xFFFFFFFF means fdir mismatch */
	const __m256i fdir_mis_mask = _mm256_set1_epi32(FDID_MIS_MAGIC);
	__m256i fdir_mask = _mm256_cmpeq_epi32(fdir_id0_7,
			fdir_mis_mask);
	/* this XOR op results to bit-reverse the fdir_mask */
	fdir_mask = _mm256_xor_si256(fdir_mask, fdir_mis_mask);
	const __m256i fdir_flags = _mm256_and_si256(fdir_mask, pkt_fdir_bit);

	return fdir_flags;
}

static __rte_always_inline uint16_t
_ice_recv_raw_pkts_vec_avx2(struct ice_rx_queue *rxq, struct rte_mbuf **rx_pkts,
			    uint16_t nb_pkts, uint8_t *split_packet,
			    bool offload)
{
#define ICE_DESCS_PER_LOOP_AVX 8

	const uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;
	const __m256i mbuf_init = _mm256_set_epi64x(0, 0,
			0, rxq->mbuf_initializer);
	struct ice_rx_entry *sw_ring = &rxq->sw_ring[rxq->rx_tail];
	volatile union ice_rx_flex_desc *rxdp = rxq->rx_ring + rxq->rx_tail;
	const int avx_aligned = ((rxq->rx_tail & 1) == 0);

	rte_prefetch0(rxdp);

	/* nb_pkts has to be floor-aligned to ICE_DESCS_PER_LOOP_AVX */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, ICE_DESCS_PER_LOOP_AVX);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > ICE_RXQ_REARM_THRESH)
		ice_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->wb.status_error0 &
			rte_cpu_to_le_32(1 << ICE_RX_FLEX_DESC_STATUS0_DD_S)))
		return 0;

	/* constants used in processing loop */
	const __m256i crc_adjust =
		_mm256_set_epi16
			(/* first descriptor */
			 0, 0, 0,       /* ignore non-length fields */
			 -rxq->crc_len, /* sub crc on data_len */
			 0,             /* ignore high-16bits of pkt_len */
			 -rxq->crc_len, /* sub crc on pkt_len */
			 0, 0,          /* ignore pkt_type field */
			 /* second descriptor */
			 0, 0, 0,       /* ignore non-length fields */
			 -rxq->crc_len, /* sub crc on data_len */
			 0,             /* ignore high-16bits of pkt_len */
			 -rxq->crc_len, /* sub crc on pkt_len */
			 0, 0           /* ignore pkt_type field */
			);

	/* 8 packets DD mask, LSB in each 32-bit value */
	const __m256i dd_check = _mm256_set1_epi32(1);

	/* 8 packets EOP mask, second-LSB in each 32-bit value */
	const __m256i eop_check = _mm256_slli_epi32(dd_check,
			ICE_RX_DESC_STATUS_EOF_S);

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
	     i += ICE_DESCS_PER_LOOP_AVX,
	     rxdp += ICE_DESCS_PER_LOOP_AVX) {
		/* step 1, copy over 8 mbuf pointers to rx_pkts array */
		_mm256_storeu_si256((void *)&rx_pkts[i],
				    _mm256_loadu_si256((void *)&sw_ring[i]));
#ifdef RTE_ARCH_X86_64
		_mm256_storeu_si256
			((void *)&rx_pkts[i + 4],
			 _mm256_loadu_si256((void *)&sw_ring[i + 4]));
#endif

		__m256i raw_desc0_1, raw_desc2_3, raw_desc4_5, raw_desc6_7;
#ifdef RTE_LIBRTE_ICE_16BYTE_RX_DESC
		/* for AVX we need alignment otherwise loads are not atomic */
		if (avx_aligned) {
			/* load in descriptors, 2 at a time, in reverse order */
			raw_desc6_7 = _mm256_load_si256((void *)(rxdp + 6));
			rte_compiler_barrier();
			raw_desc4_5 = _mm256_load_si256((void *)(rxdp + 4));
			rte_compiler_barrier();
			raw_desc2_3 = _mm256_load_si256((void *)(rxdp + 2));
			rte_compiler_barrier();
			raw_desc0_1 = _mm256_load_si256((void *)(rxdp + 0));
		} else
#endif
		{
			const __m128i raw_desc7 =
				_mm_load_si128((void *)(rxdp + 7));
			rte_compiler_barrier();
			const __m128i raw_desc6 =
				_mm_load_si128((void *)(rxdp + 6));
			rte_compiler_barrier();
			const __m128i raw_desc5 =
				_mm_load_si128((void *)(rxdp + 5));
			rte_compiler_barrier();
			const __m128i raw_desc4 =
				_mm_load_si128((void *)(rxdp + 4));
			rte_compiler_barrier();
			const __m128i raw_desc3 =
				_mm_load_si128((void *)(rxdp + 3));
			rte_compiler_barrier();
			const __m128i raw_desc2 =
				_mm_load_si128((void *)(rxdp + 2));
			rte_compiler_barrier();
			const __m128i raw_desc1 =
				_mm_load_si128((void *)(rxdp + 1));
			rte_compiler_barrier();
			const __m128i raw_desc0 =
				_mm_load_si128((void *)(rxdp + 0));

			raw_desc6_7 =
				_mm256_inserti128_si256
					(_mm256_castsi128_si256(raw_desc6),
					 raw_desc7, 1);
			raw_desc4_5 =
				_mm256_inserti128_si256
					(_mm256_castsi128_si256(raw_desc4),
					 raw_desc5, 1);
			raw_desc2_3 =
				_mm256_inserti128_si256
					(_mm256_castsi128_si256(raw_desc2),
					 raw_desc3, 1);
			raw_desc0_1 =
				_mm256_inserti128_si256
					(_mm256_castsi128_si256(raw_desc0),
					 raw_desc1, 1);
		}

		if (split_packet) {
			int j;

			for (j = 0; j < ICE_DESCS_PER_LOOP_AVX; j++)
				rte_mbuf_prefetch_part2(rx_pkts[i + j]);
		}

		/**
		 * convert descriptors 4-7 into mbufs, re-arrange fields.
		 * Then write into the mbuf.
		 */
		__m256i mb6_7 = _mm256_shuffle_epi8(raw_desc6_7, shuf_msk);
		__m256i mb4_5 = _mm256_shuffle_epi8(raw_desc4_5, shuf_msk);

		mb6_7 = _mm256_add_epi16(mb6_7, crc_adjust);
		mb4_5 = _mm256_add_epi16(mb4_5, crc_adjust);
		/**
		 * to get packet types, ptype is located in bit16-25
		 * of each 128bits
		 */
		const __m256i ptype_mask =
			_mm256_set1_epi16(ICE_RX_FLEX_DESC_PTYPE_M);
		const __m256i ptypes6_7 =
			_mm256_and_si256(raw_desc6_7, ptype_mask);
		const __m256i ptypes4_5 =
			_mm256_and_si256(raw_desc4_5, ptype_mask);
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

		mb2_3 = _mm256_add_epi16(mb2_3, crc_adjust);
		mb0_1 = _mm256_add_epi16(mb0_1, crc_adjust);
		/**
		 * to get packet types, ptype is located in bit16-25
		 * of each 128bits
		 */
		const __m256i ptypes2_3 =
			_mm256_and_si256(raw_desc2_3, ptype_mask);
		const __m256i ptypes0_1 =
			_mm256_and_si256(raw_desc0_1, ptype_mask);
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
		__m256i mbuf_flags = _mm256_set1_epi32(0);

		if (offload) {
			/* now do flag manipulation */

			/* get only flag/error bits we want */
			const __m256i flag_bits =
				_mm256_and_si256(status0_7, flags_mask);
			/**
			 * l3_l4_error flags, shuffle, then shift to correct adjustment
			 * of flags in flags_shuf, and finally mask out extra bits
			 */
			__m256i l3_l4_flags = _mm256_shuffle_epi8(l3_l4_flags_shuf,
					_mm256_srli_epi32(flag_bits, 4));
			l3_l4_flags = _mm256_slli_epi32(l3_l4_flags, 1);

			__m256i l4_outer_mask = _mm256_set1_epi32(0x6);
			__m256i l4_outer_flags =
					_mm256_and_si256(l3_l4_flags, l4_outer_mask);
			l4_outer_flags = _mm256_slli_epi32(l4_outer_flags, 20);

			__m256i l3_l4_mask = _mm256_set1_epi32(~0x6);

			l3_l4_flags = _mm256_and_si256(l3_l4_flags, l3_l4_mask);
			l3_l4_flags = _mm256_or_si256(l3_l4_flags, l4_outer_flags);
			l3_l4_flags = _mm256_and_si256(l3_l4_flags, cksum_mask);
			/* set rss and vlan flags */
			const __m256i rss_vlan_flag_bits =
				_mm256_srli_epi32(flag_bits, 12);
			const __m256i rss_vlan_flags =
				_mm256_shuffle_epi8(rss_vlan_flags_shuf,
						    rss_vlan_flag_bits);

			/* merge flags */
			mbuf_flags = _mm256_or_si256(l3_l4_flags,
						     rss_vlan_flags);
		}

		if (rxq->fdir_enabled) {
			const __m256i fdir_id4_7 =
				_mm256_unpackhi_epi32(raw_desc6_7, raw_desc4_5);

			const __m256i fdir_id0_3 =
				_mm256_unpackhi_epi32(raw_desc2_3, raw_desc0_1);

			const __m256i fdir_id0_7 =
				_mm256_unpackhi_epi64(fdir_id4_7, fdir_id0_3);

			const __m256i fdir_flags =
				ice_flex_rxd_to_fdir_flags_vec_avx2(fdir_id0_7);

			/* merge with fdir_flags */
			mbuf_flags = _mm256_or_si256(mbuf_flags, fdir_flags);

			/* write to mbuf: have to use scalar store here */
			rx_pkts[i + 0]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_id0_7, 3);

			rx_pkts[i + 1]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_id0_7, 7);

			rx_pkts[i + 2]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_id0_7, 2);

			rx_pkts[i + 3]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_id0_7, 6);

			rx_pkts[i + 4]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_id0_7, 1);

			rx_pkts[i + 5]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_id0_7, 5);

			rx_pkts[i + 6]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_id0_7, 0);

			rx_pkts[i + 7]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_id0_7, 4);
		} /* if() on fdir_enabled */

		if (offload) {
#ifndef RTE_LIBRTE_ICE_16BYTE_RX_DESC
			/**
			 * needs to load 2nd 16B of each desc for RSS hash parsing,
			 * will cause performance drop to get into this context.
			 */
			if (rxq->vsi->adapter->pf.dev_data->dev_conf.rxmode.offloads &
					RTE_ETH_RX_OFFLOAD_RSS_HASH) {
				/* load bottom half of every 32B desc */
				const __m128i raw_desc_bh7 =
					_mm_load_si128
						((void *)(&rxdp[7].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh6 =
					_mm_load_si128
						((void *)(&rxdp[6].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh5 =
					_mm_load_si128
						((void *)(&rxdp[5].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh4 =
					_mm_load_si128
						((void *)(&rxdp[4].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh3 =
					_mm_load_si128
						((void *)(&rxdp[3].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh2 =
					_mm_load_si128
						((void *)(&rxdp[2].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh1 =
					_mm_load_si128
						((void *)(&rxdp[1].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh0 =
					_mm_load_si128
						((void *)(&rxdp[0].wb.status_error1));

				__m256i raw_desc_bh6_7 =
					_mm256_inserti128_si256
						(_mm256_castsi128_si256(raw_desc_bh6),
						raw_desc_bh7, 1);
				__m256i raw_desc_bh4_5 =
					_mm256_inserti128_si256
						(_mm256_castsi128_si256(raw_desc_bh4),
						raw_desc_bh5, 1);
				__m256i raw_desc_bh2_3 =
					_mm256_inserti128_si256
						(_mm256_castsi128_si256(raw_desc_bh2),
						raw_desc_bh3, 1);
				__m256i raw_desc_bh0_1 =
					_mm256_inserti128_si256
						(_mm256_castsi128_si256(raw_desc_bh0),
						raw_desc_bh1, 1);

				/**
				 * to shift the 32b RSS hash value to the
				 * highest 32b of each 128b before mask
				 */
				__m256i rss_hash6_7 =
					_mm256_slli_epi64(raw_desc_bh6_7, 32);
				__m256i rss_hash4_5 =
					_mm256_slli_epi64(raw_desc_bh4_5, 32);
				__m256i rss_hash2_3 =
					_mm256_slli_epi64(raw_desc_bh2_3, 32);
				__m256i rss_hash0_1 =
					_mm256_slli_epi64(raw_desc_bh0_1, 32);

				__m256i rss_hash_msk =
					_mm256_set_epi32(0xFFFFFFFF, 0, 0, 0,
							 0xFFFFFFFF, 0, 0, 0);

				rss_hash6_7 = _mm256_and_si256
						(rss_hash6_7, rss_hash_msk);
				rss_hash4_5 = _mm256_and_si256
						(rss_hash4_5, rss_hash_msk);
				rss_hash2_3 = _mm256_and_si256
						(rss_hash2_3, rss_hash_msk);
				rss_hash0_1 = _mm256_and_si256
						(rss_hash0_1, rss_hash_msk);

				mb6_7 = _mm256_or_si256(mb6_7, rss_hash6_7);
				mb4_5 = _mm256_or_si256(mb4_5, rss_hash4_5);
				mb2_3 = _mm256_or_si256(mb2_3, rss_hash2_3);
				mb0_1 = _mm256_or_si256(mb0_1, rss_hash0_1);
			} /* if() on RSS hash parsing */
#endif
		}

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
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, ol_flags) !=
				 offsetof(struct rte_mbuf, rearm_data) + 8);
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, rearm_data) !=
				 RTE_ALIGN(offsetof(struct rte_mbuf,
						    rearm_data),
					   16));
		/* build up data and do writes */
		__m256i rearm0, rearm1, rearm2, rearm3, rearm4, rearm5,
			rearm6, rearm7;
		rearm6 = _mm256_blend_epi32(mbuf_init,
					    _mm256_slli_si256(mbuf_flags, 8),
					    0x04);
		rearm4 = _mm256_blend_epi32(mbuf_init,
					    _mm256_slli_si256(mbuf_flags, 4),
					    0x04);
		rearm2 = _mm256_blend_epi32(mbuf_init, mbuf_flags, 0x04);
		rearm0 = _mm256_blend_epi32(mbuf_init,
					    _mm256_srli_si256(mbuf_flags, 4),
					    0x04);
		/* permute to add in the rx_descriptor e.g. rss fields */
		rearm6 = _mm256_permute2f128_si256(rearm6, mb6_7, 0x20);
		rearm4 = _mm256_permute2f128_si256(rearm4, mb4_5, 0x20);
		rearm2 = _mm256_permute2f128_si256(rearm2, mb2_3, 0x20);
		rearm0 = _mm256_permute2f128_si256(rearm0, mb0_1, 0x20);
		/* write to mbuf */
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 6]->rearm_data,
				    rearm6);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 4]->rearm_data,
				    rearm4);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 2]->rearm_data,
				    rearm2);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 0]->rearm_data,
				    rearm0);

		/* repeat for the odd mbufs */
		const __m256i odd_flags =
			_mm256_castsi128_si256
				(_mm256_extracti128_si256(mbuf_flags, 1));
		rearm7 = _mm256_blend_epi32(mbuf_init,
					    _mm256_slli_si256(odd_flags, 8),
					    0x04);
		rearm5 = _mm256_blend_epi32(mbuf_init,
					    _mm256_slli_si256(odd_flags, 4),
					    0x04);
		rearm3 = _mm256_blend_epi32(mbuf_init, odd_flags, 0x04);
		rearm1 = _mm256_blend_epi32(mbuf_init,
					    _mm256_srli_si256(odd_flags, 4),
					    0x04);
		/* since odd mbufs are already in hi 128-bits use blend */
		rearm7 = _mm256_blend_epi32(rearm7, mb6_7, 0xF0);
		rearm5 = _mm256_blend_epi32(rearm5, mb4_5, 0xF0);
		rearm3 = _mm256_blend_epi32(rearm3, mb2_3, 0xF0);
		rearm1 = _mm256_blend_epi32(rearm1, mb0_1, 0xF0);
		/* again write to mbufs */
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 7]->rearm_data,
				    rearm7);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 5]->rearm_data,
				    rearm5);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 3]->rearm_data,
				    rearm3);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 1]->rearm_data,
				    rearm1);

		/* extract and record EOP bit */
		if (split_packet) {
			const __m128i eop_mask =
				_mm_set1_epi16(1 << ICE_RX_DESC_STATUS_EOF_S);
			const __m256i eop_bits256 = _mm256_and_si256(status0_7,
								     eop_check);
			/* pack status bits into a single 128-bit register */
			const __m128i eop_bits =
				_mm_packus_epi32
					(_mm256_castsi256_si128(eop_bits256),
					 _mm256_extractf128_si256(eop_bits256,
								  1));
			/**
			 * flip bits, and mask out the EOP bit, which is now
			 * a split-packet bit i.e. !EOP, rather than EOP one.
			 */
			__m128i split_bits = _mm_andnot_si128(eop_bits,
					eop_mask);
			/**
			 * eop bits are out of order, so we need to shuffle them
			 * back into order again. In doing so, only use low 8
			 * bits, which acts like another pack instruction
			 * The original order is (hi->lo): 1,3,5,7,0,2,4,6
			 * [Since we use epi8, the 16-bit positions are
			 * multiplied by 2 in the eop_shuffle value.]
			 */
			__m128i eop_shuffle =
				_mm_set_epi8(/* zero hi 64b */
					     0xFF, 0xFF, 0xFF, 0xFF,
					     0xFF, 0xFF, 0xFF, 0xFF,
					     /* move values to lo 64b */
					     8, 0, 10, 2,
					     12, 4, 14, 6);
			split_bits = _mm_shuffle_epi8(split_bits, eop_shuffle);
			*(uint64_t *)split_packet =
				_mm_cvtsi128_si64(split_bits);
			split_packet += ICE_DESCS_PER_LOOP_AVX;
		}

		/* perform dd_check */
		status0_7 = _mm256_and_si256(status0_7, dd_check);
		status0_7 = _mm256_packs_epi32(status0_7,
					       _mm256_setzero_si256());

		uint64_t burst = rte_popcount64
					(_mm_cvtsi128_si64
						(_mm256_extracti128_si256
							(status0_7, 1)));
		burst += rte_popcount64
				(_mm_cvtsi128_si64
					(_mm256_castsi256_si128(status0_7)));
		received += burst;
		if (burst != ICE_DESCS_PER_LOOP_AVX)
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
 * - nb_pkts < ICE_DESCS_PER_LOOP, just return no packet
 */
uint16_t
ice_recv_pkts_vec_avx2(void *rx_queue, struct rte_mbuf **rx_pkts,
		       uint16_t nb_pkts)
{
	return _ice_recv_raw_pkts_vec_avx2(rx_queue, rx_pkts,
					   nb_pkts, NULL, false);
}

uint16_t
ice_recv_pkts_vec_avx2_offload(void *rx_queue, struct rte_mbuf **rx_pkts,
			       uint16_t nb_pkts)
{
	return _ice_recv_raw_pkts_vec_avx2(rx_queue, rx_pkts,
					   nb_pkts, NULL, true);
}

/**
 * vPMD receive routine that reassembles single burst of 32 scattered packets
 * Notice:
 * - nb_pkts < ICE_DESCS_PER_LOOP, just return no packet
 */
static __rte_always_inline uint16_t
ice_recv_scattered_burst_vec_avx2(void *rx_queue, struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts, bool offload)
{
	struct ice_rx_queue *rxq = rx_queue;
	uint8_t split_flags[ICE_VPMD_RX_BURST] = {0};

	/* get some new buffers */
	uint16_t nb_bufs = _ice_recv_raw_pkts_vec_avx2(rxq, rx_pkts, nb_pkts,
						       split_flags, offload);
	if (nb_bufs == 0)
		return 0;

	/* happy day case, full burst + no packets to be joined */
	const uint64_t *split_fl64 = (uint64_t *)split_flags;

	if (!rxq->pkt_first_seg &&
	    split_fl64[0] == 0 && split_fl64[1] == 0 &&
	    split_fl64[2] == 0 && split_fl64[3] == 0)
		return nb_bufs;

	/* reassemble any packets that need reassembly*/
	unsigned int i = 0;

	if (!rxq->pkt_first_seg) {
		/* find the first split flag, and only reassemble then*/
		while (i < nb_bufs && !split_flags[i])
			i++;
		if (i == nb_bufs)
			return nb_bufs;
		rxq->pkt_first_seg = rx_pkts[i];
	}
	return i + ice_rx_reassemble_packets(rxq, &rx_pkts[i], nb_bufs - i,
					     &split_flags[i]);
}

/**
 * vPMD receive routine that reassembles scattered packets.
 * Main receive routine that can handle arbitrary burst sizes
 * Notice:
 * - nb_pkts < ICE_DESCS_PER_LOOP, just return no packet
 */
static __rte_always_inline uint16_t
ice_recv_scattered_pkts_vec_avx2_common(void *rx_queue,
					struct rte_mbuf **rx_pkts,
					uint16_t nb_pkts,
					bool offload)
{
	uint16_t retval = 0;

	while (nb_pkts > ICE_VPMD_RX_BURST) {
		uint16_t burst = ice_recv_scattered_burst_vec_avx2(rx_queue,
				rx_pkts + retval, ICE_VPMD_RX_BURST, offload);
		retval += burst;
		nb_pkts -= burst;
		if (burst < ICE_VPMD_RX_BURST)
			return retval;
	}
	return retval + ice_recv_scattered_burst_vec_avx2(rx_queue,
				rx_pkts + retval, nb_pkts, offload);
}

uint16_t
ice_recv_scattered_pkts_vec_avx2(void *rx_queue,
				 struct rte_mbuf **rx_pkts,
				 uint16_t nb_pkts)
{
	return ice_recv_scattered_pkts_vec_avx2_common(rx_queue,
						       rx_pkts,
						       nb_pkts,
						       false);
}

uint16_t
ice_recv_scattered_pkts_vec_avx2_offload(void *rx_queue,
					 struct rte_mbuf **rx_pkts,
					 uint16_t nb_pkts)
{
	return ice_recv_scattered_pkts_vec_avx2_common(rx_queue,
						       rx_pkts,
						       nb_pkts,
						       true);
}

static __rte_always_inline void
ice_vtx1(volatile struct ice_tx_desc *txdp,
	 struct rte_mbuf *pkt, uint64_t flags, bool offload)
{
	uint64_t high_qw =
		(ICE_TX_DESC_DTYPE_DATA |
		 ((uint64_t)flags  << ICE_TXD_QW1_CMD_S) |
		 ((uint64_t)pkt->data_len << ICE_TXD_QW1_TX_BUF_SZ_S));
	if (offload)
		ice_txd_enable_offload(pkt, &high_qw);

	__m128i descriptor = _mm_set_epi64x(high_qw, rte_pktmbuf_iova(pkt));
	_mm_store_si128((__m128i *)txdp, descriptor);
}

static __rte_always_inline void
ice_vtx(volatile struct ice_tx_desc *txdp,
	struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags, bool offload)
{
	const uint64_t hi_qw_tmpl = (ICE_TX_DESC_DTYPE_DATA |
			((uint64_t)flags  << ICE_TXD_QW1_CMD_S));

	/* if unaligned on 32-bit boundary, do one to align */
	if (((uintptr_t)txdp & 0x1F) != 0 && nb_pkts != 0) {
		ice_vtx1(txdp, *pkt, flags, offload);
		nb_pkts--, txdp++, pkt++;
	}

	/* do two at a time while possible, in bursts */
	for (; nb_pkts > 3; txdp += 4, pkt += 4, nb_pkts -= 4) {
		uint64_t hi_qw3 =
			hi_qw_tmpl |
			((uint64_t)pkt[3]->data_len <<
			 ICE_TXD_QW1_TX_BUF_SZ_S);
		if (offload)
			ice_txd_enable_offload(pkt[3], &hi_qw3);
		uint64_t hi_qw2 =
			hi_qw_tmpl |
			((uint64_t)pkt[2]->data_len <<
			 ICE_TXD_QW1_TX_BUF_SZ_S);
		if (offload)
			ice_txd_enable_offload(pkt[2], &hi_qw2);
		uint64_t hi_qw1 =
			hi_qw_tmpl |
			((uint64_t)pkt[1]->data_len <<
			 ICE_TXD_QW1_TX_BUF_SZ_S);
		if (offload)
			ice_txd_enable_offload(pkt[1], &hi_qw1);
		uint64_t hi_qw0 =
			hi_qw_tmpl |
			((uint64_t)pkt[0]->data_len <<
			 ICE_TXD_QW1_TX_BUF_SZ_S);
		if (offload)
			ice_txd_enable_offload(pkt[0], &hi_qw0);

		__m256i desc2_3 =
			_mm256_set_epi64x
				(hi_qw3, rte_pktmbuf_iova(pkt[3]),
				 hi_qw2, rte_pktmbuf_iova(pkt[2]));
		__m256i desc0_1 =
			_mm256_set_epi64x
				(hi_qw1, rte_pktmbuf_iova(pkt[1]),
				 hi_qw0, rte_pktmbuf_iova(pkt[0]));
		_mm256_store_si256((void *)(txdp + 2), desc2_3);
		_mm256_store_si256((void *)txdp, desc0_1);
	}

	/* do any last ones */
	while (nb_pkts) {
		ice_vtx1(txdp, *pkt, flags, offload);
		txdp++, pkt++, nb_pkts--;
	}
}

static __rte_always_inline uint16_t
ice_xmit_fixed_burst_vec_avx2(void *tx_queue, struct rte_mbuf **tx_pkts,
			      uint16_t nb_pkts, bool offload)
{
	struct ice_tx_queue *txq = (struct ice_tx_queue *)tx_queue;
	volatile struct ice_tx_desc *txdp;
	struct ice_tx_entry *txep;
	uint16_t n, nb_commit, tx_id;
	uint64_t flags = ICE_TD_CMD;
	uint64_t rs = ICE_TX_DESC_CMD_RS | ICE_TD_CMD;

	/* cross rx_thresh boundary is not allowed */
	nb_pkts = RTE_MIN(nb_pkts, txq->tx_rs_thresh);

	if (txq->nb_tx_free < txq->tx_free_thresh)
		ice_tx_free_bufs_vec(txq);

	nb_commit = nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = &txq->sw_ring[tx_id];

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {
		ice_tx_backlog_entry(txep, tx_pkts, n);

		ice_vtx(txdp, tx_pkts, n - 1, flags, offload);
		tx_pkts += (n - 1);
		txdp += (n - 1);

		ice_vtx1(txdp, *tx_pkts++, rs, offload);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &txq->tx_ring[tx_id];
		txep = &txq->sw_ring[tx_id];
	}

	ice_tx_backlog_entry(txep, tx_pkts, nb_commit);

	ice_vtx(txdp, tx_pkts, nb_commit, flags, offload);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->tx_next_rs) {
		txq->tx_ring[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)ICE_TX_DESC_CMD_RS) <<
					 ICE_TXD_QW1_CMD_S);
		txq->tx_next_rs =
			(uint16_t)(txq->tx_next_rs + txq->tx_rs_thresh);
	}

	txq->tx_tail = tx_id;

	ICE_PCI_REG_WC_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

static __rte_always_inline uint16_t
ice_xmit_pkts_vec_avx2_common(void *tx_queue, struct rte_mbuf **tx_pkts,
			      uint16_t nb_pkts, bool offload)
{
	uint16_t nb_tx = 0;
	struct ice_tx_queue *txq = (struct ice_tx_queue *)tx_queue;

	while (nb_pkts) {
		uint16_t ret, num;

		num = (uint16_t)RTE_MIN(nb_pkts, txq->tx_rs_thresh);
		ret = ice_xmit_fixed_burst_vec_avx2(tx_queue, &tx_pkts[nb_tx],
						    num, offload);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}

uint16_t
ice_xmit_pkts_vec_avx2(void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts)
{
	return ice_xmit_pkts_vec_avx2_common(tx_queue, tx_pkts, nb_pkts, false);
}

uint16_t
ice_xmit_pkts_vec_avx2_offload(void *tx_queue, struct rte_mbuf **tx_pkts,
			       uint16_t nb_pkts)
{
	return ice_xmit_pkts_vec_avx2_common(tx_queue, tx_pkts, nb_pkts, true);
}
