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

void fill_actions(struct rte_flow_action *actions, uint64_t actions_selector,
	uint32_t counter, uint16_t next_table, uint16_t hairpinq);

#endif /* FLOW_PERF_ACTION_GEN */
