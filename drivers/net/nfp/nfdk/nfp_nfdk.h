/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_NFDK_H__
#define __NFP_NFDK_H__

#include <nfp_platform.h>

#include "../nfp_rxtx.h"
#include "nfp_net_common.h"

#define NFDK_TX_DESC_PER_SIMPLE_PKT     2

#define NFDK_TX_MAX_DATA_PER_HEAD       0x00001000    /* 4K */
#define NFDK_TX_MAX_DATA_PER_DESC       0x00004000    /* 16K */
#define NFDK_TX_MAX_DATA_PER_BLOCK      0x00010000    /* 64K */

/* The mask of 'dma_len_xx' of address descriptor */
#define NFDK_DESC_TX_DMA_LEN_HEAD       0x0FFF        /* [0,11] */
#define NFDK_DESC_TX_DMA_LEN            0x3FFF        /* [0,13] */

/* The mask of upper 4 bit of first address descriptor */
#define NFDK_DESC_TX_TYPE_HEAD          0xF000        /* [12,15] */

/* The value of upper 4 bit of first address descriptor */
#define NFDK_DESC_TX_TYPE_GATHER        1
#define NFDK_DESC_TX_TYPE_TSO           2
#define NFDK_DESC_TX_TYPE_SIMPLE        8

/* The 'end of chain' flag of address descriptor */
#define NFDK_DESC_TX_EOP                RTE_BIT32(14)

/* Flags in the host metadata descriptor */
#define NFDK_DESC_TX_CHAIN_META         RTE_BIT32(3)
#define NFDK_DESC_TX_ENCAP              RTE_BIT32(2)
#define NFDK_DESC_TX_L4_CSUM            RTE_BIT32(1)
#define NFDK_DESC_TX_L3_CSUM            RTE_BIT32(0)

#define NFDK_TX_DESC_BLOCK_SZ           256
#define NFDK_TX_DESC_BLOCK_CNT          (NFDK_TX_DESC_BLOCK_SZ /         \
					sizeof(struct nfp_net_nfdk_tx_desc))
#define NFDK_TX_DESC_STOP_CNT           (NFDK_TX_DESC_BLOCK_CNT *        \
					NFDK_TX_DESC_PER_SIMPLE_PKT)
#define D_BLOCK_CPL(idx)               (NFDK_TX_DESC_BLOCK_CNT -        \
					(idx) % NFDK_TX_DESC_BLOCK_CNT)
/* Convenience macro for wrapping descriptor index on ring size */
#define D_IDX(ring, idx)               ((idx) & ((ring)->tx_count - 1))

/*
 * A full TX descriptor consists of one or more address descriptors,
 * followed by a TX metadata descriptor, and finally a TSO descriptor for
 * TSO packets.
 *
 * --> Header address descriptor:
 * Bit    3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
 * -----\ 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * Word  +-+-+---+-----------------------+-------------------------------+
 *    0  |S|E| TP|     dma_len_12        |         dma_addr_hi           |
 *       +-+-+---+-----------------------+-------------------------------+
 *    1  |                          dma_addr_lo                          |
 *       +---------------------------------------------------------------+
 *
 * --> Subsequent address descriptor(s):
 * Bit    3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
 * -----\ 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * Word  +-+-+---------------------------+-------------------------------+
 *    0  |S|E|         dma_len_14        |          dma_addr_hi          |
 *       +-+-+---------------------------+-------------------------------+
 *    1  |                          dma_addr_lo                          |
 *       +---------------------------------------------------------------+
 *
 * S - Simple Packet descriptor
 * TP - Type of descriptor
 * E - End of chain
 * dma_len - length of the host memory in bytes -1
 * dma_addr_hi - bits [47:32] of host memory address
 * dma_addr_lo - bits [31:0] of host memory address
 *
 * --> Metadata descriptor
 * Bit     3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
 * -----\  1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * Word   +-------+-----------------------+---------------------+---+-----+
 *    0   |  ZERO |   Rsvd (64b support)  |       TBD meta      | MT| CSUM|
 *        +-------+-----------------------+---------------------+---+-----+
 *    1   |                           TBD meta                            |
 *        +---------------------------------------------------------------+
 *
 * --> TSO descriptor
 * The following is only present if TP above indicates LSO:
 * Bit    3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
 * -----\ 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
 * Word  +---------------+---------------+---+---------------------------+
 *    0  | total_segments|   header_len  |sp0|          mss              |
 *       +---------------+---------------+---+---------------------------+
 *    1  |               sp1             |       L4      |      L3       |
 *       +---------------------------------------------------------------+
 *
 * total_segments - LSO: Total number of segments
 * header_len - LSO: length of the LSO header in bytes
 * sp0 - Spare Bits (ZERO)
 * mss - LSO: TCP MSS, maximum segment size of TCP payload
 * sp1 - Spare Bits (ZERO)
 * L4 - Layer 4 data
 * L3 - Layer 3 data
 */
struct nfp_net_nfdk_tx_desc {
	union {
		/** Address descriptor */
		struct {
			rte_le16_t dma_addr_hi;  /**< High bits of host buf address */
			rte_le16_t dma_len_type; /**< Length to DMA for this desc */
			rte_le32_t dma_addr_lo;  /**< Low 32bit of host buf addr */
		};

		/** TSO descriptor */
		struct {
			rte_le16_t mss;        /**< MSS to be used for LSO */
			uint8_t lso_hdrlen;    /**< LSO, TCP payload offset */
			uint8_t lso_totsegs;   /**< LSO, total segments */
			uint8_t l3_offset;     /**< L3 header offset */
			uint8_t l4_offset;     /**< L4 header offset */
			rte_le16_t lso_meta_res; /**< Rsvd bits in TSO metadata */
		};

		/** Metadata descriptor */
		struct {
			uint8_t flags;         /**< TX Flags, see @NFDK_DESC_TX_* */
			uint8_t reserved[7];   /**< Meta byte place holder */
		};

		rte_le32_t vals[2];
		rte_le64_t raw;
	};
};

static inline uint32_t
nfp_net_nfdk_free_tx_desc(struct nfp_net_txq *txq)
{
	uint32_t free_desc;

	if (txq->wr_p >= txq->rd_p)
		free_desc = txq->tx_count - (txq->wr_p - txq->rd_p);
	else
		free_desc = txq->rd_p - txq->wr_p;

	return (free_desc > NFDK_TX_DESC_STOP_CNT) ?
			(free_desc - NFDK_TX_DESC_STOP_CNT) : 0;
}

/**
 * Check if the TX queue free descriptors is below tx_free_threshold
 * for firmware of nfdk
 *
 * This function uses the host copy* of read/write pointers.
 *
 * @param txq
 *   TX queue to check
 */
static inline bool
nfp_net_nfdk_txq_full(struct nfp_net_txq *txq)
{
	return (nfp_net_nfdk_free_tx_desc(txq) < txq->tx_free_thresh);
}

static inline uint16_t
nfp_net_nfdk_headlen_to_segs(uint16_t headlen)
{
	/* First descriptor fits less data, so adjust for that */
	return DIV_ROUND_UP(headlen + NFDK_TX_MAX_DATA_PER_DESC - NFDK_TX_MAX_DATA_PER_HEAD,
			NFDK_TX_MAX_DATA_PER_DESC);
}

/* Set TX CSUM offload flags in TX descriptor of nfdk */
static inline uint64_t
nfp_net_nfdk_tx_cksum(struct nfp_net_txq *txq,
		struct rte_mbuf *mb,
		uint64_t flags)
{
	uint64_t ol_flags;
	struct nfp_net_hw *hw = txq->hw;

	if ((hw->super.cap & NFP_NET_CFG_CTRL_TXCSUM) == 0)
		return flags;

	ol_flags = mb->ol_flags;

	/* Set TCP csum offload if TSO enabled. */
	if ((ol_flags & RTE_MBUF_F_TX_TCP_SEG) != 0)
		flags |= NFDK_DESC_TX_L4_CSUM;

	if ((ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) != 0)
		flags |= NFDK_DESC_TX_ENCAP;

	/* IPv6 does not need checksum */
	if ((ol_flags & RTE_MBUF_F_TX_IP_CKSUM) != 0)
		flags |= NFDK_DESC_TX_L3_CSUM;

	if ((ol_flags & RTE_MBUF_F_TX_L4_MASK) != 0)
		flags |= NFDK_DESC_TX_L4_CSUM;

	return flags;
}

uint32_t nfp_flower_nfdk_pkt_add_metadata(struct rte_mbuf *mbuf,
		uint32_t port_id);
uint16_t nfp_net_nfdk_xmit_pkts_common(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts,
		bool repr_flag);
uint16_t nfp_net_nfdk_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);
int nfp_net_nfdk_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t queue_idx,
		uint16_t nb_desc,
		unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);
int nfp_net_nfdk_tx_maybe_close_block(struct nfp_net_txq *txq,
		struct rte_mbuf *pkt);
int nfp_net_nfdk_set_meta_data(struct rte_mbuf *pkt,
		struct nfp_net_txq *txq,
		uint64_t *metadata);
void nfp_net_nfdk_xmit_pkts_set(struct rte_eth_dev *eth_dev);

#endif /* __NFP_NFDK_H__ */
