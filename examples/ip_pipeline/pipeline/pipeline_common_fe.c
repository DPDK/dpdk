/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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
#include <fcntl.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_ring.h>
#include <rte_malloc.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline.h>

#include "pipeline_common_fe.h"
#include "parser.h"

struct app_link_params *
app_pipeline_track_pktq_out_to_link(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t pktq_out_id)
{
	struct app_pipeline_params *p;

	/* Check input arguments */
	if (app == NULL)
		return NULL;

	APP_PARAM_FIND_BY_ID(app->pipeline_params, "PIPELINE", pipeline_id, p);
	if (p == NULL)
		return NULL;

	for ( ; ; ) {
		struct app_pktq_out_params *pktq_out =
			&p->pktq_out[pktq_out_id];

		switch (pktq_out->type) {
		case APP_PKTQ_OUT_HWQ:
		{
			struct app_pktq_hwq_out_params *hwq_out;

			hwq_out = &app->hwq_out_params[pktq_out->id];

			return app_get_link_for_txq(app, hwq_out);
		}

		case APP_PKTQ_OUT_SWQ:
		{
			struct pipeline_params pp;
			struct pipeline_type *ptype;
			struct app_pktq_swq_params *swq;
			uint32_t pktq_in_id;
			int status;

			swq = &app->swq_params[pktq_out->id];
			p = app_swq_get_reader(app, swq, &pktq_in_id);
			if (p == NULL)
				return NULL;

			ptype = app_pipeline_type_find(app, p->type);
			if ((ptype == NULL) || (ptype->fe_ops->f_track == NULL))
				return NULL;

			app_pipeline_params_get(app, p, &pp);
			status = ptype->fe_ops->f_track(&pp,
				pktq_in_id,
				&pktq_out_id);
			if (status)
				return NULL;

			break;
		}

		case APP_PKTQ_OUT_TM:
		{
			struct pipeline_params pp;
			struct pipeline_type *ptype;
			struct app_pktq_tm_params *tm;
			uint32_t pktq_in_id;
			int status;

			tm = &app->tm_params[pktq_out->id];
			p = app_tm_get_reader(app, tm, &pktq_in_id);
			if (p == NULL)
				return NULL;

			ptype = app_pipeline_type_find(app, p->type);
			if ((ptype == NULL) || (ptype->fe_ops->f_track == NULL))
				return NULL;

			app_pipeline_params_get(app, p, &pp);
			status = ptype->fe_ops->f_track(&pp,
				pktq_in_id,
				&pktq_out_id);
			if (status)
				return NULL;

			break;
		}

		case APP_PKTQ_OUT_KNI:
		{
			struct pipeline_params pp;
			struct pipeline_type *ptype;
			struct app_pktq_kni_params *kni;
			uint32_t pktq_in_id;
			int status;

			kni = &app->kni_params[pktq_out->id];
			p = app_kni_get_reader(app, kni, &pktq_in_id);
			if (p == NULL)
				return NULL;

			ptype = app_pipeline_type_find(app, p->type);
			if ((ptype == NULL) || (ptype->fe_ops->f_track == NULL))
				return NULL;

			app_pipeline_params_get(app, p, &pp);
			status = ptype->fe_ops->f_track(&pp,
				pktq_in_id,
				&pktq_out_id);
			if (status)
				return NULL;

			break;
		}

		case APP_PKTQ_OUT_SINK:
		default:
			return NULL;
		}
	}
}

int
app_pipeline_track_default(struct pipeline_params *p,
	uint32_t port_in,
	uint32_t *port_out)
{
	/* Check input arguments */
	if ((p == NULL) ||
		(port_in >= p->n_ports_in) ||
		(port_out == NULL))
		return -1;

	if (p->n_ports_out == 1) {
		*port_out = 0;
		return 0;
	}

	return -1;
}

int
app_pipeline_ping(struct app_params *app,
	uint32_t pipeline_id)
{
	struct app_pipeline_params *p;
	struct pipeline_msg_req *req;
	struct pipeline_msg_rsp *rsp;
	int status = 0;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	APP_PARAM_FIND_BY_ID(app->pipeline_params, "PIPELINE", pipeline_id, p);
	if (p == NULL)
		return -1;

	/* Message buffer allocation */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	/* Fill in request */
	req->type = PIPELINE_MSG_REQ_PING;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Check response */
	status = rsp->status;

	/* Message buffer free */
	app_msg_free(app, rsp);

	return status;
}

int
app_pipeline_stats_port_in(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id,
	struct rte_pipeline_port_in_stats *stats)
{
	struct app_pipeline_params *p;
	struct pipeline_stats_msg_req *req;
	struct pipeline_stats_port_in_msg_rsp *rsp;
	int status = 0;

	/* Check input arguments */
	if ((app == NULL) ||
		(stats == NULL))
		return -1;

	APP_PARAM_FIND_BY_ID(app->pipeline_params, "PIPELINE", pipeline_id, p);
	if ((p == NULL) ||
		(port_id >= p->n_pktq_in))
		return -1;

	/* Message buffer allocation */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	/* Fill in request */
	req->type = PIPELINE_MSG_REQ_STATS_PORT_IN;
	req->id = port_id;

	/* Send request and wait for response */
	rsp = (struct pipeline_stats_port_in_msg_rsp *)
		app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Check response */
	status = rsp->status;
	if (status == 0)
		memcpy(stats, &rsp->stats, sizeof(rsp->stats));

	/* Message buffer free */
	app_msg_free(app, rsp);

	return status;
}

int
app_pipeline_stats_port_out(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id,
	struct rte_pipeline_port_out_stats *stats)
{
	struct app_pipeline_params *p;
	struct pipeline_stats_msg_req *req;
	struct pipeline_stats_port_out_msg_rsp *rsp;
	int status = 0;

	/* Check input arguments */
	if ((app == NULL) ||
		(pipeline_id >= app->n_pipelines) ||
		(stats == NULL))
		return -1;

	APP_PARAM_FIND_BY_ID(app->pipeline_params, "PIPELINE", pipeline_id, p);
	if ((p == NULL) ||
		(port_id >= p->n_pktq_out))
		return -1;

	/* Message buffer allocation */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	/* Fill in request */
	req->type = PIPELINE_MSG_REQ_STATS_PORT_OUT;
	req->id = port_id;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Check response */
	status = rsp->status;
	if (status == 0)
		memcpy(stats, &rsp->stats, sizeof(rsp->stats));

	/* Message buffer free */
	app_msg_free(app, rsp);

	return status;
}

int
app_pipeline_stats_table(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t table_id,
	struct rte_pipeline_table_stats *stats)
{
	struct app_pipeline_params *p;
	struct pipeline_stats_msg_req *req;
	struct pipeline_stats_table_msg_rsp *rsp;
	int status = 0;

	/* Check input arguments */
	if ((app == NULL) ||
		(stats == NULL))
		return -1;

	APP_PARAM_FIND_BY_ID(app->pipeline_params, "PIPELINE", pipeline_id, p);
	if (p == NULL)
		return -1;

	/* Message buffer allocation */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	/* Fill in request */
	req->type = PIPELINE_MSG_REQ_STATS_TABLE;
	req->id = table_id;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Check response */
	status = rsp->status;
	if (status == 0)
		memcpy(stats, &rsp->stats, sizeof(rsp->stats));

	/* Message buffer free */
	app_msg_free(app, rsp);

	return status;
}

int
app_pipeline_port_in_enable(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id)
{
	struct app_pipeline_params *p;
	struct pipeline_port_in_msg_req *req;
	struct pipeline_msg_rsp *rsp;
	int status = 0;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	APP_PARAM_FIND_BY_ID(app->pipeline_params, "PIPELINE", pipeline_id, p);
	if ((p == NULL) ||
		(port_id >= p->n_pktq_in))
		return -1;

	/* Message buffer allocation */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	/* Fill in request */
	req->type = PIPELINE_MSG_REQ_PORT_IN_ENABLE;
	req->port_id = port_id;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Check response */
	status = rsp->status;

	/* Message buffer free */
	app_msg_free(app, rsp);

	return status;
}

int
app_pipeline_port_in_disable(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id)
{
	struct app_pipeline_params *p;
	struct pipeline_port_in_msg_req *req;
	struct pipeline_msg_rsp *rsp;
	int status = 0;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	APP_PARAM_FIND_BY_ID(app->pipeline_params, "PIPELINE", pipeline_id, p);
	if ((p == NULL) ||
		(port_id >= p->n_pktq_in))
		return -1;

	/* Message buffer allocation */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	/* Fill in request */
	req->type = PIPELINE_MSG_REQ_PORT_IN_DISABLE;
	req->port_id = port_id;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Check response */
	status = rsp->status;

	/* Message buffer free */
	app_msg_free(app, rsp);

	return status;
}

int
app_link_set_op(struct app_params *app,
	uint32_t link_id,
	uint32_t pipeline_id,
	app_link_op op,
	void *arg)
{
	struct app_pipeline_params *pp;
	struct app_link_params *lp;
	struct app_link_data *ld;
	uint32_t ppos, lpos;

	/* Check input arguments */
	if ((app == NULL) ||
		(op == NULL))
		return -1;

	APP_PARAM_FIND_BY_ID(app->link_params, "LINK", link_id, lp);
	if (lp == NULL)
		return -1;
	lpos = lp - app->link_params;
	ld = &app->link_data[lpos];

	APP_PARAM_FIND_BY_ID(app->pipeline_params, "PIPELINE", pipeline_id, pp);
	if (pp == NULL)
		return -1;
	ppos = pp - app->pipeline_params;

	ld->f_link[ppos] = op;
	ld->arg[ppos] = arg;

	return 0;
}

int
app_link_config(struct app_params *app,
	uint32_t link_id,
	uint32_t ip,
	uint32_t depth)
{
	struct app_link_params *p;
	uint32_t i, netmask, host, bcast;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	APP_PARAM_FIND_BY_ID(app->link_params, "LINK", link_id, p);
	if (p == NULL) {
		APP_LOG(app, HIGH, "LINK%" PRIu32 " is not a valid link",
			link_id);
		return -1;
	}

	if (p->state) {
		APP_LOG(app, HIGH, "%s is UP, please bring it DOWN first",
			p->name);
		return -1;
	}

	netmask = (~0U) << (32 - depth);
	host = ip & netmask;
	bcast = host | (~netmask);

	if ((ip == 0) ||
		(ip == UINT32_MAX) ||
		(ip == host) ||
		(ip == bcast)) {
		APP_LOG(app, HIGH, "Illegal IP address");
		return -1;
	}

	for (i = 0; i < app->n_links; i++) {
		struct app_link_params *link = &app->link_params[i];

		if (strcmp(p->name, link->name) == 0)
			continue;

		if (link->ip == ip) {
			APP_LOG(app, HIGH,
				"%s is already assigned this IP address",
				link->name);
			return -1;
		}
	}

	if ((depth == 0) || (depth > 32)) {
		APP_LOG(app, HIGH, "Illegal value for depth parameter "
			"(%" PRIu32 ")",
			depth);
		return -1;
	}

	/* Save link parameters */
	p->ip = ip;
	p->depth = depth;

	return 0;
}

int
app_link_up(struct app_params *app,
	uint32_t link_id)
{
	struct app_link_params *p;
	struct app_link_data *d;
	int i;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	APP_PARAM_FIND_BY_ID(app->link_params, "LINK", link_id, p);
	if (p == NULL) {
		APP_LOG(app, HIGH, "LINK%" PRIu32 " is not a valid link",
			link_id);
		return -1;
	}

	d = &app->link_data[p - app->link_params];

	/* Check link state */
	if (p->state) {
		APP_LOG(app, HIGH, "%s is already UP", p->name);
		return 0;
	}

	/* Check that IP address is valid */
	if (p->ip == 0) {
		APP_LOG(app, HIGH, "%s IP address is not set", p->name);
		return 0;
	}

	app_link_up_internal(app, p);

	/* Callbacks */
	for (i = 0; i < APP_MAX_PIPELINES; i++)
		if (d->f_link[i])
			d->f_link[i](app, link_id, 1, d->arg[i]);

	return 0;
}

int
app_link_down(struct app_params *app,
	uint32_t link_id)
{
	struct app_link_params *p;
	struct app_link_data *d;
	uint32_t i;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	APP_PARAM_FIND_BY_ID(app->link_params, "LINK", link_id, p);
	if (p == NULL) {
		APP_LOG(app, HIGH, "LINK%" PRIu32 " is not a valid link",
			link_id);
		return -1;
	}

	d = &app->link_data[p - app->link_params];

	/* Check link state */
	if (p->state == 0) {
		APP_LOG(app, HIGH, "%s is already DOWN", p->name);
		return 0;
	}

	app_link_down_internal(app, p);

	/* Callbacks */
	for (i = 0; i < APP_MAX_PIPELINES; i++)
		if (d->f_link[i])
			d->f_link[i](app, link_id, 0, d->arg[i]);

	return 0;
}

/*
 * ping
 */

struct cmd_ping_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t ping_string;
};

static void
cmd_ping_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_ping_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_ping(app,	params->pipeline_id);
	if (status != 0)
		printf("Command failed\n");
}

static cmdline_parse_token_string_t cmd_ping_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_ping_result, p_string, "p");

static cmdline_parse_token_num_t cmd_ping_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ping_result, pipeline_id, UINT32);

static cmdline_parse_token_string_t cmd_ping_ping_string =
	TOKEN_STRING_INITIALIZER(struct cmd_ping_result, ping_string, "ping");

static cmdline_parse_inst_t cmd_ping = {
	.f = cmd_ping_parsed,
	.data = NULL,
	.help_str = "Pipeline ping",
	.tokens = {
		(void *) &cmd_ping_p_string,
		(void *) &cmd_ping_pipeline_id,
		(void *) &cmd_ping_ping_string,
		NULL,
	},
};

/*
 * stats port in
 */

struct cmd_stats_port_in_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t stats_string;
	cmdline_fixed_string_t port_string;
	cmdline_fixed_string_t in_string;
	uint32_t port_in_id;

};

static void
cmd_stats_port_in_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_stats_port_in_result *params = parsed_result;
	struct app_params *app = data;
	struct rte_pipeline_port_in_stats stats;
	int status;

	status = app_pipeline_stats_port_in(app,
			params->pipeline_id,
			params->port_in_id,
			&stats);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}

	/* Display stats */
	printf("Pipeline %" PRIu32 " - stats for input port %" PRIu32 ":\n"
		"\tPkts in: %" PRIu64 "\n"
		"\tPkts dropped by AH: %" PRIu64 "\n"
		"\tPkts dropped by other: %" PRIu64 "\n",
		params->pipeline_id,
		params->port_in_id,
		stats.stats.n_pkts_in,
		stats.n_pkts_dropped_by_ah,
		stats.stats.n_pkts_drop);
}

static cmdline_parse_token_string_t cmd_stats_port_in_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_in_result, p_string,
		"p");

static cmdline_parse_token_num_t cmd_stats_port_in_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_stats_port_in_result, pipeline_id,
		UINT32);

static cmdline_parse_token_string_t cmd_stats_port_in_stats_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_in_result, stats_string,
		"stats");

static cmdline_parse_token_string_t cmd_stats_port_in_port_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_in_result, port_string,
		"port");

static cmdline_parse_token_string_t cmd_stats_port_in_in_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_in_result, in_string,
		"in");

	cmdline_parse_token_num_t cmd_stats_port_in_port_in_id =
	TOKEN_NUM_INITIALIZER(struct cmd_stats_port_in_result, port_in_id,
		UINT32);

static cmdline_parse_inst_t cmd_stats_port_in = {
	.f = cmd_stats_port_in_parsed,
	.data = NULL,
	.help_str = "Pipeline input port stats",
	.tokens = {
		(void *) &cmd_stats_port_in_p_string,
		(void *) &cmd_stats_port_in_pipeline_id,
		(void *) &cmd_stats_port_in_stats_string,
		(void *) &cmd_stats_port_in_port_string,
		(void *) &cmd_stats_port_in_in_string,
		(void *) &cmd_stats_port_in_port_in_id,
		NULL,
	},
};

/*
 * stats port out
 */

struct cmd_stats_port_out_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t stats_string;
	cmdline_fixed_string_t port_string;
	cmdline_fixed_string_t out_string;
	uint32_t port_out_id;
};

static void
cmd_stats_port_out_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{

	struct cmd_stats_port_out_result *params = parsed_result;
	struct app_params *app = data;
	struct rte_pipeline_port_out_stats stats;
	int status;

	status = app_pipeline_stats_port_out(app,
			params->pipeline_id,
			params->port_out_id,
			&stats);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}

	/* Display stats */
	printf("Pipeline %" PRIu32 " - stats for output port %" PRIu32 ":\n"
		"\tPkts in: %" PRIu64 "\n"
		"\tPkts dropped by AH: %" PRIu64 "\n"
		"\tPkts dropped by other: %" PRIu64 "\n",
		params->pipeline_id,
		params->port_out_id,
		stats.stats.n_pkts_in,
		stats.n_pkts_dropped_by_ah,
		stats.stats.n_pkts_drop);
}

static cmdline_parse_token_string_t cmd_stats_port_out_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_out_result, p_string,
	"p");

static cmdline_parse_token_num_t cmd_stats_port_out_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_stats_port_out_result, pipeline_id,
		UINT32);

static cmdline_parse_token_string_t cmd_stats_port_out_stats_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_out_result, stats_string,
		"stats");

static cmdline_parse_token_string_t cmd_stats_port_out_port_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_out_result, port_string,
		"port");

static cmdline_parse_token_string_t cmd_stats_port_out_out_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_out_result, out_string,
		"out");

static cmdline_parse_token_num_t cmd_stats_port_out_port_out_id =
	TOKEN_NUM_INITIALIZER(struct cmd_stats_port_out_result, port_out_id,
		UINT32);

static cmdline_parse_inst_t cmd_stats_port_out = {
	.f = cmd_stats_port_out_parsed,
	.data = NULL,
	.help_str = "Pipeline output port stats",
	.tokens = {
		(void *) &cmd_stats_port_out_p_string,
		(void *) &cmd_stats_port_out_pipeline_id,
		(void *) &cmd_stats_port_out_stats_string,
		(void *) &cmd_stats_port_out_port_string,
		(void *) &cmd_stats_port_out_out_string,
		(void *) &cmd_stats_port_out_port_out_id,
		NULL,
	},
};

/*
 * stats table
 */

struct cmd_stats_table_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t stats_string;
	cmdline_fixed_string_t table_string;
	uint32_t table_id;
};

static void
cmd_stats_table_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_stats_table_result *params = parsed_result;
	struct app_params *app = data;
	struct rte_pipeline_table_stats stats;
	int status;

	status = app_pipeline_stats_table(app,
			params->pipeline_id,
			params->table_id,
			&stats);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}

	/* Display stats */
	printf("Pipeline %" PRIu32 " - stats for table %" PRIu32 ":\n"
		"\tPkts in: %" PRIu64 "\n"
		"\tPkts in with lookup miss: %" PRIu64 "\n"
		"\tPkts in with lookup hit dropped by AH: %" PRIu64 "\n"
		"\tPkts in with lookup hit dropped by others: %" PRIu64 "\n"
		"\tPkts in with lookup miss dropped by AH: %" PRIu64 "\n"
		"\tPkts in with lookup miss dropped by others: %" PRIu64 "\n",
		params->pipeline_id,
		params->table_id,
		stats.stats.n_pkts_in,
		stats.stats.n_pkts_lookup_miss,
		stats.n_pkts_dropped_by_lkp_hit_ah,
		stats.n_pkts_dropped_lkp_hit,
		stats.n_pkts_dropped_by_lkp_miss_ah,
		stats.n_pkts_dropped_lkp_miss);
}

static cmdline_parse_token_string_t cmd_stats_table_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_table_result, p_string,
		"p");

static cmdline_parse_token_num_t cmd_stats_table_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_stats_table_result, pipeline_id,
		UINT32);

static cmdline_parse_token_string_t cmd_stats_table_stats_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_table_result, stats_string,
		"stats");

static cmdline_parse_token_string_t cmd_stats_table_table_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_table_result, table_string,
		"table");

static cmdline_parse_token_num_t cmd_stats_table_table_id =
	TOKEN_NUM_INITIALIZER(struct cmd_stats_table_result, table_id, UINT32);

static cmdline_parse_inst_t cmd_stats_table = {
	.f = cmd_stats_table_parsed,
	.data = NULL,
	.help_str = "Pipeline table stats",
	.tokens = {
		(void *) &cmd_stats_table_p_string,
		(void *) &cmd_stats_table_pipeline_id,
		(void *) &cmd_stats_table_stats_string,
		(void *) &cmd_stats_table_table_string,
		(void *) &cmd_stats_table_table_id,
		NULL,
	},
};

/*
 * port in enable
 */

struct cmd_port_in_enable_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t port_string;
	cmdline_fixed_string_t in_string;
	uint32_t port_in_id;
	cmdline_fixed_string_t enable_string;
};

static void
cmd_port_in_enable_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_port_in_enable_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_port_in_enable(app,
			params->pipeline_id,
			params->port_in_id);

	if (status != 0)
		printf("Command failed\n");
}

static cmdline_parse_token_string_t cmd_port_in_enable_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_enable_result, p_string,
		"p");

static cmdline_parse_token_num_t cmd_port_in_enable_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_port_in_enable_result, pipeline_id,
		UINT32);

static cmdline_parse_token_string_t cmd_port_in_enable_port_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_enable_result, port_string,
	"port");

static cmdline_parse_token_string_t cmd_port_in_enable_in_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_enable_result, in_string,
		"in");

static cmdline_parse_token_num_t cmd_port_in_enable_port_in_id =
	TOKEN_NUM_INITIALIZER(struct cmd_port_in_enable_result, port_in_id,
		UINT32);

static cmdline_parse_token_string_t cmd_port_in_enable_enable_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_enable_result,
		enable_string, "enable");

static cmdline_parse_inst_t cmd_port_in_enable = {
	.f = cmd_port_in_enable_parsed,
	.data = NULL,
	.help_str = "Pipeline input port enable",
	.tokens = {
		(void *) &cmd_port_in_enable_p_string,
		(void *) &cmd_port_in_enable_pipeline_id,
		(void *) &cmd_port_in_enable_port_string,
		(void *) &cmd_port_in_enable_in_string,
		(void *) &cmd_port_in_enable_port_in_id,
		(void *) &cmd_port_in_enable_enable_string,
		NULL,
	},
};

/*
 * port in disable
 */

struct cmd_port_in_disable_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t port_string;
	cmdline_fixed_string_t in_string;
	uint32_t port_in_id;
	cmdline_fixed_string_t disable_string;
};

static void
cmd_port_in_disable_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_port_in_disable_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_port_in_disable(app,
			params->pipeline_id,
			params->port_in_id);

	if (status != 0)
		printf("Command failed\n");
}

static cmdline_parse_token_string_t cmd_port_in_disable_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_disable_result, p_string,
		"p");

static cmdline_parse_token_num_t cmd_port_in_disable_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_port_in_disable_result, pipeline_id,
		UINT32);

static cmdline_parse_token_string_t cmd_port_in_disable_port_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_disable_result, port_string,
		"port");

static cmdline_parse_token_string_t cmd_port_in_disable_in_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_disable_result, in_string,
		"in");

static cmdline_parse_token_num_t cmd_port_in_disable_port_in_id =
	TOKEN_NUM_INITIALIZER(struct cmd_port_in_disable_result, port_in_id,
		UINT32);

static cmdline_parse_token_string_t cmd_port_in_disable_disable_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_disable_result,
		disable_string, "disable");

static cmdline_parse_inst_t cmd_port_in_disable = {
	.f = cmd_port_in_disable_parsed,
	.data = NULL,
	.help_str = "Pipeline input port disable",
	.tokens = {
		(void *) &cmd_port_in_disable_p_string,
		(void *) &cmd_port_in_disable_pipeline_id,
		(void *) &cmd_port_in_disable_port_string,
		(void *) &cmd_port_in_disable_in_string,
		(void *) &cmd_port_in_disable_port_in_id,
		(void *) &cmd_port_in_disable_disable_string,
		NULL,
	},
};

/*
 * link config
 */

static void
print_link_info(struct app_link_params *p)
{
	struct rte_eth_stats stats;
	struct ether_addr *mac_addr;
	uint32_t netmask = (~0U) << (32 - p->depth);
	uint32_t host = p->ip & netmask;
	uint32_t bcast = host | (~netmask);

	memset(&stats, 0, sizeof(stats));
	rte_eth_stats_get(p->pmd_id, &stats);

	mac_addr = (struct ether_addr *) &p->mac_addr;

	if (strlen(p->pci_bdf))
		printf("%s(%s): flags=<%s>\n",
			p->name,
			p->pci_bdf,
			(p->state) ? "UP" : "DOWN");
	else
		printf("%s: flags=<%s>\n",
			p->name,
			(p->state) ? "UP" : "DOWN");

	if (p->ip)
		printf("\tinet %" PRIu32 ".%" PRIu32
			".%" PRIu32 ".%" PRIu32
			" netmask %" PRIu32 ".%" PRIu32
			".%" PRIu32 ".%" PRIu32 " "
			"broadcast %" PRIu32 ".%" PRIu32
			".%" PRIu32 ".%" PRIu32 "\n",
			(p->ip >> 24) & 0xFF,
			(p->ip >> 16) & 0xFF,
			(p->ip >> 8) & 0xFF,
			p->ip & 0xFF,
			(netmask >> 24) & 0xFF,
			(netmask >> 16) & 0xFF,
			(netmask >> 8) & 0xFF,
			netmask & 0xFF,
			(bcast >> 24) & 0xFF,
			(bcast >> 16) & 0xFF,
			(bcast >> 8) & 0xFF,
			bcast & 0xFF);

	printf("\tether %02" PRIx32 ":%02" PRIx32 ":%02" PRIx32
		":%02" PRIx32 ":%02" PRIx32 ":%02" PRIx32 "\n",
		mac_addr->addr_bytes[0],
		mac_addr->addr_bytes[1],
		mac_addr->addr_bytes[2],
		mac_addr->addr_bytes[3],
		mac_addr->addr_bytes[4],
		mac_addr->addr_bytes[5]);

	printf("\tRX packets %" PRIu64
		"  bytes %" PRIu64
		"\n",
		stats.ipackets,
		stats.ibytes);

	printf("\tRX errors %" PRIu64
		"  missed %" PRIu64
		"  no-mbuf %" PRIu64
		"\n",
		stats.ierrors,
		stats.imissed,
		stats.rx_nombuf);

	printf("\tTX packets %" PRIu64
		"  bytes %" PRIu64 "\n",
		stats.opackets,
		stats.obytes);

	printf("\tTX errors %" PRIu64
		"\n",
		stats.oerrors);

	printf("\n");
}

/*
 * link
 *
 * link config:
 *    link <linkid> config <ipaddr> <depth>
 *
 * link up:
 *    link <linkid> up
 *
 * link down:
 *    link <linkid> down
 *
 * link ls:
 *    link ls
 */

struct cmd_link_result {
	cmdline_fixed_string_t link_string;
	cmdline_multi_string_t multi_string;
};

static void
cmd_link_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	 void *data)
{
	struct cmd_link_result *params = parsed_result;
	struct app_params *app = data;

	char *tokens[16];
	uint32_t n_tokens = RTE_DIM(tokens);
	int status;

	uint32_t link_id;

	status = parse_tokenize_string(params->multi_string, tokens, &n_tokens);
	if (status != 0) {
		printf(CMD_MSG_TOO_MANY_ARGS, "link");
		return;
	}

	/* link ls */
	if ((n_tokens == 1) && (strcmp(tokens[0], "ls") == 0)) {
		for (link_id = 0; link_id < app->n_links; link_id++) {
			struct app_link_params *p;

			APP_PARAM_FIND_BY_ID(app->link_params, "LINK", link_id, p);
			print_link_info(p);
		}
		return;
	} /* link ls */

	if (n_tokens < 2) {
		printf(CMD_MSG_MISMATCH_ARGS, "link");
		return;
	}

	if (parser_read_uint32(&link_id, tokens[0])) {
		printf(CMD_MSG_INVALID_ARG, "linkid");
		return;
	}

	/* link config */
	if (strcmp(tokens[1], "config") == 0) {
		struct in_addr ipaddr_ipv4;
		uint32_t depth;

		if (n_tokens != 4) {
			printf(CMD_MSG_MISMATCH_ARGS, "link config");
			return;
		}

		if (parse_ipv4_addr(tokens[2], &ipaddr_ipv4)) {
			printf(CMD_MSG_INVALID_ARG, "ipaddr");
			return;
		}

		if (parser_read_uint32(&depth, tokens[3])) {
			printf(CMD_MSG_INVALID_ARG, "depth");
			return;
		}

		status = app_link_config(app,
			link_id,
			rte_be_to_cpu_32(ipaddr_ipv4.s_addr),
			depth);
		if (status)
			printf(CMD_MSG_FAIL, "link config");

		return;
	} /* link config */

	/* link up */
	if (strcmp(tokens[1], "up") == 0) {
		if (n_tokens != 2) {
			printf(CMD_MSG_MISMATCH_ARGS, "link up");
			return;
		}

		status = app_link_up(app, link_id);
		if (status)
			printf(CMD_MSG_FAIL, "link up");

		return;
	} /* link up */

	/* link down */
	if (strcmp(tokens[1], "down") == 0) {
		if (n_tokens != 2) {
			printf(CMD_MSG_MISMATCH_ARGS, "link down");
			return;
		}

		status = app_link_down(app, link_id);
		if (status)
			printf(CMD_MSG_FAIL, "link down");

		return;
	} /* link down */

	printf(CMD_MSG_MISMATCH_ARGS, "link");
}

static cmdline_parse_token_string_t cmd_link_link_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_result, link_string, "link");

static cmdline_parse_token_string_t cmd_link_multi_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_result, multi_string,
	TOKEN_STRING_MULTI);

static cmdline_parse_inst_t cmd_link = {
	.f = cmd_link_parsed,
	.data = NULL,
	.help_str = "link config / up / down / ls",
	.tokens = {
		(void *) &cmd_link_link_string,
		(void *) &cmd_link_multi_string,
		NULL,
	},
};

/*
 * quit
 */

struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void
cmd_quit_parsed(
	__rte_unused void *parsed_result,
	struct cmdline *cl,
	__rte_unused void *data)
{
	cmdline_quit(cl);
}

static cmdline_parse_token_string_t cmd_quit_quit =
	TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

static cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,
	.data = NULL,
	.help_str = "Quit",
	.tokens = {
		(void *) &cmd_quit_quit,
		NULL,
	},
};

/*
 * run
 *
 *    run <file>
 *    run <file> [<count> [<interval>]]
	 <count> default is 1
 *       <interval> is measured in milliseconds, default is 1 second
 */

static void
app_run_file(
	cmdline_parse_ctx_t *ctx,
	const char *file_name)
{
	struct cmdline *file_cl;
	int fd;

	fd = open(file_name, O_RDONLY);
	if (fd < 0) {
		printf("Cannot open file \"%s\"\n", file_name);
		return;
	}

	file_cl = cmdline_new(ctx, "", fd, 1);
	cmdline_interact(file_cl);
	close(fd);
}

struct cmd_run_result {
	cmdline_fixed_string_t run_string;
	cmdline_multi_string_t multi_string;
};

static void
cmd_run_parsed(
	void *parsed_result,
	struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_run_result *params = parsed_result;

	char *tokens[16];
	uint32_t n_tokens = RTE_DIM(tokens);
	int status;

	char *file_name;
	uint32_t count, interval, i;

	status = parse_tokenize_string(params->multi_string, tokens, &n_tokens);
	if (status) {
		printf(CMD_MSG_TOO_MANY_ARGS, "run");
		return;
	}

	switch (n_tokens) {
	case 0:
		printf(CMD_MSG_NOT_ENOUGH_ARGS, "run");
		return;

	case 1:
		file_name = tokens[0];
		count = 1;
		interval = 1000;
		break;

	case 2:
		file_name = tokens[0];

		if (parser_read_uint32(&count, tokens[1]) ||
			(count == 0)) {
			printf(CMD_MSG_INVALID_ARG, "count");
			return;
		}

		interval = 1000;
		break;

	case 3:
		file_name = tokens[0];

		if (parser_read_uint32(&count, tokens[1]) ||
			(count == 0)) {
			printf(CMD_MSG_INVALID_ARG, "count");
			return;
		}

		if (parser_read_uint32(&interval, tokens[2]) ||
			(interval == 0)) {
			printf(CMD_MSG_INVALID_ARG, "interval");
			return;
		}
		break;

	default:
		printf(CMD_MSG_MISMATCH_ARGS, "run");
		return;
	}

	for (i = 0; i < count; i++) {
		app_run_file(cl->ctx, file_name);
		if (interval)
			usleep(interval * 1000);
	}
}

static cmdline_parse_token_string_t cmd_run_run_string =
	TOKEN_STRING_INITIALIZER(struct cmd_run_result, run_string, "run");

static cmdline_parse_token_string_t cmd_run_multi_string =
	TOKEN_STRING_INITIALIZER(struct cmd_run_result, multi_string,
	TOKEN_STRING_MULTI);


static cmdline_parse_inst_t cmd_run = {
	.f = cmd_run_parsed,
	.data = NULL,
	.help_str = "Run CLI script file",
	.tokens = {
		(void *) &cmd_run_run_string,
		(void *) &cmd_run_multi_string,
		NULL,
	},
};

static cmdline_parse_ctx_t pipeline_common_cmds[] = {
	(cmdline_parse_inst_t *) &cmd_quit,
	(cmdline_parse_inst_t *) &cmd_run,
	(cmdline_parse_inst_t *) &cmd_link,
	(cmdline_parse_inst_t *) &cmd_ping,
	(cmdline_parse_inst_t *) &cmd_stats_port_in,
	(cmdline_parse_inst_t *) &cmd_stats_port_out,
	(cmdline_parse_inst_t *) &cmd_stats_table,
	(cmdline_parse_inst_t *) &cmd_port_in_enable,
	(cmdline_parse_inst_t *) &cmd_port_in_disable,
	NULL,
};

int
app_pipeline_common_cmd_push(struct app_params *app)
{
	uint32_t n_cmds, i;

	/* Check for available slots in the application commands array */
	n_cmds = RTE_DIM(pipeline_common_cmds) - 1;
	if (n_cmds > APP_MAX_CMDS - app->n_cmds)
		return -ENOMEM;

	/* Push pipeline commands into the application */
	memcpy(&app->cmds[app->n_cmds],
		pipeline_common_cmds,
		n_cmds * sizeof(cmdline_parse_ctx_t));

	for (i = 0; i < n_cmds; i++)
		app->cmds[app->n_cmds + i]->data = app;

	app->n_cmds += n_cmds;
	app->cmds[app->n_cmds] = NULL;

	return 0;
}
