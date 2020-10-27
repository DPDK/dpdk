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

/* @txgbe_rx_desc.dw0 */
#define TXGBE_RXD_RSSTYPE(dw)      RS(dw, 0, 0xF)
#define   TXGBE_RSSTYPE_NONE       0
#define   TXGBE_RSSTYPE_IPV4TCP    1
#define   TXGBE_RSSTYPE_IPV4       2
#define   TXGBE_RSSTYPE_IPV6TCP    3
#define   TXGBE_RSSTYPE_IPV4SCTP   4
#define   TXGBE_RSSTYPE_IPV6       5
#define   TXGBE_RSSTYPE_IPV6SCTP   6
#define   TXGBE_RSSTYPE_IPV4UDP    7
#define   TXGBE_RSSTYPE_IPV6UDP    8
#define   TXGBE_RSSTYPE_FDIR       15
#define TXGBE_RXD_SECTYPE(dw)      RS(dw, 4, 0x3)
#define TXGBE_RXD_SECTYPE_NONE     LS(0, 4, 0x3)
#define TXGBE_RXD_SECTYPE_LINKSEC  LS(1, 4, 0x3)
#define TXGBE_RXD_SECTYPE_IPSECESP LS(2, 4, 0x3)
#define TXGBE_RXD_SECTYPE_IPSECAH  LS(3, 4, 0x3)
#define TXGBE_RXD_TPIDSEL(dw)      RS(dw, 6, 0x7)
#define TXGBE_RXD_PTID(dw)         RS(dw, 9, 0xFF)
#define TXGBE_RXD_RSCCNT(dw)       RS(dw, 17, 0xF)
#define TXGBE_RXD_HDRLEN(dw)       RS(dw, 21, 0x3FF)
#define TXGBE_RXD_SPH              MS(31, 0x1)

/* @txgbe_rx_desc.dw1 */
/** bit 0-31, as rss hash when  **/
#define TXGBE_RXD_RSSHASH(rxd)     ((rxd)->qw0.dw1)

/** bit 0-31, as ip csum when  **/
#define TXGBE_RXD_IPID(rxd)        ((rxd)->qw0.hi.ipid)
#define TXGBE_RXD_CSUM(rxd)        ((rxd)->qw0.hi.csum)

/** bit 0-31, as fdir id when  **/
#define TXGBE_RXD_FDIRID(rxd)      ((rxd)->qw0.hi.dw1)

/* @txgbe_rx_desc.dw2 */
#define TXGBE_RXD_STATUS(rxd)      ((rxd)->qw1.lo.status)
/** bit 0-1 **/
#define TXGBE_RXD_STAT_DD          MS(0, 0x1) /* Descriptor Done */
#define TXGBE_RXD_STAT_EOP         MS(1, 0x1) /* End of Packet */
/** bit 2-31, when EOP=0 **/
#define TXGBE_RXD_NEXTP_RESV(v)    LS(v, 2, 0x3)
#define TXGBE_RXD_NEXTP(dw)        RS(dw, 4, 0xFFFF) /* Next Descriptor */
/** bit 2-31, when EOP=1 **/
#define TXGBE_RXD_PKT_CLS_MASK     MS(2, 0x7) /* Packet Class */
#define TXGBE_RXD_PKT_CLS_TC_RSS   LS(0, 2, 0x7) /* RSS Hash */
#define TXGBE_RXD_PKT_CLS_FLM      LS(1, 2, 0x7) /* FDir Match */
#define TXGBE_RXD_PKT_CLS_SYN      LS(2, 2, 0x7) /* TCP Sync */
#define TXGBE_RXD_PKT_CLS_5TUPLE   LS(3, 2, 0x7) /* 5 Tuple */
#define TXGBE_RXD_PKT_CLS_ETF      LS(4, 2, 0x7) /* Ethertype Filter */
#define TXGBE_RXD_STAT_VLAN        MS(5, 0x1) /* IEEE VLAN Packet */
#define TXGBE_RXD_STAT_UDPCS       MS(6, 0x1) /* UDP xsum calculated */
#define TXGBE_RXD_STAT_L4CS        MS(7, 0x1) /* L4 xsum calculated */
#define TXGBE_RXD_STAT_IPCS        MS(8, 0x1) /* IP xsum calculated */
#define TXGBE_RXD_STAT_PIF         MS(9, 0x1) /* Non-unicast address */
#define TXGBE_RXD_STAT_EIPCS       MS(10, 0x1) /* Encap IP xsum calculated */
#define TXGBE_RXD_STAT_VEXT        MS(11, 0x1) /* Multi-VLAN */
#define TXGBE_RXD_STAT_IPV6EX      MS(12, 0x1) /* IPv6 with option header */
#define TXGBE_RXD_STAT_LLINT       MS(13, 0x1) /* Pkt caused LLI */
#define TXGBE_RXD_STAT_1588        MS(14, 0x1) /* IEEE1588 Time Stamp */
#define TXGBE_RXD_STAT_SECP        MS(15, 0x1) /* Security Processing */
#define TXGBE_RXD_STAT_LB          MS(16, 0x1) /* Loopback Status */
/*** bit 17-30, when PTYPE=IP ***/
#define TXGBE_RXD_STAT_BMC         MS(17, 0x1) /* PTYPE=IP, BMC status */
#define TXGBE_RXD_ERR_FDIR_LEN     MS(20, 0x1) /* FDIR Length error */
#define TXGBE_RXD_ERR_FDIR_DROP    MS(21, 0x1) /* FDIR Drop error */
#define TXGBE_RXD_ERR_FDIR_COLL    MS(22, 0x1) /* FDIR Collision error */
#define TXGBE_RXD_ERR_HBO          MS(23, 0x1) /* Header Buffer Overflow */
#define TXGBE_RXD_ERR_EIPCS        MS(26, 0x1) /* Encap IP header error */
#define TXGBE_RXD_ERR_SECERR       MS(27, 0x1) /* macsec or ipsec error */
#define TXGBE_RXD_ERR_RXE          MS(29, 0x1) /* Any MAC Error */
#define TXGBE_RXD_ERR_L4CS         MS(30, 0x1) /* TCP/UDP xsum error */
#define TXGBE_RXD_ERR_IPCS         MS(31, 0x1) /* IP xsum error */
#define TXGBE_RXD_ERR_CSUM(dw)     RS(dw, 30, 0x3)
/*** bit 17-30, when PTYPE=FCOE ***/
#define TXGBE_RXD_STAT_FCOEFS      MS(17, 0x1) /* PTYPE=FCOE, FCoE EOF/SOF */
#define TXGBE_RXD_FCSTAT_MASK      MS(18, 0x3) /* FCoE Pkt Stat */
#define TXGBE_RXD_FCSTAT_NOMTCH    LS(0, 18, 0x3) /* No Ctxt Match */
#define TXGBE_RXD_FCSTAT_NODDP     LS(1, 18, 0x3) /* Ctxt w/o DDP */
#define TXGBE_RXD_FCSTAT_FCPRSP    LS(2, 18, 0x3) /* Recv. FCP_RSP */
#define TXGBE_RXD_FCSTAT_DDP       LS(3, 18, 0x3) /* Ctxt w/ DDP */
#define TXGBE_RXD_FCERR_MASK       MS(20, 0x7) /* FCERR */
#define TXGBE_RXD_FCERR_0          LS(0, 20, 0x7)
#define TXGBE_RXD_FCERR_1          LS(1, 20, 0x7)
#define TXGBE_RXD_FCERR_2          LS(2, 20, 0x7)
#define TXGBE_RXD_FCERR_3          LS(3, 20, 0x7)
#define TXGBE_RXD_FCERR_4          LS(4, 20, 0x7)
#define TXGBE_RXD_FCERR_5          LS(5, 20, 0x7)
#define TXGBE_RXD_FCERR_6          LS(6, 20, 0x7)
#define TXGBE_RXD_FCERR_7          LS(7, 20, 0x7)

/* @txgbe_rx_desc.dw3 */
#define TXGBE_RXD_LENGTH(rxd)           ((rxd)->qw1.hi.len)
#define TXGBE_RXD_VLAN(rxd)             ((rxd)->qw1.hi.tag)

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

#define rte_packet_prefetch(p)  rte_prefetch1(p)

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
	/** flags to set in mbuf when a vlan is detected. */
	uint64_t            vlan_flags;
	uint64_t	    offloads; /**< Rx offloads with DEV_RX_OFFLOAD_* */
	/** need to alloc dummy mbuf, for wraparound when scanning hw ring */
	struct rte_mbuf fake_mbuf;
	/** hold packets to return to application */
	struct rte_mbuf *rx_stage[RTE_PMD_TXGBE_RX_MAX_BURST * 2];
};

/**
 * TXGBE CTX Constants
 */
enum txgbe_ctx_num {
	TXGBE_CTX_0    = 0, /**< CTX0 */
	TXGBE_CTX_1    = 1, /**< CTX1  */
	TXGBE_CTX_NUM  = 2, /**< CTX NUMBER  */
};

/** Offload features */
union txgbe_tx_offload {
	uint64_t data[2];
	struct {
		uint64_t ptid:8; /**< Packet Type Identifier. */
		uint64_t l2_len:7; /**< L2 (MAC) Header Length. */
		uint64_t l3_len:9; /**< L3 (IP) Header Length. */
		uint64_t l4_len:8; /**< L4 (TCP/UDP) Header Length. */
		uint64_t tso_segsz:16; /**< TCP TSO segment size */
		uint64_t vlan_tci:16;
		/**< VLAN Tag Control Identifier (CPU order). */

		/* fields for TX offloading of tunnels */
		uint64_t outer_tun_len:8; /**< Outer TUN (Tunnel) Hdr Length. */
		uint64_t outer_l2_len:8; /**< Outer L2 (MAC) Hdr Length. */
		uint64_t outer_l3_len:16; /**< Outer L3 (IP) Hdr Length. */
	};
};

/**
 * Structure to check if new context need be built
 */
struct txgbe_ctx_info {
	uint64_t flags;           /**< ol_flags for context build. */
	/**< tx offload: vlan, tso, l2-l3-l4 lengths. */
	union txgbe_tx_offload tx_offload;
	/** compare mask for tx offload. */
	union txgbe_tx_offload tx_offload_mask;
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
	/** Index to last TX descriptor to have been cleaned. */
	uint16_t            last_desc_cleaned;
	/** Total number of TX descriptors ready to be allocated. */
	uint16_t            nb_tx_free;
	uint16_t            tx_next_dd;    /**< next desc to scan for DD bit */
	uint16_t            queue_id;      /**< TX queue index. */
	uint16_t            reg_idx;       /**< TX queue register index. */
	uint16_t            port_id;       /**< Device port identifier. */
	uint8_t             pthresh;       /**< Prefetch threshold register. */
	uint8_t             hthresh;       /**< Host threshold register. */
	uint8_t             wthresh;       /**< Write-back threshold reg. */
	uint64_t            offloads; /* Tx offload flags of DEV_TX_OFFLOAD_* */
	uint32_t            ctx_curr;      /**< Hardware context states. */
	/** Hardware context0 history. */
	struct txgbe_ctx_info ctx_cache[TXGBE_CTX_NUM];
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
int txgbe_dev_tx_done_cleanup(void *tx_queue, uint32_t free_cnt);

uint64_t txgbe_get_tx_port_offloads(struct rte_eth_dev *dev);
uint64_t txgbe_get_rx_queue_offloads(struct rte_eth_dev *dev);
uint64_t txgbe_get_rx_port_offloads(struct rte_eth_dev *dev);
uint64_t txgbe_get_tx_queue_offloads(struct rte_eth_dev *dev);

#endif /* _TXGBE_RXTX_H_ */
