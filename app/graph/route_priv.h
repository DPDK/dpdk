/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_ROUTE_PRIV_H
#define APP_GRAPH_ROUTE_PRIV_H

#define MAX_ROUTE_ENTRIES 32

struct ip4_lookup_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t route;
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t ip4;
	cmdline_fixed_string_t ip;
	cmdline_fixed_string_t netmask;
	cmdline_fixed_string_t mask;
	cmdline_fixed_string_t via;
	cmdline_fixed_string_t via_ip;
};

struct ip6_lookup_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t route;
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t ip6;
	cmdline_fixed_string_t ip;
	cmdline_fixed_string_t netmask;
	cmdline_fixed_string_t mask;
	cmdline_fixed_string_t via;
	cmdline_fixed_string_t via_ip;
};

struct ipv4_lookup_help_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t module;
};

struct ipv6_lookup_help_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t module;
};

#endif
