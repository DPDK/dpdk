/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef _INIT_H_
#define _INIT_H_

/*
 * #include <rte_ring.h>
 * #include "args.h"
 */

/*
 * Define a node structure with all needed info, including
 * stats from the nodes.
 */
struct node {
	struct rte_ring *rx_q;
	unsigned int node_id;
	/* these stats hold how many packets the node will actually receive,
	 * and how many packets were dropped because the node's queue was full.
	 * The port-info stats, in contrast, record how many packets were received
	 * or transmitted on an actual NIC port.
	 */
	struct {
		uint64_t rx;
		uint64_t rx_drop;
	} stats;
};

extern struct rte_efd_table *efd_table;
extern struct node *nodes;

/*
 * shared information between server and nodes: number of nodes,
 * port numbers, rx and tx stats etc.
 */
extern struct shared_info *info;

extern struct rte_mempool *pktmbuf_pool;
extern uint8_t num_nodes;
extern unsigned int num_sockets;
extern uint32_t num_flows;

int init(int argc, char *argv[]);

#endif /* ifndef _INIT_H_ */
