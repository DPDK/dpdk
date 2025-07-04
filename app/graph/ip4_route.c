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

static const char
cmd_ipv4_lookup_mode_help[] = "ipv4_lookup mode <lpm|fib>";

struct ip4_route route4 = TAILQ_HEAD_INITIALIZER(route4);

enum ip4_lookup_mode ip4_lookup_m = IP4_LOOKUP_LPM;

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
	int rc;

	portid = ethdev_portid_by_ip4(ipv4route->via, ipv4route->netmask);
	if (portid < 0) {
		printf("Invalid portid found to install the route\n");
		return portid;
	}

	depth = convert_netmask_to_depth(ipv4route->netmask);

	if (ip4_lookup_m == IP4_LOOKUP_FIB)
		rc = rte_node_ip4_fib_route_add(ipv4route->ip, depth, portid,
				RTE_NODE_IP4_LOOKUP_NEXT_REWRITE);
	else
		rc = rte_node_ip4_route_add(ipv4route->ip, depth, portid,
				RTE_NODE_IP4_LOOKUP_NEXT_REWRITE);

	return rc;
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

void
cmd_help_ipv4_lookup_parsed(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
			    __rte_unused void *data)
{
	size_t len;

	len = strlen(conn->msg_out);
	conn->msg_out += len;
	snprintf(conn->msg_out, conn->msg_out_len_max, "\n%s\n%s\n%s\n",
		 "--------------------------- ipv4_lookup command help ---------------------------",
		 cmd_ipv4_lookup_help, cmd_ipv4_lookup_mode_help);

	len = strlen(conn->msg_out);
	conn->msg_out_len_max -= len;
}

void
cmd_ipv4_lookup_route_add_ipv4_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
				      void *data __rte_unused)
{
	struct cmd_ipv4_lookup_route_add_ipv4_result *res = parsed_result;
	struct route_ipv4_config config;
	int rc = -EINVAL;

	config.ip = rte_be_to_cpu_32(res->ip.addr.ipv4.s_addr);
	config.netmask = rte_be_to_cpu_32(res->mask.addr.ipv4.s_addr);
	config.via = rte_be_to_cpu_32(res->via_ip.addr.ipv4.s_addr);

	rc = route_ip4_add(&config);
	if (rc < 0)
		printf(MSG_CMD_FAIL, res->ipv4_lookup);
}

void
cmd_ipv4_lookup_mode_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
			    void *data __rte_unused)
{
	struct cmd_ipv4_lookup_mode_result *res = parsed_result;

	if (!strcmp(res->lkup_mode, "lpm"))
		ip4_lookup_m = IP4_LOOKUP_LPM;
	else if (!strcmp(res->lkup_mode, "fib"))
		ip4_lookup_m = IP4_LOOKUP_FIB;
	else
		printf(MSG_CMD_FAIL, res->ipv4_lookup);
}
