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
#include <fcntl.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_ring.h>
#include <rte_malloc.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_etheraddr.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "pipeline_common_fe.h"

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

	netmask = (~0) << (32 - depth);
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
				p->name);
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

	/* Check input arguments */
	if (app == NULL)
		return -1;

	APP_PARAM_FIND_BY_ID(app->link_params, "LINK", link_id, p);
	if (p == NULL) {
		APP_LOG(app, HIGH, "LINK%" PRIu32 " is not a valid link",
			link_id);
		return -1;
	}

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

	return 0;
}

int
app_link_down(struct app_params *app,
	uint32_t link_id)
{
	struct app_link_params *p;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	APP_PARAM_FIND_BY_ID(app->link_params, "LINK", link_id, p);
	if (p == NULL) {
		APP_LOG(app, HIGH, "LINK%" PRIu32 " is not a valid link",
			link_id);
		return -1;
	}

	/* Check link state */
	if (p->state == 0) {
		APP_LOG(app, HIGH, "%s is already DOWN", p->name);
		return 0;
	}

	app_link_down_internal(app, p);

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

cmdline_parse_token_string_t cmd_ping_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_ping_result, p_string, "p");

cmdline_parse_token_num_t cmd_ping_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_ping_result, pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_ping_ping_string =
	TOKEN_STRING_INITIALIZER(struct cmd_ping_result, ping_string, "ping");

cmdline_parse_inst_t cmd_ping = {
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

cmdline_parse_token_string_t cmd_stats_port_in_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_in_result, p_string,
		"p");

cmdline_parse_token_num_t cmd_stats_port_in_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_stats_port_in_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_stats_port_in_stats_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_in_result, stats_string,
		"stats");

cmdline_parse_token_string_t cmd_stats_port_in_port_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_in_result, port_string,
		"port");

cmdline_parse_token_string_t cmd_stats_port_in_in_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_in_result, in_string,
		"in");

	cmdline_parse_token_num_t cmd_stats_port_in_port_in_id =
	TOKEN_NUM_INITIALIZER(struct cmd_stats_port_in_result, port_in_id,
		UINT32);

cmdline_parse_inst_t cmd_stats_port_in = {
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

cmdline_parse_token_string_t cmd_stats_port_out_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_out_result, p_string,
	"p");

cmdline_parse_token_num_t cmd_stats_port_out_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_stats_port_out_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_stats_port_out_stats_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_out_result, stats_string,
		"stats");

cmdline_parse_token_string_t cmd_stats_port_out_port_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_out_result, port_string,
		"port");

cmdline_parse_token_string_t cmd_stats_port_out_out_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_port_out_result, out_string,
		"out");

cmdline_parse_token_num_t cmd_stats_port_out_port_out_id =
	TOKEN_NUM_INITIALIZER(struct cmd_stats_port_out_result, port_out_id,
		UINT32);

cmdline_parse_inst_t cmd_stats_port_out = {
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

cmdline_parse_token_string_t cmd_stats_table_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_table_result, p_string,
		"p");

cmdline_parse_token_num_t cmd_stats_table_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_stats_table_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_stats_table_stats_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_table_result, stats_string,
		"stats");

cmdline_parse_token_string_t cmd_stats_table_table_string =
	TOKEN_STRING_INITIALIZER(struct cmd_stats_table_result, table_string,
		"table");

cmdline_parse_token_num_t cmd_stats_table_table_id =
	TOKEN_NUM_INITIALIZER(struct cmd_stats_table_result, table_id, UINT32);

cmdline_parse_inst_t cmd_stats_table = {
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

cmdline_parse_token_string_t cmd_port_in_enable_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_enable_result, p_string,
		"p");

cmdline_parse_token_num_t cmd_port_in_enable_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_port_in_enable_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_port_in_enable_port_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_enable_result, port_string,
	"port");

cmdline_parse_token_string_t cmd_port_in_enable_in_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_enable_result, in_string,
		"in");

cmdline_parse_token_num_t cmd_port_in_enable_port_in_id =
	TOKEN_NUM_INITIALIZER(struct cmd_port_in_enable_result, port_in_id,
		UINT32);

cmdline_parse_token_string_t cmd_port_in_enable_enable_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_enable_result,
		enable_string, "enable");

cmdline_parse_inst_t cmd_port_in_enable = {
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

cmdline_parse_token_string_t cmd_port_in_disable_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_disable_result, p_string,
		"p");

cmdline_parse_token_num_t cmd_port_in_disable_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_port_in_disable_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_port_in_disable_port_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_disable_result, port_string,
		"port");

cmdline_parse_token_string_t cmd_port_in_disable_in_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_disable_result, in_string,
		"in");

cmdline_parse_token_num_t cmd_port_in_disable_port_in_id =
	TOKEN_NUM_INITIALIZER(struct cmd_port_in_disable_result, port_in_id,
		UINT32);

cmdline_parse_token_string_t cmd_port_in_disable_disable_string =
	TOKEN_STRING_INITIALIZER(struct cmd_port_in_disable_result,
		disable_string, "disable");

cmdline_parse_inst_t cmd_port_in_disable = {
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
	uint32_t netmask = (~0) << (32 - p->depth);
	uint32_t host = p->ip & netmask;
	uint32_t bcast = host | (~netmask);

	memset(&stats, 0, sizeof(stats));
	rte_eth_stats_get(p->pmd_id, &stats);

	mac_addr = (struct ether_addr *) &p->mac_addr;

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

	printf("\tRX mcast %" PRIu64
		"  fdirmatch %" PRIu64
		"  fdirmiss %" PRIu64
		"  lb-packets %" PRIu64
		"  lb-bytes %" PRIu64
		"  xon %" PRIu64
		"  xoff %" PRIu64 "\n",
		stats.imcasts,
		stats.fdirmatch,
		stats.fdirmiss,
		stats.ilbpackets,
		stats.ilbbytes,
		stats.rx_pause_xon,
		stats.rx_pause_xoff);

	printf("\tRX errors %" PRIu64
		"  missed %" PRIu64
		"  badcrc %" PRIu64
		"  badlen %" PRIu64
		"  no-mbuf %" PRIu64
		"\n",
		stats.ierrors,
		stats.imissed,
		stats.ibadcrc,
		stats.ibadlen,
		stats.rx_nombuf);

	printf("\tTX packets %" PRIu64
		"  bytes %" PRIu64 "\n",
		stats.opackets,
		stats.obytes);

	printf("\tTX lb-packets %" PRIu64
		"  lb-bytes %" PRIu64
		"  xon %" PRIu64
		"  xoff %" PRIu64
		"\n",
		stats.olbpackets,
		stats.olbbytes,
		stats.tx_pause_xon,
		stats.tx_pause_xoff);

	printf("\tTX errors %" PRIu64
		"\n",
		stats.oerrors);

	printf("\n");
}

struct cmd_link_config_result {
	cmdline_fixed_string_t link_string;
	uint32_t link_id;
	cmdline_fixed_string_t config_string;
	cmdline_ipaddr_t ip;
	uint32_t depth;
};

static void
cmd_link_config_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	 void *data)
{
	struct cmd_link_config_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	uint32_t link_id = params->link_id;
	uint32_t ip  = rte_bswap32((uint32_t) params->ip.addr.ipv4.s_addr);
	uint32_t depth = params->depth;

	status = app_link_config(app, link_id, ip, depth);
	if (status)
		printf("Command failed\n");
	else {
		struct app_link_params *p;

		APP_PARAM_FIND_BY_ID(app->link_params, "LINK", link_id, p);
		print_link_info(p);
	}
}

cmdline_parse_token_string_t cmd_link_config_link_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_config_result, link_string,
		"link");

cmdline_parse_token_num_t cmd_link_config_link_id =
	TOKEN_NUM_INITIALIZER(struct cmd_link_config_result, link_id, UINT32);

cmdline_parse_token_string_t cmd_link_config_config_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_config_result, config_string,
		"config");

cmdline_parse_token_ipaddr_t cmd_link_config_ip =
	TOKEN_IPV4_INITIALIZER(struct cmd_link_config_result, ip);

cmdline_parse_token_num_t cmd_link_config_depth =
	TOKEN_NUM_INITIALIZER(struct cmd_link_config_result, depth, UINT32);

cmdline_parse_inst_t cmd_link_config = {
	.f = cmd_link_config_parsed,
	.data = NULL,
	.help_str = "Link configuration",
	.tokens = {
		(void *)&cmd_link_config_link_string,
		(void *)&cmd_link_config_link_id,
		(void *)&cmd_link_config_config_string,
		(void *)&cmd_link_config_ip,
		(void *)&cmd_link_config_depth,
		NULL,
	},
};

/*
 * link up
 */

struct cmd_link_up_result {
	cmdline_fixed_string_t link_string;
	uint32_t link_id;
	cmdline_fixed_string_t up_string;
};

static void
cmd_link_up_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	void *data)
{
	struct cmd_link_up_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_link_up(app, params->link_id);
	if (status != 0)
		printf("Command failed\n");
	else {
		struct app_link_params *p;

		APP_PARAM_FIND_BY_ID(app->link_params, "LINK", params->link_id,
			p);
		print_link_info(p);
	}
}

cmdline_parse_token_string_t cmd_link_up_link_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_up_result, link_string,
		"link");

cmdline_parse_token_num_t cmd_link_up_link_id =
	TOKEN_NUM_INITIALIZER(struct cmd_link_up_result, link_id, UINT32);

cmdline_parse_token_string_t cmd_link_up_up_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_up_result, up_string, "up");

cmdline_parse_inst_t cmd_link_up = {
	.f = cmd_link_up_parsed,
	.data = NULL,
	.help_str = "Link UP",
	.tokens = {
		(void *)&cmd_link_up_link_string,
		(void *)&cmd_link_up_link_id,
		(void *)&cmd_link_up_up_string,
		NULL,
	},
};

/*
 * link down
 */

struct cmd_link_down_result {
	cmdline_fixed_string_t link_string;
	uint32_t link_id;
	cmdline_fixed_string_t down_string;
};

static void
cmd_link_down_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	void *data)
{
	struct cmd_link_down_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_link_down(app, params->link_id);
	if (status != 0)
		printf("Command failed\n");
	else {
		struct app_link_params *p;

		APP_PARAM_FIND_BY_ID(app->link_params, "LINK", params->link_id,
			p);
		print_link_info(p);
	}
}

cmdline_parse_token_string_t cmd_link_down_link_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_down_result, link_string,
		"link");

cmdline_parse_token_num_t cmd_link_down_link_id =
	TOKEN_NUM_INITIALIZER(struct cmd_link_down_result, link_id, UINT32);

cmdline_parse_token_string_t cmd_link_down_down_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_down_result, down_string,
		"down");

cmdline_parse_inst_t cmd_link_down = {
	.f = cmd_link_down_parsed,
	.data = NULL,
	.help_str = "Link DOWN",
	.tokens = {
		(void *) &cmd_link_down_link_string,
		(void *) &cmd_link_down_link_id,
		(void *) &cmd_link_down_down_string,
		NULL,
	},
};

/*
 * link ls
 */

struct cmd_link_ls_result {
	cmdline_fixed_string_t link_string;
	cmdline_fixed_string_t ls_string;
};

static void
cmd_link_ls_parsed(
	__attribute__((unused)) void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	 void *data)
{
	struct app_params *app = data;
	uint32_t link_id;

	for (link_id = 0; link_id < app->n_links; link_id++) {
		struct app_link_params *p;

		APP_PARAM_FIND_BY_ID(app->link_params, "LINK", link_id, p);
		print_link_info(p);
	}
}

cmdline_parse_token_string_t cmd_link_ls_link_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_ls_result, link_string,
		"link");

cmdline_parse_token_string_t cmd_link_ls_ls_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_ls_result, ls_string, "ls");

cmdline_parse_inst_t cmd_link_ls = {
	.f = cmd_link_ls_parsed,
	.data = NULL,
	.help_str = "Link list",
	.tokens = {
		(void *)&cmd_link_ls_link_string,
		(void *)&cmd_link_ls_ls_string,
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

struct cmd_run_file_result {
	cmdline_fixed_string_t run_string;
	char file_name[APP_FILE_NAME_SIZE];
};

static void
cmd_run_parsed(
	void *parsed_result,
	struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_run_file_result *params = parsed_result;

	app_run_file(cl->ctx, params->file_name);
}

cmdline_parse_token_string_t cmd_run_run_string =
	TOKEN_STRING_INITIALIZER(struct cmd_run_file_result, run_string,
		"run");

cmdline_parse_token_string_t cmd_run_file_name =
	TOKEN_STRING_INITIALIZER(struct cmd_run_file_result, file_name, NULL);

cmdline_parse_inst_t cmd_run = {
	.f = cmd_run_parsed,
	.data = NULL,
	.help_str = "Run CLI script file",
	.tokens = {
		(void *) &cmd_run_run_string,
		(void *) &cmd_run_file_name,
		NULL,
	},
};

static cmdline_parse_ctx_t pipeline_common_cmds[] = {
	(cmdline_parse_inst_t *) &cmd_quit,
	(cmdline_parse_inst_t *) &cmd_run,

	(cmdline_parse_inst_t *) &cmd_link_config,
	(cmdline_parse_inst_t *) &cmd_link_up,
	(cmdline_parse_inst_t *) &cmd_link_down,
	(cmdline_parse_inst_t *) &cmd_link_ls,

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
		n_cmds * sizeof(cmdline_parse_ctx_t *));

	for (i = 0; i < n_cmds; i++)
		app->cmds[app->n_cmds + i]->data = app;

	app->n_cmds += n_cmds;
	app->cmds[app->n_cmds] = NULL;

	return 0;
}
