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
#include "bnxt_rxtx_vec_common.h"

#include "bnxt_txq.h"
#include "bnxt_txr.h"

/*
 * RX Ring handling
 */

static uint32_t
bnxt_parse_pkt_type(uint32x4_t mm_rxcmp, uint32x4_t mm_rxcmp1)
{
	uint32_t flags_type, flags2;
	uint8_t index;

	flags_type = vgetq_lane_u32(mm_rxcmp, 0);
	flags2 = (uint16_t)vgetq_lane_u32(mm_rxcmp1, 0);

	/*
	 * Index format:
	 *     bit 0: RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC
	 *     bit 1: RX_CMPL_FLAGS2_IP_TYPE
	 *     bit 2: RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN
	 *     bits 3-6: RX_PKT_CMPL_FLAGS_ITYPE
	 */
	index = ((flags_type & RX_PKT_CMPL_FLAGS_ITYPE_MASK) >> 9) |
		((flags2 & (RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN |
			   RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC)) >> 2) |
		((flags2 & RX_PKT_CMPL_FLAGS2_IP_TYPE) >> 7);

	return bnxt_ptype_table[index];
}

static uint32_t
bnxt_set_ol_flags(uint32x4_t mm_rxcmp, uint32x4_t mm_rxcmp1)
{
	uint16_t flags_type, errors, flags;
	uint32_t ol_flags;

	/* Extract rxcmp1->flags2. */
	flags = vgetq_lane_u32(mm_rxcmp1, 0) & 0x1F;
	/* Extract rxcmp->flags_type. */
	flags_type = vgetq_lane_u32(mm_rxcmp, 0);
	/* Extract rxcmp1->errors_v2. */
	errors = (vgetq_lane_u32(mm_rxcmp1, 2) >> 4) & flags & 0xF;

	ol_flags = bnxt_ol_flags_table[flags & ~errors];

	if (flags_type & RX_PKT_CMPL_FLAGS_RSS_VALID)
		ol_flags |= PKT_RX_RSS_HASH;

	if (errors)
		ol_flags |= bnxt_ol_flags_err_table[errors];

	return ol_flags;
}

uint16_t
bnxt_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		   uint16_t nb_pkts)
{
	struct bnxt_rx_queue *rxq = rx_queue;
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint32_t raw_cons = cpr->cp_raw_cons;
	uint32_t cons;
	int nb_rx_pkts = 0;
	struct rx_pkt_cmpl *rxcmp;
	const uint64x2_t mbuf_init = {rxq->mbuf_initializer, 0};
	const uint8x16_t shuf_msk = {
		0xFF, 0xFF, 0xFF, 0xFF,    /* pkt_type (zeroes) */
		2, 3, 0xFF, 0xFF,          /* pkt_len */
		2, 3,                      /* data_len */
		0xFF, 0xFF,                /* vlan_tci (zeroes) */
		12, 13, 14, 15             /* rss hash */
	};
	int i;

	/* If Rx Q was stopped return */
	if (unlikely(!rxq->rx_started))
		return 0;

	if (rxq->rxrearm_nb >= rxq->rx_free_thresh)
		bnxt_rxq_rearm(rxq, rxr);

	/* Return no more than RTE_BNXT_MAX_RX_BURST per call. */
	nb_pkts = RTE_MIN(nb_pkts, RTE_BNXT_MAX_RX_BURST);

	/* Make nb_pkts an integer multiple of RTE_BNXT_DESCS_PER_LOOP. */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, RTE_BNXT_DESCS_PER_LOOP);
	if (!nb_pkts)
		return 0;

	/* Handle RX burst request */
	for (i = 0; i < nb_pkts; i++) {
		uint32x4_t mm_rxcmp, mm_rxcmp1;
		struct rx_pkt_cmpl_hi *rxcmp1;
		uint32x4_t pkt_mb, rearm;
		uint32_t ptype, ol_flags;
		struct rte_mbuf *mbuf;
		uint16_t vlan_tci;
		uint16x8_t tmp16;
		uint8x16_t tmp;

		cons = RING_CMP(cpr->cp_ring_struct, raw_cons);

		rxcmp = (struct rx_pkt_cmpl *)&cpr->cp_desc_ring[cons];
		rxcmp1 = (struct rx_pkt_cmpl_hi *)&cpr->cp_desc_ring[cons + 1];

		if (!CMP_VALID(rxcmp1, raw_cons + 1, cpr->cp_ring_struct))
			break;

		mm_rxcmp = vld1q_u32((uint32_t *)rxcmp);
		mm_rxcmp1 = vld1q_u32((uint32_t *)rxcmp);
		raw_cons += 2;
		cons = rxcmp->opaque;

		mbuf = rxr->rx_buf_ring[cons];
		rte_prefetch0(mbuf);
		rxr->rx_buf_ring[cons] = NULL;

		/* Set fields from mbuf initializer and ol_flags. */
		ol_flags = bnxt_set_ol_flags(mm_rxcmp, mm_rxcmp1);
		rearm = vsetq_lane_u32(ol_flags,
				       vreinterpretq_u32_u64(mbuf_init), 2);
		vst1q_u32((uint32_t *)&mbuf->rearm_data, rearm);

		/* Set mbuf pkt_len, data_len, and rss_hash fields. */
		tmp = vqtbl1q_u8(vreinterpretq_u8_u32(mm_rxcmp), shuf_msk);
		pkt_mb = vreinterpretq_u32_u8(tmp);

		/* Set packet type. */
		ptype = bnxt_parse_pkt_type(mm_rxcmp, mm_rxcmp1);
		pkt_mb = vsetq_lane_u32(ptype, pkt_mb, 0);

		/* Set vlan_tci. */
		vlan_tci = vgetq_lane_u32(mm_rxcmp1, 1);
		tmp16 = vsetq_lane_u16(vlan_tci,
				       vreinterpretq_u16_u32(pkt_mb),
				       5);
		pkt_mb = vreinterpretq_u32_u16(tmp16);

		/* Store descriptor fields. */
		vst1q_u32((uint32_t *)&mbuf->rx_descriptor_fields1, pkt_mb);

		rx_pkts[nb_rx_pkts++] = mbuf;
	}

	if (nb_rx_pkts) {
		rxr->rx_prod =
			RING_ADV(rxr->rx_ring_struct, rxr->rx_prod, nb_rx_pkts);

		rxq->rxrearm_nb += nb_rx_pkts;
		cpr->cp_raw_cons = raw_cons;
		cpr->valid =
			!!(cpr->cp_raw_cons & cpr->cp_ring_struct->ring_size);
		bnxt_db_cq(cpr);
	}

	return nb_rx_pkts;
}

static void
bnxt_tx_cmp_vec(struct bnxt_tx_queue *txq, int nr_pkts)
{
	struct bnxt_tx_ring_info *txr = txq->tx_ring;
	struct rte_mbuf **free = txq->free;
	uint16_t cons = txr->tx_cons;
	unsigned int blk = 0;

	while (nr_pkts--) {
		struct bnxt_sw_tx_bd *tx_buf;
		struct rte_mbuf *mbuf;

		tx_buf = &txr->tx_buf_ring[cons];
		cons = RING_NEXT(txr->tx_ring_struct, cons);
		mbuf = rte_pktmbuf_prefree_seg(tx_buf->mbuf);
		if (unlikely(mbuf == NULL))
			continue;
		tx_buf->mbuf = NULL;

		if (blk && mbuf->pool != free[0]->pool) {
			rte_mempool_put_bulk(free[0]->pool, (void **)free, blk);
			blk = 0;
		}
		free[blk++] = mbuf;
	}
	if (blk)
		rte_mempool_put_bulk(free[0]->pool, (void **)free, blk);

	txr->tx_cons = cons;
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

		if (!CMP_VALID(txcmp, raw_cons, cp_ring_struct))
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
		bnxt_tx_cmp_vec(txq, nb_tx_pkts);
		cpr->cp_raw_cons = raw_cons;
		bnxt_db_cq(cpr);
	}
}

static uint16_t
bnxt_xmit_fixed_burst_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			  uint16_t nb_pkts)
{
	struct bnxt_tx_queue *txq = tx_queue;
	struct bnxt_tx_ring_info *txr = txq->tx_ring;
	uint16_t prod = txr->tx_prod;
	struct rte_mbuf *tx_mbuf;
	struct tx_bd_long *txbd = NULL;
	struct bnxt_sw_tx_bd *tx_buf;
	uint16_t to_send;

	nb_pkts = RTE_MIN(nb_pkts, bnxt_tx_avail(txq));

	if (unlikely(nb_pkts == 0))
		return 0;

	/* Handle TX burst request */
	to_send = nb_pkts;
	while (to_send) {
		tx_mbuf = *tx_pkts++;
		rte_prefetch0(tx_mbuf);

		tx_buf = &txr->tx_buf_ring[prod];
		tx_buf->mbuf = tx_mbuf;
		tx_buf->nr_bds = 1;

		txbd = &txr->tx_desc_ring[prod];
		txbd->address = tx_mbuf->buf_iova + tx_mbuf->data_off;
		txbd->len = tx_mbuf->data_len;
		txbd->flags_type = bnxt_xmit_flags_len(tx_mbuf->data_len,
						       TX_BD_FLAGS_NOCMPL);
		prod = RING_NEXT(txr->tx_ring_struct, prod);
		to_send--;
	}

	/* Request a completion for last packet in burst */
	if (txbd) {
		txbd->opaque = nb_pkts;
		txbd->flags_type &= ~TX_BD_LONG_FLAGS_NO_CMPL;
	}

	rte_compiler_barrier();
	bnxt_db_write(&txr->tx_db, prod);

	txr->tx_prod = prod;

	return nb_pkts;
}

uint16_t
bnxt_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
		   uint16_t nb_pkts)
{
	int nb_sent = 0;
	struct bnxt_tx_queue *txq = tx_queue;

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

		num = RTE_MIN(nb_pkts, RTE_BNXT_MAX_TX_BURST);
		ret = bnxt_xmit_fixed_burst_vec(tx_queue,
						&tx_pkts[nb_sent],
						num);
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
