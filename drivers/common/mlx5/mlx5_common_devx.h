/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_DEVX_H_
#define RTE_PMD_MLX5_COMMON_DEVX_H_

#include "mlx5_devx_cmds.h"

/* The standard page size */
#define MLX5_LOG_PAGE_SIZE 12

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

/* DevX Send Queue structure. */
struct mlx5_devx_sq {
	struct mlx5_devx_obj *sq; /* The SQ DevX object. */
	void *umem_obj; /* The SQ umem object. */
	union {
		volatile void *umem_buf;
		volatile struct mlx5_wqe *wqes; /* The SQ ring buffer. */
	};
	volatile uint32_t *db_rec; /* The SQ doorbell record. */
};


/* mlx5_common_devx.c */

__rte_internal
void mlx5_devx_cq_destroy(struct mlx5_devx_cq *cq);

__rte_internal
int mlx5_devx_cq_create(void *ctx, struct mlx5_devx_cq *cq_obj,
			uint16_t log_desc_n,
			struct mlx5_devx_cq_attr *attr, int socket);

__rte_internal
void mlx5_devx_sq_destroy(struct mlx5_devx_sq *sq);

__rte_internal
int mlx5_devx_sq_create(void *ctx, struct mlx5_devx_sq *sq_obj,
			uint16_t log_wqbb_n,
			struct mlx5_devx_create_sq_attr *attr, int socket);

#endif /* RTE_PMD_MLX5_COMMON_DEVX_H_ */
