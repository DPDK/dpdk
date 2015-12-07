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

#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <netinet/in.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_malloc.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_etheraddr.h>

#include "app.h"
#include "pipeline_common_fe.h"
#include "pipeline_flow_actions.h"
#include "hash_func.h"

/*
 * Flow actions pipeline
 */
#ifndef N_FLOWS_BULK
#define N_FLOWS_BULK					4096
#endif

struct app_pipeline_fa_flow {
	struct pipeline_fa_flow_params params;
	void *entry_ptr;
};

struct app_pipeline_fa_dscp {
	uint32_t traffic_class;
	enum rte_meter_color color;
};

struct app_pipeline_fa {
	/* Parameters */
	uint32_t n_ports_in;
	uint32_t n_ports_out;
	struct pipeline_fa_params params;

	/* Flows */
	struct app_pipeline_fa_dscp dscp[PIPELINE_FA_N_DSCP];
	struct app_pipeline_fa_flow *flows;
} __rte_cache_aligned;

static void*
app_pipeline_fa_init(struct pipeline_params *params,
	__rte_unused void *arg)
{
	struct app_pipeline_fa *p;
	uint32_t size, i;

	/* Check input arguments */
	if ((params == NULL) ||
		(params->n_ports_in == 0) ||
		(params->n_ports_out == 0))
		return NULL;

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct app_pipeline_fa));
	p = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	if (p == NULL)
		return NULL;

	/* Initialization */
	p->n_ports_in = params->n_ports_in;
	p->n_ports_out = params->n_ports_out;
	if (pipeline_fa_parse_args(&p->params, params)) {
		rte_free(p);
		return NULL;
	}

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(
		p->params.n_flows * sizeof(struct app_pipeline_fa_flow));
	p->flows = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	if (p->flows == NULL) {
		rte_free(p);
		return NULL;
	}

	/* Initialization of flow table */
	for (i = 0; i < p->params.n_flows; i++)
		pipeline_fa_flow_params_set_default(&p->flows[i].params);

	/* Initialization of DSCP table */
	for (i = 0; i < RTE_DIM(p->dscp); i++) {
		p->dscp[i].traffic_class = 0;
		p->dscp[i].color = e_RTE_METER_GREEN;
	}

	return (void *) p;
}

static int
app_pipeline_fa_free(void *pipeline)
{
	struct app_pipeline_fa *p = pipeline;

	/* Check input arguments */
	if (p == NULL)
		return -1;

	/* Free resources */
	rte_free(p->flows);
	rte_free(p);

	return 0;
}

static int
flow_params_check(struct app_pipeline_fa *p,
	__rte_unused uint32_t meter_update_mask,
	uint32_t policer_update_mask,
	uint32_t port_update,
	struct pipeline_fa_flow_params *params)
{
	uint32_t mask, i;

	/* Meter */

	/* Policer */
	for (i = 0, mask = 1; i < PIPELINE_FA_N_TC_MAX; i++, mask <<= 1) {
		struct pipeline_fa_policer_params *p = &params->p[i];
		uint32_t j;

		if ((mask & policer_update_mask) == 0)
			continue;

		for (j = 0; j < e_RTE_METER_COLORS; j++) {
			struct pipeline_fa_policer_action *action =
				&p->action[j];

			if ((action->drop == 0) &&
				(action->color >= e_RTE_METER_COLORS))
				return -1;
		}
	}

	/* Port */
	if (port_update && (params->port_id >= p->n_ports_out))
		return -1;

	return 0;
}

int
app_pipeline_fa_flow_config(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t flow_id,
	uint32_t meter_update_mask,
	uint32_t policer_update_mask,
	uint32_t port_update,
	struct pipeline_fa_flow_params *params)
{
	struct app_pipeline_fa *p;
	struct app_pipeline_fa_flow *flow;

	struct pipeline_fa_flow_config_msg_req *req;
	struct pipeline_fa_flow_config_msg_rsp *rsp;

	uint32_t i, mask;

	/* Check input arguments */
	if ((app == NULL) ||
		((meter_update_mask == 0) &&
		(policer_update_mask == 0) &&
		(port_update == 0)) ||
		(meter_update_mask >= (1 << PIPELINE_FA_N_TC_MAX)) ||
		(policer_update_mask >= (1 << PIPELINE_FA_N_TC_MAX)) ||
		(params == NULL))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id,
		&pipeline_flow_actions);
	if (p == NULL)
		return -1;

	if (flow_params_check(p,
		meter_update_mask,
		policer_update_mask,
		port_update,
		params) != 0)
		return -1;

	flow_id %= p->params.n_flows;
	flow = &p->flows[flow_id];

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_FA_MSG_REQ_FLOW_CONFIG;
	req->entry_ptr = flow->entry_ptr;
	req->flow_id = flow_id;
	req->meter_update_mask = meter_update_mask;
	req->policer_update_mask = policer_update_mask;
	req->port_update = port_update;
	memcpy(&req->params, params, sizeof(*params));

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response */
	if (rsp->status ||
		(rsp->entry_ptr == NULL)) {
		app_msg_free(app, rsp);
		return -1;
	}

	/* Commit flow */
	for (i = 0, mask = 1; i < PIPELINE_FA_N_TC_MAX; i++, mask <<= 1) {
		if ((mask & meter_update_mask) == 0)
			continue;

		memcpy(&flow->params.m[i], &params->m[i], sizeof(params->m[i]));
	}

	for (i = 0, mask = 1; i < PIPELINE_FA_N_TC_MAX; i++, mask <<= 1) {
		if ((mask & policer_update_mask) == 0)
			continue;

		memcpy(&flow->params.p[i], &params->p[i], sizeof(params->p[i]));
	}

	if (port_update)
		flow->params.port_id = params->port_id;

	flow->entry_ptr = rsp->entry_ptr;

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_fa_flow_config_bulk(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t *flow_id,
	uint32_t n_flows,
	uint32_t meter_update_mask,
	uint32_t policer_update_mask,
	uint32_t port_update,
	struct pipeline_fa_flow_params *params)
{
	struct app_pipeline_fa *p;
	struct pipeline_fa_flow_config_bulk_msg_req *req;
	struct pipeline_fa_flow_config_bulk_msg_rsp *rsp;
	void **req_entry_ptr;
	uint32_t *req_flow_id;
	uint32_t i;

	/* Check input arguments */
	if ((app == NULL) ||
		(flow_id == NULL) ||
		(n_flows == 0) ||
		((meter_update_mask == 0) &&
		(policer_update_mask == 0) &&
		(port_update == 0)) ||
		(meter_update_mask >= (1 << PIPELINE_FA_N_TC_MAX)) ||
		(policer_update_mask >= (1 << PIPELINE_FA_N_TC_MAX)) ||
		(params == NULL))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id,
		&pipeline_flow_actions);
	if (p == NULL)
		return -1;

	for (i = 0; i < n_flows; i++) {
		struct pipeline_fa_flow_params *flow_params = &params[i];

		if (flow_params_check(p,
			meter_update_mask,
			policer_update_mask,
			port_update,
			flow_params) != 0)
			return -1;
	}

	/* Allocate and write request */
	req_entry_ptr = (void **) rte_malloc(NULL,
		n_flows * sizeof(void *),
		RTE_CACHE_LINE_SIZE);
	if (req_entry_ptr == NULL)
		return -1;

	req_flow_id = (uint32_t *) rte_malloc(NULL,
		n_flows * sizeof(uint32_t),
		RTE_CACHE_LINE_SIZE);
	if (req_flow_id == NULL) {
		rte_free(req_entry_ptr);
		return -1;
	}

	for (i = 0; i < n_flows; i++) {
		uint32_t fid = flow_id[i] % p->params.n_flows;
		struct app_pipeline_fa_flow *flow = &p->flows[fid];

		req_flow_id[i] = fid;
		req_entry_ptr[i] = flow->entry_ptr;
	}

	req = app_msg_alloc(app);
	if (req == NULL) {
		rte_free(req_flow_id);
		rte_free(req_entry_ptr);
		return -1;
	}

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_FA_MSG_REQ_FLOW_CONFIG_BULK;
	req->entry_ptr = req_entry_ptr;
	req->flow_id = req_flow_id;
	req->n_flows = n_flows;
	req->meter_update_mask = meter_update_mask;
	req->policer_update_mask = policer_update_mask;
	req->port_update = port_update;
	req->params = params;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL) {
		rte_free(req_flow_id);
		rte_free(req_entry_ptr);
		return -1;
	}

	/* Read response */

	/* Commit flows */
	for (i = 0; i < rsp->n_flows; i++) {
		uint32_t fid = flow_id[i] % p->params.n_flows;
		struct app_pipeline_fa_flow *flow = &p->flows[fid];
		struct pipeline_fa_flow_params *flow_params = &params[i];
		void *entry_ptr = req_entry_ptr[i];
		uint32_t j, mask;

		for (j = 0, mask = 1; j < PIPELINE_FA_N_TC_MAX;
			j++, mask <<= 1) {
			if ((mask & meter_update_mask) == 0)
				continue;

			memcpy(&flow->params.m[j],
				&flow_params->m[j],
				sizeof(flow_params->m[j]));
		}

		for (j = 0, mask = 1; j < PIPELINE_FA_N_TC_MAX;
			j++, mask <<= 1) {
			if ((mask & policer_update_mask) == 0)
				continue;

			memcpy(&flow->params.p[j],
				&flow_params->p[j],
				sizeof(flow_params->p[j]));
		}

		if (port_update)
			flow->params.port_id = flow_params->port_id;

		flow->entry_ptr = entry_ptr;
	}

	/* Free response */
	app_msg_free(app, rsp);
	rte_free(req_flow_id);
	rte_free(req_entry_ptr);

	return (rsp->n_flows == n_flows) ? 0 : -1;
}

int
app_pipeline_fa_dscp_config(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t dscp,
	uint32_t traffic_class,
	enum rte_meter_color color)
{
	struct app_pipeline_fa *p;

	struct pipeline_fa_dscp_config_msg_req *req;
	struct pipeline_fa_dscp_config_msg_rsp *rsp;

	/* Check input arguments */
	if ((app == NULL) ||
		(dscp >= PIPELINE_FA_N_DSCP) ||
		(traffic_class >= PIPELINE_FA_N_TC_MAX) ||
		(color >= e_RTE_METER_COLORS))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id,
		&pipeline_flow_actions);
	if (p == NULL)
		return -1;

	if (p->params.dscp_enabled == 0)
		return -1;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_FA_MSG_REQ_DSCP_CONFIG;
	req->dscp = dscp;
	req->traffic_class = traffic_class;
	req->color = color;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response */
	if (rsp->status) {
		app_msg_free(app, rsp);
		return -1;
	}

	/* Commit DSCP */
	p->dscp[dscp].traffic_class = traffic_class;
	p->dscp[dscp].color = color;

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_fa_flow_policer_stats_read(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t flow_id,
	uint32_t policer_id,
	int clear,
	struct pipeline_fa_policer_stats *stats)
{
	struct app_pipeline_fa *p;
	struct app_pipeline_fa_flow *flow;

	struct pipeline_fa_policer_stats_msg_req *req;
	struct pipeline_fa_policer_stats_msg_rsp *rsp;

	/* Check input arguments */
	if ((app == NULL) || (stats == NULL))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id,
		&pipeline_flow_actions);
	if (p == NULL)
		return -1;

	flow_id %= p->params.n_flows;
	flow = &p->flows[flow_id];

	if ((policer_id >= p->params.n_meters_per_flow) ||
		(flow->entry_ptr == NULL))
		return -1;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_FA_MSG_REQ_POLICER_STATS_READ;
	req->entry_ptr = flow->entry_ptr;
	req->policer_id = policer_id;
	req->clear = clear;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response */
	if (rsp->status) {
		app_msg_free(app, rsp);
		return -1;
	}

	memcpy(stats, &rsp->stats, sizeof(*stats));

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

static const char *
color_to_string(enum rte_meter_color color)
{
	switch (color) {
	case e_RTE_METER_GREEN: return "G";
	case e_RTE_METER_YELLOW: return "Y";
	case e_RTE_METER_RED: return "R";
	default: return "?";
	}
}

static int
string_to_color(char *s, enum rte_meter_color *c)
{
	if (strcmp(s, "G") == 0) {
		*c = e_RTE_METER_GREEN;
		return 0;
	}

	if (strcmp(s, "Y") == 0) {
		*c = e_RTE_METER_YELLOW;
		return 0;
	}

	if (strcmp(s, "R") == 0) {
		*c = e_RTE_METER_RED;
		return 0;
	}

	return -1;
}

static const char *
policer_action_to_string(struct pipeline_fa_policer_action *a)
{
	if (a->drop)
		return "D";

	return color_to_string(a->color);
}

static int
string_to_policer_action(char *s, struct pipeline_fa_policer_action *a)
{
	if (strcmp(s, "G") == 0) {
		a->drop = 0;
		a->color = e_RTE_METER_GREEN;
		return 0;
	}

	if (strcmp(s, "Y") == 0) {
		a->drop = 0;
		a->color = e_RTE_METER_YELLOW;
		return 0;
	}

	if (strcmp(s, "R") == 0) {
		a->drop = 0;
		a->color = e_RTE_METER_RED;
		return 0;
	}

	if (strcmp(s, "D") == 0) {
		a->drop = 1;
		a->color = e_RTE_METER_GREEN;
		return 0;
	}

	return -1;
}

static void
print_flow(struct app_pipeline_fa *p,
	uint32_t flow_id,
	struct app_pipeline_fa_flow *flow)
{
	uint32_t i;

	printf("Flow ID = %" PRIu32 "\n", flow_id);

	for (i = 0; i < p->params.n_meters_per_flow; i++) {
		struct rte_meter_trtcm_params *meter = &flow->params.m[i];
		struct pipeline_fa_policer_params *policer = &flow->params.p[i];

	printf("\ttrTCM [CIR = %" PRIu64
		", CBS = %" PRIu64 ", PIR = %" PRIu64
		", PBS = %" PRIu64	"] Policer [G : %s, Y : %s, R : %s]\n",
		meter->cir,
		meter->cbs,
		meter->pir,
		meter->pbs,
		policer_action_to_string(&policer->action[e_RTE_METER_GREEN]),
		policer_action_to_string(&policer->action[e_RTE_METER_YELLOW]),
		policer_action_to_string(&policer->action[e_RTE_METER_RED]));
	}

	printf("\tPort %u (entry_ptr = %p)\n",
		flow->params.port_id,
		flow->entry_ptr);
}


static int
app_pipeline_fa_flow_ls(struct app_params *app,
		uint32_t pipeline_id)
{
	struct app_pipeline_fa *p;
	uint32_t i;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id,
		&pipeline_flow_actions);
	if (p == NULL)
		return -1;

	for (i = 0; i < p->params.n_flows; i++) {
		struct app_pipeline_fa_flow *flow = &p->flows[i];

		print_flow(p, i, flow);
	}

	return 0;
}

static int
app_pipeline_fa_dscp_ls(struct app_params *app,
		uint32_t pipeline_id)
{
	struct app_pipeline_fa *p;
	uint32_t i;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id,
		&pipeline_flow_actions);
	if (p == NULL)
		return -1;

	if (p->params.dscp_enabled == 0)
		return -1;

	for (i = 0; i < RTE_DIM(p->dscp); i++) {
		struct app_pipeline_fa_dscp *dscp =	&p->dscp[i];

		printf("DSCP = %2" PRIu32 ": Traffic class = %" PRIu32
			", Color = %s\n",
			i,
			dscp->traffic_class,
			color_to_string(dscp->color));
	}

	return 0;
}

/*
 * Flow meter configuration (single flow)
 *
 * p <pipeline ID> flow <flow ID> meter <meter ID> trtcm <trtcm params>
 */

struct cmd_fa_meter_config_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	uint32_t flow_id;
	cmdline_fixed_string_t meter_string;
	uint32_t meter_id;
	cmdline_fixed_string_t trtcm_string;
	uint64_t cir;
	uint64_t pir;
	uint64_t cbs;
	uint64_t pbs;
};

static void
cmd_fa_meter_config_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fa_meter_config_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fa_flow_params flow_params;
	int status;

	if (params->meter_id >= PIPELINE_FA_N_TC_MAX) {
		printf("Command failed\n");
		return;
	}

	flow_params.m[params->meter_id].cir = params->cir;
	flow_params.m[params->meter_id].pir = params->pir;
	flow_params.m[params->meter_id].cbs = params->cbs;
	flow_params.m[params->meter_id].pbs = params->pbs;

	status = app_pipeline_fa_flow_config(app,
		params->pipeline_id,
		params->flow_id,
		1 << params->meter_id,
		0,
		0,
		&flow_params);

	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fa_meter_config_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_meter_config_result,
	p_string, "p");

cmdline_parse_token_num_t cmd_fa_meter_config_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_result,
	pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fa_meter_config_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_meter_config_result,
	flow_string, "flow");

cmdline_parse_token_num_t cmd_fa_meter_config_flow_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_result,
	flow_id, UINT32);

cmdline_parse_token_string_t cmd_fa_meter_config_meter_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_meter_config_result,
	meter_string, "meter");

cmdline_parse_token_num_t cmd_fa_meter_config_meter_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_result,
	meter_id, UINT32);

cmdline_parse_token_string_t cmd_fa_meter_config_trtcm_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_meter_config_result,
	trtcm_string, "trtcm");

cmdline_parse_token_num_t cmd_fa_meter_config_cir =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_result, cir, UINT64);

cmdline_parse_token_num_t cmd_fa_meter_config_pir =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_result, pir, UINT64);

cmdline_parse_token_num_t cmd_fa_meter_config_cbs =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_result, cbs, UINT64);

cmdline_parse_token_num_t cmd_fa_meter_config_pbs =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_result, pbs, UINT64);

cmdline_parse_inst_t cmd_fa_meter_config = {
	.f = cmd_fa_meter_config_parsed,
	.data = NULL,
	.help_str = "Flow meter configuration (single flow) ",
	.tokens = {
		(void *) &cmd_fa_meter_config_p_string,
		(void *) &cmd_fa_meter_config_pipeline_id,
		(void *) &cmd_fa_meter_config_flow_string,
		(void *) &cmd_fa_meter_config_flow_id,
		(void *) &cmd_fa_meter_config_meter_string,
		(void *) &cmd_fa_meter_config_meter_id,
		(void *) &cmd_fa_meter_config_trtcm_string,
		(void *) &cmd_fa_meter_config_cir,
		(void *) &cmd_fa_meter_config_pir,
		(void *) &cmd_fa_meter_config_cbs,
		(void *) &cmd_fa_meter_config_pbs,
		NULL,
	},
};

/*
 * Flow meter configuration (multiple flows)
 *
 * p <pipeline ID> flows <n_flows> meter <meter ID> trtcm <trtcm params>
 */

struct cmd_fa_meter_config_bulk_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flows_string;
	uint32_t n_flows;
	cmdline_fixed_string_t meter_string;
	uint32_t meter_id;
	cmdline_fixed_string_t trtcm_string;
	uint64_t cir;
	uint64_t pir;
	uint64_t cbs;
	uint64_t pbs;
};

static void
cmd_fa_meter_config_bulk_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fa_meter_config_bulk_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fa_flow_params flow_template, *flow_params;
	uint32_t *flow_id;
	uint32_t i;

	if ((params->n_flows == 0) ||
		(params->meter_id >= PIPELINE_FA_N_TC_MAX)) {
		printf("Invalid arguments\n");
		return;
	}

	flow_id = (uint32_t *) rte_malloc(NULL,
		N_FLOWS_BULK * sizeof(uint32_t),
		RTE_CACHE_LINE_SIZE);
	if (flow_id == NULL) {
		printf("Memory allocation failed\n");
		return;
	}

	flow_params = (struct pipeline_fa_flow_params *) rte_malloc(NULL,
		N_FLOWS_BULK * sizeof(struct pipeline_fa_flow_params),
		RTE_CACHE_LINE_SIZE);
	if (flow_params == NULL) {
		rte_free(flow_id);
		printf("Memory allocation failed\n");
		return;
	}

	memset(&flow_template, 0, sizeof(flow_template));
	flow_template.m[params->meter_id].cir = params->cir;
	flow_template.m[params->meter_id].pir = params->pir;
	flow_template.m[params->meter_id].cbs = params->cbs;
	flow_template.m[params->meter_id].pbs = params->pbs;

	for (i = 0; i < params->n_flows; i++) {
		uint32_t pos = i % N_FLOWS_BULK;

		flow_id[pos] = i;
		memcpy(&flow_params[pos],
			&flow_template,
			sizeof(flow_template));

		if ((pos == N_FLOWS_BULK - 1) ||
			(i == params->n_flows - 1)) {
			int status;

			status = app_pipeline_fa_flow_config_bulk(app,
				params->pipeline_id,
				flow_id,
				pos + 1,
				1 << params->meter_id,
				0,
				0,
				flow_params);

			if (status != 0) {
				printf("Command failed\n");

				break;
			}
		}
	}

	rte_free(flow_params);
	rte_free(flow_id);

}

cmdline_parse_token_string_t cmd_fa_meter_config_bulk_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_meter_config_bulk_result,
	p_string, "p");

cmdline_parse_token_num_t cmd_fa_meter_config_bulk_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_bulk_result,
	pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fa_meter_config_bulk_flows_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_meter_config_bulk_result,
	flows_string, "flows");

cmdline_parse_token_num_t cmd_fa_meter_config_bulk_n_flows =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_bulk_result,
	n_flows, UINT32);

cmdline_parse_token_string_t cmd_fa_meter_config_bulk_meter_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_meter_config_bulk_result,
	meter_string, "meter");

cmdline_parse_token_num_t cmd_fa_meter_config_bulk_meter_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_bulk_result,
	meter_id, UINT32);

cmdline_parse_token_string_t cmd_fa_meter_config_bulk_trtcm_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_meter_config_bulk_result,
	trtcm_string, "trtcm");

cmdline_parse_token_num_t cmd_fa_meter_config_bulk_cir =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_bulk_result,
	cir, UINT64);

cmdline_parse_token_num_t cmd_fa_meter_config_bulk_pir =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_bulk_result,
	pir, UINT64);

cmdline_parse_token_num_t cmd_fa_meter_config_bulk_cbs =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_bulk_result,
	cbs, UINT64);

cmdline_parse_token_num_t cmd_fa_meter_config_bulk_pbs =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_meter_config_bulk_result,
	pbs, UINT64);

cmdline_parse_inst_t cmd_fa_meter_config_bulk = {
	.f = cmd_fa_meter_config_bulk_parsed,
	.data = NULL,
	.help_str = "Flow meter configuration (multiple flows)",
	.tokens = {
		(void *) &cmd_fa_meter_config_bulk_p_string,
		(void *) &cmd_fa_meter_config_bulk_pipeline_id,
		(void *) &cmd_fa_meter_config_bulk_flows_string,
		(void *) &cmd_fa_meter_config_bulk_n_flows,
		(void *) &cmd_fa_meter_config_bulk_meter_string,
		(void *) &cmd_fa_meter_config_bulk_meter_id,
		(void *) &cmd_fa_meter_config_bulk_trtcm_string,
		(void *) &cmd_fa_meter_config_cir,
		(void *) &cmd_fa_meter_config_pir,
		(void *) &cmd_fa_meter_config_cbs,
		(void *) &cmd_fa_meter_config_pbs,
		NULL,
	},
};

/*
 * Flow policer configuration (single flow)
 *
 * p <pipeline ID> flow <flow ID> policer <policer ID>
 *    G <action> Y <action> R <action>
 *
 * <action> = G (green) | Y (yellow) | R (red) | D (drop)
 */

struct cmd_fa_policer_config_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	uint32_t flow_id;
	cmdline_fixed_string_t policer_string;
	uint32_t policer_id;
	cmdline_fixed_string_t green_string;
	cmdline_fixed_string_t g_action;
	cmdline_fixed_string_t yellow_string;
	cmdline_fixed_string_t y_action;
	cmdline_fixed_string_t red_string;
	cmdline_fixed_string_t r_action;
};

static void
cmd_fa_policer_config_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fa_policer_config_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fa_flow_params flow_params;
	int status;

	if (params->policer_id >= PIPELINE_FA_N_TC_MAX) {
		printf("Command failed\n");
		return;
	}

	status = string_to_policer_action(params->g_action,
		&flow_params.p[params->policer_id].action[e_RTE_METER_GREEN]);
	if (status)
		printf("Invalid policer green action\n");

	status = string_to_policer_action(params->y_action,
		&flow_params.p[params->policer_id].action[e_RTE_METER_YELLOW]);
	if (status)
		printf("Invalid policer yellow action\n");

	status = string_to_policer_action(params->r_action,
		&flow_params.p[params->policer_id].action[e_RTE_METER_RED]);
	if (status)
		printf("Invalid policer red action\n");

	status = app_pipeline_fa_flow_config(app,
		params->pipeline_id,
		params->flow_id,
		0,
		1 << params->policer_id,
		0,
		&flow_params);

	if (status != 0)
		printf("Command failed\n");

}

cmdline_parse_token_string_t cmd_fa_policer_config_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_result,
	p_string, "p");

cmdline_parse_token_num_t cmd_fa_policer_config_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_policer_config_result,
	pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fa_policer_config_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_result,
	flow_string, "flow");

cmdline_parse_token_num_t cmd_fa_policer_config_flow_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_policer_config_result,
	flow_id, UINT32);

cmdline_parse_token_string_t cmd_fa_policer_config_policer_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_result,
	policer_string, "policer");

cmdline_parse_token_num_t cmd_fa_policer_config_policer_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_policer_config_result,
	policer_id, UINT32);

cmdline_parse_token_string_t cmd_fa_policer_config_green_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_result,
	green_string, "G");

cmdline_parse_token_string_t cmd_fa_policer_config_g_action =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_result,
	g_action, "R#Y#G#D");

cmdline_parse_token_string_t cmd_fa_policer_config_yellow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_result,
	yellow_string, "Y");

cmdline_parse_token_string_t cmd_fa_policer_config_y_action =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_result,
	y_action, "R#Y#G#D");

cmdline_parse_token_string_t cmd_fa_policer_config_red_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_result,
	red_string, "R");

cmdline_parse_token_string_t cmd_fa_policer_config_r_action =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_result,
	r_action, "R#Y#G#D");

cmdline_parse_inst_t cmd_fa_policer_config = {
	.f = cmd_fa_policer_config_parsed,
	.data = NULL,
	.help_str = "Flow policer configuration (single flow)",
	.tokens = {
		(void *) &cmd_fa_policer_config_p_string,
		(void *) &cmd_fa_policer_config_pipeline_id,
		(void *) &cmd_fa_policer_config_flow_string,
		(void *) &cmd_fa_policer_config_flow_id,
		(void *) &cmd_fa_policer_config_policer_string,
		(void *) &cmd_fa_policer_config_policer_id,
		(void *) &cmd_fa_policer_config_green_string,
		(void *) &cmd_fa_policer_config_g_action,
		(void *) &cmd_fa_policer_config_yellow_string,
		(void *) &cmd_fa_policer_config_y_action,
		(void *) &cmd_fa_policer_config_red_string,
		(void *) &cmd_fa_policer_config_r_action,
		NULL,
	},
};

/*
 * Flow policer configuration (multiple flows)
 *
 * p <pipeline ID> flows <n_flows> policer <policer ID>
 *    G <action> Y <action> R <action>
 *
 * <action> = G (green) | Y (yellow) | R (red) | D (drop)
 */

struct cmd_fa_policer_config_bulk_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flows_string;
	uint32_t n_flows;
	cmdline_fixed_string_t policer_string;
	uint32_t policer_id;
	cmdline_fixed_string_t green_string;
	cmdline_fixed_string_t g_action;
	cmdline_fixed_string_t yellow_string;
	cmdline_fixed_string_t y_action;
	cmdline_fixed_string_t red_string;
	cmdline_fixed_string_t r_action;
};

static void
cmd_fa_policer_config_bulk_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fa_policer_config_bulk_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fa_flow_params flow_template, *flow_params;
	uint32_t *flow_id, i;
	int status;

	if ((params->n_flows == 0) ||
		(params->policer_id >= PIPELINE_FA_N_TC_MAX)) {
		printf("Invalid arguments\n");
		return;
	}

	flow_id = (uint32_t *) rte_malloc(NULL,
		N_FLOWS_BULK * sizeof(uint32_t),
		RTE_CACHE_LINE_SIZE);
	if (flow_id == NULL) {
		printf("Memory allocation failed\n");
		return;
	}

	flow_params = (struct pipeline_fa_flow_params *) rte_malloc(NULL,
		N_FLOWS_BULK * sizeof(struct pipeline_fa_flow_params),
		RTE_CACHE_LINE_SIZE);
	if (flow_params == NULL) {
		rte_free(flow_id);
		printf("Memory allocation failed\n");
		return;
	}

	memset(&flow_template, 0, sizeof(flow_template));

	status = string_to_policer_action(params->g_action,
		&flow_template.p[params->policer_id].action[e_RTE_METER_GREEN]);
	if (status)
		printf("Invalid policer green action\n");

	status = string_to_policer_action(params->y_action,
	 &flow_template.p[params->policer_id].action[e_RTE_METER_YELLOW]);
	if (status)
		printf("Invalid policer yellow action\n");

	status = string_to_policer_action(params->r_action,
		&flow_template.p[params->policer_id].action[e_RTE_METER_RED]);
	if (status)
		printf("Invalid policer red action\n");

	for (i = 0; i < params->n_flows; i++) {
		uint32_t pos = i % N_FLOWS_BULK;

		flow_id[pos] = i;
		memcpy(&flow_params[pos], &flow_template,
			sizeof(flow_template));

		if ((pos == N_FLOWS_BULK - 1) ||
			(i == params->n_flows - 1)) {
			int status;

			status = app_pipeline_fa_flow_config_bulk(app,
				params->pipeline_id,
				flow_id,
				pos + 1,
				0,
				1 << params->policer_id,
				0,
				flow_params);

			if (status != 0) {
				printf("Command failed\n");

				break;
			}
		}
	}

	rte_free(flow_params);
	rte_free(flow_id);

}

cmdline_parse_token_string_t cmd_fa_policer_config_bulk_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_bulk_result,
	p_string, "p");

cmdline_parse_token_num_t cmd_fa_policer_config_bulk_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_policer_config_bulk_result,
	pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fa_policer_config_bulk_flows_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_bulk_result,
	flows_string, "flows");

cmdline_parse_token_num_t cmd_fa_policer_config_bulk_n_flows =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_policer_config_bulk_result,
	n_flows, UINT32);

cmdline_parse_token_string_t cmd_fa_policer_config_bulk_policer_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_bulk_result,
	policer_string, "policer");

cmdline_parse_token_num_t cmd_fa_policer_config_bulk_policer_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_policer_config_bulk_result,
	policer_id, UINT32);

cmdline_parse_token_string_t cmd_fa_policer_config_bulk_green_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_bulk_result,
	green_string, "G");

cmdline_parse_token_string_t cmd_fa_policer_config_bulk_g_action =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_bulk_result,
	g_action, "R#Y#G#D");

cmdline_parse_token_string_t cmd_fa_policer_config_bulk_yellow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_bulk_result,
	yellow_string, "Y");

cmdline_parse_token_string_t cmd_fa_policer_config_bulk_y_action =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_bulk_result,
	y_action, "R#Y#G#D");

cmdline_parse_token_string_t cmd_fa_policer_config_bulk_red_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_bulk_result,
	red_string, "R");

cmdline_parse_token_string_t cmd_fa_policer_config_bulk_r_action =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_config_bulk_result,
	r_action, "R#Y#G#D");

cmdline_parse_inst_t cmd_fa_policer_config_bulk = {
	.f = cmd_fa_policer_config_bulk_parsed,
	.data = NULL,
	.help_str = "Flow policer configuration (multiple flows)",
	.tokens = {
		(void *) &cmd_fa_policer_config_bulk_p_string,
		(void *) &cmd_fa_policer_config_bulk_pipeline_id,
		(void *) &cmd_fa_policer_config_bulk_flows_string,
		(void *) &cmd_fa_policer_config_bulk_n_flows,
		(void *) &cmd_fa_policer_config_bulk_policer_string,
		(void *) &cmd_fa_policer_config_bulk_policer_id,
		(void *) &cmd_fa_policer_config_bulk_green_string,
		(void *) &cmd_fa_policer_config_bulk_g_action,
		(void *) &cmd_fa_policer_config_bulk_yellow_string,
		(void *) &cmd_fa_policer_config_bulk_y_action,
		(void *) &cmd_fa_policer_config_bulk_red_string,
		(void *) &cmd_fa_policer_config_bulk_r_action,
		NULL,
	},
};

/*
 * Flow output port configuration (single flow)
 *
 * p <pipeline ID> flow <flow ID> port <port ID>
 */

struct cmd_fa_output_port_config_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	uint32_t flow_id;
	cmdline_fixed_string_t port_string;
	uint32_t port_id;
};

static void
cmd_fa_output_port_config_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fa_output_port_config_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fa_flow_params flow_params;
	int status;

	flow_params.port_id = params->port_id;

	status = app_pipeline_fa_flow_config(app,
		params->pipeline_id,
		params->flow_id,
		0,
		0,
		1,
		&flow_params);

	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fa_output_port_config_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_output_port_config_result,
	p_string, "p");

cmdline_parse_token_num_t cmd_fa_output_port_config_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_output_port_config_result,
	pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fa_output_port_config_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_output_port_config_result,
	flow_string, "flow");

cmdline_parse_token_num_t cmd_fa_output_port_config_flow_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_output_port_config_result,
	flow_id, UINT32);

cmdline_parse_token_string_t cmd_fa_output_port_config_port_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_output_port_config_result,
	port_string, "port");

cmdline_parse_token_num_t cmd_fa_output_port_config_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_output_port_config_result,
	port_id, UINT32);

cmdline_parse_inst_t cmd_fa_output_port_config = {
	.f = cmd_fa_output_port_config_parsed,
	.data = NULL,
	.help_str = "Flow output port configuration (single flow)",
	.tokens = {
		(void *) &cmd_fa_output_port_config_p_string,
		(void *) &cmd_fa_output_port_config_pipeline_id,
		(void *) &cmd_fa_output_port_config_flow_string,
		(void *) &cmd_fa_output_port_config_flow_id,
		(void *) &cmd_fa_output_port_config_port_string,
		(void *) &cmd_fa_output_port_config_port_id,
		NULL,
	},
};

/*
 * Flow output port configuration (multiple flows)
 *
 * p <pipeline ID> flows <n_flows> ports <n_ports>
 */

struct cmd_fa_output_port_config_bulk_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flows_string;
	uint32_t n_flows;
	cmdline_fixed_string_t ports_string;
	uint32_t n_ports;
};

static void
cmd_fa_output_port_config_bulk_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fa_output_port_config_bulk_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fa_flow_params *flow_params;
	uint32_t *flow_id;
	uint32_t i;

	if (params->n_flows == 0) {
		printf("Invalid arguments\n");
		return;
	}

	flow_id = (uint32_t *) rte_malloc(NULL,
		N_FLOWS_BULK * sizeof(uint32_t),
		RTE_CACHE_LINE_SIZE);
	if (flow_id == NULL) {
		printf("Memory allocation failed\n");
		return;
	}

	flow_params = (struct pipeline_fa_flow_params *) rte_malloc(NULL,
		N_FLOWS_BULK * sizeof(struct pipeline_fa_flow_params),
		RTE_CACHE_LINE_SIZE);
	if (flow_params == NULL) {
		rte_free(flow_id);
		printf("Memory allocation failed\n");
		return;
	}

	for (i = 0; i < params->n_flows; i++) {
		uint32_t pos = i % N_FLOWS_BULK;
		uint32_t port_id = i % params->n_ports;

		flow_id[pos] = i;

		memset(&flow_params[pos], 0, sizeof(flow_params[pos]));
		flow_params[pos].port_id = port_id;

		if ((pos == N_FLOWS_BULK - 1) ||
			(i == params->n_flows - 1)) {
			int status;

			status = app_pipeline_fa_flow_config_bulk(app,
				params->pipeline_id,
				flow_id,
				pos + 1,
				0,
				0,
				1,
				flow_params);

			if (status != 0) {
				printf("Command failed\n");

				break;
			}
		}
	}

	rte_free(flow_params);
	rte_free(flow_id);

}

cmdline_parse_token_string_t cmd_fa_output_port_config_bulk_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_output_port_config_bulk_result,
	p_string, "p");

cmdline_parse_token_num_t cmd_fa_output_port_config_bulk_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_output_port_config_bulk_result,
	pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fa_output_port_config_bulk_flows_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_output_port_config_bulk_result,
	flows_string, "flows");

cmdline_parse_token_num_t cmd_fa_output_port_config_bulk_n_flows =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_output_port_config_bulk_result,
	n_flows, UINT32);

cmdline_parse_token_string_t cmd_fa_output_port_config_bulk_ports_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_output_port_config_bulk_result,
	ports_string, "ports");

cmdline_parse_token_num_t cmd_fa_output_port_config_bulk_n_ports =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_output_port_config_bulk_result,
	n_ports, UINT32);

cmdline_parse_inst_t cmd_fa_output_port_config_bulk = {
	.f = cmd_fa_output_port_config_bulk_parsed,
	.data = NULL,
	.help_str = "Flow output port configuration (multiple flows)",
	.tokens = {
		(void *) &cmd_fa_output_port_config_bulk_p_string,
		(void *) &cmd_fa_output_port_config_bulk_pipeline_id,
		(void *) &cmd_fa_output_port_config_bulk_flows_string,
		(void *) &cmd_fa_output_port_config_bulk_n_flows,
		(void *) &cmd_fa_output_port_config_bulk_ports_string,
		(void *) &cmd_fa_output_port_config_bulk_n_ports,
		NULL,
	},
};

/*
 * Flow DiffServ Code Point (DSCP) translation table configuration
 *
 * p <pipeline ID> dscp <DSCP ID> class <traffic class ID> color <color>
 *
 * <color> = G (green) | Y (yellow) | R (red)
*/

struct cmd_fa_dscp_config_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t dscp_string;
	uint32_t dscp_id;
	cmdline_fixed_string_t class_string;
	uint32_t traffic_class_id;
	cmdline_fixed_string_t color_string;
	cmdline_fixed_string_t color;

};

static void
cmd_fa_dscp_config_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fa_dscp_config_result *params = parsed_result;
	struct app_params *app = data;
	enum rte_meter_color color;
	int status;

	status = string_to_color(params->color, &color);
	if (status) {
		printf("Invalid color\n");
		return;
	}

	status = app_pipeline_fa_dscp_config(app,
		params->pipeline_id,
		params->dscp_id,
		params->traffic_class_id,
		color);

	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fa_dscp_config_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_dscp_config_result,
	p_string, "p");

cmdline_parse_token_num_t cmd_fa_dscp_config_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_dscp_config_result,
	pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fa_dscp_config_dscp_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_dscp_config_result,
	dscp_string, "dscp");

cmdline_parse_token_num_t cmd_fa_dscp_config_dscp_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_dscp_config_result,
	dscp_id, UINT32);

cmdline_parse_token_string_t cmd_fa_dscp_config_class_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_dscp_config_result,
	class_string, "class");

cmdline_parse_token_num_t cmd_fa_dscp_config_traffic_class_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_dscp_config_result,
	traffic_class_id, UINT32);

cmdline_parse_token_string_t cmd_fa_dscp_config_color_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_dscp_config_result,
	color_string, "color");

cmdline_parse_token_string_t cmd_fa_dscp_config_color =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_dscp_config_result,
	color, "G#Y#R");

cmdline_parse_inst_t cmd_fa_dscp_config = {
	.f = cmd_fa_dscp_config_parsed,
	.data = NULL,
	.help_str = "Flow DSCP translation table configuration",
	.tokens = {
		(void *) &cmd_fa_dscp_config_p_string,
		(void *) &cmd_fa_dscp_config_pipeline_id,
		(void *) &cmd_fa_dscp_config_dscp_string,
		(void *) &cmd_fa_dscp_config_dscp_id,
		(void *) &cmd_fa_dscp_config_class_string,
		(void *) &cmd_fa_dscp_config_traffic_class_id,
		(void *) &cmd_fa_dscp_config_color_string,
		(void *) &cmd_fa_dscp_config_color,
		NULL,
	},
};

/*
 * Flow policer stats read
 *
 * p <pipeline ID> flow <flow ID> policer <policer ID> stats
 */

struct cmd_fa_policer_stats_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	uint32_t flow_id;
	cmdline_fixed_string_t policer_string;
	uint32_t policer_id;
	cmdline_fixed_string_t stats_string;
};

static void
cmd_fa_policer_stats_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fa_policer_stats_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_fa_policer_stats stats;
	int status;

	status = app_pipeline_fa_flow_policer_stats_read(app,
		params->pipeline_id,
		params->flow_id,
		params->policer_id,
		1,
		&stats);
	if (status != 0) {
		printf("Command failed\n");
		return;
	}

	/* Display stats */
	printf("\tPkts G: %" PRIu64
		"\tPkts Y: %" PRIu64
		"\tPkts R: %" PRIu64
		"\tPkts D: %" PRIu64 "\n",
		stats.n_pkts[e_RTE_METER_GREEN],
		stats.n_pkts[e_RTE_METER_YELLOW],
		stats.n_pkts[e_RTE_METER_RED],
		stats.n_pkts_drop);
}

cmdline_parse_token_string_t cmd_fa_policer_stats_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_stats_result,
	p_string, "p");

cmdline_parse_token_num_t cmd_fa_policer_stats_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_policer_stats_result,
	pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fa_policer_stats_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_stats_result,
	flow_string, "flow");

cmdline_parse_token_num_t cmd_fa_policer_stats_flow_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_policer_stats_result,
	flow_id, UINT32);

cmdline_parse_token_string_t cmd_fa_policer_stats_policer_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_stats_result,
	policer_string, "policer");

cmdline_parse_token_num_t cmd_fa_policer_stats_policer_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_policer_stats_result,
	policer_id, UINT32);

cmdline_parse_token_string_t cmd_fa_policer_stats_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_policer_stats_result,
	stats_string, "stats");

cmdline_parse_inst_t cmd_fa_policer_stats = {
	.f = cmd_fa_policer_stats_parsed,
	.data = NULL,
	.help_str = "Flow policer stats read",
	.tokens = {
		(void *) &cmd_fa_policer_stats_p_string,
		(void *) &cmd_fa_policer_stats_pipeline_id,
		(void *) &cmd_fa_policer_stats_flow_string,
		(void *) &cmd_fa_policer_stats_flow_id,
		(void *) &cmd_fa_policer_stats_policer_string,
		(void *) &cmd_fa_policer_stats_policer_id,
		(void *) &cmd_fa_policer_stats_string,
		NULL,
	},
};

/*
 * Flow list
 *
 * p <pipeline ID> flow ls
 */

struct cmd_fa_flow_ls_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t actions_string;
	cmdline_fixed_string_t ls_string;
};

static void
cmd_fa_flow_ls_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fa_flow_ls_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_fa_flow_ls(app, params->pipeline_id);
	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fa_flow_ls_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_flow_ls_result,
	p_string, "p");

cmdline_parse_token_num_t cmd_fa_flow_ls_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_flow_ls_result,
	pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fa_flow_ls_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_flow_ls_result,
	flow_string, "flow");

cmdline_parse_token_string_t cmd_fa_flow_ls_actions_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_flow_ls_result,
	actions_string, "actions");

cmdline_parse_token_string_t cmd_fa_flow_ls_ls_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_flow_ls_result,
	ls_string, "ls");

cmdline_parse_inst_t cmd_fa_flow_ls = {
	.f = cmd_fa_flow_ls_parsed,
	.data = NULL,
	.help_str = "Flow actions list",
	.tokens = {
		(void *) &cmd_fa_flow_ls_p_string,
		(void *) &cmd_fa_flow_ls_pipeline_id,
		(void *) &cmd_fa_flow_ls_flow_string,
		(void *) &cmd_fa_flow_ls_actions_string,
		(void *) &cmd_fa_flow_ls_ls_string,
		NULL,
	},
};

/*
 * Flow DiffServ Code Point (DSCP) translation table list
 *
 * p <pipeline ID> dscp ls
 */

struct cmd_fa_dscp_ls_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t dscp_string;
	cmdline_fixed_string_t ls_string;
};

static void
cmd_fa_dscp_ls_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_fa_dscp_ls_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_fa_dscp_ls(app, params->pipeline_id);
	if (status != 0)
		printf("Command failed\n");
}

cmdline_parse_token_string_t cmd_fa_dscp_ls_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_dscp_ls_result,
	p_string, "p");

cmdline_parse_token_num_t cmd_fa_dscp_ls_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_fa_dscp_ls_result,
	pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_fa_dscp_ls_dscp_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_dscp_ls_result,
	dscp_string, "dscp");

cmdline_parse_token_string_t cmd_fa_dscp_ls_string =
	TOKEN_STRING_INITIALIZER(struct cmd_fa_dscp_ls_result, ls_string,
	"ls");

cmdline_parse_inst_t cmd_fa_dscp_ls = {
	.f = cmd_fa_dscp_ls_parsed,
	.data = NULL,
	.help_str = "Flow DSCP translaton table list",
	.tokens = {
		(void *) &cmd_fa_dscp_ls_p_string,
		(void *) &cmd_fa_dscp_ls_pipeline_id,
		(void *) &cmd_fa_dscp_ls_dscp_string,
		(void *) &cmd_fa_dscp_ls_string,
		NULL,
	},
};

static cmdline_parse_ctx_t pipeline_cmds[] = {
	(cmdline_parse_inst_t *) &cmd_fa_meter_config,
	(cmdline_parse_inst_t *) &cmd_fa_meter_config_bulk,
	(cmdline_parse_inst_t *) &cmd_fa_policer_config,
	(cmdline_parse_inst_t *) &cmd_fa_policer_config_bulk,
	(cmdline_parse_inst_t *) &cmd_fa_output_port_config,
	(cmdline_parse_inst_t *) &cmd_fa_output_port_config_bulk,
	(cmdline_parse_inst_t *) &cmd_fa_dscp_config,
	(cmdline_parse_inst_t *) &cmd_fa_policer_stats,
	(cmdline_parse_inst_t *) &cmd_fa_flow_ls,
	(cmdline_parse_inst_t *) &cmd_fa_dscp_ls,
	NULL,
};

static struct pipeline_fe_ops pipeline_flow_actions_fe_ops = {
	.f_init = app_pipeline_fa_init,
	.f_free = app_pipeline_fa_free,
	.cmds = pipeline_cmds,
};

struct pipeline_type pipeline_flow_actions = {
	.name = "FLOW_ACTIONS",
	.be_ops = &pipeline_flow_actions_be_ops,
	.fe_ops = &pipeline_flow_actions_fe_ops,
};
