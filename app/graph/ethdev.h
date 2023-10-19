/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_ETHDEV_H
#define APP_GRAPH_ETHDEV_H

#include <cmdline_parse.h>

#define ETHDEV_IPV6_ADDR_LEN	16

extern cmdline_parse_inst_t ethdev_show_cmd_ctx;
extern cmdline_parse_inst_t ethdev_stats_cmd_ctx;
extern cmdline_parse_inst_t ethdev_mtu_cmd_ctx;
extern cmdline_parse_inst_t ethdev_prom_mode_cmd_ctx;
extern cmdline_parse_inst_t ethdev_ip4_cmd_ctx;
extern cmdline_parse_inst_t ethdev_ip6_cmd_ctx;
extern cmdline_parse_inst_t ethdev_cmd_ctx;
extern cmdline_parse_inst_t ethdev_help_cmd_ctx;

struct ipv4_addr_config {
	uint32_t ip;
	uint32_t mask;
};

struct ipv6_addr_config {
	uint8_t ip[ETHDEV_IPV6_ADDR_LEN];
	uint8_t mask[ETHDEV_IPV6_ADDR_LEN];
};

extern uint32_t enabled_port_mask;

void ethdev_start(void);
void ethdev_stop(void);
void *ethdev_mempool_list_by_portid(uint16_t portid);
int16_t ethdev_portid_by_ip4(uint32_t ip, uint32_t mask);
int16_t ethdev_portid_by_ip6(uint8_t *ip, uint8_t *mask);
void ethdev_list_clean(void);

#endif
