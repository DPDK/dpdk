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
#include <rte_malloc.h>
#include <rte_fbk_hash.h>
#include <rte_ip.h>

#define RTE_LOGTYPE_IPv4_MULTICAST RTE_LOGTYPE_USER1

#define MAX_PORTS 16

#define	MCAST_CLONE_PORTS	2
#define	MCAST_CLONE_SEGS	2

#define	PKT_MBUF_SIZE	(2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define	NB_PKT_MBUF	8192

#define	HDR_MBUF_SIZE	(sizeof(struct rte_mbuf) + 2 * RTE_PKTMBUF_HEADROOM)
#define	NB_HDR_MBUF	(NB_PKT_MBUF * MAX_PORTS)

#define	CLONE_MBUF_SIZE	(sizeof(struct rte_mbuf))
#define	NB_CLONE_MBUF	(NB_PKT_MBUF * MCAST_CLONE_PORTS * MCAST_CLONE_SEGS * 2)

/* allow max jumbo frame 9.5 KB */
#define	JUMBO_FRAME_MAX_SIZE	0x2600

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

/* Configure how many packets ahead to prefetch, when reading packets */
#define PREFETCH_OFFSET	3

/*
 * Construct Ethernet multicast address from IPv4 multicast address.
 * Citing RFC 1112, section 6.4:
 * "An IP host group address is mapped to an Ethernet multicast address
 * by placing the low-order 23-bits of the IP address into the low-order
 * 23 bits of the Ethernet multicast address 01-00-5E-00-00-00 (hex)."
 */
#define	ETHER_ADDR_FOR_IPV4_MCAST(x)	\
	(rte_cpu_to_be_64(0x01005e000000ULL | ((x) & 0x7fffff)) >> 16)

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr ports_eth_addr[MAX_PORTS];

/* mask of enabled ports */
static uint32_t enabled_port_mask = 0;

static uint8_t nb_ports = 0;

static int rx_queue_per_lcore = 1;

struct mbuf_table {
	uint16_t len;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16
struct lcore_queue_conf {
	uint64_t tx_tsc;
	uint16_t n_rx_queue;
	uint8_t rx_queue_list[MAX_RX_QUEUE_PER_LCORE];
	uint16_t tx_queue_id[MAX_PORTS];
	struct mbuf_table tx_mbufs[MAX_PORTS];
} __rte_cache_aligned;
static struct lcore_queue_conf lcore_queue_conf[RTE_MAX_LCORE];

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

static struct rte_mempool *packet_pool, *header_pool, *clone_pool;


/* Multicast */
static struct rte_fbk_hash_params mcast_hash_params = {
	.name = "MCAST_HASH",
	.entries = 1024,
	.entries_per_bucket = 4,
	.socket_id = 0,
	.hash_func = NULL,
	.init_val = 0,
};

struct rte_fbk_hash_table *mcast_hash = NULL;

struct mcast_group_params {
	uint32_t ip;
	uint16_t port_mask;
};

static struct mcast_group_params mcast_group_table[] = {
		{IPv4(224,0,0,101), 0x1},
		{IPv4(224,0,0,102), 0x2},
		{IPv4(224,0,0,103), 0x3},
		{IPv4(224,0,0,104), 0x4},
		{IPv4(224,0,0,105), 0x5},
		{IPv4(224,0,0,106), 0x6},
		{IPv4(224,0,0,107), 0x7},
		{IPv4(224,0,0,108), 0x8},
		{IPv4(224,0,0,109), 0x9},
		{IPv4(224,0,0,110), 0xA},
		{IPv4(224,0,0,111), 0xB},
		{IPv4(224,0,0,112), 0xC},
		{IPv4(224,0,0,113), 0xD},
		{IPv4(224,0,0,114), 0xE},
		{IPv4(224,0,0,115), 0xF},
};

#define N_MCAST_GROUPS \
	(sizeof (mcast_group_table) / sizeof (mcast_group_table[0]))


/* Send burst of packets on an output interface */
static void
send_burst(struct lcore_queue_conf *qconf, uint8_t port)
{
	struct rte_mbuf **m_table;
	uint16_t n, queueid;
	int ret;

	queueid = qconf->tx_queue_id[port];
	m_table = (struct rte_mbuf **)qconf->tx_mbufs[port].m_table;
	n = qconf->tx_mbufs[port].len;

	ret = rte_eth_tx_burst(port, queueid, m_table, n);
	while (unlikely (ret < n)) {
		rte_pktmbuf_free(m_table[ret]);
		ret++;
	}

	qconf->tx_mbufs[port].len = 0;
}

/* Get number of bits set. */
static inline uint32_t
bitcnt(uint32_t v)
{
	uint32_t n;

	for (n = 0; v != 0; v &= v - 1, n++)
		;

	return (n);
}

/**
 * Create the output multicast packet based on the given input packet.
 * There are two approaches for creating outgoing packet, though both
 * are based on data zero-copy idea, they differ in few details:
 * First one creates a clone of the input packet, e.g - walk though all
 * segments of the input packet, and for each of them create a new packet
 * mbuf and attach that new mbuf to the segment (refer to rte_pktmbuf_clone()
 * for more details). Then new mbuf is allocated for the packet header
 * and is prepended to the 'clone' mbuf.
 * Second approach doesn't make a clone, it just increment refcnt for all
 * input packet segments. Then it allocates new mbuf for the packet header
 * and prepends it to the input packet.
 * Basically first approach reuses only input packet's data, but creates
 * it's own copy of packet's metadata. Second approach reuses both input's
 * packet data and metadata.
 * The advantage of first approach - is that each outgoing packet has it's
 * own copy of metadata, so we can safely modify data pointer of the
 * input packet. That allows us to skip creation if the output packet for
 * the last destination port, but instead modify input packet's header inplace,
 * e.g: for N destination ports we need to invoke mcast_out_pkt (N-1) times.
 * The advantage of second approach - less work for each outgoing packet,
 * e.g: we skip "clone" operation completely. Though it comes with a price -
 * input packet's metadata has to be intact. So for N destination ports we
 * need to invoke mcast_out_pkt N times.
 * So for small number of outgoing ports (and segments in the input packet)
 * first approach will be faster.
 * As number of outgoing ports (and/or input segments) will grow,
 * second way will become more preferable.
 *
 *  @param pkt
 *  Input packet mbuf.
 *  @param use_clone
 *  Control which of the two approaches described above should be used:
 *  - 0 - use second approach:
 *    Don't "clone" input packet.
 *    Prepend new header directly to the input packet
 *  - 1 - use first approach:
 *    Make a "clone" of input packet first.
 *    Prepend new header to the clone of the input packet
 *  @return
 *  - The pointer to the new outgoing packet.
 *  - NULL if operation failed.
 */
static inline struct rte_mbuf *
mcast_out_pkt(struct rte_mbuf *pkt, int use_clone)
{
	struct rte_mbuf *hdr;

	/* Create new mbuf for the header. */
	if (unlikely ((hdr = rte_pktmbuf_alloc(header_pool)) == NULL))
		return (NULL);

	/* If requested, then make a new clone packet. */
	if (use_clone != 0 &&
	    unlikely ((pkt = rte_pktmbuf_clone(pkt, clone_pool)) == NULL)) {
		rte_pktmbuf_free(hdr);
		return (NULL);
	}

	/* prepend new header */
	hdr->next = pkt;


	/* update header's fields */
	hdr->pkt_len = (uint16_t)(hdr->data_len + pkt->pkt_len);
	hdr->nb_segs = (uint8_t)(pkt->nb_segs + 1);

	/* copy metadata from source packet*/
	hdr->port = pkt->port;
	hdr->vlan_tci = pkt->vlan_tci;
	hdr->tx_offload = pkt->tx_offload;
	hdr->hash = pkt->hash;

	hdr->ol_flags = pkt->ol_flags;

	__rte_mbuf_sanity_check(hdr, 1);
	return (hdr);
}

/*
 * Write new Ethernet header to the outgoing packet,
 * and put it into the outgoing queue for the given port.
 */
static inline void
mcast_send_pkt(struct rte_mbuf *pkt, struct ether_addr *dest_addr,
		struct lcore_queue_conf *qconf, uint8_t port)
{
	struct ether_hdr *ethdr;
	uint16_t len;

	/* Construct Ethernet header. */
	ethdr = (struct ether_hdr *)rte_pktmbuf_prepend(pkt, (uint16_t)sizeof(*ethdr));
	RTE_MBUF_ASSERT(ethdr != NULL);

	ether_addr_copy(dest_addr, &ethdr->d_addr);
	ether_addr_copy(&ports_eth_addr[port], &ethdr->s_addr);
	ethdr->ether_type = rte_be_to_cpu_16(ETHER_TYPE_IPv4);

	/* Put new packet into the output queue */
	len = qconf->tx_mbufs[port].len;
	qconf->tx_mbufs[port].m_table[len] = pkt;
	qconf->tx_mbufs[port].len = ++len;

	/* Transmit packets */
	if (unlikely(MAX_PKT_BURST == len))
		send_burst(qconf, port);
}

/* Multicast forward of the input packet */
static inline void
mcast_forward(struct rte_mbuf *m, struct lcore_queue_conf *qconf)
{
	struct rte_mbuf *mc;
	struct ipv4_hdr *iphdr;
	uint32_t dest_addr, port_mask, port_num, use_clone;
	int32_t hash;
	uint8_t port;
	union {
		uint64_t as_int;
		struct ether_addr as_addr;
	} dst_eth_addr;

	/* Remove the Ethernet header from the input packet */
	iphdr = (struct ipv4_hdr *)rte_pktmbuf_adj(m, (uint16_t)sizeof(struct ether_hdr));
	RTE_MBUF_ASSERT(iphdr != NULL);

	dest_addr = rte_be_to_cpu_32(iphdr->dst_addr);

	/*
	 * Check that it is a valid multicast address and
	 * we have some active ports assigned to it.
	 */
	if(!IS_IPV4_MCAST(dest_addr) ||
	    (hash = rte_fbk_hash_lookup(mcast_hash, dest_addr)) <= 0 ||
	    (port_mask = hash & enabled_port_mask) == 0) {
		rte_pktmbuf_free(m);
		return;
	}

	/* Calculate number of destination ports. */
	port_num = bitcnt(port_mask);

	/* Should we use rte_pktmbuf_clone() or not. */
	use_clone = (port_num <= MCAST_CLONE_PORTS &&
	    m->nb_segs <= MCAST_CLONE_SEGS);

	/* Mark all packet's segments as referenced port_num times */
	if (use_clone == 0)
		rte_pktmbuf_refcnt_update(m, (uint16_t)port_num);

	/* construct destination ethernet address */
	dst_eth_addr.as_int = ETHER_ADDR_FOR_IPV4_MCAST(dest_addr);

	for (port = 0; use_clone != port_mask; port_mask >>= 1, port++) {

		/* Prepare output packet and send it out. */
		if ((port_mask & 1) != 0) {
			if (likely ((mc = mcast_out_pkt(m, use_clone)) != NULL))
				mcast_send_pkt(mc, &dst_eth_addr.as_addr,
						qconf, port);
			else if (use_clone == 0)
				rte_pktmbuf_free(m);
		}
	}

	/*
	 * If we making clone packets, then, for the last destination port,
	 * we can overwrite input packet's metadata.
	 */
	if (use_clone != 0)
		mcast_send_pkt(m, &dst_eth_addr.as_addr, qconf, port);
	else
		rte_pktmbuf_free(m);
}

/* Send burst of outgoing packet, if timeout expires. */
static inline void
send_timeout_burst(struct lcore_queue_conf *qconf)
{
	uint64_t cur_tsc;
	uint8_t portid;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;

	cur_tsc = rte_rdtsc();
	if (likely (cur_tsc < qconf->tx_tsc + drain_tsc))
		return;

	for (portid = 0; portid < MAX_PORTS; portid++) {
		if (qconf->tx_mbufs[portid].len != 0)
			send_burst(qconf, portid);
	}
	qconf->tx_tsc = cur_tsc;
}

/* main processing loop */
static int
main_loop(__rte_unused void *dummy)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	unsigned lcore_id;
	int i, j, nb_rx;
	uint8_t portid;
	struct lcore_queue_conf *qconf;

	lcore_id = rte_lcore_id();
	qconf = &lcore_queue_conf[lcore_id];


	if (qconf->n_rx_queue == 0) {
		RTE_LOG(INFO, IPv4_MULTICAST, "lcore %u has nothing to do\n",
		    lcore_id);
		return 0;
	}

	RTE_LOG(INFO, IPv4_MULTICAST, "entering main loop on lcore %u\n",
	    lcore_id);

	for (i = 0; i < qconf->n_rx_queue; i++) {

		portid = qconf->rx_queue_list[i];
		RTE_LOG(INFO, IPv4_MULTICAST, " -- lcoreid=%u portid=%d\n",
		    lcore_id, (int) portid);
	}

	while (1) {

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
				mcast_forward(pkts_burst[j], qconf);
			}

			/* Forward remaining prefetched packets */
			for (; j < nb_rx; j++) {
				mcast_forward(pkts_burst[j], qconf);
			}
		}

		/* Send out packets from TX queues */
		send_timeout_burst(qconf);
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

static uint32_t
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned long pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return ((uint32_t)pm);
}

static int
parse_nqueue(const char *q_arg)
{
	char *end = NULL;
	unsigned long n;

	/* parse numerical string */
	errno = 0;
	n = strtoul(q_arg, &end, 0);
	if (errno != 0 || end == NULL || *end != '\0' ||
			n == 0 || n >= MAX_RX_QUEUE_PER_LCORE)
		return (-1);

	return (n);
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
			if (enabled_port_mask == 0) {
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

		default:
			print_usage(prgname);
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
print_ethaddr(const char *name, struct ether_addr *eth_addr)
{
	char buf[ETHER_ADDR_FMT_SIZE];
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

static int
init_mcast_hash(void)
{
	uint32_t i;

	mcast_hash_params.socket_id = rte_socket_id();
	mcast_hash = rte_fbk_hash_create(&mcast_hash_params);
	if (mcast_hash == NULL){
		return -1;
	}

	for (i = 0; i < N_MCAST_GROUPS; i ++){
		if (rte_fbk_hash_add_key(mcast_hash,
			mcast_group_table[i].ip,
			mcast_group_table[i].port_mask) < 0) {
			return -1;
		}
	}

	return 0;
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
main(int argc, char **argv)
{
	struct lcore_queue_conf *qconf;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf *txconf;
	int ret;
	uint16_t queueid;
	unsigned lcore_id = 0, rx_lcore_id = 0;
	uint32_t n_tx_queue, nb_lcores;
	uint8_t portid;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL parameters\n");
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid IPV4_MULTICAST parameters\n");

	/* create the mbuf pools */
	packet_pool = rte_mempool_create("packet_pool", NB_PKT_MBUF,
	    PKT_MBUF_SIZE, 32, sizeof(struct rte_pktmbuf_pool_private),
	    rte_pktmbuf_pool_init, NULL, rte_pktmbuf_init, NULL,
	    rte_socket_id(), 0);

	if (packet_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init packet mbuf pool\n");

	header_pool = rte_mempool_create("header_pool", NB_HDR_MBUF,
	    HDR_MBUF_SIZE, 32, 0, NULL, NULL, rte_pktmbuf_init, NULL,
	    rte_socket_id(), 0);

	if (header_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init header mbuf pool\n");

	clone_pool = rte_mempool_create("clone_pool", NB_CLONE_MBUF,
	    CLONE_MBUF_SIZE, 32, 0, NULL, NULL, rte_pktmbuf_init, NULL,
	    rte_socket_id(), 0);

	if (clone_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init clone mbuf pool\n");

	nb_ports = rte_eth_dev_count();
	if (nb_ports == 0)
		rte_exit(EXIT_FAILURE, "No physical ports!\n");
	if (nb_ports > MAX_PORTS)
		nb_ports = MAX_PORTS;

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
			qconf = &lcore_queue_conf[rx_lcore_id];

			if (rx_lcore_id >= RTE_MAX_LCORE)
				rte_exit(EXIT_FAILURE, "Not enough cores\n");
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
			rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%d\n",
				  ret, portid);

		rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
		print_ethaddr(" Address:", &ports_eth_addr[portid]);
		printf(", ");

		/* init one RX queue */
		queueid = 0;
		printf("rxq=%hu ", queueid);
		fflush(stdout);
		ret = rte_eth_rx_queue_setup(portid, queueid, nb_rxd,
					     rte_eth_dev_socket_id(portid),
					     NULL,
					     packet_pool);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: err=%d, port=%d\n",
				  ret, portid);

		/* init one TX queue per couple (lcore,port) */
		queueid = 0;

		RTE_LCORE_FOREACH(lcore_id) {
			if (rte_lcore_is_enabled(lcore_id) == 0)
				continue;
			printf("txq=%u,%hu ", lcore_id, queueid);
			fflush(stdout);

			rte_eth_dev_info_get(portid, &dev_info);
			txconf = &dev_info.default_txconf;
			txconf->txq_flags = 0;
			ret = rte_eth_tx_queue_setup(portid, queueid, nb_txd,
						     rte_lcore_to_socket_id(lcore_id), txconf);
			if (ret < 0)
				rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: err=%d, "
					  "port=%d\n", ret, portid);

			qconf = &lcore_queue_conf[lcore_id];
			qconf->tx_queue_id[portid] = queueid;
			queueid++;
		}

		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_exit(EXIT_FAILURE, "rte_eth_dev_start: err=%d, port=%d\n",
				  ret, portid);

		printf("done:\n");
	}

	check_all_ports_link_status(nb_ports, enabled_port_mask);

	/* initialize the multicast hash */
	int retval = init_mcast_hash();
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "Cannot build the multicast hash\n");

	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (rte_eal_wait_lcore(lcore_id) < 0)
			return -1;
	}

	return 0;
}
