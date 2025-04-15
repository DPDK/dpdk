/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2021 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_RXTX_H__
#define __NFP_RXTX_H__

#include <ethdev_driver.h>

/* Descriptor alignment */
#define NFP_ALIGN_RING_DESC 128

struct nfp_net_dp_buf {
	struct rte_mbuf *mbuf;
};

struct nfp_tx_ipsec_desc_msg {
	uint32_t sa_idx;        /**< SA index in driver table */
	uint32_t enc;           /**< IPsec enable flag */
	union {
		uint64_t value;
		struct {
			uint32_t low;
			uint32_t hi;
		};
	} esn;                  /**< Extended Sequence Number */
};

struct __rte_aligned(64) nfp_net_txq {
	/** Backpointer to nfp_net structure */
	struct nfp_net_hw *hw;
	struct nfp_net_hw_priv *hw_priv;

	/**
	 * For each descriptor keep a reference to the mbuf and
	 * DMA address used until completion is signalled.
	 */
	struct nfp_net_dp_buf *txbufs;

	/**
	 * Information about the host side queue location.
	 * It is the virtual address for the queue.
	 */
	union {
		struct nfp_net_nfd3_tx_desc *txds;
		struct nfp_net_nfdk_tx_desc *ktxds;
	};

	/**
	 * Host side read and write pointer, they are free running and
	 * have little relation to the QCP pointers.
	 */
	uint32_t wr_p;
	uint32_t rd_p;

	/** The size of the queue in number of descriptors. */
	uint32_t tx_count;

	uint32_t tx_free_thresh;

	/** The index of the QCP queue relative to the TX queue BAR. */
	uint32_t tx_qcidx;

	/** The queue index from Linux's perspective. */
	uint16_t qidx;
	uint16_t port_id;

	/** Used by NFDk only */
	uint16_t data_pending;

	/** Used by NFDk vector xmit only */
	bool simple_always;

	/**
	 * At this point 58 bytes have been used for all the fields in the
	 * TX critical path. We have room for 5 bytes and still all placed
	 * in a cache line.
	 */
	uint64_t dma;

	/** TX pointer ring write back area (indexed by queue id) */
	uint64_t *txrwb;

	/** TX pointer ring write back area DMA address */
	uint64_t txrwb_dma;

	/** Point to the base of the queue structure on the NFP. */
	uint8_t *qcp_q;
};

/* RX and freelist descriptor format */
#define PCIE_DESC_RX_DD                 (1 << 7)
#define PCIE_DESC_RX_META_LEN_MASK      (0x7f)

/* Flags in the RX descriptor */
#define PCIE_DESC_RX_RSS                (1 << 15)
#define PCIE_DESC_RX_I_IP4_CSUM         (1 << 14)
#define PCIE_DESC_RX_I_IP4_CSUM_OK      (1 << 13)
#define PCIE_DESC_RX_I_TCP_CSUM         (1 << 12)
#define PCIE_DESC_RX_I_TCP_CSUM_OK      (1 << 11)
#define PCIE_DESC_RX_I_UDP_CSUM         (1 << 10)
#define PCIE_DESC_RX_I_UDP_CSUM_OK      (1 <<  9)
#define PCIE_DESC_RX_SPARE              (1 <<  8)
#define PCIE_DESC_RX_EOP                (1 <<  7)
#define PCIE_DESC_RX_IP4_CSUM           (1 <<  6)
#define PCIE_DESC_RX_IP4_CSUM_OK        (1 <<  5)
#define PCIE_DESC_RX_TCP_CSUM           (1 <<  4)
#define PCIE_DESC_RX_TCP_CSUM_OK        (1 <<  3)
#define PCIE_DESC_RX_UDP_CSUM           (1 <<  2)
#define PCIE_DESC_RX_UDP_CSUM_OK        (1 <<  1)
#define PCIE_DESC_RX_VLAN               (1 <<  0)

#define PCIE_DESC_RX_L4_CSUM_OK         (PCIE_DESC_RX_TCP_CSUM_OK | \
					 PCIE_DESC_RX_UDP_CSUM_OK)

struct nfp_net_rx_desc {
	union {
		/** Freelist descriptor. */
		struct {
			rte_le16_t dma_addr_hi;  /**< High bits of buffer address. */
			uint8_t spare;           /**< Reserved, must be zero. */
			uint8_t dd;              /**< Whether descriptor available. */
			rte_le32_t dma_addr_lo;  /**< Low bits of buffer address. */
		} __rte_packed fld;

		/** RX descriptor. */
		struct {
			rte_le16_t data_len;     /**< Length of frame + metadata. */
			uint8_t reserved;      /**< Reserved, must be zero. */
			uint8_t meta_len_dd;   /**< Length of metadata + done flag. */

			rte_le16_t flags;        /**< RX flags. */
			rte_le16_t offload_info; /**< Offloading info. */
		} __rte_packed rxd;

		/** Reserved. */
		rte_le32_t vals[2];
	};
};

struct __rte_aligned(64) nfp_net_rxq {
	/** Backpointer to nfp_net structure */
	struct nfp_net_hw *hw;
	struct nfp_net_hw_priv *hw_priv;

	/**
	 * Point to the base addresses of the freelist queue
	 * controller peripheral queue structures on the NFP.
	 */
	uint8_t *qcp_fl;

	/**
	 * For each buffer placed on the freelist, record the
	 * associated mbuf.
	 */
	struct nfp_net_dp_buf *rxbufs;

	/**
	 * Information about the host side queue location.
	 * It is the virtual address for the queue.
	 */
	struct nfp_net_rx_desc *rxds;

	/**
	 * The mempool is created by the user specifying a mbuf size.
	 * We save here the reference of the mempool needed in the RX
	 * path and the mbuf size for checking received packets can be
	 * safely copied to the mbuf using the NFP_NET_RX_OFFSET.
	 */
	struct rte_mempool *mem_pool;

	/**
	 * Host side read pointer, free running and have little relation
	 * to the QCP pointers. It is where the driver start reading
	 * descriptors for newly arrive packets from.
	 */
	uint32_t rd_p;

	uint16_t mbuf_size;

	/**
	 * Next two fields are used for giving more free descriptors
	 * to the NFP.
	 */
	uint16_t rx_free_thresh;
	uint16_t nb_rx_hold;

	/** Referencing dev->data->port_id */
	uint16_t port_id;

	/** The queue index from Linux's perspective */
	uint16_t qidx;

	/**
	 * At this point 62 bytes have been used for all the fields in the
	 * RX critical path. We have room for 2 bytes and still all placed
	 * in a cache line.
	 */

	/** The size of the queue in number of descriptors */
	uint16_t rx_count;

	/**
	 * The index of the QCP queue relative to the RX queue BAR
	 * used for the freelist.
	 */
	uint32_t fl_qcidx;

	/** DMA address of the queue */
	uint64_t dma;
};

static inline void
nfp_net_mbuf_alloc_failed(struct nfp_net_rxq *rxq)
{
	rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
}

void nfp_net_rx_cksum(struct nfp_net_rxq *rxq, struct nfp_net_rx_desc *rxd,
		struct rte_mbuf *mb);
int nfp_net_rx_freelist_setup(struct rte_eth_dev *dev);
uint32_t nfp_net_rx_queue_count(void *rx_queue);
uint16_t nfp_net_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);
void nfp_net_rx_queue_release(struct rte_eth_dev *dev, uint16_t queue_idx);
void nfp_net_reset_rx_queue(struct nfp_net_rxq *rxq);
int nfp_net_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		uint16_t nb_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mp);
void nfp_net_tx_queue_release(struct rte_eth_dev *dev, uint16_t queue_idx);
void nfp_net_reset_tx_queue(struct nfp_net_txq *txq);

int nfp_net_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t queue_idx,
		uint16_t nb_desc,
		unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);
uint32_t nfp_net_tx_free_bufs(struct nfp_net_txq *txq);
void nfp_net_rx_queue_info_get(struct rte_eth_dev *dev,
		uint16_t queue_id,
		struct rte_eth_rxq_info *qinfo);
void nfp_net_tx_queue_info_get(struct rte_eth_dev *dev,
		uint16_t queue_id,
		struct rte_eth_txq_info *qinfo);
void nfp_net_recv_pkts_set(struct rte_eth_dev *eth_dev);
void nfp_net_parse_ptype(struct nfp_net_rxq *rxq,
		struct nfp_net_rx_desc *rxds,
		struct rte_mbuf *mb);

#endif /* __NFP_RXTX_H__ */
