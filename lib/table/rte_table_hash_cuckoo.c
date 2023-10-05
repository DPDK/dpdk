/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */
#include <string.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_log.h>

#include "rte_table_hash_cuckoo.h"

#ifdef RTE_TABLE_STATS_COLLECT

#define RTE_TABLE_HASH_CUCKOO_STATS_PKTS_IN_ADD(table, val) \
	(table->stats.n_pkts_in += val)
#define RTE_TABLE_HASH_CUCKOO_STATS_PKTS_LOOKUP_MISS(table, val) \
	(table->stats.n_pkts_lookup_miss += val)

#else

#define RTE_TABLE_HASH_CUCKOO_STATS_PKTS_IN_ADD(table, val)
#define RTE_TABLE_HASH_CUCKOO_STATS_PKTS_LOOKUP_MISS(table, val)

#endif


struct rte_table_hash {
	struct rte_table_stats stats;

	/* Input parameters */
	uint32_t key_size;
	uint32_t entry_size;
	uint32_t n_keys;
	rte_hash_function f_hash;
	uint32_t seed;
	uint32_t key_offset;

	/* cuckoo hash table object */
	struct rte_hash *h_table;

	/* Lookup table */
	uint8_t memory[0] __rte_cache_aligned;
};

static int
check_params_create_hash_cuckoo(struct rte_table_hash_cuckoo_params *params)
{
	if (params == NULL) {
		RTE_LOG(ERR, TABLE, "NULL Input Parameters.\n");
		return -EINVAL;
	}

	if (params->name == NULL) {
		RTE_LOG(ERR, TABLE, "Table name is NULL.\n");
		return -EINVAL;
	}

	if (params->key_size == 0) {
		RTE_LOG(ERR, TABLE, "Invalid key_size.\n");
		return -EINVAL;
	}

	if (params->n_keys == 0) {
		RTE_LOG(ERR, TABLE, "Invalid n_keys.\n");
		return -EINVAL;
	}

	if (params->f_hash == NULL) {
		RTE_LOG(ERR, TABLE, "f_hash is NULL.\n");
		return -EINVAL;
	}

	return 0;
}

static void *
rte_table_hash_cuckoo_create(void *params,
			int socket_id,
			uint32_t entry_size)
{
	struct rte_table_hash_cuckoo_params *p = params;
	struct rte_hash *h_table;
	struct rte_table_hash *t;
	uint32_t total_size;

	/* Check input parameters */
	if (check_params_create_hash_cuckoo(params))
		return NULL;

	/* Memory allocation */
	total_size = sizeof(struct rte_table_hash) +
		RTE_CACHE_LINE_ROUNDUP(p->n_keys * entry_size);

	t = rte_zmalloc_socket(p->name, total_size, RTE_CACHE_LINE_SIZE, socket_id);
	if (t == NULL) {
		RTE_LOG(ERR, TABLE,
			"%s: Cannot allocate %u bytes for cuckoo hash table %s\n",
			__func__, total_size, p->name);
		return NULL;
	}

	/* Create cuckoo hash table */
	struct rte_hash_parameters hash_cuckoo_params = {
		.entries = p->n_keys,
		.key_len = p->key_size,
		.hash_func = p->f_hash,
		.hash_func_init_val = p->seed,
		.socket_id = socket_id,
		.name = p->name
	};

	h_table = rte_hash_find_existing(p->name);
	if (h_table == NULL) {
		h_table = rte_hash_create(&hash_cuckoo_params);
		if (h_table == NULL) {
			RTE_LOG(ERR, TABLE,
				"%s: failed to create cuckoo hash table %s\n",
				__func__, p->name);
			rte_free(t);
			return NULL;
		}
	}

	/* initialize the cuckoo hash parameters */
	t->key_size = p->key_size;
	t->entry_size = entry_size;
	t->n_keys = p->n_keys;
	t->f_hash = p->f_hash;
	t->seed = p->seed;
	t->key_offset = p->key_offset;
	t->h_table = h_table;

	RTE_LOG(INFO, TABLE,
		"%s: Cuckoo hash table %s memory footprint is %u bytes\n",
		__func__, p->name, total_size);
	return t;
}

static int
rte_table_hash_cuckoo_free(void *table) {
	struct rte_table_hash *t = table;

	if (table == NULL)
		return -EINVAL;

	rte_hash_free(t->h_table);
	rte_free(t);

	return 0;
}

static int
rte_table_hash_cuckoo_entry_add(void *table, void *key, void *entry,
	int *key_found, void **entry_ptr)
{
	struct rte_table_hash *t = table;
	int pos = 0;

	/* Check input parameters */
	if ((table == NULL) ||
		(key == NULL) ||
		(entry == NULL) ||
		(key_found == NULL) ||
		(entry_ptr == NULL))
		return -EINVAL;

	/*  Find Existing entries */
	pos = rte_hash_lookup(t->h_table, key);
	if (pos >= 0) {
		uint8_t *existing_entry;

		*key_found = 1;
		existing_entry = &t->memory[pos * t->entry_size];
		memcpy(existing_entry, entry, t->entry_size);
		*entry_ptr = existing_entry;

		return 0;
	}

	if (pos == -ENOENT) {
		/* Entry not found. Adding new entry */
		uint8_t *new_entry;

		pos = rte_hash_add_key(t->h_table, key);
		if (pos < 0)
			return pos;

		new_entry = &t->memory[pos * t->entry_size];
		memcpy(new_entry, entry, t->entry_size);

		*key_found = 0;
		*entry_ptr = new_entry;
		return 0;
	}

	return pos;
}

static int
rte_table_hash_cuckoo_entry_delete(void *table, void *key,
	int *key_found, void *entry)
{
	struct rte_table_hash *t = table;
	int pos = 0;

	/* Check input parameters */
	if ((table == NULL) ||
		(key == NULL) ||
		(key_found == NULL))
		return -EINVAL;

	pos = rte_hash_del_key(t->h_table, key);
	if (pos >= 0) {
		*key_found = 1;
		uint8_t *entry_ptr = &t->memory[pos * t->entry_size];

		if (entry)
			memcpy(entry, entry_ptr, t->entry_size);

		memset(&t->memory[pos * t->entry_size], 0, t->entry_size);
		return 0;
	}

	*key_found = 0;
	return pos;
}

static int
rte_table_hash_cuckoo_lookup(void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_hash *t = table;
	uint64_t pkts_mask_out = 0;
	uint32_t i;

	__rte_unused uint32_t n_pkts_in = rte_popcount64(pkts_mask);

	RTE_TABLE_HASH_CUCKOO_STATS_PKTS_IN_ADD(t, n_pkts_in);

	if ((pkts_mask & (pkts_mask + 1)) == 0) {
		const uint8_t *keys[RTE_PORT_IN_BURST_SIZE_MAX];
		int32_t positions[RTE_PORT_IN_BURST_SIZE_MAX], status;

		/* Keys for bulk lookup */
		for (i = 0; i < n_pkts_in; i++)
			keys[i] = RTE_MBUF_METADATA_UINT8_PTR(pkts[i],
				t->key_offset);

		/* Bulk Lookup */
		status = rte_hash_lookup_bulk(t->h_table,
				(const void **) keys,
				n_pkts_in,
				positions);
		if (status == 0) {
			for (i = 0; i < n_pkts_in; i++) {
				if (likely(positions[i] >= 0)) {
					uint64_t pkt_mask = 1LLU << i;

					entries[i] = &t->memory[positions[i]
						* t->entry_size];
					pkts_mask_out |= pkt_mask;
				}
			}
		}
	} else
		for (i = 0; i < (uint32_t)(RTE_PORT_IN_BURST_SIZE_MAX
					- rte_clz64(pkts_mask)); i++) {
			uint64_t pkt_mask = 1LLU << i;

			if (pkt_mask & pkts_mask) {
				struct rte_mbuf *pkt = pkts[i];
				uint8_t *key = RTE_MBUF_METADATA_UINT8_PTR(pkt,
						t->key_offset);
				int pos;

				pos = rte_hash_lookup(t->h_table, key);
				if (likely(pos >= 0)) {
					entries[i] = &t->memory[pos
						* t->entry_size];
					pkts_mask_out |= pkt_mask;
				}
			}
		}

	*lookup_hit_mask = pkts_mask_out;
	RTE_TABLE_HASH_CUCKOO_STATS_PKTS_LOOKUP_MISS(t,
			n_pkts_in - rte_popcount64(pkts_mask_out));

	return 0;

}

static int
rte_table_hash_cuckoo_stats_read(void *table, struct rte_table_stats *stats,
	int clear)
{
	struct rte_table_hash *t = table;

	if (stats != NULL)
		memcpy(stats, &t->stats, sizeof(t->stats));

	if (clear)
		memset(&t->stats, 0, sizeof(t->stats));

	return 0;
}

struct rte_table_ops rte_table_hash_cuckoo_ops = {
	.f_create = rte_table_hash_cuckoo_create,
	.f_free = rte_table_hash_cuckoo_free,
	.f_add = rte_table_hash_cuckoo_entry_add,
	.f_delete = rte_table_hash_cuckoo_entry_delete,
	.f_add_bulk = NULL,
	.f_delete_bulk = NULL,
	.f_lookup = rte_table_hash_cuckoo_lookup,
	.f_stats = rte_table_hash_cuckoo_stats_read,
};
