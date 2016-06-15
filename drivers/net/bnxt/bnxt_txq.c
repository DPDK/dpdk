/*-
 *   BSD LICENSE
 *
 *   Copyright(c) Broadcom Limited.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Broadcom Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <inttypes.h>

#include <rte_malloc.h>

#include "bnxt.h"
#include "bnxt_cpr.h"
#include "bnxt_ring.h"
#include "bnxt_txq.h"
#include "bnxt_txr.h"

/*
 * TX Queues
 */

void bnxt_free_txq_stats(struct bnxt_tx_queue *txq)
{
	struct bnxt_cp_ring_info *cpr = txq->cp_ring;

	if (cpr->hw_stats)
		cpr->hw_stats = NULL;
}

static void bnxt_tx_queue_release_mbufs(struct bnxt_tx_queue *txq)
{
	struct bnxt_sw_tx_bd *sw_ring;
	uint16_t i;

	sw_ring = txq->tx_ring->tx_buf_ring;
	if (sw_ring) {
		for (i = 0; i < txq->tx_ring->tx_ring_struct->ring_size; i++) {
			if (sw_ring[i].mbuf) {
				rte_pktmbuf_free(sw_ring[i].mbuf);
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
		/* Free TX ring hardware descriptors */
		bnxt_tx_queue_release_mbufs(txq);
		bnxt_free_ring(txq->tx_ring->tx_ring_struct);

		/* Free TX completion ring hardware descriptors */
		bnxt_free_ring(txq->cp_ring->cp_ring_struct);

		bnxt_free_txq_stats(txq);

		rte_free(txq);
	}
}

int bnxt_tx_queue_setup_op(struct rte_eth_dev *eth_dev,
			       uint16_t queue_idx,
			       uint16_t nb_desc,
			       unsigned int socket_id,
			       const struct rte_eth_txconf *tx_conf)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_tx_queue *txq;
	int rc = 0;

	if (!nb_desc || nb_desc > MAX_TX_DESC_CNT) {
		RTE_LOG(ERR, PMD, "nb_desc %d is invalid", nb_desc);
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
		RTE_LOG(ERR, PMD, "bnxt_tx_queue allocation failed!");
		rc = -ENOMEM;
		goto out;
	}
	txq->bp = bp;
	txq->nb_tx_desc = nb_desc;
	txq->tx_free_thresh = tx_conf->tx_free_thresh;

	rc = bnxt_init_tx_ring_struct(txq, socket_id);
	if (rc)
		goto out;

	txq->queue_id = queue_idx;
	txq->port_id = eth_dev->data->port_id;

	/* Allocate TX ring hardware descriptors */
	if (bnxt_alloc_rings(bp, queue_idx, txq->tx_ring, NULL, txq->cp_ring,
			"txr")) {
		RTE_LOG(ERR, PMD, "ring_dma_zone_reserve for tx_ring failed!");
		bnxt_tx_queue_release_op(txq);
		rc = -ENOMEM;
		goto out;
	}

	if (bnxt_init_one_tx_ring(txq)) {
		RTE_LOG(ERR, PMD, "bnxt_init_one_tx_ring failed!");
		bnxt_tx_queue_release_op(txq);
		rc = -ENOMEM;
		goto out;
	}

	eth_dev->data->tx_queues[queue_idx] = txq;

out:
	return rc;
}
