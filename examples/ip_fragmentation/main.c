/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
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
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>
#include <rte_ip.h>
#include <rte_string_fns.h>

#include <rte_ip_frag.h>

#define RTE_LOGTYPE_IP_FRAG RTE_LOGTYPE_USER1

/* allow max jumbo frame 9.5 KB */
#define JUMBO_FRAME_MAX_SIZE	0x2600

#define	ROUNDUP_DIV(a, b)	(((a) + (b) - 1) / (b))

/*
 * Default byte size for the IPv6 Maximum Transfer Unit (MTU).
 * This value includes the size of IPv6 header.
 */
#define	IPV4_MTU_DEFAULT	ETHER_MTU
#define	IPV6_MTU_DEFAULT	ETHER_MTU

/*
 * The overhead from max frame size to MTU.
 * We have to consider the max possible overhead.
 */
#define MTU_OVERHEAD	\
	(ETHER_HDR_LEN + ETHER_CRC_LEN + 2 * sizeof(struct vlan_hdr))

/*
 * Default payload in bytes for the IPv6 packet.
 */
#define	IPV4_DEFAULT_PAYLOAD	(IPV4_MTU_DEFAULT - sizeof(struct ipv4_hdr))
#define	IPV6_DEFAULT_PAYLOAD	(IPV6_MTU_DEFAULT - sizeof(struct ipv6_hdr))

/*
 * Max number of fragments per packet expected - defined by config file.
 */
#define	MAX_PACKET_FRAG RTE_LIBRTE_IP_FRAG_MAX_FRAG

#define NB_MBUF   8192

#define MAX_PKT_BURST	32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	3

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 1024
#define RTE_TEST_TX_DESC_DEFAULT 1024
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];

#ifndef IPv4_BYTES
#define IPv4_BYTES_FMT "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8
#define IPv4_BYTES(addr) \
		(uint8_t) (((addr) >> 24) & 0xFF),\
		(uint8_t) (((addr) >> 16) & 0xFF),\
		(uint8_t) (((addr) >> 8) & 0xFF),\
		(uint8_t) ((addr) & 0xFF)
#endif

#ifndef IPv6_BYTES
#define IPv6_BYTES_FMT "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"\
                       "%02x%02x:%02x%02x:%02x%02x:%02x%02x"
#define IPv6_BYTES(addr) \
	addr[0],  addr[1], addr[2],  addr[3], \
	addr[4],  addr[5], addr[6],  addr[7], \
	addr[8],  addr[9], addr[10], addr[11],\
	addr[12], addr[13],addr[14], addr[15]
#endif

#define IPV6_ADDR_LEN 16

/* mask of enabled ports */
static int enabled_port_mask = 0;

static int rx_queue_per_lcore = 1;

#define MBUF_TABLE_SIZE  (2 * MAX(MAX_PKT_BURST, MAX_PACKET_FRAG))

struct mbuf_table {
	uint16_t len;
	struct rte_mbuf *m_table[MBUF_TABLE_SIZE];
};

struct rx_queue {
	struct rte_mempool *direct_pool;
	struct rte_mempool *indirect_pool;
	struct rte_lpm *lpm;
	struct rte_lpm6 *lpm6;
	uint16_t portid;
};

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	uint16_t n_rx_queue;
	uint16_t tx_queue_id[RTE_MAX_ETHPORTS];
	struct rx_queue rx_queue_list[MAX_RX_QUEUE_PER_LCORE];
	struct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;
struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.max_rx_pkt_len = JUMBO_FRAME_MAX_SIZE,
		.split_hdr_size = 0,
		.offloads = (DEV_RX_OFFLOAD_CHECKSUM |
			     DEV_RX_OFFLOAD_SCATTER |
			     DEV_RX_OFFLOAD_JUMBO_FRAME),
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
		.offloads = (DEV_TX_OFFLOAD_IPV4_CKSUM |
			     DEV_TX_OFFLOAD_MULTI_SEGS),
	},
};

/*
 * IPv4 forwarding table
 */
struct l3fwd_ipv4_route {
	uint32_t ip;
	uint8_t  depth;
	uint8_t  if_out;
};

struct l3fwd_ipv4_route l3fwd_ipv4_route_array[] = {
		{IPv4(100,10,0,0), 16, 0},
		{IPv4(100,20,0,0), 16, 1},
		{IPv4(100,30,0,0), 16, 2},
		{IPv4(100,40,0,0), 16, 3},
		{IPv4(100,50,0,0), 16, 4},
		{IPv4(100,60,0,0), 16, 5},
		{IPv4(100,70,0,0), 16, 6},
		{IPv4(100,80,0,0), 16, 7},
};

/*
 * IPv6 forwarding table
 */

struct l3fwd_ipv6_route {
	uint8_t ip[IPV6_ADDR_LEN];
	uint8_t depth;
	uint8_t if_out;
};

static struct l3fwd_ipv6_route l3fwd_ipv6_route_array[] = {
	{{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, 48, 0},
	{{2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, 48, 1},
	{{3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, 48, 2},
	{{4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, 48, 3},
	{{5,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, 48, 4},
	{{6,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, 48, 5},
	{{7,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, 48, 6},
	{{8,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, 48, 7},
};

#define LPM_MAX_RULES         1024
#define LPM6_MAX_RULES         1024
#define LPM6_NUMBER_TBL8S (1 << 16)

struct rte_lpm6_config lpm6_config = {
		.max_rules = LPM6_MAX_RULES,
		.number_tbl8s = LPM6_NUMBER_TBL8S,
		.flags = 0
};

static struct rte_mempool *socket_direct_pool[RTE_MAX_NUMA_NODES];
static struct rte_mempool *socket_indirect_pool[RTE_MAX_NUMA_NODES];
static struct rte_lpm *socket_lpm[RTE_MAX_NUMA_NODES];
static struct rte_lpm6 *socket_lpm6[RTE_MAX_NUMA_NODES];

/* Send burst of packets on an output interface */
static inline int
send_burst(struct lcore_queue_conf *qconf, uint16_t n, uint16_t port)
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
l3fwd_simple_forward(struct rte_mbuf *m, struct lcore_queue_conf *qconf,
		uint8_t queueid, uint16_t port_in)
{
	struct rx_queue *rxq;
	uint32_t i, len, next_hop;
	uint8_t ipv6;
	uint16_t port_out;
	int32_t len2;

	ipv6 = 0;
	rxq = &qconf->rx_queue_list[queueid];

	/* by default, send everything back to the source port */
	port_out = port_in;

	/* Remove the Ethernet header and trailer from the input packet */
	rte_pktmbuf_adj(m, (uint16_t)sizeof(struct ether_hdr));

	/* Build transmission burst */
	len = qconf->tx_mbufs[port_out].len;

	/* if this is an IPv4 packet */
	if (RTE_ETH_IS_IPV4_HDR(m->packet_type)) {
		struct ipv4_hdr *ip_hdr;
		uint32_t ip_dst;
		/* Read the lookup key (i.e. ip_dst) from the input packet */
		ip_hdr = rte_pktmbuf_mtod(m, struct ipv4_hdr *);
		ip_dst = rte_be_to_cpu_32(ip_hdr->dst_addr);

		/* Find destination port */
		if (rte_lpm_lookup(rxq->lpm, ip_dst, &next_hop) == 0 &&
				(enabled_port_mask & 1 << next_hop) != 0) {
			port_out = next_hop;

			/* Build transmission burst for new port */
			len = qconf->tx_mbufs[port_out].len;
		}

		/* if we don't need to do any fragmentation */
		if (likely (IPV4_MTU_DEFAULT >= m->pkt_len)) {
			qconf->tx_mbufs[port_out].m_table[len] = m;
			len2 = 1;
		} else {
			len2 = rte_ipv4_fragment_packet(m,
				&qconf->tx_mbufs[port_out].m_table[len],
				(uint16_t)(MBUF_TABLE_SIZE - len),
				IPV4_MTU_DEFAULT,
				rxq->direct_pool, rxq->indirect_pool);

			/* Free input packet */
			rte_pktmbuf_free(m);

			/* If we fail to fragment the packet */
			if (unlikely (len2 < 0))
				return;
		}
	} else if (RTE_ETH_IS_IPV6_HDR(m->packet_type)) {
		/* if this is an IPv6 packet */
		struct ipv6_hdr *ip_hdr;

		ipv6 = 1;

		/* Read the lookup key (i.e. ip_dst) from the input packet */
		ip_hdr = rte_pktmbuf_mtod(m, struct ipv6_hdr *);

		/* Find destination port */
		if (rte_lpm6_lookup(rxq->lpm6, ip_hdr->dst_addr,
						&next_hop) == 0 &&
				(enabled_port_mask & 1 << next_hop) != 0) {
			port_out = next_hop;

			/* Build transmission burst for new port */
			len = qconf->tx_mbufs[port_out].len;
		}

		/* if we don't need to do any fragmentation */
		if (likely (IPV6_MTU_DEFAULT >= m->pkt_len)) {
			qconf->tx_mbufs[port_out].m_table[len] = m;
			len2 = 1;
		} else {
			len2 = rte_ipv6_fragment_packet(m,
				&qconf->tx_mbufs[port_out].m_table[len],
				(uint16_t)(MBUF_TABLE_SIZE - len),
				IPV6_MTU_DEFAULT,
				rxq->direct_pool, rxq->indirect_pool);

			/* Free input packet */
			rte_pktmbuf_free(m);

			/* If we fail to fragment the packet */
			if (unlikely (len2 < 0))
				return;
		}
	}
	/* else, just forward the packet */
	else {
		qconf->tx_mbufs[port_out].m_table[len] = m;
		len2 = 1;
	}

	for (i = len; i < len + len2; i ++) {
		void *d_addr_bytes;

		m = qconf->tx_mbufs[port_out].m_table[i];
		struct ether_hdr *eth_hdr = (struct ether_hdr *)
			rte_pktmbuf_prepend(m, (uint16_t)sizeof(struct ether_hdr));
		if (eth_hdr == NULL) {
			rte_panic("No headroom in mbuf.\n");
		}

		m->l2_len = sizeof(struct ether_hdr);

		/* 02:00:00:00:00:xx */
		d_addr_bytes = &eth_hdr->d_addr.addr_bytes[0];
		*((uint64_t *)d_addr_bytes) = 0x000000000002 + ((uint64_t)port_out << 40);

		/* src addr */
		ether_addr_copy(&ports_eth_addr[port_out], &eth_hdr->s_addr);
		if (ipv6)
			eth_hdr->ether_type = rte_be_to_cpu_16(ETHER_TYPE_IPv6);
		else
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
	uint16_t portid;
	struct lcore_queue_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;

	prev_tsc = 0;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];

	if (qconf->n_rx_queue == 0) {
		RTE_LOG(INFO, IP_FRAG, "lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	RTE_LOG(INFO, IP_FRAG, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_queue; i++) {

		portid = qconf->rx_queue_list[i].portid;
		RTE_LOG(INFO, IP_FRAG, " -- lcoreid=%u portid=%d\n", lcore_id,
				portid);
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

			portid = qconf->rx_queue_list[i].portid;
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
				l3fwd_simple_forward(pkts_burst[j], qconf, i, portid);
			}

			/* Forward remaining prefetched packets */
			for (; j < nb_rx; j++) {
				l3fwd_simple_forward(pkts_burst[j], qconf, i, portid);
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
	optind = 1; /* reset getopt lib */
	return ret;
}

static void
print_ethaddr(const char *name, struct ether_addr *eth_addr)
{
	char buf[ETHER_ADDR_FMT_SIZE];
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void
check_all_ports_link_status(uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint16_t portid;
	uint8_t count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	printf("\nChecking link status");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		RTE_ETH_FOREACH_DEV(portid) {
			if ((port_mask & (1 << portid)) == 0)
				continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status)
					printf(
					"Port%d Link Up .Speed %u Mbps - %s\n",
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
			printf("\ndone\n");
		}
	}
}

/* Check L3 packet type detection capablity of the NIC port */
static int
check_ptype(int portid)
{
	int i, ret;
	int ptype_l3_ipv4 = 0, ptype_l3_ipv6 = 0;
	uint32_t ptype_mask = RTE_PTYPE_L3_MASK;

	ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, NULL, 0);
	if (ret <= 0)
		return 0;

	uint32_t ptypes[ret];

	ret = rte_eth_dev_get_supported_ptypes(portid, ptype_mask, ptypes, ret);
	for (i = 0; i < ret; ++i) {
		if (ptypes[i] & RTE_PTYPE_L3_IPV4)
			ptype_l3_ipv4 = 1;
		if (ptypes[i] & RTE_PTYPE_L3_IPV6)
			ptype_l3_ipv6 = 1;
	}

	if (ptype_l3_ipv4 == 0)
		printf("port %d cannot parse RTE_PTYPE_L3_IPV4\n", portid);

	if (ptype_l3_ipv6 == 0)
		printf("port %d cannot parse RTE_PTYPE_L3_IPV6\n", portid);

	if (ptype_l3_ipv4 && ptype_l3_ipv6)
		return 1;

	return 0;

}

/* Parse packet type of a packet by SW */
static inline void
parse_ptype(struct rte_mbuf *m)
{
	struct ether_hdr *eth_hdr;
	uint32_t packet_type = RTE_PTYPE_UNKNOWN;
	uint16_t ether_type;

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
	ether_type = eth_hdr->ether_type;
	if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv4))
		packet_type |= RTE_PTYPE_L3_IPV4_EXT_UNKNOWN;
	else if (ether_type == rte_cpu_to_be_16(ETHER_TYPE_IPv6))
		packet_type |= RTE_PTYPE_L3_IPV6_EXT_UNKNOWN;

	m->packet_type = packet_type;
}

/* callback function to detect packet type for a queue of a port */
static uint16_t
cb_parse_ptype(uint16_t port __rte_unused, uint16_t queue __rte_unused,
		   struct rte_mbuf *pkts[], uint16_t nb_pkts,
		   uint16_t max_pkts __rte_unused,
		   void *user_param __rte_unused)
{
	uint16_t i;

	for (i = 0; i < nb_pkts; ++i)
		parse_ptype(pkts[i]);

	return nb_pkts;
}

static int
init_routing_table(void)
{
	struct rte_lpm *lpm;
	struct rte_lpm6 *lpm6;
	int socket, ret;
	unsigned i;

	for (socket = 0; socket < RTE_MAX_NUMA_NODES; socket++) {
		if (socket_lpm[socket]) {
			lpm = socket_lpm[socket];
			/* populate the LPM table */
			for (i = 0; i < RTE_DIM(l3fwd_ipv4_route_array); i++) {
				ret = rte_lpm_add(lpm,
					l3fwd_ipv4_route_array[i].ip,
					l3fwd_ipv4_route_array[i].depth,
					l3fwd_ipv4_route_array[i].if_out);

				if (ret < 0) {
					RTE_LOG(ERR, IP_FRAG, "Unable to add entry %i to the l3fwd "
						"LPM table\n", i);
					return -1;
				}

				RTE_LOG(INFO, IP_FRAG, "Socket %i: adding route " IPv4_BYTES_FMT
						"/%d (port %d)\n",
					socket,
					IPv4_BYTES(l3fwd_ipv4_route_array[i].ip),
					l3fwd_ipv4_route_array[i].depth,
					l3fwd_ipv4_route_array[i].if_out);
			}
		}

		if (socket_lpm6[socket]) {
			lpm6 = socket_lpm6[socket];
			/* populate the LPM6 table */
			for (i = 0; i < RTE_DIM(l3fwd_ipv6_route_array); i++) {
				ret = rte_lpm6_add(lpm6,
					l3fwd_ipv6_route_array[i].ip,
					l3fwd_ipv6_route_array[i].depth,
					l3fwd_ipv6_route_array[i].if_out);

				if (ret < 0) {
					RTE_LOG(ERR, IP_FRAG, "Unable to add entry %i to the l3fwd "
						"LPM6 table\n", i);
					return -1;
				}

				RTE_LOG(INFO, IP_FRAG, "Socket %i: adding route " IPv6_BYTES_FMT
						"/%d (port %d)\n",
					socket,
					IPv6_BYTES(l3fwd_ipv6_route_array[i].ip),
					l3fwd_ipv6_route_array[i].depth,
					l3fwd_ipv6_route_array[i].if_out);
			}
		}
	}
	return 0;
}

static int
init_mem(void)
{
	char buf[PATH_MAX];
	struct rte_mempool *mp;
	struct rte_lpm *lpm;
	struct rte_lpm6 *lpm6;
	struct rte_lpm_config lpm_config;
	int socket;
	unsigned lcore_id;

	/* traverse through lcores and initialize structures on each socket */

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {

		if (rte_lcore_is_enabled(lcore_id) == 0)
			continue;

		socket = rte_lcore_to_socket_id(lcore_id);

		if (socket == SOCKET_ID_ANY)
			socket = 0;

		if (socket_direct_pool[socket] == NULL) {
			RTE_LOG(INFO, IP_FRAG, "Creating direct mempool on socket %i\n",
					socket);
			snprintf(buf, sizeof(buf), "pool_direct_%i", socket);

			mp = rte_pktmbuf_pool_create(buf, NB_MBUF, 32,
				0, RTE_MBUF_DEFAULT_BUF_SIZE, socket);
			if (mp == NULL) {
				RTE_LOG(ERR, IP_FRAG, "Cannot create direct mempool\n");
				return -1;
			}
			socket_direct_pool[socket] = mp;
		}

		if (socket_indirect_pool[socket] == NULL) {
			RTE_LOG(INFO, IP_FRAG, "Creating indirect mempool on socket %i\n",
					socket);
			snprintf(buf, sizeof(buf), "pool_indirect_%i", socket);

			mp = rte_pktmbuf_pool_create(buf, NB_MBUF, 32, 0, 0,
				socket);
			if (mp == NULL) {
				RTE_LOG(ERR, IP_FRAG, "Cannot create indirect mempool\n");
				return -1;
			}
			socket_indirect_pool[socket] = mp;
		}

		if (socket_lpm[socket] == NULL) {
			RTE_LOG(INFO, IP_FRAG, "Creating LPM table on socket %i\n", socket);
			snprintf(buf, sizeof(buf), "IP_FRAG_LPM_%i", socket);

			lpm_config.max_rules = LPM_MAX_RULES;
			lpm_config.number_tbl8s = 256;
			lpm_config.flags = 0;

			lpm = rte_lpm_create(buf, socket, &lpm_config);
			if (lpm == NULL) {
				RTE_LOG(ERR, IP_FRAG, "Cannot create LPM table\n");
				return -1;
			}
			socket_lpm[socket] = lpm;
		}

		if (socket_lpm6[socket] == NULL) {
			RTE_LOG(INFO, IP_FRAG, "Creating LPM6 table on socket %i\n", socket);
			snprintf(buf, sizeof(buf), "IP_FRAG_LPM_%i", socket);

			lpm6 = rte_lpm6_create(buf, socket, &lpm6_config);
			if (lpm6 == NULL) {
				RTE_LOG(ERR, IP_FRAG, "Cannot create LPM table\n");
				return -1;
			}
			socket_lpm6[socket] = lpm6;
		}
	}

	return 0;
}

int
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf *txconf;
	struct rx_queue *rxq;
	int socket, ret;
	uint16_t nb_ports;
	uint16_t queueid = 0;
	unsigned lcore_id = 0, rx_lcore_id = 0;
	uint32_t n_tx_queue, nb_lcores;
	uint16_t portid;

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

	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No ports found!\n");

	nb_lcores = rte_lcore_count();

	/* initialize structures (mempools, lpm etc.) */
	if (init_mem() < 0)
		rte_panic("Cannot initialize memory structures!\n");

	/* check if portmask has non-existent ports */
	if (enabled_port_mask & ~(RTE_LEN2MASK(nb_ports, unsigned)))
		rte_exit(EXIT_FAILURE, "Non-existent ports in portmask!\n");

	/* initialize all ports */
	RTE_ETH_FOREACH_DEV(portid) {
		struct rte_eth_conf local_port_conf = port_conf;
		struct rte_eth_rxconf rxq_conf;

		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("Skipping disabled port %d\n", portid);
			continue;
		}

		qconf = &lcore_queue_conf[rx_lcore_id];

		/* limit the frame size to the maximum supported by NIC */
		rte_eth_dev_info_get(portid, &dev_info);
		local_port_conf.rxmode.max_rx_pkt_len = RTE_MIN(
		    dev_info.max_rx_pktlen,
		    local_port_conf.rxmode.max_rx_pkt_len);

		/* get the lcore_id for this port */
		while (rte_lcore_is_enabled(rx_lcore_id) == 0 ||
		       qconf->n_rx_queue == (unsigned)rx_queue_per_lcore) {

			rx_lcore_id ++;
			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");

			qconf = &lcore_queue_conf[rx_lcore_id];
		}

		socket = (int) rte_lcore_to_socket_id(rx_lcore_id);
		if (socket == SOCKET_ID_ANY)
			socket = 0;

		rxq = &qconf->rx_queue_list[qconf->n_rx_queue];
		rxq->portid = portid;
		rxq->direct_pool = socket_direct_pool[socket];
		rxq->indirect_pool = socket_indirect_pool[socket];
		rxq->lpm = socket_lpm[socket];
		rxq->lpm6 = socket_lpm6[socket];
		qconf->n_rx_queue++;

		/* init port */
		printf("Initializing port %d on lcore %u...", portid,
		       rx_lcore_id);
		fflush(stdout);

		n_tx_queue = nb_lcores;
		if (n_tx_queue > MAX_TX_QUEUE_PER_PORT)
			n_tx_queue = MAX_TX_QUEUE_PER_PORT;
		if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				DEV_TX_OFFLOAD_MBUF_FAST_FREE;
		ret = rte_eth_dev_configure(portid, 1, (uint16_t)n_tx_queue,
					    &local_port_conf);
		if (ret < 0) {
			printf("\n");
			rte_exit(EXIT_FAILURE, "Cannot configure device: "
				"err=%d, port=%d\n",
				ret, portid);
		}

		/* set the mtu to the maximum received packet size */
		ret = rte_eth_dev_set_mtu(portid,
			local_port_conf.rxmode.max_rx_pkt_len - MTU_OVERHEAD);
		if (ret < 0) {
			printf("\n");
			rte_exit(EXIT_FAILURE, "Set MTU failed: "
				"err=%d, port=%d\n",
			ret, portid);
		}

		ret = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd,
					    &nb_txd);
		if (ret < 0) {
			printf("\n");
			rte_exit(EXIT_FAILURE, "Cannot adjust number of "
				"descriptors: err=%d, port=%d\n", ret, portid);
		}

		/* init one RX queue */
		rxq_conf = dev_info.default_rxconf;
		rxq_conf.offloads = local_port_conf.rxmode.offloads;
		ret = rte_eth_rx_queue_setup(portid, 0, nb_rxd,
					     socket, &rxq_conf,
					     socket_direct_pool[socket]);
		if (ret < 0) {
			printf("\n");
			rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup: "
				"err=%d, port=%d\n",
				ret, portid);
		}

		rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
		print_ethaddr(" Address:", &ports_eth_addr[portid]);
		printf("\n");

		/* init one TX queue per couple (lcore,port) */
		queueid = 0;
		for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;

			socket = (int) rte_lcore_to_socket_id(lcore_id);
			printf("txq=%u,%d ", lcore_id, queueid);
			fflush(stdout);

			txconf = &dev_info.default_txconf;
			txconf->offloads = local_port_conf.txmode.offloads;
			ret = rte_eth_tx_queue_setup(portid, queueid, nb_txd,
						     socket, txconf);
			if (ret < 0) {
				printf("\n");
				rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: "
					"err=%d, port=%d\n", ret, portid);
			}

			qconf = &lcore_queue_conf[lcore_id];
			qconf->tx_queue_id[portid] = queueid;
			queueid++;
		}

		printf("\n");
	}

	printf("\n");

	/* start ports */
	RTE_ETH_FOREACH_DEV(portid) {
		if ((enabled_port_mask & (1 << portid)) == 0) {
			continue;
		}
		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start: err=%d, port=%d\n",
				ret, portid);

		rte_eth_promiscuous_enable(portid);

		if (check_ptype(portid) == 0) {
			rte_eth_add_rx_callback(portid, 0, cb_parse_ptype, NULL);
			printf("Add Rx callback function to detect L3 packet type by SW :"
				" port = %d\n", portid);
		}
	}

	if (init_routing_table() < 0)
		rte_exit(EXIT_FAILURE, "Cannot init routing table\n");

	check_all_ports_link_status(enabled_port_mask);

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}
