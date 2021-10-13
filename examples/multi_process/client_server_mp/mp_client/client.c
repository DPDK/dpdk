/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
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
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_branch_prediction.h>
#include <rte_log.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_ring.h>
#include <rte_launch.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include "common.h"

/* Number of packets to attempt to read from queue */
#define PKT_READ_SIZE  ((uint16_t)32)

/* our client id number - tells us which rx queue to read, and NIC TX
 * queue to write to. */
static uint8_t client_id = 0;

#define MBQ_CAPACITY 32

/* maps input ports to output ports for packets */
static uint16_t output_ports[RTE_MAX_ETHPORTS];

/* buffers up a set of packet that are ready to send */
struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

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
 * Tx buffer error callback
 */
static void
flush_tx_error_callback(struct rte_mbuf **unsent, uint16_t count,
		void *userdata) {
	int i;
	uint16_t port_id = (uintptr_t)userdata;

	tx_stats->tx_drop[port_id] += count;

	/* free the mbufs which failed from transmit */
	for (i = 0; i < count; i++)
		rte_pktmbuf_free(unsent[i]);

}

static void
configure_tx_buffer(uint16_t port_id, uint16_t size)
{
	int ret;

	/* Initialize TX buffers */
	tx_buffer[port_id] = rte_zmalloc_socket("tx_buffer",
			RTE_ETH_TX_BUFFER_SIZE(size), 0,
			rte_eth_dev_socket_id(port_id));
	if (tx_buffer[port_id] == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
			 port_id);

	rte_eth_tx_buffer_init(tx_buffer[port_id], size);

	ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[port_id],
			flush_tx_error_callback, (void *)(intptr_t)port_id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
		"Cannot set error callback for tx buffer on port %u\n",
			 port_id);
}

/*
 * set up output ports so that all traffic on port gets sent out
 * its paired port. Index using actual port numbers since that is
 * what comes in the mbuf structure.
 */
static void
configure_output_ports(const struct port_info *ports)
{
	int i;
	if (ports->num_ports > RTE_MAX_ETHPORTS)
		rte_exit(EXIT_FAILURE, "Too many ethernet ports. RTE_MAX_ETHPORTS = %u\n",
				(unsigned)RTE_MAX_ETHPORTS);
	for (i = 0; i < ports->num_ports - 1; i+=2){
		uint16_t p1 = ports->id[i];
		uint16_t p2 = ports->id[i+1];
		output_ports[p1] = p2;
		output_ports[p2] = p1;

		configure_tx_buffer(p1, MBQ_CAPACITY);
		configure_tx_buffer(p2, MBQ_CAPACITY);

	}
}

/*
 * This function performs routing of packets
 * Just sends each input packet out an output port based solely on the input
 * port it arrived on.
 */
static void
handle_packet(struct rte_mbuf *buf)
{
	int sent;
	const uint16_t in_port = buf->port;
	const uint16_t out_port = output_ports[in_port];
	struct rte_eth_dev_tx_buffer *buffer = tx_buffer[out_port];

	sent = rte_eth_tx_buffer(out_port, client_id, buffer, buf);
	if (sent)
		tx_stats->tx[out_port] += sent;

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
	uint16_t sent;

	if ((retval = rte_eal_init(argc, argv)) < 0)
		return -1;
	argc -= retval;
	argv += retval;

	if (parse_app_args(argc, argv) < 0)
		rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

	if (rte_eth_dev_count_avail() == 0)
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
		uint16_t i, rx_pkts;

		rx_pkts = rte_ring_dequeue_burst(rx_ring, pkts,
				PKT_READ_SIZE, NULL);

		if (rx_pkts == 0 && need_flush) {
			for (i = 0; i < ports->num_ports; i++) {
				uint16_t port = ports->id[i];

				sent = rte_eth_tx_buffer_flush(port,
							       client_id,
							       tx_buffer[port]);
				tx_stats->tx[port] += sent;
			}
			need_flush = 0;
			continue;
		}

		for (i = 0; i < rx_pkts; i++)
			handle_packet(pkts[i]);

		need_flush = 1;
	}

	/* clean up the EAL */
	rte_eal_cleanup();
}
