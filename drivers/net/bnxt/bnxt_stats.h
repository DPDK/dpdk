/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_STATS_H_
#define _BNXT_STATS_H_

#include <rte_ethdev_driver.h>

void bnxt_free_stats(struct bnxt *bp);
int bnxt_stats_get_op(struct rte_eth_dev *eth_dev,
			   struct rte_eth_stats *bnxt_stats);
void bnxt_stats_reset_op(struct rte_eth_dev *eth_dev);
int bnxt_dev_xstats_get_names_op(__rte_unused struct rte_eth_dev *eth_dev,
	struct rte_eth_xstat_name *xstats_names,
	__rte_unused unsigned int limit);
int bnxt_dev_xstats_get_op(struct rte_eth_dev *eth_dev,
			   struct rte_eth_xstat *xstats, unsigned int n);
void bnxt_dev_xstats_reset_op(struct rte_eth_dev *eth_dev);
int bnxt_dev_xstats_get_by_id_op(struct rte_eth_dev *dev, const uint64_t *ids,
				uint64_t *values, unsigned int limit);
int bnxt_dev_xstats_get_names_by_id_op(struct rte_eth_dev *dev,
				struct rte_eth_xstat_name *xstats_names,
				const uint64_t *ids, unsigned int limit);

struct bnxt_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint64_t offset;
};
#endif
