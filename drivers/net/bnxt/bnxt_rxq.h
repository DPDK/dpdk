/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_RQX_H_
#define _BNXT_RQX_H_

/* Maximum receive burst supported in vector mode. */
#define RTE_BNXT_MAX_RX_BURST		64U

/* Drop by default when receive desc is not available. */
#define BNXT_DEFAULT_RX_DROP_EN		1

struct bnxt;
struct bnxt_rx_ring_info;
struct bnxt_cp_ring_info;
struct bnxt_rx_queue {
	struct rte_mempool	*mb_pool; /* mbuf pool for RX ring */
	uint64_t		mbuf_initializer; /* val to init mbuf */
	uint16_t		nb_rx_desc; /* num of RX desc */
	uint16_t		rx_free_thresh; /* max free RX desc to hold */
	uint16_t		queue_id; /* RX queue index */
#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM64)
	uint16_t		rxrearm_nb; /* number of descs to reinit. */
	uint16_t		rxrearm_start; /* next desc index to reinit. */
#endif
	uint16_t		port_id; /* Device port identifier */
	uint8_t			crc_len; /* 0 if CRC stripped, 4 otherwise */
	uint8_t			rx_deferred_start; /* not in global dev start */
	uint8_t			rx_started; /* RX queue is started */
	uint8_t			drop_en; /* Drop when rx desc not available. */
	uint8_t			in_reset; /* Rx ring is scheduled for reset */

	struct bnxt		*bp;
	int			index;
	struct bnxt_vnic_info	*vnic;

	uint32_t			rx_buf_size;
	struct bnxt_rx_ring_info	*rx_ring;
	struct bnxt_cp_ring_info	*cp_ring;
	struct rte_mbuf			fake_mbuf;
	rte_atomic64_t		rx_mbuf_alloc_fail;
	const struct rte_memzone *mz;
};

void bnxt_free_rxq_stats(struct bnxt_rx_queue *rxq);
int bnxt_mq_rx_configure(struct bnxt *bp);
void bnxt_rx_queue_release_op(void *rx_queue);
int bnxt_rx_queue_setup_op(struct rte_eth_dev *eth_dev,
			       uint16_t queue_idx,
			       uint16_t nb_desc,
			       unsigned int socket_id,
			       const struct rte_eth_rxconf *rx_conf,
			       struct rte_mempool *mp);
void bnxt_free_rx_mbufs(struct bnxt *bp);
int bnxt_rx_queue_intr_enable_op(struct rte_eth_dev *eth_dev,
				 uint16_t queue_id);
int bnxt_rx_queue_intr_disable_op(struct rte_eth_dev *eth_dev,
				  uint16_t queue_id);
int bnxt_rx_queue_start(struct rte_eth_dev *dev,
			uint16_t rx_queue_id);
int bnxt_rx_queue_stop(struct rte_eth_dev *dev,
		       uint16_t rx_queue_id);
void bnxt_rx_queue_release_mbufs(struct bnxt_rx_queue *rxq);
#endif
