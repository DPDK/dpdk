/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <rte_ring.h>

#include "hw_mod_flm_v25.h"

#include "flm_lrn_queue.h"

#define ELEM_SIZE sizeof(struct flm_v25_lrn_data_s)

uint32_t *flm_lrn_queue_get_write_buffer(void *q)
{
	struct rte_ring_zc_data zcd;
	unsigned int n = rte_ring_enqueue_zc_burst_elem_start(q, ELEM_SIZE, 1, &zcd, NULL);
	return (n == 0) ? NULL : zcd.ptr1;
}

void flm_lrn_queue_release_write_buffer(void *q)
{
	rte_ring_enqueue_zc_elem_finish(q, 1);
}
