/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell.
 */
#include <cnxk_rte_flow.h>
#include "cn9k_ethdev.h"
#include "cn9k_rte_flow.h"

struct rte_flow *
cn9k_flow_create(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct roc_npc_flow *flow;

	flow = cnxk_flow_create(eth_dev, attr, pattern, actions, error);
	if (!flow)
		return NULL;

	return (struct rte_flow *)flow;
}

int
cn9k_flow_destroy(struct rte_eth_dev *eth_dev, struct rte_flow *rte_flow,
		  struct rte_flow_error *error)
{
	struct roc_npc_flow *flow = (struct roc_npc_flow *)rte_flow;

	return cnxk_flow_destroy(eth_dev, flow, error);
}
