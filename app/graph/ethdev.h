/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_ETHDEV_H
#define APP_GRAPH_ETHDEV_H

#include <cmdline_parse.h>
#include <rte_ip6.h>

struct ipv4_addr_config {
	uint32_t ip;
	uint32_t mask;
};

struct ipv6_addr_config {
	struct rte_ipv6_addr ip;
	struct rte_ipv6_addr mask;
};

extern uint32_t enabled_port_mask;

void ethdev_start(void);
void ethdev_stop(void);
void *ethdev_mempool_list_by_portid(uint16_t portid);
int16_t ethdev_portid_by_ip4(uint32_t ip, uint32_t mask);
int16_t ethdev_portid_by_ip6(struct rte_ipv6_addr *ip, struct rte_ipv6_addr *mask);
int16_t ethdev_txport_by_rxport_get(uint16_t portid_rx);
void ethdev_list_clean(void);

#endif
