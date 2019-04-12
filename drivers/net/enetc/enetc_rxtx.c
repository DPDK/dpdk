/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "rte_ethdev.h"
#include "rte_malloc.h"
#include "rte_memzone.h"

#include "base/enetc_hw.h"
#include "enetc.h"
#include "enetc_logs.h"

#define ENETC_RXBD_BUNDLE 8 /* Number of BDs to update at once */

static int
enetc_clean_tx_ring(struct enetc_bdr *tx_ring)
{
	int tx_frm_cnt = 0;
	struct enetc_swbd *tx_swbd;
	int i;

	i = tx_ring->next_to_clean;
	tx_swbd = &tx_ring->q_swbd[i];
	while ((int)(enetc_rd_reg(tx_ring->tcisr) &
	       ENETC_TBCISR_IDX_MASK) != i) {
		rte_pktmbuf_free(tx_swbd->buffer_addr);
		tx_swbd->buffer_addr = NULL;
		tx_swbd++;
		i++;
		if (unlikely(i == tx_ring->bd_count)) {
			i = 0;
			tx_swbd = &tx_ring->q_swbd[0];
		}

		tx_frm_cnt++;
	}

	tx_ring->next_to_clean = i;
	return tx_frm_cnt++;
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
		enetc_clean_tx_ring(tx_ring);
		tx_ring->q_swbd[i].buffer_addr = tx_pkts[start];
		txbd = ENETC_TXBD(*tx_ring, i);
		tx_swbd = &tx_ring->q_swbd[i];
		txbd->frm_len = tx_pkts[start]->pkt_len;
		txbd->buf_len = txbd->frm_len;
		txbd->flags = rte_cpu_to_le_16(ENETC_TXBD_FLAGS_F);
		txbd->addr = (uint64_t)(uintptr_t)
		rte_cpu_to_le_64((size_t)tx_swbd->buffer_addr->buf_iova +
				 tx_swbd->buffer_addr->data_off);
		i++;
		start++;
		if (unlikely(i == tx_ring->bd_count))
			i = 0;
	}

	tx_ring->next_to_use = i;
	enetc_wr_reg(tx_ring->tcir, i);
	return start;
}

int
enetc_refill_rx_ring(struct enetc_bdr *rx_ring, const int buff_cnt)
{
	struct enetc_swbd *rx_swbd;
	union enetc_rx_bd *rxbd;
	int i, j;

	i = rx_ring->next_to_use;
	rx_swbd = &rx_ring->q_swbd[i];
	rxbd = ENETC_RXBD(*rx_ring, i);
	for (j = 0; j < buff_cnt; j++) {
		rx_swbd->buffer_addr = (void *)(uintptr_t)
			rte_cpu_to_le_64((uint64_t)(uintptr_t)
					rte_pktmbuf_alloc(rx_ring->mb_pool));
		rxbd->w.addr = (uint64_t)(uintptr_t)
			       rx_swbd->buffer_addr->buf_iova +
			       rx_swbd->buffer_addr->data_off;
		/* clear 'R" as well */
		rxbd->r.lstatus = 0;
		rx_swbd++;
		rxbd++;
		i++;
		if (unlikely(i == rx_ring->bd_count)) {
			i = 0;
			rxbd = ENETC_RXBD(*rx_ring, 0);
			rx_swbd = &rx_ring->q_swbd[i];
		}
	}

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
	m->ol_flags &= ~(PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD);

	switch (parse_results) {
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV4:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4;
		m->ol_flags |= PKT_RX_IP_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV6:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6;
		m->ol_flags |= PKT_RX_IP_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV4_TCP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_TCP;
		m->ol_flags |= PKT_RX_IP_CKSUM_GOOD |
			       PKT_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV6_TCP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_TCP;
		m->ol_flags |= PKT_RX_IP_CKSUM_GOOD |
			       PKT_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV4_UDP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_UDP;
		m->ol_flags |= PKT_RX_IP_CKSUM_GOOD |
			       PKT_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV6_UDP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_UDP;
		m->ol_flags |= PKT_RX_IP_CKSUM_GOOD |
			       PKT_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV4_SCTP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_SCTP;
		m->ol_flags |= PKT_RX_IP_CKSUM_GOOD |
			       PKT_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV6_SCTP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_SCTP;
		m->ol_flags |= PKT_RX_IP_CKSUM_GOOD |
			       PKT_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV4_ICMP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV4 |
				 RTE_PTYPE_L4_ICMP;
		m->ol_flags |= PKT_RX_IP_CKSUM_GOOD |
			       PKT_RX_L4_CKSUM_BAD;
		return;
	case ENETC_PARSE_ERROR | ENETC_PKT_TYPE_IPV6_ICMP:
		m->packet_type = RTE_PTYPE_L2_ETHER |
				 RTE_PTYPE_L3_IPV6 |
				 RTE_PTYPE_L4_ICMP;
		m->ol_flags |= PKT_RX_IP_CKSUM_GOOD |
			       PKT_RX_L4_CKSUM_BAD;
		return;
	/* More switch cases can be added */
	default:
		m->packet_type = RTE_PTYPE_UNKNOWN;
		m->ol_flags |= PKT_RX_IP_CKSUM_UNKNOWN |
			       PKT_RX_L4_CKSUM_UNKNOWN;
	}
}


static inline void __attribute__((hot))
enetc_dev_rx_parse(struct rte_mbuf *m, uint16_t parse_results)
{
	ENETC_PMD_DP_DEBUG("parse summary = 0x%x   ", parse_results);
	m->ol_flags |= PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD;

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
	int cleaned_cnt, i;
	struct enetc_swbd *rx_swbd;

	cleaned_cnt = enetc_bd_unused(rx_ring);
	/* next descriptor to process */
	i = rx_ring->next_to_clean;
	rx_swbd = &rx_ring->q_swbd[i];
	while (likely(rx_frm_cnt < work_limit)) {
		union enetc_rx_bd *rxbd;
		uint32_t bd_status;

		if (cleaned_cnt >= ENETC_RXBD_BUNDLE) {
			int count = enetc_refill_rx_ring(rx_ring, cleaned_cnt);

			cleaned_cnt -= count;
		}

		rxbd = ENETC_RXBD(*rx_ring, i);
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

		rx_ring->next_to_clean = i;
		rx_frm_cnt++;
	}

	return rx_frm_cnt;
}

uint16_t
enetc_recv_pkts(void *rxq, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	struct enetc_bdr *rx_ring = (struct enetc_bdr *)rxq;

	return enetc_clean_rx_ring(rx_ring, rx_pkts, nb_pkts);
}
