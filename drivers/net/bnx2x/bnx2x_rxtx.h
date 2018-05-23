/*
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 *
 * Copyright (c) 2015-2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 *
 * See LICENSE.bnx2x_pmd for copyright and licensing details.
 */

#ifndef _BNX2X_RXTX_H_
#define _BNX2X_RXTX_H_

#define DEFAULT_TX_FREE_THRESH   64
#define RTE_PMD_BNX2X_TX_MAX_BURST 1

/**
 * Structure associated with each descriptor of the RX ring of a RX queue.
 */
struct bnx2x_rx_entry {
	struct rte_mbuf     *mbuf;                /**< mbuf associated with RX descriptor. */
};

/**
 * Structure associated with each RX queue.
 */
struct bnx2x_rx_queue {
	struct rte_mempool         *mb_pool;             /**< mbuf pool to populate RX ring. */
	union eth_rx_cqe           *cq_ring;             /**< RCQ ring virtual address. */
	uint64_t                   cq_ring_phys_addr;    /**< RCQ ring DMA address. */
	uint64_t                   *rx_ring;             /**< RX ring virtual address. */
	uint64_t                   rx_ring_phys_addr;    /**< RX ring DMA address. */
	struct rte_mbuf            **sw_ring;            /**< address of RX software ring. */
	struct rte_mbuf            *pkt_first_seg;       /**< First segment of current packet. */
	struct rte_mbuf            *pkt_last_seg;        /**< Last segment of current packet. */
	uint16_t                   nb_cq_pages;          /**< number of RCQ pages. */
	uint16_t                   nb_rx_desc;           /**< number of RX descriptors. */
	uint16_t                   nb_rx_pages;          /**< number of RX pages. */
	uint16_t                   rx_bd_head;           /**< Index of current rx bd. */
	uint16_t                   rx_bd_tail;           /**< Index of last rx bd. */
	uint16_t                   rx_cq_head;           /**< Index of current rcq bd. */
	uint16_t                   rx_cq_tail;           /**< Index of last rcq bd. */
	uint16_t                   queue_id;             /**< RX queue index. */
	uint16_t                   port_id;              /**< Device port identifier. */
	struct bnx2x_softc           *sc;                  /**< Ptr to dev_private data. */
};

/**
 * Structure associated with each TX queue.
 */
struct bnx2x_tx_queue {
	/** TX ring virtual address. */
	union eth_tx_bd_types      *tx_ring;             /**< TX ring virtual address. */
	uint64_t                   tx_ring_phys_addr;    /**< TX ring DMA address. */
	struct rte_mbuf            **sw_ring;            /**< virtual address of SW ring. */
	uint16_t                   tx_pkt_tail;          /**< Index of current tx pkt. */
	uint16_t                   tx_pkt_head;          /**< Index of last pkt counted by txeof. */
	uint16_t                   tx_bd_tail;           /**< Index of current tx bd. */
	uint16_t                   tx_bd_head;           /**< Index of last bd counted by txeof. */
	uint16_t                   nb_tx_desc;           /**< number of TX descriptors. */
	uint16_t                   tx_free_thresh;       /**< minimum TX before freeing. */
	uint16_t                   nb_tx_avail;          /**< Number of TX descriptors available. */
	uint16_t                   nb_tx_pages;          /**< number of TX pages */
	uint16_t                   queue_id;             /**< TX queue index. */
	uint16_t                   port_id;              /**< Device port identifier. */
	struct bnx2x_softc           *sc;                  /**< Ptr to dev_private data */
};

int bnx2x_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
			      uint16_t nb_rx_desc, unsigned int socket_id,
			      const struct rte_eth_rxconf *rx_conf,
			      struct rte_mempool *mb_pool);

int bnx2x_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
			      uint16_t nb_tx_desc, unsigned int socket_id,
			      const struct rte_eth_txconf *tx_conf);

void bnx2x_dev_rx_queue_release(void *rxq);
void bnx2x_dev_tx_queue_release(void *txq);
int bnx2x_dev_rx_init(struct rte_eth_dev *dev);
void bnx2x_dev_clear_queues(struct rte_eth_dev *dev);

#endif /* _BNX2X_RXTX_H_ */
