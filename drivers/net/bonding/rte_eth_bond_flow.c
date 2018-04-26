/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <sys/queue.h>

#include <rte_malloc.h>
#include <rte_tailq.h>
#include <rte_flow.h>

#include "rte_eth_bond_private.h"

static struct rte_flow *
bond_flow_alloc(int numa_node, const struct rte_flow_attr *attr,
		   const struct rte_flow_item *items,
		   const struct rte_flow_action *actions)
{
	struct rte_flow *flow;
	size_t fdsz;

	fdsz = rte_flow_copy(NULL, 0, attr, items, actions);
	flow = rte_zmalloc_socket(NULL, sizeof(struct rte_flow) + fdsz,
				  RTE_CACHE_LINE_SIZE, numa_node);
	if (unlikely(flow == NULL)) {
		RTE_BOND_LOG(ERR, "Could not allocate new flow");
		return NULL;
	}
	flow->fd = (void *)((uintptr_t)flow + sizeof(*flow));
	if (unlikely(rte_flow_copy(flow->fd, fdsz, attr, items, actions) !=
		     fdsz)) {
		RTE_BOND_LOG(ERR, "Failed to copy flow description");
		rte_free(flow);
		return NULL;
	}
	return flow;
}

static void
bond_flow_release(struct rte_flow **flow)
{
	rte_free(*flow);
	*flow = NULL;
}

static int
bond_flow_validate(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		   const struct rte_flow_item patterns[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *err)
{
	struct bond_dev_private *internals = dev->data->dev_private;
	int i;
	int ret;

	for (i = 0; i < internals->slave_count; i++) {
		ret = rte_flow_validate(internals->slaves[i].port_id, attr,
					patterns, actions, err);
		if (ret) {
			RTE_BOND_LOG(ERR, "Operation rte_flow_validate failed"
				     " for slave %d with error %d", i, ret);
			return ret;
		}
	}
	return 0;
}

static struct rte_flow *
bond_flow_create(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		 const struct rte_flow_item patterns[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *err)
{
	struct bond_dev_private *internals = dev->data->dev_private;
	struct rte_flow *flow;
	int i;

	flow = bond_flow_alloc(dev->data->numa_node, attr, patterns, actions);
	if (unlikely(flow == NULL)) {
		rte_flow_error_set(err, ENOMEM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, rte_strerror(ENOMEM));
		return NULL;
	}
	for (i = 0; i < internals->slave_count; i++) {
		flow->flows[i] = rte_flow_create(internals->slaves[i].port_id,
						 attr, patterns, actions, err);
		if (unlikely(flow->flows[i] == NULL)) {
			RTE_BOND_LOG(ERR, "Failed to create flow on slave %d",
				     i);
			goto err;
		}
	}
	TAILQ_INSERT_TAIL(&internals->flow_list, flow, next);
	return flow;
err:
	/* Destroy all slaves flows. */
	for (i = 0; i < internals->slave_count; i++) {
		if (flow->flows[i] != NULL)
			rte_flow_destroy(internals->slaves[i].port_id,
					 flow->flows[i], err);
	}
	bond_flow_release(&flow);
	return NULL;
}

static int
bond_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		  struct rte_flow_error *err)
{
	struct bond_dev_private *internals = dev->data->dev_private;
	int i;
	int ret = 0;

	for (i = 0; i < internals->slave_count; i++) {
		int lret;

		if (unlikely(flow->flows[i] == NULL))
			continue;
		lret = rte_flow_destroy(internals->slaves[i].port_id,
					flow->flows[i], err);
		if (unlikely(lret != 0)) {
			RTE_BOND_LOG(ERR, "Failed to destroy flow on slave %d:"
				     " %d", i, lret);
			ret = lret;
		}
	}
	TAILQ_REMOVE(&internals->flow_list, flow, next);
	bond_flow_release(&flow);
	return ret;
}

static int
bond_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *err)
{
	struct bond_dev_private *internals = dev->data->dev_private;
	struct rte_flow *flow;
	void *tmp;
	int ret = 0;
	int lret;

	/* Destroy all bond flows from its slaves instead of flushing them to
	 * keep the LACP flow or any other external flows.
	 */
	TAILQ_FOREACH_SAFE(flow, &internals->flow_list, next, tmp) {
		lret = bond_flow_destroy(dev, flow, err);
		if (unlikely(lret != 0))
			ret = lret;
	}
	if (unlikely(ret != 0))
		RTE_BOND_LOG(ERR, "Failed to flush flow in all slaves");
	return ret;
}

static int
bond_flow_query_count(struct rte_eth_dev *dev, struct rte_flow *flow,
		      const struct rte_flow_action *action,
		      struct rte_flow_query_count *count,
		      struct rte_flow_error *err)
{
	struct bond_dev_private *internals = dev->data->dev_private;
	struct rte_flow_query_count slave_count;
	int i;
	int ret;

	count->bytes = 0;
	count->hits = 0;
	rte_memcpy(&slave_count, count, sizeof(slave_count));
	for (i = 0; i < internals->slave_count; i++) {
		ret = rte_flow_query(internals->slaves[i].port_id,
				     flow->flows[i], action,
				     &slave_count, err);
		if (unlikely(ret != 0)) {
			RTE_BOND_LOG(ERR, "Failed to query flow on"
				     " slave %d: %d", i, ret);
			return ret;
		}
		count->bytes += slave_count.bytes;
		count->hits += slave_count.hits;
		slave_count.bytes = 0;
		slave_count.hits = 0;
	}
	return 0;
}

static int
bond_flow_query(struct rte_eth_dev *dev, struct rte_flow *flow,
		const struct rte_flow_action *action, void *arg,
		struct rte_flow_error *err)
{
	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_COUNT:
		return bond_flow_query_count(dev, flow, action, arg, err);
	default:
		return rte_flow_error_set(err, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, arg,
					  rte_strerror(ENOTSUP));
	}
}

static int
bond_flow_isolate(struct rte_eth_dev *dev, int set,
		  struct rte_flow_error *err)
{
	struct bond_dev_private *internals = dev->data->dev_private;
	int i;
	int ret;

	for (i = 0; i < internals->slave_count; i++) {
		ret = rte_flow_isolate(internals->slaves[i].port_id, set, err);
		if (unlikely(ret != 0)) {
			RTE_BOND_LOG(ERR, "Operation rte_flow_isolate failed"
				     " for slave %d with error %d", i, ret);
			internals->flow_isolated_valid = 0;
			return ret;
		}
	}
	internals->flow_isolated = set;
	internals->flow_isolated_valid = 1;
	return 0;
}

const struct rte_flow_ops bond_flow_ops = {
	.validate = bond_flow_validate,
	.create = bond_flow_create,
	.destroy = bond_flow_destroy,
	.flush = bond_flow_flush,
	.query = bond_flow_query,
	.isolate = bond_flow_isolate,
};
