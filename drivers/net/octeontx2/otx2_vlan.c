/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_malloc.h>
#include <rte_tailq.h>

#include "otx2_ethdev.h"
#include "otx2_flow.h"


#define VLAN_ID_MATCH	0x1
#define VTAG_F_MATCH	0x2
#define MAC_ADDR_MATCH	0x4
#define QINQ_F_MATCH	0x8
#define VLAN_DROP	0x10
#define DEF_F_ENTRY	0x20

enum vtag_cfg_dir {
	VTAG_TX,
	VTAG_RX
};

static int
__rte_unused nix_vlan_mcam_enb_dis(struct otx2_eth_dev *dev,
				   uint32_t entry, const int enable)
{
	struct npc_mcam_ena_dis_entry_req *req;
	struct otx2_mbox *mbox = dev->mbox;
	int rc = -EINVAL;

	if (enable)
		req = otx2_mbox_alloc_msg_npc_mcam_ena_entry(mbox);
	else
		req = otx2_mbox_alloc_msg_npc_mcam_dis_entry(mbox);

	req->entry = entry;

	rc = otx2_mbox_process_msg(mbox, NULL);
	return rc;
}

static void
nix_set_rx_vlan_action(struct rte_eth_dev *eth_dev,
		    struct mcam_entry *entry, bool qinq, bool drop)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int pcifunc = otx2_pfvf_func(dev->pf, dev->vf);
	uint64_t action = 0, vtag_action = 0;

	action = NIX_RX_ACTIONOP_UCAST;

	if (eth_dev->data->dev_conf.rxmode.mq_mode == ETH_MQ_RX_RSS) {
		action = NIX_RX_ACTIONOP_RSS;
		action |= (uint64_t)(dev->rss_info.alg_idx) << 56;
	}

	action |= (uint64_t)pcifunc << 4;
	entry->action = action;

	if (drop) {
		entry->action &= ~((uint64_t)0xF);
		entry->action |= NIX_RX_ACTIONOP_DROP;
		return;
	}

	if (!qinq) {
		/* VTAG0 fields denote CTAG in single vlan case */
		vtag_action |= (NIX_RX_VTAGACTION_VTAG_VALID << 15);
		vtag_action |= (NPC_LID_LB << 8);
		vtag_action |= NIX_RX_VTAGACTION_VTAG0_RELPTR;
	} else {
		/* VTAG0 & VTAG1 fields denote CTAG & STAG respectively */
		vtag_action |= (NIX_RX_VTAGACTION_VTAG_VALID << 15);
		vtag_action |= (NPC_LID_LB << 8);
		vtag_action |= NIX_RX_VTAGACTION_VTAG1_RELPTR;
		vtag_action |= (NIX_RX_VTAGACTION_VTAG_VALID << 47);
		vtag_action |= ((uint64_t)(NPC_LID_LB) << 40);
		vtag_action |= (NIX_RX_VTAGACTION_VTAG0_RELPTR << 32);
	}

	entry->vtag_action = vtag_action;
}

static int
nix_vlan_mcam_free(struct otx2_eth_dev *dev, uint32_t entry)
{
	struct npc_mcam_free_entry_req *req;
	struct otx2_mbox *mbox = dev->mbox;
	int rc = -EINVAL;

	req = otx2_mbox_alloc_msg_npc_mcam_free_entry(mbox);
	req->entry = entry;

	rc = otx2_mbox_process_msg(mbox, NULL);
	return rc;
}

static int
nix_vlan_mcam_write(struct rte_eth_dev *eth_dev, uint16_t ent_idx,
		    struct mcam_entry *entry, uint8_t intf, uint8_t ena)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct npc_mcam_write_entry_req *req;
	struct otx2_mbox *mbox = dev->mbox;
	struct msghdr *rsp;
	int rc = -EINVAL;

	req = otx2_mbox_alloc_msg_npc_mcam_write_entry(mbox);

	req->entry = ent_idx;
	req->intf = intf;
	req->enable_entry = ena;
	memcpy(&req->entry_data, entry, sizeof(struct mcam_entry));

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	return rc;
}

static int
nix_vlan_mcam_alloc_and_write(struct rte_eth_dev *eth_dev,
			      struct mcam_entry *entry,
			      uint8_t intf, bool drop)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct npc_mcam_alloc_and_write_entry_req *req;
	struct npc_mcam_alloc_and_write_entry_rsp *rsp;
	struct otx2_mbox *mbox = dev->mbox;
	int rc = -EINVAL;

	req = otx2_mbox_alloc_msg_npc_mcam_alloc_and_write_entry(mbox);

	if (intf == NPC_MCAM_RX) {
		if (!drop && dev->vlan_info.def_rx_mcam_idx) {
			req->priority = NPC_MCAM_HIGHER_PRIO;
			req->ref_entry = dev->vlan_info.def_rx_mcam_idx;
		} else if (drop && dev->vlan_info.qinq_mcam_idx) {
			req->priority = NPC_MCAM_LOWER_PRIO;
			req->ref_entry = dev->vlan_info.qinq_mcam_idx;
		} else {
			req->priority = NPC_MCAM_ANY_PRIO;
			req->ref_entry = 0;
		}
	} else {
		req->priority = NPC_MCAM_ANY_PRIO;
		req->ref_entry = 0;
	}

	req->intf = intf;
	req->enable_entry = 1;
	memcpy(&req->entry_data, entry, sizeof(struct mcam_entry));

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		return rc;

	return rsp->entry;
}

static void
nix_vlan_update_mac(struct rte_eth_dev *eth_dev, int mcam_index,
			   int enable)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct vlan_mkex_info *mkex = &dev->vlan_info.mkex;
	volatile uint8_t *key_data, *key_mask;
	struct npc_mcam_read_entry_req *req;
	struct npc_mcam_read_entry_rsp *rsp;
	struct otx2_mbox *mbox = dev->mbox;
	uint64_t mcam_data, mcam_mask;
	struct mcam_entry entry;
	uint8_t intf, mcam_ena;
	int idx, rc = -EINVAL;
	uint8_t *mac_addr;

	memset(&entry, 0, sizeof(struct mcam_entry));

	/* Read entry first */
	req = otx2_mbox_alloc_msg_npc_mcam_read_entry(mbox);

	req->entry = mcam_index;

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	if (rc) {
		otx2_err("Failed to read entry %d", mcam_index);
		return;
	}

	entry = rsp->entry_data;
	intf = rsp->intf;
	mcam_ena = rsp->enable;

	/* Update mcam address */
	key_data = (volatile uint8_t *)entry.kw;
	key_mask = (volatile uint8_t *)entry.kw_mask;

	if (enable) {
		mcam_mask = 0;
		otx2_mbox_memcpy(key_mask + mkex->la_xtract.key_off,
				 &mcam_mask, mkex->la_xtract.len + 1);

	} else {
		mcam_data = 0ULL;
		mac_addr = dev->mac_addr;
		for (idx = RTE_ETHER_ADDR_LEN - 1; idx >= 0; idx--)
			mcam_data |= ((uint64_t)*mac_addr++) << (8 * idx);

		mcam_mask = BIT_ULL(48) - 1;

		otx2_mbox_memcpy(key_data + mkex->la_xtract.key_off,
				 &mcam_data, mkex->la_xtract.len + 1);
		otx2_mbox_memcpy(key_mask + mkex->la_xtract.key_off,
				 &mcam_mask, mkex->la_xtract.len + 1);
	}

	/* Write back the mcam entry */
	rc = nix_vlan_mcam_write(eth_dev, mcam_index,
				 &entry, intf, mcam_ena);
	if (rc) {
		otx2_err("Failed to write entry %d", mcam_index);
		return;
	}
}

void
otx2_nix_vlan_update_promisc(struct rte_eth_dev *eth_dev, int enable)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_vlan_info *vlan = &dev->vlan_info;
	struct vlan_entry *entry;

	/* Already in required mode */
	if (enable == vlan->promisc_on)
		return;

	/* Update default rx entry */
	if (vlan->def_rx_mcam_idx)
		nix_vlan_update_mac(eth_dev, vlan->def_rx_mcam_idx, enable);

	/* Update all other rx filter entries */
	TAILQ_FOREACH(entry, &vlan->fltr_tbl, next)
		nix_vlan_update_mac(eth_dev, entry->mcam_idx, enable);

	vlan->promisc_on = enable;
}

/* Configure mcam entry with required MCAM search rules */
static int
nix_vlan_mcam_config(struct rte_eth_dev *eth_dev,
		     uint16_t vlan_id, uint16_t flags)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct vlan_mkex_info *mkex = &dev->vlan_info.mkex;
	volatile uint8_t *key_data, *key_mask;
	uint64_t mcam_data, mcam_mask;
	struct mcam_entry entry;
	uint8_t *mac_addr;
	int idx, kwi = 0;

	memset(&entry, 0, sizeof(struct mcam_entry));
	key_data = (volatile uint8_t *)entry.kw;
	key_mask = (volatile uint8_t *)entry.kw_mask;

	/* Channel base extracted to KW0[11:0] */
	entry.kw[kwi] = dev->rx_chan_base;
	entry.kw_mask[kwi] = BIT_ULL(12) - 1;

	/* Adds vlan_id & LB CTAG flag to MCAM KW */
	if (flags & VLAN_ID_MATCH) {
		entry.kw[kwi] |= NPC_LT_LB_CTAG << mkex->lb_lt_offset;
		entry.kw_mask[kwi] |= 0xFULL << mkex->lb_lt_offset;

		mcam_data = (vlan_id << 16);
		mcam_mask = (BIT_ULL(16) - 1) << 16;
		otx2_mbox_memcpy(key_data + mkex->lb_xtract.key_off,
				     &mcam_data, mkex->lb_xtract.len + 1);
		otx2_mbox_memcpy(key_mask + mkex->lb_xtract.key_off,
				     &mcam_mask, mkex->lb_xtract.len + 1);
	}

	/* Adds LB STAG flag to MCAM KW */
	if (flags & QINQ_F_MATCH) {
		entry.kw[kwi] |= NPC_LT_LB_STAG << mkex->lb_lt_offset;
		entry.kw_mask[kwi] |= 0xFULL << mkex->lb_lt_offset;
	}

	/* Adds LB CTAG & LB STAG flags to MCAM KW */
	if (flags & VTAG_F_MATCH) {
		entry.kw[kwi] |= (NPC_LT_LB_CTAG | NPC_LT_LB_STAG)
							<< mkex->lb_lt_offset;
		entry.kw_mask[kwi] |= (NPC_LT_LB_CTAG & NPC_LT_LB_STAG)
							<< mkex->lb_lt_offset;
	}

	/* Adds port MAC address to MCAM KW */
	if (flags & MAC_ADDR_MATCH) {
		mcam_data = 0ULL;
		mac_addr = dev->mac_addr;
		for (idx = RTE_ETHER_ADDR_LEN - 1; idx >= 0; idx--)
			mcam_data |= ((uint64_t)*mac_addr++) << (8 * idx);

		mcam_mask = BIT_ULL(48) - 1;
		otx2_mbox_memcpy(key_data + mkex->la_xtract.key_off,
				     &mcam_data, mkex->la_xtract.len + 1);
		otx2_mbox_memcpy(key_mask + mkex->la_xtract.key_off,
				     &mcam_mask, mkex->la_xtract.len + 1);
	}

	/* VLAN_DROP: for drop action for all vlan packets when filter is on.
	 * For QinQ, enable vtag action for both outer & inner tags
	 */
	if (flags & VLAN_DROP)
		nix_set_rx_vlan_action(eth_dev, &entry, false, true);
	else if (flags & QINQ_F_MATCH)
		nix_set_rx_vlan_action(eth_dev, &entry, true, false);
	else
		nix_set_rx_vlan_action(eth_dev, &entry, false, false);

	if (flags & DEF_F_ENTRY)
		dev->vlan_info.def_rx_mcam_ent = entry;

	return nix_vlan_mcam_alloc_and_write(eth_dev, &entry, NIX_INTF_RX,
					     flags & VLAN_DROP);
}

/* Installs/Removes/Modifies default rx entry */
static int
nix_vlan_handle_default_rx_entry(struct rte_eth_dev *eth_dev, bool strip,
				 bool filter, bool enable)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_vlan_info *vlan = &dev->vlan_info;
	uint16_t flags = 0;
	int mcam_idx, rc;

	/* Use default mcam entry to either drop vlan traffic when
	 * vlan filter is on or strip vtag when strip is enabled.
	 * Allocate default entry which matches port mac address
	 * and vtag(ctag/stag) flags with drop action.
	 */
	if (!vlan->def_rx_mcam_idx) {
		if (!eth_dev->data->promiscuous)
			flags = MAC_ADDR_MATCH;

		if (filter && enable)
			flags |= VTAG_F_MATCH | VLAN_DROP;
		else if (strip && enable)
			flags |= VTAG_F_MATCH;
		else
			return 0;

		flags |= DEF_F_ENTRY;

		mcam_idx = nix_vlan_mcam_config(eth_dev, 0, flags);
		if (mcam_idx < 0) {
			otx2_err("Failed to config vlan mcam");
			return -mcam_idx;
		}

		vlan->def_rx_mcam_idx = mcam_idx;
		return 0;
	}

	/* Filter is already enabled, so packets would be dropped anyways. No
	 * processing needed for enabling strip wrt mcam entry.
	 */

	/* Filter disable request */
	if (vlan->filter_on && filter && !enable) {
		vlan->def_rx_mcam_ent.action &= ~((uint64_t)0xF);

		/* Free default rx entry only when
		 * 1. strip is not on and
		 * 2. qinq entry is allocated before default entry.
		 */
		if (vlan->strip_on ||
		    (vlan->qinq_on && !vlan->qinq_before_def)) {
			if (eth_dev->data->dev_conf.rxmode.mq_mode ==
								ETH_MQ_RX_RSS)
				vlan->def_rx_mcam_ent.action |=
							NIX_RX_ACTIONOP_RSS;
			else
				vlan->def_rx_mcam_ent.action |=
							NIX_RX_ACTIONOP_UCAST;
			return nix_vlan_mcam_write(eth_dev,
						   vlan->def_rx_mcam_idx,
						   &vlan->def_rx_mcam_ent,
						   NIX_INTF_RX, 1);
		} else {
			rc = nix_vlan_mcam_free(dev, vlan->def_rx_mcam_idx);
			if (rc)
				return rc;
			vlan->def_rx_mcam_idx = 0;
		}
	}

	/* Filter enable request */
	if (!vlan->filter_on && filter && enable) {
		vlan->def_rx_mcam_ent.action &= ~((uint64_t)0xF);
		vlan->def_rx_mcam_ent.action |= NIX_RX_ACTIONOP_DROP;
		return nix_vlan_mcam_write(eth_dev, vlan->def_rx_mcam_idx,
				   &vlan->def_rx_mcam_ent, NIX_INTF_RX, 1);
	}

	/* Strip disable request */
	if (vlan->strip_on && strip && !enable) {
		if (!vlan->filter_on &&
		    !(vlan->qinq_on && !vlan->qinq_before_def)) {
			rc = nix_vlan_mcam_free(dev, vlan->def_rx_mcam_idx);
			if (rc)
				return rc;
			vlan->def_rx_mcam_idx = 0;
		}
	}

	return 0;
}

/* Configure vlan stripping on or off */
static int
nix_vlan_hw_strip(struct rte_eth_dev *eth_dev, const uint8_t enable)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct nix_vtag_config *vtag_cfg;
	int rc = -EINVAL;

	rc = nix_vlan_handle_default_rx_entry(eth_dev, true, false, enable);
	if (rc) {
		otx2_err("Failed to config default rx entry");
		return rc;
	}

	vtag_cfg = otx2_mbox_alloc_msg_nix_vtag_cfg(mbox);
	/* cfg_type = 1 for rx vlan cfg */
	vtag_cfg->cfg_type = VTAG_RX;

	if (enable)
		vtag_cfg->rx.strip_vtag = 1;
	else
		vtag_cfg->rx.strip_vtag = 0;

	/* Always capture */
	vtag_cfg->rx.capture_vtag = 1;
	vtag_cfg->vtag_size = NIX_VTAGSIZE_T4;
	/* Use rx vtag type index[0] for now */
	vtag_cfg->rx.vtag_type = 0;

	rc = otx2_mbox_process(mbox);
	if (rc)
		return rc;

	dev->vlan_info.strip_on = enable;
	return rc;
}

/* Configure vlan filtering on or off for all vlans if vlan_id == 0 */
static int
nix_vlan_hw_filter(struct rte_eth_dev *eth_dev, const uint8_t enable,
		   uint16_t vlan_id)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc = -EINVAL;

	if (!vlan_id && enable) {
		rc = nix_vlan_handle_default_rx_entry(eth_dev, false, true,
						      enable);
		if (rc) {
			otx2_err("Failed to config vlan mcam");
			return rc;
		}
		dev->vlan_info.filter_on = enable;
		return 0;
	}

	if (!vlan_id && !enable) {
		rc = nix_vlan_handle_default_rx_entry(eth_dev, false, true,
						      enable);
		if (rc) {
			otx2_err("Failed to config vlan mcam");
			return rc;
		}
		dev->vlan_info.filter_on = enable;
		return 0;
	}

	return 0;
}

/* Configure double vlan(qinq) on or off */
static int
otx2_nix_config_double_vlan(struct rte_eth_dev *eth_dev,
			    const uint8_t enable)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_vlan_info *vlan_info;
	int mcam_idx;
	int rc;

	vlan_info = &dev->vlan_info;

	if (!enable) {
		if (!vlan_info->qinq_mcam_idx)
			return 0;

		rc = nix_vlan_mcam_free(dev, vlan_info->qinq_mcam_idx);
		if (rc)
			return rc;

		vlan_info->qinq_mcam_idx = 0;
		dev->vlan_info.qinq_on = 0;
		vlan_info->qinq_before_def = 0;
		return 0;
	}

	if (eth_dev->data->promiscuous)
		mcam_idx = nix_vlan_mcam_config(eth_dev, 0, QINQ_F_MATCH);
	else
		mcam_idx = nix_vlan_mcam_config(eth_dev, 0,
						QINQ_F_MATCH | MAC_ADDR_MATCH);
	if (mcam_idx < 0)
		return mcam_idx;

	if (!vlan_info->def_rx_mcam_idx)
		vlan_info->qinq_before_def = 1;

	vlan_info->qinq_mcam_idx = mcam_idx;
	dev->vlan_info.qinq_on = 1;
	return 0;
}

int
otx2_nix_vlan_offload_set(struct rte_eth_dev *eth_dev, int mask)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint64_t offloads = dev->rx_offloads;
	struct rte_eth_rxmode *rxmode;
	int rc;

	rxmode = &eth_dev->data->dev_conf.rxmode;

	if (mask & ETH_VLAN_EXTEND_MASK) {
		otx2_err("Extend offload not supported");
		return -ENOTSUP;
	}

	if (mask & ETH_VLAN_STRIP_MASK) {
		if (rxmode->offloads & DEV_RX_OFFLOAD_VLAN_STRIP) {
			offloads |= DEV_RX_OFFLOAD_VLAN_STRIP;
			rc = nix_vlan_hw_strip(eth_dev, true);
		} else {
			offloads &= ~DEV_RX_OFFLOAD_VLAN_STRIP;
			rc = nix_vlan_hw_strip(eth_dev, false);
		}
		if (rc)
			goto done;
	}

	if (mask & ETH_VLAN_FILTER_MASK) {
		if (rxmode->offloads & DEV_RX_OFFLOAD_VLAN_FILTER) {
			offloads |= DEV_RX_OFFLOAD_VLAN_FILTER;
			rc = nix_vlan_hw_filter(eth_dev, true, 0);
		} else {
			offloads &= ~DEV_RX_OFFLOAD_VLAN_FILTER;
			rc = nix_vlan_hw_filter(eth_dev, false, 0);
		}
		if (rc)
			goto done;
	}

	if (rxmode->offloads & DEV_RX_OFFLOAD_QINQ_STRIP) {
		if (!dev->vlan_info.qinq_on) {
			offloads |= DEV_RX_OFFLOAD_QINQ_STRIP;
			rc = otx2_nix_config_double_vlan(eth_dev, true);
			if (rc)
				goto done;
		}
	} else {
		if (dev->vlan_info.qinq_on) {
			offloads &= ~DEV_RX_OFFLOAD_QINQ_STRIP;
			rc = otx2_nix_config_double_vlan(eth_dev, false);
			if (rc)
				goto done;
		}
	}

	if (offloads & (DEV_RX_OFFLOAD_VLAN_STRIP |
			DEV_RX_OFFLOAD_QINQ_STRIP)) {
		dev->rx_offloads |= offloads;
		dev->rx_offload_flags |= NIX_RX_OFFLOAD_VLAN_STRIP_F;
	}

done:
	return rc;
}

static int
nix_vlan_rx_mkex_offset(uint64_t mask)
{
	int nib_count = 0;

	while (mask) {
		nib_count += mask & 1;
		mask >>= 1;
	}

	return nib_count * 4;
}

static int
nix_vlan_get_mkex_info(struct otx2_eth_dev *dev)
{
	struct vlan_mkex_info *mkex = &dev->vlan_info.mkex;
	struct otx2_npc_flow_info *npc = &dev->npc_flow;
	struct npc_xtract_info *x_info = NULL;
	uint64_t rx_keyx;
	otx2_dxcfg_t *p;
	int rc = -EINVAL;

	if (npc == NULL) {
		otx2_err("Missing npc mkex configuration");
		return rc;
	}

#define NPC_KEX_CHAN_NIBBLE_ENA			0x7ULL
#define NPC_KEX_LB_LTYPE_NIBBLE_ENA		0x1000ULL
#define NPC_KEX_LB_LTYPE_NIBBLE_MASK		0xFFFULL

	rx_keyx = npc->keyx_supp_nmask[NPC_MCAM_RX];
	if ((rx_keyx & NPC_KEX_CHAN_NIBBLE_ENA) != NPC_KEX_CHAN_NIBBLE_ENA)
		return rc;

	if ((rx_keyx & NPC_KEX_LB_LTYPE_NIBBLE_ENA) !=
	    NPC_KEX_LB_LTYPE_NIBBLE_ENA)
		return rc;

	mkex->lb_lt_offset =
	    nix_vlan_rx_mkex_offset(rx_keyx & NPC_KEX_LB_LTYPE_NIBBLE_MASK);

	p = &npc->prx_dxcfg;
	x_info = &(*p)[NPC_MCAM_RX][NPC_LID_LA][NPC_LT_LA_ETHER].xtract[0];
	memcpy(&mkex->la_xtract, x_info, sizeof(struct npc_xtract_info));
	x_info = &(*p)[NPC_MCAM_RX][NPC_LID_LB][NPC_LT_LB_CTAG].xtract[0];
	memcpy(&mkex->lb_xtract, x_info, sizeof(struct npc_xtract_info));

	return 0;
}

int
otx2_nix_vlan_offload_init(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	int rc, mask;

	/* Port initialized for first time or restarted */
	if (!dev->configured) {
		rc = nix_vlan_get_mkex_info(dev);
		if (rc) {
			otx2_err("Failed to get vlan mkex info rc=%d", rc);
			return rc;
		}

		TAILQ_INIT(&dev->vlan_info.fltr_tbl);
	}

	mask =
	    ETH_VLAN_STRIP_MASK | ETH_VLAN_FILTER_MASK;
	rc = otx2_nix_vlan_offload_set(eth_dev, mask);
	if (rc) {
		otx2_err("Failed to set vlan offload rc=%d", rc);
		return rc;
	}

	return 0;
}

int
otx2_nix_vlan_fini(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_vlan_info *vlan = &dev->vlan_info;
	int rc;

	if (!dev->configured) {
		if (vlan->def_rx_mcam_idx) {
			rc = nix_vlan_mcam_free(dev, vlan->def_rx_mcam_idx);
			if (rc)
				return rc;
		}
	}

	otx2_nix_config_double_vlan(eth_dev, false);
	vlan->def_rx_mcam_idx = 0;
	return 0;
}
