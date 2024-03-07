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

void
cmd_help_ipv6_lookup_parsed(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
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

void
cmd_ipv6_lookup_route_add_ipv6_parsed(void *parsed_result, __rte_unused struct cmdline *cl,
				      void *data __rte_unused)
{
	struct cmd_ipv6_lookup_route_add_ipv6_result *res = parsed_result;
	struct route_ipv6_config config;
	int rc = -EINVAL, i;

	for (i = 0; i < ETHDEV_IPV6_ADDR_LEN; i++)
		config.ip[i] = res->ip.addr.ipv6.s6_addr[i];

	for (i = 0; i < ETHDEV_IPV6_ADDR_LEN; i++)
		config.mask[i] = res->mask.addr.ipv6.s6_addr[i];

	for (i = 0; i < ETHDEV_IPV6_ADDR_LEN; i++)
		config.gateway[i] = res->via_ip.addr.ipv6.s6_addr[i];

	rc = route_ip6_add(&config);
	if (rc)
		printf(MSG_CMD_FAIL, res->ipv6_lookup);
}
