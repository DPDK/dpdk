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

#ifndef __INCLUDE_PIPELINE_COMMON_BE_H__
#define __INCLUDE_PIPELINE_COMMON_BE_H__

#include <rte_common.h>
#include <rte_ring.h>
#include <rte_pipeline.h>

#include "pipeline_be.h"

struct pipeline;

enum pipeline_msg_req_type {
	PIPELINE_MSG_REQ_PING = 0,
	PIPELINE_MSG_REQ_STATS_PORT_IN,
	PIPELINE_MSG_REQ_STATS_PORT_OUT,
	PIPELINE_MSG_REQ_STATS_TABLE,
	PIPELINE_MSG_REQ_PORT_IN_ENABLE,
	PIPELINE_MSG_REQ_PORT_IN_DISABLE,
	PIPELINE_MSG_REQ_CUSTOM,
	PIPELINE_MSG_REQS
};

typedef void *(*pipeline_msg_req_handler)(struct pipeline *p, void *msg);

struct pipeline {
	struct rte_pipeline *p;
	uint32_t port_in_id[PIPELINE_MAX_PORT_IN];
	uint32_t port_out_id[PIPELINE_MAX_PORT_OUT];
	uint32_t table_id[PIPELINE_MAX_TABLES];
	struct rte_ring *msgq_in[PIPELINE_MAX_MSGQ_IN];
	struct rte_ring *msgq_out[PIPELINE_MAX_MSGQ_OUT];

	uint32_t n_ports_in;
	uint32_t n_ports_out;
	uint32_t n_tables;
	uint32_t n_msgq;

	pipeline_msg_req_handler handlers[PIPELINE_MSG_REQS];
	char name[PIPELINE_NAME_SIZE];
	uint32_t log_level;
};

enum pipeline_log_level {
	PIPELINE_LOG_LEVEL_HIGH = 1,
	PIPELINE_LOG_LEVEL_LOW,
	PIPELINE_LOG_LEVELS
};

#define PLOG(p, level, fmt, ...)					\
do {									\
	if (p->log_level >= PIPELINE_LOG_LEVEL_ ## level)		\
		fprintf(stdout, "[%s] " fmt "\n", p->name, ## __VA_ARGS__);\
} while (0)

static inline void *
pipeline_msg_recv(struct pipeline *p,
	uint32_t msgq_id)
{
	struct rte_ring *r = p->msgq_in[msgq_id];
	void *msg;
	int status = rte_ring_sc_dequeue(r, &msg);

	if (status != 0)
		return NULL;

	return msg;
}

static inline void
pipeline_msg_send(struct pipeline *p,
	uint32_t msgq_id,
	void *msg)
{
	struct rte_ring *r = p->msgq_out[msgq_id];
	int status;

	do {
		status = rte_ring_sp_enqueue(r, msg);
	} while (status == -ENOBUFS);
}

struct pipeline_msg_req {
	enum pipeline_msg_req_type type;
};

struct pipeline_stats_msg_req {
	enum pipeline_msg_req_type type;
	uint32_t id;
};

struct pipeline_port_in_msg_req {
	enum pipeline_msg_req_type type;
	uint32_t port_id;
};

struct pipeline_custom_msg_req {
	enum pipeline_msg_req_type type;
	uint32_t subtype;
};

struct pipeline_msg_rsp {
	int status;
};

struct pipeline_stats_port_in_msg_rsp {
	int status;
	struct rte_pipeline_port_in_stats stats;
};

struct pipeline_stats_port_out_msg_rsp {
	int status;
	struct rte_pipeline_port_out_stats stats;
};

struct pipeline_stats_table_msg_rsp {
	int status;
	struct rte_pipeline_table_stats stats;
};

void *pipeline_msg_req_ping_handler(struct pipeline *p, void *msg);
void *pipeline_msg_req_stats_port_in_handler(struct pipeline *p, void *msg);
void *pipeline_msg_req_stats_port_out_handler(struct pipeline *p, void *msg);
void *pipeline_msg_req_stats_table_handler(struct pipeline *p, void *msg);
void *pipeline_msg_req_port_in_enable_handler(struct pipeline *p, void *msg);
void *pipeline_msg_req_port_in_disable_handler(struct pipeline *p, void *msg);
void *pipeline_msg_req_invalid_handler(struct pipeline *p, void *msg);

int pipeline_msg_req_handle(struct pipeline *p);

#endif
