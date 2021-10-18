/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <rte_bus_pci.h>
#include <rte_dmadev_pmd.h>
#include <rte_malloc.h>
#include <rte_prefetch.h>
#include <rte_errno.h>

#include "ioat_internal.h"

static struct rte_pci_driver ioat_pmd_drv;

RTE_LOG_REGISTER_DEFAULT(ioat_pmd_logtype, INFO);

#define DESC_SZ sizeof(struct ioat_dma_hw_desc)

#define IOAT_PMD_NAME dmadev_ioat
#define IOAT_PMD_NAME_STR RTE_STR(IOAT_PMD_NAME)

/* IOAT operations. */
enum rte_ioat_ops {
	ioat_op_copy = 0,	/* Standard DMA Operation */
	ioat_op_fill		/* Block Fill */
};

/* Configure a device. */
static int
ioat_dev_configure(struct rte_dma_dev *dev __rte_unused, const struct rte_dma_conf *dev_conf,
		uint32_t conf_sz)
{
	if (sizeof(struct rte_dma_conf) != conf_sz)
		return -EINVAL;

	if (dev_conf->nb_vchans != 1)
		return -EINVAL;

	return 0;
}

/* Setup a virtual channel for IOAT, only 1 vchan is supported. */
static int
ioat_vchan_setup(struct rte_dma_dev *dev, uint16_t vchan __rte_unused,
		const struct rte_dma_vchan_conf *qconf, uint32_t qconf_sz)
{
	struct ioat_dmadev *ioat = dev->fp_obj->dev_private;
	uint16_t max_desc = qconf->nb_desc;
	int i;

	if (sizeof(struct rte_dma_vchan_conf) != qconf_sz)
		return -EINVAL;

	ioat->qcfg = *qconf;

	if (!rte_is_power_of_2(max_desc)) {
		max_desc = rte_align32pow2(max_desc);
		IOAT_PMD_DEBUG("DMA dev %u using %u descriptors", dev->data->dev_id, max_desc);
		ioat->qcfg.nb_desc = max_desc;
	}

	/* In case we are reconfiguring a device, free any existing memory. */
	rte_free(ioat->desc_ring);

	ioat->desc_ring = rte_zmalloc(NULL, sizeof(*ioat->desc_ring) * max_desc, 0);
	if (ioat->desc_ring == NULL)
		return -ENOMEM;

	ioat->ring_addr = rte_mem_virt2iova(ioat->desc_ring);

	ioat->status_addr = rte_mem_virt2iova(ioat) + offsetof(struct ioat_dmadev, status);

	/* Ensure all counters are reset, if reconfiguring/restarting device. */
	ioat->next_read = 0;
	ioat->next_write = 0;
	ioat->last_write = 0;
	ioat->offset = 0;
	ioat->failure = 0;

	/* Reset Stats. */
	ioat->stats = (struct rte_dma_stats){0};

	/* Configure descriptor ring - each one points to next. */
	for (i = 0; i < ioat->qcfg.nb_desc; i++) {
		ioat->desc_ring[i].next = ioat->ring_addr +
				(((i + 1) % ioat->qcfg.nb_desc) * DESC_SZ);
	}

	return 0;
}

/* Recover IOAT device. */
static inline int
__ioat_recover(struct ioat_dmadev *ioat)
{
	uint32_t chanerr, retry = 0;
	uint16_t mask = ioat->qcfg.nb_desc - 1;

	/* Clear any channel errors. Reading and writing to chanerr does this. */
	chanerr = ioat->regs->chanerr;
	ioat->regs->chanerr = chanerr;

	/* Reset Channel. */
	ioat->regs->chancmd = IOAT_CHANCMD_RESET;

	/* Write new chain address to trigger state change. */
	ioat->regs->chainaddr = ioat->desc_ring[(ioat->next_read - 1) & mask].next;
	/* Ensure channel control and status addr are correct. */
	ioat->regs->chanctrl = IOAT_CHANCTRL_ANY_ERR_ABORT_EN |
			IOAT_CHANCTRL_ERR_COMPLETION_EN;
	ioat->regs->chancmp = ioat->status_addr;

	/* Allow HW time to move to the ARMED state. */
	do {
		rte_pause();
		retry++;
	} while (ioat->regs->chansts != IOAT_CHANSTS_ARMED && retry < 200);

	/* Exit as failure if device is still HALTED. */
	if (ioat->regs->chansts != IOAT_CHANSTS_ARMED)
		return -1;

	/* Store next write as offset as recover will move HW and SW ring out of sync. */
	ioat->offset = ioat->next_read;

	/* Prime status register with previous address. */
	ioat->status = ioat->desc_ring[(ioat->next_read - 2) & mask].next;

	return 0;
}

/* Start a configured device. */
static int
ioat_dev_start(struct rte_dma_dev *dev)
{
	struct ioat_dmadev *ioat = dev->fp_obj->dev_private;

	if (ioat->qcfg.nb_desc == 0 || ioat->desc_ring == NULL)
		return -EBUSY;

	/* Inform hardware of where the descriptor ring is. */
	ioat->regs->chainaddr = ioat->ring_addr;
	/* Inform hardware of where to write the status/completions. */
	ioat->regs->chancmp = ioat->status_addr;

	/* Prime the status register to be set to the last element. */
	ioat->status = ioat->ring_addr + ((ioat->qcfg.nb_desc - 1) * DESC_SZ);

	printf("IOAT.status: %s [0x%"PRIx64"]\n",
			chansts_readable[ioat->status & IOAT_CHANSTS_STATUS],
			ioat->status);

	if ((ioat->regs->chansts & IOAT_CHANSTS_STATUS) == IOAT_CHANSTS_HALTED) {
		IOAT_PMD_WARN("Device HALTED on start, attempting to recover\n");
		if (__ioat_recover(ioat) != 0) {
			IOAT_PMD_ERR("Device couldn't be recovered");
			return -1;
		}
	}

	return 0;
}

/* Stop a configured device. */
static int
ioat_dev_stop(struct rte_dma_dev *dev)
{
	struct ioat_dmadev *ioat = dev->fp_obj->dev_private;
	uint32_t retry = 0;

	ioat->regs->chancmd = IOAT_CHANCMD_SUSPEND;

	do {
		rte_pause();
		retry++;
	} while ((ioat->regs->chansts & IOAT_CHANSTS_STATUS) != IOAT_CHANSTS_SUSPENDED
			&& retry < 200);

	return ((ioat->regs->chansts & IOAT_CHANSTS_STATUS) == IOAT_CHANSTS_SUSPENDED) ? 0 : -1;
}

/* Get device information of a device. */
static int
ioat_dev_info_get(const struct rte_dma_dev *dev, struct rte_dma_info *info, uint32_t size)
{
	struct ioat_dmadev *ioat = dev->fp_obj->dev_private;
	if (size < sizeof(*info))
		return -EINVAL;
	info->dev_capa = RTE_DMA_CAPA_MEM_TO_MEM |
			RTE_DMA_CAPA_OPS_COPY |
			RTE_DMA_CAPA_OPS_FILL;
	if (ioat->version >= IOAT_VER_3_4)
		info->dev_capa |= RTE_DMA_CAPA_HANDLES_ERRORS;
	info->max_vchans = 1;
	info->min_desc = 32;
	info->max_desc = 4096;
	return 0;
}

/* Close a configured device. */
static int
ioat_dev_close(struct rte_dma_dev *dev)
{
	struct ioat_dmadev *ioat;

	if (!dev) {
		IOAT_PMD_ERR("Invalid device");
		return -EINVAL;
	}

	ioat = dev->fp_obj->dev_private;
	if (!ioat) {
		IOAT_PMD_ERR("Error getting dev_private");
		return -EINVAL;
	}

	rte_free(ioat->desc_ring);

	return 0;
}

/* Trigger hardware to begin performing enqueued operations. */
static inline void
__submit(struct ioat_dmadev *ioat)
{
	*ioat->doorbell = ioat->next_write - ioat->offset;

	ioat->stats.submitted += (uint16_t)(ioat->next_write - ioat->last_write);

	ioat->last_write = ioat->next_write;
}

/* External submit function wrapper. */
static int
ioat_submit(void *dev_private, uint16_t qid __rte_unused)
{
	struct ioat_dmadev *ioat = dev_private;

	__submit(ioat);

	return 0;
}

/* Write descriptor for enqueue. */
static inline int
__write_desc(void *dev_private, uint32_t op, uint64_t src, phys_addr_t dst,
		unsigned int length, uint64_t flags)
{
	struct ioat_dmadev *ioat = dev_private;
	uint16_t ret;
	const unsigned short mask = ioat->qcfg.nb_desc - 1;
	const unsigned short read = ioat->next_read;
	unsigned short write = ioat->next_write;
	const unsigned short space = mask + read - write;
	struct ioat_dma_hw_desc *desc;

	if (space == 0)
		return -ENOSPC;

	ioat->next_write = write + 1;
	write &= mask;

	desc = &ioat->desc_ring[write];
	desc->size = length;
	desc->u.control_raw = (uint32_t)((op << IOAT_CMD_OP_SHIFT) |
			(1 << IOAT_COMP_UPDATE_SHIFT));

	/* In IOAT the fence ensures that all operations including the current one
	 * are completed before moving on, DMAdev assumes that the fence ensures
	 * all operations before the current one are completed before starting
	 * the current one, so in IOAT we set the fence for the previous descriptor.
	 */
	if (flags & RTE_DMA_OP_FLAG_FENCE)
		ioat->desc_ring[(write - 1) & mask].u.control.fence = 1;

	desc->src_addr = src;
	desc->dest_addr = dst;

	rte_prefetch0(&ioat->desc_ring[ioat->next_write & mask]);

	ret = (uint16_t)(ioat->next_write - 1);

	if (flags & RTE_DMA_OP_FLAG_SUBMIT)
		__submit(ioat);

	return ret;
}

/* Enqueue a fill operation onto the ioat device. */
static int
ioat_enqueue_fill(void *dev_private, uint16_t qid __rte_unused, uint64_t pattern,
		rte_iova_t dst, unsigned int length, uint64_t flags)
{
	return __write_desc(dev_private, ioat_op_fill, pattern, dst, length, flags);
}

/* Enqueue a copy operation onto the ioat device. */
static int
ioat_enqueue_copy(void *dev_private, uint16_t qid __rte_unused, rte_iova_t src,
		rte_iova_t dst, unsigned int length, uint64_t flags)
{
	return __write_desc(dev_private, ioat_op_copy, src, dst, length, flags);
}

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
	fprintf(f, "  Key Stats { submitted: %"PRIu64", comp: %"PRIu64", failed: %"PRIu64" }\n",
			ioat->stats.submitted,
			ioat->stats.completed,
			ioat->stats.errors);

	return 0;
}

/* Public wrapper for dump. */
static int
ioat_dev_dump(const struct rte_dma_dev *dev, FILE *f)
{
	return __dev_dump(dev->fp_obj->dev_private, f);
}

/* Returns the index of the last completed operation. */
static inline uint16_t
__get_last_completed(const struct ioat_dmadev *ioat, int *state)
{
	/* Status register contains the address of the completed operation */
	uint64_t status = ioat->status;

	/* lower 3 bits indicate "transfer status" : active, idle, halted.
	 * We can ignore bit 0.
	 */
	*state = status & IOAT_CHANSTS_STATUS;

	/* If we are just after recovering from an error the address returned by
	 * status will be 0, in this case we return the offset - 1 as the last
	 * completed. If not return the status value minus the chainaddr which
	 * gives us an offset into the ring. Right shifting by 6 (divide by 64)
	 * gives the index of the completion from the HW point of view and adding
	 * the offset translates the ring index from HW to SW point of view.
	 */
	if ((status & ~IOAT_CHANSTS_STATUS) == 0)
		return ioat->offset - 1;

	return (status - ioat->ring_addr) >> 6;
}

/* Translates IOAT ChanERRs to DMA error codes. */
static inline enum rte_dma_status_code
__translate_status_ioat_to_dma(uint32_t chanerr)
{
	if (chanerr & IOAT_CHANERR_INVALID_SRC_ADDR_MASK)
		return RTE_DMA_STATUS_INVALID_SRC_ADDR;
	else if (chanerr & IOAT_CHANERR_INVALID_DST_ADDR_MASK)
		return RTE_DMA_STATUS_INVALID_DST_ADDR;
	else if (chanerr & IOAT_CHANERR_INVALID_LENGTH_MASK)
		return RTE_DMA_STATUS_INVALID_LENGTH;
	else if (chanerr & IOAT_CHANERR_DESCRIPTOR_READ_ERROR_MASK)
		return RTE_DMA_STATUS_DESCRIPTOR_READ_ERROR;
	else
		return RTE_DMA_STATUS_ERROR_UNKNOWN;
}

/* Returns details of operations that have been completed. */
static uint16_t
ioat_completed(void *dev_private, uint16_t qid __rte_unused, const uint16_t max_ops,
		uint16_t *last_idx, bool *has_error)
{
	struct ioat_dmadev *ioat = dev_private;

	const unsigned short mask = (ioat->qcfg.nb_desc - 1);
	const unsigned short read = ioat->next_read;
	unsigned short last_completed, count;
	int state, fails = 0;

	/* Do not do any work if there is an uncleared error. */
	if (ioat->failure != 0) {
		*has_error = true;
		*last_idx = ioat->next_read - 2;
		return 0;
	}

	last_completed = __get_last_completed(ioat, &state);
	count = (last_completed + 1 - read) & mask;

	/* Cap count at max_ops or set as last run in batch. */
	if (count > max_ops)
		count = max_ops;

	if (count == max_ops || state != IOAT_CHANSTS_HALTED) {
		ioat->next_read = read + count;
		*last_idx = ioat->next_read - 1;
	} else {
		*has_error = true;
		rte_errno = EIO;
		ioat->failure = ioat->regs->chanerr;
		ioat->next_read = read + count + 1;
		if (__ioat_recover(ioat) != 0) {
			IOAT_PMD_ERR("Device HALTED and could not be recovered\n");
			__dev_dump(dev_private, stdout);
			return 0;
		}
		__submit(ioat);
		fails++;
		*last_idx = ioat->next_read - 2;
	}

	ioat->stats.completed += count;
	ioat->stats.errors += fails;

	return count;
}

/* Returns detailed status information about operations that have been completed. */
static uint16_t
ioat_completed_status(void *dev_private, uint16_t qid __rte_unused,
		uint16_t max_ops, uint16_t *last_idx, enum rte_dma_status_code *status)
{
	struct ioat_dmadev *ioat = dev_private;

	const unsigned short mask = (ioat->qcfg.nb_desc - 1);
	const unsigned short read = ioat->next_read;
	unsigned short count, last_completed;
	uint64_t fails = 0;
	int state, i;

	last_completed = __get_last_completed(ioat, &state);
	count = (last_completed + 1 - read) & mask;

	for (i = 0; i < RTE_MIN(count + 1, max_ops); i++)
		status[i] = RTE_DMA_STATUS_SUCCESSFUL;

	/* Cap count at max_ops or set as last run in batch. */
	if (count > max_ops)
		count = max_ops;

	if (count == max_ops || state != IOAT_CHANSTS_HALTED)
		ioat->next_read = read + count;
	else {
		rte_errno = EIO;
		status[count] = __translate_status_ioat_to_dma(ioat->regs->chanerr);
		count++;
		ioat->next_read = read + count;
		if (__ioat_recover(ioat) != 0) {
			IOAT_PMD_ERR("Device HALTED and could not be recovered\n");
			__dev_dump(dev_private, stdout);
			return 0;
		}
		__submit(ioat);
		fails++;
	}

	if (ioat->failure > 0) {
		status[0] = __translate_status_ioat_to_dma(ioat->failure);
		count = RTE_MIN(count + 1, max_ops);
		ioat->failure = 0;
	}

	*last_idx = ioat->next_read - 1;

	ioat->stats.completed += count;
	ioat->stats.errors += fails;

	return count;
}

/* Get the remaining capacity of the ring. */
static uint16_t
ioat_burst_capacity(const void *dev_private, uint16_t vchan __rte_unused)
{
	const struct ioat_dmadev *ioat = dev_private;
	unsigned short size = ioat->qcfg.nb_desc - 1;
	unsigned short read = ioat->next_read;
	unsigned short write = ioat->next_write;
	unsigned short space = size - (write - read);

	return space;
}

/* Retrieve the generic stats of a DMA device. */
static int
ioat_stats_get(const struct rte_dma_dev *dev, uint16_t vchan __rte_unused,
		struct rte_dma_stats *rte_stats, uint32_t size)
{
	struct rte_dma_stats *stats = (&((struct ioat_dmadev *)dev->fp_obj->dev_private)->stats);

	if (size < sizeof(rte_stats))
		return -EINVAL;
	if (rte_stats == NULL)
		return -EINVAL;

	*rte_stats = *stats;
	return 0;
}

/* Reset the generic stat counters for the DMA device. */
static int
ioat_stats_reset(struct rte_dma_dev *dev, uint16_t vchan __rte_unused)
{
	struct ioat_dmadev *ioat = dev->fp_obj->dev_private;

	ioat->stats = (struct rte_dma_stats){0};
	return 0;
}

/* Check if the IOAT device is idle. */
static int
ioat_vchan_status(const struct rte_dma_dev *dev, uint16_t vchan __rte_unused,
		enum rte_dma_vchan_status *status)
{
	int state = 0;
	const struct ioat_dmadev *ioat = dev->fp_obj->dev_private;
	const uint16_t mask = ioat->qcfg.nb_desc - 1;
	const uint16_t last = __get_last_completed(ioat, &state);

	if (state == IOAT_CHANSTS_HALTED || state == IOAT_CHANSTS_SUSPENDED)
		*status = RTE_DMA_VCHAN_HALTED_ERROR;
	else if (last == ((ioat->next_write - 1) & mask))
		*status = RTE_DMA_VCHAN_IDLE;
	else
		*status = RTE_DMA_VCHAN_ACTIVE;

	return 0;
}

/* Create a DMA device. */
static int
ioat_dmadev_create(const char *name, struct rte_pci_device *dev)
{
	static const struct rte_dma_dev_ops ioat_dmadev_ops = {
		.dev_close = ioat_dev_close,
		.dev_configure = ioat_dev_configure,
		.dev_dump = ioat_dev_dump,
		.dev_info_get = ioat_dev_info_get,
		.dev_start = ioat_dev_start,
		.dev_stop = ioat_dev_stop,
		.stats_get = ioat_stats_get,
		.stats_reset = ioat_stats_reset,
		.vchan_status = ioat_vchan_status,
		.vchan_setup = ioat_vchan_setup,
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

	dmadev->fp_obj->burst_capacity = ioat_burst_capacity;
	dmadev->fp_obj->completed = ioat_completed;
	dmadev->fp_obj->completed_status = ioat_completed_status;
	dmadev->fp_obj->copy = ioat_enqueue_copy;
	dmadev->fp_obj->fill = ioat_enqueue_fill;
	dmadev->fp_obj->submit = ioat_submit;

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
