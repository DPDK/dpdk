/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stddef.h>
#include <stdio.h>

#include <rte_memory.h>
#include <rte_log.h>

#include "ip_frag_common.h"

#define	IP_FRAG_HASH_FNUM	2

/* free mbufs from death row */
void
rte_ip_frag_free_death_row(struct rte_ip_frag_death_row *dr,
		uint32_t prefetch)
{
	uint32_t i, k, n;

	k = RTE_MIN(prefetch, dr->cnt);
	n = dr->cnt;

	for (i = 0; i != k; i++)
		rte_prefetch0(dr->row[i]);

	for (i = 0; i != n - k; i++) {
		rte_prefetch0(dr->row[i + k]);
		rte_pktmbuf_free(dr->row[i]);
	}

	for (; i != n; i++)
		rte_pktmbuf_free(dr->row[i]);

	dr->cnt = 0;
}

/* create fragmentation table */
struct rte_ip_frag_tbl *
rte_ip_frag_table_create(uint32_t bucket_num, uint32_t bucket_entries,
	uint32_t max_entries, uint64_t max_cycles, int socket_id)
{
	struct rte_ip_frag_tbl *tbl;
	size_t sz;
	uint64_t nb_entries;

	nb_entries = rte_align32pow2(bucket_num);
	nb_entries *= bucket_entries;
	nb_entries *= IP_FRAG_HASH_FNUM;

	/* check input parameters. */
	if (rte_is_power_of_2(bucket_entries) == 0 ||
			nb_entries > UINT32_MAX || nb_entries == 0 ||
			nb_entries < max_entries) {
		RTE_LOG(ERR, USER1, "%s: invalid input parameter\n", __func__);
		return NULL;
	}

	sz = sizeof (*tbl) + nb_entries * sizeof (tbl->pkt[0]);
	if ((tbl = rte_zmalloc_socket(__func__, sz, RTE_CACHE_LINE_SIZE,
			socket_id)) == NULL) {
		RTE_LOG(ERR, USER1,
			"%s: allocation of %zu bytes at socket %d failed do\n",
			__func__, sz, socket_id);
		return NULL;
	}

	RTE_LOG(INFO, USER1, "%s: allocated of %zu bytes at socket %d\n",
		__func__, sz, socket_id);

	tbl->max_cycles = max_cycles;
	tbl->max_entries = max_entries;
	tbl->nb_entries = (uint32_t)nb_entries;
	tbl->nb_buckets = bucket_num;
	tbl->bucket_entries = bucket_entries;
	tbl->entry_mask = (tbl->nb_entries - 1) & ~(tbl->bucket_entries  - 1);

	TAILQ_INIT(&(tbl->lru));
	return tbl;
}

/* delete fragmentation table */
void
rte_ip_frag_table_destroy(struct rte_ip_frag_tbl *tbl)
{
	struct ip_frag_pkt *fp;

	TAILQ_FOREACH(fp, &tbl->lru, lru) {
		ip_frag_free_immediate(fp);
	}

	rte_free(tbl);
}

/* dump frag table statistics to file */
void
rte_ip_frag_table_statistics_dump(FILE *f, const struct rte_ip_frag_tbl *tbl)
{
	uint64_t fail_total, fail_nospace;

	fail_total = tbl->stat.fail_total;
	fail_nospace = tbl->stat.fail_nospace;

	fprintf(f, "max entries:\t%u;\n"
		"entries in use:\t%u;\n"
		"finds/inserts:\t%" PRIu64 ";\n"
		"entries added:\t%" PRIu64 ";\n"
		"entries deleted by timeout:\t%" PRIu64 ";\n"
		"entries reused by timeout:\t%" PRIu64 ";\n"
		"total add failures:\t%" PRIu64 ";\n"
		"add no-space failures:\t%" PRIu64 ";\n"
		"add hash-collisions failures:\t%" PRIu64 ";\n",
		tbl->max_entries,
		tbl->use_entries,
		tbl->stat.find_num,
		tbl->stat.add_num,
		tbl->stat.del_num,
		tbl->stat.reuse_num,
		fail_total,
		fail_nospace,
		fail_total - fail_nospace);
}
