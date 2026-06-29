/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2026 NXP
 */

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "rte_ethdev.h"
#include "rte_malloc.h"
#include "rte_memzone.h"

#include "base/enetc_hw.h"
#include "base/enetc4_hw.h"
#include "enetc.h"
#include "enetc_logs.h"

#define ENETC_CACHE_LINE_RXBDS	(RTE_CACHE_LINE_SIZE / \
				 sizeof(union enetc_rx_bd))
#define ENETC_RXBD_BUNDLE 16 /* Number of buffers to allocate at once */

static int
enetc_clean_tx_ring(struct enetc_bdr *tx_ring)
{
	int tx_frm_cnt = 0;
	struct enetc_swbd *tx_swbd, *tx_swbd_base;
	int i, hwci, bd_count;
	struct rte_mbuf *m[ENETC_RXBD_BUNDLE];
	struct enetc_tx_bd *txbd;

	/* we don't need barriers here, we just want a relatively current value
	 * from HW.
	 */
	hwci = (int)(rte_read32_relaxed(tx_ring->tcisr) &
		     ENETC_TBCISR_IDX_MASK);

	tx_swbd_base = tx_ring->q_swbd;
	bd_count = tx_ring->bd_count;
	i = tx_ring->next_to_clean;
	tx_swbd = &tx_swbd_base[i];

	/* we're only reading the CI index once here, which means HW may update
	 * it while we're doing clean-up.  We could read the register in a loop
	 * but for now I assume it's OK to leave a few Tx frames for next call.
	 * The issue with reading the register in a loop is that we're stalling
	 * here trying to catch up with HW which keeps sending traffic as long
	 * as it has traffic to send, so in effect we could be waiting here for
	 * the Tx ring to be drained by HW, instead of us doing Rx in that
	 * meantime.
	 */
	while (i != hwci) {
		/* It seems calling rte_pktmbuf_free is wasting a lot of cycles,
		 * make a list and call _free when it's done.
		 */
		/* Clear flags on the reclaimed BD so that dcbf in the
		 * cacheable TX path never flushes a stale flags_F to memory
		 * before the new BD fields are fully written.
		 */
		txbd = ENETC_TXBD(*tx_ring, i);
		txbd->flags = 0;

		if (tx_frm_cnt == ENETC_RXBD_BUNDLE) {
			rte_pktmbuf_free_bulk(m, tx_frm_cnt);
			tx_frm_cnt = 0;
		}

		m[tx_frm_cnt] = tx_swbd->buffer_addr;
		tx_swbd->buffer_addr = NULL;

		i++;
		tx_swbd++;
		if (unlikely(i == bd_count)) {
			i = 0;
			tx_swbd = tx_swbd_base;
		}

		tx_frm_cnt++;
	}

	if (tx_frm_cnt)
		rte_pktmbuf_free_bulk(m, tx_frm_cnt);

	tx_ring->next_to_clean = i;

	return 0;
}

uint16_t
enetc_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	struct enetc_swbd *tx_swbd;
	int i, start, bds_to_use;
	struct enetc_tx_bd *txbd;
	struct enetc_bdr *tx_ring = (struct enetc_bdr *)tx_queue;

	i = tx_ring->next_to_use;

	bds_to_use = enetc_bd_unused(tx_ring);
	if (bds_to_use < nb_pkts)
		nb_pkts = bds_to_use;

	start = 0;
	while (nb_pkts--) {
		tx_ring->q_swbd[i].buffer_addr = tx_pkts[start];

		txbd = ENETC_TXBD(*tx_ring, i);
		tx_swbd = &tx_ring->q_swbd[i];
		txbd->frm_len = tx_pkts[start]->pkt_len;
		txbd->buf_len = txbd->frm_len;
		txbd->flags = ENETC_TXBD_FLAGS_F;
		txbd->addr = (uint64_t)(uintptr_t)
		rte_cpu_to_le_64((size_t)tx_swbd->buffer_addr->buf_iova +
				 tx_swbd->buffer_addr->data_off);
		i++;
		start++;
		if (unlikely(i == tx_ring->bd_count))
			i = 0;
	}

	/* we're only cleaning up the Tx ring here, on the assumption that
	 * software is slower than hardware and hardware completed sending
	 * older frames out by now.
	 * We're also cleaning up the ring before kicking off Tx for the new
	 * batch to minimize chances of contention on the Tx ring
	 */
	enetc_clean_tx_ring(tx_ring);

	tx_ring->next_to_use = i;
	enetc_wr_reg(tx_ring->tcir, i);
	return start;
}

static void
enetc4_tx_offload_checksum(struct rte_mbuf *mbuf, struct enetc_tx_bd *txbd)
{
	if ((mbuf->ol_flags & (RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_IPV4))
						== ENETC4_MBUF_F_TX_IP_IPV4) {
		txbd->l3t = ENETC4_TXBD_L3T;
		txbd->ipcs = ENETC4_TXBD_IPCS;
		txbd->l3_start = mbuf->l2_len;
		txbd->l3_hdr_size = mbuf->l3_len / 4;
		txbd->flags |= ENETC4_TXBD_FLAGS_L_TX_CKSUM;
		if ((mbuf->ol_flags & RTE_MBUF_F_TX_UDP_CKSUM) == RTE_MBUF_F_TX_UDP_CKSUM) {
			txbd->l4t = ENETC4_TXBD_L4T_UDP;
			txbd->flags |= ENETC4_TXBD_FLAGS_L4CS;
		} else if ((mbuf->ol_flags & RTE_MBUF_F_TX_TCP_CKSUM) == RTE_MBUF_F_TX_TCP_CKSUM) {
			txbd->l4t = ENETC4_TXBD_L4T_TCP;
			txbd->flags |= ENETC4_TXBD_FLAGS_L4CS;
		}
	}
}

uint16_t
enetc_xmit_pkts_nc(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	struct enetc_bdr *tx_ring = (struct enetc_bdr *)tx_queue;
	int i, start, bds_to_use, bd_count;
	struct enetc_tx_bd *txbd = NULL;
	struct rte_mbuf *seg;
	uint16_t seg_len, segs_per_pkt;
	bool is_first_seg;
	unsigned int j;
	uint8_t *data;

	i = tx_ring->next_to_use;
	bds_to_use = enetc_bd_unused(tx_ring);
	bd_count = tx_ring->bd_count;

	start = 0;
	while (start < nb_pkts) {
		seg = tx_pkts[start];
		segs_per_pkt = seg->nb_segs;

		if (bds_to_use < segs_per_pkt)
			break;

		is_first_seg = true;
		while (seg) {
			tx_ring->q_swbd[i].buffer_addr = NULL;
			seg_len = rte_pktmbuf_data_len(seg);
			data = rte_pktmbuf_mtod(seg, void *);

			/* Flush payload to PoC so HW DMA reads the correct data. */
			for (j = 0; j < seg_len; j += RTE_CACHE_LINE_SIZE)
				dcbf(data + j);
			/* Cover the last byte of an unaligned buffer. */
			dcbf(data + (seg_len - 1));

			txbd = ENETC_TXBD(*tx_ring, i);
			txbd->flags = 0;
			if (is_first_seg) {
				tx_ring->q_swbd[i].buffer_addr = tx_pkts[start];
				txbd->frm_len = rte_pktmbuf_pkt_len(seg);
				if (seg->ol_flags & ENETC4_TX_CKSUM_OFFLOAD_MASK)
					enetc4_tx_offload_checksum(seg, txbd);
				is_first_seg = false;
			}

			txbd->buf_len = rte_cpu_to_le_16(seg_len);
			txbd->addr = rte_cpu_to_le_64(rte_mbuf_data_iova(seg));
			seg = seg->next;
			i++;
			bds_to_use--;
			if (unlikely(i == bd_count))
				i = 0;
		}

		/* Set the frame-last flag on the final BD of this packet. */
		if (likely(txbd))
			txbd->flags |= ENETC4_TXBD_FLAGS_F;
		start++;
	}

	enetc_clean_tx_ring(tx_ring);
	tx_ring->next_to_use = i;
	enetc_wr_reg(tx_ring->tcir, i);
	return start;
}

int
enetc_refill_rx_ring(struct enetc_bdr *rx_ring, const int buff_cnt)
{
	struct enetc_swbd *rx_swbd;
	union enetc_rx_bd *rxbd;
	union enetc_rx_bd *grp_start_rxbd;
	int i, j, k = ENETC_RXBD_BUNDLE;
	struct rte_mbuf *m[ENETC_RXBD_BUNDLE];
	struct rte_mempool *mb_pool;

	i = rx_ring->next_to_use;
	mb_pool = rx_ring->mb_pool;
	rx_swbd = &rx_ring->q_swbd[i];
	rxbd = ENETC_RXBD(*rx_ring, i);
	grp_start_rxbd = rxbd;
	for (j = 0; j < buff_cnt; j++) {
		/* bulk alloc for the next up to 8 BDs */
		if (k == ENETC_RXBD_BUNDLE) {
			k = 0;
			int m_cnt = RTE_MIN(buff_cnt - j, ENETC_RXBD_BUNDLE);

			if (rte_pktmbuf_alloc_bulk(mb_pool, m, m_cnt))
				return -1;
		}

		rx_swbd->buffer_addr = m[k];
		rxbd->w.addr = (uint64_t)(uintptr_t)
			       rx_swbd->buffer_addr->buf_iova +
			       rx_swbd->buffer_addr->data_off;
		/* clear 'R" as well */
		rxbd->r.lstatus = 0;
		rx_swbd++;
		rxbd++;
		i++;
		k++;
		if (unlikely(i == rx_ring->bd_count)) {
			/*
			 * Ring wrap: flush the current partial or full group
			 * before resetting the pointer to index 0.
			 */
			dcbf((void *)grp_start_rxbd);
			i = 0;
			rxbd = ENETC_RXBD(*rx_ring, i);
			rx_swbd = &rx_ring->q_swbd[i];
			grp_start_rxbd = rxbd;
		} else if ((i & ENETC_BD_PER_CL_MASK) == 0) {
			/*
			 * Completed a full 4-BD group (one cache line).
			 * Flush it to PoC so HW sees the updated descriptors.
			 */
			dcbf((void *)grp_start_rxbd);
			grp_start_rxbd = rxbd;
		}
	}

	/* Flush any remaining partial group at the end of the fill. */
	if (j && (i & ENETC_BD_PER_CL_MASK) != 0)
		dcbf((void *)grp_start_rxbd);

	if (likely(j)) {
		rx_ring->next_to_alloc = i;
		rx_ring->next_to_use = i;
		enetc_wr_reg(rx_ring->rcir, i);
	}

	return j;
}

static inline void enetc_slow_parsing(struct rte_mbuf *m,
				     uint64_t parse_results)
{
	m->ol_flags &= ~(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD);

	switch (parse_results) {
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV4:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4;
		m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV6:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6;
		m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV4_TCP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_TCP;
		m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD |
			       RTE_MBUF_F_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV6_TCP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_TCP;
		m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD |
			       RTE_MBUF_F_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV4_UDP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_UDP;
		m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD |
			       RTE_MBUF_F_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV6_UDP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_UDP;
		m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD |
			       RTE_MBUF_F_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV4_SCTP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_SCTP;
		m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD |
			       RTE_MBUF_F_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV6_SCTP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_SCTP;
		m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD |
			       RTE_MBUF_F_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV4_ICMP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_ICMP;
		m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD |
			       RTE_MBUF_F_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV6_ICMP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_ICMP;
		m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD |
			       RTE_MBUF_F_RX_L4_CKSUM_BAD;
		return;
	/* More switch cases can be added */
	default:
		m->packet_type = RTE_PTYPE_UNKNOWN;
		m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN |
			       RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN;
	}
}


static inline void __rte_hot
enetc_dev_rx_parse(struct rte_mbuf *m, uint16_t parse_results)
{
	ENETC_PMD_DP_DEBUG("parse summary = 0x%x   ", parse_results);
	m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD;

	switch (parse_results) {
	case ENETC_PKT_TYPE_ETHER:
		m->packet_type = RTE_PTYPE_L2_ETHER;
		return;
	case ENETC_PKT_TYPE_IPV4:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4;
		return;
	case ENETC_PKT_TYPE_IPV6:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6;
		return;
	case ENETC_PKT_TYPE_IPV4_TCP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_TCP;
		return;
	case ENETC_PKT_TYPE_IPV6_TCP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_TCP;
		return;
	case ENETC_PKT_TYPE_IPV4_UDP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_UDP;
		return;
	case ENETC_PKT_TYPE_IPV6_UDP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_UDP;
		return;
	case ENETC_PKT_TYPE_IPV4_ESP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_TUNNEL_ESP;
		return;
	case ENETC_PKT_TYPE_IPV6_ESP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_TUNNEL_ESP;
		return;
	case ENETC_PKT_TYPE_IPV4_SCTP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_SCTP;
		return;
	case ENETC_PKT_TYPE_IPV6_SCTP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_SCTP;
		return;
	case ENETC_PKT_TYPE_IPV4_ICMP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_ICMP;
		return;
	case ENETC_PKT_TYPE_IPV6_ICMP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_ICMP;
		return;
	case ENETC_PKT_TYPE_IPV4_FRAG:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_FRAG;
		return;
	case ENETC_PKT_TYPE_IPV6_FRAG:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_FRAG;
		return;
	/* More switch cases can be added */
	default:
		enetc_slow_parsing(m, parse_results);
	}

}

static int
enetc_clean_rx_ring(struct enetc_bdr *rx_ring,
		    struct rte_mbuf **rx_pkts,
		    int work_limit)
{
	int rx_frm_cnt = 0;
	int cleaned_cnt, i, bd_count;
	struct enetc_swbd *rx_swbd;
	union enetc_rx_bd *rxbd;
	uint32_t bd_status;

	/* next descriptor to process */
	i = rx_ring->next_to_clean;
	/* next descriptor to process */
	rxbd = ENETC_RXBD(*rx_ring, i);
	rte_prefetch0(rxbd);
	bd_count = rx_ring->bd_count;
	/* LS1028A does not have platform cache so any software access following
	 * a hardware write will go directly to DDR.  Latency of such a read is
	 * in excess of 100 core cycles, so try to prefetch more in advance to
	 * mitigate this.
	 * How much is worth prefetching really depends on traffic conditions.
	 * With congested Rx this could go up to 4 cache lines or so.  But if
	 * software keeps up with hardware and follows behind Rx PI by a cache
	 * line or less then it's harmful in terms of performance to cache more.
	 * We would only prefetch BDs that have yet to be written by ENETC,
	 * which will have to be evicted again anyway.
	 */
	rte_prefetch0(ENETC_RXBD(*rx_ring,
				 (i + ENETC_CACHE_LINE_RXBDS) % bd_count));
	rte_prefetch0(ENETC_RXBD(*rx_ring,
				 (i + ENETC_CACHE_LINE_RXBDS * 2) % bd_count));

	cleaned_cnt = enetc_bd_unused(rx_ring);
	rx_swbd = &rx_ring->q_swbd[i];

	while (likely(rx_frm_cnt < work_limit)) {
		bd_status = rte_le_to_cpu_32(rxbd->r.lstatus);
		if (!bd_status)
			break;

		rx_swbd->buffer_addr->pkt_len = rxbd->r.buf_len -
						rx_ring->crc_len;
		rx_swbd->buffer_addr->data_len = rxbd->r.buf_len -
						 rx_ring->crc_len;
		rx_swbd->buffer_addr->hash.rss = rxbd->r.rss_hash;
		rx_swbd->buffer_addr->ol_flags = 0;
		enetc_dev_rx_parse(rx_swbd->buffer_addr,
				   rxbd->r.parse_summary);

		rx_pkts[rx_frm_cnt] = rx_swbd->buffer_addr;
		cleaned_cnt++;
		rx_swbd++;
		i++;
		if (unlikely(i == rx_ring->bd_count)) {
			i = 0;
			rx_swbd = &rx_ring->q_swbd[i];
		}
		rxbd = ENETC_RXBD(*rx_ring, i);
		rte_prefetch0(ENETC_RXBD(*rx_ring,
					 (i + ENETC_CACHE_LINE_RXBDS) %
					  bd_count));
		rte_prefetch0(ENETC_RXBD(*rx_ring,
					 (i + ENETC_CACHE_LINE_RXBDS * 2) %
					 bd_count));

		rx_frm_cnt++;
	}

	rx_ring->next_to_clean = i;
	enetc_refill_rx_ring(rx_ring, cleaned_cnt);

	return rx_frm_cnt;
}

static int
enetc_clean_rx_ring_nc(struct enetc_bdr *rx_ring,
		    struct rte_mbuf **rx_pkts,
		    int work_limit)
{
	int rx_frm_cnt = 0;
	int cleaned_cnt, i;
	struct enetc_swbd *rx_swbd;
	union enetc_rx_bd *rxbd, rxbd_temp;
	struct rte_mbuf *first_seg, *cur_seg;
	uint32_t bd_status;
	uint8_t *data;
	uint32_t j;
	struct rte_mbuf *seg;
	uint16_t data_len;

	/* next descriptor to process */
	i = rx_ring->next_to_clean;
	rxbd = ENETC_RXBD(*rx_ring, i);
	cleaned_cnt = enetc_bd_unused(rx_ring);
	rx_swbd = &rx_ring->q_swbd[i];

	/* Restore partial multi-segment chain from a previous burst. */
	first_seg = rx_ring->pkt_first_seg;
	cur_seg = rx_ring->pkt_last_seg;

	while (likely(rx_frm_cnt < work_limit)) {
		rxbd_temp = *rxbd;
		bd_status = rte_le_to_cpu_32(rxbd_temp.r.lstatus);
		/* LSTATUS_R indicates this BD has been written by HW */
		if (!(bd_status & ENETC_RXBD_LSTATUS_R))
			break;
		if (rxbd_temp.r.error)
			rx_ring->ierrors++;

		seg = rx_swbd->buffer_addr;
		data_len = rte_le_to_cpu_16(rxbd_temp.r.buf_len);
		seg->data_len = data_len;

		if (!first_seg) {
			first_seg = seg;
			cur_seg = seg;
			first_seg->pkt_len = data_len;
			enetc_dev_rx_parse(first_seg, rxbd_temp.r.parse_summary);
			first_seg->hash.rss = rxbd_temp.r.rss_hash;
		} else {
			first_seg->pkt_len += data_len;
			first_seg->nb_segs++;
			cur_seg->next = seg;
			cur_seg = seg;
		}

		/* Invalidate packet data cache lines so CPU reads HW-written data. */
		data = rte_pktmbuf_mtod(seg, void *);
		for (j = 0; j < data_len; j += RTE_CACHE_LINE_SIZE)
			dccivac(data + j);
		dccivac(data + (data_len - 1));

		if (bd_status & ENETC_RXBD_LSTATUS_F) {
			seg->next = NULL;
			first_seg->pkt_len -= rx_ring->crc_len;
			rx_pkts[rx_frm_cnt] = first_seg;
			rx_frm_cnt++;
			first_seg = NULL;
		}

		cleaned_cnt++;
		rx_swbd++;
		i++;
		if (unlikely(i == rx_ring->bd_count)) {
			i = 0;
			rx_swbd = &rx_ring->q_swbd[i];
		}
		rxbd = ENETC_RXBD(*rx_ring, i);
	}

	/* Save partial chain for the next burst if frame is incomplete. */
	rx_ring->pkt_first_seg = first_seg;
	rx_ring->pkt_last_seg = cur_seg;
	rx_ring->next_to_clean = i;
	enetc_refill_rx_ring(rx_ring, cleaned_cnt);

	return rx_frm_cnt;
}

uint16_t
enetc_recv_pkts_nc(void *rxq, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	struct enetc_bdr *rx_ring = (struct enetc_bdr *)rxq;

	return enetc_clean_rx_ring_nc(rx_ring, rx_pkts, nb_pkts);
}

uint16_t
enetc_recv_pkts(void *rxq, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	struct enetc_bdr *rx_ring = (struct enetc_bdr *)rxq;

	return enetc_clean_rx_ring(rx_ring, rx_pkts, nb_pkts);
}

/* --- Cacheable BD ring TX path with SW cache maintenance (dcbf) --- */

uint16_t
enetc_xmit_pkts_cacheable(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	int i, start, bds_to_use;
	struct enetc_tx_bd *txbd = NULL;
	struct enetc_bdr *tx_ring = (struct enetc_bdr *)tx_queue;
	unsigned int j;
	uint8_t *data;
	struct rte_mbuf *seg;
	uint16_t seg_len, segs_per_pkt;
	bool is_first_seg;
	int first_bd_idx, bd_count;

	i = tx_ring->next_to_use;
	bds_to_use = enetc_bd_unused(tx_ring);
	bd_count = tx_ring->bd_count;
	start = 0;

	/*
	 * Remember the first BD index of this batch so we can flush the
	 * BD cache lines to PoC after all descriptors are written.
	 */
	first_bd_idx = i;

	while (start < nb_pkts) {
		seg = tx_pkts[start];
		segs_per_pkt = seg->nb_segs;

		if (bds_to_use < segs_per_pkt)
			break;

		is_first_seg = true;
		while (seg) {
			tx_ring->q_swbd[i].buffer_addr = NULL;
			seg_len = rte_pktmbuf_data_len(seg);
			data = rte_pktmbuf_mtod(seg, void *);

			/*
			 * Flush packet data cache lines to PoC so HW DMA
			 * reads the correct payload from memory.
			 */
			for (j = 0; j < seg_len; j += RTE_CACHE_LINE_SIZE)
				dcbf(data + j);

			/*
			 * Cover the last byte of an unaligned buffer to
			 * ensure the full payload is clean to the Point of
			 * Coherency.
			 */
			dcbf(data + (seg_len - 1));
			txbd = ENETC_TXBD(*tx_ring, i);
			txbd->flags = 0;
			if (is_first_seg) {
				tx_ring->q_swbd[i].buffer_addr = seg;
				txbd->frm_len = rte_pktmbuf_pkt_len(seg);
				if (seg->ol_flags & ENETC4_TX_CKSUM_OFFLOAD_MASK)
					enetc4_tx_offload_checksum(seg, txbd);
				is_first_seg = false;
			}

			txbd->buf_len = rte_cpu_to_le_16(seg_len);
			txbd->addr = rte_cpu_to_le_64(rte_mbuf_data_iova(seg));
			seg = seg->next;
			i++;
			bds_to_use--;

			if (unlikely(i == bd_count))
				i = 0;
		}

		/*
		 * Set the frame-last flag on the final BD of this packet.
		 * This is the last write to the BD group; the cache flush
		 * below will push all BDs to memory afterwards.
		 */
		if (likely(txbd))
			txbd->flags |= ENETC4_TXBD_FLAGS_F;
		start++;
	}

	/*
	 * Flush TX BDs to PoC so HW (non-cache-coherent i.MX95) can read
	 * the descriptors from memory.  TX BDs are 16 B each; 4 BDs share
	 * one 64-byte cache line.  Walk from the cache-line-aligned start
	 * of first_bd_idx to just past the last written BD, one dcbf per
	 * cache line.
	 *
	 * The flush must happen AFTER all BD fields (including flags_F) are
	 * written, so HW never sees a partial descriptor.
	 */
	if (likely(start > 0)) {
		int n = first_bd_idx & ~ENETC_BD_PER_CL_MASK;
		int written = (i - n + bd_count) % bd_count;

		if (written == 0)
			written = bd_count;
		written = (written + ENETC_BD_PER_CL_MASK) & ~ENETC_BD_PER_CL_MASK;

		while (written > 0) {
			dcbf((void *)ENETC_TXBD(*tx_ring, n));
			n = (n + ENETC_BD_PER_CL) % bd_count;
			written -= ENETC_BD_PER_CL;
		}
	}

	enetc_clean_tx_ring(tx_ring);
	tx_ring->next_to_use = i;
	enetc_wr_reg(tx_ring->tcir, i);

	return start;
}

/* --- Cacheable BD ring RX path with SW cache maintenance (dccivac) --- */

static int
enetc_clean_rx_ring_cacheable(struct enetc_bdr *rx_ring,
		struct rte_mbuf **rx_pkts,
		int work_limit)
{
	int rx_frm_cnt = 0;
	int cleaned_cnt, i;
	struct enetc_swbd *rx_swbd;
	union enetc_rx_bd *rxbd;
	struct rte_mbuf *first_seg, *cur_seg;
	uint32_t bd_status;
	uint8_t *data;
	uint32_t j;
	struct rte_mbuf *seg;
	uint16_t data_len;

	i = rx_ring->next_to_clean;
	rxbd = ENETC_RXBD(*rx_ring, i);
	cleaned_cnt = enetc_bd_unused(rx_ring);
	rx_swbd = &rx_ring->q_swbd[i];

	/* Restore partial multi-segment chain from a previous burst. */
	first_seg = rx_ring->pkt_first_seg;
	cur_seg = rx_ring->pkt_last_seg;

	/*
	 * On i.MX95 the BD ring is in cacheable hugepage memory but the
	 * platform is non-cache-coherent.  HW writes RX BDs to DDR
	 * without snooping the CPU cache, so stale cached copies of BD
	 * status fields must be discarded before the CPU reads them.
	 *
	 * Ideal instruction: DC IVAC (invalidate only, no writeback).
	 * ARM64 constraint: DC IVAC requires EL1 privilege; executing it
	 * from EL0 (DPDK userspace) raises a fault.  The only EL0-safe
	 * cache maintenance instruction that invalidates is DC CIVAC
	 * (clean + invalidate, dccivac).
	 *
	 * Safety of using dccivac here:
	 * enetc_refill_rx_ring() issues dcbf() on every BD group before
	 * returning ownership to HW.  After dcbf the CPU cache lines are
	 * marked clean (no dirty data).  When dccivac runs, the "clean"
	 * phase finds nothing dirty to write back, so it behaves as a
	 * pure invalidate - exactly what we need.
	 *
	 * Granularity: BD = 16 B, cache line = 64 B, so one dccivac
	 * covers exactly 4 BDs.  Invalidate at each 4-BD boundary.
	 */
	dccivac((void *)ENETC_RXBD(*rx_ring,
			(i & ~(int)ENETC_BD_PER_CL_MASK)));

	while (likely(rx_frm_cnt < work_limit)) {
		bd_status = rte_le_to_cpu_32(rxbd->r.lstatus);

		if (!(bd_status & ENETC_RXBD_LSTATUS_R))
			break;
		if (rxbd->r.error)
			rx_ring->ierrors++;

		seg = rx_swbd->buffer_addr;
		data_len = rte_le_to_cpu_16(rxbd->r.buf_len);
		seg->data_len = data_len;
		if (!first_seg) {
			first_seg = seg;
			cur_seg = seg;
			first_seg->pkt_len = data_len;
			enetc_dev_rx_parse(first_seg,
					   rxbd->r.parse_summary);
			first_seg->hash.rss = rxbd->r.rss_hash;
		} else {
			first_seg->pkt_len += data_len;
			first_seg->nb_segs++;
			cur_seg->next = seg;
			cur_seg = seg;
		}

		/*
		 * Invalidate packet data cache lines so the CPU reads the
		 * payload that HW DMA'd into memory, not stale cached bytes.
		 */
		data = rte_pktmbuf_mtod(seg, void *);
		for (j = 0; j < data_len; j += RTE_CACHE_LINE_SIZE)
			dccivac(data + j);
		/* Cover the last byte of an unaligned buffer. */
		dccivac(data + (data_len - 1));

		if (bd_status & ENETC_RXBD_LSTATUS_F) {
			seg->next = NULL;
			first_seg->pkt_len -= rx_ring->crc_len;
			rx_pkts[rx_frm_cnt] = first_seg;
			rx_frm_cnt++;
			first_seg = NULL;
		}

		cleaned_cnt++;
		rx_swbd++;
		i++;
		if (unlikely(i == rx_ring->bd_count)) {
			i = 0;
			rx_swbd = &rx_ring->q_swbd[i];
		}
		rxbd = ENETC_RXBD(*rx_ring, i);

		/*
		 * Crossed a 4-BD (cache-line) boundary: invalidate the new
		 * group so the next four status reads fetch fresh DDR data
		 * written by HW.
		 */
		if ((i & ENETC_BD_PER_CL_MASK) == 0 &&
		    likely(rx_frm_cnt < work_limit))
			dccivac((void *)rxbd);
	}

	/* Save partial chain for the next burst if frame is incomplete. */
	rx_ring->pkt_first_seg = first_seg;
	rx_ring->pkt_last_seg = cur_seg;
	rx_ring->next_to_clean = i;
	enetc_refill_rx_ring(rx_ring, ENETC_BD_ALIGN_DOWN(cleaned_cnt));

	return rx_frm_cnt;
}

uint16_t
enetc_recv_pkts_cacheable(void *rxq, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	struct enetc_bdr *rx_ring = (struct enetc_bdr *)rxq;

	return enetc_clean_rx_ring_cacheable(rx_ring, rx_pkts, nb_pkts);
}
