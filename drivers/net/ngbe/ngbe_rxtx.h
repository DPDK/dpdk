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

#define RTE_PMD_NGBE_RX_MAX_BURST 32

#define RX_RING_SZ ((NGBE_RING_DESC_MAX + RTE_PMD_NGBE_RX_MAX_BURST) * \
		    sizeof(struct ngbe_rx_desc))


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

#endif /* _NGBE_RXTX_H_ */
