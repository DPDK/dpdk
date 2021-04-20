/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_prefetch.h>
#include <rte_cycles.h>
#include <rte_acl.h>

#include "rte_swx_table_wm.h"

#ifndef RTE_SWX_TABLE_EM_USE_HUGE_PAGES
#define RTE_SWX_TABLE_EM_USE_HUGE_PAGES 1
#endif

#if RTE_SWX_TABLE_EM_USE_HUGE_PAGES

#include <rte_malloc.h>

static void *
env_malloc(size_t size, size_t alignment, int numa_node)
{
	return rte_zmalloc_socket(NULL, size, alignment, numa_node);
}

static void
env_free(void *start, size_t size __rte_unused)
{
	rte_free(start);
}

#else

#include <numa.h>

static void *
env_malloc(size_t size, size_t alignment __rte_unused, int numa_node)
{
	return numa_alloc_onnode(size, numa_node);
}

static void
env_free(void *start, size_t size)
{
	numa_free(start, size);
}

#endif

static char *get_unique_name(void)
{
	uint64_t tsc = rte_get_tsc_cycles();
	size_t size = sizeof(uint64_t) * 2 + 1;
	char *name = calloc(1, size);

	if (!name)
		return NULL;

	snprintf(name, size, "%016" PRIx64, tsc);
	return name;
}

static uint32_t
count_entries(struct rte_swx_table_entry_list *entries)
{
	struct rte_swx_table_entry *entry;
	uint32_t n_entries = 0;

	if (!entries)
		return 0;

	TAILQ_FOREACH(entry, entries, node)
		n_entries++;

	return n_entries;
}

static int
acl_table_cfg_get(struct rte_acl_config *cfg, struct rte_swx_table_params *p)
{
	uint32_t byte_id = 0, field_id = 0;

	/* cfg->num_categories. */
	cfg->num_categories = 1;

	/* cfg->defs and cfg->num_fields. */
	for (byte_id = 0; byte_id < p->key_size; ) {
		uint32_t field_size = field_id ? 4 : 1;
		uint8_t byte = p->key_mask0 ? p->key_mask0[byte_id] : 0xFF;

		if (!byte) {
			byte_id++;
			continue;
		}

		if (field_id == RTE_ACL_MAX_FIELDS)
			return -1;

		cfg->defs[field_id].type = RTE_ACL_FIELD_TYPE_BITMASK;
		cfg->defs[field_id].size = field_size;
		cfg->defs[field_id].field_index = field_id;
		cfg->defs[field_id].input_index = field_id;
		cfg->defs[field_id].offset = p->key_offset + byte_id;

		field_id++;
		byte_id += field_size;
	}

	if (!field_id)
		return -1;

	cfg->num_fields = field_id;

	/* cfg->max_size. */
	cfg->max_size = 0;

	return 0;
}

static void
acl_table_rule_field8(uint8_t *value,
	uint8_t *mask,
	uint8_t *key_mask0,
	uint8_t *key_mask,
	uint8_t *key,
	uint32_t offset)
{
	uint8_t km0, km;

	km0 = key_mask0 ? key_mask0[offset] : 0xFF;
	km = key_mask ? key_mask[offset] : 0xFF;

	*value = key[offset];
	*mask = km0 & km;
}

static void
acl_table_rule_field32(uint32_t *value,
	uint32_t *mask,
	uint8_t *key_mask0,
	uint8_t *key_mask,
	uint8_t *key,
	uint32_t key_size,
	uint32_t offset)
{
	uint32_t km0[4], km[4], k[4];
	uint32_t byte_id;

	/* Byte 0 = MSB, byte 3 = LSB. */
	for (byte_id = 0; byte_id < 4; byte_id++) {
		if (offset + byte_id >= key_size) {
			km0[byte_id] = 0;
			km[byte_id] = 0;
			k[byte_id] = 0;
			continue;
		}

		km0[byte_id] = key_mask0 ? key_mask0[offset + byte_id] : 0xFF;
		km[byte_id] = key_mask ? key_mask[offset + byte_id] : 0xFF;
		k[byte_id] = key[offset + byte_id];
	}

	*value = (k[0] << 24) |
		 (k[1] << 16) |
		 (k[2] << 8) |
		 k[3];

	*mask = ((km[0] & km0[0]) << 24) |
		((km[1] & km0[1]) << 16) |
		((km[2] & km0[2]) << 8) |
		(km[3] & km0[3]);
}

RTE_ACL_RULE_DEF(acl_rule, RTE_ACL_MAX_FIELDS);

static struct rte_acl_rule *
acl_table_rules_get(struct rte_acl_config *acl_cfg,
	struct rte_swx_table_params *p,
	struct rte_swx_table_entry_list *entries,
	uint32_t n_entries)
{
	struct rte_swx_table_entry *entry;
	uint8_t *memory;
	uint32_t acl_rule_size = RTE_ACL_RULE_SZ(acl_cfg->num_fields);
	uint32_t n_fields = acl_cfg->num_fields;
	uint32_t rule_id;

	if (!n_entries)
		return NULL;

	memory = malloc(n_entries * acl_rule_size);
	if (!memory)
		return NULL;

	rule_id = 0;
	TAILQ_FOREACH(entry, entries, node) {
		uint8_t *m = &memory[rule_id * acl_rule_size];
		struct acl_rule *acl_rule = (struct acl_rule *)m;
		uint32_t field_id;

		acl_rule->data.category_mask = 1;
		acl_rule->data.priority = RTE_ACL_MAX_PRIORITY -
			entry->key_priority;
		acl_rule->data.userdata = rule_id + 1;

		for (field_id = 0; field_id < n_fields; field_id++) {
			struct rte_acl_field *f = &acl_rule->field[field_id];
			uint32_t size = acl_cfg->defs[field_id].size;
			uint32_t offset = acl_cfg->defs[field_id].offset -
				p->key_offset;

			if (size == 1) {
				uint8_t value, mask;

				acl_table_rule_field8(&value,
						      &mask,
						      p->key_mask0,
						      entry->key_mask,
						      entry->key,
						      offset);

				f->value.u8 = value;
				f->mask_range.u8 = mask;
			} else {
				uint32_t value, mask;

				acl_table_rule_field32(&value,
						       &mask,
						       p->key_mask0,
						       entry->key_mask,
						       entry->key,
						       p->key_size,
						       offset);

				f->value.u32 = value;
				f->mask_range.u32 = mask;
			}
		}

		rule_id++;
	}

	return (struct rte_acl_rule *)memory;
}

/* When the table to be created has no rules, the expected behavior is to always
 * get lookup miss for any input key. To achieve this, we add a single bogus
 * rule to the table with the rule user data set to 0, i.e. the value returned
 * when lookup miss takes place. Whether lookup hit (the bogus rule is hit) or
 * miss, a user data of 0 is returned, which for the ACL library is equivalent
 * to lookup miss.
 */
static struct rte_acl_rule *
acl_table_rules_default_get(struct rte_acl_config *acl_cfg)
{
	struct rte_acl_rule *acl_rule;
	uint32_t acl_rule_size = RTE_ACL_RULE_SZ(acl_cfg->num_fields);

	acl_rule = calloc(1, acl_rule_size);
	if (!acl_rule)
		return NULL;

	acl_rule->data.category_mask = 1;
	acl_rule->data.priority = RTE_ACL_MAX_PRIORITY;
	acl_rule->data.userdata = 0;

	memset(&acl_rule[1], 0xFF, acl_rule_size - sizeof(struct rte_acl_rule));

	return acl_rule;
}

static struct rte_acl_ctx *
acl_table_create(struct rte_swx_table_params *params,
	struct rte_swx_table_entry_list *entries,
	uint32_t n_entries,
	int numa_node)
{
	struct rte_acl_param acl_params = {0};
	struct rte_acl_config acl_cfg = {0};
	struct rte_acl_ctx *acl_ctx = NULL;
	struct rte_acl_rule *acl_rules = NULL;
	char *name = NULL;
	int status = 0;

	/* ACL config data structures. */
	name = get_unique_name();
	if (!name) {
		status = -1;
		goto free_resources;
	}

	status = acl_table_cfg_get(&acl_cfg, params);
	if (status)
		goto free_resources;

	acl_rules = n_entries ?
		acl_table_rules_get(&acl_cfg, params, entries, n_entries) :
		acl_table_rules_default_get(&acl_cfg);
	if (!acl_rules) {
		status = -1;
		goto free_resources;
	}

	n_entries = n_entries ? n_entries : 1;

	/* ACL create. */
	acl_params.name = name;
	acl_params.socket_id = numa_node;
	acl_params.rule_size = RTE_ACL_RULE_SZ(acl_cfg.num_fields);
	acl_params.max_rule_num = n_entries;

	acl_ctx = rte_acl_create(&acl_params);
	if (!acl_ctx) {
		status = -1;
		goto free_resources;
	}

	/* ACL add rules. */
	status = rte_acl_add_rules(acl_ctx, acl_rules, n_entries);
	if (status)
		goto free_resources;

	/* ACL build. */
	status = rte_acl_build(acl_ctx, &acl_cfg);

free_resources:
	if (status && acl_ctx)
		rte_acl_free(acl_ctx);

	free(acl_rules);

	free(name);

	return status ? NULL : acl_ctx;
}

static void
entry_data_copy(uint8_t *data,
	struct rte_swx_table_entry_list *entries,
	uint32_t n_entries,
	uint32_t entry_data_size)
{
	struct rte_swx_table_entry *entry;
	uint32_t i = 0;

	if (!n_entries)
		return;

	TAILQ_FOREACH(entry, entries, node) {
		uint64_t *d = (uint64_t *)&data[i * entry_data_size];

		d[0] = entry->action_id;
		memcpy(&d[1], entry->action_data, entry_data_size - 8);

		i++;
	}
}

struct table {
	struct rte_acl_ctx *acl_ctx;
	uint8_t *data;
	size_t total_size;
	uint32_t entry_data_size;
};

static void
table_free(void *table)
{
	struct table *t = table;

	if (!t)
		return;

	if (t->acl_ctx)
		rte_acl_free(t->acl_ctx);
	env_free(t, t->total_size);
}

static void *
table_create(struct rte_swx_table_params *params,
	     struct rte_swx_table_entry_list *entries,
	     const char *args __rte_unused,
	     int numa_node)
{
	struct table *t = NULL;
	size_t meta_sz, data_sz, total_size;
	uint32_t entry_data_size;
	uint32_t n_entries = count_entries(entries);

	/* Check input arguments. */
	if (!params || !params->key_size)
		goto error;

	/* Memory allocation and initialization. */
	entry_data_size = 8 + params->action_data_size;
	meta_sz = sizeof(struct table);
	data_sz = n_entries * entry_data_size;
	total_size = meta_sz + data_sz;

	t = env_malloc(total_size, RTE_CACHE_LINE_SIZE, numa_node);
	if (!t)
		goto error;

	memset(t, 0, total_size);
	t->entry_data_size = entry_data_size;
	t->total_size = total_size;
	t->data = (uint8_t *)&t[1];

	t->acl_ctx = acl_table_create(params, entries, n_entries, numa_node);
	if (!t->acl_ctx)
		goto error;

	entry_data_copy(t->data, entries, n_entries, entry_data_size);

	return t;

error:
	table_free(t);
	return NULL;
}

struct mailbox {

};

static uint64_t
table_mailbox_size_get(void)
{
	return sizeof(struct mailbox);
}

static int
table_lookup(void *table,
	     void *mailbox __rte_unused,
	     const uint8_t **key,
	     uint64_t *action_id,
	     uint8_t **action_data,
	     int *hit)
{
	struct table *t = table;
	uint8_t *data;
	uint32_t user_data;

	rte_acl_classify(t->acl_ctx, key, &user_data, 1, 1);
	if (!user_data) {
		*hit = 0;
		return 1;
	}

	data = &t->data[(user_data - 1) * t->entry_data_size];
	*action_id = ((uint64_t *)data)[0];
	*action_data = &data[8];
	*hit = 1;
	return 1;
}

struct rte_swx_table_ops rte_swx_table_wildcard_match_ops = {
	.footprint_get = NULL,
	.mailbox_size_get = table_mailbox_size_get,
	.create = table_create,
	.add = NULL,
	.del = NULL,
	.lkp = (rte_swx_table_lookup_t)table_lookup,
	.free = table_free,
};
