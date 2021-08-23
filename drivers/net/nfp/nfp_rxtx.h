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

/*
 * The maximum number of descriptors is limited by design as
 * DPDK uses uint16_t variables for these values
 */
#define NFP_NET_MAX_TX_DESC (32 * 1024)
#define NFP_NET_MIN_TX_DESC 256

#define NFP_NET_MAX_RX_DESC (32 * 1024)
#define NFP_NET_MIN_RX_DESC 256

/* Descriptor alignment */
#define NFP_ALIGN_RING_DESC 128

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
#define PCIE_DESC_TX_ENCAP_VXLAN        (1 << 1)
#define PCIE_DESC_TX_ENCAP_GRE          (1 << 0)

struct nfp_net_tx_desc {
	union {
		struct {
			uint8_t dma_addr_hi; /* High bits of host buf address */
			__le16 dma_len;     /* Length to DMA for this desc */
			uint8_t offset_eop; /* Offset in buf where pkt starts +
					     * highest bit is eop flag.
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
	struct {
		struct rte_mbuf *mbuf;
	} *txbufs;

	/*
	 * Information about the host side queue location. @txds is
	 * the virtual address for the queue, @dma is the DMA address
	 * of the queue and @size is the size in bytes for the queue
	 * (needed for free)
	 */
	struct nfp_net_tx_desc *txds;

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
			uint8_t dma_addr_hi;
			__le16 spare;
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

struct nfp_net_rx_buff {
	struct rte_mbuf *mbuf;
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
	struct nfp_net_rx_buff *rxbufs;

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

int nfp_net_rx_freelist_setup(struct rte_eth_dev *dev);
uint32_t nfp_net_rx_queue_count(struct rte_eth_dev *dev,
				       uint16_t queue_idx);
uint16_t nfp_net_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts);
void nfp_net_rx_queue_release(void *rxq);
void nfp_net_reset_rx_queue(struct nfp_net_rxq *rxq);
int nfp_net_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
				  uint16_t nb_desc, unsigned int socket_id,
				  const struct rte_eth_rxconf *rx_conf,
				  struct rte_mempool *mp);
void nfp_net_tx_queue_release(void *txq);
void nfp_net_reset_tx_queue(struct nfp_net_txq *txq);
int nfp_net_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
				  uint16_t nb_desc, unsigned int socket_id,
				  const struct rte_eth_txconf *tx_conf);
uint16_t nfp_net_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts);

#endif /* _NFP_RXTX_H_ */
/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */

