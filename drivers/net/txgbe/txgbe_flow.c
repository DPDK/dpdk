/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include <rte_flow.h>
#include <rte_flow_driver.h>

#include "txgbe_ethdev.h"

/**
 * Create or destroy a flow rule.
 * Theorically one rule can match more than one filters.
 * We will let it use the filter which it hitt first.
 * So, the sequence matters.
 */
static struct rte_flow *
txgbe_flow_create(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	struct rte_flow *flow = NULL;
	return flow;
}

/**
 * Check if the flow rule is supported by txgbe.
 * It only checks the format. Don't guarantee the rule can be programmed into
 * the HW. Because there can be no enough room for the rule.
 */
static int
txgbe_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	int ret = 0;

	return ret;
}

/* Destroy a flow rule on txgbe. */
static int
txgbe_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	int ret = 0;

	return ret;
}

/*  Destroy all flow rules associated with a port on txgbe. */
static int
txgbe_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	int ret = 0;

	return ret;
}

const struct rte_flow_ops txgbe_flow_ops = {
	.validate = txgbe_flow_validate,
	.create = txgbe_flow_create,
	.destroy = txgbe_flow_destroy,
	.flush = txgbe_flow_flush,
};

