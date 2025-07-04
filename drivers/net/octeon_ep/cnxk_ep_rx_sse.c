/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include "cnxk_ep_rx.h"

static __rte_always_inline uint32_t
hadd(__m128i x)
{
	__m128i hi64 = _mm_shuffle_epi32(x, _MM_SHUFFLE(1, 0, 3, 2));
	__m128i sum64 = _mm_add_epi32(hi64, x);
	__m128i hi32 = _mm_shufflelo_epi16(sum64, _MM_SHUFFLE(1, 0, 3, 2));
	__m128i sum32 = _mm_add_epi32(sum64, hi32);
	return _mm_cvtsi128_si32(sum32);
}

static __rte_always_inline void
cnxk_ep_process_pkts_vec_sse(struct rte_mbuf **rx_pkts, struct otx_ep_droq *droq, uint16_t new_pkts)
{
	struct rte_mbuf **recv_buf_list = droq->recv_buf_list;
	uint32_t read_idx = droq->read_idx;
	struct rte_mbuf *m0, *m1, *m2, *m3;
	uint16_t nb_desc = droq->nb_desc;
	uint32_t idx0, idx1, idx2, idx3;
	uint16_t pkts = 0;
	__m128i bytes;

	idx0 = read_idx;
	bytes = _mm_setzero_si128();
	while (pkts < new_pkts) {
		const __m128i bswap_mask = _mm_set_epi8(0xFF, 0xFF, 12, 13, 0xFF, 0xFF, 8, 9, 0xFF,
							0xFF, 4, 5, 0xFF, 0xFF, 0, 1);
		const __m128i cpy_mask = _mm_set_epi8(0xFF, 0xFF, 9, 8, 0xFF, 0xFF, 9, 8, 0xFF,
						      0xFF, 1, 0, 0xFF, 0xFF, 1, 0);
		__m128i s01, s23;

		idx1 = otx_ep_incr_index(idx0, 1, nb_desc);
		idx2 = otx_ep_incr_index(idx1, 1, nb_desc);
		idx3 = otx_ep_incr_index(idx2, 1, nb_desc);

		m0 = recv_buf_list[idx0];
		m1 = recv_buf_list[idx1];
		m2 = recv_buf_list[idx2];
		m3 = recv_buf_list[idx3];

		/* Load packet size big-endian. */
		s01 = _mm_set_epi32(cnxk_pktmbuf_mtod(m3, struct otx_ep_droq_info *)->length >> 48,
				    cnxk_pktmbuf_mtod(m1, struct otx_ep_droq_info *)->length >> 48,
				    cnxk_pktmbuf_mtod(m2, struct otx_ep_droq_info *)->length >> 48,
				    cnxk_pktmbuf_mtod(m0, struct otx_ep_droq_info *)->length >> 48);
		/* Convert to little-endian. */
		s01 = _mm_shuffle_epi8(s01, bswap_mask);
		/* Vertical add, consolidate outside loop */
		bytes = _mm_add_epi32(bytes, s01);
		/* Separate into packet length and data length. */
		s23 = _mm_shuffle_epi32(s01, _MM_SHUFFLE(3, 3, 1, 1));
		s01 = _mm_shuffle_epi8(s01, cpy_mask);
		s23 = _mm_shuffle_epi8(s23, cpy_mask);

		/* Store packet length and data length to mbuf. */
#ifdef RTE_ARCH_64
		*(uint64_t *)&m0->pkt_len = _mm_extract_epi64(s01, 0);
		*(uint64_t *)&m1->pkt_len = _mm_extract_epi64(s01, 1);
		*(uint64_t *)&m2->pkt_len = _mm_extract_epi64(s23, 0);
		*(uint64_t *)&m3->pkt_len = _mm_extract_epi64(s23, 1);
#else
		/* _mm_extract_epi64 is only available on 64-bit architecture.
		 * The cast below is non-portable and results in compile error
		 * using MSVC.
		 */
		*(uint64_t *)&m0->pkt_len = ((rte_xmm_t)s01).u64[0];
		*(uint64_t *)&m1->pkt_len = ((rte_xmm_t)s01).u64[1];
		*(uint64_t *)&m2->pkt_len = ((rte_xmm_t)s23).u64[0];
		*(uint64_t *)&m3->pkt_len = ((rte_xmm_t)s23).u64[1];
#endif

		/* Reset rearm data. */
		*(uint64_t *)&m0->rearm_data = droq->rearm_data;
		*(uint64_t *)&m1->rearm_data = droq->rearm_data;
		*(uint64_t *)&m2->rearm_data = droq->rearm_data;
		*(uint64_t *)&m3->rearm_data = droq->rearm_data;

		rx_pkts[pkts++] = m0;
		rx_pkts[pkts++] = m1;
		rx_pkts[pkts++] = m2;
		rx_pkts[pkts++] = m3;
		idx0 = otx_ep_incr_index(idx3, 1, nb_desc);
	}
	droq->read_idx = idx0;

	droq->refill_count += new_pkts;
	droq->pkts_pending -= new_pkts;
	/* Stats */
	droq->stats.pkts_received += new_pkts;
	droq->stats.bytes_received += hadd(bytes);
}

uint16_t __rte_noinline __rte_hot
cnxk_ep_recv_pkts_sse(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct otx_ep_droq *droq = (struct otx_ep_droq *)rx_queue;
	uint16_t new_pkts, vpkts;

	/* Refill RX buffers */
	if (droq->refill_count >= DROQ_REFILL_THRESHOLD)
		cnxk_ep_rx_refill(droq);

	new_pkts = cnxk_ep_rx_pkts_to_process(droq, nb_pkts);
	vpkts = RTE_ALIGN_FLOOR(new_pkts, CNXK_EP_OQ_DESC_PER_LOOP_SSE);
	cnxk_ep_process_pkts_vec_sse(rx_pkts, droq, vpkts);
	cnxk_ep_process_pkts_scalar(&rx_pkts[vpkts], droq, new_pkts - vpkts);

	return new_pkts;
}

uint16_t __rte_noinline __rte_hot
cn9k_ep_recv_pkts_sse(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct otx_ep_droq *droq = (struct otx_ep_droq *)rx_queue;
	uint16_t new_pkts, vpkts;

	/* Refill RX buffers */
	if (droq->refill_count >= DROQ_REFILL_THRESHOLD) {
		cnxk_ep_rx_refill(droq);
	} else {
		/* SDP output goes into DROP state when output doorbell count
		 * goes below drop count. When door bell count is written with
		 * a value greater than drop count SDP output should come out
		 * of DROP state. Due to a race condition this is not happening.
		 * Writing doorbell register with 0 again may make SDP output
		 * come out of this state.
		 */

		rte_write32(0, droq->pkts_credit_reg);
	}

	new_pkts = cnxk_ep_rx_pkts_to_process(droq, nb_pkts);
	vpkts = RTE_ALIGN_FLOOR(new_pkts, CNXK_EP_OQ_DESC_PER_LOOP_SSE);
	cnxk_ep_process_pkts_vec_sse(rx_pkts, droq, vpkts);
	cnxk_ep_process_pkts_scalar(&rx_pkts[vpkts], droq, new_pkts - vpkts);

	return new_pkts;
}
