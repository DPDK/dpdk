/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#ifndef _FLM_AGE_QUEUE_H_
#define _FLM_AGE_QUEUE_H_

#include "stdint.h"

struct flm_age_event_s {
	void *context;
};

/* Max number of event queues */
#define MAX_EVT_AGE_QUEUES 256

#define FLM_AGE_ELEM_SIZE sizeof(struct flm_age_event_s)

int flm_age_queue_get(uint16_t caller_id, struct flm_age_event_s *obj);
unsigned int flm_age_queue_count(uint16_t caller_id);
unsigned int flm_age_queue_get_size(uint16_t caller_id);

#endif	/* _FLM_AGE_QUEUE_H_ */
