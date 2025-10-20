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

/* Indicates why the flow info record was generated */
#define INF_DATA_CAUSE_SW_UNLEARN 0
#define INF_DATA_CAUSE_TIMEOUT_FLOW_DELETED 1
#define INF_DATA_CAUSE_NA 2
#define INF_DATA_CAUSE_PERIODIC_FLOW_INFO 3
#define INF_DATA_CAUSE_SW_PROBE 4
#define INF_DATA_CAUSE_TIMEOUT_FLOW_KEPT 5

/* Max number of event queues */
#define MAX_EVT_AGE_QUEUES 256

/* Max number of event ports */
#define MAX_EVT_AGE_PORTS 128

#define FLM_AGE_ELEM_SIZE sizeof(struct flm_age_event_s)

int nthw_flm_age_event_get(uint8_t port);
void nthw_flm_age_event_set(uint8_t port);
void nthw_flm_age_event_clear(uint8_t port);
void nthw_flm_age_queue_free(uint8_t port, uint16_t caller_id);
void nthw_flm_age_queue_free_all(void);
struct rte_ring *nthw_flm_age_queue_create(uint8_t port, uint16_t caller_id, unsigned int count);
void nthw_flm_age_queue_put(uint16_t caller_id, struct flm_age_event_s *obj);
int nthw_flm_age_queue_get(uint16_t caller_id, struct flm_age_event_s *obj);
unsigned int nthw_flm_age_queue_count(uint16_t caller_id);
unsigned int nthw_flm_age_queue_get_size(uint16_t caller_id);

#endif	/* _FLM_AGE_QUEUE_H_ */
