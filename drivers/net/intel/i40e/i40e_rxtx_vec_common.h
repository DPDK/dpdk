/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _I40E_RXTX_VEC_COMMON_H_
#define _I40E_RXTX_VEC_COMMON_H_
#include <stdint.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>

#include "../common/rx.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"

static inline uint16_t
reassemble_packets(struct i40e_rx_queue *rxq, struct rte_mbuf **rx_bufs,
		   uint16_t nb_bufs, uint8_t *split_flags)
{
	struct rte_mbuf *pkts[RTE_I40E_VPMD_RX_BURST]; /*finished pkts*/
	struct rte_mbuf *start = rxq->pkt_first_seg;
	struct rte_mbuf *end =  rxq->pkt_last_seg;
	unsigned pkt_idx, buf_idx;

	for (buf_idx = 0, pkt_idx = 0; buf_idx < nb_bufs; buf_idx++) {
		if (end != NULL) {
			/* processing a split packet */
			end->next = rx_bufs[buf_idx];
			rx_bufs[buf_idx]->data_len += rxq->crc_len;

			start->nb_segs++;
			start->pkt_len += rx_bufs[buf_idx]->data_len;
			end = end->next;

			if (!split_flags[buf_idx]) {
				/* it's the last packet of the set */
				start->hash = end->hash;
				start->vlan_tci = end->vlan_tci;
				start->ol_flags = end->ol_flags;
				/* we need to strip crc for the whole packet */
				start->pkt_len -= rxq->crc_len;
				if (end->data_len > rxq->crc_len)
					end->data_len -= rxq->crc_len;
				else {
					/* free up last mbuf */
					struct rte_mbuf *secondlast = start;

					start->nb_segs--;
					while (secondlast->next != end)
						secondlast = secondlast->next;
					secondlast->data_len -= (rxq->crc_len -
							end->data_len);
					secondlast->next = NULL;
					rte_pktmbuf_free_seg(end);
				}
				pkts[pkt_idx++] = start;
				start = end = NULL;
			}
		} else {
			/* not processing a split packet */
			if (!split_flags[buf_idx]) {
				/* not a split packet, save and skip */
				pkts[pkt_idx++] = rx_bufs[buf_idx];
				continue;
			}
			end = start = rx_bufs[buf_idx];
			rx_bufs[buf_idx]->data_len += rxq->crc_len;
			rx_bufs[buf_idx]->pkt_len += rxq->crc_len;
		}
	}

	/* save the partial packet for next time */
	rxq->pkt_first_seg = start;
	rxq->pkt_last_seg = end;
	memcpy(rx_bufs, pkts, pkt_idx * (sizeof(*pkts)));
	return pkt_idx;
}

static inline int
i40e_tx_desc_done(struct ci_tx_queue *txq, uint16_t idx)
{
	return (txq->i40e_tx_ring[idx].cmd_type_offset_bsz &
			rte_cpu_to_le_64(I40E_TXD_QW1_DTYPE_MASK)) ==
				rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DESC_DONE);
}

static inline void
_i40e_rx_queue_release_mbufs_vec(struct i40e_rx_queue *rxq)
{
	const unsigned mask = rxq->nb_rx_desc - 1;
	unsigned i;

	if (rxq->sw_ring == NULL || rxq->rxrearm_nb >= rxq->nb_rx_desc)
		return;

	/* free all mbufs that are valid in the ring */
	if (rxq->rxrearm_nb == 0) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf != NULL)
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
		}
	} else {
		for (i = rxq->rx_tail;
		     i != rxq->rxrearm_start;
		     i = (i + 1) & mask) {
			if (rxq->sw_ring[i].mbuf != NULL)
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
		}
	}

	rxq->rxrearm_nb = rxq->nb_rx_desc;

	/* set all entries to NULL */
	memset(rxq->sw_ring, 0, sizeof(rxq->sw_ring[0]) * rxq->nb_rx_desc);
}

static inline int
i40e_rx_vec_dev_conf_condition_check_default(struct rte_eth_dev *dev)
{
#ifndef RTE_LIBRTE_IEEE1588
	struct i40e_adapter *ad =
		I40E_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;

	/* no QinQ support */
	if (rxmode->offloads & RTE_ETH_RX_OFFLOAD_VLAN_EXTEND)
		return -1;

	/**
	 * Vector mode is allowed only when number of Rx queue
	 * descriptor is power of 2.
	 */
	ad->rx_vec_allowed = true;
	for (uint16_t i = 0; i < dev->data->nb_rx_queues; i++) {
		struct i40e_rx_queue *rxq = dev->data->rx_queues[i];
		if (!rxq)
			continue;
		if (!ci_rxq_vec_capable(rxq->nb_rx_desc, rxq->rx_free_thresh, rxq->offloads)) {
			ad->rx_vec_allowed = false;
			break;
		}
	}

	return 0;
#else
	RTE_SET_USED(dev);
	return -1;
#endif
}

#endif
