/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include "cn10k_ethdev.h"
#include "cn10k_flow.h"
#include "cn10k_rx.h"
#include "cn10k_tx.h"

static uint16_t
nix_rx_offload_flags(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct rte_eth_dev_data *data = eth_dev->data;
	struct rte_eth_conf *conf = &data->dev_conf;
	struct rte_eth_rxmode *rxmode = &conf->rxmode;
	uint16_t flags = 0;

	if (rxmode->mq_mode == RTE_ETH_MQ_RX_RSS &&
	    (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_RSS_HASH))
		flags |= NIX_RX_OFFLOAD_RSS_F;

	if (dev->rx_offloads &
	    (RTE_ETH_RX_OFFLOAD_TCP_CKSUM | RTE_ETH_RX_OFFLOAD_UDP_CKSUM))
		flags |= NIX_RX_OFFLOAD_CHECKSUM_F;

	if (dev->rx_offloads &
	    (RTE_ETH_RX_OFFLOAD_IPV4_CKSUM | RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM))
		flags |= NIX_RX_OFFLOAD_CHECKSUM_F;

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER)
		flags |= NIX_RX_MULTI_SEG_F;

	if ((dev->rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP))
		flags |= NIX_RX_OFFLOAD_TSTAMP_F;

	if (!dev->ptype_disable)
		flags |= NIX_RX_OFFLOAD_PTYPE_F;

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY)
		flags |= NIX_RX_OFFLOAD_SECURITY_F;

	return flags;
}

static uint16_t
nix_tx_offload_flags(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	uint64_t conf = dev->tx_offloads;
	struct roc_nix *nix = &dev->nix;
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
			 offsetof(struct rte_mbuf, buf_addr) + 16);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, ol_flags) !=
			 offsetof(struct rte_mbuf, buf_addr) + 24);
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
	    conf & RTE_ETH_TX_OFFLOAD_UDP_CKSUM || conf & RTE_ETH_TX_OFFLOAD_SCTP_CKSUM)
		flags |= NIX_TX_OFFLOAD_L3_L4_CSUM_F;

	if (!(conf & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE))
		flags |= NIX_TX_OFFLOAD_MBUF_NOFF_F;

	if (conf & RTE_ETH_TX_OFFLOAD_MULTI_SEGS)
		flags |= NIX_TX_MULTI_SEG_F;

	/* Enable Inner checksum for TSO */
	if (conf & RTE_ETH_TX_OFFLOAD_TCP_TSO)
		flags |= (NIX_TX_OFFLOAD_TSO_F | NIX_TX_OFFLOAD_L3_L4_CSUM_F);

	/* Enable Inner and Outer checksum for Tunnel TSO */
	if (conf & (RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
		    RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO | RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO))
		flags |= (NIX_TX_OFFLOAD_TSO_F | NIX_TX_OFFLOAD_OL3_OL4_CSUM_F |
			  NIX_TX_OFFLOAD_L3_L4_CSUM_F);

	if ((dev->rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP))
		flags |= NIX_TX_OFFLOAD_TSTAMP_F;

	if (conf & RTE_ETH_TX_OFFLOAD_SECURITY)
		flags |= NIX_TX_OFFLOAD_SECURITY_F;

	if (dev->tx_mark)
		flags |= NIX_TX_OFFLOAD_VLAN_QINQ_F;

	if (nix->tx_compl_ena)
		flags |= NIX_TX_OFFLOAD_MBUF_NOFF_F;

	return flags;
}

static int
cn10k_nix_ptypes_set(struct rte_eth_dev *eth_dev, uint32_t ptype_mask)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	if (ptype_mask) {
		dev->rx_offload_flags |= NIX_RX_OFFLOAD_PTYPE_F;
		dev->ptype_disable = 0;
	} else {
		dev->rx_offload_flags &= ~NIX_RX_OFFLOAD_PTYPE_F;
		dev->ptype_disable = 1;
	}

	cn10k_eth_set_rx_function(eth_dev);
	return 0;
}

static void
nix_form_default_desc(struct cnxk_eth_dev *dev, struct cn10k_eth_txq *txq,
		      uint16_t qid)
{
	union nix_send_hdr_w0_u send_hdr_w0;

	/* Initialize the fields based on basic single segment packet */
	send_hdr_w0.u = 0;
	if (dev->tx_offload_flags & NIX_TX_NEED_EXT_HDR) {
		/* 2(HDR) + 2(EXT_HDR) + 1(SG) + 1(IOVA) = 6/2 - 1 = 2 */
		send_hdr_w0.sizem1 = 2;
		if (dev->tx_offload_flags & NIX_TX_OFFLOAD_TSTAMP_F) {
			/* Default: one seg packet would have:
			 * 2(HDR) + 2(EXT) + 1(SG) + 1(IOVA) + 2(MEM)
			 * => 8/2 - 1 = 3
			 */
			send_hdr_w0.sizem1 = 3;

			/* To calculate the offset for send_mem,
			 * send_hdr->w0.sizem1 * 2
			 */
			txq->ts_mem = dev->tstamp.tx_tstamp_iova;
		}
	} else {
		/* 2(HDR) + 1(SG) + 1(IOVA) = 4/2 - 1 = 1 */
		send_hdr_w0.sizem1 = 1;
	}
	send_hdr_w0.sq = qid;
	txq->send_hdr_w0 = send_hdr_w0.u;
	rte_wmb();
}

static int
cn10k_nix_tx_compl_setup(struct cnxk_eth_dev *dev,
		struct cn10k_eth_txq *txq,
		struct roc_nix_sq *sq, uint16_t nb_desc)
{
	struct roc_nix_cq *cq;

	cq = &dev->cqs[sq->cqid];
	txq->tx_compl.desc_base = (uintptr_t)cq->desc_base;
	txq->tx_compl.cq_door = cq->door;
	txq->tx_compl.cq_status = cq->status;
	txq->tx_compl.wdata = cq->wdata;
	txq->tx_compl.head = cq->head;
	txq->tx_compl.qmask = cq->qmask;
	/* Total array size holding buffers is equal to
	 * number of entries in cq and sq
	 * max buffer in array = desc in cq + desc in sq
	 */
	txq->tx_compl.nb_desc_mask = (2 * rte_align32pow2(nb_desc)) - 1;
	txq->tx_compl.ena = true;

	txq->tx_compl.ptr = (struct rte_mbuf **)plt_zmalloc(txq->tx_compl.nb_desc_mask *
			sizeof(struct rte_mbuf *), 0);
	if (!txq->tx_compl.ptr)
		return -1;

	return 0;
}

static void
cn10k_nix_tx_queue_release(struct rte_eth_dev *eth_dev, uint16_t qid)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	struct cn10k_eth_txq *txq;

	cnxk_nix_tx_queue_release(eth_dev, qid);
	txq = eth_dev->data->tx_queues[qid];

	if (nix->tx_compl_ena)
		plt_free(txq->tx_compl.ptr);
}

static int
cn10k_nix_tx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t qid,
			 uint16_t nb_desc, unsigned int socket,
			 const struct rte_eth_txconf *tx_conf)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	uint64_t mark_fmt, mark_flag;
	struct roc_cpt_lf *inl_lf;
	struct cn10k_eth_txq *txq;
	struct roc_nix_sq *sq;
	uint16_t crypto_qid;
	int rc;

	RTE_SET_USED(socket);

	/* Common Tx queue setup */
	rc = cnxk_nix_tx_queue_setup(eth_dev, qid, nb_desc,
				     sizeof(struct cn10k_eth_txq), tx_conf);
	if (rc)
		return rc;

	sq = &dev->sqs[qid];
	/* Update fast path queue */
	txq = eth_dev->data->tx_queues[qid];
	txq->fc_mem = sq->fc;
	if (nix->tx_compl_ena) {
		rc = cn10k_nix_tx_compl_setup(dev, txq, sq, nb_desc);
		if (rc)
			return rc;
	}

	/* Set Txq flag for MT_LOCKFREE */
	txq->flag = !!(dev->tx_offloads & RTE_ETH_TX_OFFLOAD_MT_LOCKFREE);

	/* Store lmt base in tx queue for easy access */
	txq->lmt_base = nix->lmt_base;
	txq->io_addr = sq->io_addr;
	txq->nb_sqb_bufs_adj = sq->nb_sqb_bufs_adj;
	txq->sqes_per_sqb_log2 = sq->sqes_per_sqb_log2;

	/* Fetch CPT LF info for outbound if present */
	if (dev->outb.lf_base) {
		crypto_qid = qid % dev->outb.nb_crypto_qs;
		inl_lf = dev->outb.lf_base + crypto_qid;

		txq->cpt_io_addr = inl_lf->io_addr;
		txq->cpt_fc = inl_lf->fc_addr;
		txq->cpt_fc_sw = (int32_t *)((uintptr_t)dev->outb.fc_sw_mem +
					     crypto_qid * RTE_CACHE_LINE_SIZE);

		txq->cpt_desc = inl_lf->nb_desc * 0.7;
		txq->sa_base = (uint64_t)dev->outb.sa_base;
		txq->sa_base |= (uint64_t)eth_dev->data->port_id;
		PLT_STATIC_ASSERT(ROC_NIX_INL_SA_BASE_ALIGN == BIT_ULL(16));
	}

	/* Restore marking flag from roc */
	mark_fmt = roc_nix_tm_mark_format_get(nix, &mark_flag);
	txq->mark_flag = mark_flag & CNXK_TM_MARK_MASK;
	txq->mark_fmt = mark_fmt & CNXK_TX_MARK_FMT_MASK;

	nix_form_default_desc(dev, txq, qid);
	txq->lso_tun_fmt = dev->lso_tun_fmt;
	return 0;
}

static int
cn10k_nix_rx_queue_setup(struct rte_eth_dev *eth_dev, uint16_t qid,
			 uint16_t nb_desc, unsigned int socket,
			 const struct rte_eth_rxconf *rx_conf,
			 struct rte_mempool *mp)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cn10k_eth_rxq *rxq;
	struct roc_nix_rq *rq;
	struct roc_nix_cq *cq;
	int rc;

	RTE_SET_USED(socket);

	/* CQ Errata needs min 4K ring */
	if (dev->cq_min_4k && nb_desc < 4096)
		nb_desc = 4096;

	/* Common Rx queue setup */
	rc = cnxk_nix_rx_queue_setup(eth_dev, qid, nb_desc,
				     sizeof(struct cn10k_eth_rxq), rx_conf, mp);
	if (rc)
		return rc;

	/* Do initial mtu setup for RQ0 before device start */
	if (!qid) {
		rc = nix_recalc_mtu(eth_dev);
		if (rc)
			return rc;

		/* Update offload flags */
		dev->rx_offload_flags = nix_rx_offload_flags(eth_dev);
		dev->tx_offload_flags = nix_tx_offload_flags(eth_dev);
	}

	rq = &dev->rqs[qid];
	cq = &dev->cqs[qid];

	/* Update fast path queue */
	rxq = eth_dev->data->rx_queues[qid];
	rxq->rq = qid;
	rxq->desc = (uintptr_t)cq->desc_base;
	rxq->cq_door = cq->door;
	rxq->cq_status = cq->status;
	rxq->wdata = cq->wdata;
	rxq->head = cq->head;
	rxq->qmask = cq->qmask;
	rxq->tstamp = &dev->tstamp;

	/* Data offset from data to start of mbuf is first_skip */
	rxq->data_off = rq->first_skip;
	rxq->mbuf_initializer = cnxk_nix_rxq_mbuf_setup(dev);

	/* Setup security related info */
	if (dev->rx_offload_flags & NIX_RX_OFFLOAD_SECURITY_F) {
		rxq->lmt_base = dev->nix.lmt_base;
		rxq->sa_base = roc_nix_inl_inb_sa_base_get(&dev->nix,
							   dev->inb.inl_dev);
	}

	/* Lookup mem */
	rxq->lookup_mem = cnxk_nix_fastpath_lookup_mem_get();
	return 0;
}

static void
cn10k_nix_rx_queue_meta_aura_update(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_eth_rxq_sp *rxq_sp;
	struct cn10k_eth_rxq *rxq;
	struct roc_nix_rq *rq;
	int i;

	/* Update Aura handle for fastpath rx queues */
	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		rq = &dev->rqs[i];
		rxq = eth_dev->data->rx_queues[i];
		rxq->meta_aura = rq->meta_aura_handle;
		rxq->meta_pool = dev->nix.meta_mempool;
		/* Assume meta packet from normal aura if meta aura is not setup
		 */
		if (!rxq->meta_aura) {
			rxq_sp = cnxk_eth_rxq_to_sp(rxq);
			rxq->meta_aura = rxq_sp->qconf.mp->pool_id;
			rxq->meta_pool = (uintptr_t)rxq_sp->qconf.mp;
		}
	}
	/* Store mempool in lookup mem */
	cnxk_nix_lookup_mem_metapool_set(dev);
}

static int
cn10k_nix_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t qidx)
{
	struct cn10k_eth_txq *txq = eth_dev->data->tx_queues[qidx];
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	uint16_t flags = dev->tx_offload_flags;
	struct roc_nix *nix = &dev->nix;
	uint32_t head = 0, tail = 0;
	int rc;

	rc = cnxk_nix_tx_queue_stop(eth_dev, qidx);
	if (rc)
		return rc;

	/* Clear fc cache pkts to trigger worker stop */
	txq->fc_cache_pkts = 0;

	if ((flags & NIX_TX_OFFLOAD_MBUF_NOFF_F) && txq->tx_compl.ena) {
		struct roc_nix_sq *sq = &dev->sqs[qidx];
		do {
			handle_tx_completion_pkts(txq, flags & NIX_TX_VWQE_F);
			/* Check if SQ is empty */
			roc_nix_sq_head_tail_get(nix, sq->qid, &head, &tail);
			if (head != tail)
				continue;

			/* Check if completion CQ is empty */
			roc_nix_cq_head_tail_get(nix, sq->cqid, &head, &tail);
		} while (head != tail);
	}

	return 0;
}

static int
cn10k_nix_configure(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	int rc;

	/* Common nix configure */
	rc = cnxk_nix_configure(eth_dev);
	if (rc)
		return rc;

	if (dev->tx_offloads & RTE_ETH_TX_OFFLOAD_SECURITY ||
	    dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		/* Register callback to handle security error work */
		roc_nix_inl_cb_register(cn10k_eth_sec_sso_work_cb, NULL);
	}

	/* Update offload flags */
	dev->rx_offload_flags = nix_rx_offload_flags(eth_dev);
	dev->tx_offload_flags = nix_tx_offload_flags(eth_dev);

	/* reset reassembly dynfield/flag offset */
	dev->reass_dynfield_off = -1;
	dev->reass_dynflag_bit = -1;

	plt_nix_dbg("Configured port%d platform specific rx_offload_flags=%x"
		    " tx_offload_flags=0x%x",
		    eth_dev->data->port_id, dev->rx_offload_flags,
		    dev->tx_offload_flags);
	return 0;
}

/* Function to enable ptp config for VFs */
static void
nix_ptp_enable_vf(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	if (nix_recalc_mtu(eth_dev))
		plt_err("Failed to set MTU size for ptp");

	dev->rx_offload_flags |= NIX_RX_OFFLOAD_TSTAMP_F;

	/* Setting up the function pointers as per new offload flags */
	cn10k_eth_set_rx_function(eth_dev);
	cn10k_eth_set_tx_function(eth_dev);
}

static uint16_t
nix_ptp_vf_burst(void *queue, struct rte_mbuf **mbufs, uint16_t pkts)
{
	struct cn10k_eth_rxq *rxq = queue;
	struct cnxk_eth_rxq_sp *rxq_sp;
	struct rte_eth_dev *eth_dev;

	RTE_SET_USED(mbufs);
	RTE_SET_USED(pkts);

	rxq_sp = cnxk_eth_rxq_to_sp(rxq);
	eth_dev = rxq_sp->dev->eth_dev;
	nix_ptp_enable_vf(eth_dev);

	return 0;
}

static int
cn10k_nix_ptp_info_update_cb(struct roc_nix *nix, bool ptp_en)
{
	struct cnxk_eth_dev *dev = (struct cnxk_eth_dev *)nix;
	struct rte_eth_dev *eth_dev;
	struct cn10k_eth_rxq *rxq;
	int i;

	if (!dev)
		return -EINVAL;

	eth_dev = dev->eth_dev;
	if (!eth_dev)
		return -EINVAL;

	dev->ptp_en = ptp_en;

	for (i = 0; i < eth_dev->data->nb_rx_queues; i++) {
		rxq = eth_dev->data->rx_queues[i];
		rxq->mbuf_initializer = cnxk_nix_rxq_mbuf_setup(dev);
	}

	if (roc_nix_is_vf_or_sdp(nix) && !(roc_nix_is_sdp(nix)) &&
	    !(roc_nix_is_lbk(nix))) {
		/* In case of VF, setting of MTU cannot be done directly in this
		 * function as this is running as part of MBOX request(PF->VF)
		 * and MTU setting also requires MBOX message to be
		 * sent(VF->PF)
		 */
		eth_dev->rx_pkt_burst = nix_ptp_vf_burst;
		rte_mb();
	}

	return 0;
}

static int
cn10k_nix_timesync_enable(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	int i, rc;

	rc = cnxk_nix_timesync_enable(eth_dev);
	if (rc)
		return rc;

	dev->rx_offload_flags |= NIX_RX_OFFLOAD_TSTAMP_F;
	dev->tx_offload_flags |= NIX_TX_OFFLOAD_TSTAMP_F;

	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		nix_form_default_desc(dev, eth_dev->data->tx_queues[i], i);

	/* Setting up the rx[tx]_offload_flags due to change
	 * in rx[tx]_offloads.
	 */
	cn10k_eth_set_rx_function(eth_dev);
	cn10k_eth_set_tx_function(eth_dev);
	return 0;
}

static int
cn10k_nix_timesync_disable(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	int i, rc;

	rc = cnxk_nix_timesync_disable(eth_dev);
	if (rc)
		return rc;

	dev->rx_offload_flags &= ~NIX_RX_OFFLOAD_TSTAMP_F;
	dev->tx_offload_flags &= ~NIX_TX_OFFLOAD_TSTAMP_F;

	for (i = 0; i < eth_dev->data->nb_tx_queues; i++)
		nix_form_default_desc(dev, eth_dev->data->tx_queues[i], i);

	/* Setting up the rx[tx]_offload_flags due to change
	 * in rx[tx]_offloads.
	 */
	cn10k_eth_set_rx_function(eth_dev);
	cn10k_eth_set_tx_function(eth_dev);
	return 0;
}

static int
cn10k_nix_timesync_read_tx_timestamp(struct rte_eth_dev *eth_dev,
				     struct timespec *timestamp)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct cnxk_timesync_info *tstamp = &dev->tstamp;
	uint64_t ns;

	if (*tstamp->tx_tstamp == 0)
		return -EINVAL;

	*tstamp->tx_tstamp = ((*tstamp->tx_tstamp >> 32) * NSEC_PER_SEC) +
		(*tstamp->tx_tstamp & 0xFFFFFFFFUL);
	ns = rte_timecounter_update(&dev->tx_tstamp_tc, *tstamp->tx_tstamp);
	*timestamp = rte_ns_to_timespec(ns);
	*tstamp->tx_tstamp = 0;
	rte_wmb();

	return 0;
}

static int
cn10k_nix_dev_start(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *nix = &dev->nix;
	int rc;

	/* Common eth dev start */
	rc = cnxk_nix_dev_start(eth_dev);
	if (rc)
		return rc;

	/* Update VF about data off shifted by 8 bytes if PTP already
	 * enabled in PF owning this VF
	 */
	if (dev->ptp_en && (!roc_nix_is_pf(nix) && (!roc_nix_is_sdp(nix))))
		nix_ptp_enable_vf(eth_dev);

	/* Setting up the rx[tx]_offload_flags due to change
	 * in rx[tx]_offloads.
	 */
	dev->rx_offload_flags |= nix_rx_offload_flags(eth_dev);
	dev->tx_offload_flags |= nix_tx_offload_flags(eth_dev);

	if (dev->rx_offload_flags & NIX_RX_OFFLOAD_SECURITY_F)
		cn10k_nix_rx_queue_meta_aura_update(eth_dev);

	cn10k_eth_set_tx_function(eth_dev);
	cn10k_eth_set_rx_function(eth_dev);
	return 0;
}

static int
cn10k_nix_reassembly_capability_get(struct rte_eth_dev *eth_dev,
		struct rte_eth_ip_reassembly_params *reassembly_capa)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	int rc = -ENOTSUP;
	RTE_SET_USED(eth_dev);

	if (!roc_feature_nix_has_reass())
		return -ENOTSUP;

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		reassembly_capa->timeout_ms = 60 * 1000;
		reassembly_capa->max_frags = 4;
		reassembly_capa->flags = RTE_ETH_DEV_REASSEMBLY_F_IPV4 |
					 RTE_ETH_DEV_REASSEMBLY_F_IPV6;
		rc = 0;
	}

	return rc;
}

static int
cn10k_nix_reassembly_conf_get(struct rte_eth_dev *eth_dev,
		struct rte_eth_ip_reassembly_params *conf)
{
	RTE_SET_USED(eth_dev);
	RTE_SET_USED(conf);
	return -ENOTSUP;
}

static int
cn10k_nix_reassembly_conf_set(struct rte_eth_dev *eth_dev,
		const struct rte_eth_ip_reassembly_params *conf)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	int rc = 0;

	if (!roc_feature_nix_has_reass())
		return -ENOTSUP;

	if (!conf->flags) {
		/* Clear offload flags on disable */
		if (!dev->inb.nb_oop)
			dev->rx_offload_flags &= ~NIX_RX_REAS_F;
		dev->inb.reass_en = false;
		return 0;
	}

	rc = roc_nix_reassembly_configure(conf->timeout_ms, conf->max_frags);
	if (!rc && dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		dev->rx_offload_flags |= NIX_RX_REAS_F;
		dev->inb.reass_en = true;
	}

	return rc;
}

static int
cn10k_nix_tm_mark_vlan_dei(struct rte_eth_dev *eth_dev, int mark_green,
			   int mark_yellow, int mark_red,
			   struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *roc_nix = &dev->nix;
	uint64_t mark_fmt, mark_flag;
	int rc, i;

	rc = cnxk_nix_tm_mark_vlan_dei(eth_dev, mark_green, mark_yellow,
				       mark_red, error);

	if (rc)
		goto exit;

	mark_fmt = roc_nix_tm_mark_format_get(roc_nix, &mark_flag);
	if (mark_flag) {
		dev->tx_offload_flags |= NIX_TX_OFFLOAD_VLAN_QINQ_F;
		dev->tx_mark = true;
	} else {
		dev->tx_mark = false;
		if (!(dev->tx_offloads & RTE_ETH_TX_OFFLOAD_VLAN_INSERT ||
		      dev->tx_offloads & RTE_ETH_TX_OFFLOAD_QINQ_INSERT))
			dev->tx_offload_flags &= ~NIX_TX_OFFLOAD_VLAN_QINQ_F;
	}

	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		struct cn10k_eth_txq *txq = eth_dev->data->tx_queues[i];

		txq->mark_flag = mark_flag & CNXK_TM_MARK_MASK;
		txq->mark_fmt = mark_fmt & CNXK_TX_MARK_FMT_MASK;
	}
	cn10k_eth_set_tx_function(eth_dev);
exit:
	return rc;
}

static int
cn10k_nix_tm_mark_ip_ecn(struct rte_eth_dev *eth_dev, int mark_green,
			 int mark_yellow, int mark_red,
			 struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *roc_nix = &dev->nix;
	uint64_t mark_fmt, mark_flag;
	int rc, i;

	rc = cnxk_nix_tm_mark_ip_ecn(eth_dev, mark_green, mark_yellow, mark_red,
				     error);
	if (rc)
		goto exit;

	mark_fmt = roc_nix_tm_mark_format_get(roc_nix, &mark_flag);
	if (mark_flag) {
		dev->tx_offload_flags |= NIX_TX_OFFLOAD_VLAN_QINQ_F;
		dev->tx_mark = true;
	} else {
		dev->tx_mark = false;
		if (!(dev->tx_offloads & RTE_ETH_TX_OFFLOAD_VLAN_INSERT ||
		      dev->tx_offloads & RTE_ETH_TX_OFFLOAD_QINQ_INSERT))
			dev->tx_offload_flags &= ~NIX_TX_OFFLOAD_VLAN_QINQ_F;
	}

	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		struct cn10k_eth_txq *txq = eth_dev->data->tx_queues[i];

		txq->mark_flag = mark_flag & CNXK_TM_MARK_MASK;
		txq->mark_fmt = mark_fmt & CNXK_TX_MARK_FMT_MASK;
	}
	cn10k_eth_set_tx_function(eth_dev);
exit:
	return rc;
}

static int
cn10k_nix_tm_mark_ip_dscp(struct rte_eth_dev *eth_dev, int mark_green,
			  int mark_yellow, int mark_red,
			  struct rte_tm_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_nix *roc_nix = &dev->nix;
	uint64_t mark_fmt, mark_flag;
	int rc, i;

	rc = cnxk_nix_tm_mark_ip_dscp(eth_dev, mark_green, mark_yellow,
				      mark_red, error);
	if (rc)
		goto exit;

	mark_fmt = roc_nix_tm_mark_format_get(roc_nix, &mark_flag);
	if (mark_flag) {
		dev->tx_offload_flags |= NIX_TX_OFFLOAD_VLAN_QINQ_F;
		dev->tx_mark = true;
	} else {
		dev->tx_mark = false;
		if (!(dev->tx_offloads & RTE_ETH_TX_OFFLOAD_VLAN_INSERT ||
		      dev->tx_offloads & RTE_ETH_TX_OFFLOAD_QINQ_INSERT))
			dev->tx_offload_flags &= ~NIX_TX_OFFLOAD_VLAN_QINQ_F;
	}

	for (i = 0; i < eth_dev->data->nb_tx_queues; i++) {
		struct cn10k_eth_txq *txq = eth_dev->data->tx_queues[i];

		txq->mark_flag = mark_flag & CNXK_TM_MARK_MASK;
		txq->mark_fmt = mark_fmt & CNXK_TX_MARK_FMT_MASK;
	}
	cn10k_eth_set_tx_function(eth_dev);
exit:
	return rc;
}

/* Update platform specific eth dev ops */
static void
nix_eth_dev_ops_override(void)
{
	static int init_once;

	if (init_once)
		return;
	init_once = 1;

	/* Update platform specific ops */
	cnxk_eth_dev_ops.dev_configure = cn10k_nix_configure;
	cnxk_eth_dev_ops.tx_queue_setup = cn10k_nix_tx_queue_setup;
	cnxk_eth_dev_ops.rx_queue_setup = cn10k_nix_rx_queue_setup;
	cnxk_eth_dev_ops.tx_queue_release = cn10k_nix_tx_queue_release;
	cnxk_eth_dev_ops.tx_queue_stop = cn10k_nix_tx_queue_stop;
	cnxk_eth_dev_ops.dev_start = cn10k_nix_dev_start;
	cnxk_eth_dev_ops.dev_ptypes_set = cn10k_nix_ptypes_set;
	cnxk_eth_dev_ops.timesync_enable = cn10k_nix_timesync_enable;
	cnxk_eth_dev_ops.timesync_disable = cn10k_nix_timesync_disable;
	cnxk_eth_dev_ops.timesync_read_tx_timestamp =
		cn10k_nix_timesync_read_tx_timestamp;
	cnxk_eth_dev_ops.ip_reassembly_capability_get =
			cn10k_nix_reassembly_capability_get;
	cnxk_eth_dev_ops.ip_reassembly_conf_get = cn10k_nix_reassembly_conf_get;
	cnxk_eth_dev_ops.ip_reassembly_conf_set = cn10k_nix_reassembly_conf_set;
}

/* Update platform specific tm ops */
static void
nix_tm_ops_override(void)
{
	static int init_once;

	if (init_once)
		return;
	init_once = 1;

	/* Update platform specific ops */
	cnxk_tm_ops.mark_vlan_dei = cn10k_nix_tm_mark_vlan_dei;
	cnxk_tm_ops.mark_ip_ecn = cn10k_nix_tm_mark_ip_ecn;
	cnxk_tm_ops.mark_ip_dscp = cn10k_nix_tm_mark_ip_dscp;
}

static void
npc_flow_ops_override(void)
{
	static int init_once;

	if (init_once)
		return;
	init_once = 1;

	/* Update platform specific ops */
	cnxk_flow_ops.create = cn10k_flow_create;
	cnxk_flow_ops.destroy = cn10k_flow_destroy;
	cnxk_flow_ops.info_get = cn10k_flow_info_get;
}

static int
cn10k_nix_remove(struct rte_pci_device *pci_dev)
{
	return cnxk_nix_remove(pci_dev);
}

static int
cn10k_nix_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	struct rte_eth_dev *eth_dev;
	struct cnxk_eth_dev *dev;
	int rc;

	rc = roc_plt_init();
	if (rc) {
		plt_err("Failed to initialize platform model, rc=%d", rc);
		return rc;
	}

	nix_eth_dev_ops_override();
	nix_tm_ops_override();
	npc_flow_ops_override();

	cn10k_eth_sec_ops_override();

	/* Common probe */
	rc = cnxk_nix_probe(pci_drv, pci_dev);
	if (rc)
		return rc;

	/* Find eth dev allocated */
	eth_dev = rte_eth_dev_allocated(pci_dev->device.name);
	if (!eth_dev) {
		/* Ignore if ethdev is in mid of detach state in secondary */
		if (rte_eal_process_type() != RTE_PROC_PRIMARY)
			return 0;
		return -ENOENT;
	}

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		/* Setup callbacks for secondary process */
		cn10k_eth_set_tx_function(eth_dev);
		cn10k_eth_set_rx_function(eth_dev);
		return 0;
	}

	dev = cnxk_eth_pmd_priv(eth_dev);

	/* DROP_RE is not supported with inline IPSec for CN10K A0 and
	 * when vector mode is enabled.
	 */
	if (roc_errata_nix_has_no_drop_re() && !roc_env_is_asim()) {
		dev->ipsecd_drop_re_dis = 1;
		dev->vec_drop_re_dis = 1;
	}

	/* Register up msg callbacks for PTP information */
	roc_nix_ptp_info_cb_register(&dev->nix, cn10k_nix_ptp_info_update_cb);

	return 0;
}

static const struct rte_pci_id cn10k_pci_nix_map[] = {
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF10KA, PCI_DEVID_CNXK_RVU_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KB, PCI_DEVID_CNXK_RVU_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF10KB, PCI_DEVID_CNXK_RVU_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF10KA, PCI_DEVID_CNXK_RVU_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KB, PCI_DEVID_CNXK_RVU_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF10KB, PCI_DEVID_CNXK_RVU_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_AF_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_AF_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF10KA, PCI_DEVID_CNXK_RVU_AF_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KB, PCI_DEVID_CNXK_RVU_AF_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF10KB, PCI_DEVID_CNXK_RVU_AF_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_SDP_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_SDP_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF10KA, PCI_DEVID_CNXK_RVU_SDP_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KB, PCI_DEVID_CNXK_RVU_SDP_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CNF10KB, PCI_DEVID_CNXK_RVU_SDP_VF),
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cn10k_pci_nix = {
	.id_table = cn10k_pci_nix_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA |
		     RTE_PCI_DRV_INTR_LSC,
	.probe = cn10k_nix_probe,
	.remove = cn10k_nix_remove,
};

RTE_PMD_REGISTER_PCI(net_cn10k, cn10k_pci_nix);
RTE_PMD_REGISTER_PCI_TABLE(net_cn10k, cn10k_pci_nix_map);
RTE_PMD_REGISTER_KMOD_DEP(net_cn10k, "vfio-pci");
