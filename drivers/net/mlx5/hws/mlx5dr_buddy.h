/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5DR_BUDDY_H_
#define MLX5DR_BUDDY_H_

struct mlx5dr_buddy_mem {
	struct rte_bitmap **bits;
	unsigned int *num_free;
	uint32_t max_order;
};

struct mlx5dr_buddy_mem *mlx5dr_buddy_create(uint32_t max_order);

void mlx5dr_buddy_cleanup(struct mlx5dr_buddy_mem *buddy);

int mlx5dr_buddy_alloc_mem(struct mlx5dr_buddy_mem *buddy, int order);

void mlx5dr_buddy_free_mem(struct mlx5dr_buddy_mem *buddy, uint32_t seg, int order);

#endif /* MLX5DR_BUDDY_H_ */
