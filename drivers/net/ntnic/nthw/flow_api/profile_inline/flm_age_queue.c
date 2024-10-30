/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#include <rte_ring.h>

#include "ntlog.h"
#include "flm_age_queue.h"

/* Queues for flm aged events */
static struct rte_ring *age_queue[MAX_EVT_AGE_QUEUES];

int flm_age_queue_get(uint16_t caller_id, struct flm_age_event_s *obj)
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

unsigned int flm_age_queue_count(uint16_t caller_id)
{
	unsigned int ret = 0;

	if (caller_id < MAX_EVT_AGE_QUEUES && age_queue[caller_id] != NULL)
		ret = rte_ring_count(age_queue[caller_id]);

	return ret;
}

unsigned int flm_age_queue_get_size(uint16_t caller_id)
{
	unsigned int ret = 0;

	if (caller_id < MAX_EVT_AGE_QUEUES && age_queue[caller_id] != NULL)
		ret = rte_ring_get_size(age_queue[caller_id]);

	return ret;
}
