/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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
 */

#include <stdio.h>
#include <string.h>

#include <rte_errno.h>
#include <rte_ring.h>
#include <rte_mempool.h>

static int
common_ring_mp_enqueue(struct rte_mempool *mp, void * const *obj_table,
		unsigned n)
{
	return rte_ring_mp_enqueue_bulk(mp->pool_data,
			obj_table, n, NULL) == 0 ? -ENOBUFS : 0;
}

static int
common_ring_sp_enqueue(struct rte_mempool *mp, void * const *obj_table,
		unsigned n)
{
	return rte_ring_sp_enqueue_bulk(mp->pool_data,
			obj_table, n, NULL) == 0 ? -ENOBUFS : 0;
}

static int
common_ring_mc_dequeue(struct rte_mempool *mp, void **obj_table, unsigned n)
{
	return rte_ring_mc_dequeue_bulk(mp->pool_data,
			obj_table, n, NULL) == 0 ? -ENOBUFS : 0;
}

static int
common_ring_sc_dequeue(struct rte_mempool *mp, void **obj_table, unsigned n)
{
	return rte_ring_sc_dequeue_bulk(mp->pool_data,
			obj_table, n, NULL) == 0 ? -ENOBUFS : 0;
}

static unsigned
common_ring_get_count(const struct rte_mempool *mp)
{
	return rte_ring_count(mp->pool_data);
}


static int
common_ring_alloc(struct rte_mempool *mp)
{
	int rg_flags = 0, ret;
	char rg_name[RTE_RING_NAMESIZE];
	struct rte_ring *r;

	ret = snprintf(rg_name, sizeof(rg_name),
		RTE_MEMPOOL_MZ_FORMAT, mp->name);
	if (ret < 0 || ret >= (int)sizeof(rg_name)) {
		rte_errno = ENAMETOOLONG;
		return -rte_errno;
	}

	/* ring flags */
	if (mp->flags & MEMPOOL_F_SP_PUT)
		rg_flags |= RING_F_SP_ENQ;
	if (mp->flags & MEMPOOL_F_SC_GET)
		rg_flags |= RING_F_SC_DEQ;

	/*
	 * Allocate the ring that will be used to store objects.
	 * Ring functions will return appropriate errors if we are
	 * running as a secondary process etc., so no checks made
	 * in this function for that condition.
	 */
	r = rte_ring_create(rg_name, rte_align32pow2(mp->size + 1),
		mp->socket_id, rg_flags);
	if (r == NULL)
		return -rte_errno;

	mp->pool_data = r;

	return 0;
}

static void
common_ring_free(struct rte_mempool *mp)
{
	rte_ring_free(mp->pool_data);
}

/*
 * The following 4 declarations of mempool ops structs address
 * the need for the backward compatible mempool handlers for
 * single/multi producers and single/multi consumers as dictated by the
 * flags provided to the rte_mempool_create function
 */
static const struct rte_mempool_ops ops_mp_mc = {
	.name = "ring_mp_mc",
	.alloc = common_ring_alloc,
	.free = common_ring_free,
	.enqueue = common_ring_mp_enqueue,
	.dequeue = common_ring_mc_dequeue,
	.get_count = common_ring_get_count,
};

static const struct rte_mempool_ops ops_sp_sc = {
	.name = "ring_sp_sc",
	.alloc = common_ring_alloc,
	.free = common_ring_free,
	.enqueue = common_ring_sp_enqueue,
	.dequeue = common_ring_sc_dequeue,
	.get_count = common_ring_get_count,
};

static const struct rte_mempool_ops ops_mp_sc = {
	.name = "ring_mp_sc",
	.alloc = common_ring_alloc,
	.free = common_ring_free,
	.enqueue = common_ring_mp_enqueue,
	.dequeue = common_ring_sc_dequeue,
	.get_count = common_ring_get_count,
};

static const struct rte_mempool_ops ops_sp_mc = {
	.name = "ring_sp_mc",
	.alloc = common_ring_alloc,
	.free = common_ring_free,
	.enqueue = common_ring_sp_enqueue,
	.dequeue = common_ring_mc_dequeue,
	.get_count = common_ring_get_count,
};

MEMPOOL_REGISTER_OPS(ops_mp_mc);
MEMPOOL_REGISTER_OPS(ops_sp_sc);
MEMPOOL_REGISTER_OPS(ops_mp_sc);
MEMPOOL_REGISTER_OPS(ops_sp_mc);
