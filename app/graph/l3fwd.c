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
#include <rte_node_ip4_api.h>
#include <rte_node_ip6_api.h>
#include <rte_node_pkt_cls_api.h>

#include "module_api.h"

static int
setup_fib(int socket)
{
	struct rte_fib_conf conf;

#define FIB_MAX_ROUTES (UINT16_MAX)
#define FIB_NUM_TBL8   (UINT16_MAX / 2)
	conf.type = RTE_FIB_DIR24_8;
	conf.max_routes = FIB_MAX_ROUTES;
	conf.rib_ext_sz = 0;
	conf.dir24_8.nh_sz = RTE_FIB_DIR24_8_4B;
	conf.dir24_8.num_tbl8 = FIB_NUM_TBL8;
	conf.flags = 0;

	return rte_node_ip4_fib_create(socket, &conf);
}

static int
setup_fib6(int socket)
{
	struct rte_fib6_conf conf;

#define FIB6_MAX_ROUTES (UINT16_MAX)
#define FIB6_NUM_TBL8   (UINT16_MAX / 2)
	conf.type = RTE_FIB6_TRIE;
	conf.max_routes = FIB6_MAX_ROUTES;
	conf.rib_ext_sz = 0;
	conf.trie.nh_sz = RTE_FIB6_TRIE_4B;
	conf.trie.num_tbl8 = FIB6_NUM_TBL8;

	return rte_node_ip6_fib_create(socket, &conf);
}

static int
l3fwd_pattern_configure(void)
{
	/* Graph initialization. 8< */
	static const char * const default_patterns[] = {
		"ip4*",
		"ethdev_tx-*",
		"pkt_drop",
	};
	struct rte_graph_param graph_conf;
	const char **node_patterns;
	uint64_t pcap_pkts_count;
	struct lcore_conf *qconf;
	uint16_t nb_patterns;
	uint8_t pcap_ena;
	int rc, lcore_id;
	char *pcap_file;

	nb_patterns = RTE_DIM(default_patterns);
	node_patterns = malloc((ETHDEV_RX_QUEUE_PER_LCORE_MAX + nb_patterns) *
			sizeof(*node_patterns));
	if (!node_patterns)
		return -ENOMEM;
	memcpy(node_patterns, default_patterns,
			nb_patterns * sizeof(*node_patterns));

	memset(&graph_conf, 0, sizeof(graph_conf));
	graph_conf.node_patterns = node_patterns;

	/* Pcap config */
	graph_pcap_config_get(&pcap_ena, &pcap_pkts_count, &pcap_file);
	graph_conf.pcap_enable = pcap_ena;
	graph_conf.num_pkt_to_capture = pcap_pkts_count;
	graph_conf.pcap_filename = strdup(pcap_file);

	if (ip4_lookup_m == IP4_LOOKUP_FIB) {
		const char *fib_n = "ip4_lookup_fib";
		const char *lpm_n = "ip4_lookup";
		rte_node_t pkt_cls;

		pkt_cls = rte_node_from_name("pkt_cls");
		rte_node_edge_update(pkt_cls, RTE_NODE_PKT_CLS_NEXT_IP4_LOOKUP, &fib_n, 1);
		rte_node_edge_update(pkt_cls, RTE_NODE_PKT_CLS_NEXT_IP4_LOOKUP_FIB, &lpm_n, 1);
	}

	if (ip6_lookup_m == IP6_LOOKUP_FIB) {
		const char *fib6_n = "ip6_lookup_fib";
		const char *lpm6_n = "ip6_lookup";
		rte_node_t pkt_cls;

		pkt_cls = rte_node_from_name("pkt_cls");
		rte_node_edge_update(pkt_cls, RTE_NODE_PKT_CLS_NEXT_IP6_LOOKUP, &fib6_n, 1);
		rte_node_edge_update(pkt_cls, RTE_NODE_PKT_CLS_NEXT_IP6_LOOKUP_FIB, &lpm6_n, 1);
	}

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

		if (ip4_lookup_m == IP4_LOOKUP_FIB) {
			rc = setup_fib(graph_conf.socket_id);
			if (rc < 0)
				rte_exit(EXIT_FAILURE, "Unable to setup fib for socket %u\n",
						graph_conf.socket_id);
		}

		if (ip6_lookup_m == IP6_LOOKUP_FIB) {
			rc = setup_fib6(graph_conf.socket_id);
			if (rc < 0)
				rte_exit(EXIT_FAILURE, "Unable to setup fib6 for socket %u\n",
						graph_conf.socket_id);
		}

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

	rc = route_ip4_add_to_lookup();
	if (rc < 0)
		rte_exit(EXIT_FAILURE, "Unable to add v4 route to lookup table\n");

	rc = route_ip6_add_to_lookup();
	if (rc < 0)
		rte_exit(EXIT_FAILURE, "Unable to add v6 route to lookup table\n");

	rc = neigh_ip4_add_to_rewrite();
	if (rc < 0)
		rte_exit(EXIT_FAILURE, "Unable to add v4 to rewrite node\n");

	rc = neigh_ip6_add_to_rewrite();
	if (rc < 0)
		rte_exit(EXIT_FAILURE, "Unable to add v6 to rewrite node\n");

	/* Launch per-lcore init on every worker lcore */
	rte_eal_mp_remote_launch(graph_walk_start, NULL, SKIP_MAIN);

	/* Accumulate and print stats on main until exit */
	if (rte_graph_has_stats_feature() && app_graph_stats_enabled())
		graph_stats_print();

	return rc;
}

int
usecase_l3fwd_configure(struct rte_node_ethdev_config *conf, uint16_t nb_confs, uint16_t nb_graphs)
{
	int rc;

	rc = rte_node_eth_config(conf, nb_confs, nb_graphs);
	if (rc)
		rte_exit(EXIT_FAILURE, "rte_node_eth_config: err=%d\n", rc);

	rc = l3fwd_pattern_configure();
	if (rc)
		rte_exit(EXIT_FAILURE, "l3fwd_pattern_failure: err=%d\n", rc);

	return rc;
}
