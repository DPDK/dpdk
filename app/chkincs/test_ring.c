/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
 *   All rights reserved.
 * 
 *   Redistribution and use in source and binary forms, with or without 
 *   modification, are permitted provided that the following conditions 
 *   are met:
 * 
 *     * Redistributions of source code must retain the above copyright 
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright 
 *       notice, this list of conditions and the following disclaimer in 
 *       the documentation and/or other materials provided with the 
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its 
 *       contributors may be used to endorse or promote products derived 
 *       from this software without specific prior written permission.
 * 
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#include <rte_ring.h>

#include "test.h"

#define MAX_BULK 16
#define RING_SIZE 4096

/*
 *      ^
 *     / \
 *    / | \     WARNING: this test program does *not* show how to use the
 *   /  .  \    API. Its only goal is to check dependencies of include files.
 *  /_______\
 */

int
test_ring(void)
{
	struct rte_ring *r;
	void *ptrs[MAX_BULK];
	int x;

	r = rte_ring_create("test", RING_SIZE, SOCKET_ID_ANY, 0);
	if (r == 0) {
		return -1;
	}
	rte_ring_dump(r);

	rte_ring_set_bulk_count(r, MAX_BULK);
	rte_ring_set_water_mark(r, 50);

	rte_ring_sp_enqueue_bulk(r, &ptrs[0], 1);
	rte_ring_mp_enqueue_bulk(r, &ptrs[0], 1);
	rte_ring_sp_enqueue_bulk(r, &ptrs[0], MAX_BULK);
	rte_ring_mp_enqueue_bulk(r, &ptrs[0], MAX_BULK);
	rte_ring_enqueue_bulk(r, &ptrs[0], MAX_BULK);
	rte_ring_enqueue_bulk(r, &ptrs[0], MAX_BULK);
	rte_ring_sp_enqueue(r, &ptrs[0]);
	rte_ring_mp_enqueue(r, &ptrs[0]);
	rte_ring_enqueue(r, &ptrs[0]);

	rte_ring_sc_dequeue_bulk(r, &ptrs[0], 1);
	rte_ring_sc_dequeue_bulk(r, &ptrs[0], MAX_BULK);
	rte_ring_mc_dequeue_bulk(r, &ptrs[0], 1);
	rte_ring_mc_dequeue_bulk(r, &ptrs[0], MAX_BULK);
	rte_ring_dequeue_bulk(r, &ptrs[0], 1);
	rte_ring_dequeue_bulk(r, &ptrs[0], MAX_BULK);
	rte_ring_sc_dequeue(r, &ptrs[0]);
	rte_ring_mc_dequeue(r, &ptrs[0]);
	rte_ring_dequeue(r, &ptrs[0]);

	__RING_STAT_ADD(r, enq_fail, 10);

	x = rte_ring_full(r);
	x = rte_ring_empty(r);
	x = rte_ring_count(r);
	x = rte_ring_free_count(r);

	(void)x;

	return 0;
}
