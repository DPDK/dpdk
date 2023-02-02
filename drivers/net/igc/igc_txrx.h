/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _IGC_TXRX_H_
#define _IGC_TXRX_H_

#include "igc_ethdev.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint64_t igc_tx_timestamp_dynflag;
extern int igc_tx_timestamp_dynfield_offset;

struct igc_rx_entry {
	struct rte_mbuf *mbuf; /**< mbuf associated with RX descriptor. */
};

/**
 * Structure associated with each RX queue.
 */
struct igc_rx_queue {
	struct rte_mempool  *mb_pool;   /**< mbuf pool to populate RX ring. */
	volatile union igc_adv_rx_desc *rx_ring;
	/**< RX ring virtual address. */
	uint64_t            rx_ring_phys_addr; /**< RX ring DMA address. */
	volatile uint32_t   *rdt_reg_addr; /**< RDT register address. */
	volatile uint32_t   *rdh_reg_addr; /**< RDH register address. */
	struct igc_rx_entry *sw_ring;   /**< address of RX software ring. */
	struct rte_mbuf *pkt_first_seg; /**< First segment of current packet. */
	struct rte_mbuf *pkt_last_seg;  /**< Last segment of current packet. */
	uint16_t            nb_rx_desc; /**< number of RX descriptors. */
	uint16_t            rx_tail;    /**< current value of RDT register. */
	uint16_t            nb_rx_hold; /**< number of held free RX desc. */
	uint16_t            rx_free_thresh; /**< max free RX desc to hold. */
	uint16_t            queue_id;   /**< RX queue index. */
	uint16_t            reg_idx;    /**< RX queue register index. */
	uint16_t            port_id;    /**< Device port identifier. */
	uint8_t             pthresh;    /**< Prefetch threshold register. */
	uint8_t             hthresh;    /**< Host threshold register. */
	uint8_t             wthresh;    /**< Write-back threshold register. */
	uint8_t             crc_len;    /**< 0 if CRC stripped, 4 otherwise. */
	uint8_t             drop_en;    /**< If not 0, set SRRCTL.Drop_En. */
	uint32_t            flags;      /**< RX flags. */
	uint64_t            offloads;   /**< offloads of RTE_ETH_RX_OFFLOAD_* */
	uint64_t            rx_timestamp;
};

/** Offload features */
union igc_tx_offload {
	uint64_t data;
	struct {
		uint64_t l3_len:9; /**< L3 (IP) Header Length. */
		uint64_t l2_len:7; /**< L2 (MAC) Header Length. */
		uint64_t vlan_tci:16;
		/**< VLAN Tag Control Identifier(CPU order). */
		uint64_t l4_len:8; /**< L4 (TCP/UDP) Header Length. */
		uint64_t tso_segsz:16; /**< TCP TSO segment size. */
		/* uint64_t unused:8; */
	};
};

/**
 * Compare mask for igc_tx_offload.data,
 * should be in sync with igc_tx_offload layout.
 */
#define TX_MACIP_LEN_CMP_MASK  0x000000000000FFFFULL /**< L2L3 header mask. */
#define TX_VLAN_CMP_MASK       0x00000000FFFF0000ULL /**< Vlan mask. */
#define TX_TCP_LEN_CMP_MASK    0x000000FF00000000ULL /**< TCP header mask. */
#define TX_TSO_MSS_CMP_MASK    0x00FFFF0000000000ULL /**< TSO segsz mask. */
/** Mac + IP + TCP + Mss mask. */
#define TX_TSO_CMP_MASK        \
	(TX_MACIP_LEN_CMP_MASK | TX_TCP_LEN_CMP_MASK | TX_TSO_MSS_CMP_MASK)

/**
 * Structure to check if new context need be built
 */
struct igc_advctx_info {
	uint64_t flags;           /**< ol_flags related to context build. */
	/** tx offload: vlan, tso, l2-l3-l4 lengths. */
	union igc_tx_offload tx_offload;
	/** compare mask for tx offload. */
	union igc_tx_offload tx_offload_mask;
};

/**
 * Hardware context number
 */
enum {
	IGC_CTX_0    = 0, /**< CTX0    */
	IGC_CTX_1    = 1, /**< CTX1    */
	IGC_CTX_NUM  = 2, /**< CTX_NUM */
};

/**
 * Structure associated with each descriptor of the TX ring of a TX queue.
 */
struct igc_tx_entry {
	struct rte_mbuf *mbuf; /**< mbuf associated with TX desc, if any. */
	uint16_t next_id; /**< Index of next descriptor in ring. */
	uint16_t last_id; /**< Index of last scattered descriptor. */
};

/**
 * Structure associated with each TX queue.
 */
struct igc_tx_queue {
	volatile union igc_adv_tx_desc *tx_ring; /**< TX ring address */
	uint64_t               tx_ring_phys_addr; /**< TX ring DMA address. */
	struct igc_tx_entry    *sw_ring; /**< virtual address of SW ring. */
	volatile uint32_t      *tdt_reg_addr; /**< Address of TDT register. */
	uint32_t               txd_type;      /**< Device-specific TXD type */
	uint16_t               nb_tx_desc;    /**< number of TX descriptors. */
	uint16_t               tx_tail;  /**< Current value of TDT register. */
	uint16_t               tx_head;
	/**< Index of first used TX descriptor. */
	uint16_t               queue_id; /**< TX queue index. */
	uint16_t               reg_idx;  /**< TX queue register index. */
	uint16_t               port_id;  /**< Device port identifier. */
	uint8_t                pthresh;  /**< Prefetch threshold register. */
	uint8_t                hthresh;  /**< Host threshold register. */
	uint8_t                wthresh;  /**< Write-back threshold register. */
	uint8_t                ctx_curr;

	/**< Start context position for transmit queue. */
	struct igc_advctx_info ctx_cache[IGC_CTX_NUM];
	/**< Hardware context history.*/
	uint64_t               offloads; /**< offloads of RTE_ETH_TX_OFFLOAD_* */
};

/*
 * RX/TX function prototypes
 */
void eth_igc_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
void eth_igc_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
void igc_dev_clear_queues(struct rte_eth_dev *dev);
int eth_igc_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

uint32_t eth_igc_rx_queue_count(void *rx_queue);

int eth_igc_rx_descriptor_status(void *rx_queue, uint16_t offset);

int eth_igc_tx_descriptor_status(void *tx_queue, uint16_t offset);

int eth_igc_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		uint16_t nb_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);
int eth_igc_tx_done_cleanup(void *txqueue, uint32_t free_cnt);

int igc_rx_init(struct rte_eth_dev *dev);
void igc_tx_init(struct rte_eth_dev *dev);
void igc_rss_disable(struct rte_eth_dev *dev);
void
igc_hw_rss_hash_set(struct igc_hw *hw, struct rte_eth_rss_conf *rss_conf);
int igc_del_rss_filter(struct rte_eth_dev *dev);
void igc_rss_conf_set(struct igc_rss_filter *out,
		const struct rte_flow_action_rss *rss);
int igc_add_rss_filter(struct rte_eth_dev *dev, struct igc_rss_filter *rss);
void igc_clear_rss_filter(struct rte_eth_dev *dev);
void eth_igc_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo);
void eth_igc_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo);
void eth_igc_vlan_strip_queue_set(struct rte_eth_dev *dev,
			uint16_t rx_queue_id, int on);
uint16_t igc_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t igc_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
uint16_t eth_igc_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts);
uint16_t igc_recv_scattered_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts);
#ifdef __cplusplus
}
#endif

#endif /* _IGC_TXRX_H_ */
