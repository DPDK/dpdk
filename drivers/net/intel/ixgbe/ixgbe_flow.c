/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_tailq.h>
#include <ethdev_driver.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <dev_driver.h>
#include <rte_hash_crc.h>
#include <rte_flow.h>
#include <rte_hexdump.h>
#include <rte_flow_driver.h>

#include "ixgbe_logs.h"
#include "base/ixgbe_api.h"
#include "base/ixgbe_vf.h"
#include "base/ixgbe_common.h"
#include "ixgbe_osdep.h"
#include "ixgbe_ethdev.h"
#include "ixgbe_bypass.h"
#include "ixgbe_rxtx.h"
#include "base/ixgbe_type.h"
#include "base/ixgbe_phy.h"
#include "rte_pmd_ixgbe.h"

#include "../common/flow_check.h"
#include "../common/flow_engine.h"
#include "ixgbe_flow.h"

struct ixgbe_filter_ele_base {
	TAILQ_ENTRY(ixgbe_filter_ele_base) entries;
};

/* rss filter list structure */
struct ixgbe_rss_conf_ele {
	struct ixgbe_filter_ele_base base;
	struct ixgbe_rte_flow_rss_conf filter_info;
};
/* ixgbe_flow memory list structure */
struct ixgbe_flow_mem {
	struct ixgbe_filter_ele_base base;
	struct rte_flow *flow;
};

const struct ci_flow_engine_list ixgbe_flow_engine_list = {
	{
		&ixgbe_ethertype_flow_engine,
		&ixgbe_syn_flow_engine,
		&ixgbe_l2_tunnel_flow_engine,
		&ixgbe_ntuple_flow_engine,
		&ixgbe_security_flow_engine,
		&ixgbe_fdir_flow_engine,
		&ixgbe_fdir_tunnel_flow_engine,
	},
};
/*
 * All ixgbe engines mostly check the same stuff, so use a common check.
 */
int
ixgbe_flow_actions_check(const struct ci_flow_actions *actions,
		const struct ci_flow_actions_check_param *param,
		struct rte_flow_error *error)
{
	const struct rte_flow_action *action;
	struct rte_eth_dev_data *dev_data = param->driver_ctx;
	struct ixgbe_adapter *ad = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev_data->dev_private);
	size_t idx;

	for (idx = 0; idx < actions->count; idx++) {
		action = actions->actions[idx];

		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
		{
			const struct rte_flow_action_queue *queue = action->conf;
			if (queue->index >= dev_data->nb_rx_queues) {
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						action,
						"queue index out of range");
			}
			break;
		}
		case RTE_FLOW_ACTION_TYPE_VF:
		{
			const struct rte_flow_action_vf *vf = action->conf;
			if (vf->id >= ad->max_vfs) {
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						action,
						"VF id out of range");
			}
			break;
		}
		default:
			/* no specific validation */
			break;
		}
	}
	return 0;
}

/**
 * Please be aware there's an assumption for all the parsers.
 * rte_flow_item is using big endian, rte_flow_attr and
 * rte_flow_action are using CPU order.
 * Because the pattern is used to describe the packets,
 * normally the packets should use network order.
 */

/* Flow actions check specific to RSS filter */
static int
ixgbe_flow_actions_check_rss(const struct ci_flow_actions *parsed_actions,
		const struct ci_flow_actions_check_param *param,
		struct rte_flow_error *error)
{
	const struct rte_flow_action *action = parsed_actions->actions[0];
	const struct rte_flow_action_rss *rss_act = action->conf;
	struct rte_eth_dev_data *dev_data = param->driver_ctx;
	const size_t rss_key_len = sizeof(((struct ixgbe_rte_flow_rss_conf *)0)->key);
	size_t q_idx, q;

	/* check if queue list is not empty */
	if (rss_act->queue_num == 0) {
		return rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
			"RSS queue list is empty");
	}

	/* check if each RSS queue is valid */
	for (q_idx = 0; q_idx < rss_act->queue_num; q_idx++) {
		q = rss_act->queue[q_idx];
		if (q >= dev_data->nb_rx_queues) {
			return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
				"Invalid RSS queue specified");
		}
	}

	/* only support default hash function */
	if (rss_act->func != RTE_ETH_HASH_FUNCTION_DEFAULT) {
		return rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
			"Non-default RSS hash functions are not supported");
	}
	/* levels aren't supported */
	if (rss_act->level) {
		return rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
			"A nonzero RSS encapsulation level is not supported");
	}
	/* check key length */
	if (rss_act->key_len != 0 && rss_act->key_len != rss_key_len) {
		return rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ACTION_CONF, rss_act,
			"RSS key must be exactly 40 bytes long");
	}
	return 0;
}

static int
ixgbe_parse_rss_filter(struct rte_eth_dev *dev,
			const struct rte_flow_attr *attr,
			const struct rte_flow_action actions[],
			struct ixgbe_rte_flow_rss_conf *rss_conf,
			struct rte_flow_error *error)
{
	struct ci_flow_actions parsed_actions;
	struct ci_flow_actions_check_param ap_param = {
		.allowed_types = (const enum rte_flow_action_type[]){
			/* only rss allowed here */
			RTE_FLOW_ACTION_TYPE_RSS,
			RTE_FLOW_ACTION_TYPE_END
		},
		.driver_ctx = dev->data,
		.check = ixgbe_flow_actions_check_rss,
		.max_actions = 1,
	};
	int ret;
	const struct rte_flow_action *action;

	/* validate attributes */
	ret = ci_flow_check_attr(attr, NULL, error);
	if (ret)
		return ret;

	/* parse requested actions */
	ret = ci_flow_check_actions(actions, &ap_param, &parsed_actions, error);
	if (ret)
		return ret;
	action = parsed_actions.actions[0];

	if (ixgbe_rss_conf_init(rss_conf, action->conf))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"RSS context initialization failure");

	return 0;
}

/* remove the rss filter */
static void
ixgbe_clear_rss_filter(struct rte_eth_dev *dev)
{
	struct ixgbe_adapter *adapter =
		IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ixgbe_filter_info *filter_info =
		IXGBE_DEV_PRIVATE_TO_FILTER_INFO(dev->data->dev_private);

	if (filter_info->rss_info.conf.queue_num)
		ixgbe_config_rss_filter(adapter, &filter_info->rss_info, FALSE);
}

void
ixgbe_filterlist_init(struct rte_eth_dev *dev)
{
	struct ixgbe_adapter *adapter = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	TAILQ_INIT(&adapter->flow_list);
}

void
ixgbe_filterlist_flush(struct rte_eth_dev *dev)
{
	struct ixgbe_adapter *adapter = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ixgbe_filter_ele_base *ele, *tmp;

	RTE_TAILQ_FOREACH_SAFE(ele, &adapter->flow_list, entries, tmp) {
		struct ixgbe_flow_mem *ixgbe_flow_mem_ptr =
			(struct ixgbe_flow_mem *)ele;
		struct rte_flow *flow = ixgbe_flow_mem_ptr->flow;

		TAILQ_REMOVE(&adapter->flow_list, ele, entries);
		rte_free(flow->rule);
		rte_free(flow);
		rte_free(ele);
	}
}

/**
 * Create or destroy a flow rule.
 * Theorically one rule can match more than one filters.
 * We will let it use the filter which it hitt first.
 * So, the sequence matters.
 */
static struct rte_flow *
ixgbe_flow_create(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	int ret;
	struct ixgbe_adapter *adapter = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ixgbe_rte_flow_rss_conf rss_conf;
	struct rte_flow *flow = NULL;
	struct ixgbe_rss_conf_ele *rss_filter_ptr;
	struct ixgbe_flow_mem *ixgbe_flow_mem_ptr;

	/* try the new flow engine first */
	flow = ci_flow_create(&adapter->flow_engine_conf, attr, pattern, actions, error);
	if (flow != NULL)
		return flow;

	/* fall back to legacy flow engines */

	flow = rte_zmalloc("ixgbe_rte_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return (struct rte_flow *)flow;
	}
	ixgbe_flow_mem_ptr = rte_zmalloc("ixgbe_flow_mem",
			sizeof(struct ixgbe_flow_mem), 0);
	if (!ixgbe_flow_mem_ptr) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		rte_free(flow);
		return NULL;
	}
	ixgbe_flow_mem_ptr->flow = flow;
	TAILQ_INSERT_TAIL(&adapter->flow_list,
				&ixgbe_flow_mem_ptr->base, entries);

	memset(&rss_conf, 0, sizeof(struct ixgbe_rte_flow_rss_conf));
	ret = ixgbe_parse_rss_filter(dev, attr,
					actions, &rss_conf, error);
	if (!ret) {
		rss_filter_ptr = rte_zmalloc("ixgbe_rss_filter",
			sizeof(struct ixgbe_rss_conf_ele), 0);
		if (!rss_filter_ptr) {
			PMD_DRV_LOG(ERR, "failed to allocate memory");
			ret = -ENOMEM;
			goto out;
		}
		ret = ixgbe_config_rss_filter(adapter, &rss_conf, TRUE);
		if (ret) {
			rte_free(rss_filter_ptr);
			goto out;
		}
		ixgbe_rss_conf_init(&rss_filter_ptr->filter_info,
				    &rss_conf.conf);
		flow->rule = rss_filter_ptr;
		flow->filter_type = RTE_ETH_FILTER_HASH;
		return flow;
	}

out:
	TAILQ_REMOVE(&adapter->flow_list,
		&ixgbe_flow_mem_ptr->base, entries);
	rte_flow_error_set(error, -ret,
			   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to create flow.");
	rte_free(ixgbe_flow_mem_ptr);
	rte_free(flow);
	return NULL;
}

/**
 * Check if the flow rule is supported by ixgbe.
 * It only checks the format. Don't guarantee the rule can be programmed into
 * the HW. Because there can be no enough room for the rule.
 */
static int
ixgbe_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct ixgbe_adapter *ad = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ixgbe_rte_flow_rss_conf rss_conf;
	int ret;

	/* try the new flow engine first */
	ret = ci_flow_validate(&ad->flow_engine_conf, attr, pattern, actions, error);
	if (ret == 0)
		return ret;

	/* fall back to legacy engines */

	memset(&rss_conf, 0, sizeof(struct ixgbe_rte_flow_rss_conf));
	ret = ixgbe_parse_rss_filter(dev, attr,
					actions, &rss_conf, error);

	return ret;
}

/* Destroy a flow rule on ixgbe. */
static int
ixgbe_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	int ret;
	struct ixgbe_adapter *adapter =
		IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct rte_flow *pmd_flow = flow;
	enum rte_filter_type filter_type = pmd_flow->filter_type;
	struct ixgbe_filter_ele_base *flow_mem_base;
	struct ixgbe_rss_conf_ele *rss_filter_ptr;

	/* try the new flow engine first */
	ret = ci_flow_destroy(&adapter->flow_engine_conf, flow, error);
	if (ret == 0)
		return 0;

	/* fall back to legacy engines */

	/* Validate ownership before touching HW/SW state. */
	TAILQ_FOREACH(flow_mem_base, &adapter->flow_list, entries) {
		struct ixgbe_flow_mem *ixgbe_flow_mem_ptr =
			(struct ixgbe_flow_mem *)flow_mem_base;

		if (ixgbe_flow_mem_ptr->flow == pmd_flow)
			break;
	}
	if (flow_mem_base == NULL) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Flow not found for this port");
	}

	switch (filter_type) {
	case RTE_ETH_FILTER_HASH:
		rss_filter_ptr = (struct ixgbe_rss_conf_ele *)
				pmd_flow->rule;
		ret = ixgbe_config_rss_filter(adapter,
					&rss_filter_ptr->filter_info, FALSE);
		if (!ret)
			rte_free(rss_filter_ptr);
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter type (%d) not supported",
			    filter_type);
		ret = -EINVAL;
		break;
	}

	if (ret) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "Failed to destroy flow");
		return ret;
	}

	TAILQ_REMOVE(&adapter->flow_list, flow_mem_base, entries);
	rte_free(flow_mem_base);
	rte_free(flow);

	return ret;
}

/*  Destroy all flow rules associated with a port on ixgbe. */
static int
ixgbe_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	struct ixgbe_adapter *ad = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	int ret = 0;

	/* flush all flows from the new flow engine */
	ret = ci_flow_flush(&ad->flow_engine_conf, error);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to flush flow");
		return ret;
	}

	ixgbe_clear_rss_filter(dev);

	ixgbe_filterlist_flush(dev);

	return 0;
}

#define IXGBE_FLOW_DUMP_CHUNK_BYTES 32

static const char *
ixgbe_flow_rule_engine_name(const struct rte_flow *flow)
{
	switch (flow->filter_type) {
	case RTE_ETH_FILTER_ETHERTYPE:
		return "ethertype";
	case RTE_ETH_FILTER_SYN:
		return "syn";
	case RTE_ETH_FILTER_FDIR:
		return "fdir";
	case RTE_ETH_FILTER_L2_TUNNEL:
		return "l2_tunnel";
	case RTE_ETH_FILTER_HASH:
		return "hash";
	default:
		return "unknown";
	}
}

static size_t
ixgbe_flow_rule_size(const struct rte_flow *flow)
{
	switch (flow->filter_type) {
	case RTE_ETH_FILTER_ETHERTYPE:
		return sizeof(struct rte_eth_ethertype_filter);
	case RTE_ETH_FILTER_SYN:
		return sizeof(struct rte_eth_syn_filter);
	case RTE_ETH_FILTER_FDIR:
		return sizeof(struct ixgbe_fdir_rule);
	case RTE_ETH_FILTER_L2_TUNNEL:
		return sizeof(struct ixgbe_l2_tunnel_conf);
	case RTE_ETH_FILTER_HASH:
		return sizeof(struct ixgbe_rte_flow_rss_conf);
	default:
		return 0;
	}
}

static const void *
ixgbe_flow_rule_data(const struct rte_flow *flow)
{
	if (flow->rule == NULL)
		return NULL;

	return RTE_PTR_ADD(flow->rule, sizeof(struct ixgbe_filter_ele_base));
}

static void
ixgbe_flow_dump_blob(FILE *file, const char *engine,
		     const void *data, size_t data_len)
{
	const uint8_t *raw = (const uint8_t *)data;
	const size_t nchunks =
		(data_len + IXGBE_FLOW_DUMP_CHUNK_BYTES - 1) /
		IXGBE_FLOW_DUMP_CHUNK_BYTES;
	char title[64];
	size_t ci;

	fprintf(file, "FLOW DUMP: driver=ixgbe engine=%s\n", engine);
	fprintf(file, "FLOW DUMP: DATA size=%zu chunks=%zu chunk_bytes=%d\n",
		data_len, nchunks, IXGBE_FLOW_DUMP_CHUNK_BYTES);

	for (ci = 0; ci < nchunks; ci++) {
		const size_t off = ci * IXGBE_FLOW_DUMP_CHUNK_BYTES;
		const size_t clen =
			RTE_MIN((size_t)IXGBE_FLOW_DUMP_CHUNK_BYTES, data_len - off);

		snprintf(title, sizeof(title), "FLOW DUMP: chunk %03zu/%03zu",
			 ci + 1, nchunks);
		rte_memdump(file, title, raw + off, clen);
	}
}

static int
ixgbe_flow_dev_dump(struct rte_eth_dev *dev,
		    struct rte_flow *flow,
		    FILE *file,
		    struct rte_flow_error *error)
{
	struct ixgbe_adapter *ad = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ixgbe_filter_ele_base *flow_mem_base;
	bool found = false;
	int ret;

	/* try the new flow engine first */
	ret = ci_flow_dump(&ad->flow_engine_conf, flow, file, error);

	/*
	 * There are multiple possible situations here:
	 *
	 * - User requested to dump all flows
	 * - User requested to dump a specific flow
	 *
	 * For the first case, we keep going because legacy engines might still
	 * have flows we want to dump.
	 *
	 * For the second case, we only stop if the flow we were asked to dump
	 * was found in the new engines, otherwise we keep looking.
	 */
	if (flow != NULL && ret == 0)
		return 0;

	TAILQ_FOREACH(flow_mem_base, &ad->flow_list, entries) {
		struct ixgbe_flow_mem *ixgbe_flow_mem_ptr =
			(struct ixgbe_flow_mem *)flow_mem_base;
		struct rte_flow *p_flow = ixgbe_flow_mem_ptr->flow;
		const void *rule_data = NULL;
		const char *engine_name;
		size_t rule_size = 0;

		if (flow != NULL && p_flow != flow)
			continue;

		/* this should not happen */
		if (p_flow->rule == NULL) {
			PMD_DRV_LOG(DEBUG, "Invalid flow");
			continue;
		}

		rule_size = ixgbe_flow_rule_size(p_flow);
		if (rule_size == 0)
			continue;

		found = true;
		rule_data = ixgbe_flow_rule_data(p_flow);
		engine_name = ixgbe_flow_rule_engine_name(p_flow);
		ixgbe_flow_dump_blob(file, engine_name,
			rule_data, rule_size);
	}

	if (flow != NULL && !found)
		return rte_flow_error_set(error, ENOENT,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Flow not found");

	return 0;
}

const struct rte_flow_ops ixgbe_flow_ops = {
	.validate = ixgbe_flow_validate,
	.create = ixgbe_flow_create,
	.destroy = ixgbe_flow_destroy,
	.flush = ixgbe_flow_flush,
	.dev_dump = ixgbe_flow_dev_dump,
};
