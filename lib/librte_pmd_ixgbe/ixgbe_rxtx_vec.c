/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <stdint.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>

#include "ixgbe_ethdev.h"
#include "ixgbe_rxtx.h"

#include <nmmintrin.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static struct rte_mbuf mb_def = {

	.ol_flags = 0,
	{
		.pkt = {
			.data_len = 0,
			.pkt_len = 0,

			.vlan_macip = {
				.data = 0,
			},
			.hash = {
				.rss = 0,
			},

			.nb_segs = 1,
			.in_port = 0,

			.next = NULL,
			.data = NULL,
		},
	},
};

static inline void
ixgbe_rxq_rearm(struct igb_rx_queue *rxq)
{
	int i;
	uint16_t rx_id;
	volatile union ixgbe_adv_rx_desc *rxdp;
	struct igb_rx_entry *rxep = &rxq->sw_ring[rxq->rxrearm_start];
	struct rte_mbuf *mb0, *mb1;
	__m128i def_low;
	__m128i hdr_room = _mm_set_epi64x(RTE_PKTMBUF_HEADROOM,
			RTE_PKTMBUF_HEADROOM);

	/* Pull 'n' more MBUFs into the software ring */
	if (rte_mempool_get_bulk(rxq->mb_pool,
				 (void *)rxep, RTE_IXGBE_RXQ_REARM_THRESH) < 0)
		return;

	rxdp = rxq->rx_ring + rxq->rxrearm_start;

	def_low = _mm_load_si128((__m128i *)&(mb_def.pkt));

	/* Initialize the mbufs in vector, process 2 mbufs in one loop */
	for (i = 0; i < RTE_IXGBE_RXQ_REARM_THRESH; i += 2, rxep += 2) {
		__m128i dma_addr0, dma_addr1;
		__m128i vaddr0, vaddr1;

		mb0 = rxep[0].mbuf;
		mb1 = rxep[1].mbuf;

		/* load buf_addr(lo 64bit) and buf_physaddr(hi 64bit) */
		vaddr0 = _mm_loadu_si128((__m128i *)&(mb0->buf_addr));
		vaddr1 = _mm_loadu_si128((__m128i *)&(mb1->buf_addr));

		/* calc va/pa of pkt data point */
		vaddr0 = _mm_add_epi64(vaddr0, hdr_room);
		vaddr1 = _mm_add_epi64(vaddr1, hdr_room);

		/* convert pa to dma_addr hdr/data */
		dma_addr0 = _mm_unpackhi_epi64(vaddr0, vaddr0);
		dma_addr1 = _mm_unpackhi_epi64(vaddr1, vaddr1);

		/* fill va into t0 def pkt template */
		vaddr0 = _mm_unpacklo_epi64(def_low, vaddr0);
		vaddr1 = _mm_unpacklo_epi64(def_low, vaddr1);

		/* flush desc with pa dma_addr */
		_mm_store_si128((__m128i *)&rxdp++->read, dma_addr0);
		_mm_store_si128((__m128i *)&rxdp++->read, dma_addr1);

		/* flush mbuf with pkt template */
		_mm_store_si128((__m128i *)&mb0->pkt, vaddr0);
		_mm_store_si128((__m128i *)&mb1->pkt, vaddr1);

		/* update refcnt per pkt */
		rte_mbuf_refcnt_set(mb0, 1);
		rte_mbuf_refcnt_set(mb1, 1);
	}

	rxq->rxrearm_start += RTE_IXGBE_RXQ_REARM_THRESH;
	if (rxq->rxrearm_start >= rxq->nb_rx_desc)
		rxq->rxrearm_start = 0;

	rxq->rxrearm_nb -= RTE_IXGBE_RXQ_REARM_THRESH;

	rx_id = (uint16_t) ((rxq->rxrearm_start == 0) ?
			     (rxq->nb_rx_desc - 1) : (rxq->rxrearm_start - 1));

	/* Update the tail pointer on the NIC */
	IXGBE_PCI_REG_WRITE(rxq->rdt_reg_addr, rx_id);
}

/* Handling the offload flags (olflags) field takes computation
 * time when receiving packets. Therefore we provide a flag to disable
 * the processing of the olflags field when they are not needed. This
 * gives improved performance, at the cost of losing the offload info
 * in the received packet
 */
#ifdef RTE_IXGBE_RX_OLFLAGS_ENABLE

#define OLFLAGS_MASK     ((uint16_t)(PKT_RX_VLAN_PKT | PKT_RX_IPV4_HDR |\
				     PKT_RX_IPV4_HDR_EXT | PKT_RX_IPV6_HDR |\
				     PKT_RX_IPV6_HDR_EXT))
#define OLFLAGS_MASK_V   (((uint64_t)OLFLAGS_MASK << 48) | \
			  ((uint64_t)OLFLAGS_MASK << 32) | \
			  ((uint64_t)OLFLAGS_MASK << 16) | \
			  ((uint64_t)OLFLAGS_MASK))
#define PTYPE_SHIFT    (1)
#define VTAG_SHIFT     (3)

static inline void
desc_to_olflags_v(__m128i descs[4], struct rte_mbuf **rx_pkts)
{
	__m128i ptype0, ptype1, vtag0, vtag1;
	union {
		uint16_t e[4];
		uint64_t dword;
	} vol;

	ptype0 = _mm_unpacklo_epi16(descs[0], descs[1]);
	ptype1 = _mm_unpacklo_epi16(descs[2], descs[3]);
	vtag0 = _mm_unpackhi_epi16(descs[0], descs[1]);
	vtag1 = _mm_unpackhi_epi16(descs[2], descs[3]);

	ptype1 = _mm_unpacklo_epi32(ptype0, ptype1);
	vtag1 = _mm_unpacklo_epi32(vtag0, vtag1);

	ptype1 = _mm_slli_epi16(ptype1, PTYPE_SHIFT);
	vtag1 = _mm_srli_epi16(vtag1, VTAG_SHIFT);

	ptype1 = _mm_or_si128(ptype1, vtag1);
	vol.dword = _mm_cvtsi128_si64(ptype1) & OLFLAGS_MASK_V;

	rx_pkts[0]->ol_flags = vol.e[0];
	rx_pkts[1]->ol_flags = vol.e[1];
	rx_pkts[2]->ol_flags = vol.e[2];
	rx_pkts[3]->ol_flags = vol.e[3];
}
#else
#define desc_to_olflags_v(desc, rx_pkts) do {} while (0)
#endif

/*
 * vPMD receive routine, now only accept (nb_pkts == RTE_IXGBE_VPMD_RX_BURST)
 * in one loop
 *
 * Notice:
 * - nb_pkts < RTE_IXGBE_VPMD_RX_BURST, just return no packet
 * - nb_pkts > RTE_IXGBE_VPMD_RX_BURST, only scan RTE_IXGBE_VPMD_RX_BURST
 *   numbers of DD bit
 * - don't support ol_flags for rss and csum err
 */
uint16_t
ixgbe_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	volatile union ixgbe_adv_rx_desc *rxdp;
	struct igb_rx_queue *rxq = rx_queue;
	struct igb_rx_entry *sw_ring;
	uint16_t nb_pkts_recd;
	int pos;
	uint64_t var;
	__m128i shuf_msk;
	__m128i in_port;
	__m128i dd_check;

	if (unlikely(nb_pkts < RTE_IXGBE_VPMD_RX_BURST))
		return 0;

	/* Just the act of getting into the function from the application is
	 * going to cost about 7 cycles */
	rxdp = rxq->rx_ring + rxq->rx_tail;

	_mm_prefetch((const void *)rxdp, _MM_HINT_T0);

	/* See if we need to rearm the RX queue - gives the prefetch a bit
	 * of time to act */
	if (rxq->rxrearm_nb > RTE_IXGBE_RXQ_REARM_THRESH)
		ixgbe_rxq_rearm(rxq);

	/* Before we start moving massive data around, check to see if
	 * there is actually a packet available */
	if (!(rxdp->wb.upper.status_error &
				rte_cpu_to_le_32(IXGBE_RXDADV_STAT_DD)))
		return 0;

	/* 4 packets DD mask */
	dd_check = _mm_set_epi64x(0x0000000100000001LL, 0x0000000100000001LL);

	/* mask to shuffle from desc. to mbuf */
	shuf_msk = _mm_set_epi8(
		7, 6, 5, 4,  /* octet 4~7, 32bits rss */
		0xFF, 0xFF,  /* skip high 16 bits vlan_macip, zero out */
		15, 14,      /* octet 14~15, low 16 bits vlan_macip */
		0xFF, 0xFF,  /* skip high 16 bits pkt_len, zero out */
		13, 12,      /* octet 12~13, low 16 bits pkt_len */
		0xFF, 0xFF,  /* skip nb_segs and in_port, zero out */
		13, 12       /* octet 12~13, 16 bits data_len */
		);


	/* Cache is empty -> need to scan the buffer rings, but first move
	 * the next 'n' mbufs into the cache */
	sw_ring = &rxq->sw_ring[rxq->rx_tail];

	/* in_port, nb_seg = 1, crc_len */
	in_port = rxq->misc_info;

	/*
	 * A. load 4 packet in one loop
	 * B. copy 4 mbuf point from swring to rx_pkts
	 * C. calc the number of DD bits among the 4 packets
	 * D. fill info. from desc to mbuf
	 */
	for (pos = 0, nb_pkts_recd = 0; pos < RTE_IXGBE_VPMD_RX_BURST;
			pos += RTE_IXGBE_DESCS_PER_LOOP,
			rxdp += RTE_IXGBE_DESCS_PER_LOOP) {
		__m128i descs[RTE_IXGBE_DESCS_PER_LOOP];
		__m128i pkt_mb1, pkt_mb2, pkt_mb3, pkt_mb4;
		__m128i zero, staterr, sterr_tmp1, sterr_tmp2;
		__m128i mbp1, mbp2; /* two mbuf pointer in one XMM reg. */

		/* B.1 load 1 mbuf point */
		mbp1 = _mm_loadu_si128((__m128i *)&sw_ring[pos]);

		/* Read desc statuses backwards to avoid race condition */
		/* A.1 load 4 pkts desc */
		descs[3] = _mm_loadu_si128((__m128i *)(rxdp + 3));

		/* B.2 copy 2 mbuf point into rx_pkts  */
		_mm_store_si128((__m128i *)&rx_pkts[pos], mbp1);

		/* B.1 load 1 mbuf point */
		mbp2 = _mm_loadu_si128((__m128i *)&sw_ring[pos+2]);

		descs[2] = _mm_loadu_si128((__m128i *)(rxdp + 2));
		/* B.1 load 2 mbuf point */
		descs[1] = _mm_loadu_si128((__m128i *)(rxdp + 1));
		descs[0] = _mm_loadu_si128((__m128i *)(rxdp));

		/* B.2 copy 2 mbuf point into rx_pkts  */
		_mm_store_si128((__m128i *)&rx_pkts[pos+2], mbp2);

		/* avoid compiler reorder optimization */
		rte_compiler_barrier();

		/* D.1 pkt 3,4 convert format from desc to pktmbuf */
		pkt_mb4 = _mm_shuffle_epi8(descs[3], shuf_msk);
		pkt_mb3 = _mm_shuffle_epi8(descs[2], shuf_msk);

		/* C.1 4=>2 filter staterr info only */
		sterr_tmp2 = _mm_unpackhi_epi32(descs[3], descs[2]);
		/* C.1 4=>2 filter staterr info only */
		sterr_tmp1 = _mm_unpackhi_epi32(descs[1], descs[0]);

		/* set ol_flags with packet type and vlan tag */
		desc_to_olflags_v(descs, &rx_pkts[pos]);

		/* D.2 pkt 3,4 set in_port/nb_seg and remove crc */
		pkt_mb4 = _mm_add_epi16(pkt_mb4, in_port);
		pkt_mb3 = _mm_add_epi16(pkt_mb3, in_port);

		/* D.1 pkt 1,2 convert format from desc to pktmbuf */
		pkt_mb2 = _mm_shuffle_epi8(descs[1], shuf_msk);
		pkt_mb1 = _mm_shuffle_epi8(descs[0], shuf_msk);

		/* C.2 get 4 pkts staterr value  */
		zero = _mm_xor_si128(dd_check, dd_check);
		staterr = _mm_unpacklo_epi32(sterr_tmp1, sterr_tmp2);

		/* D.3 copy final 3,4 data to rx_pkts */
		_mm_storeu_si128((__m128i *)&(rx_pkts[pos+3]->pkt.data_len),
				pkt_mb4);
		_mm_storeu_si128((__m128i *)&(rx_pkts[pos+2]->pkt.data_len),
				pkt_mb3);

		/* D.2 pkt 1,2 set in_port/nb_seg and remove crc */
		pkt_mb2 = _mm_add_epi16(pkt_mb2, in_port);
		pkt_mb1 = _mm_add_epi16(pkt_mb1, in_port);

		/* C.3 calc avaialbe number of desc */
		staterr = _mm_and_si128(staterr, dd_check);
		staterr = _mm_packs_epi32(staterr, zero);

		/* D.3 copy final 1,2 data to rx_pkts */
		_mm_storeu_si128((__m128i *)&(rx_pkts[pos+1]->pkt.data_len),
				pkt_mb2);
		_mm_storeu_si128((__m128i *)&(rx_pkts[pos]->pkt.data_len),
				pkt_mb1);

		/* C.4 calc avaialbe number of desc */
		var = _mm_popcnt_u64(_mm_cvtsi128_si64(staterr));
		nb_pkts_recd += var;
		if (likely(var != RTE_IXGBE_DESCS_PER_LOOP))
			break;
	}

	/* Update our internal tail pointer */
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_pkts_recd);
	rxq->rx_tail = (uint16_t)(rxq->rx_tail & (rxq->nb_rx_desc - 1));
	rxq->rxrearm_nb = (uint16_t)(rxq->rxrearm_nb + nb_pkts_recd);

	return nb_pkts_recd;
}

static inline void
vtx1(volatile union ixgbe_adv_tx_desc *txdp,
		struct rte_mbuf *pkt, __m128i flags)
{
	__m128i t0, t1, offset, ols, ba, ctl;

	/* load buf_addr/buf_physaddr in t0 */
	t0 = _mm_loadu_si128((__m128i *)&(pkt->buf_addr));
	/* load data, ... pkt_len in t1 */
	t1 = _mm_loadu_si128((__m128i *)&(pkt->pkt.data));

	/* calc offset = (data - buf_adr) */
	offset = _mm_sub_epi64(t1, t0);

	/* cmd_type_len: pkt_len |= DCMD_DTYP_FLAGS */
	ctl = _mm_or_si128(t1, flags);

	/* reorder as buf_physaddr/buf_addr */
	offset = _mm_shuffle_epi32(offset, 0x4E);

	/* olinfo_stats: pkt_len << IXGBE_ADVTXD_PAYLEN_SHIFT */
	ols = _mm_slli_epi32(t1, IXGBE_ADVTXD_PAYLEN_SHIFT);

	/* buffer_addr = buf_physaddr + offset */
	ba = _mm_add_epi64(t0, offset);

	/* format cmd_type_len/olinfo_status */
	ctl = _mm_unpackhi_epi32(ctl, ols);

	/* format buf_physaddr/cmd_type_len */
	ba = _mm_unpackhi_epi64(ba, ctl);

	/* write desc */
	_mm_store_si128((__m128i *)&txdp->read, ba);
}

static inline void
vtx(volatile union ixgbe_adv_tx_desc *txdp,
		struct rte_mbuf **pkt, uint16_t nb_pkts,  __m128i flags)
{
	int i;
	for (i = 0; i < nb_pkts; ++i, ++txdp, ++pkt)
		vtx1(txdp, *pkt, flags);
}

static inline int __attribute__((always_inline))
ixgbe_tx_free_bufs(struct igb_tx_queue *txq)
{
	struct igb_tx_entry_v *txep;
	struct igb_tx_entry_seq *txsp;
	uint32_t status;
	uint32_t n, k;
#ifdef RTE_MBUF_SCATTER_GATHER
	uint32_t i;
	int nb_free = 0;
	struct rte_mbuf *m, *free[RTE_IXGBE_TX_MAX_FREE_BUF_SZ];
#endif

	/* check DD bit on threshold descriptor */
	status = txq->tx_ring[txq->tx_next_dd].wb.status;
	if (!(status & IXGBE_ADVTXD_STAT_DD))
		return 0;

	n = txq->tx_rs_thresh;

	/*
	 * first buffer to free from S/W ring is at index
	 * tx_next_dd - (tx_rs_thresh-1)
	 */
	txep = &((struct igb_tx_entry_v *)txq->sw_ring)[txq->tx_next_dd -
			(n - 1)];
	txsp = &txq->sw_ring_seq[txq->tx_next_dd - (n - 1)];

	while (n > 0) {
		k = RTE_MIN(n, txsp[n-1].same_pool);
#ifdef RTE_MBUF_SCATTER_GATHER
		for (i = 0; i < k; i++) {
			m = __rte_pktmbuf_prefree_seg((txep+n-k+i)->mbuf);
			if (m != NULL)
				free[nb_free++] = m;
		}
		rte_mempool_put_bulk((void *)txsp[n-1].pool,
				(void **)free, nb_free);
#else
		rte_mempool_put_bulk((void *)txsp[n-1].pool,
				(void **)(txep+n-k), k);
#endif
		n -= k;
	}

	/* buffers were freed, update counters */
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + txq->tx_rs_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_rs_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);

	return txq->tx_rs_thresh;
}

static inline void __attribute__((always_inline))
tx_backlog_entry(struct igb_tx_entry_v *txep,
		 struct igb_tx_entry_seq *txsp,
		 struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i;
	for (i = 0; i < (int)nb_pkts; ++i) {
		txep[i].mbuf = tx_pkts[i];
		/* check and update sequence number */
		txsp[i].pool = tx_pkts[i]->pool;
		if (txsp[i-1].pool == tx_pkts[i]->pool)
			txsp[i].same_pool = txsp[i-1].same_pool + 1;
		else
			txsp[i].same_pool = 1;
	}
}

uint16_t
ixgbe_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts)
{
	struct igb_tx_queue *txq = (struct igb_tx_queue *)tx_queue;
	volatile union ixgbe_adv_tx_desc *txdp;
	struct igb_tx_entry_v *txep;
	struct igb_tx_entry_seq *txsp;
	uint16_t n, nb_commit, tx_id;
	__m128i flags = _mm_set_epi32(DCMD_DTYP_FLAGS, 0, 0, 0);
	__m128i rs = _mm_set_epi32(IXGBE_ADVTXD_DCMD_RS|DCMD_DTYP_FLAGS,
			0, 0, 0);
	int i;

	if (unlikely(nb_pkts > RTE_IXGBE_VPMD_TX_BURST))
		nb_pkts = RTE_IXGBE_VPMD_TX_BURST;

	if (txq->nb_tx_free < txq->tx_free_thresh)
		ixgbe_tx_free_bufs(txq);

	nb_commit = nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = &((struct igb_tx_entry_v *)txq->sw_ring)[tx_id];
	txsp = &txq->sw_ring_seq[tx_id];

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {

		tx_backlog_entry(txep, txsp, tx_pkts, n);

		for (i = 0; i < n - 1; ++i, ++tx_pkts, ++txdp)
			vtx1(txdp, *tx_pkts, flags);

		vtx1(txdp, *tx_pkts++, rs);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &(txq->tx_ring[tx_id]);
		txep = &(((struct igb_tx_entry_v *)txq->sw_ring)[tx_id]);
		txsp = &(txq->sw_ring_seq[tx_id]);
	}

	tx_backlog_entry(txep, txsp, tx_pkts, nb_commit);

	vtx(txdp, tx_pkts, nb_commit, flags);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->tx_next_rs) {
		txq->tx_ring[txq->tx_next_rs].read.cmd_type_len |=
			rte_cpu_to_le_32(IXGBE_ADVTXD_DCMD_RS);
		txq->tx_next_rs = (uint16_t)(txq->tx_next_rs +
			txq->tx_rs_thresh);
	}

	txq->tx_tail = tx_id;

	IXGBE_PCI_REG_WRITE(txq->tdt_reg_addr, txq->tx_tail);

	return nb_pkts;
}

static void
ixgbe_tx_queue_release_mbufs(struct igb_tx_queue *txq)
{
	unsigned i;
	struct igb_tx_entry_v *txe;
	struct igb_tx_entry_seq *txs;
	uint16_t nb_free, max_desc;

	if (txq->sw_ring != NULL) {
		/* release the used mbufs in sw_ring */
		nb_free = txq->nb_tx_free;
		max_desc = (uint16_t)(txq->nb_tx_desc - 1);
		for (i = txq->tx_next_dd - (txq->tx_rs_thresh - 1);
		     nb_free < max_desc && i != txq->tx_tail;
		     i = (i + 1) & max_desc) {
			txe = (struct igb_tx_entry_v *)&txq->sw_ring[i];
			if (txe->mbuf != NULL)
				rte_pktmbuf_free_seg(txe->mbuf);
		}
		/* reset tx_entry */
		for (i = 0; i < txq->nb_tx_desc; i++) {
			txe = (struct igb_tx_entry_v *)&txq->sw_ring[i];
			txe->mbuf = NULL;

			txs = &txq->sw_ring_seq[i];
			txs->pool = NULL;
			txs->same_pool = 0;
		}
	}
}

static void
ixgbe_tx_free_swring(struct igb_tx_queue *txq)
{
	if (txq == NULL)
		return;

	if (txq->sw_ring != NULL) {
		rte_free((struct igb_rx_entry *)txq->sw_ring - 1);
		txq->sw_ring = NULL;
	}

	if (txq->sw_ring_seq != NULL) {
		rte_free(txq->sw_ring_seq - 1);
		txq->sw_ring_seq = NULL;
	}
}

static void
ixgbe_reset_tx_queue(struct igb_tx_queue *txq)
{
	static const union ixgbe_adv_tx_desc zeroed_desc = { .read = {
			.buffer_addr = 0} };
	struct igb_tx_entry_v *txe = (struct igb_tx_entry_v *)txq->sw_ring;
	struct igb_tx_entry_seq *txs = txq->sw_ring_seq;
	uint16_t i;

	/* Zero out HW ring memory */
	for (i = 0; i < txq->nb_tx_desc; i++)
		txq->tx_ring[i] = zeroed_desc;

	/* Initialize SW ring entries */
	for (i = 0; i < txq->nb_tx_desc; i++) {
		volatile union ixgbe_adv_tx_desc *txd = &txq->tx_ring[i];
		txd->wb.status = IXGBE_TXD_STAT_DD;
		txe[i].mbuf = NULL;
		txs[i].pool = NULL;
		txs[i].same_pool = 0;
	}

	txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);
	txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

	txq->tx_tail = 0;
	txq->nb_tx_used = 0;
	/*
	 * Always allow 1 descriptor to be un-allocated to avoid
	 * a H/W race condition
	 */
	txq->last_desc_cleaned = (uint16_t)(txq->nb_tx_desc - 1);
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_desc - 1);
	txq->ctx_curr = 0;
	memset((void *)&txq->ctx_cache, 0,
		IXGBE_CTX_NUM * sizeof(struct ixgbe_advctx_info));
}

static struct ixgbe_txq_ops vec_txq_ops = {
	.release_mbufs = ixgbe_tx_queue_release_mbufs,
	.free_swring = ixgbe_tx_free_swring,
	.reset = ixgbe_reset_tx_queue,
};

int ixgbe_txq_vec_setup(struct igb_tx_queue *txq,
			unsigned int socket_id)
{
	uint16_t nb_desc;

	if (txq->sw_ring == NULL)
		return -1;

	/* request addtional one entry for continous sequence check */
	nb_desc = (uint16_t)(txq->nb_tx_desc + 1);

	txq->sw_ring_seq = rte_zmalloc_socket("txq->sw_ring_seq",
				sizeof(struct igb_tx_entry_seq) * nb_desc,
				CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring_seq == NULL)
		return -1;


	/* leave the first one for overflow */
	txq->sw_ring = (struct igb_tx_entry *)
		((struct igb_tx_entry_v *)txq->sw_ring + 1);
	txq->sw_ring_seq += 1;
	txq->ops = &vec_txq_ops;

	return 0;
}

int ixgbe_rxq_vec_setup(struct igb_rx_queue *rxq,
			__rte_unused unsigned int socket_id)
{
	rxq->misc_info =
		_mm_set_epi16(
			0, 0, 0, 0, 0,
			(uint16_t)-rxq->crc_len, /* sub crc on pkt_len */
			(uint16_t)(rxq->port_id << 8 | 1),
			/* 8b port_id and 8b nb_seg*/
			(uint16_t)-rxq->crc_len  /* sub crc on data_len */
			);

	return 0;
}

int ixgbe_rx_vec_condition_check(struct rte_eth_dev *dev)
{
#ifndef RTE_LIBRTE_IEEE1588
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	struct rte_fdir_conf *fconf = &dev->data->dev_conf.fdir_conf;

#ifndef RTE_IXGBE_RX_OLFLAGS_ENABLE
	/* whithout rx ol_flags, no VP flag report */
	if (rxmode->hw_vlan_strip != 0 ||
	    rxmode->hw_vlan_extend != 0)
		return -1;
#endif

	/* no fdir support */
	if (fconf->mode != RTE_FDIR_MODE_NONE)
		return -1;

	/*
	 * - no csum error report support
	 * - no header split support
	 */
	if (rxmode->hw_ip_checksum == 1 ||
	    rxmode->header_split == 1)
		return -1;

	return 0;
#else
	RTE_SET_USED(dev);
	return -1;
#endif
}
