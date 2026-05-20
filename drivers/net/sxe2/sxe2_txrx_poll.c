/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <rte_common.h>
#include <rte_net.h>
#include <rte_vect.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <ethdev_driver.h>
#include "sxe2_osal.h"
#include "sxe2_txrx_common.h"
#include "sxe2_txrx_poll.h"
#include "sxe2_txrx.h"
#include "sxe2_queue.h"
#include "sxe2_ethdev.h"
#include "sxe2_common_log.h"

static __rte_always_inline int32_t
sxe2_tx_bufs_free(struct sxe2_tx_queue *txq)
{
	struct sxe2_tx_buffer *buffer;
	struct rte_mbuf *mbuf;
	struct rte_mbuf *mbuf_free_arr[SXE2_TX_FREE_BUFFER_SIZE_MAX];
	int32_t ret;
	uint32_t i;
	uint16_t rs_thresh;
	uint16_t free_num;
	if ((txq->desc_ring[txq->next_dd].wb.dd &
		     rte_cpu_to_le_64(SXE2_TX_DESC_DTYPE_MASK)) !=
		     rte_cpu_to_le_64(SXE2_TX_DESC_DTYPE_DESC_DONE)) {
		ret = 0;
		goto l_end;
	}
	rs_thresh = txq->rs_thresh;
	buffer = &txq->buffer_ring[txq->next_dd - rs_thresh + 1];
	if (txq->offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) {
		if (likely(rs_thresh <= SXE2_TX_FREE_BUFFER_SIZE_MAX)) {
			mbuf = buffer[0].mbuf;
			mbuf_free_arr[0] = mbuf;
			free_num = 1;
			for (i = 1; i < rs_thresh; ++i) {
				mbuf = buffer[i].mbuf;
				if (likely(mbuf->pool == mbuf_free_arr[0]->pool)) {
					mbuf_free_arr[free_num] = mbuf;
					free_num++;
				} else {
					rte_mempool_put_bulk(mbuf_free_arr[0]->pool,
								(void *)mbuf_free_arr, free_num);
					mbuf_free_arr[0] = mbuf;
					free_num = 1;
				}
			}
			rte_mempool_put_bulk(mbuf_free_arr[0]->pool,
						(void *)mbuf_free_arr, free_num);
		} else {
			for (i = 0; i < rs_thresh; ++i, ++buffer) {
				rte_mempool_put(buffer->mbuf->pool, buffer->mbuf);
				buffer->mbuf = NULL;
			}
		}
	} else {
		for (i = 0; i < rs_thresh; ++i, ++buffer) {
			mbuf = rte_pktmbuf_prefree_seg(buffer[i].mbuf);
				if (mbuf != NULL)
					rte_mempool_put(mbuf->pool, mbuf);
			buffer->mbuf = NULL;
		}
	}
	txq->desc_free_num += rs_thresh;
	txq->next_dd       += rs_thresh;
	if (txq->next_dd >= txq->ring_depth)
		txq->next_dd = rs_thresh - 1;
	ret = rs_thresh;
l_end:
	return ret;
}

static inline int32_t sxe2_tx_cleanup(struct sxe2_tx_queue *txq)
{
	int32_t ret = 0;
	volatile union sxe2_tx_data_desc *desc_ring = txq->desc_ring;
	struct sxe2_tx_buffer *buffer_ring = txq->buffer_ring;
	uint16_t ring_depth = txq->ring_depth;
	uint16_t next_clean = txq->next_clean;
	uint16_t clean_last;
	uint16_t clean_num;

	clean_last = next_clean + txq->rs_thresh;
	if (clean_last >= ring_depth)
		clean_last = clean_last - ring_depth;

	clean_last = buffer_ring[clean_last].last_id;
	if (rte_cpu_to_le_64(SXE2_TX_DESC_DTYPE_DESC_DONE) !=
		(txq->desc_ring[clean_last].wb.dd & rte_cpu_to_le_64(SXE2_TX_DESC_DTYPE_MASK))) {
		PMD_LOG_DEBUG(TX, "desc[%u] is not done.port_id=%u queue_id=%u val=0x%" PRIx64,
						 clean_last, txq->port_id,
			txq->queue_id, txq->desc_ring[clean_last].wb.dd);
		ret = -1;
		goto l_end;
	}

	if (clean_last > next_clean)
		clean_num = clean_last - next_clean;
	else
		clean_num = ring_depth - next_clean + clean_last;

	desc_ring[clean_last].wb.dd = 0;

	txq->next_clean = clean_last;
	txq->desc_free_num += clean_num;

	ret = 0;

l_end:
	return ret;
}

static __rte_always_inline uint32_t
sxe2_tx_pkt_data_desc_count(struct rte_mbuf *tx_pkt)
{
	struct rte_mbuf *m_seg = tx_pkt;
	uint32_t count = 0;

	while (m_seg != NULL) {
		count += SXE2_DIV_ROUND_UP(m_seg->data_len,
				SXE2_TX_MAX_DATA_NUM_PER_DESC);
		m_seg = m_seg->next;
	}

	return count;
}

static __rte_always_inline void
sxe2_tx_desc_checksum_fill(uint64_t offloads, uint32_t *desc_cmd, uint32_t *desc_offset,
		union sxe2_tx_offload_info ol_info)
{
	if (offloads & RTE_MBUF_F_TX_IP_CKSUM) {
		*desc_cmd    |= SXE2_TX_DATA_DESC_CMD_IIPT_IPV4_CSUM;
		*desc_offset |= SXE2_TX_DATA_DESC_IPLEN_VAL(ol_info.l3_len);
	} else if (offloads & RTE_MBUF_F_TX_IPV4) {
		*desc_cmd    |= SXE2_TX_DATA_DESC_CMD_IIPT_IPV4;
		*desc_offset |= SXE2_TX_DATA_DESC_IPLEN_VAL(ol_info.l3_len);
	} else if (offloads & RTE_MBUF_F_TX_IPV6) {
		*desc_cmd    |= SXE2_TX_DATA_DESC_CMD_IIPT_IPV6;
		*desc_offset |= SXE2_TX_DATA_DESC_IPLEN_VAL(ol_info.l3_len);
	}

	if (offloads & RTE_MBUF_F_TX_TCP_SEG) {
		*desc_cmd    |= SXE2_TX_DATA_DESC_CMD_L4T_EOFT_TCP;
		*desc_offset |= SXE2_TX_DATA_DESC_L4LEN_VAL(ol_info.l4_len);
		goto l_end;
	}

	if (offloads & RTE_MBUF_F_TX_UDP_SEG) {
		*desc_cmd    |= SXE2_TX_DATA_DESC_CMD_L4T_EOFT_UDP;
		*desc_offset |= SXE2_TX_DATA_DESC_L4LEN_VAL(ol_info.l4_len);
		goto l_end;
	}

	switch (offloads & RTE_MBUF_F_TX_L4_MASK) {
	case RTE_MBUF_F_TX_TCP_CKSUM:
		*desc_cmd    |= SXE2_TX_DATA_DESC_CMD_L4T_EOFT_TCP;
		*desc_offset |= SXE2_TX_DATA_DESC_L4LEN_VAL(ol_info.l4_len);
		break;
	case RTE_MBUF_F_TX_SCTP_CKSUM:
		*desc_cmd    |= SXE2_TX_DATA_DESC_CMD_L4T_EOFT_SCTP;
		*desc_offset |= SXE2_TX_DATA_DESC_L4LEN_VAL(ol_info.l4_len);
		break;
	case RTE_MBUF_F_TX_UDP_CKSUM:
		*desc_cmd    |= SXE2_TX_DATA_DESC_CMD_L4T_EOFT_UDP;
		*desc_offset |= SXE2_TX_DATA_DESC_L4LEN_VAL(ol_info.l4_len);
		break;
	default:

		break;
	}

l_end:
	return;
}

static __rte_always_inline uint64_t
sxe2_tx_data_desc_build_cobt(uint32_t cmd, uint32_t offset, uint16_t buf_size, uint16_t l2tag)
{
	return rte_cpu_to_le_64(SXE2_TX_DESC_DTYPE_DATA |
			(((uint64_t)cmd)      << SXE2_TX_DATA_DESC_CMD_SHIFT) |
			(((uint64_t)offset)   << SXE2_TX_DATA_DESC_OFFSET_SHIFT) |
			(((uint64_t)buf_size) << SXE2_TX_DATA_DESC_BUF_SZ_SHIFT) |
			(((uint64_t)l2tag)    << SXE2_TX_DATA_DESC_L2TAG1_SHIFT));
}

uint16_t sxe2_tx_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct sxe2_tx_queue *txq = tx_queue;
	struct sxe2_tx_buffer *buffer_ring;
	struct sxe2_tx_buffer *buffer;
	struct sxe2_tx_buffer *next_buffer;
	struct rte_mbuf *tx_pkt;
	struct rte_mbuf *m_seg;
	volatile union sxe2_tx_data_desc *desc_ring;
	volatile union sxe2_tx_data_desc *desc;
	volatile struct sxe2_tx_context_desc *ctxt_desc;
	union sxe2_tx_offload_info ol_info;
	struct sxe2_vsi *vsi = txq->vsi;
	rte_iova_t buf_dma_addr;
	uint64_t offloads;
	uint64_t desc_type_cmd_tso_mss;
	uint32_t desc_cmd;
	uint32_t desc_offset;
	uint32_t desc_tag;
	uint32_t desc_tunneling_params;
	uint16_t ipsec_offset;
	uint16_t ctxt_desc_num;
	uint32_t desc_sum_num;
	uint16_t tx_num;
	uint16_t seg_len;
	uint16_t next_use;
	uint16_t last_use;
	uint16_t desc_l2tag2;

	buffer_ring = txq->buffer_ring;
	desc_ring   = txq->desc_ring;
	next_use    = txq->next_use;
	buffer      = &buffer_ring[next_use];

	if (txq->desc_free_num < txq->free_thresh)
		(void)sxe2_tx_cleanup(txq);

	for (tx_num = 0; tx_num < nb_pkts; tx_num++) {
		tx_pkt = *tx_pkts++;
		desc_cmd              = 0;
		desc_offset           = 0;
		desc_tag              = 0;
		desc_tunneling_params = 0;
		ipsec_offset          = 0;
		offloads              = tx_pkt->ol_flags;
		ol_info.l2_len        = tx_pkt->l2_len;
		ol_info.l3_len        = tx_pkt->l3_len;
		ol_info.l4_len        = tx_pkt->l4_len;
		ol_info.tso_segsz     = tx_pkt->tso_segsz;
		ol_info.outer_l2_len  = tx_pkt->outer_l2_len;
		ol_info.outer_l3_len  = tx_pkt->outer_l3_len;

		ctxt_desc_num = (offloads &
				SXE2_TX_OFFLOAD_CTXT_NEEDCK_MASK) ? 1 : 0;
		if (unlikely(vsi->vsi_type == SXE2_VSI_T_DPDK_ESW))
			ctxt_desc_num = 1;

		if (offloads & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG))
			desc_sum_num = sxe2_tx_pkt_data_desc_count(tx_pkt) + ctxt_desc_num;
		else
			desc_sum_num = tx_pkt->nb_segs + ctxt_desc_num;

		last_use = next_use + desc_sum_num - 1;
		if (last_use >= txq->ring_depth)
			last_use = last_use - txq->ring_depth;

		if (desc_sum_num > txq->desc_free_num) {
			if (unlikely(sxe2_tx_cleanup(txq) != 0))
				goto l_cleanup_exit;

			if (unlikely(desc_sum_num > txq->rs_thresh)) {
				while (desc_sum_num > txq->desc_free_num) {
					if (unlikely(sxe2_tx_cleanup(txq) != 0))
						goto l_cleanup_exit;
				}
			}
		}

		desc_offset |= SXE2_TX_DATA_DESC_MACLEN_VAL(ol_info.l2_len);

		if (offloads & SXE2_TX_OFFLOAD_CKSUM_MASK) {
			sxe2_tx_desc_checksum_fill(offloads, &desc_cmd,
					&desc_offset, ol_info);
		}

		if (offloads & (RTE_MBUF_F_TX_VLAN | RTE_MBUF_F_TX_QINQ)) {
			desc_cmd |= SXE2_TX_DATA_DESC_CMD_IL2TAG1;
			desc_tag = tx_pkt->vlan_tci;
		}

		if (ctxt_desc_num) {
			ctxt_desc = (volatile struct sxe2_tx_context_desc *)
							&desc_ring[next_use];
			desc_l2tag2 = 0;
			desc_type_cmd_tso_mss = SXE2_TX_DESC_DTYPE_CTXT;

			next_buffer = &buffer_ring[buffer->next_id];
			RTE_MBUF_PREFETCH_TO_FREE(next_buffer->mbuf);

			if (buffer->mbuf) {
				rte_pktmbuf_free_seg(buffer->mbuf);
				buffer->mbuf = NULL;
			}

			if (offloads & RTE_MBUF_F_TX_QINQ) {
				desc_l2tag2 = tx_pkt->vlan_tci_outer;
				desc_type_cmd_tso_mss |= SXE2_TX_CTXT_DESC_CMD_IL2TAG2_MASK;
			}

			ctxt_desc->tunneling_params =
				rte_cpu_to_le_32(desc_tunneling_params);
			ctxt_desc->l2tag2 = rte_cpu_to_le_16(desc_l2tag2);
			ctxt_desc->type_cmd_tso_mss = rte_cpu_to_le_64(desc_type_cmd_tso_mss);
			ctxt_desc->ipsec_offset = rte_cpu_to_le_16(ipsec_offset);

			buffer->last_id = last_use;
			next_use        = buffer->next_id;
			buffer          = next_buffer;
		}

		m_seg = tx_pkt;

		do {
			desc = &desc_ring[next_use];
			next_buffer = &buffer_ring[buffer->next_id];
			RTE_MBUF_PREFETCH_TO_FREE(next_buffer->mbuf);
			if (buffer->mbuf) {
				rte_pktmbuf_free_seg(buffer->mbuf);
				buffer->mbuf = NULL;
			}

			buffer->mbuf = m_seg;
			seg_len = m_seg->data_len;
			buf_dma_addr = rte_mbuf_data_iova(m_seg);
			while ((offloads & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_UDP_SEG)) &&
					unlikely(seg_len > SXE2_TX_MAX_DATA_NUM_PER_DESC)) {
				desc->read.buf_addr = rte_cpu_to_le_64(buf_dma_addr);
				desc->read.type_cmd_off_bsz_l2t =
					sxe2_tx_data_desc_build_cobt(desc_cmd, desc_offset,
						SXE2_TX_MAX_DATA_NUM_PER_DESC,
						desc_tag);
				buf_dma_addr += SXE2_TX_MAX_DATA_NUM_PER_DESC;
				seg_len      -= SXE2_TX_MAX_DATA_NUM_PER_DESC;
				buffer->last_id = last_use;
				next_use        = buffer->next_id;
				buffer          = next_buffer;
				desc            = &desc_ring[next_use];
				next_buffer     = &buffer_ring[buffer->next_id];
				RTE_MBUF_PREFETCH_TO_FREE(next_buffer->mbuf);
			}

			desc->read.buf_addr = rte_cpu_to_le_64(buf_dma_addr);
			desc->read.type_cmd_off_bsz_l2t =
				sxe2_tx_data_desc_build_cobt(desc_cmd,
					desc_offset, seg_len, desc_tag);

			buffer->last_id = last_use;
			next_use        = buffer->next_id;
			buffer          = next_buffer;

			m_seg = m_seg->next;
		} while (m_seg);

		desc_cmd |= SXE2_TX_DATA_DESC_CMD_EOP;
		txq->desc_used_num += desc_sum_num;
		txq->desc_free_num -= desc_sum_num;

		if (txq->desc_used_num >= txq->rs_thresh) {
			PMD_LOG_DEBUG(TX, "Tx pkts set RS bit."
					"last_use=%u port_id=%u, queue_id=%u",
					last_use, txq->port_id, txq->queue_id);
			desc_cmd |= SXE2_TX_DATA_DESC_CMD_RS;
			txq->desc_used_num = 0;
		}

		desc->read.type_cmd_off_bsz_l2t |=
			rte_cpu_to_le_64(((uint64_t)desc_cmd) << SXE2_TX_DATA_DESC_CMD_SHIFT);
	}
	goto l_end_of_tx;

l_cleanup_exit:
	if (tx_num == 0)
		return 0;
l_end_of_tx:
	SXE2_PCI_REG_WRITE_WC(txq->tdt_reg_addr, next_use);
	PMD_LOG_DEBUG(TX, "port_id=%u queue_id=%u next_use=%u send_pkts=%u",
			txq->port_id, txq->queue_id, next_use, tx_num);

	txq->next_use = next_use;
	return tx_num;
}

static __rte_always_inline void
sxe2_tx_data_desc_fill(volatile union sxe2_tx_data_desc *desc,
		struct rte_mbuf **tx_pkts)
{
	rte_iova_t buf_dma_addr;
	uint32_t desc_offset;
	buf_dma_addr = rte_mbuf_data_iova(*tx_pkts);
	desc->read.buf_addr = rte_cpu_to_le_64(buf_dma_addr);
	desc_offset = SXE2_TX_DATA_DESC_MACLEN_VAL((*tx_pkts)->l2_len);
	desc->read.type_cmd_off_bsz_l2t =
				sxe2_tx_data_desc_build_cobt(SXE2_TX_DATA_DESC_CMD_EOP,
					desc_offset, (*tx_pkts)->data_len, 0);
}
static __rte_always_inline void
sxe2_tx_data_desc_fill_batch(volatile union sxe2_tx_data_desc *desc,
		struct rte_mbuf **tx_pkts)
{
	rte_iova_t buf_dma_addr;
	uint32_t i;
	uint32_t desc_offset;
	for (i = 0; i < SXE2_TX_FILL_PER_LOOP; ++i, ++desc, ++tx_pkts) {
		buf_dma_addr = rte_mbuf_data_iova(*tx_pkts);
		desc->read.buf_addr = rte_cpu_to_le_64(buf_dma_addr);
		desc_offset = SXE2_TX_DATA_DESC_MACLEN_VAL((*tx_pkts)->l2_len);
		desc->read.type_cmd_off_bsz_l2t =
			sxe2_tx_data_desc_build_cobt(SXE2_TX_DATA_DESC_CMD_EOP,
					desc_offset,
					(*tx_pkts)->data_len,
					0);
	}
}

static inline void sxe2_tx_ring_fill(struct sxe2_tx_queue *txq,
				struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct sxe2_tx_buffer *buffer = &txq->buffer_ring[txq->next_use];
	volatile union sxe2_tx_data_desc *desc = &txq->desc_ring[txq->next_use];
	uint32_t i, j;
	uint32_t	mainpart;
	uint32_t leftover;
	mainpart = nb_pkts & ((uint32_t)~SXE2_TX_FILL_PER_LOOP_MASK);
	leftover = nb_pkts & ((uint32_t)SXE2_TX_FILL_PER_LOOP_MASK);
	for (i = 0; i < mainpart; i += SXE2_TX_FILL_PER_LOOP) {
		for (j = 0; j < SXE2_TX_FILL_PER_LOOP; ++j)
			(buffer + i + j)->mbuf = *(tx_pkts + i + j);
		sxe2_tx_data_desc_fill_batch(desc + i, tx_pkts + i);
	}
	if (unlikely(leftover > 0)) {
		for (i = 0; i < leftover; ++i) {
			(buffer + mainpart + i)->mbuf = *(tx_pkts + mainpart + i);
			sxe2_tx_data_desc_fill(desc + mainpart + i,
					tx_pkts + mainpart + i);
		}
	}
}

static inline uint16_t sxe2_tx_pkts_batch(void *tx_queue,
			struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct sxe2_tx_queue *txq = (struct sxe2_tx_queue *)tx_queue;
	volatile union sxe2_tx_data_desc *desc_ring = txq->desc_ring;
	uint16_t res_num = 0;
	if (txq->desc_free_num < txq->free_thresh)
		(void)sxe2_tx_bufs_free(txq);
	nb_pkts = RTE_MIN(txq->desc_free_num, nb_pkts);
	if (unlikely(nb_pkts == 0)) {
		PMD_LOG_DEBUG(TX, "Tx batch: may not enough free desc, "
				"free_desc=%u, need_tx_pkts=%u",
				txq->desc_free_num, nb_pkts);
		goto l_end;
	}
	txq->desc_free_num -= nb_pkts;
	if ((txq->next_use + nb_pkts) > txq->ring_depth) {
		res_num = txq->ring_depth - txq->next_use;
		sxe2_tx_ring_fill(txq, tx_pkts, res_num);
		desc_ring[txq->next_rs].read.type_cmd_off_bsz_l2t |=
				rte_cpu_to_le_64(SXE2_TX_DATA_DESC_CMD_RS_MASK);
		txq->next_rs = txq->rs_thresh - 1;
		txq->next_use = 0;
	}
	sxe2_tx_ring_fill(txq, tx_pkts + res_num, nb_pkts - res_num);
	txq->next_use = txq->next_use + (nb_pkts - res_num);
	if (txq->next_use > txq->next_rs) {
		desc_ring[txq->next_rs].read.type_cmd_off_bsz_l2t |=
				rte_cpu_to_le_64(SXE2_TX_DATA_DESC_CMD_RS_MASK);
		txq->next_rs += txq->rs_thresh;
		if (txq->next_rs >= txq->ring_depth)
			txq->next_rs = txq->rs_thresh - 1;
	}
	if (txq->next_use >= txq->ring_depth)
		txq->next_use = 0;
	PMD_LOG_DEBUG(TX, "port_id=%u queue_id=%u next_use=%u send_pkts=%u",
			txq->port_id, txq->queue_id, txq->next_use, nb_pkts);
	SXE2_PCI_REG_WRITE_WC(txq->tdt_reg_addr, txq->next_use);
l_end:
	return nb_pkts;
}

static inline void
sxe2_update_rx_tail(struct sxe2_rx_queue *rxq, uint16_t hold_num, uint16_t rx_id)
{
	hold_num += rxq->hold_num;

	if (hold_num > rxq->rx_free_thresh) {
		rx_id = (uint16_t)((rx_id == 0) ? (rxq->ring_depth - 1) : (rx_id - 1));
		SXE2_PCI_REG_WRITE_WC(rxq->rdt_reg_addr, rx_id);
		hold_num = 0;
	}
	rxq->hold_num = hold_num;
}

static inline uint64_t
sxe2_rx_desc_error_para(__rte_unused struct sxe2_rx_queue *rxq,
		union sxe2_rx_desc *desc)
{
	uint64_t flags = 0;
	uint64_t desc_qw1 = rte_le_to_cpu_64(desc->wb.status_err_ptype_len);

	if (unlikely(0 == (desc_qw1 & SXE2_RX_DESC_STATUS_L3L4_P_MASK)))
		goto l_end;

	if (likely(0 == (desc->wb.rxdid_src & SXE2_RX_DESC_EUDPE_MASK)))
		flags = RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD;
	else
		flags = RTE_MBUF_F_RX_OUTER_L4_CKSUM_BAD;

	if (likely(0 == (desc_qw1 & SXE2_RX_DESC_QW1_ERRORS_MASK))) {
		flags |= (RTE_MBUF_F_RX_IP_CKSUM_GOOD |
				RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				RTE_MBUF_F_RX_OUTER_L4_CKSUM_GOOD);
		goto l_end;
	}

	if (likely(0 == (desc_qw1 & SXE2_RX_DESC_ERROR_CSUM_IPE_MASK)))
		flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;
	else
		flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;

	if (likely(0 == (desc_qw1 & SXE2_RX_DESC_ERROR_CSUM_L4_MASK)))
		flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
	else
		flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;

	if (unlikely(0 != (desc_qw1 & SXE2_RX_DESC_ERROR_CSUM_EIP_MASK)))
		flags |= RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD;

l_end:
	return flags;
}

static __rte_always_inline void
sxe2_rx_mbuf_common_fields_fill(struct sxe2_rx_queue *rxq, struct rte_mbuf *mbuf,
		union sxe2_rx_desc *rxd)
{
	uint32_t *ptype_tbl = rxq->vsi->adapter->ptype_tbl;
	uint64_t qword1;
	uint64_t pkt_flags;
	qword1 = rte_le_to_cpu_64(rxd->wb.status_err_ptype_len);

	mbuf->ol_flags = 0;
	mbuf->packet_type = ptype_tbl[SXE2_RX_DESC_PTYPE_VAL_GET(qword1)];

	pkt_flags = sxe2_rx_desc_error_para(rxq, rxd);

	mbuf->ol_flags |= pkt_flags;
}

static __rte_always_inline void
sxe2_rx_sw_stats_update(struct sxe2_rx_queue *rxq, struct rte_mbuf *mbuf,
		union sxe2_rx_desc *rxd)
{
	uint64_t qword1 = rte_le_to_cpu_64(rxd->wb.status_err_ptype_len);
	rte_atomic_fetch_add_explicit(&rxq->sw_stats.pkts, 1,
		rte_memory_order_relaxed);
	rte_atomic_fetch_add_explicit(&rxq->sw_stats.bytes,
			mbuf->pkt_len + RTE_ETHER_CRC_LEN,
			rte_memory_order_relaxed);
	switch (SXE2_RX_DESC_STATUS_UMBCAST_VAL_GET(qword1)) {
	case SXE2_RX_DESC_STATUS_UNICAST:
		rte_atomic_fetch_add_explicit(&rxq->sw_stats.unicast_pkts, 1,
			rte_memory_order_relaxed);
		break;
	case SXE2_RX_DESC_STATUS_MULTICAST:
		rte_atomic_fetch_add_explicit(&rxq->sw_stats.multicast_pkts, 1,
			rte_memory_order_relaxed);
		break;
	case SXE2_RX_DESC_STATUS_BROADCAST:
		rte_atomic_fetch_add_explicit(&rxq->sw_stats.broadcast_pkts, 1,
			rte_memory_order_relaxed);
		break;
	default:
		break;
	}
}

uint16_t sxe2_rx_pkts_scattered(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct sxe2_rx_queue *rxq = (struct sxe2_rx_queue *)rx_queue;
	volatile union sxe2_rx_desc *desc_ring;
	volatile union sxe2_rx_desc *desc;
	union sxe2_rx_desc desc_tmp;
	struct rte_mbuf **buffer_ring;
	struct rte_mbuf **cur_buffer;
	struct rte_mbuf *cur_mbuf;
	struct rte_mbuf *new_mbuf;
	struct rte_mbuf *first_seg;
	struct rte_mbuf *last_seg;
	uint64_t qword1;
	uint16_t done_num;
	uint16_t hold_num;
	uint16_t cur_idx;
	uint16_t pkt_len;

	desc_ring   = rxq->desc_ring;
	buffer_ring = rxq->buffer_ring;
	cur_idx     = rxq->processing_idx;
	first_seg   = rxq->pkt_first_seg;
	last_seg    = rxq->pkt_last_seg;
	done_num    = 0;
	hold_num    = 0;
	while (done_num < nb_pkts) {
		desc = &desc_ring[cur_idx];
		qword1 = rte_le_to_cpu_64(desc->wb.status_err_ptype_len);
		if (0 == (SXE2_RX_DESC_STATUS_DD_MASK & qword1))
			break;

		new_mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);
		if (unlikely(new_mbuf == NULL)) {
			rxq->vsi->adapter->dev_info.dev_data->rx_mbuf_alloc_failed++;
			PMD_LOG_INFO(RX, "Rx new_mbuf alloc failed port_id:%u "
					"queue_id:%u", rxq->port_id, rxq->queue_id);
			break;
		}

		hold_num++;
		desc_tmp = *desc;
		cur_buffer = &buffer_ring[cur_idx];
		cur_idx++;
		if (unlikely(cur_idx == rxq->ring_depth))
			cur_idx = 0;

		rte_prefetch0(buffer_ring[cur_idx]);

		if (0 == (cur_idx & 0x3)) {
			rte_prefetch0(&desc_ring[cur_idx]);
			rte_prefetch0(&buffer_ring[cur_idx]);
		}

		cur_mbuf = *cur_buffer;

		*cur_buffer = new_mbuf;

		desc->read.hdr_addr = 0;
		desc->read.pkt_addr =
			rte_cpu_to_le_64(rte_mbuf_data_iova_default(new_mbuf));

		pkt_len = SXE2_RX_DESC_PKT_LEN_VAL_GET(qword1);
		cur_mbuf->data_len = pkt_len;
		cur_mbuf->data_off = RTE_PKTMBUF_HEADROOM;

		if (first_seg == NULL) {
			first_seg = cur_mbuf;
			first_seg->nb_segs = 1;
			first_seg->pkt_len = pkt_len;
		} else {
			first_seg->pkt_len += pkt_len;
			first_seg->nb_segs++;
			last_seg->next = cur_mbuf;
		}

		if (0 == (qword1 & SXE2_RX_DESC_STATUS_EOP_MASK)) {
			last_seg = cur_mbuf;
			continue;
		}

		if (unlikely(qword1 & SXE2_RX_DESC_ERROR_RXE_MASK) ||
			unlikely(qword1 & SXE2_RX_DESC_ERROR_OVERSIZE_MASK)) {
			rte_atomic_fetch_add_explicit(&rxq->sw_stats.drop_pkts, 1,
				rte_memory_order_relaxed);
			rte_atomic_fetch_add_explicit(&rxq->sw_stats.drop_bytes,
				first_seg->pkt_len - rxq->crc_len + RTE_ETHER_CRC_LEN,
				rte_memory_order_relaxed);
			rte_pktmbuf_free(first_seg);
			first_seg = NULL;
			continue;
		}

		cur_mbuf->next = NULL;
		if (unlikely(rxq->crc_len > 0)) {
			first_seg->pkt_len -= RTE_ETHER_CRC_LEN;

			if (pkt_len <= RTE_ETHER_CRC_LEN) {
				rte_pktmbuf_free_seg(cur_mbuf);
				first_seg->nb_segs--;
				last_seg->data_len = last_seg->data_len + pkt_len -
					RTE_ETHER_CRC_LEN;
				last_seg->next = NULL;
			} else {
				cur_mbuf->data_len = pkt_len - RTE_ETHER_CRC_LEN;
			}

		} else if (pkt_len == 0) {
			rte_pktmbuf_free_seg(cur_mbuf);
			first_seg->nb_segs--;
			last_seg->next = NULL;
		}

		rte_prefetch0(RTE_PTR_ADD(first_seg->buf_addr, first_seg->data_off));
		first_seg->port     = rxq->port_id;

		sxe2_rx_mbuf_common_fields_fill(rxq, first_seg, &desc_tmp);

		if (rxq->vsi->adapter->devargs.sw_stats_en)
			sxe2_rx_sw_stats_update(rxq, first_seg, &desc_tmp);

		rte_prefetch0(RTE_PTR_ADD(first_seg->buf_addr, first_seg->data_off));

		rx_pkts[done_num] = first_seg;
		done_num++;

		first_seg = NULL;
	}

	rxq->processing_idx = cur_idx;
	rxq->pkt_first_seg  = first_seg;
	rxq->pkt_last_seg   = last_seg;

	sxe2_update_rx_tail(rxq, hold_num, cur_idx);

	return done_num;
}

uint16_t sxe2_rx_pkts_scattered_split(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct sxe2_rx_queue *rxq = (struct sxe2_rx_queue *)rx_queue;
	volatile union sxe2_rx_desc *desc_ring;
	volatile union sxe2_rx_desc *desc;
	union sxe2_rx_desc desc_tmp;
	struct rte_mbuf **buffer_ring;
	struct rte_mbuf **cur_buffer;
	struct rte_mbuf *cur_mbuf;
	struct rte_mbuf *cur_mbuf_pay;
	struct rte_mbuf *new_mbuf;
	struct rte_mbuf *new_mbuf_pay = NULL;
	struct rte_mbuf *first_seg;
	struct rte_mbuf *last_seg;
	uint64_t qword1;
	uint16_t done_num;
	uint16_t hold_num;
	uint16_t cur_idx;
	uint16_t pkt_len;
	uint16_t hdr_len;

	desc_ring = rxq->desc_ring;
	buffer_ring = rxq->buffer_ring;
	cur_idx = rxq->processing_idx;
	first_seg = rxq->pkt_first_seg;
	last_seg = rxq->pkt_last_seg;
	done_num = 0;
	hold_num = 0;
	new_mbuf = NULL;

	while (done_num < nb_pkts) {
		desc = &desc_ring[cur_idx];
		qword1 = rte_le_to_cpu_64(desc->wb.status_err_ptype_len);

		if (0 == (SXE2_RX_DESC_STATUS_DD_MASK & qword1))
			break;

		if ((rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT) == 0 ||
			first_seg == NULL) {
			new_mbuf = rte_mbuf_raw_alloc(rxq->mb_pool);
			if (unlikely(new_mbuf == NULL)) {
				rxq->vsi->adapter->dev_info.dev_data->rx_mbuf_alloc_failed++;
				break;
			}
		}

		if (rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT) {
			new_mbuf_pay = rte_mbuf_raw_alloc(rxq->rx_seg[1].mp);
			if (unlikely(new_mbuf_pay == NULL)) {
				rxq->vsi->adapter->dev_info.dev_data->rx_mbuf_alloc_failed++;
				if (new_mbuf != NULL)
					rte_pktmbuf_free(new_mbuf);
				new_mbuf = NULL;
				break;
			}
		}

		hold_num++;
		desc_tmp = *desc;
		cur_buffer = &buffer_ring[cur_idx];
		cur_idx++;
		if (unlikely(cur_idx == rxq->ring_depth))
			cur_idx = 0;
		rte_prefetch0(buffer_ring[cur_idx]);
		if (0 == (cur_idx & 0x3)) {
			rte_prefetch0(&desc_ring[cur_idx]);
			rte_prefetch0(&buffer_ring[cur_idx]);
		}
		cur_mbuf = *cur_buffer;
		if (0 == (rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT)) {
			*cur_buffer = new_mbuf;
			desc->read.hdr_addr = 0;
			desc->read.pkt_addr =
				rte_cpu_to_le_64(rte_mbuf_data_iova_default(new_mbuf));
		} else {
			if (first_seg == NULL) {
				*cur_buffer = new_mbuf;
				new_mbuf->next = new_mbuf_pay;
				new_mbuf->data_off = RTE_PKTMBUF_HEADROOM;
				new_mbuf_pay->next = NULL;
				new_mbuf_pay->data_off = RTE_PKTMBUF_HEADROOM;
				desc->read.hdr_addr =
					rte_cpu_to_le_64(rte_mbuf_data_iova_default(new_mbuf));
				desc->read.pkt_addr =
					rte_cpu_to_le_64(rte_mbuf_data_iova_default(new_mbuf_pay));
			} else {
				cur_mbuf_pay = cur_mbuf->next;
				new_mbuf_pay->next = NULL;
				new_mbuf_pay->data_off = RTE_PKTMBUF_HEADROOM;
				cur_mbuf->next = new_mbuf_pay;
				desc->read.hdr_addr =
					rte_cpu_to_le_64(rte_mbuf_data_iova_default(cur_mbuf));
				desc->read.pkt_addr =
					rte_cpu_to_le_64(rte_mbuf_data_iova_default(new_mbuf_pay));
				cur_mbuf = cur_mbuf_pay;
			}
		}
		new_mbuf = NULL;
		if (0 == (rxq->offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT)) {
			pkt_len = SXE2_RX_DESC_PKT_LEN_VAL_GET(qword1);
			cur_mbuf->data_len = pkt_len;
			cur_mbuf->data_off = RTE_PKTMBUF_HEADROOM;
			if (first_seg == NULL) {
				first_seg = cur_mbuf;
				first_seg->nb_segs = 1;
				first_seg->pkt_len = pkt_len;
			} else {
				first_seg->pkt_len += pkt_len;
				first_seg->nb_segs++;
				last_seg->next = cur_mbuf;
			}
		} else {
			if (first_seg == NULL) {
				cur_mbuf->nb_segs = 2;
				cur_mbuf->next->next = NULL;
				pkt_len = SXE2_RX_DESC_PKT_LEN_VAL_GET(qword1);
				hdr_len = SXE2_RX_DESC_HDR_LEN_VAL_GET(qword1);
				cur_mbuf->data_len = hdr_len;
				cur_mbuf->pkt_len = hdr_len + pkt_len;
				cur_mbuf->next->data_len = pkt_len;
				first_seg = cur_mbuf;
				cur_mbuf = cur_mbuf->next;
				last_seg = cur_mbuf;
			} else {
				cur_mbuf->nb_segs = 1;
				cur_mbuf->next = NULL;
				pkt_len = SXE2_RX_DESC_PKT_LEN_VAL_GET(qword1);
				cur_mbuf->data_len = pkt_len;

				first_seg->pkt_len += pkt_len;
				first_seg->nb_segs++;
				last_seg->next = cur_mbuf;
			}
		}

#ifdef RTE_ETHDEV_DEBUG_RX

		rte_pktmbuf_dump(stdout, first_seg, rte_pktmbuf_pkt_len(first_seg));
#endif

		if (0 == (rte_le_to_cpu_64(desc_tmp.wb.status_err_ptype_len) &
					SXE2_RX_DESC_STATUS_EOP_MASK)) {
			last_seg = cur_mbuf;
			continue;
		}

		if (unlikely(qword1 & SXE2_RX_DESC_ERROR_RXE_MASK) ||
			unlikely(qword1 & SXE2_RX_DESC_ERROR_OVERSIZE_MASK)) {
			rte_atomic_fetch_add_explicit(&rxq->sw_stats.drop_pkts, 1,
				rte_memory_order_relaxed);
			rte_atomic_fetch_add_explicit(&rxq->sw_stats.drop_bytes,
				first_seg->pkt_len - rxq->crc_len + RTE_ETHER_CRC_LEN,
				rte_memory_order_relaxed);
			rte_pktmbuf_free(first_seg);
			first_seg = NULL;
			continue;
		}

		cur_mbuf->next = NULL;
		if (unlikely(rxq->crc_len > 0)) {
			first_seg->pkt_len -= RTE_ETHER_CRC_LEN;
			if (pkt_len <= RTE_ETHER_CRC_LEN) {
				rte_pktmbuf_free_seg(cur_mbuf);
				cur_mbuf = NULL;
				first_seg->nb_segs--;
				last_seg->data_len = last_seg->data_len +
					pkt_len - RTE_ETHER_CRC_LEN;
				last_seg->next = NULL;
			} else {
				cur_mbuf->data_len = pkt_len - RTE_ETHER_CRC_LEN;
			}
		} else if (pkt_len == 0) {
			rte_pktmbuf_free_seg(cur_mbuf);
			cur_mbuf = NULL;
			first_seg->nb_segs--;
			last_seg->next = NULL;
		}

		first_seg->port = rxq->port_id;
		sxe2_rx_mbuf_common_fields_fill(rxq, first_seg, &desc_tmp);

		if (rxq->vsi->adapter->devargs.sw_stats_en)
			sxe2_rx_sw_stats_update(rxq, first_seg, &desc_tmp);

		rte_prefetch0(RTE_PTR_ADD(first_seg->buf_addr, first_seg->data_off));

		rx_pkts[done_num] = first_seg;
		done_num++;

		first_seg = NULL;
	}

	rxq->processing_idx = cur_idx;
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg = last_seg;

	sxe2_update_rx_tail(rxq, hold_num, cur_idx);

	return done_num;
}
