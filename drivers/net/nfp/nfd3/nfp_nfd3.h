/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_NFD3_H__
#define __NFP_NFD3_H__

#include "../nfp_rxtx.h"

/* TX descriptor format */
#define NFD3_DESC_TX_EOP                RTE_BIT32(7)
#define NFD3_DESC_TX_OFFSET_MASK        (0x7F)        /* [0,6] */

#define NFD3_TX_DESC_PER_PKT     1

struct nfp_net_nfd3_tx_desc {
	union {
		struct {
			uint8_t dma_addr_hi; /**< High bits of host buf address */
			uint16_t dma_len;    /**< Length to DMA for this desc */
			/** Offset in buf where pkt starts + highest bit is eop flag */
			uint8_t offset_eop;
			uint32_t dma_addr_lo; /**< Low 32bit of host buf addr */

			uint16_t mss;         /**< MSS to be used for LSO */
			uint8_t lso_hdrlen;   /**< LSO, where the data starts */
			uint8_t flags;        /**< TX Flags, see @NFD3_DESC_TX_* */

			union {
				struct {
					uint8_t l3_offset; /**< L3 header offset */
					uint8_t l4_offset; /**< L4 header offset */
				};
				uint16_t vlan; /**< VLAN tag to add if indicated */
			};
			uint16_t data_len;     /**< Length of frame + meta data */
		} __rte_packed;
		uint32_t vals[4];
	};
};

/* Leaving always free descriptors for avoiding wrapping confusion */
static inline uint32_t
nfp_net_nfd3_free_tx_desc(struct nfp_net_txq *txq)
{
	uint32_t free_desc;

	if (txq->wr_p >= txq->rd_p)
		free_desc = txq->tx_count - (txq->wr_p - txq->rd_p);
	else
		free_desc = txq->rd_p - txq->wr_p;

	return (free_desc > 8) ? (free_desc - 8) : 0;
}

/**
 * Check if the TX queue free descriptors is below tx_free_threshold
 * for firmware with nfd3
 *
 * This function uses the host copy* of read/write pointers.
 *
 * @param txq
 *   TX queue to check
 */
static inline bool
nfp_net_nfd3_txq_full(struct nfp_net_txq *txq)
{
	return (nfp_net_nfd3_free_tx_desc(txq) < txq->tx_free_thresh);
}

uint32_t nfp_flower_nfd3_pkt_add_metadata(struct rte_mbuf *mbuf,
		uint32_t port_id);
uint16_t nfp_net_nfd3_xmit_pkts_common(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts,
		bool repr_flag);
uint16_t nfp_net_nfd3_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);
int nfp_net_nfd3_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t queue_idx,
		uint16_t nb_desc,
		unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

#endif /* __NFP_NFD3_H__ */
