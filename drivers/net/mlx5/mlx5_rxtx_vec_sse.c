/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <smmintrin.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/mlx5_hw.h>
#include <infiniband/arch.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

/**
 * Fill in buffer descriptors in a multi-packet send descriptor.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param dseg
 *   Pointer to buffer descriptor to be writen.
 * @param pkts
 *   Pointer to array of packets to be sent.
 * @param n
 *   Number of packets to be filled.
 */
static inline void
txq_wr_dseg_v(struct txq *txq, __m128i *dseg,
	      struct rte_mbuf **pkts, unsigned int n)
{
	unsigned int pos;
	uintptr_t addr;
	const __m128i shuf_mask_dseg =
		_mm_set_epi8(8,  9, 10, 11, /* addr, bswap64 */
			    12, 13, 14, 15,
			     7,  6,  5,  4, /* lkey */
			     0,  1,  2,  3  /* length, bswap32 */);
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint32_t tx_byte = 0;
#endif

	for (pos = 0; pos < n; ++pos, ++dseg) {
		__m128i desc;
		struct rte_mbuf *pkt = pkts[pos];

		addr = rte_pktmbuf_mtod(pkt, uintptr_t);
		desc = _mm_set_epi32(addr >> 32,
				     addr,
				     mlx5_tx_mb2mr(txq, pkt),
				     DATA_LEN(pkt));
		desc = _mm_shuffle_epi8(desc, shuf_mask_dseg);
		_mm_store_si128(dseg, desc);
#ifdef MLX5_PMD_SOFT_COUNTERS
		tx_byte += DATA_LEN(pkt);
#endif
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	txq->stats.obytes += tx_byte;
#endif
}

/**
 * Count the number of continuous single segment packets. The first packet must
 * be a single segment packet.
 *
 * @param pkts
 *   Pointer to array of packets.
 * @param pkts_n
 *   Number of packets.
 *
 * @return
 *   Number of continuous single segment packets.
 */
static inline unsigned int
txq_check_multiseg(struct rte_mbuf **pkts, uint16_t pkts_n)
{
	unsigned int pos;

	if (!pkts_n)
		return 0;
	assert(NB_SEGS(pkts[0]) == 1);
	/* Count the number of continuous single segment packets. */
	for (pos = 1; pos < pkts_n; ++pos)
		if (NB_SEGS(pkts[pos]) > 1)
			break;
	return pos;
}

/**
 * Count the number of packets having same ol_flags and calculate cs_flags.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param pkts
 *   Pointer to array of packets.
 * @param pkts_n
 *   Number of packets.
 * @param cs_flags
 *   Pointer of flags to be returned.
 *
 * @return
 *   Number of packets having same ol_flags.
 */
static inline unsigned int
txq_calc_offload(struct txq *txq, struct rte_mbuf **pkts, uint16_t pkts_n,
		 uint8_t *cs_flags)
{
	unsigned int pos;
	const uint64_t ol_mask =
		PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM |
		PKT_TX_UDP_CKSUM | PKT_TX_TUNNEL_GRE |
		PKT_TX_TUNNEL_VXLAN | PKT_TX_OUTER_IP_CKSUM;

	if (!pkts_n)
		return 0;
	/* Count the number of packets having same ol_flags. */
	for (pos = 1; pos < pkts_n; ++pos)
		if ((pkts[pos]->ol_flags ^ pkts[0]->ol_flags) & ol_mask)
			break;
	/* Should open another MPW session for the rest. */
	if (pkts[0]->ol_flags &
	    (PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM)) {
		const uint64_t is_tunneled =
			pkts[0]->ol_flags &
			(PKT_TX_TUNNEL_GRE |
			 PKT_TX_TUNNEL_VXLAN);

		if (is_tunneled && txq->tunnel_en) {
			*cs_flags = MLX5_ETH_WQE_L3_INNER_CSUM |
				    MLX5_ETH_WQE_L4_INNER_CSUM;
			if (pkts[0]->ol_flags & PKT_TX_OUTER_IP_CKSUM)
				*cs_flags |= MLX5_ETH_WQE_L3_CSUM;
		} else {
			*cs_flags = MLX5_ETH_WQE_L3_CSUM |
				    MLX5_ETH_WQE_L4_CSUM;
		}
	}
	return pos;
}

/**
 * Send multi-segmented packets until it encounters a single segment packet in
 * the pkts list.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param pkts
 *   Pointer to array of packets to be sent.
 * @param pkts_n
 *   Number of packets to be sent.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
static uint16_t
txq_scatter_v(struct txq *txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	uint16_t elts_head = txq->elts_head;
	const uint16_t elts_n = 1 << txq->elts_n;
	const uint16_t elts_m = elts_n - 1;
	const uint16_t wq_n = 1 << txq->wqe_n;
	const uint16_t wq_mask = wq_n - 1;
	const unsigned int nb_dword_per_wqebb =
		MLX5_WQE_SIZE / MLX5_WQE_DWORD_SIZE;
	const unsigned int nb_dword_in_hdr =
		sizeof(struct mlx5_wqe) / MLX5_WQE_DWORD_SIZE;
	unsigned int n;
	volatile struct mlx5_wqe *wqe = NULL;

	assert(elts_n > pkts_n);
	mlx5_tx_complete(txq);
	if (unlikely(!pkts_n))
		return 0;
	for (n = 0; n < pkts_n; ++n) {
		struct rte_mbuf *buf = pkts[n];
		unsigned int segs_n = buf->nb_segs;
		unsigned int ds = nb_dword_in_hdr;
		unsigned int len = PKT_LEN(buf);
		uint16_t wqe_ci = txq->wqe_ci;
		const __m128i shuf_mask_ctrl =
			_mm_set_epi8(15, 14, 13, 12,
				      8,  9, 10, 11, /* bswap32 */
				      4,  5,  6,  7, /* bswap32 */
				      0,  1,  2,  3  /* bswap32 */);
		uint8_t cs_flags = 0;
		uint16_t max_elts;
		uint16_t max_wqe;
		__m128i *t_wqe, *dseg;
		__m128i ctrl;

		assert(segs_n);
		max_elts = elts_n - (elts_head - txq->elts_tail);
		max_wqe = wq_n - (txq->wqe_ci - txq->wqe_pi);
		/*
		 * A MPW session consumes 2 WQEs at most to
		 * include MLX5_MPW_DSEG_MAX pointers.
		 */
		if (segs_n == 1 ||
		    max_elts < segs_n || max_wqe < 2)
			break;
		wqe = &((volatile struct mlx5_wqe64 *)
			 txq->wqes)[wqe_ci & wq_mask].hdr;
		if (buf->ol_flags &
		     (PKT_TX_IP_CKSUM | PKT_TX_TCP_CKSUM | PKT_TX_UDP_CKSUM)) {
			const uint64_t is_tunneled = buf->ol_flags &
						      (PKT_TX_TUNNEL_GRE |
						       PKT_TX_TUNNEL_VXLAN);

			if (is_tunneled && txq->tunnel_en) {
				cs_flags = MLX5_ETH_WQE_L3_INNER_CSUM |
					   MLX5_ETH_WQE_L4_INNER_CSUM;
				if (buf->ol_flags & PKT_TX_OUTER_IP_CKSUM)
					cs_flags |= MLX5_ETH_WQE_L3_CSUM;
			} else {
				cs_flags = MLX5_ETH_WQE_L3_CSUM |
					   MLX5_ETH_WQE_L4_CSUM;
			}
		}
		/* Title WQEBB pointer. */
		t_wqe = (__m128i *)wqe;
		dseg = (__m128i *)(wqe + 1);
		do {
			if (!(ds++ % nb_dword_per_wqebb)) {
				dseg = (__m128i *)
					&((volatile struct mlx5_wqe64 *)
					   txq->wqes)[++wqe_ci & wq_mask];
			}
			txq_wr_dseg_v(txq, dseg++, &buf, 1);
			(*txq->elts)[elts_head++ & elts_m] = buf;
			buf = buf->next;
		} while (--segs_n);
		++wqe_ci;
		/* Fill CTRL in the header. */
		ctrl = _mm_set_epi32(0, 0, txq->qp_num_8s | ds,
				     MLX5_OPC_MOD_MPW << 24 |
				     txq->wqe_ci << 8 | MLX5_OPCODE_TSO);
		ctrl = _mm_shuffle_epi8(ctrl, shuf_mask_ctrl);
		_mm_store_si128(t_wqe, ctrl);
		/* Fill ESEG in the header. */
		_mm_store_si128(t_wqe + 1,
				_mm_set_epi16(0, 0, 0, 0,
					      htons(len), cs_flags,
					      0, 0));
		txq->wqe_ci = wqe_ci;
	}
	if (!n)
		return 0;
	txq->elts_comp += (uint16_t)(elts_head - txq->elts_head);
	txq->elts_head = elts_head;
	if (txq->elts_comp >= MLX5_TX_COMP_THRESH) {
		wqe->ctrl[2] = htonl(8);
		wqe->ctrl[3] = txq->elts_head;
		txq->elts_comp = 0;
		++txq->cq_pi;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	txq->stats.opackets += n;
#endif
	mlx5_tx_dbrec(txq, wqe);
	return n;
}

/**
 * Send burst of packets with Enhanced MPW. If it encounters a multi-seg packet,
 * it returns to make it processed by txq_scatter_v(). All the packets in
 * the pkts list should be single segment packets having same offload flags.
 * This must be checked by txq_check_multiseg() and txq_calc_offload().
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param pkts
 *   Pointer to array of packets to be sent.
 * @param pkts_n
 *   Number of packets to be sent (<= MLX5_VPMD_TX_MAX_BURST).
 * @param cs_flags
 *   Checksum offload flags to be written in the descriptor.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
static inline uint16_t
txq_burst_v(struct txq *txq, struct rte_mbuf **pkts, uint16_t pkts_n,
	    uint8_t cs_flags)
{
	struct rte_mbuf **elts;
	uint16_t elts_head = txq->elts_head;
	const uint16_t elts_n = 1 << txq->elts_n;
	const uint16_t elts_m = elts_n - 1;
	const unsigned int nb_dword_per_wqebb =
		MLX5_WQE_SIZE / MLX5_WQE_DWORD_SIZE;
	const unsigned int nb_dword_in_hdr =
		sizeof(struct mlx5_wqe) / MLX5_WQE_DWORD_SIZE;
	unsigned int n = 0;
	unsigned int pos;
	uint16_t max_elts;
	uint16_t max_wqe;
	uint32_t comp_req = 0;
	const uint16_t wq_n = 1 << txq->wqe_n;
	const uint16_t wq_mask = wq_n - 1;
	uint16_t wq_idx = txq->wqe_ci & wq_mask;
	volatile struct mlx5_wqe64 *wq =
		&((volatile struct mlx5_wqe64 *)txq->wqes)[wq_idx];
	volatile struct mlx5_wqe *wqe = (volatile struct mlx5_wqe *)wq;
	const __m128i shuf_mask_ctrl =
		_mm_set_epi8(15, 14, 13, 12,
			      8,  9, 10, 11, /* bswap32 */
			      4,  5,  6,  7, /* bswap32 */
			      0,  1,  2,  3  /* bswap32 */);
	__m128i *t_wqe, *dseg;
	__m128i ctrl;

	/* Make sure all packets can fit into a single WQE. */
	assert(elts_n > pkts_n);
	mlx5_tx_complete(txq);
	max_elts = (elts_n - (elts_head - txq->elts_tail));
	max_wqe = (1u << txq->wqe_n) - (txq->wqe_ci - txq->wqe_pi);
	pkts_n = RTE_MIN((unsigned int)RTE_MIN(pkts_n, max_wqe), max_elts);
	if (unlikely(!pkts_n))
		return 0;
	elts = &(*txq->elts)[elts_head & elts_m];
	/* Loop for available tailroom first. */
	n = RTE_MIN(elts_n - (elts_head & elts_m), pkts_n);
	for (pos = 0; pos < (n & -2); pos += 2)
		_mm_storeu_si128((__m128i *)&elts[pos],
				 _mm_loadu_si128((__m128i *)&pkts[pos]));
	if (n & 1)
		elts[pos] = pkts[pos];
	/* Check if it crosses the end of the queue. */
	if (unlikely(n < pkts_n)) {
		elts = &(*txq->elts)[0];
		for (pos = 0; pos < pkts_n - n; ++pos)
			elts[pos] = pkts[n + pos];
	}
	txq->elts_head += pkts_n;
	/* Save title WQEBB pointer. */
	t_wqe = (__m128i *)wqe;
	dseg = (__m128i *)(wqe + 1);
	/* Calculate the number of entries to the end. */
	n = RTE_MIN(
		(wq_n - wq_idx) * nb_dword_per_wqebb - nb_dword_in_hdr,
		pkts_n);
	/* Fill DSEGs. */
	txq_wr_dseg_v(txq, dseg, pkts, n);
	/* Check if it crosses the end of the queue. */
	if (n < pkts_n) {
		dseg = (__m128i *)txq->wqes;
		txq_wr_dseg_v(txq, dseg, &pkts[n], pkts_n - n);
	}
	if (txq->elts_comp + pkts_n < MLX5_TX_COMP_THRESH) {
		txq->elts_comp += pkts_n;
	} else {
		/* Request a completion. */
		txq->elts_comp = 0;
		++txq->cq_pi;
		comp_req = 8;
	}
	/* Fill CTRL in the header. */
	ctrl = _mm_set_epi32(txq->elts_head, comp_req,
			     txq->qp_num_8s | (pkts_n + 2),
			     MLX5_OPC_MOD_ENHANCED_MPSW << 24 |
				txq->wqe_ci << 8 | MLX5_OPCODE_ENHANCED_MPSW);
	ctrl = _mm_shuffle_epi8(ctrl, shuf_mask_ctrl);
	_mm_store_si128(t_wqe, ctrl);
	/* Fill ESEG in the header. */
	_mm_store_si128(t_wqe + 1,
			_mm_set_epi8(0, 0, 0, 0,
				     0, 0, 0, 0,
				     0, 0, 0, cs_flags,
				     0, 0, 0, 0));
#ifdef MLX5_PMD_SOFT_COUNTERS
	txq->stats.opackets += pkts_n;
#endif
	txq->wqe_ci += (nb_dword_in_hdr + pkts_n + (nb_dword_per_wqebb - 1)) /
		       nb_dword_per_wqebb;
	/* Ring QP doorbell. */
	mlx5_tx_dbrec(txq, wqe);
	return pkts_n;
}

/**
 * DPDK callback for vectorized TX.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx5_tx_burst_raw_vec(void *dpdk_txq, struct rte_mbuf **pkts,
		      uint16_t pkts_n)
{
	struct txq *txq = (struct txq *)dpdk_txq;
	uint16_t nb_tx = 0;

	while (pkts_n > nb_tx) {
		uint16_t n;
		uint16_t ret;

		n = RTE_MIN((uint16_t)(pkts_n - nb_tx), MLX5_VPMD_TX_MAX_BURST);
		ret = txq_burst_v(txq, &pkts[nb_tx], n, 0);
		nb_tx += ret;
		if (!ret)
			break;
	}
	return nb_tx;
}

/**
 * DPDK callback for vectorized TX with multi-seg packets and offload.
 *
 * @param dpdk_txq
 *   Generic pointer to TX queue structure.
 * @param[in] pkts
 *   Packets to transmit.
 * @param pkts_n
 *   Number of packets in array.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
uint16_t
mlx5_tx_burst_vec(void *dpdk_txq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct txq *txq = (struct txq *)dpdk_txq;
	uint16_t nb_tx = 0;

	while (pkts_n > nb_tx) {
		uint8_t cs_flags = 0;
		uint16_t n;
		uint16_t ret;

		/* Transmit multi-seg packets in the head of pkts list. */
		if (!(txq->flags & ETH_TXQ_FLAGS_NOMULTSEGS) &&
		    NB_SEGS(pkts[nb_tx]) > 1)
			nb_tx += txq_scatter_v(txq,
					       &pkts[nb_tx],
					       pkts_n - nb_tx);
		n = RTE_MIN((uint16_t)(pkts_n - nb_tx), MLX5_VPMD_TX_MAX_BURST);
		if (!(txq->flags & ETH_TXQ_FLAGS_NOMULTSEGS))
			n = txq_check_multiseg(&pkts[nb_tx], n);
		if (!(txq->flags & ETH_TXQ_FLAGS_NOOFFLOADS))
			n = txq_calc_offload(txq, &pkts[nb_tx], n, &cs_flags);
		ret = txq_burst_v(txq, &pkts[nb_tx], n, cs_flags);
		nb_tx += ret;
		if (!ret)
			break;
	}
	return nb_tx;
}

/**
 * Store free buffers to RX SW ring.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param pkts
 *   Pointer to array of packets to be stored.
 * @param pkts_n
 *   Number of packets to be stored.
 */
static inline void
rxq_copy_mbuf_v(struct rxq *rxq, struct rte_mbuf **pkts, uint16_t n)
{
	const uint16_t q_mask = (1 << rxq->elts_n) - 1;
	struct rte_mbuf **elts = &(*rxq->elts)[rxq->rq_pi & q_mask];
	unsigned int pos;
	uint16_t p = n & -2;

	for (pos = 0; pos < p; pos += 2) {
		__m128i mbp;

		mbp = _mm_loadu_si128((__m128i *)&elts[pos]);
		_mm_storeu_si128((__m128i *)&pkts[pos], mbp);
	}
	if (n & 1)
		pkts[pos] = elts[pos];
}

/**
 * Replenish buffers for RX in bulk.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param n
 *   Number of buffers to be replenished.
 */
static inline void
rxq_replenish_bulk_mbuf(struct rxq *rxq, uint16_t n)
{
	const uint16_t q_n = 1 << rxq->elts_n;
	const uint16_t q_mask = q_n - 1;
	const uint16_t elts_idx = rxq->rq_ci & q_mask;
	struct rte_mbuf **elts = &(*rxq->elts)[elts_idx];
	volatile struct mlx5_wqe_data_seg *wq = &(*rxq->wqes)[elts_idx];
	unsigned int i;

	assert(n >= MLX5_VPMD_RXQ_RPLNSH_THRESH);
	assert(n <= (uint16_t)(q_n - (rxq->rq_ci - rxq->rq_pi)));
	assert(MLX5_VPMD_RXQ_RPLNSH_THRESH > MLX5_VPMD_DESCS_PER_LOOP);
	/* Not to cross queue end. */
	n = RTE_MIN(n - MLX5_VPMD_DESCS_PER_LOOP, q_n - elts_idx);
	if (rte_mempool_get_bulk(rxq->mp, (void *)elts, n) < 0) {
		rxq->stats.rx_nombuf += n;
		return;
	}
	for (i = 0; i < n; ++i)
		wq[i].addr = htonll((uintptr_t)elts[i]->buf_addr +
				    RTE_PKTMBUF_HEADROOM);
	rxq->rq_ci += n;
	rte_wmb();
	*rxq->rq_db = htonl(rxq->rq_ci);
}

/**
 * Decompress a compressed completion and fill in mbufs in RX SW ring with data
 * extracted from the title completion descriptor.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param cq
 *   Pointer to completion array having a compressed completion at first.
 * @param elts
 *   Pointer to SW ring to be filled. The first mbuf has to be pre-built from
 *   the title completion descriptor to be copied to the rest of mbufs.
 */
static inline void
rxq_cq_decompress_v(struct rxq *rxq,
		    volatile struct mlx5_cqe *cq,
		    struct rte_mbuf **elts)
{
	volatile struct mlx5_mini_cqe8 *mcq = (void *)(cq + 1);
	struct rte_mbuf *t_pkt = elts[0]; /* Title packet is pre-built. */
	unsigned int pos;
	unsigned int i;
	unsigned int inv = 0;
	/* Mask to shuffle from extracted mini CQE to mbuf. */
	const __m128i shuf_mask1 =
		_mm_set_epi8(0,  1,  2,  3, /* rss, bswap32 */
			    -1, -1,         /* skip vlan_tci */
			     6,  7,         /* data_len, bswap16 */
			    -1, -1,  6,  7, /* pkt_len, bswap16 */
			    -1, -1, -1, -1  /* skip packet_type */);
	const __m128i shuf_mask2 =
		_mm_set_epi8(8,  9, 10, 11, /* rss, bswap32 */
			    -1, -1,         /* skip vlan_tci */
			    14, 15,         /* data_len, bswap16 */
			    -1, -1, 14, 15, /* pkt_len, bswap16 */
			    -1, -1, -1, -1  /* skip packet_type */);
	/* Restore the compressed count. Must be 16 bits. */
	const uint16_t mcqe_n = t_pkt->data_len +
				(rxq->crc_present * ETHER_CRC_LEN);
	const __m128i rearm =
		_mm_loadu_si128((__m128i *)&t_pkt->rearm_data);
	const __m128i rxdf =
		_mm_loadu_si128((__m128i *)&t_pkt->rx_descriptor_fields1);
	const __m128i crc_adj =
		_mm_set_epi16(0, 0, 0,
			      rxq->crc_present * ETHER_CRC_LEN,
			      0,
			      rxq->crc_present * ETHER_CRC_LEN,
			      0, 0);
	const uint32_t flow_tag = t_pkt->hash.fdir.hi;
#ifdef MLX5_PMD_SOFT_COUNTERS
	const __m128i zero = _mm_setzero_si128();
	const __m128i ones = _mm_cmpeq_epi32(zero, zero);
	uint32_t rcvd_byte = 0;
	/* Mask to shuffle byte_cnt to add up stats. Do bswap16 for all. */
	const __m128i len_shuf_mask =
		_mm_set_epi8(-1, -1, -1, -1,
			     -1, -1, -1, -1,
			     14, 15,  6,  7,
			     10, 11,  2,  3);
#endif

	/* Compile time sanity check for this function. */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, hash) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 12);
	/*
	 * A. load mCQEs into a 128bit register.
	 * B. store rearm data to mbuf.
	 * C. combine data from mCQEs with rx_descriptor_fields1.
	 * D. store rx_descriptor_fields1.
	 * E. store flow tag (rte_flow mark).
	 */
	for (pos = 0; pos < mcqe_n; ) {
		__m128i mcqe1, mcqe2;
		__m128i rxdf1, rxdf2;
#ifdef MLX5_PMD_SOFT_COUNTERS
		__m128i byte_cnt, invalid_mask;
#endif

		if (!(pos & 0x7) && pos + 8 < mcqe_n)
			rte_prefetch0((void *)(cq + pos + 8));
		/* A.1 load mCQEs into a 128bit register. */
		mcqe1 = _mm_loadu_si128((__m128i *)&mcq[pos % 8]);
		mcqe2 = _mm_loadu_si128((__m128i *)&mcq[pos % 8 + 2]);
		/* B.1 store rearm data to mbuf. */
		_mm_storeu_si128((__m128i *)&elts[pos]->rearm_data, rearm);
		_mm_storeu_si128((__m128i *)&elts[pos + 1]->rearm_data, rearm);
		/* C.1 combine data from mCQEs with rx_descriptor_fields1. */
		rxdf1 = _mm_shuffle_epi8(mcqe1, shuf_mask1);
		rxdf2 = _mm_shuffle_epi8(mcqe1, shuf_mask2);
		rxdf1 = _mm_sub_epi16(rxdf1, crc_adj);
		rxdf2 = _mm_sub_epi16(rxdf2, crc_adj);
		rxdf1 = _mm_blend_epi16(rxdf1, rxdf, 0x23);
		rxdf2 = _mm_blend_epi16(rxdf2, rxdf, 0x23);
		/* D.1 store rx_descriptor_fields1. */
		_mm_storeu_si128((__m128i *)
				  &elts[pos]->rx_descriptor_fields1,
				 rxdf1);
		_mm_storeu_si128((__m128i *)
				  &elts[pos + 1]->rx_descriptor_fields1,
				 rxdf2);
		/* B.1 store rearm data to mbuf. */
		_mm_storeu_si128((__m128i *)&elts[pos + 2]->rearm_data, rearm);
		_mm_storeu_si128((__m128i *)&elts[pos + 3]->rearm_data, rearm);
		/* C.1 combine data from mCQEs with rx_descriptor_fields1. */
		rxdf1 = _mm_shuffle_epi8(mcqe2, shuf_mask1);
		rxdf2 = _mm_shuffle_epi8(mcqe2, shuf_mask2);
		rxdf1 = _mm_sub_epi16(rxdf1, crc_adj);
		rxdf2 = _mm_sub_epi16(rxdf2, crc_adj);
		rxdf1 = _mm_blend_epi16(rxdf1, rxdf, 0x23);
		rxdf2 = _mm_blend_epi16(rxdf2, rxdf, 0x23);
		/* D.1 store rx_descriptor_fields1. */
		_mm_storeu_si128((__m128i *)
				  &elts[pos + 2]->rx_descriptor_fields1,
				 rxdf1);
		_mm_storeu_si128((__m128i *)
				  &elts[pos + 3]->rx_descriptor_fields1,
				 rxdf2);
#ifdef MLX5_PMD_SOFT_COUNTERS
		invalid_mask = _mm_set_epi64x(0,
					      (mcqe_n - pos) *
					      sizeof(uint16_t) * 8);
		invalid_mask = _mm_sll_epi64(ones, invalid_mask);
		mcqe1 = _mm_srli_si128(mcqe1, 4);
		byte_cnt = _mm_blend_epi16(mcqe1, mcqe2, 0xcc);
		byte_cnt = _mm_shuffle_epi8(byte_cnt, len_shuf_mask);
		byte_cnt = _mm_andnot_si128(invalid_mask, byte_cnt);
		byte_cnt = _mm_hadd_epi16(byte_cnt, zero);
		rcvd_byte += _mm_cvtsi128_si64(_mm_hadd_epi16(byte_cnt, zero));
#endif
		if (rxq->mark) {
			/* E.1 store flow tag (rte_flow mark). */
			elts[pos]->hash.fdir.hi = flow_tag;
			elts[pos + 1]->hash.fdir.hi = flow_tag;
			elts[pos + 2]->hash.fdir.hi = flow_tag;
			elts[pos + 3]->hash.fdir.hi = flow_tag;
		}
		pos += MLX5_VPMD_DESCS_PER_LOOP;
		/* Move to next CQE and invalidate consumed CQEs. */
		if (!(pos & 0x7) && pos < mcqe_n) {
			mcq = (void *)(cq + pos);
			for (i = 0; i < 8; ++i)
				cq[inv++].op_own = MLX5_CQE_INVALIDATE;
		}
	}
	/* Invalidate the rest of CQEs. */
	for (; inv < mcqe_n; ++inv)
		cq[inv].op_own = MLX5_CQE_INVALIDATE;
#ifdef MLX5_PMD_SOFT_COUNTERS
	rxq->stats.ipackets += mcqe_n;
	rxq->stats.ibytes += rcvd_byte;
#endif
	rxq->cq_ci += mcqe_n;
}

/**
 * Calculate packet type and offload flag for mbuf and store it.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param cqes[4]
 *   Array of four 16bytes completions extracted from the original completion
 *   descriptor.
 * @param op_err
 *   Opcode vector having responder error status. Each field is 4B.
 * @param pkts
 *   Pointer to array of packets to be filled.
 */
static inline void
rxq_cq_to_ptype_oflags_v(struct rxq *rxq, __m128i cqes[4], __m128i op_err,
			 struct rte_mbuf **pkts)
{
	__m128i pinfo0, pinfo1;
	__m128i pinfo, ptype;
	__m128i ol_flags = _mm_set1_epi32(rxq->rss_hash * PKT_RX_RSS_HASH);
	__m128i cv_flags;
	const __m128i zero = _mm_setzero_si128();
	const __m128i ptype_mask =
		_mm_set_epi32(0xfd06, 0xfd06, 0xfd06, 0xfd06);
	const __m128i ptype_ol_mask =
		_mm_set_epi32(0x106, 0x106, 0x106, 0x106);
	const __m128i pinfo_mask =
		_mm_set_epi32(0x3, 0x3, 0x3, 0x3);
	const __m128i cv_flag_sel =
		_mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0,
			     (uint8_t)((PKT_RX_IP_CKSUM_GOOD |
					PKT_RX_L4_CKSUM_GOOD) >> 1),
			     0,
			     (uint8_t)(PKT_RX_L4_CKSUM_GOOD >> 1),
			     0,
			     (uint8_t)(PKT_RX_IP_CKSUM_GOOD >> 1),
			     (uint8_t)(PKT_RX_VLAN_PKT | PKT_RX_VLAN_STRIPPED),
			     0);
	const __m128i cv_mask =
		_mm_set_epi32(PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD |
			      PKT_RX_VLAN_PKT | PKT_RX_VLAN_STRIPPED,
			      PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD |
			      PKT_RX_VLAN_PKT | PKT_RX_VLAN_STRIPPED,
			      PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD |
			      PKT_RX_VLAN_PKT | PKT_RX_VLAN_STRIPPED,
			      PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD |
			      PKT_RX_VLAN_PKT | PKT_RX_VLAN_STRIPPED);
	const __m128i mbuf_init =
		_mm_loadl_epi64((__m128i *)&rxq->mbuf_initializer);
	__m128i rearm0, rearm1, rearm2, rearm3;

	/* Extract pkt_info field. */
	pinfo0 = _mm_unpacklo_epi32(cqes[0], cqes[1]);
	pinfo1 = _mm_unpacklo_epi32(cqes[2], cqes[3]);
	pinfo = _mm_unpacklo_epi64(pinfo0, pinfo1);
	/* Extract hdr_type_etc field. */
	pinfo0 = _mm_unpackhi_epi32(cqes[0], cqes[1]);
	pinfo1 = _mm_unpackhi_epi32(cqes[2], cqes[3]);
	ptype = _mm_unpacklo_epi64(pinfo0, pinfo1);
	if (rxq->mark) {
		const __m128i pinfo_ft_mask =
			_mm_set_epi32(0xffffff00, 0xffffff00,
				      0xffffff00, 0xffffff00);
		const __m128i fdir_flags = _mm_set1_epi32(PKT_RX_FDIR);
		const __m128i fdir_id_flags = _mm_set1_epi32(PKT_RX_FDIR_ID);
		__m128i flow_tag, invalid_mask;

		flow_tag = _mm_and_si128(pinfo, pinfo_ft_mask);
		/* Check if flow tag is non-zero then set PKT_RX_FDIR. */
		invalid_mask = _mm_cmpeq_epi32(flow_tag, zero);
		ol_flags = _mm_or_si128(ol_flags,
					_mm_andnot_si128(invalid_mask,
							 fdir_flags));
		/* Mask out invalid entries. */
		flow_tag = _mm_andnot_si128(invalid_mask, flow_tag);
		/* Check if flow tag MLX5_FLOW_MARK_DEFAULT. */
		ol_flags = _mm_or_si128(ol_flags,
					_mm_andnot_si128(
						_mm_cmpeq_epi32(flow_tag,
								pinfo_ft_mask),
						fdir_id_flags));
	}
	/*
	 * Merge the two fields to generate the following:
	 * bit[1]     = l3_ok
	 * bit[2]     = l4_ok
	 * bit[8]     = cv
	 * bit[11:10] = l3_hdr_type
	 * bit[14:12] = l4_hdr_type
	 * bit[15]    = ip_frag
	 * bit[16]    = tunneled
	 * bit[17]    = outer_l3_type
	 */
	ptype = _mm_and_si128(ptype, ptype_mask);
	pinfo = _mm_and_si128(pinfo, pinfo_mask);
	pinfo = _mm_slli_epi32(pinfo, 16);
	/* Make pinfo has merged fields for ol_flags calculation. */
	pinfo = _mm_or_si128(ptype, pinfo);
	ptype = _mm_srli_epi32(pinfo, 10);
	ptype = _mm_packs_epi32(ptype, zero);
	/* Errored packets will have RTE_PTYPE_ALL_MASK. */
	op_err = _mm_srli_epi16(op_err, 8);
	ptype = _mm_or_si128(ptype, op_err);
	pkts[0]->packet_type = mlx5_ptype_table[_mm_extract_epi8(ptype, 0)];
	pkts[1]->packet_type = mlx5_ptype_table[_mm_extract_epi8(ptype, 2)];
	pkts[2]->packet_type = mlx5_ptype_table[_mm_extract_epi8(ptype, 4)];
	pkts[3]->packet_type = mlx5_ptype_table[_mm_extract_epi8(ptype, 6)];
	/* Fill flags for checksum and VLAN. */
	pinfo = _mm_and_si128(pinfo, ptype_ol_mask);
	pinfo = _mm_shuffle_epi8(cv_flag_sel, pinfo);
	/* Locate checksum flags at byte[2:1] and merge with VLAN flags. */
	cv_flags = _mm_slli_epi32(pinfo, 9);
	cv_flags = _mm_or_si128(pinfo, cv_flags);
	/* Move back flags to start from byte[0]. */
	cv_flags = _mm_srli_epi32(cv_flags, 8);
	/* Mask out garbage bits. */
	cv_flags = _mm_and_si128(cv_flags, cv_mask);
	/* Merge to ol_flags. */
	ol_flags = _mm_or_si128(ol_flags, cv_flags);
	/* Merge mbuf_init and ol_flags. */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, ol_flags) !=
			 offsetof(struct rte_mbuf, rearm_data) + 8);
	rearm0 = _mm_blend_epi16(mbuf_init, _mm_slli_si128(ol_flags, 8), 0x30);
	rearm1 = _mm_blend_epi16(mbuf_init, _mm_slli_si128(ol_flags, 4), 0x30);
	rearm2 = _mm_blend_epi16(mbuf_init, ol_flags, 0x30);
	rearm3 = _mm_blend_epi16(mbuf_init, _mm_srli_si128(ol_flags, 4), 0x30);
	/* Write 8B rearm_data and 8B ol_flags. */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, rearm_data) !=
			 RTE_ALIGN(offsetof(struct rte_mbuf, rearm_data), 16));
	_mm_store_si128((__m128i *)&pkts[0]->rearm_data, rearm0);
	_mm_store_si128((__m128i *)&pkts[1]->rearm_data, rearm1);
	_mm_store_si128((__m128i *)&pkts[2]->rearm_data, rearm2);
	_mm_store_si128((__m128i *)&pkts[3]->rearm_data, rearm3);
}

/**
 * Skip error packets.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
static uint16_t
rxq_handle_pending_error(struct rxq *rxq, struct rte_mbuf **pkts,
			 uint16_t pkts_n)
{
	uint16_t n = 0;
	unsigned int i;
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint32_t err_bytes = 0;
#endif

	for (i = 0; i < pkts_n; ++i) {
		struct rte_mbuf *pkt = pkts[i];

		if (pkt->packet_type == RTE_PTYPE_ALL_MASK) {
#ifdef MLX5_PMD_SOFT_COUNTERS
			err_bytes += PKT_LEN(pkt);
#endif
			rte_pktmbuf_free_seg(pkt);
		} else {
			pkts[n++] = pkt;
		}
	}
	rxq->stats.idropped += (pkts_n - n);
#ifdef MLX5_PMD_SOFT_COUNTERS
	/* Correct counters of errored completions. */
	rxq->stats.ipackets -= (pkts_n - n);
	rxq->stats.ibytes -= err_bytes;
#endif
	rxq->pending_err = 0;
	return n;
}

/**
 * Receive burst of packets. An errored completion also consumes a mbuf, but the
 * packet_type is set to be RTE_PTYPE_ALL_MASK. Marked mbufs should be freed
 * before returning to application.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets received including errors (<= pkts_n).
 */
static inline uint16_t
rxq_burst_v(struct rxq *rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	const uint16_t q_n = 1 << rxq->cqe_n;
	const uint16_t q_mask = q_n - 1;
	volatile struct mlx5_cqe *cq;
	struct rte_mbuf **elts;
	unsigned int pos;
	uint64_t n;
	uint16_t repl_n;
	uint64_t comp_idx = MLX5_VPMD_DESCS_PER_LOOP;
	uint16_t nocmp_n = 0;
	uint16_t rcvd_pkt = 0;
	unsigned int cq_idx = rxq->cq_ci & q_mask;
	unsigned int elts_idx;
	unsigned int ownership = !!(rxq->cq_ci & (q_mask + 1));
	const __m128i owner_check =
		_mm_set_epi64x(0x0100000001000000LL, 0x0100000001000000LL);
	const __m128i opcode_check =
		_mm_set_epi64x(0xf0000000f0000000LL, 0xf0000000f0000000LL);
	const __m128i format_check =
		_mm_set_epi64x(0x0c0000000c000000LL, 0x0c0000000c000000LL);
	const __m128i resp_err_check =
		_mm_set_epi64x(0xe0000000e0000000LL, 0xe0000000e0000000LL);
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint32_t rcvd_byte = 0;
	/* Mask to shuffle byte_cnt to add up stats. Do bswap16 for all. */
	const __m128i len_shuf_mask =
		_mm_set_epi8(-1, -1, -1, -1,
			     -1, -1, -1, -1,
			     12, 13,  8,  9,
			      4,  5,  0,  1);
#endif
	/* Mask to shuffle from extracted CQE to mbuf. */
	const __m128i shuf_mask =
		_mm_set_epi8(-1,  3,  2,  1, /* fdir.hi */
			     12, 13, 14, 15, /* rss, bswap32 */
			     10, 11,         /* vlan_tci, bswap16 */
			      4,  5,         /* data_len, bswap16 */
			     -1, -1,         /* zero out 2nd half of pkt_len */
			      4,  5          /* pkt_len, bswap16 */);
	/* Mask to blend from the last Qword to the first DQword. */
	const __m128i blend_mask =
		_mm_set_epi8(-1, -1, -1, -1,
			     -1, -1, -1, -1,
			      0,  0,  0,  0,
			      0,  0,  0, -1);
	const __m128i zero = _mm_setzero_si128();
	const __m128i ones = _mm_cmpeq_epi32(zero, zero);
	const __m128i crc_adj =
		_mm_set_epi16(0, 0, 0, 0, 0,
			      rxq->crc_present * ETHER_CRC_LEN,
			      0,
			      rxq->crc_present * ETHER_CRC_LEN);
	const __m128i flow_mark_adj = _mm_set_epi32(rxq->mark * (-1), 0, 0, 0);

	/* Compile time sanity check for this function. */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			 offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
	RTE_BUILD_BUG_ON(offsetof(struct mlx5_cqe, pkt_info) != 0);
	RTE_BUILD_BUG_ON(offsetof(struct mlx5_cqe, rx_hash_res) !=
			 offsetof(struct mlx5_cqe, pkt_info) + 12);
	RTE_BUILD_BUG_ON(offsetof(struct mlx5_cqe, rsvd1) +
			  sizeof(((struct mlx5_cqe *)0)->rsvd1) !=
			 offsetof(struct mlx5_cqe, hdr_type_etc));
	RTE_BUILD_BUG_ON(offsetof(struct mlx5_cqe, vlan_info) !=
			 offsetof(struct mlx5_cqe, hdr_type_etc) + 2);
	RTE_BUILD_BUG_ON(offsetof(struct mlx5_cqe, rsvd2) +
			  sizeof(((struct mlx5_cqe *)0)->rsvd2) !=
			 offsetof(struct mlx5_cqe, byte_cnt));
	RTE_BUILD_BUG_ON(offsetof(struct mlx5_cqe, sop_drop_qpn) !=
			 RTE_ALIGN(offsetof(struct mlx5_cqe, sop_drop_qpn), 8));
	RTE_BUILD_BUG_ON(offsetof(struct mlx5_cqe, op_own) !=
			 offsetof(struct mlx5_cqe, sop_drop_qpn) + 7);
	assert(rxq->sges_n == 0);
	assert(rxq->cqe_n == rxq->elts_n);
	cq = &(*rxq->cqes)[cq_idx];
	rte_prefetch0(cq);
	rte_prefetch0(cq + 1);
	rte_prefetch0(cq + 2);
	rte_prefetch0(cq + 3);
	pkts_n = RTE_MIN(pkts_n, MLX5_VPMD_RX_MAX_BURST);
	/*
	 * Order of indexes:
	 *   rq_ci >= cq_ci >= rq_pi
	 * Definition of indexes:
	 *   rq_ci - cq_ci := # of buffers owned by HW (posted).
	 *   cq_ci - rq_pi := # of buffers not returned to app (decompressed).
	 *   N - (rq_ci - rq_pi) := # of buffers consumed (to be replenished).
	 */
	repl_n = q_n - (rxq->rq_ci - rxq->rq_pi);
	if (repl_n >= MLX5_VPMD_RXQ_RPLNSH_THRESH)
		rxq_replenish_bulk_mbuf(rxq, repl_n);
	/* See if there're unreturned mbufs from compressed CQE. */
	rcvd_pkt = rxq->cq_ci - rxq->rq_pi;
	if (rcvd_pkt > 0) {
		rcvd_pkt = RTE_MIN(rcvd_pkt, pkts_n);
		rxq_copy_mbuf_v(rxq, pkts, rcvd_pkt);
		rxq->rq_pi += rcvd_pkt;
		pkts += rcvd_pkt;
	}
	elts_idx = rxq->rq_pi & q_mask;
	elts = &(*rxq->elts)[elts_idx];
	/* Not to overflow pkts array. */
	pkts_n = RTE_ALIGN_FLOOR(pkts_n - rcvd_pkt, MLX5_VPMD_DESCS_PER_LOOP);
	/* Not to cross queue end. */
	pkts_n = RTE_MIN(pkts_n, q_n - elts_idx);
	if (!pkts_n)
		return rcvd_pkt;
	/* At this point, there shouldn't be any remained packets. */
	assert(rxq->rq_pi == rxq->cq_ci);
	/*
	 * A. load first Qword (8bytes) in one loop.
	 * B. copy 4 mbuf pointers from elts ring to returing pkts.
	 * C. load remained CQE data and extract necessary fields.
	 *    Final 16bytes cqes[] extracted from original 64bytes CQE has the
	 *    following structure:
	 *        struct {
	 *          uint8_t  pkt_info;
	 *          uint8_t  flow_tag[3];
	 *          uint16_t byte_cnt;
	 *          uint8_t  rsvd4;
	 *          uint8_t  op_own;
	 *          uint16_t hdr_type_etc;
	 *          uint16_t vlan_info;
	 *          uint32_t rx_has_res;
	 *        } c;
	 * D. fill in mbuf.
	 * E. get valid CQEs.
	 * F. find compressed CQE.
	 */
	for (pos = 0;
	     pos < pkts_n;
	     pos += MLX5_VPMD_DESCS_PER_LOOP) {
		__m128i cqes[MLX5_VPMD_DESCS_PER_LOOP];
		__m128i cqe_tmp1, cqe_tmp2;
		__m128i pkt_mb0, pkt_mb1, pkt_mb2, pkt_mb3;
		__m128i op_own, op_own_tmp1, op_own_tmp2;
		__m128i opcode, owner_mask, invalid_mask;
		__m128i comp_mask;
		__m128i mask;
#ifdef MLX5_PMD_SOFT_COUNTERS
		__m128i byte_cnt;
#endif
		__m128i mbp1, mbp2;
		__m128i p = _mm_set_epi16(0, 0, 0, 0, 3, 2, 1, 0);
		unsigned int p1, p2, p3;

		/* Prefetch next 4 CQEs. */
		if (pkts_n - pos >= 2 * MLX5_VPMD_DESCS_PER_LOOP) {
			rte_prefetch0(&cq[pos + MLX5_VPMD_DESCS_PER_LOOP]);
			rte_prefetch0(&cq[pos + MLX5_VPMD_DESCS_PER_LOOP + 1]);
			rte_prefetch0(&cq[pos + MLX5_VPMD_DESCS_PER_LOOP + 2]);
			rte_prefetch0(&cq[pos + MLX5_VPMD_DESCS_PER_LOOP + 3]);
		}
		/* A.0 do not cross the end of CQ. */
		mask = _mm_set_epi64x(0, (pkts_n - pos) * sizeof(uint16_t) * 8);
		mask = _mm_sll_epi64(ones, mask);
		p = _mm_andnot_si128(mask, p);
		/* A.1 load cqes. */
		p3 = _mm_extract_epi16(p, 3);
		cqes[3] = _mm_loadl_epi64((__m128i *)
					   &cq[pos + p3].sop_drop_qpn);
		rte_compiler_barrier();
		p2 = _mm_extract_epi16(p, 2);
		cqes[2] = _mm_loadl_epi64((__m128i *)
					   &cq[pos + p2].sop_drop_qpn);
		rte_compiler_barrier();
		/* B.1 load mbuf pointers. */
		mbp1 = _mm_loadu_si128((__m128i *)&elts[pos]);
		mbp2 = _mm_loadu_si128((__m128i *)&elts[pos + 2]);
		/* A.1 load a block having op_own. */
		p1 = _mm_extract_epi16(p, 1);
		cqes[1] = _mm_loadl_epi64((__m128i *)
					   &cq[pos + p1].sop_drop_qpn);
		rte_compiler_barrier();
		cqes[0] = _mm_loadl_epi64((__m128i *)
					   &cq[pos].sop_drop_qpn);
		/* B.2 copy mbuf pointers. */
		_mm_storeu_si128((__m128i *)&pkts[pos], mbp1);
		_mm_storeu_si128((__m128i *)&pkts[pos + 2], mbp2);
		rte_compiler_barrier();
		/* C.1 load remained CQE data and extract necessary fields. */
		cqe_tmp2 = _mm_load_si128((__m128i *)&cq[pos + p3]);
		cqe_tmp1 = _mm_load_si128((__m128i *)&cq[pos + p2]);
		cqes[3] = _mm_blendv_epi8(cqes[3], cqe_tmp2, blend_mask);
		cqes[2] = _mm_blendv_epi8(cqes[2], cqe_tmp1, blend_mask);
		cqe_tmp2 = _mm_loadu_si128((__m128i *)&cq[pos + p3].rsvd1[3]);
		cqe_tmp1 = _mm_loadu_si128((__m128i *)&cq[pos + p2].rsvd1[3]);
		cqes[3] = _mm_blend_epi16(cqes[3], cqe_tmp2, 0x30);
		cqes[2] = _mm_blend_epi16(cqes[2], cqe_tmp1, 0x30);
		cqe_tmp2 = _mm_loadl_epi64((__m128i *)&cq[pos + p3].rsvd2[10]);
		cqe_tmp1 = _mm_loadl_epi64((__m128i *)&cq[pos + p2].rsvd2[10]);
		cqes[3] = _mm_blend_epi16(cqes[3], cqe_tmp2, 0x04);
		cqes[2] = _mm_blend_epi16(cqes[2], cqe_tmp1, 0x04);
		/* C.2 generate final structure for mbuf with swapping bytes. */
		pkt_mb3 = _mm_shuffle_epi8(cqes[3], shuf_mask);
		pkt_mb2 = _mm_shuffle_epi8(cqes[2], shuf_mask);
		/* C.3 adjust CRC length. */
		pkt_mb3 = _mm_sub_epi16(pkt_mb3, crc_adj);
		pkt_mb2 = _mm_sub_epi16(pkt_mb2, crc_adj);
		/* C.4 adjust flow mark. */
		pkt_mb3 = _mm_add_epi32(pkt_mb3, flow_mark_adj);
		pkt_mb2 = _mm_add_epi32(pkt_mb2, flow_mark_adj);
		/* D.1 fill in mbuf - rx_descriptor_fields1. */
		_mm_storeu_si128((void *)&pkts[pos + 3]->pkt_len, pkt_mb3);
		_mm_storeu_si128((void *)&pkts[pos + 2]->pkt_len, pkt_mb2);
		/* E.1 extract op_own field. */
		op_own_tmp2 = _mm_unpacklo_epi32(cqes[2], cqes[3]);
		/* C.1 load remained CQE data and extract necessary fields. */
		cqe_tmp2 = _mm_load_si128((__m128i *)&cq[pos + p1]);
		cqe_tmp1 = _mm_load_si128((__m128i *)&cq[pos]);
		cqes[1] = _mm_blendv_epi8(cqes[1], cqe_tmp2, blend_mask);
		cqes[0] = _mm_blendv_epi8(cqes[0], cqe_tmp1, blend_mask);
		cqe_tmp2 = _mm_loadu_si128((__m128i *)&cq[pos + p1].rsvd1[3]);
		cqe_tmp1 = _mm_loadu_si128((__m128i *)&cq[pos].rsvd1[3]);
		cqes[1] = _mm_blend_epi16(cqes[1], cqe_tmp2, 0x30);
		cqes[0] = _mm_blend_epi16(cqes[0], cqe_tmp1, 0x30);
		cqe_tmp2 = _mm_loadl_epi64((__m128i *)&cq[pos + p1].rsvd2[10]);
		cqe_tmp1 = _mm_loadl_epi64((__m128i *)&cq[pos].rsvd2[10]);
		cqes[1] = _mm_blend_epi16(cqes[1], cqe_tmp2, 0x04);
		cqes[0] = _mm_blend_epi16(cqes[0], cqe_tmp1, 0x04);
		/* C.2 generate final structure for mbuf with swapping bytes. */
		pkt_mb1 = _mm_shuffle_epi8(cqes[1], shuf_mask);
		pkt_mb0 = _mm_shuffle_epi8(cqes[0], shuf_mask);
		/* C.3 adjust CRC length. */
		pkt_mb1 = _mm_sub_epi16(pkt_mb1, crc_adj);
		pkt_mb0 = _mm_sub_epi16(pkt_mb0, crc_adj);
		/* C.4 adjust flow mark. */
		pkt_mb1 = _mm_add_epi32(pkt_mb1, flow_mark_adj);
		pkt_mb0 = _mm_add_epi32(pkt_mb0, flow_mark_adj);
		/* E.1 extract op_own byte. */
		op_own_tmp1 = _mm_unpacklo_epi32(cqes[0], cqes[1]);
		op_own = _mm_unpackhi_epi64(op_own_tmp1, op_own_tmp2);
		/* D.1 fill in mbuf - rx_descriptor_fields1. */
		_mm_storeu_si128((void *)&pkts[pos + 1]->pkt_len, pkt_mb1);
		_mm_storeu_si128((void *)&pkts[pos]->pkt_len, pkt_mb0);
		/* E.2 flip owner bit to mark CQEs from last round. */
		owner_mask = _mm_and_si128(op_own, owner_check);
		if (ownership)
			owner_mask = _mm_xor_si128(owner_mask, owner_check);
		owner_mask = _mm_cmpeq_epi32(owner_mask, owner_check);
		owner_mask = _mm_packs_epi32(owner_mask, zero);
		/* E.3 get mask for invalidated CQEs. */
		opcode = _mm_and_si128(op_own, opcode_check);
		invalid_mask = _mm_cmpeq_epi32(opcode_check, opcode);
		invalid_mask = _mm_packs_epi32(invalid_mask, zero);
		/* E.4 mask out beyond boundary. */
		invalid_mask = _mm_or_si128(invalid_mask, mask);
		/* E.5 merge invalid_mask with invalid owner. */
		invalid_mask = _mm_or_si128(invalid_mask, owner_mask);
		/* F.1 find compressed CQE format. */
		comp_mask = _mm_and_si128(op_own, format_check);
		comp_mask = _mm_cmpeq_epi32(comp_mask, format_check);
		comp_mask = _mm_packs_epi32(comp_mask, zero);
		/* F.2 mask out invalid entries. */
		comp_mask = _mm_andnot_si128(invalid_mask, comp_mask);
		comp_idx = _mm_cvtsi128_si64(comp_mask);
		/* F.3 get the first compressed CQE. */
		comp_idx = comp_idx ?
				__builtin_ctzll(comp_idx) /
					(sizeof(uint16_t) * 8) :
				MLX5_VPMD_DESCS_PER_LOOP;
		/* E.6 mask out entries after the compressed CQE. */
		mask = _mm_set_epi64x(0, comp_idx * sizeof(uint16_t) * 8);
		mask = _mm_sll_epi64(ones, mask);
		invalid_mask = _mm_or_si128(invalid_mask, mask);
		/* E.7 count non-compressed valid CQEs. */
		n = _mm_cvtsi128_si64(invalid_mask);
		n = n ? __builtin_ctzll(n) / (sizeof(uint16_t) * 8) :
			MLX5_VPMD_DESCS_PER_LOOP;
		nocmp_n += n;
		/* D.2 get the final invalid mask. */
		mask = _mm_set_epi64x(0, n * sizeof(uint16_t) * 8);
		mask = _mm_sll_epi64(ones, mask);
		invalid_mask = _mm_or_si128(invalid_mask, mask);
		/* D.3 check error in opcode. */
		opcode = _mm_cmpeq_epi32(resp_err_check, opcode);
		opcode = _mm_packs_epi32(opcode, zero);
		opcode = _mm_andnot_si128(invalid_mask, opcode);
		/* D.4 mark if any error is set */
		rxq->pending_err |= !!_mm_cvtsi128_si64(opcode);
		/* D.5 fill in mbuf - rearm_data and packet_type. */
		rxq_cq_to_ptype_oflags_v(rxq, cqes, opcode, &pkts[pos]);
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Add up received bytes count. */
		byte_cnt = _mm_shuffle_epi8(op_own, len_shuf_mask);
		byte_cnt = _mm_andnot_si128(invalid_mask, byte_cnt);
		byte_cnt = _mm_hadd_epi16(byte_cnt, zero);
		rcvd_byte += _mm_cvtsi128_si64(_mm_hadd_epi16(byte_cnt, zero));
#endif
		/*
		 * Break the loop unless more valid CQE is expected, or if
		 * there's a compressed CQE.
		 */
		if (n != MLX5_VPMD_DESCS_PER_LOOP)
			break;
	}
	/* If no new CQE seen, return without updating cq_db. */
	if (unlikely(!nocmp_n && comp_idx == MLX5_VPMD_DESCS_PER_LOOP))
		return rcvd_pkt;
	/* Update the consumer indexes for non-compressed CQEs. */
	assert(nocmp_n <= pkts_n);
	rxq->cq_ci += nocmp_n;
	rxq->rq_pi += nocmp_n;
	rcvd_pkt += nocmp_n;
#ifdef MLX5_PMD_SOFT_COUNTERS
	rxq->stats.ipackets += nocmp_n;
	rxq->stats.ibytes += rcvd_byte;
#endif
	/* Decompress the last CQE if compressed. */
	if (comp_idx < MLX5_VPMD_DESCS_PER_LOOP && comp_idx == n) {
		assert(comp_idx == (nocmp_n % MLX5_VPMD_DESCS_PER_LOOP));
		rxq_cq_decompress_v(rxq, &cq[nocmp_n], &elts[nocmp_n]);
		/* Return more packets if needed. */
		if (nocmp_n < pkts_n) {
			uint16_t n = rxq->cq_ci - rxq->rq_pi;

			n = RTE_MIN(n, pkts_n - nocmp_n);
			rxq_copy_mbuf_v(rxq, &pkts[nocmp_n], n);
			rxq->rq_pi += n;
			rcvd_pkt += n;
		}
	}
	rte_wmb();
	*rxq->cq_db = htonl(rxq->cq_ci);
	return rcvd_pkt;
}

/**
 * DPDK callback for vectorized RX.
 *
 * @param dpdk_rxq
 *   Generic pointer to RX queue structure.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 *
 * @return
 *   Number of packets successfully received (<= pkts_n).
 */
uint16_t
mlx5_rx_burst_vec(void *dpdk_rxq, struct rte_mbuf **pkts, uint16_t pkts_n)
{
	struct rxq *rxq = dpdk_rxq;
	uint16_t nb_rx;

	nb_rx = rxq_burst_v(rxq, pkts, pkts_n);
	if (unlikely(rxq->pending_err))
		nb_rx = rxq_handle_pending_error(rxq, pkts, nb_rx);
	return nb_rx;
}

/**
 * Check Tx queue flags are set for raw vectorized Tx.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   1 if supported, negative errno value if not.
 */
int __attribute__((cold))
priv_check_raw_vec_tx_support(struct priv *priv)
{
	uint16_t i;

	/* All the configured queues should support. */
	for (i = 0; i < priv->txqs_n; ++i) {
		struct txq *txq = (*priv->txqs)[i];

		if (!(txq->flags & ETH_TXQ_FLAGS_NOMULTSEGS) ||
		    !(txq->flags & ETH_TXQ_FLAGS_NOOFFLOADS))
			break;
	}
	if (i != priv->txqs_n)
		return -ENOTSUP;
	return 1;
}

/**
 * Check a device can support vectorized TX.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   1 if supported, negative errno value if not.
 */
int __attribute__((cold))
priv_check_vec_tx_support(struct priv *priv)
{
	if (!priv->tx_vec_en ||
	    priv->txqs_n > MLX5_VPMD_MIN_TXQS ||
	    priv->mps != MLX5_MPW_ENHANCED ||
	    priv->tso)
		return -ENOTSUP;
	return 1;
}

/**
 * Check a RX queue can support vectorized RX.
 *
 * @param rxq
 *   Pointer to RX queue.
 *
 * @return
 *   1 if supported, negative errno value if not.
 */
int __attribute__((cold))
rxq_check_vec_support(struct rxq *rxq)
{
	struct rxq_ctrl *ctrl = container_of(rxq, struct rxq_ctrl, rxq);

	if (!ctrl->priv->rx_vec_en || rxq->sges_n != 0)
		return -ENOTSUP;
	return 1;
}

/**
 * Check a device can support vectorized RX.
 *
 * @param priv
 *   Pointer to private structure.
 *
 * @return
 *   1 if supported, negative errno value if not.
 */
int __attribute__((cold))
priv_check_vec_rx_support(struct priv *priv)
{
	uint16_t i;

	if (!priv->rx_vec_en)
		return -ENOTSUP;
	/* All the configured queues should support. */
	for (i = 0; i < priv->rxqs_n; ++i) {
		struct rxq *rxq = (*priv->rxqs)[i];

		if (rxq_check_vec_support(rxq) < 0)
			break;
	}
	if (i != priv->rxqs_n)
		return -ENOTSUP;
	return 1;
}

/**
 * Prepare for vectorized RX.
 *
 * @param priv
 *   Pointer to private structure.
 */
void
priv_prep_vec_rx_function(struct priv *priv)
{
	uint16_t i;

	for (i = 0; i < priv->rxqs_n; ++i) {
		struct rxq *rxq = (*priv->rxqs)[i];
		struct rte_mbuf *mbuf_init = &rxq->fake_mbuf;
		const uint16_t desc = 1 << rxq->elts_n;
		int j;

		assert(rxq->elts_n == rxq->cqe_n);
		/* Initialize default rearm_data for vPMD. */
		mbuf_init->data_off = RTE_PKTMBUF_HEADROOM;
		rte_mbuf_refcnt_set(mbuf_init, 1);
		mbuf_init->nb_segs = 1;
		mbuf_init->port = rxq->port_id;
		/*
		 * prevent compiler reordering:
		 * rearm_data covers previous fields.
		 */
		rte_compiler_barrier();
		rxq->mbuf_initializer =
			*(uint64_t *)&mbuf_init->rearm_data;
		/* Padding with a fake mbuf for vectorized Rx. */
		for (j = 0; j < MLX5_VPMD_DESCS_PER_LOOP; ++j)
			(*rxq->elts)[desc + j] = &rxq->fake_mbuf;
		/* Mark that it need to be cleaned up for rxq_alloc_elts(). */
		rxq->trim_elts = 1;
	}
}
