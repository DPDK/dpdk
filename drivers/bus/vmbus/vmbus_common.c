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

#include <eal_export.h>
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

static bool
vmbus_bus_match(const struct rte_driver *drv, const struct rte_device *dev)
{
	const struct rte_vmbus_driver *dr = RTE_BUS_DRIVER(drv, *dr);
	const struct rte_vmbus_device *vmbus_dev = RTE_BUS_DEVICE(dev, *vmbus_dev);
	const rte_uuid_t *id_table;

	for (id_table = dr->id_table; !rte_uuid_is_null(*id_table); ++id_table) {
		if (rte_uuid_compare(*id_table, vmbus_dev->class_id) == 0)
			return true;
	}

	return false;
}

/*
 * If device ID match, call the devinit() function of the driver.
 */
static int
vmbus_probe_device(struct rte_driver *drv, struct rte_device *dev)
{
	struct rte_vmbus_device *vmbus_dev = RTE_BUS_DEVICE(dev, *vmbus_dev);
	struct rte_vmbus_driver *vmbus_drv = RTE_BUS_DRIVER(drv, *vmbus_drv);
	char guid[RTE_UUID_STRLEN];
	int ret;

	/* Check if a driver is already loaded */
	if (rte_dev_is_probed(&vmbus_dev->device)) {
		VMBUS_LOG(DEBUG, "VMBUS driver already loaded");
		return 0;
	}

	rte_uuid_unparse(vmbus_dev->device_id, guid, sizeof(guid));
	VMBUS_LOG(INFO, "VMBUS device %s on NUMA socket %i",
		  guid, vmbus_dev->device.numa_node);

	/* no initialization when marked as blocked, return without error */
	if (vmbus_dev->device.devargs != NULL &&
		vmbus_dev->device.devargs->policy == RTE_DEV_BLOCKED) {
		VMBUS_LOG(INFO, "  Device is blocked, not initializing");
		return 1;
	}

	/* map resources for device */
	ret = rte_vmbus_map_device(vmbus_dev);
	if (ret != 0)
		return ret;

	/* reference driver structure */
	vmbus_dev->driver = vmbus_drv;

	if (vmbus_dev->device.numa_node < 0 && rte_socket_count() > 1)
		VMBUS_LOG(INFO, "Device %s is not NUMA-aware", guid);

	/* call the driver probe() function */
	VMBUS_LOG(INFO, "  probe driver: %s", vmbus_drv->driver.name);
	ret = vmbus_drv->probe(vmbus_drv, vmbus_dev);
	if (ret != 0) {
		vmbus_dev->driver = NULL;
		rte_vmbus_unmap_device(vmbus_dev);
	} else {
		vmbus_dev->device.driver = &vmbus_drv->driver;
	}

	return ret;
}

/*
 * Scan the vmbus, and call the devinit() function for
 * all registered drivers that have a matching entry in its id_table
 * for discovered devices.
 */
RTE_EXPORT_SYMBOL(rte_vmbus_probe)
int
rte_vmbus_probe(void)
{
	struct rte_vmbus_device *dev;
	size_t probed = 0, failed = 0;
	char ubuf[RTE_UUID_STRLEN];

	RTE_BUS_FOREACH_DEV(dev, &rte_vmbus_bus.bus) {
		struct rte_driver *drv = NULL;
		int ret;

		probed++;

		rte_uuid_unparse(dev->device_id, ubuf, sizeof(ubuf));

		if (rte_bus_device_is_ignored(&rte_vmbus_bus.bus, ubuf))
			continue;

next_driver:
		drv = rte_bus_find_driver(&rte_vmbus_bus.bus, drv, &dev->device);
		if (drv == NULL)
			continue;

		ret = rte_vmbus_bus.bus.probe_device(drv, &dev->device);
		if (ret < 0) {
			VMBUS_LOG(NOTICE,
				"Requested device %s cannot be used", ubuf);
			rte_errno = errno;
			failed++;
		} else if (ret > 0) {
			goto next_driver;
		}
	}

	return (probed && probed == failed) ? -1 : 0;
}

static int
rte_vmbus_cleanup(void)
{
	struct rte_vmbus_device *dev;
	int error = 0;

	RTE_BUS_FOREACH_DEV(dev, &rte_vmbus_bus.bus) {
		const struct rte_vmbus_driver *drv = dev->driver;
		int ret;

		if (!rte_dev_is_probed(&dev->device))
			continue;
		if (drv->remove == NULL)
			continue;

		ret = drv->remove(dev);
		if (ret < 0)
			error = -1;

		rte_vmbus_unmap_device(dev);

		dev->driver = NULL;
		dev->device.driver = NULL;
		rte_bus_remove_device(&rte_vmbus_bus.bus, &dev->device);
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

static int
vmbus_dev_compare(const char *name1, const char *name2)
{
	rte_uuid_t guid1, guid2;

	if (vmbus_parse(name1, &guid1) != 0 ||
			vmbus_parse(name2, &guid2) != 0)
		return 1;

	return rte_uuid_compare(guid1, guid2);
}

/* register vmbus driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_vmbus_register)
void
rte_vmbus_register(struct rte_vmbus_driver *driver)
{
	VMBUS_LOG(DEBUG,
		"Registered driver %s", driver->driver.name);

	rte_bus_add_driver(&rte_vmbus_bus.bus, &driver->driver);
}

/* unregister vmbus driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_vmbus_unregister)
void
rte_vmbus_unregister(struct rte_vmbus_driver *driver)
{
	rte_bus_remove_driver(&rte_vmbus_bus.bus, &driver->driver);
}

/* VMBUS doesn't support hotplug */
struct rte_vmbus_bus rte_vmbus_bus = {
	.bus = {
		.scan = rte_vmbus_scan,
		.probe = rte_vmbus_probe,
		.cleanup = rte_vmbus_cleanup,
		.find_device = rte_bus_generic_find_device,
		.match = vmbus_bus_match,
		.probe_device = vmbus_probe_device,
		.parse = vmbus_parse,
		.dev_compare = vmbus_dev_compare,
	},
};

RTE_REGISTER_BUS(vmbus, rte_vmbus_bus.bus);
RTE_LOG_REGISTER_DEFAULT(vmbus_logtype_bus, NOTICE);
