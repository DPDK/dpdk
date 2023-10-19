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

#include <rte_node_ip6_api.h>

#include "module_api.h"
#include "route_priv.h"

static const char
cmd_ipv6_lookup_help[] = "ipv6_lookup route add ipv6 <ip> netmask <mask> via <ip>";

struct ip6_route route6 = TAILQ_HEAD_INITIALIZER(route6);

void
route_ip6_list_clean(void)
{
	struct route_ipv6_config *route;

	while (!TAILQ_EMPTY(&route6)) {
		route = TAILQ_FIRST(&route6);
		TAILQ_REMOVE(&route6, route, next);
	}
}

static struct route_ipv6_config *
find_route6_entry(struct route_ipv6_config *route)
{
	struct route_ipv6_config *ipv6route;

	TAILQ_FOREACH(ipv6route, &route6, next) {
		if (!memcmp(ipv6route, route, sizeof(*route)))
			return ipv6route;
	}
	return NULL;
}

static uint8_t
convert_ip6_netmask_to_depth(uint8_t *netmask)
{
	uint8_t setbits = 0;
	uint8_t mask;
	int i;

	for (i = 0; i < ETHDEV_IPV6_ADDR_LEN; i++) {
		mask = netmask[i];
		while (mask & 0x80) {
			mask = mask << 1;
			setbits++;
		}
	}

	return setbits;
}

static int
route6_rewirte_table_update(struct route_ipv6_config *ipv6route)
{
	uint8_t depth;
	int portid;

	portid = ethdev_portid_by_ip6(ipv6route->gateway, ipv6route->mask);
	if (portid < 0) {
		printf("Invalid portid found to install the route\n");
		return portid;
	}
	depth = convert_ip6_netmask_to_depth(ipv6route->mask);

	return rte_node_ip6_route_add(ipv6route->ip, depth, portid,
			RTE_NODE_IP6_LOOKUP_NEXT_REWRITE);

}

static int
route_ip6_add(struct route_ipv6_config *route)
{
	struct route_ipv6_config *ipv6route;
	int rc = -EINVAL;
	int j;

	ipv6route = find_route6_entry(route);
	if (!ipv6route) {
		ipv6route = malloc(sizeof(struct route_ipv6_config));
		if (!ipv6route)
			return -ENOMEM;
	} else {
		return 0;
	}

	for (j = 0; j < ETHDEV_IPV6_ADDR_LEN; j++) {
		ipv6route->ip[j] = route->ip[j];
		ipv6route->mask[j] = route->mask[j];
		ipv6route->gateway[j] = route->gateway[j];
	}
	ipv6route->is_used = true;

	if (!graph_status_get())
		goto exit;

	rc = route6_rewirte_table_update(ipv6route);
	if (rc)
		goto free;

exit:
	TAILQ_INSERT_TAIL(&route6, ipv6route, next);
	return 0;
free:
	free(ipv6route);
	return rc;
}

int
route_ip6_add_to_lookup(void)
{
	struct route_ipv6_config *route = NULL;
	int rc = -EINVAL;

	TAILQ_FOREACH(route, &route6, next) {
		rc = route6_rewirte_table_update(route);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static void
cli_ipv6_lookup_help(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
		     __rte_unused void *data)
{
	size_t len;

	len = strlen(conn->msg_out);
	conn->msg_out += len;
	snprintf(conn->msg_out, conn->msg_out_len_max, "\n%s\n%s\n",
		 "--------------------------- ipv6_lookup command help ---------------------------",
		 cmd_ipv6_lookup_help);

	len = strlen(conn->msg_out);
	conn->msg_out_len_max -= len;
}

static void
cli_ipv6_lookup(void *parsed_result, __rte_unused struct cmdline *cl, void *data __rte_unused)
{
	struct ip6_lookup_cmd_tokens *res = parsed_result;
	struct route_ipv6_config config;
	int rc = -EINVAL;

	if (parser_ip6_read(config.ip, res->ip)) {
		printf(MSG_ARG_INVALID, "ipv6");
		return;
	}

	if (parser_ip6_read(config.mask, res->mask)) {
		printf(MSG_ARG_INVALID, "netmask");
		return;
	}

	if (parser_ip6_read(config.gateway, res->via_ip)) {
		printf(MSG_ARG_INVALID, "gateway ip");
		return;
	}

	rc = route_ip6_add(&config);
	if (rc)
		printf(MSG_CMD_FAIL, res->cmd);
}

cmdline_parse_token_string_t ip6_lookup_cmd =
	TOKEN_STRING_INITIALIZER(struct ip6_lookup_cmd_tokens, cmd, "ipv6_lookup");
cmdline_parse_token_string_t ip6_lookup_route =
	TOKEN_STRING_INITIALIZER(struct ip6_lookup_cmd_tokens, route, "route");
cmdline_parse_token_string_t ip6_lookup_add =
	TOKEN_STRING_INITIALIZER(struct ip6_lookup_cmd_tokens, add, "add");
cmdline_parse_token_string_t ip6_lookup_ip6 =
	TOKEN_STRING_INITIALIZER(struct ip6_lookup_cmd_tokens, ip6, "ipv6");
cmdline_parse_token_string_t ip6_lookup_ip =
	TOKEN_STRING_INITIALIZER(struct ip6_lookup_cmd_tokens, ip, NULL);
cmdline_parse_token_string_t ip6_lookup_netmask =
	TOKEN_STRING_INITIALIZER(struct ip6_lookup_cmd_tokens, netmask, "netmask");
cmdline_parse_token_string_t ip6_lookup_mask =
	TOKEN_STRING_INITIALIZER(struct ip6_lookup_cmd_tokens, mask, NULL);
cmdline_parse_token_string_t ip6_lookup_via =
	TOKEN_STRING_INITIALIZER(struct ip6_lookup_cmd_tokens, via, "via");
cmdline_parse_token_string_t ip6_lookup_via_ip =
	TOKEN_STRING_INITIALIZER(struct ip6_lookup_cmd_tokens, via_ip, NULL);

cmdline_parse_inst_t ipv6_lookup_cmd_ctx = {
	.f = cli_ipv6_lookup,
	.data = NULL,
	.help_str = cmd_ipv6_lookup_help,
	.tokens = {
		(void *)&ip6_lookup_cmd,
		(void *)&ip6_lookup_route,
		(void *)&ip6_lookup_add,
		(void *)&ip6_lookup_ip6,
		(void *)&ip6_lookup_ip,
		(void *)&ip6_lookup_netmask,
		(void *)&ip6_lookup_mask,
		(void *)&ip6_lookup_via,
		(void *)&ip6_lookup_via_ip,
		NULL,
	},
};

cmdline_parse_token_string_t ipv6_lookup_help_cmd =
	TOKEN_STRING_INITIALIZER(struct ipv6_lookup_help_cmd_tokens, cmd, "help");
cmdline_parse_token_string_t ipv6_lookup_help_module =
	TOKEN_STRING_INITIALIZER(struct ipv6_lookup_help_cmd_tokens, module, "ipv6_lookup");

cmdline_parse_inst_t ipv6_lookup_help_cmd_ctx = {
	.f = cli_ipv6_lookup_help,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *)&ipv6_lookup_help_cmd,
		(void *)&ipv6_lookup_help_module,
		NULL,
	},
};
