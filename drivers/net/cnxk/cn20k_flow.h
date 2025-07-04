/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */
#ifndef __CN20K_FLOW_H__
#define __CN20K_FLOW_H__

#include <rte_flow_driver.h>

struct rte_flow *cn20k_flow_create(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
				   const struct rte_flow_item pattern[],
				   const struct rte_flow_action actions[],
				   struct rte_flow_error *error);
int cn20k_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		       struct rte_flow_error *error);

int cn20k_flow_info_get(struct rte_eth_dev *dev, struct rte_flow_port_info *port_info,
			struct rte_flow_queue_info *queue_info, struct rte_flow_error *err);

#define CNXK_NPC_COUNTERS_MAX 512

#endif /* __CN20K_FLOW_H__ */
