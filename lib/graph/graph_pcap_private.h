/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell International Ltd.
 */

#ifndef _RTE_GRAPH_PCAP_PRIVATE_H_
#define _RTE_GRAPH_PCAP_PRIVATE_H_

#include <stdint.h>
#include <sys/types.h>

#include "graph_private.h"

/**
 * @internal
 *
 * Pcap trace enable/disable function.
 *
 * The function is called to enable/disable graph pcap trace functionality.
 *
 * @param val
 *   Value to be set to enable/disable graph pcap trace.
 */
void graph_pcap_enable(bool val);

/**
 * @internal
 *
 * Check graph pcap trace is enable/disable.
 *
 * The function is called to check if the graph pcap trace is enabled/disabled.
 *
 * @return
 *   - 1: Enable
 *   - 0: Disable
 */
int graph_pcap_is_enable(void);

/**
 * @internal
 *
 * Initialise graph pcap trace functionality.
 *
 * The function invoked to allocate mempool.
 *
 * @return
 *   0 on success and -1 on failure.
 */
int graph_pcap_mp_init(void);

/**
 * @internal
 *
 * Initialise graph pcap trace functionality.
 *
 * The function invoked to open pcap file.
 *
 * @param filename
 *   Pcap filename.
 *
 * @return
 *   0 on success and -1 on failure.
 */
int graph_pcap_file_open(const char *filename);

/**
 * @internal
 *
 * Initialise graph pcap trace functionality.
 *
 * The function invoked when the graph pcap trace is enabled. This function
 * open's pcap file and allocates mempool. Information needed for secondary
 * process is populated.
 *
 * @param graph
 *   Pointer to graph structure.
 *
 * @return
 *   0 on success and -1 on failure.
 */
int graph_pcap_init(struct graph *graph);

/**
 * @internal
 *
 * Exit graph pcap trace functionality.
 *
 * The function is called to exit graph pcap trace and close open fd's and
 * free up memory. Pcap trace is also disabled.
 *
 * @param graph
 *   Pointer to graph structure.
 */
void graph_pcap_exit(struct rte_graph *graph);

/**
 * @internal
 *
 * Capture mbuf metadata and node metadata to a pcap file.
 *
 * When graph pcap trace enabled, this function is invoked prior to each node
 * and mbuf, node metadata is parsed and captured in a pcap file.
 *
 * @param graph
 *   Pointer to the graph object.
 * @param node
 *   Pointer to the node object.
 * @param objs
 *   Pointer to an array of objects to be processed.
 * @param nb_objs
 *   Number of objects in the array.
 */
uint16_t graph_pcap_dispatch(struct rte_graph *graph,
				   struct rte_node *node, void **objs,
				   uint16_t nb_objs);

#endif /* _RTE_GRAPH_PCAP_PRIVATE_H_ */
