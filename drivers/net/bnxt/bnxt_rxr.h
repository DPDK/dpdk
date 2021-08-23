/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
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
	if (BNXT_CHIP_THOR(bp))
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
#define RTE_BNXT_DESCS_PER_LOOP		4U

#define BNXT_OL_FLAGS_TBL_DIM	64
#define BNXT_OL_FLAGS_ERR_TBL_DIM 32

struct bnxt_tpa_info {
	struct rte_mbuf			*mbuf;
	uint16_t			len;
	uint32_t			agg_count;
	struct rx_tpa_v2_abuf_cmpl	agg_arr[TPA_MAX_NUM_SEGS];
};

struct bnxt_rx_ring_info {
	uint16_t		rx_prod;
	uint16_t		ag_prod;
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

#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM64)
uint16_t bnxt_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			    uint16_t nb_pkts);
int bnxt_rxq_vec_setup(struct bnxt_rx_queue *rxq);
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

#define BNXT_PTYPE_TBL_DIM	128
extern uint32_t bnxt_ptype_table[BNXT_PTYPE_TBL_DIM];
#endif
static inline void bnxt_set_vlan(struct rx_pkt_cmpl_hi *rxcmp1,
				 struct rte_mbuf *mbuf)
{
	uint32_t metadata = rte_le_to_cpu_32(rxcmp1->metadata);

	mbuf->vlan_tci = metadata & (RX_PKT_CMPL_METADATA_VID_MASK |
				     RX_PKT_CMPL_METADATA_DE |
				     RX_PKT_CMPL_METADATA_PRI_MASK);
}

