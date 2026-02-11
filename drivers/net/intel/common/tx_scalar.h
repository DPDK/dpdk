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
	struct ci_tx_entry *sw_ring = txq->sw_ring;
	volatile struct ci_tx_desc *txd = txq->ci_tx_ring;
	uint16_t last_desc_cleaned = txq->last_desc_cleaned;
	uint16_t nb_tx_desc = txq->nb_tx_desc;
	uint16_t desc_to_clean_to;
	uint16_t nb_tx_to_clean;

	/* Determine the last descriptor needing to be cleaned */
	desc_to_clean_to = (uint16_t)(last_desc_cleaned + txq->tx_rs_thresh);
	if (desc_to_clean_to >= nb_tx_desc)
		desc_to_clean_to = (uint16_t)(desc_to_clean_to - nb_tx_desc);

	/* Check if descriptor is done */
	desc_to_clean_to = sw_ring[desc_to_clean_to].last_id;
	if ((txd[desc_to_clean_to].cmd_type_offset_bsz & rte_cpu_to_le_64(CI_TXD_QW1_DTYPE_M)) !=
			rte_cpu_to_le_64(CI_TX_DESC_DTYPE_DESC_DONE))
		return -1;

	/* Figure out how many descriptors will be cleaned */
	if (last_desc_cleaned > desc_to_clean_to)
		nb_tx_to_clean = (uint16_t)((nb_tx_desc - last_desc_cleaned) + desc_to_clean_to);
	else
		nb_tx_to_clean = (uint16_t)(desc_to_clean_to - last_desc_cleaned);

	/* The last descriptor to clean is done, so that means all the
	 * descriptors from the last descriptor that was cleaned
	 * up to the last descriptor with the RS bit set
	 * are done. Only reset the threshold descriptor.
	 */
	txd[desc_to_clean_to].cmd_type_offset_bsz = 0;

	/* Update the txq to reflect the last descriptor that was cleaned */
	txq->last_desc_cleaned = desc_to_clean_to;
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free + nb_tx_to_clean);

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
	     ci_get_ctx_desc_fn get_ctx_desc,
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
		uint64_t cd_qw0 = 0, cd_qw1 = 0;
		tx_pkt = *tx_pkts++;

		ol_flags = tx_pkt->ol_flags;
		td_cmd = CI_TX_DESC_CMD_ICRC;
		td_tag = 0;
		l2_len = ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK ?
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

		/* The number of descriptors that must be allocated for
		 * a packet equals to the number of the segments of that
		 * packet plus the number of context descriptor if needed.
		 * Recalculate the needed tx descs when TSO enabled in case
		 * the mbuf data size exceeds max data size that hw allows
		 * per tx desc.
		 */
		if (ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG))
			nb_used = (uint16_t)(ci_calc_pkt_desc(tx_pkt) + nb_ctx);
		else
			nb_used = (uint16_t)(tx_pkt->nb_segs + nb_ctx);
		tx_last = (uint16_t)(tx_id + nb_used - 1);

		/* Circular ring */
		if (tx_last >= txq->nb_tx_desc)
			tx_last = (uint16_t)(tx_last - txq->nb_tx_desc);

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

		/* Descriptor based VLAN insertion */
		if (ol_flags & (RTE_MBUF_F_TX_VLAN | RTE_MBUF_F_TX_QINQ)) {
			td_cmd |= CI_TX_DESC_CMD_IL2TAG1;
			td_tag = tx_pkt->vlan_tci;
		}

		/* Enable checksum offloading */
		if (ol_flags & CI_TX_CKSUM_OFFLOAD_MASK)
			ci_txd_enable_checksum(ol_flags, &td_cmd,
						&td_offset, tx_offload);

		if (nb_ctx) {
			/* Setup TX context descriptor if required */
			uint64_t *ctx_txd = RTE_CAST_PTR(uint64_t *, &ci_tx_ring[tx_id]);

			txn = &sw_ring[txe->next_id];
			RTE_MBUF_PREFETCH_TO_FREE(txn->mbuf);
			if (txe->mbuf) {
				rte_pktmbuf_free_seg(txe->mbuf);
				txe->mbuf = NULL;
			}

			ctx_txd[0] = cd_qw0;
			ctx_txd[1] = cd_qw1;

			txe->last_id = tx_last;
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
			slen = m_seg->data_len;
			buf_dma_addr = rte_mbuf_data_iova(m_seg);

			while ((ol_flags & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG)) &&
					unlikely(slen > CI_MAX_DATA_PER_TXD)) {
				txd->buffer_addr = rte_cpu_to_le_64(buf_dma_addr);
				txd->cmd_type_offset_bsz = rte_cpu_to_le_64(CI_TX_DESC_DTYPE_DATA |
					((uint64_t)td_cmd << CI_TXD_QW1_CMD_S) |
					((uint64_t)td_offset << CI_TXD_QW1_OFFSET_S) |
					((uint64_t)CI_MAX_DATA_PER_TXD << CI_TXD_QW1_TX_BUF_SZ_S) |
					((uint64_t)td_tag << CI_TXD_QW1_L2TAG1_S));

				buf_dma_addr += CI_MAX_DATA_PER_TXD;
				slen -= CI_MAX_DATA_PER_TXD;

				txe->last_id = tx_last;
				tx_id = txe->next_id;
				txe = txn;
				txd = &ci_tx_ring[tx_id];
				txn = &sw_ring[txe->next_id];
			}

			txd->buffer_addr = rte_cpu_to_le_64(buf_dma_addr);
			txd->cmd_type_offset_bsz = rte_cpu_to_le_64(CI_TX_DESC_DTYPE_DATA |
				((uint64_t)td_cmd << CI_TXD_QW1_CMD_S) |
				((uint64_t)td_offset << CI_TXD_QW1_OFFSET_S) |
				((uint64_t)slen << CI_TXD_QW1_TX_BUF_SZ_S) |
				((uint64_t)td_tag << CI_TXD_QW1_L2TAG1_S));

			txe->last_id = tx_last;
			tx_id = txe->next_id;
			txe = txn;
			m_seg = m_seg->next;
		} while (m_seg);

		/* fill the last descriptor with End of Packet (EOP) bit */
		td_cmd |= CI_TX_DESC_CMD_EOP;
		txq->nb_tx_used = (uint16_t)(txq->nb_tx_used + nb_used);
		txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_used);

		/* set RS bit on the last descriptor of one packet */
		if (txq->nb_tx_used >= txq->tx_rs_thresh) {
			td_cmd |= CI_TX_DESC_CMD_RS;

			/* Update txq RS bit counters */
			txq->nb_tx_used = 0;
		}
		txd->cmd_type_offset_bsz |=
				rte_cpu_to_le_64(((uint64_t)td_cmd) << CI_TXD_QW1_CMD_S);

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
