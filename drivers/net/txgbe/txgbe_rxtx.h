/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_RXTX_H_
#define _TXGBE_RXTX_H_

#define RTE_PMD_TXGBE_TX_MAX_BURST 32
#define RTE_PMD_TXGBE_RX_MAX_BURST 32

#define TXGBE_TX_MAX_SEG                    40

/**
 * Structure associated with each RX queue.
 */
struct txgbe_rx_queue {
	struct rte_mempool  *mb_pool; /**< mbuf pool to populate RX ring. */
	uint64_t            rx_ring_phys_addr; /**< RX ring DMA address. */
	uint16_t            nb_rx_desc; /**< number of RX descriptors. */
	uint16_t            reg_idx;  /**< RX queue register index. */
	uint8_t             crc_len;  /**< 0 if CRC stripped, 4 otherwise. */
	uint8_t             drop_en;  /**< If not 0, set SRRCTL.Drop_En. */
	uint64_t	    offloads; /**< Rx offloads with DEV_RX_OFFLOAD_* */
};

/**
 * Structure associated with each TX queue.
 */
struct txgbe_tx_queue {
	uint64_t            tx_ring_phys_addr; /**< TX ring DMA address. */
	uint16_t            nb_tx_desc;    /**< number of TX descriptors. */
	/**< Start freeing TX buffers if there are less free descriptors than
	 *   this value.
	 */
	uint16_t            tx_free_thresh;
	uint16_t            reg_idx;       /**< TX queue register index. */
	uint64_t offloads; /**< Tx offload flags of DEV_TX_OFFLOAD_* */
};

void txgbe_set_rx_function(struct rte_eth_dev *dev);

uint64_t txgbe_get_tx_port_offloads(struct rte_eth_dev *dev);
uint64_t txgbe_get_rx_queue_offloads(struct rte_eth_dev *dev);
uint64_t txgbe_get_rx_port_offloads(struct rte_eth_dev *dev);
uint64_t txgbe_get_tx_queue_offloads(struct rte_eth_dev *dev);

#endif /* _TXGBE_RXTX_H_ */
