/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <ctype.h>
#include "cnxk_telemetry.h"
#include "roc_api.h"
#include "roc_priv.h"

struct nix_tel_node {
	TAILQ_ENTRY(nix_tel_node) node;
	struct roc_nix *nix;
	uint16_t n_rq;
	uint16_t n_cq;
	uint16_t n_sq;
	struct roc_nix_rq **rqs;
	struct roc_nix_cq **cqs;
	struct roc_nix_sq **sqs;
};

TAILQ_HEAD(nix_tel_node_list, nix_tel_node);
static struct nix_tel_node_list nix_list;

static struct nix_tel_node *
nix_tel_node_get(struct roc_nix *roc_nix)
{
	struct nix_tel_node *node, *roc_node = NULL;

	TAILQ_FOREACH(node, &nix_list, node) {
		if (node->nix == roc_nix) {
			roc_node = node;
			break;
		}
	}

	return roc_node;
}

int
nix_tel_node_add(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_tel_node *node;

	node = nix_tel_node_get(roc_nix);
	if (node) {
		if (nix->nb_rx_queues == node->n_rq &&
		    nix->nb_tx_queues == node->n_sq)
			return 0;

		nix_tel_node_del(roc_nix);
	}

	node = plt_zmalloc(sizeof(struct nix_tel_node), 0);
	if (!node)
		return -1;

	node->nix = roc_nix;
	node->rqs =
		plt_zmalloc(nix->nb_rx_queues * sizeof(struct roc_nix_rq *), 0);
	node->cqs =
		plt_zmalloc(nix->nb_rx_queues * sizeof(struct roc_nix_cq *), 0);
	node->sqs =
		plt_zmalloc(nix->nb_tx_queues * sizeof(struct roc_nix_sq *), 0);
	TAILQ_INSERT_TAIL(&nix_list, node, node);

	return 0;
}

void
nix_tel_node_del(struct roc_nix *roc_nix)
{
	struct nix_tel_node *node;

	TAILQ_FOREACH(node, &nix_list, node) {
		if (node->nix == roc_nix) {
			plt_free(node->rqs);
			plt_free(node->cqs);
			plt_free(node->sqs);
			TAILQ_REMOVE(&nix_list, node, node);
		}
	}

	plt_free(node);
}

static struct nix_tel_node *
nix_tel_node_get_by_pcidev_name(const char *name)
{
	struct nix_tel_node *node, *roc_node = NULL;

	TAILQ_FOREACH(node, &nix_list, node) {
		if (!strncmp(node->nix->pci_dev->name, name,
			     PCI_PRI_STR_SIZE)) {
			roc_node = node;
			break;
		}
	}

	return roc_node;
}

int
nix_tel_node_add_rq(struct roc_nix_rq *rq)
{
	struct nix_tel_node *node;

	node = nix_tel_node_get(rq->roc_nix);
	if (!node)
		return -1;

	node->rqs[rq->qid] = rq;
	node->n_rq++;
	return 0;
}

int
nix_tel_node_add_cq(struct roc_nix_cq *cq)
{
	struct nix_tel_node *node;

	node = nix_tel_node_get(cq->roc_nix);
	if (!node)
		return -1;

	node->cqs[cq->qid] = cq;
	node->n_cq++;
	return 0;
}

int
nix_tel_node_add_sq(struct roc_nix_sq *sq)
{
	struct nix_tel_node *node;

	node = nix_tel_node_get(sq->roc_nix);
	if (!node)
		return -1;

	node->sqs[sq->qid] = sq;
	node->n_sq++;
	return 0;
}

static int
cnxk_tel_nix(struct roc_nix *roc_nix, struct plt_tel_data *d)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	struct dev *dev = &nix->dev;

	plt_tel_data_add_dict_ptr(d, "nix", nix);
	plt_tel_data_add_dict_int(d, "pf_func", dev->pf_func);
	plt_tel_data_add_dict_int(d, "pf", dev_get_pf(dev->pf_func));
	plt_tel_data_add_dict_int(d, "vf", dev_get_vf(dev->pf_func));

	CNXK_TEL_DICT_PTR(d, dev, bar2);
	CNXK_TEL_DICT_PTR(d, dev, bar4);
	CNXK_TEL_DICT_INT(d, roc_nix, port_id);
	CNXK_TEL_DICT_INT(d, roc_nix, rss_tag_as_xor);
	CNXK_TEL_DICT_INT(d, roc_nix, max_sqb_count);
	CNXK_TEL_DICT_PTR(d, nix, pci_dev);
	CNXK_TEL_DICT_PTR(d, nix, base);
	CNXK_TEL_DICT_PTR(d, nix, lmt_base);
	CNXK_TEL_DICT_INT(d, nix, reta_sz);
	CNXK_TEL_DICT_INT(d, nix, tx_chan_base);
	CNXK_TEL_DICT_INT(d, nix, rx_chan_base);
	CNXK_TEL_DICT_INT(d, nix, nb_tx_queues);
	CNXK_TEL_DICT_INT(d, nix, nb_rx_queues);
	CNXK_TEL_DICT_INT(d, nix, lso_tsov6_idx);
	CNXK_TEL_DICT_INT(d, nix, lso_tsov4_idx);

	plt_tel_data_add_dict_int(d, "lso_udp_tun_v4v4",
				  nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V4V4]);
	plt_tel_data_add_dict_int(d, "lso_udp_tun_v4v6",
				  nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V4V6]);
	plt_tel_data_add_dict_int(d, "lso_udp_tun_v6v4",
				  nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V6V4]);
	plt_tel_data_add_dict_int(d, "lso_udp_tun_v6v6",
				  nix->lso_udp_tun_idx[ROC_NIX_LSO_TUN_V6V6]);
	plt_tel_data_add_dict_int(d, "lso_tun_v4v4",
				  nix->lso_tun_idx[ROC_NIX_LSO_TUN_V4V4]);
	plt_tel_data_add_dict_int(d, "lso_tun_v4v6",
				  nix->lso_tun_idx[ROC_NIX_LSO_TUN_V4V6]);
	plt_tel_data_add_dict_int(d, "lso_tun_v6v4",
				  nix->lso_tun_idx[ROC_NIX_LSO_TUN_V6V4]);
	plt_tel_data_add_dict_int(d, "lso_tun_v6v6",
				  nix->lso_tun_idx[ROC_NIX_LSO_TUN_V6V6]);

	CNXK_TEL_DICT_INT(d, nix, lf_tx_stats);
	CNXK_TEL_DICT_INT(d, nix, lf_rx_stats);
	CNXK_TEL_DICT_INT(d, nix, cgx_links);
	CNXK_TEL_DICT_INT(d, nix, lbk_links);
	CNXK_TEL_DICT_INT(d, nix, sdp_links);
	CNXK_TEL_DICT_INT(d, nix, tx_link);
	CNXK_TEL_DICT_INT(d, nix, sqb_size);
	CNXK_TEL_DICT_INT(d, nix, msixoff);
	CNXK_TEL_DICT_INT(d, nix, cints);
	CNXK_TEL_DICT_INT(d, nix, qints);
	CNXK_TEL_DICT_INT(d, nix, sdp_link);
	CNXK_TEL_DICT_INT(d, nix, ptp_en);
	CNXK_TEL_DICT_INT(d, nix, rss_alg_idx);
	CNXK_TEL_DICT_INT(d, nix, tx_pause);

	return 0;
}

static int
cnxk_tel_nix_rq(struct roc_nix_rq *rq, struct plt_tel_data *d)
{
	plt_tel_data_add_dict_ptr(d, "nix_rq", rq);
	CNXK_TEL_DICT_INT(d, rq, qid);
	CNXK_TEL_DICT_PTR(d, rq, aura_handle);
	CNXK_TEL_DICT_INT(d, rq, ipsech_ena);
	CNXK_TEL_DICT_INT(d, rq, first_skip);
	CNXK_TEL_DICT_INT(d, rq, later_skip);
	CNXK_TEL_DICT_INT(d, rq, lpb_size);
	CNXK_TEL_DICT_INT(d, rq, sso_ena);
	CNXK_TEL_DICT_INT(d, rq, tag_mask);
	CNXK_TEL_DICT_INT(d, rq, flow_tag_width);
	CNXK_TEL_DICT_INT(d, rq, tt);
	CNXK_TEL_DICT_INT(d, rq, hwgrp);
	CNXK_TEL_DICT_INT(d, rq, vwqe_ena);
	CNXK_TEL_DICT_INT(d, rq, vwqe_first_skip);
	CNXK_TEL_DICT_INT(d, rq, vwqe_max_sz_exp);
	CNXK_TEL_DICT_INT(d, rq, vwqe_wait_tmo);
	CNXK_TEL_DICT_INT(d, rq, vwqe_aura_handle);
	CNXK_TEL_DICT_PTR(d, rq, roc_nix);

	return 0;
}

static int
cnxk_tel_nix_cq(struct roc_nix_cq *cq, struct plt_tel_data *d)
{
	plt_tel_data_add_dict_ptr(d, "nix_cq", cq);
	CNXK_TEL_DICT_INT(d, cq, qid);
	CNXK_TEL_DICT_INT(d, cq, nb_desc);
	CNXK_TEL_DICT_PTR(d, cq, roc_nix);
	CNXK_TEL_DICT_PTR(d, cq, door);
	CNXK_TEL_DICT_PTR(d, cq, status);
	CNXK_TEL_DICT_PTR(d, cq, wdata);
	CNXK_TEL_DICT_PTR(d, cq, desc_base);
	CNXK_TEL_DICT_INT(d, cq, qmask);

	return 0;
}

static int
cnxk_tel_nix_sq(struct roc_nix_sq *sq, struct plt_tel_data *d)
{
	plt_tel_data_add_dict_ptr(d, "nix_sq", sq);
	CNXK_TEL_DICT_INT(d, sq, qid);
	CNXK_TEL_DICT_INT(d, sq, max_sqe_sz);
	CNXK_TEL_DICT_INT(d, sq, nb_desc);
	CNXK_TEL_DICT_INT(d, sq, sqes_per_sqb_log2);
	CNXK_TEL_DICT_PTR(d, sq, roc_nix);
	CNXK_TEL_DICT_PTR(d, sq, aura_handle);
	CNXK_TEL_DICT_INT(d, sq, nb_sqb_bufs_adj);
	CNXK_TEL_DICT_INT(d, sq, nb_sqb_bufs);
	CNXK_TEL_DICT_PTR(d, sq, io_addr);
	CNXK_TEL_DICT_PTR(d, sq, lmt_addr);
	CNXK_TEL_DICT_PTR(d, sq, sqe_mem);
	CNXK_TEL_DICT_PTR(d, sq, fc);

	return 0;
}

static void
nix_rq_ctx_cn9k(volatile void *qctx, struct plt_tel_data *d)
{
	volatile struct nix_rq_ctx_s *ctx;

	ctx = (volatile struct nix_rq_ctx_s *)qctx;

	/* W0 */
	CNXK_TEL_DICT_INT(d, ctx, wqe_aura, w0_);
	CNXK_TEL_DICT_BF_PTR(d, ctx, substream, w0_);
	CNXK_TEL_DICT_INT(d, ctx, cq, w0_);
	CNXK_TEL_DICT_INT(d, ctx, ena_wqwd, w0_);
	CNXK_TEL_DICT_INT(d, ctx, ipsech_ena, w0_);
	CNXK_TEL_DICT_INT(d, ctx, sso_ena, w0_);
	CNXK_TEL_DICT_INT(d, ctx, ena, w0_);

	/* W1 */
	CNXK_TEL_DICT_INT(d, ctx, lpb_drop_ena, w1_);
	CNXK_TEL_DICT_INT(d, ctx, spb_drop_ena, w1_);
	CNXK_TEL_DICT_INT(d, ctx, xqe_drop_ena, w1_);
	CNXK_TEL_DICT_INT(d, ctx, wqe_caching, w1_);
	CNXK_TEL_DICT_INT(d, ctx, pb_caching, w1_);
	CNXK_TEL_DICT_INT(d, ctx, sso_tt, w1_);
	CNXK_TEL_DICT_INT(d, ctx, sso_grp, w1_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_aura, w1_);
	CNXK_TEL_DICT_INT(d, ctx, spb_aura, w1_);

	/* W2 */
	CNXK_TEL_DICT_INT(d, ctx, xqe_hdr_split, w2_);
	CNXK_TEL_DICT_INT(d, ctx, xqe_imm_copy, w2_);
	CNXK_TEL_DICT_INT(d, ctx, xqe_imm_size, w2_);
	CNXK_TEL_DICT_INT(d, ctx, later_skip, w2_);
	CNXK_TEL_DICT_INT(d, ctx, first_skip, w2_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_sizem1, w2_);
	CNXK_TEL_DICT_INT(d, ctx, spb_ena, w2_);
	CNXK_TEL_DICT_INT(d, ctx, wqe_skip, w2_);
	CNXK_TEL_DICT_INT(d, ctx, spb_sizem1, w2_);

	/* W3 */
	CNXK_TEL_DICT_INT(d, ctx, spb_pool_pass, w3_);
	CNXK_TEL_DICT_INT(d, ctx, spb_pool_drop, w3_);
	CNXK_TEL_DICT_INT(d, ctx, spb_aura_pass, w3_);
	CNXK_TEL_DICT_INT(d, ctx, spb_aura_drop, w3_);
	CNXK_TEL_DICT_INT(d, ctx, wqe_pool_pass, w3_);
	CNXK_TEL_DICT_INT(d, ctx, wqe_pool_drop, w3_);
	CNXK_TEL_DICT_INT(d, ctx, xqe_pass, w3_);
	CNXK_TEL_DICT_INT(d, ctx, xqe_drop, w3_);

	/* W4 */
	CNXK_TEL_DICT_INT(d, ctx, qint_idx, w4_);
	CNXK_TEL_DICT_INT(d, ctx, rq_int_ena, w4_);
	CNXK_TEL_DICT_INT(d, ctx, rq_int, w4_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_pool_pass, w4_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_pool_drop, w4_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_aura_pass, w4_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_aura_drop, w4_);

	/* W5 */
	CNXK_TEL_DICT_INT(d, ctx, flow_tagw, w5_);
	CNXK_TEL_DICT_INT(d, ctx, bad_utag, w5_);
	CNXK_TEL_DICT_INT(d, ctx, good_utag, w5_);
	CNXK_TEL_DICT_INT(d, ctx, ltag, w5_);

	/* W6 */
	CNXK_TEL_DICT_U64(d, ctx, octs, w6_);

	/* W7 */
	CNXK_TEL_DICT_U64(d, ctx, pkts, w7_);

	/* W8 */
	CNXK_TEL_DICT_U64(d, ctx, drop_octs, w8_);

	/* W9 */
	CNXK_TEL_DICT_U64(d, ctx, drop_pkts, w9_);

	/* W10 */
	CNXK_TEL_DICT_U64(d, ctx, re_pkts, w10_);
}

static void
nix_rq_ctx(volatile void *qctx, struct plt_tel_data *d)
{
	volatile struct nix_cn10k_rq_ctx_s *ctx;

	ctx = (volatile struct nix_cn10k_rq_ctx_s *)qctx;

	/* W0 */
	CNXK_TEL_DICT_INT(d, ctx, wqe_aura, w0_);
	CNXK_TEL_DICT_INT(d, ctx, len_ol3_dis, w0_);
	CNXK_TEL_DICT_INT(d, ctx, len_ol4_dis, w0_);
	CNXK_TEL_DICT_INT(d, ctx, len_il3_dis, w0_);
	CNXK_TEL_DICT_INT(d, ctx, len_il4_dis, w0_);
	CNXK_TEL_DICT_INT(d, ctx, csum_ol4_dis, w0_);
	CNXK_TEL_DICT_INT(d, ctx, lenerr_dis, w0_);
	CNXK_TEL_DICT_INT(d, ctx, ena_wqwd, w0);
	CNXK_TEL_DICT_INT(d, ctx, ipsech_ena, w0);
	CNXK_TEL_DICT_INT(d, ctx, sso_ena, w0);
	CNXK_TEL_DICT_INT(d, ctx, ena, w0);

	/* W1 */
	CNXK_TEL_DICT_INT(d, ctx, chi_ena, w1_);
	CNXK_TEL_DICT_INT(d, ctx, ipsecd_drop_en, w1_);
	CNXK_TEL_DICT_INT(d, ctx, pb_stashing, w1_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_drop_ena, w1_);
	CNXK_TEL_DICT_INT(d, ctx, spb_drop_ena, w1_);
	CNXK_TEL_DICT_INT(d, ctx, xqe_drop_ena, w1_);
	CNXK_TEL_DICT_INT(d, ctx, wqe_caching, w1_);
	CNXK_TEL_DICT_INT(d, ctx, pb_caching, w1_);
	CNXK_TEL_DICT_INT(d, ctx, sso_tt, w1_);
	CNXK_TEL_DICT_INT(d, ctx, sso_grp, w1_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_aura, w1_);
	CNXK_TEL_DICT_INT(d, ctx, spb_aura, w1_);

	/* W2 */
	CNXK_TEL_DICT_INT(d, ctx, xqe_hdr_split, w2_);
	CNXK_TEL_DICT_INT(d, ctx, xqe_imm_copy, w2_);
	CNXK_TEL_DICT_INT(d, ctx, xqe_imm_size, w2_);
	CNXK_TEL_DICT_INT(d, ctx, later_skip, w2_);
	CNXK_TEL_DICT_INT(d, ctx, first_skip, w2_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_sizem1, w2_);
	CNXK_TEL_DICT_INT(d, ctx, spb_ena, w2_);
	CNXK_TEL_DICT_INT(d, ctx, wqe_skip, w2_);
	CNXK_TEL_DICT_INT(d, ctx, spb_sizem1, w2_);
	CNXK_TEL_DICT_INT(d, ctx, policer_ena, w2_);
	CNXK_TEL_DICT_INT(d, ctx, band_prof_id, w2_);

	/* W3 */
	CNXK_TEL_DICT_INT(d, ctx, spb_pool_pass, w3_);
	CNXK_TEL_DICT_INT(d, ctx, spb_pool_drop, w3_);
	CNXK_TEL_DICT_INT(d, ctx, spb_aura_pass, w3_);
	CNXK_TEL_DICT_INT(d, ctx, spb_aura_drop, w3_);
	CNXK_TEL_DICT_INT(d, ctx, wqe_pool_pass, w3_);
	CNXK_TEL_DICT_INT(d, ctx, wqe_pool_drop, w3_);
	CNXK_TEL_DICT_INT(d, ctx, xqe_pass, w3_);
	CNXK_TEL_DICT_INT(d, ctx, xqe_drop, w3_);

	/* W4 */
	CNXK_TEL_DICT_INT(d, ctx, qint_idx, w4_);
	CNXK_TEL_DICT_INT(d, ctx, rq_int_ena, w4_);
	CNXK_TEL_DICT_INT(d, ctx, rq_int, w4_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_pool_pass, w4_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_pool_drop, w4_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_aura_pass, w4_);
	CNXK_TEL_DICT_INT(d, ctx, lpb_aura_drop, w4_);

	/* W5 */
	CNXK_TEL_DICT_INT(d, ctx, vwqe_skip, w5_);
	CNXK_TEL_DICT_INT(d, ctx, max_vsize_exp, w5_);
	CNXK_TEL_DICT_INT(d, ctx, vtime_wait, w5_);
	CNXK_TEL_DICT_INT(d, ctx, vwqe_ena, w5_);
	CNXK_TEL_DICT_INT(d, ctx, ipsec_vwqe, w5_);
	CNXK_TEL_DICT_INT(d, ctx, flow_tagw, w5_);
	CNXK_TEL_DICT_INT(d, ctx, bad_utag, w5_);
	CNXK_TEL_DICT_INT(d, ctx, good_utag, w5_);
	CNXK_TEL_DICT_INT(d, ctx, ltag, w5_);

	/* W6 */
	CNXK_TEL_DICT_U64(d, ctx, octs, w6_);

	/* W7 */
	CNXK_TEL_DICT_U64(d, ctx, pkts, w7_);

	/* W8 */
	CNXK_TEL_DICT_U64(d, ctx, drop_octs, w8_);

	/* W9 */
	CNXK_TEL_DICT_U64(d, ctx, drop_pkts, w9_);

	/* W10 */
	CNXK_TEL_DICT_U64(d, ctx, re_pkts, w10_);
}

static int
cnxk_tel_nix_rq_ctx(struct roc_nix *roc_nix, uint8_t n, struct plt_tel_data *d)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct npa_lf *npa_lf;
	volatile void *qctx;
	int rc = -1;

	npa_lf = idev_npa_obj_get();
	if (npa_lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	rc = nix_q_ctx_get(dev, NIX_AQ_CTYPE_RQ, n, &qctx);
	if (rc) {
		plt_err("Failed to get rq context");
		return rc;
	}

	if (roc_model_is_cn9k())
		nix_rq_ctx_cn9k(qctx, d);
	else
		nix_rq_ctx(qctx, d);

	return 0;
}

static int
cnxk_tel_nix_cq_ctx(struct roc_nix *roc_nix, uint8_t n, struct plt_tel_data *d)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct npa_lf *npa_lf;
	volatile struct nix_cq_ctx_s *ctx;
	int rc = -1;

	npa_lf = idev_npa_obj_get();
	if (npa_lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	rc = nix_q_ctx_get(dev, NIX_AQ_CTYPE_CQ, n, (void *)&ctx);
	if (rc) {
		plt_err("Failed to get cq context");
		return rc;
	}

	/* W0 */
	CNXK_TEL_DICT_PTR(d, ctx, base, w0_);

	/* W1 */
	CNXK_TEL_DICT_U64(d, ctx, wrptr, w1_);
	CNXK_TEL_DICT_INT(d, ctx, avg_con, w1_);
	CNXK_TEL_DICT_INT(d, ctx, cint_idx, w1_);
	CNXK_TEL_DICT_INT(d, ctx, cq_err, w1_);
	CNXK_TEL_DICT_INT(d, ctx, qint_idx, w1_);
	CNXK_TEL_DICT_INT(d, ctx, bpid, w1_);
	CNXK_TEL_DICT_INT(d, ctx, bp_ena, w1_);

	/* W2 */
	CNXK_TEL_DICT_INT(d, ctx, update_time, w2_);
	CNXK_TEL_DICT_INT(d, ctx, avg_level, w2_);
	CNXK_TEL_DICT_INT(d, ctx, head, w2_);
	CNXK_TEL_DICT_INT(d, ctx, tail, w2_);

	/* W3 */
	CNXK_TEL_DICT_INT(d, ctx, cq_err_int_ena, w3_);
	CNXK_TEL_DICT_INT(d, ctx, cq_err_int, w3_);
	CNXK_TEL_DICT_INT(d, ctx, qsize, w3_);
	CNXK_TEL_DICT_INT(d, ctx, caching, w3_);
	CNXK_TEL_DICT_INT(d, ctx, substream, w3_);
	CNXK_TEL_DICT_INT(d, ctx, ena, w3_);
	CNXK_TEL_DICT_INT(d, ctx, drop_ena, w3_);
	CNXK_TEL_DICT_INT(d, ctx, drop, w3_);
	CNXK_TEL_DICT_INT(d, ctx, bp, w3_);

	return 0;
}

static void
nix_sq_ctx_cn9k(volatile void *qctx, struct plt_tel_data *d)
{
	volatile struct nix_sq_ctx_s *ctx;

	ctx = (volatile struct nix_sq_ctx_s *)qctx;

	/* W0 */
	CNXK_TEL_DICT_INT(d, ctx, sqe_way_mask, w0_);
	CNXK_TEL_DICT_INT(d, ctx, cq, w0_);
	CNXK_TEL_DICT_INT(d, ctx, sdp_mcast, w0_);
	CNXK_TEL_DICT_INT(d, ctx, substream, w0_);
	CNXK_TEL_DICT_INT(d, ctx, qint_idx, w0_);
	CNXK_TEL_DICT_INT(d, ctx, ena, w0_);

	/* W1 */
	CNXK_TEL_DICT_INT(d, ctx, sqb_count, w1_);
	CNXK_TEL_DICT_INT(d, ctx, default_chan, w1_);
	CNXK_TEL_DICT_INT(d, ctx, smq_rr_quantum, w1_);
	CNXK_TEL_DICT_INT(d, ctx, sso_ena, w1_);
	CNXK_TEL_DICT_INT(d, ctx, xoff, w1_);
	CNXK_TEL_DICT_INT(d, ctx, cq_ena, w1_);
	CNXK_TEL_DICT_INT(d, ctx, smq, w1_);

	/* W2 */
	CNXK_TEL_DICT_INT(d, ctx, sqe_stype, w2_);
	CNXK_TEL_DICT_INT(d, ctx, sq_int_ena, w2_);
	CNXK_TEL_DICT_INT(d, ctx, sq_int, w2_);
	CNXK_TEL_DICT_INT(d, ctx, sqb_aura, w2_);
	CNXK_TEL_DICT_INT(d, ctx, smq_rr_count, w2_);

	/* W3 */
	CNXK_TEL_DICT_INT(d, ctx, smq_next_sq_vld, w3_);
	CNXK_TEL_DICT_INT(d, ctx, smq_pend, w3_);
	CNXK_TEL_DICT_INT(d, ctx, smenq_next_sqb_vld, w3_);
	CNXK_TEL_DICT_INT(d, ctx, head_offset, w3_);
	CNXK_TEL_DICT_INT(d, ctx, smenq_offset, w3_);
	CNXK_TEL_DICT_INT(d, ctx, tail_offset, w3_);
	CNXK_TEL_DICT_INT(d, ctx, smq_lso_segnum, w3_);
	CNXK_TEL_DICT_INT(d, ctx, smq_next_sq, w3_);
	CNXK_TEL_DICT_INT(d, ctx, mnq_dis, w3_);
	CNXK_TEL_DICT_INT(d, ctx, lmt_dis, w3_);
	CNXK_TEL_DICT_INT(d, ctx, cq_limit, w3_);
	CNXK_TEL_DICT_INT(d, ctx, max_sqe_size, w3_);

	/* W4 */
	CNXK_TEL_DICT_PTR(d, ctx, next_sqb, w4_);

	/* W5 */
	CNXK_TEL_DICT_PTR(d, ctx, tail_sqb, w5_);

	/* W6 */
	CNXK_TEL_DICT_PTR(d, ctx, smenq_sqb, w6_);

	/* W7 */
	CNXK_TEL_DICT_PTR(d, ctx, smenq_next_sqb, w7_);

	/* W8 */
	CNXK_TEL_DICT_PTR(d, ctx, head_sqb, w8_);

	/* W9 */
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_vld, w9_);
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_vlan1_ins_ena, w9_);
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_vlan0_ins_ena, w9_);
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_mps, w9_);
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_sb, w9_);
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_sizem1, w9_);
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_total, w9_);

	/* W10 */
	CNXK_TEL_DICT_BF_PTR(d, ctx, scm_lso_rem, w10_);

	/* W11 */
	CNXK_TEL_DICT_BF_PTR(d, ctx, octs, w11_);

	/* W12 */
	CNXK_TEL_DICT_BF_PTR(d, ctx, pkts, w12_);

	/* W14 */
	CNXK_TEL_DICT_BF_PTR(d, ctx, drop_octs, w14_);

	/* W15 */
	CNXK_TEL_DICT_BF_PTR(d, ctx, drop_pkts, w15_);
}

static void
nix_sq_ctx(volatile void *qctx, struct plt_tel_data *d)
{
	volatile struct nix_cn10k_sq_ctx_s *ctx;

	ctx = (volatile struct nix_cn10k_sq_ctx_s *)qctx;

	/* W0 */
	CNXK_TEL_DICT_INT(d, ctx, sqe_way_mask, w0_);
	CNXK_TEL_DICT_INT(d, ctx, cq, w0_);
	CNXK_TEL_DICT_INT(d, ctx, sdp_mcast, w0_);
	CNXK_TEL_DICT_INT(d, ctx, substream, w0_);
	CNXK_TEL_DICT_INT(d, ctx, qint_idx, w0_);
	CNXK_TEL_DICT_INT(d, ctx, ena, w0_);

	/* W1 */
	CNXK_TEL_DICT_INT(d, ctx, sqb_count, w1_);
	CNXK_TEL_DICT_INT(d, ctx, default_chan, w1_);
	CNXK_TEL_DICT_INT(d, ctx, smq_rr_weight, w1_);
	CNXK_TEL_DICT_INT(d, ctx, sso_ena, w1_);
	CNXK_TEL_DICT_INT(d, ctx, xoff, w1_);
	CNXK_TEL_DICT_INT(d, ctx, cq_ena, w1_);
	CNXK_TEL_DICT_INT(d, ctx, smq, w1_);

	/* W2 */
	CNXK_TEL_DICT_INT(d, ctx, sqe_stype, w2_);
	CNXK_TEL_DICT_INT(d, ctx, sq_int_ena, w2_);
	CNXK_TEL_DICT_INT(d, ctx, sq_int, w2_);
	CNXK_TEL_DICT_INT(d, ctx, sqb_aura, w2_);
	CNXK_TEL_DICT_INT(d, ctx, smq_rr_count_ub, w2_);
	CNXK_TEL_DICT_INT(d, ctx, smq_rr_count_lb, w2_);

	/* W3 */
	CNXK_TEL_DICT_INT(d, ctx, smq_next_sq_vld, w3_);
	CNXK_TEL_DICT_INT(d, ctx, smq_pend, w3_);
	CNXK_TEL_DICT_INT(d, ctx, smenq_next_sqb_vld, w3_);
	CNXK_TEL_DICT_INT(d, ctx, head_offset, w3_);
	CNXK_TEL_DICT_INT(d, ctx, smenq_offset, w3_);
	CNXK_TEL_DICT_INT(d, ctx, tail_offset, w3_);
	CNXK_TEL_DICT_INT(d, ctx, smq_lso_segnum, w3_);
	CNXK_TEL_DICT_INT(d, ctx, smq_next_sq, w3_);
	CNXK_TEL_DICT_INT(d, ctx, mnq_dis, w3_);
	CNXK_TEL_DICT_INT(d, ctx, lmt_dis, w3_);
	CNXK_TEL_DICT_INT(d, ctx, cq_limit, w3_);
	CNXK_TEL_DICT_INT(d, ctx, max_sqe_size, w3_);

	/* W4 */
	CNXK_TEL_DICT_PTR(d, ctx, next_sqb, w4_);

	/* W5 */
	CNXK_TEL_DICT_PTR(d, ctx, tail_sqb, w5_);

	/* W6 */
	CNXK_TEL_DICT_PTR(d, ctx, smenq_sqb, w6_);

	/* W7 */
	CNXK_TEL_DICT_PTR(d, ctx, smenq_next_sqb, w7_);

	/* W8 */
	CNXK_TEL_DICT_PTR(d, ctx, head_sqb, w8_);

	/* W9 */
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_vld, w9_);
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_vlan1_ins_ena, w9_);
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_vlan0_ins_ena, w9_);
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_mps, w9_);
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_sb, w9_);
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_sizem1, w9_);
	CNXK_TEL_DICT_INT(d, ctx, vfi_lso_total, w9_);

	/* W10 */
	CNXK_TEL_DICT_BF_PTR(d, ctx, scm_lso_rem, w10_);

	/* W11 */
	CNXK_TEL_DICT_BF_PTR(d, ctx, octs, w11_);

	/* W12 */
	CNXK_TEL_DICT_BF_PTR(d, ctx, pkts, w12_);

	/* W13 */
	CNXK_TEL_DICT_INT(d, ctx, aged_drop_octs, w13_);
	CNXK_TEL_DICT_INT(d, ctx, aged_drop_pkts, w13_);

	/* W14 */
	CNXK_TEL_DICT_BF_PTR(d, ctx, drop_octs, w14_);

	/* W15 */
	CNXK_TEL_DICT_BF_PTR(d, ctx, drop_pkts, w15_);
}

static int
cnxk_tel_nix_sq_ctx(struct roc_nix *roc_nix, uint8_t n, struct plt_tel_data *d)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct npa_lf *npa_lf;
	volatile void *qctx;
	int rc = -1;

	npa_lf = idev_npa_obj_get();
	if (npa_lf == NULL)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	rc = nix_q_ctx_get(dev, NIX_AQ_CTYPE_SQ, n, &qctx);
	if (rc) {
		plt_err("Failed to get rq context");
		return rc;
	}

	if (roc_model_is_cn9k())
		nix_sq_ctx_cn9k(qctx, d);
	else
		nix_sq_ctx(qctx, d);

	return 0;
}

static int
cnxk_nix_tel_handle_list(const char *cmd __plt_unused,
			 const char *params __plt_unused,
			 struct plt_tel_data *d)
{
	struct nix_tel_node *node;
	struct roc_nix *roc_nix;

	plt_tel_data_start_array(d, PLT_TEL_STRING_VAL);

	TAILQ_FOREACH(node, &nix_list, node) {
		roc_nix = node->nix;
		plt_tel_data_add_array_string(d, roc_nix->pci_dev->name);
	}

	return 0;
}

static int
cnxk_nix_tel_handle_info(const char *cmd __plt_unused, const char *params,
			 struct plt_tel_data *d)
{
	char name[PCI_PRI_STR_SIZE];
	struct nix_tel_node *node;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -1;

	plt_strlcpy(name, params, PCI_PRI_STR_SIZE);

	node = nix_tel_node_get_by_pcidev_name(name);
	if (!node)
		return -1;

	plt_tel_data_start_dict(d);
	return cnxk_tel_nix(node->nix, d);
}

static int
cnxk_nix_tel_handle_info_x(const char *cmd, const char *params,
			   struct plt_tel_data *d)
{
	struct nix_tel_node *node;
	char *name, *param;
	char buf[1024];
	int rc = -1;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		goto exit;

	plt_strlcpy(buf, params, PCI_PRI_STR_SIZE + 1);
	name = strtok(buf, ",");
	if (name == NULL)
		goto exit;

	param = strtok(NULL, "\0");

	node = nix_tel_node_get_by_pcidev_name(name);
	if (!node)
		goto exit;

	plt_tel_data_start_dict(d);

	if (strstr(cmd, "rq")) {
		char *tok = strtok(param, ",");
		int rq;

		if (!tok)
			goto exit;

		rq = strtol(tok, NULL, 10);
		if ((node->n_rq <= rq) || (rq < 0))
			goto exit;

		if (strstr(cmd, "ctx"))
			rc = cnxk_tel_nix_rq_ctx(node->nix, rq, d);
		else
			rc = cnxk_tel_nix_rq(node->rqs[rq], d);

	} else if (strstr(cmd, "cq")) {
		char *tok = strtok(param, ",");
		int cq;

		if (!tok)
			goto exit;

		cq = strtol(tok, NULL, 10);
		if ((node->n_cq <= cq) || (cq < 0))
			goto exit;

		if (strstr(cmd, "ctx"))
			rc = cnxk_tel_nix_cq_ctx(node->nix, cq, d);
		else
			rc = cnxk_tel_nix_cq(node->cqs[cq], d);

	} else if (strstr(cmd, "sq")) {
		char *tok = strtok(param, ",");
		int sq;

		if (!tok)
			goto exit;

		sq = strtol(tok, NULL, 10);
		if ((node->n_sq <= sq) || (sq < 0))
			goto exit;

		if (strstr(cmd, "ctx"))
			rc = cnxk_tel_nix_sq_ctx(node->nix, sq, d);
		else
			rc = cnxk_tel_nix_sq(node->sqs[sq], d);
	}

exit:
	return rc;
}

PLT_INIT(cnxk_telemetry_nix_init)
{
	TAILQ_INIT(&nix_list);

	plt_telemetry_register_cmd(
		"/cnxk/nix/list", cnxk_nix_tel_handle_list,
		"Returns list of available NIX devices. Takes no parameters");
	plt_telemetry_register_cmd(
		"/cnxk/nix/info", cnxk_nix_tel_handle_info,
		"Returns nix information. Parameters: pci id");
	plt_telemetry_register_cmd(
		"/cnxk/nix/rq/info", cnxk_nix_tel_handle_info_x,
		"Returns nix rq information. Parameters: pci id, rq id");
	plt_telemetry_register_cmd(
		"/cnxk/nix/rq/ctx", cnxk_nix_tel_handle_info_x,
		"Returns nix rq context. Parameters: pci id, rq id");
	plt_telemetry_register_cmd(
		"/cnxk/nix/cq/info", cnxk_nix_tel_handle_info_x,
		"Returns nix cq information. Parameters: pci id, cq id");
	plt_telemetry_register_cmd(
		"/cnxk/nix/cq/ctx", cnxk_nix_tel_handle_info_x,
		"Returns nix cq context. Parameters: pci id, cq id");
	plt_telemetry_register_cmd(
		"/cnxk/nix/sq/info", cnxk_nix_tel_handle_info_x,
		"Returns nix sq information. Parameters: pci id, sq id");
	plt_telemetry_register_cmd(
		"/cnxk/nix/sq/ctx", cnxk_nix_tel_handle_info_x,
		"Returns nix sq context. Parameters: pci id, sq id");
}
