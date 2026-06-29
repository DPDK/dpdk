/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <eal_export.h>
#include <rte_errno.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <bus_driver.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_eal_paging.h>
#include <rte_lcore.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_devargs.h>

#include "private.h"

#ifndef AUXILIARY_OS_SUPPORTED
/*
 * Test whether the auxiliary device exist.
 *
 * Stub for OS not supporting auxiliary bus.
 */
bool
auxiliary_dev_exists(const char *name)
{
	RTE_SET_USED(name);
	return false;
}

/*
 * Scan the devices in the auxiliary bus.
 *
 * Stub for OS not supporting auxiliary bus.
 */
int
auxiliary_scan(void)
{
	return 0;
}
#endif /* AUXILIARY_OS_SUPPORTED */

/*
 * Update a device's devargs being scanned.
 */
void
auxiliary_on_scan(struct rte_auxiliary_device *aux_dev)
{
	aux_dev->device.devargs = rte_bus_find_devargs(&auxiliary_bus, aux_dev->name);
}

static bool
auxiliary_bus_match(const struct rte_driver *drv, const struct rte_device *dev)
{
	const struct rte_auxiliary_driver *aux_drv = RTE_BUS_DRIVER(drv, *aux_drv);
	const struct rte_auxiliary_device *aux_dev = RTE_BUS_DEVICE(dev, *aux_dev);

	if (aux_drv->match == NULL)
		return false;
	return aux_drv->match(aux_dev->name);
}

/*
 * Call the probe() function of the driver.
 */
static int
auxiliary_probe_device(struct rte_driver *drv, struct rte_device *dev)
{
	struct rte_auxiliary_device *aux_dev = RTE_BUS_DEVICE(dev, *aux_dev);
	struct rte_auxiliary_driver *aux_drv = RTE_BUS_DRIVER(drv, *aux_drv);
	enum rte_iova_mode iova_mode;
	int ret;

	if (!auxiliary_dev_exists(dev->name))
		return -ENOENT;

	if (aux_dev->device.numa_node < 0 && rte_socket_count() > 1)
		AUXILIARY_LOG(INFO, "Device %s is not NUMA-aware", aux_dev->name);

	iova_mode = rte_eal_iova_mode();
	if ((aux_drv->drv_flags & RTE_AUXILIARY_DRV_NEED_IOVA_AS_VA) > 0 &&
	    iova_mode != RTE_IOVA_VA) {
		AUXILIARY_LOG(ERR, "Driver %s expecting VA IOVA mode but current mode is PA, not initializing",
			      aux_drv->driver.name);
		return -EINVAL;
	}

	/* Allocate interrupt instance */
	aux_dev->intr_handle =
		rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
	if (aux_dev->intr_handle == NULL) {
		AUXILIARY_LOG(ERR, "Could not allocate interrupt instance for device %s",
			aux_dev->name);
		return -ENOMEM;
	}

	AUXILIARY_LOG(INFO, "Probe auxiliary driver: %s device: %s (NUMA node %i)",
		      aux_drv->driver.name, aux_dev->name, aux_dev->device.numa_node);
	ret = aux_drv->probe(aux_drv, aux_dev);
	if (ret != 0) {
		rte_intr_instance_free(aux_dev->intr_handle);
		aux_dev->intr_handle = NULL;
	}

	return ret;
}

static int
auxiliary_unplug_device(struct rte_device *rte_dev)
{
	const struct rte_auxiliary_driver *drv = RTE_BUS_DRIVER(rte_dev->driver, *drv);
	struct rte_auxiliary_device *dev = RTE_BUS_DEVICE(rte_dev, *dev);
	int ret = 0;

	AUXILIARY_LOG(DEBUG, "Driver %s remove auxiliary device %s on NUMA node %i",
		      drv->driver.name, dev->name, dev->device.numa_node);

	if (drv->remove != NULL) {
		ret = drv->remove(dev);
		if (ret < 0)
			return ret;
	}

	rte_intr_instance_free(dev->intr_handle);
	dev->intr_handle = NULL;

	return 0;
}

static int
auxiliary_parse(const char *name, void *addr)
{
	struct rte_auxiliary_driver *drv = NULL;
	const char **out = addr;

	/* Allow empty device name "auxiliary:" to bypass entire bus scan. */
	if (strlen(name) == 0)
		return 0;

	RTE_BUS_FOREACH_DRV(drv, &auxiliary_bus) {
		if (drv->match(name))
			break;
	}
	if (drv != NULL && addr != NULL)
		*out = name;
	return drv != NULL ? 0 : -1;
}

/* Register a driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_auxiliary_register)
void
rte_auxiliary_register(struct rte_auxiliary_driver *driver)
{
	rte_bus_add_driver(&auxiliary_bus, &driver->driver);
}

/* Unregister a driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_auxiliary_unregister)
void
rte_auxiliary_unregister(struct rte_auxiliary_driver *driver)
{
	rte_bus_remove_driver(&auxiliary_bus, &driver->driver);
}

static void
auxiliary_free_device(struct rte_device *dev)
{
	free(RTE_BUS_DEVICE(dev, struct rte_auxiliary_device));
}

static int
auxiliary_dma_map(struct rte_device *dev, void *addr, uint64_t iova, size_t len)
{
	struct rte_auxiliary_device *aux_dev = RTE_BUS_DEVICE(dev, *aux_dev);
	const struct rte_auxiliary_driver *aux_drv = RTE_BUS_DRIVER(dev->driver, *aux_drv);

	if (aux_drv->dma_map == NULL) {
		rte_errno = ENOTSUP;
		return -1;
	}
	return aux_drv->dma_map(aux_dev, addr, iova, len);
}

static int
auxiliary_dma_unmap(struct rte_device *dev, void *addr, uint64_t iova,
		    size_t len)
{
	struct rte_auxiliary_device *aux_dev = RTE_BUS_DEVICE(dev, *aux_dev);
	const struct rte_auxiliary_driver *aux_drv = RTE_BUS_DRIVER(dev->driver, *aux_drv);

	if (aux_drv->dma_unmap == NULL) {
		rte_errno = ENOTSUP;
		return -1;
	}
	return aux_drv->dma_unmap(aux_dev, addr, iova, len);
}

static enum rte_iova_mode
auxiliary_get_iommu_class(void)
{
	const struct rte_auxiliary_driver *drv;

	RTE_BUS_FOREACH_DRV(drv, &auxiliary_bus) {
		if ((drv->drv_flags & RTE_AUXILIARY_DRV_NEED_IOVA_AS_VA) > 0)
			return RTE_IOVA_VA;
	}

	return RTE_IOVA_DC;
}

struct rte_bus auxiliary_bus = {
	.scan = auxiliary_scan,
	.probe = rte_bus_generic_probe,
	.free_device = auxiliary_free_device,
	.cleanup = rte_bus_generic_cleanup,
	.find_device = rte_bus_generic_find_device,
	.match = auxiliary_bus_match,
	.probe_device = auxiliary_probe_device,
	.unplug_device = auxiliary_unplug_device,
	.parse = auxiliary_parse,
	.dma_map = auxiliary_dma_map,
	.dma_unmap = auxiliary_dma_unmap,
	.get_iommu_class = auxiliary_get_iommu_class,
	.dev_iterate = rte_bus_generic_dev_iterate,
};

RTE_REGISTER_BUS(auxiliary, auxiliary_bus);
RTE_LOG_REGISTER_DEFAULT(auxiliary_bus_logtype, NOTICE);
