/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <stdint.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "base/i40e_prototype.h"
#include "base/i40e_type.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"
#include "i40e_rxtx_vec_common.h"
#include "i40e_rxtx_common_avx.h"

#include <rte_vect.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#define RTE_I40E_DESCS_PER_LOOP_AVX 8

static __rte_always_inline void
i40e_rxq_rearm(struct i40e_rx_queue *rxq)
{
	return i40e_rxq_rearm_common(rxq, true);
}

#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
/* Handles 32B descriptor FDIR ID processing:
 * rxdp: receive descriptor ring, required to load 2nd 16B half of each desc
 * rx_pkts: required to store metadata back to mbufs
 * pkt_idx: offset into the burst, increments in vector widths
 * desc_idx: required to select the correct shift at compile time
 */
static inline __m256i
desc_fdir_processing_32b(volatile union i40e_rx_desc *rxdp,
			 struct rte_mbuf **rx_pkts,
			 const uint32_t pkt_idx,
			 const uint32_t desc_idx)
{
	/* 32B desc path: load rxdp.wb.qword2 for EXT_STATUS and FLEXBH_STAT */
	__m128i *rxdp_desc_0 = (void *)(&rxdp[desc_idx + 0].wb.qword2);
	__m128i *rxdp_desc_1 = (void *)(&rxdp[desc_idx + 1].wb.qword2);
	const __m128i desc_qw2_0 = _mm_load_si128(rxdp_desc_0);
	const __m128i desc_qw2_1 = _mm_load_si128(rxdp_desc_1);

	/* Mask for FLEXBH_STAT, and the FDIR_ID value to compare against. The
	 * remaining data is set to all 1's to pass through data.
	 */
	const __m256i flexbh_mask = _mm256_set_epi32(-1, -1, -1, 3 << 4,
						     -1, -1, -1, 3 << 4);
	const __m256i flexbh_id   = _mm256_set_epi32(-1, -1, -1, 1 << 4,
						     -1, -1, -1, 1 << 4);

	/* Load descriptor, check for FLEXBH bits, generate a mask for both
	 * packets in the register.
	 */
	__m256i desc_qw2_0_1 =
		_mm256_inserti128_si256(_mm256_castsi128_si256(desc_qw2_0),
					desc_qw2_1, 1);
	__m256i desc_tmp_msk = _mm256_and_si256(flexbh_mask, desc_qw2_0_1);
	__m256i fdir_mask = _mm256_cmpeq_epi32(flexbh_id, desc_tmp_msk);
	__m256i fdir_data = _mm256_alignr_epi8(desc_qw2_0_1, desc_qw2_0_1, 12);
	__m256i desc_fdir_data = _mm256_and_si256(fdir_mask, fdir_data);

	/* Write data out to the mbuf. There is no store to this area of the
	 * mbuf today, so we cannot combine it with another store.
	 */
	const uint32_t idx_0 = pkt_idx + desc_idx;
	const uint32_t idx_1 = pkt_idx + desc_idx + 1;

	rx_pkts[idx_0]->hash.fdir.hi = _mm256_extract_epi32(desc_fdir_data, 0);
	rx_pkts[idx_1]->hash.fdir.hi = _mm256_extract_epi32(desc_fdir_data, 4);

	/* Create mbuf flags as required for mbuf_flags layout
	 *  (That's high lane [1,3,5,7, 0,2,4,6] as u32 lanes).
	 * Approach:
	 * - Mask away bits not required from the fdir_mask
	 * - Leave the PKT_FDIR_ID bit (1 << 13)
	 * - Position that bit correctly based on packet number
	 * - OR in the resulting bit to mbuf_flags
	 */
	RTE_BUILD_BUG_ON(RTE_MBUF_F_RX_FDIR_ID != (1 << 13));
	__m256i mbuf_flag_mask = _mm256_set_epi32(0, 0, 0, 1 << 13,
						  0, 0, 0, 1 << 13);
	__m256i desc_flag_bit =  _mm256_and_si256(mbuf_flag_mask, fdir_mask);

	/* For static-inline function, this will be stripped out
	 * as the desc_idx is a hard-coded constant.
	 */
	switch (desc_idx) {
	case 0:
		return _mm256_alignr_epi8(desc_flag_bit, desc_flag_bit,  4);
	case 2:
		return _mm256_alignr_epi8(desc_flag_bit, desc_flag_bit,  8);
	case 4:
		return _mm256_alignr_epi8(desc_flag_bit, desc_flag_bit, 12);
	case 6:
		return desc_flag_bit;
	default:
		break;
	}

	/* NOT REACHED, see above switch returns */
	return _mm256_setzero_si256();
}
#endif /* RTE_LIBRTE_I40E_16BYTE_RX_DESC */

#define PKTLEN_SHIFT     10

/* Force inline as some compilers will not inline by default. */
static __rte_always_inline uint16_t
_recv_raw_pkts_vec_avx512(struct i40e_rx_queue *rxq, struct rte_mbuf **rx_pkts,
			  uint16_t nb_pkts, uint8_t *split_packet)
{
	const uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;
	const __m256i mbuf_init = _mm256_set_epi64x(0, 0,
			0, rxq->mbuf_initializer);
	struct i40e_rx_entry *sw_ring = &rxq->sw_ring[rxq->rx_tail];
	volatile union i40e_rx_desc *rxdp = rxq->rx_ring + rxq->rx_tail;

	rte_prefetch0(rxdp);

	/* nb_pkts has to be floor-aligned to RTE_I40E_DESCS_PER_LOOP_AVX */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, RTE_I40E_DESCS_PER_LOOP_AVX);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act
	 */
	if (rxq->rxrearm_nb > RTE_I40E_RXQ_REARM_THRESH)
		i40e_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available
	 */
	if (!(rxdp->wb.qword1.status_error_len &
			rte_cpu_to_le_32(1 << I40E_RX_DESC_STATUS_DD_SHIFT)))
		return 0;

	/* constants used in processing loop */
	const __m512i crc_adjust =
		_mm512_set4_epi32
			(0,             /* ignore non-length fields */
			 -rxq->crc_len, /* sub crc on data_len */
			 -rxq->crc_len, /* sub crc on pkt_len */
			 0              /* ignore non-length fields */
			);

	/* 8 packets DD mask, LSB in each 32-bit value */
	const __m256i dd_check = _mm256_set1_epi32(1);

	/* 8 packets EOP mask, second-LSB in each 32-bit value */
	const __m256i eop_check = _mm256_slli_epi32(dd_check,
			I40E_RX_DESC_STATUS_EOF_SHIFT);

	/* mask to shuffle from desc. to mbuf (2 descriptors)*/
	const __m512i shuf_msk =
		_mm512_set4_epi32
			(/* rss hash parsed separately */
			 /* octet 4~7, 32bits rss */
			 7 << 24 | 6 << 16 | 5 << 8 | 4,
			 /* octet 2~3, low 16 bits vlan_macip */
			 /* octet 14~15, 16 bits data_len */
			 3 << 24 | 2 << 16 | 15 << 8 | 14,
			 /* skip hi 16 bits pkt_len, zero out */
			 /* octet 14~15, 16 bits pkt_len */
			 0xFFFF << 16 | 15 << 8 | 14,
			 /* pkt_type set as unknown */
			 0xFFFFFFFF
			);
	/* compile-time check the above crc and shuffle layout is correct.
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
	/* mask everything except RSS, flow director and VLAN flags
	 * bit2 is for VLAN tag, bit11 for flow director indication
	 * bit13:12 for RSS indication. Bits 3-5 of error
	 * field (bits 22-24) are for IP/L4 checksum errors
	 */
	const __m256i flags_mask = _mm256_set1_epi32
		((1 << 2) | (1 << 11) | (3 << 12) | (7 << 22));

	/* data to be shuffled by result of flag mask. If VLAN bit is set,
	 * (bit 2), then position 4 in this array will be used in the
	 * destination
	 */
	const __m256i vlan_flags_shuf = _mm256_set_epi32
		(0, 0, RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED, 0,
		0, 0, RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED, 0);

	/* data to be shuffled by result of flag mask, shifted down 11.
	 * If RSS/FDIR bits are set, shuffle moves appropriate flags in
	 * place.
	 */
	const __m256i rss_flags_shuf = _mm256_set_epi8
		(0, 0, 0, 0, 0, 0, 0, 0,
		RTE_MBUF_F_RX_RSS_HASH | RTE_MBUF_F_RX_FDIR, RTE_MBUF_F_RX_RSS_HASH, 0, 0,
		0, 0, RTE_MBUF_F_RX_FDIR, 0, /* end up 128-bits */
		0, 0, 0, 0, 0, 0, 0, 0,
		RTE_MBUF_F_RX_RSS_HASH | RTE_MBUF_F_RX_FDIR, RTE_MBUF_F_RX_RSS_HASH, 0, 0,
		0, 0, RTE_MBUF_F_RX_FDIR, 0);

	/* data to be shuffled by the result of the flags mask shifted by 22
	 * bits.  This gives use the l3_l4 flags.
	 */
	const __m256i l3_l4_flags_shuf = _mm256_set_epi8
		(0, 0, 0, 0, 0, 0, 0, 0,
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

	const __m256i cksum_mask = _mm256_set1_epi32
		(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD |
		RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
		RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD);

	uint16_t i, received;

	for (i = 0, received = 0; i < nb_pkts;
			i += RTE_I40E_DESCS_PER_LOOP_AVX,
			rxdp += RTE_I40E_DESCS_PER_LOOP_AVX) {
		/* step 1, copy over 8 mbuf pointers to rx_pkts array */
		_mm256_storeu_si256((void *)&rx_pkts[i],
				_mm256_loadu_si256((void *)&sw_ring[i]));
#ifdef RTE_ARCH_X86_64
		_mm256_storeu_si256((void *)&rx_pkts[i + 4],
				_mm256_loadu_si256((void *)&sw_ring[i + 4]));
#endif

		__m512i raw_desc0_3, raw_desc4_7;
		__m256i raw_desc0_1, raw_desc2_3, raw_desc4_5, raw_desc6_7;

		/* load in descriptors, in reverse order */
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

		raw_desc4_7 =
			_mm512_inserti64x4
				(_mm512_castsi256_si512(raw_desc4_5),
				 raw_desc6_7, 1);
		raw_desc0_3 =
			_mm512_inserti64x4
				(_mm512_castsi256_si512(raw_desc0_1),
				 raw_desc2_3, 1);

		if (split_packet) {
			int j;

			for (j = 0; j < RTE_I40E_DESCS_PER_LOOP_AVX; j++)
				rte_mbuf_prefetch_part2(rx_pkts[i + j]);
		}

		/* convert descriptors 0-7 into mbufs, adjusting length and
		 * re-arranging fields. Then write into the mbuf
		 */
		const __m512i len4_7 = _mm512_slli_epi32
					(raw_desc4_7, PKTLEN_SHIFT);
		const __m512i len0_3 = _mm512_slli_epi32
					(raw_desc0_3, PKTLEN_SHIFT);
		const __m512i desc4_7 = _mm512_mask_blend_epi16
					(0x80808080, raw_desc4_7, len4_7);
		const __m512i desc0_3 = _mm512_mask_blend_epi16
					(0x80808080, raw_desc0_3, len0_3);
		__m512i mb4_7 = _mm512_shuffle_epi8(desc4_7, shuf_msk);
		__m512i mb0_3 = _mm512_shuffle_epi8(desc0_3, shuf_msk);

		mb4_7 = _mm512_add_epi32(mb4_7, crc_adjust);
		mb0_3 = _mm512_add_epi32(mb0_3, crc_adjust);

		/* to get packet types, shift 64-bit values down 30 bits
		 * and so ptype is in lower 8-bits in each
		 */
		const __m512i ptypes4_7 = _mm512_srli_epi64(desc4_7, 30);
		const __m512i ptypes0_3 = _mm512_srli_epi64(desc0_3, 30);
		const __m256i ptypes6_7 =
			_mm512_extracti64x4_epi64(ptypes4_7, 1);
		const __m256i ptypes4_5 =
			_mm512_extracti64x4_epi64(ptypes4_7, 0);
		const __m256i ptypes2_3 =
			_mm512_extracti64x4_epi64(ptypes0_3, 1);
		const __m256i ptypes0_1 =
			_mm512_extracti64x4_epi64(ptypes0_3, 0);
		const uint8_t ptype7 = _mm256_extract_epi8(ptypes6_7, 24);
		const uint8_t ptype6 = _mm256_extract_epi8(ptypes6_7, 8);
		const uint8_t ptype5 = _mm256_extract_epi8(ptypes4_5, 24);
		const uint8_t ptype4 = _mm256_extract_epi8(ptypes4_5, 8);
		const uint8_t ptype3 = _mm256_extract_epi8(ptypes2_3, 24);
		const uint8_t ptype2 = _mm256_extract_epi8(ptypes2_3, 8);
		const uint8_t ptype1 = _mm256_extract_epi8(ptypes0_1, 24);
		const uint8_t ptype0 = _mm256_extract_epi8(ptypes0_1, 8);

		const __m512i ptype4_7 = _mm512_set_epi32
			(0, 0, 0, ptype_tbl[ptype7],
			 0, 0, 0, ptype_tbl[ptype6],
			 0, 0, 0, ptype_tbl[ptype5],
			 0, 0, 0, ptype_tbl[ptype4]);
		const __m512i ptype0_3 = _mm512_set_epi32
			(0, 0, 0, ptype_tbl[ptype3],
			 0, 0, 0, ptype_tbl[ptype2],
			 0, 0, 0, ptype_tbl[ptype1],
			 0, 0, 0, ptype_tbl[ptype0]);

		mb4_7 = _mm512_mask_blend_epi32(0x1111, mb4_7, ptype4_7);
		mb0_3 = _mm512_mask_blend_epi32(0x1111, mb0_3, ptype0_3);

		__m256i mb4_5 = _mm512_extracti64x4_epi64(mb4_7, 0);
		__m256i mb6_7 = _mm512_extracti64x4_epi64(mb4_7, 1);
		__m256i mb0_1 = _mm512_extracti64x4_epi64(mb0_3, 0);
		__m256i mb2_3 = _mm512_extracti64x4_epi64(mb0_3, 1);

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
			(desc4_7, status_permute_msk, desc0_3);
		__m256i status0_7 = _mm512_extracti64x4_epi64
			(raw_status0_7, 0);

		/* now do flag manipulation */

		/* get only flag/error bits we want */
		const __m256i flag_bits =
			_mm256_and_si256(status0_7, flags_mask);
		/* set vlan and rss flags */
		const __m256i vlan_flags =
			_mm256_shuffle_epi8(vlan_flags_shuf, flag_bits);
		const __m256i rss_fdir_bits = _mm256_srli_epi32(flag_bits, 11);
		const __m256i rss_flags = _mm256_shuffle_epi8(rss_flags_shuf,
							      rss_fdir_bits);

		/* l3_l4_error flags, shuffle, then shift to correct adjustment
		 * of flags in flags_shuf, and finally mask out extra bits
		 */
		__m256i l3_l4_flags = _mm256_shuffle_epi8(l3_l4_flags_shuf,
				_mm256_srli_epi32(flag_bits, 22));
		l3_l4_flags = _mm256_slli_epi32(l3_l4_flags, 1);
		l3_l4_flags = _mm256_and_si256(l3_l4_flags, cksum_mask);

		/* merge flags */
		__m256i mbuf_flags = _mm256_or_si256(l3_l4_flags,
				_mm256_or_si256(rss_flags, vlan_flags));

		/* If the rxq has FDIR enabled, read and process the FDIR info
		 * from the descriptor. This can cause more loads/stores, so is
		 * not always performed. Branch over the code when not enabled.
		 */
		if (rxq->fdir_enabled) {
#ifdef RTE_LIBRTE_I40E_16BYTE_RX_DESC
			/* 16B descriptor code path:
			 * RSS and FDIR ID use the same offset in the desc, so
			 * only one can be present at a time. The code below
			 * identifies an FDIR ID match, and zeros the RSS value
			 * in the mbuf on FDIR match to keep mbuf data clean.
			 */
#define FDIR_BLEND_MASK ((1 << 3) | (1 << 7))

			/* Flags:
			 * - Take flags, shift bits to null out
			 * - CMPEQ with known FDIR ID, to get 0xFFFF or 0 mask
			 * - Strip bits from mask, leaving 0 or 1 for FDIR ID
			 * - Merge with mbuf_flags
			 */
			/* FLM = 1, FLTSTAT = 0b01, (FLM | FLTSTAT) == 3.
			 * Shift left by 28 to avoid having to mask.
			 */
			const __m256i fdir =
				_mm256_slli_epi32(rss_fdir_bits, 28);
			const __m256i fdir_id = _mm256_set1_epi32(3 << 28);

			/* As above, the fdir_mask to packet mapping is this:
			 * order (hi->lo): [1, 3, 5, 7, 0, 2, 4, 6]
			 * Then OR FDIR flags to mbuf_flags on FDIR ID hit.
			 */
			RTE_BUILD_BUG_ON(RTE_MBUF_F_RX_FDIR_ID != (1 << 13));
			const __m256i pkt_fdir_bit = _mm256_set1_epi32(1 << 13);
			const __m256i fdir_mask =
				_mm256_cmpeq_epi32(fdir, fdir_id);
			__m256i fdir_bits =
				_mm256_and_si256(fdir_mask, pkt_fdir_bit);

			mbuf_flags = _mm256_or_si256(mbuf_flags, fdir_bits);

			/* Based on FDIR_MASK, clear the RSS or FDIR value.
			 * The FDIR ID value is masked to zero if not a hit,
			 * otherwise the mb0_1 register RSS field is zeroed.
			 */
			const __m256i fdir_zero_mask = _mm256_setzero_si256();
			__m256i tmp0_1 = _mm256_blend_epi32(fdir_zero_mask,
						fdir_mask, FDIR_BLEND_MASK);
			__m256i fdir_mb0_1 = _mm256_and_si256(mb0_1, fdir_mask);

			mb0_1 = _mm256_andnot_si256(tmp0_1, mb0_1);

			/* Write to mbuf: no stores to combine with, so just a
			 * scalar store to push data here.
			 */
			rx_pkts[i + 0]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_mb0_1, 3);
			rx_pkts[i + 1]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_mb0_1, 7);

			/* Same as above, only shift the fdir_mask to align
			 * the packet FDIR mask with the FDIR_ID desc lane.
			 */
			__m256i tmp2_3 =
				_mm256_alignr_epi8(fdir_mask, fdir_mask, 12);
			__m256i fdir_mb2_3 = _mm256_and_si256(mb2_3, tmp2_3);

			tmp2_3 = _mm256_blend_epi32(fdir_zero_mask, tmp2_3,
						    FDIR_BLEND_MASK);
			mb2_3 = _mm256_andnot_si256(tmp2_3, mb2_3);
			rx_pkts[i + 2]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_mb2_3, 3);
			rx_pkts[i + 3]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_mb2_3, 7);

			__m256i tmp4_5 =
				_mm256_alignr_epi8(fdir_mask, fdir_mask, 8);
			__m256i fdir_mb4_5 = _mm256_and_si256(mb4_5, tmp4_5);

			tmp4_5 = _mm256_blend_epi32(fdir_zero_mask, tmp4_5,
						    FDIR_BLEND_MASK);
			mb4_5 = _mm256_andnot_si256(tmp4_5, mb4_5);
			rx_pkts[i + 4]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_mb4_5, 3);
			rx_pkts[i + 5]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_mb4_5, 7);

			__m256i tmp6_7 =
				_mm256_alignr_epi8(fdir_mask, fdir_mask, 4);
			__m256i fdir_mb6_7 = _mm256_and_si256(mb6_7, tmp6_7);

			tmp6_7 = _mm256_blend_epi32(fdir_zero_mask, tmp6_7,
						    FDIR_BLEND_MASK);
			mb6_7 = _mm256_andnot_si256(tmp6_7, mb6_7);
			rx_pkts[i + 6]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_mb6_7, 3);
			rx_pkts[i + 7]->hash.fdir.hi =
				_mm256_extract_epi32(fdir_mb6_7, 7);

			/* End of 16B descriptor handling */
#else
			/* 32B descriptor FDIR ID mark handling. Returns bits
			 * to be OR-ed into the mbuf olflags.
			 */
			__m256i fdir_add_flags;

			fdir_add_flags =
				desc_fdir_processing_32b(rxdp, rx_pkts, i, 0);
			mbuf_flags =
				_mm256_or_si256(mbuf_flags, fdir_add_flags);

			fdir_add_flags =
				desc_fdir_processing_32b(rxdp, rx_pkts, i, 2);
			mbuf_flags =
				_mm256_or_si256(mbuf_flags, fdir_add_flags);

			fdir_add_flags =
				desc_fdir_processing_32b(rxdp, rx_pkts, i, 4);
			mbuf_flags =
				_mm256_or_si256(mbuf_flags, fdir_add_flags);

			fdir_add_flags =
				desc_fdir_processing_32b(rxdp, rx_pkts, i, 6);
			mbuf_flags =
				_mm256_or_si256(mbuf_flags, fdir_add_flags);
			/* End 32B desc handling */
#endif /* RTE_LIBRTE_I40E_16BYTE_RX_DESC */

		} /* if() on FDIR enabled */

		/* At this point, we have the 8 sets of flags in the low 16-bits
		 * of each 32-bit value in vlan0.
		 * We want to extract these, and merge them with the mbuf init data
		 * so we can do a single write to the mbuf to set the flags
		 * and all the other initialization fields. Extracting the
		 * appropriate flags means that we have to do a shift and blend for
		 * each mbuf before we do the write. However, we can also
		 * add in the previously computed rx_descriptor fields to
		 * make a single 256-bit write per mbuf
		 */
		/* check the structure matches expectations */
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, ol_flags) !=
				offsetof(struct rte_mbuf, rearm_data) + 8);
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, rearm_data) !=
				RTE_ALIGN(offsetof(struct rte_mbuf, rearm_data), 16));
		/* build up data and do writes */
		__m256i rearm0, rearm1, rearm2, rearm3, rearm4, rearm5,
				rearm6, rearm7;
		rearm6 = _mm256_blend_epi32
			(mbuf_init, _mm256_slli_si256(mbuf_flags, 8), 0x04);
		rearm4 = _mm256_blend_epi32
			(mbuf_init, _mm256_slli_si256(mbuf_flags, 4), 0x04);
		rearm2 = _mm256_blend_epi32
			(mbuf_init, mbuf_flags, 0x04);
		rearm0 = _mm256_blend_epi32
			(mbuf_init, _mm256_srli_si256(mbuf_flags, 4), 0x04);
		/* permute to add in the rx_descriptor e.g. rss fields */
		rearm6 = _mm256_permute2f128_si256(rearm6, mb6_7, 0x20);
		rearm4 = _mm256_permute2f128_si256(rearm4, mb4_5, 0x20);
		rearm2 = _mm256_permute2f128_si256(rearm2, mb2_3, 0x20);
		rearm0 = _mm256_permute2f128_si256(rearm0, mb0_1, 0x20);
		/* write to mbuf */
		_mm256_storeu_si256
			((__m256i *)&rx_pkts[i + 6]->rearm_data, rearm6);
		_mm256_storeu_si256
			((__m256i *)&rx_pkts[i + 4]->rearm_data, rearm4);
		_mm256_storeu_si256
			((__m256i *)&rx_pkts[i + 2]->rearm_data, rearm2);
		_mm256_storeu_si256
			((__m256i *)&rx_pkts[i + 0]->rearm_data, rearm0);

		/* repeat for the odd mbufs */
		const __m256i odd_flags = _mm256_castsi128_si256
			(_mm256_extracti128_si256(mbuf_flags, 1));
		rearm7 = _mm256_blend_epi32
			(mbuf_init, _mm256_slli_si256(odd_flags, 8), 0x04);
		rearm5 = _mm256_blend_epi32
			(mbuf_init, _mm256_slli_si256(odd_flags, 4), 0x04);
		rearm3 = _mm256_blend_epi32
			(mbuf_init, odd_flags, 0x04);
		rearm1 = _mm256_blend_epi32
			(mbuf_init, _mm256_srli_si256(odd_flags, 4), 0x04);
		/* since odd mbufs are already in hi 128-bits use blend */
		rearm7 = _mm256_blend_epi32(rearm7, mb6_7, 0xF0);
		rearm5 = _mm256_blend_epi32(rearm5, mb4_5, 0xF0);
		rearm3 = _mm256_blend_epi32(rearm3, mb2_3, 0xF0);
		rearm1 = _mm256_blend_epi32(rearm1, mb0_1, 0xF0);
		/* again write to mbufs */
		_mm256_storeu_si256
			((__m256i *)&rx_pkts[i + 7]->rearm_data, rearm7);
		_mm256_storeu_si256
			((__m256i *)&rx_pkts[i + 5]->rearm_data, rearm5);
		_mm256_storeu_si256
			((__m256i *)&rx_pkts[i + 3]->rearm_data, rearm3);
		_mm256_storeu_si256
			((__m256i *)&rx_pkts[i + 1]->rearm_data, rearm1);

		/* extract and record EOP bit */
		if (split_packet) {
			const __m128i eop_mask =
				_mm_set1_epi16
				(1 << I40E_RX_DESC_STATUS_EOF_SHIFT);
			const __m256i eop_bits256 =
				_mm256_and_si256(status0_7, eop_check);
			/* pack status bits into a single 128-bit register */
			const __m128i eop_bits =
				_mm_packus_epi32
				(_mm256_castsi256_si128(eop_bits256),
				_mm256_extractf128_si256(eop_bits256, 1));
			/* flip bits, and mask out the EOP bit, which is now
			 * a split-packet bit i.e. !EOP, rather than EOP one.
			 */
			__m128i split_bits = _mm_andnot_si128(eop_bits,
					eop_mask);
			/* eop bits are out of order, so we need to shuffle them
			 * back into order again. In doing so, only use low 8
			 * bits, which acts like another pack instruction
			 * The original order is (hi->lo): 1,3,5,7,0,2,4,6
			 * [Since we use epi8, the 16-bit positions are
			 * multiplied by 2 in the eop_shuffle value.]
			 */
			__m128i eop_shuffle = _mm_set_epi8
				(0xFF, 0xFF, 0xFF, 0xFF, /* zero hi 64b */
				0xFF, 0xFF, 0xFF, 0xFF,
				8, 0, 10, 2, /* move values to lo 64b */
				12, 4, 14, 6);
			split_bits = _mm_shuffle_epi8(split_bits, eop_shuffle);
			*(uint64_t *)split_packet =
				_mm_cvtsi128_si64(split_bits);
			split_packet += RTE_I40E_DESCS_PER_LOOP_AVX;
		}

		/* perform dd_check */
		status0_7 = _mm256_and_si256(status0_7, dd_check);
		status0_7 = _mm256_packs_epi32
			(status0_7, _mm256_setzero_si256());

		uint64_t burst = rte_popcount64
				(_mm_cvtsi128_si64
					(_mm256_extracti128_si256
						(status0_7, 1)));
		burst += rte_popcount64(_mm_cvtsi128_si64
				(_mm256_castsi256_si128(status0_7)));
		received += burst;
		if (burst != RTE_I40E_DESCS_PER_LOOP_AVX)
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
 * - nb_pkts < RTE_I40E_DESCS_PER_LOOP, just return no packet
 */
uint16_t
i40e_recv_pkts_vec_avx512(void *rx_queue, struct rte_mbuf **rx_pkts,
			  uint16_t nb_pkts)
{
	return _recv_raw_pkts_vec_avx512(rx_queue, rx_pkts, nb_pkts, NULL);
}

/**
 * vPMD receive routine that reassembles single burst of 32 scattered packets
 * Notice:
 * - nb_pkts < RTE_I40E_DESCS_PER_LOOP, just return no packet
 */
static uint16_t
i40e_recv_scattered_burst_vec_avx512(void *rx_queue,
				     struct rte_mbuf **rx_pkts,
				     uint16_t nb_pkts)
{
	struct i40e_rx_queue *rxq = rx_queue;
	uint8_t split_flags[RTE_I40E_VPMD_RX_BURST] = {0};

	/* get some new buffers */
	uint16_t nb_bufs = _recv_raw_pkts_vec_avx512(rxq, rx_pkts, nb_pkts,
			split_flags);
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
 * - nb_pkts < RTE_I40E_DESCS_PER_LOOP, just return no packet
 */
uint16_t
i40e_recv_scattered_pkts_vec_avx512(void *rx_queue,
				    struct rte_mbuf **rx_pkts,
				    uint16_t nb_pkts)
{
	uint16_t retval = 0;

	while (nb_pkts > RTE_I40E_VPMD_RX_BURST) {
		uint16_t burst = i40e_recv_scattered_burst_vec_avx512(rx_queue,
				rx_pkts + retval, RTE_I40E_VPMD_RX_BURST);
		retval += burst;
		nb_pkts -= burst;
		if (burst < RTE_I40E_VPMD_RX_BURST)
			return retval;
	}
	return retval + i40e_recv_scattered_burst_vec_avx512(rx_queue,
				rx_pkts + retval, nb_pkts);
}

static __rte_always_inline int
i40e_tx_free_bufs_avx512(struct i40e_tx_queue *txq)
{
	struct i40e_vec_tx_entry *txep;
	uint32_t n;
	uint32_t i;
	int nb_free = 0;
	struct rte_mbuf *m, *free[RTE_I40E_TX_MAX_FREE_BUF_SZ];

	/* check DD bits on threshold descriptor */
	if ((txq->tx_ring[txq->tx_next_dd].cmd_type_offset_bsz &
			rte_cpu_to_le_64(I40E_TXD_QW1_DTYPE_MASK)) !=
			rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DESC_DONE))
		return 0;

	n = txq->tx_rs_thresh;

	 /* first buffer to free from S/W ring is at index
	  * tx_next_dd - (tx_rs_thresh-1)
	  */
	txep = (void *)txq->sw_ring;
	txep += txq->tx_next_dd - (n - 1);

	if (txq->offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE && (n & 31) == 0) {
		struct rte_mempool *mp = txep[0].mbuf->pool;
		void **cache_objs;
		struct rte_mempool_cache *cache = rte_mempool_default_cache(mp,
				rte_lcore_id());

		if (!cache || n > RTE_MEMPOOL_CACHE_MAX_SIZE) {
			rte_mempool_generic_put(mp, (void *)txep, n, cache);
			goto done;
		}

		cache_objs = &cache->objs[cache->len];

		/* The cache follows the following algorithm
		 *   1. Add the objects to the cache
		 *   2. Anything greater than the cache min value (if it
		 *   crosses the cache flush threshold) is flushed to the ring.
		 */
		/* Add elements back into the cache */
		uint32_t copied = 0;
		/* n is multiple of 32 */
		while (copied < n) {
			const __m512i a = _mm512_load_si512(&txep[copied]);
			const __m512i b = _mm512_load_si512(&txep[copied + 8]);
			const __m512i c = _mm512_load_si512(&txep[copied + 16]);
			const __m512i d = _mm512_load_si512(&txep[copied + 24]);

			_mm512_storeu_si512(&cache_objs[copied], a);
			_mm512_storeu_si512(&cache_objs[copied + 8], b);
			_mm512_storeu_si512(&cache_objs[copied + 16], c);
			_mm512_storeu_si512(&cache_objs[copied + 24], d);
			copied += 32;
		}
		cache->len += n;

		if (cache->len >= cache->flushthresh) {
			rte_mempool_ops_enqueue_bulk
				(mp, &cache->objs[cache->size],
				cache->len - cache->size);
			cache->len = cache->size;
		}
		goto done;
	}

	m = rte_pktmbuf_prefree_seg(txep[0].mbuf);
	if (likely(m)) {
		free[0] = m;
		nb_free = 1;
		for (i = 1; i < n; i++) {
			rte_prefetch0(&txep[i + 3].mbuf->cacheline1);
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
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + txq->tx_rs_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_rs_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);

	return txq->tx_rs_thresh;
}

static inline void
vtx1(volatile struct i40e_tx_desc *txdp, struct rte_mbuf *pkt, uint64_t flags)
{
	uint64_t high_qw = (I40E_TX_DESC_DTYPE_DATA |
		((uint64_t)flags  << I40E_TXD_QW1_CMD_SHIFT) |
		((uint64_t)pkt->data_len << I40E_TXD_QW1_TX_BUF_SZ_SHIFT));

	__m128i descriptor = _mm_set_epi64x(high_qw,
				pkt->buf_iova + pkt->data_off);
	_mm_store_si128((__m128i *)txdp, descriptor);
}

static inline void
vtx(volatile struct i40e_tx_desc *txdp,
	struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags)
{
	const uint64_t hi_qw_tmpl = (I40E_TX_DESC_DTYPE_DATA |
			((uint64_t)flags  << I40E_TXD_QW1_CMD_SHIFT));

	for (; nb_pkts > 3; txdp += 4, pkt += 4, nb_pkts -= 4) {
		uint64_t hi_qw3 =
			hi_qw_tmpl |
			((uint64_t)pkt[3]->data_len <<
			 I40E_TXD_QW1_TX_BUF_SZ_SHIFT);
		uint64_t hi_qw2 =
			hi_qw_tmpl |
			((uint64_t)pkt[2]->data_len <<
			 I40E_TXD_QW1_TX_BUF_SZ_SHIFT);
		uint64_t hi_qw1 =
			hi_qw_tmpl |
			((uint64_t)pkt[1]->data_len <<
			 I40E_TXD_QW1_TX_BUF_SZ_SHIFT);
		uint64_t hi_qw0 =
			hi_qw_tmpl |
			((uint64_t)pkt[0]->data_len <<
			 I40E_TXD_QW1_TX_BUF_SZ_SHIFT);

		__m512i desc0_3 =
			_mm512_set_epi64
			(hi_qw3, pkt[3]->buf_iova + pkt[3]->data_off,
			hi_qw2, pkt[2]->buf_iova + pkt[2]->data_off,
			hi_qw1, pkt[1]->buf_iova + pkt[1]->data_off,
			hi_qw0, pkt[0]->buf_iova + pkt[0]->data_off);
		_mm512_storeu_si512((void *)txdp, desc0_3);
	}

	/* do any last ones */
	while (nb_pkts) {
		vtx1(txdp, *pkt, flags);
		txdp++, pkt++, nb_pkts--;
	}
}

static __rte_always_inline void
tx_backlog_entry_avx512(struct i40e_vec_tx_entry *txep,
			struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i;

	for (i = 0; i < (int)nb_pkts; ++i)
		txep[i].mbuf = tx_pkts[i];
}

static inline uint16_t
i40e_xmit_fixed_burst_vec_avx512(void *tx_queue, struct rte_mbuf **tx_pkts,
				 uint16_t nb_pkts)
{
	struct i40e_tx_queue *txq = (struct i40e_tx_queue *)tx_queue;
	volatile struct i40e_tx_desc *txdp;
	struct i40e_vec_tx_entry *txep;
	uint16_t n, nb_commit, tx_id;
	uint64_t flags = I40E_TD_CMD;
	uint64_t rs = I40E_TX_DESC_CMD_RS | I40E_TD_CMD;

	if (txq->nb_tx_free < txq->tx_free_thresh)
		i40e_tx_free_bufs_avx512(txq);

	nb_commit = nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = (void *)txq->sw_ring;
	txep += tx_id;

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {
		tx_backlog_entry_avx512(txep, tx_pkts, n);

		vtx(txdp, tx_pkts, n - 1, flags);
		tx_pkts += (n - 1);
		txdp += (n - 1);

		vtx1(txdp, *tx_pkts++, rs);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = txq->tx_ring;
		txep = (void *)txq->sw_ring;
	}

	tx_backlog_entry_avx512(txep, tx_pkts, nb_commit);

	vtx(txdp, tx_pkts, nb_commit, flags);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->tx_next_rs) {
		txq->tx_ring[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)I40E_TX_DESC_CMD_RS) <<
						I40E_TXD_QW1_CMD_SHIFT);
		txq->tx_next_rs =
			(uint16_t)(txq->tx_next_rs + txq->tx_rs_thresh);
	}

	txq->tx_tail = tx_id;

	I40E_PCI_REG_WC_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

uint16_t
i40e_xmit_pkts_vec_avx512(void *tx_queue, struct rte_mbuf **tx_pkts,
			  uint16_t nb_pkts)
{
	uint16_t nb_tx = 0;
	struct i40e_tx_queue *txq = (struct i40e_tx_queue *)tx_queue;

	while (nb_pkts) {
		uint16_t ret, num;

		/* cross rs_thresh boundary is not allowed */
		num = (uint16_t)RTE_MIN(nb_pkts, txq->tx_rs_thresh);
		ret = i40e_xmit_fixed_burst_vec_avx512
				(tx_queue, &tx_pkts[nb_tx], num);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}
