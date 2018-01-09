/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _IQ_CHUNK_H_
#define _IQ_CHUNK_H_

#include <stdint.h>
#include <stdbool.h>
#include <rte_eventdev.h>

#define IQ_ROB_NAMESIZE 12

struct sw_queue_chunk {
	struct rte_event events[SW_EVS_PER_Q_CHUNK];
	struct sw_queue_chunk *next;
} __rte_cache_aligned;

static __rte_always_inline bool
iq_empty(struct sw_iq *iq)
{
	return (iq->count == 0);
}

static __rte_always_inline uint16_t
iq_count(const struct sw_iq *iq)
{
	return iq->count;
}

static __rte_always_inline struct sw_queue_chunk *
iq_alloc_chunk(struct sw_evdev *sw)
{
	struct sw_queue_chunk *chunk = sw->chunk_list_head;
	sw->chunk_list_head = chunk->next;
	chunk->next = NULL;
	return chunk;
}

static __rte_always_inline void
iq_free_chunk(struct sw_evdev *sw, struct sw_queue_chunk *chunk)
{
	chunk->next = sw->chunk_list_head;
	sw->chunk_list_head = chunk;
}

static __rte_always_inline void
iq_free_chunk_list(struct sw_evdev *sw, struct sw_queue_chunk *head)
{
	while (head) {
		struct sw_queue_chunk *next;
		next = head->next;
		iq_free_chunk(sw, head);
		head = next;
	}
}

static __rte_always_inline void
iq_init(struct sw_evdev *sw, struct sw_iq *iq)
{
	iq->head = iq_alloc_chunk(sw);
	iq->tail = iq->head;
	iq->head_idx = 0;
	iq->tail_idx = 0;
	iq->count = 0;
}

static __rte_always_inline void
iq_enqueue(struct sw_evdev *sw, struct sw_iq *iq, const struct rte_event *ev)
{
	iq->tail->events[iq->tail_idx++] = *ev;
	iq->count++;

	if (unlikely(iq->tail_idx == SW_EVS_PER_Q_CHUNK)) {
		/* The number of chunks is defined in relation to the total
		 * number of inflight events and number of IQS such that
		 * allocation will always succeed.
		 */
		struct sw_queue_chunk *chunk = iq_alloc_chunk(sw);
		iq->tail->next = chunk;
		iq->tail = chunk;
		iq->tail_idx = 0;
	}
}

static __rte_always_inline void
iq_pop(struct sw_evdev *sw, struct sw_iq *iq)
{
	iq->head_idx++;
	iq->count--;

	if (unlikely(iq->head_idx == SW_EVS_PER_Q_CHUNK)) {
		struct sw_queue_chunk *next = iq->head->next;
		iq_free_chunk(sw, iq->head);
		iq->head = next;
		iq->head_idx = 0;
	}
}

static __rte_always_inline const struct rte_event *
iq_peek(struct sw_iq *iq)
{
	return &iq->head->events[iq->head_idx];
}

/* Note: the caller must ensure that count <= iq_count() */
static __rte_always_inline uint16_t
iq_dequeue_burst(struct sw_evdev *sw,
		 struct sw_iq *iq,
		 struct rte_event *ev,
		 uint16_t count)
{
	struct sw_queue_chunk *current;
	uint16_t total, index;

	count = RTE_MIN(count, iq_count(iq));

	current = iq->head;
	index = iq->head_idx;
	total = 0;

	/* Loop over the chunks */
	while (1) {
		struct sw_queue_chunk *next;
		for (; index < SW_EVS_PER_Q_CHUNK;) {
			ev[total++] = current->events[index++];

			if (unlikely(total == count))
				goto done;
		}

		/* Move to the next chunk */
		next = current->next;
		iq_free_chunk(sw, current);
		current = next;
		index = 0;
	}

done:
	if (unlikely(index == SW_EVS_PER_Q_CHUNK)) {
		struct sw_queue_chunk *next = current->next;
		iq_free_chunk(sw, current);
		iq->head = next;
		iq->head_idx = 0;
	} else {
		iq->head = current;
		iq->head_idx = index;
	}

	iq->count -= total;

	return total;
}

static __rte_always_inline void
iq_put_back(struct sw_evdev *sw,
	    struct sw_iq *iq,
	    struct rte_event *ev,
	    unsigned int count)
{
	/* Put back events that fit in the current head chunk. If necessary,
	 * put back events in a new head chunk. The caller must ensure that
	 * count <= SW_EVS_PER_Q_CHUNK, to ensure that at most one new head is
	 * needed.
	 */
	uint16_t avail_space = iq->head_idx;

	if (avail_space >= count) {
		const uint16_t idx = avail_space - count;
		uint16_t i;

		for (i = 0; i < count; i++)
			iq->head->events[idx + i] = ev[i];

		iq->head_idx = idx;
	} else if (avail_space < count) {
		const uint16_t remaining = count - avail_space;
		struct sw_queue_chunk *new_head;
		uint16_t i;

		for (i = 0; i < avail_space; i++)
			iq->head->events[i] = ev[remaining + i];

		new_head = iq_alloc_chunk(sw);
		new_head->next = iq->head;
		iq->head = new_head;
		iq->head_idx = SW_EVS_PER_Q_CHUNK - remaining;

		for (i = 0; i < remaining; i++)
			iq->head->events[iq->head_idx + i] = ev[i];
	}

	iq->count += count;
}

#endif /* _IQ_CHUNK_H_ */
