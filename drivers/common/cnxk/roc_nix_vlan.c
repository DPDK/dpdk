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
roc_nix_vlan_mcam_entry_read(struct roc_nix *roc_nix, uint32_t index,
			     struct npc_mcam_read_entry_rsp **rsp)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct npc_mcam_read_entry_req *req;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_read_entry(mbox);
	if (req == NULL)
		return rc;
	req->entry = index;

	return mbox_process_msg(mbox, (void **)rsp);
}

int
roc_nix_vlan_mcam_entry_write(struct roc_nix *roc_nix, uint32_t index,
			      struct mcam_entry *entry, uint8_t intf,
			      uint8_t enable)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct npc_mcam_write_entry_req *req;
	struct msghdr *rsp;
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_write_entry(mbox);
	if (req == NULL)
		return rc;
	req->entry = index;
	req->intf = intf;
	req->enable_entry = enable;
	mbox_memcpy(&req->entry_data, entry, sizeof(struct mcam_entry));

	return mbox_process_msg(mbox, (void *)&rsp);
}

int
roc_nix_vlan_mcam_entry_alloc_and_write(struct roc_nix *roc_nix,
					struct mcam_entry *entry, uint8_t intf,
					uint8_t priority, uint8_t ref_entry)
{
	struct npc_mcam_alloc_and_write_entry_req *req;
	struct npc_mcam_alloc_and_write_entry_rsp *rsp;
	struct mbox *mbox = get_mbox(roc_nix);
	int rc = -ENOSPC;

	req = mbox_alloc_msg_npc_mcam_alloc_and_write_entry(mbox);
	if (req == NULL)
		return rc;
	req->priority = priority;
	req->ref_entry = ref_entry;
	req->intf = intf;
	req->enable_entry = true;
	mbox_memcpy(&req->entry_data, entry, sizeof(struct mcam_entry));

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	return rsp->entry;
}

int
roc_nix_vlan_mcam_entry_free(struct roc_nix *roc_nix, uint32_t index)
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
roc_nix_vlan_mcam_entry_ena_dis(struct roc_nix *roc_nix, uint32_t index,
				const int enable)
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
	return mbox_process_msg(mbox, NULL);
}

int
roc_nix_vlan_strip_vtag_ena_dis(struct roc_nix *roc_nix, bool enable)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_vtag_config *vtag_cfg;
	int rc = -ENOSPC;

	vtag_cfg = mbox_alloc_msg_nix_vtag_cfg(mbox);
	if (vtag_cfg == NULL)
		return rc;
	vtag_cfg->vtag_size = NIX_VTAGSIZE_T4;
	vtag_cfg->cfg_type = 1;	       /* Rx VLAN configuration */
	vtag_cfg->rx.capture_vtag = 1; /* Always capture */
	vtag_cfg->rx.vtag_type = 0;    /* Use index 0 */

	if (enable)
		vtag_cfg->rx.strip_vtag = 1;
	else
		vtag_cfg->rx.strip_vtag = 0;

	return mbox_process(mbox);
}

int
roc_nix_vlan_insert_ena_dis(struct roc_nix *roc_nix,
			    struct roc_nix_vlan_config *vlan_cfg,
			    uint64_t *mcam_index, bool enable)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_vtag_config *vtag_cfg;
	struct nix_vtag_config_rsp *rsp;
	int rc = -ENOSPC;

	vtag_cfg = mbox_alloc_msg_nix_vtag_cfg(mbox);
	if (vtag_cfg == NULL)
		return rc;
	vtag_cfg->cfg_type = 0; /* Tx VLAN configuration */
	vtag_cfg->vtag_size = NIX_VTAGSIZE_T4;

	if (enable) {
		if (vlan_cfg->type & ROC_NIX_VLAN_TYPE_INNER) {
			vtag_cfg->tx.vtag0 = vlan_cfg->vlan.vtag_inner;
			vtag_cfg->tx.cfg_vtag0 = true;
		}
		if (vlan_cfg->type & ROC_NIX_VLAN_TYPE_OUTER) {
			vtag_cfg->tx.vtag1 = vlan_cfg->vlan.vtag_outer;
			vtag_cfg->tx.cfg_vtag1 = true;
		}
	} else {
		if (vlan_cfg->type & ROC_NIX_VLAN_TYPE_INNER) {
			vtag_cfg->tx.vtag0_idx = vlan_cfg->mcam.idx_inner;
			vtag_cfg->tx.free_vtag0 = true;
		}
		if (vlan_cfg->type & ROC_NIX_VLAN_TYPE_OUTER) {
			vtag_cfg->tx.vtag1_idx = vlan_cfg->mcam.idx_outer;
			vtag_cfg->tx.free_vtag1 = true;
		}
	}

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	if (enable)
		*mcam_index =
			(((uint64_t)rsp->vtag1_idx << 32) | rsp->vtag0_idx);

	return 0;
}

int
roc_nix_vlan_tpid_set(struct roc_nix *roc_nix, uint32_t type, uint16_t tpid)
{
	struct mbox *mbox = get_mbox(roc_nix);
	struct nix_set_vlan_tpid *tpid_cfg;
	int rc = -ENOSPC;

	tpid_cfg = mbox_alloc_msg_nix_set_vlan_tpid(mbox);
	if (tpid_cfg == NULL)
		return rc;
	tpid_cfg->tpid = tpid;

	if (type & ROC_NIX_VLAN_TYPE_OUTER)
		tpid_cfg->vlan_type = NIX_VLAN_TYPE_OUTER;
	else
		tpid_cfg->vlan_type = NIX_VLAN_TYPE_INNER;

	return mbox_process(mbox);
}
