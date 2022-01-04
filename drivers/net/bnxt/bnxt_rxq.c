/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include <inttypes.h>

#include <rte_malloc.h>

#include "bnxt.h"
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

/* Determine whether the current configuration needs aggregation ring in HW. */
int bnxt_need_agg_ring(struct rte_eth_dev *eth_dev)
{
	/* scattered_rx will be true if OFFLOAD_SCATTER is enabled,
	 * if LRO is enabled, or if the max packet len is greater than the
	 * mbuf data size. So AGG ring will be needed whenever scattered_rx
	 * is set.
	 */
	return eth_dev->data->scattered_rx ? 1 : 0;
}

void bnxt_free_rxq_stats(struct bnxt_rx_queue *rxq)
{
	if (rxq && rxq->cp_ring && rxq->cp_ring->hw_stats)
		rxq->cp_ring->hw_stats = NULL;
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
	enum rte_eth_nb_pools pools = 1, max_pools = 0;
	struct bnxt_rx_queue *rxq;

	bp->nr_vnics = 0;

	/* Multi-queue mode */
	if (dev_conf->rxmode.mq_mode & RTE_ETH_MQ_RX_VMDQ_DCB_RSS) {
		/* VMDq ONLY, VMDq+RSS, VMDq+DCB, VMDq+DCB+RSS */

		switch (dev_conf->rxmode.mq_mode) {
		case RTE_ETH_MQ_RX_VMDQ_RSS:
		case RTE_ETH_MQ_RX_VMDQ_ONLY:
		case RTE_ETH_MQ_RX_VMDQ_DCB_RSS:
			/* FALLTHROUGH */
			/* ETH_8/64_POOLs */
			pools = conf->nb_queue_pools;
			/* For each pool, allocate MACVLAN CFA rule & VNIC */
			max_pools = RTE_MIN(bp->max_vnics,
					    RTE_MIN(bp->max_l2_ctx,
					    RTE_MIN(bp->max_rsscos_ctx,
						    RTE_ETH_64_POOLS)));
			PMD_DRV_LOG(DEBUG,
				    "pools = %u max_pools = %u\n",
				    pools, max_pools);
			if (pools > max_pools)
				pools = max_pools;
			break;
		case RTE_ETH_MQ_RX_RSS:
			pools = bp->rx_cosq_cnt ? bp->rx_cosq_cnt : 1;
			break;
		default:
			PMD_DRV_LOG(ERR, "Unsupported mq_mod %d\n",
				dev_conf->rxmode.mq_mode);
			rc = -EINVAL;
			goto err_out;
		}
	} else if (!dev_conf->rxmode.mq_mode) {
		pools = bp->rx_cosq_cnt ? bp->rx_cosq_cnt : pools;
	}

	pools = RTE_MIN(pools, bp->rx_cp_nr_rings);
	nb_q_per_grp = bp->rx_cp_nr_rings / pools;
	PMD_DRV_LOG(DEBUG, "pools = %u nb_q_per_grp = %u\n",
		    pools, nb_q_per_grp);
	start_grp_id = 0;
	end_grp_id = nb_q_per_grp;

	for (i = 0; i < pools; i++) {
		vnic = &bp->vnic_info[i];
		if (!vnic) {
			PMD_DRV_LOG(ERR, "VNIC alloc failed\n");
			rc = -ENOMEM;
			goto err_out;
		}
		vnic->flags |= BNXT_VNIC_INFO_BCAST;
		bp->nr_vnics++;

		for (j = 0; j < nb_q_per_grp; j++, ring_idx++) {
			rxq = bp->eth_dev->data->rx_queues[ring_idx];
			rxq->vnic = vnic;
			PMD_DRV_LOG(DEBUG,
				    "rxq[%d] = %p vnic[%d] = %p\n",
				    ring_idx, rxq, i, vnic);
		}
		if (i == 0) {
			if (dev_conf->rxmode.mq_mode & RTE_ETH_MQ_RX_VMDQ_DCB) {
				bp->eth_dev->data->promiscuous = 1;
				vnic->flags |= BNXT_VNIC_INFO_PROMISC;
			}
			vnic->func_default = true;
		}
		vnic->start_grp_id = start_grp_id;
		vnic->end_grp_id = end_grp_id;

		if (i) {
			if (dev_conf->rxmode.mq_mode & RTE_ETH_MQ_RX_VMDQ_DCB ||
			    !(dev_conf->rxmode.mq_mode & RTE_ETH_MQ_RX_RSS))
				vnic->rss_dflt_cr = true;
			goto skip_filter_allocation;
		}
		filter = bnxt_alloc_filter(bp);
		if (!filter) {
			PMD_DRV_LOG(ERR, "L2 filter alloc failed\n");
			rc = -ENOMEM;
			goto err_out;
		}
		filter->mac_index = 0;
		filter->flags |= HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_OUTERMOST;
		/*
		 * TODO: Configure & associate CFA rule for
		 * each VNIC for each VMDq with MACVLAN, MACVLAN+TC
		 */
		STAILQ_INSERT_TAIL(&vnic->filter, filter, next);

skip_filter_allocation:
		start_grp_id = end_grp_id;
		end_grp_id += nb_q_per_grp;
	}

	bp->rx_num_qs_per_vnic = nb_q_per_grp;

	if (dev_conf->rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) {
		struct rte_eth_rss_conf *rss = &bp->rss_conf;

		if (bp->flags & BNXT_FLAG_UPDATE_HASH)
			bp->flags &= ~BNXT_FLAG_UPDATE_HASH;

		for (i = 0; i < bp->nr_vnics; i++) {
			uint32_t lvl = RTE_ETH_RSS_LEVEL(rss->rss_hf);

			vnic = &bp->vnic_info[i];
			vnic->hash_type =
				bnxt_rte_to_hwrm_hash_types(rss->rss_hf);
			vnic->hash_mode =
				bnxt_rte_to_hwrm_hash_level(bp,
							    rss->rss_hf,
							    lvl);

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

	return rc;

err_out:
	/* Free allocated vnic/filters */

	return rc;
}

void bnxt_rx_queue_release_mbufs(struct bnxt_rx_queue *rxq)
{
	struct rte_mbuf **sw_ring;
	struct bnxt_tpa_info *tpa_info;
	uint16_t i;

	if (!rxq || !rxq->rx_ring)
		return;

	sw_ring = rxq->rx_ring->rx_buf_ring;
	if (sw_ring) {
#if defined(RTE_ARCH_X86) || defined(RTE_ARCH_ARM64)
		/*
		 * The vector receive burst function does not set used
		 * mbuf pointers to NULL, do that here to simplify
		 * cleanup logic.
		 */
		for (i = 0; i < rxq->rxrearm_nb; i++)
			sw_ring[rxq->rxrearm_start + i] = NULL;
		rxq->rxrearm_nb = 0;
#endif
		for (i = 0;
		     i < rxq->rx_ring->rx_ring_struct->ring_size; i++) {
			if (sw_ring[i]) {
				if (sw_ring[i] != &rxq->fake_mbuf)
					rte_pktmbuf_free_seg(sw_ring[i]);
				sw_ring[i] = NULL;
			}
		}
	}
	/* Free up mbufs in Agg ring */
	if (rxq->bp == NULL ||
	    rxq->bp->eth_dev == NULL ||
	    !bnxt_need_agg_ring(rxq->bp->eth_dev))
		return;

	sw_ring = rxq->rx_ring->ag_buf_ring;
	if (sw_ring) {
		for (i = 0;
		     i < rxq->rx_ring->ag_ring_struct->ring_size; i++) {
			if (sw_ring[i]) {
				rte_pktmbuf_free_seg(sw_ring[i]);
				sw_ring[i] = NULL;
			}
		}
	}

	/* Free up mbufs in TPA */
	tpa_info = rxq->rx_ring->tpa_info;
	if (tpa_info) {
		int max_aggs = BNXT_TPA_MAX_AGGS(rxq->bp);

		for (i = 0; i < max_aggs; i++) {
			if (tpa_info[i].mbuf) {
				rte_pktmbuf_free_seg(tpa_info[i].mbuf);
				tpa_info[i].mbuf = NULL;
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

void bnxt_free_rxq_mem(struct bnxt_rx_queue *rxq)
{
	bnxt_rx_queue_release_mbufs(rxq);

	/* Free RX, AGG ring hardware descriptors */
	if (rxq->rx_ring) {
		bnxt_free_ring(rxq->rx_ring->rx_ring_struct);
		rte_free(rxq->rx_ring->rx_ring_struct);
		rxq->rx_ring->rx_ring_struct = NULL;
		/* Free RX Agg ring hardware descriptors */
		bnxt_free_ring(rxq->rx_ring->ag_ring_struct);
		rte_free(rxq->rx_ring->ag_ring_struct);
		rxq->rx_ring->ag_ring_struct = NULL;

		rte_free(rxq->rx_ring);
		rxq->rx_ring = NULL;
	}
	/* Free RX completion ring hardware descriptors */
	if (rxq->cp_ring) {
		bnxt_free_ring(rxq->cp_ring->cp_ring_struct);
		rte_free(rxq->cp_ring->cp_ring_struct);
		rxq->cp_ring->cp_ring_struct = NULL;
		rte_free(rxq->cp_ring);
		rxq->cp_ring = NULL;
	}

	bnxt_free_rxq_stats(rxq);
	rte_memzone_free(rxq->mz);
	rxq->mz = NULL;
}

void bnxt_rx_queue_release_op(struct rte_eth_dev *dev, uint16_t queue_idx)
{
	struct bnxt_rx_queue *rxq = dev->data->rx_queues[queue_idx];

	if (rxq != NULL) {
		if (is_bnxt_in_error(rxq->bp))
			return;

		bnxt_free_hwrm_rx_ring(rxq->bp, rxq->queue_id);
		bnxt_free_rxq_mem(rxq);
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
	struct bnxt *bp = eth_dev->data->dev_private;
	uint64_t rx_offloads = eth_dev->data->dev_conf.rxmode.offloads;
	struct bnxt_rx_queue *rxq;
	int rc = 0;

	rc = is_bnxt_in_error(bp);
	if (rc)
		return rc;

	if (queue_idx >= bnxt_max_rings(bp)) {
		PMD_DRV_LOG(ERR,
			"Cannot create Rx ring %d. Only %d rings available\n",
			queue_idx, bp->max_rx_rings);
		return -EINVAL;
	}

	if (nb_desc < BNXT_MIN_RING_DESC || nb_desc > MAX_RX_DESC_CNT) {
		PMD_DRV_LOG(ERR, "nb_desc %d is invalid\n", nb_desc);
		return -EINVAL;
	}

	if (eth_dev->data->rx_queues) {
		rxq = eth_dev->data->rx_queues[queue_idx];
		if (rxq)
			bnxt_rx_queue_release_op(eth_dev, queue_idx);
	}
	rxq = rte_zmalloc_socket("bnxt_rx_queue", sizeof(struct bnxt_rx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq) {
		PMD_DRV_LOG(ERR, "bnxt_rx_queue allocation failed!\n");
		return -ENOMEM;
	}
	rxq->bp = bp;
	rxq->mb_pool = mp;
	rxq->nb_rx_desc = nb_desc;
	rxq->rx_free_thresh =
		RTE_MIN(rte_align32pow2(nb_desc) / 4, RTE_BNXT_MAX_RX_BURST);

	if (rx_conf->rx_drop_en != BNXT_DEFAULT_RX_DROP_EN)
		PMD_DRV_LOG(NOTICE,
			    "Per-queue config of drop-en is not supported.\n");
	rxq->drop_en = BNXT_DEFAULT_RX_DROP_EN;

	PMD_DRV_LOG(DEBUG, "RX Buf MTU %d\n", eth_dev->data->mtu);

	eth_dev->data->rx_queues[queue_idx] = rxq;

	rc = bnxt_init_rx_ring_struct(rxq, socket_id);
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "init_rx_ring_struct failed!\n");
		goto err;
	}

	PMD_DRV_LOG(DEBUG, "RX Buf size is %d\n", rxq->rx_buf_size);
	rxq->queue_id = queue_idx;
	rxq->port_id = eth_dev->data->port_id;
	if (rx_offloads & RTE_ETH_RX_OFFLOAD_KEEP_CRC)
		rxq->crc_len = RTE_ETHER_CRC_LEN;
	else
		rxq->crc_len = 0;

	/* Allocate RX ring hardware descriptors */
	rc = bnxt_alloc_rings(bp, socket_id, queue_idx, NULL, rxq, rxq->cp_ring,
			      NULL, "rxr");
	if (rc) {
		PMD_DRV_LOG(ERR,
			    "ring_dma_zone_reserve for rx_ring failed!\n");
		goto err;
	}
	rte_atomic64_init(&rxq->rx_mbuf_alloc_fail);

	/* rxq 0 must not be stopped when used as async CPR */
	if (!BNXT_NUM_ASYNC_CPR(bp) && queue_idx == 0)
		rxq->rx_deferred_start = false;
	else
		rxq->rx_deferred_start = rx_conf->rx_deferred_start;

	rxq->rx_started = rxq->rx_deferred_start ? false : true;
	rxq->vnic = BNXT_GET_DEFAULT_VNIC(bp);

	/* Configure mtu if it is different from what was configured before */
	if (!queue_idx)
		bnxt_mtu_set_op(eth_dev, eth_dev->data->mtu);

	return 0;
err:
	bnxt_rx_queue_release_op(eth_dev, queue_idx);
	return rc;
}

int
bnxt_rx_queue_intr_enable_op(struct rte_eth_dev *eth_dev, uint16_t queue_id)
{
	struct bnxt *bp = eth_dev->data->dev_private;
	struct bnxt_rx_queue *rxq;
	struct bnxt_cp_ring_info *cpr;
	int rc = 0;

	rc = is_bnxt_in_error(bp);
	if (rc)
		return rc;

	if (eth_dev->data->rx_queues) {
		rxq = eth_dev->data->rx_queues[queue_id];
		if (!rxq)
			return -EINVAL;

		cpr = rxq->cp_ring;
		B_CP_DB_REARM(cpr, cpr->cp_raw_cons);
	}
	return rc;
}

int
bnxt_rx_queue_intr_disable_op(struct rte_eth_dev *eth_dev, uint16_t queue_id)
{
	struct bnxt *bp = eth_dev->data->dev_private;
	struct bnxt_rx_queue *rxq;
	struct bnxt_cp_ring_info *cpr;
	int rc = 0;

	rc = is_bnxt_in_error(bp);
	if (rc)
		return rc;

	if (eth_dev->data->rx_queues) {
		rxq = eth_dev->data->rx_queues[queue_id];
		if (!rxq)
			return -EINVAL;

		cpr = rxq->cp_ring;
		B_CP_DB_DISARM(cpr);
	}
	return rc;
}

int bnxt_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct bnxt *bp = dev->data->dev_private;
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	struct bnxt_rx_queue *rxq = bp->rx_queues[rx_queue_id];
	struct bnxt_vnic_info *vnic = NULL;
	int rc = 0;

	rc = is_bnxt_in_error(bp);
	if (rc)
		return rc;

	if (rxq == NULL) {
		PMD_DRV_LOG(ERR, "Invalid Rx queue %d\n", rx_queue_id);
		return -EINVAL;
	}

	/* Set the queue state to started here.
	 * We check the status of the queue while posting buffer.
	 * If queue is it started, we do not post buffers for Rx.
	 */
	rxq->rx_started = true;
	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	bnxt_free_hwrm_rx_ring(bp, rx_queue_id);
	rc = bnxt_alloc_hwrm_rx_ring(bp, rx_queue_id);
	if (rc)
		return rc;

	if (BNXT_CHIP_P5(bp)) {
		/* Reconfigure default receive ring and MRU. */
		bnxt_hwrm_vnic_cfg(bp, rxq->vnic);
	}
	PMD_DRV_LOG(INFO, "Rx queue started %d\n", rx_queue_id);

	if (dev_conf->rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) {
		vnic = rxq->vnic;

		if (BNXT_HAS_RING_GRPS(bp)) {
			if (vnic->fw_grp_ids[rx_queue_id] != INVALID_HW_RING_ID)
				return 0;

			vnic->fw_grp_ids[rx_queue_id] =
					bp->grp_info[rx_queue_id].fw_grp_id;
			PMD_DRV_LOG(DEBUG,
				    "vnic = %p fw_grp_id = %d\n",
				    vnic, bp->grp_info[rx_queue_id].fw_grp_id);
		}

		PMD_DRV_LOG(DEBUG, "Rx Queue Count %d\n", vnic->rx_queue_cnt);
		rc = bnxt_vnic_rss_configure(bp, vnic);
	}

	if (rc != 0) {
		dev->data->rx_queue_state[rx_queue_id] =
				RTE_ETH_QUEUE_STATE_STOPPED;
		rxq->rx_started = false;
	}

	PMD_DRV_LOG(INFO,
		    "queue %d, rx_deferred_start %d, state %d!\n",
		    rx_queue_id, rxq->rx_deferred_start,
		    bp->eth_dev->data->rx_queue_state[rx_queue_id]);

	return rc;
}

int bnxt_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct bnxt *bp = dev->data->dev_private;
	struct rte_eth_conf *dev_conf = &bp->eth_dev->data->dev_conf;
	struct bnxt_vnic_info *vnic = NULL;
	struct bnxt_rx_queue *rxq = NULL;
	int active_queue_cnt = 0;
	int i, rc = 0;

	rc = is_bnxt_in_error(bp);
	if (rc)
		return rc;

	/* For the stingray platform and other platforms needing tighter
	 * control of resource utilization, Rx CQ 0 also works as
	 * Default CQ for async notifications
	 */
	if (!BNXT_NUM_ASYNC_CPR(bp) && !rx_queue_id) {
		PMD_DRV_LOG(ERR, "Cannot stop Rx queue id %d\n", rx_queue_id);
		return -EINVAL;
	}

	rxq = bp->rx_queues[rx_queue_id];
	if (!rxq) {
		PMD_DRV_LOG(ERR, "Invalid Rx queue %d\n", rx_queue_id);
		return -EINVAL;
	}

	vnic = rxq->vnic;
	if (!vnic) {
		PMD_DRV_LOG(ERR, "VNIC not initialized for RxQ %d\n",
			    rx_queue_id);
		return -EINVAL;
	}

	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;
	rxq->rx_started = false;
	PMD_DRV_LOG(DEBUG, "Rx queue stopped\n");

	if (dev_conf->rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) {
		if (BNXT_HAS_RING_GRPS(bp))
			vnic->fw_grp_ids[rx_queue_id] = INVALID_HW_RING_ID;

		PMD_DRV_LOG(DEBUG, "Rx Queue Count %d\n", vnic->rx_queue_cnt);
		rc = bnxt_vnic_rss_configure(bp, vnic);
	}

	/* Compute current number of active receive queues. */
	for (i = vnic->start_grp_id; i < vnic->end_grp_id; i++)
		if (bp->rx_queues[i]->rx_started)
			active_queue_cnt++;

	if (BNXT_CHIP_P5(bp)) {
		/*
		 * For Thor, we need to ensure that the VNIC default receive
		 * ring corresponds to an active receive queue. When no queue
		 * is active, we need to temporarily set the MRU to zero so
		 * that packets are dropped early in the receive pipeline in
		 * order to prevent the VNIC default receive ring from being
		 * accessed.
		 */
		if (active_queue_cnt == 0) {
			uint16_t saved_mru = vnic->mru;

			/* clear RSS setting on vnic. */
			bnxt_vnic_rss_clear_p5(bp, vnic);

			vnic->mru = 0;
			/* Reconfigure default receive ring and MRU. */
			bnxt_hwrm_vnic_cfg(bp, vnic);
			vnic->mru = saved_mru;
		} else {
			/* Reconfigure default receive ring. */
			bnxt_hwrm_vnic_cfg(bp, vnic);
		}
	} else if (active_queue_cnt) {
		/*
		 * If the queue being stopped is the current default queue and
		 * there are other active queues, pick one of them as the
		 * default and reconfigure the vnic.
		 */
		if (vnic->dflt_ring_grp == bp->grp_info[rx_queue_id].fw_grp_id) {
			for (i = vnic->start_grp_id; i < vnic->end_grp_id; i++) {
				if (bp->rx_queues[i]->rx_started) {
					vnic->dflt_ring_grp =
						bp->grp_info[i].fw_grp_id;
					bnxt_hwrm_vnic_cfg(bp, vnic);
					break;
				}
			}
		}
	}

	if (rc == 0)
		bnxt_rx_queue_release_mbufs(rxq);

	return rc;
}
