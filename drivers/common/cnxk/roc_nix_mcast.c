/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

int
roc_nix_mcast_mcam_entry_alloc(struct roc_nix *roc_nix, uint16_t nb_entries,
			       uint8_t priority, uint16_t index[])
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct npc_mcam_alloc_entry_req *req;
	struct npc_mcam_alloc_entry_rsp *rsp;
	int rc = -ENOSPC, i;

	req = mbox_alloc_msg_npc_mcam_alloc_entry(mbox);
	if (req == NULL)
		goto exit;
	req->ref_priority = priority;
	req->count = nb_entries;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	for (i = 0; i < rsp->count; i++)
		index[i] = rsp->entry_list[i];

	rc = rsp->count;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mcast_mcam_entry_free(struct roc_nix *roc_nix, uint32_t index)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	struct npc_mcam_free_entry_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_free_entry(mbox);
	if (req == NULL)
		goto exit;
	req->entry = index;

	rc = mbox_process_msg(mbox, NULL);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mcast_mcam_entry_write(struct roc_nix *roc_nix, void *entry, uint32_t index, uint8_t intf,
			       uint64_t action)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc = -ENOSPC;

	if (roc_model_is_cn20k()) {
		struct npc_cn20k_mcam_write_entry_req *req;

		req = mbox_alloc_msg_npc_cn20k_mcam_write_entry(mbox);
		if (req == NULL)
			goto exit;
		req->entry = index;
		req->intf = intf;
		req->enable_entry = true;
		mbox_memcpy(&req->entry_data, entry, sizeof(struct cn20k_mcam_entry));
		req->entry_data.action = action;

	} else {
		struct npc_mcam_write_entry_req *req;

		req = mbox_alloc_msg_npc_mcam_write_entry(mbox);
		if (req == NULL)
			goto exit;
		req->entry = index;
		req->intf = intf;
		req->enable_entry = true;
		mbox_memcpy(&req->entry_data, entry, sizeof(struct mcam_entry));
		req->entry_data.action = action;
	}

	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mcast_mcam_entry_ena_dis(struct roc_nix *roc_nix, uint32_t index,
				 bool enable)
{
	struct npc_mcam_ena_dis_entry_req *req;
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct mbox *mbox = mbox_get(dev->mbox);
	int rc = -ENOSPC;

	if (enable) {
		req = mbox_alloc_msg_npc_mcam_ena_entry(mbox);
		if (req == NULL)
			goto exit;
	} else {
		req = mbox_alloc_msg_npc_mcam_dis_entry(mbox);
		if (req == NULL)
			goto exit;
	}

	req->entry = index;
	rc = mbox_process(mbox);
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mcast_list_setup(struct mbox *mbox, uint8_t intf, int nb_entries, uint16_t *pf_funcs,
			 uint16_t *channels, uint32_t *rqs, uint32_t *grp_index,
			 uint32_t *start_index)
{
	struct nix_mcast_grp_create_req *mce_grp_create_req;
	struct nix_mcast_grp_create_rsp *mce_grp_create_rsp;
	struct nix_mcast_grp_update_req *mce_grp_update_req;
	struct nix_mcast_grp_update_rsp *mce_grp_update_rsp;
	int rc = 0, i;

	mbox_get(mbox);

	mce_grp_create_req = mbox_alloc_msg_nix_mcast_grp_create(mbox);
	if (mce_grp_create_req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	mce_grp_create_req->dir = intf;
	rc = mbox_process_msg(mbox, (void *)&mce_grp_create_rsp);
	if (rc) {
		plt_err("Failed to create mirror list");
		goto exit;
	}

	*grp_index = mce_grp_create_rsp->mcast_grp_idx;

	mce_grp_update_req = mbox_alloc_msg_nix_mcast_grp_update(mbox);
	if (mce_grp_update_req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	mce_grp_update_req->mcast_grp_idx = *grp_index;
	mce_grp_update_req->op = NIX_MCAST_OP_ADD_ENTRY;
	mce_grp_update_req->num_mce_entry = nb_entries;
	for (i = 0; i < nb_entries; i++) {
		mce_grp_update_req->pcifunc[i] = pf_funcs[i];
		mce_grp_update_req->channel[i] = channels[i];
		mce_grp_update_req->rq_rss_index[i] = rqs[i];
		mce_grp_update_req->dest_type[i] = NIX_RX_RQ;
	}

	rc = mbox_process_msg(mbox, (void *)&mce_grp_update_rsp);
	if (rc) {
		plt_err("Failed to create mirror list");
		goto exit;
	}

	*start_index = (mce_grp_update_rsp->mce_start_index & 0xFFFFF);

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_nix_mcast_list_free(struct mbox *mbox, uint32_t mcast_grp_index)
{
	struct nix_mcast_grp_destroy_req *mce_grp_destroy_req;
	struct nix_mcast_grp_destroy_rsp *mce_grp_destroy_rsp;
	int rc = 0;

	mbox_get(mbox);

	mce_grp_destroy_req = mbox_alloc_msg_nix_mcast_grp_destroy(mbox);
	if (mce_grp_destroy_req == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	mce_grp_destroy_req->mcast_grp_idx = mcast_grp_index;
	rc = mbox_process_msg(mbox, (void *)&mce_grp_destroy_rsp);
	if (rc) {
		plt_err("Failed to destroy mirror group index");
		goto exit;
	}

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}
