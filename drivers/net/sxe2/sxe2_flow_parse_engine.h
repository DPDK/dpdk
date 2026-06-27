/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_FLOW_PARSE_ENGINE_H_
#define SXE2_FLOW_PARSE_ENGINE_H_
#include "sxe2_osal.h"
#include "sxe2_flow_define.h"

int32_t sxe2_flow_parse_engine(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
			const struct rte_flow_action actions[], struct rte_flow_error *error,
			struct sxe2_flow *flow);
#endif /* SXE2_FLOW_PARSE_ENGINE_H_ */
