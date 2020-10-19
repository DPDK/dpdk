/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_RXTX_H_
#define _TXGBE_RXTX_H_

/*****************************************************************************
 * Receive Descriptor
 *****************************************************************************/
struct txgbe_rx_desc {
	struct {
		union {
			__le32 dw0;
			struct {
				__le16 pkt;
				__le16 hdr;
			} lo;
		};
		union {
			__le32 dw1;
			struct {
				__le16 ipid;
				__le16 csum;
			} hi;
		};
	} qw0; /* also as r.pkt_addr */
	struct {
		union {
			__le32 dw2;
			struct {
				__le32 status;
			} lo;
		};
		union {
			__le32 dw3;
			struct {
				__le16 len;
				__le16 tag;
			} hi;
		};
	} qw1; /* also as r.hdr_addr */
};

/* @txgbe_rx_desc.qw0 */
#define TXGBE_RXD_PKTADDR(rxd, v)  \
	(((volatile __le64 *)(rxd))[0] = cpu_to_le64(v))

/* @txgbe_rx_desc.qw1 */
#define TXGBE_RXD_HDRADDR(rxd, v)  \
	(((volatile __le64 *)(rxd))[1] = cpu_to_le64(v))

/*****************************************************************************
 * Transmit Descriptor
 *****************************************************************************/
/**
 * Transmit Context Descriptor (TXGBE_TXD_TYP=CTXT)
 **/
struct txgbe_tx_ctx_desc {
	__le32 dw0; /* w.vlan_macip_lens  */
	__le32 dw1; /* w.seqnum_seed      */
	__le32 dw2; /* w.type_tucmd_mlhl  */
	__le32 dw3; /* w.mss_l4len_idx    */
};

/* @txgbe_tx_ctx_desc.dw0 */
#define TXGBE_TXD_IPLEN(v)         LS(v, 0, 0x1FF) /* ip/fcoe header end */
#define TXGBE_TXD_MACLEN(v)        LS(v, 9, 0x7F) /* desc mac len */
#define TXGBE_TXD_VLAN(v)          LS(v, 16, 0xFFFF) /* vlan tag */

/* @txgbe_tx_ctx_desc.dw1 */
/*** bit 0-31, when TXGBE_TXD_DTYP_FCOE=0 ***/
#define TXGBE_TXD_IPSEC_SAIDX(v)   LS(v, 0, 0x3FF) /* ipsec SA index */
#define TXGBE_TXD_ETYPE(v)         LS(v, 11, 0x1) /* tunnel type */
#define TXGBE_TXD_ETYPE_UDP        LS(0, 11, 0x1)
#define TXGBE_TXD_ETYPE_GRE        LS(1, 11, 0x1)
#define TXGBE_TXD_EIPLEN(v)        LS(v, 12, 0x7F) /* tunnel ip header */
#define TXGBE_TXD_DTYP_FCOE        MS(16, 0x1) /* FCoE/IP descriptor */
#define TXGBE_TXD_ETUNLEN(v)       LS(v, 21, 0xFF) /* tunnel header */
#define TXGBE_TXD_DECTTL(v)        LS(v, 29, 0xF) /* decrease ip TTL */
/*** bit 0-31, when TXGBE_TXD_DTYP_FCOE=1 ***/
#define TXGBE_TXD_FCOEF_EOF_MASK   MS(10, 0x3) /* FC EOF index */
#define TXGBE_TXD_FCOEF_EOF_N      LS(0, 10, 0x3) /* EOFn */
#define TXGBE_TXD_FCOEF_EOF_T      LS(1, 10, 0x3) /* EOFt */
#define TXGBE_TXD_FCOEF_EOF_NI     LS(2, 10, 0x3) /* EOFni */
#define TXGBE_TXD_FCOEF_EOF_A      LS(3, 10, 0x3) /* EOFa */
#define TXGBE_TXD_FCOEF_SOF        MS(12, 0x1) /* FC SOF index */
#define TXGBE_TXD_FCOEF_PARINC     MS(13, 0x1) /* Rel_Off in F_CTL */
#define TXGBE_TXD_FCOEF_ORIE       MS(14, 0x1) /* orientation end */
#define TXGBE_TXD_FCOEF_ORIS       MS(15, 0x1) /* orientation start */

/* @txgbe_tx_ctx_desc.dw2 */
#define TXGBE_TXD_IPSEC_ESPLEN(v)  LS(v, 1, 0x1FF) /* ipsec ESP length */
#define TXGBE_TXD_SNAP             MS(10, 0x1) /* SNAP indication */
#define TXGBE_TXD_TPID_SEL(v)      LS(v, 11, 0x7) /* vlan tag index */
#define TXGBE_TXD_IPSEC_ESP        MS(14, 0x1) /* ipsec type: esp=1 ah=0 */
#define TXGBE_TXD_IPSEC_ESPENC     MS(15, 0x1) /* ESP encrypt */
#define TXGBE_TXD_CTXT             MS(20, 0x1) /* context descriptor */
#define TXGBE_TXD_PTID(v)          LS(v, 24, 0xFF) /* packet type */
/* @txgbe_tx_ctx_desc.dw3 */
#define TXGBE_TXD_DD               MS(0, 0x1) /* descriptor done */
#define TXGBE_TXD_IDX(v)           LS(v, 4, 0x1) /* ctxt desc index */
#define TXGBE_TXD_L4LEN(v)         LS(v, 8, 0xFF) /* l4 header length */
#define TXGBE_TXD_MSS(v)           LS(v, 16, 0xFFFF) /* l4 MSS */

/**
 * Transmit Data Descriptor (TXGBE_TXD_TYP=DATA)
 **/
struct txgbe_tx_desc {
	__le64 qw0; /* r.buffer_addr ,  w.reserved    */
	__le32 dw2; /* r.cmd_type_len,  w.nxtseq_seed */
	__le32 dw3; /* r.olinfo_status, w.status      */
};
/* @txgbe_tx_desc.qw0 */

/* @txgbe_tx_desc.dw2 */
#define TXGBE_TXD_DATLEN(v)        ((0xFFFF & (v))) /* data buffer length */
#define TXGBE_TXD_1588             ((0x1) << 19) /* IEEE1588 time stamp */
#define TXGBE_TXD_DATA             ((0x0) << 20) /* data descriptor */
#define TXGBE_TXD_EOP              ((0x1) << 24) /* End of Packet */
#define TXGBE_TXD_FCS              ((0x1) << 25) /* Insert FCS */
#define TXGBE_TXD_LINKSEC          ((0x1) << 26) /* Insert LinkSec */
#define TXGBE_TXD_ECU              ((0x1) << 28) /* forward to ECU */
#define TXGBE_TXD_CNTAG            ((0x1) << 29) /* insert CN tag */
#define TXGBE_TXD_VLE              ((0x1) << 30) /* insert VLAN tag */
#define TXGBE_TXD_TSE              ((0x1) << 31) /* transmit segmentation */

#define TXGBE_TXD_FLAGS (TXGBE_TXD_FCS | TXGBE_TXD_EOP)

/* @txgbe_tx_desc.dw3 */
#define TXGBE_TXD_DD_UNUSED        TXGBE_TXD_DD
#define TXGBE_TXD_IDX_UNUSED(v)    TXGBE_TXD_IDX(v)
#define TXGBE_TXD_CC               ((0x1) << 7) /* check context */
#define TXGBE_TXD_IPSEC            ((0x1) << 8) /* request ipsec offload */
#define TXGBE_TXD_L4CS             ((0x1) << 9) /* insert TCP/UDP/SCTP csum */
#define TXGBE_TXD_IPCS             ((0x1) << 10) /* insert IPv4 csum */
#define TXGBE_TXD_EIPCS            ((0x1) << 11) /* insert outer IP csum */
#define TXGBE_TXD_MNGFLT           ((0x1) << 12) /* enable management filter */
#define TXGBE_TXD_PAYLEN(v)        ((0x7FFFF & (v)) << 13) /* payload length */

#define RTE_PMD_TXGBE_TX_MAX_BURST 32
#define RTE_PMD_TXGBE_RX_MAX_BURST 32
#define RTE_TXGBE_TX_MAX_FREE_BUF_SZ 64

#define RX_RING_SZ ((TXGBE_RING_DESC_MAX + RTE_PMD_TXGBE_RX_MAX_BURST) * \
		    sizeof(struct txgbe_rx_desc))

#define RTE_TXGBE_REGISTER_POLL_WAIT_10_MS  10
#define RTE_TXGBE_WAIT_100_US               100

#define TXGBE_TX_MAX_SEG                    40

/**
 * Structure associated with each descriptor of the RX ring of a RX queue.
 */
struct txgbe_rx_entry {
	struct rte_mbuf *mbuf; /**< mbuf associated with RX descriptor. */
};

struct txgbe_scattered_rx_entry {
	struct rte_mbuf *fbuf; /**< First segment of the fragmented packet. */
};

/**
 * Structure associated with each descriptor of the TX ring of a TX queue.
 */
struct txgbe_tx_entry {
	struct rte_mbuf *mbuf; /**< mbuf associated with TX desc, if any. */
	uint16_t next_id; /**< Index of next descriptor in ring. */
	uint16_t last_id; /**< Index of last scattered descriptor. */
};

/**
 * Structure associated with each descriptor of the TX ring of a TX queue.
 */
struct txgbe_tx_entry_v {
	struct rte_mbuf *mbuf; /**< mbuf associated with TX desc, if any. */
};

/**
 * Structure associated with each RX queue.
 */
struct txgbe_rx_queue {
	struct rte_mempool  *mb_pool; /**< mbuf pool to populate RX ring. */
	volatile struct txgbe_rx_desc *rx_ring; /**< RX ring virtual address. */
	uint64_t            rx_ring_phys_addr; /**< RX ring DMA address. */
	volatile uint32_t   *rdt_reg_addr; /**< RDT register address. */
	volatile uint32_t   *rdh_reg_addr; /**< RDH register address. */
	struct txgbe_rx_entry *sw_ring; /**< address of RX software ring. */
	/**< address of scattered Rx software ring. */
	struct txgbe_scattered_rx_entry *sw_sc_ring;
	struct rte_mbuf *pkt_first_seg; /**< First segment of current packet. */
	struct rte_mbuf *pkt_last_seg; /**< Last segment of current packet. */
	uint16_t            nb_rx_desc; /**< number of RX descriptors. */
	uint16_t            rx_tail;  /**< current value of RDT register. */
	uint16_t            nb_rx_hold; /**< number of held free RX desc. */
	uint16_t rx_nb_avail; /**< nr of staged pkts ready to ret to app */
	uint16_t rx_next_avail; /**< idx of next staged pkt to ret to app */
	uint16_t rx_free_trigger; /**< triggers rx buffer allocation */
	uint16_t            rx_free_thresh; /**< max free RX desc to hold. */
	uint16_t            queue_id; /**< RX queue index. */
	uint16_t            reg_idx;  /**< RX queue register index. */
	/**< Packet type mask for different NICs. */
	uint16_t            pkt_type_mask;
	uint16_t            port_id;  /**< Device port identifier. */
	uint8_t             crc_len;  /**< 0 if CRC stripped, 4 otherwise. */
	uint8_t             drop_en;  /**< If not 0, set SRRCTL.Drop_En. */
	uint8_t             rx_deferred_start; /**< not in global dev start. */
	uint64_t	    offloads; /**< Rx offloads with DEV_RX_OFFLOAD_* */
	/** need to alloc dummy mbuf, for wraparound when scanning hw ring */
	struct rte_mbuf fake_mbuf;
	/** hold packets to return to application */
	struct rte_mbuf *rx_stage[RTE_PMD_TXGBE_RX_MAX_BURST * 2];
};

/**
 * Structure associated with each TX queue.
 */
struct txgbe_tx_queue {
	/** TX ring virtual address. */
	volatile struct txgbe_tx_desc *tx_ring;
	uint64_t            tx_ring_phys_addr; /**< TX ring DMA address. */
	union {
		/**< address of SW ring for scalar PMD. */
		struct txgbe_tx_entry *sw_ring;
		/**< address of SW ring for vector PMD */
		struct txgbe_tx_entry_v *sw_ring_v;
	};
	volatile uint32_t   *tdt_reg_addr; /**< Address of TDT register. */
	volatile uint32_t   *tdc_reg_addr; /**< Address of TDC register. */
	uint16_t            nb_tx_desc;    /**< number of TX descriptors. */
	uint16_t            tx_tail;       /**< current value of TDT reg. */
	/**< Start freeing TX buffers if there are less free descriptors than
	 *   this value.
	 */
	uint16_t            tx_free_thresh;
	uint16_t            nb_tx_free;
	uint16_t            tx_next_dd;    /**< next desc to scan for DD bit */
	uint16_t            queue_id;      /**< TX queue index. */
	uint16_t            reg_idx;       /**< TX queue register index. */
	uint16_t            port_id;       /**< Device port identifier. */
	uint8_t             pthresh;       /**< Prefetch threshold register. */
	uint8_t             hthresh;       /**< Host threshold register. */
	uint8_t             wthresh;       /**< Write-back threshold reg. */
	uint64_t            offloads; /* Tx offload flags of DEV_TX_OFFLOAD_* */
	const struct txgbe_txq_ops *ops;       /**< txq ops */
	uint8_t             tx_deferred_start; /**< not in global dev start. */
};

struct txgbe_txq_ops {
	void (*release_mbufs)(struct txgbe_tx_queue *txq);
	void (*free_swring)(struct txgbe_tx_queue *txq);
	void (*reset)(struct txgbe_tx_queue *txq);
};

/* Takes an ethdev and a queue and sets up the tx function to be used based on
 * the queue parameters. Used in tx_queue_setup by primary process and then
 * in dev_init by secondary process when attaching to an existing ethdev.
 */
void txgbe_set_tx_function(struct rte_eth_dev *dev, struct txgbe_tx_queue *txq);

void txgbe_set_rx_function(struct rte_eth_dev *dev);

uint64_t txgbe_get_tx_port_offloads(struct rte_eth_dev *dev);
uint64_t txgbe_get_rx_queue_offloads(struct rte_eth_dev *dev);
uint64_t txgbe_get_rx_port_offloads(struct rte_eth_dev *dev);
uint64_t txgbe_get_tx_queue_offloads(struct rte_eth_dev *dev);

#endif /* _TXGBE_RXTX_H_ */
