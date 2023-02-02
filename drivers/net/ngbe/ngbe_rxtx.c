/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <sys/queue.h>

#include <stdint.h>
#include <rte_ethdev.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_net.h>

#include "ngbe_logs.h"
#include "base/ngbe.h"
#include "ngbe_ethdev.h"
#include "ngbe_rxtx.h"

#ifdef RTE_LIBRTE_IEEE1588
#define NGBE_TX_IEEE1588_TMST RTE_MBUF_F_TX_IEEE1588_TMST
#else
#define NGBE_TX_IEEE1588_TMST 0
#endif

/* Bit Mask to indicate what bits required for building Tx context */
static const u64 NGBE_TX_OFFLOAD_MASK = (RTE_MBUF_F_TX_IP_CKSUM |
		RTE_MBUF_F_TX_IPV6 |
		RTE_MBUF_F_TX_IPV4 |
		RTE_MBUF_F_TX_VLAN |
		RTE_MBUF_F_TX_L4_MASK |
		RTE_MBUF_F_TX_TCP_SEG |
		NGBE_TX_IEEE1588_TMST);

#define NGBE_TX_OFFLOAD_NOTSUP_MASK \
		(RTE_MBUF_F_TX_OFFLOAD_MASK ^ NGBE_TX_OFFLOAD_MASK)

/*
 * Prefetch a cache line into all cache levels.
 */
#define rte_ngbe_prefetch(p)   rte_prefetch0(p)

/*********************************************************************
 *
 *  Tx functions
 *
 **********************************************************************/

/*
 * Check for descriptors with their DD bit set and free mbufs.
 * Return the total number of buffers freed.
 */
static __rte_always_inline int
ngbe_tx_free_bufs(struct ngbe_tx_queue *txq)
{
	struct ngbe_tx_entry *txep;
	uint32_t status;
	int i, nb_free = 0;
	struct rte_mbuf *m, *free[RTE_NGBE_TX_MAX_FREE_BUF_SZ];

	/* check DD bit on threshold descriptor */
	status = txq->tx_ring[txq->tx_next_dd].dw3;
	if (!(status & rte_cpu_to_le_32(NGBE_TXD_DD))) {
		if (txq->nb_tx_free >> 1 < txq->tx_free_thresh)
			ngbe_set32_masked(txq->tdc_reg_addr,
				NGBE_TXCFG_FLUSH, NGBE_TXCFG_FLUSH);
		return 0;
	}

	/*
	 * first buffer to free from S/W ring is at index
	 * tx_next_dd - (tx_free_thresh-1)
	 */
	txep = &txq->sw_ring[txq->tx_next_dd - (txq->tx_free_thresh - 1)];
	for (i = 0; i < txq->tx_free_thresh; ++i, ++txep) {
		/* free buffers one at a time */
		m = rte_pktmbuf_prefree_seg(txep->mbuf);
		txep->mbuf = NULL;

		if (unlikely(m == NULL))
			continue;

		if (nb_free >= RTE_NGBE_TX_MAX_FREE_BUF_SZ ||
		    (nb_free > 0 && m->pool != free[0]->pool)) {
			rte_mempool_put_bulk(free[0]->pool,
					     (void **)free, nb_free);
			nb_free = 0;
		}

		free[nb_free++] = m;
	}

	if (nb_free > 0)
		rte_mempool_put_bulk(free[0]->pool, (void **)free, nb_free);

	/* buffers were freed, update counters */
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + txq->tx_free_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_free_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(txq->tx_free_thresh - 1);

	return txq->tx_free_thresh;
}

/* Populate 4 descriptors with data from 4 mbufs */
static inline void
tx4(volatile struct ngbe_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t buf_dma_addr;
	uint32_t pkt_len;
	int i;

	for (i = 0; i < 4; ++i, ++txdp, ++pkts) {
		buf_dma_addr = rte_mbuf_data_iova(*pkts);
		pkt_len = (*pkts)->data_len;

		/* write data to descriptor */
		txdp->qw0 = rte_cpu_to_le_64(buf_dma_addr);
		txdp->dw2 = cpu_to_le32(NGBE_TXD_FLAGS |
					NGBE_TXD_DATLEN(pkt_len));
		txdp->dw3 = cpu_to_le32(NGBE_TXD_PAYLEN(pkt_len));

		rte_prefetch0(&(*pkts)->pool);
	}
}

/* Populate 1 descriptor with data from 1 mbuf */
static inline void
tx1(volatile struct ngbe_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t buf_dma_addr;
	uint32_t pkt_len;

	buf_dma_addr = rte_mbuf_data_iova(*pkts);
	pkt_len = (*pkts)->data_len;

	/* write data to descriptor */
	txdp->qw0 = cpu_to_le64(buf_dma_addr);
	txdp->dw2 = cpu_to_le32(NGBE_TXD_FLAGS |
				NGBE_TXD_DATLEN(pkt_len));
	txdp->dw3 = cpu_to_le32(NGBE_TXD_PAYLEN(pkt_len));

	rte_prefetch0(&(*pkts)->pool);
}

/*
 * Fill H/W descriptor ring with mbuf data.
 * Copy mbuf pointers to the S/W ring.
 */
static inline void
ngbe_tx_fill_hw_ring(struct ngbe_tx_queue *txq, struct rte_mbuf **pkts,
		      uint16_t nb_pkts)
{
	volatile struct ngbe_tx_desc *txdp = &txq->tx_ring[txq->tx_tail];
	struct ngbe_tx_entry *txep = &txq->sw_ring[txq->tx_tail];
	const int N_PER_LOOP = 4;
	const int N_PER_LOOP_MASK = N_PER_LOOP - 1;
	int mainpart, leftover;
	int i, j;

	/*
	 * Process most of the packets in chunks of N pkts.  Any
	 * leftover packets will get processed one at a time.
	 */
	mainpart = (nb_pkts & ((uint32_t)~N_PER_LOOP_MASK));
	leftover = (nb_pkts & ((uint32_t)N_PER_LOOP_MASK));
	for (i = 0; i < mainpart; i += N_PER_LOOP) {
		/* Copy N mbuf pointers to the S/W ring */
		for (j = 0; j < N_PER_LOOP; ++j)
			(txep + i + j)->mbuf = *(pkts + i + j);
		tx4(txdp + i, pkts + i);
	}

	if (unlikely(leftover > 0)) {
		for (i = 0; i < leftover; ++i) {
			(txep + mainpart + i)->mbuf = *(pkts + mainpart + i);
			tx1(txdp + mainpart + i, pkts + mainpart + i);
		}
	}
}

static inline uint16_t
tx_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	     uint16_t nb_pkts)
{
	struct ngbe_tx_queue *txq = (struct ngbe_tx_queue *)tx_queue;
	uint16_t n = 0;

	/*
	 * Begin scanning the H/W ring for done descriptors when the
	 * number of available descriptors drops below tx_free_thresh.
	 * For each done descriptor, free the associated buffer.
	 */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		ngbe_tx_free_bufs(txq);

	/* Only use descriptors that are available */
	nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	/* Use exactly nb_pkts descriptors */
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);

	/*
	 * At this point, we know there are enough descriptors in the
	 * ring to transmit all the packets.  This assumes that each
	 * mbuf contains a single segment, and that no new offloads
	 * are expected, which would require a new context descriptor.
	 */

	/*
	 * See if we're going to wrap-around. If so, handle the top
	 * of the descriptor ring first, then do the bottom.  If not,
	 * the processing looks just like the "bottom" part anyway...
	 */
	if ((txq->tx_tail + nb_pkts) > txq->nb_tx_desc) {
		n = (uint16_t)(txq->nb_tx_desc - txq->tx_tail);
		ngbe_tx_fill_hw_ring(txq, tx_pkts, n);
		txq->tx_tail = 0;
	}

	/* Fill H/W descriptor ring with mbuf data */
	ngbe_tx_fill_hw_ring(txq, tx_pkts + n, (uint16_t)(nb_pkts - n));
	txq->tx_tail = (uint16_t)(txq->tx_tail + (nb_pkts - n));

	/*
	 * Check for wrap-around. This would only happen if we used
	 * up to the last descriptor in the ring, no more, no less.
	 */
	if (txq->tx_tail >= txq->nb_tx_desc)
		txq->tx_tail = 0;

	PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u tx_tail=%u nb_tx=%u",
		   (uint16_t)txq->port_id, (uint16_t)txq->queue_id,
		   (uint16_t)txq->tx_tail, (uint16_t)nb_pkts);

	/* update tail pointer */
	rte_wmb();
	ngbe_set32_relaxed(txq->tdt_reg_addr, txq->tx_tail);

	return nb_pkts;
}

uint16_t
ngbe_xmit_pkts_simple(void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts)
{
	uint16_t nb_tx;

	/* Try to transmit at least chunks of TX_MAX_BURST pkts */
	if (likely(nb_pkts <= RTE_PMD_NGBE_TX_MAX_BURST))
		return tx_xmit_pkts(tx_queue, tx_pkts, nb_pkts);

	/* transmit more than the max burst, in chunks of TX_MAX_BURST */
	nb_tx = 0;
	while (nb_pkts != 0) {
		uint16_t ret, n;

		n = (uint16_t)RTE_MIN(nb_pkts, RTE_PMD_NGBE_TX_MAX_BURST);
		ret = tx_xmit_pkts(tx_queue, &tx_pkts[nb_tx], n);
		nb_tx = (uint16_t)(nb_tx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < n)
			break;
	}

	return nb_tx;
}

static inline void
ngbe_set_xmit_ctx(struct ngbe_tx_queue *txq,
		volatile struct ngbe_tx_ctx_desc *ctx_txd,
		uint64_t ol_flags, union ngbe_tx_offload tx_offload)
{
	union ngbe_tx_offload tx_offload_mask;
	uint32_t type_tucmd_mlhl;
	uint32_t mss_l4len_idx;
	uint32_t ctx_idx;
	uint32_t vlan_macip_lens;
	uint32_t tunnel_seed;

	ctx_idx = txq->ctx_curr;
	tx_offload_mask.data[0] = 0;
	tx_offload_mask.data[1] = 0;

	/* Specify which HW CTX to upload. */
	mss_l4len_idx = NGBE_TXD_IDX(ctx_idx);
	type_tucmd_mlhl = NGBE_TXD_CTXT;

	tx_offload_mask.ptid |= ~0;
	type_tucmd_mlhl |= NGBE_TXD_PTID(tx_offload.ptid);

	/* check if TCP segmentation required for this packet */
	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		tx_offload_mask.l2_len |= ~0;
		tx_offload_mask.l3_len |= ~0;
		tx_offload_mask.l4_len |= ~0;
		tx_offload_mask.tso_segsz |= ~0;
		mss_l4len_idx |= NGBE_TXD_MSS(tx_offload.tso_segsz);
		mss_l4len_idx |= NGBE_TXD_L4LEN(tx_offload.l4_len);
	} else { /* no TSO, check if hardware checksum is needed */
		if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM) {
			tx_offload_mask.l2_len |= ~0;
			tx_offload_mask.l3_len |= ~0;
		}

		switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
		case RTE_MBUF_F_TX_UDP_CKSUM:
			mss_l4len_idx |=
				NGBE_TXD_L4LEN(sizeof(struct rte_udp_hdr));
			tx_offload_mask.l2_len |= ~0;
			tx_offload_mask.l3_len |= ~0;
			break;
		case RTE_MBUF_F_TX_TCP_CKSUM:
			mss_l4len_idx |=
				NGBE_TXD_L4LEN(sizeof(struct rte_tcp_hdr));
			tx_offload_mask.l2_len |= ~0;
			tx_offload_mask.l3_len |= ~0;
			break;
		case RTE_MBUF_F_TX_SCTP_CKSUM:
			mss_l4len_idx |=
				NGBE_TXD_L4LEN(sizeof(struct rte_sctp_hdr));
			tx_offload_mask.l2_len |= ~0;
			tx_offload_mask.l3_len |= ~0;
			break;
		default:
			break;
		}
	}

	vlan_macip_lens = NGBE_TXD_IPLEN(tx_offload.l3_len >> 1);
	vlan_macip_lens |= NGBE_TXD_MACLEN(tx_offload.l2_len);

	if (ol_flags & RTE_MBUF_F_TX_VLAN) {
		tx_offload_mask.vlan_tci |= ~0;
		vlan_macip_lens |= NGBE_TXD_VLAN(tx_offload.vlan_tci);
	}

	tunnel_seed = 0;

	txq->ctx_cache[ctx_idx].flags = ol_flags;
	txq->ctx_cache[ctx_idx].tx_offload.data[0] =
		tx_offload_mask.data[0] & tx_offload.data[0];
	txq->ctx_cache[ctx_idx].tx_offload.data[1] =
		tx_offload_mask.data[1] & tx_offload.data[1];
	txq->ctx_cache[ctx_idx].tx_offload_mask = tx_offload_mask;

	ctx_txd->dw0 = rte_cpu_to_le_32(vlan_macip_lens);
	ctx_txd->dw1 = rte_cpu_to_le_32(tunnel_seed);
	ctx_txd->dw2 = rte_cpu_to_le_32(type_tucmd_mlhl);
	ctx_txd->dw3 = rte_cpu_to_le_32(mss_l4len_idx);
}

/*
 * Check which hardware context can be used. Use the existing match
 * or create a new context descriptor.
 */
static inline uint32_t
what_ctx_update(struct ngbe_tx_queue *txq, uint64_t flags,
		   union ngbe_tx_offload tx_offload)
{
	/* If match with the current used context */
	if (likely(txq->ctx_cache[txq->ctx_curr].flags == flags &&
		   (txq->ctx_cache[txq->ctx_curr].tx_offload.data[0] ==
		    (txq->ctx_cache[txq->ctx_curr].tx_offload_mask.data[0]
		     & tx_offload.data[0])) &&
		   (txq->ctx_cache[txq->ctx_curr].tx_offload.data[1] ==
		    (txq->ctx_cache[txq->ctx_curr].tx_offload_mask.data[1]
		     & tx_offload.data[1]))))
		return txq->ctx_curr;

	/* What if match with the next context  */
	txq->ctx_curr ^= 1;
	if (likely(txq->ctx_cache[txq->ctx_curr].flags == flags &&
		   (txq->ctx_cache[txq->ctx_curr].tx_offload.data[0] ==
		    (txq->ctx_cache[txq->ctx_curr].tx_offload_mask.data[0]
		     & tx_offload.data[0])) &&
		   (txq->ctx_cache[txq->ctx_curr].tx_offload.data[1] ==
		    (txq->ctx_cache[txq->ctx_curr].tx_offload_mask.data[1]
		     & tx_offload.data[1]))))
		return txq->ctx_curr;

	/* Mismatch, use the previous context */
	return NGBE_CTX_NUM;
}

static inline uint32_t
tx_desc_cksum_flags_to_olinfo(uint64_t ol_flags)
{
	uint32_t tmp = 0;

	if ((ol_flags & RTE_MBUF_F_TX_L4_MASK) != RTE_MBUF_F_TX_L4_NO_CKSUM) {
		tmp |= NGBE_TXD_CC;
		tmp |= NGBE_TXD_L4CS;
	}
	if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM) {
		tmp |= NGBE_TXD_CC;
		tmp |= NGBE_TXD_IPCS;
	}
	if (ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM) {
		tmp |= NGBE_TXD_CC;
		tmp |= NGBE_TXD_EIPCS;
	}
	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		tmp |= NGBE_TXD_CC;
		/* implies IPv4 cksum */
		if (ol_flags & RTE_MBUF_F_TX_IPV4)
			tmp |= NGBE_TXD_IPCS;
		tmp |= NGBE_TXD_L4CS;
	}
	if (ol_flags & RTE_MBUF_F_TX_VLAN)
		tmp |= NGBE_TXD_CC;

	return tmp;
}

static inline uint32_t
tx_desc_ol_flags_to_cmdtype(uint64_t ol_flags)
{
	uint32_t cmdtype = 0;

	if (ol_flags & RTE_MBUF_F_TX_VLAN)
		cmdtype |= NGBE_TXD_VLE;
	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
		cmdtype |= NGBE_TXD_TSE;
	return cmdtype;
}

static inline uint32_t
tx_desc_ol_flags_to_ptype(uint64_t oflags)
{
	uint32_t ptype;

	/* L2 level */
	ptype = RTE_PTYPE_L2_ETHER;
	if (oflags & RTE_MBUF_F_TX_VLAN)
		ptype |= RTE_PTYPE_L2_ETHER_VLAN;

	/* L3 level */
	if (oflags & (RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM))
		ptype |= RTE_PTYPE_L3_IPV4;
	else if (oflags & (RTE_MBUF_F_TX_IPV6))
		ptype |= RTE_PTYPE_L3_IPV6;

	/* L4 level */
	switch (oflags & (RTE_MBUF_F_TX_L4_MASK)) {
	case RTE_MBUF_F_TX_TCP_CKSUM:
		ptype |= RTE_PTYPE_L4_TCP;
		break;
	case RTE_MBUF_F_TX_UDP_CKSUM:
		ptype |= RTE_PTYPE_L4_UDP;
		break;
	case RTE_MBUF_F_TX_SCTP_CKSUM:
		ptype |= RTE_PTYPE_L4_SCTP;
		break;
	}

	if (oflags & RTE_MBUF_F_TX_TCP_SEG)
		ptype |= RTE_PTYPE_L4_TCP;

	return ptype;
}

static inline uint8_t
tx_desc_ol_flags_to_ptid(uint64_t oflags)
{
	uint32_t ptype;

	ptype = tx_desc_ol_flags_to_ptype(oflags);

	return ngbe_encode_ptype(ptype);
}

/* Reset transmit descriptors after they have been used */
static inline int
ngbe_xmit_cleanup(struct ngbe_tx_queue *txq)
{
	struct ngbe_tx_entry *sw_ring = txq->sw_ring;
	volatile struct ngbe_tx_desc *txr = txq->tx_ring;
	uint16_t last_desc_cleaned = txq->last_desc_cleaned;
	uint16_t nb_tx_desc = txq->nb_tx_desc;
	uint16_t desc_to_clean_to;
	uint16_t nb_tx_to_clean;
	uint32_t status;

	/* Determine the last descriptor needing to be cleaned */
	desc_to_clean_to = (uint16_t)(last_desc_cleaned + txq->tx_free_thresh);
	if (desc_to_clean_to >= nb_tx_desc)
		desc_to_clean_to = (uint16_t)(desc_to_clean_to - nb_tx_desc);

	/* Check to make sure the last descriptor to clean is done */
	desc_to_clean_to = sw_ring[desc_to_clean_to].last_id;
	status = txr[desc_to_clean_to].dw3;
	if (!(status & rte_cpu_to_le_32(NGBE_TXD_DD))) {
		PMD_TX_LOG(DEBUG,
			"Tx descriptor %4u is not done"
			"(port=%d queue=%d)",
			desc_to_clean_to,
			txq->port_id, txq->queue_id);
		if (txq->nb_tx_free >> 1 < txq->tx_free_thresh)
			ngbe_set32_masked(txq->tdc_reg_addr,
				NGBE_TXCFG_FLUSH, NGBE_TXCFG_FLUSH);
		/* Failed to clean any descriptors, better luck next time */
		return -(1);
	}

	/* Figure out how many descriptors will be cleaned */
	if (last_desc_cleaned > desc_to_clean_to)
		nb_tx_to_clean = (uint16_t)((nb_tx_desc - last_desc_cleaned) +
							desc_to_clean_to);
	else
		nb_tx_to_clean = (uint16_t)(desc_to_clean_to -
						last_desc_cleaned);

	PMD_TX_LOG(DEBUG,
		"Cleaning %4u Tx descriptors: %4u to %4u (port=%d queue=%d)",
		nb_tx_to_clean, last_desc_cleaned, desc_to_clean_to,
		txq->port_id, txq->queue_id);

	/*
	 * The last descriptor to clean is done, so that means all the
	 * descriptors from the last descriptor that was cleaned
	 * up to the last descriptor with the RS bit set
	 * are done. Only reset the threshold descriptor.
	 */
	txr[desc_to_clean_to].dw3 = 0;

	/* Update the txq to reflect the last descriptor that was cleaned */
	txq->last_desc_cleaned = desc_to_clean_to;
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + nb_tx_to_clean);

	/* No Error */
	return 0;
}

uint16_t
ngbe_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	struct ngbe_tx_queue *txq;
	struct ngbe_tx_entry *sw_ring;
	struct ngbe_tx_entry *txe, *txn;
	volatile struct ngbe_tx_desc *txr;
	volatile struct ngbe_tx_desc *txd;
	struct rte_mbuf     *tx_pkt;
	struct rte_mbuf     *m_seg;
	uint64_t buf_dma_addr;
	uint32_t olinfo_status;
	uint32_t cmd_type_len;
	uint32_t pkt_len;
	uint16_t slen;
	uint64_t ol_flags;
	uint16_t tx_id;
	uint16_t tx_last;
	uint16_t nb_tx;
	uint16_t nb_used;
	uint64_t tx_ol_req;
	uint32_t ctx = 0;
	uint32_t new_ctx;
	union ngbe_tx_offload tx_offload;

	tx_offload.data[0] = 0;
	tx_offload.data[1] = 0;
	txq = tx_queue;
	sw_ring = txq->sw_ring;
	txr     = txq->tx_ring;
	tx_id   = txq->tx_tail;
	txe = &sw_ring[tx_id];

	/* Determine if the descriptor ring needs to be cleaned. */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		ngbe_xmit_cleanup(txq);

	rte_prefetch0(&txe->mbuf->pool);

	/* Tx loop */
	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		new_ctx = 0;
		tx_pkt = *tx_pkts++;
		pkt_len = tx_pkt->pkt_len;

		/*
		 * Determine how many (if any) context descriptors
		 * are needed for offload functionality.
		 */
		ol_flags = tx_pkt->ol_flags;

		/* If hardware offload required */
		tx_ol_req = ol_flags & NGBE_TX_OFFLOAD_MASK;
		if (tx_ol_req) {
			tx_offload.ptid = tx_desc_ol_flags_to_ptid(tx_ol_req);
			tx_offload.l2_len = tx_pkt->l2_len;
			tx_offload.l3_len = tx_pkt->l3_len;
			tx_offload.l4_len = tx_pkt->l4_len;
			tx_offload.vlan_tci = tx_pkt->vlan_tci;
			tx_offload.tso_segsz = tx_pkt->tso_segsz;

			/* If new context need be built or reuse the exist ctx*/
			ctx = what_ctx_update(txq, tx_ol_req, tx_offload);
			/* Only allocate context descriptor if required */
			new_ctx = (ctx == NGBE_CTX_NUM);
			ctx = txq->ctx_curr;
		}

		/*
		 * Keep track of how many descriptors are used this loop
		 * This will always be the number of segments + the number of
		 * Context descriptors required to transmit the packet
		 */
		nb_used = (uint16_t)(tx_pkt->nb_segs + new_ctx);

		/*
		 * The number of descriptors that must be allocated for a
		 * packet is the number of segments of that packet, plus 1
		 * Context Descriptor for the hardware offload, if any.
		 * Determine the last Tx descriptor to allocate in the Tx ring
		 * for the packet, starting from the current position (tx_id)
		 * in the ring.
		 */
		tx_last = (uint16_t)(tx_id + nb_used - 1);

		/* Circular ring */
		if (tx_last >= txq->nb_tx_desc)
			tx_last = (uint16_t)(tx_last - txq->nb_tx_desc);

		PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u pktlen=%u"
			   " tx_first=%u tx_last=%u",
			   (uint16_t)txq->port_id,
			   (uint16_t)txq->queue_id,
			   (uint32_t)pkt_len,
			   (uint16_t)tx_id,
			   (uint16_t)tx_last);

		/*
		 * Make sure there are enough Tx descriptors available to
		 * transmit the entire packet.
		 * nb_used better be less than or equal to txq->tx_free_thresh
		 */
		if (nb_used > txq->nb_tx_free) {
			PMD_TX_LOG(DEBUG,
				"Not enough free Tx descriptors "
				"nb_used=%4u nb_free=%4u "
				"(port=%d queue=%d)",
				nb_used, txq->nb_tx_free,
				txq->port_id, txq->queue_id);

			if (ngbe_xmit_cleanup(txq) != 0) {
				/* Could not clean any descriptors */
				if (nb_tx == 0)
					return 0;
				goto end_of_tx;
			}

			/* nb_used better be <= txq->tx_free_thresh */
			if (unlikely(nb_used > txq->tx_free_thresh)) {
				PMD_TX_LOG(DEBUG,
					"The number of descriptors needed to "
					"transmit the packet exceeds the "
					"RS bit threshold. This will impact "
					"performance."
					"nb_used=%4u nb_free=%4u "
					"tx_free_thresh=%4u. "
					"(port=%d queue=%d)",
					nb_used, txq->nb_tx_free,
					txq->tx_free_thresh,
					txq->port_id, txq->queue_id);
				/*
				 * Loop here until there are enough Tx
				 * descriptors or until the ring cannot be
				 * cleaned.
				 */
				while (nb_used > txq->nb_tx_free) {
					if (ngbe_xmit_cleanup(txq) != 0) {
						/*
						 * Could not clean any
						 * descriptors
						 */
						if (nb_tx == 0)
							return 0;
						goto end_of_tx;
					}
				}
			}
		}

		/*
		 * By now there are enough free Tx descriptors to transmit
		 * the packet.
		 */

		/*
		 * Set common flags of all Tx Data Descriptors.
		 *
		 * The following bits must be set in the first Data Descriptor
		 * and are ignored in the other ones:
		 *   - NGBE_TXD_FCS
		 *
		 * The following bits must only be set in the last Data
		 * Descriptor:
		 *   - NGBE_TXD_EOP
		 */
		cmd_type_len = NGBE_TXD_FCS;

#ifdef RTE_LIBRTE_IEEE1588
		if (ol_flags & RTE_MBUF_F_TX_IEEE1588_TMST)
			cmd_type_len |= NGBE_TXD_1588;
#endif

		olinfo_status = 0;
		if (tx_ol_req) {
			if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
				/* when TSO is on, paylen in descriptor is the
				 * not the packet len but the tcp payload len
				 */
				pkt_len -= (tx_offload.l2_len +
					tx_offload.l3_len + tx_offload.l4_len);
			}

			/*
			 * Setup the Tx Context Descriptor if required
			 */
			if (new_ctx) {
				volatile struct ngbe_tx_ctx_desc *ctx_txd;

				ctx_txd = (volatile struct ngbe_tx_ctx_desc *)
				    &txr[tx_id];

				txn = &sw_ring[txe->next_id];
				rte_prefetch0(&txn->mbuf->pool);

				if (txe->mbuf != NULL) {
					rte_pktmbuf_free_seg(txe->mbuf);
					txe->mbuf = NULL;
				}

				ngbe_set_xmit_ctx(txq, ctx_txd, tx_ol_req,
					tx_offload);

				txe->last_id = tx_last;
				tx_id = txe->next_id;
				txe = txn;
			}

			/*
			 * Setup the Tx Data Descriptor,
			 * This path will go through
			 * whatever new/reuse the context descriptor
			 */
			cmd_type_len  |= tx_desc_ol_flags_to_cmdtype(ol_flags);
			olinfo_status |=
				tx_desc_cksum_flags_to_olinfo(ol_flags);
			olinfo_status |= NGBE_TXD_IDX(ctx);
		}

		olinfo_status |= NGBE_TXD_PAYLEN(pkt_len);

		m_seg = tx_pkt;
		do {
			txd = &txr[tx_id];
			txn = &sw_ring[txe->next_id];
			rte_prefetch0(&txn->mbuf->pool);

			if (txe->mbuf != NULL)
				rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = m_seg;

			/*
			 * Set up Transmit Data Descriptor.
			 */
			slen = m_seg->data_len;
			buf_dma_addr = rte_mbuf_data_iova(m_seg);
			txd->qw0 = rte_cpu_to_le_64(buf_dma_addr);
			txd->dw2 = rte_cpu_to_le_32(cmd_type_len | slen);
			txd->dw3 = rte_cpu_to_le_32(olinfo_status);
			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
			m_seg = m_seg->next;
		} while (m_seg != NULL);

		/*
		 * The last packet data descriptor needs End Of Packet (EOP)
		 */
		cmd_type_len |= NGBE_TXD_EOP;
		txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_used);

		txd->dw2 |= rte_cpu_to_le_32(cmd_type_len);
	}

end_of_tx:

	rte_wmb();

	/*
	 * Set the Transmit Descriptor Tail (TDT)
	 */
	PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u tx_tail=%u nb_tx=%u",
		   (uint16_t)txq->port_id, (uint16_t)txq->queue_id,
		   (uint16_t)tx_id, (uint16_t)nb_tx);
	ngbe_set32_relaxed(txq->tdt_reg_addr, tx_id);
	txq->tx_tail = tx_id;

	return nb_tx;
}

/*********************************************************************
 *
 *  Tx prep functions
 *
 **********************************************************************/
uint16_t
ngbe_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i, ret;
	uint64_t ol_flags;
	struct rte_mbuf *m;
	struct ngbe_tx_queue *txq = (struct ngbe_tx_queue *)tx_queue;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;

		/**
		 * Check if packet meets requirements for number of segments
		 *
		 * NOTE: for ngbe it's always (40 - WTHRESH) for both TSO and
		 *       non-TSO
		 */

		if (m->nb_segs > NGBE_TX_MAX_SEG - txq->wthresh) {
			rte_errno = -EINVAL;
			return i;
		}

		if (ol_flags & NGBE_TX_OFFLOAD_NOTSUP_MASK) {
			rte_errno = -ENOTSUP;
			return i;
		}

#ifdef RTE_ETHDEV_DEBUG_TX
		ret = rte_validate_tx_offload(m);
		if (ret != 0) {
			rte_errno = ret;
			return i;
		}
#endif
		ret = rte_net_intel_cksum_prepare(m);
		if (ret != 0) {
			rte_errno = ret;
			return i;
		}
	}

	return i;
}

/*********************************************************************
 *
 *  Rx functions
 *
 **********************************************************************/
static inline uint32_t
ngbe_rxd_pkt_info_to_pkt_type(uint32_t pkt_info, uint16_t ptid_mask)
{
	uint16_t ptid = NGBE_RXD_PTID(pkt_info);

	ptid &= ptid_mask;

	return ngbe_decode_ptype(ptid);
}

static inline uint64_t
ngbe_rxd_pkt_info_to_pkt_flags(uint32_t pkt_info)
{
	static uint64_t ip_rss_types_map[16] __rte_cache_aligned = {
		0, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH, RTE_MBUF_F_RX_RSS_HASH,
		0, RTE_MBUF_F_RX_RSS_HASH, 0, RTE_MBUF_F_RX_RSS_HASH,
		RTE_MBUF_F_RX_RSS_HASH, 0, 0, 0,
		0, 0, 0,  RTE_MBUF_F_RX_FDIR,
	};
#ifdef RTE_LIBRTE_IEEE1588
	static uint64_t ip_pkt_etqf_map[8] = {
		0, 0, 0, RTE_MBUF_F_RX_IEEE1588_PTP,
		0, 0, 0, 0,
	};
	int etfid = ngbe_etflt_id(NGBE_RXD_PTID(pkt_info));
	if (likely(-1 != etfid))
		return ip_pkt_etqf_map[etfid] |
		       ip_rss_types_map[NGBE_RXD_RSSTYPE(pkt_info)];
	else
		return ip_rss_types_map[NGBE_RXD_RSSTYPE(pkt_info)];
#else
	return ip_rss_types_map[NGBE_RXD_RSSTYPE(pkt_info)];
#endif
}

static inline uint64_t
rx_desc_status_to_pkt_flags(uint32_t rx_status, uint64_t vlan_flags)
{
	uint64_t pkt_flags;

	/*
	 * Check if VLAN present only.
	 * Do not check whether L3/L4 rx checksum done by NIC or not,
	 * That can be found from rte_eth_rxmode.offloads flag
	 */
	pkt_flags = (rx_status & NGBE_RXD_STAT_VLAN &&
		     vlan_flags & RTE_MBUF_F_RX_VLAN_STRIPPED)
		    ? vlan_flags : 0;

#ifdef RTE_LIBRTE_IEEE1588
	if (rx_status & NGBE_RXD_STAT_1588)
		pkt_flags = pkt_flags | RTE_MBUF_F_RX_IEEE1588_TMST;
#endif
	return pkt_flags;
}

static inline uint64_t
rx_desc_error_to_pkt_flags(uint32_t rx_status)
{
	uint64_t pkt_flags = 0;

	/* checksum offload can't be disabled */
	if (rx_status & NGBE_RXD_STAT_IPCS)
		pkt_flags |= (rx_status & NGBE_RXD_ERR_IPCS
				? RTE_MBUF_F_RX_IP_CKSUM_BAD : RTE_MBUF_F_RX_IP_CKSUM_GOOD);

	if (rx_status & NGBE_RXD_STAT_L4CS)
		pkt_flags |= (rx_status & NGBE_RXD_ERR_L4CS
				? RTE_MBUF_F_RX_L4_CKSUM_BAD : RTE_MBUF_F_RX_L4_CKSUM_GOOD);

	if (rx_status & NGBE_RXD_STAT_EIPCS &&
	    rx_status & NGBE_RXD_ERR_EIPCS)
		pkt_flags |= RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD;

	return pkt_flags;
}

/*
 * LOOK_AHEAD defines how many desc statuses to check beyond the
 * current descriptor.
 * It must be a pound define for optimal performance.
 * Do not change the value of LOOK_AHEAD, as the ngbe_rx_scan_hw_ring
 * function only works with LOOK_AHEAD=8.
 */
#define LOOK_AHEAD 8
#if (LOOK_AHEAD != 8)
#error "PMD NGBE: LOOK_AHEAD must be 8\n"
#endif
static inline int
ngbe_rx_scan_hw_ring(struct ngbe_rx_queue *rxq)
{
	volatile struct ngbe_rx_desc *rxdp;
	struct ngbe_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t pkt_len;
	uint64_t pkt_flags;
	int nb_dd;
	uint32_t s[LOOK_AHEAD];
	uint32_t pkt_info[LOOK_AHEAD];
	int i, j, nb_rx = 0;
	uint32_t status;

	/* get references to current descriptor and S/W ring entry */
	rxdp = &rxq->rx_ring[rxq->rx_tail];
	rxep = &rxq->sw_ring[rxq->rx_tail];

	status = rxdp->qw1.lo.status;
	/* check to make sure there is at least 1 packet to receive */
	if (!(status & rte_cpu_to_le_32(NGBE_RXD_STAT_DD)))
		return 0;

	/*
	 * Scan LOOK_AHEAD descriptors at a time to determine which descriptors
	 * reference packets that are ready to be received.
	 */
	for (i = 0; i < RTE_PMD_NGBE_RX_MAX_BURST;
	     i += LOOK_AHEAD, rxdp += LOOK_AHEAD, rxep += LOOK_AHEAD) {
		/* Read desc statuses backwards to avoid race condition */
		for (j = 0; j < LOOK_AHEAD; j++)
			s[j] = rte_le_to_cpu_32(rxdp[j].qw1.lo.status);

		rte_atomic_thread_fence(__ATOMIC_ACQUIRE);

		/* Compute how many status bits were set */
		for (nb_dd = 0; nb_dd < LOOK_AHEAD &&
				(s[nb_dd] & NGBE_RXD_STAT_DD); nb_dd++)
			;

		for (j = 0; j < nb_dd; j++)
			pkt_info[j] = rte_le_to_cpu_32(rxdp[j].qw0.dw0);

		nb_rx += nb_dd;

		/* Translate descriptor info to mbuf format */
		for (j = 0; j < nb_dd; ++j) {
			mb = rxep[j].mbuf;
			pkt_len = rte_le_to_cpu_16(rxdp[j].qw1.hi.len) -
				  rxq->crc_len;
			mb->data_len = pkt_len;
			mb->pkt_len = pkt_len;
			mb->vlan_tci = rte_le_to_cpu_16(rxdp[j].qw1.hi.tag);

			/* convert descriptor fields to rte mbuf flags */
			pkt_flags = rx_desc_status_to_pkt_flags(s[j],
					rxq->vlan_flags);
			pkt_flags |= rx_desc_error_to_pkt_flags(s[j]);
			pkt_flags |=
				ngbe_rxd_pkt_info_to_pkt_flags(pkt_info[j]);
			mb->ol_flags = pkt_flags;
			mb->packet_type =
				ngbe_rxd_pkt_info_to_pkt_type(pkt_info[j],
				NGBE_PTID_MASK);

			if (likely(pkt_flags & RTE_MBUF_F_RX_RSS_HASH))
				mb->hash.rss =
					rte_le_to_cpu_32(rxdp[j].qw0.dw1);
		}

		/* Move mbuf pointers from the S/W ring to the stage */
		for (j = 0; j < LOOK_AHEAD; ++j)
			rxq->rx_stage[i + j] = rxep[j].mbuf;

		/* stop if all requested packets could not be received */
		if (nb_dd != LOOK_AHEAD)
			break;
	}

	/* clear software ring entries so we can cleanup correctly */
	for (i = 0; i < nb_rx; ++i)
		rxq->sw_ring[rxq->rx_tail + i].mbuf = NULL;

	return nb_rx;
}

static inline int
ngbe_rx_alloc_bufs(struct ngbe_rx_queue *rxq, bool reset_mbuf)
{
	volatile struct ngbe_rx_desc *rxdp;
	struct ngbe_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t alloc_idx;
	__le64 dma_addr;
	int diag, i;

	/* allocate buffers in bulk directly into the S/W ring */
	alloc_idx = rxq->rx_free_trigger - (rxq->rx_free_thresh - 1);
	rxep = &rxq->sw_ring[alloc_idx];
	diag = rte_mempool_get_bulk(rxq->mb_pool, (void *)rxep,
				    rxq->rx_free_thresh);
	if (unlikely(diag != 0))
		return -ENOMEM;

	rxdp = &rxq->rx_ring[alloc_idx];
	for (i = 0; i < rxq->rx_free_thresh; ++i) {
		/* populate the static rte mbuf fields */
		mb = rxep[i].mbuf;
		if (reset_mbuf)
			mb->port = rxq->port_id;

		rte_mbuf_refcnt_set(mb, 1);
		mb->data_off = RTE_PKTMBUF_HEADROOM;

		/* populate the descriptors */
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(mb));
		NGBE_RXD_HDRADDR(&rxdp[i], 0);
		NGBE_RXD_PKTADDR(&rxdp[i], dma_addr);
	}

	/* update state of internal queue structure */
	rxq->rx_free_trigger = rxq->rx_free_trigger + rxq->rx_free_thresh;
	if (rxq->rx_free_trigger >= rxq->nb_rx_desc)
		rxq->rx_free_trigger = rxq->rx_free_thresh - 1;

	/* no errors */
	return 0;
}

static inline uint16_t
ngbe_rx_fill_from_stage(struct ngbe_rx_queue *rxq, struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts)
{
	struct rte_mbuf **stage = &rxq->rx_stage[rxq->rx_next_avail];
	int i;

	/* how many packets are ready to return? */
	nb_pkts = (uint16_t)RTE_MIN(nb_pkts, rxq->rx_nb_avail);

	/* copy mbuf pointers to the application's packet list */
	for (i = 0; i < nb_pkts; ++i)
		rx_pkts[i] = stage[i];

	/* update internal queue state */
	rxq->rx_nb_avail = (uint16_t)(rxq->rx_nb_avail - nb_pkts);
	rxq->rx_next_avail = (uint16_t)(rxq->rx_next_avail + nb_pkts);

	return nb_pkts;
}

static inline uint16_t
ngbe_rx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	     uint16_t nb_pkts)
{
	struct ngbe_rx_queue *rxq = (struct ngbe_rx_queue *)rx_queue;
	struct rte_eth_dev *dev = &rte_eth_devices[rxq->port_id];
	uint16_t nb_rx = 0;

	/* Any previously recv'd pkts will be returned from the Rx stage */
	if (rxq->rx_nb_avail)
		return ngbe_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	/* Scan the H/W ring for packets to receive */
	nb_rx = (uint16_t)ngbe_rx_scan_hw_ring(rxq);

	/* update internal queue state */
	rxq->rx_next_avail = 0;
	rxq->rx_nb_avail = nb_rx;
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_rx);

	/* if required, allocate new buffers to replenish descriptors */
	if (rxq->rx_tail > rxq->rx_free_trigger) {
		uint16_t cur_free_trigger = rxq->rx_free_trigger;

		if (ngbe_rx_alloc_bufs(rxq, true) != 0) {
			int i, j;

			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u", (uint16_t)rxq->port_id,
				   (uint16_t)rxq->queue_id);

			dev->data->rx_mbuf_alloc_failed +=
				rxq->rx_free_thresh;

			/*
			 * Need to rewind any previous receives if we cannot
			 * allocate new buffers to replenish the old ones.
			 */
			rxq->rx_nb_avail = 0;
			rxq->rx_tail = (uint16_t)(rxq->rx_tail - nb_rx);
			for (i = 0, j = rxq->rx_tail; i < nb_rx; ++i, ++j)
				rxq->sw_ring[j].mbuf = rxq->rx_stage[i];

			return 0;
		}

		/* update tail pointer */
		rte_wmb();
		ngbe_set32_relaxed(rxq->rdt_reg_addr, cur_free_trigger);
	}

	if (rxq->rx_tail >= rxq->nb_rx_desc)
		rxq->rx_tail = 0;

	/* received any packets this loop? */
	if (rxq->rx_nb_avail)
		return ngbe_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	return 0;
}

/* split requests into chunks of size RTE_PMD_NGBE_RX_MAX_BURST */
uint16_t
ngbe_recv_pkts_bulk_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts)
{
	uint16_t nb_rx;

	if (unlikely(nb_pkts == 0))
		return 0;

	if (likely(nb_pkts <= RTE_PMD_NGBE_RX_MAX_BURST))
		return ngbe_rx_recv_pkts(rx_queue, rx_pkts, nb_pkts);

	/* request is relatively large, chunk it up */
	nb_rx = 0;
	while (nb_pkts) {
		uint16_t ret, n;

		n = (uint16_t)RTE_MIN(nb_pkts, RTE_PMD_NGBE_RX_MAX_BURST);
		ret = ngbe_rx_recv_pkts(rx_queue, &rx_pkts[nb_rx], n);
		nb_rx = (uint16_t)(nb_rx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < n)
			break;
	}

	return nb_rx;
}

uint16_t
ngbe_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	struct ngbe_rx_queue *rxq;
	volatile struct ngbe_rx_desc *rx_ring;
	volatile struct ngbe_rx_desc *rxdp;
	struct ngbe_rx_entry *sw_ring;
	struct ngbe_rx_entry *rxe;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	struct ngbe_rx_desc rxd;
	uint64_t dma_addr;
	uint32_t staterr;
	uint32_t pkt_info;
	uint16_t pkt_len;
	uint16_t rx_id;
	uint16_t nb_rx;
	uint16_t nb_hold;
	uint64_t pkt_flags;

	nb_rx = 0;
	nb_hold = 0;
	rxq = rx_queue;
	rx_id = rxq->rx_tail;
	rx_ring = rxq->rx_ring;
	sw_ring = rxq->sw_ring;
	struct rte_eth_dev *dev = &rte_eth_devices[rxq->port_id];
	while (nb_rx < nb_pkts) {
		/*
		 * The order of operations here is important as the DD status
		 * bit must not be read after any other descriptor fields.
		 * rx_ring and rxdp are pointing to volatile data so the order
		 * of accesses cannot be reordered by the compiler. If they were
		 * not volatile, they could be reordered which could lead to
		 * using invalid descriptor fields when read from rxd.
		 */
		rxdp = &rx_ring[rx_id];
		staterr = rxdp->qw1.lo.status;
		if (!(staterr & rte_cpu_to_le_32(NGBE_RXD_STAT_DD)))
			break;
		rxd = *rxdp;

		/*
		 * End of packet.
		 *
		 * If the NGBE_RXD_STAT_EOP flag is not set, the Rx packet
		 * is likely to be invalid and to be dropped by the various
		 * validation checks performed by the network stack.
		 *
		 * Allocate a new mbuf to replenish the RX ring descriptor.
		 * If the allocation fails:
		 *    - arrange for that Rx descriptor to be the first one
		 *      being parsed the next time the receive function is
		 *      invoked [on the same queue].
		 *
		 *    - Stop parsing the Rx ring and return immediately.
		 *
		 * This policy do not drop the packet received in the Rx
		 * descriptor for which the allocation of a new mbuf failed.
		 * Thus, it allows that packet to be later retrieved if
		 * mbuf have been freed in the mean time.
		 * As a side effect, holding Rx descriptors instead of
		 * systematically giving them back to the NIC may lead to
		 * Rx ring exhaustion situations.
		 * However, the NIC can gracefully prevent such situations
		 * to happen by sending specific "back-pressure" flow control
		 * frames to its peer(s).
		 */
		PMD_RX_LOG(DEBUG,
			   "port_id=%u queue_id=%u rx_id=%u ext_err_stat=0x%08x pkt_len=%u",
			   (uint16_t)rxq->port_id, (uint16_t)rxq->queue_id,
			   (uint16_t)rx_id, (uint32_t)staterr,
			   (uint16_t)rte_le_to_cpu_16(rxd.qw1.hi.len));

		nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (nmb == NULL) {
			PMD_RX_LOG(DEBUG,
				   "Rx mbuf alloc failed port_id=%u queue_id=%u",
				   (uint16_t)rxq->port_id,
				   (uint16_t)rxq->queue_id);
			dev->data->rx_mbuf_alloc_failed++;
			break;
		}

		nb_hold++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (rx_id == rxq->nb_rx_desc)
			rx_id = 0;

		/* Prefetch next mbuf while processing current one. */
		rte_ngbe_prefetch(sw_ring[rx_id].mbuf);

		/*
		 * When next Rx descriptor is on a cache-line boundary,
		 * prefetch the next 4 Rx descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_ngbe_prefetch(&rx_ring[rx_id]);
			rte_ngbe_prefetch(&sw_ring[rx_id]);
		}

		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		NGBE_RXD_HDRADDR(rxdp, 0);
		NGBE_RXD_PKTADDR(rxdp, dma_addr);

		/*
		 * Initialize the returned mbuf.
		 * 1) setup generic mbuf fields:
		 *    - number of segments,
		 *    - next segment,
		 *    - packet length,
		 *    - Rx port identifier.
		 * 2) integrate hardware offload data, if any:
		 *    - RSS flag & hash,
		 *    - IP checksum flag,
		 *    - VLAN TCI, if any,
		 *    - error flags.
		 */
		pkt_len = (uint16_t)(rte_le_to_cpu_16(rxd.qw1.hi.len) -
				      rxq->crc_len);
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rte_packet_prefetch((char *)rxm->buf_addr + rxm->data_off);
		rxm->nb_segs = 1;
		rxm->next = NULL;
		rxm->pkt_len = pkt_len;
		rxm->data_len = pkt_len;
		rxm->port = rxq->port_id;

		pkt_info = rte_le_to_cpu_32(rxd.qw0.dw0);
		/* Only valid if RTE_MBUF_F_RX_VLAN set in pkt_flags */
		rxm->vlan_tci = rte_le_to_cpu_16(rxd.qw1.hi.tag);

		pkt_flags = rx_desc_status_to_pkt_flags(staterr,
					rxq->vlan_flags);
		pkt_flags |= rx_desc_error_to_pkt_flags(staterr);
		pkt_flags |= ngbe_rxd_pkt_info_to_pkt_flags(pkt_info);
		rxm->ol_flags = pkt_flags;
		rxm->packet_type = ngbe_rxd_pkt_info_to_pkt_type(pkt_info,
						       NGBE_PTID_MASK);

		if (likely(pkt_flags & RTE_MBUF_F_RX_RSS_HASH))
			rxm->hash.rss = rte_le_to_cpu_32(rxd.qw0.dw1);

		/*
		 * Store the mbuf address into the next entry of the array
		 * of returned packets.
		 */
		rx_pkts[nb_rx++] = rxm;
	}
	rxq->rx_tail = rx_id;

	/*
	 * If the number of free Rx descriptors is greater than the Rx free
	 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
	 * register.
	 * Update the RDT with the value of the last processed Rx descriptor
	 * minus 1, to guarantee that the RDT register is never equal to the
	 * RDH register, which creates a "full" ring situation from the
	 * hardware point of view...
	 */
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG,
			   "port_id=%u queue_id=%u rx_tail=%u nb_hold=%u nb_rx=%u",
			   (uint16_t)rxq->port_id, (uint16_t)rxq->queue_id,
			   (uint16_t)rx_id, (uint16_t)nb_hold,
			   (uint16_t)nb_rx);
		rx_id = (uint16_t)((rx_id == 0) ?
				(rxq->nb_rx_desc - 1) : (rx_id - 1));
		ngbe_set32(rxq->rdt_reg_addr, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;
	return nb_rx;
}

/**
 * ngbe_fill_cluster_head_buf - fill the first mbuf of the returned packet
 *
 * Fill the following info in the HEAD buffer of the Rx cluster:
 *    - RX port identifier
 *    - hardware offload data, if any:
 *      - RSS flag & hash
 *      - IP checksum flag
 *      - VLAN TCI, if any
 *      - error flags
 * @head HEAD of the packet cluster
 * @desc HW descriptor to get data from
 * @rxq Pointer to the Rx queue
 */
static inline void
ngbe_fill_cluster_head_buf(struct rte_mbuf *head, struct ngbe_rx_desc *desc,
		struct ngbe_rx_queue *rxq, uint32_t staterr)
{
	uint32_t pkt_info;
	uint64_t pkt_flags;

	head->port = rxq->port_id;

	/* The vlan_tci field is only valid when RTE_MBUF_F_RX_VLAN is
	 * set in the pkt_flags field.
	 */
	head->vlan_tci = rte_le_to_cpu_16(desc->qw1.hi.tag);
	pkt_info = rte_le_to_cpu_32(desc->qw0.dw0);
	pkt_flags = rx_desc_status_to_pkt_flags(staterr, rxq->vlan_flags);
	pkt_flags |= rx_desc_error_to_pkt_flags(staterr);
	pkt_flags |= ngbe_rxd_pkt_info_to_pkt_flags(pkt_info);
	head->ol_flags = pkt_flags;
	head->packet_type = ngbe_rxd_pkt_info_to_pkt_type(pkt_info,
						NGBE_PTID_MASK);

	if (likely(pkt_flags & RTE_MBUF_F_RX_RSS_HASH))
		head->hash.rss = rte_le_to_cpu_32(desc->qw0.dw1);
}

/**
 * ngbe_recv_pkts_sc - receive handler for scatter case.
 *
 * @rx_queue Rx queue handle
 * @rx_pkts table of received packets
 * @nb_pkts size of rx_pkts table
 * @bulk_alloc if TRUE bulk allocation is used for a HW ring refilling
 *
 * Returns the number of received packets/clusters (according to the "bulk
 * receive" interface).
 */
static inline uint16_t
ngbe_recv_pkts_sc(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts,
		    bool bulk_alloc)
{
	struct ngbe_rx_queue *rxq = rx_queue;
	struct rte_eth_dev *dev = &rte_eth_devices[rxq->port_id];
	volatile struct ngbe_rx_desc *rx_ring = rxq->rx_ring;
	struct ngbe_rx_entry *sw_ring = rxq->sw_ring;
	struct ngbe_scattered_rx_entry *sw_sc_ring = rxq->sw_sc_ring;
	uint16_t rx_id = rxq->rx_tail;
	uint16_t nb_rx = 0;
	uint16_t nb_hold = rxq->nb_rx_hold;
	uint16_t prev_id = rxq->rx_tail;

	while (nb_rx < nb_pkts) {
		bool eop;
		struct ngbe_rx_entry *rxe;
		struct ngbe_scattered_rx_entry *sc_entry;
		struct ngbe_scattered_rx_entry *next_sc_entry = NULL;
		struct ngbe_rx_entry *next_rxe = NULL;
		struct rte_mbuf *first_seg;
		struct rte_mbuf *rxm;
		struct rte_mbuf *nmb = NULL;
		struct ngbe_rx_desc rxd;
		uint16_t data_len;
		uint16_t next_id;
		volatile struct ngbe_rx_desc *rxdp;
		uint32_t staterr;

next_desc:
		rxdp = &rx_ring[rx_id];
		staterr = rte_le_to_cpu_32(rxdp->qw1.lo.status);

		if (!(staterr & NGBE_RXD_STAT_DD))
			break;

		rxd = *rxdp;

		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_id=%u "
				  "staterr=0x%x data_len=%u",
			   rxq->port_id, rxq->queue_id, rx_id, staterr,
			   rte_le_to_cpu_16(rxd.qw1.hi.len));

		if (!bulk_alloc) {
			nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
			if (nmb == NULL) {
				PMD_RX_LOG(DEBUG, "Rx mbuf alloc failed "
						  "port_id=%u queue_id=%u",
					   rxq->port_id, rxq->queue_id);

				dev->data->rx_mbuf_alloc_failed++;
				break;
			}
		} else if (nb_hold > rxq->rx_free_thresh) {
			uint16_t next_rdt = rxq->rx_free_trigger;

			if (!ngbe_rx_alloc_bufs(rxq, false)) {
				rte_wmb();
				ngbe_set32_relaxed(rxq->rdt_reg_addr,
							    next_rdt);
				nb_hold -= rxq->rx_free_thresh;
			} else {
				PMD_RX_LOG(DEBUG, "Rx bulk alloc failed "
						  "port_id=%u queue_id=%u",
					   rxq->port_id, rxq->queue_id);

				dev->data->rx_mbuf_alloc_failed++;
				break;
			}
		}

		nb_hold++;
		rxe = &sw_ring[rx_id];
		eop = staterr & NGBE_RXD_STAT_EOP;

		next_id = rx_id + 1;
		if (next_id == rxq->nb_rx_desc)
			next_id = 0;

		/* Prefetch next mbuf while processing current one. */
		rte_ngbe_prefetch(sw_ring[next_id].mbuf);

		/*
		 * When next Rx descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 4 pointers
		 * to mbufs.
		 */
		if ((next_id & 0x3) == 0) {
			rte_ngbe_prefetch(&rx_ring[next_id]);
			rte_ngbe_prefetch(&sw_ring[next_id]);
		}

		rxm = rxe->mbuf;

		if (!bulk_alloc) {
			__le64 dma =
			  rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
			/*
			 * Update Rx descriptor with the physical address of the
			 * new data buffer of the new allocated mbuf.
			 */
			rxe->mbuf = nmb;

			rxm->data_off = RTE_PKTMBUF_HEADROOM;
			NGBE_RXD_HDRADDR(rxdp, 0);
			NGBE_RXD_PKTADDR(rxdp, dma);
		} else {
			rxe->mbuf = NULL;
		}

		/*
		 * Set data length & data buffer address of mbuf.
		 */
		data_len = rte_le_to_cpu_16(rxd.qw1.hi.len);
		rxm->data_len = data_len;

		if (!eop) {
			uint16_t nextp_id;

			nextp_id = next_id;
			next_sc_entry = &sw_sc_ring[nextp_id];
			next_rxe = &sw_ring[nextp_id];
			rte_ngbe_prefetch(next_rxe);
		}

		sc_entry = &sw_sc_ring[rx_id];
		first_seg = sc_entry->fbuf;
		sc_entry->fbuf = NULL;

		/*
		 * If this is the first buffer of the received packet,
		 * set the pointer to the first mbuf of the packet and
		 * initialize its context.
		 * Otherwise, update the total length and the number of segments
		 * of the current scattered packet, and update the pointer to
		 * the last mbuf of the current packet.
		 */
		if (first_seg == NULL) {
			first_seg = rxm;
			first_seg->pkt_len = data_len;
			first_seg->nb_segs = 1;
		} else {
			first_seg->pkt_len += data_len;
			first_seg->nb_segs++;
		}

		prev_id = rx_id;
		rx_id = next_id;

		/*
		 * If this is not the last buffer of the received packet, update
		 * the pointer to the first mbuf at the NEXTP entry in the
		 * sw_sc_ring and continue to parse the Rx ring.
		 */
		if (!eop && next_rxe) {
			rxm->next = next_rxe->mbuf;
			next_sc_entry->fbuf = first_seg;
			goto next_desc;
		}

		/* Initialize the first mbuf of the returned packet */
		ngbe_fill_cluster_head_buf(first_seg, &rxd, rxq, staterr);

		/* Deal with the case, when HW CRC srip is disabled. */
		first_seg->pkt_len -= rxq->crc_len;
		if (unlikely(rxm->data_len <= rxq->crc_len)) {
			struct rte_mbuf *lp;

			for (lp = first_seg; lp->next != rxm; lp = lp->next)
				;

			first_seg->nb_segs--;
			lp->data_len -= rxq->crc_len - rxm->data_len;
			lp->next = NULL;
			rte_pktmbuf_free_seg(rxm);
		} else {
			rxm->data_len -= rxq->crc_len;
		}

		/* Prefetch data of first segment, if configured to do so. */
		rte_packet_prefetch((char *)first_seg->buf_addr +
			first_seg->data_off);

		/*
		 * Store the mbuf address into the next entry of the array
		 * of returned packets.
		 */
		rx_pkts[nb_rx++] = first_seg;
	}

	/*
	 * Record index of the next Rx descriptor to probe.
	 */
	rxq->rx_tail = rx_id;

	/*
	 * If the number of free Rx descriptors is greater than the Rx free
	 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
	 * register.
	 * Update the RDT with the value of the last processed Rx descriptor
	 * minus 1, to guarantee that the RDT register is never equal to the
	 * RDH register, which creates a "full" ring situation from the
	 * hardware point of view...
	 */
	if (!bulk_alloc && nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_tail=%u "
			   "nb_hold=%u nb_rx=%u",
			   rxq->port_id, rxq->queue_id, rx_id, nb_hold, nb_rx);

		rte_wmb();
		ngbe_set32_relaxed(rxq->rdt_reg_addr, prev_id);
		nb_hold = 0;
	}

	rxq->nb_rx_hold = nb_hold;
	return nb_rx;
}

uint16_t
ngbe_recv_pkts_sc_single_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
				 uint16_t nb_pkts)
{
	return ngbe_recv_pkts_sc(rx_queue, rx_pkts, nb_pkts, false);
}

uint16_t
ngbe_recv_pkts_sc_bulk_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
			       uint16_t nb_pkts)
{
	return ngbe_recv_pkts_sc(rx_queue, rx_pkts, nb_pkts, true);
}

/*********************************************************************
 *
 *  Queue management functions
 *
 **********************************************************************/

static void
ngbe_tx_queue_release_mbufs(struct ngbe_tx_queue *txq)
{
	unsigned int i;

	if (txq->sw_ring != NULL) {
		for (i = 0; i < txq->nb_tx_desc; i++) {
			if (txq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
				txq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static int
ngbe_tx_done_cleanup_full(struct ngbe_tx_queue *txq, uint32_t free_cnt)
{
	struct ngbe_tx_entry *swr_ring = txq->sw_ring;
	uint16_t i, tx_last, tx_id;
	uint16_t nb_tx_free_last;
	uint16_t nb_tx_to_clean;
	uint32_t pkt_cnt;

	/* Start free mbuf from the next of tx_tail */
	tx_last = txq->tx_tail;
	tx_id  = swr_ring[tx_last].next_id;

	if (txq->nb_tx_free == 0 && ngbe_xmit_cleanup(txq))
		return 0;

	nb_tx_to_clean = txq->nb_tx_free;
	nb_tx_free_last = txq->nb_tx_free;
	if (!free_cnt)
		free_cnt = txq->nb_tx_desc;

	/* Loop through swr_ring to count the amount of
	 * freeable mubfs and packets.
	 */
	for (pkt_cnt = 0; pkt_cnt < free_cnt; ) {
		for (i = 0; i < nb_tx_to_clean &&
			pkt_cnt < free_cnt &&
			tx_id != tx_last; i++) {
			if (swr_ring[tx_id].mbuf != NULL) {
				rte_pktmbuf_free_seg(swr_ring[tx_id].mbuf);
				swr_ring[tx_id].mbuf = NULL;

				/*
				 * last segment in the packet,
				 * increment packet count
				 */
				pkt_cnt += (swr_ring[tx_id].last_id == tx_id);
			}

			tx_id = swr_ring[tx_id].next_id;
		}

		if (pkt_cnt < free_cnt) {
			if (ngbe_xmit_cleanup(txq))
				break;

			nb_tx_to_clean = txq->nb_tx_free - nb_tx_free_last;
			nb_tx_free_last = txq->nb_tx_free;
		}
	}

	return (int)pkt_cnt;
}

static int
ngbe_tx_done_cleanup_simple(struct ngbe_tx_queue *txq,
			uint32_t free_cnt)
{
	int i, n, cnt;

	if (free_cnt == 0 || free_cnt > txq->nb_tx_desc)
		free_cnt = txq->nb_tx_desc;

	cnt = free_cnt - free_cnt % txq->tx_free_thresh;

	for (i = 0; i < cnt; i += n) {
		if (txq->nb_tx_desc - txq->nb_tx_free < txq->tx_free_thresh)
			break;

		n = ngbe_tx_free_bufs(txq);

		if (n == 0)
			break;
	}

	return i;
}

int
ngbe_dev_tx_done_cleanup(void *tx_queue, uint32_t free_cnt)
{
	struct ngbe_tx_queue *txq = (struct ngbe_tx_queue *)tx_queue;
	if (txq->offloads == 0 &&
		txq->tx_free_thresh >= RTE_PMD_NGBE_TX_MAX_BURST)
		return ngbe_tx_done_cleanup_simple(txq, free_cnt);

	return ngbe_tx_done_cleanup_full(txq, free_cnt);
}

static void
ngbe_tx_free_swring(struct ngbe_tx_queue *txq)
{
	if (txq != NULL)
		rte_free(txq->sw_ring);
}

static void
ngbe_tx_queue_release(struct ngbe_tx_queue *txq)
{
	if (txq != NULL) {
		if (txq->ops != NULL) {
			txq->ops->release_mbufs(txq);
			txq->ops->free_swring(txq);
		}
		rte_free(txq);
	}
}

void
ngbe_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	ngbe_tx_queue_release(dev->data->tx_queues[qid]);
}

/* (Re)set dynamic ngbe_tx_queue fields to defaults */
static void
ngbe_reset_tx_queue(struct ngbe_tx_queue *txq)
{
	static const struct ngbe_tx_desc zeroed_desc = {0};
	struct ngbe_tx_entry *txe = txq->sw_ring;
	uint16_t prev, i;

	/* Zero out HW ring memory */
	for (i = 0; i < txq->nb_tx_desc; i++)
		txq->tx_ring[i] = zeroed_desc;

	/* Initialize SW ring entries */
	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		/* the ring can also be modified by hardware */
		volatile struct ngbe_tx_desc *txd = &txq->tx_ring[i];

		txd->dw3 = rte_cpu_to_le_32(NGBE_TXD_DD);
		txe[i].mbuf = NULL;
		txe[i].last_id = i;
		txe[prev].next_id = i;
		prev = i;
	}

	txq->tx_next_dd = (uint16_t)(txq->tx_free_thresh - 1);
	txq->tx_tail = 0;

	/*
	 * Always allow 1 descriptor to be un-allocated to avoid
	 * a H/W race condition
	 */
	txq->last_desc_cleaned = (uint16_t)(txq->nb_tx_desc - 1);
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_desc - 1);
	txq->ctx_curr = 0;
	memset((void *)&txq->ctx_cache, 0,
		NGBE_CTX_NUM * sizeof(struct ngbe_ctx_info));
}

static const struct ngbe_txq_ops def_txq_ops = {
	.release_mbufs = ngbe_tx_queue_release_mbufs,
	.free_swring = ngbe_tx_free_swring,
	.reset = ngbe_reset_tx_queue,
};

/* Takes an ethdev and a queue and sets up the tx function to be used based on
 * the queue parameters. Used in tx_queue_setup by primary process and then
 * in dev_init by secondary process when attaching to an existing ethdev.
 */
void
ngbe_set_tx_function(struct rte_eth_dev *dev, struct ngbe_tx_queue *txq)
{
	/* Use a simple Tx queue (no offloads, no multi segs) if possible */
	if (txq->offloads == 0 &&
			txq->tx_free_thresh >= RTE_PMD_NGBE_TX_MAX_BURST) {
		PMD_INIT_LOG(DEBUG, "Using simple tx code path");
		dev->tx_pkt_burst = ngbe_xmit_pkts_simple;
		dev->tx_pkt_prepare = NULL;
	} else {
		PMD_INIT_LOG(DEBUG, "Using full-featured tx code path");
		PMD_INIT_LOG(DEBUG,
				" - offloads = 0x%" PRIx64,
				txq->offloads);
		PMD_INIT_LOG(DEBUG,
				" - tx_free_thresh = %lu [RTE_PMD_NGBE_TX_MAX_BURST=%lu]",
				(unsigned long)txq->tx_free_thresh,
				(unsigned long)RTE_PMD_NGBE_TX_MAX_BURST);
		dev->tx_pkt_burst = ngbe_xmit_pkts;
		dev->tx_pkt_prepare = ngbe_prep_pkts;
	}
}

static const struct {
	eth_tx_burst_t pkt_burst;
	const char *info;
} ngbe_tx_burst_infos[] = {
	{ ngbe_xmit_pkts_simple,   "Scalar Simple"},
	{ ngbe_xmit_pkts,          "Scalar"},
};

int
ngbe_tx_burst_mode_get(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id,
		      struct rte_eth_burst_mode *mode)
{
	eth_tx_burst_t pkt_burst = dev->tx_pkt_burst;
	int ret = -EINVAL;
	unsigned int i;

	for (i = 0; i < RTE_DIM(ngbe_tx_burst_infos); ++i) {
		if (pkt_burst == ngbe_tx_burst_infos[i].pkt_burst) {
			snprintf(mode->info, sizeof(mode->info), "%s",
				 ngbe_tx_burst_infos[i].info);
			ret = 0;
			break;
		}
	}

	return ret;
}

uint64_t
ngbe_get_tx_port_offloads(struct rte_eth_dev *dev)
{
	uint64_t tx_offload_capa;
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM  |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM   |
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM   |
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM  |
		RTE_ETH_TX_OFFLOAD_TCP_TSO     |
		RTE_ETH_TX_OFFLOAD_UDP_TSO	   |
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	if (hw->is_pf)
		tx_offload_capa |= RTE_ETH_TX_OFFLOAD_QINQ_INSERT;

	return tx_offload_capa;
}

int
ngbe_dev_tx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_txconf *tx_conf)
{
	const struct rte_memzone *tz;
	struct ngbe_tx_queue *txq;
	struct ngbe_hw     *hw;
	uint16_t tx_free_thresh;
	uint64_t offloads;

	PMD_INIT_FUNC_TRACE();
	hw = ngbe_dev_hw(dev);

	offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;

	/*
	 * The Tx descriptor ring will be cleaned after txq->tx_free_thresh
	 * descriptors are used or if the number of descriptors required
	 * to transmit a packet is greater than the number of free Tx
	 * descriptors.
	 * One descriptor in the Tx ring is used as a sentinel to avoid a
	 * H/W race condition, hence the maximum threshold constraints.
	 * When set to zero use default values.
	 */
	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh) ?
			tx_conf->tx_free_thresh : DEFAULT_TX_FREE_THRESH);
	if (tx_free_thresh >= (nb_desc - 3)) {
		PMD_INIT_LOG(ERR,
			     "tx_free_thresh must be less than the number of TX descriptors minus 3. (tx_free_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	if (nb_desc % tx_free_thresh != 0) {
		PMD_INIT_LOG(ERR,
			     "tx_free_thresh must be a divisor of the number of Tx descriptors. (tx_free_thresh=%u port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	/* Free memory prior to re-allocation if needed... */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		ngbe_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* First allocate the Tx queue data structure */
	txq = rte_zmalloc_socket("ethdev Tx queue",
				 sizeof(struct ngbe_tx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL)
		return -ENOMEM;

	/*
	 * Allocate Tx ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx,
			sizeof(struct ngbe_tx_desc) * NGBE_RING_DESC_MAX,
			NGBE_ALIGN, socket_id);
	if (tz == NULL) {
		ngbe_tx_queue_release(txq);
		return -ENOMEM;
	}

	txq->nb_tx_desc = nb_desc;
	txq->tx_free_thresh = tx_free_thresh;
	txq->pthresh = tx_conf->tx_thresh.pthresh;
	txq->hthresh = tx_conf->tx_thresh.hthresh;
	txq->wthresh = tx_conf->tx_thresh.wthresh;
	txq->queue_id = queue_idx;
	txq->reg_idx = (uint16_t)((RTE_ETH_DEV_SRIOV(dev).active == 0) ?
		queue_idx : RTE_ETH_DEV_SRIOV(dev).def_pool_q_idx + queue_idx);
	txq->port_id = dev->data->port_id;
	txq->offloads = offloads;
	txq->ops = &def_txq_ops;
	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	txq->tdt_reg_addr = NGBE_REG_ADDR(hw, NGBE_TXWP(txq->reg_idx));
	txq->tdc_reg_addr = NGBE_REG_ADDR(hw, NGBE_TXCFG(txq->reg_idx));

	txq->tx_ring_phys_addr = TMZ_PADDR(tz);
	txq->tx_ring = (struct ngbe_tx_desc *)TMZ_VADDR(tz);

	/* Allocate software ring */
	txq->sw_ring = rte_zmalloc_socket("txq->sw_ring",
				sizeof(struct ngbe_tx_entry) * nb_desc,
				RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL) {
		ngbe_tx_queue_release(txq);
		return -ENOMEM;
	}
	PMD_INIT_LOG(DEBUG,
		     "sw_ring=%p hw_ring=%p dma_addr=0x%" PRIx64,
		     txq->sw_ring, txq->tx_ring, txq->tx_ring_phys_addr);

	/* set up scalar Tx function as appropriate */
	ngbe_set_tx_function(dev, txq);

	txq->ops->reset(txq);

	dev->data->tx_queues[queue_idx] = txq;

	return 0;
}

/**
 * ngbe_free_sc_cluster - free the not-yet-completed scattered cluster
 *
 * The "next" pointer of the last segment of (not-yet-completed) RSC clusters
 * in the sw_sc_ring is not set to NULL but rather points to the next
 * mbuf of this RSC aggregation (that has not been completed yet and still
 * resides on the HW ring). So, instead of calling for rte_pktmbuf_free() we
 * will just free first "nb_segs" segments of the cluster explicitly by calling
 * an rte_pktmbuf_free_seg().
 *
 * @m scattered cluster head
 */
static void
ngbe_free_sc_cluster(struct rte_mbuf *m)
{
	uint16_t i, nb_segs = m->nb_segs;
	struct rte_mbuf *next_seg;

	for (i = 0; i < nb_segs; i++) {
		next_seg = m->next;
		rte_pktmbuf_free_seg(m);
		m = next_seg;
	}
}

static void
ngbe_rx_queue_release_mbufs(struct ngbe_rx_queue *rxq)
{
	unsigned int i;

	if (rxq->sw_ring != NULL) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
				rxq->sw_ring[i].mbuf = NULL;
			}
		}
		for (i = 0; i < rxq->rx_nb_avail; ++i) {
			struct rte_mbuf *mb;

			mb = rxq->rx_stage[rxq->rx_next_avail + i];
			rte_pktmbuf_free_seg(mb);
		}
		rxq->rx_nb_avail = 0;
	}

	if (rxq->sw_sc_ring != NULL)
		for (i = 0; i < rxq->nb_rx_desc; i++)
			if (rxq->sw_sc_ring[i].fbuf != NULL) {
				ngbe_free_sc_cluster(rxq->sw_sc_ring[i].fbuf);
				rxq->sw_sc_ring[i].fbuf = NULL;
			}
}

static void
ngbe_rx_queue_release(struct ngbe_rx_queue *rxq)
{
	if (rxq != NULL) {
		ngbe_rx_queue_release_mbufs(rxq);
		rte_free(rxq->sw_ring);
		rte_free(rxq->sw_sc_ring);
		rte_free(rxq);
	}
}

void
ngbe_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	ngbe_rx_queue_release(dev->data->rx_queues[qid]);
}

/*
 * Check if Rx Burst Bulk Alloc function can be used.
 * Return
 *        0: the preconditions are satisfied and the bulk allocation function
 *           can be used.
 *  -EINVAL: the preconditions are NOT satisfied and the default Rx burst
 *           function must be used.
 */
static inline int
check_rx_burst_bulk_alloc_preconditions(struct ngbe_rx_queue *rxq)
{
	int ret = 0;

	/*
	 * Make sure the following pre-conditions are satisfied:
	 *   rxq->rx_free_thresh >= RTE_PMD_NGBE_RX_MAX_BURST
	 *   rxq->rx_free_thresh < rxq->nb_rx_desc
	 *   (rxq->nb_rx_desc % rxq->rx_free_thresh) == 0
	 * Scattered packets are not supported.  This should be checked
	 * outside of this function.
	 */
	if (rxq->rx_free_thresh < RTE_PMD_NGBE_RX_MAX_BURST) {
		PMD_INIT_LOG(DEBUG,
			     "Rx Burst Bulk Alloc Preconditions: rxq->rx_free_thresh=%d, RTE_PMD_NGBE_RX_MAX_BURST=%d",
			     rxq->rx_free_thresh, RTE_PMD_NGBE_RX_MAX_BURST);
		ret = -EINVAL;
	} else if (rxq->rx_free_thresh >= rxq->nb_rx_desc) {
		PMD_INIT_LOG(DEBUG,
			     "Rx Burst Bulk Alloc Preconditions: rxq->rx_free_thresh=%d, rxq->nb_rx_desc=%d",
			     rxq->rx_free_thresh, rxq->nb_rx_desc);
		ret = -EINVAL;
	} else if ((rxq->nb_rx_desc % rxq->rx_free_thresh) != 0) {
		PMD_INIT_LOG(DEBUG,
			     "Rx Burst Bulk Alloc Preconditions: rxq->nb_rx_desc=%d, rxq->rx_free_thresh=%d",
			     rxq->nb_rx_desc, rxq->rx_free_thresh);
		ret = -EINVAL;
	}

	return ret;
}

/* Reset dynamic ngbe_rx_queue fields back to defaults */
static void
ngbe_reset_rx_queue(struct ngbe_adapter *adapter, struct ngbe_rx_queue *rxq)
{
	static const struct ngbe_rx_desc zeroed_desc = {
						{{0}, {0} }, {{0}, {0} } };
	unsigned int i;
	uint16_t len = rxq->nb_rx_desc;

	/*
	 * By default, the Rx queue setup function allocates enough memory for
	 * NGBE_RING_DESC_MAX.  The Rx Burst bulk allocation function requires
	 * extra memory at the end of the descriptor ring to be zero'd out.
	 */
	if (adapter->rx_bulk_alloc_allowed)
		/* zero out extra memory */
		len += RTE_PMD_NGBE_RX_MAX_BURST;

	/*
	 * Zero out HW ring memory. Zero out extra memory at the end of
	 * the H/W ring so look-ahead logic in Rx Burst bulk alloc function
	 * reads extra memory as zeros.
	 */
	for (i = 0; i < len; i++)
		rxq->rx_ring[i] = zeroed_desc;

	/*
	 * initialize extra software ring entries. Space for these extra
	 * entries is always allocated
	 */
	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));
	for (i = rxq->nb_rx_desc; i < len; ++i)
		rxq->sw_ring[i].mbuf = &rxq->fake_mbuf;

	rxq->rx_nb_avail = 0;
	rxq->rx_next_avail = 0;
	rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);
	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

uint64_t
ngbe_get_rx_queue_offloads(struct rte_eth_dev *dev __rte_unused)
{
	return RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
}

uint64_t
ngbe_get_rx_port_offloads(struct rte_eth_dev *dev)
{
	uint64_t offloads;
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	offloads = RTE_ETH_RX_OFFLOAD_IPV4_CKSUM  |
		   RTE_ETH_RX_OFFLOAD_UDP_CKSUM   |
		   RTE_ETH_RX_OFFLOAD_TCP_CKSUM   |
		   RTE_ETH_RX_OFFLOAD_KEEP_CRC    |
		   RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
		   RTE_ETH_RX_OFFLOAD_SCATTER;

	if (hw->is_pf)
		offloads |= (RTE_ETH_RX_OFFLOAD_QINQ_STRIP |
			     RTE_ETH_RX_OFFLOAD_VLAN_EXTEND);

	return offloads;
}

int
ngbe_dev_rx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	const struct rte_memzone *rz;
	struct ngbe_rx_queue *rxq;
	struct ngbe_hw     *hw;
	uint16_t len;
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);
	uint64_t offloads;

	PMD_INIT_FUNC_TRACE();
	hw = ngbe_dev_hw(dev);

	offloads = rx_conf->offloads | dev->data->dev_conf.rxmode.offloads;

	/* Free memory prior to re-allocation if needed... */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		ngbe_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* First allocate the Rx queue data structure */
	rxq = rte_zmalloc_socket("ethdev RX queue",
				 sizeof(struct ngbe_rx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL)
		return -ENOMEM;
	rxq->mb_pool = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->reg_idx = (uint16_t)((RTE_ETH_DEV_SRIOV(dev).active == 0) ?
		queue_idx : RTE_ETH_DEV_SRIOV(dev).def_pool_q_idx + queue_idx);
	rxq->port_id = dev->data->port_id;
	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		rxq->crc_len = RTE_ETHER_CRC_LEN;
	else
		rxq->crc_len = 0;
	rxq->drop_en = rx_conf->rx_drop_en;
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;
	rxq->offloads = offloads;

	/*
	 * Allocate Rx ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	rz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx,
				      RX_RING_SZ, NGBE_ALIGN, socket_id);
	if (rz == NULL) {
		ngbe_rx_queue_release(rxq);
		return -ENOMEM;
	}

	/*
	 * Zero init all the descriptors in the ring.
	 */
	memset(rz->addr, 0, RX_RING_SZ);

	rxq->rdt_reg_addr = NGBE_REG_ADDR(hw, NGBE_RXWP(rxq->reg_idx));
	rxq->rdh_reg_addr = NGBE_REG_ADDR(hw, NGBE_RXRP(rxq->reg_idx));

	rxq->rx_ring_phys_addr = TMZ_PADDR(rz);
	rxq->rx_ring = (struct ngbe_rx_desc *)TMZ_VADDR(rz);

	/*
	 * Certain constraints must be met in order to use the bulk buffer
	 * allocation Rx burst function. If any of Rx queues doesn't meet them
	 * the feature should be disabled for the whole port.
	 */
	if (check_rx_burst_bulk_alloc_preconditions(rxq)) {
		PMD_INIT_LOG(DEBUG,
			     "queue[%d] doesn't meet Rx Bulk Alloc preconditions - canceling the feature for the whole port[%d]",
			     rxq->queue_id, rxq->port_id);
		adapter->rx_bulk_alloc_allowed = false;
	}

	/*
	 * Allocate software ring. Allow for space at the end of the
	 * S/W ring to make sure look-ahead logic in bulk alloc Rx burst
	 * function does not access an invalid memory region.
	 */
	len = nb_desc;
	if (adapter->rx_bulk_alloc_allowed)
		len += RTE_PMD_NGBE_RX_MAX_BURST;

	rxq->sw_ring = rte_zmalloc_socket("rxq->sw_ring",
					  sizeof(struct ngbe_rx_entry) * len,
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_ring == NULL) {
		ngbe_rx_queue_release(rxq);
		return -ENOMEM;
	}

	/*
	 * Always allocate even if it's not going to be needed in order to
	 * simplify the code.
	 *
	 * This ring is used in Scattered Rx cases and Scattered Rx may
	 * be requested in ngbe_dev_rx_init(), which is called later from
	 * dev_start() flow.
	 */
	rxq->sw_sc_ring =
		rte_zmalloc_socket("rxq->sw_sc_ring",
				  sizeof(struct ngbe_scattered_rx_entry) * len,
				  RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_sc_ring == NULL) {
		ngbe_rx_queue_release(rxq);
		return -ENOMEM;
	}

	PMD_INIT_LOG(DEBUG,
		     "sw_ring=%p sw_sc_ring=%p hw_ring=%p dma_addr=0x%" PRIx64,
		     rxq->sw_ring, rxq->sw_sc_ring, rxq->rx_ring,
		     rxq->rx_ring_phys_addr);

	dev->data->rx_queues[queue_idx] = rxq;

	ngbe_reset_rx_queue(adapter, rxq);

	return 0;
}

uint32_t
ngbe_dev_rx_queue_count(void *rx_queue)
{
#define NGBE_RXQ_SCAN_INTERVAL 4
	volatile struct ngbe_rx_desc *rxdp;
	struct ngbe_rx_queue *rxq = rx_queue;
	uint32_t desc = 0;

	rxdp = &rxq->rx_ring[rxq->rx_tail];

	while ((desc < rxq->nb_rx_desc) &&
		(rxdp->qw1.lo.status &
			rte_cpu_to_le_32(NGBE_RXD_STAT_DD))) {
		desc += NGBE_RXQ_SCAN_INTERVAL;
		rxdp += NGBE_RXQ_SCAN_INTERVAL;
		if (rxq->rx_tail + desc >= rxq->nb_rx_desc)
			rxdp = &(rxq->rx_ring[rxq->rx_tail +
				desc - rxq->nb_rx_desc]);
	}

	return desc;
}

int
ngbe_dev_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	struct ngbe_rx_queue *rxq = rx_queue;
	volatile uint32_t *status;
	uint32_t nb_hold, desc;

	if (unlikely(offset >= rxq->nb_rx_desc))
		return -EINVAL;

	nb_hold = rxq->nb_rx_hold;
	if (offset >= rxq->nb_rx_desc - nb_hold)
		return RTE_ETH_RX_DESC_UNAVAIL;

	desc = rxq->rx_tail + offset;
	if (desc >= rxq->nb_rx_desc)
		desc -= rxq->nb_rx_desc;

	status = &rxq->rx_ring[desc].qw1.lo.status;
	if (*status & rte_cpu_to_le_32(NGBE_RXD_STAT_DD))
		return RTE_ETH_RX_DESC_DONE;

	return RTE_ETH_RX_DESC_AVAIL;
}

int
ngbe_dev_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	struct ngbe_tx_queue *txq = tx_queue;
	volatile uint32_t *status;
	uint32_t desc;

	if (unlikely(offset >= txq->nb_tx_desc))
		return -EINVAL;

	desc = txq->tx_tail + offset;
	if (desc >= txq->nb_tx_desc) {
		desc -= txq->nb_tx_desc;
		if (desc >= txq->nb_tx_desc)
			desc -= txq->nb_tx_desc;
	}

	status = &txq->tx_ring[desc].dw3;
	if (*status & rte_cpu_to_le_32(NGBE_TXD_DD))
		return RTE_ETH_TX_DESC_DONE;

	return RTE_ETH_TX_DESC_FULL;
}

void
ngbe_dev_clear_queues(struct rte_eth_dev *dev)
{
	unsigned int i;
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct ngbe_tx_queue *txq = dev->data->tx_queues[i];

		if (txq != NULL) {
			txq->ops->release_mbufs(txq);
			txq->ops->reset(txq);
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct ngbe_rx_queue *rxq = dev->data->rx_queues[i];

		if (rxq != NULL) {
			ngbe_rx_queue_release_mbufs(rxq);
			ngbe_reset_rx_queue(adapter, rxq);
		}
	}
}

void
ngbe_dev_free_queues(struct rte_eth_dev *dev)
{
	unsigned int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		ngbe_dev_rx_queue_release(dev, i);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		ngbe_dev_tx_queue_release(dev, i);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}

/**
 * Receive Side Scaling (RSS)
 *
 * Principles:
 * The source and destination IP addresses of the IP header and the source
 * and destination ports of TCP/UDP headers, if any, of received packets are
 * hashed against a configurable random key to compute a 32-bit RSS hash result.
 * The seven (7) LSBs of the 32-bit hash result are used as an index into a
 * 128-entry redirection table (RETA).  Each entry of the RETA provides a 3-bit
 * RSS output index which is used as the Rx queue index where to store the
 * received packets.
 * The following output is supplied in the Rx write-back descriptor:
 *     - 32-bit result of the Microsoft RSS hash function,
 *     - 4-bit RSS type field.
 */

/*
 * Used as the default key.
 */
static uint8_t rss_intel_key[40] = {
	0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
	0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
	0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
	0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
	0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA,
};

static void
ngbe_rss_disable(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);

	wr32m(hw, NGBE_RACTL, NGBE_RACTL_RSSENA, 0);
}

int
ngbe_dev_rss_hash_update(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint8_t  *hash_key;
	uint32_t mrqc;
	uint32_t rss_key;
	uint64_t rss_hf;
	uint16_t i;

	if (!hw->is_pf) {
		PMD_DRV_LOG(ERR, "RSS hash update is not supported on this "
			"NIC.");
		return -ENOTSUP;
	}

	hash_key = rss_conf->rss_key;
	if (hash_key) {
		/* Fill in RSS hash key */
		for (i = 0; i < 10; i++) {
			rss_key  = LS32(hash_key[(i * 4) + 0], 0, 0xFF);
			rss_key |= LS32(hash_key[(i * 4) + 1], 8, 0xFF);
			rss_key |= LS32(hash_key[(i * 4) + 2], 16, 0xFF);
			rss_key |= LS32(hash_key[(i * 4) + 3], 24, 0xFF);
			wr32a(hw, NGBE_REG_RSSKEY, i, rss_key);
		}
	}

	/* Set configured hashing protocols */
	rss_hf = rss_conf->rss_hf & NGBE_RSS_OFFLOAD_ALL;

	mrqc = rd32(hw, NGBE_RACTL);
	mrqc &= ~NGBE_RACTL_RSSMASK;
	if (rss_hf & RTE_ETH_RSS_IPV4)
		mrqc |= NGBE_RACTL_RSSIPV4;
	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP)
		mrqc |= NGBE_RACTL_RSSIPV4TCP;
	if (rss_hf & RTE_ETH_RSS_IPV6 ||
	    rss_hf & RTE_ETH_RSS_IPV6_EX)
		mrqc |= NGBE_RACTL_RSSIPV6;
	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP ||
	    rss_hf & RTE_ETH_RSS_IPV6_TCP_EX)
		mrqc |= NGBE_RACTL_RSSIPV6TCP;
	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP)
		mrqc |= NGBE_RACTL_RSSIPV4UDP;
	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP ||
	    rss_hf & RTE_ETH_RSS_IPV6_UDP_EX)
		mrqc |= NGBE_RACTL_RSSIPV6UDP;

	if (rss_hf)
		mrqc |= NGBE_RACTL_RSSENA;
	else
		mrqc &= ~NGBE_RACTL_RSSENA;

	wr32(hw, NGBE_RACTL, mrqc);

	return 0;
}

int
ngbe_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			    struct rte_eth_rss_conf *rss_conf)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint8_t *hash_key;
	uint32_t mrqc;
	uint32_t rss_key;
	uint64_t rss_hf;
	uint16_t i;

	hash_key = rss_conf->rss_key;
	if (hash_key) {
		/* Return RSS hash key */
		for (i = 0; i < 10; i++) {
			rss_key = rd32a(hw, NGBE_REG_RSSKEY, i);
			hash_key[(i * 4) + 0] = RS32(rss_key, 0, 0xFF);
			hash_key[(i * 4) + 1] = RS32(rss_key, 8, 0xFF);
			hash_key[(i * 4) + 2] = RS32(rss_key, 16, 0xFF);
			hash_key[(i * 4) + 3] = RS32(rss_key, 24, 0xFF);
		}
	}

	rss_hf = 0;

	mrqc = rd32(hw, NGBE_RACTL);
	if (mrqc & NGBE_RACTL_RSSIPV4)
		rss_hf |= RTE_ETH_RSS_IPV4;
	if (mrqc & NGBE_RACTL_RSSIPV4TCP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_TCP;
	if (mrqc & NGBE_RACTL_RSSIPV6)
		rss_hf |= RTE_ETH_RSS_IPV6 |
			  RTE_ETH_RSS_IPV6_EX;
	if (mrqc & NGBE_RACTL_RSSIPV6TCP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_TCP |
			  RTE_ETH_RSS_IPV6_TCP_EX;
	if (mrqc & NGBE_RACTL_RSSIPV4UDP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_UDP;
	if (mrqc & NGBE_RACTL_RSSIPV6UDP)
		rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_UDP |
			  RTE_ETH_RSS_IPV6_UDP_EX;
	if (!(mrqc & NGBE_RACTL_RSSENA))
		rss_hf = 0;

	rss_hf &= NGBE_RSS_OFFLOAD_ALL;

	rss_conf->rss_hf = rss_hf;
	return 0;
}

static void
ngbe_rss_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_rss_conf rss_conf;
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	uint32_t reta;
	uint16_t i;
	uint16_t j;

	PMD_INIT_FUNC_TRACE();

	/*
	 * Fill in redirection table
	 * The byte-swap is needed because NIC registers are in
	 * little-endian order.
	 */
	if (adapter->rss_reta_updated == 0) {
		reta = 0;
		for (i = 0, j = 0; i < RTE_ETH_RSS_RETA_SIZE_128; i++, j++) {
			if (j == dev->data->nb_rx_queues)
				j = 0;
			reta = (reta >> 8) | LS32(j, 24, 0xFF);
			if ((i & 3) == 3)
				wr32a(hw, NGBE_REG_RSSTBL, i >> 2, reta);
		}
	}
	/*
	 * Configure the RSS key and the RSS protocols used to compute
	 * the RSS hash of input packets.
	 */
	rss_conf = dev->data->dev_conf.rx_adv_conf.rss_conf;
	if (rss_conf.rss_key == NULL)
		rss_conf.rss_key = rss_intel_key; /* Default hash key */
	ngbe_dev_rss_hash_update(dev, &rss_conf);
}

void ngbe_configure_port(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	int i = 0;
	uint16_t tpids[8] = {RTE_ETHER_TYPE_VLAN, RTE_ETHER_TYPE_QINQ,
				0x9100, 0x9200,
				0x0000, 0x0000,
				0x0000, 0x0000};

	PMD_INIT_FUNC_TRACE();

	/* default outer vlan tpid */
	wr32(hw, NGBE_EXTAG,
		NGBE_EXTAG_ETAG(RTE_ETHER_TYPE_ETAG) |
		NGBE_EXTAG_VLAN(RTE_ETHER_TYPE_QINQ));

	/* default inner vlan tpid */
	wr32m(hw, NGBE_VLANCTL,
		NGBE_VLANCTL_TPID_MASK,
		NGBE_VLANCTL_TPID(RTE_ETHER_TYPE_VLAN));
	wr32m(hw, NGBE_DMATXCTRL,
		NGBE_DMATXCTRL_TPID_MASK,
		NGBE_DMATXCTRL_TPID(RTE_ETHER_TYPE_VLAN));

	/* default vlan tpid filters */
	for (i = 0; i < 8; i++) {
		wr32m(hw, NGBE_TAGTPID(i / 2),
			(i % 2 ? NGBE_TAGTPID_MSB_MASK
			       : NGBE_TAGTPID_LSB_MASK),
			(i % 2 ? NGBE_TAGTPID_MSB(tpids[i])
			       : NGBE_TAGTPID_LSB(tpids[i])));
	}
}

static int
ngbe_alloc_rx_queue_mbufs(struct ngbe_rx_queue *rxq)
{
	struct ngbe_rx_entry *rxe = rxq->sw_ring;
	uint64_t dma_addr;
	unsigned int i;

	/* Initialize software ring entries */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		/* the ring can also be modified by hardware */
		volatile struct ngbe_rx_desc *rxd;
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);

		if (mbuf == NULL) {
			PMD_INIT_LOG(ERR, "Rx mbuf alloc failed queue_id=%u port_id=%u",
				     (unsigned int)rxq->queue_id,
				     (unsigned int)rxq->port_id);
			return -ENOMEM;
		}

		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->port = rxq->port_id;

		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));
		rxd = &rxq->rx_ring[i];
		NGBE_RXD_HDRADDR(rxd, 0);
		NGBE_RXD_PKTADDR(rxd, dma_addr);
		rxe[i].mbuf = mbuf;
	}

	return 0;
}

static int
ngbe_dev_mq_rx_configure(struct rte_eth_dev *dev)
{
	if (RTE_ETH_DEV_SRIOV(dev).active == 0) {
		switch (dev->data->dev_conf.rxmode.mq_mode) {
		case RTE_ETH_MQ_RX_RSS:
			ngbe_rss_configure(dev);
			break;

		case RTE_ETH_MQ_RX_NONE:
		default:
			/* if mq_mode is none, disable rss mode.*/
			ngbe_rss_disable(dev);
			break;
		}
	}

	return 0;
}

void
ngbe_set_rx_function(struct rte_eth_dev *dev)
{
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);

	if (dev->data->scattered_rx) {
		/*
		 * Set the scattered callback: there are bulk and
		 * single allocation versions.
		 */
		if (adapter->rx_bulk_alloc_allowed) {
			PMD_INIT_LOG(DEBUG, "Using a Scattered with bulk "
					   "allocation callback (port=%d).",
				     dev->data->port_id);
			dev->rx_pkt_burst = ngbe_recv_pkts_sc_bulk_alloc;
		} else {
			PMD_INIT_LOG(DEBUG, "Using Regular (non-vector, "
					    "single allocation) "
					    "Scattered Rx callback "
					    "(port=%d).",
				     dev->data->port_id);

			dev->rx_pkt_burst = ngbe_recv_pkts_sc_single_alloc;
		}
	/*
	 * Below we set "simple" callbacks according to port/queues parameters.
	 * If parameters allow we are going to choose between the following
	 * callbacks:
	 *    - Bulk Allocation
	 *    - Single buffer allocation (the simplest one)
	 */
	} else if (adapter->rx_bulk_alloc_allowed) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
				    "satisfied. Rx Burst Bulk Alloc function "
				    "will be used on port=%d.",
			     dev->data->port_id);

		dev->rx_pkt_burst = ngbe_recv_pkts_bulk_alloc;
	} else {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are not "
				    "satisfied, or Scattered Rx is requested "
				    "(port=%d).",
			     dev->data->port_id);

		dev->rx_pkt_burst = ngbe_recv_pkts;
	}
}

static const struct {
	eth_rx_burst_t pkt_burst;
	const char *info;
} ngbe_rx_burst_infos[] = {
	{ ngbe_recv_pkts_sc_single_alloc,    "Scalar Scattered"},
	{ ngbe_recv_pkts_sc_bulk_alloc,      "Scalar Scattered Bulk Alloc"},
	{ ngbe_recv_pkts_bulk_alloc,         "Scalar Bulk Alloc"},
	{ ngbe_recv_pkts,                    "Scalar"},
};

int
ngbe_rx_burst_mode_get(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id,
		      struct rte_eth_burst_mode *mode)
{
	eth_rx_burst_t pkt_burst = dev->rx_pkt_burst;
	int ret = -EINVAL;
	unsigned int i;

	for (i = 0; i < RTE_DIM(ngbe_rx_burst_infos); ++i) {
		if (pkt_burst == ngbe_rx_burst_infos[i].pkt_burst) {
			snprintf(mode->info, sizeof(mode->info), "%s",
				 ngbe_rx_burst_infos[i].info);
			ret = 0;
			break;
		}
	}

	return ret;
}

/*
 * Initializes Receive Unit.
 */
int
ngbe_dev_rx_init(struct rte_eth_dev *dev)
{
	struct ngbe_hw *hw;
	struct ngbe_rx_queue *rxq;
	uint64_t bus_addr;
	uint32_t fctrl;
	uint32_t hlreg0;
	uint32_t srrctl;
	uint32_t rdrxctl;
	uint32_t rxcsum;
	uint16_t buf_size;
	uint16_t i;
	struct rte_eth_rxmode *rx_conf = &dev->data->dev_conf.rxmode;

	PMD_INIT_FUNC_TRACE();
	hw = ngbe_dev_hw(dev);

	/*
	 * Make sure receives are disabled while setting
	 * up the Rx context (registers, descriptor rings, etc.).
	 */
	wr32m(hw, NGBE_MACRXCFG, NGBE_MACRXCFG_ENA, 0);
	wr32m(hw, NGBE_PBRXCTL, NGBE_PBRXCTL_ENA, 0);

	/* Enable receipt of broadcasted frames */
	fctrl = rd32(hw, NGBE_PSRCTL);
	fctrl |= NGBE_PSRCTL_BCA;
	wr32(hw, NGBE_PSRCTL, fctrl);

	/*
	 * Configure CRC stripping, if any.
	 */
	hlreg0 = rd32(hw, NGBE_SECRXCTL);
	if (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		hlreg0 &= ~NGBE_SECRXCTL_CRCSTRIP;
	else
		hlreg0 |= NGBE_SECRXCTL_CRCSTRIP;
	hlreg0 &= ~NGBE_SECRXCTL_XDSA;
	wr32(hw, NGBE_SECRXCTL, hlreg0);

	/*
	 * Configure jumbo frame support, if any.
	 */
	wr32m(hw, NGBE_FRMSZ, NGBE_FRMSZ_MAX_MASK,
		NGBE_FRMSZ_MAX(dev->data->mtu + NGBE_ETH_OVERHEAD));

	/*
	 * If loopback mode is configured, set LPBK bit.
	 */
	hlreg0 = rd32(hw, NGBE_PSRCTL);
	if (hw->is_pf && dev->data->dev_conf.lpbk_mode)
		hlreg0 |= NGBE_PSRCTL_LBENA;
	else
		hlreg0 &= ~NGBE_PSRCTL_LBENA;

	wr32(hw, NGBE_PSRCTL, hlreg0);

	/*
	 * Assume no header split and no VLAN strip support
	 * on any Rx queue first .
	 */
	rx_conf->offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_STRIP;

	/* Setup Rx queues */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		/*
		 * Reset crc_len in case it was changed after queue setup by a
		 * call to configure.
		 */
		if (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
			rxq->crc_len = RTE_ETHER_CRC_LEN;
		else
			rxq->crc_len = 0;

		/* Setup the Base and Length of the Rx Descriptor Rings */
		bus_addr = rxq->rx_ring_phys_addr;
		wr32(hw, NGBE_RXBAL(rxq->reg_idx),
				(uint32_t)(bus_addr & BIT_MASK32));
		wr32(hw, NGBE_RXBAH(rxq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		wr32(hw, NGBE_RXRP(rxq->reg_idx), 0);
		wr32(hw, NGBE_RXWP(rxq->reg_idx), 0);

		srrctl = NGBE_RXCFG_RNGLEN(rxq->nb_rx_desc);

		/* Set if packets are dropped when no descriptors available */
		if (rxq->drop_en)
			srrctl |= NGBE_RXCFG_DROP;

		/*
		 * Configure the Rx buffer size in the PKTLEN field of
		 * the RXCFG register of the queue.
		 * The value is in 1 KB resolution. Valid values can be from
		 * 1 KB to 16 KB.
		 */
		buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mb_pool) -
			RTE_PKTMBUF_HEADROOM);
		buf_size = ROUND_DOWN(buf_size, 0x1 << 10);
		srrctl |= NGBE_RXCFG_PKTLEN(buf_size);

		wr32(hw, NGBE_RXCFG(rxq->reg_idx), srrctl);

		/* It adds dual VLAN length for supporting dual VLAN */
		if (dev->data->mtu + NGBE_ETH_OVERHEAD +
				2 * RTE_VLAN_HLEN > buf_size)
			dev->data->scattered_rx = 1;
		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			rx_conf->offloads |= RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	}

	if (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_SCATTER)
		dev->data->scattered_rx = 1;

	/*
	 * Device configured with multiple RX queues.
	 */
	ngbe_dev_mq_rx_configure(dev);

	/*
	 * Setup the Checksum Register.
	 * Disable Full-Packet Checksum which is mutually exclusive with RSS.
	 * Enable IP/L4 checksum computation by hardware if requested to do so.
	 */
	rxcsum = rd32(hw, NGBE_PSRCTL);
	rxcsum |= NGBE_PSRCTL_PCSD;
	if (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM)
		rxcsum |= NGBE_PSRCTL_L4CSUM;
	else
		rxcsum &= ~NGBE_PSRCTL_L4CSUM;

	wr32(hw, NGBE_PSRCTL, rxcsum);

	if (hw->is_pf) {
		rdrxctl = rd32(hw, NGBE_SECRXCTL);
		if (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
			rdrxctl &= ~NGBE_SECRXCTL_CRCSTRIP;
		else
			rdrxctl |= NGBE_SECRXCTL_CRCSTRIP;
		wr32(hw, NGBE_SECRXCTL, rdrxctl);
	}

	ngbe_set_rx_function(dev);

	return 0;
}

/*
 * Initializes Transmit Unit.
 */
void
ngbe_dev_tx_init(struct rte_eth_dev *dev)
{
	struct ngbe_hw     *hw;
	struct ngbe_tx_queue *txq;
	uint64_t bus_addr;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();
	hw = ngbe_dev_hw(dev);

	wr32m(hw, NGBE_SECTXCTL, NGBE_SECTXCTL_ODSA, NGBE_SECTXCTL_ODSA);
	wr32m(hw, NGBE_SECTXCTL, NGBE_SECTXCTL_XDSA, 0);

	/* Setup the Base and Length of the Tx Descriptor Rings */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];

		bus_addr = txq->tx_ring_phys_addr;
		wr32(hw, NGBE_TXBAL(txq->reg_idx),
				(uint32_t)(bus_addr & BIT_MASK32));
		wr32(hw, NGBE_TXBAH(txq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		wr32m(hw, NGBE_TXCFG(txq->reg_idx), NGBE_TXCFG_BUFLEN_MASK,
			NGBE_TXCFG_BUFLEN(txq->nb_tx_desc));
		/* Setup the HW Tx Head and TX Tail descriptor pointers */
		wr32(hw, NGBE_TXRP(txq->reg_idx), 0);
		wr32(hw, NGBE_TXWP(txq->reg_idx), 0);
	}
}

/*
 * Set up link loopback mode Tx->Rx.
 */
static inline void
ngbe_setup_loopback_link(struct ngbe_hw *hw)
{
	PMD_INIT_FUNC_TRACE();

	wr32m(hw, NGBE_MACRXCFG, NGBE_MACRXCFG_LB, NGBE_MACRXCFG_LB);

	msec_delay(50);
}

/*
 * Start Transmit and Receive Units.
 */
int
ngbe_dev_rxtx_start(struct rte_eth_dev *dev)
{
	struct ngbe_hw     *hw;
	struct ngbe_tx_queue *txq;
	struct ngbe_rx_queue *rxq;
	uint32_t dmatxctl;
	uint32_t rxctrl;
	uint16_t i;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	hw = ngbe_dev_hw(dev);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		/* Setup Transmit Threshold Registers */
		wr32m(hw, NGBE_TXCFG(txq->reg_idx),
		      NGBE_TXCFG_HTHRESH_MASK |
		      NGBE_TXCFG_WTHRESH_MASK,
		      NGBE_TXCFG_HTHRESH(txq->hthresh) |
		      NGBE_TXCFG_WTHRESH(txq->wthresh));
	}

	dmatxctl = rd32(hw, NGBE_DMATXCTRL);
	dmatxctl |= NGBE_DMATXCTRL_ENA;
	wr32(hw, NGBE_DMATXCTRL, dmatxctl);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq->tx_deferred_start == 0) {
			ret = ngbe_dev_tx_queue_start(dev, i);
			if (ret < 0)
				return ret;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq->rx_deferred_start == 0) {
			ret = ngbe_dev_rx_queue_start(dev, i);
			if (ret < 0)
				return ret;
		}
	}

	/* Enable Receive engine */
	rxctrl = rd32(hw, NGBE_PBRXCTL);
	rxctrl |= NGBE_PBRXCTL_ENA;
	hw->mac.enable_rx_dma(hw, rxctrl);

	/* If loopback mode is enabled, set up the link accordingly */
	if (hw->is_pf && dev->data->dev_conf.lpbk_mode)
		ngbe_setup_loopback_link(hw);

	return 0;
}

void
ngbe_dev_save_rx_queue(struct ngbe_hw *hw, uint16_t rx_queue_id)
{
	u32 *reg = &hw->q_rx_regs[rx_queue_id * 8];
	*(reg++) = rd32(hw, NGBE_RXBAL(rx_queue_id));
	*(reg++) = rd32(hw, NGBE_RXBAH(rx_queue_id));
	*(reg++) = rd32(hw, NGBE_RXCFG(rx_queue_id));
}

void
ngbe_dev_store_rx_queue(struct ngbe_hw *hw, uint16_t rx_queue_id)
{
	u32 *reg = &hw->q_rx_regs[rx_queue_id * 8];
	wr32(hw, NGBE_RXBAL(rx_queue_id), *(reg++));
	wr32(hw, NGBE_RXBAH(rx_queue_id), *(reg++));
	wr32(hw, NGBE_RXCFG(rx_queue_id), *(reg++) & ~NGBE_RXCFG_ENA);
}

void
ngbe_dev_save_tx_queue(struct ngbe_hw *hw, uint16_t tx_queue_id)
{
	u32 *reg = &hw->q_tx_regs[tx_queue_id * 8];
	*(reg++) = rd32(hw, NGBE_TXBAL(tx_queue_id));
	*(reg++) = rd32(hw, NGBE_TXBAH(tx_queue_id));
	*(reg++) = rd32(hw, NGBE_TXCFG(tx_queue_id));
}

void
ngbe_dev_store_tx_queue(struct ngbe_hw *hw, uint16_t tx_queue_id)
{
	u32 *reg = &hw->q_tx_regs[tx_queue_id * 8];
	wr32(hw, NGBE_TXBAL(tx_queue_id), *(reg++));
	wr32(hw, NGBE_TXBAH(tx_queue_id), *(reg++));
	wr32(hw, NGBE_TXCFG(tx_queue_id), *(reg++) & ~NGBE_TXCFG_ENA);
}

/*
 * Start Receive Units for specified queue.
 */
int
ngbe_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_rx_queue *rxq;
	uint32_t rxdctl;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();

	rxq = dev->data->rx_queues[rx_queue_id];

	/* Allocate buffers for descriptor rings */
	if (ngbe_alloc_rx_queue_mbufs(rxq) != 0) {
		PMD_INIT_LOG(ERR, "Could not alloc mbuf for queue:%d",
			     rx_queue_id);
		return -1;
	}
	rxdctl = rd32(hw, NGBE_RXCFG(rxq->reg_idx));
	rxdctl |= NGBE_RXCFG_ENA;
	wr32(hw, NGBE_RXCFG(rxq->reg_idx), rxdctl);

	/* Wait until Rx Enable ready */
	poll_ms = RTE_NGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_ms(1);
		rxdctl = rd32(hw, NGBE_RXCFG(rxq->reg_idx));
	} while (--poll_ms && !(rxdctl & NGBE_RXCFG_ENA));
	if (poll_ms == 0)
		PMD_INIT_LOG(ERR, "Could not enable Rx Queue %d", rx_queue_id);
	rte_wmb();
	wr32(hw, NGBE_RXRP(rxq->reg_idx), 0);
	wr32(hw, NGBE_RXWP(rxq->reg_idx), rxq->nb_rx_desc - 1);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

/*
 * Stop Receive Units for specified queue.
 */
int
ngbe_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_adapter *adapter = ngbe_dev_adapter(dev);
	struct ngbe_rx_queue *rxq;
	uint32_t rxdctl;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();

	rxq = dev->data->rx_queues[rx_queue_id];

	ngbe_dev_save_rx_queue(hw, rxq->reg_idx);
	wr32m(hw, NGBE_RXCFG(rxq->reg_idx), NGBE_RXCFG_ENA, 0);

	/* Wait until Rx Enable bit clear */
	poll_ms = RTE_NGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_ms(1);
		rxdctl = rd32(hw, NGBE_RXCFG(rxq->reg_idx));
	} while (--poll_ms && (rxdctl & NGBE_RXCFG_ENA));
	if (poll_ms == 0)
		PMD_INIT_LOG(ERR, "Could not disable Rx Queue %d", rx_queue_id);

	rte_delay_us(RTE_NGBE_WAIT_100_US);
	ngbe_dev_store_rx_queue(hw, rxq->reg_idx);

	ngbe_rx_queue_release_mbufs(rxq);
	ngbe_reset_rx_queue(adapter, rxq);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

/*
 * Start Transmit Units for specified queue.
 */
int
ngbe_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_tx_queue *txq;
	uint32_t txdctl;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();

	txq = dev->data->tx_queues[tx_queue_id];
	wr32m(hw, NGBE_TXCFG(txq->reg_idx), NGBE_TXCFG_ENA, NGBE_TXCFG_ENA);

	/* Wait until Tx Enable ready */
	poll_ms = RTE_NGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_ms(1);
		txdctl = rd32(hw, NGBE_TXCFG(txq->reg_idx));
	} while (--poll_ms && !(txdctl & NGBE_TXCFG_ENA));
	if (poll_ms == 0)
		PMD_INIT_LOG(ERR, "Could not enable Tx Queue %d",
			     tx_queue_id);

	rte_wmb();
	wr32(hw, NGBE_TXWP(txq->reg_idx), txq->tx_tail);
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

/*
 * Stop Transmit Units for specified queue.
 */
int
ngbe_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ngbe_hw *hw = ngbe_dev_hw(dev);
	struct ngbe_tx_queue *txq;
	uint32_t txdctl;
	uint32_t txtdh, txtdt;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();

	txq = dev->data->tx_queues[tx_queue_id];

	/* Wait until Tx queue is empty */
	poll_ms = RTE_NGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_us(RTE_NGBE_WAIT_100_US);
		txtdh = rd32(hw, NGBE_TXRP(txq->reg_idx));
		txtdt = rd32(hw, NGBE_TXWP(txq->reg_idx));
	} while (--poll_ms && (txtdh != txtdt));
	if (poll_ms == 0)
		PMD_INIT_LOG(ERR, "Tx Queue %d is not empty when stopping.",
			     tx_queue_id);

	ngbe_dev_save_tx_queue(hw, txq->reg_idx);
	wr32m(hw, NGBE_TXCFG(txq->reg_idx), NGBE_TXCFG_ENA, 0);

	/* Wait until Tx Enable bit clear */
	poll_ms = RTE_NGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_ms(1);
		txdctl = rd32(hw, NGBE_TXCFG(txq->reg_idx));
	} while (--poll_ms && (txdctl & NGBE_TXCFG_ENA));
	if (poll_ms == 0)
		PMD_INIT_LOG(ERR, "Could not disable Tx Queue %d",
			     tx_queue_id);

	rte_delay_us(RTE_NGBE_WAIT_100_US);
	ngbe_dev_store_tx_queue(hw, txq->reg_idx);

	if (txq->ops != NULL) {
		txq->ops->release_mbufs(txq);
		txq->ops->reset(txq);
	}
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

void
ngbe_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct ngbe_rx_queue *rxq;

	rxq = dev->data->rx_queues[queue_id];

	qinfo->mp = rxq->mb_pool;
	qinfo->scattered_rx = dev->data->scattered_rx;
	qinfo->nb_desc = rxq->nb_rx_desc;

	qinfo->conf.rx_free_thresh = rxq->rx_free_thresh;
	qinfo->conf.rx_drop_en = rxq->drop_en;
	qinfo->conf.rx_deferred_start = rxq->rx_deferred_start;
	qinfo->conf.offloads = rxq->offloads;
}

void
ngbe_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct ngbe_tx_queue *txq;

	txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_tx_desc;

	qinfo->conf.tx_thresh.pthresh = txq->pthresh;
	qinfo->conf.tx_thresh.hthresh = txq->hthresh;
	qinfo->conf.tx_thresh.wthresh = txq->wthresh;

	qinfo->conf.tx_free_thresh = txq->tx_free_thresh;
	qinfo->conf.offloads = txq->offloads;
	qinfo->conf.tx_deferred_start = txq->tx_deferred_start;
}
