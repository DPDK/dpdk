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
#include <rte_ip.h>
#include <rte_string_fns.h>

#include "crypto.h"

#define NB_MBUF   (32 * 1024)

#define MAX_PKT_BURST 32
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */

#define TX_QUEUE_FLUSH_MASK 0xFFFFFFFF
#define TSC_COUNT_LIMIT 1000

#define ACTION_ENCRYPT 1
#define ACTION_DECRYPT 2

/*
 * Configurable number of RX/TX ring descriptors
 */
#define RTE_TEST_RX_DESC_DEFAULT 128
#define RTE_TEST_TX_DESC_DEFAULT 512
static uint16_t nb_rxd = RTE_TEST_RX_DESC_DEFAULT;
static uint16_t nb_txd = RTE_TEST_TX_DESC_DEFAULT;

/* ethernet addresses of ports */
static struct ether_addr ports_eth_addr[RTE_MAX_ETHPORTS];

/* mask of enabled ports */
static unsigned enabled_port_mask = 0;
static int promiscuous_on = 1; /**< Ports set in promiscuous mode on by default. */

/* list of enabled ports */
static uint32_t dst_ports[RTE_MAX_ETHPORTS];

struct mbuf_table {
	uint16_t len;
	struct rte_mbuf *m_table[MAX_PKT_BURST];
};

struct lcore_rx_queue {
	uint8_t port_id;
	uint8_t queue_id;
};

#define MAX_RX_QUEUE_PER_LCORE 16

#define MAX_LCORE_PARAMS 1024
struct lcore_params {
	uint8_t port_id;
	uint8_t queue_id;
	uint8_t lcore_id;
};

static struct lcore_params lcore_params_array[MAX_LCORE_PARAMS];
static struct lcore_params lcore_params_array_default[] = {
	{0, 0, 2},
	{0, 1, 2},
	{0, 2, 2},
	{1, 0, 2},
	{1, 1, 2},
	{1, 2, 2},
	{2, 0, 2},
	{3, 0, 3},
	{3, 1, 3},
};

static struct lcore_params * lcore_params = lcore_params_array_default;
static uint16_t nb_lcore_params = sizeof(lcore_params_array_default) /
				sizeof(lcore_params_array_default[0]);

static struct rte_eth_conf port_conf = {
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
	},
};

static struct rte_mempool * pktmbuf_pool[RTE_MAX_NUMA_NODES];

struct lcore_conf {
	uint64_t tsc;
	uint64_t tsc_count;
	uint32_t tx_mask;
	uint16_t n_rx_queue;
	uint16_t rx_queue_list_pos;
	struct lcore_rx_queue rx_queue_list[MAX_RX_QUEUE_PER_LCORE];
	uint16_t tx_queue_id[RTE_MAX_ETHPORTS];
	struct mbuf_table rx_mbuf;
	uint32_t rx_mbuf_pos;
	uint32_t rx_curr_queue;
	struct mbuf_table tx_mbufs[RTE_MAX_ETHPORTS];
} __rte_cache_aligned;

static struct lcore_conf lcore_conf[RTE_MAX_LCORE];

static inline struct rte_mbuf *
nic_rx_get_packet(struct lcore_conf *qconf)
{
	struct rte_mbuf *pkt;

	if (unlikely(qconf->n_rx_queue == 0))
		return NULL;

	/* Look for the next queue with packets; return if none */
	if (unlikely(qconf->rx_mbuf_pos == qconf->rx_mbuf.len)) {
		uint32_t i;

		qconf->rx_mbuf_pos = 0;
		for (i = 0; i < qconf->n_rx_queue; i++) {
			qconf->rx_mbuf.len = rte_eth_rx_burst(
				qconf->rx_queue_list[qconf->rx_curr_queue].port_id,
				qconf->rx_queue_list[qconf->rx_curr_queue].queue_id,
				qconf->rx_mbuf.m_table, MAX_PKT_BURST);

			qconf->rx_curr_queue++;
			if (unlikely(qconf->rx_curr_queue == qconf->n_rx_queue))
				qconf->rx_curr_queue = 0;
			if (likely(qconf->rx_mbuf.len > 0))
				break;
		}
		if (unlikely(i == qconf->n_rx_queue))
			return NULL;
	}

	/* Get the next packet from the current queue; if last packet, go to next queue */
	pkt = qconf->rx_mbuf.m_table[qconf->rx_mbuf_pos];
	qconf->rx_mbuf_pos++;

	return pkt;
}

static inline void
nic_tx_flush_queues(struct lcore_conf *qconf)
{
	uint8_t portid;

	for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++) {
		struct rte_mbuf **m_table = NULL;
		uint16_t queueid, len;
		uint32_t n, i;

		if (likely((qconf->tx_mask & (1 << portid)) == 0))
			continue;

		len = qconf->tx_mbufs[portid].len;
		if (likely(len == 0))
			continue;

		queueid = qconf->tx_queue_id[portid];
		m_table = qconf->tx_mbufs[portid].m_table;

		n = rte_eth_tx_burst(portid, queueid, m_table, len);
		for (i = n; i < len; i++){
			rte_pktmbuf_free(m_table[i]);
		}

		qconf->tx_mbufs[portid].len = 0;
	}

	qconf->tx_mask = TX_QUEUE_FLUSH_MASK;
}

static inline void
nic_tx_send_packet(struct rte_mbuf *pkt, uint8_t port)
{
	struct lcore_conf *qconf;
	uint32_t lcoreid;
	uint16_t len;

	if (unlikely(pkt == NULL)) {
		return;
	}

	lcoreid = rte_lcore_id();
	qconf = &lcore_conf[lcoreid];

	len = qconf->tx_mbufs[port].len;
	qconf->tx_mbufs[port].m_table[len] = pkt;
	len++;

	/* enough pkts to be sent */
	if (unlikely(len == MAX_PKT_BURST)) {
		uint32_t n, i;
		uint16_t queueid;

		queueid = qconf->tx_queue_id[port];
		n = rte_eth_tx_burst(port, queueid, qconf->tx_mbufs[port].m_table, MAX_PKT_BURST);
		for (i = n; i < MAX_PKT_BURST; i++){
			rte_pktmbuf_free(qconf->tx_mbufs[port].m_table[i]);
		}

		qconf->tx_mask &= ~(1 << port);
		len = 0;
	}

	qconf->tx_mbufs[port].len = len;
}

/* main processing loop */
static __attribute__((noreturn)) int
main_loop(__attribute__((unused)) void *dummy)
{
	uint32_t lcoreid;
	struct lcore_conf *qconf;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S * BURST_TX_DRAIN_US;

	lcoreid = rte_lcore_id();
	qconf = &lcore_conf[lcoreid];

	printf("Thread %u starting...\n", lcoreid);

	for (;;) {
		struct rte_mbuf *pkt;
		uint32_t pkt_from_nic_rx = 0;
		uint8_t port;

		/* Flush TX queues */
		qconf->tsc_count++;
		if (unlikely(qconf->tsc_count == TSC_COUNT_LIMIT)) {
			uint64_t tsc, diff_tsc;

			tsc = rte_rdtsc();

			diff_tsc = tsc - qconf->tsc;
			if (unlikely(diff_tsc > drain_tsc)) {
				nic_tx_flush_queues(qconf);
				crypto_flush_tx_queue(lcoreid);
				qconf->tsc = tsc;
			}

			qconf->tsc_count = 0;
		}

		/*
		 * Check the Intel QuickAssist queues first
		 *
		 ***/
		pkt = (struct rte_mbuf *) crypto_get_next_response();
		if (pkt == NULL) {
			pkt = nic_rx_get_packet(qconf);
			pkt_from_nic_rx = 1;
		}
		if (pkt == NULL)
			continue;
		/* Send packet to either QAT encrypt, QAT decrypt or NIC TX */
		if (pkt_from_nic_rx) {
			struct ipv4_hdr *ip  = rte_pktmbuf_mtod_offset(pkt,
								       struct ipv4_hdr *,
								       sizeof(struct ether_hdr));
			if (ip->src_addr & rte_cpu_to_be_32(ACTION_ENCRYPT)) {
				if (CRYPTO_RESULT_FAIL == crypto_encrypt(pkt,
					(enum cipher_alg)((ip->src_addr >> 16) & 0xFF),
					(enum hash_alg)((ip->src_addr >> 8) & 0xFF)))
					rte_pktmbuf_free(pkt);
				continue;
			}

			if (ip->src_addr & rte_cpu_to_be_32(ACTION_DECRYPT)) {
				if(CRYPTO_RESULT_FAIL == crypto_decrypt(pkt,
					(enum cipher_alg)((ip->src_addr >> 16) & 0xFF),
					(enum hash_alg)((ip->src_addr >> 8) & 0xFF)))
					rte_pktmbuf_free(pkt);
				continue;
			}
		}

		port = dst_ports[pkt->port];

		/* Transmit the packet */
		nic_tx_send_packet(pkt, (uint8_t)port);
	}
}

static inline unsigned
get_port_max_rx_queues(uint8_t port_id)
{
	struct rte_eth_dev_info dev_info;

	rte_eth_dev_info_get(port_id, &dev_info);
	return dev_info.max_rx_queues;
}

static inline unsigned
get_port_max_tx_queues(uint8_t port_id)
{
	struct rte_eth_dev_info dev_info;

	rte_eth_dev_info_get(port_id, &dev_info);
	return dev_info.max_tx_queues;
}

static int
check_lcore_params(void)
{
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		if (lcore_params[i].queue_id >= get_port_max_rx_queues(lcore_params[i].port_id)) {
			printf("invalid queue number: %hhu\n", lcore_params[i].queue_id);
			return -1;
		}
		if (!rte_lcore_is_enabled(lcore_params[i].lcore_id)) {
			printf("error: lcore %hhu is not enabled in lcore mask\n",
				lcore_params[i].lcore_id);
			return -1;
		}
	}
	return 0;
}

static int
check_port_config(const unsigned nb_ports)
{
	unsigned portid;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		portid = lcore_params[i].port_id;
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("port %u is not enabled in port mask\n", portid);
			return -1;
		}
		if (portid >= nb_ports) {
			printf("port %u is not present on the board\n", portid);
			return -1;
		}
	}
	return 0;
}

static uint8_t
get_port_n_rx_queues(const uint8_t port)
{
	int queue = -1;
	uint16_t i;

	for (i = 0; i < nb_lcore_params; ++i) {
		if (lcore_params[i].port_id == port && lcore_params[i].queue_id > queue)
			queue = lcore_params[i].queue_id;
	}
	return (uint8_t)(++queue);
}

static int
init_lcore_rx_queues(void)
{
	uint16_t i, nb_rx_queue;
	uint8_t lcore;

	for (i = 0; i < nb_lcore_params; ++i) {
		lcore = lcore_params[i].lcore_id;
		nb_rx_queue = lcore_conf[lcore].n_rx_queue;
		if (nb_rx_queue >= MAX_RX_QUEUE_PER_LCORE) {
			printf("error: too many queues (%u) for lcore: %u\n",
				(unsigned)nb_rx_queue + 1, (unsigned)lcore);
			return -1;
		}
		lcore_conf[lcore].rx_queue_list[nb_rx_queue].port_id =
			lcore_params[i].port_id;
		lcore_conf[lcore].rx_queue_list[nb_rx_queue].queue_id =
			lcore_params[i].queue_id;
		lcore_conf[lcore].n_rx_queue++;
	}
	return 0;
}

/* display usage */
static void
print_usage(const char *prgname)
{
	printf ("%s [EAL options] -- -p PORTMASK [--no-promisc]"
		"  [--config '(port,queue,lcore)[,(port,queue,lcore)]'\n"
		"  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
		"  --no-promisc: disable promiscuous mode (default is ON)\n"
		"  --config '(port,queue,lcore)': rx queues configuration\n",
		prgname);
}

static unsigned
parse_portmask(const char *portmask)
{
	char *end = NULL;
	unsigned pm;

	/* parse hexadecimal string */
	pm = strtoul(portmask, &end, 16);
	if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0'))
		return 0;

	return pm;
}

static int
parse_config(const char *q_arg)
{
	char s[256];
	const char *p, *p_end = q_arg;
	char *end;
	enum fieldnames {
		FLD_PORT = 0,
		FLD_QUEUE,
		FLD_LCORE,
		_NUM_FLD
	};
	unsigned long int_fld[_NUM_FLD];
	char *str_fld[_NUM_FLD];
	int i;
	unsigned size;

	nb_lcore_params = 0;

	while ((p = strchr(p_end,'(')) != NULL) {
		if (nb_lcore_params >= MAX_LCORE_PARAMS) {
			printf("exceeded max number of lcore params: %hu\n",
				nb_lcore_params);
			return -1;
		}
		++p;
		if((p_end = strchr(p,')')) == NULL)
			return -1;

		size = p_end - p;
		if(size >= sizeof(s))
			return -1;

		snprintf(s, sizeof(s), "%.*s", size, p);
		if (rte_strsplit(s, sizeof(s), str_fld, _NUM_FLD, ',') != _NUM_FLD)
			return -1;
		for (i = 0; i < _NUM_FLD; i++) {
			errno = 0;
			int_fld[i] = strtoul(str_fld[i], &end, 0);
			if (errno != 0 || end == str_fld[i] || int_fld[i] > 255)
				return -1;
		}
		lcore_params_array[nb_lcore_params].port_id = (uint8_t)int_fld[FLD_PORT];
		lcore_params_array[nb_lcore_params].queue_id = (uint8_t)int_fld[FLD_QUEUE];
		lcore_params_array[nb_lcore_params].lcore_id = (uint8_t)int_fld[FLD_LCORE];
		++nb_lcore_params;
	}
	lcore_params = lcore_params_array;
	return 0;
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
		{"config", 1, 0, 0},
		{"no-promisc", 0, 0, 0},
		{NULL, 0, 0, 0}
	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "p:",
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

		/* long options */
		case 0:
			if (strcmp(lgopts[option_index].name, "config") == 0) {
				ret = parse_config(optarg);
				if (ret) {
					printf("invalid config\n");
					print_usage(prgname);
					return -1;
				}
			}
			if (strcmp(lgopts[option_index].name, "no-promisc") == 0) {
				printf("Promiscuous mode disabled\n");
				promiscuous_on = 0;
			}
			break;
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
print_ethaddr(const char *name, const struct ether_addr *eth_addr)
{
	char buf[ETHER_ADDR_FMT_SIZE];
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", name, buf);
}

static int
init_mem(void)
{
	int socketid;
	unsigned lcoreid;
	char s[64];

	RTE_LCORE_FOREACH(lcoreid) {
		socketid = rte_lcore_to_socket_id(lcoreid);
		if (socketid >= RTE_MAX_NUMA_NODES) {
			printf("Socket %d of lcore %u is out of range %d\n",
				socketid, lcoreid, RTE_MAX_NUMA_NODES);
			return -1;
		}
		if (pktmbuf_pool[socketid] == NULL) {
			snprintf(s, sizeof(s), "mbuf_pool_%d", socketid);
			pktmbuf_pool[socketid] =
				rte_pktmbuf_pool_create(s, NB_MBUF, 32, 0,
					RTE_MBUF_DEFAULT_BUF_SIZE, socketid);
			if (pktmbuf_pool[socketid] == NULL) {
				printf("Cannot init mbuf pool on socket %d\n", socketid);
				return -1;
			}
			printf("Allocated mbuf pool on socket %d\n", socketid);
		}
	}
	return 0;
}

int
main(int argc, char **argv)
{
	struct lcore_conf *qconf;
	struct rte_eth_link link;
	int ret;
	unsigned nb_ports;
	uint16_t queueid;
	unsigned lcoreid;
	uint32_t nb_tx_queue;
	uint8_t portid, nb_rx_queue, queue, socketid, last_port;
        unsigned nb_ports_in_mask = 0;

	/* init EAL */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		return -1;
	argc -= ret;
	argv += ret;

	/* parse application arguments (after the EAL ones) */
	ret = parse_args(argc, argv);
	if (ret < 0)
		return -1;

	if (check_lcore_params() < 0)
		rte_panic("check_lcore_params failed\n");

	ret = init_lcore_rx_queues();
	if (ret < 0)
		return -1;

	ret = init_mem();
	if (ret < 0)
		return -1;

	nb_ports = rte_eth_dev_count();

	if (check_port_config(nb_ports) < 0)
		rte_panic("check_port_config failed\n");

        /* reset dst_ports */
        for (portid = 0; portid < RTE_MAX_ETHPORTS; portid++)
                dst_ports[portid] = 0;
        last_port = 0;

        /*
         * Each logical core is assigned a dedicated TX queue on each port.
         */
        for (portid = 0; portid < nb_ports; portid++) {
                /* skip ports that are not enabled */
                if ((enabled_port_mask & (1 << portid)) == 0)
                        continue;

                if (nb_ports_in_mask % 2) {
                        dst_ports[portid] = last_port;
                        dst_ports[last_port] = portid;
                }
                else
                        last_port = portid;

                nb_ports_in_mask++;
        }
        if (nb_ports_in_mask % 2) {
                printf("Notice: odd number of ports in portmask.\n");
                dst_ports[last_port] = last_port;
        }

	/* initialize all ports */
	for (portid = 0; portid < nb_ports; portid++) {
		/* skip ports that are not enabled */
		if ((enabled_port_mask & (1 << portid)) == 0) {
			printf("\nSkipping disabled port %d\n", portid);
			continue;
		}

		/* init port */
		printf("Initializing port %d ... ", portid );
		fflush(stdout);

		nb_rx_queue = get_port_n_rx_queues(portid);
		if (nb_rx_queue > get_port_max_rx_queues(portid))
			rte_panic("Number of rx queues %d exceeds max number of rx queues %u"
				" for port %d\n", nb_rx_queue, get_port_max_rx_queues(portid),
				portid);
		nb_tx_queue = rte_lcore_count();
		if (nb_tx_queue > get_port_max_tx_queues(portid))
			rte_panic("Number of lcores %u exceeds max number of tx queues %u"
				" for port %d\n", nb_tx_queue, get_port_max_tx_queues(portid),
				portid);
		printf("Creating queues: nb_rxq=%d nb_txq=%u... ",
			nb_rx_queue, (unsigned)nb_tx_queue );
		ret = rte_eth_dev_configure(portid, nb_rx_queue,
					(uint16_t)nb_tx_queue, &port_conf);
		if (ret < 0)
			rte_panic("Cannot configure device: err=%d, port=%d\n",
				ret, portid);

		rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
		print_ethaddr(" Address:", &ports_eth_addr[portid]);
		printf(", ");

		/* init one TX queue per couple (lcore,port) */
		queueid = 0;
		RTE_LCORE_FOREACH(lcoreid) {
			socketid = (uint8_t)rte_lcore_to_socket_id(lcoreid);
			printf("txq=%u,%d,%d ", lcoreid, queueid, socketid);
			fflush(stdout);
			ret = rte_eth_tx_queue_setup(portid, queueid, nb_txd,
					socketid,
					NULL);
			if (ret < 0)
				rte_panic("rte_eth_tx_queue_setup: err=%d, "
					"port=%d\n", ret, portid);

			qconf = &lcore_conf[lcoreid];
			qconf->tx_queue_id[portid] = queueid;
			queueid++;
		}
		printf("\n");
	}

	RTE_LCORE_FOREACH(lcoreid) {
		qconf = &lcore_conf[lcoreid];
		printf("\nInitializing rx queues on lcore %u ... ", lcoreid );
		fflush(stdout);
		/* init RX queues */
		for(queue = 0; queue < qconf->n_rx_queue; ++queue) {
			portid = qconf->rx_queue_list[queue].port_id;
			queueid = qconf->rx_queue_list[queue].queue_id;
			socketid = (uint8_t)rte_lcore_to_socket_id(lcoreid);
			printf("rxq=%d,%d,%d ", portid, queueid, socketid);
			fflush(stdout);

			ret = rte_eth_rx_queue_setup(portid, queueid, nb_rxd,
					socketid,
					NULL,
					pktmbuf_pool[socketid]);
			if (ret < 0)
				rte_panic("rte_eth_rx_queue_setup: err=%d,"
						"port=%d\n", ret, portid);
		}
	}

	printf("\n");

	/* start ports */
	for (portid = 0; portid < nb_ports; portid++) {
		if ((enabled_port_mask & (1 << portid)) == 0)
			continue;
		/* Start device */
		ret = rte_eth_dev_start(portid);
		if (ret < 0)
			rte_panic("rte_eth_dev_start: err=%d, port=%d\n",
				ret, portid);

		printf("done: Port %d ", portid);

		/* get link status */
		rte_eth_link_get(portid, &link);
		if (link.link_status)
			printf(" Link Up - speed %u Mbps - %s\n",
			       (unsigned) link.link_speed,
			       (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
			       ("full-duplex") : ("half-duplex\n"));
		else
			printf(" Link Down\n");
		/*
		 * If enabled, put device in promiscuous mode.
		 * This allows IO forwarding mode to forward packets
		 * to itself through 2 cross-connected  ports of the
		 * target machine.
		 */
		if (promiscuous_on)
			rte_eth_promiscuous_enable(portid);
	}
	printf("Crypto: Initializing Crypto...\n");
	if (crypto_init() != 0)
		return -1;

	RTE_LCORE_FOREACH(lcoreid) {
		if (per_core_crypto_init(lcoreid) != 0) {
	        printf("Crypto: Cannot init lcore crypto on lcore %u\n", (unsigned)lcoreid);
			return -1;
		}
	}
	printf("Crypto: Initialization complete\n");
	/* launch per-lcore init on every lcore */
	rte_eal_mp_remote_launch(main_loop, NULL, CALL_MASTER);
	RTE_LCORE_FOREACH_SLAVE(lcoreid) {
		if (rte_eal_wait_lcore(lcoreid) < 0)
			return -1;
	}

	return 0;
}
