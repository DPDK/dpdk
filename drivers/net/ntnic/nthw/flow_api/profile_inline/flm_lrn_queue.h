/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#ifndef _FLM_LRN_QUEUE_H_
#define _FLM_LRN_QUEUE_H_

#include <stdint.h>

typedef struct read_record {
	uint32_t *p;
	uint32_t num;
} read_record;

void *nthw_flm_lrn_queue_create(void);
void nthw_flm_lrn_queue_free(void *q);

uint32_t *nthw_flm_lrn_queue_get_write_buffer(void *q);
void nthw_flm_lrn_queue_release_write_buffer(void *q);

read_record nthw_flm_lrn_queue_get_read_buffer(void *q);
void nthw_flm_lrn_queue_release_read_buffer(void *q, uint32_t num);

#endif	/* _FLM_LRN_QUEUE_H_ */
