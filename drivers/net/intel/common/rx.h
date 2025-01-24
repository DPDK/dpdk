/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Intel Corporation
 */

#ifndef _COMMON_INTEL_RX_H_
#define _COMMON_INTEL_RX_H_

#include <stdint.h>
#include <unistd.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>

#define CI_RX_BURST 32

static inline uint16_t
ci_rx_reassemble_packets(struct rte_mbuf **rx_bufs, uint16_t nb_bufs, uint8_t *split_flags,
		struct rte_mbuf **pkt_first_seg, struct rte_mbuf **pkt_last_seg,
		const uint8_t crc_len)
{
	struct rte_mbuf *pkts[CI_RX_BURST] = {0}; /*finished pkts*/
	struct rte_mbuf *start = *pkt_first_seg;
	struct rte_mbuf *end = *pkt_last_seg;
	unsigned int pkt_idx, buf_idx;

	for (buf_idx = 0, pkt_idx = 0; buf_idx < nb_bufs; buf_idx++) {
		if (end) {
			/* processing a split packet */
			end->next = rx_bufs[buf_idx];
			rx_bufs[buf_idx]->data_len += crc_len;

			start->nb_segs++;
			start->pkt_len += rx_bufs[buf_idx]->data_len;
			end = end->next;

			if (!split_flags[buf_idx]) {
				/* it's the last packet of the set */
				start->hash = end->hash;
				start->vlan_tci = end->vlan_tci;
				start->ol_flags = end->ol_flags;
				/* we need to strip crc for the whole packet */
				start->pkt_len -= crc_len;
				if (end->data_len > crc_len) {
					end->data_len -= crc_len;
				} else {
					/* free up last mbuf */
					struct rte_mbuf *secondlast = start;

					start->nb_segs--;
					while (secondlast->next != end)
						secondlast = secondlast->next;
					secondlast->data_len -= (crc_len - end->data_len);
					secondlast->next = NULL;
					rte_pktmbuf_free_seg(end);
				}
				pkts[pkt_idx++] = start;
				start = NULL;
				end = NULL;
			}
		} else {
			/* not processing a split packet */
			if (!split_flags[buf_idx]) {
				/* not a split packet, save and skip */
				pkts[pkt_idx++] = rx_bufs[buf_idx];
				continue;
			}
			start = rx_bufs[buf_idx];
			end = start;
			rx_bufs[buf_idx]->data_len += crc_len;
			rx_bufs[buf_idx]->pkt_len += crc_len;
		}
	}

	/* save the partial packet for next time */
	*pkt_first_seg = start;
	*pkt_last_seg = end;
	memcpy(rx_bufs, pkts, pkt_idx * (sizeof(*pkts)));
	return pkt_idx;
}

static inline uint64_t
ci_rxq_mbuf_initializer(uint16_t port_id)
{
	struct rte_mbuf mb_def = {
		.nb_segs = 1,
		.data_off = RTE_PKTMBUF_HEADROOM,
		.port = port_id,
	};
	rte_mbuf_refcnt_set(&mb_def, 1);

	return mb_def.rearm_data[0];
}

/* basic checks for a vector-driver capable queue.
 * Individual drivers may have other further tests beyond this.
 */
static inline bool
ci_rxq_vec_capable(uint16_t nb_desc, uint16_t rx_free_thresh, uint64_t offloads)
{
	if (!rte_is_power_of_2(nb_desc) ||
			rx_free_thresh < CI_RX_BURST ||
			(nb_desc % rx_free_thresh) != 0)
		return false;

	/* no driver supports timestamping or buffer split on vector path */
	if ((offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) ||
			(offloads & RTE_ETH_RX_OFFLOAD_BUFFER_SPLIT))
		return false;

	return true;
}

#endif /* _COMMON_INTEL_RX_H_ */
