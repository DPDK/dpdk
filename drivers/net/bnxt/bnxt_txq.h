/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_TXQ_H_
#define _BNXT_TXQ_H_

/* Maximum transmit burst for vector mode.  */
#define RTE_BNXT_MAX_TX_BURST		64U

struct bnxt_tx_ring_info;
struct bnxt_cp_ring_info;
struct bnxt_tx_queue {
	uint16_t		nb_tx_desc;    /* number of TX descriptors */
	uint16_t		tx_free_thresh;/* minimum TX before freeing */
	uint16_t		queue_id; /* TX queue index */
	uint16_t		port_id; /* Device port identifier */
	uint8_t			pthresh; /* Prefetch threshold register */
	uint8_t			hthresh; /* Host threshold register */
	uint8_t			wthresh; /* Write-back threshold reg */
	uint8_t			tx_deferred_start; /* not in global dev start */
	uint8_t			tx_started; /* TX queue is started */

	struct bnxt		*bp;
	int			index;
	int			tx_wake_thresh;
	uint32_t		vfr_tx_cfa_action;
	pthread_mutex_t		txq_lock;
	struct bnxt_tx_ring_info	*tx_ring;

	unsigned int		cp_nr_rings;
	struct bnxt_cp_ring_info	*cp_ring;
	const struct rte_memzone *mz;
	struct rte_mbuf **free;
	uint64_t offloads;
};

void bnxt_free_txq_stats(struct bnxt_tx_queue *txq);
void bnxt_free_tx_mbufs(struct bnxt *bp);
void bnxt_tx_queue_release_op(struct rte_eth_dev *dev, uint16_t queue_idx);
int bnxt_tx_queue_setup_op(struct rte_eth_dev *eth_dev,
			       uint16_t queue_idx,
			       uint16_t nb_desc,
			       unsigned int socket_id,
			       const struct rte_eth_txconf *tx_conf);
uint64_t bnxt_get_tx_port_offloads(struct bnxt *bp);
#endif
