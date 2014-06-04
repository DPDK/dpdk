/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
#include <stdlib.h>
#include <stdint.h>

#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_byteorder.h>

#include <rte_port_ring.h>
#include <rte_table_hash.h>
#include <rte_pipeline.h>

#include "main.h"

struct app_core_fc_message_handle_params {
	struct rte_ring *ring_req;
	struct rte_ring *ring_resp;

	struct rte_pipeline *p;
	uint32_t *port_out_id;
	uint32_t table_id;
};

static void
app_message_handle(struct app_core_fc_message_handle_params *params);

static int app_flow_classification_table_init(
	struct rte_pipeline *p,
	uint32_t *port_out_id,
	uint32_t table_id)
{
	struct app_flow_key flow_key;
	uint32_t i;

	/* Add entries to tables */
	for (i = 0; i < (1 << 24); i++) {
		struct rte_pipeline_table_entry entry = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = port_out_id[i & (app.n_ports - 1)]},
		};
		struct rte_pipeline_table_entry *entry_ptr;
		int key_found, status;

		flow_key.ttl = 0;
		flow_key.proto = 6; /* TCP */
		flow_key.header_checksum = 0;
		flow_key.ip_src = 0;
		flow_key.ip_dst = rte_bswap32(i);
		flow_key.port_src = 0;
		flow_key.port_dst = 0;

		status = rte_pipeline_table_entry_add(p, table_id,
			(void *) &flow_key, &entry, &key_found, &entry_ptr);
		if (status < 0)
			rte_panic("Unable to add entry to table %u (%d)\n",
				table_id, status);
	}

	return 0;
}

void
app_main_loop_pipeline_flow_classification(void) {
	struct rte_pipeline_params pipeline_params = {
		.name = "pipeline",
		.socket_id = rte_socket_id(),
	};

	struct rte_pipeline *p;
	uint32_t port_in_id[APP_MAX_PORTS];
	uint32_t port_out_id[APP_MAX_PORTS];
	uint32_t table_id;
	uint32_t i;

	uint32_t core_id = rte_lcore_id();
	struct app_core_params *core_params = app_get_core_params(core_id);
	struct app_core_fc_message_handle_params mh_params;

	if ((core_params == NULL) || (core_params->core_type != APP_CORE_FC))
		rte_panic("Core %u misconfiguration\n", core_id);

	RTE_LOG(INFO, USER1, "Core %u is doing flow classification "
		"(pipeline with hash table, 16-byte key, LRU)\n", core_id);

	/* Pipeline configuration */
	p = rte_pipeline_create(&pipeline_params);
	if (p == NULL)
		rte_panic("Unable to configure the pipeline\n");

	/* Input port configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_port_ring_reader_params port_ring_params = {
			.ring = app.rings[core_params->swq_in[i]],
		};

		struct rte_pipeline_port_in_params port_params = {
			.ops = &rte_port_ring_reader_ops,
			.arg_create = (void *) &port_ring_params,
			.f_action = NULL,
			.arg_ah = NULL,
			.burst_size = app.bsz_swq_rd,
		};

		if (rte_pipeline_port_in_create(p, &port_params,
			&port_in_id[i]))
			rte_panic("Unable to configure input port for "
				"ring %d\n", i);
	}

	/* Output port configuration */
	for (i = 0; i < app.n_ports; i++) {
		struct rte_port_ring_writer_params port_ring_params = {
			.ring = app.rings[core_params->swq_out[i]],
			.tx_burst_sz = app.bsz_swq_wr,
		};

		struct rte_pipeline_port_out_params port_params = {
			.ops = &rte_port_ring_writer_ops,
			.arg_create = (void *) &port_ring_params,
			.f_action = NULL,
			.f_action_bulk = NULL,
			.arg_ah = NULL,
		};

		if (rte_pipeline_port_out_create(p, &port_params,
			&port_out_id[i]))
			rte_panic("Unable to configure output port for "
				"ring %d\n", i);
	}

	/* Table configuration */
	{
		struct rte_table_hash_key16_lru_params table_hash_params = {
			.n_entries = 1 << 24,
			.signature_offset = __builtin_offsetof(
				struct app_pkt_metadata, signature),
			.key_offset = __builtin_offsetof(
				struct app_pkt_metadata, flow_key),
			.f_hash = test_hash,
			.seed = 0,
		};

		struct rte_pipeline_table_params table_params = {
			.ops = &rte_table_hash_key16_lru_ops,
			.arg_create = &table_hash_params,
			.f_action_hit = NULL,
			.f_action_miss = NULL,
			.arg_ah = NULL,
			.action_data_size = 0,
		};

		if (rte_pipeline_table_create(p, &table_params, &table_id))
			rte_panic("Unable to configure the hash table\n");
	}

	/* Interconnecting ports and tables */
	for (i = 0; i < app.n_ports; i++)
		if (rte_pipeline_port_in_connect_to_table(p, port_in_id[i],
			table_id))
			rte_panic("Unable to connect input port %u to "
				"table %u\n", port_in_id[i],  table_id);

	/* Enable input ports */
	for (i = 0; i < app.n_ports; i++)
		if (rte_pipeline_port_in_enable(p, port_in_id[i]))
			rte_panic("Unable to enable input port %u\n",
				port_in_id[i]);

	/* Check pipeline consistency */
	if (rte_pipeline_check(p) < 0)
		rte_panic("Pipeline consistency check failed\n");

	/* Message handling */
	mh_params.ring_req = app_get_ring_req(
		app_get_first_core_id(APP_CORE_FC));
	mh_params.ring_resp = app_get_ring_resp(
		app_get_first_core_id(APP_CORE_FC));
	mh_params.p = p;
	mh_params.port_out_id = port_out_id;
	mh_params.table_id = table_id;

	/* Run-time */
	for (i = 0; ; i++) {
		rte_pipeline_run(p);

		if ((i & APP_FLUSH) == 0) {
			rte_pipeline_flush(p);
			app_message_handle(&mh_params);
		}
	}
}

void
app_message_handle(struct app_core_fc_message_handle_params *params)
{
	struct rte_ring *ring_req = params->ring_req;
	struct rte_ring *ring_resp;
	void *msg;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	struct rte_pipeline *p;
	uint32_t *port_out_id;
	uint32_t table_id;
	int result;

	/* Read request message */
	result = rte_ring_sc_dequeue(ring_req, &msg);
	if (result != 0)
		return;

	ring_resp = params->ring_resp;
	p = params->p;
	port_out_id = params->port_out_id;
	table_id = params->table_id;

	/* Handle request */
	req = (struct app_msg_req *) ((struct rte_mbuf *)msg)->ctrl.data;
	switch (req->type) {
	case APP_MSG_REQ_PING:
	{
		result = 0;
		break;
	}

	case APP_MSG_REQ_FC_ADD_ALL:
	{
		result = app_flow_classification_table_init(p, port_out_id,
			table_id);
		break;
	}

	case APP_MSG_REQ_FC_ADD:
	{
		struct rte_pipeline_table_entry entry = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = port_out_id[req->flow_classif_add.port]},
		};

		struct rte_pipeline_table_entry *entry_ptr;

		int key_found;

		result = rte_pipeline_table_entry_add(p, table_id,
			req->flow_classif_add.key_raw, &entry, &key_found,
			&entry_ptr);
		break;
	}

	case APP_MSG_REQ_FC_DEL:
	{
		int key_found;

		result = rte_pipeline_table_entry_delete(p, table_id,
			req->flow_classif_add.key_raw, &key_found, NULL);
		break;
	}

	default:
		rte_panic("FC Unrecognized message type (%u)\n", req->type);
	}

	/* Fill in response message */
	resp = (struct app_msg_resp *) ((struct rte_mbuf *)msg)->ctrl.data;
	resp->result = result;

	/* Send response */
	do {
		result = rte_ring_sp_enqueue(ring_resp, msg);
	} while (result == -ENOBUFS);
}
