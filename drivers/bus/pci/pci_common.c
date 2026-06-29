/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright 2013-2014 6WIND S.A.
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
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_eal_paging.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_devargs.h>
#include <rte_vfio.h>
#include <rte_tailq.h>

#include "private.h"


#define SYSFS_PCI_DEVICES "/sys/bus/pci/devices"

RTE_EXPORT_INTERNAL_SYMBOL(rte_pci_get_sysfs_path)
const char *rte_pci_get_sysfs_path(void)
{
	const char *path = NULL;

#ifdef RTE_EXEC_ENV_LINUX
	path = getenv("SYSFS_PCI_DEVICES");
	if (path == NULL)
		return SYSFS_PCI_DEVICES;
#endif

	return path;
}

#ifdef RTE_EXEC_ENV_WINDOWS
#define asprintf pci_asprintf

static int
__rte_format_printf(2, 3)
pci_asprintf(char **buffer, const char *format, ...)
{
	int size, ret;
	va_list arg;

	va_start(arg, format);
	size = vsnprintf(NULL, 0, format, arg);
	va_end(arg);
	if (size < 0)
		return -1;
	size++;

	*buffer = malloc(size);
	if (*buffer == NULL)
		return -1;

	va_start(arg, format);
	ret = vsnprintf(*buffer, size, format, arg);
	va_end(arg);
	if (ret != size - 1) {
		free(*buffer);
		return -1;
	}
	return ret;
}
#endif /* RTE_EXEC_ENV_WINDOWS */

void
pci_common_set(struct rte_pci_device *dev)
{
	/* Each device has its internal, canonical name set. */
	rte_pci_device_name(&dev->addr,
			dev->name, sizeof(dev->name));
	dev->device.name = dev->name;

	dev->device.devargs = rte_bus_find_devargs(&rte_pci_bus, dev->name);

	if (dev->bus_info != NULL ||
			asprintf(&dev->bus_info, "vendor_id=%"PRIx16", device_id=%"PRIx16,
				dev->id.vendor_id, dev->id.device_id) != -1)
		dev->device.bus_info = dev->bus_info;
}

void
pci_free(struct rte_pci_device_internal *pdev)
{
	if (pdev == NULL)
		return;
	free(pdev->device.bus_info);
	free(pdev);
}

/* map a particular resource from a file */
void *
pci_map_resource(void *requested_addr, int fd, off_t offset, size_t size,
		 int additional_flags)
{
	void *mapaddr;

	/* Map the PCI memory resource of device */
	mapaddr = rte_mem_map(requested_addr, size,
		RTE_PROT_READ | RTE_PROT_WRITE,
		RTE_MAP_SHARED | additional_flags, fd, offset);
	if (mapaddr == NULL) {
		PCI_LOG(ERR, "%s(): cannot map resource(%d, %p, 0x%zx, 0x%llx): %s (%p)",
			__func__, fd, requested_addr, size,
			(unsigned long long)offset,
			rte_strerror(rte_errno), mapaddr);
	} else
		PCI_LOG(DEBUG, "  PCI memory mapped at %p", mapaddr);

	return mapaddr;
}

/* unmap a particular resource */
void
pci_unmap_resource(void *requested_addr, size_t size)
{
	if (requested_addr == NULL)
		return;

	/* Unmap the PCI memory resource of device */
	if (rte_mem_unmap(requested_addr, size)) {
		PCI_LOG(ERR, "%s(): cannot mem unmap(%p, %#zx): %s",
			__func__, requested_addr, size,
			rte_strerror(rte_errno));
	} else
		PCI_LOG(DEBUG, "  PCI memory unmapped at %p", requested_addr);
}

static bool
pci_bus_match(const struct rte_driver *drv, const struct rte_device *dev)
{
	const struct rte_pci_driver *pci_drv = RTE_BUS_DRIVER(drv, *pci_drv);
	const struct rte_pci_device *pci_dev = RTE_BUS_DEVICE(dev, *pci_dev);
	const struct rte_pci_id *id_table;

	for (id_table = pci_drv->id_table; id_table->vendor_id != 0;
	     id_table++) {
		/* check if device's identifiers match the driver's ones */
		if (id_table->vendor_id != pci_dev->id.vendor_id &&
				id_table->vendor_id != RTE_PCI_ANY_ID)
			continue;
		if (id_table->device_id != pci_dev->id.device_id &&
				id_table->device_id != RTE_PCI_ANY_ID)
			continue;
		if (id_table->subsystem_vendor_id !=
		    pci_dev->id.subsystem_vendor_id &&
		    id_table->subsystem_vendor_id != RTE_PCI_ANY_ID)
			continue;
		if (id_table->subsystem_device_id !=
		    pci_dev->id.subsystem_device_id &&
		    id_table->subsystem_device_id != RTE_PCI_ANY_ID)
			continue;
		if (id_table->class_id != pci_dev->id.class_id &&
				id_table->class_id != RTE_CLASS_ANY_ID)
			continue;

		return true;
	}

	return false;
}

/*
 * If vendor/device ID match, call the probe() function of the
 * driver.
 */
static int
pci_probe_device(struct rte_driver *drv, struct rte_device *dev)
{
	struct rte_pci_device *pci_dev = RTE_BUS_DEVICE(dev, *pci_dev);
	struct rte_pci_driver *pci_drv = RTE_BUS_DRIVER(drv, *pci_drv);
	struct rte_pci_addr *loc = &pci_dev->addr;
	bool already_probed;
	int ret;

	PCI_LOG(DEBUG, "PCI device "PCI_PRI_FMT" on NUMA socket %i",
		loc->domain, loc->bus, loc->devid, loc->function,
		pci_dev->device.numa_node);

	if (pci_dev->device.numa_node < 0 && rte_socket_count() > 1)
		PCI_LOG(INFO, "Device %s is not NUMA-aware", pci_dev->name);

	already_probed = (pci_dev->intr_handle != NULL);
	if (already_probed && !(pci_drv->drv_flags & RTE_PCI_DRV_PROBE_AGAIN)) {
		PCI_LOG(DEBUG, "Device %s is already probed", pci_dev->device.name);
		return -EEXIST;
	}

	PCI_LOG(DEBUG, "  probe driver: %x:%x %s", pci_dev->id.vendor_id,
		pci_dev->id.device_id, pci_drv->driver.name);

	if (!already_probed) {
		enum rte_iova_mode dev_iova_mode;
		enum rte_iova_mode iova_mode;

		dev_iova_mode = pci_device_iova_mode(pci_drv, pci_dev);
		iova_mode = rte_eal_iova_mode();
		if (dev_iova_mode != RTE_IOVA_DC &&
		    dev_iova_mode != iova_mode) {
			PCI_LOG(ERR, "  Expecting '%s' IOVA mode but current mode is '%s', not initializing",
				dev_iova_mode == RTE_IOVA_PA ? "PA" : "VA",
				iova_mode == RTE_IOVA_PA ? "PA" : "VA");
			return -EINVAL;
		}

		/* Allocate interrupt instance for pci device */
		pci_dev->intr_handle =
			rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
		if (pci_dev->intr_handle == NULL) {
			PCI_LOG(ERR, "Failed to create interrupt instance for %s",
				pci_dev->device.name);
			return -ENOMEM;
		}

		pci_dev->vfio_req_intr_handle =
			rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
		if (pci_dev->vfio_req_intr_handle == NULL) {
			rte_intr_instance_free(pci_dev->intr_handle);
			pci_dev->intr_handle = NULL;
			PCI_LOG(ERR, "Failed to create vfio req interrupt instance for %s",
				pci_dev->device.name);
			return -ENOMEM;
		}

		if (pci_drv->drv_flags & RTE_PCI_DRV_NEED_MAPPING) {
			ret = rte_pci_map_device(pci_dev);
			if (ret != 0) {
				rte_intr_instance_free(pci_dev->vfio_req_intr_handle);
				pci_dev->vfio_req_intr_handle = NULL;
				rte_intr_instance_free(pci_dev->intr_handle);
				pci_dev->intr_handle = NULL;
				return ret;
			}
		}
	}

	PCI_LOG(INFO, "Probe PCI driver: %s (%x:%04x) device: "PCI_PRI_FMT" (socket %i)",
		pci_drv->driver.name, pci_dev->id.vendor_id, pci_dev->id.device_id,
		loc->domain, loc->bus, loc->devid, loc->function,
		pci_dev->device.numa_node);
	/* call the driver probe() function */
	ret = pci_drv->probe(pci_drv, pci_dev);
	if (already_probed)
		return ret; /* no rollback if already succeeded earlier */
	if (ret) {
		if ((pci_drv->drv_flags & RTE_PCI_DRV_NEED_MAPPING) &&
			/* Don't unmap if device is unsupported and
			 * driver needs mapped resources.
			 */
			!(ret > 0 &&
				(pci_drv->drv_flags & RTE_PCI_DRV_KEEP_MAPPED_RES)))
			rte_pci_unmap_device(pci_dev);
		rte_intr_instance_free(pci_dev->vfio_req_intr_handle);
		pci_dev->vfio_req_intr_handle = NULL;
		rte_intr_instance_free(pci_dev->intr_handle);
		pci_dev->intr_handle = NULL;
	}

	return ret;
}

static int
pci_unplug_device(struct rte_device *rte_dev)
{
	struct rte_pci_device *dev = RTE_BUS_DEVICE(rte_dev, *dev);
	struct rte_pci_addr *loc;
	const struct rte_pci_driver *dr = RTE_BUS_DRIVER(dev->device.driver, *dr);
	int ret = 0;

	loc = &dev->addr;

	PCI_LOG(DEBUG, "PCI device "PCI_PRI_FMT" on NUMA socket %i",
		loc->domain, loc->bus, loc->devid,
		loc->function, dev->device.numa_node);

	PCI_LOG(DEBUG, "  remove driver: %x:%x %s", dev->id.vendor_id,
		dev->id.device_id, dr->driver.name);

	if (dr->remove) {
		ret = dr->remove(dev);
		if (ret < 0)
			return ret;
	}

	if (dr->drv_flags & RTE_PCI_DRV_NEED_MAPPING)
		/* unmap resources for devices that use igb_uio */
		rte_pci_unmap_device(dev);

	rte_intr_instance_free(dev->intr_handle);
	dev->intr_handle = NULL;
	rte_intr_instance_free(dev->vfio_req_intr_handle);
	dev->vfio_req_intr_handle = NULL;

	return 0;
}

static void
pci_free_device(struct rte_device *dev)
{
	struct rte_pci_device *pdev = RTE_BUS_DEVICE(dev, *pdev);
	pci_free(RTE_PCI_DEVICE_INTERNAL(pdev));
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
RTE_EXPORT_SYMBOL(rte_pci_dump)
void
rte_pci_dump(FILE *f)
{
	struct rte_pci_device *dev = NULL;

	RTE_BUS_FOREACH_DEV(dev, &rte_pci_bus) {
		pci_dump_one_device(f, dev);
	}
}

static int
pci_parse(const char *name, void *addr)
{
	struct rte_pci_addr *out = addr;
	struct rte_pci_addr pci_addr;
	bool parse;

	parse = (rte_pci_addr_parse(name, &pci_addr) == 0);
	if (parse && addr != NULL)
		*out = pci_addr;
	return parse == false;
}

static int
pci_dev_compare(const char *name1, const char *name2)
{
	struct rte_pci_addr addr1, addr2;

	if (rte_pci_addr_parse(name1, &addr1) != 0 ||
			rte_pci_addr_parse(name2, &addr2) != 0)
		return 1;

	return rte_pci_addr_cmp(&addr1, &addr2);
}

/* register a driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_pci_register)
void
rte_pci_register(struct rte_pci_driver *driver)
{
	rte_bus_add_driver(&rte_pci_bus, &driver->driver);
}

/* unregister a driver */
RTE_EXPORT_INTERNAL_SYMBOL(rte_pci_unregister)
void
rte_pci_unregister(struct rte_pci_driver *driver)
{
	rte_bus_remove_driver(&rte_pci_bus, &driver->driver);
}

/*
 * find the device which encounter the failure, by iterate over all device on
 * PCI bus to check if the memory failure address is located in the range
 * of the BARs of the device.
 */
static struct rte_pci_device *
pci_find_device_by_addr(const void *failure_addr)
{
	struct rte_pci_device *pdev = NULL;
	uint64_t check_point, start, end, len;
	int i;

	check_point = (uint64_t)(uintptr_t)failure_addr;

	RTE_BUS_FOREACH_DEV(pdev, &rte_pci_bus) {
		for (i = 0; i != RTE_DIM(pdev->mem_resource); i++) {
			start = (uint64_t)(uintptr_t)pdev->mem_resource[i].addr;
			len = pdev->mem_resource[i].len;
			end = start + len;
			if (check_point >= start && check_point < end) {
				PCI_LOG(DEBUG, "Failure address %16.16"
					PRIx64" belongs to device %s!",
					check_point, pdev->device.name);
				return pdev;
			}
		}
	}
	return NULL;
}

static int
pci_hot_unplug_handler(struct rte_device *dev)
{
	struct rte_pci_device *pdev = RTE_BUS_DEVICE(dev, *pdev);
	int ret = 0;

	switch (pdev->kdrv) {
	case RTE_PCI_KDRV_VFIO:
		/*
		 * vfio kernel module guaranty the pci device would not be
		 * deleted until the user space release the resource, so no
		 * need to remap BARs resource here, just directly notify
		 * the req event to the user space to handle it.
		 */
		rte_dev_event_callback_process(dev->name,
					       RTE_DEV_EVENT_REMOVE);
		break;
	case RTE_PCI_KDRV_IGB_UIO:
	case RTE_PCI_KDRV_UIO_GENERIC:
	case RTE_PCI_KDRV_NIC_UIO:
		/* BARs resource is invalid, remap it to be safe. */
		ret = pci_uio_remap_resource(pdev);
		break;
	default:
		PCI_LOG(DEBUG, "Not managed by a supported kernel driver, skipped");
		ret = -1;
		break;
	}

	return ret;
}

static int
pci_sigbus_handler(const void *failure_addr)
{
	struct rte_pci_device *pdev = NULL;
	int ret = 0;

	pdev = pci_find_device_by_addr(failure_addr);
	if (!pdev) {
		/* It is a generic sigbus error, no bus would handle it. */
		ret = 1;
	} else {
		/* The sigbus error is caused of hot-unplug. */
		ret = pci_hot_unplug_handler(&pdev->device);
		if (ret) {
			PCI_LOG(ERR, "Failed to handle hot-unplug for device %s",
				pdev->name);
			ret = -1;
		}
	}
	return ret;
}

static int
pci_dma_map(struct rte_device *dev, void *addr, uint64_t iova, size_t len)
{
	struct rte_pci_device *pdev = RTE_BUS_DEVICE(dev, *pdev);
	const struct rte_pci_driver *pdrv = RTE_BUS_DRIVER(dev->driver, *pdrv);

	if (pdrv->dma_map != NULL)
		return pdrv->dma_map(pdev, addr, iova, len);
	/**
	 *  In case driver don't provides any specific mapping
	 *  try fallback to VFIO.
	 */
	if (pdev->kdrv == RTE_PCI_KDRV_VFIO)
		return rte_vfio_container_dma_map
				(RTE_VFIO_DEFAULT_CONTAINER_FD, (uintptr_t)addr,
				 iova, len);
	rte_errno = ENOTSUP;
	return -1;
}

static int
pci_dma_unmap(struct rte_device *dev, void *addr, uint64_t iova, size_t len)
{
	struct rte_pci_device *pdev = RTE_BUS_DEVICE(dev, *pdev);
	const struct rte_pci_driver *pdrv = RTE_BUS_DRIVER(dev->driver, *pdrv);

	if (pdrv->dma_unmap != NULL)
		return pdrv->dma_unmap(pdev, addr, iova, len);
	/**
	 *  In case driver don't provides any specific mapping
	 *  try fallback to VFIO.
	 */
	if (pdev->kdrv == RTE_PCI_KDRV_VFIO)
		return rte_vfio_container_dma_unmap
				(RTE_VFIO_DEFAULT_CONTAINER_FD, (uintptr_t)addr,
				 iova, len);
	rte_errno = ENOTSUP;
	return -1;
}

enum rte_iova_mode
rte_pci_get_iommu_class(void)
{
	enum rte_iova_mode iova_mode = RTE_IOVA_DC;
	const struct rte_pci_device *dev;
	const struct rte_pci_driver *drv;
	bool devices_want_va = false;
	bool devices_want_pa = false;
	int iommu_no_va = -1;

	RTE_BUS_FOREACH_DEV(dev, &rte_pci_bus) {
		/*
		 * We can check this only once, because the IOMMU hardware is
		 * the same for all of them.
		 */
		if (iommu_no_va == -1)
			iommu_no_va = pci_device_iommu_support_va(dev)
					? 0 : 1;

		if (dev->kdrv == RTE_PCI_KDRV_UNKNOWN ||
		    dev->kdrv == RTE_PCI_KDRV_NONE)
			continue;
		RTE_BUS_FOREACH_DRV(drv, &rte_pci_bus) {
			enum rte_iova_mode dev_iova_mode;

			if (!pci_bus_match(&drv->driver, &dev->device))
				continue;

			dev_iova_mode = pci_device_iova_mode(drv, dev);
			PCI_LOG(DEBUG, "PCI driver %s for device "PCI_PRI_FMT" wants IOVA as '%s'",
				drv->driver.name,
				dev->addr.domain, dev->addr.bus,
				dev->addr.devid, dev->addr.function,
				dev_iova_mode == RTE_IOVA_DC ? "DC" :
				(dev_iova_mode == RTE_IOVA_PA ? "PA" : "VA"));
			if (dev_iova_mode == RTE_IOVA_PA)
				devices_want_pa = true;
			else if (dev_iova_mode == RTE_IOVA_VA)
				devices_want_va = true;
		}
	}
	if (iommu_no_va == 1) {
		iova_mode = RTE_IOVA_PA;
		if (devices_want_va) {
			PCI_LOG(WARNING, "Some devices want 'VA' but IOMMU does not support 'VA'.");
			PCI_LOG(WARNING, "The devices that want 'VA' won't initialize.");
		}
	} else if (devices_want_va && !devices_want_pa) {
		iova_mode = RTE_IOVA_VA;
	} else if (devices_want_pa && !devices_want_va) {
		iova_mode = RTE_IOVA_PA;
	} else {
		iova_mode = RTE_IOVA_DC;
		if (devices_want_va) {
			PCI_LOG(WARNING, "Some devices want 'VA' but forcing 'DC' because other devices want 'PA'.");
			PCI_LOG(WARNING, "Depending on the final decision by the EAL, not all devices may be able to initialize.");
		}
	}
	return iova_mode;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pci_has_capability_list, 23.11)
bool
rte_pci_has_capability_list(const struct rte_pci_device *dev)
{
	uint16_t status;

	if (rte_pci_read_config(dev, &status, sizeof(status), RTE_PCI_STATUS) != sizeof(status))
		return false;

	return (status & RTE_PCI_STATUS_CAP_LIST) != 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pci_find_capability, 23.11)
off_t
rte_pci_find_capability(const struct rte_pci_device *dev, uint8_t cap)
{
	return rte_pci_find_next_capability(dev, cap, 0);
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pci_find_next_capability, 23.11)
off_t
rte_pci_find_next_capability(const struct rte_pci_device *dev, uint8_t cap,
	off_t offset)
{
	uint8_t pos;
	int ttl;

	if (offset == 0)
		offset = RTE_PCI_CAPABILITY_LIST;
	else
		offset += RTE_PCI_CAP_NEXT;
	ttl = (RTE_PCI_CFG_SPACE_SIZE - RTE_PCI_STD_HEADER_SIZEOF) / RTE_PCI_CAP_SIZEOF;

	if (rte_pci_read_config(dev, &pos, sizeof(pos), offset) < 0)
		return -1;

	while (pos && ttl--) {
		uint16_t ent;
		uint8_t id;

		offset = pos;
		if (rte_pci_read_config(dev, &ent, sizeof(ent), offset) < 0)
			return -1;

		id = ent & 0xff;
		if (id == 0xff)
			break;

		if (id == cap)
			return offset;

		pos = (ent >> 8);
	}

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pci_find_ext_capability, 20.11)
off_t
rte_pci_find_ext_capability(const struct rte_pci_device *dev, uint32_t cap)
{
	off_t offset = RTE_PCI_CFG_SPACE_SIZE;
	uint32_t header;
	int ttl;

	/* minimum 8 bytes per capability */
	ttl = (RTE_PCI_CFG_SPACE_EXP_SIZE - RTE_PCI_CFG_SPACE_SIZE) / 8;

	if (rte_pci_read_config(dev, &header, 4, offset) < 0) {
		PCI_LOG(ERR, "error in reading extended capabilities");
		return -1;
	}

	/*
	 * If we have no capabilities, this is indicated by cap ID,
	 * cap version and next pointer all being 0.
	 */
	if (header == 0)
		return 0;

	while (ttl != 0) {
		if (RTE_PCI_EXT_CAP_ID(header) == cap)
			return offset;

		offset = RTE_PCI_EXT_CAP_NEXT(header);

		if (offset < RTE_PCI_CFG_SPACE_SIZE)
			break;

		if (rte_pci_read_config(dev, &header, 4, offset) < 0) {
			PCI_LOG(ERR, "error in reading extended capabilities");
			return -1;
		}

		ttl--;
	}

	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pci_set_bus_master, 21.08)
int
rte_pci_set_bus_master(const struct rte_pci_device *dev, bool enable)
{
	uint16_t old_cmd, cmd;

	if (rte_pci_read_config(dev, &old_cmd, sizeof(old_cmd),
				RTE_PCI_COMMAND) < 0) {
		PCI_LOG(ERR, "error in reading PCI command register");
		return -1;
	}

	if (enable)
		cmd = old_cmd | RTE_PCI_COMMAND_MASTER;
	else
		cmd = old_cmd & ~RTE_PCI_COMMAND_MASTER;

	if (cmd == old_cmd)
		return 0;

	if (rte_pci_write_config(dev, &cmd, sizeof(cmd),
				 RTE_PCI_COMMAND) < 0) {
		PCI_LOG(ERR, "error in writing PCI command register");
		return -1;
	}

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(rte_pci_pasid_set_state)
int
rte_pci_pasid_set_state(const struct rte_pci_device *dev,
		off_t offset, bool enable)
{
	uint16_t pasid = enable;
	return rte_pci_write_config(dev, &pasid, sizeof(pasid),
			offset + RTE_PCI_PASID_CTRL) != sizeof(pasid) ? -1 : 0;
}

struct rte_bus rte_pci_bus = {
	.allow_multi_probe = true,
	.scan = rte_pci_scan,
	.probe = rte_bus_generic_probe,
	.free_device = pci_free_device,
	.cleanup = rte_bus_generic_cleanup,
	.find_device = rte_bus_generic_find_device,
	.match = pci_bus_match,
	.probe_device = pci_probe_device,
	.unplug_device = pci_unplug_device,
	.parse = pci_parse,
	.dev_compare = pci_dev_compare,
	.devargs_parse = rte_pci_devargs_parse,
	.dma_map = pci_dma_map,
	.dma_unmap = pci_dma_unmap,
	.get_iommu_class = rte_pci_get_iommu_class,
	.dev_iterate = rte_pci_dev_iterate,
	.hot_unplug_handler = pci_hot_unplug_handler,
	.sigbus_handler = pci_sigbus_handler,
};

RTE_REGISTER_BUS(pci, rte_pci_bus);
RTE_LOG_REGISTER_DEFAULT(pci_bus_logtype, NOTICE);
