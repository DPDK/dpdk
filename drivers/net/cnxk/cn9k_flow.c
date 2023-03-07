/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell.
 */
#include <cnxk_flow.h>
#include "cn9k_ethdev.h"
#include "cn9k_flow.h"
#include "cn9k_rx.h"

struct rte_flow *
cn9k_flow_create(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	struct roc_npc_flow *flow;
	int vtag_actions = 0;
	int mark_actions;

	flow = cnxk_flow_create(eth_dev, attr, pattern, actions, error);
	if (!flow)
		return NULL;

	mark_actions = roc_npc_mark_actions_get(npc);

	if (mark_actions) {
		dev->rx_offload_flags |= NIX_RX_OFFLOAD_MARK_UPDATE_F;
		cn9k_eth_set_rx_function(eth_dev);
	}

	vtag_actions = roc_npc_vtag_actions_get(npc);

	if (vtag_actions) {
		dev->rx_offload_flags |= NIX_RX_OFFLOAD_VLAN_STRIP_F;
		cn9k_eth_set_rx_function(eth_dev);
	}

	return (struct rte_flow *)flow;
}

int
cn9k_flow_destroy(struct rte_eth_dev *eth_dev, struct rte_flow *rte_flow,
		  struct rte_flow_error *error)
{
	struct roc_npc_flow *flow = (struct roc_npc_flow *)rte_flow;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	int vtag_actions = 0;
	uint16_t match_id;
	int mark_actions;

	match_id = (flow->npc_action >> NPC_RX_ACT_MATCH_OFFSET) &
		   NPC_RX_ACT_MATCH_MASK;
	if (match_id) {
		mark_actions = roc_npc_mark_actions_sub_return(npc, 1);
		if (mark_actions == 0) {
			dev->rx_offload_flags &= ~NIX_RX_OFFLOAD_MARK_UPDATE_F;
			cn9k_eth_set_rx_function(eth_dev);
		}
	}

	vtag_actions = roc_npc_vtag_actions_get(npc);
	if (vtag_actions) {
		if (flow->nix_intf == ROC_NPC_INTF_RX) {
			vtag_actions = roc_npc_vtag_actions_sub_return(npc, 1);
			if (vtag_actions == 0) {
				dev->rx_offload_flags &=
					~NIX_RX_OFFLOAD_VLAN_STRIP_F;
				cn9k_eth_set_rx_function(eth_dev);
			}
		}
	}

	return cnxk_flow_destroy(eth_dev, flow, error);
}

int
cn9k_flow_info_get(struct rte_eth_dev *dev, struct rte_flow_port_info *port_info,
		   struct rte_flow_queue_info *queue_info, struct rte_flow_error *err)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(err);

	memset(port_info, 0, sizeof(*port_info));
	memset(queue_info, 0, sizeof(*queue_info));

	port_info->max_nb_counters = CN9K_NPC_COUNTERS_MAX;

	return 0;
}
