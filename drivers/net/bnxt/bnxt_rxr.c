/*-
 *   BSD LICENSE
 *
 *   Copyright(c) Broadcom Limited.
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
 *     * Neither the name of Broadcom Corporation nor the names of its
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

#include <inttypes.h>
#include <stdbool.h>

#include <rte_bitmap.h>
#include <rte_byteorder.h>
#include <rte_malloc.h>
#include <rte_memory.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_ring.h"
#include "bnxt_rxr.h"
#include "bnxt_rxq.h"
#include "hsi_struct_def_dpdk.h"

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
				     uint16_t prod)
{
	struct rx_prod_pkt_bd *rxbd = &rxr->rx_desc_ring[prod];
	struct bnxt_sw_rx_bd *rx_buf = &rxr->rx_buf_ring[prod];
	struct rte_mbuf *data;

	data = __bnxt_alloc_rx_data(rxq->mb_pool);
	if (!data) {
		rte_atomic64_inc(&rxq->bp->rx_mbuf_alloc_fail);
		return -ENOMEM;
	}

	rx_buf->mbuf = data;

	rxbd->addr = rte_cpu_to_le_64(RTE_MBUF_DATA_DMA_ADDR(rx_buf->mbuf));

	return 0;
}

static inline int bnxt_alloc_ag_data(struct bnxt_rx_queue *rxq,
				     struct bnxt_rx_ring_info *rxr,
				     uint16_t prod)
{
	struct rx_prod_pkt_bd *rxbd = &rxr->ag_desc_ring[prod];
	struct bnxt_sw_rx_bd *rx_buf = &rxr->ag_buf_ring[prod];
	struct rte_mbuf *data;

	data = __bnxt_alloc_rx_data(rxq->mb_pool);
	if (!data) {
		rte_atomic64_inc(&rxq->bp->rx_mbuf_alloc_fail);
		return -ENOMEM;
	}

	if (rxbd == NULL)
		RTE_LOG(ERR, PMD, "Jumbo Frame. rxbd is NULL\n");
	if (rx_buf == NULL)
		RTE_LOG(ERR, PMD, "Jumbo Frame. rx_buf is NULL\n");


	rx_buf->mbuf = data;

	rxbd->addr = rte_cpu_to_le_64(RTE_MBUF_DATA_DMA_ADDR(rx_buf->mbuf));

	return 0;
}

static inline void bnxt_reuse_rx_mbuf(struct bnxt_rx_ring_info *rxr,
			       struct rte_mbuf *mbuf)
{
	uint16_t prod = RING_NEXT(rxr->rx_ring_struct, rxr->rx_prod);
	struct bnxt_sw_rx_bd *prod_rx_buf;
	struct rx_prod_pkt_bd *prod_bd;

	prod_rx_buf = &rxr->rx_buf_ring[prod];

	RTE_ASSERT(prod_rx_buf->mbuf == NULL);
	RTE_ASSERT(mbuf != NULL);

	prod_rx_buf->mbuf = mbuf;

	prod_bd = &rxr->rx_desc_ring[prod];

	prod_bd->addr = rte_cpu_to_le_64(RTE_MBUF_DATA_DMA_ADDR(mbuf));

	rxr->rx_prod = prod;
}

#ifdef BNXT_DEBUG
static void bnxt_reuse_ag_mbuf(struct bnxt_rx_ring_info *rxr, uint16_t cons,
			       struct rte_mbuf *mbuf)
{
	uint16_t prod = rxr->ag_prod;
	struct bnxt_sw_rx_bd *prod_rx_buf;
	struct rx_prod_pkt_bd *prod_bd, *cons_bd;

	prod_rx_buf = &rxr->ag_buf_ring[prod];

	prod_rx_buf->mbuf = mbuf;

	prod_bd = &rxr->ag_desc_ring[prod];
	cons_bd = &rxr->ag_desc_ring[cons];

	prod_bd->addr = cons_bd->addr;
}
#endif

static inline
struct rte_mbuf *bnxt_consume_rx_buf(struct bnxt_rx_ring_info *rxr,
				     uint16_t cons)
{
	struct bnxt_sw_rx_bd *cons_rx_buf;
	struct rte_mbuf *mbuf;

	cons_rx_buf = &rxr->rx_buf_ring[cons];
	RTE_ASSERT(cons_rx_buf->mbuf != NULL);
	mbuf = cons_rx_buf->mbuf;
	cons_rx_buf->mbuf = NULL;
	return mbuf;
}

static void bnxt_tpa_start(struct bnxt_rx_queue *rxq,
			   struct rx_tpa_start_cmpl *tpa_start,
			   struct rx_tpa_start_cmpl_hi *tpa_start1)
{
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint8_t agg_id = rte_le_to_cpu_32(tpa_start->agg_id &
		RX_TPA_START_CMPL_AGG_ID_MASK) >> RX_TPA_START_CMPL_AGG_ID_SFT;
	uint16_t data_cons;
	struct bnxt_tpa_info *tpa_info;
	struct rte_mbuf *mbuf;

	data_cons = tpa_start->opaque;
	tpa_info = &rxr->tpa_info[agg_id];

	mbuf = bnxt_consume_rx_buf(rxr, data_cons);

	bnxt_reuse_rx_mbuf(rxr, tpa_info->mbuf);

	tpa_info->mbuf = mbuf;
	tpa_info->len = rte_le_to_cpu_32(tpa_start->len);

	mbuf->nb_segs = 1;
	mbuf->next = NULL;
	mbuf->pkt_len = rte_le_to_cpu_32(tpa_start->len);
	mbuf->data_len = mbuf->pkt_len;
	mbuf->port = rxq->port_id;
	mbuf->ol_flags = PKT_RX_LRO;
	if (likely(tpa_start->flags_type &
		   rte_cpu_to_le_32(RX_TPA_START_CMPL_FLAGS_RSS_VALID))) {
		mbuf->hash.rss = rte_le_to_cpu_32(tpa_start->rss_hash);
		mbuf->ol_flags |= PKT_RX_RSS_HASH;
	} else {
		mbuf->hash.fdir.id = rte_le_to_cpu_16(tpa_start1->cfa_code);
		mbuf->ol_flags |= PKT_RX_FDIR | PKT_RX_FDIR_ID;
	}
	if (tpa_start1->flags2 &
	    rte_cpu_to_le_32(RX_TPA_START_CMPL_FLAGS2_META_FORMAT_VLAN)) {
		mbuf->vlan_tci = rte_le_to_cpu_32(tpa_start1->metadata);
		mbuf->ol_flags |= PKT_RX_VLAN_PKT;
	}
	if (likely(tpa_start1->flags2 &
		   rte_cpu_to_le_32(RX_TPA_START_CMPL_FLAGS2_L4_CS_CALC)))
		mbuf->ol_flags |= PKT_RX_L4_CKSUM_GOOD;

	/* recycle next mbuf */
	data_cons = RING_NEXT(rxr->rx_ring_struct, data_cons);
	bnxt_reuse_rx_mbuf(rxr, bnxt_consume_rx_buf(rxr, data_cons));
}

static int bnxt_agg_bufs_valid(struct bnxt_cp_ring_info *cpr,
		uint8_t agg_bufs, uint32_t raw_cp_cons)
{
	uint16_t last_cp_cons;
	struct rx_pkt_cmpl *agg_cmpl;

	raw_cp_cons = ADV_RAW_CMP(raw_cp_cons, agg_bufs);
	last_cp_cons = RING_CMP(cpr->cp_ring_struct, raw_cp_cons);
	agg_cmpl = (struct rx_pkt_cmpl *)&cpr->cp_desc_ring[last_cp_cons];
	return CMP_VALID(agg_cmpl, raw_cp_cons, cpr->cp_ring_struct);
}

/* TPA consume agg buffer out of order, allocate connected data only */
static int bnxt_prod_ag_mbuf(struct bnxt_rx_queue *rxq)
{
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint16_t next = RING_NEXT(rxr->ag_ring_struct, rxr->ag_prod);

	/* TODO batch allocation for better performance */
	while (rte_bitmap_get(rxr->ag_bitmap, next)) {
		if (unlikely(bnxt_alloc_ag_data(rxq, rxr, next))) {
			RTE_LOG(ERR, PMD,
				"agg mbuf alloc failed: prod=0x%x\n", next);
			break;
		}
		rte_bitmap_clear(rxr->ag_bitmap, next);
		rxr->ag_prod = next;
		next = RING_NEXT(rxr->ag_ring_struct, next);
	}

	return 0;
}

static int bnxt_rx_pages(struct bnxt_rx_queue *rxq,
			 struct rte_mbuf *mbuf, uint32_t *tmp_raw_cons,
			 uint8_t agg_buf)
{
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	int i;
	uint16_t cp_cons, ag_cons;
	struct rx_pkt_cmpl *rxcmp;
	struct rte_mbuf *last = mbuf;

	for (i = 0; i < agg_buf; i++) {
		struct bnxt_sw_rx_bd *ag_buf;
		struct rte_mbuf *ag_mbuf;
		*tmp_raw_cons = NEXT_RAW_CMP(*tmp_raw_cons);
		cp_cons = RING_CMP(cpr->cp_ring_struct, *tmp_raw_cons);
		rxcmp = (struct rx_pkt_cmpl *)
					&cpr->cp_desc_ring[cp_cons];

#ifdef BNXT_DEBUG
		bnxt_dump_cmpl(cp_cons, rxcmp);
#endif

		ag_cons = rxcmp->opaque;
		RTE_ASSERT(ag_cons <= rxr->ag_ring_struct->ring_mask);
		ag_buf = &rxr->ag_buf_ring[ag_cons];
		ag_mbuf = ag_buf->mbuf;
		RTE_ASSERT(ag_mbuf != NULL);

		ag_mbuf->data_len = rte_le_to_cpu_16(rxcmp->len);

		mbuf->nb_segs++;
		mbuf->pkt_len += ag_mbuf->data_len;

		last->next = ag_mbuf;
		last = ag_mbuf;

		ag_buf->mbuf = NULL;

		/*
		 * As aggregation buffer consumed out of order in TPA module,
		 * use bitmap to track freed slots to be allocated and notified
		 * to NIC
		 */
		rte_bitmap_set(rxr->ag_bitmap, ag_cons);
	}
	bnxt_prod_ag_mbuf(rxq);
	return 0;
}

static inline struct rte_mbuf *bnxt_tpa_end(
		struct bnxt_rx_queue *rxq,
		uint32_t *raw_cp_cons,
		struct rx_tpa_end_cmpl *tpa_end,
		struct rx_tpa_end_cmpl_hi *tpa_end1 __rte_unused)
{
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	uint8_t agg_id = (tpa_end->agg_id & RX_TPA_END_CMPL_AGG_ID_MASK)
			>> RX_TPA_END_CMPL_AGG_ID_SFT;
	struct rte_mbuf *mbuf;
	uint8_t agg_bufs;
	struct bnxt_tpa_info *tpa_info;

	tpa_info = &rxr->tpa_info[agg_id];
	mbuf = tpa_info->mbuf;
	RTE_ASSERT(mbuf != NULL);

	rte_prefetch0(mbuf);
	agg_bufs = (rte_le_to_cpu_32(tpa_end->agg_bufs_v1) &
		RX_TPA_END_CMPL_AGG_BUFS_MASK) >> RX_TPA_END_CMPL_AGG_BUFS_SFT;
	if (agg_bufs) {
		if (!bnxt_agg_bufs_valid(cpr, agg_bufs, *raw_cp_cons))
			return NULL;
		bnxt_rx_pages(rxq, mbuf, raw_cp_cons, agg_bufs);
	}
	mbuf->l4_len = tpa_end->payload_offset;

	struct rte_mbuf *new_data = __bnxt_alloc_rx_data(rxq->mb_pool);
	RTE_ASSERT(new_data != NULL);
	if (!new_data) {
		rte_atomic64_inc(&rxq->bp->rx_mbuf_alloc_fail);
		return NULL;
	}
	tpa_info->mbuf = new_data;

	return mbuf;
}

static int bnxt_rx_pkt(struct rte_mbuf **rx_pkt,
			    struct bnxt_rx_queue *rxq, uint32_t *raw_cons)
{
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	struct rx_pkt_cmpl *rxcmp;
	struct rx_pkt_cmpl_hi *rxcmp1;
	uint32_t tmp_raw_cons = *raw_cons;
	uint16_t cons, prod, cp_cons =
	    RING_CMP(cpr->cp_ring_struct, tmp_raw_cons);
#ifdef BNXT_DEBUG
	uint16_t ag_cons;
#endif
	struct rte_mbuf *mbuf;
	int rc = 0;
	uint8_t agg_buf = 0;
	uint16_t cmp_type;

	rxcmp = (struct rx_pkt_cmpl *)
	    &cpr->cp_desc_ring[cp_cons];

	tmp_raw_cons = NEXT_RAW_CMP(tmp_raw_cons);
	cp_cons = RING_CMP(cpr->cp_ring_struct, tmp_raw_cons);
	rxcmp1 = (struct rx_pkt_cmpl_hi *)&cpr->cp_desc_ring[cp_cons];

	if (!CMP_VALID(rxcmp1, tmp_raw_cons, cpr->cp_ring_struct))
		return -EBUSY;

	cmp_type = CMP_TYPE(rxcmp);
	if (cmp_type == RX_PKT_CMPL_TYPE_RX_L2_TPA_START) {
		bnxt_tpa_start(rxq, (struct rx_tpa_start_cmpl *)rxcmp,
			       (struct rx_tpa_start_cmpl_hi *)rxcmp1);
		rc = -EINVAL; /* Continue w/o new mbuf */
		goto next_rx;
	} else if (cmp_type == RX_PKT_CMPL_TYPE_RX_L2_TPA_END) {
		mbuf = bnxt_tpa_end(rxq, &tmp_raw_cons,
				   (struct rx_tpa_end_cmpl *)rxcmp,
				   (struct rx_tpa_end_cmpl_hi *)rxcmp1);
		if (unlikely(!mbuf))
			return -EBUSY;
		*rx_pkt = mbuf;
		goto next_rx;
	} else if (cmp_type != 0x11) {
		rc = -EINVAL;
		goto next_rx;
	}

	agg_buf = (rxcmp->agg_bufs_v1 & RX_PKT_CMPL_AGG_BUFS_MASK)
			>> RX_PKT_CMPL_AGG_BUFS_SFT;
	if (agg_buf && !bnxt_agg_bufs_valid(cpr, agg_buf, tmp_raw_cons))
		return -EBUSY;

	prod = rxr->rx_prod;

	cons = rxcmp->opaque;
	mbuf = bnxt_consume_rx_buf(rxr, cons);
	rte_prefetch0(mbuf);

	if (mbuf == NULL)
		return -ENOMEM;

	mbuf->nb_segs = 1;
	mbuf->next = NULL;
	mbuf->pkt_len = rxcmp->len;
	mbuf->data_len = mbuf->pkt_len;
	mbuf->port = rxq->port_id;
	mbuf->ol_flags = 0;
	if (rxcmp->flags_type & RX_PKT_CMPL_FLAGS_RSS_VALID) {
		mbuf->hash.rss = rxcmp->rss_hash;
		mbuf->ol_flags |= PKT_RX_RSS_HASH;
	} else {
		mbuf->hash.fdir.id = rxcmp1->cfa_code;
		mbuf->ol_flags |= PKT_RX_FDIR | PKT_RX_FDIR_ID;
	}

	if (agg_buf)
		bnxt_rx_pages(rxq, mbuf, &tmp_raw_cons, agg_buf);

	if (rxcmp1->flags2 & RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN) {
		mbuf->vlan_tci = rxcmp1->metadata &
			(RX_PKT_CMPL_METADATA_VID_MASK |
			RX_PKT_CMPL_METADATA_DE |
			RX_PKT_CMPL_METADATA_PRI_MASK);
		mbuf->ol_flags |= PKT_RX_VLAN_PKT;
	}

#ifdef BNXT_DEBUG
	if (rxcmp1->errors_v2 & RX_CMP_L2_ERRORS) {
		/* Re-install the mbuf back to the rx ring */
		bnxt_reuse_rx_mbuf(rxr, cons, mbuf);
		if (agg_buf)
			bnxt_reuse_ag_mbuf(rxr, ag_cons, mbuf);

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
	prod = RING_NEXT(rxr->rx_ring_struct, prod);
	if (bnxt_alloc_rx_data(rxq, rxr, prod)) {
		RTE_LOG(ERR, PMD, "mbuf alloc failed with prod=0x%x\n", prod);
		rc = -ENOMEM;
	}
	rxr->rx_prod = prod;
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
	uint32_t raw_cons = cpr->cp_raw_cons;
	uint32_t cons;
	int nb_rx_pkts = 0;
	struct rx_pkt_cmpl *rxcmp;
	uint16_t prod = rxr->rx_prod;
	uint16_t ag_prod = rxr->ag_prod;

	/* Handle RX burst request */
	while (1) {
		int rc;

		cons = RING_CMP(cpr->cp_ring_struct, raw_cons);
		rte_prefetch0(&cpr->cp_desc_ring[cons]);
		rxcmp = (struct rx_pkt_cmpl *)&cpr->cp_desc_ring[cons];

		if (!CMP_VALID(rxcmp, raw_cons, cpr->cp_ring_struct))
			break;

		/* TODO: Avoid magic numbers... */
		if ((CMP_TYPE(rxcmp) & 0x30) == 0x10) {
			rc = bnxt_rx_pkt(&rx_pkts[nb_rx_pkts], rxq, &raw_cons);
			if (likely(!rc))
				nb_rx_pkts++;
			if (rc == -EBUSY)	/* partial completion */
				break;
		}
		raw_cons = NEXT_RAW_CMP(raw_cons);
		if (nb_rx_pkts == nb_pkts)
			break;
	}

	cpr->cp_raw_cons = raw_cons;
	if (prod == rxr->rx_prod && ag_prod == rxr->ag_prod) {
		/*
		 * For PMD, there is no need to keep on pushing to REARM
		 * the doorbell if there are no new completions
		 */
		return nb_rx_pkts;
	}

	B_CP_DIS_DB(cpr, cpr->cp_raw_cons);
	B_RX_DB(rxr->rx_doorbell, rxr->rx_prod);
	/* Ring the AGG ring DB */
	B_RX_DB(rxr->ag_doorbell, rxr->ag_prod);
	return nb_rx_pkts;
}

void bnxt_free_rx_rings(struct bnxt *bp)
{
	int i;

	for (i = 0; i < (int)bp->rx_nr_rings; i++) {
		struct bnxt_rx_queue *rxq = bp->rx_queues[i];

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

		rte_free(rxq);
		bp->rx_queues[i] = NULL;
	}
}

int bnxt_init_rx_ring_struct(struct bnxt_rx_queue *rxq, unsigned int socket_id)
{
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_ring *ring;

	rxq->rx_buf_use_size = BNXT_MAX_MTU + ETHER_HDR_LEN + ETHER_CRC_LEN +
			       (2 * VLAN_TAG_SIZE);
	rxq->rx_buf_size = rxq->rx_buf_use_size + sizeof(struct rte_mbuf);

	rxr = rte_zmalloc_socket("bnxt_rx_ring",
				 sizeof(struct bnxt_rx_ring_info),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxr == NULL)
		return -ENOMEM;
	rxq->rx_ring = rxr;

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
	ring->vmem_size = ring->ring_size * sizeof(struct bnxt_sw_rx_bd);
	ring->vmem = (void **)&rxr->rx_buf_ring;

	cpr = rte_zmalloc_socket("bnxt_rx_ring",
				 sizeof(struct bnxt_cp_ring_info),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (cpr == NULL)
		return -ENOMEM;
	rxq->cp_ring = cpr;

	ring = rte_zmalloc_socket("bnxt_rx_ring_struct",
				   sizeof(struct bnxt_ring),
				   RTE_CACHE_LINE_SIZE, socket_id);
	if (ring == NULL)
		return -ENOMEM;
	cpr->cp_ring_struct = ring;
	ring->ring_size = rte_align32pow2(rxr->rx_ring_struct->ring_size *
					  (2 + AGG_RING_SIZE_FACTOR));
	ring->ring_mask = ring->ring_size - 1;
	ring->bd = (void *)cpr->cp_desc_ring;
	ring->bd_dma = cpr->cp_desc_mapping;
	ring->vmem_size = 0;
	ring->vmem = NULL;

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
	ring->vmem_size = ring->ring_size * sizeof(struct bnxt_sw_rx_bd);
	ring->vmem = (void **)&rxr->ag_buf_ring;

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
	uint32_t prod, type;
	unsigned int i;
	uint16_t size;

	size = rte_pktmbuf_data_room_size(rxq->mb_pool) - RTE_PKTMBUF_HEADROOM;
	if (rxq->rx_buf_use_size <= size)
		size = rxq->rx_buf_use_size;

	type = RX_PROD_PKT_BD_TYPE_RX_PROD_PKT;

	rxr = rxq->rx_ring;
	ring = rxr->rx_ring_struct;
	bnxt_init_rxbds(ring, type, size);

	prod = rxr->rx_prod;
	for (i = 0; i < ring->ring_size; i++) {
		if (bnxt_alloc_rx_data(rxq, rxr, prod) != 0) {
			RTE_LOG(WARNING, PMD,
				"init'ed rx ring %d with %d/%d mbufs only\n",
				rxq->queue_id, i, ring->ring_size);
			break;
		}
		rxr->rx_prod = prod;
		prod = RING_NEXT(rxr->rx_ring_struct, prod);
	}
	RTE_LOG(DEBUG, PMD, "%s\n", __func__);

	ring = rxr->ag_ring_struct;
	type = RX_PROD_AGG_BD_TYPE_RX_PROD_AGG;
	bnxt_init_rxbds(ring, type, size);
	prod = rxr->ag_prod;

	for (i = 0; i < ring->ring_size; i++) {
		if (bnxt_alloc_ag_data(rxq, rxr, prod) != 0) {
			RTE_LOG(WARNING, PMD,
			"init'ed AG ring %d with %d/%d mbufs only\n",
			rxq->queue_id, i, ring->ring_size);
			break;
		}
		rxr->ag_prod = prod;
		prod = RING_NEXT(rxr->ag_ring_struct, prod);
	}
	RTE_LOG(DEBUG, PMD, "%s AGG Done!\n", __func__);

	if (rxr->tpa_info) {
		for (i = 0; i < BNXT_TPA_MAX; i++) {
			rxr->tpa_info[i].mbuf =
				__bnxt_alloc_rx_data(rxq->mb_pool);
			if (!rxr->tpa_info[i].mbuf) {
				rte_atomic64_inc(&rxq->bp->rx_mbuf_alloc_fail);
				return -ENOMEM;
			}
		}
	}
	RTE_LOG(DEBUG, PMD, "%s TPA alloc Done!\n", __func__);

	return 0;
}
