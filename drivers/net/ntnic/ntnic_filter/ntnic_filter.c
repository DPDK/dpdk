/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <rte_flow_driver.h>
#include "ntnic_mod_reg.h"

static int
eth_flow_destroy(struct rte_eth_dev *eth_dev __rte_unused, struct rte_flow *flow __rte_unused,
	struct rte_flow_error *error __rte_unused)
{
	int res = 0;

	return res;
}

static struct rte_flow *eth_flow_create(struct rte_eth_dev *eth_dev __rte_unused,
	const struct rte_flow_attr *attr __rte_unused,
	const struct rte_flow_item items[] __rte_unused,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused)
{
	struct rte_flow *flow = NULL;

	return flow;
}

static const struct rte_flow_ops dev_flow_ops = {
	.create = eth_flow_create,
	.destroy = eth_flow_destroy,
};

void dev_flow_init(void)
{
	register_dev_flow_ops(&dev_flow_ops);
}
