/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_graph.h>
#include <rte_graph_worker.h>
#include <rte_lcore.h>
#include <rte_node_eth_api.h>

#include "module_api.h"

static int
l2fwd_pattern_configure(void)
{
	struct rte_graph_param graph_conf;
	const char **node_patterns;
	uint64_t pcap_pkts_count;
	struct lcore_conf *qconf;
	uint16_t nb_patterns;
	uint8_t pcap_ena;
	char *pcap_file;
	int lcore_id;

	nb_patterns = 0;
	node_patterns = malloc((ETHDEV_RX_QUEUE_PER_LCORE_MAX + nb_patterns) *
			sizeof(*node_patterns));
	if (!node_patterns)
		return -ENOMEM;

	memset(&graph_conf, 0, sizeof(graph_conf));
	graph_conf.node_patterns = node_patterns;

	/* Pcap config */
	graph_pcap_config_get(&pcap_ena, &pcap_pkts_count, &pcap_file);
	graph_conf.pcap_enable = pcap_ena;
	graph_conf.num_pkt_to_capture = pcap_pkts_count;
	graph_conf.pcap_filename = strdup(pcap_file);

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		rte_graph_t graph_id;
		rte_edge_t i;

		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		qconf = &lcore_conf[lcore_id];

		/* Skip graph creation if no source exists */
		if (!qconf->n_rx_queue)
			continue;

		/* Add rx node patterns of this lcore */
		for (i = 0; i < qconf->n_rx_queue; i++) {
			graph_conf.node_patterns[nb_patterns + i] =
				qconf->rx_queue_list[i].node_name;
		}

		graph_conf.nb_node_patterns = nb_patterns + i;
		graph_conf.socket_id = rte_lcore_to_socket_id(lcore_id);

		snprintf(qconf->name, sizeof(qconf->name), "worker_%u",
				lcore_id);

		graph_id = rte_graph_create(qconf->name, &graph_conf);
		if (graph_id == RTE_GRAPH_ID_INVALID)
			rte_exit(EXIT_FAILURE,
					"rte_graph_create(): graph_id invalid"
					" for lcore %u\n", lcore_id);

		qconf->graph_id = graph_id;
		qconf->graph = rte_graph_lookup(qconf->name);
		/* >8 End of graph initialization. */
		if (!qconf->graph)
			rte_exit(EXIT_FAILURE,
					"rte_graph_lookup(): graph %s not found\n",
					qconf->name);
	}

	/* Launch per-lcore init on every worker lcore */
	rte_eal_mp_remote_launch(graph_walk_start, NULL, SKIP_MAIN);

	/* Accumulate and print stats on main until exit */
	if (rte_graph_has_stats_feature() && app_graph_stats_enabled())
		graph_stats_print();

	return 0;
}

static int
ethdev_rx_to_tx_node_link(uint32_t lcore_id)
{
	char name[RTE_NODE_NAMESIZE];
	const char *next_node = name;
	struct lcore_conf *qconf;
	uint16_t queue, port_id;
	rte_node_t rx_id;
	int16_t txport;
	int rc = 0;

	qconf = &lcore_conf[lcore_id];

	for (queue = 0; queue < qconf->n_rx_queue; ++queue) {
		port_id = qconf->rx_queue_list[queue].port_id;
		txport = ethdev_txport_by_rxport_get(port_id);
		if (txport >= 0) {
			rx_id = rte_node_from_name(qconf->rx_queue_list[queue].node_name);
			snprintf(name, sizeof(name), "ethdev_tx-%u", txport);
			rte_node_edge_update(rx_id, RTE_EDGE_ID_INVALID, &next_node, 1);
			rc = rte_node_ethdev_rx_next_update(rx_id, name);
			if (rc)
				goto exit;
		} else {
			rc = -EINVAL;
			goto exit;
		}
	}
exit:
	return rc;
}


int
usecase_l2fwd_configure(struct rte_node_ethdev_config *conf, uint16_t nb_confs, uint16_t nb_graphs)
{
	uint32_t lcore_id;
	int rc;

	rc = rte_node_eth_config(conf, nb_confs, nb_graphs);
	if (rc)
		rte_exit(EXIT_FAILURE, "rte_node_eth_config: err=%d\n", rc);

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		rc = ethdev_rx_to_tx_node_link(lcore_id);
		if (rc)
			rte_exit(EXIT_FAILURE, "rte_node_eth_config: err=%d\n", rc);
	}

	rc = l2fwd_pattern_configure();
	if (rc)
		rte_exit(EXIT_FAILURE, "l2fwd_pattern_failure: err=%d\n", rc);

	return rc;
}
