/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

/*
 * Sample application demonstrating how to do packet I/O in a multi-process
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
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_memcpy.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

#define NB_MBUFS 64*1024 /* use 64k mbufs */
#define MBUF_CACHE_SIZE 256
#define PKT_BURST 32
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define PARAM_PROC_ID "proc-id"
#define PARAM_NUM_PROCS "num-procs"

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
} __rte_aligned(RTE_CACHE_LINE_SIZE / 2);

static int proc_id = -1;
static unsigned num_procs = 0;

static uint16_t ports[RTE_MAX_ETHPORTS];
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
	uint16_t i, port_mask = 0;
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
	RTE_ETH_FOREACH_DEV(i)
		if(port_mask & (1 << i))
			ports[num_ports++] = (uint8_t)i;

	ret = optind-1;
	optind = 1; /* reset getopt lib */

	return ret;
}

/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */
static inline int
smp_port_init(uint16_t port, struct rte_mempool *mbuf_pool,
	       uint16_t num_queues)
{
	struct rte_eth_conf port_conf = {
			.rxmode = {
				.mq_mode	= RTE_ETH_MQ_RX_RSS,
				.split_hdr_size = 0,
				.offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM,
			},
			.rx_adv_conf = {
				.rss_conf = {
					.rss_key = NULL,
					.rss_hf = RTE_ETH_RSS_IP,
				},
			},
			.txmode = {
				.mq_mode = RTE_ETH_MQ_TX_NONE,
			}
	};
	const uint16_t rx_rings = num_queues, tx_rings = num_queues;
	struct rte_eth_dev_info info;
	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_txconf txq_conf;
	int retval;
	uint16_t q;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	uint64_t rss_hf_tmp;

	if (rte_eal_process_type() == RTE_PROC_SECONDARY)
		return 0;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	printf("# Initialising port %u... ", port);
	fflush(stdout);

	retval = rte_eth_dev_info_get(port, &info);
	if (retval != 0) {
		printf("Error during getting device (port %u) info: %s\n",
				port, strerror(-retval));
		return retval;
	}

	info.default_rxconf.rx_drop_en = 1;

	if (info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	rss_hf_tmp = port_conf.rx_adv_conf.rss_conf.rss_hf;
	port_conf.rx_adv_conf.rss_conf.rss_hf &= info.flow_type_rss_offloads;
	if (port_conf.rx_adv_conf.rss_conf.rss_hf != rss_hf_tmp) {
		printf("Port %u modified RSS hash function based on hardware support,"
			"requested:%#"PRIx64" configured:%#"PRIx64"\n",
			port,
			rss_hf_tmp,
			port_conf.rx_adv_conf.rss_conf.rss_hf);
	}

	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval == -EINVAL) {
		printf("Port %u configuration failed. Re-attempting with HW checksum disabled.\n",
			port);
		port_conf.rxmode.offloads &= ~(RTE_ETH_RX_OFFLOAD_CHECKSUM);
		retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	}

	if (retval == -ENOTSUP) {
		printf("Port %u configuration failed. Re-attempting with HW RSS disabled.\n",
			port);
		port_conf.rxmode.mq_mode &= ~(RTE_ETH_MQ_RX_RSS);
		retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	}

	if (retval < 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval < 0)
		return retval;

	rxq_conf = info.default_rxconf;
	rxq_conf.offloads = port_conf.rxmode.offloads;
	for (q = 0; q < rx_rings; q ++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
				rte_eth_dev_socket_id(port),
				&rxq_conf,
				mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txq_conf = info.default_txconf;
	txq_conf.offloads = port_conf.txmode.offloads;
	for (q = 0; q < tx_rings; q ++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port),
				&txq_conf);
		if (retval < 0)
			return retval;
	}

	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
		return retval;

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

	const unsigned int lcores = rte_lcore_count();
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
check_all_ports_link_status(uint16_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;
	int ret;
	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
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
				rte_eth_link_to_str(link_status_text,
					sizeof(link_status_text), &link);
				printf("Port %d %s\n", portid,
				       link_status_text);
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == RTE_ETH_LINK_DOWN) {
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

	/* determine the NIC devices available */
	if (rte_eth_dev_count_avail() == 0)
		rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");

	/* parse application arguments (those after the EAL ones) */
	smp_parse_args(argc, argv);

	proc_type = rte_eal_process_type();
	mp = (proc_type == RTE_PROC_SECONDARY) ?
			rte_mempool_lookup(_SMP_MBUF_POOL) :
			rte_pktmbuf_pool_create(_SMP_MBUF_POOL, NB_MBUFS,
				MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
				rte_socket_id());
	if (mp == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get memory pool for buffers\n");

	/* Primary instance initialized. 8< */
	if (num_ports & 1)
		rte_exit(EXIT_FAILURE, "Application must use an even number of ports\n");
	for(i = 0; i < num_ports; i++){
		if(proc_type == RTE_PROC_PRIMARY)
			if (smp_port_init(ports[i], mp, (uint16_t)num_procs) < 0)
				rte_exit(EXIT_FAILURE, "Error initialising ports\n");
	}
	/* >8 End of primary instance initialization. */

	if (proc_type == RTE_PROC_PRIMARY)
		check_all_ports_link_status((uint8_t)num_ports, (~0x0));

	assign_ports_to_cores();

	RTE_LOG(INFO, APP, "Finished Process Init.\n");

	rte_eal_mp_remote_launch(lcore_main, NULL, CALL_MAIN);

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
