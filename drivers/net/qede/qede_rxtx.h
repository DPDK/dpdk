/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */


#ifndef _QEDE_RXTX_H_
#define _QEDE_RXTX_H_

#include "qede_ethdev.h"

/* Ring Descriptors */
#define RX_RING_SIZE_POW        16	/* 64K */
#define RX_RING_SIZE            (1ULL << RX_RING_SIZE_POW)
#define NUM_RX_BDS_MAX          (RX_RING_SIZE - 1)
#define NUM_RX_BDS_MIN          128
#define NUM_RX_BDS_DEF          NUM_RX_BDS_MAX
#define NUM_RX_BDS(q)           (q->nb_rx_desc - 1)

#define TX_RING_SIZE_POW        16	/* 64K */
#define TX_RING_SIZE            (1ULL << TX_RING_SIZE_POW)
#define NUM_TX_BDS_MAX          (TX_RING_SIZE - 1)
#define NUM_TX_BDS_MIN          128
#define NUM_TX_BDS_DEF          NUM_TX_BDS_MAX
#define NUM_TX_BDS(q)           (q->nb_tx_desc - 1)

#define TX_CONS(txq)            (txq->sw_tx_cons & NUM_TX_BDS(txq))
#define TX_PROD(txq)            (txq->sw_tx_prod & NUM_TX_BDS(txq))

#define QEDE_DEFAULT_TX_FREE_THRESH	32

#define QEDE_CSUM_ERROR			(1 << 0)
#define QEDE_CSUM_UNNECESSARY		(1 << 1)
#define QEDE_TUNN_CSUM_UNNECESSARY	(1 << 2)

#define QEDE_BD_SET_ADDR_LEN(bd, maddr, len) \
	do { \
		(bd)->addr.hi = rte_cpu_to_le_32(U64_HI(maddr)); \
		(bd)->addr.lo = rte_cpu_to_le_32(U64_LO(maddr)); \
		(bd)->nbytes = rte_cpu_to_le_16(len); \
		/* FW 8.10.x specific change */  \
		(bd)->data.bitfields = ((len) & \
					 ETH_TX_DATA_1ST_BD_PKT_LEN_MASK) \
					<< ETH_TX_DATA_1ST_BD_PKT_LEN_SHIFT; \
	} while (0)

#define CQE_HAS_VLAN(flags) \
	((flags) & (PARSING_AND_ERR_FLAGS_TAG8021QEXIST_MASK \
		<< PARSING_AND_ERR_FLAGS_TAG8021QEXIST_SHIFT))

#define CQE_HAS_OUTER_VLAN(flags) \
	((flags) & (PARSING_AND_ERR_FLAGS_TUNNEL8021QTAGEXIST_MASK \
		<< PARSING_AND_ERR_FLAGS_TUNNEL8021QTAGEXIST_SHIFT))

/* Max supported alignment is 256 (8 shift)
 * minimal alignment shift 6 is optimal for 57xxx HW performance
 */
#define QEDE_L1_CACHE_SHIFT	6
#define QEDE_RX_ALIGN_SHIFT	(RTE_MAX(6, RTE_MIN(8, QEDE_L1_CACHE_SHIFT)))
#define QEDE_FW_RX_ALIGN_END	(1UL << QEDE_RX_ALIGN_SHIFT)

#define QEDE_ETH_OVERHEAD       (ETHER_HDR_LEN + 8 + 8 + QEDE_FW_RX_ALIGN_END)

/* TBD: Excluding IPV6 */
#define QEDE_RSS_OFFLOAD_ALL    (ETH_RSS_IPV4 | ETH_RSS_NONFRAG_IPV4_TCP | \
				 ETH_RSS_NONFRAG_IPV4_UDP)

#define QEDE_TXQ_FLAGS		((uint32_t)ETH_TXQ_FLAGS_NOMULTSEGS)

#define MAX_NUM_TC		8

#define for_each_queue(i) for (i = 0; i < qdev->num_queues; i++)

/*
 * RX BD descriptor ring
 */
struct qede_rx_entry {
	struct rte_mbuf *mbuf;
	uint32_t page_offset;
	/* allows expansion .. */
};

/*
 * Structure associated with each RX queue.
 */
struct qede_rx_queue {
	struct rte_mempool *mb_pool;
	struct ecore_chain rx_bd_ring;
	struct ecore_chain rx_comp_ring;
	uint16_t *hw_cons_ptr;
	void OSAL_IOMEM *hw_rxq_prod_addr;
	struct qede_rx_entry *sw_rx_ring;
	uint16_t sw_rx_cons;
	uint16_t sw_rx_prod;
	uint16_t nb_rx_desc;
	uint16_t queue_id;
	uint16_t port_id;
	uint16_t rx_buf_size;
	uint64_t rcv_pkts;
	uint64_t rx_segs;
	uint64_t rx_hw_errors;
	uint64_t rx_alloc_errors;
	struct qede_dev *qdev;
};

/*
 * TX BD descriptor ring
 */
struct qede_tx_entry {
	struct rte_mbuf *mbuf;
	uint8_t flags;
};

union db_prod {
	struct eth_db_data data;
	uint32_t raw;
};

struct qede_tx_queue {
	struct ecore_chain tx_pbl;
	struct qede_tx_entry *sw_tx_ring;
	uint16_t nb_tx_desc;
	uint16_t nb_tx_avail;
	uint16_t tx_free_thresh;
	uint16_t queue_id;
	uint16_t *hw_cons_ptr;
	uint16_t sw_tx_cons;
	uint16_t sw_tx_prod;
	void OSAL_IOMEM *doorbell_addr;
	volatile union db_prod tx_db;
	uint16_t port_id;
	uint64_t xmit_pkts;
	struct qede_dev *qdev;
};

struct qede_fastpath {
	struct qede_dev *qdev;
	u8 type;
	uint8_t id;
	struct ecore_sb_info *sb_info;
	struct qede_rx_queue *rxq;
	struct qede_tx_queue *txqs[MAX_NUM_TC];
	char name[80];
};

/*
 * RX/TX function prototypes
 */
int qede_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			uint16_t nb_desc, unsigned int socket_id,
			const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mp);

int qede_tx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_desc,
			unsigned int socket_id,
			const struct rte_eth_txconf *tx_conf);

void qede_rx_queue_release(void *rx_queue);

void qede_tx_queue_release(void *tx_queue);

int qede_dev_start(struct rte_eth_dev *eth_dev);

void qede_dev_stop(struct rte_eth_dev *eth_dev);

int qede_reset_fp_rings(struct qede_dev *qdev);

void qede_free_fp_arrays(struct qede_dev *qdev);

void qede_free_mem_load(struct rte_eth_dev *eth_dev);

uint16_t qede_xmit_pkts(void *p_txq, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts);

uint16_t qede_recv_pkts(void *p_rxq, struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts);

/* Fastpath resource alloc/dealloc helpers */
int qede_alloc_fp_resc(struct qede_dev *qdev);

void qede_dealloc_fp_resc(struct rte_eth_dev *eth_dev);

#endif /* _QEDE_RXTX_H_ */
