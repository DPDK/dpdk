/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <arpa/inet.h>

#include "roc_api.h"
#include "roc_priv.h"

static int
eswitch_vlan_rx_cfg(uint16_t pcifunc, struct mbox *mbox)
{
	struct nix_vtag_config *vtag_cfg;
	int rc;

	vtag_cfg = mbox_alloc_msg_nix_vtag_cfg(mbox_get(mbox));
	if (!vtag_cfg) {
		rc = -EINVAL;
		goto exit;
	}

	/* config strip, capture and size */
	vtag_cfg->hdr.pcifunc = pcifunc;
	vtag_cfg->vtag_size = NIX_VTAGSIZE_T4;
	vtag_cfg->cfg_type = VTAG_RX; /* rx vlan cfg */
	vtag_cfg->rx.vtag_type = NIX_RX_VTAG_TYPE0;
	vtag_cfg->rx.strip_vtag = true;
	vtag_cfg->rx.capture_vtag = true;

	rc = mbox_process(mbox);
	if (rc)
		goto exit;

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

static int
eswitch_vlan_tx_cfg(struct roc_npc_flow *flow, uint16_t pcifunc, struct mbox *mbox,
		    uint16_t vlan_tci, uint16_t *vidx)
{
	struct nix_vtag_config *vtag_cfg;
	struct nix_vtag_config_rsp *rsp;
	int rc;

	union {
		uint64_t reg;
		struct nix_tx_vtag_action_s act;
	} tx_vtag_action;

	vtag_cfg = mbox_alloc_msg_nix_vtag_cfg(mbox_get(mbox));
	if (!vtag_cfg) {
		rc = -EINVAL;
		goto exit;
	}

	/* Insert vlan tag */
	vtag_cfg->hdr.pcifunc = pcifunc;
	vtag_cfg->vtag_size = NIX_VTAGSIZE_T4;
	vtag_cfg->cfg_type = VTAG_TX; /* tx vlan cfg */
	vtag_cfg->tx.cfg_vtag0 = true;
	vtag_cfg->tx.vtag0 = (((uint32_t)ROC_ESWITCH_VLAN_TPID << 16) | vlan_tci);

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	if (rsp->vtag0_idx < 0) {
		plt_err("Failed to config TX VTAG action");
		rc = -EINVAL;
		goto exit;
	}

	*vidx = rsp->vtag0_idx;
	tx_vtag_action.reg = 0;
	tx_vtag_action.act.vtag0_def = rsp->vtag0_idx;
	tx_vtag_action.act.vtag0_lid = NPC_LID_LA;
	tx_vtag_action.act.vtag0_op = NIX_TX_VTAGOP_INSERT;
	tx_vtag_action.act.vtag0_relptr = NIX_TX_VTAGACTION_VTAG0_RELPTR;

	flow->vtag_action = tx_vtag_action.reg;

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_eswitch_npc_mcam_tx_rule(struct roc_npc *roc_npc, struct roc_npc_flow *flow, uint16_t pcifunc,
			     uint32_t vlan_tci)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct npc_install_flow_req *req;
	struct npc_install_flow_rsp *rsp;
	struct mbox *mbox = npc->mbox;
	uint16_t vidx = 0, lbkid;
	int rc;

	rc = eswitch_vlan_tx_cfg(flow, roc_npc->pf_func, mbox, vlan_tci, &vidx);
	if (rc) {
		plt_err("Failed to configure VLAN TX, err %d", rc);
		goto fail;
	}

	req = mbox_alloc_msg_npc_install_flow(mbox_get(mbox));
	if (!req) {
		rc = -EINVAL;
		goto exit;
	}

	lbkid = 0;
	req->hdr.pcifunc = roc_npc->pf_func; /* Eswitch PF is requester */
	req->vf = pcifunc;
	req->entry = flow->mcam_id;
	req->intf = NPC_MCAM_TX;
	req->op = NIX_TX_ACTIONOP_UCAST_CHAN;
	req->index = (lbkid << 8) | ROC_ESWITCH_LBK_CHAN;
	req->set_cntr = 1;
	req->vtag0_def = vidx;
	req->vtag0_op = 1;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	flow->nix_intf = NIX_INTF_TX;
exit:
	mbox_put(mbox);
fail:
	return rc;
}

static int
eswitch_vtag_cfg_delete(struct roc_npc *roc_npc, struct roc_npc_flow *flow)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct nix_vtag_config *vtag_cfg;
	struct nix_vtag_config_rsp *rsp;
	struct mbox *mbox = npc->mbox;
	int rc = 0;

	union {
		uint64_t reg;
		struct nix_tx_vtag_action_s act;
	} tx_vtag_action;

	tx_vtag_action.reg = flow->vtag_action;
	vtag_cfg = mbox_alloc_msg_nix_vtag_cfg(mbox_get(mbox));

	if (vtag_cfg == NULL) {
		rc = -ENOSPC;
		goto exit;
	}

	vtag_cfg->cfg_type = VTAG_TX;
	vtag_cfg->vtag_size = NIX_VTAGSIZE_T4;
	vtag_cfg->tx.vtag0_idx = tx_vtag_action.act.vtag0_def;
	vtag_cfg->tx.free_vtag0 = true;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	rc = rsp->hdr.rc;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_eswitch_npc_mcam_delete_rule(struct roc_npc *roc_npc, struct roc_npc_flow *flow,
				 uint16_t pcifunc)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct npc_delete_flow_req *req;
	struct msg_rsp *rsp;
	struct mbox *mbox = npc->mbox;
	int rc = 0;

	/* Removing the VLAN TX config */
	if (flow->nix_intf == NIX_INTF_TX) {
		rc = eswitch_vtag_cfg_delete(roc_npc, flow);
		if (rc)
			plt_err("Failed to delete TX vtag config");
	}

	req = mbox_alloc_msg_npc_delete_flow(mbox_get(mbox));
	if (!req) {
		rc = -EINVAL;
		goto exit;
	}

	req->entry = flow->mcam_id;
	req->vf = pcifunc;
	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	rc = rsp->hdr.rc;
exit:
	mbox_put(mbox);
	return rc;
}

int
roc_eswitch_npc_mcam_rx_rule(struct roc_npc *roc_npc, struct roc_npc_flow *flow, uint16_t pcifunc,
			     uint16_t vlan_tci, uint16_t vlan_tci_mask)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct npc_install_flow_req *req;
	struct npc_install_flow_rsp *rsp;
	struct mbox *mbox = npc->mbox;
	bool is_esw_dev;
	int rc;

	/* For ESW PF/VF */
	is_esw_dev = (dev_get_pf(roc_npc->pf_func) == dev_get_pf(pcifunc));
	/* VLAN Rx config */
	if (is_esw_dev) {
		rc = eswitch_vlan_rx_cfg(roc_npc->pf_func, mbox);
		if (rc) {
			plt_err("Failed to configure VLAN RX rule, err %d", rc);
			goto fail;
		}
	}

	req = mbox_alloc_msg_npc_install_flow(mbox_get(mbox));
	if (!req) {
		rc = -EINVAL;
		goto exit;
	}

	req->vf = pcifunc;
	/* Action */
	req->op = NIX_RX_ACTIONOP_DEFAULT;
	req->index = 0;
	req->entry = flow->mcam_id;
	req->hdr.pcifunc = roc_npc->pf_func; /* Eswitch PF is requester */
	req->features = BIT_ULL(NPC_OUTER_VID) | BIT_ULL(NPC_VLAN_ETYPE_CTAG);
	req->vtag0_valid = true;
	/* For ESW PF/VF using configured vlan rx cfg while for other
	 * representees using standard vlan_type = 7 which is strip.
	 */
	req->vtag0_type = is_esw_dev ? NIX_RX_VTAG_TYPE0 : NIX_RX_VTAG_TYPE7;
	req->packet.vlan_etype = ROC_ESWITCH_VLAN_TPID;
	req->mask.vlan_etype = 0xFFFF;
	req->packet.vlan_tci = ntohs(vlan_tci & 0xFFFF);
	req->mask.vlan_tci = ntohs(vlan_tci_mask);

	req->channel = ROC_ESWITCH_LBK_CHAN;
	req->chan_mask = 0xffff;
	req->intf = NPC_MCAM_RX;
	req->set_cntr = 1;
	req->cntr_val = flow->ctr_id;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	flow->nix_intf = NIX_INTF_RX;
exit:
	mbox_put(mbox);
fail:
	return rc;
}

int
roc_eswitch_npc_rss_action_configure(struct roc_npc *roc_npc, struct roc_npc_flow *flow,
				     uint32_t flowkey_cfg, uint16_t *reta_tbl)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct roc_nix *roc_nix = roc_npc->roc_nix;
	uint32_t rss_grp_idx;
	uint8_t flowkey_algx;
	int rc;

	rc = npc_rss_free_grp_get(npc, &rss_grp_idx);
	/* RSS group :0 is not usable for flow rss action */
	if (rc < 0 || rss_grp_idx == 0)
		return -ENOSPC;

	/* Populating reta table for the specific RSS group */
	rc = roc_nix_rss_reta_set(roc_nix, rss_grp_idx, reta_tbl);
	if (rc) {
		plt_err("Failed to init rss table rc = %d", rc);
		return rc;
	}

	rc = roc_nix_rss_flowkey_set(roc_nix, &flowkey_algx, flowkey_cfg, rss_grp_idx,
				     flow->mcam_id);
	if (rc) {
		plt_err("Failed to set rss hash function rc = %d", rc);
		return rc;
	}

	plt_bitmap_set(npc->rss_grp_entries, rss_grp_idx);

	flow->npc_action &= (~(0xfULL));
	flow->npc_action |= NIX_RX_ACTIONOP_RSS;
	flow->npc_action |=
		((uint64_t)(flowkey_algx & NPC_RSS_ACT_ALG_MASK) << NPC_RSS_ACT_ALG_OFFSET) |
		((uint64_t)(rss_grp_idx & NPC_RSS_ACT_GRP_MASK) << NPC_RSS_ACT_GRP_OFFSET);
	return 0;
}

int
roc_eswitch_nix_vlan_tpid_set(struct roc_nix *roc_nix, uint32_t type, uint16_t tpid, bool is_vf)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	int rc;

	/* Configuring for PF/VF */
	rc = nix_vlan_tpid_set(dev->mbox, dev->pf_func | is_vf, type, tpid);
	if (rc)
		plt_err("Failed to set tpid for PF, rc %d", rc);

	return rc;
}

int
roc_eswitch_nix_process_repte_notify_cb_register(struct roc_nix *roc_nix,
						 process_repte_notify_t proc_repte_nt)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	if (proc_repte_nt == NULL)
		return NIX_ERR_PARAM;

	dev->ops->repte_notify = (repte_notify_t)proc_repte_nt;
	return 0;
}

void
roc_eswitch_nix_process_repte_notify_cb_unregister(struct roc_nix *roc_nix)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;

	dev->ops->repte_notify = NULL;
}

int
roc_eswitch_nix_repte_stats(struct roc_nix *roc_nix, uint16_t pf_func, struct roc_nix_stats *stats)
{
	struct nix *nix = roc_nix_to_nix_priv(roc_nix);
	struct dev *dev = &nix->dev;
	struct nix_get_lf_stats_req *req;
	struct nix_lf_stats_rsp *rsp;
	struct mbox *mbox;
	int rc;

	mbox = mbox_get(dev->mbox);
	req = mbox_alloc_msg_nix_get_lf_stats(mbox);
	if (!req) {
		rc = -ENOSPC;
		goto exit;
	}

	req->hdr.pcifunc = roc_nix_get_pf_func(roc_nix);
	req->pcifunc = pf_func;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	stats->rx_octs = rsp->rx.octs;
	stats->rx_ucast = rsp->rx.ucast;
	stats->rx_bcast = rsp->rx.bcast;
	stats->rx_mcast = rsp->rx.mcast;
	stats->rx_drop = rsp->rx.drop;
	stats->rx_drop_octs = rsp->rx.drop_octs;
	stats->rx_drop_bcast = rsp->rx.drop_bcast;
	stats->rx_drop_mcast = rsp->rx.drop_mcast;
	stats->rx_err = rsp->rx.err;

	stats->tx_ucast = rsp->tx.ucast;
	stats->tx_bcast = rsp->tx.bcast;
	stats->tx_mcast = rsp->tx.mcast;
	stats->tx_drop = rsp->tx.drop;
	stats->tx_octs = rsp->tx.octs;

exit:
	mbox_put(mbox);
	return rc;
}

int
roc_eswitch_is_repte_pfs_vf(uint16_t rep_pffunc, uint16_t pf_pffunc)
{
	uint16_t rep_pf = dev_get_pf(rep_pffunc);

	if (roc_model_is_cn20k())
		return ((rep_pf << RVU_PFVF_PF_SHIFT_CN20K) == pf_pffunc);
	else
		return ((rep_pf << RVU_PFVF_PF_SHIFT) == pf_pffunc);
}
