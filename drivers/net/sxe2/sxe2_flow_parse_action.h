/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_FLOW_PARSE_ACTION_H_
#define SXE2_FLOW_PARSE_ACTION_H_
#include <rte_flow_driver.h>

#include "sxe2_osal.h"
#include "sxe2_flow_define.h"


int32_t sxe2_flow_parse_action(struct rte_eth_dev *dev,
			const struct rte_flow_action actions[],
			struct rte_flow_error *error,
			struct sxe2_flow *flow);

int32_t sxe2_flow_parse_action_port_id(struct rte_eth_dev *dev,
			const struct rte_flow_action *action,
			struct rte_flow_error *error,
			struct sxe2_flow *flow);

#endif /* SXE2_FLOW_PARSE_ACTION_H_ */
