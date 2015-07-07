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

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_etheraddr.h>

#include "app.h"
#include "pipeline_common_fe.h"
#include "pipeline_routing.h"

struct app_pipeline_routing_route {
	struct pipeline_routing_route_key key;
	struct app_pipeline_routing_route_params params;
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
	uint32_t n_ports_in;
	uint32_t n_ports_out;

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

static void *
pipeline_routing_init(struct pipeline_params *params,
	__rte_unused void *arg)
{
	struct pipeline_routing *p;
	uint32_t size;

	/* Check input arguments */
	if ((params == NULL) ||
		(params->n_ports_in == 0) ||
		(params->n_ports_out == 0))
		return NULL;

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct pipeline_routing));
	p = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	if (p == NULL)
		return NULL;

	/* Initialization */
	p->n_ports_in = params->n_ports_in;
	p->n_ports_out = params->n_ports_out;

	TAILQ_INIT(&p->routes);
	p->n_routes = 0;

	TAILQ_INIT(&p->arp_entries);
	p->n_arp_entries = 0;

	return p;
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
			".%" PRIu32 ".%" PRIu32 "/%" PRIu32 " => "
			"(Port = %" PRIu32 ", Next Hop IP = "
			"%" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32 ")\n",
			(key->ip >> 24) & 0xFF,
			(key->ip >> 16) & 0xFF,
			(key->ip >> 8) & 0xFF,
			key->ip & 0xFF,

			key->depth,
			route->params.port_id,

			(route->params.ip >> 24) & 0xFF,
			(route->params.ip >> 16) & 0xFF,
			(route->params.ip >> 8) & 0xFF,
			route->params.ip & 0xFF);
	}
}

static void
print_arp_entry(const struct app_pipeline_routing_arp_entry *entry)
{
	printf("(Port = %" PRIu32 ", IP = %" PRIu32 ".%" PRIu32
		".%" PRIu32 ".%" PRIu32 ") => "
		"HWaddress = %02" PRIx32 ":%02" PRIx32 ":%02" PRIx32
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

	p = app_pipeline_data_fe(app, pipeline_id);
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
	struct app_pipeline_routing_route_params *route_params)
{
	struct pipeline_routing *p;

	struct pipeline_routing_route_add_msg_req *req;
	struct pipeline_routing_route_add_msg_rsp *rsp;

	struct app_pipeline_routing_route *entry;

	int new_entry;

	/* Check input arguments */
	if ((app == NULL) ||
		(key == NULL) ||
		(route_params == NULL))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id);
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

		netmask = (~0) << (32 - depth);
		key->key.ipv4.ip &= netmask;

		/* route params */
		if (route_params->port_id >= p->n_ports_out)
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
	req->flags = route_params->flags;
	req->port_id = route_params->port_id;
	req->ip = route_params->ip;

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
	memcpy(&entry->params, route_params, sizeof(*route_params));
	entry->entry_ptr = rsp->entry_ptr;

	/* Commit entry */
	if (new_entry) {
		TAILQ_INSERT_TAIL(&p->routes, entry, node);
		p->n_routes++;
	}

	print_route(entry);

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

	p = app_pipeline_data_fe(app, pipeline_id);
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

		netmask = (~0) << (32 - depth);
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

	p = app_pipeline_data_fe(app, pipeline_id);
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

	p = app_pipeline_data_fe(app, pipeline_id);
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

	p = app_pipeline_data_fe(app, pipeline_id);
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

	p = app_pipeline_data_fe(app, pipeline_id);
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

	print_arp_entry(entry);

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

	p = app_pipeline_data_fe(app, pipeline_id);
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

	p = app_pipeline_data_fe(app, pipeline_id);
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

	p = app_pipeline_data_fe(app, pipeline_id);
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

/*
 * route add
 */

struct cmd_route_add_result {
	cmdline_fixed_string_t p_string;
	uint32_t p;
	cmdline_fixed_string_t route_string;
	cmdline_fixed_string_t add_string;
	cmdline_ipaddr_t ip;
	uint32_t depth;
	uint32_t port;
	cmdline_ipaddr_t nh_ip;
};

static void
cmd_route_add_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_route_add_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_routing_route_key key;
	struct app_pipeline_routing_route_params rt_params;
	int status;

	/* Create route */
	key.type = PIPELINE_ROUTING_ROUTE_IPV4;
	key.key.ipv4.ip = rte_bswap32((uint32_t) params->ip.addr.ipv4.s_addr);
	key.key.ipv4.depth = params->depth;

	rt_params.flags = 0; /* remote route */
	rt_params.port_id = params->port;
	rt_params.ip = rte_bswap32((uint32_t) params->nh_ip.addr.ipv4.s_addr);

	status = app_pipeline_routing_add_route(app,
		params->p,
		&key,
		&rt_params);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

static cmdline_parse_token_string_t cmd_route_add_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_add_result, p_string,
	"p");

static cmdline_parse_token_num_t cmd_route_add_p =
	TOKEN_NUM_INITIALIZER(struct cmd_route_add_result, p, UINT32);

static cmdline_parse_token_string_t cmd_route_add_route_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_add_result, route_string,
	"route");

static cmdline_parse_token_string_t cmd_route_add_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_add_result, add_string,
	"add");

static cmdline_parse_token_ipaddr_t cmd_route_add_ip =
	TOKEN_IPV4_INITIALIZER(struct cmd_route_add_result, ip);

static cmdline_parse_token_num_t cmd_route_add_depth =
	TOKEN_NUM_INITIALIZER(struct cmd_route_add_result, depth, UINT32);

static cmdline_parse_token_num_t cmd_route_add_port =
	TOKEN_NUM_INITIALIZER(struct cmd_route_add_result, port, UINT32);

static cmdline_parse_token_ipaddr_t cmd_route_add_nh_ip =
	TOKEN_IPV4_INITIALIZER(struct cmd_route_add_result, nh_ip);

static cmdline_parse_inst_t cmd_route_add = {
	.f = cmd_route_add_parsed,
	.data = NULL,
	.help_str = "Route add",
	.tokens = {
		(void *)&cmd_route_add_p_string,
		(void *)&cmd_route_add_p,
		(void *)&cmd_route_add_route_string,
		(void *)&cmd_route_add_add_string,
		(void *)&cmd_route_add_ip,
		(void *)&cmd_route_add_depth,
		(void *)&cmd_route_add_port,
		(void *)&cmd_route_add_nh_ip,
		NULL,
	},
};

/*
 * route del
 */

struct cmd_route_del_result {
	cmdline_fixed_string_t p_string;
	uint32_t p;
	cmdline_fixed_string_t route_string;
	cmdline_fixed_string_t del_string;
	cmdline_ipaddr_t ip;
	uint32_t depth;
};

static void
cmd_route_del_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_route_del_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_routing_route_key key;

	int status;

	/* Create route */
	key.type = PIPELINE_ROUTING_ROUTE_IPV4;
	key.key.ipv4.ip = rte_bswap32((uint32_t) params->ip.addr.ipv4.s_addr);
	key.key.ipv4.depth = params->depth;

	status = app_pipeline_routing_delete_route(app, params->p, &key);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

static cmdline_parse_token_string_t cmd_route_del_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_del_result, p_string,
	"p");

static cmdline_parse_token_num_t cmd_route_del_p =
	TOKEN_NUM_INITIALIZER(struct cmd_route_del_result, p, UINT32);

static cmdline_parse_token_string_t cmd_route_del_route_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_del_result, route_string,
	"route");

static cmdline_parse_token_string_t cmd_route_del_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_del_result, del_string,
	"del");

static cmdline_parse_token_ipaddr_t cmd_route_del_ip =
	TOKEN_IPV4_INITIALIZER(struct cmd_route_del_result, ip);

static cmdline_parse_token_num_t cmd_route_del_depth =
	TOKEN_NUM_INITIALIZER(struct cmd_route_del_result, depth, UINT32);

static cmdline_parse_inst_t cmd_route_del = {
	.f = cmd_route_del_parsed,
	.data = NULL,
	.help_str = "Route delete",
	.tokens = {
		(void *)&cmd_route_del_p_string,
		(void *)&cmd_route_del_p,
		(void *)&cmd_route_del_route_string,
		(void *)&cmd_route_del_del_string,
		(void *)&cmd_route_del_ip,
		(void *)&cmd_route_del_depth,
		NULL,
	},
};

/*
 * route add default
 */

struct cmd_route_add_default_result {
	cmdline_fixed_string_t p_string;
	uint32_t p;
	cmdline_fixed_string_t route_string;
	cmdline_fixed_string_t add_string;
	cmdline_fixed_string_t default_string;
	uint32_t port;
};

static void
cmd_route_add_default_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	void *data)
{
	struct cmd_route_add_default_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_routing_add_default_route(app, params->p,
			params->port);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

static cmdline_parse_token_string_t cmd_route_add_default_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_add_default_result, p_string,
	"p");

static cmdline_parse_token_num_t cmd_route_add_default_p =
	TOKEN_NUM_INITIALIZER(struct cmd_route_add_default_result, p, UINT32);

cmdline_parse_token_string_t cmd_route_add_default_route_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_add_default_result,
		route_string, "route");

cmdline_parse_token_string_t cmd_route_add_default_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_add_default_result,
		add_string, "add");

cmdline_parse_token_string_t cmd_route_add_default_default_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_add_default_result,
	default_string, "default");

cmdline_parse_token_num_t cmd_route_add_default_port =
	TOKEN_NUM_INITIALIZER(struct cmd_route_add_default_result,
		port, UINT32);

cmdline_parse_inst_t cmd_route_add_default = {
	.f = cmd_route_add_default_parsed,
	.data = NULL,
	.help_str = "Route default set",
	.tokens = {
		(void *)&cmd_route_add_default_p_string,
		(void *)&cmd_route_add_default_p,
		(void *)&cmd_route_add_default_route_string,
		(void *)&cmd_route_add_default_add_string,
		(void *)&cmd_route_add_default_default_string,
		(void *)&cmd_route_add_default_port,
		NULL,
	},
};

/*
 * route del default
 */

struct cmd_route_del_default_result {
	cmdline_fixed_string_t p_string;
	uint32_t p;
	cmdline_fixed_string_t route_string;
	cmdline_fixed_string_t del_string;
	cmdline_fixed_string_t default_string;
};

static void
cmd_route_del_default_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	 void *data)
{
	struct cmd_route_del_default_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_routing_delete_default_route(app, params->p);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

static cmdline_parse_token_string_t cmd_route_del_default_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_del_default_result, p_string,
	"p");

static cmdline_parse_token_num_t cmd_route_del_default_p =
	TOKEN_NUM_INITIALIZER(struct cmd_route_del_default_result, p, UINT32);

static cmdline_parse_token_string_t cmd_route_del_default_route_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_del_default_result,
		route_string, "route");

static cmdline_parse_token_string_t cmd_route_del_default_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_del_default_result,
		del_string, "del");

static cmdline_parse_token_string_t cmd_route_del_default_default_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_del_default_result,
	default_string, "default");


static cmdline_parse_inst_t cmd_route_del_default = {
	.f = cmd_route_del_default_parsed,
	.data = NULL,
	.help_str = "Route default clear",
	.tokens = {
		(void *)&cmd_route_del_default_p_string,
		(void *)&cmd_route_del_default_p,
		(void *)&cmd_route_del_default_route_string,
		(void *)&cmd_route_del_default_del_string,
		(void *)&cmd_route_del_default_default_string,
		NULL,
	},
};

/*
 * route ls
 */

struct cmd_route_ls_result {
	cmdline_fixed_string_t p_string;
	uint32_t p;
	cmdline_fixed_string_t route_string;
	cmdline_fixed_string_t ls_string;
};

static void
cmd_route_ls_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_route_ls_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_routing_route_ls(app, params->p);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

static cmdline_parse_token_string_t cmd_route_ls_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_ls_result, p_string, "p");

static cmdline_parse_token_num_t cmd_route_ls_p =
	TOKEN_NUM_INITIALIZER(struct cmd_route_ls_result, p, UINT32);

static cmdline_parse_token_string_t cmd_route_ls_route_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_ls_result,
	route_string, "route");

static cmdline_parse_token_string_t cmd_route_ls_ls_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_ls_result, ls_string,
	"ls");

static cmdline_parse_inst_t cmd_route_ls = {
	.f = cmd_route_ls_parsed,
	.data = NULL,
	.help_str = "Route list",
	.tokens = {
		(void *)&cmd_route_ls_p_string,
		(void *)&cmd_route_ls_p,
		(void *)&cmd_route_ls_route_string,
		(void *)&cmd_route_ls_ls_string,
		NULL,
	},
};

/*
 * arp add
 */

struct cmd_arp_add_result {
	cmdline_fixed_string_t p_string;
	uint32_t p;
	cmdline_fixed_string_t arp_string;
	cmdline_fixed_string_t add_string;
	uint32_t port_id;
	cmdline_ipaddr_t ip;
	struct ether_addr macaddr;

};

static void
cmd_arp_add_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_arp_add_result *params = parsed_result;
	struct app_params *app = data;

	struct pipeline_routing_arp_key key;
	int status;

	key.type = PIPELINE_ROUTING_ARP_IPV4;
	key.key.ipv4.port_id = params->port_id;
	key.key.ipv4.ip = rte_cpu_to_be_32(params->ip.addr.ipv4.s_addr);

	status = app_pipeline_routing_add_arp_entry(app,
		params->p,
		&key,
		&params->macaddr);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

static cmdline_parse_token_string_t cmd_arp_add_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_add_result, p_string,
	"p");

static cmdline_parse_token_num_t cmd_arp_add_p =
	TOKEN_NUM_INITIALIZER(struct cmd_arp_add_result, p, UINT32);

static cmdline_parse_token_string_t cmd_arp_add_arp_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_add_result, arp_string, "arp");

static cmdline_parse_token_string_t cmd_arp_add_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_add_result, add_string, "add");

static cmdline_parse_token_num_t cmd_arp_add_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_arp_add_result, port_id, UINT32);

static cmdline_parse_token_ipaddr_t cmd_arp_add_ip =
	TOKEN_IPV4_INITIALIZER(struct cmd_arp_add_result, ip);

static cmdline_parse_token_etheraddr_t cmd_arp_add_macaddr =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_arp_add_result, macaddr);

static cmdline_parse_inst_t cmd_arp_add = {
	.f = cmd_arp_add_parsed,
	.data = NULL,
	.help_str = "ARP add",
	.tokens = {
		(void *)&cmd_arp_add_p_string,
		(void *)&cmd_arp_add_p,
		(void *)&cmd_arp_add_arp_string,
		(void *)&cmd_arp_add_add_string,
		(void *)&cmd_arp_add_port_id,
		(void *)&cmd_arp_add_ip,
		(void *)&cmd_arp_add_macaddr,
		NULL,
	},
};

/*
 * arp del
 */

struct cmd_arp_del_result {
	cmdline_fixed_string_t p_string;
	uint32_t p;
	cmdline_fixed_string_t arp_string;
	cmdline_fixed_string_t del_string;
	uint32_t port_id;
	cmdline_ipaddr_t ip;
};

static void
cmd_arp_del_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_arp_del_result *params = parsed_result;
	struct app_params *app = data;

	struct pipeline_routing_arp_key key;
	int status;

	key.type = PIPELINE_ROUTING_ARP_IPV4;
	key.key.ipv4.ip = rte_cpu_to_be_32(params->ip.addr.ipv4.s_addr);
	key.key.ipv4.port_id = params->port_id;

	status = app_pipeline_routing_delete_arp_entry(app, params->p, &key);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

static cmdline_parse_token_string_t cmd_arp_del_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_del_result, p_string,
	"p");

static cmdline_parse_token_num_t cmd_arp_del_p =
	TOKEN_NUM_INITIALIZER(struct cmd_arp_del_result, p, UINT32);

static cmdline_parse_token_string_t cmd_arp_del_arp_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_del_result, arp_string, "arp");

static cmdline_parse_token_string_t cmd_arp_del_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_del_result, del_string, "del");

static cmdline_parse_token_num_t cmd_arp_del_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_arp_del_result, port_id, UINT32);

static cmdline_parse_token_ipaddr_t cmd_arp_del_ip =
	TOKEN_IPV4_INITIALIZER(struct cmd_arp_del_result, ip);

static cmdline_parse_inst_t cmd_arp_del = {
	.f = cmd_arp_del_parsed,
	.data = NULL,
	.help_str = "ARP delete",
	.tokens = {
		(void *)&cmd_arp_del_p_string,
		(void *)&cmd_arp_del_p,
		(void *)&cmd_arp_del_arp_string,
		(void *)&cmd_arp_del_del_string,
		(void *)&cmd_arp_del_port_id,
		(void *)&cmd_arp_del_ip,
		NULL,
	},
};

/*
 * arp add default
 */

struct cmd_arp_add_default_result {
	cmdline_fixed_string_t p_string;
	uint32_t p;
	cmdline_fixed_string_t arp_string;
	cmdline_fixed_string_t add_string;
	cmdline_fixed_string_t default_string;
	uint32_t port_id;
};

static void
cmd_arp_add_default_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_arp_add_default_result *params = parsed_result;
	struct app_params *app = data;

	int status;

	status = app_pipeline_routing_add_default_arp_entry(app,
		params->p,
		params->port_id);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

static cmdline_parse_token_string_t cmd_arp_add_default_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_add_default_result, p_string,
	"p");

static cmdline_parse_token_num_t cmd_arp_add_default_p =
	TOKEN_NUM_INITIALIZER(struct cmd_arp_add_default_result, p, UINT32);

static cmdline_parse_token_string_t cmd_arp_add_default_arp_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_add_default_result, arp_string,
	"arp");

static cmdline_parse_token_string_t cmd_arp_add_default_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_add_default_result, add_string,
	"add");

static cmdline_parse_token_string_t cmd_arp_add_default_default_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_add_default_result,
		default_string, "default");

static cmdline_parse_token_num_t cmd_arp_add_default_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_arp_add_default_result, port_id,
	UINT32);

static cmdline_parse_inst_t cmd_arp_add_default = {
	.f = cmd_arp_add_default_parsed,
	.data = NULL,
	.help_str = "ARP add default",
	.tokens = {
		(void *)&cmd_arp_add_default_p_string,
		(void *)&cmd_arp_add_default_p,
		(void *)&cmd_arp_add_default_arp_string,
		(void *)&cmd_arp_add_default_add_string,
		(void *)&cmd_arp_add_default_default_string,
		(void *)&cmd_arp_add_default_port_id,
		NULL,
	},
};

/*
 * arp del default
 */

struct cmd_arp_del_default_result {
	cmdline_fixed_string_t p_string;
	uint32_t p;
	cmdline_fixed_string_t arp_string;
	cmdline_fixed_string_t del_string;
	cmdline_fixed_string_t default_string;
};

static void
cmd_arp_del_default_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_arp_del_default_result *params = parsed_result;
	struct app_params *app = data;

	int status;

	status = app_pipeline_routing_delete_default_arp_entry(app, params->p);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

static cmdline_parse_token_string_t cmd_arp_del_default_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_del_default_result, p_string,
	"p");

static cmdline_parse_token_num_t cmd_arp_del_default_p =
	TOKEN_NUM_INITIALIZER(struct cmd_arp_del_default_result, p, UINT32);

static cmdline_parse_token_string_t cmd_arp_del_default_arp_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_del_default_result, arp_string,
	"arp");

static cmdline_parse_token_string_t cmd_arp_del_default_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_del_default_result, del_string,
	"del");

static cmdline_parse_token_string_t cmd_arp_del_default_default_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_del_default_result,
		default_string, "default");

static cmdline_parse_inst_t cmd_arp_del_default = {
	.f = cmd_arp_del_default_parsed,
	.data = NULL,
	.help_str = "ARP delete default",
	.tokens = {
		(void *)&cmd_arp_del_default_p_string,
		(void *)&cmd_arp_del_default_p,
		(void *)&cmd_arp_del_default_arp_string,
		(void *)&cmd_arp_del_default_del_string,
		(void *)&cmd_arp_del_default_default_string,
		NULL,
	},
};

/*
 * arp ls
 */

struct cmd_arp_ls_result {
	cmdline_fixed_string_t p_string;
	uint32_t p;
	cmdline_fixed_string_t arp_string;
	cmdline_fixed_string_t ls_string;
};

static void
cmd_arp_ls_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	void *data)
{
	struct cmd_arp_ls_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_routing *p;

	p = app_pipeline_data_fe(app, params->p);
	if (p == NULL)
		return;

	app_pipeline_routing_arp_ls(app, params->p);
}

static cmdline_parse_token_string_t cmd_arp_ls_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_ls_result, p_string,
	"p");

static cmdline_parse_token_num_t cmd_arp_ls_p =
	TOKEN_NUM_INITIALIZER(struct cmd_arp_ls_result, p, UINT32);

static cmdline_parse_token_string_t cmd_arp_ls_arp_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_ls_result, arp_string,
	"arp");

static cmdline_parse_token_string_t cmd_arp_ls_ls_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_ls_result, ls_string,
	"ls");

static cmdline_parse_inst_t cmd_arp_ls = {
	.f = cmd_arp_ls_parsed,
	.data = NULL,
	.help_str = "ARP list",
	.tokens = {
		(void *)&cmd_arp_ls_p_string,
		(void *)&cmd_arp_ls_p,
		(void *)&cmd_arp_ls_arp_string,
		(void *)&cmd_arp_ls_ls_string,
		NULL,
	},
};

static cmdline_parse_ctx_t pipeline_cmds[] = {
	(cmdline_parse_inst_t *)&cmd_route_add,
	(cmdline_parse_inst_t *)&cmd_route_del,
	(cmdline_parse_inst_t *)&cmd_route_add_default,
	(cmdline_parse_inst_t *)&cmd_route_del_default,
	(cmdline_parse_inst_t *)&cmd_route_ls,
	(cmdline_parse_inst_t *)&cmd_arp_add,
	(cmdline_parse_inst_t *)&cmd_arp_del,
	(cmdline_parse_inst_t *)&cmd_arp_add_default,
	(cmdline_parse_inst_t *)&cmd_arp_del_default,
	(cmdline_parse_inst_t *)&cmd_arp_ls,
	NULL,
};

static struct pipeline_fe_ops pipeline_routing_fe_ops = {
	.f_init = pipeline_routing_init,
	.f_free = app_pipeline_routing_free,
	.cmds = pipeline_cmds,
};

struct pipeline_type pipeline_routing = {
	.name = "ROUTING",
	.be_ops = &pipeline_routing_be_ops,
	.fe_ops = &pipeline_routing_fe_ops,
};
