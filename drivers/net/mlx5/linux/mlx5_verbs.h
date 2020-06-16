/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_VERBS_H_
#define RTE_PMD_MLX5_VERBS_H_

struct mlx5_verbs_ops {
	mlx5_reg_mr_t reg_mr;
	mlx5_dereg_mr_t dereg_mr;
};

/* Verbs ops struct */
extern const struct mlx5_verbs_ops mlx5_verbs_ops;
#endif /* RTE_PMD_MLX5_VERBS_H_ */
