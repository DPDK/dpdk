/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_ethdev_driver.h>
#include <rte_memzone.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#include "txgbe_logs.h"
#include "base/txgbe.h"
#include "txgbe_ethdev.h"
#include "txgbe_rxtx.h"

/* Bit Mask to indicate what bits required for building TX context */
static const u64 TXGBE_TX_OFFLOAD_MASK = (PKT_TX_IP_CKSUM |
		PKT_TX_OUTER_IPV6 |
		PKT_TX_OUTER_IPV4 |
		PKT_TX_IPV6 |
		PKT_TX_IPV4 |
		PKT_TX_VLAN_PKT |
		PKT_TX_L4_MASK |
		PKT_TX_TCP_SEG |
		PKT_TX_TUNNEL_MASK |
		PKT_TX_OUTER_IP_CKSUM);

static int
txgbe_is_vf(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	switch (hw->mac.type) {
	case txgbe_mac_raptor_vf:
		return 1;
	default:
		return 0;
	}
}

/*********************************************************************
 *
 *  TX functions
 *
 **********************************************************************/

/*
 * Check for descriptors with their DD bit set and free mbufs.
 * Return the total number of buffers freed.
 */
static __rte_always_inline int
txgbe_tx_free_bufs(struct txgbe_tx_queue *txq)
{
	struct txgbe_tx_entry *txep;
	uint32_t status;
	int i, nb_free = 0;
	struct rte_mbuf *m, *free[RTE_TXGBE_TX_MAX_FREE_BUF_SZ];

	/* check DD bit on threshold descriptor */
	status = txq->tx_ring[txq->tx_next_dd].dw3;
	if (!(status & rte_cpu_to_le_32(TXGBE_TXD_DD))) {
		if (txq->nb_tx_free >> 1 < txq->tx_free_thresh)
			txgbe_set32_masked(txq->tdc_reg_addr,
				TXGBE_TXCFG_FLUSH, TXGBE_TXCFG_FLUSH);
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

		if (nb_free >= RTE_TXGBE_TX_MAX_FREE_BUF_SZ ||
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
tx4(volatile struct txgbe_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t buf_dma_addr;
	uint32_t pkt_len;
	int i;

	for (i = 0; i < 4; ++i, ++txdp, ++pkts) {
		buf_dma_addr = rte_mbuf_data_iova(*pkts);
		pkt_len = (*pkts)->data_len;

		/* write data to descriptor */
		txdp->qw0 = rte_cpu_to_le_64(buf_dma_addr);
		txdp->dw2 = cpu_to_le32(TXGBE_TXD_FLAGS |
					TXGBE_TXD_DATLEN(pkt_len));
		txdp->dw3 = cpu_to_le32(TXGBE_TXD_PAYLEN(pkt_len));

		rte_prefetch0(&(*pkts)->pool);
	}
}

/* Populate 1 descriptor with data from 1 mbuf */
static inline void
tx1(volatile struct txgbe_tx_desc *txdp, struct rte_mbuf **pkts)
{
	uint64_t buf_dma_addr;
	uint32_t pkt_len;

	buf_dma_addr = rte_mbuf_data_iova(*pkts);
	pkt_len = (*pkts)->data_len;

	/* write data to descriptor */
	txdp->qw0 = cpu_to_le64(buf_dma_addr);
	txdp->dw2 = cpu_to_le32(TXGBE_TXD_FLAGS |
				TXGBE_TXD_DATLEN(pkt_len));
	txdp->dw3 = cpu_to_le32(TXGBE_TXD_PAYLEN(pkt_len));

	rte_prefetch0(&(*pkts)->pool);
}

/*
 * Fill H/W descriptor ring with mbuf data.
 * Copy mbuf pointers to the S/W ring.
 */
static inline void
txgbe_tx_fill_hw_ring(struct txgbe_tx_queue *txq, struct rte_mbuf **pkts,
		      uint16_t nb_pkts)
{
	volatile struct txgbe_tx_desc *txdp = &txq->tx_ring[txq->tx_tail];
	struct txgbe_tx_entry *txep = &txq->sw_ring[txq->tx_tail];
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
	struct txgbe_tx_queue *txq = (struct txgbe_tx_queue *)tx_queue;
	uint16_t n = 0;

	/*
	 * Begin scanning the H/W ring for done descriptors when the
	 * number of available descriptors drops below tx_free_thresh.  For
	 * each done descriptor, free the associated buffer.
	 */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		txgbe_tx_free_bufs(txq);

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
		txgbe_tx_fill_hw_ring(txq, tx_pkts, n);
		txq->tx_tail = 0;
	}

	/* Fill H/W descriptor ring with mbuf data */
	txgbe_tx_fill_hw_ring(txq, tx_pkts + n, (uint16_t)(nb_pkts - n));
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
	txgbe_set32_relaxed(txq->tdt_reg_addr, txq->tx_tail);

	return nb_pkts;
}

uint16_t
txgbe_xmit_pkts_simple(void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts)
{
	uint16_t nb_tx;

	/* Try to transmit at least chunks of TX_MAX_BURST pkts */
	if (likely(nb_pkts <= RTE_PMD_TXGBE_TX_MAX_BURST))
		return tx_xmit_pkts(tx_queue, tx_pkts, nb_pkts);

	/* transmit more than the max burst, in chunks of TX_MAX_BURST */
	nb_tx = 0;
	while (nb_pkts) {
		uint16_t ret, n;

		n = (uint16_t)RTE_MIN(nb_pkts, RTE_PMD_TXGBE_TX_MAX_BURST);
		ret = tx_xmit_pkts(tx_queue, &tx_pkts[nb_tx], n);
		nb_tx = (uint16_t)(nb_tx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < n)
			break;
	}

	return nb_tx;
}

static inline void
txgbe_set_xmit_ctx(struct txgbe_tx_queue *txq,
		volatile struct txgbe_tx_ctx_desc *ctx_txd,
		uint64_t ol_flags, union txgbe_tx_offload tx_offload)
{
	union txgbe_tx_offload tx_offload_mask;
	uint32_t type_tucmd_mlhl;
	uint32_t mss_l4len_idx;
	uint32_t ctx_idx;
	uint32_t vlan_macip_lens;
	uint32_t tunnel_seed;

	ctx_idx = txq->ctx_curr;
	tx_offload_mask.data[0] = 0;
	tx_offload_mask.data[1] = 0;

	/* Specify which HW CTX to upload. */
	mss_l4len_idx = TXGBE_TXD_IDX(ctx_idx);
	type_tucmd_mlhl = TXGBE_TXD_CTXT;

	tx_offload_mask.ptid |= ~0;
	type_tucmd_mlhl |= TXGBE_TXD_PTID(tx_offload.ptid);

	/* check if TCP segmentation required for this packet */
	if (ol_flags & PKT_TX_TCP_SEG) {
		tx_offload_mask.l2_len |= ~0;
		tx_offload_mask.l3_len |= ~0;
		tx_offload_mask.l4_len |= ~0;
		tx_offload_mask.tso_segsz |= ~0;
		mss_l4len_idx |= TXGBE_TXD_MSS(tx_offload.tso_segsz);
		mss_l4len_idx |= TXGBE_TXD_L4LEN(tx_offload.l4_len);
	} else { /* no TSO, check if hardware checksum is needed */
		if (ol_flags & PKT_TX_IP_CKSUM) {
			tx_offload_mask.l2_len |= ~0;
			tx_offload_mask.l3_len |= ~0;
		}

		switch (ol_flags & PKT_TX_L4_MASK) {
		case PKT_TX_UDP_CKSUM:
			mss_l4len_idx |=
				TXGBE_TXD_L4LEN(sizeof(struct rte_udp_hdr));
			tx_offload_mask.l2_len |= ~0;
			tx_offload_mask.l3_len |= ~0;
			break;
		case PKT_TX_TCP_CKSUM:
			mss_l4len_idx |=
				TXGBE_TXD_L4LEN(sizeof(struct rte_tcp_hdr));
			tx_offload_mask.l2_len |= ~0;
			tx_offload_mask.l3_len |= ~0;
			break;
		case PKT_TX_SCTP_CKSUM:
			mss_l4len_idx |=
				TXGBE_TXD_L4LEN(sizeof(struct rte_sctp_hdr));
			tx_offload_mask.l2_len |= ~0;
			tx_offload_mask.l3_len |= ~0;
			break;
		default:
			break;
		}
	}

	vlan_macip_lens = TXGBE_TXD_IPLEN(tx_offload.l3_len >> 1);

	if (ol_flags & PKT_TX_TUNNEL_MASK) {
		tx_offload_mask.outer_tun_len |= ~0;
		tx_offload_mask.outer_l2_len |= ~0;
		tx_offload_mask.outer_l3_len |= ~0;
		tx_offload_mask.l2_len |= ~0;
		tunnel_seed = TXGBE_TXD_ETUNLEN(tx_offload.outer_tun_len >> 1);
		tunnel_seed |= TXGBE_TXD_EIPLEN(tx_offload.outer_l3_len >> 2);

		switch (ol_flags & PKT_TX_TUNNEL_MASK) {
		case PKT_TX_TUNNEL_IPIP:
			/* for non UDP / GRE tunneling, set to 0b */
			break;
		case PKT_TX_TUNNEL_VXLAN:
		case PKT_TX_TUNNEL_GENEVE:
			tunnel_seed |= TXGBE_TXD_ETYPE_UDP;
			break;
		case PKT_TX_TUNNEL_GRE:
			tunnel_seed |= TXGBE_TXD_ETYPE_GRE;
			break;
		default:
			PMD_TX_LOG(ERR, "Tunnel type not supported");
			return;
		}
		vlan_macip_lens |= TXGBE_TXD_MACLEN(tx_offload.outer_l2_len);
	} else {
		tunnel_seed = 0;
		vlan_macip_lens |= TXGBE_TXD_MACLEN(tx_offload.l2_len);
	}

	if (ol_flags & PKT_TX_VLAN_PKT) {
		tx_offload_mask.vlan_tci |= ~0;
		vlan_macip_lens |= TXGBE_TXD_VLAN(tx_offload.vlan_tci);
	}

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
what_ctx_update(struct txgbe_tx_queue *txq, uint64_t flags,
		   union txgbe_tx_offload tx_offload)
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
	return TXGBE_CTX_NUM;
}

static inline uint32_t
tx_desc_cksum_flags_to_olinfo(uint64_t ol_flags)
{
	uint32_t tmp = 0;

	if ((ol_flags & PKT_TX_L4_MASK) != PKT_TX_L4_NO_CKSUM) {
		tmp |= TXGBE_TXD_CC;
		tmp |= TXGBE_TXD_L4CS;
	}
	if (ol_flags & PKT_TX_IP_CKSUM) {
		tmp |= TXGBE_TXD_CC;
		tmp |= TXGBE_TXD_IPCS;
	}
	if (ol_flags & PKT_TX_OUTER_IP_CKSUM) {
		tmp |= TXGBE_TXD_CC;
		tmp |= TXGBE_TXD_EIPCS;
	}
	if (ol_flags & PKT_TX_TCP_SEG) {
		tmp |= TXGBE_TXD_CC;
		/* implies IPv4 cksum */
		if (ol_flags & PKT_TX_IPV4)
			tmp |= TXGBE_TXD_IPCS;
		tmp |= TXGBE_TXD_L4CS;
	}
	if (ol_flags & PKT_TX_VLAN_PKT)
		tmp |= TXGBE_TXD_CC;

	return tmp;
}

static inline uint32_t
tx_desc_ol_flags_to_cmdtype(uint64_t ol_flags)
{
	uint32_t cmdtype = 0;

	if (ol_flags & PKT_TX_VLAN_PKT)
		cmdtype |= TXGBE_TXD_VLE;
	if (ol_flags & PKT_TX_TCP_SEG)
		cmdtype |= TXGBE_TXD_TSE;
	if (ol_flags & PKT_TX_MACSEC)
		cmdtype |= TXGBE_TXD_LINKSEC;
	return cmdtype;
}

static inline uint8_t
tx_desc_ol_flags_to_ptid(uint64_t oflags, uint32_t ptype)
{
	bool tun;

	if (ptype)
		return txgbe_encode_ptype(ptype);

	/* Only support flags in TXGBE_TX_OFFLOAD_MASK */
	tun = !!(oflags & PKT_TX_TUNNEL_MASK);

	/* L2 level */
	ptype = RTE_PTYPE_L2_ETHER;
	if (oflags & PKT_TX_VLAN)
		ptype |= RTE_PTYPE_L2_ETHER_VLAN;

	/* L3 level */
	if (oflags & (PKT_TX_OUTER_IPV4 | PKT_TX_OUTER_IP_CKSUM))
		ptype |= RTE_PTYPE_L3_IPV4;
	else if (oflags & (PKT_TX_OUTER_IPV6))
		ptype |= RTE_PTYPE_L3_IPV6;

	if (oflags & (PKT_TX_IPV4 | PKT_TX_IP_CKSUM))
		ptype |= (tun ? RTE_PTYPE_INNER_L3_IPV4 : RTE_PTYPE_L3_IPV4);
	else if (oflags & (PKT_TX_IPV6))
		ptype |= (tun ? RTE_PTYPE_INNER_L3_IPV6 : RTE_PTYPE_L3_IPV6);

	/* L4 level */
	switch (oflags & (PKT_TX_L4_MASK)) {
	case PKT_TX_TCP_CKSUM:
		ptype |= (tun ? RTE_PTYPE_INNER_L4_TCP : RTE_PTYPE_L4_TCP);
		break;
	case PKT_TX_UDP_CKSUM:
		ptype |= (tun ? RTE_PTYPE_INNER_L4_UDP : RTE_PTYPE_L4_UDP);
		break;
	case PKT_TX_SCTP_CKSUM:
		ptype |= (tun ? RTE_PTYPE_INNER_L4_SCTP : RTE_PTYPE_L4_SCTP);
		break;
	}

	if (oflags & PKT_TX_TCP_SEG)
		ptype |= (tun ? RTE_PTYPE_INNER_L4_TCP : RTE_PTYPE_L4_TCP);

	/* Tunnel */
	switch (oflags & PKT_TX_TUNNEL_MASK) {
	case PKT_TX_TUNNEL_VXLAN:
		ptype |= RTE_PTYPE_L2_ETHER |
			 RTE_PTYPE_L3_IPV4 |
			 RTE_PTYPE_TUNNEL_VXLAN;
		ptype |= RTE_PTYPE_INNER_L2_ETHER;
		break;
	case PKT_TX_TUNNEL_GRE:
		ptype |= RTE_PTYPE_L2_ETHER |
			 RTE_PTYPE_L3_IPV4 |
			 RTE_PTYPE_TUNNEL_GRE;
		ptype |= RTE_PTYPE_INNER_L2_ETHER;
		break;
	case PKT_TX_TUNNEL_GENEVE:
		ptype |= RTE_PTYPE_L2_ETHER |
			 RTE_PTYPE_L3_IPV4 |
			 RTE_PTYPE_TUNNEL_GENEVE;
		ptype |= RTE_PTYPE_INNER_L2_ETHER;
		break;
	case PKT_TX_TUNNEL_VXLAN_GPE:
		ptype |= RTE_PTYPE_L2_ETHER |
			 RTE_PTYPE_L3_IPV4 |
			 RTE_PTYPE_TUNNEL_VXLAN_GPE;
		ptype |= RTE_PTYPE_INNER_L2_ETHER;
		break;
	case PKT_TX_TUNNEL_IPIP:
	case PKT_TX_TUNNEL_IP:
		ptype |= RTE_PTYPE_L2_ETHER |
			 RTE_PTYPE_L3_IPV4 |
			 RTE_PTYPE_TUNNEL_IP;
		break;
	}

	return txgbe_encode_ptype(ptype);
}

#ifndef DEFAULT_TX_FREE_THRESH
#define DEFAULT_TX_FREE_THRESH 32
#endif

/* Reset transmit descriptors after they have been used */
static inline int
txgbe_xmit_cleanup(struct txgbe_tx_queue *txq)
{
	struct txgbe_tx_entry *sw_ring = txq->sw_ring;
	volatile struct txgbe_tx_desc *txr = txq->tx_ring;
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
	if (!(status & rte_cpu_to_le_32(TXGBE_TXD_DD))) {
		PMD_TX_FREE_LOG(DEBUG,
				"TX descriptor %4u is not done"
				"(port=%d queue=%d)",
				desc_to_clean_to,
				txq->port_id, txq->queue_id);
		if (txq->nb_tx_free >> 1 < txq->tx_free_thresh)
			txgbe_set32_masked(txq->tdc_reg_addr,
				TXGBE_TXCFG_FLUSH, TXGBE_TXCFG_FLUSH);
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
	txr[desc_to_clean_to].dw3 = 0;

	/* Update the txq to reflect the last descriptor that was cleaned */
	txq->last_desc_cleaned = desc_to_clean_to;
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + nb_tx_to_clean);

	/* No Error */
	return 0;
}

static inline uint8_t
txgbe_get_tun_len(struct rte_mbuf *mbuf)
{
	struct txgbe_genevehdr genevehdr;
	const struct txgbe_genevehdr *gh;
	uint8_t tun_len;

	switch (mbuf->ol_flags & PKT_TX_TUNNEL_MASK) {
	case PKT_TX_TUNNEL_IPIP:
		tun_len = 0;
		break;
	case PKT_TX_TUNNEL_VXLAN:
	case PKT_TX_TUNNEL_VXLAN_GPE:
		tun_len = sizeof(struct txgbe_udphdr)
			+ sizeof(struct txgbe_vxlanhdr);
		break;
	case PKT_TX_TUNNEL_GRE:
		tun_len = sizeof(struct txgbe_nvgrehdr);
		break;
	case PKT_TX_TUNNEL_GENEVE:
		gh = rte_pktmbuf_read(mbuf,
			mbuf->outer_l2_len + mbuf->outer_l3_len,
			sizeof(genevehdr), &genevehdr);
		tun_len = sizeof(struct txgbe_udphdr)
			+ sizeof(struct txgbe_genevehdr)
			+ (gh->opt_len << 2);
		break;
	default:
		tun_len = 0;
	}

	return tun_len;
}

uint16_t
txgbe_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	struct txgbe_tx_queue *txq;
	struct txgbe_tx_entry *sw_ring;
	struct txgbe_tx_entry *txe, *txn;
	volatile struct txgbe_tx_desc *txr;
	volatile struct txgbe_tx_desc *txd;
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
	union txgbe_tx_offload tx_offload;

	tx_offload.data[0] = 0;
	tx_offload.data[1] = 0;
	txq = tx_queue;
	sw_ring = txq->sw_ring;
	txr     = txq->tx_ring;
	tx_id   = txq->tx_tail;
	txe = &sw_ring[tx_id];

	/* Determine if the descriptor ring needs to be cleaned. */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		txgbe_xmit_cleanup(txq);

	rte_prefetch0(&txe->mbuf->pool);

	/* TX loop */
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
		tx_ol_req = ol_flags & TXGBE_TX_OFFLOAD_MASK;
		if (tx_ol_req) {
			tx_offload.ptid = tx_desc_ol_flags_to_ptid(tx_ol_req,
					tx_pkt->packet_type);
			tx_offload.l2_len = tx_pkt->l2_len;
			tx_offload.l3_len = tx_pkt->l3_len;
			tx_offload.l4_len = tx_pkt->l4_len;
			tx_offload.vlan_tci = tx_pkt->vlan_tci;
			tx_offload.tso_segsz = tx_pkt->tso_segsz;
			tx_offload.outer_l2_len = tx_pkt->outer_l2_len;
			tx_offload.outer_l3_len = tx_pkt->outer_l3_len;
			tx_offload.outer_tun_len = txgbe_get_tun_len(tx_pkt);

			/* If new context need be built or reuse the exist ctx*/
			ctx = what_ctx_update(txq, tx_ol_req, tx_offload);
			/* Only allocate context descriptor if required */
			new_ctx = (ctx == TXGBE_CTX_NUM);
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
		 * Determine the last TX descriptor to allocate in the TX ring
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
		 * Make sure there are enough TX descriptors available to
		 * transmit the entire packet.
		 * nb_used better be less than or equal to txq->tx_free_thresh
		 */
		if (nb_used > txq->nb_tx_free) {
			PMD_TX_FREE_LOG(DEBUG,
					"Not enough free TX descriptors "
					"nb_used=%4u nb_free=%4u "
					"(port=%d queue=%d)",
					nb_used, txq->nb_tx_free,
					txq->port_id, txq->queue_id);

			if (txgbe_xmit_cleanup(txq) != 0) {
				/* Could not clean any descriptors */
				if (nb_tx == 0)
					return 0;
				goto end_of_tx;
			}

			/* nb_used better be <= txq->tx_free_thresh */
			if (unlikely(nb_used > txq->tx_free_thresh)) {
				PMD_TX_FREE_LOG(DEBUG,
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
				 * Loop here until there are enough TX
				 * descriptors or until the ring cannot be
				 * cleaned.
				 */
				while (nb_used > txq->nb_tx_free) {
					if (txgbe_xmit_cleanup(txq) != 0) {
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
		 * By now there are enough free TX descriptors to transmit
		 * the packet.
		 */

		/*
		 * Set common flags of all TX Data Descriptors.
		 *
		 * The following bits must be set in all Data Descriptors:
		 *   - TXGBE_TXD_DTYP_DATA
		 *   - TXGBE_TXD_DCMD_DEXT
		 *
		 * The following bits must be set in the first Data Descriptor
		 * and are ignored in the other ones:
		 *   - TXGBE_TXD_DCMD_IFCS
		 *   - TXGBE_TXD_MAC_1588
		 *   - TXGBE_TXD_DCMD_VLE
		 *
		 * The following bits must only be set in the last Data
		 * Descriptor:
		 *   - TXGBE_TXD_CMD_EOP
		 *
		 * The following bits can be set in any Data Descriptor, but
		 * are only set in the last Data Descriptor:
		 *   - TXGBE_TXD_CMD_RS
		 */
		cmd_type_len = TXGBE_TXD_FCS;

		olinfo_status = 0;
		if (tx_ol_req) {
			if (ol_flags & PKT_TX_TCP_SEG) {
				/* when TSO is on, paylen in descriptor is the
				 * not the packet len but the tcp payload len
				 */
				pkt_len -= (tx_offload.l2_len +
					tx_offload.l3_len + tx_offload.l4_len);
				pkt_len -=
					(tx_pkt->ol_flags & PKT_TX_TUNNEL_MASK)
					? tx_offload.outer_l2_len +
					  tx_offload.outer_l3_len : 0;
			}

			/*
			 * Setup the TX Advanced Context Descriptor if required
			 */
			if (new_ctx) {
				volatile struct txgbe_tx_ctx_desc *ctx_txd;

				ctx_txd = (volatile struct txgbe_tx_ctx_desc *)
				    &txr[tx_id];

				txn = &sw_ring[txe->next_id];
				rte_prefetch0(&txn->mbuf->pool);

				if (txe->mbuf != NULL) {
					rte_pktmbuf_free_seg(txe->mbuf);
					txe->mbuf = NULL;
				}

				txgbe_set_xmit_ctx(txq, ctx_txd, tx_ol_req,
					tx_offload);

				txe->last_id = tx_last;
				tx_id = txe->next_id;
				txe = txn;
			}

			/*
			 * Setup the TX Advanced Data Descriptor,
			 * This path will go through
			 * whatever new/reuse the context descriptor
			 */
			cmd_type_len  |= tx_desc_ol_flags_to_cmdtype(ol_flags);
			olinfo_status |=
				tx_desc_cksum_flags_to_olinfo(ol_flags);
			olinfo_status |= TXGBE_TXD_IDX(ctx);
		}

		olinfo_status |= TXGBE_TXD_PAYLEN(pkt_len);

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
		cmd_type_len |= TXGBE_TXD_EOP;
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
	txgbe_set32_relaxed(txq->tdt_reg_addr, tx_id);
	txq->tx_tail = tx_id;

	return nb_tx;
}

uint64_t
txgbe_get_rx_queue_offloads(struct rte_eth_dev *dev __rte_unused)
{
	return DEV_RX_OFFLOAD_VLAN_STRIP;
}

uint64_t
txgbe_get_rx_port_offloads(struct rte_eth_dev *dev)
{
	uint64_t offloads;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_eth_dev_sriov *sriov = &RTE_ETH_DEV_SRIOV(dev);

	offloads = DEV_RX_OFFLOAD_IPV4_CKSUM  |
		   DEV_RX_OFFLOAD_UDP_CKSUM   |
		   DEV_RX_OFFLOAD_TCP_CKSUM   |
		   DEV_RX_OFFLOAD_KEEP_CRC    |
		   DEV_RX_OFFLOAD_JUMBO_FRAME |
		   DEV_RX_OFFLOAD_VLAN_FILTER |
		   DEV_RX_OFFLOAD_RSS_HASH |
		   DEV_RX_OFFLOAD_SCATTER;

	if (!txgbe_is_vf(dev))
		offloads |= (DEV_RX_OFFLOAD_VLAN_FILTER |
			     DEV_RX_OFFLOAD_QINQ_STRIP |
			     DEV_RX_OFFLOAD_VLAN_EXTEND);

	/*
	 * RSC is only supported by PF devices in a non-SR-IOV
	 * mode.
	 */
	if (hw->mac.type == txgbe_mac_raptor && !sriov->active)
		offloads |= DEV_RX_OFFLOAD_TCP_LRO;

	if (hw->mac.type == txgbe_mac_raptor)
		offloads |= DEV_RX_OFFLOAD_MACSEC_STRIP;

	offloads |= DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM;

	return offloads;
}

static void __rte_cold
txgbe_tx_queue_release_mbufs(struct txgbe_tx_queue *txq)
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

static void __rte_cold
txgbe_tx_free_swring(struct txgbe_tx_queue *txq)
{
	if (txq != NULL &&
	    txq->sw_ring != NULL)
		rte_free(txq->sw_ring);
}

static void __rte_cold
txgbe_tx_queue_release(struct txgbe_tx_queue *txq)
{
	if (txq != NULL && txq->ops != NULL) {
		txq->ops->release_mbufs(txq);
		txq->ops->free_swring(txq);
		rte_free(txq);
	}
}

void __rte_cold
txgbe_dev_tx_queue_release(void *txq)
{
	txgbe_tx_queue_release(txq);
}

static const struct txgbe_txq_ops def_txq_ops = {
	.release_mbufs = txgbe_tx_queue_release_mbufs,
	.free_swring = txgbe_tx_free_swring,
};

void __rte_cold
txgbe_set_tx_function(struct rte_eth_dev *dev, struct txgbe_tx_queue *txq)
{
	/* Use a simple Tx queue (no offloads, no multi segs) if possible */
	if (txq->offloads == 0 &&
			txq->tx_free_thresh >= RTE_PMD_TXGBE_TX_MAX_BURST) {
		PMD_INIT_LOG(DEBUG, "Using simple tx code path");
		dev->tx_pkt_burst = txgbe_xmit_pkts_simple;
		dev->tx_pkt_prepare = NULL;
	} else {
		PMD_INIT_LOG(DEBUG, "Using full-featured tx code path");
		PMD_INIT_LOG(DEBUG,
				" - offloads = 0x%" PRIx64,
				txq->offloads);
		PMD_INIT_LOG(DEBUG,
				" - tx_free_thresh = %lu [RTE_PMD_TXGBE_TX_MAX_BURST=%lu]",
				(unsigned long)txq->tx_free_thresh,
				(unsigned long)RTE_PMD_TXGBE_TX_MAX_BURST);
		dev->tx_pkt_burst = txgbe_xmit_pkts;
	}
}

uint64_t
txgbe_get_tx_queue_offloads(struct rte_eth_dev *dev)
{
	RTE_SET_USED(dev);

	return 0;
}

uint64_t
txgbe_get_tx_port_offloads(struct rte_eth_dev *dev)
{
	uint64_t tx_offload_capa;

	tx_offload_capa =
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM  |
		DEV_TX_OFFLOAD_UDP_CKSUM   |
		DEV_TX_OFFLOAD_TCP_CKSUM   |
		DEV_TX_OFFLOAD_SCTP_CKSUM  |
		DEV_TX_OFFLOAD_TCP_TSO     |
		DEV_TX_OFFLOAD_UDP_TSO	   |
		DEV_TX_OFFLOAD_UDP_TNL_TSO	|
		DEV_TX_OFFLOAD_IP_TNL_TSO	|
		DEV_TX_OFFLOAD_VXLAN_TNL_TSO	|
		DEV_TX_OFFLOAD_GRE_TNL_TSO	|
		DEV_TX_OFFLOAD_IPIP_TNL_TSO	|
		DEV_TX_OFFLOAD_GENEVE_TNL_TSO	|
		DEV_TX_OFFLOAD_MULTI_SEGS;

	if (!txgbe_is_vf(dev))
		tx_offload_capa |= DEV_TX_OFFLOAD_QINQ_INSERT;

	tx_offload_capa |= DEV_TX_OFFLOAD_MACSEC_INSERT;

	tx_offload_capa |= DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM;

	return tx_offload_capa;
}

int __rte_cold
txgbe_dev_tx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_txconf *tx_conf)
{
	const struct rte_memzone *tz;
	struct txgbe_tx_queue *txq;
	struct txgbe_hw     *hw;
	uint16_t tx_free_thresh;
	uint64_t offloads;

	PMD_INIT_FUNC_TRACE();
	hw = TXGBE_DEV_HW(dev);

	offloads = tx_conf->offloads | dev->data->dev_conf.txmode.offloads;

	/*
	 * Validate number of transmit descriptors.
	 * It must not exceed hardware maximum, and must be multiple
	 * of TXGBE_ALIGN.
	 */
	if (nb_desc % TXGBE_TXD_ALIGN != 0 ||
	    nb_desc > TXGBE_RING_DESC_MAX ||
	    nb_desc < TXGBE_RING_DESC_MIN) {
		return -EINVAL;
	}

	/*
	 * The TX descriptor ring will be cleaned after txq->tx_free_thresh
	 * descriptors are used or if the number of descriptors required
	 * to transmit a packet is greater than the number of free TX
	 * descriptors.
	 * One descriptor in the TX ring is used as a sentinel to avoid a
	 * H/W race condition, hence the maximum threshold constraints.
	 * When set to zero use default values.
	 */
	tx_free_thresh = (uint16_t)((tx_conf->tx_free_thresh) ?
			tx_conf->tx_free_thresh : DEFAULT_TX_FREE_THRESH);
	if (tx_free_thresh >= (nb_desc - 3)) {
		PMD_INIT_LOG(ERR, "tx_free_thresh must be less than the number of "
			     "TX descriptors minus 3. (tx_free_thresh=%u "
			     "port=%d queue=%d)",
			     (unsigned int)tx_free_thresh,
			     (int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	if ((nb_desc % tx_free_thresh) != 0) {
		PMD_INIT_LOG(ERR, "tx_free_thresh must be a divisor of the "
			     "number of TX descriptors. (tx_free_thresh=%u "
			     "port=%d queue=%d)", (unsigned int)tx_free_thresh,
			     (int)dev->data->port_id, (int)queue_idx);
		return -(EINVAL);
	}

	/* Free memory prior to re-allocation if needed... */
	if (dev->data->tx_queues[queue_idx] != NULL) {
		txgbe_tx_queue_release(dev->data->tx_queues[queue_idx]);
		dev->data->tx_queues[queue_idx] = NULL;
	}

	/* First allocate the tx queue data structure */
	txq = rte_zmalloc_socket("ethdev TX queue",
				 sizeof(struct txgbe_tx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL)
		return -ENOMEM;

	/*
	 * Allocate TX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	tz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_idx,
			sizeof(struct txgbe_tx_desc) * TXGBE_RING_DESC_MAX,
			TXGBE_ALIGN, socket_id);
	if (tz == NULL) {
		txgbe_tx_queue_release(txq);
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

	/* Modification to set tail pointer for virtual function
	 * if vf is detected.
	 */
	if (hw->mac.type == txgbe_mac_raptor_vf) {
		txq->tdt_reg_addr = TXGBE_REG_ADDR(hw, TXGBE_TXWP(queue_idx));
		txq->tdc_reg_addr = TXGBE_REG_ADDR(hw, TXGBE_TXCFG(queue_idx));
	} else {
		txq->tdt_reg_addr = TXGBE_REG_ADDR(hw,
						TXGBE_TXWP(txq->reg_idx));
		txq->tdc_reg_addr = TXGBE_REG_ADDR(hw,
						TXGBE_TXCFG(txq->reg_idx));
	}

	txq->tx_ring_phys_addr = TMZ_PADDR(tz);
	txq->tx_ring = (struct txgbe_tx_desc *)TMZ_VADDR(tz);

	/* Allocate software ring */
	txq->sw_ring = rte_zmalloc_socket("txq->sw_ring",
				sizeof(struct txgbe_tx_entry) * nb_desc,
				RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL) {
		txgbe_tx_queue_release(txq);
		return -ENOMEM;
	}
	PMD_INIT_LOG(DEBUG, "sw_ring=%p hw_ring=%p dma_addr=0x%" PRIx64,
		     txq->sw_ring, txq->tx_ring, txq->tx_ring_phys_addr);

	/* set up scalar TX function as appropriate */
	txgbe_set_tx_function(dev, txq);

	txq->ops->reset(txq);

	dev->data->tx_queues[queue_idx] = txq;

	return 0;
}

/**
 * txgbe_free_sc_cluster - free the not-yet-completed scattered cluster
 *
 * The "next" pointer of the last segment of (not-yet-completed) RSC clusters
 * in the sw_rsc_ring is not set to NULL but rather points to the next
 * mbuf of this RSC aggregation (that has not been completed yet and still
 * resides on the HW ring). So, instead of calling for rte_pktmbuf_free() we
 * will just free first "nb_segs" segments of the cluster explicitly by calling
 * an rte_pktmbuf_free_seg().
 *
 * @m scattered cluster head
 */
static void __rte_cold
txgbe_free_sc_cluster(struct rte_mbuf *m)
{
	uint16_t i, nb_segs = m->nb_segs;
	struct rte_mbuf *next_seg;

	for (i = 0; i < nb_segs; i++) {
		next_seg = m->next;
		rte_pktmbuf_free_seg(m);
		m = next_seg;
	}
}

static void __rte_cold
txgbe_rx_queue_release_mbufs(struct txgbe_rx_queue *rxq)
{
	unsigned int i;

	if (rxq->sw_ring != NULL) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf != NULL) {
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
				rxq->sw_ring[i].mbuf = NULL;
			}
		}
		if (rxq->rx_nb_avail) {
			for (i = 0; i < rxq->rx_nb_avail; ++i) {
				struct rte_mbuf *mb;

				mb = rxq->rx_stage[rxq->rx_next_avail + i];
				rte_pktmbuf_free_seg(mb);
			}
			rxq->rx_nb_avail = 0;
		}
	}

	if (rxq->sw_sc_ring)
		for (i = 0; i < rxq->nb_rx_desc; i++)
			if (rxq->sw_sc_ring[i].fbuf) {
				txgbe_free_sc_cluster(rxq->sw_sc_ring[i].fbuf);
				rxq->sw_sc_ring[i].fbuf = NULL;
			}
}

static void __rte_cold
txgbe_rx_queue_release(struct txgbe_rx_queue *rxq)
{
	if (rxq != NULL) {
		txgbe_rx_queue_release_mbufs(rxq);
		rte_free(rxq->sw_ring);
		rte_free(rxq->sw_sc_ring);
		rte_free(rxq);
	}
}

void __rte_cold
txgbe_dev_rx_queue_release(void *rxq)
{
	txgbe_rx_queue_release(rxq);
}

/*
 * Check if Rx Burst Bulk Alloc function can be used.
 * Return
 *        0: the preconditions are satisfied and the bulk allocation function
 *           can be used.
 *  -EINVAL: the preconditions are NOT satisfied and the default Rx burst
 *           function must be used.
 */
static inline int __rte_cold
check_rx_burst_bulk_alloc_preconditions(struct txgbe_rx_queue *rxq)
{
	int ret = 0;

	/*
	 * Make sure the following pre-conditions are satisfied:
	 *   rxq->rx_free_thresh >= RTE_PMD_TXGBE_RX_MAX_BURST
	 *   rxq->rx_free_thresh < rxq->nb_rx_desc
	 *   (rxq->nb_rx_desc % rxq->rx_free_thresh) == 0
	 * Scattered packets are not supported.  This should be checked
	 * outside of this function.
	 */
	if (!(rxq->rx_free_thresh >= RTE_PMD_TXGBE_RX_MAX_BURST)) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->rx_free_thresh=%d, "
			     "RTE_PMD_TXGBE_RX_MAX_BURST=%d",
			     rxq->rx_free_thresh, RTE_PMD_TXGBE_RX_MAX_BURST);
		ret = -EINVAL;
	} else if (!(rxq->rx_free_thresh < rxq->nb_rx_desc)) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->rx_free_thresh=%d, "
			     "rxq->nb_rx_desc=%d",
			     rxq->rx_free_thresh, rxq->nb_rx_desc);
		ret = -EINVAL;
	} else if (!((rxq->nb_rx_desc % rxq->rx_free_thresh) == 0)) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions: "
			     "rxq->nb_rx_desc=%d, "
			     "rxq->rx_free_thresh=%d",
			     rxq->nb_rx_desc, rxq->rx_free_thresh);
		ret = -EINVAL;
	}

	return ret;
}

/* Reset dynamic txgbe_rx_queue fields back to defaults */
static void __rte_cold
txgbe_reset_rx_queue(struct txgbe_adapter *adapter, struct txgbe_rx_queue *rxq)
{
	static const struct txgbe_rx_desc zeroed_desc = {
						{{0}, {0} }, {{0}, {0} } };
	unsigned int i;
	uint16_t len = rxq->nb_rx_desc;

	/*
	 * By default, the Rx queue setup function allocates enough memory for
	 * TXGBE_RING_DESC_MAX.  The Rx Burst bulk allocation function requires
	 * extra memory at the end of the descriptor ring to be zero'd out.
	 */
	if (adapter->rx_bulk_alloc_allowed)
		/* zero out extra memory */
		len += RTE_PMD_TXGBE_RX_MAX_BURST;

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

int __rte_cold
txgbe_dev_rx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t queue_idx,
			 uint16_t nb_desc,
			 unsigned int socket_id,
			 const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	const struct rte_memzone *rz;
	struct txgbe_rx_queue *rxq;
	struct txgbe_hw     *hw;
	uint16_t len;
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);
	uint64_t offloads;

	PMD_INIT_FUNC_TRACE();
	hw = TXGBE_DEV_HW(dev);

	offloads = rx_conf->offloads | dev->data->dev_conf.rxmode.offloads;

	/*
	 * Validate number of receive descriptors.
	 * It must not exceed hardware maximum, and must be multiple
	 * of TXGBE_ALIGN.
	 */
	if (nb_desc % TXGBE_RXD_ALIGN != 0 ||
			nb_desc > TXGBE_RING_DESC_MAX ||
			nb_desc < TXGBE_RING_DESC_MIN) {
		return -EINVAL;
	}

	/* Free memory prior to re-allocation if needed... */
	if (dev->data->rx_queues[queue_idx] != NULL) {
		txgbe_rx_queue_release(dev->data->rx_queues[queue_idx]);
		dev->data->rx_queues[queue_idx] = NULL;
	}

	/* First allocate the rx queue data structure */
	rxq = rte_zmalloc_socket("ethdev RX queue",
				 sizeof(struct txgbe_rx_queue),
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
	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_KEEP_CRC)
		rxq->crc_len = RTE_ETHER_CRC_LEN;
	else
		rxq->crc_len = 0;
	rxq->drop_en = rx_conf->rx_drop_en;
	rxq->rx_deferred_start = rx_conf->rx_deferred_start;
	rxq->offloads = offloads;

	/*
	 * The packet type in RX descriptor is different for different NICs.
	 * So set different masks for different NICs.
	 */
	rxq->pkt_type_mask = TXGBE_PTID_MASK;

	/*
	 * Allocate RX ring hardware descriptors. A memzone large enough to
	 * handle the maximum ring size is allocated in order to allow for
	 * resizing in later calls to the queue setup function.
	 */
	rz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_idx,
				      RX_RING_SZ, TXGBE_ALIGN, socket_id);
	if (rz == NULL) {
		txgbe_rx_queue_release(rxq);
		return -ENOMEM;
	}

	/*
	 * Zero init all the descriptors in the ring.
	 */
	memset(rz->addr, 0, RX_RING_SZ);

	/*
	 * Modified to setup VFRDT for Virtual Function
	 */
	if (hw->mac.type == txgbe_mac_raptor_vf) {
		rxq->rdt_reg_addr =
			TXGBE_REG_ADDR(hw, TXGBE_RXWP(queue_idx));
		rxq->rdh_reg_addr =
			TXGBE_REG_ADDR(hw, TXGBE_RXRP(queue_idx));
	} else {
		rxq->rdt_reg_addr =
			TXGBE_REG_ADDR(hw, TXGBE_RXWP(rxq->reg_idx));
		rxq->rdh_reg_addr =
			TXGBE_REG_ADDR(hw, TXGBE_RXRP(rxq->reg_idx));
	}

	rxq->rx_ring_phys_addr = TMZ_PADDR(rz);
	rxq->rx_ring = (struct txgbe_rx_desc *)TMZ_VADDR(rz);

	/*
	 * Certain constraints must be met in order to use the bulk buffer
	 * allocation Rx burst function. If any of Rx queues doesn't meet them
	 * the feature should be disabled for the whole port.
	 */
	if (check_rx_burst_bulk_alloc_preconditions(rxq)) {
		PMD_INIT_LOG(DEBUG, "queue[%d] doesn't meet Rx Bulk Alloc "
				    "preconditions - canceling the feature for "
				    "the whole port[%d]",
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
		len += RTE_PMD_TXGBE_RX_MAX_BURST;

	rxq->sw_ring = rte_zmalloc_socket("rxq->sw_ring",
					  sizeof(struct txgbe_rx_entry) * len,
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq->sw_ring) {
		txgbe_rx_queue_release(rxq);
		return -ENOMEM;
	}

	/*
	 * Always allocate even if it's not going to be needed in order to
	 * simplify the code.
	 *
	 * This ring is used in LRO and Scattered Rx cases and Scattered Rx may
	 * be requested in txgbe_dev_rx_init(), which is called later from
	 * dev_start() flow.
	 */
	rxq->sw_sc_ring =
		rte_zmalloc_socket("rxq->sw_sc_ring",
				  sizeof(struct txgbe_scattered_rx_entry) * len,
				  RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq->sw_sc_ring) {
		txgbe_rx_queue_release(rxq);
		return -ENOMEM;
	}

	PMD_INIT_LOG(DEBUG, "sw_ring=%p sw_sc_ring=%p hw_ring=%p "
			    "dma_addr=0x%" PRIx64,
		     rxq->sw_ring, rxq->sw_sc_ring, rxq->rx_ring,
		     rxq->rx_ring_phys_addr);

	dev->data->rx_queues[queue_idx] = rxq;

	txgbe_reset_rx_queue(adapter, rxq);

	return 0;
}

void
txgbe_dev_free_queues(struct rte_eth_dev *dev)
{
	unsigned int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		txgbe_dev_rx_queue_release(dev->data->rx_queues[i]);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txgbe_dev_tx_queue_release(dev->data->tx_queues[i]);
		dev->data->tx_queues[i] = NULL;
	}
	dev->data->nb_tx_queues = 0;
}

void __rte_cold
txgbe_set_rx_function(struct rte_eth_dev *dev)
{
	RTE_SET_USED(dev);
}

static int __rte_cold
txgbe_alloc_rx_queue_mbufs(struct txgbe_rx_queue *rxq)
{
	struct txgbe_rx_entry *rxe = rxq->sw_ring;
	uint64_t dma_addr;
	unsigned int i;

	/* Initialize software ring entries */
	for (i = 0; i < rxq->nb_rx_desc; i++) {
		volatile struct txgbe_rx_desc *rxd;
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);

		if (mbuf == NULL) {
			PMD_INIT_LOG(ERR, "RX mbuf alloc failed queue_id=%u",
				     (unsigned int)rxq->queue_id);
			return -ENOMEM;
		}

		mbuf->data_off = RTE_PKTMBUF_HEADROOM;
		mbuf->port = rxq->port_id;

		dma_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(mbuf));
		rxd = &rxq->rx_ring[i];
		TXGBE_RXD_HDRADDR(rxd, 0);
		TXGBE_RXD_PKTADDR(rxd, dma_addr);
		rxe[i].mbuf = mbuf;
	}

	return 0;
}

/**
 * txgbe_get_rscctl_maxdesc
 *
 * @pool Memory pool of the Rx queue
 */
static inline uint32_t
txgbe_get_rscctl_maxdesc(struct rte_mempool *pool)
{
	struct rte_pktmbuf_pool_private *mp_priv = rte_mempool_get_priv(pool);

	uint16_t maxdesc =
		RTE_IPV4_MAX_PKT_LEN /
			(mp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM);

	if (maxdesc >= 16)
		return TXGBE_RXCFG_RSCMAX_16;
	else if (maxdesc >= 8)
		return TXGBE_RXCFG_RSCMAX_8;
	else if (maxdesc >= 4)
		return TXGBE_RXCFG_RSCMAX_4;
	else
		return TXGBE_RXCFG_RSCMAX_1;
}

/**
 * txgbe_set_rsc - configure RSC related port HW registers
 *
 * Configures the port's RSC related registers.
 *
 * @dev port handle
 *
 * Returns 0 in case of success or a non-zero error code
 */
static int
txgbe_set_rsc(struct rte_eth_dev *dev)
{
	struct rte_eth_rxmode *rx_conf = &dev->data->dev_conf.rxmode;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_eth_dev_info dev_info = { 0 };
	bool rsc_capable = false;
	uint16_t i;
	uint32_t rdrxctl;
	uint32_t rfctl;

	/* Sanity check */
	dev->dev_ops->dev_infos_get(dev, &dev_info);
	if (dev_info.rx_offload_capa & DEV_RX_OFFLOAD_TCP_LRO)
		rsc_capable = true;

	if (!rsc_capable && (rx_conf->offloads & DEV_RX_OFFLOAD_TCP_LRO)) {
		PMD_INIT_LOG(CRIT, "LRO is requested on HW that doesn't "
				   "support it");
		return -EINVAL;
	}

	/* RSC global configuration */

	if ((rx_conf->offloads & DEV_RX_OFFLOAD_KEEP_CRC) &&
	     (rx_conf->offloads & DEV_RX_OFFLOAD_TCP_LRO)) {
		PMD_INIT_LOG(CRIT, "LRO can't be enabled when HW CRC "
				    "is disabled");
		return -EINVAL;
	}

	rfctl = rd32(hw, TXGBE_PSRCTL);
	if (rsc_capable && (rx_conf->offloads & DEV_RX_OFFLOAD_TCP_LRO))
		rfctl &= ~TXGBE_PSRCTL_RSCDIA;
	else
		rfctl |= TXGBE_PSRCTL_RSCDIA;
	wr32(hw, TXGBE_PSRCTL, rfctl);

	/* If LRO hasn't been requested - we are done here. */
	if (!(rx_conf->offloads & DEV_RX_OFFLOAD_TCP_LRO))
		return 0;

	/* Set PSRCTL.RSCACK bit */
	rdrxctl = rd32(hw, TXGBE_PSRCTL);
	rdrxctl |= TXGBE_PSRCTL_RSCACK;
	wr32(hw, TXGBE_PSRCTL, rdrxctl);

	/* Per-queue RSC configuration */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct txgbe_rx_queue *rxq = dev->data->rx_queues[i];
		uint32_t srrctl =
			rd32(hw, TXGBE_RXCFG(rxq->reg_idx));
		uint32_t psrtype =
			rd32(hw, TXGBE_POOLRSS(rxq->reg_idx));
		uint32_t eitr =
			rd32(hw, TXGBE_ITR(rxq->reg_idx));

		/*
		 * txgbe PMD doesn't support header-split at the moment.
		 */
		srrctl &= ~TXGBE_RXCFG_HDRLEN_MASK;
		srrctl |= TXGBE_RXCFG_HDRLEN(128);

		/*
		 * TODO: Consider setting the Receive Descriptor Minimum
		 * Threshold Size for an RSC case. This is not an obviously
		 * beneficiary option but the one worth considering...
		 */

		srrctl |= TXGBE_RXCFG_RSCENA;
		srrctl &= ~TXGBE_RXCFG_RSCMAX_MASK;
		srrctl |= txgbe_get_rscctl_maxdesc(rxq->mb_pool);
		psrtype |= TXGBE_POOLRSS_L4HDR;

		/*
		 * RSC: Set ITR interval corresponding to 2K ints/s.
		 *
		 * Full-sized RSC aggregations for a 10Gb/s link will
		 * arrive at about 20K aggregation/s rate.
		 *
		 * 2K inst/s rate will make only 10% of the
		 * aggregations to be closed due to the interrupt timer
		 * expiration for a streaming at wire-speed case.
		 *
		 * For a sparse streaming case this setting will yield
		 * at most 500us latency for a single RSC aggregation.
		 */
		eitr &= ~TXGBE_ITR_IVAL_MASK;
		eitr |= TXGBE_ITR_IVAL_10G(TXGBE_QUEUE_ITR_INTERVAL_DEFAULT);
		eitr |= TXGBE_ITR_WRDSA;

		wr32(hw, TXGBE_RXCFG(rxq->reg_idx), srrctl);
		wr32(hw, TXGBE_POOLRSS(rxq->reg_idx), psrtype);
		wr32(hw, TXGBE_ITR(rxq->reg_idx), eitr);

		/*
		 * RSC requires the mapping of the queue to the
		 * interrupt vector.
		 */
		txgbe_set_ivar_map(hw, 0, rxq->reg_idx, i);
	}

	dev->data->lro = 1;

	PMD_INIT_LOG(DEBUG, "enabling LRO mode");

	return 0;
}

/*
 * Initializes Receive Unit.
 */
int __rte_cold
txgbe_dev_rx_init(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw;
	struct txgbe_rx_queue *rxq;
	uint64_t bus_addr;
	uint32_t fctrl;
	uint32_t hlreg0;
	uint32_t srrctl;
	uint32_t rdrxctl;
	uint32_t rxcsum;
	uint16_t buf_size;
	uint16_t i;
	struct rte_eth_rxmode *rx_conf = &dev->data->dev_conf.rxmode;
	int rc;

	PMD_INIT_FUNC_TRACE();
	hw = TXGBE_DEV_HW(dev);

	/*
	 * Make sure receives are disabled while setting
	 * up the RX context (registers, descriptor rings, etc.).
	 */
	wr32m(hw, TXGBE_MACRXCFG, TXGBE_MACRXCFG_ENA, 0);
	wr32m(hw, TXGBE_PBRXCTL, TXGBE_PBRXCTL_ENA, 0);

	/* Enable receipt of broadcasted frames */
	fctrl = rd32(hw, TXGBE_PSRCTL);
	fctrl |= TXGBE_PSRCTL_BCA;
	wr32(hw, TXGBE_PSRCTL, fctrl);

	/*
	 * Configure CRC stripping, if any.
	 */
	hlreg0 = rd32(hw, TXGBE_SECRXCTL);
	if (rx_conf->offloads & DEV_RX_OFFLOAD_KEEP_CRC)
		hlreg0 &= ~TXGBE_SECRXCTL_CRCSTRIP;
	else
		hlreg0 |= TXGBE_SECRXCTL_CRCSTRIP;
	wr32(hw, TXGBE_SECRXCTL, hlreg0);

	/*
	 * Configure jumbo frame support, if any.
	 */
	if (rx_conf->offloads & DEV_RX_OFFLOAD_JUMBO_FRAME) {
		wr32m(hw, TXGBE_FRMSZ, TXGBE_FRMSZ_MAX_MASK,
			TXGBE_FRMSZ_MAX(rx_conf->max_rx_pkt_len));
	} else {
		wr32m(hw, TXGBE_FRMSZ, TXGBE_FRMSZ_MAX_MASK,
			TXGBE_FRMSZ_MAX(TXGBE_FRAME_SIZE_DFT));
	}

	/*
	 * If loopback mode is configured, set LPBK bit.
	 */
	hlreg0 = rd32(hw, TXGBE_PSRCTL);
	if (hw->mac.type == txgbe_mac_raptor &&
	    dev->data->dev_conf.lpbk_mode)
		hlreg0 |= TXGBE_PSRCTL_LBENA;
	else
		hlreg0 &= ~TXGBE_PSRCTL_LBENA;

	wr32(hw, TXGBE_PSRCTL, hlreg0);

	/*
	 * Assume no header split and no VLAN strip support
	 * on any Rx queue first .
	 */
	rx_conf->offloads &= ~DEV_RX_OFFLOAD_VLAN_STRIP;

	/* Setup RX queues */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		/*
		 * Reset crc_len in case it was changed after queue setup by a
		 * call to configure.
		 */
		if (rx_conf->offloads & DEV_RX_OFFLOAD_KEEP_CRC)
			rxq->crc_len = RTE_ETHER_CRC_LEN;
		else
			rxq->crc_len = 0;

		/* Setup the Base and Length of the Rx Descriptor Rings */
		bus_addr = rxq->rx_ring_phys_addr;
		wr32(hw, TXGBE_RXBAL(rxq->reg_idx),
				(uint32_t)(bus_addr & BIT_MASK32));
		wr32(hw, TXGBE_RXBAH(rxq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		wr32(hw, TXGBE_RXRP(rxq->reg_idx), 0);
		wr32(hw, TXGBE_RXWP(rxq->reg_idx), 0);

		srrctl = TXGBE_RXCFG_RNGLEN(rxq->nb_rx_desc);

		/* Set if packets are dropped when no descriptors available */
		if (rxq->drop_en)
			srrctl |= TXGBE_RXCFG_DROP;

		/*
		 * Configure the RX buffer size in the PKTLEN field of
		 * the RXCFG register of the queue.
		 * The value is in 1 KB resolution. Valid values can be from
		 * 1 KB to 16 KB.
		 */
		buf_size = (uint16_t)(rte_pktmbuf_data_room_size(rxq->mb_pool) -
			RTE_PKTMBUF_HEADROOM);
		buf_size = ROUND_UP(buf_size, 0x1 << 10);
		srrctl |= TXGBE_RXCFG_PKTLEN(buf_size);

		wr32(hw, TXGBE_RXCFG(rxq->reg_idx), srrctl);

		/* It adds dual VLAN length for supporting dual VLAN */
		if (dev->data->dev_conf.rxmode.max_rx_pkt_len +
					    2 * TXGBE_VLAN_TAG_SIZE > buf_size)
			dev->data->scattered_rx = 1;
		if (rxq->offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
			rx_conf->offloads |= DEV_RX_OFFLOAD_VLAN_STRIP;
	}

	if (rx_conf->offloads & DEV_RX_OFFLOAD_SCATTER)
		dev->data->scattered_rx = 1;

	/*
	 * Setup the Checksum Register.
	 * Disable Full-Packet Checksum which is mutually exclusive with RSS.
	 * Enable IP/L4 checksum computation by hardware if requested to do so.
	 */
	rxcsum = rd32(hw, TXGBE_PSRCTL);
	rxcsum |= TXGBE_PSRCTL_PCSD;
	if (rx_conf->offloads & DEV_RX_OFFLOAD_CHECKSUM)
		rxcsum |= TXGBE_PSRCTL_L4CSUM;
	else
		rxcsum &= ~TXGBE_PSRCTL_L4CSUM;

	wr32(hw, TXGBE_PSRCTL, rxcsum);

	if (hw->mac.type == txgbe_mac_raptor) {
		rdrxctl = rd32(hw, TXGBE_SECRXCTL);
		if (rx_conf->offloads & DEV_RX_OFFLOAD_KEEP_CRC)
			rdrxctl &= ~TXGBE_SECRXCTL_CRCSTRIP;
		else
			rdrxctl |= TXGBE_SECRXCTL_CRCSTRIP;
		wr32(hw, TXGBE_SECRXCTL, rdrxctl);
	}

	rc = txgbe_set_rsc(dev);
	if (rc)
		return rc;

	txgbe_set_rx_function(dev);

	return 0;
}

/*
 * Initializes Transmit Unit.
 */
void __rte_cold
txgbe_dev_tx_init(struct rte_eth_dev *dev)
{
	struct txgbe_hw     *hw;
	struct txgbe_tx_queue *txq;
	uint64_t bus_addr;
	uint16_t i;

	PMD_INIT_FUNC_TRACE();
	hw = TXGBE_DEV_HW(dev);

	/* Setup the Base and Length of the Tx Descriptor Rings */
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];

		bus_addr = txq->tx_ring_phys_addr;
		wr32(hw, TXGBE_TXBAL(txq->reg_idx),
				(uint32_t)(bus_addr & BIT_MASK32));
		wr32(hw, TXGBE_TXBAH(txq->reg_idx),
				(uint32_t)(bus_addr >> 32));
		wr32m(hw, TXGBE_TXCFG(txq->reg_idx), TXGBE_TXCFG_BUFLEN_MASK,
			TXGBE_TXCFG_BUFLEN(txq->nb_tx_desc));
		/* Setup the HW Tx Head and TX Tail descriptor pointers */
		wr32(hw, TXGBE_TXRP(txq->reg_idx), 0);
		wr32(hw, TXGBE_TXWP(txq->reg_idx), 0);
	}
}

void
txgbe_dev_save_rx_queue(struct txgbe_hw *hw, uint16_t rx_queue_id)
{
	u32 *reg = &hw->q_rx_regs[rx_queue_id * 8];
	*(reg++) = rd32(hw, TXGBE_RXBAL(rx_queue_id));
	*(reg++) = rd32(hw, TXGBE_RXBAH(rx_queue_id));
	*(reg++) = rd32(hw, TXGBE_RXCFG(rx_queue_id));
}

void
txgbe_dev_store_rx_queue(struct txgbe_hw *hw, uint16_t rx_queue_id)
{
	u32 *reg = &hw->q_rx_regs[rx_queue_id * 8];
	wr32(hw, TXGBE_RXBAL(rx_queue_id), *(reg++));
	wr32(hw, TXGBE_RXBAH(rx_queue_id), *(reg++));
	wr32(hw, TXGBE_RXCFG(rx_queue_id), *(reg++) & ~TXGBE_RXCFG_ENA);
}

void
txgbe_dev_save_tx_queue(struct txgbe_hw *hw, uint16_t tx_queue_id)
{
	u32 *reg = &hw->q_tx_regs[tx_queue_id * 8];
	*(reg++) = rd32(hw, TXGBE_TXBAL(tx_queue_id));
	*(reg++) = rd32(hw, TXGBE_TXBAH(tx_queue_id));
	*(reg++) = rd32(hw, TXGBE_TXCFG(tx_queue_id));
}

void
txgbe_dev_store_tx_queue(struct txgbe_hw *hw, uint16_t tx_queue_id)
{
	u32 *reg = &hw->q_tx_regs[tx_queue_id * 8];
	wr32(hw, TXGBE_TXBAL(tx_queue_id), *(reg++));
	wr32(hw, TXGBE_TXBAH(tx_queue_id), *(reg++));
	wr32(hw, TXGBE_TXCFG(tx_queue_id), *(reg++) & ~TXGBE_TXCFG_ENA);
}

/*
 * Start Receive Units for specified queue.
 */
int __rte_cold
txgbe_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_rx_queue *rxq;
	uint32_t rxdctl;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();

	rxq = dev->data->rx_queues[rx_queue_id];

	/* Allocate buffers for descriptor rings */
	if (txgbe_alloc_rx_queue_mbufs(rxq) != 0) {
		PMD_INIT_LOG(ERR, "Could not alloc mbuf for queue:%d",
			     rx_queue_id);
		return -1;
	}
	rxdctl = rd32(hw, TXGBE_RXCFG(rxq->reg_idx));
	rxdctl |= TXGBE_RXCFG_ENA;
	wr32(hw, TXGBE_RXCFG(rxq->reg_idx), rxdctl);

	/* Wait until RX Enable ready */
	poll_ms = RTE_TXGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_ms(1);
		rxdctl = rd32(hw, TXGBE_RXCFG(rxq->reg_idx));
	} while (--poll_ms && !(rxdctl & TXGBE_RXCFG_ENA));
	if (!poll_ms)
		PMD_INIT_LOG(ERR, "Could not enable Rx Queue %d", rx_queue_id);
	rte_wmb();
	wr32(hw, TXGBE_RXRP(rxq->reg_idx), 0);
	wr32(hw, TXGBE_RXWP(rxq->reg_idx), rxq->nb_rx_desc - 1);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

/*
 * Stop Receive Units for specified queue.
 */
int __rte_cold
txgbe_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);
	struct txgbe_rx_queue *rxq;
	uint32_t rxdctl;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();

	rxq = dev->data->rx_queues[rx_queue_id];

	txgbe_dev_save_rx_queue(hw, rxq->reg_idx);
	wr32m(hw, TXGBE_RXCFG(rxq->reg_idx), TXGBE_RXCFG_ENA, 0);

	/* Wait until RX Enable bit clear */
	poll_ms = RTE_TXGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_ms(1);
		rxdctl = rd32(hw, TXGBE_RXCFG(rxq->reg_idx));
	} while (--poll_ms && (rxdctl & TXGBE_RXCFG_ENA));
	if (!poll_ms)
		PMD_INIT_LOG(ERR, "Could not disable Rx Queue %d", rx_queue_id);

	rte_delay_us(RTE_TXGBE_WAIT_100_US);
	txgbe_dev_store_rx_queue(hw, rxq->reg_idx);

	txgbe_rx_queue_release_mbufs(rxq);
	txgbe_reset_rx_queue(adapter, rxq);
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

/*
 * Start Transmit Units for specified queue.
 */
int __rte_cold
txgbe_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_tx_queue *txq;
	uint32_t txdctl;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();

	txq = dev->data->tx_queues[tx_queue_id];
	wr32m(hw, TXGBE_TXCFG(txq->reg_idx), TXGBE_TXCFG_ENA, TXGBE_TXCFG_ENA);

	/* Wait until TX Enable ready */
	poll_ms = RTE_TXGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_ms(1);
		txdctl = rd32(hw, TXGBE_TXCFG(txq->reg_idx));
	} while (--poll_ms && !(txdctl & TXGBE_TXCFG_ENA));
	if (!poll_ms)
		PMD_INIT_LOG(ERR, "Could not enable "
			     "Tx Queue %d", tx_queue_id);

	rte_wmb();
	wr32(hw, TXGBE_TXWP(txq->reg_idx), txq->tx_tail);
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

/*
 * Stop Transmit Units for specified queue.
 */
int __rte_cold
txgbe_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_tx_queue *txq;
	uint32_t txdctl;
	uint32_t txtdh, txtdt;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();

	txq = dev->data->tx_queues[tx_queue_id];

	/* Wait until TX queue is empty */
	poll_ms = RTE_TXGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_us(RTE_TXGBE_WAIT_100_US);
		txtdh = rd32(hw, TXGBE_TXRP(txq->reg_idx));
		txtdt = rd32(hw, TXGBE_TXWP(txq->reg_idx));
	} while (--poll_ms && (txtdh != txtdt));
	if (!poll_ms)
		PMD_INIT_LOG(ERR,
			"Tx Queue %d is not empty when stopping.",
			tx_queue_id);

	txgbe_dev_save_tx_queue(hw, txq->reg_idx);
	wr32m(hw, TXGBE_TXCFG(txq->reg_idx), TXGBE_TXCFG_ENA, 0);

	/* Wait until TX Enable bit clear */
	poll_ms = RTE_TXGBE_REGISTER_POLL_WAIT_10_MS;
	do {
		rte_delay_ms(1);
		txdctl = rd32(hw, TXGBE_TXCFG(txq->reg_idx));
	} while (--poll_ms && (txdctl & TXGBE_TXCFG_ENA));
	if (!poll_ms)
		PMD_INIT_LOG(ERR, "Could not disable Tx Queue %d",
			tx_queue_id);

	rte_delay_us(RTE_TXGBE_WAIT_100_US);
	txgbe_dev_store_tx_queue(hw, txq->reg_idx);

	if (txq->ops != NULL) {
		txq->ops->release_mbufs(txq);
		txq->ops->reset(txq);
	}
	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

