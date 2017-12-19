/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef __INCLUDE_PIPELINE_FLOW_ACTIONS_H__
#define __INCLUDE_PIPELINE_FLOW_ACTIONS_H__

#include <rte_meter.h>

#include "pipeline.h"
#include "pipeline_flow_actions_be.h"

int
app_pipeline_fa_flow_config(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t flow_id,
	uint32_t meter_update_mask,
	uint32_t policer_update_mask,
	uint32_t port_update,
	struct pipeline_fa_flow_params *params);

int
app_pipeline_fa_flow_config_bulk(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t *flow_id,
	uint32_t n_flows,
	uint32_t meter_update_mask,
	uint32_t policer_update_mask,
	uint32_t port_update,
	struct pipeline_fa_flow_params *params);

int
app_pipeline_fa_dscp_config(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t dscp,
	uint32_t traffic_class,
	enum rte_meter_color color);

int
app_pipeline_fa_flow_policer_stats_read(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t flow_id,
	uint32_t policer_id,
	int clear,
	struct pipeline_fa_policer_stats *stats);

#ifndef APP_PIPELINE_FA_MAX_RECORDS_IN_FILE
#define APP_PIPELINE_FA_MAX_RECORDS_IN_FILE		65536
#endif

int
app_pipeline_fa_load_file(char *filename,
	uint32_t *flow_ids,
	struct pipeline_fa_flow_params *p,
	uint32_t *n_flows,
	uint32_t *line);

extern struct pipeline_type pipeline_flow_actions;

#endif
