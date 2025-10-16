/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
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
#include <rte_ethdev.h>
#include <ethdev_driver.h>
#include <rte_security_driver.h>
#include <rte_memzone.h>
#include <rte_atomic.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_prefetch.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_string_fns.h>
#include <rte_errno.h>
#include <rte_ip.h>
#include <rte_net.h>
#include <rte_vect.h>

#include "txgbe_logs.h"
#include "base/txgbe.h"
#include "txgbe_ethdev.h"
#include "txgbe_rxtx.h"

#ifdef RTE_LIBRTE_IEEE1588
#define TXGBE_TX_IEEE1588_TMST RTE_MBUF_F_TX_IEEE1588_TMST
#else
#define TXGBE_TX_IEEE1588_TMST 0
#endif

/* Bit Mask to indicate what bits required for building TX context */
static const u64 TXGBE_TX_OFFLOAD_MASK = (RTE_MBUF_F_TX_IP_CKSUM |
		RTE_MBUF_F_TX_OUTER_IPV6 |
		RTE_MBUF_F_TX_OUTER_IPV4 |
		RTE_MBUF_F_TX_IPV6 |
		RTE_MBUF_F_TX_IPV4 |
		RTE_MBUF_F_TX_VLAN |
		RTE_MBUF_F_TX_L4_MASK |
		RTE_MBUF_F_TX_TCP_SEG |
		RTE_MBUF_F_TX_TUNNEL_MASK |
		RTE_MBUF_F_TX_OUTER_IP_CKSUM |
		RTE_MBUF_F_TX_OUTER_UDP_CKSUM |
#ifdef RTE_LIB_SECURITY
		RTE_MBUF_F_TX_SEC_OFFLOAD |
#endif
		TXGBE_TX_IEEE1588_TMST);

#define TXGBE_TX_OFFLOAD_NOTSUP_MASK \
		(RTE_MBUF_F_TX_OFFLOAD_MASK ^ TXGBE_TX_OFFLOAD_MASK)

/*
 * Prefetch a cache line into all cache levels.
 */
#define rte_txgbe_prefetch(p)   rte_prefetch0(p)

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

	if (txq->headwb_mem) {
		uint16_t tx_last_dd = txq->nb_tx_desc +
				      txq->tx_next_dd - txq->tx_free_thresh;
		if (tx_last_dd >= txq->nb_tx_desc)
			tx_last_dd -= txq->nb_tx_desc;

		volatile uint16_t head = (uint16_t)*txq->headwb_mem;

		if (txq->tx_next_dd > head && head > tx_last_dd)
			return 0;
		else if (tx_last_dd > txq->tx_next_dd &&
				(head > tx_last_dd || head < txq->tx_next_dd))
			return 0;
	} else {
		/* check DD bit on threshold descriptor */
		status = txq->tx_ring[txq->tx_next_dd].dw3;
		if (!(status & rte_cpu_to_le_32(TXGBE_TXD_DD))) {
			if (txq->nb_tx_free >> 1 < txq->tx_free_thresh)
				txgbe_set32_masked(txq->tdc_reg_addr,
					TXGBE_TXCFG_FLUSH, TXGBE_TXCFG_FLUSH);
			return 0;
	}
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
		if (pkt_len < RTE_ETHER_HDR_LEN)
			pkt_len = TXGBE_FRAME_SIZE_DFT;

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
	if (pkt_len < RTE_ETHER_HDR_LEN)
		pkt_len = TXGBE_FRAME_SIZE_DFT;

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

static uint16_t
txgbe_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
		    uint16_t nb_pkts)
{
	struct txgbe_tx_queue *txq = (struct txgbe_tx_queue *)tx_queue;
	uint16_t nb_tx = 0;

	while (nb_pkts) {
		uint16_t ret, num;

		num = (uint16_t)RTE_MIN(nb_pkts, txq->tx_free_thresh);
		ret = txgbe_xmit_fixed_burst_vec(tx_queue, &tx_pkts[nb_tx], num);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}

static inline void
txgbe_set_xmit_ctx(struct txgbe_tx_queue *txq,
		volatile struct txgbe_tx_ctx_desc *ctx_txd,
		uint64_t ol_flags, union txgbe_tx_offload tx_offload,
		__rte_unused uint64_t *mdata)
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
	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		tx_offload_mask.l2_len |= ~0;
		tx_offload_mask.l3_len |= ~0;
		tx_offload_mask.l4_len |= ~0;
		tx_offload_mask.tso_segsz |= ~0;
		mss_l4len_idx |= TXGBE_TXD_MSS(tx_offload.tso_segsz);
		mss_l4len_idx |= TXGBE_TXD_L4LEN(tx_offload.l4_len);
	} else { /* no TSO, check if hardware checksum is needed */
		if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM) {
			tx_offload_mask.l2_len |= ~0;
			tx_offload_mask.l3_len |= ~0;
		}

		switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
		case RTE_MBUF_F_TX_UDP_CKSUM:
			mss_l4len_idx |=
				TXGBE_TXD_L4LEN(sizeof(struct rte_udp_hdr));
			tx_offload_mask.l2_len |= ~0;
			tx_offload_mask.l3_len |= ~0;
			break;
		case RTE_MBUF_F_TX_TCP_CKSUM:
			mss_l4len_idx |=
				TXGBE_TXD_L4LEN(sizeof(struct rte_tcp_hdr));
			tx_offload_mask.l2_len |= ~0;
			tx_offload_mask.l3_len |= ~0;
			break;
		case RTE_MBUF_F_TX_SCTP_CKSUM:
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

	if (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
		tx_offload_mask.outer_tun_len |= ~0;
		tx_offload_mask.outer_l2_len |= ~0;
		tx_offload_mask.outer_l3_len |= ~0;
		tx_offload_mask.l2_len |= ~0;
		tunnel_seed = TXGBE_TXD_ETUNLEN(tx_offload.outer_tun_len >> 1);
		tunnel_seed |= TXGBE_TXD_EIPLEN(tx_offload.outer_l3_len >> 2);

		switch (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
		case RTE_MBUF_F_TX_TUNNEL_IPIP:
			/* for non UDP / GRE tunneling, set to 0b */
			break;
		case RTE_MBUF_F_TX_TUNNEL_VXLAN:
		case RTE_MBUF_F_TX_TUNNEL_VXLAN_GPE:
		case RTE_MBUF_F_TX_TUNNEL_GENEVE:
			tunnel_seed |= TXGBE_TXD_ETYPE_UDP;
			break;
		case RTE_MBUF_F_TX_TUNNEL_GRE:
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

	if (ol_flags & RTE_MBUF_F_TX_VLAN) {
		tx_offload_mask.vlan_tci |= ~0;
		vlan_macip_lens |= TXGBE_TXD_VLAN(tx_offload.vlan_tci);
	}

#ifdef RTE_LIB_SECURITY
	if (ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD) {
		union txgbe_crypto_tx_desc_md *md =
				(union txgbe_crypto_tx_desc_md *)mdata;
		tunnel_seed |= TXGBE_TXD_IPSEC_SAIDX(md->sa_idx);
		type_tucmd_mlhl |= md->enc ?
			(TXGBE_TXD_IPSEC_ESP | TXGBE_TXD_IPSEC_ESPENC) : 0;
		type_tucmd_mlhl |= TXGBE_TXD_IPSEC_ESPLEN(md->pad_len);
		tx_offload_mask.sa_idx |= ~0;
		tx_offload_mask.sec_pad_len |= ~0;
	}
#endif

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

	if ((ol_flags & RTE_MBUF_F_TX_L4_MASK) != RTE_MBUF_F_TX_L4_NO_CKSUM) {
		tmp |= TXGBE_TXD_CC;
		tmp |= TXGBE_TXD_L4CS;
	}
	if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM) {
		tmp |= TXGBE_TXD_CC;
		tmp |= TXGBE_TXD_IPCS;
	}
	if (ol_flags & RTE_MBUF_F_TX_OUTER_IP_CKSUM) {
		tmp |= TXGBE_TXD_CC;
		tmp |= TXGBE_TXD_EIPCS;
	}
	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		tmp |= TXGBE_TXD_CC;
		/* implies IPv4 cksum */
		if (ol_flags & RTE_MBUF_F_TX_IPV4)
			tmp |= TXGBE_TXD_IPCS;
		tmp |= TXGBE_TXD_L4CS;
	}
	if (ol_flags & RTE_MBUF_F_TX_VLAN)
		tmp |= TXGBE_TXD_CC;

	return tmp;
}

static inline uint32_t
tx_desc_ol_flags_to_cmdtype(uint64_t ol_flags)
{
	uint32_t cmdtype = 0;

	if (ol_flags & RTE_MBUF_F_TX_VLAN)
		cmdtype |= TXGBE_TXD_VLE;
	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
		cmdtype |= TXGBE_TXD_TSE;
	if (ol_flags & RTE_MBUF_F_TX_MACSEC)
		cmdtype |= TXGBE_TXD_LINKSEC;
	return cmdtype;
}

static inline uint32_t
tx_desc_ol_flags_to_ptype(uint64_t oflags)
{
	uint32_t ptype;
	bool tun;

	/* Only support flags in TXGBE_TX_OFFLOAD_MASK */
	tun = !!(oflags & RTE_MBUF_F_TX_TUNNEL_MASK);

	/* L2 level */
	ptype = RTE_PTYPE_L2_ETHER;
	if (oflags & RTE_MBUF_F_TX_VLAN)
		ptype |= (tun ? RTE_PTYPE_INNER_L2_ETHER_VLAN : RTE_PTYPE_L2_ETHER_VLAN);

	if (oflags & RTE_MBUF_F_TX_QINQ) /* tunnel + QINQ is not supported */
		ptype |= RTE_PTYPE_L2_ETHER_VLAN;

	/* L3 level */
	if (oflags & (RTE_MBUF_F_TX_OUTER_IPV4 | RTE_MBUF_F_TX_OUTER_IP_CKSUM))
		ptype |= RTE_PTYPE_L3_IPV4;
	else if (oflags & (RTE_MBUF_F_TX_OUTER_IPV6))
		ptype |= RTE_PTYPE_L3_IPV6;

	if (oflags & (RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM))
		ptype |= (tun ? RTE_PTYPE_INNER_L3_IPV4 : RTE_PTYPE_L3_IPV4);
	else if (oflags & (RTE_MBUF_F_TX_IPV6))
		ptype |= (tun ? RTE_PTYPE_INNER_L3_IPV6 : RTE_PTYPE_L3_IPV6);

	/* L4 level */
	switch (oflags & (RTE_MBUF_F_TX_L4_MASK)) {
	case RTE_MBUF_F_TX_TCP_CKSUM:
		ptype |= (tun ? RTE_PTYPE_INNER_L4_TCP : RTE_PTYPE_L4_TCP);
		break;
	case RTE_MBUF_F_TX_UDP_CKSUM:
		ptype |= (tun ? RTE_PTYPE_INNER_L4_UDP : RTE_PTYPE_L4_UDP);
		break;
	case RTE_MBUF_F_TX_SCTP_CKSUM:
		ptype |= (tun ? RTE_PTYPE_INNER_L4_SCTP : RTE_PTYPE_L4_SCTP);
		break;
	}

	if (oflags & RTE_MBUF_F_TX_TCP_SEG)
		ptype |= (tun ? RTE_PTYPE_INNER_L4_TCP : RTE_PTYPE_L4_TCP);

	/* Tunnel */
	switch (oflags & RTE_MBUF_F_TX_TUNNEL_MASK) {
	case RTE_MBUF_F_TX_TUNNEL_VXLAN:
	case RTE_MBUF_F_TX_TUNNEL_VXLAN_GPE:
		ptype |= RTE_PTYPE_TUNNEL_GRENAT;
		break;
	case RTE_MBUF_F_TX_TUNNEL_GRE:
		ptype |= RTE_PTYPE_TUNNEL_GRE;
		break;
	case RTE_MBUF_F_TX_TUNNEL_GENEVE:
		ptype |= RTE_PTYPE_TUNNEL_GENEVE;
		break;
	case RTE_MBUF_F_TX_TUNNEL_IPIP:
	case RTE_MBUF_F_TX_TUNNEL_IP:
		ptype |= RTE_PTYPE_TUNNEL_IP;
		break;
	}

	return ptype;
}

static inline uint8_t
tx_desc_ol_flags_to_ptid(uint64_t oflags)
{
	uint32_t ptype;

	ptype = tx_desc_ol_flags_to_ptype(oflags);

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

	if (txq->headwb_mem) {
		u32 head = *txq->headwb_mem;

		PMD_TX_FREE_LOG(DEBUG, "queue[%02d]: headwb_mem = %03d, desc_to_clean_to = %03d",
				txq->reg_idx, head, desc_to_clean_to);
		/* we have caught up to head, no work left to do */
		if (desc_to_clean_to == head)
			return -(1);
	} else {
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

#define GRE_CHECKSUM_PRESENT	0x8000
#define GRE_KEY_PRESENT		0x2000
#define GRE_SEQUENCE_PRESENT	0x1000
#define GRE_EXT_LEN		4
#define GRE_SUPPORTED_FIELDS	(GRE_CHECKSUM_PRESENT | GRE_KEY_PRESENT |\
				 GRE_SEQUENCE_PRESENT)

static inline uint8_t
txgbe_get_tun_len(struct rte_mbuf *mbuf)
{
	struct txgbe_genevehdr genevehdr;
	const struct txgbe_genevehdr *gh;
	const struct txgbe_grehdr *grh;
	struct txgbe_grehdr grehdr;
	uint8_t tun_len;

	switch (mbuf->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
	case RTE_MBUF_F_TX_TUNNEL_IPIP:
		tun_len = 0;
		break;
	case RTE_MBUF_F_TX_TUNNEL_VXLAN:
	case RTE_MBUF_F_TX_TUNNEL_VXLAN_GPE:
		tun_len = sizeof(struct txgbe_udphdr)
			+ sizeof(struct txgbe_vxlanhdr);
		break;
	case RTE_MBUF_F_TX_TUNNEL_GRE:
		tun_len = sizeof(struct txgbe_grehdr);
		grh = rte_pktmbuf_read(mbuf,
			mbuf->outer_l2_len + mbuf->outer_l3_len,
			sizeof(grehdr), &grehdr);
		if (grh->flags & rte_cpu_to_be_16(GRE_SUPPORTED_FIELDS))
			tun_len += GRE_EXT_LEN;
		break;
	case RTE_MBUF_F_TX_TUNNEL_GENEVE:
		gh = rte_pktmbuf_read(mbuf, mbuf->outer_l2_len +
			mbuf->outer_l3_len + sizeof(struct txgbe_udphdr),
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

static inline void
txgbe_fix_offload_len(union txgbe_tx_offload *ol)
{
	uint8_t ptid = ol->ptid;

	if (ptid & TXGBE_PTID_PKT_TUN) {
		if (ol->outer_l2_len == 0)
			ol->outer_l2_len = sizeof(struct rte_ether_hdr);
		if (ol->outer_l3_len == 0) {
			if (ptid & TXGBE_PTID_TUN_IPV6)
				ol->outer_l3_len = sizeof(struct rte_ipv6_hdr);
			else
				ol->outer_l3_len = sizeof(struct rte_ipv4_hdr);
		}
		if ((ptid & 0xF) == 0) {
			ol->l3_len = 0;
			ol->l4_len = 0;
		} else {
			goto inner;
		}
	}

	if ((ptid & 0xF0) == TXGBE_PTID_PKT_MAC) {
		if (ol->l2_len == 0)
			ol->l2_len = sizeof(struct rte_ether_hdr);
		ol->l3_len = 0;
		ol->l4_len = 0;
	} else if ((ptid & 0xF0) == TXGBE_PTID_PKT_IP) {
		if (ol->l2_len == 0)
			ol->l2_len = sizeof(struct rte_ether_hdr);
inner:
		if (ol->l3_len == 0) {
			if (ptid & TXGBE_PTID_PKT_IPV6)
				ol->l3_len = sizeof(struct rte_ipv6_hdr);
			else
				ol->l3_len = sizeof(struct rte_ipv4_hdr);
		}
		switch (ptid & 0x7) {
		case 0x1:
		case 0x2:
			ol->l4_len = 0;
			break;
		case 0x3:
			if (ol->l4_len == 0)
				ol->l4_len =  sizeof(struct rte_udp_hdr);
			break;
		case 0x4:
			if (ol->l4_len == 0)
				ol->l4_len =  sizeof(struct rte_tcp_hdr);
			break;
		case 0x5:
			if (ol->l4_len == 0)
				ol->l4_len =  sizeof(struct rte_sctp_hdr);
			break;
		default:
			break;
		}
	}
}

static inline uint8_t
txgbe_parse_tun_ptid(struct rte_mbuf *tx_pkt, uint8_t tun_len)
{
	uint64_t inner_l2_len;
	uint8_t ptid = 0;

	inner_l2_len = tx_pkt->l2_len - tun_len;

	switch (inner_l2_len) {
	case 0:
		ptid = TXGBE_PTID_TUN_EIG;
		break;
	case sizeof(struct rte_ether_hdr):
		ptid = TXGBE_PTID_TUN_EIGM;
		break;
	case sizeof(struct rte_ether_hdr) + sizeof(struct rte_vlan_hdr):
		ptid = TXGBE_PTID_TUN_EIGMV;
		break;
	default:
		ptid = TXGBE_PTID_TUN_EI;
	}

	return ptid;
}

static inline bool
txgbe_check_pkt_err(struct rte_mbuf *tx_pkt)
{
	uint32_t total_len = 0, nb_seg = 0;
	struct rte_mbuf *mseg;

	mseg = tx_pkt;
	do {
		if (mseg->data_len == 0)
			return true;
		total_len += mseg->data_len;
		nb_seg++;
		mseg = mseg->next;
	} while (mseg != NULL);

	if (tx_pkt->pkt_len != total_len || tx_pkt->pkt_len == 0)
		return true;

	if (tx_pkt->nb_segs != nb_seg || tx_pkt->nb_segs > 64)
		return true;

	return false;
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
#ifdef RTE_LIB_SECURITY
	uint8_t use_ipsec;
#endif

	txq = tx_queue;
	if (txq->resetting)
		return 0;

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
		if (txgbe_check_pkt_err(tx_pkt)) {
			rte_pktmbuf_free(tx_pkt);
			txq->desc_error++;
			continue;
		}

		pkt_len = tx_pkt->pkt_len;

		/*
		 * Determine how many (if any) context descriptors
		 * are needed for offload functionality.
		 */
		ol_flags = tx_pkt->ol_flags;
#ifdef RTE_LIB_SECURITY
		use_ipsec = txq->using_ipsec && (ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD);
#endif

		/* If hardware offload required */
		tx_ol_req = ol_flags & TXGBE_TX_OFFLOAD_MASK;
		if (tx_ol_req) {
			tx_offload.ptid = tx_desc_ol_flags_to_ptid(tx_ol_req);
			tx_offload.l2_len = tx_pkt->l2_len;
			tx_offload.l3_len = tx_pkt->l3_len;
			tx_offload.l4_len = tx_pkt->l4_len;
			tx_offload.vlan_tci = tx_pkt->vlan_tci;
			tx_offload.tso_segsz = tx_pkt->tso_segsz;
			tx_offload.outer_l2_len = tx_pkt->outer_l2_len;
			tx_offload.outer_l3_len = tx_pkt->outer_l3_len;
			tx_offload.outer_tun_len = txgbe_get_tun_len(tx_pkt);
			if (tx_offload.ptid & TXGBE_PTID_PKT_TUN)
				tx_offload.ptid |= txgbe_parse_tun_ptid(tx_pkt,
							tx_offload.outer_tun_len);
			txgbe_fix_offload_len(&tx_offload);

#ifdef RTE_LIB_SECURITY
			if (use_ipsec) {
				union txgbe_crypto_tx_desc_md *ipsec_mdata =
					(union txgbe_crypto_tx_desc_md *)
						rte_security_dynfield(tx_pkt);
				tx_offload.sa_idx = ipsec_mdata->sa_idx;
				tx_offload.sec_pad_len = ipsec_mdata->pad_len;
			}
#endif

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

#ifdef RTE_LIBRTE_IEEE1588
		if (ol_flags & RTE_MBUF_F_TX_IEEE1588_TMST)
			cmd_type_len |= TXGBE_TXD_1588;
#endif

		olinfo_status = 0;
		if (tx_ol_req) {
			if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
				/* when TSO is on, paylen in descriptor is the
				 * not the packet len but the tcp payload len
				 */
				pkt_len -= (tx_offload.l2_len +
					tx_offload.l3_len + tx_offload.l4_len);
				pkt_len -=
					(tx_pkt->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK)
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
					tx_offload,
					rte_security_dynfield(tx_pkt));

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
#ifdef RTE_LIB_SECURITY
		if (use_ipsec)
			olinfo_status |= TXGBE_TXD_IPSEC;
#endif

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

/*********************************************************************
 *
 *  TX prep functions
 *
 **********************************************************************/
uint16_t
txgbe_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	int i, ret;
	uint64_t ol_flags;
	struct rte_mbuf *m;
	struct txgbe_tx_queue *txq = (struct txgbe_tx_queue *)tx_queue;

	for (i = 0; i < nb_pkts; i++) {
		m = tx_pkts[i];
		ol_flags = m->ol_flags;

		/**
		 * Check if packet meets requirements for number of segments
		 *
		 * NOTE: for txgbe it's always (40 - WTHRESH) for both TSO and
		 *       non-TSO
		 */

		if (m->nb_segs > TXGBE_TX_MAX_SEG - txq->wthresh) {
			rte_errno = -EINVAL;
			return i;
		}

		if (ol_flags & TXGBE_TX_OFFLOAD_NOTSUP_MASK) {
			rte_errno = -ENOTSUP;
			return i;
		}

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
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
 *  RX functions
 *
 **********************************************************************/
/* @note: fix txgbe_dev_supported_ptypes_get() if any change here. */
static inline uint32_t
txgbe_rxd_pkt_info_to_pkt_type(uint32_t pkt_info, uint16_t ptid_mask)
{
	uint16_t ptid = TXGBE_RXD_PTID(pkt_info);

	ptid &= ptid_mask;

	return txgbe_decode_ptype(ptid);
}

static inline uint64_t
txgbe_rxd_pkt_info_to_pkt_flags(uint32_t pkt_info)
{
	static alignas(RTE_CACHE_LINE_SIZE) uint64_t ip_rss_types_map[16] = {
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
	int etfid = txgbe_etflt_id(TXGBE_RXD_PTID(pkt_info));
	if (likely(-1 != etfid))
		return ip_pkt_etqf_map[etfid] |
		       ip_rss_types_map[TXGBE_RXD_RSSTYPE(pkt_info)];
	else
		return ip_rss_types_map[TXGBE_RXD_RSSTYPE(pkt_info)];
#else
	return ip_rss_types_map[TXGBE_RXD_RSSTYPE(pkt_info)];
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
	pkt_flags = (rx_status & TXGBE_RXD_STAT_VLAN &&
		     vlan_flags & RTE_MBUF_F_RX_VLAN_STRIPPED)
		    ? vlan_flags : 0;

#ifdef RTE_LIBRTE_IEEE1588
	if (rx_status & TXGBE_RXD_STAT_1588)
		pkt_flags = pkt_flags | RTE_MBUF_F_RX_IEEE1588_TMST;
#endif
	return pkt_flags;
}

static inline uint64_t
rx_desc_error_to_pkt_flags(uint32_t rx_status)
{
	uint64_t pkt_flags = 0;

	/* checksum offload can't be disabled */
	if (rx_status & TXGBE_RXD_STAT_IPCS) {
		pkt_flags |= (rx_status & TXGBE_RXD_ERR_IPCS
				? RTE_MBUF_F_RX_IP_CKSUM_BAD : RTE_MBUF_F_RX_IP_CKSUM_GOOD);
	}

	if (rx_status & TXGBE_RXD_STAT_L4CS) {
		pkt_flags |= (rx_status & TXGBE_RXD_ERR_L4CS
				? RTE_MBUF_F_RX_L4_CKSUM_BAD : RTE_MBUF_F_RX_L4_CKSUM_GOOD);
	}

	if (rx_status & TXGBE_RXD_STAT_EIPCS &&
	    rx_status & TXGBE_RXD_ERR_EIPCS) {
		pkt_flags |= RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD;
	}

#ifdef RTE_LIB_SECURITY
	if (rx_status & TXGBE_RXD_STAT_SECP) {
		pkt_flags |= RTE_MBUF_F_RX_SEC_OFFLOAD;
		if (rx_status & TXGBE_RXD_ERR_SECERR)
			pkt_flags |= RTE_MBUF_F_RX_SEC_OFFLOAD_FAILED;
	}
#endif

	return pkt_flags;
}

/*
 * LOOK_AHEAD defines how many desc statuses to check beyond the
 * current descriptor.
 * It must be a pound define for optimal performance.
 * Do not change the value of LOOK_AHEAD, as the txgbe_rx_scan_hw_ring
 * function only works with LOOK_AHEAD=8.
 */
#define LOOK_AHEAD 8
#if (LOOK_AHEAD != 8)
#error "PMD TXGBE: LOOK_AHEAD must be 8\n"
#endif
static inline int
txgbe_rx_scan_hw_ring(struct txgbe_rx_queue *rxq)
{
	volatile struct txgbe_rx_desc *rxdp;
	struct txgbe_rx_entry *rxep;
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
	if (!(status & rte_cpu_to_le_32(TXGBE_RXD_STAT_DD)))
		return 0;

	/*
	 * Scan LOOK_AHEAD descriptors at a time to determine which descriptors
	 * reference packets that are ready to be received.
	 */
	for (i = 0; i < RTE_PMD_TXGBE_RX_MAX_BURST;
	     i += LOOK_AHEAD, rxdp += LOOK_AHEAD, rxep += LOOK_AHEAD) {
		/* Read desc statuses backwards to avoid race condition */
		for (j = 0; j < LOOK_AHEAD; j++)
			s[j] = rte_le_to_cpu_32(rxdp[j].qw1.lo.status);

		rte_atomic_thread_fence(rte_memory_order_acquire);

		/* Compute how many status bits were set */
		for (nb_dd = 0; nb_dd < LOOK_AHEAD &&
				(s[nb_dd] & TXGBE_RXD_STAT_DD); nb_dd++)
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
				txgbe_rxd_pkt_info_to_pkt_flags(pkt_info[j]);
			mb->ol_flags = pkt_flags;
			mb->packet_type =
				txgbe_rxd_pkt_info_to_pkt_type(pkt_info[j],
				rxq->pkt_type_mask);

			if (likely(pkt_flags & RTE_MBUF_F_RX_RSS_HASH))
				mb->hash.rss =
					rte_le_to_cpu_32(rxdp[j].qw0.dw1);
			else if (pkt_flags & RTE_MBUF_F_RX_FDIR) {
				mb->hash.fdir.hash =
					rte_le_to_cpu_16(rxdp[j].qw0.hi.csum) &
					TXGBE_ATR_HASH_MASK;
				mb->hash.fdir.id =
					rte_le_to_cpu_16(rxdp[j].qw0.hi.ipid);
			}
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
txgbe_rx_alloc_bufs(struct txgbe_rx_queue *rxq, bool reset_mbuf)
{
	volatile struct txgbe_rx_desc *rxdp;
	struct txgbe_rx_entry *rxep;
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
		TXGBE_RXD_HDRADDR(&rxdp[i], 0);
		TXGBE_RXD_PKTADDR(&rxdp[i], dma_addr);
	}

	/* update state of internal queue structure */
	rxq->rx_free_trigger = rxq->rx_free_trigger + rxq->rx_free_thresh;
	if (rxq->rx_free_trigger >= rxq->nb_rx_desc)
		rxq->rx_free_trigger = rxq->rx_free_thresh - 1;

	/* no errors */
	return 0;
}

static inline uint16_t
txgbe_rx_fill_from_stage(struct txgbe_rx_queue *rxq, struct rte_mbuf **rx_pkts,
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
txgbe_rx_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	     uint16_t nb_pkts)
{
	struct txgbe_rx_queue *rxq = (struct txgbe_rx_queue *)rx_queue;
	struct rte_eth_dev *dev = &rte_eth_devices[rxq->port_id];
	uint16_t nb_rx = 0;

	/* Any previously recv'd pkts will be returned from the Rx stage */
	if (rxq->rx_nb_avail)
		return txgbe_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	/* Scan the H/W ring for packets to receive */
	nb_rx = (uint16_t)txgbe_rx_scan_hw_ring(rxq);

	/* update internal queue state */
	rxq->rx_next_avail = 0;
	rxq->rx_nb_avail = nb_rx;
	rxq->rx_tail = (uint16_t)(rxq->rx_tail + nb_rx);

	/* if required, allocate new buffers to replenish descriptors */
	if (rxq->rx_tail > rxq->rx_free_trigger) {
		uint16_t cur_free_trigger = rxq->rx_free_trigger;

		if (txgbe_rx_alloc_bufs(rxq, true) != 0) {
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
		txgbe_set32_relaxed(rxq->rdt_reg_addr, cur_free_trigger);
	}

	if (rxq->rx_tail >= rxq->nb_rx_desc)
		rxq->rx_tail = 0;

	/* received any packets this loop? */
	if (rxq->rx_nb_avail)
		return txgbe_rx_fill_from_stage(rxq, rx_pkts, nb_pkts);

	return 0;
}

/* split requests into chunks of size RTE_PMD_TXGBE_RX_MAX_BURST */
uint16_t
txgbe_recv_pkts_bulk_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts)
{
	uint16_t nb_rx;

	if (unlikely(nb_pkts == 0))
		return 0;

	if (likely(nb_pkts <= RTE_PMD_TXGBE_RX_MAX_BURST))
		return txgbe_rx_recv_pkts(rx_queue, rx_pkts, nb_pkts);

	/* request is relatively large, chunk it up */
	nb_rx = 0;
	while (nb_pkts) {
		uint16_t ret, n;

		n = (uint16_t)RTE_MIN(nb_pkts, RTE_PMD_TXGBE_RX_MAX_BURST);
		ret = txgbe_rx_recv_pkts(rx_queue, &rx_pkts[nb_rx], n);
		nb_rx = (uint16_t)(nb_rx + ret);
		nb_pkts = (uint16_t)(nb_pkts - ret);
		if (ret < n)
			break;
	}

	return nb_rx;
}

uint16_t
txgbe_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts)
{
	struct txgbe_rx_queue *rxq;
	volatile struct txgbe_rx_desc *rx_ring;
	volatile struct txgbe_rx_desc *rxdp;
	struct txgbe_rx_entry *sw_ring;
	struct txgbe_rx_entry *rxe;
	struct rte_mbuf *rxm;
	struct rte_mbuf *nmb;
	struct txgbe_rx_desc rxd;
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
		 *
		 * Meanwhile, to prevent the CPU from executing out of order, we
		 * need to use a proper memory barrier to ensure the memory
		 * ordering below.
		 */
		rxdp = &rx_ring[rx_id];
		staterr = rxdp->qw1.lo.status;
		if (!(staterr & rte_cpu_to_le_32(TXGBE_RXD_STAT_DD)))
			break;

		/*
		 * Use acquire fence to ensure that status_error which includes
		 * DD bit is loaded before loading of other descriptor words.
		 */
		rte_atomic_thread_fence(rte_memory_order_acquire);

		rxd = *rxdp;

		/*
		 * End of packet.
		 *
		 * If the TXGBE_RXD_STAT_EOP flag is not set, the RX packet
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
			   "ext_err_stat=0x%08x pkt_len=%u",
			   (uint16_t)rxq->port_id, (uint16_t)rxq->queue_id,
			   (uint16_t)rx_id, (uint32_t)staterr,
			   (uint16_t)rte_le_to_cpu_16(rxd.qw1.hi.len));

		nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (nmb == NULL) {
			PMD_RX_LOG(DEBUG, "RX mbuf alloc failed port_id=%u "
				   "queue_id=%u", (uint16_t)rxq->port_id,
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
		rte_txgbe_prefetch(sw_ring[rx_id].mbuf);

		/*
		 * When next RX descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 8 pointers
		 * to mbufs.
		 */
		if ((rx_id & 0x3) == 0) {
			rte_txgbe_prefetch(&rx_ring[rx_id]);
			rte_txgbe_prefetch(&sw_ring[rx_id]);
		}

		rxm = rxe->mbuf;
		rxe->mbuf = nmb;
		dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		TXGBE_RXD_HDRADDR(rxdp, 0);
		TXGBE_RXD_PKTADDR(rxdp, dma_addr);

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
		pkt_flags |= txgbe_rxd_pkt_info_to_pkt_flags(pkt_info);
		rxm->ol_flags = pkt_flags;
		rxm->packet_type = txgbe_rxd_pkt_info_to_pkt_type(pkt_info,
						       rxq->pkt_type_mask);

		if (likely(pkt_flags & RTE_MBUF_F_RX_RSS_HASH)) {
			rxm->hash.rss = rte_le_to_cpu_32(rxd.qw0.dw1);
		} else if (pkt_flags & RTE_MBUF_F_RX_FDIR) {
			rxm->hash.fdir.hash =
				rte_le_to_cpu_16(rxd.qw0.hi.csum) &
				TXGBE_ATR_HASH_MASK;
			rxm->hash.fdir.id = rte_le_to_cpu_16(rxd.qw0.hi.ipid);
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
	 * RDH register, which creates a "full" ring situation from the
	 * hardware point of view...
	 */
	nb_hold = (uint16_t)(nb_hold + rxq->nb_rx_hold);
	if (nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_tail=%u "
			   "nb_hold=%u nb_rx=%u",
			   (uint16_t)rxq->port_id, (uint16_t)rxq->queue_id,
			   (uint16_t)rx_id, (uint16_t)nb_hold,
			   (uint16_t)nb_rx);
		rx_id = (uint16_t)((rx_id == 0) ?
				(rxq->nb_rx_desc - 1) : (rx_id - 1));
		txgbe_set32(rxq->rdt_reg_addr, rx_id);
		nb_hold = 0;
	}
	rxq->nb_rx_hold = nb_hold;
	return nb_rx;
}

/**
 * txgbe_fill_cluster_head_buf - fill the first mbuf of the returned packet
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
txgbe_fill_cluster_head_buf(struct rte_mbuf *head, struct txgbe_rx_desc *desc,
		struct txgbe_rx_queue *rxq, uint32_t staterr)
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
	pkt_flags |= txgbe_rxd_pkt_info_to_pkt_flags(pkt_info);
	if (TXGBE_RXD_RSCCNT(desc->qw0.dw0))
		pkt_flags |= RTE_MBUF_F_RX_LRO;
	head->ol_flags = pkt_flags;
	head->packet_type = txgbe_rxd_pkt_info_to_pkt_type(pkt_info,
						rxq->pkt_type_mask);

	if (likely(pkt_flags & RTE_MBUF_F_RX_RSS_HASH)) {
		head->hash.rss = rte_le_to_cpu_32(desc->qw0.dw1);
	} else if (pkt_flags & RTE_MBUF_F_RX_FDIR) {
		head->hash.fdir.hash = rte_le_to_cpu_16(desc->qw0.hi.csum)
				& TXGBE_ATR_HASH_MASK;
		head->hash.fdir.id = rte_le_to_cpu_16(desc->qw0.hi.ipid);
	}
}

/**
 * txgbe_recv_pkts_lro - receive handler for and LRO case.
 *
 * @rx_queue Rx queue handle
 * @rx_pkts table of received packets
 * @nb_pkts size of rx_pkts table
 * @bulk_alloc if TRUE bulk allocation is used for a HW ring refilling
 *
 * Handles the Rx HW ring completions when RSC feature is configured. Uses an
 * additional ring of txgbe_rsc_entry's that will hold the relevant RSC info.
 *
 * We use the same logic as in Linux and in FreeBSD txgbe drivers:
 * 1) When non-EOP RSC completion arrives:
 *    a) Update the HEAD of the current RSC aggregation cluster with the new
 *       segment's data length.
 *    b) Set the "next" pointer of the current segment to point to the segment
 *       at the NEXTP index.
 *    c) Pass the HEAD of RSC aggregation cluster on to the next NEXTP entry
 *       in the sw_rsc_ring.
 * 2) When EOP arrives we just update the cluster's total length and offload
 *    flags and deliver the cluster up to the upper layers. In our case - put it
 *    in the rx_pkts table.
 *
 * Returns the number of received packets/clusters (according to the "bulk
 * receive" interface).
 */
static inline uint16_t
txgbe_recv_pkts_lro(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts,
		    bool bulk_alloc)
{
	struct txgbe_rx_queue *rxq = rx_queue;
	struct rte_eth_dev *dev = &rte_eth_devices[rxq->port_id];
	volatile struct txgbe_rx_desc *rx_ring = rxq->rx_ring;
	struct txgbe_rx_entry *sw_ring = rxq->sw_ring;
	struct txgbe_scattered_rx_entry *sw_sc_ring = rxq->sw_sc_ring;
	uint16_t rx_id = rxq->rx_tail;
	uint16_t nb_rx = 0;
	uint16_t nb_hold = rxq->nb_rx_hold;
	uint16_t prev_id = rxq->rx_tail;

	while (nb_rx < nb_pkts) {
		bool eop;
		struct txgbe_rx_entry *rxe;
		struct txgbe_scattered_rx_entry *sc_entry;
		struct txgbe_scattered_rx_entry *next_sc_entry = NULL;
		struct txgbe_rx_entry *next_rxe = NULL;
		struct rte_mbuf *first_seg;
		struct rte_mbuf *rxm;
		struct rte_mbuf *nmb = NULL;
		struct txgbe_rx_desc rxd;
		uint16_t data_len;
		uint16_t next_id;
		volatile struct txgbe_rx_desc *rxdp;
		uint32_t staterr;

next_desc:
		/*
		 * "Volatile" only prevents caching of the variable marked
		 * volatile. Most important, "volatile" cannot prevent the CPU
		 * from executing out of order. So, it is necessary to use a
		 * proper memory barrier to ensure the memory ordering below.
		 */
		rxdp = &rx_ring[rx_id];
		staterr = rte_le_to_cpu_32(rxdp->qw1.lo.status);

		if (!(staterr & TXGBE_RXD_STAT_DD))
			break;

		/*
		 * Use acquire fence to ensure that status_error which includes
		 * DD bit is loaded before loading of other descriptor words.
		 */
		rte_atomic_thread_fence(rte_memory_order_acquire);

		rxd = *rxdp;

		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_id=%u "
				  "staterr=0x%x data_len=%u",
			   rxq->port_id, rxq->queue_id, rx_id, staterr,
			   rte_le_to_cpu_16(rxd.qw1.hi.len));

		if (!bulk_alloc) {
			nmb = rte_mbuf_raw_alloc(rxq->mb_pool);
			if (nmb == NULL) {
				PMD_RX_LOG(DEBUG, "RX mbuf alloc failed "
						  "port_id=%u queue_id=%u",
					   rxq->port_id, rxq->queue_id);

				dev->data->rx_mbuf_alloc_failed++;
				break;
			}
		} else if (nb_hold > rxq->rx_free_thresh) {
			uint16_t next_rdt = rxq->rx_free_trigger;

			if (!txgbe_rx_alloc_bufs(rxq, false)) {
				rte_wmb();
				txgbe_set32_relaxed(rxq->rdt_reg_addr,
							    next_rdt);
				nb_hold -= rxq->rx_free_thresh;
			} else {
				PMD_RX_LOG(DEBUG, "RX bulk alloc failed "
						  "port_id=%u queue_id=%u",
					   rxq->port_id, rxq->queue_id);

				dev->data->rx_mbuf_alloc_failed++;
				break;
			}
		}

		nb_hold++;
		rxe = &sw_ring[rx_id];
		eop = staterr & TXGBE_RXD_STAT_EOP;

		next_id = rx_id + 1;
		if (next_id == rxq->nb_rx_desc)
			next_id = 0;

		/* Prefetch next mbuf while processing current one. */
		rte_txgbe_prefetch(sw_ring[next_id].mbuf);

		/*
		 * When next RX descriptor is on a cache-line boundary,
		 * prefetch the next 4 RX descriptors and the next 4 pointers
		 * to mbufs.
		 */
		if ((next_id & 0x3) == 0) {
			rte_txgbe_prefetch(&rx_ring[next_id]);
			rte_txgbe_prefetch(&sw_ring[next_id]);
		}

		rxm = rxe->mbuf;

		if (!bulk_alloc) {
			__le64 dma =
			  rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
			/*
			 * Update RX descriptor with the physical address of the
			 * new data buffer of the new allocated mbuf.
			 */
			rxe->mbuf = nmb;

			rxm->data_off = RTE_PKTMBUF_HEADROOM;
			TXGBE_RXD_HDRADDR(rxdp, 0);
			TXGBE_RXD_PKTADDR(rxdp, dma);
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
			/*
			 * Get next descriptor index:
			 *  - For RSC it's in the NEXTP field.
			 *  - For a scattered packet - it's just a following
			 *    descriptor.
			 */
			if (TXGBE_RXD_RSCCNT(rxd.qw0.dw0))
				nextp_id = TXGBE_RXD_NEXTP(staterr);
			else
				nextp_id = next_id;

			next_sc_entry = &sw_sc_ring[nextp_id];
			next_rxe = &sw_ring[nextp_id];
			rte_txgbe_prefetch(next_rxe);
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
		 * sw_sc_ring and continue to parse the RX ring.
		 */
		if (!eop && next_rxe) {
			rxm->next = next_rxe->mbuf;
			next_sc_entry->fbuf = first_seg;
			goto next_desc;
		}

		/* Initialize the first mbuf of the returned packet */
		txgbe_fill_cluster_head_buf(first_seg, &rxd, rxq, staterr);

		/*
		 * Deal with the case, when HW CRC srip is disabled.
		 * That can't happen when LRO is enabled, but still could
		 * happen for scattered RX mode.
		 */
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
	 * Record index of the next RX descriptor to probe.
	 */
	rxq->rx_tail = rx_id;

	/*
	 * If the number of free RX descriptors is greater than the RX free
	 * threshold of the queue, advance the Receive Descriptor Tail (RDT)
	 * register.
	 * Update the RDT with the value of the last processed RX descriptor
	 * minus 1, to guarantee that the RDT register is never equal to the
	 * RDH register, which creates a "full" ring situation from the
	 * hardware point of view...
	 */
	if (!bulk_alloc && nb_hold > rxq->rx_free_thresh) {
		PMD_RX_LOG(DEBUG, "port_id=%u queue_id=%u rx_tail=%u "
			   "nb_hold=%u nb_rx=%u",
			   rxq->port_id, rxq->queue_id, rx_id, nb_hold, nb_rx);

		rte_wmb();
		txgbe_set32_relaxed(rxq->rdt_reg_addr, prev_id);
		nb_hold = 0;
	}

	rxq->nb_rx_hold = nb_hold;
	return nb_rx;
}

uint16_t
txgbe_recv_pkts_lro_single_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
				 uint16_t nb_pkts)
{
	return txgbe_recv_pkts_lro(rx_queue, rx_pkts, nb_pkts, false);
}

uint16_t
txgbe_recv_pkts_lro_bulk_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
			       uint16_t nb_pkts)
{
	return txgbe_recv_pkts_lro(rx_queue, rx_pkts, nb_pkts, true);
}

uint64_t
txgbe_get_rx_queue_offloads(struct rte_eth_dev *dev __rte_unused)
{
	return RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
}

uint64_t
txgbe_get_rx_port_offloads(struct rte_eth_dev *dev)
{
	uint64_t offloads;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct rte_eth_dev_sriov *sriov = &RTE_ETH_DEV_SRIOV(dev);

	offloads = RTE_ETH_RX_OFFLOAD_IPV4_CKSUM  |
		   RTE_ETH_RX_OFFLOAD_UDP_CKSUM   |
		   RTE_ETH_RX_OFFLOAD_TCP_CKSUM   |
		   RTE_ETH_RX_OFFLOAD_KEEP_CRC    |
		   RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
		   RTE_ETH_RX_OFFLOAD_RSS_HASH |
		   RTE_ETH_RX_OFFLOAD_SCATTER;

	if (!txgbe_is_vf(hw))
		offloads |= (RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
			     RTE_ETH_RX_OFFLOAD_QINQ_STRIP |
			     RTE_ETH_RX_OFFLOAD_VLAN_EXTEND);

	/*
	 * RSC is only supported by PF devices in a non-SR-IOV
	 * mode.
	 */
	if (txgbe_is_pf(hw) && !sriov->active)
		offloads |= RTE_ETH_RX_OFFLOAD_TCP_LRO;

	if (txgbe_is_pf(hw))
		offloads |= RTE_ETH_RX_OFFLOAD_MACSEC_STRIP;

	offloads |= RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM;

#ifdef RTE_LIB_SECURITY
	if (dev->security_ctx)
		offloads |= RTE_ETH_RX_OFFLOAD_SECURITY;
#endif

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

static int
txgbe_tx_done_cleanup_full(struct txgbe_tx_queue *txq, uint32_t free_cnt)
{
	struct txgbe_tx_entry *swr_ring = txq->sw_ring;
	uint16_t i, tx_last, tx_id;
	uint16_t nb_tx_free_last;
	uint16_t nb_tx_to_clean;
	uint32_t pkt_cnt;

	/* Start free mbuf from the next of tx_tail */
	tx_last = txq->tx_tail;
	tx_id  = swr_ring[tx_last].next_id;

	if (txq->nb_tx_free == 0 && txgbe_xmit_cleanup(txq))
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
			if (txgbe_xmit_cleanup(txq))
				break;

			nb_tx_to_clean = txq->nb_tx_free - nb_tx_free_last;
			nb_tx_free_last = txq->nb_tx_free;
		}
	}

	return (int)pkt_cnt;
}

static int
txgbe_tx_done_cleanup_simple(struct txgbe_tx_queue *txq,
			uint32_t free_cnt)
{
	int i, n, cnt;

	if (free_cnt == 0 || free_cnt > txq->nb_tx_desc)
		free_cnt = txq->nb_tx_desc;

	cnt = free_cnt - free_cnt % txq->tx_free_thresh;

	for (i = 0; i < cnt; i += n) {
		if (txq->nb_tx_desc - txq->nb_tx_free < txq->tx_free_thresh)
			break;

		n = txgbe_tx_free_bufs(txq);

		if (n == 0)
			break;
	}

	return i;
}

int
txgbe_dev_tx_done_cleanup(void *tx_queue, uint32_t free_cnt)
{
	struct txgbe_tx_queue *txq = (struct txgbe_tx_queue *)tx_queue;
	if (txq->offloads == 0 &&
#ifdef RTE_LIB_SECURITY
		!(txq->using_ipsec) &&
#endif
		txq->tx_free_thresh >= RTE_PMD_TXGBE_TX_MAX_BURST)
		return txgbe_tx_done_cleanup_simple(txq, free_cnt);

	return txgbe_tx_done_cleanup_full(txq, free_cnt);
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
		rte_memzone_free(txq->mz);
		if (txq->headwb_mem)
			rte_memzone_free(txq->headwb);
		rte_free(txq);
	}
}

void __rte_cold
txgbe_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	txgbe_tx_queue_release(dev->data->tx_queues[qid]);
}

/* (Re)set dynamic txgbe_tx_queue fields to defaults */
static void __rte_cold
txgbe_reset_tx_queue(struct txgbe_tx_queue *txq)
{
	static const struct txgbe_tx_desc zeroed_desc = {0};
	struct txgbe_tx_entry *txe = txq->sw_ring;
	uint16_t prev, i;

	/* Zero out HW ring memory */
	for (i = 0; i < txq->nb_tx_desc; i++)
		txq->tx_ring[i] = zeroed_desc;

	/* Initialize SW ring entries */
	prev = (uint16_t)(txq->nb_tx_desc - 1);
	for (i = 0; i < txq->nb_tx_desc; i++) {
		volatile struct txgbe_tx_desc *txd = &txq->tx_ring[i];

		txd->dw3 = rte_cpu_to_le_32(TXGBE_TXD_DD);
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
		TXGBE_CTX_NUM * sizeof(struct txgbe_ctx_info));
}

static const struct txgbe_txq_ops def_txq_ops = {
	.release_mbufs = txgbe_tx_queue_release_mbufs,
	.free_swring = txgbe_tx_free_swring,
	.reset = txgbe_reset_tx_queue,
};

/* Takes an ethdev and a queue and sets up the tx function to be used based on
 * the queue parameters. Used in tx_queue_setup by primary process and then
 * in dev_init by secondary process when attaching to an existing ethdev.
 */
void __rte_cold
txgbe_set_tx_function(struct rte_eth_dev *dev, struct txgbe_tx_queue *txq)
{
	/* Use a simple Tx queue (no offloads, no multi segs) if possible */
	if (txq->offloads == 0 &&
#ifdef RTE_LIB_SECURITY
			!(txq->using_ipsec) &&
#endif
			txq->tx_free_thresh >= RTE_PMD_TXGBE_TX_MAX_BURST) {
		PMD_INIT_LOG(DEBUG, "Using simple tx code path");
		dev->tx_pkt_prepare = NULL;
		if (txq->tx_free_thresh <= RTE_TXGBE_TX_MAX_FREE_BUF_SZ &&
				rte_vect_get_max_simd_bitwidth() >= RTE_VECT_SIMD_128 &&
				(rte_eal_process_type() != RTE_PROC_PRIMARY ||
					txgbe_txq_vec_setup(txq) == 0)) {
			PMD_INIT_LOG(DEBUG, "Vector tx enabled.");
			dev->tx_pkt_burst = txgbe_xmit_pkts_vec;
		} else {
			dev->tx_pkt_burst = txgbe_xmit_pkts_simple;
		}
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
		dev->tx_pkt_prepare = txgbe_prep_pkts;
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
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM  |
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM   |
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM   |
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM  |
		RTE_ETH_TX_OFFLOAD_TCP_TSO     |
		RTE_ETH_TX_OFFLOAD_UDP_TSO	   |
		RTE_ETH_TX_OFFLOAD_UDP_TNL_TSO	|
		RTE_ETH_TX_OFFLOAD_IP_TNL_TSO	|
		RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO	|
		RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO	|
		RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO	|
		RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO	|
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	if (!txgbe_is_vf(hw))
		tx_offload_capa |= RTE_ETH_TX_OFFLOAD_QINQ_INSERT;

	tx_offload_capa |= RTE_ETH_TX_OFFLOAD_MACSEC_INSERT;

	tx_offload_capa |= RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM;

#ifdef RTE_LIB_SECURITY
	if (dev->security_ctx)
		tx_offload_capa |= RTE_ETH_TX_OFFLOAD_SECURITY;
#endif
	return tx_offload_capa;
}

static int
txgbe_setup_headwb_resources(struct rte_eth_dev *dev,
					void *tx_queue,
					unsigned int socket_id)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	const struct rte_memzone *headwb;
	struct txgbe_tx_queue *txq = tx_queue;
	u8 i, headwb_size = 0;

	if (hw->mac.type != txgbe_mac_aml && hw->mac.type != txgbe_mac_aml40) {
		txq->headwb_mem = NULL;
		return 0;
	}

	headwb_size = hw->devarg.tx_headwb_size;
	headwb = rte_eth_dma_zone_reserve(dev, "tx_headwb_mem", txq->queue_id,
			sizeof(u32) * headwb_size,
			TXGBE_ALIGN, socket_id);

	if (headwb == NULL) {
		DEBUGOUT("Fail to setup headwb resources: no mem");
		txgbe_tx_queue_release(txq);
		return -ENOMEM;
	}

	txq->headwb = headwb;
	txq->headwb_dma = TMZ_PADDR(headwb);
	txq->headwb_mem = (uint32_t *)TMZ_VADDR(headwb);

	/* Zero out headwb_mem memory */
	for (i = 0; i < headwb_size; i++)
		txq->headwb_mem[i] = 0;

	return 0;
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
	s32 err = 0;

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

	txq->mz = tz;
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
#ifdef RTE_LIB_SECURITY
	txq->using_ipsec = !!(dev->data->dev_conf.txmode.offloads &
			RTE_ETH_TX_OFFLOAD_SECURITY);
#endif

	/* Modification to set tail pointer for virtual function
	 * if vf is detected.
	 */
	if (txgbe_is_vf(hw)) {
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

	if (hw->devarg.tx_headwb)
		err = txgbe_setup_headwb_resources(dev, txq, socket_id);

	txq->ops->reset(txq);
	txq->desc_error = 0;

	dev->data->tx_queues[queue_idx] = txq;

	return err;
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

	/* SSE Vector driver has a different way of releasing mbufs. */
	if (rxq->rx_using_sse) {
		txgbe_rx_queue_release_mbufs_vec(rxq);
		return;
	}

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
		rte_memzone_free(rxq->mz);
		rte_free(rxq);
	}
}

void __rte_cold
txgbe_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	txgbe_rx_queue_release(dev->data->rx_queues[qid]);
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
	rte_pktmbuf_free(rxq->pkt_first_seg);
	rxq->pkt_first_seg = NULL;
	rxq->pkt_last_seg = NULL;

#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM)
	rxq->rxrearm_start = 0;
	rxq->rxrearm_nb = 0;
#endif
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
	if (dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
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

	rxq->mz = rz;
	/*
	 * Zero init all the descriptors in the ring.
	 */
	memset(rz->addr, 0, RX_RING_SZ);

	/*
	 * Modified to setup VFRDT for Virtual Function
	 */
	if (txgbe_is_vf(hw)) {
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

	if (!rte_is_power_of_2(nb_desc)) {
		PMD_INIT_LOG(DEBUG, "queue[%d] doesn't meet Vector Rx "
				    "preconditions - canceling the feature for "
				    "the whole port[%d]",
			     rxq->queue_id, rxq->port_id);
		adapter->rx_vec_allowed = false;
	} else {
		txgbe_rxq_vec_setup(rxq);
	}

	dev->data->rx_queues[queue_idx] = rxq;

	txgbe_reset_rx_queue(adapter, rxq);

	return 0;
}

int
txgbe_dev_rx_queue_count(void *rx_queue)
{
#define TXGBE_RXQ_SCAN_INTERVAL 4
	volatile struct txgbe_rx_desc *rxdp;
	struct txgbe_rx_queue *rxq;
	uint32_t desc = 0;

	rxq = rx_queue;
	rxdp = &rxq->rx_ring[rxq->rx_tail];

	while ((desc < rxq->nb_rx_desc) &&
		(rxdp->qw1.lo.status &
			rte_cpu_to_le_32(TXGBE_RXD_STAT_DD))) {
		desc += TXGBE_RXQ_SCAN_INTERVAL;
		rxdp += TXGBE_RXQ_SCAN_INTERVAL;
		if (rxq->rx_tail + desc >= rxq->nb_rx_desc)
			rxdp = &(rxq->rx_ring[rxq->rx_tail +
				desc - rxq->nb_rx_desc]);
	}

	return desc;
}

int
txgbe_dev_rx_descriptor_status(void *rx_queue, uint16_t offset)
{
	struct txgbe_rx_queue *rxq = rx_queue;
	volatile uint32_t *status;
	uint32_t nb_hold, desc;

	if (unlikely(offset >= rxq->nb_rx_desc))
		return -EINVAL;

#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM)
	if (rxq->rx_using_sse)
		nb_hold = rxq->rxrearm_nb;
	else
#endif
		nb_hold = rxq->nb_rx_hold;
	if (offset >= rxq->nb_rx_desc - nb_hold)
		return RTE_ETH_RX_DESC_UNAVAIL;

	desc = rxq->rx_tail + offset;
	if (desc >= rxq->nb_rx_desc)
		desc -= rxq->nb_rx_desc;

	status = &rxq->rx_ring[desc].qw1.lo.status;
	if (*status & rte_cpu_to_le_32(TXGBE_RXD_STAT_DD))
		return RTE_ETH_RX_DESC_DONE;

	return RTE_ETH_RX_DESC_AVAIL;
}

int
txgbe_dev_tx_descriptor_status(void *tx_queue, uint16_t offset)
{
	struct txgbe_tx_queue *txq = tx_queue;
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
	if (*status & rte_cpu_to_le_32(TXGBE_TXD_DD))
		return RTE_ETH_TX_DESC_DONE;

	return RTE_ETH_TX_DESC_FULL;
}

void __rte_cold
txgbe_dev_clear_queues(struct rte_eth_dev *dev)
{
	unsigned int i;
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		struct txgbe_tx_queue *txq = dev->data->tx_queues[i];

		if (txq != NULL) {
			txq->ops->release_mbufs(txq);
			txq->ops->reset(txq);
		}

		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct txgbe_rx_queue *rxq = dev->data->rx_queues[i];

		if (rxq != NULL) {
			txgbe_rx_queue_release_mbufs(rxq);
			txgbe_reset_rx_queue(adapter, rxq);
		}

		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
}

void
txgbe_dev_free_queues(struct rte_eth_dev *dev)
{
	unsigned int i;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		txgbe_dev_rx_queue_release(dev, i);
		dev->data->rx_queues[i] = NULL;
	}
	dev->data->nb_rx_queues = 0;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txgbe_dev_tx_queue_release(dev, i);
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
 * RSS output index which is used as the RX queue index where to store the
 * received packets.
 * The following output is supplied in the RX write-back descriptor:
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
txgbe_rss_disable(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw;

	hw = TXGBE_DEV_HW(dev);
	if (txgbe_is_vf(hw))
		wr32m(hw, TXGBE_VFPLCFG, TXGBE_VFPLCFG_RSSENA, 0);
	else
		wr32m(hw, TXGBE_RACTL, TXGBE_RACTL_RSSENA, 0);
}

int
txgbe_dev_rss_hash_update(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint8_t  *hash_key;
	uint32_t mrqc;
	uint32_t rss_key;
	uint64_t rss_hf;
	uint16_t i;

	if (!txgbe_rss_update_sp(hw->mac.type)) {
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
			wr32at(hw, TXGBE_REG_RSSKEY, i, rss_key);
		}
	}

	/* Set configured hashing protocols */
	rss_hf = rss_conf->rss_hf & TXGBE_RSS_OFFLOAD_ALL;
	if (txgbe_is_vf(hw)) {
		mrqc = rd32(hw, TXGBE_VFPLCFG);
		mrqc &= ~TXGBE_VFPLCFG_RSSMASK;
		if (rss_hf & RTE_ETH_RSS_IPV4)
			mrqc |= TXGBE_VFPLCFG_RSSIPV4;
		if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP)
			mrqc |= TXGBE_VFPLCFG_RSSIPV4TCP;
		if (rss_hf & RTE_ETH_RSS_IPV6 ||
		    rss_hf & RTE_ETH_RSS_IPV6_EX)
			mrqc |= TXGBE_VFPLCFG_RSSIPV6;
		if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP ||
		    rss_hf & RTE_ETH_RSS_IPV6_TCP_EX)
			mrqc |= TXGBE_VFPLCFG_RSSIPV6TCP;
		if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP)
			mrqc |= TXGBE_VFPLCFG_RSSIPV4UDP;
		if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP ||
		    rss_hf & RTE_ETH_RSS_IPV6_UDP_EX)
			mrqc |= TXGBE_VFPLCFG_RSSIPV6UDP;
		if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_SCTP)
			mrqc |= TXGBE_VFPLCFG_RSSIPV4SCTP;
		if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_SCTP)
			mrqc |= TXGBE_VFPLCFG_RSSIPV6SCTP;

		if (rss_hf)
			mrqc |= TXGBE_VFPLCFG_RSSENA;
		else
			mrqc &= ~TXGBE_VFPLCFG_RSSENA;

		if (dev->data->nb_rx_queues > 3)
			mrqc |= TXGBE_VFPLCFG_RSSHASH(2);
		else if (dev->data->nb_rx_queues > 1)
			mrqc |= TXGBE_VFPLCFG_RSSHASH(1);

		wr32(hw, TXGBE_VFPLCFG, mrqc);
	} else {
		mrqc = rd32(hw, TXGBE_RACTL);
		mrqc &= ~TXGBE_RACTL_RSSMASK;
		if (rss_hf & RTE_ETH_RSS_IPV4)
			mrqc |= TXGBE_RACTL_RSSIPV4;
		if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP)
			mrqc |= TXGBE_RACTL_RSSIPV4TCP;
		if (rss_hf & RTE_ETH_RSS_IPV6 ||
		    rss_hf & RTE_ETH_RSS_IPV6_EX)
			mrqc |= TXGBE_RACTL_RSSIPV6;
		if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP ||
		    rss_hf & RTE_ETH_RSS_IPV6_TCP_EX)
			mrqc |= TXGBE_RACTL_RSSIPV6TCP;
		if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP)
			mrqc |= TXGBE_RACTL_RSSIPV4UDP;
		if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP ||
		    rss_hf & RTE_ETH_RSS_IPV6_UDP_EX)
			mrqc |= TXGBE_RACTL_RSSIPV6UDP;
		if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_SCTP)
			mrqc |= TXGBE_RACTL_RSSIPV4SCTP;
		if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_SCTP)
			mrqc |= TXGBE_RACTL_RSSIPV6SCTP;

		if (rss_hf)
			mrqc |= TXGBE_RACTL_RSSENA;
		else
			mrqc &= ~TXGBE_RACTL_RSSENA;

		wr32(hw, TXGBE_RACTL, mrqc);
	}

	return 0;
}

int
txgbe_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			    struct rte_eth_rss_conf *rss_conf)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint8_t *hash_key;
	uint32_t mrqc;
	uint32_t rss_key;
	uint64_t rss_hf;
	uint16_t i;

	hash_key = rss_conf->rss_key;
	if (hash_key) {
		/* Return RSS hash key */
		for (i = 0; i < 10; i++) {
			rss_key = rd32at(hw, TXGBE_REG_RSSKEY, i);
			hash_key[(i * 4) + 0] = RS32(rss_key, 0, 0xFF);
			hash_key[(i * 4) + 1] = RS32(rss_key, 8, 0xFF);
			hash_key[(i * 4) + 2] = RS32(rss_key, 16, 0xFF);
			hash_key[(i * 4) + 3] = RS32(rss_key, 24, 0xFF);
		}
	}

	rss_hf = 0;
	if (txgbe_is_vf(hw)) {
		mrqc = rd32(hw, TXGBE_VFPLCFG);
		if (mrqc & TXGBE_VFPLCFG_RSSIPV4)
			rss_hf |= RTE_ETH_RSS_IPV4;
		if (mrqc & TXGBE_VFPLCFG_RSSIPV4TCP)
			rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_TCP;
		if (mrqc & TXGBE_VFPLCFG_RSSIPV6)
			rss_hf |= RTE_ETH_RSS_IPV6 |
				  RTE_ETH_RSS_IPV6_EX;
		if (mrqc & TXGBE_VFPLCFG_RSSIPV6TCP)
			rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_TCP |
				  RTE_ETH_RSS_IPV6_TCP_EX;
		if (mrqc & TXGBE_VFPLCFG_RSSIPV4UDP)
			rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_UDP;
		if (mrqc & TXGBE_VFPLCFG_RSSIPV6UDP)
			rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_UDP |
				  RTE_ETH_RSS_IPV6_UDP_EX;
		if (mrqc & TXGBE_VFPLCFG_RSSIPV4SCTP)
			rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_SCTP;
		if (mrqc & TXGBE_VFPLCFG_RSSIPV6SCTP)
			rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_SCTP;
		if (!(mrqc & TXGBE_VFPLCFG_RSSENA))
			rss_hf = 0;
	} else {
		mrqc = rd32(hw, TXGBE_RACTL);
		if (mrqc & TXGBE_RACTL_RSSIPV4)
			rss_hf |= RTE_ETH_RSS_IPV4;
		if (mrqc & TXGBE_RACTL_RSSIPV4TCP)
			rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_TCP;
		if (mrqc & TXGBE_RACTL_RSSIPV6)
			rss_hf |= RTE_ETH_RSS_IPV6 |
				  RTE_ETH_RSS_IPV6_EX;
		if (mrqc & TXGBE_RACTL_RSSIPV6TCP)
			rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_TCP |
				  RTE_ETH_RSS_IPV6_TCP_EX;
		if (mrqc & TXGBE_RACTL_RSSIPV4UDP)
			rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_UDP;
		if (mrqc & TXGBE_RACTL_RSSIPV6UDP)
			rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_UDP |
				  RTE_ETH_RSS_IPV6_UDP_EX;
		if (mrqc & TXGBE_RACTL_RSSIPV4SCTP)
			rss_hf |= RTE_ETH_RSS_NONFRAG_IPV4_SCTP;
		if (mrqc & TXGBE_RACTL_RSSIPV6SCTP)
			rss_hf |= RTE_ETH_RSS_NONFRAG_IPV6_SCTP;
		if (!(mrqc & TXGBE_RACTL_RSSENA))
			rss_hf = 0;
	}

	rss_hf &= TXGBE_RSS_OFFLOAD_ALL;

	rss_conf->rss_hf = rss_hf;
	return 0;
}

static void
txgbe_rss_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_rss_conf rss_conf;
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
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
				wr32at(hw, TXGBE_REG_RSSTBL, i >> 2, reta);
		}
	}
	/*
	 * Configure the RSS key and the RSS protocols used to compute
	 * the RSS hash of input packets.
	 */
	rss_conf = dev->data->dev_conf.rx_adv_conf.rss_conf;
	if (rss_conf.rss_key == NULL)
		rss_conf.rss_key = rss_intel_key; /* Default hash key */
	txgbe_dev_rss_hash_update(dev, &rss_conf);
}

#define NUM_VFTA_REGISTERS 128
#define NIC_RX_BUFFER_SIZE 0x200

static void
txgbe_vmdq_dcb_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_vmdq_dcb_conf *cfg;
	struct txgbe_hw *hw;
	enum rte_eth_nb_pools num_pools;
	uint32_t mrqc, vt_ctl, queue_mapping, vlanctrl;
	uint16_t pbsize;
	uint8_t nb_tcs; /* number of traffic classes */
	int i;

	PMD_INIT_FUNC_TRACE();
	hw = TXGBE_DEV_HW(dev);
	cfg = &dev->data->dev_conf.rx_adv_conf.vmdq_dcb_conf;
	num_pools = cfg->nb_queue_pools;
	/* Check we have a valid number of pools */
	if (num_pools != RTE_ETH_16_POOLS && num_pools != RTE_ETH_32_POOLS) {
		txgbe_rss_disable(dev);
		return;
	}
	/* 16 pools -> 8 traffic classes, 32 pools -> 4 traffic classes */
	nb_tcs = (uint8_t)(RTE_ETH_VMDQ_DCB_NUM_QUEUES / (int)num_pools);

	/*
	 * split rx buffer up into sections, each for 1 traffic class
	 */
	pbsize = (uint16_t)(NIC_RX_BUFFER_SIZE / nb_tcs);
	for (i = 0; i < nb_tcs; i++) {
		uint32_t rxpbsize = rd32(hw, TXGBE_PBRXSIZE(i));

		rxpbsize &= (~(0x3FF << 10));
		/* clear 10 bits. */
		rxpbsize |= (pbsize << 10); /* set value */
		wr32(hw, TXGBE_PBRXSIZE(i), rxpbsize);
	}
	/* zero alloc all unused TCs */
	for (i = nb_tcs; i < RTE_ETH_DCB_NUM_USER_PRIORITIES; i++) {
		uint32_t rxpbsize = rd32(hw, TXGBE_PBRXSIZE(i));

		rxpbsize &= (~(0x3FF << 10));
		/* clear 10 bits. */
		wr32(hw, TXGBE_PBRXSIZE(i), rxpbsize);
	}

	if (num_pools == RTE_ETH_16_POOLS) {
		mrqc = TXGBE_PORTCTL_NUMTC_8;
		mrqc |= TXGBE_PORTCTL_NUMVT_16;
	} else {
		mrqc = TXGBE_PORTCTL_NUMTC_4;
		mrqc |= TXGBE_PORTCTL_NUMVT_32;
	}
	wr32m(hw, TXGBE_PORTCTL,
	      TXGBE_PORTCTL_NUMTC_MASK | TXGBE_PORTCTL_NUMVT_MASK, mrqc);

	vt_ctl = TXGBE_POOLCTL_RPLEN;
	if (cfg->enable_default_pool)
		vt_ctl |= TXGBE_POOLCTL_DEFPL(cfg->default_pool);
	else
		vt_ctl |= TXGBE_POOLCTL_DEFDSA;

	wr32(hw, TXGBE_POOLCTL, vt_ctl);

	queue_mapping = 0;
	for (i = 0; i < RTE_ETH_DCB_NUM_USER_PRIORITIES; i++)
		/*
		 * mapping is done with 3 bits per priority,
		 * so shift by i*3 each time
		 */
		queue_mapping |= ((cfg->dcb_tc[i] & 0x07) << (i * 3));

	wr32(hw, TXGBE_RPUP2TC, queue_mapping);

	wr32(hw, TXGBE_ARBRXCTL, TXGBE_ARBRXCTL_RRM);

	/* enable vlan filtering and allow all vlan tags through */
	vlanctrl = rd32(hw, TXGBE_VLANCTL);
	vlanctrl |= TXGBE_VLANCTL_VFE; /* enable vlan filters */
	wr32(hw, TXGBE_VLANCTL, vlanctrl);

	/* enable all vlan filters */
	for (i = 0; i < NUM_VFTA_REGISTERS; i++)
		wr32(hw, TXGBE_VLANTBL(i), 0xFFFFFFFF);

	wr32(hw, TXGBE_POOLRXENA(0),
			num_pools == RTE_ETH_16_POOLS ? 0xFFFF : 0xFFFFFFFF);

	wr32(hw, TXGBE_ETHADDRIDX, 0);
	wr32(hw, TXGBE_ETHADDRASSL, 0xFFFFFFFF);
	wr32(hw, TXGBE_ETHADDRASSH, 0xFFFFFFFF);

	/* set up filters for vlan tags as configured */
	for (i = 0; i < cfg->nb_pool_maps; i++) {
		/* set vlan id in VF register and set the valid bit */
		wr32(hw, TXGBE_PSRVLANIDX, i);
		wr32(hw, TXGBE_PSRVLAN, (TXGBE_PSRVLAN_EA |
				(cfg->pool_map[i].vlan_id & 0xFFF)));

		wr32(hw, TXGBE_PSRVLANPLM(0), cfg->pool_map[i].pools);
	}
}

/**
 * txgbe_dcb_config_tx_hw_config - Configure general DCB TX parameters
 * @dev: pointer to eth_dev structure
 * @dcb_config: pointer to txgbe_dcb_config structure
 */
static void
txgbe_dcb_tx_hw_config(struct rte_eth_dev *dev,
		       struct txgbe_dcb_config *dcb_config)
{
	uint32_t reg;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	PMD_INIT_FUNC_TRACE();

	/* Disable the Tx desc arbiter */
	reg = rd32(hw, TXGBE_ARBTXCTL);
	reg |= TXGBE_ARBTXCTL_DIA;
	wr32(hw, TXGBE_ARBTXCTL, reg);

	/* Enable DCB for Tx with 8 TCs */
	reg = rd32(hw, TXGBE_PORTCTL);
	reg &= TXGBE_PORTCTL_NUMTC_MASK;
	reg |= TXGBE_PORTCTL_DCB;
	if (dcb_config->num_tcs.pg_tcs == 8)
		reg |= TXGBE_PORTCTL_NUMTC_8;
	else
		reg |= TXGBE_PORTCTL_NUMTC_4;

	wr32(hw, TXGBE_PORTCTL, reg);

	/* Enable the Tx desc arbiter */
	reg = rd32(hw, TXGBE_ARBTXCTL);
	reg &= ~TXGBE_ARBTXCTL_DIA;
	wr32(hw, TXGBE_ARBTXCTL, reg);
}

/**
 * txgbe_vmdq_dcb_hw_tx_config - Configure general VMDQ+DCB TX parameters
 * @dev: pointer to rte_eth_dev structure
 * @dcb_config: pointer to txgbe_dcb_config structure
 */
static void
txgbe_vmdq_dcb_hw_tx_config(struct rte_eth_dev *dev,
			struct txgbe_dcb_config *dcb_config)
{
	struct rte_eth_vmdq_dcb_tx_conf *vmdq_tx_conf =
			&dev->data->dev_conf.tx_adv_conf.vmdq_dcb_tx_conf;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	PMD_INIT_FUNC_TRACE();
	/*PF VF Transmit Enable*/
	wr32(hw, TXGBE_POOLTXENA(0),
		vmdq_tx_conf->nb_queue_pools ==
				RTE_ETH_16_POOLS ? 0xFFFF : 0xFFFFFFFF);

	/*Configure general DCB TX parameters*/
	txgbe_dcb_tx_hw_config(dev, dcb_config);
}

static void
txgbe_vmdq_dcb_rx_config(struct rte_eth_dev *dev,
			struct txgbe_dcb_config *dcb_config)
{
	struct rte_eth_vmdq_dcb_conf *vmdq_rx_conf =
			&dev->data->dev_conf.rx_adv_conf.vmdq_dcb_conf;
	struct txgbe_dcb_tc_config *tc;
	uint8_t i, j;

	/* convert rte_eth_conf.rx_adv_conf to struct txgbe_dcb_config */
	if (vmdq_rx_conf->nb_queue_pools == RTE_ETH_16_POOLS) {
		dcb_config->num_tcs.pg_tcs = RTE_ETH_8_TCS;
		dcb_config->num_tcs.pfc_tcs = RTE_ETH_8_TCS;
	} else {
		dcb_config->num_tcs.pg_tcs = RTE_ETH_4_TCS;
		dcb_config->num_tcs.pfc_tcs = RTE_ETH_4_TCS;
	}

	/* Initialize User Priority to Traffic Class mapping */
	for (j = 0; j < TXGBE_DCB_TC_MAX; j++) {
		tc = &dcb_config->tc_config[j];
		tc->path[TXGBE_DCB_RX_CONFIG].up_to_tc_bitmap = 0;
	}

	/* User Priority to Traffic Class mapping */
	for (i = 0; i < RTE_ETH_DCB_NUM_USER_PRIORITIES; i++) {
		j = vmdq_rx_conf->dcb_tc[i];
		tc = &dcb_config->tc_config[j];
		tc->path[TXGBE_DCB_RX_CONFIG].up_to_tc_bitmap |=
						(uint8_t)(1 << i);
	}
}

static void
txgbe_dcb_vt_tx_config(struct rte_eth_dev *dev,
			struct txgbe_dcb_config *dcb_config)
{
	struct rte_eth_vmdq_dcb_tx_conf *vmdq_tx_conf =
			&dev->data->dev_conf.tx_adv_conf.vmdq_dcb_tx_conf;
	struct txgbe_dcb_tc_config *tc;
	uint8_t i, j;

	/* convert rte_eth_conf.rx_adv_conf to struct txgbe_dcb_config */
	if (vmdq_tx_conf->nb_queue_pools == RTE_ETH_16_POOLS) {
		dcb_config->num_tcs.pg_tcs = RTE_ETH_8_TCS;
		dcb_config->num_tcs.pfc_tcs = RTE_ETH_8_TCS;
	} else {
		dcb_config->num_tcs.pg_tcs = RTE_ETH_4_TCS;
		dcb_config->num_tcs.pfc_tcs = RTE_ETH_4_TCS;
	}

	/* Initialize User Priority to Traffic Class mapping */
	for (j = 0; j < TXGBE_DCB_TC_MAX; j++) {
		tc = &dcb_config->tc_config[j];
		tc->path[TXGBE_DCB_TX_CONFIG].up_to_tc_bitmap = 0;
	}

	/* User Priority to Traffic Class mapping */
	for (i = 0; i < RTE_ETH_DCB_NUM_USER_PRIORITIES; i++) {
		j = vmdq_tx_conf->dcb_tc[i];
		tc = &dcb_config->tc_config[j];
		tc->path[TXGBE_DCB_TX_CONFIG].up_to_tc_bitmap |=
						(uint8_t)(1 << i);
	}
}

static void
txgbe_dcb_rx_config(struct rte_eth_dev *dev,
		struct txgbe_dcb_config *dcb_config)
{
	struct rte_eth_dcb_rx_conf *rx_conf =
			&dev->data->dev_conf.rx_adv_conf.dcb_rx_conf;
	struct txgbe_dcb_tc_config *tc;
	uint8_t i, j;

	dcb_config->num_tcs.pg_tcs = (uint8_t)rx_conf->nb_tcs;
	dcb_config->num_tcs.pfc_tcs = (uint8_t)rx_conf->nb_tcs;

	/* Initialize User Priority to Traffic Class mapping */
	for (j = 0; j < TXGBE_DCB_TC_MAX; j++) {
		tc = &dcb_config->tc_config[j];
		tc->path[TXGBE_DCB_RX_CONFIG].up_to_tc_bitmap = 0;
	}

	/* User Priority to Traffic Class mapping */
	for (i = 0; i < RTE_ETH_DCB_NUM_USER_PRIORITIES; i++) {
		j = rx_conf->dcb_tc[i];
		tc = &dcb_config->tc_config[j];
		tc->path[TXGBE_DCB_RX_CONFIG].up_to_tc_bitmap |=
						(uint8_t)(1 << i);
	}
}

static void
txgbe_dcb_tx_config(struct rte_eth_dev *dev,
		struct txgbe_dcb_config *dcb_config)
{
	struct rte_eth_dcb_tx_conf *tx_conf =
			&dev->data->dev_conf.tx_adv_conf.dcb_tx_conf;
	struct txgbe_dcb_tc_config *tc;
	uint8_t i, j;

	dcb_config->num_tcs.pg_tcs = (uint8_t)tx_conf->nb_tcs;
	dcb_config->num_tcs.pfc_tcs = (uint8_t)tx_conf->nb_tcs;

	/* Initialize User Priority to Traffic Class mapping */
	for (j = 0; j < TXGBE_DCB_TC_MAX; j++) {
		tc = &dcb_config->tc_config[j];
		tc->path[TXGBE_DCB_TX_CONFIG].up_to_tc_bitmap = 0;
	}

	/* User Priority to Traffic Class mapping */
	for (i = 0; i < RTE_ETH_DCB_NUM_USER_PRIORITIES; i++) {
		j = tx_conf->dcb_tc[i];
		tc = &dcb_config->tc_config[j];
		tc->path[TXGBE_DCB_TX_CONFIG].up_to_tc_bitmap |=
						(uint8_t)(1 << i);
	}
}

/**
 * txgbe_dcb_rx_hw_config - Configure general DCB RX HW parameters
 * @dev: pointer to eth_dev structure
 * @dcb_config: pointer to txgbe_dcb_config structure
 */
static void
txgbe_dcb_rx_hw_config(struct rte_eth_dev *dev,
		       struct txgbe_dcb_config *dcb_config)
{
	uint32_t reg;
	uint32_t vlanctrl;
	uint8_t i;
	uint32_t q;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	PMD_INIT_FUNC_TRACE();
	/*
	 * Disable the arbiter before changing parameters
	 * (always enable recycle mode; WSP)
	 */
	reg = TXGBE_ARBRXCTL_RRM | TXGBE_ARBRXCTL_WSP | TXGBE_ARBRXCTL_DIA;
	wr32(hw, TXGBE_ARBRXCTL, reg);

	reg = rd32(hw, TXGBE_PORTCTL);
	reg &= ~(TXGBE_PORTCTL_NUMTC_MASK | TXGBE_PORTCTL_NUMVT_MASK);
	if (dcb_config->num_tcs.pg_tcs == 4) {
		reg |= TXGBE_PORTCTL_NUMTC_4;
		if (dcb_config->vt_mode)
			reg |= TXGBE_PORTCTL_NUMVT_32;
		else
			wr32(hw, TXGBE_POOLCTL, 0);
	}

	if (dcb_config->num_tcs.pg_tcs == 8) {
		reg |= TXGBE_PORTCTL_NUMTC_8;
		if (dcb_config->vt_mode)
			reg |= TXGBE_PORTCTL_NUMVT_16;
		else
			wr32(hw, TXGBE_POOLCTL, 0);
	}

	wr32(hw, TXGBE_PORTCTL, reg);

	if (RTE_ETH_DEV_SRIOV(dev).active == 0) {
		/* Disable drop for all queues in VMDQ mode*/
		for (q = 0; q < TXGBE_MAX_RX_QUEUE_NUM; q++) {
			u32 val = 1 << (q % 32);
			wr32m(hw, TXGBE_QPRXDROP(q / 32), val, val);
		}
	} else {
		/* Enable drop for all queues in SRIOV mode */
		for (q = 0; q < TXGBE_MAX_RX_QUEUE_NUM; q++) {
			u32 val = 1 << (q % 32);
			wr32m(hw, TXGBE_QPRXDROP(q / 32), val, val);
		}
	}

	/* VLNCTL: enable vlan filtering and allow all vlan tags through */
	vlanctrl = rd32(hw, TXGBE_VLANCTL);
	vlanctrl |= TXGBE_VLANCTL_VFE; /* enable vlan filters */
	wr32(hw, TXGBE_VLANCTL, vlanctrl);

	/* VLANTBL - enable all vlan filters */
	for (i = 0; i < NUM_VFTA_REGISTERS; i++)
		wr32(hw, TXGBE_VLANTBL(i), 0xFFFFFFFF);

	/*
	 * Configure Rx packet plane (recycle mode; WSP) and
	 * enable arbiter
	 */
	reg = TXGBE_ARBRXCTL_RRM | TXGBE_ARBRXCTL_WSP;
	wr32(hw, TXGBE_ARBRXCTL, reg);
}

static void
txgbe_dcb_hw_arbite_rx_config(struct txgbe_hw *hw, uint16_t *refill,
		uint16_t *max, uint8_t *bwg_id, uint8_t *tsa, uint8_t *map)
{
	txgbe_dcb_config_rx_arbiter_raptor(hw, refill, max, bwg_id,
					  tsa, map);
}

static void
txgbe_dcb_hw_arbite_tx_config(struct txgbe_hw *hw, uint16_t *refill,
		uint16_t *max, uint8_t *bwg_id, uint8_t *tsa, uint8_t *map)
{
	switch (hw->mac.type) {
	case txgbe_mac_sp:
	case txgbe_mac_aml:
	case txgbe_mac_aml40:
		txgbe_dcb_config_tx_desc_arbiter_raptor(hw, refill,
							max, bwg_id, tsa);
		txgbe_dcb_config_tx_data_arbiter_raptor(hw, refill,
							max, bwg_id, tsa, map);
		break;
	default:
		break;
	}
}

#define DCB_RX_CONFIG  1
#define DCB_TX_CONFIG  1
#define DCB_TX_PB      1024
/**
 * txgbe_dcb_hw_configure - Enable DCB and configure
 * general DCB in VT mode and non-VT mode parameters
 * @dev: pointer to rte_eth_dev structure
 * @dcb_config: pointer to txgbe_dcb_config structure
 */
static int
txgbe_dcb_hw_configure(struct rte_eth_dev *dev,
			struct txgbe_dcb_config *dcb_config)
{
	int     ret = 0;
	uint8_t i, pfc_en, nb_tcs;
	uint16_t pbsize, rx_buffer_size;
	uint8_t config_dcb_rx = 0;
	uint8_t config_dcb_tx = 0;
	uint8_t tsa[TXGBE_DCB_TC_MAX] = {0};
	uint8_t bwgid[TXGBE_DCB_TC_MAX] = {0};
	uint16_t refill[TXGBE_DCB_TC_MAX] = {0};
	uint16_t max[TXGBE_DCB_TC_MAX] = {0};
	uint8_t map[TXGBE_DCB_TC_MAX] = {0};
	struct txgbe_dcb_tc_config *tc;
	uint32_t max_frame = dev->data->mtu +
			RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_bw_conf *bw_conf = TXGBE_DEV_BW_CONF(dev);

	switch (dev->data->dev_conf.rxmode.mq_mode) {
	case RTE_ETH_MQ_RX_VMDQ_DCB:
		dcb_config->vt_mode = true;
		config_dcb_rx = DCB_RX_CONFIG;
		/*
		 * get dcb and VT rx configuration parameters
		 * from rte_eth_conf
		 */
		txgbe_vmdq_dcb_rx_config(dev, dcb_config);
		/*Configure general VMDQ and DCB RX parameters*/
		txgbe_vmdq_dcb_configure(dev);
		break;
	case RTE_ETH_MQ_RX_DCB:
	case RTE_ETH_MQ_RX_DCB_RSS:
		dcb_config->vt_mode = false;
		config_dcb_rx = DCB_RX_CONFIG;
		/* Get dcb TX configuration parameters from rte_eth_conf */
		txgbe_dcb_rx_config(dev, dcb_config);
		/*Configure general DCB RX parameters*/
		txgbe_dcb_rx_hw_config(dev, dcb_config);
		break;
	default:
		PMD_INIT_LOG(ERR, "Incorrect DCB RX mode configuration");
		break;
	}
	switch (dev->data->dev_conf.txmode.mq_mode) {
	case RTE_ETH_MQ_TX_VMDQ_DCB:
		dcb_config->vt_mode = true;
		config_dcb_tx = DCB_TX_CONFIG;
		/* get DCB and VT TX configuration parameters
		 * from rte_eth_conf
		 */
		txgbe_dcb_vt_tx_config(dev, dcb_config);
		/* Configure general VMDQ and DCB TX parameters */
		txgbe_vmdq_dcb_hw_tx_config(dev, dcb_config);
		break;

	case RTE_ETH_MQ_TX_DCB:
		dcb_config->vt_mode = false;
		config_dcb_tx = DCB_TX_CONFIG;
		/* get DCB TX configuration parameters from rte_eth_conf */
		txgbe_dcb_tx_config(dev, dcb_config);
		/* Configure general DCB TX parameters */
		txgbe_dcb_tx_hw_config(dev, dcb_config);
		break;
	default:
		PMD_INIT_LOG(ERR, "Incorrect DCB TX mode configuration");
		break;
	}

	nb_tcs = dcb_config->num_tcs.pfc_tcs;
	/* Unpack map */
	txgbe_dcb_unpack_map_cee(dcb_config, TXGBE_DCB_RX_CONFIG, map);
	if (nb_tcs == RTE_ETH_4_TCS) {
		/* Avoid un-configured priority mapping to TC0 */
		uint8_t j = 4;
		uint8_t mask = 0xFF;

		for (i = 0; i < RTE_ETH_DCB_NUM_USER_PRIORITIES - 4; i++)
			mask = (uint8_t)(mask & (~(1 << map[i])));
		for (i = 0; mask && (i < TXGBE_DCB_TC_MAX); i++) {
			if ((mask & 0x1) && j < RTE_ETH_DCB_NUM_USER_PRIORITIES)
				map[j++] = i;
			mask >>= 1;
		}
		/* Re-configure 4 TCs BW */
		for (i = 0; i < nb_tcs; i++) {
			tc = &dcb_config->tc_config[i];
			if (bw_conf->tc_num != nb_tcs)
				tc->path[TXGBE_DCB_TX_CONFIG].bwg_percent =
					(uint8_t)(100 / nb_tcs);
			tc->path[TXGBE_DCB_RX_CONFIG].bwg_percent =
						(uint8_t)(100 / nb_tcs);
		}
		for (; i < TXGBE_DCB_TC_MAX; i++) {
			tc = &dcb_config->tc_config[i];
			tc->path[TXGBE_DCB_TX_CONFIG].bwg_percent = 0;
			tc->path[TXGBE_DCB_RX_CONFIG].bwg_percent = 0;
		}
	} else {
		/* Re-configure 8 TCs BW */
		for (i = 0; i < nb_tcs; i++) {
			tc = &dcb_config->tc_config[i];
			if (bw_conf->tc_num != nb_tcs)
				tc->path[TXGBE_DCB_TX_CONFIG].bwg_percent =
					(uint8_t)(100 / nb_tcs + (i & 1));
			tc->path[TXGBE_DCB_RX_CONFIG].bwg_percent =
				(uint8_t)(100 / nb_tcs + (i & 1));
		}
	}

	rx_buffer_size = NIC_RX_BUFFER_SIZE;

	if (config_dcb_rx) {
		/* Set RX buffer size */
		pbsize = (uint16_t)(rx_buffer_size / nb_tcs);
		uint32_t rxpbsize = pbsize << 10;

		for (i = 0; i < nb_tcs; i++)
			wr32(hw, TXGBE_PBRXSIZE(i), rxpbsize);

		/* zero alloc all unused TCs */
		for (; i < RTE_ETH_DCB_NUM_USER_PRIORITIES; i++)
			wr32(hw, TXGBE_PBRXSIZE(i), 0);
	}
	if (config_dcb_tx) {
		/* Only support an equally distributed
		 *  Tx packet buffer strategy.
		 */
		uint32_t txpktsize = TXGBE_PBTXSIZE_MAX / nb_tcs;
		uint32_t txpbthresh = (txpktsize / DCB_TX_PB) -
					TXGBE_TXPKT_SIZE_MAX;

		for (i = 0; i < nb_tcs; i++) {
			wr32(hw, TXGBE_PBTXSIZE(i), txpktsize);
			wr32(hw, TXGBE_PBTXDMATH(i), txpbthresh);
		}
		/* Clear unused TCs, if any, to zero buffer size*/
		for (; i < RTE_ETH_DCB_NUM_USER_PRIORITIES; i++) {
			wr32(hw, TXGBE_PBTXSIZE(i), 0);
			wr32(hw, TXGBE_PBTXDMATH(i), 0);
		}
	}

	/*Calculates traffic class credits*/
	txgbe_dcb_calculate_tc_credits_cee(hw, dcb_config, max_frame,
				TXGBE_DCB_TX_CONFIG);
	txgbe_dcb_calculate_tc_credits_cee(hw, dcb_config, max_frame,
				TXGBE_DCB_RX_CONFIG);

	if (config_dcb_rx) {
		/* Unpack CEE standard containers */
		txgbe_dcb_unpack_refill_cee(dcb_config,
				TXGBE_DCB_RX_CONFIG, refill);
		txgbe_dcb_unpack_max_cee(dcb_config, max);
		txgbe_dcb_unpack_bwgid_cee(dcb_config,
				TXGBE_DCB_RX_CONFIG, bwgid);
		txgbe_dcb_unpack_tsa_cee(dcb_config,
				TXGBE_DCB_RX_CONFIG, tsa);
		/* Configure PG(ETS) RX */
		txgbe_dcb_hw_arbite_rx_config(hw, refill, max, bwgid, tsa, map);
	}

	if (config_dcb_tx) {
		/* Unpack CEE standard containers */
		txgbe_dcb_unpack_refill_cee(dcb_config,
				TXGBE_DCB_TX_CONFIG, refill);
		txgbe_dcb_unpack_max_cee(dcb_config, max);
		txgbe_dcb_unpack_bwgid_cee(dcb_config,
				TXGBE_DCB_TX_CONFIG, bwgid);
		txgbe_dcb_unpack_tsa_cee(dcb_config,
				TXGBE_DCB_TX_CONFIG, tsa);
		/* Configure PG(ETS) TX */
		txgbe_dcb_hw_arbite_tx_config(hw, refill, max, bwgid, tsa, map);
	}

	/* Configure queue statistics registers */
	txgbe_dcb_config_tc_stats_raptor(hw, dcb_config);

	/* Check if the PFC is supported */
	if (dev->data->dev_conf.dcb_capability_en & RTE_ETH_DCB_PFC_SUPPORT) {
		pbsize = (uint16_t)(rx_buffer_size / nb_tcs);
		for (i = 0; i < nb_tcs; i++) {
			/* If the TC count is 8,
			 * and the default high_water is 48,
			 * the low_water is 16 as default.
			 */
			hw->fc.high_water[i] = (pbsize * 3) / 4;
			hw->fc.low_water[i] = pbsize / 4;
			/* Enable pfc for this TC */
			tc = &dcb_config->tc_config[i];
			tc->pfc = txgbe_dcb_pfc_enabled;
		}
		txgbe_dcb_unpack_pfc_cee(dcb_config, map, &pfc_en);
		if (dcb_config->num_tcs.pfc_tcs == RTE_ETH_4_TCS)
			pfc_en &= 0x0F;
		ret = txgbe_dcb_config_pfc(hw, pfc_en, map);
	}

	return ret;
}

void txgbe_configure_pb(struct rte_eth_dev *dev)
{
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);

	int hdrm;
	int tc = dev_conf->rx_adv_conf.dcb_rx_conf.nb_tcs;

	/* Reserve 256KB(/512KB) rx buffer for fdir */
	hdrm = 256; /*KB*/

	hw->mac.setup_pba(hw, tc, hdrm, PBA_STRATEGY_EQUAL);
}

void txgbe_configure_port(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	int i = 0;
	uint16_t tpids[8] = {RTE_ETHER_TYPE_VLAN, RTE_ETHER_TYPE_QINQ,
				0x9100, 0x9200,
				0x0000, 0x0000,
				0x0000, 0x0000};

	PMD_INIT_FUNC_TRACE();

	/* default outer vlan tpid */
	wr32(hw, TXGBE_EXTAG,
		TXGBE_EXTAG_ETAG(RTE_ETHER_TYPE_ETAG) |
		TXGBE_EXTAG_VLAN(RTE_ETHER_TYPE_QINQ));

	/* default inner vlan tpid */
	wr32m(hw, TXGBE_VLANCTL,
		TXGBE_VLANCTL_TPID_MASK,
		TXGBE_VLANCTL_TPID(RTE_ETHER_TYPE_VLAN));
	wr32m(hw, TXGBE_DMATXCTRL,
		TXGBE_DMATXCTRL_TPID_MASK,
		TXGBE_DMATXCTRL_TPID(RTE_ETHER_TYPE_VLAN));

	/* default vlan tpid filters */
	for (i = 0; i < 8; i++) {
		wr32m(hw, TXGBE_TAGTPID(i / 2),
			(i % 2 ? TXGBE_TAGTPID_MSB_MASK
			       : TXGBE_TAGTPID_LSB_MASK),
			(i % 2 ? TXGBE_TAGTPID_MSB(tpids[i])
			       : TXGBE_TAGTPID_LSB(tpids[i])));
	}

	/* default vxlan port */
	wr32(hw, TXGBE_VXLANPORT, 4789);
}

/**
 * txgbe_configure_dcb - Configure DCB  Hardware
 * @dev: pointer to rte_eth_dev
 */
void txgbe_configure_dcb(struct rte_eth_dev *dev)
{
	struct txgbe_dcb_config *dcb_cfg = TXGBE_DEV_DCB_CONFIG(dev);
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;

	PMD_INIT_FUNC_TRACE();

	/* check support mq_mode for DCB */
	if (dev_conf->rxmode.mq_mode != RTE_ETH_MQ_RX_VMDQ_DCB &&
	    dev_conf->rxmode.mq_mode != RTE_ETH_MQ_RX_DCB &&
	    dev_conf->rxmode.mq_mode != RTE_ETH_MQ_RX_DCB_RSS)
		return;

	if (dev->data->nb_rx_queues > RTE_ETH_DCB_NUM_QUEUES)
		return;

	/** Configure DCB hardware **/
	txgbe_dcb_hw_configure(dev, dcb_cfg);
}

/*
 * VMDq only support for 10 GbE NIC.
 */
static void
txgbe_vmdq_rx_hw_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_vmdq_rx_conf *cfg;
	struct txgbe_hw *hw;
	enum rte_eth_nb_pools num_pools;
	uint32_t mrqc, vt_ctl, vlanctrl;
	uint32_t vmolr = 0;
	int i;

	PMD_INIT_FUNC_TRACE();
	hw = TXGBE_DEV_HW(dev);
	cfg = &dev->data->dev_conf.rx_adv_conf.vmdq_rx_conf;
	num_pools = cfg->nb_queue_pools;

	txgbe_rss_disable(dev);

	/* enable vmdq */
	mrqc = TXGBE_PORTCTL_NUMVT_64;
	wr32m(hw, TXGBE_PORTCTL, TXGBE_PORTCTL_NUMVT_MASK, mrqc);

	/* turn on virtualisation and set the default pool */
	vt_ctl = TXGBE_POOLCTL_RPLEN;
	if (cfg->enable_default_pool)
		vt_ctl |= TXGBE_POOLCTL_DEFPL(cfg->default_pool);
	else
		vt_ctl |= TXGBE_POOLCTL_DEFDSA;

	wr32(hw, TXGBE_POOLCTL, vt_ctl);

	for (i = 0; i < (int)num_pools; i++) {
		vmolr = txgbe_convert_vm_rx_mask_to_val(cfg->rx_mode, vmolr);
		wr32(hw, TXGBE_POOLETHCTL(i), vmolr);
	}

	/* enable vlan filtering and allow all vlan tags through */
	vlanctrl = rd32(hw, TXGBE_VLANCTL);
	vlanctrl |= TXGBE_VLANCTL_VFE; /* enable vlan filters */
	wr32(hw, TXGBE_VLANCTL, vlanctrl);

	/* enable all vlan filters */
	for (i = 0; i < NUM_VFTA_REGISTERS; i++)
		wr32(hw, TXGBE_VLANTBL(i), UINT32_MAX);

	/* pool enabling for receive - 64 */
	wr32(hw, TXGBE_POOLRXENA(0), UINT32_MAX);
	if (num_pools == RTE_ETH_64_POOLS)
		wr32(hw, TXGBE_POOLRXENA(1), UINT32_MAX);

	/*
	 * allow pools to read specific mac addresses
	 * In this case, all pools should be able to read from mac addr 0
	 */
	wr32(hw, TXGBE_ETHADDRIDX, 0);
	wr32(hw, TXGBE_ETHADDRASSL, 0xFFFFFFFF);
	wr32(hw, TXGBE_ETHADDRASSH, 0xFFFFFFFF);

	/* set up filters for vlan tags as configured */
	for (i = 0; i < cfg->nb_pool_maps; i++) {
		/* set vlan id in VF register and set the valid bit */
		wr32(hw, TXGBE_PSRVLANIDX, i);
		wr32(hw, TXGBE_PSRVLAN, (TXGBE_PSRVLAN_EA |
				TXGBE_PSRVLAN_VID(cfg->pool_map[i].vlan_id)));
		/*
		 * Put the allowed pools in VFB reg. As we only have 16 or 64
		 * pools, we only need to use the first half of the register
		 * i.e. bits 0-31
		 */
		if (((cfg->pool_map[i].pools >> 32) & UINT32_MAX) == 0)
			wr32(hw, TXGBE_PSRVLANPLM(0),
				(cfg->pool_map[i].pools & UINT32_MAX));
		else
			wr32(hw, TXGBE_PSRVLANPLM(1),
				((cfg->pool_map[i].pools >> 32) & UINT32_MAX));
	}

	/* Tx General Switch Control Enables VMDQ loopback */
	if (cfg->enable_loop_back) {
		wr32(hw, TXGBE_PSRCTL, TXGBE_PSRCTL_LBENA);
		for (i = 0; i < 64; i++)
			wr32m(hw, TXGBE_POOLETHCTL(i),
				TXGBE_POOLETHCTL_LLB, TXGBE_POOLETHCTL_LLB);
	}

	txgbe_flush(hw);
}

/*
 * txgbe_vmdq_tx_hw_configure - Configure general VMDq TX parameters
 * @hw: pointer to hardware structure
 */
static void
txgbe_vmdq_tx_hw_configure(struct txgbe_hw *hw)
{
	uint32_t reg;
	uint32_t q;

	PMD_INIT_FUNC_TRACE();
	/*PF VF Transmit Enable*/
	wr32(hw, TXGBE_POOLTXENA(0), UINT32_MAX);
	wr32(hw, TXGBE_POOLTXENA(1), UINT32_MAX);

	/* Disable the Tx desc arbiter */
	reg = rd32(hw, TXGBE_ARBTXCTL);
	reg |= TXGBE_ARBTXCTL_DIA;
	wr32(hw, TXGBE_ARBTXCTL, reg);

	wr32m(hw, TXGBE_PORTCTL, TXGBE_PORTCTL_NUMVT_MASK,
		TXGBE_PORTCTL_NUMVT_64);

	/* Disable drop for all queues */
	for (q = 0; q < 128; q++) {
		u32 val = 1 << (q % 32);
		wr32m(hw, TXGBE_QPRXDROP(q / 32), val, val);
	}

	/* Enable the Tx desc arbiter */
	reg = rd32(hw, TXGBE_ARBTXCTL);
	reg &= ~TXGBE_ARBTXCTL_DIA;
	wr32(hw, TXGBE_ARBTXCTL, reg);

	txgbe_flush(hw);
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

static int
txgbe_config_vf_rss(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw;
	uint32_t mrqc;

	txgbe_rss_configure(dev);

	hw = TXGBE_DEV_HW(dev);

	/* enable VF RSS */
	mrqc = rd32(hw, TXGBE_PORTCTL);
	mrqc &= ~(TXGBE_PORTCTL_NUMTC_MASK | TXGBE_PORTCTL_NUMVT_MASK);
	switch (RTE_ETH_DEV_SRIOV(dev).active) {
	case RTE_ETH_64_POOLS:
		mrqc |= TXGBE_PORTCTL_NUMVT_64;
		break;

	case RTE_ETH_32_POOLS:
		mrqc |= TXGBE_PORTCTL_NUMVT_32;
		break;

	default:
		PMD_INIT_LOG(ERR, "Invalid pool number in IOV mode with VMDQ RSS");
		return -EINVAL;
	}

	wr32(hw, TXGBE_PORTCTL, mrqc);

	return 0;
}

static int
txgbe_config_vf_default(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t mrqc;

	mrqc = rd32(hw, TXGBE_PORTCTL);
	mrqc &= ~(TXGBE_PORTCTL_NUMTC_MASK | TXGBE_PORTCTL_NUMVT_MASK);
	switch (RTE_ETH_DEV_SRIOV(dev).active) {
	case RTE_ETH_64_POOLS:
		mrqc |= TXGBE_PORTCTL_NUMVT_64;
		break;

	case RTE_ETH_32_POOLS:
		mrqc |= TXGBE_PORTCTL_NUMVT_32;
		break;

	case RTE_ETH_16_POOLS:
		mrqc |= TXGBE_PORTCTL_NUMVT_16;
		break;
	default:
		PMD_INIT_LOG(ERR,
			"invalid pool number in IOV mode");
		return 0;
	}

	wr32(hw, TXGBE_PORTCTL, mrqc);

	return 0;
}

static int
txgbe_dev_mq_rx_configure(struct rte_eth_dev *dev)
{
	if (RTE_ETH_DEV_SRIOV(dev).active == 0) {
		/*
		 * SRIOV inactive scheme
		 * any DCB/RSS w/o VMDq multi-queue setting
		 */
		switch (dev->data->dev_conf.rxmode.mq_mode) {
		case RTE_ETH_MQ_RX_RSS:
		case RTE_ETH_MQ_RX_DCB_RSS:
		case RTE_ETH_MQ_RX_VMDQ_RSS:
			txgbe_rss_configure(dev);
			break;

		case RTE_ETH_MQ_RX_VMDQ_DCB:
			txgbe_vmdq_dcb_configure(dev);
			break;

		case RTE_ETH_MQ_RX_VMDQ_ONLY:
			txgbe_vmdq_rx_hw_configure(dev);
			break;

		case RTE_ETH_MQ_RX_NONE:
		default:
			/* if mq_mode is none, disable rss mode.*/
			txgbe_rss_disable(dev);
			break;
		}
	} else {
		/* SRIOV active scheme
		 * Support RSS together with SRIOV.
		 */
		switch (dev->data->dev_conf.rxmode.mq_mode) {
		case RTE_ETH_MQ_RX_RSS:
		case RTE_ETH_MQ_RX_VMDQ_RSS:
			txgbe_config_vf_rss(dev);
			break;
		case RTE_ETH_MQ_RX_VMDQ_DCB:
		case RTE_ETH_MQ_RX_DCB:
		/* In SRIOV, the configuration is the same as VMDq case */
			txgbe_vmdq_dcb_configure(dev);
			break;
		/* DCB/RSS together with SRIOV is not supported */
		case RTE_ETH_MQ_RX_VMDQ_DCB_RSS:
		case RTE_ETH_MQ_RX_DCB_RSS:
			PMD_INIT_LOG(ERR,
				"Could not support DCB/RSS with VMDq & SRIOV");
			return -1;
		default:
			txgbe_config_vf_default(dev);
			break;
		}
	}

	return 0;
}

static int
txgbe_dev_mq_tx_configure(struct rte_eth_dev *dev)
{
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	uint32_t mtqc;
	uint32_t rttdcs;

	/* disable arbiter */
	rttdcs = rd32(hw, TXGBE_ARBTXCTL);
	rttdcs |= TXGBE_ARBTXCTL_DIA;
	wr32(hw, TXGBE_ARBTXCTL, rttdcs);

	if (RTE_ETH_DEV_SRIOV(dev).active == 0) {
		/*
		 * SRIOV inactive scheme
		 * any DCB w/o VMDq multi-queue setting
		 */
		if (dev->data->dev_conf.txmode.mq_mode == RTE_ETH_MQ_TX_VMDQ_ONLY)
			txgbe_vmdq_tx_hw_configure(hw);
		else
			wr32m(hw, TXGBE_PORTCTL, TXGBE_PORTCTL_NUMVT_MASK, 0);
	} else {
		switch (RTE_ETH_DEV_SRIOV(dev).active) {
		/*
		 * SRIOV active scheme
		 * FIXME if support DCB together with VMDq & SRIOV
		 */
		case RTE_ETH_64_POOLS:
			mtqc = TXGBE_PORTCTL_NUMVT_64;
			break;
		case RTE_ETH_32_POOLS:
			mtqc = TXGBE_PORTCTL_NUMVT_32;
			break;
		case RTE_ETH_16_POOLS:
			mtqc = TXGBE_PORTCTL_NUMVT_16;
			break;
		default:
			mtqc = 0;
			PMD_INIT_LOG(ERR, "invalid pool number in IOV mode");
		}
		wr32m(hw, TXGBE_PORTCTL, TXGBE_PORTCTL_NUMVT_MASK, mtqc);
	}

	/* re-enable arbiter */
	rttdcs &= ~TXGBE_ARBTXCTL_DIA;
	wr32(hw, TXGBE_ARBTXCTL, rttdcs);

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
	if (dev_info.rx_offload_capa & RTE_ETH_RX_OFFLOAD_TCP_LRO)
		rsc_capable = true;

	if (!rsc_capable && (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO)) {
		PMD_INIT_LOG(CRIT, "LRO is requested on HW that doesn't "
				   "support it");
		return -EINVAL;
	}

	/* RSC global configuration */

	if ((rx_conf->offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC) &&
	     (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO)) {
		PMD_INIT_LOG(CRIT, "LRO can't be enabled when HW CRC "
				    "is disabled");
		return -EINVAL;
	}

	rfctl = rd32(hw, TXGBE_PSRCTL);
	if (rsc_capable && (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO))
		rfctl &= ~TXGBE_PSRCTL_RSCDIA;
	else
		rfctl |= TXGBE_PSRCTL_RSCDIA;
	wr32(hw, TXGBE_PSRCTL, rfctl);

	/* If LRO hasn't been requested - we are done here. */
	if (!(rx_conf->offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO))
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

void __rte_cold
txgbe_set_rx_function(struct rte_eth_dev *dev)
{
	uint16_t i, rx_using_sse;
	struct txgbe_adapter *adapter = TXGBE_DEV_ADAPTER(dev);

	/*
	 * In order to allow Vector Rx there are a few configuration
	 * conditions to be met and Rx Bulk Allocation should be allowed.
	 */
	if (txgbe_rx_vec_dev_conf_condition_check(dev) ||
	    !adapter->rx_bulk_alloc_allowed ||
			rte_vect_get_max_simd_bitwidth() < RTE_VECT_SIMD_128) {
		PMD_INIT_LOG(DEBUG, "Port[%d] doesn't meet Vector Rx "
				    "preconditions",
			     dev->data->port_id);

		adapter->rx_vec_allowed = false;
	}

	/*
	 * Initialize the appropriate LRO callback.
	 *
	 * If all queues satisfy the bulk allocation preconditions
	 * (adapter->rx_bulk_alloc_allowed is TRUE) then we may use
	 * bulk allocation. Otherwise use a single allocation version.
	 */
	if (dev->data->lro) {
		if (adapter->rx_bulk_alloc_allowed) {
			PMD_INIT_LOG(DEBUG, "LRO is requested. Using a bulk "
					   "allocation version");
			dev->rx_pkt_burst = txgbe_recv_pkts_lro_bulk_alloc;
		} else {
			PMD_INIT_LOG(DEBUG, "LRO is requested. Using a single "
					   "allocation version");
			dev->rx_pkt_burst = txgbe_recv_pkts_lro_single_alloc;
		}
	} else if (dev->data->scattered_rx) {
		/*
		 * Set the non-LRO scattered callback: there are bulk and
		 * single allocation versions.
		 */
		if (adapter->rx_vec_allowed) {
			PMD_INIT_LOG(DEBUG, "Using Vector Scattered Rx "
					    "callback (port=%d).",
				     dev->data->port_id);
			dev->rx_pkt_burst = txgbe_recv_scattered_pkts_vec;
		} else if (adapter->rx_bulk_alloc_allowed) {
			PMD_INIT_LOG(DEBUG, "Using a Scattered with bulk "
					   "allocation callback (port=%d).",
				     dev->data->port_id);
			dev->rx_pkt_burst = txgbe_recv_pkts_lro_bulk_alloc;
		} else {
			PMD_INIT_LOG(DEBUG, "Using Regular (non-vector, "
					    "single allocation) "
					    "Scattered Rx callback "
					    "(port=%d).",
				     dev->data->port_id);

			dev->rx_pkt_burst = txgbe_recv_pkts_lro_single_alloc;
		}
	/*
	 * Below we set "simple" callbacks according to port/queues parameters.
	 * If parameters allow we are going to choose between the following
	 * callbacks:
	 *    - Vector
	 *    - Bulk Allocation
	 *    - Single buffer allocation (the simplest one)
	 */
	} else if (adapter->rx_vec_allowed) {
		PMD_INIT_LOG(DEBUG, "Vector rx enabled, please make sure RX "
				    "burst size no less than %d (port=%d).",
			     RTE_TXGBE_DESCS_PER_LOOP,
			     dev->data->port_id);
		dev->rx_pkt_burst = txgbe_recv_pkts_vec;
	} else if (adapter->rx_bulk_alloc_allowed) {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are "
				    "satisfied. Rx Burst Bulk Alloc function "
				    "will be used on port=%d.",
			     dev->data->port_id);

		dev->rx_pkt_burst = txgbe_recv_pkts_bulk_alloc;
	} else {
		PMD_INIT_LOG(DEBUG, "Rx Burst Bulk Alloc Preconditions are not "
				    "satisfied, or Scattered Rx is requested "
				    "(port=%d).",
			     dev->data->port_id);

		dev->rx_pkt_burst = txgbe_recv_pkts;
	}

	rx_using_sse = (dev->rx_pkt_burst == txgbe_recv_scattered_pkts_vec ||
			dev->rx_pkt_burst == txgbe_recv_pkts_vec);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct txgbe_rx_queue *rxq = dev->data->rx_queues[i];

		rxq->rx_using_sse = rx_using_sse;
#ifdef RTE_LIB_SECURITY
		rxq->using_ipsec = !!(dev->data->dev_conf.rxmode.offloads &
				RTE_ETH_RX_OFFLOAD_SECURITY);
#endif
	}
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
	if (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		hlreg0 &= ~TXGBE_SECRXCTL_CRCSTRIP;
	else
		hlreg0 |= TXGBE_SECRXCTL_CRCSTRIP;
	wr32(hw, TXGBE_SECRXCTL, hlreg0);

	/*
	 * Configure jumbo frame support, if any.
	 */
	wr32m(hw, TXGBE_FRMSZ, TXGBE_FRMSZ_MAX_MASK,
		TXGBE_FRMSZ_MAX(dev->data->mtu + TXGBE_ETH_OVERHEAD));

	/*
	 * If loopback mode is configured, set LPBK bit.
	 */
	hlreg0 = rd32(hw, TXGBE_PSRCTL);
	if (txgbe_is_pf(hw) &&
	    dev->data->dev_conf.lpbk_mode)
		hlreg0 |= TXGBE_PSRCTL_LBENA;
	else
		hlreg0 &= ~TXGBE_PSRCTL_LBENA;

	wr32(hw, TXGBE_PSRCTL, hlreg0);

	/*
	 * Assume no header split and no VLAN strip support
	 * on any Rx queue first .
	 */
	rx_conf->offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_STRIP;

	/* Setup RX queues */
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
		buf_size = ROUND_DOWN(buf_size, 0x1 << 10);
		srrctl |= TXGBE_RXCFG_PKTLEN(buf_size);

		wr32(hw, TXGBE_RXCFG(rxq->reg_idx), srrctl);

		/* It adds dual VLAN length for supporting dual VLAN */
		if (dev->data->mtu + TXGBE_ETH_OVERHEAD +
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
	txgbe_dev_mq_rx_configure(dev);

	/*
	 * Setup the Checksum Register.
	 * Disable Full-Packet Checksum which is mutually exclusive with RSS.
	 * Enable IP/L4 checksum computation by hardware if requested to do so.
	 */
	rxcsum = rd32(hw, TXGBE_PSRCTL);
	rxcsum |= TXGBE_PSRCTL_PCSD;
	if (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_CHECKSUM)
		rxcsum |= TXGBE_PSRCTL_L4CSUM;
	else
		rxcsum &= ~TXGBE_PSRCTL_L4CSUM;

	wr32(hw, TXGBE_PSRCTL, rxcsum);

	if (txgbe_is_pf(hw)) {
		rdrxctl = rd32(hw, TXGBE_SECRXCTL);
		if (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
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

		if ((hw->mac.type == txgbe_mac_aml || hw->mac.type == txgbe_mac_aml40) &&
		     hw->devarg.tx_headwb) {
			uint32_t txdctl;

			wr32(hw, TXGBE_PX_TR_HEAD_ADDRL(txq->reg_idx),
				(uint32_t)(txq->headwb_dma & BIT_MASK32));
			wr32(hw, TXGBE_PX_TR_HEAD_ADDRH(txq->reg_idx),
				(uint32_t)(txq->headwb_dma >> 32));
			if (hw->devarg.tx_headwb_size == 16)
				txdctl = TXGBE_PX_TR_CFG_HEAD_WB |
					 TXGBE_PX_TR_CFG_HEAD_WB_64BYTE;
			else
				txdctl = TXGBE_PX_TR_CFG_HEAD_WB;
			wr32m(hw, TXGBE_TXCFG(txq->reg_idx),
				TXGBE_PX_TR_CFG_HEAD_WB_MASK, txdctl);
		}
	}

#ifndef RTE_LIB_SECURITY
	for (i = 0; i < 4; i++)
		wr32(hw, TXGBE_TDM_DESC_CHK(i), 0xFFFFFFFF);
#endif

	/* Device configured with multiple TX queues. */
	txgbe_dev_mq_tx_configure(dev);
}

/*
 * Set up link loopback mode Tx->Rx.
 */
static inline void __rte_cold
txgbe_setup_loopback_link_raptor(struct txgbe_hw *hw)
{
	PMD_INIT_FUNC_TRACE();

	wr32m(hw, TXGBE_MACRXCFG, TXGBE_MACRXCFG_LB, TXGBE_MACRXCFG_LB);

	msec_delay(50);
}

/*
 * Start Transmit and Receive Units.
 */
int __rte_cold
txgbe_dev_rxtx_start(struct rte_eth_dev *dev)
{
	struct txgbe_hw     *hw;
	struct txgbe_tx_queue *txq;
	struct txgbe_rx_queue *rxq;
	uint32_t dmatxctl;
	uint32_t rxctrl;
	uint16_t i;
	int ret = 0;

	PMD_INIT_FUNC_TRACE();
	hw = TXGBE_DEV_HW(dev);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		/* Setup Transmit Threshold Registers */
		if (hw->mac.type == txgbe_mac_aml || hw->mac.type == txgbe_mac_aml40)
			wr32m(hw, TXGBE_TXCFG(txq->reg_idx),
			      TXGBE_TXCFG_HTHRESH_MASK |
			      TXGBE_TXCFG_WTHRESH_MASK_AML,
			      TXGBE_TXCFG_HTHRESH(txq->hthresh) |
			      TXGBE_TXCFG_WTHRESH_AML(txq->wthresh));
		else
			wr32m(hw, TXGBE_TXCFG(txq->reg_idx),
			      TXGBE_TXCFG_HTHRESH_MASK |
			      TXGBE_TXCFG_WTHRESH_MASK,
			      TXGBE_TXCFG_HTHRESH(txq->hthresh) |
			      TXGBE_TXCFG_WTHRESH(txq->wthresh));
	}

	dmatxctl = rd32(hw, TXGBE_DMATXCTRL);
	dmatxctl |= TXGBE_DMATXCTRL_ENA;
	wr32(hw, TXGBE_DMATXCTRL, dmatxctl);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (!txq->tx_deferred_start) {
			ret = txgbe_dev_tx_queue_start(dev, i);
			if (ret < 0)
				return ret;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (!rxq->rx_deferred_start) {
			ret = txgbe_dev_rx_queue_start(dev, i);
			if (ret < 0)
				return ret;
		}
	}

	/* enable mac transmitter */
	if (hw->mac.type == txgbe_mac_aml || hw->mac.type == txgbe_mac_aml40) {
		wr32(hw, TXGBE_SECTXCTL, 0);
		wr32m(hw, TXGBE_MACTXCFG,
			  TXGBE_MACTXCFG_TXE, TXGBE_MACTXCFG_TXE);
	}

	/* Enable Receive engine */
	rxctrl = rd32(hw, TXGBE_PBRXCTL);
	rxctrl |= TXGBE_PBRXCTL_ENA;
	hw->mac.enable_rx_dma(hw, rxctrl);

	/* If loopback mode is enabled, set up the link accordingly */
	if (txgbe_is_pf(hw) &&
	    dev->data->dev_conf.lpbk_mode)
		txgbe_setup_loopback_link_raptor(hw);

#ifdef RTE_LIB_SECURITY
	if ((dev->data->dev_conf.rxmode.offloads & RTE_ETH_RX_OFFLOAD_SECURITY) ||
	    (dev->data->dev_conf.txmode.offloads & RTE_ETH_TX_OFFLOAD_SECURITY)) {
		ret = txgbe_crypto_enable_ipsec(dev);
		if (ret != 0) {
			PMD_DRV_LOG(ERR,
				    "txgbe_crypto_enable_ipsec fails with %d.",
				    ret);
			return ret;
		}
	}
#endif

	return 0;
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
	txq->resetting = false;

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

void
txgbe_tx_queue_clear_error(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct txgbe_hw *hw = TXGBE_DEV_HW(dev);
	struct txgbe_tx_queue *txq;
	u32 i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (!txq->resetting)
			continue;

		/* Increase the count of Tx desc error since
		 * it causes the queue reset.
		 */
		txq->desc_error++;
		txgbe_dev_save_tx_queue(hw, i);

		/* tx ring reset */
		wr32(hw, TXGBE_TDM_DESC_NONFATAL(i / 32),
			TXGBE_TDM_DESC_MASK(i % 32));

		if (txq->ops != NULL) {
			txq->ops->release_mbufs(txq);
			txq->ops->reset(txq);
		}

		txgbe_dev_store_tx_queue(hw, i);
		txgbe_dev_tx_queue_start(dev, i);
	}
}

void
txgbe_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo)
{
	struct txgbe_rx_queue *rxq;

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
txgbe_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo)
{
	struct txgbe_tx_queue *txq;

	txq = dev->data->tx_queues[queue_id];

	qinfo->nb_desc = txq->nb_tx_desc;

	qinfo->conf.tx_thresh.pthresh = txq->pthresh;
	qinfo->conf.tx_thresh.hthresh = txq->hthresh;
	qinfo->conf.tx_thresh.wthresh = txq->wthresh;

	qinfo->conf.tx_free_thresh = txq->tx_free_thresh;
	qinfo->conf.offloads = txq->offloads;
	qinfo->conf.tx_deferred_start = txq->tx_deferred_start;
}

/*
 * [VF] Initializes Receive Unit.
 */
int __rte_cold
txgbevf_dev_rx_init(struct rte_eth_dev *dev)
{
	struct txgbe_hw     *hw;
	struct txgbe_rx_queue *rxq;
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	uint64_t bus_addr;
	uint32_t srrctl, psrtype;
	uint16_t buf_size;
	uint16_t i;
	int ret;

	PMD_INIT_FUNC_TRACE();
	hw = TXGBE_DEV_HW(dev);

	if (rte_is_power_of_2(dev->data->nb_rx_queues) == 0) {
		PMD_INIT_LOG(ERR, "The number of Rx queue invalid, "
			"it should be power of 2");
		return -1;
	}

	if (dev->data->nb_rx_queues > hw->mac.max_rx_queues) {
		PMD_INIT_LOG(ERR, "The number of Rx queue invalid, "
			"it should be equal to or less than %d",
			hw->mac.max_rx_queues);
		return -1;
	}

	/*
	 * When the VF driver issues a TXGBE_VF_RESET request, the PF driver
	 * disables the VF receipt of packets if the PF MTU is > 1500.
	 * This is done to deal with limitations that imposes
	 * the PF and all VFs to share the same MTU.
	 * Then, the PF driver enables again the VF receipt of packet when
	 * the VF driver issues a TXGBE_VF_SET_LPE request.
	 * In the meantime, the VF device cannot be used, even if the VF driver
	 * and the Guest VM network stack are ready to accept packets with a
	 * size up to the PF MTU.
	 * As a work-around to this PF behaviour, force the call to
	 * txgbevf_rlpml_set_vf even if jumbo frames are not used. This way,
	 * VF packets received can work in all cases.
	 */
	if (txgbevf_rlpml_set_vf(hw,
	    (uint16_t)dev->data->mtu + TXGBE_ETH_OVERHEAD)) {
		PMD_INIT_LOG(ERR, "Set max packet length to %d failed.",
			     dev->data->mtu + TXGBE_ETH_OVERHEAD);
		return -EINVAL;
	}

	/*
	 * Assume no header split and no VLAN strip support
	 * on any Rx queue first .
	 */
	rxmode->offloads &= ~RTE_ETH_RX_OFFLOAD_VLAN_STRIP;

	/* Set PSR type for VF RSS according to max Rx queue */
	psrtype = TXGBE_VFPLCFG_PSRL4HDR |
		  TXGBE_VFPLCFG_PSRL4HDR |
		  TXGBE_VFPLCFG_PSRL2HDR |
		  TXGBE_VFPLCFG_PSRTUNHDR |
		  TXGBE_VFPLCFG_PSRTUNMAC;
	wr32(hw, TXGBE_VFPLCFG, TXGBE_VFPLCFG_PSR(psrtype));

	/* Setup RX queues */
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		/* Allocate buffers for descriptor rings */
		ret = txgbe_alloc_rx_queue_mbufs(rxq);
		if (ret)
			return ret;

		/* Setup the Base and Length of the Rx Descriptor Rings */
		bus_addr = rxq->rx_ring_phys_addr;

		wr32(hw, TXGBE_RXBAL(i),
				(uint32_t)(bus_addr & BIT_MASK32));
		wr32(hw, TXGBE_RXBAH(i),
				(uint32_t)(bus_addr >> 32));
		wr32(hw, TXGBE_RXRP(i), 0);
		wr32(hw, TXGBE_RXWP(i), 0);

		/* Configure the RXCFG register */
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
		buf_size = ROUND_UP(buf_size, 1 << 10);
		srrctl |= TXGBE_RXCFG_PKTLEN(buf_size);

		/*
		 * VF modification to write virtual function RXCFG register
		 */
		wr32(hw, TXGBE_RXCFG(i), srrctl);

		if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_SCATTER ||
		    /* It adds dual VLAN length for supporting dual VLAN */
		    (dev->data->mtu + TXGBE_ETH_OVERHEAD +
				2 * RTE_VLAN_HLEN) > buf_size) {
			if (!dev->data->scattered_rx)
				PMD_INIT_LOG(DEBUG, "forcing scatter mode");
			dev->data->scattered_rx = 1;
		}

		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
			rxmode->offloads |= RTE_ETH_RX_OFFLOAD_VLAN_STRIP;
	}

	/*
	 * Device configured with multiple RX queues.
	 */
	txgbe_dev_mq_rx_configure(dev);

	txgbe_set_rx_function(dev);

	return 0;
}

/*
 * [VF] Initializes Transmit Unit.
 */
void __rte_cold
txgbevf_dev_tx_init(struct rte_eth_dev *dev)
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
		wr32(hw, TXGBE_TXBAL(i),
				(uint32_t)(bus_addr & BIT_MASK32));
		wr32(hw, TXGBE_TXBAH(i),
				(uint32_t)(bus_addr >> 32));
		wr32m(hw, TXGBE_TXCFG(i), TXGBE_TXCFG_BUFLEN_MASK,
			TXGBE_TXCFG_BUFLEN(txq->nb_tx_desc));
		/* Setup the HW Tx Head and TX Tail descriptor pointers */
		wr32(hw, TXGBE_TXRP(i), 0);
		wr32(hw, TXGBE_TXWP(i), 0);
	}
}

/*
 * [VF] Start Transmit and Receive Units.
 */
void __rte_cold
txgbevf_dev_rxtx_start(struct rte_eth_dev *dev)
{
	struct txgbe_hw     *hw;
	struct txgbe_tx_queue *txq;
	struct txgbe_rx_queue *rxq;
	uint32_t txdctl;
	uint32_t rxdctl;
	uint16_t i;
	int poll_ms;

	PMD_INIT_FUNC_TRACE();
	hw = TXGBE_DEV_HW(dev);

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		/* Setup Transmit Threshold Registers */
		wr32m(hw, TXGBE_TXCFG(txq->reg_idx),
		      TXGBE_TXCFG_HTHRESH_MASK |
		      TXGBE_TXCFG_WTHRESH_MASK,
		      TXGBE_TXCFG_HTHRESH(txq->hthresh) |
		      TXGBE_TXCFG_WTHRESH(txq->wthresh));
	}

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		wr32m(hw, TXGBE_TXCFG(i), TXGBE_TXCFG_ENA, TXGBE_TXCFG_ENA);

		poll_ms = 10;
		/* Wait until TX Enable ready */
		do {
			rte_delay_ms(1);
			txdctl = rd32(hw, TXGBE_TXCFG(i));
		} while (--poll_ms && !(txdctl & TXGBE_TXCFG_ENA));
		if (!poll_ms)
			PMD_INIT_LOG(ERR, "Could not enable Tx Queue %d", i);
		else
			dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	}
	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];

		wr32m(hw, TXGBE_RXCFG(i), TXGBE_RXCFG_ENA, TXGBE_RXCFG_ENA);

		/* Wait until RX Enable ready */
		poll_ms = 10;
		do {
			rte_delay_ms(1);
			rxdctl = rd32(hw, TXGBE_RXCFG(i));
		} while (--poll_ms && !(rxdctl & TXGBE_RXCFG_ENA));
		if (!poll_ms)
			PMD_INIT_LOG(ERR, "Could not enable Rx Queue %d", i);
		else
			dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
		rte_wmb();
		wr32(hw, TXGBE_RXWP(i), rxq->nb_rx_desc - 1);
	}
}

int
txgbe_rss_conf_init(struct txgbe_rte_flow_rss_conf *out,
		    const struct rte_flow_action_rss *in)
{
	if (in->key_len > RTE_DIM(out->key) ||
	    in->queue_num > RTE_DIM(out->queue))
		return -EINVAL;
	out->conf = (struct rte_flow_action_rss){
		.func = in->func,
		.level = in->level,
		.types = in->types,
		.key_len = in->key_len,
		.queue_num = in->queue_num,
		.key = memcpy(out->key, in->key, in->key_len),
		.queue = memcpy(out->queue, in->queue,
				sizeof(*in->queue) * in->queue_num),
	};
	return 0;
}

int
txgbe_action_rss_same(const struct rte_flow_action_rss *comp,
		      const struct rte_flow_action_rss *with)
{
	return (comp->func == with->func &&
		comp->level == with->level &&
		comp->types == with->types &&
		comp->key_len == with->key_len &&
		comp->queue_num == with->queue_num &&
		!memcmp(comp->key, with->key, with->key_len) &&
		!memcmp(comp->queue, with->queue,
			sizeof(*with->queue) * with->queue_num));
}

int
txgbe_config_rss_filter(struct rte_eth_dev *dev,
		struct txgbe_rte_flow_rss_conf *conf, bool add)
{
	struct txgbe_hw *hw;
	uint32_t reta;
	uint16_t i;
	uint16_t j;
	uint16_t queue;
	struct rte_eth_rss_conf rss_conf = {
		.rss_key = conf->conf.key_len ?
			(void *)(uintptr_t)conf->conf.key : NULL,
		.rss_key_len = conf->conf.key_len,
		.rss_hf = conf->conf.types,
	};
	struct txgbe_filter_info *filter_info = TXGBE_DEV_FILTER(dev);

	PMD_INIT_FUNC_TRACE();
	hw = TXGBE_DEV_HW(dev);

	if (!add) {
		if (txgbe_action_rss_same(&filter_info->rss_info.conf,
					  &conf->conf)) {
			txgbe_rss_disable(dev);
			memset(&filter_info->rss_info, 0,
				sizeof(struct txgbe_rte_flow_rss_conf));
			return 0;
		}
		return -EINVAL;
	}

	if (filter_info->rss_info.conf.queue_num)
		return -EINVAL;
	/* Fill in redirection table
	 * The byte-swap is needed because NIC registers are in
	 * little-endian order.
	 */
	reta = 0;
	for (i = 0, j = 0; i < RTE_ETH_RSS_RETA_SIZE_128; i++, j++) {
		if (j == conf->conf.queue_num)
			j = 0;
		if (RTE_ETH_DEV_SRIOV(dev).active)
			queue = RTE_ETH_DEV_SRIOV(dev).def_pool_q_idx +
				conf->conf.queue[j];
		else
			queue = conf->conf.queue[j];
		reta = (reta >> 8) | LS32(queue, 24, 0xFF);
		if ((i & 3) == 3)
			wr32at(hw, TXGBE_REG_RSSTBL, i >> 2, reta);
	}

	/* Configure the RSS key and the RSS protocols used to compute
	 * the RSS hash of input packets.
	 */
	if ((rss_conf.rss_hf & TXGBE_RSS_OFFLOAD_ALL) == 0) {
		txgbe_rss_disable(dev);
		return 0;
	}
	if (rss_conf.rss_key == NULL)
		rss_conf.rss_key = rss_intel_key; /* Default hash key */
	txgbe_dev_rss_hash_update(dev, &rss_conf);

	if (txgbe_rss_conf_init(&filter_info->rss_info, &conf->conf))
		return -EINVAL;

	return 0;
}

/* Stubs needed for linkage when RTE_ARCH_PPC_64, RTE_ARCH_RISCV or
 * RTE_ARCH_LOONGARCH is set.
 */
#if defined(RTE_ARCH_PPC_64) || defined(RTE_ARCH_RISCV) || \
	defined(RTE_ARCH_LOONGARCH)
int
txgbe_rx_vec_dev_conf_condition_check(__rte_unused struct rte_eth_dev *dev)
{
	return -1;
}

uint16_t
txgbe_recv_pkts_vec(__rte_unused void *rx_queue,
		    __rte_unused struct rte_mbuf **rx_pkts,
		    __rte_unused uint16_t nb_pkts)
{
	return 0;
}

uint16_t
txgbe_recv_scattered_pkts_vec(__rte_unused void *rx_queue,
			      __rte_unused struct rte_mbuf **rx_pkts,
			      __rte_unused uint16_t nb_pkts)
{
	return 0;
}

int
txgbe_rxq_vec_setup(__rte_unused struct txgbe_rx_queue *rxq)
{
	return -1;
}

uint16_t
txgbe_xmit_fixed_burst_vec(__rte_unused void *tx_queue,
			   __rte_unused struct rte_mbuf **tx_pkts,
			   __rte_unused uint16_t nb_pkts)
{
	return 0;
}

int
txgbe_txq_vec_setup(__rte_unused struct txgbe_tx_queue *txq)
{
	return -1;
}

void
txgbe_rx_queue_release_mbufs_vec(__rte_unused struct txgbe_rx_queue *rxq)
{
}
#endif
