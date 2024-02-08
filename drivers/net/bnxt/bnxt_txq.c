/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>

#include <rte_malloc.h>

#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_ring.h"
#include "bnxt_txq.h"
#include "bnxt_txr.h"

/*
 * TX Queues
 */

uint64_t bnxt_get_tx_port_offloads(struct bnxt *bp)
{
	uint64_t tx_offload_capa;

	tx_offload_capa = RTE_ETH_TX_OFFLOAD_IPV4_CKSUM  |
			  RTE_ETH_TX_OFFLOAD_UDP_CKSUM   |
			  RTE_ETH_TX_OFFLOAD_TCP_CKSUM   |
			  RTE_ETH_TX_OFFLOAD_TCP_TSO     |
			  RTE_ETH_TX_OFFLOAD_QINQ_INSERT |
			  RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

	if (bp->fw_cap & BNXT_FW_CAP_VLAN_TX_INSERT)
		tx_offload_capa |= RTE_ETH_TX_OFFLOAD_VLAN_INSERT;

	if (BNXT_TUNNELED_OFFLOADS_CAP_ALL_EN(bp))
		tx_offload_capa |= RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM;

	if (BNXT_TUNNELED_OFFLOADS_CAP_VXLAN_EN(bp))
		tx_offload_capa |= RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO;
	if (BNXT_TUNNELED_OFFLOADS_CAP_GRE_EN(bp))
		tx_offload_capa |= RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO;
	if (BNXT_TUNNELED_OFFLOADS_CAP_NGE_EN(bp))
		tx_offload_capa |= RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO;
	if (BNXT_TUNNELED_OFFLOADS_CAP_IPINIP_EN(bp))
		tx_offload_capa |= RTE_ETH_TX_OFFLOAD_IPIP_TNL_TSO;

	return tx_offload_capa;
}

void bnxt_free_txq_stats(struct bnxt_tx_queue *txq)
{
	if (txq && txq->cp_ring && txq->cp_ring->hw_stats)
		txq->cp_ring->hw_stats = NULL;
}

static void bnxt_tx_queue_release_mbufs(struct bnxt_tx_queue *txq)
{
	struct rte_mbuf **sw_ring;
	uint16_t i;

	if (!txq || !txq->tx_ring)
		return;

	sw_ring = txq->tx_ring->tx_buf_ring;
	if (sw_ring) {
		for (i = 0; i < txq->tx_ring->tx_ring_struct->ring_size; i++) {
			if (sw_ring[i]) {
				rte_pktmbuf_free_seg(sw_ring[i]);
				sw_ring[i] = NULL;
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

void bnxt_tx_queue_release_op(struct rte_eth_dev *dev, uint16_t queue_idx)
{
	struct bnxt_tx_queue *txq = dev->data->tx_queues[queue_idx];

	if (txq) {
		if (is_bnxt_in_error(txq->bp))
			return;

		/* Free TX ring hardware descriptors */
		bnxt_free_hwrm_tx_ring(txq->bp, txq->queue_id);
		bnxt_tx_queue_release_mbufs(txq);
		if (txq->tx_ring) {
			bnxt_free_ring(txq->tx_ring->tx_ring_struct);
			rte_free(txq->tx_ring->tx_ring_struct);
			rte_free(txq->tx_ring->nr_bds);
			rte_free(txq->tx_ring);
		}

		/* Free TX completion ring hardware descriptors */
		if (txq->cp_ring) {
			bnxt_free_ring(txq->cp_ring->cp_ring_struct);
			rte_free(txq->cp_ring->cp_ring_struct);
			rte_free(txq->cp_ring);
		}

		bnxt_free_txq_stats(txq);
		rte_memzone_free(txq->mz);
		txq->mz = NULL;

		rte_free(txq->free);
		pthread_mutex_destroy(&txq->txq_lock);
		rte_free(txq);
		dev->data->tx_queues[queue_idx] = NULL;
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

	if (queue_idx >= bnxt_max_rings(bp)) {
		PMD_DRV_LOG(ERR,
			"Cannot create Tx ring %d. Only %d rings available\n",
			queue_idx, bp->max_tx_rings);
		return -EINVAL;
	}

	if (nb_desc < BNXT_MIN_RING_DESC || nb_desc > MAX_TX_DESC_CNT) {
		PMD_DRV_LOG(ERR, "nb_desc %d is invalid", nb_desc);
		return -EINVAL;
	}

	if (eth_dev->data->tx_queues) {
		txq = eth_dev->data->tx_queues[queue_idx];
		if (txq)
			bnxt_tx_queue_release_op(eth_dev, queue_idx);
	}
	txq = rte_zmalloc_socket("bnxt_tx_queue", sizeof(struct bnxt_tx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!txq) {
		PMD_DRV_LOG(ERR, "bnxt_tx_queue allocation failed!");
		return -ENOMEM;
	}

	txq->bp = bp;
	eth_dev->data->tx_queues[queue_idx] = txq;

	txq->free = rte_zmalloc_socket(NULL,
				       sizeof(struct rte_mbuf *) * nb_desc,
				       RTE_CACHE_LINE_SIZE, socket_id);
	if (!txq->free) {
		PMD_DRV_LOG(ERR, "allocation of tx mbuf free array failed!");
		rc = -ENOMEM;
		goto err;
	}
	txq->nb_tx_desc = nb_desc;
	txq->tx_free_thresh =
		RTE_MIN(rte_align32pow2(nb_desc) / 4, RTE_BNXT_MAX_TX_BURST);
	txq->offloads = eth_dev->data->dev_conf.txmode.offloads |
			tx_conf->offloads;

	txq->tx_deferred_start = tx_conf->tx_deferred_start;

	rc = bnxt_init_tx_ring_struct(txq, socket_id);
	if (rc)
		goto err;

	txq->queue_id = queue_idx;
	txq->port_id = eth_dev->data->port_id;

	/* Allocate TX ring hardware descriptors */
	if (bnxt_alloc_rings(bp, socket_id, queue_idx, txq, NULL, txq->cp_ring,
			     NULL, "txr")) {
		PMD_DRV_LOG(ERR, "ring_dma_zone_reserve for tx_ring failed!");
		rc = -ENOMEM;
		goto err;
	}

	if (bnxt_init_one_tx_ring(txq)) {
		PMD_DRV_LOG(ERR, "bnxt_init_one_tx_ring failed!");
		rc = -ENOMEM;
		goto err;
	}

	rc = pthread_mutex_init(&txq->txq_lock, NULL);
	if (rc != 0) {
		PMD_DRV_LOG(ERR, "TxQ mutex init failed!");
		goto err;
	}
	return 0;
err:
	bnxt_tx_queue_release_op(eth_dev, queue_idx);
	return rc;
}
