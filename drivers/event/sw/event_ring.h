/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#define QE_RING_NAMESIZE 32

struct qe_ring {
	char name[QE_RING_NAMESIZE] __rte_cache_aligned;
	uint32_t ring_size; /* size of memory block allocated to the ring */
	uint32_t mask;      /* mask for read/write values == ring_size -1 */
	uint32_t size;      /* actual usable space in the ring */
	volatile uint32_t write_idx __rte_cache_aligned;
	volatile uint32_t read_idx __rte_cache_aligned;

	struct rte_event ring[0] __rte_cache_aligned;
};

static inline struct qe_ring *
qe_ring_create(const char *name, unsigned int size, unsigned int socket_id)
{
	struct qe_ring *retval;
	const uint32_t ring_size = rte_align32pow2(size + 1);
	size_t memsize = sizeof(*retval) +
			(ring_size * sizeof(retval->ring[0]));

	retval = rte_zmalloc_socket(NULL, memsize, 0, socket_id);
	if (retval == NULL)
		goto end;

	snprintf(retval->name, sizeof(retval->name), "EVDEV_RG_%s", name);
	retval->ring_size = ring_size;
	retval->mask = ring_size - 1;
	retval->size = size;
end:
	return retval;
}

static inline void
qe_ring_destroy(struct qe_ring *r)
{
	rte_free(r);
}

static __rte_always_inline unsigned int
qe_ring_count(const struct qe_ring *r)
{
	return r->write_idx - r->read_idx;
}

static __rte_always_inline unsigned int
qe_ring_free_count(const struct qe_ring *r)
{
	return r->size - qe_ring_count(r);
}

static __rte_always_inline unsigned int
qe_ring_enqueue_burst(struct qe_ring *r, const struct rte_event *qes,
		unsigned int nb_qes, uint16_t *free_count)
{
	const uint32_t size = r->size;
	const uint32_t mask = r->mask;
	const uint32_t read = r->read_idx;
	uint32_t write = r->write_idx;
	const uint32_t space = read + size - write;
	uint32_t i;

	if (space < nb_qes)
		nb_qes = space;

	for (i = 0; i < nb_qes; i++, write++)
		r->ring[write & mask] = qes[i];

	rte_smp_wmb();

	if (nb_qes != 0)
		r->write_idx = write;

	*free_count = space - nb_qes;

	return nb_qes;
}

static __rte_always_inline unsigned int
qe_ring_enqueue_burst_with_ops(struct qe_ring *r, const struct rte_event *qes,
		unsigned int nb_qes, uint8_t *ops)
{
	const uint32_t size = r->size;
	const uint32_t mask = r->mask;
	const uint32_t read = r->read_idx;
	uint32_t write = r->write_idx;
	const uint32_t space = read + size - write;
	uint32_t i;

	if (space < nb_qes)
		nb_qes = space;

	for (i = 0; i < nb_qes; i++, write++) {
		r->ring[write & mask] = qes[i];
		r->ring[write & mask].op = ops[i];
	}

	rte_smp_wmb();

	if (nb_qes != 0)
		r->write_idx = write;

	return nb_qes;
}

static __rte_always_inline unsigned int
qe_ring_dequeue_burst(struct qe_ring *r, struct rte_event *qes,
		unsigned int nb_qes)
{
	const uint32_t mask = r->mask;
	uint32_t read = r->read_idx;
	const uint32_t write = r->write_idx;
	const uint32_t items = write - read;
	uint32_t i;

	if (items < nb_qes)
		nb_qes = items;


	for (i = 0; i < nb_qes; i++, read++)
		qes[i] = r->ring[read & mask];

	rte_smp_rmb();

	if (nb_qes != 0)
		r->read_idx += nb_qes;

	return nb_qes;
}

#endif
