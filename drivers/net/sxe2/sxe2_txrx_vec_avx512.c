/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_TEST
#include <rte_vect.h>

#include "sxe2_ethdev.h"
#include "sxe2_common_log.h"
#include "sxe2_queue.h"
#include "sxe2_txrx_vec.h"
#include "sxe2_txrx_vec_common.h"
#include "sxe2_vsi.h"

static __rte_always_inline int32_t sxe2_tx_bufs_free_vec_avx512(struct sxe2_tx_queue *txq)
{
	struct sxe2_tx_buffer_vec *buffer;
	struct rte_mbuf *mbuf;
	struct rte_mbuf *mbuf_free_arr[SXE2_TX_FREE_BUFFER_SIZE_MAX_VEC];
	struct rte_mempool *mp;
	struct rte_mempool_cache *cache;
	void **cache_objs;
	uint32_t copied;
	uint32_t i;
	int32_t ret;
	uint16_t rs_thresh;
	uint16_t free_num;

	if (rte_cpu_to_le_64(SXE2_TX_DESC_DTYPE_DESC_DONE) !=
		(txq->desc_ring[txq->next_dd].wb.dd &
			rte_cpu_to_le_64(SXE2_TX_DESC_DTYPE_MASK))) {
		ret = 0;
		goto l_end;
	}

	rs_thresh = txq->rs_thresh;

	buffer = (struct sxe2_tx_buffer_vec *)txq->buffer_ring;
	buffer += txq->next_dd - (rs_thresh - 1);

	if ((txq->offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) &&
			(rs_thresh & 31) == 0) {
		mp = buffer[0].mbuf->pool;
		cache = rte_mempool_default_cache(mp, rte_lcore_id());

		if (cache == NULL || cache->len)
			goto normal;

		if (rs_thresh > RTE_MEMPOOL_CACHE_MAX_SIZE) {
			(void)rte_mempool_ops_enqueue_bulk(mp, (void *)buffer, rs_thresh);
			goto done;
		}
		cache_objs = &cache->objs[cache->len];

		copied = 0;
		while (copied < rs_thresh) {
			const __m512i objs0 = _mm512_loadu_si512(&buffer[copied]);
			const __m512i objs1 = _mm512_loadu_si512(&buffer[copied + 8]);
			const __m512i objs2 = _mm512_loadu_si512(&buffer[copied + 16]);
			const __m512i objs3 = _mm512_loadu_si512(&buffer[copied + 24]);

			_mm512_storeu_si512(&cache_objs[copied], objs0);
			_mm512_storeu_si512(&cache_objs[copied + 8], objs1);
			_mm512_storeu_si512(&cache_objs[copied + 16], objs2);
			_mm512_storeu_si512(&cache_objs[copied + 24], objs3);
			copied += 32;
		}
		cache->len += rs_thresh;

		if (cache->len >= cache->flushthresh) {
			(void)rte_mempool_ops_enqueue_bulk(mp,
					&cache->objs[cache->size], cache->len - cache->size);
			cache->len = cache->size;
		}
		goto done;
	}

normal:
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

done:
	txq->desc_free_num += txq->rs_thresh;
	txq->next_dd       += txq->rs_thresh;
	if (txq->next_dd >= txq->ring_depth)
		txq->next_dd = txq->rs_thresh - 1;
	ret = rs_thresh;

l_end:
	return ret;
}

static __rte_always_inline void
sxe2_tx_desc_fill_one_avx512(volatile union sxe2_tx_data_desc *desc, struct rte_mbuf *pkt,
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

static __rte_always_inline
void sxe2_tx_desc_fill_avx512(volatile union sxe2_tx_data_desc *desc, struct rte_mbuf **pkts,
	uint16_t pkts_num, uint64_t desc_cmd, bool with_offloads)
{
	__m512i desc_group;
	uint64_t desc0_qw1;
	uint64_t desc1_qw1;
	uint64_t desc2_qw1;
	uint64_t desc3_qw1;

	const uint64_t desc_qw1_com = (SXE2_TX_DESC_DTYPE_DATA |
					((uint64_t)desc_cmd) << SXE2_TX_DATA_DESC_CMD_SHIFT);
	uint32_t desc_offset[4] = {0};

	while (pkts_num > 3) {
		desc3_qw1 = desc_qw1_com |
				((uint64_t)pkts[3]->data_len) << SXE2_TX_DATA_DESC_BUF_SZ_SHIFT;

		desc_offset[3] = SXE2_TX_DATA_DESC_MACLEN_VAL(pkts[3]->l2_len);
		desc3_qw1 |= ((uint64_t)desc_offset[3]) << SXE2_TX_DATA_DESC_OFFSET_SHIFT;
		if (with_offloads)
			sxe2_tx_desc_fill_offloads(pkts[3], &desc3_qw1);

		desc2_qw1 = desc_qw1_com |
				((uint64_t)pkts[2]->data_len) << SXE2_TX_DATA_DESC_BUF_SZ_SHIFT;
		desc_offset[2] = SXE2_TX_DATA_DESC_MACLEN_VAL(pkts[2]->l2_len);
		desc2_qw1 |= ((uint64_t)desc_offset[2]) << SXE2_TX_DATA_DESC_OFFSET_SHIFT;
		if (with_offloads)
			sxe2_tx_desc_fill_offloads(pkts[2], &desc2_qw1);

		desc1_qw1 = (desc_qw1_com |
				((uint64_t)pkts[1]->data_len) << SXE2_TX_DATA_DESC_BUF_SZ_SHIFT);
		desc_offset[1] = SXE2_TX_DATA_DESC_MACLEN_VAL(pkts[1]->l2_len);
		desc1_qw1 |= ((uint64_t)desc_offset[1]) << SXE2_TX_DATA_DESC_OFFSET_SHIFT;
		if (with_offloads)
			sxe2_tx_desc_fill_offloads(pkts[1], &desc1_qw1);

		desc0_qw1 = (desc_qw1_com |
				((uint64_t)pkts[0]->data_len) << SXE2_TX_DATA_DESC_BUF_SZ_SHIFT);
		desc_offset[0] = SXE2_TX_DATA_DESC_MACLEN_VAL(pkts[0]->l2_len);
		desc0_qw1 |= ((uint64_t)desc_offset[0]) << SXE2_TX_DATA_DESC_OFFSET_SHIFT;
		if (with_offloads)
			sxe2_tx_desc_fill_offloads(pkts[0], &desc0_qw1);

		desc_group =
			_mm512_set_epi64(desc3_qw1, rte_pktmbuf_iova(pkts[3]),
					 desc2_qw1, rte_pktmbuf_iova(pkts[2]),
					 desc1_qw1, rte_pktmbuf_iova(pkts[1]),
					 desc0_qw1, rte_pktmbuf_iova(pkts[0]));

		_mm512_storeu_si512(RTE_CAST_PTR(void *, desc), desc_group);

		pkts_num -= 4;
		desc     += 4;
		pkts     += 4;
	}

	while (pkts_num) {
		sxe2_tx_desc_fill_one_avx512(desc, *pkts, desc_cmd, with_offloads);

		pkts_num--;
		desc++;
		pkts++;
	}
}

static __rte_always_inline void
sxe2_tx_pkts_mbuf_fill_avx512(struct sxe2_tx_buffer_vec *buffer,
	struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	uint16_t i;

	for (i = 0; i < nb_pkts; ++i)
		buffer[i].mbuf = tx_pkts[i];
}

static __rte_always_inline uint16_t
sxe2_tx_pkts_vec_avx512_batch(struct sxe2_tx_queue *txq, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts, bool with_offloads)
{
	volatile union sxe2_tx_data_desc *desc;
	struct sxe2_tx_buffer_vec *buffer;
	uint16_t next_use;
	uint16_t res_num;
	uint16_t tx_num;

	if (txq->desc_free_num < txq->free_thresh)
		(void)sxe2_tx_bufs_free_vec_avx512(txq);

	nb_pkts = RTE_MIN(txq->desc_free_num, nb_pkts);
	if (unlikely(nb_pkts == 0)) {
		PMD_LOG_DEBUG(TX, "Tx pkts avx512 batch: may not enough free desc, "
				"free_desc=%u, need_tx_pkts=%u",
				txq->desc_free_num, nb_pkts);
		goto l_end;
	}
	tx_num = nb_pkts;

	next_use = txq->next_use;
	desc     = &txq->desc_ring[next_use];
	buffer   = (struct sxe2_tx_buffer_vec *)txq->buffer_ring;
	buffer  += next_use;

	txq->desc_free_num -= nb_pkts;

	res_num = txq->ring_depth - txq->next_use;

	if (tx_num >= res_num) {
		sxe2_tx_pkts_mbuf_fill_avx512(buffer, tx_pkts, res_num);

		sxe2_tx_desc_fill_avx512(desc, tx_pkts, res_num,
					SXE2_TX_DATA_DESC_CMD_EOP, with_offloads);
		tx_pkts += (res_num - 1);
		desc    += (res_num - 1);

		sxe2_tx_desc_fill_one_avx512(desc, *tx_pkts++,
					(SXE2_TX_DATA_DESC_CMD_EOP | SXE2_TX_DATA_DESC_CMD_RS),
					with_offloads);

		tx_num -= res_num;

		next_use     = 0;
		txq->next_rs = txq->rs_thresh - 1;
		desc         = txq->desc_ring;
		buffer       = (struct sxe2_tx_buffer_vec *)txq->buffer_ring;
	}

	sxe2_tx_pkts_mbuf_fill_avx512(buffer, tx_pkts, tx_num);

	sxe2_tx_desc_fill_avx512(desc, tx_pkts, tx_num,
			SXE2_TX_DATA_DESC_CMD_EOP, with_offloads);

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
sxe2_tx_pkts_vec_avx512_common(struct sxe2_tx_queue *txq, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts, bool with_offloads)
{
	uint16_t tx_done_num = 0;
	uint16_t tx_once_num;
	uint16_t tx_need_num;

	while (nb_pkts) {
		tx_need_num = RTE_MIN(nb_pkts, txq->rs_thresh);
		tx_once_num =
			sxe2_tx_pkts_vec_avx512_batch(txq, tx_pkts + tx_done_num,
					     tx_need_num, with_offloads);
		nb_pkts     -= tx_once_num;
		tx_done_num += tx_once_num;
		if (tx_once_num < tx_need_num)
			break;
	}

	return tx_done_num;
}

uint16_t sxe2_tx_pkts_vec_avx512_simple(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	return sxe2_tx_pkts_vec_avx512_common((struct sxe2_tx_queue *)tx_queue,
					      tx_pkts, nb_pkts, false);
}

uint16_t sxe2_tx_pkts_vec_avx512(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	return sxe2_tx_pkts_vec_avx512_common((struct sxe2_tx_queue *)tx_queue,
					      tx_pkts, nb_pkts, true);
}

static inline void sxe2_rx_queue_rearm_avx512(struct sxe2_rx_queue *rxq)
{
	volatile union sxe2_rx_desc *desc;
	struct rte_mbuf **buffer;
	struct rte_mbuf *mbuf0, *mbuf1;
	__m128i dma_addr0, dma_addr1;
	__m128i virt_addr0, virt_addr1;
	__m128i hdr_room = _mm_set_epi64x(RTE_PKTMBUF_HEADROOM, RTE_PKTMBUF_HEADROOM);
	int32_t ret;
	uint16_t i;
	uint16_t new_tail;

	buffer = &rxq->buffer_ring[rxq->realloc_start];
	desc   = &rxq->desc_ring[rxq->realloc_start];

	ret = rte_mempool_get_bulk(rxq->mb_pool, (void *)buffer, SXE2_RX_REARM_THRESH_VEC);
	if (ret != 0) {
		if ((rxq->realloc_num + SXE2_RX_REARM_THRESH_VEC) >= rxq->ring_depth) {
			dma_addr0 = _mm_setzero_si128();
			for (i = 0; i < SXE2_RX_NUM_PER_LOOP_AVX; ++i) {
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

	new_tail = (rxq->realloc_start == 0) ? (rxq->ring_depth - 1) :
		(rxq->realloc_start - 1);

	SXE2_PCI_REG_WRITE_WC(rxq->rdt_reg_addr, new_tail);

l_end:
	return;
}

static __rte_always_inline uint16_t
sxe2_rx_pkts_common_vec_avx512(struct sxe2_rx_queue *rxq, struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts, uint8_t *split_rxe_flags,
	uint8_t *umbcast_flags, bool do_offload)
{
	const __m256i mbuf_init = _mm256_set_epi64x(0, 0, 0, rxq->mbuf_init_value);
	struct rte_mbuf **buffer;
	volatile union sxe2_rx_desc *desc;
	__m512i mbufs4_7;
	__m512i mbufs0_3;
	__m256i mbufs6_7;
	__m256i mbufs4_5;
	__m256i mbufs2_3;
	__m256i mbufs0_1;
	uint32_t bit_num  = 0;
	uint16_t done_num = 0;
	uint16_t i = 0;
	uint16_t j = 0;

	buffer   = &rxq->buffer_ring[rxq->processing_idx];
	desc     = &rxq->desc_ring[rxq->processing_idx];

	rte_prefetch0(desc);

	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, SXE2_RX_NUM_PER_LOOP_AVX);

	if (rxq->realloc_num > SXE2_RX_REARM_THRESH_VEC)
		sxe2_rx_queue_rearm_avx512(rxq);

	if (0 == (rte_le_to_cpu_64(desc->wb.status_err_ptype_len) & SXE2_RX_DESC_STATUS_DD_MASK))
		goto l_end;

	const __m512i crc_adjust =
			_mm512_set4_epi32(0, -rxq->crc_len, -rxq->crc_len, 0);

	const __m256i dd_mask = _mm256_set1_epi32(1);

	const __m512i rvp_shuf_mask =
			_mm512_set4_epi32((7 << 24) | (6 << 16) | (5 << 8) | 4,
					  (3 << 24) | (2 << 16) | (13 << 8) | 12,
					  (0xFFU << 24) | (0xFF << 16) | (13 << 8) | 12,
					  0xFFFFFFFF);

	const __m128i eop_shuf_mask =
		_mm_set_epi8(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			     0xFF, 0xFF, 8, 0, 10, 2, 12, 4, 14, 6);

	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, vlan_tci) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 10);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, hash) !=
			offsetof(struct rte_mbuf, rx_descriptor_fields1) + 12);

	for (i = 0; i < nb_pkts; i += SXE2_RX_NUM_PER_LOOP_AVX,
				desc += SXE2_RX_NUM_PER_LOOP_AVX) {
		_mm256_storeu_si256((void *)&rx_pkts[i],
			_mm256_loadu_si256((void *)&buffer[i]));
#ifdef RTE_ARCH_X86_64
		_mm256_storeu_si256((void *)&rx_pkts[i + 4],
			_mm256_loadu_si256((void *)&buffer[i + 4]));
#endif

		const __m128i desc7 = _mm_loadu_si128(RTE_CAST_PTR(const __m128i *, desc + 7));
		rte_compiler_barrier();
		const __m128i desc6 = _mm_loadu_si128(RTE_CAST_PTR(const __m128i *, desc + 6));
		rte_compiler_barrier();
		const __m128i desc5 = _mm_loadu_si128(RTE_CAST_PTR(const __m128i *, desc + 5));
		rte_compiler_barrier();
		const __m128i desc4 = _mm_loadu_si128(RTE_CAST_PTR(const __m128i *, desc + 4));
		rte_compiler_barrier();
		const __m128i desc3 = _mm_loadu_si128(RTE_CAST_PTR(const __m128i *, desc + 3));
		rte_compiler_barrier();
		const __m128i desc2 = _mm_loadu_si128(RTE_CAST_PTR(const __m128i *, desc + 2));
		rte_compiler_barrier();
		const __m128i desc1 = _mm_loadu_si128(RTE_CAST_PTR(const __m128i *, desc + 1));
		rte_compiler_barrier();
		const __m128i desc0 = _mm_loadu_si128(RTE_CAST_PTR(const __m128i *, desc + 0));

		const __m256i descs6_7 =
			_mm256_inserti128_si256(_mm256_castsi128_si256(desc6), desc7, 1);
		const __m256i descs4_5 =
			_mm256_inserti128_si256(_mm256_castsi128_si256(desc4), desc5, 1);
		const __m256i descs2_3 =
			_mm256_inserti128_si256(_mm256_castsi128_si256(desc2), desc3, 1);
		const __m256i descs0_1 =
			_mm256_inserti128_si256(_mm256_castsi128_si256(desc0), desc1, 1);

		const __m512i descs4_7 =
			_mm512_inserti64x4(_mm512_castsi256_si512(descs4_5), descs6_7, 1);
		const __m512i descs0_3 =
			_mm512_inserti64x4(_mm512_castsi256_si512(descs0_1), descs2_3, 1);

		if (split_rxe_flags != NULL) {
			for (j = 0; j < SXE2_RX_NUM_PER_LOOP_AVX; j++)
				rte_mbuf_prefetch_part2(rx_pkts[i + j]);
		}

		mbufs4_7 = _mm512_shuffle_epi8(descs4_7, rvp_shuf_mask);
		mbufs0_3 = _mm512_shuffle_epi8(descs0_3, rvp_shuf_mask);

		mbufs4_7 = _mm512_add_epi32(mbufs4_7, crc_adjust);
		mbufs0_3 = _mm512_add_epi32(mbufs0_3, crc_adjust);

		const __m512i ptype_mask = _mm512_set1_epi64(SXE2_RX_FLEX_DESC_PTYPE_M <<
					SXE2_RX_FLEX_DESC_PTYPE_S);

		__m512i ptypes4_7 = _mm512_and_si512(descs4_7, ptype_mask);
		__m512i ptypes0_3 = _mm512_and_si512(descs0_3, ptype_mask);

		const __m256i ptypes6_7 = _mm512_extracti64x4_epi64(ptypes4_7, 1);
		const __m256i ptypes4_5 = _mm512_extracti64x4_epi64(ptypes4_7, 0);
		const __m256i ptypes2_3 = _mm512_extracti64x4_epi64(ptypes0_3, 1);
		const __m256i ptypes0_1 = _mm512_extracti64x4_epi64(ptypes0_3, 0);

		const uint16_t ptype7 = _mm256_extract_epi16(ptypes6_7, 13);
		const uint16_t ptype6 = _mm256_extract_epi16(ptypes6_7, 5);
		const uint16_t ptype5 = _mm256_extract_epi16(ptypes4_5, 13);
		const uint16_t ptype4 = _mm256_extract_epi16(ptypes4_5, 5);
		const uint16_t ptype3 = _mm256_extract_epi16(ptypes2_3, 13);
		const uint16_t ptype2 = _mm256_extract_epi16(ptypes2_3, 5);
		const uint16_t ptype1 = _mm256_extract_epi16(ptypes0_1, 13);
		const uint16_t ptype0 = _mm256_extract_epi16(ptypes0_1, 5);

		const __m512i ptype_mask4_7 =
				_mm512_set_epi32(0, 0, 0, sxe2_ptype_tbl[ptype7],
						 0, 0, 0, sxe2_ptype_tbl[ptype6],
						 0, 0, 0, sxe2_ptype_tbl[ptype5],
						 0, 0, 0, sxe2_ptype_tbl[ptype4]);
		const __m512i ptype_mask0_3 =
				_mm512_set_epi32(0, 0, 0, sxe2_ptype_tbl[ptype3],
						 0, 0, 0, sxe2_ptype_tbl[ptype2],
						 0, 0, 0, sxe2_ptype_tbl[ptype1],
						 0, 0, 0, sxe2_ptype_tbl[ptype0]);

		mbufs4_7 = _mm512_or_si512(mbufs4_7, ptype_mask4_7);
		mbufs0_3 = _mm512_or_si512(mbufs0_3, ptype_mask0_3);

		mbufs6_7 = _mm512_extracti64x4_epi64(mbufs4_7, 1);
		mbufs4_5 = _mm512_extracti64x4_epi64(mbufs4_7, 0);
		mbufs2_3 = _mm512_extracti64x4_epi64(mbufs0_3, 1);
		mbufs0_1 = _mm512_extracti64x4_epi64(mbufs0_3, 0);

		const __m512i staterr_per_mask =
			_mm512_set_epi32(0x17, 0x1F, 0x07, 0x0F,
					 0x13, 0x1B, 0x03, 0x0B,
					 0x16, 0x1E, 0x06, 0x0E,
					 0x12, 0x1A, 0x02, 0x0A);
		__m512i qw1_0_7 = _mm512_permutex2var_epi32(descs4_7,
							    staterr_per_mask,
							    descs0_3);

		__m256i staterrs0_7 = _mm512_extracti64x4_epi64(qw1_0_7, 0);

		__m256i stu_len0_7 = _mm512_extracti64x4_epi64(qw1_0_7, 1);
		__m256i mbuf_flags = _mm256_setzero_si256();

		if (do_offload) {
			const __m256i desc_flags_mask = _mm256_set1_epi32(0xC0001C04);
			const __m256i desc_flags_rss_mask = _mm256_set1_epi32(0x20000000);
			const __m256i vlan_flags =
				_mm256_set_epi8(0, 0, 0, 0,
					0, 0, 0, 0,
					0, 0, 0, RTE_MBUF_F_RX_VLAN |
						RTE_MBUF_F_RX_VLAN_STRIPPED,
					0, 0, 0, 0,
					0, 0, 0, 0,
					0, 0, 0, 0,
					0, 0, 0, RTE_MBUF_F_RX_VLAN |
						RTE_MBUF_F_RX_VLAN_STRIPPED,
					0, 0, 0, 0);

			const __m256i rss_flags =
				_mm256_set_epi8(0, 0, 0, 0,
					0, 0, 0, 0,
					0, 0, 0, RTE_MBUF_F_RX_RSS_HASH,
					0, 0, 0, 0,
					0, 0, 0, 0,
					0, 0, 0, 0,
					0, 0, 0, RTE_MBUF_F_RX_RSS_HASH,
					0, 0, 0, 0);

			const __m256i cksum_flags =
			_mm256_set_epi8(0, 0, 0, 0, 0, 0, 0,
			0,
			((RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1),
			((RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1),
			((RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1),
			((RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1),
			((RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1),
			((RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1),
			((RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1),
			((RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1),
			0, 0, 0, 0, 0, 0, 0, 0,
			((RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1),
			((RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD |
				RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1),
			((RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1),
			((RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_GOOD |
				RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1),
			((RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1),
			((RTE_MBUF_F_RX_L4_CKSUM_BAD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1),
			((RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_BAD) >> 1),
			((RTE_MBUF_F_RX_L4_CKSUM_GOOD | RTE_MBUF_F_RX_IP_CKSUM_GOOD) >> 1));

			const __m256i cksum_mask =
				_mm256_set1_epi32(RTE_MBUF_F_RX_IP_CKSUM_MASK |
						  RTE_MBUF_F_RX_L4_CKSUM_MASK |
						  RTE_MBUF_F_RX_OUTER_L4_CKSUM_MASK |
						  RTE_MBUF_F_RX_OUTER_IP_CKSUM_BAD);

			const __m256i vlan_mask =
				_mm256_set1_epi32(RTE_MBUF_F_RX_VLAN |
						  RTE_MBUF_F_RX_VLAN_STRIPPED);

			__m256i tmp_flags;
			__m256i descs_flags = _mm256_and_si256(staterrs0_7, desc_flags_mask);
			stu_len0_7 = _mm256_and_si256(stu_len0_7, desc_flags_rss_mask);

			tmp_flags = _mm256_shuffle_epi8(vlan_flags, descs_flags);
			mbuf_flags = _mm256_and_si256(tmp_flags, vlan_mask);

			descs_flags = _mm256_srli_epi32(descs_flags, 10);
			tmp_flags   = _mm256_shuffle_epi8(cksum_flags, descs_flags);
			tmp_flags   = _mm256_slli_epi32(tmp_flags, 1);
			tmp_flags   = _mm256_and_si256(tmp_flags, cksum_mask);
			mbuf_flags  = _mm256_or_si256(mbuf_flags, tmp_flags);

			descs_flags = _mm256_srli_epi32(stu_len0_7, 27);
			tmp_flags   = _mm256_shuffle_epi8(rss_flags, descs_flags);
			mbuf_flags  = _mm256_or_si256(mbuf_flags, tmp_flags);

#ifndef RTE_LIBRTE_SXE2_16BYTE_RX_DESC
			if (rxq->fnav_enable) {
				__m256i fnav_vld0_3, fnav_vld4_7;
				__m256i fnav_vld0_7;
				__m256i v_zeros, v_ffff, v_u32_one;
				const __m256i fdir_flags =
					_mm256_set1_epi32(RTE_MBUF_F_RX_FDIR |
							  RTE_MBUF_F_RX_FDIR_ID);
				fnav_vld0_3 = _mm256_unpacklo_epi32(descs2_3, descs0_1);
				fnav_vld4_7 = _mm256_unpacklo_epi32(descs6_7, descs4_5);

				fnav_vld0_7 = _mm256_unpacklo_epi64(fnav_vld4_7, fnav_vld0_3);

				fnav_vld0_7 = _mm256_slli_epi32(fnav_vld0_7, 26);
				fnav_vld0_7 = _mm256_srli_epi32(fnav_vld0_7, 31);

				v_zeros = _mm256_setzero_si256();
				v_ffff = _mm256_cmpeq_epi32(v_zeros, v_zeros);
				v_u32_one = _mm256_srli_epi32(v_ffff, 31);

				tmp_flags = _mm256_cmpeq_epi32(fnav_vld0_7, v_u32_one);

				tmp_flags = _mm256_and_si256(tmp_flags, fdir_flags);

				mbuf_flags = _mm256_or_si256(mbuf_flags, tmp_flags);

				rx_pkts[i + 0]->hash.fdir.hi = desc[0].wb.fd_filter_id;
				rx_pkts[i + 1]->hash.fdir.hi = desc[1].wb.fd_filter_id;
				rx_pkts[i + 2]->hash.fdir.hi = desc[2].wb.fd_filter_id;
				rx_pkts[i + 3]->hash.fdir.hi = desc[3].wb.fd_filter_id;
				rx_pkts[i + 4]->hash.fdir.hi = desc[4].wb.fd_filter_id;
				rx_pkts[i + 5]->hash.fdir.hi = desc[5].wb.fd_filter_id;
				rx_pkts[i + 6]->hash.fdir.hi = desc[6].wb.fd_filter_id;
				rx_pkts[i + 7]->hash.fdir.hi = desc[7].wb.fd_filter_id;
			}
#endif
		}

		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, ol_flags) !=
				offsetof(struct rte_mbuf, rearm_data) + 8);
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, rx_descriptor_fields1) !=
				offsetof(struct rte_mbuf, rearm_data) + 16);
		RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, rearm_data) !=
				RTE_ALIGN(offsetof(struct rte_mbuf, rearm_data), 16));

		__m256i rearm_arr[8];

		rearm_arr[6] = _mm256_blend_epi32(mbuf_init,
					_mm256_slli_si256(mbuf_flags, 8), 0x04);
		rearm_arr[4] = _mm256_blend_epi32(mbuf_init,
					_mm256_slli_si256(mbuf_flags, 4), 0x04);
		rearm_arr[2] = _mm256_blend_epi32(mbuf_init, mbuf_flags, 0x04);
		rearm_arr[0] = _mm256_blend_epi32(mbuf_init,
					_mm256_srli_si256(mbuf_flags, 4), 0x04);

		rearm_arr[6] = _mm256_permute2f128_si256(rearm_arr[6], mbufs6_7, 0x20);
		rearm_arr[4] = _mm256_permute2f128_si256(rearm_arr[4], mbufs4_5, 0x20);
		rearm_arr[2] = _mm256_permute2f128_si256(rearm_arr[2], mbufs2_3, 0x20);
		rearm_arr[0] = _mm256_permute2f128_si256(rearm_arr[0], mbufs0_1, 0x20);

		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 6]->rearm_data, rearm_arr[6]);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 4]->rearm_data, rearm_arr[4]);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 2]->rearm_data, rearm_arr[2]);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 0]->rearm_data, rearm_arr[0]);

		const __m256i tmp_mbuf_flags =
				_mm256_castsi128_si256(_mm256_extracti128_si256(mbuf_flags, 1));

		rearm_arr[7] = _mm256_blend_epi32(mbuf_init,
					_mm256_slli_si256(tmp_mbuf_flags, 8), 4);
		rearm_arr[5] = _mm256_blend_epi32(mbuf_init,
					_mm256_slli_si256(tmp_mbuf_flags, 4), 4);
		rearm_arr[3] = _mm256_blend_epi32(mbuf_init, tmp_mbuf_flags, 4);
		rearm_arr[1] = _mm256_blend_epi32(mbuf_init,
					_mm256_srli_si256(tmp_mbuf_flags, 4), 4);

		rearm_arr[7] = _mm256_blend_epi32(rearm_arr[7], mbufs6_7, 0XF0);
		rearm_arr[5] = _mm256_blend_epi32(rearm_arr[5], mbufs4_5, 0XF0);
		rearm_arr[3] = _mm256_blend_epi32(rearm_arr[3], mbufs2_3, 0XF0);
		rearm_arr[1] = _mm256_blend_epi32(rearm_arr[1], mbufs0_1, 0XF0);

		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 7]->rearm_data, rearm_arr[7]);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 5]->rearm_data, rearm_arr[5]);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 3]->rearm_data, rearm_arr[3]);
		_mm256_storeu_si256((__m256i *)&rx_pkts[i + 1]->rearm_data, rearm_arr[1]);

		if (umbcast_flags) {
			const __m256i umbcast_mask =
				_mm256_set1_epi32(SXE2_RX_DESC_STATUS_UMBCAST_MASK);
			__m256i umbcast_bits_256 =
				_mm256_and_si256(staterrs0_7, umbcast_mask);

			umbcast_bits_256 = _mm256_srli_epi32(umbcast_bits_256, 24);
			__m128i umbcast_bits_128 =
				_mm_packs_epi32(_mm256_castsi256_si128(umbcast_bits_256),
						_mm256_extractf128_si256(umbcast_bits_256, 1));

			umbcast_bits_128 = _mm_shuffle_epi8(umbcast_bits_128, eop_shuf_mask);

			*(uint64_t *)umbcast_flags = _mm_cvtsi128_si64(umbcast_bits_128);
			umbcast_flags += SXE2_RX_NUM_PER_LOOP_AVX;
		}

		if (split_rxe_flags) {
			const __m256i eop_rxe_mask =
					_mm256_set1_epi32(SXE2_RX_DESC_STATUS_EOP_MASK |
								SXE2_RX_DESC_ERROR_RXE_MASK |
								SXE2_RX_DESC_ERROR_OVERSIZE_MASK);
			const __m128i eop_mask_128 =
					_mm_set1_epi16(SXE2_RX_DESC_STATUS_EOP_MASK);
			const __m128i rxe_mask_128 =
					_mm_set1_epi16(SXE2_RX_DESC_ERROR_RXE_MASK |
							SXE2_RX_DESC_ERROR_OVERSIZE_MASK);

			const __m256i tmp_stats = _mm256_and_si256(staterrs0_7, eop_rxe_mask);

			const __m128i eop_rxe_bits = _mm_packs_epi32
							(_mm256_castsi256_si128(tmp_stats),
							 _mm256_extractf128_si256(tmp_stats, 1));

			__m128i not_eop_bits = _mm_andnot_si128(eop_rxe_bits, eop_mask_128);

			not_eop_bits =
				_mm_or_si128(not_eop_bits,
					     _mm_srli_epi16(_mm_and_si128(eop_rxe_bits,
									       rxe_mask_128),
							      7));

			not_eop_bits = _mm_shuffle_epi8(not_eop_bits, eop_shuf_mask);

			*(uint64_t *)split_rxe_flags = _mm_cvtsi128_si64(not_eop_bits);
			split_rxe_flags += SXE2_RX_NUM_PER_LOOP_AVX;
		}

		staterrs0_7 = _mm256_and_si256(staterrs0_7, dd_mask);

		staterrs0_7 = _mm256_packs_epi32(staterrs0_7, _mm256_setzero_si256());

		bit_num = rte_popcount64
				(_mm_cvtsi128_si64(_mm256_extracti128_si256(staterrs0_7, 1)));
		bit_num += rte_popcount64
				(_mm_cvtsi128_si64(_mm256_castsi256_si128(staterrs0_7)));
		done_num += bit_num;

		if (bit_num != SXE2_RX_NUM_PER_LOOP_AVX)
			break;
	}

	rxq->processing_idx += done_num;
	rxq->processing_idx &= (rxq->ring_depth - 1);
	if ((rxq->processing_idx & 1) == 1 && done_num > 1) {
		rxq->processing_idx--;
		done_num--;
	}
	rxq->realloc_num     += done_num;

l_end:
	PMD_LOG_DEBUG(RX, "port_id=%u queue_id=%u last_id=%u recv_pkts=%d",
			rxq->port_id, rxq->queue_id, rxq->processing_idx, done_num);
	return done_num;
}

static __rte_always_inline uint16_t
sxe2_rx_pkts_scattered_batch_vec_avx512(struct sxe2_rx_queue *rxq, struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts, bool do_offload)
{
	uint8_t split_rxe_flags[SXE2_RX_PKTS_BURST_BATCH_NUM_VEC] = {0};
	uint8_t umbcast_flags[SXE2_RX_PKTS_BURST_BATCH_NUM_VEC] = {0};
	uint16_t rx_done_num;
	uint16_t rx_pkt_done_num;

	rx_pkt_done_num = 0;

	rx_done_num = sxe2_rx_pkts_common_vec_avx512(rxq, rx_pkts,
			nb_pkts, split_rxe_flags,
			umbcast_flags, do_offload);
	if (rx_done_num == 0)
		goto l_end;

	rx_pkt_done_num += sxe2_rx_pkts_refactor(rxq, &rx_pkts[rx_pkt_done_num],
			rx_done_num - rx_pkt_done_num, &split_rxe_flags[rx_pkt_done_num],
			&umbcast_flags[rx_pkt_done_num]);

l_end:

	return rx_pkt_done_num;
}

static __rte_always_inline uint16_t
sxe2_rx_pkts_scattered_common_vec_avx512(void *rx_queue,
	struct rte_mbuf **rx_pkts, uint16_t nb_pkts, bool offload)
{
	uint16_t done_num = 0;
	uint16_t once_num  = 0;

	while (nb_pkts > SXE2_RX_PKTS_BURST_BATCH_NUM) {
		once_num = sxe2_rx_pkts_scattered_batch_vec_avx512(rx_queue, rx_pkts + done_num,
			SXE2_RX_PKTS_BURST_BATCH_NUM, offload);

		done_num  += once_num;
		nb_pkts -= once_num;

		if (once_num < SXE2_RX_PKTS_BURST_BATCH_NUM)
			goto end;
	}

	done_num += sxe2_rx_pkts_scattered_batch_vec_avx512(rx_queue,
		rx_pkts + done_num, nb_pkts, offload);

end:
	return done_num;
}

uint16_t sxe2_rx_pkts_scattered_vec_avx512(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	return sxe2_rx_pkts_scattered_common_vec_avx512(rx_queue,
			rx_pkts, nb_pkts, false);
}

uint16_t sxe2_rx_pkts_scattered_vec_avx512_offload(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	return sxe2_rx_pkts_scattered_common_vec_avx512(rx_queue,
			rx_pkts, nb_pkts, true);
}

#endif
