/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_POLICER_H__
#define __INCLUDE_RTE_POLICER_H__

#include <stdint.h>
#include <rte_meter.h>

enum rte_phb_action {
	e_RTE_PHB_ACTION_GREEN = RTE_COLOR_GREEN,
	e_RTE_PHB_ACTION_YELLOW = RTE_COLOR_YELLOW,
	e_RTE_PHB_ACTION_RED = RTE_COLOR_RED,
	e_RTE_PHB_ACTION_DROP = 3,
};

struct rte_phb {
	enum rte_phb_action actions[RTE_COLORS][RTE_COLORS];
};

int
rte_phb_config(struct rte_phb *phb_table, uint32_t phb_table_index,
	enum rte_color pre_meter, enum rte_color post_meter, enum rte_phb_action action);

static inline enum rte_phb_action
policer_run(struct rte_phb *phb_table, uint32_t phb_table_index, enum rte_color pre_meter, enum rte_color post_meter)
{
	struct rte_phb *phb = &phb_table[phb_table_index];
	enum rte_phb_action action = phb->actions[pre_meter][post_meter];

	return action;
}

#endif
