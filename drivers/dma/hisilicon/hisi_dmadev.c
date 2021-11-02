/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 HiSilicon Limited
 */

#include <inttypes.h>
#include <string.h>

#include <rte_bus_pci.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_pci.h>
#include <rte_dmadev_pmd.h>

#include "hisi_dmadev.h"

RTE_LOG_REGISTER_DEFAULT(hisi_dma_logtype, INFO);
#define HISI_DMA_LOG(level, fmt, args...) \
		rte_log(RTE_LOG_ ## level, hisi_dma_logtype, \
		"%s(): " fmt "\n", __func__, ##args)
#define HISI_DMA_LOG_RAW(hw, level, fmt, args...) \
		rte_log(RTE_LOG_ ## level, hisi_dma_logtype, \
		"%s %s(): " fmt "\n", (hw)->data->dev_name, \
		__func__, ##args)
#define HISI_DMA_DEBUG(hw, fmt, args...) \
		HISI_DMA_LOG_RAW(hw, DEBUG, fmt, ## args)
#define HISI_DMA_INFO(hw, fmt, args...) \
		HISI_DMA_LOG_RAW(hw, INFO, fmt, ## args)
#define HISI_DMA_WARN(hw, fmt, args...) \
		HISI_DMA_LOG_RAW(hw, WARNING, fmt, ## args)
#define HISI_DMA_ERR(hw, fmt, args...) \
		HISI_DMA_LOG_RAW(hw, ERR, fmt, ## args)

static uint8_t
hisi_dma_reg_layout(uint8_t revision)
{
	if (revision == HISI_DMA_REVISION_HIP08B)
		return HISI_DMA_REG_LAYOUT_HIP08;
	else
		return HISI_DMA_REG_LAYOUT_INVALID;
}

static void
hisi_dma_gen_pci_device_name(const struct rte_pci_device *pci_dev,
			     char *name, size_t size)
{
	memset(name, 0, size);
	(void)snprintf(name, size, "%x:%x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);
}

static int
hisi_dma_check_revision(struct rte_pci_device *pci_dev, const char *name,
			uint8_t *out_revision)
{
	uint8_t revision;
	int ret;

	ret = rte_pci_read_config(pci_dev, &revision, 1,
				  HISI_DMA_PCI_REVISION_ID_REG);
	if (ret != 1) {
		HISI_DMA_LOG(ERR, "%s read PCI revision failed!", name);
		return -EINVAL;
	}
	if (hisi_dma_reg_layout(revision) == HISI_DMA_REG_LAYOUT_INVALID) {
		HISI_DMA_LOG(ERR, "%s revision: 0x%x not supported!",
			     name, revision);
		return -EINVAL;
	}

	*out_revision = revision;
	return 0;
}

static int
hisi_dma_probe(struct rte_pci_driver *pci_drv __rte_unused,
	       struct rte_pci_device *pci_dev)
{
	char name[RTE_DEV_NAME_MAX_LEN] = { 0 };
	uint8_t revision;
	int ret;

	hisi_dma_gen_pci_device_name(pci_dev, name, sizeof(name));

	if (pci_dev->mem_resource[2].addr == NULL) {
		HISI_DMA_LOG(ERR, "%s BAR2 is NULL!\n", name);
		return -ENODEV;
	}

	ret = hisi_dma_check_revision(pci_dev, name, &revision);
	if (ret)
		return ret;
	HISI_DMA_LOG(DEBUG, "%s read PCI revision: 0x%x", name, revision);

	return ret;
}

static int
hisi_dma_remove(struct rte_pci_device *pci_dev)
{
	RTE_SET_USED(pci_dev);
	return 0;
}

static const struct rte_pci_id pci_id_hisi_dma_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HISI_DMA_DEVICE_ID) },
	{ .vendor_id = 0, }, /* sentinel */
};

static struct rte_pci_driver hisi_dma_pmd_drv = {
	.id_table  = pci_id_hisi_dma_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe     = hisi_dma_probe,
	.remove    = hisi_dma_remove,
};

RTE_PMD_REGISTER_PCI(dma_hisilicon, hisi_dma_pmd_drv);
RTE_PMD_REGISTER_PCI_TABLE(dma_hisilicon, pci_id_hisi_dma_map);
RTE_PMD_REGISTER_KMOD_DEP(dma_hisilicon, "vfio-pci");
