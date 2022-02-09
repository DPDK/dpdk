/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell.
 */
#ifndef __CN10K_RTE_FLOW_H__
#define __CN10K_RTE_FLOW_H__

#include <rte_flow_driver.h>

struct rte_flow *cn10k_flow_create(struct rte_eth_dev *dev,
				   const struct rte_flow_attr *attr,
				   const struct rte_flow_item pattern[],
				   const struct rte_flow_action actions[],
				   struct rte_flow_error *error);
int cn10k_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		       struct rte_flow_error *error);

#endif /* __CN10K_RTE_FLOW_H__ */
