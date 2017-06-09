/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright 2013-2014 6WIND S.A.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/queue.h>
#include <sys/mman.h>

#include <rte_errno.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_bus.h>
#include <rte_pci.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_devargs.h>

#include "eal_private.h"

extern struct rte_pci_bus rte_pci_bus;

#define SYSFS_PCI_DEVICES "/sys/bus/pci/devices"

const char *pci_get_sysfs_path(void)
{
	const char *path = NULL;

	path = getenv("SYSFS_PCI_DEVICES");
	if (path == NULL)
		return SYSFS_PCI_DEVICES;

	return path;
}

static struct rte_devargs *pci_devargs_lookup(struct rte_pci_device *dev)
{
	struct rte_devargs *devargs;

	TAILQ_FOREACH(devargs, &devargs_list, next) {
		if (devargs->type != RTE_DEVTYPE_BLACKLISTED_PCI &&
			devargs->type != RTE_DEVTYPE_WHITELISTED_PCI)
			continue;
		if (!rte_eal_compare_pci_addr(&dev->addr, &devargs->pci.addr))
			return devargs;
	}
	return NULL;
}

/* map a particular resource from a file */
void *
pci_map_resource(void *requested_addr, int fd, off_t offset, size_t size,
		 int additional_flags)
{
	void *mapaddr;

	/* Map the PCI memory resource of device */
	mapaddr = mmap(requested_addr, size, PROT_READ | PROT_WRITE,
			MAP_SHARED | additional_flags, fd, offset);
	if (mapaddr == MAP_FAILED) {
		RTE_LOG(ERR, EAL, "%s(): cannot mmap(%d, %p, 0x%lx, 0x%lx): %s (%p)\n",
			__func__, fd, requested_addr,
			(unsigned long)size, (unsigned long)offset,
			strerror(errno), mapaddr);
	} else
		RTE_LOG(DEBUG, EAL, "  PCI memory mapped at %p\n", mapaddr);

	return mapaddr;
}

/* unmap a particular resource */
void
pci_unmap_resource(void *requested_addr, size_t size)
{
	if (requested_addr == NULL)
		return;

	/* Unmap the PCI memory resource of device */
	if (munmap(requested_addr, size)) {
		RTE_LOG(ERR, EAL, "%s(): cannot munmap(%p, 0x%lx): %s\n",
			__func__, requested_addr, (unsigned long)size,
			strerror(errno));
	} else
		RTE_LOG(DEBUG, EAL, "  PCI memory unmapped at %p\n",
				requested_addr);
}

/*
 * Match the PCI Driver and Device using the ID Table
 *
 * @param pci_drv
 *	PCI driver from which ID table would be extracted
 * @param pci_dev
 *	PCI device to match against the driver
 * @return
 *	1 for successful match
 *	0 for unsuccessful match
 */
static int
rte_pci_match(const struct rte_pci_driver *pci_drv,
	      const struct rte_pci_device *pci_dev)
{
	const struct rte_pci_id *id_table;

	for (id_table = pci_drv->id_table; id_table->vendor_id != 0;
	     id_table++) {
		/* check if device's identifiers match the driver's ones */
		if (id_table->vendor_id != pci_dev->id.vendor_id &&
				id_table->vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->device_id != pci_dev->id.device_id &&
				id_table->device_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_vendor_id !=
		    pci_dev->id.subsystem_vendor_id &&
		    id_table->subsystem_vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_device_id !=
		    pci_dev->id.subsystem_device_id &&
		    id_table->subsystem_device_id != PCI_ANY_ID)
			continue;
		if (id_table->class_id != pci_dev->id.class_id &&
				id_table->class_id != RTE_CLASS_ANY_ID)
			continue;

		return 1;
	}

	return 0;
}

/*
 * If vendor/device ID match, call the probe() function of the
 * driver.
 */
static int
rte_pci_probe_one_driver(struct rte_pci_driver *dr,
			 struct rte_pci_device *dev)
{
	int ret;
	struct rte_pci_addr *loc;

	if ((dr == NULL) || (dev == NULL))
		return -EINVAL;

	loc = &dev->addr;

	/* The device is not blacklisted; Check if driver supports it */
	if (!rte_pci_match(dr, dev)) {
		/* Match of device and driver failed */
		RTE_LOG(DEBUG, EAL, "Driver (%s) doesn't match the device\n",
			dr->driver.name);
		return 1;
	}

	RTE_LOG(INFO, EAL, "PCI device "PCI_PRI_FMT" on NUMA socket %i\n",
			loc->domain, loc->bus, loc->devid, loc->function,
			dev->device.numa_node);

	/* no initialization when blacklisted, return without error */
	if (dev->device.devargs != NULL &&
		dev->device.devargs->type ==
			RTE_DEVTYPE_BLACKLISTED_PCI) {
		RTE_LOG(INFO, EAL, "  Device is blacklisted, not"
			" initializing\n");
		return 1;
	}

	RTE_LOG(INFO, EAL, "  probe driver: %x:%x %s\n", dev->id.vendor_id,
		dev->id.device_id, dr->driver.name);

	if (dr->drv_flags & RTE_PCI_DRV_NEED_MAPPING) {
		/* map resources for devices that use igb_uio */
		ret = rte_pci_map_device(dev);
		if (ret != 0)
			return ret;
	}

	/* reference driver structure */
	dev->driver = dr;
	dev->device.driver = &dr->driver;

	/* call the driver probe() function */
	ret = dr->probe(dr, dev);
	if (ret) {
		dev->driver = NULL;
		if ((dr->drv_flags & RTE_PCI_DRV_NEED_MAPPING) &&
			/* Don't unmap if device is unsupported and
			 * driver needs mapped resources.
			 */
			!(ret > 0 &&
				(dr->drv_flags & RTE_PCI_DRV_KEEP_MAPPED_RES)))
			rte_pci_unmap_device(dev);
	}

	return ret;
}

/*
 * If vendor/device ID match, call the remove() function of the
 * driver.
 */
static int
rte_pci_detach_dev(struct rte_pci_device *dev)
{
	struct rte_pci_addr *loc;
	struct rte_pci_driver *dr;

	if (dev == NULL)
		return -EINVAL;

	dr = dev->driver;
	loc = &dev->addr;

	RTE_LOG(DEBUG, EAL, "PCI device "PCI_PRI_FMT" on NUMA socket %i\n",
			loc->domain, loc->bus, loc->devid,
			loc->function, dev->device.numa_node);

	RTE_LOG(DEBUG, EAL, "  remove driver: %x:%x %s\n", dev->id.vendor_id,
			dev->id.device_id, dr->driver.name);

	if (dr->remove && (dr->remove(dev) < 0))
		return -1;	/* negative value is an error */

	/* clear driver structure */
	dev->driver = NULL;

	if (dr->drv_flags & RTE_PCI_DRV_NEED_MAPPING)
		/* unmap resources for devices that use igb_uio */
		rte_pci_unmap_device(dev);

	return 0;
}

/*
 * If vendor/device ID match, call the probe() function of all
 * registered driver for the given device. Return -1 if initialization
 * failed, return 1 if no driver is found for this device.
 */
static int
pci_probe_all_drivers(struct rte_pci_device *dev)
{
	struct rte_pci_driver *dr = NULL;
	int rc = 0;

	if (dev == NULL)
		return -1;

	/* Check if a driver is already loaded */
	if (dev->driver != NULL)
		return 0;

	FOREACH_DRIVER_ON_PCIBUS(dr) {
		rc = rte_pci_probe_one_driver(dr, dev);
		if (rc < 0)
			/* negative value is an error */
			return -1;
		if (rc > 0)
			/* positive value means driver doesn't support it */
			continue;
		return 0;
	}
	return 1;
}

/*
 * Find the pci device specified by pci address, then invoke probe function of
 * the driver of the devive.
 */
int
rte_pci_probe_one(const struct rte_pci_addr *addr)
{
	struct rte_pci_device *dev = NULL;

	int ret = 0;

	if (addr == NULL)
		return -1;

	/* update current pci device in global list, kernel bindings might have
	 * changed since last time we looked at it.
	 */
	if (pci_update_device(addr) < 0)
		goto err_return;

	FOREACH_DEVICE_ON_PCIBUS(dev) {
		if (rte_eal_compare_pci_addr(&dev->addr, addr))
			continue;

		ret = pci_probe_all_drivers(dev);
		if (ret)
			goto err_return;
		return 0;
	}
	return -1;

err_return:
	RTE_LOG(WARNING, EAL,
		"Requested device " PCI_PRI_FMT " cannot be used\n",
		addr->domain, addr->bus, addr->devid, addr->function);
	return -1;
}

/*
 * Detach device specified by its pci address.
 */
int
rte_pci_detach(const struct rte_pci_addr *addr)
{
	struct rte_pci_device *dev = NULL;
	int ret = 0;

	if (addr == NULL)
		return -1;

	FOREACH_DEVICE_ON_PCIBUS(dev) {
		if (rte_eal_compare_pci_addr(&dev->addr, addr))
			continue;

		ret = rte_pci_detach_dev(dev);
		if (ret < 0)
			/* negative value is an error */
			goto err_return;
		if (ret > 0)
			/* positive value means driver doesn't support it */
			continue;

		rte_pci_remove_device(dev);
		free(dev);
		return 0;
	}
	return -1;

err_return:
	RTE_LOG(WARNING, EAL, "Requested device " PCI_PRI_FMT
			" cannot be used\n", dev->addr.domain, dev->addr.bus,
			dev->addr.devid, dev->addr.function);
	return -1;
}

/*
 * Scan the content of the PCI bus, and call the probe() function for
 * all registered drivers that have a matching entry in its id_table
 * for discovered devices.
 */
int
rte_pci_probe(void)
{
	struct rte_pci_device *dev = NULL;
	size_t probed = 0, failed = 0;
	struct rte_devargs *devargs;
	int probe_all = 0;
	int ret = 0;

	if (rte_eal_devargs_type_count(RTE_DEVTYPE_WHITELISTED_PCI) == 0)
		probe_all = 1;

	FOREACH_DEVICE_ON_PCIBUS(dev) {
		probed++;

		/* set devargs in PCI structure */
		devargs = pci_devargs_lookup(dev);
		if (devargs != NULL)
			dev->device.devargs = devargs;

		/* probe all or only whitelisted devices */
		if (probe_all)
			ret = pci_probe_all_drivers(dev);
		else if (devargs != NULL &&
			devargs->type == RTE_DEVTYPE_WHITELISTED_PCI)
			ret = pci_probe_all_drivers(dev);
		if (ret < 0) {
			RTE_LOG(ERR, EAL, "Requested device " PCI_PRI_FMT
				 " cannot be used\n", dev->addr.domain, dev->addr.bus,
				 dev->addr.devid, dev->addr.function);
			rte_errno = errno;
			failed++;
			ret = 0;
		}
	}

	return (probed && probed == failed) ? -1 : 0;
}

/* dump one device */
static int
pci_dump_one_device(FILE *f, struct rte_pci_device *dev)
{
	int i;

	fprintf(f, PCI_PRI_FMT, dev->addr.domain, dev->addr.bus,
	       dev->addr.devid, dev->addr.function);
	fprintf(f, " - vendor:%x device:%x\n", dev->id.vendor_id,
	       dev->id.device_id);

	for (i = 0; i != sizeof(dev->mem_resource) /
		sizeof(dev->mem_resource[0]); i++) {
		fprintf(f, "   %16.16"PRIx64" %16.16"PRIx64"\n",
			dev->mem_resource[i].phys_addr,
			dev->mem_resource[i].len);
	}
	return 0;
}

/* dump devices on the bus */
void
rte_pci_dump(FILE *f)
{
	struct rte_pci_device *dev = NULL;

	FOREACH_DEVICE_ON_PCIBUS(dev) {
		pci_dump_one_device(f, dev);
	}
}

/* register a driver */
void
rte_pci_register(struct rte_pci_driver *driver)
{
	TAILQ_INSERT_TAIL(&rte_pci_bus.driver_list, driver, next);
	driver->bus = &rte_pci_bus;
}

/* unregister a driver */
void
rte_pci_unregister(struct rte_pci_driver *driver)
{
	TAILQ_REMOVE(&rte_pci_bus.driver_list, driver, next);
	driver->bus = NULL;
}

/* Add a device to PCI bus */
void
rte_pci_add_device(struct rte_pci_device *pci_dev)
{
	TAILQ_INSERT_TAIL(&rte_pci_bus.device_list, pci_dev, next);
}

/* Insert a device into a predefined position in PCI bus */
void
rte_pci_insert_device(struct rte_pci_device *exist_pci_dev,
		      struct rte_pci_device *new_pci_dev)
{
	TAILQ_INSERT_BEFORE(exist_pci_dev, new_pci_dev, next);
}

/* Remove a device from PCI bus */
void
rte_pci_remove_device(struct rte_pci_device *pci_dev)
{
	TAILQ_REMOVE(&rte_pci_bus.device_list, pci_dev, next);
}

struct rte_pci_bus rte_pci_bus = {
	.bus = {
		.scan = rte_pci_scan,
		.probe = rte_pci_probe,
	},
	.device_list = TAILQ_HEAD_INITIALIZER(rte_pci_bus.device_list),
	.driver_list = TAILQ_HEAD_INITIALIZER(rte_pci_bus.driver_list),
};

RTE_REGISTER_BUS(PCI_BUS_NAME, rte_pci_bus.bus);
