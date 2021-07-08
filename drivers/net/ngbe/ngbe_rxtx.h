/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_RXTX_H_
#define _NGBE_RXTX_H_

/*****************************************************************************
 * Receive Descriptor
 *****************************************************************************/
struct ngbe_rx_desc {
	struct {
		union {
			rte_le32_t dw0;
			struct {
				rte_le16_t pkt;
				rte_le16_t hdr;
			} lo;
		};
		union {
			rte_le32_t dw1;
			struct {
				rte_le16_t ipid;
				rte_le16_t csum;
			} hi;
		};
	} qw0; /* also as r.pkt_addr */
	struct {
		union {
			rte_le32_t dw2;
			struct {
				rte_le32_t status;
			} lo;
		};
		union {
			rte_le32_t dw3;
			struct {
				rte_le16_t len;
				rte_le16_t tag;
			} hi;
		};
	} qw1; /* also as r.hdr_addr */
};

/*****************************************************************************
 * Transmit Descriptor
 *****************************************************************************/
/**
 * Transmit Context Descriptor (NGBE_TXD_TYP=CTXT)
 **/
struct ngbe_tx_ctx_desc {
	rte_le32_t dw0; /* w.vlan_macip_lens  */
	rte_le32_t dw1; /* w.seqnum_seed      */
	rte_le32_t dw2; /* w.type_tucmd_mlhl  */
	rte_le32_t dw3; /* w.mss_l4len_idx    */
};

/* @ngbe_tx_ctx_desc.dw3 */
#define NGBE_TXD_DD               MS(0, 0x1) /* descriptor done */

/**
 * Transmit Data Descriptor (NGBE_TXD_TYP=DATA)
 **/
struct ngbe_tx_desc {
	rte_le64_t qw0; /* r.buffer_addr ,  w.reserved    */
	rte_le32_t dw2; /* r.cmd_type_len,  w.nxtseq_seed */
	rte_le32_t dw3; /* r.olinfo_status, w.status      */
};

#define RTE_PMD_NGBE_RX_MAX_BURST 32

#define RX_RING_SZ ((NGBE_RING_DESC_MAX + RTE_PMD_NGBE_RX_MAX_BURST) * \
		    sizeof(struct ngbe_rx_desc))

#define RTE_NGBE_REGISTER_POLL_WAIT_10_MS  10
#define RTE_NGBE_WAIT_100_US               100

#define NGBE_TX_MAX_SEG                    40

#ifndef DEFAULT_TX_FREE_THRESH
#define DEFAULT_TX_FREE_THRESH 32
#endif

/**
 * Structure associated with each descriptor of the Rx ring of a Rx queue.
 */
struct ngbe_rx_entry {
	struct rte_mbuf *mbuf; /**< mbuf associated with Rx descriptor. */
};

struct ngbe_scattered_rx_entry {
	struct rte_mbuf *fbuf; /**< First segment of the fragmented packet. */
};

/**
 * Structure associated with each descriptor of the Tx ring of a Tx queue.
 */
struct ngbe_tx_entry {
	struct rte_mbuf *mbuf; /**< mbuf associated with Tx desc, if any. */
	uint16_t next_id; /**< Index of next descriptor in ring. */
	uint16_t last_id; /**< Index of last scattered descriptor. */
};

/**
 * Structure associated with each Rx queue.
 */
struct ngbe_rx_queue {
	struct rte_mempool   *mb_pool; /**< mbuf pool to populate Rx ring */
	uint64_t             rx_ring_phys_addr; /**< Rx ring DMA address */
	volatile uint32_t    *rdt_reg_addr; /**< RDT register address */
	volatile uint32_t    *rdh_reg_addr; /**< RDH register address */

	volatile struct ngbe_rx_desc   *rx_ring; /**< Rx ring virtual address */
	/** address of Rx software ring */
	struct ngbe_rx_entry           *sw_ring;
	/** address of scattered Rx software ring */
	struct ngbe_scattered_rx_entry *sw_sc_ring;

	struct rte_mbuf *pkt_first_seg; /**< First segment of current packet */
	struct rte_mbuf *pkt_last_seg; /**< Last segment of current packet */
	uint16_t        nb_rx_desc; /**< number of Rx descriptors */
	uint16_t        rx_tail;  /**< current value of RDT register */
	uint16_t        nb_rx_hold; /**< number of held free Rx desc */

	uint16_t rx_nb_avail; /**< nr of staged pkts ready to ret to app */
	uint16_t rx_next_avail; /**< idx of next staged pkt to ret to app */
	uint16_t rx_free_trigger; /**< triggers rx buffer allocation */

	uint16_t        rx_free_thresh; /**< max free Rx desc to hold */
	uint16_t        queue_id; /**< RX queue index */
	uint16_t        reg_idx;  /**< RX queue register index */
	uint16_t        port_id;  /**< Device port identifier */
	uint8_t         drop_en;  /**< If not 0, set SRRCTL.Drop_En */
	uint8_t         rx_deferred_start; /**< not in global dev start */
	/** need to alloc dummy mbuf, for wraparound when scanning hw ring */
	struct rte_mbuf fake_mbuf;
	/** hold packets to return to application */
	struct rte_mbuf *rx_stage[RTE_PMD_NGBE_RX_MAX_BURST * 2];
};

/**
 * NGBE CTX Constants
 */
enum ngbe_ctx_num {
	NGBE_CTX_0    = 0, /**< CTX0 */
	NGBE_CTX_1    = 1, /**< CTX1  */
	NGBE_CTX_NUM  = 2, /**< CTX NUMBER  */
};

/**
 * Structure to check if new context need be built
 */
struct ngbe_ctx_info {
	uint64_t flags;           /**< ol_flags for context build. */
};

/**
 * Structure associated with each Tx queue.
 */
struct ngbe_tx_queue {
	/** Tx ring virtual address */
	volatile struct ngbe_tx_desc *tx_ring;

	uint64_t             tx_ring_phys_addr; /**< Tx ring DMA address */
	struct ngbe_tx_entry *sw_ring; /**< address of SW ring for scalar PMD */
	volatile uint32_t    *tdt_reg_addr; /**< Address of TDT register */
	volatile uint32_t    *tdc_reg_addr; /**< Address of TDC register */
	uint16_t             nb_tx_desc;    /**< number of Tx descriptors */
	uint16_t             tx_tail;       /**< current value of TDT reg */
	/**
	 * Start freeing Tx buffers if there are less free descriptors than
	 * this value.
	 */
	uint16_t             tx_free_thresh;
	/** Index to last Tx descriptor to have been cleaned */
	uint16_t             last_desc_cleaned;
	/** Total number of Tx descriptors ready to be allocated */
	uint16_t             nb_tx_free;
	uint16_t             tx_next_dd;    /**< next desc to scan for DD bit */
	uint16_t             queue_id;      /**< Tx queue index */
	uint16_t             reg_idx;       /**< Tx queue register index */
	uint16_t             port_id;       /**< Device port identifier */
	uint8_t              pthresh;       /**< Prefetch threshold register */
	uint8_t              hthresh;       /**< Host threshold register */
	uint8_t              wthresh;       /**< Write-back threshold reg */
	uint32_t             ctx_curr;      /**< Hardware context states */
	/** Hardware context0 history */
	struct ngbe_ctx_info ctx_cache[NGBE_CTX_NUM];
	uint8_t              tx_deferred_start; /**< not in global dev start */

	const struct ngbe_txq_ops *ops;       /**< txq ops */
};

struct ngbe_txq_ops {
	void (*release_mbufs)(struct ngbe_tx_queue *txq);
	void (*free_swring)(struct ngbe_tx_queue *txq);
	void (*reset)(struct ngbe_tx_queue *txq);
};

#endif /* _NGBE_RXTX_H_ */
