/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _INIT_H_
#define _INIT_H_

/*
 * #include <rte_ring.h>
 * #include "args.h"
 */

/*
 * Define a client structure with all needed info, including
 * stats from the clients.
 */
struct client {
	struct rte_ring *rx_q;
	unsigned client_id;
	/* these stats hold how many packets the client will actually receive,
	 * and how many packets were dropped because the client's queue was full.
	 * The port-info stats, in contrast, record how many packets were received
	 * or transmitted on an actual NIC port.
	 */
	struct {
		volatile uint64_t rx;
		volatile uint64_t rx_drop;
	} stats;
};

extern struct client *clients;

/* the shared port information: port numbers, rx and tx stats etc. */
extern struct port_info *ports;

extern struct rte_mempool *pktmbuf_pool;
extern uint8_t num_clients;
extern unsigned num_sockets;
extern struct port_info *ports;

int init(int argc, char *argv[]);

#endif /* ifndef _INIT_H_ */
