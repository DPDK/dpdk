/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_ETHDEV_RX_PRIV_H
#define APP_GRAPH_ETHDEV_RX_PRIV_H

#include <stdint.h>

#include <rte_graph.h>
#include <rte_node_eth_api.h>

#define MAX_RX_QUEUE_PER_PORT 128
#define MAX_JUMBO_PKT_LEN  9600
#define NB_SOCKETS 8

struct ethdev_rx_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t map;
	cmdline_fixed_string_t port;
	cmdline_fixed_string_t dev;
	cmdline_fixed_string_t queue;
	cmdline_fixed_string_t core;
	uint32_t core_id;
	uint32_t qid;
};

struct ethdev_rx_help_cmd_tokens {
	cmdline_fixed_string_t cmd;
	cmdline_fixed_string_t module;
};

struct lcore_params {
	uint16_t port_id;
	uint8_t queue_id;
	uint8_t lcore_id;
} __rte_cache_aligned;

#endif
