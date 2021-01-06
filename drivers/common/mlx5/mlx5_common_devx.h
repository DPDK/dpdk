/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_DEVX_H_
#define RTE_PMD_MLX5_COMMON_DEVX_H_

#include "mlx5_devx_cmds.h"

/* DevX Completion Queue structure. */
struct mlx5_devx_cq {
	struct mlx5_devx_obj *cq; /* The CQ DevX object. */
	void *umem_obj; /* The CQ umem object. */
	union {
		volatile void *umem_buf;
		volatile struct mlx5_cqe *cqes; /* The CQ ring buffer. */
	};
	volatile uint32_t *db_rec; /* The CQ doorbell record. */
};

/* mlx5_common_devx.c */

__rte_internal
void mlx5_devx_cq_destroy(struct mlx5_devx_cq *cq);

__rte_internal
int mlx5_devx_cq_create(void *ctx, struct mlx5_devx_cq *cq_obj,
			uint16_t log_desc_n, struct mlx5_devx_cq_attr *attr,
			int socket);

#endif /* RTE_PMD_MLX5_COMMON_DEVX_H_ */
