/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
/*   BSD LICENSE
 *
 *   Copyright 2013-2014 6WIND S.A.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_pci.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_devargs.h>

#include "eal_private.h"

struct pci_driver_list pci_driver_list;
struct pci_device_list pci_device_list;

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
 * If vendor/device ID match, call the devinit() function of the
 * driver.
 */
static int
rte_eal_pci_probe_one_driver(struct rte_pci_driver *dr, struct rte_pci_device *dev)
{
	int ret;
	const struct rte_pci_id *id_table;

	for (id_table = dr->id_table; id_table->vendor_id != 0; id_table++) {

		/* check if device's identifiers match the driver's ones */
		if (id_table->vendor_id != dev->id.vendor_id &&
				id_table->vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->device_id != dev->id.device_id &&
				id_table->device_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_vendor_id != dev->id.subsystem_vendor_id &&
				id_table->subsystem_vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_device_id != dev->id.subsystem_device_id &&
				id_table->subsystem_device_id != PCI_ANY_ID)
			continue;

		struct rte_pci_addr *loc = &dev->addr;

		RTE_LOG(DEBUG, EAL, "PCI device "PCI_PRI_FMT" on NUMA socket %i\n",
				loc->domain, loc->bus, loc->devid, loc->function,
				dev->numa_node);

		RTE_LOG(DEBUG, EAL, "  probe driver: %x:%x %s\n", dev->id.vendor_id,
				dev->id.device_id, dr->name);

		/* no initialization when blacklisted, return without error */
		if (dev->devargs != NULL &&
			dev->devargs->type == RTE_DEVTYPE_BLACKLISTED_PCI) {
			RTE_LOG(DEBUG, EAL, "  Device is blacklisted, not initializing\n");
			return 1;
		}

		if (dr->drv_flags & RTE_PCI_DRV_NEED_MAPPING) {
#ifdef RTE_PCI_CONFIG
			/*
			 * Set PCIe config space for high performance.
			 * Return value can be ignored.
			 */
			pci_config_space_set(dev);
#endif
			/* map resources for devices that use igb_uio */
			ret = pci_map_device(dev);
			if (ret != 0)
				return ret;
		} else if (dr->drv_flags & RTE_PCI_DRV_FORCE_UNBIND &&
				rte_eal_process_type() == RTE_PROC_PRIMARY) {
			/* unbind current driver */
			if (pci_unbind_kernel_driver(dev) < 0)
				return -1;
		}

		/* reference driver structure */
		dev->driver = dr;

		/* call the driver devinit() function */
		return dr->devinit(dr, dev);
	}
	/* return positive value if driver is not found */
	return 1;
}

/*
 * If vendor/device ID match, call the devuninit() function of the
 * driver.
 */
static int
rte_eal_pci_detach_dev(struct rte_pci_driver *dr,
		struct rte_pci_device *dev)
{
	const struct rte_pci_id *id_table;

	if ((dr == NULL) || (dev == NULL))
		return -EINVAL;

	for (id_table = dr->id_table; id_table->vendor_id != 0; id_table++) {

		/* check if device's identifiers match the driver's ones */
		if (id_table->vendor_id != dev->id.vendor_id &&
				id_table->vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->device_id != dev->id.device_id &&
				id_table->device_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_vendor_id != dev->id.subsystem_vendor_id &&
				id_table->subsystem_vendor_id != PCI_ANY_ID)
			continue;
		if (id_table->subsystem_device_id != dev->id.subsystem_device_id &&
				id_table->subsystem_device_id != PCI_ANY_ID)
			continue;

		struct rte_pci_addr *loc = &dev->addr;

		RTE_LOG(DEBUG, EAL, "PCI device "PCI_PRI_FMT" on NUMA socket %i\n",
				loc->domain, loc->bus, loc->devid,
				loc->function, dev->numa_node);

		RTE_LOG(DEBUG, EAL, "  remove driver: %x:%x %s\n", dev->id.vendor_id,
				dev->id.device_id, dr->name);

		if (dr->devuninit && (dr->devuninit(dev) < 0))
			return -1;	/* negative value is an error */

		/* clear driver structure */
		dev->driver = NULL;

		if (dr->drv_flags & RTE_PCI_DRV_NEED_MAPPING)
			/* unmap resources for devices that use igb_uio */
			pci_unmap_device(dev);

		return 0;
	}

	/* return positive value if driver is not found */
	return 1;
}

/*
 * If vendor/device ID match, call the devinit() function of all
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

	TAILQ_FOREACH(dr, &pci_driver_list, next) {
		rc = rte_eal_pci_probe_one_driver(dr, dev);
		if (rc < 0)
			/* negative value is an error */
			return -1;
		if (rc > 0)
			/* positive value means driver not found */
			continue;
		return 0;
	}
	return 1;
}

/*
 * If vendor/device ID match, call the devuninit() function of all
 * registered driver for the given device. Return -1 if initialization
 * failed, return 1 if no driver is found for this device.
 */
static int
pci_detach_all_drivers(struct rte_pci_device *dev)
{
	struct rte_pci_driver *dr = NULL;
	int rc = 0;

	if (dev == NULL)
		return -1;

	TAILQ_FOREACH(dr, &pci_driver_list, next) {
		rc = rte_eal_pci_detach_dev(dr, dev);
		if (rc < 0)
			/* negative value is an error */
			return -1;
		if (rc > 0)
			/* positive value means driver not found */
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
rte_eal_pci_probe_one(const struct rte_pci_addr *addr)
{
	struct rte_pci_device *dev = NULL;
	int ret = 0;

	if (addr == NULL)
		return -1;

	TAILQ_FOREACH(dev, &pci_device_list, next) {
		if (rte_eal_compare_pci_addr(&dev->addr, addr))
			continue;

		ret = pci_probe_all_drivers(dev);
		if (ret < 0)
			goto err_return;
		return 0;
	}
	return -1;

err_return:
	RTE_LOG(WARNING, EAL, "Requested device " PCI_PRI_FMT
			" cannot be used\n", dev->addr.domain, dev->addr.bus,
			dev->addr.devid, dev->addr.function);
	return -1;
}

int __attribute__ ((deprecated))
rte_eal_pci_close_one(const struct rte_pci_addr *addr)
{
	return rte_eal_pci_detach(addr);
}

/*
 * Detach device specified by its pci address.
 */
int
rte_eal_pci_detach(const struct rte_pci_addr *addr)
{
	struct rte_pci_device *dev = NULL;
	int ret = 0;

	if (addr == NULL)
		return -1;

	TAILQ_FOREACH(dev, &pci_device_list, next) {
		if (rte_eal_compare_pci_addr(&dev->addr, addr))
			continue;

		ret = pci_detach_all_drivers(dev);
		if (ret < 0)
			goto err_return;

		TAILQ_REMOVE(&pci_device_list, dev, next);
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
 * Scan the content of the PCI bus, and call the devinit() function for
 * all registered drivers that have a matching entry in its id_table
 * for discovered devices.
 */
int
rte_eal_pci_probe(void)
{
	struct rte_pci_device *dev = NULL;
	struct rte_devargs *devargs;
	int probe_all = 0;
	int ret = 0;

	if (rte_eal_devargs_type_count(RTE_DEVTYPE_WHITELISTED_PCI) == 0)
		probe_all = 1;

	TAILQ_FOREACH(dev, &pci_device_list, next) {

		/* set devargs in PCI structure */
		devargs = pci_devargs_lookup(dev);
		if (devargs != NULL)
			dev->devargs = devargs;

		/* probe all or only whitelisted devices */
		if (probe_all)
			ret = pci_probe_all_drivers(dev);
		else if (devargs != NULL &&
			devargs->type == RTE_DEVTYPE_WHITELISTED_PCI)
			ret = pci_probe_all_drivers(dev);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Requested device " PCI_PRI_FMT
				 " cannot be used\n", dev->addr.domain, dev->addr.bus,
				 dev->addr.devid, dev->addr.function);
	}

	return 0;
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
rte_eal_pci_dump(FILE *f)
{
	struct rte_pci_device *dev = NULL;

	TAILQ_FOREACH(dev, &pci_device_list, next) {
		pci_dump_one_device(f, dev);
	}
}

/* register a driver */
void
rte_eal_pci_register(struct rte_pci_driver *driver)
{
	TAILQ_INSERT_TAIL(&pci_driver_list, driver, next);
}

/* unregister a driver */
void
rte_eal_pci_unregister(struct rte_pci_driver *driver)
{
	TAILQ_REMOVE(&pci_driver_list, driver, next);
}
