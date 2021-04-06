/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

int
roc_nix_tm_sq_aura_fc(struct roc_nix_sq *sq, bool enable)
{
	struct npa_aq_enq_req *req;
	struct npa_aq_enq_rsp *rsp;
	uint64_t aura_handle;
	struct npa_lf *lf;
	struct mbox *mbox;
	int rc = -ENOSPC;

	plt_tm_dbg("Setting SQ %u SQB aura FC to %s", sq->qid,
		   enable ? "enable" : "disable");

	lf = idev_npa_obj_get();
	if (!lf)
		return NPA_ERR_DEVICE_NOT_BOUNDED;

	mbox = lf->mbox;
	/* Set/clear sqb aura fc_ena */
	aura_handle = sq->aura_handle;
	req = mbox_alloc_msg_npa_aq_enq(mbox);
	if (req == NULL)
		return rc;

	req->aura_id = roc_npa_aura_handle_to_aura(aura_handle);
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_WRITE;
	/* Below is not needed for aura writes but AF driver needs it */
	/* AF will translate to associated poolctx */
	req->aura.pool_addr = req->aura_id;

	req->aura.fc_ena = enable;
	req->aura_mask.fc_ena = 1;

	rc = mbox_process(mbox);
	if (rc)
		return rc;

	/* Read back npa aura ctx */
	req = mbox_alloc_msg_npa_aq_enq(mbox);
	if (req == NULL)
		return -ENOSPC;

	req->aura_id = roc_npa_aura_handle_to_aura(aura_handle);
	req->ctype = NPA_AQ_CTYPE_AURA;
	req->op = NPA_AQ_INSTOP_READ;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	/* Init when enabled as there might be no triggers */
	if (enable)
		*(volatile uint64_t *)sq->fc = rsp->aura.count;
	else
		*(volatile uint64_t *)sq->fc = sq->nb_sqb_bufs;
	/* Sync write barrier */
	plt_wmb();
	return 0;
}
