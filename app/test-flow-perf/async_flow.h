/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2026 Mellanox Technologies, Ltd
 *
 * This file contains the async flow API related definitions
 * and function declarations.
 */

#ifndef FLOW_PERF_ASYNC_FLOW
#define FLOW_PERF_ASYNC_FLOW

#include <rte_flow.h>
#include <stdbool.h>
#include <stdint.h>

#include "config.h"

/* Per-port async flow resources */
struct async_flow_resources {
	struct rte_flow_pattern_template *pattern_template;
	struct rte_flow_actions_template *actions_template;
	struct rte_flow_template_table *table;
	uint32_t table_capacity;
	bool initialized;
};

/* Initialize async flow engine for a port */
int async_flow_init_port(uint16_t port_id, uint32_t nb_queues, uint32_t queue_size,
			 uint64_t *flow_items, uint64_t *flow_actions, uint64_t *flow_attrs,
			 uint8_t flow_group, uint32_t rules_count);

/* Create a flow rule asynchronously */
struct rte_flow *async_generate_flow(uint16_t port_id, uint32_t queue_id, uint64_t *flow_items,
				     uint64_t *flow_actions, uint32_t counter, uint16_t hairpinq,
				     uint64_t encap_data, uint64_t decap_data, uint16_t dst_port,
				     uint8_t core_idx, uint8_t rx_queues_count, bool unique_data,
				     bool postpone, struct rte_flow_error *error);

/* Cleanup async flow resources for a port */
void async_flow_cleanup_port(uint16_t port_id);

#endif /* FLOW_PERF_ASYNC_FLOW */
