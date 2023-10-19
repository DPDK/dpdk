/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_ROUTE_H
#define APP_GRAPH_ROUTE_H

#define MAX_ROUTE_ENTRIES 32

extern cmdline_parse_inst_t ipv4_lookup_cmd_ctx;
extern cmdline_parse_inst_t ipv6_lookup_cmd_ctx;
extern cmdline_parse_inst_t ipv4_lookup_help_cmd_ctx;
extern cmdline_parse_inst_t ipv6_lookup_help_cmd_ctx;

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
	uint8_t ip[16];
	uint8_t mask[16];
	uint8_t gateway[16];
	bool is_used;
};

TAILQ_HEAD(ip6_route, route_ipv6_config);

int route_ip4_add_to_lookup(void);
int route_ip6_add_to_lookup(void);
void route_ip4_list_clean(void);
void route_ip6_list_clean(void);

#endif
