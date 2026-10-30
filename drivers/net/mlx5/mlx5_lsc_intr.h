/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 NVIDIA Corporation & Affiliates
 */

#ifndef RTE_PMD_MLX5_LSC_INTR_H_
#define RTE_PMD_MLX5_LSC_INTR_H_

struct rte_eth_dev;

int mlx5_lsc_intr_setup(struct rte_eth_dev *dev);
void mlx5_lsc_intr_cleanup(struct rte_eth_dev *dev);

#endif /* RTE_PMD_MLX5_LSC_INTR_H_ */
