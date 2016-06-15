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
	if (!data)
		return -ENOMEM;

	rx_buf->mbuf = data;

	rxbd->addr = rte_cpu_to_le_64(RTE_MBUF_DATA_DMA_ADDR(rx_buf->mbuf));

	return 0;
}

static void bnxt_reuse_rx_mbuf(struct bnxt_rx_ring_info *rxr, uint16_t cons,
			       struct rte_mbuf *mbuf)
{
	uint16_t prod = rxr->rx_prod;
	struct bnxt_sw_rx_bd *prod_rx_buf;
	struct rx_prod_pkt_bd *prod_bd, *cons_bd;

	prod_rx_buf = &rxr->rx_buf_ring[prod];

	prod_rx_buf->mbuf = mbuf;

	prod_bd = &rxr->rx_desc_ring[prod];
	cons_bd = &rxr->rx_desc_ring[cons];

	prod_bd->addr = cons_bd->addr;
}

static uint16_t bnxt_rx_pkt(struct rte_mbuf **rx_pkt,
			    struct bnxt_rx_queue *rxq, uint32_t *raw_cons)
{
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;
	struct bnxt_rx_ring_info *rxr = rxq->rx_ring;
	struct rx_pkt_cmpl *rxcmp;
	struct rx_pkt_cmpl_hi *rxcmp1;
	uint32_t tmp_raw_cons = *raw_cons;
	uint16_t cons, prod, cp_cons =
	    RING_CMP(cpr->cp_ring_struct, tmp_raw_cons);
	struct bnxt_sw_rx_bd *rx_buf;
	struct rte_mbuf *mbuf;
	int rc = 0;

	rxcmp = (struct rx_pkt_cmpl *)
	    &cpr->cp_desc_ring[cp_cons];

	tmp_raw_cons = NEXT_RAW_CMP(tmp_raw_cons);
	cp_cons = RING_CMP(cpr->cp_ring_struct, tmp_raw_cons);
	rxcmp1 = (struct rx_pkt_cmpl_hi *)&cpr->cp_desc_ring[cp_cons];

	if (!CMP_VALID(rxcmp1, tmp_raw_cons, cpr->cp_ring_struct))
		return -EBUSY;

	prod = rxr->rx_prod;

	/* EW - GRO deferred to phase 3 */
	cons = rxcmp->opaque;
	rx_buf = &rxr->rx_buf_ring[cons];
	mbuf = rx_buf->mbuf;
	rte_prefetch0(mbuf);

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
	if (rxcmp1->flags2 & RX_PKT_CMPL_FLAGS2_META_FORMAT_VLAN) {
		mbuf->vlan_tci = rxcmp1->metadata &
			(RX_PKT_CMPL_METADATA_VID_MASK |
			RX_PKT_CMPL_METADATA_DE |
			RX_PKT_CMPL_METADATA_PRI_MASK);
		mbuf->ol_flags |= PKT_RX_VLAN_PKT;
	}

	rx_buf->mbuf = NULL;
	if (rxcmp1->errors_v2 & RX_CMP_L2_ERRORS) {
		/* Re-install the mbuf back to the rx ring */
		bnxt_reuse_rx_mbuf(rxr, cons, mbuf);

		rc = -EIO;
		goto next_rx;
	}
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
	if (bnxt_alloc_rx_data(rxq, rxr, prod)) {
		RTE_LOG(ERR, PMD, "mbuf alloc failed with prod=0x%x\n", prod);
		rc = -ENOMEM;
		goto next_rx;
	}

	/*
	 * All MBUFs are allocated with the same size under DPDK,
	 * no optimization for rx_copy_thresh
	 */

	/* AGG buf operation is deferred */

	/* EW - VLAN reception.  Must compare against the ol_flags */

	*rx_pkt = mbuf;
next_rx:
	rxr->rx_prod = RING_NEXT(rxr->rx_ring_struct, prod);

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
	bool rx_event = false;
	struct rx_pkt_cmpl *rxcmp;

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
			else if (rc == -EBUSY)	/* partial completion */
				break;
			rx_event = true;
		}
		raw_cons = NEXT_RAW_CMP(raw_cons);
		if (nb_rx_pkts == nb_pkts)
			break;
	}
	if (raw_cons == cpr->cp_raw_cons) {
		/*
		 * For PMD, there is no need to keep on pushing to REARM
		 * the doorbell if there are no new completions
		 */
		return nb_rx_pkts;
	}
	cpr->cp_raw_cons = raw_cons;

	B_CP_DIS_DB(cpr, cpr->cp_raw_cons);
	if (rx_event)
		B_RX_DB(rxr->rx_doorbell, rxr->rx_prod);
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
	struct bnxt *bp = rxq->bp;
	struct bnxt_cp_ring_info *cpr;
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_ring *ring;

	rxq->rx_buf_use_size = bp->eth_dev->data->mtu +
			       ETHER_HDR_LEN + ETHER_CRC_LEN +
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
	ring->ring_size = rxr->rx_ring_struct->ring_size * 2;
	ring->ring_mask = ring->ring_size - 1;
	ring->bd = (void *)cpr->cp_desc_ring;
	ring->bd_dma = cpr->cp_desc_mapping;
	ring->vmem_size = 0;
	ring->vmem = NULL;

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
		rx_bd_ring[j].flags_type = type;
		rx_bd_ring[j].len = len;
		rx_bd_ring[j].opaque = j;
	}
}

int bnxt_init_one_rx_ring(struct bnxt_rx_queue *rxq)
{
	struct bnxt_rx_ring_info *rxr;
	struct bnxt_ring *ring;
	uint32_t prod, type;
	unsigned int i;

	type = RX_PROD_PKT_BD_TYPE_RX_PROD_PKT | RX_PROD_PKT_BD_FLAGS_EOP_PAD;

	rxr = rxq->rx_ring;
	ring = rxr->rx_ring_struct;
	bnxt_init_rxbds(ring, type, rxq->rx_buf_use_size);

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

	return 0;
}
