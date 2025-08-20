/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Phytium Technology Co., Ltd.
 */

#ifndef _MACB_RXTX_H_
#define _MACB_RXTX_H_

#include "macb_ethdev.h"

#define MACB_RX_BUFFER_SIZE 128
#define MACB_MAX_RECLAIM_NUM 64
#define MACB_RX_DATA_OFFSET   0

#define MACB_DESCS_PER_LOOP    4
#define MACB_MAX_RX_BURST    32
#define MACB_RXQ_REARM_THRESH      32
#define MACB_DESC_ADDR_INTERVAL 2
#define MACB_LOOK_AHEAD	8
#define MACB_NEXT_FETCH	7
#define MACB_NEON_PREFETCH_ENTRY	4

#define BIT_TO_BYTE_SHIFT 3
#define MACB_DATA_BUS_WIDTH_MASK(x) (((x) >> BIT_TO_BYTE_SHIFT) - 1)

struct gem_tx_ts {
	struct rte_mbuf *mbuf;
	struct macb_dma_desc_ptp desc_ptp;
};

struct __rte_cache_aligned macb_rx_queue_stats {
	union {
		uint64_t first;
		uint64_t rx_packets;
	};
	uint64_t rx_bytes;
	uint64_t rx_dropped;
};

struct __rte_cache_aligned macb_tx_queue_stats {
	uint64_t tx_packets;
	uint64_t tx_bytes;
	uint64_t tx_dropped;
	uint64_t tx_start_packets;
	uint64_t tx_start_bytes;
};

struct macb_tx_entry {
	struct rte_mbuf *mbuf;
};

struct macb_rx_entry {
	struct rte_mbuf *mbuf;
};

struct macb_rx_queue {
	struct macb		*bp;
	struct rte_mempool	*mb_pool;   /**< mbuf pool to populate RX ring. */

	unsigned int		ISR;
	unsigned int		IER;
	unsigned int		IDR;
	unsigned int		IMR;
	unsigned int		RBQS;
	unsigned int		RBQP;
	unsigned int		RBQPH;

	rte_iova_t		rx_ring_dma;
	unsigned int		rx_tail;
	unsigned int		nb_rx_desc;    /**< number of TX descriptors. */
	uint16_t		rx_free_thresh;/**< max free RX desc to hold. */
	uint16_t		queue_id; /**< TX queue index. */
	uint16_t		port_id;  /**< Device port identifier. */
	uint32_t		crc_len;    /**< 0 if CRC stripped, 4 otherwise. */
	uint32_t		flags;      /**< RX flags. */
	uint64_t		offloads;   /**< offloads of DEV_RX_OFFLOAD_* */
	unsigned int		rx_prepared_head;
	struct macb_dma_desc	*rx_ring;
	struct macb_rx_entry	*rx_sw_ring;

	struct macb_rx_queue_stats	stats;
	struct rte_mbuf		*pkt_first_seg; /**< First segment of current packet. */
	struct rte_mbuf		*pkt_last_seg;  /**< Last segment of current packet. */

	uint16_t		rxrearm_nb;     /**< number of remaining to be re-armed */
	unsigned int		rxrearm_start;  /**< the idx we start the re-arming from */

	/** need to alloc dummy mbuf, for wraparound when scanning hw ring */
	struct rte_mbuf		fake_mbuf;
};

struct macb_tx_queue {
	struct macb		*bp;

	unsigned int		ISR;
	unsigned int		IER;
	unsigned int		IDR;
	unsigned int		IMR;
	unsigned int		TBQP;
	unsigned int		TBQPH;

	unsigned int		tx_head, tx_tail;
	unsigned int		nb_tx_desc;    /**< number of TX descriptors. */
	uint16_t		tx_free_thresh;/**< max free TX desc to hold. */
	uint16_t		tx_rs_thresh;
	uint16_t		queue_id; /**< TX queue index. */
	uint16_t		port_id;  /**< Device port identifier. */

	struct macb_dma_desc	*tx_ring;
	struct macb_tx_entry	*tx_sw_ring;
	rte_iova_t		tx_ring_dma;

	struct macb_tx_queue_stats	stats;
};

void macb_rxq_info_get(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		       struct rte_eth_rxq_info *qinfo);
void macb_txq_info_get(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		       struct rte_eth_txq_info *qinfo);
uint64_t macb_get_rx_port_offloads_capa(struct rte_eth_dev *dev);
uint64_t macb_get_rx_queue_offloads_capa(struct rte_eth_dev *dev);
int macb_eth_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
			    unsigned int socket, const struct rte_eth_rxconf *conf __rte_unused,
			    struct rte_mempool *mp);
int macb_eth_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
			    unsigned int socket, const struct rte_eth_txconf *conf);
void macb_eth_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
void macb_eth_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
void macb_dev_free_queues(struct rte_eth_dev *dev);
uint16_t macb_eth_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
uint16_t macb_eth_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
uint16_t macb_eth_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t macb_eth_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t macb_eth_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t macb_eth_recv_scattered_pkts_vec(void *rx_queue,
					  struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t eth_macb_prep_pkts(__rte_unused void *tx_queue,
			    struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
void __rte_cold macb_rx_queue_release_mbufs_vec(struct macb_rx_queue *rxq);
void macb_rx_queue_release_mbufs(struct macb_rx_queue *rxq);
void macb_tx_queue_release_mbufs(struct macb_tx_queue *txq);
int macb_rx_phyaddr_check(struct rte_eth_dev *dev);
int macb_tx_phyaddr_check(struct rte_eth_dev *dev);
int macb_eth_rx_init(struct rte_eth_dev *dev);
void macb_eth_tx_init(struct rte_eth_dev *dev);
void macb_reset_rx_queue(struct macb_rx_queue *rxq);
void macb_reset_tx_queue(struct macb_tx_queue *txq, struct rte_eth_dev *dev);
void macb_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id, struct rte_eth_rxq_info *qinfo);
void macb_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id, struct rte_eth_txq_info *qinfo);

void macb_init_rx_buffer_size(struct macb *bp, size_t size);


/* DMA buffer descriptor might be different size
 * depends on hardware configuration:
 *
 * 1. dma address width 32 bits:
 *    word 1: 32 bit address of Data Buffer
 *    word 2: control
 *
 * 2. dma address width 64 bits:
 *    word 1: 32 bit address of Data Buffer
 *    word 2: control
 *    word 3: upper 32 bit address of Data Buffer
 *    word 4: unused
 *
 * 3. dma address width 32 bits with hardware timestamping:
 *    word 1: 32 bit address of Data Buffer
 *    word 2: control
 *    word 3: timestamp word 1
 *    word 4: timestamp word 2
 *
 * 4. dma address width 64 bits with hardware timestamping:
 *    word 1: 32 bit address of Data Buffer
 *    word 2: control
 *    word 3: upper 32 bit address of Data Buffer
 *    word 4: unused
 *    word 5: timestamp word 1
 *    word 6: timestamp word 2
 */
static inline unsigned int macb_dma_desc_get_size(struct macb *bp)
{
	unsigned int desc_size;

	switch (bp->hw_dma_cap) {
	case HW_DMA_CAP_64B:
		desc_size =
			sizeof(struct macb_dma_desc) + sizeof(struct macb_dma_desc_64);
		break;
	case HW_DMA_CAP_PTP:
		desc_size =
			sizeof(struct macb_dma_desc) + sizeof(struct macb_dma_desc_ptp);
		break;
	case HW_DMA_CAP_64B_PTP:
		desc_size = sizeof(struct macb_dma_desc) +
					sizeof(struct macb_dma_desc_64) +
					sizeof(struct macb_dma_desc_ptp);
		break;
	default:
		desc_size = sizeof(struct macb_dma_desc);
	}
	return desc_size;

	return sizeof(struct macb_dma_desc);
}

/* Ring buffer accessors */
static inline unsigned int macb_tx_ring_wrap(struct macb *bp, unsigned int index)
{
	return index & (bp->tx_ring_size - 1);
}

static inline unsigned int macb_adj_dma_desc_idx(struct macb *bp,
						unsigned int desc_idx)
{
#ifdef MACB_EXT_DESC
	switch (bp->hw_dma_cap) {
	case HW_DMA_CAP_64B:
	case HW_DMA_CAP_PTP:
		desc_idx <<= 1;
		break;
	case HW_DMA_CAP_64B_PTP:
		desc_idx *= 3;
		break;
	default:
		break;
	}
#endif
	return desc_idx;
}

static inline struct macb_tx_entry *macb_tx_entry(struct macb_tx_queue *queue,
						unsigned int index)
{
	return &queue->tx_sw_ring[macb_tx_ring_wrap(queue->bp, index)];
}

static inline struct macb_dma_desc *macb_tx_desc(struct macb_tx_queue *queue,
						 unsigned int index)
{
	index = macb_tx_ring_wrap(queue->bp, index);
	index = macb_adj_dma_desc_idx(queue->bp, index);
	return &queue->tx_ring[index];
}

static inline struct macb_dma_desc_64 *macb_64b_desc(struct macb *bp,
						 struct macb_dma_desc *desc)
{
	if (bp->hw_dma_cap & HW_DMA_CAP_64B)
		return (struct macb_dma_desc_64 *)((uint8_t *)desc
						   + sizeof(struct macb_dma_desc));
	return NULL;
}

static inline void macb_set_addr(struct macb *bp, struct macb_dma_desc *desc,
							dma_addr_t addr)
{
	struct macb_dma_desc_64 *desc_64;

	if (bp->hw_dma_cap & HW_DMA_CAP_64B) {
		desc_64 = macb_64b_desc(bp, desc);
		desc_64->addrh = upper_32_bits(addr);
		/* The low bits of RX address contain the RX_USED bit, clearing
		 * of which allows packet RX. Make sure the high bits are also
		 * visible to HW at that point.
		 */
		rte_wmb();
	}

	desc->addr = lower_32_bits(addr);
}

static inline unsigned int macb_rx_ring_wrap(struct macb *bp, unsigned int index)
{
	return index & (bp->rx_ring_size - 1);
}

static inline struct macb_dma_desc *macb_rx_desc(struct macb_rx_queue *queue,
						unsigned int index)
{
	index = macb_rx_ring_wrap(queue->bp, index);
	index = macb_adj_dma_desc_idx(queue->bp, index);
	return &queue->rx_ring[index];
}

static inline struct macb_rx_entry *macb_rx_entry(struct macb_rx_queue *queue,
						unsigned int index)
{
	return &queue->rx_sw_ring[macb_rx_ring_wrap(queue->bp, index)];
}

static inline uint16_t macb_reclaim_txd(struct macb_tx_queue *queue)
{
	struct macb_dma_desc *curr_desc;
	uint32_t tx_head, tx_tail;
	uint16_t reclaim = 0;

	tx_head = queue->tx_head;
	tx_tail = queue->tx_tail;
	while (likely(tx_head != tx_tail && reclaim < MACB_MAX_RECLAIM_NUM)) {
		curr_desc = macb_tx_desc(queue, tx_head);
		if (unlikely(!(curr_desc->ctrl & MACB_BIT(TX_USED)))) {
			goto out;
		} else {
			if (likely(curr_desc->ctrl & MACB_BIT(TX_LAST))) {
				tx_head = macb_tx_ring_wrap(queue->bp, ++tx_head);
				reclaim++;
			} else {
				reclaim++;
				do {
					tx_head = macb_tx_ring_wrap(queue->bp, ++tx_head);
					curr_desc = macb_tx_desc(queue, tx_head);
					curr_desc->ctrl |= MACB_BIT(TX_USED);
					reclaim++;
				} while (unlikely(!(curr_desc->ctrl & MACB_BIT(TX_LAST))));
				tx_head = macb_tx_ring_wrap(queue->bp, ++tx_head);
			}
		}
	}

out:
	queue->tx_head = tx_head;
	return reclaim;
}

#endif /* _MACB_RXTX_H_ */
