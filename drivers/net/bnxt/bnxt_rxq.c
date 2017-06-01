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
#include "bnxt_filter.h"
#include "bnxt_hwrm.h"
#include "bnxt_ring.h"
#include "bnxt_rxq.h"
#include "bnxt_rxr.h"
#include "bnxt_vnic.h"
#include "hsi_struct_def_dpdk.h"

/*
 * RX Queues
 */

void bnxt_free_rxq_stats(struct bnxt_rx_queue *rxq)
{
	struct bnxt_cp_ring_info *cpr = rxq->cp_ring;

	if (cpr->hw_stats)
		cpr->hw_stats = NULL;
}

int bnxt_mq_rx_configure(struct bnxt *bp)
{
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	unsigned int i, j, nb_q_per_grp, ring_idx;
	int start_grp_id, end_grp_id, rc = 0;
	struct bnxt_vnic_info *vnic;
	struct bnxt_filter_info *filter;
	struct bnxt_rx_queue *rxq;

	bp->nr_vnics = 0;

	/* Single queue mode */
	if (bp->rx_cp_nr_rings < 2) {
		vnic = bnxt_alloc_vnic(bp);
		if (!vnic) {
			RTE_LOG(ERR, PMD, "VNIC alloc failed\n");
			rc = -ENOMEM;
			goto err_out;
		}
		vnic->flags |= BNXT_VNIC_INFO_BCAST;
		STAILQ_INSERT_TAIL(&bp->ff_pool[0], vnic, next);
		bp->nr_vnics++;

		rxq = bp->eth_dev->data->rx_queues[0];
		rxq->vnic = vnic;

		vnic->func_default = true;
		vnic->ff_pool_idx = 0;
		vnic->start_grp_id = 0;
		vnic->end_grp_id = vnic->start_grp_id;
		filter = bnxt_alloc_filter(bp);
		if (!filter) {
			RTE_LOG(ERR, PMD, "L2 filter alloc failed\n");
			rc = -ENOMEM;
			goto err_out;
		}
		STAILQ_INSERT_TAIL(&vnic->filter, filter, next);
		goto out;
	}

	/* Multi-queue mode */
	if (dev_conf->rxmode.mq_mode & ETH_MQ_RX_VMDQ_FLAG) {
		/* VMDq ONLY, VMDq+RSS, VMDq+DCB, VMDq+DCB+RSS */
		enum rte_eth_nb_pools pools;

		switch (dev_conf->rxmode.mq_mode) {
		case ETH_MQ_RX_VMDQ_RSS:
		case ETH_MQ_RX_VMDQ_ONLY:
			{
				const struct rte_eth_vmdq_rx_conf *conf =
				    &dev_conf->rx_adv_conf.vmdq_rx_conf;

				/* ETH_8/64_POOLs */
				pools = conf->nb_queue_pools;
				break;
			}
		default:
			RTE_LOG(ERR, PMD, "Unsupported mq_mod %d\n",
				dev_conf->rxmode.mq_mode);
			rc = -EINVAL;
			goto err_out;
		}
		/* For each pool, allocate MACVLAN CFA rule & VNIC */
		if (!pools) {
			pools = RTE_MIN(bp->max_vnics,
			    RTE_MIN(bp->max_l2_ctx,
			     RTE_MIN(bp->max_rsscos_ctx, ETH_64_POOLS)));
			RTE_LOG(ERR, PMD,
				"VMDq pool not set, defaulted to 64\n");
			pools = ETH_64_POOLS;
		}
		nb_q_per_grp = bp->rx_cp_nr_rings / pools;
		start_grp_id = 0;
		end_grp_id = nb_q_per_grp;

		ring_idx = 0;
		for (i = 0; i < pools; i++) {
			vnic = bnxt_alloc_vnic(bp);
			if (!vnic) {
				RTE_LOG(ERR, PMD,
					"VNIC alloc failed\n");
				rc = -ENOMEM;
				goto err_out;
			}
			vnic->flags |= BNXT_VNIC_INFO_BCAST;
			STAILQ_INSERT_TAIL(&bp->ff_pool[i], vnic, next);
			bp->nr_vnics++;

			for (j = 0; j < nb_q_per_grp; j++, ring_idx++) {
				rxq = bp->eth_dev->data->rx_queues[ring_idx];
				rxq->vnic = vnic;
			}
			if (i == 0)
				vnic->func_default = true;
			vnic->ff_pool_idx = i;
			vnic->start_grp_id = start_grp_id;
			vnic->end_grp_id = end_grp_id;

			filter = bnxt_alloc_filter(bp);
			if (!filter) {
				RTE_LOG(ERR, PMD,
					"L2 filter alloc failed\n");
				rc = -ENOMEM;
				goto err_out;
			}
			/*
			 * TODO: Configure & associate CFA rule for
			 * each VNIC for each VMDq with MACVLAN, MACVLAN+TC
			 */
			STAILQ_INSERT_TAIL(&vnic->filter, filter, next);

			start_grp_id = end_grp_id + 1;
			end_grp_id += nb_q_per_grp;
		}
		goto out;
	}

	/* Non-VMDq mode - RSS, DCB, RSS+DCB */
	/* Init default VNIC for RSS or DCB only */
	vnic = bnxt_alloc_vnic(bp);
	if (!vnic) {
		RTE_LOG(ERR, PMD, "VNIC alloc failed\n");
		rc = -ENOMEM;
		goto err_out;
	}
	vnic->flags |= BNXT_VNIC_INFO_BCAST;
	/* Partition the rx queues for the single pool */
	for (i = 0; i < bp->rx_cp_nr_rings; i++) {
		rxq = bp->eth_dev->data->rx_queues[i];
		rxq->vnic = vnic;
	}
	STAILQ_INSERT_TAIL(&bp->ff_pool[0], vnic, next);
	bp->nr_vnics++;

	vnic->func_default = true;
	vnic->ff_pool_idx = 0;
	vnic->start_grp_id = 0;
	vnic->end_grp_id = bp->rx_cp_nr_rings;
	filter = bnxt_alloc_filter(bp);
	if (!filter) {
		RTE_LOG(ERR, PMD, "L2 filter alloc failed\n");
		rc = -ENOMEM;
		goto err_out;
	}
	STAILQ_INSERT_TAIL(&vnic->filter, filter, next);

	if (dev_conf->rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG)
		vnic->hash_type =
			HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV4 |
			HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV6;

out:
	return rc;

err_out:
	/* Free allocated vnic/filters */

	return rc;
}

static void bnxt_rx_queue_release_mbufs(struct bnxt_rx_queue *rxq)
{
	struct bnxt_sw_rx_bd *sw_ring;
	struct bnxt_tpa_info *tpa_info;
	uint16_t i;

	if (rxq) {
		sw_ring = rxq->rx_ring->rx_buf_ring;
		if (sw_ring) {
			for (i = 0; i < rxq->nb_rx_desc; i++) {
				if (sw_ring[i].mbuf) {
					rte_pktmbuf_free_seg(sw_ring[i].mbuf);
					sw_ring[i].mbuf = NULL;
				}
			}
		}
		/* Free up mbufs in Agg ring */
		sw_ring = rxq->rx_ring->ag_buf_ring;
		if (sw_ring) {
			for (i = 0; i < rxq->nb_rx_desc; i++) {
				if (sw_ring[i].mbuf) {
					rte_pktmbuf_free_seg(sw_ring[i].mbuf);
					sw_ring[i].mbuf = NULL;
				}
			}
		}

		/* Free up mbufs in TPA */
		tpa_info = rxq->rx_ring->tpa_info;
		if (tpa_info) {
			for (i = 0; i < BNXT_TPA_MAX; i++) {
				if (tpa_info[i].mbuf) {
					rte_pktmbuf_free_seg(tpa_info[i].mbuf);
					tpa_info[i].mbuf = NULL;
				}
			}
		}
	}
}

void bnxt_free_rx_mbufs(struct bnxt *bp)
{
	struct bnxt_rx_queue *rxq;
	int i;

	for (i = 0; i < (int)bp->rx_nr_rings; i++) {
		rxq = bp->rx_queues[i];
		bnxt_rx_queue_release_mbufs(rxq);
	}
}

void bnxt_rx_queue_release_op(void *rx_queue)
{
	struct bnxt_rx_queue *rxq = (struct bnxt_rx_queue *)rx_queue;

	if (rxq) {
		bnxt_rx_queue_release_mbufs(rxq);

		/* Free RX ring hardware descriptors */
		bnxt_free_ring(rxq->rx_ring->rx_ring_struct);
		/* Free RX Agg ring hardware descriptors */
		bnxt_free_ring(rxq->rx_ring->ag_ring_struct);

		/* Free RX completion ring hardware descriptors */
		bnxt_free_ring(rxq->cp_ring->cp_ring_struct);

		bnxt_free_rxq_stats(rxq);

		rte_free(rxq);
	}
}

int bnxt_rx_queue_setup_op(struct rte_eth_dev *eth_dev,
			       uint16_t queue_idx,
			       uint16_t nb_desc,
			       unsigned int socket_id,
			       const struct rte_eth_rxconf *rx_conf,
			       struct rte_mempool *mp)
{
	struct bnxt *bp = (struct bnxt *)eth_dev->data->dev_private;
	struct bnxt_rx_queue *rxq;
	int rc = 0;

	if (!nb_desc || nb_desc > MAX_RX_DESC_CNT) {
		RTE_LOG(ERR, PMD, "nb_desc %d is invalid\n", nb_desc);
		rc = -EINVAL;
		goto out;
	}

	if (eth_dev->data->rx_queues) {
		rxq = eth_dev->data->rx_queues[queue_idx];
		if (rxq)
			bnxt_rx_queue_release_op(rxq);
	}
	rxq = rte_zmalloc_socket("bnxt_rx_queue", sizeof(struct bnxt_rx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq) {
		RTE_LOG(ERR, PMD, "bnxt_rx_queue allocation failed!\n");
		rc = -ENOMEM;
		goto out;
	}
	rxq->bp = bp;
	rxq->mb_pool = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;

	RTE_LOG(DEBUG, PMD, "RX Buf size is %d\n", rxq->rx_buf_use_size);
	RTE_LOG(DEBUG, PMD, "RX Buf MTU %d\n", eth_dev->data->mtu);

	rc = bnxt_init_rx_ring_struct(rxq, socket_id);
	if (rc)
		goto out;

	rxq->queue_id = queue_idx;
	rxq->port_id = eth_dev->data->port_id;
	rxq->crc_len = (uint8_t)((eth_dev->data->dev_conf.rxmode.hw_strip_crc) ?
				0 : ETHER_CRC_LEN);

	eth_dev->data->rx_queues[queue_idx] = rxq;
	/* Allocate RX ring hardware descriptors */
	if (bnxt_alloc_rings(bp, queue_idx, NULL, rxq->rx_ring, rxq->cp_ring,
			"rxr")) {
		RTE_LOG(ERR, PMD,
			"ring_dma_zone_reserve for rx_ring failed!\n");
		bnxt_rx_queue_release_op(rxq);
		rc = -ENOMEM;
		goto out;
	}

out:
	return rc;
}
