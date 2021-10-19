/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <inttypes.h>

#include <rte_errno.h>
#include <rte_eal_paging.h>

#include "mlx5_common_utils.h"
#include "mlx5_common_log.h"
#include "mlx5_autoconf.h"
#include <mlx5_glue.h>
#include <mlx5_malloc.h>
#include <mlx5_common.h>
#include <mlx5_common_mr.h>

/**
 * Verbs callback to allocate a memory. This function should allocate the space
 * according to the size provided residing inside a huge page.
 * Please note that all allocation must respect the alignment from libmlx5
 * (i.e. currently rte_mem_page_size()).
 *
 * @param[in] size
 *   The size in bytes of the memory to allocate.
 * @param[in] data
 *   A pointer to the callback data.
 *
 * @return
 *   Allocated buffer, NULL otherwise and rte_errno is set.
 */
static void *
mlx5_alloc_verbs_buf(size_t size, void *data)
{
	struct rte_device *dev = data;
	void *ret;
	size_t alignment = rte_mem_page_size();
	if (alignment == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		rte_errno = ENOMEM;
		return NULL;
	}

	MLX5_ASSERT(data != NULL);
	ret = mlx5_malloc(0, size, alignment, dev->numa_node);
	if (!ret && size)
		rte_errno = ENOMEM;
	return ret;
}

/**
 * Verbs callback to free a memory.
 *
 * @param[in] ptr
 *   A pointer to the memory to free.
 * @param[in] data
 *   A pointer to the callback data.
 */
static void
mlx5_free_verbs_buf(void *ptr, void *data __rte_unused)
{
	MLX5_ASSERT(data != NULL);
	mlx5_free(ptr);
}

/**
 * Hint libmlx5 to use PMD allocator for data plane resources.
 *
 * @param dev
 *   Pointer to the generic device.
 */
void
mlx5_set_context_attr(struct rte_device *dev, struct ibv_context *ctx)
{
	struct mlx5dv_ctx_allocators allocator = {
		.alloc = &mlx5_alloc_verbs_buf,
		.free = &mlx5_free_verbs_buf,
		.data = dev,
	};

	/* Hint libmlx5 to use PMD allocator for data plane resources */
	mlx5_glue->dv_set_context_attr(ctx, MLX5DV_CTX_ATTR_BUF_ALLOCATORS,
				       (void *)((uintptr_t)&allocator));
}

/**
 * Register mr. Given protection domain pointer, pointer to addr and length
 * register the memory region.
 *
 * @param[in] pd
 *   Pointer to protection domain context.
 * @param[in] addr
 *   Pointer to memory start address.
 * @param[in] length
 *   Length of the memory to register.
 * @param[out] pmd_mr
 *   pmd_mr struct set with lkey, address, length and pointer to mr object
 *
 * @return
 *   0 on successful registration, -1 otherwise
 */
int
mlx5_common_verbs_reg_mr(void *pd, void *addr, size_t length,
			 struct mlx5_pmd_mr *pmd_mr)
{
	struct ibv_mr *ibv_mr;

	ibv_mr = mlx5_glue->reg_mr(pd, addr, length,
				   IBV_ACCESS_LOCAL_WRITE |
				   (haswell_broadwell_cpu ? 0 :
				   IBV_ACCESS_RELAXED_ORDERING));
	if (!ibv_mr)
		return -1;

	*pmd_mr = (struct mlx5_pmd_mr){
		.lkey = ibv_mr->lkey,
		.addr = ibv_mr->addr,
		.len = ibv_mr->length,
		.obj = (void *)ibv_mr,
	};
	return 0;
}

/**
 * Deregister mr. Given the mlx5 pmd MR - deregister the MR
 *
 * @param[in] pmd_mr
 *   pmd_mr struct set with lkey, address, length and pointer to mr object
 *
 */
void
mlx5_common_verbs_dereg_mr(struct mlx5_pmd_mr *pmd_mr)
{
	if (pmd_mr && pmd_mr->obj != NULL) {
		claim_zero(mlx5_glue->dereg_mr(pmd_mr->obj));
		memset(pmd_mr, 0, sizeof(*pmd_mr));
	}
}

/**
 * Set the reg_mr and dereg_mr callbacks.
 *
 * @param[out] reg_mr_cb
 *   Pointer to reg_mr func
 * @param[out] dereg_mr_cb
 *   Pointer to dereg_mr func
 */
void
mlx5_os_set_reg_mr_cb(mlx5_reg_mr_t *reg_mr_cb, mlx5_dereg_mr_t *dereg_mr_cb)
{
	*reg_mr_cb = mlx5_common_verbs_reg_mr;
	*dereg_mr_cb = mlx5_common_verbs_dereg_mr;
}
