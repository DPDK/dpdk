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
#endif /* RTE_PMD_MLX5_COMMON_OS_H_ */
