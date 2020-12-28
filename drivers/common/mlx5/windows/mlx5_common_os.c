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

/**
 * Register umem.
 *
 * @param[in] ctx
 *   Pointer to context.
 * @param[in] addr
 *   Pointer to memory start address.
 * @param[in] size
 *   Size of the memory to register.
 * @param[out] access
 *   UMEM access type
 *
 * @return
 *   umem on successful registration, NULL and errno otherwise
 */
void *
mlx5_os_umem_reg(void *ctx, void *addr, size_t size, uint32_t access)
{
	struct mlx5_devx_umem *umem;

	umem = mlx5_malloc(MLX5_MEM_ZERO,
		(sizeof(*umem)), 0, SOCKET_ID_ANY);
	if (!umem) {
		errno = ENOMEM;
		return NULL;
	}
	umem->umem_hdl = mlx5_glue->devx_umem_reg(ctx, addr, size, access,
		&umem->umem_id);
	if (!umem->umem_hdl) {
		mlx5_free(umem);
		return NULL;
	}
	umem->addr = addr;
	return umem;
}

/**
 * Deregister umem.
 *
 * @param[in] pumem
 *   Pointer to umem.
 *
 * @return
 *   0 on successful release, negative number otherwise
 */
int
mlx5_os_umem_dereg(void *pumem)
{
	struct mlx5_devx_umem *umem;
	int err = 0;

	if (!pumem)
		return err;
	umem = pumem;
	if (umem->umem_hdl)
		err = mlx5_glue->devx_umem_dereg(umem->umem_hdl);
	mlx5_free(umem);
	return err;
}
