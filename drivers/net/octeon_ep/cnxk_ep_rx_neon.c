/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include "cnxk_ep_rx.h"

static __rte_always_inline void
cnxk_ep_process_pkts_vec_neon(struct rte_mbuf **rx_pkts, struct otx_ep_droq *droq,
			      uint16_t new_pkts)
{
	const uint8x16_t mask0 = {0, 1, 0xff, 0xff, 0, 1, 0xff, 0xff,
				  4, 5, 0xff, 0xff, 4, 5, 0xff, 0xff};
	const uint8x16_t mask1 = {8,  9,  0xff, 0xff, 8,  9,  0xff, 0xff,
				  12, 13, 0xff, 0xff, 12, 13, 0xff, 0xff};
	struct rte_mbuf **recv_buf_list = droq->recv_buf_list;
	uint32_t pidx0, pidx1, pidx2, pidx3;
	struct rte_mbuf *m0, *m1, *m2, *m3;
	uint32_t read_idx = droq->read_idx;
	uint16_t nb_desc = droq->nb_desc;
	uint32_t idx0, idx1, idx2, idx3;
	uint64x2_t s01, s23;
	uint32x4_t bytes;
	uint16_t pkts = 0;

	idx0 = read_idx;
	s01 = vdupq_n_u64(0);
	bytes = vdupq_n_u32(0);
	while (pkts < new_pkts) {
		idx1 = otx_ep_incr_index(idx0, 1, nb_desc);
		idx2 = otx_ep_incr_index(idx1, 1, nb_desc);
		idx3 = otx_ep_incr_index(idx2, 1, nb_desc);

		if (new_pkts - pkts > 4) {
			pidx0 = otx_ep_incr_index(idx3, 1, nb_desc);
			pidx1 = otx_ep_incr_index(pidx0, 1, nb_desc);
			pidx2 = otx_ep_incr_index(pidx1, 1, nb_desc);
			pidx3 = otx_ep_incr_index(pidx2, 1, nb_desc);

			rte_prefetch_non_temporal(cnxk_pktmbuf_mtod(recv_buf_list[pidx0], void *));
			rte_prefetch_non_temporal(cnxk_pktmbuf_mtod(recv_buf_list[pidx1], void *));
			rte_prefetch_non_temporal(cnxk_pktmbuf_mtod(recv_buf_list[pidx2], void *));
			rte_prefetch_non_temporal(cnxk_pktmbuf_mtod(recv_buf_list[pidx3], void *));
		}

		m0 = recv_buf_list[idx0];
		m1 = recv_buf_list[idx1];
		m2 = recv_buf_list[idx2];
		m3 = recv_buf_list[idx3];

		/* Load packet size big-endian. */
		s01 = vsetq_lane_u32(cnxk_pktmbuf_mtod(m0, struct otx_ep_droq_info *)->length >> 48,
				     s01, 0);
		s01 = vsetq_lane_u32(cnxk_pktmbuf_mtod(m1, struct otx_ep_droq_info *)->length >> 48,
				     s01, 1);
		s01 = vsetq_lane_u32(cnxk_pktmbuf_mtod(m2, struct otx_ep_droq_info *)->length >> 48,
				     s01, 2);
		s01 = vsetq_lane_u32(cnxk_pktmbuf_mtod(m3, struct otx_ep_droq_info *)->length >> 48,
				     s01, 3);
		/* Convert to little-endian. */
		s01 = vrev16q_u8(s01);

		/* Vertical add, consolidate outside the loop. */
		bytes += vaddq_u32(bytes, s01);
		/* Separate into packet length and data length. */
		s23 = vqtbl1q_u8(s01, mask1);
		s01 = vqtbl1q_u8(s01, mask0);

		/* Store packet length and data length to mbuf. */
		*(uint64_t *)&m0->pkt_len = vgetq_lane_u64(s01, 0);
		*(uint64_t *)&m1->pkt_len = vgetq_lane_u64(s01, 1);
		*(uint64_t *)&m2->pkt_len = vgetq_lane_u64(s23, 0);
		*(uint64_t *)&m3->pkt_len = vgetq_lane_u64(s23, 1);

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
#if defined(RTE_ARCH_32)
	droq->stats.bytes_received += vgetq_lane_u32(bytes, 0);
	droq->stats.bytes_received += vgetq_lane_u32(bytes, 1);
	droq->stats.bytes_received += vgetq_lane_u32(bytes, 2);
	droq->stats.bytes_received += vgetq_lane_u32(bytes, 3);
#else
	droq->stats.bytes_received += vaddvq_u32(bytes);
#endif
}

uint16_t __rte_noinline __rte_hot
cnxk_ep_recv_pkts_neon(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct otx_ep_droq *droq = (struct otx_ep_droq *)rx_queue;
	uint16_t new_pkts, vpkts;

	/* Refill RX buffers */
	if (droq->refill_count >= DROQ_REFILL_THRESHOLD)
		cnxk_ep_rx_refill(droq);

	new_pkts = cnxk_ep_rx_pkts_to_process(droq, nb_pkts);
	vpkts = RTE_ALIGN_FLOOR(new_pkts, CNXK_EP_OQ_DESC_PER_LOOP_SSE);
	cnxk_ep_process_pkts_vec_neon(rx_pkts, droq, vpkts);
	cnxk_ep_process_pkts_scalar(&rx_pkts[vpkts], droq, new_pkts - vpkts);

	return new_pkts;
}

uint16_t __rte_noinline __rte_hot
cn9k_ep_recv_pkts_neon(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
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
	cnxk_ep_process_pkts_vec_neon(rx_pkts, droq, vpkts);
	cnxk_ep_process_pkts_scalar(&rx_pkts[vpkts], droq, new_pkts - vpkts);

	return new_pkts;
}
