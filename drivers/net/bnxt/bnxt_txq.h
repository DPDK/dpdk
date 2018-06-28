/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_TXQ_H_
#define _BNXT_TXQ_H_

struct bnxt_tx_ring_info;
struct bnxt_cp_ring_info;
struct bnxt_tx_queue {
	uint16_t		nb_tx_desc;    /* number of TX descriptors */
	uint16_t		tx_free_thresh;/* minimum TX before freeing */
	/** Index to last TX descriptor to have been cleaned. */
	uint16_t		last_desc_cleaned;
	/** Total number of TX descriptors ready to be allocated. */
	uint16_t		tx_next_dd; /* next desc to scan for DD bit */
	uint16_t		tx_next_rs; /* next desc to set RS bit */
	uint16_t		queue_id; /* TX queue index */
	uint16_t		reg_idx; /* TX queue register index */
	uint16_t		port_id; /* Device port identifier */
	uint8_t			pthresh; /* Prefetch threshold register */
	uint8_t			hthresh; /* Host threshold register */
	uint8_t			wthresh; /* Write-back threshold reg */
	uint32_t		ctx_curr; /* Hardware context states */
	uint8_t			tx_deferred_start; /* not in global dev start */
	uint8_t			cmpl_next; /* Next BD to trigger a compl */

	struct bnxt		*bp;
	int			index;
	int			tx_wake_thresh;
	struct bnxt_tx_ring_info	*tx_ring;

	unsigned int		cp_nr_rings;
	struct bnxt_cp_ring_info	*cp_ring;
	const struct rte_memzone *mz;
};

void bnxt_free_txq_stats(struct bnxt_tx_queue *txq);
void bnxt_free_tx_mbufs(struct bnxt *bp);
void bnxt_tx_queue_release_op(void *tx_queue);
int bnxt_tx_queue_setup_op(struct rte_eth_dev *eth_dev,
			       uint16_t queue_idx,
			       uint16_t nb_desc,
			       unsigned int socket_id,
			       const struct rte_eth_txconf *tx_conf);
#endif
