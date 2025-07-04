/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef __ROC_ESWITCH_H__
#define __ROC_ESWITCH_H__

#define ROC_ESWITCH_VLAN_TPID 0x8100
#define ROC_ESWITCH_LBK_CHAN  63

typedef enum roc_eswitch_repte_notify_msg_type {
	ROC_ESWITCH_REPTE_STATE = 0,
	ROC_ESWITCH_LINK_STATE,
	ROC_ESWITCH_REPTE_MTU,
} roc_eswitch_repte_notify_msg_type_t;

struct roc_eswitch_repte_state {
	bool enable;
	uint16_t hw_func;
};

struct roc_eswitch_link_state {
	bool enable;
	uint16_t hw_func;
};

struct roc_eswitch_repte_mtu {
	uint16_t mtu;
	uint16_t rep_id;
	uint16_t hw_func;
};

struct roc_eswitch_repte_notify_msg {
	roc_eswitch_repte_notify_msg_type_t type;
	union {
		struct roc_eswitch_repte_state state;
		struct roc_eswitch_link_state link;
		struct roc_eswitch_repte_mtu mtu;
	};
};

/* Process representee notification callback */
typedef int (*process_repte_notify_t)(void *roc_nix,
				      struct roc_eswitch_repte_notify_msg *notify_msg);

/* Generic */
int __roc_api roc_eswitch_is_repte_pfs_vf(uint16_t rep_pffunc, uint16_t pf_pffunc);

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
int __roc_api roc_eswitch_nix_repte_stats(struct roc_nix *roc_nix, uint16_t pf_func,
					  struct roc_nix_stats *stats);
int __roc_api roc_eswitch_nix_process_repte_notify_cb_register(struct roc_nix *roc_nix,
						    process_repte_notify_t proc_repte_nt);
void __roc_api roc_eswitch_nix_process_repte_notify_cb_unregister(struct roc_nix *roc_nix);
#endif /* __ROC_ESWITCH_H__ */
