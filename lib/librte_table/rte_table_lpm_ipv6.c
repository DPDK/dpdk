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
#include <stdio.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_lpm6.h>

#include "rte_table_lpm_ipv6.h"

#define RTE_TABLE_LPM_MAX_NEXT_HOPS                        256

#ifdef RTE_TABLE_STATS_COLLECT

#define RTE_TABLE_LPM_IPV6_STATS_PKTS_IN_ADD(table, val) \
	table->stats.n_pkts_in += val
#define RTE_TABLE_LPM_IPV6_STATS_PKTS_LOOKUP_MISS(table, val) \
	table->stats.n_pkts_lookup_miss += val

#else

#define RTE_TABLE_LPM_IPV6_STATS_PKTS_IN_ADD(table, val)
#define RTE_TABLE_LPM_IPV6_STATS_PKTS_LOOKUP_MISS(table, val)

#endif

struct rte_table_lpm_ipv6 {
	struct rte_table_stats stats;

	/* Input parameters */
	uint32_t entry_size;
	uint32_t entry_unique_size;
	uint32_t n_rules;
	uint32_t offset;

	/* Handle to low-level LPM table */
	struct rte_lpm6 *lpm;

	/* Next Hop Table (NHT) */
	uint32_t nht_users[RTE_TABLE_LPM_MAX_NEXT_HOPS];
	uint8_t nht[0] __rte_cache_aligned;
};

static void *
rte_table_lpm_ipv6_create(void *params, int socket_id, uint32_t entry_size)
{
	struct rte_table_lpm_ipv6_params *p =
		params;
	struct rte_table_lpm_ipv6 *lpm;
	struct rte_lpm6_config lpm6_config;
	uint32_t total_size, nht_size;

	/* Check input parameters */
	if (p == NULL) {
		RTE_LOG(ERR, TABLE, "%s: NULL input parameters\n", __func__);
		return NULL;
	}
	if (p->n_rules == 0) {
		RTE_LOG(ERR, TABLE, "%s: Invalid n_rules\n", __func__);
		return NULL;
	}
	if (p->number_tbl8s == 0) {
		RTE_LOG(ERR, TABLE, "%s: Invalid n_rules\n", __func__);
		return NULL;
	}
	if (p->entry_unique_size == 0) {
		RTE_LOG(ERR, TABLE, "%s: Invalid entry_unique_size\n",
			__func__);
		return NULL;
	}
	if (p->entry_unique_size > entry_size) {
		RTE_LOG(ERR, TABLE, "%s: Invalid entry_unique_size\n",
			__func__);
		return NULL;
	}
	if (p->name == NULL) {
		RTE_LOG(ERR, TABLE, "%s: Table name is NULL\n",
			__func__);
		return NULL;
	}
	entry_size = RTE_ALIGN(entry_size, sizeof(uint64_t));

	/* Memory allocation */
	nht_size = RTE_TABLE_LPM_MAX_NEXT_HOPS * entry_size;
	total_size = sizeof(struct rte_table_lpm_ipv6) + nht_size;
	lpm = rte_zmalloc_socket("TABLE", total_size, RTE_CACHE_LINE_SIZE,
		socket_id);
	if (lpm == NULL) {
		RTE_LOG(ERR, TABLE,
			"%s: Cannot allocate %u bytes for LPM IPv6 table\n",
			__func__, total_size);
		return NULL;
	}

	/* LPM low-level table creation */
	lpm6_config.max_rules = p->n_rules;
	lpm6_config.number_tbl8s = p->number_tbl8s;
	lpm6_config.flags = 0;
	lpm->lpm = rte_lpm6_create(p->name, socket_id, &lpm6_config);
	if (lpm->lpm == NULL) {
		rte_free(lpm);
		RTE_LOG(ERR, TABLE,
			"Unable to create low-level LPM IPv6 table\n");
		return NULL;
	}

	/* Memory initialization */
	lpm->entry_size = entry_size;
	lpm->entry_unique_size = p->entry_unique_size;
	lpm->n_rules = p->n_rules;
	lpm->offset = p->offset;

	return lpm;
}

static int
rte_table_lpm_ipv6_free(void *table)
{
	struct rte_table_lpm_ipv6 *lpm = table;

	/* Check input parameters */
	if (lpm == NULL) {
		RTE_LOG(ERR, TABLE, "%s: table parameter is NULL\n", __func__);
		return -EINVAL;
	}

	/* Free previously allocated resources */
	rte_lpm6_free(lpm->lpm);
	rte_free(lpm);

	return 0;
}

static int
nht_find_free(struct rte_table_lpm_ipv6 *lpm, uint32_t *pos)
{
	uint32_t i;

	for (i = 0; i < RTE_TABLE_LPM_MAX_NEXT_HOPS; i++) {
		if (lpm->nht_users[i] == 0) {
			*pos = i;
			return 1;
		}
	}

	return 0;
}

static int
nht_find_existing(struct rte_table_lpm_ipv6 *lpm, void *entry, uint32_t *pos)
{
	uint32_t i;

	for (i = 0; i < RTE_TABLE_LPM_MAX_NEXT_HOPS; i++) {
		uint8_t *nht_entry = &lpm->nht[i * lpm->entry_size];

		if ((lpm->nht_users[i] > 0) && (memcmp(nht_entry, entry,
			lpm->entry_unique_size) == 0)) {
			*pos = i;
			return 1;
		}
	}

	return 0;
}

static int
rte_table_lpm_ipv6_entry_add(
	void *table,
	void *key,
	void *entry,
	int *key_found,
	void **entry_ptr)
{
	struct rte_table_lpm_ipv6 *lpm = table;
	struct rte_table_lpm_ipv6_key *ip_prefix =
		key;
	uint32_t nht_pos, nht_pos0, nht_pos0_valid;
	int status;

	/* Check input parameters */
	if (lpm == NULL) {
		RTE_LOG(ERR, TABLE, "%s: table parameter is NULL\n", __func__);
		return -EINVAL;
	}
	if (ip_prefix == NULL) {
		RTE_LOG(ERR, TABLE, "%s: ip_prefix parameter is NULL\n",
			__func__);
		return -EINVAL;
	}
	if (entry == NULL) {
		RTE_LOG(ERR, TABLE, "%s: entry parameter is NULL\n", __func__);
		return -EINVAL;
	}

	if ((ip_prefix->depth == 0) || (ip_prefix->depth > 128)) {
		RTE_LOG(ERR, TABLE, "%s: invalid depth (%d)\n", __func__,
			ip_prefix->depth);
		return -EINVAL;
	}

	/* Check if rule is already present in the table */
	status = rte_lpm6_is_rule_present(lpm->lpm, ip_prefix->ip,
		ip_prefix->depth, &nht_pos0);
	nht_pos0_valid = status > 0;

	/* Find existing or free NHT entry */
	if (nht_find_existing(lpm, entry, &nht_pos) == 0) {
		uint8_t *nht_entry;

		if (nht_find_free(lpm, &nht_pos) == 0) {
			RTE_LOG(ERR, TABLE, "%s: NHT full\n", __func__);
			return -1;
		}

		nht_entry = &lpm->nht[nht_pos * lpm->entry_size];
		memcpy(nht_entry, entry, lpm->entry_size);
	}

	/* Add rule to low level LPM table */
	if (rte_lpm6_add(lpm->lpm, ip_prefix->ip, ip_prefix->depth,
		nht_pos) < 0) {
		RTE_LOG(ERR, TABLE, "%s: LPM IPv6 rule add failed\n", __func__);
		return -1;
	}

	/* Commit NHT changes */
	lpm->nht_users[nht_pos]++;
	lpm->nht_users[nht_pos0] -= nht_pos0_valid;

	*key_found = nht_pos0_valid;
	*entry_ptr = (void *) &lpm->nht[nht_pos * lpm->entry_size];
	return 0;
}

static int
rte_table_lpm_ipv6_entry_delete(
	void *table,
	void *key,
	int *key_found,
	void *entry)
{
	struct rte_table_lpm_ipv6 *lpm = table;
	struct rte_table_lpm_ipv6_key *ip_prefix =
		key;
	uint32_t nht_pos;
	int status;

	/* Check input parameters */
	if (lpm == NULL) {
		RTE_LOG(ERR, TABLE, "%s: table parameter is NULL\n", __func__);
		return -EINVAL;
	}
	if (ip_prefix == NULL) {
		RTE_LOG(ERR, TABLE, "%s: ip_prefix parameter is NULL\n",
			__func__);
		return -EINVAL;
	}
	if ((ip_prefix->depth == 0) || (ip_prefix->depth > 128)) {
		RTE_LOG(ERR, TABLE, "%s: invalid depth (%d)\n", __func__,
			ip_prefix->depth);
		return -EINVAL;
	}

	/* Return if rule is not present in the table */
	status = rte_lpm6_is_rule_present(lpm->lpm, ip_prefix->ip,
		ip_prefix->depth, &nht_pos);
	if (status < 0) {
		RTE_LOG(ERR, TABLE, "%s: LPM IPv6 algorithmic error\n",
			__func__);
		return -1;
	}
	if (status == 0) {
		*key_found = 0;
		return 0;
	}

	/* Delete rule from the low-level LPM table */
	status = rte_lpm6_delete(lpm->lpm, ip_prefix->ip, ip_prefix->depth);
	if (status) {
		RTE_LOG(ERR, TABLE, "%s: LPM IPv6 rule delete failed\n",
			__func__);
		return -1;
	}

	/* Commit NHT changes */
	lpm->nht_users[nht_pos]--;

	*key_found = 1;
	if (entry)
		memcpy(entry, &lpm->nht[nht_pos * lpm->entry_size],
			lpm->entry_size);

	return 0;
}

static int
rte_table_lpm_ipv6_lookup(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_lpm_ipv6 *lpm = (struct rte_table_lpm_ipv6 *) table;
	uint64_t pkts_out_mask = 0;
	uint32_t i;

	__rte_unused uint32_t n_pkts_in = __builtin_popcountll(pkts_mask);
	RTE_TABLE_LPM_IPV6_STATS_PKTS_IN_ADD(lpm, n_pkts_in);

	pkts_out_mask = 0;
	for (i = 0; i < (uint32_t)(RTE_PORT_IN_BURST_SIZE_MAX -
		__builtin_clzll(pkts_mask)); i++) {
		uint64_t pkt_mask = 1LLU << i;

		if (pkt_mask & pkts_mask) {
			struct rte_mbuf *pkt = pkts[i];
			uint8_t *ip = RTE_MBUF_METADATA_UINT8_PTR(pkt,
				lpm->offset);
			int status;
			uint32_t nht_pos;

			status = rte_lpm6_lookup(lpm->lpm, ip, &nht_pos);
			if (status == 0) {
				pkts_out_mask |= pkt_mask;
				entries[i] = (void *) &lpm->nht[nht_pos *
					lpm->entry_size];
			}
		}
	}

	*lookup_hit_mask = pkts_out_mask;
	RTE_TABLE_LPM_IPV6_STATS_PKTS_LOOKUP_MISS(lpm, n_pkts_in - __builtin_popcountll(pkts_out_mask));
	return 0;
}

static int
rte_table_lpm_ipv6_stats_read(void *table, struct rte_table_stats *stats, int clear)
{
	struct rte_table_lpm_ipv6 *t = table;

	if (stats != NULL)
		memcpy(stats, &t->stats, sizeof(t->stats));

	if (clear)
		memset(&t->stats, 0, sizeof(t->stats));

	return 0;
}

struct rte_table_ops rte_table_lpm_ipv6_ops = {
	.f_create = rte_table_lpm_ipv6_create,
	.f_free = rte_table_lpm_ipv6_free,
	.f_add = rte_table_lpm_ipv6_entry_add,
	.f_delete = rte_table_lpm_ipv6_entry_delete,
	.f_add_bulk = NULL,
	.f_delete_bulk = NULL,
	.f_lookup = rte_table_lpm_ipv6_lookup,
	.f_stats = rte_table_lpm_ipv6_stats_read,
};
