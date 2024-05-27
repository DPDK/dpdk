/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <string.h>

#include <bus_pci_driver.h>
#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_dmadev.h>
#include <rte_dmadev_pmd.h>
#include <rte_pci.h>

#include "odm.h"

#define PCI_VENDOR_ID_CAVIUM	 0x177D
#define PCI_DEVID_ODYSSEY_ODM_VF 0xA08C
#define PCI_DRIVER_NAME		 dma_odm

static int
odm_dmadev_info_get(const struct rte_dma_dev *dev, struct rte_dma_info *dev_info, uint32_t size)
{
	struct odm_dev *odm = NULL;

	RTE_SET_USED(size);

	odm = dev->fp_obj->dev_private;

	dev_info->max_vchans = odm->max_qs;
	dev_info->nb_vchans = odm->num_qs;
	dev_info->dev_capa =
		(RTE_DMA_CAPA_MEM_TO_MEM | RTE_DMA_CAPA_OPS_COPY | RTE_DMA_CAPA_OPS_COPY_SG);
	dev_info->max_desc = ODM_IRING_MAX_ENTRY;
	dev_info->min_desc = 1;
	dev_info->max_sges = ODM_MAX_POINTER;

	return 0;
}

static int
odm_dmadev_configure(struct rte_dma_dev *dev, const struct rte_dma_conf *conf, uint32_t conf_sz)
{
	struct odm_dev *odm = NULL;

	RTE_SET_USED(conf_sz);

	odm = dev->fp_obj->dev_private;
	odm->num_qs = conf->nb_vchans;

	return 0;
}

static int
odm_dmadev_vchan_setup(struct rte_dma_dev *dev, uint16_t vchan,
		       const struct rte_dma_vchan_conf *conf, uint32_t conf_sz)
{
	struct odm_dev *odm = dev->fp_obj->dev_private;

	RTE_SET_USED(conf_sz);
	return odm_vchan_setup(odm, vchan, conf->nb_desc);
}

static int
odm_dmadev_start(struct rte_dma_dev *dev)
{
	struct odm_dev *odm = dev->fp_obj->dev_private;

	return odm_enable(odm);
}

static int
odm_dmadev_stop(struct rte_dma_dev *dev)
{
	struct odm_dev *odm = dev->fp_obj->dev_private;

	return odm_disable(odm);
}

static int
odm_dmadev_close(struct rte_dma_dev *dev)
{
	struct odm_dev *odm = dev->fp_obj->dev_private;

	odm_disable(odm);
	odm_dev_fini(odm);

	return 0;
}

static const struct rte_dma_dev_ops odm_dmadev_ops = {
	.dev_close = odm_dmadev_close,
	.dev_configure = odm_dmadev_configure,
	.dev_info_get = odm_dmadev_info_get,
	.dev_start = odm_dmadev_start,
	.dev_stop = odm_dmadev_stop,
	.stats_get = NULL,
	.stats_reset = NULL,
	.vchan_setup = odm_dmadev_vchan_setup,
};

static int
odm_dmadev_probe(struct rte_pci_driver *pci_drv __rte_unused, struct rte_pci_device *pci_dev)
{
	char name[RTE_DEV_NAME_MAX_LEN];
	struct odm_dev *odm = NULL;
	struct rte_dma_dev *dmadev;
	int rc;

	if (!pci_dev->mem_resource[0].addr)
		return -ENODEV;

	memset(name, 0, sizeof(name));
	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	dmadev = rte_dma_pmd_allocate(name, pci_dev->device.numa_node, sizeof(*odm));
	if (dmadev == NULL) {
		odm_err("DMA device allocation failed for %s", name);
		return -ENOMEM;
	}

	odm_info("DMA device %s probed", name);
	odm = dmadev->data->dev_private;

	dmadev->device = &pci_dev->device;
	dmadev->fp_obj->dev_private = odm;
	dmadev->dev_ops = &odm_dmadev_ops;

	odm->pci_dev = pci_dev;

	rc = odm_dev_init(odm);
	if (rc < 0)
		goto dma_pmd_release;

	return 0;

dma_pmd_release:
	rte_dma_pmd_release(name);

	return rc;
}

static int
odm_dmadev_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_DEV_NAME_MAX_LEN];

	memset(name, 0, sizeof(name));
	rte_pci_device_name(&pci_dev->addr, name, sizeof(name));

	return rte_dma_pmd_release(name);
}

static const struct rte_pci_id odm_dma_pci_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_ODYSSEY_ODM_VF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver odm_dmadev = {
	.id_table = odm_dma_pci_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = odm_dmadev_probe,
	.remove = odm_dmadev_remove,
};

RTE_PMD_REGISTER_PCI(PCI_DRIVER_NAME, odm_dmadev);
RTE_PMD_REGISTER_PCI_TABLE(PCI_DRIVER_NAME, odm_dma_pci_map);
RTE_PMD_REGISTER_KMOD_DEP(PCI_DRIVER_NAME, "vfio-pci");
RTE_LOG_REGISTER_DEFAULT(odm_logtype, NOTICE);
