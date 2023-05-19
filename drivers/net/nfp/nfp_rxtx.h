/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2021 Netronome Systems, Inc.
 * All rights reserved.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_rxtx.h
 *
 * Netronome NFP Rx/Tx specific header file
 */

#ifndef _NFP_RXTX_H_
#define _NFP_RXTX_H_

#include <linux/types.h>
#include <rte_io.h>

#define NFP_DESC_META_LEN(d) ((d)->rxd.meta_len_dd & PCIE_DESC_RX_META_LEN_MASK)

#define NFP_HASH_OFFSET      ((uint8_t *)mbuf->buf_addr + mbuf->data_off - 4)
#define NFP_HASH_TYPE_OFFSET ((uint8_t *)mbuf->buf_addr + mbuf->data_off - 8)

#define RTE_MBUF_DMA_ADDR_DEFAULT(mb) \
	((uint64_t)((mb)->buf_iova + RTE_PKTMBUF_HEADROOM))

/* Maximum number of supported VLANs in parsed form packet metadata. */
#define NFP_META_MAX_VLANS       2
/* Maximum number of NFP packet metadata fields. */
#define NFP_META_MAX_FIELDS      8

/*
 * struct nfp_net_meta_raw - Raw memory representation of packet metadata
 *
 * Describe the raw metadata format, useful when preparing metadata for a
 * transmission mbuf.
 *
 * @header: NFD3 or NFDk field type header (see format in nfp.rst)
 * @data: Array of each fields data member
 * @length: Keep track of number of valid fields in @header and data. Not part
 *          of the raw metadata.
 */
struct nfp_net_meta_raw {
	uint32_t header;
	uint32_t data[NFP_META_MAX_FIELDS];
	uint8_t length;
};

/*
 * struct nfp_meta_parsed - Record metadata parsed from packet
 *
 * Parsed NFP packet metadata are recorded in this struct. The content is
 * read-only after it have been recorded during parsing by nfp_net_parse_meta().
 *
 * @hash: RSS hash value
 * @hash_type: RSS hash type
 * @vlan_layer: The layers of VLAN info which are passed from nic.
 *              Only this number of entries of the @vlan array are valid.
 *
 * @vlan: Holds information parses from NFP_NET_META_VLAN. The inner most vlan
 *        starts at position 0 and only @vlan_layer entries contain valid
 *        information.
 *
 *        Currently only 2 layers of vlan are supported,
 *        vlan[0] - vlan strip info
 *        vlan[1] - qinq strip info
 *
 * @vlan.offload:  Flag indicates whether VLAN is offloaded
 * @vlan.tpid: Vlan TPID
 * @vlan.tci: Vlan TCI including PCP + Priority + VID
 */
struct nfp_meta_parsed {
	uint32_t hash;
	uint8_t hash_type;
	uint8_t vlan_layer;
	struct {
		uint8_t offload;
		uint8_t tpid;
		uint16_t tci;
	} vlan[NFP_META_MAX_VLANS];
};

/*
 * The maximum number of descriptors is limited by design as
 * DPDK uses uint16_t variables for these values
 */
#define NFP_NET_MAX_TX_DESC (32 * 1024)
#define NFP_NET_MIN_TX_DESC 256
#define NFP3800_NET_MIN_TX_DESC 512

#define NFP_NET_MAX_RX_DESC (32 * 1024)
#define NFP_NET_MIN_RX_DESC 256
#define NFP3800_NET_MIN_RX_DESC 512

/* Descriptor alignment */
#define NFP_ALIGN_RING_DESC 128

#define DIV_ROUND_UP(n, d)             (((n) + (d) - 1) / (d))

struct nfp_net_dp_buf {
	struct rte_mbuf *mbuf;
};

struct nfp_net_txq {
	struct nfp_net_hw *hw; /* Backpointer to nfp_net structure */

	/*
	 * Queue information: @qidx is the queue index from Linux's
	 * perspective.  @tx_qcidx is the index of the Queue
	 * Controller Peripheral queue relative to the TX queue BAR.
	 * @cnt is the size of the queue in number of
	 * descriptors. @qcp_q is a pointer to the base of the queue
	 * structure on the NFP
	 */
	uint8_t *qcp_q;

	/*
	 * Read and Write pointers.  @wr_p and @rd_p are host side pointer,
	 * they are free running and have little relation to the QCP pointers *
	 * @qcp_rd_p is a local copy queue controller peripheral read pointer
	 */

	uint32_t wr_p;
	uint32_t rd_p;

	uint32_t tx_count;

	uint32_t tx_free_thresh;

	/*
	 * For each descriptor keep a reference to the mbuf and
	 * DMA address used until completion is signalled.
	 */
	struct nfp_net_dp_buf *txbufs;

	/*
	 * Information about the host side queue location. @txds is
	 * the virtual address for the queue, @dma is the DMA address
	 * of the queue and @size is the size in bytes for the queue
	 * (needed for free)
	 */
	union {
		struct nfp_net_nfd3_tx_desc *txds;
		struct nfp_net_nfdk_tx_desc *ktxds;
	};

	/*
	 * At this point 48 bytes have been used for all the fields in the
	 * TX critical path. We have room for 8 bytes and still all placed
	 * in a cache line. We are not using the threshold values below but
	 * if we need to, we can add the most used in the remaining bytes.
	 */
	uint32_t tx_rs_thresh; /* not used by now. Future? */
	uint32_t tx_pthresh;   /* not used by now. Future? */
	uint32_t tx_hthresh;   /* not used by now. Future? */
	uint32_t tx_wthresh;   /* not used by now. Future? */
	uint16_t port_id;
	uint16_t data_pending; /* used by nfdk only */
	int qidx;
	int tx_qcidx;
	__le64 dma;
} __rte_aligned(64);

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
		/* Freelist descriptor */
		struct {
			__le16 dma_addr_hi;
			uint8_t spare;
			uint8_t dd;

			__le32 dma_addr_lo;
		} __rte_packed fld;

		/* RX descriptor */
		struct {
			__le16 data_len;
			uint8_t reserved;
			uint8_t meta_len_dd;

			__le16 flags;
			__le16 vlan;
		} __rte_packed rxd;

		__le32 vals[2];
	};
};

struct nfp_net_rxq {
	struct nfp_net_hw *hw;	/* Backpointer to nfp_net structure */

	 /*
	  * @qcp_fl and @qcp_rx are pointers to the base addresses of the
	  * freelist and RX queue controller peripheral queue structures on the
	  * NFP
	  */
	uint8_t *qcp_fl;
	uint8_t *qcp_rx;

	/*
	 * Read and Write pointers.  @wr_p and @rd_p are host side
	 * pointer, they are free running and have little relation to
	 * the QCP pointers. @wr_p is where the driver adds new
	 * freelist descriptors and @rd_p is where the driver start
	 * reading descriptors for newly arrive packets from.
	 */
	uint32_t rd_p;

	/*
	 * For each buffer placed on the freelist, record the
	 * associated SKB
	 */
	struct nfp_net_dp_buf *rxbufs;

	/*
	 * Information about the host side queue location.  @rxds is
	 * the virtual address for the queue
	 */
	struct nfp_net_rx_desc *rxds;

	/*
	 * The mempool is created by the user specifying a mbuf size.
	 * We save here the reference of the mempool needed in the RX
	 * path and the mbuf size for checking received packets can be
	 * safely copied to the mbuf using the NFP_NET_RX_OFFSET
	 */
	struct rte_mempool *mem_pool;
	uint16_t mbuf_size;

	/*
	 * Next two fields are used for giving more free descriptors
	 * to the NFP
	 */
	uint16_t rx_free_thresh;
	uint16_t nb_rx_hold;

	 /* the size of the queue in number of descriptors */
	uint16_t rx_count;

	/*
	 * Fields above this point fit in a single cache line and are all used
	 * in the RX critical path. Fields below this point are just used
	 * during queue configuration or not used at all (yet)
	 */

	/* referencing dev->data->port_id */
	uint16_t port_id;

	uint8_t  crc_len; /* Not used by now */
	uint8_t  drop_en; /* Not used by now */

	/* DMA address of the queue */
	__le64 dma;

	/*
	 * Queue information: @qidx is the queue index from Linux's
	 * perspective.  @fl_qcidx is the index of the Queue
	 * Controller peripheral queue relative to the RX queue BAR
	 * used for the freelist and @rx_qcidx is the Queue Controller
	 * Peripheral index for the RX queue.
	 */
	int qidx;
	int fl_qcidx;
	int rx_qcidx;
} __rte_aligned(64);

static inline void
nfp_net_mbuf_alloc_failed(struct nfp_net_rxq *rxq)
{
	rte_eth_devices[rxq->port_id].data->rx_mbuf_alloc_failed++;
}

/* set mbuf checksum flags based on RX descriptor flags */
static inline void
nfp_net_rx_cksum(struct nfp_net_rxq *rxq, struct nfp_net_rx_desc *rxd,
		 struct rte_mbuf *mb)
{
	struct nfp_net_hw *hw = rxq->hw;

	if (!(hw->ctrl & NFP_NET_CFG_CTRL_RXCSUM))
		return;

	/* If IPv4 and IP checksum error, fail */
	if (unlikely((rxd->rxd.flags & PCIE_DESC_RX_IP4_CSUM) &&
			!(rxd->rxd.flags & PCIE_DESC_RX_IP4_CSUM_OK)))
		mb->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_BAD;
	else
		mb->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_GOOD;

	/* If neither UDP nor TCP return */
	if (!(rxd->rxd.flags & PCIE_DESC_RX_TCP_CSUM) &&
			!(rxd->rxd.flags & PCIE_DESC_RX_UDP_CSUM))
		return;

	if (likely(rxd->rxd.flags & PCIE_DESC_RX_L4_CSUM_OK))
		mb->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
	else
		mb->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_BAD;
}

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
int nfp_net_tx_free_bufs(struct nfp_net_txq *txq);
void nfp_net_set_meta_vlan(struct nfp_net_meta_raw *meta_data,
		struct rte_mbuf *pkt,
		uint8_t layer);

#endif /* _NFP_RXTX_H_ */
/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */
