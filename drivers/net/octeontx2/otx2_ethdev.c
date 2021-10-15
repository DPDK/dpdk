/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <inttypes.h>

#include <ethdev_pci.h>
#include <rte_io.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_mempool.h>

#include "otx2_ethdev.h"
#include "otx2_ethdev_sec.h"

static inline uint64_t
nix_get_rx_offload_capa(struct otx2_eth_dev *dev)
{
	uint64_t capa = NIX_RX_OFFLOAD_CAPA;

	if (otx2_dev_is_vf(dev) ||
	    dev->npc_flow.switch_header_type == OTX2_PRIV_FLAGS_HIGIG)
		capa &= ~RTE_ETH_RX_OFFLOAD_TIMESTAMP;

	return capa;
}

static inline uint64_t
nix_get_tx_offload_capa(struct otx2_eth_dev *dev)
{
	uint64_t capa = NIX_TX_OFFLOAD_CAPA;

	/* TSO not supported for earlier chip revisions */
	if (otx2_dev_is_96xx_A0(dev) || otx2_dev_is_95xx_Ax(dev))
		capa &= ~(RTE_ETH_TX_OFFLOAD_TCP_TSO |
			  RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
			  RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO |
			  RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO);
	return capa;
}

static const struct otx2_dev_ops otx2_dev_ops = {
	.link_status_update = otx2_eth_dev_link_status_update,
	.ptp_info_update = otx2_eth_dev_ptp_info_update,
	.link_status_get = otx2_eth_dev_link_status_get,
};

static int
nix_lf_alloc(struct otx2_eth_dev *dev, uint32_t nb_rxq, uint32_t nb_txq)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_lf_alloc_req *req;
	struct nix_lf_alloc_rsp *rsp;
	int rc;

	req = otx2_mbox_alloc_msg_nix_lf_alloc(mbox);
	req->rq_cnt = nb_rxq;
	req->sq_cnt = nb_txq;
	req->cq_cnt = nb_rxq;
	/* XQE_SZ should be in Sync with NIX_CQ_ENTRY_SZ */
	RTE_BUILD_BUG_ON(NIX_CQ_ENTRY_SZ != 128);
	req->xqe_sz = NIX_XQESZ_W16;
	req->rss_sz = dev->rss_info.rss_size;
	req->rss_grps = NIX_RSS_GRPS;
	req->npa_func = otx2_npa_pf_func_get();
	req->sso_func = otx2_sso_pf_func_get();
	req->rx_cfg = BIT_ULL(35 /* DIS_APAD */);
	if (dev->rx_offloads & (RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
			 RTE_ETH_RX_OFFLOAD_UDP_CKSUM)) {
		req->rx_cfg |= BIT_ULL(37 /* CSUM_OL4 */);
		req->rx_cfg |= BIT_ULL(36 /* CSUM_IL4 */);
	}
	req->rx_cfg |= (BIT_ULL(32 /* DROP_RE */)             |
			BIT_ULL(33 /* Outer L2 Length */)     |
			BIT_ULL(38 /* Inner L4 UDP Length */) |
			BIT_ULL(39 /* Inner L3 Length */)     |
			BIT_ULL(40 /* Outer L4 UDP Length */) |
			BIT_ULL(41 /* Outer L3 Length */));

	if (dev->rss_tag_as_xor == 0)
		req->flags = NIX_LF_RSS_TAG_LSB_AS_ADDER;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	dev->sqb_size = rsp->sqb_size;
	dev->tx_chan_base = rsp->tx_chan_base;
	dev->rx_chan_base = rsp->rx_chan_base;
	dev->rx_chan_cnt = rsp->rx_chan_cnt;
	dev->tx_chan_cnt = rsp->tx_chan_cnt;
	dev->lso_tsov4_idx = rsp->lso_tsov4_idx;
	dev->lso_tsov6_idx = rsp->lso_tsov6_idx;
	dev->lf_tx_stats = rsp->lf_tx_stats;
	dev->lf_rx_stats = rsp->lf_rx_stats;
	dev->cints = rsp->cints;
	dev->qints = rsp->qints;
	dev->npc_flow.channel = dev->rx_chan_base;
	dev->ptp_en = rsp->hw_rx_tstamp_en;

	return 0;
}

static int
nix_lf_switch_header_type_enable(struct otx2_eth_dev *dev, bool enable)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct npc_set_pkind *req;
	struct msg_resp *rsp;
	int rc;

	if (dev->npc_flow.switch_header_type == 0)
		return 0;

	/* Notify AF about higig2 config */
	req = otx2_mbox_alloc_msg_npc_set_pkind(mbox);
	req->mode = dev->npc_flow.switch_header_type;
	if (dev->npc_flow.switch_header_type == OTX2_PRIV_FLAGS_CH_LEN_90B) {
		req->mode = OTX2_PRIV_FLAGS_CUSTOM;
		req->pkind = NPC_RX_CHLEN90B_PKIND;
	} else if (dev->npc_flow.switch_header_type ==
		   OTX2_PRIV_FLAGS_CH_LEN_24B) {
		req->mode = OTX2_PRIV_FLAGS_CUSTOM;
		req->pkind = NPC_RX_CHLEN24B_PKIND;
	} else if (dev->npc_flow.switch_header_type ==
		   OTX2_PRIV_FLAGS_EXDSA) {
		req->mode = OTX2_PRIV_FLAGS_CUSTOM;
		req->pkind = NPC_RX_EXDSA_PKIND;
	} else if (dev->npc_flow.switch_header_type ==
		   OTX2_PRIV_FLAGS_VLAN_EXDSA) {
		req->mode = OTX2_PRIV_FLAGS_CUSTOM;
		req->pkind = NPC_RX_VLAN_EXDSA_PKIND;
	}

	if (enable == 0)
		req->mode = OTX2_PRIV_FLAGS_DEFAULT;
	req->dir = PKIND_RX;
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;
	req = otx2_mbox_alloc_msg_npc_set_pkind(mbox);
	req->mode = dev->npc_flow.switch_header_type;
	if (dev->npc_flow.switch_header_type == OTX2_PRIV_FLAGS_CH_LEN_90B ||
	    dev->npc_flow.switch_header_type == OTX2_PRIV_FLAGS_CH_LEN_24B)
		req->mode = OTX2_PRIV_FLAGS_DEFAULT;

	if (enable == 0)
		req->mode = OTX2_PRIV_FLAGS_DEFAULT;
	req->dir = PKIND_TX;
	return otx2_mbox_process_msg(mbox, (void *)&rsp);
}

static int
nix_lf_free(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_lf_free_req *req;
	struct ndc_sync_op *ndc_req;
	int rc;

	/* Sync NDC-NIX for LF */
	ndc_req = otx2_mbox_alloc_msg_ndc_sync_op(mbox);
	ndc_req->nix_lf_tx_sync = 1;
	ndc_req->nix_lf_rx_sync = 1;
	rc = otx2_mbox_process(mbox);
	if (rc)
		otx2_err("Error on NDC-NIX-[TX, RX] LF sync, rc %d", rc);

	req = otx2_mbox_alloc_msg_nix_lf_free(mbox);
	/* Let AF driver free all this nix lf's
	 * NPC entries allocated using NPC MBOX.
	 */
	req->flags = 0;

	return otx2_mbox_process(mbox);
}

int
otx2_cgx_rxtx_start(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;

	if (otx2_dev_is_vf_or_sdp(dev))
		return 0;

	otx2_mbox_alloc_msg_cgx_start_rxtx(mbox);

	return otx2_mbox_process(mbox);
}

int
otx2_cgx_rxtx_stop(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;

	if (otx2_dev_is_vf_or_sdp(dev))
		return 0;

	otx2_mbox_alloc_msg_cgx_stop_rxtx(mbox);

	return otx2_mbox_process(mbox);
}

static int
npc_rx_enable(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;

	otx2_mbox_alloc_msg_nix_lf_start_rx(mbox);

	return otx2_mbox_process(mbox);
}

static int
npc_rx_disable(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;

	otx2_mbox_alloc_msg_nix_lf_stop_rx(mbox);

	return otx2_mbox_process(mbox);
}

static int
nix_cgx_start_link_event(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;

	if (otx2_dev_is_vf_or_sdp(dev))
		return 0;

	otx2_mbox_alloc_msg_cgx_start_linkevents(mbox);

	return otx2_mbox_process(mbox);
}

static int
cgx_intlbk_enable(struct otx2_eth_dev *dev, bool en)
{
	struct otx2_mbox *mbox = dev->mbox;

	if (en && otx2_dev_is_vf_or_sdp(dev))
		return -ENOTSUP;

	if (en)
		otx2_mbox_alloc_msg_cgx_intlbk_enable(mbox);
	else
		otx2_mbox_alloc_msg_cgx_intlbk_disable(mbox);

	return otx2_mbox_process(mbox);
}

static int
nix_cgx_stop_link_event(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;

	if (otx2_dev_is_vf_or_sdp(dev))
		return 0;

	otx2_mbox_alloc_msg_cgx_stop_linkevents(mbox);

	return otx2_mbox_process(mbox);
}

static inline void
nix_rx_queue_reset(struct otx2_eth_rxq *rxq)
{
	rxq->head = 0;
	rxq->available = 0;
}

static inline uint32_t
nix_qsize_to_val(enum nix_q_size_e qsize)
{
	return (16UL << (qsize * 2));
}

static inline enum nix_q_size_e
nix_qsize_clampup_get(struct otx2_eth_dev *dev, uint32_t val)
{
	int i;

	if (otx2_ethdev_fixup_is_min_4k_q(dev))
		i = nix_q_size_4K;
	else
		i = nix_q_size_16;

	for (; i < nix_q_size_max; i++)
		if (val <= nix_qsize_to_val(i))
			break;

	if (i >= nix_q_size_max)
		i = nix_q_size_max - 1;

	return i;
}

static int
nix_cq_rq_init(struct rte_eth_dev *eth_dev, struct otx2_eth_dev *dev,
	       uint16_t qid, struct otx2_eth_rxq *rxq, struct rte_mempool *mp)
{
	struct otx2_mbox *mbox = dev->mbox;
	const struct rte_memzone *rz;
	uint32_t ring_size, cq_size;
	struct nix_aq_enq_req *aq;
	uint16_t first_skip;
	int rc;

	cq_size = rxq->qlen;
	ring_size = cq_size * NIX_CQ_ENTRY_SZ;
	rz = rte_eth_dma_zone_reserve(eth_dev, "cq", qid, ring_size,
				      NIX_CQ_ALIGN, dev->node);
	if (rz == NULL) {
		otx2_err("Failed to allocate mem for cq hw ring");
		return -ENOMEM;
	}
	memset(rz->addr, 0, rz->len);
	rxq->desc = (uintptr_t)rz->addr;
	rxq->qmask = cq_size - 1;

	aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = qid;
	aq->ctype = NIX_AQ_CTYPE_CQ;
	aq->op = NIX_AQ_INSTOP_INIT;

	aq->cq.ena = 1;
	aq->cq.caching = 1;
	aq->cq.qsize = rxq->qsize;
	aq->cq.base = rz->iova;
	aq->cq.avg_level = 0xff;
	aq->cq.cq_err_int_ena = BIT(NIX_CQERRINT_CQE_FAULT);
	aq->cq.cq_err_int_ena |= BIT(NIX_CQERRINT_DOOR_ERR);

	/* Many to one reduction */
	aq->cq.qint_idx = qid % dev->qints;
	/* Map CQ0 [RQ0] to CINT0 and so on till max 64 irqs */
	aq->cq.cint_idx = qid;

	if (otx2_ethdev_fixup_is_limit_cq_full(dev)) {
		const float rx_cq_skid = NIX_CQ_FULL_ERRATA_SKID;
		uint16_t min_rx_drop;

		min_rx_drop = ceil(rx_cq_skid / (float)cq_size);
		aq->cq.drop = min_rx_drop;
		aq->cq.drop_ena = 1;
		rxq->cq_drop = min_rx_drop;
	} else {
		rxq->cq_drop = NIX_CQ_THRESH_LEVEL;
		aq->cq.drop = rxq->cq_drop;
		aq->cq.drop_ena = 1;
	}

	/* TX pause frames enable flowctrl on RX side */
	if (dev->fc_info.tx_pause) {
		/* Single bpid is allocated for all rx channels for now */
		aq->cq.bpid = dev->fc_info.bpid[0];
		aq->cq.bp = rxq->cq_drop;
		aq->cq.bp_ena = 1;
	}

	rc = otx2_mbox_process(mbox);
	if (rc) {
		otx2_err("Failed to init cq context");
		return rc;
	}

	aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = qid;
	aq->ctype = NIX_AQ_CTYPE_RQ;
	aq->op = NIX_AQ_INSTOP_INIT;

	aq->rq.sso_ena = 0;

	if (rxq->offloads & RTE_ETH_RX_OFFLOAD_SECURITY)
		aq->rq.ipsech_ena = 1;

	aq->rq.cq = qid; /* RQ to CQ 1:1 mapped */
	aq->rq.spb_ena = 0;
	aq->rq.lpb_aura = npa_lf_aura_handle_to_aura(mp->pool_id);
	first_skip = (sizeof(struct rte_mbuf));
	first_skip += RTE_PKTMBUF_HEADROOM;
	first_skip += rte_pktmbuf_priv_size(mp);
	rxq->data_off = first_skip;

	first_skip /= 8; /* Expressed in number of dwords */
	aq->rq.first_skip = first_skip;
	aq->rq.later_skip = (sizeof(struct rte_mbuf) / 8);
	aq->rq.flow_tagw = 32; /* 32-bits */
	aq->rq.lpb_sizem1 = mp->elt_size / 8;
	aq->rq.lpb_sizem1 -= 1; /* Expressed in size minus one */
	aq->rq.ena = 1;
	aq->rq.pb_caching = 0x2; /* First cache aligned block to LLC */
	aq->rq.xqe_imm_size = 0; /* No pkt data copy to CQE */
	aq->rq.rq_int_ena = 0;
	/* Many to one reduction */
	aq->rq.qint_idx = qid % dev->qints;

	aq->rq.xqe_drop_ena = 1;

	rc = otx2_mbox_process(mbox);
	if (rc) {
		otx2_err("Failed to init rq context");
		return rc;
	}

	if (dev->lock_rx_ctx) {
		aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
		aq->qidx = qid;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_LOCK;

		aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
		if (!aq) {
			/* The shared memory buffer can be full.
			 * Flush it and retry
			 */
			otx2_mbox_msg_send(mbox, 0);
			rc = otx2_mbox_wait_for_rsp(mbox, 0);
			if (rc < 0) {
				otx2_err("Failed to LOCK cq context");
				return rc;
			}

			aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
			if (!aq) {
				otx2_err("Failed to LOCK rq context");
				return -ENOMEM;
			}
		}
		aq->qidx = qid;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_LOCK;
		rc = otx2_mbox_process(mbox);
		if (rc < 0) {
			otx2_err("Failed to LOCK rq context");
			return rc;
		}
	}

	return 0;
}

static int
nix_rq_enb_dis(struct rte_eth_dev *eth_dev,
	       struct otx2_eth_rxq *rxq, const bool enb)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_aq_enq_req *aq;

	/* Pkts will be dropped silently if RQ is disabled */
	aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = rxq->rq;
	aq->ctype = NIX_AQ_CTYPE_RQ;
	aq->op = NIX_AQ_INSTOP_WRITE;

	aq->rq.ena = enb;
	aq->rq_mask.ena = ~(aq->rq_mask.ena);

	return otx2_mbox_process(mbox);
}

static int
nix_cq_rq_uninit(struct rte_eth_dev *eth_dev, struct otx2_eth_rxq *rxq)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_aq_enq_req *aq;
	int rc;

	/* RQ is already disabled */
	/* Disable CQ */
	aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = rxq->rq;
	aq->ctype = NIX_AQ_CTYPE_CQ;
	aq->op = NIX_AQ_INSTOP_WRITE;

	aq->cq.ena = 0;
	aq->cq_mask.ena = ~(aq->cq_mask.ena);

	rc = otx2_mbox_process(mbox);
	if (rc < 0) {
		otx2_err("Failed to disable cq context");
		return rc;
	}

	if (dev->lock_rx_ctx) {
		aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
		aq->qidx = rxq->rq;
		aq->ctype = NIX_AQ_CTYPE_CQ;
		aq->op = NIX_AQ_INSTOP_UNLOCK;

		aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
		if (!aq) {
			/* The shared memory buffer can be full.
			 * Flush it and retry
			 */
			otx2_mbox_msg_send(mbox, 0);
			rc = otx2_mbox_wait_for_rsp(mbox, 0);
			if (rc < 0) {
				otx2_err("Failed to UNLOCK cq context");
				return rc;
			}

			aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
			if (!aq) {
				otx2_err("Failed to UNLOCK rq context");
				return -ENOMEM;
			}
		}
		aq->qidx = rxq->rq;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_UNLOCK;
		rc = otx2_mbox_process(mbox);
		if (rc < 0) {
			otx2_err("Failed to UNLOCK rq context");
			return rc;
		}
	}

	return 0;
}

static inline int
nix_get_data_off(struct otx2_eth_dev *dev)
{
	return otx2_ethdev_is_ptp_en(dev) ? NIX_TIMESYNC_RX_OFFSET : 0;
}

uint64_t
otx2_nix_rxq_mbuf_setup(struct otx2_eth_dev *dev, uint16_t port_id)
{
	struct rte_mbuf mb_def;
	uint64_t *tmp;

	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_off) % 8 != 0);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, refcnt) -
				offsetof(struct rte_mbuf, data_off) != 2);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, nb_segs) -
				offsetof(struct rte_mbuf, data_off) != 4);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, port) -
				offsetof(struct rte_mbuf, data_off) != 6);
	mb_def.nb_segs = 1;
	mb_def.data_off = RTE_PKTMBUF_HEADROOM + nix_get_data_off(dev);
	mb_def.port = port_id;
	rte_mbuf_refcnt_set(&mb_def, 1);

	/* Prevent compiler reordering: rearm_data covers previous fields */
	rte_compiler_barrier();
	tmp = (uint64_t *)&mb_def.rearm_data;

	return *tmp;
}

static void
otx2_nix_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct otx2_eth_rxq *rxq = dev->data->rx_queues[qid];

	if (!rxq)
		return;

	otx2_nix_dbg("Releasing rxq %u", rxq->rq);
	nix_cq_rq_uninit(rxq->eth_dev, rxq);
	rte_free(rxq);
	dev->data->rx_queues[qid] = NULL;
}

static int
otx2_nix_rx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t rq,
			uint16_t nb_desc, unsigned int socket,
			const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mp)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct rte_mempool_ops *ops;
	struct otx2_eth_rxq *rxq;
	const char *platform_ops;
	enum nix_q_size_e qsize;
	uint64_t offloads;
	int rc;

	rc = -EINVAL;

	/* Compile time check to make sure all fast path elements in a CL */
	RTE_BUILD_BUG_ON(offsetof(struct otx2_eth_rxq, slow_path_start) >= 128);

	/* Sanity checks */
	if (rx_conf->rx_deferred_start == 1) {
		otx2_err("Deferred Rx start is not supported");
		goto fail;
	}

	platform_ops = rte_mbuf_platform_mempool_ops();
	/* This driver needs octeontx2_npa mempool ops to work */
	ops = rte_mempool_get_ops(mp->ops_index);
	if (strncmp(ops->name, platform_ops, RTE_MEMPOOL_OPS_NAMESIZE)) {
		otx2_err("mempool ops should be of octeontx2_npa type");
		goto fail;
	}

	if (mp->pool_id == 0) {
		otx2_err("Invalid pool_id");
		goto fail;
	}

	/* Free memory prior to re-allocation if needed */
	if (eth_dev->data->rx_queues[rq] != NULL) {
		otx2_nix_dbg("Freeing memory prior to re-allocation %d", rq);
		otx2_nix_rx_queue_release(eth_dev, rq);
		rte_eth_dma_zone_free(eth_dev, "cq", rq);
	}

	offloads = rx_conf->offloads | eth_dev->data->dev_conf.rxmode.offloads;
	dev->rx_offloads |= offloads;

	/* Find the CQ queue size */
	qsize = nix_qsize_clampup_get(dev, nb_desc);
	/* Allocate rxq memory */
	rxq = rte_zmalloc_socket("otx2 rxq", sizeof(*rxq), OTX2_ALIGN, socket);
	if (rxq == NULL) {
		otx2_err("Failed to allocate rq=%d", rq);
		rc = -ENOMEM;
		goto fail;
	}

	rxq->eth_dev = eth_dev;
	rxq->rq = rq;
	rxq->cq_door = dev->base + NIX_LF_CQ_OP_DOOR;
	rxq->cq_status = (int64_t *)(dev->base + NIX_LF_CQ_OP_STATUS);
	rxq->wdata = (uint64_t)rq << 32;
	rxq->aura = npa_lf_aura_handle_to_aura(mp->pool_id);
	rxq->mbuf_initializer = otx2_nix_rxq_mbuf_setup(dev,
							eth_dev->data->port_id);
	rxq->offloads = offloads;
	rxq->pool = mp;
	rxq->qlen = nix_qsize_to_val(qsize);
	rxq->qsize = qsize;
	rxq->lookup_mem = otx2_nix_fastpath_lookup_mem_get();
	rxq->tstamp = &dev->tstamp;

	eth_dev->data->rx_queues[rq] = rxq;

	/* Alloc completion queue */
	rc = nix_cq_rq_init(eth_dev, dev, rq, rxq, mp);
	if (rc) {
		otx2_err("Failed to allocate rxq=%u", rq);
		goto free_rxq;
	}

	rxq->qconf.socket_id = socket;
	rxq->qconf.nb_desc = nb_desc;
	rxq->qconf.mempool = mp;
	memcpy(&rxq->qconf.conf.rx, rx_conf, sizeof(struct rte_eth_rxconf));

	nix_rx_queue_reset(rxq);
	otx2_nix_dbg("rq=%d pool=%s qsize=%d nb_desc=%d->%d",
		     rq, mp->name, qsize, nb_desc, rxq->qlen);

	eth_dev->data->rx_queue_state[rq] = RTE_ETH_QUEUE_STATE_STOPPED;

	/* Calculating delta and freq mult between PTP HI clock and tsc.
	 * These are needed in deriving raw clock value from tsc counter.
	 * read_clock eth op returns raw clock value.
	 */
	if ((dev->rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) ||
	    otx2_ethdev_is_ptp_en(dev)) {
		rc = otx2_nix_raw_clock_tsc_conv(dev);
		if (rc) {
			otx2_err("Failed to calculate delta and freq mult");
			goto fail;
		}
	}

	/* Setup scatter mode if needed by jumbo */
	otx2_nix_enable_mseg_on_jumbo(rxq);

	return 0;

free_rxq:
	otx2_nix_rx_queue_release(eth_dev, rq);
fail:
	return rc;
}

static inline uint8_t
nix_sq_max_sqe_sz(struct otx2_eth_txq *txq)
{
	/*
	 * Maximum three segments can be supported with W8, Choose
	 * NIX_MAXSQESZ_W16 for multi segment offload.
	 */
	if (txq->offloads & RTE_ETH_TX_OFFLOAD_MULTI_SEGS)
		return NIX_MAXSQESZ_W16;
	else
		return NIX_MAXSQESZ_W8;
}

static uint16_t
nix_rx_offload_flags(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct rte_eth_conf *conf = &data->dev_conf;
	struct rte_eth_rxmode *rxmode = &conf->rxmode;
	uint16_t flags = 0;

	if (rxmode->mq_mode == RTE_ETH_MQ_RX_RSS &&
			(dev->rx_offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH))
		flags |= NIX_RX_OFFLOAD_RSS_F;

	if (dev->rx_offloads & (RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
			 RTE_ETH_RX_OFFLOAD_UDP_CKSUM))
		flags |= NIX_RX_OFFLOAD_CHECKSUM_F;

	if (dev->rx_offloads & (RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
				RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM))
		flags |= NIX_RX_OFFLOAD_CHECKSUM_F;

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER)
		flags |= NIX_RX_MULTI_SEG_F;

	if (dev->rx_offloads & (RTE_ETH_RX_OFFLOAD_VLAN_STRIP |
				RTE_ETH_RX_OFFLOAD_QINQ_STRIP))
		flags |= NIX_RX_OFFLOAD_VLAN_STRIP_F;

	if ((dev->rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP))
		flags |= NIX_RX_OFFLOAD_TSTAMP_F;

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY)
		flags |= NIX_RX_OFFLOAD_SECURITY_F;

	if (!dev->ptype_disable)
		flags |= NIX_RX_OFFLOAD_PTYPE_F;

	return flags;
}

static uint16_t
nix_tx_offload_flags(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint64_t conf = dev->tx_offloads;
	uint16_t flags = 0;

	/* Fastpath is dependent on these enums */
	RTE_BUILD_BUG_ON(RTE_MBUF_F_TX_TCP_CKSUM != (1ULL << 52));
	RTE_BUILD_BUG_ON(RTE_MBUF_F_TX_SCTP_CKSUM != (2ULL << 52));
	RTE_BUILD_BUG_ON(RTE_MBUF_F_TX_UDP_CKSUM != (3ULL << 52));
	RTE_BUILD_BUG_ON(RTE_MBUF_F_TX_IP_CKSUM != (1ULL << 54));
	RTE_BUILD_BUG_ON(RTE_MBUF_F_TX_IPV4 != (1ULL << 55));
	RTE_BUILD_BUG_ON(RTE_MBUF_F_TX_OUTER_IP_CKSUM != (1ULL << 58));
	RTE_BUILD_BUG_ON(RTE_MBUF_F_TX_OUTER_IPV4 != (1ULL << 59));
	RTE_BUILD_BUG_ON(RTE_MBUF_F_TX_OUTER_IPV6 != (1ULL << 60));
	RTE_BUILD_BUG_ON(RTE_MBUF_F_TX_OUTER_UDP_CKSUM != (1ULL << 41));
	RTE_BUILD_BUG_ON(RTE_MBUF_L2_LEN_BITS != 7);
	RTE_BUILD_BUG_ON(RTE_MBUF_L3_LEN_BITS != 9);
	RTE_BUILD_BUG_ON(RTE_MBUF_OUTL2_LEN_BITS != 7);
	RTE_BUILD_BUG_ON(RTE_MBUF_OUTL3_LEN_BITS != 9);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_off) !=
			 offsetof(struct rte_mbuf, buf_iova) + 8);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, ol_flags) !=
			 offsetof(struct rte_mbuf, buf_iova) + 16);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, pkt_len) !=
			 offsetof(struct rte_mbuf, ol_flags) + 12);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, tx_offload) !=
			 offsetof(struct rte_mbuf, pool) + 2 * sizeof(void *));

	if (conf & RTE_ETH_TX_OFFLOAD_VLAN_INSERT ||
	    conf & RTE_ETH_TX_OFFLOAD_QINQ_INSERT)
		flags |= NIX_TX_OFFLOAD_VLAN_QINQ_F;

	if (conf & RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM ||
	    conf & RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM)
		flags |= NIX_TX_OFFLOAD_OL3_OL4_CSUM_F;

	if (conf & RTE_ETH_TX_OFFLOAD_IPV4_CKSUM ||
	    conf & RTE_ETH_TX_OFFLOAD_TCP_CKSUM ||
	    conf & RTE_ETH_TX_OFFLOAD_UDP_CKSUM ||
	    conf & RTE_ETH_TX_OFFLOAD_SCTP_CKSUM)
		flags |= NIX_TX_OFFLOAD_L3_L4_CSUM_F;

	if (!(conf & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE))
		flags |= NIX_TX_OFFLOAD_MBUF_NOFF_F;

	if (conf & RTE_ETH_TX_OFFLOAD_MULTI_SEGS)
		flags |= NIX_TX_MULTI_SEG_F;

	/* Enable Inner checksum for TSO */
	if (conf & RTE_ETH_TX_OFFLOAD_TCP_TSO)
		flags |= (NIX_TX_OFFLOAD_TSO_F |
			  NIX_TX_OFFLOAD_L3_L4_CSUM_F);

	/* Enable Inner and Outer checksum for Tunnel TSO */
	if (conf & (RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
		    RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO |
		    RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO))
		flags |= (NIX_TX_OFFLOAD_TSO_F |
			  NIX_TX_OFFLOAD_OL3_OL4_CSUM_F |
			  NIX_TX_OFFLOAD_L3_L4_CSUM_F);

	if (conf & RTE_ETH_TX_OFFLOAD_SECURITY)
		flags |= NIX_TX_OFFLOAD_SECURITY_F;

	if ((dev->rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP))
		flags |= NIX_TX_OFFLOAD_TSTAMP_F;

	return flags;
}

static int
nix_sqb_lock(struct rte_mempool *mp)
{
	struct otx2_npa_lf *npa_lf = otx2_intra_dev_get_cfg()->npa_lf;
	struct npa_aq_enq_req *req;
	int rc;

	req = otx2_mbox_alloc_msg_npa_aq_enq(npa_lf->mbox);
	req->aura_id = npa_lf_aura_handle_to_aura(mp->pool_id);
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_LOCK;

	req = otx2_mbox_alloc_msg_npa_aq_enq(npa_lf->mbox);
	if (!req) {
		/* The shared memory buffer can be full.
		 * Flush it and retry
		 */
		otx2_mbox_msg_send(npa_lf->mbox, 0);
		rc = otx2_mbox_wait_for_rsp(npa_lf->mbox, 0);
		if (rc < 0) {
			otx2_err("Failed to LOCK AURA context");
			return rc;
		}

		req = otx2_mbox_alloc_msg_npa_aq_enq(npa_lf->mbox);
		if (!req) {
			otx2_err("Failed to LOCK POOL context");
			return -ENOMEM;
		}
	}

	req->aura_id = npa_lf_aura_handle_to_aura(mp->pool_id);
	req->ctype = NPA_AQ_CTYPE_POOL;
	req->op = NPA_AQ_INSTOP_LOCK;

	rc = otx2_mbox_process(npa_lf->mbox);
	if (rc < 0) {
		otx2_err("Unable to lock POOL in NDC");
		return rc;
	}

	return 0;
}

static int
nix_sqb_unlock(struct rte_mempool *mp)
{
	struct otx2_npa_lf *npa_lf = otx2_intra_dev_get_cfg()->npa_lf;
	struct npa_aq_enq_req *req;
	int rc;

	req = otx2_mbox_alloc_msg_npa_aq_enq(npa_lf->mbox);
	req->aura_id = npa_lf_aura_handle_to_aura(mp->pool_id);
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_UNLOCK;

	req = otx2_mbox_alloc_msg_npa_aq_enq(npa_lf->mbox);
	if (!req) {
		/* The shared memory buffer can be full.
		 * Flush it and retry
		 */
		otx2_mbox_msg_send(npa_lf->mbox, 0);
		rc = otx2_mbox_wait_for_rsp(npa_lf->mbox, 0);
		if (rc < 0) {
			otx2_err("Failed to UNLOCK AURA context");
			return rc;
		}

		req = otx2_mbox_alloc_msg_npa_aq_enq(npa_lf->mbox);
		if (!req) {
			otx2_err("Failed to UNLOCK POOL context");
			return -ENOMEM;
		}
	}
	req = otx2_mbox_alloc_msg_npa_aq_enq(npa_lf->mbox);
	req->aura_id = npa_lf_aura_handle_to_aura(mp->pool_id);
	req->ctype = NPA_AQ_CTYPE_POOL;
	req->op = NPA_AQ_INSTOP_UNLOCK;

	rc = otx2_mbox_process(npa_lf->mbox);
	if (rc < 0) {
		otx2_err("Unable to UNLOCK AURA in NDC");
		return rc;
	}

	return 0;
}

void
otx2_nix_enable_mseg_on_jumbo(struct otx2_eth_rxq *rxq)
{
	struct rte_pktmbuf_pool_private *mbp_priv;
	struct rte_eth_dev *eth_dev;
	struct otx2_eth_dev *dev;
	uint32_t buffsz;

	eth_dev = rxq->eth_dev;
	dev = otx2_eth_pmd_priv(eth_dev);

	/* Get rx buffer size */
	mbp_priv = rte_mempool_get_priv(rxq->pool);
	buffsz = mbp_priv->mbuf_data_room_size - RTE_PKTMBUF_HEADROOM;

	if (eth_dev->data->mtu + (uint32_t)NIX_L2_OVERHEAD > buffsz) {
		dev->rx_offloads |= RTE_ETH_RX_OFFLOAD_SCATTER;
		dev->tx_offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;

		/* Setting up the rx[tx]_offload_flags due to change
		 * in rx[tx]_offloads.
		 */
		dev->rx_offload_flags |= nix_rx_offload_flags(eth_dev);
		dev->tx_offload_flags |= nix_tx_offload_flags(eth_dev);
	}
}

static int
nix_sq_init(struct otx2_eth_txq *txq)
{
	struct otx2_eth_dev *dev = txq->dev;
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_aq_enq_req *sq;
	uint32_t rr_quantum;
	uint16_t smq;
	int rc;

	if (txq->sqb_pool->pool_id == 0)
		return -EINVAL;

	rc = otx2_nix_tm_get_leaf_data(dev, txq->sq, &rr_quantum, &smq);
	if (rc) {
		otx2_err("Failed to get sq->smq(leaf node), rc=%d", rc);
		return rc;
	}

	sq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	sq->qidx = txq->sq;
	sq->ctype = NIX_AQ_CTYPE_SQ;
	sq->op = NIX_AQ_INSTOP_INIT;
	sq->sq.max_sqe_size = nix_sq_max_sqe_sz(txq);

	sq->sq.smq = smq;
	sq->sq.smq_rr_quantum = rr_quantum;
	sq->sq.default_chan = dev->tx_chan_base;
	sq->sq.sqe_stype = NIX_STYPE_STF;
	sq->sq.ena = 1;
	if (sq->sq.max_sqe_size == NIX_MAXSQESZ_W8)
		sq->sq.sqe_stype = NIX_STYPE_STP;
	sq->sq.sqb_aura =
		npa_lf_aura_handle_to_aura(txq->sqb_pool->pool_id);
	sq->sq.sq_int_ena = BIT(NIX_SQINT_LMT_ERR);
	sq->sq.sq_int_ena |= BIT(NIX_SQINT_SQB_ALLOC_FAIL);
	sq->sq.sq_int_ena |= BIT(NIX_SQINT_SEND_ERR);
	sq->sq.sq_int_ena |= BIT(NIX_SQINT_MNQ_ERR);

	/* Many to one reduction */
	sq->sq.qint_idx = txq->sq % dev->qints;

	rc = otx2_mbox_process(mbox);
	if (rc < 0)
		return rc;

	if (dev->lock_tx_ctx) {
		sq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
		sq->qidx = txq->sq;
		sq->ctype = NIX_AQ_CTYPE_SQ;
		sq->op = NIX_AQ_INSTOP_LOCK;

		rc = otx2_mbox_process(mbox);
	}

	return rc;
}

static int
nix_sq_uninit(struct otx2_eth_txq *txq)
{
	struct otx2_eth_dev *dev = txq->dev;
	struct otx2_mbox *mbox = dev->mbox;
	struct ndc_sync_op *ndc_req;
	struct nix_aq_enq_rsp *rsp;
	struct nix_aq_enq_req *aq;
	uint16_t sqes_per_sqb;
	void *sqb_buf;
	int rc, count;

	otx2_nix_dbg("Cleaning up sq %u", txq->sq);

	aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = txq->sq;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_READ;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	/* Check if sq is already cleaned up */
	if (!rsp->sq.ena)
		return 0;

	/* Disable sq */
	aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = txq->sq;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_WRITE;

	aq->sq_mask.ena = ~aq->sq_mask.ena;
	aq->sq.ena = 0;

	rc = otx2_mbox_process(mbox);
	if (rc)
		return rc;

	if (dev->lock_tx_ctx) {
		/* Unlock sq */
		aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
		aq->qidx = txq->sq;
		aq->ctype = NIX_AQ_CTYPE_SQ;
		aq->op = NIX_AQ_INSTOP_UNLOCK;

		rc = otx2_mbox_process(mbox);
		if (rc < 0)
			return rc;

		nix_sqb_unlock(txq->sqb_pool);
	}

	/* Read SQ and free sqb's */
	aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = txq->sq;
	aq->ctype = NIX_AQ_CTYPE_SQ;
	aq->op = NIX_AQ_INSTOP_READ;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	if (aq->sq.smq_pend)
		otx2_err("SQ has pending sqe's");

	count = aq->sq.sqb_count;
	sqes_per_sqb = 1 << txq->sqes_per_sqb_log2;
	/* Free SQB's that are used */
	sqb_buf = (void *)rsp->sq.head_sqb;
	while (count) {
		void *next_sqb;

		next_sqb = *(void **)((uintptr_t)sqb_buf + (uint32_t)
				      ((sqes_per_sqb - 1) *
				      nix_sq_max_sqe_sz(txq)));
		npa_lf_aura_op_free(txq->sqb_pool->pool_id, 1,
				    (uint64_t)sqb_buf);
		sqb_buf = next_sqb;
		count--;
	}

	/* Free next to use sqb */
	if (rsp->sq.next_sqb)
		npa_lf_aura_op_free(txq->sqb_pool->pool_id, 1,
				    rsp->sq.next_sqb);

	/* Sync NDC-NIX-TX for LF */
	ndc_req = otx2_mbox_alloc_msg_ndc_sync_op(mbox);
	ndc_req->nix_lf_tx_sync = 1;
	rc = otx2_mbox_process(mbox);
	if (rc)
		otx2_err("Error on NDC-NIX-TX LF sync, rc %d", rc);

	return rc;
}

static int
nix_sqb_aura_limit_cfg(struct rte_mempool *mp, uint16_t nb_sqb_bufs)
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
nix_alloc_sqb_pool(int port, struct otx2_eth_txq *txq, uint16_t nb_desc)
{
	struct otx2_eth_dev *dev = txq->dev;
	uint16_t sqes_per_sqb, nb_sqb_bufs;
	char name[RTE_MEMPOOL_NAMESIZE];
	struct rte_mempool_objsz sz;
	struct npa_aura_s *aura;
	uint32_t tmp, blk_sz;

	aura = (struct npa_aura_s *)((uintptr_t)txq->fc_mem + OTX2_ALIGN);
	snprintf(name, sizeof(name), "otx2_sqb_pool_%d_%d", port, txq->sq);
	blk_sz = dev->sqb_size;

	if (nix_sq_max_sqe_sz(txq) == NIX_MAXSQESZ_W16)
		sqes_per_sqb = (dev->sqb_size / 8) / 16;
	else
		sqes_per_sqb = (dev->sqb_size / 8) / 8;

	nb_sqb_bufs = nb_desc / sqes_per_sqb;
	/* Clamp up to devarg passed SQB count */
	nb_sqb_bufs =  RTE_MIN(dev->max_sqb_count, RTE_MAX(NIX_DEF_SQB,
			      nb_sqb_bufs + NIX_SQB_LIST_SPACE));

	txq->sqb_pool = rte_mempool_create_empty(name, NIX_MAX_SQB, blk_sz,
						 0, 0, dev->node,
						 RTE_MEMPOOL_F_NO_SPREAD);
	txq->nb_sqb_bufs = nb_sqb_bufs;
	txq->sqes_per_sqb_log2 = (uint16_t)rte_log2_u32(sqes_per_sqb);
	txq->nb_sqb_bufs_adj = nb_sqb_bufs -
		RTE_ALIGN_MUL_CEIL(nb_sqb_bufs, sqes_per_sqb) / sqes_per_sqb;
	txq->nb_sqb_bufs_adj =
		(NIX_SQB_LOWER_THRESH * txq->nb_sqb_bufs_adj) / 100;

	if (txq->sqb_pool == NULL) {
		otx2_err("Failed to allocate sqe mempool");
		goto fail;
	}

	memset(aura, 0, sizeof(*aura));
	aura->fc_ena = 1;
	aura->fc_addr = txq->fc_iova;
	aura->fc_hyst_bits = 0; /* Store count on all updates */
	if (rte_mempool_set_ops_byname(txq->sqb_pool, "octeontx2_npa", aura)) {
		otx2_err("Failed to set ops for sqe mempool");
		goto fail;
	}
	if (rte_mempool_populate_default(txq->sqb_pool) < 0) {
		otx2_err("Failed to populate sqe mempool");
		goto fail;
	}

	tmp = rte_mempool_calc_obj_size(blk_sz, RTE_MEMPOOL_F_NO_SPREAD, &sz);
	if (dev->sqb_size != sz.elt_size) {
		otx2_err("sqe pool block size is not expected %d != %d",
			 dev->sqb_size, tmp);
		goto fail;
	}

	nix_sqb_aura_limit_cfg(txq->sqb_pool, txq->nb_sqb_bufs);
	if (dev->lock_tx_ctx)
		nix_sqb_lock(txq->sqb_pool);

	return 0;
fail:
	return -ENOMEM;
}

void
otx2_nix_form_default_desc(struct otx2_eth_txq *txq)
{
	struct nix_send_ext_s *send_hdr_ext;
	struct nix_send_hdr_s *send_hdr;
	struct nix_send_mem_s *send_mem;
	union nix_send_sg_s *sg;

	/* Initialize the fields based on basic single segment packet */
	memset(&txq->cmd, 0, sizeof(txq->cmd));

	if (txq->dev->tx_offload_flags & NIX_TX_NEED_EXT_HDR) {
		send_hdr = (struct nix_send_hdr_s *)&txq->cmd[0];
		/* 2(HDR) + 2(EXT_HDR) + 1(SG) + 1(IOVA) = 6/2 - 1 = 2 */
		send_hdr->w0.sizem1 = 2;

		send_hdr_ext = (struct nix_send_ext_s *)&txq->cmd[2];
		send_hdr_ext->w0.subdc = NIX_SUBDC_EXT;
		if (txq->dev->tx_offload_flags & NIX_TX_OFFLOAD_TSTAMP_F) {
			/* Default: one seg packet would have:
			 * 2(HDR) + 2(EXT) + 1(SG) + 1(IOVA) + 2(MEM)
			 * => 8/2 - 1 = 3
			 */
			send_hdr->w0.sizem1 = 3;
			send_hdr_ext->w0.tstmp = 1;

			/* To calculate the offset for send_mem,
			 * send_hdr->w0.sizem1 * 2
			 */
			send_mem = (struct nix_send_mem_s *)(txq->cmd +
						(send_hdr->w0.sizem1 << 1));
			send_mem->subdc = NIX_SUBDC_MEM;
			send_mem->alg = NIX_SENDMEMALG_SETTSTMP;
			send_mem->addr = txq->dev->tstamp.tx_tstamp_iova;
		}
		sg = (union nix_send_sg_s *)&txq->cmd[4];
	} else {
		send_hdr = (struct nix_send_hdr_s *)&txq->cmd[0];
		/* 2(HDR) + 1(SG) + 1(IOVA) = 4/2 - 1 = 1 */
		send_hdr->w0.sizem1 = 1;
		sg = (union nix_send_sg_s *)&txq->cmd[2];
	}

	send_hdr->w0.sq = txq->sq;
	sg->subdc = NIX_SUBDC_SG;
	sg->segs = 1;
	sg->ld_type = NIX_SENDLDTYPE_LDD;

	rte_smp_wmb();
}

static void
otx2_nix_tx_queue_release(struct rte_eth_dev *eth_dev, uint16_t qid)
{
	struct otx2_eth_txq *txq = eth_dev->data->tx_queues[qid];

	if (!txq)
		return;

	otx2_nix_dbg("Releasing txq %u", txq->sq);

	/* Flush and disable tm */
	otx2_nix_sq_flush_pre(txq, eth_dev->data->dev_started);

	/* Free sqb's and disable sq */
	nix_sq_uninit(txq);

	if (txq->sqb_pool) {
		rte_mempool_free(txq->sqb_pool);
		txq->sqb_pool = NULL;
	}
	otx2_nix_sq_flush_post(txq);
	rte_free(txq);
	eth_dev->data->tx_queues[qid] = NULL;
}


static int
otx2_nix_tx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t sq,
			uint16_t nb_desc, unsigned int socket_id,
			const struct rte_eth_txconf *tx_conf)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	const struct rte_memzone *fc;
	struct otx2_eth_txq *txq;
	uint64_t offloads;
	int rc;

	rc = -EINVAL;

	/* Compile time check to make sure all fast path elements in a CL */
	RTE_BUILD_BUG_ON(offsetof(struct otx2_eth_txq, slow_path_start) >= 128);

	if (tx_conf->tx_deferred_start) {
		otx2_err("Tx deferred start is not supported");
		goto fail;
	}

	/* Free memory prior to re-allocation if needed. */
	if (eth_dev->data->tx_queues[sq] != NULL) {
		otx2_nix_dbg("Freeing memory prior to re-allocation %d", sq);
		otx2_nix_tx_queue_release(eth_dev, sq);
	}

	/* Find the expected offloads for this queue */
	offloads = tx_conf->offloads | eth_dev->data->dev_conf.txmode.offloads;

	/* Allocating tx queue data structure */
	txq = rte_zmalloc_socket("otx2_ethdev TX queue", sizeof(*txq),
				 OTX2_ALIGN, socket_id);
	if (txq == NULL) {
		otx2_err("Failed to alloc txq=%d", sq);
		rc = -ENOMEM;
		goto fail;
	}
	txq->sq = sq;
	txq->dev = dev;
	txq->sqb_pool = NULL;
	txq->offloads = offloads;
	dev->tx_offloads |= offloads;
	eth_dev->data->tx_queues[sq] = txq;

	/*
	 * Allocate memory for flow control updates from HW.
	 * Alloc one cache line, so that fits all FC_STYPE modes.
	 */
	fc = rte_eth_dma_zone_reserve(eth_dev, "fcmem", sq,
				      OTX2_ALIGN + sizeof(struct npa_aura_s),
				      OTX2_ALIGN, dev->node);
	if (fc == NULL) {
		otx2_err("Failed to allocate mem for fcmem");
		rc = -ENOMEM;
		goto free_txq;
	}
	txq->fc_iova = fc->iova;
	txq->fc_mem = fc->addr;

	/* Initialize the aura sqb pool */
	rc = nix_alloc_sqb_pool(eth_dev->data->port_id, txq, nb_desc);
	if (rc) {
		otx2_err("Failed to alloc sqe pool rc=%d", rc);
		goto free_txq;
	}

	/* Initialize the SQ */
	rc = nix_sq_init(txq);
	if (rc) {
		otx2_err("Failed to init sq=%d context", sq);
		goto free_txq;
	}

	txq->fc_cache_pkts = 0;
	txq->io_addr = dev->base + NIX_LF_OP_SENDX(0);
	/* Evenly distribute LMT slot for each sq */
	txq->lmt_addr = (void *)(dev->lmt_addr + ((sq & LMT_SLOT_MASK) << 12));

	txq->qconf.socket_id = socket_id;
	txq->qconf.nb_desc = nb_desc;
	memcpy(&txq->qconf.conf.tx, tx_conf, sizeof(struct rte_eth_txconf));

	txq->lso_tun_fmt = dev->lso_tun_fmt;
	otx2_nix_form_default_desc(txq);

	otx2_nix_dbg("sq=%d fc=%p offload=0x%" PRIx64 " sqb=0x%" PRIx64 ""
		     " lmt_addr=%p nb_sqb_bufs=%d sqes_per_sqb_log2=%d", sq,
		     fc->addr, offloads, txq->sqb_pool->pool_id, txq->lmt_addr,
		     txq->nb_sqb_bufs, txq->sqes_per_sqb_log2);
	eth_dev->data->tx_queue_state[sq] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;

free_txq:
	otx2_nix_tx_queue_release(eth_dev, sq);
fail:
	return rc;
}

static int
nix_store_queue_cfg_and_then_release(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_eth_qconf *tx_qconf = NULL;
	struct otx2_eth_qconf *rx_qconf = NULL;
	struct otx2_eth_txq **txq;
	struct otx2_eth_rxq **rxq;
	int i, nb_rxq, nb_txq;

	nb_rxq = RTE_MIN(dev->configured_nb_rx_qs, eth_dev->data->nb_rx_queues);
	nb_txq = RTE_MIN(dev->configured_nb_tx_qs, eth_dev->data->nb_tx_queues);

	tx_qconf = malloc(nb_txq * sizeof(*tx_qconf));
	if (tx_qconf == NULL) {
		otx2_err("Failed to allocate memory for tx_qconf");
		goto fail;
	}

	rx_qconf = malloc(nb_rxq * sizeof(*rx_qconf));
	if (rx_qconf == NULL) {
		otx2_err("Failed to allocate memory for rx_qconf");
		goto fail;
	}

	txq = (struct otx2_eth_txq **)eth_dev->data->tx_queues;
	for (i = 0; i < nb_txq; i++) {
		if (txq[i] == NULL) {
			tx_qconf[i].valid = false;
			otx2_info("txq[%d] is already released", i);
			continue;
		}
		memcpy(&tx_qconf[i], &txq[i]->qconf, sizeof(*tx_qconf));
		tx_qconf[i].valid = true;
		otx2_nix_tx_queue_release(eth_dev, i);
	}

	rxq = (struct otx2_eth_rxq **)eth_dev->data->rx_queues;
	for (i = 0; i < nb_rxq; i++) {
		if (rxq[i] == NULL) {
			rx_qconf[i].valid = false;
			otx2_info("rxq[%d] is already released", i);
			continue;
		}
		memcpy(&rx_qconf[i], &rxq[i]->qconf, sizeof(*rx_qconf));
		rx_qconf[i].valid = true;
		otx2_nix_rx_queue_release(eth_dev, i);
	}

	dev->tx_qconf = tx_qconf;
	dev->rx_qconf = rx_qconf;
	return 0;

fail:
	free(tx_qconf);
	free(rx_qconf);

	return -ENOMEM;
}

static int
nix_restore_queue_cfg(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_eth_qconf *tx_qconf = dev->tx_qconf;
	struct otx2_eth_qconf *rx_qconf = dev->rx_qconf;
	int rc, i, nb_rxq, nb_txq;

	nb_rxq = RTE_MIN(dev->configured_nb_rx_qs, eth_dev->data->nb_rx_queues);
	nb_txq = RTE_MIN(dev->configured_nb_tx_qs, eth_dev->data->nb_tx_queues);

	rc = -ENOMEM;
	/* Setup tx & rx queues with previous configuration so
	 * that the queues can be functional in cases like ports
	 * are started without re configuring queues.
	 *
	 * Usual re config sequence is like below:
	 * port_configure() {
	 *      if(reconfigure) {
	 *              queue_release()
	 *              queue_setup()
	 *      }
	 *      queue_configure() {
	 *              queue_release()
	 *              queue_setup()
	 *      }
	 * }
	 * port_start()
	 *
	 * In some application's control path, queue_configure() would
	 * NOT be invoked for TXQs/RXQs in port_configure().
	 * In such cases, queues can be functional after start as the
	 * queues are already setup in port_configure().
	 */
	for (i = 0; i < nb_txq; i++) {
		if (!tx_qconf[i].valid)
			continue;
		rc = otx2_nix_tx_queue_setup(eth_dev, i, tx_qconf[i].nb_desc,
					     tx_qconf[i].socket_id,
					     &tx_qconf[i].conf.tx);
		if (rc) {
			otx2_err("Failed to setup tx queue rc=%d", rc);
			for (i -= 1; i >= 0; i--)
				otx2_nix_tx_queue_release(eth_dev, i);
			goto fail;
		}
	}

	free(tx_qconf); tx_qconf = NULL;

	for (i = 0; i < nb_rxq; i++) {
		if (!rx_qconf[i].valid)
			continue;
		rc = otx2_nix_rx_queue_setup(eth_dev, i, rx_qconf[i].nb_desc,
					     rx_qconf[i].socket_id,
					     &rx_qconf[i].conf.rx,
					     rx_qconf[i].mempool);
		if (rc) {
			otx2_err("Failed to setup rx queue rc=%d", rc);
			for (i -= 1; i >= 0; i--)
				otx2_nix_rx_queue_release(eth_dev, i);
			goto release_tx_queues;
		}
	}

	free(rx_qconf); rx_qconf = NULL;

	return 0;

release_tx_queues:
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		otx2_nix_tx_queue_release(eth_dev, i);
fail:
	if (tx_qconf)
		free(tx_qconf);
	if (rx_qconf)
		free(rx_qconf);

	return rc;
}

static uint16_t
nix_eth_nop_burst(void *queue, struct rte_mbuf **mbufs, uint16_t pkts)
{
	RTE_SET_USED(queue);
	RTE_SET_USED(mbufs);
	RTE_SET_USED(pkts);

	return 0;
}

static void
nix_set_nop_rxtx_function(struct rte_eth_dev *eth_dev)
{
	/* These dummy functions are required for supporting
	 * some applications which reconfigure queues without
	 * stopping tx burst and rx burst threads(eg kni app)
	 * When the queues context is saved, txq/rxqs are released
	 * which caused app crash since rx/tx burst is still
	 * on different lcores
	 */
	eth_dev->tx_pkt_burst = nix_eth_nop_burst;
	eth_dev->rx_pkt_burst = nix_eth_nop_burst;
	rte_mb();
}

static void
nix_lso_tcp(struct nix_lso_format_cfg *req, bool v4)
{
	volatile struct nix_lso_format *field;

	/* Format works only with TCP packet marked by OL3/OL4 */
	field = (volatile struct nix_lso_format *)&req->fields[0];
	req->field_mask = NIX_LSO_FIELD_MASK;
	/* Outer IPv4/IPv6 */
	field->layer = NIX_TXLAYER_OL3;
	field->offset = v4 ? 2 : 4;
	field->sizem1 = 1; /* 2B */
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;
	if (v4) {
		/* IPID field */
		field->layer = NIX_TXLAYER_OL3;
		field->offset = 4;
		field->sizem1 = 1;
		/* Incremented linearly per segment */
		field->alg = NIX_LSOALG_ADD_SEGNUM;
		field++;
	}

	/* TCP sequence number update */
	field->layer = NIX_TXLAYER_OL4;
	field->offset = 4;
	field->sizem1 = 3; /* 4 bytes */
	field->alg = NIX_LSOALG_ADD_OFFSET;
	field++;
	/* TCP flags field */
	field->layer = NIX_TXLAYER_OL4;
	field->offset = 12;
	field->sizem1 = 1;
	field->alg = NIX_LSOALG_TCP_FLAGS;
	field++;
}

static void
nix_lso_udp_tun_tcp(struct nix_lso_format_cfg *req,
		    bool outer_v4, bool inner_v4)
{
	volatile struct nix_lso_format *field;

	field = (volatile struct nix_lso_format *)&req->fields[0];
	req->field_mask = NIX_LSO_FIELD_MASK;
	/* Outer IPv4/IPv6 len */
	field->layer = NIX_TXLAYER_OL3;
	field->offset = outer_v4 ? 2 : 4;
	field->sizem1 = 1; /* 2B */
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;
	if (outer_v4) {
		/* IPID */
		field->layer = NIX_TXLAYER_OL3;
		field->offset = 4;
		field->sizem1 = 1;
		/* Incremented linearly per segment */
		field->alg = NIX_LSOALG_ADD_SEGNUM;
		field++;
	}

	/* Outer UDP length */
	field->layer = NIX_TXLAYER_OL4;
	field->offset = 4;
	field->sizem1 = 1;
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;

	/* Inner IPv4/IPv6 */
	field->layer = NIX_TXLAYER_IL3;
	field->offset = inner_v4 ? 2 : 4;
	field->sizem1 = 1; /* 2B */
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;
	if (inner_v4) {
		/* IPID field */
		field->layer = NIX_TXLAYER_IL3;
		field->offset = 4;
		field->sizem1 = 1;
		/* Incremented linearly per segment */
		field->alg = NIX_LSOALG_ADD_SEGNUM;
		field++;
	}

	/* TCP sequence number update */
	field->layer = NIX_TXLAYER_IL4;
	field->offset = 4;
	field->sizem1 = 3; /* 4 bytes */
	field->alg = NIX_LSOALG_ADD_OFFSET;
	field++;

	/* TCP flags field */
	field->layer = NIX_TXLAYER_IL4;
	field->offset = 12;
	field->sizem1 = 1;
	field->alg = NIX_LSOALG_TCP_FLAGS;
	field++;
}

static void
nix_lso_tun_tcp(struct nix_lso_format_cfg *req,
		bool outer_v4, bool inner_v4)
{
	volatile struct nix_lso_format *field;

	field = (volatile struct nix_lso_format *)&req->fields[0];
	req->field_mask = NIX_LSO_FIELD_MASK;
	/* Outer IPv4/IPv6 len */
	field->layer = NIX_TXLAYER_OL3;
	field->offset = outer_v4 ? 2 : 4;
	field->sizem1 = 1; /* 2B */
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;
	if (outer_v4) {
		/* IPID */
		field->layer = NIX_TXLAYER_OL3;
		field->offset = 4;
		field->sizem1 = 1;
		/* Incremented linearly per segment */
		field->alg = NIX_LSOALG_ADD_SEGNUM;
		field++;
	}

	/* Inner IPv4/IPv6 */
	field->layer = NIX_TXLAYER_IL3;
	field->offset = inner_v4 ? 2 : 4;
	field->sizem1 = 1; /* 2B */
	field->alg = NIX_LSOALG_ADD_PAYLEN;
	field++;
	if (inner_v4) {
		/* IPID field */
		field->layer = NIX_TXLAYER_IL3;
		field->offset = 4;
		field->sizem1 = 1;
		/* Incremented linearly per segment */
		field->alg = NIX_LSOALG_ADD_SEGNUM;
		field++;
	}

	/* TCP sequence number update */
	field->layer = NIX_TXLAYER_IL4;
	field->offset = 4;
	field->sizem1 = 3; /* 4 bytes */
	field->alg = NIX_LSOALG_ADD_OFFSET;
	field++;

	/* TCP flags field */
	field->layer = NIX_TXLAYER_IL4;
	field->offset = 12;
	field->sizem1 = 1;
	field->alg = NIX_LSOALG_TCP_FLAGS;
	field++;
}

static int
nix_setup_lso_formats(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_lso_format_cfg_rsp *rsp;
	struct nix_lso_format_cfg *req;
	uint8_t *fmt;
	int rc;

	/* Skip if TSO was not requested */
	if (!(dev->tx_offload_flags & NIX_TX_OFFLOAD_TSO_F))
		return 0;
	/*
	 * IPv4/TCP LSO
	 */
	req = otx2_mbox_alloc_msg_nix_lso_format_cfg(mbox);
	nix_lso_tcp(req, true);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	if (rsp->lso_format_idx != NIX_LSO_FORMAT_IDX_TSOV4)
		return -EFAULT;
	otx2_nix_dbg("tcpv4 lso fmt=%u", rsp->lso_format_idx);


	/*
	 * IPv6/TCP LSO
	 */
	req = otx2_mbox_alloc_msg_nix_lso_format_cfg(mbox);
	nix_lso_tcp(req, false);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	if (rsp->lso_format_idx != NIX_LSO_FORMAT_IDX_TSOV6)
		return -EFAULT;
	otx2_nix_dbg("tcpv6 lso fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv4/UDP/TUN HDR/IPv4/TCP LSO
	 */
	req = otx2_mbox_alloc_msg_nix_lso_format_cfg(mbox);
	nix_lso_udp_tun_tcp(req, true, true);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	dev->lso_udp_tun_idx[NIX_LSO_TUN_V4V4] = rsp->lso_format_idx;
	otx2_nix_dbg("udp tun v4v4 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv4/UDP/TUN HDR/IPv6/TCP LSO
	 */
	req = otx2_mbox_alloc_msg_nix_lso_format_cfg(mbox);
	nix_lso_udp_tun_tcp(req, true, false);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	dev->lso_udp_tun_idx[NIX_LSO_TUN_V4V6] = rsp->lso_format_idx;
	otx2_nix_dbg("udp tun v4v6 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv6/UDP/TUN HDR/IPv4/TCP LSO
	 */
	req = otx2_mbox_alloc_msg_nix_lso_format_cfg(mbox);
	nix_lso_udp_tun_tcp(req, false, true);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	dev->lso_udp_tun_idx[NIX_LSO_TUN_V6V4] = rsp->lso_format_idx;
	otx2_nix_dbg("udp tun v6v4 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv6/UDP/TUN HDR/IPv6/TCP LSO
	 */
	req = otx2_mbox_alloc_msg_nix_lso_format_cfg(mbox);
	nix_lso_udp_tun_tcp(req, false, false);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	dev->lso_udp_tun_idx[NIX_LSO_TUN_V6V6] = rsp->lso_format_idx;
	otx2_nix_dbg("udp tun v6v6 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv4/TUN HDR/IPv4/TCP LSO
	 */
	req = otx2_mbox_alloc_msg_nix_lso_format_cfg(mbox);
	nix_lso_tun_tcp(req, true, true);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	dev->lso_tun_idx[NIX_LSO_TUN_V4V4] = rsp->lso_format_idx;
	otx2_nix_dbg("tun v4v4 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv4/TUN HDR/IPv6/TCP LSO
	 */
	req = otx2_mbox_alloc_msg_nix_lso_format_cfg(mbox);
	nix_lso_tun_tcp(req, true, false);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	dev->lso_tun_idx[NIX_LSO_TUN_V4V6] = rsp->lso_format_idx;
	otx2_nix_dbg("tun v4v6 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv6/TUN HDR/IPv4/TCP LSO
	 */
	req = otx2_mbox_alloc_msg_nix_lso_format_cfg(mbox);
	nix_lso_tun_tcp(req, false, true);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	dev->lso_tun_idx[NIX_LSO_TUN_V6V4] = rsp->lso_format_idx;
	otx2_nix_dbg("tun v6v4 fmt=%u\n", rsp->lso_format_idx);

	/*
	 * IPv6/TUN HDR/IPv6/TCP LSO
	 */
	req = otx2_mbox_alloc_msg_nix_lso_format_cfg(mbox);
	nix_lso_tun_tcp(req, false, false);
	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	dev->lso_tun_idx[NIX_LSO_TUN_V6V6] = rsp->lso_format_idx;
	otx2_nix_dbg("tun v6v6 fmt=%u\n", rsp->lso_format_idx);

	/* Save all tun formats into u64 for fast path.
	 * Lower 32bit has non-udp tunnel formats.
	 * Upper 32bit has udp tunnel formats.
	 */
	fmt = dev->lso_tun_idx;
	dev->lso_tun_fmt = ((uint64_t)fmt[NIX_LSO_TUN_V4V4] |
			    (uint64_t)fmt[NIX_LSO_TUN_V4V6] << 8 |
			    (uint64_t)fmt[NIX_LSO_TUN_V6V4] << 16 |
			    (uint64_t)fmt[NIX_LSO_TUN_V6V6] << 24);

	fmt = dev->lso_udp_tun_idx;
	dev->lso_tun_fmt |= ((uint64_t)fmt[NIX_LSO_TUN_V4V4] << 32 |
			     (uint64_t)fmt[NIX_LSO_TUN_V4V6] << 40 |
			     (uint64_t)fmt[NIX_LSO_TUN_V6V4] << 48 |
			     (uint64_t)fmt[NIX_LSO_TUN_V6V6] << 56);

	return 0;
}

static int
otx2_nix_configure(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct rte_eth_conf *conf = &data->dev_conf;
	struct rte_eth_rxmode *rxmode = &conf->rxmode;
	struct rte_eth_txmode *txmode = &conf->txmode;
	char ea_fmt[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_ether_addr *ea;
	uint8_t nb_rxq, nb_txq;
	int rc;

	rc = -EINVAL;

	/* Sanity checks */
	if (rte_eal_has_hugepages() == 0) {
		otx2_err("Huge page is not configured");
		goto fail_configure;
	}

	if (conf->dcb_capability_en == 1) {
		otx2_err("dcb enable is not supported");
		goto fail_configure;
	}

	if (conf->fdir_conf.mode != RTE_FDIR_MODE_NONE) {
		otx2_err("Flow director is not supported");
		goto fail_configure;
	}

	if (rxmode->mq_mode != RTE_ETH_MQ_RX_NONE &&
	    rxmode->mq_mode != RTE_ETH_MQ_RX_RSS) {
		otx2_err("Unsupported mq rx mode %d", rxmode->mq_mode);
		goto fail_configure;
	}

	if (txmode->mq_mode != RTE_ETH_MQ_TX_NONE) {
		otx2_err("Unsupported mq tx mode %d", txmode->mq_mode);
		goto fail_configure;
	}

	if (otx2_dev_is_Ax(dev) &&
	    (txmode->offloads & RTE_ETH_TX_OFFLOAD_SCTP_CKSUM) &&
	    ((txmode->offloads & RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM) ||
	    (txmode->offloads & RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM))) {
		otx2_err("Outer IP and SCTP checksum unsupported");
		goto fail_configure;
	}

	/* Free the resources allocated from the previous configure */
	if (dev->configured == 1) {
		otx2_eth_sec_fini(eth_dev);
		otx2_nix_rxchan_bpid_cfg(eth_dev, false);
		otx2_nix_vlan_fini(eth_dev);
		otx2_nix_mc_addr_list_uninstall(eth_dev);
		otx2_flow_free_all_resources(dev);
		oxt2_nix_unregister_queue_irqs(eth_dev);
		if (eth_dev->data->dev_conf.intr_conf.rxq)
			oxt2_nix_unregister_cq_irqs(eth_dev);
		nix_set_nop_rxtx_function(eth_dev);
		rc = nix_store_queue_cfg_and_then_release(eth_dev);
		if (rc)
			goto fail_configure;
		otx2_nix_tm_fini(eth_dev);
		nix_lf_free(dev);
	}

	dev->rx_offloads = rxmode->offloads;
	dev->tx_offloads = txmode->offloads;
	dev->rx_offload_flags |= nix_rx_offload_flags(eth_dev);
	dev->tx_offload_flags |= nix_tx_offload_flags(eth_dev);
	dev->rss_info.rss_grps = NIX_RSS_GRPS;

	nb_rxq = RTE_MAX(data->nb_rx_queues, 1);
	nb_txq = RTE_MAX(data->nb_tx_queues, 1);

	/* Alloc a nix lf */
	rc = nix_lf_alloc(dev, nb_rxq, nb_txq);
	if (rc) {
		otx2_err("Failed to init nix_lf rc=%d", rc);
		goto fail_offloads;
	}

	otx2_nix_err_intr_enb_dis(eth_dev, true);
	otx2_nix_ras_intr_enb_dis(eth_dev, true);

	if (dev->ptp_en &&
	    dev->npc_flow.switch_header_type == OTX2_PRIV_FLAGS_HIGIG) {
		otx2_err("Both PTP and switch header enabled");
		goto free_nix_lf;
	}

	rc = nix_lf_switch_header_type_enable(dev, true);
	if (rc) {
		otx2_err("Failed to enable switch type nix_lf rc=%d", rc);
		goto free_nix_lf;
	}

	rc = nix_setup_lso_formats(dev);
	if (rc) {
		otx2_err("failed to setup nix lso format fields, rc=%d", rc);
		goto free_nix_lf;
	}

	/* Configure RSS */
	rc = otx2_nix_rss_config(eth_dev);
	if (rc) {
		otx2_err("Failed to configure rss rc=%d", rc);
		goto free_nix_lf;
	}

	/* Init the default TM scheduler hierarchy */
	rc = otx2_nix_tm_init_default(eth_dev);
	if (rc) {
		otx2_err("Failed to init traffic manager rc=%d", rc);
		goto free_nix_lf;
	}

	rc = otx2_nix_vlan_offload_init(eth_dev);
	if (rc) {
		otx2_err("Failed to init vlan offload rc=%d", rc);
		goto tm_fini;
	}

	/* Register queue IRQs */
	rc = oxt2_nix_register_queue_irqs(eth_dev);
	if (rc) {
		otx2_err("Failed to register queue interrupts rc=%d", rc);
		goto vlan_fini;
	}

	/* Register cq IRQs */
	if (eth_dev->data->dev_conf.intr_conf.rxq) {
		if (eth_dev->data->nb_rx_queues > dev->cints) {
			otx2_err("Rx interrupt cannot be enabled, rxq > %d",
				 dev->cints);
			goto q_irq_fini;
		}
		/* Rx interrupt feature cannot work with vector mode because,
		 * vector mode doesn't process packets unless min 4 pkts are
		 * received, while cq interrupts are generated even for 1 pkt
		 * in the CQ.
		 */
		dev->scalar_ena = true;

		rc = oxt2_nix_register_cq_irqs(eth_dev);
		if (rc) {
			otx2_err("Failed to register CQ interrupts rc=%d", rc);
			goto q_irq_fini;
		}
	}

	/* Configure loop back mode */
	rc = cgx_intlbk_enable(dev, eth_dev->data->dev_conf.lpbk_mode);
	if (rc) {
		otx2_err("Failed to configure cgx loop back mode rc=%d", rc);
		goto cq_fini;
	}

	rc = otx2_nix_rxchan_bpid_cfg(eth_dev, true);
	if (rc) {
		otx2_err("Failed to configure nix rx chan bpid cfg rc=%d", rc);
		goto cq_fini;
	}

	/* Enable security */
	rc = otx2_eth_sec_init(eth_dev);
	if (rc)
		goto cq_fini;

	rc = otx2_nix_flow_ctrl_init(eth_dev);
	if (rc) {
		otx2_err("Failed to init flow ctrl mode %d", rc);
		goto cq_fini;
	}

	rc = otx2_nix_mc_addr_list_install(eth_dev);
	if (rc < 0) {
		otx2_err("Failed to install mc address list rc=%d", rc);
		goto sec_fini;
	}

	/*
	 * Restore queue config when reconfigure followed by
	 * reconfigure and no queue configure invoked from application case.
	 */
	if (dev->configured == 1) {
		rc = nix_restore_queue_cfg(eth_dev);
		if (rc)
			goto uninstall_mc_list;
	}

	/* Update the mac address */
	ea = eth_dev->data->mac_addrs;
	memcpy(ea, dev->mac_addr, RTE_ETHER_ADDR_LEN);
	if (rte_is_zero_ether_addr(ea))
		rte_eth_random_addr((uint8_t *)ea);

	rte_ether_format_addr(ea_fmt, RTE_ETHER_ADDR_FMT_SIZE, ea);

	/* Apply new link configurations if changed */
	rc = otx2_apply_link_speed(eth_dev);
	if (rc) {
		otx2_err("Failed to set link configuration");
		goto uninstall_mc_list;
	}

	otx2_nix_dbg("Configured port%d mac=%s nb_rxq=%d nb_txq=%d"
		" rx_offloads=0x%" PRIx64 " tx_offloads=0x%" PRIx64 ""
		" rx_flags=0x%x tx_flags=0x%x",
		eth_dev->data->port_id, ea_fmt, nb_rxq,
		nb_txq, dev->rx_offloads, dev->tx_offloads,
		dev->rx_offload_flags, dev->tx_offload_flags);

	/* All good */
	dev->configured = 1;
	dev->configured_nb_rx_qs = data->nb_rx_queues;
	dev->configured_nb_tx_qs = data->nb_tx_queues;
	return 0;

uninstall_mc_list:
	otx2_nix_mc_addr_list_uninstall(eth_dev);
sec_fini:
	otx2_eth_sec_fini(eth_dev);
cq_fini:
	oxt2_nix_unregister_cq_irqs(eth_dev);
q_irq_fini:
	oxt2_nix_unregister_queue_irqs(eth_dev);
vlan_fini:
	otx2_nix_vlan_fini(eth_dev);
tm_fini:
	otx2_nix_tm_fini(eth_dev);
free_nix_lf:
	nix_lf_free(dev);
fail_offloads:
	dev->rx_offload_flags &= ~nix_rx_offload_flags(eth_dev);
	dev->tx_offload_flags &= ~nix_tx_offload_flags(eth_dev);
fail_configure:
	dev->configured = 0;
	return rc;
}

int
otx2_nix_tx_queue_start(struct rte_eth_dev *eth_dev, uint16_t qidx)
{
	struct rte_eth_dev_data *data = eth_dev->data;
	struct otx2_eth_txq *txq;
	int rc = -EINVAL;

	txq = eth_dev->data->tx_queues[qidx];

	if (data->tx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STARTED)
		return 0;

	rc = otx2_nix_sq_sqb_aura_fc(txq, true);
	if (rc) {
		otx2_err("Failed to enable sqb aura fc, txq=%u, rc=%d",
			 qidx, rc);
		goto done;
	}

	data->tx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STARTED;

done:
	return rc;
}

int
otx2_nix_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t qidx)
{
	struct rte_eth_dev_data *data = eth_dev->data;
	struct otx2_eth_txq *txq;
	int rc;

	txq = eth_dev->data->tx_queues[qidx];

	if (data->tx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STOPPED)
		return 0;

	txq->fc_cache_pkts = 0;

	rc = otx2_nix_sq_sqb_aura_fc(txq, false);
	if (rc) {
		otx2_err("Failed to disable sqb aura fc, txq=%u, rc=%d",
			 qidx, rc);
		goto done;
	}

	data->tx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STOPPED;

done:
	return rc;
}

static int
otx2_nix_rx_queue_start(struct rte_eth_dev *eth_dev, uint16_t qidx)
{
	struct otx2_eth_rxq *rxq = eth_dev->data->rx_queues[qidx];
	struct rte_eth_dev_data *data = eth_dev->data;
	int rc;

	if (data->rx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STARTED)
		return 0;

	rc = nix_rq_enb_dis(rxq->eth_dev, rxq, true);
	if (rc) {
		otx2_err("Failed to enable rxq=%u, rc=%d", qidx, rc);
		goto done;
	}

	data->rx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STARTED;

done:
	return rc;
}

static int
otx2_nix_rx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t qidx)
{
	struct otx2_eth_rxq *rxq = eth_dev->data->rx_queues[qidx];
	struct rte_eth_dev_data *data = eth_dev->data;
	int rc;

	if (data->rx_queue_state[qidx] == RTE_ETH_QUEUE_STATE_STOPPED)
		return 0;

	rc = nix_rq_enb_dis(rxq->eth_dev, rxq, false);
	if (rc) {
		otx2_err("Failed to disable rxq=%u, rc=%d", qidx, rc);
		goto done;
	}

	data->rx_queue_state[qidx] = RTE_ETH_QUEUE_STATE_STOPPED;

done:
	return rc;
}

static int
otx2_nix_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct rte_mbuf *rx_pkts[32];
	struct otx2_eth_rxq *rxq;
	struct rte_eth_link link;
	int count, i, j, rc;

	nix_lf_switch_header_type_enable(dev, false);
	nix_cgx_stop_link_event(dev);
	npc_rx_disable(dev);

	/* Stop rx queues and free up pkts pending */
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		rc = otx2_nix_rx_queue_stop(eth_dev, i);
		if (rc)
			continue;

		rxq = eth_dev->data->rx_queues[i];
		count = dev->rx_pkt_burst_no_offload(rxq, rx_pkts, 32);
		while (count) {
			for (j = 0; j < count; j++)
				rte_pktmbuf_free(rx_pkts[j]);
			count = dev->rx_pkt_burst_no_offload(rxq, rx_pkts, 32);
		}
	}

	/* Stop tx queues  */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		otx2_nix_tx_queue_stop(eth_dev, i);

	/* Bring down link status internally */
	memset(&link, 0, sizeof(link));
	rte_eth_linkstatus_set(eth_dev, &link);

	return 0;
}

static int
otx2_nix_dev_start(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc, i;

	/* MTU recalculate should be avoided here if PTP is enabled by PF, as
	 * otx2_nix_recalc_mtu would be invoked during otx2_nix_ptp_enable_vf
	 * call below.
	 */
	if (eth_dev->data->nb_rx_queues != 0 && !otx2_ethdev_is_ptp_en(dev)) {
		rc = otx2_nix_recalc_mtu(eth_dev);
		if (rc)
			return rc;
	}

	/* Start rx queues */
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		rc = otx2_nix_rx_queue_start(eth_dev, i);
		if (rc)
			return rc;
	}

	/* Start tx queues  */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		rc = otx2_nix_tx_queue_start(eth_dev, i);
		if (rc)
			return rc;
	}

	rc = otx2_nix_update_flow_ctrl_mode(eth_dev);
	if (rc) {
		otx2_err("Failed to update flow ctrl mode %d", rc);
		return rc;
	}

	/* Enable PTP if it was requested by the app or if it is already
	 * enabled in PF owning this VF
	 */
	memset(&dev->tstamp, 0, sizeof(struct otx2_timesync_info));
	if ((dev->rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP) ||
	    otx2_ethdev_is_ptp_en(dev))
		otx2_nix_timesync_enable(eth_dev);
	else
		otx2_nix_timesync_disable(eth_dev);

	/* Update VF about data off shifted by 8 bytes if PTP already
	 * enabled in PF owning this VF
	 */
	if (otx2_ethdev_is_ptp_en(dev) && otx2_dev_is_vf(dev))
		otx2_nix_ptp_enable_vf(eth_dev);

	if (dev->rx_offload_flags & NIX_RX_OFFLOAD_TSTAMP_F) {
		rc = rte_mbuf_dyn_rx_timestamp_register(
				&dev->tstamp.tstamp_dynfield_offset,
				&dev->tstamp.rx_tstamp_dynflag);
		if (rc != 0) {
			otx2_err("Failed to register Rx timestamp field/flag");
			return -rte_errno;
		}
	}

	rc = npc_rx_enable(dev);
	if (rc) {
		otx2_err("Failed to enable NPC rx %d", rc);
		return rc;
	}

	otx2_nix_toggle_flag_link_cfg(dev, true);

	rc = nix_cgx_start_link_event(dev);
	if (rc) {
		otx2_err("Failed to start cgx link event %d", rc);
		goto rx_disable;
	}

	otx2_nix_toggle_flag_link_cfg(dev, false);
	otx2_eth_set_tx_function(eth_dev);
	otx2_eth_set_rx_function(eth_dev);

	return 0;

rx_disable:
	npc_rx_disable(dev);
	otx2_nix_toggle_flag_link_cfg(dev, false);
	return rc;
}

static int otx2_nix_dev_reset(struct rte_eth_dev *eth_dev);
static int otx2_nix_dev_close(struct rte_eth_dev *eth_dev);

/* Initialize and register driver with DPDK Application */
static const struct eth_dev_ops otx2_eth_dev_ops = {
	.dev_infos_get            = otx2_nix_info_get,
	.dev_configure            = otx2_nix_configure,
	.link_update              = otx2_nix_link_update,
	.tx_queue_setup           = otx2_nix_tx_queue_setup,
	.tx_queue_release         = otx2_nix_tx_queue_release,
	.tm_ops_get               = otx2_nix_tm_ops_get,
	.rx_queue_setup           = otx2_nix_rx_queue_setup,
	.rx_queue_release         = otx2_nix_rx_queue_release,
	.dev_start                = otx2_nix_dev_start,
	.dev_stop                 = otx2_nix_dev_stop,
	.dev_close                = otx2_nix_dev_close,
	.tx_queue_start           = otx2_nix_tx_queue_start,
	.tx_queue_stop            = otx2_nix_tx_queue_stop,
	.rx_queue_start           = otx2_nix_rx_queue_start,
	.rx_queue_stop            = otx2_nix_rx_queue_stop,
	.dev_set_link_up          = otx2_nix_dev_set_link_up,
	.dev_set_link_down        = otx2_nix_dev_set_link_down,
	.dev_supported_ptypes_get = otx2_nix_supported_ptypes_get,
	.dev_ptypes_set           = otx2_nix_ptypes_set,
	.dev_reset                = otx2_nix_dev_reset,
	.stats_get                = otx2_nix_dev_stats_get,
	.stats_reset              = otx2_nix_dev_stats_reset,
	.get_reg                  = otx2_nix_dev_get_reg,
	.mtu_set                  = otx2_nix_mtu_set,
	.mac_addr_add             = otx2_nix_mac_addr_add,
	.mac_addr_remove          = otx2_nix_mac_addr_del,
	.mac_addr_set             = otx2_nix_mac_addr_set,
	.set_mc_addr_list         = otx2_nix_set_mc_addr_list,
	.promiscuous_enable       = otx2_nix_promisc_enable,
	.promiscuous_disable      = otx2_nix_promisc_disable,
	.allmulticast_enable      = otx2_nix_allmulticast_enable,
	.allmulticast_disable     = otx2_nix_allmulticast_disable,
	.queue_stats_mapping_set  = otx2_nix_queue_stats_mapping,
	.reta_update              = otx2_nix_dev_reta_update,
	.reta_query               = otx2_nix_dev_reta_query,
	.rss_hash_update          = otx2_nix_rss_hash_update,
	.rss_hash_conf_get        = otx2_nix_rss_hash_conf_get,
	.xstats_get               = otx2_nix_xstats_get,
	.xstats_get_names         = otx2_nix_xstats_get_names,
	.xstats_reset             = otx2_nix_xstats_reset,
	.xstats_get_by_id         = otx2_nix_xstats_get_by_id,
	.xstats_get_names_by_id   = otx2_nix_xstats_get_names_by_id,
	.rxq_info_get             = otx2_nix_rxq_info_get,
	.txq_info_get             = otx2_nix_txq_info_get,
	.rx_burst_mode_get        = otx2_rx_burst_mode_get,
	.tx_burst_mode_get        = otx2_tx_burst_mode_get,
	.tx_done_cleanup          = otx2_nix_tx_done_cleanup,
	.set_queue_rate_limit     = otx2_nix_tm_set_queue_rate_limit,
	.pool_ops_supported       = otx2_nix_pool_ops_supported,
	.flow_ops_get             = otx2_nix_dev_flow_ops_get,
	.get_module_info          = otx2_nix_get_module_info,
	.get_module_eeprom        = otx2_nix_get_module_eeprom,
	.fw_version_get           = otx2_nix_fw_version_get,
	.flow_ctrl_get            = otx2_nix_flow_ctrl_get,
	.flow_ctrl_set            = otx2_nix_flow_ctrl_set,
	.timesync_enable          = otx2_nix_timesync_enable,
	.timesync_disable         = otx2_nix_timesync_disable,
	.timesync_read_rx_timestamp = otx2_nix_timesync_read_rx_timestamp,
	.timesync_read_tx_timestamp = otx2_nix_timesync_read_tx_timestamp,
	.timesync_adjust_time     = otx2_nix_timesync_adjust_time,
	.timesync_read_time       = otx2_nix_timesync_read_time,
	.timesync_write_time      = otx2_nix_timesync_write_time,
	.vlan_offload_set         = otx2_nix_vlan_offload_set,
	.vlan_filter_set	  = otx2_nix_vlan_filter_set,
	.vlan_strip_queue_set	  = otx2_nix_vlan_strip_queue_set,
	.vlan_tpid_set		  = otx2_nix_vlan_tpid_set,
	.vlan_pvid_set		  = otx2_nix_vlan_pvid_set,
	.rx_queue_intr_enable	  = otx2_nix_rx_queue_intr_enable,
	.rx_queue_intr_disable	  = otx2_nix_rx_queue_intr_disable,
	.read_clock		  = otx2_nix_read_clock,
};

static inline int
nix_lf_attach(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct rsrc_attach_req *req;

	/* Attach NIX(lf) */
	req = otx2_mbox_alloc_msg_attach_resources(mbox);
	req->modify = true;
	req->nixlf = true;

	return otx2_mbox_process(mbox);
}

static inline int
nix_lf_get_msix_offset(struct otx2_eth_dev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct msix_offset_rsp *msix_rsp;
	int rc;

	/* Get NPA and NIX MSIX vector offsets */
	otx2_mbox_alloc_msg_msix_offset(mbox);

	rc = otx2_mbox_process_msg(mbox, (void *)&msix_rsp);

	dev->nix_msixoff = msix_rsp->nix_msixoff;

	return rc;
}

static inline int
otx2_eth_dev_lf_detach(struct otx2_mbox *mbox)
{
	struct rsrc_detach_req *req;

	req = otx2_mbox_alloc_msg_detach_resources(mbox);

	/* Detach all except npa lf */
	req->partial = true;
	req->nixlf = true;
	req->sso = true;
	req->ssow = true;
	req->timlfs = true;
	req->cptlfs = true;

	return otx2_mbox_process(mbox);
}

static bool
otx2_eth_dev_is_sdp(struct rte_pci_device *pci_dev)
{
	if (pci_dev->id.device_id == PCI_DEVID_OCTEONTX2_RVU_SDP_PF ||
	    pci_dev->id.device_id == PCI_DEVID_OCTEONTX2_RVU_SDP_VF)
		return true;
	return false;
}

static inline uint64_t
nix_get_blkaddr(struct otx2_eth_dev *dev)
{
	uint64_t reg;

	/* Reading the discovery register to know which NIX is the LF
	 * attached to.
	 */
	reg = otx2_read64(dev->bar2 +
			  RVU_PF_BLOCK_ADDRX_DISC(RVU_BLOCK_ADDR_NIX0));

	return reg & 0x1FFULL ? RVU_BLOCK_ADDR_NIX0 : RVU_BLOCK_ADDR_NIX1;
}

static int
otx2_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct rte_pci_device *pci_dev;
	int rc, max_entries;

	eth_dev->dev_ops = &otx2_eth_dev_ops;
	eth_dev->rx_queue_count = otx2_nix_rx_queue_count;
	eth_dev->rx_descriptor_status = otx2_nix_rx_descriptor_status;
	eth_dev->tx_descriptor_status = otx2_nix_tx_descriptor_status;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		/* Setup callbacks for secondary process */
		otx2_eth_set_tx_function(eth_dev);
		otx2_eth_set_rx_function(eth_dev);
		return 0;
	}

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	rte_eth_copy_pci_info(eth_dev, pci_dev);
	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	/* Zero out everything after OTX2_DEV to allow proper dev_reset() */
	memset(&dev->otx2_eth_dev_data_start, 0, sizeof(*dev) -
		offsetof(struct otx2_eth_dev, otx2_eth_dev_data_start));

	/* Parse devargs string */
	rc = otx2_ethdev_parse_devargs(eth_dev->device->devargs, dev);
	if (rc) {
		otx2_err("Failed to parse devargs rc=%d", rc);
		goto error;
	}

	if (!dev->mbox_active) {
		/* Initialize the base otx2_dev object
		 * only if already present
		 */
		rc = otx2_dev_init(pci_dev, dev);
		if (rc) {
			otx2_err("Failed to initialize otx2_dev rc=%d", rc);
			goto error;
		}
	}
	if (otx2_eth_dev_is_sdp(pci_dev))
		dev->sdp_link = true;
	else
		dev->sdp_link = false;
	/* Device generic callbacks */
	dev->ops = &otx2_dev_ops;
	dev->eth_dev = eth_dev;

	/* Grab the NPA LF if required */
	rc = otx2_npa_lf_init(pci_dev, dev);
	if (rc)
		goto otx2_dev_uninit;

	dev->configured = 0;
	dev->drv_inited = true;
	dev->ptype_disable = 0;
	dev->lmt_addr = dev->bar2 + (RVU_BLOCK_ADDR_LMT << 20);

	/* Attach NIX LF */
	rc = nix_lf_attach(dev);
	if (rc)
		goto otx2_npa_uninit;

	dev->base = dev->bar2 + (nix_get_blkaddr(dev) << 20);

	/* Get NIX MSIX offset */
	rc = nix_lf_get_msix_offset(dev);
	if (rc)
		goto otx2_npa_uninit;

	/* Register LF irq handlers */
	rc = otx2_nix_register_irqs(eth_dev);
	if (rc)
		goto mbox_detach;

	/* Get maximum number of supported MAC entries */
	max_entries = otx2_cgx_mac_max_entries_get(dev);
	if (max_entries < 0) {
		otx2_err("Failed to get max entries for mac addr");
		rc = -ENOTSUP;
		goto unregister_irq;
	}

	/* For VFs, returned max_entries will be 0. But to keep default MAC
	 * address, one entry must be allocated. So setting up to 1.
	 */
	if (max_entries == 0)
		max_entries = 1;

	eth_dev->data->mac_addrs = rte_zmalloc("mac_addr", max_entries *
					       RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		otx2_err("Failed to allocate memory for mac addr");
		rc = -ENOMEM;
		goto unregister_irq;
	}

	dev->max_mac_entries = max_entries;

	rc = otx2_nix_mac_addr_get(eth_dev, dev->mac_addr);
	if (rc)
		goto free_mac_addrs;

	/* Update the mac address */
	memcpy(eth_dev->data->mac_addrs, dev->mac_addr, RTE_ETHER_ADDR_LEN);

	/* Also sync same MAC address to CGX table */
	otx2_cgx_mac_addr_set(eth_dev, &eth_dev->data->mac_addrs[0]);

	/* Initialize the tm data structures */
	otx2_nix_tm_conf_init(eth_dev);

	dev->tx_offload_capa = nix_get_tx_offload_capa(dev);
	dev->rx_offload_capa = nix_get_rx_offload_capa(dev);

	if (otx2_dev_is_96xx_A0(dev) ||
	    otx2_dev_is_95xx_Ax(dev)) {
		dev->hwcap |= OTX2_FIXUP_F_MIN_4K_Q;
		dev->hwcap |= OTX2_FIXUP_F_LIMIT_CQ_FULL;
	}

	/* Create security ctx */
	rc = otx2_eth_sec_ctx_create(eth_dev);
	if (rc)
		goto free_mac_addrs;
	dev->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_SECURITY;
	dev->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_SECURITY;

	/* Initialize rte-flow */
	rc = otx2_flow_init(dev);
	if (rc)
		goto sec_ctx_destroy;

	otx2_nix_mc_filter_init(dev);

	otx2_nix_dbg("Port=%d pf=%d vf=%d ver=%s msix_off=%d hwcap=0x%" PRIx64
		     " rxoffload_capa=0x%" PRIx64 " txoffload_capa=0x%" PRIx64,
		     eth_dev->data->port_id, dev->pf, dev->vf,
		     OTX2_ETH_DEV_PMD_VERSION, dev->nix_msixoff, dev->hwcap,
		     dev->rx_offload_capa, dev->tx_offload_capa);
	return 0;

sec_ctx_destroy:
	otx2_eth_sec_ctx_destroy(eth_dev);
free_mac_addrs:
	rte_free(eth_dev->data->mac_addrs);
unregister_irq:
	otx2_nix_unregister_irqs(eth_dev);
mbox_detach:
	otx2_eth_dev_lf_detach(dev->mbox);
otx2_npa_uninit:
	otx2_npa_lf_fini();
otx2_dev_uninit:
	otx2_dev_fini(pci_dev, dev);
error:
	otx2_err("Failed to init nix eth_dev rc=%d", rc);
	return rc;
}

static int
otx2_eth_dev_uninit(struct rte_eth_dev *eth_dev, bool mbox_close)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct rte_pci_device *pci_dev;
	int rc, i;

	/* Nothing to be done for secondary processes */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Clear the flag since we are closing down */
	dev->configured = 0;

	/* Disable nix bpid config */
	otx2_nix_rxchan_bpid_cfg(eth_dev, false);

	npc_rx_disable(dev);

	/* Disable vlan offloads */
	otx2_nix_vlan_fini(eth_dev);

	/* Disable other rte_flow entries */
	otx2_flow_fini(dev);

	/* Free multicast filter list */
	otx2_nix_mc_filter_fini(dev);

	/* Disable PTP if already enabled */
	if (otx2_ethdev_is_ptp_en(dev))
		otx2_nix_timesync_disable(eth_dev);

	nix_cgx_stop_link_event(dev);

	/* Unregister the dev ops, this is required to stop VFs from
	 * receiving link status updates on exit path.
	 */
	dev->ops = NULL;

	/* Free up SQs */
	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		otx2_nix_tx_queue_release(eth_dev, i);
	eth_dev->data->nb_tx_queues = 0;

	/* Free up RQ's and CQ's */
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++)
		otx2_nix_rx_queue_release(eth_dev, i);
	eth_dev->data->nb_rx_queues = 0;

	/* Free tm resources */
	rc = otx2_nix_tm_fini(eth_dev);
	if (rc)
		otx2_err("Failed to cleanup tm, rc=%d", rc);

	/* Unregister queue irqs */
	oxt2_nix_unregister_queue_irqs(eth_dev);

	/* Unregister cq irqs */
	if (eth_dev->data->dev_conf.intr_conf.rxq)
		oxt2_nix_unregister_cq_irqs(eth_dev);

	rc = nix_lf_free(dev);
	if (rc)
		otx2_err("Failed to free nix lf, rc=%d", rc);

	rc = otx2_npa_lf_fini();
	if (rc)
		otx2_err("Failed to cleanup npa lf, rc=%d", rc);

	/* Disable security */
	otx2_eth_sec_fini(eth_dev);

	/* Destroy security ctx */
	otx2_eth_sec_ctx_destroy(eth_dev);

	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;
	dev->drv_inited = false;

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	otx2_nix_unregister_irqs(eth_dev);

	rc = otx2_eth_dev_lf_detach(dev->mbox);
	if (rc)
		otx2_err("Failed to detach resources, rc=%d", rc);

	/* Check if mbox close is needed */
	if (!mbox_close)
		return 0;

	if (otx2_npa_lf_active(dev) || otx2_dev_active_vfs(dev)) {
		/* Will be freed later by PMD */
		eth_dev->data->dev_private = NULL;
		return 0;
	}

	otx2_dev_fini(pci_dev, dev);
	return 0;
}

static int
otx2_nix_dev_close(struct rte_eth_dev *eth_dev)
{
	otx2_eth_dev_uninit(eth_dev, true);
	return 0;
}

static int
otx2_nix_dev_reset(struct rte_eth_dev *eth_dev)
{
	int rc;

	rc = otx2_eth_dev_uninit(eth_dev, false);
	if (rc)
		return rc;

	return otx2_eth_dev_init(eth_dev);
}

static int
nix_remove(struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *eth_dev;
	struct otx2_idev_cfg *idev;
	struct otx2_dev *otx2_dev;
	int rc;

	eth_dev = rte_eth_dev_allocated(pci_dev->device.name);
	if (eth_dev) {
		/* Cleanup eth dev */
		rc = otx2_eth_dev_uninit(eth_dev, true);
		if (rc)
			return rc;

		rte_eth_dev_release_port(eth_dev);
	}

	/* Nothing to be done for secondary processes */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Check for common resources */
	idev = otx2_intra_dev_get_cfg();
	if (!idev || !idev->npa_lf || idev->npa_lf->pci_dev != pci_dev)
		return 0;

	otx2_dev = container_of(idev->npa_lf, struct otx2_dev, npalf);

	if (otx2_npa_lf_active(otx2_dev) || otx2_dev_active_vfs(otx2_dev))
		goto exit;

	/* Safe to cleanup mbox as no more users */
	otx2_dev_fini(pci_dev, otx2_dev);
	rte_free(otx2_dev);
	return 0;

exit:
	otx2_info("%s: common resource in use by other devices", pci_dev->name);
	return -EAGAIN;
}

static int
nix_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	int rc;

	RTE_SET_USED(pci_drv);

	rc = rte_eth_dev_pci_generic_probe(pci_dev, sizeof(struct otx2_eth_dev),
					   otx2_eth_dev_init);

	/* On error on secondary, recheck if port exists in primary or
	 * in mid of detach state.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY && rc)
		if (!rte_eth_dev_allocated(pci_dev->device.name))
			return 0;
	return rc;
}

static const struct rte_pci_id pci_nix_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_OCTEONTX2_RVU_PF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_OCTEONTX2_RVU_VF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_RVU_AF_VF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_RVU_SDP_PF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_RVU_SDP_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_nix = {
	.id_table = pci_nix_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA |
			RTE_PCI_DRV_INTR_LSC,
	.probe = nix_probe,
	.remove = nix_remove,
};

RTE_PMD_REGISTER_PCI(OCTEONTX2_PMD, pci_nix);
RTE_PMD_REGISTER_PCI_TABLE(OCTEONTX2_PMD, pci_nix_map);
RTE_PMD_REGISTER_KMOD_DEP(OCTEONTX2_PMD, "vfio-pci");
