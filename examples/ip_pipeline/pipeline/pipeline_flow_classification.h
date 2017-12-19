/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef __INCLUDE_PIPELINE_FLOW_CLASSIFICATION_H__
#define __INCLUDE_PIPELINE_FLOW_CLASSIFICATION_H__

#include "pipeline.h"
#include "pipeline_flow_classification_be.h"

enum flow_key_type {
	FLOW_KEY_QINQ,
	FLOW_KEY_IPV4_5TUPLE,
	FLOW_KEY_IPV6_5TUPLE,
};

struct flow_key_qinq {
	uint16_t svlan;
	uint16_t cvlan;
};

struct flow_key_ipv4_5tuple {
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
	uint32_t proto;
};

struct flow_key_ipv6_5tuple {
	uint8_t ip_src[16];
	uint8_t ip_dst[16];
	uint16_t port_src;
	uint16_t port_dst;
	uint32_t proto;
};

struct pipeline_fc_key {
	enum flow_key_type type;
	union {
		struct flow_key_qinq qinq;
		struct flow_key_ipv4_5tuple ipv4_5tuple;
		struct flow_key_ipv6_5tuple ipv6_5tuple;
	} key;
};

int
app_pipeline_fc_add(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_fc_key *key,
	uint32_t port_id,
	uint32_t flow_id);

int
app_pipeline_fc_add_bulk(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_fc_key *key,
	uint32_t *port_id,
	uint32_t *flow_id,
	uint32_t n_keys);

int
app_pipeline_fc_del(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_fc_key *key);

int
app_pipeline_fc_add_default(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id);

int
app_pipeline_fc_del_default(struct app_params *app,
	uint32_t pipeline_id);

#ifndef APP_PIPELINE_FC_MAX_FLOWS_IN_FILE
#define APP_PIPELINE_FC_MAX_FLOWS_IN_FILE	(16 * 1024 * 1024)
#endif

int
app_pipeline_fc_load_file_qinq(char *filename,
	struct pipeline_fc_key *keys,
	uint32_t *port_ids,
	uint32_t *flow_ids,
	uint32_t *n_keys,
	uint32_t *line);

int
app_pipeline_fc_load_file_ipv4(char *filename,
	struct pipeline_fc_key *keys,
	uint32_t *port_ids,
	uint32_t *flow_ids,
	uint32_t *n_keys,
	uint32_t *line);

int
app_pipeline_fc_load_file_ipv6(char *filename,
	struct pipeline_fc_key *keys,
	uint32_t *port_ids,
	uint32_t *flow_ids,
	uint32_t *n_keys,
	uint32_t *line);

extern struct pipeline_type pipeline_flow_classification;

#endif
