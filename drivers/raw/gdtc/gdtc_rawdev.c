/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2024 ZTE Corporation
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <bus_pci_driver.h>
#include <rte_atomic.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_eal_paging.h>
#include <rte_errno.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include <rte_spinlock.h>
#include <rte_branch_prediction.h>

#include "gdtc_rawdev.h"

/* Register offset */
#define ZXDH_GDMA_BASE_OFFSET                   0x100000

#define ZXDH_GDMA_CHAN_SHIFT                    0x80
char zxdh_gdma_driver_name[] = "rawdev_zxdh_gdma";
char dev_name[] = "zxdh_gdma";

static inline struct zxdh_gdma_rawdev *
zxdh_gdma_rawdev_get_priv(const struct rte_rawdev *rawdev)
{
	return rawdev->dev_private;
}

static const struct rte_rawdev_ops zxdh_gdma_rawdev_ops = {
};

static int
zxdh_gdma_map_resource(struct rte_pci_device *dev)
{
	int fd = -1;
	char devname[PATH_MAX];
	void *mapaddr = NULL;
	struct rte_pci_addr *loc;

	loc = &dev->addr;
	snprintf(devname, sizeof(devname), "%s/" PCI_PRI_FMT "/resource0",
		rte_pci_get_sysfs_path(),
		loc->domain, loc->bus, loc->devid,
		loc->function);

		fd = open(devname, O_RDWR);
		if (fd < 0) {
			ZXDH_PMD_LOG(ERR, "Cannot open %s: %s", devname, strerror(errno));
			return -1;
		}

	/* Map the PCI memory resource of device */
	mapaddr = rte_mem_map(NULL, (size_t)dev->mem_resource[0].len,
			RTE_PROT_READ | RTE_PROT_WRITE,
			RTE_MAP_SHARED, fd, 0);
	if (mapaddr == NULL) {
		ZXDH_PMD_LOG(ERR, "cannot map resource(%d, 0x%zx): %s (%p)",
				fd, (size_t)dev->mem_resource[0].len,
				rte_strerror(rte_errno), mapaddr);
		close(fd);
		return -1;
	}

	close(fd);
	dev->mem_resource[0].addr = mapaddr;

	return 0;
}

static void
zxdh_gdma_unmap_resource(void *requested_addr, size_t size)
{
	if (requested_addr == NULL)
		return;

	/* Unmap the PCI memory resource of device */
	if (rte_mem_unmap(requested_addr, size))
		ZXDH_PMD_LOG(ERR, "cannot mem unmap(%p, %#zx): %s",
				requested_addr, size, rte_strerror(rte_errno));
	else
		ZXDH_PMD_LOG(DEBUG, "PCI memory unmapped at %p", requested_addr);
}

static int
zxdh_gdma_rawdev_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	struct rte_rawdev *dev = NULL;
	struct zxdh_gdma_rawdev *gdmadev = NULL;
	struct zxdh_gdma_queue *queue = NULL;
	uint8_t i = 0;
	int ret;

	if (pci_dev->mem_resource[0].phys_addr == 0) {
		ZXDH_PMD_LOG(ERR, "PCI bar0 resource is invalid");
		return -1;
	}

	ret = zxdh_gdma_map_resource(pci_dev);
	if (ret != 0) {
		ZXDH_PMD_LOG(ERR, "Failed to mmap pci device(%s)", pci_dev->name);
		return -1;
	}
	ZXDH_PMD_LOG(INFO, "%s bar0 0x%"PRIx64" mapped at %p",
			pci_dev->name, pci_dev->mem_resource[0].phys_addr,
			pci_dev->mem_resource[0].addr);

	dev = rte_rawdev_pmd_allocate(dev_name, sizeof(struct zxdh_gdma_rawdev), rte_socket_id());
	if (dev == NULL) {
		ZXDH_PMD_LOG(ERR, "Unable to allocate gdma rawdev");
		goto err_out;
	}
	ZXDH_PMD_LOG(INFO, "Init %s on NUMA node %d, dev_id is %d",
			dev_name, rte_socket_id(), dev->dev_id);

	dev->dev_ops = &zxdh_gdma_rawdev_ops;
	dev->device = &pci_dev->device;
	dev->driver_name = zxdh_gdma_driver_name;
	gdmadev = zxdh_gdma_rawdev_get_priv(dev);
	gdmadev->device_state = ZXDH_GDMA_DEV_STOPPED;
	gdmadev->rawdev = dev;
	gdmadev->queue_num = ZXDH_GDMA_TOTAL_CHAN_NUM;
	gdmadev->used_num = 0;
	gdmadev->base_addr = (uintptr_t)pci_dev->mem_resource[0].addr + ZXDH_GDMA_BASE_OFFSET;

	for (i = 0; i < ZXDH_GDMA_TOTAL_CHAN_NUM; i++) {
		queue = &(gdmadev->vqs[i]);
		queue->enable = 0;
		queue->queue_size = ZXDH_GDMA_QUEUE_SIZE;
		rte_spinlock_init(&(queue->enqueue_lock));
	}

	return 0;

err_out:
	zxdh_gdma_unmap_resource(pci_dev->mem_resource[0].addr,
			(size_t)pci_dev->mem_resource[0].len);
	return -1;
}

static int
zxdh_gdma_rawdev_remove(struct rte_pci_device *pci_dev)
{
	struct rte_rawdev *dev = NULL;
	int ret = 0;

	dev = rte_rawdev_pmd_get_named_dev(dev_name);
	if (dev == NULL)
		return -EINVAL;

	/* rte_rawdev_close is called by pmd_release */
	ret = rte_rawdev_pmd_release(dev);
	if (ret != 0) {
		ZXDH_PMD_LOG(ERR, "Device cleanup failed");
		return -1;
	}

	zxdh_gdma_unmap_resource(pci_dev->mem_resource[0].addr,
			(size_t)pci_dev->mem_resource[0].len);

	ZXDH_PMD_LOG(DEBUG, "rawdev %s remove done!", dev_name);

	return ret;
}

static const struct rte_pci_id zxdh_gdma_rawdev_map[] = {
	{ RTE_PCI_DEVICE(ZXDH_GDMA_VENDORID, ZXDH_GDMA_DEVICEID) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver zxdh_gdma_rawdev_pmd = {
	.id_table = zxdh_gdma_rawdev_map,
	.drv_flags = 0,
	.probe = zxdh_gdma_rawdev_probe,
	.remove = zxdh_gdma_rawdev_remove,
};

RTE_PMD_REGISTER_PCI(zxdh_gdma_rawdev_pci_driver, zxdh_gdma_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(zxdh_gdma_rawdev_pci_driver, zxdh_gdma_rawdev_map);
RTE_LOG_REGISTER_DEFAULT(zxdh_gdma_rawdev_logtype, NOTICE);
