/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include "iavf_rxtx_vec_common.h"

#include <rte_vect.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#define IAVF_DESCS_PER_LOOP_AVX 8
#define PKTLEN_SHIFT 10

/******************************************************************************
 * If user knows a specific offload is not enabled by APP,
 * the macro can be commented to save the effort of fast path.
 * Currently below 2 features are supported in RX path,
 * 1, checksum offload
 * 2, VLAN/QINQ stripping
 * 3, RSS hash
 * 4, packet type analysis
 * 5, flow director ID report
 ******************************************************************************/
#define IAVF_RX_CSUM_OFFLOAD
#define IAVF_RX_VLAN_OFFLOAD
#define IAVF_RX_RSS_OFFLOAD
#define IAVF_RX_PTYPE_OFFLOAD
#define IAVF_RX_FDIR_OFFLOAD

static __rte_always_inline void
iavf_rxq_rearm(struct iavf_rx_queue *rxq)
{
	iavf_rxq_rearm_common(rxq, true);
}

#define IAVF_RX_LEN_MASK 0x80808080
static __rte_always_inline uint16_t
_iavf_recv_raw_pkts_vec_avx512(struct iavf_rx_queue *rxq,
			       struct rte_mbuf **rx_pkts,
			       uint16_t nb_pkts, uint8_t *split_packet,
			       bool offload)
{
#ifdef IAVF_RX_PTYPE_OFFLOAD
	const uint32_t *type_table = rxq->vsi->adapter->ptype_tbl;
#endif

	const __m256i mbuf_init = _mm256_set_epi64x(0, 0, 0,
						    rxq->mbuf_initializer);
	struct rte_mbuf **sw_ring = &rxq->sw_ring[rxq->rx_tail];
	volatile union iavf_rx_desc *rxdp = rxq->rx_ring + rxq->rx_tail;

	rte_prefetch0(rxdp);

	/* nb_pkts has to be floor-aligned to IAVF_DESCS_PER_LOOP_AVX */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, IAVF_DESCS_PER_LOOP_AVX);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > IAVF_RXQ_REARM_THRESH)
		iavf_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->wb.qword1.status_error_len &
	      rte_cpu_to_le_32(1 << IAVF_RX_DESC_STATUS_DD_SHIFT)))
		return 0;

	/* constants used in processing loop */
	const __m512i crc_adjust =
		_mm512_set_epi32
			(/* 1st descriptor */
			 0,             /* ignore non-length fields */
			 -rxq->crc_len, /* sub crc on data_len */
			 -rxq->crc_len, /* sub crc on pkt_len */
			 0,             /* ignore pkt_type field */
			 /* 2nd descriptor */
			 0,             /* ignore non-length fields */
			 -rxq->crc_len, /* sub crc on data_len */
			 -rxq->crc_len, /* sub crc on pkt_len */
			 0,             /* ignore pkt_type field */
			 /* 3rd descriptor */
			 0,             /* ignore non-length fields */
			 -rxq->crc_len, /* sub crc on data_len */
			 -rxq->crc_len, /* sub crc on pkt_len */
			 0,             /* ignore pkt_type field */
			 /* 4th descriptor */
			 0,             /* ignore non-length fields */
			 -rxq->crc_len, /* sub crc on data_len */
			 -rxq->crc_len, /* sub crc on pkt_len */
			 0              /* ignore pkt_type field */
			);

	/* 8 packets DD mask, LSB in each 32-bit value */
	const __m256i dd_check = _mm256_set1_epi32(1);

	/* 8 packets EOP mask, second-LSB in each 32-bit value */
	const __m256i eop_check = _mm256_slli_epi32(dd_check,
			IAVF_RX_DESC_STATUS_EOF_SHIFT);

	/* mask to shuffle from desc. to mbuf (4 descriptors)*/
	const __m512i shuf_msk =
		_mm512_set_epi32
			(/* 1st descriptor */
			 0x07060504,    /* octet 4~7, 32bits rss */
			 0x03020F0E,    /* octet 2~3, low 16 bits vlan_macip */
					/* octet 15~14, 16 bits data_len */
			 0xFFFF0F0E,    /* skip high 16 bits pkt_len, zero out */
					/* octet 15~14, low 16 bits pkt_len */
			 0xFFFFFFFF,    /* pkt_type set as unknown */
			 /* 2nd descriptor */
			 0x07060504,    /* octet 4~7, 32bits rss */
			 0x03020F0E,    /* octet 2~3, low 16 bits vlan_macip */
					/* octet 15~14, 16 bits data_len */
			 0xFFFF0F0E,    /* skip high 16 bits pkt_len, zero out */
					/* octet 15~14, low 16 bits pkt_len */
			 0xFFFFFFFF,    /* pkt_type set as unknown */
			 /* 3rd descriptor */
			 0x07060504,    /* octet 4~7, 32bits rss */
			 0x03020F0E,    /* octet 2~3, low 16 bits vlan_macip */
					/* octet 15~14, 16 bits data_len */
			 0xFFFF0F0E,    /* skip high 16 bits pkt_len, zero out */
					/* octet 15~14, low 16 bits pkt_len */
			 0xFFFFFFFF,    /* pkt_type set as unknown */
			 /* 4th descriptor */
			 0x07060504,    /* octet 4~7, 32bits rss */
			 0x03020F0E,    /* octet 2~3, low 16 bits vlan_macip */
					/* octet 15~14, 16 bits data_len */
			 0xFFFF0F0E,    /* skip high 16 bits pkt_len, zero out */
					/* octet 15~14, low 16 bits pkt_len */
			 0xFFFFFFFF     /* pkt_type set as unknown */
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

	uint16_t i, received;

	for (i = 0, received = 0; i < nb_pkts;
	     i += IAVF_DESCS_PER_LOOP_AVX,
	     rxdp += IAVF_DESCS_PER_LOOP_AVX) {
		/* step 1, copy over 8 mbuf pointers to rx_pkts array */
		_mm256_storeu_si256((void *)&rx_pkts[i],
				    _mm256_loadu_si256((void *)&sw_ring[i]));
#ifdef RTE_ARCH_X86_64
		_mm256_storeu_si256
			((void *)&rx_pkts[i + 4],
			 _mm256_loadu_si256((void *)&sw_ring[i + 4]));
#endif

		__m512i raw_desc0_3, raw_desc4_7;
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

		raw_desc4_7 = _mm512_broadcast_i32x4(raw_desc4);
		raw_desc4_7 = _mm512_inserti32x4(raw_desc4_7, raw_desc5, 1);
		raw_desc4_7 = _mm512_inserti32x4(raw_desc4_7, raw_desc6, 2);
		raw_desc4_7 = _mm512_inserti32x4(raw_desc4_7, raw_desc7, 3);
		raw_desc0_3 = _mm512_broadcast_i32x4(raw_desc0);
		raw_desc0_3 = _mm512_inserti32x4(raw_desc0_3, raw_desc1, 1);
		raw_desc0_3 = _mm512_inserti32x4(raw_desc0_3, raw_desc2, 2);
		raw_desc0_3 = _mm512_inserti32x4(raw_desc0_3, raw_desc3, 3);

		if (split_packet) {
			int j;

			for (j = 0; j < IAVF_DESCS_PER_LOOP_AVX; j++)
				rte_mbuf_prefetch_part2(rx_pkts[i + j]);
		}

		/**
		 * convert descriptors 4-7 into mbufs, adjusting length and
		 * re-arranging fields. Then write into the mbuf
		 */
		const __m512i len4_7 = _mm512_slli_epi32(raw_desc4_7,
							 PKTLEN_SHIFT);
		const __m512i desc4_7 = _mm512_mask_blend_epi16(IAVF_RX_LEN_MASK,
								raw_desc4_7,
								len4_7);
		__m512i mb4_7 = _mm512_shuffle_epi8(desc4_7, shuf_msk);

		mb4_7 = _mm512_add_epi32(mb4_7, crc_adjust);
#ifdef IAVF_RX_PTYPE_OFFLOAD
		/**
		 * to get packet types, shift 64-bit values down 30 bits
		 * and so ptype is in lower 8-bits in each
		 */
		const __m512i ptypes4_7 = _mm512_srli_epi64(desc4_7, 30);
		const __m256i ptypes6_7 = _mm512_extracti64x4_epi64(ptypes4_7, 1);
		const __m256i ptypes4_5 = _mm512_extracti64x4_epi64(ptypes4_7, 0);
		const uint8_t ptype7 = _mm256_extract_epi8(ptypes6_7, 24);
		const uint8_t ptype6 = _mm256_extract_epi8(ptypes6_7, 8);
		const uint8_t ptype5 = _mm256_extract_epi8(ptypes4_5, 24);
		const uint8_t ptype4 = _mm256_extract_epi8(ptypes4_5, 8);

		const __m512i ptype4_7 = _mm512_set_epi32
			(0, 0, 0, type_table[ptype7],
			 0, 0, 0, type_table[ptype6],
			 0, 0, 0, type_table[ptype5],
			 0, 0, 0, type_table[ptype4]);
		mb4_7 = _mm512_mask_blend_epi32(0x1111, mb4_7, ptype4_7);
#endif

		/**
		 * convert descriptors 0-3 into mbufs, adjusting length and
		 * re-arranging fields. Then write into the mbuf
		 */
		const __m512i len0_3 = _mm512_slli_epi32(raw_desc0_3,
							 PKTLEN_SHIFT);
		const __m512i desc0_3 = _mm512_mask_blend_epi16(IAVF_RX_LEN_MASK,
								raw_desc0_3,
								len0_3);
		__m512i mb0_3 = _mm512_shuffle_epi8(desc0_3, shuf_msk);

		mb0_3 = _mm512_add_epi32(mb0_3, crc_adjust);
#ifdef IAVF_RX_PTYPE_OFFLOAD
		/* get the packet types */
		const __m512i ptypes0_3 = _mm512_srli_epi64(desc0_3, 30);
		const __m256i ptypes2_3 = _mm512_extracti64x4_epi64(ptypes0_3, 1);
		const __m256i ptypes0_1 = _mm512_extracti64x4_epi64(ptypes0_3, 0);
		const uint8_t ptype3 = _mm256_extract_epi8(ptypes2_3, 24);
		const uint8_t ptype2 = _mm256_extract_epi8(ptypes2_3, 8);
		const uint8_t ptype1 = _mm256_extract_epi8(ptypes0_1, 24);
		const uint8_t ptype0 = _mm256_extract_epi8(ptypes0_1, 8);

		const __m512i ptype0_3 = _mm512_set_epi32
			(0, 0, 0, type_table[ptype3],
			 0, 0, 0, type_table[ptype2],
			 0, 0, 0, type_table[ptype1],
			 0, 0, 0, type_table[ptype0]);
		mb0_3 = _mm512_mask_blend_epi32(0x1111, mb0_3, ptype0_3);
#endif

		/**
		 * use permute/extract to get status content
		 * After the operations, the packets status flags are in the
		 * order (hi->lo): [1, 3, 5, 7, 0, 2, 4, 6]
		 */
		/* merge the status bits into one register */
		const __m512i status_permute_msk = _mm512_set_epi32
			(0, 0, 0, 0,
			 0, 0, 0, 0,
			 22, 30, 6, 14,
			 18, 26, 2, 10);
		const __m512i raw_status0_7 = _mm512_permutex2var_epi32
			(raw_desc4_7, status_permute_msk, raw_desc0_3);
		__m256i status0_7 = _mm512_extracti64x4_epi64
			(raw_status0_7, 0);

		/* now do flag manipulation */

		/* merge flags */
		__m256i mbuf_flags = _mm256_set1_epi32(0);

		if (offload) {
#if defined(IAVF_RX_CSUM_OFFLOAD) || defined(IAVF_RX_VLAN_OFFLOAD) || defined(IAVF_RX_RSS_OFFLOAD)
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
#endif

#ifdef IAVF_RX_VLAN_OFFLOAD
			/**
			 * data to be shuffled by result of flag mask. If VLAN bit is set,
			 * (bit 2), then position 4 in this array will be used in the
			 * destination
			 */
			const __m256i vlan_flags_shuf =
				_mm256_set_epi32(0, 0, RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED, 0,
						 0, 0, RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED, 0);
#endif

#ifdef IAVF_RX_RSS_OFFLOAD
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
#endif

#ifdef IAVF_RX_CSUM_OFFLOAD
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
#endif

#if defined(IAVF_RX_CSUM_OFFLOAD) || defined(IAVF_RX_VLAN_OFFLOAD) || defined(IAVF_RX_RSS_OFFLOAD)
			/* get only flag/error bits we want */
			const __m256i flag_bits =
				_mm256_and_si256(status0_7, flags_mask);
#endif
			/* set vlan and rss flags */
#ifdef IAVF_RX_VLAN_OFFLOAD
			const __m256i vlan_flags =
				_mm256_shuffle_epi8(vlan_flags_shuf, flag_bits);
#endif
#ifdef IAVF_RX_RSS_OFFLOAD
			const __m256i rss_flags =
				_mm256_shuffle_epi8(rss_flags_shuf,
						    _mm256_srli_epi32(flag_bits, 11));
#endif
#ifdef IAVF_RX_CSUM_OFFLOAD
			/**
			 * l3_l4_error flags, shuffle, then shift to correct adjustment
			 * of flags in flags_shuf, and finally mask out extra bits
			 */
			__m256i l3_l4_flags = _mm256_shuffle_epi8(l3_l4_flags_shuf,
							_mm256_srli_epi32(flag_bits, 22));
			l3_l4_flags = _mm256_slli_epi32(l3_l4_flags, 1);
			l3_l4_flags = _mm256_and_si256(l3_l4_flags, cksum_mask);
#endif

#ifdef IAVF_RX_CSUM_OFFLOAD
			mbuf_flags = _mm256_or_si256(mbuf_flags, l3_l4_flags);
#endif
#ifdef IAVF_RX_RSS_OFFLOAD
			mbuf_flags = _mm256_or_si256(mbuf_flags, rss_flags);
#endif
#ifdef IAVF_RX_VLAN_OFFLOAD
			mbuf_flags = _mm256_or_si256(mbuf_flags, vlan_flags);
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
		const __m256i mb4_5 = _mm512_extracti64x4_epi64(mb4_7, 0);
		const __m256i mb6_7 = _mm512_extracti64x4_epi64(mb4_7, 1);
		const __m256i mb0_1 = _mm512_extracti64x4_epi64(mb0_3, 0);
		const __m256i mb2_3 = _mm512_extracti64x4_epi64(mb0_3, 1);

		if (offload) {
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
		} else {
			rearm6 = _mm256_permute2f128_si256(mbuf_init, mb6_7, 0x20);
			rearm4 = _mm256_permute2f128_si256(mbuf_init, mb4_5, 0x20);
			rearm2 = _mm256_permute2f128_si256(mbuf_init, mb2_3, 0x20);
			rearm0 = _mm256_permute2f128_si256(mbuf_init, mb0_1, 0x20);
		}
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
		if (offload) {
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
		} else {
			rearm7 = _mm256_blend_epi32(mbuf_init, mb6_7, 0xF0);
			rearm5 = _mm256_blend_epi32(mbuf_init, mb4_5, 0xF0);
			rearm3 = _mm256_blend_epi32(mbuf_init, mb2_3, 0xF0);
			rearm1 = _mm256_blend_epi32(mbuf_init, mb0_1, 0xF0);
		}
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
			split_packet += IAVF_DESCS_PER_LOOP_AVX;
		}

		/* perform dd_check */
		status0_7 = _mm256_and_si256(status0_7, dd_check);
		status0_7 = _mm256_packs_epi32(status0_7,
					       _mm256_setzero_si256());

		uint64_t burst = __builtin_popcountll
					(_mm_cvtsi128_si64
						(_mm256_extracti128_si256
							(status0_7, 1)));
		burst += __builtin_popcountll
				(_mm_cvtsi128_si64
					(_mm256_castsi256_si128(status0_7)));
		received += burst;
		if (burst != IAVF_DESCS_PER_LOOP_AVX)
			break;
	}

	/* update tail pointers */
	rxq->rx_tail += received;
	rxq->rx_tail &= (rxq->nb_rx_desc - 1);
	if ((rxq->rx_tail & 1) == 1 && received > 1) { /* keep aligned */
		rxq->rx_tail--;
		received--;
	}
	rxq->rxrearm_nb += received;
	return received;
}

static __rte_always_inline __m256i
flex_rxd_to_fdir_flags_vec_avx512(const __m256i fdir_id0_7)
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
_iavf_recv_raw_pkts_vec_avx512_flex_rxd(struct iavf_rx_queue *rxq,
					struct rte_mbuf **rx_pkts,
					uint16_t nb_pkts,
					uint8_t *split_packet,
					bool offload)
{
	struct iavf_adapter *adapter = rxq->vsi->adapter;

	uint64_t offloads = adapter->dev_data->dev_conf.rxmode.offloads;

#ifdef IAVF_RX_PTYPE_OFFLOAD
	const uint32_t *type_table = adapter->ptype_tbl;
#endif

	const __m256i mbuf_init = _mm256_set_epi64x(0, 0, 0,
						    rxq->mbuf_initializer);
	struct rte_mbuf **sw_ring = &rxq->sw_ring[rxq->rx_tail];
	volatile union iavf_rx_flex_desc *rxdp =
		(union iavf_rx_flex_desc *)rxq->rx_ring + rxq->rx_tail;

	rte_prefetch0(rxdp);

	/* nb_pkts has to be floor-aligned to IAVF_DESCS_PER_LOOP_AVX */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, IAVF_DESCS_PER_LOOP_AVX);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > IAVF_RXQ_REARM_THRESH)
		iavf_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->wb.status_error0 &
	      rte_cpu_to_le_32(1 << IAVF_RX_FLEX_DESC_STATUS0_DD_S)))
		return 0;

	/* constants used in processing loop */
	const __m512i crc_adjust =
		_mm512_set_epi32
			(/* 1st descriptor */
			 0,             /* ignore non-length fields */
			 -rxq->crc_len, /* sub crc on data_len */
			 -rxq->crc_len, /* sub crc on pkt_len */
			 0,             /* ignore pkt_type field */
			 /* 2nd descriptor */
			 0,             /* ignore non-length fields */
			 -rxq->crc_len, /* sub crc on data_len */
			 -rxq->crc_len, /* sub crc on pkt_len */
			 0,             /* ignore pkt_type field */
			 /* 3rd descriptor */
			 0,             /* ignore non-length fields */
			 -rxq->crc_len, /* sub crc on data_len */
			 -rxq->crc_len, /* sub crc on pkt_len */
			 0,             /* ignore pkt_type field */
			 /* 4th descriptor */
			 0,             /* ignore non-length fields */
			 -rxq->crc_len, /* sub crc on data_len */
			 -rxq->crc_len, /* sub crc on pkt_len */
			 0              /* ignore pkt_type field */
			);

	/* 8 packets DD mask, LSB in each 32-bit value */
	const __m256i dd_check = _mm256_set1_epi32(1);

	/* 8 packets EOP mask, second-LSB in each 32-bit value */
	const __m256i eop_check = _mm256_slli_epi32(dd_check,
			IAVF_RX_FLEX_DESC_STATUS0_EOF_S);

	/* mask to shuffle from desc. to mbuf (4 descriptors)*/
	const __m512i shuf_msk =
		_mm512_set_epi32
			(/* 1st descriptor */
			 0xFFFFFFFF,    /* rss hash parsed separately */
			 0x0B0A0504,    /* octet 10~11, 16 bits vlan_macip */
					/* octet 4~5, 16 bits data_len */
			 0xFFFF0504,    /* skip hi 16 bits pkt_len, zero out */
					/* octet 4~5, 16 bits pkt_len */
			 0xFFFFFFFF,    /* pkt_type set as unknown */
			 /* 2nd descriptor */
			 0xFFFFFFFF,    /* rss hash parsed separately */
			 0x0B0A0504,    /* octet 10~11, 16 bits vlan_macip */
					/* octet 4~5, 16 bits data_len */
			 0xFFFF0504,    /* skip hi 16 bits pkt_len, zero out */
					/* octet 4~5, 16 bits pkt_len */
			 0xFFFFFFFF,    /* pkt_type set as unknown */
			 /* 3rd descriptor */
			 0xFFFFFFFF,    /* rss hash parsed separately */
			 0x0B0A0504,    /* octet 10~11, 16 bits vlan_macip */
					/* octet 4~5, 16 bits data_len */
			 0xFFFF0504,    /* skip hi 16 bits pkt_len, zero out */
					/* octet 4~5, 16 bits pkt_len */
			 0xFFFFFFFF,    /* pkt_type set as unknown */
			 /* 4th descriptor */
			 0xFFFFFFFF,    /* rss hash parsed separately */
			 0x0B0A0504,    /* octet 10~11, 16 bits vlan_macip */
					/* octet 4~5, 16 bits data_len */
			 0xFFFF0504,    /* skip hi 16 bits pkt_len, zero out */
					/* octet 4~5, 16 bits pkt_len */
			 0xFFFFFFFF     /* pkt_type set as unknown */
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

	uint16_t i, received;

	for (i = 0, received = 0; i < nb_pkts;
	     i += IAVF_DESCS_PER_LOOP_AVX,
	     rxdp += IAVF_DESCS_PER_LOOP_AVX) {
		/* step 1, copy over 8 mbuf pointers to rx_pkts array */
		_mm256_storeu_si256((void *)&rx_pkts[i],
				    _mm256_loadu_si256((void *)&sw_ring[i]));
#ifdef RTE_ARCH_X86_64
		_mm256_storeu_si256
			((void *)&rx_pkts[i + 4],
			 _mm256_loadu_si256((void *)&sw_ring[i + 4]));
#endif

		__m512i raw_desc0_3, raw_desc4_7;

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

		raw_desc4_7 = _mm512_broadcast_i32x4(raw_desc4);
		raw_desc4_7 = _mm512_inserti32x4(raw_desc4_7, raw_desc5, 1);
		raw_desc4_7 = _mm512_inserti32x4(raw_desc4_7, raw_desc6, 2);
		raw_desc4_7 = _mm512_inserti32x4(raw_desc4_7, raw_desc7, 3);
		raw_desc0_3 = _mm512_broadcast_i32x4(raw_desc0);
		raw_desc0_3 = _mm512_inserti32x4(raw_desc0_3, raw_desc1, 1);
		raw_desc0_3 = _mm512_inserti32x4(raw_desc0_3, raw_desc2, 2);
		raw_desc0_3 = _mm512_inserti32x4(raw_desc0_3, raw_desc3, 3);

		if (split_packet) {
			int j;

			for (j = 0; j < IAVF_DESCS_PER_LOOP_AVX; j++)
				rte_mbuf_prefetch_part2(rx_pkts[i + j]);
		}

		/**
		 * convert descriptors 4-7 into mbufs, re-arrange fields.
		 * Then write into the mbuf.
		 */
		__m512i mb4_7 = _mm512_shuffle_epi8(raw_desc4_7, shuf_msk);

		mb4_7 = _mm512_add_epi32(mb4_7, crc_adjust);
#ifdef IAVF_RX_PTYPE_OFFLOAD
		/**
		 * to get packet types, ptype is located in bit16-25
		 * of each 128bits
		 */
		const __m512i ptype_mask =
			_mm512_set1_epi16(IAVF_RX_FLEX_DESC_PTYPE_M);
		const __m512i ptypes4_7 =
			_mm512_and_si512(raw_desc4_7, ptype_mask);
		const __m256i ptypes6_7 = _mm512_extracti64x4_epi64(ptypes4_7, 1);
		const __m256i ptypes4_5 = _mm512_extracti64x4_epi64(ptypes4_7, 0);
		const uint16_t ptype7 = _mm256_extract_epi16(ptypes6_7, 9);
		const uint16_t ptype6 = _mm256_extract_epi16(ptypes6_7, 1);
		const uint16_t ptype5 = _mm256_extract_epi16(ptypes4_5, 9);
		const uint16_t ptype4 = _mm256_extract_epi16(ptypes4_5, 1);

		const __m512i ptype4_7 = _mm512_set_epi32
			(0, 0, 0, type_table[ptype7],
			 0, 0, 0, type_table[ptype6],
			 0, 0, 0, type_table[ptype5],
			 0, 0, 0, type_table[ptype4]);
		mb4_7 = _mm512_mask_blend_epi32(0x1111, mb4_7, ptype4_7);
#endif

		/**
		 * convert descriptors 0-3 into mbufs, re-arrange fields.
		 * Then write into the mbuf.
		 */
		__m512i mb0_3 = _mm512_shuffle_epi8(raw_desc0_3, shuf_msk);

		mb0_3 = _mm512_add_epi32(mb0_3, crc_adjust);
#ifdef IAVF_RX_PTYPE_OFFLOAD
		/**
		 * to get packet types, ptype is located in bit16-25
		 * of each 128bits
		 */
		const __m512i ptypes0_3 =
			_mm512_and_si512(raw_desc0_3, ptype_mask);
		const __m256i ptypes2_3 = _mm512_extracti64x4_epi64(ptypes0_3, 1);
		const __m256i ptypes0_1 = _mm512_extracti64x4_epi64(ptypes0_3, 0);
		const uint16_t ptype3 = _mm256_extract_epi16(ptypes2_3, 9);
		const uint16_t ptype2 = _mm256_extract_epi16(ptypes2_3, 1);
		const uint16_t ptype1 = _mm256_extract_epi16(ptypes0_1, 9);
		const uint16_t ptype0 = _mm256_extract_epi16(ptypes0_1, 1);

		const __m512i ptype0_3 = _mm512_set_epi32
			(0, 0, 0, type_table[ptype3],
			 0, 0, 0, type_table[ptype2],
			 0, 0, 0, type_table[ptype1],
			 0, 0, 0, type_table[ptype0]);
		mb0_3 = _mm512_mask_blend_epi32(0x1111, mb0_3, ptype0_3);
#endif

		/**
		 * use permute/extract to get status content
		 * After the operations, the packets status flags are in the
		 * order (hi->lo): [1, 3, 5, 7, 0, 2, 4, 6]
		 */
		/* merge the status bits into one register */
		const __m512i status_permute_msk = _mm512_set_epi32
			(0, 0, 0, 0,
			 0, 0, 0, 0,
			 22, 30, 6, 14,
			 18, 26, 2, 10);
		const __m512i raw_status0_7 = _mm512_permutex2var_epi32
			(raw_desc4_7, status_permute_msk, raw_desc0_3);
		__m256i status0_7 = _mm512_extracti64x4_epi64
			(raw_status0_7, 0);

		/* now do flag manipulation */

		/* merge flags */
		__m256i mbuf_flags = _mm256_set1_epi32(0);
		__m256i vlan_flags = _mm256_setzero_si256();

		if (offload) {
#if defined(IAVF_RX_CSUM_OFFLOAD) || defined(IAVF_RX_VLAN_OFFLOAD) || defined(IAVF_RX_RSS_OFFLOAD)
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
#endif
#ifdef IAVF_RX_CSUM_OFFLOAD
			/**
			 * data to be shuffled by the result of the flags mask shifted by 4
			 * bits.  This gives use the l3_l4 flags.
			 */
			const __m256i l3_l4_flags_shuf =
				_mm256_set_epi8((RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				/**
				 * second 128-bits
				 * shift right 20 bits to use the low two bits to indicate
				 * outer checksum status
				 * shift right 1 bit to make sure it not exceed 255
				 */
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				 RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				 RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1,
				(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD >> 20 |
				 RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1);
			const __m256i cksum_mask =
				 _mm256_set1_epi32(RTE_MBUF_F_RX_IP_CKSUM_MASK |
						   RTE_MBUF_F_RX_L4_CKSUM_MASK |
						   RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
						   RTE_MBUF_F_RX_OUTER_L4_CKSUM_MASK);
#endif
#if defined(IAVF_RX_VLAN_OFFLOAD) || defined(IAVF_RX_RSS_OFFLOAD)
			/**
			 * data to be shuffled by result of flag mask, shifted down 12.
			 * If RSS(bit12)/VLAN(bit13) are set,
			 * shuffle moves appropriate flags in place.
			 */
			const __m256i rss_flags_shuf = _mm256_set_epi8
					(0, 0, 0, 0,
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

			const __m256i vlan_flags_shuf = _mm256_set_epi8
					(0, 0, 0, 0,
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
#endif

#if defined(IAVF_RX_CSUM_OFFLOAD) || defined(IAVF_RX_VLAN_OFFLOAD) || defined(IAVF_RX_RSS_OFFLOAD)
			/* get only flag/error bits we want */
			const __m256i flag_bits =
				_mm256_and_si256(status0_7, flags_mask);
#endif
#ifdef IAVF_RX_CSUM_OFFLOAD
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
#endif
#if defined(IAVF_RX_VLAN_OFFLOAD) || defined(IAVF_RX_RSS_OFFLOAD)
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

#endif

#ifdef IAVF_RX_CSUM_OFFLOAD
			mbuf_flags = _mm256_or_si256(mbuf_flags, l3_l4_flags);
#endif
#if defined(IAVF_RX_VLAN_OFFLOAD) || defined(IAVF_RX_RSS_OFFLOAD)
			mbuf_flags = _mm256_or_si256(mbuf_flags, rss_vlan_flags);
#endif
		}

#ifdef IAVF_RX_FDIR_OFFLOAD
		if (rxq->fdir_enabled) {
			const __m512i fdir_permute_mask = _mm512_set_epi32
				(0, 0, 0, 0,
				 0, 0, 0, 0,
				 7, 15, 23, 31,
				 3, 11, 19, 27);
			__m512i fdir_tmp = _mm512_permutex2var_epi32
				(raw_desc0_3, fdir_permute_mask, raw_desc4_7);
			const __m256i fdir_id0_7 = _mm512_extracti64x4_epi64
				(fdir_tmp, 0);
			const __m256i fdir_flags =
				flex_rxd_to_fdir_flags_vec_avx512(fdir_id0_7);

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
#endif

		__m256i mb4_5 = _mm512_extracti64x4_epi64(mb4_7, 0);
		__m256i mb6_7 = _mm512_extracti64x4_epi64(mb4_7, 1);
		__m256i mb0_1 = _mm512_extracti64x4_epi64(mb0_3, 0);
		__m256i mb2_3 = _mm512_extracti64x4_epi64(mb0_3, 1);

#ifndef RTE_LIBRTE_IAVF_16BYTE_RX_DESC
		if (offload) {
#ifdef IAVF_RX_RSS_OFFLOAD
			/**
			 * needs to load 2nd 16B of each desc for RSS hash parsing,
			 * will cause performance drop to get into this context.
			 */
			if (offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH ||
			    rxq->rx_flags & IAVF_RX_FLAGS_VLAN_TAG_LOC_L2TAG2_2) {
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

				if (offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH) {
					/**
					 * to shift the 32b RSS hash value to the
					 * highest 32b of each 128b before mask
					 */
					__m256i rss_hash6_7 =
						_mm256_slli_epi64
						(raw_desc_bh6_7, 32);
					__m256i rss_hash4_5 =
						_mm256_slli_epi64
						(raw_desc_bh4_5, 32);
					__m256i rss_hash2_3 =
						_mm256_slli_epi64
						(raw_desc_bh2_3, 32);
					__m256i rss_hash0_1 =
						_mm256_slli_epi64
						(raw_desc_bh0_1, 32);

					const __m256i rss_hash_msk =
						_mm256_set_epi32
						(0xFFFFFFFF, 0, 0, 0,
						 0xFFFFFFFF, 0, 0, 0);

					rss_hash6_7 = _mm256_and_si256
						(rss_hash6_7, rss_hash_msk);
					rss_hash4_5 = _mm256_and_si256
						(rss_hash4_5, rss_hash_msk);
					rss_hash2_3 = _mm256_and_si256
						(rss_hash2_3, rss_hash_msk);
					rss_hash0_1 = _mm256_and_si256
						(rss_hash0_1, rss_hash_msk);

					mb6_7 = _mm256_or_si256
						(mb6_7, rss_hash6_7);
					mb4_5 = _mm256_or_si256
						(mb4_5, rss_hash4_5);
					mb2_3 = _mm256_or_si256
						(mb2_3, rss_hash2_3);
					mb0_1 = _mm256_or_si256
						(mb0_1, rss_hash0_1);
				}

				if (rxq->rx_flags & IAVF_RX_FLAGS_VLAN_TAG_LOC_L2TAG2_2) {
					/* merge the status/error-1 bits into one register */
					const __m256i status1_4_7 =
						_mm256_unpacklo_epi32
						(raw_desc_bh6_7,
						 raw_desc_bh4_5);
					const __m256i status1_0_3 =
						_mm256_unpacklo_epi32
						(raw_desc_bh2_3,
						 raw_desc_bh0_1);

					const __m256i status1_0_7 =
						_mm256_unpacklo_epi64
						(status1_4_7, status1_0_3);

					const __m256i l2tag2p_flag_mask =
						_mm256_set1_epi32
						(1 << IAVF_RX_FLEX_DESC_STATUS1_L2TAG2P_S);

					__m256i l2tag2p_flag_bits =
						_mm256_and_si256
						(status1_0_7,
						 l2tag2p_flag_mask);

					l2tag2p_flag_bits =
						_mm256_srli_epi32
						(l2tag2p_flag_bits,
						 IAVF_RX_FLEX_DESC_STATUS1_L2TAG2P_S);

					const __m256i l2tag2_flags_shuf =
						_mm256_set_epi8
							(0, 0, 0, 0,
							 0, 0, 0, 0,
							 0, 0, 0, 0,
							 0, 0, 0, 0,
							 /* end up 128-bits */
							 0, 0, 0, 0,
							 0, 0, 0, 0,
							 0, 0, 0, 0,
							 0, 0,
							 RTE_MBUF_F_RX_VLAN |
							 RTE_MBUF_F_RX_VLAN_STRIPPED,
							 0);

					vlan_flags =
						_mm256_shuffle_epi8
							(l2tag2_flags_shuf,
							 l2tag2p_flag_bits);

					/* merge with vlan_flags */
					mbuf_flags = _mm256_or_si256
							(mbuf_flags,
							 vlan_flags);

					/* L2TAG2_2 */
					__m256i vlan_tci6_7 =
						_mm256_slli_si256
							(raw_desc_bh6_7, 4);
					__m256i vlan_tci4_5 =
						_mm256_slli_si256
							(raw_desc_bh4_5, 4);
					__m256i vlan_tci2_3 =
						_mm256_slli_si256
							(raw_desc_bh2_3, 4);
					__m256i vlan_tci0_1 =
						_mm256_slli_si256
							(raw_desc_bh0_1, 4);

					const __m256i vlan_tci_msk =
						_mm256_set_epi32
						(0, 0xFFFF0000, 0, 0,
						 0, 0xFFFF0000, 0, 0);

					vlan_tci6_7 = _mm256_and_si256
							(vlan_tci6_7,
							 vlan_tci_msk);
					vlan_tci4_5 = _mm256_and_si256
							(vlan_tci4_5,
							 vlan_tci_msk);
					vlan_tci2_3 = _mm256_and_si256
							(vlan_tci2_3,
							 vlan_tci_msk);
					vlan_tci0_1 = _mm256_and_si256
							(vlan_tci0_1,
							 vlan_tci_msk);

					mb6_7 = _mm256_or_si256
							(mb6_7, vlan_tci6_7);
					mb4_5 = _mm256_or_si256
							(mb4_5, vlan_tci4_5);
					mb2_3 = _mm256_or_si256
							(mb2_3, vlan_tci2_3);
					mb0_1 = _mm256_or_si256
							(mb0_1, vlan_tci0_1);
				}
			} /* if() on RSS hash parsing */
#endif
		}
#endif

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
			split_packet += IAVF_DESCS_PER_LOOP_AVX;
		}

		/* perform dd_check */
		status0_7 = _mm256_and_si256(status0_7, dd_check);
		status0_7 = _mm256_packs_epi32(status0_7,
					       _mm256_setzero_si256());

		uint64_t burst = __builtin_popcountll
					(_mm_cvtsi128_si64
						(_mm256_extracti128_si256
							(status0_7, 1)));
		burst += __builtin_popcountll
				(_mm_cvtsi128_si64
					(_mm256_castsi256_si128(status0_7)));
		received += burst;
		if (burst != IAVF_DESCS_PER_LOOP_AVX)
			break;
	}

	/* update tail pointers */
	rxq->rx_tail += received;
	rxq->rx_tail &= (rxq->nb_rx_desc - 1);
	if ((rxq->rx_tail & 1) == 1 && received > 1) { /* keep aligned */
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
iavf_recv_pkts_vec_avx512(void *rx_queue, struct rte_mbuf **rx_pkts,
			  uint16_t nb_pkts)
{
	return _iavf_recv_raw_pkts_vec_avx512(rx_queue, rx_pkts, nb_pkts,
					      NULL, false);
}

/**
 * Notice:
 * - nb_pkts < IAVF_DESCS_PER_LOOP, just return no packet
 */
uint16_t
iavf_recv_pkts_vec_avx512_flex_rxd(void *rx_queue, struct rte_mbuf **rx_pkts,
				   uint16_t nb_pkts)
{
	return _iavf_recv_raw_pkts_vec_avx512_flex_rxd(rx_queue, rx_pkts,
						       nb_pkts, NULL, false);
}

/**
 * vPMD receive routine that reassembles single burst of 32 scattered packets
 * Notice:
 * - nb_pkts < IAVF_DESCS_PER_LOOP, just return no packet
 */
static __rte_always_inline uint16_t
iavf_recv_scattered_burst_vec_avx512(void *rx_queue, struct rte_mbuf **rx_pkts,
				     uint16_t nb_pkts, bool offload)
{
	struct iavf_rx_queue *rxq = rx_queue;
	uint8_t split_flags[IAVF_VPMD_RX_MAX_BURST] = {0};

	/* get some new buffers */
	uint16_t nb_bufs = _iavf_recv_raw_pkts_vec_avx512(rxq, rx_pkts, nb_pkts,
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
	return i + reassemble_packets(rxq, &rx_pkts[i], nb_bufs - i,
				      &split_flags[i]);
}

/**
 * vPMD receive routine that reassembles scattered packets.
 * Main receive routine that can handle arbitrary burst sizes
 * Notice:
 * - nb_pkts < IAVF_DESCS_PER_LOOP, just return no packet
 */
static __rte_always_inline uint16_t
iavf_recv_scattered_pkts_vec_avx512_cmn(void *rx_queue, struct rte_mbuf **rx_pkts,
					uint16_t nb_pkts, bool offload)
{
	uint16_t retval = 0;

	while (nb_pkts > IAVF_VPMD_RX_MAX_BURST) {
		uint16_t burst = iavf_recv_scattered_burst_vec_avx512(rx_queue,
				rx_pkts + retval, IAVF_VPMD_RX_MAX_BURST, offload);
		retval += burst;
		nb_pkts -= burst;
		if (burst < IAVF_VPMD_RX_MAX_BURST)
			return retval;
	}
	return retval + iavf_recv_scattered_burst_vec_avx512(rx_queue,
				rx_pkts + retval, nb_pkts, offload);
}

uint16_t
iavf_recv_scattered_pkts_vec_avx512(void *rx_queue, struct rte_mbuf **rx_pkts,
				    uint16_t nb_pkts)
{
	return iavf_recv_scattered_pkts_vec_avx512_cmn(rx_queue, rx_pkts,
						       nb_pkts, false);
}

/**
 * vPMD receive routine that reassembles single burst of
 * 32 scattered packets for flex RxD
 * Notice:
 * - nb_pkts < IAVF_DESCS_PER_LOOP, just return no packet
 */
static __rte_always_inline uint16_t
iavf_recv_scattered_burst_vec_avx512_flex_rxd(void *rx_queue,
					      struct rte_mbuf **rx_pkts,
					      uint16_t nb_pkts,
					      bool offload)
{
	struct iavf_rx_queue *rxq = rx_queue;
	uint8_t split_flags[IAVF_VPMD_RX_MAX_BURST] = {0};

	/* get some new buffers */
	uint16_t nb_bufs = _iavf_recv_raw_pkts_vec_avx512_flex_rxd(rxq,
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
	return i + reassemble_packets(rxq, &rx_pkts[i], nb_bufs - i,
				      &split_flags[i]);
}

/**
 * vPMD receive routine that reassembles scattered packets for flex RxD.
 * Main receive routine that can handle arbitrary burst sizes
 * Notice:
 * - nb_pkts < IAVF_DESCS_PER_LOOP, just return no packet
 */
static __rte_always_inline uint16_t
iavf_recv_scattered_pkts_vec_avx512_flex_rxd_cmn(void *rx_queue,
						 struct rte_mbuf **rx_pkts,
						 uint16_t nb_pkts,
						 bool offload)
{
	uint16_t retval = 0;

	while (nb_pkts > IAVF_VPMD_RX_MAX_BURST) {
		uint16_t burst =
			iavf_recv_scattered_burst_vec_avx512_flex_rxd
				(rx_queue, rx_pkts + retval,
				 IAVF_VPMD_RX_MAX_BURST, offload);
		retval += burst;
		nb_pkts -= burst;
		if (burst < IAVF_VPMD_RX_MAX_BURST)
			return retval;
	}
	return retval + iavf_recv_scattered_burst_vec_avx512_flex_rxd(rx_queue,
				rx_pkts + retval, nb_pkts, offload);
}

uint16_t
iavf_recv_scattered_pkts_vec_avx512_flex_rxd(void *rx_queue,
					     struct rte_mbuf **rx_pkts,
					     uint16_t nb_pkts)
{
	return iavf_recv_scattered_pkts_vec_avx512_flex_rxd_cmn(rx_queue,
								rx_pkts,
								nb_pkts,
								false);
}

uint16_t
iavf_recv_pkts_vec_avx512_offload(void *rx_queue, struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts)
{
	return _iavf_recv_raw_pkts_vec_avx512(rx_queue, rx_pkts,
					      nb_pkts, NULL, true);
}

uint16_t
iavf_recv_scattered_pkts_vec_avx512_offload(void *rx_queue,
					    struct rte_mbuf **rx_pkts,
					    uint16_t nb_pkts)
{
	return iavf_recv_scattered_pkts_vec_avx512_cmn(rx_queue, rx_pkts,
						       nb_pkts, true);
}

uint16_t
iavf_recv_pkts_vec_avx512_flex_rxd_offload(void *rx_queue,
					   struct rte_mbuf **rx_pkts,
					   uint16_t nb_pkts)
{
	return _iavf_recv_raw_pkts_vec_avx512_flex_rxd(rx_queue,
						       rx_pkts,
						       nb_pkts,
						       NULL,
						       true);
}

uint16_t
iavf_recv_scattered_pkts_vec_avx512_flex_rxd_offload(void *rx_queue,
						     struct rte_mbuf **rx_pkts,
						     uint16_t nb_pkts)
{
	return iavf_recv_scattered_pkts_vec_avx512_flex_rxd_cmn(rx_queue,
								rx_pkts,
								nb_pkts,
								true);
}

static __rte_always_inline int
iavf_tx_free_bufs_avx512(struct iavf_tx_queue *txq)
{
	struct iavf_tx_vec_entry *txep;
	uint32_t n;
	uint32_t i;
	int nb_free = 0;
	struct rte_mbuf *m, *free[IAVF_VPMD_TX_MAX_FREE_BUF];

	/* check DD bits on threshold descriptor */
	if ((txq->tx_ring[txq->next_dd].cmd_type_offset_bsz &
	     rte_cpu_to_le_64(IAVF_TXD_QW1_DTYPE_MASK)) !=
	    rte_cpu_to_le_64(IAVF_TX_DESC_DTYPE_DESC_DONE))
		return 0;

	n = txq->rs_thresh >> txq->use_ctx;

	 /* first buffer to free from S/W ring is at index
	  * tx_next_dd - (tx_rs_thresh-1)
	  */
	txep = (void *)txq->sw_ring;
	txep += (txq->next_dd >> txq->use_ctx) - (n - 1);

	if (txq->offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE && (n & 31) == 0) {
		struct rte_mempool *mp = txep[0].mbuf->pool;
		struct rte_mempool_cache *cache = rte_mempool_default_cache(mp,
								rte_lcore_id());
		void **cache_objs;

		if (!cache || cache->len == 0)
			goto normal;

		cache_objs = &cache->objs[cache->len];

		if (n > RTE_MEMPOOL_CACHE_MAX_SIZE) {
			rte_mempool_ops_enqueue_bulk(mp, (void *)txep, n);
			goto done;
		}

		/* The cache follows the following algorithm
		 *   1. Add the objects to the cache
		 *   2. Anything greater than the cache min value (if it crosses the
		 *   cache flush threshold) is flushed to the ring.
		 */
		/* Add elements back into the cache */
		uint32_t copied = 0;
		/* n is multiple of 32 */
		while (copied < n) {
			const __m512i a = _mm512_loadu_si512(&txep[copied]);
			const __m512i b = _mm512_loadu_si512(&txep[copied + 8]);
			const __m512i c = _mm512_loadu_si512(&txep[copied + 16]);
			const __m512i d = _mm512_loadu_si512(&txep[copied + 24]);

			_mm512_storeu_si512(&cache_objs[copied], a);
			_mm512_storeu_si512(&cache_objs[copied + 8], b);
			_mm512_storeu_si512(&cache_objs[copied + 16], c);
			_mm512_storeu_si512(&cache_objs[copied + 24], d);
			copied += 32;
		}
		cache->len += n;

		if (cache->len >= cache->flushthresh) {
			rte_mempool_ops_enqueue_bulk(mp,
						     &cache->objs[cache->size],
						     cache->len - cache->size);
			cache->len = cache->size;
		}
		goto done;
	}

normal:
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

done:
	/* buffers were freed, update counters */
	txq->nb_free = (uint16_t)(txq->nb_free + txq->rs_thresh);
	txq->next_dd = (uint16_t)(txq->next_dd + txq->rs_thresh);
	if (txq->next_dd >= txq->nb_tx_desc)
		txq->next_dd = (uint16_t)(txq->rs_thresh - 1);

	return txq->rs_thresh;
}

static __rte_always_inline void
tx_backlog_entry_avx512(struct iavf_tx_vec_entry *txep,
			struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i;

	for (i = 0; i < (int)nb_pkts; ++i)
		txep[i].mbuf = tx_pkts[i];
}

static __rte_always_inline void
iavf_vtx1(volatile struct iavf_tx_desc *txdp,
	  struct rte_mbuf *pkt, uint64_t flags,
	  bool offload)
{
	uint64_t high_qw =
		(IAVF_TX_DESC_DTYPE_DATA |
		 ((uint64_t)flags  << IAVF_TXD_QW1_CMD_SHIFT) |
		 ((uint64_t)pkt->data_len << IAVF_TXD_QW1_TX_BUF_SZ_SHIFT));
	if (offload)
		iavf_txd_enable_offload(pkt, &high_qw);

	__m128i descriptor = _mm_set_epi64x(high_qw,
					    pkt->buf_iova + pkt->data_off);
	_mm_storeu_si128((__m128i *)txdp, descriptor);
}

#define IAVF_TX_LEN_MASK 0xAA
#define IAVF_TX_OFF_MASK 0x55
static __rte_always_inline void
iavf_vtx(volatile struct iavf_tx_desc *txdp,
		struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags,
		bool offload)
{
	const uint64_t hi_qw_tmpl = (IAVF_TX_DESC_DTYPE_DATA |
			((uint64_t)flags  << IAVF_TXD_QW1_CMD_SHIFT));

	/* if unaligned on 32-bit boundary, do one to align */
	if (((uintptr_t)txdp & 0x1F) != 0 && nb_pkts != 0) {
		iavf_vtx1(txdp, *pkt, flags, offload);
		nb_pkts--, txdp++, pkt++;
	}

	/* do 4 at a time while possible, in bursts */
	for (; nb_pkts > 3; txdp += 4, pkt += 4, nb_pkts -= 4) {
		uint64_t hi_qw3 =
			hi_qw_tmpl |
			((uint64_t)pkt[3]->data_len <<
			 IAVF_TXD_QW1_TX_BUF_SZ_SHIFT);
		uint64_t hi_qw2 =
			hi_qw_tmpl |
			((uint64_t)pkt[2]->data_len <<
			 IAVF_TXD_QW1_TX_BUF_SZ_SHIFT);
		uint64_t hi_qw1 =
			hi_qw_tmpl |
			((uint64_t)pkt[1]->data_len <<
			 IAVF_TXD_QW1_TX_BUF_SZ_SHIFT);
		uint64_t hi_qw0 =
			hi_qw_tmpl |
			((uint64_t)pkt[0]->data_len <<
			 IAVF_TXD_QW1_TX_BUF_SZ_SHIFT);
		if (offload) {
			iavf_txd_enable_offload(pkt[3], &hi_qw3);
			iavf_txd_enable_offload(pkt[2], &hi_qw2);
			iavf_txd_enable_offload(pkt[1], &hi_qw1);
			iavf_txd_enable_offload(pkt[0], &hi_qw0);
		}

		__m512i desc0_3 =
			_mm512_set_epi64
				(hi_qw3,
				 pkt[3]->buf_iova + pkt[3]->data_off,
				 hi_qw2,
				 pkt[2]->buf_iova + pkt[2]->data_off,
				 hi_qw1,
				 pkt[1]->buf_iova + pkt[1]->data_off,
				 hi_qw0,
				 pkt[0]->buf_iova + pkt[0]->data_off);
		_mm512_storeu_si512((void *)txdp, desc0_3);
	}

	/* do any last ones */
	while (nb_pkts) {
		iavf_vtx1(txdp, *pkt, flags, offload);
		txdp++, pkt++, nb_pkts--;
	}
}

static __rte_always_inline void
iavf_fill_ctx_desc_tunneling_avx512(uint64_t *low_ctx_qw, struct rte_mbuf *pkt)
{
	if (pkt->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
		uint64_t eip_typ = IAVF_TX_CTX_DESC_EIPT_NONE;
		uint64_t eip_len = 0;
		uint64_t eip_noinc = 0;
		/* Default - IP_ID is increment in each segment of LSO */

		switch (pkt->ol_flags & (RTE_MBUF_F_TX_OUTER_IPV4 |
				RTE_MBUF_F_TX_OUTER_IPV6 |
				RTE_MBUF_F_TX_OUTER_IP_CKSUM)) {
		case RTE_MBUF_F_TX_OUTER_IPV4:
			eip_typ = IAVF_TX_CTX_DESC_EIPT_IPV4_NO_CHECKSUM_OFFLOAD;
			eip_len = pkt->outer_l3_len >> 2;
		break;
		case RTE_MBUF_F_TX_OUTER_IPV4 | RTE_MBUF_F_TX_OUTER_IP_CKSUM:
			eip_typ = IAVF_TX_CTX_DESC_EIPT_IPV4_CHECKSUM_OFFLOAD;
			eip_len = pkt->outer_l3_len >> 2;
		break;
		case RTE_MBUF_F_TX_OUTER_IPV6:
			eip_typ = IAVF_TX_CTX_DESC_EIPT_IPV6;
			eip_len = pkt->outer_l3_len >> 2;
		break;
		}

		/* L4TUNT: L4 Tunneling Type */
		switch (pkt->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
		case RTE_MBUF_F_TX_TUNNEL_IPIP:
			/* for non UDP / GRE tunneling, set to 00b */
			break;
		case RTE_MBUF_F_TX_TUNNEL_VXLAN:
		case RTE_MBUF_F_TX_TUNNEL_VXLAN_GPE:
		case RTE_MBUF_F_TX_TUNNEL_GTP:
		case RTE_MBUF_F_TX_TUNNEL_GENEVE:
			eip_typ |= IAVF_TXD_CTX_UDP_TUNNELING;
			break;
		case RTE_MBUF_F_TX_TUNNEL_GRE:
			eip_typ |= IAVF_TXD_CTX_GRE_TUNNELING;
			break;
		default:
			PMD_TX_LOG(ERR, "Tunnel type not supported");
			return;
		}

		/* L4TUNLEN: L4 Tunneling Length, in Words
		 *
		 * We depend on app to set rte_mbuf.l2_len correctly.
		 * For IP in GRE it should be set to the length of the GRE
		 * header;
		 * For MAC in GRE or MAC in UDP it should be set to the length
		 * of the GRE or UDP headers plus the inner MAC up to including
		 * its last Ethertype.
		 * If MPLS labels exists, it should include them as well.
		 */
		eip_typ |= (pkt->l2_len >> 1) << IAVF_TXD_CTX_QW0_NATLEN_SHIFT;

		/**
		 * Calculate the tunneling UDP checksum.
		 * Shall be set only if L4TUNT = 01b and EIPT is not zero
		 */
		if ((eip_typ & (IAVF_TX_CTX_EXT_IP_IPV4 |
					IAVF_TX_CTX_EXT_IP_IPV6 |
					IAVF_TX_CTX_EXT_IP_IPV4_NO_CSUM)) &&
				(eip_typ & IAVF_TXD_CTX_UDP_TUNNELING) &&
				(pkt->ol_flags & RTE_MBUF_F_TX_OUTER_UDP_CKSUM))
			eip_typ |= IAVF_TXD_CTX_QW0_L4T_CS_MASK;

		*low_ctx_qw = eip_typ << IAVF_TXD_CTX_QW0_TUN_PARAMS_EIPT_SHIFT |
			eip_len << IAVF_TXD_CTX_QW0_TUN_PARAMS_EIPLEN_SHIFT |
			eip_noinc << IAVF_TXD_CTX_QW0_TUN_PARAMS_EIP_NOINC_SHIFT;

	} else {
		*low_ctx_qw = 0;
	}
}

static inline void
iavf_fill_ctx_desc_tunnelling_field(volatile uint64_t *qw0,
		const struct rte_mbuf *m)
{
	uint64_t eip_typ = IAVF_TX_CTX_DESC_EIPT_NONE;
	uint64_t eip_len = 0;
	uint64_t eip_noinc = 0;
	/* Default - IP_ID is increment in each segment of LSO */

	switch (m->ol_flags & (RTE_MBUF_F_TX_OUTER_IPV4 |
			RTE_MBUF_F_TX_OUTER_IPV6 |
			RTE_MBUF_F_TX_OUTER_IP_CKSUM)) {
	case RTE_MBUF_F_TX_OUTER_IPV4:
		eip_typ = IAVF_TX_CTX_DESC_EIPT_IPV4_NO_CHECKSUM_OFFLOAD;
		eip_len = m->outer_l3_len >> 2;
	break;
	case RTE_MBUF_F_TX_OUTER_IPV4 | RTE_MBUF_F_TX_OUTER_IP_CKSUM:
		eip_typ = IAVF_TX_CTX_DESC_EIPT_IPV4_CHECKSUM_OFFLOAD;
		eip_len = m->outer_l3_len >> 2;
	break;
	case RTE_MBUF_F_TX_OUTER_IPV6:
		eip_typ = IAVF_TX_CTX_DESC_EIPT_IPV6;
		eip_len = m->outer_l3_len >> 2;
	break;
	}

	/* L4TUNT: L4 Tunneling Type */
	switch (m->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
	case RTE_MBUF_F_TX_TUNNEL_IPIP:
		/* for non UDP / GRE tunneling, set to 00b */
		break;
	case RTE_MBUF_F_TX_TUNNEL_VXLAN:
	case RTE_MBUF_F_TX_TUNNEL_VXLAN_GPE:
	case RTE_MBUF_F_TX_TUNNEL_GTP:
	case RTE_MBUF_F_TX_TUNNEL_GENEVE:
		eip_typ |= IAVF_TXD_CTX_UDP_TUNNELING;
		break;
	case RTE_MBUF_F_TX_TUNNEL_GRE:
		eip_typ |= IAVF_TXD_CTX_GRE_TUNNELING;
		break;
	default:
		PMD_TX_LOG(ERR, "Tunnel type not supported");
		return;
	}

	/* L4TUNLEN: L4 Tunneling Length, in Words
	 *
	 * We depend on app to set rte_mbuf.l2_len correctly.
	 * For IP in GRE it should be set to the length of the GRE
	 * header;
	 * For MAC in GRE or MAC in UDP it should be set to the length
	 * of the GRE or UDP headers plus the inner MAC up to including
	 * its last Ethertype.
	 * If MPLS labels exists, it should include them as well.
	 */
	eip_typ |= (m->l2_len >> 1) << IAVF_TXD_CTX_QW0_NATLEN_SHIFT;

	/**
	 * Calculate the tunneling UDP checksum.
	 * Shall be set only if L4TUNT = 01b and EIPT is not zero
	 */
	if ((eip_typ & (IAVF_TX_CTX_EXT_IP_IPV6 |
				IAVF_TX_CTX_EXT_IP_IPV4 |
				IAVF_TX_CTX_EXT_IP_IPV4_NO_CSUM)) &&
			(eip_typ & IAVF_TXD_CTX_UDP_TUNNELING) &&
			(m->ol_flags & RTE_MBUF_F_TX_OUTER_UDP_CKSUM))
		eip_typ |= IAVF_TXD_CTX_QW0_L4T_CS_MASK;

	*qw0 = eip_typ << IAVF_TXD_CTX_QW0_TUN_PARAMS_EIPT_SHIFT |
		eip_len << IAVF_TXD_CTX_QW0_TUN_PARAMS_EIPLEN_SHIFT |
		eip_noinc << IAVF_TXD_CTX_QW0_TUN_PARAMS_EIP_NOINC_SHIFT;
}

static __rte_always_inline void
ctx_vtx1(volatile struct iavf_tx_desc *txdp, struct rte_mbuf *pkt,
		uint64_t flags, bool offload, uint8_t vlan_flag)
{
	uint64_t high_ctx_qw = IAVF_TX_DESC_DTYPE_CONTEXT;
	uint64_t low_ctx_qw = 0;

	if (((pkt->ol_flags & RTE_MBUF_F_TX_VLAN) || offload)) {
		if (offload)
			iavf_fill_ctx_desc_tunneling_avx512(&low_ctx_qw, pkt);
		if ((pkt->ol_flags & RTE_MBUF_F_TX_VLAN) ||
				(vlan_flag & IAVF_TX_FLAGS_VLAN_TAG_LOC_L2TAG2)) {
			high_ctx_qw |= IAVF_TX_CTX_DESC_IL2TAG2 << IAVF_TXD_CTX_QW1_CMD_SHIFT;
			low_ctx_qw |= (uint64_t)pkt->vlan_tci << IAVF_TXD_CTX_QW0_L2TAG2_PARAM;
		}
	}
	uint64_t high_data_qw = (IAVF_TX_DESC_DTYPE_DATA |
				((uint64_t)flags  << IAVF_TXD_QW1_CMD_SHIFT) |
				((uint64_t)pkt->data_len << IAVF_TXD_QW1_TX_BUF_SZ_SHIFT));
	if (offload)
		iavf_txd_enable_offload(pkt, &high_data_qw);

	__m256i ctx_data_desc = _mm256_set_epi64x(high_data_qw, pkt->buf_iova + pkt->data_off,
							high_ctx_qw, low_ctx_qw);

	_mm256_storeu_si256((__m256i *)txdp, ctx_data_desc);
}

static __rte_always_inline void
ctx_vtx(volatile struct iavf_tx_desc *txdp,
		struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags,
		bool offload, uint8_t vlan_flag)
{
	uint64_t hi_data_qw_tmpl = (IAVF_TX_DESC_DTYPE_DATA |
					((uint64_t)flags  << IAVF_TXD_QW1_CMD_SHIFT));

	/* if unaligned on 32-bit boundary, do one to align */
	if (((uintptr_t)txdp & 0x1F) != 0 && nb_pkts != 0) {
		ctx_vtx1(txdp, *pkt, flags, offload, vlan_flag);
		nb_pkts--, txdp++, pkt++;
	}

	for (; nb_pkts > 1; txdp += 4, pkt += 2, nb_pkts -= 2) {
		uint64_t hi_ctx_qw1 = IAVF_TX_DESC_DTYPE_CONTEXT;
		uint64_t hi_ctx_qw0 = IAVF_TX_DESC_DTYPE_CONTEXT;
		uint64_t low_ctx_qw1 = 0;
		uint64_t low_ctx_qw0 = 0;
		uint64_t hi_data_qw1 = 0;
		uint64_t hi_data_qw0 = 0;

		hi_data_qw1 = hi_data_qw_tmpl |
				((uint64_t)pkt[1]->data_len <<
					IAVF_TXD_QW1_TX_BUF_SZ_SHIFT);
		hi_data_qw0 = hi_data_qw_tmpl |
				((uint64_t)pkt[0]->data_len <<
					IAVF_TXD_QW1_TX_BUF_SZ_SHIFT);

		if (pkt[1]->ol_flags & RTE_MBUF_F_TX_VLAN) {
			if (vlan_flag & IAVF_TX_FLAGS_VLAN_TAG_LOC_L2TAG2) {
				hi_ctx_qw1 |=
					IAVF_TX_CTX_DESC_IL2TAG2 << IAVF_TXD_CTX_QW1_CMD_SHIFT;
				low_ctx_qw1 |=
					(uint64_t)pkt[1]->vlan_tci << IAVF_TXD_CTX_QW0_L2TAG2_PARAM;
			} else {
				hi_data_qw1 |=
					(uint64_t)pkt[1]->vlan_tci << IAVF_TXD_QW1_L2TAG1_SHIFT;
			}
		}

		if (pkt[0]->ol_flags & RTE_MBUF_F_TX_VLAN) {
			if (vlan_flag & IAVF_TX_FLAGS_VLAN_TAG_LOC_L2TAG2) {
				hi_ctx_qw0 |=
					IAVF_TX_CTX_DESC_IL2TAG2 << IAVF_TXD_CTX_QW1_CMD_SHIFT;
				low_ctx_qw0 |=
					(uint64_t)pkt[0]->vlan_tci << IAVF_TXD_CTX_QW0_L2TAG2_PARAM;
			} else {
				hi_data_qw0 |=
					(uint64_t)pkt[0]->vlan_tci << IAVF_TXD_QW1_L2TAG1_SHIFT;
			}
		}

		if (offload) {
			iavf_txd_enable_offload(pkt[1], &hi_data_qw1);
			iavf_txd_enable_offload(pkt[0], &hi_data_qw0);
			iavf_fill_ctx_desc_tunnelling_field(&low_ctx_qw1, pkt[1]);
			iavf_fill_ctx_desc_tunnelling_field(&low_ctx_qw0, pkt[0]);
		}

		__m512i desc0_3 =
				_mm512_set_epi64
						(hi_data_qw1, pkt[1]->buf_iova + pkt[1]->data_off,
						hi_ctx_qw1, low_ctx_qw1,
						hi_data_qw0, pkt[0]->buf_iova + pkt[0]->data_off,
						hi_ctx_qw0, low_ctx_qw0);
		_mm512_storeu_si512((void *)txdp, desc0_3);
	}

	if (nb_pkts)
		ctx_vtx1(txdp, *pkt, flags, offload, vlan_flag);
}

static __rte_always_inline uint16_t
iavf_xmit_fixed_burst_vec_avx512(void *tx_queue, struct rte_mbuf **tx_pkts,
				 uint16_t nb_pkts, bool offload)
{
	struct iavf_tx_queue *txq = (struct iavf_tx_queue *)tx_queue;
	volatile struct iavf_tx_desc *txdp;
	struct iavf_tx_vec_entry *txep;
	uint16_t n, nb_commit, tx_id;
	/* bit2 is reserved and must be set to 1 according to Spec */
	uint64_t flags = IAVF_TX_DESC_CMD_EOP | IAVF_TX_DESC_CMD_ICRC;
	uint64_t rs = IAVF_TX_DESC_CMD_RS | flags;

	if (txq->nb_free < txq->free_thresh)
		iavf_tx_free_bufs_avx512(txq);

	nb_commit = nb_pkts = (uint16_t)RTE_MIN(txq->nb_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = (void *)txq->sw_ring;
	txep += tx_id;

	txq->nb_free = (uint16_t)(txq->nb_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {
		tx_backlog_entry_avx512(txep, tx_pkts, n);

		iavf_vtx(txdp, tx_pkts, n - 1, flags, offload);
		tx_pkts += (n - 1);
		txdp += (n - 1);

		iavf_vtx1(txdp, *tx_pkts++, rs, offload);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->next_rs = (uint16_t)(txq->rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &txq->tx_ring[tx_id];
		txep = (void *)txq->sw_ring;
		txep += tx_id;
	}

	tx_backlog_entry_avx512(txep, tx_pkts, nb_commit);

	iavf_vtx(txdp, tx_pkts, nb_commit, flags, offload);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->next_rs) {
		txq->tx_ring[txq->next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)IAVF_TX_DESC_CMD_RS) <<
					 IAVF_TXD_QW1_CMD_SHIFT);
		txq->next_rs =
			(uint16_t)(txq->next_rs + txq->rs_thresh);
	}

	txq->tx_tail = tx_id;

	IAVF_PCI_REG_WC_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

static __rte_always_inline uint16_t
iavf_xmit_fixed_burst_vec_avx512_ctx(void *tx_queue, struct rte_mbuf **tx_pkts,
				 uint16_t nb_pkts, bool offload)
{
	struct iavf_tx_queue *txq = (struct iavf_tx_queue *)tx_queue;
	volatile struct iavf_tx_desc *txdp;
	struct iavf_tx_vec_entry *txep;
	uint16_t n, nb_commit, nb_mbuf, tx_id;
	/* bit2 is reserved and must be set to 1 according to Spec */
	uint64_t flags = IAVF_TX_DESC_CMD_EOP | IAVF_TX_DESC_CMD_ICRC;
	uint64_t rs = IAVF_TX_DESC_CMD_RS | flags;

	if (txq->nb_free < txq->free_thresh)
		iavf_tx_free_bufs_avx512(txq);

	nb_commit = (uint16_t)RTE_MIN(txq->nb_free, nb_pkts << 1);
	nb_commit &= 0xFFFE;
	if (unlikely(nb_commit == 0))
		return 0;

	nb_pkts = nb_commit >> 1;
	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = (void *)txq->sw_ring;
	txep += (tx_id >> 1);

	txq->nb_free = (uint16_t)(txq->nb_free - nb_commit);
	n = (uint16_t)(txq->nb_tx_desc - tx_id);

	if (n != 0 && nb_commit >= n) {
		nb_mbuf = n >> 1;
		tx_backlog_entry_avx512(txep, tx_pkts, nb_mbuf);

		ctx_vtx(txdp, tx_pkts, nb_mbuf - 1, flags, offload, txq->vlan_flag);
		tx_pkts += (nb_mbuf - 1);
		txdp += (n - 2);
		ctx_vtx1(txdp, *tx_pkts++, rs, offload, txq->vlan_flag);

		nb_commit = (uint16_t)(nb_commit - n);

		txq->next_rs = (uint16_t)(txq->rs_thresh - 1);
		tx_id = 0;
		/* avoid reach the end of ring */
		txdp = txq->tx_ring;
		txep = (void *)txq->sw_ring;
	}

	nb_mbuf = nb_commit >> 1;
	tx_backlog_entry_avx512(txep, tx_pkts, nb_mbuf);

	ctx_vtx(txdp, tx_pkts, nb_mbuf, flags, offload, txq->vlan_flag);
	tx_id = (uint16_t)(tx_id + nb_commit);

	if (tx_id > txq->next_rs) {
		txq->tx_ring[txq->next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)IAVF_TX_DESC_CMD_RS) <<
					 IAVF_TXD_QW1_CMD_SHIFT);
		txq->next_rs =
			(uint16_t)(txq->next_rs + txq->rs_thresh);
	}

	txq->tx_tail = tx_id;

	IAVF_PCI_REG_WC_WRITE(txq->qtx_tail, txq->tx_tail);
	return nb_pkts;
}

static __rte_always_inline uint16_t
iavf_xmit_pkts_vec_avx512_cmn(void *tx_queue, struct rte_mbuf **tx_pkts,
			      uint16_t nb_pkts, bool offload)
{
	uint16_t nb_tx = 0;
	struct iavf_tx_queue *txq = (struct iavf_tx_queue *)tx_queue;

	while (nb_pkts) {
		uint16_t ret, num;

		/* cross rs_thresh boundary is not allowed */
		num = (uint16_t)RTE_MIN(nb_pkts, txq->rs_thresh);
		ret = iavf_xmit_fixed_burst_vec_avx512(tx_queue, &tx_pkts[nb_tx],
						       num, offload);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}

uint16_t
iavf_xmit_pkts_vec_avx512(void *tx_queue, struct rte_mbuf **tx_pkts,
			  uint16_t nb_pkts)
{
	return iavf_xmit_pkts_vec_avx512_cmn(tx_queue, tx_pkts, nb_pkts, false);
}

void __rte_cold
iavf_tx_queue_release_mbufs_avx512(struct iavf_tx_queue *txq)
{
	unsigned int i;
	const uint16_t max_desc = (uint16_t)(txq->nb_tx_desc - 1);
	struct iavf_tx_vec_entry *swr = (void *)txq->sw_ring;

	if (!txq->sw_ring || txq->nb_free == max_desc)
		return;

	i = (txq->next_dd >> txq->use_ctx) + 1 -
			(txq->rs_thresh >> txq->use_ctx);

	if (txq->tx_tail < i) {
		for (; i < (unsigned int)(txq->nb_tx_desc >> txq->use_ctx); i++) {
			rte_pktmbuf_free_seg(swr[i].mbuf);
			swr[i].mbuf = NULL;
		}
		i = 0;
	}
}

int __rte_cold
iavf_txq_vec_setup_avx512(struct iavf_tx_queue *txq)
{
	txq->rel_mbufs_type = IAVF_REL_MBUFS_AVX512_VEC;
	return 0;
}

uint16_t
iavf_xmit_pkts_vec_avx512_offload(void *tx_queue, struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts)
{
	return iavf_xmit_pkts_vec_avx512_cmn(tx_queue, tx_pkts, nb_pkts, true);
}

static __rte_always_inline uint16_t
iavf_xmit_pkts_vec_avx512_ctx_cmn(void *tx_queue, struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts, bool offload)
{
	uint16_t nb_tx = 0;
	struct iavf_tx_queue *txq = (struct iavf_tx_queue *)tx_queue;

	while (nb_pkts) {
		uint16_t ret, num;

		/* cross rs_thresh boundary is not allowed */
		num = (uint16_t)RTE_MIN(nb_pkts << 1, txq->rs_thresh);
		num = num >> 1;
		ret = iavf_xmit_fixed_burst_vec_avx512_ctx(tx_queue, &tx_pkts[nb_tx],
						       num, offload);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}

uint16_t
iavf_xmit_pkts_vec_avx512_ctx_offload(void *tx_queue, struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts)
{
	return iavf_xmit_pkts_vec_avx512_ctx_cmn(tx_queue, tx_pkts, nb_pkts, true);
}
