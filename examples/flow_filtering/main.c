/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_net.h>
#include <rte_flow.h>
#include <rte_cycles.h>
#include <rte_argparse.h>

#include "common.h"
#include "snippets/snippet_match_ipv4.h"

bool enable_promiscuous_mode = true; /* default configuration of promiscuous mode */
bool enable_flow_isolation; /* some snippet may need isolation mode instead of promiscuous mode */

/* Template API enabled by default. */
static int use_template_api = 1;

static volatile bool force_quit;
static uint16_t port_id;
static uint16_t nr_queues = 5;
struct rte_mempool *mbuf_pool;
struct rte_flow *flow;

#define MAX_QUEUE_SIZE 256

static inline void
print_ether_addr(const char *what, struct rte_ether_addr *eth_addr)
{
	char buf[RTE_ETHER_ADDR_FMT_SIZE];
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", what, buf);
}

static int
main_loop(void)
{
	struct rte_mbuf *mbufs[32];
	struct rte_ether_hdr *eth_hdr;
	struct rte_flow_error error;
	uint16_t nb_rx;
	uint16_t i;
	uint16_t j;
	int ret;

	/* Reading the packets from all queues. */
	while (!force_quit) {
		for (i = 0; i < nr_queues; i++) {
			nb_rx = rte_eth_rx_burst(port_id,
						i, mbufs, 32);
			if (nb_rx) {
				for (j = 0; j < nb_rx; j++) {
					struct rte_mbuf *m = mbufs[j];

					eth_hdr = rte_pktmbuf_mtod(m,
							struct rte_ether_hdr *);
					print_ether_addr("src=",
							&eth_hdr->src_addr);
					print_ether_addr(" - dst=",
							&eth_hdr->dst_addr);
					printf(" - queue=0x%x",
							(unsigned int)i);
					printf("\n");

					rte_pktmbuf_free(m);
				}
			}
		}
	}

	/* closing and releasing resources */
	rte_flow_flush(port_id, &error);
	ret = rte_eth_dev_stop(port_id);
	if (ret < 0)
		printf("Failed to stop port %u: %s",
			   port_id, rte_strerror(-ret));
	rte_eth_dev_close(port_id);
	return ret;
}

#define CHECK_INTERVAL 1000  /* 100ms */
#define MAX_REPEAT_TIMES 90  /* 9s (90 * 100ms) in total */

static void
assert_link_status(void)
{
	struct rte_eth_link link;
	uint8_t rep_cnt = MAX_REPEAT_TIMES;
	int link_get_err = -EINVAL;

	memset(&link, 0, sizeof(link));
	do {
		link_get_err = rte_eth_link_get(port_id, &link);
		if (link_get_err == 0 && link.link_status == RTE_ETH_LINK_UP)
			break;
		rte_delay_ms(CHECK_INTERVAL);
	} while (--rep_cnt);

	if (link_get_err < 0)
		rte_exit(EXIT_FAILURE, ":: error: link get is failing: %s\n",
			 rte_strerror(-link_get_err));
	if (link.link_status == RTE_ETH_LINK_DOWN)
		rte_exit(EXIT_FAILURE, ":: error: link is still down\n");
}

static void
configure_port_template(uint16_t port_id)
{
	int ret;
	uint16_t std_queue;
	struct rte_flow_error error;
	struct rte_flow_queue_attr queue_attr[RTE_MAX_LCORE];
	const struct rte_flow_queue_attr *attr_list[RTE_MAX_LCORE];
	struct rte_flow_port_attr port_attr = { .nb_counters = 1 /* rules count */ };

	for (std_queue = 0; std_queue < RTE_MAX_LCORE; std_queue++) {
		queue_attr[std_queue].size = MAX_QUEUE_SIZE;
		attr_list[std_queue] = &queue_attr[std_queue];
	}

	ret = rte_flow_configure(port_id, &port_attr,
				 1, attr_list, &error);
	if (ret != 0)
		rte_exit(EXIT_FAILURE,
			 "rte_flow_configure:err=%d, port=%u\n",
			 ret, port_id);
	printf(":: Configuring template port [%d] Done ..\n", port_id);
}

static void
init_port(void)
{
	int ret;
	uint16_t i;
	/* Ethernet port configured with default settings. */
	struct rte_eth_conf port_conf = {
		.txmode = {
			.offloads =
				RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
				RTE_ETH_TX_OFFLOAD_IPV4_CKSUM  |
				RTE_ETH_TX_OFFLOAD_UDP_CKSUM   |
				RTE_ETH_TX_OFFLOAD_TCP_CKSUM   |
				RTE_ETH_TX_OFFLOAD_SCTP_CKSUM  |
				RTE_ETH_TX_OFFLOAD_TCP_TSO,
		},
	};
	struct rte_eth_txconf txq_conf;
	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_dev_info dev_info;

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0)
		rte_exit(EXIT_FAILURE,
			"Error during getting device (port %u) info: %s\n",
			port_id, strerror(-ret));

	port_conf.txmode.offloads &= dev_info.tx_offload_capa;
	printf(":: initializing port: %d\n", port_id);
	ret = rte_eth_dev_configure(port_id,
				nr_queues, nr_queues, &port_conf);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE,
			":: cannot configure device: err=%d, port=%u\n",
			ret, port_id);
	}

	rxq_conf = dev_info.default_rxconf;
	rxq_conf.offloads = port_conf.rxmode.offloads;

	/* Configuring number of RX and TX queues connected to single port. */
	for (i = 0; i < nr_queues; i++) {
		ret = rte_eth_rx_queue_setup(port_id, i, 512,
					 rte_eth_dev_socket_id(port_id),
					 &rxq_conf,
					 mbuf_pool);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE,
				":: Rx queue setup failed: err=%d, port=%u\n",
				ret, port_id);
		}
	}

	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = port_conf.txmode.offloads;

	for (i = 0; i < nr_queues; i++) {
		ret = rte_eth_tx_queue_setup(port_id, i, 512,
				rte_eth_dev_socket_id(port_id),
				&txq_conf);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE,
				":: Tx queue setup failed: err=%d, port=%u\n",
				ret, port_id);
		}
	}

	if (enable_promiscuous_mode) {
		/* Setting the RX port to promiscuous mode. */
		ret = rte_eth_promiscuous_enable(port_id);
		printf(":: promiscuous mode enabled\n");
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				":: promiscuous mode enable failed: err=%s, port=%u\n",
				rte_strerror(-ret), port_id);
	} else if (enable_flow_isolation) {
		/* Setting the RX port to isolate mode. */
		ret = rte_flow_isolate(port_id, 1, NULL);
		printf(":: isolate mode enabled\n");
		if (ret != 0)
			rte_exit(EXIT_FAILURE,
				":: isolate mode enable failed: err=%s, port=%u\n",
				rte_strerror(-ret), port_id);
	}

	ret = rte_eth_dev_start(port_id);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE,
			"rte_eth_dev_start:err=%d, port=%u\n",
			ret, port_id);
	}

	assert_link_status();

	printf(":: initializing port: %d done\n", port_id);

	if (use_template_api == 0)
		return;

	/* Adds rules engine configuration. 8< */
	ret = rte_eth_dev_stop(port_id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			"rte_eth_dev_stop:err=%d, port=%u\n",
			ret, port_id);

	configure_port_template(port_id);
	ret = rte_eth_dev_start(port_id);
	if (ret < 0)
		rte_exit(EXIT_FAILURE,
			"rte_eth_dev_start:err=%d, port=%u\n",
			ret, port_id);
	/* >8 End of adding rules engine configuration. */
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

/* Parse the argument given in the command line of the application */
static int
flow_filtering_parse_args(int argc, char **argv)
{
	static struct rte_argparse obj = {
		.prog_name = "flow_filtering",
		.usage = "[EAL options] -- [optional parameters]",
		.descriptor = NULL,
		.epilog = NULL,
		.exit_on_error = false,
		.callback = NULL,
		.opaque = NULL,
		.args = {
			{ "--template", NULL, "Enable template API flow",
			  &use_template_api, (void *)1,
			  RTE_ARGPARSE_VALUE_NONE, RTE_ARGPARSE_VALUE_TYPE_INT,
			},
			{ "--non-template", NULL, "Enable non template API flow",
			  &use_template_api, (void *)0,
			  RTE_ARGPARSE_VALUE_NONE, RTE_ARGPARSE_VALUE_TYPE_INT,
			},
			ARGPARSE_ARG_END(),
		},
	};

	return rte_argparse_parse(&obj, argc, argv);
}

int
main(int argc, char **argv)
{
	int ret;
	uint16_t nr_ports;
	struct rte_flow_error error;

	/* Initialize EAL. 8< */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, ":: invalid EAL arguments\n");
	/* >8 End of Initialization of EAL. */
	argc -= ret;
	argv += ret;

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Parse application arguments (after the EAL ones) */
	ret = flow_filtering_parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid flow filtering arguments\n");

	nr_ports = rte_eth_dev_count_avail();
	if (nr_ports == 0)
		rte_exit(EXIT_FAILURE, ":: no Ethernet ports found\n");
	port_id = 0;
	if (nr_ports != 1) {
		printf(":: warn: %d ports detected, but we use only one: port %u\n",
			nr_ports, port_id);
	}

	/* Allocates a mempool to hold the mbufs. 8< */
	mbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", 4096, 128, 0,
						RTE_MBUF_DEFAULT_BUF_SIZE,
						rte_socket_id());
	/* >8 End of allocating a mempool to hold the mbufs. */
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

	/* Add snippet-specific configuration. 8< */
	snippet_init();
	/* >8 End of snippet-specific configuration. */

	/* Initializes all the ports using the user defined init_port(). 8< */
	init_port();
	/* >8 End of Initializing the ports using user defined init_port(). */

	/* Function responsible for creating the flow rule. 8< */
	flow = generate_flow_skeleton(port_id, &error, use_template_api);
	/* >8 End of function responsible for creating the flow rule. */

	if (!flow) {
		printf("Flow can't be created %d message: %s\n",
			error.type,
			error.message ? error.message : "(no stated reason)");
		rte_exit(EXIT_FAILURE, "error in creating flow");
	}
	printf("Flow created!!:\n");

	/* Launching main_loop(). 8< */
	ret = main_loop();
	/* >8 End of launching main_loop(). */

	/* clean up the EAL */
	rte_eal_cleanup();

	return ret;
}
