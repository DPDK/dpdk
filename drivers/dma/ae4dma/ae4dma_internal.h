/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef AE4DMA_INTERNAL_H
#define AE4DMA_INTERNAL_H

#include <stdint.h>

#include <rte_byteorder.h>
#include <rte_dmadev.h>
#include <rte_io.h>
#include <rte_memzone.h>

#include "ae4dma_hw_defs.h"

/* Return bits 32-63 of a 64-bit number. */
#define upper_32_bits(n) ((uint32_t)(((n) >> 16) >> 16))

/* Return bits 0-31 of a 64-bit number. */
#define lower_32_bits(n) ((uint32_t)((n) & 0xffffffff))

/*
 * Hardware ring depth (slots per queue); fixed at 32 per the AE4DMA * spec and
 * must be a power of two.
 * The engine reserves one slot to tell a full ring from an empty one
 * (full when (write + 1) % max == read),
 * so at most nb_desc - 1 (31) descriptors can be outstanding.
 */
#define AE4DMA_DESCRIPTORS_PER_CMDQ	32
#define AE4DMA_QUEUE_DESC_SIZE		sizeof(struct ae4dma_desc)
#define AE4DMA_QUEUE_SIZE(n)		(AE4DMA_DESCRIPTORS_PER_CMDQ * (n))

/* AE4DMA registers Write/Read */
static inline void ae4dma_pci_reg_write(void *base, int offset, uint32_t value)
{
	volatile void *reg_addr = ((uint8_t *)base + offset);

	rte_write32((rte_cpu_to_le_32(value)), reg_addr);
}

static inline uint32_t ae4dma_pci_reg_read(void *base, int offset)
{
	volatile void *reg_addr = ((uint8_t *)base + offset);

	return rte_le_to_cpu_32(rte_read32(reg_addr));
}

#define AE4DMA_READ_REG_OFFSET(hw_addr, reg_offset) \
	ae4dma_pci_reg_read(hw_addr, reg_offset)

#define AE4DMA_WRITE_REG_OFFSET(hw_addr, reg_offset, value) \
	ae4dma_pci_reg_write(hw_addr, reg_offset, value)

#define AE4DMA_READ_REG(hw_addr) \
	ae4dma_pci_reg_read((void *)(uintptr_t)(hw_addr), 0)

#define AE4DMA_WRITE_REG(hw_addr, value) \
	ae4dma_pci_reg_write((void *)(uintptr_t)(hw_addr), 0, value)

/* A structure describing an AE4DMA command queue. */
struct __rte_cache_aligned ae4dma_cmd_queue {
	char memz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	volatile struct ae4dma_hwq_regs *hwq_regs;

	struct rte_dma_vchan_conf qcfg;
	struct rte_dma_stats stats;
	/* Queue address */
	struct ae4dma_desc *qbase_desc;
	void *qbase_addr;
	rte_iova_t qbase_phys_addr;
	/* Queue identifier */
	uint64_t id;    /* queue id */
	uint64_t qidx;  /* queue index */
	uint64_t qsize; /* queue size */
	/*
	 * Free-running 16-bit ring indices (wrap at 2^16), as required by the
	 * dmadev API. The HW ring slot is derived as ((hw_base + idx) &
	 * (nb_desc - 1)), which is why nb_desc is rounded up to a power of two.
	 */
	uint16_t next_read;  /* ring_idx of the next completion to reap */
	uint16_t next_write; /* ring_idx assigned to the next enqueue */
	uint16_t last_write; /* next_write at last submit; for submitted stat */
	/*
	 * HW ring slot that free-running index 0 maps to. read_idx is HW-owned
	 * (read-only), so on start we anchor to wherever the HW consumer sits
	 * instead of forcing the HW indices to 0.
	 */
	uint16_t hw_base;
};

/*
 * One dmadev per AE4DMA hardware channel: probe creates AE4DMA_MAX_HW_QUEUES
 * dmadevs per PCI function, each owning a single HW command queue.
 */
struct ae4dma_dmadev {
	void *io_regs;
	struct ae4dma_cmd_queue cmd_q; /* single HW queue owned by this dmadev */
};

extern int ae4dma_pmd_logtype;
#define RTE_LOGTYPE_AE4DMA_PMD ae4dma_pmd_logtype

#define AE4DMA_PMD_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, AE4DMA_PMD, "%s(): ", __func__, __VA_ARGS__)

#define AE4DMA_PMD_DEBUG(...)  AE4DMA_PMD_LOG(DEBUG, __VA_ARGS__)
#define AE4DMA_PMD_INFO(...)   AE4DMA_PMD_LOG(INFO, __VA_ARGS__)
#define AE4DMA_PMD_ERR(...)    AE4DMA_PMD_LOG(ERR, __VA_ARGS__)
#define AE4DMA_PMD_WARN(...)   AE4DMA_PMD_LOG(WARNING, __VA_ARGS__)

#endif /* AE4DMA_INTERNAL_H */
