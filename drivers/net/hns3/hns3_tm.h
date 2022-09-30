/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 HiSilicon Limited.
 */

#ifndef HNS3_TM_H
#define HNS3_TM_H

#include <stdint.h>
#include <rte_tailq.h>
#include <rte_tm_driver.h>

struct hns3_port_limit_rate_cmd {
	uint32_t speed;  /* Unit Mbps */
	uint32_t rsvd[5];
};

struct hns3_tc_limit_rate_cmd {
	uint32_t speed;  /* Unit Mbps */
	uint8_t tc_id;
	uint8_t rsvd[3];
	uint32_t rsvd1[4];
};

enum hns3_tm_node_type {
	HNS3_TM_NODE_TYPE_PORT,
	HNS3_TM_NODE_TYPE_TC,
	HNS3_TM_NODE_TYPE_QUEUE,
	HNS3_TM_NODE_TYPE_MAX,
};

enum hns3_tm_node_level {
	HNS3_TM_NODE_LEVEL_PORT,
	HNS3_TM_NODE_LEVEL_TC,
	HNS3_TM_NODE_LEVEL_QUEUE,
	HNS3_TM_NODE_LEVEL_MAX,
};

struct hns3_tm_shaper_profile {
	TAILQ_ENTRY(hns3_tm_shaper_profile) node;
	uint32_t shaper_profile_id;
	uint32_t reference_count;
	struct rte_tm_shaper_params profile;
};

TAILQ_HEAD(hns3_shaper_profile_list, hns3_tm_shaper_profile);

struct hns3_tm_node {
	TAILQ_ENTRY(hns3_tm_node) node;
	uint32_t id;
	uint32_t reference_count;
	struct hns3_tm_node *parent;
	struct hns3_tm_shaper_profile *shaper_profile;
	struct rte_tm_node_params params;
};

TAILQ_HEAD(hns3_tm_node_list, hns3_tm_node);

struct hns3_tm_conf {
	uint32_t nb_leaf_nodes_max; /* max numbers of leaf nodes */
	uint32_t nb_nodes_max; /* max numbers of nodes */
	uint32_t nb_shaper_profile_max; /* max numbers of shaper profile */

	struct hns3_shaper_profile_list shaper_profile_list;
	uint32_t nb_shaper_profile; /* number of shaper profile */

	struct hns3_tm_node *root;
	struct hns3_tm_node_list tc_list;
	struct hns3_tm_node_list queue_list;
	uint32_t nb_tc_node; /* number of added TC nodes */
	uint32_t nb_queue_node; /* number of added queue nodes */

	/*
	 * This flag is used to check if APP can change the TM node
	 * configuration.
	 * When it's true, means the configuration is applied to HW,
	 * APP should not add/delete the TM node configuration.
	 * When starting the port, APP should call the hierarchy_commit API to
	 * set this flag to true. When stopping the port, this flag should be
	 * set to false.
	 */
	bool committed;
};

/*
 * This API used to calc node TC no. User must make sure the node id is in the
 * TC node id range.
 *
 * User could call rte_eth_dev_info_get API to get port's max_tx_queues, The TM
 * id's assignment should following the below rules:
 *     [0, max_tx_queues-1]: correspond queues's node id
 *     max_tx_queues + 0   : correspond TC0's node id
 *     max_tx_queues + 1   : correspond TC1's node id
 *     ...
 *     max_tx_queues + 7   : correspond TC7's node id
 *     max_tx_queues + 8   : correspond port's node id
 *
 */
static inline uint8_t
hns3_tm_calc_node_tc_no(struct hns3_tm_conf *conf, uint32_t node_id)
{
	if (node_id >= conf->nb_leaf_nodes_max &&
	    node_id < conf->nb_nodes_max - 1)
		return node_id - conf->nb_leaf_nodes_max;
	else
		return 0;
}

struct hns3_hw;

void hns3_tm_conf_init(struct rte_eth_dev *dev);
void hns3_tm_conf_uninit(struct rte_eth_dev *dev);
int hns3_tm_ops_get(struct rte_eth_dev *dev __rte_unused, void *arg);
void hns3_tm_dev_start_proc(struct hns3_hw *hw);
void hns3_tm_dev_stop_proc(struct hns3_hw *hw);
int hns3_tm_conf_update(struct hns3_hw *hw);

#endif /* HNS3_TM_H */
