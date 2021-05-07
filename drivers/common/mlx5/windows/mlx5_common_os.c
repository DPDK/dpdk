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
#include "../mlx5_common_log.h"
#include "mlx5_common.h"
#include "mlx5_common_os.h"
#include "mlx5_malloc.h"

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

/**
 * Register mr. Given protection doamin pointer, pointer to addr and length
 * register the memory region.
 *
 * @param[in] pd
 *   Pointer to protection domain context (type mlx5_pd).
 * @param[in] addr
 *   Pointer to memory start address (type devx_device_ctx).
 * @param[in] length
 *   Lengtoh of the memory to register.
 * @param[out] pmd_mr
 *   pmd_mr struct set with lkey, address, length, pointer to mr object, mkey
 *
 * @return
 *   0 on successful registration, -1 otherwise
 */
int
mlx5_os_reg_mr(void *pd,
	       void *addr, size_t length, struct mlx5_pmd_mr *pmd_mr)
{
	struct mlx5_devx_mkey_attr mkey_attr;
	struct mlx5_pd *mlx5_pd = (struct mlx5_pd *)pd;
	struct mlx5_hca_attr attr;
	struct mlx5_devx_obj *mkey;
	void *obj;

	if (!pd || !addr) {
		rte_errno = EINVAL;
		return -1;
	}
	if (mlx5_devx_cmd_query_hca_attr(mlx5_pd->devx_ctx, &attr))
		return -1;
	obj = mlx5_os_umem_reg(mlx5_pd->devx_ctx, addr, length,
			       IBV_ACCESS_LOCAL_WRITE);
	if (!obj)
		return -1;
	memset(&mkey_attr, 0, sizeof(mkey_attr));
	mkey_attr.addr = (uintptr_t)addr;
	mkey_attr.size = length;
	mkey_attr.umem_id = ((struct mlx5_devx_umem *)(obj))->umem_id;
	mkey_attr.pd = mlx5_pd->pdn;
	if (!haswell_broadwell_cpu) {
		mkey_attr.relaxed_ordering_write = attr.relaxed_ordering_write;
		mkey_attr.relaxed_ordering_read = attr.relaxed_ordering_read;
	}
	mkey = mlx5_devx_cmd_mkey_create(mlx5_pd->devx_ctx, &mkey_attr);
	if (!mkey) {
		claim_zero(mlx5_os_umem_dereg(obj));
		return -1;
	}
	pmd_mr->addr = addr;
	pmd_mr->len = length;
	pmd_mr->obj = obj;
	pmd_mr->mkey = mkey;
	pmd_mr->lkey = pmd_mr->mkey->id;
	return 0;
}

/**
 * De-register mr.
 *
 * @param[in] pmd_mr
 *  Pointer to PMD mr object
 */
void
mlx5_os_dereg_mr(struct mlx5_pmd_mr *pmd_mr)
{
	if (pmd_mr && pmd_mr->mkey)
		claim_zero(mlx5_glue->devx_obj_destroy(pmd_mr->mkey->obj));
	if (pmd_mr && pmd_mr->obj)
		claim_zero(mlx5_os_umem_dereg(pmd_mr->obj));
	memset(pmd_mr, 0, sizeof(*pmd_mr));
}
