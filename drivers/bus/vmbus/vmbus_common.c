/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018, Microsoft Corporation.
 * All Rights Reserved.
 */

#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/queue.h>
#include <sys/mman.h>

#include <rte_log.h>
#include <rte_eal.h>
#include <rte_tailq.h>
#include <rte_devargs.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_errno.h>
#include <rte_memory.h>
#include <rte_bus_vmbus.h>

#include "private.h"

extern struct rte_vmbus_bus rte_vmbus_bus;

/* map a particular resource from a file */
void *
vmbus_map_resource(void *requested_addr, int fd, off_t offset, size_t size,
		   int flags)
{
	void *mapaddr;

	/* Map the memory resource of device */
	mapaddr = mmap(requested_addr, size, PROT_READ | PROT_WRITE,
		       MAP_SHARED | flags, fd, offset);
	if (mapaddr == MAP_FAILED) {
		VMBUS_LOG(ERR,
			  "mmap(%d, %p, %zu, %ld) failed: %s",
			  fd, requested_addr, size, (long)offset,
			  strerror(errno));
	} else {
		VMBUS_LOG(DEBUG, "  VMBUS memory mapped at %p",
			  mapaddr);
	}
	return mapaddr;
}

/* unmap a particular resource */
void
vmbus_unmap_resource(void *requested_addr, size_t size)
{
	if (requested_addr == NULL)
		return;

	/* Unmap the VMBUS memory resource of device */
	if (munmap(requested_addr, size)) {
		VMBUS_LOG(ERR, "munmap(%p, 0x%lx) failed: %s",
			requested_addr, (unsigned long)size,
			strerror(errno));
	} else {
		VMBUS_LOG(DEBUG, "  VMBUS memory unmapped at %p",
			  requested_addr);
	}
}

/**
 * Match the VMBUS driver and device using UUID table
 *
 * @param drv
 *	VMBUS driver from which ID table would be extracted
 * @param pci_dev
 *	VMBUS device to match against the driver
 * @return
 *	true for successful match
 *	false for unsuccessful match
 */
static bool
vmbus_match(const struct rte_vmbus_driver *dr,
	    const struct rte_vmbus_device *dev)
{
	const rte_uuid_t *id_table;

	for (id_table = dr->id_table; !rte_uuid_is_null(*id_table); ++id_table) {
		if (rte_uuid_compare(*id_table, dev->class_id) == 0)
			return true;
	}

	return false;
}
/*
 * If device ID match, call the devinit() function of the driver.
 */
static int
vmbus_probe_one_driver(struct rte_vmbus_driver *dr,
		       struct rte_vmbus_device *dev)
{
	char guid[RTE_UUID_STRLEN];
	int ret;

	if (!vmbus_match(dr, dev))
		return 1;	 /* not supported */

	rte_uuid_unparse(dev->device_id, guid, sizeof(guid));
	VMBUS_LOG(INFO, "VMBUS device %s on NUMA socket %i",
		  guid, dev->device.numa_node);

	/* no initialization when marked as blocked, return without error */
	if (dev->device.devargs != NULL &&
		dev->device.devargs->policy == RTE_DEV_BLOCKED) {
		VMBUS_LOG(INFO, "  Device is blocked, not initializing\n");
		return 1;
	}

	/* map resources for device */
	ret = rte_vmbus_map_device(dev);
	if (ret != 0)
		return ret;

	/* reference driver structure */
	dev->driver = dr;

	if (dev->device.numa_node < 0 && rte_socket_count() > 1)
		VMBUS_LOG(INFO, "Device %s is not NUMA-aware", guid);

	/* call the driver probe() function */
	VMBUS_LOG(INFO, "  probe driver: %s", dr->driver.name);
	ret = dr->probe(dr, dev);
	if (ret) {
		dev->driver = NULL;
		rte_vmbus_unmap_device(dev);
	} else {
		dev->device.driver = &dr->driver;
	}

	return ret;
}

/*
 * If device class GUID matches, call the probe function of
 * register drivers for the vmbus device.
 * Return -1 if initialization failed,
 * and 1 if no driver found for this device.
 */
static int
vmbus_probe_all_drivers(struct rte_vmbus_device *dev)
{
	struct rte_vmbus_driver *dr;
	int rc;

	/* Check if a driver is already loaded */
	if (rte_dev_is_probed(&dev->device)) {
		VMBUS_LOG(DEBUG, "VMBUS driver already loaded");
		return 0;
	}

	FOREACH_DRIVER_ON_VMBUS(dr) {
		rc = vmbus_probe_one_driver(dr, dev);
		if (rc < 0) /* negative is an error */
			return -1;

		if (rc > 0) /* positive driver doesn't support it */
			continue;

		return 0;
	}
	return 1;
}

static bool
vmbus_ignore_device(struct rte_vmbus_device *dev)
{
	struct rte_devargs *devargs = vmbus_devargs_lookup(dev);

	switch (rte_vmbus_bus.bus.conf.scan_mode) {
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

/*
 * Scan the vmbus, and call the devinit() function for
 * all registered drivers that have a matching entry in its id_table
 * for discovered devices.
 */
int
rte_vmbus_probe(void)
{
	struct rte_vmbus_device *dev;
	size_t probed = 0, failed = 0;
	char ubuf[RTE_UUID_STRLEN];

	FOREACH_DEVICE_ON_VMBUS(dev) {
		probed++;

		rte_uuid_unparse(dev->device_id, ubuf, sizeof(ubuf));

		if (vmbus_ignore_device(dev))
			continue;

		if (vmbus_probe_all_drivers(dev) < 0) {
			VMBUS_LOG(NOTICE,
				"Requested device %s cannot be used", ubuf);
			rte_errno = errno;
			failed++;
		}
	}

	return (probed && probed == failed) ? -1 : 0;
}

static int
rte_vmbus_cleanup(void)
{
	struct rte_vmbus_device *dev, *tmp_dev;
	int error = 0;

	RTE_TAILQ_FOREACH_SAFE(dev, &rte_vmbus_bus.device_list, next, tmp_dev) {
		const struct rte_vmbus_driver *drv = dev->driver;
		int ret;

		if (drv == NULL || drv->remove == NULL)
			continue;

		ret = drv->remove(dev);
		if (ret < 0)
			error = -1;

		rte_vmbus_unmap_device(dev);

		dev->driver = NULL;
		dev->device.driver = NULL;
		free(dev);
	}

	return error;
}

static int
vmbus_parse(const char *name, void *addr)
{
	rte_uuid_t guid;
	int ret;

	ret = rte_uuid_parse(name, guid);
	if (ret == 0 && addr)
		memcpy(addr, &guid, sizeof(guid));

	return ret;
}

/*
 * scan for matching device args on command line
 * example:
 *	-a 'vmbus:635a7ae3-091e-4410-ad59-667c4f8c04c3,latency=20'
 */
struct rte_devargs *
vmbus_devargs_lookup(struct rte_vmbus_device *dev)
{
	struct rte_devargs *devargs;
	rte_uuid_t addr;

	RTE_EAL_DEVARGS_FOREACH("vmbus", devargs) {
		vmbus_parse(devargs->name, &addr);

		if (rte_uuid_compare(dev->device_id, addr) == 0)
			return devargs;
	}
	return NULL;

}

/* register vmbus driver */
void
rte_vmbus_register(struct rte_vmbus_driver *driver)
{
	VMBUS_LOG(DEBUG,
		"Registered driver %s", driver->driver.name);

	TAILQ_INSERT_TAIL(&rte_vmbus_bus.driver_list, driver, next);
}

/* unregister vmbus driver */
void
rte_vmbus_unregister(struct rte_vmbus_driver *driver)
{
	TAILQ_REMOVE(&rte_vmbus_bus.driver_list, driver, next);
}

/* Add a device to VMBUS bus */
void
vmbus_add_device(struct rte_vmbus_device *vmbus_dev)
{
	TAILQ_INSERT_TAIL(&rte_vmbus_bus.device_list, vmbus_dev, next);
}

/* Insert a device into a predefined position in VMBUS bus */
void
vmbus_insert_device(struct rte_vmbus_device *exist_vmbus_dev,
		      struct rte_vmbus_device *new_vmbus_dev)
{
	TAILQ_INSERT_BEFORE(exist_vmbus_dev, new_vmbus_dev, next);
}

/* Remove a device from VMBUS bus */
void
vmbus_remove_device(struct rte_vmbus_device *vmbus_dev)
{
	TAILQ_REMOVE(&rte_vmbus_bus.device_list, vmbus_dev, next);
}

/* VMBUS doesn't support hotplug */
static struct rte_device *
vmbus_find_device(const struct rte_device *start, rte_dev_cmp_t cmp,
		  const void *data)
{
	struct rte_vmbus_device *dev;

	FOREACH_DEVICE_ON_VMBUS(dev) {
		if (start && &dev->device == start) {
			start = NULL;
			continue;
		}
		if (cmp(&dev->device, data) == 0)
			return &dev->device;
	}

	return NULL;
}


struct rte_vmbus_bus rte_vmbus_bus = {
	.bus = {
		.scan = rte_vmbus_scan,
		.probe = rte_vmbus_probe,
		.cleanup = rte_vmbus_cleanup,
		.find_device = vmbus_find_device,
		.parse = vmbus_parse,
	},
	.device_list = TAILQ_HEAD_INITIALIZER(rte_vmbus_bus.device_list),
	.driver_list = TAILQ_HEAD_INITIALIZER(rte_vmbus_bus.driver_list),
};

RTE_REGISTER_BUS(vmbus, rte_vmbus_bus.bus);
RTE_LOG_REGISTER_DEFAULT(vmbus_logtype_bus, NOTICE);
