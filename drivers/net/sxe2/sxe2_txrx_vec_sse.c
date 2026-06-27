/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#include <ethdev_driver.h>
#include <rte_bitops.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_vect.h>
#include "rte_common.h"
#include "sxe2_ethdev.h"
#include "sxe2_common_log.h"
#include "sxe2_queue.h"
#include "sxe2_txrx_vec.h"
#include "sxe2_txrx_vec_common.h"
#include "sxe2_vsi.h"

static __rte_always_inline void
sxe2_tx_desc_fill_one_sse(volatile union sxe2_tx_data_desc *desc,
		struct rte_mbuf *pkt,
		uint64_t desc_cmd, bool with_offloads)
{
	__m128i data_desc;
	uint64_t desc_qw1;
	uint32_t desc_offset;
	desc_qw1 = (SXE2_TX_DESC_DTYPE_DATA |
		    ((uint64_t)desc_cmd) << SXE2_TX_DATA_DESC_CMD_SHIFT |
		    ((uint64_t)pkt->data_len) << SXE2_TX_DATA_DESC_BUF_SZ_SHIFT);
	desc_offset = SXE2_TX_DATA_DESC_MACLEN_VAL(pkt->l2_len);
	desc_qw1 |= ((uint64_t)desc_offset) << SXE2_TX_DATA_DESC_OFFSET_SHIFT;
	if (with_offloads)
		sxe2_tx_desc_fill_offloads(pkt, &desc_qw1);
	data_desc = _mm_set_epi64x(desc_qw1, rte_pktmbuf_iova(pkt));
	_mm_store_si128(RTE_CAST_PTR(__m128i *, desc), data_desc);
}

static __rte_always_inline uint16_t
sxe2_tx_pkts_vec_sse_batch(struct sxe2_tx_queue *txq,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts, bool with_offloads)
{
	volatile union sxe2_tx_data_desc *desc;
	struct sxe2_tx_buffer *buffer;
	uint16_t next_use;
	uint16_t res_num;
	uint16_t tx_num;
	uint16_t i;
	if (txq->desc_free_num < txq->free_thresh)
		(void)sxe2_tx_bufs_free_vec(txq);
	nb_pkts = RTE_MIN(txq->desc_free_num, nb_pkts);
	if (unlikely(nb_pkts == 0)) {
		PMD_LOG_DEBUG(TX, "Tx pkts sse batch: may not enough free desc, "
				"free_desc=%u, need_tx_pkts=%u",
				txq->desc_free_num, nb_pkts);
		goto l_end;
	}
	tx_num = nb_pkts;
	next_use = txq->next_use;
	desc     = &txq->desc_ring[next_use];
	buffer   = &txq->buffer_ring[next_use];
	txq->desc_free_num -= nb_pkts;
	res_num = txq->ring_depth - txq->next_use;
	if (tx_num >= res_num) {
		sxe2_tx_pkts_mbuf_fill(buffer, tx_pkts, res_num);
		for (i = 0; i < res_num - 1; ++i, ++tx_pkts, ++desc) {
			sxe2_tx_desc_fill_one_sse(desc, *tx_pkts,
						  SXE2_TX_DATA_DESC_CMD_EOP,
						  with_offloads);
		}
		sxe2_tx_desc_fill_one_sse(desc, *tx_pkts++,
			(SXE2_TX_DATA_DESC_CMD_EOP | SXE2_TX_DATA_DESC_CMD_RS),
			with_offloads);
		tx_num -= res_num;
		next_use     = 0;
		txq->next_rs = txq->rs_thresh - 1;
		desc         = &txq->desc_ring[next_use];
		buffer       = &txq->buffer_ring[next_use];
	}
	sxe2_tx_pkts_mbuf_fill(buffer, tx_pkts, tx_num);
	for (i = 0; i < tx_num; ++i, ++tx_pkts, ++desc) {
		sxe2_tx_desc_fill_one_sse(desc, *tx_pkts,
					  SXE2_TX_DATA_DESC_CMD_EOP,
					  with_offloads);
	}
	next_use += tx_num;
	if (next_use > txq->next_rs) {
		txq->desc_ring[txq->next_rs].read.type_cmd_off_bsz_l2t |=
			rte_cpu_to_le_64(SXE2_TX_DATA_DESC_CMD_RS_MASK);
		txq->next_rs += txq->rs_thresh;
	}
	txq->next_use = next_use;
	SXE2_PCI_REG_WRITE_WC(txq->tdt_reg_addr, next_use);
	PMD_LOG_DEBUG(TX, "port_id=%u queue_id=%u next_use=%u send_pkts=%u",
			 txq->port_id, txq->queue_id, next_use, nb_pkts);
l_end:
	return nb_pkts;
}

static __rte_always_inline uint16_t
sxe2_tx_pkts_vec_sse_common(struct sxe2_tx_queue *txq,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts, bool with_offloads)
{
	uint16_t tx_done_num = 0;
	uint16_t tx_once_num;
	uint16_t tx_need_num;
	while (nb_pkts) {
		tx_need_num = RTE_MIN(nb_pkts, txq->rs_thresh);
		tx_once_num = sxe2_tx_pkts_vec_sse_batch(txq,
				tx_pkts + tx_done_num,
				tx_need_num, with_offloads);
		nb_pkts     -= tx_once_num;
		tx_done_num += tx_once_num;
		if (tx_once_num < tx_need_num)
			break;
	}
	return tx_done_num;
}

uint16_t sxe2_tx_pkts_vec_sse_simple(void *tx_queue,
			struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	return sxe2_tx_pkts_vec_sse_common((struct sxe2_tx_queue *)tx_queue,
				tx_pkts, nb_pkts, false);
}
uint16_t sxe2_tx_pkts_vec_sse(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	return sxe2_tx_pkts_vec_sse_common((struct sxe2_tx_queue *)tx_queue,
				tx_pkts, nb_pkts, true);
}

static inline void sxe2_rx_queue_rearm_sse(struct sxe2_rx_queue *rxq)
{
	volatile union sxe2_rx_desc *desc;
	struct rte_mbuf **buffer;
	struct rte_mbuf *mbuf0, *mbuf1;
	__m128i dma_addr0, dma_addr1;
	__m128i virt_addr0, virt_addr1;
	__m128i hdr_room = _mm_set_epi64x(RTE_PKTMBUF_HEADROOM,
				RTE_PKTMBUF_HEADROOM);
	int32_t ret;
	uint16_t i;
	uint16_t new_tail;

	buffer = &rxq->buffer_ring[rxq->realloc_start];
	desc = &rxq->desc_ring[rxq->realloc_start];
	ret = rte_mempool_get_bulk(rxq->mb_pool, (void *)buffer,
			SXE2_RX_REARM_THRESH_VEC);
	if (ret != 0) {
		PMD_LOG_INFO(RX, "Rx mbuf vec alloc failed port_id=%u "
				"queue_id=%u", rxq->port_id, rxq->queue_id);
		if ((rxq->realloc_num + SXE2_RX_REARM_THRESH_VEC) >= rxq->ring_depth) {
			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < SXE2_RX_NUM_PER_LOOP_SSE; ++i) {
				buffer[i] = &rxq->fake_mbuf;
				_mm_store_si128(RTE_CAST_PTR(__m128i *, &desc[i].read),
						dma_addr0);
			}
		}
		rxq->vsi->adapter->dev_info.dev_data->rx_mbuf_alloc_failed +=
				SXE2_RX_REARM_THRESH_VEC;
		goto l_end;
	}
	for (i = 0; i < SXE2_RX_REARM_THRESH_VEC; i += 2, buffer += 2) {
		mbuf0 = buffer[0];
		mbuf1 = buffer[1];
#if RTE_IOVA_IN_MBUF
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, buf_iova) !=
				 offsetof(struct rte_mbuf, buf_addr) + 8);
#endif
		virt_addr0 = _mm_loadu_si128((__m128i *)&mbuf0->buf_addr);
		virt_addr1 = _mm_loadu_si128((__m128i *)&mbuf1->buf_addr);
#if RTE_IOVA_IN_MBUF
		dma_addr0 = _mm_unpackhi_epi64(virt_addr0, virt_addr0);
		dma_addr1 = _mm_unpackhi_epi64(virt_addr1, virt_addr1);
#else
		dma_addr0 = _mm_unpacklo_epi64(virt_addr0, virt_addr0);
		dma_addr1 = _mm_unpacklo_epi64(virt_addr1, virt_addr1);
#endif
		dma_addr0 = _mm_add_epi64(dma_addr0, hdr_room);
		dma_addr1 = _mm_add_epi64(dma_addr1, hdr_room);
		_mm_store_si128(RTE_CAST_PTR(__m128i *, &desc++->read), dma_addr0);
		_mm_store_si128(RTE_CAST_PTR(__m128i *, &desc++->read), dma_addr1);
	}
	rxq->realloc_start += SXE2_RX_REARM_THRESH_VEC;
	if (rxq->realloc_start >= rxq->ring_depth)
		rxq->realloc_start = 0;
	rxq->realloc_num -= SXE2_RX_REARM_THRESH_VEC;
	new_tail = (rxq->realloc_start == 0) ?
		(rxq->ring_depth - 1) : (rxq->realloc_start - 1);
	SXE2_PCI_REG_WRITE_WC(rxq->rdt_reg_addr, new_tail);
l_end:
	return;
}

static __rte_always_inline __m128i
sxe2_rx_desc_fnav_flags_sse(__m128i descs_arr[4])
{
	__m128i descs_tmp1, descs_tmp2;
	__m128i descs_fnav_vld;
	__m128i v_zeros, v_ffff, v_u32_one;
	__m128i m_flags;
	const __m128i fdir_flags = _mm_set1_epi32(RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID);
	descs_tmp1 = _mm_unpacklo_epi32(descs_arr[0], descs_arr[1]);
	descs_tmp2 = _mm_unpacklo_epi32(descs_arr[2], descs_arr[3]);
	descs_fnav_vld = _mm_unpacklo_epi64(descs_tmp1, descs_tmp2);
	descs_fnav_vld = _mm_slli_epi32(descs_fnav_vld, 26);
	descs_fnav_vld = _mm_srli_epi32(descs_fnav_vld, 31);
	v_zeros = _mm_setzero_si128();
	v_ffff = _mm_cmpeq_epi32(v_zeros, v_zeros);
	v_u32_one = _mm_srli_epi32(v_ffff, 31);
	m_flags = _mm_cmpeq_epi32(descs_fnav_vld, v_u32_one);
	m_flags = _mm_and_si128(m_flags, fdir_flags);
	return m_flags;
}

static __rte_always_inline void
sxe2_rx_desc_offloads_para_fill_sse(struct sxe2_rx_queue *rxq,
		volatile union sxe2_rx_desc *desc __rte_unused,
		__m128i descs_arr[4],
		struct rte_mbuf **rx_pkts)
{
	const __m128i mbuf_init = _mm_set_epi64x(0, rxq->mbuf_init_value);
	__m128i rearm_arr[4];
	__m128i tmp_desc_lo, tmp_desc_hi, flags, tmp_flags;
	const __m128i desc_flags_mask = _mm_set_epi32(0x00001C04, 0x00001C04,
						      0x00001C04, 0x00001C04);
	const __m128i desc_flags_rss_mask = _mm_set_epi32(0x20000000, 0x20000000,
							  0x20000000, 0x20000000);
	const __m128i vlan_flags = _mm_set_epi8(0, 0, 0, 0,
						0, 0, 0, 0,
						0, 0, 0, RTE_MBUF_F_RX_VLAN |
						RTE_MBUF_F_RX_VLAN_STRIPPED,
						0, 0, 0, 0);
	const __m128i rss_flags = _mm_set_epi8(0, 0, 0, 0,
						0, 0, 0, 0, 0, 0, 0, RTE_MBUF_F_RX_RSS_HASH,
						0, 0, 0, 0);
	const __m128i cksum_flags =
			_mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0,
				     ((RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
						RTE_MBUF_F_RX_L4_CKSUM_BAD |
						RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1),
				     ((RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
						RTE_MBUF_F_RX_L4_CKSUM_BAD |
						RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1),
				     ((RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
						RTE_MBUF_F_RX_L4_CKSUM_GOOD |
						RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1),
				     ((RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD |
						RTE_MBUF_F_RX_L4_CKSUM_GOOD |
						RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1),
				     ((RTE_MBUF_F_RX_L4_CKSUM_BAD |
						RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1),
				     ((RTE_MBUF_F_RX_L4_CKSUM_BAD |
						RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1),
				     ((RTE_MBUF_F_RX_L4_CKSUM_GOOD |
						RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1),
				     ((RTE_MBUF_F_RX_L4_CKSUM_GOOD |
						RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1));
	const __m128i cksum_mask =
			_mm_set_epi32(RTE_MBUF_F_RX_IP_CKSUM_MASK |
				      RTE_MBUF_F_RX_L4_CKSUM_MASK |
				      RTE_MBUF_F_RX_OUTER_L4_CKSUM_MASK |
				      RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD,
				      RTE_MBUF_F_RX_IP_CKSUM_MASK |
				      RTE_MBUF_F_RX_L4_CKSUM_MASK |
				      RTE_MBUF_F_RX_OUTER_L4_CKSUM_MASK |
				      RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD,
				      RTE_MBUF_F_RX_IP_CKSUM_MASK |
				      RTE_MBUF_F_RX_L4_CKSUM_MASK |
				      RTE_MBUF_F_RX_OUTER_L4_CKSUM_MASK |
				      RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD,
				      RTE_MBUF_F_RX_IP_CKSUM_MASK |
				      RTE_MBUF_F_RX_L4_CKSUM_MASK |
				      RTE_MBUF_F_RX_OUTER_L4_CKSUM_MASK |
				      RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD);
	const __m128i vlan_mask =
			_mm_set_epi32(RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
				      RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED,
				      RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN |
				      RTE_MBUF_F_RX_VLAN_STRIPPED,
				      RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED);
	flags = _mm_unpackhi_epi32(descs_arr[0], descs_arr[1]);
	tmp_flags = _mm_unpackhi_epi32(descs_arr[2], descs_arr[3]);
	tmp_desc_lo = _mm_unpacklo_epi64(flags, tmp_flags);
	tmp_desc_hi = _mm_unpackhi_epi64(flags, tmp_flags);
	tmp_desc_lo = _mm_and_si128(tmp_desc_lo, desc_flags_mask);
	tmp_desc_hi = _mm_and_si128(tmp_desc_hi, desc_flags_rss_mask);
	tmp_flags = _mm_shuffle_epi8(vlan_flags, tmp_desc_lo);
	flags = _mm_and_si128(tmp_flags, vlan_mask);
	tmp_desc_lo = _mm_srli_epi32(tmp_desc_lo, 10);
	tmp_flags = _mm_shuffle_epi8(cksum_flags, tmp_desc_lo);
	tmp_flags = _mm_slli_epi32(tmp_flags, 1);
	tmp_flags = _mm_and_si128(tmp_flags, cksum_mask);
	flags = _mm_or_si128(flags, tmp_flags);
	tmp_desc_hi = _mm_srli_epi32(tmp_desc_hi, 27);
	tmp_flags = _mm_shuffle_epi8(rss_flags, tmp_desc_hi);
	flags = _mm_or_si128(flags, tmp_flags);
#ifndef RTE_LIBRTE_SXE2_16BYTE_RX_DESC
	if (rxq->fnav_enable) {
		__m128i tmp_fnav_flags = sxe2_rx_desc_fnav_flags_sse(descs_arr);
		flags = _mm_or_si128(flags, tmp_fnav_flags);
		rx_pkts[0]->hash.fdir.hi = desc[0].wb.fd_filter_id;
		rx_pkts[1]->hash.fdir.hi = desc[1].wb.fd_filter_id;
		rx_pkts[2]->hash.fdir.hi = desc[2].wb.fd_filter_id;
		rx_pkts[3]->hash.fdir.hi = desc[3].wb.fd_filter_id;
	}
#endif
	rearm_arr[0] = _mm_blend_epi16(mbuf_init, _mm_slli_si128(flags, 8), 0x30);
	rearm_arr[1] = _mm_blend_epi16(mbuf_init, _mm_slli_si128(flags, 4), 0x30);
	rearm_arr[2] = _mm_blend_epi16(mbuf_init, flags, 0x30);
	rearm_arr[3] = _mm_blend_epi16(mbuf_init, _mm_srli_si128(flags, 4), 0x30);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, ol_flags) !=
			 offsetof(struct rte_mbuf, rearm_data) + 8);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, rearm_data) !=
			 RTE_ALIGN(offsetof(struct rte_mbuf, rearm_data), 16));
	_mm_store_si128(RTE_CAST_PTR(__m128i *, &rx_pkts[0]->rearm_data), rearm_arr[0]);
	_mm_store_si128(RTE_CAST_PTR(__m128i *, &rx_pkts[1]->rearm_data), rearm_arr[1]);
	_mm_store_si128(RTE_CAST_PTR(__m128i *, &rx_pkts[2]->rearm_data), rearm_arr[2]);
	_mm_store_si128(RTE_CAST_PTR(__m128i *, &rx_pkts[3]->rearm_data), rearm_arr[3]);
}

static inline uint16_t
sxe2_rx_pkts_common_vec_sse(struct sxe2_rx_queue *rxq,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts, uint8_t *split_rxe_flags,
		uint8_t *umbcast_flags)
{
	volatile union sxe2_rx_desc *desc;
	struct rte_mbuf **buffer;
	__m128i descs_arr[SXE2_RX_NUM_PER_LOOP_SSE];
	__m128i mbuf_arr[SXE2_RX_NUM_PER_LOOP_SSE];
	__m128i staterr, sterr_tmp1, sterr_tmp2;
	__m128i pmbuf0;
	__m128i ptype_all;
#ifdef RTE_ARCH_X86_64
	__m128i pmbuf1;
#endif
	uint32_t i;
	uint32_t bit_num;
	uint16_t done_num = 0;
	const __m128i crc_adjust =
			_mm_set_epi16(0, 0, 0,
				      -rxq->crc_len,
				      0, -rxq->crc_len,
				      0, 0);
	const __m128i rvp_shuf_mask =
			_mm_set_epi8(7, 6, 5, 4,
				     3, 2,
				     13, 12,
				     0XFF, 0xFF, 13, 12,
				     0xFF, 0xFF, 0xFF, 0xFF);
	const __m128i dd_mask = _mm_set_epi64x(0x0000000100000001LL,
					0x0000000100000001LL);
	const __m128i eop_mask = _mm_slli_epi32(dd_mask,
					SXE2_RX_DESC_STATUS_EOP_SHIFT);
	const __m128i rxe_mask = _mm_set_epi64x(0x0000208000002080LL,
					0x0000208000002080LL);
	const __m128i eop_shuf_mask = _mm_set_epi8(0xFF, 0xFF,
						   0xFF, 0xFF,
						   0xFF, 0xFF,
						   0xFF, 0xFF,
						   0xFF, 0xFF,
						   0xFF, 0xFF,
						   0x04, 0x0C,
						   0x00, 0x08);
	const __m128i ptype_mask = _mm_set_epi16(SXE2_RX_DESC_PTYPE_MASK_NO_SHIFT, 0,
						 SXE2_RX_DESC_PTYPE_MASK_NO_SHIFT, 0,
						 SXE2_RX_DESC_PTYPE_MASK_NO_SHIFT, 0,
						 SXE2_RX_DESC_PTYPE_MASK_NO_SHIFT, 0);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, vlan_tci) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 10);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, hash) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 12);
	desc = &rxq->desc_ring[rxq->processing_idx];
	rte_prefetch0(desc);
	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, SXE2_RX_NUM_PER_LOOP_SSE);
	if (rxq->realloc_num > SXE2_RX_REARM_THRESH_VEC)
		sxe2_rx_queue_rearm_sse(rxq);
	if ((rte_le_to_cpu_64(desc->wb.status_err_ptype_len) &
		     SXE2_RX_DESC_STATUS_DD_MASK) == 0)
		goto l_end;
	buffer = &rxq->buffer_ring[rxq->processing_idx];
	for (i = 0; i < nb_pkts; i += SXE2_RX_NUM_PER_LOOP_SSE,
				desc += SXE2_RX_NUM_PER_LOOP_SSE) {
		pmbuf0 = _mm_loadu_si128(RTE_CAST_PTR(__m128i *, &buffer[i]));
		descs_arr[3] = _mm_loadu_si128(RTE_CAST_PTR(__m128i *, desc + 3));
		rte_compiler_barrier();
		_mm_storeu_si128((__m128i *)&rx_pkts[i], pmbuf0);
#ifdef RTE_ARCH_X86_64
		pmbuf1 = _mm_loadu_si128((__m128i *)&buffer[i + 2]);
#endif
		descs_arr[2] = _mm_loadu_si128(RTE_CAST_PTR(__m128i *, desc + 2));
		rte_compiler_barrier();
		descs_arr[1] = _mm_loadu_si128(RTE_CAST_PTR(__m128i *, desc + 1));
		rte_compiler_barrier();
		descs_arr[0] = _mm_loadu_si128(RTE_CAST_PTR(__m128i *, desc));
#ifdef RTE_ARCH_X86_64
		_mm_storeu_si128((__m128i *)&rx_pkts[i + 2], pmbuf1);
#endif
		if (split_rxe_flags) {
			rte_mbuf_prefetch_part2(rx_pkts[i]);
			rte_mbuf_prefetch_part2(rx_pkts[i + 1]);
			rte_mbuf_prefetch_part2(rx_pkts[i + 2]);
			rte_mbuf_prefetch_part2(rx_pkts[i + 3]);
		}
		rte_compiler_barrier();
		mbuf_arr[3] = _mm_shuffle_epi8(descs_arr[3], rvp_shuf_mask);
		mbuf_arr[2] = _mm_shuffle_epi8(descs_arr[2], rvp_shuf_mask);
		mbuf_arr[1] = _mm_shuffle_epi8(descs_arr[1], rvp_shuf_mask);
		mbuf_arr[0] = _mm_shuffle_epi8(descs_arr[0], rvp_shuf_mask);
		sterr_tmp2 = _mm_unpackhi_epi32(descs_arr[3], descs_arr[2]);
		sterr_tmp1 = _mm_unpackhi_epi32(descs_arr[1], descs_arr[0]);
		sxe2_rx_desc_offloads_para_fill_sse(rxq, desc, descs_arr, rx_pkts);
		mbuf_arr[3] = _mm_add_epi16(mbuf_arr[3], crc_adjust);
		mbuf_arr[2] = _mm_add_epi16(mbuf_arr[2], crc_adjust);
		mbuf_arr[1] = _mm_add_epi16(mbuf_arr[1], crc_adjust);
		mbuf_arr[0] = _mm_add_epi16(mbuf_arr[0], crc_adjust);
		staterr = _mm_unpacklo_epi32(sterr_tmp1, sterr_tmp2);
		ptype_all = _mm_and_si128(staterr, ptype_mask);
		_mm_storeu_si128((void *)&rx_pkts[i + 3]->rx_descriptor_fields1,
					mbuf_arr[3]);
		_mm_storeu_si128((void *)&rx_pkts[i + 2]->rx_descriptor_fields1,
					mbuf_arr[2]);
		if (umbcast_flags != NULL) {
			const __m128i umbcast_mask =
				_mm_set_epi32(SXE2_RX_DESC_STATUS_UMBCAST_MASK,
					      SXE2_RX_DESC_STATUS_UMBCAST_MASK,
					      SXE2_RX_DESC_STATUS_UMBCAST_MASK,
					      SXE2_RX_DESC_STATUS_UMBCAST_MASK);
			const __m128i umbcast_shuf_mask =
				_mm_set_epi8(0xFF, 0xFF,
					     0xFF, 0xFF,
					     0xFF, 0xFF,
					     0xFF, 0xFF,
					     0xFF, 0xFF,
					     0xFF, 0xFF,
					     0x07, 0x0F,
					     0x03, 0x0B);
			__m128i umbcast_bits = _mm_and_si128(staterr, umbcast_mask);
			umbcast_bits = _mm_shuffle_epi8(umbcast_bits, umbcast_shuf_mask);
			*(int32_t *)umbcast_flags = _mm_cvtsi128_si32(umbcast_bits);
			umbcast_flags += SXE2_RX_NUM_PER_LOOP_SSE;
		}
		if (split_rxe_flags != NULL) {
			__m128i eop_bits = _mm_andnot_si128(staterr, eop_mask);
			__m128i rxe_bits = _mm_and_si128(staterr, rxe_mask);
			rxe_bits = _mm_srli_epi32(rxe_bits, 7);
			eop_bits = _mm_or_si128(eop_bits, rxe_bits);
			eop_bits = _mm_shuffle_epi8(eop_bits, eop_shuf_mask);
			*(int32_t *)split_rxe_flags = _mm_cvtsi128_si32(eop_bits);
			split_rxe_flags += SXE2_RX_NUM_PER_LOOP_SSE;
		}
		staterr = _mm_and_si128(staterr, dd_mask);
		staterr = _mm_packs_epi32(staterr, _mm_setzero_si128());
		_mm_storeu_si128((void *)&rx_pkts[i + 1]->rx_descriptor_fields1,
					mbuf_arr[1]);
		_mm_storeu_si128((void *)&rx_pkts[i]->rx_descriptor_fields1,
					mbuf_arr[0]);
		rx_pkts[i + 3]->packet_type = sxe2_ptype_tbl[_mm_extract_epi16(ptype_all, 3)];
		rx_pkts[i + 2]->packet_type = sxe2_ptype_tbl[_mm_extract_epi16(ptype_all, 7)];
		rx_pkts[i + 1]->packet_type = sxe2_ptype_tbl[_mm_extract_epi16(ptype_all, 1)];
		rx_pkts[i]->packet_type     = sxe2_ptype_tbl[_mm_extract_epi16(ptype_all, 5)];
		bit_num = rte_popcount64(_mm_cvtsi128_si64(staterr));
		done_num += bit_num;
		if (likely(bit_num != SXE2_RX_NUM_PER_LOOP_SSE))
			break;
	}
	rxq->processing_idx += done_num;
	rxq->processing_idx &= (rxq->ring_depth - 1);
	rxq->realloc_num    += done_num;
	PMD_LOG_DEBUG(RX, "port_id=%u queue_id=%u last_id=%u recv_pkts=%d",
			rxq->port_id, rxq->queue_id, rxq->processing_idx, done_num);
l_end:
	return done_num;
}

static __rte_always_inline uint16_t
sxe2_rx_pkts_scattered_batch_vec_sse(struct sxe2_rx_queue *rxq,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	uint8_t split_rxe_flags[SXE2_RX_PKTS_BURST_BATCH_NUM_VEC] = {0};
	uint8_t umbcast_flags[SXE2_RX_PKTS_BURST_BATCH_NUM_VEC] = {0};
	uint16_t rx_done_num;
	uint16_t rx_pkt_done_num;
	rx_pkt_done_num = 0;

	rx_done_num = sxe2_rx_pkts_common_vec_sse(rxq, rx_pkts,
			nb_pkts, split_rxe_flags, umbcast_flags);
	if (rx_done_num == 0)
		goto l_end;
	rx_pkt_done_num += sxe2_rx_pkts_refactor(rxq, &rx_pkts[rx_pkt_done_num],
			rx_done_num - rx_pkt_done_num, &split_rxe_flags[rx_pkt_done_num],
			&umbcast_flags[rx_pkt_done_num]);
l_end:
	return rx_pkt_done_num;
}

uint16_t sxe2_rx_pkts_scattered_vec_sse_offload(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	uint16_t done_num = 0;
	uint16_t once_num;

	while (nb_pkts > SXE2_RX_PKTS_BURST_BATCH_NUM_VEC) {
		once_num =
			sxe2_rx_pkts_scattered_batch_vec_sse((struct sxe2_rx_queue *)rx_queue,
							     rx_pkts + done_num,
							     SXE2_RX_PKTS_BURST_BATCH_NUM_VEC);
		done_num += once_num;
		nb_pkts  -= once_num;
		if (once_num < SXE2_RX_PKTS_BURST_BATCH_NUM_VEC)
			goto l_end;
	}
	done_num +=
		sxe2_rx_pkts_scattered_batch_vec_sse((struct sxe2_rx_queue *)rx_queue,
						     rx_pkts + done_num, nb_pkts);
l_end:
	return done_num;
}
