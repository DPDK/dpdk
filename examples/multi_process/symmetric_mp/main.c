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

/*
 * Sample application demostrating how to do packet I/O in a multi-process
 * environment. The same code can be run as a primary process and as a
 * secondary process, just with a different proc-id parameter in each case
 * (apart from the EAL flag to indicate a secondary process).
 *
 * Each process will read from the same ports, given by the port-mask
 * parameter, which should be the same in each case, just using a different
 * queue per port as determined by the proc-id parameter.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <getopt.h>
#include <signal.h>
#include <inttypes.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_memcpy.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define NB_MBUFS 64*1024 /* use 64k mbufs */
#define MBUF_CACHE_SIZE 256
#define PKT_BURST 32
#define RX_RING_SIZE 128
#define TX_RING_SIZE 512

#define PARAM_PROC_ID "proc-id"
#define PARAM_NUM_PROCS "num-procs"

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
/* Default configuration for rx and tx thresholds etc. */
static const struct rte_eth_rxconf rx_conf_default = {
	.rx_thresh = {
		.pthresh = 8,
		.hthresh = 8,
		.wthresh = 4,
	},
};

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
static const struct rte_eth_txconf tx_conf_default = {
	.tx_thresh = {
		.pthresh = 36,
		.hthresh = 0,
		.wthresh = 0,
	},
	.tx_free_thresh = 0, /* Use PMD default values */
	.tx_rs_thresh = 0, /* Use PMD default values */
};

/* for each lcore, record the elements of the ports array to use */
struct lcore_ports{
	unsigned start_port;
	unsigned num_ports;
};

/* structure to record the rx and tx packets. Put two per cache line as ports
 * used in pairs */
struct port_stats{
	unsigned rx;
	unsigned tx;
	unsigned drop;
} __attribute__((aligned(CACHE_LINE_SIZE / 2)));

static int proc_id = -1;
static unsigned num_procs = 0;

static uint8_t ports[RTE_MAX_ETHPORTS];
static unsigned num_ports = 0;

static struct lcore_ports lcore_ports[RTE_MAX_LCORE];
static struct port_stats pstats[RTE_MAX_ETHPORTS];

/* prints the usage statement and quits with an error message */
static void
smp_usage(const char *prgname, const char *errmsg)
{
	printf("\nError: %s\n",errmsg);
	printf("\n%s [EAL options] -- -p <port mask> "
			"--"PARAM_NUM_PROCS" <n>"
			" --"PARAM_PROC_ID" <id>\n"
			"-p         : a hex bitmask indicating what ports are to be used\n"
			"--num-procs: the number of processes which will be used\n"
			"--proc-id  : the id of the current process (id < num-procs)\n"
			"\n",
			prgname);
	exit(1);
}


/* signal handler configured for SIGTERM and SIGINT to print stats on exit */
static void
print_stats(int signum)
{
	unsigned i;
	printf("\nExiting on signal %d\n\n", signum);
	for (i = 0; i < num_ports; i++){
		const uint8_t p_num = ports[i];
		printf("Port %u: RX - %u, TX - %u, Drop - %u\n", (unsigned)p_num,
				pstats[p_num].rx, pstats[p_num].tx, pstats[p_num].drop);
	}
	exit(0);
}

/* Parse the argument given in the command line of the application */
static int
smp_parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	unsigned i, port_mask = 0;
	char *prgname = argv[0];
	static struct option lgopts[] = {
			{PARAM_NUM_PROCS, 1, 0, 0},
			{PARAM_PROC_ID, 1, 0, 0},
			{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:", \
			lgopts, &option_index)) != EOF) {

		switch (opt) {
		case 'p':
			port_mask = strtoull(optarg, NULL, 16);
			break;
			/* long options */
		case 0:
			if (strncmp(lgopts[option_index].name, PARAM_NUM_PROCS, 8) == 0)
				num_procs = atoi(optarg);
			else if (strncmp(lgopts[option_index].name, PARAM_PROC_ID, 7) == 0)
				proc_id = atoi(optarg);
			break;

		default:
			smp_usage(prgname, "Cannot parse all command-line arguments\n");
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	if (proc_id < 0)
		smp_usage(prgname, "Invalid or missing proc-id parameter\n");
	if (rte_eal_process_type() == RTE_PROC_PRIMARY && num_procs == 0)
		smp_usage(prgname, "Invalid or missing num-procs parameter\n");
	if (port_mask == 0)
		smp_usage(prgname, "Invalid or missing port mask\n");

	/* get the port numbers from the port mask */
	for(i = 0; i < rte_eth_dev_count(); i++)
		if(port_mask & (1 << i))
			ports[num_ports++] = (uint8_t)i;

	ret = optind-1;
	optind = 0; /* reset getopt lib */

	return (ret);
}

/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */
static inline int
smp_port_init(uint8_t port, struct rte_mempool *mbuf_pool, uint16_t num_queues)
{
	struct rte_eth_conf port_conf = {
			.rxmode = {
				.mq_mode	= ETH_MQ_RX_RSS,
				.split_hdr_size = 0,
				.header_split   = 0, /**< Header Split disabled */
				.hw_ip_checksum = 1, /**< IP checksum offload enabled */
				.hw_vlan_filter = 0, /**< VLAN filtering disabled */
				.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
				.hw_strip_crc   = 0, /**< CRC stripped by hardware */
			},
			.rx_adv_conf = {
				.rss_conf = {
					.rss_key = NULL,
					.rss_hf = ETH_RSS_IP,
				},
			},
			.txmode = {
				.mq_mode = ETH_MQ_TX_NONE,
			}
	};
	const uint16_t rx_rings = num_queues, tx_rings = num_queues;
	int retval;
	uint16_t q;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return 0;

	if (port >= rte_eth_dev_count())
		return -1;

	printf("# Initialising port %u... ", (unsigned)port);
	fflush(stdout);

	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval < 0)
		return retval;

	for (q = 0; q < rx_rings; q ++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE,
				rte_eth_dev_socket_id(port), &rx_conf_default,
				mbuf_pool);
		if (retval < 0)
			return retval;
	}

	for (q = 0; q < tx_rings; q ++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE,
				rte_eth_dev_socket_id(port), &tx_conf_default);
		if (retval < 0)
			return retval;
	}

	rte_eth_promiscuous_enable(port);

	retval  = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	return 0;
}

/* Goes through each of the lcores and calculates what ports should
 * be used by that core. Fills in the global lcore_ports[] array.
 */
static void
assign_ports_to_cores(void)
{

	const unsigned lcores = rte_eal_get_configuration()->lcore_count;
	const unsigned port_pairs = num_ports / 2;
	const unsigned pairs_per_lcore = port_pairs / lcores;
	unsigned extra_pairs = port_pairs % lcores;
	unsigned ports_assigned = 0;
	unsigned i;

	RTE_LCORE_FOREACH(i) {
		lcore_ports[i].start_port = ports_assigned;
		lcore_ports[i].num_ports = pairs_per_lcore * 2;
		if (extra_pairs > 0) {
			lcore_ports[i].num_ports += 2;
			extra_pairs--;
		}
		ports_assigned += lcore_ports[i].num_ports;
	}
}

/* Main function used by the processing threads.
 * Prints out some configuration details for the thread and then begins
 * performing packet RX and TX.
 */
static int
lcore_main(void *arg __rte_unused)
{
	const unsigned id = rte_lcore_id();
	const unsigned start_port = lcore_ports[id].start_port;
	const unsigned end_port = start_port + lcore_ports[id].num_ports;
	const uint16_t q_id = (uint16_t)proc_id;
	unsigned p, i;
	char msgbuf[256];
	int msgbufpos = 0;

	if (start_port == end_port){
		printf("Lcore %u has nothing to do\n", id);
		return 0;
	}

	/* build up message in msgbuf before printing to decrease likelihood
	 * of multi-core message interleaving.
	 */
	msgbufpos += snprintf(msgbuf, sizeof(msgbuf) - msgbufpos,
			"Lcore %u using ports ", id);
	for (p = start_port; p < end_port; p++){
		msgbufpos += snprintf(msgbuf + msgbufpos, sizeof(msgbuf) - msgbufpos,
				"%u ", (unsigned)ports[p]);
	}
	printf("%s\n", msgbuf);
	printf("lcore %u using queue %u of each port\n", id, (unsigned)q_id);

	/* handle packet I/O from the ports, reading and writing to the
	 * queue number corresponding to our process number (not lcore id)
	 */

	for (;;) {
		struct rte_mbuf *buf[PKT_BURST];

		for (p = start_port; p < end_port; p++) {
			const uint8_t src = ports[p];
			const uint8_t dst = ports[p ^ 1]; /* 0 <-> 1, 2 <-> 3 etc */
			const uint16_t rx_c = rte_eth_rx_burst(src, q_id, buf, PKT_BURST);
			if (rx_c == 0)
				continue;
			pstats[src].rx += rx_c;

			const uint16_t tx_c = rte_eth_tx_burst(dst, q_id, buf, rx_c);
			pstats[dst].tx += tx_c;
			if (tx_c != rx_c) {
				pstats[dst].drop += (rx_c - tx_c);
				for (i = tx_c; i < rx_c; i++)
					rte_pktmbuf_free(buf[i]);
			}
		}
	}
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf("Port %d Link Up - speed %u "
						"Mbps - %s\n", (uint8_t)portid,
						(unsigned)link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					("full-duplex") : ("half-duplex\n"));
				else
					printf("Port %d Link Down\n",
							(uint8_t)portid);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == 0) {
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

/* Main function.
 * Performs initialisation and then calls the lcore_main on each core
 * to do the packet-processing work.
 */
int
main(int argc, char **argv)
{
	static const char *_SMP_MBUF_POOL = "SMP_MBUF_POOL";
	int ret;
	unsigned i;
	enum rte_proc_type_t proc_type;
	struct rte_mempool *mp;

	/* set up signal handlers to print stats on exit */
	signal(SIGINT, print_stats);
	signal(SIGTERM, print_stats);

	/* initialise the EAL for all */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Cannot init EAL\n");
	argc -= ret;
	argv += ret;

	/* probe to determine the NIC devices available */
	proc_type = rte_eal_process_type();
	if (rte_eal_pci_probe() < 0)
		rte_exit(EXIT_FAILURE, "Cannot probe PCI\n");
	if (rte_eth_dev_count() == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	/* parse application arguments (those after the EAL ones) */
	smp_parse_args(argc, argv);

	mp = (proc_type == RTE_PROC_SECONDARY) ?
			rte_mempool_lookup(_SMP_MBUF_POOL) :
			rte_mempool_create(_SMP_MBUF_POOL, NB_MBUFS, MBUF_SIZE,
					MBUF_CACHE_SIZE, sizeof(struct rte_pktmbuf_pool_private),
					rte_pktmbuf_pool_init, NULL,
					rte_pktmbuf_init, NULL,
					rte_socket_id(), 0);
	if (mp == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get memory pool for buffers\n");

	if (num_ports & 1)
		rte_exit(EXIT_FAILURE, "Application must use an even number of ports\n");
	for(i = 0; i < num_ports; i++){
		if(proc_type == RTE_PROC_PRIMARY)
			if (smp_port_init(ports[i], mp, (uint16_t)num_procs) < 0)
				rte_exit(EXIT_FAILURE, "Error initialising ports\n");
	}

	if (proc_type == RTE_PROC_PRIMARY)
		check_all_ports_link_status((uint8_t)num_ports, (~0x0));

	assign_ports_to_cores();

	RTE_LOG(INFO, APP, "Finished Process Init.\n");

	rte_eal_mp_remote_launch(lcore_main, NULL, CALL_MASTER);

	return 0;
}
