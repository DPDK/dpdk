/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>

#include <rte_malloc.h>

#include "bnxt.h"
#include "bnxt_ring.h"
#include "bnxt_txq.h"
#include "bnxt_txr.h"

/*
 * TX Queues
 */

void bnxt_free_txq_stats(struct bnxt_tx_queue *txq)
{
	if (txq && txq->cp_ring && txq->cp_ring->hw_stats)
		txq->cp_ring->hw_stats = NULL;
}

static void bnxt_tx_queue_release_mbufs(struct bnxt_tx_queue *txq)
{
	struct bnxt_sw_tx_bd *sw_ring;
	uint16_t i;

	if (!txq)
		return;

	sw_ring = txq->tx_ring->tx_buf_ring;
	if (sw_ring) {
		for (i = 0; i < txq->tx_ring->tx_ring_struct->ring_size; i++) {
			if (sw_ring[i].mbuf) {
				rte_pktmbuf_free_seg(sw_ring[i].mbuf);
				sw_ring[i].mbuf = NULL;
			}
		}
	}
}

void bnxt_free_tx_mbufs(struct bnxt *bp)
{
	struct bnxt_tx_queue *txq;
	int i;

	for (i = 0; i < (int)bp->tx_nr_rings; i++) {
		txq = bp->tx_queues[i];
		bnxt_tx_queue_release_mbufs(txq);
	}
}

void bnxt_tx_queue_release_op(void *tx_queue)
{
	struct bnxt_tx_queue *txq = (struct bnxt_tx_queue *)tx_queue;

	if (txq) {
		if (is_bnxt_in_error(txq->bp))
			return;

		/* Free TX ring hardware descriptors */
		bnxt_tx_queue_release_mbufs(txq);
		bnxt_free_ring(txq->tx_ring->tx_ring_struct);

		/* Free TX completion ring hardware descriptors */
		bnxt_free_ring(txq->cp_ring->cp_ring_struct);

		bnxt_free_txq_stats(txq);
		rte_memzone_free(txq->mz);
		txq->mz = NULL;

		rte_free(txq->free);
		rte_free(txq);
	}
}

int bnxt_tx_queue_setup_op(struct rte_eth_dev *eth_dev,
			       uint16_t queue_idx,
			       uint16_t nb_desc,
			       unsigned int socket_id,
			       const struct rte_eth_txconf *tx_conf)
{
	struct bnxt *bp = eth_dev->data->dev_private;
	struct bnxt_tx_queue *txq;
	int rc = 0;

	rc = is_bnxt_in_error(bp);
	if (rc)
		return rc;

	if (queue_idx >= BNXT_MAX_RINGS(bp)) {
		PMD_DRV_LOG(ERR,
			"Cannot create Tx ring %d. Only %d rings available\n",
			queue_idx, bp->max_tx_rings);
		return -EINVAL;
	}

	if (!nb_desc || nb_desc > MAX_TX_DESC_CNT) {
		PMD_DRV_LOG(ERR, "nb_desc %d is invalid", nb_desc);
		rc = -EINVAL;
		goto out;
	}

	if (eth_dev->data->tx_queues) {
		txq = eth_dev->data->tx_queues[queue_idx];
		if (txq) {
			bnxt_tx_queue_release_op(txq);
			txq = NULL;
		}
	}
	txq = rte_zmalloc_socket("bnxt_tx_queue", sizeof(struct bnxt_tx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!txq) {
		PMD_DRV_LOG(ERR, "bnxt_tx_queue allocation failed!");
		rc = -ENOMEM;
		goto out;
	}

	txq->free = rte_zmalloc_socket(NULL,
				       sizeof(struct rte_mbuf *) * nb_desc,
				       RTE_CACHE_LINE_SIZE, socket_id);
	if (!txq->free) {
		PMD_DRV_LOG(ERR, "allocation of tx mbuf free array failed!");
		rte_free(txq);
		rc = -ENOMEM;
		goto out;
	}
	txq->bp = bp;
	txq->nb_tx_desc = nb_desc;
	txq->tx_free_thresh = tx_conf->tx_free_thresh;
	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	rc = bnxt_init_tx_ring_struct(txq, socket_id);
	if (rc)
		goto out;

	txq->queue_id = queue_idx;
	txq->port_id = eth_dev->data->port_id;

	/* Allocate TX ring hardware descriptors */
	if (bnxt_alloc_rings(bp, queue_idx, txq, NULL, txq->cp_ring, NULL,
			     "txr")) {
		PMD_DRV_LOG(ERR, "ring_dma_zone_reserve for tx_ring failed!");
		bnxt_tx_queue_release_op(txq);
		rc = -ENOMEM;
		goto out;
	}

	if (bnxt_init_one_tx_ring(txq)) {
		PMD_DRV_LOG(ERR, "bnxt_init_one_tx_ring failed!");
		bnxt_tx_queue_release_op(txq);
		rc = -ENOMEM;
		goto out;
	}

	eth_dev->data->tx_queues[queue_idx] = txq;

	if (txq->tx_deferred_start)
		txq->tx_started = false;
	else
		txq->tx_started = true;
out:
	return rc;
}
