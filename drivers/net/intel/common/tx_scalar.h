/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Intel Corporation
 */

#ifndef _COMMON_INTEL_TX_SCALAR_H_
#define _COMMON_INTEL_TX_SCALAR_H_

#include <stdint.h>
#include <rte_io.h>
#include <rte_byteorder.h>

/* depends on common Tx definitions. */
#include "tx.h"

static inline void
write_txd(volatile void *txd, uint64_t qw0, uint64_t qw1)
{
	/* we use an aligned structure and cast away the volatile to allow the compiler
	 * to opportunistically optimize the two 64-bit writes as a single 128-bit write.
	 */
	struct __rte_aligned(16) txdesc {
		uint64_t qw0, qw1;
	} *txdesc = RTE_CAST_PTR(struct txdesc *, txd);
	txdesc->qw0 = rte_cpu_to_le_64(qw0);
	txdesc->qw1 = rte_cpu_to_le_64(qw1);
}

/* Fill hardware descriptor ring with mbuf data */
static inline void
ci_tx_fill_hw_ring(struct ci_tx_queue *txq, struct rte_mbuf **pkts,
		   uint16_t nb_pkts)
{
	volatile struct ci_tx_desc *txdp = &txq->ci_tx_ring[txq->tx_tail];
	struct ci_tx_entry *txep = &txq->sw_ring[txq->tx_tail];
	const int N_PER_LOOP = 4;
	const int N_PER_LOOP_MASK = N_PER_LOOP - 1;
	int mainpart, leftover;
	int i, j;

	mainpart = nb_pkts & ((uint32_t)~N_PER_LOOP_MASK);
	leftover = nb_pkts & ((uint32_t)N_PER_LOOP_MASK);
	for (i = 0; i < mainpart; i += N_PER_LOOP) {
		for (j = 0; j < N_PER_LOOP; ++j)
			(txep + i + j)->mbuf = *(pkts + i + j);
		for (j = 0; j < N_PER_LOOP; ++j)
			write_txd(txdp + i + j, rte_mbuf_data_iova(*(pkts + i + j)),
				CI_TX_DESC_DTYPE_DATA |
				((uint64_t)CI_TX_DESC_CMD_DEFAULT << CI_TXD_QW1_CMD_S) |
				((uint64_t)(*(pkts + i + j))->data_len << CI_TXD_QW1_TX_BUF_SZ_S));
	}

	if (unlikely(leftover > 0)) {
		for (i = 0; i < leftover; ++i) {
			uint16_t idx = mainpart + i;
			(txep + idx)->mbuf = *(pkts + idx);
			write_txd(txdp + idx, rte_mbuf_data_iova(*(pkts + idx)),
				CI_TX_DESC_DTYPE_DATA |
				((uint64_t)CI_TX_DESC_CMD_DEFAULT << CI_TXD_QW1_CMD_S) |
				((uint64_t)(*(pkts + idx))->data_len << CI_TXD_QW1_TX_BUF_SZ_S));

		}
	}
}

/* Free transmitted mbufs from descriptor ring with bulk freeing for Tx simple path */
static __rte_always_inline int
ci_tx_free_bufs(struct ci_tx_queue *txq)
{
	const uint16_t rs_thresh = txq->tx_rs_thresh;
	const uint16_t k = RTE_ALIGN_FLOOR(rs_thresh, CI_TX_MAX_FREE_BUF_SZ);
	const uint16_t m = rs_thresh % CI_TX_MAX_FREE_BUF_SZ;
	struct rte_mbuf *free[CI_TX_MAX_FREE_BUF_SZ];
	struct ci_tx_entry *txep;

	if ((txq->ci_tx_ring[txq->tx_next_dd].cmd_type_offset_bsz &
			rte_cpu_to_le_64(CI_TXD_QW1_DTYPE_M)) !=
			rte_cpu_to_le_64(CI_TX_DESC_DTYPE_DESC_DONE))
		return 0;

	txep = &txq->sw_ring[txq->tx_next_dd - (rs_thresh - 1)];

	struct rte_mempool *fast_free_mp =
			likely(txq->fast_free_mp != (void *)UINTPTR_MAX) ?
				txq->fast_free_mp :
				(txq->fast_free_mp = txep[0].mbuf->pool);

	if (fast_free_mp) {
		if (k) {
			for (uint16_t j = 0; j != k; j += CI_TX_MAX_FREE_BUF_SZ) {
				for (uint16_t i = 0; i < CI_TX_MAX_FREE_BUF_SZ; ++i, ++txep) {
					free[i] = txep->mbuf;
					txep->mbuf = NULL;
				}
				rte_mbuf_raw_free_bulk(fast_free_mp, free, CI_TX_MAX_FREE_BUF_SZ);
			}
		}

		if (m) {
			for (uint16_t i = 0; i < m; ++i, ++txep) {
				free[i] = txep->mbuf;
				txep->mbuf = NULL;
			}
			rte_mbuf_raw_free_bulk(fast_free_mp, free, m);
		}
	} else {
		for (uint16_t i = 0; i < rs_thresh; ++i, ++txep)
			rte_prefetch0((txep + i)->mbuf);

		for (uint16_t i = 0; i < rs_thresh; ++i, ++txep) {
			rte_pktmbuf_free_seg(txep->mbuf);
			txep->mbuf = NULL;
		}
	}

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + rs_thresh);
	txq->tx_next_dd = (uint16_t)(txq->tx_next_dd + rs_thresh);
	if (txq->tx_next_dd >= txq->nb_tx_desc)
		txq->tx_next_dd = (uint16_t)(rs_thresh - 1);

	return rs_thresh;
}

/* Simple burst transmit for descriptor-based simple Tx path
 *
 * Transmits a burst of packets by filling hardware descriptors with mbuf
 * data. Handles ring wrap-around and RS bit management. Performs descriptor
 * cleanup when tx_free_thresh is reached.
 *
 * Returns: number of packets transmitted
 */
static inline uint16_t
ci_xmit_burst_simple(struct ci_tx_queue *txq,
		     struct rte_mbuf **tx_pkts,
		     uint16_t nb_pkts)
{
	volatile struct ci_tx_desc *txr = txq->ci_tx_ring;
	uint16_t n = 0;

	/**
	 * Begin scanning the H/W ring for done descriptors when the number
	 * of available descriptors drops below tx_free_thresh. For each done
	 * descriptor, free the associated buffer.
	 */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		ci_tx_free_bufs(txq);

	/* Use available descriptor only */
	nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(!nb_pkts))
		return 0;

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);
	if ((txq->tx_tail + nb_pkts) > txq->nb_tx_desc) {
		n = (uint16_t)(txq->nb_tx_desc - txq->tx_tail);
		ci_tx_fill_hw_ring(txq, tx_pkts, n);
		txr[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)CI_TX_DESC_CMD_RS) <<
					  CI_TXD_QW1_CMD_S);
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);
		txq->tx_tail = 0;
	}

	/* Fill hardware descriptor ring with mbuf data */
	ci_tx_fill_hw_ring(txq, tx_pkts + n, (uint16_t)(nb_pkts - n));
	txq->tx_tail = (uint16_t)(txq->tx_tail + (nb_pkts - n));

	/* Determine if RS bit needs to be set */
	if (txq->tx_tail > txq->tx_next_rs) {
		txr[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)CI_TX_DESC_CMD_RS) <<
					  CI_TXD_QW1_CMD_S);
		txq->tx_next_rs =
			(uint16_t)(txq->tx_next_rs + txq->tx_rs_thresh);
		if (txq->tx_next_rs >= txq->nb_tx_desc)
			txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);
	}

	if (txq->tx_tail >= txq->nb_tx_desc)
		txq->tx_tail = 0;

	/* Update the tx tail register */
	rte_write32_wc((uint32_t)txq->tx_tail, txq->qtx_tail);

	return nb_pkts;
}

static __rte_always_inline uint16_t
ci_xmit_pkts_simple(struct ci_tx_queue *txq,
		     struct rte_mbuf **tx_pkts,
		     uint16_t nb_pkts)
{
	uint16_t nb_tx = 0;

	if (likely(nb_pkts <= CI_TX_MAX_BURST))
		return ci_xmit_burst_simple(txq, tx_pkts, nb_pkts);

	while (nb_pkts) {
		uint16_t ret, num = RTE_MIN(nb_pkts, CI_TX_MAX_BURST);

		ret = ci_xmit_burst_simple(txq, &tx_pkts[nb_tx], num);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}

/*
 * Common transmit descriptor cleanup function for Intel drivers.
 *
 * Returns:
 *   0 on success
 *  -1 if cleanup cannot proceed (descriptors not yet processed by HW)
 */
static __rte_always_inline int
ci_tx_xmit_cleanup(struct ci_tx_queue *txq)
{
	volatile struct ci_tx_desc *txd = txq->ci_tx_ring;
	const uint16_t last_desc_cleaned = txq->last_desc_cleaned;
	const uint16_t nb_tx_desc = txq->nb_tx_desc;

	/* Calculate where the next descriptor write-back will occur */
	const uint16_t rs_idx = (last_desc_cleaned == nb_tx_desc - 1) ?
			0 :
			(last_desc_cleaned + 1) >> txq->log2_rs_thresh;
	uint16_t desc_to_clean_to = (rs_idx << txq->log2_rs_thresh) + (txq->tx_rs_thresh - 1);

	/* Check if descriptor is done  */
	if ((txd[txq->rs_last_id[rs_idx]].cmd_type_offset_bsz &
			rte_cpu_to_le_64(CI_TXD_QW1_DTYPE_M)) !=
				rte_cpu_to_le_64(CI_TX_DESC_DTYPE_DESC_DONE))
		return -1;

	/* Update the txq to reflect the last descriptor that was cleaned */
	txq->last_desc_cleaned = desc_to_clean_to;
	txq->nb_tx_free += txq->tx_rs_thresh;

	return 0;
}

/* Common checksum enable function for Intel drivers (ice, i40e, etc.) */
static inline void
ci_txd_enable_checksum(uint64_t ol_flags,
		       uint32_t *td_cmd,
		       uint32_t *td_offset,
		       union ci_tx_offload tx_offload)
{
	/* Enable L3 checksum offloads */
	if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM) {
		*td_cmd |= CI_TX_DESC_CMD_IIPT_IPV4_CSUM;
		*td_offset |= (tx_offload.l3_len >> 2) << CI_TX_DESC_LEN_IPLEN_S;
	} else if (ol_flags & RTE_MBUF_F_TX_IPV4) {
		*td_cmd |= CI_TX_DESC_CMD_IIPT_IPV4;
		*td_offset |= (tx_offload.l3_len >> 2) << CI_TX_DESC_LEN_IPLEN_S;
	} else if (ol_flags & RTE_MBUF_F_TX_IPV6) {
		*td_cmd |= CI_TX_DESC_CMD_IIPT_IPV6;
		*td_offset |= (tx_offload.l3_len >> 2) << CI_TX_DESC_LEN_IPLEN_S;
	}

	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		*td_cmd |= CI_TX_DESC_CMD_L4T_EOFT_TCP;
		*td_offset |= (tx_offload.l4_len >> 2) << CI_TX_DESC_LEN_L4_LEN_S;
		return;
	}

	if (ol_flags & RTE_MBUF_F_TX_UDP_SEG) {
		*td_cmd |= CI_TX_DESC_CMD_L4T_EOFT_UDP;
		*td_offset |= (tx_offload.l4_len >> 2) << CI_TX_DESC_LEN_L4_LEN_S;
		return;
	}

	/* Enable L4 checksum offloads */
	switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
	case RTE_MBUF_F_TX_TCP_CKSUM:
		*td_cmd |= CI_TX_DESC_CMD_L4T_EOFT_TCP;
		*td_offset |= (sizeof(struct rte_tcp_hdr) >> 2) << CI_TX_DESC_LEN_L4_LEN_S;
		break;
	case RTE_MBUF_F_TX_SCTP_CKSUM:
		*td_cmd |= CI_TX_DESC_CMD_L4T_EOFT_SCTP;
		*td_offset |= (sizeof(struct rte_sctp_hdr) >> 2) << CI_TX_DESC_LEN_L4_LEN_S;
		break;
	case RTE_MBUF_F_TX_UDP_CKSUM:
		*td_cmd |= CI_TX_DESC_CMD_L4T_EOFT_UDP;
		*td_offset |= (sizeof(struct rte_udp_hdr) >> 2) << CI_TX_DESC_LEN_L4_LEN_S;
		break;
	default:
		break;
	}
}

static inline uint16_t
ci_div_roundup16(uint16_t x, uint16_t y)
{
	return (uint16_t)((x + y - 1) / y);
}

/* Calculate the number of TX descriptors needed for each pkt */
static inline uint16_t
ci_calc_pkt_desc(const struct rte_mbuf *tx_pkt)
{
	uint16_t count = 0;

	while (tx_pkt != NULL) {
		count += ci_div_roundup16(tx_pkt->data_len, CI_MAX_DATA_PER_TXD);
		tx_pkt = tx_pkt->next;
	}

	return count;
}

typedef uint16_t (*ci_get_ctx_desc_fn)(uint64_t ol_flags, const struct rte_mbuf *mbuf,
		const union ci_tx_offload *tx_offload, const struct ci_tx_queue *txq,
		uint64_t *qw0, uint64_t *qw1);

/* gets IPsec descriptor information and returns number of descriptors needed (0 or 1) */
typedef uint16_t (*get_ipsec_desc_t)(const struct rte_mbuf *mbuf,
		const struct ci_tx_queue *txq,
		void **ipsec_metadata,
		uint64_t *qw0,
		uint64_t *qw1);
/* calculates segment length for IPsec + TSO combinations */
typedef uint16_t (*calc_ipsec_segment_len_t)(const struct rte_mbuf *mb_seg,
		uint64_t ol_flags,
		const void *ipsec_metadata,
		uint16_t tlen);

/** IPsec descriptor operations for drivers that support inline IPsec crypto. */
struct ci_ipsec_ops {
	get_ipsec_desc_t get_ipsec_desc;
	calc_ipsec_segment_len_t calc_segment_len;
};

/* gets current timestamp tail index */
typedef uint16_t (*get_ts_tail_t)(struct ci_tx_queue *txq);
/* writes a timestamp descriptor and returns new tail index */
typedef uint16_t (*write_ts_desc_t)(struct ci_tx_queue *txq, struct rte_mbuf *mbuf,
		uint16_t tx_id, uint16_t ts_id);
/* writes a timestamp tail index - doorbell */
typedef void (*write_ts_tail_t)(struct ci_tx_queue *txq, uint16_t ts_id);

struct ci_timestamp_queue_fns {
	get_ts_tail_t get_ts_tail;
	write_ts_desc_t write_ts_desc;
	write_ts_tail_t write_ts_tail;
};

static inline uint16_t
ci_xmit_pkts(struct ci_tx_queue *txq,
	     struct rte_mbuf **tx_pkts,
	     uint16_t nb_pkts,
	     enum ci_tx_l2tag1_field l2tag1_field,
	     ci_get_ctx_desc_fn get_ctx_desc,
	     const struct ci_ipsec_ops *ipsec_ops,
	     const struct ci_timestamp_queue_fns *ts_fns)
{
	volatile struct ci_tx_desc *ci_tx_ring;
	volatile struct ci_tx_desc *txd;
	struct ci_tx_entry *sw_ring;
	struct ci_tx_entry *txe, *txn;
	struct rte_mbuf *tx_pkt;
	struct rte_mbuf *m_seg;
	uint16_t tx_id;
	uint16_t ts_id = -1;
	uint16_t nb_tx;
	uint16_t nb_used;
	uint16_t nb_ctx;
	uint32_t td_cmd = 0;
	uint32_t td_offset = 0;
	uint32_t td_tag = 0;
	uint16_t tx_last;
	uint16_t slen;
	uint16_t l2_len;
	uint64_t buf_dma_addr;
	uint64_t ol_flags;
	union ci_tx_offload tx_offload = {0};

	sw_ring = txq->sw_ring;
	ci_tx_ring = txq->ci_tx_ring;
	tx_id = txq->tx_tail;
	txe = &sw_ring[tx_id];

	if (ts_fns != NULL)
		ts_id = ts_fns->get_ts_tail(txq);

	/* Check if the descriptor ring needs to be cleaned. */
	if (txq->nb_tx_free < txq->tx_free_thresh)
		(void)ci_tx_xmit_cleanup(txq);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		void *ipsec_md = NULL;
		uint16_t nb_ipsec = 0;
		uint64_t ipsec_qw0 = 0, ipsec_qw1 = 0;
		uint64_t cd_qw0 = 0, cd_qw1 = 0;
		uint16_t pkt_rs_idx;
		tx_pkt = *tx_pkts++;

		ol_flags = tx_pkt->ol_flags;
		td_cmd = CI_TX_DESC_CMD_ICRC;
		td_tag = 0;
		l2_len = (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK &&
					!(ol_flags & RTE_MBUF_F_TX_SEC_OFFLOAD)) ?
				tx_pkt->outer_l2_len : tx_pkt->l2_len;
		td_offset = (l2_len >> 1) << CI_TX_DESC_LEN_MACLEN_S;


		tx_offload.l2_len = tx_pkt->l2_len;
		tx_offload.l3_len = tx_pkt->l3_len;
		tx_offload.outer_l2_len = tx_pkt->outer_l2_len;
		tx_offload.outer_l3_len = tx_pkt->outer_l3_len;
		tx_offload.l4_len = tx_pkt->l4_len;
		tx_offload.tso_segsz = tx_pkt->tso_segsz;

		/* Calculate the number of context descriptors needed. */
		nb_ctx = get_ctx_desc(ol_flags, tx_pkt, &tx_offload, txq, &cd_qw0, &cd_qw1);

		/* Get IPsec descriptor information if IPsec ops provided */
		if (ipsec_ops != NULL)
			nb_ipsec = ipsec_ops->get_ipsec_desc(tx_pkt, txq, &ipsec_md,
					&ipsec_qw0, &ipsec_qw1);

		/* The number of descriptors that must be allocated for
		 * a packet equals to the number of the segments of that
		 * packet plus the number of context and IPsec descriptors if needed.
		 * Recalculate the needed tx descs when TSO enabled in case
		 * the mbuf data size exceeds max data size that hw allows
		 * per tx desc.
		 */
		if (ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG))
			nb_used = (uint16_t)(ci_calc_pkt_desc(tx_pkt) + nb_ctx + nb_ipsec);
		else
			nb_used = (uint16_t)(tx_pkt->nb_segs + nb_ctx + nb_ipsec);
		tx_last = (uint16_t)(tx_id + nb_used - 1);

		/* Circular ring */
		if (tx_last >= txq->nb_tx_desc)
			tx_last = (uint16_t)(tx_last - txq->nb_tx_desc);

		/* Track the RS threshold bucket at packet start */
		pkt_rs_idx = (uint16_t)(tx_id >> txq->log2_rs_thresh);

		if (nb_used > txq->nb_tx_free) {
			if (ci_tx_xmit_cleanup(txq) != 0) {
				if (nb_tx == 0)
					return 0;
				goto end_of_tx;
			}
			if (unlikely(nb_used > txq->tx_rs_thresh)) {
				while (nb_used > txq->nb_tx_free) {
					if (ci_tx_xmit_cleanup(txq) != 0) {
						if (nb_tx == 0)
							return 0;
						goto end_of_tx;
					}
				}
			}
		}

		/* Descriptor based VLAN/QinQ insertion */
		/* for single vlan offload, only insert in data desc with VLAN_IN_L2TAG1 is set
		 * for qinq offload, we always put inner tag in L2Tag1
		 */
		if (((ol_flags & RTE_MBUF_F_TX_VLAN) && l2tag1_field == CI_VLAN_IN_L2TAG1) ||
				(ol_flags & RTE_MBUF_F_TX_QINQ)) {
			td_cmd |= CI_TX_DESC_CMD_IL2TAG1;
			td_tag = tx_pkt->vlan_tci;
		}

		/* Enable checksum offloading */
		if (ol_flags & CI_TX_CKSUM_OFFLOAD_MASK)
			ci_txd_enable_checksum(ol_flags, &td_cmd,
						&td_offset, tx_offload);

		/* special case for single descriptor packet, without TSO offload */
		if (nb_used == 1 &&
				(ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG)) == 0) {
			txd = &ci_tx_ring[tx_id];
			tx_id = txe->next_id;

			if (txe->mbuf)
				rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = tx_pkt;
			/* Setup TX Descriptor */
			td_cmd |= CI_TX_DESC_CMD_EOP;
			const uint64_t cmd_type_offset_bsz = CI_TX_DESC_DTYPE_DATA |
				((uint64_t)td_cmd << CI_TXD_QW1_CMD_S) |
				((uint64_t)td_offset << CI_TXD_QW1_OFFSET_S) |
				((uint64_t)tx_pkt->data_len << CI_TXD_QW1_TX_BUF_SZ_S) |
				((uint64_t)td_tag << CI_TXD_QW1_L2TAG1_S);
			write_txd(txd, rte_mbuf_data_iova(tx_pkt), cmd_type_offset_bsz);

			txe = &sw_ring[tx_id];
			goto end_pkt;
		}

		if (nb_ctx) {
			/* Setup TX context descriptor if required */
			uint64_t *ctx_txd = RTE_CAST_PTR(uint64_t *, &ci_tx_ring[tx_id]);

			txn = &sw_ring[txe->next_id];
			RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);
			if (txe->mbuf) {
				rte_pktmbuf_free_seg(txe->mbuf);
				txe->mbuf = NULL;
			}

			write_txd(ctx_txd, cd_qw0, cd_qw1);

			tx_id = txe->next_id;
			txe = txn;
		}

		if (ipsec_ops != NULL && nb_ipsec > 0) {
			/* Setup TX IPsec descriptor if required */
			uint64_t *ipsec_txd = RTE_CAST_PTR(uint64_t *, &ci_tx_ring[tx_id]);

			txn = &sw_ring[txe->next_id];
			RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);
			if (txe->mbuf) {
				rte_pktmbuf_free_seg(txe->mbuf);
				txe->mbuf = NULL;
			}

			ipsec_txd[0] = ipsec_qw0;
			ipsec_txd[1] = ipsec_qw1;

			tx_id = txe->next_id;
			txe = txn;
		}

		m_seg = tx_pkt;

		do {
			txd = &ci_tx_ring[tx_id];
			txn = &sw_ring[txe->next_id];

			if (txe->mbuf)
				rte_pktmbuf_free_seg(txe->mbuf);
			txe->mbuf = m_seg;

			/* Setup TX Descriptor */
			/* Calculate segment length, using IPsec callback if provided */
			if (ipsec_ops != NULL)
				slen = ipsec_ops->calc_segment_len(m_seg, ol_flags, ipsec_md, 0);
			else
				slen = m_seg->data_len;

			buf_dma_addr = rte_mbuf_data_iova(m_seg);

			while ((ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG)) &&
					unlikely(slen > CI_MAX_DATA_PER_TXD)) {
				const uint64_t cmd_type_offset_bsz = CI_TX_DESC_DTYPE_DATA |
					((uint64_t)td_cmd << CI_TXD_QW1_CMD_S) |
					((uint64_t)td_offset << CI_TXD_QW1_OFFSET_S) |
					((uint64_t)CI_MAX_DATA_PER_TXD << CI_TXD_QW1_TX_BUF_SZ_S) |
					((uint64_t)td_tag << CI_TXD_QW1_L2TAG1_S);
				write_txd(txd, buf_dma_addr, cmd_type_offset_bsz);

				buf_dma_addr += CI_MAX_DATA_PER_TXD;
				slen -= CI_MAX_DATA_PER_TXD;

				tx_id = txe->next_id;
				txe = txn;
				txd = &ci_tx_ring[tx_id];
				txn = &sw_ring[txe->next_id];
			}

			/* fill the last descriptor with End of Packet (EOP) bit */
			if (m_seg->next == NULL)
				td_cmd |= CI_TX_DESC_CMD_EOP;

			const uint64_t cmd_type_offset_bsz = CI_TX_DESC_DTYPE_DATA |
				((uint64_t)td_cmd << CI_TXD_QW1_CMD_S) |
				((uint64_t)td_offset << CI_TXD_QW1_OFFSET_S) |
				((uint64_t)slen << CI_TXD_QW1_TX_BUF_SZ_S) |
				((uint64_t)td_tag << CI_TXD_QW1_L2TAG1_S);
			write_txd(txd, buf_dma_addr, cmd_type_offset_bsz);

			tx_id = txe->next_id;
			txe = txn;
			m_seg = m_seg->next;
		} while (m_seg);
end_pkt:
		txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_used);

		/* Check if packet crosses into a new RS threshold bucket.
		 * The RS bit is set on the last descriptor when we move from one bucket to another.
		 * For example, with tx_rs_thresh=32 and a 5-descriptor packet using slots 30-34:
		 *   - pkt_rs_idx = 30 >> 5 = 0 (started in bucket 0)
		 *   - tx_last = 34, so 35 >> 5 = 1 (next packet is in bucket 1)
		 *   - Since 0 != 1, set RS bit on descriptor 34, and record rs_last_id[0] = 34
		 */
		uint16_t next_rs_idx = ((tx_last + 1) >> txq->log2_rs_thresh);

		if (next_rs_idx != pkt_rs_idx) {
			/* Packet crossed into a new bucket - set RS bit on last descriptor */
			txd->cmd_type_offset_bsz |=
					rte_cpu_to_le_64(CI_TX_DESC_CMD_RS << CI_TXD_QW1_CMD_S);

			/* Record the last descriptor ID for the bucket we're leaving */
			txq->rs_last_id[pkt_rs_idx] = tx_last;
		}

		if (ts_fns != NULL)
			ts_id = ts_fns->write_ts_desc(txq, tx_pkt, tx_id, ts_id);
	}
end_of_tx:
	/* update Tail register */
	if (ts_fns != NULL)
		ts_fns->write_ts_tail(txq, ts_id);
	else
		rte_write32_wc(tx_id, txq->qtx_tail);
	txq->tx_tail = tx_id;

	return nb_tx;
}

#endif /* _COMMON_INTEL_TX_SCALAR_H_ */
