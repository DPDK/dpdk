/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_errno.h>

#include "mlx5_devx_cmds.h"
#include "mlx5_common_utils.h"
#include "mlx5_common.h"

/**
 * Initialization routine for run-time dependency on external lib
 */
void
mlx5_glue_constructor(void)
{
}

/**
 * Allocate PD. Given a devx context object
 * return an mlx5-pd object.
 *
 * @param[in] ctx
 *   Pointer to context.
 *
 * @return
 *    The mlx5_pd if pd is valid, NULL and errno otherwise.
 */
void *
mlx5_os_alloc_pd(void *ctx)
{
	struct mlx5_pd *ppd =  mlx5_malloc(MLX5_MEM_ZERO,
		sizeof(struct mlx5_pd), 0, SOCKET_ID_ANY);
	if (!ppd)
		return NULL;

	struct mlx5_devx_obj *obj = mlx5_devx_cmd_alloc_pd(ctx);
	if (!obj) {
		mlx5_free(ppd);
		return NULL;
	}
	ppd->obj = obj;
	ppd->pdn = obj->id;
	ppd->devx_ctx = ctx;
	return ppd;
}

/**
 * Release PD. Releases a given mlx5_pd object
 *
 * @param[in] pd
 *   Pointer to mlx5_pd.
 *
 * @return
 *    Zero if pd is released successfully, negative number otherwise.
 */
int
mlx5_os_dealloc_pd(void *pd)
{
	if (!pd)
		return -EINVAL;
	mlx5_devx_cmd_destroy(((struct mlx5_pd *)pd)->obj);
	mlx5_free(pd);
	return 0;
}
