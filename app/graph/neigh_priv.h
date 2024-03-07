/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_NEIGH_PRIV_H
#define APP_GRAPH_NEIGH_PRIV_H

#define MAX_NEIGH_ENTRIES 32

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
