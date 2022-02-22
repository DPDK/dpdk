/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell.
 */
#include <cnxk_flow.h>
#include "cn10k_flow.h"
#include "cn10k_ethdev.h"
#include "cn10k_rx.h"

static int
cn10k_mtr_connect(struct rte_eth_dev *eth_dev, uint32_t mtr_id)
{
	return nix_mtr_connect(eth_dev, mtr_id);
}

static int
cn10k_mtr_destroy(struct rte_eth_dev *eth_dev, uint32_t mtr_id)
{
	struct rte_mtr_error mtr_error;

	return nix_mtr_destroy(eth_dev, mtr_id, &mtr_error);
}

static int
cn10k_mtr_configure(struct rte_eth_dev *eth_dev,
		    const struct rte_flow_action actions[])
{
	uint32_t mtr_id = 0xffff, prev_mtr_id = 0xffff, next_mtr_id = 0xffff;
	const struct rte_flow_action_meter *mtr_conf;
	const struct rte_flow_action_queue *q_conf;
	const struct rte_flow_action_rss *rss_conf;
	struct cnxk_mtr_policy_node *policy;
	bool is_mtr_act = false;
	int tree_level = 0;
	int rc = -EINVAL, i;

	for (i = 0; actions[i].type != RTE_FLOW_ACTION_TYPE_END; i++) {
		if (actions[i].type == RTE_FLOW_ACTION_TYPE_METER) {
			mtr_conf = (const struct rte_flow_action_meter
					    *)(actions[i].conf);
			mtr_id = mtr_conf->mtr_id;
			is_mtr_act = true;
		}
		if (actions[i].type == RTE_FLOW_ACTION_TYPE_QUEUE) {
			q_conf = (const struct rte_flow_action_queue
					  *)(actions[i].conf);
			if (is_mtr_act)
				nix_mtr_rq_update(eth_dev, mtr_id, 1,
						  &q_conf->index);
		}
		if (actions[i].type == RTE_FLOW_ACTION_TYPE_RSS) {
			rss_conf = (const struct rte_flow_action_rss
					    *)(actions[i].conf);
			if (is_mtr_act)
				nix_mtr_rq_update(eth_dev, mtr_id,
						  rss_conf->queue_num,
						  rss_conf->queue);
		}
	}

	if (!is_mtr_act)
		return rc;

	prev_mtr_id = mtr_id;
	next_mtr_id = mtr_id;
	while (next_mtr_id != 0xffff) {
		rc = nix_mtr_validate(eth_dev, next_mtr_id);
		if (rc)
			return rc;

		rc = nix_mtr_policy_act_get(eth_dev, next_mtr_id, &policy);
		if (rc)
			return rc;

		rc = nix_mtr_color_action_validate(eth_dev, mtr_id,
						   &prev_mtr_id, &next_mtr_id,
						   policy, &tree_level);
		if (rc)
			return rc;
	}

	return nix_mtr_configure(eth_dev, mtr_id);
}

static int
cn10k_rss_action_validate(struct rte_eth_dev *eth_dev,
			  const struct rte_flow_attr *attr,
			  const struct rte_flow_action *act)
{
	const struct rte_flow_action_rss *rss;

	if (act == NULL)
		return -EINVAL;

	rss = (const struct rte_flow_action_rss *)act->conf;

	if (attr->egress) {
		plt_err("No support of RSS in egress");
		return -EINVAL;
	}

	if (eth_dev->data->dev_conf.rxmode.mq_mode != RTE_ETH_MQ_RX_RSS) {
		plt_err("multi-queue mode is disabled");
		return -ENOTSUP;
	}

	if (!rss || !rss->queue_num) {
		plt_err("no valid queues");
		return -EINVAL;
	}

	if (rss->func != RTE_ETH_HASH_FUNCTION_DEFAULT) {
		plt_err("non-default RSS hash functions are not supported");
		return -ENOTSUP;
	}

	if (rss->key_len && rss->key_len > ROC_NIX_RSS_KEY_LEN) {
		plt_err("RSS hash key too large");
		return -ENOTSUP;
	}

	return 0;
}

struct rte_flow *
cn10k_flow_create(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct rte_flow_action *action_rss = NULL;
	const struct rte_flow_action_meter *mtr = NULL;
	const struct rte_flow_action *act_q = NULL;
	struct roc_npc *npc = &dev->npc;
	struct roc_npc_flow *flow;
	int vtag_actions = 0;
	uint32_t req_act = 0;
	int i, rc;

	for (i = 0; actions[i].type != RTE_FLOW_ACTION_TYPE_END; i++) {
		if (actions[i].type == RTE_FLOW_ACTION_TYPE_METER)
			req_act |= ROC_NPC_ACTION_TYPE_METER;

		if (actions[i].type == RTE_FLOW_ACTION_TYPE_QUEUE) {
			req_act |= ROC_NPC_ACTION_TYPE_QUEUE;
			act_q = &actions[i];
		}
		if (actions[i].type == RTE_FLOW_ACTION_TYPE_RSS) {
			req_act |= ROC_NPC_ACTION_TYPE_RSS;
			action_rss = &actions[i];
		}
	}

	if (req_act & ROC_NPC_ACTION_TYPE_METER) {
		if ((req_act & ROC_NPC_ACTION_TYPE_RSS) &&
		    ((req_act & ROC_NPC_ACTION_TYPE_QUEUE))) {
			return NULL;
		}
		if (req_act & ROC_NPC_ACTION_TYPE_RSS) {
			rc = cn10k_rss_action_validate(eth_dev, attr,
						       action_rss);
			if (rc)
				return NULL;
		} else if (req_act & ROC_NPC_ACTION_TYPE_QUEUE) {
			const struct rte_flow_action_queue *act_queue;
			act_queue = (const struct rte_flow_action_queue *)
					    act_q->conf;
			if (act_queue->index > eth_dev->data->nb_rx_queues)
				return NULL;
		} else {
			return NULL;
		}
	}
	for (i = 0; actions[i].type != RTE_FLOW_ACTION_TYPE_END; i++) {
		if (actions[i].type == RTE_FLOW_ACTION_TYPE_METER) {
			mtr = (const struct rte_flow_action_meter *)actions[i]
				      .conf;
			rc = cn10k_mtr_configure(eth_dev, actions);
			if (rc) {
				rte_flow_error_set(error, rc,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"Failed to configure mtr ");
				return NULL;
			}
			break;
		}
	}

	flow = cnxk_flow_create(eth_dev, attr, pattern, actions, error);
	if (!flow) {
		if (mtr)
			nix_mtr_chain_reset(eth_dev, mtr->mtr_id);

		return NULL;
	} else {
		if (mtr)
			cn10k_mtr_connect(eth_dev, mtr->mtr_id);
	}

	vtag_actions = roc_npc_vtag_actions_get(npc);

	if (vtag_actions) {
		dev->rx_offload_flags |= NIX_RX_OFFLOAD_VLAN_STRIP_F;
		cn10k_eth_set_rx_function(eth_dev);
	}

	return (struct rte_flow *)flow;
}

int
cn10k_flow_destroy(struct rte_eth_dev *eth_dev, struct rte_flow *rte_flow,
		   struct rte_flow_error *error)
{
	struct roc_npc_flow *flow = (struct roc_npc_flow *)rte_flow;
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	struct roc_npc *npc = &dev->npc;
	int vtag_actions = 0;
	uint32_t mtr_id;
	int rc;

	vtag_actions = roc_npc_vtag_actions_get(npc);
	if (vtag_actions) {
		if (flow->nix_intf == ROC_NPC_INTF_RX) {
			vtag_actions = roc_npc_vtag_actions_sub_return(npc, 1);
			if (vtag_actions == 0) {
				dev->rx_offload_flags &=
					~NIX_RX_OFFLOAD_VLAN_STRIP_F;
				cn10k_eth_set_rx_function(eth_dev);
			}
		}
	}

	mtr_id = flow->mtr_id;
	rc = cnxk_flow_destroy(eth_dev, flow, error);
	if (!rc && mtr_id != ROC_NIX_MTR_ID_INVALID) {
		rc = cn10k_mtr_destroy(eth_dev, mtr_id);
		if (rc) {
			rte_flow_error_set(error, ENXIO,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Meter attached to this flow does not exist");
		}
	}
	return rc;
}
