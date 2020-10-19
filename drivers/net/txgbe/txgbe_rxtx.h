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

/**
 * Transmit Data Descriptor (TXGBE_TXD_TYP=DATA)
 **/
struct txgbe_tx_desc {
	__le64 qw0; /* r.buffer_addr ,  w.reserved    */
	__le32 dw2; /* r.cmd_type_len,  w.nxtseq_seed */
	__le32 dw3; /* r.olinfo_status, w.status      */
};

#define RTE_PMD_TXGBE_TX_MAX_BURST 32
#define RTE_PMD_TXGBE_RX_MAX_BURST 32

#define RX_RING_SZ ((TXGBE_RING_DESC_MAX + RTE_PMD_TXGBE_RX_MAX_BURST) * \
		    sizeof(struct txgbe_rx_desc))

#define TXGBE_PTID_MASK                 0xFF

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
	/**< Start freeing TX buffers if there are less free descriptors than
	 *   this value.
	 */
	uint16_t            tx_free_thresh;
	uint16_t            queue_id;      /**< TX queue index. */
	uint16_t            reg_idx;       /**< TX queue register index. */
	uint16_t            port_id;       /**< Device port identifier. */
	uint8_t             pthresh;       /**< Prefetch threshold register. */
	uint8_t             hthresh;       /**< Host threshold register. */
	uint8_t             wthresh;       /**< Write-back threshold reg. */
	uint64_t offloads; /**< Tx offload flags of DEV_TX_OFFLOAD_* */
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
