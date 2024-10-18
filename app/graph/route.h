/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_ROUTE_H
#define APP_GRAPH_ROUTE_H

#include <rte_ip6.h>

#define MAX_ROUTE_ENTRIES 32

struct route_ipv4_config {
	TAILQ_ENTRY(route_ipv4_config) next;
	uint32_t ip;
	uint32_t netmask;
	uint32_t via;
	bool is_used;
};

TAILQ_HEAD(ip4_route, route_ipv4_config);

struct route_ipv6_config {
	TAILQ_ENTRY(route_ipv6_config) next;
	struct rte_ipv6_addr ip;
	struct rte_ipv6_addr mask;
	struct rte_ipv6_addr gateway;
	bool is_used;
};

TAILQ_HEAD(ip6_route, route_ipv6_config);

int route_ip4_add_to_lookup(void);
int route_ip6_add_to_lookup(void);
void route_ip4_list_clean(void);
void route_ip6_list_clean(void);

#endif
