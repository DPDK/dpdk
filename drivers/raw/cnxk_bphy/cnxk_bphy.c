/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include <roc_api.h>

#include "cnxk_bphy_irq.h"

static const struct rte_pci_id pci_bphy_map[] = {
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CNXK_BPHY)},
	{
		.vendor_id = 0,
	},
};

static void
bphy_rawdev_get_name(char *name, struct rte_pci_device *pci_dev)
{
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "BPHY:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);
}

static uint16_t
cnxk_bphy_irq_queue_count(struct rte_rawdev *dev)
{
	struct bphy_device *bphy_dev = (struct bphy_device *)dev->dev_private;

	return RTE_DIM(bphy_dev->queues);
}

static int
cnxk_bphy_irq_queue_def_conf(struct rte_rawdev *dev, uint16_t queue_id,
			     rte_rawdev_obj_t queue_conf,
			     size_t queue_conf_size)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);

	if (queue_conf_size != sizeof(unsigned int))
		return -EINVAL;

	*(unsigned int *)queue_conf = 1;

	return 0;
}

static const struct rte_rawdev_ops bphy_rawdev_ops = {
	.queue_def_conf = cnxk_bphy_irq_queue_def_conf,
	.queue_count = cnxk_bphy_irq_queue_count,
};

static int
bphy_rawdev_probe(struct rte_pci_driver *pci_drv,
		  struct rte_pci_device *pci_dev)
{
	struct bphy_device *bphy_dev = NULL;
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *bphy_rawdev;
	int ret;

	RTE_SET_USED(pci_drv);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (!pci_dev->mem_resource[0].addr) {
		plt_err("BARs have invalid values: BAR0 %p\n BAR2 %p",
			pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[2].addr);
		return -ENODEV;
	}

	ret = roc_plt_init();
	if (ret)
		return ret;

	bphy_rawdev_get_name(name, pci_dev);
	bphy_rawdev = rte_rawdev_pmd_allocate(name, sizeof(*bphy_dev),
					      rte_socket_id());
	if (bphy_rawdev == NULL) {
		plt_err("Failed to allocate rawdev");
		return -ENOMEM;
	}

	bphy_rawdev->dev_ops = &bphy_rawdev_ops;
	bphy_rawdev->device = &pci_dev->device;
	bphy_rawdev->driver_name = pci_dev->driver->driver.name;

	bphy_dev = (struct bphy_device *)bphy_rawdev->dev_private;
	bphy_dev->mem.res0 = pci_dev->mem_resource[0];
	bphy_dev->mem.res2 = pci_dev->mem_resource[2];

	return 0;
}

static int
bphy_rawdev_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev == NULL) {
		plt_err("invalid pci_dev");
		return -EINVAL;
	}

	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (rawdev == NULL) {
		plt_err("invalid device name (%s)", name);
		return -EINVAL;
	}

	bphy_rawdev_get_name(name, pci_dev);

	return rte_rawdev_pmd_release(rawdev);
}

static struct rte_pci_driver cnxk_bphy_rawdev_pmd = {
	.id_table = pci_bphy_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = bphy_rawdev_probe,
	.remove = bphy_rawdev_remove,
};

RTE_PMD_REGISTER_PCI(bphy_rawdev_pci_driver, cnxk_bphy_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(bphy_rawdev_pci_driver, pci_bphy_map);
RTE_PMD_REGISTER_KMOD_DEP(bphy_rawdev_pci_driver, "vfio-pci");
