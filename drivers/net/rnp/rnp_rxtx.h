/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_RXTX_H_
#define _RNP_RXTX_H_

#include "rnp.h"
#include "base/rnp_bdq_if.h"

#define RNP_RX_MAX_BURST_SIZE	(32)
#define RNP_TX_MAX_BURST_SIZE	(32)
#define RNP_BD_RING_ALIGN	(128)
#define RNP_MAX_RING_DESC	(4096)
#define RNP_RX_MAX_RING_SZ	\
	((RNP_MAX_RING_DESC + \
	  RNP_RX_MAX_BURST_SIZE) * \
	 sizeof(struct rnp_rx_desc))
#define RNP_TX_MAX_RING_SZ	\
	((RNP_MAX_RING_DESC + \
	  RNP_TX_MAX_BURST_SIZE) * \
	 sizeof(struct rnp_tx_desc))

#define RNP_RX_DESC_FETCH_TH		(96) /* dma fetch desC threshold */
#define RNP_RX_DESC_FETCH_BURST		(32) /* */
#define RNP_TX_DESC_FETCH_TH		(64) /* dma fetch desc threshold */
#define RNP_TX_DESC_FETCH_BURST		(32) /* max-num_descs_peer_read*/

#define RNP_DEFAULT_TX_FREE_THRESH   (32)
#define RNP_DEFAULT_TX_RS_THRESH     (32)
#define RNP_DEFAULT_RX_FREE_THRESH   (32)

/* rx queue info */
struct rnp_queue_attr {
	uint64_t sriov_st; /* enable sriov info */
	uint16_t vf_num; /* ring belong to which vf */

	uint16_t queue_id; /* sw queue index*/
	uint16_t index; /* hw ring index */
	uint16_t lane_id; /* ring belong to which physical Lane */
	uint16_t nb_desc; /* max bds */
	uint16_t nb_desc_mask; /* mask of bds */
	uint16_t port_id; /* dpdk manage port sequence id */
};

struct rnp_rxsw_entry {
	struct rte_mbuf *mbuf;
};

struct rnp_queue_stats {
	uint64_t obytes;
	uint64_t opackets;

	uint64_t ibytes;
	uint64_t ipackets;
	uint64_t errors;
};

struct rnp_rx_queue {
	struct rte_mempool *mb_pool; /* mbuf pool to populate rx ring. */
	const struct rte_memzone *rz; /* rx hw ring base alloc memzone */
	uint64_t ring_phys_addr; /* rx hw ring physical addr */
	volatile struct rnp_rx_desc *rx_bdr; /* rx hw ring rirtual Addr */
	volatile struct rnp_rx_desc zero_desc;
	struct rnp_rxsw_entry *sw_ring; /* rx software ring addr */
	volatile void *rx_tailreg; /* hw desc tail register */
	volatile void *rx_headreg; /* hw desc head register*/
	struct rnp_queue_attr attr;

	uint16_t rx_buf_len; /* mempool mbuf buf len */
	uint16_t nb_rx_free; /* number available use desc */
	uint16_t rx_free_thresh; /* rx free desc desource thresh */
	uint16_t rx_tail;

	uint16_t rxrearm_start;
	uint16_t rxrearm_nb;

	uint32_t nodesc_tm_thresh; /* rx queue no desc timeout thresh */
	uint8_t rx_deferred_start; /* do not start queue with dev_start(). */
	uint8_t rxq_started; /* rx queue is started */
	uint8_t rx_link; /* device link state */
	uint8_t pthresh; /* rx desc prefetch threshold */
	uint8_t pburst; /* rx desc prefetch burst */

	struct rnp_queue_stats stats;
	uint64_t rx_offloads; /* user set hw offload features */
	struct rte_mbuf **free_mbufs; /* rx bulk alloc reserve of free mbufs */
	struct rte_mbuf fake_mbuf; /* dummy mbuf */
	struct rte_mbuf	*pkt_first_seg; /**< First segment of current packet. */
	struct rte_mbuf *pkt_last_seg; /**< Last segment of current packet. */
};

struct rnp_txsw_entry {
	struct rte_mbuf *mbuf;  /* sync with tx desc dma physical addr */
	uint16_t next_id;       /* next entry resource used */
	uint16_t last_id;       /* last entry resource used */
};

struct rnp_tx_desc;
struct rnp_tx_queue {
	const struct rte_memzone *rz;
	uint64_t ring_phys_addr; /* tx dma ring physical addr */
	volatile struct rnp_tx_desc *tx_bdr; /* tx dma ring virtual addr */
	volatile struct rnp_tx_desc zero_desc;
	struct rnp_txsw_entry *sw_ring; /* tx software ring addr */
	volatile void *tx_tailreg; /* hw desc tail register */
	volatile void *tx_headreg; /* hw desc head register*/
	struct rnp_queue_attr attr;

	uint16_t nb_tx_free; /* avail desc to set pkts */
	uint16_t nb_tx_used; /* multiseg mbuf used num */
	uint16_t last_desc_cleaned;
	uint16_t tunnel_len;
	uint16_t tx_tail;

	uint16_t tx_next_dd; /* next to scan writeback dd bit */
	uint16_t tx_rs_thresh; /* number of interval set rs bit */
	uint16_t tx_next_rs; /* index of next time to set rs bit*/
	uint16_t tx_free_thresh; /* thresh to free tx desc resource */

	uint8_t tx_deferred_start; /*< Do not start queue with dev_start(). */
	uint8_t txq_started; /* tx queue is started */
	uint8_t tx_link; /* device link state */
	uint8_t pthresh; /* rx desc prefetch threshold */
	uint8_t pburst; /* rx desc burst*/

	struct rnp_queue_stats stats;
	uint64_t tx_offloads; /* tx offload features */
	struct rte_mbuf **free_mbufs; /* tx bulk free reserve of free mbufs */
};

void
rnp_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
void
rnp_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
int rnp_rx_queue_setup(struct rte_eth_dev *eth_dev,
		       uint16_t qidx,
		       uint16_t nb_rx_desc,
		       unsigned int socket_id,
		       const struct rte_eth_rxconf *rx_conf,
		       struct rte_mempool *mb_pool);
int rnp_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t qidx);
int rnp_tx_queue_start(struct rte_eth_dev *eth_dev, uint16_t qidx);
int rnp_tx_queue_setup(struct rte_eth_dev *dev,
		       uint16_t qidx, uint16_t nb_desc,
		       unsigned int socket_id,
		       const struct rte_eth_txconf *tx_conf);
int rnp_rx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t qidx);
int rnp_rx_queue_start(struct rte_eth_dev *eth_dev, uint16_t qidx);
int rnp_rx_func_select(struct rte_eth_dev *dev);
int rnp_tx_func_select(struct rte_eth_dev *dev);
void rnp_rx_queue_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
			   struct rte_eth_rxq_info *qinfo);
void rnp_tx_queue_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
			   struct rte_eth_txq_info *qinfo);
int rnp_rx_burst_mode_get(struct rte_eth_dev *dev,
			  __rte_unused uint16_t queue_id,
			  struct rte_eth_burst_mode *mode);
int rnp_tx_burst_mode_get(struct rte_eth_dev *dev,
			  __rte_unused uint16_t queue_id,
			  struct rte_eth_burst_mode *mode);

#endif /* _RNP_RXTX_H_ */
