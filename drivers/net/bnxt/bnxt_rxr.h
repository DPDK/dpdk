/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_RXR_H_
#define _BNXT_RXR_H_
#include "hsi_struct_def_dpdk.h"

#define BNXT_TPA_START_AGG_ID_PRE_TH(cmp) \
	((rte_le_to_cpu_16((cmp)->agg_id) & RX_TPA_START_CMPL_AGG_ID_MASK) >> \
	 RX_TPA_START_CMPL_AGG_ID_SFT)

#define BNXT_TPA_START_AGG_ID_TH(cmp) \
	rte_le_to_cpu_16((cmp)->agg_id)

static inline uint16_t bnxt_tpa_start_agg_id(struct bnxt *bp,
					     struct rx_tpa_start_cmpl *cmp)
{
	if (BNXT_CHIP_P5(bp))
		return BNXT_TPA_START_AGG_ID_TH(cmp);
	else
		return BNXT_TPA_START_AGG_ID_PRE_TH(cmp);
}

#define BNXT_TPA_END_AGG_BUFS(cmp) \
	(((cmp)->agg_bufs_v1 & RX_TPA_END_CMPL_AGG_BUFS_MASK) \
	 >> RX_TPA_END_CMPL_AGG_BUFS_SFT)

#define BNXT_TPA_END_AGG_BUFS_TH(cmp) \
	((cmp)->tpa_agg_bufs)

#define BNXT_TPA_END_AGG_ID(cmp) \
	(((cmp)->agg_id & RX_TPA_END_CMPL_AGG_ID_MASK) >> \
	 RX_TPA_END_CMPL_AGG_ID_SFT)

#define BNXT_TPA_END_AGG_ID_TH(cmp) \
	rte_le_to_cpu_16((cmp)->agg_id)

#define BNXT_RX_L2_AGG_BUFS(cmp) \
	(((cmp)->agg_bufs_v1 & RX_PKT_CMPL_AGG_BUFS_MASK) >> \
		RX_PKT_CMPL_AGG_BUFS_SFT)

/* Number of descriptors to process per inner loop in vector mode. */
#define BNXT_RX_DESCS_PER_LOOP_VEC128	4U /* SSE, Neon */
#define BNXT_RX_DESCS_PER_LOOP_VEC256	8U /* AVX2 */

/* Number of extra Rx mbuf ring entries to allocate for vector mode. */
#define BNXT_RX_EXTRA_MBUF_ENTRIES \
	RTE_MAX(BNXT_RX_DESCS_PER_LOOP_VEC128, BNXT_RX_DESCS_PER_LOOP_VEC256)

#define BNXT_OL_FLAGS_TBL_DIM	64
#define BNXT_OL_FLAGS_ERR_TBL_DIM 32

struct bnxt_tpa_info {
	struct rte_mbuf			*mbuf;
	uint16_t			len;
	uint32_t			agg_count;
	struct rx_tpa_v2_abuf_cmpl	agg_arr[TPA_MAX_NUM_SEGS];

	uint32_t                        rss_hash;
	uint32_t                        vlan;
	uint16_t                        cfa_code;
	uint8_t                         hash_valid:1;
	uint8_t                         vlan_valid:1;
	uint8_t                         cfa_code_valid:1;
	uint8_t                         l4_csum_valid:1;
};

struct bnxt_rx_ring_info {
	uint16_t		rx_raw_prod;
	uint16_t		ag_raw_prod;
	uint16_t                rx_cons; /* Needed for representor */
	uint16_t                rx_next_cons;
	struct bnxt_db_info     rx_db;
	struct bnxt_db_info     ag_db;

	struct rx_prod_pkt_bd	*rx_desc_ring;
	struct rx_prod_pkt_bd	*ag_desc_ring;
	struct rte_mbuf		**rx_buf_ring; /* sw ring */
	struct rte_mbuf		**ag_buf_ring; /* sw ring */

	rte_iova_t		rx_desc_mapping;
	rte_iova_t		ag_desc_mapping;

	struct bnxt_ring	*rx_ring_struct;
	struct bnxt_ring	*ag_ring_struct;

	/*
	 * To deal with out of order return from TPA, use free buffer indicator
	 */
	struct rte_bitmap	*ag_bitmap;

	struct bnxt_tpa_info *tpa_info;

	uint32_t ol_flags_table[BNXT_OL_FLAGS_TBL_DIM];
	uint32_t ol_flags_err_table[BNXT_OL_FLAGS_ERR_TBL_DIM];
};

uint16_t bnxt_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			       uint16_t nb_pkts);
void bnxt_free_rx_rings(struct bnxt *bp);
int bnxt_init_rx_ring_struct(struct bnxt_rx_queue *rxq, unsigned int socket_id);
int bnxt_init_one_rx_ring(struct bnxt_rx_queue *rxq);
int bnxt_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int bnxt_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int bnxt_flush_rx_cmp(struct bnxt_cp_ring_info *cpr);

#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM64)
uint16_t bnxt_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			    uint16_t nb_pkts);
int bnxt_rxq_vec_setup(struct bnxt_rx_queue *rxq);
#endif

#if defined(RTE_ARCH_X86) && defined(CC_AVX2_SUPPORT)
uint16_t bnxt_recv_pkts_vec_avx2(void *rx_queue, struct rte_mbuf **rx_pkts,
				 uint16_t nb_pkts);
#endif
void bnxt_set_mark_in_mbuf(struct bnxt *bp,
			   struct rx_pkt_cmpl_hi *rxcmp1,
			   struct rte_mbuf *mbuf);

typedef uint32_t bnxt_cfa_code_dynfield_t;
extern int bnxt_cfa_code_dynfield_offset;

static inline bnxt_cfa_code_dynfield_t *
bnxt_cfa_code_dynfield(struct rte_mbuf *mbuf)
{
	return RTE_MBUF_DYNFIELD(mbuf,
		bnxt_cfa_code_dynfield_offset, bnxt_cfa_code_dynfield_t *);
}

#define BNXT_RX_META_CFA_CODE_SHIFT		19
#define BNXT_CFA_CODE_META_SHIFT		16
#define BNXT_RX_META_CFA_CODE_INT_ACT_REC_BIT	0x8000000
#define BNXT_RX_META_CFA_CODE_EEM_BIT		0x4000000
#define BNXT_CFA_META_FMT_MASK			0x70
#define BNXT_CFA_META_FMT_SHFT			4
#define BNXT_CFA_META_FMT_EM_EEM_SHFT		1
#define BNXT_CFA_META_FMT_EEM			3
#define BNXT_CFA_META_EEM_TCAM_SHIFT		31
#define BNXT_CFA_META_EM_TEST(x) ((x) >> BNXT_CFA_META_EEM_TCAM_SHIFT)

/* Definitions for translation of hardware packet type to mbuf ptype. */
#define BNXT_PTYPE_TBL_DIM		128
#define BNXT_PTYPE_TBL_TUN_SFT		0 /* Set if tunneled packet. */
#define BNXT_PTYPE_TBL_TUN_MSK		BIT(BNXT_PTYPE_TBL_TUN_SFT)
#define BNXT_PTYPE_TBL_IP_VER_SFT	1 /* Set if IPv6, clear if IPv4. */
#define BNXT_PTYPE_TBL_IP_VER_MSK	BIT(BNXT_PTYPE_TBL_IP_VER_SFT)
#define BNXT_PTYPE_TBL_VLAN_SFT		2 /* Set if VLAN encapsulated. */
#define BNXT_PTYPE_TBL_VLAN_MSK		BIT(BNXT_PTYPE_TBL_VLAN_SFT)
#define BNXT_PTYPE_TBL_TYPE_SFT		3 /* Hardware packet type field. */
#define BNXT_PTYPE_TBL_TYPE_MSK		0x78 /* Hardware itype field mask. */
#define BNXT_PTYPE_TBL_TYPE_IP		1
#define BNXT_PTYPE_TBL_TYPE_TCP		2
#define BNXT_PTYPE_TBL_TYPE_UDP		3
#define BNXT_PTYPE_TBL_TYPE_ICMP	7

#define RX_PKT_CMPL_FLAGS2_IP_TYPE_SFT	8
#define CMPL_FLAGS2_VLAN_TUN_MSK \
	(RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN | RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC)

#define BNXT_CMPL_ITYPE_TO_IDX(ft) \
	(((ft) & RX_PKT_CMPL_FLAGS_ITYPE_MASK) >> \
	  (RX_PKT_CMPL_FLAGS_ITYPE_SFT - BNXT_PTYPE_TBL_TYPE_SFT))

#define BNXT_CMPL_VLAN_TUN_TO_IDX(f2) \
	(((f2) & CMPL_FLAGS2_VLAN_TUN_MSK) >> \
	 (RX_PKT_CMPL_FLAGS2_META_FORMAT_SFT - BNXT_PTYPE_TBL_VLAN_SFT))

#define BNXT_CMPL_IP_VER_TO_IDX(f2) \
	(((f2) & RX_PKT_CMPL_FLAGS2_IP_TYPE) >> \
	 (RX_PKT_CMPL_FLAGS2_IP_TYPE_SFT - BNXT_PTYPE_TBL_IP_VER_SFT))

static inline void
bnxt_check_ptype_constants(void)
{
	RTE_BUILD_BUG_ON(BNXT_CMPL_ITYPE_TO_IDX(RX_PKT_CMPL_FLAGS_ITYPE_MASK) !=
			 BNXT_PTYPE_TBL_TYPE_MSK);
	RTE_BUILD_BUG_ON(BNXT_CMPL_VLAN_TUN_TO_IDX(CMPL_FLAGS2_VLAN_TUN_MSK) !=
			 (BNXT_PTYPE_TBL_VLAN_MSK | BNXT_PTYPE_TBL_TUN_MSK));
	RTE_BUILD_BUG_ON(BNXT_CMPL_IP_VER_TO_IDX(RX_PKT_CMPL_FLAGS2_IP_TYPE) !=
			 BNXT_PTYPE_TBL_IP_VER_MSK);
}

extern uint32_t bnxt_ptype_table[BNXT_PTYPE_TBL_DIM];

static inline void bnxt_set_vlan(struct rx_pkt_cmpl_hi *rxcmp1,
				 struct rte_mbuf *mbuf)
{
	uint32_t metadata = rte_le_to_cpu_32(rxcmp1->metadata);

	mbuf->vlan_tci = metadata & (RX_PKT_CMPL_METADATA_VID_MASK |
				     RX_PKT_CMPL_METADATA_DE |
				     RX_PKT_CMPL_METADATA_PRI_MASK);
}

/* Stingray2 specific code for RX completion parsing */
#define RX_CMP_VLAN_VALID(rxcmp)        \
	(((struct rx_pkt_v2_cmpl *)rxcmp)->metadata1_payload_offset &	\
	 RX_PKT_V2_CMPL_METADATA1_VALID)

#define RX_CMP_METADATA0_VID(rxcmp1)				\
	((((struct rx_pkt_v2_cmpl_hi *)rxcmp1)->metadata0) &	\
	 (RX_PKT_V2_CMPL_HI_METADATA0_VID_MASK |		\
	  RX_PKT_V2_CMPL_HI_METADATA0_DE  |			\
	  RX_PKT_V2_CMPL_HI_METADATA0_PRI_MASK))

static inline void bnxt_rx_vlan_v2(struct rte_mbuf *mbuf,
				   struct rx_pkt_cmpl *rxcmp,
				   struct rx_pkt_cmpl_hi *rxcmp1)
{
	if (RX_CMP_VLAN_VALID(rxcmp)) {
		mbuf->vlan_tci = RX_CMP_METADATA0_VID(rxcmp1);
		mbuf->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
	}
}

#define RX_CMP_FLAGS2_CS_ALL_OK_MODE_MASK	(0x1 << 3)
#define RX_CMP_FLAGS2_CS_OK_HDR_CNT_MASK	(0x7 << 10)
#define RX_CMP_FLAGS2_IP_CSUM_ALL_OK_MASK	(0x1 << 13)
#define RX_CMP_FLAGS2_L4_CSUM_ALL_OK_MASK	(0x1 << 14)

#define RX_CMP_V2_CS_OK_HDR_CNT(flags)				\
	(((flags) & RX_CMP_FLAGS2_CS_OK_HDR_CNT_MASK) >>	\
	 RX_PKT_V2_CMPL_HI_FLAGS2_CS_OK_SFT)

#define RX_CMP_V2_CS_ALL_OK_MODE(flags)				\
	(((flags) & RX_CMP_FLAGS2_CS_ALL_OK_MODE_MASK))

#define RX_CMP_FLAGS2_L3_CS_OK_MASK		(0x7 << 10)
#define RX_CMP_FLAGS2_L4_CS_OK_MASK		(0x38 << 10)
#define RX_CMP_FLAGS2_L3_CS_OK_SFT		10
#define RX_CMP_FLAGS2_L4_CS_OK_SFT		13

#define RX_CMP_V2_L4_CS_OK(flags2)			\
	(((flags2) & RX_CMP_FLAGS2_L4_CS_OK_MASK) >>	\
	 RX_CMP_FLAGS2_L4_CS_OK_SFT)

#define RX_CMP_V2_L3_CS_OK(flags2)			\
	(((flags2) & RX_CMP_FLAGS2_L3_CS_OK_MASK) >>	\
	 RX_CMP_FLAGS2_L3_CS_OK_SFT)

#define RX_CMP_V2_L4_CS_ERR(err)				\
	(((err) & RX_PKT_V2_CMPL_HI_ERRORS_PKT_ERROR_MASK)  ==	\
	 RX_PKT_V2_CMPL_HI_ERRORS_PKT_ERROR_L4_CS_ERROR)

#define RX_CMP_V2_L3_CS_ERR(err)				\
	(((err) & RX_PKT_V2_CMPL_HI_ERRORS_PKT_ERROR_MASK) ==	\
	 RX_PKT_V2_CMPL_HI_ERRORS_PKT_ERROR_IP_CS_ERROR)

#define RX_CMP_V2_T_IP_CS_ERR(err)				\
	(((err) & RX_PKT_V2_CMPL_HI_ERRORS_T_PKT_ERROR_MASK) ==	\
	 RX_PKT_V2_CMPL_HI_ERRORS_T_PKT_ERROR_T_IP_CS_ERROR)

#define RX_CMP_V2_T_L4_CS_ERR(err)				\
	(((err) & RX_PKT_V2_CMPL_HI_ERRORS_T_PKT_ERROR_MASK) ==	\
	 RX_PKT_V2_CMPL_HI_ERRORS_T_PKT_ERROR_T_L4_CS_ERROR)

#define RX_CMP_V2_OT_L4_CS_ERR(err)					\
	(((err) & RX_PKT_V2_CMPL_HI_ERRORS_OT_PKT_ERROR_MASK) ==	\
	 RX_PKT_V2_CMPL_HI_ERRORS_OT_PKT_ERROR_OT_L4_CS_ERROR)

static inline void bnxt_parse_csum_v2(struct rte_mbuf *mbuf,
				      struct rx_pkt_cmpl_hi *rxcmp1)
{
	struct rx_pkt_v2_cmpl_hi *v2_cmp =
		(struct rx_pkt_v2_cmpl_hi *)(rxcmp1);
	uint16_t error_v2 = rte_le_to_cpu_16(v2_cmp->errors_v2);
	uint32_t flags2 = rte_le_to_cpu_32(v2_cmp->flags2);
	uint32_t hdr_cnt = 0, t_pkt = 0;

	if (RX_CMP_V2_CS_ALL_OK_MODE(flags2)) {
		hdr_cnt = RX_CMP_V2_CS_OK_HDR_CNT(flags2);
		if (hdr_cnt > 1)
			t_pkt = 1;

		if (unlikely(RX_CMP_V2_L4_CS_ERR(error_v2)))
			mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
		else if (flags2 & RX_CMP_FLAGS2_L4_CSUM_ALL_OK_MASK)
			mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
		else
			mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN;

		if (unlikely(RX_CMP_V2_L3_CS_ERR(error_v2)))
			mbuf->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
		else if (flags2 & RX_CMP_FLAGS2_IP_CSUM_ALL_OK_MASK)
			mbuf->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
		else
			mbuf->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN;
	} else {
		hdr_cnt = RX_CMP_V2_L4_CS_OK(flags2);
		if (hdr_cnt > 1)
			t_pkt = 1;

		if (RX_CMP_V2_L4_CS_OK(flags2))
			mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
		else if (RX_CMP_V2_L4_CS_ERR(error_v2))
			mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
		else
			mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_UNKNOWN;

		if (RX_CMP_V2_L3_CS_OK(flags2))
			mbuf->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
		else if (RX_CMP_V2_L3_CS_ERR(error_v2))
			mbuf->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
		else
			mbuf->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN;
	}

	if (t_pkt) {
		if (unlikely(RX_CMP_V2_OT_L4_CS_ERR(error_v2) ||
					RX_CMP_V2_T_L4_CS_ERR(error_v2)))
			mbuf->ol_flags |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD;
		else
			mbuf->ol_flags |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD;

		if (unlikely(RX_CMP_V2_T_IP_CS_ERR(error_v2)))
			mbuf->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
	}
}

static inline void
bnxt_parse_pkt_type_v2(struct rte_mbuf *mbuf,
		       struct rx_pkt_cmpl *rxcmp,
		       struct rx_pkt_cmpl_hi *rxcmp1)
{
	struct rx_pkt_v2_cmpl *v2_cmp =
		(struct rx_pkt_v2_cmpl *)(rxcmp);
	struct rx_pkt_v2_cmpl_hi *v2_cmp1 =
		(struct rx_pkt_v2_cmpl_hi *)(rxcmp1);
	uint16_t flags_type = v2_cmp->flags_type &
		rte_cpu_to_le_32(RX_PKT_V2_CMPL_FLAGS_ITYPE_MASK);
	uint32_t flags2 = rte_le_to_cpu_32(v2_cmp1->flags2);
	uint32_t l3, pkt_type = 0, vlan = 0;
	uint32_t ip6 = 0, t_pkt = 0;
	uint32_t hdr_cnt, csum_count;

	if (RX_CMP_V2_CS_ALL_OK_MODE(flags2)) {
		hdr_cnt = RX_CMP_V2_CS_OK_HDR_CNT(flags2);
		if (hdr_cnt > 1)
			t_pkt = 1;
	} else {
		csum_count = RX_CMP_V2_L4_CS_OK(flags2);
		if (csum_count > 1)
			t_pkt = 1;
	}

	vlan = !!RX_CMP_VLAN_VALID(rxcmp);
	pkt_type |= vlan ? RTE_PTYPE_L2_ETHER_VLAN : RTE_PTYPE_L2_ETHER;

	ip6 = !!(flags2 & RX_PKT_V2_CMPL_HI_FLAGS2_IP_TYPE);

	if (!t_pkt && !ip6)
		l3 = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
	else if (!t_pkt && ip6)
		l3 = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
	else if (t_pkt && !ip6)
		l3 = RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN;
	else
		l3 = RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN;

	switch (flags_type) {
	case RTE_LE32(RX_PKT_V2_CMPL_FLAGS_ITYPE_ICMP):
		if (!t_pkt)
			pkt_type |= l3 | RTE_PTYPE_L4_ICMP;
		else
			pkt_type |= l3 | RTE_PTYPE_INNER_L4_ICMP;
		break;
	case RTE_LE32(RX_PKT_V2_CMPL_FLAGS_ITYPE_TCP):
		if (!t_pkt)
			pkt_type |= l3 | RTE_PTYPE_L4_TCP;
		else
			pkt_type |= l3 | RTE_PTYPE_INNER_L4_TCP;
		break;
	case RTE_LE32(RX_PKT_V2_CMPL_FLAGS_ITYPE_UDP):
		if (!t_pkt)
			pkt_type |= l3 | RTE_PTYPE_L4_UDP;
		else
			pkt_type |= l3 | RTE_PTYPE_INNER_L4_UDP;
		break;
	case RTE_LE32(RX_PKT_V2_CMPL_FLAGS_ITYPE_IP):
		pkt_type |= l3;
		break;
	}

	mbuf->packet_type = pkt_type;
}
#endif /*  _BNXT_RXR_H_ */
