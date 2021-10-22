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
#include <rte_errno.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_bus.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_eal_paging.h>
#include <rte_lcore.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_devargs.h>

#include "private.h"
#include "rte_bus_auxiliary.h"

static struct rte_devargs *
auxiliary_devargs_lookup(const char *name)
{
	struct rte_devargs *devargs;

	RTE_EAL_DEVARGS_FOREACH(RTE_BUS_AUXILIARY_NAME, devargs) {
		if (strcmp(devargs->name, name) == 0)
			return devargs;
	}
	return NULL;
}

/*
 * Test whether the auxiliary device exist.
 *
 * Stub for OS not supporting auxiliary bus.
 */
__rte_weak bool
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
__rte_weak int
auxiliary_scan(void)
{
	return 0;
}

/*
 * Update a device's devargs being scanned.
 */
void
auxiliary_on_scan(struct rte_auxiliary_device *aux_dev)
{
	aux_dev->device.devargs = auxiliary_devargs_lookup(aux_dev->name);
}

/*
 * Match the auxiliary driver and device using driver function.
 */
bool
auxiliary_match(const struct rte_auxiliary_driver *aux_drv,
		const struct rte_auxiliary_device *aux_dev)
{
	if (aux_drv->match == NULL)
		return false;
	return aux_drv->match(aux_dev->name);
}

/*
 * Call the probe() function of the driver.
 */
static int
rte_auxiliary_probe_one_driver(struct rte_auxiliary_driver *drv,
			       struct rte_auxiliary_device *dev)
{
	enum rte_iova_mode iova_mode;
	int ret;

	if (drv == NULL || dev == NULL)
		return -EINVAL;

	/* Check if driver supports it. */
	if (!auxiliary_match(drv, dev))
		/* Match of device and driver failed */
		return 1;

	/* No initialization when marked as blocked, return without error. */
	if (dev->device.devargs != NULL &&
	    dev->device.devargs->policy == RTE_DEV_BLOCKED) {
		AUXILIARY_LOG(INFO, "Device is blocked, not initializing");
		return -1;
	}

	if (dev->device.numa_node < 0) {
		if (rte_socket_count() > 1)
			AUXILIARY_LOG(INFO, "Device %s is not NUMA-aware, defaulting socket to 0",
					dev->name);
		dev->device.numa_node = 0;
	}

	iova_mode = rte_eal_iova_mode();
	if ((drv->drv_flags & RTE_AUXILIARY_DRV_NEED_IOVA_AS_VA) > 0 &&
	    iova_mode != RTE_IOVA_VA) {
		AUXILIARY_LOG(ERR, "Driver %s expecting VA IOVA mode but current mode is PA, not initializing",
			      drv->driver.name);
		return -EINVAL;
	}

	/* Allocate interrupt instance */
	dev->intr_handle =
		rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
	if (dev->intr_handle == NULL) {
		AUXILIARY_LOG(ERR, "Could not allocate interrupt instance for device %s",
			dev->name);
		return -ENOMEM;
	}

	dev->driver = drv;

	AUXILIARY_LOG(INFO, "Probe auxiliary driver: %s device: %s (NUMA node %i)",
		      drv->driver.name, dev->name, dev->device.numa_node);
	ret = drv->probe(drv, dev);
	if (ret != 0) {
		dev->driver = NULL;
		rte_intr_instance_free(dev->intr_handle);
		dev->intr_handle = NULL;
	} else {
		dev->device.driver = &drv->driver;
	}

	return ret;
}

/*
 * Call the remove() function of the driver.
 */
static int
rte_auxiliary_driver_remove_dev(struct rte_auxiliary_device *dev)
{
	struct rte_auxiliary_driver *drv;
	int ret = 0;

	if (dev == NULL)
		return -EINVAL;

	drv = dev->driver;

	AUXILIARY_LOG(DEBUG, "Driver %s remove auxiliary device %s on NUMA node %i",
		      drv->driver.name, dev->name, dev->device.numa_node);

	if (drv->remove != NULL) {
		ret = drv->remove(dev);
		if (ret < 0)
			return ret;
	}

	/* clear driver structure */
	dev->driver = NULL;
	dev->device.driver = NULL;

	return 0;
}

/*
 * Call the probe() function of all registered drivers for the given device.
 * Return < 0 if initialization failed.
 * Return 1 if no driver is found for this device.
 */
static int
auxiliary_probe_all_drivers(struct rte_auxiliary_device *dev)
{
	struct rte_auxiliary_driver *drv;
	int rc;

	if (dev == NULL)
		return -EINVAL;

	FOREACH_DRIVER_ON_AUXILIARY_BUS(drv) {
		if (!drv->match(dev->name))
			continue;

		rc = rte_auxiliary_probe_one_driver(drv, dev);
		if (rc < 0)
			/* negative value is an error */
			return rc;
		if (rc > 0)
			/* positive value means driver doesn't support it */
			continue;
		return 0;
	}
	return 1;
}

/*
 * Scan the content of the auxiliary bus, and call the probe function for
 * all registered drivers to try to probe discovered devices.
 */
static int
auxiliary_probe(void)
{
	struct rte_auxiliary_device *dev = NULL;
	size_t probed = 0, failed = 0;
	int ret = 0;

	FOREACH_DEVICE_ON_AUXILIARY_BUS(dev) {
		probed++;

		ret = auxiliary_probe_all_drivers(dev);
		if (ret < 0) {
			if (ret != -EEXIST) {
				AUXILIARY_LOG(ERR, "Requested device %s cannot be used",
					      dev->name);
				rte_errno = errno;
				failed++;
			}
			ret = 0;
		}
	}

	return (probed && probed == failed) ? -1 : 0;
}

static int
auxiliary_parse(const char *name, void *addr)
{
	struct rte_auxiliary_driver *drv = NULL;
	const char **out = addr;

	/* Allow empty device name "auxiliary:" to bypass entire bus scan. */
	if (strlen(name) == 0)
		return 0;

	FOREACH_DRIVER_ON_AUXILIARY_BUS(drv) {
		if (drv->match(name))
			break;
	}
	if (drv != NULL && addr != NULL)
		*out = name;
	return drv != NULL ? 0 : -1;
}

/* Register a driver */
void
rte_auxiliary_register(struct rte_auxiliary_driver *driver)
{
	TAILQ_INSERT_TAIL(&auxiliary_bus.driver_list, driver, next);
	driver->bus = &auxiliary_bus;
}

/* Unregister a driver */
void
rte_auxiliary_unregister(struct rte_auxiliary_driver *driver)
{
	TAILQ_REMOVE(&auxiliary_bus.driver_list, driver, next);
	driver->bus = NULL;
}

/* Add a device to auxiliary bus */
void
auxiliary_add_device(struct rte_auxiliary_device *aux_dev)
{
	TAILQ_INSERT_TAIL(&auxiliary_bus.device_list, aux_dev, next);
}

/* Insert a device into a predefined position in auxiliary bus */
void
auxiliary_insert_device(struct rte_auxiliary_device *exist_aux_dev,
			struct rte_auxiliary_device *new_aux_dev)
{
	TAILQ_INSERT_BEFORE(exist_aux_dev, new_aux_dev, next);
}

/* Remove a device from auxiliary bus */
static void
rte_auxiliary_remove_device(struct rte_auxiliary_device *auxiliary_dev)
{
	TAILQ_REMOVE(&auxiliary_bus.device_list, auxiliary_dev, next);
}

static struct rte_device *
auxiliary_find_device(const struct rte_device *start, rte_dev_cmp_t cmp,
		      const void *data)
{
	const struct rte_auxiliary_device *pstart;
	struct rte_auxiliary_device *adev;

	if (start != NULL) {
		pstart = RTE_DEV_TO_AUXILIARY_CONST(start);
		adev = TAILQ_NEXT(pstart, next);
	} else {
		adev = TAILQ_FIRST(&auxiliary_bus.device_list);
	}
	while (adev != NULL) {
		if (cmp(&adev->device, data) == 0)
			return &adev->device;
		adev = TAILQ_NEXT(adev, next);
	}
	return NULL;
}

static int
auxiliary_plug(struct rte_device *dev)
{
	if (!auxiliary_dev_exists(dev->name))
		return -ENOENT;
	return auxiliary_probe_all_drivers(RTE_DEV_TO_AUXILIARY(dev));
}

static int
auxiliary_unplug(struct rte_device *dev)
{
	struct rte_auxiliary_device *adev;
	int ret;

	adev = RTE_DEV_TO_AUXILIARY(dev);
	ret = rte_auxiliary_driver_remove_dev(adev);
	if (ret == 0) {
		rte_auxiliary_remove_device(adev);
		rte_devargs_remove(dev->devargs);
		rte_intr_instance_free(adev->intr_handle);
		free(adev);
	}
	return ret;
}

static int
auxiliary_dma_map(struct rte_device *dev, void *addr, uint64_t iova, size_t len)
{
	struct rte_auxiliary_device *aux_dev = RTE_DEV_TO_AUXILIARY(dev);

	if (dev == NULL || aux_dev->driver == NULL) {
		rte_errno = EINVAL;
		return -1;
	}
	if (aux_dev->driver->dma_map == NULL) {
		rte_errno = ENOTSUP;
		return -1;
	}
	return aux_dev->driver->dma_map(aux_dev, addr, iova, len);
}

static int
auxiliary_dma_unmap(struct rte_device *dev, void *addr, uint64_t iova,
		    size_t len)
{
	struct rte_auxiliary_device *aux_dev = RTE_DEV_TO_AUXILIARY(dev);

	if (dev == NULL || aux_dev->driver == NULL) {
		rte_errno = EINVAL;
		return -1;
	}
	if (aux_dev->driver->dma_unmap == NULL) {
		rte_errno = ENOTSUP;
		return -1;
	}
	return aux_dev->driver->dma_unmap(aux_dev, addr, iova, len);
}

bool
auxiliary_is_ignored_device(const char *name)
{
	struct rte_devargs *devargs = auxiliary_devargs_lookup(name);

	switch (auxiliary_bus.bus.conf.scan_mode) {
	case RTE_BUS_SCAN_ALLOWLIST:
		if (devargs && devargs->policy == RTE_DEV_ALLOWED)
			return false;
		break;
	case RTE_BUS_SCAN_UNDEFINED:
	case RTE_BUS_SCAN_BLOCKLIST:
		if (devargs == NULL || devargs->policy != RTE_DEV_BLOCKED)
			return false;
		break;
	}
	return true;
}

static enum rte_iova_mode
auxiliary_get_iommu_class(void)
{
	const struct rte_auxiliary_driver *drv;

	FOREACH_DRIVER_ON_AUXILIARY_BUS(drv) {
		if ((drv->drv_flags & RTE_AUXILIARY_DRV_NEED_IOVA_AS_VA) > 0)
			return RTE_IOVA_VA;
	}

	return RTE_IOVA_DC;
}

struct rte_auxiliary_bus auxiliary_bus = {
	.bus = {
		.scan = auxiliary_scan,
		.probe = auxiliary_probe,
		.find_device = auxiliary_find_device,
		.plug = auxiliary_plug,
		.unplug = auxiliary_unplug,
		.parse = auxiliary_parse,
		.dma_map = auxiliary_dma_map,
		.dma_unmap = auxiliary_dma_unmap,
		.get_iommu_class = auxiliary_get_iommu_class,
		.dev_iterate = auxiliary_dev_iterate,
	},
	.device_list = TAILQ_HEAD_INITIALIZER(auxiliary_bus.device_list),
	.driver_list = TAILQ_HEAD_INITIALIZER(auxiliary_bus.driver_list),
};

RTE_REGISTER_BUS(auxiliary, auxiliary_bus.bus);
RTE_LOG_REGISTER_DEFAULT(auxiliary_bus_logtype, NOTICE);
