/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/queue.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_atomic.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_mempool.h>
#include <rte_memcpy.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_interrupts.h>
#include <rte_ethdev.h>
#include <rte_byteorder.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <rte_efd.h>
#include <rte_ip.h>

#include "common.h"
#include "args.h"
#include "init.h"

/*
 * When doing reads from the NIC or the node queues,
 * use this batch size
 */
#define PACKET_READ_SIZE 32

/*
 * Local buffers to put packets in, used to send packets in bursts to the
 * nodes
 */
struct node_rx_buf {
	struct rte_mbuf *buffer[PACKET_READ_SIZE];
	uint16_t count;
};

struct efd_stats {
	uint64_t distributed;
	uint64_t drop;
} flow_dist_stats;

/* One buffer per node rx queue - dynamically allocate array */
static struct node_rx_buf *cl_rx_buf;

static const char *
get_printable_mac_addr(uint16_t port)
{
	static const char err_address[] = "00:00:00:00:00:00";
	static char addresses[RTE_MAX_ETHPORTS][sizeof(err_address)];
	struct rte_ether_addr mac;
	int ret;

	if (unlikely(port >= RTE_MAX_ETHPORTS))
		return err_address;
	if (unlikely(addresses[port][0] == '\0')) {
		ret = rte_eth_macaddr_get(port, &mac);
		if (ret != 0) {
			printf("Failed to get MAC address (port %u): %s\n",
			       port, rte_strerror(-ret));
			return err_address;
		}

		snprintf(addresses[port], sizeof(addresses[port]),
				"%02x:%02x:%02x:%02x:%02x:%02x\n",
				mac.addr_bytes[0], mac.addr_bytes[1],
				mac.addr_bytes[2], mac.addr_bytes[3],
				mac.addr_bytes[4], mac.addr_bytes[5]);
	}
	return addresses[port];
}

/*
 * This function displays the recorded statistics for each port
 * and for each node. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single worker
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(void)
{
	unsigned int i, j;
	const char clr[] = {27, '[', '2', 'J', '\0'};
	const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};
	uint64_t port_tx[RTE_MAX_ETHPORTS], port_tx_drop[RTE_MAX_ETHPORTS];
	uint64_t node_tx[MAX_NODES], node_tx_drop[MAX_NODES];

	/* to get TX stats, we need to do some summing calculations */
	memset(port_tx, 0, sizeof(port_tx));
	memset(port_tx_drop, 0, sizeof(port_tx_drop));
	memset(node_tx, 0, sizeof(node_tx));
	memset(node_tx_drop, 0, sizeof(node_tx_drop));

	for (i = 0; i < num_nodes; i++) {
		const struct tx_stats *tx = &info->tx_stats[i];

		for (j = 0; j < info->num_ports; j++) {
			const uint64_t tx_val = tx->tx[info->id[j]];
			const uint64_t drop_val = tx->tx_drop[info->id[j]];

			port_tx[j] += tx_val;
			port_tx_drop[j] += drop_val;
			node_tx[i] += tx_val;
			node_tx_drop[i] += drop_val;
		}
	}

	/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("PORTS\n");
	printf("-----\n");
	for (i = 0; i < info->num_ports; i++)
		printf("Port %u: '%s'\t", (unsigned int)info->id[i],
				get_printable_mac_addr(info->id[i]));
	printf("\n\n");
	for (i = 0; i < info->num_ports; i++) {
		printf("Port %u - rx: %9"PRIu64"\t"
				"tx: %9"PRIu64"\n",
				(unsigned int)info->id[i], info->rx_stats.rx[i],
				port_tx[i]);
	}

	printf("\nSERVER\n");
	printf("-----\n");
	printf("distributed: %9"PRIu64", drop: %9"PRIu64"\n",
			flow_dist_stats.distributed, flow_dist_stats.drop);

	printf("\nNODES\n");
	printf("-------\n");
	for (i = 0; i < num_nodes; i++) {
		const unsigned long long rx = nodes[i].stats.rx;
		const unsigned long long rx_drop = nodes[i].stats.rx_drop;
		const struct filter_stats *filter = &info->filter_stats[i];

		printf("Node %2u - rx: %9llu, rx_drop: %9llu\n"
				"            tx: %9"PRIu64", tx_drop: %9"PRIu64"\n"
				"            filter_passed: %9"PRIu64", "
				"filter_drop: %9"PRIu64"\n",
				i, rx, rx_drop, node_tx[i], node_tx_drop[i],
				filter->passed, filter->drop);
	}

	printf("\n");
}

/*
 * The function called from each non-main lcore used by the process.
 * The test_and_set function is used to randomly pick a single lcore on which
 * the code to display the statistics will run. Otherwise, the code just
 * repeatedly sleeps.
 */
static int
sleep_lcore(__rte_unused void *dummy)
{
	/* Used to pick a display thread - static, so zero-initialised */
	static rte_atomic32_t display_stats;

	/* Only one core should display stats */
	if (rte_atomic32_test_and_set(&display_stats)) {
		const unsigned int sleeptime = 1;

		printf("Core %u displaying statistics\n", rte_lcore_id());

		/* Longer initial pause so above printf is seen */
		sleep(sleeptime * 3);

		/* Loop forever: sleep always returns 0 or <= param */
		while (sleep(sleeptime) <= sleeptime)
			do_stats_display();
	}
	return 0;
}

/*
 * Function to set all the node statistic values to zero.
 * Called at program startup.
 */
static void
clear_stats(void)
{
	unsigned int i;

	for (i = 0; i < num_nodes; i++)
		nodes[i].stats.rx = nodes[i].stats.rx_drop = 0;
}

/*
 * send a burst of traffic to a node, assuming there are packets
 * available to be sent to this node
 */
static void
flush_rx_queue(uint16_t node)
{
	uint16_t j;
	struct node *cl;

	if (cl_rx_buf[node].count == 0)
		return;

	cl = &nodes[node];
	if (rte_ring_enqueue_bulk(cl->rx_q, (void **)cl_rx_buf[node].buffer,
			cl_rx_buf[node].count, NULL) != cl_rx_buf[node].count){
		for (j = 0; j < cl_rx_buf[node].count; j++)
			rte_pktmbuf_free(cl_rx_buf[node].buffer[j]);
		cl->stats.rx_drop += cl_rx_buf[node].count;
	} else
		cl->stats.rx += cl_rx_buf[node].count;

	cl_rx_buf[node].count = 0;
}

/*
 * marks a packet down to be sent to a particular node process
 */
static inline void
enqueue_rx_packet(uint8_t node, struct rte_mbuf *buf)
{
	cl_rx_buf[node].buffer[cl_rx_buf[node].count++] = buf;
}

/*
 * This function takes a group of packets and routes them
 * individually to the node process. Very simply round-robins the packets
 * without checking any of the packet contents.
 */
static void
process_packets(uint32_t port_num __rte_unused, struct rte_mbuf *pkts[],
		uint16_t rx_count, unsigned int socket_id)
{
	uint16_t i;
	uint8_t node;
	efd_value_t data[RTE_EFD_BURST_MAX];
	const void *key_ptrs[RTE_EFD_BURST_MAX];

	struct rte_ipv4_hdr *ipv4_hdr;
	uint32_t ipv4_dst_ip[RTE_EFD_BURST_MAX];

	for (i = 0; i < rx_count; i++) {
		/* Handle IPv4 header.*/
		ipv4_hdr = rte_pktmbuf_mtod_offset(pkts[i],
			struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
		ipv4_dst_ip[i] = ipv4_hdr->dst_addr;
		key_ptrs[i] = (void *)&ipv4_dst_ip[i];
	}

	rte_efd_lookup_bulk(efd_table, socket_id, rx_count,
				(const void **) key_ptrs, data);
	for (i = 0; i < rx_count; i++) {
		node = (uint8_t) ((uintptr_t)data[i]);

		if (node >= num_nodes) {
			/*
			 * Node is out of range, which means that
			 * flow has not been inserted
			 */
			flow_dist_stats.drop++;
			rte_pktmbuf_free(pkts[i]);
		} else {
			flow_dist_stats.distributed++;
			enqueue_rx_packet(node, pkts[i]);
		}
	}

	for (i = 0; i < num_nodes; i++)
		flush_rx_queue(i);
}

/*
 * Function called by the main lcore of the DPDK process.
 */
static void
do_packet_forwarding(void)
{
	unsigned int port_num = 0; /* indexes the port[] array */
	unsigned int socket_id = rte_socket_id();

	for (;;) {
		struct rte_mbuf *buf[PACKET_READ_SIZE];
		uint16_t rx_count;

		/* read a port */
		rx_count = rte_eth_rx_burst(info->id[port_num], 0,
				buf, PACKET_READ_SIZE);
		info->rx_stats.rx[port_num] += rx_count;

		/* Now process the NIC packets read */
		if (likely(rx_count > 0))
			process_packets(port_num, buf, rx_count, socket_id);

		/* move to next port */
		if (++port_num == info->num_ports)
			port_num = 0;
	}
}

int
main(int argc, char *argv[])
{
	/* initialise the system */
	if (init(argc, argv) < 0)
		return -1;
	RTE_LOG(INFO, APP, "Finished Process Init.\n");

	cl_rx_buf = calloc(num_nodes, sizeof(cl_rx_buf[0]));

	/* clear statistics */
	clear_stats();

	/* put all other cores to sleep except main */
	rte_eal_mp_remote_launch(sleep_lcore, NULL, SKIP_MAIN);

	do_packet_forwarding();

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
