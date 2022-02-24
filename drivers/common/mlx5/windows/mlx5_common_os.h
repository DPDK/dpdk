/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_OS_H_
#define RTE_PMD_MLX5_COMMON_OS_H_

#include <stdio.h>
#include <sys/types.h>

#include <rte_errno.h>

#include "mlx5_autoconf.h"
#include "mlx5_glue.h"
#include "mlx5_malloc.h"
#include "mlx5_common_mr.h"
#include "mlx5_win_ext.h"

#define MLX5_BF_OFFSET 0x800

/**
 * This API allocates aligned or non-aligned memory.  The free can be on either
 * aligned or nonaligned memory.  To be protected - even though there may be no
 * alignment - in Windows this API will unconditionally call _aligned_malloc()
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

/**
 * Get mmap offset. Given a pointer to an DevX UAR object of type
 * 'struct mlx5dv_devx_uar *' - return its mmap offset.
 * In Windows, mmap_offset is unused.
 *
 * @param[in] uar
 *    Pointer to UAR object.
 *
 * @return
 *    0 as mmap_offset is unused
 */
static inline off_t
mlx5_os_get_devx_uar_mmap_offset(void *uar)
{
	RTE_SET_USED(uar);
	return 0;
}

/**
 * Get base addr pointer. Given a pointer to an UAR object of type
 * 'struct mlx5dv_devx_uar *' - return its base address.
 *
 * @param[in] uar
 *    Pointer to an UAR object.
 *
 * @return
 *    The base address if UAR is valid, NULL otherwise.
 */
static inline void *
mlx5_os_get_devx_uar_base_addr(void *uar)
{
	if (!uar)
		return NULL;
	return ((devx_uar_handle *)uar)->uar_page;
}

/**
 * Get reg addr pointer. Given a pointer to an UAR object of type
 * 'struct mlx5dv_devx_uar *' - return its reg address.
 *
 * @param[in] uar
 *    Pointer to an UAR object.
 *
 * @return
 *    The reg address if UAR is valid, NULL otherwise.
 */
static inline void *
mlx5_os_get_devx_uar_reg_addr(void *uar)
{
	if (!uar)
		return NULL;
	return ((char *)((devx_uar_handle *)uar)->uar_page) + MLX5_BF_OFFSET;
}

/**
 * Get page id. Given a pointer to an UAR object of type
 * 'struct mlx5dv_devx_uar *' - return its page id.
 *
 * @param[in] uar
 *    Pointer to an UAR object.
 *
 * @return
 *    The page id if UAR is valid, 0 otherwise.
 */
static inline uint32_t
mlx5_os_get_devx_uar_page_id(void *uar)
{
	if (!uar)
		return 0;
	return ((devx_uar_handle *)uar)->uar_index;
}

static inline void *
mlx5_os_devx_create_event_channel(void *ctx, int flags)
{
	(void)ctx;
	(void)flags;
	errno = ENOTSUP;
	return NULL;
}

static inline void
mlx5_os_devx_destroy_event_channel(void *eventc)
{
	(void)eventc;
}

static inline int
mlx5_os_devx_subscribe_devx_event(void *eventc,
				    void *obj,
				    uint16_t events_sz, uint16_t events_num[],
				    uint64_t cookie)
{
	(void)eventc;
	(void)obj;
	(void)events_sz;
	(void)events_num;
	(void)cookie;
	return -ENOTSUP;
}

__rte_internal
void *mlx5_os_umem_reg(void *ctx, void *addr, size_t size, uint32_t access);
__rte_internal
int mlx5_os_umem_dereg(void *pumem);

#endif /* RTE_PMD_MLX5_COMMON_OS_H_ */
