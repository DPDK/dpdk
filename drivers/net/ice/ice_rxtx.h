/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _ICE_RXTX_H_
#define _ICE_RXTX_H_

#include "ice_ethdev.h"

#define ICE_ALIGN_RING_DESC  32
#define ICE_MIN_RING_DESC    64
#define ICE_MAX_RING_DESC    4096
#define ICE_DMA_MEM_ALIGN    4096
#define ICE_RING_BASE_ALIGN  128

#define ICE_RX_MAX_BURST 32
#define ICE_TX_MAX_BURST 32

#define ICE_CHK_Q_ENA_COUNT        100
#define ICE_CHK_Q_ENA_INTERVAL_US  100

#ifdef RTE_LIBRTE_ICE_16BYTE_RX_DESC
#define ice_rx_desc ice_16byte_rx_desc
#else
#define ice_rx_desc ice_32byte_rx_desc
#endif

#define ICE_SUPPORT_CHAIN_NUM 5

#define ICE_TD_CMD                      ICE_TX_DESC_CMD_EOP

#define ICE_VPMD_RX_BURST           32
#define ICE_VPMD_TX_BURST           32
#define ICE_RXQ_REARM_THRESH        32
#define ICE_MAX_RX_BURST            ICE_RXQ_REARM_THRESH
#define ICE_TX_MAX_FREE_BUF_SZ      64
#define ICE_DESCS_PER_LOOP          4

typedef void (*ice_rx_release_mbufs_t)(struct ice_rx_queue *rxq);
typedef void (*ice_tx_release_mbufs_t)(struct ice_tx_queue *txq);

struct ice_rx_entry {
	struct rte_mbuf *mbuf;
};

struct ice_rx_queue {
	struct rte_mempool *mp; /* mbuf pool to populate RX ring */
	volatile union ice_rx_desc *rx_ring;/* RX ring virtual address */
	uint64_t rx_ring_phys_addr; /* RX ring DMA address */
	struct ice_rx_entry *sw_ring; /* address of RX soft ring */
	uint16_t nb_rx_desc; /* number of RX descriptors */
	uint16_t rx_free_thresh; /* max free RX desc to hold */
	uint16_t rx_tail; /* current value of tail */
	uint16_t nb_rx_hold; /* number of held free RX desc */
	struct rte_mbuf *pkt_first_seg; /**< first segment of current packet */
	struct rte_mbuf *pkt_last_seg; /**< last segment of current packet */
	uint16_t rx_nb_avail; /**< number of staged packets ready */
	uint16_t rx_next_avail; /**< index of next staged packets */
	uint16_t rx_free_trigger; /**< triggers rx buffer allocation */
	struct rte_mbuf fake_mbuf; /**< dummy mbuf */
	struct rte_mbuf *rx_stage[ICE_RX_MAX_BURST * 2];

	uint16_t rxrearm_nb;	/**< number of remaining to be re-armed */
	uint16_t rxrearm_start;	/**< the idx we start the re-arming from */
	uint64_t mbuf_initializer; /**< value to init mbufs */

	uint8_t port_id; /* device port ID */
	uint8_t crc_len; /* 0 if CRC stripped, 4 otherwise */
	uint16_t queue_id; /* RX queue index */
	uint16_t reg_idx; /* RX queue register index */
	uint8_t drop_en; /* if not 0, set register bit */
	volatile uint8_t *qrx_tail; /* register address of tail */
	struct ice_vsi *vsi; /* the VSI this queue belongs to */
	uint16_t rx_buf_len; /* The packet buffer size */
	uint16_t rx_hdr_len; /* The header buffer size */
	uint16_t max_pkt_len; /* Maximum packet length */
	bool q_set; /* indicate if rx queue has been configured */
	bool rx_deferred_start; /* don't start this queue in dev start */
	ice_rx_release_mbufs_t rx_rel_mbufs;
};

struct ice_tx_entry {
	struct rte_mbuf *mbuf;
	uint16_t next_id;
	uint16_t last_id;
};

struct ice_tx_queue {
	uint16_t nb_tx_desc; /* number of TX descriptors */
	uint64_t tx_ring_phys_addr; /* TX ring DMA address */
	volatile struct ice_tx_desc *tx_ring; /* TX ring virtual address */
	struct ice_tx_entry *sw_ring; /* virtual address of SW ring */
	uint16_t tx_tail; /* current value of tail register */
	volatile uint8_t *qtx_tail; /* register address of tail */
	uint16_t nb_tx_used; /* number of TX desc used since RS bit set */
	/* index to last TX descriptor to have been cleaned */
	uint16_t last_desc_cleaned;
	/* Total number of TX descriptors ready to be allocated. */
	uint16_t nb_tx_free;
	/* Start freeing TX buffers if there are less free descriptors than
	 * this value.
	 */
	uint16_t tx_free_thresh;
	/* Number of TX descriptors to use before RS bit is set. */
	uint16_t tx_rs_thresh;
	uint8_t pthresh; /**< Prefetch threshold register. */
	uint8_t hthresh; /**< Host threshold register. */
	uint8_t wthresh; /**< Write-back threshold reg. */
	uint8_t port_id; /* Device port identifier. */
	uint16_t queue_id; /* TX queue index. */
	uint32_t q_teid; /* TX schedule node id. */
	uint16_t reg_idx;
	uint64_t offloads;
	struct ice_vsi *vsi; /* the VSI this queue belongs to */
	uint16_t tx_next_dd;
	uint16_t tx_next_rs;
	bool tx_deferred_start; /* don't start this queue in dev start */
	bool q_set; /* indicate if tx queue has been configured */
	ice_tx_release_mbufs_t tx_rel_mbufs;
};

/* Offload features */
union ice_tx_offload {
	uint64_t data;
	struct {
		uint64_t l2_len:7; /* L2 (MAC) Header Length. */
		uint64_t l3_len:9; /* L3 (IP) Header Length. */
		uint64_t l4_len:8; /* L4 Header Length. */
		uint64_t tso_segsz:16; /* TCP TSO segment size */
		uint64_t outer_l2_len:8; /* outer L2 Header Length */
		uint64_t outer_l3_len:16; /* outer L3 Header Length */
	};
};

int ice_rx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t queue_idx,
		       uint16_t nb_desc,
		       unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mp);
int ice_tx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t queue_idx,
		       uint16_t nb_desc,
		       unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf);
int ice_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int ice_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int ice_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int ice_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);
void ice_rx_queue_release(void *rxq);
void ice_tx_queue_release(void *txq);
void ice_clear_queues(struct rte_eth_dev *dev);
void ice_free_queues(struct rte_eth_dev *dev);
uint16_t ice_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		       uint16_t nb_pkts);
uint16_t ice_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts);
void ice_set_rx_function(struct rte_eth_dev *dev);
uint16_t ice_prep_pkts(__rte_unused void *tx_queue, struct rte_mbuf **tx_pkts,
		       uint16_t nb_pkts);
void ice_set_tx_function_flag(struct rte_eth_dev *dev,
			      struct ice_tx_queue *txq);
void ice_set_tx_function(struct rte_eth_dev *dev);
uint32_t ice_rx_queue_count(struct rte_eth_dev *dev, uint16_t rx_queue_id);
void ice_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		      struct rte_eth_rxq_info *qinfo);
void ice_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		      struct rte_eth_txq_info *qinfo);
int ice_rx_descriptor_status(void *rx_queue, uint16_t offset);
int ice_tx_descriptor_status(void *tx_queue, uint16_t offset);
void ice_set_default_ptype_table(struct rte_eth_dev *dev);
const uint32_t *ice_dev_supported_ptypes_get(struct rte_eth_dev *dev);

int ice_rx_vec_dev_check(struct rte_eth_dev *dev);
int ice_tx_vec_dev_check(struct rte_eth_dev *dev);
int ice_rxq_vec_setup(struct ice_rx_queue *rxq);
int ice_txq_vec_setup(struct ice_tx_queue *txq);
uint16_t ice_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts);
uint16_t ice_recv_scattered_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
				     uint16_t nb_pkts);
uint16_t ice_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			   uint16_t nb_pkts);
uint16_t ice_recv_pkts_vec_avx2(void *rx_queue, struct rte_mbuf **rx_pkts,
				uint16_t nb_pkts);
uint16_t ice_recv_scattered_pkts_vec_avx2(void *rx_queue,
					  struct rte_mbuf **rx_pkts,
					  uint16_t nb_pkts);
uint16_t ice_xmit_pkts_vec_avx2(void *tx_queue, struct rte_mbuf **tx_pkts,
				uint16_t nb_pkts);
#endif /* _ICE_RXTX_H_ */
