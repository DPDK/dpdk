/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef _RNP_RSS_H_
#define _RNP_RSS_H_

#include "rnp.h"

#define RNP_RSS_LEVEL_INNER	(2)
struct rnp_rss_hash_cfg {
	uint32_t func_id;
	uint32_t reg_val;
	uint64_t rss_flag;
};

enum rnp_rss_hash_type {
	RNP_RSS_IPV4,
	RNP_RSS_IPV6,
	RNP_RSS_IPV4_TCP,
	RNP_RSS_IPV4_UDP,
	RNP_RSS_IPV4_SCTP,
	RNP_RSS_IPV6_TCP,
	RNP_RSS_IPV6_UDP,
	RNP_RSS_IPV6_SCTP,
};

int
rnp_dev_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size);
int
rnp_dev_rss_reta_query(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size);
int
rnp_dev_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf);
int
rnp_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf);
int rnp_dev_rss_configure(struct rte_eth_dev *dev);

#endif /* _RNP_RSS_H_ */
