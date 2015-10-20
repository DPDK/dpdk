/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <string.h>

#include <rte_mbuf.h>
#include <rte_malloc.h>

#include "rte_table_stub.h"

#ifdef RTE_TABLE_STATS_COLLECT

#define RTE_TABLE_LPM_STATS_PKTS_IN_ADD(table, val) \
	table->stats.n_pkts_in += val
#define RTE_TABLE_LPM_STATS_PKTS_LOOKUP_MISS(table, val) \
	table->stats.n_pkts_lookup_miss += val

#else

#define RTE_TABLE_LPM_STATS_PKTS_IN_ADD(table, val)
#define RTE_TABLE_LPM_STATS_PKTS_LOOKUP_MISS(table, val)

#endif

struct rte_table_stub {
	struct rte_table_stats stats;
};

static void *
rte_table_stub_create(__rte_unused void *params,
		__rte_unused int socket_id,
		__rte_unused uint32_t entry_size)
{
	struct rte_table_stub *stub;
	uint32_t size;

	size = sizeof(struct rte_table_stub);
	stub = rte_zmalloc_socket("TABLE", size, RTE_CACHE_LINE_SIZE,
		socket_id);
	if (stub == NULL) {
		RTE_LOG(ERR, TABLE,
			"%s: Cannot allocate %u bytes for stub table\n",
			__func__, size);
		return NULL;
	}

	return stub;
}

static int
rte_table_stub_lookup(
	__rte_unused void *table,
	__rte_unused struct rte_mbuf **pkts,
	__rte_unused uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	__rte_unused void **entries)
{
	__rte_unused struct rte_table_stub *stub = (struct rte_table_stub *) table;
	__rte_unused uint32_t n_pkts_in = __builtin_popcountll(pkts_mask);

	RTE_TABLE_LPM_STATS_PKTS_IN_ADD(stub, n_pkts_in);
	*lookup_hit_mask = 0;
	RTE_TABLE_LPM_STATS_PKTS_LOOKUP_MISS(stub, n_pkts_in);

	return 0;
}

static int
rte_table_stub_stats_read(void *table, struct rte_table_stats *stats, int clear)
{
	struct rte_table_stub *t = (struct rte_table_stub *) table;

	if (stats != NULL)
		memcpy(stats, &t->stats, sizeof(t->stats));

	if (clear)
		memset(&t->stats, 0, sizeof(t->stats));

	return 0;
}

struct rte_table_ops rte_table_stub_ops = {
	.f_create = rte_table_stub_create,
	.f_free = NULL,
	.f_add = NULL,
	.f_delete = NULL,
	.f_add_bulk = NULL,
	.f_delete_bulk = NULL,
	.f_lookup = rte_table_stub_lookup,
	.f_stats = rte_table_stub_stats_read,
};
