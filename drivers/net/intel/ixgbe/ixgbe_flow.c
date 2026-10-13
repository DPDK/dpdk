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

const struct ci_flow_engine_list ixgbe_flow_engine_list = {
	{
		&ixgbe_ethertype_flow_engine,
		&ixgbe_syn_flow_engine,
		&ixgbe_l2_tunnel_flow_engine,
		&ixgbe_ntuple_flow_engine,
		&ixgbe_security_flow_engine,
		&ixgbe_fdir_flow_engine,
		&ixgbe_fdir_tunnel_flow_engine,
		&ixgbe_hash_flow_engine,
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
	struct ixgbe_adapter *ad = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	return ci_flow_create(&ad->flow_engine_conf, attr, pattern, actions, error);
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

	return ci_flow_validate(&ad->flow_engine_conf, attr, pattern, actions, error);
}

/* Destroy a flow rule on ixgbe. */
static int
ixgbe_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	struct ixgbe_adapter *ad = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	return ci_flow_destroy(&ad->flow_engine_conf, flow, error);
}

/*  Destroy all flow rules associated with a port on ixgbe. */
static int
ixgbe_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	struct ixgbe_adapter *ad = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	int ret = 0;

	/* flush the flow engine */
	ret = ci_flow_flush(&ad->flow_engine_conf, error);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to flush flow");
		return ret;
	}

	return 0;
}

static int
ixgbe_flow_dev_dump(struct rte_eth_dev *dev,
		    struct rte_flow *flow,
		    FILE *file,
		    struct rte_flow_error *error)
{
	struct ixgbe_adapter *ad = IXGBE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	return ci_flow_dump(&ad->flow_engine_conf, flow, file, error);
}

const struct rte_flow_ops ixgbe_flow_ops = {
	.validate = ixgbe_flow_validate,
	.create = ixgbe_flow_create,
	.destroy = ixgbe_flow_destroy,
	.flush = ixgbe_flow_flush,
	.dev_dump = ixgbe_flow_dev_dump,
};
