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
#include <string.h>
#include <sys/queue.h>
#include <netinet/in.h>

#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_malloc.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_etheraddr.h>
#include <cmdline_socket.h>

#include "app.h"
#include "pipeline_common_fe.h"
#include "pipeline_firewall.h"

struct app_pipeline_firewall_rule {
	struct pipeline_firewall_key key;
	int32_t priority;
	uint32_t port_id;
	void *entry_ptr;

	TAILQ_ENTRY(app_pipeline_firewall_rule) node;
};

struct app_pipeline_firewall {
	/* parameters */
	uint32_t n_ports_in;
	uint32_t n_ports_out;

	/* rules */
	TAILQ_HEAD(, app_pipeline_firewall_rule) rules;
	uint32_t n_rules;
	uint32_t default_rule_present;
	uint32_t default_rule_port_id;
	void *default_rule_entry_ptr;
};

static void
print_firewall_ipv4_rule(struct app_pipeline_firewall_rule *rule)
{
	printf("Prio = %" PRId32 " (SA = %" PRIu32 ".%" PRIu32
		".%" PRIu32 ".%" PRIu32 "/%" PRIu32 ", "
		"DA = %" PRIu32 ".%" PRIu32
		".%"PRIu32 ".%" PRIu32 "/%" PRIu32 ", "
		"SP = %" PRIu32 "-%" PRIu32 ", "
		"DP = %" PRIu32 "-%" PRIu32 ", "
		"Proto = %" PRIu32 " / 0x%" PRIx32 ") => "
		"Port = %" PRIu32 " (entry ptr = %p)\n",

		rule->priority,

		(rule->key.key.ipv4_5tuple.src_ip >> 24) & 0xFF,
		(rule->key.key.ipv4_5tuple.src_ip >> 16) & 0xFF,
		(rule->key.key.ipv4_5tuple.src_ip >> 8) & 0xFF,
		rule->key.key.ipv4_5tuple.src_ip & 0xFF,
		rule->key.key.ipv4_5tuple.src_ip_mask,

		(rule->key.key.ipv4_5tuple.dst_ip >> 24) & 0xFF,
		(rule->key.key.ipv4_5tuple.dst_ip >> 16) & 0xFF,
		(rule->key.key.ipv4_5tuple.dst_ip >> 8) & 0xFF,
		rule->key.key.ipv4_5tuple.dst_ip & 0xFF,
		rule->key.key.ipv4_5tuple.dst_ip_mask,

		rule->key.key.ipv4_5tuple.src_port_from,
		rule->key.key.ipv4_5tuple.src_port_to,

		rule->key.key.ipv4_5tuple.dst_port_from,
		rule->key.key.ipv4_5tuple.dst_port_to,

		rule->key.key.ipv4_5tuple.proto,
		rule->key.key.ipv4_5tuple.proto_mask,

		rule->port_id,
		rule->entry_ptr);
}

static struct app_pipeline_firewall_rule *
app_pipeline_firewall_rule_find(struct app_pipeline_firewall *p,
	struct pipeline_firewall_key *key)
{
	struct app_pipeline_firewall_rule *r;

	TAILQ_FOREACH(r, &p->rules, node)
		if (memcmp(key,
			&r->key,
			sizeof(struct pipeline_firewall_key)) == 0)
			return r;

	return NULL;
}

static void
app_pipeline_firewall_ls(
	struct app_params *app,
	uint32_t pipeline_id)
{
	struct app_pipeline_firewall *p;
	struct app_pipeline_firewall_rule *rule;
	uint32_t n_rules;
	int priority;

	/* Check input arguments */
	if (app == NULL)
		return;

	p = app_pipeline_data_fe(app, pipeline_id);
	if (p == NULL)
		return;

	n_rules = p->n_rules;
	for (priority = 0; n_rules; priority++)
		TAILQ_FOREACH(rule, &p->rules, node)
			if (rule->priority == priority) {
				print_firewall_ipv4_rule(rule);
				n_rules--;
			}

	if (p->default_rule_present)
		printf("Default rule: port %" PRIu32 " (entry ptr = %p)\n",
			p->default_rule_port_id,
			p->default_rule_entry_ptr);
	else
		printf("Default rule: DROP\n");

	printf("\n");
}

static void*
app_pipeline_firewall_init(struct pipeline_params *params,
	__rte_unused void *arg)
{
	struct app_pipeline_firewall *p;
	uint32_t size;

	/* Check input arguments */
	if ((params == NULL) ||
		(params->n_ports_in == 0) ||
		(params->n_ports_out == 0))
		return NULL;

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct app_pipeline_firewall));
	p = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	if (p == NULL)
		return NULL;

	/* Initialization */
	p->n_ports_in = params->n_ports_in;
	p->n_ports_out = params->n_ports_out;

	TAILQ_INIT(&p->rules);
	p->n_rules = 0;
	p->default_rule_present = 0;
	p->default_rule_port_id = 0;
	p->default_rule_entry_ptr = NULL;

	return (void *) p;
}

static int
app_pipeline_firewall_free(void *pipeline)
{
	struct app_pipeline_firewall *p = pipeline;

	/* Check input arguments */
	if (p == NULL)
		return -1;

	/* Free resources */
	while (!TAILQ_EMPTY(&p->rules)) {
		struct app_pipeline_firewall_rule *rule;

		rule = TAILQ_FIRST(&p->rules);
		TAILQ_REMOVE(&p->rules, rule, node);
		rte_free(rule);
	}

	rte_free(p);
	return 0;
}

static int
app_pipeline_firewall_key_check_and_normalize(struct pipeline_firewall_key *key)
{
	switch (key->type) {
	case PIPELINE_FIREWALL_IPV4_5TUPLE:
	{
		uint32_t src_ip_depth = key->key.ipv4_5tuple.src_ip_mask;
		uint32_t dst_ip_depth = key->key.ipv4_5tuple.dst_ip_mask;
		uint16_t src_port_from = key->key.ipv4_5tuple.src_port_from;
		uint16_t src_port_to = key->key.ipv4_5tuple.src_port_to;
		uint16_t dst_port_from = key->key.ipv4_5tuple.dst_port_from;
		uint16_t dst_port_to = key->key.ipv4_5tuple.dst_port_to;

		uint32_t src_ip_netmask = 0;
		uint32_t dst_ip_netmask = 0;

		if ((src_ip_depth > 32) ||
			(dst_ip_depth > 32) ||
			(src_port_from > src_port_to) ||
			(dst_port_from > dst_port_to))
			return -1;

		if (src_ip_depth)
			src_ip_netmask = (~0) << (32 - src_ip_depth);

		if (dst_ip_depth)
			dst_ip_netmask = ((~0) << (32 - dst_ip_depth));

		key->key.ipv4_5tuple.src_ip &= src_ip_netmask;
		key->key.ipv4_5tuple.dst_ip &= dst_ip_netmask;

		return 0;
	}

	default:
		return -1;
	}
}

int
app_pipeline_firewall_add_rule(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_firewall_key *key,
	uint32_t priority,
	uint32_t port_id)
{
	struct app_pipeline_firewall *p;
	struct app_pipeline_firewall_rule *rule;
	struct pipeline_firewall_add_msg_req *req;
	struct pipeline_firewall_add_msg_rsp *rsp;
	int new_rule;

	/* Check input arguments */
	if ((app == NULL) ||
		(key == NULL) ||
		(key->type != PIPELINE_FIREWALL_IPV4_5TUPLE))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id);
	if (p == NULL)
		return -1;

	if (port_id >= p->n_ports_out)
		return -1;

	if (app_pipeline_firewall_key_check_and_normalize(key) != 0)
		return -1;

	/* Find existing rule or allocate new rule */
	rule = app_pipeline_firewall_rule_find(p, key);
	new_rule = (rule == NULL);
	if (rule == NULL) {
		rule = rte_malloc(NULL, sizeof(*rule), RTE_CACHE_LINE_SIZE);

		if (rule == NULL)
			return -1;
	}

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL) {
		if (new_rule)
			rte_free(rule);
		return -1;
	}

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_FIREWALL_MSG_REQ_ADD;
	memcpy(&req->key, key, sizeof(*key));
	req->priority = priority;
	req->port_id = port_id;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL) {
		if (new_rule)
			rte_free(rule);
		return -1;
	}

	/* Read response and write rule */
	if (rsp->status ||
		(rsp->entry_ptr == NULL) ||
		((new_rule == 0) && (rsp->key_found == 0)) ||
		((new_rule == 1) && (rsp->key_found == 1))) {
		app_msg_free(app, rsp);
		if (new_rule)
			rte_free(rule);
		return -1;
	}

	memcpy(&rule->key, key, sizeof(*key));
	rule->priority = priority;
	rule->port_id = port_id;
	rule->entry_ptr = rsp->entry_ptr;

	/* Commit rule */
	if (new_rule) {
		TAILQ_INSERT_TAIL(&p->rules, rule, node);
		p->n_rules++;
	}

	print_firewall_ipv4_rule(rule);

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_firewall_delete_rule(struct app_params *app,
	uint32_t pipeline_id,
	struct pipeline_firewall_key *key)
{
	struct app_pipeline_firewall *p;
	struct app_pipeline_firewall_rule *rule;
	struct pipeline_firewall_del_msg_req *req;
	struct pipeline_firewall_del_msg_rsp *rsp;

	/* Check input arguments */
	if ((app == NULL) ||
		(key == NULL) ||
		(key->type != PIPELINE_FIREWALL_IPV4_5TUPLE))
		return -1;

	p = app_pipeline_data_fe(app, pipeline_id);
	if (p == NULL)
		return -1;

	if (app_pipeline_firewall_key_check_and_normalize(key) != 0)
		return -1;

	/* Find rule */
	rule = app_pipeline_firewall_rule_find(p, key);
	if (rule == NULL)
		return 0;

	/* Allocate and write request */
	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = PIPELINE_MSG_REQ_CUSTOM;
	req->subtype = PIPELINE_FIREWALL_MSG_REQ_DEL;
	memcpy(&req->key, key, sizeof(*key));

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response */
	if (rsp->status || !rsp->key_found) {
		app_msg_free(app, rsp);
		return -1;
	}

	/* Remove rule */
	TAILQ_REMOVE(&p->rules, rule, node);
	p->n_rules--;
	rte_free(rule);

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_firewall_add_default_rule(struct app_params *app,
	uint32_t pipeline_id,
	uint32_t port_id)
{
	struct app_pipeline_firewall *p;
	struct pipeline_firewall_add_default_msg_req *req;
	struct pipeline_firewall_add_default_msg_rsp *rsp;

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
	req->subtype = PIPELINE_FIREWALL_MSG_REQ_ADD_DEFAULT;
	req->port_id = port_id;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response and write rule */
	if (rsp->status || (rsp->entry_ptr == NULL)) {
		app_msg_free(app, rsp);
		return -1;
	}

	p->default_rule_port_id = port_id;
	p->default_rule_entry_ptr = rsp->entry_ptr;

	/* Commit rule */
	p->default_rule_present = 1;

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

int
app_pipeline_firewall_delete_default_rule(struct app_params *app,
	uint32_t pipeline_id)
{
	struct app_pipeline_firewall *p;
	struct pipeline_firewall_del_default_msg_req *req;
	struct pipeline_firewall_del_default_msg_rsp *rsp;

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
	req->subtype = PIPELINE_FIREWALL_MSG_REQ_DEL_DEFAULT;

	/* Send request and wait for response */
	rsp = app_msg_send_recv(app, pipeline_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	/* Read response and write rule */
	if (rsp->status) {
		app_msg_free(app, rsp);
		return -1;
	}

	/* Commit rule */
	p->default_rule_present = 0;

	/* Free response */
	app_msg_free(app, rsp);

	return 0;
}

/*
 * p firewall add ipv4
 */

struct cmd_firewall_add_ipv4_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t firewall_string;
	cmdline_fixed_string_t add_string;
	cmdline_fixed_string_t ipv4_string;
	int32_t priority;
	cmdline_ipaddr_t src_ip;
	uint32_t src_ip_mask;
	cmdline_ipaddr_t dst_ip;
	uint32_t dst_ip_mask;
	uint16_t src_port_from;
	uint16_t src_port_to;
	uint16_t dst_port_from;
	uint16_t dst_port_to;
	uint8_t proto;
	uint8_t proto_mask;
	uint8_t port_id;
};

static void
cmd_firewall_add_ipv4_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	void *data)
{
	struct cmd_firewall_add_ipv4_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_firewall_key key;
	int status;

	key.type = PIPELINE_FIREWALL_IPV4_5TUPLE;
	key.key.ipv4_5tuple.src_ip = rte_bswap32(
		(uint32_t) params->src_ip.addr.ipv4.s_addr);
	key.key.ipv4_5tuple.src_ip_mask = params->src_ip_mask;
	key.key.ipv4_5tuple.dst_ip = rte_bswap32(
		(uint32_t) params->dst_ip.addr.ipv4.s_addr);
	key.key.ipv4_5tuple.dst_ip_mask = params->dst_ip_mask;
	key.key.ipv4_5tuple.src_port_from = params->src_port_from;
	key.key.ipv4_5tuple.src_port_to = params->src_port_to;
	key.key.ipv4_5tuple.dst_port_from = params->dst_port_from;
	key.key.ipv4_5tuple.dst_port_to = params->dst_port_to;
	key.key.ipv4_5tuple.proto = params->proto;
	key.key.ipv4_5tuple.proto_mask = params->proto_mask;

	status = app_pipeline_firewall_add_rule(app,
		params->pipeline_id,
		&key,
		params->priority,
		params->port_id);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

cmdline_parse_token_string_t cmd_firewall_add_ipv4_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_add_ipv4_result, p_string,
		"p");

cmdline_parse_token_num_t cmd_firewall_add_ipv4_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_ipv4_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_firewall_add_ipv4_firewall_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_add_ipv4_result,
		firewall_string, "firewall");

cmdline_parse_token_string_t cmd_firewall_add_ipv4_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_add_ipv4_result,
		add_string, "add");

cmdline_parse_token_string_t cmd_firewall_add_ipv4_ipv4_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_add_ipv4_result,
		ipv4_string, "ipv4");

cmdline_parse_token_num_t cmd_firewall_add_ipv4_priority =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_ipv4_result, priority,
		INT32);

cmdline_parse_token_ipaddr_t cmd_firewall_add_ipv4_src_ip =
	TOKEN_IPV4_INITIALIZER(struct cmd_firewall_add_ipv4_result, src_ip);

cmdline_parse_token_num_t cmd_firewall_add_ipv4_src_ip_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_ipv4_result, src_ip_mask,
		UINT32);

cmdline_parse_token_ipaddr_t cmd_firewall_add_ipv4_dst_ip =
	TOKEN_IPV4_INITIALIZER(struct cmd_firewall_add_ipv4_result, dst_ip);

cmdline_parse_token_num_t cmd_firewall_add_ipv4_dst_ip_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_ipv4_result, dst_ip_mask,
		UINT32);

cmdline_parse_token_num_t cmd_firewall_add_ipv4_src_port_from =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_ipv4_result,
		src_port_from, UINT16);

cmdline_parse_token_num_t cmd_firewall_add_ipv4_src_port_to =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_ipv4_result,
		src_port_to, UINT16);

cmdline_parse_token_num_t cmd_firewall_add_ipv4_dst_port_from =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_ipv4_result,
		dst_port_from, UINT16);

cmdline_parse_token_num_t cmd_firewall_add_ipv4_dst_port_to =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_ipv4_result,
		dst_port_to, UINT16);

cmdline_parse_token_num_t cmd_firewall_add_ipv4_proto =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_ipv4_result,
		proto, UINT8);

cmdline_parse_token_num_t cmd_firewall_add_ipv4_proto_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_ipv4_result,
		proto_mask, UINT8);

cmdline_parse_token_num_t cmd_firewall_add_ipv4_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_ipv4_result,
		port_id, UINT8);

cmdline_parse_inst_t cmd_firewall_add_ipv4 = {
	.f = cmd_firewall_add_ipv4_parsed,
	.data = NULL,
	.help_str = "Firewall rule add",
	.tokens = {
		(void *) &cmd_firewall_add_ipv4_p_string,
		(void *) &cmd_firewall_add_ipv4_pipeline_id,
		(void *) &cmd_firewall_add_ipv4_firewall_string,
		(void *) &cmd_firewall_add_ipv4_add_string,
		(void *) &cmd_firewall_add_ipv4_ipv4_string,
		(void *) &cmd_firewall_add_ipv4_priority,
		(void *) &cmd_firewall_add_ipv4_src_ip,
		(void *) &cmd_firewall_add_ipv4_src_ip_mask,
		(void *) &cmd_firewall_add_ipv4_dst_ip,
		(void *) &cmd_firewall_add_ipv4_dst_ip_mask,
		(void *) &cmd_firewall_add_ipv4_src_port_from,
		(void *) &cmd_firewall_add_ipv4_src_port_to,
		(void *) &cmd_firewall_add_ipv4_dst_port_from,
		(void *) &cmd_firewall_add_ipv4_dst_port_to,
		(void *) &cmd_firewall_add_ipv4_proto,
		(void *) &cmd_firewall_add_ipv4_proto_mask,
		(void *) &cmd_firewall_add_ipv4_port_id,
		NULL,
	},
};

/*
 * p firewall del ipv4
 */

struct cmd_firewall_del_ipv4_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t firewall_string;
	cmdline_fixed_string_t del_string;
	cmdline_fixed_string_t ipv4_string;
	cmdline_ipaddr_t src_ip;
	uint32_t src_ip_mask;
	cmdline_ipaddr_t dst_ip;
	uint32_t dst_ip_mask;
	uint16_t src_port_from;
	uint16_t src_port_to;
	uint16_t dst_port_from;
	uint16_t dst_port_to;
	uint8_t proto;
	uint8_t proto_mask;
};

static void
cmd_firewall_del_ipv4_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	void *data)
{
	struct cmd_firewall_del_ipv4_result *params = parsed_result;
	struct app_params *app = data;
	struct pipeline_firewall_key key;
	int status;

	key.type = PIPELINE_FIREWALL_IPV4_5TUPLE;
	key.key.ipv4_5tuple.src_ip = rte_bswap32(
		(uint32_t) params->src_ip.addr.ipv4.s_addr);
	key.key.ipv4_5tuple.src_ip_mask = params->src_ip_mask;
	key.key.ipv4_5tuple.dst_ip = rte_bswap32(
		(uint32_t) params->dst_ip.addr.ipv4.s_addr);
	key.key.ipv4_5tuple.dst_ip_mask = params->dst_ip_mask;
	key.key.ipv4_5tuple.src_port_from = params->src_port_from;
	key.key.ipv4_5tuple.src_port_to = params->src_port_to;
	key.key.ipv4_5tuple.dst_port_from = params->dst_port_from;
	key.key.ipv4_5tuple.dst_port_to = params->dst_port_to;
	key.key.ipv4_5tuple.proto = params->proto;
	key.key.ipv4_5tuple.proto_mask = params->proto_mask;

	status = app_pipeline_firewall_delete_rule(app,
		params->pipeline_id,
		&key);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

cmdline_parse_token_string_t cmd_firewall_del_ipv4_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_del_ipv4_result, p_string,
		"p");

cmdline_parse_token_num_t cmd_firewall_del_ipv4_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_ipv4_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_firewall_del_ipv4_firewall_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_del_ipv4_result,
		firewall_string, "firewall");

cmdline_parse_token_string_t cmd_firewall_del_ipv4_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_del_ipv4_result,
		del_string, "del");

cmdline_parse_token_string_t cmd_firewall_del_ipv4_ipv4_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_del_ipv4_result,
		ipv4_string, "ipv4");

cmdline_parse_token_ipaddr_t cmd_firewall_del_ipv4_src_ip =
	TOKEN_IPV4_INITIALIZER(struct cmd_firewall_del_ipv4_result, src_ip);

cmdline_parse_token_num_t cmd_firewall_del_ipv4_src_ip_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_ipv4_result, src_ip_mask,
		UINT32);

cmdline_parse_token_ipaddr_t cmd_firewall_del_ipv4_dst_ip =
	TOKEN_IPV4_INITIALIZER(struct cmd_firewall_del_ipv4_result, dst_ip);

cmdline_parse_token_num_t cmd_firewall_del_ipv4_dst_ip_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_ipv4_result, dst_ip_mask,
		UINT32);

cmdline_parse_token_num_t cmd_firewall_del_ipv4_src_port_from =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_ipv4_result,
		src_port_from, UINT16);

cmdline_parse_token_num_t cmd_firewall_del_ipv4_src_port_to =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_ipv4_result, src_port_to,
		UINT16);

cmdline_parse_token_num_t cmd_firewall_del_ipv4_dst_port_from =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_ipv4_result,
		dst_port_from, UINT16);

cmdline_parse_token_num_t cmd_firewall_del_ipv4_dst_port_to =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_ipv4_result,
		dst_port_to, UINT16);

cmdline_parse_token_num_t cmd_firewall_del_ipv4_proto =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_ipv4_result,
		proto, UINT8);

cmdline_parse_token_num_t cmd_firewall_del_ipv4_proto_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_ipv4_result, proto_mask,
		UINT8);

cmdline_parse_inst_t cmd_firewall_del_ipv4 = {
	.f = cmd_firewall_del_ipv4_parsed,
	.data = NULL,
	.help_str = "Firewall rule delete",
	.tokens = {
		(void *) &cmd_firewall_del_ipv4_p_string,
		(void *) &cmd_firewall_del_ipv4_pipeline_id,
		(void *) &cmd_firewall_del_ipv4_firewall_string,
		(void *) &cmd_firewall_del_ipv4_del_string,
		(void *) &cmd_firewall_del_ipv4_ipv4_string,
		(void *) &cmd_firewall_del_ipv4_src_ip,
		(void *) &cmd_firewall_del_ipv4_src_ip_mask,
		(void *) &cmd_firewall_del_ipv4_dst_ip,
		(void *) &cmd_firewall_del_ipv4_dst_ip_mask,
		(void *) &cmd_firewall_del_ipv4_src_port_from,
		(void *) &cmd_firewall_del_ipv4_src_port_to,
		(void *) &cmd_firewall_del_ipv4_dst_port_from,
		(void *) &cmd_firewall_del_ipv4_dst_port_to,
		(void *) &cmd_firewall_del_ipv4_proto,
		(void *) &cmd_firewall_del_ipv4_proto_mask,
		NULL,
	},
};

/*
 * p firewall add default
 */
struct cmd_firewall_add_default_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t firewall_string;
	cmdline_fixed_string_t add_string;
	cmdline_fixed_string_t default_string;
	uint8_t port_id;
};

static void
cmd_firewall_add_default_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	void *data)
{
	struct cmd_firewall_add_default_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_firewall_add_default_rule(app,
		params->pipeline_id,
		params->port_id);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

cmdline_parse_token_string_t cmd_firewall_add_default_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_add_default_result,
		p_string, "p");

cmdline_parse_token_num_t cmd_firewall_add_default_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_default_result,
		pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_firewall_add_default_firewall_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_add_default_result,
	firewall_string, "firewall");

cmdline_parse_token_string_t cmd_firewall_add_default_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_add_default_result,
	add_string, "add");

cmdline_parse_token_string_t cmd_firewall_add_default_default_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_add_default_result,
		default_string, "default");

cmdline_parse_token_num_t cmd_firewall_add_default_port_id =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_default_result, port_id,
		UINT8);

cmdline_parse_inst_t cmd_firewall_add_default = {
	.f = cmd_firewall_add_default_parsed,
	.data = NULL,
	.help_str = "Firewall default rule add",
	.tokens = {
		(void *) &cmd_firewall_add_default_p_string,
		(void *) &cmd_firewall_add_default_pipeline_id,
		(void *) &cmd_firewall_add_default_firewall_string,
		(void *) &cmd_firewall_add_default_add_string,
		(void *) &cmd_firewall_add_default_default_string,
		(void *) &cmd_firewall_add_default_port_id,
		NULL,
	},
};

/*
 * p firewall del default
 */
struct cmd_firewall_del_default_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t firewall_string;
	cmdline_fixed_string_t del_string;
	cmdline_fixed_string_t default_string;
};

static void
cmd_firewall_del_default_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	void *data)
{
	struct cmd_firewall_del_default_result *params = parsed_result;
	struct app_params *app = data;
	int status;

	status = app_pipeline_firewall_delete_default_rule(app,
		params->pipeline_id);

	if (status != 0) {
		printf("Command failed\n");
		return;
	}
}

cmdline_parse_token_string_t cmd_firewall_del_default_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_del_default_result,
		p_string, "p");

cmdline_parse_token_num_t cmd_firewall_del_default_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_default_result,
		pipeline_id, UINT32);

cmdline_parse_token_string_t cmd_firewall_del_default_firewall_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_del_default_result,
	firewall_string, "firewall");

cmdline_parse_token_string_t cmd_firewall_del_default_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_del_default_result,
		del_string, "del");

cmdline_parse_token_string_t cmd_firewall_del_default_default_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_del_default_result,
		default_string, "default");

cmdline_parse_inst_t cmd_firewall_del_default = {
	.f = cmd_firewall_del_default_parsed,
	.data = NULL,
	.help_str = "Firewall default rule delete",
	.tokens = {
		(void *) &cmd_firewall_del_default_p_string,
		(void *) &cmd_firewall_del_default_pipeline_id,
		(void *) &cmd_firewall_del_default_firewall_string,
		(void *) &cmd_firewall_del_default_del_string,
		(void *) &cmd_firewall_del_default_default_string,
		NULL,
	},
};

/*
 * p firewall ls
 */

struct cmd_firewall_ls_result {
	cmdline_fixed_string_t p_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t firewall_string;
	cmdline_fixed_string_t ls_string;
};

static void
cmd_firewall_ls_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	void *data)
{
	struct cmd_firewall_ls_result *params = parsed_result;
	struct app_params *app = data;

	app_pipeline_firewall_ls(app, params->pipeline_id);
}

cmdline_parse_token_string_t cmd_firewall_ls_p_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_ls_result, p_string,
		"p");

cmdline_parse_token_num_t cmd_firewall_ls_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_ls_result, pipeline_id,
		UINT32);

cmdline_parse_token_string_t cmd_firewall_ls_firewall_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_ls_result,
	firewall_string, "firewall");

cmdline_parse_token_string_t cmd_firewall_ls_ls_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_ls_result, ls_string,
	"ls");

cmdline_parse_inst_t cmd_firewall_ls = {
	.f = cmd_firewall_ls_parsed,
	.data = NULL,
	.help_str = "Firewall rule list",
	.tokens = {
		(void *) &cmd_firewall_ls_p_string,
		(void *) &cmd_firewall_ls_pipeline_id,
		(void *) &cmd_firewall_ls_firewall_string,
		(void *) &cmd_firewall_ls_ls_string,
		NULL,
	},
};

static cmdline_parse_ctx_t pipeline_cmds[] = {
	(cmdline_parse_inst_t *) &cmd_firewall_add_ipv4,
	(cmdline_parse_inst_t *) &cmd_firewall_del_ipv4,
	(cmdline_parse_inst_t *) &cmd_firewall_add_default,
	(cmdline_parse_inst_t *) &cmd_firewall_del_default,
	(cmdline_parse_inst_t *) &cmd_firewall_ls,
	NULL,
};

static struct pipeline_fe_ops pipeline_firewall_fe_ops = {
	.f_init = app_pipeline_firewall_init,
	.f_free = app_pipeline_firewall_free,
	.cmds = pipeline_cmds,
};

struct pipeline_type pipeline_firewall = {
	.name = "FIREWALL",
	.be_ops = &pipeline_firewall_be_ops,
	.fe_ops = &pipeline_firewall_fe_ops,
};
