/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2019-2020 Broadcom All rights reserved. */

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

/*
 * RX Ring handling
 */

#define GET_OL_FLAGS(rss_flags, ol_index, errors, pi, ol_flags)		       \
{									       \
	uint32_t tmp, of;						       \
									       \
	of = _mm_extract_epi32((rss_flags), (pi)) |			       \
		rxr->ol_flags_table[_mm_extract_epi32((ol_index), (pi))];      \
									       \
	tmp = _mm_extract_epi32((errors), (pi));			       \
	if (tmp)							       \
		of |= rxr->ol_flags_err_table[tmp];			       \
	(ol_flags) = of;						       \
}

#define GET_DESC_FIELDS(rxcmp, rxcmp1, shuf_msk, ptype_idx, pi, ret)	       \
{									       \
	uint32_t ptype;							       \
	__m128i r;							       \
									       \
	/* Set mbuf pkt_len, data_len, and rss_hash fields. */		       \
	r = _mm_shuffle_epi8((rxcmp), (shuf_msk));			       \
									       \
	/* Set packet type. */						       \
	ptype = bnxt_ptype_table[_mm_extract_epi32((ptype_idx), (pi))];	       \
	r = _mm_blend_epi16(r, _mm_set_epi32(0, 0, 0, ptype), 0x3);	       \
									       \
	/* Set vlan_tci. */						       \
	r = _mm_blend_epi16(r, _mm_slli_si128((rxcmp1), 6), 0x20);	       \
	(ret) = r;							       \
}

static inline void
descs_to_mbufs(__m128i mm_rxcmp[4], __m128i mm_rxcmp1[4],
	       __m128i mbuf_init, struct rte_mbuf **mbuf,
	       struct bnxt_rx_ring_info *rxr)
{
	const __m128i shuf_msk =
		_mm_set_epi8(15, 14, 13, 12,          /* rss */
			     0xFF, 0xFF,              /* vlan_tci (zeroes) */
			     3, 2,                    /* data_len */
			     0xFF, 0xFF, 3, 2,        /* pkt_len */
			     0xFF, 0xFF, 0xFF, 0xFF); /* pkt_type (zeroes) */
	const __m128i flags_type_mask =
		_mm_set1_epi32(RX_PKT_CMPL_FLAGS_ITYPE_MASK);
	const __m128i flags2_mask1 =
		_mm_set1_epi32(RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN |
			       RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC);
	const __m128i flags2_mask2 =
		_mm_set1_epi32(RX_PKT_CMPL_FLAGS2_IP_TYPE);
	const __m128i rss_mask =
		_mm_set1_epi32(RX_PKT_CMPL_FLAGS_RSS_VALID);
	__m128i t0, t1, flags_type, flags2, index, errors, rss_flags;
	__m128i ptype_idx, is_tunnel;
	uint32_t ol_flags;

	/* Compute packet type table indexes for four packets */
	t0 = _mm_unpacklo_epi32(mm_rxcmp[0], mm_rxcmp[1]);
	t1 = _mm_unpacklo_epi32(mm_rxcmp[2], mm_rxcmp[3]);
	flags_type = _mm_unpacklo_epi64(t0, t1);
	ptype_idx =
		_mm_srli_epi32(_mm_and_si128(flags_type, flags_type_mask), 9);

	t0 = _mm_unpacklo_epi32(mm_rxcmp1[0], mm_rxcmp1[1]);
	t1 = _mm_unpacklo_epi32(mm_rxcmp1[2], mm_rxcmp1[3]);
	flags2 = _mm_unpacklo_epi64(t0, t1);

	ptype_idx = _mm_or_si128(ptype_idx,
			_mm_srli_epi32(_mm_and_si128(flags2, flags2_mask1), 2));
	ptype_idx = _mm_or_si128(ptype_idx,
			_mm_srli_epi32(_mm_and_si128(flags2, flags2_mask2), 7));

	/* Extract RSS valid flags for four packets. */
	rss_flags = _mm_srli_epi32(_mm_and_si128(flags_type, rss_mask), 9);

	/* Extract errors_v2 fields for four packets. */
	t0 = _mm_unpackhi_epi32(mm_rxcmp1[0], mm_rxcmp1[1]);
	t1 = _mm_unpackhi_epi32(mm_rxcmp1[2], mm_rxcmp1[3]);

	/* Compute ol_flags and checksum error indexes for four packets. */
	is_tunnel = _mm_and_si128(flags2, _mm_set1_epi32(4));
	is_tunnel = _mm_slli_epi32(is_tunnel, 3);
	flags2 = _mm_and_si128(flags2, _mm_set1_epi32(0x1F));

	errors = _mm_srli_epi32(_mm_unpacklo_epi64(t0, t1), 4);
	errors = _mm_and_si128(errors, _mm_set1_epi32(0xF));
	errors = _mm_and_si128(errors, flags2);

	index = _mm_andnot_si128(errors, flags2);
	errors = _mm_or_si128(errors, _mm_srli_epi32(is_tunnel, 1));
	index = _mm_or_si128(index, is_tunnel);

	/* Update mbuf rearm_data for four packets. */
	GET_OL_FLAGS(rss_flags, index, errors, 0, ol_flags);
	_mm_store_si128((void *)&mbuf[0]->rearm_data,
			_mm_or_si128(mbuf_init, _mm_set_epi64x(ol_flags, 0)));

	GET_OL_FLAGS(rss_flags, index, errors, 1, ol_flags);
	_mm_store_si128((void *)&mbuf[1]->rearm_data,
			_mm_or_si128(mbuf_init, _mm_set_epi64x(ol_flags, 0)));

	GET_OL_FLAGS(rss_flags, index, errors, 2, ol_flags);
	_mm_store_si128((void *)&mbuf[2]->rearm_data,
			_mm_or_si128(mbuf_init, _mm_set_epi64x(ol_flags, 0)));

	GET_OL_FLAGS(rss_flags, index, errors, 3, ol_flags);
	_mm_store_si128((void *)&mbuf[3]->rearm_data,
			_mm_or_si128(mbuf_init, _mm_set_epi64x(ol_flags, 0)));

	/* Update mbuf rx_descriptor_fields1 for four packes. */
	GET_DESC_FIELDS(mm_rxcmp[0], mm_rxcmp1[0], shuf_msk, ptype_idx, 0, t0);
	_mm_store_si128((void *)&mbuf[0]->rx_descriptor_fields1, t0);

	GET_DESC_FIELDS(mm_rxcmp[1], mm_rxcmp1[1], shuf_msk, ptype_idx, 1, t0);
	_mm_store_si128((void *)&mbuf[1]->rx_descriptor_fields1, t0);

	GET_DESC_FIELDS(mm_rxcmp[2], mm_rxcmp1[2], shuf_msk, ptype_idx, 2, t0);
	_mm_store_si128((void *)&mbuf[2]->rx_descriptor_fields1, t0);

	GET_DESC_FIELDS(mm_rxcmp[3], mm_rxcmp1[3], shuf_msk, ptype_idx, 3, t0);
	_mm_store_si128((void *)&mbuf[3]->rx_descriptor_fields1, t0);
}

static uint16_t
recv_burst_vec_sse(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct bnxt_rx_queue *rxq = rx_queue;
	const __m128i mbuf_init = _mm_set_epi64x(0, rxq->mbuf_initializer);
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint16_t cp_ring_size = cpr->cp_ring_struct->ring_size;
	uint16_t rx_ring_size = rxr->rx_ring_struct->ring_size;
	struct cmpl_base *cp_desc_ring = cpr->cp_desc_ring;
	uint64_t valid, desc_valid_mask = ~0ULL;
	const __m128i info3_v_mask = _mm_set1_epi32(CMPL_BASE_V);
	uint32_t raw_cons = cpr->cp_raw_cons;
	uint32_t cons, mbcons;
	int nb_rx_pkts = 0;
	const __m128i valid_target =
		_mm_set1_epi32(!!(raw_cons & cp_ring_size));
	int i;

	/* If Rx Q was stopped return */
	if (unlikely(!rxq->rx_started))
		return 0;

	if (rxq->rxrearm_nb >= rxq->rx_free_thresh)
		bnxt_rxq_rearm(rxq, rxr);

	cons = raw_cons & (cp_ring_size - 1);
	mbcons = (raw_cons / 2) & (rx_ring_size - 1);

	/* Prefetch first four descriptor pairs. */
	rte_prefetch0(&cp_desc_ring[cons]);
	rte_prefetch0(&cp_desc_ring[cons + 4]);

	/* Ensure that we do not go past the ends of the rings. */
	nb_pkts = RTE_MIN(nb_pkts, RTE_MIN(rx_ring_size - mbcons,
					   (cp_ring_size - cons) / 2));
	/*
	 * If we are at the end of the ring, ensure that descriptors after the
	 * last valid entry are not treated as valid. Otherwise, force the
	 * maximum number of packets to receive to be a multiple of the per-
	 * loop count.
	 */
	if (nb_pkts < RTE_BNXT_DESCS_PER_LOOP)
		desc_valid_mask >>= 16 * (RTE_BNXT_DESCS_PER_LOOP - nb_pkts);
	else
		nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, RTE_BNXT_DESCS_PER_LOOP);

	/* Handle RX burst request */
	for (i = 0; i < nb_pkts; i += RTE_BNXT_DESCS_PER_LOOP,
				  cons += RTE_BNXT_DESCS_PER_LOOP * 2,
				  mbcons += RTE_BNXT_DESCS_PER_LOOP) {
		__m128i rxcmp1[RTE_BNXT_DESCS_PER_LOOP];
		__m128i rxcmp[RTE_BNXT_DESCS_PER_LOOP];
		__m128i tmp0, tmp1, info3_v;
		uint32_t num_valid;

		/* Copy four mbuf pointers to output array. */
		tmp0 = _mm_loadu_si128((void *)&rxr->rx_buf_ring[mbcons]);
#ifdef RTE_ARCH_X86_64
		tmp1 = _mm_loadu_si128((void *)&rxr->rx_buf_ring[mbcons + 2]);
#endif
		_mm_storeu_si128((void *)&rx_pkts[i], tmp0);
#ifdef RTE_ARCH_X86_64
		_mm_storeu_si128((void *)&rx_pkts[i + 2], tmp1);
#endif

		/* Prefetch four descriptor pairs for next iteration. */
		if (i + RTE_BNXT_DESCS_PER_LOOP < nb_pkts) {
			rte_prefetch0(&cp_desc_ring[cons + 8]);
			rte_prefetch0(&cp_desc_ring[cons + 12]);
		}

		/*
		 * Load the four current descriptors into SSE registers in
		 * reverse order to ensure consistent state.
		 */
		rxcmp1[3] = _mm_load_si128((void *)&cp_desc_ring[cons + 7]);
		rte_compiler_barrier();
		rxcmp[3] = _mm_load_si128((void *)&cp_desc_ring[cons + 6]);

		rxcmp1[2] = _mm_load_si128((void *)&cp_desc_ring[cons + 5]);
		rte_compiler_barrier();
		rxcmp[2] = _mm_load_si128((void *)&cp_desc_ring[cons + 4]);

		tmp1 = _mm_unpackhi_epi32(rxcmp1[2], rxcmp1[3]);

		rxcmp1[1] = _mm_load_si128((void *)&cp_desc_ring[cons + 3]);
		rte_compiler_barrier();
		rxcmp[1] = _mm_load_si128((void *)&cp_desc_ring[cons + 2]);

		rxcmp1[0] = _mm_load_si128((void *)&cp_desc_ring[cons + 1]);
		rte_compiler_barrier();
		rxcmp[0] = _mm_load_si128((void *)&cp_desc_ring[cons + 0]);

		tmp0 = _mm_unpackhi_epi32(rxcmp1[0], rxcmp1[1]);

		/* Isolate descriptor valid flags. */
		info3_v = _mm_and_si128(_mm_unpacklo_epi64(tmp0, tmp1),
					info3_v_mask);
		info3_v = _mm_xor_si128(info3_v, valid_target);

		/*
		 * Pack the 128-bit array of valid descriptor flags into 64
		 * bits and count the number of set bits in order to determine
		 * the number of valid descriptors.
		 */
		valid = _mm_cvtsi128_si64(_mm_packs_epi32(info3_v, info3_v));
		num_valid = __builtin_popcountll(valid & desc_valid_mask);

		switch (num_valid) {
		case 4:
			rxr->rx_buf_ring[mbcons + 3] = NULL;
			/* FALLTHROUGH */
		case 3:
			rxr->rx_buf_ring[mbcons + 2] = NULL;
			/* FALLTHROUGH */
		case 2:
			rxr->rx_buf_ring[mbcons + 1] = NULL;
			/* FALLTHROUGH */
		case 1:
			rxr->rx_buf_ring[mbcons + 0] = NULL;
			break;
		case 0:
			goto out;
		}

		descs_to_mbufs(rxcmp, rxcmp1, mbuf_init, &rx_pkts[nb_rx_pkts],
			       rxr);
		nb_rx_pkts += num_valid;

		if (num_valid < RTE_BNXT_DESCS_PER_LOOP)
			break;
	}

out:
	if (nb_rx_pkts) {
		rxr->rx_prod =
			RING_ADV(rxr->rx_ring_struct, rxr->rx_prod, nb_rx_pkts);

		rxq->rxrearm_nb += nb_rx_pkts;
		cpr->cp_raw_cons += 2 * nb_rx_pkts;
		cpr->valid =
			!!(cpr->cp_raw_cons & cpr->cp_ring_struct->ring_size);
		bnxt_db_cq(cpr);
	}

	return nb_rx_pkts;
}

uint16_t
bnxt_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	uint16_t cnt = 0;

	while (nb_pkts > RTE_BNXT_MAX_RX_BURST) {
		uint16_t burst;

		burst = recv_burst_vec_sse(rx_queue, rx_pkts + cnt,
					   RTE_BNXT_MAX_RX_BURST);

		cnt += burst;
		nb_pkts -= burst;

		if (burst < RTE_BNXT_MAX_RX_BURST)
			return cnt;
	}

	return cnt + recv_burst_vec_sse(rx_queue, rx_pkts + cnt, nb_pkts);
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

		if (likely(CMP_TYPE(txcmp) == TX_CMPL_TYPE_TX_L2))
			nb_tx_pkts += txcmp->opaque;
		else
			RTE_LOG_DP(ERR, PMD,
				   "Unhandled CMP type %02x\n",
				   CMP_TYPE(txcmp));
		raw_cons = NEXT_RAW_CMP(raw_cons);
	} while (nb_tx_pkts < ring_mask);

	cpr->valid = !!(raw_cons & cp_ring_struct->ring_size);
	if (nb_tx_pkts) {
		if (txq->offloads & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
			bnxt_tx_cmp_vec_fast(txq, nb_tx_pkts);
		else
			bnxt_tx_cmp_vec(txq, nb_tx_pkts);
		cpr->cp_raw_cons = raw_cons;
		bnxt_db_cq(cpr);
	}
}

static inline void
bnxt_xmit_one(struct rte_mbuf *mbuf, struct tx_bd_long *txbd,
	      struct bnxt_sw_tx_bd *tx_buf)
{
	__m128i desc;

	tx_buf->mbuf = mbuf;
	tx_buf->nr_bds = 1;

	desc = _mm_set_epi64x(mbuf->buf_iova + mbuf->data_off,
			      bnxt_xmit_flags_len(mbuf->data_len,
						  TX_BD_FLAGS_NOCMPL));
	desc = _mm_blend_epi16(desc, _mm_set_epi16(0, 0, 0, 0, 0, 0,
						   mbuf->data_len, 0), 0x02);
	_mm_store_si128((void *)txbd, desc);
}

static uint16_t
bnxt_xmit_fixed_burst_vec(struct bnxt_tx_queue *txq, struct rte_mbuf **tx_pkts,
			  uint16_t nb_pkts)
{
	struct bnxt_tx_ring_info *txr = txq->tx_ring;
	uint16_t tx_prod = txr->tx_prod;
	struct tx_bd_long *txbd;
	struct bnxt_sw_tx_bd *tx_buf;
	uint16_t to_send;

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
	while (to_send >= RTE_BNXT_DESCS_PER_LOOP) {
		/* Prefetch next transmit buffer descriptors. */
		rte_prefetch0(txbd + 4);
		rte_prefetch0(txbd + 7);

		bnxt_xmit_one(tx_pkts[0], txbd++, tx_buf++);
		bnxt_xmit_one(tx_pkts[1], txbd++, tx_buf++);
		bnxt_xmit_one(tx_pkts[2], txbd++, tx_buf++);
		bnxt_xmit_one(tx_pkts[3], txbd++, tx_buf++);

		to_send -= RTE_BNXT_DESCS_PER_LOOP;
		tx_pkts += RTE_BNXT_DESCS_PER_LOOP;
	}

	while (to_send) {
		bnxt_xmit_one(tx_pkts[0], txbd++, tx_buf++);
		to_send--;
		tx_pkts++;
	}

	/* Request a completion for the final packet of burst. */
	rte_compiler_barrier();
	txbd[-1].opaque = nb_pkts;
	txbd[-1].flags_type &= ~TX_BD_LONG_FLAGS_NO_CMPL;

	tx_prod = RING_ADV(txr->tx_ring_struct, tx_prod, nb_pkts);
	bnxt_db_write(&txr->tx_db, tx_prod);

	txr->tx_prod = tx_prod;

	return nb_pkts;
}

uint16_t
bnxt_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
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
		num = RTE_MIN(num,
			      ring_size - (txr->tx_prod & (ring_size - 1)));
		ret = bnxt_xmit_fixed_burst_vec(txq, &tx_pkts[nb_sent], num);
		nb_sent += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_sent;
}

int __rte_cold
bnxt_rxq_vec_setup(struct bnxt_rx_queue *rxq)
{
	return bnxt_rxq_vec_setup_common(rxq);
}
