/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <string.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_log.h>

#include "rte_table_acl.h"

#ifdef RTE_TABLE_STATS_COLLECT

#define RTE_TABLE_ACL_STATS_PKTS_IN_ADD(table, val) \
	table->stats.n_pkts_in += val
#define RTE_TABLE_ACL_STATS_PKTS_LOOKUP_MISS(table, val) \
	table->stats.n_pkts_lookup_miss += val

#else

#define RTE_TABLE_ACL_STATS_PKTS_IN_ADD(table, val)
#define RTE_TABLE_ACL_STATS_PKTS_LOOKUP_MISS(table, val)

#endif

struct rte_table_acl {
	struct rte_table_stats stats;

	/* Low-level ACL table */
	char name[2][RTE_ACL_NAMESIZE];
	struct rte_acl_param acl_params; /* for creating low level acl table */
	struct rte_acl_config cfg; /* Holds the field definitions (metadata) */
	struct rte_acl_ctx *ctx;
	uint32_t name_id;

	/* Input parameters */
	uint32_t n_rules;
	uint32_t entry_size;

	/* Internal tables */
	uint8_t *action_table;
	struct rte_acl_rule **acl_rule_list; /* Array of pointers to rules */
	uint8_t *acl_rule_memory; /* Memory to store the rules */

	/* Memory to store the action table and stack of free entries */
	uint8_t memory[0] __rte_cache_aligned;
};


static void *
rte_table_acl_create(
	void *params,
	int socket_id,
	uint32_t entry_size)
{
	struct rte_table_acl_params *p = params;
	struct rte_table_acl *acl;
	uint32_t action_table_size, acl_rule_list_size, acl_rule_memory_size;
	uint32_t total_size;

	RTE_BUILD_BUG_ON(((sizeof(struct rte_table_acl) % RTE_CACHE_LINE_SIZE)
		!= 0));

	/* Check input parameters */
	if (p == NULL) {
		RTE_LOG(ERR, TABLE, "%s: Invalid value for params\n", __func__);
		return NULL;
	}
	if (p->name == NULL) {
		RTE_LOG(ERR, TABLE, "%s: Invalid value for name\n", __func__);
		return NULL;
	}
	if (p->n_rules == 0) {
		RTE_LOG(ERR, TABLE, "%s: Invalid value for n_rules\n",
			__func__);
		return NULL;
	}
	if ((p->n_rule_fields == 0) ||
	    (p->n_rule_fields > RTE_ACL_MAX_FIELDS)) {
		RTE_LOG(ERR, TABLE, "%s: Invalid value for n_rule_fields\n",
			__func__);
		return NULL;
	}

	entry_size = RTE_ALIGN(entry_size, sizeof(uint64_t));

	/* Memory allocation */
	action_table_size = RTE_CACHE_LINE_ROUNDUP(p->n_rules * entry_size);
	acl_rule_list_size =
		RTE_CACHE_LINE_ROUNDUP(p->n_rules * sizeof(struct rte_acl_rule *));
	acl_rule_memory_size = RTE_CACHE_LINE_ROUNDUP(p->n_rules *
		RTE_ACL_RULE_SZ(p->n_rule_fields));
	total_size = sizeof(struct rte_table_acl) + action_table_size +
		acl_rule_list_size + acl_rule_memory_size;

	acl = rte_zmalloc_socket("TABLE", total_size, RTE_CACHE_LINE_SIZE,
		socket_id);
	if (acl == NULL) {
		RTE_LOG(ERR, TABLE,
			"%s: Cannot allocate %u bytes for ACL table\n",
			__func__, total_size);
		return NULL;
	}

	acl->action_table = &acl->memory[0];
	acl->acl_rule_list =
		(struct rte_acl_rule **) &acl->memory[action_table_size];
	acl->acl_rule_memory = (uint8_t *)
		&acl->memory[action_table_size + acl_rule_list_size];

	/* Initialization of internal fields */
	snprintf(acl->name[0], RTE_ACL_NAMESIZE, "%s_a", p->name);
	snprintf(acl->name[1], RTE_ACL_NAMESIZE, "%s_b", p->name);
	acl->name_id = 1;

	acl->acl_params.name = acl->name[acl->name_id];
	acl->acl_params.socket_id = socket_id;
	acl->acl_params.rule_size = RTE_ACL_RULE_SZ(p->n_rule_fields);
	acl->acl_params.max_rule_num = p->n_rules;

	acl->cfg.num_categories = 1;
	acl->cfg.num_fields = p->n_rule_fields;
	memcpy(&acl->cfg.defs[0], &p->field_format[0],
		p->n_rule_fields * sizeof(struct rte_acl_field_def));

	acl->ctx = NULL;

	acl->n_rules = p->n_rules;
	acl->entry_size = entry_size;

	return acl;
}

static int
rte_table_acl_free(void *table)
{
	struct rte_table_acl *acl = table;

	/* Check input parameters */
	if (table == NULL) {
		RTE_LOG(ERR, TABLE, "%s: table parameter is NULL\n", __func__);
		return -EINVAL;
	}

	/* Free previously allocated resources */
	if (acl->ctx != NULL)
		rte_acl_free(acl->ctx);

	rte_free(acl);

	return 0;
}

RTE_ACL_RULE_DEF(rte_pipeline_acl_rule, RTE_ACL_MAX_FIELDS);

static int
rte_table_acl_build(struct rte_table_acl *acl, struct rte_acl_ctx **acl_ctx)
{
	struct rte_acl_ctx *ctx = NULL;
	uint32_t n_rules, i;
	int status;

	/* Create low level ACL table */
	ctx = rte_acl_create(&acl->acl_params);
	if (ctx == NULL) {
		RTE_LOG(ERR, TABLE, "%s: Cannot create low level ACL table\n",
			__func__);
		return -1;
	}

	/* Add rules to low level ACL table */
	n_rules = 0;
	for (i = 1; i < acl->n_rules; i++) {
		if (acl->acl_rule_list[i] != NULL) {
			status = rte_acl_add_rules(ctx, acl->acl_rule_list[i],
				1);
			if (status != 0) {
				RTE_LOG(ERR, TABLE,
				"%s: Cannot add rule to low level ACL table\n",
					__func__);
				rte_acl_free(ctx);
				return -1;
			}

			n_rules++;
		}
	}

	if (n_rules == 0) {
		rte_acl_free(ctx);
		*acl_ctx = NULL;
		return 0;
	}

	/* Build low level ACl table */
	status = rte_acl_build(ctx, &acl->cfg);
	if (status != 0) {
		RTE_LOG(ERR, TABLE,
			"%s: Cannot build the low level ACL table\n",
			__func__);
		rte_acl_free(ctx);
		return -1;
	}

	*acl_ctx = ctx;
	return 0;
}

static int
rte_table_acl_entry_add(
	void *table,
	void *key,
	void *entry,
	int *key_found,
	void **entry_ptr)
{
	struct rte_table_acl *acl = table;
	struct rte_table_acl_rule_add_params *rule =
		key;
	struct rte_pipeline_acl_rule acl_rule;
	struct rte_acl_rule *rule_location;
	struct rte_acl_ctx *ctx;
	uint32_t free_pos, free_pos_valid, i;
	int status;

	/* Check input parameters */
	if (table == NULL) {
		RTE_LOG(ERR, TABLE, "%s: table parameter is NULL\n", __func__);
		return -EINVAL;
	}
	if (key == NULL) {
		RTE_LOG(ERR, TABLE, "%s: key parameter is NULL\n", __func__);
		return -EINVAL;
	}
	if (entry == NULL) {
		RTE_LOG(ERR, TABLE, "%s: entry parameter is NULL\n", __func__);
		return -EINVAL;
	}
	if (key_found == NULL) {
		RTE_LOG(ERR, TABLE, "%s: key_found parameter is NULL\n",
			__func__);
		return -EINVAL;
	}
	if (entry_ptr == NULL) {
		RTE_LOG(ERR, TABLE, "%s: entry_ptr parameter is NULL\n",
			__func__);
		return -EINVAL;
	}
	if (rule->priority > RTE_ACL_MAX_PRIORITY) {
		RTE_LOG(ERR, TABLE, "%s: Priority is too high\n", __func__);
		return -EINVAL;
	}

	/* Setup rule data structure */
	memset(&acl_rule, 0, sizeof(acl_rule));
	acl_rule.data.category_mask = 1;
	acl_rule.data.priority = RTE_ACL_MAX_PRIORITY - rule->priority;
	acl_rule.data.userdata = 0; /* To be set up later */
	memcpy(&acl_rule.field[0],
		&rule->field_value[0],
		acl->cfg.num_fields * sizeof(struct rte_acl_field));

	/* Look to see if the rule exists already in the table */
	free_pos = 0;
	free_pos_valid = 0;
	for (i = 1; i < acl->n_rules; i++) {
		if (acl->acl_rule_list[i] == NULL) {
			if (free_pos_valid == 0) {
				free_pos = i;
				free_pos_valid = 1;
			}

			continue;
		}

		/* Compare the key fields */
		status = memcmp(&acl->acl_rule_list[i]->field[0],
			&rule->field_value[0],
			acl->cfg.num_fields * sizeof(struct rte_acl_field));

		/* Rule found: update data associated with the rule */
		if (status == 0) {
			*key_found = 1;
			*entry_ptr = &acl->memory[i * acl->entry_size];
			memcpy(*entry_ptr, entry, acl->entry_size);

			return 0;
		}
	}

	/* Return if max rules */
	if (free_pos_valid == 0) {
		RTE_LOG(ERR, TABLE, "%s: Max number of rules reached\n",
			__func__);
		return -ENOSPC;
	}

	/* Add the new rule to the rule set */
	acl_rule.data.userdata = free_pos;
	rule_location = (struct rte_acl_rule *)
		&acl->acl_rule_memory[free_pos * acl->acl_params.rule_size];
	memcpy(rule_location, &acl_rule, acl->acl_params.rule_size);
	acl->acl_rule_list[free_pos] = rule_location;

	/* Build low level ACL table */
	acl->name_id ^= 1;
	acl->acl_params.name = acl->name[acl->name_id];
	status = rte_table_acl_build(acl, &ctx);
	if (status != 0) {
		/* Roll back changes */
		acl->acl_rule_list[free_pos] = NULL;
		acl->name_id ^= 1;

		return -EINVAL;
	}

	/* Commit changes */
	if (acl->ctx != NULL)
		rte_acl_free(acl->ctx);
	acl->ctx = ctx;
	*key_found = 0;
	*entry_ptr = &acl->memory[free_pos * acl->entry_size];
	memcpy(*entry_ptr, entry, acl->entry_size);

	return 0;
}

static int
rte_table_acl_entry_delete(
	void *table,
	void *key,
	int *key_found,
	void *entry)
{
	struct rte_table_acl *acl = table;
	struct rte_table_acl_rule_delete_params *rule =
		key;
	struct rte_acl_rule *deleted_rule = NULL;
	struct rte_acl_ctx *ctx;
	uint32_t pos, pos_valid, i;
	int status;

	/* Check input parameters */
	if (table == NULL) {
		RTE_LOG(ERR, TABLE, "%s: table parameter is NULL\n", __func__);
		return -EINVAL;
	}
	if (key == NULL) {
		RTE_LOG(ERR, TABLE, "%s: key parameter is NULL\n", __func__);
		return -EINVAL;
	}
	if (key_found == NULL) {
		RTE_LOG(ERR, TABLE, "%s: key_found parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	/* Look for the rule in the table */
	pos = 0;
	pos_valid = 0;
	for (i = 1; i < acl->n_rules; i++) {
		if (acl->acl_rule_list[i] != NULL) {
			/* Compare the key fields */
			status = memcmp(&acl->acl_rule_list[i]->field[0],
				&rule->field_value[0], acl->cfg.num_fields *
				sizeof(struct rte_acl_field));

			/* Rule found: remove from table */
			if (status == 0) {
				pos = i;
				pos_valid = 1;

				deleted_rule = acl->acl_rule_list[i];
				acl->acl_rule_list[i] = NULL;
			}
		}
	}

	/* Return if rule not found */
	if (pos_valid == 0) {
		*key_found = 0;
		return 0;
	}

	/* Build low level ACL table */
	acl->name_id ^= 1;
	acl->acl_params.name = acl->name[acl->name_id];
	status = rte_table_acl_build(acl, &ctx);
	if (status != 0) {
		/* Roll back changes */
		acl->acl_rule_list[pos] = deleted_rule;
		acl->name_id ^= 1;

		return -EINVAL;
	}

	/* Commit changes */
	if (acl->ctx != NULL)
		rte_acl_free(acl->ctx);

	acl->ctx = ctx;
	*key_found = 1;
	if (entry != NULL)
		memcpy(entry, &acl->memory[pos * acl->entry_size],
			acl->entry_size);

	return 0;
}

static int
rte_table_acl_entry_add_bulk(
	void *table,
	void **keys,
	void **entries,
	uint32_t n_keys,
	int *key_found,
	void **entries_ptr)
{
	struct rte_table_acl *acl = table;
	struct rte_acl_ctx *ctx;
	uint32_t rule_pos[n_keys];
	uint32_t i;
	int err = 0, build = 0;
	int status;

	/* Check input parameters */
	if (table == NULL) {
		RTE_LOG(ERR, TABLE, "%s: table parameter is NULL\n", __func__);
		return -EINVAL;
	}
	if (keys == NULL) {
		RTE_LOG(ERR, TABLE, "%s: keys parameter is NULL\n", __func__);
		return -EINVAL;
	}
	if (entries == NULL) {
		RTE_LOG(ERR, TABLE, "%s: entries parameter is NULL\n", __func__);
		return -EINVAL;
	}
	if (n_keys == 0) {
		RTE_LOG(ERR, TABLE, "%s: 0 rules to add\n", __func__);
		return -EINVAL;
	}
	if (key_found == NULL) {
		RTE_LOG(ERR, TABLE, "%s: key_found parameter is NULL\n",
			__func__);
		return -EINVAL;
	}
	if (entries_ptr == NULL) {
		RTE_LOG(ERR, TABLE, "%s: entries_ptr parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	/* Check input parameters in arrays */
	for (i = 0; i < n_keys; i++) {
		struct rte_table_acl_rule_add_params *rule;

		if (keys[i] == NULL) {
			RTE_LOG(ERR, TABLE, "%s: keys[%" PRIu32 "] parameter is NULL\n",
					__func__, i);
			return -EINVAL;
		}

		if (entries[i] == NULL) {
			RTE_LOG(ERR, TABLE, "%s: entries[%" PRIu32 "] parameter is NULL\n",
					__func__, i);
			return -EINVAL;
		}

		rule = keys[i];
		if (rule->priority > RTE_ACL_MAX_PRIORITY) {
			RTE_LOG(ERR, TABLE, "%s: Priority is too high\n", __func__);
			return -EINVAL;
		}
	}

	memset(rule_pos, 0, n_keys * sizeof(uint32_t));
	memset(key_found, 0, n_keys * sizeof(int));
	for (i = 0; i < n_keys; i++) {
		struct rte_table_acl_rule_add_params *rule =
				keys[i];
		struct rte_pipeline_acl_rule acl_rule;
		struct rte_acl_rule *rule_location;
		uint32_t free_pos, free_pos_valid, j;

		/* Setup rule data structure */
		memset(&acl_rule, 0, sizeof(acl_rule));
		acl_rule.data.category_mask = 1;
		acl_rule.data.priority = RTE_ACL_MAX_PRIORITY - rule->priority;
		acl_rule.data.userdata = 0; /* To be set up later */
		memcpy(&acl_rule.field[0],
			&rule->field_value[0],
			acl->cfg.num_fields * sizeof(struct rte_acl_field));

		/* Look to see if the rule exists already in the table */
		free_pos = 0;
		free_pos_valid = 0;
		for (j = 1; j < acl->n_rules; j++) {
			if (acl->acl_rule_list[j] == NULL) {
				if (free_pos_valid == 0) {
					free_pos = j;
					free_pos_valid = 1;
				}

				continue;
			}

			/* Compare the key fields */
			status = memcmp(&acl->acl_rule_list[j]->field[0],
				&rule->field_value[0],
				acl->cfg.num_fields * sizeof(struct rte_acl_field));

			/* Rule found: update data associated with the rule */
			if (status == 0) {
				key_found[i] = 1;
				entries_ptr[i] = &acl->memory[j * acl->entry_size];
				memcpy(entries_ptr[i], entries[i], acl->entry_size);

				break;
			}
		}

		/* Key already in the table */
		if (key_found[i] != 0)
			continue;

		/* Maximum number of rules reached */
		if (free_pos_valid == 0) {
			err = 1;
			break;
		}

		/* Add the new rule to the rule set */
		acl_rule.data.userdata = free_pos;
		rule_location = (struct rte_acl_rule *)
			&acl->acl_rule_memory[free_pos * acl->acl_params.rule_size];
		memcpy(rule_location, &acl_rule, acl->acl_params.rule_size);
		acl->acl_rule_list[free_pos] = rule_location;
		rule_pos[i] = free_pos;
		build = 1;
	}

	if (err != 0) {
		for (i = 0; i < n_keys; i++) {
			if (rule_pos[i] == 0)
				continue;

			acl->acl_rule_list[rule_pos[i]] = NULL;
		}

		return -ENOSPC;
	}

	if (build == 0)
		return 0;

	/* Build low level ACL table */
	acl->name_id ^= 1;
	acl->acl_params.name = acl->name[acl->name_id];
	status = rte_table_acl_build(acl, &ctx);
	if (status != 0) {
		/* Roll back changes */
		for (i = 0; i < n_keys; i++) {
			if (rule_pos[i] == 0)
				continue;

			acl->acl_rule_list[rule_pos[i]] = NULL;
		}
		acl->name_id ^= 1;

		return -EINVAL;
	}

	/* Commit changes */
	if (acl->ctx != NULL)
		rte_acl_free(acl->ctx);
	acl->ctx = ctx;

	for (i = 0; i < n_keys; i++) {
		if (rule_pos[i] == 0)
			continue;

		key_found[i] = 0;
		entries_ptr[i] = &acl->memory[rule_pos[i] * acl->entry_size];
		memcpy(entries_ptr[i], entries[i], acl->entry_size);
	}

	return 0;
}

static int
rte_table_acl_entry_delete_bulk(
	void *table,
	void **keys,
	uint32_t n_keys,
	int *key_found,
	void **entries)
{
	struct rte_table_acl *acl = table;
	struct rte_acl_rule *deleted_rules[n_keys];
	uint32_t rule_pos[n_keys];
	struct rte_acl_ctx *ctx;
	uint32_t i;
	int status;
	int build = 0;

	/* Check input parameters */
	if (table == NULL) {
		RTE_LOG(ERR, TABLE, "%s: table parameter is NULL\n", __func__);
		return -EINVAL;
	}
	if (keys == NULL) {
		RTE_LOG(ERR, TABLE, "%s: key parameter is NULL\n", __func__);
		return -EINVAL;
	}
	if (n_keys == 0) {
		RTE_LOG(ERR, TABLE, "%s: 0 rules to delete\n", __func__);
		return -EINVAL;
	}
	if (key_found == NULL) {
		RTE_LOG(ERR, TABLE, "%s: key_found parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	for (i = 0; i < n_keys; i++) {
		if (keys[i] == NULL) {
			RTE_LOG(ERR, TABLE, "%s: keys[%" PRIu32 "] parameter is NULL\n",
					__func__, i);
			return -EINVAL;
		}
	}

	memset(deleted_rules, 0, n_keys * sizeof(struct rte_acl_rule *));
	memset(rule_pos, 0, n_keys * sizeof(uint32_t));
	for (i = 0; i < n_keys; i++) {
		struct rte_table_acl_rule_delete_params *rule =
			keys[i];
		uint32_t pos_valid, j;

		/* Look for the rule in the table */
		pos_valid = 0;
		for (j = 1; j < acl->n_rules; j++) {
			if (acl->acl_rule_list[j] == NULL)
				continue;

			/* Compare the key fields */
			status = memcmp(&acl->acl_rule_list[j]->field[0],
					&rule->field_value[0],
					acl->cfg.num_fields * sizeof(struct rte_acl_field));

			/* Rule found: remove from table */
			if (status == 0) {
				pos_valid = 1;

				deleted_rules[i] = acl->acl_rule_list[j];
				acl->acl_rule_list[j] = NULL;
				rule_pos[i] = j;

				build = 1;
			}
		}

		if (pos_valid == 0) {
			key_found[i] = 0;
			continue;
		}
	}

	/* Return if no changes to acl table */
	if (build == 0) {
		return 0;
	}

	/* Build low level ACL table */
	acl->name_id ^= 1;
	acl->acl_params.name = acl->name[acl->name_id];
	status = rte_table_acl_build(acl, &ctx);
	if (status != 0) {
		/* Roll back changes */
		for (i = 0; i < n_keys; i++) {
			if (rule_pos[i] == 0)
				continue;

			acl->acl_rule_list[rule_pos[i]] = deleted_rules[i];
		}

		acl->name_id ^= 1;

		return -EINVAL;
	}

	/* Commit changes */
	if (acl->ctx != NULL)
		rte_acl_free(acl->ctx);

	acl->ctx = ctx;
	for (i = 0; i < n_keys; i++) {
		if (rule_pos[i] == 0)
			continue;

		key_found[i] = 1;
		if (entries != NULL && entries[i] != NULL)
			memcpy(entries[i], &acl->memory[rule_pos[i] * acl->entry_size],
					acl->entry_size);
	}

	return 0;
}

static int
rte_table_acl_lookup(
	void *table,
	struct rte_mbuf **pkts,
	uint64_t pkts_mask,
	uint64_t *lookup_hit_mask,
	void **entries)
{
	struct rte_table_acl *acl = (struct rte_table_acl *) table;
	const uint8_t *pkts_data[RTE_PORT_IN_BURST_SIZE_MAX];
	uint32_t results[RTE_PORT_IN_BURST_SIZE_MAX];
	uint64_t pkts_out_mask;
	uint32_t n_pkts, i, j;

	__rte_unused uint32_t n_pkts_in = __builtin_popcountll(pkts_mask);
	RTE_TABLE_ACL_STATS_PKTS_IN_ADD(acl, n_pkts_in);

	/* Input conversion */
	for (i = 0, j = 0; i < (uint32_t)(RTE_PORT_IN_BURST_SIZE_MAX -
		__builtin_clzll(pkts_mask)); i++) {
		uint64_t pkt_mask = 1LLU << i;

		if (pkt_mask & pkts_mask) {
			pkts_data[j] = rte_pktmbuf_mtod(pkts[i], uint8_t *);
			j++;
		}
	}
	n_pkts = j;

	/* Low-level ACL table lookup */
	if (acl->ctx != NULL)
		rte_acl_classify(acl->ctx, pkts_data, results, n_pkts, 1);
	else
		n_pkts = 0;

	/* Output conversion */
	pkts_out_mask = 0;
	for (i = 0; i < n_pkts; i++) {
		uint32_t action_table_pos = results[i];
		uint32_t pkt_pos = __builtin_ctzll(pkts_mask);
		uint64_t pkt_mask = 1LLU << pkt_pos;

		pkts_mask &= ~pkt_mask;

		if (action_table_pos != 0) {
			pkts_out_mask |= pkt_mask;
			entries[pkt_pos] = (void *)
				&acl->memory[action_table_pos *
				acl->entry_size];
			rte_prefetch0(entries[pkt_pos]);
		}
	}

	*lookup_hit_mask = pkts_out_mask;
	RTE_TABLE_ACL_STATS_PKTS_LOOKUP_MISS(acl, n_pkts_in - __builtin_popcountll(pkts_out_mask));

	return 0;
}

static int
rte_table_acl_stats_read(void *table, struct rte_table_stats *stats, int clear)
{
	struct rte_table_acl *acl = table;

	if (stats != NULL)
		memcpy(stats, &acl->stats, sizeof(acl->stats));

	if (clear)
		memset(&acl->stats, 0, sizeof(acl->stats));

	return 0;
}

struct rte_table_ops rte_table_acl_ops = {
	.f_create = rte_table_acl_create,
	.f_free = rte_table_acl_free,
	.f_add = rte_table_acl_entry_add,
	.f_delete = rte_table_acl_entry_delete,
	.f_add_bulk = rte_table_acl_entry_add_bulk,
	.f_delete_bulk = rte_table_acl_entry_delete_bulk,
	.f_lookup = rte_table_acl_lookup,
	.f_stats = rte_table_acl_stats_read,
};
