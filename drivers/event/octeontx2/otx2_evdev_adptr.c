/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019-2021 Marvell.
 */

#include "otx2_evdev.h"

#define NIX_RQ_AURA_THRESH(x) (((x)*95) / 100)

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
		*caps = RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT |
			RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ;

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
	aq->op = NIX_AQ_INSTOP_WRITE;

	aq->cq.ena = 1;
	aq->cq.caching = 1;

	otx2_mbox_memset(&aq->cq_mask, 0, sizeof(struct nix_cq_ctx_s));
	aq->cq_mask.ena = ~(aq->cq_mask.ena);
	aq->cq_mask.caching = ~(aq->cq_mask.caching);

	rc = otx2_mbox_process(mbox);
	if (rc < 0) {
		otx2_err("Failed to enable cq context");
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
	int i;

	switch (event_type) {
	case RTE_EVENT_TYPE_ETHDEV:
	{
		struct otx2_eth_rxq *rxq = data;
		uint64_t *old_ptr;

		for (i = 0; i < dev->rx_adptr_pool_cnt; i++) {
			if ((uint64_t)rxq->pool == dev->rx_adptr_pools[i])
				return;
		}

		dev->rx_adptr_pool_cnt++;
		old_ptr = dev->rx_adptr_pools;
		dev->rx_adptr_pools = rte_realloc(dev->rx_adptr_pools,
						  sizeof(uint64_t) *
						  dev->rx_adptr_pool_cnt, 0);
		if (dev->rx_adptr_pools == NULL) {
			dev->adptr_xae_cnt += rxq->pool->size;
			dev->rx_adptr_pools = old_ptr;
			dev->rx_adptr_pool_cnt--;
			return;
		}
		dev->rx_adptr_pools[dev->rx_adptr_pool_cnt - 1] =
			(uint64_t)rxq->pool;

		dev->adptr_xae_cnt += rxq->pool->size;
		break;
	}
	case RTE_EVENT_TYPE_TIMER:
	{
		struct otx2_tim_ring *timr = data;
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

		dev->timer_adptr_rings = rte_realloc(dev->timer_adptr_rings,
						     sizeof(uint16_t) *
						     dev->tim_adptr_ring_cnt,
						     0);
		if (dev->timer_adptr_rings == NULL) {
			dev->adptr_xae_cnt += timr->nb_timers;
			dev->timer_adptr_rings = old_ring_ptr;
			dev->tim_adptr_ring_cnt--;
			return;
		}

		dev->timer_adptr_sz = rte_realloc(dev->timer_adptr_sz,
						  sizeof(uint64_t) *
						  dev->tim_adptr_ring_cnt,
						  0);

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

static inline void
sso_cfg_nix_mp_bpid(struct otx2_sso_evdev *dev,
		    struct otx2_eth_dev *otx2_eth_dev, struct otx2_eth_rxq *rxq,
		    uint8_t ena)
{
	struct otx2_fc_info *fc = &otx2_eth_dev->fc_info;
	struct npa_aq_enq_req *req;
	struct npa_aq_enq_rsp *rsp;
	struct otx2_npa_lf *lf;
	struct otx2_mbox *mbox;
	uint32_t limit;
	int rc;

	if (otx2_dev_is_sdp(otx2_eth_dev))
		return;

	lf = otx2_npa_lf_obj_get();
	if (!lf)
		return;
	mbox = lf->mbox;

	req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);
	if (req == NULL)
		return;

	req->aura_id = npa_lf_aura_handle_to_aura(rxq->pool->pool_id);
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_READ;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return;

	limit = rsp->aura.limit;
	/* BP is already enabled. */
	if (rsp->aura.bp_ena) {
		/* If BP ids don't match disable BP. */
		if ((rsp->aura.nix0_bpid != fc->bpid[0]) && !dev->force_rx_bp) {
			req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);
			if (req == NULL)
				return;

			req->aura_id =
				npa_lf_aura_handle_to_aura(rxq->pool->pool_id);
			req->ctype = NPA_AQ_CTYPE_AURA;
			req->op = NPA_AQ_INSTOP_WRITE;

			req->aura.bp_ena = 0;
			req->aura_mask.bp_ena = ~(req->aura_mask.bp_ena);

			otx2_mbox_process(mbox);
		}
		return;
	}

	/* BP was previously enabled but now disabled skip. */
	if (rsp->aura.bp)
		return;

	req = otx2_mbox_alloc_msg_npa_aq_enq(mbox);
	if (req == NULL)
		return;

	req->aura_id = npa_lf_aura_handle_to_aura(rxq->pool->pool_id);
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_WRITE;

	if (ena) {
		req->aura.nix0_bpid = fc->bpid[0];
		req->aura_mask.nix0_bpid = ~(req->aura_mask.nix0_bpid);
		req->aura.bp = NIX_RQ_AURA_THRESH(
			limit > 128 ? 256 : limit); /* 95% of size*/
		req->aura_mask.bp = ~(req->aura_mask.bp);
	}

	req->aura.bp_ena = !!ena;
	req->aura_mask.bp_ena = ~(req->aura_mask.bp_ena);

	otx2_mbox_process(mbox);
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
			sso_cfg_nix_mp_bpid(dev, otx2_eth_dev, rxq, true);
			rc = sso_xae_reconfigure(
				(struct rte_eventdev *)(uintptr_t)event_dev);
			rc |= sso_rxq_enable(otx2_eth_dev, i,
					     queue_conf->ev.sched_type,
					     queue_conf->ev.queue_id, port);
		}
		rxq = eth_dev->data->rx_queues[0];
		sso_updt_lookup_mem(event_dev, rxq->lookup_mem);
	} else {
		rxq = eth_dev->data->rx_queues[rx_queue_id];
		sso_updt_xae_cnt(dev, rxq, RTE_EVENT_TYPE_ETHDEV);
		sso_cfg_nix_mp_bpid(dev, otx2_eth_dev, rxq, true);
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
	struct otx2_eth_dev *otx2_eth_dev = eth_dev->data->dev_private;
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	int i, rc;

	rc = strncmp(eth_dev->device->driver->name, "net_octeontx2", 13);
	if (rc)
		return -EINVAL;

	if (rx_queue_id < 0) {
		for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
			rc = sso_rxq_disable(otx2_eth_dev, i);
			sso_cfg_nix_mp_bpid(dev, otx2_eth_dev,
					    eth_dev->data->rx_queues[i], false);
		}
	} else {
		rc = sso_rxq_disable(otx2_eth_dev, (uint16_t)rx_queue_id);
		sso_cfg_nix_mp_bpid(dev, otx2_eth_dev,
				    eth_dev->data->rx_queues[rx_queue_id],
				    false);
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

int
otx2_sso_tx_adapter_caps_get(const struct rte_eventdev *dev,
			     const struct rte_eth_dev *eth_dev, uint32_t *caps)
{
	int ret;

	RTE_SET_USED(dev);
	ret = strncmp(eth_dev->device->driver->name, "net_octeontx2,", 13);
	if (ret)
		*caps = 0;
	else
		*caps = RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT;

	return 0;
}

static int
sso_sqb_aura_limit_edit(struct rte_mempool *mp, uint16_t nb_sqb_bufs)
{
	struct otx2_npa_lf *npa_lf = otx2_intra_dev_get_cfg()->npa_lf;
	struct npa_aq_enq_req *aura_req;

	aura_req = otx2_mbox_alloc_msg_npa_aq_enq(npa_lf->mbox);
	aura_req->aura_id = npa_lf_aura_handle_to_aura(mp->pool_id);
	aura_req->ctype = NPA_AQ_CTYPE_AURA;
	aura_req->op = NPA_AQ_INSTOP_WRITE;

	aura_req->aura.limit = nb_sqb_bufs;
	aura_req->aura_mask.limit = ~(aura_req->aura_mask.limit);

	return otx2_mbox_process(npa_lf->mbox);
}

static int
sso_add_tx_queue_data(const struct rte_eventdev *event_dev,
		      uint16_t eth_port_id, uint16_t tx_queue_id,
		      struct otx2_eth_txq *txq)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	int i;

	for (i = 0; i < event_dev->data->nb_ports; i++) {
		dev->max_port_id = RTE_MAX(dev->max_port_id, eth_port_id);
		if (dev->dual_ws) {
			struct otx2_ssogws_dual *old_dws;
			struct otx2_ssogws_dual *dws;

			old_dws = event_dev->data->ports[i];
			dws = rte_realloc_socket(ssogws_get_cookie(old_dws),
						 sizeof(struct otx2_ssogws_dual)
						 + RTE_CACHE_LINE_SIZE +
						 (sizeof(uint64_t) *
						    (dev->max_port_id + 1) *
						    RTE_MAX_QUEUES_PER_PORT),
						 RTE_CACHE_LINE_SIZE,
						 event_dev->data->socket_id);
			if (dws == NULL)
				return -ENOMEM;

			/* First cache line is reserved for cookie */
			dws = (struct otx2_ssogws_dual *)
				((uint8_t *)dws + RTE_CACHE_LINE_SIZE);

			((uint64_t (*)[RTE_MAX_QUEUES_PER_PORT]
			 )&dws->tx_adptr_data)[eth_port_id][tx_queue_id] =
				(uint64_t)txq;
			event_dev->data->ports[i] = dws;
		} else {
			struct otx2_ssogws *old_ws;
			struct otx2_ssogws *ws;

			old_ws = event_dev->data->ports[i];
			ws = rte_realloc_socket(ssogws_get_cookie(old_ws),
						sizeof(struct otx2_ssogws) +
						RTE_CACHE_LINE_SIZE +
						(sizeof(uint64_t) *
						 (dev->max_port_id + 1) *
						 RTE_MAX_QUEUES_PER_PORT),
						RTE_CACHE_LINE_SIZE,
						event_dev->data->socket_id);
			if (ws == NULL)
				return -ENOMEM;

			/* First cache line is reserved for cookie */
			ws = (struct otx2_ssogws *)
				((uint8_t *)ws + RTE_CACHE_LINE_SIZE);

			((uint64_t (*)[RTE_MAX_QUEUES_PER_PORT]
			 )&ws->tx_adptr_data)[eth_port_id][tx_queue_id] =
				(uint64_t)txq;
			event_dev->data->ports[i] = ws;
		}
	}

	return 0;
}

int
otx2_sso_tx_adapter_queue_add(uint8_t id, const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev,
			      int32_t tx_queue_id)
{
	struct otx2_eth_dev *otx2_eth_dev = eth_dev->data->dev_private;
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct otx2_eth_txq *txq;
	int i, ret;

	RTE_SET_USED(id);
	if (tx_queue_id < 0) {
		for (i = 0 ; i < eth_dev->data->nb_tx_queues; i++) {
			txq = eth_dev->data->tx_queues[i];
			sso_sqb_aura_limit_edit(txq->sqb_pool,
					OTX2_SSO_SQB_LIMIT);
			ret = sso_add_tx_queue_data(event_dev,
						    eth_dev->data->port_id, i,
						    txq);
			if (ret < 0)
				return ret;
		}
	} else {
		txq = eth_dev->data->tx_queues[tx_queue_id];
		sso_sqb_aura_limit_edit(txq->sqb_pool, OTX2_SSO_SQB_LIMIT);
		ret = sso_add_tx_queue_data(event_dev, eth_dev->data->port_id,
					    tx_queue_id, txq);
		if (ret < 0)
			return ret;
	}

	dev->tx_offloads |= otx2_eth_dev->tx_offload_flags;
	sso_fastpath_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);

	return 0;
}

int
otx2_sso_tx_adapter_queue_del(uint8_t id, const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev,
			      int32_t tx_queue_id)
{
	struct otx2_eth_txq *txq;
	int i;

	RTE_SET_USED(id);
	RTE_SET_USED(eth_dev);
	RTE_SET_USED(event_dev);
	if (tx_queue_id < 0) {
		for (i = 0 ; i < eth_dev->data->nb_tx_queues; i++) {
			txq = eth_dev->data->tx_queues[i];
			sso_sqb_aura_limit_edit(txq->sqb_pool,
						txq->nb_sqb_bufs);
		}
	} else {
		txq = eth_dev->data->tx_queues[tx_queue_id];
		sso_sqb_aura_limit_edit(txq->sqb_pool, txq->nb_sqb_bufs);
	}

	return 0;
}
