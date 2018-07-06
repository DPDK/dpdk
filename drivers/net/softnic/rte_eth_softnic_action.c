/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_string_fns.h>

#include "hash_func.h"
#include "rte_eth_softnic_internals.h"

/**
 * Input port
 */
int
softnic_port_in_action_profile_init(struct pmd_internals *p)
{
	TAILQ_INIT(&p->port_in_action_profile_list);

	return 0;
}

void
softnic_port_in_action_profile_free(struct pmd_internals *p)
{
	for ( ; ; ) {
		struct softnic_port_in_action_profile *profile;

		profile = TAILQ_FIRST(&p->port_in_action_profile_list);
		if (profile == NULL)
			break;

		TAILQ_REMOVE(&p->port_in_action_profile_list, profile, node);
		free(profile);
	}
}

struct softnic_port_in_action_profile *
softnic_port_in_action_profile_find(struct pmd_internals *p,
	const char *name)
{
	struct softnic_port_in_action_profile *profile;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(profile, &p->port_in_action_profile_list, node)
		if (strcmp(profile->name, name) == 0)
			return profile;

	return NULL;
}

struct softnic_port_in_action_profile *
softnic_port_in_action_profile_create(struct pmd_internals *p,
	const char *name,
	struct softnic_port_in_action_profile_params *params)
{
	struct softnic_port_in_action_profile *profile;
	struct rte_port_in_action_profile *ap;
	int status;

	/* Check input params */
	if (name == NULL ||
		softnic_port_in_action_profile_find(p, name) ||
		params == NULL)
		return NULL;

	if ((params->action_mask & (1LLU << RTE_PORT_IN_ACTION_LB)) &&
		params->lb.f_hash == NULL) {
		switch (params->lb.key_size) {
		case  8:
			params->lb.f_hash = hash_default_key8;
			break;

		case 16:
			params->lb.f_hash = hash_default_key16;
			break;

		case 24:
			params->lb.f_hash = hash_default_key24;
			break;

		case 32:
			params->lb.f_hash = hash_default_key32;
			break;

		case 40:
			params->lb.f_hash = hash_default_key40;
			break;

		case 48:
			params->lb.f_hash = hash_default_key48;
			break;

		case 56:
			params->lb.f_hash = hash_default_key56;
			break;

		case 64:
			params->lb.f_hash = hash_default_key64;
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
	profile = calloc(1, sizeof(struct softnic_port_in_action_profile));
	if (profile == NULL) {
		rte_port_in_action_profile_free(ap);
		return NULL;
	}

	/* Node fill in */
	strlcpy(profile->name, name, sizeof(profile->name));
	memcpy(&profile->params, params, sizeof(*params));
	profile->ap = ap;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&p->port_in_action_profile_list, profile, node);

	return profile;
}
