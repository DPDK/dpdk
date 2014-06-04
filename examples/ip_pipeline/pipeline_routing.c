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
#include <rte_table_lpm.h>
#include <rte_table_hash.h>
#include <rte_pipeline.h>

#include "main.h"

#include <unistd.h>

struct app_routing_table_entry {
	struct rte_pipeline_table_entry head;
	uint32_t nh_ip;
	uint32_t nh_iface;
};

struct app_arp_table_entry {
	struct rte_pipeline_table_entry head;
	struct ether_addr nh_arp;
};

static inline void
app_routing_table_write_metadata(
	struct rte_mbuf *pkt,
	struct app_routing_table_entry *entry)
{
	struct app_pkt_metadata *c =
		(struct app_pkt_metadata *) RTE_MBUF_METADATA_UINT8_PTR(pkt, 0);

	c->arp_key.nh_ip = entry->nh_ip;
	c->arp_key.nh_iface = entry->nh_iface;
}

static int
app_routing_table_ah(
	struct rte_mbuf **pkts,
	uint64_t *pkts_mask,
	struct rte_pipeline_table_entry **entries,
	__attribute__((unused)) void *arg)
{
	uint64_t pkts_in_mask = *pkts_mask;

	if ((pkts_in_mask & (pkts_in_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_in_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++) {
			struct rte_mbuf *m = pkts[i];
			struct app_routing_table_entry *a =
				(struct app_routing_table_entry *) entries[i];

			app_routing_table_write_metadata(m, a);
		}
	} else
		for ( ; pkts_in_mask; ) {
			struct rte_mbuf *m;
			struct app_routing_table_entry *a;
			uint64_t pkt_mask;
			uint32_t packet_index;

			packet_index = __builtin_ctzll(pkts_in_mask);
			pkt_mask = 1LLU << packet_index;
			pkts_in_mask &= ~pkt_mask;

			m = pkts[packet_index];
			a = (struct app_routing_table_entry *)
				entries[packet_index];
			app_routing_table_write_metadata(m, a);
		}

	return 0;
}

static inline void
app_arp_table_write_metadata(
	struct rte_mbuf *pkt,
	struct app_arp_table_entry *entry)
{
	struct app_pkt_metadata *c =
		(struct app_pkt_metadata *) RTE_MBUF_METADATA_UINT8_PTR(pkt, 0);
	ether_addr_copy(&entry->nh_arp, &c->nh_arp);
}

static int
app_arp_table_ah(
	struct rte_mbuf **pkts,
	uint64_t *pkts_mask,
	struct rte_pipeline_table_entry **entries,
	__attribute__((unused)) void *arg)
{
	uint64_t pkts_in_mask = *pkts_mask;

	if ((pkts_in_mask & (pkts_in_mask + 1)) == 0) {
		uint64_t n_pkts = __builtin_popcountll(pkts_in_mask);
		uint32_t i;

		for (i = 0; i < n_pkts; i++) {
			struct rte_mbuf *m = pkts[i];
			struct app_arp_table_entry *a =
				(struct app_arp_table_entry *) entries[i];

			app_arp_table_write_metadata(m, a);
		}
	} else {
		for ( ; pkts_in_mask; ) {
			struct rte_mbuf *m;
			struct app_arp_table_entry *a;
			uint64_t pkt_mask;
			uint32_t packet_index;

			packet_index = __builtin_ctzll(pkts_in_mask);
			pkt_mask = 1LLU << packet_index;
			pkts_in_mask &= ~pkt_mask;

			m = pkts[packet_index];
			a = (struct app_arp_table_entry *)
				entries[packet_index];
			app_arp_table_write_metadata(m, a);
		}
	}

	return 0;
}

static uint64_t app_arp_table_hash(
	void *key,
	__attribute__((unused)) uint32_t key_size,
	__attribute__((unused)) uint64_t seed)
{
	uint32_t *k = (uint32_t *) key;

	return k[1];
}

struct app_core_routing_message_handle_params {
	struct rte_ring *ring_req;
	struct rte_ring *ring_resp;
	struct rte_pipeline *p;
	uint32_t *port_out_id;
	uint32_t routing_table_id;
	uint32_t arp_table_id;
};

static void
app_message_handle(struct app_core_routing_message_handle_params *params);

void
app_main_loop_pipeline_routing(void) {
	struct rte_pipeline_params pipeline_params = {
		.name = "pipeline",
		.socket_id = rte_socket_id(),
	};

	struct rte_pipeline *p;
	uint32_t port_in_id[APP_MAX_PORTS];
	uint32_t port_out_id[APP_MAX_PORTS];
	uint32_t routing_table_id, arp_table_id;
	uint32_t i;

	uint32_t core_id = rte_lcore_id();
	struct app_core_params *core_params = app_get_core_params(core_id);
	struct app_core_routing_message_handle_params mh_params;

	if ((core_params == NULL) || (core_params->core_type != APP_CORE_RT))
		rte_panic("Core %u misconfiguration\n", core_id);

	RTE_LOG(INFO, USER1, "Core %u is doing routing\n", core_id);

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

	/* Routing table configuration */
	{
		struct rte_table_lpm_params table_lpm_params = {
			.n_rules = app.max_routing_rules,
			.entry_unique_size =
				sizeof(struct app_routing_table_entry),
			.offset = __builtin_offsetof(struct app_pkt_metadata,
				flow_key.ip_dst),
		};

		struct rte_pipeline_table_params table_params = {
			.ops = &rte_table_lpm_ops,
			.arg_create = &table_lpm_params,
			.f_action_hit = app_routing_table_ah,
			.f_action_miss = NULL,
			.arg_ah = NULL,
			.action_data_size =
				sizeof(struct app_routing_table_entry) -
				sizeof(struct rte_pipeline_table_entry),
		};

		if (rte_pipeline_table_create(p, &table_params,
			&routing_table_id))
			rte_panic("Unable to configure the LPM table\n");
	}

	/* ARP table configuration */
	{
		struct rte_table_hash_key8_lru_params table_arp_params = {
			.n_entries = app.max_arp_rules,
			.f_hash = app_arp_table_hash,
			.seed = 0,
			.signature_offset = 0, /* Unused */
			.key_offset = __builtin_offsetof(
				struct app_pkt_metadata, arp_key),
		};

		struct rte_pipeline_table_params table_params = {
			.ops = &rte_table_hash_key8_lru_dosig_ops,
			.arg_create = &table_arp_params,
			.f_action_hit = app_arp_table_ah,
			.f_action_miss = NULL,
			.arg_ah = NULL,
			.action_data_size = sizeof(struct app_arp_table_entry) -
				sizeof(struct rte_pipeline_table_entry),
		};

		if (rte_pipeline_table_create(p, &table_params, &arp_table_id))
			rte_panic("Unable to configure the ARP table\n");
	}

	/* Interconnecting ports and tables */
	for (i = 0; i < app.n_ports; i++) {
		if (rte_pipeline_port_in_connect_to_table(p, port_in_id[i],
			routing_table_id))
			rte_panic("Unable to connect input port %u to "
				"table %u\n", port_in_id[i],  routing_table_id);
	}

	/* Enable input ports */
	for (i = 0; i < app.n_ports; i++)
		if (rte_pipeline_port_in_enable(p, port_in_id[i]))
			rte_panic("Unable to enable input port %u\n",
				port_in_id[i]);

	/* Check pipeline consistency */
	if (rte_pipeline_check(p) < 0)
		rte_panic("Pipeline consistency check failed\n");

	/* Message handling */
	mh_params.ring_req =
		app_get_ring_req(app_get_first_core_id(APP_CORE_RT));
	mh_params.ring_resp =
		app_get_ring_resp(app_get_first_core_id(APP_CORE_RT));
	mh_params.p = p;
	mh_params.port_out_id = port_out_id;
	mh_params.routing_table_id = routing_table_id;
	mh_params.arp_table_id = arp_table_id;

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
app_message_handle(struct app_core_routing_message_handle_params *params)
{
	struct rte_ring *ring_req = params->ring_req;
	struct rte_ring *ring_resp;
	void *msg;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	struct rte_pipeline *p;
	uint32_t *port_out_id;
	uint32_t routing_table_id, arp_table_id;
	int result;

	/* Read request message */
	result = rte_ring_sc_dequeue(ring_req, &msg);
	if (result != 0)
		return;

	ring_resp = params->ring_resp;
	p = params->p;
	port_out_id = params->port_out_id;
	routing_table_id = params->routing_table_id;
	arp_table_id = params->arp_table_id;

	/* Handle request */
	req = (struct app_msg_req *) ((struct rte_mbuf *)msg)->ctrl.data;
	switch (req->type) {
	case APP_MSG_REQ_PING:
	{
		result = 0;
		break;
	}

	case APP_MSG_REQ_RT_ADD:
	{
		struct app_routing_table_entry entry = {
			.head = {
				.action = RTE_PIPELINE_ACTION_TABLE,
				{.table_id = arp_table_id},
			},
			.nh_ip = req->routing_add.nh_ip,
			.nh_iface = port_out_id[req->routing_add.port],
		};

		struct rte_table_lpm_key key = {
			.ip = req->routing_add.ip,
			.depth = req->routing_add.depth,
		};

		struct rte_pipeline_table_entry *entry_ptr;

		int key_found;

		result = rte_pipeline_table_entry_add(p, routing_table_id, &key,
			(struct rte_pipeline_table_entry *) &entry, &key_found,
			&entry_ptr);
		break;
	}

	case APP_MSG_REQ_RT_DEL:
	{
		struct rte_table_lpm_key key = {
			.ip = req->routing_del.ip,
			.depth = req->routing_del.depth,
		};

		int key_found;

		result = rte_pipeline_table_entry_delete(p, routing_table_id,
			&key, &key_found, NULL);
		break;
	}

	case APP_MSG_REQ_ARP_ADD:
	{

		struct app_arp_table_entry entry = {
			.head = {
				.action = RTE_PIPELINE_ACTION_PORT,
				{.port_id =
					port_out_id[req->arp_add.out_iface]},
			},
			.nh_arp = req->arp_add.nh_arp,
		};

		struct app_arp_key arp_key = {
			.nh_ip = req->arp_add.nh_ip,
			.nh_iface = port_out_id[req->arp_add.out_iface],
		};

		struct rte_pipeline_table_entry *entry_ptr;

		int key_found;

		result = rte_pipeline_table_entry_add(p, arp_table_id, &arp_key,
			(struct rte_pipeline_table_entry *) &entry, &key_found,
			&entry_ptr);
		break;
	}

	case APP_MSG_REQ_ARP_DEL:
	{
		struct app_arp_key arp_key = {
			.nh_ip = req->arp_del.nh_ip,
			.nh_iface = port_out_id[req->arp_del.out_iface],
		};

		int key_found;

		result = rte_pipeline_table_entry_delete(p, arp_table_id,
			&arp_key, &key_found, NULL);
		break;
	}

	default:
		rte_panic("RT Unrecognized message type (%u)\n", req->type);
	}

	/* Fill in response message */
	resp = (struct app_msg_resp *) ((struct rte_mbuf *)msg)->ctrl.data;
	resp->result = result;

	/* Send response */
	do {
		result = rte_ring_sp_enqueue(ring_resp, msg);
	} while (result == -ENOBUFS);
}
