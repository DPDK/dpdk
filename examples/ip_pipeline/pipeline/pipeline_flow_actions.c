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
#include <unistd.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_malloc.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include "app.h"
#include "pipeline_common_fe.h"
#include "pipeline_flow_actions.h"
#include "hash_func.h"
#include "parser.h"

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
	int status;

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
	status = (rsp->n_flows == n_flows) ? 0 : -1;

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

	return status;
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

int
app_pipeline_fa_load_file(char *filename,
	uint32_t *flow_ids,
	struct pipeline_fa_flow_params *p,
	uint32_t *n_flows,
	uint32_t *line)
{
	FILE *f = NULL;
	char file_buf[1024];
	uint32_t i, l;

	/* Check input arguments */
	if ((filename == NULL) ||
		(flow_ids == NULL) ||
		(p == NULL) ||
		(n_flows == NULL) ||
		(*n_flows == 0) ||
		(line == NULL)) {
		if (line)
			*line = 0;
		return -1;
		}

	/* Open input file */
	f = fopen(filename, "r");
	if (f == NULL) {
		*line = 0;
		return -1;
	}

	/* Read file */
	for (i = 0, l = 1; i < *n_flows; l++) {
		char *tokens[64];
		uint32_t n_tokens = RTE_DIM(tokens);

		int status;

		if (fgets(file_buf, sizeof(file_buf), f) == NULL)
			break;

		status = parse_tokenize_string(file_buf, tokens, &n_tokens);
		if (status)
			goto error1;

		if ((n_tokens == 0) || (tokens[0][0] == '#'))
			continue;


		if ((n_tokens != 64) ||
			/* flow */
			strcmp(tokens[0], "flow") ||
			parser_read_uint32(&flow_ids[i], tokens[1]) ||

			/* meter & policer 0 */
			strcmp(tokens[2], "meter") ||
			strcmp(tokens[3], "0") ||
			strcmp(tokens[4], "trtcm") ||
			parser_read_uint64(&p[i].m[0].cir, tokens[5]) ||
			parser_read_uint64(&p[i].m[0].pir, tokens[6]) ||
			parser_read_uint64(&p[i].m[0].cbs, tokens[7]) ||
			parser_read_uint64(&p[i].m[0].pbs, tokens[8]) ||
			strcmp(tokens[9], "policer") ||
			strcmp(tokens[10], "0") ||
			strcmp(tokens[11], "g") ||
			string_to_policer_action(tokens[12],
				&p[i].p[0].action[e_RTE_METER_GREEN]) ||
			strcmp(tokens[13], "y") ||
			string_to_policer_action(tokens[14],
				&p[i].p[0].action[e_RTE_METER_YELLOW]) ||
			strcmp(tokens[15], "r") ||
			string_to_policer_action(tokens[16],
				&p[i].p[0].action[e_RTE_METER_RED]) ||

			/* meter & policer 1 */
			strcmp(tokens[17], "meter") ||
			strcmp(tokens[18], "1") ||
			strcmp(tokens[19], "trtcm") ||
			parser_read_uint64(&p[i].m[1].cir, tokens[20]) ||
			parser_read_uint64(&p[i].m[1].pir, tokens[21]) ||
			parser_read_uint64(&p[i].m[1].cbs, tokens[22]) ||
			parser_read_uint64(&p[i].m[1].pbs, tokens[23]) ||
			strcmp(tokens[24], "policer") ||
			strcmp(tokens[25], "1") ||
			strcmp(tokens[26], "g") ||
			string_to_policer_action(tokens[27],
				&p[i].p[1].action[e_RTE_METER_GREEN]) ||
			strcmp(tokens[28], "y") ||
			string_to_policer_action(tokens[29],
				&p[i].p[1].action[e_RTE_METER_YELLOW]) ||
			strcmp(tokens[30], "r") ||
			string_to_policer_action(tokens[31],
				&p[i].p[1].action[e_RTE_METER_RED]) ||

			/* meter & policer 2 */
			strcmp(tokens[32], "meter") ||
			strcmp(tokens[33], "2") ||
			strcmp(tokens[34], "trtcm") ||
			parser_read_uint64(&p[i].m[2].cir, tokens[35]) ||
			parser_read_uint64(&p[i].m[2].pir, tokens[36]) ||
			parser_read_uint64(&p[i].m[2].cbs, tokens[37]) ||
			parser_read_uint64(&p[i].m[2].pbs, tokens[38]) ||
			strcmp(tokens[39], "policer") ||
			strcmp(tokens[40], "2") ||
			strcmp(tokens[41], "g") ||
			string_to_policer_action(tokens[42],
				&p[i].p[2].action[e_RTE_METER_GREEN]) ||
			strcmp(tokens[43], "y") ||
			string_to_policer_action(tokens[44],
				&p[i].p[2].action[e_RTE_METER_YELLOW]) ||
			strcmp(tokens[45], "r") ||
			string_to_policer_action(tokens[46],
				&p[i].p[2].action[e_RTE_METER_RED]) ||

			/* meter & policer 3 */
			strcmp(tokens[47], "meter") ||
			strcmp(tokens[48], "3") ||
			strcmp(tokens[49], "trtcm") ||
			parser_read_uint64(&p[i].m[3].cir, tokens[50]) ||
			parser_read_uint64(&p[i].m[3].pir, tokens[51]) ||
			parser_read_uint64(&p[i].m[3].cbs, tokens[52]) ||
			parser_read_uint64(&p[i].m[3].pbs, tokens[53]) ||
			strcmp(tokens[54], "policer") ||
			strcmp(tokens[55], "3") ||
			strcmp(tokens[56], "g") ||
			string_to_policer_action(tokens[57],
				&p[i].p[3].action[e_RTE_METER_GREEN]) ||
			strcmp(tokens[58], "y") ||
			string_to_policer_action(tokens[59],
				&p[i].p[3].action[e_RTE_METER_YELLOW]) ||
			strcmp(tokens[60], "r") ||
			string_to_policer_action(tokens[61],
				&p[i].p[3].action[e_RTE_METER_RED]) ||

			/* port */
			strcmp(tokens[62], "port") ||
			parser_read_uint32(&p[i].port_id, tokens[63]))
			goto error1;

		i++;
	}

	/* Close file */
	*n_flows = i;
	fclose(f);
	return 0;

error1:
	*line = l;
	fclose(f);
	return -1;
}

/*
 * action
 *
 * flow meter, policer and output port configuration:
 *    p <pipelineid> action flow <flowid> meter <meterid> trtcm <cir> <pir> <cbs> <pbs>
 *
 *    p <pipelineid> action flow <flowid> policer <policerid> g <gaction> y <yaction> r <raction>
 *  <action> is one of the following:
 *      G = recolor to green
 *      Y = recolor as yellow
 *      R = recolor as red
 *      D = drop
 *
 *    p <pipelineid> action flow <flowid> port <port ID>
 *
 *    p <pipelineid> action flow bulk <file>
 *
 * flow policer stats read:
 *    p <pipelineid> action flow <flowid> stats
 *
 * flow ls:
 *    p <pipelineid> action flow ls
 *
 * dscp table configuration:
 *    p <pipelineid> action dscp <dscpid> class <class ID> color <color>
 *
 * dscp table ls:
 *    p <pipelineid> action dscp ls
**/

struct cmd_action_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t action_string;
	cmdline_multi_string_t multi_string;
};

static void
cmd_action_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_action_result *params = parsed_result;
	struct app_params *app = data;

	char *tokens[16];
	uint32_t n_tokens = RTE_DIM(tokens);
	int status;

	status = parse_tokenize_string(params->multi_string, tokens, &n_tokens);
	if (status != 0) {
		printf(CMD_MSG_TOO_MANY_ARGS, "action");
		return;
	}

	/* action flow meter */
	if ((n_tokens >= 3) &&
		(strcmp(tokens[0], "flow") == 0) &&
		strcmp(tokens[1], "bulk") &&
		strcmp(tokens[1], "ls") &&
		(strcmp(tokens[2], "meter") == 0)) {
		struct pipeline_fa_flow_params flow_params;
		uint32_t flow_id, meter_id;

		if (n_tokens != 9) {
			printf(CMD_MSG_MISMATCH_ARGS, "action flow meter");
			return;
		}

		memset(&flow_params, 0, sizeof(flow_params));

		if (parser_read_uint32(&flow_id, tokens[1])) {
			printf(CMD_MSG_INVALID_ARG, "flowid");
			return;
		}

		if (parser_read_uint32(&meter_id, tokens[3]) ||
			(meter_id >= PIPELINE_FA_N_TC_MAX)) {
			printf(CMD_MSG_INVALID_ARG, "meterid");
			return;
		}

		if (strcmp(tokens[4], "trtcm")) {
			printf(CMD_MSG_ARG_NOT_FOUND, "trtcm");
			return;
		}

		if (parser_read_uint64(&flow_params.m[meter_id].cir, tokens[5])) {
			printf(CMD_MSG_INVALID_ARG, "cir");
			return;
		}

		if (parser_read_uint64(&flow_params.m[meter_id].pir, tokens[6])) {
			printf(CMD_MSG_INVALID_ARG, "pir");
			return;
		}

		if (parser_read_uint64(&flow_params.m[meter_id].cbs, tokens[7])) {
			printf(CMD_MSG_INVALID_ARG, "cbs");
			return;
		}

		if (parser_read_uint64(&flow_params.m[meter_id].pbs, tokens[8])) {
			printf(CMD_MSG_INVALID_ARG, "pbs");
			return;
		}

		status = app_pipeline_fa_flow_config(app,
			params->pipeline_id,
			flow_id,
			1 << meter_id,
			0,
			0,
			&flow_params);
		if (status)
			printf(CMD_MSG_FAIL, "action flow meter");

		return;
	} /* action flow meter */

	/* action flow policer */
	if ((n_tokens >= 3) &&
		(strcmp(tokens[0], "flow") == 0) &&
		strcmp(tokens[1], "bulk") &&
		strcmp(tokens[1], "ls") &&
		(strcmp(tokens[2], "policer") == 0)) {
		struct pipeline_fa_flow_params flow_params;
		uint32_t flow_id, policer_id;

		if (n_tokens != 10) {
			printf(CMD_MSG_MISMATCH_ARGS, "action flow policer");
			return;
		}

		memset(&flow_params, 0, sizeof(flow_params));

		if (parser_read_uint32(&flow_id, tokens[1])) {
			printf(CMD_MSG_INVALID_ARG, "flowid");
			return;
		}

		if (parser_read_uint32(&policer_id, tokens[3]) ||
			(policer_id >= PIPELINE_FA_N_TC_MAX)) {
			printf(CMD_MSG_INVALID_ARG, "policerid");
			return;
		}

		if (strcmp(tokens[4], "g")) {
			printf(CMD_MSG_ARG_NOT_FOUND, "g");
			return;
		}

		if (string_to_policer_action(tokens[5],
			&flow_params.p[policer_id].action[e_RTE_METER_GREEN])) {
			printf(CMD_MSG_INVALID_ARG, "gaction");
			return;
		}

		if (strcmp(tokens[6], "y")) {
			printf(CMD_MSG_ARG_NOT_FOUND, "y");
			return;
		}

		if (string_to_policer_action(tokens[7],
			&flow_params.p[policer_id].action[e_RTE_METER_YELLOW])) {
			printf(CMD_MSG_INVALID_ARG, "yaction");
			return;
		}

		if (strcmp(tokens[8], "r")) {
			printf(CMD_MSG_ARG_NOT_FOUND, "r");
			return;
		}

		if (string_to_policer_action(tokens[9],
			&flow_params.p[policer_id].action[e_RTE_METER_RED])) {
			printf(CMD_MSG_INVALID_ARG, "raction");
			return;
		}

		status = app_pipeline_fa_flow_config(app,
			params->pipeline_id,
			flow_id,
			0,
			1 << policer_id,
			0,
			&flow_params);
		if (status != 0)
			printf(CMD_MSG_FAIL, "action flow policer");

		return;
	} /* action flow policer */

	/* action flow port */
	if ((n_tokens >= 3) &&
		(strcmp(tokens[0], "flow") == 0) &&
		strcmp(tokens[1], "bulk") &&
		strcmp(tokens[1], "ls") &&
		(strcmp(tokens[2], "port") == 0)) {
		struct pipeline_fa_flow_params flow_params;
		uint32_t flow_id, port_id;

		if (n_tokens != 4) {
			printf(CMD_MSG_MISMATCH_ARGS, "action flow port");
			return;
		}

		memset(&flow_params, 0, sizeof(flow_params));

		if (parser_read_uint32(&flow_id, tokens[1])) {
			printf(CMD_MSG_INVALID_ARG, "flowid");
			return;
		}

		if (parser_read_uint32(&port_id, tokens[3])) {
			printf(CMD_MSG_INVALID_ARG, "portid");
			return;
		}

		flow_params.port_id = port_id;

		status = app_pipeline_fa_flow_config(app,
			params->pipeline_id,
			flow_id,
			0,
			0,
			1,
			&flow_params);
		if (status)
			printf(CMD_MSG_FAIL, "action flow port");

		return;
	} /* action flow port */

	/* action flow stats */
	if ((n_tokens >= 3) &&
		(strcmp(tokens[0], "flow") == 0) &&
		strcmp(tokens[1], "bulk") &&
		strcmp(tokens[1], "ls") &&
		(strcmp(tokens[2], "stats") == 0)) {
		struct pipeline_fa_policer_stats stats;
		uint32_t flow_id, policer_id;

		if (n_tokens != 3) {
			printf(CMD_MSG_MISMATCH_ARGS, "action flow stats");
			return;
		}

		if (parser_read_uint32(&flow_id, tokens[1])) {
			printf(CMD_MSG_INVALID_ARG, "flowid");
			return;
		}

		for (policer_id = 0;
			policer_id < PIPELINE_FA_N_TC_MAX;
			policer_id++) {
			status = app_pipeline_fa_flow_policer_stats_read(app,
				params->pipeline_id,
				flow_id,
				policer_id,
				1,
				&stats);
			if (status != 0) {
				printf(CMD_MSG_FAIL, "action flow stats");
				return;
			}

			/* Display stats */
			printf("\tPolicer: %" PRIu32
				"\tPkts G: %" PRIu64
				"\tPkts Y: %" PRIu64
				"\tPkts R: %" PRIu64
				"\tPkts D: %" PRIu64 "\n",
				policer_id,
				stats.n_pkts[e_RTE_METER_GREEN],
				stats.n_pkts[e_RTE_METER_YELLOW],
				stats.n_pkts[e_RTE_METER_RED],
				stats.n_pkts_drop);
		}

		return;
	} /* action flow stats */

	/* action flow bulk */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "flow") == 0) &&
		(strcmp(tokens[1], "bulk") == 0)) {
		struct pipeline_fa_flow_params *flow_params;
		uint32_t *flow_ids, n_flows, line;
		char *filename;

		if (n_tokens != 3) {
			printf(CMD_MSG_MISMATCH_ARGS, "action flow bulk");
			return;
		}

		filename = tokens[2];

		n_flows = APP_PIPELINE_FA_MAX_RECORDS_IN_FILE;
		flow_ids = malloc(n_flows * sizeof(uint32_t));
		if (flow_ids == NULL) {
			printf(CMD_MSG_OUT_OF_MEMORY);
			return;
		}

		flow_params = malloc(n_flows * sizeof(struct pipeline_fa_flow_params));
		if (flow_params == NULL) {
			printf(CMD_MSG_OUT_OF_MEMORY);
			free(flow_ids);
			return;
		}

		status = app_pipeline_fa_load_file(filename,
			flow_ids,
			flow_params,
			&n_flows,
			&line);
		if (status) {
			printf(CMD_MSG_FILE_ERR, filename, line);
			free(flow_params);
			free(flow_ids);
			return;
		}

		status = app_pipeline_fa_flow_config_bulk(app,
			params->pipeline_id,
			flow_ids,
			n_flows,
			0xF,
			0xF,
			1,
			flow_params);
		if (status)
			printf(CMD_MSG_FAIL, "action flow bulk");

		free(flow_params);
		free(flow_ids);
		return;
	} /* action flow bulk */

	/* action flow ls */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "flow") == 0) &&
		(strcmp(tokens[1], "ls") == 0)) {
		if (n_tokens != 2) {
			printf(CMD_MSG_MISMATCH_ARGS, "action flow ls");
			return;
		}

		status = app_pipeline_fa_flow_ls(app,
			params->pipeline_id);
		if (status)
			printf(CMD_MSG_FAIL, "action flow ls");

		return;
	} /* action flow ls */

	/* action dscp */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "dscp") == 0) &&
		strcmp(tokens[1], "ls")) {
		uint32_t dscp_id, tc_id;
		enum rte_meter_color color;

		if (n_tokens != 6) {
			printf(CMD_MSG_MISMATCH_ARGS, "action dscp");
			return;
		}

		if (parser_read_uint32(&dscp_id, tokens[1])) {
			printf(CMD_MSG_INVALID_ARG, "dscpid");
			return;
		}

		if (strcmp(tokens[2], "class")) {
			printf(CMD_MSG_ARG_NOT_FOUND, "class");
			return;
		}

		if (parser_read_uint32(&tc_id, tokens[3])) {
			printf(CMD_MSG_INVALID_ARG, "classid");
			return;
		}

		if (strcmp(tokens[4], "color")) {
			printf(CMD_MSG_ARG_NOT_FOUND, "color");
			return;
		}

		if (string_to_color(tokens[5], &color)) {
			printf(CMD_MSG_INVALID_ARG, "colorid");
			return;
		}

		status = app_pipeline_fa_dscp_config(app,
			params->pipeline_id,
			dscp_id,
			tc_id,
			color);
		if (status != 0)
			printf(CMD_MSG_FAIL, "action dscp");

		return;
	} /* action dscp */

	/* action dscp ls */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "dscp") == 0) &&
		(strcmp(tokens[1], "ls") == 0)) {
		if (n_tokens != 2) {
			printf(CMD_MSG_MISMATCH_ARGS, "action dscp ls");
			return;
		}

		status = app_pipeline_fa_dscp_ls(app,
			params->pipeline_id);
		if (status)
			printf(CMD_MSG_FAIL, "action dscp ls");

		return;
	} /* action dscp ls */

	printf(CMD_MSG_FAIL, "action");
}

static cmdline_parse_token_string_t cmd_action_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_action_result, p_string, "p");

static cmdline_parse_token_num_t cmd_action_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_action_result, pipeline_id, UINT32);

static cmdline_parse_token_string_t cmd_action_action_string =
	TOKEN_STRING_INITIALIZER(struct cmd_action_result, action_string, "action");

static cmdline_parse_token_string_t cmd_action_multi_string =
	TOKEN_STRING_INITIALIZER(struct cmd_action_result, multi_string,
	TOKEN_STRING_MULTI);

cmdline_parse_inst_t cmd_action = {
	.f = cmd_action_parsed,
	.data = NULL,
	.help_str = "flow actions (meter, policer, policer stats, dscp table)",
	.tokens = {
		(void *) &cmd_action_p_string,
		(void *) &cmd_action_pipeline_id,
		(void *) &cmd_action_action_string,
		(void *) &cmd_action_multi_string,
		NULL,
	},
};

static cmdline_parse_ctx_t pipeline_cmds[] = {
	(cmdline_parse_inst_t *) &cmd_action,
	NULL,
};

static struct pipeline_fe_ops pipeline_flow_actions_fe_ops = {
	.f_init = app_pipeline_fa_init,
	.f_post_init = NULL,
	.f_free = app_pipeline_fa_free,
	.f_track = app_pipeline_track_default,
	.cmds = pipeline_cmds,
};

struct pipeline_type pipeline_flow_actions = {
	.name = "FLOW_ACTIONS",
	.be_ops = &pipeline_flow_actions_be_ops,
	.fe_ops = &pipeline_flow_actions_fe_ops,
};
