/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef _NFP_NFDK_H_
#define _NFP_NFDK_H_

#define NFDK_TX_DESC_PER_SIMPLE_PKT     2
#define NFDK_TX_DESC_GATHER_MAX         17

#define NFDK_TX_MAX_DATA_PER_HEAD       0x00001000
#define NFDK_TX_MAX_DATA_PER_DESC       0x00004000
#define NFDK_TX_MAX_DATA_PER_BLOCK      0x00010000

#define NFDK_DESC_TX_DMA_LEN_HEAD       0x0FFF        /* [0,11] */
#define NFDK_DESC_TX_DMA_LEN            0x3FFF        /* [0,13] */
#define NFDK_DESC_TX_TYPE_HEAD          0xF000        /* [12,15] */

#define NFDK_DESC_TX_TYPE_GATHER        1
#define NFDK_DESC_TX_TYPE_TSO           2
#define NFDK_DESC_TX_TYPE_SIMPLE        8

/* TX descriptor format */
#define NFDK_DESC_TX_EOP                RTE_BIT32(14)

/* Flags in the host TX descriptor */
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

struct nfp_net_nfdk_tx_desc {
	union {
		struct {
			uint16_t dma_addr_hi;  /* High bits of host buf address */
			uint16_t dma_len_type; /* Length to DMA for this desc */
			uint32_t dma_addr_lo;  /* Low 32bit of host buf addr */
		};

		struct {
			uint16_t mss;          /* MSS to be used for LSO */
			uint8_t lso_hdrlen;    /* LSO, TCP payload offset */
			uint8_t lso_totsegs;   /* LSO, total segments */
			uint8_t l3_offset;     /* L3 header offset */
			uint8_t l4_offset;     /* L4 header offset */
			uint16_t lso_meta_res; /* Rsvd bits in TSO metadata */
		};

		struct {
			uint8_t flags;         /* TX Flags, see @NFDK_DESC_TX_* */
			uint8_t reserved[7];   /* meta byte placeholder */
		};

		uint32_t vals[2];
		uint64_t raw;
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

/*
 * nfp_net_nfdk_txq_full() - Check if the TX queue free descriptors
 * is below tx_free_threshold for firmware of nfdk
 *
 * @txq: TX queue to check
 *
 * This function uses the host copy* of read/write pointers.
 */
static inline bool
nfp_net_nfdk_txq_full(struct nfp_net_txq *txq)
{
	return (nfp_net_nfdk_free_tx_desc(txq) < txq->tx_free_thresh);
}

/* nfp_net_nfdk_tx_cksum() - Set TX CSUM offload flags in TX descriptor of nfdk */
static inline uint64_t
nfp_net_nfdk_tx_cksum(struct nfp_net_txq *txq,
		struct rte_mbuf *mb,
		uint64_t flags)
{
	uint64_t ol_flags;
	struct nfp_net_hw *hw = txq->hw;

	if ((hw->cap & NFP_NET_CFG_CTRL_TXCSUM) == 0)
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

/* nfp_net_nfdk_tx_tso() - Set TX descriptor for TSO of nfdk */
static inline uint64_t
nfp_net_nfdk_tx_tso(struct nfp_net_txq *txq,
		struct rte_mbuf *mb)
{
	uint8_t outer_len;
	uint64_t ol_flags;
	struct nfp_net_nfdk_tx_desc txd;
	struct nfp_net_hw *hw = txq->hw;

	txd.raw = 0;

	if ((hw->cap & NFP_NET_CFG_CTRL_LSO_ANY) == 0)
		return txd.raw;

	ol_flags = mb->ol_flags;
	if ((ol_flags & RTE_MBUF_F_TX_TCP_SEG) == 0)
		return txd.raw;

	txd.l3_offset = mb->l2_len;
	txd.l4_offset = mb->l2_len + mb->l3_len;
	txd.lso_meta_res = 0;
	txd.mss = rte_cpu_to_le_16(mb->tso_segsz);
	txd.lso_hdrlen = mb->l2_len + mb->l3_len + mb->l4_len;
	txd.lso_totsegs = (mb->pkt_len + mb->tso_segsz) / mb->tso_segsz;

	if ((ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) != 0) {
		outer_len = mb->outer_l2_len + mb->outer_l3_len;
		txd.l3_offset += outer_len;
		txd.l4_offset += outer_len;
		txd.lso_hdrlen += outer_len;
	}

	return txd.raw;
}

uint16_t nfp_net_nfdk_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);
int nfp_net_nfdk_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t queue_idx,
		uint16_t nb_desc,
		unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

#endif /* _NFP_NFDK_H_ */
