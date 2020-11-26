/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 *
 * This file contains the items related methods
 */

#ifndef FLOW_PERF_ITEMS_GEN
#define FLOW_PERF_ITEMS_GEN

#include <stdint.h>
#include <rte_flow.h>

#include "config.h"

void fill_items(struct rte_flow_item *items, uint64_t *flow_items,
	uint32_t outer_ip_src, uint8_t core_idx);

#endif /* FLOW_PERF_ITEMS_GEN */
