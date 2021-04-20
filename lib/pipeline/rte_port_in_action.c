/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>

#include "rte_port_in_action.h"

/**
 * RTE_PORT_IN_ACTION_FLTR
 */
static int
fltr_cfg_check(struct rte_port_in_action_fltr_config *cfg)
{
	if (cfg == NULL)
		return -1;

	return 0;
}

struct fltr_data {
	uint32_t port_id;
};

static void
fltr_init(struct fltr_data *data,
	struct rte_port_in_action_fltr_config *cfg)
{
	data->port_id = cfg->port_id;
}

static int
fltr_apply(struct fltr_data *data,
	struct rte_port_in_action_fltr_params *p)
{
	/* Check input arguments */
	if (p == NULL)
		return -1;

	data->port_id = p->port_id;

	return 0;
}

/**
 * RTE_PORT_IN_ACTION_LB
 */
static int
lb_cfg_check(struct rte_port_in_action_lb_config *cfg)
{
	if ((cfg == NULL) ||
		(cfg->key_size < RTE_PORT_IN_ACTION_LB_KEY_SIZE_MIN) ||
		(cfg->key_size > RTE_PORT_IN_ACTION_LB_KEY_SIZE_MAX) ||
		(!rte_is_power_of_2(cfg->key_size)) ||
		(cfg->f_hash == NULL))
		return -1;

	return 0;
}

struct lb_data {
	uint32_t port_id[RTE_PORT_IN_ACTION_LB_TABLE_SIZE];
};

static void
lb_init(struct lb_data *data,
	struct rte_port_in_action_lb_config *cfg)
{
	memcpy(data->port_id, cfg->port_id, sizeof(cfg->port_id));
}

static int
lb_apply(struct lb_data *data,
	struct rte_port_in_action_lb_params *p)
{
	/* Check input arguments */
	if (p == NULL)
		return -1;

	memcpy(data->port_id, p->port_id, sizeof(p->port_id));

	return 0;
}

/**
 * Action profile
 */
static int
action_valid(enum rte_port_in_action_type action)
{
	switch (action) {
	case RTE_PORT_IN_ACTION_FLTR:
	case RTE_PORT_IN_ACTION_LB:
		return 1;
	default:
		return 0;
	}
}

#define RTE_PORT_IN_ACTION_MAX                             64

struct ap_config {
	uint64_t action_mask;
	struct rte_port_in_action_fltr_config fltr;
	struct rte_port_in_action_lb_config lb;
};

static size_t
action_cfg_size(enum rte_port_in_action_type action)
{
	switch (action) {
	case RTE_PORT_IN_ACTION_FLTR:
		return sizeof(struct rte_port_in_action_fltr_config);
	case RTE_PORT_IN_ACTION_LB:
		return sizeof(struct rte_port_in_action_lb_config);
	default:
		return 0;
	}
}

static void*
action_cfg_get(struct ap_config *ap_config,
	enum rte_port_in_action_type type)
{
	switch (type) {
	case RTE_PORT_IN_ACTION_FLTR:
		return &ap_config->fltr;

	case RTE_PORT_IN_ACTION_LB:
		return &ap_config->lb;

	default:
		return NULL;
	}
}

static void
action_cfg_set(struct ap_config *ap_config,
	enum rte_port_in_action_type type,
	void *action_cfg)
{
	void *dst = action_cfg_get(ap_config, type);

	if (dst)
		memcpy(dst, action_cfg, action_cfg_size(type));

	ap_config->action_mask |= 1LLU << type;
}

struct ap_data {
	size_t offset[RTE_PORT_IN_ACTION_MAX];
	size_t total_size;
};

static size_t
action_data_size(enum rte_port_in_action_type action,
	struct ap_config *ap_config __rte_unused)
{
	switch (action) {
	case RTE_PORT_IN_ACTION_FLTR:
		return sizeof(struct fltr_data);

	case RTE_PORT_IN_ACTION_LB:
		return sizeof(struct lb_data);

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
	for (action = 0; action < RTE_PORT_IN_ACTION_MAX; action++)
		if (action_mask & (1LLU << action)) {
			ap_data->offset[action] = offset;
			offset += action_data_size((enum rte_port_in_action_type)action,
				ap_config);
		}

	ap_data->total_size = offset;
}

struct rte_port_in_action_profile {
	struct ap_config cfg;
	struct ap_data data;
	int frozen;
};

struct rte_port_in_action_profile *
rte_port_in_action_profile_create(uint32_t socket_id)
{
	struct rte_port_in_action_profile *ap;

	/* Memory allocation */
	ap = rte_zmalloc_socket(NULL,
		sizeof(struct rte_port_in_action_profile),
		RTE_CACHE_LINE_SIZE,
		socket_id);
	if (ap == NULL)
		return NULL;

	return ap;
}

int
rte_port_in_action_profile_action_register(struct rte_port_in_action_profile *profile,
	enum rte_port_in_action_type type,
	void *action_config)
{
	int status;

	/* Check input arguments */
	if ((profile == NULL) ||
		profile->frozen ||
		(action_valid(type) == 0) ||
		(profile->cfg.action_mask & (1LLU << type)) ||
		((action_cfg_size(type) == 0) && action_config) ||
		(action_cfg_size(type) && (action_config == NULL)))
		return -EINVAL;

	switch (type) {
	case RTE_PORT_IN_ACTION_FLTR:
		status = fltr_cfg_check(action_config);
		break;

	case RTE_PORT_IN_ACTION_LB:
		status = lb_cfg_check(action_config);
		break;

	default:
		status = 0;
		break;
	}

	if (status)
		return status;

	/* Action enable */
	action_cfg_set(&profile->cfg, type, action_config);

	return 0;
}

int
rte_port_in_action_profile_freeze(struct rte_port_in_action_profile *profile)
{
	if (profile->frozen)
		return -EBUSY;

	action_data_offset_set(&profile->data, &profile->cfg);
	profile->frozen = 1;

	return 0;
}

int
rte_port_in_action_profile_free(struct rte_port_in_action_profile *profile)
{
	if (profile == NULL)
		return 0;

	free(profile);
	return 0;
}

/**
 * Action
 */
struct rte_port_in_action {
	struct ap_config cfg;
	struct ap_data data;
	uint8_t memory[0] __rte_cache_aligned;
};

static __rte_always_inline void *
action_data_get(struct rte_port_in_action *action,
	enum rte_port_in_action_type type)
{
	size_t offset = action->data.offset[type];

	return &action->memory[offset];
}

static void
action_data_init(struct rte_port_in_action *action,
	enum rte_port_in_action_type type)
{
	void *data = action_data_get(action, type);

	switch (type) {
	case RTE_PORT_IN_ACTION_FLTR:
		fltr_init(data, &action->cfg.fltr);
		return;

	case RTE_PORT_IN_ACTION_LB:
		lb_init(data, &action->cfg.lb);
		return;

	default:
		return;
	}
}

struct rte_port_in_action *
rte_port_in_action_create(struct rte_port_in_action_profile *profile,
	uint32_t socket_id)
{
	struct rte_port_in_action *action;
	size_t size;
	uint32_t i;

	/* Check input arguments */
	if ((profile == NULL) ||
		(profile->frozen == 0))
		return NULL;

	/* Memory allocation */
	size = sizeof(struct rte_port_in_action) + profile->data.total_size;
	size = RTE_CACHE_LINE_ROUNDUP(size);

	action = rte_zmalloc_socket(NULL,
		size,
		RTE_CACHE_LINE_SIZE,
		socket_id);
	if (action == NULL)
		return NULL;

	/* Initialization */
	memcpy(&action->cfg, &profile->cfg, sizeof(profile->cfg));
	memcpy(&action->data, &profile->data, sizeof(profile->data));

	for (i = 0; i < RTE_PORT_IN_ACTION_MAX; i++)
		if (action->cfg.action_mask & (1LLU << i))
			action_data_init(action,
				(enum rte_port_in_action_type)i);

	return action;
}

int
rte_port_in_action_apply(struct rte_port_in_action *action,
	enum rte_port_in_action_type type,
	void *action_params)
{
	void *action_data;

	/* Check input arguments */
	if ((action == NULL) ||
		(action_valid(type) == 0) ||
		((action->cfg.action_mask & (1LLU << type)) == 0) ||
		(action_params == NULL))
		return -EINVAL;

	/* Data update */
	action_data = action_data_get(action, type);

	switch (type) {
	case RTE_PORT_IN_ACTION_FLTR:
		return fltr_apply(action_data,
			action_params);

	case RTE_PORT_IN_ACTION_LB:
		return lb_apply(action_data,
			action_params);

	default:
		return -EINVAL;
	}
}

static int
ah_filter_on_match(struct rte_pipeline *p,
	struct rte_mbuf **pkts,
	uint32_t n_pkts,
	void *arg)
{
	struct rte_port_in_action *action = arg;
	struct rte_port_in_action_fltr_config *cfg = &action->cfg.fltr;
	uint64_t *key_mask = (uint64_t *) cfg->key_mask;
	uint64_t *key = (uint64_t *) cfg->key;
	uint32_t key_offset = cfg->key_offset;
	struct fltr_data *data = action_data_get(action,
						RTE_PORT_IN_ACTION_FLTR);
	uint32_t i;

	for (i = 0; i < n_pkts; i++) {
		struct rte_mbuf *pkt = pkts[i];
		uint64_t *pkt_key = RTE_MBUF_METADATA_UINT64_PTR(pkt,
					key_offset);

		uint64_t xor0 = (pkt_key[0] & key_mask[0]) ^ key[0];
		uint64_t xor1 = (pkt_key[1] & key_mask[1]) ^ key[1];
		uint64_t or = xor0 | xor1;

		if (or == 0) {
			rte_pipeline_ah_packet_hijack(p, 1LLU << i);
			rte_pipeline_port_out_packet_insert(p,
				data->port_id, pkt);
		}
	}

	return 0;
}

static int
ah_filter_on_mismatch(struct rte_pipeline *p,
	struct rte_mbuf **pkts,
	uint32_t n_pkts,
	void *arg)
{
	struct rte_port_in_action *action = arg;
	struct rte_port_in_action_fltr_config *cfg = &action->cfg.fltr;
	uint64_t *key_mask = (uint64_t *) cfg->key_mask;
	uint64_t *key = (uint64_t *) cfg->key;
	uint32_t key_offset = cfg->key_offset;
	struct fltr_data *data = action_data_get(action,
						RTE_PORT_IN_ACTION_FLTR);
	uint32_t i;

	for (i = 0; i < n_pkts; i++) {
		struct rte_mbuf *pkt = pkts[i];
		uint64_t *pkt_key = RTE_MBUF_METADATA_UINT64_PTR(pkt,
						key_offset);

		uint64_t xor0 = (pkt_key[0] & key_mask[0]) ^ key[0];
		uint64_t xor1 = (pkt_key[1] & key_mask[1]) ^ key[1];
		uint64_t or = xor0 | xor1;

		if (or) {
			rte_pipeline_ah_packet_hijack(p, 1LLU << i);
			rte_pipeline_port_out_packet_insert(p,
				data->port_id, pkt);
		}
	}

	return 0;
}

static int
ah_lb(struct rte_pipeline *p,
	struct rte_mbuf **pkts,
	uint32_t n_pkts,
	void *arg)
{
	struct rte_port_in_action *action = arg;
	struct rte_port_in_action_lb_config *cfg = &action->cfg.lb;
	struct lb_data *data = action_data_get(action, RTE_PORT_IN_ACTION_LB);
	uint64_t pkt_mask = RTE_LEN2MASK(n_pkts, uint64_t);
	uint32_t i;

	rte_pipeline_ah_packet_hijack(p, pkt_mask);

	for (i = 0; i < n_pkts; i++) {
		struct rte_mbuf *pkt = pkts[i];
		uint8_t *pkt_key = RTE_MBUF_METADATA_UINT8_PTR(pkt,
					cfg->key_offset);

		uint64_t digest = cfg->f_hash(pkt_key,
			cfg->key_mask,
			cfg->key_size,
			cfg->seed);
		uint64_t pos = digest & (RTE_PORT_IN_ACTION_LB_TABLE_SIZE - 1);
		uint32_t port_id = data->port_id[pos];

		rte_pipeline_port_out_packet_insert(p, port_id, pkt);
	}

	return 0;
}

static rte_pipeline_port_in_action_handler
ah_selector(struct rte_port_in_action *action)
{
	if (action->cfg.action_mask == 0)
		return NULL;

	if (action->cfg.action_mask == 1LLU << RTE_PORT_IN_ACTION_FLTR)
		return (action->cfg.fltr.filter_on_match) ?
			ah_filter_on_match : ah_filter_on_mismatch;

	if (action->cfg.action_mask == 1LLU << RTE_PORT_IN_ACTION_LB)
		return ah_lb;

	return NULL;
}

int
rte_port_in_action_params_get(struct rte_port_in_action *action,
	struct rte_pipeline_port_in_params *params)
{
	rte_pipeline_port_in_action_handler f_action;

	/* Check input arguments */
	if ((action == NULL) ||
		(params == NULL))
		return -EINVAL;

	f_action = ah_selector(action);

	/* Fill in params */
	params->f_action = f_action;
	params->arg_ah = (f_action) ? action : NULL;

	return 0;
}

int
rte_port_in_action_free(struct rte_port_in_action *action)
{
	if (action == NULL)
		return 0;

	rte_free(action);

	return 0;
}
