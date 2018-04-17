/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#ifndef _AXGBE_RXTX_H_
#define _AXGBE_RXTX_H_

/* to suppress gcc warnings related to descriptor casting*/
#ifdef RTE_TOOLCHAIN_GCC
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#ifdef RTE_TOOLCHAIN_CLANG
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

/* Descriptor related defines */
#define AXGBE_MAX_RING_DESC		4096 /*should be power of 2*/
#define AXGBE_TX_DESC_MIN_FREE		(AXGBE_MAX_RING_DESC >> 3)
#define AXGBE_TX_DESC_MAX_PROC		(AXGBE_MAX_RING_DESC >> 1)
#define AXGBE_MIN_RING_DESC		32
#define RTE_AXGBE_DESCS_PER_LOOP	4
#define RTE_AXGBE_MAX_RX_BURST		32

#define AXGBE_RX_FREE_THRESH		32
#define AXGBE_TX_FREE_THRESH		32

#define AXGBE_DESC_ALIGN		128
#define AXGBE_DESC_OWN			0x80000000
#define AXGBE_ERR_STATUS		0x000f0000
#define AXGBE_L3_CSUM_ERR		0x00050000
#define AXGBE_L4_CSUM_ERR		0x00060000

#include "axgbe_common.h"

#define AXGBE_GET_DESC_PT(_queue, _idx)			\
	(((_queue)->desc) +				\
	((_idx) & ((_queue)->nb_desc - 1)))

#define AXGBE_GET_DESC_IDX(_queue, _idx)			\
	((_idx) & ((_queue)->nb_desc - 1))			\

/* Rx desc format */
union axgbe_rx_desc {
	struct {
		uint64_t baddr;
		uint32_t desc2;
		uint32_t desc3;
	} read;
	struct {
		uint32_t desc0;
		uint32_t desc1;
		uint32_t desc2;
		uint32_t desc3;
	} write;
};

struct axgbe_rx_queue {
	/* membuf pool for rx buffers */
	struct rte_mempool *mb_pool;
	/* H/w Rx buffer size configured in DMA */
	unsigned int buf_size;
	/* CRC h/w offload */
	uint16_t crc_len;
	/* address of  s/w rx buffers */
	struct rte_mbuf **sw_ring;
	/* Port private data */
	struct axgbe_port *pdata;
	/* Number of Rx descriptors in queue */
	uint16_t nb_desc;
	/* max free RX desc to hold */
	uint16_t free_thresh;
	/* Index of descriptor to check for packet availability */
	uint64_t cur;
	/* Index of descriptor to check for buffer reallocation */
	uint64_t dirty;
	/* Software Rx descriptor ring*/
	volatile union axgbe_rx_desc *desc;
	/* Ring physical address */
	uint64_t ring_phys_addr;
	/* Dma Channel register address */
	void *dma_regs;
	/* Dma channel tail register address*/
	volatile uint32_t *dma_tail_reg;
	/* DPDK queue index */
	uint16_t queue_id;
	/* dpdk port id*/
	uint16_t port_id;
	/* queue stats */
	uint64_t pkts;
	uint64_t bytes;
	uint64_t errors;
	/* Number of mbufs allocated from pool*/
	uint64_t mbuf_alloc;

} __rte_cache_aligned;

/*Tx descriptor format */
struct axgbe_tx_desc {
	phys_addr_t baddr;
	uint32_t desc2;
	uint32_t desc3;
};

struct axgbe_tx_queue {
	/* Port private data reference */
	struct axgbe_port *pdata;
	/* Number of Tx descriptors in queue*/
	uint16_t nb_desc;
	/* Start freeing TX buffers if there are less free descriptors than
	 * this value
	 */
	uint16_t free_thresh;
	/* Available descriptors for Tx processing*/
	uint16_t nb_desc_free;
	/* Batch of mbufs/descs to release */
	uint16_t free_batch_cnt;
	/* Flag for vector support */
	uint16_t vector_disable;
	/* Index of descriptor to be used for current transfer */
	uint64_t cur;
	/* Index of descriptor to check for transfer complete */
	uint64_t dirty;
	/* Virtual address of ring */
	volatile struct axgbe_tx_desc *desc;
	/* Physical address of ring */
	uint64_t ring_phys_addr;
	/* Dma channel register space */
	void  *dma_regs;
	/* Dma tail register address of ring*/
	volatile uint32_t *dma_tail_reg;
	/* Tx queue index/id*/
	uint16_t queue_id;
	/* Reference to hold Tx mbufs mapped to Tx descriptors freed
	 * after transmission confirmation
	 */
	struct rte_mbuf **sw_ring;
	/* dpdk port id*/
	uint16_t port_id;
	/* queue stats */
	uint64_t pkts;
	uint64_t bytes;
	uint64_t errors;

} __rte_cache_aligned;

/*Queue related APIs */

/*
 * RX/TX function prototypes
 */


void axgbe_dev_tx_queue_release(void *txq);
int  axgbe_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
			      uint16_t nb_tx_desc, unsigned int socket_id,
			      const struct rte_eth_txconf *tx_conf);
void axgbe_dev_enable_tx(struct rte_eth_dev *dev);
void axgbe_dev_disable_tx(struct rte_eth_dev *dev);
int axgbe_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int axgbe_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);

uint16_t axgbe_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			 uint16_t nb_pkts);
uint16_t axgbe_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			 uint16_t nb_pkts);


void axgbe_dev_rx_queue_release(void *rxq);
int  axgbe_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
			      uint16_t nb_rx_desc, unsigned int socket_id,
			      const struct rte_eth_rxconf *rx_conf,
			      struct rte_mempool *mb_pool);
void axgbe_dev_enable_rx(struct rte_eth_dev *dev);
void axgbe_dev_disable_rx(struct rte_eth_dev *dev);
int axgbe_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int axgbe_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);
uint16_t axgbe_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			 uint16_t nb_pkts);
uint16_t axgbe_recv_pkts_threshold_refresh(void *rx_queue,
					   struct rte_mbuf **rx_pkts,
					   uint16_t nb_pkts);
void axgbe_dev_clear_queues(struct rte_eth_dev *dev);

#endif /* _AXGBE_RXTX_H_ */
