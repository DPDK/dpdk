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
 * Defines used in the data path for interacting with hardware.
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

#define BATCH_SIZE 64

/**
 * Structure used inside the driver for building up and submitting
 * a batch of operations to the DSA hardware.
 */
struct rte_idxd_desc_batch {
	struct rte_idxd_completion comp; /* the completion record for batch */

	uint16_t submitted;
	uint16_t op_count;
	uint16_t hdl_end;

	struct rte_idxd_hw_desc batch_desc;

	/* batches must always have 2 descriptors, so put a null at the start */
	struct rte_idxd_hw_desc null_desc;
	struct rte_idxd_hw_desc ops[BATCH_SIZE];
};

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

	/* counters to track the batches and the individual op handles */
	uint16_t batch_ring_sz;  /* size of batch ring */
	uint16_t hdl_ring_sz;    /* size of the user hdl ring */

	uint16_t next_batch;     /* where we write descriptor ops */
	uint16_t next_completed; /* batch where we read completions */
	uint16_t next_ret_hdl;   /* the next user hdl to return */
	uint16_t last_completed_hdl; /* the last user hdl that has completed */
	uint16_t next_free_hdl;  /* where the handle for next op will go */
	uint16_t hdls_disable;   /* disable tracking completion handles */

	struct rte_idxd_user_hdl *hdl_ring;
	struct rte_idxd_desc_batch *batch_ring;
};

static __rte_always_inline int
__idxd_write_desc(int dev_id, const struct rte_idxd_hw_desc *desc,
		const struct rte_idxd_user_hdl *hdl)
{
	struct rte_idxd_rawdev *idxd =
			(struct rte_idxd_rawdev *)rte_rawdevs[dev_id].dev_private;
	struct rte_idxd_desc_batch *b = &idxd->batch_ring[idxd->next_batch];

	/* check for room in the handle ring */
	if (((idxd->next_free_hdl + 1) & (idxd->hdl_ring_sz - 1)) == idxd->next_ret_hdl)
		goto failed;

	/* check for space in current batch */
	if (b->op_count >= BATCH_SIZE)
		goto failed;

	/* check that we can actually use the current batch */
	if (b->submitted)
		goto failed;

	/* write the descriptor */
	b->ops[b->op_count++] = *desc;

	/* store the completion details */
	if (!idxd->hdls_disable)
		idxd->hdl_ring[idxd->next_free_hdl] = *hdl;
	if (++idxd->next_free_hdl == idxd->hdl_ring_sz)
		idxd->next_free_hdl = 0;

	idxd->xstats.enqueued++;
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
	const struct rte_idxd_hw_desc desc = {
			.op_flags =  (idxd_op_fill << IDXD_CMD_OP_SHIFT) |
				IDXD_FLAG_CACHE_CONTROL,
			.src = pattern,
			.dst = dst,
			.size = length
	};
	const struct rte_idxd_user_hdl hdl = {
			.dst = dst_hdl
	};
	return __idxd_write_desc(dev_id, &desc, &hdl);
}

static __rte_always_inline int
__idxd_enqueue_copy(int dev_id, rte_iova_t src, rte_iova_t dst,
		unsigned int length, uintptr_t src_hdl, uintptr_t dst_hdl)
{
	const struct rte_idxd_hw_desc desc = {
			.op_flags =  (idxd_op_memmove << IDXD_CMD_OP_SHIFT) |
				IDXD_FLAG_CACHE_CONTROL,
			.src = src,
			.dst = dst,
			.size = length
	};
	const struct rte_idxd_user_hdl hdl = {
			.src = src_hdl,
			.dst = dst_hdl
	};
	return __idxd_write_desc(dev_id, &desc, &hdl);
}

static __rte_always_inline int
__idxd_fence(int dev_id)
{
	static const struct rte_idxd_hw_desc fence = {
			.op_flags = IDXD_FLAG_FENCE
	};
	static const struct rte_idxd_user_hdl null_hdl;
	return __idxd_write_desc(dev_id, &fence, &null_hdl);
}

static __rte_always_inline void
__idxd_movdir64b(volatile void *dst, const void *src)
{
	asm volatile (".byte 0x66, 0x0f, 0x38, 0xf8, 0x02"
			:
			: "a" (dst), "d" (src));
}

static __rte_always_inline int
__idxd_perform_ops(int dev_id)
{
	struct rte_idxd_rawdev *idxd =
			(struct rte_idxd_rawdev *)rte_rawdevs[dev_id].dev_private;
	struct rte_idxd_desc_batch *b = &idxd->batch_ring[idxd->next_batch];

	if (b->submitted || b->op_count == 0)
		return 0;
	b->hdl_end = idxd->next_free_hdl;
	b->comp.status = 0;
	b->submitted = 1;
	b->batch_desc.size = b->op_count + 1;
	__idxd_movdir64b(idxd->portal, &b->batch_desc);

	if (++idxd->next_batch == idxd->batch_ring_sz)
		idxd->next_batch = 0;
	idxd->xstats.started = idxd->xstats.enqueued;
	return 0;
}

static __rte_always_inline int
__idxd_completed_ops(int dev_id, uint8_t max_ops,
		uintptr_t *src_hdls, uintptr_t *dst_hdls)
{
	struct rte_idxd_rawdev *idxd =
			(struct rte_idxd_rawdev *)rte_rawdevs[dev_id].dev_private;
	struct rte_idxd_desc_batch *b = &idxd->batch_ring[idxd->next_completed];
	uint16_t h_idx = idxd->next_ret_hdl;
	int n = 0;

	while (b->submitted && b->comp.status != 0) {
		idxd->last_completed_hdl = b->hdl_end;
		b->submitted = 0;
		b->op_count = 0;
		if (++idxd->next_completed == idxd->batch_ring_sz)
			idxd->next_completed = 0;
		b = &idxd->batch_ring[idxd->next_completed];
	}

	if (!idxd->hdls_disable)
		for (n = 0; n < max_ops && h_idx != idxd->last_completed_hdl; n++) {
			src_hdls[n] = idxd->hdl_ring[h_idx].src;
			dst_hdls[n] = idxd->hdl_ring[h_idx].dst;
			if (++h_idx == idxd->hdl_ring_sz)
				h_idx = 0;
		}
	else
		while (h_idx != idxd->last_completed_hdl) {
			n++;
			if (++h_idx == idxd->hdl_ring_sz)
				h_idx = 0;
		}

	idxd->next_ret_hdl = h_idx;

	idxd->xstats.completed += n;
	return n;
}

#endif
