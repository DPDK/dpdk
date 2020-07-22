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
		vector unsigned char mbp;

		mbp = (vector unsigned char)vec_vsx_ld(0,
				(signed int const *)&elts[pos]);
		*(vector unsigned char *)&pkts[pos] = mbp;
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
	volatile struct mlx5_mini_cqe8 *mcq = (void *)&(cq + 1)->pkt_info;
	struct rte_mbuf *t_pkt = elts[0]; /* Title packet is pre-built. */
	const vector unsigned char zero = (vector unsigned char){0};
	/* Mask to shuffle from extracted mini CQE to mbuf. */
	const vector unsigned char shuf_mask1 = (vector unsigned char){
			-1, -1, -1, -1,   /* skip packet_type */
			 7,  6, -1, -1,   /* bswap16, pkt_len */
			 7,  6,           /* bswap16, data_len */
			-1, -1,           /* skip vlan_tci */
			 3,  2,  1,  0};  /* bswap32, rss */
	const vector unsigned char shuf_mask2 = (vector unsigned char){
			-1, -1, -1, -1,   /* skip packet_type */
			15, 14, -1, -1,   /* bswap16, pkt_len */
			15, 14,           /* data_len, bswap16 */
			-1, -1,           /* skip vlan_tci */
			11, 10,  9,  8};  /* bswap32, rss */
	/* Restore the compressed count. Must be 16 bits. */
	const uint16_t mcqe_n = t_pkt->data_len +
		(rxq->crc_present * RTE_ETHER_CRC_LEN);
	const vector unsigned char rearm =
		(vector unsigned char)vec_vsx_ld(0,
		(signed int const *)&t_pkt->rearm_data);
	const vector unsigned char rxdf =
		(vector unsigned char)vec_vsx_ld(0,
		(signed int const *)&t_pkt->rx_descriptor_fields1);
	const vector unsigned char crc_adj =
		(vector unsigned char)(vector unsigned short){
			0, 0, rxq->crc_present * RTE_ETHER_CRC_LEN, 0,
			rxq->crc_present * RTE_ETHER_CRC_LEN, 0, 0, 0};
	const vector unsigned short rxdf_sel_mask =
		(vector unsigned short){
			0xffff, 0xffff, 0, 0, 0, 0xffff, 0, 0};
	const uint32_t flow_tag = t_pkt->hash.fdir.hi;
	unsigned int pos;
	unsigned int i;
	unsigned int inv = 0;

#ifdef MLX5_PMD_SOFT_COUNTERS
	const vector unsigned char ones = vec_splat_u8(-1);
	uint32_t rcvd_byte = 0;
	/* Mask to shuffle byte_cnt to add up stats. Do bswap16 for all. */
	const vector unsigned char len_shuf_mask = (vector unsigned char){
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
	for (pos = 0; pos < mcqe_n; ) {
		vector unsigned char mcqe1, mcqe2;
		vector unsigned char rxdf1, rxdf2;
#ifdef MLX5_PMD_SOFT_COUNTERS
		const vector unsigned short mcqe_sel_mask =
			(vector unsigned short){0, 0, 0xffff, 0xffff,
			0, 0, 0xfff, 0xffff};
		const vector unsigned char lower_half = {
			0, 1, 4, 5, 8, 9, 12, 13, 16,
			17, 20, 21, 24, 25, 28, 29};
		const vector unsigned char upper_half = {
			2, 3, 6, 7, 10, 11, 14, 15,
			18, 19, 22, 23, 26, 27, 30, 31};
		vector unsigned short left, right;
		vector unsigned char byte_cnt, invalid_mask;
		vector unsigned long lshift;
		__attribute__((altivec(vector__)))
			__attribute__((altivec(bool__)))
			unsigned long long shmask;
		const vector unsigned long shmax = {64, 64};
#endif

		for (i = 0; i < MLX5_VPMD_DESCS_PER_LOOP; ++i)
			if (likely(pos + i < mcqe_n))
				rte_prefetch0((void *)(cq + pos + i));
		/* A.1 load mCQEs into a 128bit register. */
		mcqe1 = (vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&mcq[pos % 8]);
		mcqe2 = (vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&mcq[pos % 8 + 2]);

		/* B.1 store rearm data to mbuf. */
		*(vector unsigned char *)
			&elts[pos]->rearm_data = rearm;
		*(vector unsigned char *)
			&elts[pos + 1]->rearm_data = rearm;

		/* C.1 combine data from mCQEs with rx_descriptor_fields1. */
		rxdf1 = vec_perm(mcqe1, zero, shuf_mask1);
		rxdf2 = vec_perm(mcqe1, zero, shuf_mask2);
		rxdf1 = (vector unsigned char)
			((vector unsigned short)rxdf1 -
			(vector unsigned short)crc_adj);
		rxdf2 = (vector unsigned char)
			((vector unsigned short)rxdf2 -
			(vector unsigned short)crc_adj);
		rxdf1 = (vector unsigned char)
			vec_sel((vector unsigned short)rxdf1,
			(vector unsigned short)rxdf, rxdf_sel_mask);
		rxdf2 = (vector unsigned char)
			vec_sel((vector unsigned short)rxdf2,
			(vector unsigned short)rxdf, rxdf_sel_mask);

		/* D.1 store rx_descriptor_fields1. */
		*(vector unsigned char *)
			&elts[pos]->rx_descriptor_fields1 = rxdf1;
		*(vector unsigned char *)
			&elts[pos + 1]->rx_descriptor_fields1 = rxdf2;

		/* B.1 store rearm data to mbuf. */
		*(vector unsigned char *)
			&elts[pos + 2]->rearm_data = rearm;
		*(vector unsigned char *)
			&elts[pos + 3]->rearm_data = rearm;

		/* C.1 combine data from mCQEs with rx_descriptor_fields1. */
		rxdf1 = vec_perm(mcqe2, zero, shuf_mask1);
		rxdf2 = vec_perm(mcqe2, zero, shuf_mask2);
		rxdf1 = (vector unsigned char)
			((vector unsigned short)rxdf1 -
			(vector unsigned short)crc_adj);
		rxdf2 = (vector unsigned char)
			((vector unsigned short)rxdf2 -
			(vector unsigned short)crc_adj);
		rxdf1 = (vector unsigned char)
			vec_sel((vector unsigned short)rxdf1,
			(vector unsigned short)rxdf, rxdf_sel_mask);
		rxdf2 = (vector unsigned char)
			vec_sel((vector unsigned short)rxdf2,
			(vector unsigned short)rxdf, rxdf_sel_mask);

		/* D.1 store rx_descriptor_fields1. */
		*(vector unsigned char *)
			&elts[pos + 2]->rx_descriptor_fields1 = rxdf1;
		*(vector unsigned char *)
			&elts[pos + 3]->rx_descriptor_fields1 = rxdf2;

#ifdef MLX5_PMD_SOFT_COUNTERS
		invalid_mask = (vector unsigned char)(vector unsigned long){
			(mcqe_n - pos) * sizeof(uint16_t) * 8, 0};

		lshift =
			vec_splat((vector unsigned long)invalid_mask, 0);
		shmask = vec_cmpgt(shmax, lshift);
		invalid_mask = (vector unsigned char)
			vec_sl((vector unsigned long)ones, lshift);
		invalid_mask = (vector unsigned char)
			vec_sel((vector unsigned long)shmask,
			(vector unsigned long)invalid_mask, shmask);

		mcqe1 = (vector unsigned char)
			vec_sro((vector unsigned short)mcqe1,
			(vector unsigned char){32}),
		byte_cnt = (vector unsigned char)
			vec_sel((vector unsigned short)mcqe1,
			(vector unsigned short)mcqe2, mcqe_sel_mask);
		byte_cnt = vec_perm(byte_cnt, zero, len_shuf_mask);
		byte_cnt = (vector unsigned char)
			vec_andc((vector unsigned long)byte_cnt,
			(vector unsigned long)invalid_mask);
		left = vec_perm((vector unsigned short)byte_cnt,
			(vector unsigned short)zero, lower_half);
		right = vec_perm((vector unsigned short)byte_cnt,
			(vector unsigned short)zero, upper_half);
		byte_cnt = (vector unsigned char)vec_add(left, right);
		left = vec_perm((vector unsigned short)byte_cnt,
			(vector unsigned short)zero, lower_half);
		right = vec_perm((vector unsigned short)byte_cnt,
			(vector unsigned short)zero, upper_half);
		byte_cnt = (vector unsigned char)vec_add(left, right);
		rcvd_byte += ((vector unsigned long)byte_cnt)[0];
#endif

		if (rxq->mark) {
			/* E.1 store flow tag (rte_flow mark). */
			elts[pos]->hash.fdir.hi = flow_tag;
			elts[pos + 1]->hash.fdir.hi = flow_tag;
			elts[pos + 2]->hash.fdir.hi = flow_tag;
			elts[pos + 3]->hash.fdir.hi = flow_tag;
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
		if (!(pos & 0x7) && pos < mcqe_n) {
			if (pos + 8 < mcqe_n)
				rte_prefetch0((void *)(cq + pos + 8));
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
	return mcqe_n;
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
		vector unsigned char cqes[4], vector unsigned char op_err,
		struct rte_mbuf **pkts)
{
	vector unsigned char pinfo0, pinfo1;
	vector unsigned char pinfo, ptype;
	vector unsigned char ol_flags = (vector unsigned char)
		(vector unsigned int){
			rxq->rss_hash * PKT_RX_RSS_HASH |
				rxq->hw_timestamp * PKT_RX_TIMESTAMP,
			rxq->rss_hash * PKT_RX_RSS_HASH |
				rxq->hw_timestamp * PKT_RX_TIMESTAMP,
			rxq->rss_hash * PKT_RX_RSS_HASH |
				rxq->hw_timestamp * PKT_RX_TIMESTAMP,
			rxq->rss_hash * PKT_RX_RSS_HASH |
				rxq->hw_timestamp * PKT_RX_TIMESTAMP};
	vector unsigned char cv_flags;
	const vector unsigned char zero = (vector unsigned char){0};
	const vector unsigned char ptype_mask =
		(vector unsigned char)(vector unsigned int){
		0x0000fd06, 0x0000fd06, 0x0000fd06, 0x0000fd06};
	const vector unsigned char ptype_ol_mask =
		(vector unsigned char)(vector unsigned int){
		0x00000106, 0x00000106, 0x00000106, 0x00000106};
	const vector unsigned char pinfo_mask =
		(vector unsigned char)(vector unsigned int){
		0x00000003, 0x00000003, 0x00000003, 0x00000003};
	const vector unsigned char cv_flag_sel = (vector unsigned char){
		0, (uint8_t)(PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED),
		(uint8_t)(PKT_RX_IP_CKSUM_GOOD >> 1), 0,
		(uint8_t)(PKT_RX_L4_CKSUM_GOOD >> 1), 0,
		(uint8_t)((PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD) >> 1),
		0, 0, 0, 0, 0, 0, 0, 0, 0};
	const vector unsigned char cv_mask =
		(vector unsigned char)(vector unsigned int){
		PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD |
		PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED,
		PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD |
		PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED,
		PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD |
		PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED,
		PKT_RX_IP_CKSUM_GOOD | PKT_RX_L4_CKSUM_GOOD |
		PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED};
	const vector unsigned char mbuf_init =
		(vector unsigned char)vec_vsx_ld
			(0, (vector unsigned char *)&rxq->mbuf_initializer);
	const vector unsigned short rearm_sel_mask =
		(vector unsigned short){0, 0, 0, 0, 0xffff, 0xffff, 0, 0};
	vector unsigned char rearm0, rearm1, rearm2, rearm3;
	uint8_t pt_idx0, pt_idx1, pt_idx2, pt_idx3;

	/* Extract pkt_info field. */
	pinfo0 = (vector unsigned char)
		vec_mergeh((vector unsigned int)cqes[0],
		(vector unsigned int)cqes[1]);
	pinfo1 = (vector unsigned char)
		vec_mergeh((vector unsigned int)cqes[2],
		(vector unsigned int)cqes[3]);
	pinfo = (vector unsigned char)
		vec_mergeh((vector unsigned long)pinfo0,
		(vector unsigned long)pinfo1);

	/* Extract hdr_type_etc field. */
	pinfo0 = (vector unsigned char)
		vec_mergel((vector unsigned int)cqes[0],
		(vector unsigned int)cqes[1]);
	pinfo1 = (vector unsigned char)
		vec_mergel((vector unsigned int)cqes[2],
		(vector unsigned int)cqes[3]);
	ptype = (vector unsigned char)
		vec_mergeh((vector unsigned long)pinfo0,
		(vector unsigned long)pinfo1);

	if (rxq->mark) {
		const vector unsigned char pinfo_ft_mask =
			(vector unsigned char)(vector unsigned int){
			0xffffff00, 0xffffff00, 0xffffff00, 0xffffff00};
		const vector unsigned char fdir_flags =
			(vector unsigned char)(vector unsigned int){
			PKT_RX_FDIR, PKT_RX_FDIR,
			PKT_RX_FDIR, PKT_RX_FDIR};
		vector unsigned char fdir_id_flags =
			(vector unsigned char)(vector unsigned int){
			PKT_RX_FDIR_ID, PKT_RX_FDIR_ID,
			PKT_RX_FDIR_ID, PKT_RX_FDIR_ID};
		vector unsigned char flow_tag, invalid_mask;

		flow_tag = (vector unsigned char)
			vec_and((vector unsigned long)pinfo,
			(vector unsigned long)pinfo_ft_mask);

		/* Check if flow tag is non-zero then set PKT_RX_FDIR. */
		invalid_mask = (vector unsigned char)
			vec_cmpeq((vector unsigned int)flow_tag,
			(vector unsigned int)zero);
		ol_flags = (vector unsigned char)
			vec_or((vector unsigned long)ol_flags,
			(vector unsigned long)
			vec_andc((vector unsigned long)fdir_flags,
			(vector unsigned long)invalid_mask));

		/* Mask out invalid entries. */
		fdir_id_flags = (vector unsigned char)
			vec_andc((vector unsigned long)fdir_id_flags,
			(vector unsigned long)invalid_mask);

		/* Check if flow tag MLX5_FLOW_MARK_DEFAULT. */
		ol_flags = (vector unsigned char)
			vec_or((vector unsigned long)ol_flags,
			(vector unsigned long)
			vec_andc((vector unsigned long)fdir_id_flags,
			(vector unsigned long)
			vec_cmpeq((vector unsigned int)flow_tag,
			(vector unsigned int)pinfo_ft_mask)));
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
	ptype = (vector unsigned char)
		vec_and((vector unsigned long)ptype,
		(vector unsigned long)ptype_mask);
	pinfo = (vector unsigned char)
		vec_and((vector unsigned long)pinfo,
		(vector unsigned long)pinfo_mask);
	pinfo = (vector unsigned char)
		vec_sl((vector unsigned int)pinfo,
		(vector unsigned int){16, 16, 16, 16});

	/* Make pinfo has merged fields for ol_flags calculation. */
	pinfo = (vector unsigned char)
		vec_or((vector unsigned long)ptype,
		(vector unsigned long)pinfo);
	ptype = (vector unsigned char)
		vec_sr((vector unsigned int)pinfo,
		(vector unsigned int){10, 10, 10, 10});
	ptype = (vector unsigned char)
		vec_packs((vector unsigned int)ptype,
		(vector unsigned int)zero);

	/* Errored packets will have RTE_PTYPE_ALL_MASK. */
	op_err = (vector unsigned char)
		vec_sr((vector unsigned short)op_err,
		(vector unsigned short){8, 8, 8, 8, 8, 8, 8, 8});
	ptype = (vector unsigned char)
		vec_or((vector unsigned long)ptype,
		(vector unsigned long)op_err);

	pt_idx0 = (uint8_t)((vector unsigned char)ptype)[0];
	pt_idx1 = (uint8_t)((vector unsigned char)ptype)[2];
	pt_idx2 = (uint8_t)((vector unsigned char)ptype)[4];
	pt_idx3 = (uint8_t)((vector unsigned char)ptype)[6];

	pkts[0]->packet_type = mlx5_ptype_table[pt_idx0] |
		!!(pt_idx0 & (1 << 6)) * rxq->tunnel;
	pkts[1]->packet_type = mlx5_ptype_table[pt_idx1] |
		!!(pt_idx1 & (1 << 6)) * rxq->tunnel;
	pkts[2]->packet_type = mlx5_ptype_table[pt_idx2] |
		!!(pt_idx2 & (1 << 6)) * rxq->tunnel;
	pkts[3]->packet_type = mlx5_ptype_table[pt_idx3] |
		!!(pt_idx3 & (1 << 6)) * rxq->tunnel;

	/* Fill flags for checksum and VLAN. */
	pinfo = (vector unsigned char)
		vec_and((vector unsigned long)pinfo,
		(vector unsigned long)ptype_ol_mask);
	pinfo = vec_perm(cv_flag_sel, zero, pinfo);

	/* Locate checksum flags at byte[2:1] and merge with VLAN flags. */
	cv_flags = (vector unsigned char)
		vec_sl((vector unsigned int)pinfo,
		(vector unsigned int){9, 9, 9, 9});
	cv_flags = (vector unsigned char)
		vec_or((vector unsigned long)pinfo,
		(vector unsigned long)cv_flags);

	/* Move back flags to start from byte[0]. */
	cv_flags = (vector unsigned char)
		vec_sr((vector unsigned int)cv_flags,
		(vector unsigned int){8, 8, 8, 8});

	/* Mask out garbage bits. */
	cv_flags = (vector unsigned char)
		vec_and((vector unsigned long)cv_flags,
		(vector unsigned long)cv_mask);

	/* Merge to ol_flags. */
	ol_flags = (vector unsigned char)
		vec_or((vector unsigned long)ol_flags,
		(vector unsigned long)cv_flags);

	/* Merge mbuf_init and ol_flags. */
	rearm0 = (vector unsigned char)
		vec_sel((vector unsigned short)mbuf_init,
		(vector unsigned short)
		vec_slo((vector unsigned short)ol_flags,
		(vector unsigned char){64}), rearm_sel_mask);
	rearm1 = (vector unsigned char)
		vec_sel((vector unsigned short)mbuf_init,
		(vector unsigned short)
		vec_slo((vector unsigned short)ol_flags,
		(vector unsigned char){32}), rearm_sel_mask);
	rearm2 = (vector unsigned char)
		vec_sel((vector unsigned short)mbuf_init,
		(vector unsigned short)ol_flags, rearm_sel_mask);
	rearm3 = (vector unsigned char)
		vec_sel((vector unsigned short)mbuf_init,
		(vector unsigned short)
		vec_sro((vector unsigned short)ol_flags,
		(vector unsigned char){32}), rearm_sel_mask);

	/* Write 8B rearm_data and 8B ol_flags. */
	vec_vsx_st(rearm0, 0,
		(vector unsigned char *)&pkts[0]->rearm_data);
	vec_vsx_st(rearm1, 0,
		(vector unsigned char *)&pkts[1]->rearm_data);
	vec_vsx_st(rearm2, 0,
		(vector unsigned char *)&pkts[2]->rearm_data);
	vec_vsx_st(rearm3, 0,
		(vector unsigned char *)&pkts[3]->rearm_data);
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
 * @param[out] no_cq
 *  Pointer to a boolean. Set true if no new CQE seen.
 *
 * @return
 *   Number of packets received including errors (<= pkts_n).
 */
static inline uint16_t
rxq_burst_v(struct mlx5_rxq_data *rxq, struct rte_mbuf **pkts, uint16_t pkts_n,
	    uint64_t *err, bool *no_cq)
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
	const vector unsigned char zero = (vector unsigned char){0};
	const vector unsigned char ones = vec_splat_u8(-1);
	const vector unsigned char owner_check =
		(vector unsigned char)(vector unsigned long){
		0x0100000001000000LL, 0x0100000001000000LL};
	const vector unsigned char opcode_check =
		(vector unsigned char)(vector unsigned long){
		0xf0000000f0000000LL, 0xf0000000f0000000LL};
	const vector unsigned char format_check =
		(vector unsigned char)(vector unsigned long){
		0x0c0000000c000000LL, 0x0c0000000c000000LL};
	const vector unsigned char resp_err_check =
		(vector unsigned char)(vector unsigned long){
		0xe0000000e0000000LL, 0xe0000000e0000000LL};
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint32_t rcvd_byte = 0;
	/* Mask to shuffle byte_cnt to add up stats. Do bswap16 for all. */
	const vector unsigned char len_shuf_mask = (vector unsigned char){
		 1,  0,  5,  4,
		 9,  8, 13, 12,
		-1, -1, -1, -1,
		-1, -1, -1, -1};
#endif
	/* Mask to shuffle from extracted CQE to mbuf. */
	const vector unsigned char shuf_mask = (vector unsigned char){
		 5,  4,           /* bswap16, pkt_len */
		-1, -1,           /* zero out 2nd half of pkt_len */
		 5,  4,           /* bswap16, data_len */
		11, 10,           /* bswap16, vlan+tci */
		15, 14, 13, 12,   /* bswap32, rss */
		 1,  2,  3, -1};  /* fdir.hi */
	/* Mask to blend from the last Qword to the first DQword. */
	/* Mask to blend from the last Qword to the first DQword. */
	const vector unsigned char blend_mask = (vector unsigned char){
		-1,  0,  0,  0,
		 0,  0,  0,  0,
		-1, -1, -1, -1,
		-1, -1, -1, -1};
	const vector unsigned char crc_adj =
		(vector unsigned char)(vector unsigned short){
		rxq->crc_present * RTE_ETHER_CRC_LEN, 0,
		rxq->crc_present * RTE_ETHER_CRC_LEN, 0, 0, 0, 0, 0};
	const vector unsigned char flow_mark_adj =
		(vector unsigned char)(vector unsigned int){
		0, 0, 0, rxq->mark * (-1)};
	const vector unsigned short cqe_sel_mask1 =
		(vector unsigned short){0, 0, 0, 0, 0xffff, 0xffff, 0, 0};
	const vector unsigned short cqe_sel_mask2 =
		(vector unsigned short){0, 0, 0xffff, 0, 0, 0, 0, 0};

	MLX5_ASSERT(rxq->sges_n == 0);
	MLX5_ASSERT(rxq->cqe_n == rxq->elts_n);
	cq = &(*rxq->cqes)[cq_idx];
	rte_prefetch0(cq);
	rte_prefetch0(cq + 1);
	rte_prefetch0(cq + 2);
	rte_prefetch0(cq + 3);
	pkts_n = RTE_MIN(pkts_n, MLX5_VPMD_RX_MAX_BURST);

	repl_n = q_n - (rxq->rq_ci - rxq->rq_pi);
	if (repl_n >= rxq->rq_repl_thresh)
		mlx5_rx_replenish_bulk_mbuf(rxq, repl_n);
	/* See if there're unreturned mbufs from compressed CQE. */
	rcvd_pkt = rxq->decompressed;
	if (rcvd_pkt > 0) {
		rcvd_pkt = RTE_MIN(rcvd_pkt, pkts_n);
		rxq_copy_mbuf_v(rxq, pkts, rcvd_pkt);
		rxq->rq_pi += rcvd_pkt;
		rxq->decompressed -= rcvd_pkt;
		pkts += rcvd_pkt;
	}
	elts_idx = rxq->rq_pi & q_mask;
	elts = &(*rxq->elts)[elts_idx];
	/* Not to overflow pkts array. */
	pkts_n = RTE_ALIGN_FLOOR(pkts_n - rcvd_pkt, MLX5_VPMD_DESCS_PER_LOOP);
	/* Not to cross queue end. */
	pkts_n = RTE_MIN(pkts_n, q_n - elts_idx);
	pkts_n = RTE_MIN(pkts_n, q_n - cq_idx);
	if (!pkts_n) {
		*no_cq = !rcvd_pkt;
		return rcvd_pkt;
	}
	/* At this point, there shouldn't be any remaining packets. */
	MLX5_ASSERT(rxq->decompressed == 0);

	/*
	 * A. load first Qword (8bytes) in one loop.
	 * B. copy 4 mbuf pointers from elts ring to returing pkts.
	 * C. load remaining CQE data and extract necessary fields.
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
		vector unsigned char cqes[MLX5_VPMD_DESCS_PER_LOOP];
		vector unsigned char cqe_tmp1, cqe_tmp2;
		vector unsigned char pkt_mb0, pkt_mb1, pkt_mb2, pkt_mb3;
		vector unsigned char op_own, op_own_tmp1, op_own_tmp2;
		vector unsigned char opcode, owner_mask, invalid_mask;
		vector unsigned char comp_mask;
		vector unsigned char mask;
#ifdef MLX5_PMD_SOFT_COUNTERS
		const vector unsigned char lower_half = {
			0, 1, 4, 5, 8, 9, 12, 13,
			16, 17, 20, 21, 24, 25, 28, 29};
		const vector unsigned char upper_half = {
			2, 3, 6, 7, 10, 11, 14, 15,
			18, 19, 22, 23, 26, 27, 30, 31};
		const vector unsigned long shmax = {64, 64};
		vector unsigned char byte_cnt;
		vector unsigned short left, right;
		vector unsigned long lshift;
		vector __attribute__((altivec(bool__)))
			unsigned long shmask;
#endif
		vector unsigned char mbp1, mbp2;
		vector unsigned char p =
			(vector unsigned char)(vector unsigned short){
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
		mask = (vector unsigned char)(vector unsigned long){
			(pkts_n - pos) * sizeof(uint16_t) * 8, 0};

		{
			vector unsigned long lshift;
			vector __attribute__((altivec(bool__)))
				unsigned long shmask;
			const vector unsigned long shmax = {64, 64};

			lshift = vec_splat((vector unsigned long)mask, 0);
			shmask = vec_cmpgt(shmax, lshift);
			mask = (vector unsigned char)
				vec_sl((vector unsigned long)ones, lshift);
			mask = (vector unsigned char)
				vec_sel((vector unsigned long)shmask,
				(vector unsigned long)mask, shmask);
		}

		p = (vector unsigned char)
			vec_andc((vector unsigned long)p,
			(vector unsigned long)mask);

		/* A.1 load cqes. */
		p3 = (unsigned int)((vector unsigned short)p)[3];
		cqes[3] = (vector unsigned char)(vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos + p3].sop_drop_qpn, 0LL};
		rte_compiler_barrier();

		p2 = (unsigned int)((vector unsigned short)p)[2];
		cqes[2] = (vector unsigned char)(vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos + p2].sop_drop_qpn, 0LL};
		rte_compiler_barrier();

		/* B.1 load mbuf pointers. */
		mbp1 = (vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&elts[pos]);
		mbp2 = (vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&elts[pos + 2]);

		/* A.1 load a block having op_own. */
		p1 = (unsigned int)((vector unsigned short)p)[1];
		cqes[1] = (vector unsigned char)(vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos + p1].sop_drop_qpn, 0LL};
		rte_compiler_barrier();

		cqes[0] = (vector unsigned char)(vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos].sop_drop_qpn, 0LL};
		rte_compiler_barrier();

		/* B.2 copy mbuf pointers. */
		*(vector unsigned char *)&pkts[pos] = mbp1;
		*(vector unsigned char *)&pkts[pos + 2] = mbp2;
		rte_cio_rmb();

		/* C.1 load remaining CQE data and extract necessary fields. */
		cqe_tmp2 = *(vector unsigned char *)
			&cq[pos + p3].pkt_info;
		cqe_tmp1 = *(vector unsigned char *)
			&cq[pos + p2].pkt_info;
		cqes[3] = vec_sel(cqes[3], cqe_tmp2, blend_mask);
		cqes[2] = vec_sel(cqes[2], cqe_tmp1, blend_mask);
		cqe_tmp2 = (vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&cq[pos + p3].csum);
		cqe_tmp1 = (vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&cq[pos + p2].csum);
		cqes[3] = (vector unsigned char)
			vec_sel((vector unsigned short)cqes[3],
			(vector unsigned short)cqe_tmp2, cqe_sel_mask1);
		cqes[2] = (vector unsigned char)
			vec_sel((vector unsigned short)cqes[2],
			(vector unsigned short)cqe_tmp1, cqe_sel_mask1);
		cqe_tmp2 = (vector unsigned char)(vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos + p3].rsvd3[9], 0LL};
		cqe_tmp1 = (vector unsigned char)(vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos + p2].rsvd3[9], 0LL};
		cqes[3] = (vector unsigned char)
			vec_sel((vector unsigned short)cqes[3],
			(vector unsigned short)cqe_tmp2,
			(vector unsigned short)cqe_sel_mask2);
		cqes[2] = (vector unsigned char)
			vec_sel((vector unsigned short)cqes[2],
			(vector unsigned short)cqe_tmp1,
			(vector unsigned short)cqe_sel_mask2);

		/* C.2 generate final structure for mbuf with swapping bytes. */
		pkt_mb3 = vec_perm(cqes[3], zero, shuf_mask);
		pkt_mb2 = vec_perm(cqes[2], zero, shuf_mask);

		/* C.3 adjust CRC length. */
		pkt_mb3 = (vector unsigned char)
			((vector unsigned short)pkt_mb3 -
			(vector unsigned short)crc_adj);
		pkt_mb2 = (vector unsigned char)
			((vector unsigned short)pkt_mb2 -
			(vector unsigned short)crc_adj);

		/* C.4 adjust flow mark. */
		pkt_mb3 = (vector unsigned char)
			((vector unsigned int)pkt_mb3 +
			(vector unsigned int)flow_mark_adj);
		pkt_mb2 = (vector unsigned char)
			((vector unsigned int)pkt_mb2 +
			(vector unsigned int)flow_mark_adj);

		/* D.1 fill in mbuf - rx_descriptor_fields1. */
		*(vector unsigned char *)
			&pkts[pos + 3]->pkt_len = pkt_mb3;
		*(vector unsigned char *)
			&pkts[pos + 2]->pkt_len = pkt_mb2;

		/* E.1 extract op_own field. */
		op_own_tmp2 = (vector unsigned char)
			vec_mergeh((vector unsigned int)cqes[2],
			(vector unsigned int)cqes[3]);

		/* C.1 load remaining CQE data and extract necessary fields. */
		cqe_tmp2 = *(vector unsigned char *)
			&cq[pos + p1].pkt_info;
		cqe_tmp1 = *(vector unsigned char *)
			&cq[pos].pkt_info;
		cqes[1] = vec_sel(cqes[1], cqe_tmp2, blend_mask);
		cqes[0] = vec_sel(cqes[0], cqe_tmp2, blend_mask);
		cqe_tmp2 = (vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&cq[pos + p1].csum);
		cqe_tmp1 = (vector unsigned char)vec_vsx_ld(0,
			(signed int const *)&cq[pos].csum);
		cqes[1] = (vector unsigned char)
			vec_sel((vector unsigned short)cqes[1],
			(vector unsigned short)cqe_tmp2, cqe_sel_mask1);
		cqes[0] = (vector unsigned char)
			vec_sel((vector unsigned short)cqes[0],
			(vector unsigned short)cqe_tmp1, cqe_sel_mask1);
		cqe_tmp2 = (vector unsigned char)(vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos + p1].rsvd3[9], 0LL};
		cqe_tmp1 = (vector unsigned char)(vector unsigned long){
			*(__rte_aligned(8) unsigned long *)
			&cq[pos].rsvd3[9], 0LL};
		cqes[1] = (vector unsigned char)
			vec_sel((vector unsigned short)cqes[1],
			(vector unsigned short)cqe_tmp2, cqe_sel_mask2);
		cqes[0] = (vector unsigned char)
			vec_sel((vector unsigned short)cqes[0],
			(vector unsigned short)cqe_tmp1, cqe_sel_mask2);

		/* C.2 generate final structure for mbuf with swapping bytes. */
		pkt_mb1 = vec_perm(cqes[1], zero, shuf_mask);
		pkt_mb0 = vec_perm(cqes[0], zero, shuf_mask);

		/* C.3 adjust CRC length. */
		pkt_mb1 = (vector unsigned char)
			((vector unsigned short)pkt_mb1 -
			(vector unsigned short)crc_adj);
		pkt_mb0 = (vector unsigned char)
			((vector unsigned short)pkt_mb0 -
			(vector unsigned short)crc_adj);

		/* C.4 adjust flow mark. */
		pkt_mb1 = (vector unsigned char)
			((vector unsigned int)pkt_mb1 +
			(vector unsigned int)flow_mark_adj);
		pkt_mb0 = (vector unsigned char)
			((vector unsigned int)pkt_mb0 +
			(vector unsigned int)flow_mark_adj);

		/* E.1 extract op_own byte. */
		op_own_tmp1 = (vector unsigned char)
			vec_mergeh((vector unsigned int)cqes[0],
			(vector unsigned int)cqes[1]);
		op_own = (vector unsigned char)
			vec_mergel((vector unsigned long)op_own_tmp1,
			(vector unsigned long)op_own_tmp2);

		/* D.1 fill in mbuf - rx_descriptor_fields1. */
		*(vector unsigned char *)
			&pkts[pos + 1]->pkt_len = pkt_mb1;
		*(vector unsigned char *)
			&pkts[pos]->pkt_len = pkt_mb0;

		/* E.2 flip owner bit to mark CQEs from last round. */
		owner_mask = (vector unsigned char)
			vec_and((vector unsigned long)op_own,
			(vector unsigned long)owner_check);
		if (ownership)
			owner_mask = (vector unsigned char)
				vec_xor((vector unsigned long)owner_mask,
				(vector unsigned long)owner_check);
		owner_mask = (vector unsigned char)
			vec_cmpeq((vector unsigned int)owner_mask,
			(vector unsigned int)owner_check);
		owner_mask = (vector unsigned char)
			vec_packs((vector unsigned int)owner_mask,
			(vector unsigned int)zero);

		/* E.3 get mask for invalidated CQEs. */
		opcode = (vector unsigned char)
			vec_and((vector unsigned long)op_own,
			(vector unsigned long)opcode_check);
		invalid_mask = (vector unsigned char)
			vec_cmpeq((vector unsigned int)opcode_check,
			(vector unsigned int)opcode);
		invalid_mask = (vector unsigned char)
			vec_packs((vector unsigned int)invalid_mask,
			(vector unsigned int)zero);

		/* E.4 mask out beyond boundary. */
		invalid_mask = (vector unsigned char)
			vec_or((vector unsigned long)invalid_mask,
			(vector unsigned long)mask);

		/* E.5 merge invalid_mask with invalid owner. */
		invalid_mask = (vector unsigned char)
			vec_or((vector unsigned long)invalid_mask,
			(vector unsigned long)owner_mask);

		/* F.1 find compressed CQE format. */
		comp_mask = (vector unsigned char)
			vec_and((vector unsigned long)op_own,
			(vector unsigned long)format_check);
		comp_mask = (vector unsigned char)
			vec_cmpeq((vector unsigned int)comp_mask,
			(vector unsigned int)format_check);
		comp_mask = (vector unsigned char)
			vec_packs((vector unsigned int)comp_mask,
			(vector unsigned int)zero);

		/* F.2 mask out invalid entries. */
		comp_mask = (vector unsigned char)
			vec_andc((vector unsigned long)comp_mask,
			(vector unsigned long)invalid_mask);
		comp_idx = ((vector unsigned long)comp_mask)[0];

		/* F.3 get the first compressed CQE. */
		comp_idx = comp_idx ? __builtin_ctzll(comp_idx) /
			(sizeof(uint16_t) * 8) : MLX5_VPMD_DESCS_PER_LOOP;

		/* E.6 mask out entries after the compressed CQE. */
		mask = (vector unsigned char)(vector unsigned long){
			(comp_idx * sizeof(uint16_t) * 8), 0};
		lshift = vec_splat((vector unsigned long)mask, 0);
		shmask = vec_cmpgt(shmax, lshift);
		mask = (vector unsigned char)
			vec_sl((vector unsigned long)ones, lshift);
		mask = (vector unsigned char)
			vec_sel((vector unsigned long)shmask,
			(vector unsigned long)mask, shmask);
		invalid_mask = (vector unsigned char)
			vec_or((vector unsigned long)invalid_mask,
			(vector unsigned long)mask);

		/* E.7 count non-compressed valid CQEs. */
		n = ((vector unsigned long)invalid_mask)[0];
		n = n ? __builtin_ctzll(n) / (sizeof(uint16_t) * 8) :
			MLX5_VPMD_DESCS_PER_LOOP;
		nocmp_n += n;

		/* D.2 get the final invalid mask. */
		mask = (vector unsigned char)(vector unsigned long){
			(n * sizeof(uint16_t) * 8), 0};
		lshift = vec_splat((vector unsigned long)mask, 0);
		shmask = vec_cmpgt(shmax, lshift);
		mask = (vector unsigned char)
			vec_sl((vector unsigned long)ones, lshift);
		mask = (vector unsigned char)
			vec_sel((vector unsigned long)shmask,
			(vector unsigned long)mask, shmask);
		invalid_mask = (vector unsigned char)
			vec_or((vector unsigned long)invalid_mask,
			(vector unsigned long)mask);

		/* D.3 check error in opcode. */
		opcode = (vector unsigned char)
			vec_cmpeq((vector unsigned int)resp_err_check,
			(vector unsigned int)opcode);
		opcode = (vector unsigned char)
			vec_packs((vector unsigned int)opcode,
			(vector unsigned int)zero);
		opcode = (vector unsigned char)
			vec_andc((vector unsigned long)opcode,
			(vector unsigned long)invalid_mask);

		/* D.4 mark if any error is set */
		*err |= ((vector unsigned long)opcode)[0];

		/* D.5 fill in mbuf - rearm_data and packet_type. */
		rxq_cq_to_ptype_oflags_v(rxq, cqes, opcode, &pkts[pos]);
		if (rxq->hw_timestamp) {
			if (rxq->rt_timestamp) {
				struct mlx5_dev_ctx_shared *sh = rxq->sh;
				uint64_t ts;

				ts = rte_be_to_cpu_64(cq[pos].timestamp);
				pkts[pos]->timestamp =
					mlx5_txpp_convert_rx_ts(sh, ts);
				ts = rte_be_to_cpu_64(cq[pos + p1].timestamp);
				pkts[pos + 1]->timestamp =
					mlx5_txpp_convert_rx_ts(sh, ts);
				ts = rte_be_to_cpu_64(cq[pos + p2].timestamp);
				pkts[pos + 2]->timestamp =
					mlx5_txpp_convert_rx_ts(sh, ts);
				ts = rte_be_to_cpu_64(cq[pos + p3].timestamp);
				pkts[pos + 3]->timestamp =
					mlx5_txpp_convert_rx_ts(sh, ts);
			} else {
				pkts[pos]->timestamp = rte_be_to_cpu_64
						(cq[pos].timestamp);
				pkts[pos + 1]->timestamp = rte_be_to_cpu_64
						(cq[pos + p1].timestamp);
				pkts[pos + 2]->timestamp = rte_be_to_cpu_64
						(cq[pos + p2].timestamp);
				pkts[pos + 3]->timestamp = rte_be_to_cpu_64
						(cq[pos + p3].timestamp);
			}
		}
		if (rxq->dynf_meta) {
			uint64_t flag = rxq->flow_meta_mask;
			int32_t offs = rxq->flow_meta_offset;
			uint32_t metadata;

			/* This code is subject for futher optimization. */
			metadata = cq[pos].flow_table_metadata;
			*RTE_MBUF_DYNFIELD(pkts[pos], offs, uint32_t *) =
								metadata;
			pkts[pos]->ol_flags |= metadata ? flag : 0ULL;
			metadata = cq[pos + 1].flow_table_metadata;
			*RTE_MBUF_DYNFIELD(pkts[pos + 1], offs, uint32_t *) =
								metadata;
			pkts[pos + 1]->ol_flags |= metadata ? flag : 0ULL;
			metadata = cq[pos + 2].flow_table_metadata;
			*RTE_MBUF_DYNFIELD(pkts[pos + 2], offs, uint32_t *) =
								metadata;
			pkts[pos + 2]->ol_flags |= metadata ? flag : 0ULL;
			metadata = cq[pos + 3].flow_table_metadata;
			*RTE_MBUF_DYNFIELD(pkts[pos + 3], offs, uint32_t *) =
								metadata;
			pkts[pos + 3]->ol_flags |= metadata ? flag : 0ULL;
		}
#ifdef MLX5_PMD_SOFT_COUNTERS
		/* Add up received bytes count. */
		byte_cnt = vec_perm(op_own, zero, len_shuf_mask);
		byte_cnt = (vector unsigned char)
			vec_andc((vector unsigned long)byte_cnt,
			(vector unsigned long)invalid_mask);
		left = vec_perm((vector unsigned short)byte_cnt,
			(vector unsigned short)zero, lower_half);
		right = vec_perm((vector unsigned short)byte_cnt,
			(vector unsigned short)zero, upper_half);
		byte_cnt = (vector unsigned char)vec_add(left, right);
		left = vec_perm((vector unsigned short)byte_cnt,
			(vector unsigned short)zero, lower_half);
		right = vec_perm((vector unsigned short)byte_cnt,
			(vector unsigned short)zero, upper_half);
		byte_cnt = (vector unsigned char)vec_add(left, right);
		rcvd_byte += ((vector unsigned long)byte_cnt)[0];
#endif

		/*
		 * Break the loop unless more valid CQE is expected, or if
		 * there's a compressed CQE.
		 */
		if (n != MLX5_VPMD_DESCS_PER_LOOP)
			break;
	}
	/* If no new CQE seen, return without updating cq_db. */
	if (unlikely(!nocmp_n && comp_idx == MLX5_VPMD_DESCS_PER_LOOP)) {
		*no_cq = true;
		return rcvd_pkt;
	}
	/* Update the consumer indexes for non-compressed CQEs. */
	MLX5_ASSERT(nocmp_n <= pkts_n);
	rxq->cq_ci += nocmp_n;
	rxq->rq_pi += nocmp_n;
	rcvd_pkt += nocmp_n;
#ifdef MLX5_PMD_SOFT_COUNTERS
	rxq->stats.ipackets += nocmp_n;
	rxq->stats.ibytes += rcvd_byte;
#endif
	/* Decompress the last CQE if compressed. */
	if (comp_idx < MLX5_VPMD_DESCS_PER_LOOP && comp_idx == n) {
		MLX5_ASSERT(comp_idx == (nocmp_n % MLX5_VPMD_DESCS_PER_LOOP));
		rxq->decompressed =
			rxq_cq_decompress_v(rxq, &cq[nocmp_n], &elts[nocmp_n]);
		/* Return more packets if needed. */
		if (nocmp_n < pkts_n) {
			uint16_t n = rxq->decompressed;

			n = RTE_MIN(n, pkts_n - nocmp_n);
			rxq_copy_mbuf_v(rxq, &pkts[nocmp_n], n);
			rxq->rq_pi += n;
			rcvd_pkt += n;
			rxq->decompressed -= n;
		}
	}
	rte_compiler_barrier();
	*rxq->cq_db = rte_cpu_to_be_32(rxq->cq_ci);
	*no_cq = !rcvd_pkt;
	return rcvd_pkt;
}

#endif /* RTE_PMD_MLX5_RXTX_VEC_ALTIVEC_H_ */
