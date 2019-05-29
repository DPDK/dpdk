/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <inttypes.h>
#include <math.h>

#include <rte_ethdev_pci.h>
#include <rte_io.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_mempool.h>

#include "otx2_ethdev.h"

static inline void
otx2_eth_set_rx_function(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);
}

static inline void
otx2_eth_set_tx_function(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);
}

static inline uint64_t
nix_get_rx_offload_capa(struct otx2_eth_dev *dev)
{
	uint64_t capa = NIX_RX_OFFLOAD_CAPA;

	if (otx2_dev_is_vf(dev))
		capa &= ~DEV_RX_OFFLOAD_TIMESTAMP;

	return capa;
}

static inline uint64_t
nix_get_tx_offload_capa(struct otx2_eth_dev *dev)
{
	RTE_SET_USED(dev);

	return NIX_TX_OFFLOAD_CAPA;
}

static const struct otx2_dev_ops otx2_dev_ops = {
	.link_status_update = otx2_eth_dev_link_status_update,
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
	if (dev->rx_offloads & (DEV_RX_OFFLOAD_TCP_CKSUM |
			 DEV_RX_OFFLOAD_UDP_CKSUM)) {
		req->rx_cfg |= BIT_ULL(37 /* CSUM_OL4 */);
		req->rx_cfg |= BIT_ULL(36 /* CSUM_IL4 */);
	}

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

	return 0;
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
		rc = -ENOMEM;
		goto fail;
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

	if (otx2_ethdev_fixup_is_limit_cq_full(dev)) {
		uint16_t min_rx_drop;
		const float rx_cq_skid = 1024 * 256;

		min_rx_drop = ceil(rx_cq_skid / (float)cq_size);
		aq->cq.drop = min_rx_drop;
		aq->cq.drop_ena = 1;
	}

	rc = otx2_mbox_process(mbox);
	if (rc) {
		otx2_err("Failed to init cq context");
		goto fail;
	}

	aq = otx2_mbox_alloc_msg_nix_aq_enq(mbox);
	aq->qidx = qid;
	aq->ctype = NIX_AQ_CTYPE_RQ;
	aq->op = NIX_AQ_INSTOP_INIT;

	aq->rq.sso_ena = 0;
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
	aq->rq.lpb_sizem1 = rte_pktmbuf_data_room_size(mp);
	aq->rq.lpb_sizem1 += rte_pktmbuf_priv_size(mp);
	aq->rq.lpb_sizem1 += sizeof(struct rte_mbuf);
	aq->rq.lpb_sizem1 /= 8;
	aq->rq.lpb_sizem1 -= 1; /* Expressed in size minus one */
	aq->rq.ena = 1;
	aq->rq.pb_caching = 0x2; /* First cache aligned block to LLC */
	aq->rq.xqe_imm_size = 0; /* No pkt data copy to CQE */
	aq->rq.rq_int_ena = 0;
	/* Many to one reduction */
	aq->rq.qint_idx = qid % dev->qints;

	if (otx2_ethdev_fixup_is_limit_cq_full(dev))
		aq->rq.xqe_drop_ena = 1;

	rc = otx2_mbox_process(mbox);
	if (rc) {
		otx2_err("Failed to init rq context");
		goto fail;
	}

	return 0;
fail:
	return rc;
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

	return 0;
}

static inline int
nix_get_data_off(struct otx2_eth_dev *dev)
{
	RTE_SET_USED(dev);

	return 0;
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
otx2_nix_rx_queue_release(void *rx_queue)
{
	struct otx2_eth_rxq *rxq = rx_queue;

	if (!rxq)
		return;

	otx2_nix_dbg("Releasing rxq %u", rxq->rq);
	nix_cq_rq_uninit(rxq->eth_dev, rxq);
	rte_free(rx_queue);
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
		otx2_nix_rx_queue_release(eth_dev->data->rx_queues[rq]);
		eth_dev->data->rx_queues[rq] = NULL;
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

	eth_dev->data->rx_queues[rq] = rxq;
	eth_dev->data->rx_queue_state[rq] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;

free_rxq:
	otx2_nix_rx_queue_release(rxq);
fail:
	return rc;
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
		goto fail;
	}

	if (rte_eal_iova_mode() != RTE_IOVA_VA) {
		otx2_err("iova mode should be va");
		goto fail;
	}

	if (conf->link_speeds & ETH_LINK_SPEED_FIXED) {
		otx2_err("Setting link speed/duplex not supported");
		goto fail;
	}

	if (conf->dcb_capability_en == 1) {
		otx2_err("dcb enable is not supported");
		goto fail;
	}

	if (conf->fdir_conf.mode != RTE_FDIR_MODE_NONE) {
		otx2_err("Flow director is not supported");
		goto fail;
	}

	if (rxmode->mq_mode != ETH_MQ_RX_NONE &&
	    rxmode->mq_mode != ETH_MQ_RX_RSS) {
		otx2_err("Unsupported mq rx mode %d", rxmode->mq_mode);
		goto fail;
	}

	if (txmode->mq_mode != ETH_MQ_TX_NONE) {
		otx2_err("Unsupported mq tx mode %d", txmode->mq_mode);
		goto fail;
	}

	/* Free the resources allocated from the previous configure */
	if (dev->configured == 1) {
		oxt2_nix_unregister_queue_irqs(eth_dev);
		nix_lf_free(dev);
	}

	if (otx2_dev_is_A0(dev) &&
	    (txmode->offloads & DEV_TX_OFFLOAD_SCTP_CKSUM) &&
	    ((txmode->offloads & DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM) ||
	    (txmode->offloads & DEV_TX_OFFLOAD_OUTER_UDP_CKSUM))) {
		otx2_err("Outer IP and SCTP checksum unsupported");
		rc = -EINVAL;
		goto fail;
	}

	dev->rx_offloads = rxmode->offloads;
	dev->tx_offloads = txmode->offloads;
	dev->rss_info.rss_grps = NIX_RSS_GRPS;

	nb_rxq = RTE_MAX(data->nb_rx_queues, 1);
	nb_txq = RTE_MAX(data->nb_tx_queues, 1);

	/* Alloc a nix lf */
	rc = nix_lf_alloc(dev, nb_rxq, nb_txq);
	if (rc) {
		otx2_err("Failed to init nix_lf rc=%d", rc);
		goto fail;
	}

	/* Configure RSS */
	rc = otx2_nix_rss_config(eth_dev);
	if (rc) {
		otx2_err("Failed to configure rss rc=%d", rc);
		goto free_nix_lf;
	}

	/* Register queue IRQs */
	rc = oxt2_nix_register_queue_irqs(eth_dev);
	if (rc) {
		otx2_err("Failed to register queue interrupts rc=%d", rc);
		goto free_nix_lf;
	}

	/* Update the mac address */
	ea = eth_dev->data->mac_addrs;
	memcpy(ea, dev->mac_addr, RTE_ETHER_ADDR_LEN);
	if (rte_is_zero_ether_addr(ea))
		rte_eth_random_addr((uint8_t *)ea);

	rte_ether_format_addr(ea_fmt, RTE_ETHER_ADDR_FMT_SIZE, ea);

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

free_nix_lf:
	rc = nix_lf_free(dev);
fail:
	return rc;
}

/* Initialize and register driver with DPDK Application */
static const struct eth_dev_ops otx2_eth_dev_ops = {
	.dev_infos_get            = otx2_nix_info_get,
	.dev_configure            = otx2_nix_configure,
	.link_update              = otx2_nix_link_update,
	.rx_queue_setup           = otx2_nix_rx_queue_setup,
	.rx_queue_release         = otx2_nix_rx_queue_release,
	.stats_get                = otx2_nix_dev_stats_get,
	.stats_reset              = otx2_nix_dev_stats_reset,
	.get_reg                  = otx2_nix_dev_get_reg,
	.mac_addr_add             = otx2_nix_mac_addr_add,
	.mac_addr_remove          = otx2_nix_mac_addr_del,
	.mac_addr_set             = otx2_nix_mac_addr_set,
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

static int
otx2_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct rte_pci_device *pci_dev;
	int rc, max_entries;

	eth_dev->dev_ops = &otx2_eth_dev_ops;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		/* Setup callbacks for secondary process */
		otx2_eth_set_tx_function(eth_dev);
		otx2_eth_set_rx_function(eth_dev);
		return 0;
	}

	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	rte_eth_copy_pci_info(eth_dev, pci_dev);
	eth_dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

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
	/* Device generic callbacks */
	dev->ops = &otx2_dev_ops;
	dev->eth_dev = eth_dev;

	/* Grab the NPA LF if required */
	rc = otx2_npa_lf_init(pci_dev, dev);
	if (rc)
		goto otx2_dev_uninit;

	dev->configured = 0;
	dev->drv_inited = true;
	dev->base = dev->bar2 + (RVU_BLOCK_ADDR_NIX0 << 20);
	dev->lmt_addr = dev->bar2 + (RVU_BLOCK_ADDR_LMT << 20);

	/* Attach NIX LF */
	rc = nix_lf_attach(dev);
	if (rc)
		goto otx2_npa_uninit;

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

	dev->tx_offload_capa = nix_get_tx_offload_capa(dev);
	dev->rx_offload_capa = nix_get_rx_offload_capa(dev);

	if (otx2_dev_is_A0(dev)) {
		dev->hwcap |= OTX2_FIXUP_F_MIN_4K_Q;
		dev->hwcap |= OTX2_FIXUP_F_LIMIT_CQ_FULL;
	}

	otx2_nix_dbg("Port=%d pf=%d vf=%d ver=%s msix_off=%d hwcap=0x%" PRIx64
		     " rxoffload_capa=0x%" PRIx64 " txoffload_capa=0x%" PRIx64,
		     eth_dev->data->port_id, dev->pf, dev->vf,
		     OTX2_ETH_DEV_PMD_VERSION, dev->nix_msixoff, dev->hwcap,
		     dev->rx_offload_capa, dev->tx_offload_capa);
	return 0;

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
	int rc;

	/* Nothing to be done for secondary processes */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	/* Unregister queue irqs */
	oxt2_nix_unregister_queue_irqs(eth_dev);

	rc = nix_lf_free(dev);
	if (rc)
		otx2_err("Failed to free nix lf, rc=%d", rc);

	rc = otx2_npa_lf_fini();
	if (rc)
		otx2_err("Failed to cleanup npa lf, rc=%d", rc);

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

		rte_eth_dev_pci_release(eth_dev);
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
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_nix = {
	.id_table = pci_nix_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_IOVA_AS_VA |
			RTE_PCI_DRV_INTR_LSC,
	.probe = nix_probe,
	.remove = nix_remove,
};

RTE_PMD_REGISTER_PCI(net_octeontx2, pci_nix);
RTE_PMD_REGISTER_PCI_TABLE(net_octeontx2, pci_nix_map);
RTE_PMD_REGISTER_KMOD_DEP(net_octeontx2, "vfio-pci");
