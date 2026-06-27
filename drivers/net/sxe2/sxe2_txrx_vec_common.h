/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_TXRX_VEC_COMMON_H
#define SXE2_TXRX_VEC_COMMON_H

#include <rte_atomic.h>
#ifdef PCLINT
#include "avx_stub.h"
#endif
#include "sxe2_rx.h"
#include "sxe2_queue.h"
#include "sxe2_tx.h"
#include "sxe2_vsi.h"
#include "sxe2_ethdev.h"
#define SXE2_RX_NUM_PER_LOOP_SSE    4
#define SXE2_RX_NUM_PER_LOOP_AVX     8
#define SXE2_RX_NUM_PER_LOOP_NEON    4
#define SXE2_RX_REARM_THRESH_VEC       64
#define SXE2_RX_PKTS_BURST_BATCH_NUM_VEC   32
#define SXE2_TX_RS_THRESH_MIN_VEC	32
#define SXE2_TX_FREE_BUFFER_SIZE_MAX_VEC  64

static __rte_always_inline void
sxe2_tx_pkts_mbuf_fill(struct sxe2_tx_buffer *buffer,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	uint16_t i;
	for (i = 0; i < nb_pkts; ++i)
		buffer[i].mbuf = tx_pkts[i];
}

static __rte_always_inline int32_t
sxe2_tx_bufs_free_vec(struct sxe2_tx_queue *txq)
{
	struct sxe2_tx_buffer *buffer;
	struct rte_mbuf *mbuf;
	struct rte_mbuf *mbuf_free_arr[SXE2_TX_FREE_BUFFER_SIZE_MAX_VEC];
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
	buffer = &txq->buffer_ring[txq->next_dd - (rs_thresh - 1)];
	mbuf = rte_pktmbuf_prefree_seg(buffer[0].mbuf);
	if (likely(mbuf)) {
		mbuf_free_arr[0] = mbuf;
		free_num = 1;
		for (i = 1; i < rs_thresh; ++i) {
			mbuf = rte_pktmbuf_prefree_seg(buffer[i].mbuf);
			if (likely(mbuf)) {
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
		}
		rte_mempool_put_bulk(mbuf_free_arr[0]->pool,
							(void *)mbuf_free_arr, free_num);
	} else {
		for (i = 1; i < rs_thresh; ++i) {
			mbuf = rte_pktmbuf_prefree_seg(buffer[i].mbuf);
			if (mbuf != NULL)
				rte_mempool_put(mbuf->pool, mbuf);
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

static inline void
sxe2_tx_desc_fill_offloads(struct rte_mbuf *mbuf, uint64_t *desc_qw1)
{
	uint64_t offloads = mbuf->ol_flags;
	uint32_t desc_cmd = 0;
	uint32_t desc_offset = 0;
	if (offloads & RTE_MBUF_F_TX_IP_CKSUM) {
		desc_cmd    |= SXE2_TX_DATA_DESC_CMD_IIPT_IPV4_CSUM;
		desc_offset |= SXE2_TX_DATA_DESC_IPLEN_VAL(mbuf->l3_len);
	} else if (offloads & RTE_MBUF_F_TX_IPV4) {
		desc_cmd    |= SXE2_TX_DATA_DESC_CMD_IIPT_IPV4;
		desc_offset |= SXE2_TX_DATA_DESC_IPLEN_VAL(mbuf->l3_len);
	} else if (offloads & RTE_MBUF_F_TX_IPV6) {
		desc_cmd    |= SXE2_TX_DATA_DESC_CMD_IIPT_IPV6;
		desc_offset |= SXE2_TX_DATA_DESC_IPLEN_VAL(mbuf->l3_len);
	}
	switch (offloads & RTE_MBUF_F_TX_L4_MASK) {
	case RTE_MBUF_F_TX_TCP_CKSUM:
		desc_cmd    |= SXE2_TX_DATA_DESC_CMD_L4T_EOFT_TCP;
		desc_offset |= SXE2_TX_DATA_DESC_L4LEN_VAL(mbuf->l4_len);
		break;
	case RTE_MBUF_F_TX_SCTP_CKSUM:
		desc_cmd    |= SXE2_TX_DATA_DESC_CMD_L4T_EOFT_SCTP;
		desc_offset |= SXE2_TX_DATA_DESC_L4LEN_VAL(mbuf->l4_len);
		break;
	case RTE_MBUF_F_TX_UDP_CKSUM:
		desc_cmd    |= SXE2_TX_DATA_DESC_CMD_L4T_EOFT_UDP;
		desc_offset |= SXE2_TX_DATA_DESC_L4LEN_VAL(mbuf->l4_len);
		break;
	default:
		break;
	}
	*desc_qw1 |= ((uint64_t)desc_offset) << SXE2_TX_DATA_DESC_OFFSET_SHIFT;
	if (offloads & (RTE_MBUF_F_TX_VLAN | RTE_MBUF_F_TX_QINQ)) {
		desc_cmd |= SXE2_TX_DATA_DESC_CMD_IL2TAG1;
		*desc_qw1 |= ((uint64_t)mbuf->vlan_tci) << SXE2_TX_DATA_DESC_L2TAG1_SHIFT;
	}
	*desc_qw1 |= ((uint64_t)desc_cmd) << SXE2_TX_DATA_DESC_CMD_SHIFT;
}
#define SXE2_RX_UMBCAST_FLAGS_VAL_GET(_flags) \
		(((_flags) & 0x30) >> 4)

static inline void sxe2_vf_rx_vec_sw_stats_cnt(struct sxe2_rx_queue *rxq,
		struct rte_mbuf *mbuf, uint8_t umbcast_flag)
{
	rxq->sw_stats.pkts += 1;
	rxq->sw_stats.bytes += mbuf->pkt_len + RTE_ETHER_CRC_LEN;
	switch (SXE2_RX_UMBCAST_FLAGS_VAL_GET(umbcast_flag)) {
	case SXE2_RX_DESC_STATUS_UNICAST:
		rxq->sw_stats.unicast_pkts += 1;
		break;
	case SXE2_RX_DESC_STATUS_MULTICAST:
		rxq->sw_stats.multicast_pkts += 1;
		break;
	case SXE2_RX_DESC_STATUS_BROADCAST:
		rxq->sw_stats.broadcast_pkts += 1;
		break;
	default:
		break;
	}
}

static inline uint16_t
sxe2_rx_pkts_refactor(struct sxe2_rx_queue *rxq,
		struct rte_mbuf **mbuf_bufs, uint16_t mbuf_num,
		uint8_t *split_rxe_flags, uint8_t *umbcast_flags)
{
	struct rte_mbuf *done_pkts[SXE2_RX_PKTS_BURST_BATCH_NUM_VEC] = {0};
	struct rte_mbuf *first_seg = rxq->pkt_first_seg;
	struct rte_mbuf *last_seg  = rxq->pkt_last_seg;
	struct rte_mbuf *tmp_seg;
	uint16_t done_num, buf_idx;
	done_num = 0;
	for (buf_idx = 0; buf_idx < mbuf_num; buf_idx++) {
		if (last_seg) {
			last_seg->next = mbuf_bufs[buf_idx];
			mbuf_bufs[buf_idx]->data_len += rxq->crc_len;
			first_seg->nb_segs++;
			first_seg->pkt_len += mbuf_bufs[buf_idx]->data_len;
			last_seg = last_seg->next;
			if (split_rxe_flags[buf_idx] == 0) {
				first_seg->hash = last_seg->hash;
				first_seg->vlan_tci = last_seg->vlan_tci;
				first_seg->ol_flags = last_seg->ol_flags;
				first_seg->pkt_len -= rxq->crc_len;
				if (last_seg->data_len > rxq->crc_len) {
					last_seg->data_len -= rxq->crc_len;
				} else {
					tmp_seg = first_seg;
					first_seg->nb_segs--;
					while (tmp_seg->next != last_seg)
						tmp_seg = tmp_seg->next;
					tmp_seg->data_len -= (rxq->crc_len - last_seg->data_len);
					tmp_seg->next = NULL;
					rte_pktmbuf_free_seg(last_seg);
					last_seg = NULL;
				}
				done_pkts[done_num++] = first_seg;
				sxe2_vf_rx_vec_sw_stats_cnt(rxq, first_seg, umbcast_flags[buf_idx]);
				first_seg = NULL;
				last_seg  = NULL;
			} else if (split_rxe_flags[buf_idx] & SXE2_RX_DESC_STATUS_EOP_MASK) {
				continue;
			} else {
				rxq->sw_stats.drop_pkts += 1;
				rxq->sw_stats.drop_bytes +=
					first_seg->pkt_len - rxq->crc_len + RTE_ETHER_CRC_LEN;
				rte_pktmbuf_free(first_seg);
				first_seg = NULL;
				last_seg  = NULL;
				continue;
			}
		} else {
			if (split_rxe_flags[buf_idx] == 0) {
				done_pkts[done_num++] = mbuf_bufs[buf_idx];
				sxe2_vf_rx_vec_sw_stats_cnt(rxq, mbuf_bufs[buf_idx],
					 umbcast_flags[buf_idx]);
				continue;
			} else if (split_rxe_flags[buf_idx] & SXE2_RX_DESC_STATUS_EOP_MASK) {
				first_seg = mbuf_bufs[buf_idx];
				last_seg  = first_seg;
				mbuf_bufs[buf_idx]->data_len += rxq->crc_len;
				mbuf_bufs[buf_idx]->pkt_len  += rxq->crc_len;
			} else {
				rxq->sw_stats.drop_pkts += 1;
				rxq->sw_stats.drop_bytes +=
					mbuf_bufs[buf_idx]->pkt_len - rxq->crc_len +
					RTE_ETHER_CRC_LEN;
				rte_pktmbuf_free_seg(mbuf_bufs[buf_idx]);
				continue;
			}
		}
	}
	rxq->pkt_first_seg = first_seg;
	rxq->pkt_last_seg  = last_seg;
	memcpy(mbuf_bufs, done_pkts, done_num * sizeof(*done_pkts));
	return done_num;
}

#endif /* SXE2_TXRX_VEC_COMMON_H */
