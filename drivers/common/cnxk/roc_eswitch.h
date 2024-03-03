/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __ROC_ESWITCH_H__
#define __ROC_ESWITCH_H__

#define ROC_ESWITCH_VLAN_TPID 0x8100
#define ROC_ESWITCH_LBK_CHAN  63

/* NPC */
int __roc_api roc_eswitch_npc_mcam_rx_rule(struct roc_npc *roc_npc, struct roc_npc_flow *flow,
					   uint16_t pcifunc, uint16_t vlan_tci,
					   uint16_t vlan_tci_mask);
int __roc_api roc_eswitch_npc_mcam_tx_rule(struct roc_npc *roc_npc, struct roc_npc_flow *flow,
					   uint16_t pcifunc, uint32_t vlan_tci);
int __roc_api roc_eswitch_npc_mcam_delete_rule(struct roc_npc *roc_npc, struct roc_npc_flow *flow,
					       uint16_t pcifunc);
int __roc_api roc_eswitch_npc_rss_action_configure(struct roc_npc *roc_npc,
						   struct roc_npc_flow *flow, uint32_t flowkey_cfg,
						   uint16_t *reta_tbl);

/* NIX */
int __roc_api roc_eswitch_nix_vlan_tpid_set(struct roc_nix *nix, uint32_t type, uint16_t tpid,
					    bool is_vf);
#endif /* __ROC_ESWITCH_H__ */
