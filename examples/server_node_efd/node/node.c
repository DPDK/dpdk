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
#include <rte_ip.h>

#include "common.h"

/* Number of packets to attempt to read from queue */
#define PKT_READ_SIZE  ((uint16_t)32)

/*
 * Our node id number - tells us which rx queue to read, and NIC TX
 * queue to write to.
 */
static uint8_t node_id;

#define MBQ_CAPACITY 32

/* maps input ports to output ports for packets */
static uint8_t output_ports[RTE_MAX_ETHPORTS];

/* buffers up a set of packet that are ready to send */
struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

/* shared data from server. We update statistics here */
static struct tx_stats *tx_stats;

static struct filter_stats *filter_stats;

/*
 * print a usage message
 */
static void
usage(const char *progname)
{
	printf("Usage: %s [EAL args] -- -n <node_id>\n\n", progname);
}

/*
 * Convert the node id number from a string to an int.
 */
static int
parse_node_num(const char *node)
{
	char *end = NULL;
	unsigned long temp;

	if (node == NULL || *node == '\0')
		return -1;

	temp = strtoul(node, &end, 10);
	if (end == NULL || *end != '\0')
		return -1;

	node_id = (uint8_t)temp;
	return 0;
}

/*
 * Parse the application arguments to the node app.
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
		&option_index)) != EOF) {
		switch (opt) {
		case 'n':
			if (parse_node_num(optarg) != 0) {
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
	uint8_t port_id = (uintptr_t)userdata;

	tx_stats->tx_drop[port_id] += count;

	/* free the mbufs which failed from transmit */
	for (i = 0; i < count; i++)
		rte_pktmbuf_free(unsent[i]);

}

static void
configure_tx_buffer(uint8_t port_id, uint16_t size)
{
	int ret;

	/* Initialize TX buffers */
	tx_buffer[port_id] = rte_zmalloc_socket("tx_buffer",
			RTE_ETH_TX_BUFFER_SIZE(size), 0,
			rte_eth_dev_socket_id(port_id));
	if (tx_buffer[port_id] == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx "
				"on port %u\n", (unsigned int) port_id);

	rte_eth_tx_buffer_init(tx_buffer[port_id], size);

	ret = rte_eth_tx_buffer_set_err_callback(tx_buffer[port_id],
			flush_tx_error_callback, (void *)(intptr_t)port_id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot set error callback for "
			"tx buffer on port %u\n", (unsigned int) port_id);
}

/*
 * set up output ports so that all traffic on port gets sent out
 * its paired port. Index using actual port numbers since that is
 * what comes in the mbuf structure.
 */
static void
configure_output_ports(const struct shared_info *info)
{
	int i;

	if (info->num_ports > RTE_MAX_ETHPORTS)
		rte_exit(EXIT_FAILURE, "Too many ethernet ports. "
				"RTE_MAX_ETHPORTS = %u\n",
				(unsigned int)RTE_MAX_ETHPORTS);
	for (i = 0; i < info->num_ports - 1; i += 2) {
		uint8_t p1 = info->id[i];
		uint8_t p2 = info->id[i+1];

		output_ports[p1] = p2;
		output_ports[p2] = p1;

		configure_tx_buffer(p1, MBQ_CAPACITY);
		configure_tx_buffer(p2, MBQ_CAPACITY);

	}
}

/*
 * Create the hash table that will contain the flows that
 * the node will handle, which will be used to decide if packet
 * is transmitted or dropped.
 */
static struct rte_hash *
create_hash_table(const struct shared_info *info)
{
	uint32_t num_flows_node = info->num_flows / info->num_nodes;
	char name[RTE_HASH_NAMESIZE];
	struct rte_hash *h;

	/* create table */
	struct rte_hash_parameters hash_params = {
		.entries = num_flows_node * 2, /* table load = 50% */
		.key_len = sizeof(uint32_t), /* Store IPv4 dest IP address */
		.socket_id = rte_socket_id(),
		.hash_func_init_val = 0,
	};

	snprintf(name, sizeof(name), "hash_table_%d", node_id);
	hash_params.name = name;
	h = rte_hash_create(&hash_params);

	if (h == NULL)
		rte_exit(EXIT_FAILURE,
				"Problem creating the hash table for node %d\n",
				node_id);
	return h;
}

static void
populate_hash_table(const struct rte_hash *h, const struct shared_info *info)
{
	unsigned int i;
	int32_t ret;
	uint32_t ip_dst;
	uint32_t num_flows_node = 0;
	uint64_t target_node;

	/* Add flows in table */
	for (i = 0; i < info->num_flows; i++) {
		target_node = i % info->num_nodes;
		if (target_node != node_id)
			continue;

		ip_dst = rte_cpu_to_be_32(i);

		ret = rte_hash_add_key(h, (void *) &ip_dst);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Unable to add entry %u "
					"in hash table\n", i);
		else
			num_flows_node++;

	}

	printf("Hash table: Adding 0x%x keys\n", num_flows_node);
}

/*
 * This function performs routing of packets
 * Just sends each input packet out an output port based solely on the input
 * port it arrived on.
 */
static inline void
transmit_packet(struct rte_mbuf *buf)
{
	int sent;
	const uint8_t in_port = buf->port;
	const uint8_t out_port = output_ports[in_port];
	struct rte_eth_dev_tx_buffer *buffer = tx_buffer[out_port];

	sent = rte_eth_tx_buffer(out_port, node_id, buffer, buf);
	if (sent)
		tx_stats->tx[out_port] += sent;

}

static inline void
handle_packets(struct rte_hash *h, struct rte_mbuf **bufs, uint16_t num_packets)
{
	struct ipv4_hdr *ipv4_hdr;
	uint32_t ipv4_dst_ip[PKT_READ_SIZE];
	const void *key_ptrs[PKT_READ_SIZE];
	unsigned int i;
	int32_t positions[PKT_READ_SIZE] = {0};

	for (i = 0; i < num_packets; i++) {
		/* Handle IPv4 header.*/
		ipv4_hdr = rte_pktmbuf_mtod_offset(bufs[i], struct ipv4_hdr *,
				sizeof(struct ether_hdr));
		ipv4_dst_ip[i] = ipv4_hdr->dst_addr;
		key_ptrs[i] = &ipv4_dst_ip[i];
	}
	/* Check if packets belongs to any flows handled by this node */
	rte_hash_lookup_bulk(h, key_ptrs, num_packets, positions);

	for (i = 0; i < num_packets; i++) {
		if (likely(positions[i] >= 0)) {
			filter_stats->passed++;
			transmit_packet(bufs[i]);
		} else {
			filter_stats->drop++;
			/* Drop packet, as flow is not handled by this node */
			rte_pktmbuf_free(bufs[i]);
		}
	}
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
	struct rte_hash *h;
	struct rte_mempool *mp;
	struct shared_info *info;
	int need_flush = 0; /* indicates whether we have unsent packets */
	int retval;
	void *pkts[PKT_READ_SIZE];
	uint16_t sent;

	retval = rte_eal_init(argc, argv);
	if (retval  < 0)
		return -1;
	argc -= retval;
	argv += retval;

	if (parse_app_args(argc, argv) < 0)
		rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

	if (rte_eth_dev_count() == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	rx_ring = rte_ring_lookup(get_rx_queue_name(node_id));
	if (rx_ring == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get RX ring - "
				"is server process running?\n");

	mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
	if (mp == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get mempool for mbufs\n");

	mz = rte_memzone_lookup(MZ_SHARED_INFO);
	if (mz == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get port info structure\n");
	info = mz->addr;
	tx_stats = &(info->tx_stats[node_id]);
	filter_stats = &(info->filter_stats[node_id]);

	configure_output_ports(info);

	h = create_hash_table(info);

	populate_hash_table(h, info);

	RTE_LOG(INFO, APP, "Finished Process Init.\n");

	printf("\nNode process %d handling packets\n", node_id);
	printf("[Press Ctrl-C to quit ...]\n");

	for (;;) {
		uint16_t  rx_pkts = PKT_READ_SIZE;
		uint8_t port;

		/*
		 * Try dequeuing max possible packets first, if that fails,
		 * get the most we can. Loop body should only execute once,
		 * maximum
		 */
		while (rx_pkts > 0 &&
				unlikely(rte_ring_dequeue_bulk(rx_ring, pkts,
					rx_pkts, NULL) == 0))
			rx_pkts = (uint16_t)RTE_MIN(rte_ring_count(rx_ring),
					PKT_READ_SIZE);

		if (unlikely(rx_pkts == 0)) {
			if (need_flush)
				for (port = 0; port < info->num_ports; port++) {
					sent = rte_eth_tx_buffer_flush(
							info->id[port],
							node_id,
							tx_buffer[port]);
					if (unlikely(sent))
						tx_stats->tx[port] += sent;
				}
			need_flush = 0;
			continue;
		}

		handle_packets(h, (struct rte_mbuf **)pkts, rx_pkts);

		need_flush = 1;
	}
}
