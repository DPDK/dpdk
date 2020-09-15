/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <errno.h>
#include <stdarg.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_byteorder.h>
#include <rte_atomic.h>
#include <rte_launch.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_debug.h>
#include <rte_ring.h>
#include <rte_log.h>
#include <rte_mempool.h>
#include <rte_memcpy.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_efd.h>
#include <rte_hash.h>

#include "common.h"
#include "args.h"
#include "init.h"

#define MBUFS_PER_NODE 1536
#define MBUFS_PER_PORT 1536
#define MBUF_CACHE_SIZE 512

#define RTE_MP_RX_DESC_DEFAULT 512
#define RTE_MP_TX_DESC_DEFAULT 512
#define NODE_QUEUE_RINGSIZE 128

#define NO_FLAGS 0

/* The mbuf pool for packet rx */
struct rte_mempool *pktmbuf_pool;

/* array of info/queues for nodes */
struct node *nodes;

/* EFD table */
struct rte_efd_table *efd_table;

/* Shared info between server and nodes */
struct shared_info *info;

/**
 * Initialise the mbuf pool for packet reception for the NIC, and any other
 * buffer pools needed by the app - currently none.
 */
static int
init_mbuf_pools(void)
{
	const unsigned int num_mbufs = (num_nodes * MBUFS_PER_NODE) +
			(info->num_ports * MBUFS_PER_PORT);

	/*
	 * Don't pass single-producer/single-consumer flags to mbuf create as it
	 * seems faster to use a cache instead
	 */
	printf("Creating mbuf pool '%s' [%u mbufs] ...\n",
			PKTMBUF_POOL_NAME, num_mbufs);
	pktmbuf_pool = rte_pktmbuf_pool_create(PKTMBUF_POOL_NAME, num_mbufs,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	return pktmbuf_pool == NULL; /* 0  on success */
}

/**
 * Initialise an individual port:
 * - configure number of rx and tx rings
 * - set up each rx ring, to pull from the main mbuf pool
 * - set up each tx ring
 * - start the port and report its status to stdout
 */
static int
init_port(uint16_t port_num)
{
	/* for port configuration all features are off by default */
	struct rte_eth_conf port_conf = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_RSS,
		},
	};
	const uint16_t rx_rings = 1, tx_rings = num_nodes;
	uint16_t rx_ring_size = RTE_MP_RX_DESC_DEFAULT;
	uint16_t tx_ring_size = RTE_MP_TX_DESC_DEFAULT;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;

	uint16_t q;
	int retval;

	printf("Port %u init ... ", port_num);
	fflush(stdout);

	retval = rte_eth_dev_info_get(port_num, &dev_info);
	if (retval != 0)
		return retval;

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	/*
	 * Standard DPDK port initialisation - config port, then set up
	 * rx and tx rings.
	 */
	retval = rte_eth_dev_configure(port_num, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port_num, &rx_ring_size,
			&tx_ring_size);
	if (retval != 0)
		return retval;

	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port_num, q, rx_ring_size,
				rte_eth_dev_socket_id(port_num),
				NULL, pktmbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port_num, q, tx_ring_size,
				rte_eth_dev_socket_id(port_num),
				&txconf);
		if (retval < 0)
			return retval;
	}

	retval = rte_eth_promiscuous_enable(port_num);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_start(port_num);
	if (retval < 0)
		return retval;

	printf("done:\n");

	return 0;
}

/**
 * Set up the DPDK rings which will be used to pass packets, via
 * pointers, between the multi-process server and node processes.
 * Each node needs one RX queue.
 */
static int
init_shm_rings(void)
{
	unsigned int i;
	unsigned int socket_id;
	const char *q_name;
	const unsigned int ringsize = NODE_QUEUE_RINGSIZE;

	nodes = rte_malloc("node details",
		sizeof(*nodes) * num_nodes, 0);
	if (nodes == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate memory for "
				"node program details\n");

	for (i = 0; i < num_nodes; i++) {
		/* Create an RX queue for each node */
		socket_id = rte_socket_id();
		q_name = get_rx_queue_name(i);
		nodes[i].rx_q = rte_ring_create(q_name,
				ringsize, socket_id,
				RING_F_SP_ENQ | RING_F_SC_DEQ);
		if (nodes[i].rx_q == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create rx ring queue "
					"for node %u\n", i);
	}
	return 0;
}

/*
 * Create EFD table which will contain all the flows
 * that will be distributed among the nodes
 */
static void
create_efd_table(void)
{
	uint8_t socket_id = rte_socket_id();

	/* create table */
	efd_table = rte_efd_create("flow table", num_flows * 2, sizeof(uint32_t),
			1 << socket_id,	socket_id);

	if (efd_table == NULL)
		rte_exit(EXIT_FAILURE, "Problem creating the flow table\n");
}

static void
populate_efd_table(void)
{
	unsigned int i;
	int32_t ret;
	uint32_t ip_dst;
	uint8_t socket_id = rte_socket_id();
	uint64_t node_id;

	/* Add flows in table */
	for (i = 0; i < num_flows; i++) {
		node_id = i % num_nodes;

		ip_dst = rte_cpu_to_be_32(i);
		ret = rte_efd_update(efd_table, socket_id,
				(void *)&ip_dst, (efd_value_t)node_id);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Unable to add entry %u in "
					"EFD table\n", i);
	}

	printf("EFD table: Adding 0x%x keys\n", num_flows);
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint16_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t count, all_ports_up, print_flag = 0;
	uint16_t portid;
	struct rte_eth_link link;
	int ret;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << info->id[portid])) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			ret = rte_eth_link_get_nowait(info->id[portid], &link);
			if (ret < 0) {
				all_ports_up = 0;
				if (print_flag == 1)
					printf("Port %u link get failed: %s\n",
						portid, rte_strerror(-ret));
				continue;
			}
			/* print link status if flag set */
			if (print_flag == 1) {
				rte_eth_link_to_str(link_status_text,
					sizeof(link_status_text), &link);
				printf("Port %d %s\n", info->id[portid],
				       link_status_text);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1)
			break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printf("done\n");
		}
	}
}

/**
 * Main init function for the multi-process server app,
 * calls subfunctions to do each stage of the initialisation.
 */
int
init(int argc, char *argv[])
{
	int retval;
	const struct rte_memzone *mz;
	uint8_t i, total_ports;

	/* init EAL, parsing EAL args */
	retval = rte_eal_init(argc, argv);
	if (retval < 0)
		return -1;
	argc -= retval;
	argv += retval;

	/* get total number of ports */
	total_ports = rte_eth_dev_count_avail();

	/* set up array for port data */
	mz = rte_memzone_reserve(MZ_SHARED_INFO, sizeof(*info),
				rte_socket_id(), NO_FLAGS);
	if (mz == NULL)
		rte_exit(EXIT_FAILURE, "Cannot reserve memory zone "
				"for port information\n");
	memset(mz->addr, 0, sizeof(*info));
	info = mz->addr;

	/* parse additional, application arguments */
	retval = parse_app_args(total_ports, argc, argv);
	if (retval != 0)
		return -1;

	/* initialise mbuf pools */
	retval = init_mbuf_pools();
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "Cannot create needed mbuf pools\n");

	/* now initialise the ports we will use */
	for (i = 0; i < info->num_ports; i++) {
		retval = init_port(info->id[i]);
		if (retval != 0)
			rte_exit(EXIT_FAILURE, "Cannot initialise port %u\n",
					(unsigned int) i);
	}

	check_all_ports_link_status(info->num_ports, (~0x0));

	/* initialise the node queues/rings for inter-eu comms */
	init_shm_rings();

	/* Create the EFD table */
	create_efd_table();

	/* Populate the EFD table */
	populate_efd_table();

	/* Share the total number of nodes */
	info->num_nodes = num_nodes;

	/* Share the total number of flows */
	info->num_flows = num_flows;
	return 0;
}
