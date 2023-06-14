/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

int
roc_mcs_flowid_stats_get(struct roc_mcs *mcs, struct roc_mcs_stats_req *mcs_req,
			 struct roc_mcs_flowid_stats *stats)
{
	struct mcs_flowid_stats *rsp;
	struct mcs_stats_req *req;
	int rc;

	MCS_SUPPORT_CHECK;

	req = mbox_alloc_msg_mcs_get_flowid_stats(mcs->mbox);
	if (req == NULL)
		return -ENOSPC;

	req->id = mcs_req->id;
	req->mcs_id = mcs->idx;
	req->dir = mcs_req->dir;

	rc = mbox_process_msg(mcs->mbox, (void *)&rsp);
	if (rc)
		return rc;

	stats->tcam_hit_cnt = rsp->tcam_hit_cnt;

	return rc;
}

int
roc_mcs_secy_stats_get(struct roc_mcs *mcs, struct roc_mcs_stats_req *mcs_req,
		       struct roc_mcs_secy_stats *stats)
{
	struct mcs_secy_stats *rsp;
	struct mcs_stats_req *req;
	int rc;

	MCS_SUPPORT_CHECK;

	req = mbox_alloc_msg_mcs_get_secy_stats(mcs->mbox);
	if (req == NULL)
		return -ENOSPC;

	req->id = mcs_req->id;
	req->mcs_id = mcs->idx;
	req->dir = mcs_req->dir;

	rc = mbox_process_msg(mcs->mbox, (void *)&rsp);
	if (rc)
		return rc;

	stats->ctl_pkt_bcast_cnt = rsp->ctl_pkt_bcast_cnt;
	stats->ctl_pkt_mcast_cnt = rsp->ctl_pkt_mcast_cnt;
	stats->ctl_pkt_ucast_cnt = rsp->ctl_pkt_ucast_cnt;
	stats->ctl_octet_cnt = rsp->ctl_octet_cnt;
	stats->unctl_pkt_bcast_cnt = rsp->unctl_pkt_bcast_cnt;
	stats->unctl_pkt_mcast_cnt = rsp->unctl_pkt_mcast_cnt;
	stats->unctl_pkt_ucast_cnt = rsp->unctl_pkt_ucast_cnt;
	stats->unctl_octet_cnt = rsp->unctl_octet_cnt;

	if (mcs_req->dir == MCS_RX) {
		stats->octet_decrypted_cnt = rsp->octet_decrypted_cnt;
		stats->octet_validated_cnt = rsp->octet_validated_cnt;
		stats->pkt_port_disabled_cnt = rsp->pkt_port_disabled_cnt;
		stats->pkt_badtag_cnt = rsp->pkt_badtag_cnt;
		stats->pkt_nosa_cnt = rsp->pkt_nosa_cnt;
		stats->pkt_nosaerror_cnt = rsp->pkt_nosaerror_cnt;
		stats->pkt_tagged_ctl_cnt = rsp->pkt_tagged_ctl_cnt;
		stats->pkt_untaged_cnt = rsp->pkt_untaged_cnt;
		if (roc_model_is_cn10kb_a0())
			/* CN10K-B */
			stats->pkt_ctl_cnt = rsp->pkt_ctl_cnt;
		else
			/* CNF10K-B */
			stats->pkt_notag_cnt = rsp->pkt_notag_cnt;
	} else {
		stats->octet_encrypted_cnt = rsp->octet_encrypted_cnt;
		stats->octet_protected_cnt = rsp->octet_protected_cnt;
		stats->pkt_noactivesa_cnt = rsp->pkt_noactivesa_cnt;
		stats->pkt_toolong_cnt = rsp->pkt_toolong_cnt;
		stats->pkt_untagged_cnt = rsp->pkt_untagged_cnt;
	}

	return rc;
}

int
roc_mcs_sc_stats_get(struct roc_mcs *mcs, struct roc_mcs_stats_req *mcs_req,
		     struct roc_mcs_sc_stats *stats)
{
	struct mcs_stats_req *req;
	struct mcs_sc_stats *rsp;
	int rc;

	MCS_SUPPORT_CHECK;

	req = mbox_alloc_msg_mcs_get_sc_stats(mcs->mbox);
	if (req == NULL)
		return -ENOSPC;

	req->id = mcs_req->id;
	req->mcs_id = mcs->idx;
	req->dir = mcs_req->dir;

	rc = mbox_process_msg(mcs->mbox, (void *)&rsp);
	if (rc)
		return rc;

	if (mcs_req->dir == MCS_RX) {
		stats->hit_cnt = rsp->hit_cnt;
		stats->pkt_invalid_cnt = rsp->pkt_invalid_cnt;
		stats->pkt_late_cnt = rsp->pkt_late_cnt;
		stats->pkt_notvalid_cnt = rsp->pkt_notvalid_cnt;
		stats->pkt_unchecked_cnt = rsp->pkt_unchecked_cnt;
		if (roc_model_is_cn10kb_a0()) {
			stats->octet_decrypt_cnt = rsp->octet_decrypt_cnt;
			stats->octet_validate_cnt = rsp->octet_validate_cnt;
		} else {
			stats->pkt_delay_cnt = rsp->pkt_delay_cnt;
			stats->pkt_ok_cnt = rsp->pkt_ok_cnt;
		}
	} else {
		stats->pkt_encrypt_cnt = rsp->pkt_encrypt_cnt;
		stats->pkt_protected_cnt = rsp->pkt_protected_cnt;
		if (roc_model_is_cn10kb_a0()) {
			stats->octet_encrypt_cnt = rsp->octet_encrypt_cnt;
			stats->octet_protected_cnt = rsp->octet_protected_cnt;
		}
	}

	return rc;
}

int
roc_mcs_port_stats_get(struct roc_mcs *mcs, struct roc_mcs_stats_req *mcs_req,
		       struct roc_mcs_port_stats *stats)
{
	struct mcs_port_stats *rsp;
	struct mcs_stats_req *req;
	int rc;

	MCS_SUPPORT_CHECK;

	req = mbox_alloc_msg_mcs_get_port_stats(mcs->mbox);
	if (req == NULL)
		return -ENOSPC;

	req->id = mcs_req->id;
	req->mcs_id = mcs->idx;
	req->dir = mcs_req->dir;

	rc = mbox_process_msg(mcs->mbox, (void *)&rsp);
	if (rc)
		return rc;

	stats->tcam_miss_cnt = rsp->tcam_miss_cnt;
	stats->parser_err_cnt = rsp->parser_err_cnt;
	if (roc_model_is_cnf10kb())
		stats->preempt_err_cnt = rsp->preempt_err_cnt;

	stats->sectag_insert_err_cnt = rsp->sectag_insert_err_cnt;

	return rc;
}

int
roc_mcs_stats_clear(struct roc_mcs *mcs, struct roc_mcs_clear_stats *mcs_req)
{
	struct mcs_clear_stats *req;
	struct msg_rsp *rsp;

	MCS_SUPPORT_CHECK;

	if (!roc_model_is_cn10kb_a0() && mcs_req->type == MCS_SA_STATS)
		return MCS_ERR_HW_NOTSUP;

	req = mbox_alloc_msg_mcs_clear_stats(mcs->mbox);
	if (req == NULL)
		return -ENOSPC;

	req->type = mcs_req->type;
	req->id = mcs_req->id;
	req->mcs_id = mcs->idx;
	req->dir = mcs_req->dir;
	req->all = mcs_req->all;

	return mbox_process_msg(mcs->mbox, (void *)&rsp);
}
