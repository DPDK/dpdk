/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <rte_bus_pci.h>
#include <rte_dmadev_pmd.h>
#include <rte_malloc.h>

#include "ioat_internal.h"

static struct rte_pci_driver ioat_pmd_drv;

RTE_LOG_REGISTER_DEFAULT(ioat_pmd_logtype, INFO);

#define IOAT_PMD_NAME dmadev_ioat
#define IOAT_PMD_NAME_STR RTE_STR(IOAT_PMD_NAME)

/* Create a DMA device. */
static int
ioat_dmadev_create(const char *name, struct rte_pci_device *dev)
{
	static const struct rte_dma_dev_ops ioat_dmadev_ops = { };

	struct rte_dma_dev *dmadev = NULL;
	struct ioat_dmadev *ioat = NULL;
	int retry = 0;

	if (!name) {
		IOAT_PMD_ERR("Invalid name of the device!");
		return -EINVAL;
	}

	/* Allocate device structure. */
	dmadev = rte_dma_pmd_allocate(name, dev->device.numa_node, sizeof(struct ioat_dmadev));
	if (dmadev == NULL) {
		IOAT_PMD_ERR("Unable to allocate dma device");
		return -ENOMEM;
	}

	dmadev->device = &dev->device;

	dmadev->fp_obj->dev_private = dmadev->data->dev_private;

	dmadev->dev_ops = &ioat_dmadev_ops;

	ioat = dmadev->data->dev_private;
	ioat->dmadev = dmadev;
	ioat->regs = dev->mem_resource[0].addr;
	ioat->doorbell = &ioat->regs->dmacount;
	ioat->qcfg.nb_desc = 0;
	ioat->desc_ring = NULL;
	ioat->version = ioat->regs->cbver;

	/* Do device initialization - reset and set error behaviour. */
	if (ioat->regs->chancnt != 1)
		IOAT_PMD_WARN("%s: Channel count == %d\n", __func__,
				ioat->regs->chancnt);

	/* Locked by someone else. */
	if (ioat->regs->chanctrl & IOAT_CHANCTRL_CHANNEL_IN_USE) {
		IOAT_PMD_WARN("%s: Channel appears locked\n", __func__);
		ioat->regs->chanctrl = 0;
	}

	/* clear any previous errors */
	if (ioat->regs->chanerr != 0) {
		uint32_t val = ioat->regs->chanerr;
		ioat->regs->chanerr = val;
	}

	ioat->regs->chancmd = IOAT_CHANCMD_SUSPEND;
	rte_delay_ms(1);
	ioat->regs->chancmd = IOAT_CHANCMD_RESET;
	rte_delay_ms(1);
	while (ioat->regs->chancmd & IOAT_CHANCMD_RESET) {
		ioat->regs->chainaddr = 0;
		rte_delay_ms(1);
		if (++retry >= 200) {
			IOAT_PMD_ERR("%s: cannot reset device. CHANCMD=%#"PRIx8
					", CHANSTS=%#"PRIx64", CHANERR=%#"PRIx32"\n",
					__func__,
					ioat->regs->chancmd,
					ioat->regs->chansts,
					ioat->regs->chanerr);
			rte_dma_pmd_release(name);
			return -EIO;
		}
	}
	ioat->regs->chanctrl = IOAT_CHANCTRL_ANY_ERR_ABORT_EN |
			IOAT_CHANCTRL_ERR_COMPLETION_EN;

	dmadev->fp_obj->dev_private = ioat;

	dmadev->state = RTE_DMA_DEV_READY;

	return 0;

}

/* Destroy a DMA device. */
static int
ioat_dmadev_destroy(const char *name)
{
	int ret;

	if (!name) {
		IOAT_PMD_ERR("Invalid device name");
		return -EINVAL;
	}

	ret = rte_dma_pmd_release(name);
	if (ret)
		IOAT_PMD_DEBUG("Device cleanup failed");

	return 0;
}

/* Probe DMA device. */
static int
ioat_dmadev_probe(struct rte_pci_driver *drv, struct rte_pci_device *dev)
{
	char name[32];

	rte_pci_device_name(&dev->addr, name, sizeof(name));
	IOAT_PMD_INFO("Init %s on NUMA node %d", name, dev->device.numa_node);

	dev->device.driver = &drv->driver;
	return ioat_dmadev_create(name, dev);
}

/* Remove DMA device. */
static int
ioat_dmadev_remove(struct rte_pci_device *dev)
{
	char name[32];

	rte_pci_device_name(&dev->addr, name, sizeof(name));

	IOAT_PMD_INFO("Closing %s on NUMA node %d",
			name, dev->device.numa_node);

	return ioat_dmadev_destroy(name);
}

static const struct rte_pci_id pci_id_ioat_map[] = {
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_SKX) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX0) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX1) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX2) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX3) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX4) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX5) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX6) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDX7) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDXE) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_BDXF) },
	{ RTE_PCI_DEVICE(IOAT_VENDOR_ID, IOAT_DEVICE_ID_ICX) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver ioat_pmd_drv = {
	.id_table = pci_id_ioat_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = ioat_dmadev_probe,
	.remove = ioat_dmadev_remove,
};

RTE_PMD_REGISTER_PCI(IOAT_PMD_NAME, ioat_pmd_drv);
RTE_PMD_REGISTER_PCI_TABLE(IOAT_PMD_NAME, pci_id_ioat_map);
RTE_PMD_REGISTER_KMOD_DEP(IOAT_PMD_NAME, "* igb_uio | uio_pci_generic | vfio-pci");
