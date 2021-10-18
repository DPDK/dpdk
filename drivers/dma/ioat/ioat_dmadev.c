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

/* Dump DMA device info. */
static int
__dev_dump(void *dev_private, FILE *f)
{
	struct ioat_dmadev *ioat = dev_private;
	uint64_t chansts_masked = ioat->regs->chansts & IOAT_CHANSTS_STATUS;
	uint32_t chanerr = ioat->regs->chanerr;
	uint64_t mask = (ioat->qcfg.nb_desc - 1);
	char ver = ioat->version;
	fprintf(f, "========= IOAT =========\n");
	fprintf(f, "  IOAT version: %d.%d\n", ver >> 4, ver & 0xF);
	fprintf(f, "  Channel status: %s [0x%"PRIx64"]\n",
			chansts_readable[chansts_masked], chansts_masked);
	fprintf(f, "  ChainADDR: 0x%"PRIu64"\n", ioat->regs->chainaddr);
	if (chanerr == 0) {
		fprintf(f, "  No Channel Errors\n");
	} else {
		fprintf(f, "  ChanERR: 0x%"PRIu32"\n", chanerr);
		if (chanerr & IOAT_CHANERR_INVALID_SRC_ADDR_MASK)
			fprintf(f, "    Invalid Source Address\n");
		if (chanerr & IOAT_CHANERR_INVALID_DST_ADDR_MASK)
			fprintf(f, "    Invalid Destination Address\n");
		if (chanerr & IOAT_CHANERR_INVALID_LENGTH_MASK)
			fprintf(f, "    Invalid Descriptor Length\n");
		if (chanerr & IOAT_CHANERR_DESCRIPTOR_READ_ERROR_MASK)
			fprintf(f, "    Descriptor Read Error\n");
		if ((chanerr & ~(IOAT_CHANERR_INVALID_SRC_ADDR_MASK |
				IOAT_CHANERR_INVALID_DST_ADDR_MASK |
				IOAT_CHANERR_INVALID_LENGTH_MASK |
				IOAT_CHANERR_DESCRIPTOR_READ_ERROR_MASK)) != 0)
			fprintf(f, "    Unknown Error(s)\n");
	}
	fprintf(f, "== Private Data ==\n");
	fprintf(f, "  Config: { ring_size: %u }\n", ioat->qcfg.nb_desc);
	fprintf(f, "  Status: 0x%"PRIx64"\n", ioat->status);
	fprintf(f, "  Status IOVA: 0x%"PRIx64"\n", ioat->status_addr);
	fprintf(f, "  Status ADDR: %p\n", &ioat->status);
	fprintf(f, "  Ring IOVA: 0x%"PRIx64"\n", ioat->ring_addr);
	fprintf(f, "  Ring ADDR: 0x%"PRIx64"\n", ioat->desc_ring[0].next-64);
	fprintf(f, "  Next write: %"PRIu16"\n", ioat->next_write);
	fprintf(f, "  Next read: %"PRIu16"\n", ioat->next_read);
	struct ioat_dma_hw_desc *desc_ring = &ioat->desc_ring[(ioat->next_write - 1) & mask];
	fprintf(f, "  Last Descriptor Written {\n");
	fprintf(f, "    Size: %"PRIu32"\n", desc_ring->size);
	fprintf(f, "    Control: 0x%"PRIx32"\n", desc_ring->u.control_raw);
	fprintf(f, "    Src: 0x%"PRIx64"\n", desc_ring->src_addr);
	fprintf(f, "    Dest: 0x%"PRIx64"\n", desc_ring->dest_addr);
	fprintf(f, "    Next: 0x%"PRIx64"\n", desc_ring->next);
	fprintf(f, "  }\n");
	fprintf(f, "  Next Descriptor {\n");
	fprintf(f, "    Size: %"PRIu32"\n", ioat->desc_ring[ioat->next_read & mask].size);
	fprintf(f, "    Src: 0x%"PRIx64"\n", ioat->desc_ring[ioat->next_read & mask].src_addr);
	fprintf(f, "    Dest: 0x%"PRIx64"\n", ioat->desc_ring[ioat->next_read & mask].dest_addr);
	fprintf(f, "    Next: 0x%"PRIx64"\n", ioat->desc_ring[ioat->next_read & mask].next);
	fprintf(f, "  }\n");

	return 0;
}

/* Public wrapper for dump. */
static int
ioat_dev_dump(const struct rte_dma_dev *dev, FILE *f)
{
	return __dev_dump(dev->fp_obj->dev_private, f);
}

/* Create a DMA device. */
static int
ioat_dmadev_create(const char *name, struct rte_pci_device *dev)
{
	static const struct rte_dma_dev_ops ioat_dmadev_ops = {
		.dev_dump = ioat_dev_dump,
	};

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
