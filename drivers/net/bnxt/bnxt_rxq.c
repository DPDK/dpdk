/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
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
	const struct rte_eth_vmdq_rx_conf *conf =
		    &dev_conf->rx_adv_conf.vmdq_rx_conf;
	unsigned int i, j, nb_q_per_grp = 1, ring_idx = 0;
	int start_grp_id, end_grp_id = 1, rc = 0;
	struct bnxt_vnic_info *vnic;
	struct bnxt_filter_info *filter;
	enum rte_eth_nb_pools pools = bp->rx_cp_nr_rings, max_pools = 0;
	struct bnxt_rx_queue *rxq;

	bp->nr_vnics = 0;

	/* Single queue mode */
	if (bp->rx_cp_nr_rings < 2) {
		vnic = bnxt_alloc_vnic(bp);
		if (!vnic) {
			PMD_DRV_LOG(ERR, "VNIC alloc failed\n");
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
			PMD_DRV_LOG(ERR, "L2 filter alloc failed\n");
			rc = -ENOMEM;
			goto err_out;
		}
		STAILQ_INSERT_TAIL(&vnic->filter, filter, next);
		goto out;
	}

	/* Multi-queue mode */
	if (dev_conf->rxmode.mq_mode & ETH_MQ_RX_VMDQ_DCB_RSS) {
		/* VMDq ONLY, VMDq+RSS, VMDq+DCB, VMDq+DCB+RSS */

		switch (dev_conf->rxmode.mq_mode) {
		case ETH_MQ_RX_VMDQ_RSS:
		case ETH_MQ_RX_VMDQ_ONLY:
			/* ETH_8/64_POOLs */
			pools = conf->nb_queue_pools;
			/* For each pool, allocate MACVLAN CFA rule & VNIC */
			max_pools = RTE_MIN(bp->max_vnics,
					    RTE_MIN(bp->max_l2_ctx,
					    RTE_MIN(bp->max_rsscos_ctx,
						    ETH_64_POOLS)));
			if (pools > max_pools)
				pools = max_pools;
			break;
		case ETH_MQ_RX_RSS:
			pools = 1;
			break;
		default:
			PMD_DRV_LOG(ERR, "Unsupported mq_mod %d\n",
				dev_conf->rxmode.mq_mode);
			rc = -EINVAL;
			goto err_out;
		}
	}

	nb_q_per_grp = bp->rx_cp_nr_rings / pools;
	start_grp_id = 0;
	end_grp_id = nb_q_per_grp;

	for (i = 0; i < pools; i++) {
		vnic = bnxt_alloc_vnic(bp);
		if (!vnic) {
			PMD_DRV_LOG(ERR, "VNIC alloc failed\n");
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
		if (i == 0) {
			if (dev_conf->rxmode.mq_mode & ETH_MQ_RX_VMDQ_DCB) {
				bp->eth_dev->data->promiscuous = 1;
				vnic->flags |= BNXT_VNIC_INFO_PROMISC;
			}
			vnic->func_default = true;
		}
		vnic->ff_pool_idx = i;
		vnic->start_grp_id = start_grp_id;
		vnic->end_grp_id = end_grp_id;

		if (i) {
			if (dev_conf->rxmode.mq_mode & ETH_MQ_RX_VMDQ_DCB ||
			    !(dev_conf->rxmode.mq_mode & ETH_MQ_RX_RSS))
				vnic->rss_dflt_cr = true;
			goto skip_filter_allocation;
		}
		filter = bnxt_alloc_filter(bp);
		if (!filter) {
			PMD_DRV_LOG(ERR, "L2 filter alloc failed\n");
			rc = -ENOMEM;
			goto err_out;
		}
		/*
		 * TODO: Configure & associate CFA rule for
		 * each VNIC for each VMDq with MACVLAN, MACVLAN+TC
		 */
		STAILQ_INSERT_TAIL(&vnic->filter, filter, next);

skip_filter_allocation:
		start_grp_id = end_grp_id;
		end_grp_id += nb_q_per_grp;
	}

out:
	if (dev_conf->rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG) {
		struct rte_eth_rss_conf *rss = &dev_conf->rx_adv_conf.rss_conf;
		uint16_t hash_type = 0;

		if (bp->flags & BNXT_FLAG_UPDATE_HASH) {
			rss = &bp->rss_conf;
			bp->flags &= ~BNXT_FLAG_UPDATE_HASH;
		}

		if (rss->rss_hf & ETH_RSS_IPV4)
			hash_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV4;
		if (rss->rss_hf & ETH_RSS_NONFRAG_IPV4_TCP)
			hash_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV4;
		if (rss->rss_hf & ETH_RSS_NONFRAG_IPV4_UDP)
			hash_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV4;
		if (rss->rss_hf & ETH_RSS_IPV6)
			hash_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV6;
		if (rss->rss_hf & ETH_RSS_NONFRAG_IPV6_TCP)
			hash_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV6;
		if (rss->rss_hf & ETH_RSS_NONFRAG_IPV6_UDP)
			hash_type |= HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV6;

		for (i = 0; i < bp->nr_vnics; i++) {
			STAILQ_FOREACH(vnic, &bp->ff_pool[i], next) {
			vnic->hash_type = hash_type;

			/*
			 * Use the supplied key if the key length is
			 * acceptable and the rss_key is not NULL
			 */
			if (rss->rss_key &&
			    rss->rss_key_len <= HW_HASH_KEY_SIZE)
				memcpy(vnic->rss_hash_key,
				       rss->rss_key, rss->rss_key_len);
			}
		}
	}

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

	if (queue_idx >= bp->max_rx_rings) {
		PMD_DRV_LOG(ERR,
			"Cannot create Rx ring %d. Only %d rings available\n",
			queue_idx, bp->max_rx_rings);
		return -ENOSPC;
	}

	if (!nb_desc || nb_desc > MAX_RX_DESC_CNT) {
		PMD_DRV_LOG(ERR, "nb_desc %d is invalid\n", nb_desc);
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
		PMD_DRV_LOG(ERR, "bnxt_rx_queue allocation failed!\n");
		rc = -ENOMEM;
		goto out;
	}
	rxq->bp = bp;
	rxq->mb_pool = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh = rx_conf->rx_free_thresh;

	PMD_DRV_LOG(DEBUG, "RX Buf size is %d\n", rxq->rx_buf_use_size);
	PMD_DRV_LOG(DEBUG, "RX Buf MTU %d\n", eth_dev->data->mtu);

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
		PMD_DRV_LOG(ERR,
			"ring_dma_zone_reserve for rx_ring failed!\n");
		bnxt_rx_queue_release_op(rxq);
		rc = -ENOMEM;
		goto out;
	}

out:
	return rc;
}

int
bnxt_rx_queue_intr_enable_op(struct rte_eth_dev *eth_dev, uint16_t queue_id)
{
	struct bnxt_rx_queue *rxq;
	struct bnxt_cp_ring_info *cpr;
	int rc = 0;

	if (eth_dev->data->rx_queues) {
		rxq = eth_dev->data->rx_queues[queue_id];
		if (!rxq) {
			rc = -EINVAL;
			return rc;
		}
		cpr = rxq->cp_ring;
		B_CP_DB_ARM(cpr);
	}
	return rc;
}

int
bnxt_rx_queue_intr_disable_op(struct rte_eth_dev *eth_dev, uint16_t queue_id)
{
	struct bnxt_rx_queue *rxq;
	struct bnxt_cp_ring_info *cpr;
	int rc = 0;

	if (eth_dev->data->rx_queues) {
		rxq = eth_dev->data->rx_queues[queue_id];
		if (!rxq) {
			rc = -EINVAL;
			return rc;
		}
		cpr = rxq->cp_ring;
		B_CP_DB_DISARM(cpr);
	}
	return rc;
}

int bnxt_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	struct bnxt_rx_queue *rxq = bp->rx_queues[rx_queue_id];
	struct bnxt_vnic_info *vnic = NULL;

	if (rxq == NULL) {
		PMD_DRV_LOG(ERR, "Invalid Rx queue %d\n", rx_queue_id);
		return -EINVAL;
	}

	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;
	rxq->rx_deferred_start = false;
	PMD_DRV_LOG(INFO, "Rx queue started %d\n", rx_queue_id);
	if (dev_conf->rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG) {
		vnic = rxq->vnic;
		if (vnic->fw_grp_ids[rx_queue_id] != INVALID_HW_RING_ID)
			return 0;
		PMD_DRV_LOG(DEBUG, "vnic = %p fw_grp_id = %d\n",
			vnic, bp->grp_info[rx_queue_id + 1].fw_grp_id);
		vnic->fw_grp_ids[rx_queue_id] =
					bp->grp_info[rx_queue_id + 1].fw_grp_id;
		return bnxt_vnic_rss_configure(bp, vnic);
	}

	return 0;
}

int bnxt_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct bnxt *bp = (struct bnxt *)dev->data->dev_private;
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	struct bnxt_rx_queue *rxq = bp->rx_queues[rx_queue_id];
	struct bnxt_vnic_info *vnic = NULL;

	if (rxq == NULL) {
		PMD_DRV_LOG(ERR, "Invalid Rx queue %d\n", rx_queue_id);
		return -EINVAL;
	}

	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	rxq->rx_deferred_start = true;
	PMD_DRV_LOG(DEBUG, "Rx queue stopped\n");

	if (dev_conf->rxmode.mq_mode & ETH_MQ_RX_RSS_FLAG) {
		vnic = rxq->vnic;
		vnic->fw_grp_ids[rx_queue_id] = INVALID_HW_RING_ID;
		return bnxt_vnic_rss_configure(bp, vnic);
	}
	return 0;
}
