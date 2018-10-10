/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_string_fns.h>
#include <rte_table_hash_func.h>

#include "action.h"

/**
 * Input port
 */
static struct port_in_action_profile_list port_in_action_profile_list;

int
port_in_action_profile_init(void)
{
	TAILQ_INIT(&port_in_action_profile_list);

	return 0;
}

struct port_in_action_profile *
port_in_action_profile_find(const char *name)
{
	struct port_in_action_profile *profile;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(profile, &port_in_action_profile_list, node)
		if (strcmp(profile->name, name) == 0)
			return profile;

	return NULL;
}

struct port_in_action_profile *
port_in_action_profile_create(const char *name,
	struct port_in_action_profile_params *params)
{
	struct port_in_action_profile *profile;
	struct rte_port_in_action_profile *ap;
	int status;

	/* Check input params */
	if ((name == NULL) ||
		port_in_action_profile_find(name) ||
		(params == NULL))
		return NULL;

	if ((params->action_mask & (1LLU << RTE_PORT_IN_ACTION_LB)) &&
		(params->lb.f_hash == NULL)) {
		switch (params->lb.key_size) {
		case  8:
			params->lb.f_hash = rte_table_hash_crc_key8;
			break;

		case 16:
			params->lb.f_hash = rte_table_hash_crc_key16;
			break;

		case 24:
			params->lb.f_hash = rte_table_hash_crc_key24;
			break;

		case 32:
			params->lb.f_hash = rte_table_hash_crc_key32;
			break;

		case 40:
			params->lb.f_hash = rte_table_hash_crc_key40;
			break;

		case 48:
			params->lb.f_hash = rte_table_hash_crc_key48;
			break;

		case 56:
			params->lb.f_hash = rte_table_hash_crc_key56;
			break;

		case 64:
			params->lb.f_hash = rte_table_hash_crc_key64;
			break;

		default:
			return NULL;
		}

		params->lb.seed = 0;
	}
	/* Resource */
	ap = rte_port_in_action_profile_create(0);
	if (ap == NULL)
		return NULL;

	if (params->action_mask & (1LLU << RTE_PORT_IN_ACTION_FLTR)) {
		status = rte_port_in_action_profile_action_register(ap,
			RTE_PORT_IN_ACTION_FLTR,
			&params->fltr);

		if (status) {
			rte_port_in_action_profile_free(ap);
			return NULL;
		}
	}

	if (params->action_mask & (1LLU << RTE_PORT_IN_ACTION_LB)) {
		status = rte_port_in_action_profile_action_register(ap,
			RTE_PORT_IN_ACTION_LB,
			&params->lb);

		if (status) {
			rte_port_in_action_profile_free(ap);
			return NULL;
		}
	}

	status = rte_port_in_action_profile_freeze(ap);
	if (status) {
		rte_port_in_action_profile_free(ap);
		return NULL;
	}

	/* Node allocation */
	profile = calloc(1, sizeof(struct port_in_action_profile));
	if (profile == NULL) {
		rte_port_in_action_profile_free(ap);
		return NULL;
	}

	/* Node fill in */
	strlcpy(profile->name, name, sizeof(profile->name));
	memcpy(&profile->params, params, sizeof(*params));
	profile->ap = ap;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&port_in_action_profile_list, profile, node);

	return profile;
}

/**
 * Table
 */
static struct table_action_profile_list table_action_profile_list;

int
table_action_profile_init(void)
{
	TAILQ_INIT(&table_action_profile_list);

	return 0;
}

struct table_action_profile *
table_action_profile_find(const char *name)
{
	struct table_action_profile *profile;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(profile, &table_action_profile_list, node)
		if (strcmp(profile->name, name) == 0)
			return profile;

	return NULL;
}

struct table_action_profile *
table_action_profile_create(const char *name,
	struct table_action_profile_params *params)
{
	struct table_action_profile *profile;
	struct rte_table_action_profile *ap;
	int status;

	/* Check input params */
	if ((name == NULL) ||
		table_action_profile_find(name) ||
		(params == NULL) ||
		((params->action_mask & (1LLU << RTE_TABLE_ACTION_FWD)) == 0))
		return NULL;

	if ((params->action_mask & (1LLU << RTE_TABLE_ACTION_LB)) &&
		(params->lb.f_hash == NULL)) {
		switch (params->lb.key_size) {
		case 8:
			params->lb.f_hash = rte_table_hash_crc_key8;
			break;

		case 16:
			params->lb.f_hash = rte_table_hash_crc_key16;
			break;

		case 24:
			params->lb.f_hash = rte_table_hash_crc_key24;
			break;

		case 32:
			params->lb.f_hash = rte_table_hash_crc_key32;
			break;

		case 40:
			params->lb.f_hash = rte_table_hash_crc_key40;
			break;

		case 48:
			params->lb.f_hash = rte_table_hash_crc_key48;
			break;

		case 56:
			params->lb.f_hash = rte_table_hash_crc_key56;
			break;

		case 64:
			params->lb.f_hash = rte_table_hash_crc_key64;
			break;

		default:
			return NULL;
		}

		params->lb.seed = 0;
	}

	/* Resource */
	ap = rte_table_action_profile_create(&params->common);
	if (ap == NULL)
		return NULL;

	if (params->action_mask & (1LLU << RTE_TABLE_ACTION_FWD)) {
		status = rte_table_action_profile_action_register(ap,
			RTE_TABLE_ACTION_FWD,
			NULL);

		if (status) {
			rte_table_action_profile_free(ap);
			return NULL;
		}
	}

	if (params->action_mask & (1LLU << RTE_TABLE_ACTION_LB)) {
		status = rte_table_action_profile_action_register(ap,
			RTE_TABLE_ACTION_LB,
			&params->lb);

		if (status) {
			rte_table_action_profile_free(ap);
			return NULL;
		}
	}

	if (params->action_mask & (1LLU << RTE_TABLE_ACTION_MTR)) {
		status = rte_table_action_profile_action_register(ap,
			RTE_TABLE_ACTION_MTR,
			&params->mtr);

		if (status) {
			rte_table_action_profile_free(ap);
			return NULL;
		}
	}

	if (params->action_mask & (1LLU << RTE_TABLE_ACTION_TM)) {
		status = rte_table_action_profile_action_register(ap,
			RTE_TABLE_ACTION_TM,
			&params->tm);

		if (status) {
			rte_table_action_profile_free(ap);
			return NULL;
		}
	}

	if (params->action_mask & (1LLU << RTE_TABLE_ACTION_ENCAP)) {
		status = rte_table_action_profile_action_register(ap,
			RTE_TABLE_ACTION_ENCAP,
			&params->encap);

		if (status) {
			rte_table_action_profile_free(ap);
			return NULL;
		}
	}

	if (params->action_mask & (1LLU << RTE_TABLE_ACTION_NAT)) {
		status = rte_table_action_profile_action_register(ap,
			RTE_TABLE_ACTION_NAT,
			&params->nat);

		if (status) {
			rte_table_action_profile_free(ap);
			return NULL;
		}
	}

	if (params->action_mask & (1LLU << RTE_TABLE_ACTION_TTL)) {
		status = rte_table_action_profile_action_register(ap,
			RTE_TABLE_ACTION_TTL,
			&params->ttl);

		if (status) {
			rte_table_action_profile_free(ap);
			return NULL;
		}
	}

	if (params->action_mask & (1LLU << RTE_TABLE_ACTION_STATS)) {
		status = rte_table_action_profile_action_register(ap,
			RTE_TABLE_ACTION_STATS,
			&params->stats);

		if (status) {
			rte_table_action_profile_free(ap);
			return NULL;
		}
	}
	if (params->action_mask & (1LLU << RTE_TABLE_ACTION_TIME)) {
		status = rte_table_action_profile_action_register(ap,
			RTE_TABLE_ACTION_TIME,
			NULL);

		if (status) {
			rte_table_action_profile_free(ap);
			return NULL;
		}
	}

	if (params->action_mask & (1LLU << RTE_TABLE_ACTION_SYM_CRYPTO)) {
		status = rte_table_action_profile_action_register(ap,
			RTE_TABLE_ACTION_SYM_CRYPTO,
			&params->sym_crypto);

		if (status) {
			rte_table_action_profile_free(ap);
			return NULL;
		}
	}

	if (params->action_mask & (1LLU << RTE_TABLE_ACTION_TAG)) {
		status = rte_table_action_profile_action_register(ap,
			RTE_TABLE_ACTION_TAG,
			NULL);

		if (status) {
			rte_table_action_profile_free(ap);
			return NULL;
		}
	}

	if (params->action_mask & (1LLU << RTE_TABLE_ACTION_DECAP)) {
		status = rte_table_action_profile_action_register(ap,
			RTE_TABLE_ACTION_DECAP,
			NULL);

		if (status) {
			rte_table_action_profile_free(ap);
			return NULL;
		}
	}

	status = rte_table_action_profile_freeze(ap);
	if (status) {
		rte_table_action_profile_free(ap);
		return NULL;
	}

	/* Node allocation */
	profile = calloc(1, sizeof(struct table_action_profile));
	if (profile == NULL) {
		rte_table_action_profile_free(ap);
		return NULL;
	}

	/* Node fill in */
	strlcpy(profile->name, name, sizeof(profile->name));
	memcpy(&profile->params, params, sizeof(*params));
	profile->ap = ap;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&table_action_profile_list, profile, node);

	return profile;
}
