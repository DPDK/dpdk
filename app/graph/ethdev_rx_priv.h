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

struct __rte_cache_aligned lcore_params {
	uint16_t port_id;
	uint8_t queue_id;
	uint8_t lcore_id;
};

#endif
