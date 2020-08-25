/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_OS_H_
#define RTE_PMD_MLX5_COMMON_OS_H_

#include <stdio.h>

#include <rte_pci.h>
#include <rte_debug.h>
#include <rte_atomic.h>
#include <rte_log.h>
#include <rte_kvargs.h>
#include <rte_devargs.h>

#include "mlx5_autoconf.h"
#include "mlx5_glue.h"

/**
 * Get device name. Given an ibv_device pointer - return a
 * pointer to the corresponding device name.
 *
 * @param[in] dev
 *   Pointer to ibv device.
 *
 * @return
 *   Pointer to device name if dev is valid, NULL otherwise.
 */
static inline const char *
mlx5_os_get_dev_device_name(void *dev)
{
	if (!dev)
		return NULL;
	return ((struct ibv_device *)dev)->name;
}

/**
 * Get ibv device name. Given an ibv_context pointer - return a
 * pointer to the corresponding device name.
 *
 * @param[in] ctx
 *   Pointer to ibv context.
 *
 * @return
 *   Pointer to device name if ctx is valid, NULL otherwise.
 */
static inline const char *
mlx5_os_get_ctx_device_name(void *ctx)
{
	if (!ctx)
		return NULL;
	return ((struct ibv_context *)ctx)->device->name;
}

/**
 * Get ibv device path name. Given an ibv_context pointer - return a
 * pointer to the corresponding device path name.
 *
 * @param[in] ctx
 *   Pointer to ibv context.
 *
 * @return
 *   Pointer to device path name if ctx is valid, NULL otherwise.
 */

static inline const char *
mlx5_os_get_ctx_device_path(void *ctx)
{
	if (!ctx)
		return NULL;

	return ((struct ibv_context *)ctx)->device->ibdev_path;
}

/**
 * Get umem id. Given a pointer to umem object of type
 * 'struct mlx5dv_devx_umem *' - return its id.
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
	return ((struct mlx5dv_devx_umem *)umem)->umem_id;
}

/**
 * Get fd. Given a pointer to DevX channel object of type
 * 'struct mlx5dv_devx_event_channel*' - return its fd.
 *
 * @param[in] channel
 *    Pointer to channel object.
 *
 * @return
 *    The fd if channel is valid, 0 otherwise.
 */
static inline int
mlx5_os_get_devx_channel_fd(void *channel)
{
	if (!channel)
		return 0;
	return ((struct mlx5dv_devx_event_channel *)channel)->fd;
}

/**
 * Get mmap offset. Given a pointer to an DevX UAR object of type
 * 'struct mlx5dv_devx_uar *' - return its mmap offset.
 *
 * @param[in] uar
 *    Pointer to UAR object.
 *
 * @return
 *    The mmap offset if uar is valid, 0 otherwise.
 */
static inline off_t
mlx5_os_get_devx_uar_mmap_offset(void *uar)
{
#ifdef HAVE_MLX5DV_DEVX_UAR_OFFSET
	if (!uar)
		return 0;
	return ((struct mlx5dv_devx_uar *)uar)->mmap_off;
#else
	RTE_SET_USED(uar);
	return 0;
#endif
}

/**
 * Get base addr pointer. Given a pointer to an UAR object of type
 * 'struct mlx5dv_devx_uar *' - return its base address.
 *
 * @param[in] uar
 *    Pointer to an UAR object.
 *
 * @return
 *    The base address if UAR is valid, 0 otherwise.
 */
static inline void *
mlx5_os_get_devx_uar_base_addr(void *uar)
{
#ifdef HAVE_MLX5DV_DEVX_UAR_OFFSET
	if (!uar)
		return NULL;
	return ((struct mlx5dv_devx_uar *)uar)->base_addr;
#else
	RTE_SET_USED(uar);
	return NULL;
#endif
}

/**
 * Get reg addr pointer. Given a pointer to an UAR object of type
 * 'struct mlx5dv_devx_uar *' - return its reg address.
 *
 * @param[in] uar
 *    Pointer to an UAR object.
 *
 * @return
 *    The reg address if UAR is valid, 0 otherwise.
 */
static inline void *
mlx5_os_get_devx_uar_reg_addr(void *uar)
{
#ifdef HAVE_MLX5DV_DEVX_UAR_OFFSET
	if (!uar)
		return NULL;
	return ((struct mlx5dv_devx_uar *)uar)->reg_addr;
#else
	RTE_SET_USED(uar);
	return NULL;
#endif
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
#ifdef HAVE_MLX5DV_DEVX_UAR_OFFSET
	if (!uar)
		return 0;
	return ((struct mlx5dv_devx_uar *)uar)->page_id;
#else
	RTE_SET_USED(uar);
	return 0;
#endif
}

#endif /* RTE_PMD_MLX5_COMMON_OS_H_ */
