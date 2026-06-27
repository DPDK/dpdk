/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_FLOW_PARSE_PATTERN_H_
#define SXE2_FLOW_PARSE_PATTERN_H_
#include <rte_flow_driver.h>
#include "sxe2_osal.h"
#include "sxe2_flow_define.h"

#define SXE2_FLOW_EXPAND_NEXT(...) \
	((const enum sxe2_expansion []){ \
		__VA_ARGS__, 0, \
	})

struct sxe2_flow_expand_node {
	const enum rte_flow_item_type type;
	const enum sxe2_flow_tunnel_type tunnel_type;
	const uint8_t is_tunnel;
	const enum sxe2_expansion *const next;
	const char *const name;
};

typedef int32_t (*sxe2_flow_parse_pattern_func_t)(const struct rte_flow_item *item,
					      struct rte_flow_error *error,
					      struct sxe2_flow *flow,
					      enum sxe2_expansion next,
					      bool is_inner);

struct sxe2_flow_parse_pattern_ops {
	bool is_inner;
	sxe2_flow_parse_pattern_func_t func;
};

int32_t sxe2_flow_parse_pattern(struct rte_eth_dev *dev,
			    const struct rte_flow_item patterns[],
			    struct rte_flow_error *error,
			    struct sxe2_flow *flow);

#endif /* SXE2_FLOW_PARSE_PATTERN_H_ */
