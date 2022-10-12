/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <unistd.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_byteorder.h>
#include <rte_errno.h>
#include <rte_branch_prediction.h>
#include <rte_hexdump.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#ifdef RTE_BBDEV_OFFLOAD_COST
#include <rte_cycles.h>
#endif

#include <rte_bbdev.h>
#include <rte_bbdev_pmd.h>
#include "acc200_pmd.h"

#ifdef RTE_LIBRTE_BBDEV_DEBUG
RTE_LOG_REGISTER_DEFAULT(acc200_logtype, DEBUG);
#else
RTE_LOG_REGISTER_DEFAULT(acc200_logtype, NOTICE);
#endif

static int
acc200_dev_close(struct rte_bbdev *dev)
{
	RTE_SET_USED(dev);
	/* Ensure all in flight HW transactions are completed. */
	usleep(ACC_LONG_WAIT);
	return 0;
}


static const struct rte_bbdev_ops acc200_bbdev_ops = {
	.close = acc200_dev_close,
};

/* ACC200 PCI PF address map. */
static struct rte_pci_id pci_id_acc200_pf_map[] = {
	{
		RTE_PCI_DEVICE(RTE_ACC200_VENDOR_ID, RTE_ACC200_PF_DEVICE_ID)
	},
	{.device_id = 0},
};

/* ACC200 PCI VF address map. */
static struct rte_pci_id pci_id_acc200_vf_map[] = {
	{
		RTE_PCI_DEVICE(RTE_ACC200_VENDOR_ID, RTE_ACC200_VF_DEVICE_ID)
	},
	{.device_id = 0},
};

/* Initialization Function. */
static void
acc200_bbdev_init(struct rte_bbdev *dev, struct rte_pci_driver *drv)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

	dev->dev_ops = &acc200_bbdev_ops;

	((struct acc_device *) dev->data->dev_private)->pf_device =
			!strcmp(drv->driver.name,
					RTE_STR(ACC200PF_DRIVER_NAME));
	((struct acc_device *) dev->data->dev_private)->mmio_base =
			pci_dev->mem_resource[0].addr;

	rte_bbdev_log_debug("Init device %s [%s] @ vaddr %p paddr %#"PRIx64"",
			drv->driver.name, dev->data->name,
			(void *)pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[0].phys_addr);
}

static int acc200_pci_probe(struct rte_pci_driver *pci_drv,
	struct rte_pci_device *pci_dev)
{
	struct rte_bbdev *bbdev = NULL;
	char dev_name[RTE_BBDEV_NAME_MAX_LEN];

	if (pci_dev == NULL) {
		rte_bbdev_log(ERR, "NULL PCI device");
		return -EINVAL;
	}

	rte_pci_device_name(&pci_dev->addr, dev_name, sizeof(dev_name));

	/* Allocate memory to be used privately by drivers. */
	bbdev = rte_bbdev_allocate(pci_dev->device.name);
	if (bbdev == NULL)
		return -ENODEV;

	/* allocate device private memory. */
	bbdev->data->dev_private = rte_zmalloc_socket(dev_name,
			sizeof(struct acc_device), RTE_CACHE_LINE_SIZE,
			pci_dev->device.numa_node);

	if (bbdev->data->dev_private == NULL) {
		rte_bbdev_log(CRIT,
				"Allocate of %zu bytes for device \"%s\" failed",
				sizeof(struct acc_device), dev_name);
				rte_bbdev_release(bbdev);
			return -ENOMEM;
	}

	/* Fill HW specific part of device structure. */
	bbdev->device = &pci_dev->device;
	bbdev->intr_handle = pci_dev->intr_handle;
	bbdev->data->socket_id = pci_dev->device.numa_node;

	/* Invoke ACC200 device initialization function. */
	acc200_bbdev_init(bbdev, pci_drv);

	rte_bbdev_log_debug("Initialised bbdev %s (id = %u)",
			dev_name, bbdev->data->dev_id);
	return 0;
}

static struct rte_pci_driver acc200_pci_pf_driver = {
		.probe = acc200_pci_probe,
		.remove = acc_pci_remove,
		.id_table = pci_id_acc200_pf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

static struct rte_pci_driver acc200_pci_vf_driver = {
		.probe = acc200_pci_probe,
		.remove = acc_pci_remove,
		.id_table = pci_id_acc200_vf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

RTE_PMD_REGISTER_PCI(ACC200PF_DRIVER_NAME, acc200_pci_pf_driver);
RTE_PMD_REGISTER_PCI_TABLE(ACC200PF_DRIVER_NAME, pci_id_acc200_pf_map);
RTE_PMD_REGISTER_PCI(ACC200VF_DRIVER_NAME, acc200_pci_vf_driver);
RTE_PMD_REGISTER_PCI_TABLE(ACC200VF_DRIVER_NAME, pci_id_acc200_vf_map);
