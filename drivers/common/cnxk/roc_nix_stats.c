/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <inttypes.h>

#include "roc_api.h"
#include "roc_priv.h"

#define NIX_RX_STATS(val) plt_read64(nix->base + NIX_LF_RX_STATX(val))
#define NIX_TX_STATS(val) plt_read64(nix->base + NIX_LF_TX_STATX(val))

int
roc_nix_stats_get(struct roc_nix *roc_nix, struct roc_nix_stats *stats)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	if (stats == NULL)
		return NIX_ERR_PARAM;

	stats->rx_octs = NIX_RX_STATS(NIX_STAT_LF_RX_RX_OCTS);
	stats->rx_ucast = NIX_RX_STATS(NIX_STAT_LF_RX_RX_UCAST);
	stats->rx_bcast = NIX_RX_STATS(NIX_STAT_LF_RX_RX_BCAST);
	stats->rx_mcast = NIX_RX_STATS(NIX_STAT_LF_RX_RX_MCAST);
	stats->rx_drop = NIX_RX_STATS(NIX_STAT_LF_RX_RX_DROP);
	stats->rx_drop_octs = NIX_RX_STATS(NIX_STAT_LF_RX_RX_DROP_OCTS);
	stats->rx_fcs = NIX_RX_STATS(NIX_STAT_LF_RX_RX_FCS);
	stats->rx_err = NIX_RX_STATS(NIX_STAT_LF_RX_RX_ERR);
	stats->rx_drop_bcast = NIX_RX_STATS(NIX_STAT_LF_RX_RX_DRP_BCAST);
	stats->rx_drop_mcast = NIX_RX_STATS(NIX_STAT_LF_RX_RX_DRP_MCAST);
	stats->rx_drop_l3_bcast = NIX_RX_STATS(NIX_STAT_LF_RX_RX_DRP_L3BCAST);
	stats->rx_drop_l3_mcast = NIX_RX_STATS(NIX_STAT_LF_RX_RX_DRP_L3MCAST);

	stats->tx_ucast = NIX_TX_STATS(NIX_STAT_LF_TX_TX_UCAST);
	stats->tx_bcast = NIX_TX_STATS(NIX_STAT_LF_TX_TX_BCAST);
	stats->tx_mcast = NIX_TX_STATS(NIX_STAT_LF_TX_TX_MCAST);
	stats->tx_drop = NIX_TX_STATS(NIX_STAT_LF_TX_TX_DROP);
	stats->tx_octs = NIX_TX_STATS(NIX_STAT_LF_TX_TX_OCTS);
	return 0;
}

int
roc_nix_stats_reset(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct mbox *mbox = (&nix->dev)->mbox;

	if (mbox_alloc_msg_nix_stats_rst(mbox) == NULL)
		return -ENOMEM;

	return mbox_process(mbox);
}

static int
queue_is_valid(struct nix *nix, uint16_t qid, bool is_rx)
{
	uint16_t nb_queues;

	if (is_rx)
		nb_queues = nix->nb_rx_queues;
	else
		nb_queues = nix->nb_tx_queues;

	if (qid >= nb_queues)
		return NIX_ERR_QUEUE_INVALID_RANGE;

	return 0;
}

static uint64_t
qstat_read(struct nix *nix, uint16_t qid, uint32_t off)
{
	uint64_t reg, val;
	int64_t *addr;

	addr = (int64_t *)(nix->base + off);
	reg = (((uint64_t)qid) << 32);
	val = roc_atomic64_add_nosync(reg, addr);
	if (val & BIT_ULL(NIX_CQ_OP_STAT_OP_ERR))
		val = 0;
	return val;
}

static void
nix_stat_rx_queue_get(struct nix *nix, uint16_t qid,
		      struct roc_nix_stats_queue *qstats)
{
	qstats->rx_pkts = qstat_read(nix, qid, NIX_LF_RQ_OP_PKTS);
	qstats->rx_octs = qstat_read(nix, qid, NIX_LF_RQ_OP_OCTS);
	qstats->rx_drop_pkts = qstat_read(nix, qid, NIX_LF_RQ_OP_DROP_PKTS);
	qstats->rx_drop_octs = qstat_read(nix, qid, NIX_LF_RQ_OP_DROP_OCTS);
	qstats->rx_error_pkts = qstat_read(nix, qid, NIX_LF_RQ_OP_RE_PKTS);
}

static void
nix_stat_tx_queue_get(struct nix *nix, uint16_t qid,
		      struct roc_nix_stats_queue *qstats)
{
	qstats->tx_pkts = qstat_read(nix, qid, NIX_LF_SQ_OP_PKTS);
	qstats->tx_octs = qstat_read(nix, qid, NIX_LF_SQ_OP_OCTS);
	qstats->tx_drop_pkts = qstat_read(nix, qid, NIX_LF_SQ_OP_DROP_PKTS);
	qstats->tx_drop_octs = qstat_read(nix, qid, NIX_LF_SQ_OP_DROP_OCTS);
}

static int
nix_stat_rx_queue_reset(struct nix *nix, uint16_t qid)
{
	struct mbox *mbox = (&nix->dev)->mbox;
	int rc;

	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox);
		aq->qidx = qid;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_WRITE;

		aq->rq.octs = 0;
		aq->rq.pkts = 0;
		aq->rq.drop_octs = 0;
		aq->rq.drop_pkts = 0;
		aq->rq.re_pkts = 0;

		aq->rq_mask.octs = ~(aq->rq_mask.octs);
		aq->rq_mask.pkts = ~(aq->rq_mask.pkts);
		aq->rq_mask.drop_octs = ~(aq->rq_mask.drop_octs);
		aq->rq_mask.drop_pkts = ~(aq->rq_mask.drop_pkts);
		aq->rq_mask.re_pkts = ~(aq->rq_mask.re_pkts);
	} else {
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		aq->qidx = qid;
		aq->ctype = NIX_AQ_CTYPE_RQ;
		aq->op = NIX_AQ_INSTOP_WRITE;

		aq->rq.octs = 0;
		aq->rq.pkts = 0;
		aq->rq.drop_octs = 0;
		aq->rq.drop_pkts = 0;
		aq->rq.re_pkts = 0;

		aq->rq_mask.octs = ~(aq->rq_mask.octs);
		aq->rq_mask.pkts = ~(aq->rq_mask.pkts);
		aq->rq_mask.drop_octs = ~(aq->rq_mask.drop_octs);
		aq->rq_mask.drop_pkts = ~(aq->rq_mask.drop_pkts);
		aq->rq_mask.re_pkts = ~(aq->rq_mask.re_pkts);
	}

	rc = mbox_process(mbox);
	return rc ? NIX_ERR_AQ_WRITE_FAILED : 0;
}

static int
nix_stat_tx_queue_reset(struct nix *nix, uint16_t qid)
{
	struct mbox *mbox = (&nix->dev)->mbox;
	int rc;

	if (roc_model_is_cn9k()) {
		struct nix_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_aq_enq(mbox);
		aq->qidx = qid;
		aq->ctype = NIX_AQ_CTYPE_SQ;
		aq->op = NIX_AQ_INSTOP_WRITE;
		aq->sq.octs = 0;
		aq->sq.pkts = 0;
		aq->sq.drop_octs = 0;
		aq->sq.drop_pkts = 0;

		aq->sq_mask.octs = ~(aq->sq_mask.octs);
		aq->sq_mask.pkts = ~(aq->sq_mask.pkts);
		aq->sq_mask.drop_octs = ~(aq->sq_mask.drop_octs);
		aq->sq_mask.drop_pkts = ~(aq->sq_mask.drop_pkts);
	} else {
		struct nix_cn10k_aq_enq_req *aq;

		aq = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		aq->qidx = qid;
		aq->ctype = NIX_AQ_CTYPE_SQ;
		aq->op = NIX_AQ_INSTOP_WRITE;
		aq->sq.octs = 0;
		aq->sq.pkts = 0;
		aq->sq.drop_octs = 0;
		aq->sq.drop_pkts = 0;

		aq->sq_mask.octs = ~(aq->sq_mask.octs);
		aq->sq_mask.pkts = ~(aq->sq_mask.pkts);
		aq->sq_mask.drop_octs = ~(aq->sq_mask.drop_octs);
		aq->sq_mask.drop_pkts = ~(aq->sq_mask.drop_pkts);
	}

	rc = mbox_process(mbox);
	return rc ? NIX_ERR_AQ_WRITE_FAILED : 0;
}

int
roc_nix_stats_queue_get(struct roc_nix *roc_nix, uint16_t qid, bool is_rx,
			struct roc_nix_stats_queue *qstats)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	int rc;

	if (qstats == NULL)
		return NIX_ERR_PARAM;

	rc = queue_is_valid(nix, qid, is_rx);
	if (rc)
		goto fail;

	if (is_rx)
		nix_stat_rx_queue_get(nix, qid, qstats);
	else
		nix_stat_tx_queue_get(nix, qid, qstats);

fail:
	return rc;
}

int
roc_nix_stats_queue_reset(struct roc_nix *roc_nix, uint16_t qid, bool is_rx)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	int rc;

	rc = queue_is_valid(nix, qid, is_rx);
	if (rc)
		goto fail;

	if (is_rx)
		rc = nix_stat_rx_queue_reset(nix, qid);
	else
		rc = nix_stat_tx_queue_reset(nix, qid);

fail:
	return rc;
}
