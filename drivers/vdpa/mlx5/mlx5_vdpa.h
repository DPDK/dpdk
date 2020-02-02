/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_VDPA_H_
#define RTE_PMD_MLX5_VDPA_H_

#include <sys/queue.h>

#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <rte_vdpa.h>
#include <rte_vhost.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>

struct mlx5_vdpa_query_mr {
	SLIST_ENTRY(mlx5_vdpa_query_mr) next;
	void *addr;
	uint64_t length;
	struct mlx5dv_devx_umem *umem;
	struct mlx5_devx_obj *mkey;
	int is_indirect;
};

struct mlx5_vdpa_priv {
	TAILQ_ENTRY(mlx5_vdpa_priv) next;
	int id; /* vDPA device id. */
	int vid; /* vhost device id. */
	struct ibv_context *ctx; /* Device context. */
	struct rte_vdpa_dev_addr dev_addr;
	struct mlx5_hca_vdpa_attr caps;
	uint32_t pdn; /* Protection Domain number. */
	struct ibv_pd *pd;
	uint32_t gpa_mkey_index;
	struct ibv_mr *null_mr;
	struct rte_vhost_memory *vmem;
	SLIST_HEAD(mr_list, mlx5_vdpa_query_mr) mr_list;
};

/**
 * Release all the prepared memory regions and all their related resources.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 */
void mlx5_vdpa_mem_dereg(struct mlx5_vdpa_priv *priv);

/**
 * Register all the memory regions of the virtio device to the HW and allocate
 * all their related resources.
 *
 * @param[in] priv
 *   The vdpa driver private structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int mlx5_vdpa_mem_register(struct mlx5_vdpa_priv *priv);

#endif /* RTE_PMD_MLX5_VDPA_H_ */
