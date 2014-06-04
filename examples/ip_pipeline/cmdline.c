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
#include <termios.h>
#include <inttypes.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>

#include <rte_ether.h>
#include <rte_byteorder.h>
#include <rte_ring.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_etheraddr.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "main.h"

#define IS_RULE_PRESENT(res, rule_key, table, type)			\
do {									\
	struct app_rule *it;						\
									\
	(res) = NULL;							\
	TAILQ_FOREACH(it, &table, entries) {				\
		if (memcmp(&rule_key, &it->type.key, sizeof(rule_key)) == 0) {\
			(res) = it;					\
			break;						\
		}							\
	}								\
} while (0)

/* Rules */
static void
app_init_rule_tables(void);

TAILQ_HEAD(linked_list, app_rule) arp_table, routing_table, firewall_table,
	flow_table;

uint32_t n_arp_rules;
uint32_t n_routing_rules;
uint32_t n_firewall_rules;
uint32_t n_flow_rules;

struct app_arp_rule {
	struct {
		uint8_t out_iface;
		uint32_t nh_ip;
	} key;

	struct ether_addr nh_arp;
};

struct app_routing_rule {
	struct {
		uint32_t ip;
		uint8_t depth;
	} key;

	uint8_t port;
	uint32_t nh_ip;
};

struct app_firewall_rule {
	struct {
		uint32_t src_ip;
		uint32_t src_ip_mask;
		uint32_t dst_ip;
		uint32_t dst_ip_mask;
		uint16_t src_port_from;
		uint16_t src_port_to;
		uint16_t dst_port_from;
		uint16_t dst_port_to;
		uint8_t proto;
		uint8_t proto_mask;
	} key;

	int32_t priority;
	uint8_t port;
};

struct app_flow_rule {
	struct {
		uint32_t src_ip;
		uint32_t dst_ip;
		uint16_t src_port;
		uint16_t dst_port;
		uint8_t proto;
	} key;

	uint8_t port;
};

struct app_rule {
	union {
		struct app_arp_rule arp;
		struct app_routing_rule routing;
		struct app_firewall_rule firewall;
		struct app_flow_rule flow;
	};

	TAILQ_ENTRY(app_rule) entries;
};

/* Initialization */
static void
app_init_rule_tables(void)
{
	TAILQ_INIT(&arp_table);
	TAILQ_INIT(&routing_table);
	TAILQ_INIT(&firewall_table);
	TAILQ_INIT(&flow_table);

	n_arp_rules = 0;
	n_routing_rules = 0;
	n_firewall_rules = 0;
	n_flow_rules = 0;
}

/* Printing */
static void
print_arp_rule(struct app_arp_rule rule)
{
	printf("(Iface = %u, Address = %u.%u.%u.%u) => "
		"HWaddress = %02x:%02x:%02x:%02x:%02x:%02x\n",
		rule.key.out_iface,
		(rule.key.nh_ip >> 24) & 0xFF,
		(rule.key.nh_ip >> 16) & 0xFF,
		(rule.key.nh_ip >> 8) & 0xFF,
		rule.key.nh_ip & 0xFF,

		rule.nh_arp.addr_bytes[0],
		rule.nh_arp.addr_bytes[1],
		rule.nh_arp.addr_bytes[2],
		rule.nh_arp.addr_bytes[3],
		rule.nh_arp.addr_bytes[4],
		rule.nh_arp.addr_bytes[5]);
}

static void
print_routing_rule(struct app_routing_rule rule)
{
	printf("IP Prefix = %u.%u.%u.%u/%u => "
		"(Iface = %u, Gateway = %u.%u.%u.%u)\n",
		(rule.key.ip >> 24) & 0xFF,
		(rule.key.ip >> 16) & 0xFF,
		(rule.key.ip >> 8) & 0xFF,
		rule.key.ip & 0xFF,

		rule.key.depth,
		rule.port,

		(rule.nh_ip >> 24) & 0xFF,
		(rule.nh_ip >> 16) & 0xFF,
		(rule.nh_ip >> 8) & 0xFF,
		rule.nh_ip & 0xFF);
}

#ifdef RTE_LIBRTE_ACL

static void
print_firewall_rule(struct app_firewall_rule rule)
{
	printf("Priority %d: (IP Src = %u.%u.%u.%u/%u, "
		"IP Dst = %u.%u.%u.%u/%u, "
		"Port Src = %u-%u, Port Dst = %u-%u, Proto = %u (%u)) => "
		"Port = %u\n",
		rule.priority,

		(rule.key.src_ip >> 24) & 0xFF,
		(rule.key.src_ip >> 16) & 0xFF,
		(rule.key.src_ip >> 8) & 0xFF,
		rule.key.src_ip & 0xFF,
		rule.key.src_ip_mask,

		(rule.key.dst_ip >> 24) & 0xFF,
		(rule.key.dst_ip >> 16) & 0xFF,
		(rule.key.dst_ip >> 8) & 0xFF,
		rule.key.dst_ip & 0xFF,
		rule.key.dst_ip_mask,

		rule.key.src_port_from,
		rule.key.src_port_to,
		rule.key.dst_port_from,
		rule.key.dst_port_to,
		rule.key.proto,
		rule.key.proto_mask,
		rule.port);
}

#endif

static void
print_flow_rule(struct app_flow_rule rule)
{
	printf("(IP Src = %u.%u.%u.%u, IP Dst = %u.%u.%u.%u, Port Src = %u, "
		"Port Dst = %u, Proto = %u) => Port = %u\n",
		(rule.key.src_ip >> 24) & 0xFF,
		(rule.key.src_ip >> 16) & 0xFF,
		(rule.key.src_ip >> 8) & 0xFF,
		rule.key.src_ip & 0xFF,

		(rule.key.dst_ip >> 24) & 0xFF,
		(rule.key.dst_ip >> 16) & 0xFF,
		(rule.key.dst_ip >> 8) & 0xFF,
		rule.key.dst_ip  & 0xFF,

		rule.key.src_port,
		rule.key.dst_port,
		(uint32_t) rule.key.proto,
		rule.port);
}

/* Commands */

/* *** Run file (script) *** */
struct cmd_run_file_result {
	cmdline_fixed_string_t run_string;
	char file_path[100];
};

static void
cmd_run_file_parsed(
	void *parsed_result,
	struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_run_file_result *params = parsed_result;
	struct cmdline *file_cl;
	int fd;

	/* Check params */
	if (!params->file_path) {
		printf("Illegal value for file path (%s)\n", params->file_path);
		return;
	}

	fd = open(params->file_path, O_RDONLY, 0);
	if (fd < 0) {
		printf("Illegal value for file path (%s)\n", params->file_path);
		return;
	}

	file_cl = cmdline_new(cl->ctx, "", fd, 1);
	cmdline_interact(file_cl);
	close(fd);
}

cmdline_parse_token_string_t cmd_run_file_run_string =
	TOKEN_STRING_INITIALIZER(struct cmd_run_file_result, run_string, "run");

cmdline_parse_token_string_t cmd_run_file_file_path =
	TOKEN_STRING_INITIALIZER(struct cmd_run_file_result, file_path, NULL);

cmdline_parse_inst_t cmd_run_file = {
	.f = cmd_run_file_parsed,
	.data = NULL,
	.help_str = "Run commands from file",
	.tokens = {
		(void *)&cmd_run_file_run_string,
		(void *)&cmd_run_file_file_path,
		NULL,
	},
};

/* *** Link - Enable *** */
struct cmd_link_enable_result {
	cmdline_fixed_string_t link_string;
	uint8_t port;
	cmdline_fixed_string_t up_string;
};

static void
cmd_link_enable_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_link_enable_result *params = parsed_result;
	void *msg;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	int status;

	uint32_t core_id = app_get_first_core_id(APP_CORE_RX);

	if (core_id == RTE_MAX_LCORE) {
		printf("RX core not preformed by any CPU core\n");
		return;
	}

	struct rte_ring *ring_req = app_get_ring_req(core_id);
	struct rte_ring *ring_resp = app_get_ring_resp(core_id);

	/* Check params */
	if (params->port >= app.n_ports) {
		printf("Illegal value for port parameter (%u)\n", params->port);
		return;
	}

	printf("Enabling port %d\n", params->port);

	/* Allocate message buffer */
	msg = (void *)rte_ctrlmbuf_alloc(app.msg_pool);
	if (msg == NULL)
		rte_panic("Unable to allocate new message\n");

	/* Fill request message */
	req = (struct app_msg_req *) ((struct rte_mbuf *)msg)->ctrl.data;
	req->type = APP_MSG_REQ_RX_PORT_ENABLE;
	req->rx_up.port = params->port;

	/* Send request */
	do {
		status = rte_ring_sp_enqueue(ring_req, msg);
	} while (status == -ENOBUFS);

	/* Wait for response */
	do {
		status = rte_ring_sc_dequeue(ring_resp, &msg);
	} while (status != 0);
	resp = (struct app_msg_resp *) ((struct rte_mbuf *)msg)->ctrl.data;
	/* Check response */
	if (resp->result != 0)
		printf("Request LINK_UP failed (%u)\n", resp->result);

	/* Free message buffer */
	rte_ctrlmbuf_free(msg);
}

cmdline_parse_token_string_t cmd_link_enable_link_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_enable_result, link_string,
	"link");

cmdline_parse_token_num_t cmd_link_enable_port =
	TOKEN_NUM_INITIALIZER(struct cmd_link_enable_result, port, UINT8);

cmdline_parse_token_string_t cmd_link_enable_up_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_enable_result, up_string,
	"up");

cmdline_parse_inst_t cmd_link_enable = {
	.f = cmd_link_enable_parsed,
	.data = NULL,
	.help_str = "Link down",
	.tokens = {
		(void *)&cmd_link_enable_link_string,
		(void *)&cmd_link_enable_port,
		(void *)&cmd_link_enable_up_string,
		NULL,
	},
};

/* *** Link - Disable *** */
struct cmd_link_disable_result {
	cmdline_fixed_string_t link_string;
	uint8_t port;
	cmdline_fixed_string_t down_string;
};

static void
cmd_link_disable_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_link_disable_result *params = parsed_result;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	void *msg;
	int status;

	uint32_t core_id = app_get_first_core_id(APP_CORE_RX);

	if (core_id == RTE_MAX_LCORE) {
		printf("RX not performed by any CPU core\n");
		return;
	}

	struct rte_ring *ring_req = app_get_ring_req(core_id);
	struct rte_ring *ring_resp = app_get_ring_resp(core_id);

	/* Check params */
	if (params->port >= app.n_ports) {
		printf("Illegal value for port parameter (%u)\n", params->port);
		return;
	}

	printf("Disabling port %d\n", params->port);

	/* Allocate message buffer */
	msg = rte_ctrlmbuf_alloc(app.msg_pool);
	if (msg == NULL)
		rte_panic("Unable to allocate new message\n");

	/* Fill request message */
	req = (struct app_msg_req *) ((struct rte_mbuf *)msg)->ctrl.data;
	req->type = APP_MSG_REQ_RX_PORT_DISABLE;
	req->rx_down.port = params->port;

	/* Send request */
	do {
		status = rte_ring_sp_enqueue(ring_req, msg);
	} while (status == -ENOBUFS);

	/* Wait for response */
	do {
		status = rte_ring_sc_dequeue(ring_resp, &msg);
	} while (status != 0);
	resp = (struct app_msg_resp *) ((struct rte_mbuf *)msg)->ctrl.data;

	/* Check response */
	if (resp->result != 0)
		printf("Request LINK_DOWN failed (%u)\n", resp->result);

	/* Free message buffer */
	rte_ctrlmbuf_free((struct rte_mbuf *)msg);
}

cmdline_parse_token_string_t cmd_link_disable_link_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_disable_result, link_string,
	"link");

cmdline_parse_token_num_t cmd_link_disable_port =
	TOKEN_NUM_INITIALIZER(struct cmd_link_disable_result, port, UINT8);

cmdline_parse_token_string_t cmd_link_disable_down_string =
	TOKEN_STRING_INITIALIZER(struct cmd_link_disable_result, down_string,
	"down");

cmdline_parse_inst_t cmd_link_disable = {
	.f = cmd_link_disable_parsed,
	.data = NULL,
	.help_str = "Link up",
	.tokens = {
		(void *)&cmd_link_disable_link_string,
		(void *)&cmd_link_disable_port,
		(void *)&cmd_link_disable_down_string,
		NULL,
	},
};


/* *** ARP - Add *** */
struct cmd_arp_add_result {
	cmdline_fixed_string_t arp_string;
	cmdline_fixed_string_t add_string;
	uint8_t out_iface;
	cmdline_ipaddr_t nh_ip;
	struct ether_addr nh_arp;

};

static void
cmd_arp_add_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_arp_add_result *params = parsed_result;
	struct app_rule rule, *old_rule;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	void *msg;
	int status;

	uint32_t core_id = app_get_first_core_id(APP_CORE_RT);

	if (core_id == RTE_MAX_LCORE) {
		printf("ARP not performed by any CPU core\n");
		return;
	}

	struct rte_ring *ring_req = app_get_ring_req(core_id);
	struct rte_ring *ring_resp = app_get_ring_resp(core_id);

	/* Check params */
	if (params->out_iface >= app.n_ports) {
		printf("Illegal value for output interface parameter (%u)\n",
			params->out_iface);
		return;
	}

	/* Create rule */
	memset(&rule, 0, sizeof(rule));
	rule.arp.key.out_iface = params->out_iface;
	rule.arp.key.nh_ip =
		rte_bswap32((uint32_t) params->nh_ip.addr.ipv4.s_addr);
	rule.arp.nh_arp = params->nh_arp;

	/* Check rule existence */
	IS_RULE_PRESENT(old_rule, rule.arp.key, arp_table, arp);
	if ((old_rule == NULL) && (n_arp_rules == app.max_arp_rules)) {
		printf("ARP table is full.\n");
		return;
	}

	printf("Adding ARP entry: ");
	print_arp_rule(rule.arp);

	/* Allocate message buffer */
	msg = (void *)rte_ctrlmbuf_alloc(app.msg_pool);
	if (msg == NULL)
		rte_panic("Unable to allocate new message\n");

	/* Fill request message */
	req = (struct app_msg_req *) ((struct rte_mbuf *)msg)->ctrl.data;
	req->type = APP_MSG_REQ_ARP_ADD;
	req->arp_add.out_iface = rule.arp.key.out_iface;
	req->arp_add.nh_ip = rule.arp.key.nh_ip;
	req->arp_add.nh_arp = rule.arp.nh_arp;

	/* Send request */
	do {
		status = rte_ring_sp_enqueue(ring_req, msg);
	} while (status == -ENOBUFS);

	/* Wait for response */
	do {
		status = rte_ring_sc_dequeue(ring_resp, &msg);
	} while (status != 0);
	resp = (struct app_msg_resp *) ((struct rte_mbuf *)msg)->ctrl.data;

	/* Check response */
	if (resp->result != 0)
		printf("Request ARP_ADD failed (%u)\n", resp->result);
	else {
		if (old_rule == NULL) {
			struct app_rule *new_rule = (struct app_rule *)
				rte_zmalloc_socket("CLI",
				sizeof(struct app_rule),
				CACHE_LINE_SIZE,
				rte_socket_id());

			if (new_rule == NULL)
				rte_panic("Unable to allocate new rule\n");

			memcpy(new_rule, &rule, sizeof(rule));
			TAILQ_INSERT_TAIL(&arp_table, new_rule, entries);
			n_arp_rules++;
		} else
			old_rule->arp.nh_arp = rule.arp.nh_arp;
	}

	/* Free message buffer */
	rte_ctrlmbuf_free((struct rte_mbuf *) msg);
}

cmdline_parse_token_string_t cmd_arp_add_arp_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_add_result, arp_string, "arp");

cmdline_parse_token_string_t cmd_arp_add_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_add_result, add_string, "add");

cmdline_parse_token_num_t cmd_arp_add_out_iface =
	TOKEN_NUM_INITIALIZER(struct cmd_arp_add_result, out_iface, UINT8);

cmdline_parse_token_ipaddr_t cmd_arp_add_nh_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_arp_add_result, nh_ip);

cmdline_parse_token_etheraddr_t cmd_arp_add_nh_arp =
	TOKEN_ETHERADDR_INITIALIZER(struct cmd_arp_add_result, nh_arp);

cmdline_parse_inst_t cmd_arp_add = {
	.f = cmd_arp_add_parsed,
	.data = NULL,
	.help_str = "ARP add",
	.tokens = {
		(void *)&cmd_arp_add_arp_string,
		(void *)&cmd_arp_add_add_string,
		(void *)&cmd_arp_add_out_iface,
		(void *)&cmd_arp_add_nh_ip,
		(void *)&cmd_arp_add_nh_arp,
		NULL,
	},
	};

/* *** ARP - Del *** */
struct cmd_arp_del_result {
	cmdline_fixed_string_t arp_string;
	cmdline_fixed_string_t del_string;
	uint8_t out_iface;
	cmdline_ipaddr_t nh_ip;
};

static void
cmd_arp_del_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_arp_del_result *params = parsed_result;
	struct app_rule rule, *old_rule;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	void *msg;
	int status;

	uint32_t core_id = app_get_first_core_id(APP_CORE_RT);

	if (core_id == RTE_MAX_LCORE) {
		printf("ARP not performed by any CPU core\n");
		return;
	}

	struct rte_ring *ring_req = app_get_ring_req(core_id);
	struct rte_ring *ring_resp = app_get_ring_resp(core_id);

	/* Check params */
	if (params->out_iface > app.n_ports) {
		printf("Illegal value for output interface parameter (%u)\n",
			params->out_iface);
		return;
	}

	/* Create rule */
	memset(&rule, 0, sizeof(rule));
	rule.arp.key.out_iface = params->out_iface;
	rule.arp.key.nh_ip =
		rte_bswap32((uint32_t) params->nh_ip.addr.ipv4.s_addr);

	/* Check rule existence */
	IS_RULE_PRESENT(old_rule, rule.arp.key, arp_table, arp);
	if (old_rule == NULL)
		return;

	printf("Deleting ARP entry: ");
	print_arp_rule(old_rule->arp);

	/* Allocate message buffer */
	msg = (void *)rte_ctrlmbuf_alloc(app.msg_pool);
	if (msg == NULL)
		rte_panic("Unable to allocate new message\n");

	/* Fill request message */
	req = (struct app_msg_req *) ((struct rte_mbuf *)msg)->ctrl.data;
	req->type = APP_MSG_REQ_ARP_DEL;
	req->arp_del.out_iface = rule.arp.key.out_iface;
	req->arp_del.nh_ip = rule.arp.key.nh_ip;

	/* Send request */
	do {
		status = rte_ring_sp_enqueue(ring_req, msg);
	} while (status == -ENOBUFS);

	/* Wait for response */
	do {
		status = rte_ring_sc_dequeue(ring_resp, &msg);
	} while (status != 0);
	resp = (struct app_msg_resp *) ((struct rte_mbuf *)msg)->ctrl.data;

	/* Check response */
	if (resp->result != 0)
		printf("Request ARP_DEL failed (%u)\n", resp->result);
	else {
		TAILQ_REMOVE(&arp_table, old_rule, entries);
		n_arp_rules--;
		rte_free(old_rule);
	}

	/* Free message buffer */
	rte_ctrlmbuf_free((struct rte_mbuf *) msg);
}

cmdline_parse_token_string_t cmd_arp_del_arp_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_del_result, arp_string, "arp");

cmdline_parse_token_string_t cmd_arp_del_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_del_result, del_string, "del");

cmdline_parse_token_num_t cmd_arp_del_out_iface =
	TOKEN_NUM_INITIALIZER(struct cmd_arp_del_result, out_iface, UINT8);

cmdline_parse_token_ipaddr_t cmd_arp_del_nh_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_arp_del_result, nh_ip);

cmdline_parse_inst_t cmd_arp_del = {
	.f = cmd_arp_del_parsed,
	.data = NULL,
	.help_str = "ARP delete",
	.tokens = {
		(void *)&cmd_arp_del_arp_string,
		(void *)&cmd_arp_del_del_string,
		(void *)&cmd_arp_del_out_iface,
		(void *)&cmd_arp_del_nh_ip,
		NULL,
	},
};

/* *** ARP - Print *** */
struct cmd_arp_print_result {
	cmdline_fixed_string_t arp_string;
	cmdline_fixed_string_t print_string;
};

static void
cmd_arp_print_parsed(
	__attribute__((unused)) void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct app_rule *it;

	TAILQ_FOREACH(it, &arp_table, entries) {
		print_arp_rule(it->arp);
	}
}

cmdline_parse_token_string_t cmd_arp_print_arp_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_print_result, arp_string,
	"arp");

cmdline_parse_token_string_t cmd_arp_print_print_string =
	TOKEN_STRING_INITIALIZER(struct cmd_arp_print_result, print_string,
	"ls");

cmdline_parse_inst_t cmd_arp_print = {
	.f = cmd_arp_print_parsed,
	.data = NULL,
	.help_str = "ARP list",
	.tokens = {
		(void *)&cmd_arp_print_arp_string,
		(void *)&cmd_arp_print_print_string,
		NULL,
	},
};

/* *** Routing - Add *** */
struct cmd_route_add_result {
	cmdline_fixed_string_t route_string;
	cmdline_fixed_string_t add_string;
	cmdline_ipaddr_t ip;
	uint8_t depth;
	uint8_t port;
	cmdline_ipaddr_t nh_ip;
};

static void
cmd_route_add_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_route_add_result *params = parsed_result;
	struct app_rule rule, *old_rule;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	void *msg;
	int status;

	uint32_t core_id = app_get_first_core_id(APP_CORE_RT);

	if (core_id == RTE_MAX_LCORE) {
		printf("Routing not performed by any CPU core\n");
		return;
	}

	struct rte_ring *ring_req = app_get_ring_req(core_id);
	struct rte_ring *ring_resp = app_get_ring_resp(core_id);

	/* Check params */
	if ((params->depth == 0) || (params->depth > 32)) {
		printf("Illegal value for depth parameter (%u)\n",
			params->depth);
		return;
	}

	if (params->port >= app.n_ports) {
		printf("Illegal value for port parameter (%u)\n", params->port);
		return;
	}

	/* Create rule */
	memset(&rule, 0, sizeof(rule));
	rule.routing.key.ip = rte_bswap32((uint32_t)
		params->ip.addr.ipv4.s_addr);
	rule.routing.key.depth = params->depth;
	rule.routing.port = params->port;
	rule.routing.nh_ip =
		rte_bswap32((uint32_t) params->nh_ip.addr.ipv4.s_addr);

	/* Check rule existence */
	IS_RULE_PRESENT(old_rule, rule.routing.key, routing_table, routing);
	if ((old_rule == NULL) && (n_routing_rules == app.max_routing_rules)) {
		printf("Routing table is full.\n");
		return;
	}

	printf("Adding route: ");
	print_routing_rule(rule.routing);

	/* Allocate message buffer */
	msg = (void *)rte_ctrlmbuf_alloc(app.msg_pool);
	if (msg == NULL)
		rte_panic("Unable to allocate new message\n");

	/* Fill request message */
	req = (struct app_msg_req *) ((struct rte_mbuf *)msg)->ctrl.data;
	req->type = APP_MSG_REQ_RT_ADD;
	req->routing_add.ip = rule.routing.key.ip;
	req->routing_add.depth = rule.routing.key.depth;
	req->routing_add.port = rule.routing.port;
	req->routing_add.nh_ip = rule.routing.nh_ip;

	/* Send request */
	do {
		status = rte_ring_sp_enqueue(ring_req, msg);
	} while (status == -ENOBUFS);

	/* Wait for response */
	do {
		status = rte_ring_sc_dequeue(ring_resp, &msg);
	} while (status != 0);
	resp = (struct app_msg_resp *) ((struct rte_mbuf *)msg)->ctrl.data;

	/* Check response */
	if (resp->result != 0)
		printf("Request ROUTE_ADD failed (%u)\n", resp->result);
	else {
		if (old_rule == NULL) {
			struct app_rule *new_rule = (struct app_rule *)
				rte_zmalloc_socket("CLI",
				sizeof(struct app_rule),
				CACHE_LINE_SIZE,
				rte_socket_id());

			if (new_rule == NULL)
				rte_panic("Unable to allocate new rule\n");

			memcpy(new_rule, &rule, sizeof(rule));
			TAILQ_INSERT_TAIL(&routing_table, new_rule, entries);
			n_routing_rules++;
		} else {
			old_rule->routing.port = rule.routing.port;
			old_rule->routing.nh_ip = rule.routing.nh_ip;
		}
	}

	/* Free message buffer */
	rte_ctrlmbuf_free((struct rte_mbuf *) msg);
}

cmdline_parse_token_string_t cmd_route_add_route_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_add_result, route_string,
	"route");

cmdline_parse_token_string_t cmd_route_add_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_add_result, add_string,
	"add");

cmdline_parse_token_ipaddr_t cmd_route_add_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_route_add_result, ip);

cmdline_parse_token_num_t cmd_route_add_depth =
	TOKEN_NUM_INITIALIZER(struct cmd_route_add_result, depth, UINT8);

cmdline_parse_token_num_t cmd_route_add_port =
	TOKEN_NUM_INITIALIZER(struct cmd_route_add_result, port, UINT8);

cmdline_parse_token_ipaddr_t cmd_route_add_nh_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_route_add_result, nh_ip);

cmdline_parse_inst_t cmd_route_add = {
	.f = cmd_route_add_parsed,
	.data = NULL,
	.help_str = "Route add",
	.tokens = {
		(void *)&cmd_route_add_route_string,
		(void *)&cmd_route_add_add_string,
		(void *)&cmd_route_add_ip,
		(void *)&cmd_route_add_depth,
		(void *)&cmd_route_add_port,
		(void *)&cmd_route_add_nh_ip,
		NULL,
	},
};

/* *** Routing - Del *** */
struct cmd_route_del_result {
	cmdline_fixed_string_t route_string;
	cmdline_fixed_string_t del_string;
	cmdline_ipaddr_t ip;
	uint8_t depth;
};

static void
cmd_route_del_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_route_del_result *params = parsed_result;
	struct app_rule rule, *old_rule;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	void *msg;
	int status;

	uint32_t core_id = app_get_first_core_id(APP_CORE_RT);

	if (core_id == RTE_MAX_LCORE) {
		printf("Routing not performed by any CPU core\n");
		return;
	}

	struct rte_ring *ring_req = app_get_ring_req(core_id);
	struct rte_ring *ring_resp = app_get_ring_resp(core_id);

	/* Check params */
	if ((params->depth == 0) || (params->depth > 32)) {
		printf("Illegal value for depth parameter (%u)\n",
			params->depth);
		return;
	}

	/* Create rule */
	memset(&rule, 0, sizeof(rule));
	rule.routing.key.ip = rte_bswap32((uint32_t)
		params->ip.addr.ipv4.s_addr);
	rule.routing.key.depth = params->depth;

	/* Check rule existence */
	IS_RULE_PRESENT(old_rule, rule.routing.key, routing_table, routing);
	if (old_rule == NULL)
		return;

	printf("Deleting route: ");
	print_routing_rule(old_rule->routing);

	/* Allocate message buffer */
	msg = (void *)rte_ctrlmbuf_alloc(app.msg_pool);
	if (msg == NULL)
		rte_panic("Unable to allocate new message\n");

	/* Fill request message */
	req = (struct app_msg_req *) ((struct rte_mbuf *)msg)->ctrl.data;
	req->type = APP_MSG_REQ_RT_DEL;
	req->routing_del.ip = rule.routing.key.ip;
	req->routing_del.depth = rule.routing.key.depth;

	/* Send request */
	do {
		status = rte_ring_sp_enqueue(ring_req, msg);
	} while (status == -ENOBUFS);

	/* Wait for response */
	do {
		status = rte_ring_sc_dequeue(ring_resp, &msg);
	} while (status != 0);
	resp = (struct app_msg_resp *) ((struct rte_mbuf *)msg)->ctrl.data;

	/* Check response */
	if (resp->result != 0)
		printf("Request ROUTE_DEL failed %u)\n", resp->result);
	else {
		TAILQ_REMOVE(&routing_table, old_rule, entries);
		rte_free(old_rule);
		n_routing_rules--;
	}

	/* Free message buffer */
	rte_ctrlmbuf_free((struct rte_mbuf *)msg);
}

cmdline_parse_token_string_t cmd_route_del_route_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_del_result, route_string,
	"route");

cmdline_parse_token_string_t cmd_route_del_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_route_del_result, del_string,
	"del");

cmdline_parse_token_ipaddr_t cmd_route_del_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_route_del_result, ip);

cmdline_parse_token_num_t cmd_route_del_depth =
	TOKEN_NUM_INITIALIZER(struct cmd_route_del_result, depth, UINT8);

cmdline_parse_inst_t cmd_route_del = {
	.f = cmd_route_del_parsed,
	.data = NULL,
	.help_str = "Route delete",
	.tokens = {
		(void *)&cmd_route_del_route_string,
		(void *)&cmd_route_del_del_string,
		(void *)&cmd_route_del_ip,
		(void *)&cmd_route_del_depth,
		NULL,
	},
};

/* *** Routing - Print *** */
struct cmd_routing_print_result {
	cmdline_fixed_string_t routing_string;
	cmdline_fixed_string_t print_string;
};

static void
cmd_routing_print_parsed(
	__attribute__((unused)) void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct app_rule *it;

	TAILQ_FOREACH(it, &routing_table, entries) {
		print_routing_rule(it->routing);
	}
}

cmdline_parse_token_string_t cmd_routing_print_routing_string =
	TOKEN_STRING_INITIALIZER(struct cmd_routing_print_result,
	routing_string, "route");

cmdline_parse_token_string_t cmd_routing_print_print_string =
	TOKEN_STRING_INITIALIZER(struct cmd_routing_print_result, print_string,
	"ls");

cmdline_parse_inst_t cmd_routing_print = {
	.f = cmd_routing_print_parsed,
	.data = NULL,
	.help_str = "Route list",
	.tokens = {
		(void *)&cmd_routing_print_routing_string,
		(void *)&cmd_routing_print_print_string,
		NULL,
	},
};

#ifdef RTE_LIBRTE_ACL

/* *** Firewall - Add *** */
struct cmd_firewall_add_result {
	cmdline_fixed_string_t firewall_string;
	cmdline_fixed_string_t add_string;
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
	uint8_t port;
};

static void
cmd_firewall_add_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_firewall_add_result *params = parsed_result;
	struct app_rule rule, *old_rule;
	struct rte_mbuf *msg;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	int status;

	uint32_t core_id = app_get_first_core_id(APP_CORE_FW);

	if (core_id == RTE_MAX_LCORE) {
		printf("Firewall not performed by any CPU core\n");
		return;
	}

	struct rte_ring *ring_req = app_get_ring_req(core_id);
	struct rte_ring *ring_resp = app_get_ring_resp(core_id);

	/* Check params */
	if (params->port >= app.n_ports) {
		printf("Illegal value for port parameter (%u)\n", params->port);
		return;
	}

	/* Create rule */
	memset(&rule, 0, sizeof(rule));
	rule.firewall.priority = params->priority;
	rule.firewall.key.src_ip =
		rte_bswap32((uint32_t)params->src_ip.addr.ipv4.s_addr);
	rule.firewall.key.src_ip_mask = params->src_ip_mask;
	rule.firewall.key.dst_ip =
		rte_bswap32((uint32_t)params->dst_ip.addr.ipv4.s_addr);
	rule.firewall.key.dst_ip_mask = params->dst_ip_mask;
	rule.firewall.key.src_port_from = params->src_port_from;
	rule.firewall.key.src_port_to = params->src_port_to;
	rule.firewall.key.dst_port_from = params->dst_port_from;
	rule.firewall.key.dst_port_to = params->dst_port_to;
	rule.firewall.key.proto = params->proto;
	rule.firewall.key.proto_mask = params->proto_mask;
	rule.firewall.port = params->port;

	/* Check rule existence */
	IS_RULE_PRESENT(old_rule, rule.firewall.key, firewall_table, firewall);
	if ((old_rule == NULL) &&
		(n_firewall_rules == app.max_firewall_rules)) {
		printf("Firewall table is full.\n");
		return;
	}

	printf("Adding firewall rule: ");
	print_firewall_rule(rule.firewall);

	/* Allocate message buffer */
	msg = rte_ctrlmbuf_alloc(app.msg_pool);
	if (msg == NULL)
		rte_panic("Unable to allocate new message\n");

	/* Fill request message */
	req = (struct app_msg_req *) msg->ctrl.data;
	req->type = APP_MSG_REQ_FW_ADD;
	req->firewall_add.add_params.priority = rule.firewall.priority;
	req->firewall_add.add_params.field_value[1].value.u32 =
		rule.firewall.key.src_ip;
	req->firewall_add.add_params.field_value[1].mask_range.u32 =
		rule.firewall.key.src_ip_mask;
	req->firewall_add.add_params.field_value[2].value.u32 =
		rule.firewall.key.dst_ip;
	req->firewall_add.add_params.field_value[2].mask_range.u32 =
		rule.firewall.key.dst_ip_mask;
	req->firewall_add.add_params.field_value[3].value.u16 =
		rule.firewall.key.src_port_from;
	req->firewall_add.add_params.field_value[3].mask_range.u16 =
		rule.firewall.key.src_port_to;
	req->firewall_add.add_params.field_value[4].value.u16 =
		rule.firewall.key.dst_port_from;
	req->firewall_add.add_params.field_value[4].mask_range.u16 =
		rule.firewall.key.dst_port_to;
	req->firewall_add.add_params.field_value[0].value.u8 =
		rule.firewall.key.proto;
	req->firewall_add.add_params.field_value[0].mask_range.u8 =
		rule.firewall.key.proto_mask;
	req->firewall_add.port = rule.firewall.port;

	/* Send request */
	do {
		status = rte_ring_sp_enqueue(ring_req, (void *) msg);
	} while (status == -ENOBUFS);

	/* Wait for response */
	do {
		status = rte_ring_sc_dequeue(ring_resp, (void **) &msg);
	} while (status != 0);
	resp = (struct app_msg_resp *) msg->ctrl.data;

	/* Check response */
	if (resp->result != 0)
		printf("Request FIREWALL_ADD failed (%u)\n", resp->result);
	else {
		if (old_rule == NULL) {
			struct app_rule *new_rule = (struct app_rule *)
				rte_zmalloc_socket("CLI",
				sizeof(struct app_rule),
				CACHE_LINE_SIZE,
				rte_socket_id());

			memcpy(new_rule, &rule, sizeof(rule));
			TAILQ_INSERT_TAIL(&firewall_table, new_rule, entries);
			n_firewall_rules++;
		} else {
			old_rule->firewall.priority = rule.firewall.priority;
			old_rule->firewall.port = rule.firewall.port;
		}
	}

	/* Free message buffer */
	rte_ctrlmbuf_free(msg);
}

cmdline_parse_token_string_t cmd_firewall_add_firewall_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_add_result,
	firewall_string, "firewall");

cmdline_parse_token_string_t cmd_firewall_add_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_add_result, add_string,
	"add");

cmdline_parse_token_num_t cmd_firewall_add_priority =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_result, priority, INT32);

cmdline_parse_token_ipaddr_t cmd_firewall_add_src_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_firewall_add_result, src_ip);
cmdline_parse_token_num_t cmd_firewall_add_src_ip_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_result, src_ip_mask,
	UINT32);

cmdline_parse_token_ipaddr_t cmd_firewall_add_dst_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_firewall_add_result, dst_ip);
cmdline_parse_token_num_t cmd_firewall_add_dst_ip_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_result, dst_ip_mask,
	UINT32);

cmdline_parse_token_num_t cmd_firewall_add_src_port_from =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_result, src_port_from,
	UINT16);
cmdline_parse_token_num_t cmd_firewall_add_src_port_to =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_result, src_port_to,
	UINT16);

cmdline_parse_token_num_t cmd_firewall_add_dst_port_from =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_result, dst_port_from,
	UINT16);
cmdline_parse_token_num_t cmd_firewall_add_dst_port_to =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_result, dst_port_to,
	UINT16);

cmdline_parse_token_num_t cmd_firewall_add_proto =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_result, proto, UINT8);
cmdline_parse_token_num_t cmd_firewall_add_proto_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_result, proto_mask,
	UINT8);
cmdline_parse_token_num_t cmd_firewall_add_port =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_add_result, port, UINT8);

cmdline_parse_inst_t cmd_firewall_add = {
	.f = cmd_firewall_add_parsed,
	.data = NULL,
	.help_str = "Firewall rule add",
	.tokens = {
		(void *)&cmd_firewall_add_firewall_string,
		(void *)&cmd_firewall_add_add_string,
		(void *)&cmd_firewall_add_priority,
		(void *)&cmd_firewall_add_src_ip,
		(void *)&cmd_firewall_add_src_ip_mask,
		(void *)&cmd_firewall_add_dst_ip,
		(void *)&cmd_firewall_add_dst_ip_mask,
		(void *)&cmd_firewall_add_src_port_from,
		(void *)&cmd_firewall_add_src_port_to,
		(void *)&cmd_firewall_add_dst_port_from,
		(void *)&cmd_firewall_add_dst_port_to,
		(void *)&cmd_firewall_add_proto,
		(void *)&cmd_firewall_add_proto_mask,
		(void *)&cmd_firewall_add_port,
		NULL,
	},
};

/* *** firewall - Del *** */
struct cmd_firewall_del_result {
	cmdline_fixed_string_t firewall_string;
	cmdline_fixed_string_t del_string;
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
cmd_firewall_del_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_firewall_del_result *params = parsed_result;
	struct app_rule rule, *old_rule;
	struct rte_mbuf *msg;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	int status;

	uint32_t core_id = app_get_first_core_id(APP_CORE_FW);

	if (core_id == RTE_MAX_LCORE) {
		printf("Firewall not performed by any CPU core\n");
		return;
	}

	struct rte_ring *ring_req = app_get_ring_req(core_id);
	struct rte_ring *ring_resp = app_get_ring_resp(core_id);

	/* Check params */

	/* Create rule */
	memset(&rule, 0, sizeof(rule));
	rule.firewall.key.src_ip =
		rte_bswap32((uint32_t) params->src_ip.addr.ipv4.s_addr);
	rule.firewall.key.src_ip_mask = params->src_ip_mask;
	rule.firewall.key.dst_ip =
		rte_bswap32((uint32_t) params->dst_ip.addr.ipv4.s_addr);
	rule.firewall.key.dst_ip_mask = params->dst_ip_mask;
	rule.firewall.key.src_port_from = params->src_port_from;
	rule.firewall.key.src_port_to = params->src_port_to;
	rule.firewall.key.dst_port_from = params->dst_port_from;
	rule.firewall.key.dst_port_to = params->dst_port_to;
	rule.firewall.key.proto = params->proto;
	rule.firewall.key.proto_mask = params->proto_mask;

	/* Check rule existence */
	IS_RULE_PRESENT(old_rule, rule.firewall.key, firewall_table, firewall);
	if (old_rule == NULL)
		return;

	printf("Deleting firewall rule: ");
	print_firewall_rule(old_rule->firewall);

	/* Allocate message buffer */
	msg = rte_ctrlmbuf_alloc(app.msg_pool);
	if (msg == NULL)
		rte_panic("Unable to allocate new message\n");

	/* Fill request message */
	req = (struct app_msg_req *) msg->ctrl.data;
	memset(&req->firewall_del, 0, sizeof(req->firewall_del));
	req->type = APP_MSG_REQ_FW_DEL;
	req->firewall_del.delete_params.field_value[1].value.u32 =
		rule.firewall.key.src_ip;
	req->firewall_del.delete_params.field_value[1].mask_range.u32 =
		rule.firewall.key.src_ip_mask;
	req->firewall_del.delete_params.field_value[2].value.u32 =
		rule.firewall.key.dst_ip;
	req->firewall_del.delete_params.field_value[2].mask_range.u32 =
		rule.firewall.key.dst_ip_mask;
	req->firewall_del.delete_params.field_value[3].value.u16 =
		rule.firewall.key.src_port_from;
	req->firewall_del.delete_params.field_value[3].mask_range.u16 =
		rule.firewall.key.src_port_to;
	req->firewall_del.delete_params.field_value[4].value.u16 =
		rule.firewall.key.dst_port_from;
	req->firewall_del.delete_params.field_value[4].mask_range.u16 =
		rule.firewall.key.dst_port_to;
	req->firewall_del.delete_params.field_value[0].value.u8 =
		rule.firewall.key.proto;
	req->firewall_del.delete_params.field_value[0].mask_range.u8 =
		rule.firewall.key.proto_mask;

	/* Send request */
	do {
		status = rte_ring_sp_enqueue(ring_req, (void *) msg);
	} while (status == -ENOBUFS);

	/* Wait for response */
	do {
		status = rte_ring_sc_dequeue(ring_resp, (void **) &msg);
	} while (status != 0);
	resp = (struct app_msg_resp *) msg->ctrl.data;

	/* Check response */
	if (resp->result != 0)
		printf("Request FIREWALL_DEL failed %u)\n", resp->result);
	else {
		TAILQ_REMOVE(&firewall_table, old_rule, entries);
		rte_free(old_rule);
		n_firewall_rules--;
	}

	/* Free message buffer */
	rte_ctrlmbuf_free(msg);
}

cmdline_parse_token_string_t cmd_firewall_del_firewall_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_del_result,
	firewall_string, "firewall");

cmdline_parse_token_string_t cmd_firewall_del_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_del_result, del_string,
	"del");

cmdline_parse_token_ipaddr_t cmd_firewall_del_src_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_firewall_del_result, src_ip);
cmdline_parse_token_num_t cmd_firewall_del_src_ip_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_result, src_ip_mask,
	UINT32);

cmdline_parse_token_ipaddr_t cmd_firewall_del_dst_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_firewall_del_result, dst_ip);
cmdline_parse_token_num_t cmd_firewall_del_dst_ip_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_result, dst_ip_mask,
	UINT32);

cmdline_parse_token_num_t cmd_firewall_del_src_port_from =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_result, src_port_from,
	UINT16);
cmdline_parse_token_num_t cmd_firewall_del_src_port_to =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_result, src_port_to,
	UINT16);

cmdline_parse_token_num_t cmd_firewall_del_dst_port_from =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_result, dst_port_from,
	UINT16);
cmdline_parse_token_num_t cmd_firewall_del_dst_port_to =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_result, dst_port_to,
	UINT16);

cmdline_parse_token_num_t cmd_firewall_del_proto =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_result, proto, UINT8);
cmdline_parse_token_num_t cmd_firewall_del_proto_mask =
	TOKEN_NUM_INITIALIZER(struct cmd_firewall_del_result, proto_mask,
	UINT8);

cmdline_parse_inst_t cmd_firewall_del = {
	.f = cmd_firewall_del_parsed,
	.data = NULL,
	.help_str = "Firewall rule delete",
	.tokens = {
		(void *)&cmd_firewall_del_firewall_string,
		(void *)&cmd_firewall_del_del_string,
		(void *)&cmd_firewall_del_src_ip,
		(void *)&cmd_firewall_del_src_ip_mask,
		(void *)&cmd_firewall_del_dst_ip,
		(void *)&cmd_firewall_del_dst_ip_mask,
		(void *)&cmd_firewall_del_src_port_from,
		(void *)&cmd_firewall_del_src_port_to,
		(void *)&cmd_firewall_del_dst_port_from,
		(void *)&cmd_firewall_del_dst_port_to,
		(void *)&cmd_firewall_del_proto,
		(void *)&cmd_firewall_del_proto_mask,
		NULL,
	},
};

/* *** Firewall - Print *** */
struct cmd_firewall_print_result {
	cmdline_fixed_string_t firewall_string;
	cmdline_fixed_string_t print_string;
};

static void
cmd_firewall_print_parsed(
	__attribute__((unused)) void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct app_rule *it;

	TAILQ_FOREACH(it, &firewall_table, entries) {
		print_firewall_rule(it->firewall);
	}
}

cmdline_parse_token_string_t cmd_firewall_print_firewall_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_print_result,
	firewall_string, "firewall");

cmdline_parse_token_string_t cmd_firewall_print_print_string =
	TOKEN_STRING_INITIALIZER(struct cmd_firewall_print_result, print_string,
	"ls");

cmdline_parse_inst_t cmd_firewall_print = {
	.f = cmd_firewall_print_parsed,
	.data = NULL,
	.help_str = "Firewall rules list",
	.tokens = {
		(void *)&cmd_firewall_print_firewall_string,
		(void *)&cmd_firewall_print_print_string,
		NULL,
	},
};

#endif

/* *** Flow Classification - Add All *** */
struct cmd_flow_add_all_result {
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t add_string;
	cmdline_fixed_string_t all_string;
};

static void
cmd_flow_add_all_parsed(
	__attribute__((unused)) void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	void *msg;
	int status;

	struct rte_ring *ring_req =
		app_get_ring_req(app_get_first_core_id(APP_CORE_FC));
	struct rte_ring *ring_resp =
		app_get_ring_resp(app_get_first_core_id(APP_CORE_FC));

	/* Allocate message buffer */
	msg = (void *)rte_ctrlmbuf_alloc(app.msg_pool);
	if (msg == NULL)
		rte_panic("Unable to allocate new message\n");

	/* Fill request message */
	req = (struct app_msg_req *) ((struct rte_mbuf *)msg)->ctrl.data;
	memset(req, 0, sizeof(struct app_msg_req));

	req->type = APP_MSG_REQ_FC_ADD_ALL;

	/* Send request */
	do {
		status = rte_ring_sp_enqueue(ring_req, msg);
	} while (status == -ENOBUFS);

	/* Wait for response */
	do {
		status = rte_ring_sc_dequeue(ring_resp, &msg);
	} while (status != 0);
	resp = (struct app_msg_resp *) ((struct rte_mbuf *)msg)->ctrl.data;

	/* Check response */
	if (resp->result != 0)
		printf("Request FLOW_ADD_ALL failed (%u)\n", resp->result);

	/* Free message buffer */
	rte_ctrlmbuf_free((struct rte_mbuf *)msg);
}

cmdline_parse_token_string_t cmd_flow_add_all_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_add_all_result, flow_string,
	"flow");

cmdline_parse_token_string_t cmd_flow_add_all_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_add_all_result, add_string,
	"add");

cmdline_parse_token_string_t cmd_flow_add_all_all_string =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_add_all_result, all_string,
	"all");

cmdline_parse_inst_t cmd_flow_add_all = {
	.f = cmd_flow_add_all_parsed,
	.data = NULL,
	.help_str = "Flow table initialization based on hard-coded rule",
	.tokens = {
		(void *)&cmd_flow_add_all_flow_string,
		(void *)&cmd_flow_add_all_add_string,
		(void *)&cmd_flow_add_all_all_string,
		NULL,
	},
};

/* *** Flow Classification - Add *** */
struct cmd_flow_add_result {
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t add_string;
	cmdline_ipaddr_t src_ip;
	cmdline_ipaddr_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t proto;
	uint8_t port;
};

static void
cmd_flow_add_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_flow_add_result *params = parsed_result;
	struct app_rule rule, *old_rule;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	void *msg;
	int status;

	uint32_t core_id = app_get_first_core_id(APP_CORE_FC);

	if (core_id == RTE_MAX_LCORE) {
		printf("Flow classification not performed by any CPU core\n");
		return;
	}

	struct rte_ring *ring_req = app_get_ring_req(core_id);
	struct rte_ring *ring_resp = app_get_ring_resp(core_id);

	/* Check params */
	if (params->port >= app.n_ports) {
		printf("Illegal value for port parameter (%u)\n", params->port);
		return;
	}

	/* Create rule */
	memset(&rule, 0, sizeof(rule));
	rule.flow.key.src_ip =
		rte_bswap32((uint32_t)params->src_ip.addr.ipv4.s_addr);
	rule.flow.key.dst_ip =
		rte_bswap32((uint32_t)params->dst_ip.addr.ipv4.s_addr);
	rule.flow.key.src_port = params->src_port;
	rule.flow.key.dst_port = params->dst_port;
	rule.flow.key.proto = params->proto;
	rule.flow.port = params->port;

	/* Check rule existence */
	IS_RULE_PRESENT(old_rule, rule.flow.key, flow_table, flow);
	if ((old_rule == NULL) && (n_flow_rules == app.max_flow_rules)) {
		printf("Flow table is full.\n");
		return;
	}

	printf("Adding flow: ");
	print_flow_rule(rule.flow);

	/* Allocate message buffer */
	msg = (void *)rte_ctrlmbuf_alloc(app.msg_pool);
	if (msg == NULL)
		rte_panic("Unable to allocate new message\n");

	/* Fill request message */
	req = (struct app_msg_req *) ((struct rte_mbuf *)msg)->ctrl.data;
	memset(req, 0, sizeof(struct app_msg_req));

	req->type = APP_MSG_REQ_FC_ADD;
	req->flow_classif_add.key.ip_src = rte_bswap32(rule.flow.key.src_ip);
	req->flow_classif_add.key.ip_dst = rte_bswap32(rule.flow.key.dst_ip);
	req->flow_classif_add.key.port_src =
		rte_bswap16(rule.flow.key.src_port);
	req->flow_classif_add.key.port_dst =
		rte_bswap16(rule.flow.key.dst_port);
	req->flow_classif_add.key.proto = rule.flow.key.proto;
	req->flow_classif_add.port = rule.flow.port;

	/* Send request */
	do {
		status = rte_ring_sp_enqueue(ring_req, msg);
	} while (status == -ENOBUFS);

	/* Wait for response */
	do {
		status = rte_ring_sc_dequeue(ring_resp, &msg);
	} while (status != 0);
	resp = (struct app_msg_resp *) ((struct rte_mbuf *)msg)->ctrl.data;

	/* Check response */
	if (resp->result != 0)
		printf("Request FLOW_ADD failed (%u)\n", resp->result);
	else {
		if (old_rule == NULL) {
			struct app_rule *new_rule = (struct app_rule *)
				rte_zmalloc_socket("CLI",
				sizeof(struct app_rule),
				CACHE_LINE_SIZE,
				rte_socket_id());

			if (new_rule == NULL)
				rte_panic("Unable to allocate new rule\n");

			memcpy(new_rule, &rule, sizeof(rule));
			TAILQ_INSERT_TAIL(&flow_table, new_rule, entries);
			n_flow_rules++;
		} else
			old_rule->flow.port = rule.flow.port;
	}

	/* Free message buffer */
	rte_ctrlmbuf_free((struct rte_mbuf *)msg);
}

cmdline_parse_token_string_t cmd_flow_add_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_add_result, flow_string,
	"flow");

cmdline_parse_token_string_t cmd_flow_add_add_string =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_add_result, add_string, "add");

cmdline_parse_token_ipaddr_t cmd_flow_add_src_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_flow_add_result, src_ip);

cmdline_parse_token_ipaddr_t cmd_flow_add_dst_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_flow_add_result, dst_ip);

cmdline_parse_token_num_t cmd_flow_add_src_port =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_add_result, src_port, UINT16);

cmdline_parse_token_num_t cmd_flow_add_dst_port =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_add_result, dst_port, UINT16);

cmdline_parse_token_num_t cmd_flow_add_proto =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_add_result, proto, UINT8);

cmdline_parse_token_num_t cmd_flow_add_port =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_add_result, port, UINT8);

cmdline_parse_inst_t cmd_flow_add = {
	.f = cmd_flow_add_parsed,
	.data = NULL,
	.help_str = "Flow add",
	.tokens = {
		(void *)&cmd_flow_add_flow_string,
		(void *)&cmd_flow_add_add_string,
		(void *)&cmd_flow_add_src_ip,
		(void *)&cmd_flow_add_dst_ip,
		(void *)&cmd_flow_add_src_port,
		(void *)&cmd_flow_add_dst_port,
		(void *)&cmd_flow_add_proto,
		(void *)&cmd_flow_add_port,
		NULL,
	},
};

/* *** Flow Classification - Del *** */
struct cmd_flow_del_result {
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t del_string;
	cmdline_ipaddr_t src_ip;
	cmdline_ipaddr_t dst_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t proto;
};

static void
cmd_flow_del_parsed(
	void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct cmd_flow_del_result *params = parsed_result;
	struct app_rule rule, *old_rule;
	struct app_msg_req *req;
	struct app_msg_resp *resp;
	void *msg;
	int status;

	uint32_t core_id = app_get_first_core_id(APP_CORE_FC);

	if (core_id == RTE_MAX_LCORE) {
		printf("Flow classification not performed by any CPU core.\n");
		return;
	}

	struct rte_ring *ring_req = app_get_ring_req(core_id);
	struct rte_ring *ring_resp = app_get_ring_resp(core_id);

	/* Create rule */
	memset(&rule, 0, sizeof(rule));
	rule.flow.key.src_ip =
		rte_bswap32((uint32_t)params->src_ip.addr.ipv4.s_addr);
	rule.flow.key.dst_ip =
		rte_bswap32((uint32_t)params->dst_ip.addr.ipv4.s_addr);
	rule.flow.key.src_port = params->src_port;
	rule.flow.key.dst_port = params->dst_port;
	rule.flow.key.proto = params->proto;

	/* Check rule existence */
	IS_RULE_PRESENT(old_rule, rule.flow.key, flow_table, flow);
	if (old_rule == NULL)
		return;

	printf("Deleting flow: ");
	print_flow_rule(old_rule->flow);

	/* Allocate message buffer */
	msg = (void *)rte_ctrlmbuf_alloc(app.msg_pool);
	if (msg == NULL)
		rte_panic("Unable to allocate new message\n");

	/* Fill request message */
	req = (struct app_msg_req *) ((struct rte_mbuf *)msg)->ctrl.data;
	memset(req, 0, sizeof(struct app_msg_req));

	req->type = APP_MSG_REQ_FC_DEL;
	req->flow_classif_del.key.ip_src = rte_bswap32(rule.flow.key.src_ip);
	req->flow_classif_del.key.ip_dst = rte_bswap32(rule.flow.key.dst_ip);
	req->flow_classif_del.key.port_src =
		rte_bswap32(rule.flow.key.src_port);
	req->flow_classif_del.key.port_dst =
		rte_bswap32(rule.flow.key.dst_port);
	req->flow_classif_del.key.proto = rule.flow.key.proto;

	/* Send request */
	do {
		status = rte_ring_sp_enqueue(ring_req, msg);
	} while (status == -ENOBUFS);

	/* Wait for response */
	do {
		status = rte_ring_sc_dequeue(ring_resp, &msg);
	} while (status != 0);
	resp = (struct app_msg_resp *) ((struct rte_mbuf *)msg)->ctrl.data;

	/* Check response */
	if (resp->result != 0)
		printf("Request FLOW_DEL failed (%u)\n", resp->result);
	else {
		TAILQ_REMOVE(&flow_table, old_rule, entries);
		rte_free(old_rule);
		n_flow_rules--;
	}

	/* Free message buffer */
	rte_ctrlmbuf_free((struct rte_mbuf *)msg);
}

cmdline_parse_token_string_t cmd_flow_del_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_del_result, flow_string,
	"flow");

cmdline_parse_token_string_t cmd_flow_del_del_string =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_del_result, del_string, "del");

cmdline_parse_token_ipaddr_t cmd_flow_del_src_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_flow_del_result, src_ip);

cmdline_parse_token_ipaddr_t cmd_flow_del_dst_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_flow_del_result, dst_ip);

cmdline_parse_token_num_t cmd_flow_del_src_port =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_del_result, src_port, UINT16);

cmdline_parse_token_num_t cmd_flow_del_dst_port =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_del_result, dst_port, UINT16);

cmdline_parse_token_num_t cmd_flow_del_proto =
	TOKEN_NUM_INITIALIZER(struct cmd_flow_del_result, proto, UINT8);

cmdline_parse_inst_t cmd_flow_del = {
	.f = cmd_flow_del_parsed,
	.data = NULL,
	.help_str = "Flow delete",
	.tokens = {
		(void *)&cmd_flow_del_flow_string,
		(void *)&cmd_flow_del_del_string,
		(void *)&cmd_flow_del_src_ip,
		(void *)&cmd_flow_del_dst_ip,
		(void *)&cmd_flow_del_src_port,
		(void *)&cmd_flow_del_dst_port,
		(void *)&cmd_flow_del_proto,
		NULL,
	},
};

/* *** Flow Classification - Print *** */
struct cmd_flow_print_result {
	cmdline_fixed_string_t flow_string;
	cmdline_fixed_string_t print_string;
};

static void
cmd_flow_print_parsed(
	__attribute__((unused)) void *parsed_result,
	__attribute__((unused)) struct cmdline *cl,
	__attribute__((unused)) void *data)
{
	struct app_rule *it;

	TAILQ_FOREACH(it, &flow_table, entries) {
		print_flow_rule(it->flow);
	}
}

cmdline_parse_token_string_t cmd_flow_print_flow_string =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_print_result, flow_string,
	"flow");

cmdline_parse_token_string_t cmd_flow_print_print_string =
	TOKEN_STRING_INITIALIZER(struct cmd_flow_print_result, print_string,
	"ls");

cmdline_parse_inst_t cmd_flow_print = {
	.f = cmd_flow_print_parsed,
	.data = NULL,
	.help_str = "Flow list",
	.tokens = {
		(void *)&cmd_flow_print_flow_string,
		(void *)&cmd_flow_print_print_string,
		NULL,
	},
};

/* *** QUIT *** */
struct cmd_quit_result {
	cmdline_fixed_string_t quit;
};

static void cmd_quit_parsed(__attribute__((unused)) void *parsed_result,
		struct cmdline *cl,
		__attribute__((unused)) void *data)
{
	cmdline_quit(cl);
}

cmdline_parse_token_string_t cmd_quit_quit =
		TOKEN_STRING_INITIALIZER(struct cmd_quit_result, quit, "quit");

cmdline_parse_inst_t cmd_quit = {
	.f = cmd_quit_parsed,
	.data = NULL,
	.help_str = "Exit application",
	.tokens = {
		(void *)&cmd_quit_quit,
		NULL,
	},
};

/* List of commands */
cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_flow_add,
	(cmdline_parse_inst_t *)&cmd_flow_del,
	(cmdline_parse_inst_t *)&cmd_flow_add_all,
	(cmdline_parse_inst_t *)&cmd_flow_print,
#ifdef RTE_LIBRTE_ACL
	(cmdline_parse_inst_t *)&cmd_firewall_add,
	(cmdline_parse_inst_t *)&cmd_firewall_del,
	(cmdline_parse_inst_t *)&cmd_firewall_print,
#endif
	(cmdline_parse_inst_t *)&cmd_route_add,
	(cmdline_parse_inst_t *)&cmd_route_del,
	(cmdline_parse_inst_t *)&cmd_routing_print,
	(cmdline_parse_inst_t *)&cmd_arp_add,
	(cmdline_parse_inst_t *)&cmd_arp_del,
	(cmdline_parse_inst_t *)&cmd_arp_print,
	(cmdline_parse_inst_t *)&cmd_run_file,
	(cmdline_parse_inst_t *)&cmd_link_enable,
	(cmdline_parse_inst_t *)&cmd_link_disable,
	(cmdline_parse_inst_t *)&cmd_quit,
	NULL,
};

/* Main loop */
void
app_main_loop_cmdline(void)
{
	struct cmdline *cl;
	uint32_t core_id = rte_lcore_id();

	RTE_LOG(INFO, USER1, "Core %u is running the command line interface\n",
		core_id);

	n_arp_rules = 0;
	n_routing_rules = 0;
	n_firewall_rules = 0;
	n_flow_rules = 0;

	app_init_rule_tables();

	cl = cmdline_stdin_new(main_ctx, "pipeline> ");
	if (cl == NULL)
		return;
	cmdline_interact(cl);
	cmdline_stdin_exit(cl);
}
