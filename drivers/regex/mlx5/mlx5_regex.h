/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef MLX5_REGEX_H
#define MLX5_REGEX_H

#include <rte_regexdev.h>

struct mlx5_regex_priv {
	TAILQ_ENTRY(mlx5_regex_priv) next;
	struct ibv_context *ctx; /* Device context. */
	struct rte_pci_device *pci_dev;
	struct rte_regexdev *regexdev; /* Pointer to the RegEx dev. */
};

/* mlx5_rxp.c */
int mlx5_regex_info_get(struct rte_regexdev *dev,
			struct rte_regexdev_info *info);

/* mlx5_regex_devx.c */
int mlx5_devx_regex_register_write(struct ibv_context *ctx, int engine_id,
				   uint32_t addr, uint32_t data);
int mlx5_devx_regex_register_read(struct ibv_context *ctx, int engine_id,
				  uint32_t addr, uint32_t *data);

#endif /* MLX5_REGEX_H */
