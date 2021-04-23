/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_STATS_H_
#define _BNXT_STATS_H_

#include <ethdev_driver.h>

void bnxt_free_stats(struct bnxt *bp);
int bnxt_stats_get_op(struct rte_eth_dev *eth_dev,
			   struct rte_eth_stats *bnxt_stats);
int bnxt_stats_reset_op(struct rte_eth_dev *eth_dev);
int bnxt_dev_xstats_get_names_op(struct rte_eth_dev *eth_dev,
	struct rte_eth_xstat_name *xstats_names,
	__rte_unused unsigned int limit);
int bnxt_dev_xstats_get_op(struct rte_eth_dev *eth_dev,
			   struct rte_eth_xstat *xstats, unsigned int n);
int bnxt_dev_xstats_reset_op(struct rte_eth_dev *eth_dev);

struct bnxt_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	uint64_t offset;
};
#endif
