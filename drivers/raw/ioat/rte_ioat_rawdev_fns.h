/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */
#ifndef _RTE_IOAT_RAWDEV_FNS_H_
#define _RTE_IOAT_RAWDEV_FNS_H_

#include <x86intrin.h>
#include <rte_rawdev.h>
#include <rte_memzone.h>
#include <rte_prefetch.h>
#include "rte_ioat_spec.h"

/**
 * @internal
 * Structure representing a device instance
 */
struct rte_ioat_rawdev {
	struct rte_rawdev *rawdev;
	const struct rte_memzone *mz;
	const struct rte_memzone *desc_mz;

	volatile struct rte_ioat_registers *regs;
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
};

/*
 * Enqueue a copy operation onto the ioat device
 */
static inline int
rte_ioat_enqueue_copy(int dev_id, phys_addr_t src, phys_addr_t dst,
		unsigned int length, uintptr_t src_hdl, uintptr_t dst_hdl,
		int fence)
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
	desc->u.control_raw = (uint32_t)((!!fence << 4) | (!(write & 0xF)) << 3);
	desc->src_addr = src;
	desc->dest_addr = dst;

	if (!ioat->hdls_disable)
		ioat->hdls[write] = _mm_set_epi64x((int64_t)dst_hdl,
					(int64_t)src_hdl);
	rte_prefetch0(&ioat->desc_ring[ioat->next_write & mask]);

	ioat->enqueued++;
	return 1;
}

/*
 * Trigger hardware to begin performing enqueued copy operations
 */
static inline void
rte_ioat_do_copies(int dev_id)
{
	struct rte_ioat_rawdev *ioat =
			(struct rte_ioat_rawdev *)rte_rawdevs[dev_id].dev_private;
	ioat->desc_ring[(ioat->next_write - 1) & (ioat->ring_size - 1)].u
			.control.completion_update = 1;
	rte_compiler_barrier();
	ioat->regs->dmacount = ioat->next_write;
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
 * Returns details of copy operations that have been completed
 */
static inline int
rte_ioat_completed_copies(int dev_id, uint8_t max_copies,
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

#endif /* _RTE_IOAT_RAWDEV_FNS_H_ */
