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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/param.h>
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
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_lpm.h>
#include <rte_ip.h>

#include "rte_ipv4_frag.h"
#include "main.h"

#define RTE_LOGTYPE_L3FWD RTE_LOGTYPE_USER1

#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

/* allow max jumbo frame 9.5 KB */
#define JUMBO_FRAME_MAX_SIZE	0x2600

#define	ROUNDUP_DIV(a, b)	(((a) + (b) - 1) / (b))

/*
 * Max number of fragments per packet expected.
 */
#define	MAX_PACKET_FRAG	ROUNDUP_DIV(JUMBO_FRAME_MAX_SIZE, IPV4_DEFAULT_PAYLOAD)

#define NB_MBUF   8192

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

#define MAX_PKT_BURST	32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	3

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];
static struct ether_addr remote_eth_addr =
	{{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}};

/* mask of enabled ports */
static int enabled_port_mask = 0;

static int rx_queue_per_lcore = 1;

#define MBUF_TABLE_SIZE  (2 * MAX(MAX_PKT_BURST, MAX_PACKET_FRAG))

struct mbuf_table {
	uint16_t len;
	struct rte_mbuf *m_table[MBUF_TABLE_SIZE];
};

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	uint16_t n_rx_queue;
	uint8_t rx_queue_list[MAX_RX_QUEUE_PER_LCORE];
	uint16_t tx_queue_id[RTE_MAX_ETHPORTS];
	struct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];

} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.max_rx_pkt_len = JUMBO_FRAME_MAX_SIZE,
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 1, /**< Jumbo Frame Support enabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

static const struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {
		.pthresh = RX_PTHRESH,
		.hthresh = RX_HTHRESH,
		.wthresh = RX_WTHRESH,
	},
};

static const struct rte_eth_txconf tx_conf = {
	.tx_thresh = {
		.pthresh = TX_PTHRESH,
		.hthresh = TX_HTHRESH,
		.wthresh = TX_WTHRESH,
	},
	.tx_free_thresh = 0, /* Use PMD default values */
	.tx_rs_thresh = 0, /* Use PMD default values */
};

struct rte_mempool *pool_direct = NULL, *pool_indirect = NULL;

struct l3fwd_route {
	uint32_t ip;
	uint8_t  depth;
	uint8_t  if_out;
};

struct l3fwd_route l3fwd_route_array[] = {
	{IPv4(100,10,0,0), 16, 2},
	{IPv4(100,20,0,0), 16, 2},
	{IPv4(100,30,0,0), 16, 0},
	{IPv4(100,40,0,0), 16, 0},
};

#define L3FWD_NUM_ROUTES \
	(sizeof(l3fwd_route_array) / sizeof(l3fwd_route_array[0]))

#define L3FWD_LPM_MAX_RULES     1024

struct rte_lpm *l3fwd_lpm = NULL;

/* Send burst of packets on an output interface */
static inline int
send_burst(struct lcore_queue_conf *qconf, uint16_t n, uint8_t port)
{
	struct rte_mbuf **m_table;
	int ret;
	uint16_t queueid;

	queueid = qconf->tx_queue_id[port];
	m_table = (struct rte_mbuf **)qconf->tx_mbufs[port].m_table;

	ret = rte_eth_tx_burst(port, queueid, m_table, n);
	if (unlikely(ret < n)) {
		do {
			rte_pktmbuf_free(m_table[ret]);
		} while (++ret < n);
	}

	return 0;
}

static inline void
l3fwd_simple_forward(struct rte_mbuf *m, uint8_t port_in)
{
	struct lcore_queue_conf *qconf;
	struct ipv4_hdr *ip_hdr;
	uint32_t i, len, lcore_id, ip_dst;
	uint8_t next_hop, port_out;
	int32_t len2;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	/* Remove the Ethernet header and trailer from the input packet */
	rte_pktmbuf_adj(m, (uint16_t)sizeof(struct ether_hdr));

	/* Read the lookup key (i.e. ip_dst) from the input packet */
	ip_hdr = rte_pktmbuf_mtod(m, struct ipv4_hdr *);
	ip_dst = rte_be_to_cpu_32(ip_hdr->dst_addr);

	/* Find destination port */
	if (rte_lpm_lookup(l3fwd_lpm, ip_dst, &next_hop) == 0 &&
			(enabled_port_mask & 1 << next_hop) != 0)
		port_out = next_hop;
	else
		port_out = port_in;

	/* Build transmission burst */
	len = qconf->tx_mbufs[port_out].len;

	/* if we don't need to do any fragmentation */
	if (likely (IPV4_MTU_DEFAULT  >= m->pkt.pkt_len)) {
		qconf->tx_mbufs[port_out].m_table[len] = m;
		len2 = 1;
	} else {
		len2 = rte_ipv4_fragmentation(m,
			&qconf->tx_mbufs[port_out].m_table[len],
			(uint16_t)(MBUF_TABLE_SIZE - len),
			IPV4_MTU_DEFAULT,
			pool_direct, pool_indirect);

		/* Free input packet */
		rte_pktmbuf_free(m);

		/* If we fail to fragment the packet */
		if (unlikely (len2 < 0))
			return;
	}

	for (i = len; i < len + len2; i ++) {
		m = qconf->tx_mbufs[port_out].m_table[i];
		struct ether_hdr *eth_hdr = (struct ether_hdr *)
			rte_pktmbuf_prepend(m, (uint16_t)sizeof(struct ether_hdr));
		if (eth_hdr == NULL) {
			rte_panic("No headroom in mbuf.\n");
		}

		m->pkt.vlan_macip.f.l2_len = sizeof(struct ether_hdr);

		ether_addr_copy(&remote_eth_addr, &eth_hdr->d_addr);
		ether_addr_copy(&ports_eth_addr[port_out], &eth_hdr->s_addr);
		eth_hdr->ether_type = rte_be_to_cpu_16(ETHER_TYPE_IPv4);
	}

	len += len2;

	if (likely(len < MAX_PKT_BURST)) {
		qconf->tx_mbufs[port_out].len = (uint16_t)len;
		return;
	}

	/* Transmit packets */
	send_burst(qconf, (uint16_t)len, port_out);
	qconf->tx_mbufs[port_out].len = 0;
}

/* main processing loop */
static int
main_loop(__attribute__((unused)) void *dummy)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	unsigned lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	int i, j, nb_rx;
	uint8_t portid;
	struct lcore_queue_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;

	prev_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	if (qconf->n_rx_queue == 0) {
		RTE_LOG(INFO, L3FWD, "lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	RTE_LOG(INFO, L3FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_queue; i++) {

		portid = qconf->rx_queue_list[i];
		RTE_LOG(INFO, L3FWD, " -- lcoreid=%u portid=%d\n", lcore_id,
			(int) portid);
	}

	while (1) {

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			/*
			 * This could be optimized (use queueid instead of
			 * portid), but it is not called so often
			 */
			for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
				if (qconf->tx_mbufs[portid].len == 0)
					continue;
				send_burst(&lcore_queue_conf[lcore_id],
					   qconf->tx_mbufs[portid].len,
					   portid);
				qconf->tx_mbufs[portid].len = 0;
			}

			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_queue; i++) {

			portid = qconf->rx_queue_list[i];
			nb_rx = rte_eth_rx_burst(portid, 0, pkts_burst,
						 MAX_PKT_BURST);

			/* Prefetch first packets */
			for (j = 0; j < PREFETCH_OFFSET && j < nb_rx; j++) {
				rte_prefetch0(rte_pktmbuf_mtod(
						pkts_burst[j], void *));
			}

			/* Prefetch and forward already prefetched packets */
			for (j = 0; j < (nb_rx - PREFETCH_OFFSET); j++) {
				rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[
						j + PREFETCH_OFFSET], void *));
				l3fwd_simple_forward(pkts_burst[j], portid);
			}

			/* Forward remaining prefetched packets */
			for (; j < nb_rx; j++) {
				l3fwd_simple_forward(pkts_burst[j], portid);
			}
		}
	}
}

/* display usage */
static void
print_usage(const char *prgname)
{
	printf("%s [EAL options] -- -p PORTMASK [-q NQ]\n"
	       "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	       "  -q NQ: number of queue (=ports) per lcore (default is 1)\n",
	       prgname);
}

static int
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;

	if (pm == 0)
		return -1;

	return pm;
}

static int
parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse hexadecimal string */
	n = strtoul(q_arg, &end, 10);
	if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
		return -1;
	if (n == 0)
		return -1;
	if (n >= MAX_RX_QUEUE_PER_LCORE)
		return -1;

	return n;
}

/* Parse the argument given in the command line of the application */
static int
parse_args(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index;
	char *prgname = argv[0];
	static struct option lgopts[] = {
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:q:",
				  lgopts, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case 'p':
			enabled_port_mask = parse_portmask(optarg);
			if (enabled_port_mask < 0) {
				printf("invalid portmask\n");
				print_usage(prgname);
				return -1;
			}
			break;

		/* nqueue */
		case 'q':
			rx_queue_per_lcore = parse_nqueue(optarg);
			if (rx_queue_per_lcore < 0) {
				printf("invalid queue number\n");
				print_usage(prgname);
				return -1;
			}
			break;

		/* long options */
		case 0:
			print_usage(prgname);
			return -1;

		default:
			print_usage(prgname);
			return -1;
		}
	}

	if (enabled_port_mask == 0) {
		printf("portmask not specified\n");
		print_usage(prgname);
		return -1;
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 0; /* reset getopt lib */
	return ret;
}

static void
print_ethaddr(const char *name, struct ether_addr *eth_addr)
{
	printf("%s%02X:%02X:%02X:%02X:%02X:%02X", name,
	       eth_addr->addr_bytes[0],
	       eth_addr->addr_bytes[1],
	       eth_addr->addr_bytes[2],
	       eth_addr->addr_bytes[3],
	       eth_addr->addr_bytes[4],
	       eth_addr->addr_bytes[5]);
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

int
MAIN(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	int ret;
	unsigned nb_ports, i;
	uint16_t queueid = 0;
	unsigned lcore_id = 0, rx_lcore_id = 0;
	uint32_t n_tx_queue, nb_lcores;
	uint8_t portid;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "rte_eal_init failed");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid arguments");

	/* create the mbuf pools */
	pool_direct =
		rte_mempool_create("pool_direct", NB_MBUF,
				   MBUF_SIZE, 32,
				   sizeof(struct rte_pktmbuf_pool_private),
				   rte_pktmbuf_pool_init, NULL,
				   rte_pktmbuf_init, NULL,
				   rte_socket_id(), 0);
	if (pool_direct == NULL)
		rte_panic("Cannot init direct mbuf pool\n");

	pool_indirect =
		rte_mempool_create("pool_indirect", NB_MBUF,
				   sizeof(struct rte_mbuf), 32,
				   0,
				   NULL, NULL,
				   rte_pktmbuf_init, NULL,
				   rte_socket_id(), 0);
	if (pool_indirect == NULL)
		rte_panic("Cannot init indirect mbuf pool\n");

	/* init driver */
	if (rte_pmd_init_all() < 0)
		rte_panic("Cannot init PMD\n");

	if (rte_eal_pci_probe() < 0)
		rte_panic("Cannot probe PCI\n");

	nb_ports = rte_eth_dev_count();
	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

	nb_lcores = rte_lcore_count();

	/* initialize all ports */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %d\n", portid);
			continue;
		}

		qconf = &lcore_queue_conf[rx_lcore_id];

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       qconf->n_rx_queue == (unsigned)rx_queue_per_lcore) {

			rx_lcore_id ++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");

			qconf = &lcore_queue_conf[rx_lcore_id];
		}
		qconf->rx_queue_list[qconf->n_rx_queue] = portid;
		qconf->n_rx_queue++;

		/* init port */
		printf("Initializing port %d on lcore %u... ", portid,
		       rx_lcore_id);
		fflush(stdout);

		n_tx_queue = nb_lcores;
		if (n_tx_queue > MAX_TX_QUEUE_PER_PORT)
			n_tx_queue = MAX_TX_QUEUE_PER_PORT;
		ret = rte_eth_dev_configure(portid, 1, (uint16_t)n_tx_queue,
					    &port_conf);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "Cannot configure device: "
				"err=%d, port=%d\n",
				ret, portid);

		rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
		print_ethaddr(" Address:", &ports_eth_addr[portid]);
		printf(", ");

		/* init one RX queue */
		queueid = 0;
		printf("rxq=%d ", queueid);
		fflush(stdout);
		ret = rte_eth_rx_queue_setup(portid, queueid, nb_rxd,
					     rte_eth_dev_socket_id(portid), &rx_conf,
					     pool_direct);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: "
				"err=%d, port=%d\n",
				ret, portid);

		/* init one TX queue per couple (lcore,port) */
		queueid = 0;
		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;
			printf("txq=%u,%d ", lcore_id, queueid);
			fflush(stdout);
			ret = rte_eth_tx_queue_setup(portid, queueid, nb_txd,
						     rte_eth_dev_socket_id(portid), &tx_conf);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: "
					"err=%d, port=%d\n", ret, portid);

			qconf = &lcore_queue_conf[lcore_id];
			qconf->tx_queue_id[portid] = queueid;
			queueid++;
		}

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start: "
				"err=%d, port=%d\n",
				ret, portid);

		printf("done: ");

		/* Set port in promiscuous mode */
		rte_eth_promiscuous_enable(portid);
	}

	check_all_ports_link_status((uint8_t)nb_ports, enabled_port_mask);

	/* create the LPM table */
	l3fwd_lpm = rte_lpm_create("L3FWD_LPM", rte_socket_id(), L3FWD_LPM_MAX_RULES, 0);
	if (l3fwd_lpm == NULL)
		rte_panic("Unable to create the l3fwd LPM table\n");

	/* populate the LPM table */
	for (i = 0; i < L3FWD_NUM_ROUTES; i++) {
		ret = rte_lpm_add(l3fwd_lpm,
			l3fwd_route_array[i].ip,
			l3fwd_route_array[i].depth,
			l3fwd_route_array[i].if_out);

		if (ret < 0) {
			rte_panic("Unable to add entry %u to the l3fwd "
				"LPM table\n", i);
		}

		printf("Adding route 0x%08x / %d (%d)\n",
			(unsigned) l3fwd_route_array[i].ip,
			l3fwd_route_array[i].depth,
			l3fwd_route_array[i].if_out);
	}

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}
