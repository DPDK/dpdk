/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include "cnxk_flow_common.h"
#include "cnxk_ethdev_mcs.h"
#include <cnxk_flow.h>

static int
cnxk_mtr_connect(struct rte_eth_dev *eth_dev, uint32_t mtr_id)
{
	return nix_mtr_connect(eth_dev, mtr_id);
}

int
cnxk_mtr_destroy(struct rte_eth_dev *eth_dev, uint32_t mtr_id)
{
	struct rte_mtr_error mtr_error;

	return nix_mtr_destroy(eth_dev, mtr_id, &mtr_error);
}

static int
cnxk_mtr_configure(struct rte_eth_dev *eth_dev, const struct rte_flow_action actions[])
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
			mtr_conf = (const struct rte_flow_action_meter *)(actions[i].conf);
			mtr_id = mtr_conf->mtr_id;
			is_mtr_act = true;
		}
		if (actions[i].type == RTE_FLOW_ACTION_TYPE_QUEUE) {
			q_conf = (const struct rte_flow_action_queue *)(actions[i].conf);
			if (is_mtr_act)
				nix_mtr_rq_update(eth_dev, mtr_id, 1, &q_conf->index);
		}
		if (actions[i].type == RTE_FLOW_ACTION_TYPE_RSS) {
			rss_conf = (const struct rte_flow_action_rss *)(actions[i].conf);
			if (is_mtr_act)
				nix_mtr_rq_update(eth_dev, mtr_id, rss_conf->queue_num,
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

		rc = nix_mtr_color_action_validate(eth_dev, mtr_id, &prev_mtr_id, &next_mtr_id,
						   policy, &tree_level);
		if (rc)
			return rc;
	}

	return nix_mtr_configure(eth_dev, mtr_id);
}

static int
cnxk_rss_action_validate(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
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

struct roc_npc_flow *
cnxk_flow_create(struct rte_eth_dev *eth_dev, const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[], const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);
	const struct rte_flow_action *action_rss = NULL;
	const struct rte_flow_action_meter *mtr = NULL;
	const struct rte_flow_action *act_q = NULL;
	struct roc_npc_flow *flow;
	void *mcs_flow = NULL;
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
			rc = cnxk_rss_action_validate(eth_dev, attr, action_rss);
			if (rc)
				return NULL;
		} else if (req_act & ROC_NPC_ACTION_TYPE_QUEUE) {
			const struct rte_flow_action_queue *act_queue;
			act_queue = (const struct rte_flow_action_queue *)act_q->conf;
			if (act_queue->index > eth_dev->data->nb_rx_queues)
				return NULL;
		} else {
			return NULL;
		}
	}
	for (i = 0; actions[i].type != RTE_FLOW_ACTION_TYPE_END; i++) {
		if (actions[i].type == RTE_FLOW_ACTION_TYPE_METER) {
			mtr = (const struct rte_flow_action_meter *)actions[i].conf;
			rc = cnxk_mtr_configure(eth_dev, actions);
			if (rc) {
				rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
						   "Failed to configure mtr ");
				return NULL;
			}
			break;
		}
	}

	if (actions[0].type == RTE_FLOW_ACTION_TYPE_SECURITY &&
	    cnxk_eth_macsec_sess_get_by_sess(dev, actions[0].conf) != NULL) {
		rc = cnxk_mcs_flow_configure(eth_dev, attr, pattern, actions, error, &mcs_flow);
		if (rc) {
			rte_flow_error_set(error, rc, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					   "Failed to configure mcs flow");
			return NULL;
		}
		return mcs_flow;
	}

	flow = cnxk_flow_create_common(eth_dev, attr, pattern, actions, error, false);
	if (!flow) {
		if (mtr)
			nix_mtr_chain_reset(eth_dev, mtr->mtr_id);

	} else {
		if (mtr)
			cnxk_mtr_connect(eth_dev, mtr->mtr_id);
	}

	return flow;
}

int
cnxk_flow_info_get_common(struct rte_eth_dev *dev, struct rte_flow_port_info *port_info,
			  struct rte_flow_queue_info *queue_info, struct rte_flow_error *err)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(err);

	memset(port_info, 0, sizeof(*port_info));
	memset(queue_info, 0, sizeof(*queue_info));

	port_info->max_nb_counters = CNXK_NPC_COUNTERS_MAX;
	port_info->max_nb_meters = CNXK_NIX_MTR_COUNT_MAX;

	return 0;
}
