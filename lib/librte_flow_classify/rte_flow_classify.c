/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <rte_string_fns.h>
#include <rte_compat.h>
#include <rte_flow_classify.h>
#include "rte_flow_classify_parse.h"
#include <rte_flow_driver.h>
#include <rte_table_acl.h>
#include <stdbool.h>

static uint32_t unique_id = 1;

enum rte_flow_classify_table_type table_type
	= RTE_FLOW_CLASSIFY_TABLE_TYPE_NONE;

struct rte_flow_classify_table_entry {
	/* meta-data for classify rule */
	uint32_t rule_id;

	/* Flow action */
	struct classify_action action;
};

struct rte_cls_table {
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

	/* Internal */
	/* ntuple_filter */
	struct rte_eth_ntuple_filter ntuple_filter;

	/* classifier tables */
	struct rte_cls_table tables[RTE_FLOW_CLASSIFY_TABLE_MAX];
	uint32_t table_mask;
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
	enum rte_flow_classify_table_type tbl_type; /* rule table */
	struct classify_rules rules; /* union of rules */
	union {
		struct acl_keys key;
	} u;
	int key_found;   /* rule key found in table */
	struct rte_flow_classify_table_entry entry;  /* rule meta data */
	void *entry_ptr; /* handle to the table entry for rule meta data */
};

int
rte_flow_classify_validate(
		   struct rte_flow_classifier *cls,
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

	if (error == NULL)
		return -EINVAL;

	if (cls == NULL) {
		RTE_FLOW_CLASSIFY_LOG(ERR,
			"%s: rte_flow_classifier parameter is NULL\n",
			__func__);
		return -EINVAL;
	}

	if (!attr) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "NULL attribute.");
		return -EINVAL;
	}

	if (!pattern) {
		rte_flow_error_set(error,
			EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
			NULL, "NULL pattern.");
		return -EINVAL;
	}

	if (!actions) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "NULL action.");
		return -EINVAL;
	}

	memset(&cls->ntuple_filter, 0, sizeof(cls->ntuple_filter));

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

	ret = parse_filter(attr, items, actions, &cls->ntuple_filter, error);
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
	if (params->socket_id < 0) {
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
	strlcpy(cls->name, params->name, RTE_FLOW_CLASSIFIER_MAX_NAME_SZ);

	cls->socket_id = params->socket_id;

	return cls;
}

static void
rte_flow_classify_table_free(struct rte_cls_table *table)
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
		struct rte_cls_table *table = &cls->tables[i];

		rte_flow_classify_table_free(table);
	}

	/* Free flow classifier memory */
	rte_free(cls);

	return 0;
}

static int
rte_table_check_params(struct rte_flow_classifier *cls,
		struct rte_flow_classify_table_params *params)
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
	struct rte_flow_classify_table_params *params)
{
	struct rte_cls_table *table;
	void *h_table;
	uint32_t entry_size;
	int ret;

	/* Check input arguments */
	ret = rte_table_check_params(cls, params);
	if (ret != 0)
		return ret;

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
	table = &cls->tables[cls->num_tables];
	table->type = params->type;
	cls->num_tables++;

	/* Save input parameters */
	memcpy(&table->ops, params->ops, sizeof(struct rte_table_ops));

	/* Initialize table internal data structure */
	table->entry_size = entry_size;
	table->h_table = h_table;

	return 0;
}

static struct rte_flow_classify_rule *
allocate_acl_ipv4_5tuple_rule(struct rte_flow_classifier *cls)
{
	struct rte_flow_classify_rule *rule;

	rule = malloc(sizeof(struct rte_flow_classify_rule));
	if (!rule)
		return rule;

	memset(rule, 0, sizeof(struct rte_flow_classify_rule));
	rule->id = unique_id++;
	rule->rules.type = RTE_FLOW_CLASSIFY_RULE_TYPE_IPV4_5TUPLE;

	/* key add values */
	rule->u.key.key_add.priority = cls->ntuple_filter.priority;
	rule->u.key.key_add.field_value[PROTO_FIELD_IPV4].mask_range.u8 =
			cls->ntuple_filter.proto_mask;
	rule->u.key.key_add.field_value[PROTO_FIELD_IPV4].value.u8 =
			cls->ntuple_filter.proto;
	rule->rules.u.ipv4_5tuple.proto = cls->ntuple_filter.proto;
	rule->rules.u.ipv4_5tuple.proto_mask = cls->ntuple_filter.proto_mask;

	rule->u.key.key_add.field_value[SRC_FIELD_IPV4].mask_range.u32 =
			cls->ntuple_filter.src_ip_mask;
	rule->u.key.key_add.field_value[SRC_FIELD_IPV4].value.u32 =
			cls->ntuple_filter.src_ip;
	rule->rules.u.ipv4_5tuple.src_ip_mask = cls->ntuple_filter.src_ip_mask;
	rule->rules.u.ipv4_5tuple.src_ip = cls->ntuple_filter.src_ip;

	rule->u.key.key_add.field_value[DST_FIELD_IPV4].mask_range.u32 =
			cls->ntuple_filter.dst_ip_mask;
	rule->u.key.key_add.field_value[DST_FIELD_IPV4].value.u32 =
			cls->ntuple_filter.dst_ip;
	rule->rules.u.ipv4_5tuple.dst_ip_mask = cls->ntuple_filter.dst_ip_mask;
	rule->rules.u.ipv4_5tuple.dst_ip = cls->ntuple_filter.dst_ip;

	rule->u.key.key_add.field_value[SRCP_FIELD_IPV4].mask_range.u16 =
			cls->ntuple_filter.src_port_mask;
	rule->u.key.key_add.field_value[SRCP_FIELD_IPV4].value.u16 =
			cls->ntuple_filter.src_port;
	rule->rules.u.ipv4_5tuple.src_port_mask =
			cls->ntuple_filter.src_port_mask;
	rule->rules.u.ipv4_5tuple.src_port = cls->ntuple_filter.src_port;

	rule->u.key.key_add.field_value[DSTP_FIELD_IPV4].mask_range.u16 =
			cls->ntuple_filter.dst_port_mask;
	rule->u.key.key_add.field_value[DSTP_FIELD_IPV4].value.u16 =
			cls->ntuple_filter.dst_port;
	rule->rules.u.ipv4_5tuple.dst_port_mask =
			cls->ntuple_filter.dst_port_mask;
	rule->rules.u.ipv4_5tuple.dst_port = cls->ntuple_filter.dst_port;

	if (rte_log_can_log(librte_flow_classify_logtype, RTE_LOG_DEBUG))
		print_acl_ipv4_key_add(&rule->u.key.key_add);

	/* key delete values */
	memcpy(&rule->u.key.key_del.field_value[PROTO_FIELD_IPV4],
	       &rule->u.key.key_add.field_value[PROTO_FIELD_IPV4],
	       NUM_FIELDS_IPV4 * sizeof(struct rte_acl_field));

	if (rte_log_can_log(librte_flow_classify_logtype, RTE_LOG_DEBUG))
		print_acl_ipv4_key_delete(&rule->u.key.key_del);

	return rule;
}

struct rte_flow_classify_rule *
rte_flow_classify_table_entry_add(struct rte_flow_classifier *cls,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		int *key_found,
		struct rte_flow_error *error)
{
	struct rte_flow_classify_rule *rule;
	struct rte_flow_classify_table_entry *table_entry;
	struct classify_action *action;
	uint32_t i;
	int ret;

	if (!error)
		return NULL;

	if (key_found == NULL) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "NULL key_found.");
		return NULL;
	}

	/* parse attr, pattern and actions */
	ret = rte_flow_classify_validate(cls, attr, pattern, actions, error);
	if (ret < 0)
		return NULL;

	switch (table_type) {
	case RTE_FLOW_CLASSIFY_TABLE_ACL_IP4_5TUPLE:
		rule = allocate_acl_ipv4_5tuple_rule(cls);
		if (!rule)
			return NULL;
		rule->tbl_type = table_type;
		cls->table_mask |= table_type;
		break;
	default:
		return NULL;
	}

	action = classify_get_flow_action();
	table_entry = &rule->entry;
	table_entry->rule_id = rule->id;
	table_entry->action.action_mask = action->action_mask;

	/* Copy actions */
	if (action->action_mask & (1LLU << RTE_FLOW_ACTION_TYPE_COUNT)) {
		memcpy(&table_entry->action.act.counter, &action->act.counter,
				sizeof(table_entry->action.act.counter));
	}
	if (action->action_mask & (1LLU << RTE_FLOW_ACTION_TYPE_MARK)) {
		memcpy(&table_entry->action.act.mark, &action->act.mark,
				sizeof(table_entry->action.act.mark));
	}

	for (i = 0; i < cls->num_tables; i++) {
		struct rte_cls_table *table = &cls->tables[i];

		if (table->type == table_type) {
			if (table->ops.f_add != NULL) {
				ret = table->ops.f_add(
					table->h_table,
					&rule->u.key.key_add,
					&rule->entry,
					&rule->key_found,
					&rule->entry_ptr);
				if (ret) {
					free(rule);
					return NULL;
				}

			*key_found = rule->key_found;
			}

			return rule;
		}
	}
	free(rule);
	return NULL;
}

int
rte_flow_classify_table_entry_delete(struct rte_flow_classifier *cls,
		struct rte_flow_classify_rule *rule)
{
	uint32_t i;
	int ret = -EINVAL;

	if (!cls || !rule)
		return ret;
	enum rte_flow_classify_table_type tbl_type = rule->tbl_type;

	for (i = 0; i < cls->num_tables; i++) {
		struct rte_cls_table *table = &cls->tables[i];

		if (table->type == tbl_type) {
			if (table->ops.f_delete != NULL) {
				ret = table->ops.f_delete(table->h_table,
						&rule->u.key.key_del,
						&rule->key_found,
						&rule->entry);
				if (ret == 0)
					free(rule);
				return ret;
			}
		}
	}
	return ret;
}

static int
flow_classifier_lookup(struct rte_flow_classifier *cls,
		struct rte_cls_table *table,
		struct rte_mbuf **pkts,
		const uint16_t nb_pkts)
{
	int ret = -EINVAL;
	uint64_t pkts_mask;
	uint64_t lookup_hit_mask;

	pkts_mask = RTE_LEN2MASK(nb_pkts, uint64_t);
	ret = table->ops.f_lookup(table->h_table,
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
	struct rte_flow_classify_table_entry *entry = &rule->entry;
	uint64_t count = 0;
	uint32_t action_mask = entry->action.action_mask;
	int i, ret = -EINVAL;

	if (action_mask & (1LLU << RTE_FLOW_ACTION_TYPE_COUNT)) {
		for (i = 0; i < cls->nb_pkts; i++) {
			if (rule->id == cls->entries[i]->rule_id)
				count++;
		}
		if (count) {
			ret = 0;
			ntuple_stats = stats->stats;
			ntuple_stats->counter1 = count;
			ntuple_stats->ipv4_5tuple = rule->rules.u.ipv4_5tuple;
		}
	}
	return ret;
}

int
rte_flow_classifier_query(struct rte_flow_classifier *cls,
		struct rte_mbuf **pkts,
		const uint16_t nb_pkts,
		struct rte_flow_classify_rule *rule,
		struct rte_flow_classify_stats *stats)
{
	enum rte_flow_classify_table_type tbl_type;
	uint32_t i;
	int ret = -EINVAL;

	if (!cls || !rule || !stats || !pkts  || nb_pkts == 0)
		return ret;

	tbl_type = rule->tbl_type;
	for (i = 0; i < cls->num_tables; i++) {
		struct rte_cls_table *table = &cls->tables[i];

			if (table->type == tbl_type) {
				ret = flow_classifier_lookup(cls, table,
						pkts, nb_pkts);
				if (!ret) {
					ret = action_apply(cls, rule, stats);
					return ret;
				}
			}
	}
	return ret;
}

RTE_LOG_REGISTER(librte_flow_classify_logtype, lib.flow_classify, INFO);
