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
ae4dma_dev_configure(struct rte_dma_dev *dev __rte_unused, const struct rte_dma_conf *dev_conf,
		uint32_t conf_sz)
{
	if (conf_sz < sizeof(struct rte_dma_conf))
		return -EINVAL;

	if (dev_conf->nb_vchans != 1)
		return -EINVAL;

	return 0;
}

/* Setup a virtual channel for AE4DMA, only 1 vchan is supported per dmadev. */
static int
ae4dma_vchan_setup(struct rte_dma_dev *dev, uint16_t vchan __rte_unused,
		const struct rte_dma_vchan_conf *qconf, uint32_t qconf_sz)
{
	struct ae4dma_dmadev *ae4dma = dev->fp_obj->dev_private;
	struct ae4dma_cmd_queue *cmd_q = &ae4dma->cmd_q;
	uint16_t max_desc = qconf->nb_desc;

	if (qconf_sz < sizeof(struct rte_dma_vchan_conf))
		return -EINVAL;

	/*
	 * The framework already clamps nb_desc to the advertised [min_desc,
	 * max_desc] range. The HW ring slot is computed by masking a
	 * free-running 16-bit index, so the depth must be a power of two:
	 * round up here when the application asks for a non-power-of-two size.
	 */
	if (!rte_is_power_of_2(max_desc))
		max_desc = rte_align32pow2(max_desc);

	cmd_q->qcfg = *qconf;
	cmd_q->qcfg.nb_desc = max_desc;

	/*
	 * Reset ring indices and stats when (re)configuring the vchan.
	 * hw_base is (re)sampled from the HW consumer index in dev_start.
	 */
	cmd_q->next_write = 0;
	cmd_q->next_read = 0;
	cmd_q->last_write = 0;
	cmd_q->hw_base = 0;
	memset(&cmd_q->stats, 0, sizeof(cmd_q->stats));
	return 0;
}

static int
ae4dma_dev_start(struct rte_dma_dev *dev)
{
	struct ae4dma_dmadev *ae4dma = dev->fp_obj->dev_private;
	struct ae4dma_cmd_queue *cmd_q = &ae4dma->cmd_q;
	uint16_t nb = cmd_q->qcfg.nb_desc;
	uint16_t hw_pos;

	if (nb == 0)
		return -EBUSY;

	/*
	 * Program ring depth and (re)enable the HW queue. On HW that resets
	 * the queue indices on enable, this must happen before we sample
	 * read_idx below.
	 */
	AE4DMA_WRITE_REG(&cmd_q->hwq_regs->max_idx, nb);
	AE4DMA_WRITE_REG(&cmd_q->hwq_regs->control_reg.control_raw, AE4DMA_CMD_QUEUE_ENABLE);

	/*
	 * The dmadev ring index is a free-running 16-bit counter that restarts
	 * from 0 on every (re)start. read_idx is HW-owned (read-only), so we
	 * cannot force it to 0; instead anchor index 0 to wherever the HW
	 * consumer currently sits (hw_base) and derive the ring slot as
	 * (hw_base + idx) & (nb - 1). write_idx is set equal to read_idx so the
	 * queue starts empty without touching the read-only read_idx register.
	 */
	hw_pos = (uint16_t)(AE4DMA_READ_REG(&cmd_q->hwq_regs->read_idx) & (nb - 1));
	cmd_q->hw_base = hw_pos;
	cmd_q->next_write = 0;
	cmd_q->next_read = 0;
	cmd_q->last_write = 0;
	AE4DMA_WRITE_REG(&cmd_q->hwq_regs->write_idx, hw_pos);
	return 0;
}

static int
ae4dma_dev_stop(struct rte_dma_dev *dev)
{
	struct ae4dma_dmadev *ae4dma = dev->fp_obj->dev_private;
	struct ae4dma_cmd_queue *cmd_q = &ae4dma->cmd_q;

	if (cmd_q->hwq_regs != NULL)
		AE4DMA_WRITE_REG(&cmd_q->hwq_regs->control_reg.control_raw,
			AE4DMA_CMD_QUEUE_DISABLE);
	return 0;
}

static int
ae4dma_dev_info_get(const struct rte_dma_dev *dev __rte_unused,
		struct rte_dma_info *info, uint32_t size __rte_unused)
{
	info->dev_capa = RTE_DMA_CAPA_MEM_TO_MEM | RTE_DMA_CAPA_OPS_COPY;
	info->max_vchans = 1;
	info->min_desc = 2;
	info->max_desc = AE4DMA_DESCRIPTORS_PER_CMDQ;
	info->nb_vchans = 1;
	return 0;
}

static int
ae4dma_dev_close(struct rte_dma_dev *dev)
{
	struct ae4dma_dmadev *ae4dma = dev->fp_obj->dev_private;
	struct ae4dma_cmd_queue *cmd_q = &ae4dma->cmd_q;

	if (cmd_q->hwq_regs != NULL)
		AE4DMA_WRITE_REG(&cmd_q->hwq_regs->control_reg.control_raw,
			AE4DMA_CMD_QUEUE_DISABLE);

	rte_memzone_free(cmd_q->mz);
	cmd_q->mz = NULL;
	cmd_q->qbase_desc = NULL;
	cmd_q->qbase_addr = NULL;
	cmd_q->qbase_phys_addr = 0;
	return 0;
}

static int
ae4dma_dev_dump(const struct rte_dma_dev *dev, FILE *f)
{
	struct ae4dma_dmadev *ae4dma = dev->fp_obj->dev_private;
	struct ae4dma_cmd_queue *cmd_q = &ae4dma->cmd_q;
	void *mmio = ae4dma->io_regs;

	fprintf(f, "cmd_q->id              = %" PRIx64 "\n", cmd_q->id);
	fprintf(f, "cmd_q->qidx            = %" PRIx64 "\n", cmd_q->qidx);
	fprintf(f, "cmd_q->qsize           = %" PRIx64 "\n", cmd_q->qsize);
	fprintf(f, "mmio_base_addr         = %p\n", mmio);
	fprintf(f, "queues per ae4dma engine     = %d\n",
		AE4DMA_READ_REG_OFFSET(mmio, AE4DMA_COMMON_CONFIG_OFFSET));
	fprintf(f, "== Private Data ==\n");
	fprintf(f, "  Config: { ring_size: %u }\n", cmd_q->qcfg.nb_desc);
	fprintf(f, "  Ring virt: %p\tphys: %#" PRIx64 "\n", (void *)cmd_q->qbase_desc,
		(uint64_t)cmd_q->qbase_phys_addr);
	fprintf(f, "  Next write: %u\n", cmd_q->next_write);
	fprintf(f, "  Next read: %u\n", cmd_q->next_read);
	fprintf(f, "  current queue depth: %u\n",
		(uint16_t)(cmd_q->next_write - cmd_q->next_read));
	fprintf(f, "  }\n");
	fprintf(f, "  Key Stats { submitted: %" PRIu64 ", comp: %" PRIu64 ", failed: %" PRIu64 " }\n",
		cmd_q->stats.submitted, cmd_q->stats.completed, cmd_q->stats.errors);
	return 0;
}

static int
ae4dma_stats_get(const struct rte_dma_dev *dev, uint16_t vchan __rte_unused,
		struct rte_dma_stats *rte_stats, uint32_t size __rte_unused)
{
	const struct ae4dma_dmadev *ae4dma = dev->fp_obj->dev_private;
	const struct ae4dma_cmd_queue *cmd_q = &ae4dma->cmd_q;

	*rte_stats = cmd_q->stats;
	return 0;
}

static int
ae4dma_stats_reset(struct rte_dma_dev *dev, uint16_t vchan __rte_unused)
{
	struct ae4dma_dmadev *ae4dma = dev->fp_obj->dev_private;
	struct ae4dma_cmd_queue *cmd_q = &ae4dma->cmd_q;

	memset(&cmd_q->stats, 0, sizeof(cmd_q->stats));
	return 0;
}

/*
 * Report channel state to the dmadev framework.
 *
 *   RTE_DMA_VCHAN_HALTED_ERROR - HW queue is disabled (never started, or
 *                                stopped via dev_stop()).
 *   RTE_DMA_VCHAN_IDLE         - HW has caught up: read_idx == write_idx,
 *                                no descriptors in flight.
 *   RTE_DMA_VCHAN_ACTIVE       - HW still has descriptors to process.
 */
static int
ae4dma_vchan_status(const struct rte_dma_dev *dev, uint16_t vchan __rte_unused,
		enum rte_dma_vchan_status *status)
{
	const struct ae4dma_dmadev *ae4dma = dev->fp_obj->dev_private;
	const struct ae4dma_cmd_queue *cmd_q = &ae4dma->cmd_q;
	uint32_t ctrl, hw_read, hw_write;

	if (cmd_q->hwq_regs == NULL) {
		*status = RTE_DMA_VCHAN_HALTED_ERROR;
		return 0;
	}

	ctrl = AE4DMA_READ_REG(&cmd_q->hwq_regs->control_reg.control_raw);
	if ((ctrl & AE4DMA_CMD_QUEUE_ENABLE) == 0) {
		*status = RTE_DMA_VCHAN_HALTED_ERROR;
		return 0;
	}

	hw_read = AE4DMA_READ_REG(&cmd_q->hwq_regs->read_idx);
	hw_write = AE4DMA_READ_REG(&cmd_q->hwq_regs->write_idx);

	*status = (hw_read == hw_write) ? RTE_DMA_VCHAN_IDLE : RTE_DMA_VCHAN_ACTIVE;
	return 0;
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
	static const struct rte_dma_dev_ops ae4dma_dmadev_ops = {
		.dev_close = ae4dma_dev_close,
		.dev_configure = ae4dma_dev_configure,
		.dev_dump = ae4dma_dev_dump,
		.dev_info_get = ae4dma_dev_info_get,
		.dev_start = ae4dma_dev_start,
		.dev_stop = ae4dma_dev_stop,
		.stats_get = ae4dma_stats_get,
		.stats_reset = ae4dma_stats_reset,
		.vchan_status = ae4dma_vchan_status,
		.vchan_setup = ae4dma_vchan_setup,
	};

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
	dmadev->dev_ops = &ae4dma_dmadev_ops;

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
