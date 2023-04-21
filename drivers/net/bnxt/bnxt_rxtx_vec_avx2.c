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

static uint16_t
recv_burst_vec_avx2(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct bnxt_rx_queue *rxq = rx_queue;
	const __m256i mbuf_init =
		_mm256_set_epi64x(0, 0, 0, rxq->mbuf_initializer);
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint16_t cp_ring_size = cpr->cp_ring_struct->ring_size;
	uint16_t rx_ring_size = rxr->rx_ring_struct->ring_size;
	struct cmpl_base *cp_desc_ring = cpr->cp_desc_ring;
	uint64_t valid, desc_valid_mask = ~0ULL;
	const __m256i info3_v_mask = _mm256_set1_epi32(CMPL_BASE_V);
	uint32_t raw_cons = cpr->cp_raw_cons;
	uint32_t cons, mbcons;
	int nb_rx_pkts = 0;
	int i;
	const __m256i valid_target =
		_mm256_set1_epi32(!!(raw_cons & cp_ring_size));
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
	__m256i t0, t1, flags_type, flags2, index, errors;
	__m256i ptype_idx, ptypes, is_tunnel;
	__m256i mbuf01, mbuf23, mbuf45, mbuf67;
	__m256i rearm0, rearm1, rearm2, rearm3, rearm4, rearm5, rearm6, rearm7;
	__m256i ol_flags, ol_flags_hi;
	__m256i rss_flags;

	/* Validate ptype table indexing at build time. */
	bnxt_check_ptype_constants();

	/* If Rx Q was stopped return */
	if (unlikely(!rxq->rx_started))
		return 0;

	if (rxq->rxrearm_nb >= rxq->rx_free_thresh)
		bnxt_rxq_rearm(rxq, rxr);

	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, BNXT_RX_DESCS_PER_LOOP_VEC256);

	cons = raw_cons & (cp_ring_size - 1);
	mbcons = (raw_cons / 2) & (rx_ring_size - 1);

	/* Return immediately if there is not at least one completed packet. */
	if (!bnxt_cpr_cmp_valid(&cp_desc_ring[cons], raw_cons, cp_ring_size))
		return 0;

	/* Ensure that we do not go past the ends of the rings. */
	nb_pkts = RTE_MIN(nb_pkts, RTE_MIN(rx_ring_size - mbcons,
					   (cp_ring_size - cons) / 2));
	/*
	 * If we are at the end of the ring, ensure that descriptors after the
	 * last valid entry are not treated as valid. Otherwise, force the
	 * maximum number of packets to receive to be a multiple of the per-
	 * loop count.
	 */
	if (nb_pkts < BNXT_RX_DESCS_PER_LOOP_VEC256) {
		desc_valid_mask >>=
			CHAR_BIT * (BNXT_RX_DESCS_PER_LOOP_VEC256 - nb_pkts);
	} else {
		nb_pkts =
			RTE_ALIGN_FLOOR(nb_pkts, BNXT_RX_DESCS_PER_LOOP_VEC256);
	}

	/* Handle RX burst request */
	for (i = 0; i < nb_pkts; i += BNXT_RX_DESCS_PER_LOOP_VEC256,
				  cons += BNXT_RX_DESCS_PER_LOOP_VEC256 * 2,
				  mbcons += BNXT_RX_DESCS_PER_LOOP_VEC256) {
		__m256i desc0, desc1, desc2, desc3, desc4, desc5, desc6, desc7;
		__m256i rxcmp0_1, rxcmp2_3, rxcmp4_5, rxcmp6_7, info3_v;
		__m256i errors_v2;
		uint32_t num_valid;

		/* Copy eight mbuf pointers to output array. */
		t0 = _mm256_loadu_si256((void *)&rxr->rx_buf_ring[mbcons]);
		_mm256_storeu_si256((void *)&rx_pkts[i], t0);
#ifdef RTE_ARCH_X86_64
		t0 = _mm256_loadu_si256((void *)&rxr->rx_buf_ring[mbcons + 4]);
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
		ptype_idx = _mm256_and_si256(flags_type, flags_type_mask);
		ptype_idx = _mm256_srli_epi32(ptype_idx,
					      RX_PKT_CMPL_FLAGS_ITYPE_SFT -
					      BNXT_PTYPE_TBL_TYPE_SFT);

		t0 = _mm256_unpacklo_epi32(rxcmp0_1, rxcmp2_3);
		t1 = _mm256_unpacklo_epi32(rxcmp4_5, rxcmp6_7);
		flags2 = _mm256_unpackhi_epi64(t0, t1);

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
		const __m256i perm_msk =
				_mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);
		info3_v = _mm256_permutevar8x32_epi32(errors_v2, perm_msk);
		info3_v = _mm256_and_si256(errors_v2, info3_v_mask);
		info3_v = _mm256_xor_si256(info3_v, valid_target);

		info3_v = _mm256_packs_epi32(info3_v, _mm256_setzero_si256());
		valid = _mm_cvtsi128_si64(_mm256_extracti128_si256(info3_v, 1));
		valid = (valid << CHAR_BIT) |
			_mm_cvtsi128_si64(_mm256_castsi256_si128(info3_v));
		num_valid = __builtin_popcountll(valid & desc_valid_mask);

		if (num_valid == 0)
			break;

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
		rearm0 = _mm256_permute2f128_si256(mbuf_init, mbuf01, 0x20);
		rearm1 = _mm256_blend_epi32(mbuf_init, mbuf01, 0xF0);
		rearm2 = _mm256_permute2f128_si256(mbuf_init, mbuf23, 0x20);
		rearm3 = _mm256_blend_epi32(mbuf_init, mbuf23, 0xF0);

		/* Compute final ol_flags values for eight packets. */
		rss_flags = _mm256_and_si256(flags_type, rss_mask);
		rss_flags = _mm256_srli_epi32(rss_flags, 9);
		ol_flags = _mm256_or_si256(ol_flags, errors);
		ol_flags = _mm256_or_si256(ol_flags, rss_flags);
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
		rearm4 = _mm256_permute2f128_si256(mbuf_init, mbuf45, 0x20);
		rearm5 = _mm256_blend_epi32(mbuf_init, mbuf45, 0xF0);
		rearm6 = _mm256_permute2f128_si256(mbuf_init, mbuf67, 0x20);
		rearm7 = _mm256_blend_epi32(mbuf_init, mbuf67, 0xF0);

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

	if (nb_rx_pkts) {
		rxr->rx_raw_prod = RING_ADV(rxr->rx_raw_prod, nb_rx_pkts);

		rxq->rxrearm_nb += nb_rx_pkts;
		cpr->cp_raw_cons += 2 * nb_rx_pkts;
		bnxt_db_cq(cpr);
	}

	return nb_rx_pkts;
}

uint16_t
bnxt_recv_pkts_vec_avx2(void *rx_queue, struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	uint16_t cnt = 0;

	while (nb_pkts > RTE_BNXT_MAX_RX_BURST) {
		uint16_t burst;

		burst = recv_burst_vec_avx2(rx_queue, rx_pkts + cnt,
					     RTE_BNXT_MAX_RX_BURST);

		cnt += burst;
		nb_pkts -= burst;

		if (burst < RTE_BNXT_MAX_RX_BURST)
			return cnt;
	}
	return cnt + recv_burst_vec_avx2(rx_queue, rx_pkts + cnt, nb_pkts);
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

	do {
		cons = RING_CMPL(ring_mask, raw_cons);
		txcmp = (struct tx_cmpl *)&cp_desc_ring[cons];

		if (!bnxt_cpr_cmp_valid(txcmp, raw_cons, ring_mask + 1))
			break;

		nb_tx_pkts += txcmp->opaque;
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
	uint16_t ring_size = txr->tx_ring_struct->ring_size;

	/* Tx queue was stopped; wait for it to be restarted */
	if (unlikely(!txq->tx_started)) {
		PMD_DRV_LOG(DEBUG, "Tx q stopped;return\n");
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
