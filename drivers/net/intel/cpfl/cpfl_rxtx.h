/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _CPFL_RXTX_H_
#define _CPFL_RXTX_H_

#include <idpf_common_rxtx.h>
#include "cpfl_ethdev.h"

/* In QLEN must be whole number of 32 descriptors. */
#define CPFL_ALIGN_RING_DESC	32
#define CPFL_MIN_RING_DESC	32
#define CPFL_MAX_RING_DESC	4096
#define CPFL_DMA_MEM_ALIGN	4096

#define CPFL_MAX_HAIRPINQ_RX_2_TX	1
#define CPFL_MAX_HAIRPINQ_TX_2_RX	1
#define CPFL_MAX_HAIRPINQ_NB_DESC	1024
#define CPFL_MAX_P2P_NB_QUEUES	16
#define CPFL_P2P_NB_RX_BUFQ	1
#define CPFL_P2P_NB_TX_COMPLQ	1
#define CPFL_P2P_NB_QUEUE_GRPS	1
#define CPFL_P2P_QUEUE_GRP_ID	1
#define CPFL_P2P_DESC_LEN	16
#define CPFL_P2P_NB_MBUF	4096
#define CPFL_P2P_CACHE_SIZE	250
#define CPFL_P2P_MBUF_SIZE	2048
#define CPFL_P2P_RING_BUF	128

/* Base address of the HW descriptor ring should be 128B aligned. */
#define CPFL_RING_BASE_ALIGN	128

#define CPFL_DEFAULT_RX_FREE_THRESH	32
#define CPFL_RXBUF_LOW_WATERMARK	64

#define CPFL_DEFAULT_TX_RS_THRESH	32
#define CPFL_DEFAULT_TX_FREE_THRESH	32

#define CPFL_SUPPORT_CHAIN_NUM 5

#define CPFL_RX_BUF_STRIDE 64

/* The value written in the RX buffer queue tail register,
 * and in WritePTR field in the TX completion queue context,
 * must be a multiple of 8.
 */
#define CPFL_HAIRPIN_Q_TAIL_AUX_VALUE 8

struct virtchnl2_p2p_rx_buf_desc {
	__le64  reserve0;
	__le64  pkt_addr; /* Packet buffer address */
};

struct cpfl_rxq_hairpin_info {
	bool hairpin_q;		/* if rx queue is a hairpin queue */
	uint16_t peer_txp;
	uint16_t peer_txq_id;
};

struct cpfl_rx_queue {
	struct idpf_rx_queue base;
	struct cpfl_rxq_hairpin_info hairpin_info;
};

struct cpfl_txq_hairpin_info {
	bool hairpin_q;		/* if tx queue is a hairpin queue */
	uint16_t peer_rxp;
	uint16_t peer_rxq_id;
};

struct cpfl_tx_queue {
	struct idpf_tx_queue base;
	struct cpfl_txq_hairpin_info hairpin_info;
};

static inline uint16_t
cpfl_hw_qid_get(uint16_t start_qid, uint16_t offset)
{
	return start_qid + offset;
}

static inline uint64_t
cpfl_hw_qtail_get(uint64_t tail_start, uint16_t offset, uint64_t tail_spacing)
{
	return tail_start + offset * tail_spacing;
}

int cpfl_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			uint16_t nb_desc, unsigned int socket_id,
			const struct rte_eth_txconf *tx_conf);
int cpfl_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
			uint16_t nb_desc, unsigned int socket_id,
			const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mp);
int cpfl_rx_queue_init(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int cpfl_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int cpfl_tx_queue_init(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int cpfl_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);
void cpfl_stop_queues(struct rte_eth_dev *dev);
int cpfl_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int cpfl_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);
void cpfl_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
void cpfl_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
void cpfl_set_rx_function(struct rte_eth_dev *dev);
void cpfl_set_tx_function(struct rte_eth_dev *dev);
int cpfl_rx_hairpin_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
				uint16_t nb_desc, const struct rte_eth_hairpin_conf *conf);
int cpfl_tx_hairpin_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
				uint16_t nb_desc,
				const struct rte_eth_hairpin_conf *conf);
int cpfl_hairpin_tx_complq_config(struct cpfl_vport *cpfl_vport);
int cpfl_hairpin_txq_config(struct idpf_vport *vport, struct cpfl_tx_queue *cpfl_txq);
int cpfl_hairpin_rx_bufq_config(struct cpfl_vport *cpfl_vport);
int cpfl_hairpin_rxq_config(struct idpf_vport *vport, struct cpfl_rx_queue *cpfl_rxq);
int cpfl_switch_hairpin_complq(struct cpfl_vport *cpfl_vport, bool on);
int cpfl_switch_hairpin_bufq(struct cpfl_vport *cpfl_vport, bool on);
int cpfl_switch_hairpin_rxtx_queue(struct cpfl_vport *cpfl_vport, uint16_t qid,
				   bool rx, bool on);
#endif /* _CPFL_RXTX_H_ */
