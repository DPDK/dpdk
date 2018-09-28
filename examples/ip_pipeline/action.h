/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _INCLUDE_ACTION_H_
#define _INCLUDE_ACTION_H_

#include <sys/queue.h>

#include <rte_port_in_action.h>
#include <rte_table_action.h>

#include "common.h"

/**
 * Input port action
 */
struct port_in_action_profile_params {
	uint64_t action_mask;
	struct rte_port_in_action_fltr_config fltr;
	struct rte_port_in_action_lb_config lb;
};

struct port_in_action_profile {
	TAILQ_ENTRY(port_in_action_profile) node;
	char name[NAME_SIZE];
	struct port_in_action_profile_params params;
	struct rte_port_in_action_profile *ap;
};

TAILQ_HEAD(port_in_action_profile_list, port_in_action_profile);

int
port_in_action_profile_init(void);

struct port_in_action_profile *
port_in_action_profile_find(const char *name);

struct port_in_action_profile *
port_in_action_profile_create(const char *name,
	struct port_in_action_profile_params *params);

/**
 * Table action
 */
struct table_action_profile_params {
	uint64_t action_mask;
	struct rte_table_action_common_config common;
	struct rte_table_action_lb_config lb;
	struct rte_table_action_mtr_config mtr;
	struct rte_table_action_tm_config tm;
	struct rte_table_action_encap_config encap;
	struct rte_table_action_nat_config nat;
	struct rte_table_action_ttl_config ttl;
	struct rte_table_action_stats_config stats;
	struct rte_table_action_sym_crypto_config sym_crypto;
};

struct table_action_profile {
	TAILQ_ENTRY(table_action_profile) node;
	char name[NAME_SIZE];
	struct table_action_profile_params params;
	struct rte_table_action_profile *ap;
};

TAILQ_HEAD(table_action_profile_list, table_action_profile);

int
table_action_profile_init(void);

struct table_action_profile *
table_action_profile_find(const char *name);

struct table_action_profile *
table_action_profile_create(const char *name,
	struct table_action_profile_params *params);

#endif /* _INCLUDE_ACTION_H_ */
