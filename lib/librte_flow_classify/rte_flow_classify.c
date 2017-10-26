/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#include <rte_flow_classify.h>
#include "rte_flow_classify_parse.h"
#include <rte_flow_driver.h>
#include <rte_table_acl.h>
#include <stdbool.h>

int librte_flow_classify_logtype;

static struct rte_eth_ntuple_filter ntuple_filter;
static uint32_t unique_id = 1;


struct rte_flow_classify_table_entry {
	/* meta-data for classify rule */
	uint32_t rule_id;
};

struct rte_table {
	/* Input parameters */
	struct rte_table_ops ops;
	uint32_t entry_size;
	enum rte_flow_classify_table_type type;

	/* Handle to the low-level table object */
	void *h_table;
};

#define RTE_FLOW_CLASSIFIER_MAX_NAME_SZ 256

struct rte_flow_classifier {
	/* Input parameters */
	char name[RTE_FLOW_CLASSIFIER_MAX_NAME_SZ];
	int socket_id;
	enum rte_flow_classify_table_type type;

	/* Internal tables */
	struct rte_table tables[RTE_FLOW_CLASSIFY_TABLE_MAX];
	uint32_t num_tables;
	uint16_t nb_pkts;
	struct rte_flow_classify_table_entry
		*entries[RTE_PORT_IN_BURST_SIZE_MAX];
} __rte_cache_aligned;

enum {
	PROTO_FIELD_IPV4,
	SRC_FIELD_IPV4,
	DST_FIELD_IPV4,
	SRCP_FIELD_IPV4,
	DSTP_FIELD_IPV4,
	NUM_FIELDS_IPV4
};

struct acl_keys {
	struct rte_table_acl_rule_add_params key_add; /* add key */
	struct rte_table_acl_rule_delete_params	key_del; /* delete key */
};

struct classify_rules {
	enum rte_flow_classify_rule_type type;
	union {
		struct rte_flow_classify_ipv4_5tuple ipv4_5tuple;
	} u;
};

struct rte_flow_classify_rule {
	uint32_t id; /* unique ID of classify rule */
	struct rte_flow_action action; /* action when match found */
	struct classify_rules rules; /* union of rules */
	union {
		struct acl_keys key;
	} u;
	int key_found;   /* rule key found in table */
	void *entry;     /* pointer to buffer to hold rule meta data */
	void *entry_ptr; /* handle to the table entry for rule meta data */
};

static int
flow_classify_parse_flow(
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct rte_flow_item *items;
	parse_filter_t parse_filter;
	uint32_t item_num = 0;
	uint32_t i = 0;
	int ret;

	memset(&ntuple_filter, 0, sizeof(ntuple_filter));

	/* Get the non-void item number of pattern */
	while ((pattern + i)->type != RTE_FLOW_ITEM_TYPE_END) {
		if ((pattern + i)->type != RTE_FLOW_ITEM_TYPE_VOID)
			item_num++;
		i++;
	}
	item_num++;

	items = malloc(item_num * sizeof(struct rte_flow_item));
	if (!items) {
		rte_flow_error_set(error, ENOMEM,
				RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				NULL, "No memory for pattern items.");
		return -ENOMEM;
	}

	memset(items, 0, item_num * sizeof(struct rte_flow_item));
	classify_pattern_skip_void_item(items, pattern);

	parse_filter = classify_find_parse_filter_func(items);
	if (!parse_filter) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				pattern, "Unsupported pattern");
		free(items);
		return -EINVAL;
	}

	ret = parse_filter(attr, items, actions, &ntuple_filter, error);
	free(items);
	return ret;
}


#define uint32_t_to_char(ip, a, b, c, d) do {\
		*a = (unsigned char)(ip >> 24 & 0xff);\
		*b = (unsigned char)(ip >> 16 & 0xff);\
		*c = (unsigned char)(ip >> 8 & 0xff);\
		*d = (unsigned char)(ip & 0xff);\
	} while (0)

static inline void
print_acl_ipv4_key_add(struct rte_table_acl_rule_add_params *key)
{
	unsigned char a, b, c, d;

	printf("%s:    0x%02hhx/0x%hhx ", __func__,
		key->field_value[PROTO_FIELD_IPV4].value.u8,
		key->field_value[PROTO_FIELD_IPV4].mask_range.u8);

	uint32_t_to_char(key->field_value[SRC_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	printf(" %hhu.%hhu.%hhu.%hhu/0x%x ", a, b, c, d,
			key->field_value[SRC_FIELD_IPV4].mask_range.u32);

	uint32_t_to_char(key->field_value[DST_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	printf("%hhu.%hhu.%hhu.%hhu/0x%x ", a, b, c, d,
			key->field_value[DST_FIELD_IPV4].mask_range.u32);

	printf("%hu : 0x%x %hu : 0x%x",
		key->field_value[SRCP_FIELD_IPV4].value.u16,
		key->field_value[SRCP_FIELD_IPV4].mask_range.u16,
		key->field_value[DSTP_FIELD_IPV4].value.u16,
		key->field_value[DSTP_FIELD_IPV4].mask_range.u16);

	printf(" priority: 0x%x\n", key->priority);
}

static inline void
print_acl_ipv4_key_delete(struct rte_table_acl_rule_delete_params *key)
{
	unsigned char a, b, c, d;

	printf("%s: 0x%02hhx/0x%hhx ", __func__,
		key->field_value[PROTO_FIELD_IPV4].value.u8,
		key->field_value[PROTO_FIELD_IPV4].mask_range.u8);

	uint32_t_to_char(key->field_value[SRC_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	printf(" %hhu.%hhu.%hhu.%hhu/0x%x ", a, b, c, d,
			key->field_value[SRC_FIELD_IPV4].mask_range.u32);

	uint32_t_to_char(key->field_value[DST_FIELD_IPV4].value.u32,
			&a, &b, &c, &d);
	printf("%hhu.%hhu.%hhu.%hhu/0x%x ", a, b, c, d,
			key->field_value[DST_FIELD_IPV4].mask_range.u32);

	printf("%hu : 0x%x %hu : 0x%x\n",
		key->field_value[SRCP_FIELD_IPV4].value.u16,
		key->field_value[SRCP_FIELD_IPV4].mask_range.u16,
		key->field_value[DSTP_FIELD_IPV4].value.u16,
		key->field_value[DSTP_FIELD_IPV4].mask_range.u16);
}

static int
rte_flow_classifier_check_params(struct rte_flow_classifier_params *params)
{
	if (params == NULL) {
		RTE_FLOW_CLASSIFY_LOG(ERR,
			"%s: Incorrect value for parameter params\n", __func__);
		return -EINVAL;
	}

	/* name */
	if (params->name == NULL) {
		RTE_FLOW_CLASSIFY_LOG(ERR,
			"%s: Incorrect value for parameter name\n", __func__);
		return -EINVAL;
	}

	/* socket */
	if ((params->socket_id < 0) ||
	    (params->socket_id >= RTE_MAX_NUMA_NODES)) {
		RTE_FLOW_CLASSIFY_LOG(ERR,
			"%s: Incorrect value for parameter socket_id\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

struct rte_flow_classifier *
rte_flow_classifier_create(struct rte_flow_classifier_params *params)
{
	struct rte_flow_classifier *cls;
	int ret;

	/* Check input parameters */
	ret = rte_flow_classifier_check_params(params);
	if (ret != 0) {
		RTE_FLOW_CLASSIFY_LOG(ERR,
			"%s: flow classifier params check failed (%d)\n",
			__func__, ret);
		return NULL;
	}

	/* Allocate memory for the flow classifier */
	cls = rte_zmalloc_socket("FLOW_CLASSIFIER",
			sizeof(struct rte_flow_classifier),
			RTE_CACHE_LINE_SIZE, params->socket_id);

	if (cls == NULL) {
		RTE_FLOW_CLASSIFY_LOG(ERR,
			"%s: flow classifier memory allocation failed\n",
			__func__);
		return NULL;
	}

	/* Save input parameters */
	snprintf(cls->name, RTE_FLOW_CLASSIFIER_MAX_NAME_SZ, "%s",
			params->name);
	cls->socket_id = params->socket_id;
	cls->type = params->type;

	/* Initialize flow classifier internal data structure */
	cls->num_tables = 0;

	return cls;
}

static void
rte_flow_classify_table_free(struct rte_table *table)
{
	if (table->ops.f_free != NULL)
		table->ops.f_free(table->h_table);
}

int
rte_flow_classifier_free(struct rte_flow_classifier *cls)
{
	uint32_t i;

	/* Check input parameters */
	if (cls == NULL) {
		RTE_FLOW_CLASSIFY_LOG(ERR,
			"%s: rte_flow_classifier parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	/* Free tables */
	for (i = 0; i < cls->num_tables; i++) {
		struct rte_table *table = &cls->tables[i];

		rte_flow_classify_table_free(table);
	}

	/* Free flow classifier memory */
	rte_free(cls);

	return 0;
}

static int
rte_table_check_params(struct rte_flow_classifier *cls,
		struct rte_flow_classify_table_params *params,
		uint32_t *table_id)
{
	if (cls == NULL) {
		RTE_FLOW_CLASSIFY_LOG(ERR,
			"%s: flow classifier parameter is NULL\n",
			__func__);
		return -EINVAL;
	}
	if (params == NULL) {
		RTE_FLOW_CLASSIFY_LOG(ERR, "%s: params parameter is NULL\n",
			__func__);
		return -EINVAL;
	}
	if (table_id == NULL) {
		RTE_FLOW_CLASSIFY_LOG(ERR, "%s: table_id parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	/* ops */
	if (params->ops == NULL) {
		RTE_FLOW_CLASSIFY_LOG(ERR, "%s: params->ops is NULL\n",
			__func__);
		return -EINVAL;
	}

	if (params->ops->f_create == NULL) {
		RTE_FLOW_CLASSIFY_LOG(ERR,
			"%s: f_create function pointer is NULL\n", __func__);
		return -EINVAL;
	}

	if (params->ops->f_lookup == NULL) {
		RTE_FLOW_CLASSIFY_LOG(ERR,
			"%s: f_lookup function pointer is NULL\n", __func__);
		return -EINVAL;
	}

	/* De we have room for one more table? */
	if (cls->num_tables == RTE_FLOW_CLASSIFY_TABLE_MAX) {
		RTE_FLOW_CLASSIFY_LOG(ERR,
			"%s: Incorrect value for num_tables parameter\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}

int
rte_flow_classify_table_create(struct rte_flow_classifier *cls,
	struct rte_flow_classify_table_params *params,
	uint32_t *table_id)
{
	struct rte_table *table;
	void *h_table;
	uint32_t entry_size, id;
	int ret;

	/* Check input arguments */
	ret = rte_table_check_params(cls, params, table_id);
	if (ret != 0)
		return ret;

	id = cls->num_tables;
	table = &cls->tables[id];

	/* calculate table entry size */
	entry_size = sizeof(struct rte_flow_classify_table_entry);

	/* Create the table */
	h_table = params->ops->f_create(params->arg_create, cls->socket_id,
		entry_size);
	if (h_table == NULL) {
		RTE_FLOW_CLASSIFY_LOG(ERR, "%s: Table creation failed\n",
			__func__);
		return -EINVAL;
	}

	/* Commit current table to the classifier */
	cls->num_tables++;
	*table_id = id;

	/* Save input parameters */
	memcpy(&table->ops, params->ops, sizeof(struct rte_table_ops));

	/* Initialize table internal data structure */
	table->entry_size = entry_size;
	table->h_table = h_table;

	return 0;
}

static struct rte_flow_classify_rule *
allocate_acl_ipv4_5tuple_rule(void)
{
	struct rte_flow_classify_rule *rule;
	int log_level;

	rule = malloc(sizeof(struct rte_flow_classify_rule));
	if (!rule)
		return rule;

	memset(rule, 0, sizeof(struct rte_flow_classify_rule));
	rule->id = unique_id++;
	rule->rules.type = RTE_FLOW_CLASSIFY_RULE_TYPE_IPV4_5TUPLE;

	memcpy(&rule->action, classify_get_flow_action(),
	       sizeof(struct rte_flow_action));

	/* key add values */
	rule->u.key.key_add.priority = ntuple_filter.priority;
	rule->u.key.key_add.field_value[PROTO_FIELD_IPV4].mask_range.u8 =
			ntuple_filter.proto_mask;
	rule->u.key.key_add.field_value[PROTO_FIELD_IPV4].value.u8 =
			ntuple_filter.proto;
	rule->rules.u.ipv4_5tuple.proto = ntuple_filter.proto;
	rule->rules.u.ipv4_5tuple.proto_mask = ntuple_filter.proto_mask;

	rule->u.key.key_add.field_value[SRC_FIELD_IPV4].mask_range.u32 =
			ntuple_filter.src_ip_mask;
	rule->u.key.key_add.field_value[SRC_FIELD_IPV4].value.u32 =
			ntuple_filter.src_ip;
	rule->rules.u.ipv4_5tuple.src_ip_mask = ntuple_filter.src_ip_mask;
	rule->rules.u.ipv4_5tuple.src_ip = ntuple_filter.src_ip;

	rule->u.key.key_add.field_value[DST_FIELD_IPV4].mask_range.u32 =
			ntuple_filter.dst_ip_mask;
	rule->u.key.key_add.field_value[DST_FIELD_IPV4].value.u32 =
			ntuple_filter.dst_ip;
	rule->rules.u.ipv4_5tuple.dst_ip_mask = ntuple_filter.dst_ip_mask;
	rule->rules.u.ipv4_5tuple.dst_ip = ntuple_filter.dst_ip;

	rule->u.key.key_add.field_value[SRCP_FIELD_IPV4].mask_range.u16 =
			ntuple_filter.src_port_mask;
	rule->u.key.key_add.field_value[SRCP_FIELD_IPV4].value.u16 =
			ntuple_filter.src_port;
	rule->rules.u.ipv4_5tuple.src_port_mask = ntuple_filter.src_port_mask;
	rule->rules.u.ipv4_5tuple.src_port = ntuple_filter.src_port;

	rule->u.key.key_add.field_value[DSTP_FIELD_IPV4].mask_range.u16 =
			ntuple_filter.dst_port_mask;
	rule->u.key.key_add.field_value[DSTP_FIELD_IPV4].value.u16 =
			ntuple_filter.dst_port;
	rule->rules.u.ipv4_5tuple.dst_port_mask = ntuple_filter.dst_port_mask;
	rule->rules.u.ipv4_5tuple.dst_port = ntuple_filter.dst_port;

	log_level = rte_log_get_level(librte_flow_classify_logtype);

	if (log_level == RTE_LOG_DEBUG)
		print_acl_ipv4_key_add(&rule->u.key.key_add);

	/* key delete values */
	memcpy(&rule->u.key.key_del.field_value[PROTO_FIELD_IPV4],
	       &rule->u.key.key_add.field_value[PROTO_FIELD_IPV4],
	       NUM_FIELDS_IPV4 * sizeof(struct rte_acl_field));

	if (log_level == RTE_LOG_DEBUG)
		print_acl_ipv4_key_delete(&rule->u.key.key_del);

	return rule;
}

struct rte_flow_classify_rule *
rte_flow_classify_table_entry_add(struct rte_flow_classifier *cls,
		uint32_t table_id,
		int *key_found,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct rte_flow_classify_rule *rule;
	struct rte_flow_classify_table_entry *table_entry;
	int ret;

	if (!error)
		return NULL;

	if (!cls) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "NULL classifier.");
		return NULL;
	}

	if (table_id >= cls->num_tables) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "invalid table_id.");
		return NULL;
	}

	if (key_found == NULL) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "NULL key_found.");
		return NULL;
	}

	if (!pattern) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				NULL, "NULL pattern.");
		return NULL;
	}

	if (!actions) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				NULL, "NULL action.");
		return NULL;
	}

	if (!attr) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR,
				NULL, "NULL attribute.");
		return NULL;
	}

	/* parse attr, pattern and actions */
	ret = flow_classify_parse_flow(attr, pattern, actions, error);
	if (ret < 0)
		return NULL;

	switch (cls->type) {
	case RTE_FLOW_CLASSIFY_TABLE_TYPE_ACL:
		rule = allocate_acl_ipv4_5tuple_rule();
		if (!rule)
			return NULL;
		break;
	default:
		return NULL;
	}

	rule->entry = malloc(sizeof(struct rte_flow_classify_table_entry));
	if (!rule->entry) {
		free(rule);
		return NULL;
	}

	table_entry = rule->entry;
	table_entry->rule_id = rule->id;

	if (cls->tables[table_id].ops.f_add != NULL) {
		ret = cls->tables[table_id].ops.f_add(
			cls->tables[table_id].h_table,
			&rule->u.key.key_add,
			rule->entry,
			&rule->key_found,
			&rule->entry_ptr);
		if (ret) {
			free(rule->entry);
			free(rule);
			return NULL;
		}
		*key_found = rule->key_found;
	}
	return rule;
}

int
rte_flow_classify_table_entry_delete(struct rte_flow_classifier *cls,
		uint32_t table_id,
		struct rte_flow_classify_rule *rule)
{
	int ret = -EINVAL;

	if (!cls || !rule || table_id >= cls->num_tables)
		return ret;

	if (cls->tables[table_id].ops.f_delete != NULL)
		ret = cls->tables[table_id].ops.f_delete(
			cls->tables[table_id].h_table,
			&rule->u.key.key_del,
			&rule->key_found,
			&rule->entry);

	return ret;
}

static int
flow_classifier_lookup(struct rte_flow_classifier *cls,
		uint32_t table_id,
		struct rte_mbuf **pkts,
		const uint16_t nb_pkts)
{
	int ret = -EINVAL;
	uint64_t pkts_mask;
	uint64_t lookup_hit_mask;

	pkts_mask = RTE_LEN2MASK(nb_pkts, uint64_t);
	ret = cls->tables[table_id].ops.f_lookup(
		cls->tables[table_id].h_table,
		pkts, pkts_mask, &lookup_hit_mask,
		(void **)cls->entries);

	if (!ret && lookup_hit_mask)
		cls->nb_pkts = nb_pkts;
	else
		cls->nb_pkts = 0;

	return ret;
}

static int
action_apply(struct rte_flow_classifier *cls,
		struct rte_flow_classify_rule *rule,
		struct rte_flow_classify_stats *stats)
{
	struct rte_flow_classify_ipv4_5tuple_stats *ntuple_stats;
	uint64_t count = 0;
	int i;
	int ret = -EINVAL;

	switch (rule->action.type) {
	case RTE_FLOW_ACTION_TYPE_COUNT:
		for (i = 0; i < cls->nb_pkts; i++) {
			if (rule->id == cls->entries[i]->rule_id)
				count++;
		}
		if (count) {
			ret = 0;
			ntuple_stats =
				(struct rte_flow_classify_ipv4_5tuple_stats *)
				stats->stats;
			ntuple_stats->counter1 = count;
			ntuple_stats->ipv4_5tuple = rule->rules.u.ipv4_5tuple;
		}
		break;
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}

int
rte_flow_classifier_query(struct rte_flow_classifier *cls,
		uint32_t table_id,
		struct rte_mbuf **pkts,
		const uint16_t nb_pkts,
		struct rte_flow_classify_rule *rule,
		struct rte_flow_classify_stats *stats)
{
	int ret = -EINVAL;

	if (!cls || !rule || !stats || !pkts  || nb_pkts == 0 ||
		table_id >= cls->num_tables)
		return ret;

	ret = flow_classifier_lookup(cls, table_id, pkts, nb_pkts);
	if (!ret)
		ret = action_apply(cls, rule, stats);
	return ret;
}

RTE_INIT(librte_flow_classify_init_log);

static void
librte_flow_classify_init_log(void)
{
	librte_flow_classify_logtype =
		rte_log_register("librte.flow_classify");
	if (librte_flow_classify_logtype >= 0)
		rte_log_set_level(librte_flow_classify_logtype, RTE_LOG_INFO);
}
