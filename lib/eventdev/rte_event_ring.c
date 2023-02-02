/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 * Copyright(c) 2019 Arm Limited
 */


#include "rte_event_ring.h"
#include "eventdev_trace.h"

int
rte_event_ring_init(struct rte_event_ring *r, const char *name,
	unsigned int count, unsigned int flags)
{
	/* compilation-time checks */
	RTE_BUILD_BUG_ON((sizeof(struct rte_event_ring) &
			  RTE_CACHE_LINE_MASK) != 0);

	rte_eventdev_trace_ring_init(r, name, count, flags);

	/* init the ring structure */
	return rte_ring_init(&r->r, name, count, flags);
}

/* create the ring */
struct rte_event_ring *
rte_event_ring_create(const char *name, unsigned int count, int socket_id,
		unsigned int flags)
{
	rte_eventdev_trace_ring_create(name, count, socket_id, flags);

	return (struct rte_event_ring *)rte_ring_create_elem(name,
						sizeof(struct rte_event),
						count, socket_id, flags);
}


struct rte_event_ring *
rte_event_ring_lookup(const char *name)
{
	rte_eventdev_trace_ring_lookup(name);

	return (struct rte_event_ring *)rte_ring_lookup(name);
}

/* free the ring */
void
rte_event_ring_free(struct rte_event_ring *r)
{
	rte_eventdev_trace_ring_free(r->r.name);

	rte_ring_free((struct rte_ring *)r);
}
