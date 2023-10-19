/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_ROUTE_H
#define APP_GRAPH_ROUTE_H

#define MAX_ROUTE_ENTRIES 32

extern cmdline_parse_inst_t ipv4_lookup_cmd_ctx;
extern cmdline_parse_inst_t ipv4_lookup_help_cmd_ctx;

struct route_ipv4_config {
	TAILQ_ENTRY(route_ipv4_config) next;
	uint32_t ip;
	uint32_t netmask;
	uint32_t via;
	bool is_used;
};

TAILQ_HEAD(ip4_route, route_ipv4_config);

int route_ip4_add_to_lookup(void);
void route_ip4_list_clean(void);

#endif
