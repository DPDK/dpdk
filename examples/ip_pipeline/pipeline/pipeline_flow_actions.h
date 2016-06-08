/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
