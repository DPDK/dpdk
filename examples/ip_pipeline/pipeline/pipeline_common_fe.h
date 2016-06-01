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

#ifndef __INCLUDE_PIPELINE_COMMON_FE_H__
#define __INCLUDE_PIPELINE_COMMON_FE_H__

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <cmdline_parse.h>

#include "pipeline_common_be.h"
#include "pipeline.h"
#include "app.h"

#ifndef MSG_TIMEOUT_DEFAULT
#define MSG_TIMEOUT_DEFAULT                      1000
#endif

static inline struct app_pipeline_data *
app_pipeline_data(struct app_params *app, uint32_t id)
{
	struct app_pipeline_params *params;

	APP_PARAM_FIND_BY_ID(app->pipeline_params, "PIPELINE", id, params);
	if (params == NULL)
		return NULL;

	return &app->pipeline_data[params - app->pipeline_params];
}

static inline void *
app_pipeline_data_fe(struct app_params *app, uint32_t id, struct pipeline_type *ptype)
{
	struct app_pipeline_data *pipeline_data;

	pipeline_data = app_pipeline_data(app, id);
	if (pipeline_data == NULL)
		return NULL;

	if (strcmp(pipeline_data->ptype->name, ptype->name) != 0)
		return NULL;

	if (pipeline_data->enabled == 0)
		return NULL;

	return pipeline_data->fe;
}

static inline struct rte_ring *
app_pipeline_msgq_in_get(struct app_params *app,
	uint32_t pipeline_id)
{
	struct app_msgq_params *p;

	APP_PARAM_FIND_BY_ID(app->msgq_params,
		"MSGQ-REQ-PIPELINE",
		pipeline_id,
		p);
	if (p == NULL)
		return NULL;

	return app->msgq[p - app->msgq_params];
}

static inline struct rte_ring *
app_pipeline_msgq_out_get(struct app_params *app,
	uint32_t pipeline_id)
{
	struct app_msgq_params *p;

	APP_PARAM_FIND_BY_ID(app->msgq_params,
		"MSGQ-RSP-PIPELINE",
		pipeline_id,
		p);
	if (p == NULL)
		return NULL;

	return app->msgq[p - app->msgq_params];
}

static inline void *
app_msg_alloc(__rte_unused struct app_params *app)
{
	return rte_malloc(NULL, 2048, RTE_CACHE_LINE_SIZE);
}

static inline void
app_msg_free(__rte_unused struct app_params *app,
	void *msg)
{
	rte_free(msg);
}

static inline void
app_msg_send(struct app_params *app,
	uint32_t pipeline_id,
	void *msg)
{
	struct rte_ring *r = app_pipeline_msgq_in_get(app, pipeline_id);
	int status;

	do {
		status = rte_ring_sp_enqueue(r, msg);
	} while (status == -ENOBUFS);
}

static inline void *
app_msg_recv(struct app_params *app,
	uint32_t pipeline_id)
{
	struct rte_ring *r = app_pipeline_msgq_out_get(app, pipeline_id);
	void *msg;
	int status = rte_ring_sc_dequeue(r, &msg);

	if (status != 0)
		return NULL;

	return msg;
}

static inline void *
app_msg_send_recv(struct app_params *app,
	uint32_t pipeline_id,
	void *msg,
	uint32_t timeout_ms)
{
	struct rte_ring *r_req = app_pipeline_msgq_in_get(app, pipeline_id);
	struct rte_ring *r_rsp = app_pipeline_msgq_out_get(app, pipeline_id);
	uint64_t hz = rte_get_tsc_hz();
	void *msg_recv;
	uint64_t deadline;
	int status;

	/* send */
	do {
		status = rte_ring_sp_enqueue(r_req, (void *) msg);
	} while (status == -ENOBUFS);

	/* recv */
	deadline = (timeout_ms) ?
		(rte_rdtsc() + ((hz * timeout_ms) / 1000)) :
		UINT64_MAX;

	do {
		if (rte_rdtsc() > deadline)
			return NULL;

		status = rte_ring_sc_dequeue(r_rsp, &msg_recv);
	} while (status != 0);

	return msg_recv;
}

struct app_link_params *
app_pipeline_track_pktq_out_to_link(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t pktq_out_id);

int
app_pipeline_track_default(struct pipeline_params *params,
	uint32_t port_in,
	uint32_t *port_out);

int
app_pipeline_ping(struct app_params *app,
	uint32_t pipeline_id);

int
app_pipeline_stats_port_in(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id,
	struct rte_pipeline_port_in_stats *stats);

int
app_pipeline_stats_port_out(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id,
	struct rte_pipeline_port_out_stats *stats);

int
app_pipeline_stats_table(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t table_id,
	struct rte_pipeline_table_stats *stats);

int
app_pipeline_port_in_enable(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id);

int
app_pipeline_port_in_disable(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id);

int
app_link_set_op(struct app_params *app,
	uint32_t link_id,
	uint32_t pipeline_id,
	app_link_op op,
	void *arg);

int
app_link_config(struct app_params *app,
	uint32_t link_id,
	uint32_t ip,
	uint32_t depth);

int
app_link_up(struct app_params *app,
	uint32_t link_id);

int
app_link_down(struct app_params *app,
	uint32_t link_id);

int
app_pipeline_common_cmd_push(struct app_params *app);

#define CMD_MSG_OUT_OF_MEMORY	"Not enough memory\n"
#define CMD_MSG_NOT_ENOUGH_ARGS	"Not enough arguments for command \"%s\"\n"
#define CMD_MSG_TOO_MANY_ARGS	"Too many arguments for command \"%s\"\n"
#define CMD_MSG_MISMATCH_ARGS	"Incorrect set of arguments for command \"%s\"\n"
#define CMD_MSG_INVALID_ARG	"Invalid value for argument \"%s\"\n"
#define CMD_MSG_ARG_NOT_FOUND	"Syntax error: \"%s\" not found\n"
#define CMD_MSG_FILE_ERR	"Error in file \"%s\" at line %u\n"
#define CMD_MSG_FAIL		"Command \"%s\" failed\n"

#endif
