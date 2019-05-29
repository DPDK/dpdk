/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_TM_H__
#define __OTX2_TM_H__

#include <stdbool.h>

#include <rte_tm_driver.h>

#define NIX_TM_DEFAULT_TREE	BIT_ULL(0)

struct otx2_eth_dev;

void otx2_nix_tm_conf_init(struct rte_eth_dev *eth_dev);
int otx2_nix_tm_init_default(struct rte_eth_dev *eth_dev);
int otx2_nix_tm_fini(struct rte_eth_dev *eth_dev);

struct otx2_nix_tm_node {
	TAILQ_ENTRY(otx2_nix_tm_node) node;
	uint32_t id;
	uint32_t hw_id;
	uint32_t priority;
	uint32_t weight;
	uint16_t level_id;
	uint16_t hw_lvl_id;
	uint32_t rr_prio;
	uint32_t rr_num;
	uint32_t max_prio;
	uint32_t parent_hw_id;
	uint32_t flags;
#define NIX_TM_NODE_HWRES	BIT_ULL(0)
#define NIX_TM_NODE_ENABLED	BIT_ULL(1)
#define NIX_TM_NODE_USER	BIT_ULL(2)
	struct otx2_nix_tm_node *parent;
	struct rte_tm_node_params params;
};

struct otx2_nix_tm_shaper_profile {
	TAILQ_ENTRY(otx2_nix_tm_shaper_profile) shaper;
	uint32_t shaper_profile_id;
	uint32_t reference_count;
	struct rte_tm_shaper_params profile;
};

struct shaper_params {
	uint64_t burst_exponent;
	uint64_t burst_mantissa;
	uint64_t div_exp;
	uint64_t exponent;
	uint64_t mantissa;
	uint64_t burst;
	uint64_t rate;
};

TAILQ_HEAD(otx2_nix_tm_node_list, otx2_nix_tm_node);
TAILQ_HEAD(otx2_nix_tm_shaper_profile_list, otx2_nix_tm_shaper_profile);

#define MAX_SCHED_WEIGHT ((uint8_t)~0)
#define NIX_TM_RR_QUANTUM_MAX ((1 << 24) - 1)

/* DEFAULT_RR_WEIGHT * NIX_TM_RR_QUANTUM_MAX / MAX_SCHED_WEIGHT  */
/* = NIX_MAX_HW_MTU */
#define DEFAULT_RR_WEIGHT 71

#endif /* __OTX2_TM_H__ */
