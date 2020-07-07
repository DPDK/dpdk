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
#include "bnxt_rxr.h"
#include "bnxt_rxq.h"
#include "hsi_struct_def_dpdk.h"
#include "bnxt_rxtx_vec_common.h"

#include "bnxt_txq.h"
#include "bnxt_txr.h"

/*
 * RX Ring handling
 */

static inline void
bnxt_rxq_rearm(struct bnxt_rx_queue *rxq, struct bnxt_rx_ring_info *rxr)
{
	struct rx_prod_pkt_bd *rxbds = &rxr->rx_desc_ring[rxq->rxrearm_start];
	struct bnxt_sw_rx_bd *rx_bufs = &rxr->rx_buf_ring[rxq->rxrearm_start];
	struct rte_mbuf *mb0, *mb1;
	int i;

	const uint64x2_t hdr_room = {0, RTE_PKTMBUF_HEADROOM};
	const uint64x2_t addrmask = {0, UINT64_MAX};

	/* Pull RTE_BNXT_RXQ_REARM_THRESH more mbufs into the software ring */
	if (rte_mempool_get_bulk(rxq->mb_pool,
				 (void *)rx_bufs,
				 RTE_BNXT_RXQ_REARM_THRESH) < 0) {
		rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
			RTE_BNXT_RXQ_REARM_THRESH;

		return;
	}

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < RTE_BNXT_RXQ_REARM_THRESH; i += 2, rx_bufs += 2) {
		uint64x2_t buf_addr0, buf_addr1;
		uint64x2_t rxbd0, rxbd1;

		mb0 = rx_bufs[0].mbuf;
		mb1 = rx_bufs[1].mbuf;

		/* Load address fields from both mbufs */
		buf_addr0 = vld1q_u64((uint64_t *)&mb0->buf_addr);
		buf_addr1 = vld1q_u64((uint64_t *)&mb1->buf_addr);

		/* Load both rx descriptors (preserving some existing fields) */
		rxbd0 = vld1q_u64((uint64_t *)(rxbds + 0));
		rxbd1 = vld1q_u64((uint64_t *)(rxbds + 1));

		/* Add default offset to buffer address. */
		buf_addr0 = vaddq_u64(buf_addr0, hdr_room);
		buf_addr1 = vaddq_u64(buf_addr1, hdr_room);

		/* Clear all fields except address. */
		buf_addr0 =  vandq_u64(buf_addr0, addrmask);
		buf_addr1 =  vandq_u64(buf_addr1, addrmask);

		/* Clear address field in descriptor. */
		rxbd0 = vbicq_u64(rxbd0, addrmask);
		rxbd1 = vbicq_u64(rxbd1, addrmask);

		/* Set address field in descriptor. */
		rxbd0 = vaddq_u64(rxbd0, buf_addr0);
		rxbd1 = vaddq_u64(rxbd1, buf_addr1);

		/* Store descriptors to memory. */
		vst1q_u64((uint64_t *)(rxbds++), rxbd0);
		vst1q_u64((uint64_t *)(rxbds++), rxbd1);
	}

	rxq->rxrearm_start += RTE_BNXT_RXQ_REARM_THRESH;
	bnxt_db_write(&rxr->rx_db, rxq->rxrearm_start - 1);
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= RTE_BNXT_RXQ_REARM_THRESH;
}

static uint32_t
bnxt_parse_pkt_type(struct rx_pkt_cmpl *rxcmp, struct rx_pkt_cmpl_hi *rxcmp1)
{
	uint32_t l3, pkt_type = 0;
	uint32_t t_ipcs = 0, ip6 = 0, vlan = 0;
	uint32_t flags_type;

	vlan = !!(rxcmp1->flags2 &
		rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN));
	pkt_type |= vlan ? RTE_PTYPE_L2_ETHER_VLAN : RTE_PTYPE_L2_ETHER;

	t_ipcs = !!(rxcmp1->flags2 &
		rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC));
	ip6 = !!(rxcmp1->flags2 &
		 rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS2_IP_TYPE));

	flags_type = rxcmp->flags_type &
		rte_cpu_to_le_32(RX_PKT_CMPL_FLAGS_ITYPE_MASK);

	if (!t_ipcs && !ip6)
		l3 = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
	else if (!t_ipcs && ip6)
		l3 = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
	else if (t_ipcs && !ip6)
		l3 = RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN;
	else
		l3 = RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN;

	switch (flags_type) {
	case RTE_LE32(RX_PKT_CMPL_FLAGS_ITYPE_ICMP):
		if (!t_ipcs)
			pkt_type |= l3 | RTE_PTYPE_L4_ICMP;
		else
			pkt_type |= l3 | RTE_PTYPE_INNER_L4_ICMP;
		break;

	case RTE_LE32(RX_PKT_CMPL_FLAGS_ITYPE_TCP):
		if (!t_ipcs)
			pkt_type |= l3 | RTE_PTYPE_L4_TCP;
		else
			pkt_type |= l3 | RTE_PTYPE_INNER_L4_TCP;
		break;

	case RTE_LE32(RX_PKT_CMPL_FLAGS_ITYPE_UDP):
		if (!t_ipcs)
			pkt_type |= l3 | RTE_PTYPE_L4_UDP;
		else
			pkt_type |= l3 | RTE_PTYPE_INNER_L4_UDP;
		break;

	case RTE_LE32(RX_PKT_CMPL_FLAGS_ITYPE_IP):
		pkt_type |= l3;
		break;
	}

	return pkt_type;
}

static void
bnxt_parse_csum(struct rte_mbuf *mbuf, struct rx_pkt_cmpl_hi *rxcmp1)
{
	uint32_t flags;

	flags = flags2_0xf(rxcmp1);
	/* IP Checksum */
	if (likely(IS_IP_NONTUNNEL_PKT(flags))) {
		if (unlikely(RX_CMP_IP_CS_ERROR(rxcmp1)))
			mbuf->ol_flags |= PKT_RX_IP_CKSUM_BAD;
		else
			mbuf->ol_flags |= PKT_RX_IP_CKSUM_GOOD;
	} else if (IS_IP_TUNNEL_PKT(flags)) {
		if (unlikely(RX_CMP_IP_OUTER_CS_ERROR(rxcmp1) ||
			     RX_CMP_IP_CS_ERROR(rxcmp1)))
			mbuf->ol_flags |= PKT_RX_IP_CKSUM_BAD;
		else
			mbuf->ol_flags |= PKT_RX_IP_CKSUM_GOOD;
	} else if (unlikely(RX_CMP_IP_CS_UNKNOWN(rxcmp1))) {
		mbuf->ol_flags |= PKT_RX_IP_CKSUM_UNKNOWN;
	}

	/* L4 Checksum */
	if (likely(IS_L4_NONTUNNEL_PKT(flags))) {
		if (unlikely(RX_CMP_L4_INNER_CS_ERR2(rxcmp1)))
			mbuf->ol_flags |= PKT_RX_L4_CKSUM_BAD;
		else
			mbuf->ol_flags |= PKT_RX_L4_CKSUM_GOOD;
	} else if (IS_L4_TUNNEL_PKT(flags)) {
		if (unlikely(RX_CMP_L4_INNER_CS_ERR2(rxcmp1)))
			mbuf->ol_flags |= PKT_RX_L4_CKSUM_BAD;
		else
			mbuf->ol_flags |= PKT_RX_L4_CKSUM_GOOD;
		if (unlikely(RX_CMP_L4_OUTER_CS_ERR2(rxcmp1))) {
			mbuf->ol_flags |= PKT_RX_OUTER_L4_CKSUM_BAD;
		} else if (unlikely(IS_L4_TUNNEL_PKT_ONLY_INNER_L4_CS
				    (flags))) {
			mbuf->ol_flags |= PKT_RX_OUTER_L4_CKSUM_UNKNOWN;
		} else {
			mbuf->ol_flags |= PKT_RX_OUTER_L4_CKSUM_GOOD;
		}
	} else if (unlikely(RX_CMP_L4_CS_UNKNOWN(rxcmp1))) {
		mbuf->ol_flags |= PKT_RX_L4_CKSUM_UNKNOWN;
	}
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
	bool evt = false;
	const uint64x2_t mbuf_init = {rxq->mbuf_initializer, 0};
	const uint8x16_t shuf_msk = {
		0xFF, 0xFF, 0xFF, 0xFF,    /* pkt_type (zeroes) */
		2, 3, 0xFF, 0xFF,          /* pkt_len */
		2, 3,                      /* data_len */
		0xFF, 0xFF,                /* vlan_tci (zeroes) */
		12, 13, 14, 15             /* rss hash */
	};

	/* If Rx Q was stopped return */
	if (unlikely(!rxq->rx_started))
		return 0;

	if (rxq->rxrearm_nb >= RTE_BNXT_RXQ_REARM_THRESH)
		bnxt_rxq_rearm(rxq, rxr);

	/* Return no more than RTE_BNXT_MAX_RX_BURST per call. */
	nb_pkts = RTE_MIN(nb_pkts, RTE_BNXT_MAX_RX_BURST);

	/* Make nb_pkts an integer multiple of RTE_BNXT_DESCS_PER_LOOP */
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, RTE_BNXT_DESCS_PER_LOOP);
	if (!nb_pkts)
		return 0;

	/* Handle RX burst request */
	while (1) {
		cons = RING_CMP(cpr->cp_ring_struct, raw_cons);

		rxcmp = (struct rx_pkt_cmpl *)&cpr->cp_desc_ring[cons];

		if (!CMP_VALID(rxcmp, raw_cons, cpr->cp_ring_struct))
			break;

		if (likely(CMP_TYPE(rxcmp) == RX_PKT_CMPL_TYPE_RX_L2)) {
			struct rx_pkt_cmpl_hi *rxcmp1;
			uint32_t tmp_raw_cons;
			uint16_t cp_cons;
			struct rte_mbuf *mbuf;
			uint64x2_t mm_rxcmp;
			uint8x16_t pkt_mb;

			tmp_raw_cons = NEXT_RAW_CMP(raw_cons);
			cp_cons = RING_CMP(cpr->cp_ring_struct, tmp_raw_cons);
			rxcmp1 = (struct rx_pkt_cmpl_hi *)
						&cpr->cp_desc_ring[cp_cons];

			if (!CMP_VALID(rxcmp1, tmp_raw_cons,
				       cpr->cp_ring_struct))
				break;

			raw_cons = tmp_raw_cons;
			cons = rxcmp->opaque;

			mbuf = rxr->rx_buf_ring[cons].mbuf;
			rte_prefetch0(mbuf);
			rxr->rx_buf_ring[cons].mbuf = NULL;

			/* Set constant fields from mbuf initializer. */
			vst1q_u64((uint64_t *)&mbuf->rearm_data, mbuf_init);

			/* Set mbuf pkt_len, data_len, and rss_hash fields. */
			mm_rxcmp = vld1q_u64((uint64_t *)rxcmp);
			pkt_mb = vqtbl1q_u8(vreinterpretq_u8_u64(mm_rxcmp),
					    shuf_msk);
			vst1q_u64((uint64_t *)&mbuf->rx_descriptor_fields1,
				  vreinterpretq_u64_u8(pkt_mb));

			rte_compiler_barrier();

			if (rxcmp->flags_type & RX_PKT_CMPL_FLAGS_RSS_VALID)
				mbuf->ol_flags |= PKT_RX_RSS_HASH;

			if (rxcmp1->flags2 &
			    RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN) {
				mbuf->vlan_tci = rxcmp1->metadata &
					(RX_PKT_CMPL_METADATA_VID_MASK |
					RX_PKT_CMPL_METADATA_DE |
					RX_PKT_CMPL_METADATA_PRI_MASK);
				mbuf->ol_flags |=
					PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
			}

			bnxt_parse_csum(mbuf, rxcmp1);
			mbuf->packet_type = bnxt_parse_pkt_type(rxcmp, rxcmp1);

			rx_pkts[nb_rx_pkts++] = mbuf;
		} else if (!BNXT_NUM_ASYNC_CPR(rxq->bp)) {
			evt =
			bnxt_event_hwrm_resp_handler(rxq->bp,
						     (struct cmpl_base *)rxcmp);
		}

		raw_cons = NEXT_RAW_CMP(raw_cons);
		if (nb_rx_pkts == nb_pkts || evt)
			break;
	}
	rxr->rx_prod = RING_ADV(rxr->rx_ring_struct, rxr->rx_prod, nb_rx_pkts);

	rxq->rxrearm_nb += nb_rx_pkts;
	cpr->cp_raw_cons = raw_cons;
	cpr->valid = !!(cpr->cp_raw_cons & cpr->cp_ring_struct->ring_size);
	if (nb_rx_pkts || evt)
		bnxt_db_cq(cpr);

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
