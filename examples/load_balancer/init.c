/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_lpm.h>

#include "main.h"

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode	= ETH_MQ_RX_RSS,
		.split_hdr_size = 0,
		.ignore_offload_bitfield = 1,
		.offloads = (DEV_RX_OFFLOAD_CHECKSUM |
			     DEV_RX_OFFLOAD_CRC_STRIP),
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = ETH_RSS_IP,
		},
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

static void
app_assign_worker_ids(void)
{
	uint32_t lcore, worker_id;

	/* Assign ID for each worker */
	worker_id = 0;
	for (lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		struct app_lcore_params_worker *lp_worker = &app.lcore_params[lcore].worker;

		if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
			continue;
		}

		lp_worker->worker_id = worker_id;
		worker_id ++;
	}
}

static void
app_init_mbuf_pools(void)
{
	unsigned socket, lcore;

	/* Init the buffer pools */
	for (socket = 0; socket < APP_MAX_SOCKETS; socket ++) {
		char name[32];
		if (app_is_socket_used(socket) == 0) {
			continue;
		}

		snprintf(name, sizeof(name), "mbuf_pool_%u", socket);
		printf("Creating the mbuf pool for socket %u ...\n", socket);
		app.pools[socket] = rte_pktmbuf_pool_create(
			name, APP_DEFAULT_MEMPOOL_BUFFERS,
			APP_DEFAULT_MEMPOOL_CACHE_SIZE,
			0, APP_DEFAULT_MBUF_DATA_SIZE, socket);
		if (app.pools[socket] == NULL) {
			rte_panic("Cannot create mbuf pool on socket %u\n", socket);
		}
	}

	for (lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		if (app.lcore_params[lcore].type == e_APP_LCORE_DISABLED) {
			continue;
		}

		socket = rte_lcore_to_socket_id(lcore);
		app.lcore_params[lcore].pool = app.pools[socket];
	}
}

static void
app_init_lpm_tables(void)
{
	unsigned socket, lcore;

	/* Init the LPM tables */
	for (socket = 0; socket < APP_MAX_SOCKETS; socket ++) {
		char name[32];
		uint32_t rule;

		if (app_is_socket_used(socket) == 0) {
			continue;
		}

		struct rte_lpm_config lpm_config;

		lpm_config.max_rules = APP_MAX_LPM_RULES;
		lpm_config.number_tbl8s = 256;
		lpm_config.flags = 0;
		snprintf(name, sizeof(name), "lpm_table_%u", socket);
		printf("Creating the LPM table for socket %u ...\n", socket);
		app.lpm_tables[socket] = rte_lpm_create(
			name,
			socket,
			&lpm_config);
		if (app.lpm_tables[socket] == NULL) {
			rte_panic("Unable to create LPM table on socket %u\n", socket);
		}

		for (rule = 0; rule < app.n_lpm_rules; rule ++) {
			int ret;

			ret = rte_lpm_add(app.lpm_tables[socket],
				app.lpm_rules[rule].ip,
				app.lpm_rules[rule].depth,
				app.lpm_rules[rule].if_out);

			if (ret < 0) {
				rte_panic("Unable to add entry %u (%x/%u => %u) to the LPM table on socket %u (%d)\n",
					(unsigned) rule,
					(unsigned) app.lpm_rules[rule].ip,
					(unsigned) app.lpm_rules[rule].depth,
					(unsigned) app.lpm_rules[rule].if_out,
					socket,
					ret);
			}
		}

	}

	for (lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
			continue;
		}

		socket = rte_lcore_to_socket_id(lcore);
		app.lcore_params[lcore].worker.lpm_table = app.lpm_tables[socket];
	}
}

static void
app_init_rings_rx(void)
{
	unsigned lcore;

	/* Initialize the rings for the RX side */
	for (lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		struct app_lcore_params_io *lp_io = &app.lcore_params[lcore].io;
		unsigned socket_io, lcore_worker;

		if ((app.lcore_params[lcore].type != e_APP_LCORE_IO) ||
		    (lp_io->rx.n_nic_queues == 0)) {
			continue;
		}

		socket_io = rte_lcore_to_socket_id(lcore);

		for (lcore_worker = 0; lcore_worker < APP_MAX_LCORES; lcore_worker ++) {
			char name[32];
			struct app_lcore_params_worker *lp_worker = &app.lcore_params[lcore_worker].worker;
			struct rte_ring *ring = NULL;

			if (app.lcore_params[lcore_worker].type != e_APP_LCORE_WORKER) {
				continue;
			}

			printf("Creating ring to connect I/O lcore %u (socket %u) with worker lcore %u ...\n",
				lcore,
				socket_io,
				lcore_worker);
			snprintf(name, sizeof(name), "app_ring_rx_s%u_io%u_w%u",
				socket_io,
				lcore,
				lcore_worker);
			ring = rte_ring_create(
				name,
				app.ring_rx_size,
				socket_io,
				RING_F_SP_ENQ | RING_F_SC_DEQ);
			if (ring == NULL) {
				rte_panic("Cannot create ring to connect I/O core %u with worker core %u\n",
					lcore,
					lcore_worker);
			}

			lp_io->rx.rings[lp_io->rx.n_rings] = ring;
			lp_io->rx.n_rings ++;

			lp_worker->rings_in[lp_worker->n_rings_in] = ring;
			lp_worker->n_rings_in ++;
		}
	}

	for (lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		struct app_lcore_params_io *lp_io = &app.lcore_params[lcore].io;

		if ((app.lcore_params[lcore].type != e_APP_LCORE_IO) ||
		    (lp_io->rx.n_nic_queues == 0)) {
			continue;
		}

		if (lp_io->rx.n_rings != app_get_lcores_worker()) {
			rte_panic("Algorithmic error (I/O RX rings)\n");
		}
	}

	for (lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		struct app_lcore_params_worker *lp_worker = &app.lcore_params[lcore].worker;

		if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
			continue;
		}

		if (lp_worker->n_rings_in != app_get_lcores_io_rx()) {
			rte_panic("Algorithmic error (worker input rings)\n");
		}
	}
}

static void
app_init_rings_tx(void)
{
	unsigned lcore;

	/* Initialize the rings for the TX side */
	for (lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		struct app_lcore_params_worker *lp_worker = &app.lcore_params[lcore].worker;
		unsigned port;

		if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
			continue;
		}

		for (port = 0; port < APP_MAX_NIC_PORTS; port ++) {
			char name[32];
			struct app_lcore_params_io *lp_io = NULL;
			struct rte_ring *ring;
			uint32_t socket_io, lcore_io;

			if (app.nic_tx_port_mask[port] == 0) {
				continue;
			}

			if (app_get_lcore_for_nic_tx(port, &lcore_io) < 0) {
				rte_panic("Algorithmic error (no I/O core to handle TX of port %u)\n",
					port);
			}

			lp_io = &app.lcore_params[lcore_io].io;
			socket_io = rte_lcore_to_socket_id(lcore_io);

			printf("Creating ring to connect worker lcore %u with TX port %u (through I/O lcore %u) (socket %u) ...\n",
				lcore, port, (unsigned)lcore_io, (unsigned)socket_io);
			snprintf(name, sizeof(name), "app_ring_tx_s%u_w%u_p%u", socket_io, lcore, port);
			ring = rte_ring_create(
				name,
				app.ring_tx_size,
				socket_io,
				RING_F_SP_ENQ | RING_F_SC_DEQ);
			if (ring == NULL) {
				rte_panic("Cannot create ring to connect worker core %u with TX port %u\n",
					lcore,
					port);
			}

			lp_worker->rings_out[port] = ring;
			lp_io->tx.rings[port][lp_worker->worker_id] = ring;
		}
	}

	for (lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		struct app_lcore_params_io *lp_io = &app.lcore_params[lcore].io;
		unsigned i;

		if ((app.lcore_params[lcore].type != e_APP_LCORE_IO) ||
		    (lp_io->tx.n_nic_ports == 0)) {
			continue;
		}

		for (i = 0; i < lp_io->tx.n_nic_ports; i ++){
			unsigned port, j;

			port = lp_io->tx.nic_ports[i];
			for (j = 0; j < app_get_lcores_worker(); j ++) {
				if (lp_io->tx.rings[port][j] == NULL) {
					rte_panic("Algorithmic error (I/O TX rings)\n");
				}
			}
		}
	}
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint16_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	uint32_t n_rx_queues, n_tx_queues;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			n_rx_queues = app_get_nic_rx_queues_per_port(portid);
			n_tx_queues = app.nic_tx_port_mask[portid];
			if ((n_rx_queues == 0) && (n_tx_queues == 0))
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf(
					"Port%d Link Up - speed %uMbps - %s\n",
						portid, link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n", portid);
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

static void
app_init_nics(void)
{
	unsigned socket;
	uint32_t lcore;
	uint16_t port;
	uint8_t queue;
	int ret;
	uint32_t n_rx_queues, n_tx_queues;

	/* Init NIC ports and queues, then start the ports */
	for (port = 0; port < APP_MAX_NIC_PORTS; port ++) {
		struct rte_mempool *pool;
		uint16_t nic_rx_ring_size;
		uint16_t nic_tx_ring_size;
		struct rte_eth_rxconf rxq_conf;
		struct rte_eth_txconf txq_conf;
		struct rte_eth_dev_info dev_info;
		struct rte_eth_conf local_port_conf = port_conf;

		n_rx_queues = app_get_nic_rx_queues_per_port(port);
		n_tx_queues = app.nic_tx_port_mask[port];

		if ((n_rx_queues == 0) && (n_tx_queues == 0)) {
			continue;
		}

		/* Init port */
		printf("Initializing NIC port %u ...\n", port);
		rte_eth_dev_info_get(port, &dev_info);
		if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				DEV_TX_OFFLOAD_MBUF_FAST_FREE;
		ret = rte_eth_dev_configure(
			port,
			(uint8_t) n_rx_queues,
			(uint8_t) n_tx_queues,
			&local_port_conf);
		if (ret < 0) {
			rte_panic("Cannot init NIC port %u (%d)\n", port, ret);
		}
		rte_eth_promiscuous_enable(port);

		nic_rx_ring_size = app.nic_rx_ring_size;
		nic_tx_ring_size = app.nic_tx_ring_size;
		ret = rte_eth_dev_adjust_nb_rx_tx_desc(
			port, &nic_rx_ring_size, &nic_tx_ring_size);
		if (ret < 0) {
			rte_panic("Cannot adjust number of descriptors for port %u (%d)\n",
				  port, ret);
		}
		app.nic_rx_ring_size = nic_rx_ring_size;
		app.nic_tx_ring_size = nic_tx_ring_size;

		rxq_conf = dev_info.default_rxconf;
		rxq_conf.offloads = local_port_conf.rxmode.offloads;
		/* Init RX queues */
		for (queue = 0; queue < APP_MAX_RX_QUEUES_PER_NIC_PORT; queue ++) {
			if (app.nic_rx_queue_mask[port][queue] == 0) {
				continue;
			}

			app_get_lcore_for_nic_rx(port, queue, &lcore);
			socket = rte_lcore_to_socket_id(lcore);
			pool = app.lcore_params[lcore].pool;

			printf("Initializing NIC port %u RX queue %u ...\n",
				port, queue);
			ret = rte_eth_rx_queue_setup(
				port,
				queue,
				(uint16_t) app.nic_rx_ring_size,
				socket,
				&rxq_conf,
				pool);
			if (ret < 0) {
				rte_panic("Cannot init RX queue %u for port %u (%d)\n",
					  queue, port, ret);
			}
		}

		txq_conf = dev_info.default_txconf;
		txq_conf.txq_flags = ETH_TXQ_FLAGS_IGNORE;
		txq_conf.offloads = local_port_conf.txmode.offloads;
		/* Init TX queues */
		if (app.nic_tx_port_mask[port] == 1) {
			app_get_lcore_for_nic_tx(port, &lcore);
			socket = rte_lcore_to_socket_id(lcore);
			printf("Initializing NIC port %u TX queue 0 ...\n",
				port);
			ret = rte_eth_tx_queue_setup(
				port,
				0,
				(uint16_t) app.nic_tx_ring_size,
				socket,
				&txq_conf);
			if (ret < 0) {
				rte_panic("Cannot init TX queue 0 for port %d (%d)\n",
					port,
					ret);
			}
		}

		/* Start port */
		ret = rte_eth_dev_start(port);
		if (ret < 0) {
			rte_panic("Cannot start port %d (%d)\n", port, ret);
		}
	}

	check_all_ports_link_status(APP_MAX_NIC_PORTS, (~0x0));
}

void
app_init(void)
{
	app_assign_worker_ids();
	app_init_mbuf_pools();
	app_init_lpm_tables();
	app_init_rings_rx();
	app_init_rings_tx();
	app_init_nics();

	printf("Initialization completed.\n");
}
