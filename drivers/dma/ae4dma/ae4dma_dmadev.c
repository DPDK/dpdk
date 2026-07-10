/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Advanced Micro Devices, Inc. All rights reserved.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <bus_pci_driver.h>
#include <rte_dmadev_pmd.h>
#include <rte_malloc.h>

#include "ae4dma_internal.h"

/*
 * One dmadev per AE4DMA hardware channel; each dmadev has exactly one virtual channel.
 * The HW's per-queue register block must be densely packed right after the engine-common
 * config register at BAR0+0; the build-time check below catches an accidental layout change.
 */
static_assert(sizeof(struct ae4dma_hwq_regs) == 32,
	"ae4dma_hwq_regs stride changed; per-queue offset math will break");

RTE_LOG_REGISTER_DEFAULT(ae4dma_pmd_logtype, INFO);

#define AE4DMA_PMD_NAME dmadev_ae4dma

static const struct rte_memzone *
ae4dma_queue_dma_zone_reserve(const char *queue_name, uint32_t queue_size, int socket_id)
{
	return rte_memzone_reserve_aligned(queue_name, queue_size, socket_id,
		RTE_MEMZONE_IOVA_CONTIG, queue_size);
}

static int
ae4dma_add_queue(struct ae4dma_dmadev *dev, struct rte_pci_device *pci, uint8_t qn,
		const char *pci_name)
{
	const struct rte_memzone *q_mz;
	struct ae4dma_cmd_queue *cmd_q;
	uint32_t dma_addr_lo, dma_addr_hi;

	dev->io_regs = pci->mem_resource[AE4DMA_PCIE_BAR].addr;

	cmd_q = &dev->cmd_q;
	cmd_q->id = qn;
	cmd_q->qidx = 0;
	cmd_q->qsize = AE4DMA_QUEUE_SIZE(AE4DMA_QUEUE_DESC_SIZE);
	cmd_q->hwq_regs = (volatile struct ae4dma_hwq_regs *)dev->io_regs + (qn + 1);

	/*
	 * Memzone name must be globally unique. Embed PCI BDF so multiple
	 * PCI functions probed concurrently don't collide.
	 */
	snprintf(cmd_q->memz_name, sizeof(cmd_q->memz_name), "ae4dma_%s_q%u", pci_name,
		(unsigned int)qn);

	q_mz = ae4dma_queue_dma_zone_reserve(cmd_q->memz_name, cmd_q->qsize, rte_socket_id());
	if (q_mz == NULL) {
		AE4DMA_PMD_ERR("memzone reserve failed for %s", cmd_q->memz_name);
		return -ENOMEM;
	}

	cmd_q->mz = q_mz;
	cmd_q->qbase_addr = q_mz->addr;
	cmd_q->qbase_desc = q_mz->addr;
	cmd_q->qbase_phys_addr = q_mz->iova;

	AE4DMA_WRITE_REG(&cmd_q->hwq_regs->max_idx, AE4DMA_DESCRIPTORS_PER_CMDQ);
	AE4DMA_WRITE_REG(&cmd_q->hwq_regs->control_reg.control_raw, AE4DMA_CMD_QUEUE_ENABLE);
	AE4DMA_WRITE_REG(&cmd_q->hwq_regs->intr_status_reg.intr_status_raw,
		AE4DMA_DISABLE_INTR);
	cmd_q->next_write = AE4DMA_READ_REG(&cmd_q->hwq_regs->write_idx);
	cmd_q->next_read = AE4DMA_READ_REG(&cmd_q->hwq_regs->read_idx);

	dma_addr_lo = lower_32_bits(cmd_q->qbase_phys_addr);
	AE4DMA_WRITE_REG(&cmd_q->hwq_regs->qbase_lo, dma_addr_lo);
	dma_addr_hi = upper_32_bits(cmd_q->qbase_phys_addr);
	AE4DMA_WRITE_REG(&cmd_q->hwq_regs->qbase_hi, dma_addr_hi);

	return 0;
}

static void
ae4dma_channel_dev_name(char *out, size_t outlen, const char *pci_name, unsigned int ch)
{
	snprintf(out, outlen, "%s-ch%u", pci_name, ch);
}

static int
ae4dma_dmadev_create(const char *name, struct rte_pci_device *dev, uint8_t qn)
{
	char hwq_dev_name[RTE_DEV_NAME_MAX_LEN] = {0};
	struct ae4dma_dmadev *ae4dma;
	struct rte_dma_dev *dmadev;

	ae4dma_channel_dev_name(hwq_dev_name, sizeof(hwq_dev_name), name, qn);

	dmadev = rte_dma_pmd_allocate(hwq_dev_name, dev->device.numa_node,
		sizeof(struct ae4dma_dmadev));
	if (dmadev == NULL) {
		AE4DMA_PMD_ERR("Unable to allocate dma device");
		return -ENOMEM;
	}
	dmadev->device = &dev->device;
	dmadev->fp_obj->dev_private = dmadev->data->dev_private;

	ae4dma = dmadev->data->dev_private;

	if (ae4dma_add_queue(ae4dma, dev, qn, name) != 0)
		goto init_error;
	return 0;

init_error:
	AE4DMA_PMD_ERR("Failed to create dmadev %s", hwq_dev_name);
	rte_dma_pmd_release(hwq_dev_name);
	return -ENOMEM;
}

static int
ae4dma_dmadev_probe(struct rte_pci_driver *drv __rte_unused, struct rte_pci_device *dev)
{
	char chname[RTE_DEV_NAME_MAX_LEN];
	uint32_t q_per_eng;
	void *mmio_base;
	char name[32];
	int ret = 0;
	uint8_t i;

	rte_pci_device_name(&dev->addr, name, sizeof(name));
	AE4DMA_PMD_INFO("Init %s on NUMA node %d", name, dev->device.numa_node);

	mmio_base = dev->mem_resource[AE4DMA_PCIE_BAR].addr;
	if (mmio_base == NULL) {
		AE4DMA_PMD_ERR("%s: BAR%d not mapped", name, AE4DMA_PCIE_BAR);
		return -ENODEV;
	}

	/* Program the per-engine HW queue count once. */
	AE4DMA_WRITE_REG_OFFSET(mmio_base, AE4DMA_COMMON_CONFIG_OFFSET, AE4DMA_MAX_HW_QUEUES);
	q_per_eng = AE4DMA_READ_REG_OFFSET(mmio_base, AE4DMA_COMMON_CONFIG_OFFSET);
	AE4DMA_PMD_INFO("%s: AE4DMA queues per engine = %u", name, q_per_eng);

	for (i = 0; i < AE4DMA_MAX_HW_QUEUES; i++) {
		ret = ae4dma_dmadev_create(name, dev, i);
		if (ret != 0) {
			AE4DMA_PMD_ERR("%s create dmadev %u failed!", name, i);
			while (i > 0) {
				i--;
				ae4dma_channel_dev_name(chname, sizeof(chname), name, i);
				rte_dma_pmd_release(chname);
			}
			break;
		}
	}
	return ret;
}

static int
ae4dma_dmadev_remove(struct rte_pci_device *dev)
{
	char chname[RTE_DEV_NAME_MAX_LEN];
	unsigned int i;
	char name[32];

	rte_pci_device_name(&dev->addr, name, sizeof(name));

	AE4DMA_PMD_INFO("Closing %s on NUMA node %d", name, dev->device.numa_node);

	for (i = 0; i < AE4DMA_MAX_HW_QUEUES; i++) {
		ae4dma_channel_dev_name(chname, sizeof(chname), name, i);
		rte_dma_pmd_release(chname);
	}
	return 0;
}

static const struct rte_pci_id pci_id_ae4dma_map[] = {
	{ RTE_PCI_DEVICE(AMD_VENDOR_ID, AE4DMA_DEVICE_ID) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct rte_pci_driver ae4dma_pmd_drv = {
	.id_table = pci_id_ae4dma_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = ae4dma_dmadev_probe,
	.remove = ae4dma_dmadev_remove,
};

RTE_PMD_REGISTER_PCI(AE4DMA_PMD_NAME, ae4dma_pmd_drv);
RTE_PMD_REGISTER_PCI_TABLE(AE4DMA_PMD_NAME, pci_id_ae4dma_map);
RTE_PMD_REGISTER_KMOD_DEP(AE4DMA_PMD_NAME, "* igb_uio | uio_pci_generic | vfio-pci");
