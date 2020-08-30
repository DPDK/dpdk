/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 *
 * This file contains the items, actions and attributes
 * definition. And the methods to prepare and fill items,
 * actions and attributes to generate rte_flow rule.
 */

#ifndef FLOW_PERF_FLOW_GEN
#define FLOW_PERF_FLOW_GEN

#include <stdint.h>
#include <rte_flow.h>

#include "config.h"

/* Actions */
#define HAIRPIN_QUEUE_ACTION FLOW_ACTION_MASK(0)
#define HAIRPIN_RSS_ACTION   FLOW_ACTION_MASK(1)

/* Attributes */
#define INGRESS              FLOW_ATTR_MASK(0)
#define EGRESS               FLOW_ATTR_MASK(1)
#define TRANSFER             FLOW_ATTR_MASK(2)

struct rte_flow *
generate_flow(uint16_t port_id,
	uint16_t group,
	uint64_t *flow_attrs,
	uint64_t *flow_items,
	uint64_t *flow_actions,
	uint16_t next_table,
	uint32_t outer_ip_src,
	uint16_t hairpinq,
	uint64_t encap_data,
	uint64_t decap_data,
	struct rte_flow_error *error);

#endif /* FLOW_PERF_FLOW_GEN */
