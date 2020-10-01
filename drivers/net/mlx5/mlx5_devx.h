/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_DEVX_H_
#define RTE_PMD_MLX5_DEVX_H_

#include "mlx5.h"

int mlx5_txq_devx_obj_new(struct rte_eth_dev *dev, uint16_t idx);
void mlx5_txq_devx_obj_release(struct mlx5_txq_obj *txq_obj);

extern struct mlx5_obj_ops devx_obj_ops;

#endif /* RTE_PMD_MLX5_DEVX_H_ */
