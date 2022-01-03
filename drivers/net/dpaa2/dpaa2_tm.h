/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */

#ifndef _DPAA2_TM_H_
#define _DPAA2_TM_H_

#include <rte_tm.h>

enum node_type {
	NON_LEAF_NODE = 0,
	LEAF_NODE
};

enum level_type {
	LNI_LEVEL = 0,
	CHANNEL_LEVEL,
	QUEUE_LEVEL,
	MAX_LEVEL
};

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
	uint32_t level_id;
	uint16_t channel_id; /* Only for level 1 nodes */
	uint16_t tc_id; /* Only for level 1 nodes */
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
