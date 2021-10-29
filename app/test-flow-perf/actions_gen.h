/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 *
 * This file contains the functions definitions to
 * generate each supported action.
 */

#ifndef FLOW_PERF_ACTION_GEN
#define FLOW_PERF_ACTION_GEN

#include <rte_flow.h>

#include "config.h"

#define RTE_IP_TYPE_UDP	17
#define RTE_IP_TYPE_GRE	47
#define RTE_VXLAN_GPE_UDP_PORT 250
#define RTE_GENEVE_UDP_PORT 6081

void fill_actions(struct rte_flow_action *actions, uint64_t *flow_actions,
	uint32_t counter, uint16_t next_table, uint16_t hairpinq,
	uint64_t encap_data, uint64_t decap_data, uint8_t core_idx,
	bool unique_data, uint8_t rx_queues_count, uint16_t dst_port);

#endif /* FLOW_PERF_ACTION_GEN */
