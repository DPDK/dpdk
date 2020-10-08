/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */
#ifndef _RTE_IOAT_RAWDEV_FNS_H_
#define _RTE_IOAT_RAWDEV_FNS_H_

#include <x86intrin.h>
#include <rte_rawdev.h>
#include <rte_memzone.h>
#include <rte_prefetch.h>

/**
 * @internal
 * Structure representing a device descriptor
 */
struct rte_ioat_generic_hw_desc {
	uint32_t size;
	union {
		uint32_t control_raw;
		struct {
			uint32_t int_enable: 1;
			uint32_t src_snoop_disable: 1;
			uint32_t dest_snoop_disable: 1;
			uint32_t completion_update: 1;
			uint32_t fence: 1;
			uint32_t reserved2: 1;
			uint32_t src_page_break: 1;
			uint32_t dest_page_break: 1;
			uint32_t bundle: 1;
			uint32_t dest_dca: 1;
			uint32_t hint: 1;
			uint32_t reserved: 13;
			uint32_t op: 8;
		} control;
	} u;
	uint64_t src_addr;
	uint64_t dest_addr;
	uint64_t next;
	uint64_t op_specific[4];
};

/**
 * @internal
 * Identify the data path to use.
 * Must be first field of rte_ioat_rawdev and rte_idxd_rawdev structs
 */
enum rte_ioat_dev_type {
	RTE_IOAT_DEV,
	RTE_IDXD_DEV,
};

/**
 * @internal
 * some statistics for tracking, if added/changed update xstats fns
 */
struct rte_ioat_xstats {
	uint64_t enqueue_failed;
	uint64_t enqueued;
	uint64_t started;
	uint64_t completed;
};

/**
 * @internal
 * Structure representing an IOAT device instance
 */
struct rte_ioat_rawdev {
	/* common fields at the top - match those in rte_idxd_rawdev */
	enum rte_ioat_dev_type type;
	struct rte_ioat_xstats xstats;

	struct rte_rawdev *rawdev;
	const struct rte_memzone *mz;
	const struct rte_memzone *desc_mz;

	volatile uint16_t *doorbell __rte_cache_aligned;
	phys_addr_t status_addr;
	phys_addr_t ring_addr;

	unsigned short ring_size;
	bool hdls_disable;
	struct rte_ioat_generic_hw_desc *desc_ring;
	__m128i *hdls; /* completion handles for returning to user */


	unsigned short next_read;
	unsigned short next_write;

	/* to report completions, the device will write status back here */
	volatile uint64_t status __rte_cache_aligned;

	/* pointer to the register bar */
	volatile struct rte_ioat_registers *regs;
};

#define RTE_IOAT_CHANSTS_IDLE			0x1
#define RTE_IOAT_CHANSTS_SUSPENDED		0x2
#define RTE_IOAT_CHANSTS_HALTED			0x3
#define RTE_IOAT_CHANSTS_ARMED			0x4

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

	/* 28 bytes of padding here */
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
__ioat_write_desc(int dev_id, uint32_t op, uint64_t src, phys_addr_t dst,
		unsigned int length, uintptr_t src_hdl, uintptr_t dst_hdl)
{
	struct rte_ioat_rawdev *ioat =
			(struct rte_ioat_rawdev *)rte_rawdevs[dev_id].dev_private;
	unsigned short read = ioat->next_read;
	unsigned short write = ioat->next_write;
	unsigned short mask = ioat->ring_size - 1;
	unsigned short space = mask + read - write;
	struct rte_ioat_generic_hw_desc *desc;

	if (space == 0) {
		ioat->xstats.enqueue_failed++;
		return 0;
	}

	ioat->next_write = write + 1;
	write &= mask;

	desc = &ioat->desc_ring[write];
	desc->size = length;
	/* set descriptor write-back every 16th descriptor */
	desc->u.control_raw = (uint32_t)((op << IOAT_CMD_OP_SHIFT) |
			(!(write & 0xF) << IOAT_COMP_UPDATE_SHIFT));
	desc->src_addr = src;
	desc->dest_addr = dst;

	if (!ioat->hdls_disable)
		ioat->hdls[write] = _mm_set_epi64x((int64_t)dst_hdl,
					(int64_t)src_hdl);
	rte_prefetch0(&ioat->desc_ring[ioat->next_write & mask]);

	ioat->xstats.enqueued++;
	return 1;
}

static __rte_always_inline int
__ioat_enqueue_fill(int dev_id, uint64_t pattern, phys_addr_t dst,
		unsigned int length, uintptr_t dst_hdl)
{
	static const uintptr_t null_hdl;

	return __ioat_write_desc(dev_id, ioat_op_fill, pattern, dst, length,
			null_hdl, dst_hdl);
}

/*
 * Enqueue a copy operation onto the ioat device
 */
static __rte_always_inline int
__ioat_enqueue_copy(int dev_id, phys_addr_t src, phys_addr_t dst,
		unsigned int length, uintptr_t src_hdl, uintptr_t dst_hdl)
{
	return __ioat_write_desc(dev_id, ioat_op_copy, src, dst, length,
			src_hdl, dst_hdl);
}

/* add fence to last written descriptor */
static __rte_always_inline int
__ioat_fence(int dev_id)
{
	struct rte_ioat_rawdev *ioat =
			(struct rte_ioat_rawdev *)rte_rawdevs[dev_id].dev_private;
	unsigned short write = ioat->next_write;
	unsigned short mask = ioat->ring_size - 1;
	struct rte_ioat_generic_hw_desc *desc;

	write = (write - 1) & mask;
	desc = &ioat->desc_ring[write];

	desc->u.control.fence = 1;
	return 0;
}

/*
 * Trigger hardware to begin performing enqueued operations
 */
static __rte_always_inline void
__ioat_perform_ops(int dev_id)
{
	struct rte_ioat_rawdev *ioat =
			(struct rte_ioat_rawdev *)rte_rawdevs[dev_id].dev_private;
	ioat->desc_ring[(ioat->next_write - 1) & (ioat->ring_size - 1)].u
			.control.completion_update = 1;
	rte_compiler_barrier();
	*ioat->doorbell = ioat->next_write;
	ioat->xstats.started = ioat->xstats.enqueued;
}

/**
 * @internal
 * Returns the index of the last completed operation.
 */
static __rte_always_inline int
__ioat_get_last_completed(struct rte_ioat_rawdev *ioat, int *error)
{
	uint64_t status = ioat->status;

	/* lower 3 bits indicate "transfer status" : active, idle, halted.
	 * We can ignore bit 0.
	 */
	*error = status & (RTE_IOAT_CHANSTS_SUSPENDED | RTE_IOAT_CHANSTS_ARMED);
	return (status - ioat->ring_addr) >> 6;
}

/*
 * Returns details of operations that have been completed
 */
static __rte_always_inline int
__ioat_completed_ops(int dev_id, uint8_t max_copies,
		uintptr_t *src_hdls, uintptr_t *dst_hdls)
{
	struct rte_ioat_rawdev *ioat =
			(struct rte_ioat_rawdev *)rte_rawdevs[dev_id].dev_private;
	unsigned short mask = (ioat->ring_size - 1);
	unsigned short read = ioat->next_read;
	unsigned short end_read, count;
	int error;
	int i = 0;

	end_read = (__ioat_get_last_completed(ioat, &error) + 1) & mask;
	count = (end_read - (read & mask)) & mask;

	if (error) {
		rte_errno = EIO;
		return -1;
	}

	if (ioat->hdls_disable) {
		read += count;
		goto end;
	}

	if (count > max_copies)
		count = max_copies;

	for (; i < count - 1; i += 2, read += 2) {
		__m128i hdls0 = _mm_load_si128(&ioat->hdls[read & mask]);
		__m128i hdls1 = _mm_load_si128(&ioat->hdls[(read + 1) & mask]);

		_mm_storeu_si128((__m128i *)&src_hdls[i],
				_mm_unpacklo_epi64(hdls0, hdls1));
		_mm_storeu_si128((__m128i *)&dst_hdls[i],
				_mm_unpackhi_epi64(hdls0, hdls1));
	}
	for (; i < count; i++, read++) {
		uintptr_t *hdls = (uintptr_t *)&ioat->hdls[read & mask];
		src_hdls[i] = hdls[0];
		dst_hdls[i] = hdls[1];
	}

end:
	ioat->next_read = read;
	ioat->xstats.completed += count;
	return count;
}

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

static __rte_always_inline void
__idxd_perform_ops(int dev_id)
{
	struct rte_idxd_rawdev *idxd =
			(struct rte_idxd_rawdev *)rte_rawdevs[dev_id].dev_private;
	struct rte_idxd_desc_batch *b = &idxd->batch_ring[idxd->next_batch];

	if (b->submitted || b->op_count == 0)
		return;
	b->hdl_end = idxd->next_free_hdl;
	b->comp.status = 0;
	b->submitted = 1;
	b->batch_desc.size = b->op_count + 1;
	__idxd_movdir64b(idxd->portal, &b->batch_desc);

	if (++idxd->next_batch == idxd->batch_ring_sz)
		idxd->next_batch = 0;
	idxd->xstats.started = idxd->xstats.enqueued;
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

static inline int
rte_ioat_enqueue_fill(int dev_id, uint64_t pattern, phys_addr_t dst,
		unsigned int len, uintptr_t dst_hdl)
{
	enum rte_ioat_dev_type *type =
			(enum rte_ioat_dev_type *)rte_rawdevs[dev_id].dev_private;
	if (*type == RTE_IDXD_DEV)
		return __idxd_enqueue_fill(dev_id, pattern, dst, len, dst_hdl);
	else
		return __ioat_enqueue_fill(dev_id, pattern, dst, len, dst_hdl);
}

static inline int
rte_ioat_enqueue_copy(int dev_id, phys_addr_t src, phys_addr_t dst,
		unsigned int length, uintptr_t src_hdl, uintptr_t dst_hdl)
{
	enum rte_ioat_dev_type *type =
			(enum rte_ioat_dev_type *)rte_rawdevs[dev_id].dev_private;
	if (*type == RTE_IDXD_DEV)
		return __idxd_enqueue_copy(dev_id, src, dst, length,
				src_hdl, dst_hdl);
	else
		return __ioat_enqueue_copy(dev_id, src, dst, length,
				src_hdl, dst_hdl);
}

static inline int
rte_ioat_fence(int dev_id)
{
	enum rte_ioat_dev_type *type =
			(enum rte_ioat_dev_type *)rte_rawdevs[dev_id].dev_private;
	if (*type == RTE_IDXD_DEV)
		return __idxd_fence(dev_id);
	else
		return __ioat_fence(dev_id);
}

static inline void
rte_ioat_perform_ops(int dev_id)
{
	enum rte_ioat_dev_type *type =
			(enum rte_ioat_dev_type *)rte_rawdevs[dev_id].dev_private;
	if (*type == RTE_IDXD_DEV)
		return __idxd_perform_ops(dev_id);
	else
		return __ioat_perform_ops(dev_id);
}

static inline int
rte_ioat_completed_ops(int dev_id, uint8_t max_copies,
		uintptr_t *src_hdls, uintptr_t *dst_hdls)
{
	enum rte_ioat_dev_type *type =
			(enum rte_ioat_dev_type *)rte_rawdevs[dev_id].dev_private;
	if (*type == RTE_IDXD_DEV)
		return __idxd_completed_ops(dev_id, max_copies,
				src_hdls, dst_hdls);
	else
		return __ioat_completed_ops(dev_id,  max_copies,
				src_hdls, dst_hdls);
}

static inline void
__rte_deprecated_msg("use rte_ioat_perform_ops() instead")
rte_ioat_do_copies(int dev_id) { rte_ioat_perform_ops(dev_id); }

static inline int
__rte_deprecated_msg("use rte_ioat_completed_ops() instead")
rte_ioat_completed_copies(int dev_id, uint8_t max_copies,
		uintptr_t *src_hdls, uintptr_t *dst_hdls)
{
	return rte_ioat_completed_ops(dev_id, max_copies, src_hdls, dst_hdls);
}

#endif /* _RTE_IOAT_RAWDEV_FNS_H_ */
