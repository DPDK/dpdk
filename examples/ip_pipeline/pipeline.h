/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _INCLUDE_PIPELINE_H_
#define _INCLUDE_PIPELINE_H_

#include <stdint.h>
#include <sys/queue.h>

#include <rte_pipeline.h>
#include <rte_table_action.h>

#include "common.h"
#include "action.h"

struct pipeline_params {
	uint32_t timer_period_ms;
	uint32_t offset_port_id;
	uint32_t cpu_id;
};

enum port_in_type {
	PORT_IN_RXQ,
	PORT_IN_SWQ,
	PORT_IN_TMGR,
	PORT_IN_TAP,
	PORT_IN_KNI,
	PORT_IN_SOURCE,
	PORT_IN_CRYPTODEV,
};

struct port_in_params {
	/* Read */
	enum port_in_type type;
	const char *dev_name;
	union {
		struct {
			uint16_t queue_id;
		} rxq;

		struct {
			const char *mempool_name;
			uint32_t mtu;
		} tap;

		struct {
			const char *mempool_name;
			const char *file_name;
			uint32_t n_bytes_per_pkt;
		} source;

		struct {
			uint16_t queue_id;
			void *f_callback;
			void *arg_callback;
		} cryptodev;
	};
	uint32_t burst_size;

	/* Action */
	const char *action_profile_name;
};

enum port_out_type {
	PORT_OUT_TXQ,
	PORT_OUT_SWQ,
	PORT_OUT_TMGR,
	PORT_OUT_TAP,
	PORT_OUT_KNI,
	PORT_OUT_SINK,
	PORT_OUT_CRYPTODEV,
};

struct port_out_params {
	enum port_out_type type;
	const char *dev_name;
	union {
		struct {
			uint16_t queue_id;
		} txq;

		struct {
			const char *file_name;
			uint32_t max_n_pkts;
		} sink;

		struct {
			uint16_t queue_id;
			uint32_t op_offset;
		} cryptodev;
	};
	uint32_t burst_size;
	int retry;
	uint32_t n_retries;
};

enum table_type {
	TABLE_ACL,
	TABLE_ARRAY,
	TABLE_HASH,
	TABLE_LPM,
	TABLE_STUB,
};

struct table_acl_params {
	uint32_t n_rules;
	uint32_t ip_header_offset;
	int ip_version;
};

struct table_array_params {
	uint32_t n_keys;
	uint32_t key_offset;
};

struct table_hash_params {
	uint32_t n_keys;
	uint32_t key_offset;
	uint32_t key_size;
	uint8_t *key_mask;
	uint32_t n_buckets;
	int extendable_bucket;
};

struct table_lpm_params {
	uint32_t n_rules;
	uint32_t key_offset;
	uint32_t key_size;
};

struct table_params {
	/* Match */
	enum table_type match_type;
	union {
		struct table_acl_params acl;
		struct table_array_params array;
		struct table_hash_params hash;
		struct table_lpm_params lpm;
	} match;

	/* Action */
	const char *action_profile_name;
};

struct table_rule;

TAILQ_HEAD(table_rule_list, table_rule);

struct port_in {
	struct port_in_params params;
	struct port_in_action_profile *ap;
	struct rte_port_in_action *a;
};

struct table {
	struct table_params params;
	struct table_action_profile *ap;
	struct rte_table_action *a;
	struct table_rule_list rules;
	struct table_rule *rule_default;
};

struct pipeline {
	TAILQ_ENTRY(pipeline) node;
	char name[NAME_SIZE];

	struct rte_pipeline *p;
	struct port_in port_in[RTE_PIPELINE_PORT_IN_MAX];
	struct table table[RTE_PIPELINE_TABLE_MAX];
	uint32_t n_ports_in;
	uint32_t n_ports_out;
	uint32_t n_tables;

	struct rte_ring *msgq_req;
	struct rte_ring *msgq_rsp;
	uint32_t timer_period_ms;

	int enabled;
	uint32_t thread_id;
	uint32_t cpu_id;
};

TAILQ_HEAD(pipeline_list, pipeline);

int
pipeline_init(void);

struct pipeline *
pipeline_find(const char *name);

struct pipeline *
pipeline_create(const char *name, struct pipeline_params *params);

int
pipeline_port_in_create(const char *pipeline_name,
	struct port_in_params *params,
	int enabled);

int
pipeline_port_in_connect_to_table(const char *pipeline_name,
	uint32_t port_id,
	uint32_t table_id);

int
pipeline_port_out_create(const char *pipeline_name,
	struct port_out_params *params);

int
pipeline_table_create(const char *pipeline_name,
	struct table_params *params);

struct table_rule_match_acl {
	int ip_version;

	RTE_STD_C11
	union {
		struct {
			uint32_t sa;
			uint32_t da;
		} ipv4;

		struct {
			uint8_t sa[16];
			uint8_t da[16];
		} ipv6;
	};

	uint32_t sa_depth;
	uint32_t da_depth;
	uint16_t sp0;
	uint16_t sp1;
	uint16_t dp0;
	uint16_t dp1;
	uint8_t proto;
	uint8_t proto_mask;
	uint32_t priority;
};

struct table_rule_match_array {
	uint32_t pos;
};

#ifndef TABLE_RULE_MATCH_SIZE_MAX
#define TABLE_RULE_MATCH_SIZE_MAX                          256
#endif

#ifndef TABLE_RULE_ACTION_SIZE_MAX
#define TABLE_RULE_ACTION_SIZE_MAX                         2048
#endif

struct table_rule_match_hash {
	uint8_t key[TABLE_RULE_MATCH_SIZE_MAX];
};

struct table_rule_match_lpm {
	int ip_version;

	RTE_STD_C11
	union {
		uint32_t ipv4;
		uint8_t ipv6[16];
	};

	uint8_t depth;
};

struct table_rule_match {
	enum table_type match_type;

	union {
		struct table_rule_match_acl acl;
		struct table_rule_match_array array;
		struct table_rule_match_hash hash;
		struct table_rule_match_lpm lpm;
	} match;
};

struct table_rule_action {
	uint64_t action_mask;
	struct rte_table_action_fwd_params fwd;
	struct rte_table_action_lb_params lb;
	struct rte_table_action_mtr_params mtr;
	struct rte_table_action_tm_params tm;
	struct rte_table_action_encap_params encap;
	struct rte_table_action_nat_params nat;
	struct rte_table_action_ttl_params ttl;
	struct rte_table_action_stats_params stats;
	struct rte_table_action_time_params time;
	struct rte_table_action_sym_crypto_params sym_crypto;
	struct rte_table_action_tag_params tag;
	struct rte_table_action_decap_params decap;
};

struct table_rule {
	TAILQ_ENTRY(table_rule) node;
	struct table_rule_match match;
	struct table_rule_action action;
	void *data;
};

int
pipeline_port_in_stats_read(const char *pipeline_name,
	uint32_t port_id,
	struct rte_pipeline_port_in_stats *stats,
	int clear);

int
pipeline_port_in_enable(const char *pipeline_name,
	uint32_t port_id);

int
pipeline_port_in_disable(const char *pipeline_name,
	uint32_t port_id);

int
pipeline_port_out_stats_read(const char *pipeline_name,
	uint32_t port_id,
	struct rte_pipeline_port_out_stats *stats,
	int clear);

int
pipeline_table_stats_read(const char *pipeline_name,
	uint32_t table_id,
	struct rte_pipeline_table_stats *stats,
	int clear);

int
pipeline_table_rule_add(const char *pipeline_name,
	uint32_t table_id,
	struct table_rule_match *match,
	struct table_rule_action *action);

int
pipeline_table_rule_add_bulk(const char *pipeline_name,
	uint32_t table_id,
	struct table_rule_list *list,
	uint32_t *n_rules_added,
	uint32_t *n_rules_not_added);

int
pipeline_table_rule_add_default(const char *pipeline_name,
	uint32_t table_id,
	struct table_rule_action *action);

int
pipeline_table_rule_delete(const char *pipeline_name,
	uint32_t table_id,
	struct table_rule_match *match);

int
pipeline_table_rule_delete_default(const char *pipeline_name,
	uint32_t table_id);

int
pipeline_table_rule_stats_read(const char *pipeline_name,
	uint32_t table_id,
	struct table_rule_match *match,
	struct rte_table_action_stats_counters *stats,
	int clear);

int
pipeline_table_mtr_profile_add(const char *pipeline_name,
	uint32_t table_id,
	uint32_t meter_profile_id,
	struct rte_table_action_meter_profile *profile);

int
pipeline_table_mtr_profile_delete(const char *pipeline_name,
	uint32_t table_id,
	uint32_t meter_profile_id);

int
pipeline_table_rule_mtr_read(const char *pipeline_name,
	uint32_t table_id,
	struct table_rule_match *match,
	struct rte_table_action_mtr_counters *stats,
	int clear);

int
pipeline_table_dscp_table_update(const char *pipeline_name,
	uint32_t table_id,
	uint64_t dscp_mask,
	struct rte_table_action_dscp_table *dscp_table);

int
pipeline_table_rule_ttl_read(const char *pipeline_name,
	uint32_t table_id,
	struct table_rule_match *match,
	struct rte_table_action_ttl_counters *stats,
	int clear);

int
pipeline_table_rule_time_read(const char *pipeline_name,
	uint32_t table_id,
	struct table_rule_match *match,
	uint64_t *timestamp);

struct table_rule *
table_rule_find(struct table *table,
	struct table_rule_match *match);

void
table_rule_add(struct table *table,
	struct table_rule *rule);

void
table_rule_add_bulk(struct table *table,
	struct table_rule_list *list,
	uint32_t n_rules);

void
table_rule_delete(struct table *table,
	struct table_rule_match *match);

void
table_rule_default_add(struct table *table,
	struct table_rule *rule);

void
table_rule_default_delete(struct table *table);

#endif /* _INCLUDE_PIPELINE_H_ */
