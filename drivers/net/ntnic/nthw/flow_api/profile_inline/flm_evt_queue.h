/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#ifndef _FLM_EVT_QUEUE_H_
#define _FLM_EVT_QUEUE_H_

#include "stdint.h"
#include "stdbool.h"

struct flm_status_event_s {
	void *flow;
	uint32_t learn_ignore : 1;
	uint32_t learn_failed : 1;
	uint32_t learn_done : 1;
};

struct flm_info_event_s {
	uint64_t bytes;
	uint64_t packets;
	uint64_t timestamp;
	uint64_t id;
	uint8_t cause;
};

enum {
	FLM_INFO_LOCAL,
	FLM_INFO_REMOTE,
	FLM_STAT_LOCAL,
	FLM_STAT_REMOTE,
};

/* Max number of local queues */
#define MAX_INFO_LCL_QUEUES 8
#define MAX_STAT_LCL_QUEUES 8

/* Max number of remote queues */
#define MAX_INFO_RMT_QUEUES 128
#define MAX_STAT_RMT_QUEUES 128

/* queue size */
#define FLM_EVT_QUEUE_SIZE 8192
#define FLM_STAT_QUEUE_SIZE 8192

/* Event element size */
#define FLM_EVT_ELEM_SIZE sizeof(struct flm_info_event_s)
#define FLM_STAT_ELEM_SIZE sizeof(struct flm_status_event_s)

void nthw_flm_inf_sta_queue_free_all(uint8_t caller);
void nthw_flm_inf_queue_put(uint8_t port, bool remote, struct flm_info_event_s *obj);
int nthw_flm_inf_queue_get(uint8_t port, bool remote, struct flm_info_event_s *obj);
int nthw_flm_sta_queue_put(uint8_t port, bool remote, struct flm_status_event_s *obj);

#endif	/* _FLM_EVT_QUEUE_H_ */
