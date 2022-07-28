/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Brocade Communications Systems, Inc.
 *   Author: Jan Blunck <jblunck@infradead.org>
 */

#ifndef _RTE_ETHDEV_VDEV_H_
#define _RTE_ETHDEV_VDEV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_config.h>
#include <rte_malloc.h>
#include <bus_vdev_driver.h>
#include <ethdev_driver.h>

/**
 * @internal
 * Allocates a new ethdev slot for an Ethernet device and returns the pointer
 * to that slot for the driver to use.
 *
 * @param dev
 *	Pointer to virtual device
 *
 * @param private_data_size
 *	Size of private data structure
 *
 * @return
 *	A pointer to a rte_eth_dev or NULL if allocation failed.
 */
static inline struct rte_eth_dev *
rte_eth_vdev_allocate(struct rte_vdev_device *dev, size_t private_data_size)
{
	struct rte_eth_dev *eth_dev;
	const char *name = rte_vdev_device_name(dev);

	eth_dev = rte_eth_dev_allocate(name);
	if (!eth_dev)
		return NULL;

	if (private_data_size) {
		eth_dev->data->dev_private = rte_zmalloc_socket(name,
			private_data_size, RTE_CACHE_LINE_SIZE,
			dev->device.numa_node);
		if (!eth_dev->data->dev_private) {
			rte_eth_dev_release_port(eth_dev);
			return NULL;
		}
	}

	eth_dev->device = &dev->device;
	eth_dev->intr_handle = NULL;

	eth_dev->data->numa_node = dev->device.numa_node;
	return eth_dev;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHDEV_VDEV_H_ */
