/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_RXTX_VEC_NEON_H_
#define RTE_PMD_MLX5_RXTX_VEC_NEON_H_

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <arm_neon.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_prefetch.h>

#include <mlx5_prm.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_utils.h"
#include "mlx5_rxtx.h"
#include "mlx5_rxtx_vec.h"
#include "mlx5_autoconf.h"

#pragma GCC diagnostic ignored "-Wcast-qual"

/**
 * Store free buffers to RX SW ring.
 *
 * @param elts
 *   Pointer to SW ring to be filled.
 * @param pkts
 *   Pointer to array of packets to be stored.
 * @param pkts_n
 *   Number of packets to be stored.
 */
static inline void
rxq_copy_mbuf_v(struct rte_mbuf **elts, struct rte_mbuf **pkts, uint16_t n)
{
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
 *
 * @return
 *   Number of mini-CQEs successfully decompressed.
 */
static inline uint16_t
rxq_cq_decompress_v(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cq,
		    struct rte_mbuf **elts)
{
	volatile struct mlx5_mini_cqe8 *mcq =
		(void *)&(cq + !rxq->cqe_comp_layout)->pkt_info;
	/* Title packet is pre-built. */
	struct rte_mbuf *t_pkt = rxq->cqe_comp_layout ? &rxq->title_pkt : elts[0];
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
	uint16_t mcqe_n = (rxq->cqe_comp_layout) ?
		(MLX5_CQE_NUM_MINIS(cq->op_own) + 1) :
		t_pkt->data_len + (rxq->crc_present * RTE_ETHER_CRC_LEN);
	uint16_t pkts_n = mcqe_n;
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
		rxq->crc_present * RTE_ETHER_CRC_LEN, 0,
		rxq->crc_present * RTE_ETHER_CRC_LEN, 0,
		0, 0
	};
	uint32x4_t ol_flags = {0, 0, 0, 0};
	uint32x4_t ol_flags_mask = {0, 0, 0, 0};
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
cycle:
	if (rxq->cqe_comp_layout)
		rte_prefetch0((void *)(cq + mcqe_n));
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

		if (!rxq->cqe_comp_layout)
			for (i = 0; i < MLX5_VPMD_DESCS_PER_LOOP; ++i)
				if (likely(pos + i < mcqe_n))
					rte_prefetch0((void *)(cq + pos + i));
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
			if (rxq->mcqe_format !=
			    MLX5_CQE_RESP_FORMAT_FTAG_STRIDX) {
				const uint32_t flow_tag = t_pkt->hash.fdir.hi;

				/* E.1 store flow tag (rte_flow mark). */
				elts[pos]->hash.fdir.hi = flow_tag;
				elts[pos + 1]->hash.fdir.hi = flow_tag;
				elts[pos + 2]->hash.fdir.hi = flow_tag;
				elts[pos + 3]->hash.fdir.hi = flow_tag;
			}  else {
				const uint32x4_t flow_mark_adj = {
					-1, -1, -1, -1 };
				const uint8x16_t flow_mark_shuf = {
					28, 24, 25, -1,
					20, 16, 17, -1,
					12,  8,  9, -1,
					 4,  0,  1, -1};
				/* Extract flow_tag field. */
				const uint32x4_t ft_mask =
					vdupq_n_u32(MLX5_FLOW_MARK_DEFAULT);
				const uint32x4_t fdir_flags =
					vdupq_n_u32(RTE_MBUF_F_RX_FDIR);
				const uint32x4_t fdir_all_flags =
					vdupq_n_u32(RTE_MBUF_F_RX_FDIR |
						    RTE_MBUF_F_RX_FDIR_ID);
				uint32x4_t fdir_id_flags =
					vdupq_n_u32(RTE_MBUF_F_RX_FDIR_ID);
				uint32x4_t invalid_mask, ftag;

				__asm__ volatile
				/* A.1 load mCQEs into a 128bit register. */
				("ld1 {v16.16b - v17.16b}, [%[mcq]]\n\t"
				/* Extract flow_tag. */
				 "tbl %[ftag].16b, {v16.16b - v17.16b}, %[flow_mark_shuf].16b\n\t"
				: [ftag]"=&w"(ftag)
				: [mcq]"r"(p),
				  [flow_mark_shuf]"w"(flow_mark_shuf)
				: "memory", "v16", "v17");
				invalid_mask = vceqzq_u32(ftag);
				ol_flags_mask = vorrq_u32(ol_flags_mask,
							  fdir_all_flags);
				/* Set RTE_MBUF_F_RX_FDIR if flow tag is non-zero. */
				ol_flags = vorrq_u32(ol_flags,
					vbicq_u32(fdir_flags, invalid_mask));
				/* Mask out invalid entries. */
				fdir_id_flags = vbicq_u32(fdir_id_flags,
							  invalid_mask);
				/* Check if flow tag MLX5_FLOW_MARK_DEFAULT. */
				ol_flags = vorrq_u32(ol_flags,
					vbicq_u32(fdir_id_flags,
						  vceqq_u32(ftag, ft_mask)));
				ftag = vaddq_u32(ftag, flow_mark_adj);
				elts[pos]->hash.fdir.hi =
					vgetq_lane_u32(ftag, 3);
				elts[pos + 1]->hash.fdir.hi =
					vgetq_lane_u32(ftag, 2);
				elts[pos + 2]->hash.fdir.hi =
					vgetq_lane_u32(ftag, 1);
				elts[pos + 3]->hash.fdir.hi =
					vgetq_lane_u32(ftag, 0);
				}
		}
		if (unlikely(rxq->mcqe_format !=
			     MLX5_CQE_RESP_FORMAT_HASH)) {
			if (rxq->mcqe_format ==
			    MLX5_CQE_RESP_FORMAT_L34H_STRIDX) {
				const uint8_t pkt_info =
					(cq->pkt_info & 0x3) << 6;
				const uint8_t pkt_hdr0 =
					mcq[pos % 8].hdr_type;
				const uint8_t pkt_hdr1 =
					mcq[pos % 8 + 1].hdr_type;
				const uint8_t pkt_hdr2 =
					mcq[pos % 8 + 2].hdr_type;
				const uint8_t pkt_hdr3 =
					mcq[pos % 8 + 3].hdr_type;
				const uint32x4_t vlan_mask =
					vdupq_n_u32(RTE_MBUF_F_RX_VLAN |
						    RTE_MBUF_F_RX_VLAN_STRIPPED);
				const uint32x4_t cv_mask =
					vdupq_n_u32(MLX5_CQE_VLAN_STRIPPED);
				const uint32x4_t pkt_cv = {
					pkt_hdr0 & 0x1, pkt_hdr1 & 0x1,
					pkt_hdr2 & 0x1, pkt_hdr3 & 0x1};

				ol_flags_mask = vorrq_u32(ol_flags_mask,
							  vlan_mask);
				ol_flags = vorrq_u32(ol_flags,
						vandq_u32(vlan_mask,
						vceqq_u32(pkt_cv, cv_mask)));
				elts[pos]->packet_type =
					mlx5_ptype_table[(pkt_hdr0 >> 2) |
							 pkt_info];
				elts[pos + 1]->packet_type =
					mlx5_ptype_table[(pkt_hdr1 >> 2) |
							 pkt_info];
				elts[pos + 2]->packet_type =
					mlx5_ptype_table[(pkt_hdr2 >> 2) |
							 pkt_info];
				elts[pos + 3]->packet_type =
					mlx5_ptype_table[(pkt_hdr3 >> 2) |
							 pkt_info];
				if (rxq->tunnel) {
					elts[pos]->packet_type |=
						!!(((pkt_hdr0 >> 2) |
						pkt_info) & (1 << 6));
					elts[pos + 1]->packet_type |=
						!!(((pkt_hdr1 >> 2) |
						pkt_info) & (1 << 6));
					elts[pos + 2]->packet_type |=
						!!(((pkt_hdr2 >> 2) |
						pkt_info) & (1 << 6));
					elts[pos + 3]->packet_type |=
						!!(((pkt_hdr3 >> 2) |
						pkt_info) & (1 << 6));
				}
			}
			const uint32x4_t hash_flags =
				vdupq_n_u32(RTE_MBUF_F_RX_RSS_HASH);
			const uint32x4_t rearm_flags =
				vdupq_n_u32((uint32_t)t_pkt->ol_flags);

			ol_flags_mask = vorrq_u32(ol_flags_mask, hash_flags);
			ol_flags = vorrq_u32(ol_flags,
					vbicq_u32(rearm_flags, ol_flags_mask));
			elts[pos]->ol_flags = vgetq_lane_u32(ol_flags, 3);
			elts[pos + 1]->ol_flags = vgetq_lane_u32(ol_flags, 2);
			elts[pos + 2]->ol_flags = vgetq_lane_u32(ol_flags, 1);
			elts[pos + 3]->ol_flags = vgetq_lane_u32(ol_flags, 0);
			elts[pos]->hash.rss = 0;
			elts[pos + 1]->hash.rss = 0;
			elts[pos + 2]->hash.rss = 0;
			elts[pos + 3]->hash.rss = 0;
		}
		if (rxq->dynf_meta) {
			int32_t offs = rxq->flow_meta_offset;
			const uint32_t meta =
				*RTE_MBUF_DYNFIELD(t_pkt, offs, uint32_t *);

			/* Check if title packet has valid metadata. */
			if (meta) {
				MLX5_ASSERT(t_pkt->ol_flags &
					    rxq->flow_meta_mask);
				*RTE_MBUF_DYNFIELD(elts[pos], offs,
							uint32_t *) = meta;
				*RTE_MBUF_DYNFIELD(elts[pos + 1], offs,
							uint32_t *) = meta;
				*RTE_MBUF_DYNFIELD(elts[pos + 2], offs,
							uint32_t *) = meta;
				*RTE_MBUF_DYNFIELD(elts[pos + 3], offs,
							uint32_t *) = meta;
			}
		}
		pos += MLX5_VPMD_DESCS_PER_LOOP;
		/* Move to next CQE and invalidate consumed CQEs. */
		if (!rxq->cqe_comp_layout) {
			if (!(pos & 0x7) && pos < mcqe_n) {
				if (pos + 8 < mcqe_n)
					rte_prefetch0((void *)(cq + pos + 8));
				mcq = (void *)&(cq + pos)->pkt_info;
				for (i = 0; i < 8; ++i)
					cq[inv++].op_own = MLX5_CQE_INVALIDATE;
			}
		}
	}
	if (rxq->cqe_comp_layout) {
		int ret;
		/* Keep unzipping if the next CQE is the miniCQE array. */
		cq = &cq[mcqe_n];
		ret = check_cqe_iteration(cq, rxq->cqe_n, rxq->cq_ci + pkts_n);
		if (ret == MLX5_CQE_STATUS_SW_OWN &&
		    MLX5_CQE_FORMAT(cq->op_own) == MLX5_COMPRESSED) {
			pos = 0;
			elts = &elts[mcqe_n];
			mcq = (void *)cq;
			mcqe_n = MLX5_CQE_NUM_MINIS(cq->op_own) + 1;
			pkts_n += mcqe_n;
			goto cycle;
		}
	} else {
		/* Invalidate the rest of CQEs. */
		for (; inv < pkts_n; ++inv)
			cq[inv].op_own = MLX5_CQE_INVALIDATE;
	}
#ifdef MLX5_PMD_SOFT_COUNTERS
	rxq->stats.ipackets += pkts_n;
	rxq->stats.ibytes += rcvd_byte;
#endif
	return pkts_n;
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
		vdupq_n_u32(rxq->rss_hash * RTE_MBUF_F_RX_RSS_HASH |
			    rxq->hw_timestamp * rxq->timestamp_rx_flag);
	const uint32x4_t ptype_ol_mask = { 0x106, 0x106, 0x106, 0x106 };
	const uint8x16_t cv_flag_sel = {
		0,
		(uint8_t)(RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED),
		(uint8_t)(RTE_MBUF_F_RX_IP_CKSUM_GOOD >> 1),
		0,
		(uint8_t)(RTE_MBUF_F_RX_L4_CKSUM_GOOD >> 1),
		0,
		(uint8_t)((RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1),
		0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	const uint32x4_t cv_mask =
		vdupq_n_u32(RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
			    RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED);
	const uint64x2_t mbuf_init = vld1q_u64
				((const uint64_t *)&rxq->mbuf_initializer);
	uint64x2_t rearm0, rearm1, rearm2, rearm3;
	uint8_t pt_idx0, pt_idx1, pt_idx2, pt_idx3;

	if (rxq->mark) {
		const uint32x4_t ft_def = vdupq_n_u32(MLX5_FLOW_MARK_DEFAULT);
		const uint32x4_t fdir_flags = vdupq_n_u32(RTE_MBUF_F_RX_FDIR);
		uint32x4_t fdir_id_flags = vdupq_n_u32(RTE_MBUF_F_RX_FDIR_ID);
		uint32x4_t invalid_mask;

		/* Check if flow tag is non-zero then set RTE_MBUF_F_RX_FDIR. */
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
	rearm0 = vreinterpretq_u64_u32(vsetq_lane_u32
					(vgetq_lane_u32(ol_flags, 3),
					 vreinterpretq_u32_u64(mbuf_init), 2));
	rearm1 = vreinterpretq_u64_u32(vsetq_lane_u32
					(vgetq_lane_u32(ol_flags, 2),
					 vreinterpretq_u32_u64(mbuf_init), 2));
	rearm2 = vreinterpretq_u64_u32(vsetq_lane_u32
					(vgetq_lane_u32(ol_flags, 1),
					 vreinterpretq_u32_u64(mbuf_init), 2));
	rearm3 = vreinterpretq_u64_u32(vsetq_lane_u32
					(vgetq_lane_u32(ol_flags, 0),
					 vreinterpretq_u32_u64(mbuf_init), 2));

	vst1q_u64((void *)&pkts[0]->rearm_data, rearm0);
	vst1q_u64((void *)&pkts[1]->rearm_data, rearm1);
	vst1q_u64((void *)&pkts[2]->rearm_data, rearm2);
	vst1q_u64((void *)&pkts[3]->rearm_data, rearm3);
}

/**
 * Process a non-compressed completion and fill in mbufs in RX SW ring
 * with data extracted from the title completion descriptor.
 *
 * @param rxq
 *   Pointer to RX queue structure.
 * @param cq
 *   Pointer to completion array having a non-compressed completion at first.
 * @param elts
 *   Pointer to SW ring to be filled. The first mbuf has to be pre-built from
 *   the title completion descriptor to be copied to the rest of mbufs.
 * @param[out] pkts
 *   Array to store received packets.
 * @param pkts_n
 *   Maximum number of packets in array.
 * @param[out] err
 *   Pointer to a flag. Set non-zero value if pkts array has at least one error
 *   packet to handle.
 * @param[out] comp
 *   Pointer to a index. Set it to the first compressed completion if any.
 *
 * @return
 *   Number of CQEs successfully processed.
 */
static inline uint16_t
rxq_cq_process_v(struct mlx5_rxq_data *rxq, volatile struct mlx5_cqe *cq,
		 struct rte_mbuf **elts, struct rte_mbuf **pkts,
		 uint16_t pkts_n, uint64_t *err, uint64_t *comp)
{
	const uint16_t q_n = 1 << rxq->cqe_n;
	const uint16_t q_mask = q_n - 1;
	unsigned int pos, adj;
	uint64_t n = 0;
	uint64_t comp_idx = MLX5_VPMD_DESCS_PER_LOOP;
	uint16_t nocmp_n = 0;
	const uint16x4_t validity = vdup_n_u16((rxq->cq_ci >> rxq->cqe_n) << 8);
	const uint16x4_t ownership = vdup_n_u16(!(rxq->cq_ci & (q_mask + 1)));
	const uint16x4_t vic_check = vcreate_u16(0xff00ff00ff00ff00);
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
		62,             /* validity_iteration_count */
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
		63, 51,         /* 4th CQE */
		47, 35,         /* 3rd CQE */
		31, 19,         /* 2nd CQE */
		15,  3          /* 1st CQE */
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
		0, 0, rxq->crc_present * RTE_ETHER_CRC_LEN, 0, 0, 0, 0, 0
	};
	const uint32x4_t flow_mark_adj = { 0, 0, 0, rxq->mark * (-1) };

	/*
	 * Note that vectors have reverse order - {v3, v2, v1, v0}, because
	 * there's no instruction to count trailing zeros. __builtin_clzl() is
	 * used instead.
	 *
	 * A. copy 4 mbuf pointers from elts ring to returning pkts.
	 * B. load 64B CQE and extract necessary fields
	 *    Final 16bytes cqes[] extracted from original 64bytes CQE has the
	 *    following structure:
	 *        struct {
	 *          uint16_t hdr_type_etc;
	 *          uint8_t  pkt_info;
	 *          uint8_t  validity_iteration_count;
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
		uint16x4_t comp_mask, mini_mask;
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
		rte_io_rmb();
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
		/* D.2 mask out CQEs belonging to HW. */
		if (rxq->cqe_comp_layout) {
			owner_mask = vand_u16(op_own, vic_check);
			owner_mask = vceq_u16(owner_mask, validity);
			owner_mask = vmvn_u16(owner_mask);
		} else {
			owner_mask = vand_u16(op_own, owner_check);
			owner_mask = vceq_u16(owner_mask, ownership);
		}
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
		invalid_mask = vorr_u16(invalid_mask, comp_mask);
		/* D.7 count non-compressed valid CQEs. */
		n = __builtin_clzl(vget_lane_u64(vreinterpret_u64_u16(
				   invalid_mask), 0)) / (sizeof(uint16_t) * 8);
		nocmp_n += n;
		/*
		 * D.2 mask out entries after the compressed CQE.
		 *     get the final invalid mask.
		 */
		mask = vcreate_u16(n < MLX5_VPMD_DESCS_PER_LOOP ?
				   -1UL >> (n * sizeof(uint16_t) * 8) : 0);
		invalid_mask = vorr_u16(invalid_mask, mask);
		/* D.3 check error in opcode. */
		adj = (!rxq->cqe_comp_layout &&
		       comp_idx != MLX5_VPMD_DESCS_PER_LOOP && comp_idx == n);
		mask = vcreate_u16(adj ?
			   -1UL >> ((n + 1) * sizeof(uint16_t) * 8) : -1UL);
		mini_mask = vand_u16(invalid_mask, mask);
		opcode = vceq_u16(resp_err_check, opcode);
		opcode = vbic_u16(opcode, mini_mask);
		/* D.4 mark if any error is set */
		*err |= vget_lane_u64(vreinterpret_u64_u16(opcode), 0);
		/* C.4 fill in mbuf - rearm_data and packet_type. */
		rxq_cq_to_ptype_oflags_v(rxq, ptype_info, flow_tag,
					 opcode, &elts[pos]);
		if (unlikely(rxq->shared)) {
			elts[pos]->port = container_of(p0, struct mlx5_cqe,
					      pkt_info)->user_index_low;
			elts[pos + 1]->port = container_of(p1, struct mlx5_cqe,
					      pkt_info)->user_index_low;
			elts[pos + 2]->port = container_of(p2, struct mlx5_cqe,
					      pkt_info)->user_index_low;
			elts[pos + 3]->port = container_of(p3, struct mlx5_cqe,
					      pkt_info)->user_index_low;
		}
		if (unlikely(rxq->hw_timestamp)) {
			int offset = rxq->timestamp_offset;
			if (rxq->rt_timestamp) {
				struct mlx5_dev_ctx_shared *sh = rxq->sh;
				uint64_t ts;

				ts = rte_be_to_cpu_64
					(container_of(p0, struct mlx5_cqe,
						      pkt_info)->timestamp);
				mlx5_timestamp_set(elts[pos], offset,
					mlx5_txpp_convert_rx_ts(sh, ts));
				ts = rte_be_to_cpu_64
					(container_of(p1, struct mlx5_cqe,
						      pkt_info)->timestamp);
				mlx5_timestamp_set(elts[pos + 1], offset,
					mlx5_txpp_convert_rx_ts(sh, ts));
				ts = rte_be_to_cpu_64
					(container_of(p2, struct mlx5_cqe,
						      pkt_info)->timestamp);
				mlx5_timestamp_set(elts[pos + 2], offset,
					mlx5_txpp_convert_rx_ts(sh, ts));
				ts = rte_be_to_cpu_64
					(container_of(p3, struct mlx5_cqe,
						      pkt_info)->timestamp);
				mlx5_timestamp_set(elts[pos + 3], offset,
					mlx5_txpp_convert_rx_ts(sh, ts));
			} else {
				mlx5_timestamp_set(elts[pos], offset,
					rte_be_to_cpu_64(container_of(p0,
					struct mlx5_cqe, pkt_info)->timestamp));
				mlx5_timestamp_set(elts[pos + 1], offset,
					rte_be_to_cpu_64(container_of(p1,
					struct mlx5_cqe, pkt_info)->timestamp));
				mlx5_timestamp_set(elts[pos + 2], offset,
					rte_be_to_cpu_64(container_of(p2,
					struct mlx5_cqe, pkt_info)->timestamp));
				mlx5_timestamp_set(elts[pos + 3], offset,
					rte_be_to_cpu_64(container_of(p3,
					struct mlx5_cqe, pkt_info)->timestamp));
			}
		}
		if (rxq->dynf_meta) {
			/* This code is subject for further optimization. */
			int32_t offs = rxq->flow_meta_offset;
			uint32_t mask = rxq->flow_meta_port_mask;

			*RTE_MBUF_DYNFIELD(pkts[pos], offs, uint32_t *) =
				rte_be_to_cpu_32(container_of
				(p0, struct mlx5_cqe,
				pkt_info)->flow_table_metadata) & mask;
			*RTE_MBUF_DYNFIELD(pkts[pos + 1], offs, uint32_t *) =
				rte_be_to_cpu_32(container_of
				(p1, struct mlx5_cqe,
				pkt_info)->flow_table_metadata) & mask;
			*RTE_MBUF_DYNFIELD(pkts[pos + 2], offs, uint32_t *) =
				rte_be_to_cpu_32(container_of
				(p2, struct mlx5_cqe,
				pkt_info)->flow_table_metadata) & mask;
			*RTE_MBUF_DYNFIELD(pkts[pos + 3], offs, uint32_t *) =
				rte_be_to_cpu_32(container_of
				(p3, struct mlx5_cqe,
				pkt_info)->flow_table_metadata) & mask;
			if (*RTE_MBUF_DYNFIELD(pkts[pos], offs, uint32_t *))
				elts[pos]->ol_flags |= rxq->flow_meta_mask;
			if (*RTE_MBUF_DYNFIELD(pkts[pos + 1], offs, uint32_t *))
				elts[pos + 1]->ol_flags |= rxq->flow_meta_mask;
			if (*RTE_MBUF_DYNFIELD(pkts[pos + 2], offs, uint32_t *))
				elts[pos + 2]->ol_flags |= rxq->flow_meta_mask;
			if (*RTE_MBUF_DYNFIELD(pkts[pos + 3], offs, uint32_t *))
				elts[pos + 3]->ol_flags |= rxq->flow_meta_mask;
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
#ifdef MLX5_PMD_SOFT_COUNTERS
	rxq->stats.ipackets += nocmp_n;
	rxq->stats.ibytes += rcvd_byte;
#endif
	if (comp_idx == n)
		*comp = comp_idx;
	return nocmp_n;
}

#endif /* RTE_PMD_MLX5_RXTX_VEC_NEON_H_ */
