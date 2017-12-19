/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef __INCLUDE_PIPELINE_FIREWALL_H__
#define __INCLUDE_PIPELINE_FIREWALL_H__

#include "pipeline.h"
#include "pipeline_firewall_be.h"

int
app_pipeline_firewall_add_rule(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_firewall_key *key,
	uint32_t priority,
	uint32_t port_id);

int
app_pipeline_firewall_delete_rule(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_firewall_key *key);

int
app_pipeline_firewall_add_bulk(struct app_params *app,
		uint32_t pipeline_id,
		struct pipeline_firewall_key *keys,
		uint32_t n_keys,
		uint32_t *priorities,
		uint32_t *port_ids);

int
app_pipeline_firewall_delete_bulk(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_firewall_key *keys,
	uint32_t n_keys);

int
app_pipeline_firewall_add_default_rule(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id);

int
app_pipeline_firewall_delete_default_rule(struct app_params *app,
	uint32_t pipeline_id);

#ifndef APP_PIPELINE_FIREWALL_MAX_RULES_IN_FILE
#define APP_PIPELINE_FIREWALL_MAX_RULES_IN_FILE		65536
#endif

int
app_pipeline_firewall_load_file(char *filename,
	struct pipeline_firewall_key *keys,
	uint32_t *priorities,
	uint32_t *port_ids,
	uint32_t *n_keys,
	uint32_t *line);

extern struct pipeline_type pipeline_firewall;

#endif
