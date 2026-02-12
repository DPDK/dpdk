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
#include <rte_ip6.h>

#include "module_api.h"
#include "route_priv.h"

static const char
cmd_ipv6_lookup_help[] = "ipv6_lookup route add ipv6 <ip> netmask <mask> via <ip>";

static const char
cmd_ipv6_lookup_mode_help[] = "ipv6_lookup mode <lpm|fib>";

struct ip6_route route6 = TAILQ_HEAD_INITIALIZER(route6);

enum ip6_lookup_mode ip6_lookup_m = IP6_LOOKUP_LPM;

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

static int
route6_rewirte_table_update(struct route_ipv6_config *ipv6route)
{
	uint8_t depth;
	int portid;
	int rc;

	portid = ethdev_portid_by_ip6(&ipv6route->gateway, &ipv6route->mask);
	if (portid < 0) {
		printf("Invalid portid found to install the route\n");
		return portid;
	}
	depth = rte_ipv6_mask_depth(&ipv6route->mask);

	if (ip6_lookup_m == IP6_LOOKUP_FIB)
		rc = rte_node_ip6_fib_route_add(&ipv6route->ip, depth, portid,
				RTE_NODE_IP6_LOOKUP_NEXT_REWRITE);
	else
		rc = rte_node_ip6_route_add(&ipv6route->ip, depth, portid,
				RTE_NODE_IP6_LOOKUP_NEXT_REWRITE);

	return rc;
}

static int
route_ip6_add(struct route_ipv6_config *route)
{
	struct route_ipv6_config *ipv6route;
	int rc = -EINVAL;

	ipv6route = find_route6_entry(route);
	if (!ipv6route) {
		ipv6route = malloc(sizeof(struct route_ipv6_config));
		if (!ipv6route)
			return -ENOMEM;
	} else {
		return 0;
	}

	ipv6route->ip = route->ip;
	ipv6route->mask = route->mask;
	ipv6route->gateway = route->gateway;
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

void
cmd_help_ipv6_lookup_parsed(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
			    __rte_unused void *data)
{
	size_t len;

	len = strlen(conn->msg_out);
	conn->msg_out += len;
	snprintf(conn->msg_out, conn->msg_out_len_max, "\n%s\n%s\n%s\n",
		 "--------------------------- ipv6_lookup command help ---------------------------",
		 cmd_ipv6_lookup_help, cmd_ipv6_lookup_mode_help);

	len = strlen(conn->msg_out);
	conn->msg_out_len_max -= len;
}

void
cmd_ipv6_lookup_route_add_ipv6_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
				      void *data __rte_unused)
{
	struct cmd_ipv6_lookup_route_add_ipv6_result *res = parsed_result;
	struct route_ipv6_config config = {
		.ip = res->ip.addr.ipv6,
		.mask = res->mask.addr.ipv6,
		.gateway = res->via_ip.addr.ipv6,
	};
	int rc;

	rc = route_ip6_add(&config);
	if (rc)
		printf(MSG_CMD_FAIL, res->ipv6_lookup);
}

void
cmd_ipv6_lookup_mode_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
			    void *data __rte_unused)
{
	struct cmd_ipv6_lookup_mode_result *res = parsed_result;

	if (!strcmp(res->lkup_mode, "lpm"))
		ip6_lookup_m = IP6_LOOKUP_LPM;
	else if (!strcmp(res->lkup_mode, "fib"))
		ip6_lookup_m = IP6_LOOKUP_FIB;
	else
		printf(MSG_CMD_FAIL, res->ipv6_lookup);
}
