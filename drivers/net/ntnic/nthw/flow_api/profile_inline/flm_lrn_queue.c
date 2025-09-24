/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

#include <stdlib.h>
#include <string.h>

#include <rte_ring.h>

#include "hw_mod_flm_v25.h"

#include "flm_lrn_queue.h"

#define QUEUE_SIZE (1 << 13)

#define ELEM_SIZE sizeof(struct flm_v25_lrn_data_s)

void *nthw_flm_lrn_queue_create(void)
{
	static_assert((ELEM_SIZE & ~(size_t)3) == ELEM_SIZE, "FLM LEARN struct size");
	struct rte_ring *q = rte_ring_create_elem("RFQ",
		ELEM_SIZE,
		QUEUE_SIZE,
		SOCKET_ID_ANY,
		RING_F_MP_HTS_ENQ | RING_F_SC_DEQ);
	RTE_ASSERT(q != NULL);
	return q;
}

void nthw_flm_lrn_queue_free(void *q)
{
	rte_ring_free(q);
}

uint32_t *nthw_flm_lrn_queue_get_write_buffer(void *q)
{
	struct rte_ring_zc_data zcd;
	unsigned int n = rte_ring_enqueue_zc_burst_elem_start(q, ELEM_SIZE, 1, &zcd, NULL);
	return (n == 0) ? NULL : zcd.ptr1;
}

void nthw_flm_lrn_queue_release_write_buffer(void *q)
{
	rte_ring_enqueue_zc_elem_finish(q, 1);
}

read_record nthw_flm_lrn_queue_get_read_buffer(void *q)
{
	struct rte_ring_zc_data zcd;
	read_record rr;

	if (rte_ring_dequeue_zc_burst_elem_start(q, ELEM_SIZE, QUEUE_SIZE, &zcd, NULL) != 0) {
		rr.num = zcd.n1;
		rr.p = zcd.ptr1;

	} else {
		rr.num = 0;
		rr.p = NULL;
	}

	return rr;
}

void nthw_flm_lrn_queue_release_read_buffer(void *q, uint32_t num)
{
	rte_ring_dequeue_zc_elem_finish(q, num);
}
