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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_ip.h>
#include <rte_byteorder.h>
#include <rte_table_lpm.h>
#include <rte_table_hash.h>
#include <rte_pipeline.h>

#include "pipeline_routing_be.h"
#include "pipeline_actions_common.h"
#include "hash_func.h"

struct pipeline_routing {
	struct pipeline p;
	pipeline_msg_req_handler custom_handlers[PIPELINE_ROUTING_MSG_REQS];

	uint32_t n_routes;
	uint32_t n_arp_entries;
	uint32_t ip_da_offset;
	uint32_t arp_key_offset;
} __rte_cache_aligned;

static void *
pipeline_routing_msg_req_custom_handler(struct pipeline *p, void *msg);

static pipeline_msg_req_handler handlers[] = {
	[PIPELINE_MSG_REQ_PING] =
		pipeline_msg_req_ping_handler,
	[PIPELINE_MSG_REQ_STATS_PORT_IN] =
		pipeline_msg_req_stats_port_in_handler,
	[PIPELINE_MSG_REQ_STATS_PORT_OUT] =
		pipeline_msg_req_stats_port_out_handler,
	[PIPELINE_MSG_REQ_STATS_TABLE] =
		pipeline_msg_req_stats_table_handler,
	[PIPELINE_MSG_REQ_PORT_IN_ENABLE] =
		pipeline_msg_req_port_in_enable_handler,
	[PIPELINE_MSG_REQ_PORT_IN_DISABLE] =
		pipeline_msg_req_port_in_disable_handler,
	[PIPELINE_MSG_REQ_CUSTOM] =
		pipeline_routing_msg_req_custom_handler,
};

static void *
pipeline_routing_msg_req_route_add_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_route_del_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_route_add_default_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_route_del_default_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_arp_add_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_arp_del_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_arp_add_default_handler(struct pipeline *p,
	void *msg);

static void *
pipeline_routing_msg_req_arp_del_default_handler(struct pipeline *p,
	void *msg);

static pipeline_msg_req_handler custom_handlers[] = {
	[PIPELINE_ROUTING_MSG_REQ_ROUTE_ADD] =
		pipeline_routing_msg_req_route_add_handler,
	[PIPELINE_ROUTING_MSG_REQ_ROUTE_DEL] =
		pipeline_routing_msg_req_route_del_handler,
	[PIPELINE_ROUTING_MSG_REQ_ROUTE_ADD_DEFAULT] =
		pipeline_routing_msg_req_route_add_default_handler,
	[PIPELINE_ROUTING_MSG_REQ_ROUTE_DEL_DEFAULT] =
		pipeline_routing_msg_req_route_del_default_handler,
	[PIPELINE_ROUTING_MSG_REQ_ARP_ADD] =
		pipeline_routing_msg_req_arp_add_handler,
	[PIPELINE_ROUTING_MSG_REQ_ARP_DEL] =
		pipeline_routing_msg_req_arp_del_handler,
	[PIPELINE_ROUTING_MSG_REQ_ARP_ADD_DEFAULT] =
		pipeline_routing_msg_req_arp_add_default_handler,
	[PIPELINE_ROUTING_MSG_REQ_ARP_DEL_DEFAULT] =
		pipeline_routing_msg_req_arp_del_default_handler,
};

/*
 * Routing table
 */
struct routing_table_entry {
	struct rte_pipeline_table_entry head;
	uint32_t flags;
	uint32_t port_id; /* Output port ID */
	uint32_t ip; /* Next hop IP address (only valid for remote routes) */
};

static inline void
pkt_work_routing(
	struct rte_mbuf *pkt,
	struct rte_pipeline_table_entry *table_entry,
	void *arg)
{
	struct routing_table_entry *entry =
		(struct routing_table_entry *) table_entry;
	struct pipeline_routing *p_rt = arg;

	struct pipeline_routing_arp_key_ipv4 *arp_key =
		(struct pipeline_routing_arp_key_ipv4 *)
		RTE_MBUF_METADATA_UINT8_PTR(pkt, p_rt->arp_key_offset);
	uint32_t ip = RTE_MBUF_METADATA_UINT32(pkt, p_rt->ip_da_offset);

	arp_key->port_id = entry->port_id;
	arp_key->ip = entry->ip;
	if (entry->flags & PIPELINE_ROUTING_ROUTE_LOCAL)
		arp_key->ip = ip;
}

static inline void
pkt4_work_routing(
	struct rte_mbuf **pkts,
	struct rte_pipeline_table_entry **table_entries,
	void *arg)
{
	struct routing_table_entry *entry0 =
		(struct routing_table_entry *) table_entries[0];
	struct routing_table_entry *entry1 =
		(struct routing_table_entry *) table_entries[1];
	struct routing_table_entry *entry2 =
		(struct routing_table_entry *) table_entries[2];
	struct routing_table_entry *entry3 =
		(struct routing_table_entry *) table_entries[3];
	struct pipeline_routing *p_rt = arg;

	struct pipeline_routing_arp_key_ipv4 *arp_key0 =
		(struct pipeline_routing_arp_key_ipv4 *)
		RTE_MBUF_METADATA_UINT8_PTR(pkts[0], p_rt->arp_key_offset);
	struct pipeline_routing_arp_key_ipv4 *arp_key1 =
		(struct pipeline_routing_arp_key_ipv4 *)
		RTE_MBUF_METADATA_UINT8_PTR(pkts[1], p_rt->arp_key_offset);
	struct pipeline_routing_arp_key_ipv4 *arp_key2 =
		(struct pipeline_routing_arp_key_ipv4 *)
		RTE_MBUF_METADATA_UINT8_PTR(pkts[2], p_rt->arp_key_offset);
	struct pipeline_routing_arp_key_ipv4 *arp_key3 =
		(struct pipeline_routing_arp_key_ipv4 *)
		RTE_MBUF_METADATA_UINT8_PTR(pkts[3], p_rt->arp_key_offset);

	uint32_t ip0 = RTE_MBUF_METADATA_UINT32(pkts[0], p_rt->ip_da_offset);
	uint32_t ip1 = RTE_MBUF_METADATA_UINT32(pkts[1], p_rt->ip_da_offset);
	uint32_t ip2 = RTE_MBUF_METADATA_UINT32(pkts[2], p_rt->ip_da_offset);
	uint32_t ip3 = RTE_MBUF_METADATA_UINT32(pkts[3], p_rt->ip_da_offset);

	arp_key0->port_id = entry0->port_id;
	arp_key1->port_id = entry1->port_id;
	arp_key2->port_id = entry2->port_id;
	arp_key3->port_id = entry3->port_id;

	arp_key0->ip = entry0->ip;
	if (entry0->flags & PIPELINE_ROUTING_ROUTE_LOCAL)
		arp_key0->ip = ip0;

	arp_key1->ip = entry1->ip;
	if (entry1->flags & PIPELINE_ROUTING_ROUTE_LOCAL)
		arp_key1->ip = ip1;

	arp_key2->ip = entry2->ip;
	if (entry2->flags & PIPELINE_ROUTING_ROUTE_LOCAL)
		arp_key2->ip = ip2;

	arp_key3->ip = entry3->ip;
	if (entry3->flags & PIPELINE_ROUTING_ROUTE_LOCAL)
		arp_key3->ip = ip3;
}

PIPELINE_TABLE_AH_HIT(routing_table_ah_hit,
	pkt_work_routing,
	pkt4_work_routing);

/*
 * ARP table
 */
struct arp_table_entry {
	struct rte_pipeline_table_entry head;
	uint64_t macaddr;
};

static inline void
pkt_work_arp(
	struct rte_mbuf *pkt,
	struct rte_pipeline_table_entry *table_entry,
	__rte_unused void *arg)
{
	struct arp_table_entry *entry = (struct arp_table_entry *) table_entry;

	/* Read: pkt buffer - mbuf */
	uint8_t *raw = rte_pktmbuf_mtod(pkt, uint8_t *);

	/* Read: table entry */
	uint64_t mac_addr_dst = entry->macaddr;
	uint64_t mac_addr_src = 0;

	/* Compute: Ethernet header */
	uint64_t slab0 = mac_addr_dst | (mac_addr_src << 48);
	uint32_t slab1 = mac_addr_src >> 16;

	/* Write: pkt buffer - pkt headers */
	*((uint64_t *) raw) = slab0;
	*((uint32_t *) (raw + 8)) = slab1;
}

static inline void
pkt4_work_arp(
	struct rte_mbuf **pkts,
	struct rte_pipeline_table_entry **table_entries,
	__rte_unused void *arg)
{
	struct arp_table_entry *entry0 =
		(struct arp_table_entry *) table_entries[0];
	struct arp_table_entry *entry1 =
		(struct arp_table_entry *) table_entries[1];
	struct arp_table_entry *entry2 =
		(struct arp_table_entry *) table_entries[2];
	struct arp_table_entry *entry3 =
		(struct arp_table_entry *) table_entries[3];

	/* Read: pkt buffer - mbuf */
	uint8_t *raw0 = rte_pktmbuf_mtod(pkts[0], uint8_t *);
	uint8_t *raw1 = rte_pktmbuf_mtod(pkts[1], uint8_t *);
	uint8_t *raw2 = rte_pktmbuf_mtod(pkts[2], uint8_t *);
	uint8_t *raw3 = rte_pktmbuf_mtod(pkts[3], uint8_t *);

	/* Read: table entry */
	uint64_t mac_addr_dst0 = entry0->macaddr;
	uint64_t mac_addr_dst1 = entry1->macaddr;
	uint64_t mac_addr_dst2 = entry2->macaddr;
	uint64_t mac_addr_dst3 = entry3->macaddr;

	uint64_t mac_addr_src0 = 0;
	uint64_t mac_addr_src1 = 0;
	uint64_t mac_addr_src2 = 0;
	uint64_t mac_addr_src3 = 0;

	/* Compute: Ethernet header */
	uint64_t pkt0_slab0 = mac_addr_dst0 | (mac_addr_src0 << 48);
	uint64_t pkt1_slab0 = mac_addr_dst1 | (mac_addr_src1 << 48);
	uint64_t pkt2_slab0 = mac_addr_dst2 | (mac_addr_src2 << 48);
	uint64_t pkt3_slab0 = mac_addr_dst3 | (mac_addr_src3 << 48);

	uint32_t pkt0_slab1 = mac_addr_src0 >> 16;
	uint32_t pkt1_slab1 = mac_addr_src1 >> 16;
	uint32_t pkt2_slab1 = mac_addr_src2 >> 16;
	uint32_t pkt3_slab1 = mac_addr_src3 >> 16;

	/* Write: pkt buffer - pkt headers */
	*((uint64_t *) raw0) = pkt0_slab0;
	*((uint32_t *) (raw0 + 8)) = pkt0_slab1;
	*((uint64_t *) raw1) = pkt1_slab0;
	*((uint32_t *) (raw1 + 8)) = pkt1_slab1;
	*((uint64_t *) raw2) = pkt2_slab0;
	*((uint32_t *) (raw2 + 8)) = pkt2_slab1;
	*((uint64_t *) raw3) = pkt3_slab0;
	*((uint32_t *) (raw3 + 8)) = pkt3_slab1;
}

PIPELINE_TABLE_AH_HIT(arp_table_ah_hit,
	pkt_work_arp,
	pkt4_work_arp);

static int
pipeline_routing_parse_args(struct pipeline_routing *p,
	struct pipeline_params *params)
{
	uint32_t n_routes_present = 0;
	uint32_t n_arp_entries_present = 0;
	uint32_t ip_da_offset_present = 0;
	uint32_t arp_key_offset_present = 0;
	uint32_t i;

	for (i = 0; i < params->n_args; i++) {
		char *arg_name = params->args_name[i];
		char *arg_value = params->args_value[i];

		/* n_routes */
		if (strcmp(arg_name, "n_routes") == 0) {
			if (n_routes_present)
				return -1;
			n_routes_present = 1;

			p->n_routes = atoi(arg_value);
			if (p->n_routes == 0)
				return -1;

			continue;
		}

		/* n_arp_entries */
		if (strcmp(arg_name, "n_arp_entries") == 0) {
			if (n_arp_entries_present)
				return -1;
			n_arp_entries_present = 1;

			p->n_arp_entries = atoi(arg_value);
			if (p->n_arp_entries == 0)
				return -1;

			continue;
		}

		/* ip_da_offset */
		if (strcmp(arg_name, "ip_da_offset") == 0) {
			if (ip_da_offset_present)
				return -1;
			ip_da_offset_present = 1;

			p->ip_da_offset = atoi(arg_value);

			continue;
		}

		/* arp_key_offset */
		if (strcmp(arg_name, "arp_key_offset") == 0) {
			if (arp_key_offset_present)
				return -1;
			arp_key_offset_present = 1;

			p->arp_key_offset = atoi(arg_value);

			continue;
		}

		/* any other */
		return -1;
	}

	/* Check that mandatory arguments are present */
	if ((n_routes_present == 0) ||
		(n_arp_entries_present == 0) ||
		(ip_da_offset_present == 0) ||
		(n_arp_entries_present && (arp_key_offset_present == 0)))
		return -1;

	return 0;
}

static void *
pipeline_routing_init(struct pipeline_params *params,
	__rte_unused void *arg)
{
	struct pipeline *p;
	struct pipeline_routing *p_rt;
	uint32_t size, i;

	/* Check input arguments */
	if ((params == NULL) ||
		(params->n_ports_in == 0) ||
		(params->n_ports_out == 0))
		return NULL;

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct pipeline_routing));
	p = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	p_rt = (struct pipeline_routing *) p;
	if (p == NULL)
		return NULL;

	strcpy(p->name, params->name);
	p->log_level = params->log_level;

	PLOG(p, HIGH, "Routing");

	/* Parse arguments */
	if (pipeline_routing_parse_args(p_rt, params))
		return NULL;

	/* Pipeline */
	{
		struct rte_pipeline_params pipeline_params = {
			.name = params->name,
			.socket_id = params->socket_id,
			.offset_port_id = 0,
		};

		p->p = rte_pipeline_create(&pipeline_params);
		if (p->p == NULL) {
			rte_free(p);
			return NULL;
		}
	}

	/* Input ports */
	p->n_ports_in = params->n_ports_in;
	for (i = 0; i < p->n_ports_in; i++) {
		struct rte_pipeline_port_in_params port_params = {
			.ops = pipeline_port_in_params_get_ops(
				&params->port_in[i]),
			.arg_create = pipeline_port_in_params_convert(
				&params->port_in[i]),
			.f_action = NULL,
			.arg_ah = NULL,
			.burst_size = params->port_in[i].burst_size,
		};

		int status = rte_pipeline_port_in_create(p->p,
			&port_params,
			&p->port_in_id[i]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Output ports */
	p->n_ports_out = params->n_ports_out;
	for (i = 0; i < p->n_ports_out; i++) {
		struct rte_pipeline_port_out_params port_params = {
			.ops = pipeline_port_out_params_get_ops(
				&params->port_out[i]),
			.arg_create = pipeline_port_out_params_convert(
				&params->port_out[i]),
			.f_action = NULL,
			.f_action_bulk = NULL,
			.arg_ah = NULL,
		};

		int status = rte_pipeline_port_out_create(p->p,
			&port_params,
			&p->port_out_id[i]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Routing table */
	p->n_tables = 1;
	{
		struct rte_table_lpm_params table_lpm_params = {
			.n_rules = p_rt->n_routes,
			.entry_unique_size = sizeof(struct routing_table_entry),
			.offset = p_rt->ip_da_offset,
		};

		struct rte_pipeline_table_params table_params = {
				.ops = &rte_table_lpm_ops,
				.arg_create = &table_lpm_params,
				.f_action_hit = routing_table_ah_hit,
				.f_action_miss = NULL,
				.arg_ah = p_rt,
				.action_data_size =
					sizeof(struct routing_table_entry) -
					sizeof(struct rte_pipeline_table_entry),
			};

		int status;

		status = rte_pipeline_table_create(p->p,
			&table_params,
			&p->table_id[0]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* ARP table configuration */
	if (p_rt->n_arp_entries) {
		struct rte_table_hash_key8_ext_params table_arp_params = {
			.n_entries = p_rt->n_arp_entries,
			.n_entries_ext = p_rt->n_arp_entries,
			.f_hash = hash_default_key8,
			.seed = 0,
			.signature_offset = 0, /* Unused */
			.key_offset = p_rt->arp_key_offset,
		};

		struct rte_pipeline_table_params table_params = {
			.ops = &rte_table_hash_key8_ext_dosig_ops,
			.arg_create = &table_arp_params,
			.f_action_hit = arp_table_ah_hit,
			.f_action_miss = NULL,
			.arg_ah = p_rt,
			.action_data_size = sizeof(struct arp_table_entry) -
				sizeof(struct rte_pipeline_table_entry),
		};

		int status;

		status = rte_pipeline_table_create(p->p,
			&table_params,
			&p->table_id[1]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}

		p->n_tables++;
	}

	/* Connecting input ports to tables */
	for (i = 0; i < p->n_ports_in; i++) {
		int status = rte_pipeline_port_in_connect_to_table(p->p,
			p->port_in_id[i],
			p->table_id[0]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Enable input ports */
	for (i = 0; i < p->n_ports_in; i++) {
		int status = rte_pipeline_port_in_enable(p->p,
			p->port_in_id[i]);

		if (status) {
			rte_pipeline_free(p->p);
			rte_free(p);
			return NULL;
		}
	}

	/* Check pipeline consistency */
	if (rte_pipeline_check(p->p) < 0) {
		rte_pipeline_free(p->p);
		rte_free(p);
		return NULL;
	}

	/* Message queues */
	p->n_msgq = params->n_msgq;
	for (i = 0; i < p->n_msgq; i++)
		p->msgq_in[i] = params->msgq_in[i];
	for (i = 0; i < p->n_msgq; i++)
		p->msgq_out[i] = params->msgq_out[i];

	/* Message handlers */
	memcpy(p->handlers, handlers, sizeof(p->handlers));
	memcpy(p_rt->custom_handlers,
		custom_handlers,
		sizeof(p_rt->custom_handlers));

	return p;
}

static int
pipeline_routing_free(void *pipeline)
{
	struct pipeline *p = (struct pipeline *) pipeline;

	/* Check input arguments */
	if (p == NULL)
		return -1;

	/* Free resources */
	rte_pipeline_free(p->p);
	rte_free(p);
	return 0;
}

static int
pipeline_routing_track(void *pipeline,
	__rte_unused uint32_t port_in,
	uint32_t *port_out)
{
	struct pipeline *p = (struct pipeline *) pipeline;

	/* Check input arguments */
	if ((p == NULL) ||
		(port_in >= p->n_ports_in) ||
		(port_out == NULL))
		return -1;

	if (p->n_ports_in == 1) {
		*port_out = 0;
		return 0;
	}

	return -1;
}

static int
pipeline_routing_timer(void *pipeline)
{
	struct pipeline *p = (struct pipeline *) pipeline;

	pipeline_msg_req_handle(p);
	rte_pipeline_flush(p->p);

	return 0;
}

void *
pipeline_routing_msg_req_custom_handler(struct pipeline *p,
	void *msg)
{
	struct pipeline_routing *p_rt = (struct pipeline_routing *) p;
	struct pipeline_custom_msg_req *req = msg;
	pipeline_msg_req_handler f_handle;

	f_handle = (req->subtype < PIPELINE_ROUTING_MSG_REQS) ?
		p_rt->custom_handlers[req->subtype] :
		pipeline_msg_req_invalid_handler;

	if (f_handle == NULL)
		f_handle = pipeline_msg_req_invalid_handler;

	return f_handle(p, req);
}

void *
pipeline_routing_msg_req_route_add_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing_route_add_msg_req *req = msg;
	struct pipeline_routing_route_add_msg_rsp *rsp = msg;

	struct rte_table_lpm_key key = {
		.ip = req->key.key.ipv4.ip,
		.depth = req->key.key.ipv4.depth,
	};

	struct routing_table_entry entry = {
		.head = {
			.action = RTE_PIPELINE_ACTION_TABLE,
			{.table_id = p->table_id[1]},
		},

		.flags = req->flags,
		.port_id = req->port_id,
		.ip = rte_bswap32(req->ip),
	};

	if (req->key.type != PIPELINE_ROUTING_ROUTE_IPV4) {
		rsp->status = -1;
		return rsp;
	}

	rsp->status = rte_pipeline_table_entry_add(p->p,
		p->table_id[0],
		&key,
		(struct rte_pipeline_table_entry *) &entry,
		&rsp->key_found,
		(struct rte_pipeline_table_entry **) &rsp->entry_ptr);

	return rsp;
}

void *
pipeline_routing_msg_req_route_del_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing_route_delete_msg_req *req = msg;
	struct pipeline_routing_route_delete_msg_rsp *rsp = msg;

	struct rte_table_lpm_key key = {
		.ip = req->key.key.ipv4.ip,
		.depth = req->key.key.ipv4.depth,
	};

	if (req->key.type != PIPELINE_ROUTING_ROUTE_IPV4) {
		rsp->status = -1;
		return rsp;
	}

	rsp->status = rte_pipeline_table_entry_delete(p->p,
		p->table_id[0],
		&key,
		&rsp->key_found,
		NULL);

	return rsp;
}

void *
pipeline_routing_msg_req_route_add_default_handler(struct pipeline *p,
	void *msg)
{
	struct pipeline_routing_route_add_default_msg_req *req = msg;
	struct pipeline_routing_route_add_default_msg_rsp *rsp = msg;

	struct routing_table_entry default_entry = {
		.head = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[req->port_id]},
		},

		.flags = 0,
		.port_id = 0,
		.ip = 0,
	};

	rsp->status = rte_pipeline_table_default_entry_add(p->p,
		p->table_id[0],
		(struct rte_pipeline_table_entry *) &default_entry,
		(struct rte_pipeline_table_entry **) &rsp->entry_ptr);

	return rsp;
}

void *
pipeline_routing_msg_req_route_del_default_handler(struct pipeline *p,
	void *msg)
{
	struct pipeline_routing_route_delete_default_msg_rsp *rsp = msg;

	rsp->status = rte_pipeline_table_default_entry_delete(p->p,
		p->table_id[0],
		NULL);

	return rsp;
}

void *
pipeline_routing_msg_req_arp_add_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing_arp_add_msg_req *req = msg;
	struct pipeline_routing_arp_add_msg_rsp *rsp = msg;

	struct pipeline_routing_arp_key_ipv4 key = {
		.port_id = req->key.key.ipv4.port_id,
		.ip = rte_bswap32(req->key.key.ipv4.ip),
	};

	struct arp_table_entry entry = {
		.head = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[req->key.key.ipv4.port_id]},
		},

		.macaddr = 0, /* set below */
	};

	if (req->key.type != PIPELINE_ROUTING_ARP_IPV4) {
		rsp->status = -1;
		return rsp;
	}

	*((struct ether_addr *) &entry.macaddr) = req->macaddr;

	rsp->status = rte_pipeline_table_entry_add(p->p,
		p->table_id[1],
		&key,
		(struct rte_pipeline_table_entry *) &entry,
		&rsp->key_found,
		(struct rte_pipeline_table_entry **) &rsp->entry_ptr);

	return rsp;
}

void *
pipeline_routing_msg_req_arp_del_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing_arp_delete_msg_req *req = msg;
	struct pipeline_routing_arp_delete_msg_rsp *rsp = msg;

	struct pipeline_routing_arp_key_ipv4 key = {
		.port_id = req->key.key.ipv4.port_id,
		.ip = rte_bswap32(req->key.key.ipv4.ip),
	};

	if (req->key.type != PIPELINE_ROUTING_ARP_IPV4) {
		rsp->status = -1;
		return rsp;
	}

	rsp->status = rte_pipeline_table_entry_delete(p->p,
		p->table_id[1],
		&key,
		&rsp->key_found,
		NULL);

	return rsp;
}

void *
pipeline_routing_msg_req_arp_add_default_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing_arp_add_default_msg_req *req = msg;
	struct pipeline_routing_arp_add_default_msg_rsp *rsp = msg;

	struct arp_table_entry default_entry = {
		.head = {
			.action = RTE_PIPELINE_ACTION_PORT,
			{.port_id = p->port_out_id[req->port_id]},
		},

		.macaddr = 0,
	};

	rsp->status = rte_pipeline_table_default_entry_add(p->p,
		p->table_id[1],
		(struct rte_pipeline_table_entry *) &default_entry,
		(struct rte_pipeline_table_entry **) &rsp->entry_ptr);

	return rsp;
}

void *
pipeline_routing_msg_req_arp_del_default_handler(struct pipeline *p, void *msg)
{
	struct pipeline_routing_arp_delete_default_msg_rsp *rsp = msg;

	rsp->status = rte_pipeline_table_default_entry_delete(p->p,
		p->table_id[1],
		NULL);

	return rsp;
}

struct pipeline_be_ops pipeline_routing_be_ops = {
	.f_init = pipeline_routing_init,
	.f_free = pipeline_routing_free,
	.f_run = NULL,
	.f_timer = pipeline_routing_timer,
	.f_track = pipeline_routing_track,
};
