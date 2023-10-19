/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_ETHDEV_RX_H
#define APP_GRAPH_ETHDEV_RX_H

#include <rte_graph.h>
#include <rte_node_eth_api.h>

#define ETHDEV_RX_LCORE_PARAMS_MAX 1024
#define ETHDEV_RX_QUEUE_PER_LCORE_MAX 16

struct lcore_rx_queue {
	uint16_t port_id;
	uint8_t queue_id;
	char node_name[RTE_NODE_NAMESIZE];
};

struct lcore_conf {
	uint16_t n_rx_queue;
	struct lcore_rx_queue rx_queue_list[ETHDEV_RX_QUEUE_PER_LCORE_MAX];
	struct rte_graph *graph;
	char name[RTE_GRAPH_NAMESIZE];
	rte_graph_t graph_id;
} __rte_cache_aligned;

uint8_t ethdev_rx_num_rx_queues_get(uint16_t port);

extern struct rte_node_ethdev_config ethdev_conf[RTE_MAX_ETHPORTS];
extern struct lcore_conf lcore_conf[RTE_MAX_LCORE];
extern cmdline_parse_inst_t ethdev_rx_help_cmd_ctx;
extern cmdline_parse_inst_t ethdev_rx_cmd_ctx;
extern struct lcore_params *lcore_params;
extern uint16_t nb_lcore_params;

#endif
