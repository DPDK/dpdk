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

	rte_uuid_unparse(vmbus_dev->device_id, guid, sizeof(guid));
	VMBUS_LOG(INFO, "VMBUS device %s on NUMA socket %i",
		  guid, vmbus_dev->device.numa_node);

	/* allocate interrupt handle instance */
	vmbus_dev->intr_handle =
		rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
	if (vmbus_dev->intr_handle == NULL)
		return -ENOMEM;

	/* map resources for device */
	ret = rte_vmbus_map_device(vmbus_dev);
	if (ret != 0)
		goto free_intr;

	if (vmbus_dev->device.numa_node < 0 && rte_socket_count() > 1)
		VMBUS_LOG(INFO, "Device %s is not NUMA-aware", guid);

	/* call the driver probe() function */
	VMBUS_LOG(INFO, "  probe driver: %s", vmbus_drv->driver.name);
	ret = vmbus_drv->probe(vmbus_drv, vmbus_dev);
	if (ret != 0)
		goto unmap;

	return 0;

unmap:
	rte_vmbus_unmap_device(vmbus_dev);
free_intr:
	rte_intr_instance_free(vmbus_dev->intr_handle);
	vmbus_dev->intr_handle = NULL;

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
	return rte_bus_generic_probe(&rte_vmbus_bus);
}

static int
vmbus_unplug_device(struct rte_device *rte_dev)
{
	const struct rte_vmbus_driver *drv = RTE_BUS_DRIVER(rte_dev->driver, *drv);
	struct rte_vmbus_device *dev = RTE_BUS_DEVICE(rte_dev, *dev);
	int ret = 0;

	if (drv->remove != NULL) {
		ret = drv->remove(dev);
		if (ret < 0)
			return ret;
	}

	rte_vmbus_unmap_device(dev);
	rte_intr_instance_free(dev->intr_handle);
	dev->intr_handle = NULL;

	return 0;
}

static void
vmbus_free_device(struct rte_device *dev)
{
	free(RTE_BUS_DEVICE(dev, struct rte_vmbus_device));
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

	rte_bus_add_driver(&rte_vmbus_bus, &driver->driver);
}

/* unregister vmbus driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_vmbus_unregister)
void
rte_vmbus_unregister(struct rte_vmbus_driver *driver)
{
	rte_bus_remove_driver(&rte_vmbus_bus, &driver->driver);
}

/* VMBUS doesn't support hotplug */
struct rte_bus rte_vmbus_bus = {
	.scan = rte_vmbus_scan,
	.probe = rte_bus_generic_probe,
	.free_device = vmbus_free_device,
	.cleanup = rte_bus_generic_cleanup,
	.find_device = rte_bus_generic_find_device,
	.match = vmbus_bus_match,
	.probe_device = vmbus_probe_device,
	.unplug_device = vmbus_unplug_device,
	.parse = vmbus_parse,
	.dev_compare = vmbus_dev_compare,
};

RTE_REGISTER_BUS(vmbus, rte_vmbus_bus);
RTE_LOG_REGISTER_DEFAULT(vmbus_logtype_bus, NOTICE);
