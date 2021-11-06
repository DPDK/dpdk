/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 HiSilicon Limited
 */

#ifndef _HNS3_FLOW_H_
#define _HNS3_FLOW_H_

#include <rte_flow.h>

struct hns3_flow_counter {
	LIST_ENTRY(hns3_flow_counter) next; /* Pointer to the next counter. */
	uint32_t shared:1;   /* Share counter ID with other flow rules. */
	uint32_t ref_cnt:31; /* Reference counter. */
	uint16_t id;   /* Counter ID. */
	uint64_t hits; /* Number of packets matched by the rule. */
};

struct rte_flow {
	enum rte_filter_type filter_type;
	void *rule;
	uint32_t counter_id;
};

/* rss filter list structure */
struct hns3_rss_conf_ele {
	TAILQ_ENTRY(hns3_rss_conf_ele) entries;
	struct hns3_rss_conf filter_info;
};

/* hns3_flow memory list structure */
struct hns3_flow_mem {
	TAILQ_ENTRY(hns3_flow_mem) entries;
	struct rte_flow *flow;
};

TAILQ_HEAD(hns3_rss_filter_list, hns3_rss_conf_ele);
TAILQ_HEAD(hns3_flow_mem_list, hns3_flow_mem);

int hns3_dev_flow_ops_get(struct rte_eth_dev *dev,
			  const struct rte_flow_ops **ops);
void hns3_flow_init(struct rte_eth_dev *dev);
void hns3_flow_uninit(struct rte_eth_dev *dev);

#endif /* _HNS3_FLOW_H_ */
