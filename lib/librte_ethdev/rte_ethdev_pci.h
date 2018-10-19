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

#ifndef _RTE_ETHDEV_PCI_H_
#define _RTE_ETHDEV_PCI_H_

#include <rte_malloc.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_config.h>
#include <rte_ethdev_driver.h>

/**
 * Copy pci device info to the Ethernet device data.
 *
 * @param eth_dev
 * The *eth_dev* pointer is the address of the *rte_eth_dev* structure.
 * @param pci_dev
 * The *pci_dev* pointer is the address of the *rte_pci_device* structure.
 */
static inline void
rte_eth_copy_pci_info(struct rte_eth_dev *eth_dev,
	struct rte_pci_device *pci_dev)
{
	if ((eth_dev == NULL) || (pci_dev == NULL)) {
		RTE_ETHDEV_LOG(ERR, "NULL pointer eth_dev=%p pci_dev=%p",
			(void *)eth_dev, (void *)pci_dev);
		return;
	}

	eth_dev->intr_handle = &pci_dev->intr_handle;

	eth_dev->data->dev_flags = 0;
	if (pci_dev->driver->drv_flags & RTE_PCI_DRV_INTR_LSC)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_INTR_LSC;
	if (pci_dev->driver->drv_flags & RTE_PCI_DRV_INTR_RMV)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_INTR_RMV;

	eth_dev->data->kdrv = pci_dev->kdrv;
	eth_dev->data->numa_node = pci_dev->device.numa_node;
}

static inline int
eth_dev_pci_specific_init(struct rte_eth_dev *eth_dev, void *bus_device) {
	struct rte_pci_device *pci_dev = bus_device;

	if (!pci_dev)
		return -ENODEV;

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	return 0;
}

/**
 * @internal
 * Allocates a new ethdev slot for an ethernet device and returns the pointer
 * to that slot for the driver to use.
 *
 * @param dev
 *	Pointer to the PCI device
 *
 * @param private_data_size
 *	Size of private data structure
 *
 * @return
 *	A pointer to a rte_eth_dev or NULL if allocation failed.
 */
static inline struct rte_eth_dev *
rte_eth_dev_pci_allocate(struct rte_pci_device *dev, size_t private_data_size)
{
	struct rte_eth_dev *eth_dev;
	const char *name;

	if (!dev)
		return NULL;

	name = dev->device.name;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
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
	} else {
		eth_dev = rte_eth_dev_attach_secondary(name);
		if (!eth_dev)
			return NULL;
	}

	eth_dev->device = &dev->device;
	rte_eth_copy_pci_info(eth_dev, dev);
	return eth_dev;
}

static inline void
rte_eth_dev_pci_release(struct rte_eth_dev *eth_dev)
{
	eth_dev->device = NULL;
	eth_dev->intr_handle = NULL;

	/* free ether device */
	rte_eth_dev_release_port(eth_dev);
}

typedef int (*eth_dev_pci_callback_t)(struct rte_eth_dev *eth_dev);

/**
 * @internal
 * Wrapper for use by pci drivers in a .probe function to attach to a ethdev
 * interface.
 */
static inline int
rte_eth_dev_pci_generic_probe(struct rte_pci_device *pci_dev,
	size_t private_data_size, eth_dev_pci_callback_t dev_init)
{
	struct rte_eth_dev *eth_dev;
	int ret;

	eth_dev = rte_eth_dev_pci_allocate(pci_dev, private_data_size);
	if (!eth_dev)
		return -ENOMEM;

	RTE_FUNC_PTR_OR_ERR_RET(*dev_init, -EINVAL);
	ret = dev_init(eth_dev);
	if (ret)
		rte_eth_dev_pci_release(eth_dev);
	else
		rte_eth_dev_probing_finish(eth_dev);

	return ret;
}

/**
 * @internal
 * Wrapper for use by pci drivers in a .remove function to detach a ethdev
 * interface.
 */
static inline int
rte_eth_dev_pci_generic_remove(struct rte_pci_device *pci_dev,
	eth_dev_pci_callback_t dev_uninit)
{
	struct rte_eth_dev *eth_dev;
	int ret;

	eth_dev = rte_eth_dev_allocated(pci_dev->device.name);
	if (!eth_dev)
		return -ENODEV;

	if (dev_uninit) {
		ret = dev_uninit(eth_dev);
		if (ret)
			return ret;
	}

	rte_eth_dev_pci_release(eth_dev);
	return 0;
}

#endif /* _RTE_ETHDEV_PCI_H_ */
