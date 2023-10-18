/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include "otx_ep_common.h"
#include "otx2_ep_vf.h"
#include "otx_ep_rxtx.h"

static inline int
cnxk_ep_rx_refill_mbuf(struct otx_ep_droq *droq, uint32_t count)
{
	struct otx_ep_droq_desc *desc_ring = droq->desc_ring;
	struct rte_mbuf **recv_buf_list = droq->recv_buf_list;
	uint32_t refill_idx = droq->refill_idx;
	struct rte_mbuf *buf;
	uint32_t i;
	int rc;

	rc = rte_pktmbuf_alloc_bulk(droq->mpool, &recv_buf_list[refill_idx], count);
	if (unlikely(rc)) {
		droq->stats.rx_alloc_failure++;
		return rc;
	}

	for (i = 0; i < count; i++) {
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
	uint32_t desc_refilled = 0, count;
	uint32_t nb_desc = droq->nb_desc;
	uint32_t refill_idx = droq->refill_idx;
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
		if (unlikely(rc)) {
			droq->stats.rx_alloc_failure++;
			return;
		}
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
cnxk_ep_check_rx_pkts(struct otx_ep_droq *droq)
{
	uint32_t new_pkts;
	uint32_t val;

	/* Batch subtractions from the HW counter to reduce PCIe traffic
	 * This adds an extra local variable, but almost halves the
	 * number of PCIe writes.
	 */
	val = __atomic_load_n(droq->pkts_sent_ism, __ATOMIC_RELAXED);
	new_pkts = val - droq->pkts_sent_ism_prev;
	droq->pkts_sent_ism_prev = val;

	if (val > (uint32_t)(1 << 31)) {
		/* Only subtract the packet count in the HW counter
		 * when count above halfway to saturation.
		 */
		rte_write64((uint64_t)val, droq->pkts_sent_reg);
		rte_mb();

		rte_write64(OTX2_SDP_REQUEST_ISM, droq->pkts_sent_reg);
		while (__atomic_load_n(droq->pkts_sent_ism, __ATOMIC_RELAXED) >= val) {
			rte_write64(OTX2_SDP_REQUEST_ISM, droq->pkts_sent_reg);
			rte_mb();
		}

		droq->pkts_sent_ism_prev = 0;
	}
	rte_write64(OTX2_SDP_REQUEST_ISM, droq->pkts_sent_reg);
	droq->pkts_pending += new_pkts;

	return new_pkts;
}

static inline int16_t __rte_hot
cnxk_ep_rx_pkts_to_process(struct otx_ep_droq *droq, uint16_t nb_pkts)
{
	if (droq->pkts_pending < nb_pkts)
		cnxk_ep_check_rx_pkts(droq);

	return RTE_MIN(nb_pkts, droq->pkts_pending);
}

static __rte_always_inline void
cnxk_ep_process_pkts_scalar(struct rte_mbuf **rx_pkts, struct otx_ep_droq *droq, uint16_t new_pkts)
{
	struct rte_mbuf **recv_buf_list = droq->recv_buf_list;
	uint32_t bytes_rsvd = 0, read_idx = droq->read_idx;
	uint16_t port_id = droq->otx_ep_dev->port_id;
	uint16_t nb_desc = droq->nb_desc;
	uint16_t pkts;

	for (pkts = 0; pkts < new_pkts; pkts++) {
		struct otx_ep_droq_info *info;
		struct rte_mbuf *mbuf;
		uint16_t pkt_len;

		mbuf = recv_buf_list[read_idx];
		info = rte_pktmbuf_mtod(mbuf, struct otx_ep_droq_info *);
		read_idx = otx_ep_incr_index(read_idx, 1, nb_desc);
		pkt_len = rte_bswap16(info->length >> 48);
		mbuf->data_off += OTX_EP_INFO_SIZE;
		mbuf->pkt_len = pkt_len;
		mbuf->data_len = pkt_len;
		mbuf->port = port_id;
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

static __rte_always_inline void
cnxk_ep_process_pkts_scalar_mseg(struct rte_mbuf **rx_pkts, struct otx_ep_droq *droq,
				 uint16_t new_pkts)
{
	struct rte_mbuf **recv_buf_list = droq->recv_buf_list;
	uint32_t total_pkt_len, bytes_rsvd = 0;
	uint16_t port_id = droq->otx_ep_dev->port_id;
	uint16_t nb_desc = droq->nb_desc;
	uint16_t pkts;

	for (pkts = 0; pkts < new_pkts; pkts++) {
		struct otx_ep_droq_info *info;
		struct rte_mbuf *first_buf = NULL;
		struct rte_mbuf *last_buf = NULL;
		struct rte_mbuf *mbuf;
		uint32_t pkt_len = 0;

		mbuf = recv_buf_list[droq->read_idx];
		info = rte_pktmbuf_mtod(mbuf, struct otx_ep_droq_info *);

		total_pkt_len = rte_bswap16(info->length >> 48) + OTX_EP_INFO_SIZE;

		while (pkt_len < total_pkt_len) {
			int cpy_len;

			cpy_len = ((pkt_len + droq->buffer_size) > total_pkt_len)
					? ((uint32_t)total_pkt_len - pkt_len) : droq->buffer_size;

			mbuf = droq->recv_buf_list[droq->read_idx];

			if (!pkt_len) {
				/* Note the first seg */
				first_buf = mbuf;
				mbuf->data_off += OTX_EP_INFO_SIZE;
				mbuf->pkt_len = cpy_len - OTX_EP_INFO_SIZE;
				mbuf->data_len = cpy_len - OTX_EP_INFO_SIZE;
			} else {
				mbuf->pkt_len = cpy_len;
				mbuf->data_len = cpy_len;
				first_buf->nb_segs++;
				first_buf->pkt_len += mbuf->pkt_len;
			}

			if (last_buf)
				last_buf->next = mbuf;

			last_buf = mbuf;

			pkt_len += cpy_len;
			droq->read_idx = otx_ep_incr_index(droq->read_idx, 1, nb_desc);
			droq->refill_count++;
		}
		mbuf = first_buf;
		mbuf->port = port_id;
		rx_pkts[pkts] = mbuf;
		bytes_rsvd += pkt_len;
	}

	droq->refill_count += new_pkts;
	droq->pkts_pending -= pkts;
	/* Stats */
	droq->stats.pkts_received += pkts;
	droq->stats.bytes_received += bytes_rsvd;
}

uint16_t __rte_noinline __rte_hot
cnxk_ep_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct otx_ep_droq *droq = (struct otx_ep_droq *)rx_queue;
	uint16_t new_pkts;

	new_pkts = cnxk_ep_rx_pkts_to_process(droq, nb_pkts);
	cnxk_ep_process_pkts_scalar(rx_pkts, droq, new_pkts);

	/* Refill RX buffers */
	if (droq->refill_count >= DROQ_REFILL_THRESHOLD)
		cnxk_ep_rx_refill(droq);

	return new_pkts;
}

uint16_t __rte_noinline __rte_hot
cn9k_ep_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct otx_ep_droq *droq = (struct otx_ep_droq *)rx_queue;
	uint16_t new_pkts;

	new_pkts = cnxk_ep_rx_pkts_to_process(droq, nb_pkts);
	cnxk_ep_process_pkts_scalar(rx_pkts, droq, new_pkts);

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

	return new_pkts;
}

uint16_t __rte_noinline __rte_hot
cnxk_ep_recv_pkts_mseg(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct otx_ep_droq *droq = (struct otx_ep_droq *)rx_queue;
	uint16_t new_pkts;

	new_pkts = cnxk_ep_rx_pkts_to_process(droq, nb_pkts);
	cnxk_ep_process_pkts_scalar_mseg(rx_pkts, droq, new_pkts);

	/* Refill RX buffers */
	if (droq->refill_count >= DROQ_REFILL_THRESHOLD)
		cnxk_ep_rx_refill(droq);

	return new_pkts;
}

uint16_t __rte_noinline __rte_hot
cn9k_ep_recv_pkts_mseg(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct otx_ep_droq *droq = (struct otx_ep_droq *)rx_queue;
	uint16_t new_pkts;

	new_pkts = cnxk_ep_rx_pkts_to_process(droq, nb_pkts);
	cnxk_ep_process_pkts_scalar_mseg(rx_pkts, droq, new_pkts);

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

	return new_pkts;
}
