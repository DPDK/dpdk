/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

void
roc_nix_rss_key_default_fill(struct roc_nix *roc_nix,
			     uint8_t key[ROC_NIX_RSS_KEY_LEN])
{
	PLT_SET_USED(roc_nix);
	const uint8_t default_key[ROC_NIX_RSS_KEY_LEN] = {
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED,
		0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD,
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED,
		0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD,
		0xFE, 0xED, 0x0B, 0xAD, 0xFE, 0xED, 0x0B, 0xAD};

	memcpy(key, default_key, ROC_NIX_RSS_KEY_LEN);
}

void
roc_nix_rss_key_set(struct roc_nix *roc_nix,
		    const uint8_t key[ROC_NIX_RSS_KEY_LEN])
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	const uint64_t *keyptr;
	uint64_t val;
	uint32_t idx;

	keyptr = (const uint64_t *)key;
	for (idx = 0; idx < (ROC_NIX_RSS_KEY_LEN >> 3); idx++) {
		val = plt_cpu_to_be_64(keyptr[idx]);
		plt_write64(val, nix->base + NIX_LF_RX_SECRETX(idx));
	}
}

void
roc_nix_rss_key_get(struct roc_nix *roc_nix, uint8_t key[ROC_NIX_RSS_KEY_LEN])
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint64_t *keyptr = (uint64_t *)key;
	uint64_t val;
	uint32_t idx;

	for (idx = 0; idx < (ROC_NIX_RSS_KEY_LEN >> 3); idx++) {
		val = plt_read64(nix->base + NIX_LF_RX_SECRETX(idx));
		keyptr[idx] = plt_be_to_cpu_64(val);
	}
}

static int
nix_cn9k_rss_reta_set(struct nix *nix, uint8_t group,
		      uint16_t reta[ROC_NIX_RSS_RETA_MAX], uint8_t lock_rx_ctx)
{
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_aq_enq_req *req;
	uint16_t idx;
	int rc;

	for (idx = 0; idx < nix->reta_sz; idx++) {
		req = mbox_alloc_msg_nix_aq_enq(mbox);
		if (!req) {
			/* The shared memory buffer can be full.
			 * Flush it and retry
			 */
			rc = mbox_process(mbox);
			if (rc < 0)
				goto exit;
			req = mbox_alloc_msg_nix_aq_enq(mbox);
			if (!req) {
				rc =  NIX_ERR_NO_MEM;
				goto exit;
			}
		}
		req->rss.rq = reta[idx];
		/* Fill AQ info */
		req->qidx = (group * nix->reta_sz) + idx;
		req->ctype = NIX_AQ_CTYPE_RSS;
		req->op = NIX_AQ_INSTOP_INIT;

		if (!lock_rx_ctx)
			continue;

		req = mbox_alloc_msg_nix_aq_enq(mbox);
		if (!req) {
			/* The shared memory buffer can be full.
			 * Flush it and retry
			 */
			rc = mbox_process(mbox);
			if (rc < 0)
				goto exit;
			req = mbox_alloc_msg_nix_aq_enq(mbox);
			if (!req) {
				rc =  NIX_ERR_NO_MEM;
				goto exit;
			}
		}
		req->rss.rq = reta[idx];
		/* Fill AQ info */
		req->qidx = (group * nix->reta_sz) + idx;
		req->ctype = NIX_AQ_CTYPE_RSS;
		req->op = NIX_AQ_INSTOP_LOCK;
	}

	rc = mbox_process(mbox);
	if (rc < 0)
		goto exit;

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

static int
nix_rss_reta_set(struct nix *nix, uint8_t group,
		 uint16_t reta[ROC_NIX_RSS_RETA_MAX], uint8_t lock_rx_ctx)
{
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_cn10k_aq_enq_req *req;
	uint16_t idx;
	int rc;

	for (idx = 0; idx < nix->reta_sz; idx++) {
		req = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		if (!req) {
			/* The shared memory buffer can be full.
			 * Flush it and retry
			 */
			rc = mbox_process(mbox);
			if (rc < 0)
				goto exit;
			req = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
			if (!req) {
				rc =  NIX_ERR_NO_MEM;
				goto exit;
			}
		}
		req->rss.rq = reta[idx];
		/* Fill AQ info */
		req->qidx = (group * nix->reta_sz) + idx;
		req->ctype = NIX_AQ_CTYPE_RSS;
		req->op = NIX_AQ_INSTOP_INIT;

		if (!lock_rx_ctx)
			continue;

		req = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
		if (!req) {
			/* The shared memory buffer can be full.
			 * Flush it and retry
			 */
			rc = mbox_process(mbox);
			if (rc < 0)
				goto exit;
			req = mbox_alloc_msg_nix_cn10k_aq_enq(mbox);
			if (!req) {
				rc =  NIX_ERR_NO_MEM;
				goto exit;
			}
		}
		req->rss.rq = reta[idx];
		/* Fill AQ info */
		req->qidx = (group * nix->reta_sz) + idx;
		req->ctype = NIX_AQ_CTYPE_RSS;
		req->op = NIX_AQ_INSTOP_LOCK;
	}

	rc = mbox_process(mbox);
	if (rc < 0)
		goto exit;

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_rss_reta_set(struct roc_nix *roc_nix, uint8_t group,
		     uint16_t reta[ROC_NIX_RSS_RETA_MAX])
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	int rc;

	if (group >= ROC_NIX_RSS_GRPS)
		return NIX_ERR_PARAM;

	if (roc_model_is_cn9k())
		rc = nix_cn9k_rss_reta_set(nix, group, reta,
					   roc_nix->lock_rx_ctx);
	else
		rc = nix_rss_reta_set(nix, group, reta, roc_nix->lock_rx_ctx);
	if (rc)
		return rc;

	memcpy(&nix->reta[group], reta, sizeof(uint16_t) * ROC_NIX_RSS_RETA_MAX);
	return 0;
}

int
roc_nix_rss_reta_get(struct roc_nix *roc_nix, uint8_t group,
		     uint16_t reta[ROC_NIX_RSS_RETA_MAX])
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);

	if (group >= ROC_NIX_RSS_GRPS)
		return NIX_ERR_PARAM;

	memcpy(reta, &nix->reta[group], sizeof(uint16_t) * ROC_NIX_RSS_RETA_MAX);
	return 0;
}

int
roc_nix_rss_flowkey_set(struct roc_nix *roc_nix, uint8_t *alg_idx,
			uint32_t flowkey, uint8_t group, int mcam_index)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct nix_rss_flowkey_cfg_rsp *rss_rsp;
	struct mbox *mbox = mbox_get((&nix->dev)->mbox);
	struct nix_rss_flowkey_cfg *cfg;
	int rc = -ENOSPC;

	if (group >= ROC_NIX_RSS_GRPS) {
		rc = NIX_ERR_PARAM;
		goto exit;
	}

	cfg = mbox_alloc_msg_nix_rss_flowkey_cfg(mbox);
	if (cfg == NULL)
		goto exit;
	cfg->flowkey_cfg = flowkey;
	cfg->mcam_index = mcam_index; /* -1 indicates default group */
	cfg->group = group;	      /* 0 is default group */
	rc = mbox_process_msg(mbox, (void *)&rss_rsp);
	if (rc)
		goto exit;
	if (alg_idx)
		*alg_idx = rss_rsp->alg_idx;

exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_rss_default_setup(struct roc_nix *roc_nix, uint32_t flowkey)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	uint16_t idx, qcnt = nix->nb_rx_queues;
	uint16_t reta[ROC_NIX_RSS_RETA_MAX];
	uint8_t key[ROC_NIX_RSS_KEY_LEN];
	uint8_t alg_idx;
	int rc;

	roc_nix_rss_key_default_fill(roc_nix, key);
	roc_nix_rss_key_set(roc_nix, key);

	/* Update default RSS RETA */
	for (idx = 0; idx < nix->reta_sz; idx++)
		reta[idx] = idx % qcnt;
	rc = roc_nix_rss_reta_set(roc_nix, 0, reta);
	if (rc) {
		plt_err("Failed to set RSS reta table rc=%d", rc);
		goto fail;
	}

	/* Update the default flowkey */
	rc = roc_nix_rss_flowkey_set(roc_nix, &alg_idx, flowkey,
				     ROC_NIX_RSS_GROUP_DEFAULT, -1);
	if (rc) {
		plt_err("Failed to set RSS flowkey rc=%d", rc);
		goto fail;
	}

	nix->rss_alg_idx = alg_idx;
fail:
	return rc;
}
