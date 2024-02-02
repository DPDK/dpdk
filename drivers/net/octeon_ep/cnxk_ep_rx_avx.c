/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include "cnxk_ep_rx.h"

static __rte_always_inline void
cnxk_ep_process_pkts_vec_avx(struct rte_mbuf **rx_pkts, struct otx_ep_droq *droq, uint16_t new_pkts)
{
	struct rte_mbuf **recv_buf_list = droq->recv_buf_list;
	uint32_t bytes_rsvd = 0, read_idx = droq->read_idx;
	const uint64_t rearm_data = droq->rearm_data;
	struct rte_mbuf *m[CNXK_EP_OQ_DESC_PER_LOOP_AVX];
	uint32_t pidx[CNXK_EP_OQ_DESC_PER_LOOP_AVX];
	uint32_t idx[CNXK_EP_OQ_DESC_PER_LOOP_AVX];
	uint16_t nb_desc = droq->nb_desc;
	uint16_t pkts = 0;
	uint8_t i;

	idx[0] = read_idx;
	while (pkts < new_pkts) {
		__m256i data[CNXK_EP_OQ_DESC_PER_LOOP_AVX];
		/* mask to shuffle from desc. to mbuf (2 descriptors)*/
		const __m256i mask =
			_mm256_set_epi8(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 20, 21, 0xFF, 0xFF, 20,
					21, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					0xFF, 0xFF, 0xFF, 7, 6, 5, 4, 3, 2, 1, 0);

		/* Load indexes. */
		for (i = 1; i < CNXK_EP_OQ_DESC_PER_LOOP_AVX; i++)
			idx[i] = otx_ep_incr_index(idx[i - 1], 1, nb_desc);

		/* Prefetch next indexes. */
		if (new_pkts - pkts > 8) {
			pidx[0] = otx_ep_incr_index(idx[i - 1], 1, nb_desc);
			for (i = 1; i < CNXK_EP_OQ_DESC_PER_LOOP_AVX; i++)
				pidx[i] = otx_ep_incr_index(pidx[i - 1], 1, nb_desc);

			for (i = 0; i < CNXK_EP_OQ_DESC_PER_LOOP_AVX; i++) {
				rte_prefetch0(recv_buf_list[pidx[i]]);
				rte_prefetch0(rte_pktmbuf_mtod(recv_buf_list[pidx[i]], void *));
			}
		}

		/* Load mbuf array. */
		for (i = 0; i < CNXK_EP_OQ_DESC_PER_LOOP_AVX; i++)
			m[i] = recv_buf_list[idx[i]];

		/* Load rearm data and packet length for shuffle. */
		for (i = 0; i < CNXK_EP_OQ_DESC_PER_LOOP_AVX; i++)
			data[i] = _mm256_set_epi64x(0,
				cnxk_pktmbuf_mtod(m[i], struct otx_ep_droq_info *)->length >> 16,
				0, rearm_data);

		/* Shuffle data to its place and sum the packet length. */
		for (i = 0; i < CNXK_EP_OQ_DESC_PER_LOOP_AVX; i++) {
			data[i] = _mm256_shuffle_epi8(data[i], mask);
			bytes_rsvd += _mm256_extract_epi16(data[i], 10);
		}

		/* Store the 256bit data to the mbuf. */
		for (i = 0; i < CNXK_EP_OQ_DESC_PER_LOOP_AVX; i++)
			_mm256_storeu_si256((__m256i *)&m[i]->rearm_data, data[i]);

		for (i = 0; i < CNXK_EP_OQ_DESC_PER_LOOP_AVX; i++)
			rx_pkts[pkts++] = m[i];
		idx[0] = otx_ep_incr_index(idx[i - 1], 1, nb_desc);
	}
	droq->read_idx = idx[0];

	droq->refill_count += new_pkts;
	droq->pkts_pending -= new_pkts;
	/* Stats */
	droq->stats.pkts_received += new_pkts;
	droq->stats.bytes_received += bytes_rsvd;
}

uint16_t __rte_noinline __rte_hot
cnxk_ep_recv_pkts_avx(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct otx_ep_droq *droq = (struct otx_ep_droq *)rx_queue;
	uint16_t new_pkts, vpkts;

	/* Refill RX buffers */
	if (droq->refill_count >= DROQ_REFILL_THRESHOLD)
		cnxk_ep_rx_refill(droq);

	new_pkts = cnxk_ep_rx_pkts_to_process(droq, nb_pkts);
	vpkts = RTE_ALIGN_FLOOR(new_pkts, CNXK_EP_OQ_DESC_PER_LOOP_AVX);
	cnxk_ep_process_pkts_vec_avx(rx_pkts, droq, vpkts);
	cnxk_ep_process_pkts_scalar(&rx_pkts[vpkts], droq, new_pkts - vpkts);

	return new_pkts;
}

uint16_t __rte_noinline __rte_hot
cn9k_ep_recv_pkts_avx(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
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
	vpkts = RTE_ALIGN_FLOOR(new_pkts, CNXK_EP_OQ_DESC_PER_LOOP_AVX);
	cnxk_ep_process_pkts_vec_avx(rx_pkts, droq, vpkts);
	cnxk_ep_process_pkts_scalar(&rx_pkts[vpkts], droq, new_pkts - vpkts);

	return new_pkts;
}
