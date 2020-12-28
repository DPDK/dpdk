/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_OS_H_
#define RTE_PMD_MLX5_COMMON_OS_H_

#include <stdio.h>

#include "mlx5_autoconf.h"
#include "mlx5_glue.h"
#include "mlx5_malloc.h"

/**
 * This API allocates aligned or non-aligned memory.  The free can be on either
 * aligned or nonaligned memory.  To be protected - even though there may be no
 * alignment - in Windows this API will unconditioanlly call _aligned_malloc()
 * with at least a minimal alignment size.
 *
 * @param[in] align
 *    The alignment value, which must be an integer power of 2 (or 0 for
 *    non-alignment)
 * @param[in] size
 *    Size in bytes to allocate
 *
 * @return
 *    Valid pointer to allocated memory, NULL in case of failure
 */
static inline void *
mlx5_os_malloc(size_t align, size_t size)
{
	if (align < MLX5_MALLOC_ALIGNMENT)
		align = MLX5_MALLOC_ALIGNMENT;
	return _aligned_malloc(size, align);
}

/**
 * This API de-allocates a memory that originally could have been allocated
 * aligned or non-aligned. In Windows since the allocation was with
 * _aligned_malloc() - it is safe to always call _aligned_free().
 *
 * @param[in] addr
 *    Pointer to address to free
 *
 */
static inline void
mlx5_os_free(void *addr)
{
	_aligned_free(addr);
}

/**
 * Get fd. Given a pointer to DevX channel object of type
 * 'struct mlx5dv_devx_event_channel*' - return its fd.
 * Under Windows it is a stub.
 *
 * @param[in] channel
 *    Pointer to channel object.
 *
 * @return
 *    0
 */
static inline int
mlx5_os_get_devx_channel_fd(void *channel)
{
	if (!channel)
		return 0;
	return 0;
}

/**
 * Get device name. Given a device pointer - return a
 * pointer to the corresponding device name.
 *
 * @param[in] dev
 *   Pointer to device.
 *
 * @return
 *   Pointer to device name if dev is valid, NULL otherwise.
 */
static inline const char *
mlx5_os_get_dev_device_name(void *dev)
{
	if (!dev)
		return NULL;
	return ((struct devx_device *)dev)->name;
}

/**
 * Get device name. Given a context pointer - return a
 * pointer to the corresponding device name.
 *
 * @param[in] ctx
 *   Pointer to context.
 *
 * @return
 *   Pointer to device name if ctx is valid, NULL otherwise.
 */
static inline const char *
mlx5_os_get_ctx_device_name(void *ctx)
{
	if (!ctx)
		return NULL;
	return ((mlx5_context_st *)ctx)->mlx5_dev.name;
}

/**
 * Get a device path name. Given acontext pointer - return a
 * pointer to the corresponding device path name.
 *
 * @param[in] ctx
 *   Pointer to context.
 *
 * @return
 *   Pointer to device path name if ctx is valid, NULL otherwise.
 */

static inline const char *
mlx5_os_get_ctx_device_path(void *ctx)
{
	if (!ctx)
		return NULL;
	return ((mlx5_context_st *)ctx)->mlx5_dev.dev_pnp_id;
}

/**
 * Get umem id. Given a pointer to umem object of type return its id.
 *
 * @param[in] umem
 *    Pointer to umem object.
 *
 * @return
 *    The umem id if umem is valid, 0 otherwise.
 */
static inline uint32_t
mlx5_os_get_umem_id(void *umem)
{
	if (!umem)
		return 0;
	return ((struct mlx5_devx_umem *)umem)->umem_id;
}

void *mlx5_os_alloc_pd(void *ctx);
int mlx5_os_dealloc_pd(void *pd);
#endif /* RTE_PMD_MLX5_COMMON_OS_H_ */
