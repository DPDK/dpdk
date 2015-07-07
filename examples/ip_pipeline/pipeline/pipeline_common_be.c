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

#include <rte_common.h>
#include <rte_ring.h>
#include <rte_malloc.h>

#include "pipeline_common_be.h"

void *
pipeline_msg_req_ping_handler(__rte_unused struct pipeline *p,
	void *msg)
{
	struct pipeline_msg_rsp *rsp = msg;

	rsp->status = 0; /* OK */

	return rsp;
}

void *
pipeline_msg_req_stats_port_in_handler(struct pipeline *p,
	void *msg)
{
	struct pipeline_stats_msg_req *req = msg;
	struct pipeline_stats_port_in_msg_rsp *rsp = msg;
	uint32_t port_id;

	/* Check request */
	if (req->id >= p->n_ports_in) {
		rsp->status = -1;
		return rsp;
	}
	port_id = p->port_in_id[req->id];

	/* Process request */
	rsp->status = rte_pipeline_port_in_stats_read(p->p,
		port_id,
		&rsp->stats,
		1);

	return rsp;
}

void *
pipeline_msg_req_stats_port_out_handler(struct pipeline *p,
	void *msg)
{
	struct pipeline_stats_msg_req *req = msg;
	struct pipeline_stats_port_out_msg_rsp *rsp = msg;
	uint32_t port_id;

	/* Check request */
	if (req->id >= p->n_ports_out) {
		rsp->status = -1;
		return rsp;
	}
	port_id = p->port_out_id[req->id];

	/* Process request */
	rsp->status = rte_pipeline_port_out_stats_read(p->p,
		port_id,
		&rsp->stats,
		1);

	return rsp;
}

void *
pipeline_msg_req_stats_table_handler(struct pipeline *p,
	void *msg)
{
	struct pipeline_stats_msg_req *req = msg;
	struct pipeline_stats_table_msg_rsp *rsp = msg;
	uint32_t table_id;

	/* Check request */
	if (req->id >= p->n_tables) {
		rsp->status = -1;
		return rsp;
	}
	table_id = p->table_id[req->id];

	/* Process request */
	rsp->status = rte_pipeline_table_stats_read(p->p,
		table_id,
		&rsp->stats,
		1);

	return rsp;
}

void *
pipeline_msg_req_port_in_enable_handler(struct pipeline *p,
	void *msg)
{
	struct pipeline_port_in_msg_req *req = msg;
	struct pipeline_msg_rsp *rsp = msg;
	uint32_t port_id;

	/* Check request */
	if (req->port_id >= p->n_ports_in) {
		rsp->status = -1;
		return rsp;
	}
	port_id = p->port_in_id[req->port_id];

	/* Process request */
	rsp->status = rte_pipeline_port_in_enable(p->p,
		port_id);

	return rsp;
}

void *
pipeline_msg_req_port_in_disable_handler(struct pipeline *p,
	void *msg)
{
	struct pipeline_port_in_msg_req *req = msg;
	struct pipeline_msg_rsp *rsp = msg;
	uint32_t port_id;

	/* Check request */
	if (req->port_id >= p->n_ports_in) {
		rsp->status = -1;
		return rsp;
	}
	port_id = p->port_in_id[req->port_id];

	/* Process request */
	rsp->status = rte_pipeline_port_in_disable(p->p,
		port_id);

	return rsp;
}

void *
pipeline_msg_req_invalid_handler(__rte_unused struct pipeline *p,
	void *msg)
{
	struct pipeline_msg_rsp *rsp = msg;

	rsp->status = -1; /* Error */

	return rsp;
}

int
pipeline_msg_req_handle(struct pipeline *p)
{
	uint32_t msgq_id;

	for (msgq_id = 0; msgq_id < p->n_msgq; msgq_id++) {
		for ( ; ; ) {
			struct pipeline_msg_req *req;
			pipeline_msg_req_handler f_handle;

			req = pipeline_msg_recv(p, msgq_id);
			if (req == NULL)
				break;

			f_handle = (req->type < PIPELINE_MSG_REQS) ?
				p->handlers[req->type] :
				pipeline_msg_req_invalid_handler;

			if (f_handle == NULL)
				f_handle = pipeline_msg_req_invalid_handler;

			pipeline_msg_send(p,
				msgq_id,
				f_handle(p, (void *) req));
		}
	}

	return 0;
}
