/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef __SXE2_TM_H__
#define __SXE2_TM_H__
#include <ethdev_driver.h>
#include <rte_flow_driver.h>
#include "sxe2_osal.h"

#define SXE2_TM_MAX_LEVEL 7
#define SXE2_TM_1L_NODE_NUM_MAX 1
#define SXE2_TM_2L_NODE_NUM_MAX 8
#define SXE2_TM_3L_NODE_NUM_MAX 64
#define SXE2_TM_4L_NODE_NUM_MAX 256

#define SXE2_TM_MAX_CHILDREN_COUNT 8

#define SXE2_TM_WEIGHT_MAX  (200)
#define SXE2_TM_WEIGHT_MIN  (1)
#define SXE2_TM_WEIGHT_SUM  (32768)

#define SXE2_HW_RATE_MIN 62500ull
#define SXE2_HW_RATE_MAX 12500000000ull

#define SXE2_TM_PRIO_MAX (7)
#define SXE2_TM_PRIO_MIN (0)

enum sxe2_tm_node_type {
	SXE2_TM_NODE_TYPE_VSIG = 0,
	SXE2_TM_NODE_TYPE_MID,
	SXE2_TM_NODE_TYPE_QUEUE,
	SXE2_TM_NODE_TYPE_INVALID,
};

struct sxe2_tm_shaper_profile {
	TAILQ_ENTRY(sxe2_tm_shaper_profile) node;
	uint32_t id;
	uint32_t ref_cnt;
	struct rte_tm_shaper_params profile;
};

TAILQ_HEAD(sxe2_shaper_profile_list, sxe2_tm_shaper_profile);

struct sxe2_tm_node {
	uint16_t id;
	uint16_t teid;
	uint32_t level;
	uint32_t child_cnt;
	uint32_t type;
	uint16_t hw_weight;
	uint16_t weight;
	uint8_t priority;
	struct sxe2_tm_node *parent;
	uint8_t index_in_parent;
	struct sxe2_tm_node *children[SXE2_TM_MAX_CHILDREN_COUNT];
	struct sxe2_tm_shaper_profile *shaper_profile;
};

struct sxe2_tm_context {
	uint32_t tm_layers;
	uint16_t root_teid;
	uint8_t root_max_children;
	uint8_t prio_max;
	bool committed;
	struct sxe2_tm_node *root;
	struct sxe2_shaper_profile_list profile_list;
};

int32_t sxe2_tm_ops_get(struct rte_eth_dev *dev __rte_unused, void *arg);

int32_t sxe2_tm_init(struct rte_eth_dev *dev);

int32_t sxe2_tm_uninit(struct rte_eth_dev *dev);

#endif /* __SXE2_TM_H__ */
