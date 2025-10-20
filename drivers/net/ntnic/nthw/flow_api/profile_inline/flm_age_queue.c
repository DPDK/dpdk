/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#include <rte_ring.h>
#include <rte_errno.h>

#include "ntlog.h"
#include "flm_age_queue.h"

/* Queues for flm aged events */
static struct rte_ring *age_queue[MAX_EVT_AGE_QUEUES];
static RTE_ATOMIC(uint16_t) age_event[MAX_EVT_AGE_PORTS];

__rte_always_inline int nthw_flm_age_event_get(uint8_t port)
{
	return  rte_atomic_load_explicit(&age_event[port], rte_memory_order_seq_cst);
}

__rte_always_inline void nthw_flm_age_event_set(uint8_t port)
{
	rte_atomic_store_explicit(&age_event[port], 1, rte_memory_order_seq_cst);
}

__rte_always_inline void nthw_flm_age_event_clear(uint8_t port)
{
	rte_atomic_store_explicit(&age_event[port], 0, rte_memory_order_seq_cst);
}

void nthw_flm_age_queue_free(uint8_t port, uint16_t caller_id)
{
	struct rte_ring *q = NULL;

	if (port < MAX_EVT_AGE_PORTS)
		rte_atomic_store_explicit(&age_event[port], 0, rte_memory_order_seq_cst);

	if (caller_id < MAX_EVT_AGE_QUEUES && age_queue[caller_id] != NULL) {
		q = age_queue[caller_id];
		age_queue[caller_id] = NULL;
	}

	rte_ring_free(q);
}

void nthw_flm_age_queue_free_all(void)
{
	int i;
	int j;

	for (i = 0; i < MAX_EVT_AGE_PORTS; i++)
		for (j = 0; j < MAX_EVT_AGE_QUEUES; j++)
			nthw_flm_age_queue_free(i, j);
}

struct rte_ring *nthw_flm_age_queue_create(uint8_t port, uint16_t caller_id, unsigned int count)
{
	char name[20];
	struct rte_ring *q = NULL;

	if (rte_is_power_of_2(count) == false || count > RTE_RING_SZ_MASK) {
		NT_LOG(WRN,
			FILTER,
			"FLM aged event queue number of elements (%u) is invalid, must be power of 2, and not exceed %u",
			count,
			RTE_RING_SZ_MASK);
		return NULL;
	}

	if (port >= MAX_EVT_AGE_PORTS) {
		NT_LOG(WRN,
			FILTER,
			"FLM aged event queue cannot be created for port %u. Max supported port is %u",
			port,
			MAX_EVT_AGE_PORTS - 1);
		return NULL;
	}

	rte_atomic_store_explicit(&age_event[port], 0, rte_memory_order_seq_cst);

	if (caller_id >= MAX_EVT_AGE_QUEUES) {
		NT_LOG(WRN,
			FILTER,
			"FLM aged event queue cannot be created for caller_id %u. Max supported caller_id is %u",
			caller_id,
			MAX_EVT_AGE_QUEUES - 1);
		return NULL;
	}

	if (age_queue[caller_id] != NULL) {
		NT_LOG(DBG, FILTER, "FLM aged event queue %u already created", caller_id);
		return age_queue[caller_id];
	}

	snprintf(name, 20, "AGE_EVENT%u", caller_id);
	q = rte_ring_create_elem(name,
			FLM_AGE_ELEM_SIZE,
			count,
			SOCKET_ID_ANY,
			RING_F_SP_ENQ | RING_F_SC_DEQ);

	if (q == NULL) {
		NT_LOG(WRN,
			FILTER,
			"FLM aged event queue cannot be created due to error %02X",
			rte_errno);
		return NULL;
	}

	age_queue[caller_id] = q;

	return q;
}

void nthw_flm_age_queue_put(uint16_t caller_id, struct flm_age_event_s *obj)
{
	int ret;

	/* If queues is not created, then ignore and return */
	if (caller_id < MAX_EVT_AGE_QUEUES && age_queue[caller_id] != NULL) {
		ret = rte_ring_sp_enqueue_elem(age_queue[caller_id], obj, FLM_AGE_ELEM_SIZE);

		if (ret != 0)
			NT_LOG(DBG, FILTER, "FLM aged event queue full");
	}
}

int nthw_flm_age_queue_get(uint16_t caller_id, struct flm_age_event_s *obj)
{
	int ret;

	/* If queues is not created, then ignore and return */
	if (caller_id < MAX_EVT_AGE_QUEUES && age_queue[caller_id] != NULL) {
		ret = rte_ring_sc_dequeue_elem(age_queue[caller_id], obj, FLM_AGE_ELEM_SIZE);

		if (ret != 0)
			NT_LOG(DBG, FILTER, "FLM aged event queue empty");

		return ret;
	}

	return -ENOENT;
}

unsigned int nthw_flm_age_queue_count(uint16_t caller_id)
{
	unsigned int ret = 0;

	if (caller_id < MAX_EVT_AGE_QUEUES && age_queue[caller_id] != NULL)
		ret = rte_ring_count(age_queue[caller_id]);

	return ret;
}

unsigned int nthw_flm_age_queue_get_size(uint16_t caller_id)
{
	unsigned int ret = 0;

	if (caller_id < MAX_EVT_AGE_QUEUES && age_queue[caller_id] != NULL)
		ret = rte_ring_get_size(age_queue[caller_id]);

	return ret;
}
