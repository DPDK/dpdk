/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2022-2023, Advanced Micro Devices, Inc.
 */

#ifndef CDX_PRIVATE_H
#define CDX_PRIVATE_H

#include "bus_cdx_driver.h"

/**
 * Structure describing the CDX bus.
 */
struct rte_cdx_bus {
	struct rte_bus bus;				/**< Inherit the generic class */
	RTE_TAILQ_HEAD(, rte_cdx_device) device_list;	/**< List of CDX devices */
	RTE_TAILQ_HEAD(, rte_cdx_driver) driver_list;	/**< List of CDX drivers */
};

/**
 * Map a particular resource from a file.
 *
 * @param requested_addr
 *      The starting address for the new mapping range.
 * @param fd
 *      The file descriptor.
 * @param offset
 *      The offset for the mapping range.
 * @param size
 *      The size for the mapping range.
 * @param additional_flags
 *      The additional rte_mem_map() flags for the mapping range.
 * @return
 *   - On success, the function returns a pointer to the mapped area.
 *   - On error, NULL is returned.
 */
void *cdx_map_resource(void *requested_addr, int fd, uint64_t offset,
		size_t size, int additional_flags);

/**
 * Unmap a particular resource.
 *
 * @param requested_addr
 *      The address for the unmapping range.
 * @param size
 *      The size for the unmapping range.
 */
void cdx_unmap_resource(void *requested_addr, size_t size);

/* map/unmap VFIO resource */
int cdx_vfio_map_resource(struct rte_cdx_device *dev);
int cdx_vfio_unmap_resource(struct rte_cdx_device *dev);

#endif /* CDX_PRIVATE_H */
