/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 NVIDIA Corporation & Affiliates
 */

#ifndef RTE_PMD_MLX5_NTA_SAMPLE_H_
#define RTE_PMD_MLX5_NTA_SAMPLE_H_

#include <stdint.h>

struct rte_flow_hw *
mlx5_nta_sample_flow_list_create(struct rte_eth_dev *dev,
				 enum mlx5_flow_type type,
				 const struct rte_flow_attr *attr,
				 const struct rte_flow_item pattern[],
				 const struct rte_flow_action actions[],
				 uint64_t item_flags, uint64_t action_flags,
				 struct rte_flow_error *error);

void
mlx5_nta_sample_context_free(struct rte_eth_dev *dev);

#endif /* RTE_PMD_MLX5_NTA_SAMPLE_H_ */
