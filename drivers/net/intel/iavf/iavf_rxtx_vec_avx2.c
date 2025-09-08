/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include "iavf_rxtx_vec_common.h"
#include "../common/rx_vec_x86.h"

#include <rte_vect.h>

static __rte_always_inline void
iavf_rxq_rearm(struct ci_rx_queue *rxq)
{
	ci_rxq_rearm(rxq, CI_RX_VEC_LEVEL_AVX2);
}

#define PKTLEN_SHIFT     10

static __rte_always_inline uint16_t
_iavf_recv_raw_pkts_vec_avx2(struct ci_rx_queue *rxq,
			     struct rte_mbuf **rx_pkts,
			     uint16_t nb_pkts, uint8_t *split_packet,
			     bool offload)
{
	const uint32_t *type_table = rxq->iavf_vsi->adapter->ptype_tbl;
	const __m256i mbuf_init = _mm256_set_epi64x(0, 0,
			0, rxq->mbuf_initializer);
	struct ci_rx_entry *sw_ring = &rxq->sw_ring[rxq->rx_tail];
	volatile union ci_rx_desc *rxdp = &rxq->rx_ring[rxq->rx_tail];
	const int avx_aligned = ((rxq->rx_tail & 1) == 0);

	rte_prefetch0(rxdp);

	/* nb_pkts has to be floor-aligned to IAVF_VPMD_DESCS_PER_LOOP_WIDE */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, IAVF_VPMD_DESCS_PER_LOOP_WIDE);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > IAVF_VPMD_RXQ_REARM_THRESH)
		iavf_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->wb.qword1.status_error_len &
			rte_cpu_to_le_32(1 << IAVF_RX_DESC_STATUS_DD_SHIFT)))
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
			IAVF_RX_DESC_STATUS_EOF_SHIFT);

	/* mask to shuffle from desc. to mbuf (2 descriptors)*/
	const __m256i shuf_msk =
		_mm256_set_epi8
			(/* first descriptor */
			 7, 6, 5, 4,  /* octet 4~7, 32bits rss */
			 3, 2,        /* octet 2~3, low 16 bits vlan_macip */
			 15, 14,      /* octet 15~14, 16 bits data_len */
			 0xFF, 0xFF,  /* skip high 16 bits pkt_len, zero out */
			 15, 14,      /* octet 15~14, low 16 bits pkt_len */
			 0xFF, 0xFF,  /* pkt_type set as unknown */
			 0xFF, 0xFF,  /*pkt_type set as unknown */
			 /* second descriptor */
			 7, 6, 5, 4,  /* octet 4~7, 32bits rss */
			 3, 2,        /* octet 2~3, low 16 bits vlan_macip */
			 15, 14,      /* octet 15~14, 16 bits data_len */
			 0xFF, 0xFF,  /* skip high 16 bits pkt_len, zero out */
			 15, 14,      /* octet 15~14, low 16 bits pkt_len */
			 0xFF, 0xFF,  /* pkt_type set as unknown */
			 0xFF, 0xFF   /*pkt_type set as unknown */
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
	 * mask everything except RSS, flow director and VLAN flags
	 * bit2 is for VLAN tag, bit11 for flow director indication
	 * bit13:12 for RSS indication. Bits 3-5 of error
	 * field (bits 22-24) are for IP/L4 checksum errors
	 */
	const __m256i flags_mask =
		 _mm256_set1_epi32((1 << 2) | (1 << 11) |
				   (3 << 12) | (7 << 22));
	/**
	 * data to be shuffled by result of flag mask. If VLAN bit is set,
	 * (bit 2), then position 4 in this array will be used in the
	 * destination
	 */
	const __m256i vlan_flags_shuf =
		_mm256_set_epi32(0, 0, RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED, 0,
				 0, 0, RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED, 0);
	/**
	 * data to be shuffled by result of flag mask, shifted down 11.
	 * If RSS/FDIR bits are set, shuffle moves appropriate flags in
	 * place.
	 */
	const __m256i rss_flags_shuf =
		_mm256_set_epi8(0, 0, 0, 0, 0, 0, 0, 0,
				RTE_MBUF_F_RX_RSS_HASH | RTE_MBUF_F_RX_FDIR, RTE_MBUF_F_RX_RSS_HASH,
				0, 0, 0, 0, RTE_MBUF_F_RX_FDIR, 0,/* end up 128-bits */
				0, 0, 0, 0, 0, 0, 0, 0,
				RTE_MBUF_F_RX_RSS_HASH | RTE_MBUF_F_RX_FDIR, RTE_MBUF_F_RX_RSS_HASH,
				0, 0, 0, 0, RTE_MBUF_F_RX_FDIR, 0);

	/**
	 * data to be shuffled by the result of the flags mask shifted by 22
	 * bits.  This gives use the l3_l4 flags.
	 */
	const __m256i l3_l4_flags_shuf = _mm256_set_epi8(0, 0, 0, 0, 0, 0, 0, 0,
			/* shift right 1 bit to make sure it not exceed 255 */
			(RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
			 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
			 RTE_MBUF_F_RX_L4_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD) >> 1,
			RTE_MBUF_F_RX_IP_CKSUM_BAD >> 1,
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1,
			/* second 128-bits */
			0, 0, 0, 0, 0, 0, 0, 0,
			(RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
			 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
			 RTE_MBUF_F_RX_L4_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD) >> 1,
			RTE_MBUF_F_RX_IP_CKSUM_BAD >> 1,
			(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1);

	const __m256i cksum_mask =
		 _mm256_set1_epi32(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD |
				   RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				   RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD);

	RTE_SET_USED(avx_aligned); /* for 32B descriptors we don't use this */

	uint16_t i, received;

	for (i = 0, received = 0; i < nb_pkts;
	     i += IAVF_VPMD_DESCS_PER_LOOP_WIDE,
	     rxdp += IAVF_VPMD_DESCS_PER_LOOP_WIDE) {
		/* step 1, copy over 8 mbuf pointers to rx_pkts array */
		_mm256_storeu_si256((void *)&rx_pkts[i],
				    _mm256_loadu_si256((void *)&sw_ring[i]));
#ifdef RTE_ARCH_X86_64
		_mm256_storeu_si256
			((void *)&rx_pkts[i + 4],
			 _mm256_loadu_si256((void *)&sw_ring[i + 4]));
#endif

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

		const __m256i raw_desc6_7 =
			_mm256_inserti128_si256(_mm256_castsi128_si256(raw_desc6), raw_desc7, 1);
		const __m256i raw_desc4_5 =
			_mm256_inserti128_si256(_mm256_castsi128_si256(raw_desc4), raw_desc5, 1);
		const __m256i raw_desc2_3 =
			_mm256_inserti128_si256(_mm256_castsi128_si256(raw_desc2), raw_desc3, 1);
		const __m256i raw_desc0_1 =
			_mm256_inserti128_si256(_mm256_castsi128_si256(raw_desc0), raw_desc1, 1);

		if (split_packet) {
			int j;

			for (j = 0; j < IAVF_VPMD_DESCS_PER_LOOP_WIDE; j++)
				rte_mbuf_prefetch_part2(rx_pkts[i + j]);
		}

		/**
		 * convert descriptors 4-7 into mbufs, adjusting length and
		 * re-arranging fields. Then write into the mbuf
		 */
		const __m256i len6_7 = _mm256_slli_epi32(raw_desc6_7,
							 PKTLEN_SHIFT);
		const __m256i len4_5 = _mm256_slli_epi32(raw_desc4_5,
							 PKTLEN_SHIFT);
		const __m256i desc6_7 = _mm256_blend_epi16(raw_desc6_7,
							   len6_7, 0x80);
		const __m256i desc4_5 = _mm256_blend_epi16(raw_desc4_5,
							   len4_5, 0x80);
		__m256i mb6_7 = _mm256_shuffle_epi8(desc6_7, shuf_msk);
		__m256i mb4_5 = _mm256_shuffle_epi8(desc4_5, shuf_msk);

		mb6_7 = _mm256_add_epi16(mb6_7, crc_adjust);
		mb4_5 = _mm256_add_epi16(mb4_5, crc_adjust);
		/**
		 * to get packet types, shift 64-bit values down 30 bits
		 * and so ptype is in lower 8-bits in each
		 */
		const __m256i ptypes6_7 = _mm256_srli_epi64(desc6_7, 30);
		const __m256i ptypes4_5 = _mm256_srli_epi64(desc4_5, 30);
		const uint8_t ptype7 = _mm256_extract_epi8(ptypes6_7, 24);
		const uint8_t ptype6 = _mm256_extract_epi8(ptypes6_7, 8);
		const uint8_t ptype5 = _mm256_extract_epi8(ptypes4_5, 24);
		const uint8_t ptype4 = _mm256_extract_epi8(ptypes4_5, 8);

		mb6_7 = _mm256_insert_epi32(mb6_7, type_table[ptype7], 4);
		mb6_7 = _mm256_insert_epi32(mb6_7, type_table[ptype6], 0);
		mb4_5 = _mm256_insert_epi32(mb4_5, type_table[ptype5], 4);
		mb4_5 = _mm256_insert_epi32(mb4_5, type_table[ptype4], 0);
		/* merge the status bits into one register */
		const __m256i status4_7 = _mm256_unpackhi_epi32(desc6_7,
				desc4_5);

		/**
		 * convert descriptors 0-3 into mbufs, adjusting length and
		 * re-arranging fields. Then write into the mbuf
		 */
		const __m256i len2_3 = _mm256_slli_epi32(raw_desc2_3,
							 PKTLEN_SHIFT);
		const __m256i len0_1 = _mm256_slli_epi32(raw_desc0_1,
							 PKTLEN_SHIFT);
		const __m256i desc2_3 = _mm256_blend_epi16(raw_desc2_3,
							   len2_3, 0x80);
		const __m256i desc0_1 = _mm256_blend_epi16(raw_desc0_1,
							   len0_1, 0x80);
		__m256i mb2_3 = _mm256_shuffle_epi8(desc2_3, shuf_msk);
		__m256i mb0_1 = _mm256_shuffle_epi8(desc0_1, shuf_msk);

		mb2_3 = _mm256_add_epi16(mb2_3, crc_adjust);
		mb0_1 = _mm256_add_epi16(mb0_1, crc_adjust);
		/* get the packet types */
		const __m256i ptypes2_3 = _mm256_srli_epi64(desc2_3, 30);
		const __m256i ptypes0_1 = _mm256_srli_epi64(desc0_1, 30);
		const uint8_t ptype3 = _mm256_extract_epi8(ptypes2_3, 24);
		const uint8_t ptype2 = _mm256_extract_epi8(ptypes2_3, 8);
		const uint8_t ptype1 = _mm256_extract_epi8(ptypes0_1, 24);
		const uint8_t ptype0 = _mm256_extract_epi8(ptypes0_1, 8);

		mb2_3 = _mm256_insert_epi32(mb2_3, type_table[ptype3], 4);
		mb2_3 = _mm256_insert_epi32(mb2_3, type_table[ptype2], 0);
		mb0_1 = _mm256_insert_epi32(mb0_1, type_table[ptype1], 4);
		mb0_1 = _mm256_insert_epi32(mb0_1, type_table[ptype0], 0);
		/* merge the status bits into one register */
		const __m256i status0_3 = _mm256_unpackhi_epi32(desc2_3,
								desc0_1);

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
			/* set vlan and rss flags */
			const __m256i vlan_flags =
				_mm256_shuffle_epi8(vlan_flags_shuf, flag_bits);
			const __m256i rss_flags =
				_mm256_shuffle_epi8(rss_flags_shuf,
						    _mm256_srli_epi32(flag_bits, 11));
			/**
			 * l3_l4_error flags, shuffle, then shift to correct adjustment
			 * of flags in flags_shuf, and finally mask out extra bits
			 */
			__m256i l3_l4_flags = _mm256_shuffle_epi8(l3_l4_flags_shuf,
					_mm256_srli_epi32(flag_bits, 22));
			l3_l4_flags = _mm256_slli_epi32(l3_l4_flags, 1);
			l3_l4_flags = _mm256_and_si256(l3_l4_flags, cksum_mask);

			/* merge flags */
			mbuf_flags = _mm256_or_si256(l3_l4_flags,
						     _mm256_or_si256(rss_flags, vlan_flags));
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
				_mm_set1_epi16(1 << IAVF_RX_DESC_STATUS_EOF_SHIFT);
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
			split_packet += IAVF_VPMD_DESCS_PER_LOOP_WIDE;
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
		if (burst != IAVF_VPMD_DESCS_PER_LOOP_WIDE)
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

static inline __m256i
flex_rxd_to_fdir_flags_vec_avx2(const __m256i fdir_id0_7)
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
_iavf_recv_raw_pkts_vec_avx2_flex_rxd(struct ci_rx_queue *rxq,
				      struct rte_mbuf **rx_pkts,
				      uint16_t nb_pkts, uint8_t *split_packet,
				      bool offload)
{
	struct iavf_adapter *adapter = rxq->iavf_vsi->adapter;
	uint64_t offloads = adapter->dev_data->dev_conf.rxmode.offloads;
	const uint32_t *type_table = adapter->ptype_tbl;

	const __m256i mbuf_init = _mm256_set_epi64x(0, 0,
			0, rxq->mbuf_initializer);
	struct ci_rx_entry *sw_ring = &rxq->sw_ring[rxq->rx_tail];
	volatile union ci_rx_flex_desc *rxdp = rxq->rx_flex_ring + rxq->rx_tail;

	rte_prefetch0(rxdp);

	/* nb_pkts has to be floor-aligned to IAVF_VPMD_DESCS_PER_LOOP_WIDE */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, IAVF_VPMD_DESCS_PER_LOOP_WIDE);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > IAVF_VPMD_RXQ_REARM_THRESH)
		iavf_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->wb.status_error0 &
			rte_cpu_to_le_32(1 << IAVF_RX_FLEX_DESC_STATUS0_DD_S)))
		return 0;
	bool is_tsinit = false;
	uint8_t inflection_point = 0;
	__m256i hw_low_last = _mm256_set_epi32(0, 0, 0, 0, 0, 0, 0, rxq->phc_time);
	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
		uint64_t sw_cur_time = rte_get_timer_cycles() / (rte_get_timer_hz() / 1000);

		if (unlikely(sw_cur_time - rxq->hw_time_update > 4)) {
			hw_low_last = _mm256_setzero_si256();
			is_tsinit = 1;
		} else {
			hw_low_last = _mm256_set_epi32(0, 0, 0, 0, 0, 0, 0, rxq->phc_time);
		}
	}

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
			IAVF_RX_FLEX_DESC_STATUS0_EOF_S);

	/* mask to shuffle from desc. to mbuf (2 descriptors)*/
	const __m256i shuf_msk =
		_mm256_set_epi8
			(/* first descriptor */
			 0xFF, 0xFF,
			 0xFF, 0xFF,    /* rss hash parsed separately */
			 11, 10,	/* octet 10~11, 16 bits vlan_macip */
			 5, 4,		/* octet 4~5, 16 bits data_len */
			 0xFF, 0xFF,	/* skip hi 16 bits pkt_len, zero out */
			 5, 4,		/* octet 4~5, 16 bits pkt_len */
			 0xFF, 0xFF,	/* pkt_type set as unknown */
			 0xFF, 0xFF,	/*pkt_type set as unknown */
			 /* second descriptor */
			 0xFF, 0xFF,
			 0xFF, 0xFF,    /* rss hash parsed separately */
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
	const __m256i rss_flags_shuf = _mm256_set_epi8(0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			RTE_MBUF_F_RX_RSS_HASH, 0,
			RTE_MBUF_F_RX_RSS_HASH, 0,
			/* end up 128-bits */
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			RTE_MBUF_F_RX_RSS_HASH, 0,
			RTE_MBUF_F_RX_RSS_HASH, 0);

	const __m256i vlan_flags_shuf = _mm256_set_epi8(0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
			RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
			0, 0,
			/* end up 128-bits */
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0,
			RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
			RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
			0, 0);

	uint16_t i, received;

	for (i = 0, received = 0; i < nb_pkts;
	     i += IAVF_VPMD_DESCS_PER_LOOP_WIDE,
	     rxdp += IAVF_VPMD_DESCS_PER_LOOP_WIDE) {
		/* step 1, copy over 8 mbuf pointers to rx_pkts array */
		_mm256_storeu_si256((void *)&rx_pkts[i],
				    _mm256_loadu_si256((void *)&sw_ring[i]));
#ifdef RTE_ARCH_X86_64
		_mm256_storeu_si256
			((void *)&rx_pkts[i + 4],
			 _mm256_loadu_si256((void *)&sw_ring[i + 4]));
#endif

		__m256i raw_desc0_1, raw_desc2_3, raw_desc4_5, raw_desc6_7;

		const __m128i raw_desc7 =
			_mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 7));
		rte_compiler_barrier();
		const __m128i raw_desc6 =
			_mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 6));
		rte_compiler_barrier();
		const __m128i raw_desc5 =
			_mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 5));
		rte_compiler_barrier();
		const __m128i raw_desc4 =
			_mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 4));
		rte_compiler_barrier();
		const __m128i raw_desc3 =
			_mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 3));
		rte_compiler_barrier();
		const __m128i raw_desc2 =
			_mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 2));
		rte_compiler_barrier();
		const __m128i raw_desc1 =
			_mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 1));
		rte_compiler_barrier();
		const __m128i raw_desc0 =
			_mm_load_si128(RTE_CAST_PTR(const __m128i *, rxdp + 0));

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

		if (split_packet) {
			int j;

			for (j = 0; j < IAVF_VPMD_DESCS_PER_LOOP_WIDE; j++)
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
			_mm256_set1_epi16(IAVF_RX_FLEX_DESC_PTYPE_M);
		const __m256i ptypes6_7 =
			_mm256_and_si256(raw_desc6_7, ptype_mask);
		const __m256i ptypes4_5 =
			_mm256_and_si256(raw_desc4_5, ptype_mask);
		const uint16_t ptype7 = _mm256_extract_epi16(ptypes6_7, 9);
		const uint16_t ptype6 = _mm256_extract_epi16(ptypes6_7, 1);
		const uint16_t ptype5 = _mm256_extract_epi16(ptypes4_5, 9);
		const uint16_t ptype4 = _mm256_extract_epi16(ptypes4_5, 1);

		mb6_7 = _mm256_insert_epi32(mb6_7, type_table[ptype7], 4);
		mb6_7 = _mm256_insert_epi32(mb6_7, type_table[ptype6], 0);
		mb4_5 = _mm256_insert_epi32(mb4_5, type_table[ptype5], 4);
		mb4_5 = _mm256_insert_epi32(mb4_5, type_table[ptype4], 0);
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

		mb2_3 = _mm256_insert_epi32(mb2_3, type_table[ptype3], 4);
		mb2_3 = _mm256_insert_epi32(mb2_3, type_table[ptype2], 0);
		mb0_1 = _mm256_insert_epi32(mb0_1, type_table[ptype1], 4);
		mb0_1 = _mm256_insert_epi32(mb0_1, type_table[ptype0], 0);
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
		__m256i vlan_flags = _mm256_setzero_si256();

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
			const __m256i rss_flags =
				_mm256_shuffle_epi8(rss_flags_shuf,
						    rss_vlan_flag_bits);

			if (rxq->rx_flags == IAVF_RX_FLAGS_VLAN_TAG_LOC_L2TAG1)
				vlan_flags =
					_mm256_shuffle_epi8(vlan_flags_shuf,
							    rss_vlan_flag_bits);

			const __m256i rss_vlan_flags =
				_mm256_or_si256(rss_flags, vlan_flags);

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
				flex_rxd_to_fdir_flags_vec_avx2(fdir_id0_7);

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
			/**
			 * needs to load 2nd 16B of each desc,
			 * will cause performance drop to get into this context.
			 */
			if (offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH ||
				offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP ||
			    rxq->rx_flags & IAVF_RX_FLAGS_VLAN_TAG_LOC_L2TAG2_2) {
				/* load bottom half of every 32B desc */
				const __m128i raw_desc_bh7 =
					_mm_load_si128
					(RTE_CAST_PTR(const __m128i *, &rxdp[7].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh6 =
					_mm_load_si128
					(RTE_CAST_PTR(const __m128i *, &rxdp[6].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh5 =
					_mm_load_si128
					(RTE_CAST_PTR(const __m128i *, &rxdp[5].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh4 =
					_mm_load_si128
					(RTE_CAST_PTR(const __m128i *, &rxdp[4].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh3 =
					_mm_load_si128
					(RTE_CAST_PTR(const __m128i *, &rxdp[3].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh2 =
					_mm_load_si128
					(RTE_CAST_PTR(const __m128i *, &rxdp[2].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh1 =
					_mm_load_si128
					(RTE_CAST_PTR(const __m128i *, &rxdp[1].wb.status_error1));
				rte_compiler_barrier();
				const __m128i raw_desc_bh0 =
					_mm_load_si128
					(RTE_CAST_PTR(const __m128i *, &rxdp[0].wb.status_error1));

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

				if (offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH) {
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

					const __m256i rss_hash_msk =
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

				if (rxq->rx_flags & IAVF_RX_FLAGS_VLAN_TAG_LOC_L2TAG2_2) {
					/* merge the status/error-1 bits into one register */
					const __m256i status1_4_7 =
						_mm256_unpacklo_epi32(raw_desc_bh6_7,
								      raw_desc_bh4_5);
					const __m256i status1_0_3 =
						_mm256_unpacklo_epi32(raw_desc_bh2_3,
								      raw_desc_bh0_1);

					const __m256i status1_0_7 =
						_mm256_unpacklo_epi64(status1_4_7,
								      status1_0_3);

					const __m256i l2tag2p_flag_mask =
						_mm256_set1_epi32
						(1 << IAVF_RX_FLEX_DESC_STATUS1_L2TAG2P_S);

					__m256i l2tag2p_flag_bits =
						_mm256_and_si256
						(status1_0_7, l2tag2p_flag_mask);

					l2tag2p_flag_bits =
						_mm256_srli_epi32(l2tag2p_flag_bits,
							IAVF_RX_FLEX_DESC_STATUS1_L2TAG2P_S);

					const __m256i l2tag2_flags_shuf =
						_mm256_set_epi8(0, 0, 0, 0,
								0, 0, 0, 0,
								0, 0, 0, 0,
								0, 0,
								RTE_MBUF_F_RX_VLAN |
								RTE_MBUF_F_RX_VLAN_STRIPPED,
								0,
								/* end up 128-bits */
								0, 0, 0, 0,
								0, 0, 0, 0,
								0, 0, 0, 0,
								0, 0,
								RTE_MBUF_F_RX_VLAN |
								RTE_MBUF_F_RX_VLAN_STRIPPED,
								0);

					vlan_flags =
						_mm256_shuffle_epi8(l2tag2_flags_shuf,
								    l2tag2p_flag_bits);

					/* merge with vlan_flags */
					mbuf_flags = _mm256_or_si256
							(mbuf_flags, vlan_flags);

					/* L2TAG2_2 */
					__m256i vlan_tci6_7 =
						_mm256_slli_si256(raw_desc_bh6_7, 4);
					__m256i vlan_tci4_5 =
						_mm256_slli_si256(raw_desc_bh4_5, 4);
					__m256i vlan_tci2_3 =
						_mm256_slli_si256(raw_desc_bh2_3, 4);
					__m256i vlan_tci0_1 =
						_mm256_slli_si256(raw_desc_bh0_1, 4);

					const __m256i vlan_tci_msk =
						_mm256_set_epi32(0, 0xFFFF0000, 0, 0,
								 0, 0xFFFF0000, 0, 0);

					vlan_tci6_7 = _mm256_and_si256
							(vlan_tci6_7, vlan_tci_msk);
					vlan_tci4_5 = _mm256_and_si256
							(vlan_tci4_5, vlan_tci_msk);
					vlan_tci2_3 = _mm256_and_si256
							(vlan_tci2_3, vlan_tci_msk);
					vlan_tci0_1 = _mm256_and_si256
							(vlan_tci0_1, vlan_tci_msk);

					mb6_7 = _mm256_or_si256(mb6_7, vlan_tci6_7);
					mb4_5 = _mm256_or_si256(mb4_5, vlan_tci4_5);
					mb2_3 = _mm256_or_si256(mb2_3, vlan_tci2_3);
					mb0_1 = _mm256_or_si256(mb0_1, vlan_tci0_1);
				} /* if() on Vlan parsing */

				if (offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
					uint32_t mask = 0xFFFFFFFF;
					__m256i ts;
					__m256i ts_low = _mm256_setzero_si256();
					__m256i ts_low1;
					__m256i ts_low2;
					__m256i max_ret;
					__m256i cmp_ret;
					uint8_t ret = 0;
					uint8_t shift = 8;
					__m256i ts_desp_mask = _mm256_set_epi32(mask, 0, 0, 0, mask, 0, 0, 0);
					__m256i cmp_mask = _mm256_set1_epi32(mask);
					__m256i ts_permute_mask = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);

					ts = _mm256_and_si256(raw_desc_bh0_1, ts_desp_mask);
					ts_low = _mm256_or_si256(ts_low, _mm256_srli_si256(ts, 3 * 4));
					ts = _mm256_and_si256(raw_desc_bh2_3, ts_desp_mask);
					ts_low = _mm256_or_si256(ts_low, _mm256_srli_si256(ts, 2 * 4));
					ts = _mm256_and_si256(raw_desc_bh4_5, ts_desp_mask);
					ts_low = _mm256_or_si256(ts_low, _mm256_srli_si256(ts, 4));
					ts = _mm256_and_si256(raw_desc_bh6_7, ts_desp_mask);
					ts_low = _mm256_or_si256(ts_low, ts);

					ts_low1 = _mm256_permutevar8x32_epi32(ts_low, ts_permute_mask);
					ts_low2 = _mm256_permutevar8x32_epi32(ts_low1,
								_mm256_set_epi32(6, 5, 4, 3, 2, 1, 0, 7));
					ts_low2 = _mm256_and_si256(ts_low2,
								_mm256_set_epi32(mask, mask, mask, mask, mask, mask, mask, 0));
					ts_low2 = _mm256_or_si256(ts_low2, hw_low_last);
					hw_low_last = _mm256_and_si256(ts_low1,
								_mm256_set_epi32(0, 0, 0, 0, 0, 0, 0, mask));

					*RTE_MBUF_DYNFIELD(rx_pkts[i + 0],
						iavf_timestamp_dynfield_offset, uint32_t *) = _mm256_extract_epi32(ts_low1, 0);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 1],
						iavf_timestamp_dynfield_offset, uint32_t *) = _mm256_extract_epi32(ts_low1, 1);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 2],
						iavf_timestamp_dynfield_offset, uint32_t *) = _mm256_extract_epi32(ts_low1, 2);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 3],
						iavf_timestamp_dynfield_offset, uint32_t *) = _mm256_extract_epi32(ts_low1, 3);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 4],
						iavf_timestamp_dynfield_offset, uint32_t *) = _mm256_extract_epi32(ts_low1, 4);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 5],
						iavf_timestamp_dynfield_offset, uint32_t *) = _mm256_extract_epi32(ts_low1, 5);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 6],
						iavf_timestamp_dynfield_offset, uint32_t *) = _mm256_extract_epi32(ts_low1, 6);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 7],
						iavf_timestamp_dynfield_offset, uint32_t *) = _mm256_extract_epi32(ts_low1, 7);

					if (unlikely(is_tsinit)) {
						uint32_t in_timestamp;
						if (iavf_get_phc_time(rxq))
							PMD_DRV_LOG(ERR, "get physical time failed");
						in_timestamp = *RTE_MBUF_DYNFIELD(rx_pkts[i + 0],
								iavf_timestamp_dynfield_offset, uint32_t *);
						rxq->phc_time = iavf_tstamp_convert_32b_64b(rxq->phc_time, in_timestamp);
					}

					*RTE_MBUF_DYNFIELD(rx_pkts[i + 0],
						iavf_timestamp_dynfield_offset + 4, uint32_t *) = (uint32_t)(rxq->phc_time >> 32);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 1],
						iavf_timestamp_dynfield_offset + 4, uint32_t *) = (uint32_t)(rxq->phc_time >> 32);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 2],
						iavf_timestamp_dynfield_offset + 4, uint32_t *) = (uint32_t)(rxq->phc_time >> 32);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 3],
						iavf_timestamp_dynfield_offset + 4, uint32_t *) = (uint32_t)(rxq->phc_time >> 32);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 4],
						iavf_timestamp_dynfield_offset + 4, uint32_t *) = (uint32_t)(rxq->phc_time >> 32);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 5],
						iavf_timestamp_dynfield_offset + 4, uint32_t *) = (uint32_t)(rxq->phc_time >> 32);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 6],
						iavf_timestamp_dynfield_offset + 4, uint32_t *) = (uint32_t)(rxq->phc_time >> 32);
					*RTE_MBUF_DYNFIELD(rx_pkts[i + 7],
						iavf_timestamp_dynfield_offset + 4, uint32_t *) = (uint32_t)(rxq->phc_time >> 32);

					max_ret = _mm256_max_epu32(ts_low2, ts_low1);
					cmp_ret = _mm256_andnot_si256(_mm256_cmpeq_epi32(max_ret, ts_low1), cmp_mask);

					if (_mm256_testz_si256(cmp_ret, cmp_mask)) {
						inflection_point = 0;
					} else {
						inflection_point = 1;
						while (shift > 1) {
							shift = shift >> 1;
							__m256i mask_low = _mm256_setzero_si256();
							__m256i mask_high = _mm256_setzero_si256();
							switch (shift) {
							case 4:
								mask_low = _mm256_set_epi32(0, 0, 0, 0, mask, mask, mask, mask);
								mask_high = _mm256_set_epi32(mask, mask, mask, mask, 0, 0, 0, 0);
								break;
							case 2:
								mask_low = _mm256_srli_si256(cmp_mask, 2 * 4);
								mask_high = _mm256_slli_si256(cmp_mask, 2 * 4);
								break;
							case 1:
								mask_low = _mm256_srli_si256(cmp_mask, 1 * 4);
								mask_high = _mm256_slli_si256(cmp_mask, 1 * 4);
								break;
							}
							ret = _mm256_testz_si256(cmp_ret, mask_low);
							if (ret) {
								ret = _mm256_testz_si256(cmp_ret, mask_high);
								inflection_point += ret ? 0 : shift;
								cmp_mask = mask_high;
							} else {
								cmp_mask = mask_low;
							}
						}
					}
					mbuf_flags = _mm256_or_si256(mbuf_flags, _mm256_set1_epi32(iavf_timestamp_dynflag));
				} /* if() on Timestamp parsing */
			}
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
				_mm_set1_epi16(1 <<
					       IAVF_RX_FLEX_DESC_STATUS0_EOF_S);
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
			split_packet += IAVF_VPMD_DESCS_PER_LOOP_WIDE;
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
		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
			inflection_point = (inflection_point <= burst) ? inflection_point : 0;
			switch (inflection_point) {
			case 1:
				*RTE_MBUF_DYNFIELD(rx_pkts[i + 0],
					iavf_timestamp_dynfield_offset + 4, uint32_t *) += 1;
				/* fallthrough */
			case 2:
				*RTE_MBUF_DYNFIELD(rx_pkts[i + 1],
					iavf_timestamp_dynfield_offset + 4, uint32_t *) += 1;
				/* fallthrough */
			case 3:
				*RTE_MBUF_DYNFIELD(rx_pkts[i + 2],
					iavf_timestamp_dynfield_offset + 4, uint32_t *) += 1;
				/* fallthrough */
			case 4:
				*RTE_MBUF_DYNFIELD(rx_pkts[i + 3],
					iavf_timestamp_dynfield_offset + 4, uint32_t *) += 1;
				/* fallthrough */
			case 5:
				*RTE_MBUF_DYNFIELD(rx_pkts[i + 4],
					iavf_timestamp_dynfield_offset + 4, uint32_t *) += 1;
				/* fallthrough */
			case 6:
				*RTE_MBUF_DYNFIELD(rx_pkts[i + 5],
					iavf_timestamp_dynfield_offset + 4, uint32_t *) += 1;
				/* fallthrough */
			case 7:
				*RTE_MBUF_DYNFIELD(rx_pkts[i + 6],
					iavf_timestamp_dynfield_offset + 4, uint32_t *) += 1;
				/* fallthrough */
			case 8:
				*RTE_MBUF_DYNFIELD(rx_pkts[i + 7],
					iavf_timestamp_dynfield_offset + 4, uint32_t *) += 1;
				rxq->phc_time += (uint64_t)1 << 32;
				/* fallthrough */
			case 0:
				break;
			default:
				PMD_DRV_LOG(ERR, "invalid inflection point for rx timestamp");
				break;
			}

			rxq->hw_time_update = rte_get_timer_cycles() / (rte_get_timer_hz() / 1000);
		}
		if (burst != IAVF_VPMD_DESCS_PER_LOOP_WIDE)
			break;
	}

	if (received > 0 && (rxq->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP))
		rxq->phc_time = *RTE_MBUF_DYNFIELD(rx_pkts[received - 1], iavf_timestamp_dynfield_offset, rte_mbuf_timestamp_t *);

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
 * - nb_pkts < IAVF_DESCS_PER_LOOP, just return no packet
 */
uint16_t
iavf_recv_pkts_vec_avx2(void *rx_queue, struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	return _iavf_recv_raw_pkts_vec_avx2(rx_queue, rx_pkts, nb_pkts,
					    NULL, false);
}

uint16_t
iavf_recv_pkts_vec_avx2_offload(void *rx_queue, struct rte_mbuf **rx_pkts,
				uint16_t nb_pkts)
{
	return _iavf_recv_raw_pkts_vec_avx2(rx_queue, rx_pkts, nb_pkts,
					    NULL, true);
}

/**
 * Notice:
 * - nb_pkts < IAVF_DESCS_PER_LOOP, just return no packet
 */
uint16_t
iavf_recv_pkts_vec_avx2_flex_rxd(void *rx_queue, struct rte_mbuf **rx_pkts,
				 uint16_t nb_pkts)
{
	return _iavf_recv_raw_pkts_vec_avx2_flex_rxd(rx_queue, rx_pkts,
						     nb_pkts, NULL, false);
}

uint16_t
iavf_recv_pkts_vec_avx2_flex_rxd_offload(void *rx_queue, struct rte_mbuf **rx_pkts,
					 uint16_t nb_pkts)
{
	return _iavf_recv_raw_pkts_vec_avx2_flex_rxd(rx_queue, rx_pkts,
						     nb_pkts, NULL, true);
}

/**
 * vPMD receive routine that reassembles single burst of 32 scattered packets
 * Notice:
 * - nb_pkts < IAVF_DESCS_PER_LOOP, just return no packet
 */
static __rte_always_inline uint16_t
iavf_recv_scattered_burst_vec_avx2(void *rx_queue, struct rte_mbuf **rx_pkts,
				   uint16_t nb_pkts, bool offload)
{
	struct ci_rx_queue *rxq = rx_queue;
	uint8_t split_flags[IAVF_VPMD_RX_BURST] = {0};

	/* get some new buffers */
	uint16_t nb_bufs = _iavf_recv_raw_pkts_vec_avx2(rxq, rx_pkts, nb_pkts,
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
	return i + ci_rx_reassemble_packets(&rx_pkts[i], nb_bufs - i, &split_flags[i],
			&rxq->pkt_first_seg, &rxq->pkt_last_seg, rxq->crc_len);
}

/**
 * vPMD receive routine that reassembles scattered packets.
 * Main receive routine that can handle arbitrary burst sizes
 * Notice:
 * - nb_pkts < IAVF_DESCS_PER_LOOP, just return no packet
 */
static __rte_always_inline uint16_t
iavf_recv_scattered_pkts_vec_avx2_common(void *rx_queue, struct rte_mbuf **rx_pkts,
					 uint16_t nb_pkts, bool offload)
{
	uint16_t retval = 0;

	while (nb_pkts > IAVF_VPMD_RX_BURST) {
		uint16_t burst = iavf_recv_scattered_burst_vec_avx2(rx_queue,
				rx_pkts + retval, IAVF_VPMD_RX_BURST, offload);
		retval += burst;
		nb_pkts -= burst;
		if (burst < IAVF_VPMD_RX_BURST)
			return retval;
	}
	return retval + iavf_recv_scattered_burst_vec_avx2(rx_queue,
				rx_pkts + retval, nb_pkts, offload);
}

uint16_t
iavf_recv_scattered_pkts_vec_avx2(void *rx_queue,
				  struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts)
{
	return iavf_recv_scattered_pkts_vec_avx2_common(rx_queue,
							rx_pkts,
							nb_pkts,
							false);
}

uint16_t
iavf_recv_scattered_pkts_vec_avx2_offload(void *rx_queue,
					  struct rte_mbuf **rx_pkts,
					  uint16_t nb_pkts)
{
	return iavf_recv_scattered_pkts_vec_avx2_common(rx_queue,
							rx_pkts,
							nb_pkts,
							true);
}

/**
 * vPMD receive routine that reassembles single burst of
 * 32 scattered packets for flex RxD
 * Notice:
 * - nb_pkts < IAVF_DESCS_PER_LOOP, just return no packet
 */
static __rte_always_inline uint16_t
iavf_recv_scattered_burst_vec_avx2_flex_rxd(void *rx_queue,
					    struct rte_mbuf **rx_pkts,
					    uint16_t nb_pkts, bool offload)
{
	struct ci_rx_queue *rxq = rx_queue;
	uint8_t split_flags[IAVF_VPMD_RX_BURST] = {0};

	/* get some new buffers */
	uint16_t nb_bufs = _iavf_recv_raw_pkts_vec_avx2_flex_rxd(rxq,
					rx_pkts, nb_pkts, split_flags, offload);
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
	return i + ci_rx_reassemble_packets(&rx_pkts[i], nb_bufs - i, &split_flags[i],
		&rxq->pkt_first_seg, &rxq->pkt_last_seg, rxq->crc_len);
}

/**
 * vPMD receive routine that reassembles scattered packets for flex RxD.
 * Main receive routine that can handle arbitrary burst sizes
 * Notice:
 * - nb_pkts < IAVF_DESCS_PER_LOOP, just return no packet
 */
static __rte_always_inline uint16_t
iavf_recv_scattered_pkts_vec_avx2_flex_rxd_common(void *rx_queue,
						  struct rte_mbuf **rx_pkts,
						  uint16_t nb_pkts, bool offload)
{
	uint16_t retval = 0;

	while (nb_pkts > IAVF_VPMD_RX_BURST) {
		uint16_t burst =
			iavf_recv_scattered_burst_vec_avx2_flex_rxd
			(rx_queue, rx_pkts + retval, IAVF_VPMD_RX_BURST,
			 offload);
		retval += burst;
		nb_pkts -= burst;
		if (burst < IAVF_VPMD_RX_BURST)
			return retval;
	}
	return retval + iavf_recv_scattered_burst_vec_avx2_flex_rxd(rx_queue,
				rx_pkts + retval, nb_pkts, offload);
}

uint16_t
iavf_recv_scattered_pkts_vec_avx2_flex_rxd(void *rx_queue,
					   struct rte_mbuf **rx_pkts,
					   uint16_t nb_pkts)
{
	return iavf_recv_scattered_pkts_vec_avx2_flex_rxd_common(rx_queue,
								 rx_pkts,
								 nb_pkts,
								 false);
}

uint16_t
iavf_recv_scattered_pkts_vec_avx2_flex_rxd_offload(void *rx_queue,
						   struct rte_mbuf **rx_pkts,
						   uint16_t nb_pkts)
{
	return iavf_recv_scattered_pkts_vec_avx2_flex_rxd_common(rx_queue,
								 rx_pkts,
								 nb_pkts,
								 true);
}


static __rte_always_inline void
iavf_vtx1(volatile struct iavf_tx_desc *txdp,
	  struct rte_mbuf *pkt, uint64_t flags, bool offload, uint8_t vlan_flag)
{
	uint64_t high_qw =
		(IAVF_TX_DESC_DTYPE_DATA |
		 ((uint64_t)flags  << IAVF_TXD_QW1_CMD_SHIFT) |
		 ((uint64_t)pkt->data_len << IAVF_TXD_QW1_TX_BUF_SZ_SHIFT));
	if (offload)
		iavf_txd_enable_offload(pkt, &high_qw, vlan_flag);

	__m128i descriptor = _mm_set_epi64x(high_qw,
				pkt->buf_iova + pkt->data_off);
	_mm_store_si128(RTE_CAST_PTR(__m128i *, txdp), descriptor);
}

static __rte_always_inline void
iavf_vtx(volatile struct iavf_tx_desc *txdp,
	 struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags, bool offload, uint8_t vlan_flag)
{
	const uint64_t hi_qw_tmpl = (IAVF_TX_DESC_DTYPE_DATA |
			((uint64_t)flags  << IAVF_TXD_QW1_CMD_SHIFT));

	/* if unaligned on 32-bit boundary, do one to align */
	if (((uintptr_t)txdp & 0x1F) != 0 && nb_pkts != 0) {
		iavf_vtx1(txdp, *pkt, flags, offload, vlan_flag);
		nb_pkts--, txdp++, pkt++;
	}

	/* do two at a time while possible, in bursts */
	for (; nb_pkts > 3; txdp += 4, pkt += 4, nb_pkts -= 4) {
		uint64_t hi_qw3 =
			hi_qw_tmpl |
			((uint64_t)pkt[3]->data_len <<
			 IAVF_TXD_QW1_TX_BUF_SZ_SHIFT);
		if (offload)
			iavf_txd_enable_offload(pkt[3], &hi_qw3, vlan_flag);
		uint64_t hi_qw2 =
			hi_qw_tmpl |
			((uint64_t)pkt[2]->data_len <<
			 IAVF_TXD_QW1_TX_BUF_SZ_SHIFT);
		if (offload)
			iavf_txd_enable_offload(pkt[2], &hi_qw2, vlan_flag);
		uint64_t hi_qw1 =
			hi_qw_tmpl |
			((uint64_t)pkt[1]->data_len <<
			 IAVF_TXD_QW1_TX_BUF_SZ_SHIFT);
		if (offload)
			iavf_txd_enable_offload(pkt[1], &hi_qw1, vlan_flag);
		uint64_t hi_qw0 =
			hi_qw_tmpl |
			((uint64_t)pkt[0]->data_len <<
			 IAVF_TXD_QW1_TX_BUF_SZ_SHIFT);
		if (offload)
			iavf_txd_enable_offload(pkt[0], &hi_qw0, vlan_flag);

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
		iavf_vtx1(txdp, *pkt, flags, offload, vlan_flag);
		txdp++, pkt++, nb_pkts--;
	}
}

static __rte_always_inline uint16_t
iavf_xmit_fixed_burst_vec_avx2(void *tx_queue, struct rte_mbuf **tx_pkts,
			       uint16_t nb_pkts, bool offload)
{
	struct ci_tx_queue *txq = (struct ci_tx_queue *)tx_queue;
	volatile struct iavf_tx_desc *txdp;
	struct ci_tx_entry_vec *txep;
	uint16_t n, nb_commit, tx_id;
	/* bit2 is reserved and must be set to 1 according to Spec */
	uint64_t flags = IAVF_TX_DESC_CMD_EOP | IAVF_TX_DESC_CMD_ICRC;
	uint64_t rs = IAVF_TX_DESC_CMD_RS | flags;

	if (txq->nb_tx_free < txq->tx_free_thresh)
		ci_tx_free_bufs_vec(txq, iavf_tx_desc_done, false);

	nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;
	nb_commit = nb_pkts;

	tx_id = txq->tx_tail;
	txdp = &txq->iavf_tx_ring[tx_id];
	txep = &txq->sw_ring_vec[tx_id];

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {
		ci_tx_backlog_entry_vec(txep, tx_pkts, n);

		iavf_vtx(txdp, tx_pkts, n - 1, flags, offload, txq->vlan_flag);
		tx_pkts += (n - 1);
		txdp += (n - 1);

		iavf_vtx1(txdp, *tx_pkts++, rs, offload, txq->vlan_flag);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &txq->iavf_tx_ring[tx_id];
		txep = &txq->sw_ring_vec[tx_id];
	}

	ci_tx_backlog_entry_vec(txep, tx_pkts, nb_commit);

	iavf_vtx(txdp, tx_pkts, nb_commit, flags, offload, txq->vlan_flag);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->tx_next_rs) {
		txq->iavf_tx_ring[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)IAVF_TX_DESC_CMD_RS) <<
					 IAVF_TXD_QW1_CMD_SHIFT);
		txq->tx_next_rs =
			(uint16_t)(txq->tx_next_rs + txq->tx_rs_thresh);
	}

	txq->tx_tail = tx_id;

	IAVF_PCI_REG_WC_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

static __rte_always_inline uint16_t
iavf_xmit_pkts_vec_avx2_common(void *tx_queue, struct rte_mbuf **tx_pkts,
			       uint16_t nb_pkts, bool offload)
{
	uint16_t nb_tx = 0;
	struct ci_tx_queue *txq = (struct ci_tx_queue *)tx_queue;

	while (nb_pkts) {
		uint16_t ret, num;

		/* cross rs_thresh boundary is not allowed */
		num = (uint16_t)RTE_MIN(nb_pkts, txq->tx_rs_thresh);
		ret = iavf_xmit_fixed_burst_vec_avx2(tx_queue, &tx_pkts[nb_tx],
						     num, offload);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}

uint16_t
iavf_xmit_pkts_vec_avx2(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts)
{
	return iavf_xmit_pkts_vec_avx2_common(tx_queue, tx_pkts, nb_pkts, false);
}

uint16_t
iavf_xmit_pkts_vec_avx2_offload(void *tx_queue, struct rte_mbuf **tx_pkts,
				uint16_t nb_pkts)
{
	return iavf_xmit_pkts_vec_avx2_common(tx_queue, tx_pkts, nb_pkts, true);
}
