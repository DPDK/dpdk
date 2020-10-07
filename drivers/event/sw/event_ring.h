/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

/*
 * Generic ring structure for passing events from one core to another.
 *
 * Used by the software scheduler for the producer and consumer rings for
 * each port, i.e. for passing events from worker cores to scheduler and
 * vice-versa. Designed for single-producer, single-consumer use with two
 * cores working on each ring.
 */

#ifndef _EVENT_RING_
#define _EVENT_RING_

#include <stdint.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_malloc.h>

/* Custom single threaded ring implementation used for ROB */
struct rob_ring {
	uint32_t ring_size;
	uint32_t mask;
	uint32_t size;
	uint32_t write_idx;
	uint32_t read_idx;
	void *ring[0] __rte_cache_aligned;
};

static inline struct rob_ring *
rob_ring_create(unsigned int size, unsigned int socket_id)
{
	struct rob_ring *retval;
	const uint32_t ring_size = rte_align32pow2(size + 1);
	size_t memsize = sizeof(*retval) +
			(ring_size * sizeof(retval->ring[0]));

	retval = rte_zmalloc_socket(NULL, memsize, 0, socket_id);
	if (retval == NULL)
		goto end;
	retval->ring_size = ring_size;
	retval->mask = ring_size - 1;
	retval->size = size;
end:
	return retval;
}

static inline void
rob_ring_free(struct rob_ring *r)
{
	rte_free(r);
}

static __rte_always_inline unsigned int
rob_ring_count(const struct rob_ring *r)
{
	return r->write_idx - r->read_idx;
}

static __rte_always_inline unsigned int
rob_ring_free_count(const struct rob_ring *r)
{
	return r->size - rob_ring_count(r);
}

static __rte_always_inline unsigned int
rob_ring_enqueue(struct rob_ring *r, void *re)
{
	const uint32_t size = r->size;
	const uint32_t mask = r->mask;
	const uint32_t read = r->read_idx;
	uint32_t write = r->write_idx;
	const uint32_t space = read + size - write;
	if (space < 1)
		return 0;
	r->ring[write & mask] = re;
	r->write_idx++;
	return 1;
}

static __rte_always_inline unsigned int
rob_ring_dequeue(struct rob_ring *r, void **re)
{
	const uint32_t mask = r->mask;
	uint32_t read = r->read_idx;
	const uint32_t write = r->write_idx;
	const uint32_t items = write - read;
	if (items < 1)
		return 0;
	*re = r->ring[read & mask];
	r->read_idx++;
	return 1;
}

#endif
