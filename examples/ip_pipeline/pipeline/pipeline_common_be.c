/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <rte_common.h>
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
