/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdlib.h>
#include "rte_policer.h"

int
rte_phb_config(struct rte_phb *phb_table, uint32_t phb_table_index,
	enum rte_meter_color pre_meter, enum rte_meter_color post_meter, enum rte_phb_action action)
{
	struct rte_phb *phb = NULL;

	/* User argument checking */
	if (phb_table == NULL) {
		return -1;
	}

	if ((pre_meter > e_RTE_METER_RED) || (post_meter > e_RTE_METER_RED) || (pre_meter > post_meter)) {
		return -2;
	}

	/* Set action in PHB table entry */
	phb = &phb_table[phb_table_index];
	phb->actions[pre_meter][post_meter] = action;


	return 0;
}
