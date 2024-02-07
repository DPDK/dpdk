/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 Advanced Micro Devices, Inc.
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_atomic.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_prefetch.h>

#include "ionic.h"
#include "ionic_if.h"
#include "ionic_dev.h"
#include "ionic_lif.h"
#include "ionic_rxtx.h"

static __rte_always_inline void
ionic_tx_flush(struct ionic_tx_qcq *txq)
{
	struct ionic_cq *cq = &txq->qcq.cq;
	struct ionic_queue *q = &txq->qcq.q;
	struct rte_mbuf *txm;
	struct ionic_txq_comp *cq_desc_base = cq->base;
	volatile struct ionic_txq_comp *cq_desc;
	void **info;

	cq_desc = &cq_desc_base[cq->tail_idx];

	while (color_match(cq_desc->color, cq->done_color)) {
		cq->tail_idx = Q_NEXT_TO_SRVC(cq, 1);
		if (cq->tail_idx == 0)
			cq->done_color = !cq->done_color;

		/* Prefetch 4 x 16B comp at cq->tail_idx + 4 */
		if ((cq->tail_idx & 0x3) == 0)
			rte_prefetch0(&cq_desc_base[Q_NEXT_TO_SRVC(cq, 4)]);

		while (q->tail_idx != rte_le_to_cpu_16(cq_desc->comp_index)) {
			/* Prefetch 8 mbuf ptrs at q->tail_idx + 2 */
			rte_prefetch0(&q->info[Q_NEXT_TO_SRVC(q, 2)]);

			/* Prefetch next mbuf */
			void **next_info =
				&q->info[Q_NEXT_TO_SRVC(q, 1)];
			if (next_info[0])
				rte_mbuf_prefetch_part2(next_info[0]);

			info = &q->info[q->tail_idx];
			{
				txm = info[0];

				if (txq->flags & IONIC_QCQ_F_FAST_FREE)
					rte_mempool_put(txm->pool, txm);
				else
					rte_pktmbuf_free_seg(txm);

				info[0] = NULL;
			}

			q->tail_idx = Q_NEXT_TO_SRVC(q, 1);
		}

		cq_desc = &cq_desc_base[cq->tail_idx];
	}
}

static __rte_always_inline int
ionic_tx(struct ionic_tx_qcq *txq, struct rte_mbuf *txm)
{
	struct ionic_queue *q = &txq->qcq.q;
	struct ionic_txq_desc *desc, *desc_base = q->base;
	struct ionic_tx_stats *stats = &txq->stats;
	void **info;
	uint64_t ol_flags = txm->ol_flags;
	uint64_t addr, cmd;
	uint8_t opcode = IONIC_TXQ_DESC_OPCODE_CSUM_NONE;
	uint8_t flags = 0;

	if (txm->nb_segs > 1)
		return -EINVAL;

	desc = &desc_base[q->head_idx];
	info = &q->info[q->head_idx];

	if ((ol_flags & RTE_MBUF_F_TX_IP_CKSUM) &&
	    (txq->flags & IONIC_QCQ_F_CSUM_L3)) {
		opcode = IONIC_TXQ_DESC_OPCODE_CSUM_HW;
		flags |= IONIC_TXQ_DESC_FLAG_CSUM_L3;
	}

	if (((ol_flags & RTE_MBUF_F_TX_TCP_CKSUM) &&
	     (txq->flags & IONIC_QCQ_F_CSUM_TCP)) ||
	    ((ol_flags & RTE_MBUF_F_TX_UDP_CKSUM) &&
	     (txq->flags & IONIC_QCQ_F_CSUM_UDP))) {
		opcode = IONIC_TXQ_DESC_OPCODE_CSUM_HW;
		flags |= IONIC_TXQ_DESC_FLAG_CSUM_L4;
	}

	if (opcode == IONIC_TXQ_DESC_OPCODE_CSUM_NONE)
		stats->no_csum++;

	if (((ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM) ||
	     (ol_flags & RTE_MBUF_F_TX_OUTER_UDP_CKSUM)) &&
	    ((ol_flags & RTE_MBUF_F_TX_OUTER_IPV4) ||
	     (ol_flags & RTE_MBUF_F_TX_OUTER_IPV6))) {
		flags |= IONIC_TXQ_DESC_FLAG_ENCAP;
	}

	if (ol_flags & RTE_MBUF_F_TX_VLAN) {
		flags |= IONIC_TXQ_DESC_FLAG_VLAN;
		desc->vlan_tci = rte_cpu_to_le_16(txm->vlan_tci);
	}

	addr = rte_cpu_to_le_64(rte_mbuf_data_iova(txm));

	cmd = encode_txq_desc_cmd(opcode, flags, 0, addr);
	desc->cmd = rte_cpu_to_le_64(cmd);
	desc->len = rte_cpu_to_le_16(txm->data_len);

	info[0] = txm;

	q->head_idx = Q_NEXT_TO_POST(q, 1);

	return 0;
}

uint16_t
ionic_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	struct ionic_tx_qcq *txq = tx_queue;
	struct ionic_queue *q = &txq->qcq.q;
	struct ionic_tx_stats *stats = &txq->stats;
	struct rte_mbuf *mbuf;
	uint32_t bytes_tx = 0;
	uint16_t nb_avail, nb_tx = 0;
	uint64_t then, now, hz, delta;
	int err;

	struct ionic_txq_desc *desc_base = q->base;
	if (!(txq->flags & IONIC_QCQ_F_CMB))
		rte_prefetch0(&desc_base[q->head_idx]);
	rte_prefetch0(&q->info[q->head_idx]);

	if (nb_pkts) {
		rte_mbuf_prefetch_part1(tx_pkts[0]);
		rte_mbuf_prefetch_part2(tx_pkts[0]);
	}

	if (ionic_q_space_avail(q) < txq->free_thresh) {
		/* Cleaning old buffers */
		ionic_tx_flush(txq);
	}

	nb_avail = ionic_q_space_avail(q);
	if (nb_avail < nb_pkts) {
		stats->stop += nb_pkts - nb_avail;
		nb_pkts = nb_avail;
	}

	while (nb_tx < nb_pkts) {
		uint16_t next_idx = Q_NEXT_TO_POST(q, 1);
		if (!(txq->flags & IONIC_QCQ_F_CMB))
			rte_prefetch0(&desc_base[next_idx]);
		rte_prefetch0(&q->info[next_idx]);

		if (nb_tx + 1 < nb_pkts) {
			rte_mbuf_prefetch_part1(tx_pkts[nb_tx + 1]);
			rte_mbuf_prefetch_part2(tx_pkts[nb_tx + 1]);
		}

		mbuf = tx_pkts[nb_tx];

		if (mbuf->ol_flags & RTE_MBUF_F_TX_TCP_SEG)
			err = ionic_tx_tso(txq, mbuf);
		else
			err = ionic_tx(txq, mbuf);
		if (err) {
			stats->drop += nb_pkts - nb_tx;
			break;
		}

		bytes_tx += mbuf->pkt_len;
		nb_tx++;
	}

	if (nb_tx > 0) {
		rte_wmb();
		ionic_q_flush(q);

		txq->last_wdog_cycles = rte_get_timer_cycles();

		stats->packets += nb_tx;
		stats->bytes += bytes_tx;
	} else {
		/*
		 * Ring the doorbell again if no work could be posted and work
		 * is still pending after the deadline.
		 */
		if (q->head_idx != q->tail_idx) {
			then = txq->last_wdog_cycles;
			now = rte_get_timer_cycles();
			hz = rte_get_timer_hz();
			delta = (now - then) * 1000;

			if (delta >= hz * IONIC_Q_WDOG_MS) {
				ionic_q_flush(q);
				txq->last_wdog_cycles = now;
			}
		}
	}

	return nb_tx;
}

/*
 * Cleans one descriptor. Connects the filled mbufs into a chain.
 * Does not advance the tail index.
 */
static __rte_always_inline void
ionic_rx_clean_one(struct ionic_rx_qcq *rxq,
		volatile struct ionic_rxq_comp *cq_desc,
		struct ionic_rx_service *rx_svc)
{
	struct ionic_queue *q = &rxq->qcq.q;
	struct rte_mbuf *rxm;
	struct ionic_rx_stats *stats = &rxq->stats;
	uint64_t pkt_flags = 0;
	uint32_t pkt_type;
	uint16_t cq_desc_len;
	uint8_t ptype, cflags;
	void **info;

	cq_desc_len = rte_le_to_cpu_16(cq_desc->len);

	info = &q->info[q->tail_idx];

	rxm = info[0];

	if (cq_desc->status) {
		stats->bad_cq_status++;
		return;
	}

	if (cq_desc_len > rxq->frame_size || cq_desc_len == 0) {
		stats->bad_len++;
		return;
	}

	info[0] = NULL;

	/* Set the mbuf metadata based on the cq entry */
	rxm->rearm_data[0] = rxq->rearm_data;
	rxm->pkt_len = cq_desc_len;
	rxm->data_len = cq_desc_len;

	/* RSS */
	pkt_flags |= RTE_MBUF_F_RX_RSS_HASH;
	rxm->hash.rss = rte_le_to_cpu_32(cq_desc->rss_hash);

	/* Vlan Strip */
	if (cq_desc->csum_flags & IONIC_RXQ_COMP_CSUM_F_VLAN) {
		pkt_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		rxm->vlan_tci = rte_le_to_cpu_16(cq_desc->vlan_tci);
	}

	/* Checksum */
	if (cq_desc->csum_flags & IONIC_RXQ_COMP_CSUM_F_CALC) {
		cflags = cq_desc->csum_flags & IONIC_CSUM_FLAG_MASK;
		pkt_flags |= ionic_csum_flags[cflags];
	}

	rxm->ol_flags = pkt_flags;

	/* Packet Type */
	ptype = cq_desc->pkt_type_color & IONIC_RXQ_COMP_PKT_TYPE_MASK;
	pkt_type = ionic_ptype_table[ptype];
	if (pkt_type == RTE_PTYPE_UNKNOWN) {
		struct rte_ether_hdr *eth_h = rte_pktmbuf_mtod(rxm,
				struct rte_ether_hdr *);
		uint16_t ether_type = eth_h->ether_type;
		if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP))
			pkt_type = RTE_PTYPE_L2_ETHER_ARP;
		else if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_LLDP))
			pkt_type = RTE_PTYPE_L2_ETHER_LLDP;
		else if (ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_1588))
			pkt_type = RTE_PTYPE_L2_ETHER_TIMESYNC;
		stats->mtods++;
	} else if (pkt_flags & RTE_MBUF_F_RX_VLAN) {
		pkt_type |= RTE_PTYPE_L2_ETHER_VLAN;
	} else {
		pkt_type |= RTE_PTYPE_L2_ETHER;
	}

	rxm->packet_type = pkt_type;

	rx_svc->rx_pkts[rx_svc->nb_rx] = rxm;
	rx_svc->nb_rx++;

	stats->packets++;
	stats->bytes += rxm->pkt_len;
}

/*
 * Fills one descriptor with mbufs. Does not advance the head index.
 */
static __rte_always_inline int
ionic_rx_fill_one(struct ionic_rx_qcq *rxq)
{
	struct ionic_queue *q = &rxq->qcq.q;
	struct rte_mbuf *rxm;
	struct ionic_rxq_desc *desc, *desc_base = q->base;
	rte_iova_t data_iova;
	void **info;
	int ret;

	info = &q->info[q->head_idx];
	desc = &desc_base[q->head_idx];

	/* mbuf is unused */
	if (info[0])
		return 0;

	if (rxq->mb_idx == 0) {
		ret = rte_mempool_get_bulk(rxq->mb_pool,
					(void **)rxq->mbs,
					IONIC_MBUF_BULK_ALLOC);
		if (ret) {
			assert(0);
			return -ENOMEM;
		}

		rxq->mb_idx = IONIC_MBUF_BULK_ALLOC;
	}

	rxm = rxq->mbs[--rxq->mb_idx];
	info[0] = rxm;

	data_iova = rte_mbuf_data_iova_default(rxm);
	desc->addr = rte_cpu_to_le_64(data_iova);

	return 0;
}

/*
 * Walk the CQ to find completed receive descriptors.
 * Any completed descriptor found is refilled.
 */
static __rte_always_inline void
ionic_rxq_service(struct ionic_rx_qcq *rxq, uint32_t work_to_do,
		struct ionic_rx_service *rx_svc)
{
	struct ionic_cq *cq = &rxq->qcq.cq;
	struct ionic_queue *q = &rxq->qcq.q;
	struct ionic_rxq_desc *q_desc_base = q->base;
	struct ionic_rxq_comp *cq_desc_base = cq->base;
	volatile struct ionic_rxq_comp *cq_desc;
	uint32_t work_done = 0;
	uint64_t then, now, hz, delta;

	cq_desc = &cq_desc_base[cq->tail_idx];

	while (color_match(cq_desc->pkt_type_color, cq->done_color)) {
		cq->tail_idx = Q_NEXT_TO_SRVC(cq, 1);
		if (cq->tail_idx == 0)
			cq->done_color = !cq->done_color;

		/* Prefetch 8 x 8B bufinfo */
		rte_prefetch0(&q->info[Q_NEXT_TO_SRVC(q, 8)]);
		/* Prefetch 4 x 16B comp */
		rte_prefetch0(&cq_desc_base[Q_NEXT_TO_SRVC(cq, 4)]);
		/* Prefetch 4 x 16B descriptors */
		if (!(rxq->flags & IONIC_QCQ_F_CMB))
			rte_prefetch0(&q_desc_base[Q_NEXT_TO_POST(q, 4)]);

		/* Clean one descriptor */
		ionic_rx_clean_one(rxq, cq_desc, rx_svc);
		q->tail_idx = Q_NEXT_TO_SRVC(q, 1);

		/* Fill one descriptor */
		(void)ionic_rx_fill_one(rxq);

		q->head_idx = Q_NEXT_TO_POST(q, 1);

		if (++work_done == work_to_do)
			break;

		cq_desc = &cq_desc_base[cq->tail_idx];
	}

	/* Update the queue indices and ring the doorbell */
	if (work_done) {
		ionic_q_flush(q);
		rxq->last_wdog_cycles = rte_get_timer_cycles();
		rxq->wdog_ms = IONIC_Q_WDOG_MS;
	} else {
		/*
		 * Ring the doorbell again if no recvs were posted and the
		 * recv queue is not empty after the deadline.
		 *
		 * Exponentially back off the deadline to avoid excessive
		 * doorbells when the recv queue is idle.
		 */
		if (q->head_idx != q->tail_idx) {
			then = rxq->last_wdog_cycles;
			now = rte_get_timer_cycles();
			hz = rte_get_timer_hz();
			delta = (now - then) * 1000;

			if (delta >= hz * rxq->wdog_ms) {
				ionic_q_flush(q);
				rxq->last_wdog_cycles = now;

				delta = 2 * rxq->wdog_ms;
				if (delta > IONIC_Q_WDOG_MAX_MS)
					delta = IONIC_Q_WDOG_MAX_MS;

				rxq->wdog_ms = delta;
			}
		}
	}
}

uint16_t
ionic_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	struct ionic_rx_qcq *rxq = rx_queue;
	struct ionic_rx_service rx_svc;

	rx_svc.rx_pkts = rx_pkts;
	rx_svc.nb_rx = 0;

	ionic_rxq_service(rxq, nb_pkts, &rx_svc);

	return rx_svc.nb_rx;
}

/*
 * Fills all descriptors with mbufs.
 */
int __rte_cold
ionic_rx_fill(struct ionic_rx_qcq *rxq)
{
	struct ionic_queue *q = &rxq->qcq.q;
	uint32_t i;
	int err = 0;

	for (i = 0; i < q->num_descs - 1u; i++) {
		err = ionic_rx_fill_one(rxq);
		if (err)
			break;

		q->head_idx = Q_NEXT_TO_POST(q, 1);
	}

	ionic_q_flush(q);

	return err;
}
