/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef __INCLUDE_PIPELINE_FLOW_ACTIONS_BE_H__
#define __INCLUDE_PIPELINE_FLOW_ACTIONS_BE_H__

#include <rte_meter.h>

#include "pipeline_common_be.h"

#ifndef PIPELINE_FA_N_TC_MAX
#define PIPELINE_FA_N_TC_MAX                               4
#endif

#define PIPELINE_FA_N_DSCP                                 64

struct pipeline_fa_params {
	uint32_t n_flows;
	uint32_t n_meters_per_flow;
	uint32_t flow_id_offset;
	uint32_t ip_hdr_offset;
	uint32_t color_offset;
	uint32_t dscp_enabled;
};

int
pipeline_fa_parse_args(struct pipeline_fa_params *p,
	struct pipeline_params *params);

struct pipeline_fa_policer_action {
	uint32_t drop;
	enum rte_meter_color color;
};

struct pipeline_fa_policer_params {
	struct pipeline_fa_policer_action action[e_RTE_METER_COLORS];
};

struct pipeline_fa_flow_params {
	struct rte_meter_trtcm_params m[PIPELINE_FA_N_TC_MAX];
	struct pipeline_fa_policer_params p[PIPELINE_FA_N_TC_MAX];
	uint32_t port_id;
};

int
pipeline_fa_flow_params_set_default(struct pipeline_fa_flow_params *params);

struct pipeline_fa_policer_stats {
	uint64_t n_pkts[e_RTE_METER_COLORS];
	uint64_t n_pkts_drop;
};

enum pipeline_fa_msg_req_type {
	PIPELINE_FA_MSG_REQ_FLOW_CONFIG = 0,
	PIPELINE_FA_MSG_REQ_FLOW_CONFIG_BULK,
	PIPELINE_FA_MSG_REQ_DSCP_CONFIG,
	PIPELINE_FA_MSG_REQ_POLICER_STATS_READ,
	PIPELINE_FA_MSG_REQS,
};

/*
 * MSG FLOW CONFIG
 */
struct pipeline_fa_flow_config_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_fa_msg_req_type subtype;

	void *entry_ptr;
	uint32_t flow_id;

	uint32_t meter_update_mask;
	uint32_t policer_update_mask;
	uint32_t port_update;
	struct pipeline_fa_flow_params params;
};

struct pipeline_fa_flow_config_msg_rsp {
	int status;
	void *entry_ptr;
};

/*
 * MSG FLOW CONFIG BULK
 */
struct pipeline_fa_flow_config_bulk_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_fa_msg_req_type subtype;

	void **entry_ptr;
	uint32_t *flow_id;
	uint32_t n_flows;

	uint32_t meter_update_mask;
	uint32_t policer_update_mask;
	uint32_t port_update;
	struct pipeline_fa_flow_params *params;
};

struct pipeline_fa_flow_config_bulk_msg_rsp {
	uint32_t n_flows;
};

/*
 * MSG DSCP CONFIG
 */
struct pipeline_fa_dscp_config_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_fa_msg_req_type subtype;

	uint32_t dscp;
	uint32_t traffic_class;
	enum rte_meter_color color;
};

struct pipeline_fa_dscp_config_msg_rsp {
	int status;
};

/*
 * MSG POLICER STATS READ
 */
struct pipeline_fa_policer_stats_msg_req {
	enum pipeline_msg_req_type type;
	enum pipeline_fa_msg_req_type subtype;

	void *entry_ptr;
	uint32_t policer_id;
	int clear;
};

struct pipeline_fa_policer_stats_msg_rsp {
	int status;
	struct pipeline_fa_policer_stats stats;
};

extern struct pipeline_be_ops pipeline_flow_actions_be_ops;

#endif
