/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2019-2023 Broadcom All rights reserved. */

#include <inttypes.h>
#include <stdbool.h>

#include <rte_bitmap.h>
#include <rte_byteorder.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_vect.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_ring.h"

#include "bnxt_txq.h"
#include "bnxt_txr.h"
#include "bnxt_rxtx_vec_common.h"
#include <unistd.h>

/*
 * Scalar tail handler for the V2 completion format. Processes a
 * single completion pair so that the caller can drain the packets left over
 * once the eight-wide vector loop can no longer be run without stepping past
 * the end of either ring.
 */
static uint16_t
recv_one_vec_avx2(struct bnxt_rx_queue *rxq, struct rte_mbuf **rx_pkts,
		  uint32_t cons, uint32_t mbcons, uint32_t valid_bit,
		  uint64_t mbuf_init, struct cmpl_base *cp_desc_ring,
		  struct rte_mbuf **rx_buf_ring)
{
	struct rx_pkt_cmpl *rxcmp = (struct rx_pkt_cmpl *)&cp_desc_ring[cons];
	struct rx_pkt_cmpl_hi *rxcmp1 = (struct rx_pkt_cmpl_hi *)&cp_desc_ring[cons + 1];
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint32_t flags_type, flags2, index, errors;
	struct rte_mbuf *mbuf;
	uint16_t errors_v2;
	uint64_t ol_flags;

	errors_v2 = rxcmp1->errors_v2;

	if (!((errors_v2 & CMPL_BASE_V) ^ valid_bit))
		return 0;

	rte_compiler_barrier();

	mbuf = rx_buf_ring[mbcons];
	RTE_ASSERT(mbuf != NULL);

	flags_type = rxcmp->flags_type;
	/* The vector loop only ever consumes the low 16 bits of flags2. */
	flags2 = rxcmp1->flags2 & 0xffff;

	/*
	 * Build the ol_flags and checksum error table indices exactly as the
	 * vector loop does: the tunnel bit (RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC)
	 * is folded into bit 5 of the ol_flags index and bit 4 of the error
	 * index. ol_flags_err_table[] is zero for every entry with no error
	 * bit set, so the lookup is unconditional here as well.
	 */
	errors = ((errors_v2 >> 4) & 0xf) & flags2;
	index = (flags2 & 0x1f) & ~errors;
	index |= (flags2 & RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC) << 3;
	errors |= (flags2 & RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC) << 2;

	ol_flags = rxr->ol_flags_table[index];
	ol_flags |= rxr->ol_flags_err_table[errors];

	mbuf->hash.rss = rxcmp->rss_hash;
	if (flags_type & RX_PKT_CMPL_FLAGS_RSS_VALID)
		ol_flags |= RTE_MBUF_F_RX_RSS_HASH;

	/*
	 * ol_flags_table already carries RX_VLAN|RX_VLAN_STRIPPED when strip
	 * offload is enabled; mirror the vector loop and OR them in from the
	 * per-packet VLAN metadata bit so that the two sources agree.
	 */
	if ((flags2 & RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN) &&
	    BNXT_RX_VLAN_STRIP_EN(rxq->bp))
		ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;

	mbuf->ol_flags = ol_flags;

	mbuf->next = NULL;
	mbuf->pkt_len = rxcmp->len;
	mbuf->data_len = mbuf->pkt_len;
	mbuf->rearm_data[0] = mbuf_init;
	mbuf->packet_type = bnxt_parse_pkt_type(rxcmp, rxcmp1);
	mbuf->vlan_tci = rxcmp1->metadata;

	rx_pkts[0] = mbuf;
	return 1;
}

/*
 * AVX2 V2 RX burst processing
 *
 * The function will always consume 2 * nb_pkts cpr bds and write nb_pkts
 * packets given valid. Bounds are enforced by bnxt_recv_pkts_vec_avx2.
 */
static uint16_t
recv_burst_vec_avx2(struct bnxt_rx_queue *rxq, struct rte_mbuf **rx_pkts,
		    uint16_t nb_pkts, uint32_t cons, uint32_t mbcons,
		    uint32_t valid_bit, uint64_t mbuf_init,
		    struct cmpl_base *cp_desc_ring, struct rte_mbuf **rx_buf_ring)
{
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	const __m256i info3_v_mask = _mm256_set1_epi32(CMPL_BASE_V);
	const __m256i mbuf_init_v =
		_mm256_set_epi64x(0, 0, 0, mbuf_init);
	const __m256i valid_target =
		_mm256_set1_epi32(valid_bit);
	const __m256i dsc_shuf_msk =
		_mm256_set_epi8(0xff, 0xff, 0xff, 0xff,  /* Zeroes. */
				7, 6,                    /* metadata type */
				9, 8,                    /* flags2 low 16 */
				5, 4,                    /* vlan_tci */
				1, 0,                    /* errors_v2 */
				0xff, 0xff, 0xff, 0xff,  /* Zeroes. */
				0xff, 0xff, 0xff, 0xff,  /* Zeroes. */
				7, 6,                    /* metadata type */
				9, 8,                    /* flags2 low 16 */
				5, 4,                    /* vlan_tci */
				1, 0,                    /* errors_v2 */
				0xff, 0xff, 0xff, 0xff); /* Zeroes. */
	const __m256i shuf_msk =
		_mm256_set_epi8(15, 14, 13, 12,          /* rss */
				7, 6,                    /* vlan_tci */
				3, 2,                    /* data_len */
				0xFF, 0xFF, 3, 2,        /* pkt_len */
				0xFF, 0xFF, 0xFF, 0xFF,  /* pkt_type (zeroes) */
				15, 14, 13, 12,          /* rss */
				7, 6,                    /* vlan_tci */
				3, 2,                    /* data_len */
				0xFF, 0xFF, 3, 2,        /* pkt_len */
				0xFF, 0xFF, 0xFF, 0xFF); /* pkt_type (zeroes) */
	const __m256i flags_type_mask =
		_mm256_set1_epi32(RX_PKT_CMPL_FLAGS_ITYPE_MASK);
	const __m256i flags2_mask1 =
		_mm256_set1_epi32(CMPL_FLAGS2_VLAN_TUN_MSK);
	const __m256i flags2_mask2 =
		_mm256_set1_epi32(RX_PKT_CMPL_FLAGS2_IP_TYPE);
	const __m256i rss_mask =
		_mm256_set1_epi32(RX_PKT_CMPL_FLAGS_RSS_VALID);
	/*
	 * ol_flags_table already sets RX_VLAN|RX_VLAN_STRIPPED when VLAN strip
	 * is enabled.  For completeness, also OR in the flags here based on the
	 * per-packet VLAN-metadata bit so that the two sources agree.  The
	 * constant is broadcast once: non-zero only when strip offload is on.
	 */
	const __m256i vlan_ol_val =
		BNXT_RX_VLAN_STRIP_EN(rxq->bp) ?
		_mm256_set1_epi32((uint32_t)(RTE_MBUF_F_RX_VLAN |
					     RTE_MBUF_F_RX_VLAN_STRIPPED)) :
					     _mm256_setzero_si256();
	__m256i t0, t1, flags_type, flags2, index, errors;
	__m256i ptype_idx, ptypes, is_tunnel;
	__m256i mbuf01, mbuf23, mbuf45, mbuf67;
	__m256i rearm0, rearm1, rearm2, rearm3, rearm4, rearm5, rearm6, rearm7;
	__m256i ol_flags, ol_flags_hi;
	__m256i rss_flags;

	int nb_rx_pkts = 0;
	uint64_t valid;
	int i;

	/* Validate ptype table indexing at build time. */
	bnxt_check_ptype_constants();

	/* Handle RX burst request */
	for (i = 0; i < nb_pkts; i += BNXT_RX_DESCS_PER_LOOP_VEC256,
				  cons += BNXT_RX_DESCS_PER_LOOP_VEC256 * 2,
				  mbcons += BNXT_RX_DESCS_PER_LOOP_VEC256) {
		__m256i desc0, desc1, desc2, desc3, desc4, desc5, desc6, desc7;
		__m256i rxcmp0_1, rxcmp2_3, rxcmp4_5, rxcmp6_7, info3_v;
		__m256i errors_v2;
		uint32_t num_valid;

		/* Copy eight mbuf pointers to output array. */
		t0 = _mm256_loadu_si256((void *)&rx_buf_ring[mbcons]);
		_mm256_storeu_si256((void *)&rx_pkts[i], t0);
#ifdef RTE_ARCH_X86_64
		t0 = _mm256_loadu_si256((void *)&rx_buf_ring[mbcons + 4]);
		_mm256_storeu_si256((void *)&rx_pkts[i + 4], t0);
#endif

		/*
		 * Load eight receive completion descriptors into 256-bit
		 * registers. Loads are issued in reverse order in order to
		 * ensure consistent state.
		 */
		desc7 = _mm256_load_si256((void *)&cp_desc_ring[cons + 14]);
		rte_compiler_barrier();
		desc6 = _mm256_load_si256((void *)&cp_desc_ring[cons + 12]);
		rte_compiler_barrier();
		desc5 = _mm256_load_si256((void *)&cp_desc_ring[cons + 10]);
		rte_compiler_barrier();
		desc4 = _mm256_load_si256((void *)&cp_desc_ring[cons + 8]);
		rte_compiler_barrier();
		desc3 = _mm256_load_si256((void *)&cp_desc_ring[cons + 6]);
		rte_compiler_barrier();
		desc2 = _mm256_load_si256((void *)&cp_desc_ring[cons + 4]);
		rte_compiler_barrier();
		desc1 = _mm256_load_si256((void *)&cp_desc_ring[cons + 2]);
		rte_compiler_barrier();
		desc0 = _mm256_load_si256((void *)&cp_desc_ring[cons + 0]);

		/*
		 * Pack needed fields from each descriptor into a compressed
		 * 128-bit layout and pair two compressed descriptors into
		 * 256-bit registers. The 128-bit compressed layout is as
		 * follows:
		 *     Bits  0-15: flags_type field from low completion record.
		 *     Bits 16-31: len field  from low completion record.
		 *     Bits 32-47: flags2 (low 16 bits) from high completion.
		 *     Bits 48-79: metadata from high completion record.
		 *     Bits 80-95: errors_v2 from high completion record.
		 *     Bits 96-127: rss hash from low completion record.
		 */
		t0 = _mm256_permute2f128_si256(desc6, desc7, 0x20);
		t1 = _mm256_permute2f128_si256(desc6, desc7, 0x31);
		t1 = _mm256_shuffle_epi8(t1, dsc_shuf_msk);
		rxcmp6_7 = _mm256_blend_epi32(t0, t1, 0x66);

		t0 = _mm256_permute2f128_si256(desc4, desc5, 0x20);
		t1 = _mm256_permute2f128_si256(desc4, desc5, 0x31);
		t1 = _mm256_shuffle_epi8(t1, dsc_shuf_msk);
		rxcmp4_5 = _mm256_blend_epi32(t0, t1, 0x66);

		t0 = _mm256_permute2f128_si256(desc2, desc3, 0x20);
		t1 = _mm256_permute2f128_si256(desc2, desc3, 0x31);
		t1 = _mm256_shuffle_epi8(t1, dsc_shuf_msk);
		rxcmp2_3 = _mm256_blend_epi32(t0, t1, 0x66);

		t0 = _mm256_permute2f128_si256(desc0, desc1, 0x20);
		t1 = _mm256_permute2f128_si256(desc0, desc1, 0x31);
		t1 = _mm256_shuffle_epi8(t1, dsc_shuf_msk);
		rxcmp0_1 = _mm256_blend_epi32(t0, t1, 0x66);

		/* Compute packet type table indices for eight packets. */
		t0 = _mm256_unpacklo_epi32(rxcmp0_1, rxcmp2_3);
		t1 = _mm256_unpacklo_epi32(rxcmp4_5, rxcmp6_7);
		flags_type = _mm256_unpacklo_epi64(t0, t1);
		flags2 = _mm256_unpackhi_epi64(t0, t1);
		ptype_idx = _mm256_and_si256(flags_type, flags_type_mask);
		ptype_idx = _mm256_srli_epi32(ptype_idx,
					      RX_PKT_CMPL_FLAGS_ITYPE_SFT -
					      BNXT_PTYPE_TBL_TYPE_SFT);

		t0 = _mm256_srli_epi32(_mm256_and_si256(flags2, flags2_mask1),
				       RX_PKT_CMPL_FLAGS2_META_FORMAT_SFT -
				       BNXT_PTYPE_TBL_VLAN_SFT);
		ptype_idx = _mm256_or_si256(ptype_idx, t0);

		t0 = _mm256_srli_epi32(_mm256_and_si256(flags2, flags2_mask2),
				       RX_PKT_CMPL_FLAGS2_IP_TYPE_SFT -
				       BNXT_PTYPE_TBL_IP_VER_SFT);
		ptype_idx = _mm256_or_si256(ptype_idx, t0);

		/*
		 * Load ptypes for eight packets using gather. Gather operations
		 * have extremely high latency (~19 cycles), execution and use
		 * of result should be separated as much as possible.
		 */
		ptypes = _mm256_i32gather_epi32((int *)bnxt_ptype_table,
						ptype_idx, sizeof(uint32_t));
		/*
		 * Compute ol_flags and checksum error table indices for eight
		 * packets.
		 */
		is_tunnel = _mm256_and_si256(flags2, _mm256_set1_epi32(4));
		is_tunnel = _mm256_slli_epi32(is_tunnel, 3);
		flags2 = _mm256_and_si256(flags2, _mm256_set1_epi32(0x1F));

		/* Extract errors_v2 fields for eight packets. */
		t0 = _mm256_unpackhi_epi32(rxcmp0_1, rxcmp2_3);
		t1 = _mm256_unpackhi_epi32(rxcmp4_5, rxcmp6_7);
		errors_v2 = _mm256_unpacklo_epi64(t0, t1);

		errors = _mm256_srli_epi32(errors_v2, 4);
		errors = _mm256_and_si256(errors, _mm256_set1_epi32(0xF));
		errors = _mm256_and_si256(errors, flags2);

		index = _mm256_andnot_si256(errors, flags2);
		errors = _mm256_or_si256(errors,
					 _mm256_srli_epi32(is_tunnel, 1));
		index = _mm256_or_si256(index, is_tunnel);

		/*
		 * Load ol_flags for eight packets using gather. Gather
		 * operations have extremely high latency (~19 cycles),
		 * execution and use of result should be separated as much
		 * as possible.
		 */
		ol_flags = _mm256_i32gather_epi32((int *)rxr->ol_flags_table,
						  index, sizeof(uint32_t));
		errors = _mm256_i32gather_epi32((int *)rxr->ol_flags_err_table,
						errors, sizeof(uint32_t));

		/*
		 * Pack the 128-bit array of valid descriptor flags into 64
		 * bits and count the number of set bits in order to determine
		 * the number of valid descriptors.
		 */
		info3_v = _mm256_and_si256(errors_v2, info3_v_mask);
		info3_v = _mm256_xor_si256(info3_v, valid_target);

		info3_v = _mm256_packs_epi32(info3_v, _mm256_setzero_si256());
		valid = _mm_cvtsi128_si64(_mm256_extracti128_si256(info3_v, 1));
		valid = (valid << CHAR_BIT) |
			_mm_cvtsi128_si64(_mm256_castsi256_si128(info3_v));
		num_valid = rte_popcount64(valid);

		if (num_valid == 0)
			break;

		rte_compiler_barrier();

		/* Update mbuf rearm_data for eight packets. */
		mbuf01 = _mm256_shuffle_epi8(rxcmp0_1, shuf_msk);
		mbuf23 = _mm256_shuffle_epi8(rxcmp2_3, shuf_msk);
		mbuf45 = _mm256_shuffle_epi8(rxcmp4_5, shuf_msk);
		mbuf67 = _mm256_shuffle_epi8(rxcmp6_7, shuf_msk);

		/* Blend in ptype field for two mbufs at a time. */
		mbuf01 = _mm256_blend_epi32(mbuf01, ptypes, 0x11);
		mbuf23 = _mm256_blend_epi32(mbuf23,
					_mm256_srli_si256(ptypes, 4), 0x11);
		mbuf45 = _mm256_blend_epi32(mbuf45,
					_mm256_srli_si256(ptypes, 8), 0x11);
		mbuf67 = _mm256_blend_epi32(mbuf67,
					_mm256_srli_si256(ptypes, 12), 0x11);

		/* Unpack rearm data, set fixed fields for first four mbufs. */
		rearm0 = _mm256_permute2f128_si256(mbuf_init_v, mbuf01, 0x20);
		rearm1 = _mm256_blend_epi32(mbuf_init_v, mbuf01, 0xF0);
		rearm2 = _mm256_permute2f128_si256(mbuf_init_v, mbuf23, 0x20);
		rearm3 = _mm256_blend_epi32(mbuf_init_v, mbuf23, 0xF0);

		/* Compute final ol_flags values for eight packets. */
		rss_flags = _mm256_and_si256(flags_type, rss_mask);
		rss_flags = _mm256_srli_epi32(rss_flags, 9);
		ol_flags = _mm256_or_si256(ol_flags, errors);
		ol_flags = _mm256_or_si256(ol_flags, rss_flags);
		/*
		 * Set RX_VLAN | RX_VLAN_STRIPPED for packets whose vlan_tci
		 * is non-zero (i.e. hardware reported VLAN metadata, indicated
		 * by RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN in index bit 4).
		 * vlan_ol_val is the broadcast constant computed before the
		 * loop: non-zero only when VLAN RX strip offload is enabled.
		 * _mm256_cmpeq_epi32 produces 0xFFFFFFFF per lane when the
		 * VLAN bit is set, masking the constant to those lanes only.
		 */
		{
			const __m256i vlan_bit =
				_mm256_set1_epi32(RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN);
			__m256i vlan_mask =
				_mm256_cmpeq_epi32(_mm256_and_si256(index, vlan_bit),
						   vlan_bit);
			ol_flags = _mm256_or_si256(ol_flags,
					_mm256_and_si256(vlan_mask,
							 vlan_ol_val));
		}
		ol_flags_hi = _mm256_permute2f128_si256(ol_flags,
							ol_flags, 0x11);

		/* Set ol_flags fields for first four packets. */
		rearm0 = _mm256_blend_epi32(rearm0,
					    _mm256_slli_si256(ol_flags, 8),
					    0x04);
		rearm1 = _mm256_blend_epi32(rearm1,
					    _mm256_slli_si256(ol_flags_hi, 8),
					    0x04);
		rearm2 = _mm256_blend_epi32(rearm2,
					    _mm256_slli_si256(ol_flags, 4),
					    0x04);
		rearm3 = _mm256_blend_epi32(rearm3,
					    _mm256_slli_si256(ol_flags_hi, 4),
					    0x04);

		/* Store all mbuf fields for first four packets. */
		_mm256_storeu_si256((void *)&rx_pkts[i + 0]->rearm_data,
				    rearm0);
		_mm256_storeu_si256((void *)&rx_pkts[i + 1]->rearm_data,
				    rearm1);
		_mm256_storeu_si256((void *)&rx_pkts[i + 2]->rearm_data,
				    rearm2);
		_mm256_storeu_si256((void *)&rx_pkts[i + 3]->rearm_data,
				    rearm3);

		/* Unpack rearm data, set fixed fields for final four mbufs. */
		rearm4 = _mm256_permute2f128_si256(mbuf_init_v, mbuf45, 0x20);
		rearm5 = _mm256_blend_epi32(mbuf_init_v, mbuf45, 0xF0);
		rearm6 = _mm256_permute2f128_si256(mbuf_init_v, mbuf67, 0x20);
		rearm7 = _mm256_blend_epi32(mbuf_init_v, mbuf67, 0xF0);

		/* Set ol_flags fields for final four packets. */
		rearm4 = _mm256_blend_epi32(rearm4, ol_flags, 0x04);
		rearm5 = _mm256_blend_epi32(rearm5, ol_flags_hi, 0x04);
		rearm6 = _mm256_blend_epi32(rearm6,
					    _mm256_srli_si256(ol_flags, 4),
					    0x04);
		rearm7 = _mm256_blend_epi32(rearm7,
					    _mm256_srli_si256(ol_flags_hi, 4),
					    0x04);

		/* Store all mbuf fields for final four packets. */
		_mm256_storeu_si256((void *)&rx_pkts[i + 4]->rearm_data,
				    rearm4);
		_mm256_storeu_si256((void *)&rx_pkts[i + 5]->rearm_data,
				    rearm5);
		_mm256_storeu_si256((void *)&rx_pkts[i + 6]->rearm_data,
				    rearm6);
		_mm256_storeu_si256((void *)&rx_pkts[i + 7]->rearm_data,
				    rearm7);

		nb_rx_pkts += num_valid;
		if (num_valid < BNXT_RX_DESCS_PER_LOOP_VEC256)
			break;
	}

	return nb_rx_pkts;
}

uint16_t
bnxt_recv_pkts_vec_avx2(void *rx_queue, struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	struct bnxt_rx_queue *rxq = rx_queue;
	uint16_t brst = (uint16_t)RTE_MAX((uint32_t)rxq->rx_free_thresh,
					   BNXT_RX_DESCS_PER_LOOP_VEC256);
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint32_t cp_ring_size = cpr->cp_ring_struct->ring_size;
	uint32_t rx_ring_size = rxr->rx_ring_struct->ring_size;
	struct cmpl_base *cp_desc_ring = cpr->cp_desc_ring;
	struct rte_mbuf **rx_buf_ring = rxr->rx_buf_ring;
	uint16_t cnt = 0;

	if (unlikely(!rxq->rx_started))
		return 0;

	while (nb_pkts) {
		uint32_t cons, mbcons, valid_bit, num, num_vec, received = 0;

		if (rxq->rxrearm_nb >= rxq->rx_free_thresh)
			bnxt_rxq_rearm(rxq, rxr);

		valid_bit = !!(cpr->cp_raw_cons & cp_ring_size);
		cons = cpr->cp_raw_cons & (cp_ring_size - 1);
		mbcons = (cpr->cp_raw_cons / 2) & (rx_ring_size - 1);

		/* Never wrap either ring within one pass. */
		num = RTE_MIN((uint32_t)nb_pkts, brst);
		num = RTE_MIN(num, rx_ring_size - mbcons);
		num = RTE_MIN(num, (cp_ring_size - cons) / 2);

		num_vec = RTE_ALIGN_FLOOR(num, BNXT_RX_DESCS_PER_LOOP_VEC256);

		if (num_vec)
			received = recv_burst_vec_avx2(rxq, rx_pkts + cnt,
						     num_vec, cons, mbcons,
						     valid_bit,
						     rxq->mbuf_initializer,
						     cp_desc_ring, rx_buf_ring);

		/* Only chase the tail if the vector loops all filled. */
		if (received == num_vec) {
			while (received < num &&
				recv_one_vec_avx2(rxq, rx_pkts + cnt + received,
						     cons + 2 * received, mbcons + received,
						     valid_bit,
						     rxq->mbuf_initializer,
						     cp_desc_ring, rx_buf_ring))
				received++;
		}

		if (received == 0)
			break;

		rxr->rx_raw_prod = RING_ADV(rxr->rx_raw_prod, received);
		rxq->rxrearm_nb += received;
		cpr->cp_raw_cons += 2 * received;
		bnxt_db_cq(cpr);

		cnt += received;
		nb_pkts -= received;

		if (received < num)
			break;
	}

	return cnt;
}

static uint16_t
crx_one_vec_avx2(struct rte_mbuf **rx_pkts,
		 uint32_t cons, uint32_t mbcons, uint32_t valid_bit,
		 uint64_t mbuf_init, struct cmpl_base *cp_desc_ring,
		 struct rte_mbuf **rx_buf_ring)
{
	struct rx_pkt_compress_cmpl *rxcmp = (struct rx_pkt_compress_cmpl *)&cp_desc_ring[cons];
	uint64_t hi, rss = 0, vlan_tci, pkt_type, ol_flags = RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN;
	uint32_t cs_error_calc, csum_67x, itype, ptype_inner, ip_type;
	uint32_t flags_type;
	uint16_t error_v1, tunnel;
	struct rte_mbuf *mbuf;
	uint64_t *rearm;

	error_v1 = rxcmp->metadata1_cs_error_calc_v1;

	if (!((error_v1 & CMPL_BASE_V) ^ valid_bit))
		return 0;

	rte_compiler_barrier();

	mbuf = rx_buf_ring[mbcons];
	RTE_ASSERT(mbuf != NULL);
	rx_pkts[0] = mbuf;
	rearm = (uint64_t *)&mbuf->rearm_data[0];

	cs_error_calc = (uint32_t)(error_v1 >> 4) & 0xff;
	csum_67x = crx_csum_67x_lut[(cs_error_calc >> 6) & 7];
	ol_flags |= crx_csum_012_lut[cs_error_calc & 7] &
		    crx_csum_345_lut[(cs_error_calc >> 3) & 7] &
		    csum_67x;
	tunnel = (uint16_t)(csum_67x >> 31);

	if (error_v1 & RX_PKT_COMPRESS_CMPL_METADATA1_VALID) {
		vlan_tci = rxcmp->vlanc_tcid;
		pkt_type = RTE_PTYPE_L2_ETHER_VLAN;
		ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
	} else {
		vlan_tci = 0;
		pkt_type = RTE_PTYPE_L2_ETHER;
	}
	flags_type = rxcmp->flags_type;

	if (flags_type & RX_PKT_COMPRESS_CMPL_FLAGS_RSS_VALID) {
		rss = rxcmp->rss_hash;
		ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
	}

	itype = (flags_type >> 12) & 0xf;
	ptype_inner = (itype < 8) ? crx_pkt_t_inner_lut[itype] : 0;
	ip_type = ((flags_type & RX_PKT_COMPRESS_CMPL_FLAGS_IP_TYPE) ?
			RTE_MBUF_INDETERMINATE_INNER_OUTER_IPV6 :
			RTE_MBUF_INDETERMINATE_INNER_OUTER_IPV4) |
		  RTE_MBUF_INDETERMINATE_INNER_OUTER_TCP |
		  RTE_MBUF_INDETERMINATE_INNER_OUTER_UDP |
		  RTE_MBUF_INDETERMINATE_INNER_OUTER_ICMP;

	pkt_type |= ptype_inner & ip_type &
		    (tunnel ? RTE_MBUF_INNER_ALL : RTE_MBUF_OUTER_ALL);

	mbuf->next = NULL;
	hi = rxcmp->len;
	pkt_type |= hi << 32;
	hi |= vlan_tci << 16;
	hi |= rss << 32;
	rearm[0] = mbuf_init;
	rearm[1] = ol_flags;
	rearm[2] = pkt_type;
	rearm[3] = hi;
	return 1;
}

static uint16_t
crx_burst_vec_avx2(struct rte_mbuf **rx_pkts,
		    uint16_t nb_pkts, uint32_t cons, uint32_t mbcons, uint32_t valid_bit,
		    uint64_t mbuf_init, struct cmpl_base *cp_desc_ring,
		    struct rte_mbuf **rx_buf_ring)
{
	const __m256i mask_1s =
		_mm256_set1_epi32(0x1);
	const __m256i mask_fs =
		_mm256_set1_epi32(0xf);
	const __m256i mbuf_init_v =
		_mm256_set1_epi64x(mbuf_init);
	const __m256i crx_csum_012_lut_v =
		_mm256_load_si256((const __m256i *)crx_csum_012_lut);
	const __m256i crx_csum_345_lut_v =
		_mm256_load_si256((const __m256i *)crx_csum_345_lut);
	const __m256i crx_csum_67x_lut_v =
		_mm256_load_si256((const __m256i *)crx_csum_67x_lut);
	const __m256i crx_pkt_t_inner_lut_v =
		_mm256_load_si256((const __m256i *)crx_pkt_t_inner_lut);

	const uint32_t valid_mask8 = valid_bit ? 0xffu : 0u;
	int nb_rx_pkts = 0, i;
	uint32_t valid;

	for (i = 0; i < nb_pkts; i += BNXT_RX_DESCS_PER_LOOP_VEC256,
				  cons += BNXT_RX_DESCS_PER_LOOP_VEC256,
				  mbcons += BNXT_RX_DESCS_PER_LOOP_VEC256) {
		__m256i rearm0, rearm1, rearm2, rearm3, rearm4, rearm5, rearm6, rearm7;
		__m256i third_64_6475, third_64_2031, last_64_6475, last_64_2031;
		__m256i ol_flags, ol_flags_2031, ol_flags_6475;
		__m256i rxcmp0_1, rxcmp2_3, rxcmp4_5, rxcmp6_7;
		__m256i error_v1, flags_type, cs_error_calc;
		__m256i ptypes, pkt_type, pkt_type_inner;
		__m256i itype, ip_type, itype_mask;
		__m256i tunnel_mask, t0, t1;
		__m256i vlan_mask, vlan_tci;
		__m256i vlan_tci_len, len;
		__m256i rss_mask, rss;
		uint32_t num_valid;

		/* Copy eight mbuf pointers to output array. */
		t0 = _mm256_loadu_si256((void *)&rx_buf_ring[mbcons]);
		_mm256_storeu_si256((void *)&rx_pkts[i], t0);
#ifdef RTE_ARCH_X86_64
		t0 = _mm256_loadu_si256((void *)&rx_buf_ring[mbcons + 4]);
		_mm256_storeu_si256((void *)&rx_pkts[i + 4], t0);
#endif

		/*
		 * Load eight receive completion descriptors into 256-bit
		 * registers. Loads are issued in reverse order in order to
		 * ensure consistent state.
		 */
		rxcmp6_7 = _mm256_lddqu_si256((void *)&cp_desc_ring[cons + 6]);
		rte_compiler_barrier();
		rxcmp4_5 = _mm256_lddqu_si256((void *)&cp_desc_ring[cons + 4]);
		rte_compiler_barrier();
		rxcmp2_3 = _mm256_lddqu_si256((void *)&cp_desc_ring[cons + 2]);
		rte_compiler_barrier();
		rxcmp0_1 = _mm256_lddqu_si256((void *)&cp_desc_ring[cons + 0]);
		rte_compiler_barrier();

		/*64207531*/
		t0 = _mm256_unpacklo_epi32(rxcmp6_7, rxcmp4_5);
		t1 = _mm256_unpacklo_epi32(rxcmp2_3, rxcmp0_1);
		flags_type = _mm256_unpacklo_epi64(t0, t1);
		rss = _mm256_unpackhi_epi64(t0, t1);

		t0 = _mm256_unpackhi_epi32(rxcmp6_7, rxcmp4_5);
		t1 = _mm256_unpackhi_epi32(rxcmp2_3, rxcmp0_1);
		error_v1 = _mm256_unpacklo_epi64(t0, t1);

		valid = _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_slli_epi32(error_v1, 31))) ^
			valid_mask8;
		/*
		 * Popcount is permutation invariant and mid sequence invalid
		 * completions are not expected.
		 */
		num_valid = rte_popcount32(valid);

		if (num_valid == 0)
			break;

		cs_error_calc = _mm256_srli_epi32(error_v1, 4);
		t0 = _mm256_permutevar8x32_epi32(crx_csum_012_lut_v, cs_error_calc);
		cs_error_calc = _mm256_srli_epi32(cs_error_calc, 3);
		t1 = _mm256_permutevar8x32_epi32(crx_csum_345_lut_v, cs_error_calc);
		cs_error_calc = _mm256_srli_epi32(cs_error_calc, 3);
		t1 = _mm256_and_si256(t0, t1);
		t0 = _mm256_permutevar8x32_epi32(crx_csum_67x_lut_v, cs_error_calc);
		ol_flags = _mm256_and_si256(t0, t1);

		tunnel_mask = _mm256_srai_epi32(t0, 31);
		t0 = _mm256_andnot_si256(tunnel_mask, _mm256_set1_epi32(RTE_MBUF_OUTER_ALL));
		t1 = _mm256_and_si256(tunnel_mask, _mm256_set1_epi32(RTE_MBUF_INNER_ALL));
		tunnel_mask = _mm256_or_si256(t0, t1);

		vlan_mask = _mm256_srli_epi32(error_v1, 15);
		vlan_mask = _mm256_and_si256(vlan_mask, mask_1s);
		vlan_mask = _mm256_cmpeq_epi32(vlan_mask, mask_1s);
		vlan_tci = _mm256_and_si256(error_v1, vlan_mask);
		t0 = _mm256_andnot_si256(vlan_mask, _mm256_set1_epi32(RTE_PTYPE_L2_ETHER));
		t1 = _mm256_and_si256(vlan_mask, _mm256_set1_epi32(RTE_PTYPE_L2_ETHER_VLAN));
		pkt_type = _mm256_or_si256(t0, t1);
		t1 = _mm256_and_si256(vlan_mask,
			_mm256_set1_epi32(RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED));
		ol_flags = _mm256_or_si256(ol_flags, t1);

		rss_mask = _mm256_srli_epi32(flags_type, 10);
		rss_mask = _mm256_and_si256(rss_mask, mask_1s);
		t0 = _mm256_slli_epi32(rss_mask, 1);
		ol_flags = _mm256_or_si256(ol_flags, t0);
		rss_mask = _mm256_cmpeq_epi32(rss_mask, mask_1s);
		rss = _mm256_and_si256(rss, rss_mask);

		itype = _mm256_srli_epi32(flags_type, 12);
		itype = _mm256_and_si256(itype, mask_fs);
		itype_mask = _mm256_cmpgt_epi32(_mm256_set1_epi32(8), itype);
		itype = _mm256_permutevar8x32_epi32(crx_pkt_t_inner_lut_v, itype);
		ip_type = _mm256_and_si256(flags_type,
				_mm256_set1_epi32(RX_PKT_COMPRESS_CMPL_FLAGS_IP_TYPE));
		ip_type = _mm256_cmpgt_epi32(ip_type, mask_1s);
		t0 = _mm256_andnot_si256(ip_type,
				_mm256_set1_epi32(RTE_MBUF_INDETERMINATE_INNER_OUTER_IPV4));
		t1 = _mm256_and_si256(ip_type,
				_mm256_set1_epi32(RTE_MBUF_INDETERMINATE_INNER_OUTER_IPV6));
		ip_type = _mm256_or_si256(t0, t1);
		ip_type = _mm256_or_si256(ip_type,
			      _mm256_set1_epi32(RTE_MBUF_INDETERMINATE_INNER_OUTER_TCP |
				RTE_MBUF_INDETERMINATE_INNER_OUTER_UDP |
				RTE_MBUF_INDETERMINATE_INNER_OUTER_ICMP));
		itype = _mm256_and_si256(itype, itype_mask);
		pkt_type_inner = _mm256_and_si256(itype, ip_type);
		pkt_type_inner = _mm256_and_si256(pkt_type_inner, tunnel_mask);
		ptypes = _mm256_or_si256(pkt_type, pkt_type_inner);

		len = _mm256_srli_epi32(flags_type, 16);
		vlan_tci_len = _mm256_blend_epi16(len, vlan_tci, 0xaa);

		third_64_6475 = _mm256_unpacklo_epi32(ptypes, len);
		third_64_2031 = _mm256_unpackhi_epi32(ptypes, len);

		last_64_6475 = _mm256_unpacklo_epi32(vlan_tci_len, rss);
		last_64_2031 = _mm256_unpackhi_epi32(vlan_tci_len, rss);

		t1 = _mm256_setzero_si256();
		ol_flags_6475 = _mm256_unpacklo_epi32(ol_flags, t1);
		ol_flags_2031 = _mm256_unpackhi_epi32(ol_flags, t1);

		rearm0 = _mm256_unpackhi_epi64(mbuf_init_v, ol_flags_2031);
		rearm4 = _mm256_unpackhi_epi64(mbuf_init_v, ol_flags_6475);
		t0 = _mm256_unpackhi_epi64(third_64_6475, last_64_6475);
		t1 = _mm256_unpackhi_epi64(third_64_2031, last_64_2031);
		rearm1 = _mm256_permute2f128_si256(rearm0, t1, 0x31);
		rearm0 = _mm256_permute2f128_si256(rearm0, t1, 0x20);
		rearm5 = _mm256_permute2f128_si256(rearm4, t0, 0x31);
		rearm4 = _mm256_permute2f128_si256(rearm4, t0, 0x20);

		rearm2 = _mm256_unpacklo_epi64(mbuf_init_v, ol_flags_2031);
		rearm6 = _mm256_unpacklo_epi64(mbuf_init_v, ol_flags_6475);
		t0 = _mm256_unpacklo_epi64(third_64_6475, last_64_6475);
		t1 = _mm256_unpacklo_epi64(third_64_2031, last_64_2031);
		rearm3 = _mm256_permute2f128_si256(rearm2, t1, 0x31);
		rearm2 = _mm256_permute2f128_si256(rearm2, t1, 0x20);
		rearm7 = _mm256_permute2f128_si256(rearm6, t0, 0x31);
		rearm6 = _mm256_permute2f128_si256(rearm6, t0, 0x20);

		_mm256_storeu_si256((void *)&rx_pkts[i + 0]->rearm_data,
				    rearm0);
		_mm256_storeu_si256((void *)&rx_pkts[i + 1]->rearm_data,
				    rearm1);
		_mm256_storeu_si256((void *)&rx_pkts[i + 2]->rearm_data,
				    rearm2);
		_mm256_storeu_si256((void *)&rx_pkts[i + 3]->rearm_data,
				    rearm3);
		_mm256_storeu_si256((void *)&rx_pkts[i + 4]->rearm_data,
				    rearm4);
		_mm256_storeu_si256((void *)&rx_pkts[i + 5]->rearm_data,
				    rearm5);
		_mm256_storeu_si256((void *)&rx_pkts[i + 6]->rearm_data,
				    rearm6);
		_mm256_storeu_si256((void *)&rx_pkts[i + 7]->rearm_data,
				    rearm7);

		nb_rx_pkts += num_valid;
		if (num_valid < BNXT_RX_DESCS_PER_LOOP_VEC256)
			break;
	}

	return nb_rx_pkts;
}

uint16_t
bnxt_crx_pkts_vec_avx2(void *rx_queue, struct rte_mbuf **rx_pkts,
		       uint16_t nb_pkts)
{
	struct bnxt_rx_queue *rxq = rx_queue;
	uint16_t brst = (uint16_t)RTE_MAX((uint32_t)rxq->rx_free_thresh,
					   BNXT_RX_DESCS_PER_LOOP_VEC256);
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint32_t cp_ring_size = cpr->cp_ring_struct->ring_size;
	uint32_t rx_ring_size = rxr->rx_ring_struct->ring_size;
	struct cmpl_base *cp_desc_ring = cpr->cp_desc_ring;
	struct rte_mbuf **rx_buf_ring = rxr->rx_buf_ring;
	uint16_t cnt = 0;

	if (unlikely(!rxq->rx_started))
		return 0;

	while (nb_pkts) {
		uint32_t cons, mbcons, valid_bit, num, num_vec, received = 0;

		if (rxq->rxrearm_nb >= rxq->rx_free_thresh)
			bnxt_rxq_rearm(rxq, rxr);

		valid_bit = !!(cpr->cp_raw_cons & cp_ring_size);
		cons = cpr->cp_raw_cons & (cp_ring_size - 1);
		mbcons = (cpr->cp_raw_cons) & (rx_ring_size - 1);

		/* Never wrap either ring within one pass. */
		num = RTE_MIN((uint32_t)nb_pkts, brst);
		num = RTE_MIN(num, rx_ring_size - mbcons);
		num = RTE_MIN(num, (cp_ring_size - cons));

		num_vec = RTE_ALIGN_FLOOR(num, BNXT_RX_DESCS_PER_LOOP_VEC256);

		if (num_vec)
			received = crx_burst_vec_avx2(rx_pkts + cnt,
						     num_vec, cons, mbcons,
						     valid_bit,
						     rxq->mbuf_initializer,
						     cp_desc_ring, rx_buf_ring);

		/* Only chase the tail if the vector loops all filled. */
		if (received == num_vec) {
			while (received < num &&
				crx_one_vec_avx2(rx_pkts + cnt + received,
						     cons + received, mbcons + received,
						     valid_bit,
						     rxq->mbuf_initializer,
						     cp_desc_ring, rx_buf_ring))
				received++;
		}

		if (received == 0)
			break;

		rxr->rx_raw_prod = RING_ADV(rxr->rx_raw_prod, received);
		rxq->rxrearm_nb += received;
		cpr->cp_raw_cons += received;
		bnxt_db_cq(cpr);

		cnt += received;
		nb_pkts -= received;

		if (received < num)
			break;
	}
	return cnt;
}

static void
bnxt_handle_tx_cp_vec(struct bnxt_tx_queue *txq)
{
	struct bnxt_cp_ring_info *cpr = txq->cp_ring;
	uint32_t raw_cons = cpr->cp_raw_cons;
	uint32_t cons;
	uint32_t nb_tx_pkts = 0;
	struct tx_cmpl *txcmp;
	struct cmpl_base *cp_desc_ring = cpr->cp_desc_ring;
	struct bnxt_ring *cp_ring_struct = cpr->cp_ring_struct;
	uint32_t ring_mask = cp_ring_struct->ring_mask;
	uint32_t tx_ring_mask;

	tx_ring_mask = txq->tx_ring->tx_ring_struct->ring_mask;
	do {
		cons = RING_CMPL(ring_mask, raw_cons);
		txcmp = (struct tx_cmpl *)&cp_desc_ring[cons];

		if (!bnxt_cpr_cmp_valid(txcmp, raw_cons, ring_mask + 1))
			break;

		uint16_t errors_v = rte_le_to_cpu_16(txcmp->errors_v);

		if (unlikely(errors_v & TX_CMPL_ERRORS_DMA_ERROR))
			rte_atomic_store_explicit(&txq->tx_dma_err,
				rte_atomic_load_explicit(&txq->tx_dma_err,
							 rte_memory_order_relaxed) + 1,
				rte_memory_order_relaxed);

		if (CMP_TYPE(txcmp) == CMPL_BASE_TYPE_TX_L2_COAL) {
			struct tx_cmpl_coal *txcmp_c = (struct tx_cmpl_coal *)txcmp;

			nb_tx_pkts = (rte_le_to_cpu_32(txcmp_c->sq_cons_idx) -
				      (txq->tx_ring->tx_raw_cons & tx_ring_mask)) &
				      tx_ring_mask;
		} else {
			nb_tx_pkts += txcmp->opaque;
		}
		raw_cons = NEXT_RAW_CMP(raw_cons);
	} while (nb_tx_pkts < ring_mask);

	if (nb_tx_pkts) {
		if (txq->offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
			bnxt_tx_cmp_vec_fast(txq, nb_tx_pkts);
		else
			bnxt_tx_cmp_vec(txq, nb_tx_pkts);
		cpr->cp_raw_cons = raw_cons;
		bnxt_db_cq(cpr);
	}
}

static inline void
bnxt_xmit_one(struct rte_mbuf *mbuf, struct tx_bd_long *txbd,
	      struct rte_mbuf **tx_buf)
{
	uint64_t dsc_hi, dsc_lo;
	__m128i desc;

	*tx_buf = mbuf;

	dsc_hi = mbuf->buf_iova + mbuf->data_off;
	dsc_lo = (mbuf->data_len << 16) |
		 bnxt_xmit_flags_len(mbuf->data_len, TX_BD_FLAGS_NOCMPL);

	desc = _mm_set_epi64x(dsc_hi, dsc_lo);
	_mm_store_si128((void *)txbd, desc);
}

static uint16_t
bnxt_xmit_fixed_burst_vec(struct bnxt_tx_queue *txq, struct rte_mbuf **pkts,
			  uint16_t nb_pkts)
{
	struct bnxt_tx_ring_info *txr = txq->tx_ring;
	uint16_t tx_prod, tx_raw_prod = txr->tx_raw_prod;
	struct tx_bd_long *txbd;
	struct rte_mbuf **tx_buf;
	uint16_t to_send;

	tx_prod = RING_IDX(txr->tx_ring_struct, tx_raw_prod);
	txbd = &txr->tx_desc_ring[tx_prod];
	tx_buf = &txr->tx_buf_ring[tx_prod];

	/* Prefetch next transmit buffer descriptors. */
	rte_prefetch0(txbd);
	rte_prefetch0(txbd + 3);

	nb_pkts = RTE_MIN(nb_pkts, bnxt_tx_avail(txq));

	if (unlikely(nb_pkts == 0))
		return 0;

	/* Handle TX burst request */
	to_send = nb_pkts;

	/*
	 * If current descriptor is not on a 32-byte boundary, send one packet
	 * to align for 32-byte stores.
	 */
	if (tx_prod & 1) {
		bnxt_xmit_one(pkts[0], txbd++, tx_buf++);
		to_send--;
		pkts++;
	}

	/*
	 * Send four packets per loop, with a single store for each pair
	 * of descriptors.
	 */
	while (to_send >= BNXT_TX_DESCS_PER_LOOP) {
		uint64_t dsc0_hi, dsc0_lo, dsc1_hi, dsc1_lo;
		uint64_t dsc2_hi, dsc2_lo, dsc3_hi, dsc3_lo;
		__m256i dsc01, dsc23;

		/* Prefetch next transmit buffer descriptors. */
		rte_prefetch0(txbd + 4);
		rte_prefetch0(txbd + 7);

		/* Copy four mbuf pointers to tx buf ring. */
#ifdef RTE_ARCH_X86_64
		__m256i tmp = _mm256_loadu_si256((void *)pkts);
		_mm256_storeu_si256((void *)tx_buf, tmp);
#else
		__m128i tmp = _mm_loadu_si128((void *)pkts);
		_mm_storeu_si128((void *)tx_buf, tmp);
#endif

		dsc0_hi = tx_buf[0]->buf_iova + tx_buf[0]->data_off;
		dsc0_lo = (tx_buf[0]->data_len << 16) |
			  bnxt_xmit_flags_len(tx_buf[0]->data_len,
					      TX_BD_FLAGS_NOCMPL);

		dsc1_hi = tx_buf[1]->buf_iova + tx_buf[1]->data_off;
		dsc1_lo = (tx_buf[1]->data_len << 16) |
			  bnxt_xmit_flags_len(tx_buf[1]->data_len,
					      TX_BD_FLAGS_NOCMPL);

		dsc01 = _mm256_set_epi64x(dsc1_hi, dsc1_lo, dsc0_hi, dsc0_lo);

		dsc2_hi = tx_buf[2]->buf_iova + tx_buf[2]->data_off;
		dsc2_lo = (tx_buf[2]->data_len << 16) |
			  bnxt_xmit_flags_len(tx_buf[2]->data_len,
					      TX_BD_FLAGS_NOCMPL);

		dsc3_hi = tx_buf[3]->buf_iova + tx_buf[3]->data_off;
		dsc3_lo = (tx_buf[3]->data_len << 16) |
			  bnxt_xmit_flags_len(tx_buf[3]->data_len,
					      TX_BD_FLAGS_NOCMPL);

		dsc23 = _mm256_set_epi64x(dsc3_hi, dsc3_lo, dsc2_hi, dsc2_lo);

		_mm256_store_si256((void *)txbd, dsc01);
		_mm256_store_si256((void *)(txbd + 2), dsc23);

		to_send -= BNXT_TX_DESCS_PER_LOOP;
		pkts += BNXT_TX_DESCS_PER_LOOP;
		txbd += BNXT_TX_DESCS_PER_LOOP;
		tx_buf += BNXT_TX_DESCS_PER_LOOP;
	}

	/* Send any remaining packets, writing each descriptor individually. */
	while (to_send) {
		bnxt_xmit_one(pkts[0], txbd++, tx_buf++);
		to_send--;
		pkts++;
	}

	/* Request a completion for the final packet of the burst. */
	txbd[-1].opaque = nb_pkts;
	txbd[-1].flags_type &= ~TX_BD_LONG_FLAGS_NO_CMPL;

	tx_raw_prod += nb_pkts;
	bnxt_db_write(&txr->tx_db, tx_raw_prod);

	txr->tx_raw_prod = tx_raw_prod;

	return nb_pkts;
}

uint16_t
bnxt_xmit_pkts_vec_avx2(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts)
{
	int nb_sent = 0;
	struct bnxt_tx_queue *txq = tx_queue;
	struct bnxt_tx_ring_info *txr = txq->tx_ring;
	uint32_t ring_size = txr->tx_ring_struct->ring_size;

	/* Tx queue was stopped; wait for it to be restarted */
	if (unlikely(!txq->tx_started)) {
		PMD_DRV_LOG_LINE(DEBUG, "Tx q stopped;return");
		return 0;
	}

	/* Handle TX completions */
	if (bnxt_tx_bds_in_hw(txq) >= txq->tx_free_thresh)
		bnxt_handle_tx_cp_vec(txq);

	while (nb_pkts) {
		uint16_t ret, num;

		/*
		 * Ensure that no more than RTE_BNXT_MAX_TX_BURST packets
		 * are transmitted before the next completion.
		 */
		num = RTE_MIN(nb_pkts, RTE_BNXT_MAX_TX_BURST);

		/*
		 * Ensure that a ring wrap does not occur within a call to
		 * bnxt_xmit_fixed_burst_vec().
		 */
		num = RTE_MIN(num, ring_size -
				   (txr->tx_raw_prod & (ring_size - 1)));
		ret = bnxt_xmit_fixed_burst_vec(txq, &tx_pkts[nb_sent], num);
		nb_sent += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_sent;
}

static inline uint16_t
recv_one_vec_avx2_v3(struct rte_mbuf **rx_pkts, uint32_t cons, uint32_t mbcons,
		      uint32_t valid_bit, uint64_t mbuf_init,
		      struct cmpl_base *cp_desc_ring, struct rte_mbuf **rx_buf_ring)
{
	struct rx_pkt_v3_cmpl *rxcmp = (struct rx_pkt_v3_cmpl *)&cp_desc_ring[cons];
	struct rx_pkt_v3_cmpl_hi *rxcmp1 = (struct rx_pkt_v3_cmpl_hi *)&cp_desc_ring[cons + 1];
	uint64_t hi, pkt_type, ol_flags = RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN;
	uint64_t flags_type, flags2, meta, vlan_tci = 0, rss;
	uint32_t tun, tun_ip6, vlan, type, im, im2;
	struct rte_mbuf *mbuf;
	uint64_t *rearm;
	uint16_t error_v2;

	error_v2 = rxcmp1->errors_v2;

	if (!((error_v2 & CMPL_BASE_V) ^ valid_bit))
		return 0;

	rte_compiler_barrier();

	mbuf = rx_buf_ring[mbcons];
	RTE_ASSERT(mbuf != NULL);
	rx_pkts[0] = mbuf;
	rearm = (uint64_t *)&mbuf->rearm_data[0];

	error_v2 &= RX_PKT_CMPL_ERRORS_MASK;
	flags2 = rxcmp1->flags2;
	flags_type = rxcmp->flags_type;
	meta = rxcmp->metadata1_payload_offset;

	im = (error_v2 >> 4) & 0xf;
	ol_flags = errors_to_olflags_v3[1 + im];
	ol_flags &= -(uint64_t)!!(flags2 & RX_PKT_CMPL_CALC);

	rss = rxcmp->rss_hash;
	if (flags_type & RX_PKT_V3_CMPL_FLAGS_RSS_VALID)
		ol_flags |= RTE_MBUF_F_RX_RSS_HASH;

	tun_ip6 = (flags2 & RX_PKT_V3_CMPL_HI_FLAGS2_IP_TYPE) >>
		   RX_PKT_V3_CMPL_FLAGS2_IP_TYPE_SFT;
	tun = (flags2 & BNXT_V3_FLAGS2_TUN_MSK) >> 1;
	tun_ip6 |= tun;
	type = (flags_type & RX_PKT_V3_CMPL_FLAGS_ITYPE_MASK) >>
		RX_PKT_V3_CMPL_FLAGS_ITYPE_SFT;

	im = tun_ip6_lut[tun_ip6];
	im2 = l3_type_lut[type & 0x7];

	vlan = (meta >> RX_PKT_V3_CMPL_METADATA1_VALID_SFT) & 1;
	pkt_type = RTE_PTYPE_L2_ETHER + 5 * vlan;

	im &= im2;

	pkt_type |= -(uint64_t)!(type & 0x8) & im;

	if (vlan) {
		vlan_tci = RX_CMP_V3_METADATA0_VID(rxcmp1);
		ol_flags |= RTE_MBUF_F_RX_VLAN;
		if (rxcmp1->flags2 & RX_PKT_V3_CMPL_HI_FLAGS2_META_FORMAT_MASK)
			ol_flags |= RTE_MBUF_F_RX_VLAN_STRIPPED;
	}
	mbuf->next = NULL;
	hi = rxcmp->len;
	pkt_type |= hi << 32;
	hi |= vlan_tci << 16;
	hi |= rss << 32;

	rearm[0] = mbuf_init;
	rearm[1] = ol_flags;
	rearm[2] = pkt_type;
	rearm[3] = hi;
	return 1;
}

/*
 * V3 (Thor2) RX burst processing - AVX2 vectorized implementation
 *
 * V3 completions have a different layout for checksum and VLAN handling
 * compared to the standard and compressed completion formats.
 * The function will always consume 8 cpr bds and write 8 packets given valid.
 * Bounds are enforced by bnxt_recv_pkts_vec_avx2_v3.
 */
static inline uint16_t
recv_burst_vec_avx2_v3(struct rte_mbuf **rx_pkts, uint16_t nb_pkts,
			uint32_t cons, uint32_t mbcons, uint32_t valid_bit, uint64_t mbuf_init,
			struct cmpl_base *cp_desc_ring, struct rte_mbuf **rx_buf_ring)
{
	const __m256i mask_1s =
		_mm256_set1_epi32(0x1);
	const __m256i mask_fs =
		_mm256_set1_epi32(0xf);
	const __m256i mbuf_init_v =
		_mm256_set1_epi64x(mbuf_init);
	const __m256i tun_ip6_v =
		_mm256_load_si256((const __m256i *)tun_ip6_lut);
	const __m256i l3_type_v =
		_mm256_load_si256((const __m256i *)l3_type_lut);
	const __m256i olflags_v3_v =
		_mm256_load_si256((const __m256i *)olflags_v3_lut);

	__m256i rearm0, rearm1, rearm2, rearm3, rearm4, rearm5, rearm6, rearm7;
	__m256i ptypes, cs_mask, vlan_mask, vlan_s_mask, type, tun_ip6, tun;
	__m256i third_64_0145, third_64_2367, last_64_0145, last_64_2367;
	__m256i lo_0123, lo_4567, hi_0123, hi_4567, rss_0123, rss_4567;
	__m256i ol_flags, ol_flags_0145, ol_flags_2367;
	__m256i t0, t1, vlan_flags, rss_flags;

	const uint32_t valid_mask8 = valid_bit ? 0xffu : 0u;

	int nb_rx_pkts = 0;
	uint32_t valid;
	int i;

	for (i = 0; i < nb_pkts; i += BNXT_RX_DESCS_PER_LOOP_VEC256,
				  cons += BNXT_RX_DESCS_PER_LOOP_VEC256 * 2,
				  mbcons += BNXT_RX_DESCS_PER_LOOP_VEC256) {
		__m256i vlan_tci_len, flags_type, flags2, errors_v2, metadata1, rss, len;
		__m256i desc0, desc1, desc2, desc3, desc4, desc5, desc6, desc7;
		uint32_t num_valid;

		t0 = _mm256_loadu_si256((void *)&rx_buf_ring[mbcons]);
		_mm256_storeu_si256((void *)&rx_pkts[i], t0);
#ifdef RTE_ARCH_X86_64
		t0 = _mm256_loadu_si256((void *)&rx_buf_ring[mbcons + 4]);
		_mm256_storeu_si256((void *)&rx_pkts[i + 4], t0);
#endif

		/*
		 * Load eight receive completion descriptors into 256-bit
		 * registers. Loads are issued in reverse order for consistent state.
		 */
		desc7 = _mm256_load_si256((void *)&cp_desc_ring[cons + 14]);
		rte_compiler_barrier();
		desc6 = _mm256_load_si256((void *)&cp_desc_ring[cons + 12]);
		rte_compiler_barrier();
		desc5 = _mm256_load_si256((void *)&cp_desc_ring[cons + 10]);
		rte_compiler_barrier();
		desc4 = _mm256_load_si256((void *)&cp_desc_ring[cons + 8]);
		rte_compiler_barrier();
		desc3 = _mm256_load_si256((void *)&cp_desc_ring[cons + 6]);
		rte_compiler_barrier();
		desc2 = _mm256_load_si256((void *)&cp_desc_ring[cons + 4]);
		rte_compiler_barrier();
		desc1 = _mm256_load_si256((void *)&cp_desc_ring[cons + 2]);
		rte_compiler_barrier();
		desc0 = _mm256_load_si256((void *)&cp_desc_ring[cons + 0]);

		t0 = _mm256_unpacklo_epi32(desc6, desc7);
		t1 = _mm256_unpacklo_epi32(desc4, desc5);
		lo_4567 = _mm256_unpacklo_epi64(t1, t0);

		t0 = _mm256_unpackhi_epi32(desc6, desc7);
		t1 = _mm256_unpackhi_epi32(desc4, desc5);
		hi_4567 = _mm256_unpacklo_epi64(t1, t0);
		rss_4567 = _mm256_unpackhi_epi64(t1, t0);

		t0 = _mm256_unpacklo_epi32(desc2, desc3);
		t1 = _mm256_unpacklo_epi32(desc0, desc1);
		lo_0123 = _mm256_unpacklo_epi64(t1, t0);

		t0 = _mm256_unpackhi_epi32(desc2, desc3);
		t1 = _mm256_unpackhi_epi32(desc0, desc1);
		hi_0123 = _mm256_unpacklo_epi64(t1, t0);
		rss_0123 = _mm256_unpackhi_epi64(t1, t0);

		errors_v2 = _mm256_permute2f128_si256(hi_0123, hi_4567, 0x31);
		metadata1 = _mm256_permute2f128_si256(hi_0123, hi_4567, 0x20);
		flags2 = _mm256_permute2f128_si256(lo_0123, lo_4567, 0x31);
		flags_type = _mm256_permute2f128_si256(lo_0123, lo_4567, 0x20);
		rss = _mm256_permute2f128_si256(rss_0123, rss_4567, 0x20);

		len = _mm256_srli_epi32(flags_type, 16);

		valid = _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_slli_epi32(errors_v2, 31))) ^
			valid_mask8;
		num_valid = rte_popcount32(valid);

		if (num_valid == 0)
			break;

		rte_compiler_barrier();

		t0 = _mm256_srli_epi32(errors_v2, 4);
		t0 = _mm256_permutevar8x32_epi32(olflags_v3_v, t0);

		t1 = _mm256_and_si256(errors_v2,
				      _mm256_set1_epi32(RX_PKT_CMPL_ERRORS_T_L4_CS_ERROR));
		t1 = _mm256_slli_epi32(t1, 14);
		ol_flags = _mm256_sub_epi32(_mm256_set1_epi32(RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD),
					    t1);
		ol_flags = _mm256_or_si256(ol_flags, t0);

		/* mask_fs is used in place of RX_PKT_V3_CMPL_FLAGS_ITYPE_MASK */
		type = _mm256_and_si256(_mm256_srli_epi32(flags_type,
					RX_PKT_V3_CMPL_FLAGS_ITYPE_SFT), mask_fs);

		rss_flags = _mm256_and_si256(flags_type,
					     _mm256_set1_epi32(RX_PKT_V3_CMPL_FLAGS_RSS_VALID));
		rss_flags = _mm256_srli_epi32(rss_flags, 9);
		t1 = _mm256_cmpgt_epi32(_mm256_set1_epi32(8), type);
		type = _mm256_permutevar8x32_epi32(l3_type_v, type);

		/* mask_fs used for RX_PKT_CMPL_CALC */
		cs_mask = _mm256_and_si256(flags2, mask_fs);
		cs_mask = _mm256_slli_epi32(cs_mask, 1);
		cs_mask = _mm256_cmpgt_epi32(cs_mask, mask_1s);
		ol_flags = _mm256_and_si256(ol_flags, cs_mask);
		ol_flags = _mm256_or_si256(ol_flags, rss_flags);

		vlan_s_mask = _mm256_and_si256(flags2,
				_mm256_set1_epi32(RX_PKT_V3_CMPL_HI_FLAGS2_META_FORMAT_MASK));
		/* mask_1s is used in place of RX_PKT_V3_CMPL_HI_FLAGS2_IP_TYPE */
		tun_ip6 = _mm256_and_si256(_mm256_srli_epi32(flags2,
					   RX_PKT_V3_CMPL_FLAGS2_IP_TYPE_SFT), mask_1s);
		tun = _mm256_and_si256(flags2, _mm256_set1_epi32(BNXT_V3_FLAGS2_TUN_MSK));
		tun = _mm256_srli_epi32(tun, 1);
		tun_ip6 = _mm256_or_si256(tun_ip6, tun);
		tun_ip6 = _mm256_permutevar8x32_epi32(tun_ip6_v, tun_ip6);
		vlan_mask = _mm256_srai_epi32(metadata1, 31);
		vlan_tci_len = _mm256_and_si256(vlan_mask, errors_v2);
		vlan_tci_len = _mm256_blend_epi16(len, vlan_tci_len, 0xaa);
		vlan_s_mask = _mm256_cmpgt_epi32(vlan_s_mask, mask_1s);
		ptypes = _mm256_blendv_epi8(_mm256_set1_epi32(RTE_PTYPE_L2_ETHER),
			_mm256_set1_epi32(RTE_PTYPE_L2_ETHER_VLAN), vlan_mask);
		vlan_s_mask = _mm256_and_si256(vlan_s_mask,
					       _mm256_set1_epi32(RTE_MBUF_F_RX_VLAN_STRIPPED));
		/* mask_1s is used in place of RTE_MBUF_F_RX_VLAN */
		vlan_flags = _mm256_or_si256(mask_1s, vlan_s_mask);
		vlan_flags = _mm256_and_si256(vlan_mask, vlan_flags);
		t0 = _mm256_and_si256(type, tun_ip6);
		t0 = _mm256_and_si256(t0, t1);
		ptypes = _mm256_or_si256(t0, ptypes);
		ol_flags = _mm256_or_si256(ol_flags, vlan_flags);

		third_64_0145 = _mm256_unpacklo_epi32(ptypes, len);
		third_64_2367 = _mm256_unpackhi_epi32(ptypes, len);

		last_64_0145 = _mm256_unpacklo_epi32(vlan_tci_len, rss);
		last_64_2367 = _mm256_unpackhi_epi32(vlan_tci_len, rss);

		t1 = _mm256_setzero_si256();
		ol_flags_0145 = _mm256_unpacklo_epi32(ol_flags, t1);
		ol_flags_2367 = _mm256_unpackhi_epi32(ol_flags, t1);

		ol_flags_0145 = _mm256_permute2f128_si256(ol_flags_0145,
							  ol_flags_0145, 0x01);
		ol_flags_2367 = _mm256_permute2f128_si256(ol_flags_2367,
							  ol_flags_2367, 0x01);

		rearm4 = _mm256_unpacklo_epi64(mbuf_init_v, ol_flags_0145);
		t1 = _mm256_unpacklo_epi64(third_64_0145, last_64_0145);
		rearm0 = _mm256_permute2f128_si256(rearm4, t1, 0x21);
		rearm4 = _mm256_blend_epi32(rearm4, t1, 0xf0);

		rearm5 = _mm256_unpackhi_epi64(mbuf_init_v, ol_flags_0145);
		t1 = _mm256_unpackhi_epi64(third_64_0145, last_64_0145);
		rearm1 = _mm256_permute2f128_si256(rearm5, t1, 0x21);
		rearm5 = _mm256_blend_epi32(rearm5, t1, 0xf0);

		rearm6 = _mm256_unpacklo_epi64(mbuf_init_v, ol_flags_2367);
		t1 = _mm256_unpacklo_epi64(third_64_2367, last_64_2367);
		rearm2 = _mm256_permute2f128_si256(rearm6, t1, 0x21);
		rearm6 = _mm256_blend_epi32(rearm6, t1, 0xf0);

		rearm7 = _mm256_unpackhi_epi64(mbuf_init_v, ol_flags_2367);
		t1 = _mm256_unpackhi_epi64(third_64_2367, last_64_2367);
		rearm3 = _mm256_permute2f128_si256(rearm7, t1, 0x21);
		rearm7 = _mm256_blend_epi32(rearm7, t1, 0xf0);

		_mm256_storeu_si256((void *)&rx_pkts[i + 0]->rearm_data,
				    rearm0);
		_mm256_storeu_si256((void *)&rx_pkts[i + 1]->rearm_data,
				    rearm1);
		_mm256_storeu_si256((void *)&rx_pkts[i + 2]->rearm_data,
				    rearm2);
		_mm256_storeu_si256((void *)&rx_pkts[i + 3]->rearm_data,
				    rearm3);
		_mm256_storeu_si256((void *)&rx_pkts[i + 4]->rearm_data,
				    rearm4);
		_mm256_storeu_si256((void *)&rx_pkts[i + 5]->rearm_data,
				    rearm5);
		_mm256_storeu_si256((void *)&rx_pkts[i + 6]->rearm_data,
				    rearm6);
		_mm256_storeu_si256((void *)&rx_pkts[i + 7]->rearm_data,
				    rearm7);

		nb_rx_pkts += num_valid;
		if (num_valid < BNXT_RX_DESCS_PER_LOOP_VEC256)
			break;
	}

	return nb_rx_pkts;
}

uint16_t
bnxt_recv_pkts_vec_avx2_v3(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts)
{
	struct bnxt_rx_queue *rxq = rx_queue;
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint32_t cp_ring_size = cpr->cp_ring_struct->ring_size;
	uint32_t rx_ring_size = rxr->rx_ring_struct->ring_size;
	struct cmpl_base *cp_desc_ring = cpr->cp_desc_ring;
	struct rte_mbuf **rx_buf_ring = rxr->rx_buf_ring;
	uint32_t chunk = RTE_MAX((uint32_t)rxq->rx_free_thresh,
				 BNXT_RX_DESCS_PER_LOOP_VEC256);
	uint16_t cnt = 0;

	if (unlikely(!rxq->rx_started))
		return 0;

	while (nb_pkts) {
		uint32_t cons, mbcons, valid_bit, num, num_vec, received = 0;

		if (rxq->rxrearm_nb >= rxq->rx_free_thresh)
			bnxt_rxq_rearm(rxq, rxr);

		valid_bit = !!(cpr->cp_raw_cons & cp_ring_size);
		cons = cpr->cp_raw_cons & (cp_ring_size - 1);
		mbcons = (cpr->cp_raw_cons / 2) & (rx_ring_size - 1);

		/* Never wrap either ring within one pass. */
		num = RTE_MIN((uint32_t)nb_pkts, chunk);
		num = RTE_MIN(num, rx_ring_size - mbcons);
		num = RTE_MIN(num, (cp_ring_size - cons) / 2);

		num_vec = RTE_ALIGN_FLOOR(num, BNXT_RX_DESCS_PER_LOOP_VEC256);

		if (num_vec)
			received = recv_burst_vec_avx2_v3(rx_pkts + cnt,
						     num_vec, cons, mbcons,
						     valid_bit,
						     rxq->mbuf_initializer,
						     cp_desc_ring, rx_buf_ring);

		/* Only chase the tail if the vector loops all filled. */
		if (received == num_vec) {
			while (received < num &&
				recv_one_vec_avx2_v3(rx_pkts + cnt + received,
						     cons + 2 * received, mbcons + received,
						     valid_bit,
						     rxq->mbuf_initializer,
						     cp_desc_ring, rx_buf_ring))
				received++;
		}

		if (received == 0)
			break;

		rxr->rx_raw_prod = RING_ADV(rxr->rx_raw_prod, received);
		rxq->rxrearm_nb += received;
		cpr->cp_raw_cons += 2 * received;
		bnxt_db_cq(cpr);

		cnt += received;
		nb_pkts -= received;

		if (received < num)
			break;
	}

	return cnt;
}
