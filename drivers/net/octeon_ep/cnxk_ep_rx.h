/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_vect.h>

#include "otx_ep_common.h"
#include "otx2_ep_vf.h"
#include "otx_ep_rxtx.h"

#define CNXK_EP_OQ_DESC_PER_LOOP_SSE 4
#define CNXK_EP_OQ_DESC_PER_LOOP_AVX 8

static inline int
cnxk_ep_rx_refill_mbuf(struct otx_ep_droq *droq, uint32_t count)
{
	struct otx_ep_droq_desc *desc_ring = droq->desc_ring;
	struct rte_mbuf **recv_buf_list = droq->recv_buf_list;
	uint32_t refill_idx = droq->refill_idx;
	struct rte_mbuf *buf;
	uint32_t i;
	int rc;

	rc = rte_mempool_get_bulk(droq->mpool, (void **)&recv_buf_list[refill_idx], count);
	if (unlikely(rc)) {
		droq->stats.rx_alloc_failure++;
		return rc;
	}

	for (i = 0; i < count; i++) {
		rte_prefetch_non_temporal(&desc_ring[(refill_idx + 1) & 3]);
		if (i < count - 1)
			rte_prefetch_non_temporal(recv_buf_list[refill_idx + 1]);
		buf = recv_buf_list[refill_idx];
		desc_ring[refill_idx].buffer_ptr = rte_mbuf_data_iova_default(buf);
		refill_idx++;
	}

	droq->refill_idx = otx_ep_incr_index(droq->refill_idx, count, droq->nb_desc);
	droq->refill_count -= count;

	return 0;
}

static inline void
cnxk_ep_rx_refill(struct otx_ep_droq *droq)
{
	const uint32_t nb_desc = droq->nb_desc;
	uint32_t refill_idx = droq->refill_idx;
	uint32_t desc_refilled = 0, count;
	int rc;

	if (unlikely(droq->read_idx == refill_idx))
		return;

	if (refill_idx < droq->read_idx) {
		count = droq->read_idx - refill_idx;
		rc = cnxk_ep_rx_refill_mbuf(droq, count);
		if (unlikely(rc)) {
			droq->stats.rx_alloc_failure++;
			return;
		}
		desc_refilled = count;
	} else {
		count = nb_desc - refill_idx;
		rc = cnxk_ep_rx_refill_mbuf(droq, count);
		if (unlikely(rc)) {
			droq->stats.rx_alloc_failure++;
			return;
		}

		desc_refilled = count;
		count = droq->read_idx;
		rc = cnxk_ep_rx_refill_mbuf(droq, count);
		if (unlikely(rc))
			droq->stats.rx_alloc_failure++;
		else
			desc_refilled += count;
	}

	/* Flush the droq descriptor data to memory to be sure
	 * that when we update the credits the data in memory is
	 * accurate.
	 */
	rte_io_wmb();
	rte_write32(desc_refilled, droq->pkts_credit_reg);
}

static inline uint32_t
cnxk_ep_check_rx_ism_mem(void *rx_queue)
{
	struct otx_ep_droq *droq = (struct otx_ep_droq *)rx_queue;
	uint32_t new_pkts;
	uint32_t val;

	/* Batch subtractions from the HW counter to reduce PCIe traffic
	 * This adds an extra local variable, but almost halves the
	 * number of PCIe writes.
	 */
	val = rte_atomic_load_explicit(droq->pkts_sent_ism, rte_memory_order_relaxed);

	new_pkts = val - droq->pkts_sent_prev;
	droq->pkts_sent_prev = val;

	if (val > RTE_BIT32(31)) {
		/* Only subtract the packet count in the HW counter
		 * when count above halfway to saturation.
		 */
		rte_write64((uint64_t)val, droq->pkts_sent_reg);
		rte_mb();

		rte_write64(OTX2_SDP_REQUEST_ISM, droq->pkts_sent_reg);
		while (rte_atomic_load_explicit(droq->pkts_sent_ism,
				rte_memory_order_relaxed) >= val) {
			rte_write64(OTX2_SDP_REQUEST_ISM, droq->pkts_sent_reg);
			rte_mb();
		}
		droq->pkts_sent_prev = 0;
	}

	rte_write64(OTX2_SDP_REQUEST_ISM, droq->pkts_sent_reg);

	return new_pkts;
}

static inline uint32_t
cnxk_ep_check_rx_pkt_reg(void *rx_queue)
{
	struct otx_ep_droq *droq = (struct otx_ep_droq *)rx_queue;
	uint32_t new_pkts;
	uint32_t val;

	val = rte_read32(droq->pkts_sent_reg);

	new_pkts = val - droq->pkts_sent_prev;
	droq->pkts_sent_prev = val;

	if (val > RTE_BIT32(31)) {
		/* Only subtract the packet count in the HW counter
		 * when count above halfway to saturation.
		 */
		rte_write64((uint64_t)val, droq->pkts_sent_reg);
		rte_mb();
		droq->pkts_sent_prev = 0;
	}

	return new_pkts;
}

static inline int16_t __rte_hot
cnxk_ep_rx_pkts_to_process(struct otx_ep_droq *droq, uint16_t nb_pkts)
{
	const otx_ep_check_pkt_count_t cnxk_rx_pkt_count[2] = { cnxk_ep_check_rx_pkt_reg,
								cnxk_ep_check_rx_ism_mem};

	if (droq->pkts_pending < nb_pkts)
		droq->pkts_pending += cnxk_rx_pkt_count[droq->ism_ena](droq);

	return RTE_MIN(nb_pkts, droq->pkts_pending);
}

#define cnxk_pktmbuf_mtod(m, t) ((t)(void *)((char *)(m)->buf_addr + RTE_PKTMBUF_HEADROOM))

static __rte_always_inline void
cnxk_ep_process_pkts_scalar(struct rte_mbuf **rx_pkts, struct otx_ep_droq *droq, uint16_t new_pkts)
{
	struct rte_mbuf **recv_buf_list = droq->recv_buf_list;
	uint32_t bytes_rsvd = 0, read_idx = droq->read_idx;
	uint16_t nb_desc = droq->nb_desc;
	uint16_t pkts;

	for (pkts = 0; pkts < new_pkts; pkts++) {
		struct otx_ep_droq_info *info;
		struct rte_mbuf *mbuf;
		uint16_t pkt_len;

		rte_prefetch0(recv_buf_list[otx_ep_incr_index(read_idx, 2, nb_desc)]);
		rte_prefetch0(rte_pktmbuf_mtod(recv_buf_list[otx_ep_incr_index(read_idx,
									       2, nb_desc)],
			      void *));

		mbuf = recv_buf_list[read_idx];
		info = cnxk_pktmbuf_mtod(mbuf, struct otx_ep_droq_info *);
		read_idx = otx_ep_incr_index(read_idx, 1, nb_desc);
		pkt_len = rte_bswap16(info->length >> 48);
		mbuf->pkt_len = pkt_len;
		mbuf->data_len = pkt_len;

		*(uint64_t *)&mbuf->rearm_data = droq->rearm_data;
		rx_pkts[pkts] = mbuf;
		bytes_rsvd += pkt_len;
	}
	droq->read_idx = read_idx;

	droq->refill_count += new_pkts;
	droq->pkts_pending -= new_pkts;
	/* Stats */
	droq->stats.pkts_received += new_pkts;
	droq->stats.bytes_received += bytes_rsvd;
}
