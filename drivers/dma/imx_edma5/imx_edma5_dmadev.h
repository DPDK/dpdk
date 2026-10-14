/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2026 NXP
 */

#ifndef IMX_EDMA5_DMADEV_H
#define IMX_EDMA5_DMADEV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_dmadev.h>
#include <rte_io.h>
#include <rte_memory.h>

#include "imx_edma5_hw.h"

/*
 * CPU data-cache maintenance for the non-coherent eDMA5 master: clean the
 * source before the transfer and clean+invalidate the destination after it.
 * DC IVAC (invalidate-only) is EL1-only and faults from userspace, so DC CIVAC
 * is used. No-ops on non-arm64 builds.
 */
static inline void
imx_edma5_dcbf(void *p)
{
#ifdef RTE_ARCH_ARM64
	asm volatile("dc cvac, %0" : : "r"(p) : "memory");
#else
	RTE_SET_USED(p);
#endif
}

static inline void
imx_edma5_dccivac(void *p)
{
#ifdef RTE_ARCH_ARM64
	asm volatile("dc civac, %0" : : "r"(p) : "memory");
#else
	RTE_SET_USED(p);
#endif
}

/* Clean a VA range to the Point of Coherency. */
static inline void
imx_edma5_cache_clean(void *addr, size_t len)
{
	uintptr_t p = (uintptr_t)addr & ~(uintptr_t)(RTE_CACHE_LINE_SIZE - 1);
	uintptr_t end = (uintptr_t)addr + len;

	for (; p < end; p += RTE_CACHE_LINE_SIZE)
		imx_edma5_dcbf((void *)p);
	/*
	 * DSB ensures all DC CVAC instructions have completed to the Point of
	 * Coherency before the eDMA5 is allowed to read the source data.
	 */
#ifdef RTE_ARCH_ARM64
	asm volatile("dsb sy" ::: "memory");
#endif
}

/* Clean+invalidate a VA range to the Point of Coherency. */
static inline void
imx_edma5_cache_inval(void *addr, size_t len)
{
	uintptr_t p = (uintptr_t)addr & ~(uintptr_t)(RTE_CACHE_LINE_SIZE - 1);
	uintptr_t end = (uintptr_t)addr + len;

	for (; p < end; p += RTE_CACHE_LINE_SIZE)
		imx_edma5_dccivac((void *)p);
	/*
	 * DSB ensures all DC CIVAC instructions have completed to the Point of
	 * Coherency before the CPU reads the destination data written by eDMA5.
	 */
#ifdef RTE_ARCH_ARM64
	asm volatile("dsb sy" ::: "memory");
#endif
}

/*
 * NBYTES field maximum: bits 31:30 of TCD_NBYTES are SMLOE/DMLOE control bits
 * when the minor loop offset feature is enabled. To avoid accidentally setting
 * those bits, cap every single-block transfer at the 30-bit maximum count.
 */
#define IMX_EDMA5_MAX_NBYTES		((uint32_t)((1u << 30) - 1))

/* Maximum scatter-gather segments per copy_sg request. */
#define IMX_EDMA5_MAX_SGES		16

/*
 * In-memory TCD64 descriptors reserved per job slot for scatter-gather.
 * Splitting an asymmetric src/dst segment list at the union of both sets of
 * boundaries yields at most n_src + n_dst - 1 sub-transfers, so with up to
 * IMX_EDMA5_MAX_SGES segments per side the worst case fits in this bound.
 */
#define IMX_EDMA5_SG_TCD_PER_JOB	(2 * IMX_EDMA5_MAX_SGES)

/* Software job ring size per virtual channel (power of two). */
#define IMX_EDMA5_MAX_DESC		4096
#define IMX_EDMA5_MIN_DESC		32

/* Per in-flight job bookkeeping. */
struct imx_edma5_job {
	uint16_t ridx;			/* ring index returned to application */
	uint8_t  submitted;		/* job has been started on hardware */
	uint8_t  done;			/* job completed */
	uint8_t  error;			/* job completed with error */
	/*
	 * Destination VA and byte count of the copy, used to invalidate the
	 * destination cache lines on completion (non-coherent eDMA master).
	 * NULL if the VA could not be resolved. Unused for SG jobs (nb_sg > 0).
	 */
	void    *dst_va;
	uint32_t len;
	/*
	 * Source/destination IOVAs of a plain single-block copy, recorded at
	 * enqueue time and used to program the TCD at submit time (the eDMA5 has
	 * a single register TCD shared by all jobs). Unused for SG jobs.
	 */
	rte_iova_t src_iova;
	rte_iova_t dst_iova;
	/*
	 * Scatter-gather state. nb_sg is the segment count (0 for a plain copy);
	 * sg_tcd points at this job's slice of the vchan's in-memory TCD pool.
	 */
	uint16_t nb_sg;
	struct imx_edma5_hw_tcd64 *sg_tcd;
};

/* A virtual channel maps 1:1 onto a single eDMA5 hardware channel. */
struct imx_edma5_vchan {
	uint8_t  *ch_regs;		/* channel register window base */
	uint8_t  *tcd_regs;		/* channel TCD base (ch_regs + TCD_OFF) */
	uint32_t  hw_chan;		/* hardware channel index */

	struct imx_edma5_job *jobs;	/* software job ring */
	uint16_t nb_desc;		/* size of job ring (power of two) */
	uint16_t desc_mask;		/* nb_desc - 1 */

	/*
	 * Pool of in-memory TCD64 descriptors for scatter-gather, sized
	 * nb_desc * IMX_EDMA5_SG_TCD_PER_JOB. Each job slot owns a contiguous
	 * slice of IMX_EDMA5_SG_TCD_PER_JOB descriptors. sg_tcd_iova is the
	 * pool base IOVA.
	 */
	struct imx_edma5_hw_tcd64 *sg_tcd_pool;
	rte_iova_t sg_tcd_iova;

	uint16_t head;			/* next slot to enqueue */
	uint16_t tail;			/* next slot to reap */
	uint16_t nb_enqueued;		/* outstanding jobs in ring (unreaped) */
	uint16_t ridx;			/* running ring index counter */
	uint16_t last_idx;		/* last completed ring index */

	uint64_t submitted_count;
	uint64_t completed_count;
	uint64_t errors_count;

	bool configured;
};

/* Per-device (per eDMA5 instance) private data. */
struct imx_edma5_dev {
	uint8_t *reg_base;		/* mapped register window base */
	uint64_t reg_size;		/* mapped register window length */

	uint16_t nb_channels;		/* channels available on this instance */
	uint16_t max_vchans;		/* channels usable as dmadev vchans */

	/*
	 * Bitmask of hardware channels reserved for other bus masters (from the
	 * device-tree "dma-channel-mask"). A set bit marks a channel this driver
	 * must not touch; accessing it faults with a bus external abort.
	 */
	uint64_t masked_channels;
	/* Map of usable dmadev vchan index -> hardware channel index. */
	uint16_t chan_map[IMX_EDMA5_MAX_CHANNELS];

	struct imx_edma5_vchan *vchans;	/* array of vchan states */
	uint16_t nb_vchans;		/* number of configured vchans */

	int16_t dev_id;			/* dmadev id */
};

/* MMIO helpers (little-endian device). */
static inline uint32_t
imx_edma5_read32(const uint8_t *base, uint32_t off)
{
	return rte_le_to_cpu_32(rte_read32(base + off));
}

static inline void
imx_edma5_write32(uint8_t *base, uint32_t off, uint32_t val)
{
	rte_write32(rte_cpu_to_le_32(val), base + off);
}

static inline uint16_t
imx_edma5_read16(const uint8_t *base, uint32_t off)
{
	return rte_le_to_cpu_16(rte_read16(base + off));
}

static inline void
imx_edma5_write16(uint8_t *base, uint32_t off, uint16_t val)
{
	rte_write16(rte_cpu_to_le_16(val), base + off);
}

static inline void
imx_edma5_write64(uint8_t *base, uint32_t off, uint64_t val)
{
	/* Write the 64-bit field as two 32-bit accesses (order not significant). */
	imx_edma5_write32(base, off, (uint32_t)(val & 0xFFFFFFFFu));
	imx_edma5_write32(base, off + 4, (uint32_t)(val >> 32));
}

#endif /* IMX_EDMA5_DMADEV_H */
