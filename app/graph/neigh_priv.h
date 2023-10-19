/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_NEIGH_PRIV_H
#define APP_GRAPH_NEIGH_PRIV_H

#define MAX_NEIGH_ENTRIES 32

struct neigh_v4_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t ip4;
	cmdline_fixed_string_t ip;
	cmdline_fixed_string_t mac;
};

struct neigh_v6_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t add;
	cmdline_fixed_string_t ip6;
	cmdline_fixed_string_t ip;
	cmdline_fixed_string_t mac;
};

struct neigh_help_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t module;
};

struct neigh_ipv4_config {
	TAILQ_ENTRY(neigh_ipv4_config) next;
	uint32_t ip;
	uint64_t mac;
	bool is_used;
};

TAILQ_HEAD(neigh4_head, neigh_ipv4_config);

struct neigh_ipv6_config {
	TAILQ_ENTRY(neigh_ipv6_config) next;
	uint8_t ip[16];
	uint64_t mac;
	bool is_used;
};

TAILQ_HEAD(neigh6_head, neigh_ipv6_config);

#endif
