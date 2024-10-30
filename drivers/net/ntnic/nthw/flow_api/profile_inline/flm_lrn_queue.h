/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#ifndef _FLM_LRN_QUEUE_H_
#define _FLM_LRN_QUEUE_H_

#include <stdint.h>

uint32_t *flm_lrn_queue_get_write_buffer(void *q);
void flm_lrn_queue_release_write_buffer(void *q);

#endif	/* _FLM_LRN_QUEUE_H_ */
