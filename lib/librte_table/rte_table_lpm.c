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
#include <rte_malloc.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_lpm.h>

#include "rte_table_lpm.h"

#define RTE_TABLE_LPM_MAX_NEXT_HOPS                        256

struct rte_table_lpm {
	/* Input parameters */
	uint32_t entry_size;
	uint32_t entry_unique_size;
	uint32_t n_rules;
	uint32_t offset;

	/* Handle to low-level LPM table */
	struct rte_lpm *lpm;

	/* Next Hop Table (NHT) */
	uint32_t nht_users[RTE_TABLE_LPM_MAX_NEXT_HOPS];
	uint8_t nht[0] __rte_cache_aligned;
};

static void *
rte_table_lpm_create(void *params, int socket_id, uint32_t entry_size)
{
	struct rte_table_lpm_params *p = (struct rte_table_lpm_params *) params;
	struct rte_table_lpm *lpm;
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
	if ((p->offset & 0x3) != 0) {
		RTE_LOG(ERR, TABLE, "%s: Invalid offset\n", __func__);
		return NULL;
	}

	entry_size = RTE_ALIGN(entry_size, sizeof(uint64_t));

	/* Memory allocation */
	nht_size = RTE_TABLE_LPM_MAX_NEXT_HOPS * entry_size;
	total_size = sizeof(struct rte_table_lpm) + nht_size;
	lpm = rte_zmalloc_socket("TABLE", total_size, CACHE_LINE_SIZE,
		socket_id);
	if (lpm == NULL) {
		RTE_LOG(ERR, TABLE,
			"%s: Cannot allocate %u bytes for LPM table\n",
			__func__, total_size);
		return NULL;
	}

	/* LPM low-level table creation */
	lpm->lpm = rte_lpm_create("LPM", socket_id, p->n_rules, 0);
	if (lpm->lpm == NULL) {
		rte_free(lpm);
		RTE_LOG(ERR, TABLE, "Unable to create low-level LPM table\n");
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
rte_table_lpm_free(void *table)
{
	struct rte_table_lpm *lpm = (struct rte_table_lpm *) table;

	/* Check input parameters */
	if (lpm == NULL) {
		RTE_LOG(ERR, TABLE, "%s: table parameter is NULL\n", __func__);
		return -EINVAL;
	}

	/* Free previously allocated resources */
	rte_lpm_free(lpm->lpm);
	rte_free(lpm);

	return 0;
}

static int
nht_find_free(struct rte_table_lpm *lpm, uint32_t *pos)
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
nht_find_existing(struct rte_table_lpm *lpm, void *entry, uint32_t *pos)
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
rte_table_lpm_entry_add(
	void *table,
	void *key,
	void *entry,
	int *key_found,
	void **entry_ptr)
{
	struct rte_table_lpm *lpm = (struct rte_table_lpm *) table;
	struct rte_table_lpm_key *ip_prefix = (struct rte_table_lpm_key *) key;
	uint32_t nht_pos, nht_pos0_valid;
	int status;
	uint8_t nht_pos0;

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

	if ((ip_prefix->depth == 0) || (ip_prefix->depth > 32)) {
		RTE_LOG(ERR, TABLE, "%s: invalid depth (%d)\n",
			__func__, ip_prefix->depth);
		return -EINVAL;
	}

	/* Check if rule is already present in the table */
	status = rte_lpm_is_rule_present(lpm->lpm, ip_prefix->ip,
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
	if (rte_lpm_add(lpm->lpm, ip_prefix->ip, ip_prefix->depth,
		(uint8_t) nht_pos) < 0) {
		RTE_LOG(ERR, TABLE, "%s: LPM rule add failed\n", __func__);
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
rte_table_lpm_entry_delete(
	void *table,
	void *key,
	int *key_found,
	void *entry)
{
	struct rte_table_lpm *lpm = (struct rte_table_lpm *) table;
	struct rte_table_lpm_key *ip_prefix = (struct rte_table_lpm_key *) key;
	uint8_t nht_pos;
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
	if ((ip_prefix->depth == 0) || (ip_prefix->depth > 32)) {
		RTE_LOG(ERR, TABLE, "%s: invalid depth (%d)\n", __func__,
			ip_prefix->depth);
		return -EINVAL;
	}

	/* Return if rule is not present in the table */
	status = rte_lpm_is_rule_present(lpm->lpm, ip_prefix->ip,
		ip_prefix->depth, &nht_pos);
	if (status < 0) {
		RTE_LOG(ERR, TABLE, "%s: LPM algorithmic error\n", __func__);
		return -1;
	}
	if (status == 0) {
		*key_found = 0;
		return 0;
	}

	/* Delete rule from the low-level LPM table */
	status = rte_lpm_delete(lpm->lpm, ip_prefix->ip, ip_prefix->depth);
	if (status) {
		RTE_LOG(ERR, TABLE, "%s: LPM rule delete failed\n", __func__);
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
rte_table_lpm_lookup(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_lpm *lpm = (struct rte_table_lpm *) table;
	uint64_t pkts_out_mask = 0;
	uint32_t i;

	pkts_out_mask = 0;
	for (i = 0; i < (uint32_t)(RTE_PORT_IN_BURST_SIZE_MAX -
		__builtin_clzll(pkts_mask)); i++) {
		uint64_t pkt_mask = 1LLU << i;

		if (pkt_mask & pkts_mask) {
			struct rte_mbuf *pkt = pkts[i];
			uint32_t ip = rte_bswap32(
				RTE_MBUF_METADATA_UINT32(pkt, lpm->offset));
			int status;
			uint8_t nht_pos;

			status = rte_lpm_lookup(lpm->lpm, ip, &nht_pos);
			if (status == 0) {
				pkts_out_mask |= pkt_mask;
				entries[i] = (void *) &lpm->nht[nht_pos *
					lpm->entry_size];
			}
		}
	}

	*lookup_hit_mask = pkts_out_mask;

	return 0;
}

struct rte_table_ops rte_table_lpm_ops = {
	.f_create = rte_table_lpm_create,
	.f_free = rte_table_lpm_free,
	.f_add = rte_table_lpm_entry_add,
	.f_delete = rte_table_lpm_entry_delete,
	.f_lookup = rte_table_lpm_lookup,
};
