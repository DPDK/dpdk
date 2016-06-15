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

/*
 * TX Queues
 */

void bnxt_free_txq_stats(struct bnxt_tx_queue *txq)
{
	struct bnxt_cp_ring_info *cpr = txq->cp_ring;

	/* 'Unreserve' rte_memzone */

	if (cpr->hw_stats)
		cpr->hw_stats = NULL;
}

static void bnxt_tx_queue_release_mbufs(struct bnxt_tx_queue *txq __rte_unused)
{
	/* TODO: Requires interaction with TX ring */
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
		/* TODO: Free ring and stats here */
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

	if (!nb_desc || nb_desc > MAX_TX_DESC_CNT) {
		RTE_LOG(ERR, PMD, "nb_desc %d is invalid", nb_desc);
		return -EINVAL;
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
	if (txq == NULL) {
		RTE_LOG(ERR, PMD, "bnxt_tx_queue allocation failed!");
		return -ENOMEM;
	}
	txq->bp = bp;
	txq->nb_tx_desc = nb_desc;
	txq->tx_free_thresh = tx_conf->tx_free_thresh;

	/* TODO: Initialize ring structure */

	txq->queue_id = queue_idx;
	txq->port_id = eth_dev->data->port_id;

	/* TODO: Allocate TX ring hardware descriptors */

	/* TODO: Initialize the ring */

	eth_dev->data->tx_queues[queue_idx] = txq;
	return 0;
}
