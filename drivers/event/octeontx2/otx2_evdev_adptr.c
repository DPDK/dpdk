/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include "otx2_evdev.h"

int
otx2_sso_rx_adapter_caps_get(const struct rte_eventdev *event_dev,
			     const struct rte_eth_dev *eth_dev, uint32_t *caps)
{
	int rc;

	RTE_SET_USED(event_dev);
	rc = strncmp(eth_dev->device->driver->name, "net_octeontx2", 13);
	if (rc)
		*caps = RTE_EVENT_ETH_RX_ADAPTER_SW_CAP;
	else
		*caps = RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT;

	return 0;
}

static inline int
sso_rxq_enable(struct otx2_eth_dev *dev, uint16_t qid, uint8_t tt, uint8_t ggrp,
	       uint16_t eth_port_id)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_aq_enq_req *aq;
	int rc;

	aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = qid;
	aq->ctype = NIX_AQ_CTYPE_CQ;
	aq->op = NIX_AQ_INSTOP_WRITE;

	aq->cq.ena = 0;
	aq->cq.caching = 0;

	otx2_mbox_memset(&aq->cq_mask, 0, sizeof(struct nix_cq_ctx_s));
	aq->cq_mask.ena = ~(aq->cq_mask.ena);
	aq->cq_mask.caching = ~(aq->cq_mask.caching);

	rc = otx2_mbox_process(mbox);
	if (rc < 0) {
		otx2_err("Failed to disable cq context");
		goto fail;
	}

	aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = qid;
	aq->ctype = NIX_AQ_CTYPE_RQ;
	aq->op = NIX_AQ_INSTOP_WRITE;

	aq->rq.sso_ena = 1;
	aq->rq.sso_tt = tt;
	aq->rq.sso_grp = ggrp;
	aq->rq.ena_wqwd = 1;
	/* Mbuf Header generation :
	 * > FIRST_SKIP is a super set of WQE_SKIP, dont modify first skip as
	 * it already has data related to mbuf size, headroom, private area.
	 * > Using WQE_SKIP we can directly assign
	 *		mbuf = wqe - sizeof(struct mbuf);
	 * so that mbuf header will not have unpredicted values while headroom
	 * and private data starts at the beginning of wqe_data.
	 */
	aq->rq.wqe_skip = 1;
	aq->rq.wqe_caching = 1;
	aq->rq.spb_ena = 0;
	aq->rq.flow_tagw = 20; /* 20-bits */

	/* Flow Tag calculation :
	 *
	 * rq_tag <31:24> = good/bad_tag<8:0>;
	 * rq_tag  <23:0> = [ltag]
	 *
	 * flow_tag_mask<31:0> =  (1 << flow_tagw) - 1; <31:20>
	 * tag<31:0> = (~flow_tag_mask & rq_tag) | (flow_tag_mask & flow_tag);
	 *
	 * Setup :
	 * ltag<23:0> = (eth_port_id & 0xF) << 20;
	 * good/bad_tag<8:0> =
	 *	((eth_port_id >> 4) & 0xF) | (RTE_EVENT_TYPE_ETHDEV << 4);
	 *
	 * TAG<31:0> on getwork = <31:28>(RTE_EVENT_TYPE_ETHDEV) |
	 *				<27:20> (eth_port_id) | <20:0> [TAG]
	 */

	aq->rq.ltag = (eth_port_id & 0xF) << 20;
	aq->rq.good_utag = ((eth_port_id >> 4) & 0xF) |
				(RTE_EVENT_TYPE_ETHDEV << 4);
	aq->rq.bad_utag = aq->rq.good_utag;

	aq->rq.ena = 0;		 /* Don't enable RQ yet */
	aq->rq.pb_caching = 0x2; /* First cache aligned block to LLC */
	aq->rq.xqe_imm_size = 0; /* No pkt data copy to CQE */

	otx2_mbox_memset(&aq->rq_mask, 0, sizeof(struct nix_rq_ctx_s));
	/* mask the bits to write. */
	aq->rq_mask.sso_ena      = ~(aq->rq_mask.sso_ena);
	aq->rq_mask.sso_tt       = ~(aq->rq_mask.sso_tt);
	aq->rq_mask.sso_grp      = ~(aq->rq_mask.sso_grp);
	aq->rq_mask.ena_wqwd     = ~(aq->rq_mask.ena_wqwd);
	aq->rq_mask.wqe_skip     = ~(aq->rq_mask.wqe_skip);
	aq->rq_mask.wqe_caching  = ~(aq->rq_mask.wqe_caching);
	aq->rq_mask.spb_ena      = ~(aq->rq_mask.spb_ena);
	aq->rq_mask.flow_tagw    = ~(aq->rq_mask.flow_tagw);
	aq->rq_mask.ltag         = ~(aq->rq_mask.ltag);
	aq->rq_mask.good_utag    = ~(aq->rq_mask.good_utag);
	aq->rq_mask.bad_utag     = ~(aq->rq_mask.bad_utag);
	aq->rq_mask.ena          = ~(aq->rq_mask.ena);
	aq->rq_mask.pb_caching   = ~(aq->rq_mask.pb_caching);
	aq->rq_mask.xqe_imm_size = ~(aq->rq_mask.xqe_imm_size);

	rc = otx2_mbox_process(mbox);
	if (rc < 0) {
		otx2_err("Failed to init rx adapter context");
		goto fail;
	}

	return 0;
fail:
	return rc;
}

static inline int
sso_rxq_disable(struct otx2_eth_dev *dev, uint16_t qid)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_aq_enq_req *aq;
	int rc;

	aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = qid;
	aq->ctype = NIX_AQ_CTYPE_CQ;
	aq->op = NIX_AQ_INSTOP_INIT;

	aq->cq.ena = 1;
	aq->cq.caching = 1;

	otx2_mbox_memset(&aq->cq_mask, 0, sizeof(struct nix_cq_ctx_s));
	aq->cq_mask.ena = ~(aq->cq_mask.ena);
	aq->cq_mask.caching = ~(aq->cq_mask.caching);

	rc = otx2_mbox_process(mbox);
	if (rc < 0) {
		otx2_err("Failed to init cq context");
		goto fail;
	}

	aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = qid;
	aq->ctype = NIX_AQ_CTYPE_RQ;
	aq->op = NIX_AQ_INSTOP_WRITE;

	aq->rq.sso_ena = 0;
	aq->rq.sso_tt = SSO_TT_UNTAGGED;
	aq->rq.sso_grp = 0;
	aq->rq.ena_wqwd = 0;
	aq->rq.wqe_caching = 0;
	aq->rq.wqe_skip = 0;
	aq->rq.spb_ena = 0;
	aq->rq.flow_tagw = 0x20;
	aq->rq.ltag = 0;
	aq->rq.good_utag = 0;
	aq->rq.bad_utag = 0;
	aq->rq.ena = 1;
	aq->rq.pb_caching = 0x2; /* First cache aligned block to LLC */
	aq->rq.xqe_imm_size = 0; /* No pkt data copy to CQE */

	otx2_mbox_memset(&aq->rq_mask, 0, sizeof(struct nix_rq_ctx_s));
	/* mask the bits to write. */
	aq->rq_mask.sso_ena      = ~(aq->rq_mask.sso_ena);
	aq->rq_mask.sso_tt       = ~(aq->rq_mask.sso_tt);
	aq->rq_mask.sso_grp      = ~(aq->rq_mask.sso_grp);
	aq->rq_mask.ena_wqwd     = ~(aq->rq_mask.ena_wqwd);
	aq->rq_mask.wqe_caching  = ~(aq->rq_mask.wqe_caching);
	aq->rq_mask.wqe_skip     = ~(aq->rq_mask.wqe_skip);
	aq->rq_mask.spb_ena      = ~(aq->rq_mask.spb_ena);
	aq->rq_mask.flow_tagw    = ~(aq->rq_mask.flow_tagw);
	aq->rq_mask.ltag         = ~(aq->rq_mask.ltag);
	aq->rq_mask.good_utag    = ~(aq->rq_mask.good_utag);
	aq->rq_mask.bad_utag     = ~(aq->rq_mask.bad_utag);
	aq->rq_mask.ena          = ~(aq->rq_mask.ena);
	aq->rq_mask.pb_caching   = ~(aq->rq_mask.pb_caching);
	aq->rq_mask.xqe_imm_size = ~(aq->rq_mask.xqe_imm_size);

	rc = otx2_mbox_process(mbox);
	if (rc < 0) {
		otx2_err("Failed to clear rx adapter context");
		goto fail;
	}

	return 0;
fail:
	return rc;
}

void
sso_updt_xae_cnt(struct otx2_sso_evdev *dev, void *data, uint32_t event_type)
{
	switch (event_type) {
	case RTE_EVENT_TYPE_ETHDEV:
	{
		struct otx2_eth_rxq *rxq = data;
		int i, match = false;

		for (i = 0; i < dev->rx_adptr_pool_cnt; i++) {
			if ((uint64_t)rxq->pool == dev->rx_adptr_pools[i])
				match = true;
		}

		if (!match) {
			dev->rx_adptr_pool_cnt++;
			dev->rx_adptr_pools = rte_realloc(dev->rx_adptr_pools,
							  sizeof(uint64_t) *
							  dev->rx_adptr_pool_cnt
							  , 0);
			dev->rx_adptr_pools[dev->rx_adptr_pool_cnt - 1] =
				(uint64_t)rxq->pool;

			dev->adptr_xae_cnt += rxq->pool->size;
		}
		break;
	}
	case RTE_EVENT_TYPE_TIMER:
	{
		dev->adptr_xae_cnt += (*(uint64_t *)data);
		break;
	}
	default:
		break;
	}
}

static inline void
sso_updt_lookup_mem(const struct rte_eventdev *event_dev, void *lookup_mem)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	int i;

	for (i = 0; i < dev->nb_event_ports; i++) {
		if (dev->dual_ws) {
			struct otx2_ssogws_dual *ws = event_dev->data->ports[i];

			ws->lookup_mem = lookup_mem;
		} else {
			struct otx2_ssogws *ws = event_dev->data->ports[i];

			ws->lookup_mem = lookup_mem;
		}
	}
}

int
otx2_sso_rx_adapter_queue_add(const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev,
			      int32_t rx_queue_id,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct otx2_eth_dev *otx2_eth_dev = eth_dev->data->dev_private;
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uint16_t port = eth_dev->data->port_id;
	struct otx2_eth_rxq *rxq;
	int i, rc;

	rc = strncmp(eth_dev->device->driver->name, "net_octeontx2", 13);
	if (rc)
		return -EINVAL;

	if (rx_queue_id < 0) {
		for (i = 0 ; i < eth_dev->data->nb_rx_queues; i++) {
			rxq = eth_dev->data->rx_queues[i];
			sso_updt_xae_cnt(dev, rxq, RTE_EVENT_TYPE_ETHDEV);
			rc = sso_xae_reconfigure((struct rte_eventdev *)
						 (uintptr_t)event_dev);
			rc |= sso_rxq_enable(otx2_eth_dev, i,
					     queue_conf->ev.sched_type,
					     queue_conf->ev.queue_id, port);
		}
		rxq = eth_dev->data->rx_queues[0];
		sso_updt_lookup_mem(event_dev, rxq->lookup_mem);
	} else {
		rxq = eth_dev->data->rx_queues[rx_queue_id];
		sso_updt_xae_cnt(dev, rxq, RTE_EVENT_TYPE_ETHDEV);
		rc = sso_xae_reconfigure((struct rte_eventdev *)
					 (uintptr_t)event_dev);
		rc |= sso_rxq_enable(otx2_eth_dev, (uint16_t)rx_queue_id,
				     queue_conf->ev.sched_type,
				     queue_conf->ev.queue_id, port);
		sso_updt_lookup_mem(event_dev, rxq->lookup_mem);
	}

	if (rc < 0) {
		otx2_err("Failed to configure Rx adapter port=%d, q=%d", port,
			 queue_conf->ev.queue_id);
		return rc;
	}

	dev->rx_offloads |= otx2_eth_dev->rx_offload_flags;
	dev->tstamp = &otx2_eth_dev->tstamp;
	sso_fastpath_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);

	return 0;
}

int
otx2_sso_rx_adapter_queue_del(const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev,
			      int32_t rx_queue_id)
{
	struct otx2_eth_dev *dev = eth_dev->data->dev_private;
	int i, rc;

	RTE_SET_USED(event_dev);
	rc = strncmp(eth_dev->device->driver->name, "net_octeontx2", 13);
	if (rc)
		return -EINVAL;

	if (rx_queue_id < 0) {
		for (i = 0 ; i < eth_dev->data->nb_rx_queues; i++)
			rc = sso_rxq_disable(dev, i);
	} else {
		rc = sso_rxq_disable(dev, (uint16_t)rx_queue_id);
	}

	if (rc < 0)
		otx2_err("Failed to clear Rx adapter config port=%d, q=%d",
			 eth_dev->data->port_id, rx_queue_id);

	return rc;
}

int
otx2_sso_rx_adapter_start(const struct rte_eventdev *event_dev,
			  const struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(event_dev);
	RTE_SET_USED(eth_dev);

	return 0;
}

int
otx2_sso_rx_adapter_stop(const struct rte_eventdev *event_dev,
			 const struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(event_dev);
	RTE_SET_USED(eth_dev);

	return 0;
}
