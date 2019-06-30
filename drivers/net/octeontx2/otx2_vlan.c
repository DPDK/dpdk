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

static int
__rte_unused nix_vlan_mcam_free(struct otx2_eth_dev *dev, uint32_t entry)
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
__rte_unused nix_vlan_mcam_write(struct rte_eth_dev *eth_dev, uint16_t ent_idx,
				 struct mcam_entry *entry, uint8_t intf)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct npc_mcam_write_entry_req *req;
	struct otx2_mbox *mbox = dev->mbox;
	struct msghdr *rsp;
	int rc = -EINVAL;

	req = otx2_mbox_alloc_msg_npc_mcam_write_entry(mbox);

	req->entry = ent_idx;
	req->intf = intf;
	req->enable_entry = 1;
	memcpy(&req->entry_data, entry, sizeof(struct mcam_entry));

	rc = otx2_mbox_process_msg(mbox, (void *)&rsp);
	return rc;
}

static int
__rte_unused nix_vlan_mcam_alloc_and_write(struct rte_eth_dev *eth_dev,
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
	int rc;

	/* Port initialized for first time or restarted */
	if (!dev->configured) {
		rc = nix_vlan_get_mkex_info(dev);
		if (rc) {
			otx2_err("Failed to get vlan mkex info rc=%d", rc);
			return rc;
		}
	}
	return 0;
}

int
otx2_nix_vlan_fini(__rte_unused struct rte_eth_dev *eth_dev)
{
	return 0;
}
