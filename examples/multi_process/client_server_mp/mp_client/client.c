/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_ring.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include "common.h"

/* Number of packets to attempt to read from queue */
#define PKT_READ_SIZE  ((uint16_t)32)

/* our client id number - tells us which rx queue to read, and NIC TX
 * queue to write to. */
static uint8_t client_id = 0;

struct mbuf_queue {
#define MBQ_CAPACITY 32
	struct rte_mbuf *bufs[MBQ_CAPACITY];
	uint16_t top;
};

/* maps input ports to output ports for packets */
static uint8_t output_ports[RTE_MAX_ETHPORTS];

/* buffers up a set of packet that are ready to send */
static struct mbuf_queue output_bufs[RTE_MAX_ETHPORTS];

/* shared data from server. We update statistics here */
static volatile struct tx_stats *tx_stats;


/*
 * print a usage message
 */
static void
usage(const char *progname)
{
	printf("Usage: %s [EAL args] -- -n <client_id>\n\n", progname);
}

/*
 * Convert the client id number from a string to an int.
 */
static int
parse_client_num(const char *client)
{
	char *end = NULL;
	unsigned long temp;

	if (client == NULL || *client == '\0')
		return -1;

	temp = strtoul(client, &end, 10);
	if (end == NULL || *end != '\0')
		return -1;

	client_id = (uint8_t)temp;
	return 0;
}

/*
 * Parse the application arguments to the client app.
 */
static int
parse_app_args(int argc, char *argv[])
{
	int option_index, opt;
	char **argvopt = argv;
	const char *progname = NULL;
	static struct option lgopts[] = { /* no long options */
		{NULL, 0, 0, 0 }
	};
	progname = argv[0];

	while ((opt = getopt_long(argc, argvopt, "n:", lgopts,
		&option_index)) != EOF){
		switch (opt){
			case 'n':
				if (parse_client_num(optarg) != 0){
					usage(progname);
					return -1;
				}
				break;
			default:
				usage(progname);
				return -1;
		}
	}
	return 0;
}

/*
 * set up output ports so that all traffic on port gets sent out
 * its paired port. Index using actual port numbers since that is
 * what comes in the mbuf structure.
 */
static void configure_output_ports(const struct port_info *ports)
{
	int i;
	if (ports->num_ports > RTE_MAX_ETHPORTS)
		rte_exit(EXIT_FAILURE, "Too many ethernet ports. RTE_MAX_ETHPORTS = %u\n",
				(unsigned)RTE_MAX_ETHPORTS);
	for (i = 0; i < ports->num_ports - 1; i+=2){
		uint8_t p1 = ports->id[i];
		uint8_t p2 = ports->id[i+1];
		output_ports[p1] = p2;
		output_ports[p2] = p1;
	}
}


static inline void
send_packets(uint8_t port)
{
	uint16_t i, sent;
	struct mbuf_queue *mbq = &output_bufs[port];

	if (unlikely(mbq->top == 0))
		return;

	sent = rte_eth_tx_burst(port, client_id, mbq->bufs, mbq->top);
	if (unlikely(sent < mbq->top)){
		for (i = sent; i < mbq->top; i++)
			rte_pktmbuf_free(mbq->bufs[i]);
		tx_stats->tx_drop[port] += (mbq->top - sent);
	}
	tx_stats->tx[port] += sent;
	mbq->top = 0;
}

/*
 * Enqueue a packet to be sent on a particular port, but
 * don't send it yet. Only when the buffer is full.
 */
static inline void
enqueue_packet(struct rte_mbuf *buf, uint8_t port)
{
	struct mbuf_queue *mbq = &output_bufs[port];
	mbq->bufs[mbq->top++] = buf;

	if (mbq->top == MBQ_CAPACITY)
		send_packets(port);
}

/*
 * This function performs routing of packets
 * Just sends each input packet out an output port based solely on the input
 * port it arrived on.
 */
static void
handle_packet(struct rte_mbuf *buf)
{
	const uint8_t in_port = buf->port;
	const uint8_t out_port = output_ports[in_port];

	enqueue_packet(buf, out_port);
}

/*
 * Application main function - loops through
 * receiving and processing packets. Never returns
 */
int
main(int argc, char *argv[])
{
	const struct rte_memzone *mz;
	struct rte_ring *rx_ring;
	struct rte_mempool *mp;
	struct port_info *ports;
	int need_flush = 0; /* indicates whether we have unsent packets */
	int retval;
	void *pkts[PKT_READ_SIZE];

	if ((retval = rte_eal_init(argc, argv)) < 0)
		return -1;
	argc -= retval;
	argv += retval;

	if (parse_app_args(argc, argv) < 0)
		rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

	if (rte_eth_dev_count() == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	rx_ring = rte_ring_lookup(get_rx_queue_name(client_id));
	if (rx_ring == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get RX ring - is server process running?\n");

	mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
	if (mp == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get mempool for mbufs\n");

	mz = rte_memzone_lookup(MZ_PORT_INFO);
	if (mz == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get port info structure\n");
	ports = mz->addr;
	tx_stats = &(ports->tx_stats[client_id]);

	configure_output_ports(ports);

	RTE_LOG(INFO, APP, "Finished Process Init.\n");

	printf("\nClient process %d handling packets\n", client_id);
	printf("[Press Ctrl-C to quit ...]\n");

	for (;;) {
		uint16_t i, rx_pkts = PKT_READ_SIZE;
		uint8_t port;

		/* try dequeuing max possible packets first, if that fails, get the
		 * most we can. Loop body should only execute once, maximum */
		while (rx_pkts > 0 &&
				unlikely(rte_ring_dequeue_bulk(rx_ring, pkts, rx_pkts) != 0))
			rx_pkts = (uint16_t)RTE_MIN(rte_ring_count(rx_ring), PKT_READ_SIZE);

		if (unlikely(rx_pkts == 0)){
			if (need_flush)
				for (port = 0; port < ports->num_ports; port++)
					send_packets(ports->id[port]);
			need_flush = 0;
			continue;
		}

		for (i = 0; i < rx_pkts; i++)
			handle_packet(pkts[i]);

		need_flush = 1;
	}
}
