/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <rte_node_ip4_api.h>

#include "module_api.h"
#include "route_priv.h"

static const char
cmd_ipv4_lookup_help[] = "ipv4_lookup route add ipv4 <ip> netmask <mask> via <ip>";

struct ip4_route route4 = TAILQ_HEAD_INITIALIZER(route4);


void
route_ip4_list_clean(void)
{
	struct route_ipv4_config *route;

	while (!TAILQ_EMPTY(&route4)) {
		route = TAILQ_FIRST(&route4);
		TAILQ_REMOVE(&route4, route, next);
	}
}

static struct route_ipv4_config *
find_route4_entry(struct route_ipv4_config *route)
{
	struct route_ipv4_config *ipv4route;

	TAILQ_FOREACH(ipv4route, &route4, next) {
		if (!memcmp(ipv4route, route, sizeof(*route)))
			return ipv4route;
	}
	return NULL;

}

static uint8_t
convert_netmask_to_depth(uint32_t netmask)
{
	uint8_t zerobits = 0;

	while ((netmask & 0x1) == 0) {
		netmask = netmask >> 1;
		zerobits++;
	}

	return (32 - zerobits);
}

static int
route4_rewirte_table_update(struct route_ipv4_config *ipv4route)
{
	uint8_t depth;
	int portid;

	portid = ethdev_portid_by_ip4(ipv4route->via, ipv4route->netmask);
	if (portid < 0) {
		printf("Invalid portid found to install the route\n");
		return portid;
	}

	depth = convert_netmask_to_depth(ipv4route->netmask);

	return rte_node_ip4_route_add(ipv4route->ip, depth, portid,
			RTE_NODE_IP4_LOOKUP_NEXT_REWRITE);
}

static int
route_ip4_add(struct route_ipv4_config *route)
{
	struct route_ipv4_config *ipv4route;
	int rc = -EINVAL;

	ipv4route = find_route4_entry(route);

	if (!ipv4route) {
		ipv4route = malloc(sizeof(struct route_ipv4_config));
		if (!ipv4route)
			return -ENOMEM;
	} else {
		return 0;
	}

	ipv4route->ip = route->ip;
	ipv4route->netmask = route->netmask;
	ipv4route->via = route->via;
	ipv4route->is_used = true;

	if (!graph_status_get())
		goto exit;

	rc = route4_rewirte_table_update(ipv4route);
	if (rc)
		goto free;

exit:
	TAILQ_INSERT_TAIL(&route4, ipv4route, next);
	return 0;
free:
	free(ipv4route);
	return rc;
}

int
route_ip4_add_to_lookup(void)
{
	struct route_ipv4_config *route = NULL;
	int rc = -EINVAL;

	TAILQ_FOREACH(route, &route4, next) {
		rc = route4_rewirte_table_update(route);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static void
cli_ipv4_lookup_help(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
		     __rte_unused void *data)
{
	size_t len;

	len = strlen(conn->msg_out);
	conn->msg_out += len;
	snprintf(conn->msg_out, conn->msg_out_len_max, "\n%s\n%s\n",
		 "--------------------------- ipv4_lookup command help ---------------------------",
		 cmd_ipv4_lookup_help);

	len = strlen(conn->msg_out);
	conn->msg_out_len_max -= len;
}

static void
cli_ipv4_lookup(void *parsed_result, __rte_unused struct cmdline *cl, void *data __rte_unused)
{
	struct ip4_lookup_cmd_tokens *res = parsed_result;
	struct route_ipv4_config config;
	int rc = -EINVAL;

	if (parser_ip4_read(&config.ip, res->ip)) {
		printf(MSG_ARG_INVALID, "ipv4");
		return;
	}

	if (parser_ip4_read(&config.netmask, res->mask)) {
		printf(MSG_ARG_INVALID, "netmask");
		return;
	}

	if (parser_ip4_read(&config.via, res->via_ip)) {
		printf(MSG_ARG_INVALID, "via ip");
		return;
	}

	rc = route_ip4_add(&config);
	if (rc < 0)
		printf(MSG_CMD_FAIL, res->cmd);
}

cmdline_parse_token_string_t ip4_lookup_cmd =
	TOKEN_STRING_INITIALIZER(struct ip4_lookup_cmd_tokens, cmd, "ipv4_lookup");
cmdline_parse_token_string_t ip4_lookup_route =
	TOKEN_STRING_INITIALIZER(struct ip4_lookup_cmd_tokens, route, "route");
cmdline_parse_token_string_t ip4_lookup_add =
	TOKEN_STRING_INITIALIZER(struct ip4_lookup_cmd_tokens, add, "add");
cmdline_parse_token_string_t ip4_lookup_ip4 =
	TOKEN_STRING_INITIALIZER(struct ip4_lookup_cmd_tokens, ip4, "ipv4");
cmdline_parse_token_string_t ip4_lookup_ip =
	TOKEN_STRING_INITIALIZER(struct ip4_lookup_cmd_tokens, ip, NULL);
cmdline_parse_token_string_t ip4_lookup_netmask =
	TOKEN_STRING_INITIALIZER(struct ip4_lookup_cmd_tokens, netmask, "netmask");
cmdline_parse_token_string_t ip4_lookup_mask =
	TOKEN_STRING_INITIALIZER(struct ip4_lookup_cmd_tokens, mask, NULL);
cmdline_parse_token_string_t ip4_lookup_via =
	TOKEN_STRING_INITIALIZER(struct ip4_lookup_cmd_tokens, via, "via");
cmdline_parse_token_string_t ip4_lookup_via_ip =
	TOKEN_STRING_INITIALIZER(struct ip4_lookup_cmd_tokens, via_ip, NULL);

cmdline_parse_inst_t ipv4_lookup_cmd_ctx = {
	.f = cli_ipv4_lookup,
	.data = NULL,
	.help_str = cmd_ipv4_lookup_help,
	.tokens = {
		(void *)&ip4_lookup_cmd,
		(void *)&ip4_lookup_route,
		(void *)&ip4_lookup_add,
		(void *)&ip4_lookup_ip4,
		(void *)&ip4_lookup_ip,
		(void *)&ip4_lookup_netmask,
		(void *)&ip4_lookup_mask,
		(void *)&ip4_lookup_via,
		(void *)&ip4_lookup_via_ip,
		NULL,
	},
};

cmdline_parse_token_string_t ipv4_lookup_help_cmd =
	TOKEN_STRING_INITIALIZER(struct ipv4_lookup_help_cmd_tokens, cmd, "help");
cmdline_parse_token_string_t ipv4_lookup_help_module =
	TOKEN_STRING_INITIALIZER(struct ipv4_lookup_help_cmd_tokens, module, "ipv4_lookup");

cmdline_parse_inst_t ipv4_lookup_help_cmd_ctx = {
	.f = cli_ipv4_lookup_help,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *)&ipv4_lookup_help_cmd,
		(void *)&ipv4_lookup_help_module,
		NULL,
	},
};
