/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_RXTX_VEC_ALTIVEC_H_
#define RTE_PMD_MLX5_RXTX_VEC_ALTIVEC_H_

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <rte_altivec.h>

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

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

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
		__vector unsigned char mbp;

		mbp = (__vector unsigned char)vec_vsx_ld(0,
				(signed int const *)&elts[pos]);
		*(__vector unsigned char *)&pkts[pos] = mbp;
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
	const __vector unsigned char zero = (__vector unsigned char){0};
	/* Mask to shuffle from extracted mini CQE to mbuf. */
	const __vector unsigned char shuf_mask1 = (__vector unsigned char){
			-1, -1, -1, -1,   /* skip packet_type */
			 7,  6, -1, -1,   /* bswap16, pkt_len */
			 7,  6,           /* bswap16, data_len */
			-1, -1,           /* skip vlan_tci */
			 3,  2,  1,  0};  /* bswap32, rss */
	const __vector unsigned char shuf_mask2 = (__vector unsigned char){
			-1, -1, -1, -1,   /* skip packet_type */
			15, 14, -1, -1,   /* bswap16, pkt_len */
			15, 14,           /* data_len, bswap16 */
			-1, -1,           /* skip vlan_tci */
			11, 10,  9,  8};  /* bswap32, rss */
	/* Restore the compressed count. Must be 16 bits. */
	uint16_t mcqe_n = (rxq->cqe_comp_layout) ?
		(MLX5_CQE_NUM_MINIS(cq->op_own) + 1) :
		t_pkt->data_len + (rxq->crc_present * RTE_ETHER_CRC_LEN);
	uint16_t pkts_n = mcqe_n;
	const __vector unsigned char rearm =
		(__vector unsigned char)vec_vsx_ld(0,
		(signed int const *)&t_pkt->rearm_data);
	const __vector unsigned char rxdf =
		(__vector unsigned char)vec_vsx_ld(0,
		(signed int const *)&t_pkt->rx_descriptor_fields1);
	const __vector unsigned char crc_adj =
		(__vector unsigned char)(__vector unsigned short){
			0, 0, rxq->crc_present * RTE_ETHER_CRC_LEN, 0,
			rxq->crc_present * RTE_ETHER_CRC_LEN, 0, 0, 0};
	const __vector unsigned short rxdf_sel_mask =
		(__vector unsigned short){
			0xffff, 0xffff, 0, 0, 0, 0xffff, 0, 0};
	__vector unsigned char ol_flags = (__vector unsigned char){0};
	__vector unsigned char ol_flags_mask = (__vector unsigned char){0};
	unsigned int pos;
	unsigned int i;
	unsigned int inv = 0;

#ifdef MLX5_PMD_SOFT_COUNTERS
	const __vector unsigned char ones = vec_splat_u8(-1);
	uint32_t rcvd_byte = 0;
	/* Mask to shuffle byte_cnt to add up stats. Do bswap16 for all. */
	const __vector unsigned char len_shuf_mask = (__vector unsigned char){
		 3,  2, 11, 10,
		 7,  6, 15, 14,
		-1, -1, -1, -1,
		-1, -1, -1, -1};
#endif

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
		__vector unsigned char mcqe1, mcqe2;
		__vector unsigned char rxdf1, rxdf2;
#ifdef MLX5_PMD_SOFT_COUNTERS
		const __vector unsigned short mcqe_sel_mask =
			(__vector unsigned short){0, 0, 0xffff, 0xffff,
			0, 0, 0xfff, 0xffff};
		const __vector unsigned char lower_half = {
			0, 1, 4, 5, 8, 9, 12, 13, 16,
			17, 20, 21, 24, 25, 28, 29};
		const __vector unsigned char upper_half = {
			2, 3, 6, 7, 10, 11, 14, 15,
			18, 19, 22, 23, 26, 27, 30, 31};
		__vector unsigned short left, right;
		__vector unsigned char byte_cnt, invalid_mask;
		__vector unsigned long lshift;
		__attribute__((altivec(vector__)))
			__attribute__((altivec(bool__)))
			unsigned long long shmask;
		const __vector unsigned long shmax = {64, 64};
#endif

		if (!rxq->cqe_comp_layout)
			for (i = 0; i < MLX5_VPMD_DESCS_PER_LOOP; ++i)
				if (likely(pos + i < mcqe_n))
					rte_prefetch0((void *)(cq + pos + i));
		/* A.1 load mCQEs into a 128bit register. */
		mcqe1 = (__vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&mcq[pos % 8]);
		mcqe2 = (__vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&mcq[pos % 8 + 2]);

		/* B.1 store rearm data to mbuf. */
		*(__vector unsigned char *)
			&elts[pos]->rearm_data = rearm;
		*(__vector unsigned char *)
			&elts[pos + 1]->rearm_data = rearm;

		/* C.1 combine data from mCQEs with rx_descriptor_fields1. */
		rxdf1 = vec_perm(mcqe1, zero, shuf_mask1);
		rxdf2 = vec_perm(mcqe1, zero, shuf_mask2);
		rxdf1 = (__vector unsigned char)
			((__vector unsigned short)rxdf1 -
			(__vector unsigned short)crc_adj);
		rxdf2 = (__vector unsigned char)
			((__vector unsigned short)rxdf2 -
			(__vector unsigned short)crc_adj);
		rxdf1 = (__vector unsigned char)
			vec_sel((__vector unsigned short)rxdf1,
			(__vector unsigned short)rxdf, rxdf_sel_mask);
		rxdf2 = (__vector unsigned char)
			vec_sel((__vector unsigned short)rxdf2,
			(__vector unsigned short)rxdf, rxdf_sel_mask);

		/* D.1 store rx_descriptor_fields1. */
		*(__vector unsigned char *)
			&elts[pos]->rx_descriptor_fields1 = rxdf1;
		*(__vector unsigned char *)
			&elts[pos + 1]->rx_descriptor_fields1 = rxdf2;

		/* B.1 store rearm data to mbuf. */
		*(__vector unsigned char *)
			&elts[pos + 2]->rearm_data = rearm;
		*(__vector unsigned char *)
			&elts[pos + 3]->rearm_data = rearm;

		/* C.1 combine data from mCQEs with rx_descriptor_fields1. */
		rxdf1 = vec_perm(mcqe2, zero, shuf_mask1);
		rxdf2 = vec_perm(mcqe2, zero, shuf_mask2);
		rxdf1 = (__vector unsigned char)
			((__vector unsigned short)rxdf1 -
			(__vector unsigned short)crc_adj);
		rxdf2 = (__vector unsigned char)
			((__vector unsigned short)rxdf2 -
			(__vector unsigned short)crc_adj);
		rxdf1 = (__vector unsigned char)
			vec_sel((__vector unsigned short)rxdf1,
			(__vector unsigned short)rxdf, rxdf_sel_mask);
		rxdf2 = (__vector unsigned char)
			vec_sel((__vector unsigned short)rxdf2,
			(__vector unsigned short)rxdf, rxdf_sel_mask);

		/* D.1 store rx_descriptor_fields1. */
		*(__vector unsigned char *)
			&elts[pos + 2]->rx_descriptor_fields1 = rxdf1;
		*(__vector unsigned char *)
			&elts[pos + 3]->rx_descriptor_fields1 = rxdf2;

#ifdef MLX5_PMD_SOFT_COUNTERS
		invalid_mask = (__vector unsigned char)(__vector unsigned long){
			(mcqe_n - pos) * sizeof(uint16_t) * 8, 0};

		lshift =
			vec_splat((__vector unsigned long)invalid_mask, 0);
		shmask = vec_cmpgt(shmax, lshift);
		invalid_mask = (__vector unsigned char)
			vec_sl((__vector unsigned long)ones, lshift);
		invalid_mask = (__vector unsigned char)
			vec_sel((__vector unsigned long)shmask,
			(__vector unsigned long)invalid_mask, shmask);

		byte_cnt = (__vector unsigned char)
			vec_sel((__vector unsigned short)
			vec_sro((__vector unsigned short)mcqe1,
			(__vector unsigned char){32}),
			(__vector unsigned short)mcqe2, mcqe_sel_mask);
		byte_cnt = vec_perm(byte_cnt, zero, len_shuf_mask);
		byte_cnt = (__vector unsigned char)
			vec_andc((__vector unsigned long)byte_cnt,
			(__vector unsigned long)invalid_mask);
		left = vec_perm((__vector unsigned short)byte_cnt,
			(__vector unsigned short)zero, lower_half);
		right = vec_perm((__vector unsigned short)byte_cnt,
			(__vector unsigned short)zero, upper_half);
		byte_cnt = (__vector unsigned char)vec_add(left, right);
		left = vec_perm((__vector unsigned short)byte_cnt,
			(__vector unsigned short)zero, lower_half);
		right = vec_perm((__vector unsigned short)byte_cnt,
			(__vector unsigned short)zero, upper_half);
		byte_cnt = (__vector unsigned char)vec_add(left, right);
		rcvd_byte += ((__vector unsigned long)byte_cnt)[0];
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
			} else {
				const __vector unsigned char flow_mark_adj =
					(__vector unsigned char)
					(__vector unsigned int){
					-1, -1, -1, -1};
				const __vector unsigned char flow_mark_shuf =
					(__vector unsigned char){
					-1, -1, -1, -1,
					-1, -1, -1, -1,
					12,  8,  9, -1,
					 4,  0,  1,  -1};
				const __vector unsigned char ft_mask =
					(__vector unsigned char)
					(__vector unsigned int){
					0xffffff00, 0xffffff00,
					0xffffff00, 0xffffff00};
				const __vector unsigned char fdir_flags =
					(__vector unsigned char)
					(__vector unsigned int){
					RTE_MBUF_F_RX_FDIR, RTE_MBUF_F_RX_FDIR,
					RTE_MBUF_F_RX_FDIR, RTE_MBUF_F_RX_FDIR};
				const __vector unsigned char fdir_all_flags =
					(__vector unsigned char)
					(__vector unsigned int){
					RTE_MBUF_F_RX_FDIR | rxq->mark_flag,
					RTE_MBUF_F_RX_FDIR | rxq->mark_flag,
					RTE_MBUF_F_RX_FDIR | rxq->mark_flag,
					RTE_MBUF_F_RX_FDIR | rxq->mark_flag};
				__vector unsigned char fdir_id_flags =
					(__vector unsigned char)
					(__vector unsigned int){
					rxq->mark_flag, rxq->mark_flag,
					rxq->mark_flag, rxq->mark_flag};
				/* Extract flow_tag field. */
				__vector unsigned char ftag0 = vec_perm(mcqe1,
							zero, flow_mark_shuf);
				__vector unsigned char ftag1 = vec_perm(mcqe2,
							zero, flow_mark_shuf);
				__vector unsigned char ftag =
					(__vector unsigned char)
					vec_mergel((__vector unsigned int)ftag0,
					(__vector unsigned int)ftag1);
				__vector unsigned char invalid_mask =
					(__vector unsigned char)
					vec_cmpeq((__vector unsigned int)ftag,
					(__vector unsigned int)zero);

				ol_flags_mask = (__vector unsigned char)
					vec_or((__vector unsigned long)
					ol_flags_mask,
					(__vector unsigned long)fdir_all_flags);

				/* Set RTE_MBUF_F_RX_FDIR if flow tag is non-zero. */
				invalid_mask = (__vector unsigned char)
					vec_cmpeq((__vector unsigned int)ftag,
					(__vector unsigned int)zero);
				ol_flags = (__vector unsigned char)
					vec_or((__vector unsigned long)ol_flags,
					(__vector unsigned long)
					vec_andc((__vector unsigned long)
					fdir_flags,
					(__vector unsigned long)invalid_mask));
				ol_flags_mask = (__vector unsigned char)
					vec_or((__vector unsigned long)
					ol_flags_mask,
					(__vector unsigned long)fdir_flags);

				/* Mask out invalid entries. */
				fdir_id_flags = (__vector unsigned char)
					vec_andc((__vector unsigned long)
					fdir_id_flags,
					(__vector unsigned long)invalid_mask);

				/* Check if flow tag MLX5_FLOW_MARK_DEFAULT. */
				ol_flags = (__vector unsigned char)
					vec_or((__vector unsigned long)ol_flags,
					(__vector unsigned long)
					vec_andc((__vector unsigned long)
					fdir_id_flags,
					(__vector unsigned long)
					vec_cmpeq((__vector unsigned int)ftag,
					(__vector unsigned int)ft_mask)));

				ftag = (__vector unsigned char)
					((__vector unsigned int)ftag +
					(__vector unsigned int)flow_mark_adj);
				elts[pos]->hash.fdir.hi =
					((__vector unsigned int)ftag)[0];
				elts[pos + 1]->hash.fdir.hi =
					((__vector unsigned int)ftag)[1];
				elts[pos + 2]->hash.fdir.hi =
					((__vector unsigned int)ftag)[2];
				elts[pos + 3]->hash.fdir.hi =
					((__vector unsigned int)ftag)[3];
			}
		}
		if (unlikely(rxq->mcqe_format != MLX5_CQE_RESP_FORMAT_HASH)) {
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
				const __vector unsigned char vlan_mask =
					(__vector unsigned char)
					(__vector unsigned int) {
					(RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED),
					(RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED),
					(RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED),
					(RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED)};
				const __vector unsigned char cv_mask =
					(__vector unsigned char)
					(__vector unsigned int) {
					MLX5_CQE_VLAN_STRIPPED,
					MLX5_CQE_VLAN_STRIPPED,
					MLX5_CQE_VLAN_STRIPPED,
					MLX5_CQE_VLAN_STRIPPED};
				__vector unsigned char pkt_cv =
					(__vector unsigned char)
					(__vector unsigned int) {
					pkt_hdr0 & 0x1, pkt_hdr1 & 0x1,
					pkt_hdr2 & 0x1, pkt_hdr3 & 0x1};

				ol_flags_mask = (__vector unsigned char)
					vec_or((__vector unsigned long)
					ol_flags_mask,
					(__vector unsigned long)vlan_mask);
				ol_flags = (__vector unsigned char)
					vec_or((__vector unsigned long)ol_flags,
					(__vector unsigned long)
					vec_and((__vector unsigned long)vlan_mask,
					(__vector unsigned long)
					vec_cmpeq((__vector unsigned int)pkt_cv,
					(__vector unsigned int)cv_mask)));
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
			const __vector unsigned char hash_mask =
				(__vector unsigned char)(__vector unsigned int) {
					RTE_MBUF_F_RX_RSS_HASH,
					RTE_MBUF_F_RX_RSS_HASH,
					RTE_MBUF_F_RX_RSS_HASH,
					RTE_MBUF_F_RX_RSS_HASH};
			const __vector unsigned char rearm_flags =
				(__vector unsigned char)(__vector unsigned int) {
				(uint32_t)t_pkt->ol_flags,
				(uint32_t)t_pkt->ol_flags,
				(uint32_t)t_pkt->ol_flags,
				(uint32_t)t_pkt->ol_flags};

			ol_flags_mask = (__vector unsigned char)
				vec_or((__vector unsigned long)ol_flags_mask,
				(__vector unsigned long)hash_mask);
			ol_flags = (__vector unsigned char)
				vec_or((__vector unsigned long)ol_flags,
				(__vector unsigned long)
				vec_andc((__vector unsigned long)rearm_flags,
				(__vector unsigned long)ol_flags_mask));

			elts[pos]->ol_flags =
				((__vector unsigned int)ol_flags)[0];
			elts[pos + 1]->ol_flags =
				((__vector unsigned int)ol_flags)[1];
			elts[pos + 2]->ol_flags =
				((__vector unsigned int)ol_flags)[2];
			elts[pos + 3]->ol_flags =
				((__vector unsigned int)ol_flags)[3];
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
 * @param cqes[4]
 *   Array of four 16bytes completions extracted from the original completion
 *   descriptor.
 * @param op_err
 *   Opcode vector having responder error status. Each field is 4B.
 * @param pkts
 *   Pointer to array of packets to be filled.
 */
static inline void
rxq_cq_to_ptype_oflags_v(struct mlx5_rxq_data *rxq,
		__vector unsigned char cqes[4], __vector unsigned char op_err,
		struct rte_mbuf **pkts)
{
	__vector unsigned char pinfo0, pinfo1;
	__vector unsigned char pinfo, ptype;
	__vector unsigned char ol_flags = (__vector unsigned char)
		(__vector unsigned int){
			rxq->rss_hash * RTE_MBUF_F_RX_RSS_HASH |
				rxq->hw_timestamp * rxq->timestamp_rx_flag,
			rxq->rss_hash * RTE_MBUF_F_RX_RSS_HASH |
				rxq->hw_timestamp * rxq->timestamp_rx_flag,
			rxq->rss_hash * RTE_MBUF_F_RX_RSS_HASH |
				rxq->hw_timestamp * rxq->timestamp_rx_flag,
			rxq->rss_hash * RTE_MBUF_F_RX_RSS_HASH |
				rxq->hw_timestamp * rxq->timestamp_rx_flag};
	__vector unsigned char cv_flags;
	const __vector unsigned char zero = (__vector unsigned char){0};
	const __vector unsigned char ptype_mask =
		(__vector unsigned char)(__vector unsigned int){
		0x0000fd06, 0x0000fd06, 0x0000fd06, 0x0000fd06};
	const __vector unsigned char ptype_ol_mask =
		(__vector unsigned char)(__vector unsigned int){
		0x00000106, 0x00000106, 0x00000106, 0x00000106};
	const __vector unsigned char pinfo_mask =
		(__vector unsigned char)(__vector unsigned int){
		0x00000003, 0x00000003, 0x00000003, 0x00000003};
	const __vector unsigned char cv_flag_sel = (__vector unsigned char){
		0, (uint8_t)(RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED),
		(uint8_t)(RTE_MBUF_F_RX_IP_CKSUM_GOOD >> 1), 0,
		(uint8_t)(RTE_MBUF_F_RX_L4_CKSUM_GOOD >> 1), 0,
		(uint8_t)((RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD) >> 1),
		0, 0, 0, 0, 0, 0, 0, 0, 0};
	const __vector unsigned char cv_mask =
		(__vector unsigned char)(__vector unsigned int){
		RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
		RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
		RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
		RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
		RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
		RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
		RTE_MBUF_F_RX_IP_CKSUM_GOOD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
		RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED};
	const __vector unsigned char mbuf_init =
		(__vector unsigned char)vec_vsx_ld
			(0, (__vector unsigned char *)&rxq->mbuf_initializer);
	const __vector unsigned short rearm_sel_mask =
		(__vector unsigned short){0, 0, 0, 0, 0xffff, 0xffff, 0, 0};
	__vector unsigned char rearm0, rearm1, rearm2, rearm3;
	uint8_t pt_idx0, pt_idx1, pt_idx2, pt_idx3;

	/* Extract pkt_info field. */
	pinfo0 = (__vector unsigned char)
		vec_mergeh((__vector unsigned int)cqes[0],
		(__vector unsigned int)cqes[1]);
	pinfo1 = (__vector unsigned char)
		vec_mergeh((__vector unsigned int)cqes[2],
		(__vector unsigned int)cqes[3]);
	pinfo = (__vector unsigned char)
		vec_mergeh((__vector unsigned long)pinfo0,
		(__vector unsigned long)pinfo1);

	/* Extract hdr_type_etc field. */
	pinfo0 = (__vector unsigned char)
		vec_mergel((__vector unsigned int)cqes[0],
		(__vector unsigned int)cqes[1]);
	pinfo1 = (__vector unsigned char)
		vec_mergel((__vector unsigned int)cqes[2],
		(__vector unsigned int)cqes[3]);
	ptype = (__vector unsigned char)
		vec_mergeh((__vector unsigned long)pinfo0,
		(__vector unsigned long)pinfo1);

	if (rxq->mark) {
		const __vector unsigned char pinfo_ft_mask =
			(__vector unsigned char)(__vector unsigned int){
			0xffffff00, 0xffffff00, 0xffffff00, 0xffffff00};
		const __vector unsigned char fdir_flags =
			(__vector unsigned char)(__vector unsigned int){
			RTE_MBUF_F_RX_FDIR, RTE_MBUF_F_RX_FDIR,
			RTE_MBUF_F_RX_FDIR, RTE_MBUF_F_RX_FDIR};
		__vector unsigned char fdir_id_flags =
			(__vector unsigned char)(__vector unsigned int){
			rxq->mark_flag, rxq->mark_flag,
			rxq->mark_flag, rxq->mark_flag};
		__vector unsigned char flow_tag, invalid_mask;

		flow_tag = (__vector unsigned char)
			vec_and((__vector unsigned long)pinfo,
			(__vector unsigned long)pinfo_ft_mask);

		/* Check if flow tag is non-zero then set RTE_MBUF_F_RX_FDIR. */
		invalid_mask = (__vector unsigned char)
			vec_cmpeq((__vector unsigned int)flow_tag,
			(__vector unsigned int)zero);
		ol_flags = (__vector unsigned char)
			vec_or((__vector unsigned long)ol_flags,
			(__vector unsigned long)
			vec_andc((__vector unsigned long)fdir_flags,
			(__vector unsigned long)invalid_mask));

		/* Mask out invalid entries. */
		fdir_id_flags = (__vector unsigned char)
			vec_andc((__vector unsigned long)fdir_id_flags,
			(__vector unsigned long)invalid_mask);

		/* Check if flow tag MLX5_FLOW_MARK_DEFAULT. */
		ol_flags = (__vector unsigned char)
			vec_or((__vector unsigned long)ol_flags,
			(__vector unsigned long)
			vec_andc((__vector unsigned long)fdir_id_flags,
			(__vector unsigned long)
			vec_cmpeq((__vector unsigned int)flow_tag,
			(__vector unsigned int)pinfo_ft_mask)));
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
	ptype = (__vector unsigned char)
		vec_and((__vector unsigned long)ptype,
		(__vector unsigned long)ptype_mask);
	pinfo = (__vector unsigned char)
		vec_and((__vector unsigned long)pinfo,
		(__vector unsigned long)pinfo_mask);
	pinfo = (__vector unsigned char)
		vec_sl((__vector unsigned int)pinfo,
		(__vector unsigned int){16, 16, 16, 16});

	/* Make pinfo has merged fields for ol_flags calculation. */
	pinfo = (__vector unsigned char)
		vec_or((__vector unsigned long)ptype,
		(__vector unsigned long)pinfo);
	ptype = (__vector unsigned char)
		vec_sr((__vector unsigned int)pinfo,
		(__vector unsigned int){10, 10, 10, 10});
	ptype = (__vector unsigned char)
		vec_packs((__vector unsigned int)ptype,
		(__vector unsigned int)zero);

	/* Errored packets will have RTE_PTYPE_ALL_MASK. */
	op_err = (__vector unsigned char)
		vec_sr((__vector unsigned short)op_err,
		(__vector unsigned short){8, 8, 8, 8, 8, 8, 8, 8});
	ptype = (__vector unsigned char)
		vec_or((__vector unsigned long)ptype,
		(__vector unsigned long)op_err);

	pt_idx0 = (uint8_t)((__vector unsigned char)ptype)[0];
	pt_idx1 = (uint8_t)((__vector unsigned char)ptype)[2];
	pt_idx2 = (uint8_t)((__vector unsigned char)ptype)[4];
	pt_idx3 = (uint8_t)((__vector unsigned char)ptype)[6];

	pkts[0]->packet_type = mlx5_ptype_table[pt_idx0] |
		!!(pt_idx0 & (1 << 6)) * rxq->tunnel;
	pkts[1]->packet_type = mlx5_ptype_table[pt_idx1] |
		!!(pt_idx1 & (1 << 6)) * rxq->tunnel;
	pkts[2]->packet_type = mlx5_ptype_table[pt_idx2] |
		!!(pt_idx2 & (1 << 6)) * rxq->tunnel;
	pkts[3]->packet_type = mlx5_ptype_table[pt_idx3] |
		!!(pt_idx3 & (1 << 6)) * rxq->tunnel;

	/* Fill flags for checksum and VLAN. */
	pinfo = (__vector unsigned char)
		vec_and((__vector unsigned long)pinfo,
		(__vector unsigned long)ptype_ol_mask);
	pinfo = vec_perm(cv_flag_sel, zero, pinfo);

	/* Locate checksum flags at byte[2:1] and merge with VLAN flags. */
	cv_flags = (__vector unsigned char)
		vec_sl((__vector unsigned int)pinfo,
		(__vector unsigned int){9, 9, 9, 9});
	cv_flags = (__vector unsigned char)
		vec_or((__vector unsigned long)pinfo,
		(__vector unsigned long)cv_flags);

	/* Move back flags to start from byte[0]. */
	cv_flags = (__vector unsigned char)
		vec_sr((__vector unsigned int)cv_flags,
		(__vector unsigned int){8, 8, 8, 8});

	/* Mask out garbage bits. */
	cv_flags = (__vector unsigned char)
		vec_and((__vector unsigned long)cv_flags,
		(__vector unsigned long)cv_mask);

	/* Merge to ol_flags. */
	ol_flags = (__vector unsigned char)
		vec_or((__vector unsigned long)ol_flags,
		(__vector unsigned long)cv_flags);

	/* Merge mbuf_init and ol_flags. */
	rearm0 = (__vector unsigned char)
		vec_sel((__vector unsigned short)mbuf_init,
		(__vector unsigned short)
		vec_slo((__vector unsigned short)ol_flags,
		(__vector unsigned char){64}), rearm_sel_mask);
	rearm1 = (__vector unsigned char)
		vec_sel((__vector unsigned short)mbuf_init,
		(__vector unsigned short)
		vec_slo((__vector unsigned short)ol_flags,
		(__vector unsigned char){32}), rearm_sel_mask);
	rearm2 = (__vector unsigned char)
		vec_sel((__vector unsigned short)mbuf_init,
		(__vector unsigned short)ol_flags, rearm_sel_mask);
	rearm3 = (__vector unsigned char)
		vec_sel((__vector unsigned short)mbuf_init,
		(__vector unsigned short)
		vec_sro((__vector unsigned short)ol_flags,
		(__vector unsigned char){32}), rearm_sel_mask);

	/* Write 8B rearm_data and 8B ol_flags. */
	vec_vsx_st(rearm0, 0,
		(__vector unsigned char *)&pkts[0]->rearm_data);
	vec_vsx_st(rearm1, 0,
		(__vector unsigned char *)&pkts[1]->rearm_data);
	vec_vsx_st(rearm2, 0,
		(__vector unsigned char *)&pkts[2]->rearm_data);
	vec_vsx_st(rearm3, 0,
		(__vector unsigned char *)&pkts[3]->rearm_data);
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
	const uint8_t vic = rxq->cq_ci >> rxq->cqe_n;
	unsigned int own = !(rxq->cq_ci & (q_mask + 1));
	const __vector unsigned char zero = (__vector unsigned char){0};
	const __vector unsigned char ones = vec_splat_u8(-1);
	const __vector unsigned char vic_check =
		(__vector unsigned char)(__vector unsigned long){
		0x00ff000000ff0000LL, 0x00ff000000ff0000LL};
	const __vector unsigned char owner_check =
		(__vector unsigned char)(__vector unsigned long){
		0x0100000001000000LL, 0x0100000001000000LL};
	const __vector unsigned char opcode_check =
		(__vector unsigned char)(__vector unsigned long){
		0xf0000000f0000000LL, 0xf0000000f0000000LL};
	const __vector unsigned char format_check =
		(__vector unsigned char)(__vector unsigned long){
		0x0c0000000c000000LL, 0x0c0000000c000000LL};
	const __vector unsigned char resp_err_check =
		(__vector unsigned char)(__vector unsigned long){
		0xe0000000e0000000LL, 0xe0000000e0000000LL};
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint32_t rcvd_byte = 0;
	/* Mask to shuffle byte_cnt to add up stats. Do bswap16 for all. */
	const __vector unsigned char len_shuf_mask = (__vector unsigned char){
		 1,  0,  5,  4,
		 9,  8, 13, 12,
		-1, -1, -1, -1,
		-1, -1, -1, -1};
#endif
	/* Mask to shuffle from extracted CQE to mbuf. */
	const __vector unsigned char shuf_mask = (__vector unsigned char){
		 5,  4,           /* bswap16, pkt_len */
		-1, -1,           /* zero out 2nd half of pkt_len */
		 5,  4,           /* bswap16, data_len */
		11, 10,           /* bswap16, vlan+tci */
		15, 14, 13, 12,   /* bswap32, rss */
		 1,  2,  3, -1};  /* fdir.hi */
	/* Mask to blend from the last Qword to the first DQword. */
	/* Mask to blend from the last Qword to the first DQword. */
	const __vector unsigned char blend_mask = (__vector unsigned char){
		-1,  0,  0,  0,
		 0,  0,  0,  0,
		-1, -1, -1, -1,
		-1, -1, -1, -1};
	const __vector unsigned char crc_adj =
		(__vector unsigned char)(__vector unsigned short){
		rxq->crc_present * RTE_ETHER_CRC_LEN, 0,
		rxq->crc_present * RTE_ETHER_CRC_LEN, 0, 0, 0, 0, 0};
	const __vector unsigned char flow_mark_adj =
		(__vector unsigned char)(__vector unsigned int){
		0, 0, 0, rxq->mark * (-1)};
	const __vector unsigned short cqe_sel_mask1 =
		(__vector unsigned short){0, 0, 0, 0, 0xffff, 0xffff, 0, 0};
	const __vector unsigned short cqe_sel_mask2 =
		(__vector unsigned short){0, 0, 0xffff, 0, 0, 0, 0, 0};
	const __vector unsigned char validity = (__vector unsigned char){
		0,  0,  vic,  0,
		0,  0,  vic,  0,
		0,  0,  vic,  0,
		0,  0,  vic,  0};
	const __vector unsigned char ownership = (__vector unsigned char){
		0,  0,  0,  own,
		0,  0,  0,  own,
		0,  0,  0,  own,
		0,  0,  0,  own};
	/*
	 * A. load first Qword (8bytes) in one loop.
	 * B. copy 4 mbuf pointers from elts ring to returning pkts.
	 * C. load remaining CQE data and extract necessary fields.
	 *    Final 16bytes cqes[] extracted from original 64bytes CQE has the
	 *    following structure:
	 *        struct {
	 *          uint8_t  pkt_info;
	 *          uint8_t  flow_tag[3];
	 *          uint16_t byte_cnt;
	 *          uint8_t  validity_iteration_count;
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
		__vector unsigned char cqes[MLX5_VPMD_DESCS_PER_LOOP];
		__vector unsigned char cqe_tmp1, cqe_tmp2;
		__vector unsigned char pkt_mb0, pkt_mb1, pkt_mb2, pkt_mb3;
		__vector unsigned char op_own, op_own_tmp1, op_own_tmp2;
		__vector unsigned char opcode, owner_mask, invalid_mask;
		__vector unsigned char comp_mask, mini_mask;
		__vector unsigned char mask;
#ifdef MLX5_PMD_SOFT_COUNTERS
		const __vector unsigned char lower_half = {
			0, 1, 4, 5, 8, 9, 12, 13,
			16, 17, 20, 21, 24, 25, 28, 29};
		const __vector unsigned char upper_half = {
			2, 3, 6, 7, 10, 11, 14, 15,
			18, 19, 22, 23, 26, 27, 30, 31};
		const __vector unsigned long shmax = {64, 64};
		__vector unsigned char byte_cnt;
		__vector unsigned short left, right;
		__vector unsigned long lshift;
		__vector __attribute__((altivec(bool__)))
			unsigned long shmask;
#endif
		__vector unsigned char mbp1, mbp2;
		__vector unsigned char p =
			(__vector unsigned char)(__vector unsigned short){
				0, 1, 2, 3, 0, 0, 0, 0};
		unsigned int p1, p2, p3;

		/* Prefetch next 4 CQEs. */
		if (pkts_n - pos >= 2 * MLX5_VPMD_DESCS_PER_LOOP) {
			rte_prefetch0(&cq[pos + MLX5_VPMD_DESCS_PER_LOOP]);
			rte_prefetch0(&cq[pos + MLX5_VPMD_DESCS_PER_LOOP + 1]);
			rte_prefetch0(&cq[pos + MLX5_VPMD_DESCS_PER_LOOP + 2]);
			rte_prefetch0(&cq[pos + MLX5_VPMD_DESCS_PER_LOOP + 3]);
		}

		/* A.0 do not cross the end of CQ. */
		mask = (__vector unsigned char)(__vector unsigned long){
			(pkts_n - pos) * sizeof(uint16_t) * 8, 0};

		{
			__vector unsigned long lshift;
			__vector __attribute__((altivec(bool__)))
				unsigned long shmask;
			const __vector unsigned long shmax = {64, 64};

			lshift = vec_splat((__vector unsigned long)mask, 0);
			shmask = vec_cmpgt(shmax, lshift);
			mask = (__vector unsigned char)
				vec_sl((__vector unsigned long)ones, lshift);
			mask = (__vector unsigned char)
				vec_sel((__vector unsigned long)shmask,
				(__vector unsigned long)mask, shmask);
		}

		p = (__vector unsigned char)
			vec_andc((__vector unsigned long)p,
			(__vector unsigned long)mask);

		/* A.1 load cqes. */
		p3 = (unsigned int)((__vector unsigned short)p)[3];
		cqes[3] = (__vector unsigned char)(__vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos + p3].sop_drop_qpn, 0LL};
		rte_compiler_barrier();

		p2 = (unsigned int)((__vector unsigned short)p)[2];
		cqes[2] = (__vector unsigned char)(__vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos + p2].sop_drop_qpn, 0LL};
		rte_compiler_barrier();

		/* B.1 load mbuf pointers. */
		mbp1 = (__vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&elts[pos]);
		mbp2 = (__vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&elts[pos + 2]);

		/* A.1 load a block having op_own. */
		p1 = (unsigned int)((__vector unsigned short)p)[1];
		cqes[1] = (__vector unsigned char)(__vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos + p1].sop_drop_qpn, 0LL};
		rte_compiler_barrier();

		cqes[0] = (__vector unsigned char)(__vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos].sop_drop_qpn, 0LL};
		rte_compiler_barrier();

		/* B.2 copy mbuf pointers. */
		*(__vector unsigned char *)&pkts[pos] = mbp1;
		*(__vector unsigned char *)&pkts[pos + 2] = mbp2;
		rte_io_rmb();

		/* C.1 load remaining CQE data and extract necessary fields. */
		cqe_tmp2 = *(__vector unsigned char *)
			&cq[pos + p3].pkt_info;
		cqe_tmp1 = *(__vector unsigned char *)
			&cq[pos + p2].pkt_info;
		cqes[3] = vec_sel(cqes[3], cqe_tmp2, blend_mask);
		cqes[2] = vec_sel(cqes[2], cqe_tmp1, blend_mask);
		cqe_tmp2 = (__vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&cq[pos + p3].csum);
		cqe_tmp1 = (__vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&cq[pos + p2].csum);
		cqes[3] = (__vector unsigned char)
			vec_sel((__vector unsigned short)cqes[3],
			(__vector unsigned short)cqe_tmp2, cqe_sel_mask1);
		cqes[2] = (__vector unsigned char)
			vec_sel((__vector unsigned short)cqes[2],
			(__vector unsigned short)cqe_tmp1, cqe_sel_mask1);
		cqe_tmp2 = (__vector unsigned char)(__vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos + p3].rsvd4[2], 0LL};
		cqe_tmp1 = (__vector unsigned char)(__vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos + p2].rsvd4[2], 0LL};
		cqes[3] = (__vector unsigned char)
			vec_sel((__vector unsigned short)cqes[3],
			(__vector unsigned short)cqe_tmp2,
			(__vector unsigned short)cqe_sel_mask2);
		cqes[2] = (__vector unsigned char)
			vec_sel((__vector unsigned short)cqes[2],
			(__vector unsigned short)cqe_tmp1,
			(__vector unsigned short)cqe_sel_mask2);

		/* C.2 generate final structure for mbuf with swapping bytes. */
		pkt_mb3 = vec_perm(cqes[3], zero, shuf_mask);
		pkt_mb2 = vec_perm(cqes[2], zero, shuf_mask);

		/* C.3 adjust CRC length. */
		pkt_mb3 = (__vector unsigned char)
			((__vector unsigned short)pkt_mb3 -
			(__vector unsigned short)crc_adj);
		pkt_mb2 = (__vector unsigned char)
			((__vector unsigned short)pkt_mb2 -
			(__vector unsigned short)crc_adj);

		/* C.4 adjust flow mark. */
		pkt_mb3 = (__vector unsigned char)
			((__vector unsigned int)pkt_mb3 +
			(__vector unsigned int)flow_mark_adj);
		pkt_mb2 = (__vector unsigned char)
			((__vector unsigned int)pkt_mb2 +
			(__vector unsigned int)flow_mark_adj);

		/* D.1 fill in mbuf - rx_descriptor_fields1. */
		*(__vector unsigned char *)
			&pkts[pos + 3]->pkt_len = pkt_mb3;
		*(__vector unsigned char *)
			&pkts[pos + 2]->pkt_len = pkt_mb2;

		/* E.1 extract op_own field. */
		op_own_tmp2 = (__vector unsigned char)
			vec_mergeh((__vector unsigned int)cqes[2],
			(__vector unsigned int)cqes[3]);

		/* C.1 load remaining CQE data and extract necessary fields. */
		cqe_tmp2 = *(__vector unsigned char *)
			&cq[pos + p1].pkt_info;
		cqe_tmp1 = *(__vector unsigned char *)
			&cq[pos].pkt_info;
		cqes[1] = vec_sel(cqes[1], cqe_tmp2, blend_mask);
		cqes[0] = vec_sel(cqes[0], cqe_tmp2, blend_mask);
		cqe_tmp2 = (__vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&cq[pos + p1].csum);
		cqe_tmp1 = (__vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&cq[pos].csum);
		cqes[1] = (__vector unsigned char)
			vec_sel((__vector unsigned short)cqes[1],
			(__vector unsigned short)cqe_tmp2, cqe_sel_mask1);
		cqes[0] = (__vector unsigned char)
			vec_sel((__vector unsigned short)cqes[0],
			(__vector unsigned short)cqe_tmp1, cqe_sel_mask1);
		cqe_tmp2 = (__vector unsigned char)(__vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos + p1].rsvd4[2], 0LL};
		cqe_tmp1 = (__vector unsigned char)(__vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos].rsvd4[2], 0LL};
		cqes[1] = (__vector unsigned char)
			vec_sel((__vector unsigned short)cqes[1],
			(__vector unsigned short)cqe_tmp2, cqe_sel_mask2);
		cqes[0] = (__vector unsigned char)
			vec_sel((__vector unsigned short)cqes[0],
			(__vector unsigned short)cqe_tmp1, cqe_sel_mask2);

		/* C.2 generate final structure for mbuf with swapping bytes. */
		pkt_mb1 = vec_perm(cqes[1], zero, shuf_mask);
		pkt_mb0 = vec_perm(cqes[0], zero, shuf_mask);

		/* C.3 adjust CRC length. */
		pkt_mb1 = (__vector unsigned char)
			((__vector unsigned short)pkt_mb1 -
			(__vector unsigned short)crc_adj);
		pkt_mb0 = (__vector unsigned char)
			((__vector unsigned short)pkt_mb0 -
			(__vector unsigned short)crc_adj);

		/* C.4 adjust flow mark. */
		pkt_mb1 = (__vector unsigned char)
			((__vector unsigned int)pkt_mb1 +
			(__vector unsigned int)flow_mark_adj);
		pkt_mb0 = (__vector unsigned char)
			((__vector unsigned int)pkt_mb0 +
			(__vector unsigned int)flow_mark_adj);

		/* E.1 extract op_own byte. */
		op_own_tmp1 = (__vector unsigned char)
			vec_mergeh((__vector unsigned int)cqes[0],
			(__vector unsigned int)cqes[1]);
		op_own = (__vector unsigned char)
			vec_mergel((__vector unsigned long)op_own_tmp1,
			(__vector unsigned long)op_own_tmp2);

		/* D.1 fill in mbuf - rx_descriptor_fields1. */
		*(__vector unsigned char *)
			&pkts[pos + 1]->pkt_len = pkt_mb1;
		*(__vector unsigned char *)
			&pkts[pos]->pkt_len = pkt_mb0;

		/* E.2 mask out CQEs belonging to HW. */
		if (rxq->cqe_comp_layout) {
			owner_mask = (__vector unsigned char)
				vec_and((__vector unsigned long)op_own,
				(__vector unsigned long)vic_check);
			owner_mask = (__vector unsigned char)
				vec_cmpeq((__vector unsigned int)owner_mask,
				(__vector unsigned int)validity);
			owner_mask = (__vector unsigned char)
				vec_xor((__vector unsigned long)owner_mask,
				(__vector unsigned long)ones);
		} else {
			owner_mask = (__vector unsigned char)
				vec_and((__vector unsigned long)op_own,
				(__vector unsigned long)owner_check);
			owner_mask = (__vector unsigned char)
				vec_cmpeq((__vector unsigned int)owner_mask,
				(__vector unsigned int)ownership);
		}
		owner_mask = (__vector unsigned char)
			vec_packs((__vector unsigned int)owner_mask,
			(__vector unsigned int)zero);

		/* E.3 get mask for invalidated CQEs. */
		opcode = (__vector unsigned char)
			vec_and((__vector unsigned long)op_own,
			(__vector unsigned long)opcode_check);
		invalid_mask = (__vector unsigned char)
			vec_cmpeq((__vector unsigned int)opcode_check,
			(__vector unsigned int)opcode);
		invalid_mask = (__vector unsigned char)
			vec_packs((__vector unsigned int)invalid_mask,
			(__vector unsigned int)zero);

		/* E.4 mask out beyond boundary. */
		invalid_mask = (__vector unsigned char)
			vec_or((__vector unsigned long)invalid_mask,
			(__vector unsigned long)mask);

		/* E.5 merge invalid_mask with invalid owner. */
		invalid_mask = (__vector unsigned char)
			vec_or((__vector unsigned long)invalid_mask,
			(__vector unsigned long)owner_mask);

		/* F.1 find compressed CQE format. */
		comp_mask = (__vector unsigned char)
			vec_and((__vector unsigned long)op_own,
			(__vector unsigned long)format_check);
		comp_mask = (__vector unsigned char)
			vec_cmpeq((__vector unsigned int)comp_mask,
			(__vector unsigned int)format_check);
		comp_mask = (__vector unsigned char)
			vec_packs((__vector unsigned int)comp_mask,
			(__vector unsigned int)zero);

		/* F.2 mask out invalid entries. */
		comp_mask = (__vector unsigned char)
			vec_andc((__vector unsigned long)comp_mask,
			(__vector unsigned long)invalid_mask);
		comp_idx = ((__vector unsigned long)comp_mask)[0];

		/* F.3 get the first compressed CQE. */
		comp_idx = comp_idx ? __builtin_ctzll(comp_idx) /
			(sizeof(uint16_t) * 8) : MLX5_VPMD_DESCS_PER_LOOP;

		/* E.6 mask out entries after the compressed CQE. */
		mask = (__vector unsigned char)(__vector unsigned long){
			(comp_idx * sizeof(uint16_t) * 8), 0};
		lshift = vec_splat((__vector unsigned long)mask, 0);
		shmask = vec_cmpgt(shmax, lshift);
		mask = (__vector unsigned char)
			vec_sl((__vector unsigned long)ones, lshift);
		mask = (__vector unsigned char)
			vec_sel((__vector unsigned long)shmask,
			(__vector unsigned long)mask, shmask);
		invalid_mask = (__vector unsigned char)
			vec_or((__vector unsigned long)invalid_mask,
			(__vector unsigned long)mask);

		/* E.7 count non-compressed valid CQEs. */
		n = ((__vector unsigned long)invalid_mask)[0];
		n = n ? __builtin_ctzll(n) / (sizeof(uint16_t) * 8) :
			MLX5_VPMD_DESCS_PER_LOOP;
		nocmp_n += n;

		/* D.2 get the final invalid mask. */
		mask = (__vector unsigned char)(__vector unsigned long){
			(n * sizeof(uint16_t) * 8), 0};
		lshift = vec_splat((__vector unsigned long)mask, 0);
		shmask = vec_cmpgt(shmax, lshift);
		mask = (__vector unsigned char)
			vec_sl((__vector unsigned long)ones, lshift);
		mask = (__vector unsigned char)
			vec_sel((__vector unsigned long)shmask,
			(__vector unsigned long)mask, shmask);
		invalid_mask = (__vector unsigned char)
			vec_or((__vector unsigned long)invalid_mask,
			(__vector unsigned long)mask);

		/* D.3 check error in opcode. */
		adj = (!rxq->cqe_comp_layout &&
		       comp_idx != MLX5_VPMD_DESCS_PER_LOOP && comp_idx == n);
		mask = (__vector unsigned char)(__vector unsigned long){
			(adj * sizeof(uint16_t) * 8), 0};
		lshift = vec_splat((__vector unsigned long)mask, 0);
		shmask = vec_cmpgt(shmax, lshift);
		mini_mask = (__vector unsigned char)
			vec_sl((__vector unsigned long)invalid_mask, lshift);
		mini_mask = (__vector unsigned char)
			vec_sel((__vector unsigned long)shmask,
			(__vector unsigned long)mini_mask, shmask);
		opcode = (__vector unsigned char)
			vec_cmpeq((__vector unsigned int)resp_err_check,
			(__vector unsigned int)opcode);
		opcode = (__vector unsigned char)
			vec_packs((__vector unsigned int)opcode,
			(__vector unsigned int)zero);
		opcode = (__vector unsigned char)
			vec_andc((__vector unsigned long)opcode,
			(__vector unsigned long)mini_mask);

		/* D.4 mark if any error is set */
		*err |= ((__vector unsigned long)opcode)[0];

		/* D.5 fill in mbuf - rearm_data and packet_type. */
		rxq_cq_to_ptype_oflags_v(rxq, cqes, opcode, &pkts[pos]);
		if (unlikely(rxq->shared)) {
			pkts[pos]->port = cq[pos].user_index_low;
			pkts[pos + p1]->port = cq[pos + p1].user_index_low;
			pkts[pos + p2]->port = cq[pos + p2].user_index_low;
			pkts[pos + p3]->port = cq[pos + p3].user_index_low;
		}
		if (rxq->hw_timestamp) {
			int offset = rxq->timestamp_offset;
			if (rxq->rt_timestamp) {
				struct mlx5_dev_ctx_shared *sh = rxq->sh;
				uint64_t ts;

				ts = rte_be_to_cpu_64(cq[pos].timestamp);
				mlx5_timestamp_set(pkts[pos], offset,
					mlx5_txpp_convert_rx_ts(sh, ts));
				ts = rte_be_to_cpu_64(cq[pos + p1].timestamp);
				mlx5_timestamp_set(pkts[pos + 1], offset,
					mlx5_txpp_convert_rx_ts(sh, ts));
				ts = rte_be_to_cpu_64(cq[pos + p2].timestamp);
				mlx5_timestamp_set(pkts[pos + 2], offset,
					mlx5_txpp_convert_rx_ts(sh, ts));
				ts = rte_be_to_cpu_64(cq[pos + p3].timestamp);
				mlx5_timestamp_set(pkts[pos + 3], offset,
					mlx5_txpp_convert_rx_ts(sh, ts));
			} else {
				mlx5_timestamp_set(pkts[pos], offset,
					rte_be_to_cpu_64(cq[pos].timestamp));
				mlx5_timestamp_set(pkts[pos + 1], offset,
					rte_be_to_cpu_64(cq[pos + p1].timestamp));
				mlx5_timestamp_set(pkts[pos + 2], offset,
					rte_be_to_cpu_64(cq[pos + p2].timestamp));
				mlx5_timestamp_set(pkts[pos + 3], offset,
					rte_be_to_cpu_64(cq[pos + p3].timestamp));
			}
		}
		if (rxq->dynf_meta) {
			uint64_t flag = rxq->flow_meta_mask;
			int32_t offs = rxq->flow_meta_offset;
			uint32_t mask = rxq->flow_meta_port_mask;
			uint32_t metadata;

			/* This code is subject for further optimization. */
			metadata = rte_be_to_cpu_32
				(cq[pos].flow_table_metadata) & mask;
			*RTE_MBUF_DYNFIELD(pkts[pos], offs, uint32_t *) =
								metadata;
			pkts[pos]->ol_flags |= metadata ? flag : 0ULL;
			metadata = rte_be_to_cpu_32
				(cq[pos + 1].flow_table_metadata) & mask;
			*RTE_MBUF_DYNFIELD(pkts[pos + 1], offs, uint32_t *) =
								metadata;
			pkts[pos + 1]->ol_flags |= metadata ? flag : 0ULL;
			metadata = rte_be_to_cpu_32
				(cq[pos + 2].flow_table_metadata) &	mask;
			*RTE_MBUF_DYNFIELD(pkts[pos + 2], offs, uint32_t *) =
								metadata;
			pkts[pos + 2]->ol_flags |= metadata ? flag : 0ULL;
			metadata = rte_be_to_cpu_32
				(cq[pos + 3].flow_table_metadata) &	mask;
			*RTE_MBUF_DYNFIELD(pkts[pos + 3], offs, uint32_t *) =
								metadata;
			pkts[pos + 3]->ol_flags |= metadata ? flag : 0ULL;
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Add up received bytes count. */
		byte_cnt = vec_perm(op_own, zero, len_shuf_mask);
		byte_cnt = (__vector unsigned char)
			vec_andc((__vector unsigned long)byte_cnt,
			(__vector unsigned long)invalid_mask);
		left = vec_perm((__vector unsigned short)byte_cnt,
			(__vector unsigned short)zero, lower_half);
		right = vec_perm((__vector unsigned short)byte_cnt,
			(__vector unsigned short)zero, upper_half);
		byte_cnt = (__vector unsigned char)vec_add(left, right);
		left = vec_perm((__vector unsigned short)byte_cnt,
			(__vector unsigned short)zero, lower_half);
		right = vec_perm((__vector unsigned short)byte_cnt,
			(__vector unsigned short)zero, upper_half);
		byte_cnt = (__vector unsigned char)vec_add(left, right);
		rcvd_byte += ((__vector unsigned long)byte_cnt)[0];
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

#endif /* RTE_PMD_MLX5_RXTX_VEC_ALTIVEC_H_ */
