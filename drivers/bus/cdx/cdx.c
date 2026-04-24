/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2022-2023, Advanced Micro Devices, Inc.
 */

/*
 * Architecture Overview
 * =====================
 * CDX is a Hardware Architecture designed for AMD FPGA devices. It
 * consists of sophisticated mechanism for interaction between FPGA,
 * Firmware and the APUs (Application CPUs).
 *
 * Firmware resides on RPU (Realtime CPUs) which interacts with
 * the FPGA program manager and the APUs. The RPU provides memory-mapped
 * interface (RPU if) which is used to communicate with APUs.
 *
 * The diagram below shows an overview of the AMD CDX architecture:
 *
 *          +--------------------------------------+
 *          |   DPDK                               |
 *          |                    DPDK CDX drivers  |
 *          |                             |        |
 *          |                    DPDK AMD CDX bus  |
 *          |                             |        |
 *          +-----------------------------|--------+
 *                                        |
 *          +-----------------------------|--------+
 *          |    Application CPUs (APU)   |        |
 *          |                             |        |
 *          |                     VFIO CDX driver  |
 *          |     Linux OS                |        |
 *          |                    Linux AMD CDX bus |
 *          |                             |        |
 *          +-----------------------------|--------+
 *                                        |
 *                                        |
 *          +------------------------| RPU if |----+
 *          |                             |        |
 *          |                             V        |
 *          |          Realtime CPUs (RPU)         |
 *          |                                      |
 *          +--------------------------------------+
 *                                |
 *          +---------------------|----------------+
 *          |  FPGA               |                |
 *          |      +-----------------------+       |
 *          |      |           |           |       |
 *          | +-------+    +-------+   +-------+   |
 *          | | dev 1 |    | dev 2 |   | dev 3 |   |
 *          | +-------+    +-------+   +-------+   |
 *          +--------------------------------------+
 *
 * The RPU firmware extracts the device information from the loaded FPGA
 * image and implements a mechanism that allows the APU drivers to
 * enumerate such devices (device personality and resource details) via
 * a dedicated communication channel.
 *
 * VFIO CDX driver provides the CDX device resources like MMIO and interrupts
 * to map to user-space. DPDK CDX bus uses sysfs interface and the vfio-cdx
 * driver to discover and initialize the CDX devices for user-space
 * applications.
 */

/**
 * @file
 * CDX probing using Linux sysfs.
 */

#include <string.h>
#include <dirent.h>

#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_devargs.h>
#include <rte_kvargs.h>
#include <rte_malloc.h>
#include <rte_vfio.h>

#include <eal_export.h>
#include <eal_filesystem.h>

#include "bus_cdx_driver.h"
#include "cdx_logs.h"
#include "private.h"

#define CDX_DEV_PREFIX	"cdx-"

struct rte_cdx_bus rte_cdx_bus;

static int
cdx_get_kernel_driver_by_path(const char *filename, char *driver_name,
		size_t len)
{
	int count;
	char path[PATH_MAX];
	char *name;

	if (!filename || !driver_name)
		return -1;

	count = readlink(filename, path, PATH_MAX);
	if (count >= PATH_MAX)
		return -1;

	/* For device does not have a driver */
	if (count < 0)
		return 1;

	path[count] = '\0';

	name = strrchr(path, '/');
	if (name) {
		strlcpy(driver_name, name + 1, len);
		return 0;
	}

	return -1;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_cdx_map_device)
int rte_cdx_map_device(struct rte_cdx_device *dev)
{
	return cdx_vfio_map_resource(dev);
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_cdx_unmap_device)
void rte_cdx_unmap_device(struct rte_cdx_device *dev)
{
	cdx_vfio_unmap_resource(dev);
}

/*
 * Scan one cdx sysfs entry, and fill the devices list from it.
 * It checks if the CDX device is bound to vfio-cdx driver. In case
 * the device is vfio bound, it reads the vendor and device id and
 * stores it for device-driver matching.
 */
static int
cdx_scan_one(const char *dirname, const char *dev_name)
{
	char filename[PATH_MAX];
	struct rte_cdx_device *dev = NULL;
	char driver[PATH_MAX];
	unsigned long tmp;
	int ret;

	dev = calloc(1, sizeof(*dev));
	if (!dev)
		return -ENOMEM;

	memcpy(dev->name, dev_name, RTE_DEV_NAME_MAX_LEN);
	dev->device.name = dev->name;

	/* parse driver */
	snprintf(filename, sizeof(filename), "%s/driver", dirname);
	ret = cdx_get_kernel_driver_by_path(filename, driver, sizeof(driver));
	if (ret < 0) {
		CDX_BUS_ERR("Fail to get kernel driver");
		free(dev);
		return -1;
	}

	/* Allocate interrupt instance for cdx device */
	dev->intr_handle =
		rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
	if (dev->intr_handle == NULL) {
		CDX_BUS_ERR("Failed to create interrupt instance for %s",
			dev->device.name);
		free(dev);
		return -ENOMEM;
	}

	/*
	 * Check if device is bound to 'vfio-cdx' driver, so that user-space
	 * can gracefully access the device.
	 */
	if (ret || strcmp(driver, "vfio-cdx")) {
		ret = 0;
		goto err;
	}

	/* get vendor id */
	snprintf(filename, sizeof(filename), "%s/vendor", dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0) {
		ret = -1;
		goto err;
	}
	dev->id.vendor_id = (uint16_t)tmp;

	/* get device id */
	snprintf(filename, sizeof(filename), "%s/device", dirname);
	if (eal_parse_sysfs_value(filename, &tmp) < 0) {
		ret = -1;
		goto err;
	}
	dev->id.device_id = (uint16_t)tmp;

	rte_bus_add_device(&rte_cdx_bus.bus, &dev->device);

	return 0;

err:
	rte_intr_instance_free(dev->intr_handle);
	free(dev);
	return ret;
}

/*
 * Scan the content of the CDX bus, and the devices in the devices
 * list.
 */
static int
cdx_scan(void)
{
	struct dirent *e;
	DIR *dir;
	char dirname[PATH_MAX];

	dir = opendir(RTE_CDX_BUS_DEVICES_PATH);
	if (dir == NULL) {
		CDX_BUS_INFO("%s(): opendir failed: %s", __func__,
			strerror(errno));
		return 0;
	}

	while ((e = readdir(dir)) != NULL) {
		if (e->d_name[0] == '.')
			continue;

		if (rte_bus_device_is_ignored(&rte_cdx_bus.bus, e->d_name))
			continue;

		snprintf(dirname, sizeof(dirname), "%s/%s",
				RTE_CDX_BUS_DEVICES_PATH, e->d_name);

		if (cdx_scan_one(dirname, e->d_name) < 0)
			goto error;
	}
	closedir(dir);
	return 0;

error:
	closedir(dir);
	return -1;
}

/* map a particular resource from a file */
void *
cdx_map_resource(void *requested_addr, int fd, uint64_t offset, size_t size,
		int additional_flags)
{
	void *mapaddr;

	/* Map the cdx MMIO memory resource of device */
	mapaddr = rte_mem_map(requested_addr, size,
		RTE_PROT_READ | RTE_PROT_WRITE,
		RTE_MAP_SHARED | additional_flags, fd, offset);
	if (mapaddr == NULL) {
		CDX_BUS_ERR("%s(): cannot map resource(%d, %p, 0x%zx, 0x%"PRIx64"): %s (%p)",
			__func__, fd, requested_addr, size, offset,
			rte_strerror(rte_errno), mapaddr);
	}
	CDX_BUS_DEBUG("CDX MMIO memory mapped at %p", mapaddr);

	return mapaddr;
}

/* unmap a particular resource */
void
cdx_unmap_resource(void *requested_addr, size_t size)
{
	if (requested_addr == NULL)
		return;

	CDX_BUS_DEBUG("Unmapping CDX memory at %p", requested_addr);

	/* Unmap the CDX memory resource of device */
	if (rte_mem_unmap(requested_addr, size)) {
		CDX_BUS_ERR("%s(): cannot mem unmap(%p, %#zx): %s", __func__,
			requested_addr, size, rte_strerror(rte_errno));
	}
}

static bool
cdx_bus_match(const struct rte_driver *drv, const struct rte_device *dev)
{
	const struct rte_cdx_driver *cdx_drv = RTE_BUS_DRIVER(drv, *cdx_drv);
	const struct rte_cdx_device *cdx_dev = RTE_BUS_DEVICE(dev, *cdx_dev);
	const struct rte_cdx_id *id_table;

	for (id_table = cdx_drv->id_table; id_table->vendor_id != 0;
	     id_table++) {
		/* check if device's identifiers match the driver's ones */
		if (id_table->vendor_id != cdx_dev->id.vendor_id &&
				id_table->vendor_id != RTE_CDX_ANY_ID)
			continue;
		if (id_table->device_id != cdx_dev->id.device_id &&
				id_table->device_id != RTE_CDX_ANY_ID)
			continue;

		return true;
	}

	return false;
}

/*
 * If vendor id and device id match, call the probe() function of the
 * driver.
 */
static int
cdx_probe_device(struct rte_driver *drv, struct rte_device *dev)
{
	struct rte_cdx_device *cdx_dev = RTE_BUS_DEVICE(dev, *cdx_dev);
	struct rte_cdx_driver *cdx_drv = RTE_BUS_DRIVER(drv, *cdx_drv);
	const char *dev_name = cdx_dev->name;
	int ret;

	CDX_BUS_DEBUG("  probe device %s using driver: %s", dev_name,
		cdx_drv->driver.name);

	if (cdx_drv->drv_flags & RTE_CDX_DRV_NEED_MAPPING) {
		ret = cdx_vfio_map_resource(cdx_dev);
		if (ret != 0) {
			CDX_BUS_ERR("CDX map device failed: %d", ret);
			goto error_map_device;
		}
	}

	/* call the driver probe() function */
	ret = cdx_drv->probe(cdx_drv, cdx_dev);
	if (ret) {
		CDX_BUS_ERR("Probe CDX driver: %s device: %s failed: %d",
			cdx_drv->driver.name, dev_name, ret);
		goto error_probe;
	} else {
		cdx_dev->device.driver = &cdx_drv->driver;
	}
	cdx_dev->driver = cdx_drv;

	return ret;

error_probe:
	cdx_vfio_unmap_resource(cdx_dev);
	rte_intr_instance_free(cdx_dev->intr_handle);
	cdx_dev->intr_handle = NULL;
error_map_device:
	return ret;
}

/*
 * Scan the content of the CDX bus, and call the probe() function for
 * all registered drivers that have a matching entry in its id_table
 * for discovered devices.
 */
static int
cdx_probe(void)
{
	struct rte_cdx_device *dev = NULL;
	size_t probed = 0, failed = 0;

	RTE_BUS_FOREACH_DEV(dev, &rte_cdx_bus.bus) {
		struct rte_driver *drv = NULL;
		int ret;

		probed++;

next_driver:
		drv = rte_bus_find_driver(&rte_cdx_bus.bus, drv, &dev->device);
		if (drv == NULL)
			continue;

		if (rte_dev_is_probed(&dev->device)) {
			CDX_BUS_INFO("Device %s is already probed", dev->name);
			continue;
		}

		ret = rte_cdx_bus.bus.probe_device(drv, &dev->device);
		if (ret < 0) {
			CDX_BUS_ERR("Requested device %s cannot be used",
				dev->name);
			rte_errno = errno;
			failed++;
		} else if (ret > 0) {
			goto next_driver;
		}
	}

	return (probed && probed == failed) ? -1 : 0;
}

static int
cdx_parse(const char *name, void *addr)
{
	const char **out = addr;
	int ret;

	ret = strncmp(name, CDX_DEV_PREFIX, strlen(CDX_DEV_PREFIX));

	if (ret == 0 && addr)
		*out = name;

	return ret;
}

/* register a driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_cdx_register)
void
rte_cdx_register(struct rte_cdx_driver *driver)
{
	rte_bus_add_driver(&rte_cdx_bus.bus, &driver->driver);
}

/* unregister a driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_cdx_unregister)
void
rte_cdx_unregister(struct rte_cdx_driver *driver)
{
	rte_bus_remove_driver(&rte_cdx_bus.bus, &driver->driver);
}

/*
 * If vendor/device ID match, call the remove() function of the
 * driver.
 */
static int
cdx_detach_dev(struct rte_cdx_device *dev)
{
	struct rte_cdx_driver *dr = dev->driver;
	int ret = 0;

	CDX_BUS_DEBUG("detach device %s using driver: %s",
		dev->device.name, dr->driver.name);

	if (dr->remove != NULL) {
		ret = dr->remove(dev);
		if (ret < 0)
			return ret;
	}

	/* clear driver structure */
	dev->driver = NULL;
	dev->device.driver = NULL;

	rte_cdx_unmap_device(dev);

	rte_intr_instance_free(dev->intr_handle);
	dev->intr_handle = NULL;

	return 0;
}

static int
cdx_unplug(struct rte_device *dev)
{
	struct rte_cdx_device *cdx_dev = RTE_BUS_DEVICE(dev, *cdx_dev);
	int ret;

	ret = cdx_detach_dev(cdx_dev);
	if (ret == 0) {
		rte_bus_remove_device(&rte_cdx_bus.bus, &cdx_dev->device);
		rte_devargs_remove(dev->devargs);
		free(cdx_dev);
	}
	return ret;
}

static int
cdx_dma_map(struct rte_device *dev, void *addr, uint64_t iova, size_t len)
{
	RTE_SET_USED(dev);

	return rte_vfio_container_dma_map(RTE_VFIO_DEFAULT_CONTAINER_FD,
					  (uintptr_t)addr, iova, len);
}

static int
cdx_dma_unmap(struct rte_device *dev, void *addr, uint64_t iova, size_t len)
{
	RTE_SET_USED(dev);

	return rte_vfio_container_dma_unmap(RTE_VFIO_DEFAULT_CONTAINER_FD,
					    (uintptr_t)addr, iova, len);
}

static enum rte_iova_mode
cdx_get_iommu_class(void)
{
	if (TAILQ_EMPTY(&rte_cdx_bus.bus.device_list))
		return RTE_IOVA_DC;

	return RTE_IOVA_VA;
}

struct rte_cdx_bus rte_cdx_bus = {
	.bus = {
		.scan = cdx_scan,
		.probe = cdx_probe,
		.find_device = rte_bus_generic_find_device,
		.match = cdx_bus_match,
		.probe_device = cdx_probe_device,
		.unplug = cdx_unplug,
		.parse = cdx_parse,
		.dma_map = cdx_dma_map,
		.dma_unmap = cdx_dma_unmap,
		.get_iommu_class = cdx_get_iommu_class,
		.dev_iterate = rte_bus_generic_dev_iterate,
	},
};

RTE_REGISTER_BUS(cdx, rte_cdx_bus.bus);
RTE_LOG_REGISTER_DEFAULT(cdx_logtype_bus, NOTICE);
