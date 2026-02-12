/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 NVIDIA Corporation & Affiliates
 */

#ifndef RTE_PMD_MLX5_DRIVER_EVENT_H_
#define RTE_PMD_MLX5_DRIVER_EVENT_H_

/* Forward declarations. */
struct mlx5_rxq_priv;
struct mlx5_txq_ctrl;

void mlx5_driver_event_notify_rxq_create(struct mlx5_rxq_priv *rxq);
void mlx5_driver_event_notify_rxq_destroy(struct mlx5_rxq_priv *rxq);

void mlx5_driver_event_notify_txq_create(struct mlx5_txq_ctrl *txq_ctrl);
void mlx5_driver_event_notify_txq_destroy(struct mlx5_txq_ctrl *txq_ctrl);

#endif /* RTE_PMD_MLX5_DRIVER_EVENT_H_ */
