/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef _NFP_NFD3_H_
#define _NFP_NFD3_H_

/* TX descriptor format */
#define NFD3_DESC_TX_EOP                RTE_BIT32(7)
#define NFD3_DESC_TX_OFFSET_MASK        (0x7F)        /* [0,6] */

/* Flags in the host TX descriptor */
#define NFD3_DESC_TX_CSUM               RTE_BIT32(7)
#define NFD3_DESC_TX_IP4_CSUM           RTE_BIT32(6)
#define NFD3_DESC_TX_TCP_CSUM           RTE_BIT32(5)
#define NFD3_DESC_TX_UDP_CSUM           RTE_BIT32(4)
#define NFD3_DESC_TX_VLAN               RTE_BIT32(3)
#define NFD3_DESC_TX_LSO                RTE_BIT32(2)
#define NFD3_DESC_TX_ENCAP              RTE_BIT32(1)
#define NFD3_DESC_TX_O_IP4_CSUM         RTE_BIT32(0)

#define NFD3_TX_DESC_PER_PKT     1

struct nfp_net_nfd3_tx_desc {
	union {
		struct {
			uint8_t dma_addr_hi; /* High bits of host buf address */
			uint16_t dma_len;    /* Length to DMA for this desc */
			/* Offset in buf where pkt starts + highest bit is eop flag */
			uint8_t offset_eop;
			uint32_t dma_addr_lo; /* Low 32bit of host buf addr */

			uint16_t mss;         /* MSS to be used for LSO */
			uint8_t lso_hdrlen;   /* LSO, where the data starts */
			uint8_t flags;        /* TX Flags, see @NFD3_DESC_TX_* */

			union {
				struct {
					uint8_t l3_offset; /* L3 header offset */
					uint8_t l4_offset; /* L4 header offset */
				};
				uint16_t vlan; /* VLAN tag to add if indicated */
			};
			uint16_t data_len;     /* Length of frame + meta data */
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

/*
 * nfp_net_nfd3_txq_full() - Check if the TX queue free descriptors
 * is below tx_free_threshold for firmware of nfd3
 *
 * @txq: TX queue to check
 *
 * This function uses the host copy* of read/write pointers.
 */
static inline bool
nfp_net_nfd3_txq_full(struct nfp_net_txq *txq)
{
	return (nfp_net_nfd3_free_tx_desc(txq) < txq->tx_free_thresh);
}

/* nfp_net_nfd3_tx_tso() - Set NFD3 TX descriptor for TSO */
static inline void
nfp_net_nfd3_tx_tso(struct nfp_net_txq *txq,
		struct nfp_net_nfd3_tx_desc *txd,
		struct rte_mbuf *mb)
{
	uint64_t ol_flags;
	struct nfp_net_hw *hw = txq->hw;

	if ((hw->cap & NFP_NET_CFG_CTRL_LSO_ANY) == 0)
		goto clean_txd;

	ol_flags = mb->ol_flags;
	if ((ol_flags & RTE_MBUF_F_TX_TCP_SEG) == 0)
		goto clean_txd;

	txd->l3_offset = mb->l2_len;
	txd->l4_offset = mb->l2_len + mb->l3_len;
	txd->lso_hdrlen = mb->l2_len + mb->l3_len + mb->l4_len;

	if ((ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) != 0) {
		txd->l3_offset += mb->outer_l2_len + mb->outer_l3_len;
		txd->l4_offset += mb->outer_l2_len + mb->outer_l3_len;
		txd->lso_hdrlen += mb->outer_l2_len + mb->outer_l3_len;
	}

	txd->mss = rte_cpu_to_le_16(mb->tso_segsz);
	txd->flags = NFD3_DESC_TX_LSO;

	return;

clean_txd:
	txd->flags = 0;
	txd->l3_offset = 0;
	txd->l4_offset = 0;
	txd->lso_hdrlen = 0;
	txd->mss = 0;
}

/* nfp_net_nfd3_tx_cksum() - Set TX CSUM offload flags in NFD3 TX descriptor */
static inline void
nfp_net_nfd3_tx_cksum(struct nfp_net_txq *txq,
		struct nfp_net_nfd3_tx_desc *txd,
		struct rte_mbuf *mb)
{
	uint64_t ol_flags;
	struct nfp_net_hw *hw = txq->hw;

	if ((hw->cap & NFP_NET_CFG_CTRL_TXCSUM) == 0)
		return;

	ol_flags = mb->ol_flags;

	/* Set TCP csum offload if TSO enabled. */
	if ((ol_flags & RTE_MBUF_F_TX_TCP_SEG) != 0)
		txd->flags |= NFD3_DESC_TX_TCP_CSUM;

	/* IPv6 does not need checksum */
	if ((ol_flags & RTE_MBUF_F_TX_IP_CKSUM) != 0)
		txd->flags |= NFD3_DESC_TX_IP4_CSUM;

	if ((ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) != 0)
		txd->flags |= NFD3_DESC_TX_ENCAP;

	switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
	case RTE_MBUF_F_TX_UDP_CKSUM:
		txd->flags |= NFD3_DESC_TX_UDP_CSUM;
		break;
	case RTE_MBUF_F_TX_TCP_CKSUM:
		txd->flags |= NFD3_DESC_TX_TCP_CSUM;
		break;
	}

	if ((ol_flags & (RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_L4_MASK)) != 0)
		txd->flags |= NFD3_DESC_TX_CSUM;
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

#endif /* _NFP_NFD3_H_ */
