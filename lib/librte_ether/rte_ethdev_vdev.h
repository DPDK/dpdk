/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Brocade Communications Systems, Inc.
 *   Author: Jan Blunck <jblunck@infradead.org>
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_ETHDEV_VDEV_H_
#define _RTE_ETHDEV_VDEV_H_

#include <rte_config.h>
#include <rte_malloc.h>
#include <rte_bus_vdev.h>
#include <rte_ethdev_driver.h>

/**
 * @internal
 * Allocates a new ethdev slot for an ethernet device and returns the pointer
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

	eth_dev->data->kdrv = RTE_KDRV_NONE;
	eth_dev->data->numa_node = dev->device.numa_node;
	return eth_dev;
}

#endif /* _RTE_ETHDEV_VDEV_H_ */
