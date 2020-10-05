/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
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

#include <rte_bbdev.h>
#include <rte_bbdev_pmd.h>
#include "rte_acc100_pmd.h"

#ifdef RTE_LIBRTE_BBDEV_DEBUG
RTE_LOG_REGISTER(acc100_logtype, pmd.bb.acc100, DEBUG);
#else
RTE_LOG_REGISTER(acc100_logtype, pmd.bb.acc100, NOTICE);
#endif

/* Free 64MB memory used for software rings */
static int
acc100_dev_close(struct rte_bbdev *dev  __rte_unused)
{
	return 0;
}

static const struct rte_bbdev_ops acc100_bbdev_ops = {
	.close = acc100_dev_close,
};

/* ACC100 PCI PF address map */
static struct rte_pci_id pci_id_acc100_pf_map[] = {
	{
		RTE_PCI_DEVICE(RTE_ACC100_VENDOR_ID, RTE_ACC100_PF_DEVICE_ID)
	},
	{.device_id = 0},
};

/* ACC100 PCI VF address map */
static struct rte_pci_id pci_id_acc100_vf_map[] = {
	{
		RTE_PCI_DEVICE(RTE_ACC100_VENDOR_ID, RTE_ACC100_VF_DEVICE_ID)
	},
	{.device_id = 0},
};

/* Initialization Function */
static void
acc100_bbdev_init(struct rte_bbdev *dev, struct rte_pci_driver *drv)
{
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);

	dev->dev_ops = &acc100_bbdev_ops;

	((struct acc100_device *) dev->data->dev_private)->pf_device =
			!strcmp(drv->driver.name,
					RTE_STR(ACC100PF_DRIVER_NAME));
	((struct acc100_device *) dev->data->dev_private)->mmio_base =
			pci_dev->mem_resource[0].addr;

	rte_bbdev_log_debug("Init device %s [%s] @ vaddr %p paddr %#"PRIx64"",
			drv->driver.name, dev->data->name,
			(void *)pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[0].phys_addr);
}

static int acc100_pci_probe(struct rte_pci_driver *pci_drv,
	struct rte_pci_device *pci_dev)
{
	struct rte_bbdev *bbdev = NULL;
	char dev_name[RTE_BBDEV_NAME_MAX_LEN];

	if (pci_dev == NULL) {
		rte_bbdev_log(ERR, "NULL PCI device");
		return -EINVAL;
	}

	rte_pci_device_name(&pci_dev->addr, dev_name, sizeof(dev_name));

	/* Allocate memory to be used privately by drivers */
	bbdev = rte_bbdev_allocate(pci_dev->device.name);
	if (bbdev == NULL)
		return -ENODEV;

	/* allocate device private memory */
	bbdev->data->dev_private = rte_zmalloc_socket(dev_name,
			sizeof(struct acc100_device), RTE_CACHE_LINE_SIZE,
			pci_dev->device.numa_node);

	if (bbdev->data->dev_private == NULL) {
		rte_bbdev_log(CRIT,
				"Allocate of %zu bytes for device \"%s\" failed",
				sizeof(struct acc100_device), dev_name);
				rte_bbdev_release(bbdev);
			return -ENOMEM;
	}

	/* Fill HW specific part of device structure */
	bbdev->device = &pci_dev->device;
	bbdev->intr_handle = &pci_dev->intr_handle;
	bbdev->data->socket_id = pci_dev->device.numa_node;

	/* Invoke ACC100 device initialization function */
	acc100_bbdev_init(bbdev, pci_drv);

	rte_bbdev_log_debug("Initialised bbdev %s (id = %u)",
			dev_name, bbdev->data->dev_id);
	return 0;
}

static int acc100_pci_remove(struct rte_pci_device *pci_dev)
{
	struct rte_bbdev *bbdev;
	int ret;
	uint8_t dev_id;

	if (pci_dev == NULL)
		return -EINVAL;

	/* Find device */
	bbdev = rte_bbdev_get_named_dev(pci_dev->device.name);
	if (bbdev == NULL) {
		rte_bbdev_log(CRIT,
				"Couldn't find HW dev \"%s\" to uninitialise it",
				pci_dev->device.name);
		return -ENODEV;
	}
	dev_id = bbdev->data->dev_id;

	/* free device private memory before close */
	rte_free(bbdev->data->dev_private);

	/* Close device */
	ret = rte_bbdev_close(dev_id);
	if (ret < 0)
		rte_bbdev_log(ERR,
				"Device %i failed to close during uninit: %i",
				dev_id, ret);

	/* release bbdev from library */
	rte_bbdev_release(bbdev);

	rte_bbdev_log_debug("Destroyed bbdev = %u", dev_id);

	return 0;
}

static struct rte_pci_driver acc100_pci_pf_driver = {
		.probe = acc100_pci_probe,
		.remove = acc100_pci_remove,
		.id_table = pci_id_acc100_pf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

static struct rte_pci_driver acc100_pci_vf_driver = {
		.probe = acc100_pci_probe,
		.remove = acc100_pci_remove,
		.id_table = pci_id_acc100_vf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING
};

RTE_PMD_REGISTER_PCI(ACC100PF_DRIVER_NAME, acc100_pci_pf_driver);
RTE_PMD_REGISTER_PCI_TABLE(ACC100PF_DRIVER_NAME, pci_id_acc100_pf_map);
RTE_PMD_REGISTER_PCI(ACC100VF_DRIVER_NAME, acc100_pci_vf_driver);
RTE_PMD_REGISTER_PCI_TABLE(ACC100VF_DRIVER_NAME, pci_id_acc100_vf_map);
