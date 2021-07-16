/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */
#ifndef _RTE_IDXD_RAWDEV_FNS_H_
#define _RTE_IDXD_RAWDEV_FNS_H_

/**
 * @file
 * This header file contains the implementation of the various ioat
 * rawdev functions for DSA hardware. The API specification and key
 * public structures are defined in "rte_ioat_rawdev.h".
 *
 * This file should not be included directly, but instead applications should
 * include "rte_ioat_rawdev.h", which then includes this file - and the
 * IOAT/CBDMA equivalent header - in turn.
 */

#include <stdint.h>

/*
 * Defines used in the data path for interacting with IDXD hardware.
 */
#define IDXD_CMD_OP_SHIFT 24
enum rte_idxd_ops {
	idxd_op_nop = 0,
	idxd_op_batch,
	idxd_op_drain,
	idxd_op_memmove,
	idxd_op_fill
};

#define IDXD_FLAG_FENCE                 (1 << 0)
#define IDXD_FLAG_COMPLETION_ADDR_VALID (1 << 2)
#define IDXD_FLAG_REQUEST_COMPLETION    (1 << 3)
#define IDXD_FLAG_CACHE_CONTROL         (1 << 8)

#define IOAT_COMP_UPDATE_SHIFT	3
#define IOAT_CMD_OP_SHIFT	24
enum rte_ioat_ops {
	ioat_op_copy = 0,	/* Standard DMA Operation */
	ioat_op_fill		/* Block Fill */
};

/**
 * Hardware descriptor used by DSA hardware, for both bursts and
 * for individual operations.
 */
struct rte_idxd_hw_desc {
	uint32_t pasid;
	uint32_t op_flags;
	rte_iova_t completion;

	RTE_STD_C11
	union {
		rte_iova_t src;      /* source address for copy ops etc. */
		rte_iova_t desc_addr; /* descriptor pointer for batch */
	};
	rte_iova_t dst;

	uint32_t size;    /* length of data for op, or batch size */

	uint16_t intr_handle; /* completion interrupt handle */

	/* remaining 26 bytes are reserved */
	uint16_t __reserved[13];
} __rte_aligned(64);

/**
 * Completion record structure written back by DSA
 */
struct rte_idxd_completion {
	uint8_t status;
	uint8_t result;
	/* 16-bits pad here */
	uint32_t completed_size; /* data length, or descriptors for batch */

	rte_iova_t fault_address;
	uint32_t invalid_flags;
} __rte_aligned(32);

/**
 * structure used to save the "handles" provided by the user to be
 * returned to the user on job completion.
 */
struct rte_idxd_user_hdl {
	uint64_t src;
	uint64_t dst;
};

/**
 * @internal
 * Structure representing an IDXD device instance
 */
struct rte_idxd_rawdev {
	enum rte_ioat_dev_type type;
	struct rte_ioat_xstats xstats;

	void *portal; /* address to write the batch descriptor */

	struct rte_ioat_rawdev_config cfg;
	rte_iova_t desc_iova; /* base address of desc ring, needed for completions */

	/* counters to track the batches */
	unsigned short max_batches;
	unsigned short batch_idx_read;
	unsigned short batch_idx_write;
	unsigned short *batch_idx_ring; /* store where each batch ends */

	/* track descriptors and handles */
	unsigned short desc_ring_mask;
	unsigned short hdls_avail; /* handles for ops completed */
	unsigned short hdls_read; /* the read pointer for hdls/desc rings */
	unsigned short batch_start; /* start+size == write pointer for hdls/desc */
	unsigned short batch_size;

	struct rte_idxd_hw_desc *desc_ring;
	struct rte_idxd_user_hdl *hdl_ring;
	/* flags to indicate handle validity. Kept separate from ring, to avoid
	 * using 8 bytes per flag. Upper 8 bits holds error code if any.
	 */
	uint16_t *hdl_ring_flags;
};

#define RTE_IDXD_HDL_NORMAL     0
#define RTE_IDXD_HDL_INVALID    (1 << 0) /* no handle stored for this element */
#define RTE_IDXD_HDL_OP_FAILED  (1 << 1) /* return failure for this one */
#define RTE_IDXD_HDL_OP_SKIPPED (1 << 2) /* this op was skipped */

static __rte_always_inline uint16_t
__idxd_burst_capacity(int dev_id)
{
	struct rte_idxd_rawdev *idxd =
			(struct rte_idxd_rawdev *)rte_rawdevs[dev_id].dev_private;
	uint16_t write_idx = idxd->batch_start + idxd->batch_size;
	uint16_t used_space, free_space;

	/* Check for space in the batch ring */
	if ((idxd->batch_idx_read == 0 && idxd->batch_idx_write == idxd->max_batches) ||
			idxd->batch_idx_write + 1 == idxd->batch_idx_read)
		return 0;

	/* for descriptors, check for wrap-around on write but not read */
	if (idxd->hdls_read > write_idx)
		write_idx += idxd->desc_ring_mask + 1;
	used_space = write_idx - idxd->hdls_read;

	/* Return amount of free space in the descriptor ring
	 * subtract 1 for space for batch descriptor and 1 for possible null desc
	 */
	free_space = idxd->desc_ring_mask - used_space;
	if (free_space < 2)
		return 0;
	return free_space - 2;
}

static __rte_always_inline rte_iova_t
__desc_idx_to_iova(struct rte_idxd_rawdev *idxd, uint16_t n)
{
	return idxd->desc_iova + (n * sizeof(struct rte_idxd_hw_desc));
}

static __rte_always_inline int
__idxd_write_desc(int dev_id,
		const uint32_t op_flags,
		const rte_iova_t src,
		const rte_iova_t dst,
		const uint32_t size,
		const struct rte_idxd_user_hdl *hdl)
{
	struct rte_idxd_rawdev *idxd =
			(struct rte_idxd_rawdev *)rte_rawdevs[dev_id].dev_private;
	uint16_t write_idx = idxd->batch_start + idxd->batch_size;
	uint16_t mask = idxd->desc_ring_mask;

	/* first check batch ring space then desc ring space */
	if ((idxd->batch_idx_read == 0 && idxd->batch_idx_write == idxd->max_batches) ||
			idxd->batch_idx_write + 1 == idxd->batch_idx_read)
		goto failed;
	/* for descriptor ring, we always need a slot for batch completion */
	if (((write_idx + 2) & mask) == idxd->hdls_read ||
			((write_idx + 1) & mask) == idxd->hdls_read)
		goto failed;

	/* write desc and handle. Note, descriptors don't wrap */
	idxd->desc_ring[write_idx].pasid = 0;
	idxd->desc_ring[write_idx].op_flags = op_flags | IDXD_FLAG_COMPLETION_ADDR_VALID;
	idxd->desc_ring[write_idx].completion = __desc_idx_to_iova(idxd, write_idx & mask);
	idxd->desc_ring[write_idx].src = src;
	idxd->desc_ring[write_idx].dst = dst;
	idxd->desc_ring[write_idx].size = size;

	if (hdl == NULL)
		idxd->hdl_ring_flags[write_idx & mask] = RTE_IDXD_HDL_INVALID;
	else
		idxd->hdl_ring[write_idx & mask] = *hdl;
	idxd->batch_size++;

	idxd->xstats.enqueued++;

	rte_prefetch0_write(&idxd->desc_ring[write_idx + 1]);
	return 1;

failed:
	idxd->xstats.enqueue_failed++;
	rte_errno = ENOSPC;
	return 0;
}

static __rte_always_inline int
__idxd_enqueue_fill(int dev_id, uint64_t pattern, rte_iova_t dst,
		unsigned int length, uintptr_t dst_hdl)
{
	const struct rte_idxd_user_hdl hdl = {
			.dst = dst_hdl
	};
	return __idxd_write_desc(dev_id,
			(idxd_op_fill << IDXD_CMD_OP_SHIFT) | IDXD_FLAG_CACHE_CONTROL,
			pattern, dst, length, &hdl);
}

static __rte_always_inline int
__idxd_enqueue_copy(int dev_id, rte_iova_t src, rte_iova_t dst,
		unsigned int length, uintptr_t src_hdl, uintptr_t dst_hdl)
{
	const struct rte_idxd_user_hdl hdl = {
			.src = src_hdl,
			.dst = dst_hdl
	};
	return __idxd_write_desc(dev_id,
			(idxd_op_memmove << IDXD_CMD_OP_SHIFT) | IDXD_FLAG_CACHE_CONTROL,
			src, dst, length, &hdl);
}

static __rte_always_inline int
__idxd_enqueue_nop(int dev_id)
{
	/* only op field needs filling - zero src, dst and length */
	return __idxd_write_desc(dev_id, idxd_op_nop << IDXD_CMD_OP_SHIFT,
			0, 0, 0, NULL);
}

static __rte_always_inline int
__idxd_fence(int dev_id)
{
	/* only op field needs filling - zero src, dst and length */
	return __idxd_write_desc(dev_id, IDXD_FLAG_FENCE, 0, 0, 0, NULL);
}

static __rte_always_inline void
__idxd_movdir64b(volatile void *dst, const struct rte_idxd_hw_desc *src)
{
	asm volatile (".byte 0x66, 0x0f, 0x38, 0xf8, 0x02"
			:
			: "a" (dst), "d" (src)
			: "memory");
}

static __rte_always_inline int
__idxd_perform_ops(int dev_id)
{
	struct rte_idxd_rawdev *idxd =
			(struct rte_idxd_rawdev *)rte_rawdevs[dev_id].dev_private;

	if (!idxd->cfg.no_prefetch_completions)
		rte_prefetch1(&idxd->desc_ring[idxd->batch_idx_ring[idxd->batch_idx_read]]);

	if (idxd->batch_size == 0)
		return 0;

	if (idxd->batch_size == 1)
		/* use a NOP as a null descriptor, so batch_size >= 2 */
		if (__idxd_enqueue_nop(dev_id) != 1)
			return -1;

	/* write completion beyond last desc in the batch */
	uint16_t comp_idx = (idxd->batch_start + idxd->batch_size) & idxd->desc_ring_mask;
	*((uint64_t *)&idxd->desc_ring[comp_idx]) = 0; /* zero start of desc */
	idxd->hdl_ring_flags[comp_idx] = RTE_IDXD_HDL_INVALID;

	const struct rte_idxd_hw_desc batch_desc = {
			.op_flags = (idxd_op_batch << IDXD_CMD_OP_SHIFT) |
				IDXD_FLAG_COMPLETION_ADDR_VALID |
				IDXD_FLAG_REQUEST_COMPLETION,
			.desc_addr = __desc_idx_to_iova(idxd, idxd->batch_start),
			.completion = __desc_idx_to_iova(idxd, comp_idx),
			.size = idxd->batch_size,
	};

	_mm_sfence(); /* fence before writing desc to device */
	__idxd_movdir64b(idxd->portal, &batch_desc);
	idxd->xstats.started += idxd->batch_size;

	idxd->batch_start += idxd->batch_size + 1;
	idxd->batch_start &= idxd->desc_ring_mask;
	idxd->batch_size = 0;

	idxd->batch_idx_ring[idxd->batch_idx_write++] = comp_idx;
	if (idxd->batch_idx_write > idxd->max_batches)
		idxd->batch_idx_write = 0;

	return 0;
}

static __rte_always_inline int
__idxd_completed_ops(int dev_id, uint8_t max_ops, uint32_t *status, uint8_t *num_unsuccessful,
		uintptr_t *src_hdls, uintptr_t *dst_hdls)
{
	struct rte_idxd_rawdev *idxd =
			(struct rte_idxd_rawdev *)rte_rawdevs[dev_id].dev_private;
	unsigned short n, h_idx;

	while (idxd->batch_idx_read != idxd->batch_idx_write) {
		uint16_t idx_to_chk = idxd->batch_idx_ring[idxd->batch_idx_read];
		volatile struct rte_idxd_completion *comp_to_chk =
				(struct rte_idxd_completion *)&idxd->desc_ring[idx_to_chk];
		uint8_t batch_status = comp_to_chk->status;
		if (batch_status == 0)
			break;
		comp_to_chk->status = 0;
		if (unlikely(batch_status > 1)) {
			/* error occurred somewhere in batch, start where last checked */
			uint16_t desc_count = comp_to_chk->completed_size;
			uint16_t batch_start = idxd->hdls_avail;
			uint16_t batch_end = idx_to_chk;

			if (batch_start > batch_end)
				batch_end += idxd->desc_ring_mask + 1;
			/* go through each batch entry and see status */
			for (n = 0; n < desc_count; n++) {
				uint16_t idx = (batch_start + n) & idxd->desc_ring_mask;
				volatile struct rte_idxd_completion *comp =
					(struct rte_idxd_completion *)&idxd->desc_ring[idx];
				if (comp->status != 0 &&
						idxd->hdl_ring_flags[idx] == RTE_IDXD_HDL_NORMAL) {
					idxd->hdl_ring_flags[idx] = RTE_IDXD_HDL_OP_FAILED;
					idxd->hdl_ring_flags[idx] |= (comp->status << 8);
					comp->status = 0; /* clear error for next time */
				}
			}
			/* if batch is incomplete, mark rest as skipped */
			for ( ; n < batch_end - batch_start; n++) {
				uint16_t idx = (batch_start + n) & idxd->desc_ring_mask;
				if (idxd->hdl_ring_flags[idx] == RTE_IDXD_HDL_NORMAL)
					idxd->hdl_ring_flags[idx] = RTE_IDXD_HDL_OP_SKIPPED;
			}
		}
		/* avail points to one after the last one written */
		idxd->hdls_avail = (idx_to_chk + 1) & idxd->desc_ring_mask;
		idxd->batch_idx_read++;
		if (idxd->batch_idx_read > idxd->max_batches)
			idxd->batch_idx_read = 0;
	}

	n = 0;
	h_idx = idxd->hdls_read;
	while (h_idx != idxd->hdls_avail) {
		uint16_t flag = idxd->hdl_ring_flags[h_idx];
		if (flag != RTE_IDXD_HDL_INVALID) {
			if (!idxd->cfg.hdls_disable) {
				src_hdls[n] = idxd->hdl_ring[h_idx].src;
				dst_hdls[n] = idxd->hdl_ring[h_idx].dst;
			}
			if (unlikely(flag != RTE_IDXD_HDL_NORMAL)) {
				if (status != NULL)
					status[n] = flag == RTE_IDXD_HDL_OP_SKIPPED ?
							RTE_IOAT_OP_SKIPPED :
							/* failure case, return err code */
							idxd->hdl_ring_flags[h_idx] >> 8;
				if (num_unsuccessful != NULL)
					*num_unsuccessful += 1;
			}
			n++;
		}
		idxd->hdl_ring_flags[h_idx] = RTE_IDXD_HDL_NORMAL;
		if (++h_idx > idxd->desc_ring_mask)
			h_idx = 0;
		if (n >= max_ops)
			break;
	}

	/* skip over any remaining blank elements, e.g. batch completion */
	while (idxd->hdl_ring_flags[h_idx] == RTE_IDXD_HDL_INVALID && h_idx != idxd->hdls_avail) {
		idxd->hdl_ring_flags[h_idx] = RTE_IDXD_HDL_NORMAL;
		if (++h_idx > idxd->desc_ring_mask)
			h_idx = 0;
	}
	idxd->hdls_read = h_idx;

	idxd->xstats.completed += n;
	return n;
}

#endif
