/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2021 Marvell International Ltd.
 */

#include <string.h>
#include <unistd.h>

#include <rte_bus.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_mempool.h>
#include <rte_pci.h>
#include <rte_dmadev.h>
#include <rte_dmadev_pmd.h>

#include <roc_api.h>
#include <cnxk_dmadev.h>

static int
cnxk_dmadev_probe(struct rte_pci_driver *pci_drv __rte_unused,
		  struct rte_pci_device *pci_dev)
{
	struct cnxk_dpi_vf_s *dpivf = NULL;
	char name[RTE_DEV_NAME_MAX_LEN];
	struct rte_dma_dev *dmadev;
	struct roc_dpi *rdpi = NULL;
	int rc;

	if (!pci_dev->mem_resource[0].addr)
		return -ENODEV;

	rc = roc_plt_init();
	if (rc) {
		plt_err("Failed to initialize platform model, rc=%d", rc);
		return rc;
	}
	memset(name, 0, sizeof(name));
	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	dmadev = rte_dma_pmd_allocate(name, pci_dev->device.numa_node,
				      sizeof(*dpivf));
	if (dmadev == NULL) {
		plt_err("dma device allocation failed for %s", name);
		return -ENOMEM;
	}

	dpivf = dmadev->data->dev_private;

	dmadev->device = &pci_dev->device;
	dmadev->fp_obj->dev_private = dpivf;

	rdpi = &dpivf->rdpi;

	rdpi->pci_dev = pci_dev;
	rc = roc_dpi_dev_init(rdpi);
	if (rc < 0)
		goto err_out_free;

	return 0;

err_out_free:
	if (dmadev)
		rte_dma_pmd_release(name);

	return rc;
}

static int
cnxk_dmadev_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_DEV_NAME_MAX_LEN];

	memset(name, 0, sizeof(name));
	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	return rte_dma_pmd_release(name);
}

static const struct rte_pci_id cnxk_dma_pci_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_CNXK_DPI_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cnxk_dmadev = {
	.id_table  = cnxk_dma_pci_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe     = cnxk_dmadev_probe,
	.remove    = cnxk_dmadev_remove,
};

RTE_PMD_REGISTER_PCI(cnxk_dmadev_pci_driver, cnxk_dmadev);
RTE_PMD_REGISTER_PCI_TABLE(cnxk_dmadev_pci_driver, cnxk_dma_pci_map);
RTE_PMD_REGISTER_KMOD_DEP(cnxk_dmadev_pci_driver, "vfio-pci");
