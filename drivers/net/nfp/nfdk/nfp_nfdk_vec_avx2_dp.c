/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Corigine, Inc.
 * All rights reserved.
 */

#include <bus_pci_driver.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "../nfp_logs.h"
#include "nfp_nfdk.h"
#include "nfp_nfdk_vec.h"

/*
 * One simple packet needs 2 descriptors so if send 4 packets driver will use
 * 8 descriptors at once.
 */
#define NFDK_SIMPLE_BURST_DES_NUM 8

#define NFDK_SIMPLE_DES_TYPE (NFDK_DESC_TX_EOP | \
		(NFDK_DESC_TX_TYPE_HEAD & (NFDK_DESC_TX_TYPE_SIMPLE << 12)))

static inline int
nfp_net_nfdk_vec_avx2_xmit_simple_set_des2(struct rte_mbuf *pkt,
		struct nfp_net_txq *txq,
		uint64_t *des_addr,
		uint64_t *des_meta,
		bool repr_flag)
{
	int ret;
	__m128i dma_addr;
	__m128i dma_hi;
	__m128i data_off;
	__m128i dlen_type;
	uint64_t metadata;

	if (repr_flag) {
		metadata = NFDK_DESC_TX_CHAIN_META;
	} else {
		ret = nfp_net_nfdk_set_meta_data(pkt, txq, &metadata);
		if (unlikely(ret != 0))
			return ret;
	}

	data_off = _mm_set_epi64x(0, pkt->data_off);
	dma_addr = _mm_add_epi64(_mm_loadu_si128((__m128i *)&pkt->buf_addr), data_off);
	dma_hi = _mm_srli_epi64(dma_addr, 32);

	dlen_type = _mm_set_epi64x(0, (pkt->data_len - 1) | NFDK_SIMPLE_DES_TYPE);

	*des_addr = _mm_extract_epi64(_mm_add_epi64(_mm_unpacklo_epi32(dma_hi, dma_addr),
			_mm_slli_epi64(dlen_type, 16)), 0);

	*des_meta = nfp_net_nfdk_tx_cksum(txq, pkt, metadata);

	return 0;
}

static inline int
nfp_net_nfdk_vec_avx2_xmit_simple_send1(struct nfp_net_txq *txq,
		struct nfp_net_nfdk_tx_desc *txds,
		struct rte_mbuf *pkt,
		bool repr_flag)
{
	int ret;
	__m128i des_data;
	uint64_t des_addr;
	uint64_t des_meta;

	ret = nfp_net_nfdk_vec_avx2_xmit_simple_set_des2(pkt, txq, &des_addr,
			&des_meta, repr_flag);
	if (unlikely(ret != 0))
		return ret;

	txq->wr_p = D_IDX(txq, txq->wr_p + NFDK_TX_DESC_PER_SIMPLE_PKT);
	if ((txq->wr_p & (NFDK_TX_DESC_BLOCK_CNT - 1)) != 0)
		txq->data_pending += pkt->data_len;
	else
		txq->data_pending = 0;

	des_data = _mm_set_epi64x(des_meta, des_addr);

	_mm_store_si128((void *)txds, des_data);

	return 0;
}

static inline int
nfp_vec_avx2_nfdk_xmit_simple_send4(struct nfp_net_txq *txq,
		struct nfp_net_nfdk_tx_desc *txds,
		struct rte_mbuf **pkt,
		bool repr_flag)
{
	int ret;
	uint16_t i;
	__m256i des_data0_1;
	__m256i des_data2_3;
	uint64_t des_addr[4];
	uint64_t des_meta[4];

	for (i = 0; i < 4; i++) {
		ret = nfp_net_nfdk_vec_avx2_xmit_simple_set_des2(pkt[i], txq,
				&des_addr[i], &des_meta[i], repr_flag);
		if (unlikely(ret != 0))
			return ret;
	}

	for (i = 0; i < 4; i++) {
		txq->wr_p = D_IDX(txq, txq->wr_p + NFDK_TX_DESC_PER_SIMPLE_PKT);
		if ((txq->wr_p & (NFDK_TX_DESC_BLOCK_CNT - 1)) != 0)
			txq->data_pending += pkt[i]->data_len;
		else
			txq->data_pending = 0;
	}

	des_data0_1 = _mm256_set_epi64x(des_meta[1], des_addr[1], des_meta[0], des_addr[0]);
	des_data2_3 = _mm256_set_epi64x(des_meta[3], des_addr[3], des_meta[2], des_addr[2]);

	_mm256_store_si256((void *)txds, des_data0_1);
	_mm256_store_si256((void *)(txds + 4), des_data2_3);

	return 0;
}

static inline void
nfp_net_nfdk_vec_avx2_xmit_mbuf_store4(struct rte_mbuf **mbuf,
		struct rte_mbuf **tx_pkts)
{
	__m256i mbuf_room0_1;
	__m256i mbuf_room2_3;

	mbuf_room0_1 = _mm256_set_epi64x(0, (uintptr_t)tx_pkts[1], 0,
			(uintptr_t)tx_pkts[0]);
	mbuf_room2_3 = _mm256_set_epi64x(0, (uintptr_t)tx_pkts[3], 0,
			(uintptr_t)tx_pkts[2]);

	_mm256_store_si256((void *)mbuf, mbuf_room0_1);
	_mm256_store_si256((void *)(mbuf + 4), mbuf_room2_3);
}

static inline uint16_t
nfp_net_nfdk_vec_avx2_xmit_simple_pkts(struct nfp_net_txq *txq,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts,
		uint16_t simple_close,
		bool repr_flag)
{
	int ret;
	uint16_t npkts = 0;
	uint16_t need_txds;
	uint16_t free_descs;
	struct rte_mbuf **lmbuf;
	struct nfp_net_nfdk_tx_desc *ktxds;

	PMD_TX_LOG(DEBUG, "Working for queue %hu at pos %u and %hu packets.",
			txq->qidx, txq->wr_p, nb_pkts);

	need_txds = nb_pkts << 1;
	if (nfp_net_nfdk_free_tx_desc(txq) < need_txds || nfp_net_nfdk_txq_full(txq))
		nfp_net_tx_free_bufs(txq);

	free_descs = nfp_net_nfdk_free_tx_desc(txq);
	if (unlikely(free_descs < NFDK_TX_DESC_PER_SIMPLE_PKT)) {
		if (unlikely(simple_close > 0))
			goto xmit_end;

		return 0;
	}

	PMD_TX_LOG(DEBUG, "Queue: %hu. Sending %hu packets.", txq->qidx, nb_pkts);

	/* Sending packets */
	while (npkts < nb_pkts && free_descs >= NFDK_TX_DESC_PER_SIMPLE_PKT) {
		ktxds = &txq->ktxds[txq->wr_p];
		lmbuf = &txq->txbufs[txq->wr_p].mbuf;

		/*
		 * If can not send burst, just send one.
		 * 1. Tx ring will come to the tail.
		 * 2. Do not need to send 4 packets.
		 * 3. If pointer address unaligned on 32-bit boundary.
		 * 4. If free descriptors are not enough.
		 */
		if ((txq->tx_count - txq->wr_p) < NFDK_SIMPLE_BURST_DES_NUM ||
				(nb_pkts - npkts) < 4 ||
				((uintptr_t)ktxds & 0x1F) != 0 ||
				free_descs < NFDK_SIMPLE_BURST_DES_NUM) {
			ret = nfp_net_nfdk_vec_avx2_xmit_simple_send1(txq,
					ktxds, tx_pkts[npkts], repr_flag);
			if (unlikely(ret != 0))
				goto xmit_end;

			rte_pktmbuf_free(*lmbuf);

			_mm_storel_epi64((void *)lmbuf,
					_mm_loadu_si128((void *)&tx_pkts[npkts]));
			npkts++;
			free_descs -= NFDK_TX_DESC_PER_SIMPLE_PKT;
			continue;
		}

		ret = nfp_vec_avx2_nfdk_xmit_simple_send4(txq, ktxds,
				&tx_pkts[npkts], repr_flag);
		if (unlikely(ret != 0))
			goto xmit_end;

		rte_pktmbuf_free_bulk(lmbuf, NFDK_SIMPLE_BURST_DES_NUM);

		nfp_net_nfdk_vec_avx2_xmit_mbuf_store4(lmbuf, &tx_pkts[npkts]);

		npkts += 4;
		free_descs -= NFDK_SIMPLE_BURST_DES_NUM;
	}

xmit_end:
	/* Increment write pointers. Force memory write before we let HW know */
	rte_wmb();
	nfp_qcp_ptr_add(txq->qcp_q, NFP_QCP_WRITE_PTR, ((npkts << 1) + simple_close));

	return npkts;
}

static inline void
nfp_net_nfdk_vec_avx2_xmit_simple_close_block(struct nfp_net_txq *txq,
		uint16_t *simple_close)
{
	uint16_t i;
	uint16_t wr_p;
	uint16_t nop_slots;
	__m128i zero_128 = _mm_setzero_si128();
	__m256i zero_256 = _mm256_setzero_si256();

	wr_p = txq->wr_p;
	nop_slots = D_BLOCK_CPL(wr_p);

	for (i = nop_slots; i >= 4; i -= 4, wr_p += 4) {
		_mm256_store_si256((void *)&txq->ktxds[wr_p], zero_256);
		rte_pktmbuf_free_bulk(&txq->txbufs[wr_p].mbuf, 4);
		_mm256_store_si256((void *)&txq->txbufs[wr_p], zero_256);
	}

	for (; i >= 2; i -= 2, wr_p += 2) {
		_mm_store_si128((void *)&txq->ktxds[wr_p], zero_128);
		rte_pktmbuf_free_bulk(&txq->txbufs[wr_p].mbuf, 2);
		_mm_store_si128((void *)&txq->txbufs[wr_p], zero_128);
	}

	for (; i >= 1; i--, wr_p++) {
		_mm_storel_epi64((void *)&txq->ktxds[wr_p], zero_128);
		rte_pktmbuf_free(txq->txbufs[wr_p].mbuf);
		_mm_storel_epi64((void *)&txq->txbufs[wr_p], zero_128);
	}

	txq->data_pending = 0;
	txq->wr_p = D_IDX(txq, txq->wr_p + nop_slots);

	(*simple_close) += nop_slots;
}

static inline uint32_t
nfp_net_nfdk_vec_avx2_xmit_simple_prepare(struct nfp_net_txq *txq,
		uint16_t *simple_close)
{
	uint16_t wr_p;
	__m128i zero_128 = _mm_setzero_si128();

	wr_p = txq->wr_p;

	_mm_storel_epi64((void *)&txq->ktxds[wr_p], zero_128);
	rte_pktmbuf_free(txq->txbufs[wr_p].mbuf);
	_mm_storel_epi64((void *)&txq->txbufs[wr_p], zero_128);

	txq->wr_p = D_IDX(txq, wr_p + 1);
	(*simple_close)++;

	return txq->wr_p;
}

static inline void
nfp_net_nfdk_vec_avx2_xmit_simple_check(struct nfp_net_txq *txq,
		struct rte_mbuf *pkt,
		bool *simple_flag,
		bool *pending_flag,
		uint16_t *data_pending,
		uint32_t *wr_p,
		uint16_t *simple_close)
{
	uint32_t data_pending_temp;

	/* Let the first descriptor index even before send simple packets */
	if (!(*simple_flag)) {
		if ((*wr_p & 0x1) == 0x1)
			*wr_p = nfp_net_nfdk_vec_avx2_xmit_simple_prepare(txq, simple_close);

		*simple_flag = true;
	}

	/* Simple packets only need one close block operation */
	if (!(*pending_flag)) {
		if ((*wr_p & (NFDK_TX_DESC_BLOCK_CNT - 1)) == 0) {
			*pending_flag = true;
			return;
		}

		data_pending_temp = *data_pending + pkt->data_len;
		if (data_pending_temp > NFDK_TX_MAX_DATA_PER_BLOCK) {
			nfp_net_nfdk_vec_avx2_xmit_simple_close_block(txq, simple_close);
			*pending_flag = true;
			return;
		}

		*data_pending = data_pending_temp;

		*wr_p += 2;
	}
}

static inline uint16_t
nfp_net_nfdk_vec_avx2_xmit_simple_count(struct nfp_net_txq *txq,
		struct rte_mbuf **tx_pkts,
		uint16_t head,
		uint16_t nb_pkts,
		uint16_t *simple_close)
{
	uint32_t wr_p;
	uint16_t simple_idx;
	struct rte_mbuf *pkt;
	uint16_t data_pending;
	bool simple_flag = false;
	bool pending_flag = false;
	uint16_t simple_count = 0;

	*simple_close = 0;
	wr_p = txq->wr_p;
	data_pending = txq->data_pending;

	for (simple_idx = head; simple_idx < nb_pkts; simple_idx++) {
		pkt = tx_pkts[simple_idx];
		if (!nfp_net_nfdk_is_simple_packet(pkt, txq->hw))
			break;

		simple_count++;
		if (!txq->simple_always)
			nfp_net_nfdk_vec_avx2_xmit_simple_check(txq, pkt, &simple_flag,
					&pending_flag, &data_pending, &wr_p, simple_close);
	}

	return simple_count;
}

static inline uint16_t
nfp_net_nfdk_vec_avx2_xmit_others_count(struct nfp_net_txq *txq,
		struct rte_mbuf **tx_pkts,
		uint16_t head,
		uint16_t nb_pkts)
{
	uint16_t others_idx;
	struct rte_mbuf *pkt;
	uint16_t others_count = 0;

	for (others_idx = head; others_idx < nb_pkts; others_idx++) {
		pkt = tx_pkts[others_idx];
		if (nfp_net_nfdk_is_simple_packet(pkt, txq->hw))
			break;

		others_count++;
	}

	return others_count;
}

static inline uint16_t
nfp_net_nfdk_vec_avx2_xmit_common(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	uint16_t i;
	uint16_t avail = 0;
	uint16_t simple_close;
	uint16_t simple_count;
	uint16_t simple_avail;
	uint16_t others_count;
	uint16_t others_avail;
	struct nfp_net_txq *txq = tx_queue;

	for (i = 0; i < nb_pkts; i++) {
		simple_count = nfp_net_nfdk_vec_avx2_xmit_simple_count(txq, tx_pkts, i,
				nb_pkts, &simple_close);
		if (simple_count > 0) {
			if (!txq->simple_always)
				txq->simple_always = true;

			simple_avail = nfp_net_nfdk_vec_avx2_xmit_simple_pkts(txq,
					tx_pkts + i, simple_count, simple_close,
					false);

			avail += simple_avail;
			if (simple_avail != simple_count)
				break;

			i += simple_count;
		}

		if (i == nb_pkts)
			break;

		others_count = nfp_net_nfdk_vec_avx2_xmit_others_count(txq, tx_pkts,
				i, nb_pkts);

		if (txq->simple_always)
			txq->simple_always = false;

		others_avail = nfp_net_nfdk_xmit_pkts_common(tx_queue,
				tx_pkts + i, others_count, false);

		avail += others_avail;
		if (others_avail != others_count)
			break;

		i += others_count;
	}

	return avail;
}

uint16_t
nfp_net_nfdk_vec_avx2_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts)
{
	return nfp_net_nfdk_vec_avx2_xmit_common(tx_queue, tx_pkts, nb_pkts);
}
