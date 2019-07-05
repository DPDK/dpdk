/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <string.h>
#include <unistd.h>

#include <rte_bus.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include <otx2_common.h>

#include "otx2_dpi_rawdev.h"

static const struct rte_pci_id pci_dma_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_DPI_VF)
	},
	{
		.vendor_id = 0,
	},
};

static int
otx2_dpi_rawdev_probe(struct rte_pci_driver *pci_drv __rte_unused,
		      struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct dpi_vf_s *dpivf = NULL;
	struct rte_rawdev *rawdev;
	uint16_t vf_id;

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return DPI_DMA_QUEUE_SUCCESS;

	if (pci_dev->mem_resource[0].addr == NULL) {
		otx2_dpi_dbg("Empty bars %p %p", pci_dev->mem_resource[0].addr,
			     pci_dev->mem_resource[2].addr);
		return -ENODEV;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "DPI:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	/* Allocate device structure */
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(struct dpi_vf_s),
					 rte_socket_id());
	if (rawdev == NULL) {
		otx2_err("Rawdev allocation failed");
		return -EINVAL;
	}

	rawdev->device = &pci_dev->device;
	rawdev->driver_name = pci_dev->driver->driver.name;

	dpivf = rawdev->dev_private;
	if (dpivf->state != DPI_QUEUE_STOP) {
		otx2_dpi_dbg("Device already started!!!");
		return -ENODEV;
	}

	vf_id = ((pci_dev->addr.devid & 0x1F) << 3) |
		 (pci_dev->addr.function & 0x7);
	vf_id -= 1;
	dpivf->state = DPI_QUEUE_START;
	dpivf->vf_id = vf_id;
	dpivf->vf_bar0 = (uintptr_t)pci_dev->mem_resource[0].addr;
	dpivf->vf_bar2 = (uintptr_t)pci_dev->mem_resource[2].addr;

	return DPI_DMA_QUEUE_SUCCESS;
}

static int
otx2_dpi_rawdev_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev;

	if (pci_dev == NULL) {
		otx2_dpi_dbg("Invalid pci_dev of the device!");
		return -EINVAL;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "DPI:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (rawdev == NULL) {
		otx2_dpi_dbg("Invalid device name (%s)", name);
		return -EINVAL;
	}

	/* rte_rawdev_close is called by pmd_release */
	return rte_rawdev_pmd_release(rawdev);
}

static struct rte_pci_driver rte_dpi_rawdev_pmd = {
	.id_table  = pci_dma_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_IOVA_AS_VA,
	.probe     = otx2_dpi_rawdev_probe,
	.remove    = otx2_dpi_rawdev_remove,
};

RTE_PMD_REGISTER_PCI(dpi_rawdev_pci_driver, rte_dpi_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(dpi_rawdev_pci_driver, pci_dma_map);
RTE_PMD_REGISTER_KMOD_DEP(dpi_rawdev_pci_driver, "vfio-pci");
