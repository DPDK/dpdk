/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_RXTX_VEC_SSE_H_
#define RTE_PMD_MLX5_RXTX_VEC_SSE_H_

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <smmintrin.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>

#include <mlx5_prm.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_rxtx_vec.h"
#include "mlx5_autoconf.h"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

/**
 * Store free buffers to RX SW ring.
 *
 * @param elts
 *   Pointer to SW ring to be filled.
 * @param pkts
 *   Pointer to array of packets to be stored.
 * @param pkts_n
 *   Number of packets to be stored.
 */
static inline void
rxq_copy_mbuf_v(struct rte_mbuf **elts, struct rte_mbuf **pkts, uint16_t n)
{
	unsigned int pos;
	uint16_t p = n & -2;

	for (pos = 0; pos < p; pos += 2) {
		__m128i mbp;

		mbp = _mm_loadu_si128((__m128i *)&elts[pos]);
		_mm_storeu_si128((__m128i *)&pkts[pos], mbp);
	}
	if (n & 1)
		pkts[pos] = elts[pos];
}

/**
 * Decompress a compressed completion and fill in mbufs in RX SW ring with data
 * extracted from the title completion descriptor.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param cq
 *   Pointer to completion array having a compressed completion at first.
 * @param elts
 *   Pointer to SW ring to be filled. The first mbuf has to be pre-built from
 *   the title completion descriptor to be copied to the rest of mbufs.
 *
 * @return
 *   Number of mini-CQEs successfully decompressed.
 */
static inline uint16_t
rxq_cq_decompress_v(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cq,
		    struct rte_mbuf **elts)
{
	volatile struct mlx5_mini_cqe8 *mcq = (void *)(cq + 1);
	struct rte_mbuf *t_pkt = elts[0]; /* Title packet is pre-built. */
	unsigned int pos;
	unsigned int i;
	unsigned int inv = 0;
	/* Mask to shuffle from extracted mini CQE to mbuf. */
	const __m128i shuf_mask1 =
		_mm_set_epi8(0,  1,  2,  3, /* rss, bswap32 */
			    -1, -1,         /* skip vlan_tci */
			     6,  7,         /* data_len, bswap16 */
			    -1, -1,  6,  7, /* pkt_len, bswap16 */
			    -1, -1, -1, -1  /* skip packet_type */);
	const __m128i shuf_mask2 =
		_mm_set_epi8(8,  9, 10, 11, /* rss, bswap32 */
			    -1, -1,         /* skip vlan_tci */
			    14, 15,         /* data_len, bswap16 */
			    -1, -1, 14, 15, /* pkt_len, bswap16 */
			    -1, -1, -1, -1  /* skip packet_type */);
	/* Restore the compressed count. Must be 16 bits. */
	const uint16_t mcqe_n = t_pkt->data_len +
				(rxq->crc_present * RTE_ETHER_CRC_LEN);
	const __m128i rearm =
		_mm_loadu_si128((__m128i *)&t_pkt->rearm_data);
	const __m128i rxdf =
		_mm_loadu_si128((__m128i *)&t_pkt->rx_descriptor_fields1);
	const __m128i crc_adj =
		_mm_set_epi16(0, 0, 0,
			      rxq->crc_present * RTE_ETHER_CRC_LEN,
			      0,
			      rxq->crc_present * RTE_ETHER_CRC_LEN,
			      0, 0);
	__m128i ol_flags = _mm_setzero_si128();
	__m128i ol_flags_mask = _mm_setzero_si128();
#ifdef MLX5_PMD_SOFT_COUNTERS
	const __m128i zero = _mm_setzero_si128();
	const __m128i ones = _mm_cmpeq_epi32(zero, zero);
	uint32_t rcvd_byte = 0;
	/* Mask to shuffle byte_cnt to add up stats. Do bswap16 for all. */
	const __m128i len_shuf_mask =
		_mm_set_epi8(-1, -1, -1, -1,
			     -1, -1, -1, -1,
			     14, 15,  6,  7,
			     10, 11,  2,  3);
#endif
	/*
	 * A. load mCQEs into a 128bit register.
	 * B. store rearm data to mbuf.
	 * C. combine data from mCQEs with rx_descriptor_fields1.
	 * D. store rx_descriptor_fields1.
	 * E. store flow tag (rte_flow mark).
	 */
	for (pos = 0; pos < mcqe_n; ) {
		__m128i mcqe1, mcqe2;
		__m128i rxdf1, rxdf2;
#ifdef MLX5_PMD_SOFT_COUNTERS
		__m128i byte_cnt, invalid_mask;
#endif

		for (i = 0; i < MLX5_VPMD_DESCS_PER_LOOP; ++i)
			if (likely(pos + i < mcqe_n))
				rte_prefetch0((void *)(cq + pos + i));
		/* A.1 load mCQEs into a 128bit register. */
		mcqe1 = _mm_loadu_si128((__m128i *)&mcq[pos % 8]);
		mcqe2 = _mm_loadu_si128((__m128i *)&mcq[pos % 8 + 2]);
		/* B.1 store rearm data to mbuf. */
		_mm_storeu_si128((__m128i *)&elts[pos]->rearm_data, rearm);
		_mm_storeu_si128((__m128i *)&elts[pos + 1]->rearm_data, rearm);
		/* C.1 combine data from mCQEs with rx_descriptor_fields1. */
		rxdf1 = _mm_shuffle_epi8(mcqe1, shuf_mask1);
		rxdf2 = _mm_shuffle_epi8(mcqe1, shuf_mask2);
		rxdf1 = _mm_sub_epi16(rxdf1, crc_adj);
		rxdf2 = _mm_sub_epi16(rxdf2, crc_adj);
		rxdf1 = _mm_blend_epi16(rxdf1, rxdf, 0x23);
		rxdf2 = _mm_blend_epi16(rxdf2, rxdf, 0x23);
		/* D.1 store rx_descriptor_fields1. */
		_mm_storeu_si128((__m128i *)
				  &elts[pos]->rx_descriptor_fields1,
				 rxdf1);
		_mm_storeu_si128((__m128i *)
				  &elts[pos + 1]->rx_descriptor_fields1,
				 rxdf2);
		/* B.1 store rearm data to mbuf. */
		_mm_storeu_si128((__m128i *)&elts[pos + 2]->rearm_data, rearm);
		_mm_storeu_si128((__m128i *)&elts[pos + 3]->rearm_data, rearm);
		/* C.1 combine data from mCQEs with rx_descriptor_fields1. */
		rxdf1 = _mm_shuffle_epi8(mcqe2, shuf_mask1);
		rxdf2 = _mm_shuffle_epi8(mcqe2, shuf_mask2);
		rxdf1 = _mm_sub_epi16(rxdf1, crc_adj);
		rxdf2 = _mm_sub_epi16(rxdf2, crc_adj);
		rxdf1 = _mm_blend_epi16(rxdf1, rxdf, 0x23);
		rxdf2 = _mm_blend_epi16(rxdf2, rxdf, 0x23);
		/* D.1 store rx_descriptor_fields1. */
		_mm_storeu_si128((__m128i *)
				  &elts[pos + 2]->rx_descriptor_fields1,
				 rxdf1);
		_mm_storeu_si128((__m128i *)
				  &elts[pos + 3]->rx_descriptor_fields1,
				 rxdf2);
#ifdef MLX5_PMD_SOFT_COUNTERS
		invalid_mask = _mm_set_epi64x(0,
					      (mcqe_n - pos) *
					      sizeof(uint16_t) * 8);
		invalid_mask = _mm_sll_epi64(ones, invalid_mask);
		byte_cnt = _mm_blend_epi16(_mm_srli_si128(mcqe1, 4),
					   mcqe2, 0xcc);
		byte_cnt = _mm_shuffle_epi8(byte_cnt, len_shuf_mask);
		byte_cnt = _mm_andnot_si128(invalid_mask, byte_cnt);
		byte_cnt = _mm_hadd_epi16(byte_cnt, zero);
		rcvd_byte += _mm_cvtsi128_si64(_mm_hadd_epi16(byte_cnt, zero));
#endif
		if (rxq->mark) {
			if (rxq->mcqe_format !=
				MLX5_CQE_RESP_FORMAT_FTAG_STRIDX) {
				const uint32_t flow_tag = t_pkt->hash.fdir.hi;

				/* E.1 store flow tag (rte_flow mark). */
				elts[pos]->hash.fdir.hi = flow_tag;
				elts[pos + 1]->hash.fdir.hi = flow_tag;
				elts[pos + 2]->hash.fdir.hi = flow_tag;
				elts[pos + 3]->hash.fdir.hi = flow_tag;
			} else {
				const __m128i flow_mark_adj =
					_mm_set_epi32(-1, -1, -1, -1);
				const __m128i flow_mark_shuf =
					_mm_set_epi8(-1,  9,  8, 12,
						     -1,  1,  0,  4,
						     -1, -1, -1, -1,
						     -1, -1, -1, -1);
				const __m128i ft_mask =
					_mm_set1_epi32(0xffffff00);
				const __m128i fdir_flags =
					_mm_set1_epi32(RTE_MBUF_F_RX_FDIR);
				const __m128i fdir_all_flags =
					_mm_set1_epi32(RTE_MBUF_F_RX_FDIR |
						       RTE_MBUF_F_RX_FDIR_ID);
				__m128i fdir_id_flags =
					_mm_set1_epi32(RTE_MBUF_F_RX_FDIR_ID);

				/* Extract flow_tag field. */
				__m128i ftag0 =
					_mm_shuffle_epi8(mcqe1, flow_mark_shuf);
				__m128i ftag1 =
					_mm_shuffle_epi8(mcqe2, flow_mark_shuf);
				__m128i ftag =
					_mm_unpackhi_epi64(ftag0, ftag1);
				__m128i invalid_mask =
					_mm_cmpeq_epi32(ftag, zero);

				ol_flags_mask = _mm_or_si128(ol_flags_mask,
							     fdir_all_flags);
				/* Set RTE_MBUF_F_RX_FDIR if flow tag is non-zero. */
				ol_flags = _mm_or_si128(ol_flags,
					_mm_andnot_si128(invalid_mask,
							 fdir_flags));
				/* Mask out invalid entries. */
				fdir_id_flags = _mm_andnot_si128(invalid_mask,
								 fdir_id_flags);
				/* Check if flow tag MLX5_FLOW_MARK_DEFAULT. */
				ol_flags = _mm_or_si128(ol_flags,
					_mm_andnot_si128(_mm_cmpeq_epi32(ftag,
							 ft_mask),
					fdir_id_flags));
				ftag = _mm_add_epi32(ftag, flow_mark_adj);
				elts[pos]->hash.fdir.hi =
						_mm_extract_epi32(ftag, 0);
				elts[pos + 1]->hash.fdir.hi =
						_mm_extract_epi32(ftag, 1);
				elts[pos + 2]->hash.fdir.hi =
						_mm_extract_epi32(ftag, 2);
				elts[pos + 3]->hash.fdir.hi =
						_mm_extract_epi32(ftag, 3);
			}
		}
		if (unlikely(rxq->mcqe_format != MLX5_CQE_RESP_FORMAT_HASH)) {
			if (rxq->mcqe_format ==
			    MLX5_CQE_RESP_FORMAT_L34H_STRIDX) {
				const uint8_t pkt_info =
					(cq->pkt_info & 0x3) << 6;
				const uint8_t pkt_hdr0 =
					_mm_extract_epi8(mcqe1, 0);
				const uint8_t pkt_hdr1 =
					_mm_extract_epi8(mcqe1, 8);
				const uint8_t pkt_hdr2 =
					_mm_extract_epi8(mcqe2, 0);
				const uint8_t pkt_hdr3 =
					_mm_extract_epi8(mcqe2, 8);
				const __m128i vlan_mask =
					_mm_set1_epi32(RTE_MBUF_F_RX_VLAN |
						       RTE_MBUF_F_RX_VLAN_STRIPPED);
				const __m128i cv_mask =
					_mm_set1_epi32(MLX5_CQE_VLAN_STRIPPED);
				const __m128i pkt_cv =
					_mm_set_epi32(pkt_hdr0 & 0x1,
						      pkt_hdr1 & 0x1,
						      pkt_hdr2 & 0x1,
						      pkt_hdr3 & 0x1);

				ol_flags_mask = _mm_or_si128(ol_flags_mask,
							     vlan_mask);
				ol_flags = _mm_or_si128(ol_flags,
					_mm_and_si128(_mm_cmpeq_epi32(pkt_cv,
					cv_mask), vlan_mask));
				elts[pos]->packet_type =
					mlx5_ptype_table[(pkt_hdr0 >> 2) |
							 pkt_info];
				elts[pos + 1]->packet_type =
					mlx5_ptype_table[(pkt_hdr1 >> 2) |
							 pkt_info];
				elts[pos + 2]->packet_type =
					mlx5_ptype_table[(pkt_hdr2 >> 2) |
							 pkt_info];
				elts[pos + 3]->packet_type =
					mlx5_ptype_table[(pkt_hdr3 >> 2) |
							 pkt_info];
				if (rxq->tunnel) {
					elts[pos]->packet_type |=
						!!(((pkt_hdr0 >> 2) |
						pkt_info) & (1 << 6));
					elts[pos + 1]->packet_type |=
						!!(((pkt_hdr1 >> 2) |
						pkt_info) & (1 << 6));
					elts[pos + 2]->packet_type |=
						!!(((pkt_hdr2 >> 2) |
						pkt_info) & (1 << 6));
					elts[pos + 3]->packet_type |=
						!!(((pkt_hdr3 >> 2) |
						pkt_info) & (1 << 6));
				}
			}
			const __m128i hash_flags =
				_mm_set1_epi32(RTE_MBUF_F_RX_RSS_HASH);
			const __m128i rearm_flags =
				_mm_set1_epi32((uint32_t)t_pkt->ol_flags);

			ol_flags_mask = _mm_or_si128(ol_flags_mask, hash_flags);
			ol_flags = _mm_or_si128(ol_flags,
				_mm_andnot_si128(ol_flags_mask, rearm_flags));
			elts[pos]->ol_flags =
				_mm_extract_epi32(ol_flags, 0);
			elts[pos + 1]->ol_flags =
				_mm_extract_epi32(ol_flags, 1);
			elts[pos + 2]->ol_flags =
				_mm_extract_epi32(ol_flags, 2);
			elts[pos + 3]->ol_flags =
				_mm_extract_epi32(ol_flags, 3);
			elts[pos]->hash.rss = 0;
			elts[pos + 1]->hash.rss = 0;
			elts[pos + 2]->hash.rss = 0;
			elts[pos + 3]->hash.rss = 0;
		}
		if (rxq->dynf_meta) {
			int32_t offs = rxq->flow_meta_offset;
			const uint32_t meta =
				*RTE_MBUF_DYNFIELD(t_pkt, offs, uint32_t *);

			/* Check if title packet has valid metadata. */
			if (meta) {
				MLX5_ASSERT(t_pkt->ol_flags &
					    rxq->flow_meta_mask);
				*RTE_MBUF_DYNFIELD(elts[pos], offs,
							uint32_t *) = meta;
				*RTE_MBUF_DYNFIELD(elts[pos + 1], offs,
							uint32_t *) = meta;
				*RTE_MBUF_DYNFIELD(elts[pos + 2], offs,
							uint32_t *) = meta;
				*RTE_MBUF_DYNFIELD(elts[pos + 3], offs,
							uint32_t *) = meta;
			}
		}
		pos += MLX5_VPMD_DESCS_PER_LOOP;
		/* Move to next CQE and invalidate consumed CQEs. */
		if (!(pos & 0x7) && pos < mcqe_n) {
			if (pos + 8 < mcqe_n)
				rte_prefetch0((void *)(cq + pos + 8));
			mcq = (void *)(cq + pos);
			for (i = 0; i < 8; ++i)
				cq[inv++].op_own = MLX5_CQE_INVALIDATE;
		}
	}
	/* Invalidate the rest of CQEs. */
	for (; inv < mcqe_n; ++inv)
		cq[inv].op_own = MLX5_CQE_INVALIDATE;
#ifdef MLX5_PMD_SOFT_COUNTERS
	rxq->stats.ipackets += mcqe_n;
	rxq->stats.ibytes += rcvd_byte;
#endif
	return mcqe_n;
}

/**
 * Calculate packet type and offload flag for mbuf and store it.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param cqes[4]
 *   Array of four 16bytes completions extracted from the original completion
 *   descriptor.
 * @param op_err
 *   Opcode vector having responder error status. Each field is 4B.
 * @param pkts
 *   Pointer to array of packets to be filled.
 */
static inline void
rxq_cq_to_ptype_oflags_v(struct mlx5_rxq_data *rxq, __m128i cqes[4],
			 __m128i op_err, struct rte_mbuf **pkts)
{
	__m128i pinfo0, pinfo1;
	__m128i pinfo, ptype;
	__m128i ol_flags = _mm_set1_epi32(rxq->rss_hash * RTE_MBUF_F_RX_RSS_HASH |
					  rxq->hw_timestamp * rxq->timestamp_rx_flag);
	__m128i cv_flags;
	const __m128i zero = _mm_setzero_si128();
	const __m128i ptype_mask = _mm_set1_epi32(0xfd06);
	const __m128i ptype_ol_mask = _mm_set1_epi32(0x106);
	const __m128i pinfo_mask = _mm_set1_epi32(0x3);
	const __m128i cv_flag_sel =
		_mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0,
			     (uint8_t)((RTE_MBUF_F_RX_IP_CKSUM_GOOD |
					RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1),
			     0,
			     (uint8_t)(RTE_MBUF_F_RX_L4_CKSUM_GOOD >> 1),
			     0,
			     (uint8_t)(RTE_MBUF_F_RX_IP_CKSUM_GOOD >> 1),
			     (uint8_t)(RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED),
			     0);
	const __m128i cv_mask =
		_mm_set1_epi32(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
			       RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED);
	const __m128i mbuf_init =
		_mm_load_si128((__m128i *)&rxq->mbuf_initializer);
	__m128i rearm0, rearm1, rearm2, rearm3;
	uint8_t pt_idx0, pt_idx1, pt_idx2, pt_idx3;

	/* Extract pkt_info field. */
	pinfo0 = _mm_unpacklo_epi32(cqes[0], cqes[1]);
	pinfo1 = _mm_unpacklo_epi32(cqes[2], cqes[3]);
	pinfo = _mm_unpacklo_epi64(pinfo0, pinfo1);
	/* Extract hdr_type_etc field. */
	pinfo0 = _mm_unpackhi_epi32(cqes[0], cqes[1]);
	pinfo1 = _mm_unpackhi_epi32(cqes[2], cqes[3]);
	ptype = _mm_unpacklo_epi64(pinfo0, pinfo1);
	if (rxq->mark) {
		const __m128i pinfo_ft_mask = _mm_set1_epi32(0xffffff00);
		const __m128i fdir_flags = _mm_set1_epi32(RTE_MBUF_F_RX_FDIR);
		__m128i fdir_id_flags = _mm_set1_epi32(RTE_MBUF_F_RX_FDIR_ID);
		__m128i flow_tag, invalid_mask;

		flow_tag = _mm_and_si128(pinfo, pinfo_ft_mask);
		/* Check if flow tag is non-zero then set RTE_MBUF_F_RX_FDIR. */
		invalid_mask = _mm_cmpeq_epi32(flow_tag, zero);
		ol_flags = _mm_or_si128(ol_flags,
					_mm_andnot_si128(invalid_mask,
							 fdir_flags));
		/* Mask out invalid entries. */
		fdir_id_flags = _mm_andnot_si128(invalid_mask, fdir_id_flags);
		/* Check if flow tag MLX5_FLOW_MARK_DEFAULT. */
		ol_flags = _mm_or_si128(ol_flags,
					_mm_andnot_si128(
						_mm_cmpeq_epi32(flow_tag,
								pinfo_ft_mask),
						fdir_id_flags));
	}
	/*
	 * Merge the two fields to generate the following:
	 * bit[1]     = l3_ok
	 * bit[2]     = l4_ok
	 * bit[8]     = cv
	 * bit[11:10] = l3_hdr_type
	 * bit[14:12] = l4_hdr_type
	 * bit[15]    = ip_frag
	 * bit[16]    = tunneled
	 * bit[17]    = outer_l3_type
	 */
	ptype = _mm_and_si128(ptype, ptype_mask);
	pinfo = _mm_and_si128(pinfo, pinfo_mask);
	pinfo = _mm_slli_epi32(pinfo, 16);
	/* Make pinfo has merged fields for ol_flags calculation. */
	pinfo = _mm_or_si128(ptype, pinfo);
	ptype = _mm_srli_epi32(pinfo, 10);
	ptype = _mm_packs_epi32(ptype, zero);
	/* Errored packets will have RTE_PTYPE_ALL_MASK. */
	op_err = _mm_srli_epi16(op_err, 8);
	ptype = _mm_or_si128(ptype, op_err);
	pt_idx0 = _mm_extract_epi8(ptype, 0);
	pt_idx1 = _mm_extract_epi8(ptype, 2);
	pt_idx2 = _mm_extract_epi8(ptype, 4);
	pt_idx3 = _mm_extract_epi8(ptype, 6);
	pkts[0]->packet_type = mlx5_ptype_table[pt_idx0] |
			       !!(pt_idx0 & (1 << 6)) * rxq->tunnel;
	pkts[1]->packet_type = mlx5_ptype_table[pt_idx1] |
			       !!(pt_idx1 & (1 << 6)) * rxq->tunnel;
	pkts[2]->packet_type = mlx5_ptype_table[pt_idx2] |
			       !!(pt_idx2 & (1 << 6)) * rxq->tunnel;
	pkts[3]->packet_type = mlx5_ptype_table[pt_idx3] |
			       !!(pt_idx3 & (1 << 6)) * rxq->tunnel;
	/* Fill flags for checksum and VLAN. */
	pinfo = _mm_and_si128(pinfo, ptype_ol_mask);
	pinfo = _mm_shuffle_epi8(cv_flag_sel, pinfo);
	/* Locate checksum flags at byte[2:1] and merge with VLAN flags. */
	cv_flags = _mm_slli_epi32(pinfo, 9);
	cv_flags = _mm_or_si128(pinfo, cv_flags);
	/* Move back flags to start from byte[0]. */
	cv_flags = _mm_srli_epi32(cv_flags, 8);
	/* Mask out garbage bits. */
	cv_flags = _mm_and_si128(cv_flags, cv_mask);
	/* Merge to ol_flags. */
	ol_flags = _mm_or_si128(ol_flags, cv_flags);
	/* Merge mbuf_init and ol_flags. */
	rearm0 = _mm_blend_epi16(mbuf_init, _mm_slli_si128(ol_flags, 8), 0x30);
	rearm1 = _mm_blend_epi16(mbuf_init, _mm_slli_si128(ol_flags, 4), 0x30);
	rearm2 = _mm_blend_epi16(mbuf_init, ol_flags, 0x30);
	rearm3 = _mm_blend_epi16(mbuf_init, _mm_srli_si128(ol_flags, 4), 0x30);
	/* Write 8B rearm_data and 8B ol_flags. */
	_mm_store_si128((__m128i *)&pkts[0]->rearm_data, rearm0);
	_mm_store_si128((__m128i *)&pkts[1]->rearm_data, rearm1);
	_mm_store_si128((__m128i *)&pkts[2]->rearm_data, rearm2);
	_mm_store_si128((__m128i *)&pkts[3]->rearm_data, rearm3);
}

/**
 * Process a non-compressed completion and fill in mbufs in RX SW ring
 * with data extracted from the title completion descriptor.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param cq
 *   Pointer to completion array having a non-compressed completion at first.
 * @param elts
 *   Pointer to SW ring to be filled. The first mbuf has to be pre-built from
 *   the title completion descriptor to be copied to the rest of mbufs.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 * @param[out] err
 *   Pointer to a flag. Set non-zero value if pkts array has at least one error
 *   packet to handle.
 * @param[out] comp
 *   Pointer to a index. Set it to the first compressed completion if any.
 *
 * @return
 *   Number of CQEs successfully processed.
 */
static inline uint16_t
rxq_cq_process_v(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cq,
		 struct rte_mbuf **elts, struct rte_mbuf **pkts,
		 uint16_t pkts_n, uint64_t *err, uint64_t *comp)
{
	const uint16_t q_n = 1 << rxq->cqe_n;
	const uint16_t q_mask = q_n - 1;
	unsigned int pos;
	uint64_t n = 0;
	uint64_t comp_idx = MLX5_VPMD_DESCS_PER_LOOP;
	uint16_t nocmp_n = 0;
	unsigned int ownership = !!(rxq->cq_ci & (q_mask + 1));
	const __m128i owner_check =	_mm_set1_epi64x(0x0100000001000000LL);
	const __m128i opcode_check = _mm_set1_epi64x(0xf0000000f0000000LL);
	const __m128i format_check = _mm_set1_epi64x(0x0c0000000c000000LL);
	const __m128i resp_err_check = _mm_set1_epi64x(0xe0000000e0000000LL);
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint32_t rcvd_byte = 0;
	/* Mask to shuffle byte_cnt to add up stats. Do bswap16 for all. */
	const __m128i len_shuf_mask =
		_mm_set_epi8(-1, -1, -1, -1,
			     -1, -1, -1, -1,
			     12, 13,  8,  9,
			      4,  5,  0,  1);
#endif
	/* Mask to shuffle from extracted CQE to mbuf. */
	const __m128i shuf_mask =
		_mm_set_epi8(-1,  3,  2,  1, /* fdir.hi */
			     12, 13, 14, 15, /* rss, bswap32 */
			     10, 11,         /* vlan_tci, bswap16 */
			      4,  5,         /* data_len, bswap16 */
			     -1, -1,         /* zero out 2nd half of pkt_len */
			      4,  5          /* pkt_len, bswap16 */);
	/* Mask to blend from the last Qword to the first DQword. */
	const __m128i blend_mask =
		_mm_set_epi8(-1, -1, -1, -1,
			     -1, -1, -1, -1,
			      0,  0,  0,  0,
			      0,  0,  0, -1);
	const __m128i zero = _mm_setzero_si128();
	const __m128i ones = _mm_cmpeq_epi32(zero, zero);
	const __m128i crc_adj =
		_mm_set_epi16(0, 0, 0, 0, 0,
			      rxq->crc_present * RTE_ETHER_CRC_LEN,
			      0,
			      rxq->crc_present * RTE_ETHER_CRC_LEN);
	const __m128i flow_mark_adj = _mm_set_epi32(rxq->mark * (-1), 0, 0, 0);
	/*
	 * A. load first Qword (8bytes) in one loop.
	 * B. copy 4 mbuf pointers from elts ring to returning pkts.
	 * C. load remained CQE data and extract necessary fields.
	 *    Final 16bytes cqes[] extracted from original 64bytes CQE has the
	 *    following structure:
	 *        struct {
	 *          uint8_t  pkt_info;
	 *          uint8_t  flow_tag[3];
	 *          uint16_t byte_cnt;
	 *          uint8_t  rsvd4;
	 *          uint8_t  op_own;
	 *          uint16_t hdr_type_etc;
	 *          uint16_t vlan_info;
	 *          uint32_t rx_has_res;
	 *        } c;
	 * D. fill in mbuf.
	 * E. get valid CQEs.
	 * F. find compressed CQE.
	 */
	for (pos = 0;
	     pos < pkts_n;
	     pos += MLX5_VPMD_DESCS_PER_LOOP) {
		__m128i cqes[MLX5_VPMD_DESCS_PER_LOOP];
		__m128i cqe_tmp1, cqe_tmp2;
		__m128i pkt_mb0, pkt_mb1, pkt_mb2, pkt_mb3;
		__m128i op_own, op_own_tmp1, op_own_tmp2;
		__m128i opcode, owner_mask, invalid_mask;
		__m128i comp_mask;
		__m128i mask;
#ifdef MLX5_PMD_SOFT_COUNTERS
		__m128i byte_cnt;
#endif
		__m128i mbp1, mbp2;
		__m128i p = _mm_set_epi16(0, 0, 0, 0, 3, 2, 1, 0);
		unsigned int p1, p2, p3;

		/* Prefetch next 4 CQEs. */
		if (pkts_n - pos >= 2 * MLX5_VPMD_DESCS_PER_LOOP) {
			rte_prefetch0(&cq[pos + MLX5_VPMD_DESCS_PER_LOOP]);
			rte_prefetch0(&cq[pos + MLX5_VPMD_DESCS_PER_LOOP + 1]);
			rte_prefetch0(&cq[pos + MLX5_VPMD_DESCS_PER_LOOP + 2]);
			rte_prefetch0(&cq[pos + MLX5_VPMD_DESCS_PER_LOOP + 3]);
		}
		/* A.0 do not cross the end of CQ. */
		mask = _mm_set_epi64x(0, (pkts_n - pos) * sizeof(uint16_t) * 8);
		mask = _mm_sll_epi64(ones, mask);
		p = _mm_andnot_si128(mask, p);
		/* A.1 load cqes. */
		p3 = _mm_extract_epi16(p, 3);
		cqes[3] = _mm_loadl_epi64((__m128i *)
					   &cq[pos + p3].sop_drop_qpn);
		rte_compiler_barrier();
		p2 = _mm_extract_epi16(p, 2);
		cqes[2] = _mm_loadl_epi64((__m128i *)
					   &cq[pos + p2].sop_drop_qpn);
		rte_compiler_barrier();
		/* B.1 load mbuf pointers. */
		mbp1 = _mm_loadu_si128((__m128i *)&elts[pos]);
		mbp2 = _mm_loadu_si128((__m128i *)&elts[pos + 2]);
		/* A.1 load a block having op_own. */
		p1 = _mm_extract_epi16(p, 1);
		cqes[1] = _mm_loadl_epi64((__m128i *)
					   &cq[pos + p1].sop_drop_qpn);
		rte_compiler_barrier();
		cqes[0] = _mm_loadl_epi64((__m128i *)
					   &cq[pos].sop_drop_qpn);
		/* B.2 copy mbuf pointers. */
		_mm_storeu_si128((__m128i *)&pkts[pos], mbp1);
		_mm_storeu_si128((__m128i *)&pkts[pos + 2], mbp2);
		rte_io_rmb();
		/* C.1 load remained CQE data and extract necessary fields. */
		cqe_tmp2 = _mm_load_si128((__m128i *)&cq[pos + p3]);
		cqe_tmp1 = _mm_load_si128((__m128i *)&cq[pos + p2]);
		cqes[3] = _mm_blendv_epi8(cqes[3], cqe_tmp2, blend_mask);
		cqes[2] = _mm_blendv_epi8(cqes[2], cqe_tmp1, blend_mask);
		cqe_tmp2 = _mm_loadu_si128((__m128i *)&cq[pos + p3].csum);
		cqe_tmp1 = _mm_loadu_si128((__m128i *)&cq[pos + p2].csum);
		cqes[3] = _mm_blend_epi16(cqes[3], cqe_tmp2, 0x30);
		cqes[2] = _mm_blend_epi16(cqes[2], cqe_tmp1, 0x30);
		cqe_tmp2 = _mm_loadl_epi64((__m128i *)&cq[pos + p3].rsvd4[2]);
		cqe_tmp1 = _mm_loadl_epi64((__m128i *)&cq[pos + p2].rsvd4[2]);
		cqes[3] = _mm_blend_epi16(cqes[3], cqe_tmp2, 0x04);
		cqes[2] = _mm_blend_epi16(cqes[2], cqe_tmp1, 0x04);
		/* C.2 generate final structure for mbuf with swapping bytes. */
		pkt_mb3 = _mm_shuffle_epi8(cqes[3], shuf_mask);
		pkt_mb2 = _mm_shuffle_epi8(cqes[2], shuf_mask);
		/* C.3 adjust CRC length. */
		pkt_mb3 = _mm_sub_epi16(pkt_mb3, crc_adj);
		pkt_mb2 = _mm_sub_epi16(pkt_mb2, crc_adj);
		/* C.4 adjust flow mark. */
		pkt_mb3 = _mm_add_epi32(pkt_mb3, flow_mark_adj);
		pkt_mb2 = _mm_add_epi32(pkt_mb2, flow_mark_adj);
		/* D.1 fill in mbuf - rx_descriptor_fields1. */
		_mm_storeu_si128((void *)&pkts[pos + 3]->pkt_len, pkt_mb3);
		_mm_storeu_si128((void *)&pkts[pos + 2]->pkt_len, pkt_mb2);
		/* E.1 extract op_own field. */
		op_own_tmp2 = _mm_unpacklo_epi32(cqes[2], cqes[3]);
		/* C.1 load remained CQE data and extract necessary fields. */
		cqe_tmp2 = _mm_load_si128((__m128i *)&cq[pos + p1]);
		cqe_tmp1 = _mm_load_si128((__m128i *)&cq[pos]);
		cqes[1] = _mm_blendv_epi8(cqes[1], cqe_tmp2, blend_mask);
		cqes[0] = _mm_blendv_epi8(cqes[0], cqe_tmp1, blend_mask);
		cqe_tmp2 = _mm_loadu_si128((__m128i *)&cq[pos + p1].csum);
		cqe_tmp1 = _mm_loadu_si128((__m128i *)&cq[pos].csum);
		cqes[1] = _mm_blend_epi16(cqes[1], cqe_tmp2, 0x30);
		cqes[0] = _mm_blend_epi16(cqes[0], cqe_tmp1, 0x30);
		cqe_tmp2 = _mm_loadl_epi64((__m128i *)&cq[pos + p1].rsvd4[2]);
		cqe_tmp1 = _mm_loadl_epi64((__m128i *)&cq[pos].rsvd4[2]);
		cqes[1] = _mm_blend_epi16(cqes[1], cqe_tmp2, 0x04);
		cqes[0] = _mm_blend_epi16(cqes[0], cqe_tmp1, 0x04);
		/* C.2 generate final structure for mbuf with swapping bytes. */
		pkt_mb1 = _mm_shuffle_epi8(cqes[1], shuf_mask);
		pkt_mb0 = _mm_shuffle_epi8(cqes[0], shuf_mask);
		/* C.3 adjust CRC length. */
		pkt_mb1 = _mm_sub_epi16(pkt_mb1, crc_adj);
		pkt_mb0 = _mm_sub_epi16(pkt_mb0, crc_adj);
		/* C.4 adjust flow mark. */
		pkt_mb1 = _mm_add_epi32(pkt_mb1, flow_mark_adj);
		pkt_mb0 = _mm_add_epi32(pkt_mb0, flow_mark_adj);
		/* E.1 extract op_own byte. */
		op_own_tmp1 = _mm_unpacklo_epi32(cqes[0], cqes[1]);
		op_own = _mm_unpackhi_epi64(op_own_tmp1, op_own_tmp2);
		/* D.1 fill in mbuf - rx_descriptor_fields1. */
		_mm_storeu_si128((void *)&pkts[pos + 1]->pkt_len, pkt_mb1);
		_mm_storeu_si128((void *)&pkts[pos]->pkt_len, pkt_mb0);
		/* E.2 flip owner bit to mark CQEs from last round. */
		owner_mask = _mm_and_si128(op_own, owner_check);
		if (ownership)
			owner_mask = _mm_xor_si128(owner_mask, owner_check);
		owner_mask = _mm_cmpeq_epi32(owner_mask, owner_check);
		owner_mask = _mm_packs_epi32(owner_mask, zero);
		/* E.3 get mask for invalidated CQEs. */
		opcode = _mm_and_si128(op_own, opcode_check);
		invalid_mask = _mm_cmpeq_epi32(opcode_check, opcode);
		invalid_mask = _mm_packs_epi32(invalid_mask, zero);
		/* E.4 mask out beyond boundary. */
		invalid_mask = _mm_or_si128(invalid_mask, mask);
		/* E.5 merge invalid_mask with invalid owner. */
		invalid_mask = _mm_or_si128(invalid_mask, owner_mask);
		/* F.1 find compressed CQE format. */
		comp_mask = _mm_and_si128(op_own, format_check);
		comp_mask = _mm_cmpeq_epi32(comp_mask, format_check);
		comp_mask = _mm_packs_epi32(comp_mask, zero);
		/* F.2 mask out invalid entries. */
		comp_mask = _mm_andnot_si128(invalid_mask, comp_mask);
		comp_idx = _mm_cvtsi128_si64(comp_mask);
		/* F.3 get the first compressed CQE. */
		comp_idx = comp_idx ?
				__builtin_ctzll(comp_idx) /
					(sizeof(uint16_t) * 8) :
				MLX5_VPMD_DESCS_PER_LOOP;
		/* E.6 mask out entries after the compressed CQE. */
		mask = _mm_set_epi64x(0, comp_idx * sizeof(uint16_t) * 8);
		mask = _mm_sll_epi64(ones, mask);
		invalid_mask = _mm_or_si128(invalid_mask, mask);
		/* E.7 count non-compressed valid CQEs. */
		n = _mm_cvtsi128_si64(invalid_mask);
		n = n ? __builtin_ctzll(n) / (sizeof(uint16_t) * 8) :
			MLX5_VPMD_DESCS_PER_LOOP;
		nocmp_n += n;
		/* D.2 get the final invalid mask. */
		mask = _mm_set_epi64x(0, n * sizeof(uint16_t) * 8);
		mask = _mm_sll_epi64(ones, mask);
		invalid_mask = _mm_or_si128(invalid_mask, mask);
		/* D.3 check error in opcode. */
		opcode = _mm_cmpeq_epi32(resp_err_check, opcode);
		opcode = _mm_packs_epi32(opcode, zero);
		opcode = _mm_andnot_si128(invalid_mask, opcode);
		/* D.4 mark if any error is set */
		*err |= _mm_cvtsi128_si64(opcode);
		/* D.5 fill in mbuf - rearm_data and packet_type. */
		rxq_cq_to_ptype_oflags_v(rxq, cqes, opcode, &pkts[pos]);
		if (unlikely(rxq->shared)) {
			pkts[pos]->port = cq[pos].user_index_low;
			pkts[pos + p1]->port = cq[pos + p1].user_index_low;
			pkts[pos + p2]->port = cq[pos + p2].user_index_low;
			pkts[pos + p3]->port = cq[pos + p3].user_index_low;
		}
		if (unlikely(rxq->hw_timestamp)) {
			int offset = rxq->timestamp_offset;
			if (rxq->rt_timestamp) {
				struct mlx5_dev_ctx_shared *sh = rxq->sh;
				uint64_t ts;

				ts = rte_be_to_cpu_64(cq[pos].timestamp);
				mlx5_timestamp_set(pkts[pos], offset,
					mlx5_txpp_convert_rx_ts(sh, ts));
				ts = rte_be_to_cpu_64(cq[pos + p1].timestamp);
				mlx5_timestamp_set(pkts[pos + 1], offset,
					mlx5_txpp_convert_rx_ts(sh, ts));
				ts = rte_be_to_cpu_64(cq[pos + p2].timestamp);
				mlx5_timestamp_set(pkts[pos + 2], offset,
					mlx5_txpp_convert_rx_ts(sh, ts));
				ts = rte_be_to_cpu_64(cq[pos + p3].timestamp);
				mlx5_timestamp_set(pkts[pos + 3], offset,
					mlx5_txpp_convert_rx_ts(sh, ts));
			} else {
				mlx5_timestamp_set(pkts[pos], offset,
					rte_be_to_cpu_64(cq[pos].timestamp));
				mlx5_timestamp_set(pkts[pos + 1], offset,
					rte_be_to_cpu_64(cq[pos + p1].timestamp));
				mlx5_timestamp_set(pkts[pos + 2], offset,
					rte_be_to_cpu_64(cq[pos + p2].timestamp));
				mlx5_timestamp_set(pkts[pos + 3], offset,
					rte_be_to_cpu_64(cq[pos + p3].timestamp));
			}
		}
		if (rxq->dynf_meta) {
			/* This code is subject for further optimization. */
			int32_t offs = rxq->flow_meta_offset;
			uint32_t mask = rxq->flow_meta_port_mask;

			*RTE_MBUF_DYNFIELD(pkts[pos], offs, uint32_t *) =
				rte_be_to_cpu_32
				(cq[pos].flow_table_metadata) &	mask;
			*RTE_MBUF_DYNFIELD(pkts[pos + 1], offs, uint32_t *) =
				rte_be_to_cpu_32
				(cq[pos + p1].flow_table_metadata) & mask;
			*RTE_MBUF_DYNFIELD(pkts[pos + 2], offs, uint32_t *) =
				rte_be_to_cpu_32
				(cq[pos + p2].flow_table_metadata) & mask;
			*RTE_MBUF_DYNFIELD(pkts[pos + 3], offs, uint32_t *) =
				rte_be_to_cpu_32
				(cq[pos + p3].flow_table_metadata) & mask;
			if (*RTE_MBUF_DYNFIELD(pkts[pos], offs, uint32_t *))
				pkts[pos]->ol_flags |= rxq->flow_meta_mask;
			if (*RTE_MBUF_DYNFIELD(pkts[pos + 1], offs, uint32_t *))
				pkts[pos + 1]->ol_flags |= rxq->flow_meta_mask;
			if (*RTE_MBUF_DYNFIELD(pkts[pos + 2], offs, uint32_t *))
				pkts[pos + 2]->ol_flags |= rxq->flow_meta_mask;
			if (*RTE_MBUF_DYNFIELD(pkts[pos + 3], offs, uint32_t *))
				pkts[pos + 3]->ol_flags |= rxq->flow_meta_mask;
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Add up received bytes count. */
		byte_cnt = _mm_shuffle_epi8(op_own, len_shuf_mask);
		byte_cnt = _mm_andnot_si128(invalid_mask, byte_cnt);
		byte_cnt = _mm_hadd_epi16(byte_cnt, zero);
		rcvd_byte += _mm_cvtsi128_si64(_mm_hadd_epi16(byte_cnt, zero));
#endif
		/*
		 * Break the loop unless more valid CQE is expected, or if
		 * there's a compressed CQE.
		 */
		if (n != MLX5_VPMD_DESCS_PER_LOOP)
			break;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	rxq->stats.ipackets += nocmp_n;
	rxq->stats.ibytes += rcvd_byte;
#endif
	if (comp_idx == n)
		*comp = comp_idx;
	return nocmp_n;
}

#endif /* RTE_PMD_MLX5_RXTX_VEC_SSE_H_ */
