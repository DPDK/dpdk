/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <inttypes.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_prefetch.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_string_fns.h>
#include <rte_errno.h>

#include "ixgbe_logs.h"
#include "ixgbe/ixgbe_api.h"
#include "ixgbe/ixgbe_vf.h"
#include "ixgbe_ethdev.h"
#include "ixgbe/ixgbe_dcb.h"
#include "ixgbe/ixgbe_common.h"
#include "ixgbe_rxtx.h"

#define IXGBE_RSS_OFFLOAD_ALL ( \
		ETH_RSS_IPV4 | \
		ETH_RSS_IPV4_TCP | \
		ETH_RSS_IPV6 | \
		ETH_RSS_IPV6_EX | \
		ETH_RSS_IPV6_TCP | \
		ETH_RSS_IPV6_TCP_EX | \
		ETH_RSS_IPV4_UDP | \
		ETH_RSS_IPV6_UDP | \
		ETH_RSS_IPV6_UDP_EX)

static inline struct rte_mbuf *
rte_rxmbuf_alloc(struct rte_mempool *mp)
{
	struct rte_mbuf *m;

	m = __rte_mbuf_raw_alloc(mp);
	__rte_mbuf_sanity_check_raw(m, RTE_MBUF_PKT, 0);
	return (m);
}


#if 1
#define RTE_PMD_USE_PREFETCH
#endif

#ifdef RTE_PMD_USE_PREFETCH
/*
 * Prefetch a cache line into all cache levels.
 */
#define rte_ixgbe_prefetch(p)   rte_prefetch0(p)
#else
#define rte_ixgbe_prefetch(p)   do {} while(0)
#endif

/*********************************************************************
 *
 *  TX functions
 *
 **********************************************************************/

/*
 * Check for descriptors with their DD bit set and free mbufs.
 * Return the total number of buffers freed.
 */
static inline int __attribute__((always_inline))
ixgbe_tx_free_bufs(struct igb_tx_queue *txq)
{
	struct igb_tx_entry *txep;
	uint32_t status;
	int i;

	/* check DD bit on threshold descriptor */
	status = txq->tx_ring[txq->tx_next_dd].wb.status;
	if (! (status & IXGBE_ADVTXD_STAT_DD))
		return 0;

	/*
	 * first buffer to free from S/W ring is at index
	 * tx_next_dd - (tx_rs_thresh-1)
	 */
	txep = &(txq->sw_ring[txq->tx_next_dd - (txq->tx_rs_thresh - 1)]);

	/* prefetch the mbufs that are about to be freed */
	for (i = 0; i < txq->tx_rs_thresh; ++i)
		rte_prefetch0((txep + i)->mbuf);

	/* free buffers one at a time */
	if ((txq->txq_flags & (uint32_t)ETH_TXQ_FLAGS_NOREFCOUNT) != 0) {
		for (i = 0; i < txq->tx_rs_thresh; ++i, ++txep) {
			rte_mempool_put(txep->mbuf->pool, txep->mbuf);
			txep->mbuf = NULL;
		}
	} else {
		for (i = 0; i < txq->tx_rs_thresh; ++i, ++txep) {
			rte_pktmbuf_free_seg(txep->mbuf);
			txep->mbuf = NULL;
		}
	}

	/* buffers were freed, update counters */
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + txq->tx_rs_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + txq->tx_rs_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);

	return txq->tx_rs_thresh;
}

/* Populate 4 descriptors with data from 4 mbufs */
static inline void
tx4(volatile union ixgbe_adv_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t buf_dma_addr;
	uint32_t pkt_len;
	int i;

	for (i = 0; i < 4; ++i, ++txdp, ++pkts) {
		buf_dma_addr = RTE_MBUF_DATA_DMA_ADDR(*pkts);
		pkt_len = (*pkts)->pkt.data_len;

		/* write data to descriptor */
		txdp->read.buffer_addr = buf_dma_addr;
		txdp->read.cmd_type_len =
				((uint32_t)DCMD_DTYP_FLAGS | pkt_len);
		txdp->read.olinfo_status =
				(pkt_len << IXGBE_ADVTXD_PAYLEN_SHIFT);
	}
}

/* Populate 1 descriptor with data from 1 mbuf */
static inline void
tx1(volatile union ixgbe_adv_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t buf_dma_addr;
	uint32_t pkt_len;

	buf_dma_addr = RTE_MBUF_DATA_DMA_ADDR(*pkts);
	pkt_len = (*pkts)->pkt.data_len;

	/* write data to descriptor */
	txdp->read.buffer_addr = buf_dma_addr;
	txdp->read.cmd_type_len =
			((uint32_t)DCMD_DTYP_FLAGS | pkt_len);
	txdp->read.olinfo_status =
			(pkt_len << IXGBE_ADVTXD_PAYLEN_SHIFT);
}

/*
 * Fill H/W descriptor ring with mbuf data.
 * Copy mbuf pointers to the S/W ring.
 */
static inline void
ixgbe_tx_fill_hw_ring(struct igb_tx_queue *txq, struct rte_mbuf **pkts,
		      uint16_t nb_pkts)
{
	volatile union ixgbe_adv_tx_desc *txdp = &(txq->tx_ring[txq->tx_tail]);
	struct igb_tx_entry *txep = &(txq->sw_ring[txq->tx_tail]);
	const int N_PER_LOOP = 4;
	const int N_PER_LOOP_MASK = N_PER_LOOP-1;
	int mainpart, leftover;
	int i, j;

	/*
	 * Process most of the packets in chunks of N pkts.  Any
	 * leftover packets will get processed one at a time.
	 */
	mainpart = (nb_pkts & ((uint32_t) ~N_PER_LOOP_MASK));
	leftover = (nb_pkts & ((uint32_t)  N_PER_LOOP_MASK));
	for (i = 0; i < mainpart; i += N_PER_LOOP) {
		/* Copy N mbuf pointers to the S/W ring */
		for (j = 0; j < N_PER_LOOP; ++j) {
			(txep + i + j)->mbuf = *(pkts + i + j);
		}
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
	struct igb_tx_queue *txq = (struct igb_tx_queue *)tx_queue;
	volatile union ixgbe_adv_tx_desc *tx_r = txq->tx_ring;
	uint16_t n = 0;

	/*
	 * Begin scanning the H/W ring for done descriptors when the
	 * number of available descriptors drops below tx_free_thresh.  For
	 * each done descriptor, free the associated buffer.
	 */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		ixgbe_tx_free_bufs(txq);

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
		ixgbe_tx_fill_hw_ring(txq, tx_pkts, n);

		/*
		 * We know that the last descriptor in the ring will need to
		 * have its RS bit set because tx_rs_thresh has to be
		 * a divisor of the ring size
		 */
		tx_r[txq->tx_next_rs].read.cmd_type_len |=
			rte_cpu_to_le_32(IXGBE_ADVTXD_DCMD_RS);
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

		txq->tx_tail = 0;
	}

	/* Fill H/W descriptor ring with mbuf data */
	ixgbe_tx_fill_hw_ring(txq, tx_pkts + n, (uint16_t)(nb_pkts - n));
	txq->tx_tail = (uint16_t)(txq->tx_tail + (nb_pkts - n));

	/*
	 * Determine if RS bit should be set
	 * This is what we actually want:
	 *   if ((txq->tx_tail - 1) >= txq->tx_next_rs)
	 * but instead of subtracting 1 and doing >=, we can just do
	 * greater than without subtracting.
	 */
	if (txq->tx_tail > txq->tx_next_rs) {
		tx_r[txq->tx_next_rs].read.cmd_type_len |=
			rte_cpu_to_le_32(IXGBE_ADVTXD_DCMD_RS);
		txq->tx_next_rs = (uint16_t)(txq->tx_next_rs +
						txq->tx_rs_thresh);
		if (txq->tx_next_rs >= txq->nb_tx_desc)
			txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);
	}

	/*
	 * Check for wrap-around. This would only happen if we used
	 * up to the last descriptor in the ring, no more, no less.
	 */
	if (txq->tx_tail >= txq->nb_tx_desc)
		txq->tx_tail = 0;

	/* update tail pointer */
	rte_wmb();
	IXGBE_PCI_REG_WRITE(txq->tdt_reg_addr, txq->tx_tail);

	return nb_pkts;
}

uint16_t
ixgbe_xmit_pkts_simple(void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts)
{
	uint16_t nb_tx;

	/* Try to transmit at least chunks of TX_MAX_BURST pkts */
	if (likely(nb_pkts <= RTE_PMD_IXGBE_TX_MAX_BURST))
		return tx_xmit_pkts(tx_queue, tx_pkts, nb_pkts);

	/* transmit more than the max burst, in chunks of TX_MAX_BURST */
	nb_tx = 0;
	while (nb_pkts) {
		uint16_t ret, n;
		n = (uint16_t)RTE_MIN(nb_pkts, RTE_PMD_IXGBE_TX_MAX_BURST);
		ret = tx_xmit_pkts(tx_queue, &(tx_pkts[nb_tx]), n);
		nb_tx = (uint16_t)(nb_tx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < n)
			break;
	}

	return nb_tx;
}

static inline void
ixgbe_set_xmit_ctx(struct igb_tx_queue* txq,
		volatile struct ixgbe_adv_tx_context_desc *ctx_txd,
		uint16_t ol_flags, uint32_t vlan_macip_lens)
{
	uint32_t type_tucmd_mlhl;
	uint32_t mss_l4len_idx;
	uint32_t ctx_idx;
	uint32_t cmp_mask;

	ctx_idx = txq->ctx_curr;
	cmp_mask = 0;
	type_tucmd_mlhl = 0;

	if (ol_flags & PKT_TX_VLAN_PKT) {
		cmp_mask |= TX_VLAN_CMP_MASK;
	}

	if (ol_flags & PKT_TX_IP_CKSUM) {
		type_tucmd_mlhl = IXGBE_ADVTXD_TUCMD_IPV4;
		cmp_mask |= TX_MAC_LEN_CMP_MASK;
	}

	/* Specify which HW CTX to upload. */
	mss_l4len_idx = (ctx_idx << IXGBE_ADVTXD_IDX_SHIFT);
	switch (ol_flags & PKT_TX_L4_MASK) {
	case PKT_TX_UDP_CKSUM:
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_UDP |
				IXGBE_ADVTXD_DTYP_CTXT | IXGBE_ADVTXD_DCMD_DEXT;
		mss_l4len_idx |= sizeof(struct udp_hdr) << IXGBE_ADVTXD_L4LEN_SHIFT;
		cmp_mask |= TX_MACIP_LEN_CMP_MASK;
		break;
	case PKT_TX_TCP_CKSUM:
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_TCP |
				IXGBE_ADVTXD_DTYP_CTXT | IXGBE_ADVTXD_DCMD_DEXT;
		mss_l4len_idx |= sizeof(struct tcp_hdr) << IXGBE_ADVTXD_L4LEN_SHIFT;
		cmp_mask |= TX_MACIP_LEN_CMP_MASK;
		break;
	case PKT_TX_SCTP_CKSUM:
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_SCTP |
				IXGBE_ADVTXD_DTYP_CTXT | IXGBE_ADVTXD_DCMD_DEXT;
		mss_l4len_idx |= sizeof(struct sctp_hdr) << IXGBE_ADVTXD_L4LEN_SHIFT;
		cmp_mask |= TX_MACIP_LEN_CMP_MASK;
		break;
	default:
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_RSV |
				IXGBE_ADVTXD_DTYP_CTXT | IXGBE_ADVTXD_DCMD_DEXT;
		break;
	}

	txq->ctx_cache[ctx_idx].flags = ol_flags;
	txq->ctx_cache[ctx_idx].cmp_mask = cmp_mask;
	txq->ctx_cache[ctx_idx].vlan_macip_lens.data =
		vlan_macip_lens & cmp_mask;

	ctx_txd->type_tucmd_mlhl = rte_cpu_to_le_32(type_tucmd_mlhl);
	ctx_txd->vlan_macip_lens = rte_cpu_to_le_32(vlan_macip_lens);
	ctx_txd->mss_l4len_idx   = rte_cpu_to_le_32(mss_l4len_idx);
	ctx_txd->seqnum_seed     = 0;
}

/*
 * Check which hardware context can be used. Use the existing match
 * or create a new context descriptor.
 */
static inline uint32_t
what_advctx_update(struct igb_tx_queue *txq, uint16_t flags,
		uint32_t vlan_macip_lens)
{
	/* If match with the current used context */
	if (likely((txq->ctx_cache[txq->ctx_curr].flags == flags) &&
		(txq->ctx_cache[txq->ctx_curr].vlan_macip_lens.data ==
		(txq->ctx_cache[txq->ctx_curr].cmp_mask & vlan_macip_lens)))) {
			return txq->ctx_curr;
	}

	/* What if match with the next context  */
	txq->ctx_curr ^= 1;
	if (likely((txq->ctx_cache[txq->ctx_curr].flags == flags) &&
		(txq->ctx_cache[txq->ctx_curr].vlan_macip_lens.data ==
		(txq->ctx_cache[txq->ctx_curr].cmp_mask & vlan_macip_lens)))) {
			return txq->ctx_curr;
	}

	/* Mismatch, use the previous context */
	return (IXGBE_CTX_NUM);
}

static inline uint32_t
tx_desc_cksum_flags_to_olinfo(uint16_t ol_flags)
{
	static const uint32_t l4_olinfo[2] = {0, IXGBE_ADVTXD_POPTS_TXSM};
	static const uint32_t l3_olinfo[2] = {0, IXGBE_ADVTXD_POPTS_IXSM};
	uint32_t tmp;

	tmp  = l4_olinfo[(ol_flags & PKT_TX_L4_MASK)  != PKT_TX_L4_NO_CKSUM];
	tmp |= l3_olinfo[(ol_flags & PKT_TX_IP_CKSUM) != 0];
	return tmp;
}

static inline uint32_t
tx_desc_vlan_flags_to_cmdtype(uint16_t ol_flags)
{
	static const uint32_t vlan_cmd[2] = {0, IXGBE_ADVTXD_DCMD_VLE};
	return vlan_cmd[(ol_flags & PKT_TX_VLAN_PKT) != 0];
}

/* Default RS bit threshold values */
#ifndef DEFAULT_TX_RS_THRESH
#define DEFAULT_TX_RS_THRESH   32
#endif
#ifndef DEFAULT_TX_FREE_THRESH
#define DEFAULT_TX_FREE_THRESH 32
#endif

/* Reset transmit descriptors after they have been used */
static inline int
ixgbe_xmit_cleanup(struct igb_tx_queue *txq)
{
	struct igb_tx_entry *sw_ring = txq->sw_ring;
	volatile union ixgbe_adv_tx_desc *txr = txq->tx_ring;
	uint16_t last_desc_cleaned = txq->last_desc_cleaned;
	uint16_t nb_tx_desc = txq->nb_tx_desc;
	uint16_t desc_to_clean_to;
	uint16_t nb_tx_to_clean;

	/* Determine the last descriptor needing to be cleaned */
	desc_to_clean_to = (uint16_t)(last_desc_cleaned + txq->tx_rs_thresh);
	if (desc_to_clean_to >= nb_tx_desc)
		desc_to_clean_to = (uint16_t)(desc_to_clean_to - nb_tx_desc);

	/* Check to make sure the last descriptor to clean is done */
	desc_to_clean_to = sw_ring[desc_to_clean_to].last_id;
	if (! (txr[desc_to_clean_to].wb.status & IXGBE_TXD_STAT_DD))
	{
		PMD_TX_FREE_LOG(DEBUG,
				"TX descriptor %4u is not done"
				"(port=%d queue=%d)",
				desc_to_clean_to,
				txq->port_id, txq->queue_id);
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

	PMD_TX_FREE_LOG(DEBUG,
			"Cleaning %4u TX descriptors: %4u to %4u "
			"(port=%d queue=%d)",
			nb_tx_to_clean, last_desc_cleaned, desc_to_clean_to,
			txq->port_id, txq->queue_id);

	/*
	 * The last descriptor to clean is done, so that means all the
	 * descriptors from the last descriptor that was cleaned
	 * up to the last descriptor with the RS bit set
	 * are done. Only reset the threshold descriptor.
	 */
	txr[desc_to_clean_to].wb.status = 0;

	/* Update the txq to reflect the last descriptor that was cleaned */
	txq->last_desc_cleaned = desc_to_clean_to;
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + nb_tx_to_clean);

	/* No Error */
	return (0);
}

uint16_t
ixgbe_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	struct igb_tx_queue *txq;
	struct igb_tx_entry *sw_ring;
	struct igb_tx_entry *txe, *txn;
	volatile union ixgbe_adv_tx_desc *txr;
	volatile union ixgbe_adv_tx_desc *txd;
	struct rte_mbuf     *tx_pkt;
	struct rte_mbuf     *m_seg;
	uint64_t buf_dma_addr;
	uint32_t olinfo_status;
	uint32_t cmd_type_len;
	uint32_t pkt_len;
	uint16_t slen;
	uint16_t ol_flags;
	uint16_t tx_id;
	uint16_t tx_last;
	uint16_t nb_tx;
	uint16_t nb_used;
	uint16_t tx_ol_req;
	uint32_t vlan_macip_lens;
	uint32_t ctx = 0;
	uint32_t new_ctx;

	txq = tx_queue;
	sw_ring = txq->sw_ring;
	txr     = txq->tx_ring;
	tx_id   = txq->tx_tail;
	txe = &sw_ring[tx_id];

	/* Determine if the descriptor ring needs to be cleaned. */
	if ((txq->nb_tx_desc - txq->nb_tx_free) > txq->tx_free_thresh) {
		ixgbe_xmit_cleanup(txq);
	}

	/* TX loop */
	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		new_ctx = 0;
		tx_pkt = *tx_pkts++;
		pkt_len = tx_pkt->pkt.pkt_len;

		RTE_MBUF_PREFETCH_TO_FREE(txe->mbuf);

		/*
		 * Determine how many (if any) context descriptors
		 * are needed for offload functionality.
		 */
		ol_flags = tx_pkt->ol_flags;
		vlan_macip_lens = tx_pkt->pkt.vlan_macip.data;

		/* If hardware offload required */
		tx_ol_req = (uint16_t)(ol_flags & PKT_TX_OFFLOAD_MASK);
		if (tx_ol_req) {
			/* If new context need be built or reuse the exist ctx. */
			ctx = what_advctx_update(txq, tx_ol_req,
				vlan_macip_lens);
			/* Only allocate context descriptor if required*/
			new_ctx = (ctx == IXGBE_CTX_NUM);
			ctx = txq->ctx_curr;
		}

		/*
		 * Keep track of how many descriptors are used this loop
		 * This will always be the number of segments + the number of
		 * Context descriptors required to transmit the packet
		 */
		nb_used = (uint16_t)(tx_pkt->pkt.nb_segs + new_ctx);

		/*
		 * The number of descriptors that must be allocated for a
		 * packet is the number of segments of that packet, plus 1
		 * Context Descriptor for the hardware offload, if any.
		 * Determine the last TX descriptor to allocate in the TX ring
		 * for the packet, starting from the current position (tx_id)
		 * in the ring.
		 */
		tx_last = (uint16_t) (tx_id + nb_used - 1);

		/* Circular ring */
		if (tx_last >= txq->nb_tx_desc)
			tx_last = (uint16_t) (tx_last - txq->nb_tx_desc);

		PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u pktlen=%u"
			   " tx_first=%u tx_last=%u\n",
			   (unsigned) txq->port_id,
			   (unsigned) txq->queue_id,
			   (unsigned) pkt_len,
			   (unsigned) tx_id,
			   (unsigned) tx_last);

		/*
		 * Make sure there are enough TX descriptors available to
		 * transmit the entire packet.
		 * nb_used better be less than or equal to txq->tx_rs_thresh
		 */
		if (nb_used > txq->nb_tx_free) {
			PMD_TX_FREE_LOG(DEBUG,
					"Not enough free TX descriptors "
					"nb_used=%4u nb_free=%4u "
					"(port=%d queue=%d)",
					nb_used, txq->nb_tx_free,
					txq->port_id, txq->queue_id);

			if (ixgbe_xmit_cleanup(txq) != 0) {
				/* Could not clean any descriptors */
				if (nb_tx == 0)
					return (0);
				goto end_of_tx;
			}

			/* nb_used better be <= txq->tx_rs_thresh */
			if (unlikely(nb_used > txq->tx_rs_thresh)) {
				PMD_TX_FREE_LOG(DEBUG,
					"The number of descriptors needed to "
					"transmit the packet exceeds the "
					"RS bit threshold. This will impact "
					"performance."
					"nb_used=%4u nb_free=%4u "
					"tx_rs_thresh=%4u. "
					"(port=%d queue=%d)",
					nb_used, txq->nb_tx_free,
					txq->tx_rs_thresh,
					txq->port_id, txq->queue_id);
				/*
				 * Loop here until there are enough TX
				 * descriptors or until the ring cannot be
				 * cleaned.
				 */
				while (nb_used > txq->nb_tx_free) {
					if (ixgbe_xmit_cleanup(txq) != 0) {
						/*
						 * Could not clean any
						 * descriptors
						 */
						if (nb_tx == 0)
							return (0);
						goto end_of_tx;
					}
				}
			}
		}

		/*
		 * By now there are enough free TX descriptors to transmit
		 * the packet.
		 */

		/*
		 * Set common flags of all TX Data Descriptors.
		 *
		 * The following bits must be set in all Data Descriptors:
		 *   - IXGBE_ADVTXD_DTYP_DATA
		 *   - IXGBE_ADVTXD_DCMD_DEXT
		 *
		 * The following bits must be set in the first Data Descriptor
		 * and are ignored in the other ones:
		 *   - IXGBE_ADVTXD_DCMD_IFCS
		 *   - IXGBE_ADVTXD_MAC_1588
		 *   - IXGBE_ADVTXD_DCMD_VLE
		 *
		 * The following bits must only be set in the last Data
		 * Descriptor:
		 *   - IXGBE_TXD_CMD_EOP
		 *
		 * The following bits can be set in any Data Descriptor, but
		 * are only set in the last Data Descriptor:
		 *   - IXGBE_TXD_CMD_RS
		 */
		cmd_type_len = IXGBE_ADVTXD_DTYP_DATA |
			IXGBE_ADVTXD_DCMD_IFCS | IXGBE_ADVTXD_DCMD_DEXT;
		olinfo_status = (pkt_len << IXGBE_ADVTXD_PAYLEN_SHIFT);
#ifdef RTE_LIBRTE_IEEE1588
		if (ol_flags & PKT_TX_IEEE1588_TMST)
			cmd_type_len |= IXGBE_ADVTXD_MAC_1588;
#endif

		if (tx_ol_req) {
			/*
			 * Setup the TX Advanced Context Descriptor if required
			 */
			if (new_ctx) {
				volatile struct ixgbe_adv_tx_context_desc *
				    ctx_txd;

				ctx_txd = (volatile struct
				    ixgbe_adv_tx_context_desc *)
				    &txr[tx_id];

				txn = &sw_ring[txe->next_id];
				RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);

				if (txe->mbuf != NULL) {
					rte_pktmbuf_free_seg(txe->mbuf);
					txe->mbuf = NULL;
				}

				ixgbe_set_xmit_ctx(txq, ctx_txd, tx_ol_req,
				    vlan_macip_lens);

				txe->last_id = tx_last;
				tx_id = txe->next_id;
				txe = txn;
			}

			/*
			 * Setup the TX Advanced Data Descriptor,
			 * This path will go through
			 * whatever new/reuse the context descriptor
			 */
			cmd_type_len  |= tx_desc_vlan_flags_to_cmdtype(ol_flags);
			olinfo_status |= tx_desc_cksum_flags_to_olinfo(ol_flags);
			olinfo_status |= ctx << IXGBE_ADVTXD_IDX_SHIFT;
		}

		m_seg = tx_pkt;
		do {
			txd = &txr[tx_id];
			txn = &sw_ring[txe->next_id];

			if (txe->mbuf != NULL)
				rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = m_seg;

			/*
			 * Set up Transmit Data Descriptor.
			 */
			slen = m_seg->pkt.data_len;
			buf_dma_addr = RTE_MBUF_DATA_DMA_ADDR(m_seg);
			txd->read.buffer_addr =
				rte_cpu_to_le_64(buf_dma_addr);
			txd->read.cmd_type_len =
				rte_cpu_to_le_32(cmd_type_len | slen);
			txd->read.olinfo_status =
				rte_cpu_to_le_32(olinfo_status);
			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
			m_seg = m_seg->pkt.next;
		} while (m_seg != NULL);

		/*
		 * The last packet data descriptor needs End Of Packet (EOP)
		 */
		cmd_type_len |= IXGBE_TXD_CMD_EOP;
		txq->nb_tx_used = (uint16_t)(txq->nb_tx_used + nb_used);
		txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_used);

		/* Set RS bit only on threshold packets' last descriptor */
		if (txq->nb_tx_used >= txq->tx_rs_thresh) {
			PMD_TX_FREE_LOG(DEBUG,
					"Setting RS bit on TXD id="
					"%4u (port=%d queue=%d)",
					tx_last, txq->port_id, txq->queue_id);

			cmd_type_len |= IXGBE_TXD_CMD_RS;

			/* Update txq RS bit counters */
			txq->nb_tx_used = 0;
		}
		txd->read.cmd_type_len |= rte_cpu_to_le_32(cmd_type_len);
	}
end_of_tx:
	rte_wmb();

	/*
	 * Set the Transmit Descriptor Tail (TDT)
	 */
	PMD_TX_LOG(DEBUG, "port_id=%u queue_id=%u tx_tail=%u nb_tx=%u",
		   (unsigned) txq->port_id, (unsigned) txq->queue_id,
		   (unsigned) tx_id, (unsigned) nb_tx);
	IXGBE_PCI_REG_WRITE(txq->tdt_reg_addr, tx_id);
	txq->tx_tail = tx_id;

	return (nb_tx);
}

/*********************************************************************
 *
 *  RX functions
 *
 **********************************************************************/
static inline uint16_t
rx_desc_hlen_type_rss_to_pkt_flags(uint32_t hl_tp_rs)
{
	uint16_t pkt_flags;

	static uint16_t ip_pkt_types_map[16] = {
		0, PKT_RX_IPV4_HDR, PKT_RX_IPV4_HDR_EXT, PKT_RX_IPV4_HDR_EXT,
		PKT_RX_IPV6_HDR, 0, 0, 0,
		PKT_RX_IPV6_HDR_EXT, 0, 0, 0,
		PKT_RX_IPV6_HDR_EXT, 0, 0, 0,
	};

	static uint16_t ip_rss_types_map[16] = {
		0, PKT_RX_RSS_HASH, PKT_RX_RSS_HASH, PKT_RX_RSS_HASH,
		0, PKT_RX_RSS_HASH, 0, PKT_RX_RSS_HASH,
		PKT_RX_RSS_HASH, 0, 0, 0,
		0, 0, 0,  PKT_RX_FDIR,
	};

#ifdef RTE_LIBRTE_IEEE1588
	static uint32_t ip_pkt_etqf_map[8] = {
		0, 0, 0, PKT_RX_IEEE1588_PTP,
		0, 0, 0, 0,
	};

	pkt_flags = (uint16_t) ((hl_tp_rs & IXGBE_RXDADV_PKTTYPE_ETQF) ?
				ip_pkt_etqf_map[(hl_tp_rs >> 4) & 0x07] :
				ip_pkt_types_map[(hl_tp_rs >> 4) & 0x0F]);
#else
	pkt_flags = (uint16_t) ((hl_tp_rs & IXGBE_RXDADV_PKTTYPE_ETQF) ? 0 :
				ip_pkt_types_map[(hl_tp_rs >> 4) & 0x0F]);

#endif
	return (uint16_t)(pkt_flags | ip_rss_types_map[hl_tp_rs & 0xF]);
}

static inline uint16_t
rx_desc_status_to_pkt_flags(uint32_t rx_status)
{
	uint16_t pkt_flags;

	/*
	 * Check if VLAN present only.
	 * Do not check whether L3/L4 rx checksum done by NIC or not,
	 * That can be found from rte_eth_rxmode.hw_ip_checksum flag
	 */
	pkt_flags = (uint16_t)((rx_status & IXGBE_RXD_STAT_VP) ?
						PKT_RX_VLAN_PKT : 0);

#ifdef RTE_LIBRTE_IEEE1588
	if (rx_status & IXGBE_RXD_STAT_TMST)
		pkt_flags = (uint16_t)(pkt_flags | PKT_RX_IEEE1588_TMST);
#endif
	return pkt_flags;
}

static inline uint16_t
rx_desc_error_to_pkt_flags(uint32_t rx_status)
{
	/*
	 * Bit 31: IPE, IPv4 checksum error
	 * Bit 30: L4I, L4I integrity error
	 */
	static uint16_t error_to_pkt_flags_map[4] = {
		0,  PKT_RX_L4_CKSUM_BAD, PKT_RX_IP_CKSUM_BAD,
		PKT_RX_IP_CKSUM_BAD | PKT_RX_L4_CKSUM_BAD
	};
	return error_to_pkt_flags_map[(rx_status >>
		IXGBE_RXDADV_ERR_CKSUM_BIT) & IXGBE_RXDADV_ERR_CKSUM_MSK];
}

#ifdef RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC
/*
 * LOOK_AHEAD defines how many desc statuses to check beyond the
 * current descriptor.
 * It must be a pound define for optimal performance.
 * Do not change the value of LOOK_AHEAD, as the ixgbe_rx_scan_hw_ring
 * function only works with LOOK_AHEAD=8.
 */
#define LOOK_AHEAD 8
#if (LOOK_AHEAD != 8)
#error "PMD IXGBE: LOOK_AHEAD must be 8\n"
#endif
static inline int
ixgbe_rx_scan_hw_ring(struct igb_rx_queue *rxq)
{
	volatile union ixgbe_adv_rx_desc *rxdp;
	struct igb_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t pkt_len;
	int s[LOOK_AHEAD], nb_dd;
	int i, j, nb_rx = 0;


	/* get references to current descriptor and S/W ring entry */
	rxdp = &rxq->rx_ring[rxq->rx_tail];
	rxep = &rxq->sw_ring[rxq->rx_tail];

	/* check to make sure there is at least 1 packet to receive */
	if (! (rxdp->wb.upper.status_error & IXGBE_RXDADV_STAT_DD))
		return 0;

	/*
	 * Scan LOOK_AHEAD descriptors at a time to determine which descriptors
	 * reference packets that are ready to be received.
	 */
	for (i = 0; i < RTE_PMD_IXGBE_RX_MAX_BURST;
	     i += LOOK_AHEAD, rxdp += LOOK_AHEAD, rxep += LOOK_AHEAD)
	{
		/* Read desc statuses backwards to avoid race condition */
		for (j = LOOK_AHEAD-1; j >= 0; --j)
			s[j] = rxdp[j].wb.upper.status_error;

		/* Compute how many status bits were set */
		nb_dd = 0;
		for (j = 0; j < LOOK_AHEAD; ++j)
			nb_dd += s[j] & IXGBE_RXDADV_STAT_DD;

		nb_rx += nb_dd;

		/* Translate descriptor info to mbuf format */
		for (j = 0; j < nb_dd; ++j) {
			mb = rxep[j].mbuf;
			pkt_len = (uint16_t)(rxdp[j].wb.upper.length -
							rxq->crc_len);
			mb->pkt.data_len = pkt_len;
			mb->pkt.pkt_len = pkt_len;
			mb->pkt.vlan_macip.f.vlan_tci = rxdp[j].wb.upper.vlan;
			mb->pkt.hash.rss = rxdp[j].wb.lower.hi_dword.rss;

			/* convert descriptor fields to rte mbuf flags */
			mb->ol_flags  = rx_desc_hlen_type_rss_to_pkt_flags(
					rxdp[j].wb.lower.lo_dword.data);
			/* reuse status field from scan list */
			mb->ol_flags = (uint16_t)(mb->ol_flags |
					rx_desc_status_to_pkt_flags(s[j]));
			mb->ol_flags = (uint16_t)(mb->ol_flags |
					rx_desc_error_to_pkt_flags(s[j]));
		}

		/* Move mbuf pointers from the S/W ring to the stage */
		for (j = 0; j < LOOK_AHEAD; ++j) {
			rxq->rx_stage[i + j] = rxep[j].mbuf;
		}

		/* stop if all requested packets could not be received */
		if (nb_dd != LOOK_AHEAD)
			break;
	}

	/* clear software ring entries so we can cleanup correctly */
	for (i = 0; i < nb_rx; ++i) {
		rxq->sw_ring[rxq->rx_tail + i].mbuf = NULL;
	}


	return nb_rx;
}

static inline int
ixgbe_rx_alloc_bufs(struct igb_rx_queue *rxq)
{
	volatile union ixgbe_adv_rx_desc *rxdp;
	struct igb_rx_entry *rxep;
	struct rte_mbuf *mb;
	uint16_t alloc_idx;
	uint64_t dma_addr;
	int diag, i;

	/* allocate buffers in bulk directly into the S/W ring */
	alloc_idx = (uint16_t)(rxq->rx_free_trigger -
				(rxq->rx_free_thresh - 1));
	rxep = &rxq->sw_ring[alloc_idx];
	diag = rte_mempool_get_bulk(rxq->mb_pool, (void *)rxep,
				    rxq->rx_free_thresh);
	if (unlikely(diag != 0))
		return (-ENOMEM);

	rxdp = &rxq->rx_ring[alloc_idx];
	for (i = 0; i < rxq->rx_free_thresh; ++i) {
		/* populate the static rte mbuf fields */
		mb = rxep[i].mbuf;
		rte_mbuf_refcnt_set(mb, 1);
		mb->type = RTE_MBUF_PKT;
		mb->pkt.next = NULL;
		mb->pkt.data = (char *)mb->buf_addr + RTE_PKTMBUF_HEADROOM;
		mb->pkt.nb_segs = 1;
		mb->pkt.in_port = rxq->port_id;

		/* populate the descriptors */
		dma_addr = (uint64_t)mb->buf_physaddr + RTE_PKTMBUF_HEADROOM;
		rxdp[i].read.hdr_addr = dma_addr;
		rxdp[i].read.pkt_addr = dma_addr;
	}

	/* update tail pointer */
	rte_wmb();
	IXGBE_PCI_REG_WRITE(rxq->rdt_reg_addr, rxq->rx_free_trigger);

	/* update state of internal queue structure */
	rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_trigger +
						rxq->rx_free_thresh);
	if (rxq->rx_free_trigger >= rxq->nb_rx_desc)
		rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);

	/* no errors */
	return 0;
}

static inline uint16_t
ixgbe_rx_fill_from_stage(struct igb_rx_queue *rxq, struct rte_mbuf **rx_pkts,
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
rx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	     uint16_t nb_pkts)
{
	struct igb_rx_queue *rxq = (struct igb_rx_queue *)rx_queue;
	uint16_t nb_rx = 0;

	/* Any previously recv'd pkts will be returned from the Rx stage */
	if (rxq->rx_nb_avail)
		return ixgbe_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	/* Scan the H/W ring for packets to receive */
	nb_rx = (uint16_t)ixgbe_rx_scan_hw_ring(rxq);

	/* update internal queue state */
	rxq->rx_next_avail = 0;
	rxq->rx_nb_avail = nb_rx;
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_rx);

	/* if required, allocate new buffers to replenish descriptors */
	if (rxq->rx_tail > rxq->rx_free_trigger) {
		if (ixgbe_rx_alloc_bufs(rxq) != 0) {
			int i, j;
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u\n", (unsigned) rxq->port_id,
				   (unsigned) rxq->queue_id);

			rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed +=
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
	}

	if (rxq->rx_tail >= rxq->nb_rx_desc)
		rxq->rx_tail = 0;

	/* received any packets this loop? */
	if (rxq->rx_nb_avail)
		return ixgbe_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	return 0;
}

/* split requests into chunks of size RTE_PMD_IXGBE_RX_MAX_BURST */
uint16_t
ixgbe_recv_pkts_bulk_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts)
{
	uint16_t nb_rx;

	if (unlikely(nb_pkts == 0))
		return 0;

	if (likely(nb_pkts <= RTE_PMD_IXGBE_RX_MAX_BURST))
		return rx_recv_pkts(rx_queue, rx_pkts, nb_pkts);

	/* request is relatively large, chunk it up */
	nb_rx = 0;
	while (nb_pkts) {
		uint16_t ret, n;
		n = (uint16_t)RTE_MIN(nb_pkts, RTE_PMD_IXGBE_RX_MAX_BURST);
		ret = rx_recv_pkts(rx_queue, &rx_pkts[nb_rx], n);
		nb_rx = (uint16_t)(nb_rx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < n)
			break;
	}

	return nb_rx;
}
#endif /* RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC */

uint16_t
ixgbe_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	struct igb_rx_queue *rxq;
	volatile union ixgbe_adv_rx_desc *rx_ring;
	volatile union ixgbe_adv_rx_desc *rxdp;
	struct igb_rx_entry *sw_ring;
	struct igb_rx_entry *rxe;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	union ixgbe_adv_rx_desc rxd;
	uint64_t dma_addr;
	uint32_t staterr;
	uint32_t hlen_type_rss;
	uint16_t pkt_len;
	uint16_t rx_id;
	uint16_t nb_rx;
	uint16_t nb_hold;
	uint16_t pkt_flags;

	nb_rx = 0;
	nb_hold = 0;
	rxq = rx_queue;
	rx_id = rxq->rx_tail;
	rx_ring = rxq->rx_ring;
	sw_ring = rxq->sw_ring;
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
		staterr = rxdp->wb.upper.status_error;
		if (! (staterr & rte_cpu_to_le_32(IXGBE_RXDADV_STAT_DD)))
			break;
		rxd = *rxdp;

		/*
		 * End of packet.
		 *
		 * If the IXGBE_RXDADV_STAT_EOP flag is not set, the RX packet
		 * is likely to be invalid and to be dropped by the various
		 * validation checks performed by the network stack.
		 *
		 * Allocate a new mbuf to replenish the RX ring descriptor.
		 * If the allocation fails:
		 *    - arrange for that RX descriptor to be the first one
		 *      being parsed the next time the receive function is
		 *      invoked [on the same queue].
		 *
		 *    - Stop parsing the RX ring and return immediately.
		 *
		 * This policy do not drop the packet received in the RX
		 * descriptor for which the allocation of a new mbuf failed.
		 * Thus, it allows that packet to be later retrieved if
		 * mbuf have been freed in the mean time.
		 * As a side effect, holding RX descriptors instead of
		 * systematically giving them back to the NIC may lead to
		 * RX ring exhaustion situations.
		 * However, the NIC can gracefully prevent such situations
		 * to happen by sending specific "back-pressure" flow control
		 * frames to its peer(s).
		 */
		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_id=%u "
			   "ext_err_stat=0x%08x pkt_len=%u\n",
			   (unsigned) rxq->port_id, (unsigned) rxq->queue_id,
			   (unsigned) rx_id, (unsigned) staterr,
			   (unsigned) rte_le_to_cpu_16(rxd.wb.upper.length));

		nmb = rte_rxmbuf_alloc(rxq->mb_pool);
		if (nmb == NULL) {
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u\n", (unsigned) rxq->port_id,
				   (unsigned) rxq->queue_id);
			rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
			break;
		}

		nb_hold++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (rx_id == rxq->nb_rx_desc)
			rx_id = 0;

		/* Prefetch next mbuf while processing current one. */
		rte_ixgbe_prefetch(sw_ring[rx_id].mbuf);

		/*
		 * When next RX descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_ixgbe_prefetch(&rx_ring[rx_id]);
			rte_ixgbe_prefetch(&sw_ring[rx_id]);
		}

		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma_addr =
			rte_cpu_to_le_64(RTE_MBUF_DATA_DMA_ADDR_DEFAULT(nmb));
		rxdp->read.hdr_addr = dma_addr;
		rxdp->read.pkt_addr = dma_addr;

		/*
		 * Initialize the returned mbuf.
		 * 1) setup generic mbuf fields:
		 *    - number of segments,
		 *    - next segment,
		 *    - packet length,
		 *    - RX port identifier.
		 * 2) integrate hardware offload data, if any:
		 *    - RSS flag & hash,
		 *    - IP checksum flag,
		 *    - VLAN TCI, if any,
		 *    - error flags.
		 */
		pkt_len = (uint16_t) (rte_le_to_cpu_16(rxd.wb.upper.length) -
				      rxq->crc_len);
		rxm->pkt.data = (char*) rxm->buf_addr + RTE_PKTMBUF_HEADROOM;
		rte_packet_prefetch(rxm->pkt.data);
		rxm->pkt.nb_segs = 1;
		rxm->pkt.next = NULL;
		rxm->pkt.pkt_len = pkt_len;
		rxm->pkt.data_len = pkt_len;
		rxm->pkt.in_port = rxq->port_id;

		hlen_type_rss = rte_le_to_cpu_32(rxd.wb.lower.lo_dword.data);
		/* Only valid if PKT_RX_VLAN_PKT set in pkt_flags */
		rxm->pkt.vlan_macip.f.vlan_tci =
			rte_le_to_cpu_16(rxd.wb.upper.vlan);

		pkt_flags = rx_desc_hlen_type_rss_to_pkt_flags(hlen_type_rss);
		pkt_flags = (uint16_t)(pkt_flags |
				rx_desc_status_to_pkt_flags(staterr));
		pkt_flags = (uint16_t)(pkt_flags |
				rx_desc_error_to_pkt_flags(staterr));
		rxm->ol_flags = pkt_flags;

		if (likely(pkt_flags & PKT_RX_RSS_HASH))
			rxm->pkt.hash.rss = rxd.wb.lower.hi_dword.rss;
		else if (pkt_flags & PKT_RX_FDIR) {
			rxm->pkt.hash.fdir.hash =
				(uint16_t)((rxd.wb.lower.hi_dword.csum_ip.csum)
					   & IXGBE_ATR_HASH_MASK);
			rxm->pkt.hash.fdir.id = rxd.wb.lower.hi_dword.csum_ip.ip_id;
		}
		/*
		 * Store the mbuf address into the next entry of the array
		 * of returned packets.
		 */
		rx_pkts[nb_rx++] = rxm;
	}
	rxq->rx_tail = rx_id;

	/*
	 * If the number of free RX descriptors is greater than the RX free
	 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
	 * register.
	 * Update the RDT with the value of the last processed RX descriptor
	 * minus 1, to guarantee that the RDT register is never equal to the
	 * RDH register, which creates a "full" ring situtation from the
	 * hardware point of view...
	 */
	nb_hold = (uint16_t) (nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_tail=%u "
			   "nb_hold=%u nb_rx=%u\n",
			   (unsigned) rxq->port_id, (unsigned) rxq->queue_id,
			   (unsigned) rx_id, (unsigned) nb_hold,
			   (unsigned) nb_rx);
		rx_id = (uint16_t) ((rx_id == 0) ?
				     (rxq->nb_rx_desc - 1) : (rx_id - 1));
		IXGBE_PCI_REG_WRITE(rxq->rdt_reg_addr, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;
	return (nb_rx);
}

uint16_t
ixgbe_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			  uint16_t nb_pkts)
{
	struct igb_rx_queue *rxq;
	volatile union ixgbe_adv_rx_desc *rx_ring;
	volatile union ixgbe_adv_rx_desc *rxdp;
	struct igb_rx_entry *sw_ring;
	struct igb_rx_entry *rxe;
	struct rte_mbuf *first_seg;
	struct rte_mbuf *last_seg;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	union ixgbe_adv_rx_desc rxd;
	uint64_t dma; /* Physical address of mbuf data buffer */
	uint32_t staterr;
	uint32_t hlen_type_rss;
	uint16_t rx_id;
	uint16_t nb_rx;
	uint16_t nb_hold;
	uint16_t data_len;
	uint16_t pkt_flags;

	nb_rx = 0;
	nb_hold = 0;
	rxq = rx_queue;
	rx_id = rxq->rx_tail;
	rx_ring = rxq->rx_ring;
	sw_ring = rxq->sw_ring;

	/*
	 * Retrieve RX context of current packet, if any.
	 */
	first_seg = rxq->pkt_first_seg;
	last_seg = rxq->pkt_last_seg;

	while (nb_rx < nb_pkts) {
	next_desc:
		/*
		 * The order of operations here is important as the DD status
		 * bit must not be read after any other descriptor fields.
		 * rx_ring and rxdp are pointing to volatile data so the order
		 * of accesses cannot be reordered by the compiler. If they were
		 * not volatile, they could be reordered which could lead to
		 * using invalid descriptor fields when read from rxd.
		 */
		rxdp = &rx_ring[rx_id];
		staterr = rxdp->wb.upper.status_error;
		if (! (staterr & rte_cpu_to_le_32(IXGBE_RXDADV_STAT_DD)))
			break;
		rxd = *rxdp;

		/*
		 * Descriptor done.
		 *
		 * Allocate a new mbuf to replenish the RX ring descriptor.
		 * If the allocation fails:
		 *    - arrange for that RX descriptor to be the first one
		 *      being parsed the next time the receive function is
		 *      invoked [on the same queue].
		 *
		 *    - Stop parsing the RX ring and return immediately.
		 *
		 * This policy does not drop the packet received in the RX
		 * descriptor for which the allocation of a new mbuf failed.
		 * Thus, it allows that packet to be later retrieved if
		 * mbuf have been freed in the mean time.
		 * As a side effect, holding RX descriptors instead of
		 * systematically giving them back to the NIC may lead to
		 * RX ring exhaustion situations.
		 * However, the NIC can gracefully prevent such situations
		 * to happen by sending specific "back-pressure" flow control
		 * frames to its peer(s).
		 */
		PMD_RX_LOG(DEBUG, "\nport_id=%u queue_id=%u rx_id=%u "
			   "staterr=0x%x data_len=%u\n",
			   (unsigned) rxq->port_id, (unsigned) rxq->queue_id,
			   (unsigned) rx_id, (unsigned) staterr,
			   (unsigned) rte_le_to_cpu_16(rxd.wb.upper.length));

		nmb = rte_rxmbuf_alloc(rxq->mb_pool);
		if (nmb == NULL) {
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u\n", (unsigned) rxq->port_id,
				   (unsigned) rxq->queue_id);
			rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
			break;
		}

		nb_hold++;
		rxe = &sw_ring[rx_id];
		rx_id++;
		if (rx_id == rxq->nb_rx_desc)
			rx_id = 0;

		/* Prefetch next mbuf while processing current one. */
		rte_ixgbe_prefetch(sw_ring[rx_id].mbuf);

		/*
		 * When next RX descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_ixgbe_prefetch(&rx_ring[rx_id]);
			rte_ixgbe_prefetch(&sw_ring[rx_id]);
		}

		/*
		 * Update RX descriptor with the physical address of the new
		 * data buffer of the new allocated mbuf.
		 */
		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma = rte_cpu_to_le_64(RTE_MBUF_DATA_DMA_ADDR_DEFAULT(nmb));
		rxdp->read.hdr_addr = dma;
		rxdp->read.pkt_addr = dma;

		/*
		 * Set data length & data buffer address of mbuf.
		 */
		data_len = rte_le_to_cpu_16(rxd.wb.upper.length);
		rxm->pkt.data_len = data_len;
		rxm->pkt.data = (char*) rxm->buf_addr + RTE_PKTMBUF_HEADROOM;

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
			first_seg->pkt.pkt_len = data_len;
			first_seg->pkt.nb_segs = 1;
		} else {
			first_seg->pkt.pkt_len = (uint16_t)(first_seg->pkt.pkt_len
					+ data_len);
			first_seg->pkt.nb_segs++;
			last_seg->pkt.next = rxm;
		}

		/*
		 * If this is not the last buffer of the received packet,
		 * update the pointer to the last mbuf of the current scattered
		 * packet and continue to parse the RX ring.
		 */
		if (! (staterr & IXGBE_RXDADV_STAT_EOP)) {
			last_seg = rxm;
			goto next_desc;
		}

		/*
		 * This is the last buffer of the received packet.
		 * If the CRC is not stripped by the hardware:
		 *   - Subtract the CRC	length from the total packet length.
		 *   - If the last buffer only contains the whole CRC or a part
		 *     of it, free the mbuf associated to the last buffer.
		 *     If part of the CRC is also contained in the previous
		 *     mbuf, subtract the length of that CRC part from the
		 *     data length of the previous mbuf.
		 */
		rxm->pkt.next = NULL;
		if (unlikely(rxq->crc_len > 0)) {
			first_seg->pkt.pkt_len -= ETHER_CRC_LEN;
			if (data_len <= ETHER_CRC_LEN) {
				rte_pktmbuf_free_seg(rxm);
				first_seg->pkt.nb_segs--;
				last_seg->pkt.data_len = (uint16_t)
					(last_seg->pkt.data_len -
					 (ETHER_CRC_LEN - data_len));
				last_seg->pkt.next = NULL;
			} else
				rxm->pkt.data_len =
					(uint16_t) (data_len - ETHER_CRC_LEN);
		}

		/*
		 * Initialize the first mbuf of the returned packet:
		 *    - RX port identifier,
		 *    - hardware offload data, if any:
		 *      - RSS flag & hash,
		 *      - IP checksum flag,
		 *      - VLAN TCI, if any,
		 *      - error flags.
		 */
		first_seg->pkt.in_port = rxq->port_id;

		/*
		 * The vlan_tci field is only valid when PKT_RX_VLAN_PKT is
		 * set in the pkt_flags field.
		 */
		first_seg->pkt.vlan_macip.f.vlan_tci =
				rte_le_to_cpu_16(rxd.wb.upper.vlan);
		hlen_type_rss = rte_le_to_cpu_32(rxd.wb.lower.lo_dword.data);
		pkt_flags = rx_desc_hlen_type_rss_to_pkt_flags(hlen_type_rss);
		pkt_flags = (uint16_t)(pkt_flags |
				rx_desc_status_to_pkt_flags(staterr));
		pkt_flags = (uint16_t)(pkt_flags |
				rx_desc_error_to_pkt_flags(staterr));
		first_seg->ol_flags = pkt_flags;

		if (likely(pkt_flags & PKT_RX_RSS_HASH))
			first_seg->pkt.hash.rss = rxd.wb.lower.hi_dword.rss;
		else if (pkt_flags & PKT_RX_FDIR) {
			first_seg->pkt.hash.fdir.hash =
				(uint16_t)((rxd.wb.lower.hi_dword.csum_ip.csum)
					   & IXGBE_ATR_HASH_MASK);
			first_seg->pkt.hash.fdir.id =
				rxd.wb.lower.hi_dword.csum_ip.ip_id;
		}

		/* Prefetch data of first segment, if configured to do so. */
		rte_packet_prefetch(first_seg->pkt.data);

		/*
		 * Store the mbuf address into the next entry of the array
		 * of returned packets.
		 */
		rx_pkts[nb_rx++] = first_seg;

		/*
		 * Setup receipt context for a new packet.
		 */
		first_seg = NULL;
	}

	/*
	 * Record index of the next RX descriptor to probe.
	 */
	rxq->rx_tail = rx_id;

	/*
	 * Save receive context.
	 */
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg = last_seg;

	/*
	 * If the number of free RX descriptors is greater than the RX free
	 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
	 * register.
	 * Update the RDT with the value of the last processed RX descriptor
	 * minus 1, to guarantee that the RDT register is never equal to the
	 * RDH register, which creates a "full" ring situtation from the
	 * hardware point of view...
	 */
	nb_hold = (uint16_t) (nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_tail=%u "
			   "nb_hold=%u nb_rx=%u\n",
			   (unsigned) rxq->port_id, (unsigned) rxq->queue_id,
			   (unsigned) rx_id, (unsigned) nb_hold,
			   (unsigned) nb_rx);
		rx_id = (uint16_t) ((rx_id == 0) ?
				     (rxq->nb_rx_desc - 1) : (rx_id - 1));
		IXGBE_PCI_REG_WRITE(rxq->rdt_reg_addr, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;
	return (nb_rx);
}

/*********************************************************************
 *
 *  Queue management functions
 *
 **********************************************************************/

/*
 * Rings setup and release.
 *
 * TDBA/RDBA should be aligned on 16 byte boundary. But TDLEN/RDLEN should be
 * multiple of 128 bytes. So we align TDBA/RDBA on 128 byte boundary. This will
 * also optimize cache line size effect. H/W supports up to cache line size 128.
 */
#define IXGBE_ALIGN 128

/*
 * Maximum number of Ring Descriptors.
 *
 * Since RDLEN/TDLEN should be multiple of 128 bytes, the number of ring
 * descriptors should meet the following condition:
 *      (num_ring_desc * sizeof(rx/tx descriptor)) % 128 == 0
 */
#define IXGBE_MIN_RING_DESC 32
#define IXGBE_MAX_RING_DESC 4096

/*
 * Create memzone for HW rings. malloc can't be used as the physical address is
 * needed. If the memzone is already created, then this function returns a ptr
 * to the old one.
 */
static const struct rte_memzone *
ring_dma_zone_reserve(struct rte_eth_dev *dev, const char *ring_name,
		      uint16_t queue_id, uint32_t ring_size, int socket_id)
{
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;

	snprintf(z_name, sizeof(z_name), "%s_%s_%d_%d",
			dev->driver->pci_drv.name, ring_name,
			dev->data->port_id, queue_id);

	mz = rte_memzone_lookup(z_name);
	if (mz)
		return mz;

#ifdef RTE_LIBRTE_XEN_DOM0
	return rte_memzone_reserve_bounded(z_name, ring_size,
		socket_id, 0, IXGBE_ALIGN, RTE_PGSIZE_2M);
#else
	return rte_memzone_reserve_aligned(z_name, ring_size,
		socket_id, 0, IXGBE_ALIGN);
#endif
}

static void
ixgbe_tx_queue_release_mbufs(struct igb_tx_queue *txq)
{
	unsigned i;

	if (txq->sw_ring != NULL) {
		for (i = 0; i < txq->nb_tx_desc; i++) {
			if (txq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(txq->sw_ring[i].mbuf);
				txq->sw_ring[i].mbuf = NULL;
			}
		}
	}
}

static void
ixgbe_tx_free_swring(struct igb_tx_queue *txq)
{
	if (txq != NULL &&
	    txq->sw_ring != NULL)
		rte_free(txq->sw_ring);
}

static void
ixgbe_tx_queue_release(struct igb_tx_queue *txq)
{
	if (txq != NULL && txq->ops != NULL) {
		txq->ops->release_mbufs(txq);
		txq->ops->free_swring(txq);
		rte_free(txq);
	}
}

void
ixgbe_dev_tx_queue_release(void *txq)
{
	ixgbe_tx_queue_release(txq);
}

/* (Re)set dynamic igb_tx_queue fields to defaults */
static void
ixgbe_reset_tx_queue(struct igb_tx_queue *txq)
{
	static const union ixgbe_adv_tx_desc zeroed_desc = { .read = {
			.buffer_addr = 0}};
	struct igb_tx_entry *txe = txq->sw_ring;
	uint16_t prev, i;

	/* Zero out HW ring memory */
	for (i = 0; i < txq->nb_tx_desc; i++) {
		txq->tx_ring[i] = zeroed_desc;
	}

	/* Initialize SW ring entries */
	prev = (uint16_t) (txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		volatile union ixgbe_adv_tx_desc *txd = &txq->tx_ring[i];
		txd->wb.status = IXGBE_TXD_STAT_DD;
		txe[i].mbuf = NULL;
		txe[i].last_id = i;
		txe[prev].next_id = i;
		prev = i;
	}

	txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);
	txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

	txq->tx_tail = 0;
	txq->nb_tx_used = 0;
	/*
	 * Always allow 1 descriptor to be un-allocated to avoid
	 * a H/W race condition
	 */
	txq->last_desc_cleaned = (uint16_t)(txq->nb_tx_desc - 1);
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_desc - 1);
	txq->ctx_curr = 0;
	memset((void*)&txq->ctx_cache, 0,
		IXGBE_CTX_NUM * sizeof(struct ixgbe_advctx_info));
}

static struct ixgbe_txq_ops def_txq_ops = {
	.release_mbufs = ixgbe_tx_queue_release_mbufs,
	.free_swring = ixgbe_tx_free_swring,
	.reset = ixgbe_reset_tx_queue,
};

int
ixgbe_dev_tx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_txconf *tx_conf)
{
	const struct rte_memzone *tz;
	struct igb_tx_queue *txq;
	struct ixgbe_hw     *hw;
	uint16_t tx_rs_thresh, tx_free_thresh;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * Validate number of transmit descriptors.
	 * It must not exceed hardware maximum, and must be multiple
	 * of IXGBE_ALIGN.
	 */
	if (((nb_desc * sizeof(union ixgbe_adv_tx_desc)) % IXGBE_ALIGN) != 0 ||
	    (nb_desc > IXGBE_MAX_RING_DESC) ||
	    (nb_desc < IXGBE_MIN_RING_DESC)) {
		return -EINVAL;
	}

	/*
	 * The following two parameters control the setting of the RS bit on
	 * transmit descriptors.
	 * TX descriptors will have their RS bit set after txq->tx_rs_thresh
	 * descriptors have been used.
	 * The TX descriptor ring will be cleaned after txq->tx_free_thresh
	 * descriptors are used or if the number of descriptors required
	 * to transmit a packet is greater than the number of free TX
	 * descriptors.
	 * The following constraints must be satisfied:
	 *  tx_rs_thresh must be greater than 0.
	 *  tx_rs_thresh must be less than the size of the ring minus 2.
	 *  tx_rs_thresh must be less than or equal to tx_free_thresh.
	 *  tx_rs_thresh must be a divisor of the ring size.
	 *  tx_free_thresh must be greater than 0.
	 *  tx_free_thresh must be less than the size of the ring minus 3.
	 * One descriptor in the TX ring is used as a sentinel to avoid a
	 * H/W race condition, hence the maximum threshold constraints.
	 * When set to zero use default values.
	 */
	tx_rs_thresh = (uint16_t)((tx_conf->tx_rs_thresh) ?
			tx_conf->tx_rs_thresh : DEFAULT_TX_RS_THRESH);
	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh) ?
			tx_conf->tx_free_thresh : DEFAULT_TX_FREE_THRESH);
	if (tx_rs_thresh >= (nb_desc - 2)) {
		RTE_LOG(ERR, PMD, "tx_rs_thresh must be less than the number "
			"of TX descriptors minus 2. (tx_rs_thresh=%u port=%d "
				"queue=%d)\n", (unsigned int)tx_rs_thresh,
				(int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}
	if (tx_free_thresh >= (nb_desc - 3)) {
		RTE_LOG(ERR, PMD, "tx_rs_thresh must be less than the "
			"tx_free_thresh must be less than the number of TX "
			"descriptors minus 3. (tx_free_thresh=%u port=%d "
				"queue=%d)\n", (unsigned int)tx_free_thresh,
				(int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}
	if (tx_rs_thresh > tx_free_thresh) {
		RTE_LOG(ERR, PMD, "tx_rs_thresh must be less than or equal to "
			"tx_free_thresh. (tx_free_thresh=%u tx_rs_thresh=%u "
			"port=%d queue=%d)\n", (unsigned int)tx_free_thresh,
			(unsigned int)tx_rs_thresh, (int)dev->data->port_id,
							(int)queue_idx);
		return -(EINVAL);
	}
	if ((nb_desc % tx_rs_thresh) != 0) {
		RTE_LOG(ERR, PMD, "tx_rs_thresh must be a divisor of the "
			"number of TX descriptors. (tx_rs_thresh=%u port=%d "
				"queue=%d)\n", (unsigned int)tx_rs_thresh,
				(int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	/*
	 * If rs_bit_thresh is greater than 1, then TX WTHRESH should be
	 * set to 0. If WTHRESH is greater than zero, the RS bit is ignored
	 * by the NIC and all descriptors are written back after the NIC
	 * accumulates WTHRESH descriptors.
	 */
	if ((tx_rs_thresh > 1) && (tx_conf->tx_thresh.wthresh != 0)) {
		RTE_LOG(ERR, PMD, "TX WTHRESH must be set to 0 if "
			"tx_rs_thresh is greater than 1. (tx_rs_thresh=%u "
			"port=%d queue=%d)\n", (unsigned int)tx_rs_thresh,
				(int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	/* Free memory prior to re-allocation if needed... */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		ixgbe_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* First allocate the tx queue data structure */
	txq = rte_zmalloc_socket("ethdev TX queue", sizeof(struct igb_tx_queue),
				 CACHE_LINE_SIZE, socket_id);
	if (txq == NULL)
		return (-ENOMEM);

	/*
	 * Allocate TX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	tz = ring_dma_zone_reserve(dev, "tx_ring", queue_idx,
			sizeof(union ixgbe_adv_tx_desc) * IXGBE_MAX_RING_DESC,
			socket_id);
	if (tz == NULL) {
		ixgbe_tx_queue_release(txq);
		return (-ENOMEM);
	}

	txq->nb_tx_desc = nb_desc;
	txq->tx_rs_thresh = tx_rs_thresh;
	txq->tx_free_thresh = tx_free_thresh;
	txq->pthresh = tx_conf->tx_thresh.pthresh;
	txq->hthresh = tx_conf->tx_thresh.hthresh;
	txq->wthresh = tx_conf->tx_thresh.wthresh;
	txq->queue_id = queue_idx;
	txq->reg_idx = (uint16_t)((RTE_ETH_DEV_SRIOV(dev).active == 0) ?
		queue_idx : RTE_ETH_DEV_SRIOV(dev).def_pool_q_idx + queue_idx);
	txq->port_id = dev->data->port_id;
	txq->txq_flags = tx_conf->txq_flags;
	txq->ops = &def_txq_ops;
	txq->start_tx_per_q = tx_conf->start_tx_per_q;

	/*
	 * Modification to set VFTDT for virtual function if vf is detected
	 */
	if (hw->mac.type == ixgbe_mac_82599_vf)
		txq->tdt_reg_addr = IXGBE_PCI_REG_ADDR(hw, IXGBE_VFTDT(queue_idx));
	else
		txq->tdt_reg_addr = IXGBE_PCI_REG_ADDR(hw, IXGBE_TDT(txq->reg_idx));
#ifndef	RTE_LIBRTE_XEN_DOM0
	txq->tx_ring_phys_addr = (uint64_t) tz->phys_addr;
#else
	txq->tx_ring_phys_addr = rte_mem_phy2mch(tz->memseg_id, tz->phys_addr);
#endif
	txq->tx_ring = (union ixgbe_adv_tx_desc *) tz->addr;

	/* Allocate software ring */
	txq->sw_ring = rte_zmalloc_socket("txq->sw_ring",
				sizeof(struct igb_tx_entry) * nb_desc,
				CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL) {
		ixgbe_tx_queue_release(txq);
		return (-ENOMEM);
	}
	PMD_INIT_LOG(DEBUG, "sw_ring=%p hw_ring=%p dma_addr=0x%"PRIx64"\n",
		     txq->sw_ring, txq->tx_ring, txq->tx_ring_phys_addr);

	/* Use a simple Tx queue (no offloads, no multi segs) if possible */
	if (((txq->txq_flags & IXGBE_SIMPLE_FLAGS) == IXGBE_SIMPLE_FLAGS) &&
	    (txq->tx_rs_thresh >= RTE_PMD_IXGBE_TX_MAX_BURST)) {
		PMD_INIT_LOG(INFO, "Using simple tx code path\n");
#ifdef RTE_IXGBE_INC_VECTOR
		if (txq->tx_rs_thresh <= RTE_IXGBE_TX_MAX_FREE_BUF_SZ &&
		    ixgbe_txq_vec_setup(txq, socket_id) == 0) {
			PMD_INIT_LOG(INFO, "Vector tx enabled.\n");
			dev->tx_pkt_burst = ixgbe_xmit_pkts_vec;
		}
		else
#endif
			dev->tx_pkt_burst = ixgbe_xmit_pkts_simple;
	} else {
		PMD_INIT_LOG(INFO, "Using full-featured tx code path\n");
		PMD_INIT_LOG(INFO, " - txq_flags = %lx [IXGBE_SIMPLE_FLAGS=%lx]\n", (long unsigned)txq->txq_flags, (long unsigned)IXGBE_SIMPLE_FLAGS);
		PMD_INIT_LOG(INFO, " - tx_rs_thresh = %lu [RTE_PMD_IXGBE_TX_MAX_BURST=%lu]\n", (long unsigned)txq->tx_rs_thresh, (long unsigned)RTE_PMD_IXGBE_TX_MAX_BURST);
		dev->tx_pkt_burst = ixgbe_xmit_pkts;
	}

	txq->ops->reset(txq);

	dev->data->tx_queues[queue_idx] = txq;


	return (0);
}

static void
ixgbe_rx_queue_release_mbufs(struct igb_rx_queue *rxq)
{
	unsigned i;

	if (rxq->sw_ring != NULL) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
				rxq->sw_ring[i].mbuf = NULL;
			}
		}
#ifdef RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC
		if (rxq->rx_nb_avail) {
			for (i = 0; i < rxq->rx_nb_avail; ++i) {
				struct rte_mbuf *mb;
				mb = rxq->rx_stage[rxq->rx_next_avail + i];
				rte_pktmbuf_free_seg(mb);
			}
			rxq->rx_nb_avail = 0;
		}
#endif
	}
}

static void
ixgbe_rx_queue_release(struct igb_rx_queue *rxq)
{
	if (rxq != NULL) {
		ixgbe_rx_queue_release_mbufs(rxq);
		rte_free(rxq->sw_ring);
		rte_free(rxq);
	}
}

void
ixgbe_dev_rx_queue_release(void *rxq)
{
	ixgbe_rx_queue_release(rxq);
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
#ifdef RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC
check_rx_burst_bulk_alloc_preconditions(struct igb_rx_queue *rxq)
#else
check_rx_burst_bulk_alloc_preconditions(__rte_unused struct igb_rx_queue *rxq)
#endif
{
	int ret = 0;

	/*
	 * Make sure the following pre-conditions are satisfied:
	 *   rxq->rx_free_thresh >= RTE_PMD_IXGBE_RX_MAX_BURST
	 *   rxq->rx_free_thresh < rxq->nb_rx_desc
	 *   (rxq->nb_rx_desc % rxq->rx_free_thresh) == 0
	 *   rxq->nb_rx_desc<(IXGBE_MAX_RING_DESC-RTE_PMD_IXGBE_RX_MAX_BURST)
	 * Scattered packets are not supported.  This should be checked
	 * outside of this function.
	 */
#ifdef RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC
	if (! (rxq->rx_free_thresh >= RTE_PMD_IXGBE_RX_MAX_BURST))
		ret = -EINVAL;
	else if (! (rxq->rx_free_thresh < rxq->nb_rx_desc))
		ret = -EINVAL;
	else if (! ((rxq->nb_rx_desc % rxq->rx_free_thresh) == 0))
		ret = -EINVAL;
	else if (! (rxq->nb_rx_desc <
	       (IXGBE_MAX_RING_DESC - RTE_PMD_IXGBE_RX_MAX_BURST)))
		ret = -EINVAL;
#else
	ret = -EINVAL;
#endif

	return ret;
}

/* Reset dynamic igb_rx_queue fields back to defaults */
static void
ixgbe_reset_rx_queue(struct igb_rx_queue *rxq)
{
	static const union ixgbe_adv_rx_desc zeroed_desc = { .read = {
			.pkt_addr = 0}};
	unsigned i;
	uint16_t len;

	/*
	 * By default, the Rx queue setup function allocates enough memory for
	 * IXGBE_MAX_RING_DESC.  The Rx Burst bulk allocation function requires
	 * extra memory at the end of the descriptor ring to be zero'd out. A
	 * pre-condition for using the Rx burst bulk alloc function is that the
	 * number of descriptors is less than or equal to
	 * (IXGBE_MAX_RING_DESC - RTE_PMD_IXGBE_RX_MAX_BURST). Check all the
	 * constraints here to see if we need to zero out memory after the end
	 * of the H/W descriptor ring.
	 */
#ifdef RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC
	if (check_rx_burst_bulk_alloc_preconditions(rxq) == 0)
		/* zero out extra memory */
		len = (uint16_t)(rxq->nb_rx_desc + RTE_PMD_IXGBE_RX_MAX_BURST);
	else
#endif
		/* do not zero out extra memory */
		len = rxq->nb_rx_desc;

	/*
	 * Zero out HW ring memory. Zero out extra memory at the end of
	 * the H/W ring so look-ahead logic in Rx Burst bulk alloc function
	 * reads extra memory as zeros.
	 */
	for (i = 0; i < len; i++) {
		rxq->rx_ring[i] = zeroed_desc;
	}

#ifdef RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC
	/*
	 * initialize extra software ring entries. Space for these extra
	 * entries is always allocated
	 */
	memset(&rxq->fake_mbuf, 0x0, sizeof(rxq->fake_mbuf));
	for (i = 0; i < RTE_PMD_IXGBE_RX_MAX_BURST; ++i) {
		rxq->sw_ring[rxq->nb_rx_desc + i].mbuf = &rxq->fake_mbuf;
	}

	rxq->rx_nb_avail = 0;
	rxq->rx_next_avail = 0;
	rxq->rx_free_trigger = (uint16_t)(rxq->rx_free_thresh - 1);
#endif /* RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC */
	rxq->rx_tail = 0;
	rxq->nb_rx_hold = 0;
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;
}

int
ixgbe_dev_rx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	const struct rte_memzone *rz;
	struct igb_rx_queue *rxq;
	struct ixgbe_hw     *hw;
	int use_def_burst_func = 1;
	uint16_t len;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * Validate number of receive descriptors.
	 * It must not exceed hardware maximum, and must be multiple
	 * of IXGBE_ALIGN.
	 */
	if (((nb_desc * sizeof(union ixgbe_adv_rx_desc)) % IXGBE_ALIGN) != 0 ||
	    (nb_desc > IXGBE_MAX_RING_DESC) ||
	    (nb_desc < IXGBE_MIN_RING_DESC)) {
		return (-EINVAL);
	}

	/* Free memory prior to re-allocation if needed... */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		ixgbe_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* First allocate the rx queue data structure */
	rxq = rte_zmalloc_socket("ethdev RX queue", sizeof(struct igb_rx_queue),
				 CACHE_LINE_SIZE, socket_id);
	if (rxq == NULL)
		return (-ENOMEM);
	rxq->mb_pool = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;
	rxq->queue_id = queue_idx;
	rxq->reg_idx = (uint16_t)((RTE_ETH_DEV_SRIOV(dev).active == 0) ?
		queue_idx : RTE_ETH_DEV_SRIOV(dev).def_pool_q_idx + queue_idx);
	rxq->port_id = dev->data->port_id;
	rxq->crc_len = (uint8_t) ((dev->data->dev_conf.rxmode.hw_strip_crc) ?
							0 : ETHER_CRC_LEN);
	rxq->drop_en = rx_conf->rx_drop_en;
	rxq->start_rx_per_q = rx_conf->start_rx_per_q;

	/*
	 * Allocate RX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	rz = ring_dma_zone_reserve(dev, "rx_ring", queue_idx,
				   RX_RING_SZ, socket_id);
	if (rz == NULL) {
		ixgbe_rx_queue_release(rxq);
		return (-ENOMEM);
	}

	/*
	 * Zero init all the descriptors in the ring.
	 */
	memset (rz->addr, 0, RX_RING_SZ);

	/*
	 * Modified to setup VFRDT for Virtual Function
	 */
	if (hw->mac.type == ixgbe_mac_82599_vf) {
		rxq->rdt_reg_addr =
			IXGBE_PCI_REG_ADDR(hw, IXGBE_VFRDT(queue_idx));
		rxq->rdh_reg_addr =
			IXGBE_PCI_REG_ADDR(hw, IXGBE_VFRDH(queue_idx));
	}
	else {
		rxq->rdt_reg_addr =
			IXGBE_PCI_REG_ADDR(hw, IXGBE_RDT(rxq->reg_idx));
		rxq->rdh_reg_addr =
			IXGBE_PCI_REG_ADDR(hw, IXGBE_RDH(rxq->reg_idx));
	}
#ifndef RTE_LIBRTE_XEN_DOM0
	rxq->rx_ring_phys_addr = (uint64_t) rz->phys_addr;
#else
	rxq->rx_ring_phys_addr = rte_mem_phy2mch(rz->memseg_id, rz->phys_addr);
#endif
	rxq->rx_ring = (union ixgbe_adv_rx_desc *) rz->addr;

	/*
	 * Allocate software ring. Allow for space at the end of the
	 * S/W ring to make sure look-ahead logic in bulk alloc Rx burst
	 * function does not access an invalid memory region.
	 */
#ifdef RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC
	len = (uint16_t)(nb_desc + RTE_PMD_IXGBE_RX_MAX_BURST);
#else
	len = nb_desc;
#endif
	rxq->sw_ring = rte_zmalloc_socket("rxq->sw_ring",
					  sizeof(struct igb_rx_entry) * len,
					  CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_ring == NULL) {
		ixgbe_rx_queue_release(rxq);
		return (-ENOMEM);
	}
	PMD_INIT_LOG(DEBUG, "sw_ring=%p hw_ring=%p dma_addr=0x%"PRIx64"\n",
		     rxq->sw_ring, rxq->rx_ring, rxq->rx_ring_phys_addr);

	/*
	 * Certain constraints must be met in order to use the bulk buffer
	 * allocation Rx burst function.
	 */
	use_def_burst_func = check_rx_burst_bulk_alloc_preconditions(rxq);

	/* Check if pre-conditions are satisfied, and no Scattered Rx */
	if (!use_def_burst_func && !dev->data->scattered_rx) {
#ifdef RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
			     "satisfied. Rx Burst Bulk Alloc function will be "
			     "used on port=%d, queue=%d.\n",
			     rxq->port_id, rxq->queue_id);
		dev->rx_pkt_burst = ixgbe_recv_pkts_bulk_alloc;
#ifdef RTE_IXGBE_INC_VECTOR
		if (!ixgbe_rx_vec_condition_check(dev)) {
			PMD_INIT_LOG(INFO, "Vector rx enabled, please make "
				     "sure RX burst size no less than 32.\n");
			ixgbe_rxq_vec_setup(rxq, socket_id);
			dev->rx_pkt_burst = ixgbe_recv_pkts_vec;
		}
#endif
#endif
	} else {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions "
			     "are not satisfied, Scattered Rx is requested, "
			     "or RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC is not "
			     "enabled (port=%d, queue=%d).\n",
			     rxq->port_id, rxq->queue_id);
	}
	dev->data->rx_queues[queue_idx] = rxq;

	ixgbe_reset_rx_queue(rxq);

	return 0;
}

uint32_t
ixgbe_dev_rx_queue_count(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
#define IXGBE_RXQ_SCAN_INTERVAL 4
	volatile union ixgbe_adv_rx_desc *rxdp;
	struct igb_rx_queue *rxq;
	uint32_t desc = 0;

	if (rx_queue_id >= dev->data->nb_rx_queues) {
		PMD_RX_LOG(ERR, "Invalid RX queue id=%d\n", rx_queue_id);
		return 0;
	}

	rxq = dev->data->rx_queues[rx_queue_id];
	rxdp = &(rxq->rx_ring[rxq->rx_tail]);

	while ((desc < rxq->nb_rx_desc) &&
		(rxdp->wb.upper.status_error & IXGBE_RXDADV_STAT_DD)) {
		desc += IXGBE_RXQ_SCAN_INTERVAL;
		rxdp += IXGBE_RXQ_SCAN_INTERVAL;
		if (rxq->rx_tail + desc >= rxq->nb_rx_desc)
			rxdp = &(rxq->rx_ring[rxq->rx_tail +
				desc - rxq->nb_rx_desc]);
	}

	return desc;
}

int
ixgbe_dev_rx_descriptor_done(void *rx_queue, uint16_t offset)
{
	volatile union ixgbe_adv_rx_desc *rxdp;
	struct igb_rx_queue *rxq = rx_queue;
	uint32_t desc;

	if (unlikely(offset >= rxq->nb_rx_desc))
		return 0;
	desc = rxq->rx_tail + offset;
	if (desc >= rxq->nb_rx_desc)
		desc -= rxq->nb_rx_desc;

	rxdp = &rxq->rx_ring[desc];
	return !!(rxdp->wb.upper.status_error & IXGBE_RXDADV_STAT_DD);
}

void
ixgbe_dev_clear_queues(struct rte_eth_dev *dev)
{
	unsigned i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct igb_tx_queue *txq = dev->data->tx_queues[i];
		if (txq != NULL) {
			txq->ops->release_mbufs(txq);
			txq->ops->reset(txq);
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct igb_rx_queue *rxq = dev->data->rx_queues[i];
		if (rxq != NULL) {
			ixgbe_rx_queue_release_mbufs(rxq);
			ixgbe_reset_rx_queue(rxq);
		}
	}
}

/*********************************************************************
 *
 *  Device RX/TX init functions
 *
 **********************************************************************/

/**
 * Receive Side Scaling (RSS)
 * See section 7.1.2.8 in the following document:
 *     "Intel 82599 10 GbE Controller Datasheet" - Revision 2.1 October 2009
 *
 * Principles:
 * The source and destination IP addresses of the IP header and the source
 * and destination ports of TCP/UDP headers, if any, of received packets are
 * hashed against a configurable random key to compute a 32-bit RSS hash result.
 * The seven (7) LSBs of the 32-bit hash result are used as an index into a
 * 128-entry redirection table (RETA).  Each entry of the RETA provides a 3-bit
 * RSS output index which is used as the RX queue index where to store the
 * received packets.
 * The following output is supplied in the RX write-back descriptor:
 *     - 32-bit result of the Microsoft RSS hash function,
 *     - 4-bit RSS type field.
 */

/*
 * RSS random key supplied in section 7.1.2.8.3 of the Intel 82599 datasheet.
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
ixgbe_rss_disable(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw;
	uint32_t mrqc;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	mrqc = IXGBE_READ_REG(hw, IXGBE_MRQC);
	mrqc &= ~IXGBE_MRQC_RSSEN;
	IXGBE_WRITE_REG(hw, IXGBE_MRQC, mrqc);
}

static void
ixgbe_hw_rss_hash_set(struct ixgbe_hw *hw, struct rte_eth_rss_conf *rss_conf)
{
	uint8_t  *hash_key;
	uint32_t mrqc;
	uint32_t rss_key;
	uint64_t rss_hf;
	uint16_t i;

	hash_key = rss_conf->rss_key;
	if (hash_key != NULL) {
		/* Fill in RSS hash key */
		for (i = 0; i < 10; i++) {
			rss_key  = hash_key[(i * 4)];
			rss_key |= hash_key[(i * 4) + 1] << 8;
			rss_key |= hash_key[(i * 4) + 2] << 16;
			rss_key |= hash_key[(i * 4) + 3] << 24;
			IXGBE_WRITE_REG_ARRAY(hw, IXGBE_RSSRK(0), i, rss_key);
		}
	}

	/* Set configured hashing protocols in MRQC register */
	rss_hf = rss_conf->rss_hf;
	mrqc = IXGBE_MRQC_RSSEN; /* Enable RSS */
	if (rss_hf & ETH_RSS_IPV4)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4;
	if (rss_hf & ETH_RSS_IPV4_TCP)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4_TCP;
	if (rss_hf & ETH_RSS_IPV6)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6;
	if (rss_hf & ETH_RSS_IPV6_EX)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_EX;
	if (rss_hf & ETH_RSS_IPV6_TCP)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_TCP;
	if (rss_hf & ETH_RSS_IPV6_TCP_EX)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_EX_TCP;
	if (rss_hf & ETH_RSS_IPV4_UDP)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4_UDP;
	if (rss_hf & ETH_RSS_IPV6_UDP)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_UDP;
	if (rss_hf & ETH_RSS_IPV6_UDP_EX)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_EX_UDP;
	IXGBE_WRITE_REG(hw, IXGBE_MRQC, mrqc);
}

int
ixgbe_dev_rss_hash_update(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct ixgbe_hw *hw;
	uint32_t mrqc;
	uint64_t rss_hf;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * Excerpt from section 7.1.2.8 Receive-Side Scaling (RSS):
	 *     "RSS enabling cannot be done dynamically while it must be
	 *      preceded by a software reset"
	 * Before changing anything, first check that the update RSS operation
	 * does not attempt to disable RSS, if RSS was enabled at
	 * initialization time, or does not attempt to enable RSS, if RSS was
	 * disabled at initialization time.
	 */
	rss_hf = rss_conf->rss_hf & IXGBE_RSS_OFFLOAD_ALL;
	mrqc = IXGBE_READ_REG(hw, IXGBE_MRQC);
	if (!(mrqc & IXGBE_MRQC_RSSEN)) { /* RSS disabled */
		if (rss_hf != 0) /* Enable RSS */
			return -(EINVAL);
		return 0; /* Nothing to do */
	}
	/* RSS enabled */
	if (rss_hf == 0) /* Disable RSS */
		return -(EINVAL);
	ixgbe_hw_rss_hash_set(hw, rss_conf);
	return 0;
}

int
ixgbe_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			    struct rte_eth_rss_conf *rss_conf)
{
	struct ixgbe_hw *hw;
	uint8_t *hash_key;
	uint32_t mrqc;
	uint32_t rss_key;
	uint64_t rss_hf;
	uint16_t i;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	hash_key = rss_conf->rss_key;
	if (hash_key != NULL) {
		/* Return RSS hash key */
		for (i = 0; i < 10; i++) {
			rss_key = IXGBE_READ_REG_ARRAY(hw, IXGBE_RSSRK(0), i);
			hash_key[(i * 4)] = rss_key & 0x000000FF;
			hash_key[(i * 4) + 1] = (rss_key >> 8) & 0x000000FF;
			hash_key[(i * 4) + 2] = (rss_key >> 16) & 0x000000FF;
			hash_key[(i * 4) + 3] = (rss_key >> 24) & 0x000000FF;
		}
	}

	/* Get RSS functions configured in MRQC register */
	mrqc = IXGBE_READ_REG(hw, IXGBE_MRQC);
	if ((mrqc & IXGBE_MRQC_RSSEN) == 0) { /* RSS is disabled */
		rss_conf->rss_hf = 0;
		return 0;
	}
	rss_hf = 0;
	if (mrqc & IXGBE_MRQC_RSS_FIELD_IPV4)
		rss_hf |= ETH_RSS_IPV4;
	if (mrqc & IXGBE_MRQC_RSS_FIELD_IPV4_TCP)
		rss_hf |= ETH_RSS_IPV4_TCP;
	if (mrqc & IXGBE_MRQC_RSS_FIELD_IPV6)
		rss_hf |= ETH_RSS_IPV6;
	if (mrqc & IXGBE_MRQC_RSS_FIELD_IPV6_EX)
		rss_hf |= ETH_RSS_IPV6_EX;
	if (mrqc & IXGBE_MRQC_RSS_FIELD_IPV6_TCP)
		rss_hf |= ETH_RSS_IPV6_TCP;
	if (mrqc & IXGBE_MRQC_RSS_FIELD_IPV6_EX_TCP)
		rss_hf |= ETH_RSS_IPV6_TCP_EX;
	if (mrqc & IXGBE_MRQC_RSS_FIELD_IPV4_UDP)
		rss_hf |= ETH_RSS_IPV4_UDP;
	if (mrqc & IXGBE_MRQC_RSS_FIELD_IPV6_UDP)
		rss_hf |= ETH_RSS_IPV6_UDP;
	if (mrqc & IXGBE_MRQC_RSS_FIELD_IPV6_EX_UDP)
		rss_hf |= ETH_RSS_IPV6_UDP_EX;
	rss_conf->rss_hf = rss_hf;
	return 0;
}

static void
ixgbe_rss_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_rss_conf rss_conf;
	struct ixgbe_hw *hw;
	uint32_t reta;
	uint16_t i;
	uint16_t j;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * Fill in redirection table
	 * The byte-swap is needed because NIC registers are in
	 * little-endian order.
	 */
	reta = 0;
	for (i = 0, j = 0; i < 128; i++, j++) {
		if (j == dev->data->nb_rx_queues)
			j = 0;
		reta = (reta << 8) | j;
		if ((i & 3) == 3)
			IXGBE_WRITE_REG(hw, IXGBE_RETA(i >> 2),
					rte_bswap32(reta));
	}

	/*
	 * Configure the RSS key and the RSS protocols used to compute
	 * the RSS hash of input packets.
	 */
	rss_conf = dev->data->dev_conf.rx_adv_conf.rss_conf;
	if ((rss_conf.rss_hf & IXGBE_RSS_OFFLOAD_ALL) == 0) {
		ixgbe_rss_disable(dev);
		return;
	}
	if (rss_conf.rss_key == NULL)
		rss_conf.rss_key = rss_intel_key; /* Default hash key */
	ixgbe_hw_rss_hash_set(hw, &rss_conf);
}

#define NUM_VFTA_REGISTERS 128
#define NIC_RX_BUFFER_SIZE 0x200

static void
ixgbe_vmdq_dcb_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_vmdq_dcb_conf *cfg;
	struct ixgbe_hw *hw;
	enum rte_eth_nb_pools num_pools;
	uint32_t mrqc, vt_ctl, queue_mapping, vlanctrl;
	uint16_t pbsize;
	uint8_t nb_tcs; /* number of traffic classes */
	int i;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	cfg = &dev->data->dev_conf.rx_adv_conf.vmdq_dcb_conf;
	num_pools = cfg->nb_queue_pools;
	/* Check we have a valid number of pools */
	if (num_pools != ETH_16_POOLS && num_pools != ETH_32_POOLS) {
		ixgbe_rss_disable(dev);
		return;
	}
	/* 16 pools -> 8 traffic classes, 32 pools -> 4 traffic classes */
	nb_tcs = (uint8_t)(ETH_VMDQ_DCB_NUM_QUEUES / (int)num_pools);

	/*
	 * RXPBSIZE
	 * split rx buffer up into sections, each for 1 traffic class
	 */
	pbsize = (uint16_t)(NIC_RX_BUFFER_SIZE / nb_tcs);
	for (i = 0 ; i < nb_tcs; i++) {
		uint32_t rxpbsize = IXGBE_READ_REG(hw, IXGBE_RXPBSIZE(i));
		rxpbsize &= (~(0x3FF << IXGBE_RXPBSIZE_SHIFT));
		/* clear 10 bits. */
		rxpbsize |= (pbsize << IXGBE_RXPBSIZE_SHIFT); /* set value */
		IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(i), rxpbsize);
	}
	/* zero alloc all unused TCs */
	for (i = nb_tcs; i < ETH_DCB_NUM_USER_PRIORITIES; i++) {
		uint32_t rxpbsize = IXGBE_READ_REG(hw, IXGBE_RXPBSIZE(i));
		rxpbsize &= (~( 0x3FF << IXGBE_RXPBSIZE_SHIFT ));
		/* clear 10 bits. */
		IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(i), rxpbsize);
	}

	/* MRQC: enable vmdq and dcb */
	mrqc = ((num_pools == ETH_16_POOLS) ? \
		IXGBE_MRQC_VMDQRT8TCEN : IXGBE_MRQC_VMDQRT4TCEN );
	IXGBE_WRITE_REG(hw, IXGBE_MRQC, mrqc);

	/* PFVTCTL: turn on virtualisation and set the default pool */
	vt_ctl = IXGBE_VT_CTL_VT_ENABLE | IXGBE_VT_CTL_REPLEN;
	if (cfg->enable_default_pool) {
		vt_ctl |= (cfg->default_pool << IXGBE_VT_CTL_POOL_SHIFT);
	} else {
		vt_ctl |= IXGBE_VT_CTL_DIS_DEFPL;
	}

	IXGBE_WRITE_REG(hw, IXGBE_VT_CTL, vt_ctl);

	/* RTRUP2TC: mapping user priorities to traffic classes (TCs) */
	queue_mapping = 0;
	for (i = 0; i < ETH_DCB_NUM_USER_PRIORITIES; i++)
		/*
		 * mapping is done with 3 bits per priority,
		 * so shift by i*3 each time
		 */
		queue_mapping |= ((cfg->dcb_queue[i] & 0x07) << (i * 3));

	IXGBE_WRITE_REG(hw, IXGBE_RTRUP2TC, queue_mapping);

	/* RTRPCS: DCB related */
	IXGBE_WRITE_REG(hw, IXGBE_RTRPCS, IXGBE_RMCS_RRM);

	/* VLNCTRL: enable vlan filtering and allow all vlan tags through */
	vlanctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
	vlanctrl |= IXGBE_VLNCTRL_VFE ; /* enable vlan filters */
	IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlanctrl);

	/* VFTA - enable all vlan filters */
	for (i = 0; i < NUM_VFTA_REGISTERS; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_VFTA(i), 0xFFFFFFFF);
	}

	/* VFRE: pool enabling for receive - 16 or 32 */
	IXGBE_WRITE_REG(hw, IXGBE_VFRE(0), \
			num_pools == ETH_16_POOLS ? 0xFFFF : 0xFFFFFFFF);

	/*
	 * MPSAR - allow pools to read specific mac addresses
	 * In this case, all pools should be able to read from mac addr 0
	 */
	IXGBE_WRITE_REG(hw, IXGBE_MPSAR_LO(0), 0xFFFFFFFF);
	IXGBE_WRITE_REG(hw, IXGBE_MPSAR_HI(0), 0xFFFFFFFF);

	/* PFVLVF, PFVLVFB: set up filters for vlan tags as configured */
	for (i = 0; i < cfg->nb_pool_maps; i++) {
		/* set vlan id in VF register and set the valid bit */
		IXGBE_WRITE_REG(hw, IXGBE_VLVF(i), (IXGBE_VLVF_VIEN | \
				(cfg->pool_map[i].vlan_id & 0xFFF)));
		/*
		 * Put the allowed pools in VFB reg. As we only have 16 or 32
		 * pools, we only need to use the first half of the register
		 * i.e. bits 0-31
		 */
		IXGBE_WRITE_REG(hw, IXGBE_VLVFB(i*2), cfg->pool_map[i].pools);
	}
}

/**
 * ixgbe_dcb_config_tx_hw_config - Configure general DCB TX parameters
 * @hw: pointer to hardware structure
 * @dcb_config: pointer to ixgbe_dcb_config structure
 */
static void
ixgbe_dcb_tx_hw_config(struct ixgbe_hw *hw,
               struct ixgbe_dcb_config *dcb_config)
{
	uint32_t reg;
	uint32_t q;

	PMD_INIT_FUNC_TRACE();
	if (hw->mac.type != ixgbe_mac_82598EB) {
		/* Disable the Tx desc arbiter so that MTQC can be changed */
		reg = IXGBE_READ_REG(hw, IXGBE_RTTDCS);
		reg |= IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, reg);

		/* Enable DCB for Tx with 8 TCs */
		if (dcb_config->num_tcs.pg_tcs == 8) {
			reg = IXGBE_MTQC_RT_ENA | IXGBE_MTQC_8TC_8TQ;
		}
		else {
			reg = IXGBE_MTQC_RT_ENA | IXGBE_MTQC_4TC_4TQ;
		}
		if (dcb_config->vt_mode)
	            reg |= IXGBE_MTQC_VT_ENA;
		IXGBE_WRITE_REG(hw, IXGBE_MTQC, reg);

		/* Disable drop for all queues */
		for (q = 0; q < 128; q++)
			IXGBE_WRITE_REG(hw, IXGBE_QDE,
	             (IXGBE_QDE_WRITE | (q << IXGBE_QDE_IDX_SHIFT)));

		/* Enable the Tx desc arbiter */
		reg = IXGBE_READ_REG(hw, IXGBE_RTTDCS);
		reg &= ~IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, reg);

		/* Enable Security TX Buffer IFG for DCB */
		reg = IXGBE_READ_REG(hw, IXGBE_SECTXMINIFG);
		reg |= IXGBE_SECTX_DCB;
		IXGBE_WRITE_REG(hw, IXGBE_SECTXMINIFG, reg);
	}
	return;
}

/**
 * ixgbe_vmdq_dcb_hw_tx_config - Configure general VMDQ+DCB TX parameters
 * @dev: pointer to rte_eth_dev structure
 * @dcb_config: pointer to ixgbe_dcb_config structure
 */
static void
ixgbe_vmdq_dcb_hw_tx_config(struct rte_eth_dev *dev,
			struct ixgbe_dcb_config *dcb_config)
{
	struct rte_eth_vmdq_dcb_tx_conf *vmdq_tx_conf =
			&dev->data->dev_conf.tx_adv_conf.vmdq_dcb_tx_conf;
	struct ixgbe_hw *hw =
			IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();
	if (hw->mac.type != ixgbe_mac_82598EB)
		/*PF VF Transmit Enable*/
		IXGBE_WRITE_REG(hw, IXGBE_VFTE(0),
			vmdq_tx_conf->nb_queue_pools == ETH_16_POOLS ? 0xFFFF : 0xFFFFFFFF);

	/*Configure general DCB TX parameters*/
	ixgbe_dcb_tx_hw_config(hw,dcb_config);
	return;
}

static void
ixgbe_vmdq_dcb_rx_config(struct rte_eth_dev *dev,
                        struct ixgbe_dcb_config *dcb_config)
{
	struct rte_eth_vmdq_dcb_conf *vmdq_rx_conf =
			&dev->data->dev_conf.rx_adv_conf.vmdq_dcb_conf;
	struct ixgbe_dcb_tc_config *tc;
	uint8_t i,j;

	/* convert rte_eth_conf.rx_adv_conf to struct ixgbe_dcb_config */
	if (vmdq_rx_conf->nb_queue_pools == ETH_16_POOLS ) {
		dcb_config->num_tcs.pg_tcs = ETH_8_TCS;
		dcb_config->num_tcs.pfc_tcs = ETH_8_TCS;
	}
	else {
		dcb_config->num_tcs.pg_tcs = ETH_4_TCS;
		dcb_config->num_tcs.pfc_tcs = ETH_4_TCS;
	}
	/* User Priority to Traffic Class mapping */
	for (i = 0; i < ETH_DCB_NUM_USER_PRIORITIES; i++) {
		j = vmdq_rx_conf->dcb_queue[i];
		tc = &dcb_config->tc_config[j];
		tc->path[IXGBE_DCB_RX_CONFIG].up_to_tc_bitmap =
						(uint8_t)(1 << j);
	}
}

static void
ixgbe_dcb_vt_tx_config(struct rte_eth_dev *dev,
                        struct ixgbe_dcb_config *dcb_config)
{
	struct rte_eth_vmdq_dcb_tx_conf *vmdq_tx_conf =
			&dev->data->dev_conf.tx_adv_conf.vmdq_dcb_tx_conf;
	struct ixgbe_dcb_tc_config *tc;
	uint8_t i,j;

	/* convert rte_eth_conf.rx_adv_conf to struct ixgbe_dcb_config */
	if (vmdq_tx_conf->nb_queue_pools == ETH_16_POOLS ) {
		dcb_config->num_tcs.pg_tcs = ETH_8_TCS;
		dcb_config->num_tcs.pfc_tcs = ETH_8_TCS;
	}
	else {
		dcb_config->num_tcs.pg_tcs = ETH_4_TCS;
		dcb_config->num_tcs.pfc_tcs = ETH_4_TCS;
	}

	/* User Priority to Traffic Class mapping */
	for (i = 0; i < ETH_DCB_NUM_USER_PRIORITIES; i++) {
		j = vmdq_tx_conf->dcb_queue[i];
		tc = &dcb_config->tc_config[j];
		tc->path[IXGBE_DCB_TX_CONFIG].up_to_tc_bitmap =
						(uint8_t)(1 << j);
	}
	return;
}

static void
ixgbe_dcb_rx_config(struct rte_eth_dev *dev,
		struct ixgbe_dcb_config *dcb_config)
{
	struct rte_eth_dcb_rx_conf *rx_conf =
			&dev->data->dev_conf.rx_adv_conf.dcb_rx_conf;
	struct ixgbe_dcb_tc_config *tc;
	uint8_t i,j;

	dcb_config->num_tcs.pg_tcs = (uint8_t)rx_conf->nb_tcs;
	dcb_config->num_tcs.pfc_tcs = (uint8_t)rx_conf->nb_tcs;

	/* User Priority to Traffic Class mapping */
	for (i = 0; i < ETH_DCB_NUM_USER_PRIORITIES; i++) {
		j = rx_conf->dcb_queue[i];
		tc = &dcb_config->tc_config[j];
		tc->path[IXGBE_DCB_RX_CONFIG].up_to_tc_bitmap =
						(uint8_t)(1 << j);
	}
}

static void
ixgbe_dcb_tx_config(struct rte_eth_dev *dev,
		struct ixgbe_dcb_config *dcb_config)
{
	struct rte_eth_dcb_tx_conf *tx_conf =
			&dev->data->dev_conf.tx_adv_conf.dcb_tx_conf;
	struct ixgbe_dcb_tc_config *tc;
	uint8_t i,j;

	dcb_config->num_tcs.pg_tcs = (uint8_t)tx_conf->nb_tcs;
	dcb_config->num_tcs.pfc_tcs = (uint8_t)tx_conf->nb_tcs;

	/* User Priority to Traffic Class mapping */
	for (i = 0; i < ETH_DCB_NUM_USER_PRIORITIES; i++) {
		j = tx_conf->dcb_queue[i];
		tc = &dcb_config->tc_config[j];
		tc->path[IXGBE_DCB_TX_CONFIG].up_to_tc_bitmap =
						(uint8_t)(1 << j);
	}
}

/**
 * ixgbe_dcb_rx_hw_config - Configure general DCB RX HW parameters
 * @hw: pointer to hardware structure
 * @dcb_config: pointer to ixgbe_dcb_config structure
 */
static void
ixgbe_dcb_rx_hw_config(struct ixgbe_hw *hw,
               struct ixgbe_dcb_config *dcb_config)
{
	uint32_t reg;
	uint32_t vlanctrl;
	uint8_t i;

	PMD_INIT_FUNC_TRACE();
	/*
	 * Disable the arbiter before changing parameters
	 * (always enable recycle mode; WSP)
	 */
	reg = IXGBE_RTRPCS_RRM | IXGBE_RTRPCS_RAC | IXGBE_RTRPCS_ARBDIS;
	IXGBE_WRITE_REG(hw, IXGBE_RTRPCS, reg);

	if (hw->mac.type != ixgbe_mac_82598EB) {
		reg = IXGBE_READ_REG(hw, IXGBE_MRQC);
		if (dcb_config->num_tcs.pg_tcs == 4) {
			if (dcb_config->vt_mode)
				reg = (reg & ~IXGBE_MRQC_MRQE_MASK) |
					IXGBE_MRQC_VMDQRT4TCEN;
			else {
				IXGBE_WRITE_REG(hw, IXGBE_VT_CTL, 0);
				reg = (reg & ~IXGBE_MRQC_MRQE_MASK) |
					IXGBE_MRQC_RT4TCEN;
			}
		}
		if (dcb_config->num_tcs.pg_tcs == 8) {
			if (dcb_config->vt_mode)
				reg = (reg & ~IXGBE_MRQC_MRQE_MASK) |
					IXGBE_MRQC_VMDQRT8TCEN;
			else {
				IXGBE_WRITE_REG(hw, IXGBE_VT_CTL, 0);
				reg = (reg & ~IXGBE_MRQC_MRQE_MASK) |
					IXGBE_MRQC_RT8TCEN;
			}
		}

		IXGBE_WRITE_REG(hw, IXGBE_MRQC, reg);
	}

	/* VLNCTRL: enable vlan filtering and allow all vlan tags through */
	vlanctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
	vlanctrl |= IXGBE_VLNCTRL_VFE ; /* enable vlan filters */
	IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlanctrl);

	/* VFTA - enable all vlan filters */
	for (i = 0; i < NUM_VFTA_REGISTERS; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_VFTA(i), 0xFFFFFFFF);
	}

	/*
	 * Configure Rx packet plane (recycle mode; WSP) and
	 * enable arbiter
	 */
	reg = IXGBE_RTRPCS_RRM | IXGBE_RTRPCS_RAC;
	IXGBE_WRITE_REG(hw, IXGBE_RTRPCS, reg);

	return;
}

static void
ixgbe_dcb_hw_arbite_rx_config(struct ixgbe_hw *hw, uint16_t *refill,
			uint16_t *max,uint8_t *bwg_id, uint8_t *tsa, uint8_t *map)
{
	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		ixgbe_dcb_config_rx_arbiter_82598(hw, refill, max, tsa);
		break;
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		ixgbe_dcb_config_rx_arbiter_82599(hw, refill, max, bwg_id,
						  tsa, map);
		break;
	default:
		break;
	}
}

static void
ixgbe_dcb_hw_arbite_tx_config(struct ixgbe_hw *hw, uint16_t *refill, uint16_t *max,
			    uint8_t *bwg_id, uint8_t *tsa, uint8_t *map)
{
	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		ixgbe_dcb_config_tx_desc_arbiter_82598(hw, refill, max, bwg_id,tsa);
		ixgbe_dcb_config_tx_data_arbiter_82598(hw, refill, max, bwg_id,tsa);
		break;
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		ixgbe_dcb_config_tx_desc_arbiter_82599(hw, refill, max, bwg_id,tsa);
		ixgbe_dcb_config_tx_data_arbiter_82599(hw, refill, max, bwg_id,tsa, map);
		break;
	default:
		break;
	}
}

#define DCB_RX_CONFIG  1
#define DCB_TX_CONFIG  1
#define DCB_TX_PB      1024
/**
 * ixgbe_dcb_hw_configure - Enable DCB and configure
 * general DCB in VT mode and non-VT mode parameters
 * @dev: pointer to rte_eth_dev structure
 * @dcb_config: pointer to ixgbe_dcb_config structure
 */
static int
ixgbe_dcb_hw_configure(struct rte_eth_dev *dev,
			struct ixgbe_dcb_config *dcb_config)
{
	int     ret = 0;
	uint8_t i,pfc_en,nb_tcs;
	uint16_t pbsize;
	uint8_t config_dcb_rx = 0;
	uint8_t config_dcb_tx = 0;
	uint8_t tsa[IXGBE_DCB_MAX_TRAFFIC_CLASS] = {0};
	uint8_t bwgid[IXGBE_DCB_MAX_TRAFFIC_CLASS] = {0};
	uint16_t refill[IXGBE_DCB_MAX_TRAFFIC_CLASS] = {0};
	uint16_t max[IXGBE_DCB_MAX_TRAFFIC_CLASS] = {0};
	uint8_t map[IXGBE_DCB_MAX_TRAFFIC_CLASS] = {0};
	struct ixgbe_dcb_tc_config *tc;
	uint32_t max_frame = dev->data->mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
	struct ixgbe_hw *hw =
			IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	switch(dev->data->dev_conf.rxmode.mq_mode){
	case ETH_MQ_RX_VMDQ_DCB:
		dcb_config->vt_mode = true;
		if (hw->mac.type != ixgbe_mac_82598EB) {
			config_dcb_rx = DCB_RX_CONFIG;
			/*
			 *get dcb and VT rx configuration parameters
			 *from rte_eth_conf
			 */
			ixgbe_vmdq_dcb_rx_config(dev,dcb_config);
			/*Configure general VMDQ and DCB RX parameters*/
			ixgbe_vmdq_dcb_configure(dev);
		}
		break;
	case ETH_MQ_RX_DCB:
		dcb_config->vt_mode = false;
		config_dcb_rx = DCB_RX_CONFIG;
		/* Get dcb TX configuration parameters from rte_eth_conf */
		ixgbe_dcb_rx_config(dev,dcb_config);
		/*Configure general DCB RX parameters*/
		ixgbe_dcb_rx_hw_config(hw, dcb_config);
		break;
	default:
		PMD_INIT_LOG(ERR, "Incorrect DCB RX mode configuration\n");
		break;
	}
	switch (dev->data->dev_conf.txmode.mq_mode) {
	case ETH_MQ_TX_VMDQ_DCB:
		dcb_config->vt_mode = true;
		config_dcb_tx = DCB_TX_CONFIG;
		/* get DCB and VT TX configuration parameters from rte_eth_conf */
		ixgbe_dcb_vt_tx_config(dev,dcb_config);
		/*Configure general VMDQ and DCB TX parameters*/
		ixgbe_vmdq_dcb_hw_tx_config(dev,dcb_config);
		break;

	case ETH_MQ_TX_DCB:
		dcb_config->vt_mode = false;
		config_dcb_tx = DCB_TX_CONFIG;
		/*get DCB TX configuration parameters from rte_eth_conf*/
		ixgbe_dcb_tx_config(dev,dcb_config);
		/*Configure general DCB TX parameters*/
		ixgbe_dcb_tx_hw_config(hw, dcb_config);
		break;
	default:
		PMD_INIT_LOG(ERR, "Incorrect DCB TX mode configuration\n");
		break;
	}

	nb_tcs = dcb_config->num_tcs.pfc_tcs;
	/* Unpack map */
	ixgbe_dcb_unpack_map_cee(dcb_config, IXGBE_DCB_RX_CONFIG, map);
	if(nb_tcs == ETH_4_TCS) {
		/* Avoid un-configured priority mapping to TC0 */
		uint8_t j = 4;
		uint8_t mask = 0xFF;
		for (i = 0; i < ETH_DCB_NUM_USER_PRIORITIES - 4; i++)
			mask = (uint8_t)(mask & (~ (1 << map[i])));
		for (i = 0; mask && (i < IXGBE_DCB_MAX_TRAFFIC_CLASS); i++) {
			if ((mask & 0x1) && (j < ETH_DCB_NUM_USER_PRIORITIES))
				map[j++] = i;
			mask >>= 1;
		}
		/* Re-configure 4 TCs BW */
		for (i = 0; i < nb_tcs; i++) {
			tc = &dcb_config->tc_config[i];
			tc->path[IXGBE_DCB_TX_CONFIG].bwg_percent =
						(uint8_t)(100 / nb_tcs);
			tc->path[IXGBE_DCB_RX_CONFIG].bwg_percent =
						(uint8_t)(100 / nb_tcs);
		}
		for (; i < IXGBE_DCB_MAX_TRAFFIC_CLASS; i++) {
			tc = &dcb_config->tc_config[i];
			tc->path[IXGBE_DCB_TX_CONFIG].bwg_percent = 0;
			tc->path[IXGBE_DCB_RX_CONFIG].bwg_percent = 0;
		}
	}

	if(config_dcb_rx) {
		/* Set RX buffer size */
		pbsize = (uint16_t)(NIC_RX_BUFFER_SIZE / nb_tcs);
		uint32_t rxpbsize = pbsize << IXGBE_RXPBSIZE_SHIFT;
		for (i = 0 ; i < nb_tcs; i++) {
			IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(i), rxpbsize);
		}
		/* zero alloc all unused TCs */
		for (; i < ETH_DCB_NUM_USER_PRIORITIES; i++) {
			IXGBE_WRITE_REG(hw, IXGBE_RXPBSIZE(i), 0);
		}
	}
	if(config_dcb_tx) {
		/* Only support an equally distributed Tx packet buffer strategy. */
		uint32_t txpktsize = IXGBE_TXPBSIZE_MAX / nb_tcs;
		uint32_t txpbthresh = (txpktsize / DCB_TX_PB) - IXGBE_TXPKT_SIZE_MAX;
		for (i = 0; i < nb_tcs; i++) {
			IXGBE_WRITE_REG(hw, IXGBE_TXPBSIZE(i), txpktsize);
			IXGBE_WRITE_REG(hw, IXGBE_TXPBTHRESH(i), txpbthresh);
		}
		/* Clear unused TCs, if any, to zero buffer size*/
		for (; i < ETH_DCB_NUM_USER_PRIORITIES; i++) {
			IXGBE_WRITE_REG(hw, IXGBE_TXPBSIZE(i), 0);
			IXGBE_WRITE_REG(hw, IXGBE_TXPBTHRESH(i), 0);
		}
	}

	/*Calculates traffic class credits*/
	ixgbe_dcb_calculate_tc_credits_cee(hw, dcb_config,max_frame,
				IXGBE_DCB_TX_CONFIG);
	ixgbe_dcb_calculate_tc_credits_cee(hw, dcb_config,max_frame,
				IXGBE_DCB_RX_CONFIG);

	if(config_dcb_rx) {
		/* Unpack CEE standard containers */
		ixgbe_dcb_unpack_refill_cee(dcb_config, IXGBE_DCB_RX_CONFIG, refill);
		ixgbe_dcb_unpack_max_cee(dcb_config, max);
		ixgbe_dcb_unpack_bwgid_cee(dcb_config, IXGBE_DCB_RX_CONFIG, bwgid);
		ixgbe_dcb_unpack_tsa_cee(dcb_config, IXGBE_DCB_RX_CONFIG, tsa);
		/* Configure PG(ETS) RX */
		ixgbe_dcb_hw_arbite_rx_config(hw,refill,max,bwgid,tsa,map);
	}

	if(config_dcb_tx) {
		/* Unpack CEE standard containers */
		ixgbe_dcb_unpack_refill_cee(dcb_config, IXGBE_DCB_TX_CONFIG, refill);
		ixgbe_dcb_unpack_max_cee(dcb_config, max);
		ixgbe_dcb_unpack_bwgid_cee(dcb_config, IXGBE_DCB_TX_CONFIG, bwgid);
		ixgbe_dcb_unpack_tsa_cee(dcb_config, IXGBE_DCB_TX_CONFIG, tsa);
		/* Configure PG(ETS) TX */
		ixgbe_dcb_hw_arbite_tx_config(hw,refill,max,bwgid,tsa,map);
	}

	/*Configure queue statistics registers*/
	ixgbe_dcb_config_tc_stats_82599(hw, dcb_config);

	/* Check if the PFC is supported */
	if(dev->data->dev_conf.dcb_capability_en & ETH_DCB_PFC_SUPPORT) {
		pbsize = (uint16_t) (NIC_RX_BUFFER_SIZE / nb_tcs);
		for (i = 0; i < nb_tcs; i++) {
			/*
			* If the TC count is 8,and the default high_water is 48,
			* the low_water is 16 as default.
			*/
			hw->fc.high_water[i] = (pbsize * 3 ) / 4;
			hw->fc.low_water[i] = pbsize / 4;
			/* Enable pfc for this TC */
			tc = &dcb_config->tc_config[i];
			tc->pfc = ixgbe_dcb_pfc_enabled;
		}
		ixgbe_dcb_unpack_pfc_cee(dcb_config, map, &pfc_en);
		if(dcb_config->num_tcs.pfc_tcs == ETH_4_TCS)
			pfc_en &= 0x0F;
		ret = ixgbe_dcb_config_pfc(hw, pfc_en, map);
	}

	return ret;
}

/**
 * ixgbe_configure_dcb - Configure DCB  Hardware
 * @dev: pointer to rte_eth_dev
 */
void ixgbe_configure_dcb(struct rte_eth_dev *dev)
{
	struct ixgbe_dcb_config *dcb_cfg =
			IXGBE_DEV_PRIVATE_TO_DCB_CFG(dev->data->dev_private);
	struct rte_eth_conf *dev_conf = &(dev->data->dev_conf);

	PMD_INIT_FUNC_TRACE();

	/* check support mq_mode for DCB */
	if ((dev_conf->rxmode.mq_mode != ETH_MQ_RX_VMDQ_DCB) &&
	    (dev_conf->rxmode.mq_mode != ETH_MQ_RX_DCB))
		return;

	if (dev->data->nb_rx_queues != ETH_DCB_NUM_QUEUES)
		return;

	/** Configure DCB hardware **/
	ixgbe_dcb_hw_configure(dev,dcb_cfg);

	return;
}

/*
 * VMDq only support for 10 GbE NIC.
 */
static void
ixgbe_vmdq_rx_hw_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_vmdq_rx_conf *cfg;
	struct ixgbe_hw *hw;
	enum rte_eth_nb_pools num_pools;
	uint32_t mrqc, vt_ctl, vlanctrl;
	int i;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	cfg = &dev->data->dev_conf.rx_adv_conf.vmdq_rx_conf;
	num_pools = cfg->nb_queue_pools;

	ixgbe_rss_disable(dev);

	/* MRQC: enable vmdq */
	mrqc = IXGBE_MRQC_VMDQEN;
	IXGBE_WRITE_REG(hw, IXGBE_MRQC, mrqc);

	/* PFVTCTL: turn on virtualisation and set the default pool */
	vt_ctl = IXGBE_VT_CTL_VT_ENABLE | IXGBE_VT_CTL_REPLEN;
	if (cfg->enable_default_pool)
		vt_ctl |= (cfg->default_pool << IXGBE_VT_CTL_POOL_SHIFT);
	else
		vt_ctl |= IXGBE_VT_CTL_DIS_DEFPL;

	IXGBE_WRITE_REG(hw, IXGBE_VT_CTL, vt_ctl);

	/* VLNCTRL: enable vlan filtering and allow all vlan tags through */
	vlanctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
	vlanctrl |= IXGBE_VLNCTRL_VFE ; /* enable vlan filters */
	IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, vlanctrl);

	/* VFTA - enable all vlan filters */
	for (i = 0; i < NUM_VFTA_REGISTERS; i++)
		IXGBE_WRITE_REG(hw, IXGBE_VFTA(i), UINT32_MAX);

	/* VFRE: pool enabling for receive - 64 */
	IXGBE_WRITE_REG(hw, IXGBE_VFRE(0), UINT32_MAX);
	if (num_pools == ETH_64_POOLS)
		IXGBE_WRITE_REG(hw, IXGBE_VFRE(1), UINT32_MAX);

	/*
	 * MPSAR - allow pools to read specific mac addresses
	 * In this case, all pools should be able to read from mac addr 0
	 */
	IXGBE_WRITE_REG(hw, IXGBE_MPSAR_LO(0), UINT32_MAX);
	IXGBE_WRITE_REG(hw, IXGBE_MPSAR_HI(0), UINT32_MAX);

	/* PFVLVF, PFVLVFB: set up filters for vlan tags as configured */
	for (i = 0; i < cfg->nb_pool_maps; i++) {
		/* set vlan id in VF register and set the valid bit */
		IXGBE_WRITE_REG(hw, IXGBE_VLVF(i), (IXGBE_VLVF_VIEN | \
				(cfg->pool_map[i].vlan_id & IXGBE_RXD_VLAN_ID_MASK)));
		/*
		 * Put the allowed pools in VFB reg. As we only have 16 or 64
		 * pools, we only need to use the first half of the register
		 * i.e. bits 0-31
		 */
		if (((cfg->pool_map[i].pools >> 32) & UINT32_MAX) == 0)
			IXGBE_WRITE_REG(hw, IXGBE_VLVFB(i*2), \
					(cfg->pool_map[i].pools & UINT32_MAX));
		else
			IXGBE_WRITE_REG(hw, IXGBE_VLVFB((i*2+1)), \
					((cfg->pool_map[i].pools >> 32) \
					& UINT32_MAX));

	}

	/* PFDMA Tx General Switch Control Enables VMDQ loopback */
	if (cfg->enable_loop_back) {
		IXGBE_WRITE_REG(hw, IXGBE_PFDTXGSWC, IXGBE_PFDTXGSWC_VT_LBEN);
		for (i = 0; i < RTE_IXGBE_VMTXSW_REGISTER_COUNT; i++)
			IXGBE_WRITE_REG(hw, IXGBE_VMTXSW(i), UINT32_MAX);
	}

	IXGBE_WRITE_FLUSH(hw);
}

/*
 * ixgbe_dcb_config_tx_hw_config - Configure general VMDq TX parameters
 * @hw: pointer to hardware structure
 */
static void
ixgbe_vmdq_tx_hw_configure(struct ixgbe_hw *hw)
{
	uint32_t reg;
	uint32_t q;

	PMD_INIT_FUNC_TRACE();
	/*PF VF Transmit Enable*/
	IXGBE_WRITE_REG(hw, IXGBE_VFTE(0), UINT32_MAX);
	IXGBE_WRITE_REG(hw, IXGBE_VFTE(1), UINT32_MAX);

	/* Disable the Tx desc arbiter so that MTQC can be changed */
	reg = IXGBE_READ_REG(hw, IXGBE_RTTDCS);
	reg |= IXGBE_RTTDCS_ARBDIS;
	IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, reg);

	reg = IXGBE_MTQC_VT_ENA | IXGBE_MTQC_64VF;
	IXGBE_WRITE_REG(hw, IXGBE_MTQC, reg);

	/* Disable drop for all queues */
	for (q = 0; q < IXGBE_MAX_RX_QUEUE_NUM; q++)
		IXGBE_WRITE_REG(hw, IXGBE_QDE,
	          (IXGBE_QDE_WRITE | (q << IXGBE_QDE_IDX_SHIFT)));

	/* Enable the Tx desc arbiter */
	reg = IXGBE_READ_REG(hw, IXGBE_RTTDCS);
	reg &= ~IXGBE_RTTDCS_ARBDIS;
	IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, reg);

	IXGBE_WRITE_FLUSH(hw);

	return;
}

static int
ixgbe_alloc_rx_queue_mbufs(struct igb_rx_queue *rxq)
{
	struct igb_rx_entry *rxe = rxq->sw_ring;
	uint64_t dma_addr;
	unsigned i;

	/* Initialize software ring entries */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		volatile union ixgbe_adv_rx_desc *rxd;
		struct rte_mbuf *mbuf = rte_rxmbuf_alloc(rxq->mb_pool);
		if (mbuf == NULL) {
			PMD_INIT_LOG(ERR, "RX mbuf alloc failed queue_id=%u\n",
				     (unsigned) rxq->queue_id);
			return (-ENOMEM);
		}

		rte_mbuf_refcnt_set(mbuf, 1);
		mbuf->type = RTE_MBUF_PKT;
		mbuf->pkt.next = NULL;
		mbuf->pkt.data = (char *)mbuf->buf_addr + RTE_PKTMBUF_HEADROOM;
		mbuf->pkt.nb_segs = 1;
		mbuf->pkt.in_port = rxq->port_id;

		dma_addr =
			rte_cpu_to_le_64(RTE_MBUF_DATA_DMA_ADDR_DEFAULT(mbuf));
		rxd = &rxq->rx_ring[i];
		rxd->read.hdr_addr = dma_addr;
		rxd->read.pkt_addr = dma_addr;
		rxe[i].mbuf = mbuf;
	}

	return 0;
}

static int
ixgbe_dev_mq_rx_configure(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (hw->mac.type == ixgbe_mac_82598EB)
		return 0;

	if (RTE_ETH_DEV_SRIOV(dev).active == 0) {
		/*
		 * SRIOV inactive scheme
		 * any DCB/RSS w/o VMDq multi-queue setting
		 */
		switch (dev->data->dev_conf.rxmode.mq_mode) {
			case ETH_MQ_RX_RSS:
				ixgbe_rss_configure(dev);
				break;

			case ETH_MQ_RX_VMDQ_DCB:
				ixgbe_vmdq_dcb_configure(dev);
				break;

			case ETH_MQ_RX_VMDQ_ONLY:
				ixgbe_vmdq_rx_hw_configure(dev);
				break;

			case ETH_MQ_RX_NONE:
				/* if mq_mode is none, disable rss mode.*/
			default: ixgbe_rss_disable(dev);
		}
	} else {
		switch (RTE_ETH_DEV_SRIOV(dev).active) {
		/*
		 * SRIOV active scheme
		 * FIXME if support DCB/RSS together with VMDq & SRIOV
		 */
		case ETH_64_POOLS:
			IXGBE_WRITE_REG(hw, IXGBE_MRQC, IXGBE_MRQC_VMDQEN);
			break;

		case ETH_32_POOLS:
			IXGBE_WRITE_REG(hw, IXGBE_MRQC, IXGBE_MRQC_VMDQRT4TCEN);
			break;

		case ETH_16_POOLS:
			IXGBE_WRITE_REG(hw, IXGBE_MRQC, IXGBE_MRQC_VMDQRT8TCEN);
			break;
		default:
			RTE_LOG(ERR, PMD, "invalid pool number in IOV mode\n");
		}
	}

	return 0;
}

static int
ixgbe_dev_mq_tx_configure(struct rte_eth_dev *dev)
{
	struct ixgbe_hw *hw =
		IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t mtqc;
	uint32_t rttdcs;

	if (hw->mac.type == ixgbe_mac_82598EB)
		return 0;

	/* disable arbiter before setting MTQC */
	rttdcs = IXGBE_READ_REG(hw, IXGBE_RTTDCS);
	rttdcs |= IXGBE_RTTDCS_ARBDIS;
	IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);

	if (RTE_ETH_DEV_SRIOV(dev).active == 0) {
		/*
		 * SRIOV inactive scheme
		 * any DCB w/o VMDq multi-queue setting
		 */
		if (dev->data->dev_conf.txmode.mq_mode == ETH_MQ_TX_VMDQ_ONLY)
			ixgbe_vmdq_tx_hw_configure(hw);
		else {
			mtqc = IXGBE_MTQC_64Q_1PB;
			IXGBE_WRITE_REG(hw, IXGBE_MTQC, mtqc);
		}
	} else {
		switch (RTE_ETH_DEV_SRIOV(dev).active) {

		/*
		 * SRIOV active scheme
		 * FIXME if support DCB together with VMDq & SRIOV
		 */
		case ETH_64_POOLS:
			mtqc = IXGBE_MTQC_VT_ENA | IXGBE_MTQC_64VF;
			break;
		case ETH_32_POOLS:
			mtqc = IXGBE_MTQC_VT_ENA | IXGBE_MTQC_32VF;
			break;
		case ETH_16_POOLS:
			mtqc = IXGBE_MTQC_VT_ENA | IXGBE_MTQC_RT_ENA |
				IXGBE_MTQC_8TC_8TQ;
			break;
		default:
			mtqc = IXGBE_MTQC_64Q_1PB;
			RTE_LOG(ERR, PMD, "invalid pool number in IOV mode\n");
		}
		IXGBE_WRITE_REG(hw, IXGBE_MTQC, mtqc);
	}

	/* re-enable arbiter */
	rttdcs &= ~IXGBE_RTTDCS_ARBDIS;
	IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);

	return 0;
}

/*
 * Initializes Receive Unit.
 */
int
ixgbe_dev_rx_init(struct rte_eth_dev *dev)
{
	struct ixgbe_hw     *hw;
	struct igb_rx_queue *rxq;
	struct rte_pktmbuf_pool_private *mbp_priv;
	uint64_t bus_addr;
	uint32_t rxctrl;
	uint32_t fctrl;
	uint32_t hlreg0;
	uint32_t maxfrs;
	uint32_t srrctl;
	uint32_t rdrxctl;
	uint32_t rxcsum;
	uint16_t buf_size;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * Make sure receives are disabled while setting
	 * up the RX context (registers, descriptor rings, etc.).
	 */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, rxctrl & ~IXGBE_RXCTRL_RXEN);

	/* Enable receipt of broadcasted frames */
	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	fctrl |= IXGBE_FCTRL_BAM;
	fctrl |= IXGBE_FCTRL_DPF;
	fctrl |= IXGBE_FCTRL_PMCF;
	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);

	/*
	 * Configure CRC stripping, if any.
	 */
	hlreg0 = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	if (dev->data->dev_conf.rxmode.hw_strip_crc)
		hlreg0 |= IXGBE_HLREG0_RXCRCSTRP;
	else
		hlreg0 &= ~IXGBE_HLREG0_RXCRCSTRP;

	/*
	 * Configure jumbo frame support, if any.
	 */
	if (dev->data->dev_conf.rxmode.jumbo_frame == 1) {
		hlreg0 |= IXGBE_HLREG0_JUMBOEN;
		maxfrs = IXGBE_READ_REG(hw, IXGBE_MAXFRS);
		maxfrs &= 0x0000FFFF;
		maxfrs |= (dev->data->dev_conf.rxmode.max_rx_pkt_len << 16);
		IXGBE_WRITE_REG(hw, IXGBE_MAXFRS, maxfrs);
	} else
		hlreg0 &= ~IXGBE_HLREG0_JUMBOEN;

	/*
	 * If loopback mode is configured for 82599, set LPBK bit.
	 */
	if (hw->mac.type == ixgbe_mac_82599EB &&
			dev->data->dev_conf.lpbk_mode == IXGBE_LPBK_82599_TX_RX)
		hlreg0 |= IXGBE_HLREG0_LPBK;
	else
		hlreg0 &= ~IXGBE_HLREG0_LPBK;

	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg0);

	/* Setup RX queues */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		/*
		 * Reset crc_len in case it was changed after queue setup by a
		 * call to configure.
		 */
		rxq->crc_len = (uint8_t)
				((dev->data->dev_conf.rxmode.hw_strip_crc) ? 0 :
				ETHER_CRC_LEN);

		/* Setup the Base and Length of the Rx Descriptor Rings */
		bus_addr = rxq->rx_ring_phys_addr;
		IXGBE_WRITE_REG(hw, IXGBE_RDBAL(rxq->reg_idx),
				(uint32_t)(bus_addr & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_RDBAH(rxq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_RDLEN(rxq->reg_idx),
				rxq->nb_rx_desc * sizeof(union ixgbe_adv_rx_desc));
		IXGBE_WRITE_REG(hw, IXGBE_RDH(rxq->reg_idx), 0);
		IXGBE_WRITE_REG(hw, IXGBE_RDT(rxq->reg_idx), 0);

		/* Configure the SRRCTL register */
#ifdef RTE_HEADER_SPLIT_ENABLE
		/*
		 * Configure Header Split
		 */
		if (dev->data->dev_conf.rxmode.header_split) {
			if (hw->mac.type == ixgbe_mac_82599EB) {
				/* Must setup the PSRTYPE register */
				uint32_t psrtype;
				psrtype = IXGBE_PSRTYPE_TCPHDR |
					IXGBE_PSRTYPE_UDPHDR   |
					IXGBE_PSRTYPE_IPV4HDR  |
					IXGBE_PSRTYPE_IPV6HDR;
				IXGBE_WRITE_REG(hw, IXGBE_PSRTYPE(rxq->reg_idx), psrtype);
			}
			srrctl = ((dev->data->dev_conf.rxmode.split_hdr_size <<
				   IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT) &
				  IXGBE_SRRCTL_BSIZEHDR_MASK);
			srrctl |= E1000_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS;
		} else
#endif
			srrctl = IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;

		/* Set if packets are dropped when no descriptors available */
		if (rxq->drop_en)
			srrctl |= IXGBE_SRRCTL_DROP_EN;

		/*
		 * Configure the RX buffer size in the BSIZEPACKET field of
		 * the SRRCTL register of the queue.
		 * The value is in 1 KB resolution. Valid values can be from
		 * 1 KB to 16 KB.
		 */
		mbp_priv = rte_mempool_get_priv(rxq->mb_pool);
		buf_size = (uint16_t) (mbp_priv->mbuf_data_room_size -
				       RTE_PKTMBUF_HEADROOM);
		srrctl |= ((buf_size >> IXGBE_SRRCTL_BSIZEPKT_SHIFT) &
			   IXGBE_SRRCTL_BSIZEPKT_MASK);
		IXGBE_WRITE_REG(hw, IXGBE_SRRCTL(rxq->reg_idx), srrctl);

		buf_size = (uint16_t) ((srrctl & IXGBE_SRRCTL_BSIZEPKT_MASK) <<
				       IXGBE_SRRCTL_BSIZEPKT_SHIFT);

		/* It adds dual VLAN length for supporting dual VLAN */
		if ((dev->data->dev_conf.rxmode.max_rx_pkt_len +
				2 * IXGBE_VLAN_TAG_SIZE) > buf_size){
			dev->data->scattered_rx = 1;
			dev->rx_pkt_burst = ixgbe_recv_scattered_pkts;
		}
	}

	if (dev->data->dev_conf.rxmode.enable_scatter) {
		dev->rx_pkt_burst = ixgbe_recv_scattered_pkts;
		dev->data->scattered_rx = 1;
	}

	/*
	 * Device configured with multiple RX queues.
	 */
	ixgbe_dev_mq_rx_configure(dev);

	/*
	 * Setup the Checksum Register.
	 * Disable Full-Packet Checksum which is mutually exclusive with RSS.
	 * Enable IP/L4 checkum computation by hardware if requested to do so.
	 */
	rxcsum = IXGBE_READ_REG(hw, IXGBE_RXCSUM);
	rxcsum |= IXGBE_RXCSUM_PCSD;
	if (dev->data->dev_conf.rxmode.hw_ip_checksum)
		rxcsum |= IXGBE_RXCSUM_IPPCSE;
	else
		rxcsum &= ~IXGBE_RXCSUM_IPPCSE;

	IXGBE_WRITE_REG(hw, IXGBE_RXCSUM, rxcsum);

	if (hw->mac.type == ixgbe_mac_82599EB) {
		rdrxctl = IXGBE_READ_REG(hw, IXGBE_RDRXCTL);
		if (dev->data->dev_conf.rxmode.hw_strip_crc)
			rdrxctl |= IXGBE_RDRXCTL_CRCSTRIP;
		else
			rdrxctl &= ~IXGBE_RDRXCTL_CRCSTRIP;
		rdrxctl &= ~IXGBE_RDRXCTL_RSCFRSTSIZE;
		IXGBE_WRITE_REG(hw, IXGBE_RDRXCTL, rdrxctl);
	}

	return 0;
}

/*
 * Initializes Transmit Unit.
 */
void
ixgbe_dev_tx_init(struct rte_eth_dev *dev)
{
	struct ixgbe_hw     *hw;
	struct igb_tx_queue *txq;
	uint64_t bus_addr;
	uint32_t hlreg0;
	uint32_t txctrl;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Enable TX CRC (checksum offload requirement) */
	hlreg0 = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	hlreg0 |= IXGBE_HLREG0_TXCRCEN;
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg0);

	/* Setup the Base and Length of the Tx Descriptor Rings */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];

		bus_addr = txq->tx_ring_phys_addr;
		IXGBE_WRITE_REG(hw, IXGBE_TDBAL(txq->reg_idx),
				(uint32_t)(bus_addr & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_TDBAH(txq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_TDLEN(txq->reg_idx),
				txq->nb_tx_desc * sizeof(union ixgbe_adv_tx_desc));
		/* Setup the HW Tx Head and TX Tail descriptor pointers */
		IXGBE_WRITE_REG(hw, IXGBE_TDH(txq->reg_idx), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TDT(txq->reg_idx), 0);

		/*
		 * Disable Tx Head Writeback RO bit, since this hoses
		 * bookkeeping if things aren't delivered in order.
		 */
		switch (hw->mac.type) {
			case ixgbe_mac_82598EB:
				txctrl = IXGBE_READ_REG(hw,
							IXGBE_DCA_TXCTRL(txq->reg_idx));
				txctrl &= ~IXGBE_DCA_TXCTRL_DESC_WRO_EN;
				IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL(txq->reg_idx),
						txctrl);
				break;

			case ixgbe_mac_82599EB:
			case ixgbe_mac_X540:
			default:
				txctrl = IXGBE_READ_REG(hw,
						IXGBE_DCA_TXCTRL_82599(txq->reg_idx));
				txctrl &= ~IXGBE_DCA_TXCTRL_DESC_WRO_EN;
				IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL_82599(txq->reg_idx),
						txctrl);
				break;
		}
	}

	/* Device configured with multiple TX queues. */
	ixgbe_dev_mq_tx_configure(dev);
}

/*
 * Set up link for 82599 loopback mode Tx->Rx.
 */
static inline void
ixgbe_setup_loopback_link_82599(struct ixgbe_hw *hw)
{
	DEBUGFUNC("ixgbe_setup_loopback_link_82599");

	if (ixgbe_verify_lesm_fw_enabled_82599(hw)) {
		if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_MAC_CSR_SM) !=
				IXGBE_SUCCESS) {
			PMD_INIT_LOG(ERR, "Could not enable loopback mode\n");
			/* ignore error */
			return;
		}
	}

	/* Restart link */
	IXGBE_WRITE_REG(hw,
			IXGBE_AUTOC,
			IXGBE_AUTOC_LMS_10G_LINK_NO_AN | IXGBE_AUTOC_FLU);
	ixgbe_reset_pipeline_82599(hw);

	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_MAC_CSR_SM);
	msec_delay(50);
}


/*
 * Start Transmit and Receive Units.
 */
void
ixgbe_dev_rxtx_start(struct rte_eth_dev *dev)
{
	struct ixgbe_hw     *hw;
	struct igb_tx_queue *txq;
	struct igb_rx_queue *rxq;
	uint32_t txdctl;
	uint32_t dmatxctl;
	uint32_t rxctrl;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		/* Setup Transmit Threshold Registers */
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(txq->reg_idx));
		txdctl |= txq->pthresh & 0x7F;
		txdctl |= ((txq->hthresh & 0x7F) << 8);
		txdctl |= ((txq->wthresh & 0x7F) << 16);
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(txq->reg_idx), txdctl);
	}

	if (hw->mac.type != ixgbe_mac_82598EB) {
		dmatxctl = IXGBE_READ_REG(hw, IXGBE_DMATXCTL);
		dmatxctl |= IXGBE_DMATXCTL_TE;
		IXGBE_WRITE_REG(hw, IXGBE_DMATXCTL, dmatxctl);
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (!txq->start_tx_per_q)
			ixgbe_dev_tx_queue_start(dev, i);
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (!rxq->start_rx_per_q)
			ixgbe_dev_rx_queue_start(dev, i);
	}

	/* Enable Receive engine */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	if (hw->mac.type == ixgbe_mac_82598EB)
		rxctrl |= IXGBE_RXCTRL_DMBYPS;
	rxctrl |= IXGBE_RXCTRL_RXEN;
	hw->mac.ops.enable_rx_dma(hw, rxctrl);

	/* If loopback mode is enabled for 82599, set up the link accordingly */
	if (hw->mac.type == ixgbe_mac_82599EB &&
			dev->data->dev_conf.lpbk_mode == IXGBE_LPBK_82599_TX_RX)
		ixgbe_setup_loopback_link_82599(hw);

}

/*
 * Start Receive Units for specified queue.
 */
int
ixgbe_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ixgbe_hw     *hw;
	struct igb_rx_queue *rxq;
	uint32_t rxdctl;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (rx_queue_id < dev->data->nb_rx_queues) {
		rxq = dev->data->rx_queues[rx_queue_id];

		/* Allocate buffers for descriptor rings */
		if (ixgbe_alloc_rx_queue_mbufs(rxq) != 0) {
			PMD_INIT_LOG(ERR,
				"Could not alloc mbuf for queue:%d\n",
				rx_queue_id);
			return -1;
		}
		rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(rxq->reg_idx));
		rxdctl |= IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(rxq->reg_idx), rxdctl);

		/* Wait until RX Enable ready */
		poll_ms = RTE_IXGBE_REGISTER_POLL_WAIT_10_MS;
		do {
			rte_delay_ms(1);
			rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(rxq->reg_idx));
		} while (--poll_ms && !(rxdctl & IXGBE_RXDCTL_ENABLE));
		if (!poll_ms)
			PMD_INIT_LOG(ERR, "Could not enable "
				     "Rx Queue %d\n", rx_queue_id);
		rte_wmb();
		IXGBE_WRITE_REG(hw, IXGBE_RDH(rxq->reg_idx), 0);
		IXGBE_WRITE_REG(hw, IXGBE_RDT(rxq->reg_idx), rxq->nb_rx_desc - 1);
	} else
		return -1;

	return 0;
}

/*
 * Stop Receive Units for specified queue.
 */
int
ixgbe_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct ixgbe_hw     *hw;
	struct igb_rx_queue *rxq;
	uint32_t rxdctl;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (rx_queue_id < dev->data->nb_rx_queues) {
		rxq = dev->data->rx_queues[rx_queue_id];

		rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(rxq->reg_idx));
		rxdctl &= ~IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(rxq->reg_idx), rxdctl);

		/* Wait until RX Enable ready */
		poll_ms = RTE_IXGBE_REGISTER_POLL_WAIT_10_MS;
		do {
			rte_delay_ms(1);
			rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(rxq->reg_idx));
		} while (--poll_ms && (rxdctl | IXGBE_RXDCTL_ENABLE));
		if (!poll_ms)
			PMD_INIT_LOG(ERR, "Could not disable "
				     "Rx Queue %d\n", rx_queue_id);

		rte_delay_us(RTE_IXGBE_WAIT_100_US);

		ixgbe_rx_queue_release_mbufs(rxq);
		ixgbe_reset_rx_queue(rxq);
	} else
		return -1;

	return 0;
}


/*
 * Start Transmit Units for specified queue.
 */
int
ixgbe_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ixgbe_hw     *hw;
	struct igb_tx_queue *txq;
	uint32_t txdctl;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (tx_queue_id < dev->data->nb_tx_queues) {
		txq = dev->data->tx_queues[tx_queue_id];
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(txq->reg_idx));
		txdctl |= IXGBE_TXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(txq->reg_idx), txdctl);

		/* Wait until TX Enable ready */
		if (hw->mac.type == ixgbe_mac_82599EB) {
			poll_ms = RTE_IXGBE_REGISTER_POLL_WAIT_10_MS;
			do {
				rte_delay_ms(1);
				txdctl = IXGBE_READ_REG(hw,
					IXGBE_TXDCTL(txq->reg_idx));
			} while (--poll_ms && !(txdctl & IXGBE_TXDCTL_ENABLE));
			if (!poll_ms)
				PMD_INIT_LOG(ERR, "Could not enable "
					     "Tx Queue %d\n", tx_queue_id);
		}
		rte_wmb();
		IXGBE_WRITE_REG(hw, IXGBE_TDH(txq->reg_idx), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TDT(txq->reg_idx), 0);
	} else
		return -1;

	return 0;
}

/*
 * Stop Transmit Units for specified queue.
 */
int
ixgbe_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct ixgbe_hw     *hw;
	struct igb_tx_queue *txq;
	uint32_t txdctl;
	uint32_t txtdh, txtdt;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (tx_queue_id < dev->data->nb_tx_queues) {
		txq = dev->data->tx_queues[tx_queue_id];

		/* Wait until TX queue is empty */
		if (hw->mac.type == ixgbe_mac_82599EB) {
			poll_ms = RTE_IXGBE_REGISTER_POLL_WAIT_10_MS;
			do {
				rte_delay_us(RTE_IXGBE_WAIT_100_US);
				txtdh = IXGBE_READ_REG(hw,
						IXGBE_TDH(txq->reg_idx));
				txtdt = IXGBE_READ_REG(hw,
						IXGBE_TDT(txq->reg_idx));
			} while (--poll_ms && (txtdh != txtdt));
			if (!poll_ms)
				PMD_INIT_LOG(ERR,
				"Tx Queue %d is not empty when stopping.\n",
				tx_queue_id);
		}

		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(txq->reg_idx));
		txdctl &= ~IXGBE_TXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(txq->reg_idx), txdctl);

		/* Wait until TX Enable ready */
		if (hw->mac.type == ixgbe_mac_82599EB) {
			poll_ms = RTE_IXGBE_REGISTER_POLL_WAIT_10_MS;
			do {
				rte_delay_ms(1);
				txdctl = IXGBE_READ_REG(hw,
						IXGBE_TXDCTL(txq->reg_idx));
			} while (--poll_ms && (txdctl | IXGBE_TXDCTL_ENABLE));
			if (!poll_ms)
				PMD_INIT_LOG(ERR, "Could not disable "
					     "Tx Queue %d\n", tx_queue_id);
		}

		if (txq->ops != NULL) {
			txq->ops->release_mbufs(txq);
			txq->ops->reset(txq);
		}
	} else
		return -1;

	return 0;
}

/*
 * [VF] Initializes Receive Unit.
 */
int
ixgbevf_dev_rx_init(struct rte_eth_dev *dev)
{
	struct ixgbe_hw     *hw;
	struct igb_rx_queue *rxq;
	struct rte_pktmbuf_pool_private *mbp_priv;
	uint64_t bus_addr;
	uint32_t srrctl;
	uint16_t buf_size;
	uint16_t i;
	int ret;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/*
	 * When the VF driver issues a IXGBE_VF_RESET request, the PF driver
	 * disables the VF receipt of packets if the PF MTU is > 1500.
	 * This is done to deal with 82599 limitations that imposes
	 * the PF and all VFs to share the same MTU.
	 * Then, the PF driver enables again the VF receipt of packet when
	 * the VF driver issues a IXGBE_VF_SET_LPE request.
	 * In the meantime, the VF device cannot be used, even if the VF driver
	 * and the Guest VM network stack are ready to accept packets with a
	 * size up to the PF MTU.
	 * As a work-around to this PF behaviour, force the call to
	 * ixgbevf_rlpml_set_vf even if jumbo frames are not used. This way,
	 * VF packets received can work in all cases.
	 */
	ixgbevf_rlpml_set_vf(hw,
		(uint16_t)dev->data->dev_conf.rxmode.max_rx_pkt_len);

	/* Setup RX queues */
	dev->rx_pkt_burst = ixgbe_recv_pkts;
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		/* Allocate buffers for descriptor rings */
		ret = ixgbe_alloc_rx_queue_mbufs(rxq);
		if (ret)
			return ret;

		/* Setup the Base and Length of the Rx Descriptor Rings */
		bus_addr = rxq->rx_ring_phys_addr;

		IXGBE_WRITE_REG(hw, IXGBE_VFRDBAL(i),
				(uint32_t)(bus_addr & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_VFRDBAH(i),
				(uint32_t)(bus_addr >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_VFRDLEN(i),
				rxq->nb_rx_desc * sizeof(union ixgbe_adv_rx_desc));
		IXGBE_WRITE_REG(hw, IXGBE_VFRDH(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_VFRDT(i), 0);


		/* Configure the SRRCTL register */
#ifdef RTE_HEADER_SPLIT_ENABLE
		/*
		 * Configure Header Split
		 */
		if (dev->data->dev_conf.rxmode.header_split) {

			/* Must setup the PSRTYPE register */
			uint32_t psrtype;
			psrtype = IXGBE_PSRTYPE_TCPHDR |
				IXGBE_PSRTYPE_UDPHDR   |
				IXGBE_PSRTYPE_IPV4HDR  |
				IXGBE_PSRTYPE_IPV6HDR;

			IXGBE_WRITE_REG(hw, IXGBE_VFPSRTYPE(i), psrtype);

			srrctl = ((dev->data->dev_conf.rxmode.split_hdr_size <<
				   IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT) &
				  IXGBE_SRRCTL_BSIZEHDR_MASK);
			srrctl |= E1000_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS;
		} else
#endif
			srrctl = IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;

		/* Set if packets are dropped when no descriptors available */
		if (rxq->drop_en)
			srrctl |= IXGBE_SRRCTL_DROP_EN;

		/*
		 * Configure the RX buffer size in the BSIZEPACKET field of
		 * the SRRCTL register of the queue.
		 * The value is in 1 KB resolution. Valid values can be from
		 * 1 KB to 16 KB.
		 */
		mbp_priv = rte_mempool_get_priv(rxq->mb_pool);
		buf_size = (uint16_t) (mbp_priv->mbuf_data_room_size -
				       RTE_PKTMBUF_HEADROOM);
		srrctl |= ((buf_size >> IXGBE_SRRCTL_BSIZEPKT_SHIFT) &
			   IXGBE_SRRCTL_BSIZEPKT_MASK);

		/*
		 * VF modification to write virtual function SRRCTL register
		 */
		IXGBE_WRITE_REG(hw, IXGBE_VFSRRCTL(i), srrctl);

		buf_size = (uint16_t) ((srrctl & IXGBE_SRRCTL_BSIZEPKT_MASK) <<
				       IXGBE_SRRCTL_BSIZEPKT_SHIFT);

		/* It adds dual VLAN length for supporting dual VLAN */
		if ((dev->data->dev_conf.rxmode.max_rx_pkt_len +
				2 * IXGBE_VLAN_TAG_SIZE) > buf_size) {
			dev->data->scattered_rx = 1;
			dev->rx_pkt_burst = ixgbe_recv_scattered_pkts;
		}
	}

	if (dev->data->dev_conf.rxmode.enable_scatter) {
		dev->rx_pkt_burst = ixgbe_recv_scattered_pkts;
		dev->data->scattered_rx = 1;
	}

	return 0;
}

/*
 * [VF] Initializes Transmit Unit.
 */
void
ixgbevf_dev_tx_init(struct rte_eth_dev *dev)
{
	struct ixgbe_hw     *hw;
	struct igb_tx_queue *txq;
	uint64_t bus_addr;
	uint32_t txctrl;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Setup the Base and Length of the Tx Descriptor Rings */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		bus_addr = txq->tx_ring_phys_addr;
		IXGBE_WRITE_REG(hw, IXGBE_VFTDBAL(i),
				(uint32_t)(bus_addr & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_VFTDBAH(i),
				(uint32_t)(bus_addr >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_VFTDLEN(i),
				txq->nb_tx_desc * sizeof(union ixgbe_adv_tx_desc));
		/* Setup the HW Tx Head and TX Tail descriptor pointers */
		IXGBE_WRITE_REG(hw, IXGBE_VFTDH(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_VFTDT(i), 0);

		/*
		 * Disable Tx Head Writeback RO bit, since this hoses
		 * bookkeeping if things aren't delivered in order.
		 */
		txctrl = IXGBE_READ_REG(hw,
				IXGBE_VFDCA_TXCTRL(i));
		txctrl &= ~IXGBE_DCA_TXCTRL_DESC_WRO_EN;
		IXGBE_WRITE_REG(hw, IXGBE_VFDCA_TXCTRL(i),
				txctrl);
	}
}

/*
 * [VF] Start Transmit and Receive Units.
 */
void
ixgbevf_dev_rxtx_start(struct rte_eth_dev *dev)
{
	struct ixgbe_hw     *hw;
	struct igb_tx_queue *txq;
	struct igb_rx_queue *rxq;
	uint32_t txdctl;
	uint32_t rxdctl;
	uint16_t i;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		/* Setup Transmit Threshold Registers */
		txdctl = IXGBE_READ_REG(hw, IXGBE_VFTXDCTL(i));
		txdctl |= txq->pthresh & 0x7F;
		txdctl |= ((txq->hthresh & 0x7F) << 8);
		txdctl |= ((txq->wthresh & 0x7F) << 16);
		IXGBE_WRITE_REG(hw, IXGBE_VFTXDCTL(i), txdctl);
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {

		txdctl = IXGBE_READ_REG(hw, IXGBE_VFTXDCTL(i));
		txdctl |= IXGBE_TXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_VFTXDCTL(i), txdctl);

		poll_ms = 10;
		/* Wait until TX Enable ready */
		do {
			rte_delay_ms(1);
			txdctl = IXGBE_READ_REG(hw, IXGBE_VFTXDCTL(i));
		} while (--poll_ms && !(txdctl & IXGBE_TXDCTL_ENABLE));
		if (!poll_ms)
			PMD_INIT_LOG(ERR, "Could not enable "
					 "Tx Queue %d\n", i);
	}
	for (i = 0; i < dev->data->nb_rx_queues; i++) {

		rxq = dev->data->rx_queues[i];

		rxdctl = IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(i));
		rxdctl |= IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(i), rxdctl);

		/* Wait until RX Enable ready */
		poll_ms = 10;
		do {
			rte_delay_ms(1);
			rxdctl = IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(i));
		} while (--poll_ms && !(rxdctl & IXGBE_RXDCTL_ENABLE));
		if (!poll_ms)
			PMD_INIT_LOG(ERR, "Could not enable "
					 "Rx Queue %d\n", i);
		rte_wmb();
		IXGBE_WRITE_REG(hw, IXGBE_VFRDT(i), rxq->nb_rx_desc - 1);

	}
}
