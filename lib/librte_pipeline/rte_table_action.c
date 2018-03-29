/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_malloc.h>

#include <rte_common.h>
#include <rte_malloc.h>

#include "rte_table_action.h"

/**
 * RTE_TABLE_ACTION_FWD
 */
#define fwd_data rte_pipeline_table_entry

static int
fwd_apply(struct fwd_data *data,
	struct rte_table_action_fwd_params *p)
{
	data->action = p->action;

	if (p->action == RTE_PIPELINE_ACTION_PORT)
		data->port_id = p->id;

	if (p->action == RTE_PIPELINE_ACTION_TABLE)
		data->table_id = p->id;

	return 0;
}

/**
 * Action profile
 */
static int
action_valid(enum rte_table_action_type action)
{
	switch (action) {
	case RTE_TABLE_ACTION_FWD:
		return 1;
	default:
		return 0;
	}
}


#define RTE_TABLE_ACTION_MAX                      64

struct ap_config {
	uint64_t action_mask;
	struct rte_table_action_common_config common;
};

static size_t
action_cfg_size(enum rte_table_action_type action)
{
	switch (action) {
	default:
		return 0;
	}
}

static void*
action_cfg_get(struct ap_config *ap_config __rte_unused,
	enum rte_table_action_type type)
{
	switch (type) {
	default:
		return NULL;
	}
}

static void
action_cfg_set(struct ap_config *ap_config,
	enum rte_table_action_type type,
	void *action_cfg)
{
	void *dst = action_cfg_get(ap_config, type);

	if (dst)
		memcpy(dst, action_cfg, action_cfg_size(type));

	ap_config->action_mask |= 1LLU << type;
}

struct ap_data {
	size_t offset[RTE_TABLE_ACTION_MAX];
	size_t total_size;
};

static size_t
action_data_size(enum rte_table_action_type action,
	struct ap_config *ap_config __rte_unused)
{
	switch (action) {
	case RTE_TABLE_ACTION_FWD:
		return sizeof(struct fwd_data);

	default:
		return 0;
	}
}


static void
action_data_offset_set(struct ap_data *ap_data,
	struct ap_config *ap_config)
{
	uint64_t action_mask = ap_config->action_mask;
	size_t offset;
	uint32_t action;

	memset(ap_data->offset, 0, sizeof(ap_data->offset));

	offset = 0;
	for (action = 0; action < RTE_TABLE_ACTION_MAX; action++)
		if (action_mask & (1LLU << action)) {
			ap_data->offset[action] = offset;
			offset += action_data_size((enum rte_table_action_type)action,
				ap_config);
		}

	ap_data->total_size = offset;
}

struct rte_table_action_profile {
	struct ap_config cfg;
	struct ap_data data;
	int frozen;
};

struct rte_table_action_profile *
rte_table_action_profile_create(struct rte_table_action_common_config *common)
{
	struct rte_table_action_profile *ap;

	/* Check input arguments */
	if (common == NULL)
		return NULL;

	/* Memory allocation */
	ap = calloc(1, sizeof(struct rte_table_action_profile));
	if (ap == NULL)
		return NULL;

	/* Initialization */
	memcpy(&ap->cfg.common, common, sizeof(*common));

	return ap;
}


int
rte_table_action_profile_action_register(struct rte_table_action_profile *profile,
	enum rte_table_action_type type,
	void *action_config)
{
	/* Check input arguments */
	if ((profile == NULL) ||
		profile->frozen ||
		(action_valid(type) == 0) ||
		(profile->cfg.action_mask & (1LLU << type)) ||
		((action_cfg_size(type) == 0) && action_config) ||
		(action_cfg_size(type) && (action_config == NULL)))
		return -EINVAL;

	/* Action enable */
	action_cfg_set(&profile->cfg, type, action_config);

	return 0;
}

int
rte_table_action_profile_freeze(struct rte_table_action_profile *profile)
{
	if (profile->frozen)
		return -EBUSY;

	profile->cfg.action_mask |= 1LLU << RTE_TABLE_ACTION_FWD;
	action_data_offset_set(&profile->data, &profile->cfg);
	profile->frozen = 1;

	return 0;
}

int
rte_table_action_profile_free(struct rte_table_action_profile *profile)
{
	if (profile == NULL)
		return 0;

	free(profile);
	return 0;
}

struct rte_table_action {
	struct ap_config cfg;
	struct ap_data data;
};

struct rte_table_action *
rte_table_action_create(struct rte_table_action_profile *profile,
	uint32_t socket_id)
{
	struct rte_table_action *action;

	/* Check input arguments */
	if ((profile == NULL) ||
		(profile->frozen == 0))
		return NULL;

	/* Memory allocation */
	action = rte_zmalloc_socket(NULL,
		sizeof(struct rte_table_action),
		RTE_CACHE_LINE_SIZE,
		socket_id);
	if (action == NULL)
		return NULL;

	/* Initialization */
	memcpy(&action->cfg, &profile->cfg, sizeof(profile->cfg));
	memcpy(&action->data, &profile->data, sizeof(profile->data));

	return action;
}

static __rte_always_inline void *
action_data_get(void *data,
	struct rte_table_action *action,
	enum rte_table_action_type type)
{
	size_t offset = action->data.offset[type];
	uint8_t *data_bytes = data;

	return &data_bytes[offset];
}

int
rte_table_action_apply(struct rte_table_action *action,
	void *data,
	enum rte_table_action_type type,
	void *action_params)
{
	void *action_data;

	/* Check input arguments */
	if ((action == NULL) ||
		(data == NULL) ||
		(action_valid(type) == 0) ||
		((action->cfg.action_mask & (1LLU << type)) == 0) ||
		(action_params == NULL))
		return -EINVAL;

	/* Data update */
	action_data = action_data_get(data, action, type);

	switch (type) {
	case RTE_TABLE_ACTION_FWD:
		return fwd_apply(action_data,
			action_params);

	default:
		return -EINVAL;
	}
}

int
rte_table_action_free(struct rte_table_action *action)
{
	if (action == NULL)
		return 0;

	rte_free(action);

	return 0;
}
