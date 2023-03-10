/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 HiSilicon Limited
 */

#ifndef HNS3_FLOW_H
#define HNS3_FLOW_H

#include <rte_flow.h>
#include <ethdev_driver.h>

#include "hns3_rss.h"
#include "hns3_fdir.h"

struct hns3_flow_counter {
	LIST_ENTRY(hns3_flow_counter) next; /* Pointer to the next counter. */
	uint32_t indirect:1; /* Indirect counter flag */
	uint32_t ref_cnt:31; /* Reference counter. */
	uint16_t id;   /* Counter ID. */
	uint64_t hits; /* Number of packets matched by the rule. */
};

struct rte_flow {
	enum rte_filter_type filter_type;
	void *rule;
	uint32_t counter_id;
};

struct hns3_flow_rss_conf {
	struct rte_flow_action_rss conf;
	uint8_t key[HNS3_RSS_KEY_SIZE_MAX];  /* Hash key */
	uint16_t queue[HNS3_RSS_QUEUES_BUFFER_NUM]; /* Queues indices to use */
	uint64_t pattern_type;
	uint64_t hw_pctypes; /* packet types in driver */
};

/* rss filter list structure */
struct hns3_rss_conf_ele {
	TAILQ_ENTRY(hns3_rss_conf_ele) entries;
	struct hns3_flow_rss_conf filter_info;
};

/* hns3_flow memory list structure */
struct hns3_flow_mem {
	TAILQ_ENTRY(hns3_flow_mem) entries;
	struct rte_flow *flow;
};

enum {
	HNS3_INDIRECT_ACTION_TYPE_COUNT = 1,
};

struct rte_flow_action_handle {
	int indirect_type;
	uint32_t counter_id;
};

union hns3_filter_conf {
	struct hns3_fdir_rule fdir_conf;
	struct hns3_flow_rss_conf rss_conf;
};

struct hns3_filter_info {
	enum rte_filter_type type;
	union hns3_filter_conf conf;
};

TAILQ_HEAD(hns3_rss_filter_list, hns3_rss_conf_ele);
TAILQ_HEAD(hns3_flow_mem_list, hns3_flow_mem);

int hns3_dev_flow_ops_get(struct rte_eth_dev *dev,
			  const struct rte_flow_ops **ops);
void hns3_flow_init(struct rte_eth_dev *dev);
void hns3_flow_uninit(struct rte_eth_dev *dev);
int hns3_restore_filter(struct hns3_adapter *hns);

#endif /* HNS3_FLOW_H */
