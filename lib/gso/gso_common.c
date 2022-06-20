/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <stdbool.h>
#include <errno.h>

#include <rte_memcpy.h>

#include "gso_common.h"

static inline void
hdr_segment_init(struct rte_mbuf *hdr_segment, struct rte_mbuf *pkt,
		uint16_t pkt_hdr_offset)
{
	/* Copy MBUF metadata */
	hdr_segment->nb_segs = 1;
	hdr_segment->port = pkt->port;
	hdr_segment->ol_flags = pkt->ol_flags;
	hdr_segment->packet_type = pkt->packet_type;
	hdr_segment->pkt_len = pkt_hdr_offset;
	hdr_segment->data_len = pkt_hdr_offset;
	hdr_segment->tx_offload = pkt->tx_offload;

	/* Copy the packet header */
	rte_memcpy(rte_pktmbuf_mtod(hdr_segment, char *),
			rte_pktmbuf_mtod(pkt, char *),
			pkt_hdr_offset);
}

static inline void
free_gso_segment(struct rte_mbuf **pkts, uint16_t nb_pkts)
{
	uint16_t i;

	for (i = 0; i < nb_pkts; i++)
		rte_pktmbuf_free(pkts[i]);
}

int
gso_do_segment(struct rte_mbuf *pkt,
		uint16_t pkt_hdr_offset,
		uint16_t pyld_unit_size,
		struct rte_mempool *direct_pool,
		struct rte_mempool *indirect_pool,
		struct rte_mbuf **pkts_out,
		uint16_t nb_pkts_out)
{
	struct rte_mbuf *pkt_in;
	struct rte_mbuf *hdr_segment, *pyld_segment, *prev_segment;
	uint16_t pkt_in_data_pos, segment_bytes_remaining;
	uint16_t pyld_len, nb_segs;
	bool more_in_pkt, more_out_segs;

	pkt_in = pkt;
	nb_segs = 0;
	more_in_pkt = 1;
	pkt_in_data_pos = pkt_hdr_offset;

	while (more_in_pkt) {
		if (unlikely(nb_segs >= nb_pkts_out)) {
			free_gso_segment(pkts_out, nb_segs);
			return -EINVAL;
		}

		/* Allocate a direct MBUF */
		hdr_segment = rte_pktmbuf_alloc(direct_pool);
		if (unlikely(hdr_segment == NULL)) {
			free_gso_segment(pkts_out, nb_segs);
			return -ENOMEM;
		}
		/* Fill the packet header */
		hdr_segment_init(hdr_segment, pkt, pkt_hdr_offset);

		prev_segment = hdr_segment;
		segment_bytes_remaining = pyld_unit_size;
		more_out_segs = 1;

		while (more_out_segs && more_in_pkt) {
			/* Allocate an indirect MBUF */
			pyld_segment = rte_pktmbuf_alloc(indirect_pool);
			if (unlikely(pyld_segment == NULL)) {
				rte_pktmbuf_free(hdr_segment);
				free_gso_segment(pkts_out, nb_segs);
				return -ENOMEM;
			}
			/* Attach to current MBUF segment of pkt */
			rte_pktmbuf_attach(pyld_segment, pkt_in);

			prev_segment->next = pyld_segment;
			prev_segment = pyld_segment;

			pyld_len = segment_bytes_remaining;
			if (pyld_len + pkt_in_data_pos > pkt_in->data_len)
				pyld_len = pkt_in->data_len - pkt_in_data_pos;

			pyld_segment->data_off = pkt_in_data_pos +
				pkt_in->data_off;
			pyld_segment->data_len = pyld_len;

			/* Update header segment */
			hdr_segment->pkt_len += pyld_len;
			hdr_segment->nb_segs++;

			pkt_in_data_pos += pyld_len;
			segment_bytes_remaining -= pyld_len;

			/* Finish processing a MBUF segment of pkt */
			if (pkt_in_data_pos == pkt_in->data_len) {
				pkt_in = pkt_in->next;
				pkt_in_data_pos = 0;
				if (pkt_in == NULL)
					more_in_pkt = 0;
			}

			/* Finish generating a GSO segment */
			if (segment_bytes_remaining == 0)
				more_out_segs = 0;
		}
		pkts_out[nb_segs++] = hdr_segment;
	}
	return nb_segs;
}
