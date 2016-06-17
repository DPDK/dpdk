/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2016.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium networks nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_log.h>
#include <rte_mbuf.h>
#include <rte_prefetch.h>

#include "base/nicvf_plat.h"

#include "nicvf_ethdev.h"
#include "nicvf_rxtx.h"
#include "nicvf_logs.h"

static inline void __hot
fill_sq_desc_header(union sq_entry_t *entry, struct rte_mbuf *pkt)
{
	/* Local variable sqe to avoid read from sq desc memory*/
	union sq_entry_t sqe;
	uint64_t ol_flags;

	/* Fill SQ header descriptor */
	sqe.buff[0] = 0;
	sqe.hdr.subdesc_type = SQ_DESC_TYPE_HEADER;
	/* Number of sub-descriptors following this one */
	sqe.hdr.subdesc_cnt = pkt->nb_segs;
	sqe.hdr.tot_len = pkt->pkt_len;

	ol_flags = pkt->ol_flags & NICVF_TX_OFFLOAD_MASK;
	if (unlikely(ol_flags)) {
		/* L4 cksum */
		if (ol_flags & PKT_TX_TCP_CKSUM)
			sqe.hdr.csum_l4 = SEND_L4_CSUM_TCP;
		else if (ol_flags & PKT_TX_UDP_CKSUM)
			sqe.hdr.csum_l4 = SEND_L4_CSUM_UDP;
		else
			sqe.hdr.csum_l4 = SEND_L4_CSUM_DISABLE;
		sqe.hdr.l4_offset = pkt->l3_len + pkt->l2_len;

		/* L3 cksum */
		if (ol_flags & PKT_TX_IP_CKSUM) {
			sqe.hdr.csum_l3 = 1;
			sqe.hdr.l3_offset = pkt->l2_len;
		}
	}

	entry->buff[0] = sqe.buff[0];
}

void __hot
nicvf_single_pool_free_xmited_buffers(struct nicvf_txq *sq)
{
	int j = 0;
	uint32_t curr_head;
	uint32_t head = sq->head;
	struct rte_mbuf **txbuffs = sq->txbuffs;
	void *obj_p[NICVF_MAX_TX_FREE_THRESH] __rte_cache_aligned;

	curr_head = nicvf_addr_read(sq->sq_head) >> 4;
	while (head != curr_head) {
		if (txbuffs[head])
			obj_p[j++] = txbuffs[head];

		head = (head + 1) & sq->qlen_mask;
	}

	rte_mempool_put_bulk(sq->pool, obj_p, j);
	sq->head = curr_head;
	sq->xmit_bufs -= j;
	NICVF_TX_ASSERT(sq->xmit_bufs >= 0);
}

void __hot
nicvf_multi_pool_free_xmited_buffers(struct nicvf_txq *sq)
{
	uint32_t n = 0;
	uint32_t curr_head;
	uint32_t head = sq->head;
	struct rte_mbuf **txbuffs = sq->txbuffs;

	curr_head = nicvf_addr_read(sq->sq_head) >> 4;
	while (head != curr_head) {
		if (txbuffs[head]) {
			rte_pktmbuf_free_seg(txbuffs[head]);
			n++;
		}

		head = (head + 1) & sq->qlen_mask;
	}

	sq->head = curr_head;
	sq->xmit_bufs -= n;
	NICVF_TX_ASSERT(sq->xmit_bufs >= 0);
}

static inline uint32_t __hot
nicvf_free_tx_desc(struct nicvf_txq *sq)
{
	return ((sq->head - sq->tail - 1) & sq->qlen_mask);
}

/* Send Header + Packet */
#define TX_DESC_PER_PKT 2

static inline uint32_t __hot
nicvf_free_xmitted_buffers(struct nicvf_txq *sq, struct rte_mbuf **tx_pkts,
			    uint16_t nb_pkts)
{
	uint32_t free_desc = nicvf_free_tx_desc(sq);

	if (free_desc < nb_pkts * TX_DESC_PER_PKT ||
			sq->xmit_bufs > sq->tx_free_thresh) {
		if (unlikely(sq->pool == NULL))
			sq->pool = tx_pkts[0]->pool;

		sq->pool_free(sq);
		/* Freed now, let see the number of free descs again */
		free_desc = nicvf_free_tx_desc(sq);
	}
	return free_desc;
}

uint16_t __hot
nicvf_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i;
	uint32_t free_desc;
	uint32_t tail;
	struct nicvf_txq *sq = tx_queue;
	union sq_entry_t *desc_ptr = sq->desc;
	struct rte_mbuf **txbuffs = sq->txbuffs;
	struct rte_mbuf *pkt;
	uint32_t qlen_mask = sq->qlen_mask;

	tail = sq->tail;
	free_desc = nicvf_free_xmitted_buffers(sq, tx_pkts, nb_pkts);

	for (i = 0; i < nb_pkts && (int)free_desc >= TX_DESC_PER_PKT; i++) {
		pkt = tx_pkts[i];

		txbuffs[tail] = NULL;
		fill_sq_desc_header(desc_ptr + tail, pkt);
		tail = (tail + 1) & qlen_mask;

		txbuffs[tail] = pkt;
		fill_sq_desc_gather(desc_ptr + tail, pkt);
		tail = (tail + 1) & qlen_mask;
		free_desc -= TX_DESC_PER_PKT;
	}

	sq->tail = tail;
	sq->xmit_bufs += i;
	rte_wmb();

	/* Inform HW to xmit the packets */
	nicvf_addr_write(sq->sq_door, i * TX_DESC_PER_PKT);
	return i;
}

uint16_t __hot
nicvf_xmit_pkts_multiseg(void *tx_queue, struct rte_mbuf **tx_pkts,
			 uint16_t nb_pkts)
{
	int i, k;
	uint32_t used_desc, next_used_desc, used_bufs, free_desc, tail;
	struct nicvf_txq *sq = tx_queue;
	union sq_entry_t *desc_ptr = sq->desc;
	struct rte_mbuf **txbuffs = sq->txbuffs;
	struct rte_mbuf *pkt, *seg;
	uint32_t qlen_mask = sq->qlen_mask;
	uint16_t nb_segs;

	tail = sq->tail;
	used_desc = 0;
	used_bufs = 0;

	free_desc = nicvf_free_xmitted_buffers(sq, tx_pkts, nb_pkts);

	for (i = 0; i < nb_pkts; i++) {
		pkt = tx_pkts[i];

		nb_segs = pkt->nb_segs;

		next_used_desc = used_desc + nb_segs + 1;
		if (next_used_desc > free_desc)
			break;
		used_desc = next_used_desc;
		used_bufs += nb_segs;

		txbuffs[tail] = NULL;
		fill_sq_desc_header(desc_ptr + tail, pkt);
		tail = (tail + 1) & qlen_mask;

		txbuffs[tail] = pkt;
		fill_sq_desc_gather(desc_ptr + tail, pkt);
		tail = (tail + 1) & qlen_mask;

		seg = pkt->next;
		for (k = 1; k < nb_segs; k++) {
			txbuffs[tail] = seg;
			fill_sq_desc_gather(desc_ptr + tail, seg);
			tail = (tail + 1) & qlen_mask;
			seg = seg->next;
		}
	}

	sq->tail = tail;
	sq->xmit_bufs += used_bufs;
	rte_wmb();

	/* Inform HW to xmit the packets */
	nicvf_addr_write(sq->sq_door, used_desc);
	return nb_pkts;
}
