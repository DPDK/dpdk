/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef _NFP_NFD3_H_
#define _NFP_NFD3_H_

/* TX descriptor format */
#define PCIE_DESC_TX_EOP                (1 << 7)
#define PCIE_DESC_TX_OFFSET_MASK        (0x7f)

/* Flags in the host TX descriptor */
#define PCIE_DESC_TX_CSUM               (1 << 7)
#define PCIE_DESC_TX_IP4_CSUM           (1 << 6)
#define PCIE_DESC_TX_TCP_CSUM           (1 << 5)
#define PCIE_DESC_TX_UDP_CSUM           (1 << 4)
#define PCIE_DESC_TX_VLAN               (1 << 3)
#define PCIE_DESC_TX_LSO                (1 << 2)
#define PCIE_DESC_TX_ENCAP_NONE         (0)
#define PCIE_DESC_TX_ENCAP              (1 << 1)
#define PCIE_DESC_TX_O_IP4_CSUM         (1 << 0)

#define NFD3_TX_DESC_PER_SIMPLE_PKT     1

struct nfp_net_nfd3_tx_desc {
	union {
		struct {
			uint8_t dma_addr_hi; /* High bits of host buf address */
			__le16 dma_len;     /* Length to DMA for this desc */
			uint8_t offset_eop; /* Offset in buf where pkt starts +
					     * highest bit is eop flag, low 7bit is meta_len.
					     */
			__le32 dma_addr_lo; /* Low 32bit of host buf addr */

			__le16 mss;         /* MSS to be used for LSO */
			uint8_t lso_hdrlen; /* LSO, where the data starts */
			uint8_t flags;      /* TX Flags, see @PCIE_DESC_TX_* */

			union {
				struct {
					/*
					 * L3 and L4 header offsets required
					 * for TSOv2
					 */
					uint8_t l3_offset;
					uint8_t l4_offset;
				};
				__le16 vlan; /* VLAN tag to add if indicated */
			};
			__le16 data_len;    /* Length of frame + meta data */
		} __rte_packed;
		__le32 vals[4];
	};
};

/* Leaving always free descriptors for avoiding wrapping confusion */
static inline uint32_t
nfp_net_nfd3_free_tx_desc(struct nfp_net_txq *txq)
{
	if (txq->wr_p >= txq->rd_p)
		return txq->tx_count - (txq->wr_p - txq->rd_p) - 8;
	else
		return txq->rd_p - txq->wr_p - 8;
}

/*
 * nfp_net_nfd3_txq_full() - Check if the TX queue free descriptors
 * is below tx_free_threshold for firmware of nfd3
 *
 * @txq: TX queue to check
 *
 * This function uses the host copy* of read/write pointers.
 */
static inline uint32_t
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

	if (!(hw->cap & NFP_NET_CFG_CTRL_LSO_ANY))
		goto clean_txd;

	ol_flags = mb->ol_flags;

	if (!(ol_flags & RTE_MBUF_F_TX_TCP_SEG))
		goto clean_txd;

	txd->l3_offset = mb->l2_len;
	txd->l4_offset = mb->l2_len + mb->l3_len;
	txd->lso_hdrlen = mb->l2_len + mb->l3_len + mb->l4_len;

	if (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK) {
		txd->l3_offset += mb->outer_l2_len + mb->outer_l3_len;
		txd->l4_offset += mb->outer_l2_len + mb->outer_l3_len;
		txd->lso_hdrlen += mb->outer_l2_len + mb->outer_l3_len;
	}

	txd->mss = rte_cpu_to_le_16(mb->tso_segsz);
	txd->flags = PCIE_DESC_TX_LSO;
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
nfp_net_nfd3_tx_cksum(struct nfp_net_txq *txq, struct nfp_net_nfd3_tx_desc *txd,
		 struct rte_mbuf *mb)
{
	uint64_t ol_flags;
	struct nfp_net_hw *hw = txq->hw;

	if (!(hw->cap & NFP_NET_CFG_CTRL_TXCSUM))
		return;

	ol_flags = mb->ol_flags;

	/* Set TCP csum offload if TSO enabled. */
	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
		txd->flags |= PCIE_DESC_TX_TCP_CSUM;

	/* IPv6 does not need checksum */
	if (ol_flags & RTE_MBUF_F_TX_IP_CKSUM)
		txd->flags |= PCIE_DESC_TX_IP4_CSUM;

	if (ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK)
		txd->flags |= PCIE_DESC_TX_ENCAP;

	switch (ol_flags & RTE_MBUF_F_TX_L4_MASK) {
	case RTE_MBUF_F_TX_UDP_CKSUM:
		txd->flags |= PCIE_DESC_TX_UDP_CSUM;
		break;
	case RTE_MBUF_F_TX_TCP_CKSUM:
		txd->flags |= PCIE_DESC_TX_TCP_CSUM;
		break;
	}

	if (ol_flags & (RTE_MBUF_F_TX_IP_CKSUM | RTE_MBUF_F_TX_L4_MASK))
		txd->flags |= PCIE_DESC_TX_CSUM;
}

uint16_t nfp_net_nfd3_xmit_pkts(void *tx_queue,
		struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);
int nfp_net_nfd3_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t queue_idx,
		uint16_t nb_desc,
		unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

#endif /* _NFP_NFD3_H_ */
