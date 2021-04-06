/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

static inline struct mbox *
get_mbox(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	return dev->mbox;
}

int
roc_nix_mcast_mcam_entry_alloc(struct roc_nix *roc_nix, uint16_t nb_entries,
			       uint8_t priority, uint16_t index[])
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct npc_mcam_alloc_entry_req *req;
	struct npc_mcam_alloc_entry_rsp *rsp;
	int rc = -ENOSPC, i;

	req = mbox_alloc_msg_npc_mcam_alloc_entry(mbox);
	if (req == NULL)
		return rc;
	req->priority = priority;
	req->count = nb_entries;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	for (i = 0; i < rsp->count; i++)
		index[i] = rsp->entry_list[i];

	return rsp->count;
}

int
roc_nix_mcast_mcam_entry_free(struct roc_nix *roc_nix, uint32_t index)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct npc_mcam_free_entry_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_free_entry(mbox);
	if (req == NULL)
		return rc;
	req->entry = index;

	return mbox_process_msg(mbox, NULL);
}

int
roc_nix_mcast_mcam_entry_write(struct roc_nix *roc_nix,
			       struct mcam_entry *entry, uint32_t index,
			       uint8_t intf, uint64_t action)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct npc_mcam_write_entry_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_write_entry(mbox);
	if (req == NULL)
		return rc;
	req->entry = index;
	req->intf = intf;
	req->enable_entry = true;
	mbox_memcpy(&req->entry_data, entry, sizeof(struct mcam_entry));
	req->entry_data.action = action;

	return mbox_process(mbox);
}

int
roc_nix_mcast_mcam_entry_ena_dis(struct roc_nix *roc_nix, uint32_t index,
				 bool enable)
{
	struct npc_mcam_ena_dis_entry_req *req;
	struct mbox *mbox = get_mbox(roc_nix);
	int rc = -ENOSPC;

	if (enable) {
		req = mbox_alloc_msg_npc_mcam_ena_entry(mbox);
		if (req == NULL)
			return rc;
	} else {
		req = mbox_alloc_msg_npc_mcam_dis_entry(mbox);
		if (req == NULL)
			return rc;
	}

	req->entry = index;
	return mbox_process(mbox);
}
