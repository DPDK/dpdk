/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include "cn20k_ethdev.h"
#include "cn20k_rx.h"
#include "cnxk_ethdev_mcs.h"
#include "cnxk_flow_common.h"
#include <cn20k_flow.h>
#include <cnxk_flow.h>

struct rte_flow *
cn20k_flow_create(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[], const struct rte_flow_action actions[],
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
		cn20k_eth_set_rx_function(eth_dev);
	}

	vtag_actions = roc_npc_vtag_actions_get(npc);

	if (vtag_actions) {
		dev->rx_offload_flags |= NIX_RX_OFFLOAD_VLAN_STRIP_F;
		cn20k_eth_set_rx_function(eth_dev);
	}

	return (struct rte_flow *)flow;
}

int
cn20k_flow_info_get(struct rte_eth_dev *dev, struct rte_flow_port_info *port_info,
		    struct rte_flow_queue_info *queue_info, struct rte_flow_error *err)
{
	return cnxk_flow_info_get_common(dev, port_info, queue_info, err);
}

int
cn20k_flow_destroy(struct rte_eth_dev *eth_dev, struct rte_flow *rte_flow,
		   struct rte_flow_error *error)
{
	struct roc_npc_flow *flow = (struct roc_npc_flow *)rte_flow;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	int vtag_actions = 0, rc = 0;
	int mark_actions;
	uint16_t match_id;
	uint32_t mtr_id;

	match_id = (flow->npc_action >> NPC_RX_ACT_MATCH_OFFSET) & NPC_RX_ACT_MATCH_MASK;
	if (match_id) {
		mark_actions = roc_npc_mark_actions_sub_return(npc, 1);
		if (mark_actions == 0) {
			dev->rx_offload_flags &= ~NIX_RX_OFFLOAD_MARK_UPDATE_F;
			cn20k_eth_set_rx_function(eth_dev);
		}
	}

	vtag_actions = roc_npc_vtag_actions_get(npc);
	if (vtag_actions) {
		if (flow->nix_intf == ROC_NPC_INTF_RX) {
			vtag_actions = roc_npc_vtag_actions_sub_return(npc, 1);
			if (vtag_actions == 0) {
				dev->rx_offload_flags &= ~NIX_RX_OFFLOAD_VLAN_STRIP_F;
				cn20k_eth_set_rx_function(eth_dev);
			}
		}
	}

	if (cnxk_eth_macsec_sess_get_by_sess(dev, (void *)flow) != NULL) {
		rc = cnxk_mcs_flow_destroy(dev, (void *)flow);
		if (rc < 0)
			rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					   "Failed to free mcs flow");
		return rc;
	}

	mtr_id = flow->mtr_id;
	rc = cnxk_flow_destroy(eth_dev, flow, error);
	if (!rc && mtr_id != ROC_NIX_MTR_ID_INVALID) {
		rc = cnxk_mtr_destroy(eth_dev, mtr_id);
		if (rc) {
			rte_flow_error_set(error, ENXIO, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					   "Meter attached to this flow does not exist");
		}
	}
	return rc;
}
