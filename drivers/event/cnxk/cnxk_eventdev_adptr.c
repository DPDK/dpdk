/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_ethdev.h"
#include "cnxk_eventdev.h"

void
cnxk_sso_updt_xae_cnt(struct cnxk_sso_evdev *dev, void *data,
		      uint32_t event_type)
{
	int i;

	switch (event_type) {
	case RTE_EVENT_TYPE_ETHDEV: {
		struct cnxk_eth_rxq_sp *rxq = data;
		uint64_t *old_ptr;

		for (i = 0; i < dev->rx_adptr_pool_cnt; i++) {
			if ((uint64_t)rxq->qconf.mp == dev->rx_adptr_pools[i])
				return;
		}

		dev->rx_adptr_pool_cnt++;
		old_ptr = dev->rx_adptr_pools;
		dev->rx_adptr_pools = rte_realloc(
			dev->rx_adptr_pools,
			sizeof(uint64_t) * dev->rx_adptr_pool_cnt, 0);
		if (dev->rx_adptr_pools == NULL) {
			dev->adptr_xae_cnt += rxq->qconf.mp->size;
			dev->rx_adptr_pools = old_ptr;
			dev->rx_adptr_pool_cnt--;
			return;
		}
		dev->rx_adptr_pools[dev->rx_adptr_pool_cnt - 1] =
			(uint64_t)rxq->qconf.mp;

		dev->adptr_xae_cnt += rxq->qconf.mp->size;
		break;
	}
	case RTE_EVENT_TYPE_TIMER: {
		struct cnxk_tim_ring *timr = data;
		uint16_t *old_ring_ptr;
		uint64_t *old_sz_ptr;

		for (i = 0; i < dev->tim_adptr_ring_cnt; i++) {
			if (timr->ring_id != dev->timer_adptr_rings[i])
				continue;
			if (timr->nb_timers == dev->timer_adptr_sz[i])
				return;
			dev->adptr_xae_cnt -= dev->timer_adptr_sz[i];
			dev->adptr_xae_cnt += timr->nb_timers;
			dev->timer_adptr_sz[i] = timr->nb_timers;

			return;
		}

		dev->tim_adptr_ring_cnt++;
		old_ring_ptr = dev->timer_adptr_rings;
		old_sz_ptr = dev->timer_adptr_sz;

		dev->timer_adptr_rings = rte_realloc(
			dev->timer_adptr_rings,
			sizeof(uint16_t) * dev->tim_adptr_ring_cnt, 0);
		if (dev->timer_adptr_rings == NULL) {
			dev->adptr_xae_cnt += timr->nb_timers;
			dev->timer_adptr_rings = old_ring_ptr;
			dev->tim_adptr_ring_cnt--;
			return;
		}

		dev->timer_adptr_sz = rte_realloc(
			dev->timer_adptr_sz,
			sizeof(uint64_t) * dev->tim_adptr_ring_cnt, 0);

		if (dev->timer_adptr_sz == NULL) {
			dev->adptr_xae_cnt += timr->nb_timers;
			dev->timer_adptr_sz = old_sz_ptr;
			dev->tim_adptr_ring_cnt--;
			return;
		}

		dev->timer_adptr_rings[dev->tim_adptr_ring_cnt - 1] =
			timr->ring_id;
		dev->timer_adptr_sz[dev->tim_adptr_ring_cnt - 1] =
			timr->nb_timers;

		dev->adptr_xae_cnt += timr->nb_timers;
		break;
	}
	default:
		break;
	}
}

static int
cnxk_sso_rxq_enable(struct cnxk_eth_dev *cnxk_eth_dev, uint16_t rq_id,
		    uint16_t port_id, const struct rte_event *ev,
		    uint8_t custom_flowid)
{
	struct roc_nix_rq *rq;

	rq = &cnxk_eth_dev->rqs[rq_id];
	rq->sso_ena = 1;
	rq->tt = ev->sched_type;
	rq->hwgrp = ev->queue_id;
	rq->flow_tag_width = 20;
	rq->wqe_skip = 1;
	rq->tag_mask = (port_id & 0xF) << 20;
	rq->tag_mask |= (((port_id >> 4) & 0xF) | (RTE_EVENT_TYPE_ETHDEV << 4))
			<< 24;

	if (custom_flowid) {
		rq->flow_tag_width = 0;
		rq->tag_mask |= ev->flow_id;
	}

	return roc_nix_rq_modify(&cnxk_eth_dev->nix, rq, 0);
}

static int
cnxk_sso_rxq_disable(struct cnxk_eth_dev *cnxk_eth_dev, uint16_t rq_id)
{
	struct roc_nix_rq *rq;

	rq = &cnxk_eth_dev->rqs[rq_id];
	rq->sso_ena = 0;
	rq->flow_tag_width = 32;
	rq->tag_mask = 0;

	return roc_nix_rq_modify(&cnxk_eth_dev->nix, rq, 0);
}

int
cnxk_sso_rx_adapter_queue_add(
	const struct rte_eventdev *event_dev, const struct rte_eth_dev *eth_dev,
	int32_t rx_queue_id,
	const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t port = eth_dev->data->port_id;
	struct cnxk_eth_rxq_sp *rxq_sp;
	int i, rc = 0;

	if (rx_queue_id < 0) {
		for (i = 0; i < eth_dev->data->nb_rx_queues; i++)
			rc |= cnxk_sso_rx_adapter_queue_add(event_dev, eth_dev,
							    i, queue_conf);
	} else {
		rxq_sp = cnxk_eth_rxq_to_sp(
			eth_dev->data->rx_queues[rx_queue_id]);
		cnxk_sso_updt_xae_cnt(dev, rxq_sp, RTE_EVENT_TYPE_ETHDEV);
		rc = cnxk_sso_xae_reconfigure(
			(struct rte_eventdev *)(uintptr_t)event_dev);
		rc |= cnxk_sso_rxq_enable(
			cnxk_eth_dev, (uint16_t)rx_queue_id, port,
			&queue_conf->ev,
			!!(queue_conf->rx_queue_flags &
			   RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID));
		rox_nix_fc_npa_bp_cfg(&cnxk_eth_dev->nix,
				      rxq_sp->qconf.mp->pool_id, true,
				      dev->force_ena_bp);
	}

	if (rc < 0) {
		plt_err("Failed to configure Rx adapter port=%d, q=%d", port,
			queue_conf->ev.queue_id);
		return rc;
	}

	dev->rx_offloads |= cnxk_eth_dev->rx_offload_flags;

	return 0;
}

int
cnxk_sso_rx_adapter_queue_del(const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev,
			      int32_t rx_queue_id)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct cnxk_eth_rxq_sp *rxq_sp;
	int i, rc = 0;

	RTE_SET_USED(event_dev);
	if (rx_queue_id < 0) {
		for (i = 0; i < eth_dev->data->nb_rx_queues; i++)
			cnxk_sso_rx_adapter_queue_del(event_dev, eth_dev, i);
	} else {
		rxq_sp = cnxk_eth_rxq_to_sp(
			eth_dev->data->rx_queues[rx_queue_id]);
		rc = cnxk_sso_rxq_disable(cnxk_eth_dev, (uint16_t)rx_queue_id);
		rox_nix_fc_npa_bp_cfg(&cnxk_eth_dev->nix,
				      rxq_sp->qconf.mp->pool_id, false,
				      dev->force_ena_bp);
	}

	if (rc < 0)
		plt_err("Failed to clear Rx adapter config port=%d, q=%d",
			eth_dev->data->port_id, rx_queue_id);

	return rc;
}

int
cnxk_sso_rx_adapter_start(const struct rte_eventdev *event_dev,
			  const struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(event_dev);
	RTE_SET_USED(eth_dev);

	return 0;
}

int
cnxk_sso_rx_adapter_stop(const struct rte_eventdev *event_dev,
			 const struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(event_dev);
	RTE_SET_USED(eth_dev);

	return 0;
}
