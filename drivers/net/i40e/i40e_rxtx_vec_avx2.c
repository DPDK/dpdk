/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation.
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

#include <stdint.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>

#include "base/i40e_prototype.h"
#include "base/i40e_type.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"
#include "i40e_rxtx_vec_common.h"

#include <x86intrin.h>

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

static inline void
vtx1(volatile struct i40e_tx_desc *txdp,
		struct rte_mbuf *pkt, uint64_t flags)
{
	uint64_t high_qw = (I40E_TX_DESC_DTYPE_DATA |
			((uint64_t)flags  << I40E_TXD_QW1_CMD_SHIFT) |
			((uint64_t)pkt->data_len << I40E_TXD_QW1_TX_BUF_SZ_SHIFT));

	__m128i descriptor = _mm_set_epi64x(high_qw,
				pkt->buf_physaddr + pkt->data_off);
	_mm_store_si128((__m128i *)txdp, descriptor);
}

static inline void
vtx(volatile struct i40e_tx_desc *txdp,
		struct rte_mbuf **pkt, uint16_t nb_pkts,  uint64_t flags)
{
	const uint64_t hi_qw_tmpl = (I40E_TX_DESC_DTYPE_DATA |
			((uint64_t)flags  << I40E_TXD_QW1_CMD_SHIFT));

	/* if unaligned on 32-bit boundary, do one to align */
	if (((uintptr_t)txdp & 0x1F) != 0 && nb_pkts != 0) {
		vtx1(txdp, *pkt, flags);
		nb_pkts--, txdp++, pkt++;
	}

	/* do two at a time while possible, in bursts */
	for (; nb_pkts > 3; txdp += 4, pkt += 4, nb_pkts -= 4) {
		uint64_t hi_qw3 = hi_qw_tmpl |
				((uint64_t)pkt[3]->data_len << I40E_TXD_QW1_TX_BUF_SZ_SHIFT);
		uint64_t hi_qw2 = hi_qw_tmpl |
				((uint64_t)pkt[2]->data_len << I40E_TXD_QW1_TX_BUF_SZ_SHIFT);
		uint64_t hi_qw1 = hi_qw_tmpl |
				((uint64_t)pkt[1]->data_len << I40E_TXD_QW1_TX_BUF_SZ_SHIFT);
		uint64_t hi_qw0 = hi_qw_tmpl |
				((uint64_t)pkt[0]->data_len << I40E_TXD_QW1_TX_BUF_SZ_SHIFT);

		__m256i desc2_3 = _mm256_set_epi64x(
				hi_qw3, pkt[3]->buf_physaddr + pkt[3]->data_off,
				hi_qw2, pkt[2]->buf_physaddr + pkt[2]->data_off);
		__m256i desc0_1 = _mm256_set_epi64x(
				hi_qw1, pkt[1]->buf_physaddr + pkt[1]->data_off,
				hi_qw0, pkt[0]->buf_physaddr + pkt[0]->data_off);
		_mm256_store_si256((void *)(txdp + 2), desc2_3);
		_mm256_store_si256((void *)txdp, desc0_1);
	}

	/* do any last ones */
	while (nb_pkts) {
		vtx1(txdp, *pkt, flags);
		txdp++, pkt++, nb_pkts--;
	}
}

static inline uint16_t
i40e_xmit_fixed_burst_vec_avx2(void *tx_queue, struct rte_mbuf **tx_pkts,
			  uint16_t nb_pkts)
{
	struct i40e_tx_queue *txq = (struct i40e_tx_queue *)tx_queue;
	volatile struct i40e_tx_desc *txdp;
	struct i40e_tx_entry *txep;
	uint16_t n, nb_commit, tx_id;
	uint64_t flags = I40E_TD_CMD;
	uint64_t rs = I40E_TX_DESC_CMD_RS | I40E_TD_CMD;

	/* cross rx_thresh boundary is not allowed */
	nb_pkts = RTE_MIN(nb_pkts, txq->tx_rs_thresh);

	if (txq->nb_tx_free < txq->tx_free_thresh)
		i40e_tx_free_bufs(txq);

	nb_commit = nb_pkts = (uint16_t)RTE_MIN(txq->nb_tx_free, nb_pkts);
	if (unlikely(nb_pkts == 0))
		return 0;

	tx_id = txq->tx_tail;
	txdp = &txq->tx_ring[tx_id];
	txep = &txq->sw_ring[tx_id];

	txq->nb_tx_free = (uint16_t)(txq->nb_tx_free - nb_pkts);

	n = (uint16_t)(txq->nb_tx_desc - tx_id);
	if (nb_commit >= n) {
		tx_backlog_entry(txep, tx_pkts, n);

		vtx(txdp, tx_pkts, n - 1, flags);
		tx_pkts += (n - 1);
		txdp += (n - 1);

		vtx1(txdp, *tx_pkts++, rs);

		nb_commit = (uint16_t)(nb_commit - n);

		tx_id = 0;
		txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

		/* avoid reach the end of ring */
		txdp = &txq->tx_ring[tx_id];
		txep = &txq->sw_ring[tx_id];
	}

	tx_backlog_entry(txep, tx_pkts, nb_commit);

	vtx(txdp, tx_pkts, nb_commit, flags);

	tx_id = (uint16_t)(tx_id + nb_commit);
	if (tx_id > txq->tx_next_rs) {
		txq->tx_ring[txq->tx_next_rs].cmd_type_offset_bsz |=
			rte_cpu_to_le_64(((uint64_t)I40E_TX_DESC_CMD_RS) <<
						I40E_TXD_QW1_CMD_SHIFT);
		txq->tx_next_rs =
			(uint16_t)(txq->tx_next_rs + txq->tx_rs_thresh);
	}

	txq->tx_tail = tx_id;

	I40E_PCI_REG_WRITE(txq->qtx_tail, txq->tx_tail);

	return nb_pkts;
}

uint16_t
i40e_xmit_pkts_vec_avx2(void *tx_queue, struct rte_mbuf **tx_pkts,
		   uint16_t nb_pkts)
{
	uint16_t nb_tx = 0;
	struct i40e_tx_queue *txq = (struct i40e_tx_queue *)tx_queue;

	while (nb_pkts) {
		uint16_t ret, num;

		num = (uint16_t)RTE_MIN(nb_pkts, txq->tx_rs_thresh);
		ret = i40e_xmit_fixed_burst_vec_avx2(tx_queue, &tx_pkts[nb_tx],
						num);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}
