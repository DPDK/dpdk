/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 NXP
 */

#ifndef _DPAA2_TM_H_
#define _DPAA2_TM_H_

#include <rte_tm.h>

struct dpaa2_tm_shaper_profile {
	LIST_ENTRY(dpaa2_tm_shaper_profile) next;
	uint32_t id;
	int refcnt;
	struct rte_tm_shaper_params params;
};

struct dpaa2_tm_node {
	LIST_ENTRY(dpaa2_tm_node) next;
	uint32_t id;
	uint32_t type;
	int refcnt;
	struct dpaa2_tm_node *parent;
	struct dpaa2_tm_shaper_profile *profile;
	uint32_t weight;
	uint32_t priority;
	uint64_t stats_mask;
};

int dpaa2_tm_init(struct rte_eth_dev *dev);
void dpaa2_tm_deinit(struct rte_eth_dev *dev);

#endif /* _DPAA2_TM_H_ */
