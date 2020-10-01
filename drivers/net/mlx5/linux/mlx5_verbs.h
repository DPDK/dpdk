/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_VERBS_H_
#define RTE_PMD_MLX5_VERBS_H_

#include "mlx5.h"

struct mlx5_verbs_ops {
	mlx5_reg_mr_t reg_mr;
	mlx5_dereg_mr_t dereg_mr;
};

int mlx5_txq_ibv_obj_new(struct rte_eth_dev *dev, uint16_t idx);
void mlx5_txq_ibv_obj_release(struct mlx5_txq_obj *txq_obj);

/* Verbs ops struct */
extern const struct mlx5_verbs_ops mlx5_verbs_ops;
extern struct mlx5_obj_ops ibv_obj_ops;

#endif /* RTE_PMD_MLX5_VERBS_H_ */
