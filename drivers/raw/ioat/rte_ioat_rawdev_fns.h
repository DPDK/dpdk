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
 * Structure representing an IOAT device instance
 */
struct rte_ioat_rawdev {
	enum rte_ioat_dev_type type;
	struct rte_rawdev *rawdev;
	const struct rte_memzone *mz;
	const struct rte_memzone *desc_mz;

	volatile uint16_t *doorbell;
	phys_addr_t status_addr;
	phys_addr_t ring_addr;

	unsigned short ring_size;
	bool hdls_disable;
	struct rte_ioat_generic_hw_desc *desc_ring;
	__m128i *hdls; /* completion handles for returning to user */


	unsigned short next_read;
	unsigned short next_write;

	/* some statistics for tracking, if added/changed update xstats fns*/
	uint64_t enqueue_failed __rte_cache_aligned;
	uint64_t enqueued;
	uint64_t started;
	uint64_t completed;

	/* to report completions, the device will write status back here */
	volatile uint64_t status __rte_cache_aligned;

	/* pointer to the register bar */
	volatile struct rte_ioat_registers *regs;
};

#define RTE_IOAT_CHANSTS_IDLE			0x1
#define RTE_IOAT_CHANSTS_SUSPENDED		0x2
#define RTE_IOAT_CHANSTS_HALTED			0x3
#define RTE_IOAT_CHANSTS_ARMED			0x4

/**
 * @internal
 * Structure representing an IDXD device instance
 */
struct rte_idxd_rawdev {
	enum rte_ioat_dev_type type;
	void *portal; /* address to write the batch descriptor */

	/* counters to track the batches and the individual op handles */
	uint16_t batch_ring_sz;  /* size of batch ring */
	uint16_t hdl_ring_sz;    /* size of the user hdl ring */

	uint16_t next_batch;     /* where we write descriptor ops */
	uint16_t next_completed; /* batch where we read completions */
	uint16_t next_ret_hdl;   /* the next user hdl to return */
	uint16_t last_completed_hdl; /* the last user hdl that has completed */
	uint16_t next_free_hdl;  /* where the handle for next op will go */

	struct rte_idxd_user_hdl *hdl_ring;
	struct rte_idxd_desc_batch *batch_ring;
};

/*
 * Enqueue a copy operation onto the ioat device
 */
static inline int
rte_ioat_enqueue_copy(int dev_id, phys_addr_t src, phys_addr_t dst,
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
		ioat->enqueue_failed++;
		return 0;
	}

	ioat->next_write = write + 1;
	write &= mask;

	desc = &ioat->desc_ring[write];
	desc->size = length;
	/* set descriptor write-back every 16th descriptor */
	desc->u.control_raw = (uint32_t)((!(write & 0xF)) << 3);
	desc->src_addr = src;
	desc->dest_addr = dst;

	if (!ioat->hdls_disable)
		ioat->hdls[write] = _mm_set_epi64x((int64_t)dst_hdl,
					(int64_t)src_hdl);
	rte_prefetch0(&ioat->desc_ring[ioat->next_write & mask]);

	ioat->enqueued++;
	return 1;
}

/* add fence to last written descriptor */
static inline int
rte_ioat_fence(int dev_id)
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
static inline void
rte_ioat_perform_ops(int dev_id)
{
	struct rte_ioat_rawdev *ioat =
			(struct rte_ioat_rawdev *)rte_rawdevs[dev_id].dev_private;
	ioat->desc_ring[(ioat->next_write - 1) & (ioat->ring_size - 1)].u
			.control.completion_update = 1;
	rte_compiler_barrier();
	*ioat->doorbell = ioat->next_write;
	ioat->started = ioat->enqueued;
}

/**
 * @internal
 * Returns the index of the last completed operation.
 */
static inline int
rte_ioat_get_last_completed(struct rte_ioat_rawdev *ioat, int *error)
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
static inline int
rte_ioat_completed_ops(int dev_id, uint8_t max_copies,
		uintptr_t *src_hdls, uintptr_t *dst_hdls)
{
	struct rte_ioat_rawdev *ioat =
			(struct rte_ioat_rawdev *)rte_rawdevs[dev_id].dev_private;
	unsigned short mask = (ioat->ring_size - 1);
	unsigned short read = ioat->next_read;
	unsigned short end_read, count;
	int error;
	int i = 0;

	end_read = (rte_ioat_get_last_completed(ioat, &error) + 1) & mask;
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
	ioat->completed += count;
	return count;
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
