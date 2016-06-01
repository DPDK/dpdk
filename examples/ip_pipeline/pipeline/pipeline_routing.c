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

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include "app.h"
#include "pipeline_common_fe.h"
#include "pipeline_routing.h"
#include "parser.h"

struct app_pipeline_routing_route {
	struct pipeline_routing_route_key key;
	struct pipeline_routing_route_data data;
	void *entry_ptr;

	TAILQ_ENTRY(app_pipeline_routing_route) node;
};

struct app_pipeline_routing_arp_entry {
	struct pipeline_routing_arp_key key;
	struct ether_addr macaddr;
	void *entry_ptr;

	TAILQ_ENTRY(app_pipeline_routing_arp_entry) node;
};

struct pipeline_routing {
	/* Parameters */
	struct app_params *app;
	uint32_t pipeline_id;
	uint32_t n_ports_in;
	uint32_t n_ports_out;
	struct pipeline_routing_params rp;

	/* Links */
	uint32_t link_id[PIPELINE_MAX_PORT_OUT];

	/* Routes */
	TAILQ_HEAD(, app_pipeline_routing_route) routes;
	uint32_t n_routes;

	uint32_t default_route_present;
	uint32_t default_route_port_id;
	void *default_route_entry_ptr;

	/* ARP entries */
	TAILQ_HEAD(, app_pipeline_routing_arp_entry) arp_entries;
	uint32_t n_arp_entries;

	uint32_t default_arp_entry_present;
	uint32_t default_arp_entry_port_id;
	void *default_arp_entry_ptr;
};

static int
app_pipeline_routing_find_link(struct pipeline_routing *p,
	uint32_t link_id,
	uint32_t *port_id)
{
	uint32_t i;

	for (i = 0; i < p->n_ports_out; i++)
		if (p->link_id[i] == link_id) {
			*port_id = i;
			return 0;
		}

	return -1;
}

static void
app_pipeline_routing_link_op(__rte_unused struct app_params *app,
	uint32_t link_id,
	uint32_t up,
	void *arg)
{
	struct pipeline_routing_route_key key0, key1;
	struct pipeline_routing *p = arg;
	struct app_link_params *lp;
	uint32_t port_id, netmask;
	int status;

	if (app == NULL)
		return;

	APP_PARAM_FIND_BY_ID(app->link_params, "LINK", link_id, lp);
	if (lp == NULL)
		return;

	status = app_pipeline_routing_find_link(p,
		link_id,
		&port_id);
	if (status)
		return;

	netmask = (~0U) << (32 - lp->depth);

	/* Local network (directly attached network) */
	key0.type = PIPELINE_ROUTING_ROUTE_IPV4;
	key0.key.ipv4.ip = lp->ip & netmask;
	key0.key.ipv4.depth = lp->depth;

	/* Local termination */
	key1.type = PIPELINE_ROUTING_ROUTE_IPV4;
	key1.key.ipv4.ip = lp->ip;
	key1.key.ipv4.depth = 32;

	if (up) {
		struct pipeline_routing_route_data data0, data1;

		/* Local network (directly attached network) */
		memset(&data0, 0, sizeof(data0));
		data0.flags = PIPELINE_ROUTING_ROUTE_LOCAL |
			PIPELINE_ROUTING_ROUTE_ARP;
		if (p->rp.encap == PIPELINE_ROUTING_ENCAP_ETHERNET_QINQ)
			data0.flags |= PIPELINE_ROUTING_ROUTE_QINQ;
		if (p->rp.encap == PIPELINE_ROUTING_ENCAP_ETHERNET_MPLS) {
			data0.flags |= PIPELINE_ROUTING_ROUTE_MPLS;
			data0.l2.mpls.n_labels = 1;
		}
		data0.port_id = port_id;

		if (p->rp.n_arp_entries)
			app_pipeline_routing_add_route(app,
				p->pipeline_id,
				&key0,
				&data0);

		/* Local termination */
		memset(&data1, 0, sizeof(data1));
		data1.flags = PIPELINE_ROUTING_ROUTE_LOCAL |
			PIPELINE_ROUTING_ROUTE_ARP;
		if (p->rp.encap == PIPELINE_ROUTING_ENCAP_ETHERNET_QINQ)
			data1.flags |= PIPELINE_ROUTING_ROUTE_QINQ;
		if (p->rp.encap == PIPELINE_ROUTING_ENCAP_ETHERNET_MPLS) {
			data1.flags |= PIPELINE_ROUTING_ROUTE_MPLS;
			data1.l2.mpls.n_labels = 1;
		}
		data1.port_id = p->rp.port_local_dest;

		app_pipeline_routing_add_route(app,
			p->pipeline_id,
			&key1,
			&data1);
	} else {
		/* Local network (directly attached network) */
		if (p->rp.n_arp_entries)
			app_pipeline_routing_delete_route(app,
				p->pipeline_id,
				&key0);

		/* Local termination */
		app_pipeline_routing_delete_route(app,
			p->pipeline_id,
			&key1);
	}
}

static int
app_pipeline_routing_set_link_op(
	struct app_params *app,
	struct pipeline_routing *p)
{
	uint32_t port_id;

	for (port_id = 0; port_id < p->n_ports_out; port_id++) {
		struct app_link_params *link;
		uint32_t link_id;
		int status;

		link = app_pipeline_track_pktq_out_to_link(app,
			p->pipeline_id,
			port_id);
		if (link == NULL)
			continue;

		link_id = link - app->link_params;
		p->link_id[port_id] = link_id;

		status = app_link_set_op(app,
			link_id,
			p->pipeline_id,
			app_pipeline_routing_link_op,
			(void *) p);
		if (status)
			return status;
	}

	return 0;
}

static void *
app_pipeline_routing_init(struct pipeline_params *params,
	void *arg)
{
	struct app_params *app = (struct app_params *) arg;
	struct pipeline_routing *p;
	uint32_t pipeline_id, size;
	int status;

	/* Check input arguments */
	if ((params == NULL) ||
		(params->n_ports_in == 0) ||
		(params->n_ports_out == 0))
		return NULL;

	APP_PARAM_GET_ID(params, "PIPELINE", pipeline_id);

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct pipeline_routing));
	p = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	if (p == NULL)
		return NULL;

	/* Initialization */
	p->app = app;
	p->pipeline_id = pipeline_id;
	p->n_ports_in = params->n_ports_in;
	p->n_ports_out = params->n_ports_out;

	status = pipeline_routing_parse_args(&p->rp, params);
	if (status) {
		rte_free(p);
		return NULL;
	}
	TAILQ_INIT(&p->routes);
	p->n_routes = 0;

	TAILQ_INIT(&p->arp_entries);
	p->n_arp_entries = 0;

	app_pipeline_routing_set_link_op(app, p);

	return p;
}

static int
app_pipeline_routing_post_init(void *pipeline)
{
	struct pipeline_routing *p = pipeline;

	/* Check input arguments */
	if (p == NULL)
		return -1;

	return app_pipeline_routing_set_macaddr(p->app, p->pipeline_id);
}

static int
app_pipeline_routing_free(void *pipeline)
{
	struct pipeline_routing *p = pipeline;

	/* Check input arguments */
	if (p == NULL)
		return -1;

	/* Free resources */
	while (!TAILQ_EMPTY(&p->routes)) {
		struct app_pipeline_routing_route *route;

		route = TAILQ_FIRST(&p->routes);
		TAILQ_REMOVE(&p->routes, route, node);
		rte_free(route);
	}

	while (!TAILQ_EMPTY(&p->arp_entries)) {
		struct app_pipeline_routing_arp_entry *arp_entry;

		arp_entry = TAILQ_FIRST(&p->arp_entries);
		TAILQ_REMOVE(&p->arp_entries, arp_entry, node);
		rte_free(arp_entry);
	}

	rte_free(p);
	return 0;
}

static struct app_pipeline_routing_route *
app_pipeline_routing_find_route(struct pipeline_routing *p,
		const struct pipeline_routing_route_key *key)
{
	struct app_pipeline_routing_route *it, *found;

	found = NULL;
	TAILQ_FOREACH(it, &p->routes, node) {
		if ((key->type == it->key.type) &&
			(key->key.ipv4.ip == it->key.key.ipv4.ip) &&
			(key->key.ipv4.depth == it->key.key.ipv4.depth)) {
			found = it;
			break;
		}
	}

	return found;
}

static struct app_pipeline_routing_arp_entry *
app_pipeline_routing_find_arp_entry(struct pipeline_routing *p,
		const struct pipeline_routing_arp_key *key)
{
	struct app_pipeline_routing_arp_entry *it, *found;

	found = NULL;
	TAILQ_FOREACH(it, &p->arp_entries, node) {
		if ((key->type == it->key.type) &&
			(key->key.ipv4.port_id == it->key.key.ipv4.port_id) &&
			(key->key.ipv4.ip == it->key.key.ipv4.ip)) {
			found = it;
			break;
		}
	}

	return found;
}

static void
print_route(const struct app_pipeline_routing_route *route)
{
	if (route->key.type == PIPELINE_ROUTING_ROUTE_IPV4) {
		const struct pipeline_routing_route_key_ipv4 *key =
				&route->key.key.ipv4;

		printf("IP Prefix = %" PRIu32 ".%" PRIu32
			".%" PRIu32 ".%" PRIu32 "/%" PRIu32
			" => (Port = %" PRIu32,

			(key->ip >> 24) & 0xFF,
			(key->ip >> 16) & 0xFF,
			(key->ip >> 8) & 0xFF,
			key->ip & 0xFF,

			key->depth,
			route->data.port_id);

		if (route->data.flags & PIPELINE_ROUTING_ROUTE_LOCAL)
			printf(", Local");
		else if (route->data.flags & PIPELINE_ROUTING_ROUTE_ARP)
			printf(
				", Next Hop IP = %" PRIu32 ".%" PRIu32
				".%" PRIu32 ".%" PRIu32,

				(route->data.ethernet.ip >> 24) & 0xFF,
				(route->data.ethernet.ip >> 16) & 0xFF,
				(route->data.ethernet.ip >> 8) & 0xFF,
				route->data.ethernet.ip & 0xFF);
		else
			printf(
				", Next Hop HWaddress = %02" PRIx32
				":%02" PRIx32 ":%02" PRIx32
				":%02" PRIx32 ":%02" PRIx32
				":%02" PRIx32,

				route->data.ethernet.macaddr.addr_bytes[0],
				route->data.ethernet.macaddr.addr_bytes[1],
				route->data.ethernet.macaddr.addr_bytes[2],
				route->data.ethernet.macaddr.addr_bytes[3],
				route->data.ethernet.macaddr.addr_bytes[4],
				route->data.ethernet.macaddr.addr_bytes[5]);

		if (route->data.flags & PIPELINE_ROUTING_ROUTE_QINQ)
			printf(", QinQ SVLAN = %" PRIu32 " CVLAN = %" PRIu32,
				route->data.l2.qinq.svlan,
				route->data.l2.qinq.cvlan);

		if (route->data.flags & PIPELINE_ROUTING_ROUTE_MPLS) {
			uint32_t i;

			printf(", MPLS labels");
			for (i = 0; i < route->data.l2.mpls.n_labels; i++)
				printf(" %" PRIu32,
					route->data.l2.mpls.labels[i]);
		}

		printf(")\n");
	}
}

static void
print_arp_entry(const struct app_pipeline_routing_arp_entry *entry)
{
	printf("(Port = %" PRIu32 ", IP = %" PRIu32 ".%" PRIu32
		".%" PRIu32 ".%" PRIu32
		") => HWaddress = %02" PRIx32 ":%02" PRIx32 ":%02" PRIx32
		":%02" PRIx32 ":%02" PRIx32 ":%02" PRIx32 "\n",

		entry->key.key.ipv4.port_id,
		(entry->key.key.ipv4.ip >> 24) & 0xFF,
		(entry->key.key.ipv4.ip >> 16) & 0xFF,
		(entry->key.key.ipv4.ip >> 8) & 0xFF,
		entry->key.key.ipv4.ip & 0xFF,

		entry->macaddr.addr_bytes[0],
		entry->macaddr.addr_bytes[1],
		entry->macaddr.addr_bytes[2],
		entry->macaddr.addr_bytes[3],
		entry->macaddr.addr_bytes[4],
		entry->macaddr.addr_bytes[5]);
}

static int
app_pipeline_routing_route_ls(struct app_params *app, uint32_t pipeline_id)
{
	struct pipeline_routing *p;
	struct app_pipeline_routing_route *it;

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_routing);
	if (p == NULL)
		return -EINVAL;

	TAILQ_FOREACH(it, &p->routes, node)
		print_route(it);

	if (p->default_route_present)
		printf("Default route: port %" PRIu32 " (entry ptr = %p)\n",
				p->default_route_port_id,
				p->default_route_entry_ptr);
	else
		printf("Default: DROP\n");

	return 0;
}

int
app_pipeline_routing_add_route(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_routing_route_key *key,
	struct pipeline_routing_route_data *data)
{
	struct pipeline_routing *p;

	struct pipeline_routing_route_add_msg_req *req;
	struct pipeline_routing_route_add_msg_rsp *rsp;

	struct app_pipeline_routing_route *entry;

	int new_entry;

	/* Check input arguments */
	if ((app == NULL) ||
		(key == NULL) ||
		(data == NULL))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_routing);
	if (p == NULL)
		return -1;

	switch (key->type) {
	case PIPELINE_ROUTING_ROUTE_IPV4:
	{
		uint32_t depth = key->key.ipv4.depth;
		uint32_t netmask;

		/* key */
		if ((depth == 0) || (depth > 32))
			return -1;

		netmask = (~0U) << (32 - depth);
		key->key.ipv4.ip &= netmask;

		/* data */
		if (data->port_id >= p->n_ports_out)
			return -1;
	}
	break;

	default:
		return -1;
	}

	/* Find existing rule or allocate new rule */
	entry = app_pipeline_routing_find_route(p, key);
	new_entry = (entry == NULL);
	if (entry == NULL) {
		entry = rte_malloc(NULL, sizeof(*entry), RTE_CACHE_LINE_SIZE);

		if (entry == NULL)
			return -1;
	}

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL) {
		if (new_entry)
			rte_free(entry);
		return -1;
	}

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_ROUTING_MSG_REQ_ROUTE_ADD;
	memcpy(&req->key, key, sizeof(*key));
	memcpy(&req->data, data, sizeof(*data));

	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL) {
		if (new_entry)
			rte_free(entry);
		return -1;
	}

	/* Read response and write entry */
	if (rsp->status ||
		(rsp->entry_ptr == NULL) ||
		((new_entry == 0) && (rsp->key_found == 0)) ||
		((new_entry == 1) && (rsp->key_found == 1))) {
		app_msg_free(app, rsp);
		if (new_entry)
			rte_free(entry);
		return -1;
	}

	memcpy(&entry->key, key, sizeof(*key));
	memcpy(&entry->data, data, sizeof(*data));
	entry->entry_ptr = rsp->entry_ptr;

	/* Commit entry */
	if (new_entry) {
		TAILQ_INSERT_TAIL(&p->routes, entry, node);
		p->n_routes++;
	}

	/* Message buffer free */
	app_msg_free(app, rsp);
	return 0;
}

int
app_pipeline_routing_delete_route(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_routing_route_key *key)
{
	struct pipeline_routing *p;

	struct pipeline_routing_route_delete_msg_req *req;
	struct pipeline_routing_route_delete_msg_rsp *rsp;

	struct app_pipeline_routing_route *entry;

	/* Check input arguments */
	if ((app == NULL) ||
		(key == NULL))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_routing);
	if (p == NULL)
		return -1;

	switch (key->type) {
	case PIPELINE_ROUTING_ROUTE_IPV4:
	{
		uint32_t depth = key->key.ipv4.depth;
		uint32_t netmask;

		/* key */
		if ((depth == 0) || (depth > 32))
			return -1;

		netmask = (~0U) << (32 - depth);
		key->key.ipv4.ip &= netmask;
	}
	break;

	default:
		return -1;
	}

	/* Find rule */
	entry = app_pipeline_routing_find_route(p, key);
	if (entry == NULL)
		return 0;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_ROUTING_MSG_REQ_ROUTE_DEL;
	memcpy(&req->key, key, sizeof(*key));

	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response */
	if (rsp->status || !rsp->key_found) {
		app_msg_free(app, rsp);
		return -1;
	}

	/* Remove route */
	TAILQ_REMOVE(&p->routes, entry, node);
	p->n_routes--;
	rte_free(entry);

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_routing_add_default_route(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id)
{
	struct pipeline_routing *p;

	struct pipeline_routing_route_add_default_msg_req *req;
	struct pipeline_routing_route_add_default_msg_rsp *rsp;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_routing);
	if (p == NULL)
		return -1;

	if (port_id >= p->n_ports_out)
		return -1;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_ROUTING_MSG_REQ_ROUTE_ADD_DEFAULT;
	req->port_id = port_id;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response and write route */
	if (rsp->status || (rsp->entry_ptr == NULL)) {
		app_msg_free(app, rsp);
		return -1;
	}

	p->default_route_port_id = port_id;
	p->default_route_entry_ptr = rsp->entry_ptr;

	/* Commit route */
	p->default_route_present = 1;

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_routing_delete_default_route(struct app_params *app,
	uint32_t pipeline_id)
{
	struct pipeline_routing *p;

	struct pipeline_routing_arp_delete_default_msg_req *req;
	struct pipeline_routing_arp_delete_default_msg_rsp *rsp;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_routing);
	if (p == NULL)
		return -1;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_ROUTING_MSG_REQ_ROUTE_DEL_DEFAULT;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response and write route */
	if (rsp->status) {
		app_msg_free(app, rsp);
		return -1;
	}

	/* Commit route */
	p->default_route_present = 0;

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

static int
app_pipeline_routing_arp_ls(struct app_params *app, uint32_t pipeline_id)
{
	struct pipeline_routing *p;
	struct app_pipeline_routing_arp_entry *it;

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_routing);
	if (p == NULL)
		return -EINVAL;

	TAILQ_FOREACH(it, &p->arp_entries, node)
		print_arp_entry(it);

	if (p->default_arp_entry_present)
		printf("Default entry: port %" PRIu32 " (entry ptr = %p)\n",
				p->default_arp_entry_port_id,
				p->default_arp_entry_ptr);
	else
		printf("Default: DROP\n");

	return 0;
}

int
app_pipeline_routing_add_arp_entry(struct app_params *app, uint32_t pipeline_id,
		struct pipeline_routing_arp_key *key,
		struct ether_addr *macaddr)
{
	struct pipeline_routing *p;

	struct pipeline_routing_arp_add_msg_req *req;
	struct pipeline_routing_arp_add_msg_rsp *rsp;

	struct app_pipeline_routing_arp_entry *entry;

	int new_entry;

	/* Check input arguments */
	if ((app == NULL) ||
		(key == NULL) ||
		(macaddr == NULL))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_routing);
	if (p == NULL)
		return -1;

	switch (key->type) {
	case PIPELINE_ROUTING_ARP_IPV4:
	{
		uint32_t port_id = key->key.ipv4.port_id;

		/* key */
		if (port_id >= p->n_ports_out)
			return -1;
	}
	break;

	default:
		return -1;
	}

	/* Find existing entry or allocate new */
	entry = app_pipeline_routing_find_arp_entry(p, key);
	new_entry = (entry == NULL);
	if (entry == NULL) {
		entry = rte_malloc(NULL, sizeof(*entry), RTE_CACHE_LINE_SIZE);

		if (entry == NULL)
			return -1;
	}

	/* Message buffer allocation */
	req = app_msg_alloc(app);
	if (req == NULL) {
		if (new_entry)
			rte_free(entry);
		return -1;
	}

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_ROUTING_MSG_REQ_ARP_ADD;
	memcpy(&req->key, key, sizeof(*key));
	ether_addr_copy(macaddr, &req->macaddr);

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL) {
		if (new_entry)
			rte_free(entry);
		return -1;
	}

	/* Read response and write entry */
	if (rsp->status ||
		(rsp->entry_ptr == NULL) ||
		((new_entry == 0) && (rsp->key_found == 0)) ||
		((new_entry == 1) && (rsp->key_found == 1))) {
		app_msg_free(app, rsp);
		if (new_entry)
			rte_free(entry);
		return -1;
	}

	memcpy(&entry->key, key, sizeof(*key));
	ether_addr_copy(macaddr, &entry->macaddr);
	entry->entry_ptr = rsp->entry_ptr;

	/* Commit entry */
	if (new_entry) {
		TAILQ_INSERT_TAIL(&p->arp_entries, entry, node);
		p->n_arp_entries++;
	}

	/* Message buffer free */
	app_msg_free(app, rsp);
	return 0;
}

int
app_pipeline_routing_delete_arp_entry(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_routing_arp_key *key)
{
	struct pipeline_routing *p;

	struct pipeline_routing_arp_delete_msg_req *req;
	struct pipeline_routing_arp_delete_msg_rsp *rsp;

	struct app_pipeline_routing_arp_entry *entry;

	/* Check input arguments */
	if ((app == NULL) ||
		(key == NULL))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_routing);
	if (p == NULL)
		return -EINVAL;

	switch (key->type) {
	case PIPELINE_ROUTING_ARP_IPV4:
	{
		uint32_t port_id = key->key.ipv4.port_id;

		/* key */
		if (port_id >= p->n_ports_out)
			return -1;
	}
	break;

	default:
		return -1;
	}

	/* Find rule */
	entry = app_pipeline_routing_find_arp_entry(p, key);
	if (entry == NULL)
		return 0;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_ROUTING_MSG_REQ_ARP_DEL;
	memcpy(&req->key, key, sizeof(*key));

	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response */
	if (rsp->status || !rsp->key_found) {
		app_msg_free(app, rsp);
		return -1;
	}

	/* Remove entry */
	TAILQ_REMOVE(&p->arp_entries, entry, node);
	p->n_arp_entries--;
	rte_free(entry);

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_routing_add_default_arp_entry(struct app_params *app,
		uint32_t pipeline_id,
		uint32_t port_id)
{
	struct pipeline_routing *p;

	struct pipeline_routing_arp_add_default_msg_req *req;
	struct pipeline_routing_arp_add_default_msg_rsp *rsp;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_routing);
	if (p == NULL)
		return -1;

	if (port_id >= p->n_ports_out)
		return -1;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_ROUTING_MSG_REQ_ARP_ADD_DEFAULT;
	req->port_id = port_id;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response and write entry */
	if (rsp->status || rsp->entry_ptr == NULL) {
		app_msg_free(app, rsp);
		return -1;
	}

	p->default_arp_entry_port_id = port_id;
	p->default_arp_entry_ptr = rsp->entry_ptr;

	/* Commit entry */
	p->default_arp_entry_present = 1;

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_routing_delete_default_arp_entry(struct app_params *app,
	uint32_t pipeline_id)
{
	struct pipeline_routing *p;

	struct pipeline_routing_arp_delete_default_msg_req *req;
	struct pipeline_routing_arp_delete_default_msg_rsp *rsp;

	/* Check input arguments */
	if (app == NULL)
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id, &pipeline_routing);
	if (p == NULL)
		return -EINVAL;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -ENOMEM;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_ROUTING_MSG_REQ_ARP_DEL_DEFAULT;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -ETIMEDOUT;

	/* Read response and write entry */
	if (rsp->status) {
		app_msg_free(app, rsp);
		return rsp->status;
	}

	/* Commit entry */
	p->default_arp_entry_present = 0;

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_routing_set_macaddr(struct app_params *app,
	uint32_t pipeline_id)
{
	struct app_pipeline_params *p;
	struct pipeline_routing_set_macaddr_msg_req *req;
	struct pipeline_routing_set_macaddr_msg_rsp *rsp;
	uint32_t port_id;

	/* Check input arguments */
	if (app == NULL)
		return -EINVAL;

	APP_PARAM_FIND_BY_ID(app->pipeline_params, "PIPELINE", pipeline_id, p);
	if (p == NULL)
		return -EINVAL;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -ENOMEM;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_ROUTING_MSG_REQ_SET_MACADDR;

	memset(req->macaddr, 0, sizeof(req->macaddr));
	for (port_id = 0; port_id < p->n_pktq_out; port_id++) {
		struct app_link_params *link;

		link = app_pipeline_track_pktq_out_to_link(app,
			pipeline_id,
			port_id);
		if (link)
			req->macaddr[port_id] = link->mac_addr;
	}

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -ETIMEDOUT;

	/* Read response and write entry */
	if (rsp->status) {
		app_msg_free(app, rsp);
		return rsp->status;
	}

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

/*
 * route
 *
 * route add (ARP = ON/OFF, MPLS = ON/OFF, QINQ = ON/OFF):
 *    p <pipelineid> route add <ipaddr> <depth> port <portid> ether <nhmacaddr>
 *    p <pipelineid> route add <ipaddr> <depth> port <portid> ether <nhipaddr>
 *    p <pipelineid> route add <ipaddr> <depth> port <portid> ether <nhmacaddr> qinq <svlan> <cvlan>
 *    p <pipelineid> route add <ipaddr> <depth> port <portid> ether <nhipaddr> qinq <svlan> <cvlan>
 *    p <pipelineid> route add <ipaddr> <depth> port <portid> ether <nhmacaddr> mpls <mpls labels>
 *    p <pipelineid> route add <ipaddr> <depth> port <portid> ether <nhipaddr> mpls <mpls labels>
 *
 * route add default:
 *    p <pipelineid> route add default <portid>
 *
 * route del:
 *    p <pipelineid> route del <ipaddr> <depth>
 *
 * route del default:
 *    p <pipelineid> route del default
 *
 * route ls:
 *    p <pipelineid> route ls
 */

struct cmd_route_result {
	cmdline_fixed_string_t p_string;
	uint32_t p;
	cmdline_fixed_string_t route_string;
	cmdline_multi_string_t multi_string;
};

static void
cmd_route_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_route_result *params = parsed_result;
	struct app_params *app = data;

	char *tokens[16];
	uint32_t n_tokens = RTE_DIM(tokens);
	int status;

	status = parse_tokenize_string(params->multi_string, tokens, &n_tokens);
	if (status != 0) {
		printf(CMD_MSG_TOO_MANY_ARGS, "route");
		return;
	}

	/* route add */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "add") == 0) &&
		strcmp(tokens[1], "default")) {
		struct pipeline_routing_route_key key;
		struct pipeline_routing_route_data route_data;
		struct in_addr ipv4, nh_ipv4;
		struct ether_addr mac_addr;
		uint32_t depth, port_id, svlan, cvlan, i;
		uint32_t mpls_labels[PIPELINE_ROUTING_MPLS_LABELS_MAX];
		uint32_t n_labels = RTE_DIM(mpls_labels);

		memset(&key, 0, sizeof(key));
		memset(&route_data, 0, sizeof(route_data));

		if (n_tokens < 7) {
			printf(CMD_MSG_NOT_ENOUGH_ARGS, "route add");
			return;
		}

		if (parse_ipv4_addr(tokens[1], &ipv4)) {
			printf(CMD_MSG_INVALID_ARG, "ipaddr");
			return;
		}

		if (parser_read_uint32(&depth, tokens[2])) {
			printf(CMD_MSG_INVALID_ARG, "depth");
			return;
		}

		if (strcmp(tokens[3], "port")) {
			printf(CMD_MSG_ARG_NOT_FOUND, "port");
			return;
		}

		if (parser_read_uint32(&port_id, tokens[4])) {
			printf(CMD_MSG_INVALID_ARG, "portid");
			return;
		}

		if (strcmp(tokens[5], "ether")) {
			printf(CMD_MSG_ARG_NOT_FOUND, "ether");
			return;
		}

		if (parse_mac_addr(tokens[6], &mac_addr)) {
			if (parse_ipv4_addr(tokens[6], &nh_ipv4)) {
				printf(CMD_MSG_INVALID_ARG, "nhmacaddr or nhipaddr");
				return;
			}

			route_data.flags |= PIPELINE_ROUTING_ROUTE_ARP;
		}

		if (n_tokens > 7) {
			if (strcmp(tokens[7], "mpls") == 0) {
				if (n_tokens != 9) {
					printf(CMD_MSG_MISMATCH_ARGS, "route add mpls");
					return;
				}

				if (parse_mpls_labels(tokens[8], mpls_labels, &n_labels)) {
					printf(CMD_MSG_INVALID_ARG, "mpls labels");
					return;
				}

				route_data.flags |= PIPELINE_ROUTING_ROUTE_MPLS;
			} else if (strcmp(tokens[7], "qinq") == 0) {
				if (n_tokens != 10) {
					printf(CMD_MSG_MISMATCH_ARGS, "route add qinq");
					return;
				}

				if (parser_read_uint32(&svlan, tokens[8])) {
					printf(CMD_MSG_INVALID_ARG, "svlan");
					return;
				}
				if (parser_read_uint32(&cvlan, tokens[9])) {
					printf(CMD_MSG_INVALID_ARG, "cvlan");
					return;
				}

				route_data.flags |= PIPELINE_ROUTING_ROUTE_QINQ;
			} else {
				printf(CMD_MSG_ARG_NOT_FOUND, "mpls or qinq");
				return;
			}
		}

		switch (route_data.flags) {
		case 0:
			route_data.port_id = port_id;
			route_data.ethernet.macaddr = mac_addr;
			break;

		case PIPELINE_ROUTING_ROUTE_ARP:
			route_data.port_id = port_id;
			route_data.ethernet.ip = rte_be_to_cpu_32(nh_ipv4.s_addr);
			break;

		case PIPELINE_ROUTING_ROUTE_MPLS:
			route_data.port_id = port_id;
			route_data.ethernet.macaddr = mac_addr;
			for (i = 0; i < n_labels; i++)
				route_data.l2.mpls.labels[i] = mpls_labels[i];
			route_data.l2.mpls.n_labels = n_labels;
			break;

		case PIPELINE_ROUTING_ROUTE_MPLS | PIPELINE_ROUTING_ROUTE_ARP:
			route_data.port_id = port_id;
			route_data.ethernet.ip = rte_be_to_cpu_32(nh_ipv4.s_addr);
			for (i = 0; i < n_labels; i++)
				route_data.l2.mpls.labels[i] = mpls_labels[i];
			route_data.l2.mpls.n_labels = n_labels;
			break;

		case PIPELINE_ROUTING_ROUTE_QINQ:
			route_data.port_id = port_id;
			route_data.ethernet.macaddr = mac_addr;
			route_data.l2.qinq.svlan = svlan;
			route_data.l2.qinq.cvlan = cvlan;
			break;

		case PIPELINE_ROUTING_ROUTE_QINQ | PIPELINE_ROUTING_ROUTE_ARP:
		default:
			route_data.port_id = port_id;
			route_data.ethernet.ip = rte_be_to_cpu_32(nh_ipv4.s_addr);
			route_data.l2.qinq.svlan = svlan;
			route_data.l2.qinq.cvlan = cvlan;
			break;
		}

		key.type = PIPELINE_ROUTING_ROUTE_IPV4;
		key.key.ipv4.ip = rte_be_to_cpu_32(ipv4.s_addr);
		key.key.ipv4.depth = depth;

		status = app_pipeline_routing_add_route(app,
			params->p,
			&key,
			&route_data);
		if (status != 0)
			printf(CMD_MSG_FAIL, "route add");

		return;
	} /* route add */

	/* route add default */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "add") == 0) &&
		(strcmp(tokens[1], "default") == 0)) {
		uint32_t port_id;

		if (n_tokens != 3) {
			printf(CMD_MSG_MISMATCH_ARGS, "route add default");
			return;
		}

		if (parser_read_uint32(&port_id, tokens[2])) {
			printf(CMD_MSG_INVALID_ARG, "portid");
			return;
		}

		status = app_pipeline_routing_add_default_route(app,
			params->p,
			port_id);
		if (status != 0)
			printf(CMD_MSG_FAIL, "route add default");

		return;
	} /* route add default */

	/* route del*/
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "del") == 0) &&
		strcmp(tokens[1], "default")) {
		struct pipeline_routing_route_key key;
		struct in_addr ipv4;
		uint32_t depth;

		memset(&key, 0, sizeof(key));

		if (n_tokens != 3) {
			printf(CMD_MSG_MISMATCH_ARGS, "route del");
			return;
		}

		if (parse_ipv4_addr(tokens[1], &ipv4)) {
			printf(CMD_MSG_INVALID_ARG, "ipaddr");
			return;
		}

		if (parser_read_uint32(&depth, tokens[2])) {
			printf(CMD_MSG_INVALID_ARG, "depth");
			return;
		}

		key.type = PIPELINE_ROUTING_ROUTE_IPV4;
		key.key.ipv4.ip = rte_be_to_cpu_32(ipv4.s_addr);
		key.key.ipv4.depth = depth;

		status = app_pipeline_routing_delete_route(app, params->p, &key);
		if (status != 0)
			printf(CMD_MSG_FAIL, "route del");

		return;
	} /* route del */

	/* route del default */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "del") == 0) &&
		(strcmp(tokens[1], "default") == 0)) {
		if (n_tokens != 2) {
			printf(CMD_MSG_MISMATCH_ARGS, "route del default");
			return;
		}

		status = app_pipeline_routing_delete_default_route(app,
			params->p);
		if (status != 0)
			printf(CMD_MSG_FAIL, "route del default");

		return;
	} /* route del default */

	/* route ls */
	if ((n_tokens >= 1) && (strcmp(tokens[0], "ls") == 0)) {
		if (n_tokens != 1) {
			printf(CMD_MSG_MISMATCH_ARGS, "route ls");
			return;
		}

		status = app_pipeline_routing_route_ls(app, params->p);
		if (status != 0)
			printf(CMD_MSG_FAIL, "route ls");

		return;
	} /* route ls */

	printf(CMD_MSG_MISMATCH_ARGS, "route");
}

static cmdline_parse_token_string_t cmd_route_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_result, p_string, "p");

static cmdline_parse_token_num_t cmd_route_p =
	TOKEN_NUM_INITIALIZER(struct cmd_route_result, p, UINT32);

static cmdline_parse_token_string_t cmd_route_route_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_result, route_string, "route");

static cmdline_parse_token_string_t cmd_route_multi_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_result, multi_string,
	TOKEN_STRING_MULTI);

static cmdline_parse_inst_t cmd_route = {
	.f = cmd_route_parsed,
	.data = NULL,
	.help_str = "route add / add default / del / del default / ls",
	.tokens = {
		(void *)&cmd_route_p_string,
		(void *)&cmd_route_p,
		(void *)&cmd_route_route_string,
		(void *)&cmd_route_multi_string,
		NULL,
	},
};

/*
 * arp
 *
 * arp add:
 *    p <pipelineid> arp add <portid> <ipaddr> <macaddr>
 *
 * arp add default:
 *    p <pipelineid> arp add default <portid>
 *
 * arp del:
 *    p <pipelineid> arp del <portid> <ipaddr>
 *
 * arp del default:
 *    p <pipelineid> arp del default
 *
 * arp ls:
 *    p <pipelineid> arp ls
 */

struct cmd_arp_result {
	cmdline_fixed_string_t p_string;
	uint32_t p;
	cmdline_fixed_string_t arp_string;
	cmdline_multi_string_t multi_string;
};

static void
cmd_arp_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_arp_result *params = parsed_result;
	struct app_params *app = data;

	char *tokens[16];
	uint32_t n_tokens = RTE_DIM(tokens);
	int status;

	status = parse_tokenize_string(params->multi_string, tokens, &n_tokens);
	if (status != 0) {
		printf(CMD_MSG_TOO_MANY_ARGS, "arp");
		return;
	}

	/* arp add */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "add") == 0) &&
		strcmp(tokens[1], "default")) {
		struct pipeline_routing_arp_key key;
		struct in_addr ipv4;
		struct ether_addr mac_addr;
		uint32_t port_id;

		memset(&key, 0, sizeof(key));

		if (n_tokens != 4) {
			printf(CMD_MSG_MISMATCH_ARGS, "arp add");
			return;
		}

		if (parser_read_uint32(&port_id, tokens[1])) {
			printf(CMD_MSG_INVALID_ARG, "portid");
			return;
		}

		if (parse_ipv4_addr(tokens[2], &ipv4)) {
			printf(CMD_MSG_INVALID_ARG, "ipaddr");
			return;
		}

		if (parse_mac_addr(tokens[3], &mac_addr)) {
			printf(CMD_MSG_INVALID_ARG, "macaddr");
			return;
		}

		key.type = PIPELINE_ROUTING_ARP_IPV4;
		key.key.ipv4.port_id = port_id;
		key.key.ipv4.ip = rte_be_to_cpu_32(ipv4.s_addr);

		status = app_pipeline_routing_add_arp_entry(app,
			params->p,
			&key,
			&mac_addr);
		if (status != 0)
			printf(CMD_MSG_FAIL, "arp add");

		return;
	} /* arp add */

	/* arp add default */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "add") == 0) &&
		(strcmp(tokens[1], "default") == 0)) {
		uint32_t port_id;

		if (n_tokens != 3) {
			printf(CMD_MSG_MISMATCH_ARGS, "arp add default");
			return;
		}

		if (parser_read_uint32(&port_id, tokens[2])) {
			printf(CMD_MSG_INVALID_ARG, "portid");
			return;
		}

		status = app_pipeline_routing_add_default_arp_entry(app,
			params->p,
			port_id);
		if (status != 0)
			printf(CMD_MSG_FAIL, "arp add default");

		return;
	} /* arp add default */

	/* arp del*/
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "del") == 0) &&
		strcmp(tokens[1], "default")) {
		struct pipeline_routing_arp_key key;
		struct in_addr ipv4;
		uint32_t port_id;

		memset(&key, 0, sizeof(key));

		if (n_tokens != 3) {
			printf(CMD_MSG_MISMATCH_ARGS, "arp del");
			return;
		}

		if (parser_read_uint32(&port_id, tokens[1])) {
			printf(CMD_MSG_INVALID_ARG, "portid");
			return;
		}

		if (parse_ipv4_addr(tokens[2], &ipv4)) {
			printf(CMD_MSG_INVALID_ARG, "ipaddr");
			return;
		}

		key.type = PIPELINE_ROUTING_ARP_IPV4;
		key.key.ipv4.ip = rte_be_to_cpu_32(ipv4.s_addr);
		key.key.ipv4.port_id = port_id;

		status = app_pipeline_routing_delete_arp_entry(app,
			params->p,
			&key);
		if (status != 0)
			printf(CMD_MSG_FAIL, "arp del");

		return;
	} /* arp del */

	/* arp del default */
	if ((n_tokens >= 2) &&
		(strcmp(tokens[0], "del") == 0) &&
		(strcmp(tokens[1], "default") == 0)) {
			if (n_tokens != 2) {
				printf(CMD_MSG_MISMATCH_ARGS, "arp del default");
				return;
			}

			status = app_pipeline_routing_delete_default_arp_entry(app,
				params->p);
			if (status != 0)
				printf(CMD_MSG_FAIL, "arp del default");

			return;
	} /* arp del default */

	/* arp ls */
	if ((n_tokens >= 1) && (strcmp(tokens[0], "ls") == 0)) {
		if (n_tokens != 1) {
			printf(CMD_MSG_MISMATCH_ARGS, "arp ls");
			return;
		}

		status = app_pipeline_routing_arp_ls(app, params->p);
		if (status != 0)
			printf(CMD_MSG_FAIL, "arp ls");

		return;
	} /* arp ls */

	printf(CMD_MSG_FAIL, "arp");
}

static cmdline_parse_token_string_t cmd_arp_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_result, p_string, "p");

static cmdline_parse_token_num_t cmd_arp_p =
	TOKEN_NUM_INITIALIZER(struct cmd_arp_result, p, UINT32);

static cmdline_parse_token_string_t cmd_arp_arp_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_result, arp_string, "arp");

static cmdline_parse_token_string_t cmd_arp_multi_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_result, multi_string,
	TOKEN_STRING_MULTI);

static cmdline_parse_inst_t cmd_arp = {
	.f = cmd_arp_parsed,
	.data = NULL,
	.help_str = "arp add / add default / del / del default / ls",
	.tokens = {
		(void *)&cmd_arp_p_string,
		(void *)&cmd_arp_p,
		(void *)&cmd_arp_arp_string,
		(void *)&cmd_arp_multi_string,
		NULL,
	},
};

static cmdline_parse_ctx_t pipeline_cmds[] = {
	(cmdline_parse_inst_t *)&cmd_route,
	(cmdline_parse_inst_t *)&cmd_arp,
	NULL,
};

static struct pipeline_fe_ops pipeline_routing_fe_ops = {
	.f_init = app_pipeline_routing_init,
	.f_post_init = app_pipeline_routing_post_init,
	.f_free = app_pipeline_routing_free,
	.f_track = app_pipeline_track_default,
	.cmds = pipeline_cmds,
};

struct pipeline_type pipeline_routing = {
	.name = "ROUTING",
	.be_ops = &pipeline_routing_be_ops,
	.fe_ops = &pipeline_routing_fe_ops,
};
