/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMMON_OS_H_
#define RTE_PMD_MLX5_COMMON_OS_H_

#include <stdio.h>
#include <malloc.h>

#include <rte_compat.h>
#include <rte_pci.h>
#include <bus_pci_driver.h>
#include <rte_debug.h>
#include <rte_atomic.h>
#include <rte_log.h>
#include <rte_kvargs.h>
#include <rte_devargs.h>
#include <rte_interrupts.h>

#include "mlx5_autoconf.h"
#include "mlx5_glue.h"
#include "mlx5_malloc.h"

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

__rte_internal
static inline void *
mlx5_os_umem_reg(void *ctx, void *addr, size_t size, uint32_t access)
{
	return mlx5_glue->devx_umem_reg(ctx, addr, size, access);
}

__rte_internal
static inline int
mlx5_os_umem_dereg(void *pumem)
{
	return mlx5_glue->devx_umem_dereg(pumem);
}

static inline void *
mlx5_os_devx_create_event_channel(void *ctx, int flags)
{
	return mlx5_glue->devx_create_event_channel(ctx, flags);
}

static inline void
mlx5_os_devx_destroy_event_channel(void *eventc)
{
	mlx5_glue->devx_destroy_event_channel(eventc);
}

static inline int
mlx5_os_devx_subscribe_devx_event(void *eventc,
				  void *obj,
				  uint16_t events_sz, uint16_t events_num[],
				  uint64_t cookie)
{
	return mlx5_glue->devx_subscribe_devx_event(eventc, obj, events_sz,
						    events_num, cookie);
}

/**
 * Memory allocation optionally with alignment.
 *
 * @param[in] align
 *    Alignment size (may be zero)
 * @param[in] size
 *    Size in bytes to allocate
 *
 * @return
 *    Valid pointer to allocated memory, NULL in case of failure
 */
static inline void *
mlx5_os_malloc(size_t align, size_t size)
{
	void *buf;

	if (posix_memalign(&buf, align, size))
		return NULL;
	return buf;
}

/**
 * This API de-allocates a memory that originally could have been
 * allocated aligned or non-aligned. In Linux it is a wrapper
 * around free().
 *
 * @param[in] addr
 *    Pointer to address to free
 *
 */
static inline void
mlx5_os_free(void *addr)
{
	free(addr);
}

void
mlx5_set_context_attr(struct rte_device *dev, struct ibv_context *ctx);

/**
 * This is used to query system_image_guid as describing in PRM.
 *
 * @param dev[in]
 *  Pointer to a device instance as PCIe id.
 * @param guid[out]
 *  Pointer to the buffer to hold device guid.
 *  Guid is uint64_t and corresponding to 17 bytes string.
 * @param len[in]
 *  Guid buffer length, 17 bytes at least.
 *
 * @return
 *  -1 if internal failure.
 *  0 if OFED doesn't support.
 *  >0 if success.
 */
__rte_internal
int
mlx5_get_device_guid(const struct rte_pci_addr *dev, uint8_t *guid, size_t len);

__rte_internal
struct rte_intr_handle *
mlx5_os_interrupt_handler_create(int mode, bool set_fd_nonblock, int fd,
				 rte_intr_callback_fn cb, void *cb_arg);

__rte_internal
void
mlx5_os_interrupt_handler_destroy(struct rte_intr_handle *intr_handle,
				  rte_intr_callback_fn cb, void *cb_arg);

#endif /* RTE_PMD_MLX5_COMMON_OS_H_ */
