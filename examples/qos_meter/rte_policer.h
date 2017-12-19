/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_POLICER_H__
#define __INCLUDE_RTE_POLICER_H__

#include <stdint.h>
#include <rte_meter.h>

enum rte_phb_action {
	e_RTE_PHB_ACTION_GREEN = e_RTE_METER_GREEN,
	e_RTE_PHB_ACTION_YELLOW = e_RTE_METER_YELLOW,
	e_RTE_PHB_ACTION_RED = e_RTE_METER_RED,
	e_RTE_PHB_ACTION_DROP = 3,
};

struct rte_phb {
	enum rte_phb_action actions[e_RTE_METER_COLORS][e_RTE_METER_COLORS];
};

int
rte_phb_config(struct rte_phb *phb_table, uint32_t phb_table_index,
	enum rte_meter_color pre_meter, enum rte_meter_color post_meter, enum rte_phb_action action);

static inline enum rte_phb_action
policer_run(struct rte_phb *phb_table, uint32_t phb_table_index, enum rte_meter_color pre_meter, enum rte_meter_color post_meter)
{
	struct rte_phb *phb = &phb_table[phb_table_index];
	enum rte_phb_action action = phb->actions[pre_meter][post_meter];

	return action;
}

#endif
