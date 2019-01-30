/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_RXTX_VEC_NEON_H_
#define RTE_PMD_MLX5_RXTX_VEC_NEON_H_

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <arm_neon.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>

#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_rxtx_vec.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"

#pragma GCC diagnostic ignored "-Wcast-qual"

/**
 * Fill in buffer descriptors in a multi-packet send descriptor.
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param dseg
 *   Pointer to buffer descriptor to be written.
 * @param pkts
 *   Pointer to array of packets to be sent.
 * @param n
 *   Number of packets to be filled.
 */
static inline void
txq_wr_dseg_v(struct mlx5_txq_data *txq, uint8_t *dseg,
	      struct rte_mbuf **pkts, unsigned int n)
{
	unsigned int pos;
	uintptr_t addr;
	const uint8x16_t dseg_shuf_m = {
		 3,  2,  1,  0, /* length, bswap32 */
		 4,  5,  6,  7, /* lkey */
		15, 14, 13, 12, /* addr, bswap64 */
		11, 10,  9,  8
	};
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint32_t tx_byte = 0;
#endif

	for (pos = 0; pos < n; ++pos, dseg += MLX5_WQE_DWORD_SIZE) {
		uint8x16_t desc;
		struct rte_mbuf *pkt = pkts[pos];

		addr = rte_pktmbuf_mtod(pkt, uintptr_t);
		desc = vreinterpretq_u8_u32((uint32x4_t) {
				DATA_LEN(pkt),
				mlx5_tx_mb2mr(txq, pkt),
				addr,
				addr >> 32 });
		desc = vqtbl1q_u8(desc, dseg_shuf_m);
		vst1q_u8(dseg, desc);
#ifdef MLX5_PMD_SOFT_COUNTERS
		tx_byte += DATA_LEN(pkt);
#endif
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	txq->stats.obytes += tx_byte;
#endif
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
txq_scatter_v(struct mlx5_txq_data *txq, struct rte_mbuf **pkts,
	      uint16_t pkts_n)
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
	bool metadata_ol =
		txq->offloads & DEV_TX_OFFLOAD_MATCH_METADATA ? true : false;

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
		const uint8x16_t ctrl_shuf_m = {
			3,  2,  1,  0, /* bswap32 */
			7,  6,  5,  4, /* bswap32 */
			11, 10,  9,  8, /* bswap32 */
			12, 13, 14, 15
		};
		uint8_t cs_flags;
		uint16_t max_elts;
		uint16_t max_wqe;
		uint8x16_t *t_wqe;
		uint8_t *dseg;
		uint8x16_t ctrl;
		rte_be32_t metadata =
			metadata_ol && (buf->ol_flags & PKT_TX_METADATA) ?
			buf->tx_metadata : 0;

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
		cs_flags = txq_ol_cksum_to_cs(buf);
		/* Title WQEBB pointer. */
		t_wqe = (uint8x16_t *)wqe;
		dseg = (uint8_t *)(wqe + 1);
		do {
			if (!(ds++ % nb_dword_per_wqebb)) {
				dseg = (uint8_t *)
					&((volatile struct mlx5_wqe64 *)
					   txq->wqes)[++wqe_ci & wq_mask];
			}
			txq_wr_dseg_v(txq, dseg, &buf, 1);
			dseg += MLX5_WQE_DWORD_SIZE;
			(*txq->elts)[elts_head++ & elts_m] = buf;
			buf = buf->next;
		} while (--segs_n);
		++wqe_ci;
		/* Fill CTRL in the header. */
		ctrl = vreinterpretq_u8_u32((uint32x4_t) {
				MLX5_OPC_MOD_MPW << 24 |
				txq->wqe_ci << 8 | MLX5_OPCODE_TSO,
				txq->qp_num_8s | ds, 0, 0});
		ctrl = vqtbl1q_u8(ctrl, ctrl_shuf_m);
		vst1q_u8((void *)t_wqe, ctrl);
		/* Fill ESEG in the header. */
		vst1q_u32((void *)(t_wqe + 1),
			  ((uint32x4_t){ 0,
					 cs_flags << 16 | rte_cpu_to_be_16(len),
					 metadata, 0 }));
		txq->wqe_ci = wqe_ci;
	}
	if (!n)
		return 0;
	txq->elts_comp += (uint16_t)(elts_head - txq->elts_head);
	txq->elts_head = elts_head;
	if (txq->elts_comp >= MLX5_TX_COMP_THRESH) {
		/* A CQE slot must always be available. */
		assert((1u << txq->cqe_n) - (txq->cq_pi++ - txq->cq_ci));
		wqe->ctrl[2] = rte_cpu_to_be_32(8);
		wqe->ctrl[3] = txq->elts_head;
		txq->elts_comp = 0;
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
 * This must be checked by txq_count_contig_single_seg() and txq_calc_offload().
 *
 * @param txq
 *   Pointer to TX queue structure.
 * @param pkts
 *   Pointer to array of packets to be sent.
 * @param pkts_n
 *   Number of packets to be sent (<= MLX5_VPMD_TX_MAX_BURST).
 * @param cs_flags
 *   Checksum offload flags to be written in the descriptor.
 * @param metadata
 *   Metadata value to be written in the descriptor.
 *
 * @return
 *   Number of packets successfully transmitted (<= pkts_n).
 */
static inline uint16_t
txq_burst_v(struct mlx5_txq_data *txq, struct rte_mbuf **pkts, uint16_t pkts_n,
	    uint8_t cs_flags, rte_be32_t metadata)
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
	const uint8x16_t ctrl_shuf_m = {
		 3,  2,  1,  0, /* bswap32 */
		 7,  6,  5,  4, /* bswap32 */
		11, 10,  9,  8, /* bswap32 */
		12, 13, 14, 15
	};
	uint8x16_t *t_wqe;
	uint8_t *dseg;
	uint8x16_t ctrl;

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
		vst1q_u64((void *)&elts[pos], vld1q_u64((void *)&pkts[pos]));
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
	t_wqe = (uint8x16_t *)wqe;
	dseg = (uint8_t *)(wqe + 1);
	/* Calculate the number of entries to the end. */
	n = RTE_MIN(
		(wq_n - wq_idx) * nb_dword_per_wqebb - nb_dword_in_hdr,
		pkts_n);
	/* Fill DSEGs. */
	txq_wr_dseg_v(txq, dseg, pkts, n);
	/* Check if it crosses the end of the queue. */
	if (n < pkts_n) {
		dseg = (uint8_t *)txq->wqes;
		txq_wr_dseg_v(txq, dseg, &pkts[n], pkts_n - n);
	}
	if (txq->elts_comp + pkts_n < MLX5_TX_COMP_THRESH) {
		txq->elts_comp += pkts_n;
	} else {
		/* A CQE slot must always be available. */
		assert((1u << txq->cqe_n) - (txq->cq_pi++ - txq->cq_ci));
		/* Request a completion. */
		txq->elts_comp = 0;
		comp_req = 8;
	}
	/* Fill CTRL in the header. */
	ctrl = vreinterpretq_u8_u32((uint32x4_t) {
			MLX5_OPC_MOD_ENHANCED_MPSW << 24 |
			txq->wqe_ci << 8 | MLX5_OPCODE_ENHANCED_MPSW,
			txq->qp_num_8s | (pkts_n + 2),
			comp_req,
			txq->elts_head });
	ctrl = vqtbl1q_u8(ctrl, ctrl_shuf_m);
	vst1q_u8((void *)t_wqe, ctrl);
	/* Fill ESEG in the header. */
	vst1q_u32((void *)(t_wqe + 1),
		 ((uint32x4_t) { 0, cs_flags, metadata, 0 }));
#ifdef MLX5_PMD_SOFT_COUNTERS
	txq->stats.opackets += pkts_n;
#endif
	txq->wqe_ci += (nb_dword_in_hdr + pkts_n + (nb_dword_per_wqebb - 1)) /
		       nb_dword_per_wqebb;
	/* Ring QP doorbell. */
	mlx5_tx_dbrec_cond_wmb(txq, wqe, pkts_n < MLX5_VPMD_TX_MAX_BURST);
	return pkts_n;
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
rxq_copy_mbuf_v(struct mlx5_rxq_data *rxq, struct rte_mbuf **pkts, uint16_t n)
{
	const uint16_t q_mask = (1 << rxq->elts_n) - 1;
	struct rte_mbuf **elts = &(*rxq->elts)[rxq->rq_pi & q_mask];
	unsigned int pos;
	uint16_t p = n & -2;

	for (pos = 0; pos < p; pos += 2) {
		uint64x2_t mbp;

		mbp = vld1q_u64((void *)&elts[pos]);
		vst1q_u64((void *)&pkts[pos], mbp);
	}
	if (n & 1)
		pkts[pos] = elts[pos];
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
rxq_cq_decompress_v(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cq,
		    struct rte_mbuf **elts)
{
	volatile struct mlx5_mini_cqe8 *mcq = (void *)&(cq + 1)->pkt_info;
	struct rte_mbuf *t_pkt = elts[0]; /* Title packet is pre-built. */
	unsigned int pos;
	unsigned int i;
	unsigned int inv = 0;
	/* Mask to shuffle from extracted mini CQE to mbuf. */
	const uint8x16_t mcqe_shuf_m1 = {
		-1, -1, -1, -1, /* skip packet_type */
		 7,  6, -1, -1, /* pkt_len, bswap16 */
		 7,  6,         /* data_len, bswap16 */
		-1, -1,         /* skip vlan_tci */
		 3,  2,  1,  0  /* hash.rss, bswap32 */
	};
	const uint8x16_t mcqe_shuf_m2 = {
		-1, -1, -1, -1, /* skip packet_type */
		15, 14, -1, -1, /* pkt_len, bswap16 */
		15, 14,         /* data_len, bswap16 */
		-1, -1,         /* skip vlan_tci */
		11, 10,  9,  8  /* hash.rss, bswap32 */
	};
	/* Restore the compressed count. Must be 16 bits. */
	const uint16_t mcqe_n = t_pkt->data_len +
				(rxq->crc_present * ETHER_CRC_LEN);
	const uint64x2_t rearm =
		vld1q_u64((void *)&t_pkt->rearm_data);
	const uint32x4_t rxdf_mask = {
		0xffffffff, /* packet_type */
		0,          /* skip pkt_len */
		0xffff0000, /* vlan_tci, skip data_len */
		0,          /* skip hash.rss */
	};
	const uint8x16_t rxdf =
		vandq_u8(vld1q_u8((void *)&t_pkt->rx_descriptor_fields1),
			 vreinterpretq_u8_u32(rxdf_mask));
	const uint16x8_t crc_adj = {
		0, 0,
		rxq->crc_present * ETHER_CRC_LEN, 0,
		rxq->crc_present * ETHER_CRC_LEN, 0,
		0, 0
	};
	const uint32_t flow_tag = t_pkt->hash.fdir.hi;
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint32_t rcvd_byte = 0;
#endif
	/* Mask to shuffle byte_cnt to add up stats. Do bswap16 for all. */
	const uint8x8_t len_shuf_m = {
		 7,  6,         /* 1st mCQE */
		15, 14,         /* 2nd mCQE */
		23, 22,         /* 3rd mCQE */
		31, 30          /* 4th mCQE */
	};

	/*
	 * A. load mCQEs into a 128bit register.
	 * B. store rearm data to mbuf.
	 * C. combine data from mCQEs with rx_descriptor_fields1.
	 * D. store rx_descriptor_fields1.
	 * E. store flow tag (rte_flow mark).
	 */
	for (pos = 0; pos < mcqe_n; ) {
		uint8_t *p = (void *)&mcq[pos % 8];
		uint8_t *e0 = (void *)&elts[pos]->rearm_data;
		uint8_t *e1 = (void *)&elts[pos + 1]->rearm_data;
		uint8_t *e2 = (void *)&elts[pos + 2]->rearm_data;
		uint8_t *e3 = (void *)&elts[pos + 3]->rearm_data;
		uint16x4_t byte_cnt;
#ifdef MLX5_PMD_SOFT_COUNTERS
		uint16x4_t invalid_mask =
			vcreate_u16(mcqe_n - pos < MLX5_VPMD_DESCS_PER_LOOP ?
				    -1UL << ((mcqe_n - pos) *
					     sizeof(uint16_t) * 8) : 0);
#endif

		if (!(pos & 0x7) && pos + 8 < mcqe_n)
			rte_prefetch0((void *)(cq + pos + 8));
		__asm__ volatile (
		/* A.1 load mCQEs into a 128bit register. */
		"ld1 {v16.16b - v17.16b}, [%[mcq]] \n\t"
		/* B.1 store rearm data to mbuf. */
		"st1 {%[rearm].2d}, [%[e0]] \n\t"
		"add %[e0], %[e0], #16 \n\t"
		"st1 {%[rearm].2d}, [%[e1]] \n\t"
		"add %[e1], %[e1], #16 \n\t"
		/* C.1 combine data from mCQEs with rx_descriptor_fields1. */
		"tbl v18.16b, {v16.16b}, %[mcqe_shuf_m1].16b \n\t"
		"tbl v19.16b, {v16.16b}, %[mcqe_shuf_m2].16b \n\t"
		"sub v18.8h, v18.8h, %[crc_adj].8h \n\t"
		"sub v19.8h, v19.8h, %[crc_adj].8h \n\t"
		"orr v18.16b, v18.16b, %[rxdf].16b \n\t"
		"orr v19.16b, v19.16b, %[rxdf].16b \n\t"
		/* D.1 store rx_descriptor_fields1. */
		"st1 {v18.2d}, [%[e0]] \n\t"
		"st1 {v19.2d}, [%[e1]] \n\t"
		/* B.1 store rearm data to mbuf. */
		"st1 {%[rearm].2d}, [%[e2]] \n\t"
		"add %[e2], %[e2], #16 \n\t"
		"st1 {%[rearm].2d}, [%[e3]] \n\t"
		"add %[e3], %[e3], #16 \n\t"
		/* C.1 combine data from mCQEs with rx_descriptor_fields1. */
		"tbl v18.16b, {v17.16b}, %[mcqe_shuf_m1].16b \n\t"
		"tbl v19.16b, {v17.16b}, %[mcqe_shuf_m2].16b \n\t"
		"sub v18.8h, v18.8h, %[crc_adj].8h \n\t"
		"sub v19.8h, v19.8h, %[crc_adj].8h \n\t"
		"orr v18.16b, v18.16b, %[rxdf].16b \n\t"
		"orr v19.16b, v19.16b, %[rxdf].16b \n\t"
		/* D.1 store rx_descriptor_fields1. */
		"st1 {v18.2d}, [%[e2]] \n\t"
		"st1 {v19.2d}, [%[e3]] \n\t"
#ifdef MLX5_PMD_SOFT_COUNTERS
		"tbl %[byte_cnt].8b, {v16.16b - v17.16b}, %[len_shuf_m].8b \n\t"
#endif
		:[byte_cnt]"=&w"(byte_cnt)
		:[mcq]"r"(p),
		 [rxdf]"w"(rxdf),
		 [rearm]"w"(rearm),
		 [e3]"r"(e3), [e2]"r"(e2), [e1]"r"(e1), [e0]"r"(e0),
		 [mcqe_shuf_m1]"w"(mcqe_shuf_m1),
		 [mcqe_shuf_m2]"w"(mcqe_shuf_m2),
		 [crc_adj]"w"(crc_adj),
		 [len_shuf_m]"w"(len_shuf_m)
		:"memory", "v16", "v17", "v18", "v19");
#ifdef MLX5_PMD_SOFT_COUNTERS
		byte_cnt = vbic_u16(byte_cnt, invalid_mask);
		rcvd_byte += vget_lane_u64(vpaddl_u32(vpaddl_u16(byte_cnt)), 0);
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
			mcq = (void *)&(cq + pos)->pkt_info;
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
 * @param ptype_info
 *   Array of four 4bytes packet type info extracted from the original
 *   completion descriptor.
 * @param flow_tag
 *   Array of four 4bytes flow ID extracted from the original completion
 *   descriptor.
 * @param op_err
 *   Opcode vector having responder error status. Each field is 4B.
 * @param pkts
 *   Pointer to array of packets to be filled.
 */
static inline void
rxq_cq_to_ptype_oflags_v(struct mlx5_rxq_data *rxq,
			 uint32x4_t ptype_info, uint32x4_t flow_tag,
			 uint16x4_t op_err, struct rte_mbuf **pkts)
{
	uint16x4_t ptype;
	uint32x4_t pinfo, cv_flags;
	uint32x4_t ol_flags =
		vdupq_n_u32(rxq->rss_hash * PKT_RX_RSS_HASH |
			    rxq->hw_timestamp * PKT_RX_TIMESTAMP);
	const uint32x4_t ptype_ol_mask = { 0x106, 0x106, 0x106, 0x106 };
	const uint8x16_t cv_flag_sel = {
		0,
		(uint8_t)(PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED),
		(uint8_t)(PKT_RX_IP_CKSUM_GOOD >> 1),
		0,
		(uint8_t)(PKT_RX_L4_CKSUM_GOOD >> 1),
		0,
		(uint8_t)((PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD) >> 1),
		0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	const uint32x4_t cv_mask =
		vdupq_n_u32(PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD |
			    PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED);
	const uint64x1_t mbuf_init = vld1_u64(&rxq->mbuf_initializer);
	const uint64x1_t r32_mask = vcreate_u64(0xffffffff);
	uint64x2_t rearm0, rearm1, rearm2, rearm3;
	uint8_t pt_idx0, pt_idx1, pt_idx2, pt_idx3;

	if (rxq->mark) {
		const uint32x4_t ft_def = vdupq_n_u32(MLX5_FLOW_MARK_DEFAULT);
		const uint32x4_t fdir_flags = vdupq_n_u32(PKT_RX_FDIR);
		uint32x4_t fdir_id_flags = vdupq_n_u32(PKT_RX_FDIR_ID);
		uint32x4_t invalid_mask;

		/* Check if flow tag is non-zero then set PKT_RX_FDIR. */
		invalid_mask = vceqzq_u32(flow_tag);
		ol_flags = vorrq_u32(ol_flags,
				     vbicq_u32(fdir_flags, invalid_mask));
		/* Mask out invalid entries. */
		fdir_id_flags = vbicq_u32(fdir_id_flags, invalid_mask);
		/* Check if flow tag MLX5_FLOW_MARK_DEFAULT. */
		ol_flags = vorrq_u32(ol_flags,
				     vbicq_u32(fdir_id_flags,
					       vceqq_u32(flow_tag, ft_def)));
	}
	/*
	 * ptype_info has the following:
	 * bit[1]     = l3_ok
	 * bit[2]     = l4_ok
	 * bit[8]     = cv
	 * bit[11:10] = l3_hdr_type
	 * bit[14:12] = l4_hdr_type
	 * bit[15]    = ip_frag
	 * bit[16]    = tunneled
	 * bit[17]    = outer_l3_type
	 */
	ptype = vshrn_n_u32(ptype_info, 10);
	/* Errored packets will have RTE_PTYPE_ALL_MASK. */
	ptype = vorr_u16(ptype, op_err);
	pt_idx0 = vget_lane_u8(vreinterpret_u8_u16(ptype), 6);
	pt_idx1 = vget_lane_u8(vreinterpret_u8_u16(ptype), 4);
	pt_idx2 = vget_lane_u8(vreinterpret_u8_u16(ptype), 2);
	pt_idx3 = vget_lane_u8(vreinterpret_u8_u16(ptype), 0);
	pkts[0]->packet_type = mlx5_ptype_table[pt_idx0] |
			       !!(pt_idx0 & (1 << 6)) * rxq->tunnel;
	pkts[1]->packet_type = mlx5_ptype_table[pt_idx1] |
			       !!(pt_idx1 & (1 << 6)) * rxq->tunnel;
	pkts[2]->packet_type = mlx5_ptype_table[pt_idx2] |
			       !!(pt_idx2 & (1 << 6)) * rxq->tunnel;
	pkts[3]->packet_type = mlx5_ptype_table[pt_idx3] |
			       !!(pt_idx3 & (1 << 6)) * rxq->tunnel;
	/* Fill flags for checksum and VLAN. */
	pinfo = vandq_u32(ptype_info, ptype_ol_mask);
	pinfo = vreinterpretq_u32_u8(
		vqtbl1q_u8(cv_flag_sel, vreinterpretq_u8_u32(pinfo)));
	/* Locate checksum flags at byte[2:1] and merge with VLAN flags. */
	cv_flags = vshlq_n_u32(pinfo, 9);
	cv_flags = vorrq_u32(pinfo, cv_flags);
	/* Move back flags to start from byte[0]. */
	cv_flags = vshrq_n_u32(cv_flags, 8);
	/* Mask out garbage bits. */
	cv_flags = vandq_u32(cv_flags, cv_mask);
	/* Merge to ol_flags. */
	ol_flags = vorrq_u32(ol_flags, cv_flags);
	/* Merge mbuf_init and ol_flags, and store. */
	rearm0 = vcombine_u64(mbuf_init,
			      vshr_n_u64(vget_high_u64(vreinterpretq_u64_u32(
						       ol_flags)), 32));
	rearm1 = vcombine_u64(mbuf_init,
			      vand_u64(vget_high_u64(vreinterpretq_u64_u32(
						     ol_flags)), r32_mask));
	rearm2 = vcombine_u64(mbuf_init,
			      vshr_n_u64(vget_low_u64(vreinterpretq_u64_u32(
						      ol_flags)), 32));
	rearm3 = vcombine_u64(mbuf_init,
			      vand_u64(vget_low_u64(vreinterpretq_u64_u32(
						    ol_flags)), r32_mask));
	vst1q_u64((void *)&pkts[0]->rearm_data, rearm0);
	vst1q_u64((void *)&pkts[1]->rearm_data, rearm1);
	vst1q_u64((void *)&pkts[2]->rearm_data, rearm2);
	vst1q_u64((void *)&pkts[3]->rearm_data, rearm3);
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
 * @param[out] err
 *   Pointer to a flag. Set non-zero value if pkts array has at least one error
 *   packet to handle.
 *
 * @return
 *   Number of packets received including errors (<= pkts_n).
 */
static inline uint16_t
rxq_burst_v(struct mlx5_rxq_data *rxq, struct rte_mbuf **pkts, uint16_t pkts_n,
	    uint64_t *err)
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
	const uint16x4_t ownership = vdup_n_u16(!(rxq->cq_ci & (q_mask + 1)));
	const uint16x4_t owner_check = vcreate_u16(0x0001000100010001);
	const uint16x4_t opcode_check = vcreate_u16(0x00f000f000f000f0);
	const uint16x4_t format_check = vcreate_u16(0x000c000c000c000c);
	const uint16x4_t resp_err_check = vcreate_u16(0x00e000e000e000e0);
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint32_t rcvd_byte = 0;
#endif
	/* Mask to generate 16B length vector. */
	const uint8x8_t len_shuf_m = {
		52, 53,         /* 4th CQE */
		36, 37,         /* 3rd CQE */
		20, 21,         /* 2nd CQE */
		 4,  5          /* 1st CQE */
	};
	/* Mask to extract 16B data from a 64B CQE. */
	const uint8x16_t cqe_shuf_m = {
		28, 29,         /* hdr_type_etc */
		 0,             /* pkt_info */
		-1,             /* null */
		47, 46,         /* byte_cnt, bswap16 */
		31, 30,         /* vlan_info, bswap16 */
		15, 14, 13, 12, /* rx_hash_res, bswap32 */
		57, 58, 59,     /* flow_tag */
		63              /* op_own */
	};
	/* Mask to generate 16B data for mbuf. */
	const uint8x16_t mb_shuf_m = {
		 4,  5, -1, -1, /* pkt_len */
		 4,  5,         /* data_len */
		 6,  7,         /* vlan_tci */
		 8,  9, 10, 11, /* hash.rss */
		12, 13, 14, -1  /* hash.fdir.hi */
	};
	/* Mask to generate 16B owner vector. */
	const uint8x8_t owner_shuf_m = {
		63, -1,         /* 4th CQE */
		47, -1,         /* 3rd CQE */
		31, -1,         /* 2nd CQE */
		15, -1          /* 1st CQE */
	};
	/* Mask to generate a vector having packet_type/ol_flags. */
	const uint8x16_t ptype_shuf_m = {
		48, 49, 50, -1, /* 4th CQE */
		32, 33, 34, -1, /* 3rd CQE */
		16, 17, 18, -1, /* 2nd CQE */
		 0,  1,  2, -1  /* 1st CQE */
	};
	/* Mask to generate a vector having flow tags. */
	const uint8x16_t ftag_shuf_m = {
		60, 61, 62, -1, /* 4th CQE */
		44, 45, 46, -1, /* 3rd CQE */
		28, 29, 30, -1, /* 2nd CQE */
		12, 13, 14, -1  /* 1st CQE */
	};
	const uint16x8_t crc_adj = {
		0, 0, rxq->crc_present * ETHER_CRC_LEN, 0, 0, 0, 0, 0
	};
	const uint32x4_t flow_mark_adj = { 0, 0, 0, rxq->mark * (-1) };

	assert(rxq->sges_n == 0);
	assert(rxq->cqe_n == rxq->elts_n);
	cq = &(*rxq->cqes)[cq_idx];
	rte_prefetch_non_temporal(cq);
	rte_prefetch_non_temporal(cq + 1);
	rte_prefetch_non_temporal(cq + 2);
	rte_prefetch_non_temporal(cq + 3);
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
	if (repl_n >= rxq->rq_repl_thresh)
		mlx5_rx_replenish_bulk_mbuf(rxq, repl_n);
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
	 * Note that vectors have reverse order - {v3, v2, v1, v0}, because
	 * there's no instruction to count trailing zeros. __builtin_clzl() is
	 * used instead.
	 *
	 * A. copy 4 mbuf pointers from elts ring to returing pkts.
	 * B. load 64B CQE and extract necessary fields
	 *    Final 16bytes cqes[] extracted from original 64bytes CQE has the
	 *    following structure:
	 *        struct {
	 *          uint16_t hdr_type_etc;
	 *          uint8_t  pkt_info;
	 *          uint8_t  rsvd;
	 *          uint16_t byte_cnt;
	 *          uint16_t vlan_info;
	 *          uint32_t rx_has_res;
	 *          uint8_t  flow_tag[3];
	 *          uint8_t  op_own;
	 *        } c;
	 * C. fill in mbuf.
	 * D. get valid CQEs.
	 * E. find compressed CQE.
	 */
	for (pos = 0;
	     pos < pkts_n;
	     pos += MLX5_VPMD_DESCS_PER_LOOP) {
		uint16x4_t op_own;
		uint16x4_t opcode, owner_mask, invalid_mask;
		uint16x4_t comp_mask;
		uint16x4_t mask;
		uint16x4_t byte_cnt;
		uint32x4_t ptype_info, flow_tag;
		register uint64x2_t c0, c1, c2, c3;
		uint8_t *p0, *p1, *p2, *p3;
		uint8_t *e0 = (void *)&elts[pos]->pkt_len;
		uint8_t *e1 = (void *)&elts[pos + 1]->pkt_len;
		uint8_t *e2 = (void *)&elts[pos + 2]->pkt_len;
		uint8_t *e3 = (void *)&elts[pos + 3]->pkt_len;
		void *elts_p = (void *)&elts[pos];
		void *pkts_p = (void *)&pkts[pos];

		/* A.0 do not cross the end of CQ. */
		mask = vcreate_u16(pkts_n - pos < MLX5_VPMD_DESCS_PER_LOOP ?
				   -1UL >> ((pkts_n - pos) *
					    sizeof(uint16_t) * 8) : 0);
		p0 = (void *)&cq[pos].pkt_info;
		p1 = p0 + (pkts_n - pos > 1) * sizeof(struct mlx5_cqe);
		p2 = p1 + (pkts_n - pos > 2) * sizeof(struct mlx5_cqe);
		p3 = p2 + (pkts_n - pos > 3) * sizeof(struct mlx5_cqe);
		/* B.0 (CQE 3) load a block having op_own. */
		c3 = vld1q_u64((uint64_t *)(p3 + 48));
		/* B.0 (CQE 2) load a block having op_own. */
		c2 = vld1q_u64((uint64_t *)(p2 + 48));
		/* B.0 (CQE 1) load a block having op_own. */
		c1 = vld1q_u64((uint64_t *)(p1 + 48));
		/* B.0 (CQE 0) load a block having op_own. */
		c0 = vld1q_u64((uint64_t *)(p0 + 48));
		/* Synchronize for loading the rest of blocks. */
		rte_cio_rmb();
		/* Prefetch next 4 CQEs. */
		if (pkts_n - pos >= 2 * MLX5_VPMD_DESCS_PER_LOOP) {
			unsigned int next = pos + MLX5_VPMD_DESCS_PER_LOOP;
			rte_prefetch_non_temporal(&cq[next]);
			rte_prefetch_non_temporal(&cq[next + 1]);
			rte_prefetch_non_temporal(&cq[next + 2]);
			rte_prefetch_non_temporal(&cq[next + 3]);
		}
		__asm__ volatile (
		/* B.1 (CQE 3) load the rest of blocks. */
		"ld1 {v16.16b - v18.16b}, [%[p3]] \n\t"
		/* B.2 (CQE 3) move the block having op_own. */
		"mov v19.16b, %[c3].16b \n\t"
		/* B.3 (CQE 3) extract 16B fields. */
		"tbl v23.16b, {v16.16b - v19.16b}, %[cqe_shuf_m].16b \n\t"
		/* B.1 (CQE 2) load the rest of blocks. */
		"ld1 {v16.16b - v18.16b}, [%[p2]] \n\t"
		/* B.4 (CQE 3) adjust CRC length. */
		"sub v23.8h, v23.8h, %[crc_adj].8h \n\t"
		/* C.1 (CQE 3) generate final structure for mbuf. */
		"tbl v15.16b, {v23.16b}, %[mb_shuf_m].16b \n\t"
		/* B.2 (CQE 2) move the block having op_own. */
		"mov v19.16b, %[c2].16b \n\t"
		/* B.3 (CQE 2) extract 16B fields. */
		"tbl v22.16b, {v16.16b - v19.16b}, %[cqe_shuf_m].16b \n\t"
		/* B.1 (CQE 1) load the rest of blocks. */
		"ld1 {v16.16b - v18.16b}, [%[p1]] \n\t"
		/* B.4 (CQE 2) adjust CRC length. */
		"sub v22.8h, v22.8h, %[crc_adj].8h \n\t"
		/* C.1 (CQE 2) generate final structure for mbuf. */
		"tbl v14.16b, {v22.16b}, %[mb_shuf_m].16b \n\t"
		/* B.2 (CQE 1) move the block having op_own. */
		"mov v19.16b, %[c1].16b \n\t"
		/* B.3 (CQE 1) extract 16B fields. */
		"tbl v21.16b, {v16.16b - v19.16b}, %[cqe_shuf_m].16b \n\t"
		/* B.1 (CQE 0) load the rest of blocks. */
		"ld1 {v16.16b - v18.16b}, [%[p0]] \n\t"
		/* B.4 (CQE 1) adjust CRC length. */
		"sub v21.8h, v21.8h, %[crc_adj].8h \n\t"
		/* C.1 (CQE 1) generate final structure for mbuf. */
		"tbl v13.16b, {v21.16b}, %[mb_shuf_m].16b \n\t"
		/* B.2 (CQE 0) move the block having op_own. */
		"mov v19.16b, %[c0].16b \n\t"
		/* A.1 load mbuf pointers. */
		"ld1 {v24.2d - v25.2d}, [%[elts_p]] \n\t"
		/* B.3 (CQE 0) extract 16B fields. */
		"tbl v20.16b, {v16.16b - v19.16b}, %[cqe_shuf_m].16b \n\t"
		/* B.4 (CQE 0) adjust CRC length. */
		"sub v20.8h, v20.8h, %[crc_adj].8h \n\t"
		/* D.1 extract op_own byte. */
		"tbl %[op_own].8b, {v20.16b - v23.16b}, %[owner_shuf_m].8b \n\t"
		/* C.2 (CQE 3) adjust flow mark. */
		"add v15.4s, v15.4s, %[flow_mark_adj].4s \n\t"
		/* C.3 (CQE 3) fill in mbuf - rx_descriptor_fields1. */
		"st1 {v15.2d}, [%[e3]] \n\t"
		/* C.2 (CQE 2) adjust flow mark. */
		"add v14.4s, v14.4s, %[flow_mark_adj].4s \n\t"
		/* C.3 (CQE 2) fill in mbuf - rx_descriptor_fields1. */
		"st1 {v14.2d}, [%[e2]] \n\t"
		/* C.1 (CQE 0) generate final structure for mbuf. */
		"tbl v12.16b, {v20.16b}, %[mb_shuf_m].16b \n\t"
		/* C.2 (CQE 1) adjust flow mark. */
		"add v13.4s, v13.4s, %[flow_mark_adj].4s \n\t"
		/* C.3 (CQE 1) fill in mbuf - rx_descriptor_fields1. */
		"st1 {v13.2d}, [%[e1]] \n\t"
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Extract byte_cnt. */
		"tbl %[byte_cnt].8b, {v20.16b - v23.16b}, %[len_shuf_m].8b \n\t"
#endif
		/* Extract ptype_info. */
		"tbl %[ptype_info].16b, {v20.16b - v23.16b}, %[ptype_shuf_m].16b \n\t"
		/* Extract flow_tag. */
		"tbl %[flow_tag].16b, {v20.16b - v23.16b}, %[ftag_shuf_m].16b \n\t"
		/* A.2 copy mbuf pointers. */
		"st1 {v24.2d - v25.2d}, [%[pkts_p]] \n\t"
		/* C.2 (CQE 0) adjust flow mark. */
		"add v12.4s, v12.4s, %[flow_mark_adj].4s \n\t"
		/* C.3 (CQE 1) fill in mbuf - rx_descriptor_fields1. */
		"st1 {v12.2d}, [%[e0]] \n\t"
		:[op_own]"=&w"(op_own),
		 [byte_cnt]"=&w"(byte_cnt),
		 [ptype_info]"=&w"(ptype_info),
		 [flow_tag]"=&w"(flow_tag)
		:[p3]"r"(p3), [p2]"r"(p2), [p1]"r"(p1), [p0]"r"(p0),
		 [e3]"r"(e3), [e2]"r"(e2), [e1]"r"(e1), [e0]"r"(e0),
		 [c3]"w"(c3), [c2]"w"(c2), [c1]"w"(c1), [c0]"w"(c0),
		 [elts_p]"r"(elts_p),
		 [pkts_p]"r"(pkts_p),
		 [cqe_shuf_m]"w"(cqe_shuf_m),
		 [mb_shuf_m]"w"(mb_shuf_m),
		 [owner_shuf_m]"w"(owner_shuf_m),
		 [len_shuf_m]"w"(len_shuf_m),
		 [ptype_shuf_m]"w"(ptype_shuf_m),
		 [ftag_shuf_m]"w"(ftag_shuf_m),
		 [crc_adj]"w"(crc_adj),
		 [flow_mark_adj]"w"(flow_mark_adj)
		:"memory",
		 "v12", "v13", "v14", "v15",
		 "v16", "v17", "v18", "v19",
		 "v20", "v21", "v22", "v23",
		 "v24", "v25");
		/* D.2 flip owner bit to mark CQEs from last round. */
		owner_mask = vand_u16(op_own, owner_check);
		owner_mask = vceq_u16(owner_mask, ownership);
		/* D.3 get mask for invalidated CQEs. */
		opcode = vand_u16(op_own, opcode_check);
		invalid_mask = vceq_u16(opcode_check, opcode);
		/* E.1 find compressed CQE format. */
		comp_mask = vand_u16(op_own, format_check);
		comp_mask = vceq_u16(comp_mask, format_check);
		/* D.4 mask out beyond boundary. */
		invalid_mask = vorr_u16(invalid_mask, mask);
		/* D.5 merge invalid_mask with invalid owner. */
		invalid_mask = vorr_u16(invalid_mask, owner_mask);
		/* E.2 mask out invalid entries. */
		comp_mask = vbic_u16(comp_mask, invalid_mask);
		/* E.3 get the first compressed CQE. */
		comp_idx = __builtin_clzl(vget_lane_u64(vreinterpret_u64_u16(
					  comp_mask), 0)) /
					  (sizeof(uint16_t) * 8);
		/* D.6 mask out entries after the compressed CQE. */
		mask = vcreate_u16(comp_idx < MLX5_VPMD_DESCS_PER_LOOP ?
				   -1UL >> (comp_idx * sizeof(uint16_t) * 8) :
				   0);
		invalid_mask = vorr_u16(invalid_mask, mask);
		/* D.7 count non-compressed valid CQEs. */
		n = __builtin_clzl(vget_lane_u64(vreinterpret_u64_u16(
				   invalid_mask), 0)) / (sizeof(uint16_t) * 8);
		nocmp_n += n;
		/* D.2 get the final invalid mask. */
		mask = vcreate_u16(n < MLX5_VPMD_DESCS_PER_LOOP ?
				   -1UL >> (n * sizeof(uint16_t) * 8) : 0);
		invalid_mask = vorr_u16(invalid_mask, mask);
		/* D.3 check error in opcode. */
		opcode = vceq_u16(resp_err_check, opcode);
		opcode = vbic_u16(opcode, invalid_mask);
		/* D.4 mark if any error is set */
		*err |= vget_lane_u64(vreinterpret_u64_u16(opcode), 0);
		/* C.4 fill in mbuf - rearm_data and packet_type. */
		rxq_cq_to_ptype_oflags_v(rxq, ptype_info, flow_tag,
					 opcode, &elts[pos]);
		if (rxq->hw_timestamp) {
			elts[pos]->timestamp =
				rte_be_to_cpu_64(
					container_of(p0, struct mlx5_cqe,
						     pkt_info)->timestamp);
			elts[pos + 1]->timestamp =
				rte_be_to_cpu_64(
					container_of(p1, struct mlx5_cqe,
						     pkt_info)->timestamp);
			elts[pos + 2]->timestamp =
				rte_be_to_cpu_64(
					container_of(p2, struct mlx5_cqe,
						     pkt_info)->timestamp);
			elts[pos + 3]->timestamp =
				rte_be_to_cpu_64(
					container_of(p3, struct mlx5_cqe,
						     pkt_info)->timestamp);
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Add up received bytes count. */
		byte_cnt = vbic_u16(byte_cnt, invalid_mask);
		rcvd_byte += vget_lane_u64(vpaddl_u32(vpaddl_u16(byte_cnt)), 0);
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
	rte_compiler_barrier();
	*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
	return rcvd_pkt;
}

#endif /* RTE_PMD_MLX5_RXTX_VEC_NEON_H_ */
