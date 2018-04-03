/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

/**
 * @file
 * RTE Event Ring
 *
 * This provides a ring implementation for passing rte_event structures
 * from one core to another.
 */

#ifndef _RTE_EVENT_RING_
#define _RTE_EVENT_RING_

#include <stdint.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include "rte_eventdev.h"

#define RTE_TAILQ_EVENT_RING_NAME "RTE_EVENT_RING"

/**
 * Generic ring structure for passing rte_event objects from core to core.
 *
 * Based on the primitives given in the rte_ring library. Designed to be
 * used inside software eventdev implementations and by applications
 * directly as needed.
 */
struct rte_event_ring {
	struct rte_ring r;
};

/**
 * Returns the number of events in the ring
 *
 * @param r
 *   pointer to the event ring
 * @return
 *   the number of events in the ring
 */
static __rte_always_inline unsigned int
rte_event_ring_count(const struct rte_event_ring *r)
{
	return rte_ring_count(&r->r);
}

/**
 * Returns the amount of free space in the ring
 *
 * @param r
 *   pointer to the event ring
 * @return
 *   the number of free slots in the ring, i.e. the number of events that
 *   can be successfully enqueued before dequeue must be called
 */
static __rte_always_inline unsigned int
rte_event_ring_free_count(const struct rte_event_ring *r)
{
	return rte_ring_free_count(&r->r);
}

/**
 * Enqueue a set of events onto a ring
 *
 * Note: this API enqueues by copying the events themselves onto the ring,
 * rather than just placing a pointer to each event onto the ring. This
 * means that statically-allocated events can safely be enqueued by this
 * API.
 *
 * @param r
 *   pointer to the event ring
 * @param events
 *   pointer to an array of struct rte_event objects
 * @param n
 *   number of events in the array to enqueue
 * @param free_space
 *   if non-null, is updated to indicate the amount of free space in the
 *   ring once the enqueue has completed.
 * @return
 *   the number of elements, n', enqueued to the ring, 0 <= n' <= n
 */
static __rte_always_inline unsigned int
rte_event_ring_enqueue_burst(struct rte_event_ring *r,
		const struct rte_event *events,
		unsigned int n, uint16_t *free_space)
{
	uint32_t prod_head, prod_next;
	uint32_t free_entries;

	n = __rte_ring_move_prod_head(&r->r, r->r.prod.single, n,
			RTE_RING_QUEUE_VARIABLE,
			&prod_head, &prod_next, &free_entries);
	if (n == 0)
		goto end;

	ENQUEUE_PTRS(&r->r, &r[1], prod_head, events, n, struct rte_event);

	update_tail(&r->r.prod, prod_head, prod_next, r->r.prod.single, 1);
end:
	if (free_space != NULL)
		*free_space = free_entries - n;
	return n;
}

/**
 * Dequeue a set of events from a ring
 *
 * Note: this API does not work with pointers to events, rather it copies
 * the events themselves to the destination ``events`` buffer.
 *
 * @param r
 *   pointer to the event ring
 * @param events
 *   pointer to an array to hold the struct rte_event objects
 * @param n
 *   number of events that can be held in the ``events`` array
 * @param available
 *   if non-null, is updated to indicate the number of events remaining in
 *   the ring once the dequeue has completed
 * @return
 *   the number of elements, n', dequeued from the ring, 0 <= n' <= n
 */
static __rte_always_inline unsigned int
rte_event_ring_dequeue_burst(struct rte_event_ring *r,
		struct rte_event *events,
		unsigned int n, uint16_t *available)
{
	uint32_t cons_head, cons_next;
	uint32_t entries;

	n = __rte_ring_move_cons_head(&r->r, r->r.cons.single, n,
			RTE_RING_QUEUE_VARIABLE,
			&cons_head, &cons_next, &entries);
	if (n == 0)
		goto end;

	DEQUEUE_PTRS(&r->r, &r[1], cons_head, events, n, struct rte_event);

	update_tail(&r->r.cons, cons_head, cons_next, r->r.cons.single, 0);

end:
	if (available != NULL)
		*available = entries - n;
	return n;
}

/*
 * Initializes an already-allocated ring structure
 *
 * @param r
 *   pointer to the ring memory to be initialized
 * @param name
 *   name to be given to the ring
 * @param count
 *   the number of elements to be stored in the ring. If the flag
 *   ``RING_F_EXACT_SZ`` is not set, this must be a power of 2, and the actual
 *   usable space in the ring will be ``count - 1`` entries. If the flag
 *   ``RING_F_EXACT_SZ`` is set, the this can be any value up to the ring size
 *   limit - 1, and the usable space will be exactly that requested.
 * @param flags
 *   An OR of the following:
 *    - RING_F_SP_ENQ: If this flag is set, the default behavior when
 *      using ``rte_ring_enqueue()`` or ``rte_ring_enqueue_bulk()``
 *      is "single-producer". Otherwise, it is "multi-producers".
 *    - RING_F_SC_DEQ: If this flag is set, the default behavior when
 *      using ``rte_ring_dequeue()`` or ``rte_ring_dequeue_bulk()``
 *      is "single-consumer". Otherwise, it is "multi-consumers".
 *    - RING_F_EXACT_SZ: If this flag is set, the ``count`` parameter is to
 *      be taken as the exact usable size of the ring, and as such does not
 *      need to be a power of 2. The underlying ring memory should be a
 *      power-of-2 size greater than the count value.
 * @return
 *   0 on success, or a negative value on error.
 */
int
rte_event_ring_init(struct rte_event_ring *r, const char *name,
	unsigned int count, unsigned int flags);

/*
 * Create an event ring structure
 *
 * This function allocates memory and initializes an event ring inside that
 * memory.
 *
 * @param name
 *   name to be given to the ring
 * @param count
 *   the number of elements to be stored in the ring. If the flag
 *   ``RING_F_EXACT_SZ`` is not set, this must be a power of 2, and the actual
 *   usable space in the ring will be ``count - 1`` entries. If the flag
 *   ``RING_F_EXACT_SZ`` is set, the this can be any value up to the ring size
 *   limit - 1, and the usable space will be exactly that requested.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in case of
 *   NUMA. The value can be *SOCKET_ID_ANY* if there is no NUMA
 *   constraint for the reserved zone.
 * @param flags
 *   An OR of the following:
 *    - RING_F_SP_ENQ: If this flag is set, the default behavior when
 *      using ``rte_ring_enqueue()`` or ``rte_ring_enqueue_bulk()``
 *      is "single-producer". Otherwise, it is "multi-producers".
 *    - RING_F_SC_DEQ: If this flag is set, the default behavior when
 *      using ``rte_ring_dequeue()`` or ``rte_ring_dequeue_bulk()``
 *      is "single-consumer". Otherwise, it is "multi-consumers".
 *    - RING_F_EXACT_SZ: If this flag is set, the ``count`` parameter is to
 *      be taken as the exact usable size of the ring, and as such does not
 *      need to be a power of 2. The underlying ring memory should be a
 *      power-of-2 size greater than the count value.
 * @return
 *   On success, the pointer to the new allocated ring. NULL on error with
 *    rte_errno set appropriately. Possible errno values include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - EINVAL - count provided is not a power of 2
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_event_ring *
rte_event_ring_create(const char *name, unsigned int count, int socket_id,
		unsigned int flags);

/**
 * Search for an event ring based on its name
 *
 * @param name
 *   The name of the ring.
 * @return
 *   The pointer to the ring matching the name, or NULL if not found,
 *   with rte_errno set appropriately. Possible rte_errno values include:
 *    - ENOENT - required entry not available to return.
 */
struct rte_event_ring *
rte_event_ring_lookup(const char *name);

/**
 * De-allocate all memory used by the ring.
 *
 * @param r
 *   Ring to free
 */
void
rte_event_ring_free(struct rte_event_ring *r);

/**
 * Return the size of the event ring.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   The size of the data store used by the ring.
 *   NOTE: this is not the same as the usable space in the ring. To query that
 *   use ``rte_ring_get_capacity()``.
 */
static inline unsigned int
rte_event_ring_get_size(const struct rte_event_ring *r)
{
	return rte_ring_get_size(&r->r);
}

/**
 * Return the number of elements which can be stored in the event ring.
 *
 * @param r
 *   A pointer to the ring structure.
 * @return
 *   The usable size of the ring.
 */
static inline unsigned int
rte_event_ring_get_capacity(const struct rte_event_ring *r)
{
	return rte_ring_get_capacity(&r->r);
}
#endif
