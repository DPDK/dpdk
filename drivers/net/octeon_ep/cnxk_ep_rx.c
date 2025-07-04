/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include "cnxk_ep_rx.h"

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
	/* Refill RX buffers */
	if (droq->refill_count >= DROQ_REFILL_THRESHOLD)
		cnxk_ep_rx_refill(droq);

	cnxk_ep_process_pkts_scalar(rx_pkts, droq, new_pkts);

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
