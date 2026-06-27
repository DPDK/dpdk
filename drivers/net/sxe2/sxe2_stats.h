/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef __SXE2_STATS_H__
#define __SXE2_STATS_H__

#define SXE2_STATS_FIELD_NAME_SIZE  50

struct sxe2_stats_field {
	char name[SXE2_STATS_FIELD_NAME_SIZE];
	uint32_t offset;
};

struct sxe2_adapter;

int32_t sxe2_stats_info_get(struct rte_eth_dev *dev,
			struct rte_eth_stats *stats,
			struct eth_queue_stats *qstats);

int32_t sxe2_xstats_info_get(struct rte_eth_dev *dev,
	struct rte_eth_xstat *xstats, uint32_t usr_cnt);

int32_t sxe2_xstats_names_get(__rte_unused struct rte_eth_dev *dev,
			  struct rte_eth_xstat_name *xstats_names,
			  __rte_unused unsigned int usr_cnt);

int32_t sxe2_stats_hw_reset(struct rte_eth_dev *dev);

int32_t sxe2_stats_info_reset(struct rte_eth_dev *dev);

int32_t sxe2_stats_init(struct rte_eth_dev *dev);

int32_t sxe2_queue_stats_mapping_set(struct rte_eth_dev *eth_dev,
				  uint16_t queue_id, uint8_t pool_idx, uint8_t is_rx);

int32_t sxe2_queue_stats_map_init(struct rte_eth_dev *dev);

#endif
