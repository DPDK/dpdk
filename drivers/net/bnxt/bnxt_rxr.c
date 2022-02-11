/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>
#include <stdbool.h>

#include <rte_bitmap.h>
#include <rte_byteorder.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_alarm.h>

#include "bnxt.h"
#include "bnxt_reps.h"
#include "bnxt_ring.h"
#include "bnxt_rxr.h"
#include "bnxt_rxq.h"
#include "hsi_struct_def_dpdk.h"
#include "bnxt_hwrm.h"

#include <bnxt_tf_common.h>
#include <ulp_mark_mgr.h>

/*
 * RX Ring handling
 */

static inline struct rte_mbuf *__bnxt_alloc_rx_data(struct rte_mempool *mb)
{
	struct rte_mbuf *data;

	data = rte_mbuf_raw_alloc(mb);

	return data;
}

static inline int bnxt_alloc_rx_data(struct bnxt_rx_queue *rxq,
				     struct bnxt_rx_ring_info *rxr,
				     uint16_t raw_prod)
{
	uint16_t prod = RING_IDX(rxr->rx_ring_struct, raw_prod);
	struct rx_prod_pkt_bd *rxbd;
	struct rte_mbuf **rx_buf;
	struct rte_mbuf *mbuf;

	rxbd = &rxr->rx_desc_ring[prod];
	rx_buf = &rxr->rx_buf_ring[prod];
	mbuf = __bnxt_alloc_rx_data(rxq->mb_pool);
	if (!mbuf) {
		rte_atomic64_inc(&rxq->rx_mbuf_alloc_fail);
		return -ENOMEM;
	}

	*rx_buf = mbuf;
	mbuf->data_off = RTE_PKTMBUF_HEADROOM;

	rxbd->address = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

	return 0;
}

static inline int bnxt_alloc_ag_data(struct bnxt_rx_queue *rxq,
				     struct bnxt_rx_ring_info *rxr,
				     uint16_t raw_prod)
{
	uint16_t prod = RING_IDX(rxr->ag_ring_struct, raw_prod);
	struct rx_prod_pkt_bd *rxbd;
	struct rte_mbuf **rx_buf;
	struct rte_mbuf *mbuf;

	rxbd = &rxr->ag_desc_ring[prod];
	rx_buf = &rxr->ag_buf_ring[prod];
	if (rxbd == NULL) {
		PMD_DRV_LOG(ERR, "Jumbo Frame. rxbd is NULL\n");
		return -EINVAL;
	}

	if (rx_buf == NULL) {
		PMD_DRV_LOG(ERR, "Jumbo Frame. rx_buf is NULL\n");
		return -EINVAL;
	}

	mbuf = __bnxt_alloc_rx_data(rxq->mb_pool);
	if (!mbuf) {
		rte_atomic64_inc(&rxq->rx_mbuf_alloc_fail);
		return -ENOMEM;
	}

	*rx_buf = mbuf;
	mbuf->data_off = RTE_PKTMBUF_HEADROOM;

	rxbd->address = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

	return 0;
}

static inline void bnxt_reuse_rx_mbuf(struct bnxt_rx_ring_info *rxr,
			       struct rte_mbuf *mbuf)
{
	uint16_t prod, raw_prod = RING_NEXT(rxr->rx_raw_prod);
	struct rte_mbuf **prod_rx_buf;
	struct rx_prod_pkt_bd *prod_bd;

	prod = RING_IDX(rxr->rx_ring_struct, raw_prod);
	prod_rx_buf = &rxr->rx_buf_ring[prod];

	RTE_ASSERT(*prod_rx_buf == NULL);
	RTE_ASSERT(mbuf != NULL);

	*prod_rx_buf = mbuf;

	prod_bd = &rxr->rx_desc_ring[prod];

	prod_bd->address = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));

	rxr->rx_raw_prod = raw_prod;
}

static inline
struct rte_mbuf *bnxt_consume_rx_buf(struct bnxt_rx_ring_info *rxr,
				     uint16_t cons)
{
	struct rte_mbuf **cons_rx_buf;
	struct rte_mbuf *mbuf;

	cons_rx_buf = &rxr->rx_buf_ring[RING_IDX(rxr->rx_ring_struct, cons)];
	RTE_ASSERT(*cons_rx_buf != NULL);
	mbuf = *cons_rx_buf;
	*cons_rx_buf = NULL;

	return mbuf;
}

static void bnxt_rx_ring_reset(void *arg)
{
	struct bnxt *bp = arg;
	int i, rc = 0;
	struct bnxt_rx_queue *rxq;


	for (i = 0; i < (int)bp->rx_nr_rings; i++) {
		struct bnxt_rx_ring_info *rxr;

		rxq = bp->rx_queues[i];
		if (!rxq || !rxq->in_reset)
			continue;

		rxr = rxq->rx_ring;
		/* Disable and flush TPA before resetting the RX ring */
		if (rxr->tpa_info)
			bnxt_hwrm_vnic_tpa_cfg(bp, rxq->vnic, false);
		rc = bnxt_hwrm_rx_ring_reset(bp, i);
		if (rc) {
			PMD_DRV_LOG(ERR, "Rx ring%d reset failed\n", i);
			continue;
		}

		bnxt_rx_queue_release_mbufs(rxq);
		rxr->rx_raw_prod = 0;
		rxr->ag_raw_prod = 0;
		rxr->rx_next_cons = 0;
		bnxt_init_one_rx_ring(rxq);
		bnxt_db_write(&rxr->rx_db, rxr->rx_raw_prod);
		bnxt_db_write(&rxr->ag_db, rxr->ag_raw_prod);
		if (rxr->tpa_info)
			bnxt_hwrm_vnic_tpa_cfg(bp, rxq->vnic, true);

		rxq->in_reset = 0;
	}
}


static void bnxt_sched_ring_reset(struct bnxt_rx_queue *rxq)
{
	rxq->in_reset = 1;
	rte_eal_alarm_set(1, bnxt_rx_ring_reset, (void *)rxq->bp);
}

static void bnxt_tpa_get_metadata(struct bnxt *bp,
				  struct bnxt_tpa_info *tpa_info,
				  struct rx_tpa_start_cmpl *tpa_start,
				  struct rx_tpa_start_cmpl_hi *tpa_start1)
{
	tpa_info->cfa_code_valid = 0;
	tpa_info->vlan_valid = 0;
	tpa_info->hash_valid = 0;
	tpa_info->l4_csum_valid = 0;

	if (likely(tpa_start->flags_type &
		   rte_cpu_to_le_32(RX_TPA_START_CMPL_FLAGS_RSS_VALID))) {
		tpa_info->hash_valid = 1;
		tpa_info->rss_hash = rte_le_to_cpu_32(tpa_start->rss_hash);
	}

	if (bp->vnic_cap_flags & BNXT_VNIC_CAP_RX_CMPL_V2) {
		struct rx_tpa_start_v2_cmpl *v2_tpa_start = (void *)tpa_start;
		struct rx_tpa_start_v2_cmpl_hi *v2_tpa_start1 =
			(void *)tpa_start1;

		if (v2_tpa_start->agg_id &
		    RX_TPA_START_V2_CMPL_METADATA1_VALID) {
			tpa_info->vlan_valid = 1;
			tpa_info->vlan =
				rte_le_to_cpu_16(v2_tpa_start1->metadata0);
		}

		if (v2_tpa_start1->flags2 & RX_CMP_FLAGS2_L4_CSUM_ALL_OK_MASK)
			tpa_info->l4_csum_valid = 1;

		return;
	}

	tpa_info->cfa_code_valid = 1;
	tpa_info->cfa_code = rte_le_to_cpu_16(tpa_start1->cfa_code);
	if (tpa_start1->flags2 &
	    rte_cpu_to_le_32(RX_TPA_START_CMPL_FLAGS2_META_FORMAT_VLAN)) {
		tpa_info->vlan_valid = 1;
		tpa_info->vlan = rte_le_to_cpu_32(tpa_start1->metadata);
	}

	if (likely(tpa_start1->flags2 &
		   rte_cpu_to_le_32(RX_TPA_START_CMPL_FLAGS2_L4_CS_CALC)))
		tpa_info->l4_csum_valid = 1;
}

static void bnxt_tpa_start(struct bnxt_rx_queue *rxq,
			   struct rx_tpa_start_cmpl *tpa_start,
			   struct rx_tpa_start_cmpl_hi *tpa_start1)
{
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint16_t agg_id;
	uint16_t data_cons;
	struct bnxt_tpa_info *tpa_info;
	struct rte_mbuf *mbuf;

	agg_id = bnxt_tpa_start_agg_id(rxq->bp, tpa_start);

	data_cons = tpa_start->opaque;
	tpa_info = &rxr->tpa_info[agg_id];
	if (unlikely(data_cons != rxr->rx_next_cons)) {
		PMD_DRV_LOG(ERR, "TPA cons %x, expected cons %x\n",
			    data_cons, rxr->rx_next_cons);
		bnxt_sched_ring_reset(rxq);
		return;
	}

	mbuf = bnxt_consume_rx_buf(rxr, data_cons);

	bnxt_reuse_rx_mbuf(rxr, tpa_info->mbuf);

	tpa_info->agg_count = 0;
	tpa_info->mbuf = mbuf;
	tpa_info->len = rte_le_to_cpu_32(tpa_start->len);

	mbuf->data_off = RTE_PKTMBUF_HEADROOM;
	mbuf->nb_segs = 1;
	mbuf->next = NULL;
	mbuf->pkt_len = rte_le_to_cpu_32(tpa_start->len);
	mbuf->data_len = mbuf->pkt_len;
	mbuf->port = rxq->port_id;
	mbuf->ol_flags = RTE_MBUF_F_RX_LRO;

	bnxt_tpa_get_metadata(rxq->bp, tpa_info, tpa_start, tpa_start1);

	if (likely(tpa_info->hash_valid)) {
		mbuf->hash.rss = tpa_info->rss_hash;
		mbuf->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
	} else if (tpa_info->cfa_code_valid) {
		mbuf->hash.fdir.id = tpa_info->cfa_code;
		mbuf->ol_flags |= RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID;
	}

	if (tpa_info->vlan_valid && BNXT_RX_VLAN_STRIP_EN(rxq->bp)) {
		mbuf->vlan_tci = tpa_info->vlan;
		mbuf->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
	}

	if (likely(tpa_info->l4_csum_valid))
		mbuf->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;

	/* recycle next mbuf */
	data_cons = RING_NEXT(data_cons);
	bnxt_reuse_rx_mbuf(rxr, bnxt_consume_rx_buf(rxr, data_cons));

	rxr->rx_next_cons = RING_IDX(rxr->rx_ring_struct,
				     RING_NEXT(data_cons));
}

static int bnxt_agg_bufs_valid(struct bnxt_cp_ring_info *cpr,
		uint8_t agg_bufs, uint32_t raw_cp_cons)
{
	uint16_t last_cp_cons;
	struct rx_pkt_cmpl *agg_cmpl;

	raw_cp_cons = ADV_RAW_CMP(raw_cp_cons, agg_bufs);
	last_cp_cons = RING_CMP(cpr->cp_ring_struct, raw_cp_cons);
	agg_cmpl = (struct rx_pkt_cmpl *)&cpr->cp_desc_ring[last_cp_cons];
	return bnxt_cpr_cmp_valid(agg_cmpl, raw_cp_cons,
				  cpr->cp_ring_struct->ring_size);
}

/* TPA consume agg buffer out of order, allocate connected data only */
static int bnxt_prod_ag_mbuf(struct bnxt_rx_queue *rxq)
{
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint16_t raw_next = RING_NEXT(rxr->ag_raw_prod);
	uint16_t bmap_next = RING_IDX(rxr->ag_ring_struct, raw_next);

	/* TODO batch allocation for better performance */
	while (rte_bitmap_get(rxr->ag_bitmap, bmap_next)) {
		if (unlikely(bnxt_alloc_ag_data(rxq, rxr, raw_next))) {
			PMD_DRV_LOG(ERR, "agg mbuf alloc failed: prod=0x%x\n",
				    raw_next);
			break;
		}
		rte_bitmap_clear(rxr->ag_bitmap, bmap_next);
		rxr->ag_raw_prod = raw_next;
		raw_next = RING_NEXT(raw_next);
		bmap_next = RING_IDX(rxr->ag_ring_struct, raw_next);
	}

	return 0;
}

static int bnxt_rx_pages(struct bnxt_rx_queue *rxq,
			 struct rte_mbuf *mbuf, uint32_t *tmp_raw_cons,
			 uint8_t agg_buf, struct bnxt_tpa_info *tpa_info)
{
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	int i;
	uint16_t cp_cons, ag_cons;
	struct rx_pkt_cmpl *rxcmp;
	struct rte_mbuf *last = mbuf;
	bool is_p5_tpa = tpa_info && BNXT_CHIP_P5(rxq->bp);

	for (i = 0; i < agg_buf; i++) {
		struct rte_mbuf **ag_buf;
		struct rte_mbuf *ag_mbuf;

		if (is_p5_tpa) {
			rxcmp = (void *)&tpa_info->agg_arr[i];
		} else {
			*tmp_raw_cons = NEXT_RAW_CMP(*tmp_raw_cons);
			cp_cons = RING_CMP(cpr->cp_ring_struct, *tmp_raw_cons);
			rxcmp = (struct rx_pkt_cmpl *)
					&cpr->cp_desc_ring[cp_cons];
		}

#ifdef BNXT_DEBUG
		bnxt_dump_cmpl(cp_cons, rxcmp);
#endif

		ag_cons = rxcmp->opaque;
		RTE_ASSERT(ag_cons <= rxr->ag_ring_struct->ring_mask);
		ag_buf = &rxr->ag_buf_ring[ag_cons];
		ag_mbuf = *ag_buf;
		RTE_ASSERT(ag_mbuf != NULL);

		ag_mbuf->data_len = rte_le_to_cpu_16(rxcmp->len);

		mbuf->nb_segs++;
		mbuf->pkt_len += ag_mbuf->data_len;

		last->next = ag_mbuf;
		last = ag_mbuf;

		*ag_buf = NULL;

		/*
		 * As aggregation buffer consumed out of order in TPA module,
		 * use bitmap to track freed slots to be allocated and notified
		 * to NIC
		 */
		rte_bitmap_set(rxr->ag_bitmap, ag_cons);
	}
	last->next = NULL;
	bnxt_prod_ag_mbuf(rxq);
	return 0;
}

static int bnxt_discard_rx(struct bnxt *bp, struct bnxt_cp_ring_info *cpr,
			   uint32_t *raw_cons, void *cmp)
{
	struct rx_pkt_cmpl *rxcmp = cmp;
	uint32_t tmp_raw_cons = *raw_cons;
	uint8_t cmp_type, agg_bufs = 0;

	cmp_type = CMP_TYPE(rxcmp);

	if (cmp_type == CMPL_BASE_TYPE_RX_L2) {
		agg_bufs = BNXT_RX_L2_AGG_BUFS(rxcmp);
	} else if (cmp_type == RX_TPA_END_CMPL_TYPE_RX_TPA_END) {
		struct rx_tpa_end_cmpl *tpa_end = cmp;

		if (BNXT_CHIP_P5(bp))
			return 0;

		agg_bufs = BNXT_TPA_END_AGG_BUFS(tpa_end);
	}

	if (agg_bufs) {
		if (!bnxt_agg_bufs_valid(cpr, agg_bufs, tmp_raw_cons))
			return -EBUSY;
	}
	*raw_cons = tmp_raw_cons;
	return 0;
}

static inline struct rte_mbuf *bnxt_tpa_end(
		struct bnxt_rx_queue *rxq,
		uint32_t *raw_cp_cons,
		struct rx_tpa_end_cmpl *tpa_end,
		struct rx_tpa_end_cmpl_hi *tpa_end1)
{
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint16_t agg_id;
	struct rte_mbuf *mbuf;
	uint8_t agg_bufs;
	uint8_t payload_offset;
	struct bnxt_tpa_info *tpa_info;

	if (unlikely(rxq->in_reset)) {
		PMD_DRV_LOG(ERR, "rxq->in_reset: raw_cp_cons:%d\n",
			    *raw_cp_cons);
		bnxt_discard_rx(rxq->bp, cpr, raw_cp_cons, tpa_end);
		return NULL;
	}

	if (BNXT_CHIP_P5(rxq->bp)) {
		struct rx_tpa_v2_end_cmpl *th_tpa_end;
		struct rx_tpa_v2_end_cmpl_hi *th_tpa_end1;

		th_tpa_end = (void *)tpa_end;
		th_tpa_end1 = (void *)tpa_end1;
		agg_id = BNXT_TPA_END_AGG_ID_TH(th_tpa_end);
		agg_bufs = BNXT_TPA_END_AGG_BUFS_TH(th_tpa_end1);
		payload_offset = th_tpa_end1->payload_offset;
	} else {
		agg_id = BNXT_TPA_END_AGG_ID(tpa_end);
		agg_bufs = BNXT_TPA_END_AGG_BUFS(tpa_end);
		if (!bnxt_agg_bufs_valid(cpr, agg_bufs, *raw_cp_cons))
			return NULL;
		payload_offset = tpa_end->payload_offset;
	}

	tpa_info = &rxr->tpa_info[agg_id];
	mbuf = tpa_info->mbuf;
	RTE_ASSERT(mbuf != NULL);

	if (agg_bufs) {
		bnxt_rx_pages(rxq, mbuf, raw_cp_cons, agg_bufs, tpa_info);
	}
	mbuf->l4_len = payload_offset;

	struct rte_mbuf *new_data = __bnxt_alloc_rx_data(rxq->mb_pool);
	RTE_ASSERT(new_data != NULL);
	if (!new_data) {
		rte_atomic64_inc(&rxq->rx_mbuf_alloc_fail);
		return NULL;
	}
	tpa_info->mbuf = new_data;

	return mbuf;
}

uint32_t bnxt_ptype_table[BNXT_PTYPE_TBL_DIM] __rte_cache_aligned;

static void __rte_cold
bnxt_init_ptype_table(void)
{
	uint32_t *pt = bnxt_ptype_table;
	static bool initialized;
	int ip6, tun, type;
	uint32_t l3;
	int i;

	if (initialized)
		return;

	for (i = 0; i < BNXT_PTYPE_TBL_DIM; i++) {
		if (i & BNXT_PTYPE_TBL_VLAN_MSK)
			pt[i] = RTE_PTYPE_L2_ETHER_VLAN;
		else
			pt[i] = RTE_PTYPE_L2_ETHER;

		ip6 = !!(i & BNXT_PTYPE_TBL_IP_VER_MSK);
		tun = !!(i & BNXT_PTYPE_TBL_TUN_MSK);
		type = (i & BNXT_PTYPE_TBL_TYPE_MSK) >> BNXT_PTYPE_TBL_TYPE_SFT;

		if (!tun && !ip6)
			l3 = RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
		else if (!tun && ip6)
			l3 = RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;
		else if (tun && !ip6)
			l3 = RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN;
		else
			l3 = RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN;

		switch (type) {
		case BNXT_PTYPE_TBL_TYPE_ICMP:
			if (tun)
				pt[i] |= l3 | RTE_PTYPE_INNER_L4_ICMP;
			else
				pt[i] |= l3 | RTE_PTYPE_L4_ICMP;
			break;
		case BNXT_PTYPE_TBL_TYPE_TCP:
			if (tun)
				pt[i] |= l3 | RTE_PTYPE_INNER_L4_TCP;
			else
				pt[i] |= l3 | RTE_PTYPE_L4_TCP;
			break;
		case BNXT_PTYPE_TBL_TYPE_UDP:
			if (tun)
				pt[i] |= l3 | RTE_PTYPE_INNER_L4_UDP;
			else
				pt[i] |= l3 | RTE_PTYPE_L4_UDP;
			break;
		case BNXT_PTYPE_TBL_TYPE_IP:
			pt[i] |= l3;
			break;
		}
	}
	initialized = true;
}

static uint32_t
bnxt_parse_pkt_type(struct rx_pkt_cmpl *rxcmp, struct rx_pkt_cmpl_hi *rxcmp1)
{
	uint32_t flags_type, flags2;
	uint8_t index;

	flags_type = rte_le_to_cpu_16(rxcmp->flags_type);
	flags2 = rte_le_to_cpu_32(rxcmp1->flags2);

	/* Validate ptype table indexing at build time. */
	bnxt_check_ptype_constants();

	/*
	 * Index format:
	 *     bit 0: Set if IP tunnel encapsulated packet.
	 *     bit 1: Set if IPv6 packet, clear if IPv4.
	 *     bit 2: Set if VLAN tag present.
	 *     bits 3-6: Four-bit hardware packet type field.
	 */
	index = BNXT_CMPL_ITYPE_TO_IDX(flags_type) |
		BNXT_CMPL_VLAN_TUN_TO_IDX(flags2) |
		BNXT_CMPL_IP_VER_TO_IDX(flags2);

	return bnxt_ptype_table[index];
}

static void __rte_cold
bnxt_init_ol_flags_tables(struct bnxt_rx_queue *rxq)
{
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	struct rte_eth_conf *dev_conf;
	bool outer_cksum_enabled;
	uint64_t offloads;
	uint32_t *pt;
	int i;

	dev_conf = &rxq->bp->eth_dev->data->dev_conf;
	offloads = dev_conf->rxmode.offloads;

	outer_cksum_enabled = !!(offloads & (RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |
					     RTE_ETH_RX_OFFLOAD_OUTER_UDP_CKSUM));

	/* Initialize ol_flags table. */
	pt = rxr->ol_flags_table;
	for (i = 0; i < BNXT_OL_FLAGS_TBL_DIM; i++) {
		pt[i] = 0;

		if (BNXT_RX_VLAN_STRIP_EN(rxq->bp)) {
			if (i & RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN)
				pt[i] |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;
		}

		if (i & (RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC << 3)) {
			/* Tunnel case. */
			if (outer_cksum_enabled) {
				if (i & RX_PKT_CMPL_FLAGS2_IP_CS_CALC)
					pt[i] |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;

				if (i & RX_PKT_CMPL_FLAGS2_L4_CS_CALC)
					pt[i] |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;

				if (i & RX_PKT_CMPL_FLAGS2_T_L4_CS_CALC)
					pt[i] |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD;
			} else {
				if (i & RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC)
					pt[i] |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;

				if (i & RX_PKT_CMPL_FLAGS2_T_L4_CS_CALC)
					pt[i] |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
			}
		} else {
			/* Non-tunnel case. */
			if (i & RX_PKT_CMPL_FLAGS2_IP_CS_CALC)
				pt[i] |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;

			if (i & RX_PKT_CMPL_FLAGS2_L4_CS_CALC)
				pt[i] |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
		}
	}

	/* Initialize checksum error table. */
	pt = rxr->ol_flags_err_table;
	for (i = 0; i < BNXT_OL_FLAGS_ERR_TBL_DIM; i++) {
		pt[i] = 0;

		if (i & (RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC << 2)) {
			/* Tunnel case. */
			if (outer_cksum_enabled) {
				if (i & (RX_PKT_CMPL_ERRORS_IP_CS_ERROR >> 4))
					pt[i] |= RTE_MBUF_F_RX_IP_CKSUM_BAD;

				if (i & (RX_PKT_CMPL_ERRORS_T_IP_CS_ERROR >> 4))
					pt[i] |= RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD;

				if (i & (RX_PKT_CMPL_ERRORS_L4_CS_ERROR >> 4))
					pt[i] |= RTE_MBUF_F_RX_L4_CKSUM_BAD;

				if (i & (RX_PKT_CMPL_ERRORS_T_L4_CS_ERROR >> 4))
					pt[i] |= RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD;
			} else {
				if (i & (RX_PKT_CMPL_ERRORS_T_IP_CS_ERROR >> 4))
					pt[i] |= RTE_MBUF_F_RX_IP_CKSUM_BAD;

				if (i & (RX_PKT_CMPL_ERRORS_T_L4_CS_ERROR >> 4))
					pt[i] |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
			}
		} else {
			/* Non-tunnel case. */
			if (i & (RX_PKT_CMPL_ERRORS_IP_CS_ERROR >> 4))
				pt[i] |= RTE_MBUF_F_RX_IP_CKSUM_BAD;

			if (i & (RX_PKT_CMPL_ERRORS_L4_CS_ERROR >> 4))
				pt[i] |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
		}
	}
}

static void
bnxt_set_ol_flags(struct bnxt_rx_ring_info *rxr, struct rx_pkt_cmpl *rxcmp,
		  struct rx_pkt_cmpl_hi *rxcmp1, struct rte_mbuf *mbuf)
{
	uint16_t flags_type, errors, flags;
	uint64_t ol_flags;

	flags_type = rte_le_to_cpu_16(rxcmp->flags_type);

	flags = rte_le_to_cpu_32(rxcmp1->flags2) &
				(RX_PKT_CMPL_FLAGS2_IP_CS_CALC |
				 RX_PKT_CMPL_FLAGS2_L4_CS_CALC |
				 RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC |
				 RX_PKT_CMPL_FLAGS2_T_L4_CS_CALC |
				 RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN);

	flags |= (flags & RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC) << 3;
	errors = rte_le_to_cpu_16(rxcmp1->errors_v2) &
				(RX_PKT_CMPL_ERRORS_IP_CS_ERROR |
				 RX_PKT_CMPL_ERRORS_L4_CS_ERROR |
				 RX_PKT_CMPL_ERRORS_T_IP_CS_ERROR |
				 RX_PKT_CMPL_ERRORS_T_L4_CS_ERROR);
	errors = (errors >> 4) & flags;

	ol_flags = rxr->ol_flags_table[flags & ~errors];

	if (unlikely(errors)) {
		errors |= (flags & RX_PKT_CMPL_FLAGS2_T_IP_CS_CALC) << 2;
		ol_flags |= rxr->ol_flags_err_table[errors];
	}

	if (flags_type & RX_PKT_CMPL_FLAGS_RSS_VALID) {
		mbuf->hash.rss = rte_le_to_cpu_32(rxcmp->rss_hash);
		ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
	}

#ifdef RTE_LIBRTE_IEEE1588
	if (unlikely((flags_type & RX_PKT_CMPL_FLAGS_MASK) ==
		     RX_PKT_CMPL_FLAGS_ITYPE_PTP_W_TIMESTAMP))
		ol_flags |= RTE_MBUF_F_RX_IEEE1588_PTP | RTE_MBUF_F_RX_IEEE1588_TMST;
#endif

	mbuf->ol_flags = ol_flags;
}

#ifdef RTE_LIBRTE_IEEE1588
static void
bnxt_get_rx_ts_p5(struct bnxt *bp, uint32_t rx_ts_cmpl)
{
	struct bnxt_ptp_cfg *ptp = bp->ptp_cfg;
	uint64_t last_hwrm_time;
	uint64_t pkt_time = 0;

	if (!BNXT_CHIP_P5(bp) || !ptp)
		return;

	/* On Thor, Rx timestamps are provided directly in the
	 * Rx completion records to the driver. Only 32 bits of
	 * the timestamp is present in the completion. Driver needs
	 * to read the current 48 bit free running timer using the
	 * HWRM_PORT_TS_QUERY command and combine the upper 16 bits
	 * from the HWRM response with the lower 32 bits in the
	 * Rx completion to produce the 48 bit timestamp for the Rx packet
	 */
	last_hwrm_time = ptp->current_time;
	pkt_time = (last_hwrm_time & BNXT_PTP_CURRENT_TIME_MASK) | rx_ts_cmpl;
	if (rx_ts_cmpl < (uint32_t)last_hwrm_time) {
		/* timer has rolled over */
		pkt_time += (1ULL << 32);
	}
	ptp->rx_timestamp = pkt_time;
}
#endif

static uint32_t
bnxt_ulp_set_mark_in_mbuf(struct bnxt *bp, struct rx_pkt_cmpl_hi *rxcmp1,
			  struct rte_mbuf *mbuf, uint32_t *vfr_flag)
{
	uint32_t cfa_code;
	uint32_t meta_fmt;
	uint32_t meta;
	bool gfid = false;
	uint32_t mark_id;
	uint32_t flags2;
	uint32_t gfid_support = 0;
	int rc;

	if (BNXT_GFID_ENABLED(bp))
		gfid_support = 1;

	cfa_code = rte_le_to_cpu_16(rxcmp1->cfa_code);
	flags2 = rte_le_to_cpu_32(rxcmp1->flags2);
	meta = rte_le_to_cpu_32(rxcmp1->metadata);

	/*
	 * The flags field holds extra bits of info from [6:4]
	 * which indicate if the flow is in TCAM or EM or EEM
	 */
	meta_fmt = (flags2 & BNXT_CFA_META_FMT_MASK) >>
		BNXT_CFA_META_FMT_SHFT;

	switch (meta_fmt) {
	case 0:
		if (gfid_support) {
			/* Not an LFID or GFID, a flush cmd. */
			goto skip_mark;
		} else {
			/* LFID mode, no vlan scenario */
			gfid = false;
		}
		break;
	case 4:
	case 5:
		/*
		 * EM/TCAM case
		 * Assume that EM doesn't support Mark due to GFID
		 * collisions with EEM.  Simply return without setting the mark
		 * in the mbuf.
		 */
		if (BNXT_CFA_META_EM_TEST(meta)) {
			/*This is EM hit {EM(1), GFID[27:16], 19'd0 or vtag } */
			gfid = true;
			meta >>= BNXT_RX_META_CFA_CODE_SHIFT;
			cfa_code |= meta << BNXT_CFA_CODE_META_SHIFT;
		} else {
			/*
			 * It is a TCAM entry, so it is an LFID.
			 * The TCAM IDX and Mode can also be determined
			 * by decoding the meta_data. We are not
			 * using these for now.
			 */
		}
		break;
	case 6:
	case 7:
		/* EEM Case, only using gfid in EEM for now. */
		gfid = true;

		/*
		 * For EEM flows, The first part of cfa_code is 16 bits.
		 * The second part is embedded in the
		 * metadata field from bit 19 onwards. The driver needs to
		 * ignore the first 19 bits of metadata and use the next 12
		 * bits as higher 12 bits of cfa_code.
		 */
		meta >>= BNXT_RX_META_CFA_CODE_SHIFT;
		cfa_code |= meta << BNXT_CFA_CODE_META_SHIFT;
		break;
	default:
		/* For other values, the cfa_code is assumed to be an LFID. */
		break;
	}

	rc = ulp_mark_db_mark_get(bp->ulp_ctx, gfid,
				  cfa_code, vfr_flag, &mark_id);
	if (!rc) {
		/* VF to VFR Rx path. So, skip mark_id injection in mbuf */
		if (vfr_flag && *vfr_flag)
			return mark_id;
		/* Got the mark, write it to the mbuf and return */
		mbuf->hash.fdir.hi = mark_id;
		*bnxt_cfa_code_dynfield(mbuf) = cfa_code & 0xffffffffull;
		mbuf->hash.fdir.id = rxcmp1->cfa_code;
		mbuf->ol_flags |= RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID;
		return mark_id;
	}

skip_mark:
	mbuf->hash.fdir.hi = 0;
	mbuf->hash.fdir.id = 0;

	return 0;
}

void bnxt_set_mark_in_mbuf(struct bnxt *bp,
			   struct rx_pkt_cmpl_hi *rxcmp1,
			   struct rte_mbuf *mbuf)
{
	uint32_t cfa_code = 0;

	if (unlikely(bp->mark_table == NULL))
		return;

	cfa_code = rte_le_to_cpu_16(rxcmp1->cfa_code);
	if (!cfa_code)
		return;

	if (cfa_code && !bp->mark_table[cfa_code].valid)
		return;

	mbuf->hash.fdir.hi = bp->mark_table[cfa_code].mark_id;
	mbuf->ol_flags |= RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID;
}

static int bnxt_rx_pkt(struct rte_mbuf **rx_pkt,
		       struct bnxt_rx_queue *rxq, uint32_t *raw_cons)
{
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	struct rx_pkt_cmpl *rxcmp;
	struct rx_pkt_cmpl_hi *rxcmp1;
	uint32_t tmp_raw_cons = *raw_cons;
	uint16_t cons, raw_prod, cp_cons =
	    RING_CMP(cpr->cp_ring_struct, tmp_raw_cons);
	struct rte_mbuf *mbuf;
	int rc = 0;
	uint8_t agg_buf = 0;
	uint16_t cmp_type;
	uint32_t vfr_flag = 0, mark_id = 0;
	struct bnxt *bp = rxq->bp;

	rxcmp = (struct rx_pkt_cmpl *)
	    &cpr->cp_desc_ring[cp_cons];

	cmp_type = CMP_TYPE(rxcmp);

	if (cmp_type == RX_TPA_V2_ABUF_CMPL_TYPE_RX_TPA_AGG) {
		struct rx_tpa_v2_abuf_cmpl *rx_agg = (void *)rxcmp;
		uint16_t agg_id = rte_cpu_to_le_16(rx_agg->agg_id);
		struct bnxt_tpa_info *tpa_info;

		tpa_info = &rxr->tpa_info[agg_id];
		RTE_ASSERT(tpa_info->agg_count < 16);
		tpa_info->agg_arr[tpa_info->agg_count++] = *rx_agg;
		rc = -EINVAL; /* Continue w/o new mbuf */
		goto next_rx;
	}

	tmp_raw_cons = NEXT_RAW_CMP(tmp_raw_cons);
	cp_cons = RING_CMP(cpr->cp_ring_struct, tmp_raw_cons);
	rxcmp1 = (struct rx_pkt_cmpl_hi *)&cpr->cp_desc_ring[cp_cons];

	if (!bnxt_cpr_cmp_valid(rxcmp1, tmp_raw_cons,
				cpr->cp_ring_struct->ring_size))
		return -EBUSY;

	if (cmp_type == RX_TPA_START_CMPL_TYPE_RX_TPA_START ||
	    cmp_type == RX_TPA_START_V2_CMPL_TYPE_RX_TPA_START_V2) {
		bnxt_tpa_start(rxq, (struct rx_tpa_start_cmpl *)rxcmp,
			       (struct rx_tpa_start_cmpl_hi *)rxcmp1);
		rc = -EINVAL; /* Continue w/o new mbuf */
		goto next_rx;
	} else if (cmp_type == RX_TPA_END_CMPL_TYPE_RX_TPA_END) {
		mbuf = bnxt_tpa_end(rxq, &tmp_raw_cons,
				   (struct rx_tpa_end_cmpl *)rxcmp,
				   (struct rx_tpa_end_cmpl_hi *)rxcmp1);
		if (unlikely(!mbuf))
			return -EBUSY;
		*rx_pkt = mbuf;
		goto next_rx;
	} else if ((cmp_type != CMPL_BASE_TYPE_RX_L2) &&
		   (cmp_type != CMPL_BASE_TYPE_RX_L2_V2)) {
		rc = -EINVAL;
		goto next_rx;
	}

	agg_buf = BNXT_RX_L2_AGG_BUFS(rxcmp);
	if (agg_buf && !bnxt_agg_bufs_valid(cpr, agg_buf, tmp_raw_cons))
		return -EBUSY;

	raw_prod = rxr->rx_raw_prod;

	cons = rxcmp->opaque;
	if (unlikely(cons != rxr->rx_next_cons)) {
		bnxt_discard_rx(bp, cpr, &tmp_raw_cons, rxcmp);
		PMD_DRV_LOG(ERR, "RX cons %x != expected cons %x\n",
			    cons, rxr->rx_next_cons);
		bnxt_sched_ring_reset(rxq);
		rc = -EBUSY;
		goto next_rx;
	}
	mbuf = bnxt_consume_rx_buf(rxr, cons);
	if (mbuf == NULL)
		return -EBUSY;

	mbuf->data_off = RTE_PKTMBUF_HEADROOM;
	mbuf->nb_segs = 1;
	mbuf->next = NULL;
	mbuf->pkt_len = rxcmp->len;
	mbuf->data_len = mbuf->pkt_len;
	mbuf->port = rxq->port_id;

#ifdef RTE_LIBRTE_IEEE1588
	if (unlikely((rte_le_to_cpu_16(rxcmp->flags_type) &
		      RX_PKT_CMPL_FLAGS_MASK) ==
		     RX_PKT_CMPL_FLAGS_ITYPE_PTP_W_TIMESTAMP))
		bnxt_get_rx_ts_p5(rxq->bp, rxcmp1->reorder);
#endif

	if (cmp_type == CMPL_BASE_TYPE_RX_L2_V2) {
		bnxt_parse_csum_v2(mbuf, rxcmp1);
		bnxt_parse_pkt_type_v2(mbuf, rxcmp, rxcmp1);
		bnxt_rx_vlan_v2(mbuf, rxcmp, rxcmp1);
		/* TODO Add support for cfa_code parsing */
		goto reuse_rx_mbuf;
	}

	bnxt_set_ol_flags(rxr, rxcmp, rxcmp1, mbuf);

	mbuf->packet_type = bnxt_parse_pkt_type(rxcmp, rxcmp1);

	bnxt_set_vlan(rxcmp1, mbuf);

	if (BNXT_TRUFLOW_EN(bp))
		mark_id = bnxt_ulp_set_mark_in_mbuf(rxq->bp, rxcmp1, mbuf,
						    &vfr_flag);
	else
		bnxt_set_mark_in_mbuf(rxq->bp, rxcmp1, mbuf);

reuse_rx_mbuf:
	if (agg_buf)
		bnxt_rx_pages(rxq, mbuf, &tmp_raw_cons, agg_buf, NULL);

#ifdef BNXT_DEBUG
	if (rxcmp1->errors_v2 & RX_CMP_L2_ERRORS) {
		/* Re-install the mbuf back to the rx ring */
		bnxt_reuse_rx_mbuf(rxr, cons, mbuf);

		rc = -EIO;
		goto next_rx;
	}
#endif
	/*
	 * TODO: Redesign this....
	 * If the allocation fails, the packet does not get received.
	 * Simply returning this will result in slowly falling behind
	 * on the producer ring buffers.
	 * Instead, "filling up" the producer just before ringing the
	 * doorbell could be a better solution since it will let the
	 * producer ring starve until memory is available again pushing
	 * the drops into hardware and getting them out of the driver
	 * allowing recovery to a full producer ring.
	 *
	 * This could also help with cache usage by preventing per-packet
	 * calls in favour of a tight loop with the same function being called
	 * in it.
	 */
	raw_prod = RING_NEXT(raw_prod);
	if (bnxt_alloc_rx_data(rxq, rxr, raw_prod)) {
		PMD_DRV_LOG(ERR, "mbuf alloc failed with prod=0x%x\n",
			    raw_prod);
		rc = -ENOMEM;
		goto rx;
	}
	rxr->rx_raw_prod = raw_prod;
rx:
	rxr->rx_next_cons = RING_IDX(rxr->rx_ring_struct, RING_NEXT(cons));

	if (BNXT_TRUFLOW_EN(bp) && (BNXT_VF_IS_TRUSTED(bp) || BNXT_PF(bp)) &&
	    vfr_flag) {
		bnxt_vfr_recv(mark_id, rxq->queue_id, mbuf);
		/* Now return an error so that nb_rx_pkts is not
		 * incremented.
		 * This packet was meant to be given to the representor.
		 * So no need to account the packet and give it to
		 * parent Rx burst function.
		 */
		rc = -ENODEV;
		goto next_rx;
	}
	/*
	 * All MBUFs are allocated with the same size under DPDK,
	 * no optimization for rx_copy_thresh
	 */
	*rx_pkt = mbuf;

next_rx:

	*raw_cons = tmp_raw_cons;

	return rc;
}

uint16_t bnxt_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			       uint16_t nb_pkts)
{
	struct bnxt_rx_queue *rxq = rx_queue;
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint16_t rx_raw_prod = rxr->rx_raw_prod;
	uint16_t ag_raw_prod = rxr->ag_raw_prod;
	uint32_t raw_cons = cpr->cp_raw_cons;
	bool alloc_failed = false;
	uint32_t cons;
	int nb_rx_pkts = 0;
	int nb_rep_rx_pkts = 0;
	struct rx_pkt_cmpl *rxcmp;
	int rc = 0;
	bool evt = false;

	if (unlikely(is_bnxt_in_error(rxq->bp)))
		return 0;

	/* If Rx Q was stopped return */
	if (unlikely(!rxq->rx_started))
		return 0;

#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM64)
	/*
	 * Replenish buffers if needed when a transition has been made from
	 * vector- to non-vector- receive processing.
	 */
	while (unlikely(rxq->rxrearm_nb)) {
		if (!bnxt_alloc_rx_data(rxq, rxr, rxq->rxrearm_start)) {
			rxr->rx_raw_prod = rxq->rxrearm_start;
			bnxt_db_write(&rxr->rx_db, rxr->rx_raw_prod);
			rxq->rxrearm_start++;
			rxq->rxrearm_nb--;
		} else {
			/* Retry allocation on next call. */
			break;
		}
	}
#endif

	/* Handle RX burst request */
	while (1) {
		cons = RING_CMP(cpr->cp_ring_struct, raw_cons);
		rxcmp = (struct rx_pkt_cmpl *)&cpr->cp_desc_ring[cons];

		if (!bnxt_cpr_cmp_valid(rxcmp, raw_cons,
					cpr->cp_ring_struct->ring_size))
			break;
		if (CMP_TYPE(rxcmp) == CMPL_BASE_TYPE_HWRM_DONE) {
			PMD_DRV_LOG(ERR, "Rx flush done\n");
		} else if ((CMP_TYPE(rxcmp) >= CMPL_BASE_TYPE_RX_TPA_START_V2) &&
		     (CMP_TYPE(rxcmp) <= RX_TPA_V2_ABUF_CMPL_TYPE_RX_TPA_AGG)) {
			rc = bnxt_rx_pkt(&rx_pkts[nb_rx_pkts], rxq, &raw_cons);
			if (!rc)
				nb_rx_pkts++;
			else if (rc == -EBUSY)	/* partial completion */
				break;
			else if (rc == -ENODEV)	/* completion for representor */
				nb_rep_rx_pkts++;
			else if (rc == -ENOMEM) {
				nb_rx_pkts++;
				alloc_failed = true;
			}
		} else if (!BNXT_NUM_ASYNC_CPR(rxq->bp)) {
			evt =
			bnxt_event_hwrm_resp_handler(rxq->bp,
						     (struct cmpl_base *)rxcmp);
			/* If the async event is Fatal error, return */
			if (unlikely(is_bnxt_in_error(rxq->bp)))
				goto done;
		}

		raw_cons = NEXT_RAW_CMP(raw_cons);
		if (nb_rx_pkts == nb_pkts || nb_rep_rx_pkts == nb_pkts || evt)
			break;
	}

	cpr->cp_raw_cons = raw_cons;
	if (!nb_rx_pkts && !nb_rep_rx_pkts && !evt) {
		/*
		 * For PMD, there is no need to keep on pushing to REARM
		 * the doorbell if there are no new completions
		 */
		goto done;
	}

	/* Ring the completion queue doorbell. */
	bnxt_db_cq(cpr);

	/* Ring the receive descriptor doorbell. */
	if (rx_raw_prod != rxr->rx_raw_prod)
		bnxt_db_write(&rxr->rx_db, rxr->rx_raw_prod);

	/* Ring the AGG ring DB */
	if (ag_raw_prod != rxr->ag_raw_prod)
		bnxt_db_write(&rxr->ag_db, rxr->ag_raw_prod);

	/* Attempt to alloc Rx buf in case of a previous allocation failure. */
	if (alloc_failed) {
		int cnt;

		rx_raw_prod = RING_NEXT(rx_raw_prod);
		for (cnt = 0; cnt < nb_rx_pkts + nb_rep_rx_pkts; cnt++) {
			struct rte_mbuf **rx_buf;
			uint16_t ndx;

			ndx = RING_IDX(rxr->rx_ring_struct, rx_raw_prod + cnt);
			rx_buf = &rxr->rx_buf_ring[ndx];

			/* Buffer already allocated for this index. */
			if (*rx_buf != NULL && *rx_buf != &rxq->fake_mbuf)
				continue;

			/* This slot is empty. Alloc buffer for Rx */
			if (!bnxt_alloc_rx_data(rxq, rxr, rx_raw_prod + cnt)) {
				rxr->rx_raw_prod = rx_raw_prod + cnt;
				bnxt_db_write(&rxr->rx_db, rxr->rx_raw_prod);
			} else {
				PMD_DRV_LOG(ERR, "Alloc  mbuf failed\n");
				break;
			}
		}
	}

done:
	return nb_rx_pkts;
}

void bnxt_free_rx_rings(struct bnxt *bp)
{
	int i;
	struct bnxt_rx_queue *rxq;

	if (!bp->rx_queues)
		return;

	for (i = 0; i < (int)bp->rx_nr_rings; i++) {
		rxq = bp->rx_queues[i];
		if (!rxq)
			continue;

		bnxt_free_ring(rxq->rx_ring->rx_ring_struct);
		rte_free(rxq->rx_ring->rx_ring_struct);

		/* Free the Aggregator ring */
		bnxt_free_ring(rxq->rx_ring->ag_ring_struct);
		rte_free(rxq->rx_ring->ag_ring_struct);
		rxq->rx_ring->ag_ring_struct = NULL;

		rte_free(rxq->rx_ring);

		bnxt_free_ring(rxq->cp_ring->cp_ring_struct);
		rte_free(rxq->cp_ring->cp_ring_struct);
		rte_free(rxq->cp_ring);

		rte_memzone_free(rxq->mz);
		rxq->mz = NULL;

		rte_free(rxq);
		bp->rx_queues[i] = NULL;
	}
}

int bnxt_init_rx_ring_struct(struct bnxt_rx_queue *rxq, unsigned int socket_id)
{
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_ring *ring;

	rxq->rx_buf_size = BNXT_MAX_PKT_LEN + sizeof(struct rte_mbuf);

	if (rxq->rx_ring != NULL) {
		rxr = rxq->rx_ring;
	} else {

		rxr = rte_zmalloc_socket("bnxt_rx_ring",
					 sizeof(struct bnxt_rx_ring_info),
					 RTE_CACHE_LINE_SIZE, socket_id);
		if (rxr == NULL)
			return -ENOMEM;
		rxq->rx_ring = rxr;
	}

	if (rxr->rx_ring_struct == NULL) {
		ring = rte_zmalloc_socket("bnxt_rx_ring_struct",
					   sizeof(struct bnxt_ring),
					   RTE_CACHE_LINE_SIZE, socket_id);
		if (ring == NULL)
			return -ENOMEM;
		rxr->rx_ring_struct = ring;
		ring->ring_size = rte_align32pow2(rxq->nb_rx_desc);
		ring->ring_mask = ring->ring_size - 1;
		ring->bd = (void *)rxr->rx_desc_ring;
		ring->bd_dma = rxr->rx_desc_mapping;

		/* Allocate extra rx ring entries for vector rx. */
		ring->vmem_size = sizeof(struct rte_mbuf *) *
				  (ring->ring_size + BNXT_RX_EXTRA_MBUF_ENTRIES);

		ring->vmem = (void **)&rxr->rx_buf_ring;
		ring->fw_ring_id = INVALID_HW_RING_ID;
	}

	if (rxq->cp_ring != NULL) {
		cpr = rxq->cp_ring;
	} else {
		cpr = rte_zmalloc_socket("bnxt_rx_ring",
					 sizeof(struct bnxt_cp_ring_info),
					 RTE_CACHE_LINE_SIZE, socket_id);
		if (cpr == NULL)
			return -ENOMEM;
		rxq->cp_ring = cpr;
	}

	if (cpr->cp_ring_struct == NULL) {
		ring = rte_zmalloc_socket("bnxt_rx_ring_struct",
					   sizeof(struct bnxt_ring),
					   RTE_CACHE_LINE_SIZE, socket_id);
		if (ring == NULL)
			return -ENOMEM;
		cpr->cp_ring_struct = ring;

		/* Allocate two completion slots per entry in desc ring. */
		ring->ring_size = rxr->rx_ring_struct->ring_size * 2;
		if (bnxt_need_agg_ring(rxq->bp->eth_dev))
			ring->ring_size *= AGG_RING_SIZE_FACTOR;

		ring->ring_size = rte_align32pow2(ring->ring_size);
		ring->ring_mask = ring->ring_size - 1;
		ring->bd = (void *)cpr->cp_desc_ring;
		ring->bd_dma = cpr->cp_desc_mapping;
		ring->vmem_size = 0;
		ring->vmem = NULL;
		ring->fw_ring_id = INVALID_HW_RING_ID;
	}

	if (!bnxt_need_agg_ring(rxq->bp->eth_dev))
		return 0;

	rxr = rxq->rx_ring;
	/* Allocate Aggregator rings */
	ring = rte_zmalloc_socket("bnxt_rx_ring_struct",
				   sizeof(struct bnxt_ring),
				   RTE_CACHE_LINE_SIZE, socket_id);
	if (ring == NULL)
		return -ENOMEM;
	rxr->ag_ring_struct = ring;
	ring->ring_size = rte_align32pow2(rxq->nb_rx_desc *
					  AGG_RING_SIZE_FACTOR);
	ring->ring_mask = ring->ring_size - 1;
	ring->bd = (void *)rxr->ag_desc_ring;
	ring->bd_dma = rxr->ag_desc_mapping;
	ring->vmem_size = ring->ring_size * sizeof(struct rte_mbuf *);
	ring->vmem = (void **)&rxr->ag_buf_ring;
	ring->fw_ring_id = INVALID_HW_RING_ID;

	return 0;
}

static void bnxt_init_rxbds(struct bnxt_ring *ring, uint32_t type,
			    uint16_t len)
{
	uint32_t j;
	struct rx_prod_pkt_bd *rx_bd_ring = (struct rx_prod_pkt_bd *)ring->bd;

	if (!rx_bd_ring)
		return;
	for (j = 0; j < ring->ring_size; j++) {
		rx_bd_ring[j].flags_type = rte_cpu_to_le_16(type);
		rx_bd_ring[j].len = rte_cpu_to_le_16(len);
		rx_bd_ring[j].opaque = j;
	}
}

int bnxt_init_one_rx_ring(struct bnxt_rx_queue *rxq)
{
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_ring *ring;
	uint32_t raw_prod, type;
	unsigned int i;
	uint16_t size;

	/* Initialize packet type table. */
	bnxt_init_ptype_table();

	size = rte_pktmbuf_data_room_size(rxq->mb_pool) - RTE_PKTMBUF_HEADROOM;
	size = RTE_MIN(BNXT_MAX_PKT_LEN, size);

	type = RX_PROD_PKT_BD_TYPE_RX_PROD_PKT;

	rxr = rxq->rx_ring;
	ring = rxr->rx_ring_struct;
	bnxt_init_rxbds(ring, type, size);

	/* Initialize offload flags parsing table. */
	bnxt_init_ol_flags_tables(rxq);

	raw_prod = rxr->rx_raw_prod;
	for (i = 0; i < ring->ring_size; i++) {
		if (unlikely(!rxr->rx_buf_ring[i])) {
			if (bnxt_alloc_rx_data(rxq, rxr, raw_prod) != 0) {
				PMD_DRV_LOG(WARNING,
					    "RxQ %d allocated %d of %d mbufs\n",
					    rxq->queue_id, i, ring->ring_size);
				return -ENOMEM;
			}
		}
		rxr->rx_raw_prod = raw_prod;
		raw_prod = RING_NEXT(raw_prod);
	}

	/* Initialize dummy mbuf pointers for vector mode rx. */
	for (i = ring->ring_size;
	     i < ring->ring_size + BNXT_RX_EXTRA_MBUF_ENTRIES; i++) {
		rxr->rx_buf_ring[i] = &rxq->fake_mbuf;
	}

	/* Explicitly reset this driver internal tracker on a ring init */
	rxr->rx_next_cons = 0;

	if (!bnxt_need_agg_ring(rxq->bp->eth_dev))
		return 0;

	ring = rxr->ag_ring_struct;
	type = RX_PROD_AGG_BD_TYPE_RX_PROD_AGG;
	bnxt_init_rxbds(ring, type, size);
	raw_prod = rxr->ag_raw_prod;

	for (i = 0; i < ring->ring_size; i++) {
		if (unlikely(!rxr->ag_buf_ring[i])) {
			if (bnxt_alloc_ag_data(rxq, rxr, raw_prod) != 0) {
				PMD_DRV_LOG(WARNING,
					    "RxQ %d allocated %d of %d mbufs\n",
					    rxq->queue_id, i, ring->ring_size);
				return -ENOMEM;
			}
		}
		rxr->ag_raw_prod = raw_prod;
		raw_prod = RING_NEXT(raw_prod);
	}
	PMD_DRV_LOG(DEBUG, "AGG Done!\n");

	if (rxr->tpa_info) {
		unsigned int max_aggs = BNXT_TPA_MAX_AGGS(rxq->bp);

		for (i = 0; i < max_aggs; i++) {
			if (unlikely(!rxr->tpa_info[i].mbuf)) {
				rxr->tpa_info[i].mbuf =
					__bnxt_alloc_rx_data(rxq->mb_pool);
				if (!rxr->tpa_info[i].mbuf) {
					rte_atomic64_inc(&rxq->rx_mbuf_alloc_fail);
					return -ENOMEM;
				}
			}
		}
	}
	PMD_DRV_LOG(DEBUG, "TPA alloc Done!\n");

	return 0;
}

/* Sweep the Rx completion queue till HWRM_DONE for ring flush is received.
 * The mbufs will not be freed in this call.
 * They will be freed during ring free as a part of mem cleanup.
 */
int bnxt_flush_rx_cmp(struct bnxt_cp_ring_info *cpr)
{
	struct bnxt_ring *cp_ring_struct = cpr->cp_ring_struct;
	uint32_t ring_mask = cp_ring_struct->ring_mask;
	uint32_t raw_cons = cpr->cp_raw_cons;
	struct rx_pkt_cmpl *rxcmp;
	uint32_t nb_rx = 0;
	uint32_t cons;

	do {
		cons = RING_CMP(cpr->cp_ring_struct, raw_cons);
		rxcmp = (struct rx_pkt_cmpl *)&cpr->cp_desc_ring[cons];

		if (!bnxt_cpr_cmp_valid(rxcmp, raw_cons, ring_mask + 1))
			break;

		if (CMP_TYPE(rxcmp) == CMPL_BASE_TYPE_HWRM_DONE)
			return 1;

		raw_cons = NEXT_RAW_CMP(raw_cons);
		nb_rx++;
	} while (nb_rx < ring_mask);

	cpr->cp_raw_cons = raw_cons;

	/* Ring the completion queue doorbell. */
	bnxt_db_cq(cpr);

	return 0;
}
