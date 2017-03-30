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
 * Ring structure definitions used for the internal ring buffers of the
 * SW eventdev implementation. These are designed for single-core use only.
 */
#ifndef _IQ_RING_
#define _IQ_RING_

#include <stdint.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_eventdev.h>

#define IQ_RING_NAMESIZE 12
#define QID_IQ_DEPTH 512
#define QID_IQ_MASK (uint16_t)(QID_IQ_DEPTH - 1)

struct iq_ring {
	char name[IQ_RING_NAMESIZE] __rte_cache_aligned;
	uint16_t write_idx;
	uint16_t read_idx;

	struct rte_event ring[QID_IQ_DEPTH];
};

#ifndef force_inline
#define force_inline inline __attribute__((always_inline))
#endif

static inline struct iq_ring *
iq_ring_create(const char *name, unsigned int socket_id)
{
	struct iq_ring *retval;

	retval = rte_malloc_socket(NULL, sizeof(*retval), 0, socket_id);
	if (retval == NULL)
		goto end;

	snprintf(retval->name, sizeof(retval->name), "%s", name);
	retval->write_idx = retval->read_idx = 0;
end:
	return retval;
}

static inline void
iq_ring_destroy(struct iq_ring *r)
{
	rte_free(r);
}

static force_inline uint16_t
iq_ring_count(const struct iq_ring *r)
{
	return r->write_idx - r->read_idx;
}

static force_inline uint16_t
iq_ring_free_count(const struct iq_ring *r)
{
	return QID_IQ_MASK - iq_ring_count(r);
}

static force_inline uint16_t
iq_ring_enqueue_burst(struct iq_ring *r, struct rte_event *qes, uint16_t nb_qes)
{
	const uint16_t read = r->read_idx;
	uint16_t write = r->write_idx;
	const uint16_t space = read + QID_IQ_MASK - write;
	uint16_t i;

	if (space < nb_qes)
		nb_qes = space;

	for (i = 0; i < nb_qes; i++, write++)
		r->ring[write & QID_IQ_MASK] = qes[i];

	r->write_idx = write;

	return nb_qes;
}

static force_inline uint16_t
iq_ring_dequeue_burst(struct iq_ring *r, struct rte_event *qes, uint16_t nb_qes)
{
	uint16_t read = r->read_idx;
	const uint16_t write = r->write_idx;
	const uint16_t items = write - read;
	uint16_t i;

	for (i = 0; i < nb_qes; i++, read++)
		qes[i] = r->ring[read & QID_IQ_MASK];

	if (items < nb_qes)
		nb_qes = items;

	r->read_idx += nb_qes;

	return nb_qes;
}

/* assumes there is space, from a previous dequeue_burst */
static force_inline uint16_t
iq_ring_put_back(struct iq_ring *r, struct rte_event *qes, uint16_t nb_qes)
{
	uint16_t i, read = r->read_idx;

	for (i = nb_qes; i-- > 0; )
		r->ring[--read & QID_IQ_MASK] = qes[i];

	r->read_idx = read;
	return nb_qes;
}

static force_inline const struct rte_event *
iq_ring_peek(const struct iq_ring *r)
{
	return &r->ring[r->read_idx & QID_IQ_MASK];
}

static force_inline void
iq_ring_pop(struct iq_ring *r)
{
	r->read_idx++;
}

static force_inline int
iq_ring_enqueue(struct iq_ring *r, const struct rte_event *qe)
{
	const uint16_t read = r->read_idx;
	const uint16_t write = r->write_idx;
	const uint16_t space = read + QID_IQ_MASK - write;

	if (space == 0)
		return -1;

	r->ring[write & QID_IQ_MASK] = *qe;

	r->write_idx = write + 1;

	return 0;
}

#endif
