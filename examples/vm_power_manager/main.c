/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#include <sys/queue.h>

#include <rte_common.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_log.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_ethdev.h>
#include <getopt.h>
#include <rte_cycles.h>
#include <rte_debug.h>

#include "channel_manager.h"
#include "channel_monitor.h"
#include "power_manager.h"
#include "vm_power_cli.h"
#include "oob_monitor.h"
#include "parse.h"
#ifdef RTE_LIBRTE_IXGBE_PMD
#include <rte_pmd_ixgbe.h>
#endif
#ifdef RTE_LIBRTE_I40E_PMD
#include <rte_pmd_i40e.h>
#endif
#ifdef RTE_LIBRTE_BNXT_PMD
#include <rte_pmd_bnxt.h>
#endif

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static uint32_t enabled_port_mask;
static volatile bool force_quit;

/****************/
static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
	},
};

static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txq_conf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0) {
		printf("Error during getting device (port %u) info: %s\n",
				port, strerror(-retval));
		return retval;
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = port_conf.txmode.offloads;
	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
				rte_eth_dev_socket_id(port), &txq_conf);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct rte_ether_addr addr;
	retval = rte_eth_macaddr_get(port, &addr);
	if (retval != 0) {
		printf("Failed to get device (port %u) MAC address: %s\n",
				port, rte_strerror(-retval));
		return retval;
	}

	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned int)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
		return retval;


	return 0;
}

static int
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}
/* Parse the argument given in the command line of the application */
static int
parse_args(int argc, char **argv)
{
	int opt, ret, cnt, i;
	char **argvopt;
	uint16_t *oob_enable;
	int option_index;
	char *prgname = argv[0];
	struct core_info *ci;
	float branch_ratio;
	static struct option lgopts[] = {
		{ "mac-updating", no_argument, 0, 1},
		{ "no-mac-updating", no_argument, 0, 0},
		{ "core-branch-ratio", optional_argument, 0, 'b'},
		{ "port-list", optional_argument, 0, 'p'},
		{NULL, 0, 0, 0}
	};
	argvopt = argv;
	ci = get_core_info();

	while ((opt = getopt_long(argc, argvopt, "p:q:T:b:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			enabled_port_mask = parse_portmask(optarg);
			if (enabled_port_mask == 0) {
				printf("invalid portmask\n");
				return -1;
			}
			break;
		case 'b':
			branch_ratio = BRANCH_RATIO_THRESHOLD;
			oob_enable = malloc(ci->core_count * sizeof(uint16_t));
			if (oob_enable == NULL) {
				printf("Error - Unable to allocate memory\n");
				return -1;
			}
			cnt = parse_set(optarg, oob_enable, ci->core_count);
			if (cnt < 0) {
				printf("Invalid core-list section in "
				       "core-branch-ratio matrix - [%s]\n",
						optarg);
				free(oob_enable);
				break;
			}
			cnt = parse_branch_ratio(optarg, &branch_ratio);
			if (cnt < 0) {
				printf("Invalid branch-ratio section in "
				       "core-branch-ratio matrix - [%s]\n",
						optarg);
				free(oob_enable);
				break;
			}
			if (branch_ratio <= 0.0 || branch_ratio > 100.0) {
				printf("invalid branch ratio specified\n");
				return -1;
			}
			for (i = 0; i < ci->core_count; i++) {
				if (oob_enable[i]) {
					printf("***Using core %d "
					       "with branch ratio %f\n",
					       i, branch_ratio);
					ci->cd[i].oob_enabled = 1;
					ci->cd[i].global_enabled_cpus = 1;
					ci->cd[i].branch_ratio_threshold =
								branch_ratio;
				}
			}
			free(oob_enable);
			break;
		/* long options */
		case 0:
			break;

		default:
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 0; /* reset getopt lib */
	return ret;
}

static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	int ret;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		if (force_quit)
			return;
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if (force_quit)
				return;
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			ret = rte_eth_link_get_nowait(portid, &link);
			if (ret < 0) {
				all_ports_up = 0;
				if (print_flag == 1)
					printf("Port %u link get failed: %s\n",
						portid, rte_strerror(-ret));
				continue;
			}
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint16_t)portid,
						(unsigned int)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex"));
				else
					printf("Port %d Link Down\n",
						(uint16_t)portid);
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
static int
run_monitor(__rte_unused void *arg)
{
	if (channel_monitor_init() < 0) {
		printf("Unable to initialize channel monitor\n");
		return -1;
	}
	run_channel_monitor();
	return 0;
}

static int
run_core_monitor(__rte_unused void *arg)
{
	if (branch_monitor_init() < 0) {
		printf("Unable to initialize core monitor\n");
		return -1;
	}
	run_branch_monitor();
	return 0;
}

static void
sig_handler(int signo)
{
	printf("Received signal %d, exiting...\n", signo);
	channel_monitor_exit();
	channel_manager_exit();
	power_manager_exit();

}

int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;
	unsigned int nb_ports;
	struct rte_mempool *mbuf_pool;
	uint16_t portid;
	struct core_info *ci;


	ret = core_info_init();
	if (ret < 0)
		rte_panic("Cannot allocate core info\n");

	ci = get_core_info();

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid arguments\n");

	nb_ports = rte_eth_dev_count_avail();

	if (nb_ports > 0) {
		mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
				NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
				RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

		if (mbuf_pool == NULL)
			rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

		/* Initialize ports. */
		RTE_ETH_FOREACH_DEV(portid) {
			struct rte_ether_addr eth;
			int w, j;
			int ret;

			if ((enabled_port_mask & (1 << portid)) == 0)
				continue;

			eth.addr_bytes[0] = 0xe0;
			eth.addr_bytes[1] = 0xe0;
			eth.addr_bytes[2] = 0xe0;
			eth.addr_bytes[3] = 0xe0;
			eth.addr_bytes[4] = portid + 0xf0;

			if (port_init(portid, mbuf_pool) != 0)
				rte_exit(EXIT_FAILURE,
					"Cannot init port %"PRIu8 "\n",
					portid);

			for (w = 0; w < MAX_VFS; w++) {
				eth.addr_bytes[5] = w + 0xf0;

				ret = -ENOTSUP;
#ifdef RTE_LIBRTE_IXGBE_PMD
				ret = rte_pmd_ixgbe_set_vf_mac_addr(portid,
							w, &eth);
#endif
#ifdef RTE_LIBRTE_I40E_PMD
				if (ret == -ENOTSUP)
					ret = rte_pmd_i40e_set_vf_mac_addr(
							portid, w, &eth);
#endif
#ifdef RTE_LIBRTE_BNXT_PMD
				if (ret == -ENOTSUP)
					ret = rte_pmd_bnxt_set_vf_mac_addr(
							portid, w, &eth);
#endif

				switch (ret) {
				case 0:
					printf("Port %d VF %d MAC: ",
							portid, w);
					for (j = 0; j < 5; j++) {
						printf("%02x:",
							eth.addr_bytes[j]);
					}
					printf("%02x\n", eth.addr_bytes[5]);
					break;
				}
				printf("\n");
			}
		}
	}

	check_all_ports_link_status(enabled_port_mask);

	lcore_id = rte_get_next_lcore(-1, 1, 0);
	if (lcore_id == RTE_MAX_LCORE) {
		RTE_LOG(ERR, EAL, "A minimum of three cores are required to run "
				"application\n");
		return 0;
	}
	printf("Running channel monitor on lcore id %d\n", lcore_id);
	rte_eal_remote_launch(run_monitor, NULL, lcore_id);

	lcore_id = rte_get_next_lcore(lcore_id, 1, 0);
	if (lcore_id == RTE_MAX_LCORE) {
		RTE_LOG(ERR, EAL, "A minimum of three cores are required to run "
				"application\n");
		return 0;
	}
	if (power_manager_init() < 0) {
		printf("Unable to initialize power manager\n");
		return -1;
	}
	if (channel_manager_init(CHANNEL_MGR_DEFAULT_HV_PATH) < 0) {
		printf("Unable to initialize channel manager\n");
		return -1;
	}

	add_host_channels();

	printf("Running core monitor on lcore id %d\n", lcore_id);
	rte_eal_remote_launch(run_core_monitor, NULL, lcore_id);

	run_cli(NULL);

	branch_monitor_exit();

	rte_eal_mp_wait_lcore();

	free(ci->cd);

	return 0;
}
