/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <string.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_log.h>

#include "rte_table_array.h"

#include "table_log.h"

#ifdef RTE_TABLE_STATS_COLLECT

#define RTE_TABLE_ARRAY_STATS_PKTS_IN_ADD(table, val) \
	table->stats.n_pkts_in += val
#define RTE_TABLE_ARRAY_STATS_PKTS_LOOKUP_MISS(table, val) \
	table->stats.n_pkts_lookup_miss += val

#else

#define RTE_TABLE_ARRAY_STATS_PKTS_IN_ADD(table, val)
#define RTE_TABLE_ARRAY_STATS_PKTS_LOOKUP_MISS(table, val)

#endif

struct rte_table_array {
	struct rte_table_stats stats;

	/* Input parameters */
	uint32_t entry_size;
	uint32_t n_entries;
	uint32_t offset;

	/* Internal fields */
	uint32_t entry_pos_mask;

	/* Internal table */
	uint8_t array[0] __rte_cache_aligned;
} __rte_cache_aligned;

static void *
rte_table_array_create(void *params, int socket_id, uint32_t entry_size)
{
	struct rte_table_array_params *p = params;
	struct rte_table_array *t;
	uint32_t total_cl_size, total_size;

	/* Check input parameters */
	if ((p == NULL) ||
	    (p->n_entries == 0) ||
		(!rte_is_power_of_2(p->n_entries)))
		return NULL;

	/* Memory allocation */
	total_cl_size = (sizeof(struct rte_table_array) +
			RTE_CACHE_LINE_SIZE) / RTE_CACHE_LINE_SIZE;
	total_cl_size += (p->n_entries * entry_size +
			RTE_CACHE_LINE_SIZE) / RTE_CACHE_LINE_SIZE;
	total_size = total_cl_size * RTE_CACHE_LINE_SIZE;
	t = rte_zmalloc_socket("TABLE", total_size, RTE_CACHE_LINE_SIZE, socket_id);
	if (t == NULL) {
		TABLE_LOG(ERR,
			"%s: Cannot allocate %u bytes for array table",
			__func__, total_size);
		return NULL;
	}

	/* Memory initialization */
	t->entry_size = entry_size;
	t->n_entries = p->n_entries;
	t->offset = p->offset;
	t->entry_pos_mask = t->n_entries - 1;

	return t;
}

static int
rte_table_array_free(void *table)
{
	struct rte_table_array *t = table;

	/* Check input parameters */
	if (t == NULL) {
		TABLE_LOG(ERR, "%s: table parameter is NULL", __func__);
		return -EINVAL;
	}

	/* Free previously allocated resources */
	rte_free(t);

	return 0;
}

static int
rte_table_array_entry_add(
	void *table,
	void *key,
	void *entry,
	int *key_found,
	void **entry_ptr)
{
	struct rte_table_array *t = table;
	struct rte_table_array_key *k = key;
	uint8_t *table_entry;

	/* Check input parameters */
	if (table == NULL) {
		TABLE_LOG(ERR, "%s: table parameter is NULL", __func__);
		return -EINVAL;
	}
	if (key == NULL) {
		TABLE_LOG(ERR, "%s: key parameter is NULL", __func__);
		return -EINVAL;
	}
	if (entry == NULL) {
		TABLE_LOG(ERR, "%s: entry parameter is NULL", __func__);
		return -EINVAL;
	}
	if (key_found == NULL) {
		TABLE_LOG(ERR, "%s: key_found parameter is NULL",
			__func__);
		return -EINVAL;
	}
	if (entry_ptr == NULL) {
		TABLE_LOG(ERR, "%s: entry_ptr parameter is NULL",
			__func__);
		return -EINVAL;
	}

	table_entry = &t->array[k->pos * t->entry_size];
	memcpy(table_entry, entry, t->entry_size);
	*key_found = 1;
	*entry_ptr = (void *) table_entry;

	return 0;
}

static int
rte_table_array_lookup(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_array *t = (struct rte_table_array *) table;
	__rte_unused uint32_t n_pkts_in = rte_popcount64(pkts_mask);
	RTE_TABLE_ARRAY_STATS_PKTS_IN_ADD(t, n_pkts_in);
	*lookup_hit_mask = pkts_mask;

	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		uint64_t n_pkts = rte_popcount64(pkts_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++) {
			struct rte_mbuf *pkt = pkts[i];
			uint32_t entry_pos = RTE_MBUF_METADATA_UINT32(pkt,
				t->offset) & t->entry_pos_mask;

			entries[i] = (void *) &t->array[entry_pos *
				t->entry_size];
		}
	} else {
		for ( ; pkts_mask; ) {
			uint32_t pkt_index = rte_ctz64(pkts_mask);
			uint64_t pkt_mask = 1LLU << pkt_index;
			struct rte_mbuf *pkt = pkts[pkt_index];
			uint32_t entry_pos = RTE_MBUF_METADATA_UINT32(pkt,
				t->offset) & t->entry_pos_mask;

			entries[pkt_index] = (void *) &t->array[entry_pos *
				t->entry_size];
			pkts_mask &= ~pkt_mask;
		}
	}

	return 0;
}

static int
rte_table_array_stats_read(void *table, struct rte_table_stats *stats, int clear)
{
	struct rte_table_array *array = table;

	if (stats != NULL)
		memcpy(stats, &array->stats, sizeof(array->stats));

	if (clear)
		memset(&array->stats, 0, sizeof(array->stats));

	return 0;
}

struct rte_table_ops rte_table_array_ops = {
	.f_create = rte_table_array_create,
	.f_free = rte_table_array_free,
	.f_add = rte_table_array_entry_add,
	.f_delete = NULL,
	.f_add_bulk = NULL,
	.f_delete_bulk = NULL,
	.f_lookup = rte_table_array_lookup,
	.f_stats = rte_table_array_stats_read,
};
