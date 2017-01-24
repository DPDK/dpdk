/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
